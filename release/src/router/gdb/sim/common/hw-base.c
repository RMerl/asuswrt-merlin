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


#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <ctype.h>

#include "hw-config.h"

struct hw_base_data {
  int finished_p;
  const struct hw_descriptor *descriptor;
  hw_delete_callback *to_delete;
};

static int
generic_hw_unit_decode (struct hw *bus,
			const char *unit,
			hw_unit *phys)
{
  memset (phys, 0, sizeof (*phys));
  if (unit == NULL)
    return 0;
  else
    {
      int nr_cells = 0;
      const int max_nr_cells = hw_unit_nr_address_cells (bus);
      while (1)
	{
	  char *end = NULL;
	  unsigned long val;
	  val = strtoul (unit, &end, 0);
	  /* parse error? */
	  if (unit == end)
	    return -1;
	  /* two many cells? */
	  if (nr_cells >= max_nr_cells)
	    return -1;
	  /* save it */
	  phys->cells[nr_cells] = val;
	  nr_cells++;
	  unit = end;
	  /* more to follow? */
	  if (isspace (*unit) || *unit == '\0')
	    break;
	  if (*unit != ',')
	    return -1;
	  unit++;
	}
      if (nr_cells < max_nr_cells) {
	/* shift everything to correct position */
	int i;
	for (i = 1; i <= nr_cells; i++)
	  phys->cells[max_nr_cells - i] = phys->cells[nr_cells - i];
	for (i = 0; i < (max_nr_cells - nr_cells); i++)
	  phys->cells[i] = 0;
      }
      phys->nr_cells = max_nr_cells;
      return max_nr_cells;
  }
}

static int
generic_hw_unit_encode (struct hw *bus,
			const hw_unit *phys,
			char *buf,
			int sizeof_buf)
{
  int i;
  int len;
  char *pos = buf;
  /* skip leading zero's */
  for (i = 0; i < phys->nr_cells; i++)
    {
      if (phys->cells[i] != 0)
	break;
    }
  /* don't output anything if empty */
  if (phys->nr_cells == 0)
    {
      strcpy(pos, "");
      len = 0;
    }
  else if (i == phys->nr_cells)
    {
      /* all zero */
      strcpy(pos, "0");
      len = 1;
    }
  else
    {
      for (; i < phys->nr_cells; i++)
	{
	  if (pos != buf) {
	    strcat(pos, ",");
	    pos = strchr(pos, '\0');
	  }
	  if (phys->cells[i] < 10)
	    sprintf (pos, "%ld", (unsigned long)phys->cells[i]);
	  else
	    sprintf (pos, "0x%lx", (unsigned long)phys->cells[i]);
	  pos = strchr(pos, '\0');
	}
      len = pos - buf;
    }
  if (len >= sizeof_buf)
    hw_abort (NULL, "generic_unit_encode - buffer overflow\n");
  return len;
}

static int
generic_hw_unit_address_to_attach_address (struct hw *me,
					   const hw_unit *address,
					   int *attach_space,
					   unsigned_word *attach_address,
					   struct hw *client)
{
  int i;
  for (i = 0; i < address->nr_cells - 2; i++)
    {
      if (address->cells[i] != 0)
	hw_abort (me, "Only 32bit addresses supported");
    }
  if (address->nr_cells >= 2)
    *attach_space = address->cells[address->nr_cells - 2];
  else
    *attach_space = 0;
  *attach_address = address->cells[address->nr_cells - 1];
  return 1;
}

static int
generic_hw_unit_size_to_attach_size (struct hw *me,
				     const hw_unit *size,
				     unsigned *nr_bytes,
				     struct hw *client)
{
  int i;
  for (i = 0; i < size->nr_cells - 1; i++)
    {
      if (size->cells[i] != 0)
	hw_abort (me, "Only 32bit sizes supported");
    }
  *nr_bytes = size->cells[0];
  return *nr_bytes;
}


/* ignore/passthrough versions of each function */

static void
passthrough_hw_attach_address (struct hw *me,
			       int level,
			       int space,
			       address_word addr,
			       address_word nr_bytes,
			       struct hw *client) /*callback/default*/
{
  if (hw_parent (me) == NULL)
    hw_abort (client, "hw_attach_address: no parent attach method");
  hw_attach_address (hw_parent (me), level,
		     space, addr, nr_bytes,
		     client);
}

static void
passthrough_hw_detach_address (struct hw *me,
			       int level,
			       int space,
			       address_word addr,
			       address_word nr_bytes,
			       struct hw *client) /*callback/default*/
{
  if (hw_parent (me) == NULL)
    hw_abort (client, "hw_attach_address: no parent attach method");
  hw_detach_address (hw_parent (me), level,
		     space, addr, nr_bytes,
		     client);
}

static unsigned
panic_hw_io_read_buffer (struct hw *me,
			 void *dest,
			 int space,
			 unsigned_word addr,
			 unsigned nr_bytes)
{
  hw_abort (me, "no io-read method");
  return 0;
}

static unsigned
panic_hw_io_write_buffer (struct hw *me,
			  const void *source,
			  int space,
			  unsigned_word addr,
			  unsigned nr_bytes)
{
  hw_abort (me, "no io-write method");
  return 0;
}

static unsigned
passthrough_hw_dma_read_buffer (struct hw *me,
				void *dest,
				int space,
				unsigned_word addr,
				unsigned nr_bytes)
{
  if (hw_parent (me) == NULL)
    hw_abort (me, "no parent dma-read method");
  return hw_dma_read_buffer (hw_parent (me), dest,
			     space, addr, nr_bytes);
}

static unsigned
passthrough_hw_dma_write_buffer (struct hw *me,
				 const void *source,
				 int space,
				 unsigned_word addr,
				 unsigned nr_bytes,
				 int violate_read_only_section)
{
  if (hw_parent (me) == NULL)
    hw_abort (me, "no parent dma-write method");
  return hw_dma_write_buffer (hw_parent (me), source,
			      space, addr,
			      nr_bytes,
			      violate_read_only_section);
}

static void
ignore_hw_delete (struct hw *me)
{
  /* NOP */
}




static const char *
full_name_of_hw (struct hw *leaf,
		 char *buf,
		 unsigned sizeof_buf)
{
  /* get a buffer */
  char full_name[1024];
  if (buf == (char*)0)
    {
      buf = full_name;
      sizeof_buf = sizeof (full_name);
    }

  /* use head recursion to construct the path */

  if (hw_parent (leaf) == NULL)
    /* root */
    {
      if (sizeof_buf < 1)
	hw_abort (leaf, "buffer overflow");
      *buf = '\0';
    }
  else
    /* sub node */
    {
      char unit[1024];
      full_name_of_hw (hw_parent (leaf), buf, sizeof_buf);
      if (hw_unit_encode (hw_parent (leaf),
			  hw_unit_address (leaf),
			  unit + 1,
			  sizeof (unit) - 1)
	  > 0)
	unit[0] = '@';
      else
	unit[0] = '\0';
      if (strlen (buf) + strlen ("/") + strlen (hw_name (leaf)) + strlen (unit)
	  >= sizeof_buf)
	hw_abort (leaf, "buffer overflow");
      strcat (buf, "/");
      strcat (buf, hw_name (leaf));
      strcat (buf, unit);
    }
  
  /* return it usefully */
  if (buf == full_name)
    buf = hw_strdup (leaf, full_name);
  return buf;
}

struct hw *
hw_create (struct sim_state *sd,
	   struct hw *parent,
	   const char *family,
	   const char *name,
	   const char *unit,
	   const char *args)
{
 /* NOTE: HW must be allocated using ZALLOC, others use HW_ZALLOC */
  struct hw *hw = ZALLOC (struct hw);

  /* our identity */
  hw->family_of_hw = hw_strdup (hw, family);
  hw->name_of_hw = hw_strdup (hw, name);
  hw->args_of_hw = hw_strdup (hw, args);

  /* a hook into the system */
  if (sd != NULL)
    hw->system_of_hw = sd;
  else if (parent != NULL)
    hw->system_of_hw = hw_system (parent);
  else
    hw_abort (parent, "No system found");

  /* in a tree */
  if (parent != NULL)
    {
      struct hw **sibling = &parent->child_of_hw;
      while ((*sibling) != NULL)
	sibling = &(*sibling)->sibling_of_hw;
      *sibling = hw;
      hw->parent_of_hw = parent;
    }

  /* top of tree */
  if (parent != NULL)
    {
      struct hw *root = parent;
      while (root->parent_of_hw != NULL)
	root = root->parent_of_hw;
      hw->root_of_hw = root;
    }
  
  /* a unique identifier for the device on the parents bus */
  if (parent != NULL)
    {
      hw_unit_decode (parent, unit, &hw->unit_address_of_hw);
    }

  /* Determine our path */
  if (parent != NULL)
    hw->path_of_hw = full_name_of_hw (hw, NULL, 0);
  else
    hw->path_of_hw = "/";

  /* create our base type */
  hw->base_of_hw = HW_ZALLOC (hw, struct hw_base_data);
  hw->base_of_hw->finished_p = 0;

  /* our callbacks */
  set_hw_io_read_buffer (hw, panic_hw_io_read_buffer);
  set_hw_io_write_buffer (hw, panic_hw_io_write_buffer);
  set_hw_dma_read_buffer (hw, passthrough_hw_dma_read_buffer);
  set_hw_dma_write_buffer (hw, passthrough_hw_dma_write_buffer);
  set_hw_unit_decode (hw, generic_hw_unit_decode);
  set_hw_unit_encode (hw, generic_hw_unit_encode);
  set_hw_unit_address_to_attach_address (hw, generic_hw_unit_address_to_attach_address);
  set_hw_unit_size_to_attach_size (hw, generic_hw_unit_size_to_attach_size);
  set_hw_attach_address (hw, passthrough_hw_attach_address);
  set_hw_detach_address (hw, passthrough_hw_detach_address);
  set_hw_delete (hw, ignore_hw_delete);

  /* locate a descriptor */
  {
    const struct hw_descriptor **table;
    for (table = hw_descriptors;
	 *table != NULL;
	 table++)
      {
	const struct hw_descriptor *entry;
	for (entry = *table;
	     entry->family != NULL;
	     entry++)
	  {
	    if (strcmp (family, entry->family) == 0)
	      {
		hw->base_of_hw->descriptor = entry;
		break;
	      }
	  }
      }
    if (hw->base_of_hw->descriptor == NULL)
      {
	hw_abort (parent, "Unknown device `%s'", family);
      }
  }

  /* Attach dummy ports */
  create_hw_alloc_data (hw);
  create_hw_property_data (hw);
  create_hw_port_data (hw);
  create_hw_event_data (hw);
  create_hw_handle_data (hw);
  create_hw_instance_data (hw);
  
  return hw;
}


int
hw_finished_p (struct hw *me)
{
  return (me->base_of_hw->finished_p);
}

void
hw_finish (struct hw *me)
{
  if (hw_finished_p (me))
    hw_abort (me, "Attempt to finish finished device");

  /* Fill in the (hopefully) defined address/size cells values */
  if (hw_find_property (me, "#address-cells") != NULL)
    me->nr_address_cells_of_hw_unit =
      hw_find_integer_property (me, "#address-cells");
  else
    me->nr_address_cells_of_hw_unit = 2;
  if (hw_find_property (me, "#size-cells") != NULL)
    me->nr_size_cells_of_hw_unit =
      hw_find_integer_property (me, "#size-cells");
  else
    me->nr_size_cells_of_hw_unit = 1;

  /* Fill in the (hopefully) defined trace variable */
  if (hw_find_property (me, "trace?") != NULL)
    me->trace_of_hw_p = hw_find_boolean_property (me, "trace?");
  /* allow global variable to define default tracing */
  else if (! hw_trace_p (me)
	   && hw_find_property (hw_root (me), "global-trace?") != NULL
	   && hw_find_boolean_property (hw_root (me), "global-trace?"))
    me->trace_of_hw_p = 1;
    

  /* Allow the real device to override any methods */
  me->base_of_hw->descriptor->to_finish (me);
  me->base_of_hw->finished_p = 1;
}


void
hw_delete (struct hw *me)
{
  /* give the object a chance to tidy up */
  me->base_of_hw->to_delete (me);

  delete_hw_instance_data (me);
  delete_hw_handle_data (me);
  delete_hw_event_data (me);
  delete_hw_port_data (me);
  delete_hw_property_data (me);

  /* now unlink us from the tree */
  if (hw_parent (me))
    {
      struct hw **sibling = &hw_parent (me)->child_of_hw;
      while (*sibling != NULL)
	{
	  if (*sibling == me)
	    {
	      *sibling = me->sibling_of_hw;
	      me->sibling_of_hw = NULL;
	      me->parent_of_hw = NULL;
	      break;
	    }
	}
    }

  /* some sanity checks */
  if (hw_child (me) != NULL)
    {
      hw_abort (me, "attempt to delete device with children");
    }
  if (hw_sibling (me) != NULL)
    {
      hw_abort (me, "attempt to delete device with siblings");
    }

  /* blow away all memory belonging to the device */
  delete_hw_alloc_data (me);

  /* finally */
  zfree (me);
}

void
set_hw_delete (struct hw *hw, hw_delete_callback method)
{
  hw->base_of_hw->to_delete = method;
}


/* Go through the devices various reg properties for those that
   specify attach addresses */


void
do_hw_attach_regs (struct hw *hw)
{
  static const char *(reg_property_names[]) = {
    "attach-addresses",
    "assigned-addresses",
    "reg",
    "alternate-reg" ,
    NULL
  };
  const char **reg_property_name;
  int nr_valid_reg_properties = 0;
  for (reg_property_name = reg_property_names;
       *reg_property_name != NULL;
       reg_property_name++)
    {
      if (hw_find_property (hw, *reg_property_name) != NULL)
	{
	  reg_property_spec reg;
	  int reg_entry;
	  for (reg_entry = 0;
	       hw_find_reg_array_property (hw, *reg_property_name, reg_entry,
					   &reg);
	       reg_entry++)
	    {
	      unsigned_word attach_address;
	      int attach_space;
	      unsigned attach_size;
	      if (!hw_unit_address_to_attach_address (hw_parent (hw),
						      &reg.address,
						      &attach_space,
						      &attach_address,
						      hw))
		continue;
	      if (!hw_unit_size_to_attach_size (hw_parent (hw),
						&reg.size,
						&attach_size, hw))
		continue;
	      hw_attach_address (hw_parent (hw),
				 0,
				 attach_space, attach_address, attach_size,
				 hw);
	      nr_valid_reg_properties++;
	    }
	  /* if first option matches don't try for any others */
	  if (reg_property_name == reg_property_names)
	    break;
	}
    }
}
