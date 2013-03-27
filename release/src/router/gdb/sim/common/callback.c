/* Remote target callback routines.
   Copyright 1995, 1996, 1997, 2000, 2002, 2003, 2004, 2007
   Free Software Foundation, Inc.
   Contributed by Cygnus Solutions.

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

/* This file provides a standard way for targets to talk to the host OS
   level.  */

#ifdef HAVE_CONFIG_H
#include "cconfig.h"
#endif
#include "ansidecl.h"
#include <stdarg.h>
#include <stdio.h>
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
#ifdef HAVE_LIMITS_H
/* For PIPE_BUF.  */
#include <limits.h>
#endif
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "gdb/callback.h"
#include "targ-vals.h"
/* For xmalloc.  */
#include "libiberty.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifndef PIPE_BUF
#define PIPE_BUF 512
#endif

/* ??? sim_cb_printf should be cb_printf, but until the callback support is
   broken out of the simulator directory, these are here to not require
   sim-utils.h.  */
void sim_cb_printf PARAMS ((host_callback *, const char *, ...));
void sim_cb_eprintf PARAMS ((host_callback *, const char *, ...));

extern CB_TARGET_DEFS_MAP cb_init_syscall_map[];
extern CB_TARGET_DEFS_MAP cb_init_errno_map[];
extern CB_TARGET_DEFS_MAP cb_init_open_map[];

extern int system PARAMS ((const char *));

static int os_init PARAMS ((host_callback *));
static int os_shutdown PARAMS ((host_callback *));
static int os_unlink PARAMS ((host_callback *, const char *));
static long os_time PARAMS ((host_callback *, long *));
static int os_system PARAMS ((host_callback *, const char *));
static int os_rename PARAMS ((host_callback *, const char *, const char *));
static int os_write_stdout PARAMS ((host_callback *, const char *, int));
static void os_flush_stdout PARAMS ((host_callback *));
static int os_write_stderr PARAMS ((host_callback *, const char *, int));
static void os_flush_stderr PARAMS ((host_callback *));
static int os_write PARAMS ((host_callback *, int, const char *, int));
static int os_read_stdin PARAMS ((host_callback *, char *, int));
static int os_read PARAMS ((host_callback *, int, char *, int));
static int os_open PARAMS ((host_callback *, const char *, int));
static int os_lseek PARAMS ((host_callback *, int, long, int));
static int os_isatty PARAMS ((host_callback *, int));
static int os_get_errno PARAMS ((host_callback *));
static int os_close PARAMS ((host_callback *, int));
static void os_vprintf_filtered PARAMS ((host_callback *, const char *, va_list));
static void os_evprintf_filtered PARAMS ((host_callback *, const char *, va_list));
static void os_error PARAMS ((host_callback *, const char *, ...));
static int fdmap PARAMS ((host_callback *, int));
static int fdbad PARAMS ((host_callback *, int));
static int wrap PARAMS ((host_callback *, int));

/* Set the callback copy of errno from what we see now.  */

static int 
wrap (p, val)
     host_callback *p;
     int val;
{
  p->last_errno = errno;
  return val;
}

/* Make sure the FD provided is ok.  If not, return non-zero
   and set errno. */

static int 
fdbad (p, fd)
     host_callback *p;
     int fd;
{
  if (fd < 0 || fd > MAX_CALLBACK_FDS || p->fd_buddy[fd] < 0)
    {
      p->last_errno = EINVAL;
      return -1;
    }
  return 0;
}

static int 
fdmap (p, fd)
     host_callback *p;
     int fd;
{
  return p->fdmap[fd];
}

static int 
os_close (p, fd)
     host_callback *p;
     int fd;
{
  int result;
  int i, next;

  result = fdbad (p, fd);
  if (result)
    return result;
  /* If this file descripter has one or more buddies (originals /
     duplicates from a dup), just remove it from the circular list.  */
  for (i = fd; (next = p->fd_buddy[i]) != fd; )
    i = next;
  if (fd != i)
    p->fd_buddy[i] = p->fd_buddy[fd];
  else
    {
      if (p->ispipe[fd])
	{
	  int other = p->ispipe[fd];
	  int reader, writer;

	  if (other > 0)
	    {
	      /* Closing the read side.  */
	      reader = fd;
	      writer = other;
	    }
	  else
	    {
	      /* Closing the write side.  */
	      writer = fd;
	      reader = -other;
	    }

	  /* If there was data in the buffer, make a last "now empty"
	     call, then deallocate data.  */
	  if (p->pipe_buffer[writer].buffer != NULL)
	    {
	      (*p->pipe_empty) (p, reader, writer);
	      free (p->pipe_buffer[writer].buffer);
	      p->pipe_buffer[writer].buffer = NULL;
	    }

	  /* Clear pipe data for this side.  */
	  p->pipe_buffer[fd].size = 0;
	  p->ispipe[fd] = 0;

	  /* If this was the first close, mark the other side as the
	     only remaining side.  */
	  if (fd != abs (other))
	    p->ispipe[abs (other)] = -other;
	  p->fd_buddy[fd] = -1;
	  return 0;
	}

      result = wrap (p, close (fdmap (p, fd)));
    }
  p->fd_buddy[fd] = -1;

  return result;
}


/* taken from gdb/util.c:notice_quit() - should be in a library */


#if defined(__GO32__) || defined (_MSC_VER)
static int
os_poll_quit (p)
     host_callback *p;
{
#if defined(__GO32__)
  int kbhit ();
  int getkey ();
  if (kbhit ())
    {
      int k = getkey ();
      if (k == 1)
	{
	  return 1;
	}
      else if (k == 2)
	{
	  return 1;
	}
      else 
	{
	  sim_cb_eprintf (p, "CTRL-A to quit, CTRL-B to quit harder\n");
	}
    }
#endif
#if defined (_MSC_VER)
  /* NB - this will not compile! */
  int k = win32pollquit();
  if (k == 1)
    return 1;
  else if (k == 2)
    return 1;
#endif
  return 0;
}
#else
#define os_poll_quit 0
#endif /* defined(__GO32__) || defined(_MSC_VER) */

static int 
os_get_errno (p)
     host_callback *p;
{
  return cb_host_to_target_errno (p, p->last_errno);
}


static int 
os_isatty (p, fd)
     host_callback *p;
     int fd;
{
  int result;

  result = fdbad (p, fd);
  if (result)
    return result;
  result = wrap (p, isatty (fdmap (p, fd)));

  return result;
}

static int 
os_lseek (p, fd, off, way)
     host_callback *p;
     int fd;
     long off;
     int way;
{
  int result;

  result = fdbad (p, fd);
  if (result)
    return result;
  result = lseek (fdmap (p, fd), off, way);
  return result;
}

static int 
os_open (p, name, flags)
     host_callback *p;
     const char *name;
     int flags;
{
  int i;
  for (i = 0; i < MAX_CALLBACK_FDS; i++)
    {
      if (p->fd_buddy[i] < 0)
	{
	  int f = open (name, cb_target_to_host_open (p, flags), 0644);
	  if (f < 0)
	    {
	      p->last_errno = errno;
	      return f;
	    }
	  p->fd_buddy[i] = i;
	  p->fdmap[i] = f;
	  return i;
	}
    }
  p->last_errno = EMFILE;
  return -1;
}

static int 
os_read (p, fd, buf, len)
     host_callback *p;
     int fd;
     char *buf;
     int len;
{
  int result;

  result = fdbad (p, fd);
  if (result)
    return result;
  if (p->ispipe[fd])
    {
      int writer = p->ispipe[fd];

      /* Can't read from the write-end.  */
      if (writer < 0)
	{
	  p->last_errno = EBADF;
	  return -1;
	}

      /* Nothing to read if nothing is written.  */
      if (p->pipe_buffer[writer].size == 0)
	return 0;

      /* Truncate read request size to buffer size minus what's already
         read.  */
      if (len > p->pipe_buffer[writer].size - p->pipe_buffer[fd].size)
	len = p->pipe_buffer[writer].size - p->pipe_buffer[fd].size;

      memcpy (buf, p->pipe_buffer[writer].buffer + p->pipe_buffer[fd].size,
	      len);

      /* Account for what we just read.  */
      p->pipe_buffer[fd].size += len;

      /* If we've read everything, empty and deallocate the buffer and
	 signal buffer-empty to client.  (This isn't expected to be a
	 hot path in the simulator, so we don't hold on to the buffer.)  */
      if (p->pipe_buffer[fd].size == p->pipe_buffer[writer].size)
	{
	  free (p->pipe_buffer[writer].buffer);
	  p->pipe_buffer[writer].buffer = NULL;
	  p->pipe_buffer[fd].size = 0;
	  p->pipe_buffer[writer].size = 0;
	  (*p->pipe_empty) (p, fd, writer);
	}

      return len;
    }

  result = wrap (p, read (fdmap (p, fd), buf, len));
  return result;
}

static int 
os_read_stdin (p, buf, len)
     host_callback *p;
     char *buf;
     int len;
{
  return wrap (p, read (0, buf, len));
}

static int 
os_write (p, fd, buf, len)
     host_callback *p;
     int fd;
     const char *buf;
     int len;
{
  int result;
  int real_fd;

  result = fdbad (p, fd);
  if (result)
    return result;

  if (p->ispipe[fd])
    {
      int reader = -p->ispipe[fd];

      /* Can't write to the read-end.  */
      if (reader < 0)
	{
	  p->last_errno = EBADF;
	  return -1;
	}

      /* Can't write to pipe with closed read end.
	 FIXME: We should send a SIGPIPE.  */
      if (reader == fd)
	{
	  p->last_errno = EPIPE;
	  return -1;
	}

      /* As a sanity-check, we bail out it the buffered contents is much
	 larger than the size of the buffer on the host.  We don't want
	 to run out of memory in the simulator due to a target program
	 bug if we can help it.  Unfortunately, regarding the value that
	 reaches the simulated program, it's no use returning *less*
	 than the requested amount, because cb_syscall loops calling
	 this function until the whole amount is done.  */
      if (p->pipe_buffer[fd].size + len > 10 * PIPE_BUF)
	{
	  p->last_errno = EFBIG;
	  return -1;
	}

      p->pipe_buffer[fd].buffer
	= xrealloc (p->pipe_buffer[fd].buffer, p->pipe_buffer[fd].size + len);
      memcpy (p->pipe_buffer[fd].buffer + p->pipe_buffer[fd].size,
	      buf, len);
      p->pipe_buffer[fd].size += len;

      (*p->pipe_nonempty) (p, reader, fd);
      return len;
    }

  real_fd = fdmap (p, fd);
  switch (real_fd)
    {
    default:
      result = wrap (p, write (real_fd, buf, len));
      break;
    case 1:
      result = p->write_stdout (p, buf, len);
      break;
    case 2:
      result = p->write_stderr (p, buf, len);
      break;
    }
  return result;
}

static int 
os_write_stdout (p, buf, len)
     host_callback *p ATTRIBUTE_UNUSED;
     const char *buf;
     int len;
{
  return fwrite (buf, 1, len, stdout);
}

static void
os_flush_stdout (p)
     host_callback *p ATTRIBUTE_UNUSED;
{
  fflush (stdout);
}

static int 
os_write_stderr (p, buf, len)
     host_callback *p ATTRIBUTE_UNUSED;
     const char *buf;
     int len;
{
  return fwrite (buf, 1, len, stderr);
}

static void
os_flush_stderr (p)
     host_callback *p ATTRIBUTE_UNUSED;
{
  fflush (stderr);
}

static int 
os_rename (p, f1, f2)
     host_callback *p;
     const char *f1;
     const char *f2;
{
  return wrap (p, rename (f1, f2));
}


static int
os_system (p, s)
     host_callback *p;
     const char *s;
{
  return wrap (p, system (s));
}

static long 
os_time (p, t)
     host_callback *p;
     long *t;
{
  return wrap (p, time (t));
}


static int 
os_unlink (p, f1)
     host_callback *p;
     const char *f1;
{
  return wrap (p, unlink (f1));
}

static int
os_stat (p, file, buf)
     host_callback *p;
     const char *file;
     struct stat *buf;
{
  /* ??? There is an issue of when to translate to the target layout.
     One could do that inside this function, or one could have the
     caller do it.  It's more flexible to let the caller do it, though
     I'm not sure the flexibility will ever be useful.  */
  return wrap (p, stat (file, buf));
}

static int
os_fstat (p, fd, buf)
     host_callback *p;
     int fd;
     struct stat *buf;
{
  if (fdbad (p, fd))
    return -1;

  if (p->ispipe[fd])
    {
#if defined (HAVE_STRUCT_STAT_ST_ATIME) || defined (HAVE_STRUCT_STAT_ST_CTIME) || defined (HAVE_STRUCT_STAT_ST_MTIME)
      time_t t = (*p->time) (p, NULL);
#endif

      /* We have to fake the struct stat contents, since the pipe is
	 made up in the simulator.  */
      memset (buf, 0, sizeof (*buf));

#ifdef HAVE_STRUCT_STAT_ST_MODE
      buf->st_mode = S_IFIFO;
#endif

      /* If more accurate tracking than current-time is needed (for
	 example, on GNU/Linux we get accurate numbers), the p->time
	 callback (which may be something other than os_time) should
	 happen for each read and write, and we'd need to keep track of
	 atime, ctime and mtime.  */
#ifdef HAVE_STRUCT_STAT_ST_ATIME
      buf->st_atime = t;
#endif
#ifdef HAVE_STRUCT_STAT_ST_CTIME
      buf->st_ctime = t;
#endif
#ifdef HAVE_STRUCT_STAT_ST_MTIME
      buf->st_mtime = t;
#endif
      return 0;
    }

  /* ??? There is an issue of when to translate to the target layout.
     One could do that inside this function, or one could have the
     caller do it.  It's more flexible to let the caller do it, though
     I'm not sure the flexibility will ever be useful.  */
  return wrap (p, fstat (fdmap (p, fd), buf));
}

static int
os_lstat (p, file, buf)
     host_callback *p;
     const char *file;
     struct stat *buf;
{
  /* NOTE: hpn/2004-12-12: Same issue here as with os_fstat.  */
#ifdef HAVE_LSTAT
  return wrap (p, lstat (file, buf));
#else
  return wrap (p, stat (file, buf));
#endif
}

static int 
os_ftruncate (p, fd, len)
     host_callback *p;
     int fd;
     long len;
{
  int result;

  result = fdbad (p, fd);
  if (p->ispipe[fd])
    {
      p->last_errno = EINVAL;
      return -1;
    }
  if (result)
    return result;
#ifdef HAVE_FTRUNCATE
  result = wrap (p, ftruncate (fdmap (p, fd), len));
#else
  p->last_errno = EINVAL;
  result = -1;
#endif
  return result;
}

static int
os_truncate (p, file, len)
     host_callback *p;
     const char *file;
     long len;
{
#ifdef HAVE_TRUNCATE
  return wrap (p, truncate (file, len));
#else
  p->last_errno = EINVAL;
  return -1;
#endif
}

static int
os_pipe (p, filedes)
     host_callback *p;
     int *filedes;
{
  int i;

  /* We deliberately don't use fd 0.  It's probably stdin anyway.  */
  for (i = 1; i < MAX_CALLBACK_FDS; i++)
    {
      int j;

      if (p->fd_buddy[i] < 0)
	for (j = i + 1; j < MAX_CALLBACK_FDS; j++)
	  if (p->fd_buddy[j] < 0)
	    {
	      /* Found two free fd:s.  Set stat to allocated and mark
		 pipeness.  */
	      p->fd_buddy[i] = i;
	      p->fd_buddy[j] = j;
	      p->ispipe[i] = j;
	      p->ispipe[j] = -i;
	      filedes[0] = i;
	      filedes[1] = j;

	      /* Poison the FD map to make bugs apparent.  */
	      p->fdmap[i] = -1;
	      p->fdmap[j] = -1;
	      return 0;
	    }
    }

  p->last_errno = EMFILE;
  return -1;
}

/* Stub functions for pipe support.  They should always be overridden in
   targets using the pipe support, but that's up to the target.  */

/* Called when the simulator says that the pipe at (reader, writer) is
   now empty (so the writer should leave its waiting state).  */

static void
os_pipe_empty (p, reader, writer)
     host_callback *p;
     int reader;
     int writer;
{
}

/* Called when the simulator says the pipe at (reader, writer) is now
   non-empty (so the writer should wait).  */

static void
os_pipe_nonempty (p, reader, writer)
     host_callback *p;
     int reader;
     int writer;
{
}

static int
os_shutdown (p)
     host_callback *p;
{
  int i, next, j;
  for (i = 0; i < MAX_CALLBACK_FDS; i++)
    {
      int do_close = 1;

      /* Zero out all pipe state.  Don't call callbacks for non-empty
	 pipes; the target program has likely terminated at this point
	 or we're called at initialization time.  */
      p->ispipe[i] = 0;
      p->pipe_buffer[i].size = 0;
      p->pipe_buffer[i].buffer = NULL;

      next = p->fd_buddy[i];
      if (next < 0)
	continue;
      do
	{
	  j = next;
	  if (j == MAX_CALLBACK_FDS)
	    do_close = 0;
	  next = p->fd_buddy[j];
	  p->fd_buddy[j] = -1;
	  /* At the initial call of os_init, we got -1, 0, 0, 0, ...  */
	  if (next < 0)
	    {
	      p->fd_buddy[i] = -1;
	      do_close = 0;
	      break;
	    }
	}
      while (j != i);
      if (do_close)
	close (p->fdmap[i]);
    }
  return 1;
}

static int
os_init (p)
     host_callback *p;
{
  int i;

  os_shutdown (p);
  for (i = 0; i < 3; i++)
    {
      p->fdmap[i] = i;
      p->fd_buddy[i] = i - 1;
    }
  p->fd_buddy[0] = MAX_CALLBACK_FDS;
  p->fd_buddy[MAX_CALLBACK_FDS] = 2;

  p->syscall_map = cb_init_syscall_map;
  p->errno_map = cb_init_errno_map;
  p->open_map = cb_init_open_map;

  return 1;
}

/* DEPRECATED */

/* VARARGS */
static void
os_printf_filtered (host_callback *p ATTRIBUTE_UNUSED, const char *format, ...)
{
  va_list args;
  va_start (args, format);

  vfprintf (stdout, format, args);
  va_end (args);
}

/* VARARGS */
static void
os_vprintf_filtered (host_callback *p ATTRIBUTE_UNUSED, const char *format, va_list args)
{
  vprintf (format, args);
}

/* VARARGS */
static void
os_evprintf_filtered (host_callback *p ATTRIBUTE_UNUSED, const char *format, va_list args)
{
  vfprintf (stderr, format, args);
}

/* VARARGS */
static void
os_error (host_callback *p ATTRIBUTE_UNUSED, const char *format, ...)
{
  va_list args;
  va_start (args, format);

  vfprintf (stderr, format, args);
  fprintf (stderr, "\n");

  va_end (args);
  exit (1);
}

host_callback default_callback =
{
  os_close,
  os_get_errno,
  os_isatty,
  os_lseek,
  os_open,
  os_read,
  os_read_stdin,
  os_rename,
  os_system,
  os_time,
  os_unlink,
  os_write,
  os_write_stdout,
  os_flush_stdout,
  os_write_stderr,
  os_flush_stderr,

  os_stat,
  os_fstat,
  os_lstat,

  os_ftruncate,
  os_truncate,

  os_pipe,
  os_pipe_empty,
  os_pipe_nonempty,

  os_poll_quit,

  os_shutdown,
  os_init,

  os_printf_filtered,  /* deprecated */

  os_vprintf_filtered,
  os_evprintf_filtered,
  os_error,

  0, 		/* last errno */

  { 0, },	/* fdmap */
  { -1, },	/* fd_buddy */
  { 0, },	/* ispipe */
  { { 0, 0 }, }, /* pipe_buffer */

  0, /* syscall_map */
  0, /* errno_map */
  0, /* open_map */
  0, /* signal_map */
  0, /* stat_map */
	
  /* Defaults expected to be overridden at initialization, where needed.  */
  BFD_ENDIAN_UNKNOWN, /* target_endian */
  4, /* target_sizeof_int */

  HOST_CALLBACK_MAGIC,
};

/* Read in a file describing the target's system call values.
   E.g. maybe someone will want to use something other than newlib.
   This assumes that the basic system call recognition and value passing/
   returning is supported.  So maybe some coding/recompilation will be
   necessary, but not as much.

   If an error occurs, the existing mapping is not changed.  */

CB_RC
cb_read_target_syscall_maps (cb, file)
     host_callback *cb;
     const char *file;
{
  CB_TARGET_DEFS_MAP *syscall_map, *errno_map, *open_map, *signal_map;
  const char *stat_map;
  FILE *f;

  if ((f = fopen (file, "r")) == NULL)
    return CB_RC_ACCESS;

  /* ... read in and parse file ... */

  fclose (f);
  return CB_RC_NO_MEM; /* FIXME:wip */

  /* Free storage allocated for any existing maps.  */
  if (cb->syscall_map)
    free (cb->syscall_map);
  if (cb->errno_map)
    free (cb->errno_map);
  if (cb->open_map)
    free (cb->open_map);
  if (cb->signal_map)
    free (cb->signal_map);
  if (cb->stat_map)
    free ((PTR) cb->stat_map);

  cb->syscall_map = syscall_map;
  cb->errno_map = errno_map;
  cb->open_map = open_map;
  cb->signal_map = signal_map;
  cb->stat_map = stat_map;

  return CB_RC_OK;
}

/* Translate the target's version of a syscall number to the host's.
   This isn't actually the host's version, rather a canonical form.
   ??? Perhaps this should be renamed to ..._canon_syscall.  */

int
cb_target_to_host_syscall (cb, target_val)
     host_callback *cb;
     int target_val;
{
  CB_TARGET_DEFS_MAP *m;

  for (m = &cb->syscall_map[0]; m->target_val != -1; ++m)
    if (m->target_val == target_val)
      return m->host_val;

  return -1;
}

/* FIXME: sort tables if large.
   Alternatively, an obvious improvement for errno conversion is
   to machine generate a function with a large switch().  */

/* Translate the host's version of errno to the target's.  */

int
cb_host_to_target_errno (cb, host_val)
     host_callback *cb;
     int host_val;
{
  CB_TARGET_DEFS_MAP *m;

  for (m = &cb->errno_map[0]; m->host_val; ++m)
    if (m->host_val == host_val)
      return m->target_val;

  /* ??? Which error to return in this case is up for grabs.
     Note that some missing values may have standard alternatives.
     For now return 0 and require caller to deal with it.  */
  return 0;
}

/* Given a set of target bitmasks for the open system call,
   return the host equivalent.
   Mapping open flag values is best done by looping so there's no need
   to machine generate this function.  */

int
cb_target_to_host_open (cb, target_val)
     host_callback *cb;
     int target_val;
{
  int host_val = 0;
  CB_TARGET_DEFS_MAP *m;

  for (m = &cb->open_map[0]; m->host_val != -1; ++m)
    {
      switch (m->target_val)
	{
	  /* O_RDONLY can be (and usually is) 0 which needs to be treated
	     specially.  */
	case TARGET_O_RDONLY :
	case TARGET_O_WRONLY :
	case TARGET_O_RDWR :
	  if ((target_val & (TARGET_O_RDONLY | TARGET_O_WRONLY | TARGET_O_RDWR))
	      == m->target_val)
	    host_val |= m->host_val;
	  /* Handle the host/target differentiating between binary and
             text mode.  Only one case is of importance */
#if ! defined (TARGET_O_BINARY) && defined (O_BINARY)
	  host_val |= O_BINARY;
#endif
	  break;
	default :
	  if ((m->target_val & target_val) == m->target_val)
	    host_val |= m->host_val;
	  break;
	}
    }

  return host_val;
}

/* Utility for e.g. cb_host_to_target_stat to store values in the target's
   stat struct.  */

void
cb_store_target_endian (cb, p, size, val)
     host_callback *cb;
     char *p;
     int size;
     long val; /* ??? must be as big as target word size */
{
  if (cb->target_endian == BFD_ENDIAN_BIG)
    {
      p += size;
      while (size-- > 0)
	{
	  *--p = val;
	  val >>= 8;
	}
    }
  else
    {
      while (size-- > 0)
	{
	  *p++ = val;
	  val >>= 8;
	}
    }
}

/* Translate a host's stat struct into a target's.
   If HS is NULL, just compute the length of the buffer required,
   TS is ignored.

   The result is the size of the target's stat struct,
   or zero if an error occurred during the translation.  */

int
cb_host_to_target_stat (cb, hs, ts)
     host_callback *cb;
     const struct stat *hs;
     PTR ts;
{
  const char *m = cb->stat_map;
  char *p;

  if (hs == NULL)
    ts = NULL;
  p = ts;

  while (m)
    {
      char *q = strchr (m, ',');
      int size;

      /* FIXME: Use sscanf? */
      if (q == NULL)
	{
	  /* FIXME: print error message */
	  return 0;
	}
      size = atoi (q + 1);
      if (size == 0)
	{
	  /* FIXME: print error message */
	  return 0;
	}

      if (hs != NULL)
	{
	  if (0)
	    ;
	  /* Defined here to avoid emacs indigestion on a lone "else".  */
#undef ST_x
#define ST_x(FLD)					\
	  else if (strncmp (m, #FLD, q - m) == 0)	\
	    cb_store_target_endian (cb, p, size, hs->FLD)

#ifdef HAVE_STRUCT_STAT_ST_DEV
	  ST_x (st_dev);
#endif
#ifdef HAVE_STRUCT_STAT_ST_INO
	  ST_x (st_ino);
#endif
#ifdef HAVE_STRUCT_STAT_ST_MODE
	  ST_x (st_mode);
#endif
#ifdef HAVE_STRUCT_STAT_ST_NLINK
	  ST_x (st_nlink);
#endif
#ifdef HAVE_STRUCT_STAT_ST_UID
	  ST_x (st_uid);
#endif
#ifdef HAVE_STRUCT_STAT_ST_GID
	  ST_x (st_gid);
#endif
#ifdef HAVE_STRUCT_STAT_ST_RDEV
	  ST_x (st_rdev);
#endif
#ifdef HAVE_STRUCT_STAT_ST_SIZE
	  ST_x (st_size);
#endif
#ifdef HAVE_STRUCT_STAT_ST_BLKSIZE
	  ST_x (st_blksize);
#endif
#ifdef HAVE_STRUCT_STAT_ST_BLOCKS
	  ST_x (st_blocks);
#endif
#ifdef HAVE_STRUCT_STAT_ST_ATIME
	  ST_x (st_atime);
#endif
#ifdef HAVE_STRUCT_STAT_ST_MTIME
	  ST_x (st_mtime);
#endif
#ifdef HAVE_STRUCT_STAT_ST_CTIME
	  ST_x (st_ctime);
#endif
#undef ST_x
	  /* FIXME:wip */
	  else
	    /* Unsupported field, store 0.  */
	    cb_store_target_endian (cb, p, size, 0);
	}

      p += size;
      m = strchr (q, ':');
      if (m)
	++m;
    }

  return p - (char *) ts;
}

/* Cover functions to the vfprintf callbacks.

   ??? If one thinks of the callbacks as a subsystem onto itself [or part of
   a larger "remote target subsystem"] with a well defined interface, then
   one would think that the subsystem would provide these.  However, until
   one is allowed to create such a subsystem (with its own source tree
   independent of any particular user), such a critter can't exist.  Thus
   these functions are here for the time being.  */

void
sim_cb_printf (host_callback *p, const char *fmt, ...)
{
  va_list ap;

  va_start (ap, fmt);
  p->vprintf_filtered (p, fmt, ap);
  va_end (ap);
}

void
sim_cb_eprintf (host_callback *p, const char *fmt, ...)
{
  va_list ap;

  va_start (ap, fmt);
  p->evprintf_filtered (p, fmt, ap);
  va_end (ap);
}
