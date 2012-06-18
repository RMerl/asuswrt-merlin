/* 
 *  Unix SMB/CIFS implementation.
 *  Version 3.0
 *  NTLMSSP Signing routines
 *  Copyright (C) Luke Kenneth Casson Leighton 1996-2001
 *  Copyright (C) Andrew Bartlett <abartlet@samba.org> 2003-2005
 *  
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "includes.h"
#include "auth/ntlmssp/ntlmssp.h"
#include "../librpc/gen_ndr/ntlmssp.h"
#include "../libcli/auth/libcli_auth.h"
#include "../lib/crypto/crypto.h"
#include "auth/gensec/gensec.h"

#define CLI_SIGN "session key to client-to-server signing key magic constant"
#define CLI_SEAL "session key to client-to-server sealing key magic constant"
#define SRV_SIGN "session key to server-to-client signing key magic constant"
#define SRV_SEAL "session key to server-to-client sealing key magic constant"

/**
 * Some notes on the NTLM2 code:
 *
 * NTLM2 is a AEAD system.  This means that the data encrypted is not
 * all the data that is signed.  In DCE-RPC case, the headers of the
 * DCE-RPC packets are also signed.  This prevents some of the
 * fun-and-games one might have by changing them.
 *
 */

static void calc_ntlmv2_key(TALLOC_CTX *mem_ctx, 
			    DATA_BLOB *subkey,
			    DATA_BLOB session_key, 
			    const char *constant)
{
	struct MD5Context ctx3;
	*subkey = data_blob_talloc(mem_ctx, NULL, 16);
	MD5Init(&ctx3);
	MD5Update(&ctx3, session_key.data, session_key.length);
	MD5Update(&ctx3, (const uint8_t *)constant, strlen(constant)+1);
	MD5Final(subkey->data, &ctx3);
}

enum ntlmssp_direction {
	NTLMSSP_SEND,
	NTLMSSP_RECEIVE
};

static NTSTATUS ntlmssp_make_packet_signature(struct gensec_ntlmssp_state *gensec_ntlmssp_state,
					      TALLOC_CTX *sig_mem_ctx, 
					      const uint8_t *data, size_t length, 
					      const uint8_t *whole_pdu, size_t pdu_length, 
					      enum ntlmssp_direction direction,
					      DATA_BLOB *sig, bool encrypt_sig)
{
	if (gensec_ntlmssp_state->neg_flags & NTLMSSP_NEGOTIATE_NTLM2) {

		HMACMD5Context ctx;
		uint8_t digest[16];
		uint8_t seq_num[4];

		*sig = data_blob_talloc(sig_mem_ctx, NULL, NTLMSSP_SIG_SIZE);
		if (!sig->data) {
			return NT_STATUS_NO_MEMORY;
		}
			
		switch (direction) {
		case NTLMSSP_SEND:
			SIVAL(seq_num, 0, gensec_ntlmssp_state->crypt.ntlm2.send_seq_num);
			gensec_ntlmssp_state->crypt.ntlm2.send_seq_num++;
			hmac_md5_init_limK_to_64(gensec_ntlmssp_state->crypt.ntlm2.send_sign_key.data, 
						 gensec_ntlmssp_state->crypt.ntlm2.send_sign_key.length, &ctx);
			break;
		case NTLMSSP_RECEIVE:
			SIVAL(seq_num, 0, gensec_ntlmssp_state->crypt.ntlm2.recv_seq_num);
			gensec_ntlmssp_state->crypt.ntlm2.recv_seq_num++;
			hmac_md5_init_limK_to_64(gensec_ntlmssp_state->crypt.ntlm2.recv_sign_key.data, 
						 gensec_ntlmssp_state->crypt.ntlm2.recv_sign_key.length, &ctx);
			break;
		}
		hmac_md5_update(seq_num, sizeof(seq_num), &ctx);
		hmac_md5_update(whole_pdu, pdu_length, &ctx);
		hmac_md5_final(digest, &ctx);

		if (encrypt_sig && gensec_ntlmssp_state->neg_flags & NTLMSSP_NEGOTIATE_KEY_EXCH) {
			switch (direction) {
			case NTLMSSP_SEND:
				arcfour_crypt_sbox(gensec_ntlmssp_state->crypt.ntlm2.send_seal_arcfour_state, digest, 8);
				break;
			case NTLMSSP_RECEIVE:
				arcfour_crypt_sbox(gensec_ntlmssp_state->crypt.ntlm2.recv_seal_arcfour_state, digest, 8);
				break;
			}
		}

		SIVAL(sig->data, 0, NTLMSSP_SIGN_VERSION);
		memcpy(sig->data + 4, digest, 8);
		memcpy(sig->data + 12, seq_num, 4);

		DEBUG(10, ("NTLM2: created signature over %llu bytes of input:\n", (unsigned long long)pdu_length));
		dump_data(11, sig->data, sig->length);
			
	} else {
		uint32_t crc;
		crc = crc32_calc_buffer(data, length);
		if (!msrpc_gen(sig_mem_ctx, 
			       sig, "dddd", NTLMSSP_SIGN_VERSION, 0, crc, gensec_ntlmssp_state->crypt.ntlm.seq_num)) {
			return NT_STATUS_NO_MEMORY;
		}
		gensec_ntlmssp_state->crypt.ntlm.seq_num++;

		arcfour_crypt_sbox(gensec_ntlmssp_state->crypt.ntlm.arcfour_state, sig->data+4, sig->length-4);

		DEBUG(10, ("NTLM1: created signature over %llu bytes of input:\n", (unsigned long long)length));
		dump_data(11, sig->data, sig->length);
	}
	return NT_STATUS_OK;
}

/* TODO: make this non-public */
NTSTATUS gensec_ntlmssp_sign_packet(struct gensec_security *gensec_security, 
				    TALLOC_CTX *sig_mem_ctx, 
				    const uint8_t *data, size_t length, 
				    const uint8_t *whole_pdu, size_t pdu_length, 
				    DATA_BLOB *sig)
{
	struct gensec_ntlmssp_state *gensec_ntlmssp_state = (struct gensec_ntlmssp_state *)gensec_security->private_data;

	return ntlmssp_make_packet_signature(gensec_ntlmssp_state, sig_mem_ctx, 
					     data, length, 
					     whole_pdu, pdu_length, 
					     NTLMSSP_SEND, sig, true);
}

/**
 * Check the signature of an incoming packet 
 *
 */

NTSTATUS gensec_ntlmssp_check_packet(struct gensec_security *gensec_security, 
				     TALLOC_CTX *sig_mem_ctx, 
				     const uint8_t *data, size_t length, 
				     const uint8_t *whole_pdu, size_t pdu_length, 
				     const DATA_BLOB *sig)
{
	struct gensec_ntlmssp_state *gensec_ntlmssp_state = (struct gensec_ntlmssp_state *)gensec_security->private_data;

	DATA_BLOB local_sig;
	NTSTATUS nt_status;

	if (!gensec_ntlmssp_state->session_key.length) {
		DEBUG(3, ("NO session key, cannot check packet signature\n"));
		return NT_STATUS_NO_USER_SESSION_KEY;
	}

	nt_status = ntlmssp_make_packet_signature(gensec_ntlmssp_state, sig_mem_ctx, 
						  data, length, 
						  whole_pdu, pdu_length, 
						  NTLMSSP_RECEIVE, &local_sig, true);
	
	if (!NT_STATUS_IS_OK(nt_status)) {
		DEBUG(0, ("NTLMSSP packet check failed with %s\n", nt_errstr(nt_status)));
		return nt_status;
	}

	if (gensec_ntlmssp_state->neg_flags & NTLMSSP_NEGOTIATE_NTLM2) {
		if (local_sig.length != sig->length ||
		    memcmp(local_sig.data, 
			   sig->data, sig->length) != 0) {
			DEBUG(5, ("BAD SIG NTLM2: wanted signature over %llu bytes of input:\n", (unsigned long long)pdu_length));
			dump_data(5, local_sig.data, local_sig.length);
			
			DEBUG(5, ("BAD SIG: got signature over %llu bytes of input:\n", (unsigned long long)pdu_length));
			dump_data(5, sig->data, sig->length);
			
			DEBUG(1, ("NTLMSSP NTLM2 packet check failed due to invalid signature on %llu bytes of input!\n", (unsigned long long)pdu_length));
			return NT_STATUS_ACCESS_DENIED;
		}
	} else {
		if (local_sig.length != sig->length ||
		    memcmp(local_sig.data + 8, 
			   sig->data + 8, sig->length - 8) != 0) {
			DEBUG(5, ("BAD SIG NTLM1: wanted signature of %llu bytes of input:\n", (unsigned long long)length));
			dump_data(5, local_sig.data, local_sig.length);
			
			DEBUG(5, ("BAD SIG: got signature of %llu bytes of input:\n", (unsigned long long)length));
			dump_data(5, sig->data, sig->length);
			
			DEBUG(1, ("NTLMSSP NTLM1 packet check failed due to invalid signature on %llu bytes of input:\n", (unsigned long long)length));
			return NT_STATUS_ACCESS_DENIED;
		}
	}
	dump_data_pw("checked ntlmssp signature\n", sig->data, sig->length);

	return NT_STATUS_OK;
}


/**
 * Seal data with the NTLMSSP algorithm
 *
 */

NTSTATUS gensec_ntlmssp_seal_packet(struct gensec_security *gensec_security, 
				    TALLOC_CTX *sig_mem_ctx, 
				    uint8_t *data, size_t length, 
				    const uint8_t *whole_pdu, size_t pdu_length, 
				    DATA_BLOB *sig)
{
	struct gensec_ntlmssp_state *gensec_ntlmssp_state = (struct gensec_ntlmssp_state *)gensec_security->private_data;
	NTSTATUS nt_status;
	if (!gensec_ntlmssp_state->session_key.length) {
		DEBUG(3, ("NO session key, cannot seal packet\n"));
		return NT_STATUS_NO_USER_SESSION_KEY;
	}

	DEBUG(10,("ntlmssp_seal_data: seal\n"));
	dump_data_pw("ntlmssp clear data\n", data, length);
	if (gensec_ntlmssp_state->neg_flags & NTLMSSP_NEGOTIATE_NTLM2) {
		/* The order of these two operations matters - we must first seal the packet,
		   then seal the sequence number - this is becouse the send_seal_hash is not
		   constant, but is is rather updated with each iteration */
		nt_status = ntlmssp_make_packet_signature(gensec_ntlmssp_state, sig_mem_ctx, 
							  data, length, 
							  whole_pdu, pdu_length, 
							  NTLMSSP_SEND, sig, false);
		arcfour_crypt_sbox(gensec_ntlmssp_state->crypt.ntlm2.send_seal_arcfour_state, data, length);
		if (gensec_ntlmssp_state->neg_flags & NTLMSSP_NEGOTIATE_KEY_EXCH) {
			arcfour_crypt_sbox(gensec_ntlmssp_state->crypt.ntlm2.send_seal_arcfour_state, sig->data+4, 8);
		}
	} else {
		uint32_t crc;
		crc = crc32_calc_buffer(data, length);
		if (!msrpc_gen(sig_mem_ctx, 
			       sig, "dddd", NTLMSSP_SIGN_VERSION, 0, crc, gensec_ntlmssp_state->crypt.ntlm.seq_num)) {
			return NT_STATUS_NO_MEMORY;
		}

		/* The order of these two operations matters - we must
		   first seal the packet, then seal the sequence
		   number - this is becouse the ntlmssp_hash is not
		   constant, but is is rather updated with each
		   iteration */

		arcfour_crypt_sbox(gensec_ntlmssp_state->crypt.ntlm.arcfour_state, data, length);
		arcfour_crypt_sbox(gensec_ntlmssp_state->crypt.ntlm.arcfour_state, sig->data+4, sig->length-4);
		/* increment counter on send */
		gensec_ntlmssp_state->crypt.ntlm.seq_num++;
		nt_status = NT_STATUS_OK;
	}
	dump_data_pw("ntlmssp signature\n", sig->data, sig->length);
	dump_data_pw("ntlmssp sealed data\n", data, length);


	return nt_status;
}

/**
 * Unseal data with the NTLMSSP algorithm
 *
 */

/*
  wrappers for the ntlmssp_*() functions
*/
NTSTATUS gensec_ntlmssp_unseal_packet(struct gensec_security *gensec_security, 
				      TALLOC_CTX *sig_mem_ctx, 
				      uint8_t *data, size_t length, 
				      const uint8_t *whole_pdu, size_t pdu_length, 
				      const DATA_BLOB *sig)
{
	struct gensec_ntlmssp_state *gensec_ntlmssp_state = (struct gensec_ntlmssp_state *)gensec_security->private_data;
	if (!gensec_ntlmssp_state->session_key.length) {
		DEBUG(3, ("NO session key, cannot unseal packet\n"));
		return NT_STATUS_NO_USER_SESSION_KEY;
	}

	dump_data_pw("ntlmssp sealed data\n", data, length);
	if (gensec_ntlmssp_state->neg_flags & NTLMSSP_NEGOTIATE_NTLM2) {
		arcfour_crypt_sbox(gensec_ntlmssp_state->crypt.ntlm2.recv_seal_arcfour_state, data, length);
	} else {
		arcfour_crypt_sbox(gensec_ntlmssp_state->crypt.ntlm.arcfour_state, data, length);
	}
	dump_data_pw("ntlmssp clear data\n", data, length);
	return gensec_ntlmssp_check_packet(gensec_security, sig_mem_ctx, data, length, whole_pdu, pdu_length, sig);
}

/**
   Initialise the state for NTLMSSP signing.
*/
/* TODO: make this non-public */
NTSTATUS ntlmssp_sign_init(struct gensec_ntlmssp_state *gensec_ntlmssp_state)
{
	TALLOC_CTX *mem_ctx = talloc_new(gensec_ntlmssp_state);

	if (!mem_ctx) {
		return NT_STATUS_NO_MEMORY;
	}

	DEBUG(3, ("NTLMSSP Sign/Seal - Initialising with flags:\n"));
	debug_ntlmssp_flags(gensec_ntlmssp_state->neg_flags);

	if (gensec_ntlmssp_state->session_key.length < 8) {
		talloc_free(mem_ctx);
		DEBUG(3, ("NO session key, cannot intialise signing\n"));
		return NT_STATUS_NO_USER_SESSION_KEY;
	}

	if (gensec_ntlmssp_state->neg_flags & NTLMSSP_NEGOTIATE_NTLM2)
	{
		DATA_BLOB weak_session_key = gensec_ntlmssp_state->session_key;
		const char *send_sign_const;
		const char *send_seal_const;
		const char *recv_sign_const;
		const char *recv_seal_const;

		DATA_BLOB send_seal_key;
		DATA_BLOB recv_seal_key;

		switch (gensec_ntlmssp_state->role) {
		case NTLMSSP_CLIENT:
			send_sign_const = CLI_SIGN;
			send_seal_const = CLI_SEAL;
			recv_sign_const = SRV_SIGN;
			recv_seal_const = SRV_SEAL;
			break;
		case NTLMSSP_SERVER:
			send_sign_const = SRV_SIGN;
			send_seal_const = SRV_SEAL;
			recv_sign_const = CLI_SIGN;
			recv_seal_const = CLI_SEAL;
			break;
		default:
			talloc_free(mem_ctx);
			return NT_STATUS_INTERNAL_ERROR;
		}
		
		gensec_ntlmssp_state->crypt.ntlm2.send_seal_arcfour_state = talloc(gensec_ntlmssp_state, struct arcfour_state);
		NT_STATUS_HAVE_NO_MEMORY(gensec_ntlmssp_state->crypt.ntlm2.send_seal_arcfour_state);
		gensec_ntlmssp_state->crypt.ntlm2.recv_seal_arcfour_state = talloc(gensec_ntlmssp_state, struct arcfour_state);
		NT_STATUS_HAVE_NO_MEMORY(gensec_ntlmssp_state->crypt.ntlm2.send_seal_arcfour_state);

		/**
		   Weaken NTLMSSP keys to cope with down-level
		   clients, servers and export restrictions.
		   
		   We probably should have some parameters to control
		   this, once we get NTLM2 working.
		*/

		/* Key weakening was not performed on the master key
		 * for NTLM2 (in ntlmssp_weaken_keys()), but must be
		 * done on the encryption subkeys only.  That is why
		 * we don't have this code for the ntlmv1 case.
		 */

		if (gensec_ntlmssp_state->neg_flags & NTLMSSP_NEGOTIATE_128) {
			
		} else if (gensec_ntlmssp_state->neg_flags & NTLMSSP_NEGOTIATE_56) {
			weak_session_key.length = 7;
		} else { /* forty bits */
			weak_session_key.length = 5;
		}
		dump_data_pw("NTLMSSP weakend master key:\n",
			     weak_session_key.data, 
			     weak_session_key.length);

		/* SEND: sign key */
		calc_ntlmv2_key(gensec_ntlmssp_state, 
				&gensec_ntlmssp_state->crypt.ntlm2.send_sign_key, 
				gensec_ntlmssp_state->session_key, send_sign_const);
		dump_data_pw("NTLMSSP send sign key:\n",
			     gensec_ntlmssp_state->crypt.ntlm2.send_sign_key.data, 
			     gensec_ntlmssp_state->crypt.ntlm2.send_sign_key.length);
		
		/* SEND: seal ARCFOUR pad */
		calc_ntlmv2_key(mem_ctx, 
				&send_seal_key, 
				weak_session_key, send_seal_const);
		dump_data_pw("NTLMSSP send seal key:\n",
			     send_seal_key.data, 
			     send_seal_key.length);
		arcfour_init(gensec_ntlmssp_state->crypt.ntlm2.send_seal_arcfour_state, 
			     &send_seal_key);
		dump_data_pw("NTLMSSP send sesl hash:\n", 
			     gensec_ntlmssp_state->crypt.ntlm2.send_seal_arcfour_state->sbox, 
			     sizeof(gensec_ntlmssp_state->crypt.ntlm2.send_seal_arcfour_state->sbox));

		/* RECV: sign key */
		calc_ntlmv2_key(gensec_ntlmssp_state, 
				&gensec_ntlmssp_state->crypt.ntlm2.recv_sign_key, 
				gensec_ntlmssp_state->session_key, recv_sign_const);
		dump_data_pw("NTLMSSP recv sign key:\n",
			     gensec_ntlmssp_state->crypt.ntlm2.recv_sign_key.data, 
			     gensec_ntlmssp_state->crypt.ntlm2.recv_sign_key.length);

		/* RECV: seal ARCFOUR pad */
		calc_ntlmv2_key(mem_ctx, 
				&recv_seal_key, 
				weak_session_key, recv_seal_const);
		dump_data_pw("NTLMSSP recv seal key:\n",
			     recv_seal_key.data, 
			     recv_seal_key.length);
		arcfour_init(gensec_ntlmssp_state->crypt.ntlm2.recv_seal_arcfour_state, 
			     &recv_seal_key);
		dump_data_pw("NTLMSSP receive seal hash:\n", 
			     gensec_ntlmssp_state->crypt.ntlm2.recv_seal_arcfour_state->sbox, 
			     sizeof(gensec_ntlmssp_state->crypt.ntlm2.recv_seal_arcfour_state->sbox));

		gensec_ntlmssp_state->crypt.ntlm2.send_seq_num = 0;
		gensec_ntlmssp_state->crypt.ntlm2.recv_seq_num = 0;

	} else {
		DATA_BLOB weak_session_key = ntlmssp_weakend_key(gensec_ntlmssp_state, mem_ctx);
		DEBUG(5, ("NTLMSSP Sign/Seal - using NTLM1\n"));

		gensec_ntlmssp_state->crypt.ntlm.arcfour_state = talloc(gensec_ntlmssp_state, struct arcfour_state);
		NT_STATUS_HAVE_NO_MEMORY(gensec_ntlmssp_state->crypt.ntlm.arcfour_state);

		arcfour_init(gensec_ntlmssp_state->crypt.ntlm.arcfour_state, 
			     &weak_session_key);
		dump_data_pw("NTLMSSP hash:\n", gensec_ntlmssp_state->crypt.ntlm.arcfour_state->sbox,
			     sizeof(gensec_ntlmssp_state->crypt.ntlm.arcfour_state->sbox));

		gensec_ntlmssp_state->crypt.ntlm.seq_num = 0;
	}

	talloc_free(mem_ctx);
	return NT_STATUS_OK;
}

size_t gensec_ntlmssp_sig_size(struct gensec_security *gensec_security, size_t data_size) 
{
	return NTLMSSP_SIG_SIZE;
}

NTSTATUS gensec_ntlmssp_wrap(struct gensec_security *gensec_security, 
			     TALLOC_CTX *sig_mem_ctx, 
			     const DATA_BLOB *in, 
			     DATA_BLOB *out)
{
	DATA_BLOB sig;
	NTSTATUS nt_status;

	if (gensec_have_feature(gensec_security, GENSEC_FEATURE_SEAL)) {

		*out = data_blob_talloc(sig_mem_ctx, NULL, in->length + NTLMSSP_SIG_SIZE);
		if (!out->data) {
			return NT_STATUS_NO_MEMORY;
		}
		memcpy(out->data + NTLMSSP_SIG_SIZE, in->data, in->length);
		
	        nt_status = gensec_ntlmssp_seal_packet(gensec_security, sig_mem_ctx, 
						       out->data + NTLMSSP_SIG_SIZE, 
						       out->length - NTLMSSP_SIG_SIZE, 
						       out->data + NTLMSSP_SIG_SIZE, 
						       out->length - NTLMSSP_SIG_SIZE, 
						       &sig);
		
		if (NT_STATUS_IS_OK(nt_status)) {
			memcpy(out->data, sig.data, NTLMSSP_SIG_SIZE);
		}
		return nt_status;

	} else if (gensec_have_feature(gensec_security, GENSEC_FEATURE_SIGN)) {

		*out = data_blob_talloc(sig_mem_ctx, NULL, in->length + NTLMSSP_SIG_SIZE);
		if (!out->data) {
			return NT_STATUS_NO_MEMORY;
		}
		memcpy(out->data + NTLMSSP_SIG_SIZE, in->data, in->length);

	        nt_status = gensec_ntlmssp_sign_packet(gensec_security, sig_mem_ctx, 
						       out->data + NTLMSSP_SIG_SIZE, 
						       out->length - NTLMSSP_SIG_SIZE, 
						       out->data + NTLMSSP_SIG_SIZE, 
						       out->length - NTLMSSP_SIG_SIZE, 
						       &sig);

		if (NT_STATUS_IS_OK(nt_status)) {
			memcpy(out->data, sig.data, NTLMSSP_SIG_SIZE);
		}
		return nt_status;

	} else {
		*out = *in;
		return NT_STATUS_OK;
	}
}


NTSTATUS gensec_ntlmssp_unwrap(struct gensec_security *gensec_security, 
			       TALLOC_CTX *sig_mem_ctx, 
			       const DATA_BLOB *in, 
			       DATA_BLOB *out)
{
	DATA_BLOB sig;

	if (gensec_have_feature(gensec_security, GENSEC_FEATURE_SEAL)) {
		if (in->length < NTLMSSP_SIG_SIZE) {
			return NT_STATUS_INVALID_PARAMETER;
		}
		sig.data = in->data;
		sig.length = NTLMSSP_SIG_SIZE;

		*out = data_blob_talloc(sig_mem_ctx, in->data + NTLMSSP_SIG_SIZE, in->length - NTLMSSP_SIG_SIZE);
		
	        return gensec_ntlmssp_unseal_packet(gensec_security, sig_mem_ctx, 
						    out->data, out->length, 
						    out->data, out->length, 
						    &sig);
						  
	} else if (gensec_have_feature(gensec_security, GENSEC_FEATURE_SIGN)) {
		struct gensec_ntlmssp_state *gensec_ntlmssp_state =
		(struct gensec_ntlmssp_state *)gensec_security->private_data;
		NTSTATUS status;
		uint32_t ntlm_seqnum;
		struct arcfour_state ntlm_state;
		uint32_t ntlm2_seqnum_r;
		uint8_t ntlm2_key_r[16];
		struct arcfour_state ntlm2_state_r;

		if (in->length < NTLMSSP_SIG_SIZE) {
			return NT_STATUS_INVALID_PARAMETER;
		}
		sig.data = in->data;
		sig.length = NTLMSSP_SIG_SIZE;
		*out = data_blob_talloc(sig_mem_ctx, in->data + NTLMSSP_SIG_SIZE, in->length - NTLMSSP_SIG_SIZE);

		if (gensec_ntlmssp_state->neg_flags & NTLMSSP_NEGOTIATE_NTLM2) {
			ntlm2_seqnum_r = gensec_ntlmssp_state->crypt.ntlm2.recv_seq_num;
			ntlm2_state_r = *gensec_ntlmssp_state->crypt.ntlm2.recv_seal_arcfour_state;
			memcpy(ntlm2_key_r,
			       gensec_ntlmssp_state->crypt.ntlm2.recv_sign_key.data,
			       16);
		} else {
			ntlm_seqnum = gensec_ntlmssp_state->crypt.ntlm.seq_num;
			ntlm_state = *gensec_ntlmssp_state->crypt.ntlm.arcfour_state;
		}

		status = gensec_ntlmssp_check_packet(gensec_security, sig_mem_ctx,
						     out->data, out->length,
						     out->data, out->length,
						     &sig);
		if (!NT_STATUS_IS_OK(status)) {
			NTSTATUS check_status = status;
			/*
			 * The Windows LDAP libraries seems to have a bug
			 * and always use sealing even if only signing was
			 * negotiated. So we need to fallback.
			 */

			if (gensec_ntlmssp_state->neg_flags & NTLMSSP_NEGOTIATE_NTLM2) {
				gensec_ntlmssp_state->crypt.ntlm2.recv_seq_num = ntlm2_seqnum_r;
				*gensec_ntlmssp_state->crypt.ntlm2.recv_seal_arcfour_state = ntlm2_state_r;
				memcpy(gensec_ntlmssp_state->crypt.ntlm2.recv_sign_key.data,
				       ntlm2_key_r, 16);
			} else {
				gensec_ntlmssp_state->crypt.ntlm.seq_num = ntlm_seqnum;
				*gensec_ntlmssp_state->crypt.ntlm.arcfour_state = ntlm_state;
			}

			status = gensec_ntlmssp_unseal_packet(gensec_security,
							      sig_mem_ctx,
							      out->data,
							      out->length,
							      out->data,
							      out->length,
							      &sig);
			if (NT_STATUS_IS_OK(status)) {
				gensec_ntlmssp_state->neg_flags |= NTLMSSP_NEGOTIATE_SEAL;
			} else {
				status = check_status;
			}
		}
		return status;
	} else {
		*out = *in;
		return NT_STATUS_OK;
	}
}

