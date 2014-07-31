/* POSIX compatible read() function.
   Copyright (C) 2008-2011 Free Software Foundation, Inc.
   Written by Bruno Haible <bruno@clisp.org>, 2011.

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

#include <config.h>

/* Specification.  */
#include <unistd.h>

/* Replace this function only if module 'nonblocking' is requested.  */
#if GNULIB_NONBLOCKING

# if (defined _WIN32 || defined __WIN32__) && ! defined __CYGWIN__

#  include <errno.h>
#  include <io.h>

#  define WIN32_LEAN_AND_MEAN  /* avoid including junk */
#  include <windows.h>

ssize_t
rpl_read (int fd, void *buf, size_t count)
#undef read
{
  ssize_t ret = read (fd, buf, count);

  if (ret < 0
      && GetLastError () == ERROR_NO_DATA)
    {
      HANDLE h = (HANDLE) _get_osfhandle (fd);
      if (GetFileType (h) == FILE_TYPE_PIPE)
        {
          /* h is a pipe or socket.  */
          DWORD state;
          if (GetNamedPipeHandleState (h, &state, NULL, NULL, NULL, NULL, 0)
              && (state & PIPE_NOWAIT) != 0)
            /* h is a pipe in non-blocking mode.
               Change errno from EINVAL to EAGAIN.  */
            errno = EAGAIN;
        }
    }
  return ret;
}

# endif
#endif
