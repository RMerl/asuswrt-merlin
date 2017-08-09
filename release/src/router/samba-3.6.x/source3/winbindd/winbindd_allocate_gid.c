/*
   Unix SMB/CIFS implementation.
   async implementation of WINBINDD_ALLOCATE_GID
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

struct winbindd_allocate_gid_state {
	uint64_t gid;
};

static void winbindd_allocate_gid_done(struct tevent_req *subreq);

struct tevent_req *winbindd_allocate_gid_send(TALLOC_CTX *mem_ctx,
					      struct tevent_context *ev,
					      struct winbindd_cli_state *cli,
					      struct winbindd_request *request)
{
	struct tevent_req *req, *subreq;
	struct winbindd_allocate_gid_state *state;
	struct winbindd_child *child;

	req = tevent_req_create(mem_ctx, &state,
				struct winbindd_allocate_gid_state);
	if (req == NULL) {
		return NULL;
	}

	DEBUG(3, ("allocate_gid\n"));

	child = idmap_child();

	subreq = dcerpc_wbint_AllocateGid_send(state, ev, child->binding_handle,
					       &state->gid);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, winbindd_allocate_gid_done, req);
	return req;
}

static void winbindd_allocate_gid_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct winbindd_allocate_gid_state *state = tevent_req_data(
		req, struct winbindd_allocate_gid_state);
	NTSTATUS status, result;

	status = dcerpc_wbint_AllocateGid_recv(subreq, state, &result);
	TALLOC_FREE(subreq);
	if (any_nt_status_not_ok(status, result, &status)) {
		tevent_req_nterror(req, status);
		return;
	}
	tevent_req_done(req);
}

NTSTATUS winbindd_allocate_gid_recv(struct tevent_req *req,
				    struct winbindd_response *response)
{
	struct winbindd_allocate_gid_state *state = tevent_req_data(
		req, struct winbindd_allocate_gid_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		DEBUG(5, ("Could not allocate gid: %s\n", nt_errstr(status)));
		return status;
	}
	response->data.gid = state->gid;
	return NT_STATUS_OK;
}
