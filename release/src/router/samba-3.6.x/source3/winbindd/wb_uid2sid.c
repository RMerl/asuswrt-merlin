/*
   Unix SMB/CIFS implementation.
   async uid2sid
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
#include "idmap_cache.h"
#include "idmap.h"
#include "../libcli/security/security.h"

struct wb_uid2sid_state {
	struct tevent_context *ev;
	char *dom_name;
	struct dom_sid sid;
};

static void wb_uid2sid_done(struct tevent_req *subreq);

struct tevent_req *wb_uid2sid_send(TALLOC_CTX *mem_ctx,
				   struct tevent_context *ev,
				   uid_t uid)
{
	struct tevent_req *req, *subreq;
	struct wb_uid2sid_state *state;
	struct winbindd_domain *domain;
	struct winbindd_child *child;
	bool expired;

	req = tevent_req_create(mem_ctx, &state, struct wb_uid2sid_state);
	if (req == NULL) {
		return NULL;
	}

	if (winbindd_use_idmap_cache()
	    && idmap_cache_find_uid2sid(uid, &state->sid, &expired)) {

		DEBUG(10, ("idmap_cache_find_uid2sid found %d%s\n",
			   (int)uid, expired ? " (expired)": ""));

		if (!expired || idmap_is_offline()) {
			if (is_null_sid(&state->sid)) {
				tevent_req_nterror(req,
						   NT_STATUS_NONE_MAPPED);
			} else {
				tevent_req_done(req);
			}
			return tevent_req_post(req, ev);
		}
	}

	state->dom_name = NULL;

	for (domain = domain_list(); domain != NULL; domain = domain->next) {
		if (domain->have_idmap_config
		    && (uid >= domain->id_range_low)
		    && (uid <= domain->id_range_high)) {
			state->dom_name = domain->name;
			break;
		}
	}

	child = idmap_child();

	subreq = dcerpc_wbint_Uid2Sid_send(
		state, ev, child->binding_handle, state->dom_name,
		uid, &state->sid);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, wb_uid2sid_done, req);
	return req;
}

static void wb_uid2sid_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct wb_uid2sid_state *state = tevent_req_data(
		req, struct wb_uid2sid_state);
	NTSTATUS status, result;

	status = dcerpc_wbint_Uid2Sid_recv(subreq, state, &result);
	TALLOC_FREE(subreq);
	if (any_nt_status_not_ok(status, result, &status)) {
		tevent_req_nterror(req, status);
		return;
	}
	tevent_req_done(req);
}

NTSTATUS wb_uid2sid_recv(struct tevent_req *req, struct dom_sid *sid)
{
	struct wb_uid2sid_state *state = tevent_req_data(
		req, struct wb_uid2sid_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		return status;
	}
	sid_copy(sid, &state->sid);
	return NT_STATUS_OK;
}
