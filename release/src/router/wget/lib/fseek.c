/* An fseek() function that, together with fflush(), is POSIX compliant.
   Copyright (C) 2007, 2009-2017 Free Software Foundation, Inc.

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

/* Get off_t.  */
#include <unistd.h>

int
fseek (FILE *fp, long offset, int whence)
{
  /* Use the replacement fseeko function with all its workarounds.  */
  return fseeko (fp, (off_t)offset, whence);
}
