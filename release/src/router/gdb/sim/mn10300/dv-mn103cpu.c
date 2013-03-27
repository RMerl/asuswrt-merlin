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

   
   mn103cpu - mn10300 cpu virtual device

   
   DESCRIPTION

   
   Implements the external mn10300 functionality.  This includes the
   delivery of interrupts generated from other devices and the
   handling of device specific registers.


   PROPERTIES
   

   reg = <address> <size>

   Specify the address of the mn10300's control register block.  This
   block contains the Interrupt Vector Registers.

   The reg property value `0x20000000 0x42' locates the register block
   at the address specified in the mn10300 user guide.


   PORTS


   reset (input)

   Currently ignored.


   nmi (input)

   Deliver a non-maskable interrupt to the processor.


   level (input)

   Maskable interrupt level port port.  The interrupt controller
   notifies the processor of any change in the level of pending
   requested interrupts via this port.


   ack (output)

   Output signal indicating that the processor is delivering a level
   interrupt.  The value passed with the event specifies the level of
   the interrupt being delivered.


   BUGS


   When delivering an interrupt, this code assumes that there is only
   one processor (number 0).

   This code does not attempt to be efficient at handling pending
   interrupts.  It simply schedules the interrupt delivery handler
   every instruction cycle until all pending interrupts go away.  An
   alternative implementation might modify instructions that change
   the PSW and have them check to see if the change makes an interrupt
   delivery possible.

   */


/* The interrupt vectors */

enum { NR_VECTORS = 7, };


/* The interrupt controller register address blocks */

struct mn103cpu_block {
  unsigned_word base;
  unsigned_word bound;
};


struct mn103cpu {
  struct mn103cpu_block block;
  struct hw_event *pending_handler;
  int pending_level;
  int pending_nmi;
  int pending_reset;
  /* the visible registers */
  unsigned16 interrupt_vector[NR_VECTORS];
  unsigned16 internal_memory_control;
  unsigned16 cpu_mode;
};



/* input port ID's */ 

enum {
  RESET_PORT,
  NMI_PORT,
  LEVEL_PORT,
};


/* output port ID's */

enum {
  ACK_PORT,
};

static const struct hw_port_descriptor mn103cpu_ports[] = {

  /* interrupt inputs */
  { "reset", RESET_PORT, 0, input_port, },
  { "nmi", NMI_PORT, 0, input_port, },
  { "level", LEVEL_PORT, 0, input_port, },

  /* interrupt ack (latch) output from cpu */
  { "ack", ACK_PORT, 0, output_port, },

  { NULL, },
};


/* Finish off the partially created hw device.  Attach our local
   callbacks.  Wire up our port names etc */

static hw_io_read_buffer_method mn103cpu_io_read_buffer;
static hw_io_write_buffer_method mn103cpu_io_write_buffer;
static hw_port_event_method mn103cpu_port_event;

static void
attach_mn103cpu_regs (struct hw *me,
		      struct mn103cpu *controller)
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
  controller->block.base = attach_address;
  hw_unit_size_to_attach_size (hw_parent (me),
			       &reg.size,
			       &attach_size, me);
  controller->block.bound = attach_address + (attach_size - 1);
  if ((controller->block.base & 3) != 0)
    hw_abort (me, "cpu register block must be 4 byte aligned");
  hw_attach_address (hw_parent (me),
		     0,
		     attach_space, attach_address, attach_size,
		     me);
}


static void
mn103cpu_finish (struct hw *me)
{
  struct mn103cpu *controller;

  controller = HW_ZALLOC (me, struct mn103cpu);
  set_hw_data (me, controller);
  set_hw_io_read_buffer (me, mn103cpu_io_read_buffer);
  set_hw_io_write_buffer (me, mn103cpu_io_write_buffer);
  set_hw_ports (me, mn103cpu_ports);
  set_hw_port_event (me, mn103cpu_port_event);

  /* Attach ourself to our parent bus */
  attach_mn103cpu_regs (me, controller);

  /* Initialize the read-only registers */
  controller->pending_level = 7; /* FIXME */
  /* ... */
}



/* An event arrives on an interrupt port */

static void
deliver_mn103cpu_interrupt (struct hw *me,
			    void *data)
{
  struct mn103cpu *controller = hw_data (me);
  SIM_DESC simulator = hw_system (me);
  sim_cpu *cpu = STATE_CPU (simulator, 0);

  if (controller->pending_reset)
    {
      controller->pending_reset = 0;
      /* need to clear all registers et.al! */
      HW_TRACE ((me, "Reset!"));
      hw_abort (me, "Reset!");
    }
  else if (controller->pending_nmi)
    {
      controller->pending_nmi = 0;
      store_word (SP - 4, CIA_GET (cpu));
      store_half (SP - 8, PSW);
      PSW &= ~PSW_IE;
      SP = SP - 8;
      CIA_SET (cpu, 0x40000008);
      HW_TRACE ((me, "nmi pc=0x%08lx psw=0x%04x sp=0x%08lx",
		 (long) CIA_GET (cpu), (unsigned) PSW, (long) SP));
    }
  else if ((controller->pending_level < EXTRACT_PSW_LM)
	   && (PSW & PSW_IE))
    {
      /* Don't clear pending level.  Request continues to be pending
         until the interrupt controller clears/changes it */
      store_word (SP - 4, CIA_GET (cpu));
      store_half (SP - 8, PSW);
      PSW &= ~PSW_IE;
      PSW &= ~PSW_LM;
      PSW |= INSERT_PSW_LM (controller->pending_level);
      SP = SP - 8;
      CIA_SET (cpu, 0x40000000 + controller->interrupt_vector[controller->pending_level]);
      HW_TRACE ((me, "port-out ack %d", controller->pending_level));
      hw_port_event (me, ACK_PORT, controller->pending_level);
      HW_TRACE ((me, "int level=%d pc=0x%08lx psw=0x%04x sp=0x%08lx",
		 controller->pending_level,
		 (long) CIA_GET (cpu), (unsigned) PSW, (long) SP));
    }

  if (controller->pending_level < 7) /* FIXME */
    {
      /* As long as there is the potential need to deliver an
	 interrupt we keep rescheduling this routine. */
      if (controller->pending_handler != NULL)
	controller->pending_handler =
	  hw_event_queue_schedule (me, 1, deliver_mn103cpu_interrupt, NULL);
    }
  else
    {
      /* Don't bother re-scheduling the interrupt handler as there is
         nothing to deliver */
      controller->pending_handler = NULL;
    }

}


static void
mn103cpu_port_event (struct hw *me,
		     int my_port,
		     struct hw *source,
		     int source_port,
		     int level)
{
  struct mn103cpu *controller = hw_data (me);

  /* Schedule our event handler *now* */
  if (controller->pending_handler == NULL)
    controller->pending_handler =
      hw_event_queue_schedule (me, 0, deliver_mn103cpu_interrupt, NULL);

  switch (my_port)
    {
      
    case RESET_PORT:
      controller->pending_reset = 1;
      HW_TRACE ((me, "port-in reset"));
      break;
      
    case NMI_PORT:
      controller->pending_nmi = 1;
      HW_TRACE ((me, "port-in nmi"));
      break;
      
    case LEVEL_PORT:
      controller->pending_level = level;
      HW_TRACE ((me, "port-in level=%d", level));
      break;
      
    default:
      hw_abort (me, "bad switch");
      break;

    }
}


/* Read/write to a CPU register */

enum mn103cpu_regs {
  INVALID_REG,
  IVR0_REG,
  IVR1_REG,
  IVR2_REG,
  IVR3_REG,
  IVR4_REG,
  IVR5_REG,
  IVR6_REG,
  IMCR_REG,
  CPUM_REG,
};

static enum mn103cpu_regs
decode_mn103cpu_addr (struct hw *me,
		      struct mn103cpu *controller,
		      unsigned_word base)
{
  switch (base - controller->block.base)
    {
    case 0x000: return IVR0_REG;
    case 0x004: return IVR1_REG;
    case 0x008: return IVR2_REG;
    case 0x00c: return IVR3_REG;
    case 0x010: return IVR4_REG;
    case 0x014: return IVR5_REG;
    case 0x018: return IVR6_REG;
    case 0x020: return IMCR_REG;
    case 0x040: return CPUM_REG;
    default: return INVALID_REG;
    }
}

static unsigned
mn103cpu_io_read_buffer (struct hw *me,
			 void *dest,
			 int space,
			 unsigned_word base,
			 unsigned nr_bytes)
{
  struct mn103cpu *controller = hw_data (me);
  unsigned16 val = 0;
  enum mn103cpu_regs reg = decode_mn103cpu_addr (me, controller, base);

  switch (reg)
    {
    case IVR0_REG:
    case IVR1_REG:
    case IVR2_REG:
    case IVR3_REG:
    case IVR4_REG:
    case IVR5_REG:
    case IVR6_REG:
      val = controller->interrupt_vector[reg - IVR0_REG];
      break;
    case IMCR_REG:
      val = controller->internal_memory_control;
      break;
    case CPUM_REG:
      val = controller->cpu_mode;
      break;
    default:
      /* just ignore the read */
      break;
    }

  if (nr_bytes == 2)
    *(unsigned16*) dest = H2LE_2 (val);

  return nr_bytes;
}     

static unsigned
mn103cpu_io_write_buffer (struct hw *me,
			  const void *source,
			  int space,
			  unsigned_word base,
			  unsigned nr_bytes)
{
  struct mn103cpu *controller = hw_data (me);
  unsigned16 val;
  enum mn103cpu_regs reg;

  if (nr_bytes != 2)
    hw_abort (me, "must be two byte write");

  reg = decode_mn103cpu_addr (me, controller, base);
  val = LE2H_2 (* (unsigned16 *) source);

  switch (reg)
    {
    case IVR0_REG:
    case IVR1_REG:
    case IVR2_REG:
    case IVR3_REG:
    case IVR4_REG:
    case IVR5_REG:
    case IVR6_REG:
      controller->interrupt_vector[reg - IVR0_REG] = val;
      HW_TRACE ((me, "ivr%d = 0x%04lx", reg - IVR0_REG, (long) val));
      break;
    default:
      /* just ignore the write */
      break;
    }

  return nr_bytes;
}     


const struct hw_descriptor dv_mn103cpu_descriptor[] = {
  { "mn103cpu", mn103cpu_finish, },
  { NULL },
};
