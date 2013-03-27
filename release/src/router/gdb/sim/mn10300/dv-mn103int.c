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
#include "sim-hw.h"

/* DEVICE

   
   mn103int - mn103002 interrupt controller

   
   DESCRIPTION

   
   Implements the mn103002 interrupt controller described in the
   mn103002 user guide.


   PROPERTIES
   

   reg = <icr-adr> <icr-siz> <iagr-adr> <iadr-siz> <extmd-adr> <extmd-siz>

   Specify the address of the ICR (total of 30 registers), IAGR and
   EXTMD registers (within the parent bus).

   The reg property value `0x34000100 0x7C 0x34000200 0x8 0x3400280
   0x8' locates the interrupt controller at the addresses specified in
   the mn103002 interrupt controller user guide.


   PORTS


   nmi (output)

   Non-maskable interrupt output port.  An event on this output ports
   indicates a NMI request from the interrupt controller.  The value
   attached to the event should be ignored.


   level (output)

   Maskable interrupt level output port.  An event on this output port
   indicates a maskable interrupt request at the specified level.  The
   event value defines the level being requested.

   The interrupt controller will generate an event on this port
   whenever there is a change to the internal state of the interrupt
   controller.


   ack (input)

   Signal from processor indicating that a maskable interrupt has been
   accepted and the interrupt controller should latch the IAGR with
   value of the current highest priority interrupting group.

   The event value is the interrupt level being accepted by the
   processor.  It should be consistent with the most recent LEVEL sent
   to the processor from the interrupt controller.


   int[0..100] (input)

   Level or edge triggered interrupt input port.  Each of the 30
   groups (0..30) can have up to 4 (0..3) interrupt inputs.  The
   interpretation of a port event/value is determined by the
   configuration of the corresponding interrupt group.

   For convenience, numerous aliases to these interrupt inputs are
   provided.


   BUGS


   For edge triggered interrupts, the interrupt controller does not
   differentiate between POSITIVE (rising) and NEGATIVE (falling)
   edges.  Instead any input port event is considered to be an
   interrupt trigger.

   For level sensitive interrupts, the interrupt controller ignores
   active HIGH/LOW settings and instead always interprets a nonzero
   port value as an interrupt assertion and a zero port value as a
   negation.

   */


/* The interrupt groups - numbered according to mn103002 convention */

enum mn103int_trigger {
  ACTIVE_LOW,
  ACTIVE_HIGH,
  POSITIVE_EDGE,
  NEGATIVE_EDGE,
};

enum mn103int_type {
  NMI_GROUP,
  LEVEL_GROUP,
};

struct mn103int_group {
  int gid;
  int level;
  unsigned enable;
  unsigned request;
  unsigned input;
  enum mn103int_trigger trigger;
  enum mn103int_type type;
};

enum {
  FIRST_NMI_GROUP = 0,
  LAST_NMI_GROUP = 1,
  FIRST_LEVEL_GROUP = 2,
  LAST_LEVEL_GROUP = 30,
  NR_GROUPS,
};

enum {
  LOWEST_LEVEL = 7,
};

/* The interrupt controller register address blocks */

struct mn103int_block {
  unsigned_word base;
  unsigned_word bound;
};

enum { ICR_BLOCK, IAGR_BLOCK, EXTMD_BLOCK, NR_BLOCKS };


struct mn103int {
  struct mn103int_block block[NR_BLOCKS];
  struct mn103int_group group[NR_GROUPS];
  unsigned interrupt_accepted_group;
};



/* output port ID's */ 

enum {
  NMI_PORT,
  LEVEL_PORT,
};


/* input port ID's */

enum {
  G0_PORT = 0,
  G1_PORT = 4,
  G2_PORT = 8,
  G3_PORT = 12,
  G4_PORT = 16,
  G5_PORT = 20,
  G6_PORT = 24,
  G7_PORT = 28,
  G8_PORT = 32,
  G9_PORT = 36,
  G10_PORT = 40,
  G11_PORT = 44,
  G12_PORT = 48,
  G13_PORT = 52,
  G14_PORT = 56,
  G15_PORT = 60,
  G16_PORT = 64,
  G17_PORT = 68,
  G18_PORT = 72,
  G19_PORT = 76,
  G20_PORT = 80,
  G21_PORT = 84,
  G22_PORT = 88,
  G23_PORT = 92,
  IRQ0_PORT = G23_PORT,
  G24_PORT = 96,
  G25_PORT = 100,
  G26_PORT = 104,
  G27_PORT = 108,
  IRQ4_PORT = G27_PORT,
  G28_PORT = 112,
  G29_PORT = 116,
  G30_PORT = 120,
  NR_G_PORTS = 124,
  ACK_PORT,
};

static const struct hw_port_descriptor mn103int_ports[] = {

  /* interrupt outputs */

  { "nmi", NMI_PORT, 0, output_port, },
  { "level", LEVEL_PORT, 0, output_port, },

  /* interrupt ack (latch) input from cpu */

  { "ack", ACK_PORT, 0, input_port, },

  /* interrupt inputs (as names) */

  { "nmirq", G0_PORT + 0, 0, input_port, },
  { "watchdog", G0_PORT + 1, 0, input_port, },
  { "syserr", G0_PORT + 2, 0, input_port, },

  { "timer-0-underflow", G2_PORT, 0, input_port, },
  { "timer-1-underflow", G3_PORT, 0, input_port, },
  { "timer-2-underflow", G4_PORT, 0, input_port, },
  { "timer-3-underflow", G5_PORT, 0, input_port, },
  { "timer-4-underflow", G6_PORT, 0, input_port, },
  { "timer-5-underflow", G7_PORT, 0, input_port, },
  { "timer-6-underflow", G8_PORT, 0, input_port, },

  { "timer-6-compare-a", G9_PORT, 0, input_port, },
  { "timer-6-compare-b", G10_PORT, 0, input_port, },

  { "dma-0-end", G12_PORT, 0, input_port, },
  { "dma-1-end", G13_PORT, 0, input_port, },
  { "dma-2-end", G14_PORT, 0, input_port, },
  { "dma-3-end", G15_PORT, 0, input_port, },

  { "serial-0-receive",  G16_PORT, 0, input_port, },
  { "serial-0-transmit", G17_PORT, 0, input_port, },

  { "serial-1-receive",  G18_PORT, 0, input_port, },
  { "serial-1-transmit", G19_PORT, 0, input_port, },

  { "serial-2-receive",  G20_PORT, 0, input_port, },
  { "serial-2-transmit", G21_PORT, 0, input_port, },

  { "irq-0", G23_PORT, 0, input_port, },
  { "irq-1", G24_PORT, 0, input_port, },
  { "irq-2", G25_PORT, 0, input_port, },
  { "irq-3", G26_PORT, 0, input_port, },
  { "irq-4", G27_PORT, 0, input_port, },
  { "irq-5", G28_PORT, 0, input_port, },
  { "irq-6", G29_PORT, 0, input_port, },
  { "irq-7", G30_PORT, 0, input_port, },

  /* interrupt inputs (as generic numbers) */

  { "int", 0, NR_G_PORTS, input_port, },

  { NULL, },
};


/* Macros for extracting/restoring the various register bits */

#define EXTRACT_ID(X) (LSEXTRACTED8 ((X), 3, 0))
#define INSERT_ID(X) (LSINSERTED8 ((X), 3, 0))

#define EXTRACT_IR(X) (LSEXTRACTED8 ((X), 7, 4))
#define INSERT_IR(X) (LSINSERTED8 ((X), 7, 4))

#define EXTRACT_IE(X) (LSEXTRACTED8 ((X), 3, 0))
#define INSERT_IE(X) (LSINSERTED8 ((X), 3, 0))

#define EXTRACT_LV(X) (LSEXTRACTED8 ((X), 6, 4))
#define INSERT_LV(X) (LSINSERTED8 ((X), 6, 4))



/* Finish off the partially created hw device.  Attach our local
   callbacks.  Wire up our port names etc */

static hw_io_read_buffer_method mn103int_io_read_buffer;
static hw_io_write_buffer_method mn103int_io_write_buffer;
static hw_port_event_method mn103int_port_event;
static hw_ioctl_method mn103int_ioctl;



static void
attach_mn103int_regs (struct hw *me,
		      struct mn103int *controller)
{
  int i;
  if (hw_find_property (me, "reg") == NULL)
    hw_abort (me, "Missing \"reg\" property");
  for (i = 0; i < NR_BLOCKS; i++)
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
      controller->block[i].base = attach_address;
      hw_unit_size_to_attach_size (hw_parent (me),
				   &reg.size,
				   &attach_size, me);
      controller->block[i].bound = attach_address + (attach_size - 1);
      hw_attach_address (hw_parent (me),
			 0,
			 attach_space, attach_address, attach_size,
			 me);
    }
}

static void
mn103int_finish (struct hw *me)
{
  int gid;
  struct mn103int *controller;

  controller = HW_ZALLOC (me, struct mn103int);
  set_hw_data (me, controller);
  set_hw_io_read_buffer (me, mn103int_io_read_buffer);
  set_hw_io_write_buffer (me, mn103int_io_write_buffer);
  set_hw_ports (me, mn103int_ports);
  set_hw_port_event (me, mn103int_port_event);
  me->to_ioctl = mn103int_ioctl;

  /* Attach ourself to our parent bus */
  attach_mn103int_regs (me, controller);

  /* Initialize all the groups according to their default configuration */
  for (gid = 0; gid < NR_GROUPS; gid++)
    {
      struct mn103int_group *group = &controller->group[gid];
      group->trigger = NEGATIVE_EDGE;
      group->gid = gid;
      if (FIRST_NMI_GROUP <= gid && gid <= LAST_NMI_GROUP)
	{
	  group->enable = 0xf;
	  group->type = NMI_GROUP;
	}
      else if (FIRST_LEVEL_GROUP <= gid && gid <= LAST_LEVEL_GROUP)
	{
	  group->enable = 0x0;
	  group->type = LEVEL_GROUP;
	}
      else
	hw_abort (me, "internal error - unknown group id");
    }
}



/* Perform the nasty work of figuring out which of the interrupt
   groups should have its interrupt delivered. */

static int
find_highest_interrupt_group (struct hw *me,
			      struct mn103int *controller)
{
  int gid;
  int selected;

  /* FIRST_NMI_GROUP (group zero) is used as a special default value
     when searching for an interrupt group.*/
  selected = FIRST_NMI_GROUP; 
  controller->group[FIRST_NMI_GROUP].level = 7;
  
  for (gid = FIRST_LEVEL_GROUP; gid <= LAST_LEVEL_GROUP; gid++)
    {
      struct mn103int_group *group = &controller->group[gid];
      if ((group->request & group->enable) != 0)
	{
	  /* Remember, lower level, higher priority.  */
	  if (group->level < controller->group[selected].level)
	    {
	      selected = gid;
	    }
	}
    }
  return selected;
}


/* Notify the processor of an interrupt level update */

static void
push_interrupt_level (struct hw *me,
		      struct mn103int *controller)
{
  int selected = find_highest_interrupt_group (me, controller);
  int level = controller->group[selected].level;
  HW_TRACE ((me, "port-out - selected=%d level=%d", selected, level));
  hw_port_event (me, LEVEL_PORT, level);
}


/* An event arrives on an interrupt port */

static void
mn103int_port_event (struct hw *me,
		     int my_port,
		     struct hw *source,
		     int source_port,
		     int level)
{
  struct mn103int *controller = hw_data (me);

  switch (my_port)
    {

    case ACK_PORT:
      {
	int selected = find_highest_interrupt_group (me, controller);
	if (controller->group[selected].level != level)
	  hw_abort (me, "botched level synchronisation");
	controller->interrupt_accepted_group = selected;	
	HW_TRACE ((me, "port-event port=ack level=%d - selected=%d",
		   level, selected));
	break;
      }

    default:
      {
	int gid;
	int iid;
	struct mn103int_group *group;
	unsigned interrupt;
	if (my_port > NR_G_PORTS)
	  hw_abort (me, "Event on unknown port %d", my_port);

	/* map the port onto an interrupt group */
	gid = (my_port % NR_G_PORTS) / 4;
	group = &controller->group[gid];
	iid = (my_port % 4);
	interrupt = 1 << iid;

	/* update our cached input */
	if (level)
	  group->input |= interrupt;
	else
	  group->input &= ~interrupt;

	/* update the request bits */
	switch (group->trigger)
	  {
	  case ACTIVE_LOW:
	  case ACTIVE_HIGH:
	    if (level)
	      group->request |= interrupt;
	    break;
	  case NEGATIVE_EDGE:
	  case POSITIVE_EDGE:
	    group->request |= interrupt;
	  }

	/* force a corresponding output */
	switch (group->type)
	  {

	  case NMI_GROUP:
	    {
	      /* for NMI's the event is the trigger */
	      HW_TRACE ((me, "port-in port=%d group=%d interrupt=%d - NMI",
			 my_port, gid, iid));
	      if ((group->request & group->enable) != 0)
		{
		  HW_TRACE ((me, "port-out NMI"));
		  hw_port_event (me, NMI_PORT, 1);
		}
	      break;
	    }
	      
	  case LEVEL_GROUP:
	    {
	      /* if an interrupt is now pending */
	      HW_TRACE ((me, "port-in port=%d group=%d interrupt=%d - INT",
			 my_port, gid, iid));
	      push_interrupt_level (me, controller);
	      break;
	    }
	  }
	break;
      }

    }
}

/* Read/write to to an ICR (group control register) */

static struct mn103int_group *
decode_group (struct hw *me,
	      struct mn103int *controller,
	      unsigned_word base,
	      unsigned_word *offset)
{
  int gid = (base / 4) % NR_GROUPS;
  *offset = (base % 4);
  return &controller->group[gid];
}

static unsigned8
read_icr (struct hw *me,
	  struct mn103int *controller,
	  unsigned_word base)
{
  unsigned_word offset;
  struct mn103int_group *group = decode_group (me, controller, base, &offset);
  unsigned8 val = 0;
  switch (group->type)
    {

    case NMI_GROUP:
      switch (offset)
	{
	case 0:
	  val = INSERT_ID (group->request);
	  HW_TRACE ((me, "read-icr group=%d:0 nmi 0x%02x",
		     group->gid, val));
	  break;
	default:
	  break;
	}
      break;

    case LEVEL_GROUP:
      switch (offset)
	{
	case 0:
	  val = (INSERT_IR (group->request)
		 | INSERT_ID (group->request & group->enable));
	  HW_TRACE ((me, "read-icr group=%d:0 level 0x%02x",
		     group->gid, val));
	  break;
	case 1:
	  val = (INSERT_LV (group->level)
		 | INSERT_IE (group->enable));
	  HW_TRACE ((me, "read-icr level-%d:1 level 0x%02x",
		     group->gid, val));
	  break;
	}
      break;

    default:
      break;

    }

  return val;
}

static void
write_icr (struct hw *me,
	   struct mn103int *controller,
	   unsigned_word base,
	   unsigned8 val)
{
  unsigned_word offset;
  struct mn103int_group *group = decode_group (me, controller, base, &offset);
  switch (group->type)
    {

    case NMI_GROUP:
      switch (offset)
	{
	case 0:
	  HW_TRACE ((me, "write-icr group=%d:0 nmi 0x%02x",
		     group->gid, val));
	  group->request &= ~EXTRACT_ID (val);
	  break;
	  /* Special backdoor access to SYSEF flag from CPU.  See
             interp.c:program_interrupt(). */
	case 3:
	  HW_TRACE ((me, "write-icr-special group=%d:0 nmi 0x%02x",
		     group->gid, val));
	  group->request |= EXTRACT_ID (val);
	default:
	  break;
	}
      break;

    case LEVEL_GROUP:
      switch (offset)
	{
	case 0: /* request/detect */
	  /* Clear any ID bits and then set them according to IR */
	  HW_TRACE ((me, "write-icr group=%d:0 level 0x%02x %x:%x:%x",
		     group->gid, val,
		     group->request, EXTRACT_IR (val), EXTRACT_ID (val)));
	  group->request =
	    ((EXTRACT_IR (val) & EXTRACT_ID (val))
	     | (EXTRACT_IR (val) & group->request)
	     | (~EXTRACT_IR (val) & ~EXTRACT_ID (val) & group->request));
	  break;
	case 1: /* level/enable */
	  HW_TRACE ((me, "write-icr group=%d:1 level 0x%02x",
		     group->gid, val));
	  group->level = EXTRACT_LV (val);
	  group->enable = EXTRACT_IE (val);
	  break;
	default:
	  /* ignore */
	  break;
	}
      push_interrupt_level (me, controller);
      break;

    default:
      break;

    }
}


/* Read the IAGR (Interrupt accepted group register) */

static unsigned8
read_iagr (struct hw *me,
	   struct mn103int *controller,
	   unsigned_word offset)
{
  unsigned8 val;
  switch (offset)
    {
    case 0:
      {
	if (!(controller->group[controller->interrupt_accepted_group].request
	      & controller->group[controller->interrupt_accepted_group].enable))
	  {
	    /* oops, lost the request */
	    val = 0;
	    HW_TRACE ((me, "read-iagr:0 lost-0"));
	  }
	else
	  {
	    val = (controller->interrupt_accepted_group << 2);
	    HW_TRACE ((me, "read-iagr:0 %d", (int) val));
	  }
	break;
      }
    case 1:
      val = 0;
      HW_TRACE ((me, "read-iagr:1 %d", (int) val));
      break;
    default:
      val = 0;
      HW_TRACE ((me, "read-iagr 0x%08lx bad offset", (long) offset));
      break;
    }
  return val;
}


/* Reads/writes to the EXTMD (external interrupt trigger configuration
   register) */

static struct mn103int_group *
external_group (struct mn103int *controller,
		unsigned_word offset)
{
  switch (offset)
    {
    case 0:
      return &controller->group[IRQ0_PORT/4];
    case 1:
      return &controller->group[IRQ4_PORT/4];
    default:
      return NULL;
    }
}

static unsigned8
read_extmd (struct hw *me,
	    struct mn103int *controller,
	    unsigned_word offset)
{
  int gid;
  unsigned8 val = 0;
  struct mn103int_group *group = external_group (controller, offset);
  if (group != NULL)
    {
      for (gid = 0; gid < 4; gid++)
	{
	  val |= (group[gid].trigger << (gid * 2));
	}
    }
  HW_TRACE ((me, "read-extmd 0x%02lx", (long) val));
  return val;
}

static void
write_extmd (struct hw *me,
	     struct mn103int *controller,
	     unsigned_word offset,
	     unsigned8 val)
{
  int gid;
  struct mn103int_group *group = external_group (controller, offset);
  if (group != NULL)
    {
      for (gid = 0; gid < 4; gid++)
	{
	  group[gid].trigger = (val >> (gid * 2)) & 0x3;
	  /* MAYBE: interrupts already pending? */
	}
    }
  HW_TRACE ((me, "write-extmd 0x%02lx", (long) val));
}


/* generic read/write */

static int
decode_addr (struct hw *me,
	     struct mn103int *controller,
	     unsigned_word address,
	     unsigned_word *offset)
{
  int i;
  for (i = 0; i < NR_BLOCKS; i++)
    {
      if (address >= controller->block[i].base
	  && address <= controller->block[i].bound)
	{
	  *offset = address - controller->block[i].base;
	  return i;
	}
    }
  hw_abort (me, "bad address");
  return -1;
}

static unsigned
mn103int_io_read_buffer (struct hw *me,
			 void *dest,
			 int space,
			 unsigned_word base,
			 unsigned nr_bytes)
{
  struct mn103int *controller = hw_data (me);
  unsigned8 *buf = dest;
  unsigned byte;
  /* HW_TRACE ((me, "read 0x%08lx %d", (long) base, (int) nr_bytes)); */
  for (byte = 0; byte < nr_bytes; byte++)
    {
      unsigned_word address = base + byte;
      unsigned_word offset;
      switch (decode_addr (me, controller, address, &offset))
	{
	case ICR_BLOCK:
	  buf[byte] = read_icr (me, controller, offset);
	  break;
	case IAGR_BLOCK:
	  buf[byte] = read_iagr (me, controller, offset);
	  break;
	case EXTMD_BLOCK:
	  buf[byte] = read_extmd (me, controller, offset);
	  break;
	default:
	  hw_abort (me, "bad switch");
	}
    }
  return nr_bytes;
}     

static unsigned
mn103int_io_write_buffer (struct hw *me,
			  const void *source,
			  int space,
			  unsigned_word base,
			  unsigned nr_bytes)
{
  struct mn103int *controller = hw_data (me);
  const unsigned8 *buf = source;
  unsigned byte;
  /* HW_TRACE ((me, "write 0x%08lx %d", (long) base, (int) nr_bytes)); */
  for (byte = 0; byte < nr_bytes; byte++)
    {
      unsigned_word address = base + byte;
      unsigned_word offset;
      switch (decode_addr (me, controller, address, &offset))
	{
	case ICR_BLOCK:
	  write_icr (me, controller, offset, buf[byte]);
	  break;
	case IAGR_BLOCK:
	  /* not allowed */
	  break;
	case EXTMD_BLOCK:
	  write_extmd (me, controller, offset, buf[byte]);
	  break;
	default:
	  hw_abort (me, "bad switch");
	}
    }
  return nr_bytes;
}     

static int
mn103int_ioctl(struct hw *me,
	       hw_ioctl_request request,
	       va_list ap)
{
  struct mn103int *controller = (struct mn103int *)hw_data(me);
  controller->group[0].request = EXTRACT_ID(4);
  mn103int_port_event(me, 2 /* nmi_port(syserr) */, NULL, 0, 0);
  return 0;
}


const struct hw_descriptor dv_mn103int_descriptor[] = {
  { "mn103int", mn103int_finish, },
  { NULL },
};
