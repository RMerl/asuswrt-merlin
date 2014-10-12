/* 
   Unix SMB/CIFS implementation.

   dcerpc over standard sockets transport

   Copyright (C) Andrew Tridgell 2003
   Copyright (C) Jelmer Vernooij 2004
   Copyright (C) Rafal Szczesniak 2006
   
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
#include "lib/events/events.h"
#include "lib/socket/socket.h"
#include "lib/stream/packet.h"
#include "libcli/composite/composite.h"
#include "librpc/rpc/dcerpc.h"
#include "librpc/rpc/dcerpc_proto.h"
#include "libcli/resolve/resolve.h"
#include "librpc/rpc/rpc_common.h"

/* transport private information used by general socket pipe transports */
struct sock_private {
	struct tevent_fd *fde;
	struct socket_context *sock;
	char *server_name;

	struct packet_context *packet;
	uint32_t pending_reads;

	const char *path; /* For ncacn_unix_sock and ncalrpc */
};


/*
  mark the socket dead
*/
static void sock_dead(struct dcecli_connection *p, NTSTATUS status)
{
	struct sock_private *sock = (struct sock_private *)p->transport.private_data;

	if (!sock) return;

	if (sock->packet) {
		packet_recv_disable(sock->packet);
		packet_set_fde(sock->packet, NULL);
		packet_set_socket(sock->packet, NULL);
	}

	if (sock->fde) {
		talloc_free(sock->fde);
		sock->fde = NULL;
	}

	if (sock->sock) {
		talloc_free(sock->sock);
		sock->sock = NULL;
	}

	if (NT_STATUS_EQUAL(NT_STATUS_UNSUCCESSFUL, status)) {
		status = NT_STATUS_UNEXPECTED_NETWORK_ERROR;
	}

	if (NT_STATUS_EQUAL(NT_STATUS_OK, status)) {
		status = NT_STATUS_END_OF_FILE;
	}

	if (p->transport.recv_data) {
		p->transport.recv_data(p, NULL, status);
	}
}


/*
  handle socket recv errors
*/
static void sock_error_handler(void *private_data, NTSTATUS status)
{
	struct dcecli_connection *p = talloc_get_type(private_data,
						      struct dcecli_connection);
	sock_dead(p, status);
}

/*
  check if a blob is a complete packet
*/
static NTSTATUS sock_complete_packet(void *private_data, DATA_BLOB blob, size_t *size)
{
	if (blob.length < DCERPC_FRAG_LEN_OFFSET+2) {
		return STATUS_MORE_ENTRIES;
	}
	*size = dcerpc_get_frag_length(&blob);
	if (*size < blob.length) {
		/*
		 * something is wrong, let the caller deal with it
		 */
		*size = blob.length;
	}
	if (*size > blob.length) {
		return STATUS_MORE_ENTRIES;
	}
	return NT_STATUS_OK;
}

/*
  process recv requests
*/
static NTSTATUS sock_process_recv(void *private_data, DATA_BLOB blob)
{
	struct dcecli_connection *p = talloc_get_type(private_data,
						      struct dcecli_connection);
	struct sock_private *sock = (struct sock_private *)p->transport.private_data;
	sock->pending_reads--;
	if (sock->pending_reads == 0) {
		packet_recv_disable(sock->packet);
	}
	p->transport.recv_data(p, &blob, NT_STATUS_OK);
	return NT_STATUS_OK;
}

/*
  called when a IO is triggered by the events system
*/
static void sock_io_handler(struct tevent_context *ev, struct tevent_fd *fde, 
			    uint16_t flags, void *private_data)
{
	struct dcecli_connection *p = talloc_get_type(private_data,
						      struct dcecli_connection);
	struct sock_private *sock = (struct sock_private *)p->transport.private_data;

	if (flags & EVENT_FD_WRITE) {
		packet_queue_run(sock->packet);
		return;
	}

	if (sock->sock == NULL) {
		return;
	}

	if (flags & EVENT_FD_READ) {
		packet_recv(sock->packet);
	}
}

/* 
   initiate a read request - not needed for dcerpc sockets
*/
static NTSTATUS sock_send_read(struct dcecli_connection *p)
{
	struct sock_private *sock = (struct sock_private *)p->transport.private_data;
	sock->pending_reads++;
	if (sock->pending_reads == 1) {
		packet_recv_enable(sock->packet);
	}
	return NT_STATUS_OK;
}

/* 
   send an initial pdu in a multi-pdu sequence
*/
static NTSTATUS sock_send_request(struct dcecli_connection *p, DATA_BLOB *data, 
				  bool trigger_read)
{
	struct sock_private *sock = (struct sock_private *)p->transport.private_data;
	DATA_BLOB blob;
	NTSTATUS status;

	if (sock->sock == NULL) {
		return NT_STATUS_CONNECTION_DISCONNECTED;
	}

	blob = data_blob_talloc(sock->packet, data->data, data->length);
	if (blob.data == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	status = packet_send(sock->packet, blob);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	if (trigger_read) {
		sock_send_read(p);
	}

	return NT_STATUS_OK;
}

/* 
   shutdown sock pipe connection
*/
static NTSTATUS sock_shutdown_pipe(struct dcecli_connection *p, NTSTATUS status)
{
	struct sock_private *sock = (struct sock_private *)p->transport.private_data;

	if (sock && sock->sock) {
		sock_dead(p, status);
	}

	return status;
}

/*
  return sock server name
*/
static const char *sock_peer_name(struct dcecli_connection *p)
{
	struct sock_private *sock = talloc_get_type(p->transport.private_data, struct sock_private);
	return sock->server_name;
}

/*
  return remote name we make the actual connection (good for kerberos) 
*/
static const char *sock_target_hostname(struct dcecli_connection *p)
{
	struct sock_private *sock = talloc_get_type(p->transport.private_data, struct sock_private);
	return sock->server_name;
}


struct pipe_open_socket_state {
	struct dcecli_connection *conn;
	struct socket_context *socket_ctx;
	struct sock_private *sock;
	struct socket_address *localaddr;
	struct socket_address *server;
	const char *target_hostname;
	enum dcerpc_transport_t transport;
};


static void continue_socket_connect(struct composite_context *ctx)
{
	struct dcecli_connection *conn;
	struct sock_private *sock;
	struct composite_context *c = talloc_get_type(ctx->async.private_data,
						      struct composite_context);
	struct pipe_open_socket_state *s = talloc_get_type(c->private_data,
							   struct pipe_open_socket_state);

	/* make it easier to write a function calls */
	conn = s->conn;
	sock = s->sock;

	c->status = socket_connect_recv(ctx);
	if (!NT_STATUS_IS_OK(c->status)) {
		DEBUG(0, ("Failed to connect host %s on port %d - %s\n", 
			  s->server->addr, s->server->port,
			  nt_errstr(c->status)));
		composite_error(c, c->status);
		return;
	}

	/*
	  fill in the transport methods
	*/
	conn->transport.transport       = s->transport;
	conn->transport.private_data    = NULL;

	conn->transport.send_request    = sock_send_request;
	conn->transport.send_read       = sock_send_read;
	conn->transport.recv_data       = NULL;

	conn->transport.shutdown_pipe   = sock_shutdown_pipe;
	conn->transport.peer_name       = sock_peer_name;
	conn->transport.target_hostname = sock_target_hostname;

	sock->sock          = s->socket_ctx;
	sock->pending_reads = 0;
	sock->server_name   = strupper_talloc(sock, s->target_hostname);

	sock->fde = event_add_fd(conn->event_ctx, sock->sock, socket_get_fd(sock->sock),
				 EVENT_FD_READ, sock_io_handler, conn);
	
	conn->transport.private_data = sock;

	sock->packet = packet_init(sock);
	if (sock->packet == NULL) {
		composite_error(c, NT_STATUS_NO_MEMORY);
		talloc_free(sock);
		return;
	}

	packet_set_private(sock->packet, conn);
	packet_set_socket(sock->packet, sock->sock);
	packet_set_callback(sock->packet, sock_process_recv);
	packet_set_full_request(sock->packet, sock_complete_packet);
	packet_set_error_handler(sock->packet, sock_error_handler);
	packet_set_event_context(sock->packet, conn->event_ctx);
	packet_set_fde(sock->packet, sock->fde);
	packet_set_serialise(sock->packet);
	packet_set_initial_read(sock->packet, 16);

	/* ensure we don't get SIGPIPE */
	BlockSignals(true, SIGPIPE);

	composite_done(c);
}


static struct composite_context *dcerpc_pipe_open_socket_send(TALLOC_CTX *mem_ctx,
						       struct dcecli_connection *cn,
						       struct socket_address *localaddr,
						       struct socket_address *server,
						       const char *target_hostname,
						       const char *full_path,
						       enum dcerpc_transport_t transport)
{
	struct composite_context *c;
	struct pipe_open_socket_state *s;
	struct composite_context *conn_req;

	c = composite_create(mem_ctx, cn->event_ctx);
	if (c == NULL) return NULL;

	s = talloc_zero(c, struct pipe_open_socket_state);
	if (composite_nomem(s, c)) return c;
	c->private_data = s;

	s->conn      = cn;
	s->transport = transport;
	if (localaddr) {
		s->localaddr = talloc_reference(c, localaddr);
		if (composite_nomem(s->localaddr, c)) return c;
	}
	s->server    = talloc_reference(c, server);
	if (composite_nomem(s->server, c)) return c;
	s->target_hostname = talloc_reference(s, target_hostname);

	s->sock = talloc(cn, struct sock_private);
	if (composite_nomem(s->sock, c)) return c;

	c->status = socket_create(server->family, SOCKET_TYPE_STREAM, &s->socket_ctx, 0);
	if (!composite_is_ok(c)) return c;

	talloc_steal(s->sock, s->socket_ctx);

	s->sock->path = talloc_reference(s->sock, full_path);

	conn_req = socket_connect_send(s->socket_ctx, s->localaddr, s->server, 0,
				       c->event_ctx);
	composite_continue(c, conn_req, continue_socket_connect, c);
	return c;
}


static NTSTATUS dcerpc_pipe_open_socket_recv(struct composite_context *c)
{
	NTSTATUS status = composite_wait(c);

	talloc_free(c);
	return status;
}

struct pipe_tcp_state {
	const char *server;
	const char *target_hostname;
	const char *address;
	uint32_t port;
	struct socket_address *localaddr;
	struct socket_address *srvaddr;
	struct resolve_context *resolve_ctx;
	struct dcecli_connection *conn;
};


#if 0 /* disabled till we can resolve names to ipv6 addresses */
static void continue_ipv6_open_socket(struct composite_context *ctx);
#endif
static void continue_ipv4_open_socket(struct composite_context *ctx);
static void continue_ip_resolve_name(struct composite_context *ctx);

static void continue_ip_resolve_name(struct composite_context *ctx)
{
	struct composite_context *c = talloc_get_type(ctx->async.private_data,
						      struct composite_context);
	struct pipe_tcp_state *s = talloc_get_type(c->private_data,
						   struct pipe_tcp_state);
	struct composite_context *sock_ipv4_req;

	c->status = resolve_name_recv(ctx, s, &s->address);
	if (!composite_is_ok(c)) return;

	/* prepare server address using host ip:port and transport name */
	s->srvaddr = socket_address_from_strings(s->conn, "ipv4", s->address, s->port);
	if (composite_nomem(s->srvaddr, c)) return;

	/* resolve_nbt_name gives only ipv4 ... - send socket open request */
	sock_ipv4_req = dcerpc_pipe_open_socket_send(c, s->conn, s->localaddr,
						     s->srvaddr, s->target_hostname,
						     NULL,
						     NCACN_IP_TCP);
	composite_continue(c, sock_ipv4_req, continue_ipv4_open_socket, c);
}

/*
  Stage 2 of dcerpc_pipe_open_tcp_send: receive result of pipe open request
  on IPv6 and send the request on IPv4 unless IPv6 transport succeeded.
*/
#if 0 /* disabled till we can resolve names to ipv6 addresses */
static void continue_ipv6_open_socket(struct composite_context *ctx)
{
	struct composite_context *c = talloc_get_type(ctx->async.private_data,
						      struct composite_context);
	struct pipe_tcp_state *s = talloc_get_type(c->private_data,
						   struct pipe_tcp_state);
	struct composite_context *sock_ipv4_req;

	/* receive result of socket open request */
	c->status = dcerpc_pipe_open_socket_recv(ctx);
	if (NT_STATUS_IS_OK(c->status)) {
		composite_done(c);
		return;
	}

	talloc_free(s->srvaddr);

	/* prepare server address using host:ip and transport name */
	s->srvaddr = socket_address_from_strings(s->conn, "ipv4", s->address, s->port);
	if (composite_nomem(s->srvaddr, c)) return;

	/* try IPv4 if IPv6 fails */
	sock_ipv4_req = dcerpc_pipe_open_socket_send(c, s->conn, s->localaddr,
						     s->srvaddr, s->target_hostname, 
						     NCACN_IP_TCP);
	composite_continue(c, sock_ipv4_req, continue_ipv4_open_socket, c);
}
#endif

/*
  Stage 2 of dcerpc_pipe_open_tcp_send: receive result of pipe open request
  on IPv4 transport.
*/
static void continue_ipv4_open_socket(struct composite_context *ctx)
{
	struct composite_context *c = talloc_get_type(ctx->async.private_data,
						      struct composite_context);
	struct pipe_tcp_state *s = talloc_get_type(c->private_data,
						   struct pipe_tcp_state);
	
	/* receive result socket open request */
	c->status = dcerpc_pipe_open_socket_recv(ctx);
	if (!NT_STATUS_IS_OK(c->status)) {
		/* something went wrong... */
		DEBUG(0, ("Failed to connect host %s (%s) on port %d - %s.\n",
			  s->address, s->target_hostname, 
			  s->port, nt_errstr(c->status)));

		composite_error(c, c->status);
		return;
	}

	composite_done(c);
}

/*
  Send rpc pipe open request to given host:port using
  tcp/ip transport
*/
struct composite_context* dcerpc_pipe_open_tcp_send(struct dcecli_connection *conn,
						    const char *localaddr,
						    const char *server,
						    const char *target_hostname,
						    uint32_t port,
						    struct resolve_context *resolve_ctx)
{
	struct composite_context *c;
	struct pipe_tcp_state *s;
	struct composite_context *resolve_req;
	struct nbt_name name;

	/* composite context allocation and setup */
	c = composite_create(conn, conn->event_ctx);
	if (c == NULL) return NULL;

	s = talloc_zero(c, struct pipe_tcp_state);
	if (composite_nomem(s, c)) return c;
	c->private_data = s;

	/* store input parameters in state structure */
	s->server          = talloc_strdup(c, server);
	if (composite_nomem(s->server, c)) return c;
	if (target_hostname) {
		s->target_hostname = talloc_strdup(c, target_hostname);
		if (composite_nomem(s->target_hostname, c)) return c;
	}
	s->port            = port;
	s->conn            = conn;
	s->resolve_ctx     = resolve_ctx;
	if (localaddr) {
		s->localaddr = socket_address_from_strings(s, "ip", localaddr, 0);
		/* if there is no localaddr, we pass NULL for
		   s->localaddr, which is handled by the socket libraries as
		   meaning no local binding address specified */
	}

	make_nbt_name_server(&name, server);
	resolve_req = resolve_name_send(resolve_ctx, s, &name, c->event_ctx);
	composite_continue(c, resolve_req, continue_ip_resolve_name, c);
	return c;
}

/*
  Receive result of pipe open request on tcp/ip
*/
NTSTATUS dcerpc_pipe_open_tcp_recv(struct composite_context *c)
{
	NTSTATUS status;
	status = composite_wait(c);

	talloc_free(c);
	return status;
}


struct pipe_unix_state {
	const char *path;
	struct socket_address *srvaddr;
	struct dcecli_connection *conn;
};


/*
  Stage 2 of dcerpc_pipe_open_unix_stream_send: receive result of pipe open
  request on unix socket.
*/
static void continue_unix_open_socket(struct composite_context *ctx)
{
	struct composite_context *c = talloc_get_type(ctx->async.private_data,
						      struct composite_context);

	c->status = dcerpc_pipe_open_socket_recv(ctx);
	if (NT_STATUS_IS_OK(c->status)) {
		composite_done(c);
		return;
	}

	composite_error(c, c->status);
}


/*
  Send pipe open request on unix socket
*/
struct composite_context *dcerpc_pipe_open_unix_stream_send(struct dcecli_connection *conn,
							    const char *path)
{
	struct composite_context *c;
	struct composite_context *sock_unix_req;
	struct pipe_unix_state *s;

	/* composite context allocation and setup */
	c = composite_create(conn, conn->event_ctx);
	if (c == NULL) return NULL;

	s = talloc_zero(c, struct pipe_unix_state);
	if (composite_nomem(s, c)) return c;
	c->private_data = s;

	/* store parameters in state structure */
	s->path = talloc_strdup(c, path);
	if (composite_nomem(s->path, c)) return c;
	s->conn = conn;

	/* prepare server address using socket path and transport name */
	s->srvaddr = socket_address_from_strings(conn, "unix", s->path, 0);
	if (composite_nomem(s->srvaddr, c)) return c;

	/* send socket open request */
	sock_unix_req = dcerpc_pipe_open_socket_send(c, s->conn, NULL,
						     s->srvaddr, NULL,
						     s->path,
						     NCALRPC);
	composite_continue(c, sock_unix_req, continue_unix_open_socket, c);
	return c;
}


/*
  Receive result of pipe open request on unix socket
*/
NTSTATUS dcerpc_pipe_open_unix_stream_recv(struct composite_context *c)
{
	NTSTATUS status = composite_wait(c);

	talloc_free(c);
	return status;
}


/*
  Stage 2 of dcerpc_pipe_open_pipe_send: receive socket open request
*/
static void continue_np_open_socket(struct composite_context *ctx)
{
	struct composite_context *c = talloc_get_type(ctx->async.private_data,
						      struct composite_context);

	c->status = dcerpc_pipe_open_socket_recv(ctx);
	if (!composite_is_ok(c)) return;

	composite_done(c);
}


/*
  Send pipe open request on ncalrpc
*/
struct composite_context* dcerpc_pipe_open_pipe_send(struct dcecli_connection *conn,
						     const char *ncalrpc_dir,
						     const char *identifier)
{
	char *canon = NULL;

	struct composite_context *c;
	struct composite_context *sock_np_req;
	struct pipe_unix_state *s;

	/* composite context allocation and setup */
	c = composite_create(conn, conn->event_ctx);
	if (c == NULL) return NULL;

	s = talloc_zero(c, struct pipe_unix_state);
	if (composite_nomem(s, c)) return c;
	c->private_data = s;

	/* store parameters in state structure */
	canon = talloc_strdup(s, identifier);
	if (composite_nomem(canon, c)) return c;
	s->conn = conn;

	string_replace(canon, '/', '\\');
	s->path = talloc_asprintf(canon, "%s/%s", ncalrpc_dir, canon);
	if (composite_nomem(s->path, c)) return c;

	/* prepare server address using path and transport name */
	s->srvaddr = socket_address_from_strings(conn, "unix", s->path, 0);
	if (composite_nomem(s->srvaddr, c)) return c;

	/* send socket open request */
	sock_np_req = dcerpc_pipe_open_socket_send(c, s->conn, NULL, s->srvaddr, NULL, s->path, NCALRPC);
	composite_continue(c, sock_np_req, continue_np_open_socket, c);
	return c;
}


/*
  Receive result of pipe open request on ncalrpc
*/
NTSTATUS dcerpc_pipe_open_pipe_recv(struct composite_context *c)
{
	NTSTATUS status = composite_wait(c);
	
	talloc_free(c);
	return status;
}


/*
  Open a rpc pipe on a named pipe - sync version
*/
NTSTATUS dcerpc_pipe_open_pipe(struct dcecli_connection *conn, const char *ncalrpc_dir, const char *identifier)
{
	struct composite_context *c = dcerpc_pipe_open_pipe_send(conn, ncalrpc_dir, identifier);
	return dcerpc_pipe_open_pipe_recv(c);
}

const char *dcerpc_unix_socket_path(struct dcecli_connection *p)
{
	struct sock_private *sock = (struct sock_private *)p->transport.private_data;
	return sock->path;
}

struct socket_address *dcerpc_socket_peer_addr(struct dcecli_connection *p, TALLOC_CTX *mem_ctx)
{
	struct sock_private *sock = (struct sock_private *)p->transport.private_data;
	return socket_get_peer_addr(sock->sock, mem_ctx);
}

