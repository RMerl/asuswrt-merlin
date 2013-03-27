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


#ifndef _HW_OPIC_C_
#define _HW_OPIC_C_

#include "device_table.h"

#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif


/* DEVICE


   opic - Open Programmable Interrupt Controller (OpenPIC)


   DESCRIPTION


   This device implements the core of the OpenPIC interrupt controller
   as described in the OpenPIC specification 1.2 and other related
   documents.

   The model includes:

   o	Up to 2048 external interrupt sources

   o	The four count down timers

   o	The four interprocessor multicast interrupts

   o	multiprocessor support

   o	Full tracing to assist help debugging

   o	Support for all variations of edge/level x high/low polarity.



   PROPERTIES


   reg = <address> <size> ... (required)

   Determine where the device lives in the parents address space.  The
   first <<address>> <<size>> pair specifies the address of the
   interrupt destination unit (which might contain an interrupt source
   unit) while successive reg entries specify additional interrupt
   source units.

   Note that for an <<opic>> device attached to a <<pci>> bus, the
   first <<reg>> entry may need to be ignored it will be the address
   of the devices configuration registers.


   interrupt-ranges = <int-number> <range> ... (required)

   A list of pairs.  Each pair corresponds to a block of interrupt
   source units (the address of which being specified by the
   corresponding reg tupple).  <<int-number>> is the number of the
   first interrupt in the block while <<range>> is the number of
   interrupts in the block.


   timer-frequency = <integer>  (optional)

   If present, specifies the default value of the timer frequency
   reporting register.  By default a value of 1 HZ is used.  The value
   is arbitrary, the timers are always updated once per machine cycle.


   vendor-identification = <integer>  (optional)

   If present, specifies the value to be returned when the vendor
   identification register is read.


   EXAMPLES


   See the test suite directory:

   |  psim-test/hw-opic


   BUGS

   For an OPIC controller attached to a PCI bus, it is not clear what
   the value of the <<reg>> and <<interrupt-ranges>> properties should
   be.  In particular, the PCI firmware bindings require the first
   value of the <<reg>> property to specify the devices configuration
   address while the OpenPIC bindings require that same entry to
   specify the address of the Interrupt Delivery Unit.  This
   implementation checks for and, if present, ignores any
   configuration address (and its corresponding <<interrupt-ranges>>
   entry).

   The OpenPIC specification requires the controller to be fair when
   distributing interrupts between processors.  At present the
   algorithm used isn't fair.  It is biased towards processor zero.

   The OpenPIC specification includes a 8259 pass through mode.  This
   is not supported.


   REFERENCES

   
   PowerPC Multiprocessor Interrupt Controller (MPIC), January 19,
   1996. Available from IBM.


   The Open Programmable Interrupt Controller (PIC) Register Interface
   Specification Revision 1.2.  Issue Date: Opctober 1995.  Available
   somewhere on AMD's web page (http://www.amd.com/)


   PowerPC Microprocessor Common Hardware Reference Platform (CHRP)
   System bindings to: IEEE Std 1275-1994 Standard for Boot
   (Initialization, Configuration) Firmware.  Revision 1.2b (INTERIM
   DRAFT).  April 22, 1996.  Available on the Open Firmware web site
   http://playground.sun.com/p1275/.


   */


/* forward types */

typedef struct _hw_opic_device hw_opic_device;


/* bounds */

enum {
  max_nr_interrupt_sources = 2048,
  max_nr_interrupt_destinations = 32,
  max_nr_task_priorities = 16,
};


enum {
  opic_alignment = 16,
};


/* global configuration register */

enum {
  gcr0_8259_bit = 0x20000000,
  gcr0_reset_bit = 0x80000000,
};


/* offsets and sizes */

enum {
  idu_isu_base = 0x10000,
  sizeof_isu_register_block = 32,
  idu_per_processor_register_base = 0x20000,
  sizeof_idu_per_processor_register_block = 0x1000,
  idu_timer_base = 0x01100,
  sizeof_timer_register_block = 0x00040,
};


/* Interrupt sources */

enum {
  isu_mask_bit = 0x80000000,
  isu_active_bit = 0x40000000,
  isu_multicast_bit = 0x20000000,
  isu_positive_polarity_bit = 0x00800000,
  isu_level_triggered_bit = 0x00400000,
  isu_priority_shift = 16,
  isu_vector_bits = 0x000000ff,
};


typedef struct _opic_interrupt_source {
  unsigned is_masked; /* left in place */
  unsigned is_multicast; /* left in place */
  unsigned is_positive_polarity; /* left in place */
  unsigned is_level_triggered; /* left in place */
  unsigned priority;
  unsigned vector;
  /* misc */
  int nr;
  unsigned destination;
  unsigned pending;
  unsigned in_service;
} opic_interrupt_source;


/* interrupt destinations (normally processors) */

typedef struct _opic_interrupt_destination {
  int nr;
  unsigned base_priority;
  opic_interrupt_source *current_pending;
  opic_interrupt_source *current_in_service;
  unsigned bit;
  int init_port;
  int intr_port;
} opic_interrupt_destination;


/* address map descriptors */

typedef struct _opic_isu_block { /* interrupt source unit block */
  int space;
  unsigned_word address;
  unsigned size;
  unsigned_cell int_number;
  unsigned_cell range;
  int reg;
} opic_isu_block;


typedef struct _opic_idu { /* interrupt delivery unit */
  int reg;
  int space;
  unsigned_word address;
  unsigned size;
} opic_idu;

typedef enum {
  /* bad */
  invalid_opic_register,
  /* interrupt source */
  interrupt_source_N_destination_register,
  interrupt_source_N_vector_priority_register,
  /* timers */
  timer_N_destination_register,
  timer_N_vector_priority_register,
  timer_N_base_count_register,
  timer_N_current_count_register,
  timer_frequency_reporting_register,
  /* inter-processor interrupts */
  ipi_N_vector_priority_register,
  ipi_N_dispatch_register,
  /* global configuration */
  spurious_vector_register,
  processor_init_register,
  vendor_identification_register,
  global_configuration_register_N,
  feature_reporting_register_N,
  /* per processor */
  end_of_interrupt_register_N,
  interrupt_acknowledge_register_N,
  current_task_priority_register_N,
} opic_register;

static const char *
opic_register_name(opic_register type)
{
  switch (type) {
  case invalid_opic_register: return "invalid_opic_register";
  case interrupt_source_N_destination_register: return "interrupt_source_N_destination_register";
  case interrupt_source_N_vector_priority_register: return "interrupt_source_N_vector_priority_register";
  case timer_N_destination_register: return "timer_N_destination_register";
  case timer_N_vector_priority_register: return "timer_N_vector_priority_register";
  case timer_N_base_count_register: return "timer_N_base_count_register";
  case timer_N_current_count_register: return "timer_N_current_count_register";
  case timer_frequency_reporting_register: return "timer_frequency_reporting_register";
  case ipi_N_vector_priority_register: return "ipi_N_vector_priority_register";
  case ipi_N_dispatch_register: return "ipi_N_dispatch_register";
  case spurious_vector_register: return "spurious_vector_register";
  case processor_init_register: return "processor_init_register";
  case vendor_identification_register: return "vendor_identification_register";
  case global_configuration_register_N: return "global_configuration_register_N";
  case feature_reporting_register_N: return "feature_reporting_register_N";
  case end_of_interrupt_register_N: return "end_of_interrupt_register_N";
  case interrupt_acknowledge_register_N: return "interrupt_acknowledge_register_N";
  case current_task_priority_register_N: return "current_task_priority_register_N";
  }
  return NULL;
}



/* timers */

typedef struct _opic_timer {
  int nr;
  device *me; /* find my way home */
  hw_opic_device *opic; /* ditto */
  unsigned base_count;
  int inhibited;
  signed64 count; /* *ONLY* if inhibited */
  event_entry_tag timeout_event;
  opic_interrupt_source *interrupt_source;
} opic_timer;


/* the OPIC */

struct _hw_opic_device {

  /* vendor id */
  unsigned vendor_identification;

  /* interrupt destinations - processors */
  int nr_interrupt_destinations;
  opic_interrupt_destination *interrupt_destination;
  unsigned sizeof_interrupt_destination;

  /* bogus interrupts */
  int spurious_vector;

  /* interrupt sources - external interrupt source units + extra internal ones */
  int nr_interrupt_sources;
  opic_interrupt_source *interrupt_source;
  unsigned sizeof_interrupt_source;

  /* external interrupts */
  int nr_external_interrupts;
  opic_interrupt_source *external_interrupt_source;

  /* inter-processor-interrupts */
  int nr_interprocessor_interrupts;
  opic_interrupt_source *interprocessor_interrupt_source;

  /* timers */
  int nr_timer_interrupts;
  opic_timer *timer;
  unsigned sizeof_timer;
  opic_interrupt_source *timer_interrupt_source;
  unsigned timer_frequency;

  /* init register */
  unsigned32 init;

  /* address maps */
  opic_idu idu;
  int nr_isu_blocks;
  opic_isu_block *isu_block;
};


static void
hw_opic_init_data(device *me)
{
  hw_opic_device *opic = (hw_opic_device*)device_data(me);
  int isb;
  int idu_reg;
  int nr_isu_blocks;
  int i;

  /* determine the first valid reg property entry (there could be
     leading reg entries with invalid (zero) size fields) and the
     number of isu entries found in the reg property. */
  idu_reg = 0;
  nr_isu_blocks = 0;
  while (1) {
    reg_property_spec unit;
    int attach_space;
    unsigned_word attach_address;
    unsigned attach_size;
    if (!device_find_reg_array_property(me, "reg", idu_reg + nr_isu_blocks,
					&unit))
      break;
    if (nr_isu_blocks > 0
	|| (device_address_to_attach_address(device_parent(me), &unit.address,
					     &attach_space, &attach_address,
					     me)
	    && device_size_to_attach_size(device_parent(me), &unit.size,
					  &attach_size,
					  me))) {
      /* we count any thing once we've found one valid address/size pair */
      nr_isu_blocks += 1;
    }
    else {
      idu_reg += 1;
    }
  }

  /* determine the number and location of the multiple interrupt
     source units and the single interrupt delivery unit */
  if (opic->isu_block == NULL) {
    int reg_nr;
    opic->nr_isu_blocks = nr_isu_blocks;
    opic->isu_block = zalloc(sizeof(opic_isu_block) * opic->nr_isu_blocks);
    isb = 0;
    reg_nr = idu_reg;
    while (isb < opic->nr_isu_blocks) {
      reg_property_spec reg;
      if (!device_find_reg_array_property(me, "reg", reg_nr, &reg))
	device_error(me, "reg property missing entry number %d", reg_nr);
      opic->isu_block[isb].reg = reg_nr;
      if (!device_address_to_attach_address(device_parent(me), &reg.address,
					    &opic->isu_block[isb].space,
					    &opic->isu_block[isb].address,
					    me)
	  || !device_size_to_attach_size(device_parent(me), &reg.size,
					 &opic->isu_block[isb].size,
					 me)) {
	device_error(me, "reg property entry %d invalid", reg_nr);
      }
      if (!device_find_integer_array_property(me, "interrupt-ranges",
					      reg_nr * 2,
					      &opic->isu_block[isb].int_number)
	  || !device_find_integer_array_property(me, "interrupt-ranges",
						 reg_nr * 2 + 1,
						 &opic->isu_block[isb].range))
	device_error(me, "missing or invalid interrupt-ranges property entry %d", reg_nr);
      /* first reg entry specifies the address of both the IDU and the
         first set of ISU registers, adjust things accordingly */
      if (reg_nr == idu_reg) {
	opic->idu.reg = opic->isu_block[isb].reg;
	opic->idu.space = opic->isu_block[isb].space;
	opic->idu.address = opic->isu_block[isb].address;
	opic->idu.size = opic->isu_block[isb].size;
	opic->isu_block[isb].address += idu_isu_base;
	opic->isu_block[isb].size = opic->isu_block[isb].range * (16 + 16);
      }
      /* was this a valid reg entry? */
      if (opic->isu_block[isb].range == 0) {
	opic->nr_isu_blocks -= 1;
      }
      else {
	opic->nr_external_interrupts += opic->isu_block[isb].range;
	isb++;
      }
      reg_nr++;
    }
  }
  DTRACE(opic, ("interrupt source unit block - effective number of blocks %d\n",
		(int)opic->nr_isu_blocks));


  /* the number of other interrupts */
  opic->nr_interprocessor_interrupts = 4;
  opic->nr_timer_interrupts = 4;


  /* create space for the interrupt source registers */
  if (opic->interrupt_source != NULL) {
    memset(opic->interrupt_source, 0, opic->sizeof_interrupt_source);
  }
  else {
    opic->nr_interrupt_sources = (opic->nr_external_interrupts
				  + opic->nr_interprocessor_interrupts
				  + opic->nr_timer_interrupts);
    if (opic->nr_interrupt_sources > max_nr_interrupt_sources)
      device_error(me, "number of interrupt sources exceeded");
    opic->sizeof_interrupt_source = (sizeof(opic_interrupt_source)
				     * opic->nr_interrupt_sources);
    opic->interrupt_source = zalloc(opic->sizeof_interrupt_source);
    opic->external_interrupt_source = opic->interrupt_source;
    opic->interprocessor_interrupt_source = (opic->external_interrupt_source
					     + opic->nr_external_interrupts);
    opic->timer_interrupt_source = (opic->interprocessor_interrupt_source
				    + opic->nr_interprocessor_interrupts);
  }
  for (i = 0; i < opic->nr_interrupt_sources; i++) {
    opic_interrupt_source *source = &opic->interrupt_source[i];
    source->nr = i;
    source->is_masked = isu_mask_bit;
  }
  DTRACE(opic, ("interrupt sources - external %d, timer %d, ipi %d, total %d\n",
		opic->nr_external_interrupts,
		opic->nr_timer_interrupts,
		opic->nr_interprocessor_interrupts,
		opic->nr_interrupt_sources));


  /* timers or interprocessor interrupts */
  if (opic->timer != NULL)
    memset(opic->timer, 0, opic->sizeof_timer);
  else {
    opic->nr_timer_interrupts = 4;
    opic->sizeof_timer = sizeof(opic_timer) * opic->nr_timer_interrupts;
    opic->timer = zalloc(opic->sizeof_timer);
  }
  for (i = 0; i < opic->nr_timer_interrupts; i++) {
    opic_timer *timer = &opic->timer[i];
    timer->nr = i;
    timer->me = me;
    timer->opic = opic;
    timer->inhibited = 1;
    timer->interrupt_source = &opic->timer_interrupt_source[i];
  }
  if (device_find_property(me, "timer-frequency"))
    opic->timer_frequency = device_find_integer_property(me, "timer-frequency");
  else
    opic->timer_frequency = 1;


  /* create space for the interrupt destination registers */
  if (opic->interrupt_destination != NULL) {
    memset(opic->interrupt_destination, 0, opic->sizeof_interrupt_destination);
  }
  else {
    opic->nr_interrupt_destinations = tree_find_integer_property(me, "/openprom/options/smp");
    opic->sizeof_interrupt_destination = (sizeof(opic_interrupt_destination)
					  * opic->nr_interrupt_destinations);
    opic->interrupt_destination = zalloc(opic->sizeof_interrupt_destination);
    if (opic->nr_interrupt_destinations > max_nr_interrupt_destinations)
      device_error(me, "number of interrupt destinations exceeded");
  }
  for (i = 0; i < opic->nr_interrupt_destinations; i++) {
    opic_interrupt_destination *dest = &opic->interrupt_destination[i];
    dest->bit = (1 << i);
    dest->nr = i;
    dest->init_port = (device_interrupt_decode(me, "init0", output_port)
		       + i);
    dest->intr_port = (device_interrupt_decode(me, "intr0", output_port)
		       + i);
    dest->base_priority = max_nr_task_priorities - 1;
  }
  DTRACE(opic, ("interrupt destinations - total %d\n",
		(int)opic->nr_interrupt_destinations));
  

  /* verify and print out the ISU's */
  for (isb = 0; isb < opic->nr_isu_blocks; isb++) {
    unsigned correct_size;
    if ((opic->isu_block[isb].address % opic_alignment) != 0)
      device_error(me, "interrupt source unit %d address not aligned to %d byte boundary",
		   isb, opic_alignment);
    correct_size = opic->isu_block[isb].range * sizeof_isu_register_block;
    if (opic->isu_block[isb].size != correct_size)
      device_error(me, "interrupt source unit %d (reg %d) has an incorrect size, should be 0x%x",
		   isb, opic->isu_block[isb].reg, correct_size);
    DTRACE(opic, ("interrupt source unit block %ld - address %d:0x%lx, size 0x%lx, int-number %ld, range %ld\n",
		  (long)isb,
		  (int)opic->isu_block[isb].space,
		  (unsigned long)opic->isu_block[isb].address,
		  (unsigned long)opic->isu_block[isb].size,
		  (long)opic->isu_block[isb].int_number,
		  (long)opic->isu_block[isb].range));
  }


  /* verify and print out the IDU */
  {
    unsigned correct_size;
    unsigned alternate_size;
    if ((opic->idu.address % opic_alignment) != 0)
      device_error(me, "interrupt delivery unit not aligned to %d byte boundary",
		   opic_alignment);
    correct_size = (idu_per_processor_register_base
		    + (sizeof_idu_per_processor_register_block
		       * opic->nr_interrupt_destinations));
    alternate_size = (idu_per_processor_register_base
		      + (sizeof_idu_per_processor_register_block
			 * max_nr_interrupt_destinations));
    if (opic->idu.size != correct_size
	&& opic->idu.size != alternate_size)
      device_error(me, "interrupt delivery unit has incorrect size, should be 0x%x or 0x%x",
		   correct_size, alternate_size);
    DTRACE(opic, ("interrupt delivery unit - address %d:0x%lx, size 0x%lx\n",
		  (int)opic->idu.space,
		  (unsigned long)opic->idu.address,
		  (unsigned long)opic->idu.size));
  }

  /* initialize the init interrupts */
  opic->init = 0;


  /* vendor ident */
  if (device_find_property(me, "vendor-identification") != NULL)
    opic->vendor_identification = device_find_integer_property(me, "vendor-identification");
  else
    opic->vendor_identification = 0;

  /* misc registers */
  opic->spurious_vector = 0xff;

}


/* interrupt related actions */

static void
assert_interrupt(device *me,
		 hw_opic_device *opic,
		 opic_interrupt_destination *dest)
{
  ASSERT(dest >= opic->interrupt_destination);
  ASSERT(dest < opic->interrupt_destination + opic->nr_interrupt_destinations);
  DTRACE(opic, ("assert interrupt - intr port %d\n", dest->intr_port));
  device_interrupt_event(me, dest->intr_port, 1, NULL, 0);
}


static void
negate_interrupt(device *me,
		 hw_opic_device *opic,
		 opic_interrupt_destination *dest)
{
  ASSERT(dest >= opic->interrupt_destination);
  ASSERT(dest < opic->interrupt_destination + opic->nr_interrupt_destinations);
  DTRACE(opic, ("negate interrupt - intr port %d\n", dest->intr_port));
  device_interrupt_event(me, dest->intr_port, 0, NULL, 0);
}


static int
can_deliver(device *me,
	    opic_interrupt_source *source,
	    opic_interrupt_destination *dest)
{
  return (source != NULL && dest != NULL
	  && source->priority > dest->base_priority
	  && (dest->current_in_service == NULL
	      || source->priority > dest->current_in_service->priority));
}


static unsigned
deliver_pending(device *me,
		hw_opic_device *opic,
		opic_interrupt_destination *dest)
{
  ASSERT(can_deliver(me, dest->current_pending, dest));
  dest->current_in_service = dest->current_pending;
  dest->current_in_service->in_service |= dest->bit;
  if (!dest->current_pending->is_level_triggered) {
    if (dest->current_pending->is_multicast)
      dest->current_pending->pending &= ~dest->bit;
    else
      dest->current_pending->pending = 0;
  }
  dest->current_pending = NULL;
  negate_interrupt(me, opic, dest);
  return dest->current_in_service->vector;
}


typedef enum {
  pending_interrupt,
  in_service_interrupt,
} interrupt_class;

static opic_interrupt_source *
find_interrupt_for_dest(device *me,
			hw_opic_device *opic,
			opic_interrupt_destination *dest,
			interrupt_class class)
{
  int i;
  opic_interrupt_source *pending = NULL;
  for (i = 0; i < opic->nr_interrupt_sources; i++) {
    opic_interrupt_source *src = &opic->interrupt_source[i];
    /* is this a potential hit? */
    switch (class) {
    case in_service_interrupt:
      if ((src->in_service & dest->bit) == 0)
	continue;
      break;
    case pending_interrupt:
      if ((src->pending & dest->bit) == 0)
	continue;
      break;
    }
    /* see if it is the highest priority */
    if (pending == NULL)
      pending = src;
    else if (src->priority > pending->priority)
      pending = src;
  }
  return pending;
}


static opic_interrupt_destination *
find_lowest_dest(device *me, 
		 hw_opic_device *opic,
		 opic_interrupt_source *src)
{
  int i;
  opic_interrupt_destination *lowest = NULL;
  for (i = 0; i < opic->nr_interrupt_destinations; i++) {
    opic_interrupt_destination *dest = &opic->interrupt_destination[i];
    if (src->destination & dest->bit) {
      if (dest->base_priority < src->priority) {
	if (lowest == NULL)
	  lowest = dest;
	else if (lowest->base_priority > dest->base_priority)
	  lowest = dest;
	else if (lowest->current_in_service != NULL
		 && dest->current_in_service == NULL)
	  lowest = dest; /* not doing anything */
	else if (lowest->current_in_service != NULL
		 && dest->current_in_service != NULL
		 && (lowest->current_in_service->priority
		     > dest->current_in_service->priority))
	  lowest = dest; /* less urgent */
	/* FIXME - need to be more fair */	
      }
    }
  }
  return lowest;
}


static void
handle_interrupt(device *me,
		 hw_opic_device *opic,
		 opic_interrupt_source *src,
		 int asserted)
{
  if (src->is_masked) {
    DTRACE(opic, ("interrupt %d - ignore masked\n", src->nr));
  }
  else if (src->is_multicast) {
    /* always try to deliver multicast interrupts - just easier */
    int i;
    ASSERT(!src->is_level_triggered);
    ASSERT(src->is_positive_polarity);
    ASSERT(asserted);
    for (i = 0; i < opic->nr_interrupt_destinations; i++) {
      opic_interrupt_destination *dest = &opic->interrupt_destination[i];
      if (src->destination & dest->bit) {
	if (src->pending & dest->bit) {
	  DTRACE(opic, ("interrupt %d - multicast still pending to %d\n",
			src->nr, dest->nr));
	}
	else if (can_deliver(me, src, dest)) {
	  dest->current_pending = src;
	  src->pending |= dest->bit;
	  assert_interrupt(me, opic, dest);
	  DTRACE(opic, ("interrupt %d - multicast to %d\n",
			src->nr, dest->nr));
	}
	else {
	  src->pending |= dest->bit;
	  DTRACE(opic, ("interrupt %d - multicast pending to %d\n",
			src->nr, dest->nr));
	}
      }
    }
  }
  else if (src->is_level_triggered
	   && src->is_positive_polarity
	   && !asserted) {
    if (src->pending)
      DTRACE(opic, ("interrupt %d - ignore withdrawn (active high)\n",
		    src->nr));
    else
      DTRACE(opic, ("interrupt %d - ignore low level (active high)\n",
		    src->nr));
    ASSERT(!src->is_multicast);
    src->pending = 0;
  }
  else if (src->is_level_triggered
	   && !src->is_positive_polarity
	   && asserted) {
    if (src->pending)
      DTRACE(opic, ("interrupt %d - ignore withdrawn (active low)\n",
		    src->nr));
    else
      DTRACE(opic, ("interrupt %d - ignore high level (active low)\n",
		    src->nr));

    ASSERT(!src->is_multicast);
    src->pending = 0;
  }
  else if (!src->is_level_triggered
	   && src->is_positive_polarity
	   && !asserted) {
    DTRACE(opic, ("interrupt %d - ignore falling edge (positive edge trigered)\n",
		  src->nr));
  }
  else if (!src->is_level_triggered
	   && !src->is_positive_polarity
	   && asserted) {
    DTRACE(opic, ("interrupt %d - ignore rising edge (negative edge trigered)\n",
		  src->nr));
  }
  else if (src->in_service != 0) {
    /* leave the interrupt where it is */
    ASSERT(!src->is_multicast);
    ASSERT(src->pending == 0 || src->pending == src->in_service);
    src->pending = src->in_service;
    DTRACE(opic, ("interrupt %ld - ignore already in service to 0x%lx\n",
		  (long)src->nr, (long)src->in_service));
  }
  else if (src->pending != 0) {
    DTRACE(opic, ("interrupt %ld - ignore still pending to 0x%lx\n",
		  (long)src->nr, (long)src->pending));
  }
  else {
    /* delivery is needed */
    opic_interrupt_destination *dest = find_lowest_dest(me, opic, src);
    if (can_deliver(me, src, dest)) {
      dest->current_pending = src;
      src->pending = dest->bit;
      DTRACE(opic, ("interrupt %d - delivered to %d\n", src->nr, dest->nr));
      assert_interrupt(me, opic, dest);
    }
    else {
      src->pending = src->destination; /* any can take this */
      DTRACE(opic, ("interrupt %ld - pending to 0x%lx\n",
		    (long)src->nr, (long)src->pending));
    }
  }
}

static unsigned
do_interrupt_acknowledge_register_N_read(device *me,
					 hw_opic_device *opic,
					 int dest_nr)
{
  opic_interrupt_destination *dest = &opic->interrupt_destination[dest_nr];
  unsigned vector;

  ASSERT(dest_nr >= 0 && dest_nr < opic->nr_interrupt_destinations);
  ASSERT(dest_nr == dest->nr);

  /* try the current pending */
  if (can_deliver(me, dest->current_pending, dest)) {
    ASSERT(dest->current_pending->pending & dest->bit);
    vector = deliver_pending(me, opic, dest);
    DTRACE(opic, ("interrupt ack %d - entering %d (pending) - vector %d (%d), priority %d\n",
		  dest->nr,
		  dest->current_in_service->nr,
		  dest->current_in_service->vector, vector,
		  dest->current_in_service->priority));
  }
  else {
    /* try for something else */
    dest->current_pending = find_interrupt_for_dest(me, opic, dest, pending_interrupt);
    if (can_deliver(me, dest->current_pending, dest)) {
      vector = deliver_pending(me, opic, dest);    
      DTRACE(opic, ("interrupt ack %d - entering %d (not pending) - vector %d (%d), priority %d\n",
		    dest->nr,
		    dest->current_in_service->nr,
		    dest->current_in_service->vector, vector,
		    dest->current_in_service->priority));
    }
    else {
      dest->current_pending = NULL;
      vector = opic->spurious_vector;
      DTRACE(opic, ("interrupt ack %d - spurious interrupt %d\n",
		    dest->nr, vector));
    }
  }
  return vector;
}


static void
do_end_of_interrupt_register_N_write(device *me,
				     hw_opic_device *opic,
				     int dest_nr,
				     unsigned reg)
{
  opic_interrupt_destination *dest = &opic->interrupt_destination[dest_nr];

  ASSERT(dest_nr >= 0 && dest_nr < opic->nr_interrupt_destinations);
  ASSERT(dest_nr == dest->nr);

  /* check the value written is zero */
  if (reg != 0) {
    DTRACE(opic, ("eoi %d - ignoring nonzero value\n", dest->nr));
  }

  /* user doing wierd things? */
  if (dest->current_in_service == NULL) {
    DTRACE(opic, ("eoi %d - strange, no current interrupt\n", dest->nr));
    return;
  }

  /* an internal stuff up? */
  if (!(dest->current_in_service->in_service & dest->bit)) {
    device_error(me, "eoi %d - current interrupt not in service", dest->nr);
  }

  /* find what was probably the previous in service interrupt */
  dest->current_in_service->in_service &= ~dest->bit;
  DTRACE(opic, ("eoi %d - ending %d - priority %d, vector %d\n",
		dest->nr,
		dest->current_in_service->nr,
		dest->current_in_service->priority,
		dest->current_in_service->vector));
  dest->current_in_service = find_interrupt_for_dest(me, opic, dest, in_service_interrupt);
  if (dest->current_in_service != NULL)
    DTRACE(opic, ("eoi %d - resuming %d - priority %d, vector %d\n",
		  dest->nr,
		  dest->current_in_service->nr,
		  dest->current_in_service->priority,
		  dest->current_in_service->vector));
  else
    DTRACE(opic, ("eoi %d - resuming none\n", dest->nr));

  /* check to see if that shouldn't be interrupted */
  dest->current_pending = find_interrupt_for_dest(me, opic, dest, pending_interrupt);
  if (can_deliver(me, dest->current_pending, dest)) {
    ASSERT(dest->current_pending->pending & dest->bit);
    assert_interrupt(me, opic, dest);
  }
  else {
    dest->current_pending = NULL;
  }
}


static void
decode_opic_address(device *me,
		    hw_opic_device *opic,
		    int space,
		    unsigned_word address,
		    unsigned nr_bytes,
		    opic_register *type,
		    int *index)
{
  int isb = 0;

  /* is the size valid? */
  if (nr_bytes != 4) {
    *type = invalid_opic_register;
    *index = -1;
    return;
  }

  /* try for a per-processor register within the interrupt delivery
     unit */
  if (space == opic->idu.space
      && address >= (opic->idu.address + idu_per_processor_register_base)
      && address < (opic->idu.address + idu_per_processor_register_base
		    + (sizeof_idu_per_processor_register_block
		       * opic->nr_interrupt_destinations))) {
    unsigned_word block_offset = (address
				  - opic->idu.address
				  - idu_per_processor_register_base);
    unsigned_word offset = block_offset % sizeof_idu_per_processor_register_block;
    *index = block_offset / sizeof_idu_per_processor_register_block;
    switch (offset) {
    case 0x040:
      *type = ipi_N_dispatch_register;
      *index = 0;
      break;
    case 0x050:
      *type = ipi_N_dispatch_register;
      *index = 1;
      break;
    case 0x060:
      *type = ipi_N_dispatch_register;
      *index = 2;
      break;
    case 0x070:
      *type = ipi_N_dispatch_register;
      *index = 3;
      break;
    case 0x080:
      *type = current_task_priority_register_N;
      break;
    case 0x0a0:
      *type = interrupt_acknowledge_register_N;
      break;
    case 0x0b0:
      *type = end_of_interrupt_register_N;
      break;
    default:
      *type = invalid_opic_register;
      break;
    }
    DTRACE(opic, ("per-processor register %d:0x%lx - %s[%d]\n",
		  space, (unsigned long)address,
		  opic_register_name(*type),
		  *index));
    return;
  }

  /* try for an interrupt source unit */
  for (isb = 0; isb < opic->nr_isu_blocks; isb++) {
    if (opic->isu_block[isb].space == space
	&& address >= opic->isu_block[isb].address
	&& address < (opic->isu_block[isb].address + opic->isu_block[isb].size)) {
      unsigned_word block_offset = address - opic->isu_block[isb].address;
      unsigned_word offset = block_offset % sizeof_isu_register_block;
      *index = (opic->isu_block[isb].int_number
		+ (block_offset / sizeof_isu_register_block));
      switch (offset) {
      case 0x00:
	*type = interrupt_source_N_vector_priority_register;
	break;
      case 0x10:
	*type = interrupt_source_N_destination_register;
	break;
      default:
	*type = invalid_opic_register;
	break;
      }
      DTRACE(opic, ("isu register %d:0x%lx - %s[%d]\n",
		    space, (unsigned long)address,
		    opic_register_name(*type),
		    *index));
      return;
    }
  }

  /* try for a timer */
  if (space == opic->idu.space
      && address >= (opic->idu.address + idu_timer_base)
      && address < (opic->idu.address + idu_timer_base
		    + opic->nr_timer_interrupts * sizeof_timer_register_block)) {
    unsigned_word offset = address % sizeof_timer_register_block;
    *index = ((address - opic->idu.address - idu_timer_base)
	      / sizeof_timer_register_block);
    switch (offset) {
    case 0x00:
      *type = timer_N_current_count_register;
      break;
    case 0x10:
      *type = timer_N_base_count_register;
      break;
    case 0x20:
      *type = timer_N_vector_priority_register;
      break;
    case 0x30:
      *type = timer_N_destination_register;
      break;
    default:
      *type = invalid_opic_register;
      break;
    }
    DTRACE(opic, ("timer register %d:0x%lx - %s[%d]\n",
		  space, (unsigned long)address,
		  opic_register_name(*type),
		  *index));
    return;
  }

  /* finally some other misc global register */
  if (space == opic->idu.space
      && address >= opic->idu.address
      && address < opic->idu.address + opic->idu.size) {
    unsigned_word block_offset = address - opic->idu.address;
    switch (block_offset) {
    case 0x010f0:
      *type = timer_frequency_reporting_register;
      *index = -1;
      break;
    case 0x010e0:
      *type = spurious_vector_register;
      *index = -1;
      break;
    case 0x010d0:
    case 0x010c0:
    case 0x010b0:
    case 0x010a0:
      *type = ipi_N_vector_priority_register;
      *index = (block_offset - 0x010a0) / 16;
      break;
    case 0x01090:
      *type = processor_init_register;
      *index = -1;
      break;
    case 0x01080:
      *type = vendor_identification_register;
      *index = -1;
      break;
    case 0x01020:
      *type = global_configuration_register_N;
      *index = 0;
      break;
    case 0x01000:
      *type = feature_reporting_register_N;
      *index = 0;
      break;
    default:
      *type = invalid_opic_register;
      *index = -1;
      break;
    }
    DTRACE(opic, ("global register %d:0x%lx - %s[%d]\n",
		  space, (unsigned long)address,
		  opic_register_name(*type),
		  *index));
    return;
  }

  /* nothing matched */
  *type = invalid_opic_register;
  DTRACE(opic, ("invalid register %d:0x%lx\n",
		space, (unsigned long)address));
  return;
}


/* Processor init register:

   The bits in this register (one per processor) are directly wired to
   output "init" interrupt ports. */

static unsigned
do_processor_init_register_read(device *me,
				hw_opic_device *opic)
{
  unsigned reg = opic->init;
  DTRACE(opic, ("processor init register - read 0x%lx\n",
		(long)reg));
  return reg;
}

static void
do_processor_init_register_write(device *me,
				 hw_opic_device *opic,
				 unsigned reg)
{
  int i;
  for (i = 0; i < opic->nr_interrupt_destinations; i++) {
    opic_interrupt_destination *dest = &opic->interrupt_destination[i];
    if ((reg & dest->bit) != (opic->init & dest->bit)) {
      if (reg & dest->bit) {
	DTRACE(opic, ("processor init register - write 0x%lx - asserting init%d\n",
		      (long)reg, i));
	opic->init |= dest->bit;
	device_interrupt_event(me, dest->init_port, 1, NULL, 0);
      }
      else {
	DTRACE(opic, ("processor init register - write 0x%lx - negating init%d\n",
		      (long)reg, i));
	opic->init &= ~dest->bit;
	device_interrupt_event(me, dest->init_port, 0, NULL, 0);
      }
    }
  }
}



/* Interrupt Source Vector/Priority Register: */

static unsigned
read_vector_priority_register(device *me,
			      hw_opic_device *opic,
			      opic_interrupt_source *interrupt,
			      const char *reg_name,
			      int reg_index)
{
  unsigned reg;
  reg = 0;
  reg |= interrupt->is_masked;
  reg |= (interrupt->in_service || interrupt->pending
	  ? isu_active_bit : 0); /* active */
  reg |= interrupt->is_multicast;
  reg |= interrupt->is_positive_polarity;
  reg |= interrupt->is_level_triggered; /* sense? */
  reg |= interrupt->priority << isu_priority_shift;
  reg |= interrupt->vector;
  DTRACE(opic, ("%s %d vector/priority register - read 0x%lx\n",
		reg_name, reg_index, (unsigned long)reg));
  return reg;
}

static unsigned
do_interrupt_source_N_vector_priority_register_read(device *me,
						    hw_opic_device *opic,
						    int index)
{
  unsigned reg;
  ASSERT(index < opic->nr_external_interrupts);
  reg = read_vector_priority_register(me, opic,
				      &opic->interrupt_source[index],
				      "interrupt source", index);
  return reg;
}

static void
write_vector_priority_register(device *me,
			       hw_opic_device *opic,
			       opic_interrupt_source *interrupt,
			       unsigned reg,
			       const char *reg_name,
			       int reg_index)
{
  interrupt->is_masked = (reg & isu_mask_bit);
  interrupt->is_multicast = (reg & isu_multicast_bit);
  interrupt->is_positive_polarity = (reg & isu_positive_polarity_bit);
  interrupt->is_level_triggered = (reg & isu_level_triggered_bit);
  interrupt->priority = ((reg >> isu_priority_shift)
			 % max_nr_task_priorities);
  interrupt->vector = (reg & isu_vector_bits);
  DTRACE(opic, ("%s %d vector/priority register - write 0x%lx - %s%s%s-polarity, %s-triggered, priority %ld vector %ld\n",
		reg_name,
		reg_index,
		(unsigned long)reg,
		interrupt->is_masked ? "masked, " : "",
		interrupt->is_multicast ? "multicast, " : "",
		interrupt->is_positive_polarity ? "positive" : "negative",
		interrupt->is_level_triggered ? "level" : "edge",
		(long)interrupt->priority,
		(long)interrupt->vector));
}

static void
do_interrupt_source_N_vector_priority_register_write(device *me,
						     hw_opic_device *opic,
						     int index,
						     unsigned reg)
{
  ASSERT(index < opic->nr_external_interrupts);
  reg &= ~isu_multicast_bit; /* disable multicast */
  write_vector_priority_register(me, opic,
				 &opic->interrupt_source[index],
				 reg, "interrupt source", index);
}



/* Interrupt Source Destination Register: */

static unsigned
read_destination_register(device *me,
			  hw_opic_device *opic,
			  opic_interrupt_source *interrupt,
			  const char *reg_name,
			  int reg_index)
{
  unsigned long reg;
  reg = interrupt->destination;
  DTRACE(opic, ("%s %d destination register - read 0x%lx\n",
		reg_name, reg_index, reg));
  return reg;
}
			     
static unsigned
do_interrupt_source_N_destination_register_read(device *me,
						hw_opic_device *opic,
						int index)
{
  unsigned reg;
  ASSERT(index < opic->nr_external_interrupts);
  reg = read_destination_register(me, opic, &opic->external_interrupt_source[index],
				  "interrupt source", index);
  return reg;
}

static void
write_destination_register(device *me,
			   hw_opic_device *opic,
			   opic_interrupt_source *interrupt,
			   unsigned reg,
			   const char *reg_name,
			   int reg_index)
{
  reg &= (1 << opic->nr_interrupt_destinations) - 1; /* mask out invalid */
  DTRACE(opic, ("%s %d destination register - write 0x%x\n",
		reg_name, reg_index, reg));
  interrupt->destination = reg;
}

static void
do_interrupt_source_N_destination_register_write(device *me,
						 hw_opic_device *opic,
						 int index,
						 unsigned reg)
{
  ASSERT(index < opic->nr_external_interrupts);
  write_destination_register(me, opic, &opic->external_interrupt_source[index],
			     reg, "interrupt source", index);
}



/* Spurious vector register: */

static unsigned
do_spurious_vector_register_read(device *me,
				 hw_opic_device *opic)
{
  unsigned long reg = opic->spurious_vector;
  DTRACE(opic, ("spurious vector register - read 0x%lx\n", reg));
  return reg;
}

static void
do_spurious_vector_register_write(device *me,
				  hw_opic_device *opic,
				  unsigned reg)
{
  reg &= 0xff; /* mask off invalid */
  DTRACE(opic, ("spurious vector register - write 0x%x\n", reg));
  opic->spurious_vector = reg;
}



/* current task priority register: */

static unsigned
do_current_task_priority_register_N_read(device *me,
					 hw_opic_device *opic,
					 int index)
{
  opic_interrupt_destination *interrupt_destination = &opic->interrupt_destination[index];
  unsigned reg;
  ASSERT(index >= 0 && index < opic->nr_interrupt_destinations);
  reg = interrupt_destination->base_priority;
  DTRACE(opic, ("current task priority register %d - read 0x%x\n", index, reg));
  return reg;
}

static void
do_current_task_priority_register_N_write(device *me,
					  hw_opic_device *opic,
					  int index,
					  unsigned reg)
{
  opic_interrupt_destination *interrupt_destination = &opic->interrupt_destination[index];
  ASSERT(index >= 0 && index < opic->nr_interrupt_destinations);
  reg %= max_nr_task_priorities;
  DTRACE(opic, ("current task priority register %d - write 0x%x\n", index, reg));
  interrupt_destination->base_priority = reg;
}



/* Timer Frequency Reporting Register: */

static unsigned
do_timer_frequency_reporting_register_read(device *me,
					   hw_opic_device *opic)
{
  unsigned reg;
  reg = opic->timer_frequency;
  DTRACE(opic, ("timer frequency reporting register - read 0x%x\n", reg));
  return reg;
}

static void
do_timer_frequency_reporting_register_write(device *me,
					    hw_opic_device *opic,
					    unsigned reg)
{
  DTRACE(opic, ("timer frequency reporting register - write 0x%x\n", reg));
  opic->timer_frequency = reg;
}


/* timer registers: */

static unsigned
do_timer_N_current_count_register_read(device *me,
				       hw_opic_device *opic,
				       int index)
{
  opic_timer *timer = &opic->timer[index];
  unsigned reg;
  ASSERT(index >= 0 && index < opic->nr_timer_interrupts);
  if (timer->inhibited)
    reg = timer->count; /* stalled value */
  else
    reg = timer->count - device_event_queue_time(me); /* time remaining */
  DTRACE(opic, ("timer %d current count register - read 0x%x\n", index, reg));
  return reg;
}


static unsigned
do_timer_N_base_count_register_read(device *me,
				    hw_opic_device *opic,
				    int index)
{
  opic_timer *timer = &opic->timer[index];
  unsigned reg;
  ASSERT(index >= 0 && index < opic->nr_timer_interrupts);
  reg = timer->base_count;
  DTRACE(opic, ("timer %d base count register - read 0x%x\n", index, reg));
  return reg;
}


static void
timer_event(void *data)
{
  opic_timer *timer = data;
  device *me = timer->me;
  if (timer->inhibited)
    device_error(timer->me, "internal-error - timer event occured when timer %d inhibited",
		 timer->nr);
  handle_interrupt(timer->me, timer->opic, timer->interrupt_source, 1);
  timer->timeout_event = device_event_queue_schedule(me, timer->base_count,
						     timer_event, timer);
  DTRACE(opic, ("timer %d - interrupt at %ld, next at %d\n",
		timer->nr, (long)device_event_queue_time(me), timer->base_count));
}


static void
do_timer_N_base_count_register_write(device *me,
				     hw_opic_device *opic,
				     int index,
				     unsigned reg)
{
  opic_timer *timer = &opic->timer[index];
  int inhibit;
  ASSERT(index >= 0 && index < opic->nr_timer_interrupts);
  inhibit = reg & 0x80000000;
  if (timer->inhibited && !inhibit) {
    timer->inhibited = 0;
    if (timer->timeout_event != NULL)
      device_event_queue_deschedule(me, timer->timeout_event);
    timer->count = device_event_queue_time(me) + reg;
    timer->base_count = reg;
    timer->timeout_event = device_event_queue_schedule(me, timer->base_count,
						       timer_event, (void*)timer);
    DTRACE(opic, ("timer %d base count register - write 0x%x - timer started\n",
		  index, reg));
  }
  else if (!timer->inhibited && inhibit) {
    if (timer->timeout_event != NULL)
      device_event_queue_deschedule(me, timer->timeout_event);
    timer->count = timer->count - device_event_queue_time(me);
    timer->inhibited = 1;
    timer->base_count = reg;
    DTRACE(opic, ("timer %d base count register - write 0x%x - timer stopped\n",
		  index, reg));
  }
  else {
    ASSERT((timer->inhibited && inhibit) || (!timer->inhibited && !inhibit));
    DTRACE(opic, ("timer %d base count register - write 0x%x\n", index, reg));
    timer->base_count = reg;
  }
}


static unsigned
do_timer_N_vector_priority_register_read(device *me,
					 hw_opic_device *opic,
					 int index)
{
  unsigned reg;
  ASSERT(index >= 0 && index < opic->nr_timer_interrupts);
  reg = read_vector_priority_register(me, opic,
				      &opic->timer_interrupt_source[index],
				      "timer", index);
  return reg;
}

static void
do_timer_N_vector_priority_register_write(device *me,
					  hw_opic_device *opic,
					  int index,
					  unsigned reg)
{
  ASSERT(index >= 0 && index < opic->nr_timer_interrupts);
  reg &= ~isu_level_triggered_bit; /* force edge trigger */
  reg |= isu_positive_polarity_bit; /* force rising (positive) edge */
  reg |= isu_multicast_bit; /* force multicast */
  write_vector_priority_register(me, opic,
				 &opic->timer_interrupt_source[index],
				 reg, "timer", index);
}


static unsigned
do_timer_N_destination_register_read(device *me,
				     hw_opic_device *opic,
				     int index)
{
  unsigned reg;
  ASSERT(index >= 0 && index < opic->nr_timer_interrupts);
  reg = read_destination_register(me, opic, &opic->timer_interrupt_source[index],
				  "timer", index);
  return reg;
}

static void
do_timer_N_destination_register_write(device *me,
				      hw_opic_device *opic,
				      int index,
				      unsigned reg)
{
  ASSERT(index >= 0 && index < opic->nr_timer_interrupts);
  write_destination_register(me, opic, &opic->timer_interrupt_source[index],
			     reg, "timer", index);
}


/* IPI registers */

static unsigned
do_ipi_N_vector_priority_register_read(device *me,
				       hw_opic_device *opic,
				       int index)
{
  unsigned reg;
  ASSERT(index >= 0 && index < opic->nr_interprocessor_interrupts);
  reg = read_vector_priority_register(me, opic,
				      &opic->interprocessor_interrupt_source[index],
				      "ipi", index);
  return reg;
}

static void
do_ipi_N_vector_priority_register_write(device *me,
					hw_opic_device *opic,
					int index,
					unsigned reg)
{
  ASSERT(index >= 0 && index < opic->nr_interprocessor_interrupts);
  reg &= ~isu_level_triggered_bit; /* force edge trigger */
  reg |= isu_positive_polarity_bit; /* force rising (positive) edge */
  reg |= isu_multicast_bit; /* force a multicast source */
  write_vector_priority_register(me, opic,
				 &opic->interprocessor_interrupt_source[index],
				 reg, "ipi", index);
}

static void
do_ipi_N_dispatch_register_write(device *me,
				 hw_opic_device *opic,
				 int index,
				 unsigned reg)
{
  opic_interrupt_source *source = &opic->interprocessor_interrupt_source[index];
  ASSERT(index >= 0 && index < opic->nr_interprocessor_interrupts);
  DTRACE(opic, ("ipi %d interrupt dispatch register - write 0x%x\n", index, reg));
  source->destination = reg;
  handle_interrupt(me, opic, source, 1);
}


/* vendor and other global registers */

static unsigned
do_vendor_identification_register_read(device *me,
				       hw_opic_device *opic)
{
  unsigned reg;
  reg = opic->vendor_identification;
  DTRACE(opic, ("vendor identification register - read 0x%x\n", reg));
  return reg;
}

static unsigned
do_feature_reporting_register_N_read(device *me,
				     hw_opic_device *opic,
				     int index)
{
  unsigned reg = 0;
  ASSERT(index == 0);
  switch (index) {
  case 0:
    reg |= (opic->nr_external_interrupts << 16);
    reg |= (opic->nr_interrupt_destinations << 8);
    reg |= (2/*version 1.2*/);
    break;
  }
  DTRACE(opic, ("feature reporting register %d - read 0x%x\n", index, reg));
  return reg;
}

static unsigned
do_global_configuration_register_N_read(device *me,
					hw_opic_device *opic,
					int index)
{
  unsigned reg = 0;
  ASSERT(index == 0);
  switch (index) {
  case 0:
    reg |= gcr0_8259_bit; /* hardwire 8259 disabled */
    break;
  }
  DTRACE(opic, ("global configuration register %d - read 0x%x\n", index, reg));
  return reg;
}

static void
do_global_configuration_register_N_write(device *me,
					 hw_opic_device *opic,
					 int index,
					 unsigned reg)
{
  ASSERT(index == 0);
  if (reg & gcr0_reset_bit) {
    DTRACE(opic, ("global configuration register %d - write 0x%x - reseting opic\n", index, reg));
    hw_opic_init_data(me);
  }
  if (!(reg & gcr0_8259_bit)) {
    DTRACE(opic, ("global configuration register %d - write 0x%x - ignoring 8259 enable\n", index, reg));
  }
}



/* register read-write */

static unsigned
hw_opic_io_read_buffer(device *me,
		       void *dest,
		       int space,
		       unsigned_word addr,
		       unsigned nr_bytes,
		       cpu *processor,
		       unsigned_word cia)
{
  hw_opic_device *opic = (hw_opic_device*)device_data(me);
  opic_register type;
  int index;
  decode_opic_address(me, opic, space, addr, nr_bytes, &type, &index);
  if (type == invalid_opic_register) {
    device_error(me, "invalid opic read access to %d:0x%lx (%d bytes)",
		 space, (unsigned long)addr, nr_bytes);
  }
  else {
    unsigned reg;
    switch (type) {
    case processor_init_register:
      reg = do_processor_init_register_read(me, opic);
      break;
    case interrupt_source_N_vector_priority_register:
      reg = do_interrupt_source_N_vector_priority_register_read(me, opic, index);
      break;
    case interrupt_source_N_destination_register:
      reg = do_interrupt_source_N_destination_register_read(me, opic, index);
      break;
    case interrupt_acknowledge_register_N:
      reg = do_interrupt_acknowledge_register_N_read(me, opic, index);
      break;
    case spurious_vector_register:
      reg = do_spurious_vector_register_read(me, opic);
      break;
    case current_task_priority_register_N:
      reg = do_current_task_priority_register_N_read(me, opic, index);
      break;
    case timer_frequency_reporting_register:
      reg = do_timer_frequency_reporting_register_read(me, opic);
      break;
    case timer_N_current_count_register:
      reg = do_timer_N_current_count_register_read(me, opic, index);
      break;
    case timer_N_base_count_register:
      reg = do_timer_N_base_count_register_read(me, opic, index);
      break;
    case timer_N_vector_priority_register:
      reg = do_timer_N_vector_priority_register_read(me, opic, index);
      break;
    case timer_N_destination_register:
      reg = do_timer_N_destination_register_read(me, opic, index);
      break;
    case ipi_N_vector_priority_register:
      reg = do_ipi_N_vector_priority_register_read(me, opic, index);
      break;
    case feature_reporting_register_N:
      reg = do_feature_reporting_register_N_read(me, opic, index);
      break;
    case global_configuration_register_N:
      reg = do_global_configuration_register_N_read(me, opic, index);
      break;
    case vendor_identification_register:
      reg = do_vendor_identification_register_read(me, opic);
      break;
    default:
      reg = 0;
      device_error(me, "unimplemented read of register %s[%d]",
		   opic_register_name(type), index);
    }
    *(unsigned_4*)dest = H2LE_4(reg);
  }
  return nr_bytes;
}


static unsigned
hw_opic_io_write_buffer(device *me,
			const void *source,
			int space,
			unsigned_word addr,
			unsigned nr_bytes,
			cpu *processor,
			unsigned_word cia)
{
  hw_opic_device *opic = (hw_opic_device*)device_data(me);
  opic_register type;
  int index;
  decode_opic_address(me, opic, space, addr, nr_bytes, &type, &index);
  if (type == invalid_opic_register) {
    device_error(me, "invalid opic write access to %d:0x%lx (%d bytes)",
		 space, (unsigned long)addr, nr_bytes);
  }
  else {
    unsigned reg = LE2H_4(*(unsigned_4*)source);
    switch (type) {
    case processor_init_register:
      do_processor_init_register_write(me, opic, reg);
      break;
    case interrupt_source_N_vector_priority_register:
      do_interrupt_source_N_vector_priority_register_write(me, opic, index, reg);
      break;
    case interrupt_source_N_destination_register:
      do_interrupt_source_N_destination_register_write(me, opic, index, reg);
      break;
    case end_of_interrupt_register_N:
      do_end_of_interrupt_register_N_write(me, opic, index, reg);
      break;
    case spurious_vector_register:
      do_spurious_vector_register_write(me, opic, reg);
      break;
    case current_task_priority_register_N:
      do_current_task_priority_register_N_write(me, opic, index, reg);
      break;
    case timer_frequency_reporting_register:
      do_timer_frequency_reporting_register_write(me, opic, reg);
      break;
    case timer_N_base_count_register:
      do_timer_N_base_count_register_write(me, opic, index, reg);
      break;
    case timer_N_vector_priority_register:
      do_timer_N_vector_priority_register_write(me, opic, index, reg);
      break;
    case timer_N_destination_register:
      do_timer_N_destination_register_write(me, opic, index, reg);
      break;
    case ipi_N_dispatch_register:
      do_ipi_N_dispatch_register_write(me, opic, index, reg);
      break;
    case ipi_N_vector_priority_register:
      do_ipi_N_vector_priority_register_write(me, opic, index, reg);
      break;
    case global_configuration_register_N:
      do_global_configuration_register_N_write(me, opic, index, reg);
      break;
    default:
      device_error(me, "unimplemented write to register %s[%d]",
		   opic_register_name(type), index);
    }
  }
  return nr_bytes;
}
  
  
static void
hw_opic_interrupt_event(device *me,
			int my_port,
			device *source,
			int source_port,
			int level,
			cpu *processor,
			unsigned_word cia)
{
  hw_opic_device *opic = (hw_opic_device*)device_data(me);

  int isb;
  int src_nr = 0;

  /* find the corresponding internal input port */
  for (isb = 0; isb < opic->nr_isu_blocks; isb++) {
    if (my_port >= opic->isu_block[isb].int_number
	&& my_port < opic->isu_block[isb].int_number + opic->isu_block[isb].range) {
      src_nr += my_port - opic->isu_block[isb].int_number;
      break;
    }
    else
      src_nr += opic->isu_block[isb].range;
  }
  if (isb == opic->nr_isu_blocks)
    device_error(me, "interrupt %d out of range", my_port);
  DTRACE(opic, ("external-interrupt %d, internal %d, level %d\n",
		my_port, src_nr, level));

  /* pass it on */
  ASSERT(src_nr >= 0 && src_nr < opic->nr_external_interrupts);
  handle_interrupt(me, opic, &opic->external_interrupt_source[src_nr], level);
}


static const device_interrupt_port_descriptor hw_opic_interrupt_ports[] = {
  { "irq", 0, max_nr_interrupt_sources, input_port, },
  { "intr", 0, max_nr_interrupt_destinations, output_port, },
  { "init", max_nr_interrupt_destinations, max_nr_interrupt_destinations, output_port, },
  { NULL }
};


static device_callbacks const hw_opic_callbacks = {
  { generic_device_init_address,
    hw_opic_init_data },
  { NULL, }, /* address */
  { hw_opic_io_read_buffer,
    hw_opic_io_write_buffer }, /* IO */
  { NULL, }, /* DMA */
  { hw_opic_interrupt_event, NULL, hw_opic_interrupt_ports }, /* interrupt */
  { NULL, }, /* unit */
  NULL, /* instance */
};

static void *
hw_opic_create(const char *name,
	       const device_unit *unit_address,
	       const char *args)
{
  hw_opic_device *opic = ZALLOC(hw_opic_device);
  return opic;
}



const device_descriptor hw_opic_device_descriptor[] = {
  { "opic", hw_opic_create, &hw_opic_callbacks },
  { NULL },
};

#endif /* _HW_OPIC_C_ */
