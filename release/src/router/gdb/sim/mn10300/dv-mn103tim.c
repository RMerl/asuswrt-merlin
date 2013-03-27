/*  This file is part of the program GDB, the GNU debugger.
    
    Copyright (C) 1998, 2003, 2007 Free Software Foundation, Inc.
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
#include "sim-assert.h"

/* DEVICE

   
   mn103tim - mn103002 timers (8 and 16 bit)

   
   DESCRIPTION
   
   Implements the mn103002 8 and 16 bit timers as described in the mn103002 user guide.


   PROPERTIES   

   reg = <8bit-timers-addr> <8bit-timers-size> <16bit-timers-addr> <16bit-timers-size>


   BUGS

   */


/* The timers' register address blocks */

struct mn103tim_block {
  unsigned_word base;
  unsigned_word bound;
};

enum { TIMER8_BLOCK, TIMER16_BLOCK, NR_TIMER_BLOCKS };

enum timer_register_types {
  FIRST_MODE_REG = 0,
  TM0MD = FIRST_MODE_REG,
  TM1MD,
  TM2MD,
  TM3MD,
  TM4MD,
  TM5MD,
  TM6MD,
  LAST_MODE_REG = TM6MD,
  FIRST_BASE_REG,
  TM0BR = FIRST_BASE_REG,
  TM1BR,
  TM2BR,
  TM3BR,
  TM4BR,
  TM5BR,
  LAST_BASE_REG = TM5BR,
  FIRST_COUNTER,
  TM0BC = FIRST_COUNTER,
  TM1BC,
  TM2BC,
  TM3BC,
  TM4BC,
  TM5BC,
  TM6BC,
  LAST_COUNTER = TM6BC,
  TM6MDA,
  TM6MDB,
  TM6CA,
  TM6CB,
  LAST_TIMER_REG = TM6BC,
};


/* Don't include timer 6 because it's handled specially. */
#define NR_8BIT_TIMERS 4
#define NR_16BIT_TIMERS 2
#define NR_REG_TIMERS 6 /* Exclude timer 6 - it's handled specially. */
#define NR_TIMERS 7

typedef struct _mn10300_timer_regs {
  unsigned32 base;
  unsigned8  mode;
} mn10300_timer_regs;

typedef struct _mn10300_timer {
  unsigned32 div_ratio, start;
  struct hw_event *event;
} mn10300_timer;


struct mn103tim {
  struct mn103tim_block block[NR_TIMER_BLOCKS];
  mn10300_timer_regs reg[NR_REG_TIMERS];
  mn10300_timer timer[NR_TIMERS];

  /* treat timer 6 registers specially. */
  unsigned16   tm6md0, tm6md1, tm6bc, tm6ca, tm6cb; 
  unsigned8  tm6mda, tm6mdb;  /* compare/capture mode regs for timer 6 */
};

/* output port ID's */

/* for mn103002 */
enum {
  TIMER0_UFLOW,
  TIMER1_UFLOW,
  TIMER2_UFLOW,
  TIMER3_UFLOW,
  TIMER4_UFLOW,
  TIMER5_UFLOW,
  TIMER6_UFLOW,
  TIMER6_CMPA,
  TIMER6_CMPB,
};


static const struct hw_port_descriptor mn103tim_ports[] = {

  { "timer-0-underflow", TIMER0_UFLOW, 0, output_port, },
  { "timer-1-underflow", TIMER1_UFLOW, 0, output_port, },
  { "timer-2-underflow", TIMER2_UFLOW, 0, output_port, },
  { "timer-3-underflow", TIMER3_UFLOW, 0, output_port, },
  { "timer-4-underflow", TIMER4_UFLOW, 0, output_port, },
  { "timer-5-underflow", TIMER5_UFLOW, 0, output_port, },

  { "timer-6-underflow", TIMER6_UFLOW, 0, output_port, },
  { "timer-6-compare-a", TIMER6_CMPA, 0, output_port, },
  { "timer-6-compare-b", TIMER6_CMPB, 0, output_port, },

  { NULL, },
};

#define bits2to5_mask 0x3c
#define bits0to2_mask 0x07
#define load_mask     0x40
#define count_mask    0x80
#define count_and_load_mask (load_mask | count_mask)
#define clock_mask    0x03
#define clk_ioclk    0x00
#define clk_cascaded 0x03


/* Finish off the partially created hw device.  Attach our local
   callbacks.  Wire up our port names etc */

static hw_io_read_buffer_method mn103tim_io_read_buffer;
static hw_io_write_buffer_method mn103tim_io_write_buffer;

static void
attach_mn103tim_regs (struct hw *me,
		      struct mn103tim *timers)
{
  int i;
  if (hw_find_property (me, "reg") == NULL)
    hw_abort (me, "Missing \"reg\" property");
  for (i = 0; i < NR_TIMER_BLOCKS; i++)
    {
      unsigned_word attach_address;
      int attach_space;
      unsigned attach_size;
      reg_property_spec reg;
      if (!hw_find_reg_array_property (me, "reg", i, &reg))
	hw_abort (me, "\"reg\" property must contain three addr/size entries");
      hw_unit_address_to_attach_address (hw_parent (me),
					 &reg.address,
					 &attach_space,
					 &attach_address,
					 me);
      timers->block[i].base = attach_address;
      hw_unit_size_to_attach_size (hw_parent (me),
				   &reg.size,
				   &attach_size, me);
      timers->block[i].bound = attach_address + (attach_size - 1);
      hw_attach_address (hw_parent (me),
			 0,
			 attach_space, attach_address, attach_size,
			 me);
    }
}

static void
mn103tim_finish (struct hw *me)
{
  struct mn103tim *timers;
  int i;

  timers = HW_ZALLOC (me, struct mn103tim);
  set_hw_data (me, timers);
  set_hw_io_read_buffer (me, mn103tim_io_read_buffer);
  set_hw_io_write_buffer (me, mn103tim_io_write_buffer);
  set_hw_ports (me, mn103tim_ports);

  /* Attach ourself to our parent bus */
  attach_mn103tim_regs (me, timers);

  /* Initialize the timers */
  for ( i=0; i < NR_REG_TIMERS; ++i )
    {
      timers->reg[i].mode = 0x00;
      timers->reg[i].base = 0;
    }
  for ( i=0; i < NR_TIMERS; ++i )
    {
      timers->timer[i].event = NULL;
      timers->timer[i].div_ratio = 0;
      timers->timer[i].start = 0;
    }
  timers->tm6md0 = 0x00;
  timers->tm6md1 = 0x00;
  timers->tm6bc = 0x0000;
  timers->tm6ca = 0x0000;
  timers->tm6cb = 0x0000;
  timers->tm6mda = 0x00;
  timers->tm6mdb = 0x00;
}



/* read and write */

static int
decode_addr (struct hw *me,
	     struct mn103tim *timers,
	     unsigned_word address)
{
  unsigned_word offset;
  offset = address - timers->block[0].base;

  switch (offset)
    {
    case 0x00: return TM0MD;
    case 0x01: return TM1MD;
    case 0x02: return TM2MD;
    case 0x03: return TM3MD;
    case 0x10: return TM0BR;
    case 0x11: return TM1BR;
    case 0x12: return TM2BR;
    case 0x13: return TM3BR;
    case 0x20: return TM0BC;
    case 0x21: return TM1BC;
    case 0x22: return TM2BC;
    case 0x23: return TM3BC;
    case 0x80: return TM4MD;
    case 0x82: return TM5MD;
    case 0x84: /* fall through */
    case 0x85: return TM6MD;
    case 0x90: return TM4BR;
    case 0x92: return TM5BR;
    case 0xa0: return TM4BC;
    case 0xa2: return TM5BC;
    case 0xa4: return TM6BC;
    case 0xb4: return TM6MDA;
    case 0xb5: return TM6MDB;
    case 0xc4: return TM6CA;
    case 0xd4: return TM6CB;
    default: 
      {
	hw_abort (me, "bad address");
	return -1;
      }
    }
}

static void
read_mode_reg (struct hw *me,
	       struct mn103tim *timers,
	       int timer_nr,
	       void *dest,
	       unsigned nr_bytes)
{
  unsigned16 val16;
  unsigned32 val32;

  switch ( nr_bytes )
    {
    case 1:
      /* Accessing 1 byte is ok for all mode registers. */
      if ( timer_nr == 6 )
	{
	  *(unsigned8*)dest = timers->tm6md0;
	}
      else
	{
	  *(unsigned8*)dest = timers->reg[timer_nr].mode;
	}
      break;

    case 2:
      if ( timer_nr == 6 )
	{
	  *(unsigned16 *)dest = (timers->tm6md0 << 8) | timers->tm6md1;
	}
      else if ( timer_nr == 0 || timer_nr == 2 )
	{
	  val16 = (timers->reg[timer_nr].mode << 8)
	    | timers->reg[timer_nr+1].mode;
	  *(unsigned16*)dest = val16;
	}
      else
	{
	  hw_abort (me, "bad read size of 2 bytes to TM%dMD.", timer_nr);
	}
      break;

    case 4:
      if ( timer_nr == 0 )
	{
	  val32 = (timers->reg[0].mode << 24 )
	    | (timers->reg[1].mode << 16)
	    | (timers->reg[2].mode << 8)
	    | timers->reg[3].mode;
	  *(unsigned32*)dest = val32;
	}
      else
	{
	  hw_abort (me, "bad read size of 4 bytes to TM%dMD.", timer_nr);
	}
      break;

    default:
      hw_abort (me, "bad read size of %d bytes to TM%dMD.",
		nr_bytes, timer_nr);
    }
}


static void
read_base_reg (struct hw *me,
	       struct mn103tim *timers,
	       int timer_nr,
	       void *dest,
	       unsigned  nr_bytes)
{
  unsigned16 val16;
  unsigned32 val32;

  /* Check nr_bytes: accesses of 1, 2 and 4 bytes allowed depending on timer. */
  switch ( nr_bytes )
    {
    case 1:
      /* Reading 1 byte is ok for all registers. */
      if ( timer_nr < NR_8BIT_TIMERS )
	{
	  *(unsigned8*)dest = timers->reg[timer_nr].base;
	}
      break;

    case 2:
      if ( timer_nr == 1 || timer_nr == 3 )
	{
	  hw_abort (me, "bad read size of 2 bytes to TM%dBR.", timer_nr);
	}
      else
	{
	  if ( timer_nr < NR_8BIT_TIMERS )
	    {
	      val16 = (timers->reg[timer_nr].base<<8)
		| timers->reg[timer_nr+1].base;
	    }
	  else 
	    {
	      val16 = timers->reg[timer_nr].base;
	    }
	  *(unsigned16*)dest = val16;
	}
      break;

    case 4:
      if ( timer_nr == 0 )
	{
	  val32 = (timers->reg[0].base << 24) | (timers->reg[1].base << 16)
	    | (timers->reg[2].base << 8) | timers->reg[3].base;
	  *(unsigned32*)dest = val32;
	}
      else if ( timer_nr == 4 ) 
	{
	  val32 = (timers->reg[4].base << 16) | timers->reg[5].base;
	  *(unsigned32*)dest = val32;
	}
      else
	{
	  hw_abort (me, "bad read size of 4 bytes to TM%dBR.", timer_nr);
	}
      break;

    default:
      hw_abort (me, "bad read size must of %d bytes to TM%dBR.",
		nr_bytes, timer_nr); 
    }
}


static void
read_counter (struct hw *me,
	      struct mn103tim *timers,
	      int timer_nr,
	      void *dest,
	      unsigned  nr_bytes)
{
  unsigned32 val;

  if ( NULL == timers->timer[timer_nr].event )
    {
      /* Timer is not counting, use value in base register. */
      if ( timer_nr == 6 )
	{
	  val = 0;  /* timer 6 is an up counter */
	}
      else
	{
	  val = timers->reg[timer_nr].base;
	}
    }
  else
    {
      if ( timer_nr == 6 )  /* timer 6 is an up counter. */
	{
	  val = hw_event_queue_time(me) - timers->timer[timer_nr].start;
	}
      else
	{
	  /* ticks left = start time + div ratio - curr time */
	  /* Cannot use base register because it can be written during counting and it
	     doesn't affect counter until underflow occurs. */
	  
	  val = timers->timer[timer_nr].start + timers->timer[timer_nr].div_ratio
	    - hw_event_queue_time(me);
	}
    }

  switch (nr_bytes) {
  case 1:
    *(unsigned8 *)dest = val;
    break;
    
  case 2:
    *(unsigned16 *)dest = val;
    break;

  case 4:
    *(unsigned32 *)dest = val;
    break;

  default:
    hw_abort(me, "bad read size for reading counter");
  }
      
}


static void
read_special_timer6_reg (struct hw *me,
			 struct mn103tim *timers,
			 int timer_nr,
			 void *dest,
			 unsigned  nr_bytes)
{
  unsigned32 val;

  switch (nr_bytes) {
  case 1:
    {
      switch ( timer_nr ) {
      case TM6MDA:
	*(unsigned8 *)dest = timers->tm6mda;
	break;
    
      case TM6MDB:
	*(unsigned8 *)dest = timers->tm6mdb;
	break;
    
      case TM6CA:
	*(unsigned8 *)dest = timers->tm6ca;
	break;
    
      case TM6CB:
	*(unsigned8 *)dest = timers->tm6cb;
	break;
      
      default:
	break;
      }
      break;
    }
    
  case 2:
    if ( timer_nr == TM6CA )
      {
	*(unsigned16 *)dest = timers->tm6ca;
      }
    else if ( timer_nr == TM6CB )
      {
	*(unsigned16 *)dest = timers->tm6cb;
      }
    else
      {
	hw_abort(me, "bad read size for timer 6 mode A/B register");
      }
    break;

  default:
    hw_abort(me, "bad read size for timer 6 register");
  }
      
}


static unsigned
mn103tim_io_read_buffer (struct hw *me,
			 void *dest,
			 int space,
			 unsigned_word base,
			 unsigned nr_bytes)
{
  struct mn103tim *timers = hw_data (me);
  enum timer_register_types timer_reg;

  HW_TRACE ((me, "read 0x%08lx %d", (long) base, (int) nr_bytes));

  timer_reg = decode_addr (me, timers, base);

  /* It can be either a mode register, a base register, a binary counter, */
  /* or a special timer 6 register.  Check in that order. */
  if ( timer_reg >= FIRST_MODE_REG && timer_reg <= LAST_MODE_REG )
    {
      read_mode_reg(me, timers, timer_reg-FIRST_MODE_REG, dest, nr_bytes);
    }
  else if ( timer_reg <= LAST_BASE_REG )
    {
      read_base_reg(me, timers, timer_reg-FIRST_BASE_REG, dest, nr_bytes);
    }
  else if ( timer_reg <= LAST_COUNTER )
    {
      read_counter(me, timers, timer_reg-FIRST_COUNTER, dest, nr_bytes);
    }
  else if ( timer_reg <= LAST_TIMER_REG )
    {
      read_special_timer6_reg(me, timers, timer_reg, dest, nr_bytes);
    }
  else
    {
      hw_abort(me, "invalid timer register address.");
    }

  return nr_bytes;
}     


static void
do_counter_event (struct hw *me,
		  void *data)
{
  struct mn103tim *timers = hw_data(me);
  long timer_nr = (long) data;
  int next_timer;

  /* Check if counting is still enabled. */
  if ( (timers->reg[timer_nr].mode & count_mask) != 0 )
    {
      /* Generate an interrupt for the timer underflow (TIMERn_UFLOW). */

      /* Port event occurs on port of last cascaded timer. */
      /* This works across timer range from 0 to NR_REG_TIMERS because */
      /* the first 16 bit timer (timer 4) is not allowed to be set as  */
      /* a cascading timer. */
      for ( next_timer = timer_nr+1; next_timer < NR_REG_TIMERS; ++next_timer )
	{
	  if ( (timers->reg[next_timer].mode & clock_mask) != clk_cascaded )
	    {
	      break;
	    }
	}
      hw_port_event (me, next_timer-1, 1);

      /* Schedule next timeout.  */
      timers->timer[timer_nr].start = hw_event_queue_time(me);
      /* FIX: Check if div_ratio has changed and if it's now 0. */
      timers->timer[timer_nr].event
	= hw_event_queue_schedule (me, timers->timer[timer_nr].div_ratio,
				   do_counter_event, (void *)timer_nr);
    }
  else
    {
      timers->timer[timer_nr].event = NULL;
    }

}


static void
do_counter6_event (struct hw *me,
		  void *data)
{
  struct mn103tim *timers = hw_data(me);
  long timer_nr = (long) data;
  int next_timer;

  /* Check if counting is still enabled. */
  if ( (timers->reg[timer_nr].mode & count_mask) != 0 )
    {
      /* Generate an interrupt for the timer underflow (TIMERn_UFLOW). */
      hw_port_event (me, timer_nr, 1);

      /* Schedule next timeout.  */
      timers->timer[timer_nr].start = hw_event_queue_time(me);
      /* FIX: Check if div_ratio has changed and if it's now 0. */
      timers->timer[timer_nr].event
	= hw_event_queue_schedule (me, timers->timer[timer_nr].div_ratio,
				   do_counter6_event, (void *)timer_nr);
    }
  else
    {
      timers->timer[timer_nr].event = NULL;
    }

}

static void
write_base_reg (struct hw *me,
		struct mn103tim *timers,
		int timer_nr,
		const void *source,
		unsigned  nr_bytes)
{
  unsigned i;
  const unsigned8 *buf8 = source;
  const unsigned16 *buf16 = source;

  /* If TMnCNE == 0 (counting is off),  writing to the base register
     (TMnBR) causes a simultaneous write to the counter reg (TMnBC).
     Else, the TMnBC is reloaded with the value from TMnBR when
     underflow occurs.  Since the counter register is not explicitly
     maintained, this functionality is handled in read_counter. */

  /* Check nr_bytes: write of 1, 2 or 4 bytes allowed depending on timer. */
  switch ( nr_bytes )
    {
    case 1:
      /* Storing 1 byte is ok for all registers. */
      timers->reg[timer_nr].base = buf8[0];
      break;

    case 2:
      if ( timer_nr == 1 || timer_nr == 3 )
	{
	  hw_abort (me, "bad write size of 2 bytes to TM%dBR.", timer_nr);
	}
      else
	{
	  if ( timer_nr < NR_8BIT_TIMERS )
	    {
	      timers->reg[timer_nr].base = buf8[0];
	      timers->reg[timer_nr+1].base = buf8[1];
	    }
	  else 
	    {
	      timers->reg[timer_nr].base = buf16[0];
	    }
	}
      break;

    case 4:
      if ( timer_nr == 0 )
	{
	  timers->reg[0].base = buf8[0];
	  timers->reg[1].base = buf8[1];
	  timers->reg[2].base = buf8[2];
	  timers->reg[3].base = buf8[3];
	}
      else if ( timer_nr == 4 )
	{
	  timers->reg[4].base = buf16[0];
	  timers->reg[5].base = buf16[1];
	}
      else
	{
	  hw_abort (me, "bad write size of 4 bytes to TM%dBR.", timer_nr);
	}
      break;

    default:
      hw_abort (me, "bad write size must of %d bytes to TM%dBR.",
		nr_bytes, timer_nr);
    }
     
}

static void
write_mode_reg (struct hw *me,
		struct mn103tim *timers,
		long timer_nr,
		const void *source,
		unsigned nr_bytes)
     /* for timers 0 to 5 */
{
  unsigned i;
  unsigned8 mode_val, next_mode_val;
  unsigned32 div_ratio;

  if ( nr_bytes != 1 )
    {
      hw_abort (me, "bad write size of %d bytes to TM%ldMD.", nr_bytes,
		timer_nr);
    }

  mode_val = *(unsigned8 *)source;
  timers->reg[timer_nr].mode = mode_val;
      
  if ( ( mode_val & count_and_load_mask ) == count_and_load_mask )
    {
      hw_abort(me, "Cannot load base reg and start counting simultaneously.");
    }
  if ( ( mode_val & bits2to5_mask ) != 0 )
    {
      hw_abort(me, "Cannot write to bits 2 to 5 of mode register");
    }

  if ( mode_val & count_mask )
    {
      /* - de-schedule any previous event. */
      /* - add new event to queue to start counting. */
      /* - assert that counter == base reg? */

      /* For cascaded timers, */
      if ( (mode_val & clock_mask) == clk_cascaded )
	{
	  if ( timer_nr == 0 || timer_nr == 4 )
	    {
	      hw_abort(me, "Timer %ld cannot be cascaded.", timer_nr);
	    }
	}
      else
	{
	  div_ratio = timers->reg[timer_nr].base;

	  /* Check for cascading. */
	  if ( timer_nr < NR_8BIT_TIMERS )
	    {
	      for ( i = timer_nr + 1; i <= 3; ++i ) 
		{
		  next_mode_val = timers->reg[i].mode;
		  if ( ( next_mode_val & clock_mask ) == clk_cascaded )
		    {
		      /* Check that CNE is on. */
		      if ( ( next_mode_val & count_mask ) == 0 ) 
			{
			  hw_abort (me, "cascaded timer not ready for counting");
			}
		      ASSERT(timers->timer[i].event == NULL);
		      ASSERT(timers->timer[i].div_ratio == 0);
		      div_ratio = div_ratio
			| (timers->reg[i].base << (8*(i-timer_nr)));
		    }
		  else
		    {
		      break;
		    }
		}
	    }
	  else
	    {
	      /* Mode register for a 16 bit timer */
	      next_mode_val = timers->reg[timer_nr+1].mode;
	      if ( ( next_mode_val & clock_mask ) == clk_cascaded )
		{
		  /* Check that CNE is on. */
		  if ( ( next_mode_val & count_mask ) == 0 ) 
		    {
		      hw_abort (me, "cascaded timer not ready for counting");
		    }
		  ASSERT(timers->timer[timer_nr+1].event == NULL);
		  ASSERT(timers->timer[timer_nr+1].div_ratio == 0);
		  div_ratio = div_ratio | (timers->reg[timer_nr+1].base << 16);
		}
	    }

	  timers->timer[timer_nr].div_ratio = div_ratio;

	  if ( NULL != timers->timer[timer_nr].event )
	    {
	      hw_event_queue_deschedule (me, timers->timer[timer_nr].event);
	      timers->timer[timer_nr].event = NULL;
	    }

	  if ( div_ratio > 0 )
	    {
	      /* Set start time. */
	      timers->timer[timer_nr].start = hw_event_queue_time(me);
	      timers->timer[timer_nr].event
		= hw_event_queue_schedule(me, div_ratio,
					  do_counter_event,
					  (void *)(timer_nr)); 
	    }
	}
    }
  else
    {
      /* Turn off counting */
      if ( NULL != timers->timer[timer_nr].event )
	{
	  ASSERT((timers->reg[timer_nr].mode & clock_mask) != clk_cascaded);
	  hw_event_queue_deschedule (me, timers->timer[timer_nr].event);
	  timers->timer[timer_nr].event = NULL;
	}
      else
	{
	  if ( (timers->reg[timer_nr].mode & clock_mask) == clk_cascaded )
	    {
	      ASSERT(timers->timer[timer_nr].event == NULL);
	    }
	}
      
    }

}

static void
write_tm6md (struct hw *me,
	     struct mn103tim *timers,
	     unsigned_word address,
	     const void *source,
	     unsigned nr_bytes)
{
  unsigned8 mode_val0 = 0x00, mode_val1 = 0x00;
  unsigned32 div_ratio;
  long timer_nr = 6;

  unsigned_word offset = address - timers->block[0].base;
  
  if ((offset != 0x84 && nr_bytes > 1) || nr_bytes > 2 )
    {
      hw_abort (me, "Bad write size of %d bytes to TM6MD", nr_bytes);
    }

  if ( offset == 0x84 )  /* address of TM6MD */
    {
      /*  Fill in first byte of mode */
      mode_val0 = *(unsigned8 *)source;
      timers->tm6md0 = mode_val0;
    
      if ( ( mode_val0 & 0x26 ) != 0 )
	{
	  hw_abort(me, "Cannot write to bits 5, 3, and 2 of TM6MD");
	}
    }
  
  if ( offset == 0x85 || nr_bytes == 2 )
    {
      /*  Fill in second byte of mode */
      if ( nr_bytes == 2 )
	{
	  mode_val1 = *(unsigned8 *)source+1;
	}
      else
	{
	  mode_val1 = *(unsigned8 *)source;
	}

      timers->tm6md1 = mode_val1;

      if ( ( mode_val1 & count_and_load_mask ) == count_and_load_mask )
	{
	  hw_abort(me, "Cannot load base reg and start counting simultaneously.");
	}
      if ( ( mode_val1 & bits0to2_mask ) != 0 )
	{
	  hw_abort(me, "Cannot write to bits 8 to 10 of TM6MD");
	}
    }

  if ( mode_val1 & count_mask )
    {
      /* - de-schedule any previous event. */
      /* - add new event to queue to start counting. */
      /* - assert that counter == base reg? */

      div_ratio = timers->tm6ca;  /* binary counter for timer 6 */
      timers->timer[timer_nr].div_ratio = div_ratio;
      if ( NULL != timers->timer[timer_nr].event )
	{
	  hw_event_queue_deschedule (me, timers->timer[timer_nr].event);
	  timers->timer[timer_nr].event = NULL;
	}

      if ( div_ratio > 0 )
	{
	  /* Set start time. */
	  timers->timer[timer_nr].start = hw_event_queue_time(me);
	  timers->timer[timer_nr].event
	    = hw_event_queue_schedule(me, div_ratio,
				      do_counter6_event,
				      (void *)(timer_nr)); 
	}
    }
  else
    {
      /* Turn off counting */
      if ( NULL != timers->timer[timer_nr].event )
	{
	  hw_event_queue_deschedule (me, timers->timer[timer_nr].event);
	  timers->timer[timer_nr].event = NULL;
	}
    }
}



static void
write_special_timer6_reg (struct hw *me,
			  struct mn103tim *timers,
			  int timer_nr,
			  const void *source,
			  unsigned  nr_bytes)
{
  unsigned32 val;

  switch (nr_bytes) {
  case 1:
    {
      switch ( timer_nr ) {
      case TM6MDA:
	timers->tm6mda = *(unsigned8 *)source;
	break;
    
      case TM6MDB:
	timers->tm6mdb = *(unsigned8 *)source;
	break;
    
      case TM6CA:
	timers->tm6ca = *(unsigned8 *)source;
	break;
    
      case TM6CB:
	timers->tm6cb = *(unsigned8 *)source;
	break;
      
      default:
	break;
      }
      break;
    }
    
  case 2:
    if ( timer_nr == TM6CA )
      {
	timers->tm6ca = *(unsigned16 *)source;
      }
    else if ( timer_nr == TM6CB )
      {
	timers->tm6cb = *(unsigned16 *)source;
      }
    else
      {
	hw_abort(me, "bad read size for timer 6 mode A/B register");
      }
    break;

  default:
    hw_abort(me, "bad read size for timer 6 register");
  }
      
}


static unsigned
mn103tim_io_write_buffer (struct hw *me,
			  const void *source,
			  int space,
			  unsigned_word base,
			  unsigned nr_bytes)
{
  struct mn103tim *timers = hw_data (me);
  enum timer_register_types timer_reg;

  HW_TRACE ((me, "write to 0x%08lx length %d with 0x%x", (long) base,
	     (int) nr_bytes, *(unsigned32 *)source));

  timer_reg = decode_addr (me, timers, base);

  /* It can be either a mode register, a base register, a binary counter, */
  /* or a special timer 6 register.  Check in that order. */
  if ( timer_reg <= LAST_MODE_REG )
    {
      if ( timer_reg == 6 ) 
	{
	  write_tm6md(me, timers, base, source, nr_bytes);
	}
      else
	{
	  write_mode_reg(me, timers, timer_reg-FIRST_MODE_REG,
			 source, nr_bytes);
	}
    }
  else if ( timer_reg <= LAST_BASE_REG )
    {
      write_base_reg(me, timers, timer_reg-FIRST_BASE_REG, source, nr_bytes);
    }
  else if ( timer_reg <= LAST_COUNTER )
    {
      hw_abort(me, "cannot write to counter");
    }
  else if ( timer_reg <= LAST_TIMER_REG )
    {
      write_special_timer6_reg(me, timers, timer_reg, source, nr_bytes);
    }
  else
    {
      hw_abort(me, "invalid reg type");
    }

  return nr_bytes;
}     


const struct hw_descriptor dv_mn103tim_descriptor[] = {
  { "mn103tim", mn103tim_finish, },
  { NULL },
};
