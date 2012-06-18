/*
   Unix SMB/CIFS implementation.

   Winbind daemon - pam auth funcions

   Copyright (C) Andrew Tridgell 2000
   Copyright (C) Tim Potter 2001
   Copyright (C) Andrew Bartlett 2001-2002
   Copyright (C) Guenther Deschner 2005

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
#include "../libcli/auth/libcli_auth.h"
#include "../librpc/gen_ndr/cli_samr.h"
#include "smb_krb5.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_WINBIND

#define LOGON_KRB5_FAIL_CLOCK_SKEW	0x02000000

static NTSTATUS append_info3_as_txt(TALLOC_CTX *mem_ctx,
				    struct winbindd_cli_state *state,
				    struct netr_SamInfo3 *info3)
{
	char *ex;
	uint32_t i;

	state->response->data.auth.info3.logon_time =
		nt_time_to_unix(info3->base.last_logon);
	state->response->data.auth.info3.logoff_time =
		nt_time_to_unix(info3->base.last_logoff);
	state->response->data.auth.info3.kickoff_time =
		nt_time_to_unix(info3->base.acct_expiry);
	state->response->data.auth.info3.pass_last_set_time =
		nt_time_to_unix(info3->base.last_password_change);
	state->response->data.auth.info3.pass_can_change_time =
		nt_time_to_unix(info3->base.allow_password_change);
	state->response->data.auth.info3.pass_must_change_time =
		nt_time_to_unix(info3->base.force_password_change);

	state->response->data.auth.info3.logon_count = info3->base.logon_count;
	state->response->data.auth.info3.bad_pw_count = info3->base.bad_password_count;

	state->response->data.auth.info3.user_rid = info3->base.rid;
	state->response->data.auth.info3.group_rid = info3->base.primary_gid;
	sid_to_fstring(state->response->data.auth.info3.dom_sid, info3->base.domain_sid);

	state->response->data.auth.info3.num_groups = info3->base.groups.count;
	state->response->data.auth.info3.user_flgs = info3->base.user_flags;

	state->response->data.auth.info3.acct_flags = info3->base.acct_flags;
	state->response->data.auth.info3.num_other_sids = info3->sidcount;

	fstrcpy(state->response->data.auth.info3.user_name,
		info3->base.account_name.string);
	fstrcpy(state->response->data.auth.info3.full_name,
		info3->base.full_name.string);
	fstrcpy(state->response->data.auth.info3.logon_script,
		info3->base.logon_script.string);
	fstrcpy(state->response->data.auth.info3.profile_path,
		info3->base.profile_path.string);
	fstrcpy(state->response->data.auth.info3.home_dir,
		info3->base.home_directory.string);
	fstrcpy(state->response->data.auth.info3.dir_drive,
		info3->base.home_drive.string);

	fstrcpy(state->response->data.auth.info3.logon_srv,
		info3->base.logon_server.string);
	fstrcpy(state->response->data.auth.info3.logon_dom,
		info3->base.domain.string);

	ex = talloc_strdup(state->mem_ctx, "");
	NT_STATUS_HAVE_NO_MEMORY(ex);

	for (i=0; i < info3->base.groups.count; i++) {
		ex = talloc_asprintf_append_buffer(ex, "0x%08X:0x%08X\n",
						   info3->base.groups.rids[i].rid,
						   info3->base.groups.rids[i].attributes);
		NT_STATUS_HAVE_NO_MEMORY(ex);
	}

	for (i=0; i < info3->sidcount; i++) {
		char *sid;

		sid = dom_sid_string(mem_ctx, info3->sids[i].sid);
		NT_STATUS_HAVE_NO_MEMORY(sid);

		ex = talloc_asprintf_append_buffer(ex, "%s:0x%08X\n",
						   sid,
						   info3->sids[i].attributes);
		NT_STATUS_HAVE_NO_MEMORY(ex);

		talloc_free(sid);
	}

	state->response->extra_data.data = ex;
	state->response->length += talloc_get_size(ex);

	return NT_STATUS_OK;
}

static NTSTATUS append_info3_as_ndr(TALLOC_CTX *mem_ctx,
				    struct winbindd_cli_state *state,
				    struct netr_SamInfo3 *info3)
{
	DATA_BLOB blob;
	enum ndr_err_code ndr_err;

	ndr_err = ndr_push_struct_blob(&blob, mem_ctx, NULL, info3,
				       (ndr_push_flags_fn_t)ndr_push_netr_SamInfo3);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		DEBUG(0,("append_info3_as_ndr: failed to append\n"));
		return ndr_map_error2ntstatus(ndr_err);
	}

	state->response->extra_data.data = blob.data;
	state->response->length += blob.length;

	return NT_STATUS_OK;
}

static NTSTATUS append_unix_username(TALLOC_CTX *mem_ctx,
				     struct winbindd_cli_state *state,
				     const struct netr_SamInfo3 *info3,
				     const char *name_domain,
				     const char *name_user)
{
	/* We've been asked to return the unix username, per
	   'winbind use default domain' settings and the like */

	const char *nt_username, *nt_domain;

	nt_domain = talloc_strdup(mem_ctx, info3->base.domain.string);
	if (!nt_domain) {
		/* If the server didn't give us one, just use the one
		 * we sent them */
		nt_domain = name_domain;
	}

	nt_username = talloc_strdup(mem_ctx, info3->base.account_name.string);
	if (!nt_username) {
		/* If the server didn't give us one, just use the one
		 * we sent them */
		nt_username = name_user;
	}

	fill_domain_username(state->response->data.auth.unix_username,
			     nt_domain, nt_username, true);

	DEBUG(5,("Setting unix username to [%s]\n",
		state->response->data.auth.unix_username));

	return NT_STATUS_OK;
}

static NTSTATUS append_afs_token(TALLOC_CTX *mem_ctx,
				 struct winbindd_cli_state *state,
				 const struct netr_SamInfo3 *info3,
				 const char *name_domain,
				 const char *name_user)
{
	char *afsname = NULL;
	char *cell;
	char *token;

	afsname = talloc_strdup(mem_ctx, lp_afs_username_map());
	if (afsname == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	afsname = talloc_string_sub(mem_ctx,
				    lp_afs_username_map(),
				    "%D", name_domain);
	afsname = talloc_string_sub(mem_ctx, afsname,
				    "%u", name_user);
	afsname = talloc_string_sub(mem_ctx, afsname,
				    "%U", name_user);

	{
		DOM_SID user_sid;
		fstring sidstr;

		sid_copy(&user_sid, info3->base.domain_sid);
		sid_append_rid(&user_sid, info3->base.rid);
		sid_to_fstring(sidstr, &user_sid);
		afsname = talloc_string_sub(mem_ctx, afsname,
					    "%s", sidstr);
	}

	if (afsname == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	strlower_m(afsname);

	DEBUG(10, ("Generating token for user %s\n", afsname));

	cell = strchr(afsname, '@');

	if (cell == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	*cell = '\0';
	cell += 1;

	token = afs_createtoken_str(afsname, cell);
	if (token == NULL) {
		return NT_STATUS_OK;
	}
	state->response->extra_data.data = talloc_strdup(state->mem_ctx,
							 token);
	if (state->response->extra_data.data == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	state->response->length +=
		strlen((const char *)state->response->extra_data.data)+1;

	return NT_STATUS_OK;
}

NTSTATUS check_info3_in_group(struct netr_SamInfo3 *info3,
			      const char *group_sid)
/**
 * Check whether a user belongs to a group or list of groups.
 *
 * @param mem_ctx talloc memory context.
 * @param info3 user information, including group membership info.
 * @param group_sid One or more groups , separated by commas.
 *
 * @return NT_STATUS_OK on success,
 *    NT_STATUS_LOGON_FAILURE if the user does not belong,
 *    or other NT_STATUS_IS_ERR(status) for other kinds of failure.
 */
{
	DOM_SID *require_membership_of_sid;
	size_t num_require_membership_of_sid;
	char *req_sid;
	const char *p;
	DOM_SID sid;
	size_t i;
	struct nt_user_token *token;
	TALLOC_CTX *frame = talloc_stackframe();
	NTSTATUS status;

	/* Parse the 'required group' SID */

	if (!group_sid || !group_sid[0]) {
		/* NO sid supplied, all users may access */
		return NT_STATUS_OK;
	}

	token = talloc_zero(talloc_tos(), struct nt_user_token);
	if (token == NULL) {
		DEBUG(0, ("talloc failed\n"));
		TALLOC_FREE(frame);
		return NT_STATUS_NO_MEMORY;
	}

	num_require_membership_of_sid = 0;
	require_membership_of_sid = NULL;

	p = group_sid;

	while (next_token_talloc(talloc_tos(), &p, &req_sid, ",")) {
		if (!string_to_sid(&sid, req_sid)) {
			DEBUG(0, ("check_info3_in_group: could not parse %s "
				  "as a SID!", req_sid));
			TALLOC_FREE(frame);
			return NT_STATUS_INVALID_PARAMETER;
		}

		status = add_sid_to_array(talloc_tos(), &sid,
					  &require_membership_of_sid,
					  &num_require_membership_of_sid);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0, ("add_sid_to_array failed\n"));
			TALLOC_FREE(frame);
			return status;
		}
	}

	status = sid_array_from_info3(talloc_tos(), info3,
				      &token->user_sids,
				      &token->num_sids,
				      true, false);
	if (!NT_STATUS_IS_OK(status)) {
		TALLOC_FREE(frame);
		return status;
	}

	if (!NT_STATUS_IS_OK(status = add_aliases(get_global_sam_sid(),
						  token))
	    || !NT_STATUS_IS_OK(status = add_aliases(&global_sid_Builtin,
						     token))) {
		DEBUG(3, ("could not add aliases: %s\n",
			  nt_errstr(status)));
		TALLOC_FREE(frame);
		return status;
	}

	debug_nt_user_token(DBGC_CLASS, 10, token);

	for (i=0; i<num_require_membership_of_sid; i++) {
		DEBUG(10, ("Checking SID %s\n", sid_string_dbg(
				   &require_membership_of_sid[i])));
		if (nt_token_check_sid(&require_membership_of_sid[i],
				       token)) {
			DEBUG(10, ("Access ok\n"));
			TALLOC_FREE(frame);
			return NT_STATUS_OK;
		}
	}

	/* Do not distinguish this error from a wrong username/pw */

	TALLOC_FREE(frame);
	return NT_STATUS_LOGON_FAILURE;
}

struct winbindd_domain *find_auth_domain(uint8_t flags,
					 const char *domain_name)
{
	struct winbindd_domain *domain;

	if (IS_DC) {
		domain = find_domain_from_name_noinit(domain_name);
		if (domain == NULL) {
			DEBUG(3, ("Authentication for domain [%s] refused "
				  "as it is not a trusted domain\n",
				  domain_name));
		}
		return domain;
	}

	if (is_myname(domain_name)) {
		DEBUG(3, ("Authentication for domain %s (local domain "
			  "to this server) not supported at this "
			  "stage\n", domain_name));
		return NULL;
	}

	/* we can auth against trusted domains */
	if (flags & WBFLAG_PAM_CONTACT_TRUSTDOM) {
		domain = find_domain_from_name_noinit(domain_name);
		if (domain == NULL) {
			DEBUG(3, ("Authentication for domain [%s] skipped "
				  "as it is not a trusted domain\n",
				  domain_name));
		} else {
			return domain;
		}
	}

	return find_our_domain();
}

static void fill_in_password_policy(struct winbindd_response *r,
				    const struct samr_DomInfo1 *p)
{
	r->data.auth.policy.min_length_password =
		p->min_password_length;
	r->data.auth.policy.password_history =
		p->password_history_length;
	r->data.auth.policy.password_properties =
		p->password_properties;
	r->data.auth.policy.expire	=
		nt_time_to_unix_abs((NTTIME *)&(p->max_password_age));
	r->data.auth.policy.min_passwordage =
		nt_time_to_unix_abs((NTTIME *)&(p->min_password_age));
}

static NTSTATUS fillup_password_policy(struct winbindd_domain *domain,
				       struct winbindd_cli_state *state)
{
	struct winbindd_methods *methods;
	NTSTATUS status = NT_STATUS_UNSUCCESSFUL;
	struct samr_DomInfo1 password_policy;

	if ( !winbindd_can_contact_domain( domain ) ) {
		DEBUG(5,("fillup_password_policy: No inbound trust to "
			 "contact domain %s\n", domain->name));
		return NT_STATUS_NOT_SUPPORTED;
	}

	methods = domain->methods;

	status = methods->password_policy(domain, state->mem_ctx, &password_policy);
	if (NT_STATUS_IS_ERR(status)) {
		return status;
	}

	fill_in_password_policy(state->response, &password_policy);

	return NT_STATUS_OK;
}

static NTSTATUS get_max_bad_attempts_from_lockout_policy(struct winbindd_domain *domain,
							 TALLOC_CTX *mem_ctx,
							 uint16 *lockout_threshold)
{
	struct winbindd_methods *methods;
	NTSTATUS status = NT_STATUS_UNSUCCESSFUL;
	struct samr_DomInfo12 lockout_policy;

	*lockout_threshold = 0;

	methods = domain->methods;

	status = methods->lockout_policy(domain, mem_ctx, &lockout_policy);
	if (NT_STATUS_IS_ERR(status)) {
		return status;
	}

	*lockout_threshold = lockout_policy.lockout_threshold;

	return NT_STATUS_OK;
}

static NTSTATUS get_pwd_properties(struct winbindd_domain *domain,
				   TALLOC_CTX *mem_ctx,
				   uint32 *password_properties)
{
	struct winbindd_methods *methods;
	NTSTATUS status = NT_STATUS_UNSUCCESSFUL;
	struct samr_DomInfo1 password_policy;

	*password_properties = 0;

	methods = domain->methods;

	status = methods->password_policy(domain, mem_ctx, &password_policy);
	if (NT_STATUS_IS_ERR(status)) {
		return status;
	}

	*password_properties = password_policy.password_properties;

	return NT_STATUS_OK;
}

#ifdef HAVE_KRB5

static const char *generate_krb5_ccache(TALLOC_CTX *mem_ctx,
					const char *type,
					uid_t uid,
					bool *internal_ccache)
{
	/* accept FILE and WRFILE as krb5_cc_type from the client and then
	 * build the full ccname string based on the user's uid here -
	 * Guenther*/

	const char *gen_cc = NULL;

	*internal_ccache = true;

	if (uid == -1) {
		goto memory_ccache;
	}

	if (!type || type[0] == '\0') {
		goto memory_ccache;
	}

	if (strequal(type, "FILE")) {
		gen_cc = talloc_asprintf(mem_ctx, "FILE:/tmp/krb5cc_%d", uid);
	} else if (strequal(type, "WRFILE")) {
		gen_cc = talloc_asprintf(mem_ctx, "WRFILE:/tmp/krb5cc_%d", uid);
	} else {
		DEBUG(10,("we don't allow to set a %s type ccache\n", type));
		goto memory_ccache;
	}

	*internal_ccache = false;
	goto done;

  memory_ccache:
  	gen_cc = talloc_strdup(mem_ctx, "MEMORY:winbindd_pam_ccache");

  done:
  	if (gen_cc == NULL) {
		DEBUG(0,("out of memory\n"));
		return NULL;
	}

	DEBUG(10,("using ccache: %s %s\n", gen_cc, *internal_ccache ? "(internal)":""));

	return gen_cc;
}

static void setup_return_cc_name(struct winbindd_cli_state *state, const char *cc)
{
	const char *type = state->request->data.auth.krb5_cc_type;

	state->response->data.auth.krb5ccname[0] = '\0';

	if (type[0] == '\0') {
		return;
	}

	if (!strequal(type, "FILE") &&
	    !strequal(type, "WRFILE")) {
		DEBUG(10,("won't return krbccname for a %s type ccache\n",
			type));
		return;
	}

	fstrcpy(state->response->data.auth.krb5ccname, cc);
}

#endif

static uid_t get_uid_from_state(struct winbindd_cli_state *state)
{
	uid_t uid;

	uid = state->request->data.auth.uid;

	if (uid < 0) {
		DEBUG(1,("invalid uid: '%u'\n", (unsigned int)uid));
		return -1;
	}
	return uid;
}

/**********************************************************************
 Authenticate a user with a clear text password using Kerberos and fill up
 ccache if required
 **********************************************************************/

static NTSTATUS winbindd_raw_kerberos_login(struct winbindd_domain *domain,
					    struct winbindd_cli_state *state,
					    struct netr_SamInfo3 **info3)
{
#ifdef HAVE_KRB5
	NTSTATUS result = NT_STATUS_UNSUCCESSFUL;
	krb5_error_code krb5_ret;
	const char *cc = NULL;
	const char *principal_s = NULL;
	const char *service = NULL;
	char *realm = NULL;
	fstring name_domain, name_user;
	time_t ticket_lifetime = 0;
	time_t renewal_until = 0;
	uid_t uid = -1;
	ADS_STRUCT *ads;
	time_t time_offset = 0;
	bool internal_ccache = true;

	ZERO_STRUCTP(info3);

	*info3 = NULL;

	/* 1st step:
	 * prepare a krb5_cc_cache string for the user */

	uid = get_uid_from_state(state);
	if (uid == -1) {
		DEBUG(0,("no valid uid\n"));
	}

	cc = generate_krb5_ccache(state->mem_ctx,
				  state->request->data.auth.krb5_cc_type,
				  state->request->data.auth.uid,
				  &internal_ccache);
	if (cc == NULL) {
		return NT_STATUS_NO_MEMORY;
	}


	/* 2nd step:
	 * get kerberos properties */

	if (domain->private_data) {
		ads = (ADS_STRUCT *)domain->private_data;
		time_offset = ads->auth.time_offset;
	}


	/* 3rd step:
	 * do kerberos auth and setup ccache as the user */

	parse_domain_user(state->request->data.auth.user, name_domain, name_user);

	realm = domain->alt_name;
	strupper_m(realm);

	principal_s = talloc_asprintf(state->mem_ctx, "%s@%s", name_user, realm);
	if (principal_s == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	service = talloc_asprintf(state->mem_ctx, "%s/%s@%s", KRB5_TGS_NAME, realm, realm);
	if (service == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	/* if this is a user ccache, we need to act as the user to let the krb5
	 * library handle the chown, etc. */

	/************************ ENTERING NON-ROOT **********************/

	if (!internal_ccache) {
		set_effective_uid(uid);
		DEBUG(10,("winbindd_raw_kerberos_login: uid is %d\n", uid));
	}

	result = kerberos_return_info3_from_pac(state->mem_ctx,
						principal_s,
						state->request->data.auth.pass,
						time_offset,
						&ticket_lifetime,
						&renewal_until,
						cc,
						true,
						true,
						WINBINDD_PAM_AUTH_KRB5_RENEW_TIME,
						NULL,
						info3);
	if (!internal_ccache) {
		gain_root_privilege();
	}

	/************************ RETURNED TO ROOT **********************/

	if (!NT_STATUS_IS_OK(result)) {
		goto failed;
	}

	DEBUG(10,("winbindd_raw_kerberos_login: winbindd validated ticket of %s\n",
		principal_s));

	/* if we had a user's ccache then return that string for the pam
	 * environment */

	if (!internal_ccache) {

		setup_return_cc_name(state, cc);

		result = add_ccache_to_list(principal_s,
					    cc,
					    service,
					    state->request->data.auth.user,
					    realm,
					    uid,
					    time(NULL),
					    ticket_lifetime,
					    renewal_until,
					    false);

		if (!NT_STATUS_IS_OK(result)) {
			DEBUG(10,("winbindd_raw_kerberos_login: failed to add ccache to list: %s\n",
				nt_errstr(result)));
		}
	} else {

		/* need to delete the memory cred cache, it is not used anymore */

		krb5_ret = ads_kdestroy(cc);
		if (krb5_ret) {
			DEBUG(3,("winbindd_raw_kerberos_login: "
				 "could not destroy krb5 credential cache: "
				 "%s\n", error_message(krb5_ret)));
		}

	}

	return NT_STATUS_OK;

failed:

	/* we could have created a new credential cache with a valid tgt in it
	 * but we werent able to get or verify the service ticket for this
	 * local host and therefor didn't get the PAC, we need to remove that
	 * cache entirely now */

	krb5_ret = ads_kdestroy(cc);
	if (krb5_ret) {
		DEBUG(3,("winbindd_raw_kerberos_login: "
			 "could not destroy krb5 credential cache: "
			 "%s\n", error_message(krb5_ret)));
	}

	if (!NT_STATUS_IS_OK(remove_ccache(state->request->data.auth.user))) {
		DEBUG(3,("winbindd_raw_kerberos_login: "
			  "could not remove ccache for user %s\n",
			state->request->data.auth.user));
	}

	return result;
#else
	return NT_STATUS_NOT_SUPPORTED;
#endif /* HAVE_KRB5 */
}

/****************************************************************
****************************************************************/

bool check_request_flags(uint32_t flags)
{
	uint32_t flags_edata = WBFLAG_PAM_AFS_TOKEN |
			       WBFLAG_PAM_INFO3_TEXT |
			       WBFLAG_PAM_INFO3_NDR;

	if ( ( (flags & flags_edata) == WBFLAG_PAM_AFS_TOKEN) ||
	     ( (flags & flags_edata) == WBFLAG_PAM_INFO3_NDR) ||
	     ( (flags & flags_edata) == WBFLAG_PAM_INFO3_TEXT)||
	      !(flags & flags_edata) ) {
		return true;
	}

	DEBUG(1, ("check_request_flags: invalid request flags[0x%08X]\n",
		  flags));

	return false;
}

/****************************************************************
****************************************************************/

NTSTATUS append_auth_data(struct winbindd_cli_state *state,
			  struct netr_SamInfo3 *info3,
			  const char *name_domain,
			  const char *name_user)
{
	NTSTATUS result;
	uint32_t flags = state->request->flags;

	if (flags & WBFLAG_PAM_USER_SESSION_KEY) {
		memcpy(state->response->data.auth.user_session_key,
		       info3->base.key.key,
		       sizeof(state->response->data.auth.user_session_key)
		       /* 16 */);
	}

	if (flags & WBFLAG_PAM_LMKEY) {
		memcpy(state->response->data.auth.first_8_lm_hash,
		       info3->base.LMSessKey.key,
		       sizeof(state->response->data.auth.first_8_lm_hash)
		       /* 8 */);
	}

	if (flags & WBFLAG_PAM_INFO3_TEXT) {
		result = append_info3_as_txt(state->mem_ctx, state, info3);
		if (!NT_STATUS_IS_OK(result)) {
			DEBUG(10,("Failed to append INFO3 (TXT): %s\n",
				nt_errstr(result)));
			return result;
		}
	}

	/* currently, anything from here on potentially overwrites extra_data. */

	if (flags & WBFLAG_PAM_INFO3_NDR) {
		result = append_info3_as_ndr(state->mem_ctx, state, info3);
		if (!NT_STATUS_IS_OK(result)) {
			DEBUG(10,("Failed to append INFO3 (NDR): %s\n",
				nt_errstr(result)));
			return result;
		}
	}

	if (flags & WBFLAG_PAM_UNIX_NAME) {
		result = append_unix_username(state->mem_ctx, state, info3,
					      name_domain, name_user);
		if (!NT_STATUS_IS_OK(result)) {
			DEBUG(10,("Failed to append Unix Username: %s\n",
				nt_errstr(result)));
			return result;
		}
	}

	if (flags & WBFLAG_PAM_AFS_TOKEN) {
		result = append_afs_token(state->mem_ctx, state, info3,
					  name_domain, name_user);
		if (!NT_STATUS_IS_OK(result)) {
			DEBUG(10,("Failed to append AFS token: %s\n",
				nt_errstr(result)));
			return result;
		}
	}

	return NT_STATUS_OK;
}

void winbindd_pam_auth(struct winbindd_cli_state *state)
{
	struct winbindd_domain *domain;
	fstring name_domain, name_user;
	char *mapped = NULL;
	NTSTATUS result;
	NTSTATUS name_map_status = NT_STATUS_UNSUCCESSFUL;

	/* Ensure null termination */
	state->request->data.auth.user
		[sizeof(state->request->data.auth.user)-1]='\0';

	/* Ensure null termination */
	state->request->data.auth.pass
		[sizeof(state->request->data.auth.pass)-1]='\0';

	DEBUG(3, ("[%5lu]: pam auth %s\n", (unsigned long)state->pid,
		  state->request->data.auth.user));

	if (!check_request_flags(state->request->flags)) {
		result = NT_STATUS_INVALID_PARAMETER_MIX;
		goto done;
	}

	/* Parse domain and username */

	name_map_status = normalize_name_unmap(state->mem_ctx,
					       state->request->data.auth.user,
					       &mapped);

	/* Update the auth name if we did any mapping */

	if (NT_STATUS_IS_OK(name_map_status) ||
	    NT_STATUS_EQUAL(name_map_status, NT_STATUS_FILE_RENAMED))
	{
		fstrcpy(state->request->data.auth.user, mapped);
	}

	if (!canonicalize_username(state->request->data.auth.user, name_domain, name_user)) {
		result = NT_STATUS_NO_SUCH_USER;
		goto done;
	}

	domain = find_auth_domain(state->request->flags, name_domain);

	if (domain == NULL) {
		result = NT_STATUS_NO_SUCH_USER;
		goto done;
	}

	sendto_domain(state, domain);
	return;
 done:
	set_auth_errors(state->response, result);
	DEBUG(5, ("Plain text authentication for %s returned %s "
		  "(PAM: %d)\n",
		  state->request->data.auth.user,
		  state->response->data.auth.nt_status_string,
		  state->response->data.auth.pam_error));
	request_error(state);
}

static NTSTATUS winbindd_dual_pam_auth_cached(struct winbindd_domain *domain,
					      struct winbindd_cli_state *state,
					      struct netr_SamInfo3 **info3)
{
	NTSTATUS result = NT_STATUS_LOGON_FAILURE;
	uint16 max_allowed_bad_attempts;
	fstring name_domain, name_user;
	DOM_SID sid;
	enum lsa_SidType type;
	uchar new_nt_pass[NT_HASH_LEN];
	const uint8 *cached_nt_pass;
	const uint8 *cached_salt;
	struct netr_SamInfo3 *my_info3;
	time_t kickoff_time, must_change_time;
	bool password_good = false;
#ifdef HAVE_KRB5
	struct winbindd_tdc_domain *tdc_domain = NULL;
#endif

	*info3 = NULL;

	ZERO_STRUCTP(info3);

	DEBUG(10,("winbindd_dual_pam_auth_cached\n"));

	/* Parse domain and username */

	parse_domain_user(state->request->data.auth.user, name_domain, name_user);


	if (!lookup_cached_name(state->mem_ctx,
	                        name_domain,
				name_user,
				&sid,
				&type)) {
		DEBUG(10,("winbindd_dual_pam_auth_cached: no such user in the cache\n"));
		return NT_STATUS_NO_SUCH_USER;
	}

	if (type != SID_NAME_USER) {
		DEBUG(10,("winbindd_dual_pam_auth_cached: not a user (%s)\n", sid_type_lookup(type)));
		return NT_STATUS_LOGON_FAILURE;
	}

	result = winbindd_get_creds(domain,
				    state->mem_ctx,
				    &sid,
				    &my_info3,
				    &cached_nt_pass,
				    &cached_salt);
	if (!NT_STATUS_IS_OK(result)) {
		DEBUG(10,("winbindd_dual_pam_auth_cached: failed to get creds: %s\n", nt_errstr(result)));
		return result;
	}

	*info3 = my_info3;

	E_md4hash(state->request->data.auth.pass, new_nt_pass);

	dump_data_pw("new_nt_pass", new_nt_pass, NT_HASH_LEN);
	dump_data_pw("cached_nt_pass", cached_nt_pass, NT_HASH_LEN);
	if (cached_salt) {
		dump_data_pw("cached_salt", cached_salt, NT_HASH_LEN);
	}

	if (cached_salt) {
		/* In this case we didn't store the nt_hash itself,
		   but the MD5 combination of salt + nt_hash. */
		uchar salted_hash[NT_HASH_LEN];
		E_md5hash(cached_salt, new_nt_pass, salted_hash);

		password_good = (memcmp(cached_nt_pass, salted_hash,
					NT_HASH_LEN) == 0);
	} else {
		/* Old cached cred - direct store of nt_hash (bad bad bad !). */
		password_good = (memcmp(cached_nt_pass, new_nt_pass,
					NT_HASH_LEN) == 0);
	}

	if (password_good) {

		/* User *DOES* know the password, update logon_time and reset
		 * bad_pw_count */

		my_info3->base.user_flags |= NETLOGON_CACHED_ACCOUNT;

		if (my_info3->base.acct_flags & ACB_AUTOLOCK) {
			return NT_STATUS_ACCOUNT_LOCKED_OUT;
		}

		if (my_info3->base.acct_flags & ACB_DISABLED) {
			return NT_STATUS_ACCOUNT_DISABLED;
		}

		if (my_info3->base.acct_flags & ACB_WSTRUST) {
			return NT_STATUS_NOLOGON_WORKSTATION_TRUST_ACCOUNT;
		}

		if (my_info3->base.acct_flags & ACB_SVRTRUST) {
			return NT_STATUS_NOLOGON_SERVER_TRUST_ACCOUNT;
		}

		if (my_info3->base.acct_flags & ACB_DOMTRUST) {
			return NT_STATUS_NOLOGON_INTERDOMAIN_TRUST_ACCOUNT;
		}

		if (!(my_info3->base.acct_flags & ACB_NORMAL)) {
			DEBUG(0,("winbindd_dual_pam_auth_cached: whats wrong with that one?: 0x%08x\n",
				my_info3->base.acct_flags));
			return NT_STATUS_LOGON_FAILURE;
		}

		kickoff_time = nt_time_to_unix(my_info3->base.acct_expiry);
		if (kickoff_time != 0 && time(NULL) > kickoff_time) {
			return NT_STATUS_ACCOUNT_EXPIRED;
		}

		must_change_time = nt_time_to_unix(my_info3->base.force_password_change);
		if (must_change_time != 0 && must_change_time < time(NULL)) {
			/* we allow grace logons when the password has expired */
			my_info3->base.user_flags |= NETLOGON_GRACE_LOGON;
			/* return NT_STATUS_PASSWORD_EXPIRED; */
			goto success;
		}

#ifdef HAVE_KRB5
		if ((state->request->flags & WBFLAG_PAM_KRB5) &&
		    ((tdc_domain = wcache_tdc_fetch_domain(state->mem_ctx, name_domain)) != NULL) &&
		    ((tdc_domain->trust_type & NETR_TRUST_TYPE_UPLEVEL) ||
		    /* used to cope with the case winbindd starting without network. */
		    !strequal(tdc_domain->domain_name, tdc_domain->dns_name))) {

			uid_t uid = -1;
			const char *cc = NULL;
			char *realm = NULL;
			const char *principal_s = NULL;
			const char *service = NULL;
			bool internal_ccache = false;

			uid = get_uid_from_state(state);
			if (uid == -1) {
				DEBUG(0,("winbindd_dual_pam_auth_cached: invalid uid\n"));
				return NT_STATUS_INVALID_PARAMETER;
			}

			cc = generate_krb5_ccache(state->mem_ctx,
						state->request->data.auth.krb5_cc_type,
						state->request->data.auth.uid,
						&internal_ccache);
			if (cc == NULL) {
				return NT_STATUS_NO_MEMORY;
			}

			realm = domain->alt_name;
			strupper_m(realm);

			principal_s = talloc_asprintf(state->mem_ctx, "%s@%s", name_user, realm);
			if (principal_s == NULL) {
				return NT_STATUS_NO_MEMORY;
			}

			service = talloc_asprintf(state->mem_ctx, "%s/%s@%s", KRB5_TGS_NAME, realm, realm);
			if (service == NULL) {
				return NT_STATUS_NO_MEMORY;
			}

			if (!internal_ccache) {

				setup_return_cc_name(state, cc);

				result = add_ccache_to_list(principal_s,
							    cc,
							    service,
							    state->request->data.auth.user,
							    domain->alt_name,
							    uid,
							    time(NULL),
							    time(NULL) + lp_winbind_cache_time(),
							    time(NULL) + WINBINDD_PAM_AUTH_KRB5_RENEW_TIME,
							    true);

				if (!NT_STATUS_IS_OK(result)) {
					DEBUG(10,("winbindd_dual_pam_auth_cached: failed "
						"to add ccache to list: %s\n",
						nt_errstr(result)));
				}
			}
		}
#endif /* HAVE_KRB5 */
 success:
		/* FIXME: we possibly should handle logon hours as well (does xp when
		 * offline?) see auth/auth_sam.c:sam_account_ok for details */

		unix_to_nt_time(&my_info3->base.last_logon, time(NULL));
		my_info3->base.bad_password_count = 0;

		result = winbindd_update_creds_by_info3(domain,
							state->mem_ctx,
							state->request->data.auth.user,
							state->request->data.auth.pass,
							my_info3);
		if (!NT_STATUS_IS_OK(result)) {
			DEBUG(1,("winbindd_dual_pam_auth_cached: failed to update creds: %s\n",
				nt_errstr(result)));
			return result;
		}

		return NT_STATUS_OK;

	}

	/* User does *NOT* know the correct password, modify info3 accordingly */

	/* failure of this is not critical */
	result = get_max_bad_attempts_from_lockout_policy(domain, state->mem_ctx, &max_allowed_bad_attempts);
	if (!NT_STATUS_IS_OK(result)) {
		DEBUG(10,("winbindd_dual_pam_auth_cached: failed to get max_allowed_bad_attempts. "
			  "Won't be able to honour account lockout policies\n"));
	}

	/* increase counter */
	my_info3->base.bad_password_count++;

	if (max_allowed_bad_attempts == 0) {
		goto failed;
	}

	/* lockout user */
	if (my_info3->base.bad_password_count >= max_allowed_bad_attempts) {

		uint32 password_properties;

		result = get_pwd_properties(domain, state->mem_ctx, &password_properties);
		if (!NT_STATUS_IS_OK(result)) {
			DEBUG(10,("winbindd_dual_pam_auth_cached: failed to get password properties.\n"));
		}

		if ((my_info3->base.rid != DOMAIN_USER_RID_ADMIN) ||
		    (password_properties & DOMAIN_PASSWORD_LOCKOUT_ADMINS)) {
			my_info3->base.acct_flags |= ACB_AUTOLOCK;
		}
	}

failed:
	result = winbindd_update_creds_by_info3(domain,
						state->mem_ctx,
						state->request->data.auth.user,
						NULL,
						my_info3);

	if (!NT_STATUS_IS_OK(result)) {
		DEBUG(0,("winbindd_dual_pam_auth_cached: failed to update creds %s\n",
			nt_errstr(result)));
	}

	return NT_STATUS_LOGON_FAILURE;
}

static NTSTATUS winbindd_dual_pam_auth_kerberos(struct winbindd_domain *domain,
						struct winbindd_cli_state *state,
						struct netr_SamInfo3 **info3)
{
	struct winbindd_domain *contact_domain;
	fstring name_domain, name_user;
	NTSTATUS result;

	DEBUG(10,("winbindd_dual_pam_auth_kerberos\n"));

	/* Parse domain and username */

	parse_domain_user(state->request->data.auth.user, name_domain, name_user);

	/* what domain should we contact? */

	if ( IS_DC ) {
		if (!(contact_domain = find_domain_from_name(name_domain))) {
			DEBUG(3, ("Authentication for domain for [%s] -> [%s]\\[%s] failed as %s is not a trusted domain\n",
				  state->request->data.auth.user, name_domain, name_user, name_domain));
			result = NT_STATUS_NO_SUCH_USER;
			goto done;
		}

	} else {
		if (is_myname(name_domain)) {
			DEBUG(3, ("Authentication for domain %s (local domain to this server) not supported at this stage\n", name_domain));
			result =  NT_STATUS_NO_SUCH_USER;
			goto done;
		}

		contact_domain = find_domain_from_name(name_domain);
		if (contact_domain == NULL) {
			DEBUG(3, ("Authentication for domain for [%s] -> [%s]\\[%s] failed as %s is not a trusted domain\n",
				  state->request->data.auth.user, name_domain, name_user, name_domain));

			contact_domain = find_our_domain();
		}
	}

	if (contact_domain->initialized &&
	    contact_domain->active_directory) {
	    	goto try_login;
	}

	if (!contact_domain->initialized) {
		init_dc_connection(contact_domain);
	}

	if (!contact_domain->active_directory) {
		DEBUG(3,("krb5 auth requested but domain is not Active Directory\n"));
		return NT_STATUS_INVALID_LOGON_TYPE;
	}
try_login:
	result = winbindd_raw_kerberos_login(contact_domain, state, info3);
done:
	return result;
}

typedef	NTSTATUS (*netlogon_fn_t)(struct rpc_pipe_client *cli,
				  TALLOC_CTX *mem_ctx,
				  uint32 logon_parameters,
				  const char *server,
				  const char *username,
				  const char *domain,
				  const char *workstation,
				  const uint8 chal[8],
				  uint16_t validation_level,
				  DATA_BLOB lm_response,
				  DATA_BLOB nt_response,
				  struct netr_SamInfo3 **info3);

static NTSTATUS winbindd_dual_pam_auth_samlogon(struct winbindd_domain *domain,
						struct winbindd_cli_state *state,
						struct netr_SamInfo3 **info3)
{

	struct rpc_pipe_client *netlogon_pipe;
	uchar chal[8];
	DATA_BLOB lm_resp;
	DATA_BLOB nt_resp;
	int attempts = 0;
	unsigned char local_lm_response[24];
	unsigned char local_nt_response[24];
	struct winbindd_domain *contact_domain;
	fstring name_domain, name_user;
	bool retry;
	NTSTATUS result;
	struct netr_SamInfo3 *my_info3 = NULL;

	*info3 = NULL;

	DEBUG(10,("winbindd_dual_pam_auth_samlogon\n"));

	/* Parse domain and username */

	parse_domain_user(state->request->data.auth.user, name_domain, name_user);

	/* do password magic */


	generate_random_buffer(chal, 8);
	if (lp_client_ntlmv2_auth()) {
		DATA_BLOB server_chal;
		DATA_BLOB names_blob;
		DATA_BLOB nt_response;
		DATA_BLOB lm_response;
		server_chal = data_blob_talloc(state->mem_ctx, chal, 8);

		/* note that the 'workgroup' here is a best guess - we don't know
		   the server's domain at this point.  The 'server name' is also
		   dodgy...
		*/
		names_blob = NTLMv2_generate_names_blob(state->mem_ctx, global_myname(), lp_workgroup());

		if (!SMBNTLMv2encrypt(NULL, name_user, name_domain,
				      state->request->data.auth.pass,
				      &server_chal,
				      &names_blob,
				      &lm_response, &nt_response, NULL, NULL)) {
			data_blob_free(&names_blob);
			data_blob_free(&server_chal);
			DEBUG(0, ("winbindd_pam_auth: SMBNTLMv2encrypt() failed!\n"));
			result = NT_STATUS_NO_MEMORY;
			goto done;
		}
		data_blob_free(&names_blob);
		data_blob_free(&server_chal);
		lm_resp = data_blob_talloc(state->mem_ctx, lm_response.data,
					   lm_response.length);
		nt_resp = data_blob_talloc(state->mem_ctx, nt_response.data,
					   nt_response.length);
		data_blob_free(&lm_response);
		data_blob_free(&nt_response);

	} else {
		if (lp_client_lanman_auth()
		    && SMBencrypt(state->request->data.auth.pass,
				  chal,
				  local_lm_response)) {
			lm_resp = data_blob_talloc(state->mem_ctx,
						   local_lm_response,
						   sizeof(local_lm_response));
		} else {
			lm_resp = data_blob_null;
		}
		SMBNTencrypt(state->request->data.auth.pass,
			     chal,
			     local_nt_response);

		nt_resp = data_blob_talloc(state->mem_ctx,
					   local_nt_response,
					   sizeof(local_nt_response));
	}

	/* what domain should we contact? */

	if ( IS_DC ) {
		if (!(contact_domain = find_domain_from_name(name_domain))) {
			DEBUG(3, ("Authentication for domain for [%s] -> [%s]\\[%s] failed as %s is not a trusted domain\n",
				  state->request->data.auth.user, name_domain, name_user, name_domain));
			result = NT_STATUS_NO_SUCH_USER;
			goto done;
		}

	} else {
		if (is_myname(name_domain)) {
			DEBUG(3, ("Authentication for domain %s (local domain to this server) not supported at this stage\n", name_domain));
			result =  NT_STATUS_NO_SUCH_USER;
			goto done;
		}

		contact_domain = find_our_domain();
	}

	/* check authentication loop */

	do {
		netlogon_fn_t logon_fn;
		const struct cli_pipe_auth_data *auth;
		uint32_t neg_flags = 0;

		ZERO_STRUCTP(my_info3);
		retry = false;

		result = cm_connect_netlogon(contact_domain, &netlogon_pipe);

		if (!NT_STATUS_IS_OK(result)) {
			DEBUG(3, ("could not open handle to NETLOGON pipe\n"));
			goto done;
		}
		auth = netlogon_pipe->auth;
		if (netlogon_pipe->dc) {
			neg_flags = netlogon_pipe->dc->negotiate_flags;
		}

		/* It is really important to try SamLogonEx here,
		 * because in a clustered environment, we want to use
		 * one machine account from multiple physical
		 * computers.
		 *
		 * With a normal SamLogon call, we must keep the
		 * credentials chain updated and intact between all
		 * users of the machine account (which would imply
		 * cross-node communication for every NTLM logon).
		 *
		 * (The credentials chain is not per NETLOGON pipe
		 * connection, but globally on the server/client pair
		 * by machine name).
		 *
		 * When using SamLogonEx, the credentials are not
		 * supplied, but the session key is implied by the
		 * wrapping SamLogon context.
		 *
		 *  -- abartlet 21 April 2008
		 *
		 * It's also important to use NetlogonValidationSamInfo4 (6),
		 * because it relies on the rpc transport encryption
		 * and avoids using the global netlogon schannel
		 * session key to en/decrypt secret information
		 * like the user_session_key for network logons.
		 *
		 * [MS-APDS] 3.1.5.2 NTLM Network Logon
		 * says NETLOGON_NEG_CROSS_FOREST_TRUSTS and
		 * NETLOGON_NEG_AUTHENTICATED_RPC set together
		 * are the indication that the server supports
		 * NetlogonValidationSamInfo4 (6). And must only
		 * be used if "SealSecureChannel" is used.
		 *
		 * -- metze 4 February 2011
		 */

		if (auth == NULL) {
			domain->can_do_validation6 = false;
		} else if (auth->auth_type != PIPE_AUTH_TYPE_SCHANNEL) {
			domain->can_do_validation6 = false;
		} else if (auth->auth_level != DCERPC_AUTH_LEVEL_PRIVACY) {
			domain->can_do_validation6 = false;
		} else if (!(neg_flags & NETLOGON_NEG_CROSS_FOREST_TRUSTS)) {
			domain->can_do_validation6 = false;
		} else if (!(neg_flags & NETLOGON_NEG_AUTHENTICATED_RPC)) {
			domain->can_do_validation6 = false;
		}

		logon_fn = contact_domain->can_do_samlogon_ex
			? rpccli_netlogon_sam_network_logon_ex
			: rpccli_netlogon_sam_network_logon;

		result = logon_fn(netlogon_pipe,
				  state->mem_ctx,
				  0,
				  contact_domain->dcname, /* server name */
				  name_user,              /* user name */
				  name_domain,            /* target domain */
				  global_myname(),        /* workstation */
				  chal,
				  domain->can_do_validation6 ? 6 : 3,
				  lm_resp,
				  nt_resp,
				  &my_info3);

		if ((NT_STATUS_V(result) == DCERPC_FAULT_OP_RNG_ERROR)
		    && contact_domain->can_do_samlogon_ex) {
			DEBUG(3, ("Got a DC that can not do NetSamLogonEx, "
				  "retrying with NetSamLogon\n"));
			contact_domain->can_do_samlogon_ex = false;
			/*
			 * It's likely that the server also does not support
			 * validation level 6
			 */
			domain->can_do_validation6 = false;
			retry = true;
			continue;
		}

		if (domain->can_do_validation6 &&
		    (NT_STATUS_EQUAL(result, NT_STATUS_INVALID_INFO_CLASS) ||
		     NT_STATUS_EQUAL(result, NT_STATUS_INVALID_PARAMETER) ||
		     NT_STATUS_EQUAL(result, NT_STATUS_BUFFER_TOO_SMALL))) {
			DEBUG(3,("Got a DC that can not do validation level 6, "
				  "retrying with level 3\n"));
			domain->can_do_validation6 = false;
			retry = true;
			continue;
		}

		/*
		 * we increment this after the "feature negotiation"
		 * for can_do_samlogon_ex and can_do_validation6
		 */
		attempts += 1;

		/* We have to try a second time as cm_connect_netlogon
		   might not yet have noticed that the DC has killed
		   our connection. */

		if (!rpccli_is_connected(netlogon_pipe)) {
			retry = true;
			continue;
		}

		/* if we get access denied, a possible cause was that we had
		   and open connection to the DC, but someone changed our
		   machine account password out from underneath us using 'net
		   rpc changetrustpw' */

		if ( NT_STATUS_EQUAL(result, NT_STATUS_ACCESS_DENIED) ) {
			DEBUG(3,("winbindd_pam_auth: sam_logon returned "
				 "ACCESS_DENIED.  Maybe the trust account "
				"password was changed and we didn't know it. "
				 "Killing connections to domain %s\n",
				name_domain));
			invalidate_cm_connection(&contact_domain->conn);
			retry = true;
		}

	} while ( (attempts < 2) && retry );

	/* handle the case where a NT4 DC does not fill in the acct_flags in
	 * the samlogon reply info3. When accurate info3 is required by the
	 * caller, we look up the account flags ourselve - gd */

	if ((state->request->flags & WBFLAG_PAM_INFO3_TEXT) &&
	    NT_STATUS_IS_OK(result) && (my_info3->base.acct_flags == 0)) {

		struct rpc_pipe_client *samr_pipe;
		struct policy_handle samr_domain_handle, user_pol;
		union samr_UserInfo *info = NULL;
		NTSTATUS status_tmp;
		uint32 acct_flags;

		status_tmp = cm_connect_sam(contact_domain, state->mem_ctx,
					    &samr_pipe, &samr_domain_handle);

		if (!NT_STATUS_IS_OK(status_tmp)) {
			DEBUG(3, ("could not open handle to SAMR pipe: %s\n",
				nt_errstr(status_tmp)));
			goto done;
		}

		status_tmp = rpccli_samr_OpenUser(samr_pipe, state->mem_ctx,
						  &samr_domain_handle,
						  MAXIMUM_ALLOWED_ACCESS,
						  my_info3->base.rid,
						  &user_pol);

		if (!NT_STATUS_IS_OK(status_tmp)) {
			DEBUG(3, ("could not open user handle on SAMR pipe: %s\n",
				nt_errstr(status_tmp)));
			goto done;
		}

		status_tmp = rpccli_samr_QueryUserInfo(samr_pipe, state->mem_ctx,
						       &user_pol,
						       16,
						       &info);

		if (!NT_STATUS_IS_OK(status_tmp)) {
			DEBUG(3, ("could not query user info on SAMR pipe: %s\n",
				nt_errstr(status_tmp)));
			rpccli_samr_Close(samr_pipe, state->mem_ctx, &user_pol);
			goto done;
		}

		acct_flags = info->info16.acct_flags;

		if (acct_flags == 0) {
			rpccli_samr_Close(samr_pipe, state->mem_ctx, &user_pol);
			goto done;
		}

		my_info3->base.acct_flags = acct_flags;

		DEBUG(10,("successfully retrieved acct_flags 0x%x\n", acct_flags));

		rpccli_samr_Close(samr_pipe, state->mem_ctx, &user_pol);
	}

	*info3 = my_info3;
done:
	return result;
}

enum winbindd_result winbindd_dual_pam_auth(struct winbindd_domain *domain,
					    struct winbindd_cli_state *state)
{
	NTSTATUS result = NT_STATUS_LOGON_FAILURE;
	NTSTATUS krb5_result = NT_STATUS_OK;
	fstring name_domain, name_user;
	char *mapped_user;
	fstring domain_user;
	struct netr_SamInfo3 *info3 = NULL;
	NTSTATUS name_map_status = NT_STATUS_UNSUCCESSFUL;

	/* Ensure null termination */
	state->request->data.auth.user[sizeof(state->request->data.auth.user)-1]='\0';

	/* Ensure null termination */
	state->request->data.auth.pass[sizeof(state->request->data.auth.pass)-1]='\0';

	DEBUG(3, ("[%5lu]: dual pam auth %s\n", (unsigned long)state->pid,
		  state->request->data.auth.user));

	if (!check_request_flags(state->request->flags)) {
		result = NT_STATUS_INVALID_PARAMETER_MIX;
		goto done;
	}

	/* Parse domain and username */

	name_map_status = normalize_name_unmap(state->mem_ctx,
					       state->request->data.auth.user,
					       &mapped_user);

	/* If the name normalization didnt' actually do anything,
	   just use the original name */

	if (!NT_STATUS_IS_OK(name_map_status) &&
	    !NT_STATUS_EQUAL(name_map_status, NT_STATUS_FILE_RENAMED))
	{
		mapped_user = state->request->data.auth.user;
	}

	parse_domain_user(mapped_user, name_domain, name_user);

	if ( mapped_user != state->request->data.auth.user ) {
		fstr_sprintf( domain_user, "%s\\%s", name_domain, name_user );
		safe_strcpy( state->request->data.auth.user, domain_user,
			     sizeof(state->request->data.auth.user)-1 );
	}

	if (domain->online == false) {
		result = NT_STATUS_DOMAIN_CONTROLLER_NOT_FOUND;
		if (domain->startup) {
			/* Logons are very important to users. If we're offline and
			   we get a request within the first 30 seconds of startup,
			   try very hard to find a DC and go online. */

			DEBUG(10,("winbindd_dual_pam_auth: domain: %s offline and auth "
				"request in startup mode.\n", domain->name ));

			winbindd_flush_negative_conn_cache(domain);
			result = init_dc_connection(domain);
		}
	}

	DEBUG(10,("winbindd_dual_pam_auth: domain: %s last was %s\n", domain->name, domain->online ? "online":"offline"));

	/* Check for Kerberos authentication */
	if (domain->online && (state->request->flags & WBFLAG_PAM_KRB5)) {

		result = winbindd_dual_pam_auth_kerberos(domain, state, &info3);
		/* save for later */
		krb5_result = result;


		if (NT_STATUS_IS_OK(result)) {
			DEBUG(10,("winbindd_dual_pam_auth_kerberos succeeded\n"));
			goto process_result;
		} else {
			DEBUG(10,("winbindd_dual_pam_auth_kerberos failed: %s\n", nt_errstr(result)));
		}

		if (NT_STATUS_EQUAL(result, NT_STATUS_NO_LOGON_SERVERS) ||
		    NT_STATUS_EQUAL(result, NT_STATUS_IO_TIMEOUT) ||
		    NT_STATUS_EQUAL(result, NT_STATUS_DOMAIN_CONTROLLER_NOT_FOUND)) {
			DEBUG(10,("winbindd_dual_pam_auth_kerberos setting domain to offline\n"));
			set_domain_offline( domain );
			goto cached_logon;
		}

		/* there are quite some NT_STATUS errors where there is no
		 * point in retrying with a samlogon, we explictly have to take
		 * care not to increase the bad logon counter on the DC */

		if (NT_STATUS_EQUAL(result, NT_STATUS_ACCOUNT_DISABLED) ||
		    NT_STATUS_EQUAL(result, NT_STATUS_ACCOUNT_EXPIRED) ||
		    NT_STATUS_EQUAL(result, NT_STATUS_ACCOUNT_LOCKED_OUT) ||
		    NT_STATUS_EQUAL(result, NT_STATUS_INVALID_LOGON_HOURS) ||
		    NT_STATUS_EQUAL(result, NT_STATUS_INVALID_WORKSTATION) ||
		    NT_STATUS_EQUAL(result, NT_STATUS_LOGON_FAILURE) ||
		    NT_STATUS_EQUAL(result, NT_STATUS_NO_SUCH_USER) ||
		    NT_STATUS_EQUAL(result, NT_STATUS_PASSWORD_EXPIRED) ||
		    NT_STATUS_EQUAL(result, NT_STATUS_PASSWORD_MUST_CHANGE) ||
		    NT_STATUS_EQUAL(result, NT_STATUS_WRONG_PASSWORD)) {
			goto process_result;
		}

		if (state->request->flags & WBFLAG_PAM_FALLBACK_AFTER_KRB5) {
			DEBUG(3,("falling back to samlogon\n"));
			goto sam_logon;
		} else {
			goto cached_logon;
		}
	}

sam_logon:
	/* Check for Samlogon authentication */
	if (domain->online) {
		result = winbindd_dual_pam_auth_samlogon(domain, state, &info3);

		if (NT_STATUS_IS_OK(result)) {
			DEBUG(10,("winbindd_dual_pam_auth_samlogon succeeded\n"));
			/* add the Krb5 err if we have one */
			if ( NT_STATUS_EQUAL(krb5_result, NT_STATUS_TIME_DIFFERENCE_AT_DC ) ) {
				info3->base.user_flags |= LOGON_KRB5_FAIL_CLOCK_SKEW;
			}
			goto process_result;
		}

		DEBUG(10,("winbindd_dual_pam_auth_samlogon failed: %s\n",
			  nt_errstr(result)));

		if (NT_STATUS_EQUAL(result, NT_STATUS_NO_LOGON_SERVERS) ||
		    NT_STATUS_EQUAL(result, NT_STATUS_IO_TIMEOUT) ||
		    NT_STATUS_EQUAL(result, NT_STATUS_DOMAIN_CONTROLLER_NOT_FOUND))
		{
			DEBUG(10,("winbindd_dual_pam_auth_samlogon setting domain to offline\n"));
			set_domain_offline( domain );
			goto cached_logon;
		}

			if (domain->online) {
				/* We're still online - fail. */
				goto done;
			}
	}

cached_logon:
	/* Check for Cached logons */
	if (!domain->online && (state->request->flags & WBFLAG_PAM_CACHED_LOGIN) &&
	    lp_winbind_offline_logon()) {

		result = winbindd_dual_pam_auth_cached(domain, state, &info3);

		if (NT_STATUS_IS_OK(result)) {
			DEBUG(10,("winbindd_dual_pam_auth_cached succeeded\n"));
			goto process_result;
		} else {
			DEBUG(10,("winbindd_dual_pam_auth_cached failed: %s\n", nt_errstr(result)));
			goto done;
		}
	}

process_result:

	if (NT_STATUS_IS_OK(result)) {

		DOM_SID user_sid;

		/* In all codepaths where result == NT_STATUS_OK info3 must have
		   been initialized. */
		if (!info3) {
			result = NT_STATUS_INTERNAL_ERROR;
			goto done;
		}

		wcache_invalidate_samlogon(find_domain_from_name(name_domain), info3);
		netsamlogon_cache_store(name_user, info3);

		/* save name_to_sid info as early as possible (only if
		   this is our primary domain so we don't invalidate
		   the cache entry by storing the seq_num for the wrong
		   domain). */
		if ( domain->primary ) {
			sid_compose(&user_sid, info3->base.domain_sid,
				    info3->base.rid);
			cache_name2sid(domain, name_domain, name_user,
				       SID_NAME_USER, &user_sid);
		}

		/* Check if the user is in the right group */

		result = check_info3_in_group(
			info3,
			state->request->data.auth.require_membership_of_sid);
		if (!NT_STATUS_IS_OK(result)) {
			DEBUG(3, ("User %s is not in the required group (%s), so plaintext authentication is rejected\n",
				  state->request->data.auth.user,
				  state->request->data.auth.require_membership_of_sid));
			goto done;
		}

		result = append_auth_data(state, info3, name_domain,
					  name_user);
		if (!NT_STATUS_IS_OK(result)) {
			goto done;
		}

		if ((state->request->flags & WBFLAG_PAM_CACHED_LOGIN)) {

			/* Store in-memory creds for single-signon using ntlm_auth. */
			result = winbindd_add_memory_creds(state->request->data.auth.user,
							get_uid_from_state(state),
							state->request->data.auth.pass);

			if (!NT_STATUS_IS_OK(result)) {
				DEBUG(10,("Failed to store memory creds: %s\n", nt_errstr(result)));
				goto done;
			}

			if (lp_winbind_offline_logon()) {
				result = winbindd_store_creds(domain,
						      state->mem_ctx,
						      state->request->data.auth.user,
						      state->request->data.auth.pass,
						      info3, NULL);
				if (!NT_STATUS_IS_OK(result)) {

					/* Release refcount. */
					winbindd_delete_memory_creds(state->request->data.auth.user);

					DEBUG(10,("Failed to store creds: %s\n", nt_errstr(result)));
					goto done;
				}
			}
		}


		if (state->request->flags & WBFLAG_PAM_GET_PWD_POLICY) {
			struct winbindd_domain *our_domain = find_our_domain();

			/* This is not entirely correct I believe, but it is
			   consistent.  Only apply the password policy settings
			   too warn users for our own domain.  Cannot obtain these
			   from trusted DCs all the  time so don't do it at all.
			   -- jerry */

			result = NT_STATUS_NOT_SUPPORTED;
			if (our_domain == domain ) {
				result = fillup_password_policy(our_domain, state);
			}

			if (!NT_STATUS_IS_OK(result)
			    && !NT_STATUS_EQUAL(result, NT_STATUS_NOT_SUPPORTED) )
			{
				DEBUG(10,("Failed to get password policies for domain %s: %s\n",
					  domain->name, nt_errstr(result)));
				goto done;
			}
		}

		result = NT_STATUS_OK;
	}

done:
	/* give us a more useful (more correct?) error code */
	if ((NT_STATUS_EQUAL(result, NT_STATUS_DOMAIN_CONTROLLER_NOT_FOUND) ||
	    (NT_STATUS_EQUAL(result, NT_STATUS_UNSUCCESSFUL)))) {
		result = NT_STATUS_NO_LOGON_SERVERS;
	}

	set_auth_errors(state->response, result);

	DEBUG(NT_STATUS_IS_OK(result) ? 5 : 2, ("Plain-text authentication for user %s returned %s (PAM: %d)\n",
	      state->request->data.auth.user,
	      state->response->data.auth.nt_status_string,
	      state->response->data.auth.pam_error));

	return NT_STATUS_IS_OK(result) ? WINBINDD_OK : WINBINDD_ERROR;
}


/**********************************************************************
 Challenge Response Authentication Protocol
**********************************************************************/

void winbindd_pam_auth_crap(struct winbindd_cli_state *state)
{
	struct winbindd_domain *domain = NULL;
	const char *domain_name = NULL;
	NTSTATUS result;

	if (!check_request_flags(state->request->flags)) {
		result = NT_STATUS_INVALID_PARAMETER_MIX;
		goto done;
	}

	if (!state->privileged) {
		DEBUG(2, ("winbindd_pam_auth_crap: non-privileged access "
			  "denied.  !\n"));
		DEBUGADD(2, ("winbindd_pam_auth_crap: Ensure permissions "
			     "on %s are set correctly.\n",
			     get_winbind_priv_pipe_dir()));
		/* send a better message than ACCESS_DENIED */
		fstr_sprintf(state->response->data.auth.error_string,
			     "winbind client not authorized to use "
			     "winbindd_pam_auth_crap. Ensure permissions on "
			     "%s are set correctly.",
			     get_winbind_priv_pipe_dir());
		result = NT_STATUS_ACCESS_DENIED;
		goto done;
	}

	/* Ensure null termination */
	state->request->data.auth_crap.user
		[sizeof(state->request->data.auth_crap.user)-1]=0;
	state->request->data.auth_crap.domain
		[sizeof(state->request->data.auth_crap.domain)-1]=0;

	DEBUG(3, ("[%5lu]: pam auth crap domain: [%s] user: %s\n",
		  (unsigned long)state->pid,
		  state->request->data.auth_crap.domain,
		  state->request->data.auth_crap.user));

	if (*state->request->data.auth_crap.domain != '\0') {
		domain_name = state->request->data.auth_crap.domain;
	} else if (lp_winbind_use_default_domain()) {
		domain_name = lp_workgroup();
	}

	if (domain_name != NULL)
		domain = find_auth_domain(state->request->flags, domain_name);

	if (domain != NULL) {
		sendto_domain(state, domain);
		return;
	}

	result = NT_STATUS_NO_SUCH_USER;

 done:
	set_auth_errors(state->response, result);
	DEBUG(5, ("CRAP authentication for %s\\%s returned %s (PAM: %d)\n",
		  state->request->data.auth_crap.domain,
		  state->request->data.auth_crap.user,
		  state->response->data.auth.nt_status_string,
		  state->response->data.auth.pam_error));
	request_error(state);
	return;
}


enum winbindd_result winbindd_dual_pam_auth_crap(struct winbindd_domain *domain,
						 struct winbindd_cli_state *state)
{
	NTSTATUS result;
	struct netr_SamInfo3 *info3 = NULL;
	struct rpc_pipe_client *netlogon_pipe;
	const char *name_user = NULL;
	const char *name_domain = NULL;
	const char *workstation;
	struct winbindd_domain *contact_domain;
	int attempts = 0;
	bool retry;

	DATA_BLOB lm_resp, nt_resp;

	/* This is child-only, so no check for privileged access is needed
	   anymore */

	/* Ensure null termination */
	state->request->data.auth_crap.user[sizeof(state->request->data.auth_crap.user)-1]=0;
	state->request->data.auth_crap.domain[sizeof(state->request->data.auth_crap.domain)-1]=0;

	if (!check_request_flags(state->request->flags)) {
		result = NT_STATUS_INVALID_PARAMETER_MIX;
		goto done;
	}

	name_user = state->request->data.auth_crap.user;

	if (*state->request->data.auth_crap.domain) {
		name_domain = state->request->data.auth_crap.domain;
	} else if (lp_winbind_use_default_domain()) {
		name_domain = lp_workgroup();
	} else {
		DEBUG(5,("no domain specified with username (%s) - failing auth\n",
			 name_user));
		result = NT_STATUS_NO_SUCH_USER;
		goto done;
	}

	DEBUG(3, ("[%5lu]: pam auth crap domain: %s user: %s\n", (unsigned long)state->pid,
		  name_domain, name_user));

	if (*state->request->data.auth_crap.workstation) {
		workstation = state->request->data.auth_crap.workstation;
	} else {
		workstation = global_myname();
	}

	if (state->request->data.auth_crap.lm_resp_len > sizeof(state->request->data.auth_crap.lm_resp)
		|| state->request->data.auth_crap.nt_resp_len > sizeof(state->request->data.auth_crap.nt_resp)) {
		if (!(state->request->flags & WBFLAG_BIG_NTLMV2_BLOB) ||
		     state->request->extra_len != state->request->data.auth_crap.nt_resp_len) {
			DEBUG(0, ("winbindd_pam_auth_crap: invalid password length %u/%u\n",
				  state->request->data.auth_crap.lm_resp_len,
				  state->request->data.auth_crap.nt_resp_len));
			result = NT_STATUS_INVALID_PARAMETER;
			goto done;
		}
	}

	lm_resp = data_blob_talloc(state->mem_ctx, state->request->data.auth_crap.lm_resp,
					state->request->data.auth_crap.lm_resp_len);

	if (state->request->flags & WBFLAG_BIG_NTLMV2_BLOB) {
		nt_resp = data_blob_talloc(state->mem_ctx,
					   state->request->extra_data.data,
					   state->request->data.auth_crap.nt_resp_len);
	} else {
		nt_resp = data_blob_talloc(state->mem_ctx,
					   state->request->data.auth_crap.nt_resp,
					   state->request->data.auth_crap.nt_resp_len);
	}

	/* what domain should we contact? */

	if ( IS_DC ) {
		if (!(contact_domain = find_domain_from_name(name_domain))) {
			DEBUG(3, ("Authentication for domain for [%s] -> [%s]\\[%s] failed as %s is not a trusted domain\n",
				  state->request->data.auth_crap.user, name_domain, name_user, name_domain));
			result = NT_STATUS_NO_SUCH_USER;
			goto done;
		}
	} else {
		if (is_myname(name_domain)) {
			DEBUG(3, ("Authentication for domain %s (local domain to this server) not supported at this stage\n", name_domain));
			result =  NT_STATUS_NO_SUCH_USER;
			goto done;
		}
		contact_domain = find_our_domain();
	}

	do {
		netlogon_fn_t logon_fn;
		const struct cli_pipe_auth_data *auth;
		uint32_t neg_flags = 0;

		retry = false;

		netlogon_pipe = NULL;
		result = cm_connect_netlogon(contact_domain, &netlogon_pipe);

		if (!NT_STATUS_IS_OK(result)) {
			DEBUG(3, ("could not open handle to NETLOGON pipe (error: %s)\n",
				  nt_errstr(result)));
			goto done;
		}
		auth = netlogon_pipe->auth;
		if (netlogon_pipe->dc) {
			neg_flags = netlogon_pipe->dc->negotiate_flags;
		}

		if (auth == NULL) {
			domain->can_do_validation6 = false;
		} else if (auth->auth_type != PIPE_AUTH_TYPE_SCHANNEL) {
			domain->can_do_validation6 = false;
		} else if (auth->auth_level != DCERPC_AUTH_LEVEL_PRIVACY) {
			domain->can_do_validation6 = false;
		} else if (!(neg_flags & NETLOGON_NEG_CROSS_FOREST_TRUSTS)) {
			domain->can_do_validation6 = false;
		} else if (!(neg_flags & NETLOGON_NEG_AUTHENTICATED_RPC)) {
			domain->can_do_validation6 = false;
		}

		logon_fn = contact_domain->can_do_samlogon_ex
			? rpccli_netlogon_sam_network_logon_ex
			: rpccli_netlogon_sam_network_logon;

		result = logon_fn(netlogon_pipe,
				  state->mem_ctx,
				  state->request->data.auth_crap.logon_parameters,
				  contact_domain->dcname,
				  name_user,
				  name_domain,
				  /* Bug #3248 - found by Stefan Burkei. */
				  workstation, /* We carefully set this above so use it... */
				  state->request->data.auth_crap.chal,
				  domain->can_do_validation6 ? 6 : 3,
				  lm_resp,
				  nt_resp,
				  &info3);

		if ((NT_STATUS_V(result) == DCERPC_FAULT_OP_RNG_ERROR)
		    && contact_domain->can_do_samlogon_ex) {
			DEBUG(3, ("Got a DC that can not do NetSamLogonEx, "
				  "retrying with NetSamLogon\n"));
			contact_domain->can_do_samlogon_ex = false;
			/*
			 * It's likely that the server also does not support
			 * validation level 6
			 */
			domain->can_do_validation6 = false;
			retry = true;
			continue;
		}

		if (domain->can_do_validation6 &&
		    (NT_STATUS_EQUAL(result, NT_STATUS_INVALID_INFO_CLASS) ||
		     NT_STATUS_EQUAL(result, NT_STATUS_INVALID_PARAMETER) ||
		     NT_STATUS_EQUAL(result, NT_STATUS_BUFFER_TOO_SMALL))) {
			DEBUG(3,("Got a DC that can not do validation level 6, "
				  "retrying with level 3\n"));
			domain->can_do_validation6 = false;
			retry = true;
			continue;
		}

		/*
		 * we increment this after the "feature negotiation"
		 * for can_do_samlogon_ex and can_do_validation6
		 */
		attempts += 1;

		/* We have to try a second time as cm_connect_netlogon
		   might not yet have noticed that the DC has killed
		   our connection. */

		if (!rpccli_is_connected(netlogon_pipe)) {
			retry = true;
			continue;
		}

		/* if we get access denied, a possible cause was that we had and open
		   connection to the DC, but someone changed our machine account password
		   out from underneath us using 'net rpc changetrustpw' */

		if ( NT_STATUS_EQUAL(result, NT_STATUS_ACCESS_DENIED) ) {
			DEBUG(3,("winbindd_pam_auth: sam_logon returned "
				 "ACCESS_DENIED.  Maybe the trust account "
				"password was changed and we didn't know it. "
				 "Killing connections to domain %s\n",
				name_domain));
			invalidate_cm_connection(&contact_domain->conn);
			retry = true;
		}

	} while ( (attempts < 2) && retry );

	if (NT_STATUS_IS_OK(result)) {

		wcache_invalidate_samlogon(find_domain_from_name(name_domain), info3);
		netsamlogon_cache_store(name_user, info3);

		/* Check if the user is in the right group */

		result = check_info3_in_group(
			info3,
			state->request->data.auth_crap.require_membership_of_sid);
		if (!NT_STATUS_IS_OK(result)) {
			DEBUG(3, ("User %s is not in the required group (%s), so "
				  "crap authentication is rejected\n",
				  state->request->data.auth_crap.user,
				  state->request->data.auth_crap.require_membership_of_sid));
			goto done;
		}

		result = append_auth_data(state, info3, name_domain,
					  name_user);
		if (!NT_STATUS_IS_OK(result)) {
			goto done;
		}
	}

done:

	/* give us a more useful (more correct?) error code */
	if ((NT_STATUS_EQUAL(result, NT_STATUS_DOMAIN_CONTROLLER_NOT_FOUND) ||
	    (NT_STATUS_EQUAL(result, NT_STATUS_UNSUCCESSFUL)))) {
		result = NT_STATUS_NO_LOGON_SERVERS;
	}

	if (state->request->flags & WBFLAG_PAM_NT_STATUS_SQUASH) {
		result = nt_status_squash(result);
	}

	set_auth_errors(state->response, result);

	DEBUG(NT_STATUS_IS_OK(result) ? 5 : 2,
	      ("NTLM CRAP authentication for user [%s]\\[%s] returned %s (PAM: %d)\n",
	       name_domain,
	       name_user,
	       state->response->data.auth.nt_status_string,
	       state->response->data.auth.pam_error));

	return NT_STATUS_IS_OK(result) ? WINBINDD_OK : WINBINDD_ERROR;
}

/* Change a user password */

void winbindd_pam_chauthtok(struct winbindd_cli_state *state)
{
	fstring domain, user;
	char *mapped_user;
	struct winbindd_domain *contact_domain;
	NTSTATUS nt_status = NT_STATUS_UNSUCCESSFUL;

	DEBUG(3, ("[%5lu]: pam chauthtok %s\n", (unsigned long)state->pid,
		state->request->data.chauthtok.user));

	/* Setup crap */

	nt_status = normalize_name_unmap(state->mem_ctx,
					 state->request->data.chauthtok.user,
					 &mapped_user);

	/* Update the chauthtok name if we did any mapping */

	if (NT_STATUS_IS_OK(nt_status) ||
	    NT_STATUS_EQUAL(nt_status, NT_STATUS_FILE_RENAMED))
	{
		fstrcpy(state->request->data.chauthtok.user, mapped_user);
	}

	/* Must pass in state->...chauthtok.user because
	   canonicalize_username() assumes an fstring().  Since
	   we have already copied it (if necessary), this is ok. */

	if (!canonicalize_username(state->request->data.chauthtok.user, domain, user)) {
		set_auth_errors(state->response, NT_STATUS_NO_SUCH_USER);
		DEBUG(5, ("winbindd_pam_chauthtok: canonicalize_username %s failed with %s"
			  "(PAM: %d)\n",
			  state->request->data.auth.user,
			  state->response->data.auth.nt_status_string,
			  state->response->data.auth.pam_error));
		request_error(state);
		return;
	}

	contact_domain = find_domain_from_name(domain);
	if (!contact_domain) {
		set_auth_errors(state->response, NT_STATUS_NO_SUCH_USER);
		DEBUG(3, ("Cannot change password for [%s] -> [%s]\\[%s] as %s is not a trusted domain\n",
			  state->request->data.chauthtok.user, domain, user, domain));
		request_error(state);
		return;
	}

	sendto_domain(state, contact_domain);
}

enum winbindd_result winbindd_dual_pam_chauthtok(struct winbindd_domain *contact_domain,
						 struct winbindd_cli_state *state)
{
	char *oldpass;
	char *newpass = NULL;
	struct policy_handle dom_pol;
	struct rpc_pipe_client *cli;
	bool got_info = false;
	struct samr_DomInfo1 *info = NULL;
	struct samr_ChangeReject *reject = NULL;
	NTSTATUS result = NT_STATUS_UNSUCCESSFUL;
	fstring domain, user;

	DEBUG(3, ("[%5lu]: dual pam chauthtok %s\n", (unsigned long)state->pid,
		  state->request->data.auth.user));

	if (!parse_domain_user(state->request->data.chauthtok.user, domain, user)) {
		goto done;
	}

	/* Change password */

	oldpass = state->request->data.chauthtok.oldpass;
	newpass = state->request->data.chauthtok.newpass;

	/* Initialize reject reason */
	state->response->data.auth.reject_reason = Undefined;

	/* Get sam handle */

	result = cm_connect_sam(contact_domain, state->mem_ctx, &cli,
				&dom_pol);
	if (!NT_STATUS_IS_OK(result)) {
		DEBUG(1, ("could not get SAM handle on DC for %s\n", domain));
		goto done;
	}

	result = rpccli_samr_chgpasswd_user3(cli, state->mem_ctx,
					     user,
					     newpass,
					     oldpass,
					     &info,
					     &reject);

 	/* Windows 2003 returns NT_STATUS_PASSWORD_RESTRICTION */

	if (NT_STATUS_EQUAL(result, NT_STATUS_PASSWORD_RESTRICTION) ) {

		fill_in_password_policy(state->response, info);

		state->response->data.auth.reject_reason =
			reject->reason;

		got_info = true;
	}

	/* atm the pidl generated rpccli_samr_ChangePasswordUser3 function will
	 * return with NT_STATUS_BUFFER_TOO_SMALL for w2k dcs as w2k just
	 * returns with 4byte error code (NT_STATUS_NOT_SUPPORTED) which is too
	 * short to comply with the samr_ChangePasswordUser3 idl - gd */

	/* only fallback when the chgpasswd_user3 call is not supported */
	if ((NT_STATUS_EQUAL(result, NT_STATUS(DCERPC_FAULT_OP_RNG_ERROR))) ||
		   (NT_STATUS_EQUAL(result, NT_STATUS_NOT_SUPPORTED)) ||
		   (NT_STATUS_EQUAL(result, NT_STATUS_BUFFER_TOO_SMALL)) ||
		   (NT_STATUS_EQUAL(result, NT_STATUS_NOT_IMPLEMENTED))) {

		DEBUG(10,("Password change with chgpasswd_user3 failed with: %s, retrying chgpasswd_user2\n",
			nt_errstr(result)));

		result = rpccli_samr_chgpasswd_user2(cli, state->mem_ctx, user, newpass, oldpass);

		/* Windows 2000 returns NT_STATUS_ACCOUNT_RESTRICTION.
		   Map to the same status code as Windows 2003. */

		if ( NT_STATUS_EQUAL(NT_STATUS_ACCOUNT_RESTRICTION, result ) ) {
			result = NT_STATUS_PASSWORD_RESTRICTION;
		}
	}

done:

	if (NT_STATUS_IS_OK(result) && (state->request->flags & WBFLAG_PAM_CACHED_LOGIN)) {

		/* Update the single sign-on memory creds. */
		result = winbindd_replace_memory_creds(state->request->data.chauthtok.user,
							newpass);

		/* When we login from gdm or xdm and password expires,
		 * we change password, but there are no memory crendentials
		 * So, winbindd_replace_memory_creds() returns
		 * NT_STATUS_OBJECT_NAME_NOT_FOUND. This is not a failure.
		 * --- BoYang
		 * */
		if (NT_STATUS_EQUAL(result, NT_STATUS_OBJECT_NAME_NOT_FOUND)) {
			result = NT_STATUS_OK;
		}

		if (!NT_STATUS_IS_OK(result)) {
			DEBUG(10,("Failed to replace memory creds: %s\n", nt_errstr(result)));
			goto process_result;
		}

		if (lp_winbind_offline_logon()) {
			result = winbindd_update_creds_by_name(contact_domain,
							 state->mem_ctx, user,
							 newpass);
			/* Again, this happens when we login from gdm or xdm
			 * and the password expires, *BUT* cached crendentials
			 * doesn't exist. winbindd_update_creds_by_name()
			 * returns NT_STATUS_NO_SUCH_USER.
			 * This is not a failure.
			 * --- BoYang
			 * */
			if (NT_STATUS_EQUAL(result, NT_STATUS_NO_SUCH_USER)) {
				result = NT_STATUS_OK;
			}

			if (!NT_STATUS_IS_OK(result)) {
				DEBUG(10,("Failed to store creds: %s\n", nt_errstr(result)));
				goto process_result;
			}
		}
	}

	if (!NT_STATUS_IS_OK(result) && !got_info && contact_domain) {

		NTSTATUS policy_ret;

		policy_ret = fillup_password_policy(contact_domain, state);

		/* failure of this is non critical, it will just provide no
		 * additional information to the client why the change has
		 * failed - Guenther */

		if (!NT_STATUS_IS_OK(policy_ret)) {
			DEBUG(10,("Failed to get password policies: %s\n", nt_errstr(policy_ret)));
			goto process_result;
		}
	}

process_result:

	set_auth_errors(state->response, result);

	DEBUG(NT_STATUS_IS_OK(result) ? 5 : 2,
	      ("Password change for user [%s]\\[%s] returned %s (PAM: %d)\n",
	       domain,
	       user,
	       state->response->data.auth.nt_status_string,
	       state->response->data.auth.pam_error));

	return NT_STATUS_IS_OK(result) ? WINBINDD_OK : WINBINDD_ERROR;
}

void winbindd_pam_logoff(struct winbindd_cli_state *state)
{
	struct winbindd_domain *domain;
	fstring name_domain, user;
	uid_t caller_uid = (uid_t)-1;
	uid_t request_uid = state->request->data.logoff.uid;

	DEBUG(3, ("[%5lu]: pam logoff %s\n", (unsigned long)state->pid,
		state->request->data.logoff.user));

	/* Ensure null termination */
	state->request->data.logoff.user
		[sizeof(state->request->data.logoff.user)-1]='\0';

	state->request->data.logoff.krb5ccname
		[sizeof(state->request->data.logoff.krb5ccname)-1]='\0';

	if (request_uid == (gid_t)-1) {
		goto failed;
	}

	if (!canonicalize_username(state->request->data.logoff.user, name_domain, user)) {
		goto failed;
	}

	if ((domain = find_auth_domain(state->request->flags,
				       name_domain)) == NULL) {
		goto failed;
	}

	if ((sys_getpeereid(state->sock, &caller_uid)) != 0) {
		DEBUG(1,("winbindd_pam_logoff: failed to check peerid: %s\n",
			strerror(errno)));
		goto failed;
	}

	switch (caller_uid) {
		case -1:
			goto failed;
		case 0:
			/* root must be able to logoff any user - gd */
			state->request->data.logoff.uid = request_uid;
			break;
		default:
			if (caller_uid != request_uid) {
				DEBUG(1,("winbindd_pam_logoff: caller requested invalid uid\n"));
				goto failed;
			}
			state->request->data.logoff.uid = caller_uid;
			break;
	}

	sendto_domain(state, domain);
	return;

 failed:
	set_auth_errors(state->response, NT_STATUS_NO_SUCH_USER);
	DEBUG(5, ("Pam Logoff for %s returned %s "
		  "(PAM: %d)\n",
		  state->request->data.logoff.user,
		  state->response->data.auth.nt_status_string,
		  state->response->data.auth.pam_error));
	request_error(state);
	return;
}

enum winbindd_result winbindd_dual_pam_logoff(struct winbindd_domain *domain,
					      struct winbindd_cli_state *state)
{
	NTSTATUS result = NT_STATUS_NOT_SUPPORTED;

	DEBUG(3, ("[%5lu]: pam dual logoff %s\n", (unsigned long)state->pid,
		state->request->data.logoff.user));

	if (!(state->request->flags & WBFLAG_PAM_KRB5)) {
		result = NT_STATUS_OK;
		goto process_result;
	}

	if (state->request->data.logoff.krb5ccname[0] == '\0') {
		result = NT_STATUS_OK;
		goto process_result;
	}

#ifdef HAVE_KRB5

	if (state->request->data.logoff.uid < 0) {
		DEBUG(0,("winbindd_pam_logoff: invalid uid\n"));
		goto process_result;
	}

	/* what we need here is to find the corresponding krb5 ccache name *we*
	 * created for a given username and destroy it */

	if (!ccache_entry_exists(state->request->data.logoff.user)) {
		result = NT_STATUS_OK;
		DEBUG(10,("winbindd_pam_logoff: no entry found.\n"));
		goto process_result;
	}

	if (!ccache_entry_identical(state->request->data.logoff.user,
					state->request->data.logoff.uid,
					state->request->data.logoff.krb5ccname)) {
		DEBUG(0,("winbindd_pam_logoff: cached entry differs.\n"));
		goto process_result;
	}

	result = remove_ccache(state->request->data.logoff.user);
	if (!NT_STATUS_IS_OK(result)) {
		DEBUG(0,("winbindd_pam_logoff: failed to remove ccache: %s\n",
			nt_errstr(result)));
		goto process_result;
	}

#else
	result = NT_STATUS_NOT_SUPPORTED;
#endif

process_result:

	winbindd_delete_memory_creds(state->request->data.logoff.user);

	set_auth_errors(state->response, result);

	return NT_STATUS_IS_OK(result) ? WINBINDD_OK : WINBINDD_ERROR;
}

/* Change user password with auth crap*/

void winbindd_pam_chng_pswd_auth_crap(struct winbindd_cli_state *state)
{
	struct winbindd_domain *domain = NULL;
	const char *domain_name = NULL;

	/* Ensure null termination */
	state->request->data.chng_pswd_auth_crap.user[
		sizeof(state->request->data.chng_pswd_auth_crap.user)-1]=0;
	state->request->data.chng_pswd_auth_crap.domain[
		sizeof(state->request->data.chng_pswd_auth_crap.domain)-1]=0;

	DEBUG(3, ("[%5lu]: pam change pswd auth crap domain: %s user: %s\n",
		  (unsigned long)state->pid,
		  state->request->data.chng_pswd_auth_crap.domain,
		  state->request->data.chng_pswd_auth_crap.user));

	if (*state->request->data.chng_pswd_auth_crap.domain != '\0') {
		domain_name = state->request->data.chng_pswd_auth_crap.domain;
	} else if (lp_winbind_use_default_domain()) {
		domain_name = lp_workgroup();
	}

	if (domain_name != NULL)
		domain = find_domain_from_name(domain_name);

	if (domain != NULL) {
		DEBUG(7, ("[%5lu]: pam auth crap changing pswd in domain: "
			  "%s\n", (unsigned long)state->pid,domain->name));
		sendto_domain(state, domain);
		return;
	}

	set_auth_errors(state->response, NT_STATUS_NO_SUCH_USER);
	DEBUG(5, ("CRAP change password  for %s\\%s returned %s (PAM: %d)\n",
		  state->request->data.chng_pswd_auth_crap.domain,
		  state->request->data.chng_pswd_auth_crap.user,
		  state->response->data.auth.nt_status_string,
		  state->response->data.auth.pam_error));
	request_error(state);
	return;
}

enum winbindd_result winbindd_dual_pam_chng_pswd_auth_crap(struct winbindd_domain *domainSt, struct winbindd_cli_state *state)
{
	NTSTATUS result;
	DATA_BLOB new_nt_password;
	DATA_BLOB old_nt_hash_enc;
	DATA_BLOB new_lm_password;
	DATA_BLOB old_lm_hash_enc;
	fstring  domain,user;
	struct policy_handle dom_pol;
	struct winbindd_domain *contact_domain = domainSt;
	struct rpc_pipe_client *cli;

	/* Ensure null termination */
	state->request->data.chng_pswd_auth_crap.user[
		sizeof(state->request->data.chng_pswd_auth_crap.user)-1]=0;
	state->request->data.chng_pswd_auth_crap.domain[
		sizeof(state->request->data.chng_pswd_auth_crap.domain)-1]=0;
	*domain = 0;
	*user = 0;

	DEBUG(3, ("[%5lu]: pam change pswd auth crap domain: %s user: %s\n",
		  (unsigned long)state->pid,
		  state->request->data.chng_pswd_auth_crap.domain,
		  state->request->data.chng_pswd_auth_crap.user));

	if (lp_winbind_offline_logon()) {
		DEBUG(0,("Refusing password change as winbind offline logons are enabled. "));
		DEBUGADD(0,("Changing passwords here would risk inconsistent logons\n"));
		result = NT_STATUS_ACCESS_DENIED;
		goto done;
	}

	if (*state->request->data.chng_pswd_auth_crap.domain) {
		fstrcpy(domain,state->request->data.chng_pswd_auth_crap.domain);
	} else {
		parse_domain_user(state->request->data.chng_pswd_auth_crap.user,
				  domain, user);

		if(!*domain) {
			DEBUG(3,("no domain specified with username (%s) - "
				 "failing auth\n",
				 state->request->data.chng_pswd_auth_crap.user));
			result = NT_STATUS_NO_SUCH_USER;
			goto done;
		}
	}

	if (!*domain && lp_winbind_use_default_domain()) {
		fstrcpy(domain,(char *)lp_workgroup());
	}

	if(!*user) {
		fstrcpy(user, state->request->data.chng_pswd_auth_crap.user);
	}

	DEBUG(3, ("[%5lu]: pam auth crap domain: %s user: %s\n",
		  (unsigned long)state->pid, domain, user));

	/* Change password */
	new_nt_password = data_blob_talloc(
		state->mem_ctx,
		state->request->data.chng_pswd_auth_crap.new_nt_pswd,
		state->request->data.chng_pswd_auth_crap.new_nt_pswd_len);

	old_nt_hash_enc = data_blob_talloc(
		state->mem_ctx,
		state->request->data.chng_pswd_auth_crap.old_nt_hash_enc,
		state->request->data.chng_pswd_auth_crap.old_nt_hash_enc_len);

	if(state->request->data.chng_pswd_auth_crap.new_lm_pswd_len > 0)	{
		new_lm_password = data_blob_talloc(
			state->mem_ctx,
			state->request->data.chng_pswd_auth_crap.new_lm_pswd,
			state->request->data.chng_pswd_auth_crap.new_lm_pswd_len);

		old_lm_hash_enc = data_blob_talloc(
			state->mem_ctx,
			state->request->data.chng_pswd_auth_crap.old_lm_hash_enc,
			state->request->data.chng_pswd_auth_crap.old_lm_hash_enc_len);
	} else {
		new_lm_password.length = 0;
		old_lm_hash_enc.length = 0;
	}

	/* Get sam handle */

	result = cm_connect_sam(contact_domain, state->mem_ctx, &cli, &dom_pol);
	if (!NT_STATUS_IS_OK(result)) {
		DEBUG(1, ("could not get SAM handle on DC for %s\n", domain));
		goto done;
	}

	result = rpccli_samr_chng_pswd_auth_crap(
		cli, state->mem_ctx, user, new_nt_password, old_nt_hash_enc,
		new_lm_password, old_lm_hash_enc);

 done:

	set_auth_errors(state->response, result);

	DEBUG(NT_STATUS_IS_OK(result) ? 5 : 2,
	      ("Password change for user [%s]\\[%s] returned %s (PAM: %d)\n",
	       domain, user,
	       state->response->data.auth.nt_status_string,
	       state->response->data.auth.pam_error));

	return NT_STATUS_IS_OK(result) ? WINBINDD_OK : WINBINDD_ERROR;
}
