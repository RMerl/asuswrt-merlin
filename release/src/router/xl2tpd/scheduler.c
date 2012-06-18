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
 * Scheduler code for time based functionality
 *
 */

#include <stdlib.h>
#include <string.h>
#include "l2tp.h"
#include "scheduler.h"

struct schedule_entry *events;

void init_scheduler (void)
{
    events = NULL;
}

struct timeval *process_schedule (struct timeval *ptv)
{
    /* Check queue for events which should be
       executed right now.  Execute them, then
       see how long we should set the next timer
     */
    struct schedule_entry *p = events;
    struct timeval now;
    struct timeval then;
    while (events)
    {
        gettimeofday (&now, NULL);
        p = events;
        if (TVLESSEQ (p->tv, now))
        {
            events = events->next;
            /* This needs to be executed, as it has expired.
               It is expected that p->func will free p->data
               if it is necessary */
            (*p->func) (p->data);
            free (p);
        }
        else
            break;
    }
    /* When we get here, either there are no more events
       in the queue, or the remaining events need to happen
       in the future, so we should schedule another alarm */
    if (events)
    {
        then.tv_sec = events->tv.tv_sec - now.tv_sec;
        then.tv_usec = events->tv.tv_usec - now.tv_usec;
        if (then.tv_usec < 0)
        {
            then.tv_sec -= 1;
            then.tv_usec += 1000000;
        }
        if ((then.tv_sec <= 0) && (then.tv_usec <= 0))
        {
            l2tp_log (LOG_WARNING, "%s: Whoa...  Scheduling for <=0 time???\n",
                 __FUNCTION__);
            then.tv_sec = 1;
            then.tv_usec = 0;
        }
        *ptv = then;
        return ptv;
    }
    else
    {
        return NULL;
    }
}

struct schedule_entry *schedule (struct timeval tv, void (*func) (void *),
                                 void *data)
{
    /* Schedule func to be run at relative time tv with data
       as arguments.  If it has already expired, run it 
       immediately.  The queue should be in order of
       increasing time */
    struct schedule_entry *p = events, *q = NULL;
    struct timeval diff;
    diff = tv;
    gettimeofday (&tv, NULL);
    tv.tv_sec += diff.tv_sec;
    tv.tv_usec += diff.tv_usec;
    if (tv.tv_usec > 1000000)
    {
        tv.tv_sec++;
        tv.tv_usec -= 1000000;
    }
    while (p)
    {
        if (TVLESS (tv, p->tv))
            break;
        q = p;
        p = p->next;
    };
    if (q)
    {
        q->next =
            (struct schedule_entry *) malloc (sizeof (struct schedule_entry));
        q = q->next;
    }
    else
    {
        q = (struct schedule_entry *) malloc (sizeof (struct schedule_entry));
        events = q;
    }
    q->tv = tv;
    q->func = func;
    q->data = data;
    q->next = p;
    return q;

}

inline struct schedule_entry *aschedule (struct timeval tv,
                                         void (*func) (void *), void *data)
{
    /* Schedule func to be run at absolute time tv in the future with data
       as arguments */
    struct timeval now;
    gettimeofday (&now, NULL);
    tv.tv_usec -= now.tv_usec;
    if (tv.tv_usec < 0)
    {
        tv.tv_usec += 1000000;
        tv.tv_sec--;
    }
    tv.tv_sec -= now.tv_sec;
    return schedule (tv, func, data);
}

void deschedule (struct schedule_entry *s)
{
    struct schedule_entry *p = events, *q = NULL;
    if (!s)
        return;
    while (p)
    {
        if (p == s)
        {
            if (q)
            {
                q->next = p->next;
            }
            else
            {
                events = events->next;
            }
            free (p);
            break;
        }
        q = p;
        p = p->next;
    }
}
