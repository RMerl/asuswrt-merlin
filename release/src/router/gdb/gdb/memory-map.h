/* Routines for handling XML memory maps provided by target.

   Copyright (C) 2006, 2007 Free Software Foundation, Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */


#ifndef MEMORY_MAP_H
#define MEMORY_MAP_H

#include "memattr.h"

/* Parses XML memory map passed as argument and returns the memory
   regions it describes.  On any error, emits error message and
   returns 0. Does not throw.  Ownership of result is passed to the caller.  */
VEC(mem_region_s) *parse_memory_map (const char *memory_map);

#endif
