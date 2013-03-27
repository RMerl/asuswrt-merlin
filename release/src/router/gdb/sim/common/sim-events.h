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


#ifndef SIM_EVENTS_H
#define SIM_EVENTS_H


/* Notes:

   When scheduling an event, the a delta of zero/one refers to the
   timeline as follows:

   epoch   0|1              1|2              2|3              3|
   **queue**|--insn--|*queue*|--insn--|*queue*|--insn--|*queue*|
     |   ^               ^        |       ^                ^
     `- +0 ------------ +1 --..   `----- +0 ------------- +1 --..

   When the queue is initialized, the time is set to zero with a
   number of initialization events scheduled.  Consequently, as also
   illustrated above, the event queue should be processed before the
   first instruction.  That instruction being executed during tick 1.

   The simulator main loop may take a form similar to:

       if (halt-/restart-setjmp)
         {

	   .... // Determine who should go next
	   last-cpu-nr = get-last-cpu-nr (sd);
	   next-cpu-nr = get-next-cpu-nr (sd);
	   events-were-last? = (last-cpu-nr >= nr-cpus);
	   events-were-next? = (next-cpu-nr >= nr-cpus);

           .... // process any outstanding events
           sim_events_preprocess (sd, events-were-last?, events-were-next?);
	   if (events-were-next)
	     next-cpu-nr = 0;

           .... // prime main loop 

           while (1)
             {
	        .... // model one insn of next-cpu-nr .. nr-cpus
                if (sim_events_tick (sd))
	          sim_events_process (sd);
                next-cpu-nr = 0
	     }
         }

   NB.  In the above pseudo code it is assumed that any cpu-nr >=
   nr-cpus is a marker for the event queue. */


typedef void sim_event_handler(SIM_DESC sd, void *data);

typedef struct _sim_event sim_event;

typedef struct _sim_events sim_events;
struct _sim_events {
  int nr_ticks_to_process;
  sim_event *queue;
  sim_event *watchpoints;
  sim_event *watchedpoints;
  sim_event *free_list;
  /* flag additional work needed */
  volatile int work_pending;
  /* the asynchronous event queue */
#ifndef MAX_NR_SIGNAL_SIM_EVENTS
#define MAX_NR_SIGNAL_SIM_EVENTS 2
#endif
  sim_event *held;
  volatile int nr_held;
  /* timekeeping */
  unsigned long elapsed_wallclock;
  SIM_ELAPSED_TIME resume_wallclock;
  signed64 time_of_event;
  int time_from_event;
  int trace;
};



/* Install the "events" module.  */

extern SIM_RC sim_events_install (SIM_DESC sd);


/* Schedule an event DELTA_TIME ticks into the future */

extern sim_event *sim_events_schedule
(SIM_DESC sd,
 signed64 delta_time,
 sim_event_handler *handler,
 void *data);

extern sim_event *sim_events_schedule_tracef
(SIM_DESC sd,
 signed64 delta_time,
 sim_event_handler *handler,
 void *data,
 const char *fmt,
 ...) __attribute__ ((format (printf, 5, 6)));

extern sim_event *sim_events_schedule_vtracef
(SIM_DESC sd,
 signed64 delta_time,
 sim_event_handler *handler,
 void *data,
 const char *fmt,
 va_list ap);


extern void sim_events_schedule_after_signal
(SIM_DESC sd,
 signed64 delta_time,
 sim_event_handler *handler,
 void *data);

/* NB: signal level events can't have trace strings as malloc isn't
   available */



/* Schedule an event milli-seconds from NOW.  The exact interpretation
   of wallclock is host dependant. */

extern sim_event *sim_events_watch_clock
(SIM_DESC sd,
 unsigned delta_ms_time,
 sim_event_handler *handler,
 void *data);


/* Schedule an event when the test (IS_WITHIN == (VAL >= LB && VAL <=
   UB)) of the NR_BYTES value at HOST_ADDR with BYTE_ORDER endian is
   true.

   HOST_ADDR: pointer into the host address space.
   BYTE_ORDER: 0 - host endian; BIG_ENDIAN; LITTLE_ENDIAN */

extern sim_event *sim_events_watch_sim
(SIM_DESC sd,
 void *host_addr,
 int nr_bytes,
 int byte_order,
 int is_within,
 unsigned64 lb,
 unsigned64 ub,
 sim_event_handler *handler,
 void *data);


/* Schedule an event when the test (IS_WITHIN == (VAL >= LB && VAL <=
   UB)) of the NR_BYTES value at CORE_ADDR in BYTE_ORDER endian is
   true.

   CORE_ADDR/MAP: pointer into the target address space.
   BYTE_ORDER: 0 - current target endian; BIG_ENDIAN; LITTLE_ENDIAN */

extern sim_event *sim_events_watch_core
(SIM_DESC sd,
 address_word core_addr,
 unsigned map,
 int nr_bytes,
 int byte_order,
 int is_within,
 unsigned64 lb,
 unsigned64 ub,
 sim_event_handler *handler,
 void *data);

/* Deschedule the specified event */

extern void sim_events_deschedule
(SIM_DESC sd,
 sim_event *event_to_remove);


/* Prepare for main simulator loop.  Ensure that the next thing to do
   is not event processing.

   If the simulator halted part way through event processing then both
   EVENTS_WERE_LAST and EVENTS_WERE_NEXT shall be true.

   If the simulator halted after processing the last cpu, then only
   EVENTS_WERE_NEXT shall be true. */

INLINE_SIM_EVENTS\
(void) sim_events_preprocess
(SIM_DESC sd,
 int events_were_last,
 int events_were_next);


/* Progress time.

   Separated into two parts so that the main loop can save its context
   before the event queue is processed.  When sim_events_tick*()
   returns true, any simulation context should be saved and
   sim_events_process() called.

   SIM_EVENTS_TICK advances the clock by 1 cycle.

   SIM_EVENTS_TICKN advances the clock by N cycles (1..MAXINT). */

INLINE_SIM_EVENTS\
(int) sim_events_tick
(SIM_DESC sd);

INLINE_SIM_EVENTS\
(int) sim_events_tickn
(SIM_DESC sd,
 int n);

INLINE_SIM_EVENTS\
(void) sim_events_process
(SIM_DESC sd);


/* Advance the clock by an additional SLIP cycles at the next call to
   sim_events_tick*().  For multiple calls, the effect is
   accumulative. */

INLINE_SIM_EVENTS\
(void) sim_events_slip
(SIM_DESC sd,
 int slip);


/* Progress time such that an event shall occur upon the next call to
   sim_events tick */

#if 0
INLINE_SIM_EVENTS\
(void) sim_events_timewarp
(SIM_DESC sd);
#endif


/* local concept of elapsed target time */

INLINE_SIM_EVENTS\
(signed64) sim_events_time
(SIM_DESC sd);


/* local concept of elapsed host time (milliseconds) */

INLINE_SIM_EVENTS\
(unsigned long) sim_events_elapsed_time
(SIM_DESC sd);

/* Returns the time that remains before the event is raised. */
INLINE_SIM_EVENTS\
(signed64) sim_events_remain_time
(SIM_DESC sd, sim_event *event);


#endif
