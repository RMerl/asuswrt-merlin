/*
   Unix SMB/CIFS implementation.
   Wrap win32 errors around tevent_req
   Copyright (C) Kai Blin 2010

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

#include "../replace/replace.h"
#include "tevent_werror.h"

bool _tevent_req_werror(struct tevent_req *req,
			WERROR werror,
			const char *location)
{
	return _tevent_req_error(req, W_ERROR_V(werror),
				 location);
}

bool tevent_req_is_werror(struct tevent_req *req, WERROR *error)
{
	enum tevent_req_state state;
	uint64_t err;

	if (!tevent_req_is_error(req, &state, &err)) {
		return false;
	}
	switch (state) {
	case TEVENT_REQ_TIMED_OUT:
		*error = WERR_TIMEOUT;
		break;
	case TEVENT_REQ_NO_MEMORY:
		*error = WERR_NOMEM;
		break;
	case TEVENT_REQ_USER_ERROR:
		*error = W_ERROR(err);
		break;
	default:
		*error = WERR_INTERNAL_ERROR;
		break;
	}
	return true;
}

WERROR tevent_req_simple_recv_werror(struct tevent_req *req)
{
	WERROR werror;

	if (tevent_req_is_werror(req, &werror)) {
		tevent_req_received(req);
		return werror;
	}
	tevent_req_received(req);
	return WERR_OK;
}

void tevent_req_simple_finish_werror(struct tevent_req *subreq,
				     WERROR subreq_error)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);

	TALLOC_FREE(subreq);

	if (!W_ERROR_IS_OK(subreq_error)) {
		tevent_req_werror(req, subreq_error);
		return;
	}
	tevent_req_done(req);
}
