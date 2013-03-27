/* Support for printing Modula 2 values for GDB, the GNU debugger.

   Copyright (C) 1986, 1988, 1989, 1991, 1992, 1996, 1998, 2000, 2005, 2006,
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

#include "defs.h"
#include "symtab.h"
#include "gdbtypes.h"
#include "expression.h"
#include "value.h"
#include "valprint.h"
#include "language.h"
#include "typeprint.h"
#include "c-lang.h"
#include "m2-lang.h"
#include "target.h"

int print_unpacked_pointer (struct type *type,
			    CORE_ADDR address, CORE_ADDR addr,
			    int format, struct ui_file *stream);


/* Print function pointer with inferior address ADDRESS onto stdio
   stream STREAM.  */

static void
print_function_pointer_address (CORE_ADDR address, struct ui_file *stream)
{
  CORE_ADDR func_addr = gdbarch_convert_from_func_ptr_addr (current_gdbarch,
							    address,
							    &current_target);

  /* If the function pointer is represented by a description, print the
     address of the description.  */
  if (addressprint && func_addr != address)
    {
      fputs_filtered ("@", stream);
      fputs_filtered (paddress (address), stream);
      fputs_filtered (": ", stream);
    }
  print_address_demangle (func_addr, stream, demangle);
}

/*
 *  get_long_set_bounds - assigns the bounds of the long set to low and high.
 */

int
get_long_set_bounds (struct type *type, LONGEST *low, LONGEST *high)
{
  int len, i;

  if (TYPE_CODE (type) == TYPE_CODE_STRUCT)
    {
      len = TYPE_NFIELDS (type);
      i = TYPE_N_BASECLASSES (type);
      if (len == 0)
	return 0;
      *low = TYPE_LOW_BOUND (TYPE_INDEX_TYPE (TYPE_FIELD_TYPE (type, i)));
      *high = TYPE_HIGH_BOUND (TYPE_INDEX_TYPE (TYPE_FIELD_TYPE (type,
								 len-1)));
      return 1;
    }
  error (_("expecting long_set"));
  return 0;
}

static void
m2_print_long_set (struct type *type, const gdb_byte *valaddr,
		   int embedded_offset, CORE_ADDR address,
		   struct ui_file *stream, int format,
		   enum val_prettyprint pretty)
{
  int empty_set        = 1;
  int element_seen     = 0;
  LONGEST previous_low = 0;
  LONGEST previous_high= 0;
  LONGEST i, low_bound, high_bound;
  LONGEST field_low, field_high;
  struct type *range;
  int len, field;
  struct type *target;
  int bitval;

  CHECK_TYPEDEF (type);

  fprintf_filtered (stream, "{");
  len = TYPE_NFIELDS (type);
  if (get_long_set_bounds (type, &low_bound, &high_bound))
    {
      field = TYPE_N_BASECLASSES (type);
      range = TYPE_INDEX_TYPE (TYPE_FIELD_TYPE (type, field));
    }
  else
    {
      fprintf_filtered (stream, " %s }", _("<unknown bounds of set>"));
      return;
    }

  target = TYPE_TARGET_TYPE (range);
  if (target == NULL)
    target = builtin_type_int;

  if (get_discrete_bounds (range, &field_low, &field_high) >= 0)
    {
      for (i = low_bound; i <= high_bound; i++)
	{
	  bitval = value_bit_index (TYPE_FIELD_TYPE (type, field),
				    (TYPE_FIELD_BITPOS (type, field) / 8) +
				    valaddr + embedded_offset, i);
	  if (bitval < 0)
	    error (_("bit test is out of range"));
	  else if (bitval > 0)
	    {
	      previous_high = i;
	      if (! element_seen)
		{
		  if (! empty_set)
		    fprintf_filtered (stream, ", ");
		  print_type_scalar (target, i, stream);
		  empty_set    = 0;
		  element_seen = 1;
		  previous_low = i;
		}
	    }
	  else
	    {
	      /* bit is not set */
	      if (element_seen)
		{
		  if (previous_low+1 < previous_high)
		    fprintf_filtered (stream, "..");
		  if (previous_low+1 < previous_high)
		    print_type_scalar (target, previous_high, stream);
		  element_seen = 0;
		}
	    }
	  if (i == field_high)
	    {
	      field++;
	      if (field == len)
		break;
	      range = TYPE_INDEX_TYPE (TYPE_FIELD_TYPE (type, field));
	      if (get_discrete_bounds (range, &field_low, &field_high) < 0)
		break;
	      target = TYPE_TARGET_TYPE (range);
	      if (target == NULL)
		target = builtin_type_int;
	    }
	}
      if (element_seen)
	{
	  if (previous_low+1 < previous_high)
	    {
	      fprintf_filtered (stream, "..");
	      print_type_scalar (target, previous_high, stream);
	    }
	  element_seen = 0;
	}
      fprintf_filtered (stream, "}");
    }
}

int
print_unpacked_pointer (struct type *type,
			CORE_ADDR address, CORE_ADDR addr,
			int format, struct ui_file *stream)
{
  struct type *elttype = check_typedef (TYPE_TARGET_TYPE (type));

  if (TYPE_CODE (elttype) == TYPE_CODE_FUNC)
    {
      /* Try to print what function it points to.  */
      print_function_pointer_address (addr, stream);
      /* Return value is irrelevant except for string pointers.  */
      return 0;
    }

  if (addressprint && format != 's')
    fputs_filtered (paddress (address), stream);

  /* For a pointer to char or unsigned char, also print the string
     pointed to, unless pointer is null.  */

  if (TYPE_LENGTH (elttype) == 1
      && TYPE_CODE (elttype) == TYPE_CODE_INT
      && (format == 0 || format == 's')
      && addr != 0)
      return val_print_string (addr, -1, TYPE_LENGTH (elttype), stream);
  
  return 0;
}

static void
print_variable_at_address (struct type *type, const gdb_byte *valaddr,
			   struct ui_file *stream, int format,
			   int deref_ref, int recurse,
			   enum val_prettyprint pretty)
{
  CORE_ADDR addr = unpack_pointer (type, valaddr);
  struct type *elttype = check_typedef (TYPE_TARGET_TYPE (type));

  fprintf_filtered (stream, "[");
  fputs_filtered (paddress (addr), stream);
  fprintf_filtered (stream, "] : ");
  
  if (TYPE_CODE (elttype) != TYPE_CODE_UNDEF)
    {
      struct value *deref_val =
	value_at
	(TYPE_TARGET_TYPE (type),
	 unpack_pointer (lookup_pointer_type (builtin_type_void),
			 valaddr));
      common_val_print (deref_val, stream, format, deref_ref,
			recurse, pretty);
    }
  else
    fputs_filtered ("???", stream);
}

/* Print data of type TYPE located at VALADDR (within GDB), which came from
   the inferior at address ADDRESS, onto stdio stream STREAM according to
   FORMAT (a letter or 0 for natural format).  The data at VALADDR is in
   target byte order.

   If the data are a string pointer, returns the number of string characters
   printed.

   If DEREF_REF is nonzero, then dereference references, otherwise just print
   them like pointers.

   The PRETTY parameter controls prettyprinting.  */

int
m2_val_print (struct type *type, const gdb_byte *valaddr, int embedded_offset,
	      CORE_ADDR address, struct ui_file *stream, int format,
	      int deref_ref, int recurse, enum val_prettyprint pretty)
{
  unsigned int i = 0;	/* Number of characters printed */
  unsigned len;
  struct type *elttype;
  unsigned eltlen;
  int length_pos, length_size, string_pos;
  int char_size;
  LONGEST val;
  CORE_ADDR addr;

  CHECK_TYPEDEF (type);
  switch (TYPE_CODE (type))
    {
    case TYPE_CODE_ARRAY:
      if (TYPE_LENGTH (type) > 0 && TYPE_LENGTH (TYPE_TARGET_TYPE (type)) > 0)
	{
	  elttype = check_typedef (TYPE_TARGET_TYPE (type));
	  eltlen = TYPE_LENGTH (elttype);
	  len = TYPE_LENGTH (type) / eltlen;
	  if (prettyprint_arrays)
	    print_spaces_filtered (2 + 2 * recurse, stream);
	  /* For an array of chars, print with string syntax.  */
	  if (eltlen == 1 &&
	      ((TYPE_CODE (elttype) == TYPE_CODE_INT)
	       || ((current_language->la_language == language_m2)
		   && (TYPE_CODE (elttype) == TYPE_CODE_CHAR)))
	      && (format == 0 || format == 's'))
	    {
	      /* If requested, look for the first null char and only print
	         elements up to it.  */
	      if (stop_print_at_null)
		{
		  unsigned int temp_len;

		  /* Look for a NULL char. */
		  for (temp_len = 0;
		       (valaddr + embedded_offset)[temp_len]
			 && temp_len < len && temp_len < print_max;
		       temp_len++);
		  len = temp_len;
		}

	      LA_PRINT_STRING (stream, valaddr + embedded_offset, len, 1, 0);
	      i = len;
	    }
	  else
	    {
	      fprintf_filtered (stream, "{");
	      val_print_array_elements (type, valaddr + embedded_offset,
					address, stream, format, deref_ref,
					recurse, pretty, 0);
	      fprintf_filtered (stream, "}");
	    }
	  break;
	}
      /* Array of unspecified length: treat like pointer to first elt.  */
      print_unpacked_pointer (type, address, address, format, stream);
      break;

    case TYPE_CODE_PTR:
      if (TYPE_CONST (type))
	print_variable_at_address (type, valaddr + embedded_offset,
				   stream, format, deref_ref, recurse,
				   pretty);
      else if (format && format != 's')
	print_scalar_formatted (valaddr + embedded_offset, type, format,
				0, stream);
      else
	{
	  addr = unpack_pointer (type, valaddr + embedded_offset);
	  print_unpacked_pointer (type, addr, address, format, stream);
	}
      break;

    case TYPE_CODE_REF:
      elttype = check_typedef (TYPE_TARGET_TYPE (type));
      if (addressprint)
	{
	  CORE_ADDR addr
	    = extract_typed_address (valaddr + embedded_offset, type);
	  fprintf_filtered (stream, "@");
	  fputs_filtered (paddress (addr), stream);
	  if (deref_ref)
	    fputs_filtered (": ", stream);
	}
      /* De-reference the reference.  */
      if (deref_ref)
	{
	  if (TYPE_CODE (elttype) != TYPE_CODE_UNDEF)
	    {
	      struct value *deref_val =
		value_at
		(TYPE_TARGET_TYPE (type),
		 unpack_pointer (lookup_pointer_type (builtin_type_void),
				 valaddr + embedded_offset));
	      common_val_print (deref_val, stream, format, deref_ref,
				recurse, pretty);
	    }
	  else
	    fputs_filtered ("???", stream);
	}
      break;

    case TYPE_CODE_UNION:
      if (recurse && !unionprint)
	{
	  fprintf_filtered (stream, "{...}");
	  break;
	}
      /* Fall through.  */
    case TYPE_CODE_STRUCT:
      if (m2_is_long_set (type))
	m2_print_long_set (type, valaddr, embedded_offset, address,
			   stream, format, pretty);
      else
	cp_print_value_fields (type, type, valaddr, embedded_offset,
			       address, stream, format,
			       recurse, pretty, NULL, 0);
      break;

    case TYPE_CODE_ENUM:
      if (format)
	{
	  print_scalar_formatted (valaddr + embedded_offset, type,
				  format, 0, stream);
	  break;
	}
      len = TYPE_NFIELDS (type);
      val = unpack_long (type, valaddr + embedded_offset);
      for (i = 0; i < len; i++)
	{
	  QUIT;
	  if (val == TYPE_FIELD_BITPOS (type, i))
	    {
	      break;
	    }
	}
      if (i < len)
	{
	  fputs_filtered (TYPE_FIELD_NAME (type, i), stream);
	}
      else
	{
	  print_longest (stream, 'd', 0, val);
	}
      break;

    case TYPE_CODE_FUNC:
      if (format)
	{
	  print_scalar_formatted (valaddr + embedded_offset, type,
				  format, 0, stream);
	  break;
	}
      /* FIXME, we should consider, at least for ANSI C language, eliminating
         the distinction made between FUNCs and POINTERs to FUNCs.  */
      fprintf_filtered (stream, "{");
      type_print (type, "", stream, -1);
      fprintf_filtered (stream, "} ");
      /* Try to print what function it points to, and its address.  */
      print_address_demangle (address, stream, demangle);
      break;

    case TYPE_CODE_BOOL:
      format = format ? format : output_format;
      if (format)
	print_scalar_formatted (valaddr + embedded_offset, type,
				format, 0, stream);
      else
	{
	  val = unpack_long (type, valaddr + embedded_offset);
	  if (val == 0)
	    fputs_filtered ("FALSE", stream);
	  else if (val == 1)
	    fputs_filtered ("TRUE", stream);
	  else
	    fprintf_filtered (stream, "%ld)", (long int) val);
	}
      break;

    case TYPE_CODE_RANGE:
      if (TYPE_LENGTH (type) == TYPE_LENGTH (TYPE_TARGET_TYPE (type)))
	{
	  m2_val_print (TYPE_TARGET_TYPE (type), valaddr, embedded_offset,
			address, stream, format, deref_ref, recurse, pretty);
	  break;
	}
      /* FIXME: create_range_type does not set the unsigned bit in a
         range type (I think it probably should copy it from the target
         type), so we won't print values which are too large to
         fit in a signed integer correctly.  */
      /* FIXME: Doesn't handle ranges of enums correctly.  (Can't just
         print with the target type, though, because the size of our type
         and the target type might differ).  */
      /* FALLTHROUGH */

    case TYPE_CODE_INT:
      format = format ? format : output_format;
      if (format)
	print_scalar_formatted (valaddr + embedded_offset, type, format,
				0, stream);
      else
	val_print_type_code_int (type, valaddr + embedded_offset, stream);
      break;

    case TYPE_CODE_CHAR:
      format = format ? format : output_format;
      if (format)
	print_scalar_formatted (valaddr + embedded_offset, type,
				format, 0, stream);
      else
	{
	  val = unpack_long (type, valaddr + embedded_offset);
	  if (TYPE_UNSIGNED (type))
	    fprintf_filtered (stream, "%u", (unsigned int) val);
	  else
	    fprintf_filtered (stream, "%d", (int) val);
	  fputs_filtered (" ", stream);
	  LA_PRINT_CHAR ((unsigned char) val, stream);
	}
      break;

    case TYPE_CODE_FLT:
      if (format)
	print_scalar_formatted (valaddr + embedded_offset, type,
				format, 0, stream);
      else
	print_floating (valaddr + embedded_offset, type, stream);
      break;

    case TYPE_CODE_METHOD:
      break;

    case TYPE_CODE_BITSTRING:
    case TYPE_CODE_SET:
      elttype = TYPE_INDEX_TYPE (type);
      CHECK_TYPEDEF (elttype);
      if (TYPE_STUB (elttype))
	{
	  fprintf_filtered (stream, _("<incomplete type>"));
	  gdb_flush (stream);
	  break;
	}
      else
	{
	  struct type *range = elttype;
	  LONGEST low_bound, high_bound;
	  int i;
	  int is_bitstring = TYPE_CODE (type) == TYPE_CODE_BITSTRING;
	  int need_comma = 0;

	  if (is_bitstring)
	    fputs_filtered ("B'", stream);
	  else
	    fputs_filtered ("{", stream);

	  i = get_discrete_bounds (range, &low_bound, &high_bound);
	maybe_bad_bstring:
	  if (i < 0)
	    {
	      fputs_filtered (_("<error value>"), stream);
	      goto done;
	    }

	  for (i = low_bound; i <= high_bound; i++)
	    {
	      int element = value_bit_index (type, valaddr + embedded_offset,
					     i);
	      if (element < 0)
		{
		  i = element;
		  goto maybe_bad_bstring;
		}
	      if (is_bitstring)
		fprintf_filtered (stream, "%d", element);
	      else if (element)
		{
		  if (need_comma)
		    fputs_filtered (", ", stream);
		  print_type_scalar (range, i, stream);
		  need_comma = 1;

		  if (i + 1 <= high_bound
		      && value_bit_index (type, valaddr + embedded_offset,
					  ++i))
		    {
		      int j = i;
		      fputs_filtered ("..", stream);
		      while (i + 1 <= high_bound
			     && value_bit_index (type,
						 valaddr + embedded_offset,
						 ++i))
			j = i;
		      print_type_scalar (range, j, stream);
		    }
		}
	    }
	done:
	  if (is_bitstring)
	    fputs_filtered ("'", stream);
	  else
	    fputs_filtered ("}", stream);
	}
      break;

    case TYPE_CODE_VOID:
      fprintf_filtered (stream, "void");
      break;

    case TYPE_CODE_ERROR:
      fprintf_filtered (stream, _("<error type>"));
      break;

    case TYPE_CODE_UNDEF:
      /* This happens (without TYPE_FLAG_STUB set) on systems which don't use
         dbx xrefs (NO_DBX_XREFS in gcc) if a file has a "struct foo *bar"
         and no complete type for struct foo in that file.  */
      fprintf_filtered (stream, _("<incomplete type>"));
      break;

    default:
      error (_("Invalid m2 type code %d in symbol table."), TYPE_CODE (type));
    }
  gdb_flush (stream);
  return (0);
}
