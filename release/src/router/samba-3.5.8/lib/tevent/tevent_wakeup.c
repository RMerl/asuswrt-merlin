/*
   Unix SMB/CIFS implementation.
   Infrastructure for async requests
   Copyright (C) Volker Lendecke 2008
   Copyright (C) Stefan Metzmacher 2009

     ** NOTE! The following LGPL license applies to the tevent
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
#include "tevent.h"
#include "tevent_internal.h"
#include "tevent_util.h"

struct tevent_wakeup_state {
	struct timeval wakeup_time;
};

struct tevent_req *tevent_wakeup_send(TALLOC_CTX *mem_ctx,
				      struct tevent_context *ev,
				      struct timeval wakeup_time)
{
	struct tevent_req *req;
	struct tevent_wakeup_state *state;

	req = tevent_req_create(mem_ctx, &state,
				struct tevent_wakeup_state);
	if (!req) {
		return NULL;
	}
	state->wakeup_time = wakeup_time;

	if (!tevent_req_set_endtime(req, ev, wakeup_time)) {
		goto post;
	}

	return req;
post:
	return tevent_req_post(req, ev);
}

bool tevent_wakeup_recv(struct tevent_req *req)
{
	enum tevent_req_state state;
	uint64_t error;

	if (tevent_req_is_error(req, &state, &error)) {
		if (state == TEVENT_REQ_TIMED_OUT) {
			return true;
		}
	}

	return false;
}

