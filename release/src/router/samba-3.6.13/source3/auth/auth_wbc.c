/*
   Unix SMB/CIFS implementation.

   Winbind client authentication mechanism designed to defer all
   authentication to the winbind daemon.

   Copyright (C) Tim Potter 2000
   Copyright (C) Andrew Bartlett 2001 - 2002
   Copyright (C) Dan Sledz 2009

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

/* This auth module is very similar to auth_winbind with 3 distinct
 * differences.
 *
 *      1) Does not fallback to another auth module if winbindd is unavailable
 *      2) Does not validate the domain of the user
 *      3) Handles unencrypted passwords
 *
 * The purpose of this module is to defer all authentication decisions (ie:
 * local user vs NIS vs LDAP vs AD; encrypted vs plaintext) to the wbc
 * compatible daemon.  This centeralizes all authentication decisions to a
 * single provider.
 *
 * This auth backend is most useful when used in conjunction with pdb_wbc_sam.
 */

#include "includes.h"
#include "auth.h"
#include "nsswitch/libwbclient/wbclient.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_AUTH

/* Authenticate a user with a challenge/response */

static NTSTATUS check_wbc_security(const struct auth_context *auth_context,
				       void *my_private_data,
				       TALLOC_CTX *mem_ctx,
				       const struct auth_usersupplied_info *user_info,
				       struct auth_serversupplied_info **server_info)
{
	NTSTATUS nt_status;
	wbcErr wbc_status;
	struct wbcAuthUserParams params;
	struct wbcAuthUserInfo *info = NULL;
	struct wbcAuthErrorInfo *err = NULL;

	if (!user_info || !auth_context || !server_info) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	ZERO_STRUCT(params);

	/* Send off request */

	DEBUG(10, ("Check auth for: [%s]", user_info->mapped.account_name));

	params.account_name	= user_info->client.account_name;
	params.domain_name	= user_info->mapped.domain_name;
	params.workstation_name	= user_info->workstation_name;

	params.flags		= 0;
	params.parameter_control= user_info->logon_parameters;

	/* Handle plaintext */
	switch (user_info->password_state) {
	case AUTH_PASSWORD_PLAIN:
	{
		DEBUG(3,("Checking plaintext password for %s.\n",
			 user_info->mapped.account_name));
		params.level = WBC_AUTH_USER_LEVEL_PLAIN;

		params.password.plaintext = user_info->password.plaintext;
		break;
	}
	case AUTH_PASSWORD_RESPONSE:
	case AUTH_PASSWORD_HASH:
	{
		DEBUG(3,("Checking encrypted password for %s.\n",
			 user_info->mapped.account_name));
		params.level = WBC_AUTH_USER_LEVEL_RESPONSE;

		memcpy(params.password.response.challenge,
		    auth_context->challenge.data,
		    sizeof(params.password.response.challenge));

		if (user_info->password.response.nt.length != 0) {
			params.password.response.nt_length =
				user_info->password.response.nt.length;
			params.password.response.nt_data =
				user_info->password.response.nt.data;
		}
		if (user_info->password.response.lanman.length != 0) {
			params.password.response.lm_length =
				user_info->password.response.lanman.length;
			params.password.response.lm_data =
				user_info->password.response.lanman.data;
		}
		break;
	}
	default:
		DEBUG(0,("user_info constructed for user '%s' was invalid - password_state=%u invalid.\n",user_info->mapped.account_name, user_info->password_state));
		return NT_STATUS_INTERNAL_ERROR;
#if 0 /* If ever implemented in libwbclient */
	case AUTH_PASSWORD_HASH:
	{
		DEBUG(3,("Checking logon (hash) password for %s.\n",
			 user_info->mapped.account_name));
		params.level = WBC_AUTH_USER_LEVEL_HASH;

		if (user_info->password.hash.nt) {
			memcpy(params.password.hash.nt_hash, user_info->password.hash.nt, sizeof(* user_info->password.hash.nt));
		} else {
			memset(params.password.hash.nt_hash, '\0', sizeof(params.password.hash.nt_hash));
		}

		if (user_info->password.hash.lanman) {
			memcpy(params.password.hash.lm_hash, user_info->password.hash.lanman, sizeof(* user_info->password.hash.lanman));
		} else {
			memset(params.password.hash.lm_hash, '\0', sizeof(params.password.hash.lm_hash));
		}

	}
#endif
	}

	/* we are contacting the privileged pipe */
	become_root();
	wbc_status = wbcAuthenticateUserEx(&params, &info, &err);
	unbecome_root();

	if (!WBC_ERROR_IS_OK(wbc_status)) {
		DEBUG(10,("wbcAuthenticateUserEx failed (%d): %s\n",
			wbc_status, wbcErrorString(wbc_status)));
	}

	if (wbc_status == WBC_ERR_NO_MEMORY) {
		return NT_STATUS_NO_MEMORY;
	}

	if (wbc_status == WBC_ERR_AUTH_ERROR) {
		nt_status = NT_STATUS(err->nt_status);
		wbcFreeMemory(err);
		return nt_status;
	}

	if (!WBC_ERROR_IS_OK(wbc_status)) {
		return NT_STATUS_LOGON_FAILURE;
	}

	DEBUG(10,("wbcAuthenticateUserEx succeeded\n"));

	nt_status = make_server_info_wbcAuthUserInfo(mem_ctx,
						     user_info->client.account_name,
						     user_info->mapped.domain_name,
						     info, server_info);
	wbcFreeMemory(info);
	if (!NT_STATUS_IS_OK(nt_status)) {
		return nt_status;
	}

	(*server_info)->nss_token |= user_info->was_mapped;

        return nt_status;
}

/* module initialisation */
static NTSTATUS auth_init_wbc(struct auth_context *auth_context, const char *param, auth_methods **auth_method)
{
	struct auth_methods *result;

	result = TALLOC_ZERO_P(auth_context, struct auth_methods);
	if (result == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	result->name = "wbc";
	result->auth = check_wbc_security;

	*auth_method = result;
	return NT_STATUS_OK;
}

NTSTATUS auth_wbc_init(void)
{
	return smb_register_auth(AUTH_INTERFACE_VERSION, "wbc", auth_init_wbc);
}
