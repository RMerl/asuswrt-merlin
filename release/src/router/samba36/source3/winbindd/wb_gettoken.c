/*
   Unix SMB/CIFS implementation.
   async gettoken
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
#include "passdb/machine_sid.h"

struct wb_gettoken_state {
	struct tevent_context *ev;
	struct dom_sid usersid;
	int num_sids;
	struct dom_sid *sids;
};

static bool wb_add_rids_to_sids(TALLOC_CTX *mem_ctx,
				int *pnum_sids, struct dom_sid **psids,
				const struct dom_sid *domain_sid,
				int num_rids, uint32_t *rids);

static void wb_gettoken_gotgroups(struct tevent_req *subreq);
static void wb_gettoken_gotlocalgroups(struct tevent_req *subreq);
static void wb_gettoken_gotbuiltins(struct tevent_req *subreq);

struct tevent_req *wb_gettoken_send(TALLOC_CTX *mem_ctx,
				    struct tevent_context *ev,
				    const struct dom_sid *sid)
{
	struct tevent_req *req, *subreq;
	struct wb_gettoken_state *state;
	struct winbindd_domain *domain;

	req = tevent_req_create(mem_ctx, &state, struct wb_gettoken_state);
	if (req == NULL) {
		return NULL;
	}
	sid_copy(&state->usersid, sid);
	state->ev = ev;

	domain = find_domain_from_sid_noinit(sid);
	if (domain == NULL) {
		DEBUG(5, ("Could not find domain from SID %s\n",
			  sid_string_dbg(sid)));
		tevent_req_nterror(req, NT_STATUS_NO_SUCH_USER);
		return tevent_req_post(req, ev);
	}

	if (lp_winbind_trusted_domains_only() && domain->primary) {
		DEBUG(7, ("wb_gettoken: My domain -- rejecting getgroups() "
			  "for %s.\n", sid_string_tos(sid)));
		tevent_req_nterror(req, NT_STATUS_NO_SUCH_USER);
		return tevent_req_post(req, ev);
	}

	subreq = wb_lookupusergroups_send(state, ev, domain, &state->usersid);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, wb_gettoken_gotgroups, req);
	return req;
}

static void wb_gettoken_gotgroups(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct wb_gettoken_state *state = tevent_req_data(
		req, struct wb_gettoken_state);
        struct dom_sid *sids;
	struct winbindd_domain *domain;
	NTSTATUS status;

	status = wb_lookupusergroups_recv(subreq, state, &state->num_sids,
					  &state->sids);
	TALLOC_FREE(subreq);
	if (tevent_req_nterror(req, status)) {
		return;
	}

	sids = talloc_realloc(state, state->sids, struct dom_sid,
			      state->num_sids + 1);
	if (tevent_req_nomem(sids, req)) {
		return;
	}
	memmove(&sids[1], &sids[0], state->num_sids * sizeof(sids[0]));
	sid_copy(&sids[0], &state->usersid);
	state->num_sids += 1;
	state->sids = sids;

	/*
	 * Expand our domain's aliases
	 */
	domain = find_domain_from_sid_noinit(get_global_sam_sid());
	if (domain == NULL) {
		tevent_req_nterror(req, NT_STATUS_INTERNAL_ERROR);
		return;
	}

	subreq = wb_lookupuseraliases_send(state, state->ev, domain,
					   state->num_sids, state->sids);
	if (tevent_req_nomem(subreq, req)) {
		return;
	}
	tevent_req_set_callback(subreq, wb_gettoken_gotlocalgroups, req);
}

static void wb_gettoken_gotlocalgroups(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct wb_gettoken_state *state = tevent_req_data(
		req, struct wb_gettoken_state);
	uint32_t num_rids;
	uint32_t *rids;
	struct winbindd_domain *domain;
	NTSTATUS status;

	status = wb_lookupuseraliases_recv(subreq, state, &num_rids, &rids);
	TALLOC_FREE(subreq);
	if (tevent_req_nterror(req, status)) {
		return;
	}
	domain = find_domain_from_sid_noinit(get_global_sam_sid());
	if (domain == NULL) {
		tevent_req_nterror(req, NT_STATUS_INTERNAL_ERROR);
		return;
	}
	if (!wb_add_rids_to_sids(state, &state->num_sids, &state->sids,
				 &domain->sid, num_rids, rids)) {
		tevent_req_nterror(req, NT_STATUS_NO_MEMORY);
		return;
	}
	TALLOC_FREE(rids);

	/*
	 * Now expand the builtin groups
	 */

	domain = find_builtin_domain();
	if (domain == NULL) {
		tevent_req_nterror(req, NT_STATUS_INTERNAL_ERROR);
		return;
	}

	subreq = wb_lookupuseraliases_send(state, state->ev, domain,
					   state->num_sids, state->sids);
	if (tevent_req_nomem(subreq, req)) {
		return;
	}
	tevent_req_set_callback(subreq, wb_gettoken_gotbuiltins, req);
}

static void wb_gettoken_gotbuiltins(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct wb_gettoken_state *state = tevent_req_data(
		req, struct wb_gettoken_state);
	uint32_t num_rids;
        uint32_t *rids;
	NTSTATUS status;

	status = wb_lookupuseraliases_recv(subreq, state, &num_rids, &rids);
	TALLOC_FREE(subreq);
	if (tevent_req_nterror(req, status)) {
		return;
	}
	if (!wb_add_rids_to_sids(state, &state->num_sids, &state->sids,
				 &global_sid_Builtin, num_rids, rids)) {
		tevent_req_nterror(req, NT_STATUS_NO_MEMORY);
		return;
	}
	tevent_req_done(req);
}

NTSTATUS wb_gettoken_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx,
			  int *num_sids, struct dom_sid **sids)
{
	struct wb_gettoken_state *state = tevent_req_data(
		req, struct wb_gettoken_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		return status;
	}
	*num_sids = state->num_sids;
	*sids = talloc_move(mem_ctx, &state->sids);
	return NT_STATUS_OK;
}

static bool wb_add_rids_to_sids(TALLOC_CTX *mem_ctx,
				int *pnum_sids, struct dom_sid **psids,
				const struct dom_sid *domain_sid,
				int num_rids, uint32_t *rids)
{
	struct dom_sid *sids;
	int i;

	sids = talloc_realloc(mem_ctx, *psids, struct dom_sid,
			      *pnum_sids + num_rids);
	if (sids == NULL) {
		return false;
	}
	for (i=0; i<num_rids; i++) {
		sid_compose(&sids[i+*pnum_sids], domain_sid, rids[i]);
	}

	*pnum_sids += num_rids;
	*psids = sids;
	return true;
}
