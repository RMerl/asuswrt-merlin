/* An ftell() function that works around platform bugs.
   Copyright (C) 2007-2017 Free Software Foundation, Inc.

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

#include <errno.h>
#include <limits.h>

long
ftell (FILE *fp)
{
  /* Use the replacement ftello function with all its workarounds.  */
  off_t offset = ftello (fp);
  if (LONG_MIN <= offset && offset <= LONG_MAX)
    return /* (long) */ offset;
  else
    {
      errno = EOVERFLOW;
      return -1;
    }
}
