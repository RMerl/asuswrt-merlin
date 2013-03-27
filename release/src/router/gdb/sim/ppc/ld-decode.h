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

/* Instruction decode table:

   <options>:<first>:<last>:<force-first>:<force-last>:<force-expand>:<special>...



   Ignore the below:


   The instruction decode table contains rules that dictate how igen
   is going to firstly break down the opcode table and secondly

   The table that follows is used by gen to construct a decision tree
   that can identify each possible instruction.  Gen then outputs this
   decision tree as (according to config) a table or switch statement
   as the function idecode.

   In parallel to this, as mentioned above, WITH_EXPANDED_SEMANTICS
   determines of the semantic functions themselves should be expanded
   in a similar way.

   <first>
   <last>

   Range of bits (within the instruction) that should be searched for
   an instruction field.  Within such ranges, gen looks for opcodes
   (constants), registers (strings) and reserved bits (slash) and
   according to the rules that follows includes or excludes them from
   a possible instruction field.

   <force_first>
   <force_last>

   If an instruction field was found, enlarge the field size so that
   it is forced to at least include bits starting from <force_first>
   (<force_last>).  To stop this occuring, use <force_first> = <last>
   + 1 and <force_last> = <first> - 1.

   <force_slash>

   Treat `/' fields as a constant instead of variable when looking for
   an instruction field.

   <force_expansion>

   Treat any contained register (string) fields as constant when
   determining the instruction field.  For the instruction decode (and
   controled by IDECODE_EXPAND_SEMANTICS) this forces the expansion of
   what would otherwize be non constant bits of an instruction.

   <use_switch>

   Should this table be expanded using a switch statement (val 1) and
   if so, should it be padded with entries so as to force the compiler
   to generate a jump table (val 2). Or a branch table (val 3).

   <special_mask>
   <special_value>
   <special_rule>
   <special_constant>

   Special rule to fine tune how specific (or groups) of instructions
   are expanded.  The applicability of the rule is determined by

     <special_mask> != 0 && (instruction> & <special_mask>) == <special_value>

   Where <instruction> is obtained by looking only at constant fields
   with in an instructions spec.  When determining an expansion, the
   rule is only considered when a node contains a single instruction.
   <special_rule> can be any of:

        0: for this instruction, expand by earlier rules
   	1: expand bits <force_low> .. <force_hi> only
	2: boolean expansion of only zero/non-zero cases
	3: boolean expansion of equality of special constant

	*/


typedef enum {
  normal_decode_rule,
  expand_forced_rule,
  boolean_rule,
  nr_decode_rules
} decode_special_type;

typedef enum {
  invalid_gen,
  array_gen,
  switch_gen,
  padded_switch_gen,
  goto_switch_gen,
  nr_decode_gen_types,
} decode_gen_type;


typedef struct _decode_table decode_table;
struct _decode_table {
  decode_special_type type;
  decode_gen_type gen;
  int first;
  int last;
  int force_first;
  int force_last;
  int force_slash;
  char *force_expansion;
  unsigned special_mask;
  unsigned special_value;
  unsigned special_constant;
  decode_table *next;
};


extern void force_decode_gen_type
(const char *type);

extern decode_table *load_decode_table
(char *file_name,
 int hi_bit_nr);

extern void dump_decode_rule
(decode_table *rule,
 int indent);
