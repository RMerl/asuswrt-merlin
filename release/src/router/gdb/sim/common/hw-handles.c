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


struct hw_handle_mapping {
  cell_word external;
  struct hw *phandle;
  struct hw_instance *ihandle;
  struct hw_handle_mapping *next;
};


struct hw_handle_data {
  int nr_mappings;
  struct hw_handle_mapping *mappings;
};

void
create_hw_handle_data (struct hw *hw)
{
  if (hw_parent (hw) == NULL)
    {
      hw->handles_of_hw = HW_ZALLOC (hw, struct hw_handle_data);
    }
  else
    {
      hw->handles_of_hw = hw_root (hw)->handles_of_hw;
    }
}

void
delete_hw_handle_data (struct hw *hw)
{
  /* NULL */
}



#if 0
void
hw_handle_init (struct hw *hw)
{
  struct hw_handle_mapping *current_map = db->mappings;
  if (current_map != NULL)
    {
      db->nr_mappings = db->mappings->external;
      /* verify that the mappings that were not removed are in
	 sequence down to nr 1 */
      while (current_map->next != NULL)
	{
	  if (current_map->external != current_map->next->external + 1)
	    error ("hw_handle: hw_handle database possibly corrupt");
	  current_map = current_map->next;
	}
      ASSERT (current_map->next == NULL);
      if (current_map->external != 1)
	error ("hw_handle: hw_handle database possibly corrupt");
    }
  else
    {
      db->nr_mappings = 0;
    }
}
#endif


struct hw_instance *
hw_handle_ihandle2 (struct hw *hw,
		    cell_word external)
{
  struct hw_handle_data *db = hw->handles_of_hw;
  struct hw_handle_mapping *current_map = db->mappings;
  while (current_map != NULL)
    {
      if (current_map->external == external)
	return current_map->ihandle;
      current_map = current_map->next;
    }
  return (void*)0;
}


struct hw *
hw_handle_phandle2 (struct hw *hw,
		    cell_word external)
{
  struct hw_handle_data *db = hw->handles_of_hw;
  struct hw_handle_mapping *current_map = db->mappings;
  while (current_map != NULL)
    {
      if (current_map->external == external)
	return current_map->phandle;
      current_map = current_map->next;
    }
  return (void*)0;
}


cell_word
hw_handle_2ihandle (struct hw *hw,
		    struct hw_instance *internal)
{
  struct hw_handle_data *db = hw->handles_of_hw;
  struct hw_handle_mapping *current_map = db->mappings;
  while (current_map != NULL)
    {
      if (current_map->ihandle == internal)
	return current_map->external;
      current_map = current_map->next;
    }
  return 0;
}


cell_word
hw_handle_2phandle (struct hw *hw,
		    struct hw *internal)
{
  struct hw_handle_data *db = hw->handles_of_hw;
  struct hw_handle_mapping *current_map = db->mappings;
  while (current_map != NULL)
    {
      if (current_map->phandle == internal)
	return current_map->external;
      current_map = current_map->next;
    }
  return 0;
}


void
hw_handle_add_ihandle (struct hw *hw,
		       struct hw_instance *internal)
{
  struct hw_handle_data *db = hw->handles_of_hw;
  if (hw_handle_2ihandle (hw, internal) != 0) 
    {
      hw_abort (hw, "attempting to add an ihandle already in the data base");
    }
  else
    {
      /* insert at the front making things in decending order */
      struct hw_handle_mapping *new_map = ZALLOC (struct hw_handle_mapping);
      new_map->next = db->mappings;
      new_map->ihandle = internal;
      db->nr_mappings += 1;
      new_map->external = db->nr_mappings;
      db->mappings = new_map;
    }
}


void
hw_handle_add_phandle (struct hw *hw,
		       struct hw *internal)
{
  struct hw_handle_data *db = hw->handles_of_hw;
  if (hw_handle_2phandle (hw, internal) != 0) 
    {
      hw_abort (hw, "attempting to add a phandle already in the data base");
    }
  else
    {
      /* insert at the front making things in decending order */
      struct hw_handle_mapping *new_map = ZALLOC (struct hw_handle_mapping);
      new_map->next = db->mappings;
      new_map->phandle = internal;
      db->nr_mappings += 1;
      new_map->external = db->nr_mappings;
      db->mappings = new_map;
    }
}


void
hw_handle_remove_ihandle (struct hw *hw,
			  struct hw_instance *internal)
{
  struct hw_handle_data *db = hw->handles_of_hw;
  struct hw_handle_mapping **current_map = &db->mappings;
  while (*current_map != NULL)
    {
      if ((*current_map)->ihandle == internal)
	{
	  struct hw_handle_mapping *delete = *current_map;
	  *current_map = delete->next;
	  zfree (delete);
	  return;
	}
      current_map = &(*current_map)->next;
    }
  hw_abort (hw, "attempt to remove nonexistant ihandle");
}


void
hw_handle_remove_phandle (struct hw *hw,
			  struct hw *internal)
{
  struct hw_handle_data *db = hw->handles_of_hw;
  struct hw_handle_mapping **current_map = &db->mappings;
  while (*current_map != NULL)
    {
      if ((*current_map)->phandle == internal)
	{
	  struct hw_handle_mapping *delete = *current_map;
	  *current_map = delete->next;
	  zfree (delete);
	  return;
	}
      current_map = &(*current_map)->next;
    }
  hw_abort (hw, "attempt to remove nonexistant phandle");
}


