/* Multiple source language support for GDB.

   Copyright (C) 1991, 1992, 1993, 1994, 1995, 1996, 1998, 1999, 2000, 2001,
   2002, 2003, 2004, 2005, 2007 Free Software Foundation, Inc.

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

/* This file contains functions that return things that are specific
   to languages.  Each function should examine current_language if necessary,
   and return the appropriate result. */

/* FIXME:  Most of these would be better organized as macros which
   return data out of a "language-specific" struct pointer that is set
   whenever the working language changes.  That would be a lot faster.  */

#include "defs.h"
#include <ctype.h>
#include "gdb_string.h"

#include "symtab.h"
#include "gdbtypes.h"
#include "value.h"
#include "gdbcmd.h"
#include "expression.h"
#include "language.h"
#include "target.h"
#include "parser-defs.h"
#include "jv-lang.h"
#include "demangle.h"

extern void _initialize_language (void);

static void set_case_str (void);

static void set_range_str (void);

static void set_type_str (void);

static void set_lang_str (void);

static void unk_lang_error (char *);

static int unk_lang_parser (void);

static void show_check (char *, int);

static void set_check (char *, int);

static void set_type_range_case (void);

static void unk_lang_emit_char (int c, struct ui_file *stream, int quoter);

static void unk_lang_printchar (int c, struct ui_file *stream);

static struct type *unk_lang_create_fundamental_type (struct objfile *, int);

static void unk_lang_print_type (struct type *, char *, struct ui_file *,
				 int, int);

static int unk_lang_value_print (struct value *, struct ui_file *, int, enum val_prettyprint);

static CORE_ADDR unk_lang_trampoline (struct frame_info *, CORE_ADDR pc);

/* Forward declaration */
extern const struct language_defn unknown_language_defn;

/* The current (default at startup) state of type and range checking.
   (If the modes are set to "auto", though, these are changed based
   on the default language at startup, and then again based on the
   language of the first source file.  */

enum range_mode range_mode = range_mode_auto;
enum range_check range_check = range_check_off;
enum type_mode type_mode = type_mode_auto;
enum type_check type_check = type_check_off;
enum case_mode case_mode = case_mode_auto;
enum case_sensitivity case_sensitivity = case_sensitive_on;

/* The current language and language_mode (see language.h) */

const struct language_defn *current_language = &unknown_language_defn;
enum language_mode language_mode = language_mode_auto;

/* The language that the user expects to be typing in (the language
   of main(), or the last language we notified them about, or C).  */

const struct language_defn *expected_language;

/* The list of supported languages.  The list itself is malloc'd.  */

static const struct language_defn **languages;
static unsigned languages_size;
static unsigned languages_allocsize;
#define	DEFAULT_ALLOCSIZE 4

/* The "set language/type/range" commands all put stuff in these
   buffers.  This is to make them work as set/show commands.  The
   user's string is copied here, then the set_* commands look at
   them and update them to something that looks nice when it is
   printed out. */

static char *language;
static char *type;
static char *range;
static char *case_sensitive;

/* Warning issued when current_language and the language of the current
   frame do not match. */
char lang_frame_mismatch_warn[] =
"Warning: the current language does not match this frame.";

/* This page contains the functions corresponding to GDB commands
   and their helpers. */

/* Show command.  Display a warning if the language set
   does not match the frame. */
static void
show_language_command (struct ui_file *file, int from_tty,
		       struct cmd_list_element *c, const char *value)
{
  enum language flang;		/* The language of the current frame */

  deprecated_show_value_hack (file, from_tty, c, value);
  flang = get_frame_language ();
  if (flang != language_unknown &&
      language_mode == language_mode_manual &&
      current_language->la_language != flang)
    printf_filtered ("%s\n", lang_frame_mismatch_warn);
}

/* Set command.  Change the current working language. */
static void
set_language_command (char *ignore, int from_tty, struct cmd_list_element *c)
{
  int i;
  enum language flang;
  char *err_lang;

  if (!language || !language[0])
    {
      printf_unfiltered (_("\
The currently understood settings are:\n\n\
local or auto    Automatic setting based on source file\n"));

      for (i = 0; i < languages_size; ++i)
	{
	  /* Already dealt with these above.  */
	  if (languages[i]->la_language == language_unknown
	      || languages[i]->la_language == language_auto)
	    continue;

	  /* FIXME: i18n: for now assume that the human-readable name
	     is just a capitalization of the internal name.  */
	  printf_unfiltered ("%-16s Use the %c%s language\n",
			     languages[i]->la_name,
	  /* Capitalize first letter of language
	     name.  */
			     toupper (languages[i]->la_name[0]),
			     languages[i]->la_name + 1);
	}
      /* Restore the silly string. */
      set_language (current_language->la_language);
      return;
    }

  /* Search the list of languages for a match.  */
  for (i = 0; i < languages_size; i++)
    {
      if (strcmp (languages[i]->la_name, language) == 0)
	{
	  /* Found it!  Go into manual mode, and use this language.  */
	  if (languages[i]->la_language == language_auto)
	    {
	      /* Enter auto mode.  Set to the current frame's language, if known.  */
	      language_mode = language_mode_auto;
	      flang = get_frame_language ();
	      if (flang != language_unknown)
		set_language (flang);
	      expected_language = current_language;
	      return;
	    }
	  else
	    {
	      /* Enter manual mode.  Set the specified language.  */
	      language_mode = language_mode_manual;
	      current_language = languages[i];
	      set_type_range_case ();
	      set_lang_str ();
	      expected_language = current_language;
	      return;
	    }
	}
    }

  /* Reset the language (esp. the global string "language") to the 
     correct values. */
  err_lang = savestring (language, strlen (language));
  make_cleanup (xfree, err_lang);	/* Free it after error */
  set_language (current_language->la_language);
  error (_("Unknown language `%s'."), err_lang);
}

/* Show command.  Display a warning if the type setting does
   not match the current language. */
static void
show_type_command (struct ui_file *file, int from_tty,
		   struct cmd_list_element *c, const char *value)
{
  deprecated_show_value_hack (file, from_tty, c, value);
  if (type_check != current_language->la_type_check)
    printf_unfiltered (
			"Warning: the current type check setting does not match the language.\n");
}

/* Set command.  Change the setting for type checking. */
static void
set_type_command (char *ignore, int from_tty, struct cmd_list_element *c)
{
  if (strcmp (type, "on") == 0)
    {
      type_check = type_check_on;
      type_mode = type_mode_manual;
    }
  else if (strcmp (type, "warn") == 0)
    {
      type_check = type_check_warn;
      type_mode = type_mode_manual;
    }
  else if (strcmp (type, "off") == 0)
    {
      type_check = type_check_off;
      type_mode = type_mode_manual;
    }
  else if (strcmp (type, "auto") == 0)
    {
      type_mode = type_mode_auto;
      set_type_range_case ();
      /* Avoid hitting the set_type_str call below.  We
         did it in set_type_range_case. */
      return;
    }
  else
    {
      warning (_("Unrecognized type check setting: \"%s\""), type);
    }
  set_type_str ();
  show_type_command (NULL, from_tty, NULL, NULL);
}

/* Show command.  Display a warning if the range setting does
   not match the current language. */
static void
show_range_command (struct ui_file *file, int from_tty,
		    struct cmd_list_element *c, const char *value)
{
  deprecated_show_value_hack (file, from_tty, c, value);
  if (range_check != current_language->la_range_check)
    printf_unfiltered (
			"Warning: the current range check setting does not match the language.\n");
}

/* Set command.  Change the setting for range checking. */
static void
set_range_command (char *ignore, int from_tty, struct cmd_list_element *c)
{
  if (strcmp (range, "on") == 0)
    {
      range_check = range_check_on;
      range_mode = range_mode_manual;
    }
  else if (strcmp (range, "warn") == 0)
    {
      range_check = range_check_warn;
      range_mode = range_mode_manual;
    }
  else if (strcmp (range, "off") == 0)
    {
      range_check = range_check_off;
      range_mode = range_mode_manual;
    }
  else if (strcmp (range, "auto") == 0)
    {
      range_mode = range_mode_auto;
      set_type_range_case ();
      /* Avoid hitting the set_range_str call below.  We
         did it in set_type_range_case. */
      return;
    }
  else
    {
      warning (_("Unrecognized range check setting: \"%s\""), range);
    }
  set_range_str ();
  show_range_command (NULL, from_tty, NULL, NULL);
}

/* Show command.  Display a warning if the case sensitivity setting does
   not match the current language. */
static void
show_case_command (struct ui_file *file, int from_tty,
		   struct cmd_list_element *c, const char *value)
{
  deprecated_show_value_hack (file, from_tty, c, value);
  if (case_sensitivity != current_language->la_case_sensitivity)
    printf_unfiltered(
"Warning: the current case sensitivity setting does not match the language.\n");
}

/* Set command.  Change the setting for case sensitivity.  */

static void
set_case_command (char *ignore, int from_tty, struct cmd_list_element *c)
{
   if (strcmp (case_sensitive, "on") == 0)
     {
       case_sensitivity = case_sensitive_on;
       case_mode = case_mode_manual;
     }
   else if (strcmp (case_sensitive, "off") == 0)
     {
       case_sensitivity = case_sensitive_off;
       case_mode = case_mode_manual;
     }
   else if (strcmp (case_sensitive, "auto") == 0)
     {
       case_mode = case_mode_auto;
       set_type_range_case ();
       /* Avoid hitting the set_case_str call below.  We did it in
	  set_type_range_case.  */
       return;
     }
   else
     {
       warning (_("Unrecognized case-sensitive setting: \"%s\""),
		case_sensitive);
     }
   set_case_str();
   show_case_command (NULL, from_tty, NULL, NULL);
}

/* Set the status of range and type checking and case sensitivity based on
   the current modes and the current language.
   If SHOW is non-zero, then print out the current language,
   type and range checking status. */
static void
set_type_range_case (void)
{

  if (range_mode == range_mode_auto)
    range_check = current_language->la_range_check;

  if (type_mode == type_mode_auto)
    type_check = current_language->la_type_check;

  if (case_mode == case_mode_auto)
    case_sensitivity = current_language->la_case_sensitivity;

  set_type_str ();
  set_range_str ();
  set_case_str ();
}

/* Set current language to (enum language) LANG.  Returns previous language. */

enum language
set_language (enum language lang)
{
  int i;
  enum language prev_language;

  prev_language = current_language->la_language;

  for (i = 0; i < languages_size; i++)
    {
      if (languages[i]->la_language == lang)
	{
	  current_language = languages[i];
	  set_type_range_case ();
	  set_lang_str ();
	  break;
	}
    }

  return prev_language;
}

/* This page contains functions that update the global vars
   language, type and range. */
static void
set_lang_str (void)
{
  char *prefix = "";

  if (language)
    xfree (language);
  if (language_mode == language_mode_auto)
    prefix = "auto; currently ";

  language = concat (prefix, current_language->la_name, (char *)NULL);
}

static void
set_type_str (void)
{
  char *tmp = NULL, *prefix = "";

  if (type)
    xfree (type);
  if (type_mode == type_mode_auto)
    prefix = "auto; currently ";

  switch (type_check)
    {
    case type_check_on:
      tmp = "on";
      break;
    case type_check_off:
      tmp = "off";
      break;
    case type_check_warn:
      tmp = "warn";
      break;
    default:
      error (_("Unrecognized type check setting."));
    }

  type = concat (prefix, tmp, (char *)NULL);
}

static void
set_range_str (void)
{
  char *tmp, *pref = "";

  if (range_mode == range_mode_auto)
    pref = "auto; currently ";

  switch (range_check)
    {
    case range_check_on:
      tmp = "on";
      break;
    case range_check_off:
      tmp = "off";
      break;
    case range_check_warn:
      tmp = "warn";
      break;
    default:
      error (_("Unrecognized range check setting."));
    }

  if (range)
    xfree (range);
  range = concat (pref, tmp, (char *)NULL);
}

static void
set_case_str (void)
{
   char *tmp = NULL, *prefix = "";

   if (case_mode==case_mode_auto)
      prefix = "auto; currently ";

   switch (case_sensitivity)
   {
   case case_sensitive_on:
     tmp = "on";
     break;
   case case_sensitive_off:
     tmp = "off";
     break;
   default:
     error (_("Unrecognized case-sensitive setting."));
   }

   xfree (case_sensitive);
   case_sensitive = concat (prefix, tmp, (char *)NULL);
}

/* Print out the current language settings: language, range and
   type checking.  If QUIETLY, print only what has changed.  */

void
language_info (int quietly)
{
  if (quietly && expected_language == current_language)
    return;

  expected_language = current_language;
  printf_unfiltered (_("Current language:  %s\n"), language);
  show_language_command (NULL, 1, NULL, NULL);

  if (!quietly)
    {
      printf_unfiltered (_("Type checking:     %s\n"), type);
      show_type_command (NULL, 1, NULL, NULL);
      printf_unfiltered (_("Range checking:    %s\n"), range);
      show_range_command (NULL, 1, NULL, NULL);
      printf_unfiltered (_("Case sensitivity:  %s\n"), case_sensitive);
      show_case_command (NULL, 1, NULL, NULL);
    }
}

/* Return the result of a binary operation. */

#if 0				/* Currently unused */

struct type *
binop_result_type (struct value *v1, struct value *v2)
{
  int size, uns;
  struct type *t1 = check_typedef (VALUE_TYPE (v1));
  struct type *t2 = check_typedef (VALUE_TYPE (v2));

  int l1 = TYPE_LENGTH (t1);
  int l2 = TYPE_LENGTH (t2);

  switch (current_language->la_language)
    {
    case language_c:
    case language_cplus:
    case language_objc:
      if (TYPE_CODE (t1) == TYPE_CODE_FLT)
	return TYPE_CODE (t2) == TYPE_CODE_FLT && l2 > l1 ?
	  VALUE_TYPE (v2) : VALUE_TYPE (v1);
      else if (TYPE_CODE (t2) == TYPE_CODE_FLT)
	return TYPE_CODE (t1) == TYPE_CODE_FLT && l1 > l2 ?
	  VALUE_TYPE (v1) : VALUE_TYPE (v2);
      else if (TYPE_UNSIGNED (t1) && l1 > l2)
	return VALUE_TYPE (v1);
      else if (TYPE_UNSIGNED (t2) && l2 > l1)
	return VALUE_TYPE (v2);
      else			/* Both are signed.  Result is the longer type */
	return l1 > l2 ? VALUE_TYPE (v1) : VALUE_TYPE (v2);
      break;
    case language_m2:
      /* If we are doing type-checking, l1 should equal l2, so this is
         not needed. */
      return l1 > l2 ? VALUE_TYPE (v1) : VALUE_TYPE (v2);
      break;
    }
  internal_error (__FILE__, __LINE__, _("failed internal consistency check"));
  return (struct type *) 0;	/* For lint */
}

#endif /* 0 */
#if 0
/* This page contains functions that are used in type/range checking.
   They all return zero if the type/range check fails.

   It is hoped that these will make extending GDB to parse different
   languages a little easier.  These are primarily used in eval.c when
   evaluating expressions and making sure that their types are correct.
   Instead of having a mess of conjucted/disjuncted expressions in an "if",
   the ideas of type can be wrapped up in the following functions.

   Note that some of them are not currently dependent upon which language
   is currently being parsed.  For example, floats are the same in
   C and Modula-2 (ie. the only floating point type has TYPE_CODE of
   TYPE_CODE_FLT), while booleans are different. */

/* Returns non-zero if its argument is a simple type.  This is the same for
   both Modula-2 and for C.  In the C case, TYPE_CODE_CHAR will never occur,
   and thus will never cause the failure of the test. */
int
simple_type (struct type *type)
{
  CHECK_TYPEDEF (type);
  switch (TYPE_CODE (type))
    {
    case TYPE_CODE_INT:
    case TYPE_CODE_CHAR:
    case TYPE_CODE_ENUM:
    case TYPE_CODE_FLT:
    case TYPE_CODE_RANGE:
    case TYPE_CODE_BOOL:
      return 1;

    default:
      return 0;
    }
}

/* Returns non-zero if its argument is of an ordered type.
   An ordered type is one in which the elements can be tested for the
   properties of "greater than", "less than", etc, or for which the
   operations "increment" or "decrement" make sense. */
int
ordered_type (struct type *type)
{
  CHECK_TYPEDEF (type);
  switch (TYPE_CODE (type))
    {
    case TYPE_CODE_INT:
    case TYPE_CODE_CHAR:
    case TYPE_CODE_ENUM:
    case TYPE_CODE_FLT:
    case TYPE_CODE_RANGE:
      return 1;

    default:
      return 0;
    }
}

/* Returns non-zero if the two types are the same */
int
same_type (struct type *arg1, struct type *arg2)
{
  CHECK_TYPEDEF (type);
  if (structured_type (arg1) ? !structured_type (arg2) : structured_type (arg2))
    /* One is structured and one isn't */
    return 0;
  else if (structured_type (arg1) && structured_type (arg2))
    return arg1 == arg2;
  else if (numeric_type (arg1) && numeric_type (arg2))
    return (TYPE_CODE (arg2) == TYPE_CODE (arg1)) &&
      (TYPE_UNSIGNED (arg1) == TYPE_UNSIGNED (arg2))
      ? 1 : 0;
  else
    return arg1 == arg2;
}

/* Returns non-zero if the type is integral */
int
integral_type (struct type *type)
{
  CHECK_TYPEDEF (type);
  switch (current_language->la_language)
    {
    case language_c:
    case language_cplus:
    case language_objc:
      return (TYPE_CODE (type) != TYPE_CODE_INT) &&
	(TYPE_CODE (type) != TYPE_CODE_ENUM) ? 0 : 1;
    case language_m2:
    case language_pascal:
      return TYPE_CODE (type) != TYPE_CODE_INT ? 0 : 1;
    default:
      error (_("Language not supported."));
    }
}

/* Returns non-zero if the value is numeric */
int
numeric_type (struct type *type)
{
  CHECK_TYPEDEF (type);
  switch (TYPE_CODE (type))
    {
    case TYPE_CODE_INT:
    case TYPE_CODE_FLT:
      return 1;

    default:
      return 0;
    }
}

/* Returns non-zero if the value is a character type */
int
character_type (struct type *type)
{
  CHECK_TYPEDEF (type);
  switch (current_language->la_language)
    {
    case language_m2:
    case language_pascal:
      return TYPE_CODE (type) != TYPE_CODE_CHAR ? 0 : 1;

    case language_c:
    case language_cplus:
    case language_objc:
      return (TYPE_CODE (type) == TYPE_CODE_INT) &&
	TYPE_LENGTH (type) == sizeof (char)
      ? 1 : 0;
    default:
      return (0);
    }
}

/* Returns non-zero if the value is a string type */
int
string_type (struct type *type)
{
  CHECK_TYPEDEF (type);
  switch (current_language->la_language)
    {
    case language_m2:
    case language_pascal:
      return TYPE_CODE (type) != TYPE_CODE_STRING ? 0 : 1;

    case language_c:
    case language_cplus:
    case language_objc:
      /* C does not have distinct string type. */
      return (0);
    default:
      return (0);
    }
}

/* Returns non-zero if the value is a boolean type */
int
boolean_type (struct type *type)
{
  CHECK_TYPEDEF (type);
  if (TYPE_CODE (type) == TYPE_CODE_BOOL)
    return 1;
  switch (current_language->la_language)
    {
    case language_c:
    case language_cplus:
    case language_objc:
      /* Might be more cleanly handled by having a
         TYPE_CODE_INT_NOT_BOOL for (the deleted) CHILL and such
         languages, or a TYPE_CODE_INT_OR_BOOL for C.  */
      if (TYPE_CODE (type) == TYPE_CODE_INT)
	return 1;
    default:
      break;
    }
  return 0;
}

/* Returns non-zero if the value is a floating-point type */
int
float_type (struct type *type)
{
  CHECK_TYPEDEF (type);
  return TYPE_CODE (type) == TYPE_CODE_FLT;
}

/* Returns non-zero if the value is a pointer type */
int
pointer_type (struct type *type)
{
  return TYPE_CODE (type) == TYPE_CODE_PTR ||
    TYPE_CODE (type) == TYPE_CODE_REF;
}

/* Returns non-zero if the value is a structured type */
int
structured_type (struct type *type)
{
  CHECK_TYPEDEF (type);
  switch (current_language->la_language)
    {
    case language_c:
    case language_cplus:
    case language_objc:
      return (TYPE_CODE (type) == TYPE_CODE_STRUCT) ||
	(TYPE_CODE (type) == TYPE_CODE_UNION) ||
	(TYPE_CODE (type) == TYPE_CODE_ARRAY);
   case language_pascal:
      return (TYPE_CODE(type) == TYPE_CODE_STRUCT) ||
	 (TYPE_CODE(type) == TYPE_CODE_UNION) ||
	 (TYPE_CODE(type) == TYPE_CODE_SET) ||
	    (TYPE_CODE(type) == TYPE_CODE_ARRAY);
    case language_m2:
      return (TYPE_CODE (type) == TYPE_CODE_STRUCT) ||
	(TYPE_CODE (type) == TYPE_CODE_SET) ||
	(TYPE_CODE (type) == TYPE_CODE_ARRAY);
    default:
      return (0);
    }
}
#endif

struct type *
lang_bool_type (void)
{
  struct symbol *sym;
  struct type *type;
  switch (current_language->la_language)
    {
    case language_fortran:
      sym = lookup_symbol ("logical", NULL, VAR_DOMAIN, NULL, NULL);
      if (sym)
	{
	  type = SYMBOL_TYPE (sym);
	  if (type && TYPE_CODE (type) == TYPE_CODE_BOOL)
	    return type;
	}
      return builtin_type_f_logical_s2;
    case language_cplus:
    case language_pascal:
      if (current_language->la_language==language_cplus)
        {sym = lookup_symbol ("bool", NULL, VAR_DOMAIN, NULL, NULL);}
      else
        {sym = lookup_symbol ("boolean", NULL, VAR_DOMAIN, NULL, NULL);}
      if (sym)
	{
	  type = SYMBOL_TYPE (sym);
	  if (type && TYPE_CODE (type) == TYPE_CODE_BOOL)
	    return type;
	}
      return builtin_type_bool;
    case language_java:
      sym = lookup_symbol ("boolean", NULL, VAR_DOMAIN, NULL, NULL);
      if (sym)
	{
	  type = SYMBOL_TYPE (sym);
	  if (type && TYPE_CODE (type) == TYPE_CODE_BOOL)
	    return type;
	}
      return java_boolean_type;
    default:
      return builtin_type_int;
    }
}

/* This page contains functions that return info about
   (struct value) values used in GDB. */

/* Returns non-zero if the value VAL represents a true value. */
int
value_true (struct value *val)
{
  /* It is possible that we should have some sort of error if a non-boolean
     value is used in this context.  Possibly dependent on some kind of
     "boolean-checking" option like range checking.  But it should probably
     not depend on the language except insofar as is necessary to identify
     a "boolean" value (i.e. in C using a float, pointer, etc., as a boolean
     should be an error, probably).  */
  return !value_logical_not (val);
}

/* This page contains functions for the printing out of
   error messages that occur during type- and range-
   checking. */

/* These are called when a language fails a type- or range-check.  The
   first argument should be a printf()-style format string, and the
   rest of the arguments should be its arguments.  If
   [type|range]_check is [type|range]_check_on, an error is printed;
   if [type|range]_check_warn, a warning; otherwise just the
   message. */

void
type_error (const char *string,...)
{
  va_list args;
  va_start (args, string);

  switch (type_check)
    {
    case type_check_warn:
      vwarning (string, args);
      break;
    case type_check_on:
      verror (string, args);
      break;
    case type_check_off:
      /* FIXME: cagney/2002-01-30: Should this function print anything
         when type error is off?  */
      vfprintf_filtered (gdb_stderr, string, args);
      fprintf_filtered (gdb_stderr, "\n");
      break;
    default:
      internal_error (__FILE__, __LINE__, _("bad switch"));
    }
  va_end (args);
}

void
range_error (const char *string,...)
{
  va_list args;
  va_start (args, string);

  switch (range_check)
    {
    case range_check_warn:
      vwarning (string, args);
      break;
    case range_check_on:
      verror (string, args);
      break;
    case range_check_off:
      /* FIXME: cagney/2002-01-30: Should this function print anything
         when range error is off?  */
      vfprintf_filtered (gdb_stderr, string, args);
      fprintf_filtered (gdb_stderr, "\n");
      break;
    default:
      internal_error (__FILE__, __LINE__, _("bad switch"));
    }
  va_end (args);
}


/* This page contains miscellaneous functions */

/* Return the language enum for a given language string. */

enum language
language_enum (char *str)
{
  int i;

  for (i = 0; i < languages_size; i++)
    if (strcmp (languages[i]->la_name, str) == 0)
      return languages[i]->la_language;

  return language_unknown;
}

/* Return the language struct for a given language enum. */

const struct language_defn *
language_def (enum language lang)
{
  int i;

  for (i = 0; i < languages_size; i++)
    {
      if (languages[i]->la_language == lang)
	{
	  return languages[i];
	}
    }
  return NULL;
}

/* Return the language as a string */
char *
language_str (enum language lang)
{
  int i;

  for (i = 0; i < languages_size; i++)
    {
      if (languages[i]->la_language == lang)
	{
	  return languages[i]->la_name;
	}
    }
  return "Unknown";
}

static void
set_check (char *ignore, int from_tty)
{
  printf_unfiltered (
     "\"set check\" must be followed by the name of a check subcommand.\n");
  help_list (setchecklist, "set check ", -1, gdb_stdout);
}

static void
show_check (char *ignore, int from_tty)
{
  cmd_show_list (showchecklist, from_tty, "");
}

/* Add a language to the set of known languages.  */

void
add_language (const struct language_defn *lang)
{
  if (lang->la_magic != LANG_MAGIC)
    {
      fprintf_unfiltered (gdb_stderr, "Magic number of %s language struct wrong\n",
			  lang->la_name);
      internal_error (__FILE__, __LINE__, _("failed internal consistency check"));
    }

  if (!languages)
    {
      languages_allocsize = DEFAULT_ALLOCSIZE;
      languages = (const struct language_defn **) xmalloc
	(languages_allocsize * sizeof (*languages));
    }
  if (languages_size >= languages_allocsize)
    {
      languages_allocsize *= 2;
      languages = (const struct language_defn **) xrealloc ((char *) languages,
				 languages_allocsize * sizeof (*languages));
    }
  languages[languages_size++] = lang;
}

/* Iterate through all registered languages looking for and calling
   any non-NULL struct language_defn.skip_trampoline() functions.
   Return the result from the first that returns non-zero, or 0 if all
   `fail'.  */
CORE_ADDR 
skip_language_trampoline (struct frame_info *frame, CORE_ADDR pc)
{
  int i;

  for (i = 0; i < languages_size; i++)
    {
      if (languages[i]->skip_trampoline)
	{
	  CORE_ADDR real_pc = (languages[i]->skip_trampoline) (frame, pc);
	  if (real_pc)
	    return real_pc;
	}
    }

  return 0;
}

/* Return demangled language symbol, or NULL.  
   FIXME: Options are only useful for certain languages and ignored
   by others, so it would be better to remove them here and have a
   more flexible demangler for the languages that need it.  
   FIXME: Sometimes the demangler is invoked when we don't know the
   language, so we can't use this everywhere.  */
char *
language_demangle (const struct language_defn *current_language, 
				const char *mangled, int options)
{
  if (current_language != NULL && current_language->la_demangle)
    return current_language->la_demangle (mangled, options);
  return NULL;
}

/* Return class name from physname or NULL.  */
char *
language_class_name_from_physname (const struct language_defn *current_language,
				   const char *physname)
{
  if (current_language != NULL && current_language->la_class_name_from_physname)
    return current_language->la_class_name_from_physname (physname);
  return NULL;
}

/* Return the default string containing the list of characters
   delimiting words.  This is a reasonable default value that
   most languages should be able to use.  */

char *
default_word_break_characters (void)
{
  return " \t\n!@#$%^&*()+=|~`}{[]\"';:?/>.<,-";
}

/* Print the index of array elements using the C99 syntax.  */

void
default_print_array_index (struct value *index_value, struct ui_file *stream,
                           int format, enum val_prettyprint pretty)
{
  fprintf_filtered (stream, "[");
  LA_VALUE_PRINT (index_value, stream, format, pretty);
  fprintf_filtered (stream, "] = ");
}

/* Define the language that is no language.  */

static int
unk_lang_parser (void)
{
  return 1;
}

static void
unk_lang_error (char *msg)
{
  error (_("Attempted to parse an expression with unknown language"));
}

static void
unk_lang_emit_char (int c, struct ui_file *stream, int quoter)
{
  error (_("internal error - unimplemented function unk_lang_emit_char called."));
}

static void
unk_lang_printchar (int c, struct ui_file *stream)
{
  error (_("internal error - unimplemented function unk_lang_printchar called."));
}

static void
unk_lang_printstr (struct ui_file *stream, const gdb_byte *string,
		   unsigned int length, int width, int force_ellipses)
{
  error (_("internal error - unimplemented function unk_lang_printstr called."));
}

static struct type *
unk_lang_create_fundamental_type (struct objfile *objfile, int typeid)
{
  error (_("internal error - unimplemented function unk_lang_create_fundamental_type called."));
}

static void
unk_lang_print_type (struct type *type, char *varstring, struct ui_file *stream,
		     int show, int level)
{
  error (_("internal error - unimplemented function unk_lang_print_type called."));
}

static int
unk_lang_val_print (struct type *type, const gdb_byte *valaddr,
		    int embedded_offset, CORE_ADDR address,
		    struct ui_file *stream, int format,
		    int deref_ref, int recurse, enum val_prettyprint pretty)
{
  error (_("internal error - unimplemented function unk_lang_val_print called."));
}

static int
unk_lang_value_print (struct value *val, struct ui_file *stream, int format,
		      enum val_prettyprint pretty)
{
  error (_("internal error - unimplemented function unk_lang_value_print called."));
}

static CORE_ADDR unk_lang_trampoline (struct frame_info *frame, CORE_ADDR pc)
{
  return 0;
}

/* Unknown languages just use the cplus demangler.  */
static char *unk_lang_demangle (const char *mangled, int options)
{
  return cplus_demangle (mangled, options);
}

static char *unk_lang_class_name (const char *mangled)
{
  return NULL;
}

static const struct op_print unk_op_print_tab[] =
{
  {NULL, OP_NULL, PREC_NULL, 0}
};

static void
unknown_language_arch_info (struct gdbarch *gdbarch,
			    struct language_arch_info *lai)
{
  lai->string_char_type = builtin_type (gdbarch)->builtin_char;
  lai->primitive_type_vector = GDBARCH_OBSTACK_CALLOC (gdbarch, 1,
						       struct type *);
}

const struct language_defn unknown_language_defn =
{
  "unknown",
  language_unknown,
  NULL,
  range_check_off,
  type_check_off,
  array_row_major,
  case_sensitive_on,
  &exp_descriptor_standard,
  unk_lang_parser,
  unk_lang_error,
  null_post_parser,
  unk_lang_printchar,		/* Print character constant */
  unk_lang_printstr,
  unk_lang_emit_char,
  unk_lang_create_fundamental_type,
  unk_lang_print_type,		/* Print a type using appropriate syntax */
  unk_lang_val_print,		/* Print a value using appropriate syntax */
  unk_lang_value_print,		/* Print a top-level value */
  unk_lang_trampoline,		/* Language specific skip_trampoline */
  value_of_this,		/* value_of_this */
  basic_lookup_symbol_nonlocal, /* lookup_symbol_nonlocal */
  basic_lookup_transparent_type,/* lookup_transparent_type */
  unk_lang_demangle,		/* Language specific symbol demangler */
  unk_lang_class_name,		/* Language specific class_name_from_physname */
  unk_op_print_tab,		/* expression operators for printing */
  1,				/* c-style arrays */
  0,				/* String lower bound */
  NULL,
  default_word_break_characters,
  unknown_language_arch_info,	/* la_language_arch_info.  */
  default_print_array_index,
  LANG_MAGIC
};

/* These two structs define fake entries for the "local" and "auto" options. */
const struct language_defn auto_language_defn =
{
  "auto",
  language_auto,
  NULL,
  range_check_off,
  type_check_off,
  array_row_major,
  case_sensitive_on,
  &exp_descriptor_standard,
  unk_lang_parser,
  unk_lang_error,
  null_post_parser,
  unk_lang_printchar,		/* Print character constant */
  unk_lang_printstr,
  unk_lang_emit_char,
  unk_lang_create_fundamental_type,
  unk_lang_print_type,		/* Print a type using appropriate syntax */
  unk_lang_val_print,		/* Print a value using appropriate syntax */
  unk_lang_value_print,		/* Print a top-level value */
  unk_lang_trampoline,		/* Language specific skip_trampoline */
  value_of_this,		/* value_of_this */
  basic_lookup_symbol_nonlocal,	/* lookup_symbol_nonlocal */
  basic_lookup_transparent_type,/* lookup_transparent_type */
  unk_lang_demangle,		/* Language specific symbol demangler */
  unk_lang_class_name,		/* Language specific class_name_from_physname */
  unk_op_print_tab,		/* expression operators for printing */
  1,				/* c-style arrays */
  0,				/* String lower bound */
  NULL,
  default_word_break_characters,
  unknown_language_arch_info,	/* la_language_arch_info.  */
  default_print_array_index,
  LANG_MAGIC
};

const struct language_defn local_language_defn =
{
  "local",
  language_auto,
  NULL,
  range_check_off,
  type_check_off,
  case_sensitive_on,
  array_row_major,
  &exp_descriptor_standard,
  unk_lang_parser,
  unk_lang_error,
  null_post_parser,
  unk_lang_printchar,		/* Print character constant */
  unk_lang_printstr,
  unk_lang_emit_char,
  unk_lang_create_fundamental_type,
  unk_lang_print_type,		/* Print a type using appropriate syntax */
  unk_lang_val_print,		/* Print a value using appropriate syntax */
  unk_lang_value_print,		/* Print a top-level value */
  unk_lang_trampoline,		/* Language specific skip_trampoline */
  value_of_this,		/* value_of_this */
  basic_lookup_symbol_nonlocal,	/* lookup_symbol_nonlocal */
  basic_lookup_transparent_type,/* lookup_transparent_type */
  unk_lang_demangle,		/* Language specific symbol demangler */
  unk_lang_class_name,		/* Language specific class_name_from_physname */
  unk_op_print_tab,		/* expression operators for printing */
  1,				/* c-style arrays */
  0,				/* String lower bound */
  NULL,
  default_word_break_characters,
  unknown_language_arch_info,	/* la_language_arch_info.  */
  default_print_array_index,
  LANG_MAGIC
};

/* Per-architecture language information.  */

static struct gdbarch_data *language_gdbarch_data;

struct language_gdbarch
{
  /* A vector of per-language per-architecture info.  Indexed by "enum
     language".  */
  struct language_arch_info arch_info[nr_languages];
};

static void *
language_gdbarch_post_init (struct gdbarch *gdbarch)
{
  struct language_gdbarch *l;
  int i;

  l = GDBARCH_OBSTACK_ZALLOC (gdbarch, struct language_gdbarch);
  for (i = 0; i < languages_size; i++)
    {
      if (languages[i] != NULL
	  && languages[i]->la_language_arch_info != NULL)
	languages[i]->la_language_arch_info
	  (gdbarch, l->arch_info + languages[i]->la_language);
    }
  return l;
}

struct type *
language_string_char_type (const struct language_defn *la,
			   struct gdbarch *gdbarch)
{
  struct language_gdbarch *ld = gdbarch_data (gdbarch,
					      language_gdbarch_data);
  if (ld->arch_info[la->la_language].string_char_type != NULL)
    return ld->arch_info[la->la_language].string_char_type;
  else
    return (*la->string_char_type);
}

struct type *
language_lookup_primitive_type_by_name (const struct language_defn *la,
					struct gdbarch *gdbarch,
					const char *name)
{
  struct language_gdbarch *ld = gdbarch_data (gdbarch,
					      language_gdbarch_data);
  if (ld->arch_info[la->la_language].primitive_type_vector != NULL)
    {
      struct type *const *p;
      for (p = ld->arch_info[la->la_language].primitive_type_vector;
	   (*p) != NULL;
	   p++)
	{
	  if (strcmp (TYPE_NAME (*p), name) == 0)
	    return (*p);
	}
    }
  else
    {
      struct type **const *p;
      for (p = current_language->la_builtin_type_vector; *p != NULL; p++)
	{
	  if (strcmp (TYPE_NAME (**p), name) == 0)
	    return (**p);
	}
    }
  return (NULL);
}

/* Initialize the language routines */

void
_initialize_language (void)
{
  struct cmd_list_element *set, *show;

  language_gdbarch_data
    = gdbarch_data_register_post_init (language_gdbarch_post_init);

  /* GDB commands for language specific stuff */

  /* FIXME: cagney/2005-02-20: This should be implemented using an
     enum.  */
  add_setshow_string_noescape_cmd ("language", class_support, &language, _("\
Set the current source language."), _("\
Show the current source language."), NULL,
				   set_language_command,
				   show_language_command,
				   &setlist, &showlist);

  add_prefix_cmd ("check", no_class, set_check,
		  _("Set the status of the type/range checker."),
		  &setchecklist, "set check ", 0, &setlist);
  add_alias_cmd ("c", "check", no_class, 1, &setlist);
  add_alias_cmd ("ch", "check", no_class, 1, &setlist);

  add_prefix_cmd ("check", no_class, show_check,
		  _("Show the status of the type/range checker."),
		  &showchecklist, "show check ", 0, &showlist);
  add_alias_cmd ("c", "check", no_class, 1, &showlist);
  add_alias_cmd ("ch", "check", no_class, 1, &showlist);

  /* FIXME: cagney/2005-02-20: This should be implemented using an
     enum.  */
  add_setshow_string_noescape_cmd ("type", class_support, &type, _("\
Set type checking.  (on/warn/off/auto)"), _("\
Show type checking.  (on/warn/off/auto)"), NULL,
				   set_type_command,
				   show_type_command,
				   &setchecklist, &showchecklist);

  /* FIXME: cagney/2005-02-20: This should be implemented using an
     enum.  */
  add_setshow_string_noescape_cmd ("range", class_support, &range, _("\
Set range checking.  (on/warn/off/auto)"), _("\
Show range checking.  (on/warn/off/auto)"), NULL,
				   set_range_command,
				   show_range_command,
				   &setchecklist, &showchecklist);

  /* FIXME: cagney/2005-02-20: This should be implemented using an
     enum.  */
  add_setshow_string_noescape_cmd ("case-sensitive", class_support,
				   &case_sensitive, _("\
Set case sensitivity in name search.  (on/off/auto)"), _("\
Show case sensitivity in name search.  (on/off/auto)"), _("\
For Fortran the default is off; for other languages the default is on."),
				   set_case_command,
				   show_case_command,
				   &setlist, &showlist);

  add_language (&unknown_language_defn);
  add_language (&local_language_defn);
  add_language (&auto_language_defn);

  language = savestring ("auto", strlen ("auto"));
  type = savestring ("auto", strlen ("auto"));
  range = savestring ("auto", strlen ("auto"));
  case_sensitive = savestring ("auto",strlen ("auto"));

  /* Have the above take effect */
  set_language (language_auto);
}
