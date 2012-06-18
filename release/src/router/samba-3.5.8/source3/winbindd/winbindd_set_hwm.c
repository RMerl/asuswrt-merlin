/*
   Unix SMB/CIFS implementation.
   async implementation of WINBINDD_SET_HWM
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
#include "librpc/gen_ndr/cli_wbint.h"

struct winbindd_set_hwm_state {
	uint8_t dummy;
};

static void winbindd_set_hwm_done(struct tevent_req *subreq);

struct tevent_req *winbindd_set_hwm_send(TALLOC_CTX *mem_ctx,
					 struct tevent_context *ev,
					 struct winbindd_cli_state *cli,
					 struct winbindd_request *request)
{
	struct tevent_req *req, *subreq;
	struct winbindd_set_hwm_state *state;
	struct winbindd_child *child;
	enum wbint_IdType type;

	req = tevent_req_create(mem_ctx, &state,
				struct winbindd_set_hwm_state);
	if (req == NULL) {
		return NULL;
	}

	DEBUG(3, ("set_hwm %d\n", (int)request->data.dual_idmapset.id));

	switch (request->data.dual_idmapset.type) {
	case ID_TYPE_UID:
		type = WBINT_ID_TYPE_UID;
		break;
	case ID_TYPE_GID:
		type = WBINT_ID_TYPE_GID;
		break;
	default:
		tevent_req_nterror(req, NT_STATUS_INVALID_PARAMETER);
		return tevent_req_post(req, ev);
	}

	child = idmap_child();

	subreq = rpccli_wbint_SetHWM_send(state, ev, child->rpccli, type,
					  request->data.dual_idmapset.id);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, winbindd_set_hwm_done, req);
	return req;
}

static void winbindd_set_hwm_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct winbindd_set_hwm_state *state = tevent_req_data(
		req, struct winbindd_set_hwm_state);
	NTSTATUS status, result;

	status = rpccli_wbint_SetHWM_recv(subreq, state, &result);
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}
	if (!NT_STATUS_IS_OK(result)) {
		tevent_req_nterror(req, result);
		return;
	}
	tevent_req_done(req);
}

NTSTATUS winbindd_set_hwm_recv(struct tevent_req *req,
			       struct winbindd_response *response)
{
	return tevent_req_simple_recv_ntstatus(req);
}
