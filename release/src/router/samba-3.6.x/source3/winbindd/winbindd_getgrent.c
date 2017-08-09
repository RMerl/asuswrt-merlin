/*
   Unix SMB/CIFS implementation.
   async implementation of WINBINDD_GETGRENT
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

struct winbindd_getgrent_state {
	struct tevent_context *ev;
	struct winbindd_cli_state *cli;
	int max_groups;
	int num_groups;
	struct winbindd_gr *groups;
	struct talloc_dict **members;
};

static void winbindd_getgrent_done(struct tevent_req *subreq);

struct tevent_req *winbindd_getgrent_send(TALLOC_CTX *mem_ctx,
					  struct tevent_context *ev,
					  struct winbindd_cli_state *cli,
					  struct winbindd_request *request)
{
	struct tevent_req *req, *subreq;
	struct winbindd_getgrent_state *state;

	req = tevent_req_create(mem_ctx, &state,
				struct winbindd_getgrent_state);
	if (req == NULL) {
		return NULL;
	}
	state->ev = ev;
	state->num_groups = 0;
	state->cli = cli;

	DEBUG(3, ("[%5lu]: getgrent\n", (unsigned long)cli->pid));

	if (!lp_winbind_enum_groups()) {
		tevent_req_nterror(req, NT_STATUS_NO_MORE_ENTRIES);
		return tevent_req_post(req, ev);
	}

	if (cli->grent_state == NULL) {
		tevent_req_nterror(req, NT_STATUS_NO_MORE_ENTRIES);
		return tevent_req_post(req, ev);
	}

	state->max_groups = MIN(500, request->data.num_entries);
	if (state->max_groups == 0) {
		tevent_req_nterror(req, NT_STATUS_INVALID_PARAMETER);
		return tevent_req_post(req, ev);
	}

	state->groups = talloc_zero_array(state, struct winbindd_gr,
					  state->max_groups);
	if (tevent_req_nomem(state->groups, req)) {
		return tevent_req_post(req, ev);
	}

	state->members = talloc_array(state, struct talloc_dict *,
				      state->max_groups);
	if (tevent_req_nomem(state->members, req)) {
		TALLOC_FREE(state->groups);
		return tevent_req_post(req, ev);
	}

	subreq = wb_next_grent_send(state, ev, lp_winbind_expand_groups(),
				    cli->grent_state,
				    &state->groups[state->num_groups]);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, winbindd_getgrent_done, req);
	return req;
}

static void winbindd_getgrent_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct winbindd_getgrent_state *state = tevent_req_data(
		req, struct winbindd_getgrent_state);
	NTSTATUS status;

	status = wb_next_grent_recv(subreq, state,
				    &state->members[state->num_groups]);
	TALLOC_FREE(subreq);
	if (NT_STATUS_EQUAL(status, NT_STATUS_NO_MORE_ENTRIES)) {
		DEBUG(10, ("winbindd_getgrent_done: done with %d groups\n",
			   (int)state->num_groups));
		TALLOC_FREE(state->cli->grent_state);
		tevent_req_done(req);
		return;
	}
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}
	state->num_groups += 1;
	if (state->num_groups >= state->max_groups) {
		DEBUG(10, ("winbindd_getgrent_done: Got enough groups: %d\n",
			   (int)state->num_groups));
		tevent_req_done(req);
		return;
	}
	if (state->cli->grent_state == NULL) {
		DEBUG(10, ("winbindd_getgrent_done: endgrent called in "
			   "between\n"));
		tevent_req_nterror(req, NT_STATUS_INVALID_PARAMETER);
		return;
	}
	subreq = wb_next_grent_send(state, state->ev,
				    lp_winbind_expand_groups(),
				    state->cli->grent_state,
				    &state->groups[state->num_groups]);
	if (tevent_req_nomem(subreq, req)) {
		return;
	}
	tevent_req_set_callback(subreq, winbindd_getgrent_done, req);
}

NTSTATUS winbindd_getgrent_recv(struct tevent_req *req,
				struct winbindd_response *response)
{
	struct winbindd_getgrent_state *state = tevent_req_data(
		req, struct winbindd_getgrent_state);
	NTSTATUS status;
	char **memberstrings;
	char *result;
	size_t base_memberofs, total_memberlen;
	int i;

	if (tevent_req_is_nterror(req, &status)) {
		DEBUG(5, ("getgrent failed: %s\n", nt_errstr(status)));
		return status;
	}

	if (state->num_groups == 0) {
		return NT_STATUS_NO_MORE_ENTRIES;
	}

	memberstrings = talloc_array(talloc_tos(), char *, state->num_groups);
	if (memberstrings == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	total_memberlen = 0;

	for (i=0; i<state->num_groups; i++) {
		int num_members;

		status = winbindd_print_groupmembers(
			state->members[i], memberstrings, &num_members,
			&memberstrings[i]);

		if (!NT_STATUS_IS_OK(status)) {
			TALLOC_FREE(memberstrings);
			return status;
		}
		TALLOC_FREE(state->members[i]);

		state->groups[i].num_gr_mem = num_members;
		state->groups[i].gr_mem_ofs = total_memberlen;

		total_memberlen += talloc_get_size(memberstrings[i]);
	}

	base_memberofs = state->num_groups * sizeof(struct winbindd_gr);

	result = talloc_realloc(state, state->groups, char,
				base_memberofs + total_memberlen);
	if (result == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	state->groups = (struct winbindd_gr *)result;

	for (i=0; i<state->num_groups; i++) {
		memcpy(result + base_memberofs + state->groups[i].gr_mem_ofs,
		       memberstrings[i], talloc_get_size(memberstrings[i]));
		TALLOC_FREE(memberstrings[i]);
	}

	TALLOC_FREE(memberstrings);

	response->data.num_entries = state->num_groups;
	response->length += talloc_get_size(result);
	response->extra_data.data = talloc_move(response, &result);
	return NT_STATUS_OK;
}
