/* Duplicate an open file descriptor to a specified file descriptor.

   Copyright (C) 1999, 2004-2007, 2009-2011 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* written by Paul Eggert */

#include <config.h>

/* Specification.  */
#include <unistd.h>

#include <errno.h>
#include <fcntl.h>

#if (defined _WIN32 || defined __WIN32__) && ! defined __CYGWIN__
/* Get declarations of the Win32 API functions.  */
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
#endif

#if HAVE_DUP2

# undef dup2

int
rpl_dup2 (int fd, int desired_fd)
{
  int result;
# if (defined _WIN32 || defined __WIN32__) && ! defined __CYGWIN__
  /* If fd is closed, mingw hangs on dup2 (fd, fd).  If fd is open,
     dup2 (fd, fd) returns 0, but all further attempts to use fd in
     future dup2 calls will hang.  */
  if (fd == desired_fd)
    {
      if ((HANDLE) _get_osfhandle (fd) == INVALID_HANDLE_VALUE)
        {
          errno = EBADF;
          return -1;
        }
      return fd;
    }
  /* Wine 1.0.1 return 0 when desired_fd is negative but not -1:
     http://bugs.winehq.org/show_bug.cgi?id=21289 */
  if (desired_fd < 0)
    {
      errno = EBADF;
      return -1;
    }
# elif !defined __linux__
  /* On Haiku, dup2 (fd, fd) mistakenly clears FD_CLOEXEC.  */
  if (fd == desired_fd)
    return fcntl (fd, F_GETFL) == -1 ? -1 : fd;
# endif
  result = dup2 (fd, desired_fd);
# ifdef __linux__
  /* Correct a Linux return value.
     <http://git.kernel.org/?p=linux/kernel/git/stable/linux-2.6.30.y.git;a=commitdiff;h=2b79bc4f7ebbd5af3c8b867968f9f15602d5f802>
   */
  if (fd == desired_fd && result == (unsigned int) -EBADF)
    {
      errno = EBADF;
      result = -1;
    }
# endif
  if (result == 0)
    result = desired_fd;
  /* Correct a cygwin 1.5.x errno value.  */
  else if (result == -1 && errno == EMFILE)
    errno = EBADF;
# if REPLACE_FCHDIR
  if (fd != desired_fd && result != -1)
    result = _gl_register_dup (fd, result);
# endif
  return result;
}

#else /* !HAVE_DUP2 */

/* On older platforms, dup2 did not exist.  */

# ifndef F_DUPFD
static int
dupfd (int fd, int desired_fd)
{
  int duplicated_fd = dup (fd);
  if (duplicated_fd < 0 || duplicated_fd == desired_fd)
    return duplicated_fd;
  else
    {
      int r = dupfd (fd, desired_fd);
      int e = errno;
      close (duplicated_fd);
      errno = e;
      return r;
    }
}
# endif

int
dup2 (int fd, int desired_fd)
{
  int result = fcntl (fd, F_GETFL) < 0 ? -1 : fd;
  if (result == -1 || fd == desired_fd)
    return result;
  close (desired_fd);
# ifdef F_DUPFD
  result = fcntl (fd, F_DUPFD, desired_fd);
#  if REPLACE_FCHDIR
  if (0 <= result)
    result = _gl_register_dup (fd, result);
#  endif
# else
  result = dupfd (fd, desired_fd);
# endif
  if (result == -1 && (errno == EMFILE || errno == EINVAL))
    errno = EBADF;
  return result;
}
#endif /* !HAVE_DUP2 */
