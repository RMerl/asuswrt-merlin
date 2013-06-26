/*
   Unix SMB/CIFS implementation.
   async implementation of WINBINDD_GETUSERSIDS
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

struct winbindd_getusersids_state {
	struct dom_sid sid;
	int num_sids;
	struct dom_sid *sids;
};

static void winbindd_getusersids_done(struct tevent_req *subreq);

struct tevent_req *winbindd_getusersids_send(TALLOC_CTX *mem_ctx,
					     struct tevent_context *ev,
					     struct winbindd_cli_state *cli,
					     struct winbindd_request *request)
{
	struct tevent_req *req, *subreq;
	struct winbindd_getusersids_state *state;

	req = tevent_req_create(mem_ctx, &state,
				struct winbindd_getusersids_state);
	if (req == NULL) {
		return NULL;
	}

	/* Ensure null termination */
	request->data.sid[sizeof(request->data.sid)-1]='\0';

	DEBUG(3, ("getusersids %s\n", request->data.sid));

	if (!string_to_sid(&state->sid, request->data.sid)) {
		DEBUG(1, ("Could not get convert sid %s from string\n",
			  request->data.sid));
		tevent_req_nterror(req, NT_STATUS_INVALID_PARAMETER);
		return tevent_req_post(req, ev);
	}

	subreq = wb_gettoken_send(state, ev, &state->sid);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, winbindd_getusersids_done, req);
	return req;
}

static void winbindd_getusersids_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct winbindd_getusersids_state *state = tevent_req_data(
		req, struct winbindd_getusersids_state);
	NTSTATUS status;

	status = wb_gettoken_recv(subreq, state, &state->num_sids,
				  &state->sids);
	TALLOC_FREE(subreq);
	if (tevent_req_nterror(req, status)) {
		return;
	}
	tevent_req_done(req);
}

NTSTATUS winbindd_getusersids_recv(struct tevent_req *req,
				   struct winbindd_response *response)
{
	struct winbindd_getusersids_state *state = tevent_req_data(
		req, struct winbindd_getusersids_state);
	NTSTATUS status;
	int i;
	char *result;

	if (tevent_req_is_nterror(req, &status)) {
		DEBUG(5, ("Could not convert sid %s: %s\n",
			  sid_string_dbg(&state->sid), nt_errstr(status)));
		return status;
	}

	result = talloc_strdup(response, "");
	if (result == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	for (i=0; i<state->num_sids; i++) {
		char *str = sid_string_tos(&state->sids[i]);
		if (str == NULL) {
			TALLOC_FREE(result);
			return NT_STATUS_NO_MEMORY;
		}
		result = talloc_asprintf_append_buffer(result, "%s\n", str);
		TALLOC_FREE(str);
		if (result == NULL) {
			return NT_STATUS_NO_MEMORY;
		}
	}

	response->data.num_entries = state->num_sids;
	response->extra_data.data = result;
	response->length += talloc_get_size(result);
	return NT_STATUS_OK;
}
