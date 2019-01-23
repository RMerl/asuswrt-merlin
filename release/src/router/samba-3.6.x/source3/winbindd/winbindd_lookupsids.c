/*
   Unix SMB/CIFS implementation.
   async implementation of WINBINDD_LOOKUPSIDS
   Copyright (C) Volker Lendecke 2011

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

struct winbindd_lookupsids_state {
	struct dom_sid *sids;
	uint32_t num_sids;
	struct lsa_RefDomainList *domains;
	struct lsa_TransNameArray *names;
};

static void winbindd_lookupsids_done(struct tevent_req *subreq);

struct tevent_req *winbindd_lookupsids_send(TALLOC_CTX *mem_ctx,
					    struct tevent_context *ev,
					   struct winbindd_cli_state *cli,
					   struct winbindd_request *request)
{
	struct tevent_req *req, *subreq;
	struct winbindd_lookupsids_state *state;

	req = tevent_req_create(mem_ctx, &state,
				struct winbindd_lookupsids_state);
	if (req == NULL) {
		return NULL;
	}

	DEBUG(3, ("lookupsids\n"));

	if (request->extra_len == 0) {
		tevent_req_done(req);
		return tevent_req_post(req, ev);
	}
	if (request->extra_data.data[request->extra_len-1] != '\0') {
		DEBUG(10, ("Got invalid sids list\n"));
		tevent_req_nterror(req, NT_STATUS_INVALID_PARAMETER);
		return tevent_req_post(req, ev);
	}
	if (!parse_sidlist(state, request->extra_data.data,
			   &state->sids, &state->num_sids)) {
		DEBUG(10, ("parse_sidlist failed\n"));
		tevent_req_nterror(req, NT_STATUS_INVALID_PARAMETER);
		return tevent_req_post(req, ev);
	}
	subreq = wb_lookupsids_send(state, ev, state->sids, state->num_sids);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, winbindd_lookupsids_done, req);
	return req;
}

static void winbindd_lookupsids_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct winbindd_lookupsids_state *state = tevent_req_data(
		req, struct winbindd_lookupsids_state);
	NTSTATUS status;

	status = wb_lookupsids_recv(subreq, state, &state->domains,
				    &state->names);
	TALLOC_FREE(subreq);
	if (tevent_req_nterror(req, status)) {
		return;
	}
	tevent_req_done(req);
}

NTSTATUS winbindd_lookupsids_recv(struct tevent_req *req,
				  struct winbindd_response *response)
{
	struct winbindd_lookupsids_state *state = tevent_req_data(
		req, struct winbindd_lookupsids_state);
	NTSTATUS status;
	char *result;
	uint32_t i;

	if (tevent_req_is_nterror(req, &status)) {
		DEBUG(5, ("wb_lookupsids failed: %s\n", nt_errstr(status)));
		return status;
	}

	result = talloc_asprintf(response, "%d\n", (int)state->domains->count);
	if (result == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	for (i=0; i<state->domains->count; i++) {
		fstring sid_str;

		result = talloc_asprintf_append_buffer(
			result, "%s %s\n",
			sid_to_fstring(sid_str,
				       state->domains->domains[i].sid),
			state->domains->domains[i].name.string);
		if (result == NULL) {
			return NT_STATUS_NO_MEMORY;
		}
	}

	result = talloc_asprintf_append_buffer(
		result, "%d\n", (int)state->names->count);
	if (result == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	for (i=0; i<state->names->count; i++) {
		struct lsa_TranslatedName *name;

		name = &state->names->names[i];

		result = talloc_asprintf_append_buffer(
			result, "%d %d %s\n",
			(int)name->sid_index, (int)name->sid_type,
			name->name.string);
		if (result == NULL) {
			return NT_STATUS_NO_MEMORY;
		}
	}

	response->extra_data.data = result;
	response->length += talloc_get_size(result);
	return NT_STATUS_OK;
}
