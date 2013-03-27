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


typedef struct _opcode_field opcode_field;
struct _opcode_field
{
  int word_nr;
  int first;
  int last;
  int is_boolean;
  int nr_opcodes;
  unsigned boolean_constant;
  opcode_field *parent;
};

typedef struct _opcode_bits opcode_bits;
struct _opcode_bits
{
  int value;
  int first;
  int last;
  insn_field_entry *field;
  opcode_field *opcode;
  opcode_bits *next;
};

typedef struct _insn_opcodes insn_opcodes;
struct _insn_opcodes
{
  opcode_field *opcode;
  insn_opcodes *next;
};

typedef struct _insn_list insn_list;
struct _insn_list
{
  /* the instruction */
  insn_entry *insn;
  /* list of non constant bits that have been made constant */
  opcode_bits *expanded_bits;
  /* list of the various opcode field paths used to reach this
     instruction */
  insn_opcodes *opcodes;
  /* number of prefetched words for this instruction */
  int nr_prefetched_words;
  /* The semantic function list_entry corresponding to this insn */
  insn_list *semantic;
  /* linked list */
  insn_list *next;
};

/* forward */
typedef struct _gen_list gen_list;

typedef struct _gen_entry gen_entry;
struct _gen_entry
{

  /* as an entry in a table */
  int word_nr;
  int opcode_nr;
  gen_entry *sibling;
  opcode_bits *expanded_bits;
  gen_entry *parent;		/* parent has the opcode* data */

  /* as a table containing entries */
  decode_table *opcode_rule;
  opcode_field *opcode;
  int nr_prefetched_words;
  int nr_entries;
  gen_entry *entries;

  /* as both an entry and a table */
  int nr_insns;
  insn_list *insns;

  /* if siblings are being combined */
  gen_entry *combined_next;
  gen_entry *combined_parent;

  /* our top-of-tree */
  gen_list *top;
};


struct _gen_list
{
  model_entry *model;
  insn_table *isa;
  gen_entry *table;
  gen_list *next;
};


typedef struct _gen_table gen_table;
struct _gen_table
{
  /* list of all the instructions */
  insn_table *isa;
  /* list of all the semantic functions */
  decode_table *rules;
  /* list of all the generated instruction tables */
  gen_list *tables;
  /* list of all the semantic functions */
  int nr_semantics;
  insn_list *semantics;
};


extern gen_table *make_gen_tables (insn_table *isa, decode_table *rules);


extern void gen_tables_expand_insns (gen_table *gen);

extern void gen_tables_expand_semantics (gen_table *gen);

extern int gen_entry_depth (gen_entry *table);



/* Traverse the created data structure */

typedef void gen_entry_handler
  (lf *file, gen_entry *entry, int depth, void *data);

extern void gen_entry_traverse_tree
  (lf *file,
   gen_entry *table,
   int depth,
   gen_entry_handler * start,
   gen_entry_handler * leaf, gen_entry_handler * end, void *data);



/* Misc functions - actually in igen.c */


/* Cache functions: */

extern int print_icache_function_formal (lf *file, int nr_prefetched_words);

extern int print_icache_function_actual (lf *file, int nr_prefetched_words);

extern int print_icache_function_type (lf *file);

extern int print_semantic_function_formal (lf *file, int nr_prefetched_words);

extern int print_semantic_function_actual (lf *file, int nr_prefetched_words);

extern int print_semantic_function_type (lf *file);

extern int print_idecode_function_formal (lf *file, int nr_prefetched_words);

extern int print_idecode_function_actual (lf *file, int nr_prefetched_words);

typedef enum
{
  function_name_prefix_semantics,
  function_name_prefix_idecode,
  function_name_prefix_itable,
  function_name_prefix_icache,
  function_name_prefix_engine,
  function_name_prefix_none
}
lf_function_name_prefixes;

typedef enum
{
  is_function_declaration = 0,
  is_function_definition = 1,
  is_function_variable,
}
function_decl_type;

extern int print_function_name
  (lf *file,
   const char *basename,
   const char *format_name,
   const char *model_name,
   opcode_bits *expanded_bits, lf_function_name_prefixes prefix);

extern void print_my_defines
  (lf *file,
   const char *basename, const char *format_name, opcode_bits *expanded_bits);

extern void print_itrace (lf *file, insn_entry * insn, int idecode);

extern void print_sim_engine_abort (lf *file, const char *message);


extern void print_include (lf *file, igen_module module);
extern void print_include_inline (lf *file, igen_module module);
extern void print_includes (lf *file);
