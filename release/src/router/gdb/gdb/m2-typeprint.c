/* Support for printing Modula 2 types for GDB, the GNU debugger.
   Copyright (C) 1986, 1988, 1989, 1991, 1992, 1995, 2000, 2001, 2002, 2003,
                 2004, 2005, 2006, 2007 Free Software Foundation, Inc.

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
#include "gdb_obstack.h"
#include "bfd.h"		/* Binary File Description */
#include "symtab.h"
#include "gdbtypes.h"
#include "expression.h"
#include "value.h"
#include "gdbcore.h"
#include "m2-lang.h"
#include "target.h"
#include "language.h"
#include "demangle.h"
#include "c-lang.h"
#include "typeprint.h"
#include "cp-abi.h"

#include "gdb_string.h"
#include <errno.h>

static void m2_print_bounds (struct type *type,
			     struct ui_file *stream, int show, int level,
			     int print_high);

static void m2_typedef (struct type *, struct ui_file *, int, int);
static void m2_array (struct type *, struct ui_file *, int, int);
static void m2_pointer (struct type *, struct ui_file *, int, int);
static void m2_ref (struct type *, struct ui_file *, int, int);
static void m2_procedure (struct type *, struct ui_file *, int, int);
static void m2_union (struct type *, struct ui_file *);
static void m2_enum (struct type *, struct ui_file *, int, int);
static void m2_range (struct type *, struct ui_file *, int, int);
static void m2_type_name (struct type *type, struct ui_file *stream);
static void m2_short_set (struct type *type, struct ui_file *stream,
			  int show, int level);
static int m2_long_set (struct type *type, struct ui_file *stream,
			int show, int level);
static void m2_record_fields (struct type *type, struct ui_file *stream,
			      int show, int level);
static void m2_unknown (const char *s, struct type *type,
			struct ui_file *stream, int show, int level);

int m2_is_long_set (struct type *type);
int m2_is_long_set_of_type (struct type *type, struct type **of_type);


void
m2_print_type (struct type *type, char *varstring, struct ui_file *stream,
	       int show, int level)
{
  enum type_code code;
  int demangled_args;

  CHECK_TYPEDEF (type);

  QUIT;

  wrap_here ("    ");
  if (type == NULL)
    {
      fputs_filtered (_("<type unknown>"), stream);
      return;
    }

  code = TYPE_CODE (type);
  switch (TYPE_CODE (type))
    {
    case TYPE_CODE_SET:
      m2_short_set(type, stream, show, level);
      break;

    case TYPE_CODE_STRUCT:
      if (m2_long_set (type, stream, show, level))
	break;
      m2_record_fields (type, stream, show, level);
      break;

    case TYPE_CODE_TYPEDEF:
      m2_typedef (type, stream, show, level);
      break;

    case TYPE_CODE_ARRAY:
      m2_array (type, stream, show, level);
      break;

    case TYPE_CODE_PTR:
      m2_pointer (type, stream, show, level);
      break;

    case TYPE_CODE_REF:
      m2_ref (type, stream, show, level);
      break;

    case TYPE_CODE_METHOD:
      m2_unknown (_("method"), type, stream, show, level);
      break;

    case TYPE_CODE_FUNC:
      m2_procedure (type, stream, show, level);
      break;

    case TYPE_CODE_UNION:
      m2_union (type, stream);
      break;

    case TYPE_CODE_ENUM:
      m2_enum (type, stream, show, level);
      break;

    case TYPE_CODE_VOID:
      break;

    case TYPE_CODE_UNDEF:
      /* i18n: Do not translate the "struct" part! */
      m2_unknown (_("undef"), type, stream, show, level);
      break;

    case TYPE_CODE_ERROR:
      m2_unknown (_("error"), type, stream, show, level);
      break;

    case TYPE_CODE_RANGE:
      m2_range (type, stream, show, level);
      break;

    case TYPE_CODE_TEMPLATE:
      break;

    default:
      m2_type_name (type, stream);
      break;
    }
}

/*
 *  m2_type_name - if a, type, has a name then print it.
 */

void
m2_type_name (struct type *type, struct ui_file *stream)
{
  if (TYPE_NAME (type) != NULL)
    fputs_filtered (TYPE_NAME (type), stream);
}

/*
 *  m2_range - displays a Modula-2 subrange type.
 */

void
m2_range (struct type *type, struct ui_file *stream, int show,
	  int level)
{
  if (TYPE_HIGH_BOUND (type) == TYPE_LOW_BOUND (type))
    m2_print_type (TYPE_DOMAIN_TYPE (type), "", stream, show, level);
  else
    {
      struct type *target = TYPE_TARGET_TYPE (type);

      fprintf_filtered (stream, "[");
      print_type_scalar (target, TYPE_LOW_BOUND (type), stream);
      fprintf_filtered (stream, "..");
      print_type_scalar (target, TYPE_HIGH_BOUND (type), stream);
      fprintf_filtered (stream, "]");
    }
}

static void
m2_typedef (struct type *type, struct ui_file *stream, int show,
	    int level)
{
  if (TYPE_NAME (type) != NULL)
    {
      fputs_filtered (TYPE_NAME (type), stream);
      fputs_filtered (" = ", stream);
    }
  m2_print_type (TYPE_TARGET_TYPE (type), "", stream, show, level);
}

/*
 *  m2_array - prints out a Modula-2 ARRAY ... OF type
 */

static void m2_array (struct type *type, struct ui_file *stream,
		      int show, int level)
{
  fprintf_filtered (stream, "ARRAY [");
  if (TYPE_LENGTH (TYPE_TARGET_TYPE (type)) > 0
      && TYPE_ARRAY_UPPER_BOUND_TYPE (type) != BOUND_CANNOT_BE_DETERMINED)
    {
      if (TYPE_INDEX_TYPE (type) != 0)
	{
	  m2_print_bounds (TYPE_INDEX_TYPE (type), stream, show, -1, 0);
	  fprintf_filtered (stream, "..");
	  m2_print_bounds (TYPE_INDEX_TYPE (type), stream, show, -1, 1);
	}
      else
	fprintf_filtered (stream, "%d",
			  (TYPE_LENGTH (type)
			   / TYPE_LENGTH (TYPE_TARGET_TYPE (type))));
    }
  fprintf_filtered (stream, "] OF ");
  m2_print_type (TYPE_TARGET_TYPE (type), "", stream, show, level);
}

static void
m2_pointer (struct type *type, struct ui_file *stream, int show,
	    int level)
{
  if (TYPE_CONST (type))
    fprintf_filtered (stream, "[...] : ");
  else
    fprintf_filtered (stream, "POINTER TO ");

  m2_print_type (TYPE_TARGET_TYPE (type), "", stream, show, level);
}

static void
m2_ref (struct type *type, struct ui_file *stream, int show,
	int level)
{
  fprintf_filtered (stream, "VAR");
  m2_print_type (TYPE_TARGET_TYPE (type), "", stream, show, level);
}

static void
m2_unknown (const char *s, struct type *type, struct ui_file *stream,
	    int show, int level)
{
  fprintf_filtered (stream, "%s %s", s, _("is unknown"));
}

static void m2_union (struct type *type, struct ui_file *stream)
{
  fprintf_filtered (stream, "union");
}

static void
m2_procedure (struct type *type, struct ui_file *stream,
	      int show, int level)
{
  fprintf_filtered (stream, "PROCEDURE ");
  m2_type_name (type, stream);
  if (TYPE_CODE (TYPE_TARGET_TYPE (type)) != TYPE_CODE_VOID)
    {
      int i, len = TYPE_NFIELDS (type);

      fprintf_filtered (stream, " (");
      for (i = 0; i < len; i++)
	{
	  if (i > 0)
	    {
	      fputs_filtered (", ", stream);
	      wrap_here ("    ");
	    }
	  m2_print_type (TYPE_FIELD_TYPE (type, i), "", stream, -1, 0);
	}
      if (TYPE_TARGET_TYPE (type) != NULL)
	{
	  fprintf_filtered (stream, " : ");
	  m2_print_type (TYPE_TARGET_TYPE (type), "", stream, 0, 0);
	}
    }
}

static void
m2_print_bounds (struct type *type,
		 struct ui_file *stream, int show, int level,
		 int print_high)
{
  struct type *target = TYPE_TARGET_TYPE (type);

  if (target == NULL)
    target = builtin_type_int;

  if (TYPE_NFIELDS(type) == 0)
    return;

  if (print_high)
    print_type_scalar (target, TYPE_HIGH_BOUND (type), stream);
  else
    print_type_scalar (target, TYPE_LOW_BOUND (type), stream);
}

static void
m2_short_set (struct type *type, struct ui_file *stream, int show, int level)
{
  fprintf_filtered(stream, "SET [");
  m2_print_bounds (TYPE_INDEX_TYPE (type), stream,
		   show - 1, level, 0);

  fprintf_filtered(stream, "..");
  m2_print_bounds (TYPE_INDEX_TYPE (type), stream,
		   show - 1, level, 1);
  fprintf_filtered(stream, "]");
}

int
m2_is_long_set (struct type *type)
{
  LONGEST previous_high = 0;  /* unnecessary initialization
				 keeps gcc -Wall happy */
  int len, i;
  struct type *range;

  if (TYPE_CODE (type) == TYPE_CODE_STRUCT)
    {

      /*
       *  check if all fields of the RECORD are consecutive sets
       */
      len = TYPE_NFIELDS (type);
      for (i = TYPE_N_BASECLASSES (type); i < len; i++)
	{
	  if (TYPE_FIELD_TYPE (type, i) == NULL)
	    return 0;
	  if (TYPE_CODE (TYPE_FIELD_TYPE (type, i)) != TYPE_CODE_SET)
	    return 0;
	  if (TYPE_FIELD_NAME (type, i) != NULL
	      && (strcmp (TYPE_FIELD_NAME (type, i), "") != 0))
	    return 0;
	  range = TYPE_INDEX_TYPE (TYPE_FIELD_TYPE (type, i));
	  if ((i > TYPE_N_BASECLASSES (type))
	      && previous_high + 1 != TYPE_LOW_BOUND (range))
	    return 0;
	  previous_high = TYPE_HIGH_BOUND (range);
	}
      return len>0;
    }
  return 0;
}

/*
 *  m2_get_discrete_bounds - a wrapper for get_discrete_bounds which
 *                           understands that CHARs might be signed.
 *                           This should be integrated into gdbtypes.c
 *                           inside get_discrete_bounds.
 */

int
m2_get_discrete_bounds (struct type *type, LONGEST *lowp, LONGEST *highp)
{
  CHECK_TYPEDEF (type);
  switch (TYPE_CODE (type))
    {
    case TYPE_CODE_CHAR:
      if (TYPE_LENGTH (type) < sizeof (LONGEST))
	{
	  if (!TYPE_UNSIGNED (type))
	    {
	      *lowp = -(1 << (TYPE_LENGTH (type) * TARGET_CHAR_BIT - 1));
	      *highp = -*lowp - 1;
	      return 0;
	    }
	}
      /* fall through */
    default:
      return get_discrete_bounds (type, lowp, highp);
    }
}

/*
 *  m2_is_long_set_of_type - returns TRUE if the long set was declared as
 *                           SET OF <oftype> of_type is assigned to the
 *                           subtype.
 */

int
m2_is_long_set_of_type (struct type *type, struct type **of_type)
{
  int len, i;
  struct type *range;
  struct type *target;
  LONGEST l1, l2;
  LONGEST h1, h2;

  if (TYPE_CODE (type) == TYPE_CODE_STRUCT)
    {
      len = TYPE_NFIELDS (type);
      i = TYPE_N_BASECLASSES (type);
      if (len == 0)
	return 0;
      range = TYPE_INDEX_TYPE (TYPE_FIELD_TYPE (type, i));
      target = TYPE_TARGET_TYPE (range);
      if (target == NULL)
	target = builtin_type_int;

      l1 = TYPE_LOW_BOUND (TYPE_INDEX_TYPE (TYPE_FIELD_TYPE (type, i)));
      h1 = TYPE_HIGH_BOUND (TYPE_INDEX_TYPE (TYPE_FIELD_TYPE (type, len-1)));
      *of_type = target;
      if (m2_get_discrete_bounds (target, &l2, &h2) >= 0)
	return (l1 == l2 && h1 == h2);
      error (_("long_set failed to find discrete bounds for its subtype"));
      return 0;
    }
  error (_("expecting long_set"));
  return 0;
}

static int
m2_long_set (struct type *type, struct ui_file *stream, int show, int level)
{
  struct type *index_type;
  struct type *range_type;
  struct type *of_type;
  int i;
  int len = TYPE_NFIELDS (type);
  LONGEST low;
  LONGEST high;

  if (m2_is_long_set (type))
    {
      if (TYPE_TAG_NAME (type) != NULL)
	{
	  fputs_filtered (TYPE_TAG_NAME (type), stream);
	  if (show == 0)
	    return 1;
	}
      else if (TYPE_NAME (type) != NULL)
	{
	  fputs_filtered (TYPE_NAME (type), stream);
	  if (show == 0)
	    return 1;
	}

      if (TYPE_TAG_NAME (type) != NULL || TYPE_NAME (type) != NULL)
	fputs_filtered (" = ", stream);

      if (get_long_set_bounds (type, &low, &high))
	{
	  fprintf_filtered(stream, "SET OF ");
	  i = TYPE_N_BASECLASSES (type);
	  if (m2_is_long_set_of_type (type, &of_type))
	    m2_print_type (of_type, "", stream, show - 1, level);
	  else
	    {
	      fprintf_filtered(stream, "[");
	      m2_print_bounds (TYPE_INDEX_TYPE (TYPE_FIELD_TYPE (type, i)),
			       stream, show - 1, level, 0);

	      fprintf_filtered(stream, "..");

	      m2_print_bounds (TYPE_INDEX_TYPE (TYPE_FIELD_TYPE (type, len-1)),
			       stream, show - 1, level, 1);
	      fprintf_filtered(stream, "]");
	    }
	}
      else
	/* i18n: Do not translate the "SET OF" part! */
	fprintf_filtered(stream, _("SET OF <unknown>"));

      return 1;
    }
  return 0;
}

void
m2_record_fields (struct type *type, struct ui_file *stream, int show,
		  int level)
{
  /* Print the tag if it exists. 
   */
  if (TYPE_TAG_NAME (type) != NULL)
    {
      if (strncmp (TYPE_TAG_NAME (type), "$$", 2) != 0)
	{
	  fputs_filtered (TYPE_TAG_NAME (type), stream);
	  if (show > 0)
	    fprintf_filtered (stream, " = ");
	}
    }
  wrap_here ("    ");
  if (show < 0)
    {
      if (TYPE_CODE (type) == DECLARED_TYPE_STRUCT)
	fprintf_filtered (stream, "RECORD ... END ");
      else if (TYPE_DECLARED_TYPE (type) == DECLARED_TYPE_UNION)
	fprintf_filtered (stream, "CASE ... END ");
    }
  else if (show > 0)
    {
      int i;
      int len = TYPE_NFIELDS (type);

      if (TYPE_CODE (type) == TYPE_CODE_STRUCT)
	fprintf_filtered (stream, "RECORD\n");
      else if (TYPE_CODE (type) == TYPE_CODE_UNION)
	/* i18n: Do not translate "CASE" and "OF" */
	fprintf_filtered (stream, _("CASE <variant> OF\n"));

      for (i = TYPE_N_BASECLASSES (type); i < len; i++)
	{
	  QUIT;

	  print_spaces_filtered (level + 4, stream);
	  fputs_filtered (TYPE_FIELD_NAME (type, i), stream);
	  fputs_filtered (" : ", stream);
	  m2_print_type (TYPE_FIELD_TYPE (type, i),
			 "",
			 stream, 0, level + 4);
	  if (TYPE_FIELD_PACKED (type, i))
	    {
	      /* It is a bitfield.  This code does not attempt
		 to look at the bitpos and reconstruct filler,
		 unnamed fields.  This would lead to misleading
		 results if the compiler does not put out fields
		 for such things (I don't know what it does).  */
	      fprintf_filtered (stream, " : %d",
				TYPE_FIELD_BITSIZE (type, i));
	    }
	  fprintf_filtered (stream, ";\n");
	}
      
      fprintfi_filtered (level, stream, "END ");
    }
}

void
m2_enum (struct type *type, struct ui_file *stream, int show, int level)
{
  int lastval, i, len;

  if (show < 0)
    {
      /* If we just printed a tag name, no need to print anything else.  */
      if (TYPE_TAG_NAME (type) == NULL)
	fprintf_filtered (stream, "(...)");
    }
  else if (show > 0 || TYPE_TAG_NAME (type) == NULL)
    {
      fprintf_filtered (stream, "(");
      len = TYPE_NFIELDS (type);
      lastval = 0;
      for (i = 0; i < len; i++)
	{
	  QUIT;
	  if (i > 0)
	    fprintf_filtered (stream, ", ");
	  wrap_here ("    ");
	  fputs_filtered (TYPE_FIELD_NAME (type, i), stream);
	  if (lastval != TYPE_FIELD_BITPOS (type, i))
	    {
	      fprintf_filtered (stream, " = %d", TYPE_FIELD_BITPOS (type, i));
	      lastval = TYPE_FIELD_BITPOS (type, i);
	    }
	  lastval++;
	}
      fprintf_filtered (stream, ")");
    }
}
