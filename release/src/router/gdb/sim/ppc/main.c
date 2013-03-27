/*  This file is part of the program psim.

    Copyright (C) 1994-1997, Andrew Cagney <cagney@highland.com.au>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
 
    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 
    */


#include <stdarg.h>
#include <stdio.h>
#include <fcntl.h>

#include <signal.h>

#include "psim.h"
#include "options.h"
#include "device.h" /* FIXME: psim should provide the interface */
#include "events.h" /* FIXME: psim should provide the interface */

#include "bfd.h"
#include "gdb/callback.h"
#include "gdb/remote-sim.h"

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif

#include <errno.h>

#if !defined(O_NDELAY) || !defined(F_GETFL) || !defined(F_SETFL)
#undef WITH_STDIO
#define WITH_STDIO DO_USE_STDIO
#endif


extern char **environ;

static psim *simulation = NULL;


void
sim_io_poll_quit (void)
{
  /* nothing to do */
}

void
sim_io_printf_filtered(const char *msg, ...)
{
  va_list ap;
  va_start(ap, msg);
  vprintf(msg, ap);
  va_end(ap);
}

void
error (const char *msg, ...)
{
  va_list ap;
  va_start(ap, msg);
  vprintf(msg, ap);
  printf("\n");
  va_end(ap);

  /* any final clean up */
  if (ppc_trace[trace_print_info] && simulation != NULL)
    psim_print_info (simulation, ppc_trace[trace_print_info]);

  exit (1);
}

int
sim_io_write_stdout(const char *buf,
		    int sizeof_buf)
{
  switch (CURRENT_STDIO) {
  case DO_USE_STDIO:
    {
      int i;
      for (i = 0; i < sizeof_buf; i++) {
	putchar(buf[i]);
      }
      return i;
    }
    break;
  case DONT_USE_STDIO:
    return write(1, buf, sizeof_buf);
    break;
  default:
    error("sim_io_write_stdout: invalid switch\n");
  }
  return 0;
}

int
sim_io_write_stderr(const char *buf,
		    int sizeof_buf)
{
  switch (CURRENT_STDIO) {
  case DO_USE_STDIO:
    {
      int i;
      for (i = 0; i < sizeof_buf; i++) {
	fputc(buf[i], stderr);
      }
      return i;
    }
    break;
  case DONT_USE_STDIO:
    return write(2, buf, sizeof_buf);
    break;
  default:
    error("sim_io_write_stdout: invalid switch\n");
  }
  return 0;
}

int
sim_io_read_stdin(char *buf,
		  int sizeof_buf)
{
  switch (CURRENT_STDIO) {
  case DO_USE_STDIO:
    if (sizeof_buf > 1) {
      if (fgets(buf, sizeof_buf, stdin) != NULL)
	return strlen(buf);
    }
    else if (sizeof_buf == 1) {
      char b[2];
      if (fgets(b, sizeof(b), stdin) != NULL) {
	memcpy(buf, b, strlen(b));
	return strlen(b);
      }
    }
    else if (sizeof_buf == 0)
      return 0;
    return sim_io_eof;
    break;
  case DONT_USE_STDIO:
#if defined(O_NDELAY) && defined(F_GETFL) && defined(F_SETFL)
    {
      /* check for input */
      int flags;
      int status;
      int nr_read;
      int result;
      /* get the old status */
      flags = fcntl(0, F_GETFL, 0);
      if (flags == -1) {
	perror("sim_io_read_stdin");
	return sim_io_eof;
      }
      /* temp, disable blocking IO */
      status = fcntl(0, F_SETFL, flags | O_NDELAY);
      if (status == -1) {
	perror("sim_io_read_stdin");
	return sim_io_eof;
      }
      /* try for input */
      nr_read = read(0, buf, sizeof_buf);
      if (nr_read > 0
	  || (nr_read == 0 && sizeof_buf == 0))
	result = nr_read;
      else if (nr_read == 0)
	result = sim_io_eof;
      else { /* nr_read < 0 */
	if (errno == EAGAIN)
	  result = sim_io_not_ready;
	else 
	  result = sim_io_eof;
      }
      /* return to regular vewing */
      status = fcntl(0, F_SETFL, flags);
      if (status == -1) {
	perror("sim_io_read_stdin");
	return sim_io_eof;
      }
      return result;
    }
    break;
#endif
  default:
    error("sim_io_read_stdin: invalid switch\n");
    break;
  }
  return 0;
}

void
sim_io_flush_stdoutput(void)
{
  switch (CURRENT_STDIO) {
  case DO_USE_STDIO:
    fflush (stdout);
    break;
  case DONT_USE_STDIO:
    break;
  default:
    error("sim_io_flush_stdoutput: invalid switch\n");
    break;
  }
}

void
sim_io_error (SIM_DESC sd, const char *msg, ...)
{
  va_list ap;
  va_start(ap, msg);
  vprintf(msg, ap);
  printf("\n");
  va_end(ap);

  /* any final clean up */
  if (ppc_trace[trace_print_info] && simulation != NULL)
    psim_print_info (simulation, ppc_trace[trace_print_info]);

  exit (1);
}


void *
zalloc(long size)
{
  void *memory = malloc(size);
  if (memory == NULL)
    error("zalloc failed\n");
  memset(memory, 0, size);
  return memory;
}

void
zfree(void *chunk)
{
  free(chunk);
}

/* When a CNTRL-C occures, queue an event to shut down the simulation */

static RETSIGTYPE
cntrl_c(int sig)
{
  psim_stop (simulation);
}


int
main(int argc, char **argv)
{
  const char *name_of_file;
  char *arg_;
  psim_status status;
  device *root = psim_tree();

  /* parse the arguments */
  argv = psim_options(root, argv + 1);
  if (argv[0] == NULL) {
    if (ppc_trace[trace_opts]) {
      print_options ();
      return 0;
    } else {
      psim_usage(0);
    }
  }
  name_of_file = argv[0];

  if (ppc_trace[trace_opts])
    print_options ();

  /* create the simulator */
  simulation = psim_create(name_of_file, root);

  /* fudge the environment so that _=prog-name */
  arg_ = (char*)zalloc(strlen(argv[0]) + strlen("_=") + 1);
  strcpy(arg_, "_=");
  strcat(arg_, argv[0]);
  putenv(arg_);

  /* initialize it */
  psim_init(simulation);
  psim_stack(simulation, argv, environ);

  {
    RETSIGTYPE (*prev) ();
    prev = signal(SIGINT, cntrl_c);
    psim_run(simulation);
    signal(SIGINT, prev);
  }

  /* any final clean up */
  if (ppc_trace[trace_print_info])
    psim_print_info (simulation, ppc_trace[trace_print_info]);

  /* why did we stop */
  status = psim_get_status(simulation);
  switch (status.reason) {
  case was_continuing:
    error("psim: continuing while stoped!\n");
    return 0;
  case was_trap:
    error("psim: no trap insn\n");
    return 0;
  case was_exited:
    return status.signal;
  case was_signalled:
    printf ("%s: Caught signal %d at address 0x%lx\n",
 	    name_of_file, (int)status.signal,
 	    (long)status.program_counter);
    return status.signal;
  default:
    error("unknown halt condition\n");
    return 0;
  }
}
