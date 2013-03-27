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


#ifndef _HW_VM_C_
#define _HW_VM_C_

#include "device_table.h"
#include "cpu.h"

#include <signal.h>

/* DEVICE

   vm - virtual memory device for user simulation modes

   DESCRIPTION

   In user mode, mapped text, data and stack addresses are managed by
   the core.  Unmapped addresses are passed onto this device (because
   it establishes its self as the fallback device) for processing.

   During initialization, children of this device will request the
   mapping of the initial text and data segments.  Those requests are
   passed onto the core device so that that may establish the initial
   memory regions.

   Once the simulation has started (as noted above) any access to an
   unmapped address range will be passed down to this device as an IO
   access.  This device will then either attach additional memory to
   the core device or signal the access as being invalid.

   The IOCTL function is used to notify this device of any changes to
   the users `brk' point.

   PROPERTIES

   stack-base = <number>

   Specifies the lower address of the stack segment in the users
   virtual address space.  The initial stack page is defined by
   stack-base + nr-bytes.

   nr-bytes = <number>

   Specifies the maximum size of the stack segment in the users
   address space.

   */

typedef struct _hw_vm_device {
  /* area of memory valid for stack addresses */
  unsigned_word stack_base; /* min possible stack value */
  unsigned_word stack_bound;
  unsigned_word stack_lower_limit;
  /* area of memory valid for heap addresses */
  unsigned_word heap_base;
  unsigned_word heap_bound;
  unsigned_word heap_upper_limit;
} hw_vm_device;


static void
hw_vm_init_address_callback(device *me)
{
  hw_vm_device *vm = (hw_vm_device*)device_data(me);

  /* revert the stack/heap variables to their defaults */
  vm->stack_base = device_find_integer_property(me, "stack-base");
  vm->stack_bound = (vm->stack_base
		     + device_find_integer_property(me, "nr-bytes"));
  vm->stack_lower_limit = vm->stack_bound;
  vm->heap_base = 0;
  vm->heap_bound = 0;
  vm->heap_upper_limit = 0;

  /* establish this device as the default memory handler */
  device_attach_address(device_parent(me),
			attach_callback + 1,
			0 /*address space - ignore*/,
			0 /*addr - ignore*/,
			(((unsigned)0)-1) /*nr_bytes - ignore*/,
			access_read_write /*access*/,
			me);
}


static void
hw_vm_attach_address(device *me,
		     attach_type attach,
		     int space,
		     unsigned_word addr,
		     unsigned nr_bytes,
		     access_type access,
		     device *client) /*callback/default*/
{
  hw_vm_device *vm = (hw_vm_device*)device_data(me);
  /* update end of bss if necessary */
  if (vm->heap_base < addr + nr_bytes) {
    vm->heap_base = addr + nr_bytes;
    vm->heap_bound = addr + nr_bytes;
    vm->heap_upper_limit = addr + nr_bytes;
  }
  device_attach_address(device_parent(me),
			attach_raw_memory,
			0 /*address space*/,
			addr,
			nr_bytes,
			access,
			me);
}


static unsigned
hw_vm_add_space(device *me,
		unsigned_word addr,
		unsigned nr_bytes,
		cpu *processor,
		unsigned_word cia)
{
  hw_vm_device *vm = (hw_vm_device*)device_data(me);
  unsigned_word block_addr;
  unsigned block_nr_bytes;

  /* an address in the stack area, allocate just down to the addressed
     page */
  if (addr >= vm->stack_base && addr < vm->stack_lower_limit) {
    block_addr = FLOOR_PAGE(addr);
    block_nr_bytes = vm->stack_lower_limit - block_addr;
    vm->stack_lower_limit = block_addr;
  }
  /* an address in the heap area, allocate all of the required heap */
  else if (addr >= vm->heap_upper_limit && addr < vm->heap_bound) {
    block_addr = vm->heap_upper_limit;
    block_nr_bytes = vm->heap_bound - vm->heap_upper_limit;
    vm->heap_upper_limit = vm->heap_bound;
  }
  /* oops - an invalid address - abort the cpu */
  else if (processor != NULL) {
    cpu_halt(processor, cia, was_signalled, SIGSEGV);
    return 0;
  }
  /* 2*oops - an invalid address and no processor */
  else {
    return 0;
  }

  /* got the parameters, allocate the space */
  device_attach_address(device_parent(me),
			attach_raw_memory,
			0 /*address space*/,
			block_addr,
			block_nr_bytes,
			access_read_write,
			me);
  return block_nr_bytes;
}


static unsigned
hw_vm_io_read_buffer_callback(device *me,
			   void *dest,
			   int space,
			   unsigned_word addr,
			   unsigned nr_bytes,
			   cpu *processor,
			   unsigned_word cia)
{
  if (hw_vm_add_space(me, addr, nr_bytes, processor, cia) >= nr_bytes) {
    memset(dest, 0, nr_bytes); /* always initialized to zero */
    return nr_bytes;
  }
  else 
    return 0;
}


static unsigned
hw_vm_io_write_buffer_callback(device *me,
			    const void *source,
			    int space,
			    unsigned_word addr,
			    unsigned nr_bytes,
			    cpu *processor,
			    unsigned_word cia)
{
  if (hw_vm_add_space(me, addr, nr_bytes, processor, cia) >= nr_bytes) {
    return device_dma_write_buffer(device_parent(me), source,
				   space, addr,
				   nr_bytes,
				   0/*violate_read_only*/);
  }
  else
    return 0;
}


static int
hw_vm_ioctl(device *me,
	    cpu *processor,
	    unsigned_word cia,
	    device_ioctl_request request,
	    va_list ap)
{
  /* While the caller is notified that the heap has grown by the
     requested amount, the heap is actually extended out to a page
     boundary. */
  hw_vm_device *vm = (hw_vm_device*)device_data(me);
  switch (request) {
  case device_ioctl_break:
    {
      unsigned_word requested_break = va_arg(ap, unsigned_word);
      unsigned_word new_break = ALIGN_8(requested_break);
      unsigned_word old_break = vm->heap_bound;
      signed_word delta = new_break - old_break;
      if (delta > 0)
	vm->heap_bound = ALIGN_PAGE(new_break);
      break;
    }
  default:
    device_error(me, "Unsupported ioctl request");
    break;
  }
  return 0;
    
}


static device_callbacks const hw_vm_callbacks = {
  { hw_vm_init_address_callback, },
  { hw_vm_attach_address,
    passthrough_device_address_detach, },
  { hw_vm_io_read_buffer_callback,
    hw_vm_io_write_buffer_callback, },
  { NULL, passthrough_device_dma_write_buffer, },
  { NULL, }, /* interrupt */
  { generic_device_unit_decode,
    generic_device_unit_encode, },
  NULL, /* instance */
  hw_vm_ioctl,
};


static void *
hw_vm_create(const char *name,
	     const device_unit *address,
	     const char *args)
{
  hw_vm_device *vm = ZALLOC(hw_vm_device);
  return vm;
}

const device_descriptor hw_vm_device_descriptor[] = {
  { "vm", hw_vm_create, &hw_vm_callbacks },
  { NULL },
};

#endif /* _HW_VM_C_ */
