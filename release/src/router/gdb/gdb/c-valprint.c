/* Support for printing C values for GDB, the GNU debugger.

   Copyright (C) 1986, 1988, 1989, 1991, 1992, 1993, 1994, 1995, 1996, 1997,
   1998, 1999, 2000, 2001, 2003, 2005, 2006, 2007
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
#include "gdb_string.h"
#include "symtab.h"
#include "gdbtypes.h"
#include "expression.h"
#include "value.h"
#include "valprint.h"
#include "language.h"
#include "c-lang.h"
#include "cp-abi.h"
#include "target.h"


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
      deprecated_print_address_numeric (address, 1, stream);
      fputs_filtered (": ", stream);
    }
  print_address_demangle (func_addr, stream, demangle);
}


/* Apply a heuristic to decide whether an array of TYPE or a pointer
   to TYPE should be printed as a textual string.  Return non-zero if
   it should, or zero if it should be treated as an array of integers
   or pointer to integers.  FORMAT is the current format letter,
   or 0 if none.

   We guess that "char" is a character.  Explicitly signed and
   unsigned character types are also characters.  Integer data from
   vector types is not.  The user can override this by using the /s
   format letter.  */

static int
textual_element_type (struct type *type, char format)
{
  struct type *true_type = check_typedef (type);

  if (format != 0 && format != 's')
    return 0;

  /* TYPE_CODE_CHAR is always textual.  */
  if (TYPE_CODE (true_type) == TYPE_CODE_CHAR)
    return 1;

  if (format == 's')
    {
      /* Print this as a string if we can manage it.  For now, no
	 wide character support.  */
      if (TYPE_CODE (true_type) == TYPE_CODE_INT
	  && TYPE_LENGTH (true_type) == 1)
	return 1;
    }
  else
    {
      /* If a one-byte TYPE_CODE_INT is missing the not-a-character
	 flag, then we treat it as text; otherwise, we assume it's
	 being used as data.  */
      if (TYPE_CODE (true_type) == TYPE_CODE_INT
	  && TYPE_LENGTH (true_type) == 1
	  && !TYPE_NOTTEXT (true_type))
	return 1;
    }

  return 0;
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
c_val_print (struct type *type, const gdb_byte *valaddr, int embedded_offset,
	     CORE_ADDR address, struct ui_file *stream, int format,
	     int deref_ref, int recurse, enum val_prettyprint pretty)
{
  unsigned int i = 0;	/* Number of characters printed */
  unsigned len;
  struct type *elttype;
  unsigned eltlen;
  LONGEST val;
  CORE_ADDR addr;

  CHECK_TYPEDEF (type);
  switch (TYPE_CODE (type))
    {
    case TYPE_CODE_ARRAY:
      elttype = check_typedef (TYPE_TARGET_TYPE (type));
      if (TYPE_LENGTH (type) > 0 && TYPE_LENGTH (TYPE_TARGET_TYPE (type)) > 0)
	{
	  eltlen = TYPE_LENGTH (elttype);
	  len = TYPE_LENGTH (type) / eltlen;
	  if (prettyprint_arrays)
	    {
	      print_spaces_filtered (2 + 2 * recurse, stream);
	    }

	  /* Print arrays of textual chars with a string syntax.  */
          if (textual_element_type (elttype, format))
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

	      LA_PRINT_STRING (stream, valaddr + embedded_offset, len, eltlen, 0);
	      i = len;
	    }
	  else
	    {
	      fprintf_filtered (stream, "{");
	      /* If this is a virtual function table, print the 0th
	         entry specially, and the rest of the members normally.  */
	      if (cp_is_vtbl_ptr_type (elttype))
		{
		  i = 1;
		  fprintf_filtered (stream, _("%d vtable entries"), len - 1);
		}
	      else
		{
		  i = 0;
		}
	      val_print_array_elements (type, valaddr + embedded_offset, address, stream,
				     format, deref_ref, recurse, pretty, i);
	      fprintf_filtered (stream, "}");
	    }
	  break;
	}
      /* Array of unspecified length: treat like pointer to first elt.  */
      addr = address;
      goto print_unpacked_pointer;

    case TYPE_CODE_MEMBERPTR:
      if (format)
	{
	  print_scalar_formatted (valaddr + embedded_offset, type, format, 0, stream);
	  break;
	}
      cp_print_class_member (valaddr + embedded_offset,
			     TYPE_DOMAIN_TYPE (type),
			     stream, "&");
      break;

    case TYPE_CODE_METHODPTR:
      cplus_print_method_ptr (valaddr + embedded_offset, type, stream);
      break;

    case TYPE_CODE_PTR:
      if (format && format != 's')
	{
	  print_scalar_formatted (valaddr + embedded_offset, type, format, 0, stream);
	  break;
	}
      if (vtblprint && cp_is_vtbl_ptr_type (type))
	{
	  /* Print the unmangled name if desired.  */
	  /* Print vtable entry - we only get here if we ARE using
	     -fvtable_thunks.  (Otherwise, look under TYPE_CODE_STRUCT.) */
	  CORE_ADDR addr
	    = extract_typed_address (valaddr + embedded_offset, type);
	  print_function_pointer_address (addr, stream);
	  break;
	}
      elttype = check_typedef (TYPE_TARGET_TYPE (type));
	{
	  addr = unpack_pointer (type, valaddr + embedded_offset);
	print_unpacked_pointer:

	  if (TYPE_CODE (elttype) == TYPE_CODE_FUNC)
	    {
	      /* Try to print what function it points to.  */
	      print_function_pointer_address (addr, stream);
	      /* Return value is irrelevant except for string pointers.  */
	      return (0);
	    }

	  if (addressprint)
	    {
	      deprecated_print_address_numeric (addr, 1, stream);
	    }

	  /* For a pointer to a textual type, also print the string
	     pointed to, unless pointer is null.  */
	  /* FIXME: need to handle wchar_t here... */

	  if (textual_element_type (elttype, format) && addr != 0)
	    {
	      i = val_print_string (addr, -1, TYPE_LENGTH (elttype), stream);
	    }
	  else if (cp_is_vtbl_member (type))
	    {
	      /* print vtbl's nicely */
	      CORE_ADDR vt_address = unpack_pointer (type, valaddr + embedded_offset);

	      struct minimal_symbol *msymbol =
	      lookup_minimal_symbol_by_pc (vt_address);
	      if ((msymbol != NULL) &&
		  (vt_address == SYMBOL_VALUE_ADDRESS (msymbol)))
		{
		  fputs_filtered (" <", stream);
		  fputs_filtered (SYMBOL_PRINT_NAME (msymbol), stream);
		  fputs_filtered (">", stream);
		}
	      if (vt_address && vtblprint)
		{
		  struct value *vt_val;
		  struct symbol *wsym = (struct symbol *) NULL;
		  struct type *wtype;
		  struct block *block = (struct block *) NULL;
		  int is_this_fld;

		  if (msymbol != NULL)
		    wsym = lookup_symbol (DEPRECATED_SYMBOL_NAME (msymbol), block,
					  VAR_DOMAIN, &is_this_fld, NULL);

		  if (wsym)
		    {
		      wtype = SYMBOL_TYPE (wsym);
		    }
		  else
		    {
		      wtype = TYPE_TARGET_TYPE (type);
		    }
		  vt_val = value_at (wtype, vt_address);
		  common_val_print (vt_val, stream, format,
				    deref_ref, recurse + 1, pretty);
		  if (pretty)
		    {
		      fprintf_filtered (stream, "\n");
		      print_spaces_filtered (2 + 2 * recurse, stream);
		    }
		}
	    }

	  /* Return number of characters printed, including the terminating
	     '\0' if we reached the end.  val_print_string takes care including
	     the terminating '\0' if necessary.  */
	  return i;
	}
      break;

    case TYPE_CODE_REF:
      elttype = check_typedef (TYPE_TARGET_TYPE (type));
      if (addressprint)
	{
	  CORE_ADDR addr
	    = extract_typed_address (valaddr + embedded_offset, type);
	  fprintf_filtered (stream, "@");
	  deprecated_print_address_numeric (addr, 1, stream);
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
      /*FIXME: Abstract this away */
      if (vtblprint && cp_is_vtbl_ptr_type (type))
	{
	  /* Print the unmangled name if desired.  */
	  /* Print vtable entry - we only get here if NOT using
	     -fvtable_thunks.  (Otherwise, look under TYPE_CODE_PTR.) */
	  int offset = (embedded_offset +
			TYPE_FIELD_BITPOS (type, VTBL_FNADDR_OFFSET) / 8);
	  struct type *field_type = TYPE_FIELD_TYPE (type, VTBL_FNADDR_OFFSET);
	  CORE_ADDR addr
	    = extract_typed_address (valaddr + offset, field_type);

	  print_function_pointer_address (addr, stream);
	}
      else
	cp_print_value_fields (type, type, valaddr, embedded_offset, address, stream, format,
			       recurse, pretty, NULL, 0);
      break;

    case TYPE_CODE_ENUM:
      if (format)
	{
	  print_scalar_formatted (valaddr + embedded_offset, type, format, 0, stream);
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

    case TYPE_CODE_FLAGS:
      if (format)
	  print_scalar_formatted (valaddr + embedded_offset, type, format, 0, stream);
      else
	val_print_type_code_flags (type, valaddr + embedded_offset, stream);
      break;

    case TYPE_CODE_FUNC:
    case TYPE_CODE_METHOD:
      if (format)
	{
	  print_scalar_formatted (valaddr + embedded_offset, type, format, 0, stream);
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
	print_scalar_formatted (valaddr + embedded_offset, type, format, 0, stream);
      else
	{
	  val = unpack_long (type, valaddr + embedded_offset);
	  if (val == 0)
	    fputs_filtered ("false", stream);
	  else if (val == 1)
	    fputs_filtered ("true", stream);
	  else
	    print_longest (stream, 'd', 0, val);
	}
      break;

    case TYPE_CODE_RANGE:
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
	{
	  print_scalar_formatted (valaddr + embedded_offset, type, format, 0, stream);
	}
      else
	{
	  val_print_type_code_int (type, valaddr + embedded_offset, stream);
	  /* C and C++ has no single byte int type, char is used instead.
	     Since we don't know whether the value is really intended to
	     be used as an integer or a character, print the character
	     equivalent as well.  */
	  if (textual_element_type (type, format))
	    {
	      fputs_filtered (" ", stream);
	      LA_PRINT_CHAR ((unsigned char) unpack_long (type, valaddr + embedded_offset),
			     stream);
	    }
	}
      break;

    case TYPE_CODE_CHAR:
      format = format ? format : output_format;
      if (format)
	{
	  print_scalar_formatted (valaddr + embedded_offset, type, format, 0, stream);
	}
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
	{
	  print_scalar_formatted (valaddr + embedded_offset, type, format, 0, stream);
	}
      else
	{
	  print_floating (valaddr + embedded_offset, type, stream);
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

    case TYPE_CODE_COMPLEX:
      if (format)
	print_scalar_formatted (valaddr + embedded_offset,
				TYPE_TARGET_TYPE (type),
				format, 0, stream);
      else
	print_floating (valaddr + embedded_offset, TYPE_TARGET_TYPE (type),
			stream);
      fprintf_filtered (stream, " + ");
      if (format)
	print_scalar_formatted (valaddr + embedded_offset
				+ TYPE_LENGTH (TYPE_TARGET_TYPE (type)),
				TYPE_TARGET_TYPE (type),
				format, 0, stream);
      else
	print_floating (valaddr + embedded_offset
			+ TYPE_LENGTH (TYPE_TARGET_TYPE (type)),
			TYPE_TARGET_TYPE (type),
			stream);
      fprintf_filtered (stream, " * I");
      break;

    default:
      error (_("Invalid C/C++ type code %d in symbol table."), TYPE_CODE (type));
    }
  gdb_flush (stream);
  return (0);
}

int
c_value_print (struct value *val, struct ui_file *stream, int format,
	       enum val_prettyprint pretty)
{
  struct type *type, *real_type;
  int full, top, using_enc;

  /* If it is a pointer, indicate what it points to.

     Print type also if it is a reference.

     C++: if it is a member pointer, we will take care
     of that when we print it.  */

  type = check_typedef (value_type (val));

  if (TYPE_CODE (type) == TYPE_CODE_PTR
      || TYPE_CODE (type) == TYPE_CODE_REF)
    {
      /* Hack:  remove (char *) for char strings.  Their
         type is indicated by the quoted string anyway.
         (Don't use textual_element_type here; quoted strings
         are always exactly (char *).  */
      if (TYPE_CODE (type) == TYPE_CODE_PTR
	  && TYPE_NAME (type) == NULL
	  && TYPE_NAME (TYPE_TARGET_TYPE (type)) != NULL
	  && strcmp (TYPE_NAME (TYPE_TARGET_TYPE (type)), "char") == 0)
	{
	  /* Print nothing */
	}
      else if (objectprint && (TYPE_CODE (TYPE_TARGET_TYPE (type)) == TYPE_CODE_CLASS))
	{

	  if (TYPE_CODE(type) == TYPE_CODE_REF)
	    {
	      /* Copy value, change to pointer, so we don't get an
	       * error about a non-pointer type in value_rtti_target_type
	       */
	      struct value *temparg;
	      temparg=value_copy(val);
	      deprecated_set_value_type (temparg, lookup_pointer_type (TYPE_TARGET_TYPE(type)));
	      val=temparg;
	    }
	  /* Pointer to class, check real type of object */
	  fprintf_filtered (stream, "(");
          real_type = value_rtti_target_type (val, &full, &top, &using_enc);
          if (real_type)
	    {
	      /* RTTI entry found */
              if (TYPE_CODE (type) == TYPE_CODE_PTR)
                {
                  /* create a pointer type pointing to the real type */
                  type = lookup_pointer_type (real_type);
                }
              else
                {
                  /* create a reference type referencing the real type */
                  type = lookup_reference_type (real_type);
                }
	      /* JYG: Need to adjust pointer value. */
	      /* NOTE: cagney/2005-01-02: THIS IS BOGUS.  */
              value_contents_writeable (val)[0] -= top;

              /* Note: When we look up RTTI entries, we don't get any 
                 information on const or volatile attributes */
            }
          type_print (type, "", stream, -1);
	  fprintf_filtered (stream, ") ");
	}
      else
	{
	  /* normal case */
	  fprintf_filtered (stream, "(");
	  type_print (value_type (val), "", stream, -1);
	  fprintf_filtered (stream, ") ");
	}
    }

  if (!value_initialized (val))
    fprintf_filtered (stream, " [uninitialized] ");

  if (objectprint && (TYPE_CODE (type) == TYPE_CODE_CLASS))
    {
      /* Attempt to determine real type of object */
      real_type = value_rtti_type (val, &full, &top, &using_enc);
      if (real_type)
	{
	  /* We have RTTI information, so use it */
	  val = value_full_object (val, real_type, full, top, using_enc);
	  fprintf_filtered (stream, "(%s%s) ",
			    TYPE_NAME (real_type),
			    full ? "" : _(" [incomplete object]"));
	  /* Print out object: enclosing type is same as real_type if full */
	  return val_print (value_enclosing_type (val),
			    value_contents_all (val), 0,
			    VALUE_ADDRESS (val), stream, format, 1, 0, pretty);
          /* Note: When we look up RTTI entries, we don't get any information on
             const or volatile attributes */
	}
      else if (type != check_typedef (value_enclosing_type (val)))
	{
	  /* No RTTI information, so let's do our best */
	  fprintf_filtered (stream, "(%s ?) ",
			    TYPE_NAME (value_enclosing_type (val)));
	  return val_print (value_enclosing_type (val),
			    value_contents_all (val), 0,
			    VALUE_ADDRESS (val), stream, format, 1, 0, pretty);
	}
      /* Otherwise, we end up at the return outside this "if" */
    }

  return val_print (type, value_contents_all (val),
		    value_embedded_offset (val),
		    VALUE_ADDRESS (val) + value_offset (val),
		    stream, format, 1, 0, pretty);
}
