/*	$KAME: timer.c,v 1.6 2003/07/31 23:25:59 jinmei Exp $	*/

/*
 * Copyright (C) 2002 WIDE Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/queue.h>

#include <netinet/in.h>

#include <unistd.h>
#include <syslog.h>
#include <stdlib.h>
#include <string.h>
#if defined(__NetBSD__) || defined(__OpenBSD__)
#include <search.h>
#endif
#include "dhcp6.h"
#include "config.h"
#include "common.h"
#include "timer.h"

#define MILLION 1000000

LIST_HEAD(, dhcp6_timer) timer_head;
static struct timeval tm_sentinel;
static struct timeval tm_max = {0x7fffffff, 0x7fffffff};

static void timeval_add __P((struct timeval *, struct timeval *,
			     struct timeval *));

void
dhcp6_timer_init()
{
	LIST_INIT(&timer_head);
	tm_sentinel = tm_max;
}

struct dhcp6_timer *
dhcp6_add_timer(timeout, timeodata)
	struct dhcp6_timer *(*timeout) __P((void *));
	void *timeodata;
{
	struct dhcp6_timer *newtimer;

	if ((newtimer = malloc(sizeof(*newtimer))) == NULL) {
		dprintf(LOG_ERR, FNAME, "can't allocate memory");
		return (NULL);
	}

	memset(newtimer, 0, sizeof(*newtimer));

	if (timeout == NULL) {
		dprintf(LOG_ERR, FNAME, "timeout function unspecified");
		exit(1);
	}
	newtimer->expire = timeout;
	newtimer->expire_data = timeodata;
	newtimer->tm = tm_max;

	LIST_INSERT_HEAD(&timer_head, newtimer, link);

	return (newtimer);
}

void
dhcp6_remove_timer(timer)
	struct dhcp6_timer **timer;
{
	LIST_REMOVE(*timer, link);
	free(*timer);
	*timer = NULL;
}

void
dhcp6_set_timer(tm, timer)
	struct timeval *tm;
	struct dhcp6_timer *timer;
{
	struct timeval now;

	/* reset the timer */
	gettimeofday(&now, NULL);

	timeval_add(&now, tm, &timer->tm);

	/* update the next expiration time */
	if (TIMEVAL_LT(timer->tm, tm_sentinel))
		tm_sentinel = timer->tm;

	return;
}

/*
 * Check expiration for each timer. If a timer is expired,
 * call the expire function for the timer and update the timer.
 * Return the next interval for select() call.
 */
struct timeval *
dhcp6_check_timer()
{
	static struct timeval returnval;
	struct timeval now;
	struct dhcp6_timer *tm, *tm_next;

	gettimeofday(&now, NULL);

	tm_sentinel = tm_max;
	for (tm = LIST_FIRST(&timer_head); tm; tm = tm_next) {
		tm_next = LIST_NEXT(tm, link);
		
		if (TIMEVAL_LEQ(tm->tm, now)) {
			if ((*tm->expire)(tm->expire_data) == NULL)
				continue; /* timer has been freed */
		}

		if (TIMEVAL_LT(tm->tm, tm_sentinel))
			tm_sentinel = tm->tm;
	}

	if (TIMEVAL_EQUAL(tm_max, tm_sentinel)) {
		/* no need to timeout */
		return (NULL);
	} else if (TIMEVAL_LT(tm_sentinel, now)) {
		/* this may occur when the interval is too small */
		returnval.tv_sec = returnval.tv_usec = 0;
	} else
		timeval_sub(&tm_sentinel, &now, &returnval);
	return (&returnval);
}

struct timeval *
dhcp6_timer_rest(timer)
	struct dhcp6_timer *timer;
{
	struct timeval now;
	static struct timeval returnval; /* XXX */

	gettimeofday(&now, NULL);
	if (TIMEVAL_LEQ(timer->tm, now)) {
		dprintf(LOG_DEBUG, FNAME,
		    "a timer must be expired, but not yet");
		returnval.tv_sec = returnval.tv_usec = 0;
	} else
		timeval_sub(&timer->tm, &now, &returnval);

	return (&returnval);
}

/* result = a + b */
static void
timeval_add(a, b, result)
	struct timeval *a, *b, *result;
{
	long l;

	if ((l = a->tv_usec + b->tv_usec) < MILLION) {
		result->tv_usec = l;
		result->tv_sec = a->tv_sec + b->tv_sec;
	}
	else {
		result->tv_usec = l - MILLION;
		result->tv_sec = a->tv_sec + b->tv_sec + 1;
	}
}

/*
 * result = a - b
 * XXX: this function assumes that a >= b.
 */
void
timeval_sub(a, b, result)
	struct timeval *a, *b, *result;
{
	long l;

	if ((l = a->tv_usec - b->tv_usec) >= 0) {
		result->tv_usec = l;
		result->tv_sec = a->tv_sec - b->tv_sec;
	}
	else {
		result->tv_usec = MILLION + l;
		result->tv_sec = a->tv_sec - b->tv_sec - 1;
	}
}
