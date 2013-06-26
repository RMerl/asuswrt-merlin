/*
   Unix SMB/CIFS implementation.
   async lookupusergroups
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
#include "../libcli/security/security.h"

struct wb_lookupusergroups_state {
	struct tevent_context *ev;
	struct dom_sid sid;
	struct wbint_SidArray sids;
};

static void wb_lookupusergroups_done(struct tevent_req *subreq);

struct tevent_req *wb_lookupusergroups_send(TALLOC_CTX *mem_ctx,
					    struct tevent_context *ev,
					    struct winbindd_domain *domain,
					    const struct dom_sid *sid)
{
	struct tevent_req *req, *subreq;
	struct wb_lookupusergroups_state *state;

	req = tevent_req_create(mem_ctx, &state,
				struct wb_lookupusergroups_state);
	if (req == NULL) {
		return NULL;
	}
	sid_copy(&state->sid, sid);

	subreq = dcerpc_wbint_LookupUserGroups_send(
		state, ev, dom_child_handle(domain), &state->sid, &state->sids);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, wb_lookupusergroups_done, req);
	return req;
}

static void wb_lookupusergroups_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct wb_lookupusergroups_state *state = tevent_req_data(
		req, struct wb_lookupusergroups_state);
	NTSTATUS status, result;

	status = dcerpc_wbint_LookupUserGroups_recv(subreq, state, &result);
	TALLOC_FREE(subreq);
	if (any_nt_status_not_ok(status, result, &status)) {
		tevent_req_nterror(req, status);
		return;
	}
	tevent_req_done(req);
}

NTSTATUS wb_lookupusergroups_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx,
				  int *num_sids, struct dom_sid **sids)
{
	struct wb_lookupusergroups_state *state = tevent_req_data(
		req, struct wb_lookupusergroups_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		return status;
	}
	*num_sids = state->sids.num_sids;
	*sids = talloc_move(mem_ctx, &state->sids.sids);
	return NT_STATUS_OK;
}
