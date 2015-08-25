/* $Id: sleep.c 3553 2011-05-05 06:14:19Z nanang $ */
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
 * \page page_pjlib_sleep_test Test: Sleep, Time, and Timestamp
 *
 * This file provides implementation of \b sleep_test().
 *
 * \section sleep_test_sec Scope of the Test
 *
 * This tests:
 *  - whether pj_thread_sleep() works.
 *  - whether pj_gettimeofday() works.
 *  - whether pj_get_timestamp() and friends works.
 *
 * API tested:
 *  - pj_thread_sleep()
 *  - pj_gettimeofday()
 *  - PJ_TIME_VAL_SUB()
 *  - PJ_TIME_VAL_LTE()
 *  - pj_get_timestamp()
 *  - pj_get_timestamp_freq() (implicitly)
 *  - pj_elapsed_time()
 *  - pj_elapsed_usec()
 *
 *
 * This file is <b>pjlib-test/sleep.c</b>
 *
 * \include pjlib-test/sleep.c
 */

#if INCLUDE_SLEEP_TEST

#include <pjlib.h>

#define THIS_FILE   "sleep_test"

static int simple_sleep_test(void)
{
    enum { COUNT = 10 };
    int i;
    pj_status_t rc;
    
    PJ_LOG(3,(THIS_FILE, "..will write messages every 1 second:"));
    
    for (i=0; i<COUNT; ++i) {
	pj_time_val tv;
	pj_parsed_time pt;

	rc = pj_thread_sleep(1000);
	if (rc != PJ_SUCCESS) {
	    app_perror("...error: pj_thread_sleep()", rc);
	    return -10;
	}

	rc = pj_gettimeofday(&tv);
	if (rc != PJ_SUCCESS) {
	    app_perror("...error: pj_gettimeofday()", rc);
	    return -11;
	}

	pj_time_decode(&tv, &pt);

	PJ_LOG(3,(THIS_FILE, 
		  "...%04d-%02d-%02d %02d:%02d:%02d.%03d",
		  pt.year, pt.mon, pt.day,
		  pt.hour, pt.min, pt.sec, pt.msec));

    }

    return 0;
}

static int sleep_duration_test(void)
{
    enum { MIS = 20};
    unsigned duration[] = { 2000, 1000, 500, 200, 100 };
    unsigned i;
    pj_status_t rc;

    PJ_LOG(3,(THIS_FILE, "..running sleep duration test"));

    /* Test pj_thread_sleep() and pj_gettimeofday() */
    for (i=0; i<PJ_ARRAY_SIZE(duration); ++i) {
        pj_time_val start, stop;
	pj_uint32_t msec;

        /* Mark start of test. */
        rc = pj_gettimeofday(&start);
        if (rc != PJ_SUCCESS) {
            app_perror("...error: pj_gettimeofday()", rc);
            return -10;
        }

        /* Sleep */
        rc = pj_thread_sleep(duration[i]);
        if (rc != PJ_SUCCESS) {
            app_perror("...error: pj_thread_sleep()", rc);
            return -20;
        }

        /* Mark end of test. */
        rc = pj_gettimeofday(&stop);

        /* Calculate duration (store in stop). */
        PJ_TIME_VAL_SUB(stop, start);

	/* Convert to msec. */
	msec = PJ_TIME_VAL_MSEC(stop);

	/* Check if it's within range. */
	if (msec < duration[i] * (100-MIS)/100 ||
	    msec > duration[i] * (100+MIS)/100)
	{
	    PJ_LOG(3,(THIS_FILE, 
		      "...error: slept for %d ms instead of %d ms "
		      "(outside %d%% err window)",
		      msec, duration[i], MIS));
	    return -30;
	}
    }


    /* Test pj_thread_sleep() and pj_get_timestamp() and friends */
    for (i=0; i<PJ_ARRAY_SIZE(duration); ++i) {
	pj_time_val t1, t2;
        pj_timestamp start, stop;
	pj_uint32_t msec;

	pj_thread_sleep(0);

        /* Mark start of test. */
        rc = pj_get_timestamp(&start);
        if (rc != PJ_SUCCESS) {
            app_perror("...error: pj_get_timestamp()", rc);
            return -60;
        }

	/* ..also with gettimeofday() */
	pj_gettimeofday(&t1);

        /* Sleep */
        rc = pj_thread_sleep(duration[i]);
        if (rc != PJ_SUCCESS) {
            app_perror("...error: pj_thread_sleep()", rc);
            return -70;
        }

        /* Mark end of test. */
        pj_get_timestamp(&stop);

	/* ..also with gettimeofday() */
	pj_gettimeofday(&t2);

	/* Compare t1 and t2. */
	if (PJ_TIME_VAL_LT(t2, t1)) {
	    PJ_LOG(3,(THIS_FILE, "...error: t2 is less than t1!!"));
	    return -75;
	}

        /* Get elapsed time in msec */
        msec = pj_elapsed_msec(&start, &stop);

	/* Check if it's within range. */
	if (msec < duration[i] * (100-MIS)/100 ||
	    msec > duration[i] * (100+MIS)/100)
	{
	    PJ_LOG(3,(THIS_FILE, 
		      "...error: slept for %d ms instead of %d ms "
		      "(outside %d%% err window)",
		      msec, duration[i], MIS));
	    PJ_TIME_VAL_SUB(t2, t1);
	    PJ_LOG(3,(THIS_FILE, 
		      "...info: gettimeofday() reported duration is "
		      "%d msec",
		      PJ_TIME_VAL_MSEC(t2)));

	    return -76;
	}
    }

    /* All done. */
    return 0;
}

int sleep_test()
{
    int rc;

    rc = simple_sleep_test();
    if (rc != PJ_SUCCESS)
	return rc;

    rc = sleep_duration_test();
    if (rc != PJ_SUCCESS)
	return rc;

    return 0;
}

#else
/* To prevent warning about "translation unit is empty"
 * when this test is disabled. 
 */
int dummy_sleep_test;
#endif  /* INCLUDE_SLEEP_TEST */
