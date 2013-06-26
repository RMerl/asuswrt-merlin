/*
   Unix SMB/CIFS implementation.
   async implementation of WINBINDD_GETDCNAME
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

struct winbindd_getdcname_state {
	struct netr_DsRGetDCNameInfo *dcinfo;
};

static void winbindd_getdcname_done(struct tevent_req *subreq);

struct tevent_req *winbindd_getdcname_send(TALLOC_CTX *mem_ctx,
					   struct tevent_context *ev,
					   struct winbindd_cli_state *cli,
					   struct winbindd_request *request)
{
	struct tevent_req *req, *subreq;
	struct winbindd_getdcname_state *state;

	req = tevent_req_create(mem_ctx, &state,
				struct winbindd_getdcname_state);
	if (req == NULL) {
		return NULL;
	}

	request->domain_name[sizeof(request->domain_name)-1] = '\0';

	DEBUG(3, ("[%5lu]: getdcname for %s\n", (unsigned long)cli->pid,
		  request->domain_name));

	subreq = wb_dsgetdcname_send(state, ev, request->domain_name, NULL,
				     NULL, 0);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, winbindd_getdcname_done, req);
	return req;
}

static void winbindd_getdcname_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct winbindd_getdcname_state *state = tevent_req_data(
		req, struct winbindd_getdcname_state);
	NTSTATUS status;

	status = wb_dsgetdcname_recv(subreq, state, &state->dcinfo);
	TALLOC_FREE(subreq);
	if (tevent_req_nterror(req, status)) {
		return;
	}
	tevent_req_done(req);
}

NTSTATUS winbindd_getdcname_recv(struct tevent_req *req,
				 struct winbindd_response *response)
{
	struct winbindd_getdcname_state *state = tevent_req_data(
		req, struct winbindd_getdcname_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		DEBUG(5, ("getdcname failed: %s\n", nt_errstr(status)));
		return status;
	}
	fstrcpy(response->data.dc_name, strip_hostname(state->dcinfo->dc_unc));
	return NT_STATUS_OK;
}
