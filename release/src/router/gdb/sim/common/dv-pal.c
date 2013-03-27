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
#include "sim-io.h"

/* NOTE: pal is naughty and grubs around looking at things outside of
   its immediate domain */
#include "hw-tree.h"

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

   
   pal - glue logic device containing assorted junk

   
   DESCRIPTION

   
   Typical hardware dependant hack.  This device allows the firmware
   to gain access to all the things the firmware needs (but the OS
   doesn't).

   The pal contains the following registers:

   |0	reset register (write, 8bit)
   |4	processor id register (read, 8bit)
   |8	interrupt register (8 - port, 9 - level) (write, 16bit)
   |12	processor count register (read, 8bit)

   |16	tty input fifo register (read, 8bit)
   |20	tty input status register (read, 8bit)
   |24	tty output fifo register (write, 8bit)
   |28	tty output status register (read, 8bit)

   |32  countdown register (read/write, 32bit, big-endian)
   |36  countdown value register (read, 32bit, big-endian)
   |40  timer register (read/write, 32bit, big-endian)
   |44  timer value register (read, 32bit, big-endian)

   RESET (write): halts the simulator.  The value written to the
   register is used as an exit status.
   
   PROCESSOR ID (read): returns the processor identifier (0 .. N-1) of
   the processor performing the read.
   
   INTERRUPT (write): This register must be written using a two byte
   store.  The low byte specifies a port and the upper byte specifies
   the a level.  LEVEL is driven on the specified port.  By
   convention, the pal's interrupt ports (int0, int1, ...) are wired
   up to the corresponding processor's level sensative external
   interrupt pin.  Eg: A two byte write to address 8 of 0x0102
   (big-endian) will result in processor 2's external interrupt pin
   being asserted.

   PROCESSOR COUNT (read): returns the total number of processors
   active in the current simulation.

   TTY INPUT FIFO (read): if the TTY input status register indicates a
   character is available by being nonzero, returns the next available
   character from the pal's tty input port.

   TTY OUTPUT FIFO (write): if the TTY output status register
   indicates the output fifo is not full by being nonzero, outputs the
   character written to the tty's output port.

   COUNDOWN (read/write): The countdown registers provide a
   non-repeating timed interrupt source.  Writing a 32 bit big-endian
   zero value to this register clears the countdown timer.  Writing a
   non-zero 32 bit big-endian value to this register sets the
   countdown timer to expire in VALUE ticks (ticks is target
   dependant).  Reading the countdown register returns the last value
   writen.

   COUNTDOWN VALUE (read): Reading this 32 bit big-endian register
   returns the number of ticks remaining until the countdown timer
   expires.

   TIMER (read/write): The timer registers provide a periodic timed
   interrupt source.  Writing a 32 bit big-endian zero value to this
   register clears the periodic timer.  Writing a 32 bit non-zero
   value to this register sets the periodic timer to triger every
   VALUE ticks (ticks is target dependant).  Reading the timer
   register returns the last value written.

   TIMER VALUE (read): Reading this 32 bit big-endian register returns
   the number of ticks until the next periodic interrupt.


   PROPERTIES
   

   reg = <address> <size> (required)

   Specify the address (within the parent bus) that this device is to
   be located.

   poll? = <boolean>

   If present and true, indicates that the device should poll its
   input.


   PORTS


   int[0..NR_PROCESSORS] (output)

   Driven as a result of a write to the interrupt-port /
   interrupt-level register pair.


   countdown

   Driven whenever the countdown counter reaches zero.


   timer

   Driven whenever the timer counter reaches zero.


   BUGS


   At present the common simulator framework does not support input
   polling.

   */


enum {
  hw_pal_reset_register = 0x0,
  hw_pal_cpu_nr_register = 0x4,
  hw_pal_int_register = 0x8,
  hw_pal_nr_cpu_register = 0xa,
  hw_pal_read_fifo = 0x10,
  hw_pal_read_status = 0x14,
  hw_pal_write_fifo = 0x18,
  hw_pal_write_status = 0x1a,
  hw_pal_countdown = 0x20,
  hw_pal_countdown_value = 0x24,
  hw_pal_timer = 0x28,
  hw_pal_timer_value = 0x2c,
  hw_pal_address_mask = 0x3f,
};


typedef struct _hw_pal_console_buffer {
  char buffer;
  int status;
} hw_pal_console_buffer;

typedef struct _hw_pal_counter {
  struct hw_event *handler;
  signed64 start;
  unsigned32 delta;
  int periodic_p;
} hw_pal_counter;


typedef struct _hw_pal_device {
  hw_pal_console_buffer input;
  hw_pal_console_buffer output;
  hw_pal_counter countdown;
  hw_pal_counter timer;
  struct hw *disk;
  do_hw_poll_read_method *reader;
} hw_pal_device;

enum {
  COUNTDOWN_PORT,
  TIMER_PORT,
  INT_PORT,
};

static const struct hw_port_descriptor hw_pal_ports[] = {
  { "countdown", COUNTDOWN_PORT, 0, output_port, },
  { "timer", TIMER_PORT, 0, output_port, },
  { "int", INT_PORT, MAX_NR_PROCESSORS, output_port, },
  { NULL }
};


/* countdown and simple timer */

static void
do_counter_event (struct hw *me,
		  void *data)
{
  hw_pal_counter *counter = (hw_pal_counter *) data;
  if (counter->periodic_p)
    {
      HW_TRACE ((me, "timer expired"));
      counter->start = hw_event_queue_time (me);
      hw_port_event (me, TIMER_PORT, 1);
      hw_event_queue_schedule (me, counter->delta, do_counter_event, counter);
    }
  else
    {
      HW_TRACE ((me, "countdown expired"));
      counter->delta = 0;
      hw_port_event (me, COUNTDOWN_PORT, 1);
    }
}

static void
do_counter_read (struct hw *me,
		 hw_pal_device *pal,
		 const char *reg,
		 hw_pal_counter *counter,
		 unsigned32 *word,
		 unsigned nr_bytes)
{
  unsigned32 val;
  if (nr_bytes != 4)
    hw_abort (me, "%s - bad read size must be 4 bytes", reg);
  val = counter->delta;
  HW_TRACE ((me, "read - %s %ld", reg, (long) val));
  *word = H2BE_4 (val);
}

static void
do_counter_value (struct hw *me,
		  hw_pal_device *pal,
		  const char *reg,
		  hw_pal_counter *counter,
		  unsigned32 *word,
		  unsigned nr_bytes)
{
  unsigned32 val;
  if (nr_bytes != 4)
    hw_abort (me, "%s - bad read size must be 4 bytes", reg);
  if (counter->delta != 0)
    val = (counter->start + counter->delta
	   - hw_event_queue_time (me));
  else
    val = 0;
  HW_TRACE ((me, "read - %s %ld", reg, (long) val));
  *word = H2BE_4 (val);
}

static void
do_counter_write (struct hw *me,
		  hw_pal_device *pal,
		  const char *reg,
		  hw_pal_counter *counter,
		  const unsigned32 *word,
		  unsigned nr_bytes)
{
  if (nr_bytes != 4)
    hw_abort (me, "%s - bad write size must be 4 bytes", reg);
  if (counter->handler != NULL)
    {
      hw_event_queue_deschedule (me, counter->handler);
      counter->handler = NULL;
    }
  counter->delta = BE2H_4 (*word);
  counter->start = hw_event_queue_time (me);
  HW_TRACE ((me, "write - %s %ld", reg, (long) counter->delta));
  if (counter->delta > 0)
    hw_event_queue_schedule (me, counter->delta, do_counter_event, counter);
}




/* check the console for an available character */
static void
scan_hw_pal (struct hw *me)
{
  hw_pal_device *hw_pal = (hw_pal_device *)hw_data (me);
  char c;
  int count;
  count = do_hw_poll_read (me, hw_pal->reader, 0/*STDIN*/, &c, sizeof(c));
  switch (count)
    {
    case HW_IO_NOT_READY:
    case HW_IO_EOF:
      hw_pal->input.buffer = 0;
      hw_pal->input.status = 0;
      break;
    default:
      hw_pal->input.buffer = c;
      hw_pal->input.status = 1;
    }
}

/* write the character to the hw_pal */

static void
write_hw_pal (struct hw *me,
	      char val)
{
  hw_pal_device *hw_pal = (hw_pal_device *) hw_data (me);
  sim_io_write_stdout (hw_system (me), &val, 1);
  hw_pal->output.buffer = val;
  hw_pal->output.status = 1;
}


/* Reads/writes */

static unsigned
hw_pal_io_read_buffer (struct hw *me,
		       void *dest,
		       int space,
		       unsigned_word addr,
		       unsigned nr_bytes)
{
  hw_pal_device *hw_pal = (hw_pal_device *) hw_data (me);
  unsigned_1 *byte = (unsigned_1 *) dest;
  memset (dest, 0, nr_bytes);
  switch (addr & hw_pal_address_mask)
    {

    case hw_pal_cpu_nr_register:
#ifdef CPU_INDEX
      *byte = CPU_INDEX (hw_system_cpu (me));
#else
      *byte = 0;
#endif
      HW_TRACE ((me, "read - cpu-nr %d\n", *byte));
      break;

    case hw_pal_nr_cpu_register:
      if (hw_tree_find_property (me, "/openprom/options/smp") == NULL)
	{
	  *byte = 1;
	  HW_TRACE ((me, "read - nr-cpu %d (not defined)\n", *byte));
	}
      else
	{
	  *byte = hw_tree_find_integer_property (me, "/openprom/options/smp");
	  HW_TRACE ((me, "read - nr-cpu %d\n", *byte));
	}
      break;

    case hw_pal_read_fifo:
      *byte = hw_pal->input.buffer;
      HW_TRACE ((me, "read - input-fifo %d\n", *byte));
      break;

    case hw_pal_read_status:
      scan_hw_pal (me);
      *byte = hw_pal->input.status;
      HW_TRACE ((me, "read - input-status %d\n", *byte));
      break;

    case hw_pal_write_fifo:
      *byte = hw_pal->output.buffer;
      HW_TRACE ((me, "read - output-fifo %d\n", *byte));
      break;

    case hw_pal_write_status:
      *byte = hw_pal->output.status;
      HW_TRACE ((me, "read - output-status %d\n", *byte));
      break;

    case hw_pal_countdown:
      do_counter_read (me, hw_pal, "countdown",
		       &hw_pal->countdown, dest, nr_bytes);
      break;

    case hw_pal_countdown_value:
      do_counter_value (me, hw_pal, "countdown-value",
			&hw_pal->countdown, dest, nr_bytes);
      break;

    case hw_pal_timer:
      do_counter_read (me, hw_pal, "timer",
		       &hw_pal->timer, dest, nr_bytes);
      break;

    case hw_pal_timer_value:
      do_counter_value (me, hw_pal, "timer-value",
			&hw_pal->timer, dest, nr_bytes);
      break;

    default:
      HW_TRACE ((me, "read - ???\n"));
      break;

    }
  return nr_bytes;
}


static unsigned
hw_pal_io_write_buffer (struct hw *me,
			const void *source,
			int space,
			unsigned_word addr,
			unsigned nr_bytes)
{
  hw_pal_device *hw_pal = (hw_pal_device*) hw_data (me);
  unsigned_1 *byte = (unsigned_1 *) source;
  
  switch (addr & hw_pal_address_mask)
    {

    case hw_pal_reset_register:
      hw_halt (me, sim_exited, byte[0]);
      break;

    case hw_pal_int_register:
      hw_port_event (me,
		     INT_PORT + byte[0], /*port*/
		     (nr_bytes > 1 ? byte[1] : 0)); /* val */
      break;

    case hw_pal_read_fifo:
      hw_pal->input.buffer = byte[0];
      HW_TRACE ((me, "write - input-fifo %d\n", byte[0]));
      break;

    case hw_pal_read_status:
      hw_pal->input.status = byte[0];
      HW_TRACE ((me, "write - input-status %d\n", byte[0]));
      break;

    case hw_pal_write_fifo:
      write_hw_pal (me, byte[0]);
      HW_TRACE ((me, "write - output-fifo %d\n", byte[0]));
      break;

    case hw_pal_write_status:
      hw_pal->output.status = byte[0];
      HW_TRACE ((me, "write - output-status %d\n", byte[0]));
      break;

    case hw_pal_countdown:
      do_counter_write (me, hw_pal, "countdown",
			&hw_pal->countdown, source, nr_bytes);
      break;
      
    case hw_pal_timer:
      do_counter_write (me, hw_pal, "timer",
			&hw_pal->timer, source, nr_bytes);
      break;
      
    }
  return nr_bytes;
}


/* instances of the hw_pal struct hw */

#if NOT_YET
static void
hw_pal_instance_delete_callback(hw_instance *instance)
{
  /* nothing to delete, the hw_pal is attached to the struct hw */
  return;
}
#endif

#if NOT_YET
static int
hw_pal_instance_read_callback (hw_instance *instance,
			      void *buf,
			      unsigned_word len)
{
  DITRACE (pal, ("read - %s (%ld)", (const char*) buf, (long int) len));
  return sim_io_read_stdin (buf, len);
}
#endif

#if NOT_YET
static int
hw_pal_instance_write_callback (hw_instance *instance,
				const void *buf,
				unsigned_word len)
{
  int i;
  const char *chp = buf;
  hw_pal_device *hw_pal = hw_instance_data (instance);
  DITRACE (pal, ("write - %s (%ld)", (const char*) buf, (long int) len));
  for (i = 0; i < len; i++)
    write_hw_pal (hw_pal, chp[i]);
  sim_io_flush_stdoutput ();
  return i;
}
#endif

#if NOT_YET
static const hw_instance_callbacks hw_pal_instance_callbacks = {
  hw_pal_instance_delete_callback,
  hw_pal_instance_read_callback,
  hw_pal_instance_write_callback,
};
#endif

#if 0
static hw_instance *
hw_pal_create_instance (struct hw *me,
			const char *path,
			const char *args)
{
  return hw_create_instance_from (me, NULL,
				      hw_data (me),
				      path, args,
				      &hw_pal_instance_callbacks);
}
#endif


static void
hw_pal_attach_address (struct hw *me,
		       int level,
		       int space,
		       address_word addr,
		       address_word nr_bytes,
		       struct hw *client)
{
  hw_pal_device *pal = (hw_pal_device*) hw_data (me);
  pal->disk = client;
}


#if 0
static hw_callbacks const hw_pal_callbacks = {
  { generic_hw_init_address, },
  { hw_pal_attach_address, }, /* address */
  { hw_pal_io_read_buffer_callback,
      hw_pal_io_write_buffer_callback, },
  { NULL, }, /* DMA */
  { NULL, NULL, hw_pal_interrupt_ports }, /* interrupt */
  { generic_hw_unit_decode,
    generic_hw_unit_encode,
    generic_hw_address_to_attach_address,
    generic_hw_size_to_attach_size },
  hw_pal_create_instance,
};
#endif


static void
hw_pal_finish (struct hw *hw)
{
  /* create the descriptor */
  hw_pal_device *hw_pal = HW_ZALLOC (hw, hw_pal_device);
  hw_pal->output.status = 1;
  hw_pal->output.buffer = '\0';
  hw_pal->input.status = 0;
  hw_pal->input.buffer = '\0';
  set_hw_data (hw, hw_pal);
  set_hw_attach_address (hw, hw_pal_attach_address);
  set_hw_io_read_buffer (hw, hw_pal_io_read_buffer);
  set_hw_io_write_buffer (hw, hw_pal_io_write_buffer);
  set_hw_ports (hw, hw_pal_ports);
  /* attach ourselves */
  do_hw_attach_regs (hw);
  /* If so configured, enable polled input */
  if (hw_find_property (hw, "poll?") != NULL
      && hw_find_boolean_property (hw, "poll?"))
    {
      hw_pal->reader = sim_io_poll_read;
    }
  else
    {
      hw_pal->reader = sim_io_read;
    }
  /* tag the periodic timer */
  hw_pal->timer.periodic_p = 1;
}


const struct hw_descriptor dv_pal_descriptor[] = {
  { "pal", hw_pal_finish, },
  { NULL },
};
