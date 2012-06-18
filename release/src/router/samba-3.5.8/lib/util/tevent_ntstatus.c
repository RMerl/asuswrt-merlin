/*
   Unix SMB/CIFS implementation.
   Wrap unix errno around tevent_req
   Copyright (C) Volker Lendecke 2009

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
#include "tevent_ntstatus.h"

bool tevent_req_nterror(struct tevent_req *req,	NTSTATUS status)
{
	return tevent_req_error(req, NT_STATUS_V(status));
}

bool tevent_req_is_nterror(struct tevent_req *req, NTSTATUS *status)
{
	enum tevent_req_state state;
	uint64_t err;

	if (!tevent_req_is_error(req, &state, &err)) {
		return false;
	}
	switch (state) {
	case TEVENT_REQ_TIMED_OUT:
		*status = NT_STATUS_IO_TIMEOUT;
		break;
	case TEVENT_REQ_NO_MEMORY:
		*status = NT_STATUS_NO_MEMORY;
		break;
	case TEVENT_REQ_USER_ERROR:
		*status = NT_STATUS(err);
		break;
	default:
		*status = NT_STATUS_INTERNAL_ERROR;
		break;
	}
	return true;
}

NTSTATUS tevent_req_simple_recv_ntstatus(struct tevent_req *req)
{
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		return status;
	}
	return NT_STATUS_OK;
}
