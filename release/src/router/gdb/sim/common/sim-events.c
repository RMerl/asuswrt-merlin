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


#ifndef _SIM_EVENTS_C_
#define _SIM_EVENTS_C_

#include "sim-main.h"
#include "sim-assert.h"

#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <signal.h> /* For SIGPROCMASK et al. */

typedef enum {
  watch_invalid,

  /* core - target byte order */
  watch_core_targ_1,
  watch_core_targ_2,
  watch_core_targ_4,
  watch_core_targ_8,
  /* core - big-endian */
  watch_core_be_1,
  watch_core_be_2,
  watch_core_be_4,
  watch_core_be_8,
  /* core - little-endian */
  watch_core_le_1,
  watch_core_le_2,
  watch_core_le_4,
  watch_core_le_8,

  /* sim - host byte order */
  watch_sim_host_1,
  watch_sim_host_2,
  watch_sim_host_4,
  watch_sim_host_8,
  /* sim - big-endian */
  watch_sim_be_1,
  watch_sim_be_2,
  watch_sim_be_4,
  watch_sim_be_8,
  /* sim - little-endian */
  watch_sim_le_1,
  watch_sim_le_2,
  watch_sim_le_4,
  watch_sim_le_8,
  
  /* wallclock */
  watch_clock,

  /* timer */
  watch_timer,
} sim_event_watchpoints;


struct _sim_event {
  sim_event_watchpoints watching;
  void *data;
  sim_event_handler *handler;
  /* timer event */
  signed64 time_of_event;
  /* watch wallclock event */
  unsigned wallclock;
  /* watch core address */
  address_word core_addr;
  unsigned core_map;
  /* watch sim addr */
  void *host_addr;
  /* watch core/sim range */
  int is_within; /* 0/1 */
  unsigned ub;
  unsigned lb;
  unsigned64 ub64;
  unsigned64 lb64;
  /* trace info (if any) */
  char *trace;
  /* list */
  sim_event *next;
};


/* The event queue maintains a single absolute time using two
   variables.
   
   TIME_OF_EVENT: this holds the time at which the next event is ment
   to occur.  If no next event it will hold the time of the last
   event.

   TIME_FROM_EVENT: The current distance from TIME_OF_EVENT.  A value
   <= 0 (except when poll-event is being processed) indicates that
   event processing is due.  This variable is decremented once for
   each iteration of a clock cycle.

   Initially, the clock is started at time one (0) with TIME_OF_EVENT
   == 0 and TIME_FROM_EVENT == 0 and with NR_TICKS_TO_PROCESS == 1.

   Clearly there is a bug in that this code assumes that the absolute
   time counter will never become greater than 2^62.

   To avoid the need to use 64bit arithmetic, the event queue always
   contains at least one event scheduled every 16 000 ticks.  This
   limits the time from event counter to values less than
   16 000. */


#if !defined (SIM_EVENTS_POLL_RATE)
#define SIM_EVENTS_POLL_RATE 0x1000
#endif


#define _ETRACE sd, NULL

#undef ETRACE_P
#define ETRACE_P (WITH_TRACE && STATE_EVENTS (sd)->trace)

#undef ETRACE
#define ETRACE(ARGS) \
do \
  { \
    if (ETRACE_P) \
      { \
        if (STRACE_DEBUG_P (sd)) \
	  { \
	    const char *file; \
	    SIM_FILTER_PATH (file, __FILE__); \
	    trace_printf (sd, NULL, "%s:%d: ", file, __LINE__); \
	  } \
        trace_printf  ARGS; \
      } \
  } \
while (0)


/* event queue iterator - don't iterate over the held queue. */

#if EXTERN_SIM_EVENTS_P
static sim_event **
next_event_queue (SIM_DESC sd,
		  sim_event **queue)
{
  if (queue == NULL)
    return &STATE_EVENTS (sd)->queue;
  else if (queue == &STATE_EVENTS (sd)->queue)
    return &STATE_EVENTS (sd)->watchpoints;
  else if (queue == &STATE_EVENTS (sd)->watchpoints)
    return &STATE_EVENTS (sd)->watchedpoints;
  else if (queue == &STATE_EVENTS (sd)->watchedpoints)
    return NULL;
  else
    sim_io_error (sd, "next_event_queue - bad queue");
  return NULL;
}
#endif


STATIC_INLINE_SIM_EVENTS\
(void)
sim_events_poll (SIM_DESC sd,
		 void *data)
{
  /* just re-schedule in 1000 million ticks time */
  sim_events_schedule (sd, SIM_EVENTS_POLL_RATE, sim_events_poll, sd);
  sim_io_poll_quit (sd);
}


/* "events" module install handler.
   This is called via sim_module_install to install the "events" subsystem
   into the simulator.  */

#if EXTERN_SIM_EVENTS_P
STATIC_SIM_EVENTS (MODULE_UNINSTALL_FN) sim_events_uninstall;
STATIC_SIM_EVENTS (MODULE_INIT_FN) sim_events_init;
STATIC_SIM_EVENTS (MODULE_RESUME_FN) sim_events_resume;
STATIC_SIM_EVENTS (MODULE_SUSPEND_FN) sim_events_suspend;
#endif

#if EXTERN_SIM_EVENTS_P
SIM_RC
sim_events_install (SIM_DESC sd)
{
  SIM_ASSERT (STATE_MAGIC (sd) == SIM_MAGIC_NUMBER);
  sim_module_add_uninstall_fn (sd, sim_events_uninstall);
  sim_module_add_init_fn (sd, sim_events_init);
  sim_module_add_resume_fn (sd, sim_events_resume);
  sim_module_add_suspend_fn (sd, sim_events_suspend);
  return SIM_RC_OK;
}
#endif


/* Suspend/resume the event queue manager when the simulator is not
   running */

#if EXTERN_SIM_EVENTS_P
static SIM_RC
sim_events_resume (SIM_DESC sd)
{
  sim_events *events = STATE_EVENTS (sd);
  SIM_ASSERT (STATE_MAGIC (sd) == SIM_MAGIC_NUMBER);
  SIM_ASSERT (events->resume_wallclock == 0);
  events->resume_wallclock = sim_elapsed_time_get ();
  return SIM_RC_OK;
}
#endif

#if EXTERN_SIM_EVENTS_P
static SIM_RC
sim_events_suspend (SIM_DESC sd)
{
  sim_events *events = STATE_EVENTS (sd);
  SIM_ASSERT (STATE_MAGIC (sd) == SIM_MAGIC_NUMBER);
  SIM_ASSERT (events->resume_wallclock != 0);
  events->elapsed_wallclock += sim_elapsed_time_since (events->resume_wallclock);
  events->resume_wallclock = 0;
  return SIM_RC_OK;
}
#endif


/* Uninstall the "events" subsystem from the simulator.  */

#if EXTERN_SIM_EVENTS_P
static void
sim_events_uninstall (SIM_DESC sd)
{
  SIM_ASSERT (STATE_MAGIC (sd) == SIM_MAGIC_NUMBER);
  /* FIXME: free buffers, etc. */
}
#endif


/* malloc/free */

#if EXTERN_SIM_EVENTS_P
static sim_event *
sim_events_zalloc (SIM_DESC sd)
{
  sim_events *events = STATE_EVENTS (sd);
  sim_event *new = events->free_list;
  if (new != NULL)
    {
      events->free_list = new->next;
      memset (new, 0, sizeof (*new));
    }
  else
    {
#if defined (HAVE_SIGPROCMASK) && defined (SIG_SETMASK)
      /*-LOCK-*/
      sigset_t old_mask;
      sigset_t new_mask;
      sigfillset(&new_mask);
      sigprocmask (SIG_SETMASK, &new_mask, &old_mask);
#endif
      new = ZALLOC (sim_event);
#if defined (HAVE_SIGPROCMASK) && defined (SIG_SETMASK)
      /*-UNLOCK-*/
      sigprocmask (SIG_SETMASK, &old_mask, NULL);
#endif
    }
  return new;
}
#endif

STATIC_INLINE_SIM_EVENTS\
(void)
sim_events_free (SIM_DESC sd,
		 sim_event *dead)
{
  sim_events *events = STATE_EVENTS (sd);
  dead->next = events->free_list;
  events->free_list = dead;
  if (dead->trace != NULL)
    {
      free (dead->trace); /* NB: asprintf returns a `free' buf */
      dead->trace = NULL;
    }
}


/* Initialize the simulator event manager */

#if EXTERN_SIM_EVENTS_P
SIM_RC
sim_events_init (SIM_DESC sd)
{
  sim_events *events = STATE_EVENTS (sd);

  /* drain the interrupt queue */
  events->nr_held = 0;
  if (events->held == NULL)
    events->held = NZALLOC (sim_event, MAX_NR_SIGNAL_SIM_EVENTS);

  /* drain the normal queues */
  {
    sim_event **queue = NULL;
    while ((queue = next_event_queue (sd, queue)) != NULL)
      {
	if (queue == NULL) break;
	while (*queue != NULL)
	  {
	    sim_event *dead = *queue;
	    *queue = dead->next;
	    sim_events_free (sd, dead);
	  }
	*queue = NULL;
      }
  }

  /* wind time back to zero */
  events->nr_ticks_to_process = 1; /* start by doing queue */
  events->time_of_event = 0;
  events->time_from_event = 0;
  events->elapsed_wallclock = 0;
  events->resume_wallclock = 0;

  /* schedule our initial counter event */
  sim_events_schedule (sd, 0, sim_events_poll, sd);

  /* from now on, except when the large-int event is being processed
     the event queue is non empty */
  SIM_ASSERT (events->queue != NULL);

  return SIM_RC_OK;
}
#endif


INLINE_SIM_EVENTS\
(signed64)
sim_events_time (SIM_DESC sd)
{
  sim_events *events = STATE_EVENTS (sd);
  return (events->time_of_event - events->time_from_event);
}


INLINE_SIM_EVENTS\
(unsigned long)
sim_events_elapsed_time (SIM_DESC sd)
{
  unsigned long elapsed = STATE_EVENTS (sd)->elapsed_wallclock;

  /* Are we being called inside sim_resume?
     (Is there a simulation in progress?)  */
  if (STATE_EVENTS (sd)->resume_wallclock != 0)
     elapsed += sim_elapsed_time_since (STATE_EVENTS (sd)->resume_wallclock);

  return elapsed;
}


/* Returns the time that remains before the event is raised. */
INLINE_SIM_EVENTS\
(signed64)
sim_events_remain_time (SIM_DESC sd, sim_event *event)
{
  if (event == 0)
    return 0;
  
  return (event->time_of_event - sim_events_time (sd));
}



STATIC_INLINE_SIM_EVENTS\
(void)
update_time_from_event (SIM_DESC sd)
{
  sim_events *events = STATE_EVENTS (sd);
  signed64 current_time = sim_events_time (sd);
  if (events->queue != NULL)
    {
      events->time_of_event = events->queue->time_of_event;
      events->time_from_event = (events->queue->time_of_event - current_time);
    }
  else
    {
      events->time_of_event = current_time - 1;
      events->time_from_event = -1;
    }
  if (ETRACE_P)
    {
      sim_event *event;
      int i;
      for (event = events->queue, i = 0;
	   event != NULL;
	   event = event->next, i++)
	{
	  ETRACE ((_ETRACE,
		   "event time-from-event - time %ld, delta %ld - event %d, tag 0x%lx, time %ld, handler 0x%lx, data 0x%lx%s%s\n",
		   (long)current_time,
		   (long)events->time_from_event,
		   i,
		   (long)event,
		   (long)event->time_of_event,
		   (long)event->handler,
		   (long)event->data,
		   (event->trace != NULL) ? ", " : "",
		   (event->trace != NULL) ? event->trace : ""));
	}
    }
  SIM_ASSERT (current_time == sim_events_time (sd));
}


#if EXTERN_SIM_EVENTS_P
static void
insert_sim_event (SIM_DESC sd,
		  sim_event *new_event,
		  signed64 delta)
{
  sim_events *events = STATE_EVENTS (sd);
  sim_event *curr;
  sim_event **prev;
  signed64 time_of_event;

  if (delta < 0)
    sim_io_error (sd, "what is past is past!\n");
  
  /* compute when the event should occur */
  time_of_event = sim_events_time (sd) + delta;
  
  /* find the queue insertion point - things are time ordered */
  prev = &events->queue;
  curr = events->queue;
  while (curr != NULL && time_of_event >= curr->time_of_event)
    {
      SIM_ASSERT (curr->next == NULL
		  || curr->time_of_event <= curr->next->time_of_event);
      prev = &curr->next;
      curr = curr->next;
    }
  SIM_ASSERT (curr == NULL || time_of_event < curr->time_of_event);
  
  /* insert it */
  new_event->next = curr;
  *prev = new_event;
  new_event->time_of_event = time_of_event;
  
  /* adjust the time until the first event */
  update_time_from_event (sd);
}
#endif


#if EXTERN_SIM_EVENTS_P
sim_event *
sim_events_schedule (SIM_DESC sd,
		     signed64 delta_time,
		     sim_event_handler *handler,
		     void *data)
{
  va_list dummy;
  memset (&dummy, 0, sizeof dummy);
  return sim_events_schedule_vtracef (sd, delta_time, handler, data,
				      NULL, dummy);
}
#endif


#if EXTERN_SIM_EVENTS_P
sim_event *
sim_events_schedule_tracef (SIM_DESC sd,
			    signed64 delta_time,
			    sim_event_handler *handler,
			    void *data,
			    const char *fmt,
			    ...)
{
  sim_event *new_event;
  va_list ap;
  va_start (ap, fmt);
  new_event = sim_events_schedule_vtracef (sd, delta_time, handler, data, fmt, ap);
  va_end (ap);
  return new_event;
}
#endif


#if EXTERN_SIM_EVENTS_P
sim_event *
sim_events_schedule_vtracef (SIM_DESC sd,
			     signed64 delta_time,
			     sim_event_handler *handler,
			     void *data,
			     const char *fmt,
			     va_list ap)
{
  sim_event *new_event = sim_events_zalloc (sd);
  new_event->data = data;
  new_event->handler = handler;
  new_event->watching = watch_timer;
  if (fmt == NULL || !ETRACE_P || vasprintf (&new_event->trace, fmt, ap) < 0)
    new_event->trace = NULL;
  insert_sim_event(sd, new_event, delta_time);
  ETRACE((_ETRACE,
	  "event scheduled at %ld - tag 0x%lx - time %ld, handler 0x%lx, data 0x%lx%s%s\n",
	  (long)sim_events_time(sd),
	  (long)new_event,
	  (long)new_event->time_of_event,
	  (long)new_event->handler,
	  (long)new_event->data,
	  (new_event->trace != NULL) ? ", " : "",
	  (new_event->trace != NULL) ? new_event->trace : ""));
  return new_event;
}
#endif


#if EXTERN_SIM_EVENTS_P
void
sim_events_schedule_after_signal (SIM_DESC sd,
				  signed64 delta_time,
				  sim_event_handler *handler,
				  void *data)
{
  sim_events *events = STATE_EVENTS (sd);
  sim_event *new_event;
#if defined (HAVE_SIGPROCMASK) && defined (SIG_SETMASK)
  /*-LOCK-*/
  sigset_t old_mask;
  sigset_t new_mask;
  sigfillset(&new_mask);
  sigprocmask (SIG_SETMASK, &new_mask, &old_mask);
#endif
  
  /* allocate an event entry from the signal buffer */
  new_event = &events->held [events->nr_held];
  events->nr_held ++;
  if (events->nr_held > MAX_NR_SIGNAL_SIM_EVENTS)
    {
      sim_engine_abort (NULL, NULL, NULL_CIA,
			"sim_events_schedule_after_signal - buffer oveflow");
    }
  
  new_event->data = data;
  new_event->handler = handler;
  new_event->time_of_event = delta_time; /* work it out later */
  new_event->next = NULL;

  events->work_pending = 1; /* notify main process */

#if defined (HAVE_SIGPROCMASK) && defined (SIG_SETMASK)
  /*-UNLOCK-*/
  sigprocmask (SIG_SETMASK, &old_mask, NULL);
#endif
  
  ETRACE ((_ETRACE,
	   "signal scheduled at %ld - tag 0x%lx - time %ld, handler 0x%lx, data 0x%lx\n",
	   (long)sim_events_time(sd),
	   (long)new_event,
	   (long)new_event->time_of_event,
	   (long)new_event->handler,
	   (long)new_event->data));
}
#endif


#if EXTERN_SIM_EVENTS_P
sim_event *
sim_events_watch_clock (SIM_DESC sd,
			unsigned delta_ms_time,
			sim_event_handler *handler,
			void *data)
{
  sim_events *events = STATE_EVENTS (sd);
  sim_event *new_event = sim_events_zalloc (sd);
  /* type */
  new_event->watching = watch_clock;
  /* handler */
  new_event->data = data;
  new_event->handler = handler;
  /* data */
  if (events->resume_wallclock == 0)
    new_event->wallclock = (events->elapsed_wallclock + delta_ms_time);
  else
    new_event->wallclock = (events->elapsed_wallclock
			    + sim_elapsed_time_since (events->resume_wallclock)
			    + delta_ms_time);
  /* insert */
  new_event->next = events->watchpoints;
  events->watchpoints = new_event;
  events->work_pending = 1;
  ETRACE ((_ETRACE,
	  "event watching clock at %ld - tag 0x%lx - wallclock %ld, handler 0x%lx, data 0x%lx\n",
	   (long)sim_events_time (sd),
	   (long)new_event,
	   (long)new_event->wallclock,
	   (long)new_event->handler,
	   (long)new_event->data));
  return new_event;
}
#endif


#if EXTERN_SIM_EVENTS_P
sim_event *
sim_events_watch_sim (SIM_DESC sd,
		      void *host_addr,
		      int nr_bytes,
		      int byte_order,
		      int is_within,
		      unsigned64 lb,
		      unsigned64 ub,
		      sim_event_handler *handler,
		      void *data)
{
  sim_events *events = STATE_EVENTS (sd);
  sim_event *new_event = sim_events_zalloc (sd);
  /* type */
  switch (byte_order)
    {
    case 0:
      switch (nr_bytes)
	{
	case 1: new_event->watching = watch_sim_host_1; break;
	case 2: new_event->watching = watch_sim_host_2; break;
	case 4: new_event->watching = watch_sim_host_4; break;
	case 8: new_event->watching = watch_sim_host_8; break;
	default: sim_io_error (sd, "sim_events_watch_sim - invalid nr bytes");
	}
      break;
    case BIG_ENDIAN:
      switch (nr_bytes)
	{
	case 1: new_event->watching = watch_sim_be_1; break;
	case 2: new_event->watching = watch_sim_be_2; break;
	case 4: new_event->watching = watch_sim_be_4; break;
	case 8: new_event->watching = watch_sim_be_8; break;
	default: sim_io_error (sd, "sim_events_watch_sim - invalid nr bytes");
	}
      break;
    case LITTLE_ENDIAN:
      switch (nr_bytes)
	{
	case 1: new_event->watching = watch_sim_le_1; break;
	case 2: new_event->watching = watch_sim_le_2; break;
	case 4: new_event->watching = watch_sim_le_4; break;
	case 8: new_event->watching = watch_sim_le_8; break;
	default: sim_io_error (sd, "sim_events_watch_sim - invalid nr bytes");
	}
      break;
    default:
      sim_io_error (sd, "sim_events_watch_sim - invalid byte order");
    }
  /* handler */
  new_event->data = data;
  new_event->handler = handler;
  /* data */
  new_event->host_addr = host_addr;
  new_event->lb = lb;
  new_event->lb64 = lb;
  new_event->ub = ub;
  new_event->ub64 = ub;
  new_event->is_within = (is_within != 0);
  /* insert */
  new_event->next = events->watchpoints;
  events->watchpoints = new_event;
  events->work_pending = 1;
  ETRACE ((_ETRACE,
	   "event watching host at %ld - tag 0x%lx - host-addr 0x%lx, 0x%lx..0x%lx, handler 0x%lx, data 0x%lx\n",
	   (long)sim_events_time (sd),
	   (long)new_event,
	   (long)new_event->host_addr,
	   (long)new_event->lb,
	   (long)new_event->ub,
	   (long)new_event->handler,
	   (long)new_event->data));
  return new_event;
}
#endif


#if EXTERN_SIM_EVENTS_P
sim_event *
sim_events_watch_core (SIM_DESC sd,
		       address_word core_addr,
		       unsigned core_map,
		       int nr_bytes,
		       int byte_order,
		       int is_within,
		       unsigned64 lb,
		       unsigned64 ub,
		       sim_event_handler *handler,
		       void *data)
{
  sim_events *events = STATE_EVENTS (sd);
  sim_event *new_event = sim_events_zalloc (sd);
  /* type */
  switch (byte_order)
    {
    case 0:
      switch (nr_bytes)
	{
	case 1: new_event->watching = watch_core_targ_1; break;
	case 2: new_event->watching = watch_core_targ_2; break;
	case 4: new_event->watching = watch_core_targ_4; break;
	case 8: new_event->watching = watch_core_targ_8; break;
	default: sim_io_error (sd, "sim_events_watch_core - invalid nr bytes");
	}
      break;
    case BIG_ENDIAN:
      switch (nr_bytes)
	{
	case 1: new_event->watching = watch_core_be_1; break;
	case 2: new_event->watching = watch_core_be_2; break;
	case 4: new_event->watching = watch_core_be_4; break;
	case 8: new_event->watching = watch_core_be_8; break;
	default: sim_io_error (sd, "sim_events_watch_core - invalid nr bytes");
	}
      break;
    case LITTLE_ENDIAN:
      switch (nr_bytes)
	{
	case 1: new_event->watching = watch_core_le_1; break;
	case 2: new_event->watching = watch_core_le_2; break;
	case 4: new_event->watching = watch_core_le_4; break;
	case 8: new_event->watching = watch_core_le_8; break;
	default: sim_io_error (sd, "sim_events_watch_core - invalid nr bytes");
	}
      break;
    default:
      sim_io_error (sd, "sim_events_watch_core - invalid byte order");
    }
  /* handler */
  new_event->data = data;
  new_event->handler = handler;
  /* data */
  new_event->core_addr = core_addr;
  new_event->core_map = core_map;
  new_event->lb = lb;
  new_event->lb64 = lb;
  new_event->ub = ub;
  new_event->ub64 = ub;
  new_event->is_within = (is_within != 0);
  /* insert */
  new_event->next = events->watchpoints;
  events->watchpoints = new_event;
  events->work_pending = 1;
  ETRACE ((_ETRACE,
	   "event watching host at %ld - tag 0x%lx - host-addr 0x%lx, 0x%lx..0x%lx, handler 0x%lx, data 0x%lx\n",
	   (long)sim_events_time (sd),
	   (long)new_event,
	   (long)new_event->host_addr,
	   (long)new_event->lb,
	   (long)new_event->ub,
	   (long)new_event->handler,
	   (long)new_event->data));
  return new_event;
}
#endif


#if EXTERN_SIM_EVENTS_P
void
sim_events_deschedule (SIM_DESC sd,
		       sim_event *event_to_remove)
{
  sim_events *events = STATE_EVENTS (sd);
  sim_event *to_remove = (sim_event*)event_to_remove;
  if (event_to_remove != NULL)
    {
      sim_event **queue = NULL;
      while ((queue = next_event_queue (sd, queue)) != NULL)
	{
	  sim_event **ptr_to_current;
	  for (ptr_to_current = queue;
	       *ptr_to_current != NULL && *ptr_to_current != to_remove;
	       ptr_to_current = &(*ptr_to_current)->next);
	  if (*ptr_to_current == to_remove)
	    {
	      sim_event *dead = *ptr_to_current;
	      *ptr_to_current = dead->next;
	      ETRACE ((_ETRACE,
		       "event/watch descheduled at %ld - tag 0x%lx - time %ld, handler 0x%lx, data 0x%lx%s%s\n",
		       (long) sim_events_time (sd),
		       (long) event_to_remove,
		       (long) dead->time_of_event,
		       (long) dead->handler,
		       (long) dead->data,
		       (dead->trace != NULL) ? ", " : "",
		       (dead->trace != NULL) ? dead->trace : ""));
	      sim_events_free (sd, dead);
	      update_time_from_event (sd);
	      SIM_ASSERT ((events->time_from_event >= 0) == (events->queue != NULL));
	      return;
	    }
	}
    }
  ETRACE ((_ETRACE,
	   "event/watch descheduled at %ld - tag 0x%lx - not found\n",
	   (long) sim_events_time (sd),
	   (long) event_to_remove));
}
#endif


STATIC_INLINE_SIM_EVENTS\
(int)
sim_watch_valid (SIM_DESC sd,
		 sim_event *to_do)
{
  switch (to_do->watching)
    {

#define WATCH_CORE(N,OP,EXT) \
      int ok; \
      unsigned_##N word = 0; \
      int nr_read = sim_core_read_buffer (sd, NULL, to_do->core_map, &word, \
					  to_do->core_addr, sizeof (word)); \
      OP (word); \
      ok = (nr_read == sizeof (unsigned_##N) \
	    && (to_do->is_within \
		== (word >= to_do->lb##EXT \
		    && word <= to_do->ub##EXT)));

    case watch_core_targ_1:
      {
	WATCH_CORE (1, T2H,);
	return ok;
      }
    case watch_core_targ_2:
      {
        WATCH_CORE (2, T2H,);
	return ok;
      }
    case watch_core_targ_4:
      {
        WATCH_CORE (4, T2H,);
	return ok;
      }
    case watch_core_targ_8:
      {
        WATCH_CORE (8, T2H,64);
	return ok;
      }

    case watch_core_be_1:
      {
        WATCH_CORE (1, BE2H,);
	return ok;
      }
    case watch_core_be_2:
      {
        WATCH_CORE (2, BE2H,);
	return ok;
      }
    case watch_core_be_4:
      {
        WATCH_CORE (4, BE2H,);
	return ok;
      }
    case watch_core_be_8:
      {
        WATCH_CORE (8, BE2H,64);
	return ok;
      }

    case watch_core_le_1:
      {
        WATCH_CORE (1, LE2H,);
	return ok;
      }
    case watch_core_le_2:
      {
        WATCH_CORE (2, LE2H,);
	return ok;
      }
    case watch_core_le_4:
      {
        WATCH_CORE (4, LE2H,);
	return ok;
      }
    case watch_core_le_8:
      {
        WATCH_CORE (8, LE2H,64);
	return ok;
      }
#undef WATCH_CORE

#define WATCH_SIM(N,OP,EXT) \
      int ok; \
      unsigned_##N word = *(unsigned_##N*)to_do->host_addr; \
      OP (word); \
      ok = (to_do->is_within \
	    == (word >= to_do->lb##EXT \
		&& word <= to_do->ub##EXT));

    case watch_sim_host_1:
      {
        WATCH_SIM (1, word = ,);
	return ok;
      }
    case watch_sim_host_2:
      {
        WATCH_SIM (2, word = ,);
	return ok;
      }
    case watch_sim_host_4:
      {
        WATCH_SIM (4, word = ,);
	return ok;
      }
    case watch_sim_host_8:
      {
        WATCH_SIM (8, word = ,64);
	return ok;
      }

    case watch_sim_be_1:
      {
        WATCH_SIM (1, BE2H,);
	return ok;
      }
    case watch_sim_be_2:
      {
        WATCH_SIM (2, BE2H,);
	return ok;
      }
    case watch_sim_be_4:
      {
        WATCH_SIM (4, BE2H,);
	return ok;
      }
    case watch_sim_be_8:
      {
        WATCH_SIM (8, BE2H,64);
	return ok;
      }

    case watch_sim_le_1:
      {
        WATCH_SIM (1, LE2H,);
	return ok;
      }
    case watch_sim_le_2:
      {
        WATCH_SIM (1, LE2H,);
	return ok;
      }
    case watch_sim_le_4:
      {
        WATCH_SIM (1, LE2H,);
	return ok;
      }
    case watch_sim_le_8:
      {
        WATCH_SIM (1, LE2H,64);
	return ok;
      }
#undef WATCH_SIM

    case watch_clock: /* wallclock */
      {
	unsigned long elapsed_time = sim_events_elapsed_time (sd);
	return (elapsed_time >= to_do->wallclock);
      }

    default:
      sim_io_error (sd, "sim_watch_valid - bad switch");
      break;

    }
  return 1;
}


INLINE_SIM_EVENTS\
(int)
sim_events_tick (SIM_DESC sd)
{
  sim_events *events = STATE_EVENTS (sd);

  /* this should only be called after the previous ticks have been
     fully processed */

  /* Advance the time but *only* if there is nothing to process */
  if (events->work_pending
      || events->time_from_event == 0)
    {
      events->nr_ticks_to_process += 1;
      return 1;
    }
  else
    {
      events->time_from_event -= 1;
      return 0;
    }
}


INLINE_SIM_EVENTS\
(int)
sim_events_tickn (SIM_DESC sd,
		  int n)
{
  sim_events *events = STATE_EVENTS (sd);
  SIM_ASSERT (n > 0);

  /* this should only be called after the previous ticks have been
     fully processed */

  /* Advance the time but *only* if there is nothing to process */
  if (events->work_pending || events->time_from_event < n)
    {
      events->nr_ticks_to_process += n;
      return 1;
    }
  else
    {
      events->time_from_event -= n;
      return 0;
    }
}


INLINE_SIM_EVENTS\
(void)
sim_events_slip (SIM_DESC sd,
		 int slip)
{
  sim_events *events = STATE_EVENTS (sd);
  SIM_ASSERT (slip > 0);

  /* Flag a ready event with work_pending instead of number of ticks
     to process so that the time continues to be correct */
  if (events->time_from_event < slip)
    {
      events->work_pending = 1;
    }
  events->time_from_event -= slip;
}


INLINE_SIM_EVENTS\
(void)
sim_events_preprocess (SIM_DESC sd,
		       int events_were_last,
		       int events_were_next)
{
  sim_events *events = STATE_EVENTS(sd);
  if (events_were_last)
    {
      /* Halted part way through event processing */
      ASSERT (events->nr_ticks_to_process != 0);
      /* The external world can't tell if the event that stopped the
         simulator was the last event to process. */
      ASSERT (events_were_next);
      sim_events_process (sd);
    }
  else if (events_were_next)
    {
      /* Halted by the last processor */
      if (sim_events_tick (sd))
	sim_events_process (sd);
    }
}


INLINE_SIM_EVENTS\
(void)
sim_events_process (SIM_DESC sd)
{
  sim_events *events = STATE_EVENTS(sd);
  signed64 event_time = sim_events_time(sd);

  /* Clear work_pending before checking nr_held.  Clearing
     work_pending after nr_held (with out a lock could loose an
     event). */
  events->work_pending = 0;

  /* move any events that were asynchronously queued by any signal
     handlers onto the real event queue.  */
  if (events->nr_held > 0)
    {
      int i;
      
#if defined(HAVE_SIGPROCMASK) && defined(SIG_SETMASK)
      /*-LOCK-*/
      sigset_t old_mask;
      sigset_t new_mask;
      sigfillset(&new_mask);
      sigprocmask(SIG_SETMASK, &new_mask, &old_mask);
#endif

      for (i = 0; i < events->nr_held; i++)
	{
	  sim_event *entry = &events->held [i];
	  sim_events_schedule (sd,
			       entry->time_of_event,
			       entry->handler,
			       entry->data);
	}
      events->nr_held = 0;
      
#if defined(HAVE_SIGPROCMASK) && defined(SIG_SETMASK)
      /*-UNLOCK-*/
      sigprocmask(SIG_SETMASK, &old_mask, NULL);
#endif
      
    }
  
  /* Process any watchpoints. Be careful to allow a watchpoint to
     appear/disappear under our feet.
     To ensure that watchpoints are processed only once per cycle,
     they are moved onto a watched queue, this returned to the
     watchpoint queue when all queue processing has been
     completed. */
  while (events->watchpoints != NULL)
    {
      sim_event *to_do = events->watchpoints;
      events->watchpoints = to_do->next;
      if (sim_watch_valid (sd, to_do))
	{
	  sim_event_handler *handler = to_do->handler;
	  void *data = to_do->data;
	  ETRACE((_ETRACE,
		  "event issued at %ld - tag 0x%lx - handler 0x%lx, data 0x%lx%s%s\n",
		  (long) event_time,
		  (long) to_do,
		  (long) handler,
		  (long) data,
		  (to_do->trace != NULL) ? ", " : "",
		  (to_do->trace != NULL) ? to_do->trace : ""));
	  sim_events_free (sd, to_do);
	  handler (sd, data);
	}
      else
	{
	  to_do->next = events->watchedpoints;
	  events->watchedpoints = to_do;
	}
    }
  
  /* consume all events for this or earlier times.  Be careful to
     allow an event to appear/disappear under our feet */
  while (events->queue->time_of_event <
	 (event_time + events->nr_ticks_to_process))
    {
      sim_event *to_do = events->queue;
      sim_event_handler *handler = to_do->handler;
      void *data = to_do->data;
      events->queue = to_do->next;
      update_time_from_event (sd);
      ETRACE((_ETRACE,
	      "event issued at %ld - tag 0x%lx - handler 0x%lx, data 0x%lx%s%s\n",
	      (long) event_time,
	      (long) to_do,
	      (long) handler,
	      (long) data,
	      (to_do->trace != NULL) ? ", " : "",
	      (to_do->trace != NULL) ? to_do->trace : ""));
      sim_events_free (sd, to_do);
      handler (sd, data);
    }
  
  /* put things back where they belong ready for the next iteration */
  events->watchpoints = events->watchedpoints;
  events->watchedpoints = NULL;
  if (events->watchpoints != NULL)
    events->work_pending = 1;
  
  /* advance the time */
  SIM_ASSERT (events->time_from_event >= events->nr_ticks_to_process);
  SIM_ASSERT (events->queue != NULL); /* always poll event */
  events->time_from_event -= events->nr_ticks_to_process;

  /* this round of processing complete */
  events->nr_ticks_to_process = 0;
}

#endif
