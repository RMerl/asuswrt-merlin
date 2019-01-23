/*
   Unix SMB/CIFS implementation.
   poll.c - poll wrapper

   This file is based on code from libssh (LGPLv2.1+ at the time it
   was downloaded), thus the following copyrights:

   Copyright (c) 2009-2010 by Andreas Schneider <mail@cynapses.org>
   Copyright (c) 2003-2009 by Aris Adamantiadis
   Copyright (c) 2009 Aleksandar Kanchev
   Copyright (C) Volker Lendecke 2011

     ** NOTE! The following LGPL license applies to the replace
     ** library. This does NOT imply that all of Samba is released
     ** under the LGPL

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "replace.h"
#include "system/select.h"
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif


int rep_poll(struct pollfd *fds, nfds_t nfds, int timeout)
{
	fd_set rfds, wfds, efds;
	struct timeval tv, *ptv;
	int max_fd;
	int rc;
	nfds_t i;

	if ((fds == NULL) && (nfds != 0)) {
		errno = EFAULT;
		return -1;
	}

	FD_ZERO(&rfds);
	FD_ZERO(&wfds);
	FD_ZERO(&efds);

	rc = 0;
	max_fd = 0;

	/* compute fd_sets and find largest descriptor */
	for (i = 0; i < nfds; i++) {
		if ((fds[i].fd < 0) || (fds[i].fd >= FD_SETSIZE)) {
			fds[i].revents = POLLNVAL;
			continue;
		}

		if (fds[i].events & (POLLIN | POLLRDNORM)) {
			FD_SET(fds[i].fd, &rfds);
		}
		if (fds[i].events & (POLLOUT | POLLWRNORM | POLLWRBAND)) {
			FD_SET(fds[i].fd, &wfds);
		}
		if (fds[i].events & (POLLPRI | POLLRDBAND)) {
			FD_SET(fds[i].fd, &efds);
		}
		if (fds[i].fd > max_fd &&
		    (fds[i].events & (POLLIN | POLLOUT | POLLPRI |
				      POLLRDNORM | POLLRDBAND |
				      POLLWRNORM | POLLWRBAND))) {
			max_fd = fds[i].fd;
		}
	}

	if (timeout < 0) {
		ptv = NULL;
	} else {
		ptv = &tv;
		if (timeout == 0) {
			tv.tv_sec = 0;
			tv.tv_usec = 0;
		} else {
			tv.tv_sec = timeout / 1000;
			tv.tv_usec = (timeout % 1000) * 1000;
		}
	}

	rc = select(max_fd + 1, &rfds, &wfds, &efds, ptv);
	if (rc < 0) {
		return -1;
	}

	for (rc = 0, i = 0; i < nfds; i++) {
		if ((fds[i].fd < 0) || (fds[i].fd >= FD_SETSIZE)) {
			continue;
		}

		fds[i].revents = 0;

		if (FD_ISSET(fds[i].fd, &rfds)) {
			int err = errno;
			int available = 0;
			int ret;

			/* support for POLLHUP */
			ret = ioctl(fds[i].fd, FIONREAD, &available);
			if ((ret == -1) || (available == 0)) {
				fds[i].revents |= POLLHUP;
			} else {
				fds[i].revents |= fds[i].events
					& (POLLIN | POLLRDNORM);
			}

			errno = err;
		}
		if (FD_ISSET(fds[i].fd, &wfds)) {
			fds[i].revents |= fds[i].events
				& (POLLOUT | POLLWRNORM | POLLWRBAND);
		}
		if (FD_ISSET(fds[i].fd, &efds)) {
			fds[i].revents |= fds[i].events
				& (POLLPRI | POLLRDBAND);
		}
		if (fds[i].revents & ~POLLHUP) {
			rc++;
		}
	}
	return rc;
}
