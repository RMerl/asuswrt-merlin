/*
   Unix SMB/CIFS implementation.
   async implementation of WINBINDD_PAM_CHAUTHTOK
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

struct winbindd_pam_chauthtok_state {
	struct winbindd_request *request;
	struct winbindd_response *response;
};

static void winbindd_pam_chauthtok_done(struct tevent_req *subreq);

struct tevent_req *winbindd_pam_chauthtok_send(
	TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct winbindd_cli_state *cli,
	struct winbindd_request *request)
{
	struct tevent_req *req, *subreq;
	struct winbindd_pam_chauthtok_state *state;
	struct winbindd_domain *contact_domain;
	fstring domain, user;
	char *mapped_user;
	NTSTATUS status;

	req = tevent_req_create(mem_ctx, &state,
				struct winbindd_pam_chauthtok_state);
	if (req == NULL) {
		return NULL;
	}
	state->request = request;

	/* Ensure null termination */
	request->data.chauthtok.user[
		sizeof(request->data.chauthtok.user)-1]='\0';

	DEBUG(3, ("[%5lu]: pam chauthtok %s\n", (unsigned long)cli->pid,
		  request->data.chauthtok.user));

	status = normalize_name_unmap(state, request->data.chauthtok.user,
				      &mapped_user);

	if (NT_STATUS_IS_OK(status) ||
	    NT_STATUS_EQUAL(status, NT_STATUS_FILE_RENAMED)) {
		fstrcpy(request->data.chauthtok.user, mapped_user);
	}

	if (!canonicalize_username(request->data.chauthtok.user, domain,
				   user)) {
		DEBUG(10, ("winbindd_pam_chauthtok: canonicalize_username %s "
			   "failed with\n", request->data.chauthtok.user));
		tevent_req_nterror(req, NT_STATUS_NO_SUCH_USER);
		return tevent_req_post(req, ev);
	}

	contact_domain = find_domain_from_name(domain);
	if (contact_domain == NULL) {
		DEBUG(3, ("Cannot change password for [%s] -> [%s]\\[%s] "
			  "as %s is not a trusted domain\n",
			  request->data.chauthtok.user, domain, user, domain));
		tevent_req_nterror(req, NT_STATUS_NO_SUCH_USER);
		return tevent_req_post(req, ev);
	}

	subreq = wb_domain_request_send(state, winbind_event_context(),
					contact_domain, request);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, winbindd_pam_chauthtok_done, req);
	return req;
}

static void winbindd_pam_chauthtok_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct winbindd_pam_chauthtok_state *state = tevent_req_data(
		req, struct winbindd_pam_chauthtok_state);
	int res, err;

	res = wb_domain_request_recv(subreq, state, &state->response, &err);
	TALLOC_FREE(subreq);
	if (res == -1) {
		tevent_req_nterror(req, map_nt_error_from_unix(err));
		return;
	}
	tevent_req_done(req);
}

NTSTATUS winbindd_pam_chauthtok_recv(struct tevent_req *req,
				     struct winbindd_response *response)
{
	struct winbindd_pam_chauthtok_state *state = tevent_req_data(
		req, struct winbindd_pam_chauthtok_state);
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

		/* Update the single sign-on memory creds. */
		status = winbindd_replace_memory_creds(
			state->request->data.chauthtok.user,
			state->request->data.chauthtok.newpass);

		DEBUG(10, ("winbindd_replace_memory_creds returned %s\n",
			   nt_errstr(status)));

		/*
		 * When we login from gdm or xdm and password expires,
		 * we change password, but there are no memory
		 * crendentials So, winbindd_replace_memory_creds()
		 * returns NT_STATUS_OBJECT_NAME_NOT_FOUND. This is
		 * not a failure.  --- BoYang
		 */
		if (NT_STATUS_EQUAL(status, NT_STATUS_OBJECT_NAME_NOT_FOUND)) {
			status = NT_STATUS_OK;
		}
	}
	return status;
}
