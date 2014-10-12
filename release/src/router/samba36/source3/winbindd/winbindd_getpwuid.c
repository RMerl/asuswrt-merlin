/*
   Unix SMB/CIFS implementation.
   async implementation of WINBINDD_GETPWUID
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

struct winbindd_getpwuid_state {
	struct tevent_context *ev;
	struct dom_sid sid;
	struct winbindd_pw pw;
};

static void winbindd_getpwuid_uid2sid_done(struct tevent_req *subreq);
static void winbindd_getpwuid_done(struct tevent_req *subreq);

struct tevent_req *winbindd_getpwuid_send(TALLOC_CTX *mem_ctx,
					  struct tevent_context *ev,
					  struct winbindd_cli_state *cli,
					  struct winbindd_request *request)
{
	struct tevent_req *req, *subreq;
	struct winbindd_getpwuid_state *state;

	req = tevent_req_create(mem_ctx, &state,
				struct winbindd_getpwuid_state);
	if (req == NULL) {
		return NULL;
	}
	state->ev = ev;

	DEBUG(3, ("getpwuid %d\n", (int)request->data.uid));

	subreq = wb_uid2sid_send(state, ev, request->data.uid);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, winbindd_getpwuid_uid2sid_done,
				req);
	return req;
}

static void winbindd_getpwuid_uid2sid_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct winbindd_getpwuid_state *state = tevent_req_data(
		req, struct winbindd_getpwuid_state);
	NTSTATUS status;

	status = wb_uid2sid_recv(subreq, &state->sid);
	TALLOC_FREE(subreq);
	if (tevent_req_nterror(req, status)) {
		return;
	}

	subreq = wb_getpwsid_send(state, state->ev, &state->sid, &state->pw);
	if (tevent_req_nomem(subreq, req)) {
		return;
	}
	tevent_req_set_callback(subreq, winbindd_getpwuid_done, req);
}

static void winbindd_getpwuid_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	NTSTATUS status;

	status = wb_getpwsid_recv(subreq);
	TALLOC_FREE(subreq);
	if (tevent_req_nterror(req, status)) {
		return;
	}
	tevent_req_done(req);
}

NTSTATUS winbindd_getpwuid_recv(struct tevent_req *req,
				struct winbindd_response *response)
{
	struct winbindd_getpwuid_state *state = tevent_req_data(
		req, struct winbindd_getpwuid_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		DEBUG(5, ("Could not convert sid %s: %s\n",
			  sid_string_dbg(&state->sid), nt_errstr(status)));
		return status;
	}
	response->data.pw = state->pw;
	return NT_STATUS_OK;
}
