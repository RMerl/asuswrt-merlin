/*  This file is part of the program psim.

    Copyright (C) 1994-1998, Andrew Cagney <cagney@highland.com.au>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
 
    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 
    */


#ifndef _EVENTS_C_
#define _EVENTS_C_

#include "basics.h"
#include "events.h"

#include <signal.h>

#if !defined (SIM_EVENTS_POLL_RATE)
#define SIM_EVENTS_POLL_RATE 0x1000
#endif



/* The event queue maintains a single absolute time using two
   variables.
   
   TIME_OF_EVENT: this holds the time at which the next event is ment
   to occure.  If no next event it will hold the time of the last
   event.

   TIME_FROM_EVENT: The current distance from TIME_OF_EVENT.  If an
   event is pending, this will be positive.  If no future event is
   pending this will be negative.  This variable is decremented once
   for each iteration of a clock cycle.

   Initially, the clock is started at time one (1) with TIME_OF_EVENT
   == 0 and TIME_FROM_EVENT == -1.

   Clearly there is a bug in that this code assumes that the absolute
   time counter will never become greater than 2^62. */

typedef struct _event_entry event_entry;
struct _event_entry {
  void *data;
  event_handler *handler;
  signed64 time_of_event;  
  event_entry *next;
};

struct _event_queue {
  int processing;
  event_entry *queue;
  event_entry *volatile held;
  event_entry *volatile *volatile held_end;
  signed64 time_of_event;
  signed64 time_from_event;
};


STATIC_INLINE_EVENTS\
(void)
sim_events_poll (void *data)
{
  event_queue *queue = data;
  /* just re-schedule in 1000 million ticks time */
  event_queue_schedule (queue, SIM_EVENTS_POLL_RATE, sim_events_poll, queue);
  sim_io_poll_quit ();
}


INLINE_EVENTS\
(event_queue *)
event_queue_create(void)
{
  event_queue *new_event_queue = ZALLOC(event_queue);

  new_event_queue->processing = 0;
  new_event_queue->queue = NULL;
  new_event_queue->held = NULL;
  new_event_queue->held_end = &new_event_queue->held;

  /* both times are already zero */
  return new_event_queue;
}


INLINE_EVENTS\
(void)
event_queue_init(event_queue *queue)
{
  event_entry *event;

  /* drain the interrupt queue */
  {
#if defined(HAVE_SIGPROCMASK) && defined(SIG_SETMASK)
    sigset_t old_mask;
    sigset_t new_mask;
    sigfillset(&new_mask);
    /*-LOCK-*/ sigprocmask(SIG_SETMASK, &new_mask, &old_mask);
#endif
    event = queue->held;
    while (event != NULL) {
      event_entry *dead = event;
      event = event->next;
      zfree(dead);
    }
    queue->held = NULL;
    queue->held_end = &queue->held;
#if defined(HAVE_SIGPROCMASK) && defined(SIG_SETMASK)
    /*-UNLOCK-*/ sigprocmask(SIG_SETMASK, &old_mask, NULL);
#endif
  }

  /* drain the normal queue */
  event = queue->queue;
  while (event != NULL) {
    event_entry *dead = event;
    event = event->next;
    zfree(dead);
  }
  queue->queue = NULL;
    
  /* wind time back to one */
  queue->processing = 0;
  queue->time_of_event = 0;
  queue->time_from_event = -1;

  /* schedule our initial counter event */
  event_queue_schedule (queue, 0, sim_events_poll, queue);
}

INLINE_EVENTS\
(signed64)
event_queue_time(event_queue *queue)
{
  return queue->time_of_event - queue->time_from_event;
}

STATIC_INLINE_EVENTS\
(void)
update_time_from_event(event_queue *events)
{
  signed64 current_time = event_queue_time(events);
  if (events->queue != NULL) {
    events->time_from_event = (events->queue->time_of_event - current_time);
    events->time_of_event = events->queue->time_of_event;
  }
  else {
    events->time_of_event = current_time - 1;
    events->time_from_event = -1;
  }
  if (WITH_TRACE && ppc_trace[trace_events])
    {
      event_entry *event;
      int i;
      for (event = events->queue, i = 0;
	   event != NULL;
	   event = event->next, i++)
	{
	  TRACE(trace_events, ("event time-from-event - time %ld, delta %ld - event %d, tag 0x%lx, time %ld, handler 0x%lx, data 0x%lx\n",
			       (long)current_time,
			       (long)events->time_from_event,
			       i,
			       (long)event,
			       (long)event->time_of_event,
			       (long)event->handler,
			       (long)event->data));
	}
    }
  ASSERT(current_time == event_queue_time(events));
}

STATIC_INLINE_EVENTS\
(void)
insert_event_entry(event_queue *events,
		   event_entry *new_event,
		   signed64 delta)
{
  event_entry *curr;
  event_entry **prev;
  signed64 time_of_event;

  if (delta < 0)
    error("what is past is past!\n");

  /* compute when the event should occure */
  time_of_event = event_queue_time(events) + delta;

  /* find the queue insertion point - things are time ordered */
  prev = &events->queue;
  curr = events->queue;
  while (curr != NULL && time_of_event >= curr->time_of_event) {
    ASSERT(curr->next == NULL
	   || curr->time_of_event <= curr->next->time_of_event);
    prev = &curr->next;
    curr = curr->next;
  }
  ASSERT(curr == NULL || time_of_event < curr->time_of_event);

  /* insert it */
  new_event->next = curr;
  *prev = new_event;
  new_event->time_of_event = time_of_event;

  /* adjust the time until the first event */
  update_time_from_event(events);
}

INLINE_EVENTS\
(event_entry_tag)
event_queue_schedule(event_queue *events,
		     signed64 delta_time,
		     event_handler *handler,
		     void *data)
{
  event_entry *new_event = ZALLOC(event_entry);
  new_event->data = data;
  new_event->handler = handler;
  insert_event_entry(events, new_event, delta_time);
  TRACE(trace_events, ("event scheduled at %ld - tag 0x%lx - time %ld, handler 0x%lx, data 0x%lx\n",
		       (long)event_queue_time(events),
		       (long)new_event,
		       (long)new_event->time_of_event,
		       (long)new_event->handler,
		       (long)new_event->data));
  return (event_entry_tag)new_event;
}


INLINE_EVENTS\
(event_entry_tag)
event_queue_schedule_after_signal(event_queue *events,
				  signed64 delta_time,
				  event_handler *handler,
				  void *data)
{
  event_entry *new_event = ZALLOC(event_entry);

  new_event->data = data;
  new_event->handler = handler;
  new_event->time_of_event = delta_time; /* work it out later */
  new_event->next = NULL;

  {
#if defined(HAVE_SIGPROCMASK) && defined(SIG_SETMASK)
    sigset_t old_mask;
    sigset_t new_mask;
    sigfillset(&new_mask);
    /*-LOCK-*/ sigprocmask(SIG_SETMASK, &new_mask, &old_mask);
#endif
    if (events->held == NULL) {
      events->held = new_event;
    }
    else {
      *events->held_end = new_event;
    }
    events->held_end = &new_event->next;
#if defined(HAVE_SIGPROCMASK) && defined(SIG_SETMASK)
    /*-UNLOCK-*/ sigprocmask(SIG_SETMASK, &old_mask, NULL);
#endif
  }

  TRACE(trace_events, ("event scheduled at %ld - tag 0x%lx - time %ld, handler 0x%lx, data 0x%lx\n",
		       (long)event_queue_time(events),
		       (long)new_event,
		       (long)new_event->time_of_event,
		       (long)new_event->handler,
		       (long)new_event->data));

  return (event_entry_tag)new_event;
}


INLINE_EVENTS\
(void)
event_queue_deschedule(event_queue *events,
		       event_entry_tag event_to_remove)
{
  event_entry *to_remove = (event_entry*)event_to_remove;
  ASSERT((events->time_from_event >= 0) == (events->queue != NULL));
  if (event_to_remove != NULL) {
    event_entry *current;
    event_entry **ptr_to_current;
    for (ptr_to_current = &events->queue, current = *ptr_to_current;
	 current != NULL && current != to_remove;
	 ptr_to_current = &current->next, current = *ptr_to_current);
    if (current == to_remove) {
      *ptr_to_current = current->next;
      TRACE(trace_events, ("event descheduled at %ld - tag 0x%lx - time %ld, handler 0x%lx, data 0x%lx\n",
			   (long)event_queue_time(events),
			   (long)event_to_remove,
			   (long)current->time_of_event,
			   (long)current->handler,
			   (long)current->data));
      zfree(current);
      update_time_from_event(events);
    }
    else {
      TRACE(trace_events, ("event descheduled at %ld - tag 0x%lx - not found\n",
			   (long)event_queue_time(events),
			   (long)event_to_remove));
    }
  }
  ASSERT((events->time_from_event >= 0) == (events->queue != NULL));
}




INLINE_EVENTS\
(int)
event_queue_tick(event_queue *events)
{
  signed64 time_from_event;

  /* we should only be here when the previous tick has been fully processed */
  ASSERT(!events->processing);

  /* move any events that were queued by any signal handlers onto the
     real event queue.  BTW: When inlining, having this code here,
     instead of in event_queue_process() causes GCC to put greater
     weight on keeping the pointer EVENTS in a register.  This, in
     turn results in better code being output. */
  if (events->held != NULL) {
    event_entry *held_events;
    event_entry *curr_event;

    {
#if defined(HAVE_SIGPROCMASK) && defined(SIG_SETMASK)
      sigset_t old_mask;
      sigset_t new_mask;
      sigfillset(&new_mask);
      /*-LOCK-*/ sigprocmask(SIG_SETMASK, &new_mask, &old_mask);
#endif
      held_events = events->held;
      events->held = NULL;
      events->held_end = &events->held;
#if defined(HAVE_SIGPROCMASK) && defined(SIG_SETMASK)
      /*-UNLOCK-*/ sigprocmask(SIG_SETMASK, &old_mask, NULL);
#endif
    }

    do {
      curr_event = held_events;
      held_events = curr_event->next;
      insert_event_entry(events, curr_event, curr_event->time_of_event);
    } while (held_events != NULL);
  }

  /* advance time, checking to see if we've reached time zero which
     would indicate the time for the next event has arrived */
  time_from_event = events->time_from_event;
  events->time_from_event = time_from_event - 1;
  return time_from_event == 0;
}



INLINE_EVENTS\
(void)
event_queue_process(event_queue *events)
{
  signed64 event_time = event_queue_time(events);

  ASSERT((events->time_from_event == -1 && events->queue != NULL)
	 || events->processing); /* something to do */

  /* consume all events for this or earlier times.  Be careful to
     allow a new event to appear under our feet */
  events->processing = 1;
  while (events->queue != NULL
	 && events->queue->time_of_event <= event_time) {
    event_entry *to_do = events->queue;
    event_handler *handler = to_do->handler;
    void *data = to_do->data;
    events->queue = to_do->next;
    TRACE(trace_events, ("event issued at %ld - tag 0x%lx - time %ld, handler 0x%lx, data 0x%lx\n",
			 (long)event_time,
			 (long)to_do,
			 (long)to_do->time_of_event,
			 (long)handler,
			 (long)data));
    zfree(to_do);
    /* Always re-compute the time to the next event so that HANDLER()
       can safely insert new events into the queue. */
    update_time_from_event(events);
    handler(data);
  }
  events->processing = 0;

  ASSERT(events->time_from_event > 0);
  ASSERT(events->queue != NULL); /* always poll event */
}


#endif /* _EVENTS_C_ */
