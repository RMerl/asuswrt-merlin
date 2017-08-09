/* 
   Unix SMB/CIFS implementation.
   Password and authentication handling
   Copyright (C) Andrew Tridgell              1992-2000
   Copyright (C) Luke Kenneth Casson Leighton 1996-2000
   Copyright (C) Andrew Bartlett              2001-2003
   Copyright (C) Gerald Carter                2003

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

static NTSTATUS auth_sam_ignoredomain_auth(const struct auth_context *auth_context,
					   void *my_private_data,
					   TALLOC_CTX *mem_ctx,
					   const struct auth_usersupplied_info *user_info,
					   struct auth_serversupplied_info **server_info)
{
	if (!user_info || !auth_context) {
		return NT_STATUS_UNSUCCESSFUL;
	}
	return check_sam_security(&auth_context->challenge, mem_ctx,
				  user_info, server_info);
}

/* module initialisation */
static NTSTATUS auth_init_sam_ignoredomain(struct auth_context *auth_context, const char *param, auth_methods **auth_method) 
{
	struct auth_methods *result;

	result = TALLOC_ZERO_P(auth_context, struct auth_methods);
	if (result == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	result->auth = auth_sam_ignoredomain_auth;
	result->name = "sam_ignoredomain";

        *auth_method = result;
	return NT_STATUS_OK;
}


/****************************************************************************
Check SAM security (above) but with a few extra checks.
****************************************************************************/

static NTSTATUS auth_samstrict_auth(const struct auth_context *auth_context,
				    void *my_private_data,
				    TALLOC_CTX *mem_ctx,
				    const struct auth_usersupplied_info *user_info,
				    struct auth_serversupplied_info **server_info)
{
	bool is_local_name, is_my_domain;

	if (!user_info || !auth_context) {
		return NT_STATUS_LOGON_FAILURE;
	}

	DEBUG(10, ("Check auth for: [%s]\n", user_info->mapped.account_name));

	is_local_name = is_myname(user_info->mapped.domain_name);
	is_my_domain  = strequal(user_info->mapped.domain_name, lp_workgroup());

	/* check whether or not we service this domain/workgroup name */

	switch ( lp_server_role() ) {
		case ROLE_STANDALONE:
		case ROLE_DOMAIN_MEMBER:
			if ( !is_local_name ) {
				DEBUG(6,("check_samstrict_security: %s is not one of my local names (%s)\n",
					user_info->mapped.domain_name, (lp_server_role() == ROLE_DOMAIN_MEMBER
					? "ROLE_DOMAIN_MEMBER" : "ROLE_STANDALONE") ));
				return NT_STATUS_NOT_IMPLEMENTED;
			}
		case ROLE_DOMAIN_PDC:
		case ROLE_DOMAIN_BDC:
			if ( !is_local_name && !is_my_domain ) {
				DEBUG(6,("check_samstrict_security: %s is not one of my local names or domain name (DC)\n",
					user_info->mapped.domain_name));
				return NT_STATUS_NOT_IMPLEMENTED;
			}
		default: /* name is ok */
			break;
	}

	return check_sam_security(&auth_context->challenge, mem_ctx,
				  user_info, server_info);
}

/* module initialisation */
static NTSTATUS auth_init_sam(struct auth_context *auth_context, const char *param, auth_methods **auth_method) 
{
	struct auth_methods *result;

	result = TALLOC_ZERO_P(auth_context, struct auth_methods);
	if (result == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	result->auth = auth_samstrict_auth;
	result->name = "sam";

        *auth_method = result;
	return NT_STATUS_OK;
}

NTSTATUS auth_sam_init(void)
{
	smb_register_auth(AUTH_INTERFACE_VERSION, "sam", auth_init_sam);
	smb_register_auth(AUTH_INTERFACE_VERSION, "sam_ignoredomain", auth_init_sam_ignoredomain);
	return NT_STATUS_OK;
}
