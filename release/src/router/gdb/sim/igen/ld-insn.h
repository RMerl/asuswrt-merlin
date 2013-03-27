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



typedef unsigned64 insn_uint;


/* Common among most entries:

   All non instruction records have the format:

   <...> ::=
       ":" <record-name>
       ":" <filter-flags>
       ":" <filter-models>
       ":" ...
   
 */

enum
{
  record_type_field = 1,
  old_record_type_field = 2,
  record_filter_flags_field = 2,
  record_filter_models_field = 3,
};


/* Include:

   Include the specified file.

   <include> ::=
       ":" "include"
       ":" <filter-flags>
       ":" <filter-models>
       ":" <filename>
       <nl>
       ;

   */

enum
{
  include_filename_field = 4,
  nr_include_fields,
};



/* Options:

   Valid options are: hi-bit-nr (default 0), insn-bit-size (default
   32), insn-specifying-widths (default true), multi-sim (default false).

   <option> ::=
       ":" "option"
       ":" <filter-flags>
       ":" <filter-models>
       ":" <option-name>
       ":" <option-value>
       <nl>
       ;

   <option-name> ::=
       "insn-bit-size"
       | "insn-specifying-widths"
       | "hi-bit-nr"
       | "flags-filter"
       | "model-filter"
       | "multi-sim"
       | "format-names"
       ;

   <option-value> ::=
       "true"
       | "false"
       | <integer>
       | <list>
       ;


   These update the global options structure. */


enum
{
  option_name_field = 4,
  option_value_field,
  nr_option_fields,
};



/* Macro definitions:

   <insn-macro> ::=
       ":" "define"
       ":" <filter-flags>
       ":" <filter-models>
       ":" <name>
       ":" <arg-list>
       ":" <expression>
       <nl>
       ;

   <arg-list> ::=
       [ <name> { "," <arg-list> } ]
       ;

   */


enum
{
  macro_name_field = 4,
  macro_args_field,
  macro_expr_field,
  nr_macro_fields,
};



/* Functions and internal routins:

   NB: <filter-models> and <function-models> are equivalent.


   <function> ::=
       ":" "function"
       <function-spec>
       ;

   <internal> ::=
       ":" "internal"
       <function-spec>
       ;

   <format> ::=
       ":" ( "%s" | ... )
       <function-spec>
       ;

   <function-model> ::=
       "*" [ <processor-list> ]
       ":"
       <nl>
       ;

   <function-spec> ::=
       ":" <filter-flags>
       ":" <filter-models>
       ":" <typedef>
       ":" <name>
       [ ":" <parameter-list> ]
       <nl>
       [ <function-model> ]
       <code-block>
       ;

   */

enum
{
  function_typedef_field = 4,
  function_name_field,
  function_param_field,
  nr_function_fields,
};

enum
{
  function_model_name_field = 0,
  nr_function_model_fields = 1,
};

enum
{
  old_function_typedef_field = 0,
  old_function_type_field = 2,
  old_function_name_field = 4,
  old_function_param_field = 5,
  nr_old_function_fields = 5,	/* parameter-list is optional */
};


typedef struct _function_entry function_entry;
struct _function_entry
{
  line_ref *line;
  filter *flags;
  filter *models;
  char *type;
  char *name;
  char *param;
  table_entry *code;
  int is_internal;
  function_entry *next;
};


typedef void function_entry_handler
  (lf *file, function_entry * function, void *data);

extern void function_entry_traverse
  (lf *file,
   function_entry * functions, function_entry_handler * handler, void *data);


/* cache-macro:

   <cache-macro> ::=
       ":" <macro-type>
       ":" <filter-flags>
       ":" <filter-models>
       ":" <typedef>
       ":" <name>
       ":" <field-name> { "," <field-name> }
       ":" <expression>
       <nl>
       ;

   <cache-macro-type> ::=
       "scratch"
       | "cache"
       | "compute"
       ;

   <name> ::=
       <ident>
       | <ident> "_is_" <integer>
       ;

   A cache entry is defined (for an instruction) when all
   <field-name>s are present as named opcode fields within the
   instructions format.

   SCRATCH and CACHE macros are defined during the cache fill stage
   while CACHE and COMPUTE macros are defined during the instruction
   execution stage.

   */

enum
{
  cache_typedef_field = 4,
  cache_name_field,
  cache_original_fields_field,
  cache_expression_field,
  nr_cache_fields,
};

typedef enum
{
  scratch_value,
  cache_value,
  compute_value,
}
cache_entry_type;

typedef struct _cache_entry cache_entry;
struct _cache_entry
{
  line_ref *line;
  filter *flags;
  filter *models;
  cache_entry_type entry_type;
  char *name;
  filter *original_fields;
  char *type;
  char *expression;
  cache_entry *next;
};



/* Model specs:

   <model-processor> ::=
       ":" "model"
       ":" <filter-flags>
       ":" <filter-models>
       ":" <processor>
       ":" <BFD-processor>
       ":" <function-unit-data>
       <nl>
       ;

   <model-macro> ::=
       ":" "model-macro"
       ":" <filter-flags>
       ":" <filter-models>
       <nl>
       <code-block>
       ;

   <model-data> ::=
       ":" "model-data"
       ":" <filter-flags>
       ":" <filter-models>
       <nl>
       <code-block>
       ;

   <model-static> ::=
       ":" "model-static"
       <function-spec>
       ;

   <model-internal> ::=
       ":" "model-internal"
       <function-spec>
       ;

   <model-function> ::=
       ":" "model-internal"
       <function-spec>
       ;

 */

enum
{
  nr_model_macro_fields = 4,
  nr_model_data_fields = 4,
  nr_model_static_fields = nr_function_fields,
  nr_model_internal_fields = nr_function_fields,
  nr_model_function_fields = nr_function_fields,
};

typedef struct _model_data model_data;
struct _model_data
{
  line_ref *line;
  filter *flags;
  table_entry *entry;
  table_entry *code;
  model_data *next;
};

enum
{
  model_name_field = 4,
  model_full_name_field,
  model_unit_data_field,
  nr_model_processor_fields,
};

typedef struct _model_entry model_entry;
struct _model_entry
{
  line_ref *line;
  filter *flags;
  char *name;
  char *full_name;
  char *unit_data;
  model_entry *next;
};


typedef struct _model_table model_table;
struct _model_table
{
  filter *processors;
  int nr_models;
  model_entry *models;
  model_data *macros;
  model_data *data;
  function_entry *statics;
  function_entry *internals;
  function_entry *functions;
};



/* Instruction format:

   An instruction is composed of a sequence of N bit instruction
   words.  Each word broken into a number of instruction fields.
   Those fields being constant (ex. an opcode) or variable (register
   spec).

   <insn-word> ::=
       <insn-field> { "," <insn-field> } ;

   <insn-field> ::=
       ( <binary-value-implying-width>
       | <field-name-implying-width>
       | [ <start-or-width> "." ] <field> 
       )
       { [ "!" | "=" ] [ <value> | <field-name> ] }
       ;

   <field> ::=
         { "*" }+
       | { "/" }+
       | <field-name>
       | "0x" <hex-value>
       | "0b" <binary-value>
       | "0" <octal-value>
       | <integer-value> ;

*/

typedef enum _insn_field_cond_type
{
  insn_field_cond_value,
  insn_field_cond_field,
}
insn_field_cond_type;
typedef enum _insn_field_cond_test
{
  insn_field_cond_eq,
  insn_field_cond_ne,
}
insn_field_cond_test;
typedef struct _insn_field_cond insn_field_cond;
struct _insn_field_cond
{
  insn_field_cond_type type;
  insn_field_cond_test test;
  insn_uint value;
  struct _insn_field_entry *field;
  char *string;
  insn_field_cond *next;
};


typedef enum _insn_field_type
{
  insn_field_invalid,
  insn_field_int,
  insn_field_reserved,
  insn_field_wild,
  insn_field_string,
}
insn_field_type;

typedef struct _insn_field_entry insn_field_entry;
struct _insn_field_entry
{
  int first;
  int last;
  int width;
  int word_nr;
  insn_field_type type;
  insn_uint val_int;
  char *pos_string;
  char *val_string;
  insn_field_cond *conditions;
  insn_field_entry *next;
  insn_field_entry *prev;
};

typedef struct _insn_bit_entry insn_bit_entry;
struct _insn_bit_entry
{
  int value;
  int mask;
  insn_field_entry *field;
};




typedef struct _insn_entry insn_entry;	/* forward */

typedef struct _insn_word_entry insn_word_entry;
struct _insn_word_entry
{
  /* list of sub-fields making up the instruction.  bit provides
     faster access to the field data for bit N.  */
  insn_field_entry *first;
  insn_field_entry *last;
  insn_bit_entry *bit[max_insn_bit_size];
  /* set of all the string fields */
  filter *field_names;
  /* For multi-word instructions, The Nth word (from zero). */
  insn_word_entry *next;
};



/* Instruction model:

   Provides scheduling and other data for the code modeling the
   instruction unit.

   <insn-model> ::=
       "*" [ <processor-list> ]
       ":" [ <function-unit-data> ]
       <nl>
       ;

   <processor-list> ::=
       <processor> { "," <processor>" }
       ;

   If the <processor-list> is empty, the model is made the default for
   this instruction.

   */

enum
{
  insn_model_name_field = 0,
  insn_model_unit_data_field = 1,
  nr_insn_model_fields = 1,
};

typedef struct _insn_model_entry insn_model_entry;
struct _insn_model_entry
{
  line_ref *line;
  insn_entry *insn;
  filter *names;
  char *full_name;
  char *unit_data;
  insn_model_entry *next;
};



/* Instruction mnemonic:

   List of assembler mnemonics for the instruction.

   <insn-mnenonic> ::=
       "\"" <assembler-mnemonic> "\""
       [ ":" <conditional-expression> ]
       <nl>
       ;

   An assembler mnemonic string has the syntax:

   <assembler-mnemonic>  ::=
       ( [ "%" <format-spec> ] "<" <func> [ "#" <param-list> ] ">"
       | "%%"
       | <other-letter>
       )+

    Where, for instance, the text is translated into a printf format
    and argument pair:

       "<FUNC>"         : "%ld", (long) FUNC
       "%<FUNC>..."     : "%...", FUNC
       "%s<FUNC>"       : "%s", <%s>FUNC (SD_, FUNC)
       "%s<FUNC#P1,P2>" : "%s", <%s>FUNC (SD_, P1,P2)
       "%lx<FUNC>"      : "%lx", (unsigned long) FUNC
       "%08lx<FUNC>"    : "%08lx", (unsigned long) FUNC

    And "<%s>FUNC" denotes a function declared using the "%s" record
    specifier.



       ;

   */

enum
{
  insn_mnemonic_format_field = 0,
  insn_mnemonic_condition_field = 1,
  nr_insn_mnemonic_fields = 1,
};

typedef struct _insn_mnemonic_entry insn_mnemonic_entry;
struct _insn_mnemonic_entry
{
  line_ref *line;
  insn_entry *insn;
  char *format;
  char *condition;
  insn_mnemonic_entry *next;
};



/* Instruction:

   <insn> ::=
       <insn-word> { "+" <insn-word> }
       ":" <format-name>
       ":" <filter-flags>
       ":" <options>
       ":" <name>
       <nl>
       { <insn-model> }
       { <insn-mnemonic> }
       <code-block>

 */

enum
{
  insn_word_field = 0,
  insn_format_name_field = 1,
  insn_filter_flags_field = 2,
  insn_options_field = 3,
  insn_name_field = 4,
  nr_insn_fields = 5,
};


/* typedef struct _insn_entry insn_entry; */
struct _insn_entry
{
  line_ref *line;
  filter *flags;		/* filtered by options.filters */
  char *format_name;
  filter *options;
  char *name;
  /* the words that make up the instruction. Word provides direct
     access to word N. Pseudo instructions can be identified by
     nr_words == 0. */
  int nr_words;
  insn_word_entry *words;
  insn_word_entry **word;
  /* a set of all the fields from all the words */
  filter *field_names;
  /* an array of processor models, missing models are NULL! */
  int nr_models;
  insn_model_entry *models;
  insn_model_entry **model;
  filter *processors;
  /* list of assember formats */
  int nr_mnemonics;
  insn_mnemonic_entry *mnemonics;
  /* code body */
  table_entry *code;
  insn_entry *next;
};


/* Instruction table:

 */

typedef struct _insn_table insn_table;
struct _insn_table
{
  cache_entry *caches;
  int max_nr_words;
  int nr_insns;
  insn_entry *insns;
  function_entry *functions;
  insn_entry *illegal_insn;
  model_table *model;
  filter *options;
  filter *flags;
};

extern insn_table *load_insn_table (char *file_name, cache_entry *cache);

typedef void insn_entry_handler
  (lf *file, insn_table *isa, insn_entry * insn, void *data);

extern void insn_table_traverse_insn
  (lf *file, insn_table *isa, insn_entry_handler * handler, void *data);



/* Printing */

extern void print_insn_words (lf *file, insn_entry * insn);



/* Debugging */

void
  dump_insn_field
  (lf *file, char *prefix, insn_field_entry *field, char *suffix);

void
  dump_insn_word_entry
  (lf *file, char *prefix, insn_word_entry *word, char *suffix);

void
  dump_insn_entry (lf *file, char *prefix, insn_entry * insn, char *suffix);

void
  dump_cache_entries
  (lf *file, char *prefix, cache_entry *entry, char *suffix);

void dump_insn_table (lf *file, char *prefix, insn_table *isa, char *suffix);
