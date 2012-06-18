/* 
   Unix SMB/CIFS implementation.
   SMB Transport encryption (sealing) code.
   Copyright (C) Jeremy Allison 2007.
   
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

/******************************************************************************
 Pull out the encryption context for this packet. 0 means global context.
******************************************************************************/

NTSTATUS get_enc_ctx_num(const uint8_t *buf, uint16 *p_enc_ctx_num)
{
	if (smb_len(buf) < 8) {
		return NT_STATUS_INVALID_BUFFER_SIZE;
	}

	if (buf[4] == 0xFF) {
		if (buf[5] == 'S' && buf [6] == 'M' && buf[7] == 'B') {
			/* Not an encrypted buffer. */
			return NT_STATUS_NOT_FOUND;
		}
		if (buf[5] == 'E') {
			*p_enc_ctx_num = SVAL(buf,6);
			return NT_STATUS_OK;
		}
	}
	return NT_STATUS_INVALID_NETWORK_RESPONSE;
}

/******************************************************************************
 Generic code for client and server.
 Is encryption turned on ?
******************************************************************************/

bool common_encryption_on(struct smb_trans_enc_state *es)
{
	return ((es != NULL) && es->enc_on);
}

/******************************************************************************
 Generic code for client and server.
 NTLM decrypt an incoming buffer.
 Abartlett tells me that SSPI puts the signature first before the encrypted
 output, so cope with the same for compatibility.
******************************************************************************/

NTSTATUS common_ntlm_decrypt_buffer(NTLMSSP_STATE *ntlmssp_state, char *buf)
{
	NTSTATUS status;
	size_t buf_len = smb_len(buf) + 4; /* Don't forget the 4 length bytes. */
	size_t data_len;
	char *inbuf;
	DATA_BLOB sig;

	if (buf_len < 8 + NTLMSSP_SIG_SIZE) {
		return NT_STATUS_BUFFER_TOO_SMALL;
	}

	inbuf = (char *)smb_xmemdup(buf, buf_len);

	/* Adjust for the signature. */
	data_len = buf_len - 8 - NTLMSSP_SIG_SIZE;

	/* Point at the signature. */
	sig = data_blob_const(inbuf+8, NTLMSSP_SIG_SIZE);

	status = ntlmssp_unseal_packet(ntlmssp_state,
		(unsigned char *)inbuf + 8 + NTLMSSP_SIG_SIZE, /* 4 byte len + 0xFF 'E' <enc> <ctx> */
		data_len,
		(unsigned char *)inbuf + 8 + NTLMSSP_SIG_SIZE,
		data_len,
		&sig);

	if (!NT_STATUS_IS_OK(status)) {
		SAFE_FREE(inbuf);
		return status;
	}

	memcpy(buf + 8, inbuf + 8 + NTLMSSP_SIG_SIZE, data_len);

	/* Reset the length and overwrite the header. */
	smb_setlen(buf,data_len + 4);

	SAFE_FREE(inbuf);
	return NT_STATUS_OK;
}

/******************************************************************************
 Generic code for client and server.
 NTLM encrypt an outgoing buffer. Return the encrypted pointer in ppbuf_out.
 Abartlett tells me that SSPI puts the signature first before the encrypted
 output, so do the same for compatibility.
******************************************************************************/

NTSTATUS common_ntlm_encrypt_buffer(NTLMSSP_STATE *ntlmssp_state,
				uint16 enc_ctx_num,
				char *buf,
				char **ppbuf_out)
{
	NTSTATUS status;
	char *buf_out;
	size_t data_len = smb_len(buf) - 4; /* Ignore the 0xFF SMB bytes. */
	DATA_BLOB sig;

	*ppbuf_out = NULL;

	if (data_len == 0) {
		return NT_STATUS_BUFFER_TOO_SMALL;
	}

	/* 
	 * We know smb_len can't return a value > 128k, so no int overflow
	 * check needed.
	 */

	buf_out = SMB_XMALLOC_ARRAY(char, 8 + NTLMSSP_SIG_SIZE + data_len);

	/* Copy the data from the original buffer. */

	memcpy(buf_out + 8 + NTLMSSP_SIG_SIZE, buf + 8, data_len);

	smb_set_enclen(buf_out, smb_len(buf) + NTLMSSP_SIG_SIZE, enc_ctx_num);

	ZERO_STRUCT(sig);

	status = ntlmssp_seal_packet(ntlmssp_state,
		(unsigned char *)buf_out + 8 + NTLMSSP_SIG_SIZE, /* 4 byte len + 0xFF 'S' <enc> <ctx> */
		data_len,
		(unsigned char *)buf_out + 8 + NTLMSSP_SIG_SIZE,
		data_len,
		&sig);

	if (!NT_STATUS_IS_OK(status)) {
		data_blob_free(&sig);
		SAFE_FREE(buf_out);
		return status;
	}

	/* First 16 data bytes are signature for SSPI compatibility. */
	memcpy(buf_out + 8, sig.data, NTLMSSP_SIG_SIZE);
	data_blob_free(&sig);
	*ppbuf_out = buf_out;
	return NT_STATUS_OK;
}

/******************************************************************************
 Generic code for client and server.
 gss-api decrypt an incoming buffer. We insist that the size of the
 unwrapped buffer must be smaller or identical to the incoming buffer.
******************************************************************************/

#if defined(HAVE_GSSAPI) && defined(HAVE_KRB5)
static NTSTATUS common_gss_decrypt_buffer(struct smb_tran_enc_state_gss *gss_state, char *buf)
{
	gss_ctx_id_t gss_ctx = gss_state->gss_ctx;
	OM_uint32 ret = 0;
	OM_uint32 minor = 0;
	int flags_got = 0;
	gss_buffer_desc in_buf, out_buf;
	size_t buf_len = smb_len(buf) + 4; /* Don't forget the 4 length bytes. */

	if (buf_len < 8) {
		return NT_STATUS_BUFFER_TOO_SMALL;
	}

	in_buf.value = buf + 8;
	in_buf.length = buf_len - 8;

	ret = gss_unwrap(&minor,
			gss_ctx,
			&in_buf,
			&out_buf,
			&flags_got,		/* did we get sign+seal ? */
			(gss_qop_t *) NULL);	

	if (ret != GSS_S_COMPLETE) {
		ADS_STATUS adss = ADS_ERROR_GSS(ret, minor);
		DEBUG(0,("common_gss_encrypt_buffer: gss_unwrap failed. Error %s\n",
			ads_errstr(adss) ));
		return map_nt_error_from_gss(ret, minor);
	}

	if (out_buf.length > in_buf.length) {
		DEBUG(0,("common_gss_encrypt_buffer: gss_unwrap size (%u) too large (%u) !\n",
			(unsigned int)out_buf.length,
			(unsigned int)in_buf.length ));
		gss_release_buffer(&minor, &out_buf);
		return NT_STATUS_INVALID_PARAMETER;
	}

	memcpy(buf + 8, out_buf.value, out_buf.length);
	/* Reset the length and overwrite the header. */
	smb_setlen(buf, out_buf.length + 4);

	gss_release_buffer(&minor, &out_buf);
	return NT_STATUS_OK;
}

/******************************************************************************
 Generic code for client and server.
 gss-api encrypt an outgoing buffer. Return the alloced encrypted pointer in buf_out.
******************************************************************************/

static NTSTATUS common_gss_encrypt_buffer(struct smb_tran_enc_state_gss *gss_state,
					uint16 enc_ctx_num,
		 			char *buf,
					char **ppbuf_out)
{
	gss_ctx_id_t gss_ctx = gss_state->gss_ctx;
	OM_uint32 ret = 0;
	OM_uint32 minor = 0;
	int flags_got = 0;
	gss_buffer_desc in_buf, out_buf;
	size_t buf_len = smb_len(buf) + 4; /* Don't forget the 4 length bytes. */

	*ppbuf_out = NULL;

	if (buf_len < 8) {
		return NT_STATUS_BUFFER_TOO_SMALL;
	}

	in_buf.value = buf + 8;
	in_buf.length = buf_len - 8;

	ret = gss_wrap(&minor,
			gss_ctx,
			true,			/* we want sign+seal. */
			GSS_C_QOP_DEFAULT,
			&in_buf,
			&flags_got,		/* did we get sign+seal ? */
			&out_buf);

	if (ret != GSS_S_COMPLETE) {
		ADS_STATUS adss = ADS_ERROR_GSS(ret, minor);
		DEBUG(0,("common_gss_encrypt_buffer: gss_wrap failed. Error %s\n",
			ads_errstr(adss) ));
		return map_nt_error_from_gss(ret, minor);
	}

	if (!flags_got) {
		/* Sign+seal not supported. */
		gss_release_buffer(&minor, &out_buf);
		return NT_STATUS_NOT_SUPPORTED;
	}

	/* Ya see - this is why I *hate* gss-api. I don't 
	 * want to have to malloc another buffer of the
	 * same size + 8 bytes just to get a continuous
	 * header + buffer, but gss won't let me pass in
	 * a pre-allocated buffer. Bastards (and you know
	 * who you are....). I might fix this by
	 * going to "encrypt_and_send" passing in a file
	 * descriptor and doing scatter-gather write with
	 * TCP cork on Linux. But I shouldn't have to
	 * bother :-*(. JRA.
	 */

	*ppbuf_out = (char *)SMB_MALLOC(out_buf.length + 8); /* We know this can't wrap. */
	if (!*ppbuf_out) {
		gss_release_buffer(&minor, &out_buf);
		return NT_STATUS_NO_MEMORY;
	}

	memcpy(*ppbuf_out+8, out_buf.value, out_buf.length);
	smb_set_enclen(*ppbuf_out, out_buf.length + 4, enc_ctx_num);

	gss_release_buffer(&minor, &out_buf);
	return NT_STATUS_OK;
}
#endif

/******************************************************************************
 Generic code for client and server.
 Encrypt an outgoing buffer. Return the alloced encrypted pointer in buf_out.
******************************************************************************/

NTSTATUS common_encrypt_buffer(struct smb_trans_enc_state *es, char *buffer, char **buf_out)
{
	if (!common_encryption_on(es)) {
		/* Not encrypting. */
		*buf_out = buffer;
		return NT_STATUS_OK;
	}

	switch (es->smb_enc_type) {
		case SMB_TRANS_ENC_NTLM:
			return common_ntlm_encrypt_buffer(es->s.ntlmssp_state, es->enc_ctx_num, buffer, buf_out);
#if defined(HAVE_GSSAPI) && defined(HAVE_KRB5)
		case SMB_TRANS_ENC_GSS:
			return common_gss_encrypt_buffer(es->s.gss_state, es->enc_ctx_num, buffer, buf_out);
#endif
		default:
			return NT_STATUS_NOT_SUPPORTED;
	}
}

/******************************************************************************
 Generic code for client and server.
 Decrypt an incoming SMB buffer. Replaces the data within it.
 New data must be less than or equal to the current length.
******************************************************************************/

NTSTATUS common_decrypt_buffer(struct smb_trans_enc_state *es, char *buf)
{
	if (!common_encryption_on(es)) {
		/* Not decrypting. */
		return NT_STATUS_OK;
	}

	switch (es->smb_enc_type) {
		case SMB_TRANS_ENC_NTLM:
			return common_ntlm_decrypt_buffer(es->s.ntlmssp_state, buf);
#if defined(HAVE_GSSAPI) && defined(HAVE_KRB5)
		case SMB_TRANS_ENC_GSS:
			return common_gss_decrypt_buffer(es->s.gss_state, buf);
#endif
		default:
			return NT_STATUS_NOT_SUPPORTED;
	}
}

#if defined(HAVE_GSSAPI) && defined(HAVE_KRB5)
/******************************************************************************
 Shutdown a gss encryption state.
******************************************************************************/

static void common_free_gss_state(struct smb_tran_enc_state_gss **pp_gss_state)
{
	OM_uint32 minor = 0;
	struct smb_tran_enc_state_gss *gss_state = *pp_gss_state;

	if (gss_state->creds != GSS_C_NO_CREDENTIAL) {
		gss_release_cred(&minor, &gss_state->creds);
	}
	if (gss_state->gss_ctx != GSS_C_NO_CONTEXT) {
		gss_delete_sec_context(&minor, &gss_state->gss_ctx, NULL);
	}
	SAFE_FREE(*pp_gss_state);
}
#endif

/******************************************************************************
 Shutdown an encryption state.
******************************************************************************/

void common_free_encryption_state(struct smb_trans_enc_state **pp_es)
{
	struct smb_trans_enc_state *es = *pp_es;

	if (es == NULL) {
		return;
	}

	if (es->smb_enc_type == SMB_TRANS_ENC_NTLM) {
		if (es->s.ntlmssp_state) {
			ntlmssp_end(&es->s.ntlmssp_state);
		}
	}
#if defined(HAVE_GSSAPI) && defined(HAVE_KRB5)
	if (es->smb_enc_type == SMB_TRANS_ENC_GSS) {
		/* Free the gss context handle. */
		if (es->s.gss_state) {
			common_free_gss_state(&es->s.gss_state);
		}
	}
#endif
	SAFE_FREE(es);
	*pp_es = NULL;
}

/******************************************************************************
 Free an encryption-allocated buffer.
******************************************************************************/

void common_free_enc_buffer(struct smb_trans_enc_state *es, char *buf)
{
	uint16_t enc_ctx_num;

	if (!common_encryption_on(es)) {
		return;
	}

	if (!NT_STATUS_IS_OK(get_enc_ctx_num((const uint8_t *)buf,
			&enc_ctx_num))) {
		return;
	}

	if (es->smb_enc_type == SMB_TRANS_ENC_NTLM) {
		SAFE_FREE(buf);
		return;
	}

#if defined(HAVE_GSSAPI) && defined(HAVE_KRB5)
	if (es->smb_enc_type == SMB_TRANS_ENC_GSS) {
		OM_uint32 min;
		gss_buffer_desc rel_buf;
		rel_buf.value = buf;
		rel_buf.length = smb_len(buf) + 4;
		gss_release_buffer(&min, &rel_buf);
	}
#endif
}

/******************************************************************************
 Client side encryption.
******************************************************************************/

/******************************************************************************
 Is client encryption on ?
******************************************************************************/

bool cli_encryption_on(struct cli_state *cli)
{
	/* If we supported multiple encrytion contexts
	 * here we'd look up based on tid.
	 */
	return common_encryption_on(cli->trans_enc_state);
}

/******************************************************************************
 Shutdown a client encryption state.
******************************************************************************/

void cli_free_encryption_context(struct cli_state *cli)
{
	common_free_encryption_state(&cli->trans_enc_state);
}

/******************************************************************************
 Free an encryption-allocated buffer.
******************************************************************************/

void cli_free_enc_buffer(struct cli_state *cli, char *buf)
{
	/* We know this is an smb buffer, and we
	 * didn't malloc, only copy, for a keepalive,
	 * so ignore non-session messages. */

	if(CVAL(buf,0)) {
		return;
	}

	/* If we supported multiple encrytion contexts
	 * here we'd look up based on tid.
	 */
	common_free_enc_buffer(cli->trans_enc_state, buf);
}

/******************************************************************************
 Decrypt an incoming buffer.
******************************************************************************/

NTSTATUS cli_decrypt_message(struct cli_state *cli)
{
	NTSTATUS status;
	uint16 enc_ctx_num;

	/* Ignore non-session messages. */
	if(CVAL(cli->inbuf,0)) {
		return NT_STATUS_OK;
	}

	status = get_enc_ctx_num((const uint8_t *)cli->inbuf, &enc_ctx_num);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	if (enc_ctx_num != cli->trans_enc_state->enc_ctx_num) {
		return NT_STATUS_INVALID_HANDLE;
	}

	return common_decrypt_buffer(cli->trans_enc_state, cli->inbuf);
}

/******************************************************************************
 Encrypt an outgoing buffer. Return the encrypted pointer in buf_out.
******************************************************************************/

NTSTATUS cli_encrypt_message(struct cli_state *cli, char *buf, char **buf_out)
{
	/* Ignore non-session messages. */
	if (CVAL(buf,0)) {
		return NT_STATUS_OK;
	}

	/* If we supported multiple encrytion contexts
	 * here we'd look up based on tid.
	 */
	return common_encrypt_buffer(cli->trans_enc_state, buf, buf_out);
}
