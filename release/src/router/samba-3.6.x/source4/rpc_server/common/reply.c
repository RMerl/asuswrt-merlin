/*
   Unix SMB/CIFS implementation.

   server side dcerpc common code

   Copyright (C) Andrew Tridgell 2003-2010
   Copyright (C) Stefan (metze) Metzmacher 2004-2005

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
#include "auth/auth.h"
#include "auth/gensec/gensec.h"
#include "../lib/util/dlinklist.h"
#include "rpc_server/dcerpc_server.h"
#include "rpc_server/dcerpc_server_proto.h"
#include "rpc_server/common/proto.h"
#include "librpc/rpc/dcerpc_proto.h"
#include "system/filesys.h"
#include "libcli/security/security.h"
#include "param/param.h"
#include "../lib/tsocket/tsocket.h"
#include "../libcli/named_pipe_auth/npa_tstream.h"
#include "smbd/service_stream.h"
#include "../lib/tsocket/tsocket.h"
#include "lib/socket/socket.h"
#include "smbd/process_model.h"
#include "lib/messaging/irpc.h"
#include "librpc/rpc/rpc_common.h"


/*
  move a call from an existing linked list to the specified list. This
  prevents bugs where we forget to remove the call from a previous
  list when moving it.
 */
static void dcesrv_call_set_list(struct dcesrv_call_state *call,
				 enum dcesrv_call_list list)
{
	switch (call->list) {
	case DCESRV_LIST_NONE:
		break;
	case DCESRV_LIST_CALL_LIST:
		DLIST_REMOVE(call->conn->call_list, call);
		break;
	case DCESRV_LIST_FRAGMENTED_CALL_LIST:
		DLIST_REMOVE(call->conn->incoming_fragmented_call_list, call);
		break;
	case DCESRV_LIST_PENDING_CALL_LIST:
		DLIST_REMOVE(call->conn->pending_call_list, call);
		break;
	}
	call->list = list;
	switch (list) {
	case DCESRV_LIST_NONE:
		break;
	case DCESRV_LIST_CALL_LIST:
		DLIST_ADD_END(call->conn->call_list, call, struct dcesrv_call_state *);
		break;
	case DCESRV_LIST_FRAGMENTED_CALL_LIST:
		DLIST_ADD_END(call->conn->incoming_fragmented_call_list, call, struct dcesrv_call_state *);
		break;
	case DCESRV_LIST_PENDING_CALL_LIST:
		DLIST_ADD_END(call->conn->pending_call_list, call, struct dcesrv_call_state *);
		break;
	}
}


void dcesrv_init_hdr(struct ncacn_packet *pkt, bool bigendian)
{
	pkt->rpc_vers = 5;
	pkt->rpc_vers_minor = 0;
	if (bigendian) {
		pkt->drep[0] = 0;
	} else {
		pkt->drep[0] = DCERPC_DREP_LE;
	}
	pkt->drep[1] = 0;
	pkt->drep[2] = 0;
	pkt->drep[3] = 0;
}


/*
  return a dcerpc fault
*/
NTSTATUS dcesrv_fault(struct dcesrv_call_state *call, uint32_t fault_code)
{
	struct ncacn_packet pkt;
	struct data_blob_list_item *rep;
	uint8_t zeros[4];
	NTSTATUS status;

	/* setup a bind_ack */
	dcesrv_init_hdr(&pkt, lpcfg_rpc_big_endian(call->conn->dce_ctx->lp_ctx));
	pkt.auth_length = 0;
	pkt.call_id = call->pkt.call_id;
	pkt.ptype = DCERPC_PKT_FAULT;
	pkt.pfc_flags = DCERPC_PFC_FLAG_FIRST | DCERPC_PFC_FLAG_LAST;
	pkt.u.fault.alloc_hint = 0;
	pkt.u.fault.context_id = 0;
	pkt.u.fault.cancel_count = 0;
	pkt.u.fault.status = fault_code;

	ZERO_STRUCT(zeros);
	pkt.u.fault._pad = data_blob_const(zeros, sizeof(zeros));

	rep = talloc(call, struct data_blob_list_item);
	if (!rep) {
		return NT_STATUS_NO_MEMORY;
	}

	status = ncacn_push_auth(&rep->blob, call, &pkt, NULL);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	dcerpc_set_frag_length(&rep->blob, rep->blob.length);

	DLIST_ADD_END(call->replies, rep, struct data_blob_list_item *);
	dcesrv_call_set_list(call, DCESRV_LIST_CALL_LIST);

	if (call->conn->call_list && call->conn->call_list->replies) {
		if (call->conn->transport.report_output_data) {
			call->conn->transport.report_output_data(call->conn);
		}
	}

	return NT_STATUS_OK;
}



_PUBLIC_ NTSTATUS dcesrv_reply(struct dcesrv_call_state *call)
{
	struct ndr_push *push;
	NTSTATUS status;
	DATA_BLOB stub;
	uint32_t total_length, chunk_size;
	struct dcesrv_connection_context *context = call->context;
	size_t sig_size = 0;

	/* call the reply function */
	status = context->iface->reply(call, call, call->r);
	if (!NT_STATUS_IS_OK(status)) {
		return dcesrv_fault(call, call->fault_code);
	}

	/* form the reply NDR */
	push = ndr_push_init_ctx(call);
	NT_STATUS_HAVE_NO_MEMORY(push);

	/* carry over the pointer count to the reply in case we are
	   using full pointer. See NDR specification for full
	   pointers */
	push->ptr_count = call->ndr_pull->ptr_count;

	if (lpcfg_rpc_big_endian(call->conn->dce_ctx->lp_ctx)) {
		push->flags |= LIBNDR_FLAG_BIGENDIAN;
	}

	status = context->iface->ndr_push(call, call, push, call->r);
	if (!NT_STATUS_IS_OK(status)) {
		return dcesrv_fault(call, call->fault_code);
	}

	stub = ndr_push_blob(push);

	total_length = stub.length;

	/* we can write a full max_recv_frag size, minus the dcerpc
	   request header size */
	chunk_size = call->conn->cli_max_recv_frag;
	chunk_size -= DCERPC_REQUEST_LENGTH;
	if (call->conn->auth_state.auth_info &&
	    call->conn->auth_state.gensec_security) {
		sig_size = gensec_sig_size(call->conn->auth_state.gensec_security,
					   call->conn->cli_max_recv_frag);
		if (sig_size) {
			chunk_size -= DCERPC_AUTH_TRAILER_LENGTH;
			chunk_size -= sig_size;
		}
	}
	chunk_size -= (chunk_size % 16);

	do {
		uint32_t length;
		struct data_blob_list_item *rep;
		struct ncacn_packet pkt;

		rep = talloc(call, struct data_blob_list_item);
		NT_STATUS_HAVE_NO_MEMORY(rep);

		length = MIN(chunk_size, stub.length);

		/* form the dcerpc response packet */
		dcesrv_init_hdr(&pkt,
				lpcfg_rpc_big_endian(call->conn->dce_ctx->lp_ctx));
		pkt.auth_length = 0;
		pkt.call_id = call->pkt.call_id;
		pkt.ptype = DCERPC_PKT_RESPONSE;
		pkt.pfc_flags = 0;
		if (stub.length == total_length) {
			pkt.pfc_flags |= DCERPC_PFC_FLAG_FIRST;
		}
		if (length == stub.length) {
			pkt.pfc_flags |= DCERPC_PFC_FLAG_LAST;
		}
		pkt.u.response.alloc_hint = stub.length;
		pkt.u.response.context_id = call->pkt.u.request.context_id;
		pkt.u.response.cancel_count = 0;
		pkt.u.response._pad.data = call->pkt.u.request._pad.data;
		pkt.u.response._pad.length = call->pkt.u.request._pad.length;
		pkt.u.response.stub_and_verifier.data = stub.data;
		pkt.u.response.stub_and_verifier.length = length;

		if (!dcesrv_auth_response(call, &rep->blob, sig_size, &pkt)) {
			return dcesrv_fault(call, DCERPC_FAULT_OTHER);
		}

		dcerpc_set_frag_length(&rep->blob, rep->blob.length);

		DLIST_ADD_END(call->replies, rep, struct data_blob_list_item *);

		stub.data += length;
		stub.length -= length;
	} while (stub.length != 0);

	/* move the call from the pending to the finished calls list */
	dcesrv_call_set_list(call, DCESRV_LIST_CALL_LIST);

	if (call->conn->call_list && call->conn->call_list->replies) {
		if (call->conn->transport.report_output_data) {
			call->conn->transport.report_output_data(call->conn);
		}
	}

	return NT_STATUS_OK;
}

NTSTATUS dcesrv_generic_session_key(struct dcesrv_connection *c,
				    DATA_BLOB *session_key)
{
	return dcerpc_generic_session_key(NULL, session_key);
}
