/*
 *  Unix SMB/CIFS implementation.
 *  Version 3.0
 *  NTLMSSP Signing routines
 *  Copyright (C) Andrew Bartlett 2003-2005
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
#include "../libcli/auth/ntlmssp.h"
#include "../libcli/auth/libcli_auth.h"
#include "../lib/crypto/md5.h"
#include "../lib/crypto/hmacmd5.h"
#include "../lib/crypto/crc32.h"
#include "../libcli/auth/ntlmssp_private.h"

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

static void dump_arc4_state(const char *description,
			    struct arcfour_state *state)
{
	dump_data_pw(description, state->sbox, sizeof(state->sbox));
}

static void calc_ntlmv2_key(uint8_t subkey[16],
			    DATA_BLOB session_key,
			    const char *constant)
{
	MD5_CTX ctx3;
	MD5Init(&ctx3);
	MD5Update(&ctx3, session_key.data, session_key.length);
	MD5Update(&ctx3, (const uint8_t *)constant, strlen(constant)+1);
	MD5Final(subkey, &ctx3);
}

enum ntlmssp_direction {
	NTLMSSP_SEND,
	NTLMSSP_RECEIVE
};

static NTSTATUS ntlmssp_make_packet_signature(struct ntlmssp_state *ntlmssp_state,
					      TALLOC_CTX *sig_mem_ctx,
					      const uint8_t *data, size_t length,
					      const uint8_t *whole_pdu, size_t pdu_length,
					      enum ntlmssp_direction direction,
					      DATA_BLOB *sig, bool encrypt_sig)
{
	if (ntlmssp_state->neg_flags & NTLMSSP_NEGOTIATE_NTLM2) {
		HMACMD5Context ctx;
		uint8_t digest[16];
		uint8_t seq_num[4];

		*sig = data_blob_talloc(sig_mem_ctx, NULL, NTLMSSP_SIG_SIZE);
		if (!sig->data) {
			return NT_STATUS_NO_MEMORY;
		}

		switch (direction) {
		case NTLMSSP_SEND:
			DEBUG(100,("ntlmssp_make_packet_signature: SEND seq = %u, len = %u, pdu_len = %u\n",
				ntlmssp_state->crypt->ntlm2.sending.seq_num,
				(unsigned int)length,
				(unsigned int)pdu_length));

			SIVAL(seq_num, 0, ntlmssp_state->crypt->ntlm2.sending.seq_num);
			ntlmssp_state->crypt->ntlm2.sending.seq_num++;
			hmac_md5_init_limK_to_64(ntlmssp_state->crypt->ntlm2.sending.sign_key, 16, &ctx);
			break;
		case NTLMSSP_RECEIVE:

			DEBUG(100,("ntlmssp_make_packet_signature: RECV seq = %u, len = %u, pdu_len = %u\n",
				ntlmssp_state->crypt->ntlm2.receiving.seq_num,
				(unsigned int)length,
				(unsigned int)pdu_length));

			SIVAL(seq_num, 0, ntlmssp_state->crypt->ntlm2.receiving.seq_num);
			ntlmssp_state->crypt->ntlm2.receiving.seq_num++;
			hmac_md5_init_limK_to_64(ntlmssp_state->crypt->ntlm2.receiving.sign_key, 16, &ctx);
			break;
		}

		dump_data_pw("pdu data ", whole_pdu, pdu_length);

		hmac_md5_update(seq_num, sizeof(seq_num), &ctx);
		hmac_md5_update(whole_pdu, pdu_length, &ctx);
		hmac_md5_final(digest, &ctx);

		if (encrypt_sig && (ntlmssp_state->neg_flags & NTLMSSP_NEGOTIATE_KEY_EXCH)) {
			switch (direction) {
			case NTLMSSP_SEND:
				arcfour_crypt_sbox(&ntlmssp_state->crypt->ntlm2.sending.seal_state,
						   digest, 8);
				break;
			case NTLMSSP_RECEIVE:
				arcfour_crypt_sbox(&ntlmssp_state->crypt->ntlm2.receiving.seal_state,
						   digest, 8);
				break;
			}
		}

		SIVAL(sig->data, 0, NTLMSSP_SIGN_VERSION);
		memcpy(sig->data + 4, digest, 8);
		memcpy(sig->data + 12, seq_num, 4);

		dump_data_pw("ntlmssp v2 sig ", sig->data, sig->length);

	} else {
		NTSTATUS status;
		uint32_t crc;

		crc = crc32_calc_buffer(data, length);

		status = msrpc_gen(sig_mem_ctx,
			       sig, "dddd",
			       NTLMSSP_SIGN_VERSION, 0, crc,
			       ntlmssp_state->crypt->ntlm.seq_num);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}

		ntlmssp_state->crypt->ntlm.seq_num++;

		dump_arc4_state("ntlmssp hash: \n",
				&ntlmssp_state->crypt->ntlm.seal_state);
		arcfour_crypt_sbox(&ntlmssp_state->crypt->ntlm.seal_state,
				   sig->data+4, sig->length-4);
	}
	return NT_STATUS_OK;
}

NTSTATUS ntlmssp_sign_packet(struct ntlmssp_state *ntlmssp_state,
			     TALLOC_CTX *sig_mem_ctx,
			     const uint8_t *data, size_t length,
			     const uint8_t *whole_pdu, size_t pdu_length,
			     DATA_BLOB *sig)
{
	NTSTATUS nt_status;

	if (!(ntlmssp_state->neg_flags & NTLMSSP_NEGOTIATE_SIGN)) {
		DEBUG(3, ("NTLMSSP Signing not negotiated - cannot sign packet!\n"));
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (!ntlmssp_state->session_key.length) {
		DEBUG(3, ("NO session key, cannot check sign packet\n"));
		return NT_STATUS_NO_USER_SESSION_KEY;
	}

	nt_status = ntlmssp_make_packet_signature(ntlmssp_state,
						  sig_mem_ctx,
						  data, length,
						  whole_pdu, pdu_length,
						  NTLMSSP_SEND, sig, true);

	return nt_status;
}

/**
 * Check the signature of an incoming packet
 * @note caller *must* check that the signature is the size it expects
 *
 */

NTSTATUS ntlmssp_check_packet(struct ntlmssp_state *ntlmssp_state,
			      const uint8_t *data, size_t length,
			      const uint8_t *whole_pdu, size_t pdu_length,
			      const DATA_BLOB *sig)
{
	DATA_BLOB local_sig;
	NTSTATUS nt_status;
	TALLOC_CTX *tmp_ctx;

	if (!ntlmssp_state->session_key.length) {
		DEBUG(3, ("NO session key, cannot check packet signature\n"));
		return NT_STATUS_NO_USER_SESSION_KEY;
	}

	if (sig->length < 8) {
		DEBUG(0, ("NTLMSSP packet check failed due to short signature (%lu bytes)!\n",
			  (unsigned long)sig->length));
	}

	tmp_ctx = talloc_new(ntlmssp_state);
	if (!tmp_ctx) {
		return NT_STATUS_NO_MEMORY;
	}

	nt_status = ntlmssp_make_packet_signature(ntlmssp_state,
						  tmp_ctx,
						  data, length,
						  whole_pdu, pdu_length,
						  NTLMSSP_RECEIVE,
						  &local_sig, true);

	if (!NT_STATUS_IS_OK(nt_status)) {
		DEBUG(0,("NTLMSSP packet sig creation failed with %s\n",
			 nt_errstr(nt_status)));
		talloc_free(tmp_ctx);
		return nt_status;
	}

	if (ntlmssp_state->neg_flags & NTLMSSP_NEGOTIATE_NTLM2) {
		if (local_sig.length != sig->length ||
		    memcmp(local_sig.data, sig->data, sig->length) != 0) {
			DEBUG(5, ("BAD SIG NTLM2: wanted signature of\n"));
			dump_data(5, local_sig.data, local_sig.length);

			DEBUG(5, ("BAD SIG: got signature of\n"));
			dump_data(5, sig->data, sig->length);

			DEBUG(0, ("NTLMSSP NTLM2 packet check failed due to invalid signature!\n"));
			talloc_free(tmp_ctx);
			return NT_STATUS_ACCESS_DENIED;
		}
	} else {
		if (local_sig.length != sig->length ||
		    memcmp(local_sig.data + 8, sig->data + 8, sig->length - 8) != 0) {
			DEBUG(5, ("BAD SIG NTLM1: wanted signature of\n"));
			dump_data(5, local_sig.data, local_sig.length);

			DEBUG(5, ("BAD SIG: got signature of\n"));
			dump_data(5, sig->data, sig->length);

			DEBUG(0, ("NTLMSSP NTLM1 packet check failed due to invalid signature!\n"));
			talloc_free(tmp_ctx);
			return NT_STATUS_ACCESS_DENIED;
		}
	}
	dump_data_pw("checked ntlmssp signature\n", sig->data, sig->length);
	DEBUG(10,("ntlmssp_check_packet: NTLMSSP signature OK !\n"));

	talloc_free(tmp_ctx);
	return NT_STATUS_OK;
}

/**
 * Seal data with the NTLMSSP algorithm
 *
 */

NTSTATUS ntlmssp_seal_packet(struct ntlmssp_state *ntlmssp_state,
			     TALLOC_CTX *sig_mem_ctx,
			     uint8_t *data, size_t length,
			     const uint8_t *whole_pdu, size_t pdu_length,
			     DATA_BLOB *sig)
{
	if (!(ntlmssp_state->neg_flags & NTLMSSP_NEGOTIATE_SEAL)) {
		DEBUG(3, ("NTLMSSP Sealing not negotiated - cannot seal packet!\n"));
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (!ntlmssp_state->session_key.length) {
		DEBUG(3, ("NO session key, cannot seal packet\n"));
		return NT_STATUS_NO_USER_SESSION_KEY;
	}

	DEBUG(10,("ntlmssp_seal_data: seal\n"));
	dump_data_pw("ntlmssp clear data\n", data, length);
	if (ntlmssp_state->neg_flags & NTLMSSP_NEGOTIATE_NTLM2) {
		NTSTATUS nt_status;
		/*
		 * The order of these two operations matters - we
		 * must first seal the packet, then seal the
		 * sequence number - this is because the
		 * send_seal_hash is not constant, but is is rather
		 * updated with each iteration
		 */
		nt_status = ntlmssp_make_packet_signature(ntlmssp_state,
							  sig_mem_ctx,
							  data, length,
							  whole_pdu, pdu_length,
							  NTLMSSP_SEND,
							  sig, false);
		if (!NT_STATUS_IS_OK(nt_status)) {
			return nt_status;
		}

		arcfour_crypt_sbox(&ntlmssp_state->crypt->ntlm2.sending.seal_state,
				   data, length);
		if (ntlmssp_state->neg_flags & NTLMSSP_NEGOTIATE_KEY_EXCH) {
			arcfour_crypt_sbox(&ntlmssp_state->crypt->ntlm2.sending.seal_state,
					   sig->data+4, 8);
		}
	} else {
		NTSTATUS status;
		uint32_t crc;

		crc = crc32_calc_buffer(data, length);

		status = msrpc_gen(sig_mem_ctx,
			       sig, "dddd",
			       NTLMSSP_SIGN_VERSION, 0, crc,
			       ntlmssp_state->crypt->ntlm.seq_num);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}

		/*
		 * The order of these two operations matters - we
		 * must first seal the packet, then seal the
		 * sequence number - this is because the ntlmv1_arc4_state
		 * is not constant, but is is rather updated with
		 * each iteration
		 */

		dump_arc4_state("ntlmv1 arc4 state:\n",
				&ntlmssp_state->crypt->ntlm.seal_state);
		arcfour_crypt_sbox(&ntlmssp_state->crypt->ntlm.seal_state,
				   data, length);

		dump_arc4_state("ntlmv1 arc4 state:\n",
				&ntlmssp_state->crypt->ntlm.seal_state);

		arcfour_crypt_sbox(&ntlmssp_state->crypt->ntlm.seal_state,
				   sig->data+4, sig->length-4);

		ntlmssp_state->crypt->ntlm.seq_num++;
	}
	dump_data_pw("ntlmssp signature\n", sig->data, sig->length);
	dump_data_pw("ntlmssp sealed data\n", data, length);

	return NT_STATUS_OK;
}

/**
 * Unseal data with the NTLMSSP algorithm
 *
 */

NTSTATUS ntlmssp_unseal_packet(struct ntlmssp_state *ntlmssp_state,
			       uint8_t *data, size_t length,
			       const uint8_t *whole_pdu, size_t pdu_length,
			       const DATA_BLOB *sig)
{
	NTSTATUS status;
	if (!ntlmssp_state->session_key.length) {
		DEBUG(3, ("NO session key, cannot unseal packet\n"));
		return NT_STATUS_NO_USER_SESSION_KEY;
	}

	DEBUG(10,("ntlmssp_unseal_packet: seal\n"));
	dump_data_pw("ntlmssp sealed data\n", data, length);

	if (ntlmssp_state->neg_flags & NTLMSSP_NEGOTIATE_NTLM2) {
		/* First unseal the data. */
		arcfour_crypt_sbox(&ntlmssp_state->crypt->ntlm2.receiving.seal_state,
				   data, length);
		dump_data_pw("ntlmv2 clear data\n", data, length);
	} else {
		arcfour_crypt_sbox(&ntlmssp_state->crypt->ntlm.seal_state,
				   data, length);
		dump_data_pw("ntlmv1 clear data\n", data, length);
	}
	status = ntlmssp_check_packet(ntlmssp_state,
				      data, length,
				      whole_pdu, pdu_length,
				      sig);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1,("NTLMSSP packet check for unseal failed due to invalid signature on %llu bytes of input:\n",
			 (unsigned long long)length));
	}
	return status;
}

NTSTATUS ntlmssp_wrap(struct ntlmssp_state *ntlmssp_state,
		      TALLOC_CTX *out_mem_ctx,
		      const DATA_BLOB *in,
		      DATA_BLOB *out)
{
	NTSTATUS nt_status;
	DATA_BLOB sig;

	if (ntlmssp_state->neg_flags & NTLMSSP_NEGOTIATE_SEAL) {
		*out = data_blob_talloc(out_mem_ctx, NULL, in->length + NTLMSSP_SIG_SIZE);
		if (!out->data) {
			return NT_STATUS_NO_MEMORY;
		}
		memcpy(out->data + NTLMSSP_SIG_SIZE, in->data, in->length);

	        nt_status = ntlmssp_seal_packet(ntlmssp_state, out_mem_ctx,
						out->data + NTLMSSP_SIG_SIZE,
						out->length - NTLMSSP_SIG_SIZE,
						out->data + NTLMSSP_SIG_SIZE,
						out->length - NTLMSSP_SIG_SIZE,
						&sig);

		if (NT_STATUS_IS_OK(nt_status)) {
			memcpy(out->data, sig.data, NTLMSSP_SIG_SIZE);
			talloc_free(sig.data);
		}
		return nt_status;

	} else if (ntlmssp_state->neg_flags & NTLMSSP_NEGOTIATE_SIGN) {

		*out = data_blob_talloc(out_mem_ctx, NULL, in->length + NTLMSSP_SIG_SIZE);
		if (!out->data) {
			return NT_STATUS_NO_MEMORY;
		}
		memcpy(out->data + NTLMSSP_SIG_SIZE, in->data, in->length);

	        nt_status = ntlmssp_sign_packet(ntlmssp_state, out_mem_ctx,
						out->data + NTLMSSP_SIG_SIZE,
						out->length - NTLMSSP_SIG_SIZE,
						out->data + NTLMSSP_SIG_SIZE,
						out->length - NTLMSSP_SIG_SIZE,
						&sig);

		if (NT_STATUS_IS_OK(nt_status)) {
			memcpy(out->data, sig.data, NTLMSSP_SIG_SIZE);
			talloc_free(sig.data);
		}
		return nt_status;
	} else {
		*out = data_blob_talloc(out_mem_ctx, in->data, in->length);
		if (!out->data) {
			return NT_STATUS_NO_MEMORY;
		}
		return NT_STATUS_OK;
	}
}

NTSTATUS ntlmssp_unwrap(struct ntlmssp_state *ntlmssp_state,
			TALLOC_CTX *out_mem_ctx,
			const DATA_BLOB *in,
			DATA_BLOB *out)
{
	DATA_BLOB sig;

	if (ntlmssp_state->neg_flags & NTLMSSP_NEGOTIATE_SEAL) {
		if (in->length < NTLMSSP_SIG_SIZE) {
			return NT_STATUS_INVALID_PARAMETER;
		}
		sig.data = in->data;
		sig.length = NTLMSSP_SIG_SIZE;

		*out = data_blob_talloc(out_mem_ctx, in->data + NTLMSSP_SIG_SIZE, in->length - NTLMSSP_SIG_SIZE);

	        return ntlmssp_unseal_packet(ntlmssp_state,
					     out->data, out->length,
					     out->data, out->length,
					     &sig);

	} else if (ntlmssp_state->neg_flags & NTLMSSP_NEGOTIATE_SIGN) {
		NTSTATUS status;
		struct ntlmssp_crypt_direction save_direction;

		if (in->length < NTLMSSP_SIG_SIZE) {
			return NT_STATUS_INVALID_PARAMETER;
		}
		sig.data = in->data;
		sig.length = NTLMSSP_SIG_SIZE;
		*out = data_blob_talloc(out_mem_ctx, in->data + NTLMSSP_SIG_SIZE, in->length - NTLMSSP_SIG_SIZE);

		if (ntlmssp_state->neg_flags & NTLMSSP_NEGOTIATE_NTLM2) {
			save_direction = ntlmssp_state->crypt->ntlm2.receiving;
		} else {
			save_direction = ntlmssp_state->crypt->ntlm;
		}

		status = ntlmssp_check_packet(ntlmssp_state,
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

			if (ntlmssp_state->neg_flags & NTLMSSP_NEGOTIATE_NTLM2) {
				ntlmssp_state->crypt->ntlm2.receiving = save_direction;
			} else {
				ntlmssp_state->crypt->ntlm = save_direction;
			}

			status = ntlmssp_unseal_packet(ntlmssp_state,
						       out->data,
						       out->length,
						       out->data,
						       out->length,
						       &sig);
			if (NT_STATUS_IS_OK(status)) {
				ntlmssp_state->neg_flags |= NTLMSSP_NEGOTIATE_SEAL;
			} else {
				status = check_status;
			}
		}

		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(1, ("NTLMSSP packet check for unwrap failed due to invalid signature\n"));
		}
		return status;
	} else {
		*out = data_blob_talloc(out_mem_ctx, in->data, in->length);
		if (!out->data) {
			return NT_STATUS_NO_MEMORY;
		}
		return NT_STATUS_OK;
	}
}

/**
   Initialise the state for NTLMSSP signing.
*/
NTSTATUS ntlmssp_sign_init(struct ntlmssp_state *ntlmssp_state)
{
	DEBUG(3, ("NTLMSSP Sign/Seal - Initialising with flags:\n"));
	debug_ntlmssp_flags(ntlmssp_state->neg_flags);

	if (ntlmssp_state->session_key.length < 8) {
		DEBUG(3, ("NO session key, cannot intialise signing\n"));
		return NT_STATUS_NO_USER_SESSION_KEY;
	}

	ntlmssp_state->crypt = talloc_zero(ntlmssp_state,
					   union ntlmssp_crypt_state);
	if (ntlmssp_state->crypt == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	if (ntlmssp_state->neg_flags & NTLMSSP_NEGOTIATE_NTLM2) {
		DATA_BLOB weak_session_key = ntlmssp_state->session_key;
		const char *send_sign_const;
		const char *send_seal_const;
		const char *recv_sign_const;
		const char *recv_seal_const;
		uint8_t send_seal_key[16];
		DATA_BLOB send_seal_blob = data_blob_const(send_seal_key,
					   sizeof(send_seal_key));
		uint8_t recv_seal_key[16];
		DATA_BLOB recv_seal_blob = data_blob_const(recv_seal_key,
					   sizeof(recv_seal_key));

		switch (ntlmssp_state->role) {
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
			return NT_STATUS_INTERNAL_ERROR;
		}

		/*
		 * Weaken NTLMSSP keys to cope with down-level
		 * clients, servers and export restrictions.
		 *
		 * We probably should have some parameters to
		 * control this, once we get NTLM2 working.
		 *
		 * Key weakening was not performed on the master key
		 * for NTLM2, but must be done on the encryption subkeys only.
		 */

		if (ntlmssp_state->neg_flags & NTLMSSP_NEGOTIATE_128) {
			/* nothing to do */
		} else if (ntlmssp_state->neg_flags & NTLMSSP_NEGOTIATE_56) {
			weak_session_key.length = 7;
		} else { /* forty bits */
			weak_session_key.length = 5;
		}

		dump_data_pw("NTLMSSP weakend master key:\n",
			     weak_session_key.data,
			     weak_session_key.length);

		/* SEND: sign key */
		calc_ntlmv2_key(ntlmssp_state->crypt->ntlm2.sending.sign_key,
				ntlmssp_state->session_key, send_sign_const);
		dump_data_pw("NTLMSSP send sign key:\n",
			     ntlmssp_state->crypt->ntlm2.sending.sign_key, 16);

		/* SEND: seal ARCFOUR pad */
		calc_ntlmv2_key(send_seal_key,
				weak_session_key, send_seal_const);
		dump_data_pw("NTLMSSP send seal key:\n", send_seal_key, 16);

		arcfour_init(&ntlmssp_state->crypt->ntlm2.sending.seal_state,
			     &send_seal_blob);

		dump_arc4_state("NTLMSSP send seal arc4 state:\n",
				&ntlmssp_state->crypt->ntlm2.sending.seal_state);

		/* SEND: seq num */
		ntlmssp_state->crypt->ntlm2.sending.seq_num = 0;

		/* RECV: sign key */
		calc_ntlmv2_key(ntlmssp_state->crypt->ntlm2.receiving.sign_key,
				ntlmssp_state->session_key, recv_sign_const);
		dump_data_pw("NTLMSSP recv sign key:\n",
			     ntlmssp_state->crypt->ntlm2.receiving.sign_key, 16);

		/* RECV: seal ARCFOUR pad */
		calc_ntlmv2_key(recv_seal_key,
				weak_session_key, recv_seal_const);
		dump_data_pw("NTLMSSP recv seal key:\n", recv_seal_key, 16);

		arcfour_init(&ntlmssp_state->crypt->ntlm2.receiving.seal_state,
			     &recv_seal_blob);

		dump_arc4_state("NTLMSSP recv seal arc4 state:\n",
				&ntlmssp_state->crypt->ntlm2.receiving.seal_state);

		/* RECV: seq num */
		ntlmssp_state->crypt->ntlm2.receiving.seq_num = 0;
	} else {
		uint8_t weak_session_key[8];
		DATA_BLOB seal_session_key = ntlmssp_state->session_key;
		bool do_weak = false;

		DEBUG(5, ("NTLMSSP Sign/Seal - using NTLM1\n"));

		/*
		 * Key weakening not performed on the master key for NTLM2
		 * and does not occour for NTLM1. Therefore we only need
		 * to do this for the LM_KEY.
		 */
		if (ntlmssp_state->neg_flags & NTLMSSP_NEGOTIATE_LM_KEY) {
			do_weak = true;
		}

		/*
		 * Nothing to weaken.
		 * We certainly don't want to 'extend' the length...
		 */
		if (seal_session_key.length < 16) {
			/* TODO: is this really correct? */
			do_weak = false;
		}

		if (do_weak) {
			memcpy(weak_session_key, seal_session_key.data, 8);
			seal_session_key = data_blob_const(weak_session_key, 8);

			/*
			 * LM key doesn't support 128 bit crypto, so this is
			 * the best we can do. If you negotiate 128 bit, but
			 * not 56, you end up with 40 bit...
			 */
			if (ntlmssp_state->neg_flags & NTLMSSP_NEGOTIATE_56) {
				weak_session_key[7] = 0xa0;
			} else { /* forty bits */
				weak_session_key[5] = 0xe5;
				weak_session_key[6] = 0x38;
				weak_session_key[7] = 0xb0;
			}
		}

		arcfour_init(&ntlmssp_state->crypt->ntlm.seal_state,
			     &seal_session_key);

		dump_arc4_state("NTLMv1 arc4 state:\n",
				&ntlmssp_state->crypt->ntlm.seal_state);

		ntlmssp_state->crypt->ntlm.seq_num = 0;
	}

	return NT_STATUS_OK;
}
