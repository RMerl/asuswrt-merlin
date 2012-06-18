/* 
   Unix SMB/CIFS implementation.
   Authentication utility functions
   Copyright (C) Andrew Tridgell 1992-1998
   Copyright (C) Andrew Bartlett 2001
   Copyright (C) Jeremy Allison 2000-2001
   Copyright (C) Rafal Szczesniak 2002
   Copyright (C) Stefan Metzmacher 2005

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
#include "libcli/security/security.h"
#include "libcli/auth/libcli_auth.h"
#include "auth/credentials/credentials.h"
#include "param/param.h"
#include "auth/auth.h" /* for auth_serversupplied_info */
#include "auth/session.h"
#include "auth/system_session_proto.h"

/**
 * Create the SID list for this user. 
 *
 * @note Specialised version for system sessions that doesn't use the SAM.
 */
static NTSTATUS create_token(TALLOC_CTX *mem_ctx, 
			       struct dom_sid *user_sid,
			       struct dom_sid *group_sid, 
			       int n_groupSIDs,
			       struct dom_sid **groupSIDs, 
			       bool is_authenticated,
			       struct security_token **token)
{
	struct security_token *ptoken;
	int i;

	ptoken = security_token_initialise(mem_ctx);
	NT_STATUS_HAVE_NO_MEMORY(ptoken);

	ptoken->sids = talloc_array(ptoken, struct dom_sid *, n_groupSIDs + 5);
	NT_STATUS_HAVE_NO_MEMORY(ptoken->sids);

	ptoken->user_sid = talloc_reference(ptoken, user_sid);
	ptoken->group_sid = talloc_reference(ptoken, group_sid);
	ptoken->privilege_mask = 0;

	ptoken->sids[0] = ptoken->user_sid;
	ptoken->sids[1] = ptoken->group_sid;

	/*
	 * Finally add the "standard" SIDs.
	 * The only difference between guest and "anonymous"
	 * is the addition of Authenticated_Users.
	 */
	ptoken->sids[2] = dom_sid_parse_talloc(ptoken->sids, SID_WORLD);
	NT_STATUS_HAVE_NO_MEMORY(ptoken->sids[2]);
	ptoken->sids[3] = dom_sid_parse_talloc(ptoken->sids, SID_NT_NETWORK);
	NT_STATUS_HAVE_NO_MEMORY(ptoken->sids[3]);
	ptoken->num_sids = 4;

	if (is_authenticated) {
		ptoken->sids[4] = dom_sid_parse_talloc(ptoken->sids, SID_NT_AUTHENTICATED_USERS);
		NT_STATUS_HAVE_NO_MEMORY(ptoken->sids[4]);
		ptoken->num_sids++;
	}

	for (i = 0; i < n_groupSIDs; i++) {
		size_t check_sid_idx;
		for (check_sid_idx = 1; 
		     check_sid_idx < ptoken->num_sids; 
		     check_sid_idx++) {
			if (dom_sid_equal(ptoken->sids[check_sid_idx], groupSIDs[i])) {
				break;
			}
		}

		if (check_sid_idx == ptoken->num_sids) {
			ptoken->sids[ptoken->num_sids++] = talloc_reference(ptoken->sids, groupSIDs[i]);
		}
	}

	*token = ptoken;

	/* Shortcuts to prevent recursion and avoid lookups */
	if (ptoken->user_sid == NULL) {
		ptoken->privilege_mask = 0;
		return NT_STATUS_OK;
	} 
	
	if (security_token_is_system(ptoken)) {
		ptoken->privilege_mask = ~0;
		return NT_STATUS_OK;
	} 
	
	if (security_token_is_anonymous(ptoken)) {
		ptoken->privilege_mask = 0;
		return NT_STATUS_OK;
	}

	DEBUG(0, ("Created token was not system or anonymous token!"));
	*token = NULL;
	return NT_STATUS_INTERNAL_ERROR;
}

static NTSTATUS generate_session_info(TALLOC_CTX *mem_ctx, 
				    struct auth_serversupplied_info *server_info, 
				    struct auth_session_info **_session_info) 
{
	struct auth_session_info *session_info;
	NTSTATUS nt_status;

	session_info = talloc(mem_ctx, struct auth_session_info);
	NT_STATUS_HAVE_NO_MEMORY(session_info);

	session_info->server_info = talloc_reference(session_info, server_info);

	/* unless set otherwise, the session key is the user session
	 * key from the auth subsystem */ 
	session_info->session_key = server_info->user_session_key;

	nt_status = create_token(session_info,
					  server_info->account_sid,
					  server_info->primary_group_sid,
					  server_info->n_domain_groups,
					  server_info->domain_groups,
					  server_info->authenticated,
					  &session_info->security_token);
	NT_STATUS_NOT_OK_RETURN(nt_status);

	session_info->credentials = NULL;

	*_session_info = session_info;
	return NT_STATUS_OK;
}



/* Create a security token for a session SYSTEM (the most
 * trusted/prvilaged account), including the local machine account as
 * the off-host credentials
 */ 
_PUBLIC_ struct auth_session_info *system_session(TALLOC_CTX *mem_ctx, struct loadparm_context *lp_ctx) 
{
	NTSTATUS nt_status;
	struct auth_session_info *session_info = NULL;
	nt_status = auth_system_session_info(mem_ctx,
					     lp_ctx,
					     &session_info);
	if (!NT_STATUS_IS_OK(nt_status)) {
		return NULL;
	}
	return session_info;
}

static NTSTATUS _auth_system_session_info(TALLOC_CTX *parent_ctx, 
					  struct loadparm_context *lp_ctx,
					  bool anonymous_credentials, 
					  struct auth_session_info **_session_info) 
{
	NTSTATUS nt_status;
	struct auth_serversupplied_info *server_info = NULL;
	struct auth_session_info *session_info = NULL;
	TALLOC_CTX *mem_ctx = talloc_new(parent_ctx);
	
	nt_status = auth_system_server_info(mem_ctx, lp_netbios_name(lp_ctx),
					    &server_info);
	if (!NT_STATUS_IS_OK(nt_status)) {
		talloc_free(mem_ctx);
		return nt_status;
	}

	/* references the server_info into the session_info */
	nt_status = generate_session_info(parent_ctx, server_info, &session_info);
	talloc_free(mem_ctx);

	NT_STATUS_NOT_OK_RETURN(nt_status);

	session_info->credentials = cli_credentials_init(session_info);
	if (!session_info->credentials) {
		return NT_STATUS_NO_MEMORY;
	}

	cli_credentials_set_conf(session_info->credentials, lp_ctx);

	if (anonymous_credentials) {
		cli_credentials_set_anonymous(session_info->credentials);
	} else {
		cli_credentials_set_machine_account_pending(session_info->credentials, lp_ctx);
	}
	*_session_info = session_info;

	return NT_STATUS_OK;
}

/*
  Create a system session, but with anonymous credentials (so we do not need to open secrets.ldb)
*/
_PUBLIC_ struct auth_session_info *system_session_anon(TALLOC_CTX *mem_ctx, struct loadparm_context *lp_ctx) 
{
	NTSTATUS nt_status;
	struct auth_session_info *session_info = NULL;
	nt_status = _auth_system_session_info(mem_ctx, lp_ctx, false, &session_info);
	if (!NT_STATUS_IS_OK(nt_status)) {
		return NULL;
	}
	return session_info;
}



_PUBLIC_ NTSTATUS auth_system_session_info(TALLOC_CTX *parent_ctx, 
					   struct loadparm_context *lp_ctx,
					   struct auth_session_info **_session_info) 
{
	return _auth_system_session_info(parent_ctx, 
			lp_ctx,
			lp_parm_bool(lp_ctx, NULL, "system", "anonymous", false), 
			_session_info);
}

NTSTATUS auth_system_server_info(TALLOC_CTX *mem_ctx, const char *netbios_name, 
				 struct auth_serversupplied_info **_server_info) 
{
	struct auth_serversupplied_info *server_info;

	server_info = talloc(mem_ctx, struct auth_serversupplied_info);
	NT_STATUS_HAVE_NO_MEMORY(server_info);

	server_info->account_sid = dom_sid_parse_talloc(server_info, SID_NT_SYSTEM);
	NT_STATUS_HAVE_NO_MEMORY(server_info->account_sid);

	/* is this correct? */
	server_info->primary_group_sid = dom_sid_parse_talloc(server_info, SID_BUILTIN_ADMINISTRATORS);
	NT_STATUS_HAVE_NO_MEMORY(server_info->primary_group_sid);

	server_info->n_domain_groups = 0;
	server_info->domain_groups = NULL;

	/* annoying, but the Anonymous really does have a session key, 
	   and it is all zeros! */
	server_info->user_session_key = data_blob_talloc(server_info, NULL, 16);
	NT_STATUS_HAVE_NO_MEMORY(server_info->user_session_key.data);

	server_info->lm_session_key = data_blob_talloc(server_info, NULL, 16);
	NT_STATUS_HAVE_NO_MEMORY(server_info->lm_session_key.data);

	data_blob_clear(&server_info->user_session_key);
	data_blob_clear(&server_info->lm_session_key);

	server_info->account_name = talloc_strdup(server_info, "SYSTEM");
	NT_STATUS_HAVE_NO_MEMORY(server_info->account_name);

	server_info->domain_name = talloc_strdup(server_info, "NT AUTHORITY");
	NT_STATUS_HAVE_NO_MEMORY(server_info->domain_name);

	server_info->full_name = talloc_strdup(server_info, "System");
	NT_STATUS_HAVE_NO_MEMORY(server_info->full_name);

	server_info->logon_script = talloc_strdup(server_info, "");
	NT_STATUS_HAVE_NO_MEMORY(server_info->logon_script);

	server_info->profile_path = talloc_strdup(server_info, "");
	NT_STATUS_HAVE_NO_MEMORY(server_info->profile_path);

	server_info->home_directory = talloc_strdup(server_info, "");
	NT_STATUS_HAVE_NO_MEMORY(server_info->home_directory);

	server_info->home_drive = talloc_strdup(server_info, "");
	NT_STATUS_HAVE_NO_MEMORY(server_info->home_drive);

	server_info->logon_server = talloc_strdup(server_info, netbios_name);
	NT_STATUS_HAVE_NO_MEMORY(server_info->logon_server);

	server_info->last_logon = 0;
	server_info->last_logoff = 0;
	server_info->acct_expiry = 0;
	server_info->last_password_change = 0;
	server_info->allow_password_change = 0;
	server_info->force_password_change = 0;

	server_info->logon_count = 0;
	server_info->bad_password_count = 0;

	server_info->acct_flags = ACB_NORMAL;

	server_info->authenticated = true;

	*_server_info = server_info;

	return NT_STATUS_OK;
}


/* Create server info for the Administrator account. This should only be used
 * during provisioning when we need to impersonate Administrator but
 * the account has not been created yet */

static NTSTATUS create_admin_token(TALLOC_CTX *mem_ctx,
				   struct dom_sid *user_sid,
				   struct dom_sid *group_sid,
				   int n_groupSIDs,
				   struct dom_sid **groupSIDs,
				   struct security_token **token)
{
	struct security_token *ptoken;
	int i;

	ptoken = security_token_initialise(mem_ctx);
	NT_STATUS_HAVE_NO_MEMORY(ptoken);

	ptoken->sids = talloc_array(ptoken, struct dom_sid *, n_groupSIDs + 3);
	NT_STATUS_HAVE_NO_MEMORY(ptoken->sids);

	ptoken->user_sid = talloc_reference(ptoken, user_sid);
	ptoken->group_sid = talloc_reference(ptoken, group_sid);
	ptoken->privilege_mask = 0;

	ptoken->sids[0] = ptoken->user_sid;
	ptoken->sids[1] = ptoken->group_sid;
	ptoken->sids[2] = dom_sid_parse_talloc(ptoken->sids, SID_NT_AUTHENTICATED_USERS);
	NT_STATUS_HAVE_NO_MEMORY(ptoken->sids[2]);
	ptoken->num_sids = 3;


	for (i = 0; i < n_groupSIDs; i++) {
		size_t check_sid_idx;
		for (check_sid_idx = 1;
		     check_sid_idx < ptoken->num_sids;
		     check_sid_idx++) {
			if (dom_sid_equal(ptoken->sids[check_sid_idx], groupSIDs[i])) {
				break;
			}
		}

		if (check_sid_idx == ptoken->num_sids) {
			ptoken->sids[ptoken->num_sids++] = talloc_reference(ptoken->sids, groupSIDs[i]);
		}
	}

	*token = ptoken;
	ptoken->privilege_mask = ~0;
	return NT_STATUS_OK;
}

static NTSTATUS auth_domain_admin_server_info(TALLOC_CTX *mem_ctx,
					      const char *netbios_name,
					      const char *domain_name,
					      struct dom_sid *domain_sid,
					      struct auth_serversupplied_info **_server_info)
{
	struct auth_serversupplied_info *server_info;

	server_info = talloc(mem_ctx, struct auth_serversupplied_info);
	NT_STATUS_HAVE_NO_MEMORY(server_info);

	server_info->account_sid = dom_sid_add_rid(server_info, domain_sid, DOMAIN_RID_ADMINISTRATOR);
	NT_STATUS_HAVE_NO_MEMORY(server_info->account_sid);

	server_info->primary_group_sid = dom_sid_add_rid(server_info, domain_sid, DOMAIN_RID_USERS);
	NT_STATUS_HAVE_NO_MEMORY(server_info->primary_group_sid);

	server_info->n_domain_groups = 6;
	server_info->domain_groups = talloc_array(server_info, struct dom_sid *, server_info->n_domain_groups);

	server_info->domain_groups[0] = dom_sid_parse_talloc(server_info, SID_BUILTIN_ADMINISTRATORS);
	server_info->domain_groups[1] = dom_sid_add_rid(server_info, domain_sid, DOMAIN_RID_ADMINS);
	server_info->domain_groups[2] = dom_sid_add_rid(server_info, domain_sid, DOMAIN_RID_USERS);
	server_info->domain_groups[3] = dom_sid_add_rid(server_info, domain_sid, DOMAIN_RID_ENTERPRISE_ADMINS);
	server_info->domain_groups[4] = dom_sid_add_rid(server_info, domain_sid, DOMAIN_RID_POLICY_ADMINS);
	server_info->domain_groups[5] = dom_sid_add_rid(server_info, domain_sid, DOMAIN_RID_SCHEMA_ADMINS);

	/* What should the session key be?*/
	server_info->user_session_key = data_blob_talloc(server_info, NULL, 16);
	NT_STATUS_HAVE_NO_MEMORY(server_info->user_session_key.data);

	server_info->lm_session_key = data_blob_talloc(server_info, NULL, 16);
	NT_STATUS_HAVE_NO_MEMORY(server_info->lm_session_key.data);

	data_blob_clear(&server_info->user_session_key);
	data_blob_clear(&server_info->lm_session_key);

	server_info->account_name = talloc_strdup(server_info, "Administrator");
	NT_STATUS_HAVE_NO_MEMORY(server_info->account_name);

	server_info->domain_name = talloc_strdup(server_info, domain_name);
	NT_STATUS_HAVE_NO_MEMORY(server_info->domain_name);

	server_info->full_name = talloc_strdup(server_info, "Administrator");
	NT_STATUS_HAVE_NO_MEMORY(server_info->full_name);

	server_info->logon_script = talloc_strdup(server_info, "");
	NT_STATUS_HAVE_NO_MEMORY(server_info->logon_script);

	server_info->profile_path = talloc_strdup(server_info, "");
	NT_STATUS_HAVE_NO_MEMORY(server_info->profile_path);

	server_info->home_directory = talloc_strdup(server_info, "");
	NT_STATUS_HAVE_NO_MEMORY(server_info->home_directory);

	server_info->home_drive = talloc_strdup(server_info, "");
	NT_STATUS_HAVE_NO_MEMORY(server_info->home_drive);

	server_info->logon_server = talloc_strdup(server_info, netbios_name);
	NT_STATUS_HAVE_NO_MEMORY(server_info->logon_server);

	server_info->last_logon = 0;
	server_info->last_logoff = 0;
	server_info->acct_expiry = 0;
	server_info->last_password_change = 0;
	server_info->allow_password_change = 0;
	server_info->force_password_change = 0;

	server_info->logon_count = 0;
	server_info->bad_password_count = 0;

	server_info->acct_flags = ACB_NORMAL;

	server_info->authenticated = true;

	*_server_info = server_info;

	return NT_STATUS_OK;
}

static NTSTATUS auth_domain_admin_session_info(TALLOC_CTX *parent_ctx,
					       struct loadparm_context *lp_ctx,
					       struct dom_sid *domain_sid,
					       struct auth_session_info **_session_info)
{
	NTSTATUS nt_status;
	struct auth_serversupplied_info *server_info = NULL;
	struct auth_session_info *session_info = NULL;
	TALLOC_CTX *mem_ctx = talloc_new(parent_ctx);

	nt_status = auth_domain_admin_server_info(mem_ctx, lp_netbios_name(lp_ctx),
						  lp_workgroup(lp_ctx), domain_sid,
						  &server_info);
	if (!NT_STATUS_IS_OK(nt_status)) {
		talloc_free(mem_ctx);
		return nt_status;
	}

	session_info = talloc(mem_ctx, struct auth_session_info);
	NT_STATUS_HAVE_NO_MEMORY(session_info);

	session_info->server_info = talloc_reference(session_info, server_info);

	/* unless set otherwise, the session key is the user session
	 * key from the auth subsystem */
	session_info->session_key = server_info->user_session_key;

	nt_status = create_admin_token(session_info,
				       server_info->account_sid,
				       server_info->primary_group_sid,
				       server_info->n_domain_groups,
				       server_info->domain_groups,
				       &session_info->security_token);
	NT_STATUS_NOT_OK_RETURN(nt_status);

	session_info->credentials = cli_credentials_init(session_info);
	if (!session_info->credentials) {
		return NT_STATUS_NO_MEMORY;
	}

	cli_credentials_set_conf(session_info->credentials, lp_ctx);

	*_session_info = session_info;

	return NT_STATUS_OK;
}

_PUBLIC_ struct auth_session_info *admin_session(TALLOC_CTX *mem_ctx, struct loadparm_context *lp_ctx, struct dom_sid *domain_sid)
{
	NTSTATUS nt_status;
	struct auth_session_info *session_info = NULL;
	nt_status = auth_domain_admin_session_info(mem_ctx,
						   lp_ctx,
						   domain_sid,
						   &session_info);
	if (!NT_STATUS_IS_OK(nt_status)) {
		return NULL;
	}
	return session_info;
}
