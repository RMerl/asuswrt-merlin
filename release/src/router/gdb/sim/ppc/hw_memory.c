/*  This file is part of the program psim.

    Copyright (C) 1994-1997, Andrew Cagney <cagney@highland.com.au>

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


#ifndef _HW_MEMORY_C_
#define _HW_MEMORY_C_

#ifndef STATIC_INLINE_HW_MEMORY
#define STATIC_INLINE_HW_MEMORY STATIC_INLINE
#endif

#include "device_table.h"

/* DEVICE


   memory - description of system memory


   DESCRIPTION


   This device describes the size and location of the banks of
   physical memory within the simulation.

   In addition, this device supports the "claim" and "release" methods
   that can be used by OpenBoot client programs to manage the
   allocation of physical memory.


   PROPERTIES


   reg = { <address> <size> } (required)

   Each pair specify one bank of memory.

   available = { <address> <size> } (automatic)

   Each pair specifies a block of memory that is currently unallocated.  


   BUGS


   OpenFirmware doesn't make it clear if, when releasing memory the
   same address + size pair as was used during the claim should be
   specified.

   It is assumed that #size-cells and #address-cells for the parent
   node of this device are both one i.e. an address or size can be
   specified using a single memory cell (word).

   Significant work will be required before the <<memory>> device can
   support 64bit addresses (#address-cells equal two).

   */

typedef struct _memory_reg_spec {
  unsigned_cell base;
  unsigned_cell size;
} memory_reg_spec;

typedef struct _hw_memory_chunk hw_memory_chunk;
struct _hw_memory_chunk {
  unsigned_word address;
  unsigned_word size;
  int available;
  hw_memory_chunk *next;
};

typedef struct _hw_memory_device {
  hw_memory_chunk *heap;
} hw_memory_device;


static void *
hw_memory_create(const char *name,
		 const device_unit *unit_address,
		 const char *args)
{
  hw_memory_device *hw_memory = ZALLOC(hw_memory_device);
  return hw_memory;
}


static void
hw_memory_set_available(device *me,
			hw_memory_device *hw_memory)
{
  hw_memory_chunk *chunk = NULL;
  memory_reg_spec *available = NULL;
  int nr_available = 0;
  int curr = 0;
  int sizeof_available = 0;
  /* determine the nr of available chunks */
  chunk = hw_memory->heap;
  nr_available = 0;
  while (chunk != NULL) {
    if (chunk->available)
      nr_available += 1;
    ASSERT(chunk->next == NULL
	   || chunk->address < chunk->next->address);
    ASSERT(chunk->next == NULL
	   || chunk->address + chunk->size == chunk->next->address);
    chunk = chunk->next;
  }
  /* now create the available struct */
  ASSERT(nr_available > 0);
  sizeof_available = sizeof(memory_reg_spec) * nr_available;
  available = zalloc(sizeof_available);
  chunk = hw_memory->heap;
  curr = 0;
  while (chunk != NULL) {
    if (chunk->available) {
      available[curr].base = H2BE_cell(chunk->address);
      available[curr].size = H2BE_cell(chunk->size);
      curr += 1;
    }
    chunk = chunk->next;
  }
  /* update */
  device_set_array_property(me, "available", available, sizeof_available);
  zfree(available);
}


static void
hw_memory_init_address(device *me)
{
  hw_memory_device *hw_memory = (hw_memory_device*)device_data(me);

  /* free up any previous structures */
  {
    hw_memory_chunk *curr_chunk = hw_memory->heap;
    hw_memory->heap = NULL;
    while (curr_chunk != NULL) {
      hw_memory_chunk *dead_chunk = curr_chunk;
      curr_chunk = dead_chunk->next;
      dead_chunk->next = NULL;
      zfree(dead_chunk);
    }
  }

  /* attach memory regions according to the "reg" property */
  {
    int reg_nr;
    reg_property_spec reg;
    for (reg_nr = 0;
	 device_find_reg_array_property(me, "reg", reg_nr, &reg);
	 reg_nr++) {
      int i;
      /* check that the entry meets restrictions */
      for (i = 0; i < reg.address.nr_cells - 1; i++)
	if (reg.address.cells[i] != 0)
	  device_error(me, "Only single celled addresses supported");
      for (i = 0; i < reg.size.nr_cells - 1; i++)
	if (reg.size.cells[i] != 0)
	  device_error(me, "Only single celled sizes supported");
      /* attach the range */
      device_attach_address(device_parent(me),
			    attach_raw_memory,
			    0 /*address space*/,
			    reg.address.cells[reg.address.nr_cells - 1],
			    reg.size.cells[reg.size.nr_cells - 1],
			    access_read_write_exec,
			    me);
    }
  }

  /* create the initial `available memory' data structure */
  if (device_find_property(me, "available") != NULL) {
    hw_memory_chunk **curr_chunk = &hw_memory->heap;
    int cell_nr;
    unsigned_cell dummy;
    int nr_cells = device_find_integer_array_property(me, "available", 0, &dummy);
    if ((nr_cells % 2) != 0)
      device_error(me, "property \"available\" invalid - contains an odd number of cells");
    for (cell_nr = 0;
	 cell_nr < nr_cells;
	 cell_nr += 2) {
      hw_memory_chunk *new_chunk = ZALLOC(hw_memory_chunk);
      device_find_integer_array_property(me, "available", cell_nr,
					 &new_chunk->address);
      device_find_integer_array_property(me, "available", cell_nr + 1,
					 &new_chunk->size);
      new_chunk->available = 1;
      *curr_chunk = new_chunk;
      curr_chunk = &new_chunk->next;
    }
  }
  else {
    hw_memory_chunk **curr_chunk = &hw_memory->heap;
    int reg_nr;
    reg_property_spec reg;
    for (reg_nr = 0;
	 device_find_reg_array_property(me, "reg", reg_nr, &reg);
	 reg_nr++) {
      hw_memory_chunk *new_chunk;
      new_chunk = ZALLOC(hw_memory_chunk);
      new_chunk->address = reg.address.cells[reg.address.nr_cells - 1];
      new_chunk->size = reg.size.cells[reg.size.nr_cells - 1];
      new_chunk->available = 1;
      *curr_chunk = new_chunk;
      curr_chunk = &new_chunk->next;
    }
  }

  /* initialize the alloc property for this device */
  hw_memory_set_available(me, hw_memory);
}

static void
hw_memory_instance_delete(device_instance *instance)
{
  return;
}

static int
hw_memory_instance_claim(device_instance *instance,
			 int n_stack_args,
			 unsigned_cell stack_args[/*n_stack_args*/],
			 int n_stack_returns,
			 unsigned_cell stack_returns[/*n_stack_returns*/])
{
  hw_memory_device *hw_memory = device_instance_data(instance);
  device *me = device_instance_device(instance);
  int stackp = 0;
  unsigned_word alignment;
  unsigned_cell size;
  unsigned_cell address;
  hw_memory_chunk *chunk = NULL;

  /* get the alignment from the stack */
  if (n_stack_args < stackp + 1)
    device_error(me, "claim - incorrect number of arguments (alignment missing)");
  alignment = stack_args[stackp];
  stackp++;

  /* get the size from the stack */
  {
    int i;
    int nr_cells = device_nr_size_cells(device_parent(me));
    if (n_stack_args < stackp + nr_cells)
      device_error(me, "claim - incorrect number of arguments (size missing)");
    for (i = 0; i < nr_cells - 1; i++) {
      if (stack_args[stackp] != 0)
	device_error(me, "claim - multi-cell sizes not supported");
      stackp++;
    }
    size = stack_args[stackp];
    stackp++;
  }

  /* get the address from the stack */
  {
    int nr_cells = device_nr_address_cells(device_parent(me));
    if (alignment != 0) {
      if (n_stack_args != stackp) {
	if (n_stack_args == stackp + nr_cells)
	  DTRACE(memory, ("claim - extra address argument ignored\n"));
	else
	  device_error(me, "claim - incorrect number of arguments (optional addr)");
      }
      address = 0;
    }
    else {
      int i;
      if (n_stack_args != stackp + nr_cells)
	device_error(me, "claim - incorrect number of arguments (addr missing)");
      for (i = 0; i < nr_cells - 1; i++) {
	if (stack_args[stackp] != 0)
	  device_error(me, "claim - multi-cell addresses not supported");
	stackp++;
      }
      address = stack_args[stackp];
    }
  }

  /* check that there is space for the result */
  if (n_stack_returns != 0
      && n_stack_returns != device_nr_address_cells(device_parent(me)))
    device_error(me, "claim - invalid number of return arguments");

  /* find a chunk candidate, either according to address or alignment */
  if (alignment == 0) {
    chunk = hw_memory->heap;
    while (chunk != NULL) {
      if ((address + size) <= (chunk->address + chunk->size))
	break;
      chunk = chunk->next;
    }
    if (chunk == NULL || address < chunk->address || !chunk->available)
      device_error(me, "failed to allocate %ld bytes at 0x%lx",
		   (unsigned long)size, (unsigned long)address);
    DTRACE(memory, ("claim - address=0x%lx size=0x%lx\n",
		    (unsigned long)address,
		    (unsigned long)size));
  }
  else {
    /* adjust the alignment so that it is a power of two */
    unsigned_word align_mask = 1;
    while (align_mask < alignment && align_mask != 0)
      align_mask <<= 1;
    if (align_mask == 0)
      device_error(me, "alignment 0x%lx is to large", (unsigned long)alignment);
    align_mask -= 1;
    /* now find an aligned chunk that fits */
    chunk = hw_memory->heap;
    while (chunk != NULL) {
      address = ((chunk->address + align_mask) & ~align_mask);
      if ((chunk->available)
	  && (chunk->address + chunk->size >= address + size))
	break;
      chunk = chunk->next;
    }
    if (chunk == NULL)
      device_error(me, "failed to allocate %ld bytes with alignment %ld",
		   (unsigned long)size, (unsigned long)alignment);
    DTRACE(memory, ("claim - size=0x%lx alignment=%ld (0x%lx), address=0x%lx\n",
		    (unsigned long)size,
		    (unsigned long)alignment,
		    (unsigned long)alignment,
		    (unsigned long)address));
  }

  /* break off a bit before this chunk if needed */
  ASSERT(address >= chunk->address);
  if (address > chunk->address) {
    hw_memory_chunk *next_chunk = ZALLOC(hw_memory_chunk);
    /* insert a new chunk */
    next_chunk->next = chunk->next;
    chunk->next = next_chunk;
    /* adjust the address/size */
    next_chunk->address = address;
    next_chunk->size = chunk->address + chunk->size - next_chunk->address;
    next_chunk->available = 1;
    chunk->size = next_chunk->address - chunk->address;
    /* make this new chunk the one to allocate */
    chunk = next_chunk;
  }
  ASSERT(address == chunk->address);

  /* break off a bit after this chunk if needed */
  ASSERT(address + size <= chunk->address + chunk->size);
  if (address + size < chunk->address + chunk->size) {
    hw_memory_chunk *next_chunk = ZALLOC(hw_memory_chunk);
    /* insert it in to the list */
    next_chunk->next = chunk->next;
    chunk->next = next_chunk;
    /* adjust the address/size */
    next_chunk->address = address + size;
    next_chunk->size = chunk->address + chunk->size - next_chunk->address;
    next_chunk->available = 1;
    chunk->size = next_chunk->address - chunk->address;
  }
  ASSERT(address + size == chunk->address + chunk->size);

  /* now allocate/return it */
  chunk->available = 0;
  hw_memory_set_available(device_instance_device(instance), hw_memory);
  if (n_stack_returns > 0) {
    int i;
    for (i = 0; i < n_stack_returns - 1; i++)
      stack_returns[i] = 0;
    stack_returns[n_stack_returns - 1] = address;
  }

  return 0;
}


static int
hw_memory_instance_release(device_instance *instance,
			   int n_stack_args,
			   unsigned_cell stack_args[/*n_stack_args*/],
			   int n_stack_returns,
			   unsigned_cell stack_returns[/*n_stack_returns*/])
{
  hw_memory_device *hw_memory = device_instance_data(instance);
  device *me = device_instance_device(instance);
  unsigned_word length;
  unsigned_word address;
  int stackp = 0;
  hw_memory_chunk *chunk;
  
  /* get the length from the stack */
  {
    int i;
    int nr_cells = device_nr_size_cells(device_parent(me));
    if (n_stack_args < stackp + nr_cells)
      device_error(me, "release - incorrect number of arguments (length missing)");
    for (i = 0; i < nr_cells - 1; i++) {
      if (stack_args[stackp] != 0)
	device_error(me, "release - multi-cell length not supported");
      stackp++;
    }
    length = stack_args[stackp];
    stackp++;
  }

  /* get the address from the stack */
  {
    int i;
    int nr_cells = device_nr_address_cells(device_parent(me));
    if (n_stack_args != stackp + nr_cells)
      device_error(me, "release - incorrect number of arguments (addr missing)");
    for (i = 0; i < nr_cells - 1; i++) {
      if (stack_args[stackp] != 0)
	device_error(me, "release - multi-cell addresses not supported");
      stackp++;
    }
    address = stack_args[stackp];
  }

  /* returns ok */
  if (n_stack_returns != 0)
    device_error(me, "release - nonzero number of results");

  /* try to free the corresponding memory chunk */
  chunk = hw_memory->heap;
  while (chunk != NULL) {
    if (chunk->address == address
	&& chunk->size == length) {
      /* an exact match */
      if (chunk->available)
	device_error(me, "memory chunk 0x%lx (size 0x%lx) already available",
		     (unsigned long)address,
		     (unsigned long)length);
      else {
	/* free this chunk */
	DTRACE(memory, ("release - address=0x%lx, length=0x%lx\n",
			(unsigned long) address,
			(unsigned long) length));
	chunk->available = 1;
	break;
      }
    }
    else if (chunk->address >= address
	     && chunk->address + chunk->size <= address + length) {
      /* a sub region */
      if (!chunk->available) {
	DTRACE(memory, ("release - address=0x%lx, size=0x%lx within region 0x%lx length 0x%lx\n",
			(unsigned long) chunk->address,
			(unsigned long) chunk->size,
			(unsigned long) address,
			(unsigned long) length));
	chunk->available = 1;
      }
    }
    chunk = chunk->next;
  }
  if (chunk == NULL) {
    printf_filtered("warning: released chunks within region 0x%lx..0x%lx\n",
		    (unsigned long)address,
		    (unsigned long)(address + length - 1));
  }

  /* check for the chance to merge two adjacent available memory chunks */
  chunk = hw_memory->heap;
  while (chunk != NULL) {
    if (chunk->available
	&& chunk->next != NULL && chunk->next->available) {
      /* adjacent */
      hw_memory_chunk *delete = chunk->next;
      ASSERT(chunk->address + chunk->size == delete->address);
      chunk->size += delete->size;
      chunk->next = delete->next;
      zfree(delete);
    }
    else {
      chunk = chunk->next;
    }
  }

  /* update the corresponding property */
  hw_memory_set_available(device_instance_device(instance), hw_memory);

  return 0;
}


static device_instance_methods hw_memory_instance_methods[] = {
  { "claim", hw_memory_instance_claim },
  { "release", hw_memory_instance_release },
  { NULL, },
};

static device_instance_callbacks const hw_memory_instance_callbacks = {
  hw_memory_instance_delete,
  NULL /*read*/, NULL /*write*/, NULL /*seek*/,
  hw_memory_instance_methods
};

static device_instance *
hw_memory_create_instance(device *me,
			  const char *path,
			  const char *args)
{
  return device_create_instance_from(me, NULL,
				     device_data(me), /* nothing better */
				     path, args,
				     &hw_memory_instance_callbacks);
}

static device_callbacks const hw_memory_callbacks = {
  { hw_memory_init_address, },
  { NULL, }, /* address */
  { NULL, }, /* IO */
  { NULL, }, /* DMA */
  { NULL, }, /* interrupt */
  { NULL, }, /* unit */
  hw_memory_create_instance,
};

const device_descriptor hw_memory_device_descriptor[] = {
  { "memory", hw_memory_create, &hw_memory_callbacks },
  { NULL },
};

#endif /* _HW_MEMORY_C_ */
