/* 
   Unix SMB/CIFS implementation.

   server side dcerpc authentication code

   Copyright (C) Andrew Tridgell 2003
   Copyright (C) Stefan (metze) Metzmacher 2004

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
#include "rpc_server/dcerpc_server.h"
#include "rpc_server/dcerpc_server_proto.h"
#include "librpc/rpc/dcerpc_proto.h"
#include "librpc/gen_ndr/ndr_dcerpc.h"
#include "auth/credentials/credentials.h"
#include "auth/gensec/gensec.h"
#include "auth/auth.h"
#include "param/param.h"

/*
  parse any auth information from a dcerpc bind request
  return false if we can't handle the auth request for some 
  reason (in which case we send a bind_nak)
*/
bool dcesrv_auth_bind(struct dcesrv_call_state *call)
{
	struct cli_credentials *server_credentials;
	struct ncacn_packet *pkt = &call->pkt;
	struct dcesrv_connection *dce_conn = call->conn;
	struct dcesrv_auth *auth = &dce_conn->auth_state;
	NTSTATUS status;
	enum ndr_err_code ndr_err;

	if (pkt->u.bind.auth_info.length == 0) {
		dce_conn->auth_state.auth_info = NULL;
		return true;
	}

	dce_conn->auth_state.auth_info = talloc(dce_conn, struct dcerpc_auth);
	if (!dce_conn->auth_state.auth_info) {
		return false;
	}

	ndr_err = ndr_pull_struct_blob(&pkt->u.bind.auth_info,
				       call, NULL,
				       dce_conn->auth_state.auth_info,
				       (ndr_pull_flags_fn_t)ndr_pull_dcerpc_auth);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return false;
	}

	server_credentials 
		= cli_credentials_init(call);
	if (!server_credentials) {
		DEBUG(1, ("Failed to init server credentials\n"));
		return false;
	}
	
	cli_credentials_set_conf(server_credentials, call->conn->dce_ctx->lp_ctx);
	status = cli_credentials_set_machine_account(server_credentials, call->conn->dce_ctx->lp_ctx);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(10, ("Failed to obtain server credentials, perhaps a standalone server?: %s\n", nt_errstr(status)));
		talloc_free(server_credentials);
		server_credentials = NULL;
	}

	status = samba_server_gensec_start(dce_conn, call->event_ctx, 
					   call->msg_ctx,
					   call->conn->dce_ctx->lp_ctx, 
					   server_credentials,
					   NULL,
					   &auth->gensec_security);

	status = gensec_start_mech_by_authtype(auth->gensec_security, auth->auth_info->auth_type, 
					       auth->auth_info->auth_level);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("Failed to start GENSEC mechanism for DCERPC server: auth_type=%d, auth_level=%d: %s\n", 
			  (int)auth->auth_info->auth_type,
			  (int)auth->auth_info->auth_level,
			  nt_errstr(status)));
		return false;
	}

	if (call->conn->state_flags & DCESRV_CALL_STATE_FLAG_HEADER_SIGNING) {
		gensec_want_feature(auth->gensec_security, GENSEC_FEATURE_SIGN_PKT_HEADER);
	}

	return true;
}

/*
  add any auth information needed in a bind ack, and process the authentication
  information found in the bind.
*/
NTSTATUS dcesrv_auth_bind_ack(struct dcesrv_call_state *call, struct ncacn_packet *pkt)
{
	struct dcesrv_connection *dce_conn = call->conn;
	NTSTATUS status;

	if (!call->conn->auth_state.gensec_security) {
		return NT_STATUS_OK;
	}

	status = gensec_update(dce_conn->auth_state.gensec_security,
			       call,
			       dce_conn->auth_state.auth_info->credentials, 
			       &dce_conn->auth_state.auth_info->credentials);
	
	if (NT_STATUS_IS_OK(status)) {
		status = gensec_session_info(dce_conn->auth_state.gensec_security,
					     &dce_conn->auth_state.session_info);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(1, ("Failed to establish session_info: %s\n", nt_errstr(status)));
			return status;
		}

		if (dce_conn->state_flags & DCESRV_CALL_STATE_FLAG_HEADER_SIGNING) {
			gensec_want_feature(dce_conn->auth_state.gensec_security,
					    GENSEC_FEATURE_SIGN_PKT_HEADER);
		}

		/* Now that we are authenticated, go back to the generic session key... */
		dce_conn->auth_state.session_key = dcesrv_generic_session_key;
		return NT_STATUS_OK;
	} else if (NT_STATUS_EQUAL(status, NT_STATUS_MORE_PROCESSING_REQUIRED)) {
		dce_conn->auth_state.auth_info->auth_pad_length = 0;
		dce_conn->auth_state.auth_info->auth_reserved = 0;
		return NT_STATUS_OK;
	} else {
		DEBUG(2, ("Failed to start dcesrv auth negotiate: %s\n", nt_errstr(status)));
		return status;
	}
}


/*
  process the final stage of a auth request
*/
bool dcesrv_auth_auth3(struct dcesrv_call_state *call)
{
	struct ncacn_packet *pkt = &call->pkt;
	struct dcesrv_connection *dce_conn = call->conn;
	NTSTATUS status;
	enum ndr_err_code ndr_err;

	/* We can't work without an existing gensec state, and an new blob to feed it */
	if (!dce_conn->auth_state.auth_info ||
	    !dce_conn->auth_state.gensec_security ||
	    pkt->u.auth3.auth_info.length == 0) {
		return false;
	}

	ndr_err = ndr_pull_struct_blob(&pkt->u.auth3.auth_info,
				       call, NULL,
				       dce_conn->auth_state.auth_info,
				       (ndr_pull_flags_fn_t)ndr_pull_dcerpc_auth);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return false;
	}

	/* Pass the extra data we got from the client down to gensec for processing */
	status = gensec_update(dce_conn->auth_state.gensec_security,
			       call,
			       dce_conn->auth_state.auth_info->credentials, 
			       &dce_conn->auth_state.auth_info->credentials);
	if (NT_STATUS_IS_OK(status)) {
		status = gensec_session_info(dce_conn->auth_state.gensec_security,
					     &dce_conn->auth_state.session_info);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(1, ("Failed to establish session_info: %s\n", nt_errstr(status)));
			return false;
		}
		/* Now that we are authenticated, go back to the generic session key... */
		dce_conn->auth_state.session_key = dcesrv_generic_session_key;
		return true;
	} else {
		DEBUG(4, ("dcesrv_auth_auth3: failed to authenticate: %s\n", 
			  nt_errstr(status)));
		return false;
	}

	return true;
}

/*
  parse any auth information from a dcerpc alter request
  return false if we can't handle the auth request for some 
  reason (in which case we send a bind_nak (is this true for here?))
*/
bool dcesrv_auth_alter(struct dcesrv_call_state *call)
{
	struct ncacn_packet *pkt = &call->pkt;
	struct dcesrv_connection *dce_conn = call->conn;
	enum ndr_err_code ndr_err;

	/* on a pure interface change there is no auth blob */
	if (pkt->u.alter.auth_info.length == 0) {
		return true;
	}

	/* We can't work without an existing gensec state */
	if (!dce_conn->auth_state.gensec_security) {
		return false;
	}

	dce_conn->auth_state.auth_info = talloc(dce_conn, struct dcerpc_auth);
	if (!dce_conn->auth_state.auth_info) {
		return false;
	}

	ndr_err = ndr_pull_struct_blob(&pkt->u.alter.auth_info,
				       call, NULL,
				       dce_conn->auth_state.auth_info,
				       (ndr_pull_flags_fn_t)ndr_pull_dcerpc_auth);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return false;
	}

	return true;
}

/*
  add any auth information needed in a alter ack, and process the authentication
  information found in the alter.
*/
NTSTATUS dcesrv_auth_alter_ack(struct dcesrv_call_state *call, struct ncacn_packet *pkt)
{
	struct dcesrv_connection *dce_conn = call->conn;
	NTSTATUS status;

	/* on a pure interface change there is no auth_info structure
	   setup */
	if (!call->conn->auth_state.auth_info ||
	    dce_conn->auth_state.auth_info->credentials.length == 0) {
		return NT_STATUS_OK;
	}

	if (!call->conn->auth_state.gensec_security) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	status = gensec_update(dce_conn->auth_state.gensec_security,
			       call,
			       dce_conn->auth_state.auth_info->credentials, 
			       &dce_conn->auth_state.auth_info->credentials);

	if (NT_STATUS_IS_OK(status)) {
		status = gensec_session_info(dce_conn->auth_state.gensec_security,
					     &dce_conn->auth_state.session_info);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(1, ("Failed to establish session_info: %s\n", nt_errstr(status)));
			return status;
		}

		/* Now that we are authenticated, got back to the generic session key... */
		dce_conn->auth_state.session_key = dcesrv_generic_session_key;
		return NT_STATUS_OK;
	} else if (NT_STATUS_EQUAL(status, NT_STATUS_MORE_PROCESSING_REQUIRED)) {
		dce_conn->auth_state.auth_info->auth_pad_length = 0;
		dce_conn->auth_state.auth_info->auth_reserved = 0;
		return NT_STATUS_OK;
	}

	DEBUG(2, ("Failed to finish dcesrv auth alter_ack: %s\n", nt_errstr(status)));
	return status;
}

/*
  check credentials on a request
*/
bool dcesrv_auth_request(struct dcesrv_call_state *call, DATA_BLOB *full_packet)
{
	struct ncacn_packet *pkt = &call->pkt;
	struct dcesrv_connection *dce_conn = call->conn;
	DATA_BLOB auth_blob;
	struct dcerpc_auth auth;
	struct ndr_pull *ndr;
	NTSTATUS status;
	enum ndr_err_code ndr_err;
	size_t hdr_size = DCERPC_REQUEST_LENGTH;

	if (!dce_conn->auth_state.auth_info ||
	    !dce_conn->auth_state.gensec_security) {
		return true;
	}

	switch (dce_conn->auth_state.auth_info->auth_level) {
	case DCERPC_AUTH_LEVEL_PRIVACY:
	case DCERPC_AUTH_LEVEL_INTEGRITY:
		break;

	case DCERPC_AUTH_LEVEL_CONNECT:
		if (pkt->auth_length != 0) {
			break;
		}
		return true;
	case DCERPC_AUTH_LEVEL_NONE:
		if (pkt->auth_length != 0) {
			return false;
		}
		return true;

	default:
		return false;
	}

	auth_blob.length = 8 + pkt->auth_length;

	/* check for a valid length */
	if (pkt->u.request.stub_and_verifier.length < auth_blob.length) {
		return false;
	}

	auth_blob.data = 
		pkt->u.request.stub_and_verifier.data + 
		pkt->u.request.stub_and_verifier.length - auth_blob.length;
	pkt->u.request.stub_and_verifier.length -= auth_blob.length;

	/* pull the auth structure */
	ndr = ndr_pull_init_blob(&auth_blob, call, lp_iconv_convenience(call->conn->dce_ctx->lp_ctx));
	if (!ndr) {
		return false;
	}

	if (!(pkt->drep[0] & DCERPC_DREP_LE)) {
		ndr->flags |= LIBNDR_FLAG_BIGENDIAN;
	}

	if (pkt->pfc_flags & DCERPC_PFC_FLAG_OBJECT_UUID) {
		ndr->flags |= LIBNDR_FLAG_OBJECT_PRESENT;
		hdr_size += 16;
	}

	ndr_err = ndr_pull_dcerpc_auth(ndr, NDR_SCALARS|NDR_BUFFERS, &auth);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(ndr);
		return false;
	}

	/* check signature or unseal the packet */
	switch (dce_conn->auth_state.auth_info->auth_level) {
	case DCERPC_AUTH_LEVEL_PRIVACY:
		status = gensec_unseal_packet(dce_conn->auth_state.gensec_security,
					      call,
					      full_packet->data + hdr_size,
					      pkt->u.request.stub_and_verifier.length, 
					      full_packet->data,
					      full_packet->length-auth.credentials.length,
					      &auth.credentials);
		memcpy(pkt->u.request.stub_and_verifier.data, 
		       full_packet->data + hdr_size,
		       pkt->u.request.stub_and_verifier.length);
		break;

	case DCERPC_AUTH_LEVEL_INTEGRITY:
		status = gensec_check_packet(dce_conn->auth_state.gensec_security,
					     call,
					     pkt->u.request.stub_and_verifier.data, 
					     pkt->u.request.stub_and_verifier.length,
					     full_packet->data,
					     full_packet->length-auth.credentials.length,
					     &auth.credentials);
		break;

	case DCERPC_AUTH_LEVEL_CONNECT:
		/* for now we ignore possible signatures here */
		status = NT_STATUS_OK;
		break;

	default:
		status = NT_STATUS_INVALID_LEVEL;
		break;
	}

	/* remove the indicated amount of padding */
	if (pkt->u.request.stub_and_verifier.length < auth.auth_pad_length) {
		talloc_free(ndr);
		return false;
	}
	pkt->u.request.stub_and_verifier.length -= auth.auth_pad_length;
	talloc_free(ndr);

	return NT_STATUS_IS_OK(status);
}


/* 
   push a signed or sealed dcerpc request packet into a blob
*/
bool dcesrv_auth_response(struct dcesrv_call_state *call,
			  DATA_BLOB *blob, size_t sig_size,
			  struct ncacn_packet *pkt)
{
	struct dcesrv_connection *dce_conn = call->conn;
	NTSTATUS status;
	enum ndr_err_code ndr_err;
	struct ndr_push *ndr;
	uint32_t payload_length;
	DATA_BLOB creds2;

	/* non-signed packets are simple */
	if (sig_size == 0) {
		status = ncacn_push_auth(blob, call, lp_iconv_convenience(dce_conn->dce_ctx->lp_ctx), pkt, NULL);
		return NT_STATUS_IS_OK(status);
	}

	switch (dce_conn->auth_state.auth_info->auth_level) {
	case DCERPC_AUTH_LEVEL_PRIVACY:
	case DCERPC_AUTH_LEVEL_INTEGRITY:
		break;

	case DCERPC_AUTH_LEVEL_CONNECT:
		/*
		 * TODO: let the gensec mech decide if it wants to generate a signature
		 *       that might be needed for schannel...
		 */
		status = ncacn_push_auth(blob, call, lp_iconv_convenience(dce_conn->dce_ctx->lp_ctx), pkt, NULL);
		return NT_STATUS_IS_OK(status);

	case DCERPC_AUTH_LEVEL_NONE:
		status = ncacn_push_auth(blob, call, lp_iconv_convenience(dce_conn->dce_ctx->lp_ctx), pkt, NULL);
		return NT_STATUS_IS_OK(status);

	default:
		return false;
	}

	ndr = ndr_push_init_ctx(call, lp_iconv_convenience(dce_conn->dce_ctx->lp_ctx));
	if (!ndr) {
		return false;
	}

	if (!(pkt->drep[0] & DCERPC_DREP_LE)) {
		ndr->flags |= LIBNDR_FLAG_BIGENDIAN;
	}

	ndr_err = ndr_push_ncacn_packet(ndr, NDR_SCALARS|NDR_BUFFERS, pkt);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return false;
	}

	/* pad to 16 byte multiple, match win2k3 */
	dce_conn->auth_state.auth_info->auth_pad_length =
		(16 - (pkt->u.response.stub_and_verifier.length & 15)) & 15;
	ndr_err = ndr_push_zero(ndr, dce_conn->auth_state.auth_info->auth_pad_length);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return false;
	}

	payload_length = pkt->u.response.stub_and_verifier.length +
		dce_conn->auth_state.auth_info->auth_pad_length;

	/* we start without signature, it will appended later */
	dce_conn->auth_state.auth_info->credentials = data_blob(NULL, 0);

	/* add the auth verifier */
	ndr_err = ndr_push_dcerpc_auth(ndr, NDR_SCALARS|NDR_BUFFERS,
				      dce_conn->auth_state.auth_info);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return false;
	}

	/* extract the whole packet as a blob */
	*blob = ndr_push_blob(ndr);

	/*
	 * Setup the frag and auth length in the packet buffer.
	 * This is needed if the GENSEC mech does AEAD signing
	 * of the packet headers. The signature itself will be
	 * appended later.
	 */
	dcerpc_set_frag_length(blob, blob->length + sig_size);
	dcerpc_set_auth_length(blob, sig_size);

	/* sign or seal the packet */
	switch (dce_conn->auth_state.auth_info->auth_level) {
	case DCERPC_AUTH_LEVEL_PRIVACY:
		status = gensec_seal_packet(dce_conn->auth_state.gensec_security, 
					    call,
					    ndr->data + DCERPC_REQUEST_LENGTH, 
					    payload_length,
					    blob->data,
					    blob->length,
					    &creds2);
		break;

	case DCERPC_AUTH_LEVEL_INTEGRITY:
		status = gensec_sign_packet(dce_conn->auth_state.gensec_security, 
					    call,
					    ndr->data + DCERPC_REQUEST_LENGTH, 
					    payload_length,
					    blob->data,
					    blob->length,
					    &creds2);
		break;

	default:
		status = NT_STATUS_INVALID_LEVEL;
		break;
	}

	if (NT_STATUS_IS_OK(status)) {
		if (creds2.length != sig_size) {
			DEBUG(0,("dcesrv_auth_response: creds2.length[%u] != sig_size[%u] pad[%u] stub[%u]\n",
				 (unsigned)creds2.length, (uint32_t)sig_size,
				 (unsigned)dce_conn->auth_state.auth_info->auth_pad_length,
				 (unsigned)pkt->u.response.stub_and_verifier.length));
			data_blob_free(&creds2);
			status = NT_STATUS_INTERNAL_ERROR;
		}
	}

	if (NT_STATUS_IS_OK(status)) {
		if (!data_blob_append(call, blob, creds2.data, creds2.length)) {
			status = NT_STATUS_NO_MEMORY;
		}
		data_blob_free(&creds2);
	}

	if (!NT_STATUS_IS_OK(status)) {
		return false;
	}	

	return true;
}
