/*
   Unix SMB/CIFS implementation.
   raw dcerpc operations

   Copyright (C) Andrew Tridgell 2003-2005
   Copyright (C) Jelmer Vernooij 2004-2005

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
#include <tevent.h>
#include "lib/tsocket/tsocket.h"
#include "lib/util/tevent_ntstatus.h"
#include "librpc/rpc/dcerpc.h"
#include "librpc/gen_ndr/ndr_dcerpc.h"
#include "rpc_common.h"
#include "lib/util/bitmap.h"

/* we need to be able to get/set the fragment length without doing a full
   decode */
void dcerpc_set_frag_length(DATA_BLOB *blob, uint16_t v)
{
	if (CVAL(blob->data,DCERPC_DREP_OFFSET) & DCERPC_DREP_LE) {
		SSVAL(blob->data, DCERPC_FRAG_LEN_OFFSET, v);
	} else {
		RSSVAL(blob->data, DCERPC_FRAG_LEN_OFFSET, v);
	}
}

uint16_t dcerpc_get_frag_length(const DATA_BLOB *blob)
{
	if (CVAL(blob->data,DCERPC_DREP_OFFSET) & DCERPC_DREP_LE) {
		return SVAL(blob->data, DCERPC_FRAG_LEN_OFFSET);
	} else {
		return RSVAL(blob->data, DCERPC_FRAG_LEN_OFFSET);
	}
}

uint32_t dcerpc_get_call_id(const DATA_BLOB *blob)
{
	if (CVAL(blob->data,DCERPC_DREP_OFFSET) & DCERPC_DREP_LE) {
		return IVAL(blob->data, DCERPC_CALL_ID_OFFSET);
	} else {
		return RIVAL(blob->data, DCERPC_CALL_ID_OFFSET);
	}
}

void dcerpc_set_auth_length(DATA_BLOB *blob, uint16_t v)
{
	if (CVAL(blob->data,DCERPC_DREP_OFFSET) & DCERPC_DREP_LE) {
		SSVAL(blob->data, DCERPC_AUTH_LEN_OFFSET, v);
	} else {
		RSSVAL(blob->data, DCERPC_AUTH_LEN_OFFSET, v);
	}
}

uint8_t dcerpc_get_endian_flag(DATA_BLOB *blob)
{
	return blob->data[DCERPC_DREP_OFFSET];
}


/**
* @brief	Pull a dcerpc_auth structure, taking account of any auth
*		padding in the blob. For request/response packets we pass
*		the whole data blob, so auth_data_only must be set to false
*		as the blob contains data+pad+auth and no just pad+auth.
*
* @param pkt		- The ncacn_packet strcuture
* @param mem_ctx	- The mem_ctx used to allocate dcerpc_auth elements
* @param pkt_trailer	- The packet trailer data, usually the trailing
*			  auth_info blob, but in the request/response case
*			  this is the stub_and_verifier blob.
* @param auth		- A preallocated dcerpc_auth *empty* structure
* @param auth_length	- The length of the auth trail, sum of auth header
*			  lenght and pkt->auth_length
* @param auth_data_only	- Whether the pkt_trailer includes only the auth_blob
*			  (+ padding) or also other data.
*
* @return		- A NTSTATUS error code.
*/
NTSTATUS dcerpc_pull_auth_trailer(const struct ncacn_packet *pkt,
				  TALLOC_CTX *mem_ctx,
				  const DATA_BLOB *pkt_trailer,
				  struct dcerpc_auth *auth,
				  uint32_t *_auth_length,
				  bool auth_data_only)
{
	struct ndr_pull *ndr;
	enum ndr_err_code ndr_err;
	uint16_t data_and_pad;
	uint16_t auth_length;
	uint32_t tmp_length;

	ZERO_STRUCTP(auth);
	if (_auth_length != NULL) {
		*_auth_length = 0;
	}

	/* Paranoia checks for auth_length. The caller should check this... */
	if (pkt->auth_length == 0) {
		return NT_STATUS_INTERNAL_ERROR;
	}

	/* Paranoia checks for auth_length. The caller should check this... */
	if (pkt->auth_length > pkt->frag_length) {
		return NT_STATUS_INTERNAL_ERROR;
	}
	tmp_length = DCERPC_NCACN_PAYLOAD_OFFSET;
	tmp_length += DCERPC_AUTH_TRAILER_LENGTH;
	tmp_length += pkt->auth_length;
	if (tmp_length > pkt->frag_length) {
		return NT_STATUS_INTERNAL_ERROR;
	}
	if (pkt_trailer->length > UINT16_MAX) {
		return NT_STATUS_INTERNAL_ERROR;
	}

	auth_length = DCERPC_AUTH_TRAILER_LENGTH + pkt->auth_length;
	if (pkt_trailer->length < auth_length) {
		return NT_STATUS_RPC_PROTOCOL_ERROR;
	}

	data_and_pad = pkt_trailer->length - auth_length;

	ndr = ndr_pull_init_blob(pkt_trailer, mem_ctx);
	if (!ndr) {
		return NT_STATUS_NO_MEMORY;
	}

	if (!(pkt->drep[0] & DCERPC_DREP_LE)) {
		ndr->flags |= LIBNDR_FLAG_BIGENDIAN;
	}

	ndr_err = ndr_pull_advance(ndr, data_and_pad);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(ndr);
		return ndr_map_error2ntstatus(ndr_err);
	}

	ndr_err = ndr_pull_dcerpc_auth(ndr, NDR_SCALARS|NDR_BUFFERS, auth);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(ndr);
		ZERO_STRUCTP(auth);
		return ndr_map_error2ntstatus(ndr_err);
	}

	if (data_and_pad < auth->auth_pad_length) {
		DEBUG(1, (__location__ ": ERROR: pad length mismatch. "
			  "Calculated %u  got %u\n",
			  (unsigned)data_and_pad,
			  (unsigned)auth->auth_pad_length));
		talloc_free(ndr);
		ZERO_STRUCTP(auth);
		return NT_STATUS_RPC_PROTOCOL_ERROR;
	}

	if (auth_data_only && data_and_pad != auth->auth_pad_length) {
		DEBUG(1, (__location__ ": ERROR: pad length mismatch. "
			  "Calculated %u  got %u\n",
			  (unsigned)data_and_pad,
			  (unsigned)auth->auth_pad_length));
		talloc_free(ndr);
		ZERO_STRUCTP(auth);
		return NT_STATUS_RPC_PROTOCOL_ERROR;
	}

	DEBUG(6,(__location__ ": auth_pad_length %u\n",
		 (unsigned)auth->auth_pad_length));

	talloc_steal(mem_ctx, auth->credentials.data);
	talloc_free(ndr);

	if (_auth_length != NULL) {
		*_auth_length = auth_length;
	}

	return NT_STATUS_OK;
}

/**
* @brief	Verify the fields in ncacn_packet header.
*
* @param pkt		- The ncacn_packet strcuture
* @param ptype		- The expected PDU type
* @param max_auth_info	- The maximum size of a possible auth trailer
* @param required_flags	- The required flags for the pdu.
* @param optional_flags	- The possible optional flags for the pdu.
*
* @return		- A NTSTATUS error code.
*/
NTSTATUS dcerpc_verify_ncacn_packet_header(const struct ncacn_packet *pkt,
					   enum dcerpc_pkt_type ptype,
					   size_t max_auth_info,
					   uint8_t required_flags,
					   uint8_t optional_flags)
{
	if (pkt->rpc_vers != 5) {
		return NT_STATUS_RPC_PROTOCOL_ERROR;
	}

	if (pkt->rpc_vers_minor != 0) {
		return NT_STATUS_RPC_PROTOCOL_ERROR;
	}

	if (pkt->auth_length > pkt->frag_length) {
		return NT_STATUS_RPC_PROTOCOL_ERROR;
	}

	if (pkt->ptype != ptype) {
		return NT_STATUS_RPC_PROTOCOL_ERROR;
	}

	if (max_auth_info > UINT16_MAX) {
		return NT_STATUS_INTERNAL_ERROR;
	}

	if (pkt->auth_length > 0) {
		size_t max_auth_length;

		if (max_auth_info <= DCERPC_AUTH_TRAILER_LENGTH) {
			return NT_STATUS_RPC_PROTOCOL_ERROR;
		}
		max_auth_length = max_auth_info - DCERPC_AUTH_TRAILER_LENGTH;

		if (pkt->auth_length > max_auth_length) {
			return NT_STATUS_RPC_PROTOCOL_ERROR;
		}
	}

	if ((pkt->pfc_flags & required_flags) != required_flags) {
		return NT_STATUS_RPC_PROTOCOL_ERROR;
	}
	if (pkt->pfc_flags & ~(optional_flags|required_flags)) {
		return NT_STATUS_RPC_PROTOCOL_ERROR;
	}

	if (pkt->drep[0] & ~DCERPC_DREP_LE) {
		return NT_STATUS_RPC_PROTOCOL_ERROR;
	}
	if (pkt->drep[1] != 0) {
		return NT_STATUS_RPC_PROTOCOL_ERROR;
	}
	if (pkt->drep[2] != 0) {
		return NT_STATUS_RPC_PROTOCOL_ERROR;
	}
	if (pkt->drep[3] != 0) {
		return NT_STATUS_RPC_PROTOCOL_ERROR;
	}

	return NT_STATUS_OK;
}

struct dcerpc_read_ncacn_packet_state {
#if 0
	struct {
	} caller;
#endif
	DATA_BLOB buffer;
	struct ncacn_packet *pkt;
};

static int dcerpc_read_ncacn_packet_next_vector(struct tstream_context *stream,
						void *private_data,
						TALLOC_CTX *mem_ctx,
						struct iovec **_vector,
						size_t *_count);
static void dcerpc_read_ncacn_packet_done(struct tevent_req *subreq);

struct tevent_req *dcerpc_read_ncacn_packet_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct tstream_context *stream)
{
	struct tevent_req *req;
	struct dcerpc_read_ncacn_packet_state *state;
	struct tevent_req *subreq;

	req = tevent_req_create(mem_ctx, &state,
				struct dcerpc_read_ncacn_packet_state);
	if (req == NULL) {
		return NULL;
	}

	state->buffer = data_blob_const(NULL, 0);
	state->pkt = talloc(state, struct ncacn_packet);
	if (tevent_req_nomem(state->pkt, req)) {
		goto post;
	}

	subreq = tstream_readv_pdu_send(state, ev,
					stream,
					dcerpc_read_ncacn_packet_next_vector,
					state);
	if (tevent_req_nomem(subreq, req)) {
		goto post;
	}
	tevent_req_set_callback(subreq, dcerpc_read_ncacn_packet_done, req);

	return req;
 post:
	tevent_req_post(req, ev);
	return req;
}

static int dcerpc_read_ncacn_packet_next_vector(struct tstream_context *stream,
						void *private_data,
						TALLOC_CTX *mem_ctx,
						struct iovec **_vector,
						size_t *_count)
{
	struct dcerpc_read_ncacn_packet_state *state =
		talloc_get_type_abort(private_data,
		struct dcerpc_read_ncacn_packet_state);
	struct iovec *vector;
	off_t ofs = 0;

	if (state->buffer.length == 0) {
		/* first get enough to read the fragment length */
		ofs = 0;
		state->buffer.length = DCERPC_FRAG_LEN_OFFSET + 2;
		state->buffer.data = talloc_array(state, uint8_t,
						  state->buffer.length);
		if (!state->buffer.data) {
			return -1;
		}
	} else if (state->buffer.length == (DCERPC_FRAG_LEN_OFFSET + 2)) {
		/* now read the fragment length and allocate the full buffer */
		size_t frag_len = dcerpc_get_frag_length(&state->buffer);

		ofs = state->buffer.length;

		if (frag_len < ofs) {
			/*
			 * something is wrong, let the caller deal with it
			 */
			*_vector = NULL;
			*_count = 0;
			return 0;
		}

		state->buffer.data = talloc_realloc(state,
						    state->buffer.data,
						    uint8_t, frag_len);
		if (!state->buffer.data) {
			return -1;
		}
		state->buffer.length = frag_len;
	} else {
		/* if we reach this we have a full fragment */
		*_vector = NULL;
		*_count = 0;
		return 0;
	}

	/* now create the vector that we want to be filled */
	vector = talloc_array(mem_ctx, struct iovec, 1);
	if (!vector) {
		return -1;
	}

	vector[0].iov_base = (void *) (state->buffer.data + ofs);
	vector[0].iov_len = state->buffer.length - ofs;

	*_vector = vector;
	*_count = 1;
	return 0;
}

static void dcerpc_read_ncacn_packet_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(subreq,
				 struct tevent_req);
	struct dcerpc_read_ncacn_packet_state *state = tevent_req_data(req,
					struct dcerpc_read_ncacn_packet_state);
	int ret;
	int sys_errno;
	struct ndr_pull *ndr;
	enum ndr_err_code ndr_err;
	NTSTATUS status;

	ret = tstream_readv_pdu_recv(subreq, &sys_errno);
	TALLOC_FREE(subreq);
	if (ret == -1) {
		status = map_nt_error_from_unix(sys_errno);
		tevent_req_nterror(req, status);
		return;
	}

	ndr = ndr_pull_init_blob(&state->buffer, state->pkt);
	if (tevent_req_nomem(ndr, req)) {
		return;
	}

	if (!(CVAL(ndr->data, DCERPC_DREP_OFFSET) & DCERPC_DREP_LE)) {
		ndr->flags |= LIBNDR_FLAG_BIGENDIAN;
	}

	if (CVAL(ndr->data, DCERPC_PFC_OFFSET) & DCERPC_PFC_FLAG_OBJECT_UUID) {
		ndr->flags |= LIBNDR_FLAG_OBJECT_PRESENT;
	}

	ndr_err = ndr_pull_ncacn_packet(ndr, NDR_SCALARS|NDR_BUFFERS, state->pkt);
	TALLOC_FREE(ndr);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		status = ndr_map_error2ntstatus(ndr_err);
		tevent_req_nterror(req, status);
		return;
	}

	if (state->pkt->frag_length != state->buffer.length) {
		tevent_req_nterror(req, NT_STATUS_RPC_PROTOCOL_ERROR);
		return;
	}

	tevent_req_done(req);
}

NTSTATUS dcerpc_read_ncacn_packet_recv(struct tevent_req *req,
				       TALLOC_CTX *mem_ctx,
				       struct ncacn_packet **pkt,
				       DATA_BLOB *buffer)
{
	struct dcerpc_read_ncacn_packet_state *state = tevent_req_data(req,
					struct dcerpc_read_ncacn_packet_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		tevent_req_received(req);
		return status;
	}

	*pkt = talloc_move(mem_ctx, &state->pkt);
	if (buffer) {
		buffer->data = talloc_move(mem_ctx, &state->buffer.data);
		buffer->length = state->buffer.length;
	}

	tevent_req_received(req);
	return NT_STATUS_OK;
}

struct dcerpc_sec_vt_header2 dcerpc_sec_vt_header2_from_ncacn_packet(const struct ncacn_packet *pkt)
{
	struct dcerpc_sec_vt_header2 ret;

	ZERO_STRUCT(ret);
	ret.ptype = pkt->ptype;
	memcpy(&ret.drep, pkt->drep, sizeof(ret.drep));
	ret.call_id = pkt->call_id;

	switch (pkt->ptype) {
	case DCERPC_PKT_REQUEST:
		ret.context_id = pkt->u.request.context_id;
		ret.opnum      = pkt->u.request.opnum;
		break;

	case DCERPC_PKT_RESPONSE:
		ret.context_id = pkt->u.response.context_id;
		break;

	case DCERPC_PKT_FAULT:
		ret.context_id = pkt->u.fault.context_id;
		break;

	default:
		break;
	}

	return ret;
}

bool dcerpc_sec_vt_header2_equal(const struct dcerpc_sec_vt_header2 *v1,
				 const struct dcerpc_sec_vt_header2 *v2)
{
	if (v1->ptype != v2->ptype) {
		return false;
	}

	if (memcmp(v1->drep, v2->drep, sizeof(v1->drep)) != 0) {
		return false;
	}

	if (v1->call_id != v2->call_id) {
		return false;
	}

	if (v1->context_id != v2->context_id) {
		return false;
	}

	if (v1->opnum != v2->opnum) {
		return false;
	}

	return true;
}

static bool dcerpc_sec_vt_is_valid(const struct dcerpc_sec_verification_trailer *r)
{
	bool ret = false;
	TALLOC_CTX *frame = talloc_stackframe();
	struct bitmap *commands_seen;
	int i;

	if (r->count.count == 0) {
		ret = true;
		goto done;
	}

	if (memcmp(r->magic, DCERPC_SEC_VT_MAGIC, sizeof(r->magic)) != 0) {
		goto done;
	}

	commands_seen = bitmap_talloc(frame, DCERPC_SEC_VT_COMMAND_ENUM + 1);
	if (commands_seen == NULL) {
		goto done;
	}

	for (i=0; i < r->count.count; i++) {
		enum dcerpc_sec_vt_command_enum cmd =
			r->commands[i].command & DCERPC_SEC_VT_COMMAND_ENUM;

		if (bitmap_query(commands_seen, cmd)) {
			/* Each command must appear at most once. */
			goto done;
		}
		bitmap_set(commands_seen, cmd);

		switch (cmd) {
		case DCERPC_SEC_VT_COMMAND_BITMASK1:
		case DCERPC_SEC_VT_COMMAND_PCONTEXT:
		case DCERPC_SEC_VT_COMMAND_HEADER2:
			break;
		default:
			if ((r->commands[i].u._unknown.length % 4) != 0) {
				goto done;
			}
			break;
		}
	}
	ret = true;
done:
	TALLOC_FREE(frame);
	return ret;
}

#define CHECK(msg, ok)						\
do {								\
	if (!ok) {						\
		DEBUG(10, ("SEC_VT check %s failed\n", msg));	\
		return false;					\
	}							\
} while(0)

#define CHECK_SYNTAX(msg, s1, s2)					\
do {								\
	if (!ndr_syntax_id_equal(&s1, &s2)) {				\
		TALLOC_CTX *frame = talloc_stackframe();		\
		DEBUG(10, ("SEC_VT check %s failed: %s vs. %s\n", msg,	\
			   ndr_syntax_id_to_string(frame, &s1),		\
			   ndr_syntax_id_to_string(frame, &s1)));	\
		TALLOC_FREE(frame);					\
		return false;						\
	}								\
} while(0)


bool dcerpc_sec_verification_trailer_check(
		const struct dcerpc_sec_verification_trailer *vt,
		const uint32_t *bitmask1,
		const struct dcerpc_sec_vt_pcontext *pcontext,
		const struct dcerpc_sec_vt_header2 *header2)
{
	size_t i;

	if (!dcerpc_sec_vt_is_valid(vt)) {
		return false;
	}

	for (i=0; i < vt->count.count; i++) {
		struct dcerpc_sec_vt *c = &vt->commands[i];

		switch (c->command & DCERPC_SEC_VT_COMMAND_ENUM) {
		case DCERPC_SEC_VT_COMMAND_BITMASK1:
			if (bitmask1 == NULL) {
				CHECK("Bitmask1 must_process_command",
				      !(c->command & DCERPC_SEC_VT_MUST_PROCESS));
				break;
			}

			if (c->u.bitmask1 & DCERPC_SEC_VT_CLIENT_SUPPORTS_HEADER_SIGNING) {
				CHECK("Bitmask1 client_header_signing",
				      *bitmask1 & DCERPC_SEC_VT_CLIENT_SUPPORTS_HEADER_SIGNING);
			}
			break;

		case DCERPC_SEC_VT_COMMAND_PCONTEXT:
			if (pcontext == NULL) {
				CHECK("Pcontext must_process_command",
				      !(c->command & DCERPC_SEC_VT_MUST_PROCESS));
				break;
			}

			CHECK_SYNTAX("Pcontect abstract_syntax",
				     pcontext->abstract_syntax,
				     c->u.pcontext.abstract_syntax);
			CHECK_SYNTAX("Pcontext transfer_syntax",
				     pcontext->transfer_syntax,
				     c->u.pcontext.transfer_syntax);
			break;

		case DCERPC_SEC_VT_COMMAND_HEADER2: {
			if (header2 == NULL) {
				CHECK("Header2 must_process_command",
				      !(c->command & DCERPC_SEC_VT_MUST_PROCESS));
				break;
			}

			CHECK("Header2", dcerpc_sec_vt_header2_equal(header2, &c->u.header2));
			break;
		}

		default:
			CHECK("Unknown must_process_command",
			      !(c->command & DCERPC_SEC_VT_MUST_PROCESS));
			break;
		}
	}

	return true;
}
