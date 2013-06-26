#ifndef _system_select_h
#define _system_select_h
/* 
   Unix SMB/CIFS implementation.

   select system include wrappers

   Copyright (C) Andrew Tridgell 2004
   
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

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#ifdef HAVE_SYS_EPOLL_H
#include <sys/epoll.h>
#endif

#ifndef SELECT_CAST
#define SELECT_CAST
#endif

#ifdef HAVE_POLL

#include <poll.h>

#else

/* Type used for the number of file descriptors.  */
typedef unsigned long int nfds_t;

/* Data structure describing a polling request.  */
struct pollfd
{
	int fd;		   /* File descriptor to poll.  */
	short int events;  /* Types of events poller cares about.  */
	short int revents; /* Types of events that actually occurred.  */
};

/* Event types that can be polled for.  These bits may be set in `events'
   to indicate the interesting event types; they will appear in `revents'
   to indicate the status of the file descriptor.  */
#define POLLIN		0x001		/* There is data to read.  */
#define POLLPRI		0x002		/* There is urgent data to read.  */
#define POLLOUT		0x004		/* Writing now will not block.  */
#define POLLRDNORM	0x040		/* Normal data may be read.  */
#define POLLRDBAND	0x080		/* Priority data may be read.  */
#define POLLWRNORM	0x100		/* Writing now will not block.  */
#define POLLWRBAND	0x200		/* Priority data may be written.  */
#define POLLERR		0x008		/* Error condition.  */
#define POLLHUP		0x010		/* Hung up.  */
#define POLLNVAL	0x020		/* Invalid polling request.  */

/* define is in "replace.h" */
int rep_poll(struct pollfd *fds, nfds_t nfds, int timeout);

#endif

#endif
