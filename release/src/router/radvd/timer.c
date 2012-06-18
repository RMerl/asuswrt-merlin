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
 *   may request it from <pekkas@netcore.fi>.
 *
 */

#include "radvd.h"

struct timeval
next_timeval(double next)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	tv.tv_sec += (int)next;
	tv.tv_usec += 1000000 * (next - (int)next);
	return tv;
}

int
timevaldiff(struct timeval const *a, struct timeval const *b)
{
  int msec;
  msec = (a->tv_sec - b->tv_sec) * 1000;
  msec += (a->tv_usec - b->tv_usec) / 1000;
  return msec;
}


/* Returns when the next time should expire in milliseconds. */
int
next_time_msec(struct Interface const * iface)
{
	struct timeval tv;
	int retval;
	gettimeofday(&tv, NULL);
	retval = timevaldiff(&iface->next_multicast, &tv);
	return retval >= 1 ? retval : 1;
}

int
expired(struct Interface const * iface)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	if(timevaldiff(&iface->next_multicast, &tv) > 0)
		return 0;
	return 1;
}

