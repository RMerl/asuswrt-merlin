/*
   Unix SMB/CIFS implementation.
   async implementation of WINBINDD_SID_TO_UID
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

#include "includes.h"
#include "winbindd.h"
#include "../libcli/security/security.h"

struct winbindd_sid_to_uid_state {
	struct dom_sid sid;
	uid_t uid;
};

static void winbindd_sid_to_uid_done(struct tevent_req *subreq);

struct tevent_req *winbindd_sid_to_uid_send(TALLOC_CTX *mem_ctx,
					    struct tevent_context *ev,
					    struct winbindd_cli_state *cli,
					    struct winbindd_request *request)
{
	struct tevent_req *req, *subreq;
	struct winbindd_sid_to_uid_state *state;

	req = tevent_req_create(mem_ctx, &state,
				struct winbindd_sid_to_uid_state);
	if (req == NULL) {
		return NULL;
	}

	/* Ensure null termination */
	request->data.sid[sizeof(request->data.sid)-1]='\0';

	DEBUG(3, ("sid to uid %s\n", request->data.sid));

	if (!string_to_sid(&state->sid, request->data.sid)) {
		DEBUG(1, ("Could not get convert sid %s from string\n",
			  request->data.sid));
		tevent_req_nterror(req, NT_STATUS_INVALID_PARAMETER);
		return tevent_req_post(req, ev);
	}

	subreq = wb_sid2uid_send(state, ev, &state->sid);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, winbindd_sid_to_uid_done, req);
	return req;
}

static void winbindd_sid_to_uid_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct winbindd_sid_to_uid_state *state = tevent_req_data(
		req, struct winbindd_sid_to_uid_state);
	NTSTATUS status;

	status = wb_sid2uid_recv(subreq, &state->uid);
	TALLOC_FREE(subreq);
	if (tevent_req_nterror(req, status)) {
		return;
	}
	tevent_req_done(req);
}

NTSTATUS winbindd_sid_to_uid_recv(struct tevent_req *req,
				  struct winbindd_response *response)
{
	struct winbindd_sid_to_uid_state *state = tevent_req_data(
		req, struct winbindd_sid_to_uid_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		DEBUG(5, ("Could not convert sid %s: %s\n",
			  sid_string_dbg(&state->sid), nt_errstr(status)));
		return status;
	}
	response->data.uid = state->uid;
	return NT_STATUS_OK;
}
