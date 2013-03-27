/* The common simulator framework for GDB, the GNU Debugger.

   Copyright 2002, 2007 Free Software Foundation, Inc.

   Contributed by Andrew Cagney and Red Hat.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */


#include "sim-main.h"
#include "sim-io.h"
#include "targ-vals.h"

#include <errno.h>
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif

#if HAVE_UNISTD_H
#include <unistd.h>
#endif

/* Define the rate at which the simulator should poll the host
   for a quit. */
#ifndef POLL_QUIT_INTERVAL
#define POLL_QUIT_INTERVAL 0x10
#endif

static int poll_quit_count = POLL_QUIT_INTERVAL;

/* See the file include/callbacks.h for a description */


int
sim_io_init(SIM_DESC sd)
{
  return STATE_CALLBACK (sd)->init (STATE_CALLBACK (sd));
}


int
sim_io_shutdown(SIM_DESC sd)
{
  return STATE_CALLBACK (sd)->shutdown (STATE_CALLBACK (sd));
}


int
sim_io_unlink(SIM_DESC sd,
	      const char *f1)
{
  return STATE_CALLBACK (sd)->unlink (STATE_CALLBACK (sd), f1);
}


long
sim_io_time(SIM_DESC sd,
	    long *t)
{
  return STATE_CALLBACK (sd)->time (STATE_CALLBACK (sd), t);
}


int
sim_io_system(SIM_DESC sd, const char *s)
{
  return STATE_CALLBACK (sd)->system (STATE_CALLBACK (sd), s);
}


int
sim_io_rename(SIM_DESC sd,
	      const char *f1,
	      const char *f2)
{
  return STATE_CALLBACK (sd)->rename (STATE_CALLBACK (sd), f1, f2);
}


int
sim_io_write_stdout(SIM_DESC sd,
		    const char *buf,
		    int len)
{
  switch (CURRENT_STDIO) {
  case DO_USE_STDIO:
    return STATE_CALLBACK (sd)->write_stdout (STATE_CALLBACK (sd), buf, len);
    break;
  case DONT_USE_STDIO:
    return STATE_CALLBACK (sd)->write (STATE_CALLBACK (sd), 1, buf, len);
    break;
  default:
    sim_io_error (sd, "sim_io_write_stdout: unaccounted switch\n");
    break;
  }
  return 0;
}


void
sim_io_flush_stdout(SIM_DESC sd)
{
  switch (CURRENT_STDIO) {
  case DO_USE_STDIO:
    STATE_CALLBACK (sd)->flush_stdout (STATE_CALLBACK (sd));
    break;
  case DONT_USE_STDIO:
    break;
  default:
    sim_io_error (sd, "sim_io_flush_stdout: unaccounted switch\n");
    break;
  }
}


int
sim_io_write_stderr(SIM_DESC sd,
		    const char *buf,
		    int len)
{
  switch (CURRENT_STDIO) {
  case DO_USE_STDIO:
    return STATE_CALLBACK (sd)->write_stderr (STATE_CALLBACK (sd), buf, len);
    break;
  case DONT_USE_STDIO:
    return STATE_CALLBACK (sd)->write (STATE_CALLBACK (sd), 2, buf, len);
    break;
  default:
    sim_io_error (sd, "sim_io_write_stderr: unaccounted switch\n");
    break;
  }
  return 0;
}


void
sim_io_flush_stderr(SIM_DESC sd)
{
  switch (CURRENT_STDIO) {
  case DO_USE_STDIO:
    STATE_CALLBACK (sd)->flush_stderr (STATE_CALLBACK (sd));
    break;
  case DONT_USE_STDIO:
    break;
  default:
    sim_io_error (sd, "sim_io_flush_stderr: unaccounted switch\n");
    break;
  }
}


int
sim_io_write(SIM_DESC sd,
	     int fd,
	     const char *buf,
	     int len)
{
  return STATE_CALLBACK (sd)->write (STATE_CALLBACK (sd), fd, buf, len);
}


int
sim_io_read_stdin(SIM_DESC sd,
		  char *buf,
		  int len)
{
  switch (CURRENT_STDIO) {
  case DO_USE_STDIO:
    return STATE_CALLBACK (sd)->read_stdin (STATE_CALLBACK (sd), buf, len);
    break;
  case DONT_USE_STDIO:
    return STATE_CALLBACK (sd)->read (STATE_CALLBACK (sd), 0, buf, len);
    break;
  default:
    sim_io_error (sd, "sim_io_read_stdin: unaccounted switch\n");
    break;
  }
  return 0;
}


int
sim_io_read(SIM_DESC sd, int fd,
	    char *buf,
	    int len)
{
  return STATE_CALLBACK (sd)->read (STATE_CALLBACK (sd), fd, buf, len);
}


int
sim_io_open(SIM_DESC sd,
	    const char *name,
	    int flags)
{
  return STATE_CALLBACK (sd)->open (STATE_CALLBACK (sd), name, flags);
}


int
sim_io_lseek(SIM_DESC sd,
	     int fd,
	     long off,
	     int way)
{
  return STATE_CALLBACK (sd)->lseek (STATE_CALLBACK (sd), fd, off, way);
}


int
sim_io_isatty(SIM_DESC sd,
	      int fd)
{
  return STATE_CALLBACK (sd)->isatty (STATE_CALLBACK (sd), fd);
}


int
sim_io_get_errno(SIM_DESC sd)
{
  return STATE_CALLBACK (sd)->get_errno (STATE_CALLBACK (sd));
}


int
sim_io_close(SIM_DESC sd,
	     int fd)
{
  return STATE_CALLBACK (sd)->close (STATE_CALLBACK (sd), fd);
}


void
sim_io_printf(SIM_DESC sd,
	      const char *fmt,
	      ...)
{
  va_list ap;
  va_start(ap, fmt);
  STATE_CALLBACK (sd)->vprintf_filtered (STATE_CALLBACK (sd), fmt, ap);
  va_end(ap);
}


void
sim_io_vprintf(SIM_DESC sd,
	       const char *fmt,
	       va_list ap)
{
  STATE_CALLBACK (sd)->vprintf_filtered (STATE_CALLBACK (sd), fmt, ap);
}


void
sim_io_eprintf(SIM_DESC sd,
	      const char *fmt,
	      ...)
{
  va_list ap;
  va_start(ap, fmt);
  STATE_CALLBACK (sd)->evprintf_filtered (STATE_CALLBACK (sd), fmt, ap);
  va_end(ap);
}


void
sim_io_evprintf(SIM_DESC sd,
		const char *fmt,
		va_list ap)
{
  STATE_CALLBACK (sd)->evprintf_filtered (STATE_CALLBACK (sd), fmt, ap);
}


void
sim_io_error(SIM_DESC sd,
	     const char *fmt,
	     ...)
{
  if (sd == NULL || STATE_CALLBACK (sd) == NULL) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf (stderr, fmt, ap);
    va_end(ap);
    fprintf (stderr, "\n");
    abort ();
  }
  else {
    va_list ap;
    va_start(ap, fmt);
    STATE_CALLBACK (sd)->evprintf_filtered (STATE_CALLBACK (sd), fmt, ap);
    va_end(ap);
    STATE_CALLBACK (sd)->error (STATE_CALLBACK (sd), "");
  }
}


void
sim_io_poll_quit(SIM_DESC sd)
{
  if (STATE_CALLBACK (sd)->poll_quit != NULL && poll_quit_count-- < 0)
    {
      poll_quit_count = POLL_QUIT_INTERVAL;
      if (STATE_CALLBACK (sd)->poll_quit (STATE_CALLBACK (sd)))
	sim_stop (sd);
    }
}


/* Based on gdb-4.17/sim/ppc/main.c:sim_io_read_stdin().

   FIXME: Should not be calling fcntl() or grubbing around inside of
   ->fdmap and ->errno.

   FIXME: Some completly new mechanism for handling the general
   problem of asynchronous IO is needed.

   FIXME: This function does not supress the echoing (ECHO) of input.
   Consequently polled input is always displayed.

   FIXME: This function does not perform uncooked reads.
   Consequently, data will not be read until an EOLN character has
   been entered. A cntrl-d may force the early termination of a line */


int
sim_io_poll_read (SIM_DESC sd,
		  int sim_io_fd,
		  char *buf,
		  int sizeof_buf)
{
#if defined(O_NDELAY) && defined(F_GETFL) && defined(F_SETFL)
  int fd = STATE_CALLBACK (sd)->fdmap[sim_io_fd];
  int flags;
  int status;
  int nr_read;
  int result;
  STATE_CALLBACK (sd)->last_errno = 0;
  /* get the old status */
  flags = fcntl (fd, F_GETFL, 0);
  if (flags == -1)
    {
      perror ("sim_io_poll_read");
      return 0;
    }
  /* temp, disable blocking IO */
  status = fcntl (fd, F_SETFL, flags | O_NDELAY);
  if (status == -1)
    {
      perror ("sim_io_read_stdin");
      return 0;
    }
  /* try for input */
  nr_read = read (fd, buf, sizeof_buf);
  if (nr_read >= 0)
    {
      /* printf ("<nr-read=%d>\n", nr_read); */
      result = nr_read;
    }
  else
    { /* nr_read < 0 */
      result = -1;
      STATE_CALLBACK (sd)->last_errno = errno;
    }
  /* return to regular vewing */
  status = fcntl (fd, F_SETFL, flags);
  if (status == -1)
    {
      perror ("sim_io_read_stdin");
      /* return 0; */
    }
  return result;
#else
  return sim_io_read (sd, sim_io_fd, buf, sizeof_buf);
#endif
}
