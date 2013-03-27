/*  This file is part of the program psim.

    Copyright 1994, 1995, 1996, 1998, 2003 Andrew Cagney

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


#include <signal.h> /* FIXME - should be machine dependant version */
#include <stdarg.h>
#include <ctype.h>

#include "psim.h"
#include "options.h"

#undef printf_filtered /* blow away the mapping */

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif

#include "libiberty.h"
#include "bfd.h"
#include "gdb/callback.h"
#include "gdb/remote-sim.h"
#include "gdb/signals.h"

/* Define the rate at which the simulator should poll the host
   for a quit. */
#ifndef POLL_QUIT_INTERVAL
#define POLL_QUIT_INTERVAL 0x20
#endif

static int poll_quit_count = POLL_QUIT_INTERVAL;

/* Structures used by the simulator, for gdb just have static structures */

psim *simulator;
static device *root_device;
static host_callback *callbacks;

SIM_DESC
sim_open (SIM_OPEN_KIND kind,
	  host_callback *callback,
	  struct bfd *abfd,
	  char **argv)
{
  callbacks = callback;

  /* Note: The simulation is not created by sim_open() because
     complete information is not yet available */
  /* trace the call */
  TRACE(trace_gdb, ("sim_open called\n"));

  if (root_device != NULL)
    sim_io_printf_filtered("Warning - re-open of simulator leaks memory\n");
  root_device = psim_tree();
  simulator = NULL;

  psim_options(root_device, argv + 1);

  if (ppc_trace[trace_opts])
    print_options ();

  /* fudge our descriptor for now */
  return (SIM_DESC) 1;
}


void
sim_close (SIM_DESC sd, int quitting)
{
  TRACE(trace_gdb, ("sim_close(quitting=%d) called\n", quitting));
  if (ppc_trace[trace_print_info] && simulator != NULL)
    psim_print_info (simulator, ppc_trace[trace_print_info]);
}


SIM_RC
sim_load (SIM_DESC sd, char *prog, bfd *abfd, int from_tty)
{
  TRACE(trace_gdb, ("sim_load(prog=%s, from_tty=%d) called\n",
		    prog, from_tty));
  ASSERT(prog != NULL);

  /* create the simulator */
  TRACE(trace_gdb, ("sim_load() - first time, create the simulator\n"));
  simulator = psim_create(prog, root_device);

  /* bring in all the data section */
  psim_init(simulator);

  /* get the start address */
  if (abfd == NULL)
    {
      abfd = bfd_openr (prog, 0);
      if (abfd == NULL)
	error ("psim: can't open \"%s\": %s\n", 
	       prog, bfd_errmsg (bfd_get_error ()));
      if (!bfd_check_format (abfd, bfd_object)) 
	{
	  const char *errmsg = bfd_errmsg (bfd_get_error ());
	  bfd_close (abfd);
	  error ("psim: \"%s\" is not an object file: %s\n",
		 prog, errmsg);
	}
      bfd_close (abfd);
    }

  return SIM_RC_OK;
}


int
sim_read (SIM_DESC sd, SIM_ADDR mem, unsigned char *buf, int length)
{
  int result = psim_read_memory(simulator, MAX_NR_PROCESSORS,
				buf, mem, length);
  TRACE(trace_gdb, ("sim_read(mem=0x%lx, buf=0x%lx, length=%d) = %d\n",
		    (long)mem, (long)buf, length, result));
  return result;
}


int
sim_write (SIM_DESC sd, SIM_ADDR mem, unsigned char *buf, int length)
{
  int result = psim_write_memory(simulator, MAX_NR_PROCESSORS,
				 buf, mem, length,
				 1/*violate_ro*/);
  TRACE(trace_gdb, ("sim_write(mem=0x%lx, buf=0x%lx, length=%d) = %d\n",
		    (long)mem, (long)buf, length, result));
  return result;
}

void
sim_info (SIM_DESC sd, int verbose)
{
  TRACE(trace_gdb, ("sim_info(verbose=%d) called\n", verbose));
  psim_print_info (simulator, verbose);
}


SIM_RC
sim_create_inferior (SIM_DESC sd,
		     struct bfd *abfd,
		     char **argv,
		     char **envp)
{
  unsigned_word entry_point;
  TRACE(trace_gdb, ("sim_create_inferior(start_address=0x%x, ...)\n",
		    entry_point));

  if (simulator == NULL)
    error ("No program loaded");

  if (abfd != NULL)
    entry_point = bfd_get_start_address (abfd);
  else
    entry_point = 0xfff00000; /* ??? */

  psim_init(simulator);
  psim_stack(simulator, argv, envp);

  ASSERT (psim_write_register(simulator, -1 /* all start at same PC */,
			      &entry_point, "pc", cooked_transfer) > 0);
  return SIM_RC_OK;
}


void
sim_stop_reason (SIM_DESC sd, enum sim_stop *reason, int *sigrc)
{
  psim_status status = psim_get_status(simulator);

  switch (status.reason) {
  case was_continuing:
    *reason = sim_stopped;
    if (status.signal == 0)
      *sigrc = TARGET_SIGNAL_TRAP;
    else
      *sigrc = status.signal;
    break;
  case was_trap:
    *reason = sim_stopped;
    *sigrc = TARGET_SIGNAL_TRAP;
    break;
  case was_exited:
    *reason = sim_exited;
    *sigrc = status.signal;
    break;
  case was_signalled:
    *reason = sim_signalled;
    *sigrc = status.signal;
    break;
  }

  TRACE(trace_gdb, ("sim_stop_reason(reason=0x%lx(%ld), sigrc=0x%lx(%ld))\n",
		    (long)reason, (long)*reason, (long)sigrc, (long)*sigrc));
}



/* Run (or resume) the program.  */

int
sim_stop (SIM_DESC sd)
{
  psim_stop (simulator);
  return 1;
}

void
sim_resume (SIM_DESC sd, int step, int siggnal)
{
  TRACE(trace_gdb, ("sim_resume(step=%d, siggnal=%d)\n",
		    step, siggnal));

  if (step)
    {
      psim_step (simulator);
    }
  else
    {
      psim_run (simulator);
    }
}

void
sim_do_command (SIM_DESC sd, char *cmd)
{
  TRACE(trace_gdb, ("sim_do_commands(cmd=%s) called\n",
		    cmd ? cmd : "(null)"));
  if (cmd != NULL) {
    char **argv = buildargv(cmd);
    psim_command(root_device, argv);
    freeargv(argv);
  }
}


/* Polling, if required */

void
sim_io_poll_quit (void)
{
  if (callbacks->poll_quit != NULL && poll_quit_count-- < 0)
    {
      poll_quit_count = POLL_QUIT_INTERVAL;
      if (callbacks->poll_quit (callbacks))
	psim_stop (simulator);
    }
}



/* Map simulator IO operations onto the corresponding GDB I/O
   functions.
   
   NB: Only a limited subset of operations are mapped across.  More
   advanced operations (such as dup or write) must either be mapped to
   one of the below calls or handled internally */

int
sim_io_read_stdin(char *buf,
		  int sizeof_buf)
{
  switch (CURRENT_STDIO) {
  case DO_USE_STDIO:
    return callbacks->read_stdin(callbacks, buf, sizeof_buf);
    break;
  case DONT_USE_STDIO:
    return callbacks->read(callbacks, 0, buf, sizeof_buf);
    break;
  default:
    error("sim_io_read_stdin: unaccounted switch\n");
    break;
  }
  return 0;
}

int
sim_io_write_stdout(const char *buf,
		    int sizeof_buf)
{
  switch (CURRENT_STDIO) {
  case DO_USE_STDIO:
    return callbacks->write_stdout(callbacks, buf, sizeof_buf);
    break;
  case DONT_USE_STDIO:
    return callbacks->write(callbacks, 1, buf, sizeof_buf);
    break;
  default:
    error("sim_io_write_stdout: unaccounted switch\n");
    break;
  }
  return 0;
}

int
sim_io_write_stderr(const char *buf,
		    int sizeof_buf)
{
  switch (CURRENT_STDIO) {
  case DO_USE_STDIO:
    /* NB: I think there should be an explicit write_stderr callback */
    return callbacks->write(callbacks, 3, buf, sizeof_buf);
    break;
  case DONT_USE_STDIO:
    return callbacks->write(callbacks, 3, buf, sizeof_buf);
    break;
  default:
    error("sim_io_write_stderr: unaccounted switch\n");
    break;
  }
  return 0;
}


void
sim_io_printf_filtered(const char *fmt,
		       ...)
{
  char message[1024];
  va_list ap;
  /* format the message */
  va_start(ap, fmt);
  vsprintf(message, fmt, ap);
  va_end(ap);
  /* sanity check */
  if (strlen(message) >= sizeof(message))
    error("sim_io_printf_filtered: buffer overflow\n");
  callbacks->printf_filtered(callbacks, "%s", message);
}

void
sim_io_flush_stdoutput(void)
{
  switch (CURRENT_STDIO) {
  case DO_USE_STDIO:
    callbacks->flush_stdout (callbacks);
    break;
  case DONT_USE_STDIO:
    break;
  default:
    error("sim_io_read_stdin: unaccounted switch\n");
    break;
  }
}

void
sim_io_error (SIM_DESC sd, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  callbacks->evprintf_filtered (callbacks, fmt, ap);
  va_end(ap);
  callbacks->error (callbacks, "");
}

/****/

void *
zalloc(long size)
{
  void *memory = (void*)xmalloc(size);
  if (memory == NULL)
    error("xmalloc failed\n");
  memset(memory, 0, size);
  return memory;
}

void zfree(void *data)
{
  free(data);
}
