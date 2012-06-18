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
#include "smbd/globals.h"
#include "../libcli/auth/libcli_auth.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_AUTH

/****************************************************************************
 Ensure primary group SID is always at position 0 in a 
 auth_serversupplied_info struct.
****************************************************************************/

static void sort_sid_array_for_smbd(auth_serversupplied_info *result,
				const DOM_SID *pgroup_sid)
{
	unsigned int i;

	if (!result->sids) {
		return;
	}

	if (sid_compare(&result->sids[0], pgroup_sid)==0) {
		return;
	}

	for (i = 1; i < result->num_sids; i++) {
		if (sid_compare(pgroup_sid,
				&result->sids[i]) == 0) {
			sid_copy(&result->sids[i], &result->sids[0]);
			sid_copy(&result->sids[0], pgroup_sid);
			return;
		}
	}
}

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
 Create an auth_usersupplied_data structure
****************************************************************************/

static NTSTATUS make_user_info(auth_usersupplied_info **user_info,
                               const char *smb_name,
                               const char *internal_username,
                               const char *client_domain,
                               const char *domain,
                               const char *wksta_name,
                               DATA_BLOB *lm_pwd, DATA_BLOB *nt_pwd,
                               DATA_BLOB *lm_interactive_pwd, DATA_BLOB *nt_interactive_pwd,
                               DATA_BLOB *plaintext,
                               bool encrypted)
{

	DEBUG(5,("attempting to make a user_info for %s (%s)\n", internal_username, smb_name));

	*user_info = SMB_MALLOC_P(auth_usersupplied_info);
	if (*user_info == NULL) {
		DEBUG(0,("malloc failed for user_info (size %lu)\n", (unsigned long)sizeof(*user_info)));
		return NT_STATUS_NO_MEMORY;
	}

	ZERO_STRUCTP(*user_info);

	DEBUG(5,("making strings for %s's user_info struct\n", internal_username));

	(*user_info)->smb_name = SMB_STRDUP(smb_name);
	if ((*user_info)->smb_name == NULL) { 
		free_user_info(user_info);
		return NT_STATUS_NO_MEMORY;
	}
	
	(*user_info)->internal_username = SMB_STRDUP(internal_username);
	if ((*user_info)->internal_username == NULL) { 
		free_user_info(user_info);
		return NT_STATUS_NO_MEMORY;
	}

	(*user_info)->domain = SMB_STRDUP(domain);
	if ((*user_info)->domain == NULL) { 
		free_user_info(user_info);
		return NT_STATUS_NO_MEMORY;
	}

	(*user_info)->client_domain = SMB_STRDUP(client_domain);
	if ((*user_info)->client_domain == NULL) { 
		free_user_info(user_info);
		return NT_STATUS_NO_MEMORY;
	}

	(*user_info)->wksta_name = SMB_STRDUP(wksta_name);
	if ((*user_info)->wksta_name == NULL) { 
		free_user_info(user_info);
		return NT_STATUS_NO_MEMORY;
	}

	DEBUG(5,("making blobs for %s's user_info struct\n", internal_username));

	if (lm_pwd)
		(*user_info)->lm_resp = data_blob(lm_pwd->data, lm_pwd->length);
	if (nt_pwd)
		(*user_info)->nt_resp = data_blob(nt_pwd->data, nt_pwd->length);
	if (lm_interactive_pwd)
		(*user_info)->lm_interactive_pwd = data_blob(lm_interactive_pwd->data, lm_interactive_pwd->length);
	if (nt_interactive_pwd)
		(*user_info)->nt_interactive_pwd = data_blob(nt_interactive_pwd->data, nt_interactive_pwd->length);

	if (plaintext)
		(*user_info)->plaintext_password = data_blob(plaintext->data, plaintext->length);

	(*user_info)->encrypted = encrypted;

	(*user_info)->logon_parameters = 0;

	DEBUG(10,("made an %sencrypted user_info for %s (%s)\n", encrypted ? "":"un" , internal_username, smb_name));

	return NT_STATUS_OK;
}

/****************************************************************************
 Create an auth_usersupplied_data structure after appropriate mapping.
****************************************************************************/

NTSTATUS make_user_info_map(auth_usersupplied_info **user_info,
			    const char *smb_name,
			    const char *client_domain,
			    const char *wksta_name,
			    DATA_BLOB *lm_pwd,
			    DATA_BLOB *nt_pwd,
			    DATA_BLOB *lm_interactive_pwd,
			    DATA_BLOB *nt_interactive_pwd,
			    DATA_BLOB *plaintext,
			    bool encrypted)
{
	struct smbd_server_connection *sconn = smbd_server_conn;
	const char *domain;
	NTSTATUS result;
	bool was_mapped;
	fstring internal_username;
	fstrcpy(internal_username, smb_name);
	was_mapped = map_username(sconn, internal_username);

	DEBUG(5, ("Mapping user [%s]\\[%s] from workstation [%s]\n",
		 client_domain, smb_name, wksta_name));

	domain = client_domain;

	/* If you connect to a Windows domain member using a bogus domain name,
	 * the Windows box will map the BOGUS\user to SAMNAME\user.  Thus, if
	 * the Windows box is a DC the name will become DOMAIN\user and be
	 * authenticated against AD, if the Windows box is a member server but
	 * not a DC the name will become WORKSTATION\user.  A standalone
	 * non-domain member box will also map to WORKSTATION\user.
	 * This also deals with the client passing in a "" domain */

	if (!is_trusted_domain(domain) &&
	    !strequal(domain, my_sam_name()))
	{
		if (lp_map_untrusted_to_domain())
			domain = my_sam_name();
		else
			domain = get_global_sam_name();
		DEBUG(5, ("Mapped domain from [%s] to [%s] for user [%s] from "
			  "workstation [%s]\n",
			  client_domain, domain, smb_name, wksta_name));
	}

	/* We know that the given domain is trusted (and we are allowing them),
	 * it is our global SAM name, or for legacy behavior it is our
	 * primary domain name */

	result = make_user_info(user_info, smb_name, internal_username,
			      client_domain, domain, wksta_name,
			      lm_pwd, nt_pwd,
			      lm_interactive_pwd, nt_interactive_pwd,
			      plaintext, encrypted);
	if (NT_STATUS_IS_OK(result)) {
		(*user_info)->was_mapped = was_mapped;
	}
	return result;
}

/****************************************************************************
 Create an auth_usersupplied_data, making the DATA_BLOBs here. 
 Decrypt and encrypt the passwords.
****************************************************************************/

bool make_user_info_netlogon_network(auth_usersupplied_info **user_info, 
				     const char *smb_name, 
				     const char *client_domain, 
				     const char *wksta_name, 
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
				    wksta_name, 
				    lm_pwd_len ? &lm_blob : NULL, 
				    nt_pwd_len ? &nt_blob : NULL,
				    NULL, NULL, NULL,
				    True);

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

bool make_user_info_netlogon_interactive(auth_usersupplied_info **user_info, 
					 const char *smb_name, 
					 const char *client_domain, 
					 const char *wksta_name, 
					 uint32 logon_parameters,
					 const uchar chal[8], 
					 const uchar lm_interactive_pwd[16], 
					 const uchar nt_interactive_pwd[16], 
					 const uchar *dc_sess_key)
{
	unsigned char lm_pwd[16];
	unsigned char nt_pwd[16];
	unsigned char local_lm_response[24];
	unsigned char local_nt_response[24];
	unsigned char key[16];
	
	memcpy(key, dc_sess_key, 16);
	
	if (lm_interactive_pwd)
		memcpy(lm_pwd, lm_interactive_pwd, sizeof(lm_pwd));

	if (nt_interactive_pwd)
		memcpy(nt_pwd, nt_interactive_pwd, sizeof(nt_pwd));
	
#ifdef DEBUG_PASSWORD
	DEBUG(100,("key:"));
	dump_data(100, key, sizeof(key));
	
	DEBUG(100,("lm owf password:"));
	dump_data(100, lm_pwd, sizeof(lm_pwd));
	
	DEBUG(100,("nt owf password:"));
	dump_data(100, nt_pwd, sizeof(nt_pwd));
#endif
	
	if (lm_interactive_pwd)
		arcfour_crypt(lm_pwd, key, sizeof(lm_pwd));
	
	if (nt_interactive_pwd)
		arcfour_crypt(nt_pwd, key, sizeof(nt_pwd));
	
#ifdef DEBUG_PASSWORD
	DEBUG(100,("decrypt of lm owf password:"));
	dump_data(100, lm_pwd, sizeof(lm_pwd));
	
	DEBUG(100,("decrypt of nt owf password:"));
	dump_data(100, nt_pwd, sizeof(nt_pwd));
#endif
	
	if (lm_interactive_pwd)
		SMBOWFencrypt(lm_pwd, chal,
			      local_lm_response);

	if (nt_interactive_pwd)
		SMBOWFencrypt(nt_pwd, chal,
			      local_nt_response);
	
	/* Password info paranoia */
	ZERO_STRUCT(key);

	{
		bool ret;
		NTSTATUS nt_status;
		DATA_BLOB local_lm_blob;
		DATA_BLOB local_nt_blob;

		DATA_BLOB lm_interactive_blob;
		DATA_BLOB nt_interactive_blob;
		
		if (lm_interactive_pwd) {
			local_lm_blob = data_blob(local_lm_response,
						  sizeof(local_lm_response));
			lm_interactive_blob = data_blob(lm_pwd,
							sizeof(lm_pwd));
			ZERO_STRUCT(lm_pwd);
		}
		
		if (nt_interactive_pwd) {
			local_nt_blob = data_blob(local_nt_response,
						  sizeof(local_nt_response));
			nt_interactive_blob = data_blob(nt_pwd,
							sizeof(nt_pwd));
			ZERO_STRUCT(nt_pwd);
		}

		nt_status = make_user_info_map(
			user_info, 
			smb_name, client_domain, wksta_name, 
			lm_interactive_pwd ? &local_lm_blob : NULL,
			nt_interactive_pwd ? &local_nt_blob : NULL,
			lm_interactive_pwd ? &lm_interactive_blob : NULL,
			nt_interactive_pwd ? &nt_interactive_blob : NULL,
			NULL, True);

		if (NT_STATUS_IS_OK(nt_status)) {
			(*user_info)->logon_parameters = logon_parameters;
		}

		ret = NT_STATUS_IS_OK(nt_status) ? True : False;
		data_blob_free(&local_lm_blob);
		data_blob_free(&local_nt_blob);
		data_blob_free(&lm_interactive_blob);
		data_blob_free(&nt_interactive_blob);
		return ret;
	}
}


/****************************************************************************
 Create an auth_usersupplied_data structure
****************************************************************************/

bool make_user_info_for_reply(auth_usersupplied_info **user_info, 
			      const char *smb_name, 
			      const char *client_domain,
			      const uint8 chal[8],
			      DATA_BLOB plaintext_password)
{

	DATA_BLOB local_lm_blob;
	DATA_BLOB local_nt_blob;
	NTSTATUS ret = NT_STATUS_UNSUCCESSFUL;
			
	/*
	 * Not encrypted - do so.
	 */
	
	DEBUG(5,("make_user_info_for_reply: User passwords not in encrypted "
		 "format.\n"));
	
	if (plaintext_password.data) {
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
	
	ret = make_user_info_map(
		user_info, smb_name, client_domain, 
		get_remote_machine_name(),
		local_lm_blob.data ? &local_lm_blob : NULL,
		local_nt_blob.data ? &local_nt_blob : NULL,
		NULL, NULL,
		plaintext_password.data ? &plaintext_password : NULL, 
		False);
	
	data_blob_free(&local_lm_blob);
	return NT_STATUS_IS_OK(ret) ? True : False;
}

/****************************************************************************
 Create an auth_usersupplied_data structure
****************************************************************************/

NTSTATUS make_user_info_for_reply_enc(auth_usersupplied_info **user_info, 
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
				  True);
}

/****************************************************************************
 Create a guest user_info blob, for anonymous authenticaion.
****************************************************************************/

bool make_user_info_guest(auth_usersupplied_info **user_info) 
{
	NTSTATUS nt_status;

	nt_status = make_user_info(user_info, 
				   "","", 
				   "","", 
				   "", 
				   NULL, NULL, 
				   NULL, NULL, 
				   NULL,
				   True);
			      
	return NT_STATUS_IS_OK(nt_status) ? True : False;
}

static int server_info_dtor(auth_serversupplied_info *server_info)
{
	TALLOC_FREE(server_info->sam_account);
	ZERO_STRUCTP(server_info);
	return 0;
}

/***************************************************************************
 Make a server_info struct. Free with TALLOC_FREE().
***************************************************************************/

static auth_serversupplied_info *make_server_info(TALLOC_CTX *mem_ctx)
{
	struct auth_serversupplied_info *result;

	result = TALLOC_ZERO_P(mem_ctx, auth_serversupplied_info);
	if (result == NULL) {
		DEBUG(0, ("talloc failed\n"));
		return NULL;
	}

	talloc_set_destructor(result, server_info_dtor);

	/* Initialise the uid and gid values to something non-zero
	   which may save us from giving away root access if there
	   is a bug in allocating these fields. */

	result->utok.uid = -1;
	result->utok.gid = -1;
	return result;
}

static char *sanitize_username(TALLOC_CTX *mem_ctx, const char *username)
{
	fstring tmp;

	alpha_strcpy(tmp, username, ". _-$", sizeof(tmp));
	return talloc_strdup(mem_ctx, tmp);
}

/***************************************************************************
 Is the incoming username our own machine account ?
 If so, the connection is almost certainly from winbindd.
***************************************************************************/

static bool is_our_machine_account(const char *username)
{
	bool ret;
	char *truncname = NULL;
	size_t ulen = strlen(username);

	if (ulen == 0 || username[ulen-1] != '$') {
		return false;
	}
	truncname = SMB_STRDUP(username);
	if (!truncname) {
		return false;
	}
	truncname[ulen-1] = '\0';
	ret = strequal(truncname, global_myname());
	SAFE_FREE(truncname);
	return ret;
}

/***************************************************************************
 Make (and fill) a user_info struct from a struct samu
***************************************************************************/

NTSTATUS make_server_info_sam(auth_serversupplied_info **server_info,
			      struct samu *sampass)
{
	struct passwd *pwd;
	gid_t *gids;
	auth_serversupplied_info *result;
	const char *username = pdb_get_username(sampass);
	NTSTATUS status;

	if ( !(result = make_server_info(NULL)) ) {
		return NT_STATUS_NO_MEMORY;
	}

	if ( !(pwd = Get_Pwnam_alloc(result, username)) ) {
		DEBUG(1, ("User %s in passdb, but getpwnam() fails!\n",
			  pdb_get_username(sampass)));
		TALLOC_FREE(result);
		return NT_STATUS_NO_SUCH_USER;
	}

	result->sam_account = sampass;
	result->unix_name = pwd->pw_name;
	/* Ensure that we keep pwd->pw_name, because we will free pwd below */
	talloc_steal(result, pwd->pw_name);
	result->utok.gid = pwd->pw_gid;
	result->utok.uid = pwd->pw_uid;

	TALLOC_FREE(pwd);

	result->sanitized_username = sanitize_username(result,
						       result->unix_name);
	if (result->sanitized_username == NULL) {
		TALLOC_FREE(result);
		return NT_STATUS_NO_MEMORY;
	}

	if (IS_DC && is_our_machine_account(username)) {
		/*
		 * Ensure for a connection from our own
		 * machine account (from winbindd on a DC)
		 * there are no supplementary groups.
		 * Prevents loops in calling gid_to_sid().
		 */
		result->sids = NULL;
		gids = NULL;
		result->num_sids = 0;

		/*
		 * This is a hack of monstrous proportions.
		 * If we know it's winbindd talking to us,
		 * we know we must never recurse into it,
		 * so turn off contacting winbindd for this
		 * entire process. This will get fixed when
		 * winbindd doesn't need to talk to smbd on
		 * a PDC. JRA.
		 */

		(void)winbind_off();

		DEBUG(10, ("make_server_info_sam: our machine account %s "
			"setting supplementary group list empty and "
			"turning off winbindd requests.\n",
			username));
	} else {
		status = pdb_enum_group_memberships(result, sampass,
					    &result->sids, &gids,
					    &result->num_sids);

		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(10, ("pdb_enum_group_memberships failed: %s\n",
				   nt_errstr(status)));
			result->sam_account = NULL; /* Don't free on error exit. */
			TALLOC_FREE(result);
			return status;
		}
	}

	/* For now we throw away the gids and convert via sid_to_gid
	 * later. This needs fixing, but I'd like to get the code straight and
	 * simple first. */
	 
	TALLOC_FREE(gids);

	DEBUG(5,("make_server_info_sam: made server info for user %s -> %s\n",
		 pdb_get_username(sampass), result->unix_name));

	*server_info = result;
	/* Ensure that the sampass will be freed with the result */
	talloc_steal(result, sampass);

	return NT_STATUS_OK;
}

static NTSTATUS log_nt_token(NT_USER_TOKEN *token)
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
			sid_string_talloc(frame, &token->user_sids[i]));
	}

	command = talloc_string_sub(
		frame, lp_log_nt_token_command(),
		"%s", sid_string_talloc(frame, &token->user_sids[0]));
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
 * Create the token to use from server_info->sam_account and
 * server_info->sids (the info3/sam groups). Find the unix gids.
 */

NTSTATUS create_local_token(auth_serversupplied_info *server_info)
{
	NTSTATUS status;
	size_t i;
	struct dom_sid tmp_sid;

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
						    &server_info->ptok);

	} else {
		server_info->ptok = create_local_nt_token(
			server_info,
			pdb_get_user_sid(server_info->sam_account),
			server_info->guest,
			server_info->num_sids, server_info->sids);
		status = server_info->ptok ?
			NT_STATUS_OK : NT_STATUS_NO_SUCH_USER;
	}

	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	/* Convert the SIDs to gids. */

	server_info->utok.ngroups = 0;
	server_info->utok.groups = NULL;

	/* Start at index 1, where the groups start. */

	for (i=1; i<server_info->ptok->num_sids; i++) {
		gid_t gid;
		DOM_SID *sid = &server_info->ptok->user_sids[i];

		if (!sid_to_gid(sid, &gid)) {
			DEBUG(10, ("Could not convert SID %s to gid, "
				   "ignoring it\n", sid_string_dbg(sid)));
			continue;
		}
		add_gid_to_array_unique(server_info, gid,
					&server_info->utok.groups,
					&server_info->utok.ngroups);
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

	if (!uid_to_unix_users_sid(server_info->utok.uid, &tmp_sid)) {
		DEBUG(1,("create_local_token: Failed to create SID "
			"for uid %u!\n", (unsigned int)server_info->utok.uid));
	}
	add_sid_to_array_unique(server_info->ptok, &tmp_sid,
				&server_info->ptok->user_sids,
				&server_info->ptok->num_sids);

	for ( i=0; i<server_info->utok.ngroups; i++ ) {
		if (!gid_to_unix_groups_sid( server_info->utok.groups[i], &tmp_sid ) ) {
			DEBUG(1,("create_local_token: Failed to create SID "
				"for gid %u!\n", (unsigned int)server_info->utok.groups[i]));
			continue;
		}
		add_sid_to_array_unique(server_info->ptok, &tmp_sid,
					&server_info->ptok->user_sids,
					&server_info->ptok->num_sids);
	}

	debug_nt_user_token(DBGC_AUTH, 10, server_info->ptok);
	debug_unix_user_token(DBGC_AUTH, 10,
			      server_info->utok.uid,
			      server_info->utok.gid,
			      server_info->utok.ngroups,
			      server_info->utok.groups);

	status = log_nt_token(server_info->ptok);
	return status;
}

/*
 * Create an artificial NT token given just a username. (Initially intended
 * for force user)
 *
 * We go through lookup_name() to avoid problems we had with 'winbind use
 * default domain'.
 *
 * We have 3 cases:
 *
 * unmapped unix users: Go directly to nss to find the user's group.
 *
 * A passdb user: The list of groups is provided by pdb_enum_group_memberships.
 *
 * If the user is provided by winbind, the primary gid is set to "domain
 * users" of the user's domain. For an explanation why this is necessary, see
 * the thread starting at
 * http://lists.samba.org/archive/samba-technical/2006-January/044803.html.
 */

NTSTATUS create_token_from_username(TALLOC_CTX *mem_ctx, const char *username,
				    bool is_guest,
				    uid_t *uid, gid_t *gid,
				    char **found_username,
				    struct nt_user_token **token)
{
	NTSTATUS result = NT_STATUS_NO_SUCH_USER;
	TALLOC_CTX *tmp_ctx;
	DOM_SID user_sid;
	enum lsa_SidType type;
	gid_t *gids;
	DOM_SID *group_sids;
	DOM_SID unix_group_sid;
	size_t num_group_sids;
	size_t num_gids;
	size_t i;

	tmp_ctx = talloc_new(NULL);
	if (tmp_ctx == NULL) {
		DEBUG(0, ("talloc_new failed\n"));
		return NT_STATUS_NO_MEMORY;
	}

	if (!lookup_name_smbconf(tmp_ctx, username, LOOKUP_NAME_ALL,
			 NULL, NULL, &user_sid, &type)) {
		DEBUG(1, ("lookup_name_smbconf for %s failed\n", username));
		goto done;
	}

	if (type != SID_NAME_USER) {
		DEBUG(1, ("%s is a %s, not a user\n", username,
			  sid_type_lookup(type)));
		goto done;
	}

	if (sid_check_is_in_our_domain(&user_sid)) {
		bool ret;

		/* This is a passdb user, so ask passdb */

		struct samu *sam_acct = NULL;

		if ( !(sam_acct = samu_new( tmp_ctx )) ) {
			result = NT_STATUS_NO_MEMORY;
			goto done;
		}

		become_root();
		ret = pdb_getsampwsid(sam_acct, &user_sid);
		unbecome_root();

		if (!ret) {
			DEBUG(1, ("pdb_getsampwsid(%s) for user %s failed\n",
				  sid_string_dbg(&user_sid), username));
			DEBUGADD(1, ("Fall back to unix user %s\n", username));
			goto unix_user;
		}

		result = pdb_enum_group_memberships(tmp_ctx, sam_acct,
						    &group_sids, &gids,
						    &num_group_sids);
		if (!NT_STATUS_IS_OK(result)) {
			DEBUG(1, ("enum_group_memberships failed for %s (%s): "
				  "%s\n", username, sid_string_dbg(&user_sid),
				  nt_errstr(result)));
			DEBUGADD(1, ("Fall back to unix user %s\n", username));
			goto unix_user;
		}

		/* see the smb_panic() in pdb_default_enum_group_memberships */
		SMB_ASSERT(num_group_sids > 0); 

		*gid = gids[0];

		/* Ensure we're returning the found_username on the right context. */
		*found_username = talloc_strdup(mem_ctx,
						pdb_get_username(sam_acct));

		/*
		 * If the SID from lookup_name() was the guest sid, passdb knows
		 * about the mapping of guest sid to lp_guestaccount()
		 * username and will return the unix_pw info for a guest
		 * user. Use it if it's there, else lookup the *uid details
		 * using Get_Pwnam_alloc(). See bug #6291 for details. JRA.
		 */

		/* We must always assign the *uid. */
		if (sam_acct->unix_pw == NULL) {
			struct passwd *pwd = Get_Pwnam_alloc(sam_acct, *found_username );
			if (!pwd) {
				DEBUG(10, ("Get_Pwnam_alloc failed for %s\n",
					*found_username));
				result = NT_STATUS_NO_SUCH_USER;
				goto done;
			}
			result = samu_set_unix(sam_acct, pwd );
			if (!NT_STATUS_IS_OK(result)) {
				DEBUG(10, ("samu_set_unix failed for %s\n",
					*found_username));
				result = NT_STATUS_NO_SUCH_USER;
				goto done;
			}
		}
		*uid = sam_acct->unix_pw->pw_uid;

	} else 	if (sid_check_is_in_unix_users(&user_sid)) {

		/* This is a unix user not in passdb. We need to ask nss
		 * directly, without consulting passdb */

		struct passwd *pass;

		/*
		 * This goto target is used as a fallback for the passdb
		 * case. The concrete bug report is when passdb gave us an
		 * unmapped gid.
		 */

	unix_user:

		if (!sid_to_uid(&user_sid, uid)) {
			DEBUG(1, ("unix_user case, sid_to_uid for %s (%s) failed\n",
				  username, sid_string_dbg(&user_sid)));
			result = NT_STATUS_NO_SUCH_USER;
			goto done;
		}

		uid_to_unix_users_sid(*uid, &user_sid);

		pass = getpwuid_alloc(tmp_ctx, *uid);
		if (pass == NULL) {
			DEBUG(1, ("getpwuid(%u) for user %s failed\n",
				  (unsigned int)*uid, username));
			goto done;
		}

		if (!getgroups_unix_user(tmp_ctx, username, pass->pw_gid,
					 &gids, &num_group_sids)) {
			DEBUG(1, ("getgroups_unix_user for user %s failed\n",
				  username));
			goto done;
		}

		if (num_group_sids) {
			group_sids = TALLOC_ARRAY(tmp_ctx, DOM_SID, num_group_sids);
			if (group_sids == NULL) {
				DEBUG(1, ("TALLOC_ARRAY failed\n"));
				result = NT_STATUS_NO_MEMORY;
				goto done;
			}
		} else {
			group_sids = NULL;
		}

		for (i=0; i<num_group_sids; i++) {
			gid_to_sid(&group_sids[i], gids[i]);
		}

		/* In getgroups_unix_user we always set the primary gid */
		SMB_ASSERT(num_group_sids > 0); 

		*gid = gids[0];

		/* Ensure we're returning the found_username on the right context. */
		*found_username = talloc_strdup(mem_ctx, pass->pw_name);
	} else {

		/* This user is from winbind, force the primary gid to the
		 * user's "domain users" group. Under certain circumstances
		 * (user comes from NT4), this might be a loss of
		 * information. But we can not rely on winbind getting the
		 * correct info. AD might prohibit winbind looking up that
		 * information. */

		uint32 dummy;

		/* We must always assign the *uid. */
		if (!sid_to_uid(&user_sid, uid)) {
			DEBUG(1, ("winbindd case, sid_to_uid for %s (%s) failed\n",
				  username, sid_string_dbg(&user_sid)));
			result = NT_STATUS_NO_SUCH_USER;
			goto done;
		}

		num_group_sids = 1;
		group_sids = TALLOC_ARRAY(tmp_ctx, DOM_SID, num_group_sids);
		if (group_sids == NULL) {
			DEBUG(1, ("TALLOC_ARRAY failed\n"));
			result = NT_STATUS_NO_MEMORY;
			goto done;
		}

		sid_copy(&group_sids[0], &user_sid);
		sid_split_rid(&group_sids[0], &dummy);
		sid_append_rid(&group_sids[0], DOMAIN_GROUP_RID_USERS);

		if (!sid_to_gid(&group_sids[0], gid)) {
			DEBUG(1, ("sid_to_gid(%s) failed\n",
				  sid_string_dbg(&group_sids[0])));
			goto done;
		}

		gids = gid;

		/* Ensure we're returning the found_username on the right context. */
		*found_username = talloc_strdup(mem_ctx, username);
	}

	/* Add the "Unix Group" SID for each gid to catch mapped groups
	   and their Unix equivalent.  This is to solve the backwards
	   compatibility problem of 'valid users = +ntadmin' where
	   ntadmin has been paired with "Domain Admins" in the group
	   mapping table.  Otherwise smb.conf would need to be changed
	   to 'valid user = "Domain Admins"'.  --jerry */

	num_gids = num_group_sids;
	for ( i=0; i<num_gids; i++ ) {
		gid_t high, low;

		/* don't pickup anything managed by Winbind */

		if ( lp_idmap_gid(&low, &high) && (gids[i] >= low) && (gids[i] <= high) )
			continue;

		if ( !gid_to_unix_groups_sid( gids[i], &unix_group_sid ) ) {
			DEBUG(1,("create_token_from_username: Failed to create SID "
				"for gid %u!\n", (unsigned int)gids[i]));
			continue;
		}
		result = add_sid_to_array_unique(tmp_ctx, &unix_group_sid,
						 &group_sids, &num_group_sids);
		if (!NT_STATUS_IS_OK(result)) {
			goto done;
		}
	}

	/* Ensure we're creating the nt_token on the right context. */
	*token = create_local_nt_token(mem_ctx, &user_sid,
				       is_guest, num_group_sids, group_sids);

	if ((*token == NULL) || (*found_username == NULL)) {
		result = NT_STATUS_NO_MEMORY;
		goto done;
	}

	result = NT_STATUS_OK;
 done:
	TALLOC_FREE(tmp_ctx);
	return result;
}

/***************************************************************************
 Build upon create_token_from_username:

 Expensive helper function to figure out whether a user given its name is
 member of a particular group.
***************************************************************************/

bool user_in_group_sid(const char *username, const DOM_SID *group_sid)
{
	NTSTATUS status;
	uid_t uid;
	gid_t gid;
	char *found_username;
	struct nt_user_token *token;
	bool result;

	TALLOC_CTX *mem_ctx;

	mem_ctx = talloc_new(NULL);
	if (mem_ctx == NULL) {
		DEBUG(0, ("talloc_new failed\n"));
		return False;
	}

	status = create_token_from_username(mem_ctx, username, False,
					    &uid, &gid, &found_username,
					    &token);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(10, ("could not create token for %s\n", username));
		return False;
	}

	result = nt_token_check_sid(group_sid, token);

	TALLOC_FREE(mem_ctx);
	return result;
	
}

bool user_in_group(const char *username, const char *groupname)
{
	TALLOC_CTX *mem_ctx;
	DOM_SID group_sid;
	bool ret;

	mem_ctx = talloc_new(NULL);
	if (mem_ctx == NULL) {
		DEBUG(0, ("talloc_new failed\n"));
		return False;
	}

	ret = lookup_name(mem_ctx, groupname, LOOKUP_NAME_ALL,
			  NULL, NULL, &group_sid, NULL);
	TALLOC_FREE(mem_ctx);

	if (!ret) {
		DEBUG(10, ("lookup_name for (%s) failed.\n", groupname));
		return False;
	}

	return user_in_group_sid(username, &group_sid);
}

/***************************************************************************
 Make (and fill) a server_info struct from a 'struct passwd' by conversion
 to a struct samu
***************************************************************************/

NTSTATUS make_server_info_pw(auth_serversupplied_info **server_info, 
                             char *unix_username,
			     struct passwd *pwd)
{
	NTSTATUS status;
	struct samu *sampass = NULL;
	gid_t *gids;
	char *qualified_name = NULL;
	TALLOC_CTX *mem_ctx = NULL;
	DOM_SID u_sid;
	enum lsa_SidType type;
	auth_serversupplied_info *result;
	
	if ( !(sampass = samu_new( NULL )) ) {
		return NT_STATUS_NO_MEMORY;
	}
	
	status = samu_set_unix( sampass, pwd );
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	result = make_server_info(NULL);
	if (result == NULL) {
		TALLOC_FREE(sampass);
		return NT_STATUS_NO_MEMORY;
	}

	result->sam_account = sampass;

	result->unix_name = talloc_strdup(result, unix_username);
	result->sanitized_username = sanitize_username(result, unix_username);

	if ((result->unix_name == NULL)
	    || (result->sanitized_username == NULL)) {
		TALLOC_FREE(sampass);
		TALLOC_FREE(result);
		return NT_STATUS_NO_MEMORY;
	}

	result->utok.uid = pwd->pw_uid;
	result->utok.gid = pwd->pw_gid;

	status = pdb_enum_group_memberships(result, sampass,
					    &result->sids, &gids,
					    &result->num_sids);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(10, ("pdb_enum_group_memberships failed: %s\n",
			   nt_errstr(status)));
		TALLOC_FREE(result);
		return status;
	}

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
		TALLOC_FREE(result);
		return NT_STATUS_NO_MEMORY;
	}

	qualified_name = talloc_asprintf(mem_ctx, "%s\\%s",
					unix_users_domain_name(),
					unix_username );
	if (!qualified_name) {
		TALLOC_FREE(result);
		TALLOC_FREE(mem_ctx);
		return NT_STATUS_NO_MEMORY;
	}

	if (!lookup_name(mem_ctx, qualified_name, LOOKUP_NAME_ALL,
						NULL, NULL,
						&u_sid, &type)) {
		TALLOC_FREE(result);
		TALLOC_FREE(mem_ctx);
		return NT_STATUS_NO_SUCH_USER;
	}

	TALLOC_FREE(mem_ctx);

	if (type != SID_NAME_USER) {
		TALLOC_FREE(result);
		return NT_STATUS_NO_SUCH_USER;
	}

	status = add_sid_to_array_unique(result, &u_sid,
					 &result->sids,
					 &result->num_sids);
	if (!NT_STATUS_IS_OK(status)) {
		TALLOC_FREE(result);
		return status;
	}

	/* For now we throw away the gids and convert via sid_to_gid
	 * later. This needs fixing, but I'd like to get the code straight and
	 * simple first. */
	TALLOC_FREE(gids);

	*server_info = result;

	return NT_STATUS_OK;
}

/***************************************************************************
 Make (and fill) a user_info struct for a guest login.
 This *must* succeed for smbd to start. If there is no mapping entry for
 the guest gid, then create one.
***************************************************************************/

static NTSTATUS make_new_server_info_guest(auth_serversupplied_info **server_info)
{
	NTSTATUS status;
	struct samu *sampass = NULL;
	DOM_SID guest_sid;
	bool ret;
	char zeros[16];
	fstring tmp;

	if ( !(sampass = samu_new( NULL )) ) {
		return NT_STATUS_NO_MEMORY;
	}

	sid_copy(&guest_sid, get_global_sam_sid());
	sid_append_rid(&guest_sid, DOMAIN_USER_RID_GUEST);

	become_root();
	ret = pdb_getsampwsid(sampass, &guest_sid);
	unbecome_root();

	if (!ret) {
		TALLOC_FREE(sampass);
		return NT_STATUS_NO_SUCH_USER;
	}

	status = make_server_info_sam(server_info, sampass);
	if (!NT_STATUS_IS_OK(status)) {
		TALLOC_FREE(sampass);
		return status;
	}
	
	(*server_info)->guest = True;

	status = create_local_token(*server_info);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(10, ("create_local_token failed: %s\n",
			   nt_errstr(status)));
		return status;
	}

	/* annoying, but the Guest really does have a session key, and it is
	   all zeros! */
	ZERO_STRUCT(zeros);
	(*server_info)->user_session_key = data_blob(zeros, sizeof(zeros));
	(*server_info)->lm_session_key = data_blob(zeros, sizeof(zeros));

	alpha_strcpy(tmp, pdb_get_username(sampass), ". _-$", sizeof(tmp));
	(*server_info)->sanitized_username = talloc_strdup(*server_info, tmp);

	return NT_STATUS_OK;
}

/****************************************************************************
  Fake a auth_serversupplied_info just from a username
****************************************************************************/

NTSTATUS make_serverinfo_from_username(TALLOC_CTX *mem_ctx,
				       const char *username,
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

	status = create_local_token(result);

	if (!NT_STATUS_IS_OK(status)) {
		TALLOC_FREE(result);
		return status;
	}

	*presult = result;
	return NT_STATUS_OK;
}


struct auth_serversupplied_info *copy_serverinfo(TALLOC_CTX *mem_ctx,
						 const auth_serversupplied_info *src)
{
	auth_serversupplied_info *dst;

	dst = make_server_info(mem_ctx);
	if (dst == NULL) {
		return NULL;
	}

	dst->guest = src->guest;
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

	if (src->ptok) {
		dst->ptok = dup_nt_token(dst, src->ptok);
		if (!dst->ptok) {
			TALLOC_FREE(dst);
			return NULL;
		}
	}
	
	dst->user_session_key = data_blob_talloc( dst, src->user_session_key.data,
						src->user_session_key.length);

	dst->lm_session_key = data_blob_talloc(dst, src->lm_session_key.data,
						src->lm_session_key.length);

	dst->sam_account = samu_new(NULL);
	if (!dst->sam_account) {
		TALLOC_FREE(dst);
		return NULL;
	}

	if (!pdb_copy_sam_account(dst->sam_account, src->sam_account)) {
		TALLOC_FREE(dst);
		return NULL;
	}
	
	dst->pam_handle = NULL;
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

bool server_info_set_session_key(struct auth_serversupplied_info *info,
				 DATA_BLOB session_key)
{
	TALLOC_FREE(info->user_session_key.data);

	info->user_session_key = data_blob_talloc(
		info, session_key.data, session_key.length);

	return (info->user_session_key.data != NULL);
}

static auth_serversupplied_info *guest_info = NULL;

bool init_guest_info(void)
{
	if (guest_info != NULL)
		return True;

	return NT_STATUS_IS_OK(make_new_server_info_guest(&guest_info));
}

NTSTATUS make_server_info_guest(TALLOC_CTX *mem_ctx,
				auth_serversupplied_info **server_info)
{
	*server_info = copy_serverinfo(mem_ctx, guest_info);
	return (*server_info != NULL) ? NT_STATUS_OK : NT_STATUS_NO_MEMORY;
}

bool copy_current_user(struct current_user *dst, struct current_user *src)
{
	gid_t *groups;
	NT_USER_TOKEN *nt_token;

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
 Fill the sam account from getpwnam
***************************************************************************/
static NTSTATUS fill_sam_account(TALLOC_CTX *mem_ctx, 
				 const char *domain,
				 const char *username,
				 char **found_username,
				 uid_t *uid, gid_t *gid,
				 struct samu *account,
				 bool *username_was_mapped)
{
	struct smbd_server_connection *sconn = smbd_server_conn;
	NTSTATUS nt_status;
	fstring dom_user, lower_username;
	fstring real_username;
	struct passwd *passwd;

	fstrcpy( lower_username, username );
	strlower_m( lower_username );

	fstr_sprintf(dom_user, "%s%c%s", domain, *lp_winbind_separator(), 
		lower_username);

	/* Get the passwd struct.  Try to create the account is necessary. */

	*username_was_mapped = map_username(sconn, dom_user);

	if ( !(passwd = smb_getpwnam( NULL, dom_user, real_username, True )) )
		return NT_STATUS_NO_SUCH_USER;

	*uid = passwd->pw_uid;
	*gid = passwd->pw_gid;

	/* This is pointless -- there is no suport for differing 
	   unix and windows names.  Make sure to always store the 
	   one we actually looked up and succeeded. Have I mentioned
	   why I hate the 'winbind use default domain' parameter?   
	                                 --jerry              */
	   
	*found_username = talloc_strdup( mem_ctx, real_username );
	
	DEBUG(5,("fill_sam_account: located username was [%s]\n", *found_username));

	nt_status = samu_set_unix( account, passwd );
	
	TALLOC_FREE(passwd);
	
	return nt_status;
}

/****************************************************************************
 Wrapper to allow the getpwnam() call to strip the domain name and 
 try again in case a local UNIX user is already there.  Also run through 
 the username if we fallback to the username only.
 ****************************************************************************/
 
struct passwd *smb_getpwnam( TALLOC_CTX *mem_ctx, char *domuser,
			     fstring save_username, bool create )
{
	struct passwd *pw = NULL;
	char *p;
	fstring username;
	
	/* we only save a copy of the username it has been mangled 
	   by winbindd use default domain */
	   
	save_username[0] = '\0';
	   
	/* don't call map_username() here since it has to be done higher 
	   up the stack so we don't call it mutliple times */

	fstrcpy( username, domuser );
	
	p = strchr_m( username, *lp_winbind_separator() );
	
	/* code for a DOMAIN\user string */
	
	if ( p ) {
		fstring strip_username;

		pw = Get_Pwnam_alloc( mem_ctx, domuser );
		if ( pw ) {	
			/* make sure we get the case of the username correct */
			/* work around 'winbind use default domain = yes' */

			if ( !strchr_m( pw->pw_name, *lp_winbind_separator() ) ) {
				char *domain;
				
				/* split the domain and username into 2 strings */
				*p = '\0';
				domain = username;

				fstr_sprintf(save_username, "%s%c%s", domain, *lp_winbind_separator(), pw->pw_name);
			}
			else
				fstrcpy( save_username, pw->pw_name );

			/* whew -- done! */		
			return pw;
		}

		/* setup for lookup of just the username */
		/* remember that p and username are overlapping memory */

		p++;
		fstrcpy( strip_username, p );
		fstrcpy( username, strip_username );
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
	
	if ( pw )
		fstrcpy( save_username, pw->pw_name );

	return pw;
}

/***************************************************************************
 Make a server_info struct from the info3 returned by a domain logon 
***************************************************************************/

NTSTATUS make_server_info_info3(TALLOC_CTX *mem_ctx, 
				const char *sent_nt_username,
				const char *domain,
				auth_serversupplied_info **server_info, 
				struct netr_SamInfo3 *info3)
{
	char zeros[16];

	NTSTATUS nt_status = NT_STATUS_OK;
	char *found_username = NULL;
	const char *nt_domain;
	const char *nt_username;
	struct samu *sam_account = NULL;
	DOM_SID user_sid;
	DOM_SID group_sid;
	bool username_was_mapped;

	uid_t uid = (uid_t)-1;
	gid_t gid = (gid_t)-1;

	auth_serversupplied_info *result;

	/* 
	   Here is where we should check the list of
	   trusted domains, and verify that the SID 
	   matches.
	*/

	sid_copy(&user_sid, info3->base.domain_sid);
	if (!sid_append_rid(&user_sid, info3->base.rid)) {
		return NT_STATUS_INVALID_PARAMETER;
	}
	
	sid_copy(&group_sid, info3->base.domain_sid);
	if (!sid_append_rid(&group_sid, info3->base.primary_gid)) {
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
	
	/* try to fill the SAM account..  If getpwnam() fails, then try the 
	   add user script (2.2.x behavior).

	   We use the _unmapped_ username here in an attempt to provide
	   consistent username mapping behavior between kerberos and NTLM[SSP]
	   authentication in domain mode security.  I.E. Username mapping
	   should be applied to the fully qualified username
	   (e.g. DOMAIN\user) and not just the login name.  Yes this means we
	   called map_username() unnecessarily in make_user_info_map() but
	   that is how the current code is designed.  Making the change here
	   is the least disruptive place.  -- jerry */
	   
	if ( !(sam_account = samu_new( NULL )) ) {
		return NT_STATUS_NO_MEMORY;
	}

	/* this call will try to create the user if necessary */

	nt_status = fill_sam_account(mem_ctx, nt_domain, sent_nt_username,
				     &found_username, &uid, &gid, sam_account,
				     &username_was_mapped);

	
	/* if we still don't have a valid unix account check for 
	  'map to guest = bad uid' */
	  
	if (!NT_STATUS_IS_OK(nt_status)) {
		TALLOC_FREE( sam_account );
		if ( lp_map_to_guest() == MAP_TO_GUEST_ON_BAD_UID ) {
			make_server_info_guest(NULL, server_info);
			return NT_STATUS_OK;
		}
		return nt_status;
	}
		
	if (!pdb_set_nt_username(sam_account, nt_username, PDB_CHANGED)) {
		TALLOC_FREE(sam_account);
		return NT_STATUS_NO_MEMORY;
	}

	if (!pdb_set_username(sam_account, nt_username, PDB_CHANGED)) {
		TALLOC_FREE(sam_account);
		return NT_STATUS_NO_MEMORY;
	}

	if (!pdb_set_domain(sam_account, nt_domain, PDB_CHANGED)) {
		TALLOC_FREE(sam_account);
		return NT_STATUS_NO_MEMORY;
	}

	if (!pdb_set_user_sid(sam_account, &user_sid, PDB_CHANGED)) {
		TALLOC_FREE(sam_account);
		return NT_STATUS_UNSUCCESSFUL;
	}

	if (!pdb_set_group_sid(sam_account, &group_sid, PDB_CHANGED)) {
		TALLOC_FREE(sam_account);
		return NT_STATUS_UNSUCCESSFUL;
	}

	if (!pdb_set_fullname(sam_account,
			      info3->base.full_name.string,
			      PDB_CHANGED)) {
		TALLOC_FREE(sam_account);
		return NT_STATUS_NO_MEMORY;
	}

	if (!pdb_set_logon_script(sam_account,
				  info3->base.logon_script.string,
				  PDB_CHANGED)) {
		TALLOC_FREE(sam_account);
		return NT_STATUS_NO_MEMORY;
	}

	if (!pdb_set_profile_path(sam_account,
				  info3->base.profile_path.string,
				  PDB_CHANGED)) {
		TALLOC_FREE(sam_account);
		return NT_STATUS_NO_MEMORY;
	}

	if (!pdb_set_homedir(sam_account,
			     info3->base.home_directory.string,
			     PDB_CHANGED)) {
		TALLOC_FREE(sam_account);
		return NT_STATUS_NO_MEMORY;
	}

	if (!pdb_set_dir_drive(sam_account,
			       info3->base.home_drive.string,
			       PDB_CHANGED)) {
		TALLOC_FREE(sam_account);
		return NT_STATUS_NO_MEMORY;
	}

	if (!pdb_set_acct_ctrl(sam_account, info3->base.acct_flags, PDB_CHANGED)) {
		TALLOC_FREE(sam_account);
		return NT_STATUS_NO_MEMORY;
	}

	if (!pdb_set_pass_last_set_time(
		    sam_account,
		    nt_time_to_unix(info3->base.last_password_change),
		    PDB_CHANGED)) {
		TALLOC_FREE(sam_account);
		return NT_STATUS_NO_MEMORY;
	}

	if (!pdb_set_pass_can_change_time(
		    sam_account,
		    nt_time_to_unix(info3->base.allow_password_change),
		    PDB_CHANGED)) {
		TALLOC_FREE(sam_account);
		return NT_STATUS_NO_MEMORY;
	}

	if (!pdb_set_pass_must_change_time(
		    sam_account,
		    nt_time_to_unix(info3->base.force_password_change),
		    PDB_CHANGED)) {
		TALLOC_FREE(sam_account);
		return NT_STATUS_NO_MEMORY;
	}

	result = make_server_info(NULL);
	if (result == NULL) {
		DEBUG(4, ("make_server_info failed!\n"));
		TALLOC_FREE(sam_account);
		return NT_STATUS_NO_MEMORY;
	}

	/* save this here to _net_sam_logon() doesn't fail (it assumes a 
	   valid struct samu) */
		   
	result->sam_account = sam_account;
	result->unix_name = talloc_strdup(result, found_username);

	result->sanitized_username = sanitize_username(result,
						       result->unix_name);
	if (result->sanitized_username == NULL) {
		TALLOC_FREE(result);
		return NT_STATUS_NO_MEMORY;
	}

	/* Fill in the unix info we found on the way */

	result->utok.uid = uid;
	result->utok.gid = gid;

	/* Create a 'combined' list of all SIDs we might want in the SD */

	result->num_sids = 0;
	result->sids = NULL;

	nt_status = sid_array_from_info3(result, info3,
					 &result->sids,
					 &result->num_sids,
					 false, false);
	if (!NT_STATUS_IS_OK(nt_status)) {
		TALLOC_FREE(result);
		return nt_status;
	}

	/* Ensure the primary group sid is at position 0. */
	sort_sid_array_for_smbd(result, &group_sid);

	result->login_server = talloc_strdup(result,
					     info3->base.logon_server.string);

	/* ensure we are never given NULL session keys */

	ZERO_STRUCT(zeros);

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
					  auth_serversupplied_info **server_info)
{
	char zeros[16];

	NTSTATUS nt_status = NT_STATUS_OK;
	char *found_username = NULL;
	const char *nt_domain;
	const char *nt_username;
	struct samu *sam_account = NULL;
	DOM_SID user_sid;
	DOM_SID group_sid;
	bool username_was_mapped;
	uint32_t i;

	uid_t uid = (uid_t)-1;
	gid_t gid = (gid_t)-1;

	auth_serversupplied_info *result;

	result = make_server_info(NULL);
	if (result == NULL) {
		DEBUG(4, ("make_server_info failed!\n"));
		return NT_STATUS_NO_MEMORY;
	}

	/*
	   Here is where we should check the list of
	   trusted domains, and verify that the SID
	   matches.
	*/

	memcpy(&user_sid, &info->sids[0].sid, sizeof(user_sid));
	memcpy(&group_sid, &info->sids[1].sid, sizeof(group_sid));

	if (info->account_name) {
		nt_username = talloc_strdup(result, info->account_name);
	} else {
		/* If the server didn't give us one, just use the one we sent
		 * them */
		nt_username = talloc_strdup(result, sent_nt_username);
	}
	if (!nt_username) {
		TALLOC_FREE(result);
		return NT_STATUS_NO_MEMORY;
	}

	if (info->domain_name) {
		nt_domain = talloc_strdup(result, info->domain_name);
	} else {
		/* If the server didn't give us one, just use the one we sent
		 * them */
		nt_domain = talloc_strdup(result, domain);
	}
	if (!nt_domain) {
		TALLOC_FREE(result);
		return NT_STATUS_NO_MEMORY;
	}

	/* try to fill the SAM account..  If getpwnam() fails, then try the
	   add user script (2.2.x behavior).

	   We use the _unmapped_ username here in an attempt to provide
	   consistent username mapping behavior between kerberos and NTLM[SSP]
	   authentication in domain mode security.  I.E. Username mapping
	   should be applied to the fully qualified username
	   (e.g. DOMAIN\user) and not just the login name.  Yes this means we
	   called map_username() unnecessarily in make_user_info_map() but
	   that is how the current code is designed.  Making the change here
	   is the least disruptive place.  -- jerry */

	if ( !(sam_account = samu_new( result )) ) {
		TALLOC_FREE(result);
		return NT_STATUS_NO_MEMORY;
	}

	/* this call will try to create the user if necessary */

	nt_status = fill_sam_account(result, nt_domain, sent_nt_username,
				     &found_username, &uid, &gid, sam_account,
				     &username_was_mapped);

	/* if we still don't have a valid unix account check for
	  'map to guest = bad uid' */

	if (!NT_STATUS_IS_OK(nt_status)) {
		TALLOC_FREE( result );
		if ( lp_map_to_guest() == MAP_TO_GUEST_ON_BAD_UID ) {
			make_server_info_guest(NULL, server_info);
			return NT_STATUS_OK;
		}
		return nt_status;
	}

	if (!pdb_set_nt_username(sam_account, nt_username, PDB_CHANGED)) {
		TALLOC_FREE(result);
		return NT_STATUS_NO_MEMORY;
	}

	if (!pdb_set_username(sam_account, nt_username, PDB_CHANGED)) {
		TALLOC_FREE(result);
		return NT_STATUS_NO_MEMORY;
	}

	if (!pdb_set_domain(sam_account, nt_domain, PDB_CHANGED)) {
		TALLOC_FREE(result);
		return NT_STATUS_NO_MEMORY;
	}

	if (!pdb_set_user_sid(sam_account, &user_sid, PDB_CHANGED)) {
		TALLOC_FREE(result);
		return NT_STATUS_UNSUCCESSFUL;
	}

	if (!pdb_set_group_sid(sam_account, &group_sid, PDB_CHANGED)) {
		TALLOC_FREE(result);
		return NT_STATUS_UNSUCCESSFUL;
	}

	if (!pdb_set_fullname(sam_account, info->full_name, PDB_CHANGED)) {
		TALLOC_FREE(result);
		return NT_STATUS_NO_MEMORY;
	}

	if (!pdb_set_logon_script(sam_account, info->logon_script, PDB_CHANGED)) {
		TALLOC_FREE(result);
		return NT_STATUS_NO_MEMORY;
	}

	if (!pdb_set_profile_path(sam_account, info->profile_path, PDB_CHANGED)) {
		TALLOC_FREE(result);
		return NT_STATUS_NO_MEMORY;
	}

	if (!pdb_set_homedir(sam_account, info->home_directory, PDB_CHANGED)) {
		TALLOC_FREE(result);
		return NT_STATUS_NO_MEMORY;
	}

	if (!pdb_set_dir_drive(sam_account, info->home_drive, PDB_CHANGED)) {
		TALLOC_FREE(result);
		return NT_STATUS_NO_MEMORY;
	}

	if (!pdb_set_acct_ctrl(sam_account, info->acct_flags, PDB_CHANGED)) {
		TALLOC_FREE(result);
		return NT_STATUS_NO_MEMORY;
	}

	if (!pdb_set_pass_last_set_time(
		    sam_account,
		    info->pass_last_set_time,
		    PDB_CHANGED)) {
		TALLOC_FREE(result);
		return NT_STATUS_NO_MEMORY;
	}

	if (!pdb_set_pass_can_change_time(
		    sam_account,
		    info->pass_can_change_time,
		    PDB_CHANGED)) {
		TALLOC_FREE(result);
		return NT_STATUS_NO_MEMORY;
	}

	if (!pdb_set_pass_must_change_time(
		    sam_account,
		    info->pass_must_change_time,
		    PDB_CHANGED)) {
		TALLOC_FREE(result);
		return NT_STATUS_NO_MEMORY;
	}

	/* save this here to _net_sam_logon() doesn't fail (it assumes a
	   valid struct samu) */

	result->sam_account = sam_account;
	result->unix_name = talloc_strdup(result, found_username);

	result->sanitized_username = sanitize_username(result,
						       result->unix_name);
	result->login_server = talloc_strdup(result, info->logon_server);

	if ((result->unix_name == NULL)
	    || (result->sanitized_username == NULL)
	    || (result->login_server == NULL)) {
		TALLOC_FREE(result);
		return NT_STATUS_NO_MEMORY;
	}

	/* Fill in the unix info we found on the way */

	result->utok.uid = uid;
	result->utok.gid = gid;

	/* Create a 'combined' list of all SIDs we might want in the SD */

	result->num_sids = info->num_sids - 2;
	result->sids = talloc_array(result, DOM_SID, result->num_sids);
	if (result->sids == NULL) {
		TALLOC_FREE(result);
		return NT_STATUS_NO_MEMORY;
	}

	for (i=0; i < result->num_sids; i++) {
		memcpy(&result->sids[i], &info->sids[i+2].sid, sizeof(result->sids[i]));
	}

	/* Ensure the primary group sid is at position 0. */
	sort_sid_array_for_smbd(result, &group_sid);

	/* ensure we are never given NULL session keys */

	ZERO_STRUCT(zeros);

	if (memcmp(info->user_session_key, zeros, sizeof(zeros)) == 0) {
		result->user_session_key = data_blob_null;
	} else {
		result->user_session_key = data_blob_talloc(
			result, info->user_session_key,
			sizeof(info->user_session_key));
	}

	if (memcmp(info->lm_session_key, zeros, 8) == 0) {
		result->lm_session_key = data_blob_null;
	} else {
		result->lm_session_key = data_blob_talloc(
			result, info->lm_session_key,
			sizeof(info->lm_session_key));
	}

	result->nss_token |= username_was_mapped;

	*server_info = result;

	return NT_STATUS_OK;
}

/***************************************************************************
 Free a user_info struct
***************************************************************************/

void free_user_info(auth_usersupplied_info **user_info)
{
	DEBUG(5,("attempting to free (and zero) a user_info structure\n"));
	if (*user_info != NULL) {
		if ((*user_info)->smb_name) {
			DEBUG(10,("structure was created for %s\n",
				  (*user_info)->smb_name));
		}
		SAFE_FREE((*user_info)->smb_name);
		SAFE_FREE((*user_info)->internal_username);
		SAFE_FREE((*user_info)->client_domain);
		SAFE_FREE((*user_info)->domain);
		SAFE_FREE((*user_info)->wksta_name);
		data_blob_free(&(*user_info)->lm_resp);
		data_blob_free(&(*user_info)->nt_resp);
		data_blob_clear_free(&(*user_info)->lm_interactive_pwd);
		data_blob_clear_free(&(*user_info)->nt_interactive_pwd);
		data_blob_clear_free(&(*user_info)->plaintext_password);
		ZERO_STRUCT(**user_info);
	}
	SAFE_FREE(*user_info);
}

/***************************************************************************
 Make an auth_methods struct
***************************************************************************/

bool make_auth_methods(struct auth_context *auth_context, auth_methods **auth_method) 
{
	if (!auth_context) {
		smb_panic("no auth_context supplied to "
			  "make_auth_methods()!\n");
	}

	if (!auth_method) {
		smb_panic("make_auth_methods: pointer to auth_method pointer "
			  "is NULL!\n");
	}

	*auth_method = TALLOC_P(auth_context->mem_ctx, auth_methods);
	if (!*auth_method) {
		DEBUG(0,("make_auth_method: malloc failed!\n"));
		return False;
	}
	ZERO_STRUCTP(*auth_method);
	
	return True;
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
	DOM_SID trustdom_sid;
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

