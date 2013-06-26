/* 
   Unix SMB/CIFS implementation.
   
   WINS Replication server
   
   Copyright (C) Stefan Metzmacher	2005
   
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
#include "lib/socket/socket.h"
#include "lib/stream/packet.h"
#include "smbd/service_task.h"
#include "smbd/service_stream.h"
#include "smbd/service.h"
#include "lib/messaging/irpc.h"
#include "librpc/gen_ndr/ndr_winsrepl.h"
#include "wrepl_server/wrepl_server.h"
#include "smbd/process_model.h"
#include "system/network.h"
#include "lib/socket/netif.h"
#include "lib/tsocket/tsocket.h"
#include "libcli/util/tstream.h"
#include "param/param.h"

void wreplsrv_terminate_in_connection(struct wreplsrv_in_connection *wreplconn, const char *reason)
{
	stream_terminate_connection(wreplconn->conn, reason);
}

/*
  receive some data on a WREPL connection
*/
static NTSTATUS wreplsrv_process(struct wreplsrv_in_connection *wrepl_conn,
				 struct wreplsrv_in_call **_call)
{
	struct wrepl_wrap packet_out_wrap;
	NTSTATUS status;
	enum ndr_err_code ndr_err;
	struct wreplsrv_in_call *call = *_call;

	ndr_err = ndr_pull_struct_blob(&call->in, call,
				       &call->req_packet,
				       (ndr_pull_flags_fn_t)ndr_pull_wrepl_packet);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return ndr_map_error2ntstatus(ndr_err);
	}

	if (DEBUGLVL(10)) {
		DEBUG(10,("Received WINS-Replication packet of length %u\n",
			  (unsigned int) call->in.length + 4));
		NDR_PRINT_DEBUG(wrepl_packet, &call->req_packet);
	}

	status = wreplsrv_in_call(call);
	NT_STATUS_IS_ERR_RETURN(status);
	if (!NT_STATUS_IS_OK(status)) {
		/* w2k just ignores invalid packets, so we do */
		DEBUG(10,("Received WINS-Replication packet was invalid, we just ignore it\n"));
		TALLOC_FREE(call);
		*_call = NULL;
		return NT_STATUS_OK;
	}

	/* and now encode the reply */
	packet_out_wrap.packet = call->rep_packet;
	ndr_err = ndr_push_struct_blob(&call->out, call,
				       &packet_out_wrap,
				       (ndr_push_flags_fn_t) ndr_push_wrepl_wrap);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return ndr_map_error2ntstatus(ndr_err);
	}

	if (DEBUGLVL(10)) {
		DEBUG(10,("Sending WINS-Replication packet of length %u\n",
			 (unsigned int) call->out.length));
		NDR_PRINT_DEBUG(wrepl_packet, &call->rep_packet);
	}

	return NT_STATUS_OK;
}

static void wreplsrv_call_loop(struct tevent_req *subreq);

/*
  called when we get a new connection
*/
static void wreplsrv_accept(struct stream_connection *conn)
{
	struct wreplsrv_service *service = talloc_get_type(conn->private_data, struct wreplsrv_service);
	struct wreplsrv_in_connection *wrepl_conn;
	struct tsocket_address *peer_addr;
	char *peer_ip;
	struct tevent_req *subreq;
	int rc;

	wrepl_conn = talloc_zero(conn, struct wreplsrv_in_connection);
	if (wrepl_conn == NULL) {
		stream_terminate_connection(conn,
					    "wreplsrv_accept: out of memory");
		return;
	}

	wrepl_conn->send_queue = tevent_queue_create(conn, "wrepl_accept");
	if (wrepl_conn->send_queue == NULL) {
		stream_terminate_connection(conn,
					    "wrepl_accept: out of memory");
		return;
	}

	TALLOC_FREE(conn->event.fde);

	rc = tstream_bsd_existing_socket(wrepl_conn,
					 socket_get_fd(conn->socket),
					 &wrepl_conn->tstream);
	if (rc < 0) {
		stream_terminate_connection(conn,
					    "wrepl_accept: out of memory");
		return;
	}
	socket_set_flags(conn->socket, SOCKET_FLAG_NOCLOSE);

	wrepl_conn->conn = conn;
	wrepl_conn->service = service;

	peer_addr = conn->remote_address;

	if (!tsocket_address_is_inet(peer_addr, "ipv4")) {
		DEBUG(0,("wreplsrv_accept: non ipv4 peer addr '%s'\n",
			tsocket_address_string(peer_addr, wrepl_conn)));
		wreplsrv_terminate_in_connection(wrepl_conn, "wreplsrv_accept: "
				"invalid peer IP");
		return;
	}

	peer_ip = tsocket_address_inet_addr_string(peer_addr, wrepl_conn);
	if (peer_ip == NULL) {
		wreplsrv_terminate_in_connection(wrepl_conn, "wreplsrv_accept: "
				"could not convert peer IP into a string");
		return;
	}

	wrepl_conn->partner = wreplsrv_find_partner(service, peer_ip);
	irpc_add_name(conn->msg_ctx, "wreplsrv_connection");

	/*
	 * The wrepl pdu's has the length as 4 byte (initial_read_size),
	 * packet_full_request_u32 provides the pdu length then.
	 */
	subreq = tstream_read_pdu_blob_send(wrepl_conn,
					    wrepl_conn->conn->event.ctx,
					    wrepl_conn->tstream,
					    4, /* initial_read_size */
					    packet_full_request_u32,
					    wrepl_conn);
	if (subreq == NULL) {
		wreplsrv_terminate_in_connection(wrepl_conn, "wrepl_accept: "
				"no memory for tstream_read_pdu_blob_send");
		return;
	}
	tevent_req_set_callback(subreq, wreplsrv_call_loop, wrepl_conn);
}

static void wreplsrv_call_writev_done(struct tevent_req *subreq);

static void wreplsrv_call_loop(struct tevent_req *subreq)
{
	struct wreplsrv_in_connection *wrepl_conn = tevent_req_callback_data(subreq,
				      struct wreplsrv_in_connection);
	struct wreplsrv_in_call *call;
	NTSTATUS status;

	call = talloc_zero(wrepl_conn, struct wreplsrv_in_call);
	if (call == NULL) {
		wreplsrv_terminate_in_connection(wrepl_conn, "wreplsrv_call_loop: "
				"no memory for wrepl_samba3_call");
		return;
	}
	call->wreplconn = wrepl_conn;

	status = tstream_read_pdu_blob_recv(subreq,
					    call,
					    &call->in);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		const char *reason;

		reason = talloc_asprintf(call, "wreplsrv_call_loop: "
					 "tstream_read_pdu_blob_recv() - %s",
					 nt_errstr(status));
		if (!reason) {
			reason = nt_errstr(status);
		}

		wreplsrv_terminate_in_connection(wrepl_conn, reason);
		return;
	}

	DEBUG(10,("Received wrepl packet of length %lu from %s\n",
		 (long) call->in.length,
		 tsocket_address_string(wrepl_conn->conn->remote_address, call)));

	/* skip length header */
	call->in.data += 4;
	call->in.length -= 4;

	status = wreplsrv_process(wrepl_conn, &call);
	if (!NT_STATUS_IS_OK(status)) {
		const char *reason;

		reason = talloc_asprintf(call, "wreplsrv_call_loop: "
					 "tstream_read_pdu_blob_recv() - %s",
					 nt_errstr(status));
		if (reason == NULL) {
			reason = nt_errstr(status);
		}

		wreplsrv_terminate_in_connection(wrepl_conn, reason);
		return;
	}

	/* We handed over the connection so we're done here */
	if (wrepl_conn->tstream == NULL) {
	    return;
	}

	/* Invalid WINS-Replication packet, we just ignore it */
	if (call == NULL) {
		goto noreply;
	}

	call->out_iov[0].iov_base = (char *) call->out.data;
	call->out_iov[0].iov_len = call->out.length;

	subreq = tstream_writev_queue_send(call,
					   wrepl_conn->conn->event.ctx,
					   wrepl_conn->tstream,
					   wrepl_conn->send_queue,
					   call->out_iov, 1);
	if (subreq == NULL) {
		wreplsrv_terminate_in_connection(wrepl_conn, "wreplsrv_call_loop: "
				"no memory for tstream_writev_queue_send");
		return;
	}
	tevent_req_set_callback(subreq, wreplsrv_call_writev_done, call);

noreply:
	/*
	 * The wrepl pdu's has the length as 4 byte (initial_read_size),
	 *  provides the pdu length then.
	 */
	subreq = tstream_read_pdu_blob_send(wrepl_conn,
					    wrepl_conn->conn->event.ctx,
					    wrepl_conn->tstream,
					    4, /* initial_read_size */
					    packet_full_request_u32,
					    wrepl_conn);
	if (subreq == NULL) {
		wreplsrv_terminate_in_connection(wrepl_conn, "wreplsrv_call_loop: "
				"no memory for tstream_read_pdu_blob_send");
		return;
	}
	tevent_req_set_callback(subreq, wreplsrv_call_loop, wrepl_conn);
}

static void wreplsrv_call_writev_done(struct tevent_req *subreq)
{
	struct wreplsrv_in_call *call = tevent_req_callback_data(subreq,
			struct wreplsrv_in_call);
	int sys_errno;
	int rc;

	rc = tstream_writev_queue_recv(subreq, &sys_errno);
	TALLOC_FREE(subreq);
	if (rc == -1) {
		const char *reason;

		reason = talloc_asprintf(call, "wreplsrv_call_writev_done: "
					 "tstream_writev_queue_recv() - %d:%s",
					 sys_errno, strerror(sys_errno));
		if (reason == NULL) {
			reason = "wreplsrv_call_writev_done: "
				 "tstream_writev_queue_recv() failed";
		}

		wreplsrv_terminate_in_connection(call->wreplconn, reason);
		return;
	}

	if (call->terminate_after_send) {
		wreplsrv_terminate_in_connection(call->wreplconn,
				"wreplsrv_in_connection: terminate_after_send");
		return;
	}

	talloc_free(call);
}

/*
  called on a tcp recv
*/
static void wreplsrv_recv(struct stream_connection *conn, uint16_t flags)
{
	struct wreplsrv_in_connection *wrepl_conn = talloc_get_type(conn->private_data,
							struct wreplsrv_in_connection);
	/* this should never be triggered! */
	DEBUG(0,("Terminating connection - '%s'\n", "wrepl_recv: called"));
	wreplsrv_terminate_in_connection(wrepl_conn, "wrepl_recv: called");
}

/*
  called when we can write to a connection
*/
static void wreplsrv_send(struct stream_connection *conn, uint16_t flags)
{
	struct wreplsrv_in_connection *wrepl_conn = talloc_get_type(conn->private_data,
							struct wreplsrv_in_connection);
	/* this should never be triggered! */
	DEBUG(0,("Terminating connection - '%s'\n", "wrepl_send: called"));
	wreplsrv_terminate_in_connection(wrepl_conn, "wrepl_send: called");
}

static const struct stream_server_ops wreplsrv_stream_ops = {
	.name			= "wreplsrv",
	.accept_connection	= wreplsrv_accept,
	.recv_handler		= wreplsrv_recv,
	.send_handler		= wreplsrv_send,
};

/*
  called when we get a new connection
*/
NTSTATUS wreplsrv_in_connection_merge(struct wreplsrv_partner *partner,
				      uint32_t peer_assoc_ctx,
				      struct tstream_context **stream,
				      struct wreplsrv_in_connection **_wrepl_in)
{
	struct wreplsrv_service *service = partner->service;
	struct wreplsrv_in_connection *wrepl_in;
	const struct model_ops *model_ops;
	struct stream_connection *conn;
	struct tevent_req *subreq;
	NTSTATUS status;

	/* within the wrepl task we want to be a single process, so
	   ask for the single process model ops and pass these to the
	   stream_setup_socket() call. */
	model_ops = process_model_startup("single");
	if (!model_ops) {
		DEBUG(0,("Can't find 'single' process model_ops"));
		return NT_STATUS_INTERNAL_ERROR;
	}

	wrepl_in = talloc_zero(partner, struct wreplsrv_in_connection);
	NT_STATUS_HAVE_NO_MEMORY(wrepl_in);

	wrepl_in->service	= service;
	wrepl_in->partner	= partner;
	wrepl_in->tstream	= talloc_move(wrepl_in, stream);
	wrepl_in->assoc_ctx.peer_ctx = peer_assoc_ctx;

	status = stream_new_connection_merge(service->task->event_ctx,
					     service->task->lp_ctx,
					     model_ops,
					     &wreplsrv_stream_ops,
					     service->task->msg_ctx,
					     wrepl_in,
					     &conn);
	NT_STATUS_NOT_OK_RETURN(status);

	/*
	 * make the wreplsrv_in_connection structure a child of the
	 * stream_connection, to match the hierarchy of wreplsrv_accept
	 */
	wrepl_in->conn		= conn;
	talloc_steal(conn, wrepl_in);

	wrepl_in->send_queue = tevent_queue_create(wrepl_in, "wreplsrv_in_connection_merge");
	if (wrepl_in->send_queue == NULL) {
		stream_terminate_connection(conn,
					    "wreplsrv_in_connection_merge: out of memory");
		return NT_STATUS_NO_MEMORY;
	}

	/*
	 * The wrepl pdu's has the length as 4 byte (initial_read_size),
	 * packet_full_request_u32 provides the pdu length then.
	 */
	subreq = tstream_read_pdu_blob_send(wrepl_in,
					    wrepl_in->conn->event.ctx,
					    wrepl_in->tstream,
					    4, /* initial_read_size */
					    packet_full_request_u32,
					    wrepl_in);
	if (subreq == NULL) {
		wreplsrv_terminate_in_connection(wrepl_in, "wreplsrv_in_connection_merge: "
				"no memory for tstream_read_pdu_blob_send");
		return NT_STATUS_NO_MEMORY;
	}
	tevent_req_set_callback(subreq, wreplsrv_call_loop, wrepl_in);

	*_wrepl_in = wrepl_in;

	return NT_STATUS_OK;
}

/*
  startup the wrepl port 42 server sockets
*/
NTSTATUS wreplsrv_setup_sockets(struct wreplsrv_service *service, struct loadparm_context *lp_ctx)
{
	NTSTATUS status;
	struct task_server *task = service->task;
	const struct model_ops *model_ops;
	const char *address;
	uint16_t port = WINS_REPLICATION_PORT;

	/* within the wrepl task we want to be a single process, so
	   ask for the single process model ops and pass these to the
	   stream_setup_socket() call. */
	model_ops = process_model_startup("single");
	if (!model_ops) {
		DEBUG(0,("Can't find 'single' process model_ops"));
		return NT_STATUS_INTERNAL_ERROR;
	}

	if (lpcfg_interfaces(lp_ctx) && lpcfg_bind_interfaces_only(lp_ctx)) {
		int num_interfaces;
		int i;
		struct interface *ifaces;

		load_interfaces(task, lpcfg_interfaces(lp_ctx), &ifaces);

		num_interfaces = iface_count(ifaces);

		/* We have been given an interfaces line, and been 
		   told to only bind to those interfaces. Create a
		   socket per interface and bind to only these.
		*/
		for(i = 0; i < num_interfaces; i++) {
			address = iface_n_ip(ifaces, i);
			status = stream_setup_socket(task, task->event_ctx,
						     task->lp_ctx, model_ops,
						     &wreplsrv_stream_ops,
						     "ipv4", address, &port, 
					              lpcfg_socket_options(task->lp_ctx),
						     service);
			if (!NT_STATUS_IS_OK(status)) {
				DEBUG(0,("stream_setup_socket(address=%s,port=%u) failed - %s\n",
					 address, port, nt_errstr(status)));
				return status;
			}
		}
	} else {
		address = lpcfg_socket_address(lp_ctx);
		status = stream_setup_socket(task, task->event_ctx, task->lp_ctx,
					     model_ops, &wreplsrv_stream_ops,
					     "ipv4", address, &port, lpcfg_socket_options(task->lp_ctx),
					     service);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0,("stream_setup_socket(address=%s,port=%u) failed - %s\n",
				 address, port, nt_errstr(status)));
			return status;
		}
	}

	return NT_STATUS_OK;
}
