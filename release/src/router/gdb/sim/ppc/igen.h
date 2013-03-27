/*  This file is part of the program psim.

    Copyright (C) 1994,1995,1996, Andrew Cagney <cagney@highland.com.au>

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


/* What does the instruction look like - bit ordering and size */
extern int hi_bit_nr;
extern int insn_bit_size;


/* generation options: */


enum {
  generate_with_direct_access = 0x1,
  generate_with_icache = 0x2,
  generate_with_semantic_icache = 0x4,
  generate_with_insn_in_icache = 0x8,
};


typedef enum {

  /* Transfer control to an instructions semantic code using the the
     standard call/return mechanism */

  generate_calls = 0x100,

  /* In addition, when refering to fields access them directly instead
     of via variables */

  generate_calls_with_direct_access
    = generate_calls | generate_with_direct_access,

  /* In addition, pre-decode an instructions opcode fields (entering
     them into an icache) so that semantic code can avoid the need to
     re-decode fields each time it is executed */

  generate_calls_with_icache
    = generate_calls | generate_with_icache,

  /* In addition, the instruction decode code includes a duplicated
     copy of the instructions semantic code.  This avoids the need to
     perform two calls (one to decode an instructions opcode fields
     and one to execute the instruction) when there is a miss of the
     icache */

  generate_calls_with_semantic_icache
    = generate_calls_with_icache | generate_with_semantic_icache,

  /* In addition, the semantic function refers to icache entries
     directly instead of first moving them into local variables */

  generate_calls_with_direct_access_icache
    = generate_calls_with_icache | generate_with_direct_access,

  generate_calls_with_direct_access_semantic_icache
    = generate_calls_with_direct_access_icache | generate_with_semantic_icache,


  /* Transfer control to an instructions semantic code using
     (computed) goto's instead of the more conventional call/return
     mechanism */

  generate_jumps = 0x200,

  /* As for generate jumps but with instruction fields accessed
     directly */

  generate_jumps_with_direct_access
    = generate_jumps | generate_with_direct_access,

  /* As for generate_calls_with_icache but applies to jumping code */

  generate_jumps_with_icache
    = generate_jumps | generate_with_icache,

  /* As for generate_calls_with_semantic_icache but applies to jumping
     code */

  generate_jumps_with_semantic_icache
    = generate_jumps_with_icache | generate_with_semantic_icache,

  /* As for generate_calls_with_direct_access */

  generate_jumps_with_direct_access_icache
    = generate_jumps_with_icache | generate_with_direct_access,

  generate_jumps_with_direct_access_semantic_icache
    = generate_jumps_with_direct_access_icache | generate_with_semantic_icache,

} igen_code;

extern igen_code code;




extern int icache_size;


/* Instruction expansion?

   Should the semantic code for each instruction, when the oportunity
   arrises, be expanded according to the variable opcode files that
   the instruction decode process renders constant */

extern int generate_expanded_instructions;


/* SMP?

   Should the generated code include SMP support (>0) and if so, for
   how many processors? */

extern int generate_smp;




/* Misc junk */



/* Function header definitions */


/* Cache functions: */

#define ICACHE_FUNCTION_FORMAL \
"cpu *processor,\n\
 instruction_word instruction,\n\
 unsigned_word cia,\n\
 idecode_cache *cache_entry"

#define ICACHE_FUNCTION_ACTUAL "processor, instruction, cia, cache_entry"

#define ICACHE_FUNCTION_TYPE \
((code & generate_with_semantic_icache) \
 ? SEMANTIC_FUNCTION_TYPE \
 : "idecode_semantic *")


/* Semantic functions: */

#define SEMANTIC_FUNCTION_FORMAL \
((code & generate_with_icache) \
 ? "cpu *processor,\n idecode_cache *cache_entry,\n unsigned_word cia" \
 : "cpu *processor,\n instruction_word instruction,\n unsigned_word cia")

#define SEMANTIC_FUNCTION_ACTUAL \
((code & generate_with_icache) \
 ? "processor, instruction, cia, cache_entry" \
 : "processor, instruction, cia")

#define SEMANTIC_FUNCTION_TYPE "unsigned_word"



extern void print_my_defines
(lf *file,
 insn_bits *expanded_bits,
 table_entry *file_entry);

extern void print_itrace
(lf *file,
 table_entry *file_entry,
 int idecode);


typedef enum {
  function_name_prefix_semantics,
  function_name_prefix_idecode,
  function_name_prefix_itable,
  function_name_prefix_icache,
  function_name_prefix_none
} lf_function_name_prefixes;

extern int print_function_name
(lf *file,
 const char *basename,
 insn_bits *expanded_bits,
 lf_function_name_prefixes prefix);
