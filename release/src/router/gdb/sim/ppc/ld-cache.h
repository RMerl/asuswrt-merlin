/*  This file is part of the program psim.

    Copyright 1994, 1995, 1996, 1997, 2003, Andrew Cagney

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
 
    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 
    */

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


typedef enum {
  scratch_value,
  cache_value,
  compute_value,
} cache_rule_type;

typedef struct _cache_table cache_table;
struct _cache_table {
  cache_rule_type type;
  char *field_name;
  char *derived_name;
  char *type_def;
  char *expression;
  table_entry *file_entry;
  cache_table *next;
};


extern cache_table *load_cache_table
(char *file_name,
 int hi_bit_nr);

extern void append_cache_rule
(cache_table **table,
 char *type,
 char *field_name,
 char *derived_name,
 char *type_def,
 char *expression,
 table_entry *file_entry);

