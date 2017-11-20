/* Emulate flock on platforms that lack it, primarily Windows and MinGW.

   This is derived from sqlite3 sources.
   http://www.sqlite.org/cvstrac/rlog?f=sqlite/src/os_win.c
   http://www.sqlite.org/copyright.html

   Written by Richard W.M. Jones <rjones.at.redhat.com>

   Copyright (C) 2008-2017 Free Software Foundation, Inc.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include <config.h>
#include <sys/file.h>

#if (defined _WIN32 || defined __WIN32__) && ! defined __CYGWIN__

/* LockFileEx */
# define WIN32_LEAN_AND_MEAN
# include <windows.h>

# include <errno.h>

/* _get_osfhandle */
# include "msvc-nothrow.h"

/* Determine the current size of a file.  Because the other braindead
 * APIs we'll call need lower/upper 32 bit pairs, keep the file size
 * like that too.
 */
static BOOL
file_size (HANDLE h, DWORD * lower, DWORD * upper)
{
  *lower = GetFileSize (h, upper);
  return 1;
}

/* LOCKFILE_FAIL_IMMEDIATELY is undefined on some Windows systems. */
# ifndef LOCKFILE_FAIL_IMMEDIATELY
#  define LOCKFILE_FAIL_IMMEDIATELY 1
# endif

/* Acquire a lock. */
static BOOL
do_lock (HANDLE h, int non_blocking, int exclusive)
{
  BOOL res;
  DWORD size_lower, size_upper;
  OVERLAPPED ovlp;
  int flags = 0;

  /* We're going to lock the whole file, so get the file size. */
  res = file_size (h, &size_lower, &size_upper);
  if (!res)
    return 0;

  /* Start offset is 0, and also zero the remaining members of this struct. */
  memset (&ovlp, 0, sizeof ovlp);

  if (non_blocking)
    flags |= LOCKFILE_FAIL_IMMEDIATELY;
  if (exclusive)
    flags |= LOCKFILE_EXCLUSIVE_LOCK;

  return LockFileEx (h, flags, 0, size_lower, size_upper, &ovlp);
}

/* Unlock reader or exclusive lock. */
static BOOL
do_unlock (HANDLE h)
{
  int res;
  DWORD size_lower, size_upper;

  res = file_size (h, &size_lower, &size_upper);
  if (!res)
    return 0;

  return UnlockFile (h, 0, 0, size_lower, size_upper);
}

/* Now our BSD-like flock operation. */
int
flock (int fd, int operation)
{
  HANDLE h = (HANDLE) _get_osfhandle (fd);
  DWORD res;
  int non_blocking;

  if (h == INVALID_HANDLE_VALUE)
    {
      errno = EBADF;
      return -1;
    }

  non_blocking = operation & LOCK_NB;
  operation &= ~LOCK_NB;

  switch (operation)
    {
    case LOCK_SH:
      res = do_lock (h, non_blocking, 0);
      break;
    case LOCK_EX:
      res = do_lock (h, non_blocking, 1);
      break;
    case LOCK_UN:
      res = do_unlock (h);
      break;
    default:
      errno = EINVAL;
      return -1;
    }

  /* Map Windows errors into Unix errnos.  As usual MSDN fails to
   * document the permissible error codes.
   */
  if (!res)
    {
      DWORD err = GetLastError ();
      switch (err)
        {
          /* This means someone else is holding a lock. */
        case ERROR_LOCK_VIOLATION:
          errno = EAGAIN;
          break;

          /* Out of memory. */
        case ERROR_NOT_ENOUGH_MEMORY:
          errno = ENOMEM;
          break;

        case ERROR_BAD_COMMAND:
          errno = EINVAL;
          break;

          /* Unlikely to be other errors, but at least don't lose the
           * error code.
           */
        default:
          errno = err;
        }

      return -1;
    }

  return 0;
}

#else /* !Windows */

# ifdef HAVE_STRUCT_FLOCK_L_TYPE
/* We know how to implement flock in terms of fcntl. */

#  include <fcntl.h>

#  ifdef HAVE_UNISTD_H
#   include <unistd.h>
#  endif

#  include <errno.h>
#  include <string.h>

int
flock (int fd, int operation)
{
  int cmd, r;
  struct flock fl;

  if (operation & LOCK_NB)
    cmd = F_SETLK;
  else
    cmd = F_SETLKW;
  operation &= ~LOCK_NB;

  memset (&fl, 0, sizeof fl);
  fl.l_whence = SEEK_SET;
  /* l_start & l_len are 0, which as a special case means "whole file". */

  switch (operation)
    {
    case LOCK_SH:
      fl.l_type = F_RDLCK;
      break;
    case LOCK_EX:
      fl.l_type = F_WRLCK;
      break;
    case LOCK_UN:
      fl.l_type = F_UNLCK;
      break;
    default:
      errno = EINVAL;
      return -1;
    }

  r = fcntl (fd, cmd, &fl);
  if (r == -1 && errno == EACCES)
    errno = EAGAIN;

  return r;
}

# else /* !HAVE_STRUCT_FLOCK_L_TYPE */

#  error "This platform lacks flock function, and Gnulib doesn't provide a replacement. This is a bug in Gnulib."

# endif /* !HAVE_STRUCT_FLOCK_L_TYPE */

#endif /* !Windows */
