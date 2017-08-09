/* 
   Unix SMB/CIFS implementation.
   Main winbindd server routines

   Copyright (C) Stefan Metzmacher	2005-2008
   Copyright (C) Andrew Tridgell	2005
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2010
   
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
#include "winbind/wb_server.h"
#include "lib/stream/packet.h"
#include "lib/tsocket/tsocket.h"
#include "libcli/util/tstream.h"
#include "param/param.h"
#include "param/secrets.h"

void wbsrv_terminate_connection(struct wbsrv_connection *wbconn, const char *reason)
{
	stream_terminate_connection(wbconn->conn, reason);
}

static void wbsrv_call_loop(struct tevent_req *subreq)
{
	struct wbsrv_connection *wbsrv_conn = tevent_req_callback_data(subreq,
				      struct wbsrv_connection);
	struct wbsrv_samba3_call *call;
	NTSTATUS status;

	call = talloc_zero(wbsrv_conn, struct wbsrv_samba3_call);
	if (call == NULL) {
		wbsrv_terminate_connection(wbsrv_conn, "wbsrv_call_loop: "
				"no memory for wbsrv_samba3_call");
		return;
	}
	call->wbconn = wbsrv_conn;

	status = tstream_read_pdu_blob_recv(subreq,
					    call,
					    &call->in);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		const char *reason;

		reason = talloc_asprintf(call, "wbsrv_call_loop: "
					 "tstream_read_pdu_blob_recv() - %s",
					 nt_errstr(status));
		if (!reason) {
			reason = nt_errstr(status);
		}

		wbsrv_terminate_connection(wbsrv_conn, reason);
		return;
	}

	DEBUG(10,("Received winbind TCP packet of length %lu from %s\n",
		 (long) call->in.length,
		 tsocket_address_string(wbsrv_conn->conn->remote_address, call)));

	status = wbsrv_samba3_process(call);
	if (!NT_STATUS_IS_OK(status)) {
		const char *reason;

		reason = talloc_asprintf(call, "wbsrv_call_loop: "
					 "tstream_read_pdu_blob_recv() - %s",
					 nt_errstr(status));
		if (!reason) {
			reason = nt_errstr(status);
		}

		wbsrv_terminate_connection(wbsrv_conn, reason);
		return;
	}

	/*
	 * The winbind pdu's has the length as 4 byte (initial_read_size),
	 * wbsrv_samba3_packet_full_request provides the pdu length then.
	 */
	subreq = tstream_read_pdu_blob_send(wbsrv_conn,
					    wbsrv_conn->conn->event.ctx,
					    wbsrv_conn->tstream,
					    4, /* initial_read_size */
					    wbsrv_samba3_packet_full_request,
					    wbsrv_conn);
	if (subreq == NULL) {
		wbsrv_terminate_connection(wbsrv_conn, "wbsrv_call_loop: "
				"no memory for tstream_read_pdu_blob_send");
		return;
	}
	tevent_req_set_callback(subreq, wbsrv_call_loop, wbsrv_conn);
}

static void wbsrv_accept(struct stream_connection *conn)
{
	struct wbsrv_listen_socket *wbsrv_socket = talloc_get_type(conn->private_data,
								   struct wbsrv_listen_socket);
	struct wbsrv_connection *wbsrv_conn;
	struct tevent_req *subreq;
	int rc;

	wbsrv_conn = talloc_zero(conn, struct wbsrv_connection);
	if (wbsrv_conn == NULL) {
		stream_terminate_connection(conn, "wbsrv_accept: out of memory");
		return;
	}

	wbsrv_conn->send_queue = tevent_queue_create(conn, "wbsrv_accept");
	if (wbsrv_conn->send_queue == NULL) {
		stream_terminate_connection(conn,
				"wbsrv_accept: out of memory");
		return;
	}

	TALLOC_FREE(conn->event.fde);

	rc = tstream_bsd_existing_socket(wbsrv_conn,
			socket_get_fd(conn->socket),
			&wbsrv_conn->tstream);
	if (rc < 0) {
		stream_terminate_connection(conn,
				"wbsrv_accept: out of memory");
		return;
	}

	wbsrv_conn->conn = conn;
	wbsrv_conn->listen_socket = wbsrv_socket;
	wbsrv_conn->lp_ctx = wbsrv_socket->service->task->lp_ctx;
	conn->private_data = wbsrv_conn;

	/*
	 * The winbind pdu's has the length as 4 byte (initial_read_size),
	 * wbsrv_samba3_packet_full_request provides the pdu length then.
	 */
	subreq = tstream_read_pdu_blob_send(wbsrv_conn,
					    wbsrv_conn->conn->event.ctx,
					    wbsrv_conn->tstream,
					    4, /* initial_read_size */
					    wbsrv_samba3_packet_full_request,
					    wbsrv_conn);
	if (subreq == NULL) {
		wbsrv_terminate_connection(wbsrv_conn, "wbsrv_accept: "
				"no memory for tstream_read_pdu_blob_send");
		return;
	}
	tevent_req_set_callback(subreq, wbsrv_call_loop, wbsrv_conn);
}

/*
  called on a tcp recv
*/
static void wbsrv_recv(struct stream_connection *conn, uint16_t flags)
{
	struct wbsrv_connection *wbsrv_conn = talloc_get_type(conn->private_data,
							struct wbsrv_connection);
	wbsrv_terminate_connection(wbsrv_conn, "wbsrv_recv: called");
}

/*
  called when we can write to a connection
*/
static void wbsrv_send(struct stream_connection *conn, uint16_t flags)
{
	struct wbsrv_connection *wbsrv_conn = talloc_get_type(conn->private_data,
							struct wbsrv_connection);
	/* this should never be triggered! */
	wbsrv_terminate_connection(wbsrv_conn, "wbsrv_send: called");
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
	char *errstring;
	struct dom_sid *primary_sid;

	task_server_set_title(task, "task[winbind]");

	/* within the winbind task we want to be a single process, so
	   ask for the single process model ops and pass these to the
	   stream_setup_socket() call. */
	model_ops = process_model_startup("single");
	if (!model_ops) {
		task_server_terminate(task,
				      "Can't find 'single' process model_ops", true);
		return;
	}

	/* Make sure the directory for the Samba3 socket exists, and is of the correct permissions */
	if (!directory_create_or_exist(lpcfg_winbindd_socket_directory(task->lp_ctx), geteuid(), 0755)) {
		task_server_terminate(task,
				      "Cannot create winbindd pipe directory", true);
		return;
	}

	/* Make sure the directory for the Samba3 socket exists, and is of the correct permissions */
	if (!directory_create_or_exist(lpcfg_winbindd_privileged_socket_directory(task->lp_ctx), geteuid(), 0750)) {
		task_server_terminate(task,
				      "Cannot create winbindd privileged pipe directory", true);
		return;
	}

	service = talloc_zero(task, struct wbsrv_service);
	if (!service) goto nomem;
	service->task	= task;


	/* Find the primary SID, depending if we are a standalone
	 * server (what good is winbind in this case, but anyway...),
	 * or are in a domain as a member or a DC */
	switch (lpcfg_server_role(service->task->lp_ctx)) {
	case ROLE_STANDALONE:
		primary_sid = secrets_get_domain_sid(service,
						     service->task->lp_ctx,
						     lpcfg_netbios_name(service->task->lp_ctx),
						     &service->sec_channel_type,
						     &errstring);
		if (!primary_sid) {
			char *message = talloc_asprintf(task, 
							"Cannot start Winbind (standalone configuration): %s: "
							"Have you provisioned this server (%s) or changed it's name?", 
							errstring, lpcfg_netbios_name(service->task->lp_ctx));
			task_server_terminate(task, message, true);
			return;
		}
		break;
	case ROLE_DOMAIN_MEMBER:
		primary_sid = secrets_get_domain_sid(service,
						     service->task->lp_ctx,
						     lpcfg_workgroup(service->task->lp_ctx),
						     &service->sec_channel_type,
						     &errstring);
		if (!primary_sid) {
			char *message = talloc_asprintf(task, "Cannot start Winbind (domain member): %s: "
							"Have you joined the %s domain?", 
							errstring, lpcfg_workgroup(service->task->lp_ctx));
			task_server_terminate(task, message, true);
			return;
		}
		break;
	case ROLE_DOMAIN_CONTROLLER:
		primary_sid = secrets_get_domain_sid(service,
						     service->task->lp_ctx,
						     lpcfg_workgroup(service->task->lp_ctx),
						     &service->sec_channel_type,
						     &errstring);
		if (!primary_sid) {
			char *message = talloc_asprintf(task, "Cannot start Winbind (domain controller): %s: "
							"Have you provisioned the %s domain?", 
							errstring, lpcfg_workgroup(service->task->lp_ctx));
			task_server_terminate(task, message, true);
			return;
		}
		break;
	}
	service->primary_sid = primary_sid;

	service->idmap_ctx = idmap_init(service, task->event_ctx, task->lp_ctx);
	if (service->idmap_ctx == NULL) {
		task_server_terminate(task, "Failed to load idmap database", true);
		return;
	}

	service->priv_pipe_dir = lpcfg_winbindd_privileged_socket_directory(task->lp_ctx);
	service->pipe_dir = lpcfg_winbindd_socket_directory(task->lp_ctx);

	/* setup the unprivileged samba3 socket */
	listen_socket = talloc(service, struct wbsrv_listen_socket);
	if (!listen_socket) goto nomem;
	listen_socket->socket_path	= talloc_asprintf(listen_socket, "%s/%s", 
							  service->pipe_dir, 
							  WINBINDD_SOCKET_NAME);
	if (!listen_socket->socket_path) goto nomem;
	listen_socket->service		= service;
	listen_socket->privileged	= false;
	status = stream_setup_socket(task, task->event_ctx, task->lp_ctx, model_ops,
				     &wbsrv_ops, "unix",
				     listen_socket->socket_path, &port,
				     lpcfg_socket_options(task->lp_ctx),
				     listen_socket);
	if (!NT_STATUS_IS_OK(status)) goto listen_failed;

	/* setup the privileged samba3 socket */
	listen_socket = talloc(service, struct wbsrv_listen_socket);
	if (!listen_socket) goto nomem;
	listen_socket->socket_path 
		= talloc_asprintf(listen_socket, "%s/%s", 
				  service->priv_pipe_dir,
				  WINBINDD_SOCKET_NAME);
	if (!listen_socket->socket_path) goto nomem;
	listen_socket->service		= service;
	listen_socket->privileged	= true;
	status = stream_setup_socket(task, task->event_ctx, task->lp_ctx, model_ops,
				     &wbsrv_ops, "unix",
				     listen_socket->socket_path, &port,
				     lpcfg_socket_options(task->lp_ctx),
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
