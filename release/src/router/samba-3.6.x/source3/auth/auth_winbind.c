/* 
   Unix SMB/CIFS implementation.

   Winbind authentication mechnism

   Copyright (C) Tim Potter 2000
   Copyright (C) Andrew Bartlett 2001 - 2002

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
#include "nsswitch/libwbclient/wbclient.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_AUTH

/* Authenticate a user with a challenge/response */

static NTSTATUS check_winbind_security(const struct auth_context *auth_context,
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

	ZERO_STRUCT(params);

	if (!user_info) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	DEBUG(10, ("Check auth for: [%s]\n", user_info->mapped.account_name));

	if (!auth_context) {
		DEBUG(3,("Password for user %s cannot be checked because we have no auth_info to get the challenge from.\n", 
			 user_info->mapped.account_name));
		return NT_STATUS_INVALID_PARAMETER;
	}		

	if (strequal(user_info->mapped.domain_name, get_global_sam_name())) {
		DEBUG(3,("check_winbind_security: Not using winbind, requested domain [%s] was for this SAM.\n",
			user_info->mapped.domain_name));
		return NT_STATUS_NOT_IMPLEMENTED;
	}

	/* Send off request */
	params.account_name	= user_info->client.account_name;
	/*
	 * We need to send the domain name from the client to the DC. With
	 * NTLMv2 the domain name is part of the hashed second challenge,
	 * if we change the domain name, the DC will fail to verify the
	 * challenge cause we changed the domain name, this is like a
	 * man in the middle attack.
	 */
	params.domain_name	= user_info->client.domain_name;
	params.workstation_name	= user_info->workstation_name;

	params.flags		= 0;
	params.parameter_control= user_info->logon_parameters;

	params.level		= WBC_AUTH_USER_LEVEL_RESPONSE;

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

	/* we are contacting the privileged pipe */
	become_root();
	wbc_status = wbcAuthenticateUserEx(&params, &info, &err);
	unbecome_root();

	if (!WBC_ERROR_IS_OK(wbc_status)) {
		DEBUG(10,("check_winbind_security: wbcAuthenticateUserEx failed: %s\n",
			wbcErrorString(wbc_status)));
	}

	if (wbc_status == WBC_ERR_NO_MEMORY) {
		return NT_STATUS_NO_MEMORY;
	}

	if (wbc_status == WBC_ERR_WINBIND_NOT_AVAILABLE) {
		struct auth_methods *auth_method =
			(struct auth_methods *)my_private_data;

		if ( auth_method )
			return auth_method->auth(auth_context, auth_method->private_data, 
				mem_ctx, user_info, server_info);
		return NT_STATUS_LOGON_FAILURE;
	}

	if (wbc_status == WBC_ERR_AUTH_ERROR) {
		nt_status = NT_STATUS(err->nt_status);
		wbcFreeMemory(err);
		return nt_status;
	}

	if (!WBC_ERROR_IS_OK(wbc_status)) {
		return NT_STATUS_LOGON_FAILURE;
	}

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
static NTSTATUS auth_init_winbind(struct auth_context *auth_context, const char *param, auth_methods **auth_method) 
{
	struct auth_methods *result;

	result = TALLOC_ZERO_P(auth_context, struct auth_methods);
	if (result == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	result->name = "winbind";
	result->auth = check_winbind_security;

	if (param && *param) {
		/* we load the 'fallback' module - if winbind isn't here, call this
		   module */
		auth_methods *priv;
		if (!load_auth_module(auth_context, param, &priv)) {
			return NT_STATUS_UNSUCCESSFUL;
		}
		result->private_data = (void *)priv;
	}

	*auth_method = result;
	return NT_STATUS_OK;
}

NTSTATUS auth_winbind_init(void)
{
	return smb_register_auth(AUTH_INTERFACE_VERSION, "winbind", auth_init_winbind);
}
