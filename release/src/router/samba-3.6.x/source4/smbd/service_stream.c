/* 
   Unix SMB/CIFS implementation.

   helper functions for stream based servers

   Copyright (C) Andrew Tridgell 2003-2005
   Copyright (C) Stefan (metze) Metzmacher	2004
   
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
#include <tevent.h>
#include "process_model.h"
#include "lib/messaging/irpc.h"
#include "cluster/cluster.h"
#include "param/param.h"
#include "../lib/tsocket/tsocket.h"

/* the range of ports to try for dcerpc over tcp endpoints */
#define SERVER_TCP_LOW_PORT  1024
#define SERVER_TCP_HIGH_PORT 1300

/* size of listen() backlog in smbd */
#define SERVER_LISTEN_BACKLOG 10


/*
  private structure for a single listening stream socket
*/
struct stream_socket {
	const struct stream_server_ops *ops;
	struct loadparm_context *lp_ctx;
	struct tevent_context *event_ctx;
	const struct model_ops *model_ops;
	struct socket_context *sock;
	void *private_data;
};


/*
  close the socket and shutdown a stream_connection
*/
void stream_terminate_connection(struct stream_connection *srv_conn, const char *reason)
{
	struct tevent_context *event_ctx = srv_conn->event.ctx;
	const struct model_ops *model_ops = srv_conn->model_ops;

	if (!reason) reason = "unknown reason";

	DEBUG(3,("Terminating connection - '%s'\n", reason));

	srv_conn->terminate = reason;

	if (srv_conn->processing) {
		/* 
		 * if we're currently inside the stream_io_handler(),
		 * defer the termination to the end of stream_io_hendler()
		 *
		 * and we don't want to read or write to the connection...
		 */
		tevent_fd_set_flags(srv_conn->event.fde, 0);
		return;
	}

	talloc_free(srv_conn->event.fde);
	srv_conn->event.fde = NULL;
	model_ops->terminate(event_ctx, srv_conn->lp_ctx, reason);
	talloc_free(srv_conn);
}

/**
  the select loop has indicated that a stream is ready for IO
*/
static void stream_io_handler(struct stream_connection *conn, uint16_t flags)
{
	conn->processing++;
	if (flags & TEVENT_FD_WRITE) {
		conn->ops->send_handler(conn, flags);
	} else if (flags & TEVENT_FD_READ) {
		conn->ops->recv_handler(conn, flags);
	}
	conn->processing--;

	if (conn->terminate) {
		stream_terminate_connection(conn, conn->terminate);
	}
}

void stream_io_handler_fde(struct tevent_context *ev, struct tevent_fd *fde,
				  uint16_t flags, void *private_data)
{
	struct stream_connection *conn = talloc_get_type(private_data,
							 struct stream_connection);
	stream_io_handler(conn, flags);
}

void stream_io_handler_callback(void *private_data, uint16_t flags)
{
	struct stream_connection *conn = talloc_get_type(private_data,
							 struct stream_connection);
	stream_io_handler(conn, flags);
}

/*
  this creates a stream_connection from an already existing connection,
  used for protocols, where a client connection needs to switched into
  a server connection
*/
NTSTATUS stream_new_connection_merge(struct tevent_context *ev,
				     struct loadparm_context *lp_ctx,
				     const struct model_ops *model_ops,
				     const struct stream_server_ops *stream_ops,
				     struct messaging_context *msg_ctx,
				     void *private_data,
				     struct stream_connection **_srv_conn)
{
	struct stream_connection *srv_conn;

	srv_conn = talloc_zero(ev, struct stream_connection);
	NT_STATUS_HAVE_NO_MEMORY(srv_conn);

	srv_conn->private_data  = private_data;
	srv_conn->model_ops     = model_ops;
	srv_conn->socket	= NULL;
	srv_conn->server_id	= cluster_id(0, 0);
	srv_conn->ops           = stream_ops;
	srv_conn->msg_ctx	= msg_ctx;
	srv_conn->event.ctx	= ev;
	srv_conn->lp_ctx	= lp_ctx;
	srv_conn->event.fde	= NULL;

	*_srv_conn = srv_conn;
	return NT_STATUS_OK;
}

/*
  called when a new socket connection has been established. This is called in the process
  context of the new process (if appropriate)
*/
static void stream_new_connection(struct tevent_context *ev,
				  struct loadparm_context *lp_ctx,
				  struct socket_context *sock, 
				  struct server_id server_id, void *private_data)
{
	struct stream_socket *stream_socket = talloc_get_type(private_data, struct stream_socket);
	struct stream_connection *srv_conn;

	srv_conn = talloc_zero(ev, struct stream_connection);
	if (!srv_conn) {
		DEBUG(0,("talloc(mem_ctx, struct stream_connection) failed\n"));
		return;
	}

	talloc_steal(srv_conn, sock);

	srv_conn->private_data	= stream_socket->private_data;
	srv_conn->model_ops     = stream_socket->model_ops;
	srv_conn->socket	= sock;
	srv_conn->server_id	= server_id;
	srv_conn->ops           = stream_socket->ops;
	srv_conn->event.ctx	= ev;
	srv_conn->lp_ctx	= lp_ctx;

	if (!socket_check_access(sock, "smbd", lpcfg_hostsallow(NULL, lpcfg_default_service(lp_ctx)), lpcfg_hostsdeny(NULL, lpcfg_default_service(lp_ctx)))) {
		stream_terminate_connection(srv_conn, "denied by access rules");
		return;
	}

	srv_conn->event.fde	= tevent_add_fd(ev, srv_conn, socket_get_fd(sock),
						0, stream_io_handler_fde, srv_conn);
	if (!srv_conn->event.fde) {
		stream_terminate_connection(srv_conn, "tevent_add_fd() failed");
		return;
	}

	/* setup to receive internal messages on this connection */
	srv_conn->msg_ctx = messaging_init(srv_conn, 
					   lpcfg_messaging_path(srv_conn, lp_ctx),
					   srv_conn->server_id, ev);
	if (!srv_conn->msg_ctx) {
		stream_terminate_connection(srv_conn, "messaging_init() failed");
		return;
	}

	srv_conn->remote_address = socket_get_remote_addr(srv_conn->socket, srv_conn);
	if (!srv_conn->remote_address) {
		stream_terminate_connection(srv_conn, "socket_get_remote_addr() failed");
		return;
	}

	srv_conn->local_address = socket_get_local_addr(srv_conn->socket, srv_conn);
	if (!srv_conn->local_address) {
		stream_terminate_connection(srv_conn, "socket_get_local_addr() failed");
		return;
	}

	{
		TALLOC_CTX *tmp_ctx;
		const char *title;

		tmp_ctx = talloc_new(srv_conn);

		title = talloc_asprintf(tmp_ctx, "conn[%s] c[%s] s[%s] server_id[%s]",
					stream_socket->ops->name, 
					tsocket_address_string(srv_conn->remote_address, tmp_ctx),
					tsocket_address_string(srv_conn->local_address, tmp_ctx),
					cluster_id_string(tmp_ctx, server_id));
		if (title) {
			stream_connection_set_title(srv_conn, title);
		}
		talloc_free(tmp_ctx);
	}

	/* we're now ready to start receiving events on this stream */
	TEVENT_FD_READABLE(srv_conn->event.fde);

	/* call the server specific accept code */
	stream_socket->ops->accept_connection(srv_conn);
}


/*
  called when someone opens a connection to one of our listening ports
*/
static void stream_accept_handler(struct tevent_context *ev, struct tevent_fd *fde, 
				  uint16_t flags, void *private_data)
{
	struct stream_socket *stream_socket = talloc_get_type(private_data, struct stream_socket);

	/* ask the process model to create us a process for this new
	   connection.  When done, it calls stream_new_connection()
	   with the newly created socket */
	stream_socket->model_ops->accept_connection(ev, stream_socket->lp_ctx,
						    stream_socket->sock, 
						    stream_new_connection, stream_socket);
}

/*
  setup a listen stream socket
  if you pass *port == 0, then a port > 1024 is used

  FIXME: This function is TCP/IP specific - uses an int rather than 
  	 a string for the port. Should leave allocating a port nr 
         to the socket implementation - JRV20070903
 */
NTSTATUS stream_setup_socket(TALLOC_CTX *mem_ctx,
			     struct tevent_context *event_context,
			     struct loadparm_context *lp_ctx,
			     const struct model_ops *model_ops,
			     const struct stream_server_ops *stream_ops,
			     const char *family,
			     const char *sock_addr,
			     uint16_t *port,
			     const char *socket_options,
			     void *private_data)
{
	NTSTATUS status;
	struct stream_socket *stream_socket;
	struct socket_address *socket_address;
	struct tevent_fd *fde;
	int i;

	stream_socket = talloc_zero(mem_ctx, struct stream_socket);
	NT_STATUS_HAVE_NO_MEMORY(stream_socket);

	status = socket_create(family, SOCKET_TYPE_STREAM, &stream_socket->sock, 0);
	NT_STATUS_NOT_OK_RETURN(status);

	talloc_steal(stream_socket, stream_socket->sock);

	stream_socket->lp_ctx = talloc_reference(stream_socket, lp_ctx);

	/* ready to listen */
	status = socket_set_option(stream_socket->sock, "SO_KEEPALIVE", NULL);
	NT_STATUS_NOT_OK_RETURN(status);

	if (socket_options != NULL) {
		status = socket_set_option(stream_socket->sock, socket_options, NULL);
		NT_STATUS_NOT_OK_RETURN(status);
	}

	/* TODO: set socket ACL's (host allow etc) here when they're
	 * implemented */

	/* Some sockets don't have a port, or are just described from
	 * the string.  We are indicating this by having port == NULL */
	if (!port) {
		socket_address = socket_address_from_strings(stream_socket, 
							     stream_socket->sock->backend_name,
							     sock_addr, 0);
		NT_STATUS_HAVE_NO_MEMORY(socket_address);
		status = socket_listen(stream_socket->sock, socket_address, SERVER_LISTEN_BACKLOG, 0);
		talloc_free(socket_address);

	} else if (*port == 0) {
		for (i=SERVER_TCP_LOW_PORT;i<= SERVER_TCP_HIGH_PORT;i++) {
			socket_address = socket_address_from_strings(stream_socket, 
								     stream_socket->sock->backend_name,
								     sock_addr, i);
			NT_STATUS_HAVE_NO_MEMORY(socket_address);
			status = socket_listen(stream_socket->sock, socket_address, 
					       SERVER_LISTEN_BACKLOG, 0);
			talloc_free(socket_address);
			if (NT_STATUS_IS_OK(status)) {
				*port = i;
				break;
			}
		}
	} else {
		socket_address = socket_address_from_strings(stream_socket, 
							     stream_socket->sock->backend_name,
							     sock_addr, *port);
		NT_STATUS_HAVE_NO_MEMORY(socket_address);
		status = socket_listen(stream_socket->sock, socket_address, SERVER_LISTEN_BACKLOG, 0);
		talloc_free(socket_address);
	}

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("Failed to listen on %s:%u - %s\n",
			 sock_addr, port ? (unsigned int)(*port) : 0,
			 nt_errstr(status)));
		talloc_free(stream_socket);
		return status;
	}

	/* Add the FD from the newly created socket into the event
	 * subsystem.  it will call the accept handler whenever we get
	 * new connections */

	fde = tevent_add_fd(event_context, stream_socket->sock,
			    socket_get_fd(stream_socket->sock),
			    TEVENT_FD_READ,
			    stream_accept_handler, stream_socket);
	if (!fde) {
		DEBUG(0,("Failed to setup fd event\n"));
		talloc_free(stream_socket);
		return NT_STATUS_NO_MEMORY;
	}

	/* we let events system to the close on the socket. This avoids
	 * nasty interactions with waiting for talloc to close the socket. */
	tevent_fd_set_close_fn(fde, socket_tevent_fd_close_fn);
	socket_set_flags(stream_socket->sock, SOCKET_FLAG_NOCLOSE);

	stream_socket->private_data     = talloc_reference(stream_socket, private_data);
	stream_socket->ops              = stream_ops;
	stream_socket->event_ctx	= event_context;
	stream_socket->model_ops        = model_ops;

	return NT_STATUS_OK;
}

/*
  setup a connection title 
*/
void stream_connection_set_title(struct stream_connection *conn, const char *title)
{
	conn->model_ops->set_title(conn->event.ctx, title);
}
