/* The IGEN simulator generator for GDB, the GNU Debugger.

   Copyright 2002, 2007 Free Software Foundation, Inc.

   Contributed by Andrew Cagney.

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



/* For backward compatibility only - load a standalone cache macro table */

/* Instruction unpacking:

   Once the instruction has been decoded, the register (and other)
   fields within the instruction need to be extracted.

   The table that follows determines how each field should be treated.
   Importantly it considers the case where the extracted field is to
   be used immediatly or stored in an instruction cache.

   <type>

   Indicates what to do with the cache entry.  If a cache is to be
   used.  SCRATCH and CACHE values are defined when a cache entry is
   being filled while CACHE and COMPUTE values are defined in the
   semantic code.

   Zero marks the end of the table.  More importantly 1. indicates
   that the entry is valid and can be cached. 2. indicates that that
   the entry is valid but can not be cached.

   <field_name>

   The field name as given in the instruction spec.

   <derived_name>

   A new name for <field_name> once it has been extracted from the
   instruction (and possibly stored in the instruction cache).

   <type>

   String specifying the storage type for <new_name> (the extracted
   field>.

   <expression>

   Specifies how to get <new_name> from <old_name>.  If null, old and
   new name had better be the same. */


extern cache_entry *load_cache_table (char *file_name);
