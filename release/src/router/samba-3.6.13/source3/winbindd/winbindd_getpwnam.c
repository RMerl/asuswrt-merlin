/*
   Unix SMB/CIFS implementation.
   async implementation of WINBINDD_GETPWNAM
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
#include "passdb/lookup_sid.h" /* only for LOOKUP_NAME_NO_NSS flag */

struct winbindd_getpwnam_state {
	struct tevent_context *ev;
	fstring domname;
	fstring username;
	struct dom_sid sid;
	enum lsa_SidType type;
	struct winbindd_pw pw;
};

static void winbindd_getpwnam_lookupname_done(struct tevent_req *subreq);
static void winbindd_getpwnam_done(struct tevent_req *subreq);

struct tevent_req *winbindd_getpwnam_send(TALLOC_CTX *mem_ctx,
					  struct tevent_context *ev,
					  struct winbindd_cli_state *cli,
					  struct winbindd_request *request)
{
	struct tevent_req *req, *subreq;
	struct winbindd_getpwnam_state *state;
	char *domuser, *mapped_user;
	NTSTATUS status;

	req = tevent_req_create(mem_ctx, &state,
				struct winbindd_getpwnam_state);
	if (req == NULL) {
		return NULL;
	}
	state->ev = ev;

	/* Ensure null termination */
	request->data.username[sizeof(request->data.username)-1]='\0';

	DEBUG(3, ("getpwnam %s\n", request->data.username));

	domuser = request->data.username;

	status = normalize_name_unmap(state, domuser, &mapped_user);

	if (NT_STATUS_IS_OK(status)
	    || NT_STATUS_EQUAL(status, NT_STATUS_FILE_RENAMED)) {
		/* normalize_name_unmapped did something */
		domuser = mapped_user;
	}

	if (!parse_domain_user(domuser, state->domname, state->username)) {
		DEBUG(5, ("Could not parse domain user: %s\n", domuser));
		tevent_req_nterror(req, NT_STATUS_INVALID_PARAMETER);
		return tevent_req_post(req, ev);
	}

	if (lp_winbind_trusted_domains_only()
	    && strequal(state->domname, lp_workgroup())) {
		DEBUG(7,("winbindd_getpwnam: My domain -- "
			 "rejecting getpwnam() for %s\\%s.\n",
			 state->domname, state->username));
		tevent_req_nterror(req, NT_STATUS_NO_SUCH_USER);
		return tevent_req_post(req, ev);
	}

	subreq = wb_lookupname_send(state, ev, state->domname, state->username,
				    LOOKUP_NAME_NO_NSS);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, winbindd_getpwnam_lookupname_done,
				req);
	return req;
}

static void winbindd_getpwnam_lookupname_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct winbindd_getpwnam_state *state = tevent_req_data(
		req, struct winbindd_getpwnam_state);
	NTSTATUS status;

	status = wb_lookupname_recv(subreq, &state->sid, &state->type);
	TALLOC_FREE(subreq);
	if (tevent_req_nterror(req, status)) {
		return;
	}

	subreq = wb_getpwsid_send(state, state->ev, &state->sid, &state->pw);
	if (tevent_req_nomem(subreq, req)) {
		return;
	}
	tevent_req_set_callback(subreq, winbindd_getpwnam_done, req);
}

static void winbindd_getpwnam_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	NTSTATUS status;

	status = wb_getpwsid_recv(subreq);
	TALLOC_FREE(subreq);
	if (tevent_req_nterror(req, status)) {
		return;
	}
	tevent_req_done(req);
}

NTSTATUS winbindd_getpwnam_recv(struct tevent_req *req,
				struct winbindd_response *response)
{
	struct winbindd_getpwnam_state *state = tevent_req_data(
		req, struct winbindd_getpwnam_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		DEBUG(5, ("Could not convert sid %s: %s\n",
			  sid_string_dbg(&state->sid), nt_errstr(status)));
		return status;
	}
	response->data.pw = state->pw;
	return NT_STATUS_OK;
}
