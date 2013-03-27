/*  This file is part of the program GDB, the GNU debugger.
    
    Copyright (C) 1998, 2007 Free Software Foundation, Inc.
    Contributed by Cygnus Solutions.
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
    
    */

#include "sim-main.h"
#include "hw-main.h"

/* DEVICE

   
   mn103iop - mn103002 I/O ports 0-3.

   
   DESCRIPTION
   
   Implements the mn103002 i/o ports as described in the mn103002 user guide.


   PROPERTIES   

   reg = <ioport-addr> <ioport-size> ...


   BUGS

   */


/* The I/O ports' registers' address block */

struct mn103iop_block {
  unsigned_word base;
  unsigned_word bound;
};



enum io_port_register_types {
  P0OUT,
  P1OUT,
  P2OUT,
  P3OUT,
  P0MD,
  P1MD,
  P2MD,
  P3MD,
  P2SS,
  P4SS,
  P0DIR,
  P1DIR,
  P2DIR,
  P3DIR,
  P0IN,
  P1IN,
  P2IN,
  P3IN,
};

#define NR_PORTS  4

enum {
  OUTPUT_BLOCK,
  MODE_BLOCK,
  DED_CTRL_BLOCK,
  CTRL_BLOCK,
  PIN_BLOCK,
  NR_BLOCKS
};

typedef struct _mn10300_ioport {
  unsigned8 output, output_mode, control, pin;
  struct hw_event *event;
} mn10300_ioport;



struct mn103iop {
  struct mn103iop_block block[NR_BLOCKS];
  mn10300_ioport port[NR_PORTS];
  unsigned8      p2ss, p4ss;
};


/* Finish off the partially created hw device.  Attach our local
   callbacks.  Wire up our port names etc */

static hw_io_read_buffer_method mn103iop_io_read_buffer;
static hw_io_write_buffer_method mn103iop_io_write_buffer;

static void
attach_mn103iop_regs (struct hw *me,
		      struct mn103iop *io_port)
{
  int i;
  unsigned_word attach_address;
  int attach_space;
  unsigned attach_size;
  reg_property_spec reg;

  if (hw_find_property (me, "reg") == NULL)
    hw_abort (me, "Missing \"reg\" property");

  for (i=0; i < NR_BLOCKS; ++i )
    {
      if (!hw_find_reg_array_property (me, "reg", i, &reg))
	hw_abort (me, "\"reg\" property must contain five addr/size entries");
      hw_unit_address_to_attach_address (hw_parent (me),
					 &reg.address,
					 &attach_space,
					 &attach_address,
					 me);
      io_port->block[i].base = attach_address;
      hw_unit_size_to_attach_size (hw_parent (me),
				   &reg.size,
				   &attach_size, me);
      io_port->block[i].bound = attach_address + (attach_size - 1);
      hw_attach_address (hw_parent (me),
			 0,
			 attach_space, attach_address, attach_size,
			 me);
    }
}

static void
mn103iop_finish (struct hw *me)
{
  struct mn103iop *io_port;
  int i;

  io_port = HW_ZALLOC (me, struct mn103iop);
  set_hw_data (me, io_port);
  set_hw_io_read_buffer (me, mn103iop_io_read_buffer);
  set_hw_io_write_buffer (me, mn103iop_io_write_buffer);

  /* Attach ourself to our parent bus */
  attach_mn103iop_regs (me, io_port);

  /* Initialize the i/o port registers. */
  for ( i=0; i<NR_PORTS; ++i )
    {
      io_port->port[i].output = 0;
      io_port->port[i].output_mode = 0;
      io_port->port[i].control = 0;
      io_port->port[i].pin = 0;
    }
  io_port->port[2].output_mode = 0xff;
  io_port->p2ss = 0;
  io_port->p4ss = 0x0f;
}


/* read and write */

static int
decode_addr (struct hw *me,
	     struct mn103iop *io_port,
	     unsigned_word address)
{
  unsigned_word offset;
  offset = address - io_port->block[0].base;
  switch (offset)
    {
    case 0x00: return P0OUT;
    case 0x01: return P1OUT;
    case 0x04: return P2OUT;
    case 0x05: return P3OUT;
    case 0x20: return P0MD;
    case 0x21: return P1MD;
    case 0x24: return P2MD;
    case 0x25: return P3MD;
    case 0x44: return P2SS;
    case 0x48: return P4SS;
    case 0x60: return P0DIR;
    case 0x61: return P1DIR;
    case 0x64: return P2DIR;
    case 0x65: return P3DIR;
    case 0x80: return P0IN;
    case 0x81: return P1IN;
    case 0x84: return P2IN;
    case 0x85: return P3IN;
    default: 
      {
	hw_abort (me, "bad address");
	return -1;
      }
    }
}


static void
read_output_reg (struct hw *me,
		 struct mn103iop *io_port,
		 unsigned_word io_port_reg,
		 const void *dest,
		 unsigned  nr_bytes)
{
  if ( nr_bytes == 1 )
    {
      *(unsigned8 *)dest = io_port->port[io_port_reg].output;
    }
  else
    {
      hw_abort (me, "bad read size of %d bytes from P%dOUT.", nr_bytes, 
		io_port_reg);
    }
}


static void
read_output_mode_reg (struct hw *me,
		      struct mn103iop *io_port,
		      unsigned_word io_port_reg,
		      const void *dest,
		      unsigned  nr_bytes)
{
  if ( nr_bytes == 1 )
    {
      /* check if there are fields which can't be written and
	 take appropriate action depending what bits are set */
      *(unsigned8 *)dest = io_port->port[io_port_reg].output_mode;
    }
  else
    {
      hw_abort (me, "bad read size of %d bytes to P%dMD.", nr_bytes, 
		io_port_reg);
    }
}


static void
read_control_reg (struct hw *me,
		  struct mn103iop *io_port,
		  unsigned_word io_port_reg,
		  const void *dest,
		  unsigned  nr_bytes)
{
  if ( nr_bytes == 1 )
    {
      *(unsigned8 *)dest = io_port->port[io_port_reg].control;
    }
  else
    {
      hw_abort (me, "bad read size of %d bytes to P%dDIR.", nr_bytes, 
		io_port_reg);
    }
}


static void
read_pin_reg (struct hw *me,
	      struct mn103iop *io_port,
	      unsigned_word io_port_reg,
	      const void *dest,
	      unsigned  nr_bytes)
{
  if ( nr_bytes == 1 )
    {
      *(unsigned8 *)dest = io_port->port[io_port_reg].pin;
    }
  else
    {
      hw_abort (me, "bad read size of %d bytes to P%dIN.", nr_bytes, 
		io_port_reg);
    }
}


static void
read_dedicated_control_reg (struct hw *me,
			    struct mn103iop *io_port,
			    unsigned_word io_port_reg,
			    const void *dest,
			    unsigned  nr_bytes)
{
  if ( nr_bytes == 1 )
    {
      /* select on io_port_reg: */
      if ( io_port_reg == P2SS )
	{
	  *(unsigned8 *)dest = io_port->p2ss;
	}
      else
	{
	  *(unsigned8 *)dest = io_port->p4ss;
	}
    }
  else
    {
      hw_abort (me, "bad read size of %d bytes to PSS.", nr_bytes); 
    }
}


static unsigned
mn103iop_io_read_buffer (struct hw *me,
			 void *dest,
			 int space,
			 unsigned_word base,
			 unsigned nr_bytes)
{
  struct mn103iop *io_port = hw_data (me);
  enum io_port_register_types io_port_reg;
  HW_TRACE ((me, "read 0x%08lx %d", (long) base, (int) nr_bytes));

  io_port_reg = decode_addr (me, io_port, base);
  switch (io_port_reg)
    {
    /* Port output registers */
    case P0OUT:
    case P1OUT:
    case P2OUT:
    case P3OUT:
      read_output_reg(me, io_port, io_port_reg-P0OUT, dest, nr_bytes);
      break;

    /* Port output mode registers */
    case P0MD:
    case P1MD:
    case P2MD:
    case P3MD:
      read_output_mode_reg(me, io_port, io_port_reg-P0MD, dest, nr_bytes);
      break;

    /* Port control registers */
    case P0DIR:
    case P1DIR:
    case P2DIR:
    case P3DIR:
      read_control_reg(me, io_port, io_port_reg-P0DIR, dest, nr_bytes);
      break;

    /* Port pin registers */
    case P0IN:
    case P1IN:
    case P2IN:
      read_pin_reg(me, io_port, io_port_reg-P0IN, dest, nr_bytes);
      break;

    case P2SS:
    case P4SS:
      read_dedicated_control_reg(me, io_port, io_port_reg, dest, nr_bytes);
      break;

    default:
      hw_abort(me, "invalid address");
    }

  return nr_bytes;
}     


static void
write_output_reg (struct hw *me,
		  struct mn103iop *io_port,
		  unsigned_word io_port_reg,
		  const void *source,
		  unsigned  nr_bytes)
{
  unsigned8 buf = *(unsigned8 *)source;
  if ( nr_bytes == 1 )
    {
      if ( io_port_reg == 3 && (buf & 0xfc) != 0 )
	{
	  hw_abort(me, "Cannot write to read-only bits of P3OUT.");
	}
      else
	{
	  io_port->port[io_port_reg].output = buf;
	}
    }
  else
    {
      hw_abort (me, "bad read size of %d bytes from P%dOUT.", nr_bytes, 
		io_port_reg);
    }
}


static void
write_output_mode_reg (struct hw *me,
		       struct mn103iop *io_port,
		       unsigned_word io_port_reg,
		       const void *source,
		       unsigned  nr_bytes)
{
  unsigned8 buf = *(unsigned8 *)source;
  if ( nr_bytes == 1 )
    {
      /* check if there are fields which can't be written and
	 take appropriate action depending what bits are set */
      if ( ( io_port_reg == 3 && (buf & 0xfc) != 0 )
	   || ( (io_port_reg == 0 || io_port_reg == 1)  && (buf & 0xfe) != 0 ) )
	{
	  hw_abort(me, "Cannot write to read-only bits of output mode register.");
	}
      else
	{
	  io_port->port[io_port_reg].output_mode = buf;
	}
    }
  else
    {
      hw_abort (me, "bad write size of %d bytes to P%dMD.", nr_bytes, 
		io_port_reg);
    }
}


static void
write_control_reg (struct hw *me,
		   struct mn103iop *io_port,
		   unsigned_word io_port_reg,
		   const void *source,
		   unsigned  nr_bytes)
{
  unsigned8 buf = *(unsigned8 *)source;
  if ( nr_bytes == 1 )
    {
      if ( io_port_reg == 3 && (buf & 0xfc) != 0 )
	{
	  hw_abort(me, "Cannot write to read-only bits of P3DIR.");
	}
      else
	{
	  io_port->port[io_port_reg].control = buf;
	}
    }
  else
    {
      hw_abort (me, "bad write size of %d bytes to P%dDIR.", nr_bytes, 
		io_port_reg);
    }
}


static void
write_dedicated_control_reg (struct hw *me,
			     struct mn103iop *io_port,
			     unsigned_word io_port_reg,
			     const void *source,
			     unsigned  nr_bytes)
{
  unsigned8 buf = *(unsigned8 *)source;
  if ( nr_bytes == 1 )
    {
      /* select on io_port_reg: */
      if ( io_port_reg == P2SS )
	{
	  if ( (buf && 0xfc)  != 0 )
	    {
	      hw_abort(me, "Cannot write to read-only bits in p2ss.");
	    }
	  else
	    {
	      io_port->p2ss = buf;
	    }
	}
      else
	{
	  if ( (buf && 0xf0) != 0 )
	    {
	      hw_abort(me, "Cannot write to read-only bits in p4ss.");
	    }
	  else
	    {
	      io_port->p4ss = buf;
	    }
	}
    }
  else
    {
      hw_abort (me, "bad write size of %d bytes to PSS.", nr_bytes); 
    }
}


static unsigned
mn103iop_io_write_buffer (struct hw *me,
			  const void *source,
			  int space,
			  unsigned_word base,
			  unsigned nr_bytes)
{
  struct mn103iop *io_port = hw_data (me);
  enum io_port_register_types io_port_reg;
  HW_TRACE ((me, "write 0x%08lx %d", (long) base, (int) nr_bytes));

  io_port_reg = decode_addr (me, io_port, base);
  switch (io_port_reg)
    {
    /* Port output registers */
    case P0OUT:
    case P1OUT:
    case P2OUT:
    case P3OUT:
      write_output_reg(me, io_port, io_port_reg-P0OUT, source, nr_bytes);
      break;

    /* Port output mode registers */
    case P0MD:
    case P1MD:
    case P2MD:
    case P3MD:
      write_output_mode_reg(me, io_port, io_port_reg-P0MD, source, nr_bytes);
      break;

    /* Port control registers */
    case P0DIR:
    case P1DIR:
    case P2DIR:
    case P3DIR:
      write_control_reg(me, io_port, io_port_reg-P0DIR, source, nr_bytes);
      break;

    /* Port pin registers */
    case P0IN:
    case P1IN:
    case P2IN:
      hw_abort(me, "Cannot write to pin register.");
      break;

    case P2SS:
    case P4SS:
      write_dedicated_control_reg(me, io_port, io_port_reg, source, nr_bytes);
      break;

    default:
      hw_abort(me, "invalid address");
    }

  return nr_bytes;
}     


const struct hw_descriptor dv_mn103iop_descriptor[] = {
  { "mn103iop", mn103iop_finish, },
  { NULL },
};
