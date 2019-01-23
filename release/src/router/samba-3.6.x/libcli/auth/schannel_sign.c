/*
   Unix SMB/CIFS implementation.

   schannel library code

   Copyright (C) Andrew Tridgell 2004
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2005

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
#include "../libcli/auth/schannel.h"
#include "../lib/crypto/crypto.h"

static void netsec_offset_and_sizes(struct schannel_state *state,
				    bool do_seal,
				    uint32_t *_min_sig_size,
				    uint32_t *_used_sig_size,
				    uint32_t *_checksum_length,
				    uint32_t *_confounder_ofs)
{
	uint32_t min_sig_size = 24;
	uint32_t used_sig_size = 32;
	uint32_t checksum_length = 8;
	uint32_t confounder_ofs = 24;

	if (do_seal) {
		min_sig_size += 8;
	}

	if (_min_sig_size) {
		*_min_sig_size = min_sig_size;
	}

	if (_used_sig_size) {
		*_used_sig_size = used_sig_size;
	}

	if (_checksum_length) {
		*_checksum_length = checksum_length;
	}

	if (_confounder_ofs) {
		*_confounder_ofs = confounder_ofs;
	}
}

/*******************************************************************
 Encode or Decode the sequence number (which is symmetric)
 ********************************************************************/
static void netsec_do_seq_num(struct schannel_state *state,
			      const uint8_t *checksum,
			      uint32_t checksum_length,
			      uint8_t seq_num[8])
{
	static const uint8_t zeros[4];
	uint8_t sequence_key[16];
	uint8_t digest1[16];

	hmac_md5(state->creds->session_key, zeros, sizeof(zeros), digest1);
	hmac_md5(digest1, checksum, checksum_length, sequence_key);
	arcfour_crypt(seq_num, sequence_key, 8);

	state->seq_num++;
}

static void netsec_do_seal(struct schannel_state *state,
			   const uint8_t seq_num[8],
			   uint8_t confounder[8],
			   uint8_t *data, uint32_t length)
{
	uint8_t sealing_key[16];
	static const uint8_t zeros[4];
	uint8_t digest2[16];
	uint8_t sess_kf0[16];
	int i;

	for (i = 0; i < 16; i++) {
		sess_kf0[i] = state->creds->session_key[i] ^ 0xf0;
	}

	hmac_md5(sess_kf0, zeros, 4, digest2);
	hmac_md5(digest2, seq_num, 8, sealing_key);

	arcfour_crypt(confounder, sealing_key, 8);
	arcfour_crypt(data, sealing_key, length);
}

/*******************************************************************
 Create a digest over the entire packet (including the data), and
 MD5 it with the session key.
 ********************************************************************/
static void netsec_do_sign(struct schannel_state *state,
			   const uint8_t *confounder,
			   const uint8_t *data, size_t data_len,
			   uint8_t header[8],
			   uint8_t *checksum)
{
	uint8_t packet_digest[16];
	static const uint8_t zeros[4];
	MD5_CTX ctx;

	MD5Init(&ctx);
	MD5Update(&ctx, zeros, 4);
	if (confounder) {
		SSVAL(header, 0, NL_SIGN_HMAC_MD5);
		SSVAL(header, 2, NL_SEAL_RC4);
		SSVAL(header, 4, 0xFFFF);
		SSVAL(header, 6, 0x0000);

		MD5Update(&ctx, header, 8);
		MD5Update(&ctx, confounder, 8);
	} else {
		SSVAL(header, 0, NL_SIGN_HMAC_MD5);
		SSVAL(header, 2, NL_SEAL_NONE);
		SSVAL(header, 4, 0xFFFF);
		SSVAL(header, 6, 0x0000);

		MD5Update(&ctx, header, 8);
	}
	MD5Update(&ctx, data, data_len);
	MD5Final(packet_digest, &ctx);

	hmac_md5(state->creds->session_key,
		 packet_digest, sizeof(packet_digest),
		 checksum);
}

NTSTATUS netsec_incoming_packet(struct schannel_state *state,
				TALLOC_CTX *mem_ctx,
				bool do_unseal,
				uint8_t *data, size_t length,
				const DATA_BLOB *sig)
{
	uint32_t min_sig_size = 0;
	uint8_t header[8];
	uint8_t checksum[32];
	uint32_t checksum_length = sizeof(checksum_length);
	uint8_t _confounder[8];
	uint8_t *confounder = NULL;
	uint32_t confounder_ofs = 0;
	uint8_t seq_num[8];
	int ret;

	netsec_offset_and_sizes(state,
				do_unseal,
				&min_sig_size,
				NULL,
				&checksum_length,
				&confounder_ofs);

	if (sig->length < min_sig_size) {
		return NT_STATUS_ACCESS_DENIED;
	}

	if (do_unseal) {
		confounder = _confounder;
		memcpy(confounder, sig->data+confounder_ofs, 8);
	} else {
		confounder = NULL;
	}

	RSIVAL(seq_num, 0, state->seq_num);
	SIVAL(seq_num, 4, state->initiator?0:0x80);

	if (do_unseal) {
		netsec_do_seal(state, seq_num,
			       confounder,
			       data, length);
	}

	netsec_do_sign(state, confounder,
		       data, length,
		       header, checksum);

	ret = memcmp(checksum, sig->data+16, checksum_length);
	if (ret != 0) {
		dump_data_pw("calc digest:", checksum, checksum_length);
		dump_data_pw("wire digest:", sig->data+16, checksum_length);
		return NT_STATUS_ACCESS_DENIED;
	}

	netsec_do_seq_num(state, checksum, checksum_length, seq_num);

	ret = memcmp(seq_num, sig->data+8, 8);
	if (ret != 0) {
		dump_data_pw("calc seq num:", seq_num, 8);
		dump_data_pw("wire seq num:", sig->data+8, 8);
		return NT_STATUS_ACCESS_DENIED;
	}

	return NT_STATUS_OK;
}

uint32_t netsec_outgoing_sig_size(struct schannel_state *state)
{
	uint32_t sig_size = 0;

	netsec_offset_and_sizes(state,
				true,
				NULL,
				&sig_size,
				NULL,
				NULL);

	return sig_size;
}

NTSTATUS netsec_outgoing_packet(struct schannel_state *state,
				TALLOC_CTX *mem_ctx,
				bool do_seal,
				uint8_t *data, size_t length,
				DATA_BLOB *sig)
{
	uint32_t min_sig_size = 0;
	uint32_t used_sig_size = 0;
	uint8_t header[8];
	uint8_t checksum[32];
	uint32_t checksum_length = sizeof(checksum_length);
	uint8_t _confounder[8];
	uint8_t *confounder = NULL;
	uint32_t confounder_ofs = 0;
	uint8_t seq_num[8];

	netsec_offset_and_sizes(state,
				do_seal,
				&min_sig_size,
				&used_sig_size,
				&checksum_length,
				&confounder_ofs);

	RSIVAL(seq_num, 0, state->seq_num);
	SIVAL(seq_num, 4, state->initiator?0x80:0);

	if (do_seal) {
		confounder = _confounder;
		generate_random_buffer(confounder, 8);
	} else {
		confounder = NULL;
	}

	netsec_do_sign(state, confounder,
		       data, length,
		       header, checksum);

	if (do_seal) {
		netsec_do_seal(state, seq_num,
			       confounder,
			       data, length);
	}

	netsec_do_seq_num(state, checksum, checksum_length, seq_num);

	(*sig) = data_blob_talloc_zero(mem_ctx, used_sig_size);

	memcpy(sig->data, header, 8);
	memcpy(sig->data+8, seq_num, 8);
	memcpy(sig->data+16, checksum, checksum_length);

	if (confounder) {
		memcpy(sig->data+confounder_ofs, confounder, 8);
	}

	dump_data_pw("signature:", sig->data+ 0, 8);
	dump_data_pw("seq_num  :", sig->data+ 8, 8);
	dump_data_pw("digest   :", sig->data+16, checksum_length);
	dump_data_pw("confound :", sig->data+confounder_ofs, 8);

	return NT_STATUS_OK;
}
