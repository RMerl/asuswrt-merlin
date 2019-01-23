/*
   Unix SMB/CIFS implementation.
   async implementation of WINBINDD_LOOKUPRIDS
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

struct winbindd_lookuprids_state {
	struct tevent_context *ev;
	struct dom_sid domain_sid;
	const char *domain_name;
	struct wbint_RidArray rids;
	struct wbint_Principals names;
};

static bool parse_ridlist(TALLOC_CTX *mem_ctx, char *ridstr,
			  uint32_t **prids, uint32_t *pnum_rids);

static void winbindd_lookuprids_done(struct tevent_req *subreq);

struct tevent_req *winbindd_lookuprids_send(TALLOC_CTX *mem_ctx,
					    struct tevent_context *ev,
					    struct winbindd_cli_state *cli,
					    struct winbindd_request *request)
{
	struct tevent_req *req, *subreq;
	struct winbindd_lookuprids_state *state;
	struct winbindd_domain *domain;

	req = tevent_req_create(mem_ctx, &state,
				struct winbindd_lookuprids_state);
	if (req == NULL) {
		return NULL;
	}
	state->ev = ev;

	/* Ensure null termination */
	request->data.sid[sizeof(request->data.sid)-1]='\0';

	DEBUG(3, ("lookuprids (%s)\n", request->data.sid));

	if (!string_to_sid(&state->domain_sid, request->data.sid)) {
		DEBUG(5, ("%s not a SID\n", request->data.sid));
		tevent_req_nterror(req, NT_STATUS_INVALID_PARAMETER);
		return tevent_req_post(req, ev);
	}

	domain = find_lookup_domain_from_sid(&state->domain_sid);
	if (domain == NULL) {
		DEBUG(5, ("Domain for sid %s not found\n",
			  sid_string_dbg(&state->domain_sid)));
		tevent_req_nterror(req, NT_STATUS_NO_SUCH_DOMAIN);
		return tevent_req_post(req, ev);
	}

	if (request->extra_data.data[request->extra_len-1] != '\0') {
		DEBUG(5, ("extra_data not 0-terminated\n"));
		tevent_req_nterror(req, NT_STATUS_INVALID_PARAMETER);
		return tevent_req_post(req, ev);
	}

	if (!parse_ridlist(state, request->extra_data.data,
			   &state->rids.rids, &state->rids.num_rids)) {
		DEBUG(5, ("parse_ridlist failed\n"));
		tevent_req_nterror(req, NT_STATUS_INVALID_PARAMETER);
		return tevent_req_post(req, ev);
	}

	subreq = dcerpc_wbint_LookupRids_send(
		state, ev, dom_child_handle(domain), &state->domain_sid,
		&state->rids, &state->domain_name, &state->names);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, winbindd_lookuprids_done, req);
	return req;
}

static void winbindd_lookuprids_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct winbindd_lookuprids_state *state = tevent_req_data(
		req, struct winbindd_lookuprids_state);
	NTSTATUS status, result;

	status = dcerpc_wbint_LookupRids_recv(subreq, state, &result);
	TALLOC_FREE(subreq);
	if (any_nt_status_not_ok(status, result, &status)) {
		tevent_req_nterror(req, status);
		return;
	}
	tevent_req_done(req);
}

NTSTATUS winbindd_lookuprids_recv(struct tevent_req *req,
				  struct winbindd_response *response)
{
	struct winbindd_lookuprids_state *state = tevent_req_data(
		req, struct winbindd_lookuprids_state);
	NTSTATUS status;
	char *result;
	int i;

	if (tevent_req_is_nterror(req, &status)) {
		DEBUG(5, ("Lookuprids failed: %s\n",nt_errstr(status)));
		return status;
	}

	result = talloc_strdup(response, "");
	if (result == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	for (i=0; i<state->names.num_principals; i++) {
		struct wbint_Principal *p = &state->names.principals[i];

		result = talloc_asprintf_append_buffer(
			result, "%d %s\n", (int)p->type, p->name);
		if (result == NULL) {
			return NT_STATUS_NO_MEMORY;
		}
	}

	fstrcpy(response->data.domain_name, state->domain_name);
	response->extra_data.data = result;
	response->length += talloc_get_size(result);
	return NT_STATUS_OK;
}

static bool parse_ridlist(TALLOC_CTX *mem_ctx, char *ridstr,
			  uint32_t **prids, uint32_t *pnum_rids)
{
	uint32_t i, num_rids;
	uint32_t *rids;
	char *p;

	if (ridstr == NULL) {
		return false;
	}

	p = ridstr;
	num_rids = 0;

	/* count rids */

	while ((p = strchr(p, '\n')) != NULL) {
		p += 1;
		num_rids += 1;
	}

	if (num_rids == 0) {
		*pnum_rids = 0;
		*prids = NULL;
		return true;
	}

	rids = talloc_array(mem_ctx, uint32_t, num_rids);
	if (rids == NULL) {
		return false;
	}

	p = ridstr;

	for (i=0; i<num_rids; i++) {
		char *q;
		rids[i] = strtoul(p, &q, 10);
		if (*q != '\n') {
			DEBUG(0, ("Got invalid ridstr: %s\n", p));
			return false;
		}
		p = q+1;
	}

	*pnum_rids = num_rids;
	*prids = rids;
	return true;
}
