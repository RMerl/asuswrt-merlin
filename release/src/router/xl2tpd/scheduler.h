/*
 * Layer Two Tunnelling Protocol Daemon
 * Copyright (C) 1998 Adtran, Inc.
 * Copyright (C) 2002 Jeff McAdams
 *
 * Mark Spencer
 *
 * This software is distributed under the terms
 * of the GPL, which you should have received
 * along with this source.
 *
 * Scheduler structures and functions
 *
 */

#ifndef _SCHEDULER_H
#define _SCHEDULER_H
#include <sys/time.h>

/*
 * The idea is to provide a general scheduler which can schedule
 * events to be run periodically
 */

struct schedule_entry
{
    struct timeval tv;          /* Scheduled time to execute */
    void (*func) (void *);      /* Function to execute */
    void *data;                 /* Data to be passed to func */
    struct schedule_entry *next;        /* Next entry in queue */
};

extern struct schedule_entry *events;

/* Schedule func to be executed with argument data sometime
   tv in the future. */

struct schedule_entry *schedule (struct timeval tv, void (*func) (void *),
                                 void *data);

/* Like schedule() but tv represents an absolute time in the future */

struct schedule_entry *aschedule (struct timeval tv, void (*func) (void *),
                                  void *data);

/* Remove a scheduled event from the queue */

void deschedule (struct schedule_entry *);

/* Initialization function */
void init_scheduler (void);

/* Scheduled event processor */
struct timeval *process_schedule(struct timeval *);

/* Compare two timeval functions and see if a <= b */

#define TVLESS(a,b) ((a).tv_sec == (b).tv_sec ? \
				((a).tv_usec < (b).tv_usec) : \
				((a).tv_sec < (b).tv_sec))
#define TVLESSEQ(a,b) ((a).tv_sec == (b).tv_sec ? \
				((a).tv_usec <= (b).tv_usec) : \
				((a).tv_sec <= (b).tv_sec))
#define TVGT(a,b) ((a).tv_sec == (b).tv_sec ? \
				((a).tv_usec > (b).tv_usec) : \
				((a).tv_sec > (b).tv_sec))
#endif
