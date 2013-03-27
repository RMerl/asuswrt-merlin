/* Ada language support routines for GDB, the GNU debugger.  Copyright (C)

   1992, 1993, 1994, 1997, 1998, 1999, 2000, 2003, 2004, 2005, 2007
   Free Software Foundation, Inc.

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


#include "defs.h"
#include <stdio.h>
#include "gdb_string.h"
#include <ctype.h>
#include <stdarg.h>
#include "demangle.h"
#include "gdb_regex.h"
#include "frame.h"
#include "symtab.h"
#include "gdbtypes.h"
#include "gdbcmd.h"
#include "expression.h"
#include "parser-defs.h"
#include "language.h"
#include "c-lang.h"
#include "inferior.h"
#include "symfile.h"
#include "objfiles.h"
#include "breakpoint.h"
#include "gdbcore.h"
#include "hashtab.h"
#include "gdb_obstack.h"
#include "ada-lang.h"
#include "completer.h"
#include "gdb_stat.h"
#ifdef UI_OUT
#include "ui-out.h"
#endif
#include "block.h"
#include "infcall.h"
#include "dictionary.h"
#include "exceptions.h"
#include "annotate.h"
#include "valprint.h"
#include "source.h"
#include "observer.h"

#ifndef ADA_RETAIN_DOTS
#define ADA_RETAIN_DOTS 0
#endif

/* Define whether or not the C operator '/' truncates towards zero for
   differently signed operands (truncation direction is undefined in C). 
   Copied from valarith.c.  */

#ifndef TRUNCATION_TOWARDS_ZERO
#define TRUNCATION_TOWARDS_ZERO ((-5 / 2) == -2)
#endif


static void extract_string (CORE_ADDR addr, char *buf);

static struct type *ada_create_fundamental_type (struct objfile *, int);

static void modify_general_field (char *, LONGEST, int, int);

static struct type *desc_base_type (struct type *);

static struct type *desc_bounds_type (struct type *);

static struct value *desc_bounds (struct value *);

static int fat_pntr_bounds_bitpos (struct type *);

static int fat_pntr_bounds_bitsize (struct type *);

static struct type *desc_data_type (struct type *);

static struct value *desc_data (struct value *);

static int fat_pntr_data_bitpos (struct type *);

static int fat_pntr_data_bitsize (struct type *);

static struct value *desc_one_bound (struct value *, int, int);

static int desc_bound_bitpos (struct type *, int, int);

static int desc_bound_bitsize (struct type *, int, int);

static struct type *desc_index_type (struct type *, int);

static int desc_arity (struct type *);

static int ada_type_match (struct type *, struct type *, int);

static int ada_args_match (struct symbol *, struct value **, int);

static struct value *ensure_lval (struct value *, CORE_ADDR *);

static struct value *convert_actual (struct value *, struct type *,
                                     CORE_ADDR *);

static struct value *make_array_descriptor (struct type *, struct value *,
                                            CORE_ADDR *);

static void ada_add_block_symbols (struct obstack *,
                                   struct block *, const char *,
                                   domain_enum, struct objfile *,
                                   struct symtab *, int);

static int is_nonfunction (struct ada_symbol_info *, int);

static void add_defn_to_vec (struct obstack *, struct symbol *,
                             struct block *, struct symtab *);

static int num_defns_collected (struct obstack *);

static struct ada_symbol_info *defns_collected (struct obstack *, int);

static struct partial_symbol *ada_lookup_partial_symbol (struct partial_symtab
                                                         *, const char *, int,
                                                         domain_enum, int);

static struct symtab *symtab_for_sym (struct symbol *);

static struct value *resolve_subexp (struct expression **, int *, int,
                                     struct type *);

static void replace_operator_with_call (struct expression **, int, int, int,
                                        struct symbol *, struct block *);

static int possible_user_operator_p (enum exp_opcode, struct value **);

static char *ada_op_name (enum exp_opcode);

static const char *ada_decoded_op_name (enum exp_opcode);

static int numeric_type_p (struct type *);

static int integer_type_p (struct type *);

static int scalar_type_p (struct type *);

static int discrete_type_p (struct type *);

static struct type *ada_lookup_struct_elt_type (struct type *, char *,
                                                int, int, int *);

static struct value *evaluate_subexp (struct type *, struct expression *,
                                      int *, enum noside);

static struct value *evaluate_subexp_type (struct expression *, int *);

static int is_dynamic_field (struct type *, int);

static struct type *to_fixed_variant_branch_type (struct type *,
						  const gdb_byte *,
                                                  CORE_ADDR, struct value *);

static struct type *to_fixed_array_type (struct type *, struct value *, int);

static struct type *to_fixed_range_type (char *, struct value *,
                                         struct objfile *);

static struct type *to_static_fixed_type (struct type *);

static struct value *unwrap_value (struct value *);

static struct type *packed_array_type (struct type *, long *);

static struct type *decode_packed_array_type (struct type *);

static struct value *decode_packed_array (struct value *);

static struct value *value_subscript_packed (struct value *, int,
                                             struct value **);

static void move_bits (gdb_byte *, int, const gdb_byte *, int, int);

static struct value *coerce_unspec_val_to_type (struct value *,
                                                struct type *);

static struct value *get_var_value (char *, char *);

static int lesseq_defined_than (struct symbol *, struct symbol *);

static int equiv_types (struct type *, struct type *);

static int is_name_suffix (const char *);

static int wild_match (const char *, int, const char *);

static struct value *ada_coerce_ref (struct value *);

static LONGEST pos_atr (struct value *);

static struct value *value_pos_atr (struct value *);

static struct value *value_val_atr (struct type *, struct value *);

static struct symbol *standard_lookup (const char *, const struct block *,
                                       domain_enum);

static struct value *ada_search_struct_field (char *, struct value *, int,
                                              struct type *);

static struct value *ada_value_primitive_field (struct value *, int, int,
                                                struct type *);

static int find_struct_field (char *, struct type *, int,
                              struct type **, int *, int *, int *, int *);

static struct value *ada_to_fixed_value_create (struct type *, CORE_ADDR,
                                                struct value *);

static struct value *ada_to_fixed_value (struct value *);

static int ada_resolve_function (struct ada_symbol_info *, int,
                                 struct value **, int, const char *,
                                 struct type *);

static struct value *ada_coerce_to_simple_array (struct value *);

static int ada_is_direct_array_type (struct type *);

static void ada_language_arch_info (struct gdbarch *,
				    struct language_arch_info *);

static void check_size (const struct type *);

static struct value *ada_index_struct_field (int, struct value *, int,
					     struct type *);

static struct value *assign_aggregate (struct value *, struct value *, 
				       struct expression *, int *, enum noside);

static void aggregate_assign_from_choices (struct value *, struct value *, 
					   struct expression *,
					   int *, LONGEST *, int *,
					   int, LONGEST, LONGEST);

static void aggregate_assign_positional (struct value *, struct value *,
					 struct expression *,
					 int *, LONGEST *, int *, int,
					 LONGEST, LONGEST);


static void aggregate_assign_others (struct value *, struct value *,
				     struct expression *,
				     int *, LONGEST *, int, LONGEST, LONGEST);


static void add_component_interval (LONGEST, LONGEST, LONGEST *, int *, int);


static struct value *ada_evaluate_subexp (struct type *, struct expression *,
					  int *, enum noside);

static void ada_forward_operator_length (struct expression *, int, int *,
					 int *);



/* Maximum-sized dynamic type.  */
static unsigned int varsize_limit;

/* FIXME: brobecker/2003-09-17: No longer a const because it is
   returned by a function that does not return a const char *.  */
static char *ada_completer_word_break_characters =
#ifdef VMS
  " \t\n!@#%^&*()+=|~`}{[]\";:?/,-";
#else
  " \t\n!@#$%^&*()+=|~`}{[]\";:?/,-";
#endif

/* The name of the symbol to use to get the name of the main subprogram.  */
static const char ADA_MAIN_PROGRAM_SYMBOL_NAME[]
  = "__gnat_ada_main_program_name";

/* Limit on the number of warnings to raise per expression evaluation.  */
static int warning_limit = 2;

/* Number of warning messages issued; reset to 0 by cleanups after
   expression evaluation.  */
static int warnings_issued = 0;

static const char *known_runtime_file_name_patterns[] = {
  ADA_KNOWN_RUNTIME_FILE_NAME_PATTERNS NULL
};

static const char *known_auxiliary_function_name_patterns[] = {
  ADA_KNOWN_AUXILIARY_FUNCTION_NAME_PATTERNS NULL
};

/* Space for allocating results of ada_lookup_symbol_list.  */
static struct obstack symbol_list_obstack;

                        /* Utilities */


static char *
ada_get_gdb_completer_word_break_characters (void)
{
  return ada_completer_word_break_characters;
}

/* Print an array element index using the Ada syntax.  */

static void
ada_print_array_index (struct value *index_value, struct ui_file *stream,
                       int format, enum val_prettyprint pretty)
{
  LA_VALUE_PRINT (index_value, stream, format, pretty);
  fprintf_filtered (stream, " => ");
}

/* Read the string located at ADDR from the inferior and store the
   result into BUF.  */

static void
extract_string (CORE_ADDR addr, char *buf)
{
  int char_index = 0;

  /* Loop, reading one byte at a time, until we reach the '\000'
     end-of-string marker.  */
  do
    {
      target_read_memory (addr + char_index * sizeof (char),
                          buf + char_index * sizeof (char), sizeof (char));
      char_index++;
    }
  while (buf[char_index - 1] != '\000');
}

/* Assuming VECT points to an array of *SIZE objects of size
   ELEMENT_SIZE, grow it to contain at least MIN_SIZE objects,
   updating *SIZE as necessary and returning the (new) array.  */

void *
grow_vect (void *vect, size_t *size, size_t min_size, int element_size)
{
  if (*size < min_size)
    {
      *size *= 2;
      if (*size < min_size)
        *size = min_size;
      vect = xrealloc (vect, *size * element_size);
    }
  return vect;
}

/* True (non-zero) iff TARGET matches FIELD_NAME up to any trailing
   suffix of FIELD_NAME beginning "___".  */

static int
field_name_match (const char *field_name, const char *target)
{
  int len = strlen (target);
  return
    (strncmp (field_name, target, len) == 0
     && (field_name[len] == '\0'
         || (strncmp (field_name + len, "___", 3) == 0
             && strcmp (field_name + strlen (field_name) - 6,
                        "___XVN") != 0)));
}


/* Assuming TYPE is a TYPE_CODE_STRUCT, find the field whose name matches
   FIELD_NAME, and return its index.  This function also handles fields
   whose name have ___ suffixes because the compiler sometimes alters
   their name by adding such a suffix to represent fields with certain
   constraints.  If the field could not be found, return a negative
   number if MAYBE_MISSING is set.  Otherwise raise an error.  */

int
ada_get_field_index (const struct type *type, const char *field_name,
                     int maybe_missing)
{
  int fieldno;
  for (fieldno = 0; fieldno < TYPE_NFIELDS (type); fieldno++)
    if (field_name_match (TYPE_FIELD_NAME (type, fieldno), field_name))
      return fieldno;

  if (!maybe_missing)
    error (_("Unable to find field %s in struct %s.  Aborting"),
           field_name, TYPE_NAME (type));

  return -1;
}

/* The length of the prefix of NAME prior to any "___" suffix.  */

int
ada_name_prefix_len (const char *name)
{
  if (name == NULL)
    return 0;
  else
    {
      const char *p = strstr (name, "___");
      if (p == NULL)
        return strlen (name);
      else
        return p - name;
    }
}

/* Return non-zero if SUFFIX is a suffix of STR.
   Return zero if STR is null.  */

static int
is_suffix (const char *str, const char *suffix)
{
  int len1, len2;
  if (str == NULL)
    return 0;
  len1 = strlen (str);
  len2 = strlen (suffix);
  return (len1 >= len2 && strcmp (str + len1 - len2, suffix) == 0);
}

/* Create a value of type TYPE whose contents come from VALADDR, if it
   is non-null, and whose memory address (in the inferior) is
   ADDRESS.  */

struct value *
value_from_contents_and_address (struct type *type,
				 const gdb_byte *valaddr,
                                 CORE_ADDR address)
{
  struct value *v = allocate_value (type);
  if (valaddr == NULL)
    set_value_lazy (v, 1);
  else
    memcpy (value_contents_raw (v), valaddr, TYPE_LENGTH (type));
  VALUE_ADDRESS (v) = address;
  if (address != 0)
    VALUE_LVAL (v) = lval_memory;
  return v;
}

/* The contents of value VAL, treated as a value of type TYPE.  The
   result is an lval in memory if VAL is.  */

static struct value *
coerce_unspec_val_to_type (struct value *val, struct type *type)
{
  type = ada_check_typedef (type);
  if (value_type (val) == type)
    return val;
  else
    {
      struct value *result;

      /* Make sure that the object size is not unreasonable before
         trying to allocate some memory for it.  */
      check_size (type);

      result = allocate_value (type);
      VALUE_LVAL (result) = VALUE_LVAL (val);
      set_value_bitsize (result, value_bitsize (val));
      set_value_bitpos (result, value_bitpos (val));
      VALUE_ADDRESS (result) = VALUE_ADDRESS (val) + value_offset (val);
      if (value_lazy (val)
          || TYPE_LENGTH (type) > TYPE_LENGTH (value_type (val)))
        set_value_lazy (result, 1);
      else
        memcpy (value_contents_raw (result), value_contents (val),
                TYPE_LENGTH (type));
      return result;
    }
}

static const gdb_byte *
cond_offset_host (const gdb_byte *valaddr, long offset)
{
  if (valaddr == NULL)
    return NULL;
  else
    return valaddr + offset;
}

static CORE_ADDR
cond_offset_target (CORE_ADDR address, long offset)
{
  if (address == 0)
    return 0;
  else
    return address + offset;
}

/* Issue a warning (as for the definition of warning in utils.c, but
   with exactly one argument rather than ...), unless the limit on the
   number of warnings has passed during the evaluation of the current
   expression.  */

/* FIXME: cagney/2004-10-10: This function is mimicking the behavior
   provided by "complaint".  */
static void lim_warning (const char *format, ...) ATTR_FORMAT (printf, 1, 2);

static void
lim_warning (const char *format, ...)
{
  va_list args;
  va_start (args, format);

  warnings_issued += 1;
  if (warnings_issued <= warning_limit)
    vwarning (format, args);

  va_end (args);
}

/* Issue an error if the size of an object of type T is unreasonable,
   i.e. if it would be a bad idea to allocate a value of this type in
   GDB.  */

static void
check_size (const struct type *type)
{
  if (TYPE_LENGTH (type) > varsize_limit)
    error (_("object size is larger than varsize-limit"));
}


/* Note: would have used MAX_OF_TYPE and MIN_OF_TYPE macros from
   gdbtypes.h, but some of the necessary definitions in that file
   seem to have gone missing. */

/* Maximum value of a SIZE-byte signed integer type. */
static LONGEST
max_of_size (int size)
{
  LONGEST top_bit = (LONGEST) 1 << (size * 8 - 2);
  return top_bit | (top_bit - 1);
}

/* Minimum value of a SIZE-byte signed integer type. */
static LONGEST
min_of_size (int size)
{
  return -max_of_size (size) - 1;
}

/* Maximum value of a SIZE-byte unsigned integer type. */
static ULONGEST
umax_of_size (int size)
{
  ULONGEST top_bit = (ULONGEST) 1 << (size * 8 - 1);
  return top_bit | (top_bit - 1);
}

/* Maximum value of integral type T, as a signed quantity. */
static LONGEST
max_of_type (struct type *t)
{
  if (TYPE_UNSIGNED (t))
    return (LONGEST) umax_of_size (TYPE_LENGTH (t));
  else
    return max_of_size (TYPE_LENGTH (t));
}

/* Minimum value of integral type T, as a signed quantity. */
static LONGEST
min_of_type (struct type *t)
{
  if (TYPE_UNSIGNED (t)) 
    return 0;
  else
    return min_of_size (TYPE_LENGTH (t));
}

/* The largest value in the domain of TYPE, a discrete type, as an integer.  */
static struct value *
discrete_type_high_bound (struct type *type)
{
  switch (TYPE_CODE (type))
    {
    case TYPE_CODE_RANGE:
      return value_from_longest (TYPE_TARGET_TYPE (type),
                                 TYPE_HIGH_BOUND (type));
    case TYPE_CODE_ENUM:
      return
        value_from_longest (type,
                            TYPE_FIELD_BITPOS (type,
                                               TYPE_NFIELDS (type) - 1));
    case TYPE_CODE_INT:
      return value_from_longest (type, max_of_type (type));
    default:
      error (_("Unexpected type in discrete_type_high_bound."));
    }
}

/* The largest value in the domain of TYPE, a discrete type, as an integer.  */
static struct value *
discrete_type_low_bound (struct type *type)
{
  switch (TYPE_CODE (type))
    {
    case TYPE_CODE_RANGE:
      return value_from_longest (TYPE_TARGET_TYPE (type),
                                 TYPE_LOW_BOUND (type));
    case TYPE_CODE_ENUM:
      return value_from_longest (type, TYPE_FIELD_BITPOS (type, 0));
    case TYPE_CODE_INT:
      return value_from_longest (type, min_of_type (type));
    default:
      error (_("Unexpected type in discrete_type_low_bound."));
    }
}

/* The identity on non-range types.  For range types, the underlying
   non-range scalar type.  */

static struct type *
base_type (struct type *type)
{
  while (type != NULL && TYPE_CODE (type) == TYPE_CODE_RANGE)
    {
      if (type == TYPE_TARGET_TYPE (type) || TYPE_TARGET_TYPE (type) == NULL)
        return type;
      type = TYPE_TARGET_TYPE (type);
    }
  return type;
}


                                /* Language Selection */

/* If the main program is in Ada, return language_ada, otherwise return LANG
   (the main program is in Ada iif the adainit symbol is found).

   MAIN_PST is not used.  */

enum language
ada_update_initial_language (enum language lang,
                             struct partial_symtab *main_pst)
{
  if (lookup_minimal_symbol ("adainit", (const char *) NULL,
                             (struct objfile *) NULL) != NULL)
    return language_ada;

  return lang;
}

/* If the main procedure is written in Ada, then return its name.
   The result is good until the next call.  Return NULL if the main
   procedure doesn't appear to be in Ada.  */

char *
ada_main_name (void)
{
  struct minimal_symbol *msym;
  CORE_ADDR main_program_name_addr;
  static char main_program_name[1024];

  /* For Ada, the name of the main procedure is stored in a specific
     string constant, generated by the binder.  Look for that symbol,
     extract its address, and then read that string.  If we didn't find
     that string, then most probably the main procedure is not written
     in Ada.  */
  msym = lookup_minimal_symbol (ADA_MAIN_PROGRAM_SYMBOL_NAME, NULL, NULL);

  if (msym != NULL)
    {
      main_program_name_addr = SYMBOL_VALUE_ADDRESS (msym);
      if (main_program_name_addr == 0)
        error (_("Invalid address for Ada main program name."));

      extract_string (main_program_name_addr, main_program_name);
      return main_program_name;
    }

  /* The main procedure doesn't seem to be in Ada.  */
  return NULL;
}

                                /* Symbols */

/* Table of Ada operators and their GNAT-encoded names.  Last entry is pair
   of NULLs.  */

const struct ada_opname_map ada_opname_table[] = {
  {"Oadd", "\"+\"", BINOP_ADD},
  {"Osubtract", "\"-\"", BINOP_SUB},
  {"Omultiply", "\"*\"", BINOP_MUL},
  {"Odivide", "\"/\"", BINOP_DIV},
  {"Omod", "\"mod\"", BINOP_MOD},
  {"Orem", "\"rem\"", BINOP_REM},
  {"Oexpon", "\"**\"", BINOP_EXP},
  {"Olt", "\"<\"", BINOP_LESS},
  {"Ole", "\"<=\"", BINOP_LEQ},
  {"Ogt", "\">\"", BINOP_GTR},
  {"Oge", "\">=\"", BINOP_GEQ},
  {"Oeq", "\"=\"", BINOP_EQUAL},
  {"One", "\"/=\"", BINOP_NOTEQUAL},
  {"Oand", "\"and\"", BINOP_BITWISE_AND},
  {"Oor", "\"or\"", BINOP_BITWISE_IOR},
  {"Oxor", "\"xor\"", BINOP_BITWISE_XOR},
  {"Oconcat", "\"&\"", BINOP_CONCAT},
  {"Oabs", "\"abs\"", UNOP_ABS},
  {"Onot", "\"not\"", UNOP_LOGICAL_NOT},
  {"Oadd", "\"+\"", UNOP_PLUS},
  {"Osubtract", "\"-\"", UNOP_NEG},
  {NULL, NULL}
};

/* Return non-zero if STR should be suppressed in info listings.  */

static int
is_suppressed_name (const char *str)
{
  if (strncmp (str, "_ada_", 5) == 0)
    str += 5;
  if (str[0] == '_' || str[0] == '\000')
    return 1;
  else
    {
      const char *p;
      const char *suffix = strstr (str, "___");
      if (suffix != NULL && suffix[3] != 'X')
        return 1;
      if (suffix == NULL)
        suffix = str + strlen (str);
      for (p = suffix - 1; p != str; p -= 1)
        if (isupper (*p))
          {
            int i;
            if (p[0] == 'X' && p[-1] != '_')
              goto OK;
            if (*p != 'O')
              return 1;
            for (i = 0; ada_opname_table[i].encoded != NULL; i += 1)
              if (strncmp (ada_opname_table[i].encoded, p,
                           strlen (ada_opname_table[i].encoded)) == 0)
                goto OK;
            return 1;
          OK:;
          }
      return 0;
    }
}

/* The "encoded" form of DECODED, according to GNAT conventions.
   The result is valid until the next call to ada_encode.  */

char *
ada_encode (const char *decoded)
{
  static char *encoding_buffer = NULL;
  static size_t encoding_buffer_size = 0;
  const char *p;
  int k;

  if (decoded == NULL)
    return NULL;

  GROW_VECT (encoding_buffer, encoding_buffer_size,
             2 * strlen (decoded) + 10);

  k = 0;
  for (p = decoded; *p != '\0'; p += 1)
    {
      if (!ADA_RETAIN_DOTS && *p == '.')
        {
          encoding_buffer[k] = encoding_buffer[k + 1] = '_';
          k += 2;
        }
      else if (*p == '"')
        {
          const struct ada_opname_map *mapping;

          for (mapping = ada_opname_table;
               mapping->encoded != NULL
               && strncmp (mapping->decoded, p,
                           strlen (mapping->decoded)) != 0; mapping += 1)
            ;
          if (mapping->encoded == NULL)
            error (_("invalid Ada operator name: %s"), p);
          strcpy (encoding_buffer + k, mapping->encoded);
          k += strlen (mapping->encoded);
          break;
        }
      else
        {
          encoding_buffer[k] = *p;
          k += 1;
        }
    }

  encoding_buffer[k] = '\0';
  return encoding_buffer;
}

/* Return NAME folded to lower case, or, if surrounded by single
   quotes, unfolded, but with the quotes stripped away.  Result good
   to next call.  */

char *
ada_fold_name (const char *name)
{
  static char *fold_buffer = NULL;
  static size_t fold_buffer_size = 0;

  int len = strlen (name);
  GROW_VECT (fold_buffer, fold_buffer_size, len + 1);

  if (name[0] == '\'')
    {
      strncpy (fold_buffer, name + 1, len - 2);
      fold_buffer[len - 2] = '\000';
    }
  else
    {
      int i;
      for (i = 0; i <= len; i += 1)
        fold_buffer[i] = tolower (name[i]);
    }

  return fold_buffer;
}

/* Return nonzero if C is either a digit or a lowercase alphabet character.  */

static int
is_lower_alphanum (const char c)
{
  return (isdigit (c) || (isalpha (c) && islower (c)));
}

/* Decode:
      . Discard trailing .{DIGIT}+, ${DIGIT}+ or ___{DIGIT}+
        These are suffixes introduced by GNAT5 to nested subprogram
        names, and do not serve any purpose for the debugger.
      . Discard final __{DIGIT}+ or $({DIGIT}+(__{DIGIT}+)*)
      . Discard final N if it follows a lowercase alphanumeric character
        (protected object subprogram suffix)
      . Convert other instances of embedded "__" to `.'.
      . Discard leading _ada_.
      . Convert operator names to the appropriate quoted symbols.
      . Remove everything after first ___ if it is followed by
        'X'.
      . Replace TK__ with __, and a trailing B or TKB with nothing.
      . Replace _[EB]{DIGIT}+[sb] with nothing (protected object entries)
      . Put symbols that should be suppressed in <...> brackets.
      . Remove trailing X[bn]* suffix (indicating names in package bodies).

   The resulting string is valid until the next call of ada_decode.
   If the string is unchanged by demangling, the original string pointer
   is returned.  */

const char *
ada_decode (const char *encoded)
{
  int i, j;
  int len0;
  const char *p;
  char *decoded;
  int at_start_name;
  static char *decoding_buffer = NULL;
  static size_t decoding_buffer_size = 0;

  if (strncmp (encoded, "_ada_", 5) == 0)
    encoded += 5;

  if (encoded[0] == '_' || encoded[0] == '<')
    goto Suppress;

  /* Remove trailing .{DIGIT}+ or ___{DIGIT}+ or __{DIGIT}+.  */
  len0 = strlen (encoded);
  if (len0 > 1 && isdigit (encoded[len0 - 1]))
    {
      i = len0 - 2;
      while (i > 0 && isdigit (encoded[i]))
        i--;
      if (i >= 0 && encoded[i] == '.')
        len0 = i;
      else if (i >= 0 && encoded[i] == '$')
        len0 = i;
      else if (i >= 2 && strncmp (encoded + i - 2, "___", 3) == 0)
        len0 = i - 2;
      else if (i >= 1 && strncmp (encoded + i - 1, "__", 2) == 0)
        len0 = i - 1;
    }

  /* Remove trailing N.  */

  /* Protected entry subprograms are broken into two
     separate subprograms: The first one is unprotected, and has
     a 'N' suffix; the second is the protected version, and has
     the 'P' suffix. The second calls the first one after handling
     the protection.  Since the P subprograms are internally generated,
     we leave these names undecoded, giving the user a clue that this
     entity is internal.  */

  if (len0 > 1
      && encoded[len0 - 1] == 'N'
      && (isdigit (encoded[len0 - 2]) || islower (encoded[len0 - 2])))
    len0--;

  /* Remove the ___X.* suffix if present.  Do not forget to verify that
     the suffix is located before the current "end" of ENCODED.  We want
     to avoid re-matching parts of ENCODED that have previously been
     marked as discarded (by decrementing LEN0).  */
  p = strstr (encoded, "___");
  if (p != NULL && p - encoded < len0 - 3)
    {
      if (p[3] == 'X')
        len0 = p - encoded;
      else
        goto Suppress;
    }

  if (len0 > 3 && strncmp (encoded + len0 - 3, "TKB", 3) == 0)
    len0 -= 3;

  if (len0 > 1 && strncmp (encoded + len0 - 1, "B", 1) == 0)
    len0 -= 1;

  /* Make decoded big enough for possible expansion by operator name.  */
  GROW_VECT (decoding_buffer, decoding_buffer_size, 2 * len0 + 1);
  decoded = decoding_buffer;

  if (len0 > 1 && isdigit (encoded[len0 - 1]))
    {
      i = len0 - 2;
      while ((i >= 0 && isdigit (encoded[i]))
             || (i >= 1 && encoded[i] == '_' && isdigit (encoded[i - 1])))
        i -= 1;
      if (i > 1 && encoded[i] == '_' && encoded[i - 1] == '_')
        len0 = i - 1;
      else if (encoded[i] == '$')
        len0 = i;
    }

  for (i = 0, j = 0; i < len0 && !isalpha (encoded[i]); i += 1, j += 1)
    decoded[j] = encoded[i];

  at_start_name = 1;
  while (i < len0)
    {
      if (at_start_name && encoded[i] == 'O')
        {
          int k;
          for (k = 0; ada_opname_table[k].encoded != NULL; k += 1)
            {
              int op_len = strlen (ada_opname_table[k].encoded);
              if ((strncmp (ada_opname_table[k].encoded + 1, encoded + i + 1,
                            op_len - 1) == 0)
                  && !isalnum (encoded[i + op_len]))
                {
                  strcpy (decoded + j, ada_opname_table[k].decoded);
                  at_start_name = 0;
                  i += op_len;
                  j += strlen (ada_opname_table[k].decoded);
                  break;
                }
            }
          if (ada_opname_table[k].encoded != NULL)
            continue;
        }
      at_start_name = 0;

      /* Replace "TK__" with "__", which will eventually be translated
         into "." (just below).  */

      if (i < len0 - 4 && strncmp (encoded + i, "TK__", 4) == 0)
        i += 2;

      /* Remove _E{DIGITS}+[sb] */

      /* Just as for protected object subprograms, there are 2 categories
         of subprograms created by the compiler for each entry. The first
         one implements the actual entry code, and has a suffix following
         the convention above; the second one implements the barrier and
         uses the same convention as above, except that the 'E' is replaced
         by a 'B'.

         Just as above, we do not decode the name of barrier functions
         to give the user a clue that the code he is debugging has been
         internally generated.  */

      if (len0 - i > 3 && encoded [i] == '_' && encoded[i+1] == 'E'
          && isdigit (encoded[i+2]))
        {
          int k = i + 3;

          while (k < len0 && isdigit (encoded[k]))
            k++;

          if (k < len0
              && (encoded[k] == 'b' || encoded[k] == 's'))
            {
              k++;
              /* Just as an extra precaution, make sure that if this
                 suffix is followed by anything else, it is a '_'.
                 Otherwise, we matched this sequence by accident.  */
              if (k == len0
                  || (k < len0 && encoded[k] == '_'))
                i = k;
            }
        }

      /* Remove trailing "N" in [a-z0-9]+N__.  The N is added by
         the GNAT front-end in protected object subprograms.  */

      if (i < len0 + 3
          && encoded[i] == 'N' && encoded[i+1] == '_' && encoded[i+2] == '_')
        {
          /* Backtrack a bit up until we reach either the begining of
             the encoded name, or "__".  Make sure that we only find
             digits or lowercase characters.  */
          const char *ptr = encoded + i - 1;

          while (ptr >= encoded && is_lower_alphanum (ptr[0]))
            ptr--;
          if (ptr < encoded
              || (ptr > encoded && ptr[0] == '_' && ptr[-1] == '_'))
            i++;
        }

      if (encoded[i] == 'X' && i != 0 && isalnum (encoded[i - 1]))
        {
          do
            i += 1;
          while (i < len0 && (encoded[i] == 'b' || encoded[i] == 'n'));
          if (i < len0)
            goto Suppress;
        }
      else if (!ADA_RETAIN_DOTS
               && i < len0 - 2 && encoded[i] == '_' && encoded[i + 1] == '_')
        {
          decoded[j] = '.';
          at_start_name = 1;
          i += 2;
          j += 1;
        }
      else
        {
          decoded[j] = encoded[i];
          i += 1;
          j += 1;
        }
    }
  decoded[j] = '\000';

  for (i = 0; decoded[i] != '\0'; i += 1)
    if (isupper (decoded[i]) || decoded[i] == ' ')
      goto Suppress;

  if (strcmp (decoded, encoded) == 0)
    return encoded;
  else
    return decoded;

Suppress:
  GROW_VECT (decoding_buffer, decoding_buffer_size, strlen (encoded) + 3);
  decoded = decoding_buffer;
  if (encoded[0] == '<')
    strcpy (decoded, encoded);
  else
    sprintf (decoded, "<%s>", encoded);
  return decoded;

}

/* Table for keeping permanent unique copies of decoded names.  Once
   allocated, names in this table are never released.  While this is a
   storage leak, it should not be significant unless there are massive
   changes in the set of decoded names in successive versions of a 
   symbol table loaded during a single session.  */
static struct htab *decoded_names_store;

/* Returns the decoded name of GSYMBOL, as for ada_decode, caching it
   in the language-specific part of GSYMBOL, if it has not been
   previously computed.  Tries to save the decoded name in the same
   obstack as GSYMBOL, if possible, and otherwise on the heap (so that,
   in any case, the decoded symbol has a lifetime at least that of
   GSYMBOL).  
   The GSYMBOL parameter is "mutable" in the C++ sense: logically
   const, but nevertheless modified to a semantically equivalent form
   when a decoded name is cached in it.
*/

char *
ada_decode_symbol (const struct general_symbol_info *gsymbol)
{
  char **resultp =
    (char **) &gsymbol->language_specific.cplus_specific.demangled_name;
  if (*resultp == NULL)
    {
      const char *decoded = ada_decode (gsymbol->name);
      if (gsymbol->bfd_section != NULL)
        {
          bfd *obfd = gsymbol->bfd_section->owner;
          if (obfd != NULL)
            {
              struct objfile *objf;
              ALL_OBJFILES (objf)
              {
                if (obfd == objf->obfd)
                  {
                    *resultp = obsavestring (decoded, strlen (decoded),
                                             &objf->objfile_obstack);
                    break;
                  }
              }
            }
        }
      /* Sometimes, we can't find a corresponding objfile, in which
         case, we put the result on the heap.  Since we only decode
         when needed, we hope this usually does not cause a
         significant memory leak (FIXME).  */
      if (*resultp == NULL)
        {
          char **slot = (char **) htab_find_slot (decoded_names_store,
                                                  decoded, INSERT);
          if (*slot == NULL)
            *slot = xstrdup (decoded);
          *resultp = *slot;
        }
    }

  return *resultp;
}

char *
ada_la_decode (const char *encoded, int options)
{
  return xstrdup (ada_decode (encoded));
}

/* Returns non-zero iff SYM_NAME matches NAME, ignoring any trailing
   suffixes that encode debugging information or leading _ada_ on
   SYM_NAME (see is_name_suffix commentary for the debugging
   information that is ignored).  If WILD, then NAME need only match a
   suffix of SYM_NAME minus the same suffixes.  Also returns 0 if
   either argument is NULL.  */

int
ada_match_name (const char *sym_name, const char *name, int wild)
{
  if (sym_name == NULL || name == NULL)
    return 0;
  else if (wild)
    return wild_match (name, strlen (name), sym_name);
  else
    {
      int len_name = strlen (name);
      return (strncmp (sym_name, name, len_name) == 0
              && is_name_suffix (sym_name + len_name))
        || (strncmp (sym_name, "_ada_", 5) == 0
            && strncmp (sym_name + 5, name, len_name) == 0
            && is_name_suffix (sym_name + len_name + 5));
    }
}

/* True (non-zero) iff, in Ada mode, the symbol SYM should be
   suppressed in info listings.  */

int
ada_suppress_symbol_printing (struct symbol *sym)
{
  if (SYMBOL_DOMAIN (sym) == STRUCT_DOMAIN)
    return 1;
  else
    return is_suppressed_name (SYMBOL_LINKAGE_NAME (sym));
}


                                /* Arrays */

/* Names of MAX_ADA_DIMENS bounds in P_BOUNDS fields of array descriptors.  */

static char *bound_name[] = {
  "LB0", "UB0", "LB1", "UB1", "LB2", "UB2", "LB3", "UB3",
  "LB4", "UB4", "LB5", "UB5", "LB6", "UB6", "LB7", "UB7"
};

/* Maximum number of array dimensions we are prepared to handle.  */

#define MAX_ADA_DIMENS (sizeof(bound_name) / (2*sizeof(char *)))

/* Like modify_field, but allows bitpos > wordlength.  */

static void
modify_general_field (char *addr, LONGEST fieldval, int bitpos, int bitsize)
{
  modify_field (addr + bitpos / 8, fieldval, bitpos % 8, bitsize);
}


/* The desc_* routines return primitive portions of array descriptors
   (fat pointers).  */

/* The descriptor or array type, if any, indicated by TYPE; removes
   level of indirection, if needed.  */

static struct type *
desc_base_type (struct type *type)
{
  if (type == NULL)
    return NULL;
  type = ada_check_typedef (type);
  if (type != NULL
      && (TYPE_CODE (type) == TYPE_CODE_PTR
          || TYPE_CODE (type) == TYPE_CODE_REF))
    return ada_check_typedef (TYPE_TARGET_TYPE (type));
  else
    return type;
}

/* True iff TYPE indicates a "thin" array pointer type.  */

static int
is_thin_pntr (struct type *type)
{
  return
    is_suffix (ada_type_name (desc_base_type (type)), "___XUT")
    || is_suffix (ada_type_name (desc_base_type (type)), "___XUT___XVE");
}

/* The descriptor type for thin pointer type TYPE.  */

static struct type *
thin_descriptor_type (struct type *type)
{
  struct type *base_type = desc_base_type (type);
  if (base_type == NULL)
    return NULL;
  if (is_suffix (ada_type_name (base_type), "___XVE"))
    return base_type;
  else
    {
      struct type *alt_type = ada_find_parallel_type (base_type, "___XVE");
      if (alt_type == NULL)
        return base_type;
      else
        return alt_type;
    }
}

/* A pointer to the array data for thin-pointer value VAL.  */

static struct value *
thin_data_pntr (struct value *val)
{
  struct type *type = value_type (val);
  if (TYPE_CODE (type) == TYPE_CODE_PTR)
    return value_cast (desc_data_type (thin_descriptor_type (type)),
                       value_copy (val));
  else
    return value_from_longest (desc_data_type (thin_descriptor_type (type)),
                               VALUE_ADDRESS (val) + value_offset (val));
}

/* True iff TYPE indicates a "thick" array pointer type.  */

static int
is_thick_pntr (struct type *type)
{
  type = desc_base_type (type);
  return (type != NULL && TYPE_CODE (type) == TYPE_CODE_STRUCT
          && lookup_struct_elt_type (type, "P_BOUNDS", 1) != NULL);
}

/* If TYPE is the type of an array descriptor (fat or thin pointer) or a
   pointer to one, the type of its bounds data; otherwise, NULL.  */

static struct type *
desc_bounds_type (struct type *type)
{
  struct type *r;

  type = desc_base_type (type);

  if (type == NULL)
    return NULL;
  else if (is_thin_pntr (type))
    {
      type = thin_descriptor_type (type);
      if (type == NULL)
        return NULL;
      r = lookup_struct_elt_type (type, "BOUNDS", 1);
      if (r != NULL)
        return ada_check_typedef (r);
    }
  else if (TYPE_CODE (type) == TYPE_CODE_STRUCT)
    {
      r = lookup_struct_elt_type (type, "P_BOUNDS", 1);
      if (r != NULL)
        return ada_check_typedef (TYPE_TARGET_TYPE (ada_check_typedef (r)));
    }
  return NULL;
}

/* If ARR is an array descriptor (fat or thin pointer), or pointer to
   one, a pointer to its bounds data.   Otherwise NULL.  */

static struct value *
desc_bounds (struct value *arr)
{
  struct type *type = ada_check_typedef (value_type (arr));
  if (is_thin_pntr (type))
    {
      struct type *bounds_type =
        desc_bounds_type (thin_descriptor_type (type));
      LONGEST addr;

      if (bounds_type == NULL)
        error (_("Bad GNAT array descriptor"));

      /* NOTE: The following calculation is not really kosher, but
         since desc_type is an XVE-encoded type (and shouldn't be),
         the correct calculation is a real pain.  FIXME (and fix GCC).  */
      if (TYPE_CODE (type) == TYPE_CODE_PTR)
        addr = value_as_long (arr);
      else
        addr = VALUE_ADDRESS (arr) + value_offset (arr);

      return
        value_from_longest (lookup_pointer_type (bounds_type),
                            addr - TYPE_LENGTH (bounds_type));
    }

  else if (is_thick_pntr (type))
    return value_struct_elt (&arr, NULL, "P_BOUNDS", NULL,
                             _("Bad GNAT array descriptor"));
  else
    return NULL;
}

/* If TYPE is the type of an array-descriptor (fat pointer),  the bit
   position of the field containing the address of the bounds data.  */

static int
fat_pntr_bounds_bitpos (struct type *type)
{
  return TYPE_FIELD_BITPOS (desc_base_type (type), 1);
}

/* If TYPE is the type of an array-descriptor (fat pointer), the bit
   size of the field containing the address of the bounds data.  */

static int
fat_pntr_bounds_bitsize (struct type *type)
{
  type = desc_base_type (type);

  if (TYPE_FIELD_BITSIZE (type, 1) > 0)
    return TYPE_FIELD_BITSIZE (type, 1);
  else
    return 8 * TYPE_LENGTH (ada_check_typedef (TYPE_FIELD_TYPE (type, 1)));
}

/* If TYPE is the type of an array descriptor (fat or thin pointer) or a
   pointer to one, the type of its array data (a
   pointer-to-array-with-no-bounds type); otherwise, NULL.  Use
   ada_type_of_array to get an array type with bounds data.  */

static struct type *
desc_data_type (struct type *type)
{
  type = desc_base_type (type);

  /* NOTE: The following is bogus; see comment in desc_bounds.  */
  if (is_thin_pntr (type))
    return lookup_pointer_type
      (desc_base_type (TYPE_FIELD_TYPE (thin_descriptor_type (type), 1)));
  else if (is_thick_pntr (type))
    return lookup_struct_elt_type (type, "P_ARRAY", 1);
  else
    return NULL;
}

/* If ARR is an array descriptor (fat or thin pointer), a pointer to
   its array data.  */

static struct value *
desc_data (struct value *arr)
{
  struct type *type = value_type (arr);
  if (is_thin_pntr (type))
    return thin_data_pntr (arr);
  else if (is_thick_pntr (type))
    return value_struct_elt (&arr, NULL, "P_ARRAY", NULL,
                             _("Bad GNAT array descriptor"));
  else
    return NULL;
}


/* If TYPE is the type of an array-descriptor (fat pointer), the bit
   position of the field containing the address of the data.  */

static int
fat_pntr_data_bitpos (struct type *type)
{
  return TYPE_FIELD_BITPOS (desc_base_type (type), 0);
}

/* If TYPE is the type of an array-descriptor (fat pointer), the bit
   size of the field containing the address of the data.  */

static int
fat_pntr_data_bitsize (struct type *type)
{
  type = desc_base_type (type);

  if (TYPE_FIELD_BITSIZE (type, 0) > 0)
    return TYPE_FIELD_BITSIZE (type, 0);
  else
    return TARGET_CHAR_BIT * TYPE_LENGTH (TYPE_FIELD_TYPE (type, 0));
}

/* If BOUNDS is an array-bounds structure (or pointer to one), return
   the Ith lower bound stored in it, if WHICH is 0, and the Ith upper
   bound, if WHICH is 1.  The first bound is I=1.  */

static struct value *
desc_one_bound (struct value *bounds, int i, int which)
{
  return value_struct_elt (&bounds, NULL, bound_name[2 * i + which - 2], NULL,
                           _("Bad GNAT array descriptor bounds"));
}

/* If BOUNDS is an array-bounds structure type, return the bit position
   of the Ith lower bound stored in it, if WHICH is 0, and the Ith upper
   bound, if WHICH is 1.  The first bound is I=1.  */

static int
desc_bound_bitpos (struct type *type, int i, int which)
{
  return TYPE_FIELD_BITPOS (desc_base_type (type), 2 * i + which - 2);
}

/* If BOUNDS is an array-bounds structure type, return the bit field size
   of the Ith lower bound stored in it, if WHICH is 0, and the Ith upper
   bound, if WHICH is 1.  The first bound is I=1.  */

static int
desc_bound_bitsize (struct type *type, int i, int which)
{
  type = desc_base_type (type);

  if (TYPE_FIELD_BITSIZE (type, 2 * i + which - 2) > 0)
    return TYPE_FIELD_BITSIZE (type, 2 * i + which - 2);
  else
    return 8 * TYPE_LENGTH (TYPE_FIELD_TYPE (type, 2 * i + which - 2));
}

/* If TYPE is the type of an array-bounds structure, the type of its
   Ith bound (numbering from 1).  Otherwise, NULL.  */

static struct type *
desc_index_type (struct type *type, int i)
{
  type = desc_base_type (type);

  if (TYPE_CODE (type) == TYPE_CODE_STRUCT)
    return lookup_struct_elt_type (type, bound_name[2 * i - 2], 1);
  else
    return NULL;
}

/* The number of index positions in the array-bounds type TYPE.
   Return 0 if TYPE is NULL.  */

static int
desc_arity (struct type *type)
{
  type = desc_base_type (type);

  if (type != NULL)
    return TYPE_NFIELDS (type) / 2;
  return 0;
}

/* Non-zero iff TYPE is a simple array type (not a pointer to one) or 
   an array descriptor type (representing an unconstrained array
   type).  */

static int
ada_is_direct_array_type (struct type *type)
{
  if (type == NULL)
    return 0;
  type = ada_check_typedef (type);
  return (TYPE_CODE (type) == TYPE_CODE_ARRAY
          || ada_is_array_descriptor_type (type));
}

/* Non-zero iff TYPE represents any kind of array in Ada, or a pointer
 * to one. */

int
ada_is_array_type (struct type *type)
{
  while (type != NULL 
	 && (TYPE_CODE (type) == TYPE_CODE_PTR 
	     || TYPE_CODE (type) == TYPE_CODE_REF))
    type = TYPE_TARGET_TYPE (type);
  return ada_is_direct_array_type (type);
}

/* Non-zero iff TYPE is a simple array type or pointer to one.  */

int
ada_is_simple_array_type (struct type *type)
{
  if (type == NULL)
    return 0;
  type = ada_check_typedef (type);
  return (TYPE_CODE (type) == TYPE_CODE_ARRAY
          || (TYPE_CODE (type) == TYPE_CODE_PTR
              && TYPE_CODE (TYPE_TARGET_TYPE (type)) == TYPE_CODE_ARRAY));
}

/* Non-zero iff TYPE belongs to a GNAT array descriptor.  */

int
ada_is_array_descriptor_type (struct type *type)
{
  struct type *data_type = desc_data_type (type);

  if (type == NULL)
    return 0;
  type = ada_check_typedef (type);
  return
    data_type != NULL
    && ((TYPE_CODE (data_type) == TYPE_CODE_PTR
         && TYPE_TARGET_TYPE (data_type) != NULL
         && TYPE_CODE (TYPE_TARGET_TYPE (data_type)) == TYPE_CODE_ARRAY)
        || TYPE_CODE (data_type) == TYPE_CODE_ARRAY)
    && desc_arity (desc_bounds_type (type)) > 0;
}

/* Non-zero iff type is a partially mal-formed GNAT array
   descriptor.  FIXME: This is to compensate for some problems with
   debugging output from GNAT.  Re-examine periodically to see if it
   is still needed.  */

int
ada_is_bogus_array_descriptor (struct type *type)
{
  return
    type != NULL
    && TYPE_CODE (type) == TYPE_CODE_STRUCT
    && (lookup_struct_elt_type (type, "P_BOUNDS", 1) != NULL
        || lookup_struct_elt_type (type, "P_ARRAY", 1) != NULL)
    && !ada_is_array_descriptor_type (type);
}


/* If ARR has a record type in the form of a standard GNAT array descriptor,
   (fat pointer) returns the type of the array data described---specifically,
   a pointer-to-array type.  If BOUNDS is non-zero, the bounds data are filled
   in from the descriptor; otherwise, they are left unspecified.  If
   the ARR denotes a null array descriptor and BOUNDS is non-zero,
   returns NULL.  The result is simply the type of ARR if ARR is not
   a descriptor.  */
struct type *
ada_type_of_array (struct value *arr, int bounds)
{
  if (ada_is_packed_array_type (value_type (arr)))
    return decode_packed_array_type (value_type (arr));

  if (!ada_is_array_descriptor_type (value_type (arr)))
    return value_type (arr);

  if (!bounds)
    return
      ada_check_typedef (TYPE_TARGET_TYPE (desc_data_type (value_type (arr))));
  else
    {
      struct type *elt_type;
      int arity;
      struct value *descriptor;
      struct objfile *objf = TYPE_OBJFILE (value_type (arr));

      elt_type = ada_array_element_type (value_type (arr), -1);
      arity = ada_array_arity (value_type (arr));

      if (elt_type == NULL || arity == 0)
        return ada_check_typedef (value_type (arr));

      descriptor = desc_bounds (arr);
      if (value_as_long (descriptor) == 0)
        return NULL;
      while (arity > 0)
        {
          struct type *range_type = alloc_type (objf);
          struct type *array_type = alloc_type (objf);
          struct value *low = desc_one_bound (descriptor, arity, 0);
          struct value *high = desc_one_bound (descriptor, arity, 1);
          arity -= 1;

          create_range_type (range_type, value_type (low),
                             longest_to_int (value_as_long (low)),
                             longest_to_int (value_as_long (high)));
          elt_type = create_array_type (array_type, elt_type, range_type);
        }

      return lookup_pointer_type (elt_type);
    }
}

/* If ARR does not represent an array, returns ARR unchanged.
   Otherwise, returns either a standard GDB array with bounds set
   appropriately or, if ARR is a non-null fat pointer, a pointer to a standard
   GDB array.  Returns NULL if ARR is a null fat pointer.  */

struct value *
ada_coerce_to_simple_array_ptr (struct value *arr)
{
  if (ada_is_array_descriptor_type (value_type (arr)))
    {
      struct type *arrType = ada_type_of_array (arr, 1);
      if (arrType == NULL)
        return NULL;
      return value_cast (arrType, value_copy (desc_data (arr)));
    }
  else if (ada_is_packed_array_type (value_type (arr)))
    return decode_packed_array (arr);
  else
    return arr;
}

/* If ARR does not represent an array, returns ARR unchanged.
   Otherwise, returns a standard GDB array describing ARR (which may
   be ARR itself if it already is in the proper form).  */

static struct value *
ada_coerce_to_simple_array (struct value *arr)
{
  if (ada_is_array_descriptor_type (value_type (arr)))
    {
      struct value *arrVal = ada_coerce_to_simple_array_ptr (arr);
      if (arrVal == NULL)
        error (_("Bounds unavailable for null array pointer."));
      check_size (TYPE_TARGET_TYPE (value_type (arrVal)));
      return value_ind (arrVal);
    }
  else if (ada_is_packed_array_type (value_type (arr)))
    return decode_packed_array (arr);
  else
    return arr;
}

/* If TYPE represents a GNAT array type, return it translated to an
   ordinary GDB array type (possibly with BITSIZE fields indicating
   packing).  For other types, is the identity.  */

struct type *
ada_coerce_to_simple_array_type (struct type *type)
{
  struct value *mark = value_mark ();
  struct value *dummy = value_from_longest (builtin_type_long, 0);
  struct type *result;
  deprecated_set_value_type (dummy, type);
  result = ada_type_of_array (dummy, 0);
  value_free_to_mark (mark);
  return result;
}

/* Non-zero iff TYPE represents a standard GNAT packed-array type.  */

int
ada_is_packed_array_type (struct type *type)
{
  if (type == NULL)
    return 0;
  type = desc_base_type (type);
  type = ada_check_typedef (type);
  return
    ada_type_name (type) != NULL
    && strstr (ada_type_name (type), "___XP") != NULL;
}

/* Given that TYPE is a standard GDB array type with all bounds filled
   in, and that the element size of its ultimate scalar constituents
   (that is, either its elements, or, if it is an array of arrays, its
   elements' elements, etc.) is *ELT_BITS, return an identical type,
   but with the bit sizes of its elements (and those of any
   constituent arrays) recorded in the BITSIZE components of its
   TYPE_FIELD_BITSIZE values, and with *ELT_BITS set to its total size
   in bits.  */

static struct type *
packed_array_type (struct type *type, long *elt_bits)
{
  struct type *new_elt_type;
  struct type *new_type;
  LONGEST low_bound, high_bound;

  type = ada_check_typedef (type);
  if (TYPE_CODE (type) != TYPE_CODE_ARRAY)
    return type;

  new_type = alloc_type (TYPE_OBJFILE (type));
  new_elt_type = packed_array_type (ada_check_typedef (TYPE_TARGET_TYPE (type)),
                                    elt_bits);
  create_array_type (new_type, new_elt_type, TYPE_FIELD_TYPE (type, 0));
  TYPE_FIELD_BITSIZE (new_type, 0) = *elt_bits;
  TYPE_NAME (new_type) = ada_type_name (type);

  if (get_discrete_bounds (TYPE_FIELD_TYPE (type, 0),
                           &low_bound, &high_bound) < 0)
    low_bound = high_bound = 0;
  if (high_bound < low_bound)
    *elt_bits = TYPE_LENGTH (new_type) = 0;
  else
    {
      *elt_bits *= (high_bound - low_bound + 1);
      TYPE_LENGTH (new_type) =
        (*elt_bits + HOST_CHAR_BIT - 1) / HOST_CHAR_BIT;
    }

  TYPE_FLAGS (new_type) |= TYPE_FLAG_FIXED_INSTANCE;
  return new_type;
}

/* The array type encoded by TYPE, where ada_is_packed_array_type (TYPE).  */

static struct type *
decode_packed_array_type (struct type *type)
{
  struct symbol *sym;
  struct block **blocks;
  const char *raw_name = ada_type_name (ada_check_typedef (type));
  char *name = (char *) alloca (strlen (raw_name) + 1);
  char *tail = strstr (raw_name, "___XP");
  struct type *shadow_type;
  long bits;
  int i, n;

  type = desc_base_type (type);

  memcpy (name, raw_name, tail - raw_name);
  name[tail - raw_name] = '\000';

  sym = standard_lookup (name, get_selected_block (0), VAR_DOMAIN);
  if (sym == NULL || SYMBOL_TYPE (sym) == NULL)
    {
      lim_warning (_("could not find bounds information on packed array"));
      return NULL;
    }
  shadow_type = SYMBOL_TYPE (sym);

  if (TYPE_CODE (shadow_type) != TYPE_CODE_ARRAY)
    {
      lim_warning (_("could not understand bounds information on packed array"));
      return NULL;
    }

  if (sscanf (tail + sizeof ("___XP") - 1, "%ld", &bits) != 1)
    {
      lim_warning
	(_("could not understand bit size information on packed array"));
      return NULL;
    }

  return packed_array_type (shadow_type, &bits);
}

/* Given that ARR is a struct value *indicating a GNAT packed array,
   returns a simple array that denotes that array.  Its type is a
   standard GDB array type except that the BITSIZEs of the array
   target types are set to the number of bits in each element, and the
   type length is set appropriately.  */

static struct value *
decode_packed_array (struct value *arr)
{
  struct type *type;

  arr = ada_coerce_ref (arr);
  if (TYPE_CODE (value_type (arr)) == TYPE_CODE_PTR)
    arr = ada_value_ind (arr);

  type = decode_packed_array_type (value_type (arr));
  if (type == NULL)
    {
      error (_("can't unpack array"));
      return NULL;
    }

  if (BITS_BIG_ENDIAN && ada_is_modular_type (value_type (arr)))
    {
       /* This is a (right-justified) modular type representing a packed
 	 array with no wrapper.  In order to interpret the value through
 	 the (left-justified) packed array type we just built, we must
 	 first left-justify it.  */
      int bit_size, bit_pos;
      ULONGEST mod;

      mod = ada_modulus (value_type (arr)) - 1;
      bit_size = 0;
      while (mod > 0)
	{
	  bit_size += 1;
	  mod >>= 1;
	}
      bit_pos = HOST_CHAR_BIT * TYPE_LENGTH (value_type (arr)) - bit_size;
      arr = ada_value_primitive_packed_val (arr, NULL,
					    bit_pos / HOST_CHAR_BIT,
					    bit_pos % HOST_CHAR_BIT,
					    bit_size,
					    type);
    }

  return coerce_unspec_val_to_type (arr, type);
}


/* The value of the element of packed array ARR at the ARITY indices
   given in IND.   ARR must be a simple array.  */

static struct value *
value_subscript_packed (struct value *arr, int arity, struct value **ind)
{
  int i;
  int bits, elt_off, bit_off;
  long elt_total_bit_offset;
  struct type *elt_type;
  struct value *v;

  bits = 0;
  elt_total_bit_offset = 0;
  elt_type = ada_check_typedef (value_type (arr));
  for (i = 0; i < arity; i += 1)
    {
      if (TYPE_CODE (elt_type) != TYPE_CODE_ARRAY
          || TYPE_FIELD_BITSIZE (elt_type, 0) == 0)
        error
          (_("attempt to do packed indexing of something other than a packed array"));
      else
        {
          struct type *range_type = TYPE_INDEX_TYPE (elt_type);
          LONGEST lowerbound, upperbound;
          LONGEST idx;

          if (get_discrete_bounds (range_type, &lowerbound, &upperbound) < 0)
            {
              lim_warning (_("don't know bounds of array"));
              lowerbound = upperbound = 0;
            }

          idx = value_as_long (value_pos_atr (ind[i]));
          if (idx < lowerbound || idx > upperbound)
            lim_warning (_("packed array index %ld out of bounds"), (long) idx);
          bits = TYPE_FIELD_BITSIZE (elt_type, 0);
          elt_total_bit_offset += (idx - lowerbound) * bits;
          elt_type = ada_check_typedef (TYPE_TARGET_TYPE (elt_type));
        }
    }
  elt_off = elt_total_bit_offset / HOST_CHAR_BIT;
  bit_off = elt_total_bit_offset % HOST_CHAR_BIT;

  v = ada_value_primitive_packed_val (arr, NULL, elt_off, bit_off,
                                      bits, elt_type);
  return v;
}

/* Non-zero iff TYPE includes negative integer values.  */

static int
has_negatives (struct type *type)
{
  switch (TYPE_CODE (type))
    {
    default:
      return 0;
    case TYPE_CODE_INT:
      return !TYPE_UNSIGNED (type);
    case TYPE_CODE_RANGE:
      return TYPE_LOW_BOUND (type) < 0;
    }
}


/* Create a new value of type TYPE from the contents of OBJ starting
   at byte OFFSET, and bit offset BIT_OFFSET within that byte,
   proceeding for BIT_SIZE bits.  If OBJ is an lval in memory, then
   assigning through the result will set the field fetched from.  
   VALADDR is ignored unless OBJ is NULL, in which case,
   VALADDR+OFFSET must address the start of storage containing the 
   packed value.  The value returned  in this case is never an lval.
   Assumes 0 <= BIT_OFFSET < HOST_CHAR_BIT.  */

struct value *
ada_value_primitive_packed_val (struct value *obj, const gdb_byte *valaddr,
				long offset, int bit_offset, int bit_size,
                                struct type *type)
{
  struct value *v;
  int src,                      /* Index into the source area */
    targ,                       /* Index into the target area */
    srcBitsLeft,                /* Number of source bits left to move */
    nsrc, ntarg,                /* Number of source and target bytes */
    unusedLS,                   /* Number of bits in next significant
                                   byte of source that are unused */
    accumSize;                  /* Number of meaningful bits in accum */
  unsigned char *bytes;         /* First byte containing data to unpack */
  unsigned char *unpacked;
  unsigned long accum;          /* Staging area for bits being transferred */
  unsigned char sign;
  int len = (bit_size + bit_offset + HOST_CHAR_BIT - 1) / 8;
  /* Transmit bytes from least to most significant; delta is the direction
     the indices move.  */
  int delta = BITS_BIG_ENDIAN ? -1 : 1;

  type = ada_check_typedef (type);

  if (obj == NULL)
    {
      v = allocate_value (type);
      bytes = (unsigned char *) (valaddr + offset);
    }
  else if (value_lazy (obj))
    {
      v = value_at (type,
                    VALUE_ADDRESS (obj) + value_offset (obj) + offset);
      bytes = (unsigned char *) alloca (len);
      read_memory (VALUE_ADDRESS (v), bytes, len);
    }
  else
    {
      v = allocate_value (type);
      bytes = (unsigned char *) value_contents (obj) + offset;
    }

  if (obj != NULL)
    {
      VALUE_LVAL (v) = VALUE_LVAL (obj);
      if (VALUE_LVAL (obj) == lval_internalvar)
        VALUE_LVAL (v) = lval_internalvar_component;
      VALUE_ADDRESS (v) = VALUE_ADDRESS (obj) + value_offset (obj) + offset;
      set_value_bitpos (v, bit_offset + value_bitpos (obj));
      set_value_bitsize (v, bit_size);
      if (value_bitpos (v) >= HOST_CHAR_BIT)
        {
          VALUE_ADDRESS (v) += 1;
          set_value_bitpos (v, value_bitpos (v) - HOST_CHAR_BIT);
        }
    }
  else
    set_value_bitsize (v, bit_size);
  unpacked = (unsigned char *) value_contents (v);

  srcBitsLeft = bit_size;
  nsrc = len;
  ntarg = TYPE_LENGTH (type);
  sign = 0;
  if (bit_size == 0)
    {
      memset (unpacked, 0, TYPE_LENGTH (type));
      return v;
    }
  else if (BITS_BIG_ENDIAN)
    {
      src = len - 1;
      if (has_negatives (type)
          && ((bytes[0] << bit_offset) & (1 << (HOST_CHAR_BIT - 1))))
        sign = ~0;

      unusedLS =
        (HOST_CHAR_BIT - (bit_size + bit_offset) % HOST_CHAR_BIT)
        % HOST_CHAR_BIT;

      switch (TYPE_CODE (type))
        {
        case TYPE_CODE_ARRAY:
        case TYPE_CODE_UNION:
        case TYPE_CODE_STRUCT:
          /* Non-scalar values must be aligned at a byte boundary...  */
          accumSize =
            (HOST_CHAR_BIT - bit_size % HOST_CHAR_BIT) % HOST_CHAR_BIT;
          /* ... And are placed at the beginning (most-significant) bytes
             of the target.  */
          targ = (bit_size + HOST_CHAR_BIT - 1) / HOST_CHAR_BIT - 1;
          break;
        default:
          accumSize = 0;
          targ = TYPE_LENGTH (type) - 1;
          break;
        }
    }
  else
    {
      int sign_bit_offset = (bit_size + bit_offset - 1) % 8;

      src = targ = 0;
      unusedLS = bit_offset;
      accumSize = 0;

      if (has_negatives (type) && (bytes[len - 1] & (1 << sign_bit_offset)))
        sign = ~0;
    }

  accum = 0;
  while (nsrc > 0)
    {
      /* Mask for removing bits of the next source byte that are not
         part of the value.  */
      unsigned int unusedMSMask =
        (1 << (srcBitsLeft >= HOST_CHAR_BIT ? HOST_CHAR_BIT : srcBitsLeft)) -
        1;
      /* Sign-extend bits for this byte.  */
      unsigned int signMask = sign & ~unusedMSMask;
      accum |=
        (((bytes[src] >> unusedLS) & unusedMSMask) | signMask) << accumSize;
      accumSize += HOST_CHAR_BIT - unusedLS;
      if (accumSize >= HOST_CHAR_BIT)
        {
          unpacked[targ] = accum & ~(~0L << HOST_CHAR_BIT);
          accumSize -= HOST_CHAR_BIT;
          accum >>= HOST_CHAR_BIT;
          ntarg -= 1;
          targ += delta;
        }
      srcBitsLeft -= HOST_CHAR_BIT - unusedLS;
      unusedLS = 0;
      nsrc -= 1;
      src += delta;
    }
  while (ntarg > 0)
    {
      accum |= sign << accumSize;
      unpacked[targ] = accum & ~(~0L << HOST_CHAR_BIT);
      accumSize -= HOST_CHAR_BIT;
      accum >>= HOST_CHAR_BIT;
      ntarg -= 1;
      targ += delta;
    }

  return v;
}

/* Move N bits from SOURCE, starting at bit offset SRC_OFFSET to
   TARGET, starting at bit offset TARG_OFFSET.  SOURCE and TARGET must
   not overlap.  */
static void
move_bits (gdb_byte *target, int targ_offset, const gdb_byte *source,
	   int src_offset, int n)
{
  unsigned int accum, mask;
  int accum_bits, chunk_size;

  target += targ_offset / HOST_CHAR_BIT;
  targ_offset %= HOST_CHAR_BIT;
  source += src_offset / HOST_CHAR_BIT;
  src_offset %= HOST_CHAR_BIT;
  if (BITS_BIG_ENDIAN)
    {
      accum = (unsigned char) *source;
      source += 1;
      accum_bits = HOST_CHAR_BIT - src_offset;

      while (n > 0)
        {
          int unused_right;
          accum = (accum << HOST_CHAR_BIT) + (unsigned char) *source;
          accum_bits += HOST_CHAR_BIT;
          source += 1;
          chunk_size = HOST_CHAR_BIT - targ_offset;
          if (chunk_size > n)
            chunk_size = n;
          unused_right = HOST_CHAR_BIT - (chunk_size + targ_offset);
          mask = ((1 << chunk_size) - 1) << unused_right;
          *target =
            (*target & ~mask)
            | ((accum >> (accum_bits - chunk_size - unused_right)) & mask);
          n -= chunk_size;
          accum_bits -= chunk_size;
          target += 1;
          targ_offset = 0;
        }
    }
  else
    {
      accum = (unsigned char) *source >> src_offset;
      source += 1;
      accum_bits = HOST_CHAR_BIT - src_offset;

      while (n > 0)
        {
          accum = accum + ((unsigned char) *source << accum_bits);
          accum_bits += HOST_CHAR_BIT;
          source += 1;
          chunk_size = HOST_CHAR_BIT - targ_offset;
          if (chunk_size > n)
            chunk_size = n;
          mask = ((1 << chunk_size) - 1) << targ_offset;
          *target = (*target & ~mask) | ((accum << targ_offset) & mask);
          n -= chunk_size;
          accum_bits -= chunk_size;
          accum >>= chunk_size;
          target += 1;
          targ_offset = 0;
        }
    }
}

/* Store the contents of FROMVAL into the location of TOVAL.
   Return a new value with the location of TOVAL and contents of
   FROMVAL.   Handles assignment into packed fields that have
   floating-point or non-scalar types.  */

static struct value *
ada_value_assign (struct value *toval, struct value *fromval)
{
  struct type *type = value_type (toval);
  int bits = value_bitsize (toval);

  toval = ada_coerce_ref (toval);
  fromval = ada_coerce_ref (fromval);

  if (ada_is_direct_array_type (value_type (toval)))
    toval = ada_coerce_to_simple_array (toval);
  if (ada_is_direct_array_type (value_type (fromval)))
    fromval = ada_coerce_to_simple_array (fromval);

  if (!deprecated_value_modifiable (toval))
    error (_("Left operand of assignment is not a modifiable lvalue."));

  if (VALUE_LVAL (toval) == lval_memory
      && bits > 0
      && (TYPE_CODE (type) == TYPE_CODE_FLT
          || TYPE_CODE (type) == TYPE_CODE_STRUCT))
    {
      int len = (value_bitpos (toval)
		 + bits + HOST_CHAR_BIT - 1) / HOST_CHAR_BIT;
      char *buffer = (char *) alloca (len);
      struct value *val;
      CORE_ADDR to_addr = VALUE_ADDRESS (toval) + value_offset (toval);

      if (TYPE_CODE (type) == TYPE_CODE_FLT)
        fromval = value_cast (type, fromval);

      read_memory (to_addr, buffer, len);
      if (BITS_BIG_ENDIAN)
        move_bits (buffer, value_bitpos (toval),
                   value_contents (fromval),
                   TYPE_LENGTH (value_type (fromval)) * TARGET_CHAR_BIT -
                   bits, bits);
      else
        move_bits (buffer, value_bitpos (toval), value_contents (fromval),
                   0, bits);
      write_memory (to_addr, buffer, len);
      if (deprecated_memory_changed_hook)
	deprecated_memory_changed_hook (to_addr, len);
      
      val = value_copy (toval);
      memcpy (value_contents_raw (val), value_contents (fromval),
              TYPE_LENGTH (type));
      deprecated_set_value_type (val, type);

      return val;
    }

  return value_assign (toval, fromval);
}


/* Given that COMPONENT is a memory lvalue that is part of the lvalue 
 * CONTAINER, assign the contents of VAL to COMPONENTS's place in 
 * CONTAINER.  Modifies the VALUE_CONTENTS of CONTAINER only, not 
 * COMPONENT, and not the inferior's memory.  The current contents 
 * of COMPONENT are ignored.  */
static void
value_assign_to_component (struct value *container, struct value *component,
			   struct value *val)
{
  LONGEST offset_in_container =
    (LONGEST)  (VALUE_ADDRESS (component) + value_offset (component)
		- VALUE_ADDRESS (container) - value_offset (container));
  int bit_offset_in_container = 
    value_bitpos (component) - value_bitpos (container);
  int bits;
  
  val = value_cast (value_type (component), val);

  if (value_bitsize (component) == 0)
    bits = TARGET_CHAR_BIT * TYPE_LENGTH (value_type (component));
  else
    bits = value_bitsize (component);

  if (BITS_BIG_ENDIAN)
    move_bits (value_contents_writeable (container) + offset_in_container, 
	       value_bitpos (container) + bit_offset_in_container,
	       value_contents (val),
	       TYPE_LENGTH (value_type (component)) * TARGET_CHAR_BIT - bits,
	       bits);
  else
    move_bits (value_contents_writeable (container) + offset_in_container, 
	       value_bitpos (container) + bit_offset_in_container,
	       value_contents (val), 0, bits);
}	       
			
/* The value of the element of array ARR at the ARITY indices given in IND.
   ARR may be either a simple array, GNAT array descriptor, or pointer
   thereto.  */

struct value *
ada_value_subscript (struct value *arr, int arity, struct value **ind)
{
  int k;
  struct value *elt;
  struct type *elt_type;

  elt = ada_coerce_to_simple_array (arr);

  elt_type = ada_check_typedef (value_type (elt));
  if (TYPE_CODE (elt_type) == TYPE_CODE_ARRAY
      && TYPE_FIELD_BITSIZE (elt_type, 0) > 0)
    return value_subscript_packed (elt, arity, ind);

  for (k = 0; k < arity; k += 1)
    {
      if (TYPE_CODE (elt_type) != TYPE_CODE_ARRAY)
        error (_("too many subscripts (%d expected)"), k);
      elt = value_subscript (elt, value_pos_atr (ind[k]));
    }
  return elt;
}

/* Assuming ARR is a pointer to a standard GDB array of type TYPE, the
   value of the element of *ARR at the ARITY indices given in
   IND.  Does not read the entire array into memory.  */

struct value *
ada_value_ptr_subscript (struct value *arr, struct type *type, int arity,
                         struct value **ind)
{
  int k;

  for (k = 0; k < arity; k += 1)
    {
      LONGEST lwb, upb;
      struct value *idx;

      if (TYPE_CODE (type) != TYPE_CODE_ARRAY)
        error (_("too many subscripts (%d expected)"), k);
      arr = value_cast (lookup_pointer_type (TYPE_TARGET_TYPE (type)),
                        value_copy (arr));
      get_discrete_bounds (TYPE_INDEX_TYPE (type), &lwb, &upb);
      idx = value_pos_atr (ind[k]);
      if (lwb != 0)
        idx = value_sub (idx, value_from_longest (builtin_type_int, lwb));
      arr = value_add (arr, idx);
      type = TYPE_TARGET_TYPE (type);
    }

  return value_ind (arr);
}

/* Given that ARRAY_PTR is a pointer or reference to an array of type TYPE (the
   actual type of ARRAY_PTR is ignored), returns a reference to
   the Ada slice of HIGH-LOW+1 elements starting at index LOW.  The lower
   bound of this array is LOW, as per Ada rules. */
static struct value *
ada_value_slice_ptr (struct value *array_ptr, struct type *type,
                     int low, int high)
{
  CORE_ADDR base = value_as_address (array_ptr)
    + ((low - TYPE_LOW_BOUND (TYPE_INDEX_TYPE (type)))
       * TYPE_LENGTH (TYPE_TARGET_TYPE (type)));
  struct type *index_type =
    create_range_type (NULL, TYPE_TARGET_TYPE (TYPE_INDEX_TYPE (type)),
                       low, high);
  struct type *slice_type =
    create_array_type (NULL, TYPE_TARGET_TYPE (type), index_type);
  return value_from_pointer (lookup_reference_type (slice_type), base);
}


static struct value *
ada_value_slice (struct value *array, int low, int high)
{
  struct type *type = value_type (array);
  struct type *index_type =
    create_range_type (NULL, TYPE_INDEX_TYPE (type), low, high);
  struct type *slice_type =
    create_array_type (NULL, TYPE_TARGET_TYPE (type), index_type);
  return value_cast (slice_type, value_slice (array, low, high - low + 1));
}

/* If type is a record type in the form of a standard GNAT array
   descriptor, returns the number of dimensions for type.  If arr is a
   simple array, returns the number of "array of"s that prefix its
   type designation.  Otherwise, returns 0.  */

int
ada_array_arity (struct type *type)
{
  int arity;

  if (type == NULL)
    return 0;

  type = desc_base_type (type);

  arity = 0;
  if (TYPE_CODE (type) == TYPE_CODE_STRUCT)
    return desc_arity (desc_bounds_type (type));
  else
    while (TYPE_CODE (type) == TYPE_CODE_ARRAY)
      {
        arity += 1;
        type = ada_check_typedef (TYPE_TARGET_TYPE (type));
      }

  return arity;
}

/* If TYPE is a record type in the form of a standard GNAT array
   descriptor or a simple array type, returns the element type for
   TYPE after indexing by NINDICES indices, or by all indices if
   NINDICES is -1.  Otherwise, returns NULL.  */

struct type *
ada_array_element_type (struct type *type, int nindices)
{
  type = desc_base_type (type);

  if (TYPE_CODE (type) == TYPE_CODE_STRUCT)
    {
      int k;
      struct type *p_array_type;

      p_array_type = desc_data_type (type);

      k = ada_array_arity (type);
      if (k == 0)
        return NULL;

      /* Initially p_array_type = elt_type(*)[]...(k times)...[].  */
      if (nindices >= 0 && k > nindices)
        k = nindices;
      p_array_type = TYPE_TARGET_TYPE (p_array_type);
      while (k > 0 && p_array_type != NULL)
        {
          p_array_type = ada_check_typedef (TYPE_TARGET_TYPE (p_array_type));
          k -= 1;
        }
      return p_array_type;
    }
  else if (TYPE_CODE (type) == TYPE_CODE_ARRAY)
    {
      while (nindices != 0 && TYPE_CODE (type) == TYPE_CODE_ARRAY)
        {
          type = TYPE_TARGET_TYPE (type);
          nindices -= 1;
        }
      return type;
    }

  return NULL;
}

/* The type of nth index in arrays of given type (n numbering from 1).
   Does not examine memory.  */

struct type *
ada_index_type (struct type *type, int n)
{
  struct type *result_type;

  type = desc_base_type (type);

  if (n > ada_array_arity (type))
    return NULL;

  if (ada_is_simple_array_type (type))
    {
      int i;

      for (i = 1; i < n; i += 1)
        type = TYPE_TARGET_TYPE (type);
      result_type = TYPE_TARGET_TYPE (TYPE_FIELD_TYPE (type, 0));
      /* FIXME: The stabs type r(0,0);bound;bound in an array type
         has a target type of TYPE_CODE_UNDEF.  We compensate here, but
         perhaps stabsread.c would make more sense.  */
      if (result_type == NULL || TYPE_CODE (result_type) == TYPE_CODE_UNDEF)
        result_type = builtin_type_int;

      return result_type;
    }
  else
    return desc_index_type (desc_bounds_type (type), n);
}

/* Given that arr is an array type, returns the lower bound of the
   Nth index (numbering from 1) if WHICH is 0, and the upper bound if
   WHICH is 1.  This returns bounds 0 .. -1 if ARR_TYPE is an
   array-descriptor type.  If TYPEP is non-null, *TYPEP is set to the
   bounds type.  It works for other arrays with bounds supplied by
   run-time quantities other than discriminants.  */

LONGEST
ada_array_bound_from_type (struct type * arr_type, int n, int which,
                           struct type ** typep)
{
  struct type *type;
  struct type *index_type_desc;

  if (ada_is_packed_array_type (arr_type))
    arr_type = decode_packed_array_type (arr_type);

  if (arr_type == NULL || !ada_is_simple_array_type (arr_type))
    {
      if (typep != NULL)
        *typep = builtin_type_int;
      return (LONGEST) - which;
    }

  if (TYPE_CODE (arr_type) == TYPE_CODE_PTR)
    type = TYPE_TARGET_TYPE (arr_type);
  else
    type = arr_type;

  index_type_desc = ada_find_parallel_type (type, "___XA");
  if (index_type_desc == NULL)
    {
      struct type *range_type;
      struct type *index_type;

      while (n > 1)
        {
          type = TYPE_TARGET_TYPE (type);
          n -= 1;
        }

      range_type = TYPE_INDEX_TYPE (type);
      index_type = TYPE_TARGET_TYPE (range_type);
      if (TYPE_CODE (index_type) == TYPE_CODE_UNDEF)
        index_type = builtin_type_long;
      if (typep != NULL)
        *typep = index_type;
      return
        (LONGEST) (which == 0
                   ? TYPE_LOW_BOUND (range_type)
                   : TYPE_HIGH_BOUND (range_type));
    }
  else
    {
      struct type *index_type =
        to_fixed_range_type (TYPE_FIELD_NAME (index_type_desc, n - 1),
                             NULL, TYPE_OBJFILE (arr_type));
      if (typep != NULL)
        *typep = TYPE_TARGET_TYPE (index_type);
      return
        (LONGEST) (which == 0
                   ? TYPE_LOW_BOUND (index_type)
                   : TYPE_HIGH_BOUND (index_type));
    }
}

/* Given that arr is an array value, returns the lower bound of the
   nth index (numbering from 1) if which is 0, and the upper bound if
   which is 1.  This routine will also work for arrays with bounds
   supplied by run-time quantities other than discriminants.  */

struct value *
ada_array_bound (struct value *arr, int n, int which)
{
  struct type *arr_type = value_type (arr);

  if (ada_is_packed_array_type (arr_type))
    return ada_array_bound (decode_packed_array (arr), n, which);
  else if (ada_is_simple_array_type (arr_type))
    {
      struct type *type;
      LONGEST v = ada_array_bound_from_type (arr_type, n, which, &type);
      return value_from_longest (type, v);
    }
  else
    return desc_one_bound (desc_bounds (arr), n, which);
}

/* Given that arr is an array value, returns the length of the
   nth index.  This routine will also work for arrays with bounds
   supplied by run-time quantities other than discriminants.
   Does not work for arrays indexed by enumeration types with representation
   clauses at the moment.  */

struct value *
ada_array_length (struct value *arr, int n)
{
  struct type *arr_type = ada_check_typedef (value_type (arr));

  if (ada_is_packed_array_type (arr_type))
    return ada_array_length (decode_packed_array (arr), n);

  if (ada_is_simple_array_type (arr_type))
    {
      struct type *type;
      LONGEST v =
        ada_array_bound_from_type (arr_type, n, 1, &type) -
        ada_array_bound_from_type (arr_type, n, 0, NULL) + 1;
      return value_from_longest (type, v);
    }
  else
    return
      value_from_longest (builtin_type_int,
                          value_as_long (desc_one_bound (desc_bounds (arr),
                                                         n, 1))
                          - value_as_long (desc_one_bound (desc_bounds (arr),
                                                           n, 0)) + 1);
}

/* An empty array whose type is that of ARR_TYPE (an array type),
   with bounds LOW to LOW-1.  */

static struct value *
empty_array (struct type *arr_type, int low)
{
  struct type *index_type =
    create_range_type (NULL, TYPE_TARGET_TYPE (TYPE_INDEX_TYPE (arr_type)),
                       low, low - 1);
  struct type *elt_type = ada_array_element_type (arr_type, 1);
  return allocate_value (create_array_type (NULL, elt_type, index_type));
}


                                /* Name resolution */

/* The "decoded" name for the user-definable Ada operator corresponding
   to OP.  */

static const char *
ada_decoded_op_name (enum exp_opcode op)
{
  int i;

  for (i = 0; ada_opname_table[i].encoded != NULL; i += 1)
    {
      if (ada_opname_table[i].op == op)
        return ada_opname_table[i].decoded;
    }
  error (_("Could not find operator name for opcode"));
}


/* Same as evaluate_type (*EXP), but resolves ambiguous symbol
   references (marked by OP_VAR_VALUE nodes in which the symbol has an
   undefined namespace) and converts operators that are
   user-defined into appropriate function calls.  If CONTEXT_TYPE is
   non-null, it provides a preferred result type [at the moment, only
   type void has any effect---causing procedures to be preferred over
   functions in calls].  A null CONTEXT_TYPE indicates that a non-void
   return type is preferred.  May change (expand) *EXP.  */

static void
resolve (struct expression **expp, int void_context_p)
{
  int pc;
  pc = 0;
  resolve_subexp (expp, &pc, 1, void_context_p ? builtin_type_void : NULL);
}

/* Resolve the operator of the subexpression beginning at
   position *POS of *EXPP.  "Resolving" consists of replacing
   the symbols that have undefined namespaces in OP_VAR_VALUE nodes
   with their resolutions, replacing built-in operators with
   function calls to user-defined operators, where appropriate, and,
   when DEPROCEDURE_P is non-zero, converting function-valued variables
   into parameterless calls.  May expand *EXPP.  The CONTEXT_TYPE functions
   are as in ada_resolve, above.  */

static struct value *
resolve_subexp (struct expression **expp, int *pos, int deprocedure_p,
                struct type *context_type)
{
  int pc = *pos;
  int i;
  struct expression *exp;       /* Convenience: == *expp.  */
  enum exp_opcode op = (*expp)->elts[pc].opcode;
  struct value **argvec;        /* Vector of operand types (alloca'ed).  */
  int nargs;                    /* Number of operands.  */
  int oplen;

  argvec = NULL;
  nargs = 0;
  exp = *expp;

  /* Pass one: resolve operands, saving their types and updating *pos,
     if needed.  */
  switch (op)
    {
    case OP_FUNCALL:
      if (exp->elts[pc + 3].opcode == OP_VAR_VALUE
          && SYMBOL_DOMAIN (exp->elts[pc + 5].symbol) == UNDEF_DOMAIN)
        *pos += 7;
      else
        {
          *pos += 3;
          resolve_subexp (expp, pos, 0, NULL);
        }
      nargs = longest_to_int (exp->elts[pc + 1].longconst);
      break;

    case UNOP_ADDR:
      *pos += 1;
      resolve_subexp (expp, pos, 0, NULL);
      break;

    case UNOP_QUAL:
      *pos += 3;
      resolve_subexp (expp, pos, 1, exp->elts[pc + 1].type);
      break;

    case OP_ATR_MODULUS:
    case OP_ATR_SIZE:
    case OP_ATR_TAG:
    case OP_ATR_FIRST:
    case OP_ATR_LAST:
    case OP_ATR_LENGTH:
    case OP_ATR_POS:
    case OP_ATR_VAL:
    case OP_ATR_MIN:
    case OP_ATR_MAX:
    case TERNOP_IN_RANGE:
    case BINOP_IN_BOUNDS:
    case UNOP_IN_RANGE:
    case OP_AGGREGATE:
    case OP_OTHERS:
    case OP_CHOICES:
    case OP_POSITIONAL:
    case OP_DISCRETE_RANGE:
    case OP_NAME:
      ada_forward_operator_length (exp, pc, &oplen, &nargs);
      *pos += oplen;
      break;

    case BINOP_ASSIGN:
      {
        struct value *arg1;

        *pos += 1;
        arg1 = resolve_subexp (expp, pos, 0, NULL);
        if (arg1 == NULL)
          resolve_subexp (expp, pos, 1, NULL);
        else
          resolve_subexp (expp, pos, 1, value_type (arg1));
        break;
      }

    case UNOP_CAST:
      *pos += 3;
      nargs = 1;
      break;

    case BINOP_ADD:
    case BINOP_SUB:
    case BINOP_MUL:
    case BINOP_DIV:
    case BINOP_REM:
    case BINOP_MOD:
    case BINOP_EXP:
    case BINOP_CONCAT:
    case BINOP_LOGICAL_AND:
    case BINOP_LOGICAL_OR:
    case BINOP_BITWISE_AND:
    case BINOP_BITWISE_IOR:
    case BINOP_BITWISE_XOR:

    case BINOP_EQUAL:
    case BINOP_NOTEQUAL:
    case BINOP_LESS:
    case BINOP_GTR:
    case BINOP_LEQ:
    case BINOP_GEQ:

    case BINOP_REPEAT:
    case BINOP_SUBSCRIPT:
    case BINOP_COMMA:
      *pos += 1;
      nargs = 2;
      break;

    case UNOP_NEG:
    case UNOP_PLUS:
    case UNOP_LOGICAL_NOT:
    case UNOP_ABS:
    case UNOP_IND:
      *pos += 1;
      nargs = 1;
      break;

    case OP_LONG:
    case OP_DOUBLE:
    case OP_VAR_VALUE:
      *pos += 4;
      break;

    case OP_TYPE:
    case OP_BOOL:
    case OP_LAST:
    case OP_INTERNALVAR:
      *pos += 3;
      break;

    case UNOP_MEMVAL:
      *pos += 3;
      nargs = 1;
      break;

    case OP_REGISTER:
      *pos += 4 + BYTES_TO_EXP_ELEM (exp->elts[pc + 1].longconst + 1);
      break;

    case STRUCTOP_STRUCT:
      *pos += 4 + BYTES_TO_EXP_ELEM (exp->elts[pc + 1].longconst + 1);
      nargs = 1;
      break;

    case TERNOP_SLICE:
      *pos += 1;
      nargs = 3;
      break;

    case OP_STRING:
      break;

    default:
      error (_("Unexpected operator during name resolution"));
    }

  argvec = (struct value * *) alloca (sizeof (struct value *) * (nargs + 1));
  for (i = 0; i < nargs; i += 1)
    argvec[i] = resolve_subexp (expp, pos, 1, NULL);
  argvec[i] = NULL;
  exp = *expp;

  /* Pass two: perform any resolution on principal operator.  */
  switch (op)
    {
    default:
      break;

    case OP_VAR_VALUE:
      if (SYMBOL_DOMAIN (exp->elts[pc + 2].symbol) == UNDEF_DOMAIN)
        {
          struct ada_symbol_info *candidates;
          int n_candidates;

          n_candidates =
            ada_lookup_symbol_list (SYMBOL_LINKAGE_NAME
                                    (exp->elts[pc + 2].symbol),
                                    exp->elts[pc + 1].block, VAR_DOMAIN,
                                    &candidates);

          if (n_candidates > 1)
            {
              /* Types tend to get re-introduced locally, so if there
                 are any local symbols that are not types, first filter
                 out all types.  */
              int j;
              for (j = 0; j < n_candidates; j += 1)
                switch (SYMBOL_CLASS (candidates[j].sym))
                  {
                  case LOC_REGISTER:
                  case LOC_ARG:
                  case LOC_REF_ARG:
                  case LOC_REGPARM:
                  case LOC_REGPARM_ADDR:
                  case LOC_LOCAL:
                  case LOC_LOCAL_ARG:
                  case LOC_BASEREG:
                  case LOC_BASEREG_ARG:
                  case LOC_COMPUTED:
                  case LOC_COMPUTED_ARG:
                    goto FoundNonType;
                  default:
                    break;
                  }
            FoundNonType:
              if (j < n_candidates)
                {
                  j = 0;
                  while (j < n_candidates)
                    {
                      if (SYMBOL_CLASS (candidates[j].sym) == LOC_TYPEDEF)
                        {
                          candidates[j] = candidates[n_candidates - 1];
                          n_candidates -= 1;
                        }
                      else
                        j += 1;
                    }
                }
            }

          if (n_candidates == 0)
            error (_("No definition found for %s"),
                   SYMBOL_PRINT_NAME (exp->elts[pc + 2].symbol));
          else if (n_candidates == 1)
            i = 0;
          else if (deprocedure_p
                   && !is_nonfunction (candidates, n_candidates))
            {
              i = ada_resolve_function
                (candidates, n_candidates, NULL, 0,
                 SYMBOL_LINKAGE_NAME (exp->elts[pc + 2].symbol),
                 context_type);
              if (i < 0)
                error (_("Could not find a match for %s"),
                       SYMBOL_PRINT_NAME (exp->elts[pc + 2].symbol));
            }
          else
            {
              printf_filtered (_("Multiple matches for %s\n"),
                               SYMBOL_PRINT_NAME (exp->elts[pc + 2].symbol));
              user_select_syms (candidates, n_candidates, 1);
              i = 0;
            }

          exp->elts[pc + 1].block = candidates[i].block;
          exp->elts[pc + 2].symbol = candidates[i].sym;
          if (innermost_block == NULL
              || contained_in (candidates[i].block, innermost_block))
            innermost_block = candidates[i].block;
        }

      if (deprocedure_p
          && (TYPE_CODE (SYMBOL_TYPE (exp->elts[pc + 2].symbol))
              == TYPE_CODE_FUNC))
        {
          replace_operator_with_call (expp, pc, 0, 0,
                                      exp->elts[pc + 2].symbol,
                                      exp->elts[pc + 1].block);
          exp = *expp;
        }
      break;

    case OP_FUNCALL:
      {
        if (exp->elts[pc + 3].opcode == OP_VAR_VALUE
            && SYMBOL_DOMAIN (exp->elts[pc + 5].symbol) == UNDEF_DOMAIN)
          {
            struct ada_symbol_info *candidates;
            int n_candidates;

            n_candidates =
              ada_lookup_symbol_list (SYMBOL_LINKAGE_NAME
                                      (exp->elts[pc + 5].symbol),
                                      exp->elts[pc + 4].block, VAR_DOMAIN,
                                      &candidates);
            if (n_candidates == 1)
              i = 0;
            else
              {
                i = ada_resolve_function
                  (candidates, n_candidates,
                   argvec, nargs,
                   SYMBOL_LINKAGE_NAME (exp->elts[pc + 5].symbol),
                   context_type);
                if (i < 0)
                  error (_("Could not find a match for %s"),
                         SYMBOL_PRINT_NAME (exp->elts[pc + 5].symbol));
              }

            exp->elts[pc + 4].block = candidates[i].block;
            exp->elts[pc + 5].symbol = candidates[i].sym;
            if (innermost_block == NULL
                || contained_in (candidates[i].block, innermost_block))
              innermost_block = candidates[i].block;
          }
      }
      break;
    case BINOP_ADD:
    case BINOP_SUB:
    case BINOP_MUL:
    case BINOP_DIV:
    case BINOP_REM:
    case BINOP_MOD:
    case BINOP_CONCAT:
    case BINOP_BITWISE_AND:
    case BINOP_BITWISE_IOR:
    case BINOP_BITWISE_XOR:
    case BINOP_EQUAL:
    case BINOP_NOTEQUAL:
    case BINOP_LESS:
    case BINOP_GTR:
    case BINOP_LEQ:
    case BINOP_GEQ:
    case BINOP_EXP:
    case UNOP_NEG:
    case UNOP_PLUS:
    case UNOP_LOGICAL_NOT:
    case UNOP_ABS:
      if (possible_user_operator_p (op, argvec))
        {
          struct ada_symbol_info *candidates;
          int n_candidates;

          n_candidates =
            ada_lookup_symbol_list (ada_encode (ada_decoded_op_name (op)),
                                    (struct block *) NULL, VAR_DOMAIN,
                                    &candidates);
          i = ada_resolve_function (candidates, n_candidates, argvec, nargs,
                                    ada_decoded_op_name (op), NULL);
          if (i < 0)
            break;

          replace_operator_with_call (expp, pc, nargs, 1,
                                      candidates[i].sym, candidates[i].block);
          exp = *expp;
        }
      break;

    case OP_TYPE:
      return NULL;
    }

  *pos = pc;
  return evaluate_subexp_type (exp, pos);
}

/* Return non-zero if formal type FTYPE matches actual type ATYPE.  If
   MAY_DEREF is non-zero, the formal may be a pointer and the actual
   a non-pointer.   A type of 'void' (which is never a valid expression type)
   by convention matches anything. */
/* The term "match" here is rather loose.  The match is heuristic and
   liberal.  FIXME: TOO liberal, in fact.  */

static int
ada_type_match (struct type *ftype, struct type *atype, int may_deref)
{
  ftype = ada_check_typedef (ftype);
  atype = ada_check_typedef (atype);

  if (TYPE_CODE (ftype) == TYPE_CODE_REF)
    ftype = TYPE_TARGET_TYPE (ftype);
  if (TYPE_CODE (atype) == TYPE_CODE_REF)
    atype = TYPE_TARGET_TYPE (atype);

  if (TYPE_CODE (ftype) == TYPE_CODE_VOID
      || TYPE_CODE (atype) == TYPE_CODE_VOID)
    return 1;

  switch (TYPE_CODE (ftype))
    {
    default:
      return 1;
    case TYPE_CODE_PTR:
      if (TYPE_CODE (atype) == TYPE_CODE_PTR)
        return ada_type_match (TYPE_TARGET_TYPE (ftype),
                               TYPE_TARGET_TYPE (atype), 0);
      else
        return (may_deref
                && ada_type_match (TYPE_TARGET_TYPE (ftype), atype, 0));
    case TYPE_CODE_INT:
    case TYPE_CODE_ENUM:
    case TYPE_CODE_RANGE:
      switch (TYPE_CODE (atype))
        {
        case TYPE_CODE_INT:
        case TYPE_CODE_ENUM:
        case TYPE_CODE_RANGE:
          return 1;
        default:
          return 0;
        }

    case TYPE_CODE_ARRAY:
      return (TYPE_CODE (atype) == TYPE_CODE_ARRAY
              || ada_is_array_descriptor_type (atype));

    case TYPE_CODE_STRUCT:
      if (ada_is_array_descriptor_type (ftype))
        return (TYPE_CODE (atype) == TYPE_CODE_ARRAY
                || ada_is_array_descriptor_type (atype));
      else
        return (TYPE_CODE (atype) == TYPE_CODE_STRUCT
                && !ada_is_array_descriptor_type (atype));

    case TYPE_CODE_UNION:
    case TYPE_CODE_FLT:
      return (TYPE_CODE (atype) == TYPE_CODE (ftype));
    }
}

/* Return non-zero if the formals of FUNC "sufficiently match" the
   vector of actual argument types ACTUALS of size N_ACTUALS.  FUNC
   may also be an enumeral, in which case it is treated as a 0-
   argument function.  */

static int
ada_args_match (struct symbol *func, struct value **actuals, int n_actuals)
{
  int i;
  struct type *func_type = SYMBOL_TYPE (func);

  if (SYMBOL_CLASS (func) == LOC_CONST
      && TYPE_CODE (func_type) == TYPE_CODE_ENUM)
    return (n_actuals == 0);
  else if (func_type == NULL || TYPE_CODE (func_type) != TYPE_CODE_FUNC)
    return 0;

  if (TYPE_NFIELDS (func_type) != n_actuals)
    return 0;

  for (i = 0; i < n_actuals; i += 1)
    {
      if (actuals[i] == NULL)
        return 0;
      else
        {
          struct type *ftype = ada_check_typedef (TYPE_FIELD_TYPE (func_type, i));
          struct type *atype = ada_check_typedef (value_type (actuals[i]));

          if (!ada_type_match (ftype, atype, 1))
            return 0;
        }
    }
  return 1;
}

/* False iff function type FUNC_TYPE definitely does not produce a value
   compatible with type CONTEXT_TYPE.  Conservatively returns 1 if
   FUNC_TYPE is not a valid function type with a non-null return type
   or an enumerated type.  A null CONTEXT_TYPE indicates any non-void type.  */

static int
return_match (struct type *func_type, struct type *context_type)
{
  struct type *return_type;

  if (func_type == NULL)
    return 1;

  if (TYPE_CODE (func_type) == TYPE_CODE_FUNC)
    return_type = base_type (TYPE_TARGET_TYPE (func_type));
  else
    return_type = base_type (func_type);
  if (return_type == NULL)
    return 1;

  context_type = base_type (context_type);

  if (TYPE_CODE (return_type) == TYPE_CODE_ENUM)
    return context_type == NULL || return_type == context_type;
  else if (context_type == NULL)
    return TYPE_CODE (return_type) != TYPE_CODE_VOID;
  else
    return TYPE_CODE (return_type) == TYPE_CODE (context_type);
}


/* Returns the index in SYMS[0..NSYMS-1] that contains  the symbol for the
   function (if any) that matches the types of the NARGS arguments in
   ARGS.  If CONTEXT_TYPE is non-null and there is at least one match
   that returns that type, then eliminate matches that don't.  If
   CONTEXT_TYPE is void and there is at least one match that does not
   return void, eliminate all matches that do.

   Asks the user if there is more than one match remaining.  Returns -1
   if there is no such symbol or none is selected.  NAME is used
   solely for messages.  May re-arrange and modify SYMS in
   the process; the index returned is for the modified vector.  */

static int
ada_resolve_function (struct ada_symbol_info syms[],
                      int nsyms, struct value **args, int nargs,
                      const char *name, struct type *context_type)
{
  int k;
  int m;                        /* Number of hits */
  struct type *fallback;
  struct type *return_type;

  return_type = context_type;
  if (context_type == NULL)
    fallback = builtin_type_void;
  else
    fallback = NULL;

  m = 0;
  while (1)
    {
      for (k = 0; k < nsyms; k += 1)
        {
          struct type *type = ada_check_typedef (SYMBOL_TYPE (syms[k].sym));

          if (ada_args_match (syms[k].sym, args, nargs)
              && return_match (type, return_type))
            {
              syms[m] = syms[k];
              m += 1;
            }
        }
      if (m > 0 || return_type == fallback)
        break;
      else
        return_type = fallback;
    }

  if (m == 0)
    return -1;
  else if (m > 1)
    {
      printf_filtered (_("Multiple matches for %s\n"), name);
      user_select_syms (syms, m, 1);
      return 0;
    }
  return 0;
}

/* Returns true (non-zero) iff decoded name N0 should appear before N1
   in a listing of choices during disambiguation (see sort_choices, below).
   The idea is that overloadings of a subprogram name from the
   same package should sort in their source order.  We settle for ordering
   such symbols by their trailing number (__N  or $N).  */

static int
encoded_ordered_before (char *N0, char *N1)
{
  if (N1 == NULL)
    return 0;
  else if (N0 == NULL)
    return 1;
  else
    {
      int k0, k1;
      for (k0 = strlen (N0) - 1; k0 > 0 && isdigit (N0[k0]); k0 -= 1)
        ;
      for (k1 = strlen (N1) - 1; k1 > 0 && isdigit (N1[k1]); k1 -= 1)
        ;
      if ((N0[k0] == '_' || N0[k0] == '$') && N0[k0 + 1] != '\000'
          && (N1[k1] == '_' || N1[k1] == '$') && N1[k1 + 1] != '\000')
        {
          int n0, n1;
          n0 = k0;
          while (N0[n0] == '_' && n0 > 0 && N0[n0 - 1] == '_')
            n0 -= 1;
          n1 = k1;
          while (N1[n1] == '_' && n1 > 0 && N1[n1 - 1] == '_')
            n1 -= 1;
          if (n0 == n1 && strncmp (N0, N1, n0) == 0)
            return (atoi (N0 + k0 + 1) < atoi (N1 + k1 + 1));
        }
      return (strcmp (N0, N1) < 0);
    }
}

/* Sort SYMS[0..NSYMS-1] to put the choices in a canonical order by the
   encoded names.  */

static void
sort_choices (struct ada_symbol_info syms[], int nsyms)
{
  int i;
  for (i = 1; i < nsyms; i += 1)
    {
      struct ada_symbol_info sym = syms[i];
      int j;

      for (j = i - 1; j >= 0; j -= 1)
        {
          if (encoded_ordered_before (SYMBOL_LINKAGE_NAME (syms[j].sym),
                                      SYMBOL_LINKAGE_NAME (sym.sym)))
            break;
          syms[j + 1] = syms[j];
        }
      syms[j + 1] = sym;
    }
}

/* Given a list of NSYMS symbols in SYMS, select up to MAX_RESULTS>0 
   by asking the user (if necessary), returning the number selected, 
   and setting the first elements of SYMS items.  Error if no symbols
   selected.  */

/* NOTE: Adapted from decode_line_2 in symtab.c, with which it ought
   to be re-integrated one of these days.  */

int
user_select_syms (struct ada_symbol_info *syms, int nsyms, int max_results)
{
  int i;
  int *chosen = (int *) alloca (sizeof (int) * nsyms);
  int n_chosen;
  int first_choice = (max_results == 1) ? 1 : 2;

  if (max_results < 1)
    error (_("Request to select 0 symbols!"));
  if (nsyms <= 1)
    return nsyms;

  printf_unfiltered (_("[0] cancel\n"));
  if (max_results > 1)
    printf_unfiltered (_("[1] all\n"));

  sort_choices (syms, nsyms);

  for (i = 0; i < nsyms; i += 1)
    {
      if (syms[i].sym == NULL)
        continue;

      if (SYMBOL_CLASS (syms[i].sym) == LOC_BLOCK)
        {
          struct symtab_and_line sal =
            find_function_start_sal (syms[i].sym, 1);
	  if (sal.symtab == NULL)
	    printf_unfiltered (_("[%d] %s at <no source file available>:%d\n"),
			       i + first_choice,
			       SYMBOL_PRINT_NAME (syms[i].sym),
			       sal.line);
	  else
	    printf_unfiltered (_("[%d] %s at %s:%d\n"), i + first_choice,
			       SYMBOL_PRINT_NAME (syms[i].sym),
			       sal.symtab->filename, sal.line);
          continue;
        }
      else
        {
          int is_enumeral =
            (SYMBOL_CLASS (syms[i].sym) == LOC_CONST
             && SYMBOL_TYPE (syms[i].sym) != NULL
             && TYPE_CODE (SYMBOL_TYPE (syms[i].sym)) == TYPE_CODE_ENUM);
          struct symtab *symtab = symtab_for_sym (syms[i].sym);

          if (SYMBOL_LINE (syms[i].sym) != 0 && symtab != NULL)
            printf_unfiltered (_("[%d] %s at %s:%d\n"),
                               i + first_choice,
                               SYMBOL_PRINT_NAME (syms[i].sym),
                               symtab->filename, SYMBOL_LINE (syms[i].sym));
          else if (is_enumeral
                   && TYPE_NAME (SYMBOL_TYPE (syms[i].sym)) != NULL)
            {
              printf_unfiltered (("[%d] "), i + first_choice);
              ada_print_type (SYMBOL_TYPE (syms[i].sym), NULL,
                              gdb_stdout, -1, 0);
              printf_unfiltered (_("'(%s) (enumeral)\n"),
                                 SYMBOL_PRINT_NAME (syms[i].sym));
            }
          else if (symtab != NULL)
            printf_unfiltered (is_enumeral
                               ? _("[%d] %s in %s (enumeral)\n")
                               : _("[%d] %s at %s:?\n"),
                               i + first_choice,
                               SYMBOL_PRINT_NAME (syms[i].sym),
                               symtab->filename);
          else
            printf_unfiltered (is_enumeral
                               ? _("[%d] %s (enumeral)\n")
                               : _("[%d] %s at ?\n"),
                               i + first_choice,
                               SYMBOL_PRINT_NAME (syms[i].sym));
        }
    }

  n_chosen = get_selections (chosen, nsyms, max_results, max_results > 1,
                             "overload-choice");

  for (i = 0; i < n_chosen; i += 1)
    syms[i] = syms[chosen[i]];

  return n_chosen;
}

/* Read and validate a set of numeric choices from the user in the
   range 0 .. N_CHOICES-1.  Place the results in increasing
   order in CHOICES[0 .. N-1], and return N.

   The user types choices as a sequence of numbers on one line
   separated by blanks, encoding them as follows:

     + A choice of 0 means to cancel the selection, throwing an error.
     + If IS_ALL_CHOICE, a choice of 1 selects the entire set 0 .. N_CHOICES-1.
     + The user chooses k by typing k+IS_ALL_CHOICE+1.

   The user is not allowed to choose more than MAX_RESULTS values.

   ANNOTATION_SUFFIX, if present, is used to annotate the input
   prompts (for use with the -f switch).  */

int
get_selections (int *choices, int n_choices, int max_results,
                int is_all_choice, char *annotation_suffix)
{
  char *args;
  const char *prompt;
  int n_chosen;
  int first_choice = is_all_choice ? 2 : 1;

  prompt = getenv ("PS2");
  if (prompt == NULL)
    prompt = ">";

  printf_unfiltered (("%s "), prompt);
  gdb_flush (gdb_stdout);

  args = command_line_input ((char *) NULL, 0, annotation_suffix);

  if (args == NULL)
    error_no_arg (_("one or more choice numbers"));

  n_chosen = 0;

  /* Set choices[0 .. n_chosen-1] to the users' choices in ascending
     order, as given in args.  Choices are validated.  */
  while (1)
    {
      char *args2;
      int choice, j;

      while (isspace (*args))
        args += 1;
      if (*args == '\0' && n_chosen == 0)
        error_no_arg (_("one or more choice numbers"));
      else if (*args == '\0')
        break;

      choice = strtol (args, &args2, 10);
      if (args == args2 || choice < 0
          || choice > n_choices + first_choice - 1)
        error (_("Argument must be choice number"));
      args = args2;

      if (choice == 0)
        error (_("cancelled"));

      if (choice < first_choice)
        {
          n_chosen = n_choices;
          for (j = 0; j < n_choices; j += 1)
            choices[j] = j;
          break;
        }
      choice -= first_choice;

      for (j = n_chosen - 1; j >= 0 && choice < choices[j]; j -= 1)
        {
        }

      if (j < 0 || choice != choices[j])
        {
          int k;
          for (k = n_chosen - 1; k > j; k -= 1)
            choices[k + 1] = choices[k];
          choices[j + 1] = choice;
          n_chosen += 1;
        }
    }

  if (n_chosen > max_results)
    error (_("Select no more than %d of the above"), max_results);

  return n_chosen;
}

/* Replace the operator of length OPLEN at position PC in *EXPP with a call
   on the function identified by SYM and BLOCK, and taking NARGS
   arguments.  Update *EXPP as needed to hold more space.  */

static void
replace_operator_with_call (struct expression **expp, int pc, int nargs,
                            int oplen, struct symbol *sym,
                            struct block *block)
{
  /* A new expression, with 6 more elements (3 for funcall, 4 for function
     symbol, -oplen for operator being replaced).  */
  struct expression *newexp = (struct expression *)
    xmalloc (sizeof (struct expression)
             + EXP_ELEM_TO_BYTES ((*expp)->nelts + 7 - oplen));
  struct expression *exp = *expp;

  newexp->nelts = exp->nelts + 7 - oplen;
  newexp->language_defn = exp->language_defn;
  memcpy (newexp->elts, exp->elts, EXP_ELEM_TO_BYTES (pc));
  memcpy (newexp->elts + pc + 7, exp->elts + pc + oplen,
          EXP_ELEM_TO_BYTES (exp->nelts - pc - oplen));

  newexp->elts[pc].opcode = newexp->elts[pc + 2].opcode = OP_FUNCALL;
  newexp->elts[pc + 1].longconst = (LONGEST) nargs;

  newexp->elts[pc + 3].opcode = newexp->elts[pc + 6].opcode = OP_VAR_VALUE;
  newexp->elts[pc + 4].block = block;
  newexp->elts[pc + 5].symbol = sym;

  *expp = newexp;
  xfree (exp);
}

/* Type-class predicates */

/* True iff TYPE is numeric (i.e., an INT, RANGE (of numeric type),
   or FLOAT).  */

static int
numeric_type_p (struct type *type)
{
  if (type == NULL)
    return 0;
  else
    {
      switch (TYPE_CODE (type))
        {
        case TYPE_CODE_INT:
        case TYPE_CODE_FLT:
          return 1;
        case TYPE_CODE_RANGE:
          return (type == TYPE_TARGET_TYPE (type)
                  || numeric_type_p (TYPE_TARGET_TYPE (type)));
        default:
          return 0;
        }
    }
}

/* True iff TYPE is integral (an INT or RANGE of INTs).  */

static int
integer_type_p (struct type *type)
{
  if (type == NULL)
    return 0;
  else
    {
      switch (TYPE_CODE (type))
        {
        case TYPE_CODE_INT:
          return 1;
        case TYPE_CODE_RANGE:
          return (type == TYPE_TARGET_TYPE (type)
                  || integer_type_p (TYPE_TARGET_TYPE (type)));
        default:
          return 0;
        }
    }
}

/* True iff TYPE is scalar (INT, RANGE, FLOAT, ENUM).  */

static int
scalar_type_p (struct type *type)
{
  if (type == NULL)
    return 0;
  else
    {
      switch (TYPE_CODE (type))
        {
        case TYPE_CODE_INT:
        case TYPE_CODE_RANGE:
        case TYPE_CODE_ENUM:
        case TYPE_CODE_FLT:
          return 1;
        default:
          return 0;
        }
    }
}

/* True iff TYPE is discrete (INT, RANGE, ENUM).  */

static int
discrete_type_p (struct type *type)
{
  if (type == NULL)
    return 0;
  else
    {
      switch (TYPE_CODE (type))
        {
        case TYPE_CODE_INT:
        case TYPE_CODE_RANGE:
        case TYPE_CODE_ENUM:
          return 1;
        default:
          return 0;
        }
    }
}

/* Returns non-zero if OP with operands in the vector ARGS could be
   a user-defined function.  Errs on the side of pre-defined operators
   (i.e., result 0).  */

static int
possible_user_operator_p (enum exp_opcode op, struct value *args[])
{
  struct type *type0 =
    (args[0] == NULL) ? NULL : ada_check_typedef (value_type (args[0]));
  struct type *type1 =
    (args[1] == NULL) ? NULL : ada_check_typedef (value_type (args[1]));

  if (type0 == NULL)
    return 0;

  switch (op)
    {
    default:
      return 0;

    case BINOP_ADD:
    case BINOP_SUB:
    case BINOP_MUL:
    case BINOP_DIV:
      return (!(numeric_type_p (type0) && numeric_type_p (type1)));

    case BINOP_REM:
    case BINOP_MOD:
    case BINOP_BITWISE_AND:
    case BINOP_BITWISE_IOR:
    case BINOP_BITWISE_XOR:
      return (!(integer_type_p (type0) && integer_type_p (type1)));

    case BINOP_EQUAL:
    case BINOP_NOTEQUAL:
    case BINOP_LESS:
    case BINOP_GTR:
    case BINOP_LEQ:
    case BINOP_GEQ:
      return (!(scalar_type_p (type0) && scalar_type_p (type1)));

    case BINOP_CONCAT:
      return !ada_is_array_type (type0) || !ada_is_array_type (type1);

    case BINOP_EXP:
      return (!(numeric_type_p (type0) && integer_type_p (type1)));

    case UNOP_NEG:
    case UNOP_PLUS:
    case UNOP_LOGICAL_NOT:
    case UNOP_ABS:
      return (!numeric_type_p (type0));

    }
}

                                /* Renaming */

/* NOTE: In the following, we assume that a renaming type's name may
   have an ___XD suffix.  It would be nice if this went away at some
   point.  */

/* If TYPE encodes a renaming, returns the renaming suffix, which
   is XR for an object renaming, XRP for a procedure renaming, XRE for
   an exception renaming, and XRS for a subprogram renaming.  Returns
   NULL if NAME encodes none of these.  */

const char *
ada_renaming_type (struct type *type)
{
  if (type != NULL && TYPE_CODE (type) == TYPE_CODE_ENUM)
    {
      const char *name = type_name_no_tag (type);
      const char *suffix = (name == NULL) ? NULL : strstr (name, "___XR");
      if (suffix == NULL
          || (suffix[5] != '\000' && strchr ("PES_", suffix[5]) == NULL))
        return NULL;
      else
        return suffix + 3;
    }
  else
    return NULL;
}

/* Return non-zero iff SYM encodes an object renaming.  */

int
ada_is_object_renaming (struct symbol *sym)
{
  const char *renaming_type = ada_renaming_type (SYMBOL_TYPE (sym));
  return renaming_type != NULL
    && (renaming_type[2] == '\0' || renaming_type[2] == '_');
}

/* Assuming that SYM encodes a non-object renaming, returns the original
   name of the renamed entity.  The name is good until the end of
   parsing.  */

char *
ada_simple_renamed_entity (struct symbol *sym)
{
  struct type *type;
  const char *raw_name;
  int len;
  char *result;

  type = SYMBOL_TYPE (sym);
  if (type == NULL || TYPE_NFIELDS (type) < 1)
    error (_("Improperly encoded renaming."));

  raw_name = TYPE_FIELD_NAME (type, 0);
  len = (raw_name == NULL ? 0 : strlen (raw_name)) - 5;
  if (len <= 0)
    error (_("Improperly encoded renaming."));

  result = xmalloc (len + 1);
  strncpy (result, raw_name, len);
  result[len] = '\000';
  return result;
}



                                /* Evaluation: Function Calls */

/* Return an lvalue containing the value VAL.  This is the identity on
   lvalues, and otherwise has the side-effect of pushing a copy of VAL 
   on the stack, using and updating *SP as the stack pointer, and 
   returning an lvalue whose VALUE_ADDRESS points to the copy.  */

static struct value *
ensure_lval (struct value *val, CORE_ADDR *sp)
{
  if (! VALUE_LVAL (val))
    {
      int len = TYPE_LENGTH (ada_check_typedef (value_type (val)));

      /* The following is taken from the structure-return code in
	 call_function_by_hand. FIXME: Therefore, some refactoring seems 
	 indicated. */
      if (gdbarch_inner_than (current_gdbarch, 1, 2))
	{
	  /* Stack grows downward.  Align SP and VALUE_ADDRESS (val) after
	     reserving sufficient space. */
	  *sp -= len;
	  if (gdbarch_frame_align_p (current_gdbarch))
	    *sp = gdbarch_frame_align (current_gdbarch, *sp);
	  VALUE_ADDRESS (val) = *sp;
	}
      else
	{
	  /* Stack grows upward.  Align the frame, allocate space, and
	     then again, re-align the frame. */
	  if (gdbarch_frame_align_p (current_gdbarch))
	    *sp = gdbarch_frame_align (current_gdbarch, *sp);
	  VALUE_ADDRESS (val) = *sp;
	  *sp += len;
	  if (gdbarch_frame_align_p (current_gdbarch))
	    *sp = gdbarch_frame_align (current_gdbarch, *sp);
	}

      write_memory (VALUE_ADDRESS (val), value_contents_raw (val), len);
    }

  return val;
}

/* Return the value ACTUAL, converted to be an appropriate value for a
   formal of type FORMAL_TYPE.  Use *SP as a stack pointer for
   allocating any necessary descriptors (fat pointers), or copies of
   values not residing in memory, updating it as needed.  */

static struct value *
convert_actual (struct value *actual, struct type *formal_type0,
                CORE_ADDR *sp)
{
  struct type *actual_type = ada_check_typedef (value_type (actual));
  struct type *formal_type = ada_check_typedef (formal_type0);
  struct type *formal_target =
    TYPE_CODE (formal_type) == TYPE_CODE_PTR
    ? ada_check_typedef (TYPE_TARGET_TYPE (formal_type)) : formal_type;
  struct type *actual_target =
    TYPE_CODE (actual_type) == TYPE_CODE_PTR
    ? ada_check_typedef (TYPE_TARGET_TYPE (actual_type)) : actual_type;

  if (ada_is_array_descriptor_type (formal_target)
      && TYPE_CODE (actual_target) == TYPE_CODE_ARRAY)
    return make_array_descriptor (formal_type, actual, sp);
  else if (TYPE_CODE (formal_type) == TYPE_CODE_PTR)
    {
      if (TYPE_CODE (formal_target) == TYPE_CODE_ARRAY
          && ada_is_array_descriptor_type (actual_target))
        return desc_data (actual);
      else if (TYPE_CODE (actual_type) != TYPE_CODE_PTR)
        {
          if (VALUE_LVAL (actual) != lval_memory)
            {
              struct value *val;
              actual_type = ada_check_typedef (value_type (actual));
              val = allocate_value (actual_type);
              memcpy ((char *) value_contents_raw (val),
                      (char *) value_contents (actual),
                      TYPE_LENGTH (actual_type));
              actual = ensure_lval (val, sp);
            }
          return value_addr (actual);
        }
    }
  else if (TYPE_CODE (actual_type) == TYPE_CODE_PTR)
    return ada_value_ind (actual);

  return actual;
}


/* Push a descriptor of type TYPE for array value ARR on the stack at
   *SP, updating *SP to reflect the new descriptor.  Return either
   an lvalue representing the new descriptor, or (if TYPE is a pointer-
   to-descriptor type rather than a descriptor type), a struct value *
   representing a pointer to this descriptor.  */

static struct value *
make_array_descriptor (struct type *type, struct value *arr, CORE_ADDR *sp)
{
  struct type *bounds_type = desc_bounds_type (type);
  struct type *desc_type = desc_base_type (type);
  struct value *descriptor = allocate_value (desc_type);
  struct value *bounds = allocate_value (bounds_type);
  int i;

  for (i = ada_array_arity (ada_check_typedef (value_type (arr))); i > 0; i -= 1)
    {
      modify_general_field (value_contents_writeable (bounds),
                            value_as_long (ada_array_bound (arr, i, 0)),
                            desc_bound_bitpos (bounds_type, i, 0),
                            desc_bound_bitsize (bounds_type, i, 0));
      modify_general_field (value_contents_writeable (bounds),
                            value_as_long (ada_array_bound (arr, i, 1)),
                            desc_bound_bitpos (bounds_type, i, 1),
                            desc_bound_bitsize (bounds_type, i, 1));
    }

  bounds = ensure_lval (bounds, sp);

  modify_general_field (value_contents_writeable (descriptor),
                        VALUE_ADDRESS (ensure_lval (arr, sp)),
                        fat_pntr_data_bitpos (desc_type),
                        fat_pntr_data_bitsize (desc_type));

  modify_general_field (value_contents_writeable (descriptor),
                        VALUE_ADDRESS (bounds),
                        fat_pntr_bounds_bitpos (desc_type),
                        fat_pntr_bounds_bitsize (desc_type));

  descriptor = ensure_lval (descriptor, sp);

  if (TYPE_CODE (type) == TYPE_CODE_PTR)
    return value_addr (descriptor);
  else
    return descriptor;
}


/* Assuming a dummy frame has been established on the target, perform any
   conversions needed for calling function FUNC on the NARGS actual
   parameters in ARGS, other than standard C conversions.  Does
   nothing if FUNC does not have Ada-style prototype data, or if NARGS
   does not match the number of arguments expected.  Use *SP as a
   stack pointer for additional data that must be pushed, updating its
   value as needed.  */

void
ada_convert_actuals (struct value *func, int nargs, struct value *args[],
                     CORE_ADDR *sp)
{
  int i;

  if (TYPE_NFIELDS (value_type (func)) == 0
      || nargs != TYPE_NFIELDS (value_type (func)))
    return;

  for (i = 0; i < nargs; i += 1)
    args[i] =
      convert_actual (args[i], TYPE_FIELD_TYPE (value_type (func), i), sp);
}

/* Dummy definitions for an experimental caching module that is not
 * used in the public sources. */

static int
lookup_cached_symbol (const char *name, domain_enum namespace,
                      struct symbol **sym, struct block **block,
                      struct symtab **symtab)
{
  return 0;
}

static void
cache_symbol (const char *name, domain_enum namespace, struct symbol *sym,
              struct block *block, struct symtab *symtab)
{
}

                                /* Symbol Lookup */

/* Return the result of a standard (literal, C-like) lookup of NAME in
   given DOMAIN, visible from lexical block BLOCK.  */

static struct symbol *
standard_lookup (const char *name, const struct block *block,
                 domain_enum domain)
{
  struct symbol *sym;
  struct symtab *symtab;

  if (lookup_cached_symbol (name, domain, &sym, NULL, NULL))
    return sym;
  sym =
    lookup_symbol_in_language (name, block, domain, language_c, 0, &symtab);
  cache_symbol (name, domain, sym, block_found, symtab);
  return sym;
}


/* Non-zero iff there is at least one non-function/non-enumeral symbol
   in the symbol fields of SYMS[0..N-1].  We treat enumerals as functions, 
   since they contend in overloading in the same way.  */
static int
is_nonfunction (struct ada_symbol_info syms[], int n)
{
  int i;

  for (i = 0; i < n; i += 1)
    if (TYPE_CODE (SYMBOL_TYPE (syms[i].sym)) != TYPE_CODE_FUNC
        && (TYPE_CODE (SYMBOL_TYPE (syms[i].sym)) != TYPE_CODE_ENUM
            || SYMBOL_CLASS (syms[i].sym) != LOC_CONST))
      return 1;

  return 0;
}

/* If true (non-zero), then TYPE0 and TYPE1 represent equivalent
   struct types.  Otherwise, they may not.  */

static int
equiv_types (struct type *type0, struct type *type1)
{
  if (type0 == type1)
    return 1;
  if (type0 == NULL || type1 == NULL
      || TYPE_CODE (type0) != TYPE_CODE (type1))
    return 0;
  if ((TYPE_CODE (type0) == TYPE_CODE_STRUCT
       || TYPE_CODE (type0) == TYPE_CODE_ENUM)
      && ada_type_name (type0) != NULL && ada_type_name (type1) != NULL
      && strcmp (ada_type_name (type0), ada_type_name (type1)) == 0)
    return 1;

  return 0;
}

/* True iff SYM0 represents the same entity as SYM1, or one that is
   no more defined than that of SYM1.  */

static int
lesseq_defined_than (struct symbol *sym0, struct symbol *sym1)
{
  if (sym0 == sym1)
    return 1;
  if (SYMBOL_DOMAIN (sym0) != SYMBOL_DOMAIN (sym1)
      || SYMBOL_CLASS (sym0) != SYMBOL_CLASS (sym1))
    return 0;

  switch (SYMBOL_CLASS (sym0))
    {
    case LOC_UNDEF:
      return 1;
    case LOC_TYPEDEF:
      {
        struct type *type0 = SYMBOL_TYPE (sym0);
        struct type *type1 = SYMBOL_TYPE (sym1);
        char *name0 = SYMBOL_LINKAGE_NAME (sym0);
        char *name1 = SYMBOL_LINKAGE_NAME (sym1);
        int len0 = strlen (name0);
        return
          TYPE_CODE (type0) == TYPE_CODE (type1)
          && (equiv_types (type0, type1)
              || (len0 < strlen (name1) && strncmp (name0, name1, len0) == 0
                  && strncmp (name1 + len0, "___XV", 5) == 0));
      }
    case LOC_CONST:
      return SYMBOL_VALUE (sym0) == SYMBOL_VALUE (sym1)
        && equiv_types (SYMBOL_TYPE (sym0), SYMBOL_TYPE (sym1));
    default:
      return 0;
    }
}

/* Append (SYM,BLOCK,SYMTAB) to the end of the array of struct ada_symbol_info
   records in OBSTACKP.  Do nothing if SYM is a duplicate.  */

static void
add_defn_to_vec (struct obstack *obstackp,
                 struct symbol *sym,
                 struct block *block, struct symtab *symtab)
{
  int i;
  size_t tmp;
  struct ada_symbol_info *prevDefns = defns_collected (obstackp, 0);

  /* Do not try to complete stub types, as the debugger is probably
     already scanning all symbols matching a certain name at the
     time when this function is called.  Trying to replace the stub
     type by its associated full type will cause us to restart a scan
     which may lead to an infinite recursion.  Instead, the client
     collecting the matching symbols will end up collecting several
     matches, with at least one of them complete.  It can then filter
     out the stub ones if needed.  */

  for (i = num_defns_collected (obstackp) - 1; i >= 0; i -= 1)
    {
      if (lesseq_defined_than (sym, prevDefns[i].sym))
        return;
      else if (lesseq_defined_than (prevDefns[i].sym, sym))
        {
          prevDefns[i].sym = sym;
          prevDefns[i].block = block;
          prevDefns[i].symtab = symtab;
          return;
        }
    }

  {
    struct ada_symbol_info info;

    info.sym = sym;
    info.block = block;
    info.symtab = symtab;
    obstack_grow (obstackp, &info, sizeof (struct ada_symbol_info));
  }
}

/* Number of ada_symbol_info structures currently collected in 
   current vector in *OBSTACKP.  */

static int
num_defns_collected (struct obstack *obstackp)
{
  return obstack_object_size (obstackp) / sizeof (struct ada_symbol_info);
}

/* Vector of ada_symbol_info structures currently collected in current 
   vector in *OBSTACKP.  If FINISH, close off the vector and return
   its final address.  */

static struct ada_symbol_info *
defns_collected (struct obstack *obstackp, int finish)
{
  if (finish)
    return obstack_finish (obstackp);
  else
    return (struct ada_symbol_info *) obstack_base (obstackp);
}

/* Look, in partial_symtab PST, for symbol NAME in given namespace.
   Check the global symbols if GLOBAL, the static symbols if not.
   Do wild-card match if WILD.  */

static struct partial_symbol *
ada_lookup_partial_symbol (struct partial_symtab *pst, const char *name,
                           int global, domain_enum namespace, int wild)
{
  struct partial_symbol **start;
  int name_len = strlen (name);
  int length = (global ? pst->n_global_syms : pst->n_static_syms);
  int i;

  if (length == 0)
    {
      return (NULL);
    }

  start = (global ?
           pst->objfile->global_psymbols.list + pst->globals_offset :
           pst->objfile->static_psymbols.list + pst->statics_offset);

  if (wild)
    {
      for (i = 0; i < length; i += 1)
        {
          struct partial_symbol *psym = start[i];

          if (SYMBOL_DOMAIN (psym) == namespace
              && wild_match (name, name_len, SYMBOL_LINKAGE_NAME (psym)))
            return psym;
        }
      return NULL;
    }
  else
    {
      if (global)
        {
          int U;
          i = 0;
          U = length - 1;
          while (U - i > 4)
            {
              int M = (U + i) >> 1;
              struct partial_symbol *psym = start[M];
              if (SYMBOL_LINKAGE_NAME (psym)[0] < name[0])
                i = M + 1;
              else if (SYMBOL_LINKAGE_NAME (psym)[0] > name[0])
                U = M - 1;
              else if (strcmp (SYMBOL_LINKAGE_NAME (psym), name) < 0)
                i = M + 1;
              else
                U = M;
            }
        }
      else
        i = 0;

      while (i < length)
        {
          struct partial_symbol *psym = start[i];

          if (SYMBOL_DOMAIN (psym) == namespace)
            {
              int cmp = strncmp (name, SYMBOL_LINKAGE_NAME (psym), name_len);

              if (cmp < 0)
                {
                  if (global)
                    break;
                }
              else if (cmp == 0
                       && is_name_suffix (SYMBOL_LINKAGE_NAME (psym)
                                          + name_len))
                return psym;
            }
          i += 1;
        }

      if (global)
        {
          int U;
          i = 0;
          U = length - 1;
          while (U - i > 4)
            {
              int M = (U + i) >> 1;
              struct partial_symbol *psym = start[M];
              if (SYMBOL_LINKAGE_NAME (psym)[0] < '_')
                i = M + 1;
              else if (SYMBOL_LINKAGE_NAME (psym)[0] > '_')
                U = M - 1;
              else if (strcmp (SYMBOL_LINKAGE_NAME (psym), "_ada_") < 0)
                i = M + 1;
              else
                U = M;
            }
        }
      else
        i = 0;

      while (i < length)
        {
          struct partial_symbol *psym = start[i];

          if (SYMBOL_DOMAIN (psym) == namespace)
            {
              int cmp;

              cmp = (int) '_' - (int) SYMBOL_LINKAGE_NAME (psym)[0];
              if (cmp == 0)
                {
                  cmp = strncmp ("_ada_", SYMBOL_LINKAGE_NAME (psym), 5);
                  if (cmp == 0)
                    cmp = strncmp (name, SYMBOL_LINKAGE_NAME (psym) + 5,
                                   name_len);
                }

              if (cmp < 0)
                {
                  if (global)
                    break;
                }
              else if (cmp == 0
                       && is_name_suffix (SYMBOL_LINKAGE_NAME (psym)
                                          + name_len + 5))
                return psym;
            }
          i += 1;
        }
    }
  return NULL;
}

/* Find a symbol table containing symbol SYM or NULL if none.  */

static struct symtab *
symtab_for_sym (struct symbol *sym)
{
  struct symtab *s;
  struct objfile *objfile;
  struct block *b;
  struct symbol *tmp_sym;
  struct dict_iterator iter;
  int j;

  ALL_PRIMARY_SYMTABS (objfile, s)
  {
    switch (SYMBOL_CLASS (sym))
      {
      case LOC_CONST:
      case LOC_STATIC:
      case LOC_TYPEDEF:
      case LOC_REGISTER:
      case LOC_LABEL:
      case LOC_BLOCK:
      case LOC_CONST_BYTES:
        b = BLOCKVECTOR_BLOCK (BLOCKVECTOR (s), GLOBAL_BLOCK);
        ALL_BLOCK_SYMBOLS (b, iter, tmp_sym) if (sym == tmp_sym)
          return s;
        b = BLOCKVECTOR_BLOCK (BLOCKVECTOR (s), STATIC_BLOCK);
        ALL_BLOCK_SYMBOLS (b, iter, tmp_sym) if (sym == tmp_sym)
          return s;
        break;
      default:
        break;
      }
    switch (SYMBOL_CLASS (sym))
      {
      case LOC_REGISTER:
      case LOC_ARG:
      case LOC_REF_ARG:
      case LOC_REGPARM:
      case LOC_REGPARM_ADDR:
      case LOC_LOCAL:
      case LOC_TYPEDEF:
      case LOC_LOCAL_ARG:
      case LOC_BASEREG:
      case LOC_BASEREG_ARG:
      case LOC_COMPUTED:
      case LOC_COMPUTED_ARG:
        for (j = FIRST_LOCAL_BLOCK;
             j < BLOCKVECTOR_NBLOCKS (BLOCKVECTOR (s)); j += 1)
          {
            b = BLOCKVECTOR_BLOCK (BLOCKVECTOR (s), j);
            ALL_BLOCK_SYMBOLS (b, iter, tmp_sym) if (sym == tmp_sym)
              return s;
          }
        break;
      default:
        break;
      }
  }
  return NULL;
}

/* Return a minimal symbol matching NAME according to Ada decoding
   rules.  Returns NULL if there is no such minimal symbol.  Names 
   prefixed with "standard__" are handled specially: "standard__" is 
   first stripped off, and only static and global symbols are searched.  */

struct minimal_symbol *
ada_lookup_simple_minsym (const char *name)
{
  struct objfile *objfile;
  struct minimal_symbol *msymbol;
  int wild_match;

  if (strncmp (name, "standard__", sizeof ("standard__") - 1) == 0)
    {
      name += sizeof ("standard__") - 1;
      wild_match = 0;
    }
  else
    wild_match = (strstr (name, "__") == NULL);

  ALL_MSYMBOLS (objfile, msymbol)
  {
    if (ada_match_name (SYMBOL_LINKAGE_NAME (msymbol), name, wild_match)
        && MSYMBOL_TYPE (msymbol) != mst_solib_trampoline)
      return msymbol;
  }

  return NULL;
}

/* For all subprograms that statically enclose the subprogram of the
   selected frame, add symbols matching identifier NAME in DOMAIN
   and their blocks to the list of data in OBSTACKP, as for
   ada_add_block_symbols (q.v.).   If WILD, treat as NAME with a
   wildcard prefix.  */

static void
add_symbols_from_enclosing_procs (struct obstack *obstackp,
                                  const char *name, domain_enum namespace,
                                  int wild_match)
{
}

/* True if TYPE is definitely an artificial type supplied to a symbol
   for which no debugging information was given in the symbol file.  */

static int
is_nondebugging_type (struct type *type)
{
  char *name = ada_type_name (type);
  return (name != NULL && strcmp (name, "<variable, no debug info>") == 0);
}

/* Remove any non-debugging symbols in SYMS[0 .. NSYMS-1] that definitely
   duplicate other symbols in the list (The only case I know of where
   this happens is when object files containing stabs-in-ecoff are
   linked with files containing ordinary ecoff debugging symbols (or no
   debugging symbols)).  Modifies SYMS to squeeze out deleted entries.
   Returns the number of items in the modified list.  */

static int
remove_extra_symbols (struct ada_symbol_info *syms, int nsyms)
{
  int i, j;

  i = 0;
  while (i < nsyms)
    {
      if (SYMBOL_LINKAGE_NAME (syms[i].sym) != NULL
          && SYMBOL_CLASS (syms[i].sym) == LOC_STATIC
          && is_nondebugging_type (SYMBOL_TYPE (syms[i].sym)))
        {
          for (j = 0; j < nsyms; j += 1)
            {
              if (i != j
                  && SYMBOL_LINKAGE_NAME (syms[j].sym) != NULL
                  && strcmp (SYMBOL_LINKAGE_NAME (syms[i].sym),
                             SYMBOL_LINKAGE_NAME (syms[j].sym)) == 0
                  && SYMBOL_CLASS (syms[i].sym) == SYMBOL_CLASS (syms[j].sym)
                  && SYMBOL_VALUE_ADDRESS (syms[i].sym)
                  == SYMBOL_VALUE_ADDRESS (syms[j].sym))
                {
                  int k;
                  for (k = i + 1; k < nsyms; k += 1)
                    syms[k - 1] = syms[k];
                  nsyms -= 1;
                  goto NextSymbol;
                }
            }
        }
      i += 1;
    NextSymbol:
      ;
    }
  return nsyms;
}

/* Given a type that corresponds to a renaming entity, use the type name
   to extract the scope (package name or function name, fully qualified,
   and following the GNAT encoding convention) where this renaming has been
   defined.  The string returned needs to be deallocated after use.  */

static char *
xget_renaming_scope (struct type *renaming_type)
{
  /* The renaming types adhere to the following convention:
     <scope>__<rename>___<XR extension>. 
     So, to extract the scope, we search for the "___XR" extension,
     and then backtrack until we find the first "__".  */

  const char *name = type_name_no_tag (renaming_type);
  char *suffix = strstr (name, "___XR");
  char *last;
  int scope_len;
  char *scope;

  /* Now, backtrack a bit until we find the first "__".  Start looking
     at suffix - 3, as the <rename> part is at least one character long.  */

  for (last = suffix - 3; last > name; last--)
    if (last[0] == '_' && last[1] == '_')
      break;

  /* Make a copy of scope and return it.  */

  scope_len = last - name;
  scope = (char *) xmalloc ((scope_len + 1) * sizeof (char));

  strncpy (scope, name, scope_len);
  scope[scope_len] = '\0';

  return scope;
}

/* Return nonzero if NAME corresponds to a package name.  */

static int
is_package_name (const char *name)
{
  /* Here, We take advantage of the fact that no symbols are generated
     for packages, while symbols are generated for each function.
     So the condition for NAME represent a package becomes equivalent
     to NAME not existing in our list of symbols.  There is only one
     small complication with library-level functions (see below).  */

  char *fun_name;

  /* If it is a function that has not been defined at library level,
     then we should be able to look it up in the symbols.  */
  if (standard_lookup (name, NULL, VAR_DOMAIN) != NULL)
    return 0;

  /* Library-level function names start with "_ada_".  See if function
     "_ada_" followed by NAME can be found.  */

  /* Do a quick check that NAME does not contain "__", since library-level
     functions names cannot contain "__" in them.  */
  if (strstr (name, "__") != NULL)
    return 0;

  fun_name = xstrprintf ("_ada_%s", name);

  return (standard_lookup (fun_name, NULL, VAR_DOMAIN) == NULL);
}

/* Return nonzero if SYM corresponds to a renaming entity that is
   visible from FUNCTION_NAME.  */

static int
renaming_is_visible (const struct symbol *sym, char *function_name)
{
  char *scope = xget_renaming_scope (SYMBOL_TYPE (sym));

  make_cleanup (xfree, scope);

  /* If the rename has been defined in a package, then it is visible.  */
  if (is_package_name (scope))
    return 1;

  /* Check that the rename is in the current function scope by checking
     that its name starts with SCOPE.  */

  /* If the function name starts with "_ada_", it means that it is
     a library-level function.  Strip this prefix before doing the
     comparison, as the encoding for the renaming does not contain
     this prefix.  */
  if (strncmp (function_name, "_ada_", 5) == 0)
    function_name += 5;

  return (strncmp (function_name, scope, strlen (scope)) == 0);
}

/* Iterates over the SYMS list and remove any entry that corresponds to
   a renaming entity that is not visible from the function associated
   with CURRENT_BLOCK. 
   
   Rationale:
   GNAT emits a type following a specified encoding for each renaming
   entity.  Unfortunately, STABS currently does not support the definition
   of types that are local to a given lexical block, so all renamings types
   are emitted at library level.  As a consequence, if an application
   contains two renaming entities using the same name, and a user tries to
   print the value of one of these entities, the result of the ada symbol
   lookup will also contain the wrong renaming type.

   This function partially covers for this limitation by attempting to
   remove from the SYMS list renaming symbols that should be visible
   from CURRENT_BLOCK.  However, there does not seem be a 100% reliable
   method with the current information available.  The implementation
   below has a couple of limitations (FIXME: brobecker-2003-05-12):  
   
      - When the user tries to print a rename in a function while there
        is another rename entity defined in a package:  Normally, the
        rename in the function has precedence over the rename in the
        package, so the latter should be removed from the list.  This is
        currently not the case.
        
      - This function will incorrectly remove valid renames if
        the CURRENT_BLOCK corresponds to a function which symbol name
        has been changed by an "Export" pragma.  As a consequence,
        the user will be unable to print such rename entities.  */

static int
remove_out_of_scope_renamings (struct ada_symbol_info *syms,
                               int nsyms, const struct block *current_block)
{
  struct symbol *current_function;
  char *current_function_name;
  int i;

  /* Extract the function name associated to CURRENT_BLOCK.
     Abort if unable to do so.  */

  if (current_block == NULL)
    return nsyms;

  current_function = block_function (current_block);
  if (current_function == NULL)
    return nsyms;

  current_function_name = SYMBOL_LINKAGE_NAME (current_function);
  if (current_function_name == NULL)
    return nsyms;

  /* Check each of the symbols, and remove it from the list if it is
     a type corresponding to a renaming that is out of the scope of
     the current block.  */

  i = 0;
  while (i < nsyms)
    {
      if (ada_is_object_renaming (syms[i].sym)
          && !renaming_is_visible (syms[i].sym, current_function_name))
        {
          int j;
          for (j = i + 1; j < nsyms; j++)
            syms[j - 1] = syms[j];
          nsyms -= 1;
        }
      else
        i += 1;
    }

  return nsyms;
}

/* Find symbols in DOMAIN matching NAME0, in BLOCK0 and enclosing
   scope and in global scopes, returning the number of matches.  Sets
   *RESULTS to point to a vector of (SYM,BLOCK,SYMTAB) triples,
   indicating the symbols found and the blocks and symbol tables (if
   any) in which they were found.  This vector are transient---good only to 
   the next call of ada_lookup_symbol_list.  Any non-function/non-enumeral 
   symbol match within the nest of blocks whose innermost member is BLOCK0,
   is the one match returned (no other matches in that or
     enclosing blocks is returned).  If there are any matches in or
   surrounding BLOCK0, then these alone are returned.  Otherwise, the
   search extends to global and file-scope (static) symbol tables.
   Names prefixed with "standard__" are handled specially: "standard__" 
   is first stripped off, and only static and global symbols are searched.  */

int
ada_lookup_symbol_list (const char *name0, const struct block *block0,
                        domain_enum namespace,
                        struct ada_symbol_info **results)
{
  struct symbol *sym;
  struct symtab *s;
  struct partial_symtab *ps;
  struct blockvector *bv;
  struct objfile *objfile;
  struct block *block;
  const char *name;
  struct minimal_symbol *msymbol;
  int wild_match;
  int cacheIfUnique;
  int block_depth;
  int ndefns;

  obstack_free (&symbol_list_obstack, NULL);
  obstack_init (&symbol_list_obstack);

  cacheIfUnique = 0;

  /* Search specified block and its superiors.  */

  wild_match = (strstr (name0, "__") == NULL);
  name = name0;
  block = (struct block *) block0;      /* FIXME: No cast ought to be
                                           needed, but adding const will
                                           have a cascade effect.  */
  if (strncmp (name0, "standard__", sizeof ("standard__") - 1) == 0)
    {
      wild_match = 0;
      block = NULL;
      name = name0 + sizeof ("standard__") - 1;
    }

  block_depth = 0;
  while (block != NULL)
    {
      block_depth += 1;
      ada_add_block_symbols (&symbol_list_obstack, block, name,
                             namespace, NULL, NULL, wild_match);

      /* If we found a non-function match, assume that's the one.  */
      if (is_nonfunction (defns_collected (&symbol_list_obstack, 0),
                          num_defns_collected (&symbol_list_obstack)))
        goto done;

      block = BLOCK_SUPERBLOCK (block);
    }

  /* If no luck so far, try to find NAME as a local symbol in some lexically
     enclosing subprogram.  */
  if (num_defns_collected (&symbol_list_obstack) == 0 && block_depth > 2)
    add_symbols_from_enclosing_procs (&symbol_list_obstack,
                                      name, namespace, wild_match);

  /* If we found ANY matches among non-global symbols, we're done.  */

  if (num_defns_collected (&symbol_list_obstack) > 0)
    goto done;

  cacheIfUnique = 1;
  if (lookup_cached_symbol (name0, namespace, &sym, &block, &s))
    {
      if (sym != NULL)
        add_defn_to_vec (&symbol_list_obstack, sym, block, s);
      goto done;
    }

  /* Now add symbols from all global blocks: symbol tables, minimal symbol
     tables, and psymtab's.  */

  ALL_PRIMARY_SYMTABS (objfile, s)
  {
    QUIT;
    bv = BLOCKVECTOR (s);
    block = BLOCKVECTOR_BLOCK (bv, GLOBAL_BLOCK);
    ada_add_block_symbols (&symbol_list_obstack, block, name, namespace,
                           objfile, s, wild_match);
  }

  if (namespace == VAR_DOMAIN)
    {
      ALL_MSYMBOLS (objfile, msymbol)
      {
        if (ada_match_name (SYMBOL_LINKAGE_NAME (msymbol), name, wild_match))
          {
            switch (MSYMBOL_TYPE (msymbol))
              {
              case mst_solib_trampoline:
                break;
              default:
                s = find_pc_symtab (SYMBOL_VALUE_ADDRESS (msymbol));
                if (s != NULL)
                  {
                    int ndefns0 = num_defns_collected (&symbol_list_obstack);
                    QUIT;
                    bv = BLOCKVECTOR (s);
                    block = BLOCKVECTOR_BLOCK (bv, GLOBAL_BLOCK);
                    ada_add_block_symbols (&symbol_list_obstack, block,
                                           SYMBOL_LINKAGE_NAME (msymbol),
                                           namespace, objfile, s, wild_match);

                    if (num_defns_collected (&symbol_list_obstack) == ndefns0)
                      {
                        block = BLOCKVECTOR_BLOCK (bv, STATIC_BLOCK);
                        ada_add_block_symbols (&symbol_list_obstack, block,
                                               SYMBOL_LINKAGE_NAME (msymbol),
                                               namespace, objfile, s,
                                               wild_match);
                      }
                  }
              }
          }
      }
    }

  ALL_PSYMTABS (objfile, ps)
  {
    QUIT;
    if (!ps->readin
        && ada_lookup_partial_symbol (ps, name, 1, namespace, wild_match))
      {
        s = PSYMTAB_TO_SYMTAB (ps);
        if (!s->primary)
          continue;
        bv = BLOCKVECTOR (s);
        block = BLOCKVECTOR_BLOCK (bv, GLOBAL_BLOCK);
        ada_add_block_symbols (&symbol_list_obstack, block, name,
                               namespace, objfile, s, wild_match);
      }
  }

  /* Now add symbols from all per-file blocks if we've gotten no hits
     (Not strictly correct, but perhaps better than an error).
     Do the symtabs first, then check the psymtabs.  */

  if (num_defns_collected (&symbol_list_obstack) == 0)
    {

      ALL_PRIMARY_SYMTABS (objfile, s)
      {
        QUIT;
        bv = BLOCKVECTOR (s);
        block = BLOCKVECTOR_BLOCK (bv, STATIC_BLOCK);
        ada_add_block_symbols (&symbol_list_obstack, block, name, namespace,
                               objfile, s, wild_match);
      }

      ALL_PSYMTABS (objfile, ps)
      {
        QUIT;
        if (!ps->readin
            && ada_lookup_partial_symbol (ps, name, 0, namespace, wild_match))
          {
            s = PSYMTAB_TO_SYMTAB (ps);
            bv = BLOCKVECTOR (s);
            if (!s->primary)
              continue;
            block = BLOCKVECTOR_BLOCK (bv, STATIC_BLOCK);
            ada_add_block_symbols (&symbol_list_obstack, block, name,
                                   namespace, objfile, s, wild_match);
          }
      }
    }

done:
  ndefns = num_defns_collected (&symbol_list_obstack);
  *results = defns_collected (&symbol_list_obstack, 1);

  ndefns = remove_extra_symbols (*results, ndefns);

  if (ndefns == 0)
    cache_symbol (name0, namespace, NULL, NULL, NULL);

  if (ndefns == 1 && cacheIfUnique)
    cache_symbol (name0, namespace, (*results)[0].sym, (*results)[0].block,
                  (*results)[0].symtab);

  ndefns = remove_out_of_scope_renamings (*results, ndefns, block0);

  return ndefns;
}

/* Return a symbol in DOMAIN matching NAME, in BLOCK0 and enclosing
   scope and in global scopes, or NULL if none.  NAME is folded and
   encoded first.  Otherwise, the result is as for ada_lookup_symbol_list,
   choosing the first symbol if there are multiple choices.  
   *IS_A_FIELD_OF_THIS is set to 0 and *SYMTAB is set to the symbol
   table in which the symbol was found (in both cases, these
   assignments occur only if the pointers are non-null).  */

struct symbol *
ada_lookup_symbol (const char *name, const struct block *block0,
                   domain_enum namespace, int *is_a_field_of_this,
                   struct symtab **symtab)
{
  struct ada_symbol_info *candidates;
  int n_candidates;

  n_candidates = ada_lookup_symbol_list (ada_encode (ada_fold_name (name)),
                                         block0, namespace, &candidates);

  if (n_candidates == 0)
    return NULL;

  if (is_a_field_of_this != NULL)
    *is_a_field_of_this = 0;

  if (symtab != NULL)
    {
      *symtab = candidates[0].symtab;
      if (*symtab == NULL && candidates[0].block != NULL)
        {
          struct objfile *objfile;
          struct symtab *s;
          struct block *b;
          struct blockvector *bv;

          /* Search the list of symtabs for one which contains the
             address of the start of this block.  */
          ALL_PRIMARY_SYMTABS (objfile, s)
          {
            bv = BLOCKVECTOR (s);
            b = BLOCKVECTOR_BLOCK (bv, GLOBAL_BLOCK);
            if (BLOCK_START (b) <= BLOCK_START (candidates[0].block)
                && BLOCK_END (b) > BLOCK_START (candidates[0].block))
              {
                *symtab = s;
                return fixup_symbol_section (candidates[0].sym, objfile);
              }
          }
          /* FIXME: brobecker/2004-11-12: I think that we should never
             reach this point.  I don't see a reason why we would not
             find a symtab for a given block, so I suggest raising an
             internal_error exception here.  Otherwise, we end up
             returning a symbol but no symtab, which certain parts of
             the code that rely (indirectly) on this function do not
             expect, eventually causing a SEGV.  */
          return fixup_symbol_section (candidates[0].sym, NULL);
        }
    }
  return candidates[0].sym;
}

static struct symbol *
ada_lookup_symbol_nonlocal (const char *name,
                            const char *linkage_name,
                            const struct block *block,
                            const domain_enum domain, struct symtab **symtab)
{
  if (linkage_name == NULL)
    linkage_name = name;
  return ada_lookup_symbol (linkage_name, block_static_block (block), domain,
                            NULL, symtab);
}


/* True iff STR is a possible encoded suffix of a normal Ada name
   that is to be ignored for matching purposes.  Suffixes of parallel
   names (e.g., XVE) are not included here.  Currently, the possible suffixes
   are given by either of the regular expression:

   (__[0-9]+)?[.$][0-9]+  [nested subprogram suffix, on platforms such 
                           as GNU/Linux]
   ___[0-9]+            [nested subprogram suffix, on platforms such as HP/UX]
   _E[0-9]+[bs]$          [protected object entry suffixes]
   (X[nb]*)?((\$|__)[0-9](_?[0-9]+)|___(JM|LJM|X([FDBUP].*|R[^T]?)))?$
 */

static int
is_name_suffix (const char *str)
{
  int k;
  const char *matching;
  const int len = strlen (str);

  /* (__[0-9]+)?\.[0-9]+ */
  matching = str;
  if (len > 3 && str[0] == '_' && str[1] == '_' && isdigit (str[2]))
    {
      matching += 3;
      while (isdigit (matching[0]))
        matching += 1;
      if (matching[0] == '\0')
        return 1;
    }

  if (matching[0] == '.' || matching[0] == '$')
    {
      matching += 1;
      while (isdigit (matching[0]))
        matching += 1;
      if (matching[0] == '\0')
        return 1;
    }

  /* ___[0-9]+ */
  if (len > 3 && str[0] == '_' && str[1] == '_' && str[2] == '_')
    {
      matching = str + 3;
      while (isdigit (matching[0]))
        matching += 1;
      if (matching[0] == '\0')
        return 1;
    }

#if 0
  /* FIXME: brobecker/2005-09-23: Protected Object subprograms end
     with a N at the end. Unfortunately, the compiler uses the same
     convention for other internal types it creates. So treating
     all entity names that end with an "N" as a name suffix causes
     some regressions. For instance, consider the case of an enumerated
     type. To support the 'Image attribute, it creates an array whose
     name ends with N.
     Having a single character like this as a suffix carrying some
     information is a bit risky. Perhaps we should change the encoding
     to be something like "_N" instead.  In the meantime, do not do
     the following check.  */
  /* Protected Object Subprograms */
  if (len == 1 && str [0] == 'N')
    return 1;
#endif

  /* _E[0-9]+[bs]$ */
  if (len > 3 && str[0] == '_' && str [1] == 'E' && isdigit (str[2]))
    {
      matching = str + 3;
      while (isdigit (matching[0]))
        matching += 1;
      if ((matching[0] == 'b' || matching[0] == 's')
          && matching [1] == '\0')
        return 1;
    }

  /* ??? We should not modify STR directly, as we are doing below.  This
     is fine in this case, but may become problematic later if we find
     that this alternative did not work, and want to try matching
     another one from the begining of STR.  Since we modified it, we
     won't be able to find the begining of the string anymore!  */
  if (str[0] == 'X')
    {
      str += 1;
      while (str[0] != '_' && str[0] != '\0')
        {
          if (str[0] != 'n' && str[0] != 'b')
            return 0;
          str += 1;
        }
    }
  if (str[0] == '\000')
    return 1;
  if (str[0] == '_')
    {
      if (str[1] != '_' || str[2] == '\000')
        return 0;
      if (str[2] == '_')
        {
          if (strcmp (str + 3, "JM") == 0)
            return 1;
          /* FIXME: brobecker/2004-09-30: GNAT will soon stop using
             the LJM suffix in favor of the JM one.  But we will
             still accept LJM as a valid suffix for a reasonable
             amount of time, just to allow ourselves to debug programs
             compiled using an older version of GNAT.  */
          if (strcmp (str + 3, "LJM") == 0)
            return 1;
          if (str[3] != 'X')
            return 0;
          if (str[4] == 'F' || str[4] == 'D' || str[4] == 'B'
              || str[4] == 'U' || str[4] == 'P')
            return 1;
          if (str[4] == 'R' && str[5] != 'T')
            return 1;
          return 0;
        }
      if (!isdigit (str[2]))
        return 0;
      for (k = 3; str[k] != '\0'; k += 1)
        if (!isdigit (str[k]) && str[k] != '_')
          return 0;
      return 1;
    }
  if (str[0] == '$' && isdigit (str[1]))
    {
      for (k = 2; str[k] != '\0'; k += 1)
        if (!isdigit (str[k]) && str[k] != '_')
          return 0;
      return 1;
    }
  return 0;
}

/* Return nonzero if the given string starts with a dot ('.')
   followed by zero or more digits.  
   
   Note: brobecker/2003-11-10: A forward declaration has not been
   added at the begining of this file yet, because this function
   is only used to work around a problem found during wild matching
   when trying to match minimal symbol names against symbol names
   obtained from dwarf-2 data.  This function is therefore currently
   only used in wild_match() and is likely to be deleted when the
   problem in dwarf-2 is fixed.  */

static int
is_dot_digits_suffix (const char *str)
{
  if (str[0] != '.')
    return 0;

  str++;
  while (isdigit (str[0]))
    str++;
  return (str[0] == '\0');
}

/* Return non-zero if NAME0 is a valid match when doing wild matching.
   Certain symbols appear at first to match, except that they turn out
   not to follow the Ada encoding and hence should not be used as a wild
   match of a given pattern.  */

static int
is_valid_name_for_wild_match (const char *name0)
{
  const char *decoded_name = ada_decode (name0);
  int i;

  for (i=0; decoded_name[i] != '\0'; i++)
    if (isalpha (decoded_name[i]) && !islower (decoded_name[i]))
      return 0;

  return 1;
}

/* True if NAME represents a name of the form A1.A2....An, n>=1 and
   PATN[0..PATN_LEN-1] = Ak.Ak+1.....An for some k >= 1.  Ignores
   informational suffixes of NAME (i.e., for which is_name_suffix is
   true).  */

static int
wild_match (const char *patn0, int patn_len, const char *name0)
{
  int name_len;
  char *name;
  char *patn;

  /* FIXME: brobecker/2003-11-10: For some reason, the symbol name
     stored in the symbol table for nested function names is sometimes
     different from the name of the associated entity stored in
     the dwarf-2 data: This is the case for nested subprograms, where
     the minimal symbol name contains a trailing ".[:digit:]+" suffix,
     while the symbol name from the dwarf-2 data does not.

     Although the DWARF-2 standard documents that entity names stored
     in the dwarf-2 data should be identical to the name as seen in
     the source code, GNAT takes a different approach as we already use
     a special encoding mechanism to convey the information so that
     a C debugger can still use the information generated to debug
     Ada programs.  A corollary is that the symbol names in the dwarf-2
     data should match the names found in the symbol table.  I therefore
     consider this issue as a compiler defect.

     Until the compiler is properly fixed, we work-around the problem
     by ignoring such suffixes during the match.  We do so by making
     a copy of PATN0 and NAME0, and then by stripping such a suffix
     if present.  We then perform the match on the resulting strings.  */
  {
    char *dot;
    name_len = strlen (name0);

    name = (char *) alloca ((name_len + 1) * sizeof (char));
    strcpy (name, name0);
    dot = strrchr (name, '.');
    if (dot != NULL && is_dot_digits_suffix (dot))
      *dot = '\0';

    patn = (char *) alloca ((patn_len + 1) * sizeof (char));
    strncpy (patn, patn0, patn_len);
    patn[patn_len] = '\0';
    dot = strrchr (patn, '.');
    if (dot != NULL && is_dot_digits_suffix (dot))
      {
        *dot = '\0';
        patn_len = dot - patn;
      }
  }

  /* Now perform the wild match.  */

  name_len = strlen (name);
  if (name_len >= patn_len + 5 && strncmp (name, "_ada_", 5) == 0
      && strncmp (patn, name + 5, patn_len) == 0
      && is_name_suffix (name + patn_len + 5))
    return 1;

  while (name_len >= patn_len)
    {
      if (strncmp (patn, name, patn_len) == 0
          && is_name_suffix (name + patn_len))
        return (is_valid_name_for_wild_match (name0));
      do
        {
          name += 1;
          name_len -= 1;
        }
      while (name_len > 0
             && name[0] != '.' && (name[0] != '_' || name[1] != '_'));
      if (name_len <= 0)
        return 0;
      if (name[0] == '_')
        {
          if (!islower (name[2]))
            return 0;
          name += 2;
          name_len -= 2;
        }
      else
        {
          if (!islower (name[1]))
            return 0;
          name += 1;
          name_len -= 1;
        }
    }

  return 0;
}


/* Add symbols from BLOCK matching identifier NAME in DOMAIN to
   vector *defn_symbols, updating the list of symbols in OBSTACKP 
   (if necessary).  If WILD, treat as NAME with a wildcard prefix. 
   OBJFILE is the section containing BLOCK.
   SYMTAB is recorded with each symbol added.  */

static void
ada_add_block_symbols (struct obstack *obstackp,
                       struct block *block, const char *name,
                       domain_enum domain, struct objfile *objfile,
                       struct symtab *symtab, int wild)
{
  struct dict_iterator iter;
  int name_len = strlen (name);
  /* A matching argument symbol, if any.  */
  struct symbol *arg_sym;
  /* Set true when we find a matching non-argument symbol.  */
  int found_sym;
  struct symbol *sym;

  arg_sym = NULL;
  found_sym = 0;
  if (wild)
    {
      struct symbol *sym;
      ALL_BLOCK_SYMBOLS (block, iter, sym)
      {
        if (SYMBOL_DOMAIN (sym) == domain
            && wild_match (name, name_len, SYMBOL_LINKAGE_NAME (sym)))
          {
            switch (SYMBOL_CLASS (sym))
              {
              case LOC_ARG:
              case LOC_LOCAL_ARG:
              case LOC_REF_ARG:
              case LOC_REGPARM:
              case LOC_REGPARM_ADDR:
              case LOC_BASEREG_ARG:
              case LOC_COMPUTED_ARG:
                arg_sym = sym;
                break;
              case LOC_UNRESOLVED:
                continue;
              default:
                found_sym = 1;
                add_defn_to_vec (obstackp,
                                 fixup_symbol_section (sym, objfile),
                                 block, symtab);
                break;
              }
          }
      }
    }
  else
    {
      ALL_BLOCK_SYMBOLS (block, iter, sym)
      {
        if (SYMBOL_DOMAIN (sym) == domain)
          {
            int cmp = strncmp (name, SYMBOL_LINKAGE_NAME (sym), name_len);
            if (cmp == 0
                && is_name_suffix (SYMBOL_LINKAGE_NAME (sym) + name_len))
              {
                switch (SYMBOL_CLASS (sym))
                  {
                  case LOC_ARG:
                  case LOC_LOCAL_ARG:
                  case LOC_REF_ARG:
                  case LOC_REGPARM:
                  case LOC_REGPARM_ADDR:
                  case LOC_BASEREG_ARG:
                  case LOC_COMPUTED_ARG:
                    arg_sym = sym;
                    break;
                  case LOC_UNRESOLVED:
                    break;
                  default:
                    found_sym = 1;
                    add_defn_to_vec (obstackp,
                                     fixup_symbol_section (sym, objfile),
                                     block, symtab);
                    break;
                  }
              }
          }
      }
    }

  if (!found_sym && arg_sym != NULL)
    {
      add_defn_to_vec (obstackp,
                       fixup_symbol_section (arg_sym, objfile),
                       block, symtab);
    }

  if (!wild)
    {
      arg_sym = NULL;
      found_sym = 0;

      ALL_BLOCK_SYMBOLS (block, iter, sym)
      {
        if (SYMBOL_DOMAIN (sym) == domain)
          {
            int cmp;

            cmp = (int) '_' - (int) SYMBOL_LINKAGE_NAME (sym)[0];
            if (cmp == 0)
              {
                cmp = strncmp ("_ada_", SYMBOL_LINKAGE_NAME (sym), 5);
                if (cmp == 0)
                  cmp = strncmp (name, SYMBOL_LINKAGE_NAME (sym) + 5,
                                 name_len);
              }

            if (cmp == 0
                && is_name_suffix (SYMBOL_LINKAGE_NAME (sym) + name_len + 5))
              {
                switch (SYMBOL_CLASS (sym))
                  {
                  case LOC_ARG:
                  case LOC_LOCAL_ARG:
                  case LOC_REF_ARG:
                  case LOC_REGPARM:
                  case LOC_REGPARM_ADDR:
                  case LOC_BASEREG_ARG:
                  case LOC_COMPUTED_ARG:
                    arg_sym = sym;
                    break;
                  case LOC_UNRESOLVED:
                    break;
                  default:
                    found_sym = 1;
                    add_defn_to_vec (obstackp,
                                     fixup_symbol_section (sym, objfile),
                                     block, symtab);
                    break;
                  }
              }
          }
      }

      /* NOTE: This really shouldn't be needed for _ada_ symbols.
         They aren't parameters, right?  */
      if (!found_sym && arg_sym != NULL)
        {
          add_defn_to_vec (obstackp,
                           fixup_symbol_section (arg_sym, objfile),
                           block, symtab);
        }
    }
}

                                /* Field Access */

/* True if field number FIELD_NUM in struct or union type TYPE is supposed
   to be invisible to users.  */

int
ada_is_ignored_field (struct type *type, int field_num)
{
  if (field_num < 0 || field_num > TYPE_NFIELDS (type))
    return 1;
  else
    {
      const char *name = TYPE_FIELD_NAME (type, field_num);
      return (name == NULL
              || (name[0] == '_' && strncmp (name, "_parent", 7) != 0));
    }
}

/* True iff TYPE has a tag field.  If REFOK, then TYPE may also be a
   pointer or reference type whose ultimate target has a tag field. */

int
ada_is_tagged_type (struct type *type, int refok)
{
  return (ada_lookup_struct_elt_type (type, "_tag", refok, 1, NULL) != NULL);
}

/* True iff TYPE represents the type of X'Tag */

int
ada_is_tag_type (struct type *type)
{
  if (type == NULL || TYPE_CODE (type) != TYPE_CODE_PTR)
    return 0;
  else
    {
      const char *name = ada_type_name (TYPE_TARGET_TYPE (type));
      return (name != NULL
              && strcmp (name, "ada__tags__dispatch_table") == 0);
    }
}

/* The type of the tag on VAL.  */

struct type *
ada_tag_type (struct value *val)
{
  return ada_lookup_struct_elt_type (value_type (val), "_tag", 1, 0, NULL);
}

/* The value of the tag on VAL.  */

struct value *
ada_value_tag (struct value *val)
{
  return ada_value_struct_elt (val, "_tag", 0);
}

/* The value of the tag on the object of type TYPE whose contents are
   saved at VALADDR, if it is non-null, or is at memory address
   ADDRESS. */

static struct value *
value_tag_from_contents_and_address (struct type *type,
				     const gdb_byte *valaddr,
                                     CORE_ADDR address)
{
  int tag_byte_offset, dummy1, dummy2;
  struct type *tag_type;
  if (find_struct_field ("_tag", type, 0, &tag_type, &tag_byte_offset,
                         NULL, NULL, NULL))
    {
      const gdb_byte *valaddr1 = ((valaddr == NULL)
				  ? NULL
				  : valaddr + tag_byte_offset);
      CORE_ADDR address1 = (address == 0) ? 0 : address + tag_byte_offset;

      return value_from_contents_and_address (tag_type, valaddr1, address1);
    }
  return NULL;
}

static struct type *
type_from_tag (struct value *tag)
{
  const char *type_name = ada_tag_name (tag);
  if (type_name != NULL)
    return ada_find_any_type (ada_encode (type_name));
  return NULL;
}

struct tag_args
{
  struct value *tag;
  char *name;
};


static int ada_tag_name_1 (void *);
static int ada_tag_name_2 (struct tag_args *);

/* Wrapper function used by ada_tag_name.  Given a struct tag_args*
   value ARGS, sets ARGS->name to the tag name of ARGS->tag.  
   The value stored in ARGS->name is valid until the next call to 
   ada_tag_name_1.  */

static int
ada_tag_name_1 (void *args0)
{
  struct tag_args *args = (struct tag_args *) args0;
  static char name[1024];
  char *p;
  struct value *val;
  args->name = NULL;
  val = ada_value_struct_elt (args->tag, "tsd", 1);
  if (val == NULL)
    return ada_tag_name_2 (args);
  val = ada_value_struct_elt (val, "expanded_name", 1);
  if (val == NULL)
    return 0;
  read_memory_string (value_as_address (val), name, sizeof (name) - 1);
  for (p = name; *p != '\0'; p += 1)
    if (isalpha (*p))
      *p = tolower (*p);
  args->name = name;
  return 0;
}

/* Utility function for ada_tag_name_1 that tries the second
   representation for the dispatch table (in which there is no
   explicit 'tsd' field in the referent of the tag pointer, and instead
   the tsd pointer is stored just before the dispatch table. */
   
static int
ada_tag_name_2 (struct tag_args *args)
{
  struct type *info_type;
  static char name[1024];
  char *p;
  struct value *val, *valp;

  args->name = NULL;
  info_type = ada_find_any_type ("ada__tags__type_specific_data");
  if (info_type == NULL)
    return 0;
  info_type = lookup_pointer_type (lookup_pointer_type (info_type));
  valp = value_cast (info_type, args->tag);
  if (valp == NULL)
    return 0;
  val = value_ind (value_add (valp, value_from_longest (builtin_type_int, -1)));
  if (val == NULL)
    return 0;
  val = ada_value_struct_elt (val, "expanded_name", 1);
  if (val == NULL)
    return 0;
  read_memory_string (value_as_address (val), name, sizeof (name) - 1);
  for (p = name; *p != '\0'; p += 1)
    if (isalpha (*p))
      *p = tolower (*p);
  args->name = name;
  return 0;
}

/* The type name of the dynamic type denoted by the 'tag value TAG, as
 * a C string.  */

const char *
ada_tag_name (struct value *tag)
{
  struct tag_args args;
  if (!ada_is_tag_type (value_type (tag)))
    return NULL;
  args.tag = tag;
  args.name = NULL;
  catch_errors (ada_tag_name_1, &args, NULL, RETURN_MASK_ALL);
  return args.name;
}

/* The parent type of TYPE, or NULL if none.  */

struct type *
ada_parent_type (struct type *type)
{
  int i;

  type = ada_check_typedef (type);

  if (type == NULL || TYPE_CODE (type) != TYPE_CODE_STRUCT)
    return NULL;

  for (i = 0; i < TYPE_NFIELDS (type); i += 1)
    if (ada_is_parent_field (type, i))
      return ada_check_typedef (TYPE_FIELD_TYPE (type, i));

  return NULL;
}

/* True iff field number FIELD_NUM of structure type TYPE contains the
   parent-type (inherited) fields of a derived type.  Assumes TYPE is
   a structure type with at least FIELD_NUM+1 fields.  */

int
ada_is_parent_field (struct type *type, int field_num)
{
  const char *name = TYPE_FIELD_NAME (ada_check_typedef (type), field_num);
  return (name != NULL
          && (strncmp (name, "PARENT", 6) == 0
              || strncmp (name, "_parent", 7) == 0));
}

/* True iff field number FIELD_NUM of structure type TYPE is a
   transparent wrapper field (which should be silently traversed when doing
   field selection and flattened when printing).  Assumes TYPE is a
   structure type with at least FIELD_NUM+1 fields.  Such fields are always
   structures.  */

int
ada_is_wrapper_field (struct type *type, int field_num)
{
  const char *name = TYPE_FIELD_NAME (type, field_num);
  return (name != NULL
          && (strncmp (name, "PARENT", 6) == 0
              || strcmp (name, "REP") == 0
              || strncmp (name, "_parent", 7) == 0
              || name[0] == 'S' || name[0] == 'R' || name[0] == 'O'));
}

/* True iff field number FIELD_NUM of structure or union type TYPE
   is a variant wrapper.  Assumes TYPE is a structure type with at least
   FIELD_NUM+1 fields.  */

int
ada_is_variant_part (struct type *type, int field_num)
{
  struct type *field_type = TYPE_FIELD_TYPE (type, field_num);
  return (TYPE_CODE (field_type) == TYPE_CODE_UNION
          || (is_dynamic_field (type, field_num)
              && (TYPE_CODE (TYPE_TARGET_TYPE (field_type)) 
		  == TYPE_CODE_UNION)));
}

/* Assuming that VAR_TYPE is a variant wrapper (type of the variant part)
   whose discriminants are contained in the record type OUTER_TYPE,
   returns the type of the controlling discriminant for the variant.  */

struct type *
ada_variant_discrim_type (struct type *var_type, struct type *outer_type)
{
  char *name = ada_variant_discrim_name (var_type);
  struct type *type =
    ada_lookup_struct_elt_type (outer_type, name, 1, 1, NULL);
  if (type == NULL)
    return builtin_type_int;
  else
    return type;
}

/* Assuming that TYPE is the type of a variant wrapper, and FIELD_NUM is a
   valid field number within it, returns 1 iff field FIELD_NUM of TYPE
   represents a 'when others' clause; otherwise 0.  */

int
ada_is_others_clause (struct type *type, int field_num)
{
  const char *name = TYPE_FIELD_NAME (type, field_num);
  return (name != NULL && name[0] == 'O');
}

/* Assuming that TYPE0 is the type of the variant part of a record,
   returns the name of the discriminant controlling the variant.
   The value is valid until the next call to ada_variant_discrim_name.  */

char *
ada_variant_discrim_name (struct type *type0)
{
  static char *result = NULL;
  static size_t result_len = 0;
  struct type *type;
  const char *name;
  const char *discrim_end;
  const char *discrim_start;

  if (TYPE_CODE (type0) == TYPE_CODE_PTR)
    type = TYPE_TARGET_TYPE (type0);
  else
    type = type0;

  name = ada_type_name (type);

  if (name == NULL || name[0] == '\000')
    return "";

  for (discrim_end = name + strlen (name) - 6; discrim_end != name;
       discrim_end -= 1)
    {
      if (strncmp (discrim_end, "___XVN", 6) == 0)
        break;
    }
  if (discrim_end == name)
    return "";

  for (discrim_start = discrim_end; discrim_start != name + 3;
       discrim_start -= 1)
    {
      if (discrim_start == name + 1)
        return "";
      if ((discrim_start > name + 3
           && strncmp (discrim_start - 3, "___", 3) == 0)
          || discrim_start[-1] == '.')
        break;
    }

  GROW_VECT (result, result_len, discrim_end - discrim_start + 1);
  strncpy (result, discrim_start, discrim_end - discrim_start);
  result[discrim_end - discrim_start] = '\0';
  return result;
}

/* Scan STR for a subtype-encoded number, beginning at position K.
   Put the position of the character just past the number scanned in
   *NEW_K, if NEW_K!=NULL.  Put the scanned number in *R, if R!=NULL.
   Return 1 if there was a valid number at the given position, and 0
   otherwise.  A "subtype-encoded" number consists of the absolute value
   in decimal, followed by the letter 'm' to indicate a negative number.
   Assumes 0m does not occur.  */

int
ada_scan_number (const char str[], int k, LONGEST * R, int *new_k)
{
  ULONGEST RU;

  if (!isdigit (str[k]))
    return 0;

  /* Do it the hard way so as not to make any assumption about
     the relationship of unsigned long (%lu scan format code) and
     LONGEST.  */
  RU = 0;
  while (isdigit (str[k]))
    {
      RU = RU * 10 + (str[k] - '0');
      k += 1;
    }

  if (str[k] == 'm')
    {
      if (R != NULL)
        *R = (-(LONGEST) (RU - 1)) - 1;
      k += 1;
    }
  else if (R != NULL)
    *R = (LONGEST) RU;

  /* NOTE on the above: Technically, C does not say what the results of
     - (LONGEST) RU or (LONGEST) -RU are for RU == largest positive
     number representable as a LONGEST (although either would probably work
     in most implementations).  When RU>0, the locution in the then branch
     above is always equivalent to the negative of RU.  */

  if (new_k != NULL)
    *new_k = k;
  return 1;
}

/* Assuming that TYPE is a variant part wrapper type (a VARIANTS field),
   and FIELD_NUM is a valid field number within it, returns 1 iff VAL is
   in the range encoded by field FIELD_NUM of TYPE; otherwise 0.  */

int
ada_in_variant (LONGEST val, struct type *type, int field_num)
{
  const char *name = TYPE_FIELD_NAME (type, field_num);
  int p;

  p = 0;
  while (1)
    {
      switch (name[p])
        {
        case '\0':
          return 0;
        case 'S':
          {
            LONGEST W;
            if (!ada_scan_number (name, p + 1, &W, &p))
              return 0;
            if (val == W)
              return 1;
            break;
          }
        case 'R':
          {
            LONGEST L, U;
            if (!ada_scan_number (name, p + 1, &L, &p)
                || name[p] != 'T' || !ada_scan_number (name, p + 1, &U, &p))
              return 0;
            if (val >= L && val <= U)
              return 1;
            break;
          }
        case 'O':
          return 1;
        default:
          return 0;
        }
    }
}

/* FIXME: Lots of redundancy below.  Try to consolidate. */

/* Given a value ARG1 (offset by OFFSET bytes) of a struct or union type
   ARG_TYPE, extract and return the value of one of its (non-static)
   fields.  FIELDNO says which field.   Differs from value_primitive_field
   only in that it can handle packed values of arbitrary type.  */

static struct value *
ada_value_primitive_field (struct value *arg1, int offset, int fieldno,
                           struct type *arg_type)
{
  struct type *type;

  arg_type = ada_check_typedef (arg_type);
  type = TYPE_FIELD_TYPE (arg_type, fieldno);

  /* Handle packed fields.  */

  if (TYPE_FIELD_BITSIZE (arg_type, fieldno) != 0)
    {
      int bit_pos = TYPE_FIELD_BITPOS (arg_type, fieldno);
      int bit_size = TYPE_FIELD_BITSIZE (arg_type, fieldno);

      return ada_value_primitive_packed_val (arg1, value_contents (arg1),
                                             offset + bit_pos / 8,
                                             bit_pos % 8, bit_size, type);
    }
  else
    return value_primitive_field (arg1, offset, fieldno, arg_type);
}

/* Find field with name NAME in object of type TYPE.  If found, 
   set the following for each argument that is non-null:
    - *FIELD_TYPE_P to the field's type; 
    - *BYTE_OFFSET_P to OFFSET + the byte offset of the field within 
      an object of that type;
    - *BIT_OFFSET_P to the bit offset modulo byte size of the field; 
    - *BIT_SIZE_P to its size in bits if the field is packed, and 
      0 otherwise;
   If INDEX_P is non-null, increment *INDEX_P by the number of source-visible
   fields up to but not including the desired field, or by the total
   number of fields if not found.   A NULL value of NAME never
   matches; the function just counts visible fields in this case.
   
   Returns 1 if found, 0 otherwise. */

static int
find_struct_field (char *name, struct type *type, int offset,
                   struct type **field_type_p,
                   int *byte_offset_p, int *bit_offset_p, int *bit_size_p,
		   int *index_p)
{
  int i;

  type = ada_check_typedef (type);

  if (field_type_p != NULL)
    *field_type_p = NULL;
  if (byte_offset_p != NULL)
    *byte_offset_p = 0;
  if (bit_offset_p != NULL)
    *bit_offset_p = 0;
  if (bit_size_p != NULL)
    *bit_size_p = 0;

  for (i = 0; i < TYPE_NFIELDS (type); i += 1)
    {
      int bit_pos = TYPE_FIELD_BITPOS (type, i);
      int fld_offset = offset + bit_pos / 8;
      char *t_field_name = TYPE_FIELD_NAME (type, i);

      if (t_field_name == NULL)
        continue;

      else if (name != NULL && field_name_match (t_field_name, name))
        {
          int bit_size = TYPE_FIELD_BITSIZE (type, i);
	  if (field_type_p != NULL)
	    *field_type_p = TYPE_FIELD_TYPE (type, i);
	  if (byte_offset_p != NULL)
	    *byte_offset_p = fld_offset;
	  if (bit_offset_p != NULL)
	    *bit_offset_p = bit_pos % 8;
	  if (bit_size_p != NULL)
	    *bit_size_p = bit_size;
          return 1;
        }
      else if (ada_is_wrapper_field (type, i))
        {
	  if (find_struct_field (name, TYPE_FIELD_TYPE (type, i), fld_offset,
				 field_type_p, byte_offset_p, bit_offset_p,
				 bit_size_p, index_p))
            return 1;
        }
      else if (ada_is_variant_part (type, i))
        {
	  /* PNH: Wait.  Do we ever execute this section, or is ARG always of 
	     fixed type?? */
          int j;
          struct type *field_type
	    = ada_check_typedef (TYPE_FIELD_TYPE (type, i));

          for (j = 0; j < TYPE_NFIELDS (field_type); j += 1)
            {
              if (find_struct_field (name, TYPE_FIELD_TYPE (field_type, j),
                                     fld_offset
                                     + TYPE_FIELD_BITPOS (field_type, j) / 8,
                                     field_type_p, byte_offset_p,
                                     bit_offset_p, bit_size_p, index_p))
                return 1;
            }
        }
      else if (index_p != NULL)
	*index_p += 1;
    }
  return 0;
}

/* Number of user-visible fields in record type TYPE. */

static int
num_visible_fields (struct type *type)
{
  int n;
  n = 0;
  find_struct_field (NULL, type, 0, NULL, NULL, NULL, NULL, &n);
  return n;
}

/* Look for a field NAME in ARG.  Adjust the address of ARG by OFFSET bytes,
   and search in it assuming it has (class) type TYPE.
   If found, return value, else return NULL.

   Searches recursively through wrapper fields (e.g., '_parent').  */

static struct value *
ada_search_struct_field (char *name, struct value *arg, int offset,
                         struct type *type)
{
  int i;
  type = ada_check_typedef (type);

  for (i = 0; i < TYPE_NFIELDS (type); i += 1)
    {
      char *t_field_name = TYPE_FIELD_NAME (type, i);

      if (t_field_name == NULL)
        continue;

      else if (field_name_match (t_field_name, name))
        return ada_value_primitive_field (arg, offset, i, type);

      else if (ada_is_wrapper_field (type, i))
        {
          struct value *v =     /* Do not let indent join lines here. */
            ada_search_struct_field (name, arg,
                                     offset + TYPE_FIELD_BITPOS (type, i) / 8,
                                     TYPE_FIELD_TYPE (type, i));
          if (v != NULL)
            return v;
        }

      else if (ada_is_variant_part (type, i))
        {
	  /* PNH: Do we ever get here?  See find_struct_field. */
          int j;
          struct type *field_type = ada_check_typedef (TYPE_FIELD_TYPE (type, i));
          int var_offset = offset + TYPE_FIELD_BITPOS (type, i) / 8;

          for (j = 0; j < TYPE_NFIELDS (field_type); j += 1)
            {
              struct value *v = ada_search_struct_field /* Force line break.  */
                (name, arg,
                 var_offset + TYPE_FIELD_BITPOS (field_type, j) / 8,
                 TYPE_FIELD_TYPE (field_type, j));
              if (v != NULL)
                return v;
            }
        }
    }
  return NULL;
}

static struct value *ada_index_struct_field_1 (int *, struct value *,
					       int, struct type *);


/* Return field #INDEX in ARG, where the index is that returned by
 * find_struct_field through its INDEX_P argument.  Adjust the address
 * of ARG by OFFSET bytes, and search in it assuming it has (class) type TYPE.
 * If found, return value, else return NULL. */

static struct value *
ada_index_struct_field (int index, struct value *arg, int offset,
			struct type *type)
{
  return ada_index_struct_field_1 (&index, arg, offset, type);
}


/* Auxiliary function for ada_index_struct_field.  Like
 * ada_index_struct_field, but takes index from *INDEX_P and modifies
 * *INDEX_P. */

static struct value *
ada_index_struct_field_1 (int *index_p, struct value *arg, int offset,
			  struct type *type)
{
  int i;
  type = ada_check_typedef (type);

  for (i = 0; i < TYPE_NFIELDS (type); i += 1)
    {
      if (TYPE_FIELD_NAME (type, i) == NULL)
        continue;
      else if (ada_is_wrapper_field (type, i))
        {
          struct value *v =     /* Do not let indent join lines here. */
            ada_index_struct_field_1 (index_p, arg,
				      offset + TYPE_FIELD_BITPOS (type, i) / 8,
				      TYPE_FIELD_TYPE (type, i));
          if (v != NULL)
            return v;
        }

      else if (ada_is_variant_part (type, i))
        {
	  /* PNH: Do we ever get here?  See ada_search_struct_field,
	     find_struct_field. */
	  error (_("Cannot assign this kind of variant record"));
        }
      else if (*index_p == 0)
        return ada_value_primitive_field (arg, offset, i, type);
      else
	*index_p -= 1;
    }
  return NULL;
}

/* Given ARG, a value of type (pointer or reference to a)*
   structure/union, extract the component named NAME from the ultimate
   target structure/union and return it as a value with its
   appropriate type.  If ARG is a pointer or reference and the field
   is not packed, returns a reference to the field, otherwise the
   value of the field (an lvalue if ARG is an lvalue).     

   The routine searches for NAME among all members of the structure itself
   and (recursively) among all members of any wrapper members
   (e.g., '_parent').

   If NO_ERR, then simply return NULL in case of error, rather than 
   calling error.  */

struct value *
ada_value_struct_elt (struct value *arg, char *name, int no_err)
{
  struct type *t, *t1;
  struct value *v;

  v = NULL;
  t1 = t = ada_check_typedef (value_type (arg));
  if (TYPE_CODE (t) == TYPE_CODE_REF)
    {
      t1 = TYPE_TARGET_TYPE (t);
      if (t1 == NULL)
	goto BadValue;
      t1 = ada_check_typedef (t1);
      if (TYPE_CODE (t1) == TYPE_CODE_PTR)
        {
          arg = coerce_ref (arg);
          t = t1;
        }
    }

  while (TYPE_CODE (t) == TYPE_CODE_PTR)
    {
      t1 = TYPE_TARGET_TYPE (t);
      if (t1 == NULL)
	goto BadValue;
      t1 = ada_check_typedef (t1);
      if (TYPE_CODE (t1) == TYPE_CODE_PTR)
        {
          arg = value_ind (arg);
          t = t1;
        }
      else
        break;
    }

  if (TYPE_CODE (t1) != TYPE_CODE_STRUCT && TYPE_CODE (t1) != TYPE_CODE_UNION)
    goto BadValue;

  if (t1 == t)
    v = ada_search_struct_field (name, arg, 0, t);
  else
    {
      int bit_offset, bit_size, byte_offset;
      struct type *field_type;
      CORE_ADDR address;

      if (TYPE_CODE (t) == TYPE_CODE_PTR)
        address = value_as_address (arg);
      else
        address = unpack_pointer (t, value_contents (arg));

      t1 = ada_to_fixed_type (ada_get_base_type (t1), NULL, address, NULL);
      if (find_struct_field (name, t1, 0,
                             &field_type, &byte_offset, &bit_offset,
                             &bit_size, NULL))
        {
          if (bit_size != 0)
            {
              if (TYPE_CODE (t) == TYPE_CODE_REF)
                arg = ada_coerce_ref (arg);
              else
                arg = ada_value_ind (arg);
              v = ada_value_primitive_packed_val (arg, NULL, byte_offset,
                                                  bit_offset, bit_size,
                                                  field_type);
            }
          else
            v = value_from_pointer (lookup_reference_type (field_type),
                                    address + byte_offset);
        }
    }

  if (v != NULL || no_err)
    return v;
  else
    error (_("There is no member named %s."), name);

 BadValue:
  if (no_err)
    return NULL;
  else
    error (_("Attempt to extract a component of a value that is not a record."));
}

/* Given a type TYPE, look up the type of the component of type named NAME.
   If DISPP is non-null, add its byte displacement from the beginning of a
   structure (pointed to by a value) of type TYPE to *DISPP (does not
   work for packed fields).

   Matches any field whose name has NAME as a prefix, possibly
   followed by "___".

   TYPE can be either a struct or union. If REFOK, TYPE may also 
   be a (pointer or reference)+ to a struct or union, and the
   ultimate target type will be searched.

   Looks recursively into variant clauses and parent types.

   If NOERR is nonzero, return NULL if NAME is not suitably defined or
   TYPE is not a type of the right kind.  */

static struct type *
ada_lookup_struct_elt_type (struct type *type, char *name, int refok,
                            int noerr, int *dispp)
{
  int i;

  if (name == NULL)
    goto BadName;

  if (refok && type != NULL)
    while (1)
      {
        type = ada_check_typedef (type);
        if (TYPE_CODE (type) != TYPE_CODE_PTR
            && TYPE_CODE (type) != TYPE_CODE_REF)
          break;
        type = TYPE_TARGET_TYPE (type);
      }

  if (type == NULL
      || (TYPE_CODE (type) != TYPE_CODE_STRUCT
          && TYPE_CODE (type) != TYPE_CODE_UNION))
    {
      if (noerr)
        return NULL;
      else
        {
          target_terminal_ours ();
          gdb_flush (gdb_stdout);
	  if (type == NULL)
	    error (_("Type (null) is not a structure or union type"));
	  else
	    {
	      /* XXX: type_sprint */
	      fprintf_unfiltered (gdb_stderr, _("Type "));
	      type_print (type, "", gdb_stderr, -1);
	      error (_(" is not a structure or union type"));
	    }
        }
    }

  type = to_static_fixed_type (type);

  for (i = 0; i < TYPE_NFIELDS (type); i += 1)
    {
      char *t_field_name = TYPE_FIELD_NAME (type, i);
      struct type *t;
      int disp;

      if (t_field_name == NULL)
        continue;

      else if (field_name_match (t_field_name, name))
        {
          if (dispp != NULL)
            *dispp += TYPE_FIELD_BITPOS (type, i) / 8;
          return ada_check_typedef (TYPE_FIELD_TYPE (type, i));
        }

      else if (ada_is_wrapper_field (type, i))
        {
          disp = 0;
          t = ada_lookup_struct_elt_type (TYPE_FIELD_TYPE (type, i), name,
                                          0, 1, &disp);
          if (t != NULL)
            {
              if (dispp != NULL)
                *dispp += disp + TYPE_FIELD_BITPOS (type, i) / 8;
              return t;
            }
        }

      else if (ada_is_variant_part (type, i))
        {
          int j;
          struct type *field_type = ada_check_typedef (TYPE_FIELD_TYPE (type, i));

          for (j = TYPE_NFIELDS (field_type) - 1; j >= 0; j -= 1)
            {
              disp = 0;
              t = ada_lookup_struct_elt_type (TYPE_FIELD_TYPE (field_type, j),
                                              name, 0, 1, &disp);
              if (t != NULL)
                {
                  if (dispp != NULL)
                    *dispp += disp + TYPE_FIELD_BITPOS (type, i) / 8;
                  return t;
                }
            }
        }

    }

BadName:
  if (!noerr)
    {
      target_terminal_ours ();
      gdb_flush (gdb_stdout);
      if (name == NULL)
        {
	  /* XXX: type_sprint */
	  fprintf_unfiltered (gdb_stderr, _("Type "));
	  type_print (type, "", gdb_stderr, -1);
	  error (_(" has no component named <null>"));
	}
      else
	{
	  /* XXX: type_sprint */
	  fprintf_unfiltered (gdb_stderr, _("Type "));
	  type_print (type, "", gdb_stderr, -1);
	  error (_(" has no component named %s"), name);
	}
    }

  return NULL;
}

/* Assuming that VAR_TYPE is the type of a variant part of a record (a union),
   within a value of type OUTER_TYPE that is stored in GDB at
   OUTER_VALADDR, determine which variant clause (field number in VAR_TYPE,
   numbering from 0) is applicable.  Returns -1 if none are.  */

int
ada_which_variant_applies (struct type *var_type, struct type *outer_type,
                           const gdb_byte *outer_valaddr)
{
  int others_clause;
  int i;
  int disp;
  struct type *discrim_type;
  char *discrim_name = ada_variant_discrim_name (var_type);
  LONGEST discrim_val;

  disp = 0;
  discrim_type =
    ada_lookup_struct_elt_type (outer_type, discrim_name, 1, 1, &disp);
  if (discrim_type == NULL)
    return -1;
  discrim_val = unpack_long (discrim_type, outer_valaddr + disp);

  others_clause = -1;
  for (i = 0; i < TYPE_NFIELDS (var_type); i += 1)
    {
      if (ada_is_others_clause (var_type, i))
        others_clause = i;
      else if (ada_in_variant (discrim_val, var_type, i))
        return i;
    }

  return others_clause;
}



                                /* Dynamic-Sized Records */

/* Strategy: The type ostensibly attached to a value with dynamic size
   (i.e., a size that is not statically recorded in the debugging
   data) does not accurately reflect the size or layout of the value.
   Our strategy is to convert these values to values with accurate,
   conventional types that are constructed on the fly.  */

/* There is a subtle and tricky problem here.  In general, we cannot
   determine the size of dynamic records without its data.  However,
   the 'struct value' data structure, which GDB uses to represent
   quantities in the inferior process (the target), requires the size
   of the type at the time of its allocation in order to reserve space
   for GDB's internal copy of the data.  That's why the
   'to_fixed_xxx_type' routines take (target) addresses as parameters,
   rather than struct value*s.

   However, GDB's internal history variables ($1, $2, etc.) are
   struct value*s containing internal copies of the data that are not, in
   general, the same as the data at their corresponding addresses in
   the target.  Fortunately, the types we give to these values are all
   conventional, fixed-size types (as per the strategy described
   above), so that we don't usually have to perform the
   'to_fixed_xxx_type' conversions to look at their values.
   Unfortunately, there is one exception: if one of the internal
   history variables is an array whose elements are unconstrained
   records, then we will need to create distinct fixed types for each
   element selected.  */

/* The upshot of all of this is that many routines take a (type, host
   address, target address) triple as arguments to represent a value.
   The host address, if non-null, is supposed to contain an internal
   copy of the relevant data; otherwise, the program is to consult the
   target at the target address.  */

/* Assuming that VAL0 represents a pointer value, the result of
   dereferencing it.  Differs from value_ind in its treatment of
   dynamic-sized types.  */

struct value *
ada_value_ind (struct value *val0)
{
  struct value *val = unwrap_value (value_ind (val0));
  return ada_to_fixed_value (val);
}

/* The value resulting from dereferencing any "reference to"
   qualifiers on VAL0.  */

static struct value *
ada_coerce_ref (struct value *val0)
{
  if (TYPE_CODE (value_type (val0)) == TYPE_CODE_REF)
    {
      struct value *val = val0;
      val = coerce_ref (val);
      val = unwrap_value (val);
      return ada_to_fixed_value (val);
    }
  else
    return val0;
}

/* Return OFF rounded upward if necessary to a multiple of
   ALIGNMENT (a power of 2).  */

static unsigned int
align_value (unsigned int off, unsigned int alignment)
{
  return (off + alignment - 1) & ~(alignment - 1);
}

/* Return the bit alignment required for field #F of template type TYPE.  */

static unsigned int
field_alignment (struct type *type, int f)
{
  const char *name = TYPE_FIELD_NAME (type, f);
  int len;
  int align_offset;

  /* The field name should never be null, unless the debugging information
     is somehow malformed.  In this case, we assume the field does not
     require any alignment.  */
  if (name == NULL)
    return 1;

  len = strlen (name);

  if (!isdigit (name[len - 1]))
    return 1;

  if (isdigit (name[len - 2]))
    align_offset = len - 2;
  else
    align_offset = len - 1;

  if (align_offset < 7 || strncmp ("___XV", name + align_offset - 6, 5) != 0)
    return TARGET_CHAR_BIT;

  return atoi (name + align_offset) * TARGET_CHAR_BIT;
}

/* Find a symbol named NAME.  Ignores ambiguity.  */

struct symbol *
ada_find_any_symbol (const char *name)
{
  struct symbol *sym;

  sym = standard_lookup (name, get_selected_block (NULL), VAR_DOMAIN);
  if (sym != NULL && SYMBOL_CLASS (sym) == LOC_TYPEDEF)
    return sym;

  sym = standard_lookup (name, NULL, STRUCT_DOMAIN);
  return sym;
}

/* Find a type named NAME.  Ignores ambiguity.  */

struct type *
ada_find_any_type (const char *name)
{
  struct symbol *sym = ada_find_any_symbol (name);

  if (sym != NULL)
    return SYMBOL_TYPE (sym);

  return NULL;
}

/* Given a symbol NAME and its associated BLOCK, search all symbols
   for its ___XR counterpart, which is the ``renaming'' symbol
   associated to NAME.  Return this symbol if found, return
   NULL otherwise.  */

struct symbol *
ada_find_renaming_symbol (const char *name, struct block *block)
{
  const struct symbol *function_sym = block_function (block);
  char *rename;

  if (function_sym != NULL)
    {
      /* If the symbol is defined inside a function, NAME is not fully
         qualified.  This means we need to prepend the function name
         as well as adding the ``___XR'' suffix to build the name of
         the associated renaming symbol.  */
      char *function_name = SYMBOL_LINKAGE_NAME (function_sym);
      /* Function names sometimes contain suffixes used
         for instance to qualify nested subprograms.  When building
         the XR type name, we need to make sure that this suffix is
         not included.  So do not include any suffix in the function
         name length below.  */
      const int function_name_len = ada_name_prefix_len (function_name);
      const int rename_len = function_name_len + 2      /*  "__" */
        + strlen (name) + 6 /* "___XR\0" */ ;

      /* Strip the suffix if necessary.  */
      function_name[function_name_len] = '\0';

      /* Library-level functions are a special case, as GNAT adds
         a ``_ada_'' prefix to the function name to avoid namespace
         pollution.  However, the renaming symbol themselves do not
         have this prefix, so we need to skip this prefix if present.  */
      if (function_name_len > 5 /* "_ada_" */
          && strstr (function_name, "_ada_") == function_name)
        function_name = function_name + 5;

      rename = (char *) alloca (rename_len * sizeof (char));
      sprintf (rename, "%s__%s___XR", function_name, name);
    }
  else
    {
      const int rename_len = strlen (name) + 6;
      rename = (char *) alloca (rename_len * sizeof (char));
      sprintf (rename, "%s___XR", name);
    }

  return ada_find_any_symbol (rename);
}

/* Because of GNAT encoding conventions, several GDB symbols may match a
   given type name.  If the type denoted by TYPE0 is to be preferred to
   that of TYPE1 for purposes of type printing, return non-zero;
   otherwise return 0.  */

int
ada_prefer_type (struct type *type0, struct type *type1)
{
  if (type1 == NULL)
    return 1;
  else if (type0 == NULL)
    return 0;
  else if (TYPE_CODE (type1) == TYPE_CODE_VOID)
    return 1;
  else if (TYPE_CODE (type0) == TYPE_CODE_VOID)
    return 0;
  else if (TYPE_NAME (type1) == NULL && TYPE_NAME (type0) != NULL)
    return 1;
  else if (ada_is_packed_array_type (type0))
    return 1;
  else if (ada_is_array_descriptor_type (type0)
           && !ada_is_array_descriptor_type (type1))
    return 1;
  else if (ada_renaming_type (type0) != NULL
           && ada_renaming_type (type1) == NULL)
    return 1;
  return 0;
}

/* The name of TYPE, which is either its TYPE_NAME, or, if that is
   null, its TYPE_TAG_NAME.  Null if TYPE is null.  */

char *
ada_type_name (struct type *type)
{
  if (type == NULL)
    return NULL;
  else if (TYPE_NAME (type) != NULL)
    return TYPE_NAME (type);
  else
    return TYPE_TAG_NAME (type);
}

/* Find a parallel type to TYPE whose name is formed by appending
   SUFFIX to the name of TYPE.  */

struct type *
ada_find_parallel_type (struct type *type, const char *suffix)
{
  static char *name;
  static size_t name_len = 0;
  int len;
  char *typename = ada_type_name (type);

  if (typename == NULL)
    return NULL;

  len = strlen (typename);

  GROW_VECT (name, name_len, len + strlen (suffix) + 1);

  strcpy (name, typename);
  strcpy (name + len, suffix);

  return ada_find_any_type (name);
}


/* If TYPE is a variable-size record type, return the corresponding template
   type describing its fields.  Otherwise, return NULL.  */

static struct type *
dynamic_template_type (struct type *type)
{
  type = ada_check_typedef (type);

  if (type == NULL || TYPE_CODE (type) != TYPE_CODE_STRUCT
      || ada_type_name (type) == NULL)
    return NULL;
  else
    {
      int len = strlen (ada_type_name (type));
      if (len > 6 && strcmp (ada_type_name (type) + len - 6, "___XVE") == 0)
        return type;
      else
        return ada_find_parallel_type (type, "___XVE");
    }
}

/* Assuming that TEMPL_TYPE is a union or struct type, returns
   non-zero iff field FIELD_NUM of TEMPL_TYPE has dynamic size.  */

static int
is_dynamic_field (struct type *templ_type, int field_num)
{
  const char *name = TYPE_FIELD_NAME (templ_type, field_num);
  return name != NULL
    && TYPE_CODE (TYPE_FIELD_TYPE (templ_type, field_num)) == TYPE_CODE_PTR
    && strstr (name, "___XVL") != NULL;
}

/* The index of the variant field of TYPE, or -1 if TYPE does not
   represent a variant record type.  */

static int
variant_field_index (struct type *type)
{
  int f;

  if (type == NULL || TYPE_CODE (type) != TYPE_CODE_STRUCT)
    return -1;

  for (f = 0; f < TYPE_NFIELDS (type); f += 1)
    {
      if (ada_is_variant_part (type, f))
        return f;
    }
  return -1;
}

/* A record type with no fields.  */

static struct type *
empty_record (struct objfile *objfile)
{
  struct type *type = alloc_type (objfile);
  TYPE_CODE (type) = TYPE_CODE_STRUCT;
  TYPE_NFIELDS (type) = 0;
  TYPE_FIELDS (type) = NULL;
  TYPE_NAME (type) = "<empty>";
  TYPE_TAG_NAME (type) = NULL;
  TYPE_FLAGS (type) = 0;
  TYPE_LENGTH (type) = 0;
  return type;
}

/* An ordinary record type (with fixed-length fields) that describes
   the value of type TYPE at VALADDR or ADDRESS (see comments at
   the beginning of this section) VAL according to GNAT conventions.
   DVAL0 should describe the (portion of a) record that contains any
   necessary discriminants.  It should be NULL if value_type (VAL) is
   an outer-level type (i.e., as opposed to a branch of a variant.)  A
   variant field (unless unchecked) is replaced by a particular branch
   of the variant.

   If not KEEP_DYNAMIC_FIELDS, then all fields whose position or
   length are not statically known are discarded.  As a consequence,
   VALADDR, ADDRESS and DVAL0 are ignored.

   NOTE: Limitations: For now, we assume that dynamic fields and
   variants occupy whole numbers of bytes.  However, they need not be
   byte-aligned.  */

struct type *
ada_template_to_fixed_record_type_1 (struct type *type,
				     const gdb_byte *valaddr,
                                     CORE_ADDR address, struct value *dval0,
                                     int keep_dynamic_fields)
{
  struct value *mark = value_mark ();
  struct value *dval;
  struct type *rtype;
  int nfields, bit_len;
  int variant_field;
  long off;
  int fld_bit_len, bit_incr;
  int f;

  /* Compute the number of fields in this record type that are going
     to be processed: unless keep_dynamic_fields, this includes only
     fields whose position and length are static will be processed.  */
  if (keep_dynamic_fields)
    nfields = TYPE_NFIELDS (type);
  else
    {
      nfields = 0;
      while (nfields < TYPE_NFIELDS (type)
             && !ada_is_variant_part (type, nfields)
             && !is_dynamic_field (type, nfields))
        nfields++;
    }

  rtype = alloc_type (TYPE_OBJFILE (type));
  TYPE_CODE (rtype) = TYPE_CODE_STRUCT;
  INIT_CPLUS_SPECIFIC (rtype);
  TYPE_NFIELDS (rtype) = nfields;
  TYPE_FIELDS (rtype) = (struct field *)
    TYPE_ALLOC (rtype, nfields * sizeof (struct field));
  memset (TYPE_FIELDS (rtype), 0, sizeof (struct field) * nfields);
  TYPE_NAME (rtype) = ada_type_name (type);
  TYPE_TAG_NAME (rtype) = NULL;
  TYPE_FLAGS (rtype) |= TYPE_FLAG_FIXED_INSTANCE;

  off = 0;
  bit_len = 0;
  variant_field = -1;

  for (f = 0; f < nfields; f += 1)
    {
      off = align_value (off, field_alignment (type, f))
	+ TYPE_FIELD_BITPOS (type, f);
      TYPE_FIELD_BITPOS (rtype, f) = off;
      TYPE_FIELD_BITSIZE (rtype, f) = 0;

      if (ada_is_variant_part (type, f))
        {
          variant_field = f;
          fld_bit_len = bit_incr = 0;
        }
      else if (is_dynamic_field (type, f))
        {
          if (dval0 == NULL)
            dval = value_from_contents_and_address (rtype, valaddr, address);
          else
            dval = dval0;

          TYPE_FIELD_TYPE (rtype, f) =
            ada_to_fixed_type
            (ada_get_base_type
             (TYPE_TARGET_TYPE (TYPE_FIELD_TYPE (type, f))),
             cond_offset_host (valaddr, off / TARGET_CHAR_BIT),
             cond_offset_target (address, off / TARGET_CHAR_BIT), dval);
          TYPE_FIELD_NAME (rtype, f) = TYPE_FIELD_NAME (type, f);
          bit_incr = fld_bit_len =
            TYPE_LENGTH (TYPE_FIELD_TYPE (rtype, f)) * TARGET_CHAR_BIT;
        }
      else
        {
          TYPE_FIELD_TYPE (rtype, f) = TYPE_FIELD_TYPE (type, f);
          TYPE_FIELD_NAME (rtype, f) = TYPE_FIELD_NAME (type, f);
          if (TYPE_FIELD_BITSIZE (type, f) > 0)
            bit_incr = fld_bit_len =
              TYPE_FIELD_BITSIZE (rtype, f) = TYPE_FIELD_BITSIZE (type, f);
          else
            bit_incr = fld_bit_len =
              TYPE_LENGTH (TYPE_FIELD_TYPE (type, f)) * TARGET_CHAR_BIT;
        }
      if (off + fld_bit_len > bit_len)
        bit_len = off + fld_bit_len;
      off += bit_incr;
      TYPE_LENGTH (rtype) =
        align_value (bit_len, TARGET_CHAR_BIT) / TARGET_CHAR_BIT;
    }

  /* We handle the variant part, if any, at the end because of certain
     odd cases in which it is re-ordered so as NOT the last field of
     the record.  This can happen in the presence of representation
     clauses.  */
  if (variant_field >= 0)
    {
      struct type *branch_type;

      off = TYPE_FIELD_BITPOS (rtype, variant_field);

      if (dval0 == NULL)
        dval = value_from_contents_and_address (rtype, valaddr, address);
      else
        dval = dval0;

      branch_type =
        to_fixed_variant_branch_type
        (TYPE_FIELD_TYPE (type, variant_field),
         cond_offset_host (valaddr, off / TARGET_CHAR_BIT),
         cond_offset_target (address, off / TARGET_CHAR_BIT), dval);
      if (branch_type == NULL)
        {
          for (f = variant_field + 1; f < TYPE_NFIELDS (rtype); f += 1)
            TYPE_FIELDS (rtype)[f - 1] = TYPE_FIELDS (rtype)[f];
          TYPE_NFIELDS (rtype) -= 1;
        }
      else
        {
          TYPE_FIELD_TYPE (rtype, variant_field) = branch_type;
          TYPE_FIELD_NAME (rtype, variant_field) = "S";
          fld_bit_len =
            TYPE_LENGTH (TYPE_FIELD_TYPE (rtype, variant_field)) *
            TARGET_CHAR_BIT;
          if (off + fld_bit_len > bit_len)
            bit_len = off + fld_bit_len;
          TYPE_LENGTH (rtype) =
            align_value (bit_len, TARGET_CHAR_BIT) / TARGET_CHAR_BIT;
        }
    }

  /* According to exp_dbug.ads, the size of TYPE for variable-size records
     should contain the alignment of that record, which should be a strictly
     positive value.  If null or negative, then something is wrong, most
     probably in the debug info.  In that case, we don't round up the size
     of the resulting type. If this record is not part of another structure,
     the current RTYPE length might be good enough for our purposes.  */
  if (TYPE_LENGTH (type) <= 0)
    {
      if (TYPE_NAME (rtype))
	warning (_("Invalid type size for `%s' detected: %d."),
		 TYPE_NAME (rtype), TYPE_LENGTH (type));
      else
	warning (_("Invalid type size for <unnamed> detected: %d."),
		 TYPE_LENGTH (type));
    }
  else
    {
      TYPE_LENGTH (rtype) = align_value (TYPE_LENGTH (rtype),
                                         TYPE_LENGTH (type));
    }

  value_free_to_mark (mark);
  if (TYPE_LENGTH (rtype) > varsize_limit)
    error (_("record type with dynamic size is larger than varsize-limit"));
  return rtype;
}

/* As for ada_template_to_fixed_record_type_1 with KEEP_DYNAMIC_FIELDS
   of 1.  */

static struct type *
template_to_fixed_record_type (struct type *type, const gdb_byte *valaddr,
                               CORE_ADDR address, struct value *dval0)
{
  return ada_template_to_fixed_record_type_1 (type, valaddr,
                                              address, dval0, 1);
}

/* An ordinary record type in which ___XVL-convention fields and
   ___XVU- and ___XVN-convention field types in TYPE0 are replaced with
   static approximations, containing all possible fields.  Uses
   no runtime values.  Useless for use in values, but that's OK,
   since the results are used only for type determinations.   Works on both
   structs and unions.  Representation note: to save space, we memorize
   the result of this function in the TYPE_TARGET_TYPE of the
   template type.  */

static struct type *
template_to_static_fixed_type (struct type *type0)
{
  struct type *type;
  int nfields;
  int f;

  if (TYPE_TARGET_TYPE (type0) != NULL)
    return TYPE_TARGET_TYPE (type0);

  nfields = TYPE_NFIELDS (type0);
  type = type0;

  for (f = 0; f < nfields; f += 1)
    {
      struct type *field_type = ada_check_typedef (TYPE_FIELD_TYPE (type0, f));
      struct type *new_type;

      if (is_dynamic_field (type0, f))
        new_type = to_static_fixed_type (TYPE_TARGET_TYPE (field_type));
      else
        new_type = to_static_fixed_type (field_type);
      if (type == type0 && new_type != field_type)
        {
          TYPE_TARGET_TYPE (type0) = type = alloc_type (TYPE_OBJFILE (type0));
          TYPE_CODE (type) = TYPE_CODE (type0);
          INIT_CPLUS_SPECIFIC (type);
          TYPE_NFIELDS (type) = nfields;
          TYPE_FIELDS (type) = (struct field *)
            TYPE_ALLOC (type, nfields * sizeof (struct field));
          memcpy (TYPE_FIELDS (type), TYPE_FIELDS (type0),
                  sizeof (struct field) * nfields);
          TYPE_NAME (type) = ada_type_name (type0);
          TYPE_TAG_NAME (type) = NULL;
          TYPE_FLAGS (type) |= TYPE_FLAG_FIXED_INSTANCE;
          TYPE_LENGTH (type) = 0;
        }
      TYPE_FIELD_TYPE (type, f) = new_type;
      TYPE_FIELD_NAME (type, f) = TYPE_FIELD_NAME (type0, f);
    }
  return type;
}

/* Given an object of type TYPE whose contents are at VALADDR and
   whose address in memory is ADDRESS, returns a revision of TYPE --
   a non-dynamic-sized record with a variant part -- in which
   the variant part is replaced with the appropriate branch.  Looks
   for discriminant values in DVAL0, which can be NULL if the record
   contains the necessary discriminant values.  */

static struct type *
to_record_with_fixed_variant_part (struct type *type, const gdb_byte *valaddr,
                                   CORE_ADDR address, struct value *dval0)
{
  struct value *mark = value_mark ();
  struct value *dval;
  struct type *rtype;
  struct type *branch_type;
  int nfields = TYPE_NFIELDS (type);
  int variant_field = variant_field_index (type);

  if (variant_field == -1)
    return type;

  if (dval0 == NULL)
    dval = value_from_contents_and_address (type, valaddr, address);
  else
    dval = dval0;

  rtype = alloc_type (TYPE_OBJFILE (type));
  TYPE_CODE (rtype) = TYPE_CODE_STRUCT;
  INIT_CPLUS_SPECIFIC (rtype);
  TYPE_NFIELDS (rtype) = nfields;
  TYPE_FIELDS (rtype) =
    (struct field *) TYPE_ALLOC (rtype, nfields * sizeof (struct field));
  memcpy (TYPE_FIELDS (rtype), TYPE_FIELDS (type),
          sizeof (struct field) * nfields);
  TYPE_NAME (rtype) = ada_type_name (type);
  TYPE_TAG_NAME (rtype) = NULL;
  TYPE_FLAGS (rtype) |= TYPE_FLAG_FIXED_INSTANCE;
  TYPE_LENGTH (rtype) = TYPE_LENGTH (type);

  branch_type = to_fixed_variant_branch_type
    (TYPE_FIELD_TYPE (type, variant_field),
     cond_offset_host (valaddr,
                       TYPE_FIELD_BITPOS (type, variant_field)
                       / TARGET_CHAR_BIT),
     cond_offset_target (address,
                         TYPE_FIELD_BITPOS (type, variant_field)
                         / TARGET_CHAR_BIT), dval);
  if (branch_type == NULL)
    {
      int f;
      for (f = variant_field + 1; f < nfields; f += 1)
        TYPE_FIELDS (rtype)[f - 1] = TYPE_FIELDS (rtype)[f];
      TYPE_NFIELDS (rtype) -= 1;
    }
  else
    {
      TYPE_FIELD_TYPE (rtype, variant_field) = branch_type;
      TYPE_FIELD_NAME (rtype, variant_field) = "S";
      TYPE_FIELD_BITSIZE (rtype, variant_field) = 0;
      TYPE_LENGTH (rtype) += TYPE_LENGTH (branch_type);
    }
  TYPE_LENGTH (rtype) -= TYPE_LENGTH (TYPE_FIELD_TYPE (type, variant_field));

  value_free_to_mark (mark);
  return rtype;
}

/* An ordinary record type (with fixed-length fields) that describes
   the value at (TYPE0, VALADDR, ADDRESS) [see explanation at
   beginning of this section].   Any necessary discriminants' values
   should be in DVAL, a record value; it may be NULL if the object
   at ADDR itself contains any necessary discriminant values.
   Additionally, VALADDR and ADDRESS may also be NULL if no discriminant
   values from the record are needed.  Except in the case that DVAL,
   VALADDR, and ADDRESS are all 0 or NULL, a variant field (unless
   unchecked) is replaced by a particular branch of the variant.

   NOTE: the case in which DVAL and VALADDR are NULL and ADDRESS is 0
   is questionable and may be removed.  It can arise during the
   processing of an unconstrained-array-of-record type where all the
   variant branches have exactly the same size.  This is because in
   such cases, the compiler does not bother to use the XVS convention
   when encoding the record.  I am currently dubious of this
   shortcut and suspect the compiler should be altered.  FIXME.  */

static struct type *
to_fixed_record_type (struct type *type0, const gdb_byte *valaddr,
                      CORE_ADDR address, struct value *dval)
{
  struct type *templ_type;

  if (TYPE_FLAGS (type0) & TYPE_FLAG_FIXED_INSTANCE)
    return type0;

  templ_type = dynamic_template_type (type0);

  if (templ_type != NULL)
    return template_to_fixed_record_type (templ_type, valaddr, address, dval);
  else if (variant_field_index (type0) >= 0)
    {
      if (dval == NULL && valaddr == NULL && address == 0)
        return type0;
      return to_record_with_fixed_variant_part (type0, valaddr, address,
                                                dval);
    }
  else
    {
      TYPE_FLAGS (type0) |= TYPE_FLAG_FIXED_INSTANCE;
      return type0;
    }

}

/* An ordinary record type (with fixed-length fields) that describes
   the value at (VAR_TYPE0, VALADDR, ADDRESS), where VAR_TYPE0 is a
   union type.  Any necessary discriminants' values should be in DVAL,
   a record value.  That is, this routine selects the appropriate
   branch of the union at ADDR according to the discriminant value
   indicated in the union's type name.  */

static struct type *
to_fixed_variant_branch_type (struct type *var_type0, const gdb_byte *valaddr,
                              CORE_ADDR address, struct value *dval)
{
  int which;
  struct type *templ_type;
  struct type *var_type;

  if (TYPE_CODE (var_type0) == TYPE_CODE_PTR)
    var_type = TYPE_TARGET_TYPE (var_type0);
  else
    var_type = var_type0;

  templ_type = ada_find_parallel_type (var_type, "___XVU");

  if (templ_type != NULL)
    var_type = templ_type;

  which =
    ada_which_variant_applies (var_type,
                               value_type (dval), value_contents (dval));

  if (which < 0)
    return empty_record (TYPE_OBJFILE (var_type));
  else if (is_dynamic_field (var_type, which))
    return to_fixed_record_type
      (TYPE_TARGET_TYPE (TYPE_FIELD_TYPE (var_type, which)),
       valaddr, address, dval);
  else if (variant_field_index (TYPE_FIELD_TYPE (var_type, which)) >= 0)
    return
      to_fixed_record_type
      (TYPE_FIELD_TYPE (var_type, which), valaddr, address, dval);
  else
    return TYPE_FIELD_TYPE (var_type, which);
}

/* Assuming that TYPE0 is an array type describing the type of a value
   at ADDR, and that DVAL describes a record containing any
   discriminants used in TYPE0, returns a type for the value that
   contains no dynamic components (that is, no components whose sizes
   are determined by run-time quantities).  Unless IGNORE_TOO_BIG is
   true, gives an error message if the resulting type's size is over
   varsize_limit.  */

static struct type *
to_fixed_array_type (struct type *type0, struct value *dval,
                     int ignore_too_big)
{
  struct type *index_type_desc;
  struct type *result;

  if (ada_is_packed_array_type (type0)  /* revisit? */
      || (TYPE_FLAGS (type0) & TYPE_FLAG_FIXED_INSTANCE))
    return type0;

  index_type_desc = ada_find_parallel_type (type0, "___XA");
  if (index_type_desc == NULL)
    {
      struct type *elt_type0 = ada_check_typedef (TYPE_TARGET_TYPE (type0));
      /* NOTE: elt_type---the fixed version of elt_type0---should never
         depend on the contents of the array in properly constructed
         debugging data.  */
      /* Create a fixed version of the array element type.
         We're not providing the address of an element here,
         and thus the actual object value cannot be inspected to do
         the conversion.  This should not be a problem, since arrays of
         unconstrained objects are not allowed.  In particular, all
         the elements of an array of a tagged type should all be of
         the same type specified in the debugging info.  No need to
         consult the object tag.  */
      struct type *elt_type = ada_to_fixed_type (elt_type0, 0, 0, dval);

      if (elt_type0 == elt_type)
        result = type0;
      else
        result = create_array_type (alloc_type (TYPE_OBJFILE (type0)),
                                    elt_type, TYPE_INDEX_TYPE (type0));
    }
  else
    {
      int i;
      struct type *elt_type0;

      elt_type0 = type0;
      for (i = TYPE_NFIELDS (index_type_desc); i > 0; i -= 1)
        elt_type0 = TYPE_TARGET_TYPE (elt_type0);

      /* NOTE: result---the fixed version of elt_type0---should never
         depend on the contents of the array in properly constructed
         debugging data.  */
      /* Create a fixed version of the array element type.
         We're not providing the address of an element here,
         and thus the actual object value cannot be inspected to do
         the conversion.  This should not be a problem, since arrays of
         unconstrained objects are not allowed.  In particular, all
         the elements of an array of a tagged type should all be of
         the same type specified in the debugging info.  No need to
         consult the object tag.  */
      result = ada_to_fixed_type (ada_check_typedef (elt_type0), 0, 0, dval);
      for (i = TYPE_NFIELDS (index_type_desc) - 1; i >= 0; i -= 1)
        {
          struct type *range_type =
            to_fixed_range_type (TYPE_FIELD_NAME (index_type_desc, i),
                                 dval, TYPE_OBJFILE (type0));
          result = create_array_type (alloc_type (TYPE_OBJFILE (type0)),
                                      result, range_type);
        }
      if (!ignore_too_big && TYPE_LENGTH (result) > varsize_limit)
        error (_("array type with dynamic size is larger than varsize-limit"));
    }

  TYPE_FLAGS (result) |= TYPE_FLAG_FIXED_INSTANCE;
  return result;
}


/* A standard type (containing no dynamically sized components)
   corresponding to TYPE for the value (TYPE, VALADDR, ADDRESS)
   DVAL describes a record containing any discriminants used in TYPE0,
   and may be NULL if there are none, or if the object of type TYPE at
   ADDRESS or in VALADDR contains these discriminants.
   
   In the case of tagged types, this function attempts to locate the object's
   tag and use it to compute the actual type.  However, when ADDRESS is null,
   we cannot use it to determine the location of the tag, and therefore
   compute the tagged type's actual type.  So we return the tagged type
   without consulting the tag.  */
   
struct type *
ada_to_fixed_type (struct type *type, const gdb_byte *valaddr,
                   CORE_ADDR address, struct value *dval)
{
  type = ada_check_typedef (type);
  switch (TYPE_CODE (type))
    {
    default:
      return type;
    case TYPE_CODE_STRUCT:
      {
        struct type *static_type = to_static_fixed_type (type);

        /* If STATIC_TYPE is a tagged type and we know the object's address,
           then we can determine its tag, and compute the object's actual
           type from there.  */

        if (address != 0 && ada_is_tagged_type (static_type, 0))
          {
            struct type *real_type =
              type_from_tag (value_tag_from_contents_and_address (static_type,
                                                                  valaddr,
                                                                  address));
            if (real_type != NULL)
              type = real_type;
          }
        return to_fixed_record_type (type, valaddr, address, NULL);
      }
    case TYPE_CODE_ARRAY:
      return to_fixed_array_type (type, dval, 1);
    case TYPE_CODE_UNION:
      if (dval == NULL)
        return type;
      else
        return to_fixed_variant_branch_type (type, valaddr, address, dval);
    }
}

/* A standard (static-sized) type corresponding as well as possible to
   TYPE0, but based on no runtime data.  */

static struct type *
to_static_fixed_type (struct type *type0)
{
  struct type *type;

  if (type0 == NULL)
    return NULL;

  if (TYPE_FLAGS (type0) & TYPE_FLAG_FIXED_INSTANCE)
    return type0;

  type0 = ada_check_typedef (type0);

  switch (TYPE_CODE (type0))
    {
    default:
      return type0;
    case TYPE_CODE_STRUCT:
      type = dynamic_template_type (type0);
      if (type != NULL)
        return template_to_static_fixed_type (type);
      else
        return template_to_static_fixed_type (type0);
    case TYPE_CODE_UNION:
      type = ada_find_parallel_type (type0, "___XVU");
      if (type != NULL)
        return template_to_static_fixed_type (type);
      else
        return template_to_static_fixed_type (type0);
    }
}

/* A static approximation of TYPE with all type wrappers removed.  */

static struct type *
static_unwrap_type (struct type *type)
{
  if (ada_is_aligner_type (type))
    {
      struct type *type1 = TYPE_FIELD_TYPE (ada_check_typedef (type), 0);
      if (ada_type_name (type1) == NULL)
        TYPE_NAME (type1) = ada_type_name (type);

      return static_unwrap_type (type1);
    }
  else
    {
      struct type *raw_real_type = ada_get_base_type (type);
      if (raw_real_type == type)
        return type;
      else
        return to_static_fixed_type (raw_real_type);
    }
}

/* In some cases, incomplete and private types require
   cross-references that are not resolved as records (for example,
      type Foo;
      type FooP is access Foo;
      V: FooP;
      type Foo is array ...;
   ).  In these cases, since there is no mechanism for producing
   cross-references to such types, we instead substitute for FooP a
   stub enumeration type that is nowhere resolved, and whose tag is
   the name of the actual type.  Call these types "non-record stubs".  */

/* A type equivalent to TYPE that is not a non-record stub, if one
   exists, otherwise TYPE.  */

struct type *
ada_check_typedef (struct type *type)
{
  CHECK_TYPEDEF (type);
  if (type == NULL || TYPE_CODE (type) != TYPE_CODE_ENUM
      || !TYPE_STUB (type)
      || TYPE_TAG_NAME (type) == NULL)
    return type;
  else
    {
      char *name = TYPE_TAG_NAME (type);
      struct type *type1 = ada_find_any_type (name);
      return (type1 == NULL) ? type : type1;
    }
}

/* A value representing the data at VALADDR/ADDRESS as described by
   type TYPE0, but with a standard (static-sized) type that correctly
   describes it.  If VAL0 is not NULL and TYPE0 already is a standard
   type, then return VAL0 [this feature is simply to avoid redundant
   creation of struct values].  */

static struct value *
ada_to_fixed_value_create (struct type *type0, CORE_ADDR address,
                           struct value *val0)
{
  struct type *type = ada_to_fixed_type (type0, 0, address, NULL);
  if (type == type0 && val0 != NULL)
    return val0;
  else
    return value_from_contents_and_address (type, 0, address);
}

/* A value representing VAL, but with a standard (static-sized) type
   that correctly describes it.  Does not necessarily create a new
   value.  */

static struct value *
ada_to_fixed_value (struct value *val)
{
  return ada_to_fixed_value_create (value_type (val),
                                    VALUE_ADDRESS (val) + value_offset (val),
                                    val);
}

/* A value representing VAL, but with a standard (static-sized) type
   chosen to approximate the real type of VAL as well as possible, but
   without consulting any runtime values.  For Ada dynamic-sized
   types, therefore, the type of the result is likely to be inaccurate.  */

struct value *
ada_to_static_fixed_value (struct value *val)
{
  struct type *type =
    to_static_fixed_type (static_unwrap_type (value_type (val)));
  if (type == value_type (val))
    return val;
  else
    return coerce_unspec_val_to_type (val, type);
}


/* Attributes */

/* Table mapping attribute numbers to names.
   NOTE: Keep up to date with enum ada_attribute definition in ada-lang.h.  */

static const char *attribute_names[] = {
  "<?>",

  "first",
  "last",
  "length",
  "image",
  "max",
  "min",
  "modulus",
  "pos",
  "size",
  "tag",
  "val",
  0
};

const char *
ada_attribute_name (enum exp_opcode n)
{
  if (n >= OP_ATR_FIRST && n <= (int) OP_ATR_VAL)
    return attribute_names[n - OP_ATR_FIRST + 1];
  else
    return attribute_names[0];
}

/* Evaluate the 'POS attribute applied to ARG.  */

static LONGEST
pos_atr (struct value *arg)
{
  struct type *type = value_type (arg);

  if (!discrete_type_p (type))
    error (_("'POS only defined on discrete types"));

  if (TYPE_CODE (type) == TYPE_CODE_ENUM)
    {
      int i;
      LONGEST v = value_as_long (arg);

      for (i = 0; i < TYPE_NFIELDS (type); i += 1)
        {
          if (v == TYPE_FIELD_BITPOS (type, i))
            return i;
        }
      error (_("enumeration value is invalid: can't find 'POS"));
    }
  else
    return value_as_long (arg);
}

static struct value *
value_pos_atr (struct value *arg)
{
  return value_from_longest (builtin_type_int, pos_atr (arg));
}

/* Evaluate the TYPE'VAL attribute applied to ARG.  */

static struct value *
value_val_atr (struct type *type, struct value *arg)
{
  if (!discrete_type_p (type))
    error (_("'VAL only defined on discrete types"));
  if (!integer_type_p (value_type (arg)))
    error (_("'VAL requires integral argument"));

  if (TYPE_CODE (type) == TYPE_CODE_ENUM)
    {
      long pos = value_as_long (arg);
      if (pos < 0 || pos >= TYPE_NFIELDS (type))
        error (_("argument to 'VAL out of range"));
      return value_from_longest (type, TYPE_FIELD_BITPOS (type, pos));
    }
  else
    return value_from_longest (type, value_as_long (arg));
}


                                /* Evaluation */

/* True if TYPE appears to be an Ada character type.
   [At the moment, this is true only for Character and Wide_Character;
   It is a heuristic test that could stand improvement].  */

int
ada_is_character_type (struct type *type)
{
  const char *name = ada_type_name (type);
  return
    name != NULL
    && (TYPE_CODE (type) == TYPE_CODE_CHAR
        || TYPE_CODE (type) == TYPE_CODE_INT
        || TYPE_CODE (type) == TYPE_CODE_RANGE)
    && (strcmp (name, "character") == 0
        || strcmp (name, "wide_character") == 0
        || strcmp (name, "unsigned char") == 0);
}

/* True if TYPE appears to be an Ada string type.  */

int
ada_is_string_type (struct type *type)
{
  type = ada_check_typedef (type);
  if (type != NULL
      && TYPE_CODE (type) != TYPE_CODE_PTR
      && (ada_is_simple_array_type (type)
          || ada_is_array_descriptor_type (type))
      && ada_array_arity (type) == 1)
    {
      struct type *elttype = ada_array_element_type (type, 1);

      return ada_is_character_type (elttype);
    }
  else
    return 0;
}


/* True if TYPE is a struct type introduced by the compiler to force the
   alignment of a value.  Such types have a single field with a
   distinctive name.  */

int
ada_is_aligner_type (struct type *type)
{
  type = ada_check_typedef (type);

  /* If we can find a parallel XVS type, then the XVS type should
     be used instead of this type.  And hence, this is not an aligner
     type.  */
  if (ada_find_parallel_type (type, "___XVS") != NULL)
    return 0;

  return (TYPE_CODE (type) == TYPE_CODE_STRUCT
          && TYPE_NFIELDS (type) == 1
          && strcmp (TYPE_FIELD_NAME (type, 0), "F") == 0);
}

/* If there is an ___XVS-convention type parallel to SUBTYPE, return
   the parallel type.  */

struct type *
ada_get_base_type (struct type *raw_type)
{
  struct type *real_type_namer;
  struct type *raw_real_type;

  if (raw_type == NULL || TYPE_CODE (raw_type) != TYPE_CODE_STRUCT)
    return raw_type;

  real_type_namer = ada_find_parallel_type (raw_type, "___XVS");
  if (real_type_namer == NULL
      || TYPE_CODE (real_type_namer) != TYPE_CODE_STRUCT
      || TYPE_NFIELDS (real_type_namer) != 1)
    return raw_type;

  raw_real_type = ada_find_any_type (TYPE_FIELD_NAME (real_type_namer, 0));
  if (raw_real_type == NULL)
    return raw_type;
  else
    return raw_real_type;
}

/* The type of value designated by TYPE, with all aligners removed.  */

struct type *
ada_aligned_type (struct type *type)
{
  if (ada_is_aligner_type (type))
    return ada_aligned_type (TYPE_FIELD_TYPE (type, 0));
  else
    return ada_get_base_type (type);
}


/* The address of the aligned value in an object at address VALADDR
   having type TYPE.  Assumes ada_is_aligner_type (TYPE).  */

const gdb_byte *
ada_aligned_value_addr (struct type *type, const gdb_byte *valaddr)
{
  if (ada_is_aligner_type (type))
    return ada_aligned_value_addr (TYPE_FIELD_TYPE (type, 0),
                                   valaddr +
                                   TYPE_FIELD_BITPOS (type,
                                                      0) / TARGET_CHAR_BIT);
  else
    return valaddr;
}



/* The printed representation of an enumeration literal with encoded
   name NAME.  The value is good to the next call of ada_enum_name.  */
const char *
ada_enum_name (const char *name)
{
  static char *result;
  static size_t result_len = 0;
  char *tmp;

  /* First, unqualify the enumeration name:
     1. Search for the last '.' character.  If we find one, then skip
     all the preceeding characters, the unqualified name starts
     right after that dot.
     2. Otherwise, we may be debugging on a target where the compiler
     translates dots into "__".  Search forward for double underscores,
     but stop searching when we hit an overloading suffix, which is
     of the form "__" followed by digits.  */

  tmp = strrchr (name, '.');
  if (tmp != NULL)
    name = tmp + 1;
  else
    {
      while ((tmp = strstr (name, "__")) != NULL)
        {
          if (isdigit (tmp[2]))
            break;
          else
            name = tmp + 2;
        }
    }

  if (name[0] == 'Q')
    {
      int v;
      if (name[1] == 'U' || name[1] == 'W')
        {
          if (sscanf (name + 2, "%x", &v) != 1)
            return name;
        }
      else
        return name;

      GROW_VECT (result, result_len, 16);
      if (isascii (v) && isprint (v))
        sprintf (result, "'%c'", v);
      else if (name[1] == 'U')
        sprintf (result, "[\"%02x\"]", v);
      else
        sprintf (result, "[\"%04x\"]", v);

      return result;
    }
  else
    {
      tmp = strstr (name, "__");
      if (tmp == NULL)
	tmp = strstr (name, "$");
      if (tmp != NULL)
        {
          GROW_VECT (result, result_len, tmp - name + 1);
          strncpy (result, name, tmp - name);
          result[tmp - name] = '\0';
          return result;
        }

      return name;
    }
}

static struct value *
evaluate_subexp (struct type *expect_type, struct expression *exp, int *pos,
                 enum noside noside)
{
  return (*exp->language_defn->la_exp_desc->evaluate_exp)
    (expect_type, exp, pos, noside);
}

/* Evaluate the subexpression of EXP starting at *POS as for
   evaluate_type, updating *POS to point just past the evaluated
   expression.  */

static struct value *
evaluate_subexp_type (struct expression *exp, int *pos)
{
  return (*exp->language_defn->la_exp_desc->evaluate_exp)
    (NULL_TYPE, exp, pos, EVAL_AVOID_SIDE_EFFECTS);
}

/* If VAL is wrapped in an aligner or subtype wrapper, return the
   value it wraps.  */

static struct value *
unwrap_value (struct value *val)
{
  struct type *type = ada_check_typedef (value_type (val));
  if (ada_is_aligner_type (type))
    {
      struct value *v = value_struct_elt (&val, NULL, "F",
                                          NULL, "internal structure");
      struct type *val_type = ada_check_typedef (value_type (v));
      if (ada_type_name (val_type) == NULL)
        TYPE_NAME (val_type) = ada_type_name (type);

      return unwrap_value (v);
    }
  else
    {
      struct type *raw_real_type =
        ada_check_typedef (ada_get_base_type (type));

      if (type == raw_real_type)
        return val;

      return
        coerce_unspec_val_to_type
        (val, ada_to_fixed_type (raw_real_type, 0,
                                 VALUE_ADDRESS (val) + value_offset (val),
                                 NULL));
    }
}

static struct value *
cast_to_fixed (struct type *type, struct value *arg)
{
  LONGEST val;

  if (type == value_type (arg))
    return arg;
  else if (ada_is_fixed_point_type (value_type (arg)))
    val = ada_float_to_fixed (type,
                              ada_fixed_to_float (value_type (arg),
                                                  value_as_long (arg)));
  else
    {
      DOUBLEST argd =
        value_as_double (value_cast (builtin_type_double, value_copy (arg)));
      val = ada_float_to_fixed (type, argd);
    }

  return value_from_longest (type, val);
}

static struct value *
cast_from_fixed_to_double (struct value *arg)
{
  DOUBLEST val = ada_fixed_to_float (value_type (arg),
                                     value_as_long (arg));
  return value_from_double (builtin_type_double, val);
}

/* Coerce VAL as necessary for assignment to an lval of type TYPE, and
   return the converted value.  */

static struct value *
coerce_for_assign (struct type *type, struct value *val)
{
  struct type *type2 = value_type (val);
  if (type == type2)
    return val;

  type2 = ada_check_typedef (type2);
  type = ada_check_typedef (type);

  if (TYPE_CODE (type2) == TYPE_CODE_PTR
      && TYPE_CODE (type) == TYPE_CODE_ARRAY)
    {
      val = ada_value_ind (val);
      type2 = value_type (val);
    }

  if (TYPE_CODE (type2) == TYPE_CODE_ARRAY
      && TYPE_CODE (type) == TYPE_CODE_ARRAY)
    {
      if (TYPE_LENGTH (type2) != TYPE_LENGTH (type)
          || TYPE_LENGTH (TYPE_TARGET_TYPE (type2))
          != TYPE_LENGTH (TYPE_TARGET_TYPE (type2)))
        error (_("Incompatible types in assignment"));
      deprecated_set_value_type (val, type);
    }
  return val;
}

static struct value *
ada_value_binop (struct value *arg1, struct value *arg2, enum exp_opcode op)
{
  struct value *val;
  struct type *type1, *type2;
  LONGEST v, v1, v2;

  arg1 = coerce_ref (arg1);
  arg2 = coerce_ref (arg2);
  type1 = base_type (ada_check_typedef (value_type (arg1)));
  type2 = base_type (ada_check_typedef (value_type (arg2)));

  if (TYPE_CODE (type1) != TYPE_CODE_INT
      || TYPE_CODE (type2) != TYPE_CODE_INT)
    return value_binop (arg1, arg2, op);

  switch (op)
    {
    case BINOP_MOD:
    case BINOP_DIV:
    case BINOP_REM:
      break;
    default:
      return value_binop (arg1, arg2, op);
    }

  v2 = value_as_long (arg2);
  if (v2 == 0)
    error (_("second operand of %s must not be zero."), op_string (op));

  if (TYPE_UNSIGNED (type1) || op == BINOP_MOD)
    return value_binop (arg1, arg2, op);

  v1 = value_as_long (arg1);
  switch (op)
    {
    case BINOP_DIV:
      v = v1 / v2;
      if (!TRUNCATION_TOWARDS_ZERO && v1 * (v1 % v2) < 0)
        v += v > 0 ? -1 : 1;
      break;
    case BINOP_REM:
      v = v1 % v2;
      if (v * v1 < 0)
        v -= v2;
      break;
    default:
      /* Should not reach this point.  */
      v = 0;
    }

  val = allocate_value (type1);
  store_unsigned_integer (value_contents_raw (val),
                          TYPE_LENGTH (value_type (val)), v);
  return val;
}

static int
ada_value_equal (struct value *arg1, struct value *arg2)
{
  if (ada_is_direct_array_type (value_type (arg1))
      || ada_is_direct_array_type (value_type (arg2)))
    {
      arg1 = ada_coerce_to_simple_array (arg1);
      arg2 = ada_coerce_to_simple_array (arg2);
      if (TYPE_CODE (value_type (arg1)) != TYPE_CODE_ARRAY
          || TYPE_CODE (value_type (arg2)) != TYPE_CODE_ARRAY)
        error (_("Attempt to compare array with non-array"));
      /* FIXME: The following works only for types whose
         representations use all bits (no padding or undefined bits)
         and do not have user-defined equality.  */
      return
        TYPE_LENGTH (value_type (arg1)) == TYPE_LENGTH (value_type (arg2))
        && memcmp (value_contents (arg1), value_contents (arg2),
                   TYPE_LENGTH (value_type (arg1))) == 0;
    }
  return value_equal (arg1, arg2);
}

/* Total number of component associations in the aggregate starting at
   index PC in EXP.  Assumes that index PC is the start of an
   OP_AGGREGATE. */

static int
num_component_specs (struct expression *exp, int pc)
{
  int n, m, i;
  m = exp->elts[pc + 1].longconst;
  pc += 3;
  n = 0;
  for (i = 0; i < m; i += 1)
    {
      switch (exp->elts[pc].opcode) 
	{
	default:
	  n += 1;
	  break;
	case OP_CHOICES:
	  n += exp->elts[pc + 1].longconst;
	  break;
	}
      ada_evaluate_subexp (NULL, exp, &pc, EVAL_SKIP);
    }
  return n;
}

/* Assign the result of evaluating EXP starting at *POS to the INDEXth 
   component of LHS (a simple array or a record), updating *POS past
   the expression, assuming that LHS is contained in CONTAINER.  Does
   not modify the inferior's memory, nor does it modify LHS (unless
   LHS == CONTAINER).  */

static void
assign_component (struct value *container, struct value *lhs, LONGEST index,
		  struct expression *exp, int *pos)
{
  struct value *mark = value_mark ();
  struct value *elt;
  if (TYPE_CODE (value_type (lhs)) == TYPE_CODE_ARRAY)
    {
      struct value *index_val = value_from_longest (builtin_type_int, index);
      elt = unwrap_value (ada_value_subscript (lhs, 1, &index_val));
    }
  else
    {
      elt = ada_index_struct_field (index, lhs, 0, value_type (lhs));
      elt = ada_to_fixed_value (unwrap_value (elt));
    }

  if (exp->elts[*pos].opcode == OP_AGGREGATE)
    assign_aggregate (container, elt, exp, pos, EVAL_NORMAL);
  else
    value_assign_to_component (container, elt, 
			       ada_evaluate_subexp (NULL, exp, pos, 
						    EVAL_NORMAL));

  value_free_to_mark (mark);
}

/* Assuming that LHS represents an lvalue having a record or array
   type, and EXP->ELTS[*POS] is an OP_AGGREGATE, evaluate an assignment
   of that aggregate's value to LHS, advancing *POS past the
   aggregate.  NOSIDE is as for evaluate_subexp.  CONTAINER is an
   lvalue containing LHS (possibly LHS itself).  Does not modify
   the inferior's memory, nor does it modify the contents of 
   LHS (unless == CONTAINER).  Returns the modified CONTAINER. */

static struct value *
assign_aggregate (struct value *container, 
		  struct value *lhs, struct expression *exp, 
		  int *pos, enum noside noside)
{
  struct type *lhs_type;
  int n = exp->elts[*pos+1].longconst;
  LONGEST low_index, high_index;
  int num_specs;
  LONGEST *indices;
  int max_indices, num_indices;
  int is_array_aggregate;
  int i;
  struct value *mark = value_mark ();

  *pos += 3;
  if (noside != EVAL_NORMAL)
    {
      int i;
      for (i = 0; i < n; i += 1)
	ada_evaluate_subexp (NULL, exp, pos, noside);
      return container;
    }

  container = ada_coerce_ref (container);
  if (ada_is_direct_array_type (value_type (container)))
    container = ada_coerce_to_simple_array (container);
  lhs = ada_coerce_ref (lhs);
  if (!deprecated_value_modifiable (lhs))
    error (_("Left operand of assignment is not a modifiable lvalue."));

  lhs_type = value_type (lhs);
  if (ada_is_direct_array_type (lhs_type))
    {
      lhs = ada_coerce_to_simple_array (lhs);
      lhs_type = value_type (lhs);
      low_index = TYPE_ARRAY_LOWER_BOUND_VALUE (lhs_type);
      high_index = TYPE_ARRAY_UPPER_BOUND_VALUE (lhs_type);
      is_array_aggregate = 1;
    }
  else if (TYPE_CODE (lhs_type) == TYPE_CODE_STRUCT)
    {
      low_index = 0;
      high_index = num_visible_fields (lhs_type) - 1;
      is_array_aggregate = 0;
    }
  else
    error (_("Left-hand side must be array or record."));

  num_specs = num_component_specs (exp, *pos - 3);
  max_indices = 4 * num_specs + 4;
  indices = alloca (max_indices * sizeof (indices[0]));
  indices[0] = indices[1] = low_index - 1;
  indices[2] = indices[3] = high_index + 1;
  num_indices = 4;

  for (i = 0; i < n; i += 1)
    {
      switch (exp->elts[*pos].opcode)
	{
	case OP_CHOICES:
	  aggregate_assign_from_choices (container, lhs, exp, pos, indices, 
					 &num_indices, max_indices,
					 low_index, high_index);
	  break;
	case OP_POSITIONAL:
	  aggregate_assign_positional (container, lhs, exp, pos, indices,
				       &num_indices, max_indices,
				       low_index, high_index);
	  break;
	case OP_OTHERS:
	  if (i != n-1)
	    error (_("Misplaced 'others' clause"));
	  aggregate_assign_others (container, lhs, exp, pos, indices, 
				   num_indices, low_index, high_index);
	  break;
	default:
	  error (_("Internal error: bad aggregate clause"));
	}
    }

  return container;
}
	      
/* Assign into the component of LHS indexed by the OP_POSITIONAL
   construct at *POS, updating *POS past the construct, given that
   the positions are relative to lower bound LOW, where HIGH is the 
   upper bound.  Record the position in INDICES[0 .. MAX_INDICES-1]
   updating *NUM_INDICES as needed.  CONTAINER is as for
   assign_aggregate. */
static void
aggregate_assign_positional (struct value *container,
			     struct value *lhs, struct expression *exp,
			     int *pos, LONGEST *indices, int *num_indices,
			     int max_indices, LONGEST low, LONGEST high) 
{
  LONGEST ind = longest_to_int (exp->elts[*pos + 1].longconst) + low;
  
  if (ind - 1 == high)
    warning (_("Extra components in aggregate ignored."));
  if (ind <= high)
    {
      add_component_interval (ind, ind, indices, num_indices, max_indices);
      *pos += 3;
      assign_component (container, lhs, ind, exp, pos);
    }
  else
    ada_evaluate_subexp (NULL, exp, pos, EVAL_SKIP);
}

/* Assign into the components of LHS indexed by the OP_CHOICES
   construct at *POS, updating *POS past the construct, given that
   the allowable indices are LOW..HIGH.  Record the indices assigned
   to in INDICES[0 .. MAX_INDICES-1], updating *NUM_INDICES as
   needed.  CONTAINER is as for assign_aggregate. */
static void
aggregate_assign_from_choices (struct value *container,
			       struct value *lhs, struct expression *exp,
			       int *pos, LONGEST *indices, int *num_indices,
			       int max_indices, LONGEST low, LONGEST high) 
{
  int j;
  int n_choices = longest_to_int (exp->elts[*pos+1].longconst);
  int choice_pos, expr_pc;
  int is_array = ada_is_direct_array_type (value_type (lhs));

  choice_pos = *pos += 3;

  for (j = 0; j < n_choices; j += 1)
    ada_evaluate_subexp (NULL, exp, pos, EVAL_SKIP);
  expr_pc = *pos;
  ada_evaluate_subexp (NULL, exp, pos, EVAL_SKIP);
  
  for (j = 0; j < n_choices; j += 1)
    {
      LONGEST lower, upper;
      enum exp_opcode op = exp->elts[choice_pos].opcode;
      if (op == OP_DISCRETE_RANGE)
	{
	  choice_pos += 1;
	  lower = value_as_long (ada_evaluate_subexp (NULL, exp, pos,
						      EVAL_NORMAL));
	  upper = value_as_long (ada_evaluate_subexp (NULL, exp, pos, 
						      EVAL_NORMAL));
	}
      else if (is_array)
	{
	  lower = value_as_long (ada_evaluate_subexp (NULL, exp, &choice_pos, 
						      EVAL_NORMAL));
	  upper = lower;
	}
      else
	{
	  int ind;
	  char *name;
	  switch (op)
	    {
	    case OP_NAME:
	      name = &exp->elts[choice_pos + 2].string;
	      break;
	    case OP_VAR_VALUE:
	      name = SYMBOL_NATURAL_NAME (exp->elts[choice_pos + 2].symbol);
	      break;
	    default:
	      error (_("Invalid record component association."));
	    }
	  ada_evaluate_subexp (NULL, exp, &choice_pos, EVAL_SKIP);
	  ind = 0;
	  if (! find_struct_field (name, value_type (lhs), 0, 
				   NULL, NULL, NULL, NULL, &ind))
	    error (_("Unknown component name: %s."), name);
	  lower = upper = ind;
	}

      if (lower <= upper && (lower < low || upper > high))
	error (_("Index in component association out of bounds."));

      add_component_interval (lower, upper, indices, num_indices,
			      max_indices);
      while (lower <= upper)
	{
	  int pos1;
	  pos1 = expr_pc;
	  assign_component (container, lhs, lower, exp, &pos1);
	  lower += 1;
	}
    }
}

/* Assign the value of the expression in the OP_OTHERS construct in
   EXP at *POS into the components of LHS indexed from LOW .. HIGH that
   have not been previously assigned.  The index intervals already assigned
   are in INDICES[0 .. NUM_INDICES-1].  Updates *POS to after the 
   OP_OTHERS clause.  CONTAINER is as for assign_aggregate*/
static void
aggregate_assign_others (struct value *container,
			 struct value *lhs, struct expression *exp,
			 int *pos, LONGEST *indices, int num_indices,
			 LONGEST low, LONGEST high) 
{
  int i;
  int expr_pc = *pos+1;
  
  for (i = 0; i < num_indices - 2; i += 2)
    {
      LONGEST ind;
      for (ind = indices[i + 1] + 1; ind < indices[i + 2]; ind += 1)
	{
	  int pos;
	  pos = expr_pc;
	  assign_component (container, lhs, ind, exp, &pos);
	}
    }
  ada_evaluate_subexp (NULL, exp, pos, EVAL_SKIP);
}

/* Add the interval [LOW .. HIGH] to the sorted set of intervals 
   [ INDICES[0] .. INDICES[1] ],..., [ INDICES[*SIZE-2] .. INDICES[*SIZE-1] ],
   modifying *SIZE as needed.  It is an error if *SIZE exceeds
   MAX_SIZE.  The resulting intervals do not overlap.  */
static void
add_component_interval (LONGEST low, LONGEST high, 
			LONGEST* indices, int *size, int max_size)
{
  int i, j;
  for (i = 0; i < *size; i += 2) {
    if (high >= indices[i] && low <= indices[i + 1])
      {
	int kh;
	for (kh = i + 2; kh < *size; kh += 2)
	  if (high < indices[kh])
	    break;
	if (low < indices[i])
	  indices[i] = low;
	indices[i + 1] = indices[kh - 1];
	if (high > indices[i + 1])
	  indices[i + 1] = high;
	memcpy (indices + i + 2, indices + kh, *size - kh);
	*size -= kh - i - 2;
	return;
      }
    else if (high < indices[i])
      break;
  }
	
  if (*size == max_size)
    error (_("Internal error: miscounted aggregate components."));
  *size += 2;
  for (j = *size-1; j >= i+2; j -= 1)
    indices[j] = indices[j - 2];
  indices[i] = low;
  indices[i + 1] = high;
}

static struct value *
ada_evaluate_subexp (struct type *expect_type, struct expression *exp,
                     int *pos, enum noside noside)
{
  enum exp_opcode op;
  int tem, tem2, tem3;
  int pc;
  struct value *arg1 = NULL, *arg2 = NULL, *arg3;
  struct type *type;
  int nargs, oplen;
  struct value **argvec;

  pc = *pos;
  *pos += 1;
  op = exp->elts[pc].opcode;

  switch (op)
    {
    default:
      *pos -= 1;
      return
        unwrap_value (evaluate_subexp_standard
                      (expect_type, exp, pos, noside));

    case OP_STRING:
      {
        struct value *result;
        *pos -= 1;
        result = evaluate_subexp_standard (expect_type, exp, pos, noside);
        /* The result type will have code OP_STRING, bashed there from 
           OP_ARRAY.  Bash it back.  */
        if (TYPE_CODE (value_type (result)) == TYPE_CODE_STRING)
          TYPE_CODE (value_type (result)) = TYPE_CODE_ARRAY;
        return result;
      }

    case UNOP_CAST:
      (*pos) += 2;
      type = exp->elts[pc + 1].type;
      arg1 = evaluate_subexp (type, exp, pos, noside);
      if (noside == EVAL_SKIP)
        goto nosideret;
      if (type != ada_check_typedef (value_type (arg1)))
        {
          if (ada_is_fixed_point_type (type))
            arg1 = cast_to_fixed (type, arg1);
          else if (ada_is_fixed_point_type (value_type (arg1)))
            arg1 = value_cast (type, cast_from_fixed_to_double (arg1));
          else if (VALUE_LVAL (arg1) == lval_memory)
            {
              /* This is in case of the really obscure (and undocumented,
                 but apparently expected) case of (Foo) Bar.all, where Bar
                 is an integer constant and Foo is a dynamic-sized type.
                 If we don't do this, ARG1 will simply be relabeled with
                 TYPE.  */
              if (noside == EVAL_AVOID_SIDE_EFFECTS)
                return value_zero (to_static_fixed_type (type), not_lval);
              arg1 =
                ada_to_fixed_value_create
                (type, VALUE_ADDRESS (arg1) + value_offset (arg1), 0);
            }
          else
            arg1 = value_cast (type, arg1);
        }
      return arg1;

    case UNOP_QUAL:
      (*pos) += 2;
      type = exp->elts[pc + 1].type;
      return ada_evaluate_subexp (type, exp, pos, noside);

    case BINOP_ASSIGN:
      arg1 = evaluate_subexp (NULL_TYPE, exp, pos, noside);
      if (exp->elts[*pos].opcode == OP_AGGREGATE)
	{
	  arg1 = assign_aggregate (arg1, arg1, exp, pos, noside);
	  if (noside == EVAL_SKIP || noside == EVAL_AVOID_SIDE_EFFECTS)
	    return arg1;
	  return ada_value_assign (arg1, arg1);
	}
      arg2 = evaluate_subexp (value_type (arg1), exp, pos, noside);
      if (noside == EVAL_SKIP || noside == EVAL_AVOID_SIDE_EFFECTS)
        return arg1;
      if (ada_is_fixed_point_type (value_type (arg1)))
        arg2 = cast_to_fixed (value_type (arg1), arg2);
      else if (ada_is_fixed_point_type (value_type (arg2)))
        error
          (_("Fixed-point values must be assigned to fixed-point variables"));
      else
        arg2 = coerce_for_assign (value_type (arg1), arg2);
      return ada_value_assign (arg1, arg2);

    case BINOP_ADD:
      arg1 = evaluate_subexp_with_coercion (exp, pos, noside);
      arg2 = evaluate_subexp_with_coercion (exp, pos, noside);
      if (noside == EVAL_SKIP)
        goto nosideret;
      if ((ada_is_fixed_point_type (value_type (arg1))
           || ada_is_fixed_point_type (value_type (arg2)))
          && value_type (arg1) != value_type (arg2))
        error (_("Operands of fixed-point addition must have the same type"));
      return value_cast (value_type (arg1), value_add (arg1, arg2));

    case BINOP_SUB:
      arg1 = evaluate_subexp_with_coercion (exp, pos, noside);
      arg2 = evaluate_subexp_with_coercion (exp, pos, noside);
      if (noside == EVAL_SKIP)
        goto nosideret;
      if ((ada_is_fixed_point_type (value_type (arg1))
           || ada_is_fixed_point_type (value_type (arg2)))
          && value_type (arg1) != value_type (arg2))
        error (_("Operands of fixed-point subtraction must have the same type"));
      return value_cast (value_type (arg1), value_sub (arg1, arg2));

    case BINOP_MUL:
    case BINOP_DIV:
      arg1 = evaluate_subexp (NULL_TYPE, exp, pos, noside);
      arg2 = evaluate_subexp (NULL_TYPE, exp, pos, noside);
      if (noside == EVAL_SKIP)
        goto nosideret;
      else if (noside == EVAL_AVOID_SIDE_EFFECTS
               && (op == BINOP_DIV || op == BINOP_REM || op == BINOP_MOD))
        return value_zero (value_type (arg1), not_lval);
      else
        {
          if (ada_is_fixed_point_type (value_type (arg1)))
            arg1 = cast_from_fixed_to_double (arg1);
          if (ada_is_fixed_point_type (value_type (arg2)))
            arg2 = cast_from_fixed_to_double (arg2);
          return ada_value_binop (arg1, arg2, op);
        }

    case BINOP_REM:
    case BINOP_MOD:
      arg1 = evaluate_subexp (NULL_TYPE, exp, pos, noside);
      arg2 = evaluate_subexp (NULL_TYPE, exp, pos, noside);
      if (noside == EVAL_SKIP)
        goto nosideret;
      else if (noside == EVAL_AVOID_SIDE_EFFECTS
               && (op == BINOP_DIV || op == BINOP_REM || op == BINOP_MOD))
        return value_zero (value_type (arg1), not_lval);
      else
        return ada_value_binop (arg1, arg2, op);

    case BINOP_EQUAL:
    case BINOP_NOTEQUAL:
      arg1 = evaluate_subexp (NULL_TYPE, exp, pos, noside);
      arg2 = evaluate_subexp (value_type (arg1), exp, pos, noside);
      if (noside == EVAL_SKIP)
        goto nosideret;
      if (noside == EVAL_AVOID_SIDE_EFFECTS)
        tem = 0;
      else
        tem = ada_value_equal (arg1, arg2);
      if (op == BINOP_NOTEQUAL)
        tem = !tem;
      return value_from_longest (LA_BOOL_TYPE, (LONGEST) tem);

    case UNOP_NEG:
      arg1 = evaluate_subexp (NULL_TYPE, exp, pos, noside);
      if (noside == EVAL_SKIP)
        goto nosideret;
      else if (ada_is_fixed_point_type (value_type (arg1)))
        return value_cast (value_type (arg1), value_neg (arg1));
      else
        return value_neg (arg1);

    case OP_VAR_VALUE:
      *pos -= 1;
      if (noside == EVAL_SKIP)
        {
          *pos += 4;
          goto nosideret;
        }
      else if (SYMBOL_DOMAIN (exp->elts[pc + 2].symbol) == UNDEF_DOMAIN)
        /* Only encountered when an unresolved symbol occurs in a
           context other than a function call, in which case, it is
           invalid.  */
        error (_("Unexpected unresolved symbol, %s, during evaluation"),
               SYMBOL_PRINT_NAME (exp->elts[pc + 2].symbol));
      else if (noside == EVAL_AVOID_SIDE_EFFECTS)
        {
          *pos += 4;
          return value_zero
            (to_static_fixed_type
             (static_unwrap_type (SYMBOL_TYPE (exp->elts[pc + 2].symbol))),
             not_lval);
        }
      else
        {
          arg1 =
            unwrap_value (evaluate_subexp_standard
                          (expect_type, exp, pos, noside));
          return ada_to_fixed_value (arg1);
        }

    case OP_FUNCALL:
      (*pos) += 2;

      /* Allocate arg vector, including space for the function to be
         called in argvec[0] and a terminating NULL.  */
      nargs = longest_to_int (exp->elts[pc + 1].longconst);
      argvec =
        (struct value **) alloca (sizeof (struct value *) * (nargs + 2));

      if (exp->elts[*pos].opcode == OP_VAR_VALUE
          && SYMBOL_DOMAIN (exp->elts[pc + 5].symbol) == UNDEF_DOMAIN)
        error (_("Unexpected unresolved symbol, %s, during evaluation"),
               SYMBOL_PRINT_NAME (exp->elts[pc + 5].symbol));
      else
        {
          for (tem = 0; tem <= nargs; tem += 1)
            argvec[tem] = evaluate_subexp (NULL_TYPE, exp, pos, noside);
          argvec[tem] = 0;

          if (noside == EVAL_SKIP)
            goto nosideret;
        }

      if (ada_is_packed_array_type (desc_base_type (value_type (argvec[0]))))
        argvec[0] = ada_coerce_to_simple_array (argvec[0]);
      else if (TYPE_CODE (value_type (argvec[0])) == TYPE_CODE_REF
               || (TYPE_CODE (value_type (argvec[0])) == TYPE_CODE_ARRAY
                   && VALUE_LVAL (argvec[0]) == lval_memory))
        argvec[0] = value_addr (argvec[0]);

      type = ada_check_typedef (value_type (argvec[0]));
      if (TYPE_CODE (type) == TYPE_CODE_PTR)
        {
          switch (TYPE_CODE (ada_check_typedef (TYPE_TARGET_TYPE (type))))
            {
            case TYPE_CODE_FUNC:
              type = ada_check_typedef (TYPE_TARGET_TYPE (type));
              break;
            case TYPE_CODE_ARRAY:
              break;
            case TYPE_CODE_STRUCT:
              if (noside != EVAL_AVOID_SIDE_EFFECTS)
                argvec[0] = ada_value_ind (argvec[0]);
              type = ada_check_typedef (TYPE_TARGET_TYPE (type));
              break;
            default:
              error (_("cannot subscript or call something of type `%s'"),
                     ada_type_name (value_type (argvec[0])));
              break;
            }
        }

      switch (TYPE_CODE (type))
        {
        case TYPE_CODE_FUNC:
          if (noside == EVAL_AVOID_SIDE_EFFECTS)
            return allocate_value (TYPE_TARGET_TYPE (type));
          return call_function_by_hand (argvec[0], nargs, argvec + 1);
        case TYPE_CODE_STRUCT:
          {
            int arity;

            arity = ada_array_arity (type);
            type = ada_array_element_type (type, nargs);
            if (type == NULL)
              error (_("cannot subscript or call a record"));
            if (arity != nargs)
              error (_("wrong number of subscripts; expecting %d"), arity);
            if (noside == EVAL_AVOID_SIDE_EFFECTS)
              return allocate_value (ada_aligned_type (type));
            return
              unwrap_value (ada_value_subscript
                            (argvec[0], nargs, argvec + 1));
          }
        case TYPE_CODE_ARRAY:
          if (noside == EVAL_AVOID_SIDE_EFFECTS)
            {
              type = ada_array_element_type (type, nargs);
              if (type == NULL)
                error (_("element type of array unknown"));
              else
                return allocate_value (ada_aligned_type (type));
            }
          return
            unwrap_value (ada_value_subscript
                          (ada_coerce_to_simple_array (argvec[0]),
                           nargs, argvec + 1));
        case TYPE_CODE_PTR:     /* Pointer to array */
          type = to_fixed_array_type (TYPE_TARGET_TYPE (type), NULL, 1);
          if (noside == EVAL_AVOID_SIDE_EFFECTS)
            {
              type = ada_array_element_type (type, nargs);
              if (type == NULL)
                error (_("element type of array unknown"));
              else
                return allocate_value (ada_aligned_type (type));
            }
          return
            unwrap_value (ada_value_ptr_subscript (argvec[0], type,
                                                   nargs, argvec + 1));

        default:
          error (_("Attempt to index or call something other than an "
		   "array or function"));
        }

    case TERNOP_SLICE:
      {
        struct value *array = evaluate_subexp (NULL_TYPE, exp, pos, noside);
        struct value *low_bound_val =
          evaluate_subexp (NULL_TYPE, exp, pos, noside);
        struct value *high_bound_val =
          evaluate_subexp (NULL_TYPE, exp, pos, noside);
        LONGEST low_bound;
        LONGEST high_bound;
        low_bound_val = coerce_ref (low_bound_val);
        high_bound_val = coerce_ref (high_bound_val);
        low_bound = pos_atr (low_bound_val);
        high_bound = pos_atr (high_bound_val);

        if (noside == EVAL_SKIP)
          goto nosideret;

        /* If this is a reference to an aligner type, then remove all
           the aligners.  */
        if (TYPE_CODE (value_type (array)) == TYPE_CODE_REF
            && ada_is_aligner_type (TYPE_TARGET_TYPE (value_type (array))))
          TYPE_TARGET_TYPE (value_type (array)) =
            ada_aligned_type (TYPE_TARGET_TYPE (value_type (array)));

        if (ada_is_packed_array_type (value_type (array)))
          error (_("cannot slice a packed array"));

        /* If this is a reference to an array or an array lvalue,
           convert to a pointer.  */
        if (TYPE_CODE (value_type (array)) == TYPE_CODE_REF
            || (TYPE_CODE (value_type (array)) == TYPE_CODE_ARRAY
                && VALUE_LVAL (array) == lval_memory))
          array = value_addr (array);

        if (noside == EVAL_AVOID_SIDE_EFFECTS
            && ada_is_array_descriptor_type (ada_check_typedef
                                             (value_type (array))))
          return empty_array (ada_type_of_array (array, 0), low_bound);

        array = ada_coerce_to_simple_array_ptr (array);

        /* If we have more than one level of pointer indirection,
           dereference the value until we get only one level.  */
        while (TYPE_CODE (value_type (array)) == TYPE_CODE_PTR
               && (TYPE_CODE (TYPE_TARGET_TYPE (value_type (array)))
                     == TYPE_CODE_PTR))
          array = value_ind (array);

        /* Make sure we really do have an array type before going further,
           to avoid a SEGV when trying to get the index type or the target
           type later down the road if the debug info generated by
           the compiler is incorrect or incomplete.  */
        if (!ada_is_simple_array_type (value_type (array)))
          error (_("cannot take slice of non-array"));

        if (TYPE_CODE (value_type (array)) == TYPE_CODE_PTR)
          {
            if (high_bound < low_bound || noside == EVAL_AVOID_SIDE_EFFECTS)
              return empty_array (TYPE_TARGET_TYPE (value_type (array)),
                                  low_bound);
            else
              {
                struct type *arr_type0 =
                  to_fixed_array_type (TYPE_TARGET_TYPE (value_type (array)),
                                       NULL, 1);
                return ada_value_slice_ptr (array, arr_type0,
                                            longest_to_int (low_bound),
					    longest_to_int (high_bound));
              }
          }
        else if (noside == EVAL_AVOID_SIDE_EFFECTS)
          return array;
        else if (high_bound < low_bound)
          return empty_array (value_type (array), low_bound);
        else
          return ada_value_slice (array, longest_to_int (low_bound),
				  longest_to_int (high_bound));
      }

    case UNOP_IN_RANGE:
      (*pos) += 2;
      arg1 = evaluate_subexp (NULL_TYPE, exp, pos, noside);
      type = exp->elts[pc + 1].type;

      if (noside == EVAL_SKIP)
        goto nosideret;

      switch (TYPE_CODE (type))
        {
        default:
          lim_warning (_("Membership test incompletely implemented; "
			 "always returns true"));
          return value_from_longest (builtin_type_int, (LONGEST) 1);

        case TYPE_CODE_RANGE:
          arg2 = value_from_longest (builtin_type_int, TYPE_LOW_BOUND (type));
          arg3 = value_from_longest (builtin_type_int,
                                     TYPE_HIGH_BOUND (type));
          return
            value_from_longest (builtin_type_int,
                                (value_less (arg1, arg3)
                                 || value_equal (arg1, arg3))
                                && (value_less (arg2, arg1)
                                    || value_equal (arg2, arg1)));
        }

    case BINOP_IN_BOUNDS:
      (*pos) += 2;
      arg1 = evaluate_subexp (NULL_TYPE, exp, pos, noside);
      arg2 = evaluate_subexp (NULL_TYPE, exp, pos, noside);

      if (noside == EVAL_SKIP)
        goto nosideret;

      if (noside == EVAL_AVOID_SIDE_EFFECTS)
        return value_zero (builtin_type_int, not_lval);

      tem = longest_to_int (exp->elts[pc + 1].longconst);

      if (tem < 1 || tem > ada_array_arity (value_type (arg2)))
        error (_("invalid dimension number to 'range"));

      arg3 = ada_array_bound (arg2, tem, 1);
      arg2 = ada_array_bound (arg2, tem, 0);

      return
        value_from_longest (builtin_type_int,
                            (value_less (arg1, arg3)
                             || value_equal (arg1, arg3))
                            && (value_less (arg2, arg1)
                                || value_equal (arg2, arg1)));

    case TERNOP_IN_RANGE:
      arg1 = evaluate_subexp (NULL_TYPE, exp, pos, noside);
      arg2 = evaluate_subexp (NULL_TYPE, exp, pos, noside);
      arg3 = evaluate_subexp (NULL_TYPE, exp, pos, noside);

      if (noside == EVAL_SKIP)
        goto nosideret;

      return
        value_from_longest (builtin_type_int,
                            (value_less (arg1, arg3)
                             || value_equal (arg1, arg3))
                            && (value_less (arg2, arg1)
                                || value_equal (arg2, arg1)));

    case OP_ATR_FIRST:
    case OP_ATR_LAST:
    case OP_ATR_LENGTH:
      {
        struct type *type_arg;
        if (exp->elts[*pos].opcode == OP_TYPE)
          {
            evaluate_subexp (NULL_TYPE, exp, pos, EVAL_SKIP);
            arg1 = NULL;
            type_arg = exp->elts[pc + 2].type;
          }
        else
          {
            arg1 = evaluate_subexp (NULL_TYPE, exp, pos, noside);
            type_arg = NULL;
          }

        if (exp->elts[*pos].opcode != OP_LONG)
          error (_("Invalid operand to '%s"), ada_attribute_name (op));
        tem = longest_to_int (exp->elts[*pos + 2].longconst);
        *pos += 4;

        if (noside == EVAL_SKIP)
          goto nosideret;

        if (type_arg == NULL)
          {
            arg1 = ada_coerce_ref (arg1);

            if (ada_is_packed_array_type (value_type (arg1)))
              arg1 = ada_coerce_to_simple_array (arg1);

            if (tem < 1 || tem > ada_array_arity (value_type (arg1)))
              error (_("invalid dimension number to '%s"),
                     ada_attribute_name (op));

            if (noside == EVAL_AVOID_SIDE_EFFECTS)
              {
                type = ada_index_type (value_type (arg1), tem);
                if (type == NULL)
                  error
                    (_("attempt to take bound of something that is not an array"));
                return allocate_value (type);
              }

            switch (op)
              {
              default:          /* Should never happen.  */
                error (_("unexpected attribute encountered"));
              case OP_ATR_FIRST:
                return ada_array_bound (arg1, tem, 0);
              case OP_ATR_LAST:
                return ada_array_bound (arg1, tem, 1);
              case OP_ATR_LENGTH:
                return ada_array_length (arg1, tem);
              }
          }
        else if (discrete_type_p (type_arg))
          {
            struct type *range_type;
            char *name = ada_type_name (type_arg);
            range_type = NULL;
            if (name != NULL && TYPE_CODE (type_arg) != TYPE_CODE_ENUM)
              range_type =
                to_fixed_range_type (name, NULL, TYPE_OBJFILE (type_arg));
            if (range_type == NULL)
              range_type = type_arg;
            switch (op)
              {
              default:
                error (_("unexpected attribute encountered"));
              case OP_ATR_FIRST:
                return discrete_type_low_bound (range_type);
              case OP_ATR_LAST:
                return discrete_type_high_bound (range_type);
              case OP_ATR_LENGTH:
                error (_("the 'length attribute applies only to array types"));
              }
          }
        else if (TYPE_CODE (type_arg) == TYPE_CODE_FLT)
          error (_("unimplemented type attribute"));
        else
          {
            LONGEST low, high;

            if (ada_is_packed_array_type (type_arg))
              type_arg = decode_packed_array_type (type_arg);

            if (tem < 1 || tem > ada_array_arity (type_arg))
              error (_("invalid dimension number to '%s"),
                     ada_attribute_name (op));

            type = ada_index_type (type_arg, tem);
            if (type == NULL)
              error
                (_("attempt to take bound of something that is not an array"));
            if (noside == EVAL_AVOID_SIDE_EFFECTS)
              return allocate_value (type);

            switch (op)
              {
              default:
                error (_("unexpected attribute encountered"));
              case OP_ATR_FIRST:
                low = ada_array_bound_from_type (type_arg, tem, 0, &type);
                return value_from_longest (type, low);
              case OP_ATR_LAST:
                high = ada_array_bound_from_type (type_arg, tem, 1, &type);
                return value_from_longest (type, high);
              case OP_ATR_LENGTH:
                low = ada_array_bound_from_type (type_arg, tem, 0, &type);
                high = ada_array_bound_from_type (type_arg, tem, 1, NULL);
                return value_from_longest (type, high - low + 1);
              }
          }
      }

    case OP_ATR_TAG:
      arg1 = evaluate_subexp (NULL_TYPE, exp, pos, noside);
      if (noside == EVAL_SKIP)
        goto nosideret;

      if (noside == EVAL_AVOID_SIDE_EFFECTS)
        return value_zero (ada_tag_type (arg1), not_lval);

      return ada_value_tag (arg1);

    case OP_ATR_MIN:
    case OP_ATR_MAX:
      evaluate_subexp (NULL_TYPE, exp, pos, EVAL_SKIP);
      arg1 = evaluate_subexp (NULL_TYPE, exp, pos, noside);
      arg2 = evaluate_subexp (NULL_TYPE, exp, pos, noside);
      if (noside == EVAL_SKIP)
        goto nosideret;
      else if (noside == EVAL_AVOID_SIDE_EFFECTS)
        return value_zero (value_type (arg1), not_lval);
      else
        return value_binop (arg1, arg2,
                            op == OP_ATR_MIN ? BINOP_MIN : BINOP_MAX);

    case OP_ATR_MODULUS:
      {
        struct type *type_arg = exp->elts[pc + 2].type;
        evaluate_subexp (NULL_TYPE, exp, pos, EVAL_SKIP);

        if (noside == EVAL_SKIP)
          goto nosideret;

        if (!ada_is_modular_type (type_arg))
          error (_("'modulus must be applied to modular type"));

        return value_from_longest (TYPE_TARGET_TYPE (type_arg),
                                   ada_modulus (type_arg));
      }


    case OP_ATR_POS:
      evaluate_subexp (NULL_TYPE, exp, pos, EVAL_SKIP);
      arg1 = evaluate_subexp (NULL_TYPE, exp, pos, noside);
      if (noside == EVAL_SKIP)
        goto nosideret;
      else if (noside == EVAL_AVOID_SIDE_EFFECTS)
        return value_zero (builtin_type_int, not_lval);
      else
        return value_pos_atr (arg1);

    case OP_ATR_SIZE:
      arg1 = evaluate_subexp (NULL_TYPE, exp, pos, noside);
      if (noside == EVAL_SKIP)
        goto nosideret;
      else if (noside == EVAL_AVOID_SIDE_EFFECTS)
        return value_zero (builtin_type_int, not_lval);
      else
        return value_from_longest (builtin_type_int,
                                   TARGET_CHAR_BIT
                                   * TYPE_LENGTH (value_type (arg1)));

    case OP_ATR_VAL:
      evaluate_subexp (NULL_TYPE, exp, pos, EVAL_SKIP);
      arg1 = evaluate_subexp (NULL_TYPE, exp, pos, noside);
      type = exp->elts[pc + 2].type;
      if (noside == EVAL_SKIP)
        goto nosideret;
      else if (noside == EVAL_AVOID_SIDE_EFFECTS)
        return value_zero (type, not_lval);
      else
        return value_val_atr (type, arg1);

    case BINOP_EXP:
      arg1 = evaluate_subexp (NULL_TYPE, exp, pos, noside);
      arg2 = evaluate_subexp (NULL_TYPE, exp, pos, noside);
      if (noside == EVAL_SKIP)
        goto nosideret;
      else if (noside == EVAL_AVOID_SIDE_EFFECTS)
        return value_zero (value_type (arg1), not_lval);
      else
        return value_binop (arg1, arg2, op);

    case UNOP_PLUS:
      arg1 = evaluate_subexp (NULL_TYPE, exp, pos, noside);
      if (noside == EVAL_SKIP)
        goto nosideret;
      else
        return arg1;

    case UNOP_ABS:
      arg1 = evaluate_subexp (NULL_TYPE, exp, pos, noside);
      if (noside == EVAL_SKIP)
        goto nosideret;
      if (value_less (arg1, value_zero (value_type (arg1), not_lval)))
        return value_neg (arg1);
      else
        return arg1;

    case UNOP_IND:
      if (expect_type && TYPE_CODE (expect_type) == TYPE_CODE_PTR)
        expect_type = TYPE_TARGET_TYPE (ada_check_typedef (expect_type));
      arg1 = evaluate_subexp (expect_type, exp, pos, noside);
      if (noside == EVAL_SKIP)
        goto nosideret;
      type = ada_check_typedef (value_type (arg1));
      if (noside == EVAL_AVOID_SIDE_EFFECTS)
        {
          if (ada_is_array_descriptor_type (type))
            /* GDB allows dereferencing GNAT array descriptors.  */
            {
              struct type *arrType = ada_type_of_array (arg1, 0);
              if (arrType == NULL)
                error (_("Attempt to dereference null array pointer."));
              return value_at_lazy (arrType, 0);
            }
          else if (TYPE_CODE (type) == TYPE_CODE_PTR
                   || TYPE_CODE (type) == TYPE_CODE_REF
                   /* In C you can dereference an array to get the 1st elt.  */
                   || TYPE_CODE (type) == TYPE_CODE_ARRAY)
            {
              type = to_static_fixed_type
                (ada_aligned_type
                 (ada_check_typedef (TYPE_TARGET_TYPE (type))));
              check_size (type);
              return value_zero (type, lval_memory);
            }
          else if (TYPE_CODE (type) == TYPE_CODE_INT)
            /* GDB allows dereferencing an int.  */
            return value_zero (builtin_type_int, lval_memory);
          else
            error (_("Attempt to take contents of a non-pointer value."));
        }
      arg1 = ada_coerce_ref (arg1);     /* FIXME: What is this for?? */
      type = ada_check_typedef (value_type (arg1));

      if (ada_is_array_descriptor_type (type))
        /* GDB allows dereferencing GNAT array descriptors.  */
        return ada_coerce_to_simple_array (arg1);
      else
        return ada_value_ind (arg1);

    case STRUCTOP_STRUCT:
      tem = longest_to_int (exp->elts[pc + 1].longconst);
      (*pos) += 3 + BYTES_TO_EXP_ELEM (tem + 1);
      arg1 = evaluate_subexp (NULL_TYPE, exp, pos, noside);
      if (noside == EVAL_SKIP)
        goto nosideret;
      if (noside == EVAL_AVOID_SIDE_EFFECTS)
        {
          struct type *type1 = value_type (arg1);
          if (ada_is_tagged_type (type1, 1))
            {
              type = ada_lookup_struct_elt_type (type1,
                                                 &exp->elts[pc + 2].string,
                                                 1, 1, NULL);
              if (type == NULL)
                /* In this case, we assume that the field COULD exist
                   in some extension of the type.  Return an object of 
                   "type" void, which will match any formal 
                   (see ada_type_match). */
                return value_zero (builtin_type_void, lval_memory);
            }
          else
            type =
              ada_lookup_struct_elt_type (type1, &exp->elts[pc + 2].string, 1,
                                          0, NULL);

          return value_zero (ada_aligned_type (type), lval_memory);
        }
      else
        return
          ada_to_fixed_value (unwrap_value
                              (ada_value_struct_elt
                               (arg1, &exp->elts[pc + 2].string, 0)));
    case OP_TYPE:
      /* The value is not supposed to be used.  This is here to make it
         easier to accommodate expressions that contain types.  */
      (*pos) += 2;
      if (noside == EVAL_SKIP)
        goto nosideret;
      else if (noside == EVAL_AVOID_SIDE_EFFECTS)
        return allocate_value (exp->elts[pc + 1].type);
      else
        error (_("Attempt to use a type name as an expression"));

    case OP_AGGREGATE:
    case OP_CHOICES:
    case OP_OTHERS:
    case OP_DISCRETE_RANGE:
    case OP_POSITIONAL:
    case OP_NAME:
      if (noside == EVAL_NORMAL)
	switch (op) 
	  {
	  case OP_NAME:
	    error (_("Undefined name, ambiguous name, or renaming used in "
		     "component association: %s."), &exp->elts[pc+2].string);
	  case OP_AGGREGATE:
	    error (_("Aggregates only allowed on the right of an assignment"));
	  default:
	    internal_error (__FILE__, __LINE__, _("aggregate apparently mangled"));
	  }

      ada_forward_operator_length (exp, pc, &oplen, &nargs);
      *pos += oplen - 1;
      for (tem = 0; tem < nargs; tem += 1) 
	ada_evaluate_subexp (NULL, exp, pos, noside);
      goto nosideret;
    }

nosideret:
  return value_from_longest (builtin_type_long, (LONGEST) 1);
}


                                /* Fixed point */

/* If TYPE encodes an Ada fixed-point type, return the suffix of the
   type name that encodes the 'small and 'delta information.
   Otherwise, return NULL.  */

static const char *
fixed_type_info (struct type *type)
{
  const char *name = ada_type_name (type);
  enum type_code code = (type == NULL) ? TYPE_CODE_UNDEF : TYPE_CODE (type);

  if ((code == TYPE_CODE_INT || code == TYPE_CODE_RANGE) && name != NULL)
    {
      const char *tail = strstr (name, "___XF_");
      if (tail == NULL)
        return NULL;
      else
        return tail + 5;
    }
  else if (code == TYPE_CODE_RANGE && TYPE_TARGET_TYPE (type) != type)
    return fixed_type_info (TYPE_TARGET_TYPE (type));
  else
    return NULL;
}

/* Returns non-zero iff TYPE represents an Ada fixed-point type.  */

int
ada_is_fixed_point_type (struct type *type)
{
  return fixed_type_info (type) != NULL;
}

/* Return non-zero iff TYPE represents a System.Address type.  */

int
ada_is_system_address_type (struct type *type)
{
  return (TYPE_NAME (type)
          && strcmp (TYPE_NAME (type), "system__address") == 0);
}

/* Assuming that TYPE is the representation of an Ada fixed-point
   type, return its delta, or -1 if the type is malformed and the
   delta cannot be determined.  */

DOUBLEST
ada_delta (struct type *type)
{
  const char *encoding = fixed_type_info (type);
  long num, den;

  if (sscanf (encoding, "_%ld_%ld", &num, &den) < 2)
    return -1.0;
  else
    return (DOUBLEST) num / (DOUBLEST) den;
}

/* Assuming that ada_is_fixed_point_type (TYPE), return the scaling
   factor ('SMALL value) associated with the type.  */

static DOUBLEST
scaling_factor (struct type *type)
{
  const char *encoding = fixed_type_info (type);
  unsigned long num0, den0, num1, den1;
  int n;

  n = sscanf (encoding, "_%lu_%lu_%lu_%lu", &num0, &den0, &num1, &den1);

  if (n < 2)
    return 1.0;
  else if (n == 4)
    return (DOUBLEST) num1 / (DOUBLEST) den1;
  else
    return (DOUBLEST) num0 / (DOUBLEST) den0;
}


/* Assuming that X is the representation of a value of fixed-point
   type TYPE, return its floating-point equivalent.  */

DOUBLEST
ada_fixed_to_float (struct type *type, LONGEST x)
{
  return (DOUBLEST) x *scaling_factor (type);
}

/* The representation of a fixed-point value of type TYPE
   corresponding to the value X.  */

LONGEST
ada_float_to_fixed (struct type *type, DOUBLEST x)
{
  return (LONGEST) (x / scaling_factor (type) + 0.5);
}


                                /* VAX floating formats */

/* Non-zero iff TYPE represents one of the special VAX floating-point
   types.  */

int
ada_is_vax_floating_type (struct type *type)
{
  int name_len =
    (ada_type_name (type) == NULL) ? 0 : strlen (ada_type_name (type));
  return
    name_len > 6
    && (TYPE_CODE (type) == TYPE_CODE_INT
        || TYPE_CODE (type) == TYPE_CODE_RANGE)
    && strncmp (ada_type_name (type) + name_len - 6, "___XF", 5) == 0;
}

/* The type of special VAX floating-point type this is, assuming
   ada_is_vax_floating_point.  */

int
ada_vax_float_type_suffix (struct type *type)
{
  return ada_type_name (type)[strlen (ada_type_name (type)) - 1];
}

/* A value representing the special debugging function that outputs
   VAX floating-point values of the type represented by TYPE.  Assumes
   ada_is_vax_floating_type (TYPE).  */

struct value *
ada_vax_float_print_function (struct type *type)
{
  switch (ada_vax_float_type_suffix (type))
    {
    case 'F':
      return get_var_value ("DEBUG_STRING_F", 0);
    case 'D':
      return get_var_value ("DEBUG_STRING_D", 0);
    case 'G':
      return get_var_value ("DEBUG_STRING_G", 0);
    default:
      error (_("invalid VAX floating-point type"));
    }
}


                                /* Range types */

/* Scan STR beginning at position K for a discriminant name, and
   return the value of that discriminant field of DVAL in *PX.  If
   PNEW_K is not null, put the position of the character beyond the
   name scanned in *PNEW_K.  Return 1 if successful; return 0 and do
   not alter *PX and *PNEW_K if unsuccessful.  */

static int
scan_discrim_bound (char *str, int k, struct value *dval, LONGEST * px,
                    int *pnew_k)
{
  static char *bound_buffer = NULL;
  static size_t bound_buffer_len = 0;
  char *bound;
  char *pend;
  struct value *bound_val;

  if (dval == NULL || str == NULL || str[k] == '\0')
    return 0;

  pend = strstr (str + k, "__");
  if (pend == NULL)
    {
      bound = str + k;
      k += strlen (bound);
    }
  else
    {
      GROW_VECT (bound_buffer, bound_buffer_len, pend - (str + k) + 1);
      bound = bound_buffer;
      strncpy (bound_buffer, str + k, pend - (str + k));
      bound[pend - (str + k)] = '\0';
      k = pend - str;
    }

  bound_val = ada_search_struct_field (bound, dval, 0, value_type (dval));
  if (bound_val == NULL)
    return 0;

  *px = value_as_long (bound_val);
  if (pnew_k != NULL)
    *pnew_k = k;
  return 1;
}

/* Value of variable named NAME in the current environment.  If
   no such variable found, then if ERR_MSG is null, returns 0, and
   otherwise causes an error with message ERR_MSG.  */

static struct value *
get_var_value (char *name, char *err_msg)
{
  struct ada_symbol_info *syms;
  int nsyms;

  nsyms = ada_lookup_symbol_list (name, get_selected_block (0), VAR_DOMAIN,
                                  &syms);

  if (nsyms != 1)
    {
      if (err_msg == NULL)
        return 0;
      else
        error (("%s"), err_msg);
    }

  return value_of_variable (syms[0].sym, syms[0].block);
}

/* Value of integer variable named NAME in the current environment.  If
   no such variable found, returns 0, and sets *FLAG to 0.  If
   successful, sets *FLAG to 1.  */

LONGEST
get_int_var_value (char *name, int *flag)
{
  struct value *var_val = get_var_value (name, 0);

  if (var_val == 0)
    {
      if (flag != NULL)
        *flag = 0;
      return 0;
    }
  else
    {
      if (flag != NULL)
        *flag = 1;
      return value_as_long (var_val);
    }
}


/* Return a range type whose base type is that of the range type named
   NAME in the current environment, and whose bounds are calculated
   from NAME according to the GNAT range encoding conventions.
   Extract discriminant values, if needed, from DVAL.  If a new type
   must be created, allocate in OBJFILE's space.  The bounds
   information, in general, is encoded in NAME, the base type given in
   the named range type.  */

static struct type *
to_fixed_range_type (char *name, struct value *dval, struct objfile *objfile)
{
  struct type *raw_type = ada_find_any_type (name);
  struct type *base_type;
  char *subtype_info;

  if (raw_type == NULL)
    base_type = builtin_type_int;
  else if (TYPE_CODE (raw_type) == TYPE_CODE_RANGE)
    base_type = TYPE_TARGET_TYPE (raw_type);
  else
    base_type = raw_type;

  subtype_info = strstr (name, "___XD");
  if (subtype_info == NULL)
    return raw_type;
  else
    {
      static char *name_buf = NULL;
      static size_t name_len = 0;
      int prefix_len = subtype_info - name;
      LONGEST L, U;
      struct type *type;
      char *bounds_str;
      int n;

      GROW_VECT (name_buf, name_len, prefix_len + 5);
      strncpy (name_buf, name, prefix_len);
      name_buf[prefix_len] = '\0';

      subtype_info += 5;
      bounds_str = strchr (subtype_info, '_');
      n = 1;

      if (*subtype_info == 'L')
        {
          if (!ada_scan_number (bounds_str, n, &L, &n)
              && !scan_discrim_bound (bounds_str, n, dval, &L, &n))
            return raw_type;
          if (bounds_str[n] == '_')
            n += 2;
          else if (bounds_str[n] == '.')        /* FIXME? SGI Workshop kludge.  */
            n += 1;
          subtype_info += 1;
        }
      else
        {
          int ok;
          strcpy (name_buf + prefix_len, "___L");
          L = get_int_var_value (name_buf, &ok);
          if (!ok)
            {
              lim_warning (_("Unknown lower bound, using 1."));
              L = 1;
            }
        }

      if (*subtype_info == 'U')
        {
          if (!ada_scan_number (bounds_str, n, &U, &n)
              && !scan_discrim_bound (bounds_str, n, dval, &U, &n))
            return raw_type;
        }
      else
        {
          int ok;
          strcpy (name_buf + prefix_len, "___U");
          U = get_int_var_value (name_buf, &ok);
          if (!ok)
            {
              lim_warning (_("Unknown upper bound, using %ld."), (long) L);
              U = L;
            }
        }

      if (objfile == NULL)
        objfile = TYPE_OBJFILE (base_type);
      type = create_range_type (alloc_type (objfile), base_type, L, U);
      TYPE_NAME (type) = name;
      return type;
    }
}

/* True iff NAME is the name of a range type.  */

int
ada_is_range_type_name (const char *name)
{
  return (name != NULL && strstr (name, "___XD"));
}


                                /* Modular types */

/* True iff TYPE is an Ada modular type.  */

int
ada_is_modular_type (struct type *type)
{
  struct type *subranged_type = base_type (type);

  return (subranged_type != NULL && TYPE_CODE (type) == TYPE_CODE_RANGE
          && TYPE_CODE (subranged_type) != TYPE_CODE_ENUM
          && TYPE_UNSIGNED (subranged_type));
}

/* Assuming ada_is_modular_type (TYPE), the modulus of TYPE.  */

ULONGEST
ada_modulus (struct type * type)
{
  return (ULONGEST) TYPE_HIGH_BOUND (type) + 1;
}


/* Ada exception catchpoint support:
   ---------------------------------

   We support 3 kinds of exception catchpoints:
     . catchpoints on Ada exceptions
     . catchpoints on unhandled Ada exceptions
     . catchpoints on failed assertions

   Exceptions raised during failed assertions, or unhandled exceptions
   could perfectly be caught with the general catchpoint on Ada exceptions.
   However, we can easily differentiate these two special cases, and having
   the option to distinguish these two cases from the rest can be useful
   to zero-in on certain situations.

   Exception catchpoints are a specialized form of breakpoint,
   since they rely on inserting breakpoints inside known routines
   of the GNAT runtime.  The implementation therefore uses a standard
   breakpoint structure of the BP_BREAKPOINT type, but with its own set
   of breakpoint_ops.

   Support in the runtime for exception catchpoints have been changed
   a few times already, and these changes affect the implementation
   of these catchpoints.  In order to be able to support several
   variants of the runtime, we use a sniffer that will determine
   the runtime variant used by the program being debugged.

   At this time, we do not support the use of conditions on Ada exception
   catchpoints.  The COND and COND_STRING fields are therefore set
   to NULL (most of the time, see below).
   
   Conditions where EXP_STRING, COND, and COND_STRING are used:

     When a user specifies the name of a specific exception in the case
     of catchpoints on Ada exceptions, we store the name of that exception
     in the EXP_STRING.  We then translate this request into an actual
     condition stored in COND_STRING, and then parse it into an expression
     stored in COND.  */

/* The different types of catchpoints that we introduced for catching
   Ada exceptions.  */

enum exception_catchpoint_kind
{
  ex_catch_exception,
  ex_catch_exception_unhandled,
  ex_catch_assert
};

typedef CORE_ADDR (ada_unhandled_exception_name_addr_ftype) (void);

/* A structure that describes how to support exception catchpoints
   for a given executable.  */

struct exception_support_info
{
   /* The name of the symbol to break on in order to insert
      a catchpoint on exceptions.  */
   const char *catch_exception_sym;

   /* The name of the symbol to break on in order to insert
      a catchpoint on unhandled exceptions.  */
   const char *catch_exception_unhandled_sym;

   /* The name of the symbol to break on in order to insert
      a catchpoint on failed assertions.  */
   const char *catch_assert_sym;

   /* Assuming that the inferior just triggered an unhandled exception
      catchpoint, this function is responsible for returning the address
      in inferior memory where the name of that exception is stored.
      Return zero if the address could not be computed.  */
   ada_unhandled_exception_name_addr_ftype *unhandled_exception_name_addr;
};

static CORE_ADDR ada_unhandled_exception_name_addr (void);
static CORE_ADDR ada_unhandled_exception_name_addr_from_raise (void);

/* The following exception support info structure describes how to
   implement exception catchpoints with the latest version of the
   Ada runtime (as of 2007-03-06).  */

static const struct exception_support_info default_exception_support_info =
{
  "__gnat_debug_raise_exception", /* catch_exception_sym */
  "__gnat_unhandled_exception", /* catch_exception_unhandled_sym */
  "__gnat_debug_raise_assert_failure", /* catch_assert_sym */
  ada_unhandled_exception_name_addr
};

/* The following exception support info structure describes how to
   implement exception catchpoints with a slightly older version
   of the Ada runtime.  */

static const struct exception_support_info exception_support_info_fallback =
{
  "__gnat_raise_nodefer_with_msg", /* catch_exception_sym */
  "__gnat_unhandled_exception", /* catch_exception_unhandled_sym */
  "system__assertions__raise_assert_failure",  /* catch_assert_sym */
  ada_unhandled_exception_name_addr_from_raise
};

/* For each executable, we sniff which exception info structure to use
   and cache it in the following global variable.  */

static const struct exception_support_info *exception_info = NULL;

/* Inspect the Ada runtime and determine which exception info structure
   should be used to provide support for exception catchpoints.

   This function will always set exception_info, or raise an error.  */

static void
ada_exception_support_info_sniffer (void)
{
  struct symbol *sym;

  /* If the exception info is already known, then no need to recompute it.  */
  if (exception_info != NULL)
    return;

  /* Check the latest (default) exception support info.  */
  sym = standard_lookup (default_exception_support_info.catch_exception_sym,
                         NULL, VAR_DOMAIN);
  if (sym != NULL)
    {
      exception_info = &default_exception_support_info;
      return;
    }

  /* Try our fallback exception suport info.  */
  sym = standard_lookup (exception_support_info_fallback.catch_exception_sym,
                         NULL, VAR_DOMAIN);
  if (sym != NULL)
    {
      exception_info = &exception_support_info_fallback;
      return;
    }

  /* Sometimes, it is normal for us to not be able to find the routine
     we are looking for.  This happens when the program is linked with
     the shared version of the GNAT runtime, and the program has not been
     started yet.  Inform the user of these two possible causes if
     applicable.  */

  if (ada_update_initial_language (language_unknown, NULL) != language_ada)
    error (_("Unable to insert catchpoint.  Is this an Ada main program?"));

  /* If the symbol does not exist, then check that the program is
     already started, to make sure that shared libraries have been
     loaded.  If it is not started, this may mean that the symbol is
     in a shared library.  */

  if (ptid_get_pid (inferior_ptid) == 0)
    error (_("Unable to insert catchpoint. Try to start the program first."));

  /* At this point, we know that we are debugging an Ada program and
     that the inferior has been started, but we still are not able to
     find the run-time symbols. That can mean that we are in
     configurable run time mode, or that a-except as been optimized
     out by the linker...  In any case, at this point it is not worth
     supporting this feature.  */

  error (_("Cannot insert catchpoints in this configuration."));
}

/* An observer of "executable_changed" events.
   Its role is to clear certain cached values that need to be recomputed
   each time a new executable is loaded by GDB.  */

static void
ada_executable_changed_observer (void *unused)
{
  /* If the executable changed, then it is possible that the Ada runtime
     is different.  So we need to invalidate the exception support info
     cache.  */
  exception_info = NULL;
}

/* Return the name of the function at PC, NULL if could not find it.
   This function only checks the debugging information, not the symbol
   table.  */

static char *
function_name_from_pc (CORE_ADDR pc)
{
  char *func_name;

  if (!find_pc_partial_function (pc, &func_name, NULL, NULL))
    return NULL;

  return func_name;
}

/* True iff FRAME is very likely to be that of a function that is
   part of the runtime system.  This is all very heuristic, but is
   intended to be used as advice as to what frames are uninteresting
   to most users.  */

static int
is_known_support_routine (struct frame_info *frame)
{
  struct symtab_and_line sal;
  char *func_name;
  int i;

  /* If this code does not have any debugging information (no symtab),
     This cannot be any user code.  */

  find_frame_sal (frame, &sal);
  if (sal.symtab == NULL)
    return 1;

  /* If there is a symtab, but the associated source file cannot be
     located, then assume this is not user code:  Selecting a frame
     for which we cannot display the code would not be very helpful
     for the user.  This should also take care of case such as VxWorks
     where the kernel has some debugging info provided for a few units.  */

  if (symtab_to_fullname (sal.symtab) == NULL)
    return 1;

  /* Check the unit filename againt the Ada runtime file naming.
     We also check the name of the objfile against the name of some
     known system libraries that sometimes come with debugging info
     too.  */

  for (i = 0; known_runtime_file_name_patterns[i] != NULL; i += 1)
    {
      re_comp (known_runtime_file_name_patterns[i]);
      if (re_exec (sal.symtab->filename))
        return 1;
      if (sal.symtab->objfile != NULL
          && re_exec (sal.symtab->objfile->name))
        return 1;
    }

  /* Check whether the function is a GNAT-generated entity.  */

  func_name = function_name_from_pc (get_frame_address_in_block (frame));
  if (func_name == NULL)
    return 1;

  for (i = 0; known_auxiliary_function_name_patterns[i] != NULL; i += 1)
    {
      re_comp (known_auxiliary_function_name_patterns[i]);
      if (re_exec (func_name))
        return 1;
    }

  return 0;
}

/* Find the first frame that contains debugging information and that is not
   part of the Ada run-time, starting from FI and moving upward.  */

static void
ada_find_printable_frame (struct frame_info *fi)
{
  for (; fi != NULL; fi = get_prev_frame (fi))
    {
      if (!is_known_support_routine (fi))
        {
          select_frame (fi);
          break;
        }
    }

}

/* Assuming that the inferior just triggered an unhandled exception
   catchpoint, return the address in inferior memory where the name
   of the exception is stored.
   
   Return zero if the address could not be computed.  */

static CORE_ADDR
ada_unhandled_exception_name_addr (void)
{
  return parse_and_eval_address ("e.full_name");
}

/* Same as ada_unhandled_exception_name_addr, except that this function
   should be used when the inferior uses an older version of the runtime,
   where the exception name needs to be extracted from a specific frame
   several frames up in the callstack.  */

static CORE_ADDR
ada_unhandled_exception_name_addr_from_raise (void)
{
  int frame_level;
  struct frame_info *fi;

  /* To determine the name of this exception, we need to select
     the frame corresponding to RAISE_SYM_NAME.  This frame is
     at least 3 levels up, so we simply skip the first 3 frames
     without checking the name of their associated function.  */
  fi = get_current_frame ();
  for (frame_level = 0; frame_level < 3; frame_level += 1)
    if (fi != NULL)
      fi = get_prev_frame (fi); 

  while (fi != NULL)
    {
      const char *func_name =
        function_name_from_pc (get_frame_address_in_block (fi));
      if (func_name != NULL
          && strcmp (func_name, exception_info->catch_exception_sym) == 0)
        break; /* We found the frame we were looking for...  */
      fi = get_prev_frame (fi);
    }

  if (fi == NULL)
    return 0;

  select_frame (fi);
  return parse_and_eval_address ("id.full_name");
}

/* Assuming the inferior just triggered an Ada exception catchpoint
   (of any type), return the address in inferior memory where the name
   of the exception is stored, if applicable.

   Return zero if the address could not be computed, or if not relevant.  */

static CORE_ADDR
ada_exception_name_addr_1 (enum exception_catchpoint_kind ex,
                           struct breakpoint *b)
{
  switch (ex)
    {
      case ex_catch_exception:
        return (parse_and_eval_address ("e.full_name"));
        break;

      case ex_catch_exception_unhandled:
        return exception_info->unhandled_exception_name_addr ();
        break;
      
      case ex_catch_assert:
        return 0;  /* Exception name is not relevant in this case.  */
        break;

      default:
        internal_error (__FILE__, __LINE__, _("unexpected catchpoint type"));
        break;
    }

  return 0; /* Should never be reached.  */
}

/* Same as ada_exception_name_addr_1, except that it intercepts and contains
   any error that ada_exception_name_addr_1 might cause to be thrown.
   When an error is intercepted, a warning with the error message is printed,
   and zero is returned.  */

static CORE_ADDR
ada_exception_name_addr (enum exception_catchpoint_kind ex,
                         struct breakpoint *b)
{
  struct gdb_exception e;
  CORE_ADDR result = 0;

  TRY_CATCH (e, RETURN_MASK_ERROR)
    {
      result = ada_exception_name_addr_1 (ex, b);
    }

  if (e.reason < 0)
    {
      warning (_("failed to get exception name: %s"), e.message);
      return 0;
    }

  return result;
}

/* Implement the PRINT_IT method in the breakpoint_ops structure
   for all exception catchpoint kinds.  */

static enum print_stop_action
print_it_exception (enum exception_catchpoint_kind ex, struct breakpoint *b)
{
  const CORE_ADDR addr = ada_exception_name_addr (ex, b);
  char exception_name[256];

  if (addr != 0)
    {
      read_memory (addr, exception_name, sizeof (exception_name) - 1);
      exception_name [sizeof (exception_name) - 1] = '\0';
    }

  ada_find_printable_frame (get_current_frame ());

  annotate_catchpoint (b->number);
  switch (ex)
    {
      case ex_catch_exception:
        if (addr != 0)
          printf_filtered (_("\nCatchpoint %d, %s at "),
                           b->number, exception_name);
        else
          printf_filtered (_("\nCatchpoint %d, exception at "), b->number);
        break;
      case ex_catch_exception_unhandled:
        if (addr != 0)
          printf_filtered (_("\nCatchpoint %d, unhandled %s at "),
                           b->number, exception_name);
        else
          printf_filtered (_("\nCatchpoint %d, unhandled exception at "),
                           b->number);
        break;
      case ex_catch_assert:
        printf_filtered (_("\nCatchpoint %d, failed assertion at "),
                         b->number);
        break;
    }

  return PRINT_SRC_AND_LOC;
}

/* Implement the PRINT_ONE method in the breakpoint_ops structure
   for all exception catchpoint kinds.  */

static void
print_one_exception (enum exception_catchpoint_kind ex,
                     struct breakpoint *b, CORE_ADDR *last_addr)
{ 
  if (addressprint)
    {
      annotate_field (4);
      ui_out_field_core_addr (uiout, "addr", b->loc->address);
    }

  annotate_field (5);
  *last_addr = b->loc->address;
  switch (ex)
    {
      case ex_catch_exception:
        if (b->exp_string != NULL)
          {
            char *msg = xstrprintf (_("`%s' Ada exception"), b->exp_string);
            
            ui_out_field_string (uiout, "what", msg);
            xfree (msg);
          }
        else
          ui_out_field_string (uiout, "what", "all Ada exceptions");
        
        break;

      case ex_catch_exception_unhandled:
        ui_out_field_string (uiout, "what", "unhandled Ada exceptions");
        break;
      
      case ex_catch_assert:
        ui_out_field_string (uiout, "what", "failed Ada assertions");
        break;

      default:
        internal_error (__FILE__, __LINE__, _("unexpected catchpoint type"));
        break;
    }
}

/* Implement the PRINT_MENTION method in the breakpoint_ops structure
   for all exception catchpoint kinds.  */

static void
print_mention_exception (enum exception_catchpoint_kind ex,
                         struct breakpoint *b)
{
  switch (ex)
    {
      case ex_catch_exception:
        if (b->exp_string != NULL)
          printf_filtered (_("Catchpoint %d: `%s' Ada exception"),
                           b->number, b->exp_string);
        else
          printf_filtered (_("Catchpoint %d: all Ada exceptions"), b->number);
        
        break;

      case ex_catch_exception_unhandled:
        printf_filtered (_("Catchpoint %d: unhandled Ada exceptions"),
                         b->number);
        break;
      
      case ex_catch_assert:
        printf_filtered (_("Catchpoint %d: failed Ada assertions"), b->number);
        break;

      default:
        internal_error (__FILE__, __LINE__, _("unexpected catchpoint type"));
        break;
    }
}

/* Virtual table for "catch exception" breakpoints.  */

static enum print_stop_action
print_it_catch_exception (struct breakpoint *b)
{
  return print_it_exception (ex_catch_exception, b);
}

static void
print_one_catch_exception (struct breakpoint *b, CORE_ADDR *last_addr)
{
  print_one_exception (ex_catch_exception, b, last_addr);
}

static void
print_mention_catch_exception (struct breakpoint *b)
{
  print_mention_exception (ex_catch_exception, b);
}

static struct breakpoint_ops catch_exception_breakpoint_ops =
{
  print_it_catch_exception,
  print_one_catch_exception,
  print_mention_catch_exception
};

/* Virtual table for "catch exception unhandled" breakpoints.  */

static enum print_stop_action
print_it_catch_exception_unhandled (struct breakpoint *b)
{
  return print_it_exception (ex_catch_exception_unhandled, b);
}

static void
print_one_catch_exception_unhandled (struct breakpoint *b, CORE_ADDR *last_addr)
{
  print_one_exception (ex_catch_exception_unhandled, b, last_addr);
}

static void
print_mention_catch_exception_unhandled (struct breakpoint *b)
{
  print_mention_exception (ex_catch_exception_unhandled, b);
}

static struct breakpoint_ops catch_exception_unhandled_breakpoint_ops = {
  print_it_catch_exception_unhandled,
  print_one_catch_exception_unhandled,
  print_mention_catch_exception_unhandled
};

/* Virtual table for "catch assert" breakpoints.  */

static enum print_stop_action
print_it_catch_assert (struct breakpoint *b)
{
  return print_it_exception (ex_catch_assert, b);
}

static void
print_one_catch_assert (struct breakpoint *b, CORE_ADDR *last_addr)
{
  print_one_exception (ex_catch_assert, b, last_addr);
}

static void
print_mention_catch_assert (struct breakpoint *b)
{
  print_mention_exception (ex_catch_assert, b);
}

static struct breakpoint_ops catch_assert_breakpoint_ops = {
  print_it_catch_assert,
  print_one_catch_assert,
  print_mention_catch_assert
};

/* Return non-zero if B is an Ada exception catchpoint.  */

int
ada_exception_catchpoint_p (struct breakpoint *b)
{
  return (b->ops == &catch_exception_breakpoint_ops
          || b->ops == &catch_exception_unhandled_breakpoint_ops
          || b->ops == &catch_assert_breakpoint_ops);
}

/* Return a newly allocated copy of the first space-separated token
   in ARGSP, and then adjust ARGSP to point immediately after that
   token.

   Return NULL if ARGPS does not contain any more tokens.  */

static char *
ada_get_next_arg (char **argsp)
{
  char *args = *argsp;
  char *end;
  char *result;

  /* Skip any leading white space.  */

  while (isspace (*args))
    args++;

  if (args[0] == '\0')
    return NULL; /* No more arguments.  */
  
  /* Find the end of the current argument.  */

  end = args;
  while (*end != '\0' && !isspace (*end))
    end++;

  /* Adjust ARGSP to point to the start of the next argument.  */

  *argsp = end;

  /* Make a copy of the current argument and return it.  */

  result = xmalloc (end - args + 1);
  strncpy (result, args, end - args);
  result[end - args] = '\0';
  
  return result;
}

/* Split the arguments specified in a "catch exception" command.  
   Set EX to the appropriate catchpoint type.
   Set EXP_STRING to the name of the specific exception if
   specified by the user.  */

static void
catch_ada_exception_command_split (char *args,
                                   enum exception_catchpoint_kind *ex,
                                   char **exp_string)
{
  struct cleanup *old_chain = make_cleanup (null_cleanup, NULL);
  char *exception_name;

  exception_name = ada_get_next_arg (&args);
  make_cleanup (xfree, exception_name);

  /* Check that we do not have any more arguments.  Anything else
     is unexpected.  */

  while (isspace (*args))
    args++;

  if (args[0] != '\0')
    error (_("Junk at end of expression"));

  discard_cleanups (old_chain);

  if (exception_name == NULL)
    {
      /* Catch all exceptions.  */
      *ex = ex_catch_exception;
      *exp_string = NULL;
    }
  else if (strcmp (exception_name, "unhandled") == 0)
    {
      /* Catch unhandled exceptions.  */
      *ex = ex_catch_exception_unhandled;
      *exp_string = NULL;
    }
  else
    {
      /* Catch a specific exception.  */
      *ex = ex_catch_exception;
      *exp_string = exception_name;
    }
}

/* Return the name of the symbol on which we should break in order to
   implement a catchpoint of the EX kind.  */

static const char *
ada_exception_sym_name (enum exception_catchpoint_kind ex)
{
  gdb_assert (exception_info != NULL);

  switch (ex)
    {
      case ex_catch_exception:
        return (exception_info->catch_exception_sym);
        break;
      case ex_catch_exception_unhandled:
        return (exception_info->catch_exception_unhandled_sym);
        break;
      case ex_catch_assert:
        return (exception_info->catch_assert_sym);
        break;
      default:
        internal_error (__FILE__, __LINE__,
                        _("unexpected catchpoint kind (%d)"), ex);
    }
}

/* Return the breakpoint ops "virtual table" used for catchpoints
   of the EX kind.  */

static struct breakpoint_ops *
ada_exception_breakpoint_ops (enum exception_catchpoint_kind ex)
{
  switch (ex)
    {
      case ex_catch_exception:
        return (&catch_exception_breakpoint_ops);
        break;
      case ex_catch_exception_unhandled:
        return (&catch_exception_unhandled_breakpoint_ops);
        break;
      case ex_catch_assert:
        return (&catch_assert_breakpoint_ops);
        break;
      default:
        internal_error (__FILE__, __LINE__,
                        _("unexpected catchpoint kind (%d)"), ex);
    }
}

/* Return the condition that will be used to match the current exception
   being raised with the exception that the user wants to catch.  This
   assumes that this condition is used when the inferior just triggered
   an exception catchpoint.
   
   The string returned is a newly allocated string that needs to be
   deallocated later.  */

static char *
ada_exception_catchpoint_cond_string (const char *exp_string)
{
  return xstrprintf ("long_integer (e) = long_integer (&%s)", exp_string);
}

/* Return the expression corresponding to COND_STRING evaluated at SAL.  */

static struct expression *
ada_parse_catchpoint_condition (char *cond_string,
                                struct symtab_and_line sal)
{
  return (parse_exp_1 (&cond_string, block_for_pc (sal.pc), 0));
}

/* Return the symtab_and_line that should be used to insert an exception
   catchpoint of the TYPE kind.

   EX_STRING should contain the name of a specific exception
   that the catchpoint should catch, or NULL otherwise.

   The idea behind all the remaining parameters is that their names match
   the name of certain fields in the breakpoint structure that are used to
   handle exception catchpoints.  This function returns the value to which
   these fields should be set, depending on the type of catchpoint we need
   to create.
   
   If COND and COND_STRING are both non-NULL, any value they might
   hold will be free'ed, and then replaced by newly allocated ones.
   These parameters are left untouched otherwise.  */

static struct symtab_and_line
ada_exception_sal (enum exception_catchpoint_kind ex, char *exp_string,
                   char **addr_string, char **cond_string,
                   struct expression **cond, struct breakpoint_ops **ops)
{
  const char *sym_name;
  struct symbol *sym;
  struct symtab_and_line sal;

  /* First, find out which exception support info to use.  */
  ada_exception_support_info_sniffer ();

  /* Then lookup the function on which we will break in order to catch
     the Ada exceptions requested by the user.  */

  sym_name = ada_exception_sym_name (ex);
  sym = standard_lookup (sym_name, NULL, VAR_DOMAIN);

  /* The symbol we're looking up is provided by a unit in the GNAT runtime
     that should be compiled with debugging information.  As a result, we
     expect to find that symbol in the symtabs.  If we don't find it, then
     the target most likely does not support Ada exceptions, or we cannot
     insert exception breakpoints yet, because the GNAT runtime hasn't been
     loaded yet.  */

  /* brobecker/2006-12-26: It is conceivable that the runtime was compiled
     in such a way that no debugging information is produced for the symbol
     we are looking for.  In this case, we could search the minimal symbols
     as a fall-back mechanism.  This would still be operating in degraded
     mode, however, as we would still be missing the debugging information
     that is needed in order to extract the name of the exception being
     raised (this name is printed in the catchpoint message, and is also
     used when trying to catch a specific exception).  We do not handle
     this case for now.  */

  if (sym == NULL)
    error (_("Unable to break on '%s' in this configuration."), sym_name);

  /* Make sure that the symbol we found corresponds to a function.  */
  if (SYMBOL_CLASS (sym) != LOC_BLOCK)
    error (_("Symbol \"%s\" is not a function (class = %d)"),
           sym_name, SYMBOL_CLASS (sym));

  sal = find_function_start_sal (sym, 1);

  /* Set ADDR_STRING.  */

  *addr_string = xstrdup (sym_name);

  /* Set the COND and COND_STRING (if not NULL).  */

  if (cond_string != NULL && cond != NULL)
    {
      if (*cond_string != NULL)
        {
          xfree (*cond_string);
          *cond_string = NULL;
        }
      if (*cond != NULL)
        {
          xfree (*cond);
          *cond = NULL;
        }
      if (exp_string != NULL)
        {
          *cond_string = ada_exception_catchpoint_cond_string (exp_string);
          *cond = ada_parse_catchpoint_condition (*cond_string, sal);
        }
    }

  /* Set OPS.  */
  *ops = ada_exception_breakpoint_ops (ex);

  return sal;
}

/* Parse the arguments (ARGS) of the "catch exception" command.
 
   Set TYPE to the appropriate exception catchpoint type.
   If the user asked the catchpoint to catch only a specific
   exception, then save the exception name in ADDR_STRING.

   See ada_exception_sal for a description of all the remaining
   function arguments of this function.  */

struct symtab_and_line
ada_decode_exception_location (char *args, char **addr_string,
                               char **exp_string, char **cond_string,
                               struct expression **cond,
                               struct breakpoint_ops **ops)
{
  enum exception_catchpoint_kind ex;

  catch_ada_exception_command_split (args, &ex, exp_string);
  return ada_exception_sal (ex, *exp_string, addr_string, cond_string,
                            cond, ops);
}

struct symtab_and_line
ada_decode_assert_location (char *args, char **addr_string,
                            struct breakpoint_ops **ops)
{
  /* Check that no argument where provided at the end of the command.  */

  if (args != NULL)
    {
      while (isspace (*args))
        args++;
      if (*args != '\0')
        error (_("Junk at end of arguments."));
    }

  return ada_exception_sal (ex_catch_assert, NULL, addr_string, NULL, NULL,
                            ops);
}

                                /* Operators */
/* Information about operators given special treatment in functions
   below.  */
/* Format: OP_DEFN (<operator>, <operator length>, <# args>, <binop>).  */

#define ADA_OPERATORS \
    OP_DEFN (OP_VAR_VALUE, 4, 0, 0) \
    OP_DEFN (BINOP_IN_BOUNDS, 3, 2, 0) \
    OP_DEFN (TERNOP_IN_RANGE, 1, 3, 0) \
    OP_DEFN (OP_ATR_FIRST, 1, 2, 0) \
    OP_DEFN (OP_ATR_LAST, 1, 2, 0) \
    OP_DEFN (OP_ATR_LENGTH, 1, 2, 0) \
    OP_DEFN (OP_ATR_IMAGE, 1, 2, 0) \
    OP_DEFN (OP_ATR_MAX, 1, 3, 0) \
    OP_DEFN (OP_ATR_MIN, 1, 3, 0) \
    OP_DEFN (OP_ATR_MODULUS, 1, 1, 0) \
    OP_DEFN (OP_ATR_POS, 1, 2, 0) \
    OP_DEFN (OP_ATR_SIZE, 1, 1, 0) \
    OP_DEFN (OP_ATR_TAG, 1, 1, 0) \
    OP_DEFN (OP_ATR_VAL, 1, 2, 0) \
    OP_DEFN (UNOP_QUAL, 3, 1, 0) \
    OP_DEFN (UNOP_IN_RANGE, 3, 1, 0) \
    OP_DEFN (OP_OTHERS, 1, 1, 0) \
    OP_DEFN (OP_POSITIONAL, 3, 1, 0) \
    OP_DEFN (OP_DISCRETE_RANGE, 1, 2, 0)

static void
ada_operator_length (struct expression *exp, int pc, int *oplenp, int *argsp)
{
  switch (exp->elts[pc - 1].opcode)
    {
    default:
      operator_length_standard (exp, pc, oplenp, argsp);
      break;

#define OP_DEFN(op, len, args, binop) \
    case op: *oplenp = len; *argsp = args; break;
      ADA_OPERATORS;
#undef OP_DEFN

    case OP_AGGREGATE:
      *oplenp = 3;
      *argsp = longest_to_int (exp->elts[pc - 2].longconst);
      break;

    case OP_CHOICES:
      *oplenp = 3;
      *argsp = longest_to_int (exp->elts[pc - 2].longconst) + 1;
      break;
    }
}

static char *
ada_op_name (enum exp_opcode opcode)
{
  switch (opcode)
    {
    default:
      return op_name_standard (opcode);

#define OP_DEFN(op, len, args, binop) case op: return #op;
      ADA_OPERATORS;
#undef OP_DEFN

    case OP_AGGREGATE:
      return "OP_AGGREGATE";
    case OP_CHOICES:
      return "OP_CHOICES";
    case OP_NAME:
      return "OP_NAME";
    }
}

/* As for operator_length, but assumes PC is pointing at the first
   element of the operator, and gives meaningful results only for the 
   Ada-specific operators, returning 0 for *OPLENP and *ARGSP otherwise.  */

static void
ada_forward_operator_length (struct expression *exp, int pc,
                             int *oplenp, int *argsp)
{
  switch (exp->elts[pc].opcode)
    {
    default:
      *oplenp = *argsp = 0;
      break;

#define OP_DEFN(op, len, args, binop) \
    case op: *oplenp = len; *argsp = args; break;
      ADA_OPERATORS;
#undef OP_DEFN

    case OP_AGGREGATE:
      *oplenp = 3;
      *argsp = longest_to_int (exp->elts[pc + 1].longconst);
      break;

    case OP_CHOICES:
      *oplenp = 3;
      *argsp = longest_to_int (exp->elts[pc + 1].longconst) + 1;
      break;

    case OP_STRING:
    case OP_NAME:
      {
	int len = longest_to_int (exp->elts[pc + 1].longconst);
	*oplenp = 4 + BYTES_TO_EXP_ELEM (len + 1);
	*argsp = 0;
	break;
      }
    }
}

static int
ada_dump_subexp_body (struct expression *exp, struct ui_file *stream, int elt)
{
  enum exp_opcode op = exp->elts[elt].opcode;
  int oplen, nargs;
  int pc = elt;
  int i;

  ada_forward_operator_length (exp, elt, &oplen, &nargs);

  switch (op)
    {
      /* Ada attributes ('Foo).  */
    case OP_ATR_FIRST:
    case OP_ATR_LAST:
    case OP_ATR_LENGTH:
    case OP_ATR_IMAGE:
    case OP_ATR_MAX:
    case OP_ATR_MIN:
    case OP_ATR_MODULUS:
    case OP_ATR_POS:
    case OP_ATR_SIZE:
    case OP_ATR_TAG:
    case OP_ATR_VAL:
      break;

    case UNOP_IN_RANGE:
    case UNOP_QUAL:
      /* XXX: gdb_sprint_host_address, type_sprint */
      fprintf_filtered (stream, _("Type @"));
      gdb_print_host_address (exp->elts[pc + 1].type, stream);
      fprintf_filtered (stream, " (");
      type_print (exp->elts[pc + 1].type, NULL, stream, 0);
      fprintf_filtered (stream, ")");
      break;
    case BINOP_IN_BOUNDS:
      fprintf_filtered (stream, " (%d)",
			longest_to_int (exp->elts[pc + 2].longconst));
      break;
    case TERNOP_IN_RANGE:
      break;

    case OP_AGGREGATE:
    case OP_OTHERS:
    case OP_DISCRETE_RANGE:
    case OP_POSITIONAL:
    case OP_CHOICES:
      break;

    case OP_NAME:
    case OP_STRING:
      {
	char *name = &exp->elts[elt + 2].string;
	int len = longest_to_int (exp->elts[elt + 1].longconst);
	fprintf_filtered (stream, "Text: `%.*s'", len, name);
	break;
      }

    default:
      return dump_subexp_body_standard (exp, stream, elt);
    }

  elt += oplen;
  for (i = 0; i < nargs; i += 1)
    elt = dump_subexp (exp, stream, elt);

  return elt;
}

/* The Ada extension of print_subexp (q.v.).  */

static void
ada_print_subexp (struct expression *exp, int *pos,
                  struct ui_file *stream, enum precedence prec)
{
  int oplen, nargs, i;
  int pc = *pos;
  enum exp_opcode op = exp->elts[pc].opcode;

  ada_forward_operator_length (exp, pc, &oplen, &nargs);

  *pos += oplen;
  switch (op)
    {
    default:
      *pos -= oplen;
      print_subexp_standard (exp, pos, stream, prec);
      return;

    case OP_VAR_VALUE:
      fputs_filtered (SYMBOL_NATURAL_NAME (exp->elts[pc + 2].symbol), stream);
      return;

    case BINOP_IN_BOUNDS:
      /* XXX: sprint_subexp */
      print_subexp (exp, pos, stream, PREC_SUFFIX);
      fputs_filtered (" in ", stream);
      print_subexp (exp, pos, stream, PREC_SUFFIX);
      fputs_filtered ("'range", stream);
      if (exp->elts[pc + 1].longconst > 1)
        fprintf_filtered (stream, "(%ld)",
                          (long) exp->elts[pc + 1].longconst);
      return;

    case TERNOP_IN_RANGE:
      if (prec >= PREC_EQUAL)
        fputs_filtered ("(", stream);
      /* XXX: sprint_subexp */
      print_subexp (exp, pos, stream, PREC_SUFFIX);
      fputs_filtered (" in ", stream);
      print_subexp (exp, pos, stream, PREC_EQUAL);
      fputs_filtered (" .. ", stream);
      print_subexp (exp, pos, stream, PREC_EQUAL);
      if (prec >= PREC_EQUAL)
        fputs_filtered (")", stream);
      return;

    case OP_ATR_FIRST:
    case OP_ATR_LAST:
    case OP_ATR_LENGTH:
    case OP_ATR_IMAGE:
    case OP_ATR_MAX:
    case OP_ATR_MIN:
    case OP_ATR_MODULUS:
    case OP_ATR_POS:
    case OP_ATR_SIZE:
    case OP_ATR_TAG:
    case OP_ATR_VAL:
      if (exp->elts[*pos].opcode == OP_TYPE)
        {
          if (TYPE_CODE (exp->elts[*pos + 1].type) != TYPE_CODE_VOID)
            LA_PRINT_TYPE (exp->elts[*pos + 1].type, "", stream, 0, 0);
          *pos += 3;
        }
      else
        print_subexp (exp, pos, stream, PREC_SUFFIX);
      fprintf_filtered (stream, "'%s", ada_attribute_name (op));
      if (nargs > 1)
        {
          int tem;
          for (tem = 1; tem < nargs; tem += 1)
            {
              fputs_filtered ((tem == 1) ? " (" : ", ", stream);
              print_subexp (exp, pos, stream, PREC_ABOVE_COMMA);
            }
          fputs_filtered (")", stream);
        }
      return;

    case UNOP_QUAL:
      type_print (exp->elts[pc + 1].type, "", stream, 0);
      fputs_filtered ("'(", stream);
      print_subexp (exp, pos, stream, PREC_PREFIX);
      fputs_filtered (")", stream);
      return;

    case UNOP_IN_RANGE:
      /* XXX: sprint_subexp */
      print_subexp (exp, pos, stream, PREC_SUFFIX);
      fputs_filtered (" in ", stream);
      LA_PRINT_TYPE (exp->elts[pc + 1].type, "", stream, 1, 0);
      return;

    case OP_DISCRETE_RANGE:
      print_subexp (exp, pos, stream, PREC_SUFFIX);
      fputs_filtered ("..", stream);
      print_subexp (exp, pos, stream, PREC_SUFFIX);
      return;

    case OP_OTHERS:
      fputs_filtered ("others => ", stream);
      print_subexp (exp, pos, stream, PREC_SUFFIX);
      return;

    case OP_CHOICES:
      for (i = 0; i < nargs-1; i += 1)
	{
	  if (i > 0)
	    fputs_filtered ("|", stream);
	  print_subexp (exp, pos, stream, PREC_SUFFIX);
	}
      fputs_filtered (" => ", stream);
      print_subexp (exp, pos, stream, PREC_SUFFIX);
      return;
      
    case OP_POSITIONAL:
      print_subexp (exp, pos, stream, PREC_SUFFIX);
      return;

    case OP_AGGREGATE:
      fputs_filtered ("(", stream);
      for (i = 0; i < nargs; i += 1)
	{
	  if (i > 0)
	    fputs_filtered (", ", stream);
	  print_subexp (exp, pos, stream, PREC_SUFFIX);
	}
      fputs_filtered (")", stream);
      return;
    }
}

/* Table mapping opcodes into strings for printing operators
   and precedences of the operators.  */

static const struct op_print ada_op_print_tab[] = {
  {":=", BINOP_ASSIGN, PREC_ASSIGN, 1},
  {"or else", BINOP_LOGICAL_OR, PREC_LOGICAL_OR, 0},
  {"and then", BINOP_LOGICAL_AND, PREC_LOGICAL_AND, 0},
  {"or", BINOP_BITWISE_IOR, PREC_BITWISE_IOR, 0},
  {"xor", BINOP_BITWISE_XOR, PREC_BITWISE_XOR, 0},
  {"and", BINOP_BITWISE_AND, PREC_BITWISE_AND, 0},
  {"=", BINOP_EQUAL, PREC_EQUAL, 0},
  {"/=", BINOP_NOTEQUAL, PREC_EQUAL, 0},
  {"<=", BINOP_LEQ, PREC_ORDER, 0},
  {">=", BINOP_GEQ, PREC_ORDER, 0},
  {">", BINOP_GTR, PREC_ORDER, 0},
  {"<", BINOP_LESS, PREC_ORDER, 0},
  {">>", BINOP_RSH, PREC_SHIFT, 0},
  {"<<", BINOP_LSH, PREC_SHIFT, 0},
  {"+", BINOP_ADD, PREC_ADD, 0},
  {"-", BINOP_SUB, PREC_ADD, 0},
  {"&", BINOP_CONCAT, PREC_ADD, 0},
  {"*", BINOP_MUL, PREC_MUL, 0},
  {"/", BINOP_DIV, PREC_MUL, 0},
  {"rem", BINOP_REM, PREC_MUL, 0},
  {"mod", BINOP_MOD, PREC_MUL, 0},
  {"**", BINOP_EXP, PREC_REPEAT, 0},
  {"@", BINOP_REPEAT, PREC_REPEAT, 0},
  {"-", UNOP_NEG, PREC_PREFIX, 0},
  {"+", UNOP_PLUS, PREC_PREFIX, 0},
  {"not ", UNOP_LOGICAL_NOT, PREC_PREFIX, 0},
  {"not ", UNOP_COMPLEMENT, PREC_PREFIX, 0},
  {"abs ", UNOP_ABS, PREC_PREFIX, 0},
  {".all", UNOP_IND, PREC_SUFFIX, 1},
  {"'access", UNOP_ADDR, PREC_SUFFIX, 1},
  {"'size", OP_ATR_SIZE, PREC_SUFFIX, 1},
  {NULL, 0, 0, 0}
};

				/* Fundamental Ada Types */

/* Create a fundamental Ada type using default reasonable for the current
   target machine.

   Some object/debugging file formats (DWARF version 1, COFF, etc) do not
   define fundamental types such as "int" or "double".  Others (stabs or
   DWARF version 2, etc) do define fundamental types.  For the formats which
   don't provide fundamental types, gdb can create such types using this
   function.

   FIXME:  Some compilers distinguish explicitly signed integral types
   (signed short, signed int, signed long) from "regular" integral types
   (short, int, long) in the debugging information.  There is some dis-
   agreement as to how useful this feature is.  In particular, gcc does
   not support this.  Also, only some debugging formats allow the
   distinction to be passed on to a debugger.  For now, we always just
   use "short", "int", or "long" as the type name, for both the implicit
   and explicitly signed types.  This also makes life easier for the
   gdb test suite since we don't have to account for the differences
   in output depending upon what the compiler and debugging format
   support.  We will probably have to re-examine the issue when gdb
   starts taking it's fundamental type information directly from the
   debugging information supplied by the compiler.  fnf@cygnus.com */

static struct type *
ada_create_fundamental_type (struct objfile *objfile, int typeid)
{
  struct type *type = NULL;

  switch (typeid)
    {
    default:
      /* FIXME:  For now, if we are asked to produce a type not in this
         language, create the equivalent of a C integer type with the
         name "<?type?>".  When all the dust settles from the type
         reconstruction work, this should probably become an error.  */
      type = init_type (TYPE_CODE_INT,
                        gdbarch_int_bit (current_gdbarch) / TARGET_CHAR_BIT,
                        0, "<?type?>", objfile);
      warning (_("internal error: no Ada fundamental type %d"), typeid);
      break;
    case FT_VOID:
      type = init_type (TYPE_CODE_VOID,
                        TARGET_CHAR_BIT / TARGET_CHAR_BIT,
                        0, "void", objfile);
      break;
    case FT_CHAR:
      type = init_type (TYPE_CODE_INT,
                        TARGET_CHAR_BIT / TARGET_CHAR_BIT,
                        0, "character", objfile);
      break;
    case FT_SIGNED_CHAR:
      type = init_type (TYPE_CODE_INT,
                        TARGET_CHAR_BIT / TARGET_CHAR_BIT,
                        0, "signed char", objfile);
      break;
    case FT_UNSIGNED_CHAR:
      type = init_type (TYPE_CODE_INT,
                        TARGET_CHAR_BIT / TARGET_CHAR_BIT,
                        TYPE_FLAG_UNSIGNED, "unsigned char", objfile);
      break;
    case FT_SHORT:
      type = init_type (TYPE_CODE_INT,
                        gdbarch_short_bit (current_gdbarch) / TARGET_CHAR_BIT,
                        0, "short_integer", objfile);
      break;
    case FT_SIGNED_SHORT:
      type = init_type (TYPE_CODE_INT,
                        gdbarch_short_bit (current_gdbarch) / TARGET_CHAR_BIT,
                        0, "short_integer", objfile);
      break;
    case FT_UNSIGNED_SHORT:
      type = init_type (TYPE_CODE_INT,
                        gdbarch_short_bit (current_gdbarch) / TARGET_CHAR_BIT,
                        TYPE_FLAG_UNSIGNED, "unsigned short", objfile);
      break;
    case FT_INTEGER:
      type = init_type (TYPE_CODE_INT,
                        gdbarch_int_bit (current_gdbarch) / TARGET_CHAR_BIT,
                        0, "integer", objfile);
      break;
    case FT_SIGNED_INTEGER:
      type = init_type (TYPE_CODE_INT,
			gdbarch_int_bit (current_gdbarch) / TARGET_CHAR_BIT,
			0, "integer", objfile);        /* FIXME -fnf */
      break;
    case FT_UNSIGNED_INTEGER:
      type = init_type (TYPE_CODE_INT,
                        gdbarch_int_bit (current_gdbarch) / TARGET_CHAR_BIT,
                        TYPE_FLAG_UNSIGNED, "unsigned int", objfile);
      break;
    case FT_LONG:
      type = init_type (TYPE_CODE_INT,
                        gdbarch_long_bit (current_gdbarch) / TARGET_CHAR_BIT,
                        0, "long_integer", objfile);
      break;
    case FT_SIGNED_LONG:
      type = init_type (TYPE_CODE_INT,
                        gdbarch_long_bit (current_gdbarch) / TARGET_CHAR_BIT,
                        0, "long_integer", objfile);
      break;
    case FT_UNSIGNED_LONG:
      type = init_type (TYPE_CODE_INT,
                        gdbarch_long_bit (current_gdbarch) / TARGET_CHAR_BIT,
                        TYPE_FLAG_UNSIGNED, "unsigned long", objfile);
      break;
    case FT_LONG_LONG:
      type = init_type (TYPE_CODE_INT,
			gdbarch_long_long_bit (current_gdbarch)
			  / TARGET_CHAR_BIT,
			0, "long_long_integer", objfile);
      break;
    case FT_SIGNED_LONG_LONG:
      type = init_type (TYPE_CODE_INT,
			gdbarch_long_long_bit (current_gdbarch)
			  / TARGET_CHAR_BIT,
			0, "long_long_integer", objfile);
      break;
    case FT_UNSIGNED_LONG_LONG:
      type = init_type (TYPE_CODE_INT,
			gdbarch_long_long_bit (current_gdbarch)
			  / TARGET_CHAR_BIT,
			TYPE_FLAG_UNSIGNED, "unsigned long long", objfile);
      break;
    case FT_FLOAT:
      type = init_type (TYPE_CODE_FLT,
                        gdbarch_float_bit (current_gdbarch) / TARGET_CHAR_BIT,
                        0, "float", objfile);
      break;
    case FT_DBL_PREC_FLOAT:
      type = init_type (TYPE_CODE_FLT,
                        gdbarch_double_bit (current_gdbarch) / TARGET_CHAR_BIT,
                        0, "long_float", objfile);
      break;
    case FT_EXT_PREC_FLOAT:
      type = init_type (TYPE_CODE_FLT,
                        gdbarch_long_double_bit (current_gdbarch)
			  / TARGET_CHAR_BIT,
                        0, "long_long_float", objfile);
      break;
    }
  return (type);
}

enum ada_primitive_types {
  ada_primitive_type_int,
  ada_primitive_type_long,
  ada_primitive_type_short,
  ada_primitive_type_char,
  ada_primitive_type_float,
  ada_primitive_type_double,
  ada_primitive_type_void,
  ada_primitive_type_long_long,
  ada_primitive_type_long_double,
  ada_primitive_type_natural,
  ada_primitive_type_positive,
  ada_primitive_type_system_address,
  nr_ada_primitive_types
};

static void
ada_language_arch_info (struct gdbarch *current_gdbarch,
			struct language_arch_info *lai)
{
  const struct builtin_type *builtin = builtin_type (current_gdbarch);
  lai->primitive_type_vector
    = GDBARCH_OBSTACK_CALLOC (current_gdbarch, nr_ada_primitive_types + 1,
			      struct type *);
  lai->primitive_type_vector [ada_primitive_type_int] =
    init_type (TYPE_CODE_INT,
	       gdbarch_int_bit (current_gdbarch) / TARGET_CHAR_BIT,
	       0, "integer", (struct objfile *) NULL);
  lai->primitive_type_vector [ada_primitive_type_long] =
    init_type (TYPE_CODE_INT,
	       gdbarch_long_bit (current_gdbarch) / TARGET_CHAR_BIT,
	       0, "long_integer", (struct objfile *) NULL);
  lai->primitive_type_vector [ada_primitive_type_short] =
    init_type (TYPE_CODE_INT,
	       gdbarch_short_bit (current_gdbarch) / TARGET_CHAR_BIT,
	       0, "short_integer", (struct objfile *) NULL);
  lai->string_char_type = 
    lai->primitive_type_vector [ada_primitive_type_char] =
    init_type (TYPE_CODE_INT, TARGET_CHAR_BIT / TARGET_CHAR_BIT,
               0, "character", (struct objfile *) NULL);
  lai->primitive_type_vector [ada_primitive_type_float] =
    init_type (TYPE_CODE_FLT,
	       gdbarch_float_bit (current_gdbarch)/ TARGET_CHAR_BIT,
               0, "float", (struct objfile *) NULL);
  lai->primitive_type_vector [ada_primitive_type_double] =
    init_type (TYPE_CODE_FLT,
	       gdbarch_double_bit (current_gdbarch) / TARGET_CHAR_BIT,
               0, "long_float", (struct objfile *) NULL);
  lai->primitive_type_vector [ada_primitive_type_long_long] =
    init_type (TYPE_CODE_INT, 
	       gdbarch_long_long_bit (current_gdbarch) / TARGET_CHAR_BIT,
               0, "long_long_integer", (struct objfile *) NULL);
  lai->primitive_type_vector [ada_primitive_type_long_double] =
    init_type (TYPE_CODE_FLT,
	       gdbarch_double_bit (current_gdbarch) / TARGET_CHAR_BIT,
               0, "long_long_float", (struct objfile *) NULL);
  lai->primitive_type_vector [ada_primitive_type_natural] =
    init_type (TYPE_CODE_INT,
	       gdbarch_int_bit (current_gdbarch) / TARGET_CHAR_BIT,
	       0, "natural", (struct objfile *) NULL);
  lai->primitive_type_vector [ada_primitive_type_positive] =
    init_type (TYPE_CODE_INT,
	       gdbarch_int_bit (current_gdbarch) / TARGET_CHAR_BIT,
	       0, "positive", (struct objfile *) NULL);
  lai->primitive_type_vector [ada_primitive_type_void] = builtin->builtin_void;

  lai->primitive_type_vector [ada_primitive_type_system_address] =
    lookup_pointer_type (init_type (TYPE_CODE_VOID, 1, 0, "void",
                                    (struct objfile *) NULL));
  TYPE_NAME (lai->primitive_type_vector [ada_primitive_type_system_address])
    = "system__address";
}

				/* Language vector */

/* Not really used, but needed in the ada_language_defn.  */

static void
emit_char (int c, struct ui_file *stream, int quoter)
{
  ada_emit_char (c, stream, quoter, 1);
}

static int
parse (void)
{
  warnings_issued = 0;
  return ada_parse ();
}

static const struct exp_descriptor ada_exp_descriptor = {
  ada_print_subexp,
  ada_operator_length,
  ada_op_name,
  ada_dump_subexp_body,
  ada_evaluate_subexp
};

const struct language_defn ada_language_defn = {
  "ada",                        /* Language name */
  language_ada,
  NULL,
  range_check_off,
  type_check_off,
  case_sensitive_on,            /* Yes, Ada is case-insensitive, but
                                   that's not quite what this means.  */
  array_row_major,
  &ada_exp_descriptor,
  parse,
  ada_error,
  resolve,
  ada_printchar,                /* Print a character constant */
  ada_printstr,                 /* Function to print string constant */
  emit_char,                    /* Function to print single char (not used) */
  ada_create_fundamental_type,  /* Create fundamental type in this language */
  ada_print_type,               /* Print a type using appropriate syntax */
  ada_val_print,                /* Print a value using appropriate syntax */
  ada_value_print,              /* Print a top-level value */
  NULL,                         /* Language specific skip_trampoline */
  NULL,                         /* value_of_this */
  ada_lookup_symbol_nonlocal,   /* Looking up non-local symbols.  */
  basic_lookup_transparent_type,        /* lookup_transparent_type */
  ada_la_decode,                /* Language specific symbol demangler */
  NULL,                         /* Language specific class_name_from_physname */
  ada_op_print_tab,             /* expression operators for printing */
  0,                            /* c-style arrays */
  1,                            /* String lower bound */
  NULL,
  ada_get_gdb_completer_word_break_characters,
  ada_language_arch_info,
  ada_print_array_index,
  LANG_MAGIC
};

void
_initialize_ada_language (void)
{
  add_language (&ada_language_defn);

  varsize_limit = 65536;

  obstack_init (&symbol_list_obstack);

  decoded_names_store = htab_create_alloc
    (256, htab_hash_string, (int (*)(const void *, const void *)) streq,
     NULL, xcalloc, xfree);
}
