/*
 * Copyright (C) 1984 Sun Microsystems, Inc.
 * Based on svc_run.c from statd which claimed:
 * Modified by Jeffrey A. Uphoff, 1995, 1997-1999.
 * Modified by Olaf Kirch, 1996.
 *
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
 * Allow svc_run to listen to other file descriptors as well
 */

/* 
 * This is the RPC server side idle loop.
 * Wait for input, call server program.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <rpc/rpc.h>
#include "xlog.h"
#include <errno.h>
#include <time.h>

#ifdef HAVE_LIBTIRPC
#include <rpc/rpc_com.h>
#endif

void cache_set_fds(fd_set *fdset);
int cache_process_req(fd_set *readfds);

#if LONG_MAX != INT_MAX	
/* bug in glibc 2.3.6 and earlier, we need
 * our own svc_getreqset
 */
static void
my_svc_getreqset (fd_set *readfds)
{
	fd_mask mask;
	fd_mask *maskp;
	int setsize;
	int sock;
	int bit;

	setsize = _rpc_dtablesize ();
	if (setsize > FD_SETSIZE)
		setsize = FD_SETSIZE;
	maskp = readfds->fds_bits;
	for (sock = 0; sock < setsize; sock += NFDBITS)
		for (mask = *maskp++;
		     (bit = ffsl (mask));
		     mask ^= (1L << (bit - 1)))
			svc_getreq_common (sock + bit - 1);
}
#define svc_getreqset my_svc_getreqset

#endif

/*
 * The heart of the server.  A crib from libc for the most part...
 */
void
my_svc_run(void)
{
	fd_set	readfds;
	int	selret;

	for (;;) {

		readfds = svc_fdset;
		cache_set_fds(&readfds);

		selret = select(FD_SETSIZE, &readfds,
				(void *) 0, (void *) 0, (struct timeval *) 0);


		switch (selret) {
		case -1:
			if (errno == EINTR || errno == ECONNREFUSED
			 || errno == ENETUNREACH || errno == EHOSTUNREACH)
				continue;
			xlog(L_ERROR, "my_svc_run() - select: %m");
			return;

		default:
			selret -= cache_process_req(&readfds);
			if (selret)
				svc_getreqset(&readfds);
		}
	}
}
