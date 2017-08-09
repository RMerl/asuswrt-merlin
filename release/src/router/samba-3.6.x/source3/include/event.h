/*
   Unix SMB/CIFS implementation.
   event handling
   Copyright (C) Andrew Tridgell 1992-1998
   Copyright (C) Volker Lendecke 2005

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#define TEVENT_COMPAT_DEFINES
#include <tevent.h>

#undef event_context_init
#define event_context_init(mem_ctx) s3_tevent_context_init(mem_ctx)

/* The following definitions come from lib/events.c  */
struct pollfd;
struct timeval *get_timed_events_timeout(struct event_context *event_ctx,
					 struct timeval *to_ret);
void dump_event_list(struct event_context *event_ctx);
struct tevent_context *s3_tevent_context_init(TALLOC_CTX *mem_ctx);

bool event_add_to_poll_args(struct tevent_context *ev, TALLOC_CTX *mem_ctx,
			    struct pollfd **pfds, int *num_pfds,
			    int *ptimeout);
bool run_events_poll(struct tevent_context *ev, int pollrtn,
		     struct pollfd *pfds, int num_pfds);
