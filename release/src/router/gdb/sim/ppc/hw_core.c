/*  This file is part of the program psim.
    
    Copyright (C) 1994-1996, Andrew Cagney <cagney@highland.com.au>
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
    
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    
    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
    
    */


#ifndef _HW_CORE_C_
#define _HW_CORE_C_

#include "device_table.h"

#include "corefile.h"


/* DEVICE
   
   core - root of the device tree
   
   DESCRIPTION
   
   The core device positioned at the root of the device tree appears
   to its child devices as a normal device just like every other
   device in the tree.

   Internally it is implemented using a core object.  Requests to
   attach (or detach) address spaces are passed to that core object.
   Requests to transfer (DMA) data are reflected back down the device
   tree using the core_map data transfer methods.

   PROPERTIES

   None.

   */


static void
hw_core_init_address_callback(device *me)
{
  core *memory = (core*)device_data(me);
  core_init(memory);
}


static void
hw_core_attach_address_callback(device *me,
				attach_type attach,
				int space,
				unsigned_word addr,
				unsigned nr_bytes,
				access_type access,
				device *client) /*callback/default*/
{
  core *memory = (core*)device_data(me);
  if (space != 0)
    error("core_attach_address_callback() invalid address space\n");
  core_attach(memory,
	      attach,
	      space,
	      access,
	      addr,
	      nr_bytes,
	      client);
}


static unsigned
hw_core_dma_read_buffer_callback(device *me,
				 void *dest,
				 int space,
				 unsigned_word addr,
				 unsigned nr_bytes)
{
  core *memory = (core*)device_data(me);
  return core_map_read_buffer(core_readable(memory),
			      dest,
			      addr,
			      nr_bytes);
}


static unsigned
hw_core_dma_write_buffer_callback(device *me,
				  const void *source,
				  int space,
				  unsigned_word addr,
				  unsigned nr_bytes,
				  int violate_read_only_section)
{
  core *memory = (core*)device_data(me);
  core_map *map = (violate_read_only_section
		   ? core_readable(memory)
		   : core_writeable(memory));
  return core_map_write_buffer(map,
			       source,
			       addr,
			       nr_bytes);
}

static device_callbacks const hw_core_callbacks = {
  { hw_core_init_address_callback, },
  { hw_core_attach_address_callback, },
  { NULL, }, /* IO */
  { hw_core_dma_read_buffer_callback,
    hw_core_dma_write_buffer_callback, },
  { NULL, }, /* interrupt */
  { generic_device_unit_decode,
    generic_device_unit_encode,
    generic_device_address_to_attach_address,
    generic_device_size_to_attach_size, },
};


static void *
hw_core_create(const char *name,
	       const device_unit *unit_address,
	       const char *args)
{
  core *memory = core_create();
  return memory;
}

const device_descriptor hw_core_device_descriptor[] = {
  { "core", hw_core_create, &hw_core_callbacks },
  { NULL },
};

#endif /* _HW_CORE_C_ */
