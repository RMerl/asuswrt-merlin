/* 
   Unix SMB/CIFS implementation.
   Generic authentication types
   Copyright (C) Andrew Bartlett         2001-2002
   Copyright (C) Jelmer Vernooij              2002

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

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_AUTH

/**
 * Return a guest logon for guest users (username = "")
 *
 * Typically used as the first module in the auth chain, this allows
 * guest logons to be dealt with in one place.  Non-guest logons 'fail'
 * and pass onto the next module.
 **/

static NTSTATUS check_guest_security(const struct auth_context *auth_context,
				     void *my_private_data, 
				     TALLOC_CTX *mem_ctx,
				     const struct auth_usersupplied_info *user_info,
				     struct auth_serversupplied_info **server_info)
{
	/* mark this as 'not for me' */
	NTSTATUS nt_status = NT_STATUS_NOT_IMPLEMENTED;

	DEBUG(10, ("Check auth for: [%s]\n", user_info->mapped.account_name));

	if (!(user_info->mapped.account_name
	      && *user_info->mapped.account_name)) {
		nt_status = make_server_info_guest(NULL, server_info);
	}

	return nt_status;
}

/* Guest modules initialisation */

static NTSTATUS auth_init_guest(struct auth_context *auth_context, const char *options, auth_methods **auth_method) 
{
	struct auth_methods *result;

	result = TALLOC_ZERO_P(auth_context, struct auth_methods);
	if (result == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	result->auth = check_guest_security;
	result->name = "guest";

        *auth_method = result;
	return NT_STATUS_OK;
}

#ifdef DEVELOPER
/** 
 * Return an error based on username
 *
 * This function allows the testing of obsure errors, as well as the generation
 * of NT_STATUS -> DOS error mapping tables.
 *
 * This module is of no value to end-users.
 *
 * The password is ignored.
 *
 * @return An NTSTATUS value based on the username
 **/

static NTSTATUS check_name_to_ntstatus_security(const struct auth_context *auth_context,
						void *my_private_data, 
						TALLOC_CTX *mem_ctx,
						const struct auth_usersupplied_info *user_info,
						struct auth_serversupplied_info **server_info)
{
	NTSTATUS nt_status;
	fstring user;
	long error_num;

	DEBUG(10, ("Check auth for: [%s]\n", user_info->mapped.account_name));

	fstrcpy(user, user_info->client.account_name);

	if (strnequal("NT_STATUS", user, strlen("NT_STATUS"))) {
		strupper_m(user);
		return nt_status_string_to_code(user);
	}

	strlower_m(user);
	error_num = strtoul(user, NULL, 16);

	DEBUG(5,("check_name_to_ntstatus_security: Error for user %s was %lx\n", user, error_num));

	nt_status = NT_STATUS(error_num);

	return nt_status;
}

/** Module initialisation function */

static NTSTATUS auth_init_name_to_ntstatus(struct auth_context *auth_context, const char *param, auth_methods **auth_method) 
{
	struct auth_methods *result;

	result = TALLOC_ZERO_P(auth_context, struct auth_methods);
	if (result == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	result->auth = check_name_to_ntstatus_security;
	result->name = "name_to_ntstatus";

        *auth_method = result;
	return NT_STATUS_OK;
}

/** 
 * Return a 'fixed' challenge instead of a variable one.
 *
 * The idea of this function is to make packet snifs consistant
 * with a fixed challenge, so as to aid debugging.
 *
 * This module is of no value to end-users.
 *
 * This module does not actually authenticate the user, but
 * just pretenteds to need a specified challenge.  
 * This module removes *all* security from the challenge-response system
 *
 * @return NT_STATUS_UNSUCCESSFUL
 **/

static NTSTATUS check_fixed_challenge_security(const struct auth_context *auth_context,
					       void *my_private_data, 
					       TALLOC_CTX *mem_ctx,
					       const struct auth_usersupplied_info *user_info,
					       struct auth_serversupplied_info **server_info)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}

/****************************************************************************
 Get the challenge out of a password server.
****************************************************************************/

static DATA_BLOB auth_get_fixed_challenge(const struct auth_context *auth_context,
					  void **my_private_data, 
					  TALLOC_CTX *mem_ctx)
{
	const char *challenge = "I am a teapot";   
	return data_blob(challenge, 8);
}


/** Module initialisation function */

static NTSTATUS auth_init_fixed_challenge(struct auth_context *auth_context, const char *param, auth_methods **auth_method) 
{
	struct auth_methods *result;

	result = TALLOC_ZERO_P(auth_context, struct auth_methods);
	if (result == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	result->auth = check_fixed_challenge_security;
	result->get_chal = auth_get_fixed_challenge;
	result->name = "fixed_challenge";

	*auth_method = result;
	return NT_STATUS_OK;
}
#endif /* DEVELOPER */

NTSTATUS auth_builtin_init(void)
{
	smb_register_auth(AUTH_INTERFACE_VERSION, "guest", auth_init_guest);
#ifdef DEVELOPER
	smb_register_auth(AUTH_INTERFACE_VERSION, "fixed_challenge", auth_init_fixed_challenge);
	smb_register_auth(AUTH_INTERFACE_VERSION, "name_to_ntstatus", auth_init_name_to_ntstatus);
#endif
	return NT_STATUS_OK;
}
