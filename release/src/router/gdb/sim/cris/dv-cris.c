/* The CRIS interrupt framework for GDB, the GNU Debugger.

   Copyright 2006, 2007 Free Software Foundation, Inc.

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

#include "sim-main.h"
#include "hw-main.h"

/* DEVICE

   CRIS cpu virtual device (very rudimental; generic enough for all
   currently used CRIS versions).


   DESCRIPTION

   Implements the external CRIS functionality.  This includes the
   delivery of interrupts generated from other devices.


   PROPERTIES

   vec-for-int = <int-a> <vec-a> <int-b> <vec-b> ...
   These are the translations to interrupt vector for values appearing
   on the "int" port, as pairs of the value and the corresponding
   vector.  Defaults to no translation.  All values that may appear on
   the "int" port must be defined, or the device aborts.

   multiple-int = ("abort" | "ignore_previous" | <vector>)
   If multiple interrupt values are dispatched, this property decides
   what to do.  The value is either a number corresponding to the
   vector to use, or the string "abort" to cause a hard abort, or the
   string "ignore_previous", to silently use the new vector instead.
   The default is "abort".


   PORTS

   int (input)
   Interrupt port.  An event with a non-zero value on this port causes
   an interrupt.  If, after an event but before the interrupt has been
   properly dispatched, a non-zero value appears that is different
   after mapping than the previous, then the property multiple_int
   decides what to do.

   FIXME: reg port so internal registers can be read.  Requires
   chip-specific versions, though.  Ports "nmi" and "reset".


   BUGS
   When delivering an interrupt, this code assumes that there is only
   one processor (number 0).

   This code does not attempt to be efficient at handling pending
   interrupts.  It simply schedules the interrupt delivery handler
   every instruction cycle until all pending interrupts go away.
   It also works around a bug in sim_events_process when doing so.
   */

/* Keep this an enum for simple addition of "reset" and "nmi".  */
enum
 {
   INT_PORT,
 };

static const struct hw_port_descriptor cris_ports[] =
 {
   { "int", INT_PORT, 0, input_port },
   { NULL, 0, 0, 0 }
 };

struct cris_vec_tr
 {
   unsigned32 portval, vec;
 };

enum cris_multiple_ints
  {
    cris_multint_abort,
    cris_multint_ignore_previous,
    cris_multint_vector
  };

struct cris_hw
 {
   struct hw_event *pending_handler;
   unsigned32 pending_vector;
   struct cris_vec_tr *int_to_vec;
   enum cris_multiple_ints multi_int_action;
   unsigned32 multiple_int_vector;
 };

/* An event function, calling the actual CPU-model-specific
   interrupt-delivery function.  */

static void
deliver_cris_interrupt (struct hw *me, void *data)
{
  struct cris_hw *crishw = hw_data (me);
  SIM_DESC simulator = hw_system (me);
  sim_cpu *cpu = STATE_CPU (simulator, 0);
  unsigned int intno = crishw->pending_vector;

 if (CPU_CRIS_DELIVER_INTERRUPT (cpu) (cpu, CRIS_INT_INT, intno))
    {
      crishw->pending_vector = 0;
      crishw->pending_handler = NULL;
      return;
    }

 {
   /* Bug workaround: at time T with a pending number of cycles N to
      process, if re-scheduling an event at time T+M, M < N,
      sim_events_process gets stuck at T (updating the "time" to
      before the event rather than after the event, or somesuch).

      Hacking this locally is thankfully easy: if we see the same
      simulation time, increase the number of cycles.  Do this every
      time we get here, until a new time is seen (supposedly unstuck
      re-delivery).  (Fixing in SIM/GDB source will hopefully then
      also be easier, having a tangible test-case.)  */
   static signed64 last_events_time = 0;
   static signed64 delta = 1;
   signed64 this_events_time = hw_event_queue_time (me);

   if (this_events_time == last_events_time)
     delta++;
   else
     {
       delta = 1;
       last_events_time = this_events_time;
     }

   crishw->pending_handler
     = hw_event_queue_schedule (me, delta, deliver_cris_interrupt, NULL);
 }
}


/* A port-event function for events arriving to an interrupt port.  */

static void
cris_port_event (struct hw *me,
		 int my_port,
		 struct hw *source,
		 int source_port,
		 int intparam)
{
  struct cris_hw *crishw = hw_data (me);
  unsigned32 vec;

  /* A few placeholders; only the INT port is implemented.  */
  switch (my_port)
    {
    case INT_PORT:
      HW_TRACE ((me, "INT value=0x%x", intparam));
      break;

    default:
      hw_abort (me, "bad switch");
      break;
    }

  if (intparam == 0)
    return;

  if (crishw->int_to_vec != NULL)
    {
      unsigned int i;
      for (i = 0; crishw->int_to_vec[i].portval != 0; i++)
	if (crishw->int_to_vec[i].portval == intparam)
	  break;

      if (crishw->int_to_vec[i].portval == 0)
	hw_abort (me, "unsupported value for int port: 0x%x", intparam);

      vec = crishw->int_to_vec[i].vec;
    }
  else
    vec = (unsigned32) intparam;

  if (crishw->pending_vector != 0)
    {
      if (vec == crishw->pending_vector)
	return;

      switch (crishw->multi_int_action)
	{
	case cris_multint_abort:
	  hw_abort (me, "int 0x%x (0x%x) while int 0x%x hasn't been delivered",
		    vec, intparam, crishw->pending_vector);
	  break;

	case cris_multint_ignore_previous:
	  break;

	case cris_multint_vector:
	  vec = crishw->multiple_int_vector;
	  break;

	default:
	  hw_abort (me, "bad switch");
	}
    }

  crishw->pending_vector = vec;

  /* Schedule our event handler *now*.  */
  if (crishw->pending_handler == NULL)
    crishw->pending_handler
      = hw_event_queue_schedule (me, 0, deliver_cris_interrupt, NULL);
}

/* Instance initializer function.  */

static void
cris_finish (struct hw *me)
{
  struct cris_hw *crishw;
  const struct hw_property *vec_for_int;
  const struct hw_property *multiple_int;

  crishw = HW_ZALLOC (me, struct cris_hw);
  set_hw_data (me, crishw);
  set_hw_ports (me, cris_ports);
  set_hw_port_event (me, cris_port_event);

  vec_for_int = hw_find_property (me, "vec-for-int");
  if (vec_for_int != NULL)
    {
      unsigned32 vecsize;
      unsigned32 i;

      if (hw_property_type (vec_for_int) != array_property)
	hw_abort (me, "property \"vec-for-int\" has the wrong type");

      vecsize = hw_property_sizeof_array (vec_for_int) / sizeof (signed_cell);

      if ((vecsize % 2) != 0)
	hw_abort (me, "translation vector does not consist of even pairs");

      crishw->int_to_vec
	= hw_malloc (me, (vecsize/2 + 1) * sizeof (crishw->int_to_vec[0]));

      for (i = 0; i < vecsize/2; i++)
	{
	  signed_cell portval_sc;
	  signed_cell vec_sc;

	  if (!hw_find_integer_array_property (me, "vec-for-int", i*2,
					       &portval_sc)
	      || !hw_find_integer_array_property (me, "vec-for-int", i*2 + 1,
						  &vec_sc)
	      || portval_sc < 0
	      || vec_sc < 0)
	    hw_abort (me, "no valid vector translation pair %u", i);

	  crishw->int_to_vec[i].portval = (unsigned32) portval_sc;
	  crishw->int_to_vec[i].vec = (unsigned32) vec_sc;
	}

      crishw->int_to_vec[i].portval = 0;
      crishw->int_to_vec[i].vec = 0;
    }

  multiple_int = hw_find_property (me, "multiple-int");
  if (multiple_int != NULL)
    {
      if (hw_property_type (multiple_int) == integer_property)
	{
	  crishw->multiple_int_vector
	    = hw_find_integer_property (me, "multiple-int");
	  crishw->multi_int_action = cris_multint_vector;
	}
      else
	{
	  const char *action = hw_find_string_property (me, "multiple-int");

	  if (action == NULL)
	    hw_abort (me, "property \"multiple-int\" has the wrong type");

	  if (strcmp (action, "abort") == 0)
	    crishw->multi_int_action = cris_multint_abort;
	  else if (strcmp (action, "ignore_previous") == 0)
	    crishw->multi_int_action = cris_multint_ignore_previous;
	  else
	    hw_abort (me, "property \"multiple-int\" must be one of <vector number>\n"
		      "\"abort\" and \"ignore_previous\", not \"%s\"", action);
	}
    }
  else
    crishw->multi_int_action = cris_multint_abort;
}

const struct hw_descriptor dv_cris_descriptor[] = {
  { "cris", cris_finish, },
  { NULL },
};
