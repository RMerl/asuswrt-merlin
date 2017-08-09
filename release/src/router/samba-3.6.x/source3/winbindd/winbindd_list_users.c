/*
   Unix SMB/CIFS implementation.
   async implementation of WINBINDD_LIST_USERS
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

struct winbindd_list_users_domstate {
	struct tevent_req *subreq;
	struct winbindd_domain *domain;
	struct wbint_userinfos users;
};

struct winbindd_list_users_state {
	int num_received;
	/* All domains */
	int num_domains;
	struct winbindd_list_users_domstate *domains;
};

static void winbindd_list_users_done(struct tevent_req *subreq);

struct tevent_req *winbindd_list_users_send(TALLOC_CTX *mem_ctx,
					    struct tevent_context *ev,
					    struct winbindd_cli_state *cli,
					    struct winbindd_request *request)
{
	struct tevent_req *req;
	struct winbindd_list_users_state *state;
	struct winbindd_domain *domain;
	int i;

	req = tevent_req_create(mem_ctx, &state,
				struct winbindd_list_users_state);
	if (req == NULL) {
		return NULL;
	}

	/* Ensure null termination */
	request->domain_name[sizeof(request->domain_name)-1]='\0';

	DEBUG(3, ("list_users %s\n", request->domain_name));

	if (request->domain_name[0] != '\0') {
		state->num_domains = 1;
	} else {
		state->num_domains = 0;
		for (domain = domain_list(); domain; domain = domain->next) {
			state->num_domains += 1;
		}
	}

	state->domains = talloc_array(state,
				      struct winbindd_list_users_domstate,
				      state->num_domains);
	if (tevent_req_nomem(state->domains, req)) {
		return tevent_req_post(req, ev);
	}

	if (request->domain_name[0] != '\0') {
		state->domains[0].domain = find_domain_from_name_noinit(
			request->domain_name);
		if (state->domains[0].domain == NULL) {
			tevent_req_nterror(req, NT_STATUS_NO_SUCH_DOMAIN);
			return tevent_req_post(req, ev);
		}
	} else {
		i = 0;
		for (domain = domain_list(); domain; domain = domain->next) {
			state->domains[i++].domain = domain;
		}
	}

	for (i=0; i<state->num_domains; i++) {
		struct winbindd_list_users_domstate *d = &state->domains[i];

		d->subreq = dcerpc_wbint_QueryUserList_send(
			state->domains, ev, dom_child_handle(d->domain),
			&d->users);
		if (tevent_req_nomem(d->subreq, req)) {
			TALLOC_FREE(state->domains);
			return tevent_req_post(req, ev);
		}
		tevent_req_set_callback(d->subreq, winbindd_list_users_done,
					req);
	}
	state->num_received = 0;
	return req;
}

static void winbindd_list_users_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct winbindd_list_users_state *state = tevent_req_data(
		req, struct winbindd_list_users_state);
	NTSTATUS status, result;
	int i;

	status = dcerpc_wbint_QueryUserList_recv(subreq, state->domains,
						 &result);

	for (i=0; i<state->num_domains; i++) {
		if (subreq == state->domains[i].subreq) {
			break;
		}
	}
	if (i < state->num_domains) {
		struct winbindd_list_users_domstate *d = &state->domains[i];

		DEBUG(10, ("Domain %s returned %d users\n", d->domain->name,
			   d->users.num_userinfos));

		d->subreq = NULL;

		if (!NT_STATUS_IS_OK(status) || !NT_STATUS_IS_OK(result)) {
			DEBUG(10, ("List_users for domain %s failed\n",
				   d->domain->name));
			d->users.num_userinfos = 0;
		}
	}

	TALLOC_FREE(subreq);

	state->num_received += 1;

	if (state->num_received >= state->num_domains) {
		tevent_req_done(req);
	}
}

NTSTATUS winbindd_list_users_recv(struct tevent_req *req,
				  struct winbindd_response *response)
{
	struct winbindd_list_users_state *state = tevent_req_data(
		req, struct winbindd_list_users_state);
	NTSTATUS status;
	char *result;
	int i;
	uint32_t j;
	size_t len;

	if (tevent_req_is_nterror(req, &status)) {
		return status;
	}

	len = 0;
	response->data.num_entries = 0;
	for (i=0; i<state->num_domains; i++) {
		struct winbindd_list_users_domstate *d = &state->domains[i];

		for (j=0; j<d->users.num_userinfos; j++) {
			fstring name;
			fill_domain_username(name, d->domain->name,
					     d->users.userinfos[j].acct_name,
					     True);
			len += strlen(name)+1;
		}
		response->data.num_entries += d->users.num_userinfos;
	}

	result = talloc_array(response, char, len+1);
	if (result == 0) {
		return NT_STATUS_NO_MEMORY;
	}

	len = 0;
	for (i=0; i<state->num_domains; i++) {
		struct winbindd_list_users_domstate *d = &state->domains[i];

		for (j=0; j<d->users.num_userinfos; j++) {
			fstring name;
			size_t this_len;
			fill_domain_username(name, d->domain->name,
					     d->users.userinfos[j].acct_name,
					     True);
			this_len = strlen(name);
			memcpy(result+len, name, this_len);
			len += this_len;
			result[len] = ',';
			len += 1;
		}
	}
	result[len-1] = '\0';

	response->extra_data.data = result;
	response->length += len;

	return NT_STATUS_OK;
}
