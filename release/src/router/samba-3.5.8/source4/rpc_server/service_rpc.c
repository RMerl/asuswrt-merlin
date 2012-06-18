/* 
   Unix SMB/CIFS implementation.

   smbd-specific dcerpc server code

   Copyright (C) Andrew Tridgell 2003-2005
   Copyright (C) Stefan (metze) Metzmacher 2004-2005
   Copyright (C) Jelmer Vernooij <jelmer@samba.org> 2004,2007
   
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
#include "librpc/gen_ndr/ndr_dcerpc.h"
#include "auth/auth.h"
#include "../lib/util/dlinklist.h"
#include "rpc_server/dcerpc_server.h"
#include "rpc_server/dcerpc_server_proto.h"
#include "smbd/service.h"
#include "system/filesys.h"
#include "lib/socket/socket.h"
#include "lib/messaging/irpc.h"
#include "system/network.h"
#include "lib/socket/netif.h"
#include "param/param.h"
#include "../lib/tsocket/tsocket.h"
#include "librpc/rpc/dcerpc_proto.h"
#include "../lib/util/tevent_ntstatus.h"
#include "libcli/raw/smb.h"
#include "../libcli/named_pipe_auth/npa_tstream.h"
#include "smbd/process_model.h"

struct dcesrv_socket_context {
	const struct dcesrv_endpoint *endpoint;
	struct dcesrv_context *dcesrv_ctx;
};

static void dcesrv_terminate_connection(struct dcesrv_connection *dce_conn, const char *reason)
{
	struct stream_connection *srv_conn;
	srv_conn = talloc_get_type(dce_conn->transport.private_data,
				   struct stream_connection);

	stream_terminate_connection(srv_conn, reason);
}

static void dcesrv_sock_reply_done(struct tevent_req *subreq);

struct dcesrv_sock_reply_state {
	struct dcesrv_connection *dce_conn;
	struct dcesrv_call_state *call;
	struct iovec iov;
};

static void dcesrv_sock_report_output_data(struct dcesrv_connection *dce_conn)
{
	struct dcesrv_call_state *call;

	call = dce_conn->call_list;
	if (!call || !call->replies) {
		return;
	}

	while (call->replies) {
		struct data_blob_list_item *rep = call->replies;
		struct dcesrv_sock_reply_state *substate;
		struct tevent_req *subreq;

		substate = talloc(call, struct dcesrv_sock_reply_state);
		if (!substate) {
			dcesrv_terminate_connection(dce_conn, "no memory");
			return;
		}

		substate->dce_conn = dce_conn;
		substate->call = NULL;

		DLIST_REMOVE(call->replies, rep);

		if (call->replies == NULL) {
			substate->call = call;
		}

		substate->iov.iov_base = rep->blob.data;
		substate->iov.iov_len = rep->blob.length;

		subreq = tstream_writev_queue_send(substate,
						   dce_conn->event_ctx,
						   dce_conn->stream,
						   dce_conn->send_queue,
						   &substate->iov, 1);
		if (!subreq) {
			dcesrv_terminate_connection(dce_conn, "no memory");
			return;
		}
		tevent_req_set_callback(subreq, dcesrv_sock_reply_done,
					substate);
	}

	DLIST_REMOVE(call->conn->call_list, call);
	call->list = DCESRV_LIST_NONE;
}

static void dcesrv_sock_reply_done(struct tevent_req *subreq)
{
	struct dcesrv_sock_reply_state *substate = tevent_req_callback_data(subreq,
						struct dcesrv_sock_reply_state);
	int ret;
	int sys_errno;
	NTSTATUS status;
	struct dcesrv_call_state *call = substate->call;

	ret = tstream_writev_queue_recv(subreq, &sys_errno);
	TALLOC_FREE(subreq);
	if (ret == -1) {
		status = map_nt_error_from_unix(sys_errno);
		dcesrv_terminate_connection(substate->dce_conn, nt_errstr(status));
		return;
	}

	talloc_free(substate);
	if (call) {
		talloc_free(call);
	}
}

static struct socket_address *dcesrv_sock_get_my_addr(struct dcesrv_connection *dcesrv_conn, TALLOC_CTX *mem_ctx)
{
	struct stream_connection *srv_conn;
	srv_conn = talloc_get_type(dcesrv_conn->transport.private_data,
				   struct stream_connection);

	return socket_get_my_addr(srv_conn->socket, mem_ctx);
}

static struct socket_address *dcesrv_sock_get_peer_addr(struct dcesrv_connection *dcesrv_conn, TALLOC_CTX *mem_ctx)
{
	struct stream_connection *srv_conn;
	srv_conn = talloc_get_type(dcesrv_conn->transport.private_data,
				   struct stream_connection);

	return socket_get_peer_addr(srv_conn->socket, mem_ctx);
}

struct dcerpc_read_ncacn_packet_state {
	struct {
		struct smb_iconv_convenience *smb_iconv_c;
	} caller;
	DATA_BLOB buffer;
	struct ncacn_packet *pkt;
};

static int dcerpc_read_ncacn_packet_next_vector(struct tstream_context *stream,
						void *private_data,
						TALLOC_CTX *mem_ctx,
						struct iovec **_vector,
						size_t *_count);
static void dcerpc_read_ncacn_packet_done(struct tevent_req *subreq);

static struct tevent_req *dcerpc_read_ncacn_packet_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct tstream_context *stream,
						 struct smb_iconv_convenience *ic)
{
	struct tevent_req *req;
	struct dcerpc_read_ncacn_packet_state *state;
	struct tevent_req *subreq;

	req = tevent_req_create(mem_ctx, &state,
				struct dcerpc_read_ncacn_packet_state);
	if (req == NULL) {
		return NULL;
	}

	state->caller.smb_iconv_c = ic;
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

	vector[0].iov_base = state->buffer.data + ofs;
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

	ndr = ndr_pull_init_blob(&state->buffer,
				 state->pkt,
				 state->caller.smb_iconv_c);
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

	tevent_req_done(req);
}

static NTSTATUS dcerpc_read_ncacn_packet_recv(struct tevent_req *req,
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

static void dcesrv_read_fragment_done(struct tevent_req *subreq);

static void dcesrv_sock_accept(struct stream_connection *srv_conn)
{
	NTSTATUS status;
	struct dcesrv_socket_context *dcesrv_sock = 
		talloc_get_type(srv_conn->private_data, struct dcesrv_socket_context);
	struct dcesrv_connection *dcesrv_conn = NULL;
	int ret;
	struct tevent_req *subreq;
	struct loadparm_context *lp_ctx = dcesrv_sock->dcesrv_ctx->lp_ctx;

	if (!srv_conn->session_info) {
		status = auth_anonymous_session_info(srv_conn,
						     srv_conn->event.ctx,
						     lp_ctx,
						     &srv_conn->session_info);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0,("dcesrv_sock_accept: auth_anonymous_session_info failed: %s\n",
				nt_errstr(status)));
			stream_terminate_connection(srv_conn, nt_errstr(status));
			return;
		}
	}

	status = dcesrv_endpoint_connect(dcesrv_sock->dcesrv_ctx,
					 srv_conn,
					 dcesrv_sock->endpoint,
					 srv_conn->session_info,
					 srv_conn->event.ctx,
					 srv_conn->msg_ctx,
					 srv_conn->server_id,
					 DCESRV_CALL_STATE_FLAG_MAY_ASYNC,
					 &dcesrv_conn);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("dcesrv_sock_accept: dcesrv_endpoint_connect failed: %s\n", 
			nt_errstr(status)));
		stream_terminate_connection(srv_conn, nt_errstr(status));
		return;
	}

	dcesrv_conn->transport.private_data		= srv_conn;
	dcesrv_conn->transport.report_output_data	= dcesrv_sock_report_output_data;
	dcesrv_conn->transport.get_my_addr		= dcesrv_sock_get_my_addr;
	dcesrv_conn->transport.get_peer_addr		= dcesrv_sock_get_peer_addr;

	TALLOC_FREE(srv_conn->event.fde);

	dcesrv_conn->send_queue = tevent_queue_create(dcesrv_conn, "dcesrv send queue");
	if (!dcesrv_conn->send_queue) {
		status = NT_STATUS_NO_MEMORY;
		DEBUG(0,("dcesrv_sock_accept: tevent_queue_create(%s)\n",
			nt_errstr(status)));
		stream_terminate_connection(srv_conn, nt_errstr(status));
		return;
	}

	if (dcesrv_sock->endpoint->ep_description->transport == NCACN_NP) {
		dcesrv_conn->auth_state.session_key = dcesrv_inherited_session_key;
		ret = tstream_npa_existing_socket(dcesrv_conn,
						  socket_get_fd(srv_conn->socket),
						  FILE_TYPE_MESSAGE_MODE_PIPE,
						  &dcesrv_conn->stream);
	} else {
		ret = tstream_bsd_existing_socket(dcesrv_conn,
						  socket_get_fd(srv_conn->socket),
						  &dcesrv_conn->stream);
	}
	if (ret == -1) {
		status = map_nt_error_from_unix(errno);
		DEBUG(0,("dcesrv_sock_accept: failed to setup tstream: %s\n",
			nt_errstr(status)));
		stream_terminate_connection(srv_conn, nt_errstr(status));
		return;
	}

	srv_conn->private_data = dcesrv_conn;

	irpc_add_name(srv_conn->msg_ctx, "rpc_server");

	subreq = dcerpc_read_ncacn_packet_send(dcesrv_conn,
					       dcesrv_conn->event_ctx,
					       dcesrv_conn->stream,
					       lp_iconv_convenience(lp_ctx));
	if (!subreq) {
		status = NT_STATUS_NO_MEMORY;
		DEBUG(0,("dcesrv_sock_accept: dcerpc_read_fragment_buffer_send(%s)\n",
			nt_errstr(status)));
		stream_terminate_connection(srv_conn, nt_errstr(status));
		return;
	}
	tevent_req_set_callback(subreq, dcesrv_read_fragment_done, dcesrv_conn);

	return;
}

static void dcesrv_read_fragment_done(struct tevent_req *subreq)
{
	struct dcesrv_connection *dce_conn = tevent_req_callback_data(subreq,
					     struct dcesrv_connection);
	struct ncacn_packet *pkt;
	DATA_BLOB buffer;
	NTSTATUS status;
	struct loadparm_context *lp_ctx = dce_conn->dce_ctx->lp_ctx;

	status = dcerpc_read_ncacn_packet_recv(subreq, dce_conn,
					       &pkt, &buffer);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		dcesrv_terminate_connection(dce_conn, nt_errstr(status));
		return;
	}

	status = dcesrv_process_ncacn_packet(dce_conn, pkt, buffer);
	if (!NT_STATUS_IS_OK(status)) {
		dcesrv_terminate_connection(dce_conn, nt_errstr(status));
		return;
	}

	subreq = dcerpc_read_ncacn_packet_send(dce_conn,
					       dce_conn->event_ctx,
					       dce_conn->stream,
					       lp_iconv_convenience(lp_ctx));
	if (!subreq) {
		status = NT_STATUS_NO_MEMORY;
		dcesrv_terminate_connection(dce_conn, nt_errstr(status));
		return;
	}
	tevent_req_set_callback(subreq, dcesrv_read_fragment_done, dce_conn);
}

static void dcesrv_sock_recv(struct stream_connection *conn, uint16_t flags)
{
	struct dcesrv_connection *dce_conn = talloc_get_type(conn->private_data,
					     struct dcesrv_connection);
	dcesrv_terminate_connection(dce_conn, "dcesrv_sock_recv triggered");
}

static void dcesrv_sock_send(struct stream_connection *conn, uint16_t flags)
{
	struct dcesrv_connection *dce_conn = talloc_get_type(conn->private_data,
					     struct dcesrv_connection);
	dcesrv_terminate_connection(dce_conn, "dcesrv_sock_send triggered");
}


static const struct stream_server_ops dcesrv_stream_ops = {
	.name			= "rpc",
	.accept_connection	= dcesrv_sock_accept,
	.recv_handler		= dcesrv_sock_recv,
	.send_handler		= dcesrv_sock_send,
};



static NTSTATUS dcesrv_add_ep_unix(struct dcesrv_context *dce_ctx, 
				   struct loadparm_context *lp_ctx,
				   struct dcesrv_endpoint *e,
			    struct tevent_context *event_ctx, const struct model_ops *model_ops)
{
	struct dcesrv_socket_context *dcesrv_sock;
	uint16_t port = 1;
	NTSTATUS status;

	dcesrv_sock = talloc(event_ctx, struct dcesrv_socket_context);
	NT_STATUS_HAVE_NO_MEMORY(dcesrv_sock);

	/* remember the endpoint of this socket */
	dcesrv_sock->endpoint		= e;
	dcesrv_sock->dcesrv_ctx		= talloc_reference(dcesrv_sock, dce_ctx);

	status = stream_setup_socket(event_ctx, lp_ctx,
				     model_ops, &dcesrv_stream_ops, 
				     "unix", e->ep_description->endpoint, &port, 
				     lp_socket_options(lp_ctx), 
				     dcesrv_sock);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("service_setup_stream_socket(path=%s) failed - %s\n",
			 e->ep_description->endpoint, nt_errstr(status)));
	}

	return status;
}

static NTSTATUS dcesrv_add_ep_ncalrpc(struct dcesrv_context *dce_ctx, 
				      struct loadparm_context *lp_ctx,
				      struct dcesrv_endpoint *e,
				      struct tevent_context *event_ctx, const struct model_ops *model_ops)
{
	struct dcesrv_socket_context *dcesrv_sock;
	uint16_t port = 1;
	char *full_path;
	NTSTATUS status;

	if (!e->ep_description->endpoint) {
		/* No identifier specified: use DEFAULT. 
		 * DO NOT hardcode this value anywhere else. Rather, specify 
		 * no endpoint and let the epmapper worry about it. */
		e->ep_description->endpoint = talloc_strdup(dce_ctx, "DEFAULT");
	}

	full_path = talloc_asprintf(dce_ctx, "%s/%s", lp_ncalrpc_dir(lp_ctx), 
				    e->ep_description->endpoint);

	dcesrv_sock = talloc(event_ctx, struct dcesrv_socket_context);
	NT_STATUS_HAVE_NO_MEMORY(dcesrv_sock);

	/* remember the endpoint of this socket */
	dcesrv_sock->endpoint		= e;
	dcesrv_sock->dcesrv_ctx		= talloc_reference(dcesrv_sock, dce_ctx);

	status = stream_setup_socket(event_ctx, lp_ctx,
				     model_ops, &dcesrv_stream_ops, 
				     "unix", full_path, &port, 
				     lp_socket_options(lp_ctx), 
				     dcesrv_sock);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("service_setup_stream_socket(identifier=%s,path=%s) failed - %s\n",
			 e->ep_description->endpoint, full_path, nt_errstr(status)));
	}
	return status;
}

static NTSTATUS dcesrv_add_ep_np(struct dcesrv_context *dce_ctx,
				 struct loadparm_context *lp_ctx,
				 struct dcesrv_endpoint *e,
				 struct tevent_context *event_ctx, const struct model_ops *model_ops)
{
	struct dcesrv_socket_context *dcesrv_sock;
	NTSTATUS status;
			
	if (e->ep_description->endpoint == NULL) {
		DEBUG(0, ("Endpoint mandatory for named pipes\n"));
		return NT_STATUS_INVALID_PARAMETER;
	}

	dcesrv_sock = talloc(event_ctx, struct dcesrv_socket_context);
	NT_STATUS_HAVE_NO_MEMORY(dcesrv_sock);

	/* remember the endpoint of this socket */
	dcesrv_sock->endpoint		= e;
	dcesrv_sock->dcesrv_ctx		= talloc_reference(dcesrv_sock, dce_ctx);

	status = stream_setup_named_pipe(event_ctx, lp_ctx,
					 model_ops, &dcesrv_stream_ops,
					 e->ep_description->endpoint, dcesrv_sock);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("stream_setup_named_pipe(pipe=%s) failed - %s\n",
			 e->ep_description->endpoint, nt_errstr(status)));
		return status;
	}

	return NT_STATUS_OK;
}

/*
  add a socket address to the list of events, one event per dcerpc endpoint
*/
static NTSTATUS add_socket_rpc_tcp_iface(struct dcesrv_context *dce_ctx, struct dcesrv_endpoint *e,
					 struct tevent_context *event_ctx, const struct model_ops *model_ops,
					 const char *address)
{
	struct dcesrv_socket_context *dcesrv_sock;
	uint16_t port = 0;
	NTSTATUS status;
			
	if (e->ep_description->endpoint) {
		port = atoi(e->ep_description->endpoint);
	}

	dcesrv_sock = talloc(event_ctx, struct dcesrv_socket_context);
	NT_STATUS_HAVE_NO_MEMORY(dcesrv_sock);

	/* remember the endpoint of this socket */
	dcesrv_sock->endpoint		= e;
	dcesrv_sock->dcesrv_ctx		= talloc_reference(dcesrv_sock, dce_ctx);

	status = stream_setup_socket(event_ctx, dce_ctx->lp_ctx,
				     model_ops, &dcesrv_stream_ops, 
				     "ipv4", address, &port, 
				     lp_socket_options(dce_ctx->lp_ctx), 
				     dcesrv_sock);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("service_setup_stream_socket(address=%s,port=%u) failed - %s\n", 
			 address, port, nt_errstr(status)));
	}

	if (e->ep_description->endpoint == NULL) {
		e->ep_description->endpoint = talloc_asprintf(dce_ctx, "%d", port);
	}

	return status;
}

static NTSTATUS dcesrv_add_ep_tcp(struct dcesrv_context *dce_ctx, 
				  struct loadparm_context *lp_ctx,
				  struct dcesrv_endpoint *e,
				  struct tevent_context *event_ctx, const struct model_ops *model_ops)
{
	NTSTATUS status;

	/* Add TCP/IP sockets */
	if (lp_interfaces(lp_ctx) && lp_bind_interfaces_only(lp_ctx)) {
		int num_interfaces;
		int i;
		struct interface *ifaces;

		load_interfaces(dce_ctx, lp_interfaces(lp_ctx), &ifaces);

		num_interfaces = iface_count(ifaces);
		for(i = 0; i < num_interfaces; i++) {
			const char *address = iface_n_ip(ifaces, i);
			status = add_socket_rpc_tcp_iface(dce_ctx, e, event_ctx, model_ops, address);
			NT_STATUS_NOT_OK_RETURN(status);
		}
	} else {
		status = add_socket_rpc_tcp_iface(dce_ctx, e, event_ctx, model_ops, 
						  lp_socket_address(lp_ctx));
		NT_STATUS_NOT_OK_RETURN(status);
	}

	return NT_STATUS_OK;
}

NTSTATUS dcesrv_add_ep(struct dcesrv_context *dce_ctx,
		       struct loadparm_context *lp_ctx,
		       struct dcesrv_endpoint *e,
		       struct tevent_context *event_ctx,
		       const struct model_ops *model_ops)
{
	switch (e->ep_description->transport) {
	case NCACN_UNIX_STREAM:
		return dcesrv_add_ep_unix(dce_ctx, lp_ctx, e, event_ctx, model_ops);

	case NCALRPC:
		return dcesrv_add_ep_ncalrpc(dce_ctx, lp_ctx, e, event_ctx, model_ops);

	case NCACN_IP_TCP:
		return dcesrv_add_ep_tcp(dce_ctx, lp_ctx, e, event_ctx, model_ops);

	case NCACN_NP:
		return dcesrv_add_ep_np(dce_ctx, lp_ctx, e, event_ctx, model_ops);

	default:
		return NT_STATUS_NOT_SUPPORTED;
	}
}

/*
  open the dcerpc server sockets
*/
static void dcesrv_task_init(struct task_server *task)
{
	NTSTATUS status;
	struct dcesrv_context *dce_ctx;
	struct dcesrv_endpoint *e;
	const struct model_ops *model_ops;

	dcerpc_server_init(task->lp_ctx);

	task_server_set_title(task, "task[dcesrv]");

	/* run the rpc server as a single process to allow for shard
	 * handles, and sharing of ldb contexts */
	model_ops = process_model_startup(task->event_ctx, "single");
	if (!model_ops) goto failed;

	status = dcesrv_init_context(task->event_ctx,
				     task->lp_ctx,
				     lp_dcerpc_endpoint_servers(task->lp_ctx),
				     &dce_ctx);
	if (!NT_STATUS_IS_OK(status)) goto failed;

	/* Make sure the directory for NCALRPC exists */
	if (!directory_exist(lp_ncalrpc_dir(task->lp_ctx))) {
		mkdir(lp_ncalrpc_dir(task->lp_ctx), 0755);
	}

	for (e=dce_ctx->endpoint_list;e;e=e->next) {
		status = dcesrv_add_ep(dce_ctx, task->lp_ctx, e, task->event_ctx, model_ops);
		if (!NT_STATUS_IS_OK(status)) goto failed;
	}

	return;
failed:
	task_server_terminate(task, "Failed to startup dcerpc server task", true);	
}

NTSTATUS server_service_rpc_init(void)
{

	return register_server_service("rpc", dcesrv_task_init);
}
