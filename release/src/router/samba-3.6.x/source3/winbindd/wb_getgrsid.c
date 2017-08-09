/*
   Unix SMB/CIFS implementation.
   async getgrsid
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

struct wb_getgrsid_state {
	struct tevent_context *ev;
	struct dom_sid sid;
	int max_nesting;
	const char *domname;
	const char *name;
	enum lsa_SidType type;
	gid_t gid;
	struct talloc_dict *members;
};

static void wb_getgrsid_lookupsid_done(struct tevent_req *subreq);
static void wb_getgrsid_sid2gid_done(struct tevent_req *subreq);
static void wb_getgrsid_got_members(struct tevent_req *subreq);

struct tevent_req *wb_getgrsid_send(TALLOC_CTX *mem_ctx,
				    struct tevent_context *ev,
				    const struct dom_sid *group_sid,
				    int max_nesting)
{
	struct tevent_req *req, *subreq;
	struct wb_getgrsid_state *state;

	req = tevent_req_create(mem_ctx, &state, struct wb_getgrsid_state);
	if (req == NULL) {
		return NULL;
	}
	sid_copy(&state->sid, group_sid);
	state->ev = ev;
	state->max_nesting = max_nesting;

	if (lp_winbind_trusted_domains_only()) {
		struct winbindd_domain *our_domain = find_our_domain();

		if (dom_sid_compare_domain(group_sid, &our_domain->sid) == 0) {
			DEBUG(7, ("winbindd_getgrsid: My domain -- rejecting "
				  "getgrsid() for %s\n", sid_string_tos(group_sid)));
			tevent_req_nterror(req, NT_STATUS_NO_SUCH_GROUP);
			return tevent_req_post(req, ev);
		}
	}

	subreq = wb_lookupsid_send(state, ev, &state->sid);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, wb_getgrsid_lookupsid_done, req);
	return req;
}

static void wb_getgrsid_lookupsid_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct wb_getgrsid_state *state = tevent_req_data(
		req, struct wb_getgrsid_state);
	NTSTATUS status;

	status = wb_lookupsid_recv(subreq, state, &state->type,
				   &state->domname, &state->name);
	TALLOC_FREE(subreq);
	if (tevent_req_nterror(req, status)) {
		return;
	}

	switch (state->type) {
	case SID_NAME_DOM_GRP:
	case SID_NAME_ALIAS:
	case SID_NAME_WKN_GRP:
		break;
	default:
		tevent_req_nterror(req, NT_STATUS_NO_SUCH_GROUP);
		return;
	}

	subreq = wb_sid2gid_send(state, state->ev, &state->sid);
	if (tevent_req_nomem(subreq, req)) {
		return;
	}
	tevent_req_set_callback(subreq, wb_getgrsid_sid2gid_done, req);
}

static void wb_getgrsid_sid2gid_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct wb_getgrsid_state *state = tevent_req_data(
		req, struct wb_getgrsid_state);
	NTSTATUS status;

	status = wb_sid2gid_recv(subreq, &state->gid);
	TALLOC_FREE(subreq);
	if (tevent_req_nterror(req, status)) {
		return;
	}
	subreq = wb_group_members_send(state, state->ev, &state->sid,
				       state->type, state->max_nesting);
	if (tevent_req_nomem(subreq, req)) {
		return;
	}
	tevent_req_set_callback(subreq, wb_getgrsid_got_members, req);
}

static void wb_getgrsid_got_members(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct wb_getgrsid_state *state = tevent_req_data(
		req, struct wb_getgrsid_state);
	NTSTATUS status;

	status = wb_group_members_recv(subreq, state, &state->members);
	TALLOC_FREE(subreq);
	if (tevent_req_nterror(req, status)) {
		return;
	}
	tevent_req_done(req);
}

NTSTATUS wb_getgrsid_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx,
			  const char **domname, const char **name, gid_t *gid,
			  struct talloc_dict **members)
{
	struct wb_getgrsid_state *state = tevent_req_data(
		req, struct wb_getgrsid_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		return status;
	}
	*domname = talloc_move(mem_ctx, &state->domname);
	*name = talloc_move(mem_ctx, &state->name);
	*gid = state->gid;
	*members = talloc_move(mem_ctx, &state->members);
	return NT_STATUS_OK;
}
