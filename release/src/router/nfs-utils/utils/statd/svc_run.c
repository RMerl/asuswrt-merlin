/*
 * Copyright (C) 1984 Sun Microsystems, Inc.
 * Modified by Jeffrey A. Uphoff, 1995, 1997-1999.
 * Modified by Olaf Kirch, 1996.
 *
 * NSM for Linux.
 */

/* 
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user.
 * 
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 * 
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 * 
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 * 
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 * 
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */

/* 
 * This has been modified for my own evil purposes to prevent deadlocks
 * when two hosts start NSM's simultaneously and try to notify each
 * other (which mainly occurs during testing), or to stop and smell the
 * roses when I have callbacks due.
 * --Jeff Uphoff.
 */

/* 
 * This is the RPC server side idle loop.
 * Wait for input, call server program.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <time.h>
#include "statd.h"
#include "notlist.h"

static int	svc_stop = 0;

/*
 * This is the global notify list onto which all SM_NOTIFY and CALLBACK
 * requests are put.
 */
notify_list *	notify = NULL;

/*
 * Jump-off function.
 */
void
my_svc_exit(void)
{
	svc_stop = 1;
}


/*
 * The heart of the server.  A crib from libc for the most part...
 */
void
my_svc_run(void)
{
	FD_SET_TYPE	readfds;
	int             selret;
	time_t		now;

	svc_stop = 0;

	for (;;) {
		if (svc_stop)
			return;

		/* Ah, there are some notifications to be processed */
		while (notify && NL_WHEN(notify) <= time(&now)) {
			process_notify_list();
		}

		readfds = SVC_FDSET;
		if (notify) {
			struct timeval	tv;

			tv.tv_sec  = NL_WHEN(notify) - now;
			tv.tv_usec = 0;
			dprintf(N_DEBUG, "Waiting for reply... (timeo %d)",
							tv.tv_sec);
			selret = select(FD_SETSIZE, &readfds,
				(void *) 0, (void *) 0, &tv);
		} else {
			dprintf(N_DEBUG, "Waiting for client connections.");
			selret = select(FD_SETSIZE, &readfds,
				(void *) 0, (void *) 0, (struct timeval *) 0);
		}

		switch (selret) {
		case -1:
			if (errno == EINTR || errno == ECONNREFUSED
			 || errno == ENETUNREACH || errno == EHOSTUNREACH)
				continue;
			note(N_ERROR, "my_svc_run() - select: %s",
				strerror (errno));
			return;

		case 0:
			/* A notify/callback timed out. */
			continue;

		default:
			selret -= process_reply(&readfds);
			if (selret)
				svc_getreqset(&readfds);
		}
	}
}
