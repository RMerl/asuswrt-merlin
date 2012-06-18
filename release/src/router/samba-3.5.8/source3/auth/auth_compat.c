/* 
   Unix SMB/CIFS implementation.
   Password and authentication handling
   Copyright (C) Andrew Bartlett         2001-2002

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

extern struct auth_context *negprot_global_auth_context;
extern bool global_encrypted_passwords_negotiated;

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_AUTH

/****************************************************************************
 COMPATIBILITY INTERFACES:
 ***************************************************************************/

/****************************************************************************
check if a username/password is OK assuming the password is a 24 byte
SMB hash
return True if the password is correct, False otherwise
****************************************************************************/

NTSTATUS check_plaintext_password(const char *smb_name, DATA_BLOB plaintext_password, auth_serversupplied_info **server_info)
{
	struct auth_context *plaintext_auth_context = NULL;
	auth_usersupplied_info *user_info = NULL;
	uint8_t chal[8];
	NTSTATUS nt_status;
	if (!NT_STATUS_IS_OK(nt_status = make_auth_context_subsystem(&plaintext_auth_context))) {
		return nt_status;
	}

	plaintext_auth_context->get_ntlm_challenge(plaintext_auth_context,
						   chal);

	if (!make_user_info_for_reply(&user_info, 
				      smb_name, lp_workgroup(), chal,
				      plaintext_password)) {
		return NT_STATUS_NO_MEMORY;
	}

	nt_status = plaintext_auth_context->check_ntlm_password(plaintext_auth_context, 
								user_info, server_info); 

	(plaintext_auth_context->free)(&plaintext_auth_context);
	free_user_info(&user_info);
	return nt_status;
}

static NTSTATUS pass_check_smb(struct auth_context *actx,
			       const char *smb_name,
			       const char *domain, 
			       DATA_BLOB lm_pwd,
			       DATA_BLOB nt_pwd,
			       DATA_BLOB plaintext_password,
			       bool encrypted)

{
	NTSTATUS nt_status;
	auth_serversupplied_info *server_info = NULL;
	if (encrypted) {
		auth_usersupplied_info *user_info = NULL;
		if (actx == NULL) {
			return NT_STATUS_INTERNAL_ERROR;
		}
		make_user_info_for_reply_enc(&user_info, smb_name, 
					     domain,
					     lm_pwd, 
					     nt_pwd);
		nt_status = actx->check_ntlm_password(actx, user_info, &server_info);
		free_user_info(&user_info);
	} else {
		nt_status = check_plaintext_password(smb_name, plaintext_password, &server_info);
	}		
	TALLOC_FREE(server_info);
	return nt_status;
}

/****************************************************************************
check if a username/password pair is ok via the auth subsystem.
return True if the password is correct, False otherwise
****************************************************************************/

bool password_ok(struct auth_context *actx, bool global_encrypted,
		 const char *session_workgroup,
		 const char *smb_name, DATA_BLOB password_blob)
{

	DATA_BLOB null_password = data_blob_null;
	bool encrypted = (global_encrypted && (password_blob.length == 24 || password_blob.length > 46));

	if (encrypted) {
		/* 
		 * The password could be either NTLM or plain LM.  Try NTLM first, 
		 * but fall-through as required.
		 * Vista sends NTLMv2 here - we need to try the client given workgroup.
		 */
		if (session_workgroup) {
			if (NT_STATUS_IS_OK(pass_check_smb(actx, smb_name, session_workgroup, null_password, password_blob, null_password, encrypted))) {
				return True;
			}
			if (NT_STATUS_IS_OK(pass_check_smb(actx, smb_name, session_workgroup, password_blob, null_password, null_password, encrypted))) {
				return True;
			}
		}

		if (NT_STATUS_IS_OK(pass_check_smb(actx, smb_name, lp_workgroup(), null_password, password_blob, null_password, encrypted))) {
			return True;
		}

		if (NT_STATUS_IS_OK(pass_check_smb(actx, smb_name, lp_workgroup(), password_blob, null_password, null_password, encrypted))) {
			return True;
		}
	} else {
		if (NT_STATUS_IS_OK(pass_check_smb(actx, smb_name, lp_workgroup(), null_password, null_password, password_blob, encrypted))) {
			return True;
		}
	}

	return False;
}
