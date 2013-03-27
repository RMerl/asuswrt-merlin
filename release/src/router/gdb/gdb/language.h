/* Source-language-related definitions for GDB.

   Copyright (C) 1991, 1992, 1993, 1994, 1995, 1998, 1999, 2000, 2003, 2004,
   2007 Free Software Foundation, Inc.

   Contributed by the Department of Computer Science at the State University
   of New York at Buffalo.

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

#if !defined (LANGUAGE_H)
#define LANGUAGE_H 1

/* Forward decls for prototypes */
struct value;
struct objfile;
struct frame_info;
struct expression;
struct ui_file;

/* enum exp_opcode;     ANSI's `wisdom' didn't include forward enum decls. */

/* This used to be included to configure GDB for one or more specific
   languages.  Now it is left out to configure for all of them.  FIXME.  */
/* #include "lang_def.h" */
#define	_LANG_c
#define	_LANG_m2
#define  _LANG_fortran
#define  _LANG_pascal

#define MAX_FORTRAN_DIMS  7	/* Maximum number of F77 array dims */

/* range_mode ==
   range_mode_auto:   range_check set automatically to default of language.
   range_mode_manual: range_check set manually by user.  */

extern enum range_mode
  {
    range_mode_auto, range_mode_manual
  }
range_mode;

/* range_check ==
   range_check_on:    Ranges are checked in GDB expressions, producing errors.
   range_check_warn:  Ranges are checked, producing warnings.
   range_check_off:   Ranges are not checked in GDB expressions.  */

extern enum range_check
  {
    range_check_off, range_check_warn, range_check_on
  }
range_check;

/* type_mode ==
   type_mode_auto:   type_check set automatically to default of language
   type_mode_manual: type_check set manually by user. */

extern enum type_mode
  {
    type_mode_auto, type_mode_manual
  }
type_mode;

/* type_check ==
   type_check_on:    Types are checked in GDB expressions, producing errors.
   type_check_warn:  Types are checked, producing warnings.
   type_check_off:   Types are not checked in GDB expressions.  */

extern enum type_check
  {
    type_check_off, type_check_warn, type_check_on
  }
type_check;

/* case_mode ==
   case_mode_auto:   case_sensitivity set upon selection of scope 
   case_mode_manual: case_sensitivity set only by user.  */

extern enum case_mode
  {
    case_mode_auto, case_mode_manual
  }
case_mode;

/* array_ordering ==
   array_row_major:     Arrays are in row major order
   array_column_major:  Arrays are in column major order.*/

extern enum array_ordering
  {
    array_row_major, array_column_major
  } 
array_ordering;


/* case_sensitivity ==
   case_sensitive_on:   Case sensitivity in name matching is used
   case_sensitive_off:  Case sensitivity in name matching is not used  */

extern enum case_sensitivity
  {
    case_sensitive_on, case_sensitive_off
  }
case_sensitivity;

/* Per architecture (OS/ABI) language information.  */

struct language_arch_info
{
  /* Its primitive types.  This is a vector ended by a NULL pointer.
     These types can be specified by name in parsing types in
     expressions, regardless of whether the program being debugged
     actually defines such a type.  */
  struct type **primitive_type_vector;
  /* Type of elements of strings. */
  struct type *string_char_type;
};

struct type *language_string_char_type (const struct language_defn *l,
					struct gdbarch *gdbarch);

struct type *language_lookup_primitive_type_by_name (const struct language_defn *l,
						     struct gdbarch *gdbarch,
						     const char *name);

/* Structure tying together assorted information about a language.  */

struct language_defn
  {
    /* Name of the language */

    char *la_name;

    /* its symtab language-enum (defs.h) */

    enum language la_language;

    /* Its builtin types.  This is a vector ended by a NULL pointer.  These
       types can be specified by name in parsing types in expressions,
       regardless of whether the program being debugged actually defines
       such a type.  */

    struct type **const *la_builtin_type_vector;

    /* Default range checking */

    enum range_check la_range_check;

    /* Default type checking */

    enum type_check la_type_check;

    /* Default case sensitivity */
    enum case_sensitivity la_case_sensitivity;

    /* Multi-dimensional array ordering */
    enum array_ordering la_array_ordering;

    /* Definitions related to expression printing, prefixifying, and
       dumping */

    const struct exp_descriptor *la_exp_desc;

    /* Parser function. */

    int (*la_parser) (void);

    /* Parser error function */

    void (*la_error) (char *);

    /* Given an expression *EXPP created by prefixifying the result of
       la_parser, perform any remaining processing necessary to complete
       its translation.  *EXPP may change; la_post_parser is responsible 
       for releasing its previous contents, if necessary.  If 
       VOID_CONTEXT_P, then no value is expected from the expression.  */

    void (*la_post_parser) (struct expression ** expp, int void_context_p);

    void (*la_printchar) (int ch, struct ui_file * stream);

    void (*la_printstr) (struct ui_file * stream, const gdb_byte *string,
			 unsigned int length, int width,
			 int force_ellipses);

    void (*la_emitchar) (int ch, struct ui_file * stream, int quoter);

    struct type *(*la_fund_type) (struct objfile *, int);

    /* Print a type using syntax appropriate for this language. */

    void (*la_print_type) (struct type *, char *, struct ui_file *, int,
			   int);

    /* Print a value using syntax appropriate for this language. */

    int (*la_val_print) (struct type *, const gdb_byte *, int, CORE_ADDR,
			 struct ui_file *, int, int, int,
			 enum val_prettyprint);

    /* Print a top-level value using syntax appropriate for this language. */

    int (*la_value_print) (struct value *, struct ui_file *,
			   int, enum val_prettyprint);

    /* PC is possibly an unknown languages trampoline.
       If that PC falls in a trampoline belonging to this language,
       return the address of the first pc in the real function, or 0
       if it isn't a language tramp for this language.  */
    CORE_ADDR (*skip_trampoline) (struct frame_info *, CORE_ADDR);

    /* Now come some hooks for lookup_symbol.  */

    /* If this is non-NULL, lookup_symbol will do the 'field_of_this'
       check, using this function to find the value of this.  */

    /* FIXME: carlton/2003-05-19: Audit all the language_defn structs
       to make sure we're setting this appropriately: I'm sure it
       could be NULL in more languages.  */

    struct value *(*la_value_of_this) (int complain);

    /* This is a function that lookup_symbol will call when it gets to
       the part of symbol lookup where C looks up static and global
       variables.  */

    struct symbol *(*la_lookup_symbol_nonlocal) (const char *,
						 const char *,
						 const struct block *,
						 const domain_enum,
						 struct symtab **);

    /* Find the definition of the type with the given name.  */
    struct type *(*la_lookup_transparent_type) (const char *);

    /* Return demangled language symbol, or NULL.  */
    char *(*la_demangle) (const char *mangled, int options);

    /* Return class name of a mangled method name or NULL.  */
    char *(*la_class_name_from_physname) (const char *physname);

    /* Table for printing expressions */

    const struct op_print *la_op_print_tab;

    /* Zero if the language has first-class arrays.  True if there are no
       array values, and array objects decay to pointers, as in C. */

    char c_style_arrays;

    /* Index to use for extracting the first element of a string. */
    char string_lower_bound;

    /* Type of elements of strings. */
    struct type **string_char_type;

    /* The list of characters forming word boundaries.  */
    char *(*la_word_break_characters) (void);

    /* The per-architecture (OS/ABI) language information.  */
    void (*la_language_arch_info) (struct gdbarch *,
				   struct language_arch_info *);

    /* Print the index of an element of an array.  */
    void (*la_print_array_index) (struct value *index_value,
                                  struct ui_file *stream,
                                  int format,
                                  enum val_prettyprint pretty);

    /* Add fields above this point, so the magic number is always last. */
    /* Magic number for compat checking */

    long la_magic;

  };

#define LANG_MAGIC	910823L

/* Pointer to the language_defn for our current language.  This pointer
   always points to *some* valid struct; it can be used without checking
   it for validity.

   The current language affects expression parsing and evaluation
   (FIXME: it might be cleaner to make the evaluation-related stuff
   separate exp_opcodes for each different set of semantics.  We
   should at least think this through more clearly with respect to
   what happens if the language is changed between parsing and
   evaluation) and printing of things like types and arrays.  It does
   *not* affect symbol-reading-- each source file in a symbol-file has
   its own language and we should keep track of that regardless of the
   language when symbols are read.  If we want some manual setting for
   the language of symbol files (e.g. detecting when ".c" files are
   C++), it should be a separate setting from the current_language.  */

extern const struct language_defn *current_language;

/* Pointer to the language_defn expected by the user, e.g. the language
   of main(), or the language we last mentioned in a message, or C.  */

extern const struct language_defn *expected_language;

/* language_mode == 
   language_mode_auto:   current_language automatically set upon selection
   of scope (e.g. stack frame)
   language_mode_manual: current_language set only by user.  */

extern enum language_mode
  {
    language_mode_auto, language_mode_manual
  }
language_mode;

/* These macros define the behaviour of the expression 
   evaluator.  */

/* Should we strictly type check expressions? */
#define STRICT_TYPE (type_check != type_check_off)

/* Should we range check values against the domain of their type? */
#define RANGE_CHECK (range_check != range_check_off)

/* "cast" really means conversion */
/* FIXME -- should be a setting in language_defn */
#define CAST_IS_CONVERSION (current_language->la_language == language_c  || \
			    current_language->la_language == language_cplus || \
			    current_language->la_language == language_objc)

extern void language_info (int);

extern enum language set_language (enum language);


/* This page contains functions that return things that are
   specific to languages.  Each of these functions is based on
   the current setting of working_lang, which the user sets
   with the "set language" command. */

#define create_fundamental_type(objfile,typeid) \
  (current_language->la_fund_type(objfile, typeid))

#define LA_PRINT_TYPE(type,varstring,stream,show,level) \
  (current_language->la_print_type(type,varstring,stream,show,level))

#define LA_VAL_PRINT(type,valaddr,offset,addr,stream,fmt,deref,recurse,pretty) \
  (current_language->la_val_print(type,valaddr,offset,addr,stream,fmt,deref, \
				  recurse,pretty))
#define LA_VALUE_PRINT(val,stream,fmt,pretty) \
  (current_language->la_value_print(val,stream,fmt,pretty))

#define LA_PRINT_CHAR(ch, stream) \
  (current_language->la_printchar(ch, stream))
#define LA_PRINT_STRING(stream, string, length, width, force_ellipses) \
  (current_language->la_printstr(stream, string, length, width, force_ellipses))
#define LA_EMIT_CHAR(ch, stream, quoter) \
  (current_language->la_emitchar(ch, stream, quoter))

#define LA_PRINT_ARRAY_INDEX(index_value, stream, format, pretty) \
  (current_language->la_print_array_index(index_value, stream, format, pretty))

/* Test a character to decide whether it can be printed in literal form
   or needs to be printed in another representation.  For example,
   in C the literal form of the character with octal value 141 is 'a'
   and the "other representation" is '\141'.  The "other representation"
   is program language dependent. */

#define PRINT_LITERAL_FORM(c)		\
  ((c) >= 0x20				\
   && ((c) < 0x7F || (c) >= 0xA0)	\
   && (!sevenbit_strings || (c) < 0x80))

#if 0
/* FIXME: cagney/2000-03-04: This function does not appear to be used.
   It can be deleted once 5.0 has been released. */
/* Return a string that contains the hex digits of the number.  No preceeding
   "0x" */

extern char *longest_raw_hex_string (LONGEST);
#endif

/* Type predicates */

extern int simple_type (struct type *);

extern int ordered_type (struct type *);

extern int same_type (struct type *, struct type *);

extern int integral_type (struct type *);

extern int numeric_type (struct type *);

extern int character_type (struct type *);

extern int boolean_type (struct type *);

extern int float_type (struct type *);

extern int pointer_type (struct type *);

extern int structured_type (struct type *);

/* Checks Binary and Unary operations for semantic type correctness */
/* FIXME:  Does not appear to be used */
#define unop_type_check(v,o) binop_type_check((v),NULL,(o))

extern void binop_type_check (struct value *, struct value *, int);

/* Error messages */

extern void op_error (const char *lhs, enum exp_opcode,
		      const char *rhs);

extern void type_error (const char *, ...) ATTR_FORMAT (printf, 1, 2);

extern void range_error (const char *, ...) ATTR_FORMAT (printf, 1, 2);

/* Data:  Does this value represent "truth" to the current language?  */

extern int value_true (struct value *);

extern struct type *lang_bool_type (void);

/* The type used for Boolean values in the current language. */
#define LA_BOOL_TYPE lang_bool_type ()

/* Misc:  The string representing a particular enum language.  */

extern enum language language_enum (char *str);

extern const struct language_defn *language_def (enum language);

extern char *language_str (enum language);

/* Add a language to the set known by GDB (at initialization time).  */

extern void add_language (const struct language_defn *);

extern enum language get_frame_language (void);	/* In stack.c */

/* Check for a language-specific trampoline. */

extern CORE_ADDR skip_language_trampoline (struct frame_info *, CORE_ADDR pc);

/* Return demangled language symbol, or NULL.  */
extern char *language_demangle (const struct language_defn *current_language, 
				const char *mangled, int options);

/* Return class name from physname, or NULL.  */
extern char *language_class_name_from_physname (const struct language_defn *,
					        const char *physname);

/* Splitting strings into words.  */
extern char *default_word_break_characters (void);

/* Print the index of an array element using the C99 syntax.  */
extern void default_print_array_index (struct value *index_value,
                                       struct ui_file *stream,
                                       int format,
                                       enum val_prettyprint pretty);

#endif /* defined (LANGUAGE_H) */
