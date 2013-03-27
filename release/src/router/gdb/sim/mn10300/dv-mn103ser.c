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
#include "dv-sockser.h"


/* DEVICE

   
   mn103ser - mn103002 serial devices 0, 1 and 2.

   
   DESCRIPTION
   
   Implements the mn103002 serial interfaces as described in the
   mn103002 user guide. 


   PROPERTIES   

   reg = <serial-addr> <serial-size>


   BUGS

   */


/* The serial devices' registers' address block */

struct mn103ser_block {
  unsigned_word base;
  unsigned_word bound;
};



enum serial_register_types {
    SC0CTR,
    SC1CTR,
    SC2CTR,
    SC0ICR,
    SC1ICR,
    SC2ICR,
    SC0TXB,
    SC1TXB,
    SC2TXB,
    SC0RXB,
    SC1RXB,
    SC2RXB,
    SC0STR,
    SC1STR,
    SC2STR,
    SC2TIM,
};


/* Access dv-sockser state */
extern char* sockser_addr;
#define USE_SOCKSER_P (sockser_addr != NULL)


#define NR_SERIAL_DEVS  3
#define SIO_STAT_RRDY 0x0010

typedef struct _mn10300_serial {
  unsigned16 status, control;
  unsigned8  txb, rxb, intmode;
  struct hw_event *event;
} mn10300_serial;



struct mn103ser {
  struct mn103ser_block block;
  mn10300_serial device[NR_SERIAL_DEVS];
  unsigned8      serial2_timer_reg;
  do_hw_poll_read_method *reader;
};

/* output port ID's */

/* for mn103002 */
enum {
  SERIAL0_RECEIVE,
  SERIAL1_RECEIVE,
  SERIAL2_RECEIVE,
  SERIAL0_SEND,
  SERIAL1_SEND,
  SERIAL2_SEND,
};


static const struct hw_port_descriptor mn103ser_ports[] = {

  { "serial-0-receive",  SERIAL0_RECEIVE, 0, output_port, },
  { "serial-1-receive",  SERIAL1_RECEIVE, 0, output_port, },
  { "serial-2-receive",  SERIAL2_RECEIVE, 0, output_port, },
  { "serial-0-transmit", SERIAL0_SEND, 0, output_port, },
  { "serial-1-transmit", SERIAL1_SEND, 0, output_port, },
  { "serial-2-transmit", SERIAL2_SEND, 0, output_port, },

  { NULL, },
};



/* Finish off the partially created hw device.  Attach our local
   callbacks.  Wire up our port names etc */

static hw_io_read_buffer_method mn103ser_io_read_buffer;
static hw_io_write_buffer_method mn103ser_io_write_buffer;

static void
attach_mn103ser_regs (struct hw *me,
		      struct mn103ser *serial)
{
  unsigned_word attach_address;
  int attach_space;
  unsigned attach_size;
  reg_property_spec reg;

  if (hw_find_property (me, "reg") == NULL)
    hw_abort (me, "Missing \"reg\" property");

  if (!hw_find_reg_array_property (me, "reg", 0, &reg))
    hw_abort (me, "\"reg\" property must contain three addr/size entries");
  hw_unit_address_to_attach_address (hw_parent (me),
				     &reg.address,
				     &attach_space,
				     &attach_address,
				     me);
  serial->block.base = attach_address;
  hw_unit_size_to_attach_size (hw_parent (me),
			       &reg.size,
			       &attach_size, me);
  serial->block.bound = attach_address + (attach_size - 1);
  hw_attach_address (hw_parent (me),
		     0,
		     attach_space, attach_address, attach_size,
		     me);
}

static void
mn103ser_finish (struct hw *me)
{
  struct mn103ser *serial;
  int i;

  serial = HW_ZALLOC (me, struct mn103ser);
  set_hw_data (me, serial);
  set_hw_io_read_buffer (me, mn103ser_io_read_buffer);
  set_hw_io_write_buffer (me, mn103ser_io_write_buffer);
  set_hw_ports (me, mn103ser_ports);

  /* Attach ourself to our parent bus */
  attach_mn103ser_regs (me, serial);

  /* If so configured, enable polled input */
  if (hw_find_property (me, "poll?") != NULL
      && hw_find_boolean_property (me, "poll?"))
    {
      serial->reader = sim_io_poll_read;
    }
  else
    {
      serial->reader = sim_io_read;
    }

  /* Initialize the serial device registers. */
  for ( i=0; i<NR_SERIAL_DEVS; ++i )
    {
      serial->device[i].txb = 0;
      serial->device[i].rxb = 0;
      serial->device[i].status = 0;
      serial->device[i].control = 0;
      serial->device[i].intmode = 0;
      serial->device[i].event = NULL;
    }
}


/* read and write */

static int
decode_addr (struct hw *me,
	     struct mn103ser *serial,
	     unsigned_word address)
{
  unsigned_word offset;
  offset = address - serial->block.base;
  switch (offset)
    {
    case 0x00: return SC0CTR;
    case 0x04: return SC0ICR;
    case 0x08: return SC0TXB;
    case 0x09: return SC0RXB;
    case 0x0C: return SC0STR;
    case 0x10: return SC1CTR;
    case 0x14: return SC1ICR;
    case 0x18: return SC1TXB;
    case 0x19: return SC1RXB;
    case 0x1C: return SC1STR;
    case 0x20: return SC2CTR;
    case 0x24: return SC2ICR;
    case 0x28: return SC2TXB;
    case 0x29: return SC2RXB;
    case 0x2C: return SC2STR;
    case 0x2D: return SC2TIM;
    default: 
      {
	hw_abort (me, "bad address");
	return -1;
      }
    }
}

static void
do_polling_event (struct hw *me,
		  void *data)
{
  struct mn103ser *serial = hw_data(me);
  long serial_reg = (long) data;
  char c;
  int count;

  if(USE_SOCKSER_P)
    {
      int rd;
      rd = dv_sockser_read (hw_system (me));
      if(rd != -1)
	{
	  c = (char) rd;
	  count = 1;
	}
      else
	{
	  count = HW_IO_NOT_READY;
	}
    }
  else
    {
      count = do_hw_poll_read (me, serial->reader,
			       0/*STDIN*/, &c, sizeof(c));
    }


  switch (count)
    {
    case HW_IO_NOT_READY:
    case HW_IO_EOF:
      serial->device[serial_reg].rxb = 0;
      serial->device[serial_reg].status &= ~SIO_STAT_RRDY;
      break;
    default:
      serial->device[serial_reg].rxb = c;
      serial->device[serial_reg].status |= SIO_STAT_RRDY;
      hw_port_event (me, serial_reg+SERIAL0_RECEIVE, 1);
    }

  /* Schedule next polling event */
  serial->device[serial_reg].event
    = hw_event_queue_schedule (me, 1000,
			       do_polling_event, (void *)serial_reg);

}

static void
read_control_reg (struct hw *me,
		  struct mn103ser *serial,
		  unsigned_word serial_reg,
		  void *dest,
		  unsigned  nr_bytes)
{
  /* really allow 1 byte read, too */
  if ( nr_bytes == 2 )
    {
      *(unsigned16 *)dest = H2LE_2 (serial->device[serial_reg].control);
    }
  else
    {
      hw_abort (me, "bad read size of %d bytes from SC%dCTR.", nr_bytes, 
		serial_reg);
    }
}


static void
read_intmode_reg (struct hw *me,
		  struct mn103ser *serial,
		  unsigned_word serial_reg,
		  void *dest,
		  unsigned  nr_bytes)
{
  if ( nr_bytes == 1 )
    {
      *(unsigned8 *)dest = serial->device[serial_reg].intmode;
    }
  else
    {
      hw_abort (me, "bad read size of %d bytes from SC%dICR.", nr_bytes, 
		serial_reg);
    }
}


static void
read_txb (struct hw *me,
	  struct mn103ser *serial,
	  unsigned_word serial_reg,
	  void *dest,
	  unsigned  nr_bytes)
{
  if ( nr_bytes == 1 )
    {
      *(unsigned8 *)dest = serial->device[serial_reg].txb;
    }
  else
    {
      hw_abort (me, "bad read size of %d bytes from SC%dTXB.", nr_bytes, 
		serial_reg);
    }
}


static void
read_rxb (struct hw *me,
	  struct mn103ser *serial,
	  unsigned_word serial_reg,
	  void *dest,
	  unsigned  nr_bytes)
{
  if ( nr_bytes == 1 )
    {
      *(unsigned8 *)dest = serial->device[serial_reg].rxb;
      /* Reception buffer is now empty. */
      serial->device[serial_reg].status &= ~SIO_STAT_RRDY;
    }
  else
    {
      hw_abort (me, "bad read size of %d bytes from SC%dRXB.", nr_bytes, 
		serial_reg);
    }
}


static void
read_status_reg (struct hw *me,
		 struct mn103ser *serial,
		 unsigned_word serial_reg,
		 void *dest,
		 unsigned  nr_bytes)
{
  char c;
  int count;

  if ( (serial->device[serial_reg].status & SIO_STAT_RRDY) == 0 )
    {
      /* FIFO is empty */
      /* Kill current poll event */
      if ( NULL != serial->device[serial_reg].event )
	{
	  hw_event_queue_deschedule (me, serial->device[serial_reg].event);
	  serial->device[serial_reg].event = NULL;
	}

      if(USE_SOCKSER_P)
	{
	  int rd;
	  rd = dv_sockser_read (hw_system (me));
	  if(rd != -1)
	    {
	      c = (char) rd;
	      count = 1;
	    }
	  else
	    {
	      count = HW_IO_NOT_READY;
	    }
	}
      else
	{
	  count = do_hw_poll_read (me, serial->reader,
				   0/*STDIN*/, &c, sizeof(c));
	}

      switch (count)
	{
	case HW_IO_NOT_READY:
	case HW_IO_EOF:
	  serial->device[serial_reg].rxb = 0;
	  serial->device[serial_reg].status &= ~SIO_STAT_RRDY;
	  break;
	default:
	  serial->device[serial_reg].rxb = c;
	  serial->device[serial_reg].status |= SIO_STAT_RRDY;
	  hw_port_event (me, serial_reg+SERIAL0_RECEIVE, 1);
	}

      /* schedule polling event */
      serial->device[serial_reg].event
	= hw_event_queue_schedule (me, 1000,
				   do_polling_event,
				   (void *) (long) serial_reg);
    }

  if ( nr_bytes == 1 )
    {
      *(unsigned8 *)dest = (unsigned8)serial->device[serial_reg].status;
    }
  else if ( nr_bytes == 2 && serial_reg != SC2STR )
    {
      *(unsigned16 *)dest = H2LE_2 (serial->device[serial_reg].status);
    }
  else
    {
      hw_abort (me, "bad read size of %d bytes from SC%dSTR.", nr_bytes, 
		serial_reg);
    }
}


static void
read_serial2_timer_reg (struct hw *me,
			struct mn103ser *serial,
			void *dest,
			unsigned  nr_bytes)
{
  if ( nr_bytes == 1 )
    {
      * (unsigned8 *) dest = (unsigned8) serial->serial2_timer_reg;
    }
  else
    {
      hw_abort (me, "bad read size of %d bytes to SC2TIM.", nr_bytes);
    }
}


static unsigned
mn103ser_io_read_buffer (struct hw *me,
			 void *dest,
			 int space,
			 unsigned_word base,
			 unsigned nr_bytes)
{
  struct mn103ser *serial = hw_data (me);
  enum serial_register_types serial_reg;
  HW_TRACE ((me, "read 0x%08lx %d", (long) base, (int) nr_bytes));

  serial_reg = decode_addr (me, serial, base);
  switch (serial_reg)
    {
    /* control registers */
    case SC0CTR:
    case SC1CTR:
    case SC2CTR:
      read_control_reg(me, serial, serial_reg-SC0CTR, dest, nr_bytes);
      HW_TRACE ((me, "read - ctrl reg%d has 0x%x\n", serial_reg-SC0CTR,
		 *(unsigned8 *)dest));
      break;

    /* interrupt mode registers */
    case SC0ICR:
    case SC1ICR:
    case SC2ICR:
      read_intmode_reg(me, serial, serial_reg-SC0ICR, dest, nr_bytes);
      HW_TRACE ((me, "read - intmode reg%d has 0x%x\n", serial_reg-SC0ICR,
		 *(unsigned8 *)dest));
      break;

    /* transmission buffers */
    case SC0TXB:
    case SC1TXB:
    case SC2TXB:
      read_txb(me, serial, serial_reg-SC0TXB, dest, nr_bytes);
      HW_TRACE ((me, "read - txb%d has %c\n", serial_reg-SC0TXB,
		 *(char *)dest));
      break;

    /* reception buffers */
    case SC0RXB: 
    case SC1RXB:
    case SC2RXB:
      read_rxb(me, serial, serial_reg-SC0RXB, dest, nr_bytes);
      HW_TRACE ((me, "read - rxb%d has %c\n", serial_reg-SC0RXB,
		 *(char *)dest));
     break;

    /* status registers */
    case SC0STR: 
    case SC1STR: 
    case SC2STR: 
      read_status_reg(me, serial, serial_reg-SC0STR, dest, nr_bytes);
      HW_TRACE ((me, "read - status reg%d has 0x%x\n", serial_reg-SC0STR,
		 *(unsigned8 *)dest));
      break;

    case SC2TIM:
      read_serial2_timer_reg(me, serial, dest, nr_bytes);
      HW_TRACE ((me, "read - serial2 timer reg %d\n", *(unsigned8 *)dest));
      break;

    default:
      hw_abort(me, "invalid address");
    }

  return nr_bytes;
}     


static void
write_control_reg (struct hw *me,
		   struct mn103ser *serial,
		   unsigned_word serial_reg,
		   const void *source,
		   unsigned  nr_bytes)
{
  unsigned16 val = LE2H_2 (*(unsigned16 *)source);

  /* really allow 1 byte write, too */
  if ( nr_bytes == 2 )
    {
      if ( serial_reg == 2 && (val & 0x0C04) != 0 )
	{
	  hw_abort(me, "Cannot write to read-only bits of SC2CTR.");
	}
      else
	{
	  serial->device[serial_reg].control = val;
	}
    }
  else
    {
      hw_abort (me, "bad read size of %d bytes from SC%dSTR.", nr_bytes, 
		serial_reg);
    }
}


static void
write_intmode_reg (struct hw *me,
		   struct mn103ser *serial,
		   unsigned_word serial_reg,
		   const void *source,
		   unsigned  nr_bytes)
{
unsigned8 val = *(unsigned8 *)source;

  if ( nr_bytes == 1 )
    {
      /* Check for attempt to write to read-only bits of register. */
      if ( ( serial_reg == 2 && (val & 0xCA) != 0 )
	   || ( serial_reg != 2 && (val & 0x4A) != 0 ) )
	{
	  hw_abort(me, "Cannot write to read-only bits of SC%dICR.",
		   serial_reg);
	}
      else
	{
	  serial->device[serial_reg].intmode = val;
	}
    }
  else
    {
      hw_abort (me, "bad write size of %d bytes to SC%dICR.", nr_bytes, 
		serial_reg);
    }
}


static void
write_txb (struct hw *me,
	   struct mn103ser *serial,
	   unsigned_word serial_reg,
	   const void *source,
	   unsigned  nr_bytes)
{
  if ( nr_bytes == 1 )
    {
      serial->device[serial_reg].txb = *(unsigned8 *)source;

      if(USE_SOCKSER_P)
	{
	  dv_sockser_write(hw_system (me), * (char*) source);
	}
      else
	{
	  sim_io_write_stdout(hw_system (me), (char *)source, 1);
	  sim_io_flush_stdout(hw_system (me));
	}

      hw_port_event (me, serial_reg+SERIAL0_SEND, 1);
    }
  else
    {
      hw_abort (me, "bad write size of %d bytes to SC%dTXB.", nr_bytes, 
		serial_reg);
    }
}


static void
write_serial2_timer_reg (struct hw *me,
			 struct mn103ser *serial,
			 const void *source,
			 unsigned  nr_bytes)
{
  if ( nr_bytes == 1 )
    {
      serial->serial2_timer_reg = *(unsigned8 *)source;
    }
  else
    {
      hw_abort (me, "bad write size of %d bytes to SC2TIM.", nr_bytes); 
    }
}


static unsigned
mn103ser_io_write_buffer (struct hw *me,
			  const void *source,
			  int space,
			  unsigned_word base,
			  unsigned nr_bytes)
{
  struct mn103ser *serial = hw_data (me);
  enum serial_register_types serial_reg;
  HW_TRACE ((me, "write 0x%08lx %d", (long) base, (int) nr_bytes));

  serial_reg = decode_addr (me, serial, base);
  switch (serial_reg)
    {
    /* control registers */
    case SC0CTR:
    case SC1CTR:
    case SC2CTR:
      HW_TRACE ((me, "write - ctrl reg%d has 0x%x, nrbytes=%d.\n",
		 serial_reg-SC0CTR, *(unsigned8 *)source, nr_bytes));
      write_control_reg(me, serial, serial_reg-SC0CTR, source, nr_bytes);
      break;

    /* interrupt mode registers */
    case SC0ICR:
    case SC1ICR:
    case SC2ICR:
      HW_TRACE ((me, "write - intmode reg%d has 0x%x, nrbytes=%d.\n",
		 serial_reg-SC0ICR, *(unsigned8 *)source, nr_bytes));
      write_intmode_reg(me, serial, serial_reg-SC0ICR, source, nr_bytes);
      break;

    /* transmission buffers */
    case SC0TXB:
    case SC1TXB:
    case SC2TXB:
      HW_TRACE ((me, "write - txb%d has %c, nrbytes=%d.\n",
		 serial_reg-SC0TXB, *(char *)source, nr_bytes));
      write_txb(me, serial, serial_reg-SC0TXB, source, nr_bytes);
      break;

    /* reception buffers */
    case SC0RXB: 
    case SC1RXB:
    case SC2RXB:
      hw_abort(me, "Cannot write to reception buffer.");
     break;

    /* status registers */
    case SC0STR: 
    case SC1STR: 
    case SC2STR: 
      hw_abort(me, "Cannot write to status register.");
      break;

    case SC2TIM:
      HW_TRACE ((me, "read - serial2 timer reg %d (nrbytes=%d)\n",
		 *(unsigned8 *)source, nr_bytes));
      write_serial2_timer_reg(me, serial, source, nr_bytes);
      break;

    default:
      hw_abort(me, "invalid address");
    }

  return nr_bytes;
}     


const struct hw_descriptor dv_mn103ser_descriptor[] = {
  { "mn103ser", mn103ser_finish, },
  { NULL },
};
