/* 
   Unix SMB/Netbios implementation.
   Version 3.0
   handle NLTMSSP, client server side parsing

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
#include "system/network.h"
#include "auth/ntlmssp/ntlmssp.h"
#include "../librpc/gen_ndr/ntlmssp.h"
#include "../libcli/auth/libcli_auth.h"
#include "../lib/crypto/crypto.h"
#include "auth/gensec/gensec.h"
#include "auth/auth.h"
#include "auth/ntlm/auth_proto.h"
#include "param/param.h"
#include "auth/session_proto.h"

/** 
 * Set a username on an NTLMSSP context - ensures it is talloc()ed 
 *
 */

static NTSTATUS ntlmssp_set_username(struct gensec_ntlmssp_state *gensec_ntlmssp_state, const char *user) 
{
	if (!user) {
		/* it should be at least "" */
		DEBUG(1, ("NTLMSSP failed to set username - cannot accept NULL username\n"));
		return NT_STATUS_INVALID_PARAMETER;
	}
	gensec_ntlmssp_state->user = talloc_strdup(gensec_ntlmssp_state, user);
	if (!gensec_ntlmssp_state->user) {
		return NT_STATUS_NO_MEMORY;
	}
	return NT_STATUS_OK;
}

/** 
 * Set a domain on an NTLMSSP context - ensures it is talloc()ed 
 *
 */
static NTSTATUS ntlmssp_set_domain(struct gensec_ntlmssp_state *gensec_ntlmssp_state, const char *domain) 
{
	gensec_ntlmssp_state->domain = talloc_strdup(gensec_ntlmssp_state, domain);
	if (!gensec_ntlmssp_state->domain) {
		return NT_STATUS_NO_MEMORY;
	}
	return NT_STATUS_OK;
}

/** 
 * Set a workstation on an NTLMSSP context - ensures it is talloc()ed 
 *
 */
static NTSTATUS ntlmssp_set_workstation(struct gensec_ntlmssp_state *gensec_ntlmssp_state, const char *workstation) 
{
	gensec_ntlmssp_state->workstation = talloc_strdup(gensec_ntlmssp_state, workstation);
	if (!gensec_ntlmssp_state->workstation) {
		return NT_STATUS_NO_MEMORY;
	}
	return NT_STATUS_OK;
}

/**
 * Determine correct target name flags for reply, given server role 
 * and negotiated flags
 * 
 * @param gensec_ntlmssp_state NTLMSSP State
 * @param neg_flags The flags from the packet
 * @param chal_flags The flags to be set in the reply packet
 * @return The 'target name' string.
 */

static const char *ntlmssp_target_name(struct gensec_ntlmssp_state *gensec_ntlmssp_state,
				       uint32_t neg_flags, uint32_t *chal_flags) 
{
	if (neg_flags & NTLMSSP_REQUEST_TARGET) {
		*chal_flags |= NTLMSSP_NEGOTIATE_TARGET_INFO;
		*chal_flags |= NTLMSSP_REQUEST_TARGET;
		if (gensec_ntlmssp_state->server_role == ROLE_STANDALONE) {
			*chal_flags |= NTLMSSP_TARGET_TYPE_SERVER;
			return gensec_ntlmssp_state->server_name;
		} else {
			*chal_flags |= NTLMSSP_TARGET_TYPE_DOMAIN;
			return gensec_ntlmssp_state->domain;
		};
	} else {
		return "";
	}
}



/**
 * Next state function for the Negotiate packet
 * 
 * @param gensec_security GENSEC state
 * @param out_mem_ctx Memory context for *out
 * @param in The request, as a DATA_BLOB.  reply.data must be NULL
 * @param out The reply, as an allocated DATA_BLOB, caller to free.
 * @return Errors or MORE_PROCESSING_REQUIRED if (normal) a reply is required. 
 */

NTSTATUS ntlmssp_server_negotiate(struct gensec_security *gensec_security, 
				  TALLOC_CTX *out_mem_ctx, 
				  const DATA_BLOB in, DATA_BLOB *out) 
{
	struct gensec_ntlmssp_state *gensec_ntlmssp_state = (struct gensec_ntlmssp_state *)gensec_security->private_data;
	DATA_BLOB struct_blob;
	uint32_t neg_flags = 0;
	uint32_t ntlmssp_command, chal_flags;
	const uint8_t *cryptkey;
	const char *target_name;

	/* parse the NTLMSSP packet */
#if 0
	file_save("ntlmssp_negotiate.dat", request.data, request.length);
#endif

	if (in.length) {
		if ((in.length < 16) || !msrpc_parse(out_mem_ctx, 
				 			 &in, "Cdd",
							 "NTLMSSP",
							 &ntlmssp_command,
							 &neg_flags)) {
			DEBUG(1, ("ntlmssp_server_negotiate: failed to parse "
				"NTLMSSP Negotiate of length %u:\n",
				(unsigned int)in.length ));
			dump_data(2, in.data, in.length);
			return NT_STATUS_INVALID_PARAMETER;
		}
		debug_ntlmssp_flags(neg_flags);
	}
	
	ntlmssp_handle_neg_flags(gensec_ntlmssp_state, neg_flags, gensec_ntlmssp_state->allow_lm_key);

	/* Ask our caller what challenge they would like in the packet */
	cryptkey = gensec_ntlmssp_state->get_challenge(gensec_ntlmssp_state);
	if (!cryptkey) {
		DEBUG(1, ("ntlmssp_server_negotiate: backend doesn't give a challenge\n"));
		return NT_STATUS_INTERNAL_ERROR;
	}

	/* Check if we may set the challenge */
	if (!gensec_ntlmssp_state->may_set_challenge(gensec_ntlmssp_state)) {
		gensec_ntlmssp_state->neg_flags &= ~NTLMSSP_NEGOTIATE_NTLM2;
	}

	/* The flags we send back are not just the negotiated flags,
	 * they are also 'what is in this packet'.  Therfore, we
	 * operate on 'chal_flags' from here on 
	 */

	chal_flags = gensec_ntlmssp_state->neg_flags;

	/* get the right name to fill in as 'target' */
	target_name = ntlmssp_target_name(gensec_ntlmssp_state, 
					  neg_flags, &chal_flags); 
	if (target_name == NULL) 
		return NT_STATUS_INVALID_PARAMETER;

	gensec_ntlmssp_state->chal = data_blob_talloc(gensec_ntlmssp_state, cryptkey, 8);
	gensec_ntlmssp_state->internal_chal = data_blob_talloc(gensec_ntlmssp_state, cryptkey, 8);

	/* This creates the 'blob' of names that appears at the end of the packet */
	if (chal_flags & NTLMSSP_NEGOTIATE_TARGET_INFO) {
		char dnsdomname[MAXHOSTNAMELEN], dnsname[MAXHOSTNAMELEN];
		const char *target_name_dns = "";

		/* Find out the DNS domain name */
		dnsdomname[0] = '\0';
		safe_strcpy(dnsdomname, lp_realm(gensec_security->settings->lp_ctx), sizeof(dnsdomname) - 1);
		strlower_m(dnsdomname);

		/* Find out the DNS host name */
		safe_strcpy(dnsname, gensec_ntlmssp_state->server_name, sizeof(dnsname) - 1);
		if (dnsdomname[0] != '\0') {
			safe_strcat(dnsname, ".", sizeof(dnsname) - 1);
			safe_strcat(dnsname, dnsdomname, sizeof(dnsname) - 1);
		}
		strlower_m(dnsname);

		if (chal_flags |= NTLMSSP_TARGET_TYPE_DOMAIN) {
			target_name_dns = dnsdomname;
		} else if (chal_flags |= NTLMSSP_TARGET_TYPE_SERVER) {
			target_name_dns = dnsname;
		}

		msrpc_gen(out_mem_ctx, 
			  &struct_blob, "aaaaa",
			  MsvAvNbDomainName, target_name,
			  MsvAvNbComputerName, gensec_ntlmssp_state->server_name,
			  MsvAvDnsDomainName, dnsdomname,
			  MsvAvDnsComputerName, dnsname,
			  MsvAvEOL, "");
	} else {
		struct_blob = data_blob(NULL, 0);
	}

	{
		/* Marshal the packet in the right format, be it unicode or ASCII */
		const char *gen_string;
		if (gensec_ntlmssp_state->unicode) {
			gen_string = "CdUdbddB";
		} else {
			gen_string = "CdAdbddB";
		}
		
		msrpc_gen(out_mem_ctx, 
			  out, gen_string,
			  "NTLMSSP", 
			  NTLMSSP_CHALLENGE,
			  target_name,
			  chal_flags,
			  cryptkey, 8,
			  0, 0,
			  struct_blob.data, struct_blob.length);
	}
		
	gensec_ntlmssp_state->expected_state = NTLMSSP_AUTH;

	return NT_STATUS_MORE_PROCESSING_REQUIRED;
}

/**
 * Next state function for the Authenticate packet
 * 
 * @param gensec_ntlmssp_state NTLMSSP State
 * @param request The request, as a DATA_BLOB
 * @return Errors or NT_STATUS_OK. 
 */

static NTSTATUS ntlmssp_server_preauth(struct gensec_ntlmssp_state *gensec_ntlmssp_state,
				       const DATA_BLOB request) 
{
	uint32_t ntlmssp_command, auth_flags;
	NTSTATUS nt_status;

	uint8_t session_nonce_hash[16];

	const char *parse_string;
	char *domain = NULL;
	char *user = NULL;
	char *workstation = NULL;

#if 0
	file_save("ntlmssp_auth.dat", request.data, request.length);
#endif

	if (gensec_ntlmssp_state->unicode) {
		parse_string = "CdBBUUUBd";
	} else {
		parse_string = "CdBBAAABd";
	}

	/* zero these out */
	data_blob_free(&gensec_ntlmssp_state->lm_resp);
	data_blob_free(&gensec_ntlmssp_state->nt_resp);
	data_blob_free(&gensec_ntlmssp_state->encrypted_session_key);

	gensec_ntlmssp_state->user = NULL;
	gensec_ntlmssp_state->domain = NULL;
	gensec_ntlmssp_state->workstation = NULL;

	/* now the NTLMSSP encoded auth hashes */
	if (!msrpc_parse(gensec_ntlmssp_state, 
			 &request, parse_string,
			 "NTLMSSP", 
			 &ntlmssp_command, 
			 &gensec_ntlmssp_state->lm_resp,
			 &gensec_ntlmssp_state->nt_resp,
			 &domain, 
			 &user, 
			 &workstation,
			 &gensec_ntlmssp_state->encrypted_session_key,
			 &auth_flags)) {
		DEBUG(10, ("ntlmssp_server_auth: failed to parse NTLMSSP (nonfatal):\n"));
		dump_data(10, request.data, request.length);

		/* zero this out */
		data_blob_free(&gensec_ntlmssp_state->encrypted_session_key);
		auth_flags = 0;
		
		/* Try again with a shorter string (Win9X truncates this packet) */
		if (gensec_ntlmssp_state->unicode) {
			parse_string = "CdBBUUU";
		} else {
			parse_string = "CdBBAAA";
		}

		/* now the NTLMSSP encoded auth hashes */
		if (!msrpc_parse(gensec_ntlmssp_state, 
				 &request, parse_string,
				 "NTLMSSP", 
				 &ntlmssp_command, 
				 &gensec_ntlmssp_state->lm_resp,
				 &gensec_ntlmssp_state->nt_resp,
				 &domain, 
				 &user, 
				 &workstation)) {
			DEBUG(1, ("ntlmssp_server_auth: failed to parse NTLMSSP:\n"));
			dump_data(2, request.data, request.length);

			return NT_STATUS_INVALID_PARAMETER;
		}
	}

	if (auth_flags)
		ntlmssp_handle_neg_flags(gensec_ntlmssp_state, auth_flags, gensec_ntlmssp_state->allow_lm_key);

	if (!NT_STATUS_IS_OK(nt_status = ntlmssp_set_domain(gensec_ntlmssp_state, domain))) {
		/* zero this out */
		data_blob_free(&gensec_ntlmssp_state->encrypted_session_key);
		return nt_status;
	}

	if (!NT_STATUS_IS_OK(nt_status = ntlmssp_set_username(gensec_ntlmssp_state, user))) {
		/* zero this out */
		data_blob_free(&gensec_ntlmssp_state->encrypted_session_key);
		return nt_status;
	}

	if (!NT_STATUS_IS_OK(nt_status = ntlmssp_set_workstation(gensec_ntlmssp_state, workstation))) {
		/* zero this out */
		data_blob_free(&gensec_ntlmssp_state->encrypted_session_key);
		return nt_status;
	}

	DEBUG(3,("Got user=[%s] domain=[%s] workstation=[%s] len1=%lu len2=%lu\n",
		 gensec_ntlmssp_state->user, gensec_ntlmssp_state->domain, gensec_ntlmssp_state->workstation, (unsigned long)gensec_ntlmssp_state->lm_resp.length, (unsigned long)gensec_ntlmssp_state->nt_resp.length));

#if 0
	file_save("nthash1.dat",  &gensec_ntlmssp_state->nt_resp.data,  &gensec_ntlmssp_state->nt_resp.length);
	file_save("lmhash1.dat",  &gensec_ntlmssp_state->lm_resp.data,  &gensec_ntlmssp_state->lm_resp.length);
#endif

	/* NTLM2 uses a 'challenge' that is made of up both the server challenge, and a 
	   client challenge 
	
	   However, the NTLM2 flag may still be set for the real NTLMv2 logins, be careful.
	*/
	if (gensec_ntlmssp_state->neg_flags & NTLMSSP_NEGOTIATE_NTLM2) {
		if (gensec_ntlmssp_state->nt_resp.length == 24 && gensec_ntlmssp_state->lm_resp.length == 24) {
			struct MD5Context md5_session_nonce_ctx;
			SMB_ASSERT(gensec_ntlmssp_state->internal_chal.data 
				   && gensec_ntlmssp_state->internal_chal.length == 8);
			
			gensec_ntlmssp_state->doing_ntlm2 = true;

			memcpy(gensec_ntlmssp_state->crypt.ntlm2.session_nonce, gensec_ntlmssp_state->internal_chal.data, 8);
			memcpy(&gensec_ntlmssp_state->crypt.ntlm2.session_nonce[8], gensec_ntlmssp_state->lm_resp.data, 8);
			
			MD5Init(&md5_session_nonce_ctx);
			MD5Update(&md5_session_nonce_ctx, gensec_ntlmssp_state->crypt.ntlm2.session_nonce, 16);
			MD5Final(session_nonce_hash, &md5_session_nonce_ctx);
			
			gensec_ntlmssp_state->chal = data_blob_talloc(gensec_ntlmssp_state, 
							       session_nonce_hash, 8);

			/* LM response is no longer useful, zero it out */
			data_blob_free(&gensec_ntlmssp_state->lm_resp);

			/* We changed the effective challenge - set it */
			if (!NT_STATUS_IS_OK(nt_status = 
					     gensec_ntlmssp_state->set_challenge(gensec_ntlmssp_state, 
										 &gensec_ntlmssp_state->chal))) {
				/* zero this out */
				data_blob_free(&gensec_ntlmssp_state->encrypted_session_key);
				return nt_status;
			}

			/* LM Key is incompatible... */
			gensec_ntlmssp_state->neg_flags &= ~NTLMSSP_NEGOTIATE_LM_KEY;
		}
	}
	return NT_STATUS_OK;
}

/**
 * Next state function for the Authenticate packet 
 * (after authentication - figures out the session keys etc)
 * 
 * @param gensec_ntlmssp_state NTLMSSP State
 * @return Errors or NT_STATUS_OK. 
 */

static NTSTATUS ntlmssp_server_postauth(struct gensec_security *gensec_security, 
					DATA_BLOB *user_session_key, 
					DATA_BLOB *lm_session_key) 
{
	struct gensec_ntlmssp_state *gensec_ntlmssp_state = (struct gensec_ntlmssp_state *)gensec_security->private_data;
	NTSTATUS nt_status;
	DATA_BLOB session_key = data_blob(NULL, 0);

	if (user_session_key)
		dump_data_pw("USER session key:\n", user_session_key->data, user_session_key->length);

	if (lm_session_key) 
		dump_data_pw("LM first-8:\n", lm_session_key->data, lm_session_key->length);

	/* Handle the different session key derivation for NTLM2 */
	if (gensec_ntlmssp_state->doing_ntlm2) {
		if (user_session_key && user_session_key->data && user_session_key->length == 16) {
			session_key = data_blob_talloc(gensec_ntlmssp_state, NULL, 16);
			hmac_md5(user_session_key->data, gensec_ntlmssp_state->crypt.ntlm2.session_nonce, 
				 sizeof(gensec_ntlmssp_state->crypt.ntlm2.session_nonce), session_key.data);
			DEBUG(10,("ntlmssp_server_auth: Created NTLM2 session key.\n"));
			dump_data_pw("NTLM2 session key:\n", session_key.data, session_key.length);
			
		} else {
			DEBUG(10,("ntlmssp_server_auth: Failed to create NTLM2 session key.\n"));
			session_key = data_blob(NULL, 0);
		}
	} else if ((gensec_ntlmssp_state->neg_flags & NTLMSSP_NEGOTIATE_LM_KEY) 
		/* Ensure we can never get here on NTLMv2 */
		&& (gensec_ntlmssp_state->nt_resp.length == 0 || gensec_ntlmssp_state->nt_resp.length == 24)) {

		if (lm_session_key && lm_session_key->data && lm_session_key->length >= 8) {
			if (gensec_ntlmssp_state->lm_resp.data && gensec_ntlmssp_state->lm_resp.length == 24) {
				session_key = data_blob_talloc(gensec_ntlmssp_state, NULL, 16);
				SMBsesskeygen_lm_sess_key(lm_session_key->data, gensec_ntlmssp_state->lm_resp.data, 
							  session_key.data);
				DEBUG(10,("ntlmssp_server_auth: Created NTLM session key.\n"));
				dump_data_pw("LM session key:\n", session_key.data, session_key.length);
  			} else {
				
				/* When there is no LM response, just use zeros */
 				static const uint8_t zeros[24];
 				session_key = data_blob_talloc(gensec_ntlmssp_state, NULL, 16);
 				SMBsesskeygen_lm_sess_key(zeros, zeros, 
 							  session_key.data);
 				DEBUG(10,("ntlmssp_server_auth: Created NTLM session key.\n"));
 				dump_data_pw("LM session key:\n", session_key.data, session_key.length);
			}
		} else {
 			/* LM Key not selected */
 			gensec_ntlmssp_state->neg_flags &= ~NTLMSSP_NEGOTIATE_LM_KEY;

			DEBUG(10,("ntlmssp_server_auth: Failed to create NTLM session key.\n"));
			session_key = data_blob(NULL, 0);
		}

	} else if (user_session_key && user_session_key->data) {
		session_key = data_blob_talloc(gensec_ntlmssp_state, user_session_key->data, user_session_key->length);
		DEBUG(10,("ntlmssp_server_auth: Using unmodified nt session key.\n"));
		dump_data_pw("unmodified session key:\n", session_key.data, session_key.length);

		/* LM Key not selected */
		gensec_ntlmssp_state->neg_flags &= ~NTLMSSP_NEGOTIATE_LM_KEY;

	} else if (lm_session_key && lm_session_key->data) {
		/* Very weird to have LM key, but no user session key, but anyway.. */
		session_key = data_blob_talloc(gensec_ntlmssp_state, lm_session_key->data, lm_session_key->length);
		DEBUG(10,("ntlmssp_server_auth: Using unmodified lm session key.\n"));
		dump_data_pw("unmodified session key:\n", session_key.data, session_key.length);

		/* LM Key not selected */
		gensec_ntlmssp_state->neg_flags &= ~NTLMSSP_NEGOTIATE_LM_KEY;

	} else {
		DEBUG(10,("ntlmssp_server_auth: Failed to create unmodified session key.\n"));
		session_key = data_blob(NULL, 0);

		/* LM Key not selected */
		gensec_ntlmssp_state->neg_flags &= ~NTLMSSP_NEGOTIATE_LM_KEY;
	}

	/* With KEY_EXCH, the client supplies the proposed session key, 
	   but encrypts it with the long-term key */
	if (gensec_ntlmssp_state->neg_flags & NTLMSSP_NEGOTIATE_KEY_EXCH) {
		if (!gensec_ntlmssp_state->encrypted_session_key.data 
		    || gensec_ntlmssp_state->encrypted_session_key.length != 16) {
			data_blob_free(&gensec_ntlmssp_state->encrypted_session_key);
			DEBUG(1, ("Client-supplied KEY_EXCH session key was of invalid length (%u)!\n", 
				  (unsigned)gensec_ntlmssp_state->encrypted_session_key.length));
			return NT_STATUS_INVALID_PARAMETER;
		} else if (!session_key.data || session_key.length != 16) {
			DEBUG(5, ("server session key is invalid (len == %u), cannot do KEY_EXCH!\n", 
				  (unsigned)session_key.length));
			gensec_ntlmssp_state->session_key = session_key;
		} else {
			dump_data_pw("KEY_EXCH session key (enc):\n", 
				     gensec_ntlmssp_state->encrypted_session_key.data, 
				     gensec_ntlmssp_state->encrypted_session_key.length);
			arcfour_crypt(gensec_ntlmssp_state->encrypted_session_key.data, 
				      session_key.data, 
				      gensec_ntlmssp_state->encrypted_session_key.length);
			gensec_ntlmssp_state->session_key = data_blob_talloc(gensec_ntlmssp_state, 
								      gensec_ntlmssp_state->encrypted_session_key.data, 
								      gensec_ntlmssp_state->encrypted_session_key.length);
			dump_data_pw("KEY_EXCH session key:\n", gensec_ntlmssp_state->encrypted_session_key.data, 
				     gensec_ntlmssp_state->encrypted_session_key.length);
			talloc_free(session_key.data);
		}
	} else {
		gensec_ntlmssp_state->session_key = session_key;
	}

	if ((gensec_security->want_features & GENSEC_FEATURE_SIGN)
	    || (gensec_security->want_features & GENSEC_FEATURE_SEAL)) {
		nt_status = ntlmssp_sign_init(gensec_ntlmssp_state);
	} else {
		nt_status = NT_STATUS_OK;
	}

	data_blob_free(&gensec_ntlmssp_state->encrypted_session_key);
	
	/* allow arbitarily many authentications, but watch that this will cause a 
	   memory leak, until the gensec_ntlmssp_state is shutdown 
	*/

	if (gensec_ntlmssp_state->server_multiple_authentications) {
		gensec_ntlmssp_state->expected_state = NTLMSSP_AUTH;
	} else {
		gensec_ntlmssp_state->expected_state = NTLMSSP_DONE;
	}

	return nt_status;
}


/**
 * Next state function for the Authenticate packet
 * 
 * @param gensec_security GENSEC state
 * @param out_mem_ctx Memory context for *out
 * @param in The request, as a DATA_BLOB.  reply.data must be NULL
 * @param out The reply, as an allocated DATA_BLOB, caller to free.
 * @return Errors or NT_STATUS_OK if authentication sucessful
 */

NTSTATUS ntlmssp_server_auth(struct gensec_security *gensec_security, 
			     TALLOC_CTX *out_mem_ctx, 
			     const DATA_BLOB in, DATA_BLOB *out) 
{	
	struct gensec_ntlmssp_state *gensec_ntlmssp_state = (struct gensec_ntlmssp_state *)gensec_security->private_data;
	DATA_BLOB user_session_key = data_blob(NULL, 0);
	DATA_BLOB lm_session_key = data_blob(NULL, 0);
	NTSTATUS nt_status;

	TALLOC_CTX *mem_ctx = talloc_new(out_mem_ctx);
	if (!mem_ctx) {
		return NT_STATUS_NO_MEMORY;
	}

	/* zero the outbound NTLMSSP packet */
	*out = data_blob_talloc(out_mem_ctx, NULL, 0);

	if (!NT_STATUS_IS_OK(nt_status = ntlmssp_server_preauth(gensec_ntlmssp_state, in))) {
		talloc_free(mem_ctx);
		return nt_status;
	}

	/*
	 * Note we don't check here for NTLMv2 auth settings. If NTLMv2 auth
	 * is required (by "ntlm auth = no" and "lm auth = no" being set in the
	 * smb.conf file) and no NTLMv2 response was sent then the password check
	 * will fail here. JRA.
	 */

	/* Finally, actually ask if the password is OK */

	if (!NT_STATUS_IS_OK(nt_status = gensec_ntlmssp_state->check_password(gensec_ntlmssp_state, mem_ctx,
									      &user_session_key, &lm_session_key))) {
		talloc_free(mem_ctx);
		return nt_status;
	}
	
	if (gensec_security->want_features
	    & (GENSEC_FEATURE_SIGN|GENSEC_FEATURE_SEAL|GENSEC_FEATURE_SESSION_KEY)) {
		nt_status = ntlmssp_server_postauth(gensec_security, &user_session_key, &lm_session_key);
		talloc_free(mem_ctx);
		return nt_status;
	} else {
		gensec_ntlmssp_state->session_key = data_blob(NULL, 0);
		talloc_free(mem_ctx);
		return NT_STATUS_OK;
	}
}

/**
 * Return the challenge as determined by the authentication subsystem 
 * @return an 8 byte random challenge
 */

static const uint8_t *auth_ntlmssp_get_challenge(const struct gensec_ntlmssp_state *gensec_ntlmssp_state)
{
	NTSTATUS status;
	const uint8_t *chal;

	status = gensec_ntlmssp_state->auth_context->get_challenge(gensec_ntlmssp_state->auth_context, &chal);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("auth_ntlmssp_get_challenge: failed to get challenge: %s\n",
			nt_errstr(status)));
		return NULL;
	}

	return chal;
}

/**
 * Some authentication methods 'fix' the challenge, so we may not be able to set it
 *
 * @return If the effective challenge used by the auth subsystem may be modified
 */
static bool auth_ntlmssp_may_set_challenge(const struct gensec_ntlmssp_state *gensec_ntlmssp_state)
{
	return gensec_ntlmssp_state->auth_context->challenge_may_be_modified(gensec_ntlmssp_state->auth_context);
}

/**
 * NTLM2 authentication modifies the effective challenge, 
 * @param challenge The new challenge value
 */
static NTSTATUS auth_ntlmssp_set_challenge(struct gensec_ntlmssp_state *gensec_ntlmssp_state, DATA_BLOB *challenge)
{
	NTSTATUS nt_status;
	struct auth_context *auth_context = gensec_ntlmssp_state->auth_context;
	const uint8_t *chal;

	if (challenge->length != 8) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	chal = challenge->data;

	nt_status = gensec_ntlmssp_state->auth_context->set_challenge(auth_context, 
								      chal, 
								      "NTLMSSP callback (NTLM2)");

	return nt_status;
}

/**
 * Check the password on an NTLMSSP login.  
 *
 * Return the session keys used on the connection.
 */

static NTSTATUS auth_ntlmssp_check_password(struct gensec_ntlmssp_state *gensec_ntlmssp_state, 
					    TALLOC_CTX *mem_ctx, 
					    DATA_BLOB *user_session_key, DATA_BLOB *lm_session_key) 
{
	NTSTATUS nt_status;
	struct auth_usersupplied_info *user_info = talloc(mem_ctx, struct auth_usersupplied_info);
	if (!user_info) {
		return NT_STATUS_NO_MEMORY;
	}

	user_info->logon_parameters = MSV1_0_ALLOW_SERVER_TRUST_ACCOUNT | MSV1_0_ALLOW_WORKSTATION_TRUST_ACCOUNT;
	user_info->flags = 0;
	user_info->mapped_state = false;
	user_info->client.account_name = gensec_ntlmssp_state->user;
	user_info->client.domain_name = gensec_ntlmssp_state->domain;
	user_info->workstation_name = gensec_ntlmssp_state->workstation;
	user_info->remote_host = gensec_get_peer_addr(gensec_ntlmssp_state->gensec_security);

	user_info->password_state = AUTH_PASSWORD_RESPONSE;
	user_info->password.response.lanman = gensec_ntlmssp_state->lm_resp;
	user_info->password.response.lanman.data = talloc_steal(user_info, gensec_ntlmssp_state->lm_resp.data);
	user_info->password.response.nt = gensec_ntlmssp_state->nt_resp;
	user_info->password.response.nt.data = talloc_steal(user_info, gensec_ntlmssp_state->nt_resp.data);

	nt_status = gensec_ntlmssp_state->auth_context->check_password(gensec_ntlmssp_state->auth_context, 
								       mem_ctx,
								       user_info, 
								       &gensec_ntlmssp_state->server_info);
	talloc_free(user_info);
	NT_STATUS_NOT_OK_RETURN(nt_status);

	talloc_steal(gensec_ntlmssp_state, gensec_ntlmssp_state->server_info);

	if (gensec_ntlmssp_state->server_info->user_session_key.length) {
		DEBUG(10, ("Got NT session key of length %u\n", 
			   (unsigned)gensec_ntlmssp_state->server_info->user_session_key.length));
		if (!talloc_reference(mem_ctx, gensec_ntlmssp_state->server_info->user_session_key.data)) {
			return NT_STATUS_NO_MEMORY;
		}

		*user_session_key = gensec_ntlmssp_state->server_info->user_session_key;
	}
	if (gensec_ntlmssp_state->server_info->lm_session_key.length) {
		DEBUG(10, ("Got LM session key of length %u\n", 
			   (unsigned)gensec_ntlmssp_state->server_info->lm_session_key.length));
		if (!talloc_reference(mem_ctx, gensec_ntlmssp_state->server_info->lm_session_key.data)) {
			return NT_STATUS_NO_MEMORY;
		}

		*lm_session_key = gensec_ntlmssp_state->server_info->lm_session_key;
	}
	return nt_status;
}

/** 
 * Return the credentials of a logged on user, including session keys
 * etc.
 *
 * Only valid after a successful authentication
 *
 * May only be called once per authentication.
 *
 */

NTSTATUS gensec_ntlmssp_session_info(struct gensec_security *gensec_security,
				     struct auth_session_info **session_info) 
{
	NTSTATUS nt_status;
	struct gensec_ntlmssp_state *gensec_ntlmssp_state = (struct gensec_ntlmssp_state *)gensec_security->private_data;

	nt_status = auth_generate_session_info(gensec_ntlmssp_state, gensec_security->event_ctx, gensec_security->settings->lp_ctx, gensec_ntlmssp_state->server_info, session_info);
	NT_STATUS_NOT_OK_RETURN(nt_status);

	(*session_info)->session_key = data_blob_talloc(*session_info, 
							gensec_ntlmssp_state->session_key.data,
							gensec_ntlmssp_state->session_key.length);

	return NT_STATUS_OK;
}

/**
 * Start NTLMSSP on the server side 
 *
 */
NTSTATUS gensec_ntlmssp_server_start(struct gensec_security *gensec_security)
{
	NTSTATUS nt_status;
	struct gensec_ntlmssp_state *gensec_ntlmssp_state;

	nt_status = gensec_ntlmssp_start(gensec_security);
	NT_STATUS_NOT_OK_RETURN(nt_status);

	gensec_ntlmssp_state = (struct gensec_ntlmssp_state *)gensec_security->private_data;

	gensec_ntlmssp_state->role = NTLMSSP_SERVER;

	gensec_ntlmssp_state->workstation = NULL;
	gensec_ntlmssp_state->server_name = lp_netbios_name(gensec_security->settings->lp_ctx);

	gensec_ntlmssp_state->domain = lp_workgroup(gensec_security->settings->lp_ctx);

	gensec_ntlmssp_state->expected_state = NTLMSSP_NEGOTIATE;

	gensec_ntlmssp_state->allow_lm_key = (lp_lanman_auth(gensec_security->settings->lp_ctx) 
					  && gensec_setting_bool(gensec_security->settings, "ntlmssp_server", "allow_lm_key", false));

	gensec_ntlmssp_state->server_multiple_authentications = false;
	
	gensec_ntlmssp_state->neg_flags = 
		NTLMSSP_NEGOTIATE_NTLM | NTLMSSP_NEGOTIATE_VERSION;

	gensec_ntlmssp_state->lm_resp = data_blob(NULL, 0);
	gensec_ntlmssp_state->nt_resp = data_blob(NULL, 0);
	gensec_ntlmssp_state->encrypted_session_key = data_blob(NULL, 0);

	if (gensec_setting_bool(gensec_security->settings, "ntlmssp_server", "128bit", true)) {
		gensec_ntlmssp_state->neg_flags |= NTLMSSP_NEGOTIATE_128;		
	}

	if (gensec_setting_bool(gensec_security->settings, "ntlmssp_server", "56bit", true)) {
		gensec_ntlmssp_state->neg_flags |= NTLMSSP_NEGOTIATE_56;		
	}

	if (gensec_setting_bool(gensec_security->settings, "ntlmssp_server", "keyexchange", true)) {
		gensec_ntlmssp_state->neg_flags |= NTLMSSP_NEGOTIATE_KEY_EXCH;		
	}

	if (gensec_setting_bool(gensec_security->settings, "ntlmssp_server", "alwayssign", true)) {
		gensec_ntlmssp_state->neg_flags |= NTLMSSP_NEGOTIATE_ALWAYS_SIGN;		
	}

	if (gensec_setting_bool(gensec_security->settings, "ntlmssp_server", "ntlm2", true)) {
		gensec_ntlmssp_state->neg_flags |= NTLMSSP_NEGOTIATE_NTLM2;		
	}

	if (gensec_security->want_features & GENSEC_FEATURE_SIGN) {
		gensec_ntlmssp_state->neg_flags |= NTLMSSP_NEGOTIATE_SIGN;
	}
	if (gensec_security->want_features & GENSEC_FEATURE_SEAL) {
		gensec_ntlmssp_state->neg_flags |= NTLMSSP_NEGOTIATE_SEAL;
	}

	gensec_ntlmssp_state->auth_context = gensec_security->auth_context;

	gensec_ntlmssp_state->get_challenge = auth_ntlmssp_get_challenge;
	gensec_ntlmssp_state->may_set_challenge = auth_ntlmssp_may_set_challenge;
	gensec_ntlmssp_state->set_challenge = auth_ntlmssp_set_challenge;
	gensec_ntlmssp_state->check_password = auth_ntlmssp_check_password;
	gensec_ntlmssp_state->server_role = lp_server_role(gensec_security->settings->lp_ctx);

	return NT_STATUS_OK;
}

