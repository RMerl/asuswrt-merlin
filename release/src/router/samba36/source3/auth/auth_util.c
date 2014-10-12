/*
   Unix SMB/CIFS implementation.
   Authentication utility functions
   Copyright (C) Andrew Tridgell 1992-1998
   Copyright (C) Andrew Bartlett 2001
   Copyright (C) Jeremy Allison 2000-2001
   Copyright (C) Rafal Szczesniak 2002
   Copyright (C) Volker Lendecke 2006

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
#include "auth.h"
#include "../libcli/auth/libcli_auth.h"
#include "../lib/crypto/arcfour.h"
#include "rpc_client/init_lsa.h"
#include "../libcli/security/security.h"
#include "../lib/util/util_pw.h"
#include "lib/winbind_util.h"
#include "passdb.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_AUTH

/****************************************************************************
 Create a UNIX user on demand.
****************************************************************************/

static int _smb_create_user(const char *domain, const char *unix_username, const char *homedir)
{
	TALLOC_CTX *ctx = talloc_tos();
	char *add_script;
	int ret;

	add_script = talloc_strdup(ctx, lp_adduser_script());
	if (!add_script || !*add_script) {
		return -1;
	}
	add_script = talloc_all_string_sub(ctx,
				add_script,
				"%u",
				unix_username);
	if (!add_script) {
		return -1;
	}
	if (domain) {
		add_script = talloc_all_string_sub(ctx,
					add_script,
					"%D",
					domain);
		if (!add_script) {
			return -1;
		}
	}
	if (homedir) {
		add_script = talloc_all_string_sub(ctx,
				add_script,
				"%H",
				homedir);
		if (!add_script) {
			return -1;
		}
	}
	ret = smbrun(add_script,NULL);
	flush_pwnam_cache();
	DEBUG(ret ? 0 : 3,
		("smb_create_user: Running the command `%s' gave %d\n",
		 add_script,ret));
	return ret;
}

/****************************************************************************
 Create an auth_usersupplied_data structure after appropriate mapping.
****************************************************************************/

NTSTATUS make_user_info_map(struct auth_usersupplied_info **user_info,
			    const char *smb_name,
			    const char *client_domain,
			    const char *workstation_name,
			    DATA_BLOB *lm_pwd,
			    DATA_BLOB *nt_pwd,
			    const struct samr_Password *lm_interactive_pwd,
			    const struct samr_Password *nt_interactive_pwd,
			    const char *plaintext,
			    enum auth_password_state password_state)
{
	const char *domain;
	NTSTATUS result;
	bool was_mapped;
	char *internal_username = NULL;

	was_mapped = map_username(talloc_tos(), smb_name, &internal_username);
	if (!internal_username) {
		return NT_STATUS_NO_MEMORY;
	}

	DEBUG(5, ("Mapping user [%s]\\[%s] from workstation [%s]\n",
		 client_domain, smb_name, workstation_name));

	domain = client_domain;

	/* If you connect to a Windows domain member using a bogus domain name,
	 * the Windows box will map the BOGUS\user to SAMNAME\user.  Thus, if
	 * the Windows box is a DC the name will become DOMAIN\user and be
	 * authenticated against AD, if the Windows box is a member server but
	 * not a DC the name will become WORKSTATION\user.  A standalone
	 * non-domain member box will also map to WORKSTATION\user.
	 * This also deals with the client passing in a "" domain */

	if (!is_trusted_domain(domain) &&
	    !strequal(domain, my_sam_name()) &&
	    !strequal(domain, get_global_sam_name()))
	{
		if (lp_map_untrusted_to_domain())
			domain = my_sam_name();
		else
			domain = get_global_sam_name();
		DEBUG(5, ("Mapped domain from [%s] to [%s] for user [%s] from "
			  "workstation [%s]\n",
			  client_domain, domain, smb_name, workstation_name));
	}

	/* We know that the given domain is trusted (and we are allowing them),
	 * it is our global SAM name, or for legacy behavior it is our
	 * primary domain name */

	result = make_user_info(user_info, smb_name, internal_username,
			      client_domain, domain, workstation_name,
			      lm_pwd, nt_pwd,
			      lm_interactive_pwd, nt_interactive_pwd,
			      plaintext, password_state);
	if (NT_STATUS_IS_OK(result)) {
		/* We have tried mapping */
		(*user_info)->mapped_state = True;
		/* did we actually map the user to a different name? */
		(*user_info)->was_mapped = was_mapped;
	}
	return result;
}

/****************************************************************************
 Create an auth_usersupplied_data, making the DATA_BLOBs here. 
 Decrypt and encrypt the passwords.
****************************************************************************/

bool make_user_info_netlogon_network(struct auth_usersupplied_info **user_info,
				     const char *smb_name, 
				     const char *client_domain, 
				     const char *workstation_name,
				     uint32 logon_parameters,
				     const uchar *lm_network_pwd,
				     int lm_pwd_len,
				     const uchar *nt_network_pwd,
				     int nt_pwd_len)
{
	bool ret;
	NTSTATUS status;
	DATA_BLOB lm_blob = data_blob(lm_network_pwd, lm_pwd_len);
	DATA_BLOB nt_blob = data_blob(nt_network_pwd, nt_pwd_len);

	status = make_user_info_map(user_info,
				    smb_name, client_domain, 
				    workstation_name,
				    lm_pwd_len ? &lm_blob : NULL, 
				    nt_pwd_len ? &nt_blob : NULL,
				    NULL, NULL, NULL,
				    AUTH_PASSWORD_RESPONSE);

	if (NT_STATUS_IS_OK(status)) {
		(*user_info)->logon_parameters = logon_parameters;
	}
	ret = NT_STATUS_IS_OK(status) ? True : False;

	data_blob_free(&lm_blob);
	data_blob_free(&nt_blob);
	return ret;
}

/****************************************************************************
 Create an auth_usersupplied_data, making the DATA_BLOBs here. 
 Decrypt and encrypt the passwords.
****************************************************************************/

bool make_user_info_netlogon_interactive(struct auth_usersupplied_info **user_info,
					 const char *smb_name, 
					 const char *client_domain, 
					 const char *workstation_name,
					 uint32 logon_parameters,
					 const uchar chal[8], 
					 const uchar lm_interactive_pwd[16], 
					 const uchar nt_interactive_pwd[16], 
					 const uchar *dc_sess_key)
{
	struct samr_Password lm_pwd;
	struct samr_Password nt_pwd;
	unsigned char local_lm_response[24];
	unsigned char local_nt_response[24];
	unsigned char key[16];

	memcpy(key, dc_sess_key, 16);

	if (lm_interactive_pwd)
		memcpy(lm_pwd.hash, lm_interactive_pwd, sizeof(lm_pwd.hash));

	if (nt_interactive_pwd)
		memcpy(nt_pwd.hash, nt_interactive_pwd, sizeof(nt_pwd.hash));

#ifdef DEBUG_PASSWORD
	DEBUG(100,("key:"));
	dump_data(100, key, sizeof(key));

	DEBUG(100,("lm owf password:"));
	dump_data(100, lm_pwd.hash, sizeof(lm_pwd.hash));

	DEBUG(100,("nt owf password:"));
	dump_data(100, nt_pwd.hash, sizeof(nt_pwd.hash));
#endif

	if (lm_interactive_pwd)
		arcfour_crypt(lm_pwd.hash, key, sizeof(lm_pwd.hash));

	if (nt_interactive_pwd)
		arcfour_crypt(nt_pwd.hash, key, sizeof(nt_pwd.hash));

#ifdef DEBUG_PASSWORD
	DEBUG(100,("decrypt of lm owf password:"));
	dump_data(100, lm_pwd.hash, sizeof(lm_pwd));

	DEBUG(100,("decrypt of nt owf password:"));
	dump_data(100, nt_pwd.hash, sizeof(nt_pwd));
#endif

	if (lm_interactive_pwd)
		SMBOWFencrypt(lm_pwd.hash, chal,
			      local_lm_response);

	if (nt_interactive_pwd)
		SMBOWFencrypt(nt_pwd.hash, chal,
			      local_nt_response);

	/* Password info paranoia */
	ZERO_STRUCT(key);

	{
		bool ret;
		NTSTATUS nt_status;
		DATA_BLOB local_lm_blob;
		DATA_BLOB local_nt_blob;

		if (lm_interactive_pwd) {
			local_lm_blob = data_blob(local_lm_response,
						  sizeof(local_lm_response));
		}

		if (nt_interactive_pwd) {
			local_nt_blob = data_blob(local_nt_response,
						  sizeof(local_nt_response));
		}

		nt_status = make_user_info_map(
			user_info, 
			smb_name, client_domain, workstation_name,
			lm_interactive_pwd ? &local_lm_blob : NULL,
			nt_interactive_pwd ? &local_nt_blob : NULL,
			lm_interactive_pwd ? &lm_pwd : NULL,
			nt_interactive_pwd ? &nt_pwd : NULL,
			NULL, AUTH_PASSWORD_HASH);

		if (NT_STATUS_IS_OK(nt_status)) {
			(*user_info)->logon_parameters = logon_parameters;
		}

		ret = NT_STATUS_IS_OK(nt_status) ? True : False;
		data_blob_free(&local_lm_blob);
		data_blob_free(&local_nt_blob);
		return ret;
	}
}


/****************************************************************************
 Create an auth_usersupplied_data structure
****************************************************************************/

bool make_user_info_for_reply(struct auth_usersupplied_info **user_info,
			      const char *smb_name, 
			      const char *client_domain,
			      const uint8 chal[8],
			      DATA_BLOB plaintext_password)
{

	DATA_BLOB local_lm_blob;
	DATA_BLOB local_nt_blob;
	NTSTATUS ret = NT_STATUS_UNSUCCESSFUL;
	char *plaintext_password_string;
	/*
	 * Not encrypted - do so.
	 */

	DEBUG(5,("make_user_info_for_reply: User passwords not in encrypted "
		 "format.\n"));
	if (plaintext_password.data && plaintext_password.length) {
		unsigned char local_lm_response[24];

#ifdef DEBUG_PASSWORD
		DEBUG(10,("Unencrypted password (len %d):\n",
			  (int)plaintext_password.length));
		dump_data(100, plaintext_password.data,
			  plaintext_password.length);
#endif

		SMBencrypt( (const char *)plaintext_password.data,
			    (const uchar*)chal, local_lm_response);
		local_lm_blob = data_blob(local_lm_response, 24);

		/* We can't do an NT hash here, as the password needs to be
		   case insensitive */
		local_nt_blob = data_blob_null; 
	} else {
		local_lm_blob = data_blob_null; 
		local_nt_blob = data_blob_null; 
	}

	plaintext_password_string = talloc_strndup(talloc_tos(),
						   (const char *)plaintext_password.data,
						   plaintext_password.length);
	if (!plaintext_password_string) {
		return False;
	}

	ret = make_user_info_map(
		user_info, smb_name, client_domain, 
		get_remote_machine_name(),
		local_lm_blob.data ? &local_lm_blob : NULL,
		local_nt_blob.data ? &local_nt_blob : NULL,
		NULL, NULL,
		plaintext_password_string,
		AUTH_PASSWORD_PLAIN);

	if (plaintext_password_string) {
		memset(plaintext_password_string, '\0', strlen(plaintext_password_string));
		talloc_free(plaintext_password_string);
	}

	data_blob_free(&local_lm_blob);
	return NT_STATUS_IS_OK(ret) ? True : False;
}

/****************************************************************************
 Create an auth_usersupplied_data structure
****************************************************************************/

NTSTATUS make_user_info_for_reply_enc(struct auth_usersupplied_info **user_info,
                                      const char *smb_name,
                                      const char *client_domain, 
                                      DATA_BLOB lm_resp, DATA_BLOB nt_resp)
{
	return make_user_info_map(user_info, smb_name, 
				  client_domain, 
				  get_remote_machine_name(), 
				  lm_resp.data && (lm_resp.length > 0) ? &lm_resp : NULL,
				  nt_resp.data && (nt_resp.length > 0) ? &nt_resp : NULL,
				  NULL, NULL, NULL,
				  AUTH_PASSWORD_RESPONSE);
}

/****************************************************************************
 Create a guest user_info blob, for anonymous authenticaion.
****************************************************************************/

bool make_user_info_guest(struct auth_usersupplied_info **user_info)
{
	NTSTATUS nt_status;

	nt_status = make_user_info(user_info, 
				   "","", 
				   "","", 
				   "", 
				   NULL, NULL, 
				   NULL, NULL, 
				   NULL,
				   AUTH_PASSWORD_RESPONSE);

	return NT_STATUS_IS_OK(nt_status) ? True : False;
}

static NTSTATUS log_nt_token(struct security_token *token)
{
	TALLOC_CTX *frame = talloc_stackframe();
	char *command;
	char *group_sidstr;
	size_t i;

	if ((lp_log_nt_token_command() == NULL) ||
	    (strlen(lp_log_nt_token_command()) == 0)) {
		TALLOC_FREE(frame);
		return NT_STATUS_OK;
	}

	group_sidstr = talloc_strdup(frame, "");
	for (i=1; i<token->num_sids; i++) {
		group_sidstr = talloc_asprintf(
			frame, "%s %s", group_sidstr,
			sid_string_talloc(frame, &token->sids[i]));
	}

	command = talloc_string_sub(
		frame, lp_log_nt_token_command(),
		"%s", sid_string_talloc(frame, &token->sids[0]));
	command = talloc_string_sub(frame, command, "%t", group_sidstr);

	if (command == NULL) {
		TALLOC_FREE(frame);
		return NT_STATUS_NO_MEMORY;
	}

	DEBUG(8, ("running command: [%s]\n", command));
	if (smbrun(command, NULL) != 0) {
		DEBUG(0, ("Could not log NT token\n"));
		TALLOC_FREE(frame);
		return NT_STATUS_ACCESS_DENIED;
	}

	TALLOC_FREE(frame);
	return NT_STATUS_OK;
}

/*
 * Create the token to use from server_info->info3 and
 * server_info->sids (the info3/sam groups). Find the unix gids.
 */

NTSTATUS create_local_token(struct auth_serversupplied_info *server_info)
{
	struct security_token *t;
	NTSTATUS status;
	size_t i;
	struct dom_sid tmp_sid;
	struct wbcUnixId *ids;

	/*
	 * If winbind is not around, we can not make much use of the SIDs the
	 * domain controller provided us with. Likewise if the user name was
	 * mapped to some local unix user.
	 */

	if (((lp_server_role() == ROLE_DOMAIN_MEMBER) && !winbind_ping()) ||
	    (server_info->nss_token)) {
		status = create_token_from_username(server_info,
						    server_info->unix_name,
						    server_info->guest,
						    &server_info->utok.uid,
						    &server_info->utok.gid,
						    &server_info->unix_name,
						    &server_info->security_token);

	} else {
		status = create_local_nt_token_from_info3(server_info,
							  server_info->guest,
							  server_info->info3,
							  &server_info->extra,
							  &server_info->security_token);
	}

	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	/* Convert the SIDs to gids. */

	server_info->utok.ngroups = 0;
	server_info->utok.groups = NULL;

	t = server_info->security_token;

	ids = TALLOC_ARRAY(talloc_tos(), struct wbcUnixId,
			   t->num_sids);
	if (ids == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	if (!sids_to_unix_ids(t->sids, t->num_sids, ids)) {
		TALLOC_FREE(ids);
		return NT_STATUS_NO_MEMORY;
	}

	/* Start at index 1, where the groups start. */

	for (i=1; i<t->num_sids; i++) {

		if (ids[i].type != WBC_ID_TYPE_GID) {
			DEBUG(10, ("Could not convert SID %s to gid, "
				   "ignoring it\n",
				   sid_string_dbg(&t->sids[i])));
			continue;
		}
		if (!add_gid_to_array_unique(server_info, ids[i].id.gid,
					     &server_info->utok.groups,
					     &server_info->utok.ngroups)) {
			return NT_STATUS_NO_MEMORY;
		}
	}

	/*
	 * Add the "Unix Group" SID for each gid to catch mapped groups
	 * and their Unix equivalent.  This is to solve the backwards
	 * compatibility problem of 'valid users = +ntadmin' where
	 * ntadmin has been paired with "Domain Admins" in the group
	 * mapping table.  Otherwise smb.conf would need to be changed
	 * to 'valid user = "Domain Admins"'.  --jerry
	 *
	 * For consistency we also add the "Unix User" SID,
	 * so that the complete unix token is represented within
	 * the nt token.
	 */

	uid_to_unix_users_sid(server_info->utok.uid, &tmp_sid);

	add_sid_to_array_unique(server_info->security_token, &tmp_sid,
				&server_info->security_token->sids,
				&server_info->security_token->num_sids);

	for ( i=0; i<server_info->utok.ngroups; i++ ) {
		gid_to_unix_groups_sid(server_info->utok.groups[i], &tmp_sid);
		add_sid_to_array_unique(server_info->security_token, &tmp_sid,
					&server_info->security_token->sids,
					&server_info->security_token->num_sids);
	}

	security_token_debug(DBGC_AUTH, 10, server_info->security_token);
	debug_unix_user_token(DBGC_AUTH, 10,
			      server_info->utok.uid,
			      server_info->utok.gid,
			      server_info->utok.ngroups,
			      server_info->utok.groups);

	status = log_nt_token(server_info->security_token);
	return status;
}

/***************************************************************************
 Make (and fill) a server_info struct from a 'struct passwd' by conversion
 to a struct samu
***************************************************************************/

NTSTATUS make_server_info_pw(struct auth_serversupplied_info **server_info,
                             char *unix_username,
			     struct passwd *pwd)
{
	NTSTATUS status;
	struct samu *sampass = NULL;
	char *qualified_name = NULL;
	TALLOC_CTX *mem_ctx = NULL;
	struct dom_sid u_sid;
	enum lsa_SidType type;
	struct auth_serversupplied_info *result;

	/*
	 * The SID returned in server_info->sam_account is based
	 * on our SAM sid even though for a pure UNIX account this should
	 * not be the case as it doesn't really exist in the SAM db.
	 * This causes lookups on "[in]valid users" to fail as they
	 * will lookup this name as a "Unix User" SID to check against
	 * the user token. Fix this by adding the "Unix User"\unix_username
	 * SID to the sid array. The correct fix should probably be
	 * changing the server_info->sam_account user SID to be a
	 * S-1-22 Unix SID, but this might break old configs where
	 * plaintext passwords were used with no SAM backend.
	 */

	mem_ctx = talloc_init("make_server_info_pw_tmp");
	if (!mem_ctx) {
		return NT_STATUS_NO_MEMORY;
	}

	qualified_name = talloc_asprintf(mem_ctx, "%s\\%s",
					unix_users_domain_name(),
					unix_username );
	if (!qualified_name) {
		TALLOC_FREE(mem_ctx);
		return NT_STATUS_NO_MEMORY;
	}

	if (!lookup_name(mem_ctx, qualified_name, LOOKUP_NAME_ALL,
						NULL, NULL,
						&u_sid, &type)) {
		TALLOC_FREE(mem_ctx);
		return NT_STATUS_NO_SUCH_USER;
	}

	TALLOC_FREE(mem_ctx);

	if (type != SID_NAME_USER) {
		return NT_STATUS_NO_SUCH_USER;
	}

	if ( !(sampass = samu_new( NULL )) ) {
		return NT_STATUS_NO_MEMORY;
	}

	status = samu_set_unix( sampass, pwd );
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	/* In pathological cases the above call can set the account
	 * name to the DOMAIN\username form. Reset the account name
	 * using unix_username */
	pdb_set_username(sampass, unix_username, PDB_SET);

	/* set the user sid to be the calculated u_sid */
	pdb_set_user_sid(sampass, &u_sid, PDB_SET);

	result = make_server_info(NULL);
	if (result == NULL) {
		TALLOC_FREE(sampass);
		return NT_STATUS_NO_MEMORY;
	}

	status = samu_to_SamInfo3(result, sampass, global_myname(),
				  &result->info3, &result->extra);
	TALLOC_FREE(sampass);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(10, ("Failed to convert samu to info3: %s\n",
			   nt_errstr(status)));
		TALLOC_FREE(result);
		return status;
	}

	result->unix_name = talloc_strdup(result, unix_username);
	result->sanitized_username = sanitize_username(result, unix_username);

	if ((result->unix_name == NULL)
	    || (result->sanitized_username == NULL)) {
		TALLOC_FREE(result);
		return NT_STATUS_NO_MEMORY;
	}

	result->utok.uid = pwd->pw_uid;
	result->utok.gid = pwd->pw_gid;

	*server_info = result;

	return NT_STATUS_OK;
}

static NTSTATUS get_system_info3(TALLOC_CTX *mem_ctx,
				 struct passwd *pwd,
				 struct netr_SamInfo3 *info3)
{
	struct dom_sid domain_sid;
	const char *tmp;

	/* Set account name */
	tmp = talloc_strdup(mem_ctx, pwd->pw_name);
	if (tmp == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	init_lsa_String(&info3->base.account_name, tmp);

	/* Set domain name */
	tmp = talloc_strdup(mem_ctx, get_global_sam_name());
	if (tmp == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	init_lsa_StringLarge(&info3->base.domain, tmp);

	/* Domain sid */
	sid_copy(&domain_sid, get_global_sam_sid());

	info3->base.domain_sid = dom_sid_dup(mem_ctx, &domain_sid);
	if (info3->base.domain_sid == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	/* Admin rid */
	info3->base.rid = DOMAIN_RID_ADMINISTRATOR;

	/* Primary gid */
	info3->base.primary_gid = BUILTIN_RID_ADMINISTRATORS;

	return NT_STATUS_OK;
}

static NTSTATUS get_guest_info3(TALLOC_CTX *mem_ctx,
				struct netr_SamInfo3 *info3)
{
	const char *guest_account = lp_guestaccount();
	struct dom_sid domain_sid;
	struct passwd *pwd;
	const char *tmp;

	pwd = Get_Pwnam_alloc(mem_ctx, guest_account);
	if (pwd == NULL) {
		DEBUG(0,("SamInfo3_for_guest: Unable to locate guest "
			 "account [%s]!\n", guest_account));
		return NT_STATUS_NO_SUCH_USER;
	}

	/* Set acount name */
	tmp = talloc_strdup(mem_ctx, pwd->pw_name);
	if (tmp == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	init_lsa_String(&info3->base.account_name, tmp);

	/* Set domain name */
	tmp = talloc_strdup(mem_ctx, get_global_sam_name());
	if (tmp == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	init_lsa_StringLarge(&info3->base.domain, tmp);

	/* Domain sid */
	sid_copy(&domain_sid, get_global_sam_sid());

	info3->base.domain_sid = dom_sid_dup(mem_ctx, &domain_sid);
	if (info3->base.domain_sid == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	/* Guest rid */
	info3->base.rid = DOMAIN_RID_GUEST;

	/* Primary gid */
	info3->base.primary_gid = DOMAIN_RID_GUESTS;

	TALLOC_FREE(pwd);
	return NT_STATUS_OK;
}

/***************************************************************************
 Make (and fill) a user_info struct for a guest login.
 This *must* succeed for smbd to start. If there is no mapping entry for
 the guest gid, then create one.
***************************************************************************/

static NTSTATUS make_new_server_info_guest(struct auth_serversupplied_info **server_info)
{
	static const char zeros[16] = {0};
	const char *guest_account = lp_guestaccount();
	const char *domain = global_myname();
	struct netr_SamInfo3 info3;
	TALLOC_CTX *tmp_ctx;
	NTSTATUS status;
	fstring tmp;

	tmp_ctx = talloc_stackframe();
	if (tmp_ctx == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	ZERO_STRUCT(info3);

	status = get_guest_info3(tmp_ctx, &info3);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	status = make_server_info_info3(tmp_ctx,
					guest_account,
					domain,
					server_info,
					&info3);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	(*server_info)->guest = True;

	status = create_local_token(*server_info);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(10, ("create_local_token failed: %s\n",
			   nt_errstr(status)));
		goto done;
	}

	/* annoying, but the Guest really does have a session key, and it is
	   all zeros! */
	(*server_info)->user_session_key = data_blob(zeros, sizeof(zeros));
	(*server_info)->lm_session_key = data_blob(zeros, sizeof(zeros));

	alpha_strcpy(tmp, (*server_info)->info3->base.account_name.string,
		     ". _-$", sizeof(tmp));
	(*server_info)->sanitized_username = talloc_strdup(*server_info, tmp);

	status = NT_STATUS_OK;
done:
	TALLOC_FREE(tmp_ctx);
	return status;
}

/****************************************************************************
  Fake a auth_session_info just from a username (as a
  session_info structure, with create_local_token() already called on
  it.
****************************************************************************/

static NTSTATUS make_system_session_info_from_pw(TALLOC_CTX *mem_ctx,
						 struct passwd *pwd,
						 struct auth_serversupplied_info **server_info)
{
	const char *domain = global_myname();
	struct netr_SamInfo3 info3;
	TALLOC_CTX *tmp_ctx;
	NTSTATUS status;

	tmp_ctx = talloc_stackframe();
	if (tmp_ctx == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	ZERO_STRUCT(info3);

	status = get_system_info3(tmp_ctx, pwd, &info3);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("Failed creating system info3 with %s\n",
			  nt_errstr(status)));
		goto done;
	}

	status = make_server_info_info3(mem_ctx,
					pwd->pw_name,
					domain,
					server_info,
					&info3);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("make_server_info_info3 failed with %s\n",
			  nt_errstr(status)));
		goto done;
	}

	(*server_info)->nss_token = true;

	/* Now turn the server_info into a session_info with the full token etc */
	status = create_local_token(*server_info);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("create_local_token failed: %s\n",
			  nt_errstr(status)));
		goto done;
	}

	status = NT_STATUS_OK;
done:
	TALLOC_FREE(tmp_ctx);
	return status;
}

/***************************************************************************
 Make (and fill) a auth_session_info struct for a system user login.
 This *must* succeed for smbd to start.
***************************************************************************/

static NTSTATUS make_new_session_info_system(TALLOC_CTX *mem_ctx,
					    struct auth_serversupplied_info **session_info)
{
	struct passwd *pwd;
	NTSTATUS status;

	pwd = getpwuid_alloc(mem_ctx, sec_initial_uid());
	if (pwd == NULL) {
		return NT_STATUS_NO_SUCH_USER;
	}

	status = make_system_session_info_from_pw(mem_ctx,
						  pwd,
						  session_info);
	TALLOC_FREE(pwd);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	(*session_info)->system = true;

	status = add_sid_to_array_unique((*session_info)->security_token->sids,
					 &global_sid_System,
					 &(*session_info)->security_token->sids,
					 &(*session_info)->security_token->num_sids);
	if (!NT_STATUS_IS_OK(status)) {
		TALLOC_FREE((*session_info));
		return status;
	}

	return NT_STATUS_OK;
}

/****************************************************************************
  Fake a auth_serversupplied_info just from a username
****************************************************************************/

NTSTATUS make_serverinfo_from_username(TALLOC_CTX *mem_ctx,
				       const char *username,
				       bool use_guest_token,
				       bool is_guest,
				       struct auth_serversupplied_info **presult)
{
	struct auth_serversupplied_info *result;
	struct passwd *pwd;
	NTSTATUS status;

	pwd = Get_Pwnam_alloc(talloc_tos(), username);
	if (pwd == NULL) {
		return NT_STATUS_NO_SUCH_USER;
	}

	status = make_server_info_pw(&result, pwd->pw_name, pwd);

	TALLOC_FREE(pwd);

	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	result->nss_token = true;
	result->guest = is_guest;

	if (use_guest_token) {
		status = make_server_info_guest(mem_ctx, &result);
	} else {
		status = create_local_token(result);
	}

	if (!NT_STATUS_IS_OK(status)) {
		TALLOC_FREE(result);
		return status;
	}

	*presult = talloc_steal(mem_ctx, result);
	return NT_STATUS_OK;
}


struct auth_serversupplied_info *copy_serverinfo(TALLOC_CTX *mem_ctx,
						 const struct auth_serversupplied_info *src)
{
	struct auth_serversupplied_info *dst;

	dst = make_server_info(mem_ctx);
	if (dst == NULL) {
		return NULL;
	}

	dst->guest = src->guest;
	dst->system = src->system;
	dst->utok.uid = src->utok.uid;
	dst->utok.gid = src->utok.gid;
	dst->utok.ngroups = src->utok.ngroups;
	if (src->utok.ngroups != 0) {
		dst->utok.groups = (gid_t *)TALLOC_MEMDUP(
			dst, src->utok.groups,
			sizeof(gid_t)*dst->utok.ngroups);
	} else {
		dst->utok.groups = NULL;
	}

	if (src->security_token) {
		dst->security_token = dup_nt_token(dst, src->security_token);
		if (!dst->security_token) {
			TALLOC_FREE(dst);
			return NULL;
		}
	}

	dst->user_session_key = data_blob_talloc( dst, src->user_session_key.data,
						src->user_session_key.length);

	dst->lm_session_key = data_blob_talloc(dst, src->lm_session_key.data,
						src->lm_session_key.length);

	dst->info3 = copy_netr_SamInfo3(dst, src->info3);
	if (!dst->info3) {
		TALLOC_FREE(dst);
		return NULL;
	}
	dst->extra = src->extra;

	dst->unix_name = talloc_strdup(dst, src->unix_name);
	if (!dst->unix_name) {
		TALLOC_FREE(dst);
		return NULL;
	}

	dst->sanitized_username = talloc_strdup(dst, src->sanitized_username);
	if (!dst->sanitized_username) {
		TALLOC_FREE(dst);
		return NULL;
	}

	return dst;
}

/*
 * Set a new session key. Used in the rpc server where we have to override the
 * SMB level session key with SystemLibraryDTC
 */

bool session_info_set_session_key(struct auth_serversupplied_info *info,
				 DATA_BLOB session_key)
{
	TALLOC_FREE(info->user_session_key.data);

	info->user_session_key = data_blob_talloc(
		info, session_key.data, session_key.length);

	return (info->user_session_key.data != NULL);
}

static struct auth_serversupplied_info *guest_info = NULL;

bool init_guest_info(void)
{
	if (guest_info != NULL)
		return True;

	return NT_STATUS_IS_OK(make_new_server_info_guest(&guest_info));
}

NTSTATUS make_server_info_guest(TALLOC_CTX *mem_ctx,
				struct auth_serversupplied_info **server_info)
{
	*server_info = copy_serverinfo(mem_ctx, guest_info);
	return (*server_info != NULL) ? NT_STATUS_OK : NT_STATUS_NO_MEMORY;
}

static struct auth_serversupplied_info *system_info = NULL;

NTSTATUS init_system_info(void)
{
	if (system_info != NULL)
		return NT_STATUS_OK;

	return make_new_session_info_system(NULL, &system_info);
}

NTSTATUS make_session_info_system(TALLOC_CTX *mem_ctx,
				struct auth_serversupplied_info **session_info)
{
	if (system_info == NULL) return NT_STATUS_UNSUCCESSFUL;
	*session_info = copy_serverinfo(mem_ctx, system_info);
	return (*session_info != NULL) ? NT_STATUS_OK : NT_STATUS_NO_MEMORY;
}

const struct auth_serversupplied_info *get_session_info_system(void)
{
    return system_info;
}

bool copy_current_user(struct current_user *dst, struct current_user *src)
{
	gid_t *groups;
	struct security_token *nt_token;

	groups = (gid_t *)memdup(src->ut.groups,
				 sizeof(gid_t) * src->ut.ngroups);
	if ((src->ut.ngroups != 0) && (groups == NULL)) {
		return False;
	}

	nt_token = dup_nt_token(NULL, src->nt_user_token);
	if (nt_token == NULL) {
		SAFE_FREE(groups);
		return False;
	}

	dst->conn = src->conn;
	dst->vuid = src->vuid;
	dst->ut.uid = src->ut.uid;
	dst->ut.gid = src->ut.gid;
	dst->ut.ngroups = src->ut.ngroups;
	dst->ut.groups = groups;
	dst->nt_user_token = nt_token;
	return True;
}

/***************************************************************************
 Purely internal function for make_server_info_info3
***************************************************************************/

static NTSTATUS check_account(TALLOC_CTX *mem_ctx, const char *domain,
			      const char *username, char **found_username,
			      struct passwd **pwd,
			      bool *username_was_mapped)
{
	char *orig_dom_user = NULL;
	char *dom_user = NULL;
	char *lower_username = NULL;
	char *real_username = NULL;
	struct passwd *passwd;

	lower_username = talloc_strdup(mem_ctx, username);
	if (!lower_username) {
		return NT_STATUS_NO_MEMORY;
	}
	strlower_m( lower_username );

	orig_dom_user = talloc_asprintf(mem_ctx,
				"%s%c%s",
				domain,
				*lp_winbind_separator(),
				lower_username);
	if (!orig_dom_user) {
		return NT_STATUS_NO_MEMORY;
	}

	/* Get the passwd struct.  Try to create the account if necessary. */

	*username_was_mapped = map_username(mem_ctx, orig_dom_user, &dom_user);
	if (!dom_user) {
		return NT_STATUS_NO_MEMORY;
	}

	passwd = smb_getpwnam(mem_ctx, dom_user, &real_username, True );
	if (!passwd) {
		DEBUG(3, ("Failed to find authenticated user %s via "
			  "getpwnam(), denying access.\n", dom_user));
		return NT_STATUS_NO_SUCH_USER;
	}

	if (!real_username) {
		return NT_STATUS_NO_MEMORY;
	}

	*pwd = passwd;

	/* This is pointless -- there is no suport for differing 
	   unix and windows names.  Make sure to always store the 
	   one we actually looked up and succeeded. Have I mentioned
	   why I hate the 'winbind use default domain' parameter?   
	                                 --jerry              */

	*found_username = talloc_strdup( mem_ctx, real_username );

	return NT_STATUS_OK;
}

/****************************************************************************
 Wrapper to allow the getpwnam() call to strip the domain name and 
 try again in case a local UNIX user is already there.  Also run through 
 the username if we fallback to the username only.
 ****************************************************************************/

struct passwd *smb_getpwnam( TALLOC_CTX *mem_ctx, const char *domuser,
			     char **p_save_username, bool create )
{
	struct passwd *pw = NULL;
	char *p = NULL;
	char *username = NULL;

	/* we only save a copy of the username it has been mangled 
	   by winbindd use default domain */
	*p_save_username = NULL;

	/* don't call map_username() here since it has to be done higher 
	   up the stack so we don't call it multiple times */

	username = talloc_strdup(mem_ctx, domuser);
	if (!username) {
		return NULL;
	}

	p = strchr_m( username, *lp_winbind_separator() );

	/* code for a DOMAIN\user string */

	if ( p ) {
		pw = Get_Pwnam_alloc( mem_ctx, domuser );
		if ( pw ) {
			/* make sure we get the case of the username correct */
			/* work around 'winbind use default domain = yes' */

			if ( lp_winbind_use_default_domain() &&
			     !strchr_m( pw->pw_name, *lp_winbind_separator() ) ) {
				char *domain;

				/* split the domain and username into 2 strings */
				*p = '\0';
				domain = username;

				*p_save_username = talloc_asprintf(mem_ctx,
								"%s%c%s",
								domain,
								*lp_winbind_separator(),
								pw->pw_name);
				if (!*p_save_username) {
					TALLOC_FREE(pw);
					return NULL;
				}
			} else {
				*p_save_username = talloc_strdup(mem_ctx, pw->pw_name);
			}

			/* whew -- done! */
			return pw;
		}

		/* setup for lookup of just the username */
		/* remember that p and username are overlapping memory */

		p++;
		username = talloc_strdup(mem_ctx, p);
		if (!username) {
			return NULL;
		}
	}

	/* just lookup a plain username */

	pw = Get_Pwnam_alloc(mem_ctx, username);

	/* Create local user if requested but only if winbindd
	   is not running.  We need to protect against cases
	   where winbindd is failing and then prematurely
	   creating users in /etc/passwd */

	if ( !pw && create && !winbind_ping() ) {
		/* Don't add a machine account. */
		if (username[strlen(username)-1] == '$')
			return NULL;

		_smb_create_user(NULL, username, NULL);
		pw = Get_Pwnam_alloc(mem_ctx, username);
	}

	/* one last check for a valid passwd struct */

	if (pw) {
		*p_save_username = talloc_strdup(mem_ctx, pw->pw_name);
	}
	return pw;
}

/***************************************************************************
 Make a server_info struct from the info3 returned by a domain logon 
***************************************************************************/

NTSTATUS make_server_info_info3(TALLOC_CTX *mem_ctx, 
				const char *sent_nt_username,
				const char *domain,
				struct auth_serversupplied_info **server_info,
				struct netr_SamInfo3 *info3)
{
	static const char zeros[16] = {0, };

	NTSTATUS nt_status = NT_STATUS_OK;
	char *found_username = NULL;
	const char *nt_domain;
	const char *nt_username;
	struct dom_sid user_sid;
	struct dom_sid group_sid;
	bool username_was_mapped;
	struct passwd *pwd;
	struct auth_serversupplied_info *result;

	/* 
	   Here is where we should check the list of
	   trusted domains, and verify that the SID 
	   matches.
	*/

	if (!sid_compose(&user_sid, info3->base.domain_sid, info3->base.rid)) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (!sid_compose(&group_sid, info3->base.domain_sid,
			 info3->base.primary_gid)) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	nt_username = talloc_strdup(mem_ctx, info3->base.account_name.string);
	if (!nt_username) {
		/* If the server didn't give us one, just use the one we sent
		 * them */
		nt_username = sent_nt_username;
	}

	nt_domain = talloc_strdup(mem_ctx, info3->base.domain.string);
	if (!nt_domain) {
		/* If the server didn't give us one, just use the one we sent
		 * them */
		nt_domain = domain;
	}

	/* If getpwnam() fails try the add user script (2.2.x behavior).

	   We use the _unmapped_ username here in an attempt to provide
	   consistent username mapping behavior between kerberos and NTLM[SSP]
	   authentication in domain mode security.  I.E. Username mapping
	   should be applied to the fully qualified username
	   (e.g. DOMAIN\user) and not just the login name.  Yes this means we
	   called map_username() unnecessarily in make_user_info_map() but
	   that is how the current code is designed.  Making the change here
	   is the least disruptive place.  -- jerry */

	/* this call will try to create the user if necessary */

	nt_status = check_account(mem_ctx, nt_domain, sent_nt_username,
				     &found_username, &pwd,
				     &username_was_mapped);

	if (!NT_STATUS_IS_OK(nt_status)) {
		return nt_status;
	}

	result = make_server_info(NULL);
	if (result == NULL) {
		DEBUG(4, ("make_server_info failed!\n"));
		return NT_STATUS_NO_MEMORY;
	}

	result->unix_name = talloc_strdup(result, found_username);

	result->sanitized_username = sanitize_username(result,
						       result->unix_name);
	if (result->sanitized_username == NULL) {
		TALLOC_FREE(result);
		return NT_STATUS_NO_MEMORY;
	}

	/* copy in the info3 */
	result->info3 = copy_netr_SamInfo3(result, info3);
	if (result->info3 == NULL) {
		TALLOC_FREE(result);
		return NT_STATUS_NO_MEMORY;
	}

	/* Fill in the unix info we found on the way */

	result->utok.uid = pwd->pw_uid;
	result->utok.gid = pwd->pw_gid;

	/* ensure we are never given NULL session keys */

	if (memcmp(info3->base.key.key, zeros, sizeof(zeros)) == 0) {
		result->user_session_key = data_blob_null;
	} else {
		result->user_session_key = data_blob_talloc(
			result, info3->base.key.key,
			sizeof(info3->base.key.key));
	}

	if (memcmp(info3->base.LMSessKey.key, zeros, 8) == 0) {
		result->lm_session_key = data_blob_null;
	} else {
		result->lm_session_key = data_blob_talloc(
			result, info3->base.LMSessKey.key,
			sizeof(info3->base.LMSessKey.key));
	}

	result->nss_token |= username_was_mapped;

	*server_info = result;

	return NT_STATUS_OK;
}

/*****************************************************************************
 Make a server_info struct from the wbcAuthUserInfo returned by a domain logon
******************************************************************************/

NTSTATUS make_server_info_wbcAuthUserInfo(TALLOC_CTX *mem_ctx,
					  const char *sent_nt_username,
					  const char *domain,
					  const struct wbcAuthUserInfo *info,
					  struct auth_serversupplied_info **server_info)
{
	struct netr_SamInfo3 *info3;

	info3 = wbcAuthUserInfo_to_netr_SamInfo3(mem_ctx, info);
	if (!info3) {
		return NT_STATUS_NO_MEMORY;
	}

	return make_server_info_info3(mem_ctx,
				      sent_nt_username, domain,
				      server_info, info3);
}

/**
 * Verify whether or not given domain is trusted.
 *
 * @param domain_name name of the domain to be verified
 * @return true if domain is one of the trusted ones or
 *         false if otherwise
 **/

bool is_trusted_domain(const char* dom_name)
{
	struct dom_sid trustdom_sid;
	bool ret;

	/* no trusted domains for a standalone server */

	if ( lp_server_role() == ROLE_STANDALONE )
		return False;

	if (dom_name == NULL || dom_name[0] == '\0') {
		return false;
	}

	if (strequal(dom_name, get_global_sam_name())) {
		return false;
	}

	/* if we are a DC, then check for a direct trust relationships */

	if ( IS_DC ) {
		become_root();
		DEBUG (5,("is_trusted_domain: Checking for domain trust with "
			  "[%s]\n", dom_name ));
		ret = pdb_get_trusteddom_pw(dom_name, NULL, NULL, NULL);
		unbecome_root();
		if (ret)
			return True;
	}
	else {
		wbcErr result;

		/* If winbind is around, ask it */

		result = wb_is_trusted_domain(dom_name);

		if (result == WBC_ERR_SUCCESS) {
			return True;
		}

		if (result == WBC_ERR_DOMAIN_NOT_FOUND) {
			/* winbind could not find the domain */
			return False;
		}

		/* The only other possible result is that winbind is not up
		   and running. We need to update the trustdom_cache
		   ourselves */

		update_trustdom_cache();
	}

	/* now the trustdom cache should be available a DC could still
	 * have a transitive trust so fall back to the cache of trusted
	 * domains (like a domain member would use  */

	if ( trustdom_cache_fetch(dom_name, &trustdom_sid) ) {
		return True;
	}

	return False;
}

