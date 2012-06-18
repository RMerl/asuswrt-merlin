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
#include "auth/auth.h"
#include "libcli/security/security.h"
#include "libcli/auth/libcli_auth.h"
#include "dsdb/samdb/samdb.h"
#include "auth/credentials/credentials.h"
#include "param/param.h"
#include "auth/session_proto.h"

_PUBLIC_ struct auth_session_info *anonymous_session(TALLOC_CTX *mem_ctx, 
					    struct tevent_context *event_ctx, 
					    struct loadparm_context *lp_ctx) 
{
	NTSTATUS nt_status;
	struct auth_session_info *session_info = NULL;
	nt_status = auth_anonymous_session_info(mem_ctx, event_ctx, lp_ctx, &session_info);
	if (!NT_STATUS_IS_OK(nt_status)) {
		return NULL;
	}
	return session_info;
}

_PUBLIC_ NTSTATUS auth_anonymous_session_info(TALLOC_CTX *parent_ctx, 
				     struct tevent_context *event_ctx, 
				     struct loadparm_context *lp_ctx,
				     struct auth_session_info **_session_info) 
{
	NTSTATUS nt_status;
	struct auth_serversupplied_info *server_info = NULL;
	struct auth_session_info *session_info = NULL;
	TALLOC_CTX *mem_ctx = talloc_new(parent_ctx);
	
	nt_status = auth_anonymous_server_info(mem_ctx,
					       lp_netbios_name(lp_ctx),
					       &server_info);
	if (!NT_STATUS_IS_OK(nt_status)) {
		talloc_free(mem_ctx);
		return nt_status;
	}

	/* references the server_info into the session_info */
	nt_status = auth_generate_session_info(parent_ctx, event_ctx, lp_ctx, server_info, &session_info);
	talloc_free(mem_ctx);

	NT_STATUS_NOT_OK_RETURN(nt_status);

	session_info->credentials = cli_credentials_init(session_info);
	if (!session_info->credentials) {
		return NT_STATUS_NO_MEMORY;
	}

	cli_credentials_set_conf(session_info->credentials, lp_ctx);
	cli_credentials_set_anonymous(session_info->credentials);
	
	*_session_info = session_info;

	return NT_STATUS_OK;
}

_PUBLIC_ NTSTATUS auth_anonymous_server_info(TALLOC_CTX *mem_ctx, 
				    const char *netbios_name,
				    struct auth_serversupplied_info **_server_info) 
{
	struct auth_serversupplied_info *server_info;
	server_info = talloc(mem_ctx, struct auth_serversupplied_info);
	NT_STATUS_HAVE_NO_MEMORY(server_info);

	server_info->account_sid = dom_sid_parse_talloc(server_info, SID_NT_ANONYMOUS);
	NT_STATUS_HAVE_NO_MEMORY(server_info->account_sid);

	/* is this correct? */
	server_info->primary_group_sid = dom_sid_parse_talloc(server_info, SID_BUILTIN_GUESTS);
	NT_STATUS_HAVE_NO_MEMORY(server_info->primary_group_sid);

	server_info->n_domain_groups = 0;
	server_info->domain_groups = NULL;

	/* annoying, but the Anonymous really does have a session key... */
	server_info->user_session_key = data_blob_talloc(server_info, NULL, 16);
	NT_STATUS_HAVE_NO_MEMORY(server_info->user_session_key.data);

	server_info->lm_session_key = data_blob_talloc(server_info, NULL, 16);
	NT_STATUS_HAVE_NO_MEMORY(server_info->lm_session_key.data);

	/*  and it is all zeros! */
	data_blob_clear(&server_info->user_session_key);
	data_blob_clear(&server_info->lm_session_key);

	server_info->account_name = talloc_strdup(server_info, "ANONYMOUS LOGON");
	NT_STATUS_HAVE_NO_MEMORY(server_info->account_name);

	server_info->domain_name = talloc_strdup(server_info, "NT AUTHORITY");
	NT_STATUS_HAVE_NO_MEMORY(server_info->domain_name);

	server_info->full_name = talloc_strdup(server_info, "Anonymous Logon");
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

	server_info->authenticated = false;

	*_server_info = server_info;

	return NT_STATUS_OK;
}

_PUBLIC_ NTSTATUS auth_generate_session_info(TALLOC_CTX *mem_ctx, 
				    struct tevent_context *event_ctx, 
				    struct loadparm_context *lp_ctx,
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

	nt_status = security_token_create(session_info,
					  event_ctx,
					  lp_ctx,
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

/**
 * prints a struct auth_session_info security token to debug output.
 */
void auth_session_info_debug(int dbg_lev, 
			     const struct auth_session_info *session_info)
{
	if (!session_info) {
		DEBUG(dbg_lev, ("Session Info: (NULL)\n"));
		return;	
	}

	security_token_debug(dbg_lev, session_info->security_token);
}

