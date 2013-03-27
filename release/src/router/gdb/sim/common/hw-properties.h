/* The common simulator framework for GDB, the GNU Debugger.

   Copyright 2002, 2007 Free Software Foundation, Inc.

   Contributed by Andrew Cagney and Red Hat.

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


#ifndef HW_PROPERTIES_H
#define HW_PROPERTIES_H

/* The following are valid property types.  The property `array' is
   for generic untyped data. */

typedef enum {
  array_property,
  boolean_property,
#if 0
  ihandle_property, /*runtime*/
#endif
  integer_property,
  range_array_property,
  reg_array_property,
  string_property,
  string_array_property,
} hw_property_type;

struct hw_property {
  struct hw *owner;
  const char *name;
  hw_property_type type;
  unsigned sizeof_array;
  const void *array;
  const struct hw_property *original;
  object_disposition disposition;
};

#define hw_property_owner(p) ((p)->owner + 0)
#define hw_property_name(p) ((p)->name + 0)
#define hw_property_type(p) ((p)->type + 0)
#define hw_property_array(p) ((p)->array + 0)
#define hw_property_sizeof_array(p) ((p)->sizeof_array + 0)
#define hw_property_original(p) ((p)->original + 0)
#define hw_property_disposition(p) ((p)->disposition + 0)


/* Find/iterate over properites attached to a device.

   To iterate over all properties attached to a device, call
   hw_find_property (.., NULL) and then hw_property_next. */

const struct hw_property *hw_find_property
(struct hw *me,
 const char *property);

const struct hw_property *hw_next_property
(const struct hw_property *previous);


/* Manipulate the properties belonging to a given device.

   HW_ADD_* will, if the property is not already present, add a
   property to the device.  Adding a property to a device after it has
   been created is a checked run-time error (use HW_SET_*).

   HW_SET_* will always update (or create) the property so that it has
   the specified value.  Changing the type of a property is a checked
   run-time error.

   FIND returns the specified properties value.  It is a checked
   runtime error to either request a nonexistant property or to
   request a property using the wrong type. Code locating a property
   should first check its type (using hw_find_property above) and then
   obtain its value using the below.

   Naming convention:

   void hw_add_<type>_property(struct hw *, const char *, <type>)
   void hw_add_*_array_property(struct hw *, const char *, const <type>*, int)
   void hw_set_*_property(struct hw *, const char *, <type>)
   void hw_set_*_array_property(struct hw *, const char *, const <type>*, int)
   <type> hw_find_*_property(struct hw *, const char *)
   int hw_find_*_array_property(struct hw *, const char *, int, <type>*)

   */


void hw_add_array_property
(struct hw *me,
 const char *property,
 const void *array,
 int sizeof_array);

void hw_set_array_property
(struct hw *me,
 const char *property,
 const void *array,
 int sizeof_array);

const struct hw_property *hw_find_array_property
(struct hw *me,
 const char *property);



void hw_add_boolean_property
(struct hw *me,
 const char *property,
 int bool);

int hw_find_boolean_property
(struct hw *me,
 const char *property);



#if 0
typedef struct _ihandle_runtime_property_spec {
  const char *full_path;
} ihandle_runtime_property_spec;

void hw_add_ihandle_runtime_property
(struct hw *me,
 const char *property,
 const ihandle_runtime_property_spec *ihandle);

void hw_find_ihandle_runtime_property
(struct hw *me,
 const char *property,
 ihandle_runtime_property_spec *ihandle);

void hw_set_ihandle_property
(struct hw *me,
 const char *property,
 hw_instance *ihandle);

hw_instance * hw_find_ihandle_property
(struct hw *me,
 const char *property);
#endif


void hw_add_integer_property
(struct hw *me,
 const char *property,
 signed_cell integer);

signed_cell hw_find_integer_property
(struct hw *me,
 const char *property);

int hw_find_integer_array_property
(struct hw *me,
 const char *property,
 unsigned index,
 signed_cell *integer);



typedef struct _range_property_spec {
  hw_unit child_address;
  hw_unit parent_address;
  hw_unit size;
} range_property_spec;

void hw_add_range_array_property
(struct hw *me,
 const char *property,
 const range_property_spec *ranges,
 unsigned nr_ranges);

int hw_find_range_array_property
(struct hw *me,
 const char *property,
 unsigned index,
 range_property_spec *range);



typedef struct _reg_property_spec {
  hw_unit address;
  hw_unit size;
} reg_property_spec;

void hw_add_reg_array_property
(struct hw *me,
 const char *property,
 const reg_property_spec *reg,
 unsigned nr_regs);

int hw_find_reg_array_property
(struct hw *me,
 const char *property,
 unsigned index,
 reg_property_spec *reg);



void hw_add_string_property
(struct hw *me,
 const char *property,
 const char *string);

const char *hw_find_string_property
(struct hw *me,
 const char *property);



typedef const char *string_property_spec;

void hw_add_string_array_property
(struct hw *me,
 const char *property,
 const string_property_spec *strings,
 unsigned nr_strings);

int hw_find_string_array_property
(struct hw *me,
 const char *property,
 unsigned index,
 string_property_spec *string);



void hw_add_duplicate_property
(struct hw *me,
 const char *property,
 const struct hw_property *original);

#endif
