/*
   Unix SMB/CIFS implementation.
   async lookupsid
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

struct wb_lookupsid_state {
	struct tevent_context *ev;
	struct winbindd_domain *lookup_domain;
	struct dom_sid sid;
	enum lsa_SidType type;
	const char *domname;
	const char *name;
};

static void wb_lookupsid_done(struct tevent_req *subreq);

struct tevent_req *wb_lookupsid_send(TALLOC_CTX *mem_ctx,
				     struct tevent_context *ev,
				     const struct dom_sid *sid)
{
	struct tevent_req *req, *subreq;
	struct wb_lookupsid_state *state;

	req = tevent_req_create(mem_ctx, &state, struct wb_lookupsid_state);
	if (req == NULL) {
		return NULL;
	}
	sid_copy(&state->sid, sid);
	state->ev = ev;

	state->lookup_domain = find_lookup_domain_from_sid(sid);
	if (state->lookup_domain == NULL) {
		DEBUG(5, ("Could not find domain for sid %s\n",
			  sid_string_dbg(sid)));
		tevent_req_nterror(req, NT_STATUS_NONE_MAPPED);
		return tevent_req_post(req, ev);
	}

	subreq = dcerpc_wbint_LookupSid_send(
		state, ev, dom_child_handle(state->lookup_domain),
		&state->sid, &state->type, &state->domname, &state->name);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, wb_lookupsid_done, req);
	return req;
}

static void wb_lookupsid_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct wb_lookupsid_state *state = tevent_req_data(
		req, struct wb_lookupsid_state);
	struct winbindd_domain *forest_root;
	NTSTATUS status, result;

	status = dcerpc_wbint_LookupSid_recv(subreq, state, &result);
	TALLOC_FREE(subreq);
	if (tevent_req_nterror(req, status)) {
		return;
	}
	if (NT_STATUS_IS_OK(result)) {
		tevent_req_done(req);
		return;
	}

	/*
	 * Let's try the forest root
	 */
	forest_root = find_root_domain();
	if ((forest_root == NULL) || (forest_root == state->lookup_domain)) {
		tevent_req_nterror(req, result);
		return;
	}
	state->lookup_domain = forest_root;

	subreq = dcerpc_wbint_LookupSid_send(
		state, state->ev, dom_child_handle(state->lookup_domain),
		&state->sid, &state->type, &state->domname, &state->name);
	if (tevent_req_nomem(subreq, req)) {
		return;
	}
	tevent_req_set_callback(subreq, wb_lookupsid_done, req);
}

NTSTATUS wb_lookupsid_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx,
			   enum lsa_SidType *type, const char **domain,
			   const char **name)
{
	struct wb_lookupsid_state *state = tevent_req_data(
		req, struct wb_lookupsid_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		return status;
	}
	*type = state->type;
	*domain = talloc_move(mem_ctx, &state->domname);
	*name = talloc_move(mem_ctx, &state->name);
	return NT_STATUS_OK;
}
