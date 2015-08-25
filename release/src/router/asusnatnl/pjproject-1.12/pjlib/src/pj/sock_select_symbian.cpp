/* $Id: sock_select_symbian.cpp 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pj/sock_select.h>
#include <pj/array.h>
#include <pj/assert.h>
#include <pj/os.h>
#include "os_symbian.h"

 
struct symbian_fd_set
{
    unsigned	 count;
    CPjSocket	*sock[PJ_IOQUEUE_MAX_HANDLES];
};


PJ_DEF(void) PJ_FD_ZERO(pj_fd_set_t *fdsetp)
{
    symbian_fd_set *fds = (symbian_fd_set *)fdsetp;
    fds->count = 0;
}


PJ_DEF(void) PJ_FD_SET(pj_sock_t fd, pj_fd_set_t *fdsetp)
{
    symbian_fd_set *fds = (symbian_fd_set *)fdsetp;

    PJ_ASSERT_ON_FAIL(fds->count < PJ_IOQUEUE_MAX_HANDLES, return);
    fds->sock[fds->count++] = (CPjSocket*)fd;
}


PJ_DEF(void) PJ_FD_CLR(pj_sock_t fd, pj_fd_set_t *fdsetp)
{
    symbian_fd_set *fds = (symbian_fd_set *)fdsetp;
    unsigned i;
    
    for (i=0; i<fds->count; ++i) {
	if (fds->sock[i] == (CPjSocket*)fd) {
	    pj_array_erase(fds->sock, sizeof(fds->sock[0]), fds->count, i);
	    --fds->count;
	    return;
	}
    }
}


PJ_DEF(pj_bool_t) PJ_FD_ISSET(pj_sock_t fd, const pj_fd_set_t *fdsetp)
{
    symbian_fd_set *fds = (symbian_fd_set *)fdsetp;
    unsigned i;
    
    for (i=0; i<fds->count; ++i) {
	if (fds->sock[i] == (CPjSocket*)fd) {
	    return PJ_TRUE;
	}
    }

    return PJ_FALSE;
}

PJ_DEF(pj_size_t) PJ_FD_COUNT(const pj_fd_set_t *fdsetp)
{
    symbian_fd_set *fds = (symbian_fd_set *)fdsetp;
    return fds->count;
}


PJ_DEF(int) pj_sock_select( int n, 
			    pj_fd_set_t *readfds, 
			    pj_fd_set_t *writefds,
			    pj_fd_set_t *exceptfds, 
			    const pj_time_val *timeout)
{
    CPjTimeoutTimer *pjTimer;
    unsigned i;

    PJ_UNUSED_ARG(n);
    PJ_UNUSED_ARG(writefds);
    PJ_UNUSED_ARG(exceptfds);

    if (timeout) {
	pjTimer = PjSymbianOS::Instance()->SelectTimeoutTimer();
	pjTimer->StartTimer(timeout->sec*1000 + timeout->msec);

    } else {
	pjTimer = NULL;
    }

    /* Scan for readable sockets */

    if (readfds) {
	symbian_fd_set *fds = (symbian_fd_set *)readfds;

	do {
	    /* Scan sockets for readily available data */
	    for (i=0; i<fds->count; ++i) {
		CPjSocket *pjsock = fds->sock[i];

		if (pjsock->Reader()) {
		    if (pjsock->Reader()->HasData() && !pjsock->Reader()->IsActive()) {

			/* Found socket with data ready */
			PJ_FD_ZERO(readfds);
			PJ_FD_SET((pj_sock_t)pjsock, readfds);

			/* Cancel timer, if any */
			if (pjTimer) {
			    pjTimer->Cancel();
			}

			/* Clear writable and exception fd_set */
			if (writefds)
			    PJ_FD_ZERO(writefds);
			if (exceptfds)
			    PJ_FD_ZERO(exceptfds);

			return 1;

		    } else if (!pjsock->Reader()->IsActive())
			pjsock->Reader()->StartRecvFrom();

		} else {
		    pjsock->CreateReader();
		    pjsock->Reader()->StartRecvFrom();
		}
	    }

	    PjSymbianOS::Instance()->WaitForActiveObjects();

	} while (pjTimer==NULL || !pjTimer->HasTimedOut());
    }


    /* Timeout */

    if (readfds)
	PJ_FD_ZERO(readfds);
    if (writefds)
	PJ_FD_ZERO(writefds);
    if (exceptfds)
	PJ_FD_ZERO(exceptfds);

    return 0;
}

