/*
   Unix SMB/CIFS implementation.
   async implementation of WINBINDD_GETUSERDOMGROUPS
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
#include "../libcli/security/security.h"

struct winbindd_getuserdomgroups_state {
	struct dom_sid sid;
	int num_sids;
	struct dom_sid *sids;
};

static void winbindd_getuserdomgroups_done(struct tevent_req *subreq);

struct tevent_req *winbindd_getuserdomgroups_send(TALLOC_CTX *mem_ctx,
						  struct tevent_context *ev,
						  struct winbindd_cli_state *cli,
						  struct winbindd_request *request)
{
	struct tevent_req *req, *subreq;
	struct winbindd_getuserdomgroups_state *state;
	struct winbindd_domain *domain;

	req = tevent_req_create(mem_ctx, &state,
				struct winbindd_getuserdomgroups_state);
	if (req == NULL) {
		return NULL;
	}

	/* Ensure null termination */
	request->data.sid[sizeof(request->data.sid)-1]='\0';

	DEBUG(3, ("getuserdomgroups %s\n", request->data.sid));

	if (!string_to_sid(&state->sid, request->data.sid)) {
		DEBUG(1, ("Could not get convert sid %s from string\n",
			  request->data.sid));
		tevent_req_nterror(req, NT_STATUS_INVALID_PARAMETER);
		return tevent_req_post(req, ev);
	}

	domain = find_domain_from_sid_noinit(&state->sid);
	if (domain == NULL) {
		DEBUG(1,("could not find domain entry for sid %s\n",
			 request->data.sid));
		tevent_req_nterror(req, NT_STATUS_NO_SUCH_DOMAIN);
		return tevent_req_post(req, ev);
	}

	subreq = wb_lookupusergroups_send(state, ev, domain, &state->sid);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, winbindd_getuserdomgroups_done, req);
	return req;
}

static void winbindd_getuserdomgroups_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct winbindd_getuserdomgroups_state *state = tevent_req_data(
		req, struct winbindd_getuserdomgroups_state);
	NTSTATUS status;

	status = wb_lookupusergroups_recv(subreq, state, &state->num_sids,
					  &state->sids);
	TALLOC_FREE(subreq);
	if (tevent_req_nterror(req, status)) {
		return;
	}
	tevent_req_done(req);
}

NTSTATUS winbindd_getuserdomgroups_recv(struct tevent_req *req,
					struct winbindd_response *response)
{
	struct winbindd_getuserdomgroups_state *state = tevent_req_data(
		req, struct winbindd_getuserdomgroups_state);
	NTSTATUS status;
	int i;
	char *sidlist;

	if (tevent_req_is_nterror(req, &status)) {
		return status;
	}

	sidlist = talloc_strdup(response, "");
	if (sidlist == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	for (i=0; i<state->num_sids; i++) {
		fstring tmp;
		sidlist = talloc_asprintf_append_buffer(
			sidlist, "%s\n",
			sid_to_fstring(tmp, &state->sids[i]));
		if (sidlist == NULL) {
			return NT_STATUS_NO_MEMORY;
		}
	}
	response->extra_data.data = sidlist;
	response->length += talloc_get_size(sidlist);
	response->data.num_entries = state->num_sids;
	return NT_STATUS_OK;
}
