/*
 * ppoll.c - GNU extension ppoll() replacement
 */

/***********************************************************************
 *  Copyright © 2008 Rémi Denis-Courmont.                              *
 *  This program is free software; you can redistribute and/or modify  *
 *  it under the terms of the GNU General Public License as published  *
 *  by the Free Software Foundation; version 2 of the license, or (at  *
 *  your option) any later version.                                    *
 *                                                                     *
 *  This program is distributed in the hope that it will be useful,    *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of     *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.               *
 *  See the GNU General Public License for more details.               *
 *                                                                     *
 *  You should have received a copy of the GNU General Public License  *
 *  along with this program; if not, you can get it from:              *
 *  http://www.gnu.org/copyleft/gpl.html                               *
 ***********************************************************************/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <time.h>
#include <signal.h>

#include <sys/types.h>
#include <poll.h>
#if 0
# include <pthread.h> // sigprocmask is not thread-safe
#endif

int ppoll (struct pollfd *restrict fds, int n,
           const struct timespec *restrict ts,
           const sigset_t *restrict sigset)
{
	sigset_t origset;
	int timeout;
	int val;

	if (ts != NULL)
		timeout = (ts->tv_sec * 1000) + (ts->tv_nsec / 1000000);
	else
		timeout = -1;

	/* NOTE: ppoll() was introduced to fix the race condition between
	 * sigprocmask()/pthread_sigmask() and poll(). This replacement
	 * obviously reintroduces it. A more intricate implementation could
	 * avoid this bug (at a high performance cost).
	 */
#if 0
	val = pthread_sigmask (SIG_SETMASK, sigset, &origset);
	if (val)
	{
		errno = val;
		return -1;
	}
#else
	sigprocmask (SIG_SETMASK, sigset, &origset);
#endif

	val = poll (fds, n, timeout);

#if 0
	pthread_sigmask (SIG_SETMASK, &origset, NULL); /* cannot fail */
#else
	sigprocmask (SIG_SETMASK, &origset, NULL); /* cannot fail */
#endif
	return val;
}
