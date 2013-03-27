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

   
   tx3904irc - tx3904 interrupt controller

   
   DESCRIPTION

   
   Implements the tx3904 interrupt controller described in the tx3904
   user guide.  It does not include the interrupt detection circuit
   that preprocesses the eight external interrupts, so assumes that
   each event on an input interrupt port signals a new interrupt.
   That is, it implements edge- rather than level-triggered
   interrupts.

   This implementation does not support multiple concurrent
   interrupts.


   PROPERTIES


   reg <base> <length>

   Base of IRC control register bank.  <length> must equal 0x20.
   Registers offsets:       0: ISR: interrupt status register
                            4: IMR: interrupt mask register
                           16: ILR0: interrupt level register 3..0
                           20: ILR1: interrupt level register 7..4
                           24: ILR2: interrupt level register 11..8
                           28: ILR3: interrupt level register 15..12



   PORTS


   ip (output)

   Interrupt priority port.  An event is generated when an interrupt
   of a sufficient priority is passed through the IRC.  The value
   associated with the event is the interrupt level (16-31), as given
   for bits IP[5:0] in the book TMPR3904F Rev. 2.0, pg. 11-3.  Note
   that even though INT[0] is tied externally to IP[5], we simulate
   it as passing through the controller.

   An output level of zero signals the clearing of a level interrupt.


   int0-7 (input)

   External interrupts.  Level = 0 -> level interrupt cleared.

   
   dmac0-3 (input)

   DMA internal interrupts, correspond to DMA channels 0-3.  Level = 0 -> level interrupt cleared.


   sio0-1 (input)

   SIO internal interrupts.  Level = 0 -> level interrupt cleared.


   tmr0-2 (input)

   Timer internal interrupts.  Level = 0 -> level interrupt cleared.

   */





/* register numbers; each is one word long */
enum
{
  ISR_REG = 0,
  IMR_REG = 1,
  ILR0_REG = 4,
  ILR1_REG = 5,
  ILR2_REG = 6,
  ILR3_REG = 7,
};


/* port ID's */

enum
{
  /* inputs, ordered to correspond to interrupt sources 0..15 */
  INT1_PORT = 0, INT2_PORT, INT3_PORT, INT4_PORT, INT5_PORT, INT6_PORT, INT7_PORT,
  DMAC3_PORT, DMAC2_PORT, DMAC1_PORT, DMAC0_PORT, SIO0_PORT, SIO1_PORT,
  TMR0_PORT, TMR1_PORT, TMR2_PORT,

  /* special INT[0] port */
  INT0_PORT,

  /* reset */
  RESET_PORT,

  /* output */
  IP_PORT
};


static const struct hw_port_descriptor tx3904irc_ports[] = {

  /* interrupt output */

  { "ip", IP_PORT, 0, output_port, },

  /* interrupt inputs (as names) */
  /* in increasing order of level number */

  { "int1", INT1_PORT, 0, input_port, },
  { "int2", INT2_PORT, 0, input_port, },
  { "int3", INT3_PORT, 0, input_port, },
  { "int4", INT4_PORT, 0, input_port, },
  { "int5", INT5_PORT, 0, input_port, },
  { "int6", INT6_PORT, 0, input_port, },
  { "int7", INT7_PORT, 0, input_port, },

  { "dmac3", DMAC3_PORT, 0, input_port, },
  { "dmac2", DMAC2_PORT, 0, input_port, },
  { "dmac1", DMAC1_PORT, 0, input_port, },
  { "dmac0", DMAC0_PORT, 0, input_port, },

  { "sio0", SIO0_PORT, 0, input_port, },
  { "sio1", SIO1_PORT, 0, input_port, },

  { "tmr0", TMR0_PORT, 0, input_port, },
  { "tmr1", TMR1_PORT, 0, input_port, },
  { "tmr2", TMR2_PORT, 0, input_port, },

  { "reset", RESET_PORT, 0, input_port, },
  { "int0", INT0_PORT, 0, input_port, },

  { NULL, },
};


#define NR_SOURCES (TMR3_PORT - INT1_PORT + 1) /* 16: number of interrupt sources */


/* The interrupt controller register internal state.  Note that we
   store state using the control register images, in host endian
   order. */

struct tx3904irc {
  address_word base_address; /* control register base */
  unsigned_4 isr;
#define ISR_SET(c,s) ((c)->isr &= ~ (1 << (s)))
  unsigned_4 imr;
#define IMR_GET(c) ((c)->imr)
  unsigned_4 ilr[4];
#define ILR_GET(c,s) LSEXTRACTED32((c)->ilr[(s)/4], (s) % 4 * 8 + 2, (s) % 4 * 8)
};



/* Finish off the partially created hw device.  Attach our local
   callbacks.  Wire up our port names etc */

static hw_io_read_buffer_method tx3904irc_io_read_buffer;
static hw_io_write_buffer_method tx3904irc_io_write_buffer;
static hw_port_event_method tx3904irc_port_event;

static void
attach_tx3904irc_regs (struct hw *me,
		      struct tx3904irc *controller)
{
  unsigned_word attach_address;
  int attach_space;
  unsigned attach_size;
  reg_property_spec reg;

  if (hw_find_property (me, "reg") == NULL)
    hw_abort (me, "Missing \"reg\" property");

  if (!hw_find_reg_array_property (me, "reg", 0, &reg))
    hw_abort (me, "\"reg\" property must contain one addr/size entry");

  hw_unit_address_to_attach_address (hw_parent (me),
				     &reg.address,
				     &attach_space,
				     &attach_address,
				     me);
  hw_unit_size_to_attach_size (hw_parent (me),
			       &reg.size,
			       &attach_size, me);

  hw_attach_address (hw_parent (me), 0,
		     attach_space, attach_address, attach_size,
		     me);

  controller->base_address = attach_address;
}


static void
tx3904irc_finish (struct hw *me)
{
  struct tx3904irc *controller;

  controller = HW_ZALLOC (me, struct tx3904irc);
  set_hw_data (me, controller);
  set_hw_io_read_buffer (me, tx3904irc_io_read_buffer);
  set_hw_io_write_buffer (me, tx3904irc_io_write_buffer);
  set_hw_ports (me, tx3904irc_ports);
  set_hw_port_event (me, tx3904irc_port_event);

  /* Attach ourself to our parent bus */
  attach_tx3904irc_regs (me, controller);

  /* Initialize to reset state */
  controller->isr = 0x0000ffff;
  controller->imr = 0;
  controller->ilr[0] =
    controller->ilr[1] =
    controller->ilr[2] =
    controller->ilr[3] = 0;
}



/* An event arrives on an interrupt port */

static void
tx3904irc_port_event (struct hw *me,
		     int my_port,
		     struct hw *source_dev,
		     int source_port,
		     int level)
{
  struct tx3904irc *controller = hw_data (me);

  /* handle deactivated interrupt */
  if(level == 0)
    {
      HW_TRACE ((me, "interrupt cleared on port %d", my_port));
      hw_port_event(me, IP_PORT, 0);
      return;
    }

  switch (my_port)
    {
    case INT0_PORT: 
      {
	int ip_number = 32; /* compute IP[5:0] */
	HW_TRACE ((me, "port-event INT[0]"));
	hw_port_event(me, IP_PORT, ip_number);
	break;
      }

    case INT1_PORT: case INT2_PORT: case INT3_PORT: case INT4_PORT:
    case INT5_PORT: case INT6_PORT: case INT7_PORT: case DMAC3_PORT:
    case DMAC2_PORT: case DMAC1_PORT: case DMAC0_PORT: case SIO0_PORT:
    case SIO1_PORT: case TMR0_PORT: case TMR1_PORT: case TMR2_PORT:
      {
	int source = my_port - INT1_PORT;

	HW_TRACE ((me, "interrupt asserted on port %d", source));
	ISR_SET(controller, source);
	if(ILR_GET(controller, source) > IMR_GET(controller))
	  {
	    int ip_number = 16 + source; /* compute IP[4:0] */
	    HW_TRACE ((me, "interrupt level %d", ILR_GET(controller,source)));
	    hw_port_event(me, IP_PORT, ip_number);
	  }
	break;
      }

    case RESET_PORT:
      {
	HW_TRACE ((me, "reset"));
	controller->isr = 0x0000ffff;
	controller->imr = 0;
	controller->ilr[0] =
	  controller->ilr[1] =
	  controller->ilr[2] =
	  controller->ilr[3] = 0;
	break;
      }

    case IP_PORT:
      hw_abort (me, "Event on output port %d", my_port);
      break;

    default:
      hw_abort (me, "Event on unknown port %d", my_port);
      break;
    }
}


/* generic read/write */

static unsigned
tx3904irc_io_read_buffer (struct hw *me,
			 void *dest,
			 int space,
			 unsigned_word base,
			 unsigned nr_bytes)
{
  struct tx3904irc *controller = hw_data (me);
  unsigned byte;

  HW_TRACE ((me, "read 0x%08lx %d", (long) base, (int) nr_bytes));
  for (byte = 0; byte < nr_bytes; byte++)
    {
      address_word address = base + byte;
      int reg_number = (address - controller->base_address) / 4;
      int reg_offset = (address - controller->base_address) % 4;
      unsigned_4 register_value; /* in target byte order */

      /* fill in entire register_value word */
      switch (reg_number)
	{
	case ISR_REG: register_value = controller->isr; break;
	case IMR_REG: register_value = controller->imr; break;
	case ILR0_REG: register_value = controller->ilr[0]; break;
	case ILR1_REG: register_value = controller->ilr[1]; break;
	case ILR2_REG: register_value = controller->ilr[2]; break;
	case ILR3_REG: register_value = controller->ilr[3]; break;
	default: register_value = 0;
	}

      /* write requested byte out */
      register_value = H2T_4(register_value);
      memcpy ((char*) dest + byte, ((char*)& register_value)+reg_offset, 1);
    }

  return nr_bytes;
}     



static unsigned
tx3904irc_io_write_buffer (struct hw *me,
			  const void *source,
			  int space,
			  unsigned_word base,
			  unsigned nr_bytes)
{
  struct tx3904irc *controller = hw_data (me);
  unsigned byte;

  HW_TRACE ((me, "write 0x%08lx %d", (long) base, (int) nr_bytes));
  for (byte = 0; byte < nr_bytes; byte++)
    {
      address_word address = base + byte;
      int reg_number = (address - controller->base_address) / 4;
      int reg_offset = (address - controller->base_address) % 4;
      unsigned_4* register_ptr;
      unsigned_4 register_value;

      /* fill in entire register_value word */
      switch (reg_number)
	{
	case ISR_REG: register_ptr = & controller->isr; break;
	case IMR_REG: register_ptr = & controller->imr; break;
	case ILR0_REG: register_ptr = & controller->ilr[0]; break;
	case ILR1_REG: register_ptr = & controller->ilr[1]; break;
	case ILR2_REG: register_ptr = & controller->ilr[2]; break;
	case ILR3_REG: register_ptr = & controller->ilr[3]; break;
	default: register_ptr = & register_value; /* used as a dummy */
	}

      /* HW_TRACE ((me, "reg %d pre: %08lx", reg_number, (long) *register_ptr)); */

      /* overwrite requested byte */
      register_value = H2T_4(* register_ptr);
      memcpy (((char*)&register_value)+reg_offset, (const char*)source + byte, 1);
      * register_ptr = T2H_4(register_value);

      /* HW_TRACE ((me, "post: %08lx", (long) *register_ptr)); */
    }
  return nr_bytes;
}     


const struct hw_descriptor dv_tx3904irc_descriptor[] = {
  { "tx3904irc", tx3904irc_finish, },
  { NULL },
};
