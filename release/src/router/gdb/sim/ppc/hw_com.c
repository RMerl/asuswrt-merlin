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


#ifndef _HW_COM_C_
#define _HW_COM_C_

#ifndef STATIC_INLINE_HW_COM
#define STATIC_INLINE_HW_COM STATIC_INLINE
#endif

#include "device_table.h"

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
   

   com - '550 compatible serial device
   

   DESCRIPTION
   

   Models the basics of the 8 register '550 serial device.  The model 
   includes an interrupt line, input and output fifos, and status 
   information.

   Independent configuration of the devices input and output streams is 
   allowed: use either the console or a file (buffered or unbuffered) as 
   the data source/sink; specify the real-time delay between each character 
   transfer.

   When the devices input stream is being taken from a file, the end of 
   file is signaled by a loss of carrier (the loss of carrier may be 
   incorrectly proceeded by a single null character).
   

   PROPERTIES
   

   reg = <address> <size> ... (optional - note 1)

   List of <address> <size> pairs.  Each pair specifies an address for
   the devices 8 registers.  The address should be 8 byte aligned.


   alternate-reg = <address> <size> ... (optional - note 1)

   Alternative addreses for the registers.


   assigned-addresses = <address> <size> ... (optional - note 1)
   
   On a PCI bus, this property specifies the addresses assigned to the 
   device.  The values reflect the devices configuration base registers.
   
   Note 1: At least one of "assigned-addresses", "reg" or "alternative-reg" 
   must be specified.  If "assigned-addresses" is specified the other 
   address specifications are ignored.
   
   
   input-file = <file-name> (optional)

   File to take all serial port input from (instead of the simulation
   console).


   output-file = <file-name> (optional)

   File to send all output to (instead of the simulation console).


   input-buffering = "unbuffered" (optional)

   Specifying "unbuffered" buffering disables buffering on the serial
   devices input stream (all data is immediatly read).  In the future,
   this option may be used to provide input buffering alternatives.


   output-buffering = "unbuffered" (optional)

   Specifying "unbuffered" buffering disables buffering on the serial 
   devices output stream (all data is immediatly written).  In the future, 
   this option may be extended to include other buffering alternatives.


   input-delay = <integer-delay> (optional)

   Specify the number of ticks after the current character has been
   read from the serial port that the next character becomes
   available.


   output-delay = <integer-delay> (optional)

   Specify the number of ticks after a character has been written to
   the empty output fifo that the fifo finishes draining.  Any
   characters written to the output fifo before it has drained will
   not be lost and will still be displayed.


   EXAMPLES


   |  /iobus@0xf0000000/com@0x3000/reg 0x3000 8

   Create a simple console device at address <<0x3000>> within
   <<iobus>>.  Since iobus starts at address <<0xf0000000>> the
   absolute address of the serial port will be <<0xf0003000>>.

   The device will always be ready for I/O (no delay properties specified) 
   and both the input and output streams will use the simulation console 
   (no file properties).


   |  $ psim \
   |    -o '/cpus/cpu@0' \
   |    -o '/iobus@0xf0000000/com@0x4000/reg 0x4000 8' \
   |    -o '/iobus@0xf0000000/com@0x4000/input-file /etc/passwd' \
   |    -o '/iobus@0xf0000000/com@0x4000/input-delay 1000' \
   |    -o '/iobus@0xf0000000/com@0x4000 > 0 int /cpus/cpu@0x0' \
   |    psim-test/hw-com/cat.be 0xf0004000

   The serial port (at address <<0xf0004000>> is configured so that it
   takes its input from the file <</etc/passwd>> while its output is
   allowed to appear on the simulation console.
   
   The node <</cpus/cpu@0>> was explicitly specified to ensure that it had 
   been created before any interrupts were attached to it.

   The program <<psim-test/hw-com/cat>> copies any characters on the serial 
   port's input (<</etc/passwd>>) to its output (the console).  
   Consequently, the aove program will display the contents of the file 
   <</etc/passwd>> on the screen.


   BUGS


   IEEE 1275 requires that a device on a PCI bus have, as its first reg 
   entry, the address of its configuration space registers.  Currently, 
   this device does not even implement configuration registers.
   
   This model does not attempt to model the '550's input and output fifos.  
   Instead, the input fifo is limited to a single character at a time, 
   while the output fifo is effectivly infinite.  Consequently, unlike the 
   '550, this device will not discard output characters once a stream of 16 
   have been written to the data output register.

   The input and output can only be taken from a file (or the current 
   terminal device).  In the future, the <<com>> device should allow the 
   specification of other data streams (such as an xterm or TK window).

   The input blocks if no data is available.

   Interrupts have not been tested.

   */

enum {
  max_hw_com_registers = 8,
};

typedef struct _com_port {
  int ready;
  int delay;
  int interrupting;
  FILE *file;
} com_port;

typedef struct _com_modem {
  int carrier;
  int carrier_changed;
  int interrupting;
} com_modem;

typedef struct _hw_com_device {
  com_port input;
  com_port output;
  com_modem modem;
  char dlab[2];
  char reg[max_hw_com_registers];
  int interrupting;
} hw_com_device;


static void
hw_com_device_init_data(device *me)
{
  hw_com_device *com = (hw_com_device*)device_data(me);
  /* clean up */
  if (com->output.file != NULL)
    fclose(com->output.file);
  if (com->input.file != NULL)
    fclose(com->input.file);
  memset(com, 0, sizeof(hw_com_device));

  /* the fifo speed */
  com->output.delay = (device_find_property(me, "output-delay") != NULL
		       ? device_find_integer_property(me, "output-delay")
		       : 0);
  com->input.delay = (device_find_property(me, "input-delay") != NULL
		      ? device_find_integer_property(me, "input-delay")
		      : 0);

  /* the data source/sink */
  if (device_find_property(me, "input-file") != NULL) {
    const char *input_file = device_find_string_property(me, "input-file");
    com->input.file = fopen(input_file, "r");
    if (com->input.file == NULL)
      device_error(me, "Problem opening input file %s\n", input_file);
    if (device_find_property(me, "input-buffering") != NULL) {
      const char *buffering = device_find_string_property(me, "input-buffering");
      if (strcmp(buffering, "unbuffered") == 0)
	setbuf(com->input.file, NULL);
    }
  }
  if (device_find_property(me, "output-file") != NULL) {
    const char *output_file = device_find_string_property(me, "output-file");
    com->output.file = fopen(output_file, "w");
    if (com->output.file == NULL)
      device_error(me, "Problem opening output file %s\n", output_file);
    if (device_find_property(me, "output-buffering") != NULL) {
      const char *buffering = device_find_string_property(me, "output-buffering");
      if (strcmp(buffering, "unbuffered") == 0)
	setbuf(com->output.file, NULL);
    }
  }

  /* ready from the start */
  com->input.ready = 1;
  com->modem.carrier = 1;
  com->output.ready = 1;
}


static void
update_com_interrupts(device *me,
		      hw_com_device *com)
{
  int interrupting;
  com->modem.interrupting = (com->modem.carrier_changed && (com->reg[1] & 0x80));
  com->input.interrupting = (com->input.ready && (com->reg[1] & 0x1));
  com->output.interrupting = (com->output.ready && (com->reg[1] & 0x2));
  interrupting = (com->input.interrupting
		  || com->output.interrupting
		  || com->modem.interrupting);

  if (interrupting) {
    if (!com->interrupting) {
      device_interrupt_event(me, 0 /*port*/, 1 /*value*/, NULL, 0);
    }
  }
  else /*!interrupting*/ {
    if (com->interrupting)
      device_interrupt_event(me, 0 /*port*/, 0 /*value*/, NULL, 0);
  }
  com->interrupting = interrupting;
}


static void
make_read_ready(void *data)
{
  device *me = (device*)data;
  hw_com_device *com = (hw_com_device*)device_data(me);
  com->input.ready = 1;
  update_com_interrupts(me, com);
}

static void
read_com(device *me,
	 hw_com_device *com,
	 unsigned_word a,
	 char val[1])
{
  unsigned_word addr = a % 8;

  /* the divisor latch is special */
  if (com->reg[3] & 0x8 && addr < 2) {
    *val = com->dlab[addr];
    return;
  }

  switch (addr) {
  
  case 0:
    /* fifo */
    if (!com->modem.carrier)
      *val = '\0';
    if (com->input.ready) {
      /* read the char in */
      if (com->input.file == NULL) {
	if (sim_io_read_stdin(val, 1) < 0)
	  com->modem.carrier_changed = 1;
      }
      else {
	if (fread(val, 1, 1, com->input.file) == 0)
	  com->modem.carrier_changed = 1;
      }
      /* setup for next read */
      if (com->modem.carrier_changed) {
	/* once lost carrier, never ready */
	com->modem.carrier = 0;
	com->input.ready = 0;
	*val = '\0';
      }
      else if (com->input.delay > 0) {
	com->input.ready = 0;
	device_event_queue_schedule(me, com->input.delay, make_read_ready, me);
      }
    }
    else {
      /* discard it? */
      /* overflow input fifo? */
      *val = '\0';
    }
    break;

  case 2:
    /* interrupt ident */
    if (com->interrupting) {
      if (com->input.interrupting)
	*val = 0x4;
      else if (com->output.interrupting)
	*val = 0x2;
      else if (com->modem.interrupting == 0)
	*val = 0;
      else
	device_error(me, "bad elif for interrupts\n");
    }
    else
      *val = 0x1;
    break;

  case 5:
    /* line status */
    *val = ((com->input.ready ? 0x1 : 0)
	    | (com->output.ready ? 0x60 : 0)
	    );
    break;

  case 6:
    /* modem status */
    *val = ((com->modem.carrier_changed ? 0x08 : 0)
	    | (com->modem.carrier ? 0x80 : 0)
	    );
    com->modem.carrier_changed = 0;
    break;

  default:
    *val = com->reg[addr];
    break;

  }
  update_com_interrupts(me, com);
}

static unsigned
hw_com_io_read_buffer_callback(device *me,
			       void *dest,
			       int space,
			       unsigned_word addr,
			       unsigned nr_bytes,
			       cpu *processor,
			       unsigned_word cia)
{
  hw_com_device *com = device_data(me);
  int i;
  for (i = 0; i < nr_bytes; i++) {
    read_com(me, com, addr + i, &((char*)dest)[i]);
  }
  return nr_bytes;
}


static void
make_write_ready(void *data)
{
  device *me = (device*)data;
  hw_com_device *com = (hw_com_device*)device_data(me);
  com->output.ready = 1;
  update_com_interrupts(me, com);
}

static void
write_com(device *me,
	  hw_com_device *com,
	  unsigned_word a,
	  char val)
{
  unsigned_word addr = a % 8;

  /* the divisor latch is special */
  if (com->reg[3] & 0x8 && addr < 2) {
    com->dlab[addr] = val;
    return;
  }

  switch (addr) {
  
  case 0:
    /* fifo */
    if (com->output.file == NULL) {
      sim_io_write_stdout(&val, 1);
    }
    else {
      fwrite(&val, 1, 1, com->output.file);
    }
    /* setup for next write */
    if (com->output.ready && com->output.delay > 0) {
      com->output.ready = 0;
      device_event_queue_schedule(me, com->output.delay, make_write_ready, me);
    }
    break;

  default:
    com->reg[addr] = val;
    break;

  }
  update_com_interrupts(me, com);
}

static unsigned
hw_com_io_write_buffer_callback(device *me,
				const void *source,
				int space,
				unsigned_word addr,
				unsigned nr_bytes,
				cpu *processor,
				unsigned_word cia)
{
  hw_com_device *com = device_data(me);
  int i;
  for (i = 0; i < nr_bytes; i++) {
    write_com(me, com, addr + i, ((char*)source)[i]);
  }
  return nr_bytes;
}


/* instances of the hw_com device */

static void
hw_com_instance_delete(device_instance *instance)
{
  /* nothing to delete, the hw_com is attached to the device */
  return;
}

static int
hw_com_instance_read(device_instance *instance,
		     void *buf,
		     unsigned_word len)
{
  device *me = device_instance_device(instance);
  hw_com_device *com = device_data(me);
  if (com->input.file == NULL)
    return sim_io_read_stdin(buf, len);
  else {
    return fread(buf, 1, len, com->input.file);
  }
}

static int
hw_com_instance_write(device_instance *instance,
		      const void *buf,
		      unsigned_word len)
{
  device *me = device_instance_device(instance);
  hw_com_device *com = device_data(me);
  if (com->output.file == NULL)
    return sim_io_write_stdout(buf, len);
  else {
    return fwrite(buf, 1, len, com->output.file);
  }
}

static const device_instance_callbacks hw_com_instance_callbacks = {
  hw_com_instance_delete,
  hw_com_instance_read,
  hw_com_instance_write,
};

static device_instance *
hw_com_create_instance(device *me,
		       const char *path,
		       const char *args)
{
  /* point an instance directly at the device */
  return device_create_instance_from(me, NULL,
				     device_data(me),
				     path, args,
				     &hw_com_instance_callbacks);
}


static device_callbacks const hw_com_callbacks = {
  { generic_device_init_address,
    hw_com_device_init_data },
  { NULL, }, /* address */
  { hw_com_io_read_buffer_callback,
      hw_com_io_write_buffer_callback, },
  { NULL, }, /* DMA */
  { NULL, }, /* interrupt */
  { NULL, }, /* unit */
  hw_com_create_instance,
};


static void *
hw_com_create(const char *name,
	      const device_unit *unit_address,
	      const char *args)
{
  /* create the descriptor */
  hw_com_device *hw_com = ZALLOC(hw_com_device);
  return hw_com;
}


const device_descriptor hw_com_device_descriptor[] = {
  { "com", hw_com_create, &hw_com_callbacks },
  { NULL },
};

#endif /* _HW_COM_C_ */
