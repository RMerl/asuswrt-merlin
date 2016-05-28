/* 
   Unix SMB/Netbios implementation.
   Version 3.0
   Samba select/poll implementation
   Copyright (C) Andrew Tridgell 1992-1998
   Copyright (C) Derrell Lipman 2003-2005
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

/*
 * WHY THIS FILE?
 *
 * This file implements the two functions in the select() family, as required
 * by samba.  The samba native functions, though, implement a pipe to help
 * alleviate a deadlock problem, but which creates problems of its own (the
 * timeout stops working correctly).  Those functions also require that all
 * signal handlers call a function which writes to the pipe -- a task which is
 * difficult to do in the smbwrapper environment.
 */


#include <sys/select.h>
#include <errno.h>
#include <stdio.h>

int sys_select(int maxfd, fd_set *readfds, fd_set *writefds, fd_set *errorfds, struct timeval *tval)
{
        int ret;
	fd_set *readfds2, readfds_buf;

	/* If readfds is NULL we need to provide our own set. */
	if (readfds) {
		readfds2 = readfds;
	} else {
		readfds2 = &readfds_buf;
		FD_ZERO(readfds2);
	}

	errno = 0;
	ret = select(maxfd,readfds2,writefds,errorfds,tval);

	if (ret <= 0) {
		FD_ZERO(readfds2);
		if (writefds)
			FD_ZERO(writefds);
		if (errorfds)
			FD_ZERO(errorfds);
	}

	return ret;
}

/*******************************************************************
 Similar to sys_select() but catch EINTR and continue.
 This is what sys_select() used to do in Samba.
********************************************************************/

int sys_select_intr(int maxfd, fd_set *readfds, fd_set *writefds, fd_set *errorfds, struct timeval *tval)
{
	int ret;
	fd_set *readfds2, readfds_buf, *writefds2, writefds_buf, *errorfds2, errorfds_buf;
	struct timeval tval2, *ptval, end_time, now_time;
        extern void GetTimeOfDay(struct timeval *tval);

	readfds2 = (readfds ? &readfds_buf : NULL);
	writefds2 = (writefds ? &writefds_buf : NULL);
	errorfds2 = (errorfds ? &errorfds_buf : NULL);
        if (tval) {
                GetTimeOfDay(&end_time);
                end_time.tv_sec += tval->tv_sec;
                end_time.tv_usec += tval->tv_usec;
                end_time.tv_sec += end_time.tv_usec / 1000000;
                end_time.tv_usec %= 1000000;
                ptval = &tval2;
        } else {
                ptval = NULL;
        }

	do {
		if (readfds)
			readfds_buf = *readfds;
		if (writefds)
			writefds_buf = *writefds;
		if (errorfds)
			errorfds_buf = *errorfds;
		if (tval) {
                        GetTimeOfDay(&now_time);
                        tval2.tv_sec = end_time.tv_sec - now_time.tv_sec;
			tval2.tv_usec = end_time.tv_usec - now_time.tv_usec;
                        if ((signed long) tval2.tv_usec < 0) {
                                tval2.tv_usec += 1000000;
                                tval2.tv_sec--;
                        }
                        if ((signed long) tval2.tv_sec < 0) {
                                ret = 0;
                                break;          /* time has already elapsed */
                        }
                }

		ret = sys_select(maxfd, readfds2, writefds2, errorfds2, ptval);
	} while (ret == -1 && errno == EINTR);

	if (readfds)
		*readfds = readfds_buf;
	if (writefds)
		*writefds = writefds_buf;
	if (errorfds)
		*errorfds = errorfds_buf;

	return ret;
}
