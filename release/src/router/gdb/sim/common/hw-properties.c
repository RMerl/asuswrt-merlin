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

#include "hw-main.h"
#include "hw-base.h"

#include "sim-io.h"
#include "sim-assert.h"

#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif

#define TRACE(A,B)

/* property entries */

struct hw_property_data {
  struct hw_property_data *next;
  struct hw_property *property;
  const void *init_array;
  unsigned sizeof_init_array;
};

void
create_hw_property_data (struct hw *me)
{
}

void
delete_hw_property_data (struct hw *me)
{
}


/* Device Properties: */

static struct hw_property_data *
find_property_data (struct hw *me,
		    const char *property)
{
  struct hw_property_data *entry;
  ASSERT (property != NULL);
  entry = me->properties_of_hw;
  while (entry != NULL)
    {
      if (strcmp (entry->property->name, property) == 0)
	return entry;
      entry = entry->next;
    }
  return NULL;
}


static void
hw_add_property (struct hw *me,
		 const char *property,
		 hw_property_type type,
		 const void *init_array,
		 unsigned sizeof_init_array,
		 const void *array,
		 unsigned sizeof_array,
		 const struct hw_property *original,
		 object_disposition disposition)
{
  struct hw_property_data *new_entry = NULL;
  struct hw_property *new_value = NULL;
  
  /* find the list end */
  struct hw_property_data **insertion_point = &me->properties_of_hw;
  while (*insertion_point != NULL)
    {
      if (strcmp ((*insertion_point)->property->name, property) == 0)
	return;
      insertion_point = &(*insertion_point)->next;
    }
  
  /* create a new value */
  new_value = HW_ZALLOC (me, struct hw_property);
  new_value->name = (char *) strdup (property);
  new_value->type = type;
  if (sizeof_array > 0)
    {
      void *new_array = hw_zalloc (me, sizeof_array);
      memcpy (new_array, array, sizeof_array);
      new_value->array = new_array;
      new_value->sizeof_array = sizeof_array;
    }
  new_value->owner = me;
  new_value->original = original;
  new_value->disposition = disposition;
  
  /* insert the value into the list */
  new_entry = HW_ZALLOC (me, struct hw_property_data);
  *insertion_point = new_entry;
  if (sizeof_init_array > 0)
    {
      void *new_init_array = hw_zalloc (me, sizeof_init_array);
      memcpy (new_init_array, init_array, sizeof_init_array);
      new_entry->init_array = new_init_array;
      new_entry->sizeof_init_array = sizeof_init_array;
    }
  new_entry->property = new_value;
}


static void
hw_set_property (struct hw *me,
		 const char *property,
		 hw_property_type type,
		 const void *array,
		 int sizeof_array)
{
  /* find the property */
  struct hw_property_data *entry = find_property_data (me, property);
  if (entry != NULL)
    {
      /* existing property - update it */
      void *new_array = 0;
      struct hw_property *value = entry->property;
      /* check the type matches */
      if (value->type != type)
	hw_abort (me, "conflict between type of new and old value for property %s", property);
      /* replace its value */
      if (value->array != NULL)
	hw_free (me, (void*)value->array);
      new_array = (sizeof_array > 0
		   ? hw_zalloc (me, sizeof_array)
		   : (void*)0);
      value->array = new_array;
      value->sizeof_array = sizeof_array;
      if (sizeof_array > 0)
	memcpy (new_array, array, sizeof_array);
      return;
    }
  else
    {
      /* new property - create it */
      hw_add_property (me, property, type,
		       NULL, 0, array, sizeof_array,
		       NULL, temporary_object);
    }
}


#if 0
static void
clean_hw_properties (struct hw *me)
{
  struct hw_property_data **delete_point = &me->properties_of_hw;
  while (*delete_point != NULL)
    {
      struct hw_property_data *current = *delete_point;
      switch (current->property->disposition)
	{
	case permenant_object:
	  /* zap the current value, will be initialized later */
	  ASSERT (current->init_array != NULL);
	  if (current->property->array != NULL)
	    {
	      hw_free (me, (void*)current->property->array);
	      current->property->array = NULL;
	    }
	  delete_point = &(*delete_point)->next;
	  break;
	case temporary_object:
	  /* zap the actual property, was created during simulation run */
	  ASSERT (current->init_array == NULL);
	  *delete_point = current->next;
	  if (current->property->array != NULL)
	    hw_free (me, (void*)current->property->array);
	  hw_free (me, current->property);
	  hw_free (me, current);
	  break;
	}
    }
}
#endif

#if 0
void
hw_init_static_properties (SIM_DESC sd,
			   struct hw *me,
			   void *data)
{
  struct hw_property_data *property;
  for (property = me->properties_of_hw;
       property != NULL;
       property = property->next)
    {
      ASSERT (property->init_array != NULL);
      ASSERT (property->property->array == NULL);
      ASSERT(property->property->disposition == permenant_object);
      switch (property->property->type)
	{
	case array_property:
	case boolean_property:
	case range_array_property:
	case reg_array_property:
	case string_property:
	case string_array_property:
	case integer_property:
	  /* delete the property, and replace it with the original */
	  hw_set_property (me, property->property->name,
			   property->property->type,
			   property->init_array,
			   property->sizeof_init_array);
	  break;
#if 0
	case ihandle_property:
	  break;
#endif
	}
    }
}
#endif


#if 0
void
hw_init_runtime_properties (SIM_DESC sd,
			    struct hw *me,
			    void *data)
{
  struct hw_property_data *property;
  for (property = me->properties_of_hw;
       property != NULL;
       property = property->next)
    {
      switch (property->property->disposition)
	{
	case permenant_object:
	  switch (property->property->type)
	    {
#if 0
	    case ihandle_property:
	      {
		struct hw_instance *ihandle;
		ihandle_runtime_property_spec spec;
		ASSERT (property->init_array != NULL);
		ASSERT (property->property->array == NULL);
		hw_find_ihandle_runtime_property (me, property->property->name, &spec);
		ihandle = tree_instance (me, spec.full_path);
		hw_set_ihandle_property (me, property->property->name, ihandle);
		break;
	      }
#endif
	    case array_property:
	    case boolean_property:
	    case range_array_property:
	    case integer_property:
	    case reg_array_property:
	    case string_property:
	    case string_array_property:
	      ASSERT (property->init_array != NULL);
	      ASSERT (property->property->array != NULL);
	      break;
	    }
	  break;
	case temporary_object:
	  ASSERT (property->init_array == NULL);
	  ASSERT (property->property->array != NULL);
	  break;
	}
    }
}
#endif



const struct hw_property *
hw_next_property (const struct hw_property *property)
{
  /* find the property in the list */
  struct hw *owner = property->owner;
  struct hw_property_data *entry = owner->properties_of_hw;
  while (entry != NULL && entry->property != property)
    entry = entry->next;
  /* now return the following property */
  ASSERT (entry != NULL); /* must be a member! */
  if (entry->next != NULL)
    return entry->next->property;
  else
    return NULL;
}


const struct hw_property *
hw_find_property (struct hw *me,
		  const char *property)
{
  if (me == NULL)
    {
      return NULL;
    }
  else if (property == NULL || strcmp (property, "") == 0)
    {
      if (me->properties_of_hw == NULL)
	return NULL;
      else
	return me->properties_of_hw->property;
    }
  else
    {
      struct hw_property_data *entry = find_property_data (me, property);
      if (entry != NULL)
	return entry->property;
    }
  return NULL;
}


void
hw_add_array_property (struct hw *me,
		       const char *property,
		       const void *array,
		       int sizeof_array)
{
  hw_add_property (me, property, array_property,
		   array, sizeof_array, array, sizeof_array,
		   NULL, permenant_object);
}

void
hw_set_array_property (struct hw *me,
		       const char *property,
		       const void *array,
		       int sizeof_array)
{
  hw_set_property (me, property, array_property, array, sizeof_array);
}

const struct hw_property *
hw_find_array_property (struct hw *me,
			const char *property)
{
  const struct hw_property *node;
  node = hw_find_property (me, property);
  if (node == NULL)
    hw_abort (me, "property \"%s\" not found", property);
  if (node->type != array_property)
    hw_abort (me, "property \"%s\" of wrong type (array)", property);
  return node;
}



void
hw_add_boolean_property (struct hw *me,
			 const char *property,
			 int boolean)
{
  signed32 new_boolean = (boolean ? -1 : 0);
  hw_add_property (me, property, boolean_property,
		   &new_boolean, sizeof(new_boolean),
		   &new_boolean, sizeof(new_boolean),
		   NULL, permenant_object);
}

int
hw_find_boolean_property (struct hw *me,
			  const char *property)
{
  const struct hw_property *node;
  unsigned_cell boolean;
  node = hw_find_property (me, property);
  if (node == NULL)
    hw_abort (me, "property \"%s\" not found", property);
  if (node->type != boolean_property)
    hw_abort (me, "property \"%s\" of wrong type (boolean)", property);
  ASSERT (sizeof (boolean) == node->sizeof_array);
  memcpy (&boolean, node->array, sizeof (boolean));
  return boolean;
}



#if 0
void
hw_add_ihandle_runtime_property (struct hw *me,
				 const char *property,
				 const ihandle_runtime_property_spec *ihandle)
{
  /* enter the full path as the init array */
  hw_add_property (me, property, ihandle_property,
		   ihandle->full_path, strlen(ihandle->full_path) + 1,
		   NULL, 0,
		   NULL, permenant_object);
}
#endif

#if 0
void
hw_find_ihandle_runtime_property (struct hw *me,
				  const char *property,
				  ihandle_runtime_property_spec *ihandle)
{
  struct hw_property_data *entry = find_property_data (me, property);
  TRACE (trace_devices,
	 ("hw_find_ihandle_runtime_property(me=0x%lx, property=%s)\n",
	  (long)me, property));
  if (entry == NULL)
    hw_abort (me, "property \"%s\" not found", property);
  if (entry->property->type != ihandle_property
      || entry->property->disposition != permenant_object)
    hw_abort (me, "property \"%s\" of wrong type", property);
  ASSERT (entry->init_array != NULL);
  /* the full path */
  ihandle->full_path = entry->init_array;
}
#endif



#if 0
void
hw_set_ihandle_property (struct hw *me,
			 const char *property,
			 hw_instance *ihandle)
{
  unsigned_cell cells;
  cells = H2BE_cell (hw_instance_to_external (ihandle));
  hw_set_property (me, property, ihandle_property,
		   &cells, sizeof (cells));
		      
}
#endif

#if 0
hw_instance *
hw_find_ihandle_property (struct hw *me,
			  const char *property)
{
  const hw_property_data *node;
  unsigned_cell ihandle;
  hw_instance *instance;

  node = hw_find_property (me, property);
  if (node == NULL)
    hw_abort (me, "property \"%s\" not found", property);
  if (node->type != ihandle_property)
    hw_abort(me, "property \"%s\" of wrong type (ihandle)", property);
  if (node->array == NULL)
    hw_abort(me, "runtime property \"%s\" not yet initialized", property);

  ASSERT (sizeof(ihandle) == node->sizeof_array);
  memcpy (&ihandle, node->array, sizeof(ihandle));
  instance = external_to_hw_instance (me, BE2H_cell(ihandle));
  ASSERT (instance != NULL);
  return instance;
}
#endif


void
hw_add_integer_property (struct hw *me,
			 const char *property,
			 signed_cell integer)
{
  H2BE (integer);
  hw_add_property (me, property, integer_property,
		   &integer, sizeof(integer),
		   &integer, sizeof(integer),
		   NULL, permenant_object);
}

signed_cell
hw_find_integer_property (struct hw *me,
			  const char *property)
{
  const struct hw_property *node;
  signed_cell integer;
  TRACE (trace_devices,
	 ("hw_find_integer(me=0x%lx, property=%s)\n",
	  (long)me, property));
  node = hw_find_property (me, property);
  if (node == NULL)
    hw_abort (me, "property \"%s\" not found", property);
  if (node->type != integer_property)
    hw_abort (me, "property \"%s\" of wrong type (integer)", property);
  ASSERT (sizeof(integer) == node->sizeof_array);
  memcpy (&integer, node->array, sizeof (integer));
  return BE2H_cell (integer);
}

int
hw_find_integer_array_property (struct hw *me,
				const char *property,
				unsigned index,
				signed_cell *integer)
{
  const struct hw_property *node;
  int sizeof_integer = sizeof (*integer);
  signed_cell *cell;
  TRACE (trace_devices,
	 ("hw_find_integer(me=0x%lx, property=%s)\n",
	  (long)me, property));
  
  /* check things sane */
  node = hw_find_property (me, property);
  if (node == NULL)
    hw_abort (me, "property \"%s\" not found", property);
  if (node->type != integer_property
      && node->type != array_property)
    hw_abort (me, "property \"%s\" of wrong type (integer or array)", property);
  if ((node->sizeof_array % sizeof_integer) != 0)
    hw_abort (me, "property \"%s\" contains an incomplete number of cells", property);
  if (node->sizeof_array <= sizeof_integer * index)
    return 0;
  
  /* Find and convert the value */
  cell = ((signed_cell*)node->array) + index;
  *integer = BE2H_cell (*cell);
  
  return node->sizeof_array / sizeof_integer;
}


static unsigned_cell *
unit_address_to_cells (const hw_unit *unit,
		       unsigned_cell *cell,
		       int nr_cells)
{
  int i;
  ASSERT(nr_cells == unit->nr_cells);
  for (i = 0; i < unit->nr_cells; i++)
    {
      *cell = H2BE_cell (unit->cells[i]);
      cell += 1;
    }
  return cell;
}


static const unsigned_cell *
cells_to_unit_address (const unsigned_cell *cell,
		       hw_unit *unit,
		       int nr_cells)
{
  int i;
  memset(unit, 0, sizeof(*unit));
  unit->nr_cells = nr_cells;
  for (i = 0; i < unit->nr_cells; i++)
    {
      unit->cells[i] = BE2H_cell (*cell);
      cell += 1;
    }
  return cell;
}


static unsigned
nr_range_property_cells (struct hw *me,
			 int nr_ranges)
{
  return ((hw_unit_nr_address_cells (me)
	   + hw_unit_nr_address_cells (hw_parent (me))
	   + hw_unit_nr_size_cells (me))
	  ) * nr_ranges;
}

void
hw_add_range_array_property (struct hw *me,
			     const char *property,
			     const range_property_spec *ranges,
			     unsigned nr_ranges)
{
  unsigned sizeof_cells = (nr_range_property_cells (me, nr_ranges)
			   * sizeof (unsigned_cell));
  unsigned_cell *cells = hw_zalloc (me, sizeof_cells);
  unsigned_cell *cell;
  int i;
  
  /* copy the property elements over */
  cell = cells;
  for (i = 0; i < nr_ranges; i++)
    {
      const range_property_spec *range = &ranges[i];
      /* copy the child address */
      cell = unit_address_to_cells (&range->child_address, cell,
				    hw_unit_nr_address_cells (me));
      /* copy the parent address */
      cell = unit_address_to_cells (&range->parent_address, cell, 
				    hw_unit_nr_address_cells (hw_parent (me)));
      /* copy the size */
      cell = unit_address_to_cells (&range->size, cell, 
				    hw_unit_nr_size_cells (me));
    }
  ASSERT (cell == &cells[nr_range_property_cells (me, nr_ranges)]);
  
  /* add it */
  hw_add_property (me, property, range_array_property,
		   cells, sizeof_cells,
		   cells, sizeof_cells,
		   NULL, permenant_object);
  
  hw_free (me, cells);
}

int
hw_find_range_array_property (struct hw *me,
			      const char *property,
			      unsigned index,
			      range_property_spec *range)
{
  const struct hw_property *node;
  unsigned sizeof_entry = (nr_range_property_cells (me, 1)
			   * sizeof (unsigned_cell));
  const unsigned_cell *cells;
  
  /* locate the property */
  node = hw_find_property (me, property);
  if (node == NULL)
    hw_abort (me, "property \"%s\" not found", property);
  if (node->type != range_array_property)
    hw_abort (me, "property \"%s\" of wrong type (range array)", property);
  
  /* aligned ? */
  if ((node->sizeof_array % sizeof_entry) != 0)
    hw_abort (me, "property \"%s\" contains an incomplete number of entries",
	      property);
  
  /* within bounds? */
  if (node->sizeof_array < sizeof_entry * (index + 1))
    return 0;
  
  /* find the range of interest */
  cells = (unsigned_cell*)((char*)node->array + sizeof_entry * index);
  
  /* copy the child address out - converting as we go */
  cells = cells_to_unit_address (cells, &range->child_address,
				 hw_unit_nr_address_cells (me));
  
  /* copy the parent address out - converting as we go */
  cells = cells_to_unit_address (cells, &range->parent_address,
				 hw_unit_nr_address_cells (hw_parent (me)));
  
  /* copy the size - converting as we go */
  cells = cells_to_unit_address (cells, &range->size,
				 hw_unit_nr_size_cells (me));
  
  return node->sizeof_array / sizeof_entry;
}


static unsigned
nr_reg_property_cells (struct hw *me,
		       int nr_regs)
{
  return (hw_unit_nr_address_cells (hw_parent(me))
	  + hw_unit_nr_size_cells (hw_parent(me))
	  ) * nr_regs;
}

void
hw_add_reg_array_property (struct hw *me,
			   const char *property,
			   const reg_property_spec *regs,
			   unsigned nr_regs)
{
  unsigned sizeof_cells = (nr_reg_property_cells (me, nr_regs)
			   * sizeof (unsigned_cell));
  unsigned_cell *cells = hw_zalloc (me, sizeof_cells);
  unsigned_cell *cell;
  int i;
  
  /* copy the property elements over */
  cell = cells;
  for (i = 0; i < nr_regs; i++)
    {
      const reg_property_spec *reg = &regs[i];
      /* copy the address */
      cell = unit_address_to_cells (&reg->address, cell,
				    hw_unit_nr_address_cells (hw_parent (me)));
      /* copy the size */
      cell = unit_address_to_cells (&reg->size, cell,
				    hw_unit_nr_size_cells (hw_parent (me)));
    }
  ASSERT (cell == &cells[nr_reg_property_cells (me, nr_regs)]);
  
  /* add it */
  hw_add_property (me, property, reg_array_property,
		   cells, sizeof_cells,
		   cells, sizeof_cells,
		   NULL, permenant_object);
  
  hw_free (me, cells);
}

int
hw_find_reg_array_property (struct hw *me,
			    const char *property,
			    unsigned index,
			    reg_property_spec *reg)
{
  const struct hw_property *node;
  unsigned sizeof_entry = (nr_reg_property_cells (me, 1)
			   * sizeof (unsigned_cell));
  const unsigned_cell *cells;
  
  /* locate the property */
  node = hw_find_property (me, property);
  if (node == NULL)
    hw_abort (me, "property \"%s\" not found", property);
  if (node->type != reg_array_property)
    hw_abort (me, "property \"%s\" of wrong type (reg array)", property);
  
  /* aligned ? */
  if ((node->sizeof_array % sizeof_entry) != 0)
    hw_abort (me, "property \"%s\" contains an incomplete number of entries",
	      property);
  
  /* within bounds? */
  if (node->sizeof_array < sizeof_entry * (index + 1))
    return 0;
  
  /* find the range of interest */
  cells = (unsigned_cell*)((char*)node->array + sizeof_entry * index);
  
  /* copy the address out - converting as we go */
  cells = cells_to_unit_address (cells, &reg->address,
				 hw_unit_nr_address_cells (hw_parent (me)));
  
  /* copy the size out - converting as we go */
  cells = cells_to_unit_address (cells, &reg->size,
				 hw_unit_nr_size_cells (hw_parent (me)));
  
  return node->sizeof_array / sizeof_entry;
}


void
hw_add_string_property (struct hw *me,
			const char *property,
			const char *string)
{
  hw_add_property (me, property, string_property,
		   string, strlen(string) + 1,
		   string, strlen(string) + 1,
		   NULL, permenant_object);
}

const char *
hw_find_string_property (struct hw *me,
			 const char *property)
{
  const struct hw_property *node;
  const char *string;
  node = hw_find_property (me, property);
  if (node == NULL)
    hw_abort (me, "property \"%s\" not found", property);
  if (node->type != string_property)
    hw_abort (me, "property \"%s\" of wrong type (string)", property);
  string = node->array;
  ASSERT (strlen(string) + 1 == node->sizeof_array);
  return string;
}

void
hw_add_string_array_property (struct hw *me,
			      const char *property,
			      const string_property_spec *strings,
			      unsigned nr_strings)
{
  int sizeof_array;
  int string_nr;
  char *array;
  char *chp;
  if (nr_strings == 0)
    hw_abort (me, "property \"%s\" must be non-null", property);
  /* total up the size of the needed array */
  for (sizeof_array = 0, string_nr = 0;
       string_nr < nr_strings;
       string_nr ++)
    {
      sizeof_array += strlen (strings[string_nr]) + 1;
    }
  /* create the array */
  array = (char*) hw_zalloc (me, sizeof_array);
  chp = array;
  for (string_nr = 0;
       string_nr < nr_strings;
       string_nr++)
    {
      strcpy (chp, strings[string_nr]);
      chp += strlen (chp) + 1;
    }
  ASSERT (chp == array + sizeof_array);
  /* now enter it */
  hw_add_property (me, property, string_array_property,
		   array, sizeof_array,
		   array, sizeof_array,
		   NULL, permenant_object);
}

int
hw_find_string_array_property (struct hw *me,
			       const char *property,
			       unsigned index,
			       string_property_spec *string)
{
  const struct hw_property *node;
  node = hw_find_property (me, property);
  if (node == NULL)
    hw_abort (me, "property \"%s\" not found", property);
  switch (node->type)
    {
    default:
      hw_abort (me, "property \"%s\" of wrong type", property);
      break;
    case string_property:
      if (index == 0)
	{
	  *string = node->array;
	  ASSERT (strlen(*string) + 1 == node->sizeof_array);
	  return 1;
	}
      break;
    case array_property:
      if (node->sizeof_array == 0
	  || ((char*)node->array)[node->sizeof_array - 1] != '\0')
	hw_abort (me, "property \"%s\" invalid for string array", property);
      /* FALL THROUGH */
    case string_array_property:
      ASSERT (node->sizeof_array > 0);
      ASSERT (((char*)node->array)[node->sizeof_array - 1] == '\0');
      {
	const char *chp = node->array;
	int nr_entries = 0;
	/* count the number of strings, keeping an eye out for the one
	   we're looking for */
	*string = chp;
	do
	  {
	    if (*chp == '\0')
	      {
		/* next string */
		nr_entries++;
		chp++;
		if (nr_entries == index)
		  *string = chp;
	      }
	    else
	      {
		chp++;
	      }
	  } while (chp < (char*)node->array + node->sizeof_array);
	if (index < nr_entries)
	  return nr_entries;
	else
	  {
	    *string = NULL;
	    return 0;
	  }
      }
      break;
    }
  return 0;
}

void
hw_add_duplicate_property (struct hw *me,
			   const char *property,
			   const struct hw_property *original)
{
  struct hw_property_data *master;
  TRACE (trace_devices,
	 ("hw_add_duplicate_property(me=0x%lx, property=%s, ...)\n",
	  (long)me, property));
  if (original->disposition != permenant_object)
    hw_abort (me, "Can only duplicate permenant objects");
  /* find the original's master */
  master = original->owner->properties_of_hw;
  while (master->property != original)
    {
      master = master->next;
      ASSERT(master != NULL);
    }
  /* now duplicate it */
  hw_add_property (me, property,
		   original->type,
		   master->init_array, master->sizeof_init_array,
		   original->array, original->sizeof_array,
		   original, permenant_object);
}
