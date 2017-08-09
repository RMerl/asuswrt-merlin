/* 
   Unix SMB/CIFS implementation.
   Password and authentication handling
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2001-2004
   Copyright (C) Gerald Carter                             2003
   Copyright (C) Luke Kenneth Casson Leighton         1996-2000

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
#include "../lib/crypto/crypto.h"
#include "librpc/gen_ndr/netlogon.h"
#include "libcli/auth/libcli_auth.h"

/****************************************************************************
 Core of smb password checking routine.
****************************************************************************/

static bool smb_pwd_check_ntlmv1(TALLOC_CTX *mem_ctx,
				 const DATA_BLOB *nt_response,
				 const uint8_t *part_passwd,
				 const DATA_BLOB *sec_blob,
				 DATA_BLOB *user_sess_key)
{
	/* Finish the encryption of part_passwd. */
	uint8_t p24[24];

	if (part_passwd == NULL) {
		DEBUG(10,("No password set - DISALLOWING access\n"));
		/* No password set - always false ! */
		return false;
	}

	if (sec_blob->length != 8) {
		DEBUG(0, ("smb_pwd_check_ntlmv1: incorrect challenge size (%lu)\n", 
			  (unsigned long)sec_blob->length));
		return false;
	}

	if (nt_response->length != 24) {
		DEBUG(0, ("smb_pwd_check_ntlmv1: incorrect password length (%lu)\n", 
			  (unsigned long)nt_response->length));
		return false;
	}

	SMBOWFencrypt(part_passwd, sec_blob->data, p24);

#if DEBUG_PASSWORD
	DEBUG(100,("Part password (P16) was |\n"));
	dump_data(100, part_passwd, 16);
	DEBUGADD(100,("Password from client was |\n"));
	dump_data(100, nt_response->data, nt_response->length);
	DEBUGADD(100,("Given challenge was |\n"));
	dump_data(100, sec_blob->data, sec_blob->length);
	DEBUGADD(100,("Value from encryption was |\n"));
	dump_data(100, p24, 24);
#endif
	if (memcmp(p24, nt_response->data, 24) == 0) {
		if (user_sess_key != NULL) {
			*user_sess_key = data_blob_talloc(mem_ctx, NULL, 16);
			SMBsesskeygen_ntv1(part_passwd, user_sess_key->data);
		}
		return true;
	} 
	return false;
}

/****************************************************************************
 Core of smb password checking routine. (NTLMv2, LMv2)
 Note:  The same code works with both NTLMv2 and LMv2.
****************************************************************************/

static bool smb_pwd_check_ntlmv2(TALLOC_CTX *mem_ctx,
				 const DATA_BLOB *ntv2_response,
				 const uint8_t *part_passwd,
				 const DATA_BLOB *sec_blob,
				 const char *user, const char *domain,
				 DATA_BLOB *user_sess_key)
{
	/* Finish the encryption of part_passwd. */
	uint8_t kr[16];
	uint8_t value_from_encryption[16];
	DATA_BLOB client_key_data;

	if (part_passwd == NULL) {
		DEBUG(10,("No password set - DISALLOWING access\n"));
		/* No password set - always false */
		return false;
	}

	if (sec_blob->length != 8) {
		DEBUG(0, ("smb_pwd_check_ntlmv2: incorrect challenge size (%lu)\n", 
			  (unsigned long)sec_blob->length));
		return false;
	}

	if (ntv2_response->length < 24) {
		/* We MUST have more than 16 bytes, or the stuff below will go
		   crazy.  No known implementation sends less than the 24 bytes
		   for LMv2, let alone NTLMv2. */
		DEBUG(0, ("smb_pwd_check_ntlmv2: incorrect password length (%lu)\n", 
			  (unsigned long)ntv2_response->length));
		return false;
	}

	client_key_data = data_blob_talloc(mem_ctx, ntv2_response->data+16, ntv2_response->length-16);
	/* 
	   todo:  should we be checking this for anything?  We can't for LMv2, 
	   but for NTLMv2 it is meant to contain the current time etc.
	*/

	if (!ntv2_owf_gen(part_passwd, user, domain, kr)) {
		return false;
	}

	SMBOWFencrypt_ntv2(kr, sec_blob, &client_key_data, value_from_encryption);

#if DEBUG_PASSWORD
	DEBUG(100,("Part password (P16) was |\n"));
	dump_data(100, part_passwd, 16);
	DEBUGADD(100,("Password from client was |\n"));
	dump_data(100, ntv2_response->data, ntv2_response->length);
	DEBUGADD(100,("Variable data from client was |\n"));
	dump_data(100, client_key_data.data, client_key_data.length);
	DEBUGADD(100,("Given challenge was |\n"));
	dump_data(100, sec_blob->data, sec_blob->length);
	DEBUGADD(100,("Value from encryption was |\n"));
	dump_data(100, value_from_encryption, 16);
#endif
	data_blob_clear_free(&client_key_data);
	if (memcmp(value_from_encryption, ntv2_response->data, 16) == 0) { 
		if (user_sess_key != NULL) {
			*user_sess_key = data_blob_talloc(mem_ctx, NULL, 16);
			SMBsesskeygen_ntv2(kr, value_from_encryption, user_sess_key->data);
		}
		return true;
	}
	return false;
}

/****************************************************************************
 Core of smb password checking routine. (NTLMv2, LMv2)
 Note:  The same code works with both NTLMv2 and LMv2.
****************************************************************************/

static bool smb_sess_key_ntlmv2(TALLOC_CTX *mem_ctx,
				const DATA_BLOB *ntv2_response,
				const uint8_t *part_passwd,
				const DATA_BLOB *sec_blob,
				const char *user, const char *domain,
				DATA_BLOB *user_sess_key)
{
	/* Finish the encryption of part_passwd. */
	uint8_t kr[16];
	uint8_t value_from_encryption[16];
	DATA_BLOB client_key_data;

	if (part_passwd == NULL) {
		DEBUG(10,("No password set - DISALLOWING access\n"));
		/* No password set - always false */
		return false;
	}

	if (sec_blob->length != 8) {
		DEBUG(0, ("smb_sess_key_ntlmv2: incorrect challenge size (%lu)\n", 
			  (unsigned long)sec_blob->length));
		return false;
	}

	if (ntv2_response->length < 24) {
		/* We MUST have more than 16 bytes, or the stuff below will go
		   crazy.  No known implementation sends less than the 24 bytes
		   for LMv2, let alone NTLMv2. */
		DEBUG(0, ("smb_sess_key_ntlmv2: incorrect password length (%lu)\n", 
			  (unsigned long)ntv2_response->length));
		return false;
	}

	client_key_data = data_blob_talloc(mem_ctx, ntv2_response->data+16, ntv2_response->length-16);

	if (!ntv2_owf_gen(part_passwd, user, domain, kr)) {
		return false;
	}

	SMBOWFencrypt_ntv2(kr, sec_blob, &client_key_data, value_from_encryption);
	*user_sess_key = data_blob_talloc(mem_ctx, NULL, 16);
	SMBsesskeygen_ntv2(kr, value_from_encryption, user_sess_key->data);
	return true;
}

/**
 * Compare password hashes against those from the SAM
 *
 * @param mem_ctx talloc context
 * @param client_lanman LANMAN password hash, as supplied by the client
 * @param client_nt NT (MD4) password hash, as supplied by the client
 * @param username internal Samba username, for log messages
 * @param client_username username the client used
 * @param client_domain domain name the client used (may be mapped)
 * @param stored_lanman LANMAN password hash, as stored on the SAM
 * @param stored_nt NT (MD4) password hash, as stored on the SAM
 * @param user_sess_key User session key
 * @param lm_sess_key LM session key (first 8 bytes of the LM hash)
 */

NTSTATUS hash_password_check(TALLOC_CTX *mem_ctx,
			     bool lanman_auth,
			     const struct samr_Password *client_lanman,
			     const struct samr_Password *client_nt,
			     const char *username, 
			     const struct samr_Password *stored_lanman, 
			     const struct samr_Password *stored_nt)
{
	if (stored_nt == NULL) {
		DEBUG(3,("ntlm_password_check: NO NT password stored for user %s.\n", 
			 username));
	}

	if (client_nt && stored_nt) {
		if (memcmp(client_nt->hash, stored_nt->hash, sizeof(stored_nt->hash)) == 0) {
			return NT_STATUS_OK;
		} else {
			DEBUG(3,("ntlm_password_check: Interactive logon: NT password check failed for user %s\n",
				 username));
			return NT_STATUS_WRONG_PASSWORD;
		}

	} else if (client_lanman && stored_lanman) {
		if (!lanman_auth) {
			DEBUG(3,("ntlm_password_check: Interactive logon: only LANMAN password supplied for user %s, and LM passwords are disabled!\n",
				 username));
			return NT_STATUS_WRONG_PASSWORD;
		}
		if (strchr_m(username, '@')) {
			return NT_STATUS_NOT_FOUND;
		}

		if (memcmp(client_lanman->hash, stored_lanman->hash, sizeof(stored_lanman->hash)) == 0) {
			return NT_STATUS_OK;
		} else {
			DEBUG(3,("ntlm_password_check: Interactive logon: LANMAN password check failed for user %s\n",
				 username));
			return NT_STATUS_WRONG_PASSWORD;
		}
	}
	if (strchr_m(username, '@')) {
		return NT_STATUS_NOT_FOUND;
	}
	return NT_STATUS_WRONG_PASSWORD;
}

/**
 * Check a challenge-response password against the value of the NT or
 * LM password hash.
 *
 * @param mem_ctx talloc context
 * @param challenge 8-byte challenge.  If all zero, forces plaintext comparison
 * @param nt_response 'unicode' NT response to the challenge, or unicode password
 * @param lm_response ASCII or LANMAN response to the challenge, or password in DOS code page
 * @param username internal Samba username, for log messages
 * @param client_username username the client used
 * @param client_domain domain name the client used (may be mapped)
 * @param stored_lanman LANMAN ASCII password from our passdb or similar
 * @param stored_nt MD4 unicode password from our passdb or similar
 * @param user_sess_key User session key
 * @param lm_sess_key LM session key (first 8 bytes of the LM hash)
 */

NTSTATUS ntlm_password_check(TALLOC_CTX *mem_ctx,
			     bool lanman_auth,
			     bool ntlm_auth,
			     uint32_t logon_parameters,
			     const DATA_BLOB *challenge,
			     const DATA_BLOB *lm_response,
			     const DATA_BLOB *nt_response,
			     const char *username, 
			     const char *client_username, 
			     const char *client_domain,
			     const struct samr_Password *stored_lanman, 
			     const struct samr_Password *stored_nt, 
			     DATA_BLOB *user_sess_key, 
			     DATA_BLOB *lm_sess_key)
{
	const static uint8_t zeros[8];
	DATA_BLOB tmp_sess_key;
	const char *upper_client_domain = NULL;

	if (client_domain != NULL) {
		upper_client_domain = talloc_strdup_upper(mem_ctx, client_domain);
		if (upper_client_domain == NULL) {
			return NT_STATUS_NO_MEMORY;
		}
	}

	if (stored_nt == NULL) {
		DEBUG(3,("ntlm_password_check: NO NT password stored for user %s.\n", 
			 username));
	}

	*lm_sess_key = data_blob(NULL, 0);
	*user_sess_key = data_blob(NULL, 0);

	/* Check for cleartext netlogon. Used by Exchange 5.5. */
	if ((logon_parameters & MSV1_0_CLEARTEXT_PASSWORD_ALLOWED)
	    && challenge->length == sizeof(zeros) 
	    && (memcmp(challenge->data, zeros, challenge->length) == 0 )) {
		struct samr_Password client_nt;
		struct samr_Password client_lm;
		char *unix_pw = NULL;
		bool lm_ok;

		DEBUG(4,("ntlm_password_check: checking plaintext passwords for user %s\n",
			 username));
		mdfour(client_nt.hash, nt_response->data, nt_response->length);

		if (lm_response->length && 
		    (convert_string_talloc(mem_ctx, CH_DOS, CH_UNIX, 
					  lm_response->data, lm_response->length, 
					   (void *)&unix_pw, NULL, false))) {
			if (E_deshash(unix_pw, client_lm.hash)) {
				lm_ok = true;
			} else {
				lm_ok = false;
			}
		} else {
			lm_ok = false;
		}
		return hash_password_check(mem_ctx, 
					   lanman_auth,
					   lm_ok ? &client_lm : NULL, 
					   nt_response->length ? &client_nt : NULL, 
					   username,  
					   stored_lanman, stored_nt);
	}

	if (nt_response->length != 0 && nt_response->length < 24) {
		DEBUG(2,("ntlm_password_check: invalid NT password length (%lu) for user %s\n", 
			 (unsigned long)nt_response->length, username));		
	}

	if (nt_response->length > 24 && stored_nt) {
		/* We have the NT MD4 hash challenge available - see if we can
		   use it 
		*/
		DEBUG(4,("ntlm_password_check: Checking NTLMv2 password with domain [%s]\n",
			client_domain ? client_domain : "<NULL>"));
		if (smb_pwd_check_ntlmv2(mem_ctx,
					 nt_response, 
					 stored_nt->hash, challenge, 
					 client_username, 
					 client_domain,
					 user_sess_key)) {
			if (user_sess_key->length) {
				*lm_sess_key = data_blob_talloc(mem_ctx, user_sess_key->data, MIN(8, user_sess_key->length));
			}
			return NT_STATUS_OK;
		}

		DEBUG(4,("ntlm_password_check: Checking NTLMv2 password with uppercased version of domain [%s]\n",
			upper_client_domain ? upper_client_domain : "<NULL>"));
		if (smb_pwd_check_ntlmv2(mem_ctx,
					 nt_response, 
					 stored_nt->hash, challenge, 
					 client_username, 
					 upper_client_domain,
					 user_sess_key)) {
			if (user_sess_key->length) {
				*lm_sess_key = data_blob_talloc(mem_ctx, user_sess_key->data, MIN(8, user_sess_key->length));
			}
			return NT_STATUS_OK;
		}

		DEBUG(4,("ntlm_password_check: Checking NTLMv2 password without a domain\n"));
		if (smb_pwd_check_ntlmv2(mem_ctx,
					 nt_response, 
					 stored_nt->hash, challenge, 
					 client_username, 
					 "",
					 user_sess_key)) {
			if (user_sess_key->length) {
				*lm_sess_key = data_blob_talloc(mem_ctx, user_sess_key->data, MIN(8, user_sess_key->length));
			}
			return NT_STATUS_OK;
		} else {
			DEBUG(3,("ntlm_password_check: NTLMv2 password check failed\n"));
		}
	} else if (nt_response->length == 24 && stored_nt) {
		if (ntlm_auth) {		
			/* We have the NT MD4 hash challenge available - see if we can
			   use it (ie. does it exist in the smbpasswd file).
			*/
			DEBUG(4,("ntlm_password_check: Checking NT MD4 password\n"));
			if (smb_pwd_check_ntlmv1(mem_ctx, 
						 nt_response, 
						 stored_nt->hash, challenge,
						 user_sess_key)) {
				/* The LM session key for this response is not very secure, 
				   so use it only if we otherwise allow LM authentication */

				if (lanman_auth && stored_lanman) {
					*lm_sess_key = data_blob_talloc(mem_ctx, stored_lanman->hash, MIN(8, user_sess_key->length));
				}
				return NT_STATUS_OK;
			} else {
				DEBUG(3,("ntlm_password_check: NT MD4 password check failed for user %s\n",
					 username));
				return NT_STATUS_WRONG_PASSWORD;
			}
		} else {
			DEBUG(2,("ntlm_password_check: NTLMv1 passwords NOT PERMITTED for user %s\n",
				 username));			
			/* no return, because we might pick up LMv2 in the LM field */
		}
	}

	if (lm_response->length == 0) {
		DEBUG(3,("ntlm_password_check: NEITHER LanMan nor NT password supplied for user %s\n",
			 username));
		return NT_STATUS_WRONG_PASSWORD;
	}

	if (lm_response->length < 24) {
		DEBUG(2,("ntlm_password_check: invalid LanMan password length (%lu) for user %s\n", 
			 (unsigned long)nt_response->length, username));		
		return NT_STATUS_WRONG_PASSWORD;
	}

	if (!lanman_auth) {
		DEBUG(3,("ntlm_password_check: Lanman passwords NOT PERMITTED for user %s\n",
			 username));
	} else if (!stored_lanman) {
		DEBUG(3,("ntlm_password_check: NO LanMan password set for user %s (and no NT password supplied)\n",
			 username));
	} else if (strchr_m(username, '@')) {
		DEBUG(3,("ntlm_password_check: NO LanMan password allowed for username@realm logins (user: %s)\n",
			 username));
	} else {
		DEBUG(4,("ntlm_password_check: Checking LM password\n"));
		if (smb_pwd_check_ntlmv1(mem_ctx,
					 lm_response, 
					 stored_lanman->hash, challenge,
					 NULL)) {
			/* The session key for this response is still very odd.  
			   It not very secure, so use it only if we otherwise 
			   allow LM authentication */

			if (lanman_auth && stored_lanman) {
				uint8_t first_8_lm_hash[16];
				memcpy(first_8_lm_hash, stored_lanman->hash, 8);
				memset(first_8_lm_hash + 8, '\0', 8);
				*user_sess_key = data_blob_talloc(mem_ctx, first_8_lm_hash, 16);
				*lm_sess_key = data_blob_talloc(mem_ctx, stored_lanman->hash, 8);
			}
			return NT_STATUS_OK;
		}
	}

	if (!stored_nt) {
		DEBUG(4,("ntlm_password_check: LM password check failed for user, no NT password %s\n",username));
		return NT_STATUS_WRONG_PASSWORD;
	}

	/* This is for 'LMv2' authentication.  almost NTLMv2 but limited to 24 bytes.
	   - related to Win9X, legacy NAS pass-though authentication
	*/
	DEBUG(4,("ntlm_password_check: Checking LMv2 password with domain %s\n",
		client_domain ? client_domain : "<NULL>"));
	if (smb_pwd_check_ntlmv2(mem_ctx,
				 lm_response, 
				 stored_nt->hash, challenge, 
				 client_username,
				 client_domain,
				 &tmp_sess_key)) {
		if (nt_response->length > 24) {
			/* If NTLMv2 authentication has preceeded us
			 * (even if it failed), then use the session
			 * key from that.  See the RPC-SAMLOGON
			 * torture test */
			smb_sess_key_ntlmv2(mem_ctx,
					    nt_response, 
					    stored_nt->hash, challenge, 
					    client_username,
					    client_domain,
					    user_sess_key);
		} else {
			/* Otherwise, use the LMv2 session key */
			*user_sess_key = tmp_sess_key;
		}
		if (user_sess_key->length) {
			*lm_sess_key = data_blob_talloc(mem_ctx, user_sess_key->data, MIN(8, user_sess_key->length));
		}
		return NT_STATUS_OK;
	}

	DEBUG(4,("ntlm_password_check: Checking LMv2 password with upper-cased version of domain %s\n",
		upper_client_domain ? upper_client_domain : "<NULL>"));
	if (smb_pwd_check_ntlmv2(mem_ctx,
				 lm_response, 
				 stored_nt->hash, challenge, 
				 client_username,
				 upper_client_domain,
				 &tmp_sess_key)) {
		if (nt_response->length > 24) {
			/* If NTLMv2 authentication has preceeded us
			 * (even if it failed), then use the session
			 * key from that.  See the RPC-SAMLOGON
			 * torture test */
			smb_sess_key_ntlmv2(mem_ctx,
					    nt_response, 
					    stored_nt->hash, challenge, 
					    client_username,
					    upper_client_domain,
					    user_sess_key);
		} else {
			/* Otherwise, use the LMv2 session key */
			*user_sess_key = tmp_sess_key;
		}
		if (user_sess_key->length) {
			*lm_sess_key = data_blob_talloc(mem_ctx, user_sess_key->data, MIN(8, user_sess_key->length));
		}
		return NT_STATUS_OK;
	}

	DEBUG(4,("ntlm_password_check: Checking LMv2 password without a domain\n"));
	if (smb_pwd_check_ntlmv2(mem_ctx,
				 lm_response, 
				 stored_nt->hash, challenge, 
				 client_username,
				 "",
				 &tmp_sess_key)) {
		if (nt_response->length > 24) {
			/* If NTLMv2 authentication has preceeded us
			 * (even if it failed), then use the session
			 * key from that.  See the RPC-SAMLOGON
			 * torture test */
			smb_sess_key_ntlmv2(mem_ctx,
					    nt_response, 
					    stored_nt->hash, challenge, 
					    client_username,
					    "",
					    user_sess_key);
		} else {
			/* Otherwise, use the LMv2 session key */
			*user_sess_key = tmp_sess_key;
		}
		if (user_sess_key->length) {
			*lm_sess_key = data_blob_talloc(mem_ctx, user_sess_key->data, MIN(8, user_sess_key->length));
		}
		return NT_STATUS_OK;
	}

	/* Apparently NT accepts NT responses in the LM field
	   - I think this is related to Win9X pass-though authentication
	*/
	DEBUG(4,("ntlm_password_check: Checking NT MD4 password in LM field\n"));
	if (ntlm_auth) {
		if (smb_pwd_check_ntlmv1(mem_ctx, 
					 lm_response, 
					 stored_nt->hash, challenge,
					 NULL)) {
			/* The session key for this response is still very odd.  
			   It not very secure, so use it only if we otherwise 
			   allow LM authentication */

			if (lanman_auth && stored_lanman) {
				uint8_t first_8_lm_hash[16];
				memcpy(first_8_lm_hash, stored_lanman->hash, 8);
				memset(first_8_lm_hash + 8, '\0', 8);
				*user_sess_key = data_blob_talloc(mem_ctx, first_8_lm_hash, 16);
				*lm_sess_key = data_blob_talloc(mem_ctx, stored_lanman->hash, 8);
			}
			return NT_STATUS_OK;
		}
		DEBUG(3,("ntlm_password_check: LM password, NT MD4 password in LM field and LMv2 failed for user %s\n",username));
	} else {
		DEBUG(3,("ntlm_password_check: LM password and LMv2 failed for user %s, and NT MD4 password in LM field not permitted\n",username));
	}

	/* Try and match error codes */
	if (strchr_m(username, '@')) {
		return NT_STATUS_NOT_FOUND;
	}
	return NT_STATUS_WRONG_PASSWORD;
}

