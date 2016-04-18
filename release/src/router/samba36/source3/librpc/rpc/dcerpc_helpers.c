/*
 *  DCERPC Helper routines
 *  Günther Deschner <gd@samba.org> 2010.
 *  Simo Sorce <idra@samba.org> 2010.
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
#include "librpc/rpc/dcerpc.h"
#include "librpc/gen_ndr/ndr_dcerpc.h"
#include "librpc/gen_ndr/ndr_schannel.h"
#include "../libcli/auth/schannel.h"
#include "../libcli/auth/spnego.h"
#include "../libcli/auth/ntlmssp.h"
#include "ntlmssp_wrap.h"
#include "librpc/crypto/gse.h"
#include "librpc/crypto/spnego.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_RPC_PARSE

/**
* @brief NDR Encodes a ncacn_packet
*
* @param mem_ctx	The memory context the blob will be allocated on
* @param ptype		The DCERPC packet type
* @param pfc_flags	The DCERPC PFC Falgs
* @param auth_length	The length of the trailing auth blob
* @param call_id	The call ID
* @param u		The payload of the packet
* @param blob [out]	The encoded blob if successful
*
* @return an NTSTATUS error code
*/
NTSTATUS dcerpc_push_ncacn_packet(TALLOC_CTX *mem_ctx,
				  enum dcerpc_pkt_type ptype,
				  uint8_t pfc_flags,
				  uint16_t auth_length,
				  uint32_t call_id,
				  union dcerpc_payload *u,
				  DATA_BLOB *blob)
{
	struct ncacn_packet r;
	enum ndr_err_code ndr_err;

	r.rpc_vers		= 5;
	r.rpc_vers_minor	= 0;
	r.ptype			= ptype;
	r.pfc_flags		= pfc_flags;
	r.drep[0]		= DCERPC_DREP_LE;
	r.drep[1]		= 0;
	r.drep[2]		= 0;
	r.drep[3]		= 0;
	r.auth_length		= auth_length;
	r.call_id		= call_id;
	r.u			= *u;

	ndr_err = ndr_push_struct_blob(blob, mem_ctx, &r,
		(ndr_push_flags_fn_t)ndr_push_ncacn_packet);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return ndr_map_error2ntstatus(ndr_err);
	}

	dcerpc_set_frag_length(blob, blob->length);


	if (DEBUGLEVEL >= 10) {
		/* set frag len for print function */
		r.frag_length = blob->length;
		NDR_PRINT_DEBUG(ncacn_packet, &r);
	}

	return NT_STATUS_OK;
}

/**
* @brief Decodes a ncacn_packet
*
* @param mem_ctx	The memory context on which to allocate the packet
*			elements
* @param blob		The blob of data to decode
* @param r		An empty ncacn_packet, must not be NULL
* @param bigendian	Whether the packet is bignedian encoded
*
* @return a NTSTATUS error code
*/
NTSTATUS dcerpc_pull_ncacn_packet(TALLOC_CTX *mem_ctx,
				  const DATA_BLOB *blob,
				  struct ncacn_packet *r,
				  bool bigendian)
{
	enum ndr_err_code ndr_err;
	struct ndr_pull *ndr;

	ndr = ndr_pull_init_blob(blob, mem_ctx);
	if (!ndr) {
		return NT_STATUS_NO_MEMORY;
	}
	if (bigendian) {
		ndr->flags |= LIBNDR_FLAG_BIGENDIAN;
	}

	if (CVAL(blob->data, DCERPC_PFC_OFFSET) & DCERPC_PFC_FLAG_OBJECT_UUID) {
		ndr->flags |= LIBNDR_FLAG_OBJECT_PRESENT;
	}

	ndr_err = ndr_pull_ncacn_packet(ndr, NDR_SCALARS|NDR_BUFFERS, r);

	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(ndr);
		return ndr_map_error2ntstatus(ndr_err);
	}
	talloc_free(ndr);

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_DEBUG(ncacn_packet, r);
	}

	if (r->frag_length != blob->length) {
		return NT_STATUS_RPC_PROTOCOL_ERROR;
	}

	return NT_STATUS_OK;
}

/**
* @brief NDR Encodes a NL_AUTH_MESSAGE
*
* @param mem_ctx	The memory context the blob will be allocated on
* @param r		The NL_AUTH_MESSAGE to encode
* @param blob [out]	The encoded blob if successful
*
* @return a NTSTATUS error code
*/
NTSTATUS dcerpc_push_schannel_bind(TALLOC_CTX *mem_ctx,
				   struct NL_AUTH_MESSAGE *r,
				   DATA_BLOB *blob)
{
	enum ndr_err_code ndr_err;

	ndr_err = ndr_push_struct_blob(blob, mem_ctx, r,
		(ndr_push_flags_fn_t)ndr_push_NL_AUTH_MESSAGE);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return ndr_map_error2ntstatus(ndr_err);
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_DEBUG(NL_AUTH_MESSAGE, r);
	}

	return NT_STATUS_OK;
}

/**
* @brief NDR Encodes a dcerpc_auth structure
*
* @param mem_ctx	  The memory context the blob will be allocated on
* @param auth_type	  The DCERPC Authentication Type
* @param auth_level	  The DCERPC Authentication Level
* @param auth_pad_length  The padding added to the packet this blob will be
*			   appended to.
* @param auth_context_id  The context id
* @param credentials	  The authentication credentials blob (signature)
* @param blob [out]	  The encoded blob if successful
*
* @return a NTSTATUS error code
*/
NTSTATUS dcerpc_push_dcerpc_auth(TALLOC_CTX *mem_ctx,
				 enum dcerpc_AuthType auth_type,
				 enum dcerpc_AuthLevel auth_level,
				 uint8_t auth_pad_length,
				 uint32_t auth_context_id,
				 const DATA_BLOB *credentials,
				 DATA_BLOB *blob)
{
	struct dcerpc_auth r;
	enum ndr_err_code ndr_err;

	r.auth_type		= auth_type;
	r.auth_level		= auth_level;
	r.auth_pad_length	= auth_pad_length;
	r.auth_reserved		= 0;
	r.auth_context_id	= auth_context_id;
	r.credentials		= *credentials;

	ndr_err = ndr_push_struct_blob(blob, mem_ctx, &r,
		(ndr_push_flags_fn_t)ndr_push_dcerpc_auth);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return ndr_map_error2ntstatus(ndr_err);
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_DEBUG(dcerpc_auth, &r);
	}

	return NT_STATUS_OK;
}

/**
* @brief Calculate how much data we can in a packet, including calculating
*	 auth token and pad lengths.
*
* @param auth		The pipe_auth_data structure for this pipe.
* @param header_len	The length of the packet header
* @param data_left	The data left in the send buffer
* @param max_xmit_frag	The max fragment size.
* @param pad_alignment	The NDR padding size.
* @param data_to_send	[out] The max data we will send in the pdu
* @param frag_len	[out] The total length of the fragment
* @param auth_len	[out] The length of the auth trailer
* @param pad_len	[out] The padding to be applied
*
* @return A NT Error status code.
*/
NTSTATUS dcerpc_guess_sizes(struct pipe_auth_data *auth,
			    size_t header_len, size_t data_left,
			    size_t max_xmit_frag, size_t pad_alignment,
			    size_t *data_to_send, size_t *frag_len,
			    size_t *auth_len, size_t *pad_len)
{
	size_t max_len;
	size_t mod_len;
	struct schannel_state *schannel_auth;
	struct spnego_context *spnego_ctx;
	struct gse_context *gse_ctx;
	enum spnego_mech auth_type;
	void *auth_ctx;
	bool seal = false;
	NTSTATUS status;

	/* no auth token cases first */
	switch (auth->auth_level) {
	case DCERPC_AUTH_LEVEL_NONE:
	case DCERPC_AUTH_LEVEL_CONNECT:
	case DCERPC_AUTH_LEVEL_PACKET:
		max_len = max_xmit_frag - header_len;
		*data_to_send = MIN(max_len, data_left);
		*pad_len = 0;
		*auth_len = 0;
		*frag_len = header_len + *data_to_send;
		return NT_STATUS_OK;

	case DCERPC_AUTH_LEVEL_PRIVACY:
		seal = true;
		break;

	case DCERPC_AUTH_LEVEL_INTEGRITY:
		break;

	default:
		return NT_STATUS_INVALID_PARAMETER;
	}


	/* Sign/seal case, calculate auth and pad lengths */

	max_len = max_xmit_frag - header_len - DCERPC_AUTH_TRAILER_LENGTH;

	/* Treat the same for all authenticated rpc requests. */
	switch (auth->auth_type) {
	case DCERPC_AUTH_TYPE_SPNEGO:
		spnego_ctx = talloc_get_type_abort(auth->auth_ctx,
						   struct spnego_context);
		status = spnego_get_negotiated_mech(spnego_ctx,
						    &auth_type, &auth_ctx);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}
		switch (auth_type) {
		case SPNEGO_NTLMSSP:
			*auth_len = NTLMSSP_SIG_SIZE;
			break;

		case SPNEGO_KRB5:
			gse_ctx = talloc_get_type_abort(auth_ctx,
							struct gse_context);
			if (!gse_ctx) {
				return NT_STATUS_INVALID_PARAMETER;
			}
			*auth_len = gse_get_signature_length(gse_ctx,
							     seal, max_len);
			break;

		default:
			return NT_STATUS_INVALID_PARAMETER;
		}
		break;

	case DCERPC_AUTH_TYPE_NTLMSSP:
		*auth_len = NTLMSSP_SIG_SIZE;
		break;

	case DCERPC_AUTH_TYPE_SCHANNEL:
		schannel_auth = talloc_get_type_abort(auth->auth_ctx,
						      struct schannel_state);
		*auth_len = netsec_outgoing_sig_size(schannel_auth);
		break;

	case DCERPC_AUTH_TYPE_KRB5:
		gse_ctx = talloc_get_type_abort(auth->auth_ctx,
						struct gse_context);
		*auth_len = gse_get_signature_length(gse_ctx,
						     seal, max_len);
		break;

	default:
		return NT_STATUS_INVALID_PARAMETER;
	}

	max_len -= *auth_len;

	*data_to_send = MIN(max_len, data_left);

	mod_len = (header_len + *data_to_send) % pad_alignment;
	if (mod_len) {
		*pad_len = pad_alignment - mod_len;
	} else {
		*pad_len = 0;
	}

	if (*data_to_send + *pad_len > max_len) {
		*data_to_send -= pad_alignment;
	}

	*frag_len = header_len + *data_to_send + *pad_len
			+ DCERPC_AUTH_TRAILER_LENGTH + *auth_len;

	return NT_STATUS_OK;
}

/*******************************************************************
 Create and add the NTLMSSP sign/seal auth data.
 ********************************************************************/

static NTSTATUS add_ntlmssp_auth_footer(struct auth_ntlmssp_state *auth_state,
					enum dcerpc_AuthLevel auth_level,
					DATA_BLOB *rpc_out)
{
	uint16_t data_and_pad_len = rpc_out->length
					- DCERPC_RESPONSE_LENGTH
					- DCERPC_AUTH_TRAILER_LENGTH;
	DATA_BLOB auth_blob;
	NTSTATUS status;

	if (!auth_state) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	switch (auth_level) {
	case DCERPC_AUTH_LEVEL_PRIVACY:
		/* Data portion is encrypted. */
		status = auth_ntlmssp_seal_packet(auth_state,
					     rpc_out->data,
					     rpc_out->data
						+ DCERPC_RESPONSE_LENGTH,
					     data_and_pad_len,
					     rpc_out->data,
					     rpc_out->length,
					     &auth_blob);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}
		break;

	case DCERPC_AUTH_LEVEL_INTEGRITY:
		/* Data is signed. */
		status = auth_ntlmssp_sign_packet(auth_state,
					     rpc_out->data,
					     rpc_out->data
						+ DCERPC_RESPONSE_LENGTH,
					     data_and_pad_len,
					     rpc_out->data,
					     rpc_out->length,
					     &auth_blob);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}
		break;

	default:
		/* Can't happen. */
		smb_panic("bad auth level");
		/* Notreached. */
		return NT_STATUS_INVALID_PARAMETER;
	}

	/* Finally attach the blob. */
	if (!data_blob_append(NULL, rpc_out,
				auth_blob.data, auth_blob.length)) {
		DEBUG(0, ("Failed to add %u bytes auth blob.\n",
			  (unsigned int)auth_blob.length));
		return NT_STATUS_NO_MEMORY;
	}
	data_blob_free(&auth_blob);

	return NT_STATUS_OK;
}

/*******************************************************************
 Check/unseal the NTLMSSP auth data. (Unseal in place).
 ********************************************************************/

static NTSTATUS get_ntlmssp_auth_footer(struct auth_ntlmssp_state *auth_state,
					enum dcerpc_AuthLevel auth_level,
					DATA_BLOB *data, DATA_BLOB *full_pkt,
					DATA_BLOB *auth_token)
{
	switch (auth_level) {
	case DCERPC_AUTH_LEVEL_PRIVACY:
		/* Data portion is encrypted. */
		return auth_ntlmssp_unseal_packet(auth_state,
						  data->data,
						  data->length,
						  full_pkt->data,
						  full_pkt->length,
						  auth_token);

	case DCERPC_AUTH_LEVEL_INTEGRITY:
		/* Data is signed. */
		return auth_ntlmssp_check_packet(auth_state,
						 data->data,
						 data->length,
						 full_pkt->data,
						 full_pkt->length,
						 auth_token);

	default:
		return NT_STATUS_INVALID_PARAMETER;
	}
}

/*******************************************************************
 Create and add the schannel sign/seal auth data.
 ********************************************************************/

static NTSTATUS add_schannel_auth_footer(struct schannel_state *sas,
					enum dcerpc_AuthLevel auth_level,
					DATA_BLOB *rpc_out)
{
	uint8_t *data_p = rpc_out->data + DCERPC_RESPONSE_LENGTH;
	size_t data_and_pad_len = rpc_out->length
					- DCERPC_RESPONSE_LENGTH
					- DCERPC_AUTH_TRAILER_LENGTH;
	DATA_BLOB auth_blob;
	NTSTATUS status;

	if (!sas) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	DEBUG(10,("add_schannel_auth_footer: SCHANNEL seq_num=%d\n",
			sas->seq_num));

	switch (auth_level) {
	case DCERPC_AUTH_LEVEL_PRIVACY:
		status = netsec_outgoing_packet(sas,
						rpc_out->data,
						true,
						data_p,
						data_and_pad_len,
						&auth_blob);
		break;
	case DCERPC_AUTH_LEVEL_INTEGRITY:
		status = netsec_outgoing_packet(sas,
						rpc_out->data,
						false,
						data_p,
						data_and_pad_len,
						&auth_blob);
		break;
	default:
		status = NT_STATUS_INTERNAL_ERROR;
		break;
	}

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1,("add_schannel_auth_footer: failed to process packet: %s\n",
			nt_errstr(status)));
		return status;
	}

	if (DEBUGLEVEL >= 10) {
		dump_NL_AUTH_SIGNATURE(talloc_tos(), &auth_blob);
	}

	/* Finally attach the blob. */
	if (!data_blob_append(NULL, rpc_out,
				auth_blob.data, auth_blob.length)) {
		return NT_STATUS_NO_MEMORY;
	}
	data_blob_free(&auth_blob);

	return NT_STATUS_OK;
}

/*******************************************************************
 Check/unseal the Schannel auth data. (Unseal in place).
 ********************************************************************/

static NTSTATUS get_schannel_auth_footer(TALLOC_CTX *mem_ctx,
					 struct schannel_state *auth_state,
					 enum dcerpc_AuthLevel auth_level,
					 DATA_BLOB *data, DATA_BLOB *full_pkt,
					 DATA_BLOB *auth_token)
{
	switch (auth_level) {
	case DCERPC_AUTH_LEVEL_PRIVACY:
		/* Data portion is encrypted. */
		return netsec_incoming_packet(auth_state,
						mem_ctx, true,
						data->data,
						data->length,
						auth_token);

	case DCERPC_AUTH_LEVEL_INTEGRITY:
		/* Data is signed. */
		return netsec_incoming_packet(auth_state,
						mem_ctx, false,
						data->data,
						data->length,
						auth_token);

	default:
		return NT_STATUS_INVALID_PARAMETER;
	}
}

/*******************************************************************
 Create and add the gssapi sign/seal auth data.
 ********************************************************************/

static NTSTATUS add_gssapi_auth_footer(struct gse_context *gse_ctx,
					enum dcerpc_AuthLevel auth_level,
					DATA_BLOB *rpc_out)
{
	DATA_BLOB data;
	DATA_BLOB auth_blob;
	NTSTATUS status;

	if (!gse_ctx) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	data.data = rpc_out->data + DCERPC_RESPONSE_LENGTH;
	data.length = rpc_out->length - DCERPC_RESPONSE_LENGTH
					- DCERPC_AUTH_TRAILER_LENGTH;

	switch (auth_level) {
	case DCERPC_AUTH_LEVEL_PRIVACY:
		status = gse_seal(talloc_tos(), gse_ctx, &data, &auth_blob);
		break;
	case DCERPC_AUTH_LEVEL_INTEGRITY:
		status = gse_sign(talloc_tos(), gse_ctx, &data, &auth_blob);
		break;
	default:
		status = NT_STATUS_INTERNAL_ERROR;
		break;
	}

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("Failed to process packet: %s\n",
			  nt_errstr(status)));
		return status;
	}

	/* Finally attach the blob. */
	if (!data_blob_append(NULL, rpc_out,
				auth_blob.data, auth_blob.length)) {
		return NT_STATUS_NO_MEMORY;
	}

	data_blob_free(&auth_blob);

	return NT_STATUS_OK;
}

/*******************************************************************
 Check/unseal the gssapi auth data. (Unseal in place).
 ********************************************************************/

static NTSTATUS get_gssapi_auth_footer(TALLOC_CTX *mem_ctx,
					struct gse_context *gse_ctx,
					enum dcerpc_AuthLevel auth_level,
					DATA_BLOB *data, DATA_BLOB *full_pkt,
					DATA_BLOB *auth_token)
{
	/* TODO: pass in full_pkt when
	 * DCERPC_PFC_FLAG_SUPPORT_HEADER_SIGN is set */
	switch (auth_level) {
	case DCERPC_AUTH_LEVEL_PRIVACY:
		/* Data portion is encrypted. */
		return gse_unseal(mem_ctx, gse_ctx,
				  data, auth_token);

	case DCERPC_AUTH_LEVEL_INTEGRITY:
		/* Data is signed. */
		return gse_sigcheck(mem_ctx, gse_ctx,
				    data, auth_token);
	default:
		return NT_STATUS_INVALID_PARAMETER;
	}
}

/*******************************************************************
 Create and add the spnego-negotiated sign/seal auth data.
 ********************************************************************/

static NTSTATUS add_spnego_auth_footer(struct spnego_context *spnego_ctx,
					enum dcerpc_AuthLevel auth_level,
					DATA_BLOB *rpc_out)
{
	DATA_BLOB auth_blob;
	DATA_BLOB rpc_data;
	NTSTATUS status;

	if (!spnego_ctx) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	rpc_data = data_blob_const(rpc_out->data
					+ DCERPC_RESPONSE_LENGTH,
				   rpc_out->length
					- DCERPC_RESPONSE_LENGTH
					- DCERPC_AUTH_TRAILER_LENGTH);

	switch (auth_level) {
	case DCERPC_AUTH_LEVEL_PRIVACY:
		/* Data portion is encrypted. */
		status = spnego_seal(rpc_out->data, spnego_ctx,
				     &rpc_data, rpc_out, &auth_blob);
		break;

		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}
		break;

	case DCERPC_AUTH_LEVEL_INTEGRITY:
		/* Data is signed. */
		status = spnego_sign(rpc_out->data, spnego_ctx,
				     &rpc_data, rpc_out, &auth_blob);
		break;

		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}
		break;

	default:
		/* Can't happen. */
		smb_panic("bad auth level");
		/* Notreached. */
		return NT_STATUS_INVALID_PARAMETER;
	}

	/* Finally attach the blob. */
	if (!data_blob_append(NULL, rpc_out,
				auth_blob.data, auth_blob.length)) {
		DEBUG(0, ("Failed to add %u bytes auth blob.\n",
			  (unsigned int)auth_blob.length));
		return NT_STATUS_NO_MEMORY;
	}
	data_blob_free(&auth_blob);

	return NT_STATUS_OK;
}

static NTSTATUS get_spnego_auth_footer(TALLOC_CTX *mem_ctx,
					struct spnego_context *sp_ctx,
					enum dcerpc_AuthLevel auth_level,
					DATA_BLOB *data, DATA_BLOB *full_pkt,
					DATA_BLOB *auth_token)
{
	switch (auth_level) {
	case DCERPC_AUTH_LEVEL_PRIVACY:
		/* Data portion is encrypted. */
		return spnego_unseal(mem_ctx, sp_ctx,
				     data, full_pkt, auth_token);

	case DCERPC_AUTH_LEVEL_INTEGRITY:
		/* Data is signed. */
		return spnego_sigcheck(mem_ctx, sp_ctx,
				       data, full_pkt, auth_token);

	default:
		return NT_STATUS_INVALID_PARAMETER;
	}
}

/**
* @brief   Append an auth footer according to what is the current mechanism
*
* @param auth		The pipe_auth_data associated with the connection
* @param pad_len	The padding used in the packet
* @param rpc_out	Packet blob up to and including the auth header
*
* @return A NTSTATUS error code.
*/
NTSTATUS dcerpc_add_auth_footer(struct pipe_auth_data *auth,
				size_t pad_len, DATA_BLOB *rpc_out)
{
	struct schannel_state *schannel_auth;
	struct auth_ntlmssp_state *ntlmssp_ctx;
	struct spnego_context *spnego_ctx;
	struct gse_context *gse_ctx;
	char pad[CLIENT_NDR_PADDING_SIZE] = { 0, };
	DATA_BLOB auth_info;
	DATA_BLOB auth_blob;
	NTSTATUS status;

	if (auth->auth_type == DCERPC_AUTH_TYPE_NONE ||
	    auth->auth_type == DCERPC_AUTH_TYPE_NCALRPC_AS_SYSTEM) {
		return NT_STATUS_OK;
	}

	if (pad_len) {
		/* Copy the sign/seal padding data. */
		if (!data_blob_append(NULL, rpc_out, pad, pad_len)) {
			return NT_STATUS_NO_MEMORY;
		}
	}

	/* marshall the dcerpc_auth with an actually empty auth_blob.
	 * This is needed because the ntmlssp signature includes the
	 * auth header. We will append the actual blob later. */
	auth_blob = data_blob_null;
	status = dcerpc_push_dcerpc_auth(rpc_out->data,
					 auth->auth_type,
					 auth->auth_level,
					 pad_len,
					 auth->auth_context_id,
					 &auth_blob,
					 &auth_info);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	/* append the header */
	if (!data_blob_append(NULL, rpc_out,
				auth_info.data, auth_info.length)) {
		DEBUG(0, ("Failed to add %u bytes auth blob.\n",
			  (unsigned int)auth_info.length));
		return NT_STATUS_NO_MEMORY;
	}
	data_blob_free(&auth_info);

	/* Generate any auth sign/seal and add the auth footer. */
	switch (auth->auth_type) {
	case DCERPC_AUTH_TYPE_NONE:
	case DCERPC_AUTH_TYPE_NCALRPC_AS_SYSTEM:
		status = NT_STATUS_OK;
		break;
	case DCERPC_AUTH_TYPE_SPNEGO:
		spnego_ctx = talloc_get_type_abort(auth->auth_ctx,
						   struct spnego_context);
		status = add_spnego_auth_footer(spnego_ctx,
						auth->auth_level, rpc_out);
		break;
	case DCERPC_AUTH_TYPE_NTLMSSP:
		ntlmssp_ctx = talloc_get_type_abort(auth->auth_ctx,
						struct auth_ntlmssp_state);
		status = add_ntlmssp_auth_footer(ntlmssp_ctx,
						 auth->auth_level,
						 rpc_out);
		break;
	case DCERPC_AUTH_TYPE_SCHANNEL:
		schannel_auth = talloc_get_type_abort(auth->auth_ctx,
						      struct schannel_state);
		status = add_schannel_auth_footer(schannel_auth,
						  auth->auth_level,
						  rpc_out);
		break;
	case DCERPC_AUTH_TYPE_KRB5:
		gse_ctx = talloc_get_type_abort(auth->auth_ctx,
						struct gse_context);
		status = add_gssapi_auth_footer(gse_ctx,
						auth->auth_level,
						rpc_out);
		break;
	default:
		status = NT_STATUS_INVALID_PARAMETER;
		break;
	}

	return status;
}

/**
* @brief Check authentication for request/response packets
*
* @param auth		The auth data for the connection
* @param pkt		The actual ncacn_packet
* @param pkt_trailer [in][out]	The stub_and_verifier part of the packet,
* 			the auth_trailer and padding will be removed.
* @param header_size	The header size
* @param raw_pkt	The whole raw packet data blob
*
* @return A NTSTATUS error code
*/
NTSTATUS dcerpc_check_auth(struct pipe_auth_data *auth,
			   struct ncacn_packet *pkt,
			   DATA_BLOB *pkt_trailer,
			   uint8_t header_size,
			   DATA_BLOB *raw_pkt)
{
	struct schannel_state *schannel_auth;
	struct auth_ntlmssp_state *ntlmssp_ctx;
	struct spnego_context *spnego_ctx;
	struct gse_context *gse_ctx;
	NTSTATUS status;
	struct dcerpc_auth auth_info;
	uint32_t auth_length;
	DATA_BLOB full_pkt;
	DATA_BLOB data;

	/*
	 * These check should be done in the caller.
	 */
	SMB_ASSERT(raw_pkt->length == pkt->frag_length);
	SMB_ASSERT(header_size <= pkt->frag_length);
	SMB_ASSERT(pkt_trailer->length < pkt->frag_length);
	SMB_ASSERT((pkt_trailer->length + header_size) <= pkt->frag_length);

	switch (auth->auth_level) {
	case DCERPC_AUTH_LEVEL_PRIVACY:
		DEBUG(10, ("Requested Privacy.\n"));
		break;

	case DCERPC_AUTH_LEVEL_INTEGRITY:
		DEBUG(10, ("Requested Integrity.\n"));
		break;

	case DCERPC_AUTH_LEVEL_CONNECT:
		if (pkt->auth_length != 0) {
			break;
		}
		return NT_STATUS_OK;

	case DCERPC_AUTH_LEVEL_NONE:
		if (pkt->auth_length != 0) {
			DEBUG(3, ("Got non-zero auth len on non "
				  "authenticated connection!\n"));
			return NT_STATUS_INVALID_PARAMETER;
		}
		return NT_STATUS_OK;

	default:
		DEBUG(3, ("Unimplemented Auth Level %d",
			  auth->auth_level));
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (pkt->auth_length == 0) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	status = dcerpc_pull_auth_trailer(pkt, pkt, pkt_trailer,
					  &auth_info, &auth_length, false);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	if (auth_info.auth_type != auth->auth_type) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (auth_info.auth_level != auth->auth_level) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (auth_info.auth_context_id != auth->auth_context_id) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	pkt_trailer->length -= auth_length;
	data = data_blob_const(raw_pkt->data + header_size,
			       pkt_trailer->length);
	full_pkt = data_blob_const(raw_pkt->data, raw_pkt->length);
	full_pkt.length -= auth_info.credentials.length;

	switch (auth->auth_type) {
	case DCERPC_AUTH_TYPE_NONE:
	case DCERPC_AUTH_TYPE_NCALRPC_AS_SYSTEM:
		return NT_STATUS_OK;

	case DCERPC_AUTH_TYPE_SPNEGO:
		spnego_ctx = talloc_get_type_abort(auth->auth_ctx,
						   struct spnego_context);
		status = get_spnego_auth_footer(pkt, spnego_ctx,
						auth->auth_level,
						&data, &full_pkt,
						&auth_info.credentials);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}
		break;

	case DCERPC_AUTH_TYPE_NTLMSSP:

		DEBUG(10, ("NTLMSSP auth\n"));

		ntlmssp_ctx = talloc_get_type_abort(auth->auth_ctx,
						struct auth_ntlmssp_state);
		status = get_ntlmssp_auth_footer(ntlmssp_ctx,
						 auth->auth_level,
						 &data, &full_pkt,
						 &auth_info.credentials);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}
		break;

	case DCERPC_AUTH_TYPE_SCHANNEL:

		DEBUG(10, ("SCHANNEL auth\n"));

		schannel_auth = talloc_get_type_abort(auth->auth_ctx,
						      struct schannel_state);
		status = get_schannel_auth_footer(pkt, schannel_auth,
						  auth->auth_level,
						  &data, &full_pkt,
						  &auth_info.credentials);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}
		break;

	case DCERPC_AUTH_TYPE_KRB5:

		DEBUG(10, ("KRB5 auth\n"));

		gse_ctx = talloc_get_type_abort(auth->auth_ctx,
						struct gse_context);
		status = get_gssapi_auth_footer(pkt, gse_ctx,
						auth->auth_level,
						&data, &full_pkt,
						&auth_info.credentials);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}
		break;

	default:
		DEBUG(0, ("process_request_pdu: "
			  "unknown auth type %u set.\n",
			  (unsigned int)auth->auth_type));
		return NT_STATUS_INVALID_PARAMETER;
	}

	/* TODO: remove later
	 * this is still needed because in the server code the
	 * pkt_trailer actually has a copy of the raw data, and they
	 * are still both used in later calls */
	if (auth->auth_level == DCERPC_AUTH_LEVEL_PRIVACY) {
		if (pkt_trailer->length != data.length) {
			return NT_STATUS_INVALID_PARAMETER;
		}
		memcpy(pkt_trailer->data, data.data, data.length);
	}

	pkt_trailer->length -= auth_info.auth_pad_length;
	data_blob_free(&auth_info.credentials);
	return NT_STATUS_OK;
}

