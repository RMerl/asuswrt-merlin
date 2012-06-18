/* 
   Unix SMB/CIFS implementation.

   User credentials handling

   Copyright (C) Andrew Tridgell      2001
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2001-2005
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
#include "librpc/gen_ndr/samr.h" /* for struct samrPassword */
#include "../lib/crypto/crypto.h"
#include "libcli/auth/libcli_auth.h"
#include "auth/credentials/credentials.h"

_PUBLIC_ void cli_credentials_get_ntlm_username_domain(struct cli_credentials *cred, TALLOC_CTX *mem_ctx, 
					      const char **username, 
					      const char **domain) 
{
	if (cred->principal_obtained > cred->username_obtained) {
		*domain = talloc_strdup(mem_ctx, "");
		*username = cli_credentials_get_principal(cred, mem_ctx);
	} else {
		*domain = cli_credentials_get_domain(cred);
		*username = cli_credentials_get_username(cred);
	}
}

_PUBLIC_ NTSTATUS cli_credentials_get_ntlm_response(struct cli_credentials *cred, TALLOC_CTX *mem_ctx, 
					   int *flags,
					   DATA_BLOB challenge, DATA_BLOB target_info, 
					   DATA_BLOB *_lm_response, DATA_BLOB *_nt_response, 
					   DATA_BLOB *_lm_session_key, DATA_BLOB *_session_key) 
{
	const char *user, *domain;
	DATA_BLOB lm_response, nt_response;
	DATA_BLOB lm_session_key, session_key;
	const struct samr_Password *nt_hash;
	lm_session_key = data_blob(NULL, 0);

	/* We may already have an NTLM response we prepared earlier.
	 * This is used for NTLM pass-though authentication */
	if (cred->nt_response.data || cred->lm_response.data) {
		*_nt_response = cred->nt_response;
		*_lm_response = cred->lm_response;

		if (!cred->lm_response.data) {
			*flags = *flags & ~CLI_CRED_LANMAN_AUTH;
		}
		*_lm_session_key = data_blob(NULL, 0);
		*_session_key = data_blob(NULL, 0);
		return NT_STATUS_OK;
	}

	nt_hash = cli_credentials_get_nt_hash(cred, mem_ctx);

	cli_credentials_get_ntlm_username_domain(cred, mem_ctx, &user, &domain);

	/* If we are sending a username@realm login (see function
	 * above), then we will not send LM, it will not be
	 * accepted */
	if (cred->principal_obtained > cred->username_obtained) {
		*flags = *flags & ~CLI_CRED_LANMAN_AUTH;
	}

	/* Likewise if we are a machine account (avoid protocol downgrade attacks) */
	if (cred->machine_account) {
		*flags = *flags & ~CLI_CRED_LANMAN_AUTH;
	}
	
	if (cred->use_kerberos == CRED_MUST_USE_KERBEROS) {
		return NT_STATUS_ACCESS_DENIED;
	}

	if (!nt_hash) {
		static const uint8_t zeros[16];
		/* do nothing - blobs are zero length */

		/* session key is all zeros */
		session_key = data_blob_talloc(mem_ctx, zeros, 16);
		lm_session_key = data_blob_talloc(mem_ctx, zeros, 16);

		lm_response = data_blob(NULL, 0);
		nt_response = data_blob(NULL, 0);
		
		/* not doing NTLM2 without a password */
		*flags &= ~CLI_CRED_NTLM2;
	} else if (*flags & CLI_CRED_NTLMv2_AUTH) {

		if (!target_info.length) {
			/* be lazy, match win2k - we can't do NTLMv2 without it */
			DEBUG(1, ("Server did not provide 'target information', required for NTLMv2\n"));
			return NT_STATUS_INVALID_PARAMETER;
		}

		/* TODO: if the remote server is standalone, then we should replace 'domain'
		   with the server name as supplied above */
		
		if (!SMBNTLMv2encrypt_hash(mem_ctx,
					   user, 
					   domain, 
					   nt_hash->hash, &challenge, 
					   &target_info, 
					   &lm_response, &nt_response, 
					   NULL, &session_key)) {
			return NT_STATUS_NO_MEMORY;
		}

		/* LM Key is incompatible... */
		*flags &= ~CLI_CRED_LANMAN_AUTH;
	} else if (*flags & CLI_CRED_NTLM2) {
		struct MD5Context md5_session_nonce_ctx;
		uint8_t session_nonce[16];
		uint8_t session_nonce_hash[16];
		uint8_t user_session_key[16];
		
		lm_response = data_blob_talloc(mem_ctx, NULL, 24);
		generate_random_buffer(lm_response.data, 8);
		memset(lm_response.data+8, 0, 16);

		memcpy(session_nonce, challenge.data, 8);
		memcpy(&session_nonce[8], lm_response.data, 8);
	
		MD5Init(&md5_session_nonce_ctx);
		MD5Update(&md5_session_nonce_ctx, challenge.data, 8);
		MD5Update(&md5_session_nonce_ctx, lm_response.data, 8);
		MD5Final(session_nonce_hash, &md5_session_nonce_ctx);

		DEBUG(5, ("NTLMSSP challenge set by NTLM2\n"));
		DEBUG(5, ("challenge is: \n"));
		dump_data(5, session_nonce_hash, 8);
		
		nt_response = data_blob_talloc(mem_ctx, NULL, 24);
		SMBOWFencrypt(nt_hash->hash,
			      session_nonce_hash,
			      nt_response.data);
		
		session_key = data_blob_talloc(mem_ctx, NULL, 16);

		SMBsesskeygen_ntv1(nt_hash->hash, user_session_key);
		hmac_md5(user_session_key, session_nonce, sizeof(session_nonce), session_key.data);
		dump_data_pw("NTLM2 session key:\n", session_key.data, session_key.length);

		/* LM Key is incompatible... */
		*flags &= ~CLI_CRED_LANMAN_AUTH;
	} else {
		uint8_t lm_hash[16];
		nt_response = data_blob_talloc(mem_ctx, NULL, 24);
		SMBOWFencrypt(nt_hash->hash, challenge.data,
			      nt_response.data);
		
		session_key = data_blob_talloc(mem_ctx, NULL, 16);
		SMBsesskeygen_ntv1(nt_hash->hash, session_key.data);
		dump_data_pw("NT session key:\n", session_key.data, session_key.length);

		/* lanman auth is insecure, it may be disabled.  
		   We may also not have a password */
		if (*flags & CLI_CRED_LANMAN_AUTH) {
			const char *password;
			password = cli_credentials_get_password(cred);
			if (!password) {
				lm_response = nt_response;
			} else {
				lm_response = data_blob_talloc(mem_ctx, NULL, 24);
				if (!SMBencrypt(password,challenge.data,
						lm_response.data)) {
					/* If the LM password was too long (and therefore the LM hash being
					   of the first 14 chars only), don't send it.

					   We don't have any better options but to send the NT response 
					*/
					data_blob_free(&lm_response);
					lm_response = nt_response;
					/* LM Key is incompatible with 'long' passwords */
					*flags &= ~CLI_CRED_LANMAN_AUTH;
				} else {
					E_deshash(password, lm_hash);
					lm_session_key = data_blob_talloc(mem_ctx, NULL, 16);
					memcpy(lm_session_key.data, lm_hash, 8);
					memset(&lm_session_key.data[8], '\0', 8);
					
					if (!(*flags & CLI_CRED_NTLM_AUTH)) {
						session_key = lm_session_key;
					}
				}
			}
		} else {
			const char *password;

			/* LM Key is incompatible... */
			lm_response = nt_response;
			*flags &= ~CLI_CRED_LANMAN_AUTH;

			password = cli_credentials_get_password(cred);
			if (password) {
				E_deshash(password, lm_hash);
				lm_session_key = data_blob_talloc(mem_ctx, NULL, 16);
				memcpy(lm_session_key.data, lm_hash, 8);
				memset(&lm_session_key.data[8], '\0', 8);
			}
		}
	}
	if (_lm_response) {
		*_lm_response = lm_response;
	}
	if (_nt_response) {
		*_nt_response = nt_response;
	}
	if (_lm_session_key) {
		*_lm_session_key = lm_session_key;
	}
	if (_session_key) {
		*_session_key = session_key;
	}
	return NT_STATUS_OK;
}
	
_PUBLIC_ bool cli_credentials_set_nt_hash(struct cli_credentials *cred,
				 const struct samr_Password *nt_hash, 
				 enum credentials_obtained obtained)
{
	if (obtained >= cred->password_obtained) {
		cli_credentials_set_password(cred, NULL, obtained);
		if (nt_hash) {
			cred->nt_hash = talloc(cred, struct samr_Password);
			*cred->nt_hash = *nt_hash;
		} else {
			cred->nt_hash = NULL;
		}
		return true;
	}

	return false;
}

_PUBLIC_ bool cli_credentials_set_ntlm_response(struct cli_credentials *cred,
						const DATA_BLOB *lm_response, 
						const DATA_BLOB *nt_response, 
						enum credentials_obtained obtained)
{
	if (obtained >= cred->password_obtained) {
		cli_credentials_set_password(cred, NULL, obtained);
		if (nt_response) {
			cred->nt_response = data_blob_talloc(cred, nt_response->data, nt_response->length);
			talloc_steal(cred, cred->nt_response.data);
		}
		if (nt_response) {
			cred->lm_response = data_blob_talloc(cred, lm_response->data, lm_response->length);
		}
		return true;
	}

	return false;
}

