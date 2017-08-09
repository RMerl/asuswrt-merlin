/*
   Unix SMB/CIFS implementation.
   async sid2gid
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
#include "../libcli/security/security.h"

struct wb_sid2gid_state {
	struct tevent_context *ev;
	struct dom_sid sid;
	char *dom_name;
	uint64 gid64;
	gid_t gid;
};

static void wb_sid2gid_lookup_done(struct tevent_req *subreq);
static void wb_sid2gid_done(struct tevent_req *subreq);


struct tevent_req *wb_sid2gid_send(TALLOC_CTX *mem_ctx,
				   struct tevent_context *ev,
				   const struct dom_sid *sid)
{
	struct tevent_req *req, *subreq;
	struct wb_sid2gid_state *state;
	bool expired;

	req = tevent_req_create(mem_ctx, &state, struct wb_sid2gid_state);
	if (req == NULL) {
		return NULL;
	}
	sid_copy(&state->sid, sid);
	state->ev = ev;

	if (winbindd_use_idmap_cache()
	    && idmap_cache_find_sid2gid(sid, &state->gid, &expired)) {

		DEBUG(10, ("idmap_cache_find_sid2gid found %d%s\n",
			   (int)state->gid, expired ? " (expired)": ""));

		if (!expired || is_domain_offline(find_our_domain())) {
			if (state->gid == -1) {
				tevent_req_nterror(req, NT_STATUS_NONE_MAPPED);
			} else {
				tevent_req_done(req);
			}
			return tevent_req_post(req, ev);
		}
	}

	/*
	 * We need to make sure the sid is of the right type to not flood
	 * idmap with wrong entries
	 */

	subreq = wb_lookupsid_send(state, ev, &state->sid);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, wb_sid2gid_lookup_done, req);
	return req;
}

static void wb_sid2gid_lookup_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct wb_sid2gid_state *state = tevent_req_data(
		req, struct wb_sid2gid_state);
	struct winbindd_domain *domain;
	const char *domname;
	const char *name;
	enum lsa_SidType type;
	NTSTATUS status;
	struct winbindd_child *child;

	status = wb_lookupsid_recv(subreq, talloc_tos(), &type, &domname,
				   &name);
	if (tevent_req_nterror(req, status)) {
		return;
	}

	if ((type != SID_NAME_DOM_GRP) && (type != SID_NAME_ALIAS)
	    && (type != SID_NAME_WKN_GRP)) {
		DEBUG(5, ("Sid %s is not a group.\n",
			  sid_string_dbg(&state->sid)));
		/*
		 * We have to set the cache ourselves here, the child
		 * which is normally responsible was not queried yet.
		 */
		idmap_cache_set_sid2gid(&state->sid, -1);
		tevent_req_nterror(req, NT_STATUS_INVALID_SID);
		return;
	}

	domain = find_domain_from_sid_noinit(&state->sid);

	/*
	 * TODO: Issue a gettrustinfo here in case we don't have "domain" yet?
	 */

	if ((domain != NULL) && domain->have_idmap_config) {
		state->dom_name = domain->name;
	} else {
		state->dom_name = NULL;
	}

	child = idmap_child();

	subreq = dcerpc_wbint_Sid2Gid_send(state, state->ev, child->binding_handle,
					   state->dom_name, &state->sid,
					   &state->gid64);
	if (tevent_req_nomem(subreq, req)) {
		return;
	}
	tevent_req_set_callback(subreq, wb_sid2gid_done, req);
}

static void wb_sid2gid_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct wb_sid2gid_state *state = tevent_req_data(
		req, struct wb_sid2gid_state);
	NTSTATUS status, result;

	status = dcerpc_wbint_Sid2Gid_recv(subreq, state, &result);
	TALLOC_FREE(subreq);
	if (any_nt_status_not_ok(status, result, &status)) {
		tevent_req_nterror(req, status);
		return;
	}

	state->gid = state->gid64;
	tevent_req_done(req);
}

NTSTATUS wb_sid2gid_recv(struct tevent_req *req, gid_t *gid)
{
	struct wb_sid2gid_state *state = tevent_req_data(
		req, struct wb_sid2gid_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		return status;
	}
	*gid = state->gid;
	return NT_STATUS_OK;
}
