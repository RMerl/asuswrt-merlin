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
#include "param/param.h"

void wreplsrv_terminate_in_connection(struct wreplsrv_in_connection *wreplconn, const char *reason)
{
	stream_terminate_connection(wreplconn->conn, reason);
}

static int terminate_after_send_destructor(struct wreplsrv_in_connection **tas)
{
	wreplsrv_terminate_in_connection(*tas, "wreplsrv_in_connection: terminate_after_send");
	return 0;
}

/*
  receive some data on a WREPL connection
*/
static NTSTATUS wreplsrv_recv_request(void *private_data, DATA_BLOB blob)
{
	struct wreplsrv_in_connection *wreplconn = talloc_get_type(private_data, struct wreplsrv_in_connection);
	struct wreplsrv_in_call *call;
	DATA_BLOB packet_in_blob;
	DATA_BLOB packet_out_blob;
	struct wrepl_wrap packet_out_wrap;
	NTSTATUS status;
	enum ndr_err_code ndr_err;

	call = talloc_zero(wreplconn, struct wreplsrv_in_call);
	NT_STATUS_HAVE_NO_MEMORY(call);
	call->wreplconn = wreplconn;
	talloc_steal(call, blob.data);

	packet_in_blob.data = blob.data + 4;
	packet_in_blob.length = blob.length - 4;

	ndr_err = ndr_pull_struct_blob(&packet_in_blob, call, 
				       lp_iconv_convenience(wreplconn->service->task->lp_ctx),
				       &call->req_packet,
				       (ndr_pull_flags_fn_t)ndr_pull_wrepl_packet);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return ndr_map_error2ntstatus(ndr_err);
	}

	if (DEBUGLVL(10)) {
		DEBUG(10,("Received WINS-Replication packet of length %u\n", 
			  (unsigned)packet_in_blob.length + 4));
		NDR_PRINT_DEBUG(wrepl_packet, &call->req_packet);
	}

	status = wreplsrv_in_call(call);
	NT_STATUS_IS_ERR_RETURN(status);
	if (!NT_STATUS_IS_OK(status)) {
		/* w2k just ignores invalid packets, so we do */
		DEBUG(10,("Received WINS-Replication packet was invalid, we just ignore it\n"));
		talloc_free(call);
		return NT_STATUS_OK;
	}

	/* and now encode the reply */
	packet_out_wrap.packet = call->rep_packet;
	ndr_err = ndr_push_struct_blob(&packet_out_blob, call, 
				       lp_iconv_convenience(wreplconn->service->task->lp_ctx),
				       &packet_out_wrap,
				      (ndr_push_flags_fn_t)ndr_push_wrepl_wrap);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return ndr_map_error2ntstatus(ndr_err);
	}

	if (DEBUGLVL(10)) {
		DEBUG(10,("Sending WINS-Replication packet of length %d\n", (int)packet_out_blob.length));
		NDR_PRINT_DEBUG(wrepl_packet, &call->rep_packet);
	}

	if (call->terminate_after_send) {
		struct wreplsrv_in_connection **tas;
		tas = talloc(packet_out_blob.data, struct wreplsrv_in_connection *);
		NT_STATUS_HAVE_NO_MEMORY(tas);
		*tas = wreplconn;
		talloc_set_destructor(tas, terminate_after_send_destructor);
	}

	status = packet_send(wreplconn->packet, packet_out_blob);
	NT_STATUS_NOT_OK_RETURN(status);

	talloc_free(call);
	return NT_STATUS_OK;
}

/*
  called when the socket becomes readable
*/
static void wreplsrv_recv(struct stream_connection *conn, uint16_t flags)
{
	struct wreplsrv_in_connection *wreplconn = talloc_get_type(conn->private_data,
								   struct wreplsrv_in_connection);

	packet_recv(wreplconn->packet);
}

/*
  called when the socket becomes writable
*/
static void wreplsrv_send(struct stream_connection *conn, uint16_t flags)
{
	struct wreplsrv_in_connection *wreplconn = talloc_get_type(conn->private_data,
								   struct wreplsrv_in_connection);
	packet_queue_run(wreplconn->packet);
}

/*
  handle socket recv errors
*/
static void wreplsrv_recv_error(void *private_data, NTSTATUS status)
{
	struct wreplsrv_in_connection *wreplconn = talloc_get_type(private_data,
								   struct wreplsrv_in_connection);
	wreplsrv_terminate_in_connection(wreplconn, nt_errstr(status));
}

/*
  called when we get a new connection
*/
static void wreplsrv_accept(struct stream_connection *conn)
{
	struct wreplsrv_service *service = talloc_get_type(conn->private_data, struct wreplsrv_service);
	struct wreplsrv_in_connection *wreplconn;
	struct socket_address *peer_ip;

	wreplconn = talloc_zero(conn, struct wreplsrv_in_connection);
	if (!wreplconn) {
		stream_terminate_connection(conn, "wreplsrv_accept: out of memory");
		return;
	}

	wreplconn->packet = packet_init(wreplconn);
	if (!wreplconn->packet) {
		wreplsrv_terminate_in_connection(wreplconn, "wreplsrv_accept: out of memory");
		return;
	}
	packet_set_private(wreplconn->packet, wreplconn);
	packet_set_socket(wreplconn->packet, conn->socket);
	packet_set_callback(wreplconn->packet, wreplsrv_recv_request);
	packet_set_full_request(wreplconn->packet, packet_full_request_u32);
	packet_set_error_handler(wreplconn->packet, wreplsrv_recv_error);
	packet_set_event_context(wreplconn->packet, conn->event.ctx);
	packet_set_fde(wreplconn->packet, conn->event.fde);
	packet_set_serialise(wreplconn->packet);

	wreplconn->conn		= conn;
	wreplconn->service	= service;

	peer_ip	= socket_get_peer_addr(conn->socket, wreplconn);
	if (!peer_ip) {
		wreplsrv_terminate_in_connection(wreplconn, "wreplsrv_accept: could not obtain peer IP from kernel");
		return;
	}

	wreplconn->partner	= wreplsrv_find_partner(service, peer_ip->addr);

	conn->private_data = wreplconn;

	irpc_add_name(conn->msg_ctx, "wreplsrv_connection");
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
				      struct socket_context *sock,
				      struct packet_context *packet,
				      struct wreplsrv_in_connection **_wrepl_in)
{
	struct wreplsrv_service *service = partner->service;
	struct wreplsrv_in_connection *wrepl_in;
	const struct model_ops *model_ops;
	struct stream_connection *conn;
	NTSTATUS status;

	/* within the wrepl task we want to be a single process, so
	   ask for the single process model ops and pass these to the
	   stream_setup_socket() call. */
	model_ops = process_model_startup(service->task->event_ctx, "single");
	if (!model_ops) {
		DEBUG(0,("Can't find 'single' process model_ops"));
		return NT_STATUS_INTERNAL_ERROR;
	}

	wrepl_in = talloc_zero(partner, struct wreplsrv_in_connection);
	NT_STATUS_HAVE_NO_MEMORY(wrepl_in);

	wrepl_in->service	= service;
	wrepl_in->partner	= partner;

	status = stream_new_connection_merge(service->task->event_ctx, service->task->lp_ctx, model_ops,
					     sock, &wreplsrv_stream_ops, service->task->msg_ctx,
					     wrepl_in, &conn);
	NT_STATUS_NOT_OK_RETURN(status);

	/*
	 * make the wreplsrv_in_connection structure a child of the
	 * stream_connection, to match the hierarchy of wreplsrv_accept
	 */
	wrepl_in->conn		= conn;
	talloc_steal(conn, wrepl_in);

	/*
	 * now update the packet handling callback,...
	 */
	wrepl_in->packet	= talloc_steal(wrepl_in, packet);
	packet_set_private(wrepl_in->packet, wrepl_in);
	packet_set_socket(wrepl_in->packet, conn->socket);
	packet_set_callback(wrepl_in->packet, wreplsrv_recv_request);
	packet_set_full_request(wrepl_in->packet, packet_full_request_u32);
	packet_set_error_handler(wrepl_in->packet, wreplsrv_recv_error);
	packet_set_event_context(wrepl_in->packet, conn->event.ctx);
	packet_set_fde(wrepl_in->packet, conn->event.fde);
	packet_set_serialise(wrepl_in->packet);

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
	model_ops = process_model_startup(task->event_ctx, "single");
	if (!model_ops) {
		DEBUG(0,("Can't find 'single' process model_ops"));
		return NT_STATUS_INTERNAL_ERROR;
	}

	if (lp_interfaces(lp_ctx) && lp_bind_interfaces_only(lp_ctx)) {
		int num_interfaces;
		int i;
		struct interface *ifaces;

		load_interfaces(task, lp_interfaces(lp_ctx), &ifaces);

		num_interfaces = iface_count(ifaces);

		/* We have been given an interfaces line, and been 
		   told to only bind to those interfaces. Create a
		   socket per interface and bind to only these.
		*/
		for(i = 0; i < num_interfaces; i++) {
			address = iface_n_ip(ifaces, i);
			status = stream_setup_socket(task->event_ctx, 
						     task->lp_ctx, model_ops, 
						     &wreplsrv_stream_ops,
						     "ipv4", address, &port, 
				     	              lp_socket_options(task->lp_ctx), 
						     service);
			if (!NT_STATUS_IS_OK(status)) {
				DEBUG(0,("stream_setup_socket(address=%s,port=%u) failed - %s\n",
					 address, port, nt_errstr(status)));
				return status;
			}
		}
	} else {
		address = lp_socket_address(lp_ctx);
		status = stream_setup_socket(task->event_ctx, task->lp_ctx, 
					     model_ops, &wreplsrv_stream_ops,
					     "ipv4", address, &port, lp_socket_options(task->lp_ctx), 
					     service);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0,("stream_setup_socket(address=%s,port=%u) failed - %s\n",
				 address, port, nt_errstr(status)));
			return status;
		}
	}

	return NT_STATUS_OK;
}
