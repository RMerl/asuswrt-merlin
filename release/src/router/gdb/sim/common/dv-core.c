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


#include "sim-main.h"
#include "hw-main.h"

/* DEVICE
   
   core - root of the device tree
   
   DESCRIPTION
   
   The core device, positioned at the root of the device tree appears
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
dv_core_attach_address_callback (struct hw *me,
				 int level,
				 int space,
				 address_word addr,
				 address_word nr_bytes,
				 struct hw *client)
{
  HW_TRACE ((me, "attach - level=%d, space=%d, addr=0x%lx, nr_bytes=%ld, client=%s",
	     level, space, (unsigned long) addr, (unsigned long) nr_bytes, hw_path (client)));
  /* NOTE: At preset the space is assumed to be zero.  Perhaphs the
     space should be mapped onto something for instance: space0 -
     unified memory; space1 - IO memory; ... */
  sim_core_attach (hw_system (me),
		   NULL, /*cpu*/
		   level,
		   access_read_write_exec,
		   space, addr,
		   nr_bytes,
		   0, /* modulo */
		   client,
		   NULL);
}


static unsigned
dv_core_dma_read_buffer_callback (struct hw *me,
				  void *dest,
				  int space,
				  unsigned_word addr,
				  unsigned nr_bytes)
{
  return sim_core_read_buffer (hw_system (me),
			       NULL, /*CPU*/
			       space, /*???*/
			       dest,
			       addr,
			       nr_bytes);
}


static unsigned
dv_core_dma_write_buffer_callback (struct hw *me,
				   const void *source,
				   int space,
				   unsigned_word addr,
				   unsigned nr_bytes,
				   int violate_read_only_section)
{
  return sim_core_write_buffer (hw_system (me),
				NULL, /*cpu*/
				space, /*???*/
				source,
				addr,
				nr_bytes);
}


static void
dv_core_finish (struct hw *me)
{
  set_hw_attach_address (me, dv_core_attach_address_callback);
  set_hw_dma_write_buffer (me, dv_core_dma_write_buffer_callback);
  set_hw_dma_read_buffer (me, dv_core_dma_read_buffer_callback);
}

const struct hw_descriptor dv_core_descriptor[] = {
  { "core", dv_core_finish, },
  { NULL },
};
