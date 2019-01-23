/* Create a pipe.
   Copyright (C) 2009-2017 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, see <http://www.gnu.org/licenses/>.  */

#include <config.h>

/* Specification.  */
#include <unistd.h>

#if (defined _WIN32 || defined __WIN32__) && ! defined __CYGWIN__
/* Native Windows API.  */

/* Get _pipe().  */
# include <io.h>

/* Get _O_BINARY.  */
# include <fcntl.h>

int
pipe (int fd[2])
{
  /* Mingw changes fd to {-1,-1} on failure, but this violates
     http://austingroupbugs.net/view.php?id=467 */
  int tmp[2];
  int result = _pipe (tmp, 4096, _O_BINARY);
  if (!result)
    {
      fd[0] = tmp[0];
      fd[1] = tmp[1];
    }
  return result;
}

#else

# error "This platform lacks a pipe function, and Gnulib doesn't provide a replacement. This is a bug in Gnulib."

#endif
