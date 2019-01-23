/* POSIX compatible FILE stream write function.
   Copyright (C) 2008-2017 Free Software Foundation, Inc.
   Written by Bruno Haible <bruno@clisp.org>, 2008.

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
#include <stdio.h>

/* Replace these functions only if module 'nonblocking' or module 'sigpipe' is
   requested.  */
#if GNULIB_NONBLOCKING || GNULIB_SIGPIPE

/* On native Windows platforms, SIGPIPE does not exist.  When write() is
   called on a pipe with no readers, WriteFile() fails with error
   GetLastError() = ERROR_NO_DATA, and write() in consequence fails with
   error EINVAL.  This write() function is at the basis of the function
   which flushes the buffer of a FILE stream.  */

# if (defined _WIN32 || defined __WIN32__) && ! defined __CYGWIN__

#  include <errno.h>
#  include <signal.h>
#  include <io.h>

#  define WIN32_LEAN_AND_MEAN  /* avoid including junk */
#  include <windows.h>

#  include "msvc-nothrow.h"

#  if GNULIB_NONBLOCKING
#   define CLEAR_ERRNO \
      errno = 0;
#   define HANDLE_ENOSPC \
          if (errno == ENOSPC && ferror (stream))                             \
            {                                                                 \
              int fd = fileno (stream);                                       \
              if (fd >= 0)                                                    \
                {                                                             \
                  HANDLE h = (HANDLE) _get_osfhandle (fd);                    \
                  if (GetFileType (h) == FILE_TYPE_PIPE)                      \
                    {                                                         \
                      /* h is a pipe or socket.  */                           \
                      DWORD state;                                            \
                      if (GetNamedPipeHandleState (h, &state, NULL, NULL,     \
                                                   NULL, NULL, 0)             \
                          && (state & PIPE_NOWAIT) != 0)                      \
                        /* h is a pipe in non-blocking mode.                  \
                           Change errno from ENOSPC to EAGAIN.  */            \
                        errno = EAGAIN;                                       \
                    }                                                         \
                }                                                             \
            }                                                                 \
          else
#  else
#   define CLEAR_ERRNO
#   define HANDLE_ENOSPC
#  endif

#  if GNULIB_SIGPIPE
#   define CLEAR_LastError \
      SetLastError (0);
#   define HANDLE_ERROR_NO_DATA \
          if (GetLastError () == ERROR_NO_DATA && ferror (stream))            \
            {                                                                 \
              int fd = fileno (stream);                                       \
              if (fd >= 0                                                     \
                  && GetFileType ((HANDLE) _get_osfhandle (fd))               \
                     == FILE_TYPE_PIPE)                                       \
                {                                                             \
                  /* Try to raise signal SIGPIPE.  */                         \
                  raise (SIGPIPE);                                            \
                  /* If it is currently blocked or ignored, change errno from \
                     EINVAL to EPIPE.  */                                     \
                  errno = EPIPE;                                              \
                }                                                             \
            }                                                                 \
          else
#  else
#   define CLEAR_LastError
#   define HANDLE_ERROR_NO_DATA
#  endif

#  define CALL_WITH_SIGPIPE_EMULATION(RETTYPE, EXPRESSION, FAILED) \
  if (ferror (stream))                                                        \
    return (EXPRESSION);                                                      \
  else                                                                        \
    {                                                                         \
      RETTYPE ret;                                                            \
      CLEAR_ERRNO                                                             \
      CLEAR_LastError                                                         \
      ret = (EXPRESSION);                                                     \
      if (FAILED)                                                             \
        {                                                                     \
          HANDLE_ENOSPC                                                       \
          HANDLE_ERROR_NO_DATA                                                \
          ;                                                                   \
        }                                                                     \
      return ret;                                                             \
    }

#  if !REPLACE_PRINTF_POSIX /* avoid collision with printf.c */
int
printf (const char *format, ...)
{
  int retval;
  va_list args;

  va_start (args, format);
  retval = vfprintf (stdout, format, args);
  va_end (args);

  return retval;
}
#  endif

#  if !REPLACE_FPRINTF_POSIX /* avoid collision with fprintf.c */
int
fprintf (FILE *stream, const char *format, ...)
{
  int retval;
  va_list args;

  va_start (args, format);
  retval = vfprintf (stream, format, args);
  va_end (args);

  return retval;
}
#  endif

#  if !REPLACE_VPRINTF_POSIX /* avoid collision with vprintf.c */
int
vprintf (const char *format, va_list args)
{
  return vfprintf (stdout, format, args);
}
#  endif

#  if !REPLACE_VFPRINTF_POSIX /* avoid collision with vfprintf.c */
int
vfprintf (FILE *stream, const char *format, va_list args)
#undef vfprintf
{
  CALL_WITH_SIGPIPE_EMULATION (int, vfprintf (stream, format, args), ret == EOF)
}
#  endif

int
putchar (int c)
{
  return fputc (c, stdout);
}

int
fputc (int c, FILE *stream)
#undef fputc
{
  CALL_WITH_SIGPIPE_EMULATION (int, fputc (c, stream), ret == EOF)
}

int
fputs (const char *string, FILE *stream)
#undef fputs
{
  CALL_WITH_SIGPIPE_EMULATION (int, fputs (string, stream), ret == EOF)
}

int
puts (const char *string)
#undef puts
{
  FILE *stream = stdout;
  CALL_WITH_SIGPIPE_EMULATION (int, puts (string), ret == EOF)
}

size_t
fwrite (const void *ptr, size_t s, size_t n, FILE *stream)
#undef fwrite
{
  CALL_WITH_SIGPIPE_EMULATION (size_t, fwrite (ptr, s, n, stream), ret < n)
}

# endif
#endif
