/* Abstraction of GNU v3 abi.
   Contributed by Jim Blandy <jimb@redhat.com>

   Copyright (C) 2001, 2002, 2003, 2005, 2006, 2007
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
#include "value.h"
#include "cp-abi.h"
#include "cp-support.h"
#include "demangle.h"
#include "objfiles.h"
#include "valprint.h"

#include "gdb_assert.h"
#include "gdb_string.h"

static struct cp_abi_ops gnu_v3_abi_ops;

static int
gnuv3_is_vtable_name (const char *name)
{
  return strncmp (name, "_ZTV", 4) == 0;
}

static int
gnuv3_is_operator_name (const char *name)
{
  return strncmp (name, "operator", 8) == 0;
}


/* To help us find the components of a vtable, we build ourselves a
   GDB type object representing the vtable structure.  Following the
   V3 ABI, it goes something like this:

   struct gdb_gnu_v3_abi_vtable {

     / * An array of virtual call and virtual base offsets.  The real
         length of this array depends on the class hierarchy; we use
         negative subscripts to access the elements.  Yucky, but
         better than the alternatives.  * /
     ptrdiff_t vcall_and_vbase_offsets[0];

     / * The offset from a virtual pointer referring to this table
         to the top of the complete object.  * /
     ptrdiff_t offset_to_top;

     / * The type_info pointer for this class.  This is really a
         std::type_info *, but GDB doesn't really look at the
         type_info object itself, so we don't bother to get the type
         exactly right.  * /
     void *type_info;

     / * Virtual table pointers in objects point here.  * /

     / * Virtual function pointers.  Like the vcall/vbase array, the
         real length of this table depends on the class hierarchy.  * /
     void (*virtual_functions[0]) ();

   };

   The catch, of course, is that the exact layout of this table
   depends on the ABI --- word size, endianness, alignment, etc.  So
   the GDB type object is actually a per-architecture kind of thing.

   vtable_type_gdbarch_data is a gdbarch per-architecture data pointer
   which refers to the struct type * for this structure, laid out
   appropriately for the architecture.  */
static struct gdbarch_data *vtable_type_gdbarch_data;


/* Human-readable names for the numbers of the fields above.  */
enum {
  vtable_field_vcall_and_vbase_offsets,
  vtable_field_offset_to_top,
  vtable_field_type_info,
  vtable_field_virtual_functions
};


/* Return a GDB type representing `struct gdb_gnu_v3_abi_vtable',
   described above, laid out appropriately for ARCH.

   We use this function as the gdbarch per-architecture data
   initialization function.  We assume that the gdbarch framework
   calls the per-architecture data initialization functions after it
   sets current_gdbarch to the new architecture.  */
static void *
build_gdb_vtable_type (struct gdbarch *arch)
{
  struct type *t;
  struct field *field_list, *field;
  int offset;

  struct type *void_ptr_type
    = lookup_pointer_type (builtin_type_void);
  struct type *ptr_to_void_fn_type
    = lookup_pointer_type (lookup_function_type (builtin_type_void));

  /* ARCH can't give us the true ptrdiff_t type, so we guess.  */
  struct type *ptrdiff_type
    = init_type (TYPE_CODE_INT,
		 gdbarch_ptr_bit (current_gdbarch) / TARGET_CHAR_BIT, 0,
                 "ptrdiff_t", 0);

  /* We assume no padding is necessary, since GDB doesn't know
     anything about alignment at the moment.  If this assumption bites
     us, we should add a gdbarch method which, given a type, returns
     the alignment that type requires, and then use that here.  */

  /* Build the field list.  */
  field_list = xmalloc (sizeof (struct field [4]));
  memset (field_list, 0, sizeof (struct field [4]));
  field = &field_list[0];
  offset = 0;

  /* ptrdiff_t vcall_and_vbase_offsets[0]; */
  FIELD_NAME (*field) = "vcall_and_vbase_offsets";
  FIELD_TYPE (*field)
    = create_array_type (0, ptrdiff_type,
                         create_range_type (0, builtin_type_int, 0, -1));
  FIELD_BITPOS (*field) = offset * TARGET_CHAR_BIT;
  offset += TYPE_LENGTH (FIELD_TYPE (*field));
  field++;

  /* ptrdiff_t offset_to_top; */
  FIELD_NAME (*field) = "offset_to_top";
  FIELD_TYPE (*field) = ptrdiff_type;
  FIELD_BITPOS (*field) = offset * TARGET_CHAR_BIT;
  offset += TYPE_LENGTH (FIELD_TYPE (*field));
  field++;

  /* void *type_info; */
  FIELD_NAME (*field) = "type_info";
  FIELD_TYPE (*field) = void_ptr_type;
  FIELD_BITPOS (*field) = offset * TARGET_CHAR_BIT;
  offset += TYPE_LENGTH (FIELD_TYPE (*field));
  field++;

  /* void (*virtual_functions[0]) (); */
  FIELD_NAME (*field) = "virtual_functions";
  FIELD_TYPE (*field)
    = create_array_type (0, ptr_to_void_fn_type,
                         create_range_type (0, builtin_type_int, 0, -1));
  FIELD_BITPOS (*field) = offset * TARGET_CHAR_BIT;
  offset += TYPE_LENGTH (FIELD_TYPE (*field));
  field++;

  /* We assumed in the allocation above that there were four fields.  */
  gdb_assert (field == (field_list + 4));

  t = init_type (TYPE_CODE_STRUCT, offset, 0, 0, 0);
  TYPE_NFIELDS (t) = field - field_list;
  TYPE_FIELDS (t) = field_list;
  TYPE_TAG_NAME (t) = "gdb_gnu_v3_abi_vtable";

  return t;
}


/* Return the offset from the start of the imaginary `struct
   gdb_gnu_v3_abi_vtable' object to the vtable's "address point"
   (i.e., where objects' virtual table pointers point).  */
static int
vtable_address_point_offset (void)
{
  struct type *vtable_type = gdbarch_data (current_gdbarch,
					   vtable_type_gdbarch_data);

  return (TYPE_FIELD_BITPOS (vtable_type, vtable_field_virtual_functions)
          / TARGET_CHAR_BIT);
}


static struct type *
gnuv3_rtti_type (struct value *value,
                 int *full_p, int *top_p, int *using_enc_p)
{
  struct type *vtable_type = gdbarch_data (current_gdbarch,
					   vtable_type_gdbarch_data);
  struct type *values_type = check_typedef (value_type (value));
  CORE_ADDR vtable_address;
  struct value *vtable;
  struct minimal_symbol *vtable_symbol;
  const char *vtable_symbol_name;
  const char *class_name;
  struct type *run_time_type;
  struct type *base_type;
  LONGEST offset_to_top;

  /* We only have RTTI for class objects.  */
  if (TYPE_CODE (values_type) != TYPE_CODE_CLASS)
    return NULL;

  /* If we can't find the virtual table pointer for values_type, we
     can't find the RTTI.  */
  fill_in_vptr_fieldno (values_type);
  if (TYPE_VPTR_FIELDNO (values_type) == -1)
    return NULL;

  if (using_enc_p)
    *using_enc_p = 0;

  /* Fetch VALUE's virtual table pointer, and tweak it to point at
     an instance of our imaginary gdb_gnu_v3_abi_vtable structure.  */
  base_type = check_typedef (TYPE_VPTR_BASETYPE (values_type));
  if (values_type != base_type)
    {
      value = value_cast (base_type, value);
      if (using_enc_p)
	*using_enc_p = 1;
    }
  vtable_address
    = value_as_address (value_field (value, TYPE_VPTR_FIELDNO (values_type)));
  vtable = value_at_lazy (vtable_type,
                          vtable_address - vtable_address_point_offset ());
  
  /* Find the linker symbol for this vtable.  */
  vtable_symbol
    = lookup_minimal_symbol_by_pc (VALUE_ADDRESS (vtable)
                                   + value_offset (vtable)
                                   + value_embedded_offset (vtable));
  if (! vtable_symbol)
    return NULL;
  
  /* The symbol's demangled name should be something like "vtable for
     CLASS", where CLASS is the name of the run-time type of VALUE.
     If we didn't like this approach, we could instead look in the
     type_info object itself to get the class name.  But this way
     should work just as well, and doesn't read target memory.  */
  vtable_symbol_name = SYMBOL_DEMANGLED_NAME (vtable_symbol);
  if (vtable_symbol_name == NULL
      || strncmp (vtable_symbol_name, "vtable for ", 11))
    {
      warning (_("can't find linker symbol for virtual table for `%s' value"),
	       TYPE_NAME (values_type));
      if (vtable_symbol_name)
	warning (_("  found `%s' instead"), vtable_symbol_name);
      return NULL;
    }
  class_name = vtable_symbol_name + 11;

  /* Try to look up the class name as a type name.  */
  /* FIXME: chastain/2003-11-26: block=NULL is bogus.  See pr gdb/1465. */
  run_time_type = cp_lookup_rtti_type (class_name, NULL);
  if (run_time_type == NULL)
    return NULL;

  /* Get the offset from VALUE to the top of the complete object.
     NOTE: this is the reverse of the meaning of *TOP_P.  */
  offset_to_top
    = value_as_long (value_field (vtable, vtable_field_offset_to_top));

  if (full_p)
    *full_p = (- offset_to_top == value_embedded_offset (value)
               && (TYPE_LENGTH (value_enclosing_type (value))
                   >= TYPE_LENGTH (run_time_type)));
  if (top_p)
    *top_p = - offset_to_top;

  return run_time_type;
}

/* Find the vtable for CONTAINER and return a value of the correct
   vtable type for this architecture.  */

static struct value *
gnuv3_get_vtable (struct value *container)
{
  struct type *vtable_type = gdbarch_data (current_gdbarch,
					   vtable_type_gdbarch_data);
  struct type *vtable_pointer_type;
  struct value *vtable_pointer;
  CORE_ADDR vtable_pointer_address, vtable_address;

  /* We do not consult the debug information to find the virtual table.
     The ABI specifies that it is always at offset zero in any class,
     and debug information may not represent it.  We won't issue an
     error if there's a class with virtual functions but no virtual table
     pointer, but something's already gone seriously wrong if that
     happens.

     We avoid using value_contents on principle, because the object might
     be large.  */

  /* Find the type "pointer to virtual table".  */
  vtable_pointer_type = lookup_pointer_type (vtable_type);

  /* Load it from the start of the class.  */
  vtable_pointer_address = value_as_address (value_addr (container));
  vtable_pointer = value_at (vtable_pointer_type, vtable_pointer_address);
  vtable_address = value_as_address (vtable_pointer);

  /* Correct it to point at the start of the virtual table, rather
     than the address point.  */
  return value_at_lazy (vtable_type,
			vtable_address - vtable_address_point_offset ());
}

/* Return a function pointer for CONTAINER's VTABLE_INDEX'th virtual
   function, of type FNTYPE.  */

static struct value *
gnuv3_get_virtual_fn (struct value *container, struct type *fntype,
		      int vtable_index)
{
  struct value *vtable = gnuv3_get_vtable (container);
  struct value *vfn;

  /* Fetch the appropriate function pointer from the vtable.  */
  vfn = value_subscript (value_field (vtable, vtable_field_virtual_functions),
                         value_from_longest (builtin_type_int, vtable_index));

  /* If this architecture uses function descriptors directly in the vtable,
     then the address of the vtable entry is actually a "function pointer"
     (i.e. points to the descriptor).  We don't need to scale the index
     by the size of a function descriptor; GCC does that before outputing
     debug information.  */
  if (gdbarch_vtable_function_descriptors (current_gdbarch))
    vfn = value_addr (vfn);

  /* Cast the function pointer to the appropriate type.  */
  vfn = value_cast (lookup_pointer_type (fntype), vfn);

  return vfn;
}

/* GNU v3 implementation of value_virtual_fn_field.  See cp-abi.h
   for a description of the arguments.  */

static struct value *
gnuv3_virtual_fn_field (struct value **value_p,
                        struct fn_field *f, int j,
			struct type *vfn_base, int offset)
{
  struct type *values_type = check_typedef (value_type (*value_p));

  /* Some simple sanity checks.  */
  if (TYPE_CODE (values_type) != TYPE_CODE_CLASS)
    error (_("Only classes can have virtual functions."));

  /* Cast our value to the base class which defines this virtual
     function.  This takes care of any necessary `this'
     adjustments.  */
  if (vfn_base != values_type)
    *value_p = value_cast (vfn_base, *value_p);

  return gnuv3_get_virtual_fn (*value_p, TYPE_FN_FIELD_TYPE (f, j),
			       TYPE_FN_FIELD_VOFFSET (f, j));
}

/* Compute the offset of the baseclass which is
   the INDEXth baseclass of class TYPE,
   for value at VALADDR (in host) at ADDRESS (in target).
   The result is the offset of the baseclass value relative
   to (the address of)(ARG) + OFFSET.

   -1 is returned on error. */
static int
gnuv3_baseclass_offset (struct type *type, int index, const bfd_byte *valaddr,
			CORE_ADDR address)
{
  struct type *vtable_type = gdbarch_data (current_gdbarch,
					   vtable_type_gdbarch_data);
  struct value *vtable;
  struct type *vbasetype;
  struct value *offset_val, *vbase_array;
  CORE_ADDR vtable_address;
  long int cur_base_offset, base_offset;

  /* If it isn't a virtual base, this is easy.  The offset is in the
     type definition.  */
  if (!BASETYPE_VIA_VIRTUAL (type, index))
    return TYPE_BASECLASS_BITPOS (type, index) / 8;

  /* To access a virtual base, we need to use the vbase offset stored in
     our vtable.  Recent GCC versions provide this information.  If it isn't
     available, we could get what we needed from RTTI, or from drawing the
     complete inheritance graph based on the debug info.  Neither is
     worthwhile.  */
  cur_base_offset = TYPE_BASECLASS_BITPOS (type, index) / 8;
  if (cur_base_offset >= - vtable_address_point_offset ())
    error (_("Expected a negative vbase offset (old compiler?)"));

  cur_base_offset = cur_base_offset + vtable_address_point_offset ();
  if ((- cur_base_offset) % TYPE_LENGTH (builtin_type_void_data_ptr) != 0)
    error (_("Misaligned vbase offset."));
  cur_base_offset = cur_base_offset
    / ((int) TYPE_LENGTH (builtin_type_void_data_ptr));

  /* We're now looking for the cur_base_offset'th entry (negative index)
     in the vcall_and_vbase_offsets array.  We used to cast the object to
     its TYPE_VPTR_BASETYPE, and reference the vtable as TYPE_VPTR_FIELDNO;
     however, that cast can not be done without calling baseclass_offset again
     if the TYPE_VPTR_BASETYPE is a virtual base class, as described in the
     v3 C++ ABI Section 2.4.I.2.b.  Fortunately the ABI guarantees that the
     vtable pointer will be located at the beginning of the object, so we can
     bypass the casting.  Verify that the TYPE_VPTR_FIELDNO is in fact at the
     start of whichever baseclass it resides in, as a sanity measure - iff
     we have debugging information for that baseclass.  */

  vbasetype = TYPE_VPTR_BASETYPE (type);
  if (TYPE_VPTR_FIELDNO (vbasetype) < 0)
    fill_in_vptr_fieldno (vbasetype);

  if (TYPE_VPTR_FIELDNO (vbasetype) >= 0
      && TYPE_FIELD_BITPOS (vbasetype, TYPE_VPTR_FIELDNO (vbasetype)) != 0)
    error (_("Illegal vptr offset in class %s"),
	   TYPE_NAME (vbasetype) ? TYPE_NAME (vbasetype) : "<unknown>");

  vtable_address = value_as_address (value_at_lazy (builtin_type_void_data_ptr,
						    address));
  vtable = value_at_lazy (vtable_type,
                          vtable_address - vtable_address_point_offset ());
  offset_val = value_from_longest(builtin_type_int, cur_base_offset);
  vbase_array = value_field (vtable, vtable_field_vcall_and_vbase_offsets);
  base_offset = value_as_long (value_subscript (vbase_array, offset_val));
  return base_offset;
}

/* Locate a virtual method in DOMAIN or its non-virtual base classes
   which has virtual table index VOFFSET.  The method has an associated
   "this" adjustment of ADJUSTMENT bytes.  */

const char *
gnuv3_find_method_in (struct type *domain, CORE_ADDR voffset,
		      LONGEST adjustment)
{
  int i;
  const char *physname;

  /* Search this class first.  */
  physname = NULL;
  if (adjustment == 0)
    {
      int len;

      len = TYPE_NFN_FIELDS (domain);
      for (i = 0; i < len; i++)
	{
	  int len2, j;
	  struct fn_field *f;

	  f = TYPE_FN_FIELDLIST1 (domain, i);
	  len2 = TYPE_FN_FIELDLIST_LENGTH (domain, i);

	  check_stub_method_group (domain, i);
	  for (j = 0; j < len2; j++)
	    if (TYPE_FN_FIELD_VOFFSET (f, j) == voffset)
	      return TYPE_FN_FIELD_PHYSNAME (f, j);
	}
    }

  /* Next search non-virtual bases.  If it's in a virtual base,
     we're out of luck.  */
  for (i = 0; i < TYPE_N_BASECLASSES (domain); i++)
    {
      int pos;
      struct type *basetype;

      if (BASETYPE_VIA_VIRTUAL (domain, i))
	continue;

      pos = TYPE_BASECLASS_BITPOS (domain, i) / 8;
      basetype = TYPE_FIELD_TYPE (domain, i);
      /* Recurse with a modified adjustment.  We don't need to adjust
	 voffset.  */
      if (adjustment >= pos && adjustment < pos + TYPE_LENGTH (basetype))
	return gnuv3_find_method_in (basetype, voffset, adjustment - pos);
    }

  return NULL;
}

/* GNU v3 implementation of cplus_print_method_ptr.  */

static void
gnuv3_print_method_ptr (const gdb_byte *contents,
			struct type *type,
			struct ui_file *stream)
{
  CORE_ADDR ptr_value;
  LONGEST adjustment;
  struct type *domain;
  int vbit;

  domain = TYPE_DOMAIN_TYPE (type);

  /* Extract the pointer to member.  */
  ptr_value = extract_typed_address (contents, builtin_type_void_func_ptr);
  contents += TYPE_LENGTH (builtin_type_void_func_ptr);
  adjustment = extract_signed_integer (contents,
				       TYPE_LENGTH (builtin_type_long));

  if (!gdbarch_vbit_in_delta (current_gdbarch))
    {
      vbit = ptr_value & 1;
      ptr_value = ptr_value ^ vbit;
    }
  else
    {
      vbit = adjustment & 1;
      adjustment = adjustment >> 1;
    }

  /* Check for NULL.  */
  if (ptr_value == 0 && vbit == 0)
    {
      fprintf_filtered (stream, "NULL");
      return;
    }

  /* Search for a virtual method.  */
  if (vbit)
    {
      CORE_ADDR voffset;
      const char *physname;

      /* It's a virtual table offset, maybe in this class.  Search
	 for a field with the correct vtable offset.  First convert it
	 to an index, as used in TYPE_FN_FIELD_VOFFSET.  */
      voffset = ptr_value / TYPE_LENGTH (builtin_type_long);

      physname = gnuv3_find_method_in (domain, voffset, adjustment);

      /* If we found a method, print that.  We don't bother to disambiguate
	 possible paths to the method based on the adjustment.  */
      if (physname)
	{
	  char *demangled_name = cplus_demangle (physname,
						 DMGL_ANSI | DMGL_PARAMS);
	  if (demangled_name != NULL)
	    {
	      fprintf_filtered (stream, "&virtual ");
	      fputs_filtered (demangled_name, stream);
	      xfree (demangled_name);
	      return;
	    }
	}
    }

  /* We didn't find it; print the raw data.  */
  if (vbit)
    {
      fprintf_filtered (stream, "&virtual table offset ");
      print_longest (stream, 'd', 1, ptr_value);
    }
  else
    print_address_demangle (ptr_value, stream, demangle);

  if (adjustment)
    {
      fprintf_filtered (stream, ", this adjustment ");
      print_longest (stream, 'd', 1, adjustment);
    }
}

/* GNU v3 implementation of cplus_method_ptr_size.  */

static int
gnuv3_method_ptr_size (void)
{
  return 2 * TYPE_LENGTH (builtin_type_void_data_ptr);
}

/* GNU v3 implementation of cplus_make_method_ptr.  */

static void
gnuv3_make_method_ptr (gdb_byte *contents, CORE_ADDR value, int is_virtual)
{
  int size = TYPE_LENGTH (builtin_type_void_data_ptr);

  /* FIXME drow/2006-12-24: The adjustment of "this" is currently
     always zero, since the method pointer is of the correct type.
     But if the method pointer came from a base class, this is
     incorrect - it should be the offset to the base.  The best
     fix might be to create the pointer to member pointing at the
     base class and cast it to the derived class, but that requires
     support for adjusting pointers to members when casting them -
     not currently supported by GDB.  */

  if (!gdbarch_vbit_in_delta (current_gdbarch))
    {
      store_unsigned_integer (contents, size, value | is_virtual);
      store_unsigned_integer (contents + size, size, 0);
    }
  else
    {
      store_unsigned_integer (contents, size, value);
      store_unsigned_integer (contents + size, size, is_virtual);
    }
}

/* GNU v3 implementation of cplus_method_ptr_to_value.  */

static struct value *
gnuv3_method_ptr_to_value (struct value **this_p, struct value *method_ptr)
{
  const gdb_byte *contents = value_contents (method_ptr);
  CORE_ADDR ptr_value;
  struct type *final_type, *method_type;
  LONGEST adjustment;
  struct value *adjval;
  int vbit;

  final_type = TYPE_DOMAIN_TYPE (check_typedef (value_type (method_ptr)));
  final_type = lookup_pointer_type (final_type);

  method_type = TYPE_TARGET_TYPE (check_typedef (value_type (method_ptr)));

  ptr_value = extract_typed_address (contents, builtin_type_void_func_ptr);
  contents += TYPE_LENGTH (builtin_type_void_func_ptr);
  adjustment = extract_signed_integer (contents,
				       TYPE_LENGTH (builtin_type_long));

  if (!gdbarch_vbit_in_delta (current_gdbarch))
    {
      vbit = ptr_value & 1;
      ptr_value = ptr_value ^ vbit;
    }
  else
    {
      vbit = adjustment & 1;
      adjustment = adjustment >> 1;
    }

  /* First convert THIS to match the containing type of the pointer to
     member.  This cast may adjust the value of THIS.  */
  *this_p = value_cast (final_type, *this_p);

  /* Then apply whatever adjustment is necessary.  This creates a somewhat
     strange pointer: it claims to have type FINAL_TYPE, but in fact it
     might not be a valid FINAL_TYPE.  For instance, it might be a
     base class of FINAL_TYPE.  And if it's not the primary base class,
     then printing it out as a FINAL_TYPE object would produce some pretty
     garbage.

     But we don't really know the type of the first argument in
     METHOD_TYPE either, which is why this happens.  We can't
     dereference this later as a FINAL_TYPE, but once we arrive in the
     called method we'll have debugging information for the type of
     "this" - and that'll match the value we produce here.

     You can provoke this case by casting a Base::* to a Derived::*, for
     instance.  */
  *this_p = value_cast (builtin_type_void_data_ptr, *this_p);
  adjval = value_from_longest (builtin_type_long, adjustment);
  *this_p = value_add (*this_p, adjval);
  *this_p = value_cast (final_type, *this_p);

  if (vbit)
    {
      LONGEST voffset = ptr_value / TYPE_LENGTH (builtin_type_long);
      return gnuv3_get_virtual_fn (value_ind (*this_p), method_type, voffset);
    }
  else
    return value_from_pointer (lookup_pointer_type (method_type), ptr_value);
}

/* Determine if we are currently in a C++ thunk.  If so, get the address
   of the routine we are thunking to and continue to there instead.  */

static CORE_ADDR 
gnuv3_skip_trampoline (struct frame_info *frame, CORE_ADDR stop_pc)
{
  CORE_ADDR real_stop_pc, method_stop_pc;
  struct minimal_symbol *thunk_sym, *fn_sym;
  struct obj_section *section;
  char *thunk_name, *fn_name;
  
  real_stop_pc = gdbarch_skip_trampoline_code
		   (current_gdbarch, frame, stop_pc);
  if (real_stop_pc == 0)
    real_stop_pc = stop_pc;

  /* Find the linker symbol for this potential thunk.  */
  thunk_sym = lookup_minimal_symbol_by_pc (real_stop_pc);
  section = find_pc_section (real_stop_pc);
  if (thunk_sym == NULL || section == NULL)
    return 0;

  /* The symbol's demangled name should be something like "virtual
     thunk to FUNCTION", where FUNCTION is the name of the function
     being thunked to.  */
  thunk_name = SYMBOL_DEMANGLED_NAME (thunk_sym);
  if (thunk_name == NULL || strstr (thunk_name, " thunk to ") == NULL)
    return 0;

  fn_name = strstr (thunk_name, " thunk to ") + strlen (" thunk to ");
  fn_sym = lookup_minimal_symbol (fn_name, NULL, section->objfile);
  if (fn_sym == NULL)
    return 0;

  method_stop_pc = SYMBOL_VALUE_ADDRESS (fn_sym);
  real_stop_pc = gdbarch_skip_trampoline_code
		   (current_gdbarch, frame, method_stop_pc);
  if (real_stop_pc == 0)
    real_stop_pc = method_stop_pc;

  return real_stop_pc;
}

static void
init_gnuv3_ops (void)
{
  vtable_type_gdbarch_data = gdbarch_data_register_post_init (build_gdb_vtable_type);

  gnu_v3_abi_ops.shortname = "gnu-v3";
  gnu_v3_abi_ops.longname = "GNU G++ Version 3 ABI";
  gnu_v3_abi_ops.doc = "G++ Version 3 ABI";
  gnu_v3_abi_ops.is_destructor_name =
    (enum dtor_kinds (*) (const char *))is_gnu_v3_mangled_dtor;
  gnu_v3_abi_ops.is_constructor_name =
    (enum ctor_kinds (*) (const char *))is_gnu_v3_mangled_ctor;
  gnu_v3_abi_ops.is_vtable_name = gnuv3_is_vtable_name;
  gnu_v3_abi_ops.is_operator_name = gnuv3_is_operator_name;
  gnu_v3_abi_ops.rtti_type = gnuv3_rtti_type;
  gnu_v3_abi_ops.virtual_fn_field = gnuv3_virtual_fn_field;
  gnu_v3_abi_ops.baseclass_offset = gnuv3_baseclass_offset;
  gnu_v3_abi_ops.print_method_ptr = gnuv3_print_method_ptr;
  gnu_v3_abi_ops.method_ptr_size = gnuv3_method_ptr_size;
  gnu_v3_abi_ops.make_method_ptr = gnuv3_make_method_ptr;
  gnu_v3_abi_ops.method_ptr_to_value = gnuv3_method_ptr_to_value;
  gnu_v3_abi_ops.skip_trampoline = gnuv3_skip_trampoline;
}

extern initialize_file_ftype _initialize_gnu_v3_abi; /* -Wmissing-prototypes */

void
_initialize_gnu_v3_abi (void)
{
  init_gnuv3_ops ();

  register_cp_abi (&gnu_v3_abi_ops);
}
