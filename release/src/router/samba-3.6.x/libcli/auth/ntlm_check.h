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
			     const struct samr_Password *stored_nt);

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
			     DATA_BLOB *lm_sess_key);
