/* Copy piece of UTF-8/UTF-16/UTF-32 string.
   Copyright (C) 1999, 2002, 2006, 2009-2017 Free Software Foundation, Inc.
   Written by Bruno Haible <bruno@clisp.org>, 2002.

   This program is free software: you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published
   by the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include <string.h>

UNIT *
FUNC (UNIT *dest, const UNIT *src, size_t n)
{
#if 0
  UNIT *destptr = dest;

  for (; n > 0; n--)
    *destptr++ = *src++;
#else
  memcpy ((char *) dest, (const char *) src, n * sizeof (UNIT));
#endif
  return dest;
}
