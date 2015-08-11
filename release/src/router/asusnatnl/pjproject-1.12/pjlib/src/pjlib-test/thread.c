/* $Id: thread.c 3553 2011-05-05 06:14:19Z nanang $ */
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
 * \page page_pjlib_thread_test Test: Thread Test
 *
 * This file contains \a thread_test() definition.
 *
 * \section thread_test_scope_sec Scope of Test
 * This tests:
 *  - whether PJ_THREAD_SUSPENDED flag works.
 *  - whether multithreading works.
 *  - whether thread timeslicing works, and threads have equal
 *    time-slice proportion.
 * 
 * APIs tested:
 *  - pj_thread_create()
 *  - pj_thread_register()
 *  - pj_thread_this()
 *  - pj_thread_get_name()
 *  - pj_thread_destroy()
 *  - pj_thread_resume()
 *  - pj_thread_sleep()
 *  - pj_thread_join()
 *  - pj_thread_destroy()
 *
 *
 * This file is <b>pjlib-test/thread.c</b>
 *
 * \include pjlib-test/thread.c
 */
#if INCLUDE_THREAD_TEST

#include <pjlib.h>

#define THIS_FILE   "thread_test"

static volatile int quit_flag=0;

#if 0
#   define TRACE__(args)	PJ_LOG(3,args)
#else
#   define TRACE__(args)
#endif


/*
 * The thread's entry point.
 *
 * Each of the thread mainly will just execute the loop which
 * increments a variable.
 */
static void* thread_proc(pj_uint32_t *pcounter)
{
    /* Test that pj_thread_register() works. */
    pj_thread_desc desc;
    pj_thread_t *this_thread;
    unsigned id;
    pj_status_t rc;

    id = *pcounter;
    TRACE__((THIS_FILE, "     thread %d running..", id));

    pj_bzero(desc, sizeof(desc));

    rc = pj_thread_register(0, "thread", desc, &this_thread);
    if (rc != PJ_SUCCESS) {
        app_perror("...error in pj_thread_register", rc);
        return NULL;
    }

    /* Test that pj_thread_this() works */
    this_thread = pj_thread_this(0);
    if (this_thread == NULL) {
        PJ_LOG(3,(THIS_FILE, "...error: pj_thread_this() returns NULL!"));
        return NULL;
    }

    /* Test that pj_thread_get_name() works */
    if (pj_thread_get_name(this_thread) == NULL) {
        PJ_LOG(3,(THIS_FILE, "...error: pj_thread_get_name() returns NULL!"));
        return NULL;
    }

    /* Main loop */
    for (;!quit_flag;) {
	(*pcounter)++;
        //Must sleep if platform doesn't do time-slicing.
	//pj_thread_sleep(0);
    }

    TRACE__((THIS_FILE, "     thread %d quitting..", id));
    return NULL;
}

/*
 * simple_thread()
 */
static int simple_thread(const char *title, unsigned flags)
{
    pj_pool_t *pool;
    pj_thread_t *thread;
    pj_status_t rc;
    pj_uint32_t counter = 0;
 
    PJ_LOG(3,(THIS_FILE, "..%s", title));
    
    pool = pj_pool_create(mem, NULL, 4000, 4000, NULL);
    if (!pool)
	return -1000;

    quit_flag = 0;

    TRACE__((THIS_FILE, "    Creating thread 0.."));
    rc = pj_thread_create(pool, "thread", (pj_thread_proc*)&thread_proc,
			  &counter,
			  PJ_THREAD_DEFAULT_STACK_SIZE,
			  flags,
			  &thread);

    if (rc != PJ_SUCCESS) {
	app_perror("...error: unable to create thread", rc);
	return -1010;
    }

    TRACE__((THIS_FILE, "    Main thread waiting.."));
    pj_thread_sleep(1500);
    TRACE__((THIS_FILE, "    Main thread resuming.."));

    if (flags & PJ_THREAD_SUSPENDED) {

	/* Check that counter is still zero */
	if (counter != 0) {
	    PJ_LOG(3,(THIS_FILE, "...error: thread is not suspended"));
	    return -1015;
	}
	
	rc = pj_thread_resume(thread);
	if (rc != PJ_SUCCESS) {
	    app_perror("...error: resume thread error", rc);
	    return -1020;
	}
    }
    
    PJ_LOG(3,(THIS_FILE, "..waiting for thread to quit.."));

    pj_thread_sleep(1500);

    quit_flag = 1;
    pj_thread_join(thread);

    pj_pool_release(pool);

    if (counter == 0) {
	PJ_LOG(3,(THIS_FILE, "...error: thread is not running"));
	return -1025;
    }
    
    PJ_LOG(3,(THIS_FILE, "...%s success", title));
    return PJ_SUCCESS;
}


/*
 * timeslice_test()
 */
static int timeslice_test(void)
{
    enum { NUM_THREADS = 4 };
    pj_pool_t *pool;
    pj_uint32_t counter[NUM_THREADS], lowest, highest, diff;
    pj_thread_t *thread[NUM_THREADS];
    unsigned i;
    pj_status_t rc;

    quit_flag = 0;

    pool = pj_pool_create(mem, NULL, 4000, 4000, NULL);
    if (!pool)
        return -10;

    PJ_LOG(3,(THIS_FILE, "..timeslice testing with %d threads", NUM_THREADS));

    /* Create all threads in suspended mode. */
    for (i=0; i<NUM_THREADS; ++i) {
        counter[i] = i;
        rc = pj_thread_create(pool, "thread", (pj_thread_proc*)&thread_proc, 
			      &counter[i], 
                              PJ_THREAD_DEFAULT_STACK_SIZE, 
                              PJ_THREAD_SUSPENDED, 
                              &thread[i]);
        if (rc!=PJ_SUCCESS) {
            app_perror("...ERROR in pj_thread_create()", rc);
            return -20;
        }
    }

    /* Sleep for 1 second.
     * The purpose of this is to test whether all threads are suspended.
     */
    TRACE__((THIS_FILE, "    Main thread waiting.."));
    pj_thread_sleep(1000);
    TRACE__((THIS_FILE, "    Main thread resuming.."));

    /* Check that all counters are still zero. */
    for (i=0; i<NUM_THREADS; ++i) {
        if (counter[i] > i) {
            PJ_LOG(3,(THIS_FILE, "....ERROR! Thread %d-th is not suspended!", 
		      i));
            return -30;
        }
    }

    /* Now resume all threads. */
    for (i=0; i<NUM_THREADS; ++i) {
	TRACE__((THIS_FILE, "    Resuming thread %d [%p]..", i, thread[i]));
        rc = pj_thread_resume(thread[i]);
        if (rc != PJ_SUCCESS) {
            app_perror("...ERROR in pj_thread_resume()", rc);
            return -40;
        }
    }

    /* Main thread sleeps for some time to allow threads to run. 
     * The longer we sleep, the more accurate the calculation will be,
     * but it'll make user waits for longer for the test to finish.
     */
    TRACE__((THIS_FILE, "    Main thread waiting (5s).."));
    pj_thread_sleep(5000);
    TRACE__((THIS_FILE, "    Main thread resuming.."));

    /* Signal all threads to quit. */
    quit_flag = 1;

    /* Wait until all threads quit, then destroy. */
    for (i=0; i<NUM_THREADS; ++i) {
	TRACE__((THIS_FILE, "    Main thread joining thread %d [%p]..", 
			    i, thread[i]));
        rc = pj_thread_join(thread[i]);
        if (rc != PJ_SUCCESS) {
            app_perror("...ERROR in pj_thread_join()", rc);
            return -50;
        }
	TRACE__((THIS_FILE, "    Destroying thread %d [%p]..", i, thread[i]));
        rc = pj_thread_destroy(thread[i]);
        if (rc != PJ_SUCCESS) {
            app_perror("...ERROR in pj_thread_destroy()", rc);
            return -60;
        }
    }

    TRACE__((THIS_FILE, "    Main thread calculating time slices.."));

    /* Now examine the value of the counters.
     * Check that all threads had equal proportion of processing.
     */
    lowest = 0xFFFFFFFF;
    highest = 0;
    for (i=0; i<NUM_THREADS; ++i) {
        if (counter[i] < lowest)
            lowest = counter[i];
        if (counter[i] > highest)
            highest = counter[i];
    }

    /* Check that all threads are running. */
    if (lowest < 2) {
        PJ_LOG(3,(THIS_FILE, "...ERROR: not all threads were running!"));
        return -70;
    }

    /* The difference between lowest and higest should be lower than 50%.
     */
    diff = (highest-lowest)*100 / ((highest+lowest)/2);
    if ( diff >= 50) {
        PJ_LOG(3,(THIS_FILE, 
		  "...ERROR: thread didn't have equal timeslice!"));
        PJ_LOG(3,(THIS_FILE, 
		  ".....lowest counter=%u, highest counter=%u, diff=%u%%",
                  lowest, highest, diff));
        return -80;
    } else {
        PJ_LOG(3,(THIS_FILE, 
                  "...info: timeslice diff between lowest & highest=%u%%",
                  diff));
    }

    pj_pool_release(pool);
    return 0;
}

int thread_test(void)
{
    int rc;

    rc = simple_thread("simple thread test", 0);
    if (rc != PJ_SUCCESS)
	return rc;
    
    rc = simple_thread("suspended thread test", PJ_THREAD_SUSPENDED);
    if (rc != PJ_SUCCESS)
	return rc;
    
    rc = timeslice_test();
    if (rc != PJ_SUCCESS)
	return rc;

    return rc;
}

#else
/* To prevent warning about "translation unit is empty"
 * when this test is disabled. 
 */
int dummy_thread_test;
#endif	/* INCLUDE_THREAD_TEST */


