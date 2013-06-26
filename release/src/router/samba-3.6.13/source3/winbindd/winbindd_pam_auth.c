/*
   Unix SMB/CIFS implementation.
   async implementation of WINBINDD_PAM_AUTH
   Copyright (C) Volker Lendecke 2010

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

struct winbindd_pam_auth_state {
	struct winbindd_request *request;
	struct winbindd_response *response;
};

static void winbindd_pam_auth_done(struct tevent_req *subreq);

struct tevent_req *winbindd_pam_auth_send(TALLOC_CTX *mem_ctx,
					  struct tevent_context *ev,
					  struct winbindd_cli_state *cli,
					  struct winbindd_request *request)
{
	struct tevent_req *req, *subreq;
	struct winbindd_pam_auth_state *state;
	struct winbindd_domain *domain;
	fstring name_domain, name_user;
	char *mapped = NULL;
	NTSTATUS status;

	req = tevent_req_create(mem_ctx, &state,
				struct winbindd_pam_auth_state);
	if (req == NULL) {
		return NULL;
	}
	state->request = request;

	/* Ensure null termination */
	request->data.auth.user[sizeof(request->data.auth.user)-1] = '\0';
	request->data.auth.pass[sizeof(request->data.auth.pass)-1] = '\0';

	DEBUG(3, ("[%5lu]: pam auth %s\n", (unsigned long)cli->pid,
		  request->data.auth.user));

	if (!check_request_flags(request->flags)) {
		tevent_req_nterror(req, NT_STATUS_INVALID_PARAMETER_MIX);
		return tevent_req_post(req, ev);
	}

	/* Parse domain and username */

	status = normalize_name_unmap(state, request->data.auth.user, &mapped);

	/* If the name normalization changed something, copy it over the given
	   name */

	if (NT_STATUS_IS_OK(status)
	    || NT_STATUS_EQUAL(status, NT_STATUS_FILE_RENAMED)) {
		fstrcpy(request->data.auth.user, mapped);
	}

	if (!canonicalize_username(request->data.auth.user, name_domain, name_user)) {
		tevent_req_nterror(req, NT_STATUS_NO_SUCH_USER);
		return tevent_req_post(req, ev);
	}

	domain = find_auth_domain(request->flags, name_domain);
	if (domain == NULL) {
		tevent_req_nterror(req, NT_STATUS_NO_SUCH_USER);
		return tevent_req_post(req, ev);
	}

	subreq = wb_domain_request_send(state, winbind_event_context(), domain,
					request);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, winbindd_pam_auth_done, req);
	return req;
}

static void winbindd_pam_auth_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct winbindd_pam_auth_state *state = tevent_req_data(
		req, struct winbindd_pam_auth_state);
	int res, err;

	res = wb_domain_request_recv(subreq, state, &state->response, &err);
	TALLOC_FREE(subreq);
	if (res == -1) {
		tevent_req_nterror(req, map_nt_error_from_unix(err));
		return;
	}
	tevent_req_done(req);
}

NTSTATUS winbindd_pam_auth_recv(struct tevent_req *req,
				struct winbindd_response *response)
{
	struct winbindd_pam_auth_state *state = tevent_req_data(
		req, struct winbindd_pam_auth_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		set_auth_errors(response, status);
		return status;
	}
	*response = *state->response;
	response->result = WINBINDD_PENDING;
	state->response = talloc_move(response, &state->response);

	status = NT_STATUS(response->data.auth.nt_status);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	if (state->request->flags & WBFLAG_PAM_CACHED_LOGIN) {

		/* Store in-memory creds for single-signon using ntlm_auth. */

		status = winbindd_add_memory_creds(
			state->request->data.auth.user,
			get_uid_from_request(state->request),
			state->request->data.auth.pass);
		DEBUG(10, ("winbindd_add_memory_creds returned: %s\n",
			   nt_errstr(status)));
	}

	return status;
}
