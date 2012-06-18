/* 
   Unix SMB/CIFS implementation.
   Main winbindd server routines

   Copyright (C) Stefan Metzmacher	2005
   Copyright (C) Andrew Tridgell	2005
   
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
#include "../lib/util/dlinklist.h"
#include "lib/events/events.h"
#include "smbd/service_task.h"
#include "smbd/process_model.h"
#include "smbd/service_stream.h"
#include "nsswitch/winbind_nss_config.h"
#include "winbind/wb_server.h"
#include "lib/stream/packet.h"
#include "smbd/service.h"
#include "param/secrets.h"
#include "param/param.h"

void wbsrv_terminate_connection(struct wbsrv_connection *wbconn, const char *reason)
{
	stream_terminate_connection(wbconn->conn, reason);
}

/*
  called on a tcp recv error
*/
static void wbsrv_recv_error(void *private_data, NTSTATUS status)
{
	struct wbsrv_connection *wbconn = talloc_get_type(private_data, struct wbsrv_connection);
	wbsrv_terminate_connection(wbconn, nt_errstr(status));
}

static void wbsrv_accept(struct stream_connection *conn)
{
	struct wbsrv_listen_socket *listen_socket = talloc_get_type(conn->private_data,
								    struct wbsrv_listen_socket);
	struct wbsrv_connection *wbconn;

	wbconn = talloc_zero(conn, struct wbsrv_connection);
	if (!wbconn) {
		stream_terminate_connection(conn, "wbsrv_accept: out of memory");
		return;
	}
	wbconn->conn	      = conn;
	wbconn->listen_socket = listen_socket;
	wbconn->lp_ctx        = listen_socket->service->task->lp_ctx;
	conn->private_data    = wbconn;

	wbconn->packet = packet_init(wbconn);
	if (wbconn->packet == NULL) {
		wbsrv_terminate_connection(wbconn, "wbsrv_accept: out of memory");
		return;
	}
	packet_set_private(wbconn->packet, wbconn);
	packet_set_socket(wbconn->packet, conn->socket);
	packet_set_callback(wbconn->packet, wbsrv_samba3_process);
	packet_set_full_request(wbconn->packet, wbsrv_samba3_packet_full_request);
	packet_set_error_handler(wbconn->packet, wbsrv_recv_error);
	packet_set_event_context(wbconn->packet, conn->event.ctx);
	packet_set_fde(wbconn->packet, conn->event.fde);
	packet_set_serialise(wbconn->packet);
}

/*
  receive some data on a winbind connection
*/
static void wbsrv_recv(struct stream_connection *conn, uint16_t flags)
{
	struct wbsrv_connection *wbconn = talloc_get_type(conn->private_data,
							  struct wbsrv_connection);
	packet_recv(wbconn->packet);

}

/*
  called when we can write to a connection
*/
static void wbsrv_send(struct stream_connection *conn, uint16_t flags)
{
	struct wbsrv_connection *wbconn = talloc_get_type(conn->private_data,
							  struct wbsrv_connection);
	packet_queue_run(wbconn->packet);
}

static const struct stream_server_ops wbsrv_ops = {
	.name			= "winbind samba3 protocol",
	.accept_connection	= wbsrv_accept,
	.recv_handler		= wbsrv_recv,
	.send_handler		= wbsrv_send
};

/*
  startup the winbind task
*/
static void winbind_task_init(struct task_server *task)
{
	uint16_t port = 1;
	const struct model_ops *model_ops;
	NTSTATUS status;
	struct wbsrv_service *service;
	struct wbsrv_listen_socket *listen_socket;

	task_server_set_title(task, "task[winbind]");

	/* within the winbind task we want to be a single process, so
	   ask for the single process model ops and pass these to the
	   stream_setup_socket() call. */
	model_ops = process_model_startup(task->event_ctx, "single");
	if (!model_ops) {
		task_server_terminate(task,
				      "Can't find 'single' process model_ops", true);
		return;
	}

	/* Make sure the directory for the Samba3 socket exists, and is of the correct permissions */
	if (!directory_create_or_exist(lp_winbindd_socket_directory(task->lp_ctx), geteuid(), 0755)) {
		task_server_terminate(task,
				      "Cannot create winbindd pipe directory", true);
		return;
	}

	/* Make sure the directory for the Samba3 socket exists, and is of the correct permissions */
	if (!directory_create_or_exist(lp_winbindd_privileged_socket_directory(task->lp_ctx), geteuid(), 0750)) {
		task_server_terminate(task,
				      "Cannot create winbindd privileged pipe directory", true);
		return;
	}

	service = talloc_zero(task, struct wbsrv_service);
	if (!service) goto nomem;
	service->task	= task;

	status = wbsrv_setup_domains(service);
	if (!NT_STATUS_IS_OK(status)) {
		task_server_terminate(task, nt_errstr(status), true);
		return;
	}

	service->idmap_ctx = idmap_init(service, task->event_ctx, task->lp_ctx);
	if (service->idmap_ctx == NULL) {
		task_server_terminate(task, "Failed to load idmap database", true);
		return;
	}

	/* setup the unprivileged samba3 socket */
	listen_socket = talloc(service, struct wbsrv_listen_socket);
	if (!listen_socket) goto nomem;
	listen_socket->socket_path	= talloc_asprintf(listen_socket, "%s/%s", 
							  lp_winbindd_socket_directory(task->lp_ctx), 
							  WINBINDD_SAMBA3_SOCKET);
	if (!listen_socket->socket_path) goto nomem;
	listen_socket->service		= service;
	listen_socket->privileged	= false;
	status = stream_setup_socket(task->event_ctx, task->lp_ctx, model_ops,
				     &wbsrv_ops, "unix",
				     listen_socket->socket_path, &port,
				     lp_socket_options(task->lp_ctx), 
				     listen_socket);
	if (!NT_STATUS_IS_OK(status)) goto listen_failed;

	/* setup the privileged samba3 socket */
	listen_socket = talloc(service, struct wbsrv_listen_socket);
	if (!listen_socket) goto nomem;
	listen_socket->socket_path 
		= service->priv_socket_path 
		= talloc_asprintf(listen_socket, "%s/%s", 
							  lp_winbindd_privileged_socket_directory(task->lp_ctx), 
							  WINBINDD_SAMBA3_SOCKET);
	if (!listen_socket->socket_path) goto nomem;
	if (!listen_socket->socket_path) goto nomem;
	listen_socket->service		= service;
	listen_socket->privileged	= true;
	status = stream_setup_socket(task->event_ctx, task->lp_ctx, model_ops,
				     &wbsrv_ops, "unix",
				     listen_socket->socket_path, &port,
				     lp_socket_options(task->lp_ctx), 
				     listen_socket);
	if (!NT_STATUS_IS_OK(status)) goto listen_failed;

	status = wbsrv_init_irpc(service);
	if (!NT_STATUS_IS_OK(status)) goto irpc_failed;

	return;

listen_failed:
	DEBUG(0,("stream_setup_socket(path=%s) failed - %s\n",
		 listen_socket->socket_path, nt_errstr(status)));
	task_server_terminate(task, nt_errstr(status), true);
	return;
irpc_failed:
	DEBUG(0,("wbsrv_init_irpc() failed - %s\n",
		 nt_errstr(status)));
	task_server_terminate(task, nt_errstr(status), true);
	return;
nomem:
	task_server_terminate(task, nt_errstr(NT_STATUS_NO_MEMORY), true);
	return;
}

/*
  register ourselves as a available server
*/
NTSTATUS server_service_winbind_init(void)
{
	return register_server_service("winbind", winbind_task_init);
}
