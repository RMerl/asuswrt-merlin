/*
   Unix SMB/CIFS implementation.
   async next_grent
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
#include "librpc/gen_ndr/ndr_wbint_c.h"
#include "passdb/machine_sid.h"

struct wb_next_grent_state {
	struct tevent_context *ev;
	int max_nesting;
	struct getgrent_state *gstate;
	struct wbint_Principals next_groups;
	struct winbindd_gr *gr;
	struct talloc_dict *members;
};

static void wb_next_grent_fetch_done(struct tevent_req *subreq);
static void wb_next_grent_getgrsid_done(struct tevent_req *subreq);

struct tevent_req *wb_next_grent_send(TALLOC_CTX *mem_ctx,
				      struct tevent_context *ev,
				      int max_nesting,
				      struct getgrent_state *gstate,
				      struct winbindd_gr *gr)
{
	struct tevent_req *req, *subreq;
	struct wb_next_grent_state *state;

	req = tevent_req_create(mem_ctx, &state, struct wb_next_grent_state);
	if (req == NULL) {
		return NULL;
	}
	state->ev = ev;
	state->gstate = gstate;
	state->gr = gr;
	state->max_nesting = max_nesting;

	if (state->gstate->next_group >= state->gstate->num_groups) {
		TALLOC_FREE(state->gstate->groups);

		if (state->gstate->domain == NULL) {
			state->gstate->domain = domain_list();
		} else {
			state->gstate->domain = state->gstate->domain->next;
		}

		if ((state->gstate->domain != NULL)
		    && sid_check_is_domain(&state->gstate->domain->sid)) {
			state->gstate->domain = state->gstate->domain->next;
		}

		if (state->gstate->domain == NULL) {
			tevent_req_nterror(req, NT_STATUS_NO_MORE_ENTRIES);
			return tevent_req_post(req, ev);
		}
		subreq = dcerpc_wbint_QueryGroupList_send(
			state, state->ev, dom_child_handle(state->gstate->domain),
			&state->next_groups);
		if (tevent_req_nomem(subreq, req)) {
			return tevent_req_post(req, ev);
		}
		tevent_req_set_callback(subreq, wb_next_grent_fetch_done, req);
		return req;
	}

	subreq = wb_getgrsid_send(
		state, state->ev,
		&state->gstate->groups[state->gstate->next_group].sid,
		state->max_nesting);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, wb_next_grent_getgrsid_done, req);
	return req;
}

static void wb_next_grent_fetch_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct wb_next_grent_state *state = tevent_req_data(
		req, struct wb_next_grent_state);
	NTSTATUS status, result;

	status = dcerpc_wbint_QueryGroupList_recv(subreq, state, &result);
	TALLOC_FREE(subreq);
	if (tevent_req_nterror(req, status)) {
		/* Ignore errors here, just log it */
		DEBUG(10, ("query_user_list for domain %s returned %s\n",
			   state->gstate->domain->name,
			   nt_errstr(status)));
		return;
	}
	if (!NT_STATUS_IS_OK(result)) {
		/* Ignore errors here, just log it */
		DEBUG(10, ("query_user_list for domain %s returned %s/%s\n",
			   state->gstate->domain->name,
			   nt_errstr(status), nt_errstr(result)));
		tevent_req_nterror(req, result);
		return;
	}

	state->gstate->num_groups = state->next_groups.num_principals;
	state->gstate->groups = talloc_move(
		state->gstate, &state->next_groups.principals);

	if (state->gstate->num_groups == 0) {
		state->gstate->domain = state->gstate->domain->next;

		if ((state->gstate->domain != NULL)
		    && sid_check_is_domain(&state->gstate->domain->sid)) {
			state->gstate->domain = state->gstate->domain->next;
		}

		if (state->gstate->domain == NULL) {
			tevent_req_nterror(req, NT_STATUS_NO_MORE_ENTRIES);
			return;
		}
		subreq = dcerpc_wbint_QueryGroupList_send(
			state, state->ev, dom_child_handle(state->gstate->domain),
			&state->next_groups);
		if (tevent_req_nomem(subreq, req)) {
			return;
		}
		tevent_req_set_callback(subreq, wb_next_grent_fetch_done, req);
		return;
	}

	state->gstate->next_group = 0;

	subreq = wb_getgrsid_send(
		state, state->ev,
		&state->gstate->groups[state->gstate->next_group].sid,
		state->max_nesting);
	if (tevent_req_nomem(subreq, req)) {
		return;
	}
	tevent_req_set_callback(subreq, wb_next_grent_getgrsid_done, req);
	return;
}

static void wb_next_grent_getgrsid_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct wb_next_grent_state *state = tevent_req_data(
		req, struct wb_next_grent_state);
	const char *domname, *name;
	NTSTATUS status;

	status = wb_getgrsid_recv(subreq, talloc_tos(), &domname, &name,
				  &state->gr->gr_gid, &state->members);
	TALLOC_FREE(subreq);
	if (tevent_req_nterror(req, status)) {
		return;
	}
	if (!fill_grent(talloc_tos(), state->gr, domname, name,
			state->gr->gr_gid)) {
		DEBUG(5, ("fill_grent failed\n"));
		tevent_req_nterror(req, NT_STATUS_NO_MEMORY);
		return;
	}
	state->gstate->next_group += 1;
	tevent_req_done(req);
}

NTSTATUS wb_next_grent_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx,
			    struct talloc_dict **members)
{
	struct wb_next_grent_state *state = tevent_req_data(
		req, struct wb_next_grent_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		return status;
	}
	*members = talloc_move(mem_ctx, &state->members);
	return NT_STATUS_OK;
}
