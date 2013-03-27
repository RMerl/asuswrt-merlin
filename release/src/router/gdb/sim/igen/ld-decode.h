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


/* Instruction decode table:

   <decode-rule> ::=
       { <option> }
       ":" [ <first> ]
       ":" [ <last> ]
       ":" [ <force-first> ]
       ":" [ <force-last> ]
       ":" [ <constant-field-names> ]
       ":" [ <word-nr> ]
       ":" [ <format-names> ]
       ":" [ <model-names> ]
       ":" [ <constant> ]
       ":" [ <path> { "," <path> } ]
       { ":" <special-mask>
         ":" [ "!" ] <special-value>
         ":" <word-nr> }
       <nl>
       ;


   <path> ::= <int> "," <int> ;;

   <option> ::=
       <reserved-options>
       | <code-options>
       | <optimize-options>
       | <decode-options>
       | <constant>
       | <search-options>
       ;

   <reserved-options> ::= "zero-reserved" ;
   <gen-options> ::= "array" | "switch" | "padded-switch" | "goto-switch" ;
   <optimize-options> ::= "duplicate" | "combine"
   <decode-options> ::= "normal" | "boolean" ;
   <search-options> ::= "constants" | "variables" | "mixed"

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

   <force_reserved>

   Treat `/' (reserved) fields as a constant (zero) instead of
   variable when looking for an instruction field.

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


typedef enum
{
  normal_decode_rule,
  boolean_rule,
}
decode_special_type;


typedef enum
{
  invalid_gen,
  array_gen,
  switch_gen,
  padded_switch_gen,
  goto_switch_gen,
}
decode_gen_type;


enum
{
  decode_cond_mask_field,
  decode_cond_value_field,
  decode_cond_word_nr_field,
  nr_decode_cond_fields,
};

typedef struct _decode_path decode_path;
struct _decode_path
{
  int opcode_nr;
  decode_path *parent;
};

typedef struct _decode_path_list decode_path_list;
struct _decode_path_list
{
  decode_path *path;
  decode_path_list *next;
};


typedef struct _decode_cond decode_cond;
struct _decode_cond
{
  int word_nr;
  int mask[max_insn_bit_size];
  int value[max_insn_bit_size];
  int is_equal;
  decode_cond *next;
};

typedef enum
{
  decode_find_mixed,
  decode_find_constants,
  decode_find_strings,
}
decode_search_type;

enum
{
  decode_options_field,
  decode_first_field,
  decode_last_field,
  decode_force_first_field,
  decode_force_last_field,
  decode_constant_field_names_field,
  decode_word_nr_field,
  decode_format_names_field,
  decode_model_names_field,
  decode_paths_field,
  nr_decode_fields,
  min_nr_decode_fields = decode_last_field + 1,
};


typedef struct _decode_table decode_table;
struct _decode_table
{
  line_ref *line;
  decode_special_type type;
  decode_gen_type gen;
  decode_search_type search;
  int first;
  int last;
  int force_first;
  int force_last;
  filter *constant_field_names;
  int word_nr;
  /* if a boolean */
  unsigned constant;
  /* options */
  int with_zero_reserved;
  int with_duplicates;
  int with_combine;
  /* conditions on the rule being applied */
  decode_path_list *paths;
  filter *format_names;
  filter *model_names;
  decode_cond *conditions;
  decode_table *next;
};


extern decode_table *load_decode_table (char *file_name);

extern int decode_table_max_word_nr (decode_table *rule);

extern void dump_decode_rule
  (lf *file, char *prefix, decode_table *rule, char *suffix);
