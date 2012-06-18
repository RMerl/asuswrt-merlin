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
#include "auth/auth_proto.h"
#include "libcli/security/security.h"
#include "libcli/auth/libcli_auth.h"
#include "dsdb/samdb/samdb.h"
#include "auth/credentials/credentials.h"
#include "param/param.h"

/* this default function can be used by mostly all backends
 * which don't want to set a challenge
 */
NTSTATUS auth_get_challenge_not_implemented(struct auth_method_context *ctx, TALLOC_CTX *mem_ctx, DATA_BLOB *challenge)
{
	/* we don't want to set a challenge */
	return NT_STATUS_NOT_IMPLEMENTED;
}

/****************************************************************************
 Create an auth_usersupplied_data structure after appropriate mapping.
****************************************************************************/

NTSTATUS map_user_info(TALLOC_CTX *mem_ctx,
		       const char *default_domain,
		       const struct auth_usersupplied_info *user_info,
		       struct auth_usersupplied_info **user_info_mapped)
{
	const char *domain;
	char *account_name;
	char *d;
	DEBUG(5,("map_user_info: Mapping user [%s]\\[%s] from workstation [%s]\n",
		user_info->client.domain_name, user_info->client.account_name, user_info->workstation_name));

	account_name = talloc_strdup(mem_ctx, user_info->client.account_name);
	if (!account_name) {
		return NT_STATUS_NO_MEMORY;
	}
	
	/* don't allow "" as a domain, fixes a Win9X bug 
	   where it doesn't supply a domain for logon script
	   'net use' commands.                                 */

	/* Split user@realm names into user and realm components.  This is TODO to fix with proper userprincipalname support */
	if (user_info->client.domain_name && *user_info->client.domain_name) {
		domain = user_info->client.domain_name;
	} else if (strchr_m(user_info->client.account_name, '@')) {
		d = strchr_m(account_name, '@');
		if (!d) {
			return NT_STATUS_INTERNAL_ERROR;
		}
		d[0] = '\0';
		d++;
		domain = d;
	} else {
		domain = default_domain;
	}

	*user_info_mapped = talloc(mem_ctx, struct auth_usersupplied_info);
	if (!*user_info_mapped) {
		return NT_STATUS_NO_MEMORY;
	}
	if (!talloc_reference(*user_info_mapped, user_info)) {
		return NT_STATUS_NO_MEMORY;
	}
	**user_info_mapped = *user_info;
	(*user_info_mapped)->mapped_state = true;
	(*user_info_mapped)->mapped.domain_name = talloc_strdup(*user_info_mapped, domain);
	(*user_info_mapped)->mapped.account_name = talloc_strdup(*user_info_mapped, account_name);
	talloc_free(account_name);
	if (!(*user_info_mapped)->mapped.domain_name 
	    || !(*user_info_mapped)->mapped.account_name) {
		return NT_STATUS_NO_MEMORY;
	}

	return NT_STATUS_OK;
}

/****************************************************************************
 Create an auth_usersupplied_data structure after appropriate mapping.
****************************************************************************/

NTSTATUS encrypt_user_info(TALLOC_CTX *mem_ctx, struct auth_context *auth_context, 
			   enum auth_password_state to_state,
			   const struct auth_usersupplied_info *user_info_in,
			   const struct auth_usersupplied_info **user_info_encrypted)
{
	NTSTATUS nt_status;
	struct auth_usersupplied_info *user_info_temp;
	switch (to_state) {
	case AUTH_PASSWORD_RESPONSE:
		switch (user_info_in->password_state) {
		case AUTH_PASSWORD_PLAIN:
		{
			const struct auth_usersupplied_info *user_info_temp2;
			nt_status = encrypt_user_info(mem_ctx, auth_context, 
						      AUTH_PASSWORD_HASH, 
						      user_info_in, &user_info_temp2);
			if (!NT_STATUS_IS_OK(nt_status)) {
				return nt_status;
			}
			user_info_in = user_info_temp2;
			/* fall through */
		}
		case AUTH_PASSWORD_HASH:
		{
			const uint8_t *challenge;
			DATA_BLOB chall_blob;
			user_info_temp = talloc(mem_ctx, struct auth_usersupplied_info);
			if (!user_info_temp) {
				return NT_STATUS_NO_MEMORY;
			}
			if (!talloc_reference(user_info_temp, user_info_in)) {
				return NT_STATUS_NO_MEMORY;
			}
			*user_info_temp = *user_info_in;
			user_info_temp->mapped_state = to_state;
			
			nt_status = auth_get_challenge(auth_context, &challenge);
			if (!NT_STATUS_IS_OK(nt_status)) {
				return nt_status;
			}
			
			chall_blob = data_blob_talloc(mem_ctx, challenge, 8);
			if (lp_client_ntlmv2_auth(auth_context->lp_ctx)) {
				DATA_BLOB names_blob = NTLMv2_generate_names_blob(mem_ctx,  lp_netbios_name(auth_context->lp_ctx), lp_workgroup(auth_context->lp_ctx));
				DATA_BLOB lmv2_response, ntlmv2_response, lmv2_session_key, ntlmv2_session_key;
				
				if (!SMBNTLMv2encrypt_hash(user_info_temp,
							   user_info_in->client.account_name, 
							   user_info_in->client.domain_name, 
							   user_info_in->password.hash.nt->hash, &chall_blob,
							   &names_blob,
							   &lmv2_response, &ntlmv2_response, 
							   &lmv2_session_key, &ntlmv2_session_key)) {
					data_blob_free(&names_blob);
					return NT_STATUS_NO_MEMORY;
				}
				data_blob_free(&names_blob);
				user_info_temp->password.response.lanman = lmv2_response;
				user_info_temp->password.response.nt = ntlmv2_response;
				
				data_blob_free(&lmv2_session_key);
				data_blob_free(&ntlmv2_session_key);
			} else {
				DATA_BLOB blob = data_blob_talloc(mem_ctx, NULL, 24);
				SMBOWFencrypt(user_info_in->password.hash.nt->hash, challenge, blob.data);

				user_info_temp->password.response.nt = blob;
				if (lp_client_lanman_auth(auth_context->lp_ctx) && user_info_in->password.hash.lanman) {
					DATA_BLOB lm_blob = data_blob_talloc(mem_ctx, NULL, 24);
					SMBOWFencrypt(user_info_in->password.hash.lanman->hash, challenge, blob.data);
					user_info_temp->password.response.lanman = lm_blob;
				} else {
					/* if not sending the LM password, send the NT password twice */
					user_info_temp->password.response.lanman = user_info_temp->password.response.nt;
				}
			}

			user_info_in = user_info_temp;
			/* fall through */
		}
		case AUTH_PASSWORD_RESPONSE:
			*user_info_encrypted = user_info_in;
		}
		break;
	case AUTH_PASSWORD_HASH:
	{	
		switch (user_info_in->password_state) {
		case AUTH_PASSWORD_PLAIN:
		{
			struct samr_Password lanman;
			struct samr_Password nt;
			
			user_info_temp = talloc(mem_ctx, struct auth_usersupplied_info);
			if (!user_info_temp) {
				return NT_STATUS_NO_MEMORY;
			}
			if (!talloc_reference(user_info_temp, user_info_in)) {
				return NT_STATUS_NO_MEMORY;
			}
			*user_info_temp = *user_info_in;
			user_info_temp->mapped_state = to_state;
			
			if (E_deshash(user_info_in->password.plaintext, lanman.hash)) {
				user_info_temp->password.hash.lanman = talloc(user_info_temp,
									      struct samr_Password);
				*user_info_temp->password.hash.lanman = lanman;
			} else {
				user_info_temp->password.hash.lanman = NULL;
			}
			
			E_md4hash(user_info_in->password.plaintext, nt.hash);
			user_info_temp->password.hash.nt = talloc(user_info_temp,
								   struct samr_Password);
			*user_info_temp->password.hash.nt = nt;
			
			user_info_in = user_info_temp;
			/* fall through */
		}
		case AUTH_PASSWORD_HASH:
			*user_info_encrypted = user_info_in;
			break;
		default:
			return NT_STATUS_INVALID_PARAMETER;
			break;
		}
		break;
	}
	default:
		return NT_STATUS_INVALID_PARAMETER;
	}

	return NT_STATUS_OK;
}


/**
 * Squash an NT_STATUS in line with security requirements.
 * In an attempt to avoid giving the whole game away when users
 * are authenticating, NT replaces both NT_STATUS_NO_SUCH_USER and 
 * NT_STATUS_WRONG_PASSWORD with NT_STATUS_LOGON_FAILURE in certain situations 
 * (session setups in particular).
 *
 * @param nt_status NTSTATUS input for squashing.
 * @return the 'squashed' nt_status
 **/
_PUBLIC_ NTSTATUS auth_nt_status_squash(NTSTATUS nt_status)
{
	if NT_STATUS_EQUAL(nt_status, NT_STATUS_NO_SUCH_USER) {
		/* Match WinXP and don't give the game away */
		return NT_STATUS_LOGON_FAILURE;
	} else if NT_STATUS_EQUAL(nt_status, NT_STATUS_WRONG_PASSWORD) {
		/* Match WinXP and don't give the game away */
		return NT_STATUS_LOGON_FAILURE;
	}

	return nt_status;
}
