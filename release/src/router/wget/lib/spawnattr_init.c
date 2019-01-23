/* Copyright (C) 2000, 2009-2017 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

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
#include <spawn.h>

#include <string.h>

/* Initialize data structure for file attribute for 'spawn' call.  */
int
posix_spawnattr_init (posix_spawnattr_t *attr)
{
  /* All elements have to be initialized to the default values which
     is generally zero.  */
  memset (attr, '\0', sizeof (*attr));

  return 0;
}
