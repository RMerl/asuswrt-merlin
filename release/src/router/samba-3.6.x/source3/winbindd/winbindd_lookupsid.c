/*
   Unix SMB/CIFS implementation.
   async implementation of WINBINDD_LOOKUPSID
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

struct winbindd_lookupsid_state {
	struct dom_sid sid;
	enum lsa_SidType type;
	const char *domname;
	const char *name;
};

static void winbindd_lookupsid_done(struct tevent_req *subreq);

struct tevent_req *winbindd_lookupsid_send(TALLOC_CTX *mem_ctx,
					  struct tevent_context *ev,
					   struct winbindd_cli_state *cli,
					   struct winbindd_request *request)
{
	struct tevent_req *req, *subreq;
	struct winbindd_lookupsid_state *state;

	req = tevent_req_create(mem_ctx, &state,
				struct winbindd_lookupsid_state);
	if (req == NULL) {
		return NULL;
	}

	/* Ensure null termination */
	request->data.sid[sizeof(request->data.sid)-1]='\0';

	DEBUG(3, ("lookupsid %s\n", request->data.sid));

	if (!string_to_sid(&state->sid, request->data.sid)) {
		DEBUG(5, ("%s not a SID\n", request->data.sid));
		tevent_req_nterror(req, NT_STATUS_INVALID_PARAMETER);
		return tevent_req_post(req, ev);
	}

	subreq = wb_lookupsid_send(state, ev, &state->sid);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, winbindd_lookupsid_done, req);
	return req;
}

static void winbindd_lookupsid_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct winbindd_lookupsid_state *state = tevent_req_data(
		req, struct winbindd_lookupsid_state);
	NTSTATUS status;

	status = wb_lookupsid_recv(subreq, state, &state->type,
				   &state->domname, &state->name);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}
	tevent_req_done(req);
}

NTSTATUS winbindd_lookupsid_recv(struct tevent_req *req,
				 struct winbindd_response *response)
{
	struct winbindd_lookupsid_state *state = tevent_req_data(
		req, struct winbindd_lookupsid_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		DEBUG(5, ("Could not lookup sid %s: %s\n",
			  sid_string_dbg(&state->sid), nt_errstr(status)));
		return status;
	}

	fstrcpy(response->data.name.dom_name, state->domname);
	fstrcpy(response->data.name.name, state->name);
	response->data.name.type = state->type;
	return NT_STATUS_OK;
}
