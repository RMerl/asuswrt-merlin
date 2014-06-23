/*
 *
 *   Authors:
 *    Pedro Roque		<roque@di.fc.ul.pt>
 *    Lars Fenneberg		<lf@elemental.net>
 *
 *   This software is Copyright 1996-2000 by the above mentioned author(s),
 *   All Rights Reserved.
 *
 *   The license which is distributed with this software in the file COPYRIGHT
 *   applies to this software. If your distribution is missing this file, you
 *   may request it from <reubenhwk@gmail.com>.
 *
 */

#include "radvd.h"

#if defined CLOCK_HIGHRES && !defined CLOCK_MONOTONIC
#define CLOCK_MONOTONIC CLOCK_HIGHRES
#endif

int now(struct timeval *tv)
{
	static int monotonic = -1;
	/*
	 * clock_gettime() is granted to be increased monotonically when the
	 * monotonic clock is queried. Time starting point is unspecified, it
	 * could be the system start-up time, the Epoch, or something else,
	 * in any case the time starting point does not change once that the
	 * system has started up.
	 */
	if (monotonic) {
#ifdef CLOCK_MONOTONIC
		struct timespec ts;
		int retval = clock_gettime(CLOCK_MONOTONIC, &ts);
		if (retval == 0) {
			if (tv) {
				tv->tv_sec = ts.tv_sec;
				tv->tv_usec = ts.tv_nsec / 1000;
			}
			monotonic = 1;
		}
		if (monotonic == 1)
			return retval;
#endif
		monotonic = 0;
		flog(LOG_WARNING, "Monotonic clock source isn't unavailable.");
	}
	return gettimeofday(tv, NULL);
}

struct timeval next_timeval(double next)
{
	struct timeval tv;
	now(&tv);
	tv.tv_sec += (int)next;
	tv.tv_usec += 1000000 * (next - (int)next);
	return tv;
}

int timevaldiff(struct timeval const *a, struct timeval const *b)
{
	int msec;
	msec = (a->tv_sec - b->tv_sec) * 1000;
	msec += (a->tv_usec - b->tv_usec) / 1000;
	return msec;
}

/* Returns when the next time should expire in milliseconds. */
int next_time_msec(struct Interface const *iface)
{
	struct timeval tv;
	int retval;
	now(&tv);
	retval = timevaldiff(&iface->next_multicast, &tv);
	return retval >= 1 ? retval : 1;
}

int expired(struct Interface const *iface)
{
	struct timeval tv;
	now(&tv);
	if (timevaldiff(&iface->next_multicast, &tv) > 0)
		return 0;
	return 1;
}
