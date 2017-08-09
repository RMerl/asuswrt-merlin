/*
   Unix SMB/CIFS implementation.

   KDC Server startup

   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2005-2008
   Copyright (C) Andrew Tridgell	2005
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
#include "smbd/process_model.h"
#include "lib/tsocket/tsocket.h"
#include "libcli/util/tstream.h"
#include "lib/messaging/irpc.h"
#include "librpc/gen_ndr/ndr_irpc.h"
#include "librpc/gen_ndr/ndr_krb5pac.h"
#include "lib/stream/packet.h"
#include "lib/socket/netif.h"
#include "param/param.h"
#include "kdc/kdc-glue.h"
#include "dsdb/samdb/samdb.h"
#include "auth/session.h"

extern struct krb5plugin_windc_ftable windc_plugin_table;
extern struct hdb_method hdb_samba4;

static NTSTATUS kdc_proxy_unavailable_error(struct kdc_server *kdc,
					    TALLOC_CTX *mem_ctx,
					    DATA_BLOB *out)
{
	int kret;
	krb5_data k5_error_blob;

	kret = krb5_mk_error(kdc->smb_krb5_context->krb5_context,
			     KRB5KDC_ERR_SVC_UNAVAILABLE, NULL, NULL,
			     NULL, NULL, NULL, NULL, &k5_error_blob);
	if (kret != 0) {
		DEBUG(2,(__location__ ": Unable to form krb5 error reply\n"));
		return NT_STATUS_INTERNAL_ERROR;
	}

	*out = data_blob_talloc(mem_ctx, k5_error_blob.data, k5_error_blob.length);
	krb5_data_free(&k5_error_blob);
	if (!out->data) {
		return NT_STATUS_NO_MEMORY;
	}

	return NT_STATUS_OK;
}

typedef enum kdc_process_ret (*kdc_process_fn_t)(struct kdc_server *kdc,
						 TALLOC_CTX *mem_ctx,
						 DATA_BLOB *input,
						 DATA_BLOB *reply,
						 struct tsocket_address *peer_addr,
						 struct tsocket_address *my_addr,
						 int datagram);

/* hold information about one kdc socket */
struct kdc_socket {
	struct kdc_server *kdc;
	struct tsocket_address *local_address;
	kdc_process_fn_t process;
};

struct kdc_tcp_call {
	struct kdc_tcp_connection *kdc_conn;
	DATA_BLOB in;
	DATA_BLOB out;
	uint8_t out_hdr[4];
	struct iovec out_iov[2];
};

/*
  state of an open tcp connection
*/
struct kdc_tcp_connection {
	/* stream connection we belong to */
	struct stream_connection *conn;

	/* the kdc_server the connection belongs to */
	struct kdc_socket *kdc_socket;

	struct tstream_context *tstream;

	struct tevent_queue *send_queue;
};


static void kdc_tcp_terminate_connection(struct kdc_tcp_connection *kdcconn, const char *reason)
{
	stream_terminate_connection(kdcconn->conn, reason);
}

static void kdc_tcp_recv(struct stream_connection *conn, uint16_t flags)
{
	struct kdc_tcp_connection *kdcconn = talloc_get_type(conn->private_data,
							     struct kdc_tcp_connection);
	/* this should never be triggered! */
	kdc_tcp_terminate_connection(kdcconn, "kdc_tcp_recv: called");
}

static void kdc_tcp_send(struct stream_connection *conn, uint16_t flags)
{
	struct kdc_tcp_connection *kdcconn = talloc_get_type(conn->private_data,
							     struct kdc_tcp_connection);
	/* this should never be triggered! */
	kdc_tcp_terminate_connection(kdcconn, "kdc_tcp_send: called");
}

/**
   Wrapper for krb5_kdc_process_krb5_request, converting to/from Samba
   calling conventions
*/

static enum kdc_process_ret kdc_process(struct kdc_server *kdc,
					TALLOC_CTX *mem_ctx,
					DATA_BLOB *input,
					DATA_BLOB *reply,
					struct tsocket_address *peer_addr,
					struct tsocket_address *my_addr,
					int datagram_reply)
{
	int ret;
	char *pa;
	struct sockaddr_storage ss;
	krb5_data k5_reply;
	krb5_data_zero(&k5_reply);

	krb5_kdc_update_time(NULL);

	ret = tsocket_address_bsd_sockaddr(peer_addr, (struct sockaddr *) &ss,
				sizeof(struct sockaddr_storage));
	if (ret < 0) {
		return KDC_PROCESS_FAILED;
	}
	pa = tsocket_address_string(peer_addr, mem_ctx);
	if (pa == NULL) {
		return KDC_PROCESS_FAILED;
	}

	DEBUG(10,("Received KDC packet of length %lu from %s\n",
				(long)input->length - 4, pa));

	ret = krb5_kdc_process_krb5_request(kdc->smb_krb5_context->krb5_context,
					    kdc->config,
					    input->data, input->length,
					    &k5_reply,
					    pa,
					    (struct sockaddr *) &ss,
					    datagram_reply);
	if (ret == -1) {
		*reply = data_blob(NULL, 0);
		return KDC_PROCESS_FAILED;
	}

	if (ret == HDB_ERR_NOT_FOUND_HERE) {
		*reply = data_blob(NULL, 0);
		return KDC_PROCESS_PROXY;
	}

	if (k5_reply.length) {
		*reply = data_blob_talloc(mem_ctx, k5_reply.data, k5_reply.length);
		krb5_data_free(&k5_reply);
	} else {
		*reply = data_blob(NULL, 0);
	}
	return KDC_PROCESS_OK;
}

static void kdc_tcp_call_proxy_done(struct tevent_req *subreq);
static void kdc_tcp_call_writev_done(struct tevent_req *subreq);

static void kdc_tcp_call_loop(struct tevent_req *subreq)
{
	struct kdc_tcp_connection *kdc_conn = tevent_req_callback_data(subreq,
				      struct kdc_tcp_connection);
	struct kdc_tcp_call *call;
	NTSTATUS status;
	enum kdc_process_ret ret;

	call = talloc(kdc_conn, struct kdc_tcp_call);
	if (call == NULL) {
		kdc_tcp_terminate_connection(kdc_conn, "kdc_tcp_call_loop: "
				"no memory for kdc_tcp_call");
		return;
	}
	call->kdc_conn = kdc_conn;

	status = tstream_read_pdu_blob_recv(subreq,
					    call,
					    &call->in);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		const char *reason;

		reason = talloc_asprintf(call, "kdc_tcp_call_loop: "
					 "tstream_read_pdu_blob_recv() - %s",
					 nt_errstr(status));
		if (!reason) {
			reason = nt_errstr(status);
		}

		kdc_tcp_terminate_connection(kdc_conn, reason);
		return;
	}

	DEBUG(10,("Received krb5 TCP packet of length %lu from %s\n",
		 (long) call->in.length,
		 tsocket_address_string(kdc_conn->conn->remote_address, call)));

	/* skip length header */
	call->in.data +=4;
	call->in.length -= 4;

	/* Call krb5 */
	ret = kdc_conn->kdc_socket->process(kdc_conn->kdc_socket->kdc,
					   call,
					   &call->in,
					   &call->out,
					   kdc_conn->conn->remote_address,
					   kdc_conn->conn->local_address,
					   0 /* Stream */);
	if (ret == KDC_PROCESS_FAILED) {
		kdc_tcp_terminate_connection(kdc_conn,
				"kdc_tcp_call_loop: process function failed");
		return;
	}

	if (ret == KDC_PROCESS_PROXY) {
		uint16_t port;

		if (!kdc_conn->kdc_socket->kdc->am_rodc) {
			kdc_tcp_terminate_connection(kdc_conn,
						     "kdc_tcp_call_loop: proxying requested when not RODC");
			return;
		}
		port = tsocket_address_inet_port(kdc_conn->conn->local_address);

		subreq = kdc_tcp_proxy_send(call,
					    kdc_conn->conn->event.ctx,
					    kdc_conn->kdc_socket->kdc,
					    port,
					    call->in);
		if (subreq == NULL) {
			kdc_tcp_terminate_connection(kdc_conn,
				"kdc_tcp_call_loop: kdc_tcp_proxy_send failed");
			return;
		}
		tevent_req_set_callback(subreq, kdc_tcp_call_proxy_done, call);
		return;
	}

	/* First add the length of the out buffer */
	RSIVAL(call->out_hdr, 0, call->out.length);
	call->out_iov[0].iov_base = (char *) call->out_hdr;
	call->out_iov[0].iov_len = 4;

	call->out_iov[1].iov_base = (char *) call->out.data;
	call->out_iov[1].iov_len = call->out.length;

	subreq = tstream_writev_queue_send(call,
					   kdc_conn->conn->event.ctx,
					   kdc_conn->tstream,
					   kdc_conn->send_queue,
					   call->out_iov, 2);
	if (subreq == NULL) {
		kdc_tcp_terminate_connection(kdc_conn, "kdc_tcp_call_loop: "
				"no memory for tstream_writev_queue_send");
		return;
	}
	tevent_req_set_callback(subreq, kdc_tcp_call_writev_done, call);

	/*
	 * The krb5 tcp pdu's has the length as 4 byte (initial_read_size),
	 * packet_full_request_u32 provides the pdu length then.
	 */
	subreq = tstream_read_pdu_blob_send(kdc_conn,
					    kdc_conn->conn->event.ctx,
					    kdc_conn->tstream,
					    4, /* initial_read_size */
					    packet_full_request_u32,
					    kdc_conn);
	if (subreq == NULL) {
		kdc_tcp_terminate_connection(kdc_conn, "kdc_tcp_call_loop: "
				"no memory for tstream_read_pdu_blob_send");
		return;
	}
	tevent_req_set_callback(subreq, kdc_tcp_call_loop, kdc_conn);
}

static void kdc_tcp_call_proxy_done(struct tevent_req *subreq)
{
	struct kdc_tcp_call *call = tevent_req_callback_data(subreq,
			struct kdc_tcp_call);
	struct kdc_tcp_connection *kdc_conn = call->kdc_conn;
	NTSTATUS status;

	status = kdc_tcp_proxy_recv(subreq, call, &call->out);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		/* generate an error packet */
		status = kdc_proxy_unavailable_error(kdc_conn->kdc_socket->kdc,
						     call, &call->out);
	}

	if (!NT_STATUS_IS_OK(status)) {
		const char *reason;

		reason = talloc_asprintf(call, "kdc_tcp_call_proxy_done: "
					 "kdc_proxy_unavailable_error - %s",
					 nt_errstr(status));
		if (!reason) {
			reason = "kdc_tcp_call_proxy_done: kdc_proxy_unavailable_error() failed";
		}

		kdc_tcp_terminate_connection(call->kdc_conn, reason);
		return;
	}

	/* First add the length of the out buffer */
	RSIVAL(call->out_hdr, 0, call->out.length);
	call->out_iov[0].iov_base = (char *) call->out_hdr;
	call->out_iov[0].iov_len = 4;

	call->out_iov[1].iov_base = (char *) call->out.data;
	call->out_iov[1].iov_len = call->out.length;

	subreq = tstream_writev_queue_send(call,
					   kdc_conn->conn->event.ctx,
					   kdc_conn->tstream,
					   kdc_conn->send_queue,
					   call->out_iov, 2);
	if (subreq == NULL) {
		kdc_tcp_terminate_connection(kdc_conn, "kdc_tcp_call_loop: "
				"no memory for tstream_writev_queue_send");
		return;
	}
	tevent_req_set_callback(subreq, kdc_tcp_call_writev_done, call);

	/*
	 * The krb5 tcp pdu's has the length as 4 byte (initial_read_size),
	 * packet_full_request_u32 provides the pdu length then.
	 */
	subreq = tstream_read_pdu_blob_send(kdc_conn,
					    kdc_conn->conn->event.ctx,
					    kdc_conn->tstream,
					    4, /* initial_read_size */
					    packet_full_request_u32,
					    kdc_conn);
	if (subreq == NULL) {
		kdc_tcp_terminate_connection(kdc_conn, "kdc_tcp_call_loop: "
				"no memory for tstream_read_pdu_blob_send");
		return;
	}
	tevent_req_set_callback(subreq, kdc_tcp_call_loop, kdc_conn);
}

static void kdc_tcp_call_writev_done(struct tevent_req *subreq)
{
	struct kdc_tcp_call *call = tevent_req_callback_data(subreq,
			struct kdc_tcp_call);
	int sys_errno;
	int rc;

	rc = tstream_writev_queue_recv(subreq, &sys_errno);
	TALLOC_FREE(subreq);
	if (rc == -1) {
		const char *reason;

		reason = talloc_asprintf(call, "kdc_tcp_call_writev_done: "
					 "tstream_writev_queue_recv() - %d:%s",
					 sys_errno, strerror(sys_errno));
		if (!reason) {
			reason = "kdc_tcp_call_writev_done: tstream_writev_queue_recv() failed";
		}

		kdc_tcp_terminate_connection(call->kdc_conn, reason);
		return;
	}

	/* We don't care about errors */

	talloc_free(call);
}

/*
  called when we get a new connection
*/
static void kdc_tcp_accept(struct stream_connection *conn)
{
	struct kdc_socket *kdc_socket;
	struct kdc_tcp_connection *kdc_conn;
	struct tevent_req *subreq;
	int rc;

	kdc_conn = talloc_zero(conn, struct kdc_tcp_connection);
	if (kdc_conn == NULL) {
		stream_terminate_connection(conn,
				"kdc_tcp_accept: out of memory");
		return;
	}

	kdc_conn->send_queue = tevent_queue_create(conn, "kdc_tcp_accept");
	if (kdc_conn->send_queue == NULL) {
		stream_terminate_connection(conn,
				"kdc_tcp_accept: out of memory");
		return;
	}

	kdc_socket = talloc_get_type(conn->private_data, struct kdc_socket);

	TALLOC_FREE(conn->event.fde);

	rc = tstream_bsd_existing_socket(kdc_conn,
			socket_get_fd(conn->socket),
			&kdc_conn->tstream);
	if (rc < 0) {
		stream_terminate_connection(conn,
				"kdc_tcp_accept: out of memory");
		return;
	}

	kdc_conn->conn = conn;
	kdc_conn->kdc_socket = kdc_socket;
	conn->private_data = kdc_conn;

	/*
	 * The krb5 tcp pdu's has the length as 4 byte (initial_read_size),
	 * packet_full_request_u32 provides the pdu length then.
	 */
	subreq = tstream_read_pdu_blob_send(kdc_conn,
					    kdc_conn->conn->event.ctx,
					    kdc_conn->tstream,
					    4, /* initial_read_size */
					    packet_full_request_u32,
					    kdc_conn);
	if (subreq == NULL) {
		kdc_tcp_terminate_connection(kdc_conn, "kdc_tcp_accept: "
				"no memory for tstream_read_pdu_blob_send");
		return;
	}
	tevent_req_set_callback(subreq, kdc_tcp_call_loop, kdc_conn);
}

static const struct stream_server_ops kdc_tcp_stream_ops = {
	.name			= "kdc_tcp",
	.accept_connection	= kdc_tcp_accept,
	.recv_handler		= kdc_tcp_recv,
	.send_handler		= kdc_tcp_send
};

/* hold information about one kdc/kpasswd udp socket */
struct kdc_udp_socket {
	struct kdc_socket *kdc_socket;
	struct tdgram_context *dgram;
	struct tevent_queue *send_queue;
};

struct kdc_udp_call {
	struct kdc_udp_socket *sock;
	struct tsocket_address *src;
	DATA_BLOB in;
	DATA_BLOB out;
};

static void kdc_udp_call_proxy_done(struct tevent_req *subreq);
static void kdc_udp_call_sendto_done(struct tevent_req *subreq);

static void kdc_udp_call_loop(struct tevent_req *subreq)
{
	struct kdc_udp_socket *sock = tevent_req_callback_data(subreq,
				      struct kdc_udp_socket);
	struct kdc_udp_call *call;
	uint8_t *buf;
	ssize_t len;
	int sys_errno;
	enum kdc_process_ret ret;

	call = talloc(sock, struct kdc_udp_call);
	if (call == NULL) {
		talloc_free(call);
		goto done;
	}
	call->sock = sock;

	len = tdgram_recvfrom_recv(subreq, &sys_errno,
				   call, &buf, &call->src);
	TALLOC_FREE(subreq);
	if (len == -1) {
		talloc_free(call);
		goto done;
	}

	call->in.data = buf;
	call->in.length = len;

	DEBUG(10,("Received krb5 UDP packet of length %lu from %s\n",
		 (long)call->in.length,
		 tsocket_address_string(call->src, call)));

	/* Call krb5 */
	ret = sock->kdc_socket->process(sock->kdc_socket->kdc,
				       call,
				       &call->in,
				       &call->out,
				       call->src,
				       sock->kdc_socket->local_address,
				       1 /* Datagram */);
	if (ret == KDC_PROCESS_FAILED) {
		talloc_free(call);
		goto done;
	}

	if (ret == KDC_PROCESS_PROXY) {
		uint16_t port;

		if (!sock->kdc_socket->kdc->am_rodc) {
			DEBUG(0,("kdc_udp_call_loop: proxying requested when not RODC"));
			talloc_free(call);
			goto done;
		}

		port = tsocket_address_inet_port(sock->kdc_socket->local_address);

		subreq = kdc_udp_proxy_send(call,
					    sock->kdc_socket->kdc->task->event_ctx,
					    sock->kdc_socket->kdc,
					    port,
					    call->in);
		if (subreq == NULL) {
			talloc_free(call);
			goto done;
		}
		tevent_req_set_callback(subreq, kdc_udp_call_proxy_done, call);
		goto done;
	}

	subreq = tdgram_sendto_queue_send(call,
					  sock->kdc_socket->kdc->task->event_ctx,
					  sock->dgram,
					  sock->send_queue,
					  call->out.data,
					  call->out.length,
					  call->src);
	if (subreq == NULL) {
		talloc_free(call);
		goto done;
	}
	tevent_req_set_callback(subreq, kdc_udp_call_sendto_done, call);

done:
	subreq = tdgram_recvfrom_send(sock,
				      sock->kdc_socket->kdc->task->event_ctx,
				      sock->dgram);
	if (subreq == NULL) {
		task_server_terminate(sock->kdc_socket->kdc->task,
				      "no memory for tdgram_recvfrom_send",
				      true);
		return;
	}
	tevent_req_set_callback(subreq, kdc_udp_call_loop, sock);
}

static void kdc_udp_call_proxy_done(struct tevent_req *subreq)
{
	struct kdc_udp_call *call =
		tevent_req_callback_data(subreq,
		struct kdc_udp_call);
	NTSTATUS status;

	status = kdc_udp_proxy_recv(subreq, call, &call->out);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		/* generate an error packet */
		status = kdc_proxy_unavailable_error(call->sock->kdc_socket->kdc,
						     call, &call->out);
	}

	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(call);
		return;
	}

	subreq = tdgram_sendto_queue_send(call,
					  call->sock->kdc_socket->kdc->task->event_ctx,
					  call->sock->dgram,
					  call->sock->send_queue,
					  call->out.data,
					  call->out.length,
					  call->src);
	if (subreq == NULL) {
		talloc_free(call);
		return;
	}

	tevent_req_set_callback(subreq, kdc_udp_call_sendto_done, call);
}

static void kdc_udp_call_sendto_done(struct tevent_req *subreq)
{
	struct kdc_udp_call *call = tevent_req_callback_data(subreq,
				       struct kdc_udp_call);
	ssize_t ret;
	int sys_errno;

	ret = tdgram_sendto_queue_recv(subreq, &sys_errno);

	/* We don't care about errors */

	talloc_free(call);
}

/*
  start listening on the given address
*/
static NTSTATUS kdc_add_socket(struct kdc_server *kdc,
			       const struct model_ops *model_ops,
			       const char *name,
			       const char *address,
			       uint16_t port,
			       kdc_process_fn_t process,
			       bool udp_only)
{
	struct kdc_socket *kdc_socket;
	struct kdc_udp_socket *kdc_udp_socket;
	struct tevent_req *udpsubreq;
	NTSTATUS status;
	int ret;

	kdc_socket = talloc(kdc, struct kdc_socket);
	NT_STATUS_HAVE_NO_MEMORY(kdc_socket);

	kdc_socket->kdc = kdc;
	kdc_socket->process = process;

	ret = tsocket_address_inet_from_strings(kdc_socket, "ip",
						address, port,
						&kdc_socket->local_address);
	if (ret != 0) {
		status = map_nt_error_from_unix(errno);
		return status;
	}

	if (!udp_only) {
		status = stream_setup_socket(kdc->task,
					     kdc->task->event_ctx,
					     kdc->task->lp_ctx,
					     model_ops,
					     &kdc_tcp_stream_ops,
					     "ip", address, &port,
					     lpcfg_socket_options(kdc->task->lp_ctx),
					     kdc_socket);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0,("Failed to bind to %s:%u TCP - %s\n",
				 address, port, nt_errstr(status)));
			talloc_free(kdc_socket);
			return status;
		}
	}

	kdc_udp_socket = talloc(kdc_socket, struct kdc_udp_socket);
	NT_STATUS_HAVE_NO_MEMORY(kdc_udp_socket);

	kdc_udp_socket->kdc_socket = kdc_socket;

	ret = tdgram_inet_udp_socket(kdc_socket->local_address,
				     NULL,
				     kdc_udp_socket,
				     &kdc_udp_socket->dgram);
	if (ret != 0) {
		status = map_nt_error_from_unix(errno);
		DEBUG(0,("Failed to bind to %s:%u UDP - %s\n",
			 address, port, nt_errstr(status)));
		return status;
	}

	kdc_udp_socket->send_queue = tevent_queue_create(kdc_udp_socket,
							 "kdc_udp_send_queue");
	NT_STATUS_HAVE_NO_MEMORY(kdc_udp_socket->send_queue);

	udpsubreq = tdgram_recvfrom_send(kdc_udp_socket,
					 kdc->task->event_ctx,
					 kdc_udp_socket->dgram);
	NT_STATUS_HAVE_NO_MEMORY(udpsubreq);
	tevent_req_set_callback(udpsubreq, kdc_udp_call_loop, kdc_udp_socket);

	return NT_STATUS_OK;
}


/*
  setup our listening sockets on the configured network interfaces
*/
static NTSTATUS kdc_startup_interfaces(struct kdc_server *kdc, struct loadparm_context *lp_ctx,
				       struct interface *ifaces)
{
	const struct model_ops *model_ops;
	int num_interfaces;
	TALLOC_CTX *tmp_ctx = talloc_new(kdc);
	NTSTATUS status;
	int i;
	uint16_t kdc_port = lpcfg_krb5_port(lp_ctx);
	uint16_t kpasswd_port = lpcfg_kpasswd_port(lp_ctx);
	bool done_wildcard = false;

	/* within the kdc task we want to be a single process, so
	   ask for the single process model ops and pass these to the
	   stream_setup_socket() call. */
	model_ops = process_model_startup("single");
	if (!model_ops) {
		DEBUG(0,("Can't find 'single' process model_ops\n"));
		return NT_STATUS_INTERNAL_ERROR;
	}

	num_interfaces = iface_count(ifaces);

	/* if we are allowing incoming packets from any address, then
	   we need to bind to the wildcard address */
	if (!lpcfg_bind_interfaces_only(lp_ctx)) {
		if (kdc_port) {
			status = kdc_add_socket(kdc, model_ops,
						"kdc", "0.0.0.0", kdc_port,
						kdc_process, false);
			NT_STATUS_NOT_OK_RETURN(status);
		}

		if (kpasswd_port) {
			status = kdc_add_socket(kdc, model_ops,
						"kpasswd", "0.0.0.0", kpasswd_port,
						kpasswdd_process, false);
			NT_STATUS_NOT_OK_RETURN(status);
		}
		done_wildcard = true;
	}

	for (i=0; i<num_interfaces; i++) {
		const char *address = talloc_strdup(tmp_ctx, iface_n_ip(ifaces, i));

		if (kdc_port) {
			status = kdc_add_socket(kdc, model_ops,
						"kdc", address, kdc_port,
						kdc_process, done_wildcard);
			NT_STATUS_NOT_OK_RETURN(status);
		}

		if (kpasswd_port) {
			status = kdc_add_socket(kdc, model_ops,
						"kpasswd", address, kpasswd_port,
						kpasswdd_process, done_wildcard);
			NT_STATUS_NOT_OK_RETURN(status);
		}
	}

	talloc_free(tmp_ctx);

	return NT_STATUS_OK;
}


static NTSTATUS kdc_check_generic_kerberos(struct irpc_message *msg,
				 struct kdc_check_generic_kerberos *r)
{
	struct PAC_Validate pac_validate;
	DATA_BLOB srv_sig;
	struct PAC_SIGNATURE_DATA kdc_sig;
	struct kdc_server *kdc = talloc_get_type(msg->private_data, struct kdc_server);
	enum ndr_err_code ndr_err;
	krb5_enctype etype;
	int ret;
	hdb_entry_ex ent;
	krb5_principal principal;
	krb5_keyblock keyblock;
	Key *key;

	/* There is no reply to this request */
	r->out.generic_reply = data_blob(NULL, 0);

	ndr_err = ndr_pull_struct_blob(&r->in.generic_request, msg, &pac_validate,
				       (ndr_pull_flags_fn_t)ndr_pull_PAC_Validate);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (pac_validate.MessageType != 3) {
		/* We don't implement any other message types - such as certificate validation - yet */
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (pac_validate.ChecksumAndSignature.length != (pac_validate.ChecksumLength + pac_validate.SignatureLength)
	    || pac_validate.ChecksumAndSignature.length < pac_validate.ChecksumLength
	    || pac_validate.ChecksumAndSignature.length < pac_validate.SignatureLength ) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	srv_sig = data_blob_const(pac_validate.ChecksumAndSignature.data,
				  pac_validate.ChecksumLength);

	if (pac_validate.SignatureType == CKSUMTYPE_HMAC_MD5) {
		etype = ETYPE_ARCFOUR_HMAC_MD5;
	} else {
		ret = krb5_cksumtype_to_enctype(kdc->smb_krb5_context->krb5_context, pac_validate.SignatureType,
						&etype);
		if (ret != 0) {
			return NT_STATUS_LOGON_FAILURE;
		}
	}

	ret = krb5_make_principal(kdc->smb_krb5_context->krb5_context, &principal,
				  lpcfg_realm(kdc->task->lp_ctx),
				  "krbtgt", lpcfg_realm(kdc->task->lp_ctx),
				  NULL);

	if (ret != 0) {
		return NT_STATUS_NO_MEMORY;
	}

	ret = kdc->config->db[0]->hdb_fetch_kvno(kdc->smb_krb5_context->krb5_context,
						 kdc->config->db[0],
						 principal,
						 HDB_F_GET_KRBTGT | HDB_F_DECRYPT,
						 0,
						 &ent);
	
	if (ret != 0) {
		hdb_free_entry(kdc->smb_krb5_context->krb5_context, &ent);
		krb5_free_principal(kdc->smb_krb5_context->krb5_context, principal);

		return NT_STATUS_LOGON_FAILURE;
	}

	ret = hdb_enctype2key(kdc->smb_krb5_context->krb5_context, &ent.entry, etype, &key);

	if (ret != 0) {
		hdb_free_entry(kdc->smb_krb5_context->krb5_context, &ent);
		krb5_free_principal(kdc->smb_krb5_context->krb5_context, principal);
		return NT_STATUS_LOGON_FAILURE;
	}

	keyblock = key->key;

	kdc_sig.type = pac_validate.SignatureType;
	kdc_sig.signature = data_blob_const(&pac_validate.ChecksumAndSignature.data[pac_validate.ChecksumLength],
					    pac_validate.SignatureLength);
	ret = check_pac_checksum(msg, srv_sig, &kdc_sig,
			   kdc->smb_krb5_context->krb5_context, &keyblock);

	hdb_free_entry(kdc->smb_krb5_context->krb5_context, &ent);
	krb5_free_principal(kdc->smb_krb5_context->krb5_context, principal);

	if (ret != 0) {
		return NT_STATUS_LOGON_FAILURE;
	}

	return NT_STATUS_OK;
}


/*
  startup the kdc task
*/
static void kdc_task_init(struct task_server *task)
{
	struct kdc_server *kdc;
	NTSTATUS status;
	krb5_error_code ret;
	struct interface *ifaces;
	int ldb_ret;

	switch (lpcfg_server_role(task->lp_ctx)) {
	case ROLE_STANDALONE:
		task_server_terminate(task, "kdc: no KDC required in standalone configuration", false);
		return;
	case ROLE_DOMAIN_MEMBER:
		task_server_terminate(task, "kdc: no KDC required in member server configuration", false);
		return;
	case ROLE_DOMAIN_CONTROLLER:
		/* Yes, we want a KDC */
		break;
	}

	load_interfaces(task, lpcfg_interfaces(task->lp_ctx), &ifaces);

	if (iface_count(ifaces) == 0) {
		task_server_terminate(task, "kdc: no network interfaces configured", false);
		return;
	}

	task_server_set_title(task, "task[kdc]");

	kdc = talloc_zero(task, struct kdc_server);
	if (kdc == NULL) {
		task_server_terminate(task, "kdc: out of memory", true);
		return;
	}

	kdc->task = task;


	/* get a samdb connection */
	kdc->samdb = samdb_connect(kdc, kdc->task->event_ctx, kdc->task->lp_ctx,
				   system_session(kdc->task->lp_ctx), 0);
	if (!kdc->samdb) {
		DEBUG(1,("kdc_task_init: unable to connect to samdb\n"));
		task_server_terminate(task, "kdc: krb5_init_context samdb connect failed", true);
		return;
	}

	ldb_ret = samdb_rodc(kdc->samdb, &kdc->am_rodc);
	if (ldb_ret != LDB_SUCCESS) {
		DEBUG(1, ("kdc_task_init: Cannot determine if we are an RODC: %s\n",
			  ldb_errstring(kdc->samdb)));
		task_server_terminate(task, "kdc: krb5_init_context samdb RODC connect failed", true);
		return;
	}

	kdc->proxy_timeout = lpcfg_parm_int(kdc->task->lp_ctx, NULL, "kdc", "proxy timeout", 5);

	initialize_krb5_error_table();

	ret = smb_krb5_init_context(kdc, task->event_ctx, task->lp_ctx, &kdc->smb_krb5_context);
	if (ret) {
		DEBUG(1,("kdc_task_init: krb5_init_context failed (%s)\n",
			 error_message(ret)));
		task_server_terminate(task, "kdc: krb5_init_context failed", true);
		return;
	}

	krb5_add_et_list(kdc->smb_krb5_context->krb5_context, initialize_hdb_error_table_r);

	ret = krb5_kdc_get_config(kdc->smb_krb5_context->krb5_context,
				  &kdc->config);
	if(ret) {
		task_server_terminate(task, "kdc: failed to get KDC configuration", true);
		return;
	}

	kdc->config->logf = kdc->smb_krb5_context->logf;
	kdc->config->db = talloc(kdc, struct HDB *);
	if (!kdc->config->db) {
		task_server_terminate(task, "kdc: out of memory", true);
		return;
	}
	kdc->config->num_db = 1;

	/* Register hdb-samba4 hooks for use as a keytab */

	kdc->base_ctx = talloc_zero(kdc, struct samba_kdc_base_context);
	if (!kdc->base_ctx) {
		task_server_terminate(task, "kdc: out of memory", true);
		return;
	}

	kdc->base_ctx->ev_ctx = task->event_ctx;
	kdc->base_ctx->lp_ctx = task->lp_ctx;

	status = hdb_samba4_create_kdc(kdc->base_ctx,
				       kdc->smb_krb5_context->krb5_context,
				       &kdc->config->db[0]);
	if (!NT_STATUS_IS_OK(status)) {
		task_server_terminate(task, "kdc: hdb_samba4_create_kdc (setup KDC database) failed", true);
		return;
	}

	ret = krb5_plugin_register(kdc->smb_krb5_context->krb5_context,
				   PLUGIN_TYPE_DATA, "hdb",
				   &hdb_samba4);
	if(ret) {
		task_server_terminate(task, "kdc: failed to register hdb plugin", true);
		return;
	}

	ret = krb5_kt_register(kdc->smb_krb5_context->krb5_context, &hdb_kt_ops);
	if(ret) {
		task_server_terminate(task, "kdc: failed to register keytab plugin", true);
		return;
	}

	/* Register WinDC hooks */
	ret = krb5_plugin_register(kdc->smb_krb5_context->krb5_context,
				   PLUGIN_TYPE_DATA, "windc",
				   &windc_plugin_table);
	if(ret) {
		task_server_terminate(task, "kdc: failed to register windc plugin", true);
		return;
	}

	ret = krb5_kdc_windc_init(kdc->smb_krb5_context->krb5_context);

	if(ret) {
		task_server_terminate(task, "kdc: failed to init windc plugin", true);
		return;
	}

	ret = krb5_kdc_pkinit_config(kdc->smb_krb5_context->krb5_context, kdc->config);

	if(ret) {
		task_server_terminate(task, "kdc: failed to init kdc pkinit subsystem", true);
		return;
	}

	/* start listening on the configured network interfaces */
	status = kdc_startup_interfaces(kdc, task->lp_ctx, ifaces);
	if (!NT_STATUS_IS_OK(status)) {
		task_server_terminate(task, "kdc failed to setup interfaces", true);
		return;
	}

	status = IRPC_REGISTER(task->msg_ctx, irpc, KDC_CHECK_GENERIC_KERBEROS,
			       kdc_check_generic_kerberos, kdc);
	if (!NT_STATUS_IS_OK(status)) {
		task_server_terminate(task, "kdc failed to setup monitoring", true);
		return;
	}

	irpc_add_name(task->msg_ctx, "kdc_server");
}


/* called at smbd startup - register ourselves as a server service */
NTSTATUS server_service_kdc_init(void)
{
	return register_server_service("kdc", kdc_task_init);
}
