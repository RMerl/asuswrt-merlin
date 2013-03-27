/*  This file is part of the program psim.

    Copyright (C) 1994-1997 Andrew Cagney <cagney@highland.com.au>

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



/* Output code to manipulate the instruction cache: either create it
   or reference it */

typedef enum {
  declare_variables,
  define_variables,
  undef_variables,
} icache_decl_type;

typedef enum {
  do_not_use_icache = 0,
  get_values_from_icache = 0x1,
  put_values_in_icache = 0x2,
  both_values_and_icache = 0x3,
} icache_body_type;

extern void print_icache_body
(lf *file,
 insn *instruction,
 insn_bits *expanded_bits,
 cache_table *cache_rules,
 icache_decl_type what_to_declare,
 icache_body_type what_to_do);


/* Output an instruction cache decode function */

extern insn_handler print_icache_declaration;
extern insn_handler print_icache_definition;


/* Output an instruction cache support function */

extern function_handler print_icache_internal_function_declaration;
extern function_handler print_icache_internal_function_definition;


/* Output the instruction cache table data structure */

extern void print_icache_struct
(insn_table *instructions,
 cache_table *cache_rules,
 lf *file);


/* Output a single instructions decoder */
