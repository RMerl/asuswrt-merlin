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


/* Output code to manipulate the instruction cache: either create it
   or reference it */

typedef enum
{
  declare_variables,
  define_variables,
  undef_variables,
}
icache_decl_type;

typedef enum
{
  do_not_use_icache = 0,
  get_values_from_icache = 0x1,
  put_values_in_icache = 0x2,
  both_values_and_icache = 0x3,
}
icache_body_type;

extern void print_icache_body
  (lf *file,
   insn_entry * instruction,
   opcode_bits *expanded_bits,
   cache_entry *cache_rules,
   icache_decl_type what_to_declare,
   icache_body_type what_to_do, int nr_prefetched_words);


/* Output an instruction cache decode function */

extern void print_icache_declaration
  (lf *file,
   insn_entry * insn,
   opcode_bits *expanded_bits,
   insn_opcodes *opcodes, int nr_prefetched_words);

extern void print_icache_definition
  (lf *file,
   insn_entry * insn,
   opcode_bits *expanded_bits,
   insn_opcodes *opcodes, cache_entry *cache_rules, int nr_prefetched_words);


/* Output an instruction cache support function */

extern function_entry_handler print_icache_internal_function_declaration;
extern function_entry_handler print_icache_internal_function_definition;


/* Output the instruction cache table data structure */

extern void print_icache_struct
  (lf *file, insn_table *instructions, cache_entry *cache_rules);


/* Output a single instructions decoder */
