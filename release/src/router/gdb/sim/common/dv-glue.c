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

#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif

/* DEVICE
   

   glue - glue to interconnect and test hardware ports
   

   DESCRIPTION
   

   The glue device provides two functions.  Firstly, it provides a
   mechanism for inspecting and driving the port network.  Secondly,
   it provides a set of boolean primitives that can be used to apply
   combinatorial operations to the port network.

   Glue devices have a variable number of big endian <<output>>
   registers.  Each register is target-word sized.  The registers can
   be read and written.

   Writing to an output register results in an event being driven
   (level determined by the value written) on the devices
   corresponding output port.

   Reading an <<output>> register returns either the last value
   written or the most recently computed value (for that register) as
   a result of an event ariving on that port (which ever was computed
   last).

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
   live.  The address must be 2048 * sizeof (word) (8k in a 32bit
   simulation) aligned.


   interrupt-ranges = <int-number> <range> (optional)

   If present, this specifies the number of valid interrupt inputs (up
   to the maximum of 2048).  By default, <<int-number>> is zero and
   range is determined by the <<reg>> size.


   PORTS

   
   int[0..] (input, output)

   Both an input and an output port.


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

   Additional devices such as a D flip-flop (DFF), an inverter (INV)
   or a latch (LAT) may prove useful.

   */


enum {
  max_nr_ports = 2048,
};

enum hw_glue_type {
  glue_undefined = 0,
  glue_io,
  glue_and,
  glue_nand,
  glue_or,
  glue_xor,
  glue_nor,
  glue_not,
};

struct hw_glue {
  enum hw_glue_type type;
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
};


static hw_io_read_buffer_method hw_glue_io_read_buffer;
static hw_io_write_buffer_method hw_glue_io_write_buffer;
static hw_port_event_method hw_glue_port_event;
const static struct hw_port_descriptor hw_glue_ports[];

static void
hw_glue_finish (struct hw *me)
{
  struct hw_glue *glue = HW_ZALLOC (me, struct hw_glue);
  
  /* establish our own methods */
  set_hw_data (me, glue);
  set_hw_io_read_buffer (me, hw_glue_io_read_buffer);
  set_hw_io_write_buffer (me, hw_glue_io_write_buffer);
  set_hw_ports (me, hw_glue_ports);
  set_hw_port_event (me, hw_glue_port_event);

  /* attach to our parent bus */
  do_hw_attach_regs (me);
  
  /* establish the output registers */
  {
    reg_property_spec unit;
    int reg_nr;
    /* find a relevant reg entry */
    reg_nr = 0;
    while (hw_find_reg_array_property (me, "reg", reg_nr, &unit)
	   && !hw_unit_size_to_attach_size (hw_parent (me),
					    &unit.size,
					    &glue->sizeof_output,
					    me))
      reg_nr++;
    /* check out the size */
    if (glue->sizeof_output == 0)
      hw_abort (me, "at least one reg property size must be nonzero");
    if (glue->sizeof_output % sizeof (unsigned_word) != 0)
      hw_abort (me, "reg property size must be %ld aligned",
		(long) sizeof (unsigned_word));
    /* and the address */
    hw_unit_address_to_attach_address (hw_parent (me),
				       &unit.address,
				       &glue->space,
				       &glue->address,
				       me);
    if (glue->address % (sizeof (unsigned_word) * max_nr_ports) != 0)
      hw_abort (me, "reg property address must be %ld aligned",
		(long) (sizeof (unsigned_word) * max_nr_ports));
    glue->nr_outputs = glue->sizeof_output / sizeof (unsigned_word);
    glue->output = hw_zalloc (me, glue->sizeof_output);
  }
  
  /* establish the input ports */
  {
    const struct hw_property *ranges;
    ranges = hw_find_property (me, "interrupt-ranges");
    if (ranges == NULL)
      {
	glue->int_number = 0;
	glue->nr_inputs = glue->nr_outputs;
      }
    else if (ranges->sizeof_array != sizeof (unsigned_cell) * 2)
      {
	hw_abort (me, "invalid interrupt-ranges property (incorrect size)");
      }
    else
      {
	const unsigned_cell *int_range = ranges->array;
	glue->int_number = BE2H_cell (int_range[0]);
	glue->nr_inputs = BE2H_cell (int_range[1]);
      }
    glue->sizeof_input = glue->nr_inputs * sizeof (unsigned);
    glue->input = hw_zalloc (me, glue->sizeof_input);
  }
  
  /* determine our type */
  {
    const char *name = hw_name(me);
    if (strcmp (name, "glue") == 0)
      glue->type = glue_io;
    else if (strcmp (name, "glue-and") == 0)
      glue->type = glue_and;
    else
      hw_abort (me, "unimplemented glue type");
  }
  
  HW_TRACE ((me, "int-number %d, nr_inputs %d, nr_outputs %d",
	     glue->int_number, glue->nr_inputs, glue->nr_outputs));
}

static unsigned
hw_glue_io_read_buffer (struct hw *me,
			void *dest,
			int space,
			unsigned_word addr,
			unsigned nr_bytes)
{
  struct hw_glue *glue = (struct hw_glue *) hw_data (me);
  int reg = ((addr - glue->address) / sizeof (unsigned_word)) % glue->nr_outputs;
  if (nr_bytes != sizeof (unsigned_word)
      || (addr % sizeof (unsigned_word)) != 0)
    hw_abort (me, "missaligned read access (%d:0x%lx:%d) not supported",
	      space, (unsigned long)addr, nr_bytes);
  *(unsigned_word*)dest = H2BE_4(glue->output[reg]);
  HW_TRACE ((me, "read - port %d (0x%lx), level %d",
	     reg, (unsigned long) addr, glue->output[reg]));
  return nr_bytes;
}


static unsigned
hw_glue_io_write_buffer (struct hw *me,
			 const void *source,
			 int space,
			 unsigned_word addr,
			 unsigned nr_bytes)
{
  struct hw_glue *glue = (struct hw_glue *) hw_data (me);
  int reg = ((addr - glue->address) / sizeof (unsigned_word)) % max_nr_ports;
  if (nr_bytes != sizeof (unsigned_word)
      || (addr % sizeof (unsigned_word)) != 0)
    hw_abort (me, "missaligned write access (%d:0x%lx:%d) not supported",
	      space, (unsigned long) addr, nr_bytes);
  glue->output[reg] = H2BE_4 (*(unsigned_word*)source);
  HW_TRACE ((me, "write - port %d (0x%lx), level %d",
	     reg, (unsigned long) addr, glue->output[reg]));
  hw_port_event (me, reg, glue->output[reg]);
  return nr_bytes;
}

static void
hw_glue_port_event (struct hw *me,
		    int my_port,
		    struct hw *source,
		    int source_port,
		    int level)
{
  struct hw_glue *glue = (struct hw_glue *) hw_data (me);
  int i;
  if (my_port < glue->int_number
      || my_port >= glue->int_number + glue->nr_inputs)
    hw_abort (me, "port %d outside of valid range", my_port);
  glue->input[my_port - glue->int_number] = level;
  switch (glue->type)
    {
    case glue_io:
      {
	int port = my_port % glue->nr_outputs;
	glue->output[port] = level;
	HW_TRACE ((me, "input - port %d (0x%lx), level %d",
		   my_port,
		   (unsigned long) glue->address + port * sizeof (unsigned_word),
		   level));
	break;
      }
    case glue_and:
      {
	glue->output[0] = glue->input[0];
	for (i = 1; i < glue->nr_inputs; i++)
	  glue->output[0] &= glue->input[i];
	HW_TRACE ((me, "and - port %d, level %d arrived - output %d",
		   my_port, level, glue->output[0]));
	hw_port_event (me, 0, glue->output[0]);
	break;
      }
    default:
      {
	hw_abort (me, "operator not implemented");
	break;
      }
    }
}


static const struct hw_port_descriptor hw_glue_ports[] = {
  { "int", 0, max_nr_ports },
  { NULL }
};


const struct hw_descriptor dv_glue_descriptor[] = {
  { "glue", hw_glue_finish, },
  { "glue-and", hw_glue_finish, },
  { "glue-nand", hw_glue_finish, },
  { "glue-or", hw_glue_finish, },
  { "glue-xor", hw_glue_finish, },
  { "glue-nor", hw_glue_finish, },
  { "glue-not", hw_glue_finish, },
  { NULL },
};
