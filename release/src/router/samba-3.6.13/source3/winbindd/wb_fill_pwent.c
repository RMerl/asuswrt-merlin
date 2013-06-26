/*
   Unix SMB/CIFS implementation.
   async fill_pwent
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

struct wb_fill_pwent_state {
	struct tevent_context *ev;
	struct wbint_userinfo *info;
	struct winbindd_pw *pw;
};

static bool fillup_pw_field(const char *lp_template,
			    const char *username,
			    const char *domname,
			    uid_t uid,
			    gid_t gid,
			    const char *in,
			    fstring out);

static void wb_fill_pwent_sid2uid_done(struct tevent_req *subreq);
static void wb_fill_pwent_sid2gid_done(struct tevent_req *subreq);

struct tevent_req *wb_fill_pwent_send(TALLOC_CTX *mem_ctx,
				      struct tevent_context *ev,
				      struct wbint_userinfo *info,
				      struct winbindd_pw *pw)
{
	struct tevent_req *req, *subreq;
	struct wb_fill_pwent_state *state;

	req = tevent_req_create(mem_ctx, &state, struct wb_fill_pwent_state);
	if (req == NULL) {
		return NULL;
	}
	state->ev = ev;
	state->info = info;
	state->pw = pw;

	subreq = wb_sid2uid_send(state, state->ev, &state->info->user_sid);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, wb_fill_pwent_sid2uid_done, req);
	return req;
}

static void wb_fill_pwent_sid2uid_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct wb_fill_pwent_state *state = tevent_req_data(
		req, struct wb_fill_pwent_state);
	NTSTATUS status;

	status = wb_sid2uid_recv(subreq, &state->pw->pw_uid);
	TALLOC_FREE(subreq);
	if (tevent_req_nterror(req, status)) {
		return;
	}

	subreq = wb_sid2gid_send(state, state->ev, &state->info->group_sid);
	if (tevent_req_nomem(subreq, req)) {
		return;
	}
	tevent_req_set_callback(subreq, wb_fill_pwent_sid2gid_done, req);
}

static void wb_fill_pwent_sid2gid_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct wb_fill_pwent_state *state = tevent_req_data(
		req, struct wb_fill_pwent_state);
	struct winbindd_domain *domain;
	char *dom_name;
	fstring user_name, output_username;
	char *mapped_name = NULL;
	NTSTATUS status;

	status = wb_sid2gid_recv(subreq, &state->pw->pw_gid);
	TALLOC_FREE(subreq);
	if (tevent_req_nterror(req, status)) {
		return;
	}

	domain = find_domain_from_sid_noinit(&state->info->user_sid);
	if (domain == NULL) {
		tevent_req_nterror(req, NT_STATUS_NO_SUCH_USER);
		return;
	}
	dom_name = domain->name;

	/* Username */

	fstrcpy(user_name, state->info->acct_name);
	strlower_m(user_name);
	status = normalize_name_map(state, domain, user_name, &mapped_name);

	/* Basic removal of whitespace */
	if (NT_STATUS_IS_OK(status)) {
		fill_domain_username(output_username, dom_name, mapped_name,
				     true);
	}
	/* Complete name replacement */
	else if (NT_STATUS_EQUAL(status, NT_STATUS_FILE_RENAMED)) {
		fstrcpy(output_username, mapped_name);
	}
	/* No change at all */
	else {
		fill_domain_username(output_username, dom_name, user_name,
				     true);
	}

	fstrcpy(state->pw->pw_name, output_username);
	fstrcpy(state->pw->pw_gecos, state->info->full_name);

	/* Home directory and shell */

	if (!fillup_pw_field(lp_template_homedir(), user_name, dom_name,
			     state->pw->pw_uid, state->pw->pw_gid,
			     state->info->homedir, state->pw->pw_dir)) {
		tevent_req_nterror(req, NT_STATUS_NO_SUCH_USER);
		return;
	}

	if (!fillup_pw_field(lp_template_shell(), user_name, dom_name,
			     state->pw->pw_uid, state->pw->pw_gid,
			     state->info->shell, state->pw->pw_shell)) {
		tevent_req_nterror(req, NT_STATUS_NO_SUCH_USER);
		return;
	}

	/* Password - set to "*" as we can't generate anything useful here.
	   Authentication can be done using the pam_winbind module. */

	fstrcpy(state->pw->pw_passwd, "*");
	tevent_req_done(req);
}

NTSTATUS wb_fill_pwent_recv(struct tevent_req *req)
{
	return tevent_req_simple_recv_ntstatus(req);
}

static bool fillup_pw_field(const char *lp_template,
			    const char *username,
			    const char *domname,
			    uid_t uid,
			    gid_t gid,
			    const char *in,
			    fstring out)
{
	char *templ;

	if (out == NULL)
		return False;

	/* The substitution of %U and %D in the 'template
	   homedir' is done by talloc_sub_specified() below.
	   If we have an in string (which means the value has already
	   been set in the nss_info backend), then use that.
	   Otherwise use the template value passed in. */

	if ((in != NULL) && (in[0] != '\0') && (lp_security() == SEC_ADS)) {
		templ = talloc_sub_specified(talloc_tos(), in,
					     username, domname,
					     uid, gid);
	} else {
		templ = talloc_sub_specified(talloc_tos(), lp_template,
					     username, domname,
					     uid, gid);
	}

	if (!templ)
		return False;

	safe_strcpy(out, templ, sizeof(fstring) - 1);
	TALLOC_FREE(templ);

	return True;

}
