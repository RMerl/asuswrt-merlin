/*
   Unix SMB/CIFS implementation.
   async implementation of WINBINDD_PAM_CHNG_PSWD_AUTH_CRAP
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

struct winbindd_pam_chng_pswd_auth_crap_state {
	struct winbindd_request *request;
	struct winbindd_response *response;
};

static void winbindd_pam_chng_pswd_auth_crap_done(struct tevent_req *subreq);

struct tevent_req *winbindd_pam_chng_pswd_auth_crap_send(
	TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct winbindd_cli_state *cli,
	struct winbindd_request *request)
{
	struct tevent_req *req, *subreq;
	struct winbindd_pam_chng_pswd_auth_crap_state *state;
	struct winbindd_domain *domain;
	const char *domain_name;

	req = tevent_req_create(mem_ctx, &state,
				struct winbindd_pam_chng_pswd_auth_crap_state);
	if (req == NULL) {
		return NULL;
	}
	state->request = request;

	/* Ensure null termination */
	request->data.chng_pswd_auth_crap.user[
		sizeof(request->data.chng_pswd_auth_crap.user)-1]='\0';
	request->data.chng_pswd_auth_crap.domain[
		sizeof(request->data.chng_pswd_auth_crap.domain)-1]=0;

	DEBUG(3, ("[%5lu]: pam change pswd auth crap domain: %s user: %s\n",
		  (unsigned long)cli->pid,
		  request->data.chng_pswd_auth_crap.domain,
		  request->data.chng_pswd_auth_crap.user));

	domain_name = NULL;
	if (*state->request->data.chng_pswd_auth_crap.domain != '\0') {
		domain_name = state->request->data.chng_pswd_auth_crap.domain;
	} else if (lp_winbind_use_default_domain()) {
		domain_name = lp_workgroup();
	}

	domain = NULL;
	if (domain_name != NULL) {
		domain = find_domain_from_name(domain_name);
	}

	if (domain == NULL) {
		tevent_req_nterror(req, NT_STATUS_NO_SUCH_USER);
		return tevent_req_post(req, ev);
	}

	subreq = wb_domain_request_send(state, winbind_event_context(),
					domain, request);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, winbindd_pam_chng_pswd_auth_crap_done,
				req);
	return req;
}

static void winbindd_pam_chng_pswd_auth_crap_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct winbindd_pam_chng_pswd_auth_crap_state *state = tevent_req_data(
		req, struct winbindd_pam_chng_pswd_auth_crap_state);
	int res, err;

	res = wb_domain_request_recv(subreq, state, &state->response, &err);
	TALLOC_FREE(subreq);
	if (res == -1) {
		tevent_req_nterror(req, map_nt_error_from_unix(err));
		return;
	}
	tevent_req_done(req);
}

NTSTATUS winbindd_pam_chng_pswd_auth_crap_recv(
	struct tevent_req *req,
	struct winbindd_response *response)
{
	struct winbindd_pam_chng_pswd_auth_crap_state *state = tevent_req_data(
		req, struct winbindd_pam_chng_pswd_auth_crap_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		set_auth_errors(response, status);
		return status;
	}
	*response = *state->response;
	response->result = WINBINDD_PENDING;
	state->response = talloc_move(response, &state->response);

	return NT_STATUS(response->data.auth.nt_status);
}
