/*
   Unix SMB/CIFS implementation.

   DNS server startup

   Copyright (C) 2010 Kai Blin  <kai@samba.org>

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
#include "smbd/service_task.h"
#include "smbd/service.h"
#include "smbd/service_stream.h"
#include "smbd/process_model.h"
#include "lib/events/events.h"
#include "lib/socket/socket.h"
#include "lib/tsocket/tsocket.h"
#include "libcli/util/tstream.h"
#include "libcli/util/ntstatus.h"
#include "system/network.h"
#include "lib/stream/packet.h"
#include "lib/socket/netif.h"
#include "dns_server/dns_server.h"
#include "param/param.h"
#include "librpc/ndr/libndr.h"
#include "librpc/gen_ndr/ndr_dns.h"
#include "librpc/gen_ndr/ndr_dnsp.h"
#include <ldb.h>
#include "dsdb/samdb/samdb.h"
#include "dsdb/common/util.h"
#include "auth/session.h"
#include "lib/util/dlinklist.h"

/* hold information about one dns socket */
struct dns_socket {
	struct dns_server *dns;
	struct tsocket_address *local_address;
};

struct dns_udp_socket {
	struct dns_socket *dns_socket;
	struct tdgram_context *dgram;
	struct tevent_queue *send_queue;
};

/*
  state of an open tcp connection
*/
struct dns_tcp_connection {
	/* stream connection we belong to */
	struct stream_connection *conn;

	/* the dns_server the connection belongs to */
	struct dns_socket *dns_socket;

	struct tstream_context *tstream;

	struct tevent_queue *send_queue;
};

static void dns_tcp_terminate_connection(struct dns_tcp_connection *dnsconn, const char *reason)
{
	stream_terminate_connection(dnsconn->conn, reason);
}

static void dns_tcp_recv(struct stream_connection *conn, uint16_t flags)
{
	struct dns_tcp_connection *dnsconn = talloc_get_type(conn->private_data,
							     struct dns_tcp_connection);
	/* this should never be triggered! */
	dns_tcp_terminate_connection(dnsconn, "dns_tcp_recv: called");
}

static void dns_tcp_send(struct stream_connection *conn, uint16_t flags)
{
	struct dns_tcp_connection *dnsconn = talloc_get_type(conn->private_data,
							     struct dns_tcp_connection);
	/* this should never be triggered! */
	dns_tcp_terminate_connection(dnsconn, "dns_tcp_send: called");
}

static NTSTATUS dns_process(struct dns_server *dns,
			    TALLOC_CTX *mem_ctx,
			    DATA_BLOB *in,
			    DATA_BLOB *out)
{
	enum ndr_err_code ndr_err;
	WERROR ret;
	struct dns_name_packet *in_packet;
	struct dns_name_packet *out_packet;
	struct dns_res_rec *answers = NULL, *nsrecs = NULL, *additional = NULL;
	uint16_t num_answers = 0 , num_nsrecs = 0, num_additional = 0;

	if (in->length < 12) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	in_packet = talloc_zero(mem_ctx, struct dns_name_packet);
	/* TODO: We don't really need an out_packet. */
	out_packet = talloc_zero(mem_ctx, struct dns_name_packet);

	if (in_packet == NULL) return NT_STATUS_NO_MEMORY;
	if (out_packet == NULL) return NT_STATUS_NO_MEMORY;

	dump_data(2, in->data, in->length);

	ndr_err = ndr_pull_struct_blob(in, in_packet, in_packet,
			(ndr_pull_flags_fn_t)ndr_pull_dns_name_packet);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		TALLOC_FREE(in_packet);
		DEBUG(0, ("Failed to parse packet %d!\n", ndr_err));
		*out = *in;

		out->data[2] |= 0x80; /* Toggle DNS_FLAG_REPLY */
		out->data[3] |= DNS_RCODE_FORMERR;

		return NT_STATUS_OK;
	}

	NDR_PRINT_DEBUG(dns_name_packet, in_packet);
	*out_packet = *in_packet;
	out_packet->operation |= DNS_FLAG_REPLY;

	switch (in_packet->operation & DNS_OPCODE) {
	case DNS_OPCODE_QUERY:

		ret = dns_server_process_query(dns, out_packet, in_packet,
					       &answers, &num_answers,
					       &nsrecs,  &num_nsrecs,
					       &additional, &num_additional);

		break;
	case DNS_OPCODE_REGISTER:
		ret = dns_server_process_update(dns, out_packet, in_packet,
						answers, num_answers,
						&nsrecs,  &num_nsrecs,
						&additional, &num_additional);
		break;
	default:
		ret = WERR_DNS_ERROR_RCODE_NOT_IMPLEMENTED;
		break;
	}

	if (W_ERROR_IS_OK(ret)) {
		out_packet->ancount = num_answers;
		out_packet->answers = answers;

		out_packet->nscount = num_nsrecs;
		out_packet->nsrecs  = nsrecs;

		out_packet->arcount = num_additional;
		out_packet->additional = additional;
	} else {
		out_packet->operation |= werr_to_dns_err(ret);
	}

	NDR_PRINT_DEBUG(dns_name_packet, out_packet);
	ndr_err = ndr_push_struct_blob(out, out_packet, out_packet,
			(ndr_push_flags_fn_t)ndr_push_dns_name_packet);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		TALLOC_FREE(in_packet);
		TALLOC_FREE(out_packet);
		DEBUG(0, ("Failed to push packet %d!\n", ndr_err));
		*out = *in;

		out->data[2] |= 0x80; /* Toggle DNS_FLAG_REPLY */
		out->data[3] |= DNS_RCODE_SERVFAIL;

		return NT_STATUS_OK;
	}

	dump_data(2, out->data, out->length);
	return NT_STATUS_OK;
}

struct dns_tcp_call {
	struct dns_tcp_connection *dns_conn;
	DATA_BLOB in;
	DATA_BLOB out;
	uint8_t out_hdr[4];
	struct iovec out_iov[2];
};

static void dns_tcp_call_writev_done(struct tevent_req *subreq);

static void dns_tcp_call_loop(struct tevent_req *subreq)
{
	struct dns_tcp_connection *dns_conn = tevent_req_callback_data(subreq,
				      struct dns_tcp_connection);
	struct dns_tcp_call *call;
	NTSTATUS status;

	call = talloc(dns_conn, struct dns_tcp_call);
	if (call == NULL) {
		dns_tcp_terminate_connection(dns_conn, "dns_tcp_call_loop: "
				"no memory for dns_tcp_call");
		return;
	}
	call->dns_conn = dns_conn;

	status = tstream_read_pdu_blob_recv(subreq,
					    call,
					    &call->in);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		const char *reason;

		reason = talloc_asprintf(call, "dns_tcp_call_loop: "
					 "tstream_read_pdu_blob_recv() - %s",
					 nt_errstr(status));
		if (!reason) {
			reason = nt_errstr(status);
		}

		dns_tcp_terminate_connection(dns_conn, reason);
		return;
	}

	DEBUG(10,("Received krb5 TCP packet of length %lu from %s\n",
		 (long) call->in.length,
		 tsocket_address_string(dns_conn->conn->remote_address, call)));

	/* skip length header */
	call->in.data +=4;
	call->in.length -= 4;

	/* Call dns */
	status = dns_process(dns_conn->dns_socket->dns, call, &call->in, &call->out);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("dns_process returned %s\n", nt_errstr(status)));
		dns_tcp_terminate_connection(dns_conn,
				"dns_tcp_call_loop: process function failed");
		return;
	}

	/* First add the length of the out buffer */
	RSIVAL(call->out_hdr, 0, call->out.length);
	call->out_iov[0].iov_base = (char *) call->out_hdr;
	call->out_iov[0].iov_len = 4;

	call->out_iov[1].iov_base = (char *) call->out.data;
	call->out_iov[1].iov_len = call->out.length;

	subreq = tstream_writev_queue_send(call,
					   dns_conn->conn->event.ctx,
					   dns_conn->tstream,
					   dns_conn->send_queue,
					   call->out_iov, 2);
	if (subreq == NULL) {
		dns_tcp_terminate_connection(dns_conn, "dns_tcp_call_loop: "
				"no memory for tstream_writev_queue_send");
		return;
	}
	tevent_req_set_callback(subreq, dns_tcp_call_writev_done, call);

	/*
	 * The krb5 tcp pdu's has the length as 4 byte (initial_read_size),
	 * packet_full_request_u32 provides the pdu length then.
	 */
	subreq = tstream_read_pdu_blob_send(dns_conn,
					    dns_conn->conn->event.ctx,
					    dns_conn->tstream,
					    4, /* initial_read_size */
					    packet_full_request_u32,
					    dns_conn);
	if (subreq == NULL) {
		dns_tcp_terminate_connection(dns_conn, "dns_tcp_call_loop: "
				"no memory for tstream_read_pdu_blob_send");
		return;
	}
	tevent_req_set_callback(subreq, dns_tcp_call_loop, dns_conn);
}

static void dns_tcp_call_writev_done(struct tevent_req *subreq)
{
	struct dns_tcp_call *call = tevent_req_callback_data(subreq,
			struct dns_tcp_call);
	int sys_errno;
	int rc;

	rc = tstream_writev_queue_recv(subreq, &sys_errno);
	TALLOC_FREE(subreq);
	if (rc == -1) {
		const char *reason;

		reason = talloc_asprintf(call, "dns_tcp_call_writev_done: "
					 "tstream_writev_queue_recv() - %d:%s",
					 sys_errno, strerror(sys_errno));
		if (!reason) {
			reason = "dns_tcp_call_writev_done: tstream_writev_queue_recv() failed";
		}

		dns_tcp_terminate_connection(call->dns_conn, reason);
		return;
	}

	/* We don't care about errors */

	talloc_free(call);
}

/*
  called when we get a new connection
*/
static void dns_tcp_accept(struct stream_connection *conn)
{
	struct dns_socket *dns_socket;
	struct dns_tcp_connection *dns_conn;
	struct tevent_req *subreq;
	int rc;

	dns_conn = talloc_zero(conn, struct dns_tcp_connection);
	if (dns_conn == NULL) {
		stream_terminate_connection(conn,
				"dns_tcp_accept: out of memory");
		return;
	}

	dns_conn->send_queue = tevent_queue_create(conn, "dns_tcp_accept");
	if (dns_conn->send_queue == NULL) {
		stream_terminate_connection(conn,
				"dns_tcp_accept: out of memory");
		return;
	}

	dns_socket = talloc_get_type(conn->private_data, struct dns_socket);

	TALLOC_FREE(conn->event.fde);

	rc = tstream_bsd_existing_socket(dns_conn,
			socket_get_fd(conn->socket),
			&dns_conn->tstream);
	if (rc < 0) {
		stream_terminate_connection(conn,
				"dns_tcp_accept: out of memory");
		return;
	}

	dns_conn->conn = conn;
	dns_conn->dns_socket = dns_socket;
	conn->private_data = dns_conn;

	/*
	 * The krb5 tcp pdu's has the length as 4 byte (initial_read_size),
	 * packet_full_request_u32 provides the pdu length then.
	 */
	subreq = tstream_read_pdu_blob_send(dns_conn,
					    dns_conn->conn->event.ctx,
					    dns_conn->tstream,
					    4, /* initial_read_size */
					    packet_full_request_u32,
					    dns_conn);
	if (subreq == NULL) {
		dns_tcp_terminate_connection(dns_conn, "dns_tcp_accept: "
				"no memory for tstream_read_pdu_blob_send");
		return;
	}
	tevent_req_set_callback(subreq, dns_tcp_call_loop, dns_conn);
}

static const struct stream_server_ops dns_tcp_stream_ops = {
	.name			= "dns_tcp",
	.accept_connection	= dns_tcp_accept,
	.recv_handler		= dns_tcp_recv,
	.send_handler		= dns_tcp_send
};

struct dns_udp_call {
	struct tsocket_address *src;
	DATA_BLOB in;
	DATA_BLOB out;
};

static void dns_udp_call_sendto_done(struct tevent_req *subreq);

static void dns_udp_call_loop(struct tevent_req *subreq)
{
	struct dns_udp_socket *sock = tevent_req_callback_data(subreq,
				      struct dns_udp_socket);
	struct dns_udp_call *call;
	uint8_t *buf;
	ssize_t len;
	int sys_errno;
	NTSTATUS status;

	call = talloc(sock, struct dns_udp_call);
	if (call == NULL) {
		talloc_free(call);
		goto done;
	}

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
	status = dns_process(sock->dns_socket->dns, call, &call->in, &call->out);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(call);
		DEBUG(0, ("dns_process returned %s\n", nt_errstr(status)));
		goto done;
	}

	subreq = tdgram_sendto_queue_send(call,
					  sock->dns_socket->dns->task->event_ctx,
					  sock->dgram,
					  sock->send_queue,
					  call->out.data,
					  call->out.length,
					  call->src);
	if (subreq == NULL) {
		talloc_free(call);
		goto done;
	}
	tevent_req_set_callback(subreq, dns_udp_call_sendto_done, call);

done:
	subreq = tdgram_recvfrom_send(sock,
				      sock->dns_socket->dns->task->event_ctx,
				      sock->dgram);
	if (subreq == NULL) {
		task_server_terminate(sock->dns_socket->dns->task,
				      "no memory for tdgram_recvfrom_send",
				      true);
		return;
	}
	tevent_req_set_callback(subreq, dns_udp_call_loop, sock);
}

static void dns_udp_call_sendto_done(struct tevent_req *subreq)
{
	struct dns_udp_call *call = tevent_req_callback_data(subreq,
				       struct dns_udp_call);
	ssize_t ret;
	int sys_errno;

	ret = tdgram_sendto_queue_recv(subreq, &sys_errno);

	/* We don't care about errors */

	talloc_free(call);
}

/*
  start listening on the given address
*/
static NTSTATUS dns_add_socket(struct dns_server *dns,
			       const struct model_ops *model_ops,
			       const char *name,
			       const char *address,
			       uint16_t port)
{
	struct dns_socket *dns_socket;
	struct dns_udp_socket *dns_udp_socket;
	struct tevent_req *udpsubreq;
	NTSTATUS status;
	int ret;

	dns_socket = talloc(dns, struct dns_socket);
	NT_STATUS_HAVE_NO_MEMORY(dns_socket);

	dns_socket->dns = dns;

	ret = tsocket_address_inet_from_strings(dns_socket, "ip",
						address, port,
						&dns_socket->local_address);
	if (ret != 0) {
		status = map_nt_error_from_unix(errno);
		return status;
	}

	status = stream_setup_socket(dns->task,
				     dns->task->event_ctx,
				     dns->task->lp_ctx,
				     model_ops,
				     &dns_tcp_stream_ops,
				     "ip", address, &port,
				     lpcfg_socket_options(dns->task->lp_ctx),
				     dns_socket);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("Failed to bind to %s:%u TCP - %s\n",
			 address, port, nt_errstr(status)));
		talloc_free(dns_socket);
		return status;
	}

	dns_udp_socket = talloc(dns_socket, struct dns_udp_socket);
	NT_STATUS_HAVE_NO_MEMORY(dns_udp_socket);

	dns_udp_socket->dns_socket = dns_socket;

	ret = tdgram_inet_udp_socket(dns_socket->local_address,
				     NULL,
				     dns_udp_socket,
				     &dns_udp_socket->dgram);
	if (ret != 0) {
		status = map_nt_error_from_unix(errno);
		DEBUG(0,("Failed to bind to %s:%u UDP - %s\n",
			 address, port, nt_errstr(status)));
		return status;
	}

	dns_udp_socket->send_queue = tevent_queue_create(dns_udp_socket,
							 "dns_udp_send_queue");
	NT_STATUS_HAVE_NO_MEMORY(dns_udp_socket->send_queue);

	udpsubreq = tdgram_recvfrom_send(dns_udp_socket,
					 dns->task->event_ctx,
					 dns_udp_socket->dgram);
	NT_STATUS_HAVE_NO_MEMORY(udpsubreq);
	tevent_req_set_callback(udpsubreq, dns_udp_call_loop, dns_udp_socket);

	return NT_STATUS_OK;
}

/*
  setup our listening sockets on the configured network interfaces
*/
static NTSTATUS dns_startup_interfaces(struct dns_server *dns, struct loadparm_context *lp_ctx,
				       struct interface *ifaces)
{
	const struct model_ops *model_ops;
	int num_interfaces;
	TALLOC_CTX *tmp_ctx = talloc_new(dns);
	NTSTATUS status;
	int i;

	/* within the dns task we want to be a single process, so
	   ask for the single process model ops and pass these to the
	   stream_setup_socket() call. */
	model_ops = process_model_startup("single");
	if (!model_ops) {
		DEBUG(0,("Can't find 'single' process model_ops\n"));
		return NT_STATUS_INTERNAL_ERROR;
	}

	num_interfaces = iface_count(ifaces);

	for (i=0; i<num_interfaces; i++) {
		const char *address = talloc_strdup(tmp_ctx, iface_n_ip(ifaces, i));

		status = dns_add_socket(dns, model_ops, "dns", address, DNS_SERVICE_PORT);
		NT_STATUS_NOT_OK_RETURN(status);
	}

	talloc_free(tmp_ctx);

	return NT_STATUS_OK;
}

static int dns_server_sort_zones(struct ldb_message **m1, struct ldb_message **m2)
{
	const char *n1, *n2;
	size_t l1, l2;

	n1 = ldb_msg_find_attr_as_string(*m1, "name", NULL);
	n2 = ldb_msg_find_attr_as_string(*m2, "name", NULL);

	l1 = strlen(n1);
	l2 = strlen(n2);

	/* If the string lengths are not equal just sort by length */
	if (l1 != l2) {
		/* If m1 is the larger zone name, return it first */
		return l2 - l1;
	}

	/*TODO: We need to compare DNs here, we want the DomainDNSZones first */
	return 0;
}

static void dns_task_init(struct task_server *task)
{
	struct dns_server *dns;
	NTSTATUS status;
	struct interface *ifaces;
	int ret;
	struct ldb_result *res;
	struct ldb_dn *rootdn;
	static const char * const attrs[] = { "name", NULL};
	unsigned int i;

	switch (lpcfg_server_role(task->lp_ctx)) {
	case ROLE_STANDALONE:
		task_server_terminate(task, "dns: no DNS required in standalone configuration", false);
		return;
	case ROLE_DOMAIN_MEMBER:
		task_server_terminate(task, "dns: no DNS required in member server configuration", false);
		return;
	case ROLE_DOMAIN_CONTROLLER:
		/* Yes, we want a DNS */
		break;
	}

	load_interfaces(task, lpcfg_interfaces(task->lp_ctx), &ifaces);

	if (iface_count(ifaces) == 0) {
		task_server_terminate(task, "dns: no network interfaces configured", false);
		return;
	}

	task_server_set_title(task, "task[dns]");

	dns = talloc_zero(task, struct dns_server);
	if (dns == NULL) {
		task_server_terminate(task, "dns: out of memory", true);
		return;
	}

	dns->task = task;

	dns->samdb = samdb_connect(dns, dns->task->event_ctx, dns->task->lp_ctx,
			      system_session(dns->task->lp_ctx), 0);
	if (!dns->samdb) {
		task_server_terminate(task, "dns: samdb_connect failed", true);
		return;
	}

	rootdn = ldb_dn_new(dns, dns->samdb, "");
	if (rootdn == NULL) {
		task_server_terminate(task, "dns: out of memory", true);
		return;
	}

	// TODO: this search does not work against windows
	ret = dsdb_search(dns->samdb, dns, &res, rootdn, LDB_SCOPE_SUBTREE,
			  attrs, DSDB_SEARCH_SEARCH_ALL_PARTITIONS, "(objectClass=dnsZone)");
	if (ret != LDB_SUCCESS) {
		task_server_terminate(task,
				      "dns: failed to look up root DNS zones",
				      true);
		return;
	}

	TYPESAFE_QSORT(res->msgs, res->count, dns_server_sort_zones);

	for (i=0; i < res->count; i++) {
		struct dns_server_zone *z;

		z = talloc_zero(dns, struct dns_server_zone);
		if (z == NULL) {
		}

		z->name = ldb_msg_find_attr_as_string(res->msgs[i], "name", NULL);
		z->dn = talloc_move(z, &res->msgs[i]->dn);

		DLIST_ADD_END(dns->zones, z, NULL);
	}

	status = dns_startup_interfaces(dns, task->lp_ctx, ifaces);
	if (!NT_STATUS_IS_OK(status)) {
		task_server_terminate(task, "dns failed to setup interfaces", true);
		return;
	}
}

NTSTATUS server_service_dns_init(void)
{
	return register_server_service("dns", dns_task_init);
}
