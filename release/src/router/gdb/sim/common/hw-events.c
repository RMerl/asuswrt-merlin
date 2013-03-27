/* Hardware event manager.
   Copyright (C) 1998, 2007 Free Software Foundation, Inc.
   Contributed by Cygnus Support.

This file is part of GDB, the GNU debugger.

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
#include "hw-base.h"

#include "sim-events.h"


/* The hw-events object is implemented using sim-events */

struct hw_event {
  void *data;
  struct hw *me;
  hw_event_callback *callback;
  sim_event *real;
  struct hw_event_data *entry;
};

struct hw_event_data {
  struct hw_event event;
  struct hw_event_data *next;
};

void
create_hw_event_data (struct hw *me)
{
  if (me->events_of_hw != NULL)
    hw_abort (me, "stray events");
  /* NOP */
}

void
delete_hw_event_data (struct hw *me)
{
  /* Remove the scheduled event.  */
  while (me->events_of_hw)
    hw_event_queue_deschedule (me, &me->events_of_hw->event);
}


/* Pass the H/W event onto the real callback */

static void
bounce_hw_event (SIM_DESC sd,
		 void *data)
{
  /* save the data */
  struct hw_event_data *entry = (struct hw_event_data *) data;
  struct hw *me = entry->event.me;
  void *event_data = entry->event.data;
  hw_event_callback *callback = entry->event.callback;
  struct hw_event_data **prev = &me->events_of_hw;
  while ((*prev) != entry)
    prev = &(*prev)->next;
  (*prev) = entry->next;
  hw_free (me, entry);
  callback (me, event_data); /* may not return */
}



/* Map onto the event functions */

struct hw_event *
hw_event_queue_schedule (struct hw *me,
			 signed64 delta_time,
			 hw_event_callback *callback,
			 void *data)
{
  struct hw_event *event;
  va_list dummy;
  memset (&dummy, 0, sizeof dummy);
  event = hw_event_queue_schedule_vtracef (me, delta_time, callback, data,
					   NULL, dummy);
  return event;
}

struct hw_event *
hw_event_queue_schedule_tracef (struct hw *me,
				signed64 delta_time,
				hw_event_callback *callback,
				void *data,
				const char *fmt,
				...)
{
  struct hw_event *event;
  va_list ap;
  va_start (ap, fmt);
  event = hw_event_queue_schedule_vtracef (me, delta_time, callback, data, fmt, ap);
  va_end (ap);
  return event;
}

struct hw_event *
hw_event_queue_schedule_vtracef (struct hw *me,
				 signed64 delta_time,
				 hw_event_callback *callback,
				 void *data,
				 const char *fmt,
				 va_list ap)
{
  struct hw_event_data *entry = HW_ZALLOC (me, struct hw_event_data);
  entry->next = me->events_of_hw;
  me->events_of_hw = entry;
  /* fill it in */
  entry->event.entry = entry;
  entry->event.data = data;
  entry->event.callback = callback;
  entry->event.me = me;
  entry->event.real = sim_events_schedule_vtracef (hw_system (me),
						   delta_time,
						   bounce_hw_event,
						   entry,
						   fmt, ap);
  return &entry->event;
}


void
hw_event_queue_deschedule (struct hw *me,
			   struct hw_event *event_to_remove)
{
/* ZAP the event but only if it is still in the event queue.  Note
   that event_to_remove is only de-referenced after its validity has
   been confirmed.  */
  struct hw_event_data **prev;
  for (prev = &me->events_of_hw;
       (*prev) != NULL;
       prev = &(*prev)->next)
    {
      struct hw_event_data *entry = (*prev);
      if (&entry->event == event_to_remove)
	{
	  sim_events_deschedule (hw_system (me),
				 entry->event.real);
	  (*prev) = entry->next;
	  hw_free (me, entry);
	  return;
	}
    }
}


signed64
hw_event_queue_time (struct hw *me)
{
  return sim_events_time (hw_system (me));
}

/* Returns the time that remains before the event is raised. */
signed64
hw_event_remain_time (struct hw *me, struct hw_event *event)
{
  signed64 t;

  t = sim_events_remain_time (hw_system (me), event->real);
  return t;
}

/* Only worry about this compling on ANSI systems.
   Build with `make test-hw-events' in sim/<cpu> directory*/

#if defined (MAIN)
#include "sim-main.h"
#include <string.h>
#include <stdio.h>

static void
test_handler (struct hw *me,
	      void *data)
{
  int *n = data;
  if (*n != hw_event_queue_time (me))
    abort ();
  *n = -(*n);
}

int
main (int argc,
      char **argv)
{
  host_callback *cb = ZALLOC (host_callback);
  struct sim_state *sd = sim_state_alloc (0, cb);
  struct hw *me = ZALLOC (struct hw);
  sim_pre_argv_init (sd, "test-hw-events");
  sim_post_argv_init (sd);
  me->system_of_hw = sd;

  printf ("Create hw-event-data\n");
  {
    create_hw_alloc_data (me);
    create_hw_event_data (me);
    delete_hw_event_data (me);
    delete_hw_alloc_data (me);
  }

  printf ("Create hw-events\n");
  {
    struct hw_event *a;
    struct hw_event *b;
    struct hw_event *c;
    struct hw_event *d;
    create_hw_alloc_data (me);
    create_hw_event_data (me);
    a = hw_event_queue_schedule (me, 0, NULL, NULL);
    b = hw_event_queue_schedule (me, 1, NULL, NULL);
    c = hw_event_queue_schedule (me, 2, NULL, NULL);
    d = hw_event_queue_schedule (me, 1, NULL, NULL);
    hw_event_queue_deschedule (me, c);
    hw_event_queue_deschedule (me, b);
    hw_event_queue_deschedule (me, a);
    hw_event_queue_deschedule (me, d);
    c = HW_ZALLOC (me, struct hw_event);
    hw_event_queue_deschedule (me, b); /* OOPS! */
    hw_free (me, c);
    delete_hw_event_data (me);
    delete_hw_alloc_data (me);
  }

  printf ("Schedule hw-events\n");
  {
    struct hw_event **e;
    int *n;
    int i;
    int nr = 4;
    e = HW_NZALLOC (me, struct hw_event *, nr);
    n = HW_NZALLOC (me, int, nr);
    create_hw_alloc_data (me);
    create_hw_event_data (me);
    for (i = 0; i < nr; i++)
      {
	n[i] = i;
	e[i] = hw_event_queue_schedule (me, i, test_handler, &n[i]);
      }
    sim_events_preprocess (sd, 1, 1);
    for (i = 0; i < nr; i++)
      {
	if (sim_events_tick (sd))
	  sim_events_process (sd);
      }
    for (i = 0; i < nr; i++)
      {
	if (n[i] != -i)
	  abort ();
	hw_event_queue_deschedule (me, e[i]);
      }
    hw_free (me, n);
    hw_free (me, e);
    delete_hw_event_data (me);
    delete_hw_alloc_data (me);
  }

  return 0;
}
#endif
