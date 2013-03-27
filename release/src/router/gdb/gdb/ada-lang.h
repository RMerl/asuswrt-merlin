/* Ada language support definitions for GDB, the GNU debugger.

   Copyright (C) 1992, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005,
   2007 Free Software Foundation, Inc.

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

#if !defined (ADA_LANG_H)
#define ADA_LANG_H 1

struct partial_symbol;
struct frame_info;

#include "value.h"
#include "gdbtypes.h"
#include "breakpoint.h"

/* Names of specific files known to be part of the runtime
   system and that might consider (confusing) debugging information.
   Each name (a basic regular expression string) is followed by a
   comma.  FIXME: Should be part of a configuration file. */
#if defined(__alpha__) && defined(__osf__)
#define ADA_KNOWN_RUNTIME_FILE_NAME_PATTERNS \
   "^[agis]-.*\\.ad[bs]$", \
   "/usr/shlib/libpthread\\.so",
#elif defined (__linux__)
#define ADA_KNOWN_RUNTIME_FILE_NAME_PATTERNS \
   "^[agis]-.*\\.ad[bs]$", \
   "/lib.*/libpthread\\.so[.0-9]*$", "/lib.*/libpthread\\.a$", \
   "/lib.*/libc\\.so[.0-9]*$", "/lib.*/libc\\.a$",
#endif

#if !defined (ADA_KNOWN_RUNTIME_FILE_NAME_PATTERNS)
#define ADA_KNOWN_RUNTIME_FILE_NAME_PATTERNS \
   "^[agis]-.*\\.ad[bs]$",
#endif

/* Names of compiler-generated auxiliary functions probably of no
   interest to users. Each name (a basic regular expression string)
   is followed by a comma. */
#define ADA_KNOWN_AUXILIARY_FUNCTION_NAME_PATTERNS \
   "___clean[.$a-zA-Z0-9_]*$",

/* The maximum number of frame levels searched for non-local,
 * non-global symbols.  This limit exists as a precaution to prevent
 * infinite search loops when the stack is screwed up. */
#define MAX_ENCLOSING_FRAME_LEVELS 7

/* Maximum number of steps followed in looking for the ultimate
   referent of a renaming.  This prevents certain infinite loops that
   can otherwise result. */
#define MAX_RENAMING_CHAIN_LENGTH 10

struct block;

/* Corresponding encoded/decoded names and opcodes for Ada user-definable
   operators. */
struct ada_opname_map
{
  const char *encoded;
  const char *decoded;
  enum exp_opcode op;
};

/* Table of Ada operators in encoded and decoded forms. */
/* Defined in ada-lang.c */
extern const struct ada_opname_map ada_opname_table[];

enum ada_operator 
  {
    /* X IN A'RANGE(N).  N is an immediate operand, surrounded by 
       BINOP_IN_BOUNDS before and after.  A is an array, X an index 
       value.  Evaluates to true iff X is within range of the Nth
       dimension (1-based) of A.  (A multi-dimensional array
       type is represented as array of array of ...) */
    BINOP_IN_BOUNDS = OP_EXTENDED0,

    /* X IN L .. U.  True iff L <= X <= U.  */
    TERNOP_IN_RANGE,

    /* Ada attributes ('Foo). */
    OP_ATR_FIRST,
    OP_ATR_LAST,
    OP_ATR_LENGTH,
    OP_ATR_IMAGE,
    OP_ATR_MAX,
    OP_ATR_MIN,
    OP_ATR_MODULUS,
    OP_ATR_POS,
    OP_ATR_SIZE,
    OP_ATR_TAG,
    OP_ATR_VAL,

    /* Ada type qualification.  It is encoded as for UNOP_CAST, above, 
       and denotes the TYPE'(EXPR) construct. */
    UNOP_QUAL,

    /* X IN TYPE.  The `TYPE' argument is immediate, with 
       UNOP_IN_RANGE before and after it. True iff X is a member of 
       type TYPE (typically a subrange). */
    UNOP_IN_RANGE,

    /* An aggregate.   A single immediate operand, N>0, gives
       the number of component specifications that follow.  The
       immediate operand is followed by a second OP_AGGREGATE.  
       Next come N component specifications.  A component
       specification is either an OP_OTHERS (others=>...), an
       OP_CHOICES (for named associations), or other expression (for
       positional aggregates only).  Aggregates currently
       occur only as the right sides of assignments. */
    OP_AGGREGATE,

    /* An others clause.  Followed by a single expression. */
    OP_OTHERS,

    /* An aggregate component association.  A single immediate operand, N, 
       gives the number of choices that follow.  This is followed by a second
       OP_CHOICES operator.  Next come N operands, each of which is an
       expression, an OP_DISCRETE_RANGE, or an OP_NAME---the latter 
       for a simple name that must be a record component name and does 
       not correspond to a single existing symbol.  After the N choice 
       indicators comes an expression giving the value.

       In an aggregate such as (X => E1, ...), where X is a simple
       name, X could syntactically be either a component_selector_name 
       or an expression used as a discrete_choice, depending on the
       aggregate's type context.  Since this is not known at parsing
       time, we don't attempt to disambiguate X if it has multiple
       definitions, but instead supply an OP_NAME.  If X has a single
       definition, we represent it with an OP_VAR_VALUE, even though
       it may turn out to be within a record aggregate.  Aggregate 
       evaluation can use either OP_NAMEs or OP_VAR_VALUEs to get a
       record field name, and can evaluate OP_VAR_VALUE normally to
       get its value as an expression.  Unfortunately, we lose out in
       cases where X has multiple meanings and is part of an array
       aggregate.  I hope these are not common enough to annoy users,
       who can work around the problem in any case by putting
       parentheses around X. */
    OP_CHOICES,

    /* A positional aggregate component association.  The operator is 
       followed by a single integer indicating the position in the 
       aggregate (0-based), followed by a second OP_POSITIONAL.  Next 
       follows a single expression giving the component value.  */
    OP_POSITIONAL,

    /* A range of values.  Followed by two expressions giving the
       upper and lower bounds of the range. */
    OP_DISCRETE_RANGE,       

    /* End marker */
    OP_ADA_LAST
  };

/* A triple, (symbol, block, symtab), representing one instance of a 
 * symbol-lookup operation. */
struct ada_symbol_info {
  struct symbol* sym;
  struct block* block;
  struct symtab* symtab;
};

/* Ada task structures.  */

/* Ada task control block, as defined in the GNAT runt-time library.  */

struct task_control_block
{
  char state;
  CORE_ADDR parent;
  int priority;
  char image [32];
  int image_len;    /* This field is not always present in the ATCB.  */
  CORE_ADDR call;
  CORE_ADDR thread;
  CORE_ADDR lwp;    /* This field is not always present in the ATCB.  */

  /* If the task is waiting on a task entry, this field contains the
   task_id of the other task.  */
  CORE_ADDR called_task;
};

struct task_ptid
{
  int pid;                      /* The Process id */
  long lwp;                     /* The Light Weight Process id */
  long tid;                     /* The Thread id */
};
typedef struct task_ptid task_ptid_t;

struct task_entry
{
  CORE_ADDR task_id;
  struct task_control_block atcb;
  int task_num;
  int known_tasks_index;
  struct task_entry *next_task;
  task_ptid_t task_ptid;
  int stack_per;
};

/* task entry list.  */
extern struct task_entry *task_list;


/* Assuming V points to an array of S objects,  make sure that it contains at
   least M objects, updating V and S as necessary. */

#define GROW_VECT(v, s, m)                                              \
   if ((s) < (m)) (v) = grow_vect (v, &(s), m, sizeof *(v));

extern void *grow_vect (void *, size_t *, size_t, int);

extern int ada_get_field_index (const struct type *type,
                                const char *field_name,
                                int maybe_missing);

extern int ada_parse (void);    /* Defined in ada-exp.y */

extern void ada_error (char *); /* Defined in ada-exp.y */

                        /* Defined in ada-typeprint.c */
extern void ada_print_type (struct type *, char *, struct ui_file *, int,
                            int);

extern int ada_val_print (struct type *, const gdb_byte *, int, CORE_ADDR,
                          struct ui_file *, int, int, int,
                          enum val_prettyprint);

extern int ada_value_print (struct value *, struct ui_file *, int,
                            enum val_prettyprint);

                                /* Defined in ada-lang.c */

extern struct value *value_from_contents_and_address (struct type *,
						      const gdb_byte *,
                                                      CORE_ADDR);

extern void ada_emit_char (int, struct ui_file *, int, int);

extern void ada_printchar (int, struct ui_file *);

extern void ada_printstr (struct ui_file *, const gdb_byte *,
			  unsigned int, int, int);

extern void ada_convert_actuals (struct value *, int, struct value **,
                                 CORE_ADDR *);

extern struct value *ada_value_subscript (struct value *, int,
                                          struct value **);

extern struct type *ada_array_element_type (struct type *, int);

extern int ada_array_arity (struct type *);

struct type *ada_type_of_array (struct value *, int);

extern struct value *ada_coerce_to_simple_array_ptr (struct value *);

extern int ada_is_simple_array_type (struct type *);

extern int ada_is_array_descriptor_type (struct type *);

extern int ada_is_bogus_array_descriptor (struct type *);

extern struct type *ada_index_type (struct type *, int);

extern struct value *ada_array_bound (struct value *, int, int);

extern char *ada_decode_symbol (const struct general_symbol_info*);

extern const char *ada_decode (const char*);

extern enum language ada_update_initial_language (enum language, 
						  struct partial_symtab*);

extern void clear_ada_sym_cache (void);

extern char **ada_make_symbol_completion_list (const char *text0,
                                               const char *word);

extern int ada_lookup_symbol_list (const char *, const struct block *,
                                   domain_enum, struct ada_symbol_info**);

extern char *ada_fold_name (const char *);

extern struct symbol *ada_lookup_symbol (const char *, const struct block *,
                                         domain_enum, int *, 
					 struct symtab **);

extern struct minimal_symbol *ada_lookup_simple_minsym (const char *);

extern void ada_fill_in_ada_prototype (struct symbol *);

extern int user_select_syms (struct ada_symbol_info *, int, int);

extern int get_selections (int *, int, int, int, char *);

extern char *ada_start_decode_line_1 (char *);

extern struct symtabs_and_lines ada_finish_decode_line_1 (char **,
                                                          struct symtab *,
                                                          int, char ***);

extern struct symtabs_and_lines ada_sals_for_line (const char*, int,
						   int, char***, int);

extern int ada_scan_number (const char *, int, LONGEST *, int *);

extern struct type *ada_parent_type (struct type *);

extern int ada_is_ignored_field (struct type *, int);

extern int ada_is_packed_array_type (struct type *);

extern struct value *ada_value_primitive_packed_val (struct value *,
						     const gdb_byte *,
                                                     long, int, int,
                                                     struct type *);

extern struct type *ada_coerce_to_simple_array_type (struct type *);

extern int ada_is_character_type (struct type *);

extern int ada_is_string_type (struct type *);

extern int ada_is_tagged_type (struct type *, int);

extern int ada_is_tag_type (struct type *);

extern struct type *ada_tag_type (struct value *);

extern struct value *ada_value_tag (struct value *);

extern const char *ada_tag_name (struct value *);

extern int ada_is_parent_field (struct type *, int);

extern int ada_is_wrapper_field (struct type *, int);

extern int ada_is_variant_part (struct type *, int);

extern struct type *ada_variant_discrim_type (struct type *, struct type *);

extern int ada_is_others_clause (struct type *, int);

extern int ada_in_variant (LONGEST, struct type *, int);

extern char *ada_variant_discrim_name (struct type *);

extern struct value *ada_value_struct_elt (struct value *, char *, int);

extern int ada_is_aligner_type (struct type *);

extern struct type *ada_aligned_type (struct type *);

extern const gdb_byte *ada_aligned_value_addr (struct type *,
					       const gdb_byte *);

extern const char *ada_attribute_name (enum exp_opcode);

extern int ada_is_fixed_point_type (struct type *);

extern int ada_is_system_address_type (struct type *);

extern DOUBLEST ada_delta (struct type *);

extern DOUBLEST ada_fixed_to_float (struct type *, LONGEST);

extern LONGEST ada_float_to_fixed (struct type *, DOUBLEST);

extern int ada_is_vax_floating_type (struct type *);

extern int ada_vax_float_type_suffix (struct type *);

extern struct value *ada_vax_float_print_function (struct type *);

extern struct type *ada_system_address_type (void);

extern int ada_which_variant_applies (struct type *, struct type *,
				      const gdb_byte *);

extern struct type *ada_to_fixed_type (struct type *, const gdb_byte *,
				       CORE_ADDR, struct value *);

extern struct type *ada_template_to_fixed_record_type_1 (struct type *type,
							 const gdb_byte *valaddr,
							 CORE_ADDR address,
							 struct value *dval0,
							 int keep_dynamic_fields);

extern int ada_name_prefix_len (const char *);

extern char *ada_type_name (struct type *);

extern struct type *ada_find_parallel_type (struct type *,
                                            const char *suffix);

extern LONGEST get_int_var_value (char *, int *);

extern struct symbol *ada_find_any_symbol (const char *name);

extern struct type *ada_find_any_type (const char *name);

extern struct symbol *ada_find_renaming_symbol (const char *name,
                                                struct block *block);

extern int ada_prefer_type (struct type *, struct type *);

extern struct type *ada_get_base_type (struct type *);

extern struct type *ada_check_typedef (struct type *);

extern char *ada_encode (const char *);

extern const char *ada_enum_name (const char *);

extern int ada_is_modular_type (struct type *);

extern ULONGEST ada_modulus (struct type *);

extern struct value *ada_value_ind (struct value *);

extern void ada_print_scalar (struct type *, LONGEST, struct ui_file *);

extern int ada_is_range_type_name (const char *);

extern const char *ada_renaming_type (struct type *);

extern int ada_is_object_renaming (struct symbol *);

extern char *ada_simple_renamed_entity (struct symbol *);

extern char *ada_breakpoint_rewrite (char *, int *);

extern char *ada_main_name (void);

/* Tasking-related: ada-tasks.c */

extern int valid_task_id (int);

extern void init_task_list (void);

extern int ada_is_exception_breakpoint (bpstat bs);

extern void ada_adjust_exception_stop (bpstat bs);

extern void ada_print_exception_stop (bpstat bs);

extern int ada_get_current_task (ptid_t);

extern int breakpoint_ada_task_match (CORE_ADDR, ptid_t);

extern int ada_print_exception_breakpoint_nontask (struct breakpoint *);

extern void ada_print_exception_breakpoint_task (struct breakpoint *);

extern void ada_reset_thread_registers (void);

extern int ada_build_task_list (void);

extern int ada_exception_catchpoint_p (struct breakpoint *b);
  
extern struct symtab_and_line
  ada_decode_exception_location (char *args, char **addr_string,
                                 char **exp_string, char **cond_string,
                                 struct expression **cond,
                                 struct breakpoint_ops **ops);

extern struct symtab_and_line
  ada_decode_assert_location (char *args, char **addr_string,
                              struct breakpoint_ops **ops);


#endif
