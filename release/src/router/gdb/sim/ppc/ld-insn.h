/*  This file is part of the program psim.

    Copyright 1994, 1995, 1996, 2003 Andrew Cagney

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

/*
#   --
#
#
# Fields:
#
#	1	Instruction format as a `start-bit,content' pairs.
#		the content is one of a digit, field name or `/' (aka.0)
#
#	2	Format specifier
#
#	3	Flags:	64 - 64bit only
#			f - floating point enabled required
#
#	4 	short name
#
#	5	Description
#
#
# For flags marked 'model', the fields are interpreted as follows:
#
#	1	Not used
#
#	2	Not used
#
#	3	"macro"
#
#	4	String name for model
#
#	5	Specific CPU model, must be an identifier
#
#	6	Comma separated list of functional units

*/


/* Global constants */

enum {
  max_insn_bit_size = 32,
};


typedef struct _insn_field insn_field;
struct _insn_field {
  int first;
  int last;
  int width;
  int is_int;
  int is_slash;
  int is_string;
  int val_int;
  char *pos_string;
  char *val_string;
  insn_field *next;
  insn_field *prev;
};

typedef struct _insn_fields insn_fields;
struct _insn_fields {
  insn_field *bits[max_insn_bit_size];
  insn_field *first;
  insn_field *last;
  unsigned value;
};


/****************************************************************/

typedef struct _opcode_field opcode_field;
struct _opcode_field {
  int first;
  int last;
  int is_boolean;
  unsigned boolean_constant;
  opcode_field *parent;
};


/****************************************************************/

typedef struct _insn_bits insn_bits;
struct _insn_bits {
  int is_expanded;
  int value;
  insn_field *field;
  opcode_field *opcode;
  insn_bits *last;
};


/****************************************************************/


typedef enum {
  insn_format,
  insn_form,
  insn_flags,
  insn_mnemonic,
  insn_name,
  insn_comment,
  insn_field_6,
  insn_field_7,
  nr_insn_table_fields
} insn_table_fields;

typedef enum {
  function_type = insn_format,
  function_name = insn_name,
  function_param = insn_comment
} function_table_fields;

typedef enum {
  model_name = insn_mnemonic,
  model_identifer = insn_name,
  model_default = insn_comment,
} model_table_fields;

typedef enum {
  include_flags = insn_flags,
  include_path = insn_name,
} model_include_fields;

typedef enum {
  cache_type_def = insn_name,
  cache_derived_name = insn_comment,
  cache_name = insn_field_6,
  cache_expression = insn_field_7,
} cache_fields;

typedef struct _insn insn;
struct _insn {
  table_entry *file_entry;
  insn_fields *fields;
  insn *next;
};

typedef struct _insn_undef insn_undef;
struct _insn_undef {
  insn_undef *next;
  char *name;
};

typedef struct _model model;
struct _model {
  model *next;
  char *name;
  char *printable_name;
  char *insn_default;
  table_model_entry *func_unit_start;
  table_model_entry *func_unit_end;
};
  
typedef struct _insn_table insn_table;
struct _insn_table {
  int opcode_nr;
  insn_bits *expanded_bits;
  int nr_insn;
  insn *insns;
  insn *functions;
  insn *last_function;
  decode_table *opcode_rule;
  opcode_field *opcode;
  int nr_entries;
  insn_table *entries;
  insn_table *sibling;
  insn_table *parent;
};

typedef enum {
  insn_model_name,
  insn_model_fields,
  nr_insn_model_table_fields
} insn_model_table_fields;


extern insn_table *load_insn_table
(const char *file_name,
 decode_table *decode_rules,
 filter *filters,
 table_include *includes,
 cache_table **cache_rules);

model *models;
model *last_model;

insn *model_macros;
insn *last_model_macro;

insn *model_functions;
insn *last_model_function;

insn *model_internal;
insn *last_model_internal;

insn *model_static;
insn *last_model_static;

insn *model_data;
insn *last_model_data;

int max_model_fields_len;

extern void insn_table_insert_insn
(insn_table *table,
 table_entry *file_entry,
 insn_fields *fields);


/****************************************************************/

/****************************************************************/

typedef void leaf_handler
(insn_table *entry,
 lf *file,
 void *data,
 int depth);

typedef void insn_handler
(insn_table *table,
 lf *file,
 void *data,
 insn *instruction,
 int depth);

typedef void padding_handler
(insn_table *table,
 lf *file,
 void *data,
 int depth,
 int opcode_nr);


extern void insn_table_traverse_tree
(insn_table *table,
 lf *file,
 void *data,
 int depth,
 leaf_handler *start,
 insn_handler *handler,
 leaf_handler *end,
 padding_handler *padding);


extern void insn_table_traverse_insn
(insn_table *table,
 lf *file,
 void *data,
 insn_handler *handler);



/****************************************************************/

typedef void function_handler
(insn_table *table,
 lf *file,
 void *data,
 table_entry *function);

extern void
insn_table_traverse_function
(insn_table *table,
 lf *file,
 void *data,
 function_handler *leaf);

/****************************************************************/



extern void insn_table_expand_insns
(insn_table *table);

extern int insn_table_depth
(insn_table *table);
