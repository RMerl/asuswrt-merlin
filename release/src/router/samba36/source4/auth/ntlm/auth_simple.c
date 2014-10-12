/* 
   Unix SMB/CIFS implementation.

   auth functions

   Copyright (C) Simo Sorce 2005
   Copyright (C) Andrew Tridgell 2005
   Copyright (C) Andrew Bartlett 2005
   
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

/*
 It's allowed to pass NULL as session_info,
 when the caller doesn't need a session_info
*/
_PUBLIC_ NTSTATUS authenticate_username_pw(TALLOC_CTX *mem_ctx,
					   struct tevent_context *ev,
					   struct messaging_context *msg,
					   struct loadparm_context *lp_ctx,
					   const char *nt4_domain,
					   const char *nt4_username,
					   const char *password,
					   const uint32_t logon_parameters,
					   struct auth_session_info **session_info) 
{
	struct auth_context *auth_context;
	struct auth_usersupplied_info *user_info;
	struct auth_user_info_dc *user_info_dc;
	NTSTATUS nt_status;
	TALLOC_CTX *tmp_ctx = talloc_new(mem_ctx);

	if (!tmp_ctx) {
		return NT_STATUS_NO_MEMORY;
	}

	nt_status = auth_context_create(tmp_ctx, 
					ev, msg,
					lp_ctx,
					&auth_context);
	if (!NT_STATUS_IS_OK(nt_status)) {
		talloc_free(tmp_ctx);
		return nt_status;
	}

	user_info = talloc_zero(tmp_ctx, struct auth_usersupplied_info);
	if (!user_info) {
		talloc_free(tmp_ctx);
		return NT_STATUS_NO_MEMORY;
	}

	user_info->mapped_state = true;
	user_info->client.account_name = nt4_username;
	user_info->mapped.account_name = nt4_username;
	user_info->client.domain_name = nt4_domain;
	user_info->mapped.domain_name = nt4_domain;

	user_info->workstation_name = NULL;

	user_info->remote_host = NULL;

	user_info->password_state = AUTH_PASSWORD_PLAIN;
	user_info->password.plaintext = talloc_strdup(user_info, password);

	user_info->flags = USER_INFO_CASE_INSENSITIVE_USERNAME |
		USER_INFO_DONT_CHECK_UNIX_ACCOUNT;

	user_info->logon_parameters = logon_parameters |
		MSV1_0_CLEARTEXT_PASSWORD_ALLOWED |
		MSV1_0_CLEARTEXT_PASSWORD_SUPPLIED;

	nt_status = auth_check_password(auth_context, tmp_ctx, user_info, &user_info_dc);
	if (!NT_STATUS_IS_OK(nt_status)) {
		talloc_free(tmp_ctx);
		return nt_status;
	}

	if (session_info) {
		uint32_t flags = AUTH_SESSION_INFO_DEFAULT_GROUPS;
		if (user_info_dc->info->authenticated) {
			flags |= AUTH_SESSION_INFO_AUTHENTICATED;
		}
		nt_status = auth_context->generate_session_info(tmp_ctx, auth_context,
								user_info_dc,
								flags,
								session_info);

		if (NT_STATUS_IS_OK(nt_status)) {
			talloc_steal(mem_ctx, *session_info);
		}
	}

	talloc_free(tmp_ctx);
	return nt_status;
}

