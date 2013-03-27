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

   
   tx3904tmr - tx3904 timer

   
   DESCRIPTION

   
   Implements one tx3904 timer/counter described in the tx3904
   user guide.  Three instances are required for TMR0, TMR1, and
   TMR3 within the tx3904, at different base addresses.  

   Both internal and system clocks are synthesized as divided versions
   of the simulator clock.
   
   There is no support for:
    - edge sensitivity of external clock
    - different mode restrictions for TMR0..2
    - level interrupts (interrupts are treated as events that occur at edges)



   PROPERTIES


   reg <base> <length>

   Base of TMR control register bank.  <length> must equal 0x100.
   Register offsets:       0: TCR: timer control  register
                           4: TISR: timer interrupt status register
                           8: CPRA: compare register A
                          12: CPRB: compare register B
                          16: ITMR: interval timer mode register
			  32: CCDR: divider register
			  48: PMGR: pulse generator mode register
			  64: WTMR: watchdog timer mode register
			 240: TRR: timer read register


   clock <ticks>

   Rate of timer clock signal.  This number is the number of simulator
   ticks per clock signal tick.  Default 1.

   
   ext <ticks>

   Rate of "external input clock signal", the other clock input of the
   timer.  It uses the same scale as above.  Default 100.



   PORTS


   int (output)

   Interrupt port.  An event is generated when a timer interrupt
   occurs.


   ff (output)

   Flip-flop output, corresponds to the TMFFOUT port.  An event is
   generated when flip-flop changes value.  The integer associated
   with the event is 1/0 according to flip-flop value.


   reset (input)

   Reset port.

   */



/* static functions */

static void deliver_tx3904tmr_tick (struct hw *me, void *data);


/* register numbers; each is one word long */
enum 
{
  TCR_REG = 0,
  TISR_REG = 1,
  CPRA_REG = 2,
  CPRB_REG = 3,
  ITMR_REG = 4,
  CCDR_REG = 8,
  PMGR_REG = 12,
  WTMR_REG = 16,
  TRR_REG = 60
};



/* port ID's */

enum
 {
  RESET_PORT,
  INT_PORT,
  FF_PORT
};


static const struct hw_port_descriptor tx3904tmr_ports[] = 
{
  { "int", INT_PORT, 0, output_port, },
  { "ff", FF_PORT, 0, output_port, },
  { "reset", RESET_PORT, 0, input_port, },
  { NULL, },
};



/* The timer/counter register internal state.  Note that we store
   state using the control register images, in host endian order. */

struct tx3904tmr {
  address_word base_address; /* control register base */
  unsigned_4 clock_ticks, ext_ticks; /* clock frequencies */
  signed_8 last_ticks; /* time at last deliver_*_tick call */
  signed_8 roundoff_ticks; /* sim ticks unprocessed during last tick call */
  int ff; /* pulse generator flip-flop value: 1/0 */
  struct hw_event* event; /* last scheduled event */

  unsigned_4 tcr;
#define GET_TCR_TCE(c)      (((c)->tcr & 0x80) >> 7)
#define GET_TCR_CCDE(c)     (((c)->tcr & 0x40) >> 6)
#define GET_TCR_CRE(c)      (((c)->tcr & 0x20) >> 5)
#define GET_TCR_CCS(c)      (((c)->tcr & 0x04) >> 2)
#define GET_TCR_TMODE(c)    (((c)->tcr & 0x03) >> 0)
  unsigned_4 tisr;
#define SET_TISR_TWIS(c)    ((c)->tisr |= 0x08)
#define SET_TISR_TPIBS(c)   ((c)->tisr |= 0x04)
#define SET_TISR_TPIAS(c)   ((c)->tisr |= 0x02)
#define SET_TISR_TIIS(c)    ((c)->tisr |= 0x01)
  unsigned_4 cpra;
  unsigned_4 cprb;
  unsigned_4 itmr;
#define GET_ITMR_TIIE(c)    (((c)->itmr & 0x8000) >> 15)
#define SET_ITMR_TIIE(c,v)  BLIT32((c)->itmr, 15, (v) ? 1 : 0)
#define GET_ITMR_TZCE(c)    (((c)->itmr & 0x0001) >> 0)
#define SET_ITMR_TZCE(c,v)  BLIT32((c)->itmr, 0, (v) ? 1 : 0)
  unsigned_4 ccdr;
#define GET_CCDR_CDR(c)     (((c)->ccdr & 0x07) >> 0)
  unsigned_4 pmgr;
#define GET_PMGR_TPIBE(c)   (((c)->pmgr & 0x8000) >> 15)
#define SET_PMGR_TPIBE(c,v) BLIT32((c)->pmgr, 15, (v) ? 1 : 0)
#define GET_PMGR_TPIAE(c)   (((c)->pmgr & 0x4000) >> 14)
#define SET_PMGR_TPIAE(c,v) BLIT32((c)->pmgr, 14, (v) ? 1 : 0)
#define GET_PMGR_FFI(c)     (((c)->pmgr & 0x0001) >> 0)
#define SET_PMGR_FFI(c,v)   BLIT32((c)->pmgr, 0, (v) ? 1 : 0)
  unsigned_4 wtmr;
#define GET_WTMR_TWIE(c)    (((c)->wtmr & 0x8000) >> 15)
#define SET_WTMR_TWIE(c,v)  BLIT32((c)->wtmr, 15, (v) ? 1 : 0)
#define GET_WTMR_WDIS(c)    (((c)->wtmr & 0x0080) >> 7)
#define SET_WTMR_WDIS(c,v)  BLIT32((c)->wtmr, 7, (v) ? 1 : 0)
#define GET_WTMR_TWC(c)     (((c)->wtmr & 0x0001) >> 0)
#define SET_WTMR_TWC(c,v)   BLIT32((c)->wtmr, 0, (v) ? 1 : 0)
  unsigned_4 trr;
};



/* Finish off the partially created hw device.  Attach our local
   callbacks.  Wire up our port names etc */

static hw_io_read_buffer_method tx3904tmr_io_read_buffer;
static hw_io_write_buffer_method tx3904tmr_io_write_buffer;
static hw_port_event_method tx3904tmr_port_event;

static void
attach_tx3904tmr_regs (struct hw *me,
		      struct tx3904tmr *controller)
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

  if(hw_find_property(me, "clock") != NULL)
    controller->clock_ticks = (unsigned_4) hw_find_integer_property(me, "clock");

  if(hw_find_property(me, "ext") != NULL)
    controller->ext_ticks = (unsigned_4) hw_find_integer_property(me, "ext");

  controller->base_address = attach_address;
}


static void
tx3904tmr_finish (struct hw *me)
{
  struct tx3904tmr *controller;

  controller = HW_ZALLOC (me, struct tx3904tmr);
  set_hw_data (me, controller);
  set_hw_io_read_buffer (me, tx3904tmr_io_read_buffer);
  set_hw_io_write_buffer (me, tx3904tmr_io_write_buffer);
  set_hw_ports (me, tx3904tmr_ports);
  set_hw_port_event (me, tx3904tmr_port_event);

  /* Preset clock dividers */
  controller->clock_ticks = 1;
  controller->ext_ticks = 100;

  /* Attach ourself to our parent bus */
  attach_tx3904tmr_regs (me, controller);

  /* Initialize to reset state */
  controller->tcr = 
    controller->itmr =
    controller->ccdr =
    controller->pmgr = 
    controller->wtmr =
    controller->tisr = 
    controller->trr = 0;
  controller->cpra = controller->cprb = 0x00FFFFFF;
  controller->ff = 0;
  controller->last_ticks = controller->roundoff_ticks = 0;
  controller->event = NULL;
}



/* An event arrives on an interrupt port */

static void
tx3904tmr_port_event (struct hw *me,
		     int my_port,
		     struct hw *source,
		     int source_port,
		     int level)
{
  struct tx3904tmr *controller = hw_data (me);

  switch (my_port)
    {
    case RESET_PORT:
      {
	HW_TRACE ((me, "reset"));

	/* preset flip-flop to FFI value */
	controller->ff = GET_PMGR_FFI(controller);

	controller->tcr = 
	  controller->itmr =
	  controller->ccdr =
	  controller->pmgr = 
	  controller->wtmr =
	  controller->tisr = 
	  controller->trr = 0;
	controller->cpra = controller->cprb = 0x00FFFFFF;
	controller->last_ticks = controller->roundoff_ticks = 0;
	if(controller->event != NULL)
	  hw_event_queue_deschedule(me, controller->event);
	controller->event = NULL;
	break;
      }

    default:
      hw_abort (me, "Event on unknown port %d", my_port);
      break;
    }
}


/* generic read/write */

static unsigned
tx3904tmr_io_read_buffer (struct hw *me,
			 void *dest,
			 int space,
			 unsigned_word base,
			 unsigned nr_bytes)
{
  struct tx3904tmr *controller = hw_data (me);
  unsigned byte;

  HW_TRACE ((me, "read 0x%08lx %d", (long) base, (int) nr_bytes));
  for (byte = 0; byte < nr_bytes; byte++)
    {
      address_word address = base + byte;
      int reg_number = (address - controller->base_address) / 4;
      int reg_offset = 3 - (address - controller->base_address) % 4;
      unsigned_4 register_value; /* in target byte order */

      /* fill in entire register_value word */
      switch (reg_number)
	{
	case TCR_REG: register_value = controller->tcr; break;
	case TISR_REG: register_value = controller->tisr; break;
	case CPRA_REG: register_value = controller->cpra; break;
	case CPRB_REG: register_value = controller->cprb; break;
	case ITMR_REG: register_value = controller->itmr; break;
	case CCDR_REG: register_value = controller->ccdr; break;
	case PMGR_REG: register_value = controller->pmgr; break;
	case WTMR_REG: register_value = controller->wtmr; break;
	case TRR_REG: register_value = controller->trr; break;
	default: register_value = 0;
	}

      /* write requested byte out */
      memcpy ((char*) dest + byte, ((char*)& register_value)+reg_offset, 1);
    }

  return nr_bytes;
}     



static unsigned
tx3904tmr_io_write_buffer (struct hw *me,
			  const void *source,
			  int space,
			  unsigned_word base,
			  unsigned nr_bytes)
{
  struct tx3904tmr *controller = hw_data (me);
  unsigned byte;

  HW_TRACE ((me, "write 0x%08lx %d", (long) base, (int) nr_bytes));
  for (byte = 0; byte < nr_bytes; byte++)
    {
      address_word address = base + byte;
      unsigned_1 write_byte = ((const char*) source)[byte];
      int reg_number = (address - controller->base_address) / 4;
      int reg_offset = 3 - (address - controller->base_address) % 4;

      /* fill in entire register_value word */
      switch (reg_number)
	{
	case TCR_REG:
	  if(reg_offset == 0) /* first byte */
	    {
	      /* update register, but mask out NOP bits */
	      controller->tcr = (unsigned_4) (write_byte & 0xef);

	      /* Reset counter value if timer suspended and CRE is set. */
	      if(GET_TCR_TCE(controller) == 0 &&
		 GET_TCR_CRE(controller) == 1)
		controller->trr = 0;
	    }
	  /* HW_TRACE ((me, "tcr: %08lx", (long) controller->tcr)); */
	  break;

	case ITMR_REG:
	  if(reg_offset == 1) /* second byte */
	    {
	      SET_ITMR_TIIE(controller, write_byte & 0x80);
	    }
	  else if(reg_offset == 0) /* first byte */
	    {
	      SET_ITMR_TZCE(controller, write_byte & 0x01);
	    }
	  /* HW_TRACE ((me, "itmr: %08lx", (long) controller->itmr)); */
	  break;

	case CCDR_REG:
	  if(reg_offset == 0) /* first byte */
	    {
	      controller->ccdr = write_byte & 0x07;
	    }
	  /* HW_TRACE ((me, "ccdr: %08lx", (long) controller->ccdr)); */
	  break;

	case PMGR_REG:
	  if(reg_offset == 1) /* second byte */
	    {
	      SET_PMGR_TPIBE(controller, write_byte & 0x80);
	      SET_PMGR_TPIAE(controller, write_byte & 0x40);
	    }
	  else if(reg_offset == 0) /* first byte */
	    {
	      SET_PMGR_FFI(controller, write_byte & 0x01);
	    }
	  /* HW_TRACE ((me, "pmgr: %08lx", (long) controller->pmgr)); */
	  break;

	case WTMR_REG:
	  if(reg_offset == 1) /* second byte */
	    {
	      SET_WTMR_TWIE(controller, write_byte & 0x80);
	    }
	  else if(reg_offset == 0) /* first byte */
	    {
	      SET_WTMR_WDIS(controller, write_byte & 0x80);
	      SET_WTMR_TWC(controller, write_byte & 0x01);
	    }
	  /* HW_TRACE ((me, "wtmr: %08lx", (long) controller->wtmr)); */
	  break;

	case TISR_REG:
	  if(reg_offset == 0) /* first byte */
	    {
	      /* All bits must be zero in given byte, according to
                 spec. */

	      /* Send an "interrupt off" event on the interrupt port */
	      if(controller->tisr != 0) /* any interrupts active? */
		{
		  hw_port_event(me, INT_PORT, 0);		  
		}
	      
	      /* clear interrupt status register */
	      controller->tisr = 0;
	    }
	  /* HW_TRACE ((me, "tisr: %08lx", (long) controller->tisr)); */
	  break;

	case CPRA_REG:
	  if(reg_offset < 3) /* first, second, or third byte */
	    {
	      MBLIT32(controller->cpra, (reg_offset*8)+7, (reg_offset*8), write_byte);
	    }
	  /* HW_TRACE ((me, "cpra: %08lx", (long) controller->cpra)); */
	  break;

	case CPRB_REG:
	  if(reg_offset < 3) /* first, second, or third byte */
	    {
	      MBLIT32(controller->cprb, (reg_offset*8)+7, (reg_offset*8), write_byte);
	    }
	  /* HW_TRACE ((me, "cprb: %08lx", (long) controller->cprb)); */
	  break;

	default: 
	  HW_TRACE ((me, "write to illegal register %d", reg_number));
	}
    } /* loop over bytes */

  /* Schedule a timer event in near future, so we can increment or
     stop the counter, to respond to register updates. */
  hw_event_queue_schedule(me, 1, deliver_tx3904tmr_tick, NULL);

  return nr_bytes;
}     



/* Deliver a clock tick to the counter. */
static void
deliver_tx3904tmr_tick (struct hw *me,
			void *data)
{
  struct tx3904tmr *controller = hw_data (me);
  SIM_DESC sd = hw_system (me);
  signed_8 this_ticks = sim_events_time(sd);

  signed_8 warp;
  signed_8 divisor;
  signed_8 quotient, remainder;

  /* compute simulation ticks between last tick and this tick */
  if(controller->last_ticks != 0)
    warp = this_ticks - controller->last_ticks + controller->roundoff_ticks;
  else
    {
      controller->last_ticks = this_ticks; /* initialize */
      warp = controller->roundoff_ticks;
    }

  if(controller->event != NULL)
    hw_event_queue_deschedule(me, controller->event);
  controller->event = NULL;

  /* Check whether the timer ticking is enabled at this moment.  This
     largely a function of the TCE bit, but is also slightly
     mode-dependent. */
  switch((int) GET_TCR_TMODE(controller))
    {
    case 0: /* interval */
      /* do not advance counter if TCE = 0 or if holding at count = CPRA */
      if(GET_TCR_TCE(controller) == 0 ||
	 controller->trr == controller->cpra)
	return;
      break;

    case 1: /* pulse generator */
      /* do not advance counter if TCE = 0 */
      if(GET_TCR_TCE(controller) == 0)
	return;
      break;

    case 2: /* watchdog */
      /* do not advance counter if TCE = 0 and WDIS = 1 */
      if(GET_TCR_TCE(controller) == 0 &&
	 GET_WTMR_WDIS(controller) == 1)
	return;
      break;

    case 3: /* disabled */
      /* regardless of TCE, do not advance counter */
      return;
    }

  /* In any of the above cases that return, a subsequent register
     write will be needed to restart the timer.  A tick event is
     scheduled by any register write, so it is more efficient not to
     reschedule dummy events here. */


  /* find appropriate divisor etc. */ 
  if(GET_TCR_CCS(controller) == 0) /* internal system clock */
    {
      /* apply internal clock divider */
      if(GET_TCR_CCDE(controller)) /* divisor circuit enabled? */
	divisor = controller->clock_ticks * (1 << (1 + GET_CCDR_CDR(controller)));
      else
	divisor = controller->clock_ticks;
    }
  else
    {
      divisor = controller->ext_ticks;
    }

  /* how many times to increase counter? */
  quotient = warp / divisor;
  remainder = warp % divisor;

  /* NOTE: If the event rescheduling code works properly, the quotient
     should never be larger than 1.  That is, we should receive events
     here at least as frequently as the simulated counter is supposed
     to decrement.  So the remainder (-> roundoff_ticks) will slowly
     accumulate, with the quotient == 0.  Once in a while, quotient
     will equal 1. */

  controller->roundoff_ticks = remainder;
  controller->last_ticks = this_ticks;
  while(quotient > 0) /* Is it time to increment counter? */
    {
      /* next 24-bit counter value */
      unsigned_4 next_trr = (controller->trr + 1) % (1 << 24);
      quotient --;
      
      switch((int) GET_TCR_TMODE(controller))
	{
	case 0: /* interval timer mode */
	  {
	    /* Current or next counter value matches CPRA value?  The
	       first case covers counter holding at maximum before
	       reset.  The second case covers normal counting
	       behavior. */
	    if(controller->trr == controller->cpra ||
	       next_trr == controller->cpra)
	      {
		/* likely hold CPRA value */
		if(controller->trr == controller->cpra)
		  next_trr = controller->cpra;

		SET_TISR_TIIS(controller);

		/* Signal an interrupt if it is enabled with TIIE,
		   and if we just arrived at CPRA.  Don't repeatedly
		   interrupt if holding due to TZCE=0 */
		if(GET_ITMR_TIIE(controller) &&
		   next_trr != controller->trr)
		  {
		    hw_port_event(me, INT_PORT, 1);
		  }

		/* Reset counter? */
		if(GET_ITMR_TZCE(controller))
		  {
		    next_trr = 0;
		  }
	      }
	  }
	break;

	case 1: /* pulse generator mode */
	  {
	    /* first trip point */
	    if(next_trr == controller->cpra)
	      {
		/* flip flip-flop & report */
		controller->ff ^= 1;
		hw_port_event(me, FF_PORT, controller->ff);
		SET_TISR_TPIAS(controller);

		/* signal interrupt */
		if(GET_PMGR_TPIAE(controller))
		  {
		    hw_port_event(me, INT_PORT, 1);
		  }

	      }
	    /* second trip point */
	    else if(next_trr == controller->cprb)
	      {
		/* flip flip-flop & report */
		controller->ff ^= 1;
		hw_port_event(me, FF_PORT, controller->ff);
		SET_TISR_TPIBS(controller);

		/* signal interrupt */
		if(GET_PMGR_TPIBE(controller))
		  {
		    hw_port_event(me, INT_PORT, 1);
		  }

		/* clear counter */
		next_trr = 0;
	      }
	  }
	break;

	case 2: /* watchdog timer mode */
	  {
	    /* watchdog timer expiry */
	    if(next_trr == controller->cpra)
	      {
		SET_TISR_TWIS(controller);

		/* signal interrupt */
		if(GET_WTMR_TWIE(controller))
		  {
		    hw_port_event(me, INT_PORT, 1);
		  }

		/* clear counter */
		next_trr = 0;
	      }
	  }
	break;

	case 3: /* disabled */
	default:
	  break;
	}

      /* update counter and report */
      controller->trr = next_trr;
      /* HW_TRACE ((me, "counter trr %ld tisr %lx",
	 (long) controller->trr, (long) controller->tisr)); */
    } /* end quotient loop */

  /* Reschedule a timer event in near future, so we can increment the
     counter again.  Set the event about 75% of divisor time away, so
     we will experience roughly 1.3 events per counter increment. */
  controller->event = hw_event_queue_schedule(me, divisor*3/4, deliver_tx3904tmr_tick, NULL);
}




const struct hw_descriptor dv_tx3904tmr_descriptor[] = {
  { "tx3904tmr", tx3904tmr_finish, },
  { NULL },
};
