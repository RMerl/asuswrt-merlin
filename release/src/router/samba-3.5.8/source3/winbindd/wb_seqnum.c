/*
   Unix SMB/CIFS implementation.
   async seqnum
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

struct wb_seqnum_state {
	uint32_t seqnum;
};

static void wb_seqnum_done(struct tevent_req *subreq);

struct tevent_req *wb_seqnum_send(TALLOC_CTX *mem_ctx,
				  struct tevent_context *ev,
				  struct winbindd_domain *domain)
{
	struct tevent_req *req, *subreq;
	struct wb_seqnum_state *state;

	req = tevent_req_create(mem_ctx, &state, struct wb_seqnum_state);
	if (req == NULL) {
		return NULL;
	}
	subreq = rpccli_wbint_QuerySequenceNumber_send(
		state, ev, domain->child.rpccli, &state->seqnum);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, wb_seqnum_done, req);
	return req;
}

static void wb_seqnum_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct wb_seqnum_state *state = tevent_req_data(
		req, struct wb_seqnum_state);
	NTSTATUS status, result;

	status = rpccli_wbint_QuerySequenceNumber_recv(subreq, state, &result);
	TALLOC_FREE(subreq);
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

NTSTATUS wb_seqnum_recv(struct tevent_req *req, uint32_t *seqnum)
{
	struct wb_seqnum_state *state = tevent_req_data(
		req, struct wb_seqnum_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		return status;
	}
	*seqnum = state->seqnum;
	return NT_STATUS_OK;
}
