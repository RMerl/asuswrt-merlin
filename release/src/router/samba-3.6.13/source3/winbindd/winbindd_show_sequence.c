/*
   Unix SMB/CIFS implementation.
   async implementation of WINBINDD_SHOW_SEQUENCE
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

struct winbindd_show_sequence_state {
	bool one_domain;
	/* One domain */
	uint32_t seqnum;

	/* All domains */
	int num_domains;
	NTSTATUS *stati;
	struct winbindd_domain **domains;
	uint32_t *seqnums;
};

static void winbindd_show_sequence_done_one(struct tevent_req *subreq);
static void winbindd_show_sequence_done_all(struct tevent_req *subreq);

struct tevent_req *winbindd_show_sequence_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct winbindd_cli_state *cli,
					       struct winbindd_request *request)
{
	struct tevent_req *req, *subreq;
	struct winbindd_show_sequence_state *state;

	req = tevent_req_create(mem_ctx, &state,
				struct winbindd_show_sequence_state);
	if (req == NULL) {
		return NULL;
	}
	state->one_domain = false;
	state->domains = NULL;
	state->stati = NULL;
	state->seqnums = NULL;

	/* Ensure null termination */
	request->domain_name[sizeof(request->domain_name)-1]='\0';

	DEBUG(3, ("show_sequence %s\n", request->domain_name));

	if (request->domain_name[0] != '\0') {
		struct winbindd_domain *domain;

		state->one_domain = true;

		domain = find_domain_from_name_noinit(
			request->domain_name);
		if (domain == NULL) {
			tevent_req_nterror(req, NT_STATUS_NO_SUCH_DOMAIN);
			return tevent_req_post(req, ev);
		}

		subreq = wb_seqnum_send(state, ev, domain);
		if (tevent_req_nomem(subreq, req)) {
			return tevent_req_post(req, ev);
		}
		tevent_req_set_callback(
			subreq, winbindd_show_sequence_done_one, req);
		return req;
	}

	subreq = wb_seqnums_send(state, ev);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, winbindd_show_sequence_done_all, req);
	return req;
}

static void winbindd_show_sequence_done_one(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct winbindd_show_sequence_state *state = tevent_req_data(
		req, struct winbindd_show_sequence_state);
	NTSTATUS status;

	status = wb_seqnum_recv(subreq, &state->seqnum);
	TALLOC_FREE(subreq);
	if (tevent_req_nterror(req, status)) {
		return;
	}
	tevent_req_done(req);
}

static void winbindd_show_sequence_done_all(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct winbindd_show_sequence_state *state = tevent_req_data(
		req, struct winbindd_show_sequence_state);
	NTSTATUS status;

	status = wb_seqnums_recv(subreq, state, &state->num_domains,
				 &state->domains, &state->stati,
				 &state->seqnums);
	TALLOC_FREE(subreq);
	if (tevent_req_nterror(req, status)) {
		return;
	}
	tevent_req_done(req);
}

NTSTATUS winbindd_show_sequence_recv(struct tevent_req *req,
				     struct winbindd_response *response)
{
	struct winbindd_show_sequence_state *state = tevent_req_data(
		req, struct winbindd_show_sequence_state);
	NTSTATUS status;
	char *extra_data;
	int i;

	if (tevent_req_is_nterror(req, &status)) {
		return status;
	}

	if (state->one_domain) {
		response->data.sequence_number = state->seqnum;
		return NT_STATUS_OK;
	}

	extra_data = talloc_strdup(response, "");
	if (extra_data == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	for (i=0; i<state->num_domains; i++) {
		if (!NT_STATUS_IS_OK(state->stati[i])
		    || (state->seqnums[i] == DOM_SEQUENCE_NONE)) {
			extra_data = talloc_asprintf_append_buffer(
				extra_data, "%s : DISCONNECTED\n",
				state->domains[i]->name);
		} else {
			extra_data = talloc_asprintf_append_buffer(
				extra_data, "%s : %d\n",
				state->domains[i]->name,
				(int)state->seqnums[i]);
		}
		if (extra_data == NULL) {
			return NT_STATUS_NO_MEMORY;
		}
	}

	response->extra_data.data = extra_data;
	response->length += talloc_get_size(extra_data);
	return NT_STATUS_OK;
}
