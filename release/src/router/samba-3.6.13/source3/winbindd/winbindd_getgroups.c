/*
   Unix SMB/CIFS implementation.
   async implementation of WINBINDD_GETGROUPS
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
#include "passdb/lookup_sid.h" /* only for LOOKUP_NAME_NO_NSS flag */

struct winbindd_getgroups_state {
	struct tevent_context *ev;
	fstring domname;
	fstring username;
	struct dom_sid sid;
	enum lsa_SidType type;
	int num_sids;
	struct dom_sid *sids;
	int next_sid;
	int num_gids;
	gid_t *gids;
};

static void winbindd_getgroups_lookupname_done(struct tevent_req *subreq);
static void winbindd_getgroups_gettoken_done(struct tevent_req *subreq);
static void winbindd_getgroups_sid2gid_done(struct tevent_req *subreq);

struct tevent_req *winbindd_getgroups_send(TALLOC_CTX *mem_ctx,
					   struct tevent_context *ev,
					   struct winbindd_cli_state *cli,
					   struct winbindd_request *request)
{
	struct tevent_req *req, *subreq;
	struct winbindd_getgroups_state *state;
	char *domuser, *mapped_user;
	NTSTATUS status;

	req = tevent_req_create(mem_ctx, &state,
				struct winbindd_getgroups_state);
	if (req == NULL) {
		return NULL;
	}
	state->ev = ev;

	/* Ensure null termination */
	request->data.username[sizeof(request->data.username)-1]='\0';

	DEBUG(3, ("getgroups %s\n", request->data.username));

	domuser = request->data.username;

	status = normalize_name_unmap(state, domuser, &mapped_user);

	if (NT_STATUS_IS_OK(status)
	    || NT_STATUS_EQUAL(status, NT_STATUS_FILE_RENAMED)) {
		/* normalize_name_unmapped did something */
		domuser = mapped_user;
	}

	if (!parse_domain_user(domuser, state->domname, state->username)) {
		DEBUG(5, ("Could not parse domain user: %s\n", domuser));
		tevent_req_nterror(req, NT_STATUS_INVALID_PARAMETER);
		return tevent_req_post(req, ev);
	}

	subreq = wb_lookupname_send(state, ev, state->domname, state->username,
				    LOOKUP_NAME_NO_NSS);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, winbindd_getgroups_lookupname_done,
				req);
	return req;
}

static void winbindd_getgroups_lookupname_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct winbindd_getgroups_state *state = tevent_req_data(
		req, struct winbindd_getgroups_state);
	NTSTATUS status;

	status = wb_lookupname_recv(subreq, &state->sid, &state->type);
	TALLOC_FREE(subreq);
	if (tevent_req_nterror(req, status)) {
		return;
	}

	subreq = wb_gettoken_send(state, state->ev, &state->sid);
	if (tevent_req_nomem(subreq, req)) {
		return;
	}
	tevent_req_set_callback(subreq, winbindd_getgroups_gettoken_done, req);
}

static void winbindd_getgroups_gettoken_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct winbindd_getgroups_state *state = tevent_req_data(
		req, struct winbindd_getgroups_state);
	NTSTATUS status;

	status = wb_gettoken_recv(subreq, state, &state->num_sids,
				  &state->sids);
	TALLOC_FREE(subreq);
	if (tevent_req_nterror(req, status)) {
		return;
	}

	/*
	 * Convert the group SIDs to gids. state->sids[0] contains the user
	 * sid, so start at index 1.
	 */

	state->gids = talloc_array(state, gid_t, state->num_sids-1);
	if (tevent_req_nomem(state->gids, req)) {
		return;
	}
	state->num_gids = 0;
	state->next_sid = 1;

	subreq = wb_sid2gid_send(state, state->ev,
				 &state->sids[state->next_sid]);
	if (tevent_req_nomem(subreq, req)) {
		return;
	}
	tevent_req_set_callback(subreq, winbindd_getgroups_sid2gid_done, req);
}

static void winbindd_getgroups_sid2gid_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct winbindd_getgroups_state *state = tevent_req_data(
		req, struct winbindd_getgroups_state);
	NTSTATUS status;

	status = wb_sid2gid_recv(subreq, &state->gids[state->num_gids]);
	TALLOC_FREE(subreq);

	/*
	 * In case of failure, just continue with the next gid
	 */
	if (NT_STATUS_IS_OK(status)) {
		state->num_gids += 1;
	}
	state->next_sid += 1;

	if (state->next_sid >= state->num_sids) {
		tevent_req_done(req);
		return;
	}

	subreq = wb_sid2gid_send(state, state->ev,
				 &state->sids[state->next_sid]);
	if (tevent_req_nomem(subreq, req)) {
		return;
	}
	tevent_req_set_callback(subreq, winbindd_getgroups_sid2gid_done, req);
}

NTSTATUS winbindd_getgroups_recv(struct tevent_req *req,
				 struct winbindd_response *response)
{
	struct winbindd_getgroups_state *state = tevent_req_data(
		req, struct winbindd_getgroups_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		DEBUG(5, ("Could not convert sid %s: %s\n",
			  sid_string_dbg(&state->sid), nt_errstr(status)));
		return status;
	}

	response->data.num_entries = state->num_gids;

	if (state->num_gids > 0) {
		response->extra_data.data = talloc_move(response,
							&state->gids);
		response->length += state->num_gids * sizeof(gid_t);
	}
	return NT_STATUS_OK;
}
