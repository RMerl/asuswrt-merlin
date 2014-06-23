/*****************************************************************************
  Copyright (c) 2006 EMC Corporation.

  This program is free software; you can redistribute it and/or modify it 
  under the terms of the GNU General Public License as published by the Free 
  Software Foundation; either version 2 of the License, or (at your option) 
  any later version.
  
  This program is distributed in the hope that it will be useful, but WITHOUT 
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or 
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for 
  more details.
  
  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc., 59 
  Temple Place - Suite 330, Boston, MA  02111-1307, USA.
  
  The full GNU General Public License is included in this distribution in the
  file called LICENSE.

  Authors: Srinivas Aji <Aji_Srinivas@emc.com>

******************************************************************************/

#include "epoll_loop.h"

#include <sys/epoll.h>
#include <errno.h>
#include <sys/time.h>
#include <time.h>
#include <stdio.h>
#include <unistd.h>

#include "bridge_ctl.h"

// globals
int epoll_fd = -1;
struct timeval nexttimeout;

int init_epoll(void)
{
	int r = epoll_create(128);
	if (r < 0) {
		fprintf(stderr, "epoll_create failed: %m\n");
		return -1;
	}
	epoll_fd = r;
	return 0;
}

int add_epoll(struct epoll_event_handler *h)
{
	struct epoll_event ev = {
		.events = EPOLLIN,
		.data.ptr = h,
	};
	h->ref_ev = NULL;
	int r = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, h->fd, &ev);
	if (r < 0) {
		fprintf(stderr, "epoll_ctl_add: %m\n");
		return -1;
	}
	return 0;
}

int remove_epoll(struct epoll_event_handler *h)
{
	int r = epoll_ctl(epoll_fd, EPOLL_CTL_DEL, h->fd, NULL);
	if (r < 0) {
		fprintf(stderr, "epoll_ctl_del: %m\n");
		return -1;
	}
	if (h->ref_ev && h->ref_ev->data.ptr == h) {
		h->ref_ev->data.ptr = NULL;
		h->ref_ev = NULL;
	}
	return 0;
}

void clear_epoll(void)
{
	if (epoll_fd >= 0)
		close(epoll_fd);
}

int time_diff(struct timeval *second, struct timeval *first)
{
	return (second->tv_sec - first->tv_sec) * 1000
	    + (second->tv_usec - first->tv_usec) / 1000;
}

void run_timeouts(void)
{
	bridge_one_second();
	nexttimeout.tv_sec++;
}

int epoll_main_loop(void)
{
	gettimeofday(&nexttimeout, NULL);
	nexttimeout.tv_sec++;
#define EV_SIZE 8
	struct epoll_event ev[EV_SIZE];

	while (1) {
		int r, i;
		int timeout;

		struct timeval tv;
		gettimeofday(&tv, NULL);
		timeout = time_diff(&nexttimeout, &tv);
		if (timeout < 0) {
			run_timeouts();
			timeout = 0;
		}

		r = epoll_wait(epoll_fd, ev, EV_SIZE, timeout);
		if (r < 0 && errno != EINTR) {
			fprintf(stderr, "epoll_wait: %m\n");
			return -1;
		}
		for (i = 0; i < r; i++) {
			struct epoll_event_handler *p = ev[i].data.ptr;
			if (p != NULL)
				p->ref_ev = &ev[i];
		}
		for (i = 0; i < r; i++) {
			struct epoll_event_handler *p = ev[i].data.ptr;
			if (p && p->handler)
				p->handler(ev[i].events, p);
		}
		for (i = 0; i < r; i++) {
			struct epoll_event_handler *p = ev[i].data.ptr;
			if (p != NULL)
				p->ref_ev = NULL;
		}
	}

	return 0;
}
