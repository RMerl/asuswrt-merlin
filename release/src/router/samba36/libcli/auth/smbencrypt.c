/*
   Unix SMB/CIFS implementation.
   SMB parameters and setup
   Copyright (C) Andrew Tridgell 1992-1998
   Modified by Jeremy Allison 1995.
   Copyright (C) Jeremy Allison 1995-2000.
   Copyright (C) Luke Kennethc Casson Leighton 1996-2000.
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2002-2003

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
#include "system/time.h"
#include "../libcli/auth/msrpc_parse.h"
#include "../lib/crypto/crypto.h"
#include "../libcli/auth/libcli_auth.h"
#include "../librpc/gen_ndr/ndr_ntlmssp.h"

void SMBencrypt_hash(const uint8_t lm_hash[16], const uint8_t *c8, uint8_t p24[24])
{
	uint8_t p21[21];

	memset(p21,'\0',21);
	memcpy(p21, lm_hash, 16);

	SMBOWFencrypt(p21, c8, p24);

#ifdef DEBUG_PASSWORD
	DEBUG(100,("SMBencrypt_hash: lm#, challenge, response\n"));
	dump_data(100, p21, 16);
	dump_data(100, c8, 8);
	dump_data(100, p24, 24);
#endif
}

/*
   This implements the X/Open SMB password encryption
   It takes a password ('unix' string), a 8 byte "crypt key"
   and puts 24 bytes of encrypted password into p24

   Returns False if password must have been truncated to create LM hash
*/

bool SMBencrypt(const char *passwd, const uint8_t *c8, uint8_t p24[24])
{
	bool ret;
	uint8_t lm_hash[16];

	ret = E_deshash(passwd, lm_hash);
	SMBencrypt_hash(lm_hash, c8, p24);
	return ret;
}

/**
 * Creates the MD4 Hash of the users password in NT UNICODE.
 * @param passwd password in 'unix' charset.
 * @param p16 return password hashed with md4, caller allocated 16 byte buffer
 */

bool E_md4hash(const char *passwd, uint8_t p16[16])
{
	size_t len;
	smb_ucs2_t *wpwd;
	bool ret;

	ret = push_ucs2_talloc(NULL, &wpwd, passwd, &len);
	if (!ret || len < 2) {
		/* We don't want to return fixed data, as most callers
		 * don't check */
		mdfour(p16, (const uint8_t *)passwd, strlen(passwd));
		return false;
	}

	len -= 2;
	mdfour(p16, (const uint8_t *)wpwd, len);

	talloc_free(wpwd);
	return true;
}

/**
 * Creates the MD5 Hash of a combination of 16 byte salt and 16 byte NT hash.
 * @param 16 byte salt.
 * @param 16 byte NT hash.
 * @param 16 byte return hashed with md5, caller allocated 16 byte buffer
 */

void E_md5hash(const uint8_t salt[16], const uint8_t nthash[16], uint8_t hash_out[16])
{
	MD5_CTX tctx;
	MD5Init(&tctx);
	MD5Update(&tctx, salt, 16);
	MD5Update(&tctx, nthash, 16);
	MD5Final(hash_out, &tctx);
}

/**
 * Creates the DES forward-only Hash of the users password in DOS ASCII charset
 * @param passwd password in 'unix' charset.
 * @param p16 return password hashed with DES, caller allocated 16 byte buffer
 * @return false if password was > 14 characters, and therefore may be incorrect, otherwise true
 * @note p16 is filled in regardless
 */

bool E_deshash(const char *passwd, uint8_t p16[16])
{
	bool ret = true;
	char dospwd[256];
	ZERO_STRUCT(dospwd);

	/* Password must be converted to DOS charset - null terminated, uppercase. */
	push_string(dospwd, passwd, sizeof(dospwd), STR_ASCII|STR_UPPER|STR_TERMINATE);

	/* Only the first 14 chars are considered, password need not be null terminated. */
	E_P16((const uint8_t *)dospwd, p16);

	if (strlen(dospwd) > 14) {
		ret = false;
	}

	ZERO_STRUCT(dospwd);

	return ret;
}

/**
 * Creates the MD4 and DES (LM) Hash of the users password.
 * MD4 is of the NT Unicode, DES is of the DOS UPPERCASE password.
 * @param passwd password in 'unix' charset.
 * @param nt_p16 return password hashed with md4, caller allocated 16 byte buffer
 * @param p16 return password hashed with des, caller allocated 16 byte buffer
 */

/* Does both the NT and LM owfs of a user's password */
void nt_lm_owf_gen(const char *pwd, uint8_t nt_p16[16], uint8_t p16[16])
{
	/* Calculate the MD4 hash (NT compatible) of the password */
	memset(nt_p16, '\0', 16);
	E_md4hash(pwd, nt_p16);

#ifdef DEBUG_PASSWORD
	DEBUG(100,("nt_lm_owf_gen: pwd, nt#\n"));
	dump_data(120, (const uint8_t *)pwd, strlen(pwd));
	dump_data(100, nt_p16, 16);
#endif

	E_deshash(pwd, (uint8_t *)p16);

#ifdef DEBUG_PASSWORD
	DEBUG(100,("nt_lm_owf_gen: pwd, lm#\n"));
	dump_data(120, (const uint8_t *)pwd, strlen(pwd));
	dump_data(100, p16, 16);
#endif
}

/* Does both the NTLMv2 owfs of a user's password */
bool ntv2_owf_gen(const uint8_t owf[16],
		  const char *user_in, const char *domain_in,
		  uint8_t kr_buf[16])
{
	smb_ucs2_t *user;
	smb_ucs2_t *domain;
	size_t user_byte_len;
	size_t domain_byte_len;
	bool ret;

	HMACMD5Context ctx;
	TALLOC_CTX *mem_ctx = talloc_init("ntv2_owf_gen for %s\\%s", domain_in, user_in);

	if (!mem_ctx) {
		return false;
	}

	if (!user_in) {
		user_in = "";
	}

	if (!domain_in) {
		domain_in = "";
	}

	user_in = strupper_talloc(mem_ctx, user_in);
	if (user_in == NULL) {
		talloc_free(mem_ctx);
		return false;
	}

	ret = push_ucs2_talloc(mem_ctx, &user, user_in, &user_byte_len );
	if (!ret) {
		DEBUG(0, ("push_uss2_talloc() for user failed)\n"));
		talloc_free(mem_ctx);
		return false;
	}

	ret = push_ucs2_talloc(mem_ctx, &domain, domain_in, &domain_byte_len);
	if (!ret) {
		DEBUG(0, ("push_ucs2_talloc() for domain failed\n"));
		talloc_free(mem_ctx);
		return false;
	}

	SMB_ASSERT(user_byte_len >= 2);
	SMB_ASSERT(domain_byte_len >= 2);

	/* We don't want null termination */
	user_byte_len = user_byte_len - 2;
	domain_byte_len = domain_byte_len - 2;

	hmac_md5_init_limK_to_64(owf, 16, &ctx);
	hmac_md5_update((uint8_t *)user, user_byte_len, &ctx);
	hmac_md5_update((uint8_t *)domain, domain_byte_len, &ctx);
	hmac_md5_final(kr_buf, &ctx);

#ifdef DEBUG_PASSWORD
	DEBUG(100, ("ntv2_owf_gen: user, domain, owfkey, kr\n"));
	dump_data(100, (uint8_t *)user, user_byte_len);
	dump_data(100, (uint8_t *)domain, domain_byte_len);
	dump_data(100, owf, 16);
	dump_data(100, kr_buf, 16);
#endif

	talloc_free(mem_ctx);
	return true;
}

/* Does the des encryption from the NT or LM MD4 hash. */
void SMBOWFencrypt(const uint8_t passwd[16], const uint8_t *c8, uint8_t p24[24])
{
	uint8_t p21[21];

	ZERO_STRUCT(p21);

	memcpy(p21, passwd, 16);
	E_P24(p21, c8, p24);
}

/* Does the des encryption. */

void SMBNTencrypt_hash(const uint8_t nt_hash[16], uint8_t *c8, uint8_t *p24)
{
	uint8_t p21[21];

	memset(p21,'\0',21);
	memcpy(p21, nt_hash, 16);
	SMBOWFencrypt(p21, c8, p24);

#ifdef DEBUG_PASSWORD
	DEBUG(100,("SMBNTencrypt: nt#, challenge, response\n"));
	dump_data(100, p21, 16);
	dump_data(100, c8, 8);
	dump_data(100, p24, 24);
#endif
}

/* Does the NT MD4 hash then des encryption. Plaintext version of the above. */

void SMBNTencrypt(const char *passwd, uint8_t *c8, uint8_t *p24)
{
	uint8_t nt_hash[16];
	E_md4hash(passwd, nt_hash);
	SMBNTencrypt_hash(nt_hash, c8, p24);
}


/* Does the md5 encryption from the Key Response for NTLMv2. */
void SMBOWFencrypt_ntv2(const uint8_t kr[16],
			const DATA_BLOB *srv_chal,
			const DATA_BLOB *smbcli_chal,
			uint8_t resp_buf[16])
{
	HMACMD5Context ctx;

	hmac_md5_init_limK_to_64(kr, 16, &ctx);
	hmac_md5_update(srv_chal->data, srv_chal->length, &ctx);
	hmac_md5_update(smbcli_chal->data, smbcli_chal->length, &ctx);
	hmac_md5_final(resp_buf, &ctx);

#ifdef DEBUG_PASSWORD
	DEBUG(100, ("SMBOWFencrypt_ntv2: srv_chal, smbcli_chal, resp_buf\n"));
	dump_data(100, srv_chal->data, srv_chal->length);
	dump_data(100, smbcli_chal->data, smbcli_chal->length);
	dump_data(100, resp_buf, 16);
#endif
}

void SMBsesskeygen_ntv2(const uint8_t kr[16],
			const uint8_t * nt_resp, uint8_t sess_key[16])
{
	/* a very nice, 128 bit, variable session key */

	HMACMD5Context ctx;

	hmac_md5_init_limK_to_64(kr, 16, &ctx);
	hmac_md5_update(nt_resp, 16, &ctx);
	hmac_md5_final((uint8_t *)sess_key, &ctx);

#ifdef DEBUG_PASSWORD
	DEBUG(100, ("SMBsesskeygen_ntv2:\n"));
	dump_data(100, sess_key, 16);
#endif
}

void SMBsesskeygen_ntv1(const uint8_t kr[16], uint8_t sess_key[16])
{
	/* yes, this session key does not change - yes, this
	   is a problem - but it is 128 bits */

	mdfour((uint8_t *)sess_key, kr, 16);

#ifdef DEBUG_PASSWORD
	DEBUG(100, ("SMBsesskeygen_ntv1:\n"));
	dump_data(100, sess_key, 16);
#endif
}

void SMBsesskeygen_lm_sess_key(const uint8_t lm_hash[16],
			       const uint8_t lm_resp[24], /* only uses 8 */
			       uint8_t sess_key[16])
{
	/* Calculate the LM session key (effective length 40 bits,
	   but changes with each session) */
	uint8_t p24[24];
	uint8_t partial_lm_hash[14];

	memcpy(partial_lm_hash, lm_hash, 8);
	memset(partial_lm_hash + 8, 0xbd, 6);

	des_crypt56(p24,   lm_resp, partial_lm_hash,     1);
	des_crypt56(p24+8, lm_resp, partial_lm_hash + 7, 1);

	memcpy(sess_key, p24, 16);

#ifdef DEBUG_PASSWORD
	DEBUG(100, ("SMBsesskeygen_lm_sess_key: \n"));
	dump_data(100, sess_key, 16);
#endif
}

DATA_BLOB NTLMv2_generate_names_blob(TALLOC_CTX *mem_ctx,
				     const char *hostname,
				     const char *domain)
{
	DATA_BLOB names_blob = data_blob_talloc(mem_ctx, NULL, 0);

	/* Deliberately ignore return here.. */
	if (hostname != NULL) {
		(void)msrpc_gen(mem_ctx, &names_blob,
			  "aaa",
			  MsvAvNbDomainName, domain,
			  MsvAvNbComputerName, hostname,
			  MsvAvEOL, "");
	} else {
		(void)msrpc_gen(mem_ctx, &names_blob,
			  "aa",
			  MsvAvNbDomainName, domain,
			  MsvAvEOL, "");
	}
	return names_blob;
}

static DATA_BLOB NTLMv2_generate_client_data(TALLOC_CTX *mem_ctx, const DATA_BLOB *names_blob)
{
	uint8_t client_chal[8];
	DATA_BLOB response = data_blob(NULL, 0);
	uint8_t long_date[8];
	NTTIME nttime;

	unix_to_nt_time(&nttime, time(NULL));

	generate_random_buffer(client_chal, sizeof(client_chal));

	push_nttime(long_date, 0, nttime);

	/* See http://www.ubiqx.org/cifs/SMB.html#SMB.8.5 */

	/* Deliberately ignore return here.. */
	(void)msrpc_gen(mem_ctx, &response, "ddbbdb",
		  0x00000101,     /* Header  */
		  0,              /* 'Reserved'  */
		  long_date, 8,	  /* Timestamp */
		  client_chal, 8, /* client challenge */
		  0,		  /* Unknown */
		  names_blob->data, names_blob->length);	/* End of name list */

	return response;
}

static DATA_BLOB NTLMv2_generate_response(TALLOC_CTX *out_mem_ctx,
					  const uint8_t ntlm_v2_hash[16],
					  const DATA_BLOB *server_chal,
					  const DATA_BLOB *names_blob)
{
	uint8_t ntlmv2_response[16];
	DATA_BLOB ntlmv2_client_data;
	DATA_BLOB final_response;

	TALLOC_CTX *mem_ctx = talloc_named(out_mem_ctx, 0,
					   "NTLMv2_generate_response internal context");

	if (!mem_ctx) {
		return data_blob(NULL, 0);
	}

	/* NTLMv2 */
	/* generate some data to pass into the response function - including
	   the hostname and domain name of the server */
	ntlmv2_client_data = NTLMv2_generate_client_data(mem_ctx, names_blob);

	/* Given that data, and the challenge from the server, generate a response */
	SMBOWFencrypt_ntv2(ntlm_v2_hash, server_chal, &ntlmv2_client_data, ntlmv2_response);

	final_response = data_blob_talloc(out_mem_ctx, NULL, sizeof(ntlmv2_response) + ntlmv2_client_data.length);

	memcpy(final_response.data, ntlmv2_response, sizeof(ntlmv2_response));

	memcpy(final_response.data+sizeof(ntlmv2_response),
	       ntlmv2_client_data.data, ntlmv2_client_data.length);

	talloc_free(mem_ctx);

	return final_response;
}

static DATA_BLOB LMv2_generate_response(TALLOC_CTX *mem_ctx,
					const uint8_t ntlm_v2_hash[16],
					const DATA_BLOB *server_chal)
{
	uint8_t lmv2_response[16];
	DATA_BLOB lmv2_client_data = data_blob_talloc(mem_ctx, NULL, 8);
	DATA_BLOB final_response = data_blob_talloc(mem_ctx, NULL,24);

	/* LMv2 */
	/* client-supplied random data */
	generate_random_buffer(lmv2_client_data.data, lmv2_client_data.length);

	/* Given that data, and the challenge from the server, generate a response */
	SMBOWFencrypt_ntv2(ntlm_v2_hash, server_chal, &lmv2_client_data, lmv2_response);
	memcpy(final_response.data, lmv2_response, sizeof(lmv2_response));

	/* after the first 16 bytes is the random data we generated above,
	   so the server can verify us with it */
	memcpy(final_response.data+sizeof(lmv2_response),
	       lmv2_client_data.data, lmv2_client_data.length);

	data_blob_free(&lmv2_client_data);

	return final_response;
}

bool SMBNTLMv2encrypt_hash(TALLOC_CTX *mem_ctx,
			   const char *user, const char *domain, const uint8_t nt_hash[16],
			   const DATA_BLOB *server_chal,
			   const DATA_BLOB *names_blob,
			   DATA_BLOB *lm_response, DATA_BLOB *nt_response,
			   DATA_BLOB *lm_session_key, DATA_BLOB *user_session_key)
{
	uint8_t ntlm_v2_hash[16];

	/* We don't use the NT# directly.  Instead we use it mashed up with
	   the username and domain.
	   This prevents username swapping during the auth exchange
	*/
	if (!ntv2_owf_gen(nt_hash, user, domain, ntlm_v2_hash)) {
		return false;
	}

	if (nt_response) {
		*nt_response = NTLMv2_generate_response(mem_ctx,
							ntlm_v2_hash, server_chal,
							names_blob);
		if (user_session_key) {
			*user_session_key = data_blob_talloc(mem_ctx, NULL, 16);

			/* The NTLMv2 calculations also provide a session key, for signing etc later */
			/* use only the first 16 bytes of nt_response for session key */
			SMBsesskeygen_ntv2(ntlm_v2_hash, nt_response->data, user_session_key->data);
		}
	}

	/* LMv2 */

	if (lm_response) {
		*lm_response = LMv2_generate_response(mem_ctx,
						      ntlm_v2_hash, server_chal);
		if (lm_session_key) {
			*lm_session_key = data_blob_talloc(mem_ctx, NULL, 16);

			/* The NTLMv2 calculations also provide a session key, for signing etc later */
			/* use only the first 16 bytes of lm_response for session key */
			SMBsesskeygen_ntv2(ntlm_v2_hash, lm_response->data, lm_session_key->data);
		}
	}

	return true;
}

bool SMBNTLMv2encrypt(TALLOC_CTX *mem_ctx,
		      const char *user, const char *domain,
		      const char *password,
		      const DATA_BLOB *server_chal,
		      const DATA_BLOB *names_blob,
		      DATA_BLOB *lm_response, DATA_BLOB *nt_response,
		      DATA_BLOB *lm_session_key, DATA_BLOB *user_session_key)
{
	uint8_t nt_hash[16];
	E_md4hash(password, nt_hash);

	return SMBNTLMv2encrypt_hash(mem_ctx,
				     user, domain, nt_hash, server_chal, names_blob,
				     lm_response, nt_response, lm_session_key, user_session_key);
}

NTSTATUS NTLMv2_RESPONSE_verify_netlogon_creds(const char *account_name,
			const char *account_domain,
			const DATA_BLOB response,
			const struct netlogon_creds_CredentialState *creds,
			const char *workgroup)
{
	TALLOC_CTX *frame = NULL;
	/* RespType + HiRespType */
	static const char *magic = "\x01\x01";
	int cmp;
	struct NTLMv2_RESPONSE v2_resp;
	enum ndr_err_code err;
	const struct AV_PAIR *av_nb_cn = NULL;
	const struct AV_PAIR *av_nb_dn = NULL;

	if (response.length < 48) {
		/*
		 * NTLMv2_RESPONSE has at least 48 bytes.
		 */
		return NT_STATUS_OK;
	}

	cmp = memcmp(response.data + 16, magic, 2);
	if (cmp != 0) {
		/*
		 * It doesn't look like a valid NTLMv2_RESPONSE
		 */
		return NT_STATUS_OK;
	}

	frame = talloc_stackframe();

	err = ndr_pull_struct_blob(&response, frame, &v2_resp,
		(ndr_pull_flags_fn_t)ndr_pull_NTLMv2_RESPONSE);
	if (!NDR_ERR_CODE_IS_SUCCESS(err)) {
		NTSTATUS status;
		status = ndr_map_error2ntstatus(err);
		DEBUG(2,("Failed to parse NTLMv2_RESPONSE "
			 "length %u - %s - %s\n",
			 (unsigned)response.length,
			 ndr_map_error2string(err),
			 nt_errstr(status)));
		dump_data(2, response.data, response.length);
		TALLOC_FREE(frame);
		return status;
	}

	if (DEBUGLVL(10)) {
		NDR_PRINT_DEBUG(NTLMv2_RESPONSE, &v2_resp);
	}

	/*
	 * Make sure the netbios computer name in the
	 * NTLMv2_RESPONSE matches the computer name
	 * in the secure channel credentials for workstation
	 * trusts.
	 *
	 * And the netbios domain name matches our
	 * workgroup.
	 *
	 * This prevents workstations from requesting
	 * the session key of NTLMSSP sessions of clients
	 * to other hosts.
	 */
	if (creds->secure_channel_type == SEC_CHAN_WKSTA) {
		av_nb_cn = ndr_ntlmssp_find_av(&v2_resp.Challenge.AvPairs,
					       MsvAvNbComputerName);
		av_nb_dn = ndr_ntlmssp_find_av(&v2_resp.Challenge.AvPairs,
					       MsvAvNbDomainName);
	}

	if (av_nb_cn != NULL) {
		const char *v = NULL;
		char *a = NULL;
		size_t len;

		v = av_nb_cn->Value.AvNbComputerName;

		a = talloc_strdup(frame, creds->account_name);
		if (a == NULL) {
			TALLOC_FREE(frame);
			return NT_STATUS_NO_MEMORY;
		}
		len = strlen(a);
		if (len > 0 && a[len - 1] == '$') {
			a[len - 1] = '\0';
		}

#ifdef SAMBA4_INTERNAL_HEIMDAL /* smbtorture4 for make test */
		cmp = strcasecmp_m(a, v);
#else /* smbd */
		cmp = StrCaseCmp(a, v);
#endif
		if (cmp != 0) {
			DEBUG(2,("%s: NTLMv2_RESPONSE with "
				 "NbComputerName[%s] rejected "
				 "for user[%s\\%s] "
				 "against SEC_CHAN_WKSTA[%s/%s] "
				 "in workgroup[%s]\n",
				 __func__, v,
				 account_domain,
				 account_name,
				 creds->computer_name,
				 creds->account_name,
				 workgroup));
			TALLOC_FREE(frame);
			return NT_STATUS_LOGON_FAILURE;
		}
	}
	if (av_nb_dn != NULL) {
		const char *v = NULL;

		v = av_nb_dn->Value.AvNbDomainName;

#ifdef SAMBA4_INTERNAL_HEIMDAL /* smbtorture4 for make test */
		cmp = strcasecmp_m(workgroup, v);
#else /* smbd */
		cmp = StrCaseCmp(workgroup, v);
#endif
		if (cmp != 0) {
			DEBUG(2,("%s: NTLMv2_RESPONSE with "
				 "NbDomainName[%s] rejected "
				 "for user[%s\\%s] "
				 "against SEC_CHAN_WKSTA[%s/%s] "
				 "in workgroup[%s]\n",
				 __func__, v,
				 account_domain,
				 account_name,
				 creds->computer_name,
				 creds->account_name,
				 workgroup));
			TALLOC_FREE(frame);
			return NT_STATUS_LOGON_FAILURE;
		}
	}

	TALLOC_FREE(frame);
	return NT_STATUS_OK;
}

/***********************************************************
 encode a password buffer with a unicode password.  The buffer
 is filled with random data to make it harder to attack.
************************************************************/
bool encode_pw_buffer(uint8_t buffer[516], const char *password, int string_flags)
{
	uint8_t new_pw[512];
	ssize_t new_pw_len;

	/* the incoming buffer can be any alignment. */
	string_flags |= STR_NOALIGN;

	new_pw_len = push_string(new_pw,
				 password,
				 sizeof(new_pw), string_flags);
	if (new_pw_len == -1) {
		return false;
	}

	memcpy(&buffer[512 - new_pw_len], new_pw, new_pw_len);

	generate_random_buffer(buffer, 512 - new_pw_len);

	/*
	 * The length of the new password is in the last 4 bytes of
	 * the data buffer.
	 */
	SIVAL(buffer, 512, new_pw_len);
	ZERO_STRUCT(new_pw);
	return true;
}


/***********************************************************
 decode a password buffer
 *new_pw_len is the length in bytes of the possibly mulitbyte
 returned password including termination.
************************************************************/

bool decode_pw_buffer(TALLOC_CTX *ctx,
		      uint8_t in_buffer[516],
		      char **pp_new_pwrd,
		      size_t *new_pw_len,
		      charset_t string_charset)
{
	int byte_len=0;

	*pp_new_pwrd = NULL;
	*new_pw_len = 0;

	/*
	  Warning !!! : This function is called from some rpc call.
	  The password IN the buffer may be a UNICODE string.
	  The password IN new_pwrd is an ASCII string
	  If you reuse that code somewhere else check first.
	*/

	/* The length of the new password is in the last 4 bytes of the data buffer. */

	byte_len = IVAL(in_buffer, 512);

#ifdef DEBUG_PASSWORD
	dump_data(100, in_buffer, 516);
#endif

	/* Password cannot be longer than the size of the password buffer */
	if ( (byte_len < 0) || (byte_len > 512)) {
		DEBUG(0, ("decode_pw_buffer: incorrect password length (%d).\n", byte_len));
		DEBUG(0, ("decode_pw_buffer: check that 'encrypt passwords = yes'\n"));
		return false;
	}

	/* decode into the return buffer. */
	if (!convert_string_talloc(ctx, string_charset, CH_UNIX,
				   &in_buffer[512 - byte_len],
				   byte_len,
				   (void *)pp_new_pwrd,
				   new_pw_len,
				   false)) {
		DEBUG(0, ("decode_pw_buffer: failed to convert incoming password\n"));
		return false;
	}

#ifdef DEBUG_PASSWORD
	DEBUG(100,("decode_pw_buffer: new_pwrd: "));
	dump_data(100, (uint8_t *)*pp_new_pwrd, *new_pw_len);
	DEBUG(100,("multibyte len:%lu\n", (unsigned long int)*new_pw_len));
	DEBUG(100,("original char len:%d\n", byte_len/2));
#endif

	return true;
}

/***********************************************************
 Decode an arc4 encrypted password change buffer.
************************************************************/

void encode_or_decode_arc4_passwd_buffer(unsigned char pw_buf[532], const DATA_BLOB *psession_key)
{
	MD5_CTX tctx;
	unsigned char key_out[16];

	/* Confounder is last 16 bytes. */

	MD5Init(&tctx);
	MD5Update(&tctx, &pw_buf[516], 16);
	MD5Update(&tctx, psession_key->data, psession_key->length);
	MD5Final(key_out, &tctx);
	/* arc4 with key_out. */
	arcfour_crypt(pw_buf, key_out, 516);
}

/***********************************************************
 encode a password buffer with an already unicode password.  The
 rest of the buffer is filled with random data to make it harder to attack.
************************************************************/
bool set_pw_in_buffer(uint8_t buffer[516], DATA_BLOB *password)
{
	if (password->length > 512) {
		return false;
	}

	memcpy(&buffer[512 - password->length], password->data, password->length);

	generate_random_buffer(buffer, 512 - password->length);

	/*
	 * The length of the new password is in the last 4 bytes of
	 * the data buffer.
	 */
	SIVAL(buffer, 512, password->length);
	return true;
}

/***********************************************************
 decode a password buffer
 *new_pw_size is the length in bytes of the extracted unicode password
************************************************************/
bool extract_pw_from_buffer(TALLOC_CTX *mem_ctx,
			    uint8_t in_buffer[516], DATA_BLOB *new_pass)
{
	int byte_len=0;

	/* The length of the new password is in the last 4 bytes of the data buffer. */

	byte_len = IVAL(in_buffer, 512);

#ifdef DEBUG_PASSWORD
	dump_data(100, in_buffer, 516);
#endif

	/* Password cannot be longer than the size of the password buffer */
	if ( (byte_len < 0) || (byte_len > 512)) {
		return false;
	}

	*new_pass = data_blob_talloc(mem_ctx, &in_buffer[512 - byte_len], byte_len);

	if (!new_pass->data) {
		return false;
	}

	return true;
}


/* encode a wkssvc_PasswordBuffer:
 *
 * similar to samr_CryptPasswordEx. Different: 8byte confounder (instead of
 * 16byte), confounder in front of the 516 byte buffer (instead of after that
 * buffer), calling MD5Update() first with session_key and then with confounder
 * (vice versa in samr) - Guenther */

void encode_wkssvc_join_password_buffer(TALLOC_CTX *mem_ctx,
					const char *pwd,
					DATA_BLOB *session_key,
					struct wkssvc_PasswordBuffer **pwd_buf)
{
	uint8_t buffer[516];
	MD5_CTX ctx;
	struct wkssvc_PasswordBuffer *my_pwd_buf = NULL;
	DATA_BLOB confounded_session_key;
	int confounder_len = 8;
	uint8_t confounder[8];

	my_pwd_buf = talloc_zero(mem_ctx, struct wkssvc_PasswordBuffer);
	if (!my_pwd_buf) {
		return;
	}

	confounded_session_key = data_blob_talloc(mem_ctx, NULL, 16);

	encode_pw_buffer(buffer, pwd, STR_UNICODE);

	generate_random_buffer((uint8_t *)confounder, confounder_len);

	MD5Init(&ctx);
	MD5Update(&ctx, session_key->data, session_key->length);
	MD5Update(&ctx, confounder, confounder_len);
	MD5Final(confounded_session_key.data, &ctx);

	arcfour_crypt_blob(buffer, 516, &confounded_session_key);

	memcpy(&my_pwd_buf->data[0], confounder, confounder_len);
	memcpy(&my_pwd_buf->data[8], buffer, 516);

	data_blob_free(&confounded_session_key);

	*pwd_buf = my_pwd_buf;
}

WERROR decode_wkssvc_join_password_buffer(TALLOC_CTX *mem_ctx,
					  struct wkssvc_PasswordBuffer *pwd_buf,
					  DATA_BLOB *session_key,
					  char **pwd)
{
	uint8_t buffer[516];
	MD5_CTX ctx;
	size_t pwd_len;

	DATA_BLOB confounded_session_key;

	int confounder_len = 8;
	uint8_t confounder[8];

	*pwd = NULL;

	if (!pwd_buf) {
		return WERR_BAD_PASSWORD;
	}

	if (session_key->length != 16) {
		DEBUG(10,("invalid session key\n"));
		return WERR_BAD_PASSWORD;
	}

	confounded_session_key = data_blob_talloc(mem_ctx, NULL, 16);

	memcpy(&confounder, &pwd_buf->data[0], confounder_len);
	memcpy(&buffer, &pwd_buf->data[8], 516);

	MD5Init(&ctx);
	MD5Update(&ctx, session_key->data, session_key->length);
	MD5Update(&ctx, confounder, confounder_len);
	MD5Final(confounded_session_key.data, &ctx);

	arcfour_crypt_blob(buffer, 516, &confounded_session_key);

	if (!decode_pw_buffer(mem_ctx, buffer, pwd, &pwd_len, CH_UTF16)) {
		data_blob_free(&confounded_session_key);
		return WERR_BAD_PASSWORD;
	}

	data_blob_free(&confounded_session_key);

	return WERR_OK;
}

