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


#ifndef _HW_CPU_C_
#define _HW_CPU_C_

#ifndef STATIC_INLINE_HW_CPU
#define STATIC_INLINE_HW_CPU STATIC_INLINE
#endif

#include "device_table.h"
#include "hw_cpu.h"

#include "interrupts.h"
#include "cpu.h"


/* DEVICE


   cpu - Interface to a Processor


   DESCRIPTION


   The CPU device provides the connection between the interrupt net
   (linking the devices and the interrupt controller) and the
   simulated model of each processor.  This device contains interrupt
   ports that correspond directly to the external interrupt stimulus
   that can be sent to a given processor.  Sending an interrupt to one
   of the ports results in an interrupt being delivered to the
   corresponding processor.

   Typically, an interrupt controller would have its inputs connected
   to device interrupt sources and its outputs (sreset, int, et.al.)
   connected to this device.


   PROPERTIES


   cpu-nr = <integer> (required)


   Specify the processor (1..N) that this cpu device node should
   control.


   EXAMPLES

   
   Connect an OpenPIC interrupt controller interrupt ports to
   processor zero.

   | -o '/phb/opic@0 > irq0 int /cpus/cpu@0' \
   | -o '/phb/opic@0 > init hreset /cpus/cpu@0' \


   */

typedef struct _hw_cpu_device {
  int cpu_nr;
  cpu *processor;
} hw_cpu_device;

static const device_interrupt_port_descriptor hw_cpu_interrupt_ports[] = {
  { "hreset", hw_cpu_hard_reset },
  { "sreset", hw_cpu_soft_reset },
  { "int", hw_cpu_external_interrupt },
  { "mci", hw_cpu_machine_check_interrupt },
  { "smi", hw_cpu_system_management_interrupt },
  { NULL }
};


static void *
hw_cpu_create(const char *name,
	      const device_unit *unit_address,
	      const char *args)
{
  hw_cpu_device *hw_cpu = ZALLOC(hw_cpu_device);
  return hw_cpu;
}


/* during address initialization ensure that any missing cpu
   properties are added to this devices node */

static void
hw_cpu_init_address(device *me)
{
  hw_cpu_device *hw_cpu = (hw_cpu_device*)device_data(me);
  /* populate the node with properties */
  /* clear our data */
  memset(hw_cpu, 0x0, sizeof(hw_cpu_device));
  hw_cpu->cpu_nr = device_find_integer_property(me, "cpu-nr");
  hw_cpu->processor = psim_cpu(device_system(me), hw_cpu->cpu_nr);
}


/* Take the interrupt and synchronize its delivery with the clock.  If
   we've not yet scheduled an interrupt for the next clock tick, take
   the oportunity to do it now */

static void
hw_cpu_interrupt_event(device *me,
		       int my_port,
		       device *source,
		       int source_port,
		       int level,
		       cpu *processor,
		       unsigned_word cia)
{
  hw_cpu_device *hw_cpu = (hw_cpu_device*)device_data(me);
  if (my_port < 0 || my_port >= hw_cpu_nr_interrupt_ports)
    error("hw_cpu_interrupt_event_callback: interrupt port out of range %d\n",
	  my_port);
  switch (my_port) {
    /*case hw_cpu_hard_reset:*/
    /*case hw_cpu_soft_reset:*/
  case hw_cpu_external_interrupt:
    external_interrupt(hw_cpu->processor, level);
    break;
    /*case hw_cpu_machine_check_interrupt:*/
  default:
    error("hw_cpu_deliver_interrupt: unimplemented interrupt port %d\n",
	  my_port);
    break;
  }
}


static device_callbacks const hw_cpu_callbacks = {
  { hw_cpu_init_address, }, /* init */
  { NULL, }, /* address */
  { NULL, }, /* io */
  { NULL, }, /* DMA */
  { hw_cpu_interrupt_event, NULL, hw_cpu_interrupt_ports }, /* interrupts */
  { NULL, NULL, },
};

const device_descriptor hw_cpu_device_descriptor[] = {
  { "hw-cpu", hw_cpu_create, &hw_cpu_callbacks },
  { "cpu", hw_cpu_create, &hw_cpu_callbacks },
  { NULL, },
};

#endif /* _HW_CPU_C_ */
