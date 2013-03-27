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


#ifndef _HW_PAL_C_
#define _HW_PAL_C_

#ifndef STATIC_INLINE_HW_PAL
#define STATIC_INLINE_HW_PAL STATIC_INLINE
#endif

#include "device_table.h"

#include "cpu.h"

#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif


/* DEVICE

   
   pal - glue logic device containing assorted junk

   
   DESCRIPTION

   
   Typical hardware dependant hack.  This device allows the firmware
   to gain access to all the things the firmware needs (but the OS
   doesn't).

   The pal contains the following registers.  Except for the interrupt
   level register, each of the below is 8 bytes in size and must be
   accessed using correct alignment.  For 16 and 32 bit accesses the
   bytes not directed to the register are ignored:
   
   |0	reset register (write)
   |4	processor id register (read)
   |8	interrupt port (write)
   |9	interrupt level (write)
   |12	processor count register (read)
   |16	tty input fifo register (read)
   |20	tty input status register (read)
   |24	tty output fifo register (write)
   |28	tty output status register (read)

   Reset register (write) halts the simulator exiting with the
   value written.
   
   Processor id register (read) returns the processor number (0
   .. N-1) of the processor performing the read.
   
   The interrupt registers should be accessed as a pair (using a 16 or
   32 bit store).  The low byte specifies the interrupt port while the
   high byte specifies the level to drive that port at.  By
   convention, the pal's interrupt ports (int0, int1, ...) are wired
   up to the corresponding processor's level sensative external
   interrupt pin.  Eg: A two byte write to address 8 of 0x0102
   (big-endian) will result in processor 2's external interrupt pin to
   be asserted.

   Processor count register (read) returns the total number of
   processors active in the current simulation.

   TTY input fifo register (read), if the TTY input status register
   indicates a character is available by being nonzero, returns the
   next available character from the pal's tty input port.

   Similarly, the TTY output fifo register (write), if the TTY output
   status register indicates the output fifo is not full by being
   nonzero, outputs the character written to the tty's output port.


   PROPERTIES
   

   reg = <address> <size> (required)

   Specify the address (within the parent bus) that this device is to
   live.


   */


enum {
  hw_pal_reset_register = 0x0,
  hw_pal_cpu_nr_register = 0x4,
  hw_pal_int_register = 0x8,
  hw_pal_nr_cpu_register = 0xa,
  hw_pal_read_fifo = 0x10,
  hw_pal_read_status = 0x14,
  hw_pal_write_fifo = 0x18,
  hw_pal_write_status = 0x1a,
  hw_pal_address_mask = 0x1f,
};


typedef struct _hw_pal_console_buffer {
  char buffer;
  int status;
} hw_pal_console_buffer;

typedef struct _hw_pal_device {
  hw_pal_console_buffer input;
  hw_pal_console_buffer output;
  device *disk;
} hw_pal_device;


/* check the console for an available character */
static void
scan_hw_pal(hw_pal_device *hw_pal)
{
  char c;
  int count;
  count = sim_io_read_stdin(&c, sizeof(c));
  switch (count) {
  case sim_io_not_ready:
  case sim_io_eof:
    hw_pal->input.buffer = 0;
    hw_pal->input.status = 0;
    break;
  default:
    hw_pal->input.buffer = c;
    hw_pal->input.status = 1;
  }
}

/* write the character to the hw_pal */
static void
write_hw_pal(hw_pal_device *hw_pal,
	     char val)
{
  sim_io_write_stdout(&val, 1);
  hw_pal->output.buffer = val;
  hw_pal->output.status = 1;
}


static unsigned
hw_pal_io_read_buffer_callback(device *me,
			       void *dest,
			       int space,
			       unsigned_word addr,
			       unsigned nr_bytes,
			       cpu *processor,
			       unsigned_word cia)
{
  hw_pal_device *hw_pal = (hw_pal_device*)device_data(me);
  unsigned_1 val;
  switch (addr & hw_pal_address_mask) {
  case hw_pal_cpu_nr_register:
    val = cpu_nr(processor);
    DTRACE(pal, ("read - cpu-nr %d\n", val));
    break;
  case hw_pal_nr_cpu_register:
    val = tree_find_integer_property(me, "/openprom/options/smp");
    DTRACE(pal, ("read - nr-cpu %d\n", val));
    break;
  case hw_pal_read_fifo:
    val = hw_pal->input.buffer;
    DTRACE(pal, ("read - input-fifo %d\n", val));
    break;
  case hw_pal_read_status:
    scan_hw_pal(hw_pal);
    val = hw_pal->input.status;
    DTRACE(pal, ("read - input-status %d\n", val));
    break;
  case hw_pal_write_fifo:
    val = hw_pal->output.buffer;
    DTRACE(pal, ("read - output-fifo %d\n", val));
    break;
  case hw_pal_write_status:
    val = hw_pal->output.status;
    DTRACE(pal, ("read - output-status %d\n", val));
    break;
  default:
    val = 0;
    DTRACE(pal, ("read - ???\n"));
  }
  memset(dest, 0, nr_bytes);
  *(unsigned_1*)dest = val;
  return nr_bytes;
}


static unsigned
hw_pal_io_write_buffer_callback(device *me,
				const void *source,
				int space,
				unsigned_word addr,
				unsigned nr_bytes,
				cpu *processor,
				unsigned_word cia)
{
  hw_pal_device *hw_pal = (hw_pal_device*)device_data(me);
  unsigned_1 *byte = (unsigned_1*)source;
  
  switch (addr & hw_pal_address_mask) {
  case hw_pal_reset_register:
    cpu_halt(processor, cia, was_exited, byte[0]);
    break;
  case hw_pal_int_register:
    device_interrupt_event(me,
			   byte[0], /*port*/
			   (nr_bytes > 1 ? byte[1] : 0), /* val */
			   processor, cia);
    break;
  case hw_pal_read_fifo:
    hw_pal->input.buffer = byte[0];
    DTRACE(pal, ("write - input-fifo %d\n", byte[0]));
    break;
  case hw_pal_read_status:
    hw_pal->input.status = byte[0];
    DTRACE(pal, ("write - input-status %d\n", byte[0]));
    break;
  case hw_pal_write_fifo:
    write_hw_pal(hw_pal, byte[0]);
    DTRACE(pal, ("write - output-fifo %d\n", byte[0]));
    break;
  case hw_pal_write_status:
    hw_pal->output.status = byte[0];
    DTRACE(pal, ("write - output-status %d\n", byte[0]));
    break;
  }
  return nr_bytes;
}


/* instances of the hw_pal device */

static void
hw_pal_instance_delete_callback(device_instance *instance)
{
  /* nothing to delete, the hw_pal is attached to the device */
  return;
}

static int
hw_pal_instance_read_callback(device_instance *instance,
			      void *buf,
			      unsigned_word len)
{
  DITRACE(pal, ("read - %s (%ld)", (const char*)buf, (long int)len));
  return sim_io_read_stdin(buf, len);
}

static int
hw_pal_instance_write_callback(device_instance *instance,
			       const void *buf,
			       unsigned_word len)
{
  int i;
  const char *chp = buf;
  hw_pal_device *hw_pal = device_instance_data(instance);
  DITRACE(pal, ("write - %s (%ld)", (const char*)buf, (long int)len));
  for (i = 0; i < len; i++)
    write_hw_pal(hw_pal, chp[i]);
  sim_io_flush_stdoutput();
  return i;
}

static const device_instance_callbacks hw_pal_instance_callbacks = {
  hw_pal_instance_delete_callback,
  hw_pal_instance_read_callback,
  hw_pal_instance_write_callback,
};

static device_instance *
hw_pal_create_instance(device *me,
		       const char *path,
		       const char *args)
{
  return device_create_instance_from(me, NULL,
				     device_data(me),
				     path, args,
				     &hw_pal_instance_callbacks);
}

static const device_interrupt_port_descriptor hw_pal_interrupt_ports[] = {
  { "int", 0, MAX_NR_PROCESSORS },
  { NULL }
};


static void
hw_pal_attach_address(device *me,
		      attach_type attach,
		      int space,
		      unsigned_word addr,
		      unsigned nr_bytes,
		      access_type access,
		      device *client)
{
  hw_pal_device *pal = (hw_pal_device*)device_data(me);
  pal->disk = client;
}


static device_callbacks const hw_pal_callbacks = {
  { generic_device_init_address, },
  { hw_pal_attach_address, }, /* address */
  { hw_pal_io_read_buffer_callback,
      hw_pal_io_write_buffer_callback, },
  { NULL, }, /* DMA */
  { NULL, NULL, hw_pal_interrupt_ports }, /* interrupt */
  { generic_device_unit_decode,
    generic_device_unit_encode,
    generic_device_address_to_attach_address,
    generic_device_size_to_attach_size },
  hw_pal_create_instance,
};


static void *
hw_pal_create(const char *name,
	      const device_unit *unit_address,
	      const char *args)
{
  /* create the descriptor */
  hw_pal_device *hw_pal = ZALLOC(hw_pal_device);
  hw_pal->output.status = 1;
  hw_pal->output.buffer = '\0';
  hw_pal->input.status = 0;
  hw_pal->input.buffer = '\0';
  return hw_pal;
}


const device_descriptor hw_pal_device_descriptor[] = {
  { "pal", hw_pal_create, &hw_pal_callbacks },
  { NULL },
};

#endif /* _HW_PAL_C_ */
