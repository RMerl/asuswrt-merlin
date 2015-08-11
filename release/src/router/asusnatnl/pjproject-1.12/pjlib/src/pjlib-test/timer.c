/* $Id: timer.c 3553 2011-05-05 06:14:19Z nanang $ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */
#include "test.h"

/**
 * \page page_pjlib_timer_test Test: Timer
 *
 * This file provides implementation of \b timer_test(). It tests the
 * functionality of the timer heap.
 *
 *
 * This file is <b>pjlib-test/timer.c</b>
 *
 * \include pjlib-test/timer.c
 */


#if INCLUDE_TIMER_TEST

#include <pjlib.h>

#define LOOP		16
#define MIN_COUNT	250
#define MAX_COUNT	(LOOP * MIN_COUNT)
#define MIN_DELAY	2
#define	D		(MAX_COUNT / 32000)
#define DELAY		(D < MIN_DELAY ? MIN_DELAY : D)
#define THIS_FILE	"timer_test"


static void timer_callback(pj_timer_heap_t *ht, pj_timer_entry *e)
{
    PJ_UNUSED_ARG(ht);
    PJ_UNUSED_ARG(e);
}

static int test_timer_heap(void)
{
    int i, j;
    pj_timer_entry *entry;
    pj_pool_t *pool;
    pj_timer_heap_t *timer;
    pj_time_val delay;
    pj_status_t rc;    int err=0;
    unsigned size, count;

    size = pj_timer_heap_mem_size(MAX_COUNT)+MAX_COUNT*sizeof(pj_timer_entry);
    pool = pj_pool_create( mem, NULL, size, 4000, NULL);
    if (!pool) {
	PJ_LOG(3,("test", "...error: unable to create pool of %u bytes",
		  size));
	return -10;
    }

    entry = (pj_timer_entry*)pj_pool_calloc(pool, MAX_COUNT, sizeof(*entry));
    if (!entry)
	return -20;

    for (i=0; i<MAX_COUNT; ++i) {
	entry[i].cb = &timer_callback;
    }
    rc = pj_timer_heap_create(pool, MAX_COUNT, &timer);
    if (rc != PJ_SUCCESS) {
        app_perror("...error: unable to create timer heap", rc);
	return -30;
    }

    count = MIN_COUNT;
    for (i=0; i<LOOP; ++i) {
	int early = 0;
	int done=0;
	int cancelled=0;
	int rc;
	pj_timestamp t1, t2, t_sched, t_cancel, t_poll;
	pj_time_val now, expire;

	pj_gettimeofday(&now);
	pj_srand(now.sec);
	t_sched.u32.lo = t_cancel.u32.lo = t_poll.u32.lo = 0;

	// Register timers
	for (j=0; j<(int)count; ++j) {
	    delay.sec = pj_rand() % DELAY;
	    delay.msec = pj_rand() % 1000;

	    // Schedule timer
	    pj_get_timestamp(&t1);
	    rc = pj_timer_heap_schedule(timer, &entry[j], &delay);
	    if (rc != 0)
		return -40;
	    pj_get_timestamp(&t2);

	    t_sched.u32.lo += (t2.u32.lo - t1.u32.lo);

	    // Poll timers.
	    pj_get_timestamp(&t1);
	    rc = pj_timer_heap_poll(timer, NULL);
	    pj_get_timestamp(&t2);
	    if (rc > 0) {
		t_poll.u32.lo += (t2.u32.lo - t1.u32.lo);
		early += rc;
	    }
	}

	// Set the time where all timers should finish
	pj_gettimeofday(&expire);
	delay.sec = DELAY; 
	delay.msec = 0;
	PJ_TIME_VAL_ADD(expire, delay);

	// Wait unfil all timers finish, cancel some of them.
	do {
	    int index = pj_rand() % count;
	    pj_get_timestamp(&t1);
	    rc = pj_timer_heap_cancel(timer, &entry[index]);
	    pj_get_timestamp(&t2);
	    if (rc > 0) {
		cancelled += rc;
		t_cancel.u32.lo += (t2.u32.lo - t1.u32.lo);
	    }

	    pj_gettimeofday(&now);

	    pj_get_timestamp(&t1);
#if defined(PJ_SYMBIAN) && PJ_SYMBIAN!=0
	    /* On Symbian, we must use OS poll (Active Scheduler poll) since 
	     * timer is implemented using Active Object.
	     */
	    rc = 0;
	    while (pj_symbianos_poll(-1, 0))
		++rc;
#else
	    rc = pj_timer_heap_poll(timer, NULL);
#endif
	    pj_get_timestamp(&t2);
	    if (rc > 0) {
		done += rc;
		t_poll.u32.lo += (t2.u32.lo - t1.u32.lo);
	    }

	} while (PJ_TIME_VAL_LTE(now, expire)&&pj_timer_heap_count(timer) > 0);

	if (pj_timer_heap_count(timer)) {
	    PJ_LOG(3, (THIS_FILE, "ERROR: %d timers left", 
		       pj_timer_heap_count(timer)));
	    ++err;
	}
	t_sched.u32.lo /= count; 
	t_cancel.u32.lo /= count;
	t_poll.u32.lo /= count;
	PJ_LOG(4, (THIS_FILE, 
	        "...ok (count:%d, early:%d, cancelled:%d, "
		"sched:%d, cancel:%d poll:%d)", 
		count, early, cancelled, t_sched.u32.lo, t_cancel.u32.lo,
		t_poll.u32.lo));

	count = count * 2;
	if (count > MAX_COUNT)
	    break;
    }

    pj_pool_release(pool);
    return err;
}


int timer_test()
{
    return test_timer_heap();
}

#else
/* To prevent warning about "translation unit is empty"
 * when this test is disabled. 
 */
int dummy_timer_test;
#endif	/* INCLUDE_TIMER_TEST */


