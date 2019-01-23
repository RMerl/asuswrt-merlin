/*
   Unix SMB/CIFS implementation.
   async implementation of WINBINDD_GETPWENT
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

struct winbindd_getpwent_state {
	struct tevent_context *ev;
	struct winbindd_cli_state *cli;
	int max_users;
	int num_users;
	struct winbindd_pw *users;
};

static void winbindd_getpwent_done(struct tevent_req *subreq);

struct tevent_req *winbindd_getpwent_send(TALLOC_CTX *mem_ctx,
					  struct tevent_context *ev,
					  struct winbindd_cli_state *cli,
					  struct winbindd_request *request)
{
	struct tevent_req *req, *subreq;
	struct winbindd_getpwent_state *state;

	req = tevent_req_create(mem_ctx, &state,
				struct winbindd_getpwent_state);
	if (req == NULL) {
		return NULL;
	}
	state->ev = ev;
	state->num_users = 0;
	state->cli = cli;

	DEBUG(3, ("[%5lu]: getpwent\n", (unsigned long)cli->pid));

	if (!lp_winbind_enum_users()) {
		tevent_req_nterror(req, NT_STATUS_NO_MORE_ENTRIES);
		return tevent_req_post(req, ev);
	}

	if (cli->pwent_state == NULL) {
		tevent_req_nterror(req, NT_STATUS_NO_MORE_ENTRIES);
		return tevent_req_post(req, ev);
	}

	state->max_users = MIN(500, request->data.num_entries);
	if (state->max_users == 0) {
		tevent_req_nterror(req, NT_STATUS_INVALID_PARAMETER);
		return tevent_req_post(req, ev);
	}

	state->users = talloc_zero_array(state, struct winbindd_pw,
					 state->max_users);
	if (tevent_req_nomem(state->users, req)) {
		return tevent_req_post(req, ev);
	}

	subreq = wb_next_pwent_send(state, ev, cli->pwent_state,
				    &state->users[state->num_users]);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, winbindd_getpwent_done, req);
	return req;
}

static void winbindd_getpwent_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct winbindd_getpwent_state *state = tevent_req_data(
		req, struct winbindd_getpwent_state);
	NTSTATUS status;

	status = wb_next_pwent_recv(subreq);
	TALLOC_FREE(subreq);
	if (NT_STATUS_EQUAL(status, NT_STATUS_NO_MORE_ENTRIES)) {
		DEBUG(10, ("winbindd_getpwent_done: done with %d users\n",
			   (int)state->num_users));
		TALLOC_FREE(state->cli->pwent_state);
		tevent_req_done(req);
		return;
	}
	if (tevent_req_nterror(req, status)) {
		return;
	}
	state->num_users += 1;
	if (state->num_users >= state->max_users) {
		DEBUG(10, ("winbindd_getpwent_done: Got enough users: %d\n",
			   (int)state->num_users));
		tevent_req_done(req);
		return;
	}
	if (state->cli->pwent_state == NULL) {
		DEBUG(10, ("winbindd_getpwent_done: endpwent called in "
			   "between\n"));
		tevent_req_nterror(req, NT_STATUS_INVALID_PARAMETER);
		return;
	}
	subreq = wb_next_pwent_send(state, state->ev, state->cli->pwent_state,
				    &state->users[state->num_users]);
	if (tevent_req_nomem(subreq, req)) {
		return;
	}
	tevent_req_set_callback(subreq, winbindd_getpwent_done, req);
}

NTSTATUS winbindd_getpwent_recv(struct tevent_req *req,
				struct winbindd_response *response)
{
	struct winbindd_getpwent_state *state = tevent_req_data(
		req, struct winbindd_getpwent_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		DEBUG(5, ("getpwent failed: %s\n", nt_errstr(status)));
		return status;
	}

	if (state->num_users == 0) {
		return NT_STATUS_NO_MORE_ENTRIES;
	}

	response->data.num_entries = state->num_users;
	response->extra_data.data = talloc_move(response, &state->users);
	response->length += state->num_users * sizeof(struct winbindd_pw);
	return NT_STATUS_OK;
}
