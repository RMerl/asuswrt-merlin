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

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_AUTH

/* Authenticate a user with a challenge/response */

static NTSTATUS check_winbind_security(const struct auth_context *auth_context,
				       void *my_private_data, 
				       TALLOC_CTX *mem_ctx,
				       const auth_usersupplied_info *user_info, 
				       auth_serversupplied_info **server_info)
{
	NTSTATUS nt_status;
	wbcErr wbc_status;
	struct wbcAuthUserParams params;
	struct wbcAuthUserInfo *info = NULL;
	struct wbcAuthErrorInfo *err = NULL;

	if (!user_info) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (!auth_context) {
		DEBUG(3,("Password for user %s cannot be checked because we have no auth_info to get the challenge from.\n", 
			 user_info->internal_username));
		return NT_STATUS_INVALID_PARAMETER;
	}		

	if (strequal(user_info->domain, get_global_sam_name())) {
		DEBUG(3,("check_winbind_security: Not using winbind, requested domain [%s] was for this SAM.\n",
			user_info->domain));
		return NT_STATUS_NOT_IMPLEMENTED;
	}

	/* Send off request */

	params.account_name	= user_info->smb_name;
	params.domain_name	= user_info->domain;
	params.workstation_name	= user_info->wksta_name;

	params.flags		= 0;
	params.parameter_control= user_info->logon_parameters;

	params.level		= WBC_AUTH_USER_LEVEL_RESPONSE;

	memcpy(params.password.response.challenge,
	       auth_context->challenge.data,
	       sizeof(params.password.response.challenge));

	params.password.response.nt_length	= user_info->nt_resp.length;
	params.password.response.nt_data	= user_info->nt_resp.data;
	params.password.response.lm_length	= user_info->lm_resp.length;
	params.password.response.lm_data	= user_info->lm_resp.data;

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
		else
			/* log an error since this should not happen */
			DEBUG(0,("check_winbind_security: ERROR!  my_private_data == NULL!\n"));
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
						     user_info->smb_name,
						     user_info->domain,
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
	if (!make_auth_methods(auth_context, auth_method)) {
		return NT_STATUS_NO_MEMORY;
	}

	(*auth_method)->name = "winbind";
	(*auth_method)->auth = check_winbind_security;

	if (param && *param) {
		/* we load the 'fallback' module - if winbind isn't here, call this
		   module */
		auth_methods *priv;
		if (!load_auth_module(auth_context, param, &priv)) {
			return NT_STATUS_UNSUCCESSFUL;
		}
		(*auth_method)->private_data = (void *)priv;
	}
	return NT_STATUS_OK;
}

NTSTATUS auth_winbind_init(void)
{
	return smb_register_auth(AUTH_INTERFACE_VERSION, "winbind", auth_init_winbind);
}
