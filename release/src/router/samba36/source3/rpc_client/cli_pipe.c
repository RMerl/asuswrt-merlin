/*
 *  Unix SMB/CIFS implementation.
 *  RPC Pipe client routines
 *  Largely rewritten by Jeremy Allison		    2005.
 *  Heavily modified by Simo Sorce		    2010.
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
#include "../lib/util/tevent_ntstatus.h"
#include "librpc/gen_ndr/ndr_epmapper_c.h"
#include "../librpc/gen_ndr/ndr_schannel.h"
#include "../librpc/gen_ndr/ndr_dssetup.h"
#include "../libcli/auth/schannel.h"
#include "../libcli/auth/spnego.h"
#include "../libcli/auth/ntlmssp.h"
#include "ntlmssp_wrap.h"
#include "librpc/gen_ndr/ndr_dcerpc.h"
#include "librpc/rpc/dcerpc.h"
#include "librpc/crypto/gse.h"
#include "librpc/crypto/spnego.h"
#include "rpc_dce.h"
#include "cli_pipe.h"
#include "client.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_RPC_CLI

/********************************************************************
 Pipe description for a DEBUG
 ********************************************************************/
static const char *rpccli_pipe_txt(TALLOC_CTX *mem_ctx,
				   struct rpc_pipe_client *cli)
{
	char *result = talloc_asprintf(mem_ctx, "host %s", cli->desthost);
	if (result == NULL) {
		return "pipe";
	}
	return result;
}

/********************************************************************
 Rpc pipe call id.
 ********************************************************************/

static uint32 get_rpc_call_id(void)
{
	static uint32 call_id = 0;
	return ++call_id;
}

/*******************************************************************
 Use SMBreadX to get rest of one fragment's worth of rpc data.
 Reads the whole size or give an error message
 ********************************************************************/

struct rpc_read_state {
	struct event_context *ev;
	struct rpc_cli_transport *transport;
	uint8_t *data;
	size_t size;
	size_t num_read;
};

static void rpc_read_done(struct tevent_req *subreq);

static struct tevent_req *rpc_read_send(TALLOC_CTX *mem_ctx,
					struct event_context *ev,
					struct rpc_cli_transport *transport,
					uint8_t *data, size_t size)
{
	struct tevent_req *req, *subreq;
	struct rpc_read_state *state;

	req = tevent_req_create(mem_ctx, &state, struct rpc_read_state);
	if (req == NULL) {
		return NULL;
	}
	state->ev = ev;
	state->transport = transport;
	state->data = data;
	state->size = size;
	state->num_read = 0;

	DEBUG(5, ("rpc_read_send: data_to_read: %u\n", (unsigned int)size));

	subreq = transport->read_send(state, ev, (uint8_t *)data, size,
				      transport->priv);
	if (subreq == NULL) {
		goto fail;
	}
	tevent_req_set_callback(subreq, rpc_read_done, req);
	return req;

 fail:
	TALLOC_FREE(req);
	return NULL;
}

static void rpc_read_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct rpc_read_state *state = tevent_req_data(
		req, struct rpc_read_state);
	NTSTATUS status;
	ssize_t received;

	status = state->transport->read_recv(subreq, &received);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}

	state->num_read += received;
	if (state->num_read == state->size) {
		tevent_req_done(req);
		return;
	}

	subreq = state->transport->read_send(state, state->ev,
					     state->data + state->num_read,
					     state->size - state->num_read,
					     state->transport->priv);
	if (tevent_req_nomem(subreq, req)) {
		return;
	}
	tevent_req_set_callback(subreq, rpc_read_done, req);
}

static NTSTATUS rpc_read_recv(struct tevent_req *req)
{
	return tevent_req_simple_recv_ntstatus(req);
}

struct rpc_write_state {
	struct event_context *ev;
	struct rpc_cli_transport *transport;
	const uint8_t *data;
	size_t size;
	size_t num_written;
};

static void rpc_write_done(struct tevent_req *subreq);

static struct tevent_req *rpc_write_send(TALLOC_CTX *mem_ctx,
					 struct event_context *ev,
					 struct rpc_cli_transport *transport,
					 const uint8_t *data, size_t size)
{
	struct tevent_req *req, *subreq;
	struct rpc_write_state *state;

	req = tevent_req_create(mem_ctx, &state, struct rpc_write_state);
	if (req == NULL) {
		return NULL;
	}
	state->ev = ev;
	state->transport = transport;
	state->data = data;
	state->size = size;
	state->num_written = 0;

	DEBUG(5, ("rpc_write_send: data_to_write: %u\n", (unsigned int)size));

	subreq = transport->write_send(state, ev, data, size, transport->priv);
	if (subreq == NULL) {
		goto fail;
	}
	tevent_req_set_callback(subreq, rpc_write_done, req);
	return req;
 fail:
	TALLOC_FREE(req);
	return NULL;
}

static void rpc_write_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct rpc_write_state *state = tevent_req_data(
		req, struct rpc_write_state);
	NTSTATUS status;
	ssize_t written;

	status = state->transport->write_recv(subreq, &written);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}

	state->num_written += written;

	if (state->num_written == state->size) {
		tevent_req_done(req);
		return;
	}

	subreq = state->transport->write_send(state, state->ev,
					      state->data + state->num_written,
					      state->size - state->num_written,
					      state->transport->priv);
	if (tevent_req_nomem(subreq, req)) {
		return;
	}
	tevent_req_set_callback(subreq, rpc_write_done, req);
}

static NTSTATUS rpc_write_recv(struct tevent_req *req)
{
	return tevent_req_simple_recv_ntstatus(req);
}


/****************************************************************************
 Try and get a PDU's worth of data from current_pdu. If not, then read more
 from the wire.
 ****************************************************************************/

struct get_complete_frag_state {
	struct event_context *ev;
	struct rpc_pipe_client *cli;
	uint16_t frag_len;
	uint32_t call_id;
	DATA_BLOB *pdu;
};

static void get_complete_frag_got_header(struct tevent_req *subreq);
static void get_complete_frag_got_rest(struct tevent_req *subreq);

static struct tevent_req *get_complete_frag_send(TALLOC_CTX *mem_ctx,
						 struct event_context *ev,
						 struct rpc_pipe_client *cli,
						 uint32_t call_id,
						 DATA_BLOB *pdu)
{
	struct tevent_req *req, *subreq;
	struct get_complete_frag_state *state;
	size_t received;
	NTSTATUS status;

	req = tevent_req_create(mem_ctx, &state,
				struct get_complete_frag_state);
	if (req == NULL) {
		return NULL;
	}
	state->ev = ev;
	state->cli = cli;
	state->frag_len = RPC_HEADER_LEN;
	state->call_id = call_id;
	state->pdu = pdu;

	received = pdu->length;
	if (received < RPC_HEADER_LEN) {
		if (!data_blob_realloc(mem_ctx, pdu, RPC_HEADER_LEN)) {
			status = NT_STATUS_NO_MEMORY;
			goto post_status;
		}
		subreq = rpc_read_send(state, state->ev,
					state->cli->transport,
					pdu->data + received,
					RPC_HEADER_LEN - received);
		if (subreq == NULL) {
			status = NT_STATUS_NO_MEMORY;
			goto post_status;
		}
		tevent_req_set_callback(subreq, get_complete_frag_got_header,
					req);
		return req;
	}

	state->frag_len = dcerpc_get_frag_length(pdu);
	if (state->frag_len < RPC_HEADER_LEN) {
		tevent_req_nterror(req, NT_STATUS_RPC_PROTOCOL_ERROR);
		return tevent_req_post(req, ev);
	}

	if (state->call_id != dcerpc_get_call_id(pdu)) {
		tevent_req_nterror(req, NT_STATUS_RPC_PROTOCOL_ERROR);
		return tevent_req_post(req, ev);
	}

	/*
	 * Ensure we have frag_len bytes of data.
	 */
	if (received < state->frag_len) {
		if (!data_blob_realloc(NULL, pdu, state->frag_len)) {
			status = NT_STATUS_NO_MEMORY;
			goto post_status;
		}
		subreq = rpc_read_send(state, state->ev,
					state->cli->transport,
					pdu->data + received,
					state->frag_len - received);
		if (subreq == NULL) {
			status = NT_STATUS_NO_MEMORY;
			goto post_status;
		}
		tevent_req_set_callback(subreq, get_complete_frag_got_rest,
					req);
		return req;
	}

	status = NT_STATUS_OK;
 post_status:
	if (NT_STATUS_IS_OK(status)) {
		tevent_req_done(req);
	} else {
		tevent_req_nterror(req, status);
	}
	return tevent_req_post(req, ev);
}

static void get_complete_frag_got_header(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct get_complete_frag_state *state = tevent_req_data(
		req, struct get_complete_frag_state);
	NTSTATUS status;

	status = rpc_read_recv(subreq);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}

	state->frag_len = dcerpc_get_frag_length(state->pdu);
	if (state->frag_len < RPC_HEADER_LEN) {
		tevent_req_nterror(req, NT_STATUS_RPC_PROTOCOL_ERROR);
		return;
	}

	if (state->call_id != dcerpc_get_call_id(state->pdu)) {
		tevent_req_nterror(req, NT_STATUS_RPC_PROTOCOL_ERROR);
		return;
	}

	if (!data_blob_realloc(NULL, state->pdu, state->frag_len)) {
		tevent_req_nterror(req, NT_STATUS_NO_MEMORY);
		return;
	}

	/*
	 * We're here in this piece of code because we've read exactly
	 * RPC_HEADER_LEN bytes into state->pdu.
	 */

	subreq = rpc_read_send(state, state->ev, state->cli->transport,
				state->pdu->data + RPC_HEADER_LEN,
				state->frag_len - RPC_HEADER_LEN);
	if (tevent_req_nomem(subreq, req)) {
		return;
	}
	tevent_req_set_callback(subreq, get_complete_frag_got_rest, req);
}

static void get_complete_frag_got_rest(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	NTSTATUS status;

	status = rpc_read_recv(subreq);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}
	tevent_req_done(req);
}

static NTSTATUS get_complete_frag_recv(struct tevent_req *req)
{
	return tevent_req_simple_recv_ntstatus(req);
}

/****************************************************************************
 Do basic authentication checks on an incoming pdu.
 ****************************************************************************/

static NTSTATUS cli_pipe_validate_current_pdu(TALLOC_CTX *mem_ctx,
						struct rpc_pipe_client *cli,
						struct ncacn_packet *pkt,
						DATA_BLOB *pdu,
						uint8_t expected_pkt_type,
						DATA_BLOB *rdata,
						DATA_BLOB *reply_pdu)
{
	struct dcerpc_response *r;
	NTSTATUS ret = NT_STATUS_OK;
	size_t pad_len = 0;

	/*
	 * Point the return values at the real data including the RPC
	 * header. Just in case the caller wants it.
	 */
	*rdata = *pdu;

	/* Ensure we have the correct type. */
	switch (pkt->ptype) {
	case DCERPC_PKT_ALTER_RESP:
	case DCERPC_PKT_BIND_ACK:

		/* Client code never receives this kind of packets */
		break;


	case DCERPC_PKT_RESPONSE:

		r = &pkt->u.response;

		/* Here's where we deal with incoming sign/seal. */
		ret = dcerpc_check_auth(cli->auth, pkt,
					&r->stub_and_verifier,
					DCERPC_RESPONSE_LENGTH,
					pdu, &pad_len);
		if (!NT_STATUS_IS_OK(ret)) {
			return ret;
		}

		if (pkt->frag_length < DCERPC_RESPONSE_LENGTH + pad_len) {
			return NT_STATUS_BUFFER_TOO_SMALL;
		}

		/* Point the return values at the NDR data. */
		rdata->data = r->stub_and_verifier.data;

		if (pkt->auth_length) {
			/* We've already done integer wrap tests in
			 * dcerpc_check_auth(). */
			rdata->length = r->stub_and_verifier.length
					 - pad_len
					 - DCERPC_AUTH_TRAILER_LENGTH
					 - pkt->auth_length;
		} else {
			rdata->length = r->stub_and_verifier.length;
		}

		DEBUG(10, ("Got pdu len %lu, data_len %lu, ss_len %u\n",
			   (long unsigned int)pdu->length,
			   (long unsigned int)rdata->length,
			   (unsigned int)pad_len));

		/*
		 * If this is the first reply, and the allocation hint is
		 * reasonable, try and set up the reply_pdu DATA_BLOB to the
		 * correct size.
		 */

		if ((reply_pdu->length == 0) &&
		    r->alloc_hint && (r->alloc_hint < 15*1024*1024)) {
			if (!data_blob_realloc(mem_ctx, reply_pdu,
							r->alloc_hint)) {
				DEBUG(0, ("reply alloc hint %d too "
					  "large to allocate\n",
					  (int)r->alloc_hint));
				return NT_STATUS_NO_MEMORY;
			}
		}

		break;

	case DCERPC_PKT_BIND_NAK:
		DEBUG(1, (__location__ ": Bind NACK received from %s!\n",
			  rpccli_pipe_txt(talloc_tos(), cli)));
		/* Use this for now... */
		return NT_STATUS_NETWORK_ACCESS_DENIED;

	case DCERPC_PKT_FAULT:

		DEBUG(1, (__location__ ": RPC fault code %s received "
			  "from %s!\n",
			  dcerpc_errstr(talloc_tos(),
			  pkt->u.fault.status),
			  rpccli_pipe_txt(talloc_tos(), cli)));

		return dcerpc_fault_to_nt_status(pkt->u.fault.status);

	default:
		DEBUG(0, (__location__ "Unknown packet type %u received "
			  "from %s!\n",
			  (unsigned int)pkt->ptype,
			  rpccli_pipe_txt(talloc_tos(), cli)));
		return NT_STATUS_INVALID_INFO_CLASS;
	}

	if (pkt->ptype != expected_pkt_type) {
		DEBUG(3, (__location__ ": Connection to %s got an unexpected "
			  "RPC packet type - %u, not %u\n",
			  rpccli_pipe_txt(talloc_tos(), cli),
			  pkt->ptype, expected_pkt_type));
		return NT_STATUS_INVALID_INFO_CLASS;
	}

	/* Do this just before return - we don't want to modify any rpc header
	   data before now as we may have needed to do cryptographic actions on
	   it before. */

	if ((pkt->ptype == DCERPC_PKT_BIND_ACK) &&
	    !(pkt->pfc_flags & DCERPC_PFC_FLAG_LAST)) {
		DEBUG(5, (__location__ ": bug in server (AS/U?), setting "
			  "fragment first/last ON.\n"));
		pkt->pfc_flags |= DCERPC_PFC_FLAG_FIRST | DCERPC_PFC_FLAG_LAST;
	}

	return NT_STATUS_OK;
}

/****************************************************************************
 Call a remote api on an arbitrary pipe.  takes param, data and setup buffers.
****************************************************************************/

struct cli_api_pipe_state {
	struct event_context *ev;
	struct rpc_cli_transport *transport;
	uint8_t *rdata;
	uint32_t rdata_len;
};

static void cli_api_pipe_trans_done(struct tevent_req *subreq);
static void cli_api_pipe_write_done(struct tevent_req *subreq);
static void cli_api_pipe_read_done(struct tevent_req *subreq);

static struct tevent_req *cli_api_pipe_send(TALLOC_CTX *mem_ctx,
					    struct event_context *ev,
					    struct rpc_cli_transport *transport,
					    uint8_t *data, size_t data_len,
					    uint32_t max_rdata_len)
{
	struct tevent_req *req, *subreq;
	struct cli_api_pipe_state *state;
	NTSTATUS status;

	req = tevent_req_create(mem_ctx, &state, struct cli_api_pipe_state);
	if (req == NULL) {
		return NULL;
	}
	state->ev = ev;
	state->transport = transport;

	if (max_rdata_len < RPC_HEADER_LEN) {
		/*
		 * For a RPC reply we always need at least RPC_HEADER_LEN
		 * bytes. We check this here because we will receive
		 * RPC_HEADER_LEN bytes in cli_trans_sock_send_done.
		 */
		status = NT_STATUS_INVALID_PARAMETER;
		goto post_status;
	}

	if (transport->trans_send != NULL) {
		subreq = transport->trans_send(state, ev, data, data_len,
					       max_rdata_len, transport->priv);
		if (subreq == NULL) {
			goto fail;
		}
		tevent_req_set_callback(subreq, cli_api_pipe_trans_done, req);
		return req;
	}

	/*
	 * If the transport does not provide a "trans" routine, i.e. for
	 * example the ncacn_ip_tcp transport, do the write/read step here.
	 */

	subreq = rpc_write_send(state, ev, transport, data, data_len);
	if (subreq == NULL) {
		goto fail;
	}
	tevent_req_set_callback(subreq, cli_api_pipe_write_done, req);
	return req;

 post_status:
	tevent_req_nterror(req, status);
	return tevent_req_post(req, ev);
 fail:
	TALLOC_FREE(req);
	return NULL;
}

static void cli_api_pipe_trans_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct cli_api_pipe_state *state = tevent_req_data(
		req, struct cli_api_pipe_state);
	NTSTATUS status;

	status = state->transport->trans_recv(subreq, state, &state->rdata,
					      &state->rdata_len);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}
	tevent_req_done(req);
}

static void cli_api_pipe_write_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct cli_api_pipe_state *state = tevent_req_data(
		req, struct cli_api_pipe_state);
	NTSTATUS status;

	status = rpc_write_recv(subreq);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}

	state->rdata = TALLOC_ARRAY(state, uint8_t, RPC_HEADER_LEN);
	if (tevent_req_nomem(state->rdata, req)) {
		return;
	}

	/*
	 * We don't need to use rpc_read_send here, the upper layer will cope
	 * with a short read, transport->trans_send could also return less
	 * than state->max_rdata_len.
	 */
	subreq = state->transport->read_send(state, state->ev, state->rdata,
					     RPC_HEADER_LEN,
					     state->transport->priv);
	if (tevent_req_nomem(subreq, req)) {
		return;
	}
	tevent_req_set_callback(subreq, cli_api_pipe_read_done, req);
}

static void cli_api_pipe_read_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct cli_api_pipe_state *state = tevent_req_data(
		req, struct cli_api_pipe_state);
	NTSTATUS status;
	ssize_t received;

	status = state->transport->read_recv(subreq, &received);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}
	state->rdata_len = received;
	tevent_req_done(req);
}

static NTSTATUS cli_api_pipe_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx,
				  uint8_t **prdata, uint32_t *prdata_len)
{
	struct cli_api_pipe_state *state = tevent_req_data(
		req, struct cli_api_pipe_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		return status;
	}

	*prdata = talloc_move(mem_ctx, &state->rdata);
	*prdata_len = state->rdata_len;
	return NT_STATUS_OK;
}

/****************************************************************************
 Send data on an rpc pipe via trans. The data must be the last
 pdu fragment of an NDR data stream.

 Receive response data from an rpc pipe, which may be large...

 Read the first fragment: unfortunately have to use SMBtrans for the first
 bit, then SMBreadX for subsequent bits.

 If first fragment received also wasn't the last fragment, continue
 getting fragments until we _do_ receive the last fragment.

 Request/Response PDU's look like the following...

 |<------------------PDU len----------------------------------------------->|
 |<-HDR_LEN-->|<--REQ LEN------>|.............|<-AUTH_HDRLEN->|<-AUTH_LEN-->|

 +------------+-----------------+-------------+---------------+-------------+
 | RPC HEADER | REQ/RESP HEADER | DATA ...... | AUTH_HDR      | AUTH DATA   |
 +------------+-----------------+-------------+---------------+-------------+

 Where the presence of the AUTH_HDR and AUTH DATA are dependent on the
 signing & sealing being negotiated.

 ****************************************************************************/

struct rpc_api_pipe_state {
	struct event_context *ev;
	struct rpc_pipe_client *cli;
	uint8_t expected_pkt_type;
	uint32_t call_id;

	DATA_BLOB incoming_frag;
	struct ncacn_packet *pkt;

	/* Incoming reply */
	DATA_BLOB reply_pdu;
	size_t reply_pdu_offset;
	uint8_t endianess;
};

static void rpc_api_pipe_trans_done(struct tevent_req *subreq);
static void rpc_api_pipe_got_pdu(struct tevent_req *subreq);
static void rpc_api_pipe_auth3_done(struct tevent_req *subreq);

static struct tevent_req *rpc_api_pipe_send(TALLOC_CTX *mem_ctx,
					    struct event_context *ev,
					    struct rpc_pipe_client *cli,
					    DATA_BLOB *data, /* Outgoing PDU */
					    uint8_t expected_pkt_type,
					    uint32_t call_id)
{
	struct tevent_req *req, *subreq;
	struct rpc_api_pipe_state *state;
	uint16_t max_recv_frag;
	NTSTATUS status;

	req = tevent_req_create(mem_ctx, &state, struct rpc_api_pipe_state);
	if (req == NULL) {
		return NULL;
	}
	state->ev = ev;
	state->cli = cli;
	state->expected_pkt_type = expected_pkt_type;
	state->call_id = call_id;
	state->incoming_frag = data_blob_null;
	state->reply_pdu = data_blob_null;
	state->reply_pdu_offset = 0;
	state->endianess = DCERPC_DREP_LE;

	/*
	 * Ensure we're not sending too much.
	 */
	if (data->length > cli->max_xmit_frag) {
		status = NT_STATUS_INVALID_PARAMETER;
		goto post_status;
	}

	DEBUG(5,("rpc_api_pipe: %s\n", rpccli_pipe_txt(talloc_tos(), cli)));

	if (state->expected_pkt_type == DCERPC_PKT_AUTH3) {
		subreq = rpc_write_send(state, ev, cli->transport,
					data->data, data->length);
		if (subreq == NULL) {
			goto fail;
		}
		tevent_req_set_callback(subreq, rpc_api_pipe_auth3_done, req);
		return req;
	}

	/* get the header first, then fetch the rest once we have
	 * the frag_length available */
	max_recv_frag = RPC_HEADER_LEN;

	subreq = cli_api_pipe_send(state, ev, cli->transport,
				   data->data, data->length, max_recv_frag);
	if (subreq == NULL) {
		goto fail;
	}
	tevent_req_set_callback(subreq, rpc_api_pipe_trans_done, req);
	return req;

 post_status:
	tevent_req_nterror(req, status);
	return tevent_req_post(req, ev);
 fail:
	TALLOC_FREE(req);
	return NULL;
}

static void rpc_api_pipe_auth3_done(struct tevent_req *subreq)
{
	struct tevent_req *req =
		tevent_req_callback_data(subreq,
		struct tevent_req);
	NTSTATUS status;

	status = rpc_write_recv(subreq);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}

	tevent_req_done(req);
}

static void rpc_api_pipe_trans_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct rpc_api_pipe_state *state = tevent_req_data(
		req, struct rpc_api_pipe_state);
	NTSTATUS status;
	uint8_t *rdata = NULL;
	uint32_t rdata_len = 0;

	status = cli_api_pipe_recv(subreq, state, &rdata, &rdata_len);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(5, ("cli_api_pipe failed: %s\n", nt_errstr(status)));
		tevent_req_nterror(req, status);
		return;
	}

	if (rdata == NULL) {
		DEBUG(3,("rpc_api_pipe: %s failed to return data.\n",
			 rpccli_pipe_txt(talloc_tos(), state->cli)));
		tevent_req_done(req);
		return;
	}

	/*
	 * Move data on state->incoming_frag.
	 */
	state->incoming_frag.data = talloc_move(state, &rdata);
	state->incoming_frag.length = rdata_len;
	if (!state->incoming_frag.data) {
		tevent_req_nterror(req, NT_STATUS_NO_MEMORY);
		return;
	}

	/* Ensure we have enough data for a pdu. */
	subreq = get_complete_frag_send(state, state->ev, state->cli,
					state->call_id,
					&state->incoming_frag);
	if (tevent_req_nomem(subreq, req)) {
		return;
	}
	tevent_req_set_callback(subreq, rpc_api_pipe_got_pdu, req);
}

static void rpc_api_pipe_got_pdu(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct rpc_api_pipe_state *state = tevent_req_data(
		req, struct rpc_api_pipe_state);
	NTSTATUS status;
	DATA_BLOB rdata = data_blob_null;

	status = get_complete_frag_recv(subreq);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(5, ("get_complete_frag failed: %s\n",
			  nt_errstr(status)));
		tevent_req_nterror(req, status);
		return;
	}

	state->pkt = talloc(state, struct ncacn_packet);
	if (!state->pkt) {
		tevent_req_nterror(req, NT_STATUS_NO_MEMORY);
		return;
	}

	status = dcerpc_pull_ncacn_packet(state->pkt,
					  &state->incoming_frag,
					  state->pkt,
					  !state->endianess);
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}

	if (state->incoming_frag.length != state->pkt->frag_length) {
		DEBUG(5, ("Incorrect pdu length %u, expected %u\n",
			  (unsigned int)state->incoming_frag.length,
			  (unsigned int)state->pkt->frag_length));
		tevent_req_nterror(req,  NT_STATUS_INVALID_PARAMETER);
		return;
	}

	status = cli_pipe_validate_current_pdu(state,
						state->cli, state->pkt,
						&state->incoming_frag,
						state->expected_pkt_type,
						&rdata,
						&state->reply_pdu);

	DEBUG(10,("rpc_api_pipe: got frag len of %u at offset %u: %s\n",
		  (unsigned)state->incoming_frag.length,
		  (unsigned)state->reply_pdu_offset,
		  nt_errstr(status)));

	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}

	if ((state->pkt->pfc_flags & DCERPC_PFC_FLAG_FIRST)
	    && (state->pkt->drep[0] != DCERPC_DREP_LE)) {
		/*
		 * Set the data type correctly for big-endian data on the
		 * first packet.
		 */
		DEBUG(10,("rpc_api_pipe: On %s PDU data format is "
			  "big-endian.\n",
			  rpccli_pipe_txt(talloc_tos(), state->cli)));
		state->endianess = 0x00; /* BIG ENDIAN */
	}
	/*
	 * Check endianness on subsequent packets.
	 */
	if (state->endianess != state->pkt->drep[0]) {
		DEBUG(0,("rpc_api_pipe: Error : Endianness changed from %s to "
			 "%s\n",
			 state->endianess?"little":"big",
			 state->pkt->drep[0]?"little":"big"));
		tevent_req_nterror(req, NT_STATUS_INVALID_PARAMETER);
		return;
	}

	/* Now copy the data portion out of the pdu into rbuf. */
	if (state->reply_pdu.length < state->reply_pdu_offset + rdata.length) {
		if (!data_blob_realloc(NULL, &state->reply_pdu,
				state->reply_pdu_offset + rdata.length)) {
			tevent_req_nterror(req, NT_STATUS_NO_MEMORY);
			return;
		}
	}

	memcpy(state->reply_pdu.data + state->reply_pdu_offset,
		rdata.data, rdata.length);
	state->reply_pdu_offset += rdata.length;

	/* reset state->incoming_frag, there is no need to free it,
	 * it will be reallocated to the right size the next time
	 * it is used */
	state->incoming_frag.length = 0;

	if (state->pkt->pfc_flags & DCERPC_PFC_FLAG_LAST) {
		/* make sure the pdu length is right now that we
		 * have all the data available (alloc hint may
		 * have allocated more than was actually used) */
		state->reply_pdu.length = state->reply_pdu_offset;
		DEBUG(10,("rpc_api_pipe: %s returned %u bytes.\n",
			  rpccli_pipe_txt(talloc_tos(), state->cli),
			  (unsigned)state->reply_pdu.length));
		tevent_req_done(req);
		return;
	}

	subreq = get_complete_frag_send(state, state->ev, state->cli,
					state->call_id,
					&state->incoming_frag);
	if (tevent_req_nomem(subreq, req)) {
		return;
	}
	tevent_req_set_callback(subreq, rpc_api_pipe_got_pdu, req);
}

static NTSTATUS rpc_api_pipe_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx,
				  struct ncacn_packet **pkt,
				  DATA_BLOB *reply_pdu)
{
	struct rpc_api_pipe_state *state = tevent_req_data(
		req, struct rpc_api_pipe_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		return status;
	}

	/* return data to caller and assign it ownership of memory */
	if (reply_pdu) {
		reply_pdu->data = talloc_move(mem_ctx, &state->reply_pdu.data);
		reply_pdu->length = state->reply_pdu.length;
		state->reply_pdu.length = 0;
	} else {
		data_blob_free(&state->reply_pdu);
	}

	if (pkt) {
		*pkt = talloc_steal(mem_ctx, state->pkt);
	}

	return NT_STATUS_OK;
}

/*******************************************************************
 Creates spnego auth bind.
 ********************************************************************/

static NTSTATUS create_spnego_auth_bind_req(TALLOC_CTX *mem_ctx,
					    struct pipe_auth_data *auth,
					    DATA_BLOB *auth_token)
{
	struct spnego_context *spnego_ctx;
	DATA_BLOB in_token = data_blob_null;
	NTSTATUS status;

	spnego_ctx = talloc_get_type_abort(auth->auth_ctx,
					   struct spnego_context);

	/* Negotiate the initial auth token */
	status = spnego_get_client_auth_token(mem_ctx, spnego_ctx,
					      &in_token, auth_token);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	DEBUG(5, ("Created GSS Authentication Token:\n"));
	dump_data(5, auth_token->data, auth_token->length);

	return NT_STATUS_OK;
}

/*******************************************************************
 Creates krb5 auth bind.
 ********************************************************************/

static NTSTATUS create_gssapi_auth_bind_req(TALLOC_CTX *mem_ctx,
					    struct pipe_auth_data *auth,
					    DATA_BLOB *auth_token)
{
	struct gse_context *gse_ctx;
	DATA_BLOB in_token = data_blob_null;
	NTSTATUS status;

	gse_ctx = talloc_get_type_abort(auth->auth_ctx,
					struct gse_context);

	/* Negotiate the initial auth token */
	status = gse_get_client_auth_token(mem_ctx, gse_ctx,
					   &in_token,
					   auth_token);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	DEBUG(5, ("Created GSS Authentication Token:\n"));
	dump_data(5, auth_token->data, auth_token->length);

	return NT_STATUS_OK;
}

/*******************************************************************
 Creates NTLMSSP auth bind.
 ********************************************************************/

static NTSTATUS create_ntlmssp_auth_rpc_bind_req(struct rpc_pipe_client *cli,
						 DATA_BLOB *auth_token)
{
	struct auth_ntlmssp_state *ntlmssp_ctx;
	DATA_BLOB null_blob = data_blob_null;
	NTSTATUS status;

	ntlmssp_ctx = talloc_get_type_abort(cli->auth->auth_ctx,
					    struct auth_ntlmssp_state);

	DEBUG(5, ("create_ntlmssp_auth_rpc_bind_req: Processing NTLMSSP Negotiate\n"));
	status = auth_ntlmssp_update(ntlmssp_ctx, null_blob, auth_token);

	if (!NT_STATUS_EQUAL(status, NT_STATUS_MORE_PROCESSING_REQUIRED)) {
		data_blob_free(auth_token);
		return status;
	}

	DEBUG(5, ("create_ntlmssp_auth_rpc_bind_req: NTLMSSP Negotiate:\n"));
	dump_data(5, auth_token->data, auth_token->length);

	return NT_STATUS_OK;
}

/*******************************************************************
 Creates schannel auth bind.
 ********************************************************************/

static NTSTATUS create_schannel_auth_rpc_bind_req(struct rpc_pipe_client *cli,
						  DATA_BLOB *auth_token)
{
	NTSTATUS status;
	struct NL_AUTH_MESSAGE r;

	/* Use lp_workgroup() if domain not specified */

	if (!cli->auth->domain || !cli->auth->domain[0]) {
		cli->auth->domain = talloc_strdup(cli, lp_workgroup());
		if (cli->auth->domain == NULL) {
			return NT_STATUS_NO_MEMORY;
		}
	}

	/*
	 * Now marshall the data into the auth parse_struct.
	 */

	r.MessageType			= NL_NEGOTIATE_REQUEST;
	r.Flags				= NL_FLAG_OEM_NETBIOS_DOMAIN_NAME |
					  NL_FLAG_OEM_NETBIOS_COMPUTER_NAME;
	r.oem_netbios_domain.a		= cli->auth->domain;
	r.oem_netbios_computer.a	= global_myname();

	status = dcerpc_push_schannel_bind(cli, &r, auth_token);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	return NT_STATUS_OK;
}

/*******************************************************************
 Creates the internals of a DCE/RPC bind request or alter context PDU.
 ********************************************************************/

static NTSTATUS create_bind_or_alt_ctx_internal(TALLOC_CTX *mem_ctx,
						enum dcerpc_pkt_type ptype,
						uint32 rpc_call_id,
						const struct ndr_syntax_id *abstract,
						const struct ndr_syntax_id *transfer,
						const DATA_BLOB *auth_info,
						DATA_BLOB *blob)
{
	uint16 auth_len = auth_info->length;
	NTSTATUS status;
	union dcerpc_payload u;
	struct dcerpc_ctx_list ctx_list;

	if (auth_len) {
		auth_len -= DCERPC_AUTH_TRAILER_LENGTH;
	}

	ctx_list.context_id = 0;
	ctx_list.num_transfer_syntaxes = 1;
	ctx_list.abstract_syntax = *abstract;
	ctx_list.transfer_syntaxes = (struct ndr_syntax_id *)discard_const(transfer);

	u.bind.max_xmit_frag	= RPC_MAX_PDU_FRAG_LEN;
	u.bind.max_recv_frag	= RPC_MAX_PDU_FRAG_LEN;
	u.bind.assoc_group_id	= 0x0;
	u.bind.num_contexts	= 1;
	u.bind.ctx_list		= &ctx_list;
	u.bind.auth_info	= *auth_info;

	status = dcerpc_push_ncacn_packet(mem_ctx,
					  ptype,
					  DCERPC_PFC_FLAG_FIRST |
					  DCERPC_PFC_FLAG_LAST,
					  auth_len,
					  rpc_call_id,
					  &u,
					  blob);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("Failed to marshall bind/alter ncacn_packet.\n"));
		return status;
	}

	return NT_STATUS_OK;
}

/*******************************************************************
 Creates a DCE/RPC bind request.
 ********************************************************************/

static NTSTATUS create_rpc_bind_req(TALLOC_CTX *mem_ctx,
				    struct rpc_pipe_client *cli,
				    struct pipe_auth_data *auth,
				    uint32 rpc_call_id,
				    const struct ndr_syntax_id *abstract,
				    const struct ndr_syntax_id *transfer,
				    DATA_BLOB *rpc_out)
{
	DATA_BLOB auth_token = data_blob_null;
	DATA_BLOB auth_info = data_blob_null;
	NTSTATUS ret = NT_STATUS_OK;

	switch (auth->auth_type) {
	case DCERPC_AUTH_TYPE_SCHANNEL:
		ret = create_schannel_auth_rpc_bind_req(cli, &auth_token);
		if (!NT_STATUS_IS_OK(ret)) {
			return ret;
		}
		break;

	case DCERPC_AUTH_TYPE_NTLMSSP:
		ret = create_ntlmssp_auth_rpc_bind_req(cli, &auth_token);
		if (!NT_STATUS_IS_OK(ret)) {
			return ret;
		}
		break;

	case DCERPC_AUTH_TYPE_SPNEGO:
		ret = create_spnego_auth_bind_req(cli, auth, &auth_token);
		if (!NT_STATUS_IS_OK(ret)) {
			return ret;
		}
		break;

	case DCERPC_AUTH_TYPE_KRB5:
		ret = create_gssapi_auth_bind_req(mem_ctx, auth, &auth_token);
		if (!NT_STATUS_IS_OK(ret)) {
			return ret;
		}
		break;

	case DCERPC_AUTH_TYPE_NCALRPC_AS_SYSTEM:
		auth_token = data_blob_talloc(mem_ctx,
					      "NCALRPC_AUTH_TOKEN",
					      18);
		break;

	case DCERPC_AUTH_TYPE_NONE:
		break;

	default:
		/* "Can't" happen. */
		return NT_STATUS_INVALID_INFO_CLASS;
	}

	if (auth_token.length != 0) {
		ret = dcerpc_push_dcerpc_auth(cli,
						auth->auth_type,
						auth->auth_level,
						0, /* auth_pad_length */
						1, /* auth_context_id */
						&auth_token,
						&auth_info);
		if (!NT_STATUS_IS_OK(ret)) {
			return ret;
		}
		data_blob_free(&auth_token);
	}

	ret = create_bind_or_alt_ctx_internal(mem_ctx,
					      DCERPC_PKT_BIND,
					      rpc_call_id,
					      abstract,
					      transfer,
					      &auth_info,
					      rpc_out);
	return ret;
}

/*******************************************************************
 External interface.
 Does an rpc request on a pipe. Incoming data is NDR encoded in in_data.
 Reply is NDR encoded in out_data. Splits the data stream into RPC PDU's
 and deals with signing/sealing details.
 ********************************************************************/

struct rpc_api_pipe_req_state {
	struct event_context *ev;
	struct rpc_pipe_client *cli;
	uint8_t op_num;
	uint32_t call_id;
	DATA_BLOB *req_data;
	uint32_t req_data_sent;
	DATA_BLOB rpc_out;
	DATA_BLOB reply_pdu;
};

static void rpc_api_pipe_req_write_done(struct tevent_req *subreq);
static void rpc_api_pipe_req_done(struct tevent_req *subreq);
static NTSTATUS prepare_next_frag(struct rpc_api_pipe_req_state *state,
				  bool *is_last_frag);

struct tevent_req *rpc_api_pipe_req_send(TALLOC_CTX *mem_ctx,
					 struct event_context *ev,
					 struct rpc_pipe_client *cli,
					 uint8_t op_num,
					 DATA_BLOB *req_data)
{
	struct tevent_req *req, *subreq;
	struct rpc_api_pipe_req_state *state;
	NTSTATUS status;
	bool is_last_frag;

	req = tevent_req_create(mem_ctx, &state,
				struct rpc_api_pipe_req_state);
	if (req == NULL) {
		return NULL;
	}
	state->ev = ev;
	state->cli = cli;
	state->op_num = op_num;
	state->req_data = req_data;
	state->req_data_sent = 0;
	state->call_id = get_rpc_call_id();
	state->reply_pdu = data_blob_null;
	state->rpc_out = data_blob_null;

	if (cli->max_xmit_frag < DCERPC_REQUEST_LENGTH
					+ RPC_MAX_SIGN_SIZE) {
		/* Server is screwed up ! */
		status = NT_STATUS_INVALID_PARAMETER;
		goto post_status;
	}

	status = prepare_next_frag(state, &is_last_frag);
	if (!NT_STATUS_IS_OK(status)) {
		goto post_status;
	}

	if (is_last_frag) {
		subreq = rpc_api_pipe_send(state, ev, state->cli,
					   &state->rpc_out,
					   DCERPC_PKT_RESPONSE,
					   state->call_id);
		if (subreq == NULL) {
			goto fail;
		}
		tevent_req_set_callback(subreq, rpc_api_pipe_req_done, req);
	} else {
		subreq = rpc_write_send(state, ev, cli->transport,
					state->rpc_out.data,
					state->rpc_out.length);
		if (subreq == NULL) {
			goto fail;
		}
		tevent_req_set_callback(subreq, rpc_api_pipe_req_write_done,
					req);
	}
	return req;

 post_status:
	tevent_req_nterror(req, status);
	return tevent_req_post(req, ev);
 fail:
	TALLOC_FREE(req);
	return NULL;
}

static NTSTATUS prepare_next_frag(struct rpc_api_pipe_req_state *state,
				  bool *is_last_frag)
{
	size_t data_sent_thistime;
	size_t auth_len;
	size_t frag_len;
	uint8_t flags = 0;
	size_t pad_len;
	size_t data_left;
	NTSTATUS status;
	union dcerpc_payload u;

	data_left = state->req_data->length - state->req_data_sent;

	status = dcerpc_guess_sizes(state->cli->auth,
				    DCERPC_REQUEST_LENGTH, data_left,
				    state->cli->max_xmit_frag,
				    CLIENT_NDR_PADDING_SIZE,
				    &data_sent_thistime,
				    &frag_len, &auth_len, &pad_len);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	if (state->req_data_sent == 0) {
		flags = DCERPC_PFC_FLAG_FIRST;
	}

	if (data_sent_thistime == data_left) {
		flags |= DCERPC_PFC_FLAG_LAST;
	}

	data_blob_free(&state->rpc_out);

	ZERO_STRUCT(u.request);

	u.request.alloc_hint	= state->req_data->length;
	u.request.context_id	= 0;
	u.request.opnum		= state->op_num;

	status = dcerpc_push_ncacn_packet(state,
					  DCERPC_PKT_REQUEST,
					  flags,
					  auth_len,
					  state->call_id,
					  &u,
					  &state->rpc_out);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	/* explicitly set frag_len here as dcerpc_push_ncacn_packet() can't
	 * compute it right for requests because the auth trailer is missing
	 * at this stage */
	dcerpc_set_frag_length(&state->rpc_out, frag_len);

	/* Copy in the data. */
	if (!data_blob_append(NULL, &state->rpc_out,
				state->req_data->data + state->req_data_sent,
				data_sent_thistime)) {
		return NT_STATUS_NO_MEMORY;
	}

	switch (state->cli->auth->auth_level) {
	case DCERPC_AUTH_LEVEL_NONE:
	case DCERPC_AUTH_LEVEL_CONNECT:
	case DCERPC_AUTH_LEVEL_PACKET:
		break;
	case DCERPC_AUTH_LEVEL_INTEGRITY:
	case DCERPC_AUTH_LEVEL_PRIVACY:
		status = dcerpc_add_auth_footer(state->cli->auth, pad_len,
						&state->rpc_out);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}
		break;
	default:
		return NT_STATUS_INVALID_PARAMETER;
	}

	state->req_data_sent += data_sent_thistime;
	*is_last_frag = ((flags & DCERPC_PFC_FLAG_LAST) != 0);

	return status;
}

static void rpc_api_pipe_req_write_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct rpc_api_pipe_req_state *state = tevent_req_data(
		req, struct rpc_api_pipe_req_state);
	NTSTATUS status;
	bool is_last_frag;

	status = rpc_write_recv(subreq);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}

	status = prepare_next_frag(state, &is_last_frag);
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}

	if (is_last_frag) {
		subreq = rpc_api_pipe_send(state, state->ev, state->cli,
					   &state->rpc_out,
					   DCERPC_PKT_RESPONSE,
					   state->call_id);
		if (tevent_req_nomem(subreq, req)) {
			return;
		}
		tevent_req_set_callback(subreq, rpc_api_pipe_req_done, req);
	} else {
		subreq = rpc_write_send(state, state->ev,
					state->cli->transport,
					state->rpc_out.data,
					state->rpc_out.length);
		if (tevent_req_nomem(subreq, req)) {
			return;
		}
		tevent_req_set_callback(subreq, rpc_api_pipe_req_write_done,
					req);
	}
}

static void rpc_api_pipe_req_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct rpc_api_pipe_req_state *state = tevent_req_data(
		req, struct rpc_api_pipe_req_state);
	NTSTATUS status;

	status = rpc_api_pipe_recv(subreq, state, NULL, &state->reply_pdu);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}
	tevent_req_done(req);
}

NTSTATUS rpc_api_pipe_req_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx,
			       DATA_BLOB *reply_pdu)
{
	struct rpc_api_pipe_req_state *state = tevent_req_data(
		req, struct rpc_api_pipe_req_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		/*
		 * We always have to initialize to reply pdu, even if there is
		 * none. The rpccli_* caller routines expect this.
		 */
		*reply_pdu = data_blob_null;
		return status;
	}

	/* return data to caller and assign it ownership of memory */
	reply_pdu->data = talloc_move(mem_ctx, &state->reply_pdu.data);
	reply_pdu->length = state->reply_pdu.length;
	state->reply_pdu.length = 0;

	return NT_STATUS_OK;
}

/****************************************************************************
 Check the rpc bind acknowledge response.
****************************************************************************/

static bool check_bind_response(const struct dcerpc_bind_ack *r,
				const struct ndr_syntax_id *transfer)
{
	struct dcerpc_ack_ctx ctx;

	if (r->secondary_address_size == 0) {
		DEBUG(4,("Ignoring length check -- ASU bug (server didn't fill in the pipe name correctly)"));
	}

	if (r->num_results < 1 || !r->ctx_list) {
		return false;
	}

	ctx = r->ctx_list[0];

	/* check the transfer syntax */
	if ((ctx.syntax.if_version != transfer->if_version) ||
	     (memcmp(&ctx.syntax.uuid, &transfer->uuid, sizeof(transfer->uuid)) !=0)) {
		DEBUG(2,("bind_rpc_pipe: transfer syntax differs\n"));
		return False;
	}

	if (r->num_results != 0x1 || ctx.result != 0) {
		DEBUG(2,("bind_rpc_pipe: bind denied results: %d reason: %x\n",
		          r->num_results, ctx.reason));
	}

	DEBUG(5,("check_bind_response: accepted!\n"));
	return True;
}

/*******************************************************************
 Creates a DCE/RPC bind authentication response.
 This is the packet that is sent back to the server once we
 have received a BIND-ACK, to finish the third leg of
 the authentication handshake.
 ********************************************************************/

static NTSTATUS create_rpc_bind_auth3(TALLOC_CTX *mem_ctx,
				struct rpc_pipe_client *cli,
				uint32 rpc_call_id,
				enum dcerpc_AuthType auth_type,
				enum dcerpc_AuthLevel auth_level,
				DATA_BLOB *pauth_blob,
				DATA_BLOB *rpc_out)
{
	NTSTATUS status;
	union dcerpc_payload u;

	u.auth3._pad = 0;

	status = dcerpc_push_dcerpc_auth(mem_ctx,
					 auth_type,
					 auth_level,
					 0, /* auth_pad_length */
					 1, /* auth_context_id */
					 pauth_blob,
					 &u.auth3.auth_info);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	status = dcerpc_push_ncacn_packet(mem_ctx,
					  DCERPC_PKT_AUTH3,
					  DCERPC_PFC_FLAG_FIRST |
					  DCERPC_PFC_FLAG_LAST,
					  pauth_blob->length,
					  rpc_call_id,
					  &u,
					  rpc_out);
	data_blob_free(&u.auth3.auth_info);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("create_bind_or_alt_ctx_internal: failed to marshall RPC_HDR_RB.\n"));
		return status;
	}

	return NT_STATUS_OK;
}

/*******************************************************************
 Creates a DCE/RPC bind alter context authentication request which
 may contain a spnego auth blobl
 ********************************************************************/

static NTSTATUS create_rpc_alter_context(TALLOC_CTX *mem_ctx,
					enum dcerpc_AuthType auth_type,
					enum dcerpc_AuthLevel auth_level,
					uint32 rpc_call_id,
					const struct ndr_syntax_id *abstract,
					const struct ndr_syntax_id *transfer,
					const DATA_BLOB *pauth_blob, /* spnego auth blob already created. */
					DATA_BLOB *rpc_out)
{
	DATA_BLOB auth_info;
	NTSTATUS status;

	status = dcerpc_push_dcerpc_auth(mem_ctx,
					 auth_type,
					 auth_level,
					 0, /* auth_pad_length */
					 1, /* auth_context_id */
					 pauth_blob,
					 &auth_info);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	status = create_bind_or_alt_ctx_internal(mem_ctx,
						 DCERPC_PKT_ALTER,
						 rpc_call_id,
						 abstract,
						 transfer,
						 &auth_info,
						 rpc_out);
	data_blob_free(&auth_info);
	return status;
}

/****************************************************************************
 Do an rpc bind.
****************************************************************************/

struct rpc_pipe_bind_state {
	struct event_context *ev;
	struct rpc_pipe_client *cli;
	DATA_BLOB rpc_out;
	bool auth3;
	uint32_t rpc_call_id;
};

static void rpc_pipe_bind_step_one_done(struct tevent_req *subreq);
static NTSTATUS rpc_bind_next_send(struct tevent_req *req,
				   struct rpc_pipe_bind_state *state,
				   DATA_BLOB *credentials);
static NTSTATUS rpc_bind_finish_send(struct tevent_req *req,
				     struct rpc_pipe_bind_state *state,
				     DATA_BLOB *credentials);

struct tevent_req *rpc_pipe_bind_send(TALLOC_CTX *mem_ctx,
				      struct event_context *ev,
				      struct rpc_pipe_client *cli,
				      struct pipe_auth_data *auth)
{
	struct tevent_req *req, *subreq;
	struct rpc_pipe_bind_state *state;
	NTSTATUS status;

	req = tevent_req_create(mem_ctx, &state, struct rpc_pipe_bind_state);
	if (req == NULL) {
		return NULL;
	}

	DEBUG(5,("Bind RPC Pipe: %s auth_type %u, auth_level %u\n",
		rpccli_pipe_txt(talloc_tos(), cli),
		(unsigned int)auth->auth_type,
		(unsigned int)auth->auth_level ));

	state->ev = ev;
	state->cli = cli;
	state->rpc_call_id = get_rpc_call_id();

	cli->auth = talloc_move(cli, &auth);

	/* Marshall the outgoing data. */
	status = create_rpc_bind_req(state, cli,
				     cli->auth,
				     state->rpc_call_id,
				     &cli->abstract_syntax,
				     &cli->transfer_syntax,
				     &state->rpc_out);

	if (!NT_STATUS_IS_OK(status)) {
		goto post_status;
	}

	subreq = rpc_api_pipe_send(state, ev, cli, &state->rpc_out,
				   DCERPC_PKT_BIND_ACK, state->rpc_call_id);
	if (subreq == NULL) {
		goto fail;
	}
	tevent_req_set_callback(subreq, rpc_pipe_bind_step_one_done, req);
	return req;

 post_status:
	tevent_req_nterror(req, status);
	return tevent_req_post(req, ev);
 fail:
	TALLOC_FREE(req);
	return NULL;
}

static void rpc_pipe_bind_step_one_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct rpc_pipe_bind_state *state = tevent_req_data(
		req, struct rpc_pipe_bind_state);
	struct pipe_auth_data *pauth = state->cli->auth;
	struct auth_ntlmssp_state *ntlmssp_ctx;
	struct spnego_context *spnego_ctx;
	struct gse_context *gse_ctx;
	struct ncacn_packet *pkt = NULL;
	struct dcerpc_auth auth;
	DATA_BLOB auth_token = data_blob_null;
	NTSTATUS status;

	status = rpc_api_pipe_recv(subreq, talloc_tos(), &pkt, NULL);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(3, ("rpc_pipe_bind: %s bind request returned %s\n",
			  rpccli_pipe_txt(talloc_tos(), state->cli),
			  nt_errstr(status)));
		tevent_req_nterror(req, status);
		return;
	}

	if (state->auth3) {
		tevent_req_done(req);
		return;
	}

	if (!check_bind_response(&pkt->u.bind_ack, &state->cli->transfer_syntax)) {
		DEBUG(2, ("rpc_pipe_bind: check_bind_response failed.\n"));
		tevent_req_nterror(req, NT_STATUS_BUFFER_TOO_SMALL);
		return;
	}

	state->cli->max_xmit_frag = pkt->u.bind_ack.max_xmit_frag;
	state->cli->max_recv_frag = pkt->u.bind_ack.max_recv_frag;

	switch(pauth->auth_type) {

	case DCERPC_AUTH_TYPE_NONE:
	case DCERPC_AUTH_TYPE_NCALRPC_AS_SYSTEM:
	case DCERPC_AUTH_TYPE_SCHANNEL:
		/* Bind complete. */
		tevent_req_done(req);
		return;

	case DCERPC_AUTH_TYPE_NTLMSSP:
	case DCERPC_AUTH_TYPE_SPNEGO:
	case DCERPC_AUTH_TYPE_KRB5:
		/* Paranoid lenght checks */
		if (pkt->frag_length < DCERPC_AUTH_TRAILER_LENGTH
						+ pkt->auth_length) {
			tevent_req_nterror(req,
					NT_STATUS_INFO_LENGTH_MISMATCH);
			return;
		}
		/* get auth credentials */
		status = dcerpc_pull_dcerpc_auth(talloc_tos(),
						 &pkt->u.bind_ack.auth_info,
						 &auth, false);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0, ("Failed to pull dcerpc auth: %s.\n",
				  nt_errstr(status)));
			tevent_req_nterror(req, status);
			return;
		}
		break;

	default:
		goto err_out;
	}

	/*
	 * For authenticated binds we may need to do 3 or 4 leg binds.
	 */

	switch(pauth->auth_type) {

	case DCERPC_AUTH_TYPE_NONE:
	case DCERPC_AUTH_TYPE_NCALRPC_AS_SYSTEM:
	case DCERPC_AUTH_TYPE_SCHANNEL:
		/* Bind complete. */
		tevent_req_done(req);
		return;

	case DCERPC_AUTH_TYPE_NTLMSSP:
		ntlmssp_ctx = talloc_get_type_abort(pauth->auth_ctx,
						    struct auth_ntlmssp_state);
		status = auth_ntlmssp_update(ntlmssp_ctx,
					     auth.credentials, &auth_token);
		if (NT_STATUS_EQUAL(status,
				    NT_STATUS_MORE_PROCESSING_REQUIRED)) {
			status = rpc_bind_next_send(req, state,
							&auth_token);
		} else if (NT_STATUS_IS_OK(status)) {
			status = rpc_bind_finish_send(req, state,
							&auth_token);
		}
		break;

	case DCERPC_AUTH_TYPE_SPNEGO:
		spnego_ctx = talloc_get_type_abort(pauth->auth_ctx,
						   struct spnego_context);
		status = spnego_get_client_auth_token(state,
						spnego_ctx,
						&auth.credentials,
						&auth_token);
		if (!NT_STATUS_IS_OK(status)) {
			break;
		}
		if (auth_token.length == 0) {
			/* Bind complete. */
			tevent_req_done(req);
			return;
		}
		if (spnego_require_more_processing(spnego_ctx)) {
			status = rpc_bind_next_send(req, state,
							&auth_token);
		} else {
			status = rpc_bind_finish_send(req, state,
							&auth_token);
		}
		break;

	case DCERPC_AUTH_TYPE_KRB5:
		gse_ctx = talloc_get_type_abort(pauth->auth_ctx,
						struct gse_context);
		status = gse_get_client_auth_token(state,
						   gse_ctx,
						   &auth.credentials,
						   &auth_token);
		if (!NT_STATUS_IS_OK(status)) {
			break;
		}

		if (gse_require_more_processing(gse_ctx)) {
			status = rpc_bind_next_send(req, state, &auth_token);
		} else {
			status = rpc_bind_finish_send(req, state, &auth_token);
		}
		break;

	default:
		goto err_out;
	}

	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
	}
	return;

err_out:
	DEBUG(0,("cli_finish_bind_auth: unknown auth type %u\n",
		 (unsigned int)state->cli->auth->auth_type));
	tevent_req_nterror(req, NT_STATUS_INTERNAL_ERROR);
}

static NTSTATUS rpc_bind_next_send(struct tevent_req *req,
				   struct rpc_pipe_bind_state *state,
				   DATA_BLOB *auth_token)
{
	struct pipe_auth_data *auth = state->cli->auth;
	struct tevent_req *subreq;
	NTSTATUS status;

	/* Now prepare the alter context pdu. */
	data_blob_free(&state->rpc_out);

	status = create_rpc_alter_context(state,
					  auth->auth_type,
					  auth->auth_level,
					  state->rpc_call_id,
					  &state->cli->abstract_syntax,
					  &state->cli->transfer_syntax,
					  auth_token,
					  &state->rpc_out);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	subreq = rpc_api_pipe_send(state, state->ev, state->cli,
				   &state->rpc_out, DCERPC_PKT_ALTER_RESP,
				   state->rpc_call_id);
	if (subreq == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	tevent_req_set_callback(subreq, rpc_pipe_bind_step_one_done, req);
	return NT_STATUS_OK;
}

static NTSTATUS rpc_bind_finish_send(struct tevent_req *req,
				     struct rpc_pipe_bind_state *state,
				     DATA_BLOB *auth_token)
{
	struct pipe_auth_data *auth = state->cli->auth;
	struct tevent_req *subreq;
	NTSTATUS status;

	state->auth3 = true;

	/* Now prepare the auth3 context pdu. */
	data_blob_free(&state->rpc_out);

	status = create_rpc_bind_auth3(state, state->cli,
					state->rpc_call_id,
					auth->auth_type,
					auth->auth_level,
					auth_token,
					&state->rpc_out);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	subreq = rpc_api_pipe_send(state, state->ev, state->cli,
				   &state->rpc_out, DCERPC_PKT_AUTH3,
				   state->rpc_call_id);
	if (subreq == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	tevent_req_set_callback(subreq, rpc_pipe_bind_step_one_done, req);
	return NT_STATUS_OK;
}

NTSTATUS rpc_pipe_bind_recv(struct tevent_req *req)
{
	return tevent_req_simple_recv_ntstatus(req);
}

NTSTATUS rpc_pipe_bind(struct rpc_pipe_client *cli,
		       struct pipe_auth_data *auth)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct event_context *ev;
	struct tevent_req *req;
	NTSTATUS status = NT_STATUS_OK;

	ev = event_context_init(frame);
	if (ev == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	req = rpc_pipe_bind_send(frame, ev, cli, auth);
	if (req == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	if (!tevent_req_poll(req, ev)) {
		status = map_nt_error_from_unix(errno);
		goto fail;
	}

	status = rpc_pipe_bind_recv(req);
 fail:
	TALLOC_FREE(frame);
	return status;
}

#define RPCCLI_DEFAULT_TIMEOUT 10000 /* 10 seconds. */

unsigned int rpccli_set_timeout(struct rpc_pipe_client *rpc_cli,
				unsigned int timeout)
{
	unsigned int old;

	if (rpc_cli->transport == NULL) {
		return RPCCLI_DEFAULT_TIMEOUT;
	}

	if (rpc_cli->transport->set_timeout == NULL) {
		return RPCCLI_DEFAULT_TIMEOUT;
	}

	old = rpc_cli->transport->set_timeout(rpc_cli->transport->priv, timeout);
	if (old == 0) {
		return RPCCLI_DEFAULT_TIMEOUT;
	}

	return old;
}

bool rpccli_is_connected(struct rpc_pipe_client *rpc_cli)
{
	if (rpc_cli == NULL) {
		return false;
	}

	if (rpc_cli->transport == NULL) {
		return false;
	}

	return rpc_cli->transport->is_connected(rpc_cli->transport->priv);
}

struct rpccli_bh_state {
	struct rpc_pipe_client *rpc_cli;
};

static bool rpccli_bh_is_connected(struct dcerpc_binding_handle *h)
{
	struct rpccli_bh_state *hs = dcerpc_binding_handle_data(h,
				     struct rpccli_bh_state);

	return rpccli_is_connected(hs->rpc_cli);
}

static uint32_t rpccli_bh_set_timeout(struct dcerpc_binding_handle *h,
				      uint32_t timeout)
{
	struct rpccli_bh_state *hs = dcerpc_binding_handle_data(h,
				     struct rpccli_bh_state);

	return rpccli_set_timeout(hs->rpc_cli, timeout);
}

struct rpccli_bh_raw_call_state {
	DATA_BLOB in_data;
	DATA_BLOB out_data;
	uint32_t out_flags;
};

static void rpccli_bh_raw_call_done(struct tevent_req *subreq);

static struct tevent_req *rpccli_bh_raw_call_send(TALLOC_CTX *mem_ctx,
						  struct tevent_context *ev,
						  struct dcerpc_binding_handle *h,
						  const struct GUID *object,
						  uint32_t opnum,
						  uint32_t in_flags,
						  const uint8_t *in_data,
						  size_t in_length)
{
	struct rpccli_bh_state *hs = dcerpc_binding_handle_data(h,
				     struct rpccli_bh_state);
	struct tevent_req *req;
	struct rpccli_bh_raw_call_state *state;
	bool ok;
	struct tevent_req *subreq;

	req = tevent_req_create(mem_ctx, &state,
				struct rpccli_bh_raw_call_state);
	if (req == NULL) {
		return NULL;
	}
	state->in_data.data = discard_const_p(uint8_t, in_data);
	state->in_data.length = in_length;

	ok = rpccli_bh_is_connected(h);
	if (!ok) {
		tevent_req_nterror(req, NT_STATUS_INVALID_CONNECTION);
		return tevent_req_post(req, ev);
	}

	subreq = rpc_api_pipe_req_send(state, ev, hs->rpc_cli,
				       opnum, &state->in_data);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, rpccli_bh_raw_call_done, req);

	return req;
}

static void rpccli_bh_raw_call_done(struct tevent_req *subreq)
{
	struct tevent_req *req =
		tevent_req_callback_data(subreq,
		struct tevent_req);
	struct rpccli_bh_raw_call_state *state =
		tevent_req_data(req,
		struct rpccli_bh_raw_call_state);
	NTSTATUS status;

	state->out_flags = 0;

	/* TODO: support bigendian responses */

	status = rpc_api_pipe_req_recv(subreq, state, &state->out_data);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}

	tevent_req_done(req);
}

static NTSTATUS rpccli_bh_raw_call_recv(struct tevent_req *req,
					TALLOC_CTX *mem_ctx,
					uint8_t **out_data,
					size_t *out_length,
					uint32_t *out_flags)
{
	struct rpccli_bh_raw_call_state *state =
		tevent_req_data(req,
		struct rpccli_bh_raw_call_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		tevent_req_received(req);
		return status;
	}

	*out_data = talloc_move(mem_ctx, &state->out_data.data);
	*out_length = state->out_data.length;
	*out_flags = state->out_flags;
	tevent_req_received(req);
	return NT_STATUS_OK;
}

struct rpccli_bh_disconnect_state {
	uint8_t _dummy;
};

static struct tevent_req *rpccli_bh_disconnect_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct dcerpc_binding_handle *h)
{
	struct rpccli_bh_state *hs = dcerpc_binding_handle_data(h,
				     struct rpccli_bh_state);
	struct tevent_req *req;
	struct rpccli_bh_disconnect_state *state;
	bool ok;

	req = tevent_req_create(mem_ctx, &state,
				struct rpccli_bh_disconnect_state);
	if (req == NULL) {
		return NULL;
	}

	ok = rpccli_bh_is_connected(h);
	if (!ok) {
		tevent_req_nterror(req, NT_STATUS_INVALID_CONNECTION);
		return tevent_req_post(req, ev);
	}

	/*
	 * TODO: do a real async disconnect ...
	 *
	 * For now the caller needs to free rpc_cli
	 */
	hs->rpc_cli = NULL;

	tevent_req_done(req);
	return tevent_req_post(req, ev);
}

static NTSTATUS rpccli_bh_disconnect_recv(struct tevent_req *req)
{
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		tevent_req_received(req);
		return status;
	}

	tevent_req_received(req);
	return NT_STATUS_OK;
}

static bool rpccli_bh_ref_alloc(struct dcerpc_binding_handle *h)
{
	return true;
}

static void rpccli_bh_do_ndr_print(struct dcerpc_binding_handle *h,
				   int ndr_flags,
				   const void *_struct_ptr,
				   const struct ndr_interface_call *call)
{
	void *struct_ptr = discard_const(_struct_ptr);

	if (DEBUGLEVEL < 10) {
		return;
	}

	if (ndr_flags & NDR_IN) {
		ndr_print_function_debug(call->ndr_print,
					 call->name,
					 ndr_flags,
					 struct_ptr);
	}
	if (ndr_flags & NDR_OUT) {
		ndr_print_function_debug(call->ndr_print,
					 call->name,
					 ndr_flags,
					 struct_ptr);
	}
}

static const struct dcerpc_binding_handle_ops rpccli_bh_ops = {
	.name			= "rpccli",
	.is_connected		= rpccli_bh_is_connected,
	.set_timeout		= rpccli_bh_set_timeout,
	.raw_call_send		= rpccli_bh_raw_call_send,
	.raw_call_recv		= rpccli_bh_raw_call_recv,
	.disconnect_send	= rpccli_bh_disconnect_send,
	.disconnect_recv	= rpccli_bh_disconnect_recv,

	.ref_alloc		= rpccli_bh_ref_alloc,
	.do_ndr_print		= rpccli_bh_do_ndr_print,
};

/* initialise a rpc_pipe_client binding handle */
struct dcerpc_binding_handle *rpccli_bh_create(struct rpc_pipe_client *c)
{
	struct dcerpc_binding_handle *h;
	struct rpccli_bh_state *hs;

	h = dcerpc_binding_handle_create(c,
					 &rpccli_bh_ops,
					 NULL,
					 NULL, /* TODO */
					 &hs,
					 struct rpccli_bh_state,
					 __location__);
	if (h == NULL) {
		return NULL;
	}
	hs->rpc_cli = c;

	return h;
}

bool rpccli_get_pwd_hash(struct rpc_pipe_client *rpc_cli, uint8_t nt_hash[16])
{
	struct auth_ntlmssp_state *a = NULL;
	struct cli_state *cli;

	if (rpc_cli->auth->auth_type == DCERPC_AUTH_TYPE_NTLMSSP) {
		a = talloc_get_type_abort(rpc_cli->auth->auth_ctx,
					  struct auth_ntlmssp_state);
	} else if (rpc_cli->auth->auth_type == DCERPC_AUTH_TYPE_SPNEGO) {
		struct spnego_context *spnego_ctx;
		enum spnego_mech auth_type;
		void *auth_ctx;
		NTSTATUS status;

		spnego_ctx = talloc_get_type_abort(rpc_cli->auth->auth_ctx,
						   struct spnego_context);
		status = spnego_get_negotiated_mech(spnego_ctx,
						    &auth_type, &auth_ctx);
		if (!NT_STATUS_IS_OK(status)) {
			return false;
		}

		if (auth_type == SPNEGO_NTLMSSP) {
			a = talloc_get_type_abort(auth_ctx,
						  struct auth_ntlmssp_state);
		}
	}

	if (a) {
		memcpy(nt_hash, auth_ntlmssp_get_nt_hash(a), 16);
		return true;
	}

	cli = rpc_pipe_np_smb_conn(rpc_cli);
	if (cli == NULL) {
		return false;
	}
	E_md4hash(cli->password ? cli->password : "", nt_hash);
	return true;
}

NTSTATUS rpccli_ncalrpc_bind_data(TALLOC_CTX *mem_ctx,
				  struct pipe_auth_data **presult)
{
	struct pipe_auth_data *result;

	result = talloc(mem_ctx, struct pipe_auth_data);
	if (result == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	result->auth_type = DCERPC_AUTH_TYPE_NCALRPC_AS_SYSTEM;
	result->auth_level = DCERPC_AUTH_LEVEL_CONNECT;

	result->user_name = talloc_strdup(result, "");
	result->domain = talloc_strdup(result, "");
	if ((result->user_name == NULL) || (result->domain == NULL)) {
		TALLOC_FREE(result);
		return NT_STATUS_NO_MEMORY;
	}

	*presult = result;
	return NT_STATUS_OK;
}

NTSTATUS rpccli_anon_bind_data(TALLOC_CTX *mem_ctx,
			       struct pipe_auth_data **presult)
{
	struct pipe_auth_data *result;

	result = talloc(mem_ctx, struct pipe_auth_data);
	if (result == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	result->auth_type = DCERPC_AUTH_TYPE_NONE;
	result->auth_level = DCERPC_AUTH_LEVEL_NONE;

	result->user_name = talloc_strdup(result, "");
	result->domain = talloc_strdup(result, "");
	if ((result->user_name == NULL) || (result->domain == NULL)) {
		TALLOC_FREE(result);
		return NT_STATUS_NO_MEMORY;
	}

	*presult = result;
	return NT_STATUS_OK;
}

static int cli_auth_ntlmssp_data_destructor(struct pipe_auth_data *auth)
{
	TALLOC_FREE(auth->auth_ctx);
	return 0;
}

static NTSTATUS rpccli_ntlmssp_bind_data(TALLOC_CTX *mem_ctx,
				  enum dcerpc_AuthType auth_type,
				  enum dcerpc_AuthLevel auth_level,
				  const char *domain,
				  const char *username,
				  const char *password,
				  struct pipe_auth_data **presult)
{
	struct auth_ntlmssp_state *ntlmssp_ctx;
	struct pipe_auth_data *result;
	NTSTATUS status;

	result = talloc(mem_ctx, struct pipe_auth_data);
	if (result == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	result->auth_type = auth_type;
	result->auth_level = auth_level;

	result->user_name = talloc_strdup(result, username);
	result->domain = talloc_strdup(result, domain);
	if ((result->user_name == NULL) || (result->domain == NULL)) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	status = auth_ntlmssp_client_start(NULL,
				      global_myname(),
				      lp_workgroup(),
				      lp_client_ntlmv2_auth(),
				      &ntlmssp_ctx);
	if (!NT_STATUS_IS_OK(status)) {
		goto fail;
	}

	talloc_set_destructor(result, cli_auth_ntlmssp_data_destructor);

	status = auth_ntlmssp_set_username(ntlmssp_ctx, username);
	if (!NT_STATUS_IS_OK(status)) {
		goto fail;
	}

	status = auth_ntlmssp_set_domain(ntlmssp_ctx, domain);
	if (!NT_STATUS_IS_OK(status)) {
		goto fail;
	}

	status = auth_ntlmssp_set_password(ntlmssp_ctx, password);
	if (!NT_STATUS_IS_OK(status)) {
		goto fail;
	}

	/*
	 * Turn off sign+seal to allow selected auth level to turn it back on.
	 */
	auth_ntlmssp_and_flags(ntlmssp_ctx, ~(NTLMSSP_NEGOTIATE_SIGN |
						NTLMSSP_NEGOTIATE_SEAL));

	if (auth_level == DCERPC_AUTH_LEVEL_INTEGRITY) {
		auth_ntlmssp_or_flags(ntlmssp_ctx, NTLMSSP_NEGOTIATE_SIGN);
	} else if (auth_level == DCERPC_AUTH_LEVEL_PRIVACY) {
		auth_ntlmssp_or_flags(ntlmssp_ctx, NTLMSSP_NEGOTIATE_SEAL |
						     NTLMSSP_NEGOTIATE_SIGN);
	}

	result->auth_ctx = ntlmssp_ctx;
	*presult = result;
	return NT_STATUS_OK;

 fail:
	TALLOC_FREE(result);
	return status;
}

NTSTATUS rpccli_schannel_bind_data(TALLOC_CTX *mem_ctx, const char *domain,
				   enum dcerpc_AuthLevel auth_level,
				   struct netlogon_creds_CredentialState *creds,
				   struct pipe_auth_data **presult)
{
	struct schannel_state *schannel_auth;
	struct pipe_auth_data *result;

	result = talloc(mem_ctx, struct pipe_auth_data);
	if (result == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	result->auth_type = DCERPC_AUTH_TYPE_SCHANNEL;
	result->auth_level = auth_level;

	result->user_name = talloc_strdup(result, "");
	result->domain = talloc_strdup(result, domain);
	if ((result->user_name == NULL) || (result->domain == NULL)) {
		goto fail;
	}

	schannel_auth = talloc(result, struct schannel_state);
	if (schannel_auth == NULL) {
		goto fail;
	}

	schannel_auth->state = SCHANNEL_STATE_START;
	schannel_auth->seq_num = 0;
	schannel_auth->initiator = true;
	schannel_auth->creds = netlogon_creds_copy(result, creds);

	result->auth_ctx = schannel_auth;
	*presult = result;
	return NT_STATUS_OK;

 fail:
	TALLOC_FREE(result);
	return NT_STATUS_NO_MEMORY;
}

/**
 * Create an rpc pipe client struct, connecting to a tcp port.
 */
static NTSTATUS rpc_pipe_open_tcp_port(TALLOC_CTX *mem_ctx, const char *host,
				       uint16_t port,
				       const struct ndr_syntax_id *abstract_syntax,
				       struct rpc_pipe_client **presult)
{
	struct rpc_pipe_client *result;
	struct sockaddr_storage addr;
	NTSTATUS status;
	int fd;

	result = TALLOC_ZERO_P(mem_ctx, struct rpc_pipe_client);
	if (result == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	result->abstract_syntax = *abstract_syntax;
	result->transfer_syntax = ndr_transfer_syntax;

	result->desthost = talloc_strdup(result, host);
	result->srv_name_slash = talloc_asprintf_strupper_m(
		result, "\\\\%s", result->desthost);
	if ((result->desthost == NULL) || (result->srv_name_slash == NULL)) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	result->max_xmit_frag = RPC_MAX_PDU_FRAG_LEN;
	result->max_recv_frag = RPC_MAX_PDU_FRAG_LEN;

	if (!resolve_name(host, &addr, NBT_NAME_SERVER, false)) {
		status = NT_STATUS_NOT_FOUND;
		goto fail;
	}

	status = open_socket_out(&addr, port, 60*1000, &fd);
	if (!NT_STATUS_IS_OK(status)) {
		goto fail;
	}
	set_socket_options(fd, lp_socket_options());

	status = rpc_transport_sock_init(result, fd, &result->transport);
	if (!NT_STATUS_IS_OK(status)) {
		close(fd);
		goto fail;
	}

	result->transport->transport = NCACN_IP_TCP;

	result->binding_handle = rpccli_bh_create(result);
	if (result->binding_handle == NULL) {
		TALLOC_FREE(result);
		return NT_STATUS_NO_MEMORY;
	}

	*presult = result;
	return NT_STATUS_OK;

 fail:
	TALLOC_FREE(result);
	return status;
}

/**
 * Determine the tcp port on which a dcerpc interface is listening
 * for the ncacn_ip_tcp transport via the endpoint mapper of the
 * target host.
 */
static NTSTATUS rpc_pipe_get_tcp_port(const char *host,
				      const struct ndr_syntax_id *abstract_syntax,
				      uint16_t *pport)
{
	NTSTATUS status;
	struct rpc_pipe_client *epm_pipe = NULL;
	struct dcerpc_binding_handle *epm_handle = NULL;
	struct pipe_auth_data *auth = NULL;
	struct dcerpc_binding *map_binding = NULL;
	struct dcerpc_binding *res_binding = NULL;
	struct epm_twr_t *map_tower = NULL;
	struct epm_twr_t *res_towers = NULL;
	struct policy_handle *entry_handle = NULL;
	uint32_t num_towers = 0;
	uint32_t max_towers = 1;
	struct epm_twr_p_t towers;
	TALLOC_CTX *tmp_ctx = talloc_stackframe();
	uint32_t result = 0;

	if (pport == NULL) {
		status = NT_STATUS_INVALID_PARAMETER;
		goto done;
	}

	if (ndr_syntax_id_equal(abstract_syntax,
				&ndr_table_epmapper.syntax_id)) {
		*pport = 135;
		return NT_STATUS_OK;
	}

	/* open the connection to the endpoint mapper */
	status = rpc_pipe_open_tcp_port(tmp_ctx, host, 135,
					&ndr_table_epmapper.syntax_id,
					&epm_pipe);

	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}
	epm_handle = epm_pipe->binding_handle;

	status = rpccli_anon_bind_data(tmp_ctx, &auth);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	status = rpc_pipe_bind(epm_pipe, auth);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	/* create tower for asking the epmapper */

	map_binding = TALLOC_ZERO_P(tmp_ctx, struct dcerpc_binding);
	if (map_binding == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto done;
	}

	map_binding->transport = NCACN_IP_TCP;
	map_binding->object = *abstract_syntax;
	map_binding->host = host; /* needed? */
	map_binding->endpoint = "0"; /* correct? needed? */

	map_tower = TALLOC_ZERO_P(tmp_ctx, struct epm_twr_t);
	if (map_tower == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto done;
	}

	status = dcerpc_binding_build_tower(tmp_ctx, map_binding,
					    &(map_tower->tower));
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	/* allocate further parameters for the epm_Map call */

	res_towers = TALLOC_ARRAY(tmp_ctx, struct epm_twr_t, max_towers);
	if (res_towers == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto done;
	}
	towers.twr = res_towers;

	entry_handle = TALLOC_ZERO_P(tmp_ctx, struct policy_handle);
	if (entry_handle == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto done;
	}

	/* ask the endpoint mapper for the port */

	status = dcerpc_epm_Map(epm_handle,
				tmp_ctx,
				CONST_DISCARD(struct GUID *,
					      &(abstract_syntax->uuid)),
				map_tower,
				entry_handle,
				max_towers,
				&num_towers,
				&towers,
				&result);

	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	if (result != EPMAPPER_STATUS_OK) {
		status = NT_STATUS_UNSUCCESSFUL;
		goto done;
	}

	if (num_towers != 1) {
		status = NT_STATUS_UNSUCCESSFUL;
		goto done;
	}

	/* extract the port from the answer */

	status = dcerpc_binding_from_tower(tmp_ctx,
					   &(towers.twr->tower),
					   &res_binding);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	/* are further checks here necessary? */
	if (res_binding->transport != NCACN_IP_TCP) {
		status = NT_STATUS_UNSUCCESSFUL;
		goto done;
	}

	*pport = (uint16_t)atoi(res_binding->endpoint);

done:
	TALLOC_FREE(tmp_ctx);
	return status;
}

/**
 * Create a rpc pipe client struct, connecting to a host via tcp.
 * The port is determined by asking the endpoint mapper on the given
 * host.
 */
NTSTATUS rpc_pipe_open_tcp(TALLOC_CTX *mem_ctx, const char *host,
			   const struct ndr_syntax_id *abstract_syntax,
			   struct rpc_pipe_client **presult)
{
	NTSTATUS status;
	uint16_t port = 0;

	status = rpc_pipe_get_tcp_port(host, abstract_syntax, &port);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	return rpc_pipe_open_tcp_port(mem_ctx, host, port,
					abstract_syntax, presult);
}

/********************************************************************
 Create a rpc pipe client struct, connecting to a unix domain socket
 ********************************************************************/
NTSTATUS rpc_pipe_open_ncalrpc(TALLOC_CTX *mem_ctx, const char *socket_path,
			       const struct ndr_syntax_id *abstract_syntax,
			       struct rpc_pipe_client **presult)
{
	struct rpc_pipe_client *result;
	struct sockaddr_un addr;
	NTSTATUS status;
	int fd;

	result = talloc_zero(mem_ctx, struct rpc_pipe_client);
	if (result == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	result->abstract_syntax = *abstract_syntax;
	result->transfer_syntax = ndr_transfer_syntax;

	result->desthost = get_myname(result);
	result->srv_name_slash = talloc_asprintf_strupper_m(
		result, "\\\\%s", result->desthost);
	if ((result->desthost == NULL) || (result->srv_name_slash == NULL)) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	result->max_xmit_frag = RPC_MAX_PDU_FRAG_LEN;
	result->max_recv_frag = RPC_MAX_PDU_FRAG_LEN;

	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd == -1) {
		status = map_nt_error_from_unix(errno);
		goto fail;
	}

	ZERO_STRUCT(addr);
	addr.sun_family = AF_UNIX;
	strlcpy(addr.sun_path, socket_path, sizeof(addr.sun_path));

	if (sys_connect(fd, (struct sockaddr *)(void *)&addr) == -1) {
		DEBUG(0, ("connect(%s) failed: %s\n", socket_path,
			  strerror(errno)));
		close(fd);
		return map_nt_error_from_unix(errno);
	}

	status = rpc_transport_sock_init(result, fd, &result->transport);
	if (!NT_STATUS_IS_OK(status)) {
		close(fd);
		goto fail;
	}

	result->transport->transport = NCALRPC;

	result->binding_handle = rpccli_bh_create(result);
	if (result->binding_handle == NULL) {
		TALLOC_FREE(result);
		return NT_STATUS_NO_MEMORY;
	}

	*presult = result;
	return NT_STATUS_OK;

 fail:
	TALLOC_FREE(result);
	return status;
}

struct rpc_pipe_client_np_ref {
	struct cli_state *cli;
	struct rpc_pipe_client *pipe;
};

static int rpc_pipe_client_np_ref_destructor(struct rpc_pipe_client_np_ref *np_ref)
{
	DLIST_REMOVE(np_ref->cli->pipe_list, np_ref->pipe);
	return 0;
}

/****************************************************************************
 Open a named pipe over SMB to a remote server.
 *
 * CAVEAT CALLER OF THIS FUNCTION:
 *    The returned rpc_pipe_client saves a copy of the cli_state cli pointer,
 *    so be sure that this function is called AFTER any structure (vs pointer)
 *    assignment of the cli.  In particular, libsmbclient does structure
 *    assignments of cli, which invalidates the data in the returned
 *    rpc_pipe_client if this function is called before the structure assignment
 *    of cli.
 * 
 ****************************************************************************/

static NTSTATUS rpc_pipe_open_np(struct cli_state *cli,
				 const struct ndr_syntax_id *abstract_syntax,
				 struct rpc_pipe_client **presult)
{
	struct rpc_pipe_client *result;
	NTSTATUS status;
	struct rpc_pipe_client_np_ref *np_ref;

	/* sanity check to protect against crashes */

	if ( !cli ) {
		return NT_STATUS_INVALID_HANDLE;
	}

	result = TALLOC_ZERO_P(NULL, struct rpc_pipe_client);
	if (result == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	result->abstract_syntax = *abstract_syntax;
	result->transfer_syntax = ndr_transfer_syntax;
	result->desthost = talloc_strdup(result, cli->desthost);
	result->srv_name_slash = talloc_asprintf_strupper_m(
		result, "\\\\%s", result->desthost);

	result->max_xmit_frag = RPC_MAX_PDU_FRAG_LEN;
	result->max_recv_frag = RPC_MAX_PDU_FRAG_LEN;

	if ((result->desthost == NULL) || (result->srv_name_slash == NULL)) {
		TALLOC_FREE(result);
		return NT_STATUS_NO_MEMORY;
	}

	status = rpc_transport_np_init(result, cli, abstract_syntax,
				       &result->transport);
	if (!NT_STATUS_IS_OK(status)) {
		TALLOC_FREE(result);
		return status;
	}

	result->transport->transport = NCACN_NP;

	np_ref = talloc(result->transport, struct rpc_pipe_client_np_ref);
	if (np_ref == NULL) {
		TALLOC_FREE(result);
		return NT_STATUS_NO_MEMORY;
	}
	np_ref->cli = cli;
	np_ref->pipe = result;

	DLIST_ADD(np_ref->cli->pipe_list, np_ref->pipe);
	talloc_set_destructor(np_ref, rpc_pipe_client_np_ref_destructor);

	result->binding_handle = rpccli_bh_create(result);
	if (result->binding_handle == NULL) {
		TALLOC_FREE(result);
		return NT_STATUS_NO_MEMORY;
	}

	*presult = result;
	return NT_STATUS_OK;
}

/****************************************************************************
 Open a pipe to a remote server.
 ****************************************************************************/

static NTSTATUS cli_rpc_pipe_open(struct cli_state *cli,
				  enum dcerpc_transport_t transport,
				  const struct ndr_syntax_id *interface,
				  struct rpc_pipe_client **presult)
{
	switch (transport) {
	case NCACN_IP_TCP:
		return rpc_pipe_open_tcp(NULL, cli->desthost, interface,
					 presult);
	case NCACN_NP:
		return rpc_pipe_open_np(cli, interface, presult);
	default:
		return NT_STATUS_NOT_IMPLEMENTED;
	}
}

/****************************************************************************
 Open a named pipe to an SMB server and bind anonymously.
 ****************************************************************************/

NTSTATUS cli_rpc_pipe_open_noauth_transport(struct cli_state *cli,
					    enum dcerpc_transport_t transport,
					    const struct ndr_syntax_id *interface,
					    struct rpc_pipe_client **presult)
{
	struct rpc_pipe_client *result;
	struct pipe_auth_data *auth;
	NTSTATUS status;

	status = cli_rpc_pipe_open(cli, transport, interface, &result);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	status = rpccli_anon_bind_data(result, &auth);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("rpccli_anon_bind_data returned %s\n",
			  nt_errstr(status)));
		TALLOC_FREE(result);
		return status;
	}

	/*
	 * This is a bit of an abstraction violation due to the fact that an
	 * anonymous bind on an authenticated SMB inherits the user/domain
	 * from the enclosing SMB creds
	 */

	TALLOC_FREE(auth->user_name);
	TALLOC_FREE(auth->domain);

	auth->user_name = talloc_strdup(auth, cli->user_name);
	auth->domain = talloc_strdup(auth, cli->domain);
	auth->user_session_key = data_blob_talloc(auth,
		cli->user_session_key.data,
		cli->user_session_key.length);

	if ((auth->user_name == NULL) || (auth->domain == NULL)) {
		TALLOC_FREE(result);
		return NT_STATUS_NO_MEMORY;
	}

	status = rpc_pipe_bind(result, auth);
	if (!NT_STATUS_IS_OK(status)) {
		int lvl = 0;
#ifdef ACTIVE_DIRECTORY
		if (ndr_syntax_id_equal(interface,
					&ndr_table_dssetup.syntax_id)) {
			/* non AD domains just don't have this pipe, avoid
			 * level 0 statement in that case - gd */
			lvl = 3;
		}
#endif
		DEBUG(lvl, ("cli_rpc_pipe_open_noauth: rpc_pipe_bind for pipe "
			    "%s failed with error %s\n",
			    get_pipe_name_from_syntax(talloc_tos(), interface),
			    nt_errstr(status) ));
		TALLOC_FREE(result);
		return status;
	}

	DEBUG(10,("cli_rpc_pipe_open_noauth: opened pipe %s to machine "
		  "%s and bound anonymously.\n",
		  get_pipe_name_from_syntax(talloc_tos(), interface),
		  cli->desthost));

	*presult = result;
	return NT_STATUS_OK;
}

/****************************************************************************
 ****************************************************************************/

NTSTATUS cli_rpc_pipe_open_noauth(struct cli_state *cli,
				  const struct ndr_syntax_id *interface,
				  struct rpc_pipe_client **presult)
{
	return cli_rpc_pipe_open_noauth_transport(cli, NCACN_NP,
						  interface, presult);
}

/****************************************************************************
 Open a named pipe to an SMB server and bind using NTLMSSP or SPNEGO NTLMSSP
 ****************************************************************************/

NTSTATUS cli_rpc_pipe_open_ntlmssp(struct cli_state *cli,
				   const struct ndr_syntax_id *interface,
				   enum dcerpc_transport_t transport,
				   enum dcerpc_AuthLevel auth_level,
				   const char *domain,
				   const char *username,
				   const char *password,
				   struct rpc_pipe_client **presult)
{
	struct rpc_pipe_client *result;
	struct pipe_auth_data *auth = NULL;
	enum dcerpc_AuthType auth_type = DCERPC_AUTH_TYPE_NTLMSSP;
	NTSTATUS status;

	status = cli_rpc_pipe_open(cli, transport, interface, &result);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	status = rpccli_ntlmssp_bind_data(result,
					  auth_type, auth_level,
					  domain, username, password,
					  &auth);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("rpccli_ntlmssp_bind_data returned %s\n",
			  nt_errstr(status)));
		goto err;
	}

	status = rpc_pipe_bind(result, auth);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("cli_rpc_pipe_open_ntlmssp_internal: cli_rpc_pipe_bind failed with error %s\n",
			nt_errstr(status) ));
		goto err;
	}

	DEBUG(10,("cli_rpc_pipe_open_ntlmssp_internal: opened pipe %s to "
		"machine %s and bound NTLMSSP as user %s\\%s.\n",
		  get_pipe_name_from_syntax(talloc_tos(), interface),
		  cli->desthost, domain, username ));

	*presult = result;
	return NT_STATUS_OK;

  err:

	TALLOC_FREE(result);
	return status;
}

/****************************************************************************
 External interface.
 Open a named pipe to an SMB server and bind using schannel (bind type 68)
 using session_key. sign and seal.

 The *pdc will be stolen onto this new pipe
 ****************************************************************************/

NTSTATUS cli_rpc_pipe_open_schannel_with_key(struct cli_state *cli,
					     const struct ndr_syntax_id *interface,
					     enum dcerpc_transport_t transport,
					     enum dcerpc_AuthLevel auth_level,
					     const char *domain,
					     struct netlogon_creds_CredentialState **pdc,
					     struct rpc_pipe_client **presult)
{
	struct rpc_pipe_client *result;
	struct pipe_auth_data *auth;
	NTSTATUS status;

	status = cli_rpc_pipe_open(cli, transport, interface, &result);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	status = rpccli_schannel_bind_data(result, domain, auth_level,
					   *pdc, &auth);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("rpccli_schannel_bind_data returned %s\n",
			  nt_errstr(status)));
		TALLOC_FREE(result);
		return status;
	}

	status = rpc_pipe_bind(result, auth);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("cli_rpc_pipe_open_schannel_with_key: "
			  "cli_rpc_pipe_bind failed with error %s\n",
			  nt_errstr(status) ));
		TALLOC_FREE(result);
		return status;
	}

	/*
	 * The credentials on a new netlogon pipe are the ones we are passed
	 * in - copy them over
	 */
	result->dc = netlogon_creds_copy(result, *pdc);
	if (result->dc == NULL) {
		TALLOC_FREE(result);
		return NT_STATUS_NO_MEMORY;
	}

	DEBUG(10,("cli_rpc_pipe_open_schannel_with_key: opened pipe %s to machine %s "
		  "for domain %s and bound using schannel.\n",
		  get_pipe_name_from_syntax(talloc_tos(), interface),
		  cli->desthost, domain ));

	*presult = result;
	return NT_STATUS_OK;
}

/****************************************************************************
 Open a named pipe to an SMB server and bind using krb5 (bind type 16).
 The idea is this can be called with service_princ, username and password all
 NULL so long as the caller has a TGT.
 ****************************************************************************/

NTSTATUS cli_rpc_pipe_open_krb5(struct cli_state *cli,
				const struct ndr_syntax_id *interface,
				enum dcerpc_transport_t transport,
				enum dcerpc_AuthLevel auth_level,
				const char *server,
				const char *username,
				const char *password,
				struct rpc_pipe_client **presult)
{
	struct rpc_pipe_client *result;
	struct pipe_auth_data *auth;
	struct gse_context *gse_ctx;
	NTSTATUS status;

	status = cli_rpc_pipe_open(cli, transport, interface, &result);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	auth = talloc(result, struct pipe_auth_data);
	if (auth == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto err_out;
	}
	auth->auth_type = DCERPC_AUTH_TYPE_KRB5;
	auth->auth_level = auth_level;

	if (!username) {
		username = "";
	}
	auth->user_name = talloc_strdup(auth, username);
	if (!auth->user_name) {
		status = NT_STATUS_NO_MEMORY;
		goto err_out;
	}

	/* Fixme, should we fetch/set the Realm ? */
	auth->domain = talloc_strdup(auth, "");
	if (!auth->domain) {
		status = NT_STATUS_NO_MEMORY;
		goto err_out;
	}

	status = gse_init_client(auth,
				 (auth_level == DCERPC_AUTH_LEVEL_INTEGRITY),
				 (auth_level == DCERPC_AUTH_LEVEL_PRIVACY),
				 NULL, server, "cifs", username, password,
				 GSS_C_DCE_STYLE, &gse_ctx);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("gse_init_client returned %s\n",
			  nt_errstr(status)));
		goto err_out;
	}
	auth->auth_ctx = gse_ctx;

	status = rpc_pipe_bind(result, auth);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("cli_rpc_pipe_bind failed with error %s\n",
			  nt_errstr(status)));
		goto err_out;
	}

	*presult = result;
	return NT_STATUS_OK;

err_out:
	TALLOC_FREE(result);
	return status;
}

NTSTATUS cli_rpc_pipe_open_spnego_krb5(struct cli_state *cli,
					const struct ndr_syntax_id *interface,
					enum dcerpc_transport_t transport,
					enum dcerpc_AuthLevel auth_level,
					const char *server,
					const char *username,
					const char *password,
					struct rpc_pipe_client **presult)
{
	struct rpc_pipe_client *result;
	struct pipe_auth_data *auth;
	struct spnego_context *spnego_ctx;
	NTSTATUS status;

	status = cli_rpc_pipe_open(cli, transport, interface, &result);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	auth = talloc(result, struct pipe_auth_data);
	if (auth == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto err_out;
	}
	auth->auth_type = DCERPC_AUTH_TYPE_SPNEGO;
	auth->auth_level = auth_level;

	if (!username) {
		username = "";
	}
	auth->user_name = talloc_strdup(auth, username);
	if (!auth->user_name) {
		status = NT_STATUS_NO_MEMORY;
		goto err_out;
	}

	/* Fixme, should we fetch/set the Realm ? */
	auth->domain = talloc_strdup(auth, "");
	if (!auth->domain) {
		status = NT_STATUS_NO_MEMORY;
		goto err_out;
	}

	status = spnego_gssapi_init_client(auth,
					   (auth->auth_level ==
						DCERPC_AUTH_LEVEL_INTEGRITY),
					   (auth->auth_level ==
						DCERPC_AUTH_LEVEL_PRIVACY),
					   true,
					   NULL, server, "cifs",
					   username, password,
					   &spnego_ctx);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("spnego_init_client returned %s\n",
			  nt_errstr(status)));
		goto err_out;
	}
	auth->auth_ctx = spnego_ctx;

	status = rpc_pipe_bind(result, auth);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("cli_rpc_pipe_bind failed with error %s\n",
			  nt_errstr(status)));
		goto err_out;
	}

	*presult = result;
	return NT_STATUS_OK;

err_out:
	TALLOC_FREE(result);
	return status;
}

NTSTATUS cli_rpc_pipe_open_spnego_ntlmssp(struct cli_state *cli,
					  const struct ndr_syntax_id *interface,
					  enum dcerpc_transport_t transport,
					  enum dcerpc_AuthLevel auth_level,
					  const char *domain,
					  const char *username,
					  const char *password,
					  struct rpc_pipe_client **presult)
{
	struct rpc_pipe_client *result;
	struct pipe_auth_data *auth;
	struct spnego_context *spnego_ctx;
	NTSTATUS status;

	status = cli_rpc_pipe_open(cli, transport, interface, &result);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	auth = talloc(result, struct pipe_auth_data);
	if (auth == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto err_out;
	}
	auth->auth_type = DCERPC_AUTH_TYPE_SPNEGO;
	auth->auth_level = auth_level;

	if (!username) {
		username = "";
	}
	auth->user_name = talloc_strdup(auth, username);
	if (!auth->user_name) {
		status = NT_STATUS_NO_MEMORY;
		goto err_out;
	}

	if (!domain) {
		domain = "";
	}
	auth->domain = talloc_strdup(auth, domain);
	if (!auth->domain) {
		status = NT_STATUS_NO_MEMORY;
		goto err_out;
	}

	status = spnego_ntlmssp_init_client(auth,
					    (auth->auth_level ==
						DCERPC_AUTH_LEVEL_INTEGRITY),
					    (auth->auth_level ==
						DCERPC_AUTH_LEVEL_PRIVACY),
					    true,
					    domain, username, password,
					    &spnego_ctx);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("spnego_init_client returned %s\n",
			  nt_errstr(status)));
		goto err_out;
	}
	auth->auth_ctx = spnego_ctx;

	status = rpc_pipe_bind(result, auth);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("cli_rpc_pipe_bind failed with error %s\n",
			  nt_errstr(status)));
		goto err_out;
	}

	*presult = result;
	return NT_STATUS_OK;

err_out:
	TALLOC_FREE(result);
	return status;
}

NTSTATUS cli_get_session_key(TALLOC_CTX *mem_ctx,
			     struct rpc_pipe_client *cli,
			     DATA_BLOB *session_key)
{
	struct pipe_auth_data *a = cli->auth;
	struct schannel_state *schannel_auth;
	struct auth_ntlmssp_state *ntlmssp_ctx;
	struct spnego_context *spnego_ctx;
	struct gse_context *gse_ctx;
	DATA_BLOB sk = data_blob_null;
	bool make_dup = false;

	if (!session_key || !cli) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (!cli->auth) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	switch (cli->auth->auth_type) {
	case DCERPC_AUTH_TYPE_SCHANNEL:
		schannel_auth = talloc_get_type_abort(a->auth_ctx,
						      struct schannel_state);
		sk = data_blob_const(schannel_auth->creds->session_key, 16);
		make_dup = true;
		break;
	case DCERPC_AUTH_TYPE_SPNEGO:
		spnego_ctx = talloc_get_type_abort(a->auth_ctx,
						   struct spnego_context);
		sk = spnego_get_session_key(mem_ctx, spnego_ctx);
		make_dup = false;
		break;
	case DCERPC_AUTH_TYPE_NTLMSSP:
		ntlmssp_ctx = talloc_get_type_abort(a->auth_ctx,
						    struct auth_ntlmssp_state);
		sk = auth_ntlmssp_get_session_key(ntlmssp_ctx);
		make_dup = true;
		break;
	case DCERPC_AUTH_TYPE_KRB5:
		gse_ctx = talloc_get_type_abort(a->auth_ctx,
						struct gse_context);
		sk = gse_get_session_key(mem_ctx, gse_ctx);
		make_dup = false;
		break;
	case DCERPC_AUTH_TYPE_NCALRPC_AS_SYSTEM:
	case DCERPC_AUTH_TYPE_NONE:
		sk = data_blob_const(a->user_session_key.data,
				     a->user_session_key.length);
		make_dup = true;
		break;
	default:
		break;
	}

	if (!sk.data) {
		return NT_STATUS_NO_USER_SESSION_KEY;
	}

	if (make_dup) {
		*session_key = data_blob_dup_talloc(mem_ctx, &sk);
	} else {
		*session_key = sk;
	}

	return NT_STATUS_OK;
}
