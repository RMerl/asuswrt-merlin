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


#ifndef _HW_GLUE_C_
#define _HW_GLUE_C_

#include "device_table.h"


/* DEVICE
   

   glue - glue to interconnect and test interrupts
   

   DESCRIPTION
   

   The glue device provides two functions.  Firstly, it provides a
   mechanism for inspecting and driving the interrupt net.  Secondly,
   it provides a set of boolean primitives that can be used add
   combinatorial operations to the interrupt network.

   Glue devices have a variable number of big endian <<output>>
   registers.  Each host-word size.  The registers can be both read
   and written.

   Writing a value to an output register causes an interrupt (of the
   specified level) to be driven on the devices corresponding output
   interrupt port.

   Reading an <<output>> register returns either the last value
   written or the most recently computed value (for that register) as
   a result of an interrupt ariving (which ever was computed last).

   At present the following sub device types are available:

   <<glue>>: In addition to driving its output interrupt port with any
   value written to an interrupt input port is stored in the
   corresponding <<output>> register.  Such input interrupts, however,
   are not propogated to an output interrupt port.

   <<glue-and>>: The bit-wise AND of the interrupt inputs is computed
   and then both stored in <<output>> register zero and propogated to
   output interrupt output port zero.


   PROPERTIES
   

   reg = <address> <size> (required)

   Specify the address (within the parent bus) that this device is to
   live.  The address must be 2048 * sizeof(word) (8k in a 32bit
   simulation) aligned.


   interrupt-ranges = <int-number> <range> (optional)

   If present, this specifies the number of valid interrupt inputs (up
   to the maximum of 2048).  By default, <<int-number>> is zero and
   range is determined by the <<reg>> size.


   EXAMPLES


   Enable tracing of the device:

   | -t glue-device \


   Create source, bitwize-and, and sink glue devices.  Since the
   device at address <<0x10000>> is of size <<8>> it will have two
   output interrupt ports.

   | -o '/iobus@0xf0000000/glue@0x10000/reg 0x10000 8' \
   | -o '/iobus@0xf0000000/glue-and@0x20000/reg 0x20000 4' \
   | -o '/iobus@0xf0000000/glue-and/interrupt-ranges 0 2' \
   | -o '/iobus@0xf0000000/glue@0x30000/reg 0x30000 4' \


   Wire the two source interrupts to the AND device:

   | -o '/iobus@0xf0000000/glue@0x10000 > 0 0 /iobus/glue-and' \
   | -o '/iobus@0xf0000000/glue@0x10000 > 1 1 /iobus/glue-and' \


   Wire the AND device up to the sink so that the and's output is not
   left open.

   | -o '/iobus@0xf0000000/glue-and > 0 0 /iobus/glue@0x30000' \


   With the above configuration.  The client program is able to
   compute a two bit AND.  For instance the <<C>> stub below prints 1
   AND 0.

   |  unsigned *input = (void*)0xf0010000;
   |  unsigned *output = (void*)0xf0030000;
   |  unsigned ans;
   |  input[0] = htonl(1);
   |  input[1] = htonl(0);
   |  ans = ntohl(*output);
   |  write_string("AND is ");
   |  write_int(ans);
   |  write_line();
   

   BUGS

   
   A future implementation of this device may support multiple
   interrupt ranges.

   Some of the devices listed may not yet be fully implemented.

   Additional devices such as a dff, an inverter or a latch may be
   useful.

   */


enum {
  max_nr_interrupts = 2048,
};

typedef enum _hw_glue_type {
  glue_undefined = 0,
  glue_io,
  glue_and,
  glue_nand,
  glue_or,
  glue_xor,
  glue_nor,
  glue_not,
} hw_glue_type;

typedef struct _hw_glue_device {
  hw_glue_type type;
  int int_number;
  int *input;
  int nr_inputs;
  unsigned sizeof_input;
  /* our output registers */
  int space;
  unsigned_word address;
  unsigned sizeof_output;
  int *output;
  int nr_outputs;
} hw_glue_device;


static void
hw_glue_init_address(device *me)
{
  hw_glue_device *glue = (hw_glue_device*)device_data(me);

  /* attach to my parent */
  generic_device_init_address(me);

  /* establish the output registers */
  if (glue->output != NULL) {
    memset(glue->output, 0, glue->sizeof_output);
  }
  else {
    reg_property_spec unit;
    int reg_nr;
    /* find a relevant reg entry */
    reg_nr = 0;
    while (device_find_reg_array_property(me, "reg", reg_nr, &unit)
	   && !device_size_to_attach_size(device_parent(me), &unit.size,
					  &glue->sizeof_output, me))
      reg_nr++;
    /* check out the size */
    if (glue->sizeof_output == 0)
      device_error(me, "at least one reg property size must be nonzero");
    if (glue->sizeof_output % sizeof(unsigned_word) != 0)
      device_error(me, "reg property size must be %d aligned", sizeof(unsigned_word));
    /* and the address */
    device_address_to_attach_address(device_parent(me),
				     &unit.address, &glue->space, &glue->address,
				     me);
    if (glue->address % (sizeof(unsigned_word) * max_nr_interrupts) != 0)
      device_error(me, "reg property address must be %d aligned",
		   sizeof(unsigned_word) * max_nr_interrupts);
    glue->nr_outputs = glue->sizeof_output / sizeof(unsigned_word);
    glue->output = zalloc(glue->sizeof_output);
  }

  /* establish the input interrupt ports */
  if (glue->input != NULL) {
    memset(glue->input, 0, glue->sizeof_input);
  }
  else {
    const device_property *ranges = device_find_property(me, "interrupt-ranges");
    if (ranges == NULL) {
      glue->int_number = 0;
      glue->nr_inputs = glue->nr_outputs;
    }
    else if (ranges->sizeof_array != sizeof(unsigned_cell) * 2) {
      device_error(me, "invalid interrupt-ranges property (incorrect size)");
    }
    else {
      const unsigned_cell *int_range = ranges->array;
      glue->int_number = BE2H_cell(int_range[0]);
      glue->nr_inputs = BE2H_cell(int_range[1]);
    }
    glue->sizeof_input = glue->nr_inputs * sizeof(unsigned);
    glue->input = zalloc(glue->sizeof_input);
  }

  /* determine our type */
  if (glue->type == glue_undefined) {
    const char *name = device_name(me);
    if (strcmp(name, "glue") == 0)
      glue->type = glue_io;
    else if (strcmp(name, "glue-and") == 0)
      glue->type = glue_and;
    else
      device_error(me, "unimplemented glue type");
  }

  DTRACE(glue, ("int-number %d, nr_inputs %d, nr_outputs %d\n",
		glue->int_number, glue->nr_inputs, glue->nr_outputs));
}

static unsigned
hw_glue_io_read_buffer_callback(device *me,
				void *dest,
				int space,
				unsigned_word addr,
				unsigned nr_bytes,
				cpu *processor,
				unsigned_word cia)
{
  hw_glue_device *glue = (hw_glue_device*)device_data(me);
  int reg = ((addr - glue->address) / sizeof(unsigned_word)) % glue->nr_outputs;
  if (nr_bytes != sizeof(unsigned_word)
      || (addr % sizeof(unsigned_word)) != 0)
     device_error(me, "missaligned read access (%d:0x%lx:%d) not supported",
		  space, (unsigned long)addr, nr_bytes);
  *(unsigned_word*)dest = H2BE_4(glue->output[reg]);
  DTRACE(glue, ("read - interrupt %d (0x%lx), level %d\n",
		reg, (unsigned long) addr, glue->output[reg]));
  return nr_bytes;
}


static unsigned
hw_glue_io_write_buffer_callback(device *me,
				 const void *source,
				 int space,
				 unsigned_word addr,
				 unsigned nr_bytes,
				 cpu *processor,
				 unsigned_word cia)
{
  hw_glue_device *glue = (hw_glue_device*)device_data(me);
  int reg = ((addr - glue->address) / sizeof(unsigned_word)) % max_nr_interrupts;
  if (nr_bytes != sizeof(unsigned_word)
      || (addr % sizeof(unsigned_word)) != 0)
    device_error(me, "missaligned write access (%d:0x%lx:%d) not supported",
		 space, (unsigned long)addr, nr_bytes);
  glue->output[reg] = H2BE_4(*(unsigned_word*)source);
  DTRACE(glue, ("write - interrupt %d (0x%lx), level %d\n",
		reg, (unsigned long) addr, glue->output[reg]));
  device_interrupt_event(me, reg, glue->output[reg], processor, cia);
  return nr_bytes;
}

static void
hw_glue_interrupt_event(device *me,
			int my_port,
			device *source,
			int source_port,
			int level,
			cpu *processor,
			unsigned_word cia)
{
  hw_glue_device *glue = (hw_glue_device*)device_data(me);
  int i;
  if (my_port < glue->int_number
      || my_port >= glue->int_number + glue->nr_inputs)
    device_error(me, "interrupt %d outside of valid range", my_port);
  glue->input[my_port - glue->int_number] = level;
  switch (glue->type) {
  case glue_io:
    {
      int port = my_port % glue->nr_outputs;
      glue->output[port] = level;
      DTRACE(glue, ("input - interrupt %d (0x%lx), level %d\n",
		    my_port,
		    (unsigned long)glue->address + port * sizeof(unsigned_word),
		    level));
      break;
    }
  case glue_and:
    glue->output[0] = glue->input[0];
    for (i = 1; i < glue->nr_inputs; i++)
      glue->output[0] &= glue->input[i];
    DTRACE(glue, ("and - interrupt %d, level %d arrived - output %d\n",
		  my_port, level, glue->output[0]));
    device_interrupt_event(me, 0, glue->output[0], processor, cia);
    break;
  default:
    device_error(me, "operator not implemented");
    break;
  }
}


static const device_interrupt_port_descriptor hw_glue_interrupt_ports[] = {
  { "int", 0, max_nr_interrupts },
  { NULL }
};


static device_callbacks const hw_glue_callbacks = {
  { hw_glue_init_address, NULL },
  { NULL, }, /* address */
  { hw_glue_io_read_buffer_callback,
      hw_glue_io_write_buffer_callback, },
  { NULL, }, /* DMA */
  { hw_glue_interrupt_event, NULL, hw_glue_interrupt_ports }, /* interrupt */
  { NULL, }, /* unit */
  NULL, /* instance */
};


static void *
hw_glue_create(const char *name,
	      const device_unit *unit_address,
	      const char *args)
{
  /* create the descriptor */
  hw_glue_device *glue = ZALLOC(hw_glue_device);
  return glue;
}


const device_descriptor hw_glue_device_descriptor[] = {
  { "glue", hw_glue_create, &hw_glue_callbacks },
  { "glue-and", hw_glue_create, &hw_glue_callbacks },
  { "glue-nand", hw_glue_create, &hw_glue_callbacks },
  { "glue-or", hw_glue_create, &hw_glue_callbacks },
  { "glue-xor", hw_glue_create, &hw_glue_callbacks },
  { "glue-nor", hw_glue_create, &hw_glue_callbacks },
  { "glue-not", hw_glue_create, &hw_glue_callbacks },
  { NULL },
};

#endif /* _HW_GLUE_C_ */
