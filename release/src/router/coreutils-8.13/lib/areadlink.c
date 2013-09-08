/* areadlink.c -- readlink wrapper to return the link name in malloc'd storage
   Unlike xreadlink and xreadlink_with_size, don't ever call exit.

   Copyright (C) 2001, 2003-2007, 2009-2011 Free Software Foundation, Inc.

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

/* Written by Jim Meyering <jim@meyering.net>
   and Bruno Haible <bruno@clisp.org>.  */

#include <config.h>

/* Specification.  */
#include "areadlink.h"

#include "careadlinkat.h"

/* Call readlink to get the symbolic link value of FILENAME.
   Return a pointer to that NUL-terminated string in malloc'd storage.
   If readlink fails, return NULL and set errno.
   If allocation fails, or if the link value is longer than SIZE_MAX :-),
   return NULL and set errno to ENOMEM.  */

char *
areadlink (char const *filename)
{
  return careadlinkat (AT_FDCWD, filename, NULL, 0, NULL, careadlinkatcwd);
}
