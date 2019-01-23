/* 
   Unix SMB/CIFS implementation.

   LDAP server

   Copyright (C) Andrew Tridgell 2005
   Copyright (C) Volker Lendecke 2004
   Copyright (C) Stefan Metzmacher 2004

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
#include "system/network.h"
#include "lib/events/events.h"
#include "auth/auth.h"
#include "auth/credentials/credentials.h"
#include "librpc/gen_ndr/ndr_samr.h"
#include "../lib/util/dlinklist.h"
#include "../lib/util/asn1.h"
#include "ldap_server/ldap_server.h"
#include "smbd/service_task.h"
#include "smbd/service_stream.h"
#include "smbd/service.h"
#include "smbd/process_model.h"
#include "lib/tls/tls.h"
#include "lib/messaging/irpc.h"
#include <ldb.h>
#include <ldb_errors.h>
#include "libcli/ldap/ldap_proto.h"
#include "system/network.h"
#include "lib/socket/netif.h"
#include "dsdb/samdb/samdb.h"
#include "param/param.h"
#include "../lib/tsocket/tsocket.h"
#include "../lib/util/tevent_ntstatus.h"
#include "../libcli/util/tstream.h"

static void ldapsrv_terminate_connection_done(struct tevent_req *subreq);

/*
  close the socket and shutdown a server_context
*/
static void ldapsrv_terminate_connection(struct ldapsrv_connection *conn,
					 const char *reason)
{
	struct tevent_req *subreq;

	if (conn->limits.reason) {
		return;
	}

	conn->limits.endtime = timeval_current_ofs(0, 500);

	tevent_queue_stop(conn->sockets.send_queue);
	if (conn->active_call) {
		tevent_req_cancel(conn->active_call);
		conn->active_call = NULL;
	}

	conn->limits.reason = talloc_strdup(conn, reason);
	if (conn->limits.reason == NULL) {
		TALLOC_FREE(conn->sockets.tls);
		TALLOC_FREE(conn->sockets.sasl);
		TALLOC_FREE(conn->sockets.raw);
		stream_terminate_connection(conn->connection, reason);
		return;
	}

	subreq = tstream_disconnect_send(conn,
					 conn->connection->event.ctx,
					 conn->sockets.active);
	if (subreq == NULL) {
		TALLOC_FREE(conn->sockets.tls);
		TALLOC_FREE(conn->sockets.sasl);
		TALLOC_FREE(conn->sockets.raw);
		stream_terminate_connection(conn->connection, reason);
		return;
	}
	tevent_req_set_endtime(subreq,
			       conn->connection->event.ctx,
			       conn->limits.endtime);
	tevent_req_set_callback(subreq, ldapsrv_terminate_connection_done, conn);
}

static void ldapsrv_terminate_connection_done(struct tevent_req *subreq)
{
	struct ldapsrv_connection *conn =
		tevent_req_callback_data(subreq,
		struct ldapsrv_connection);
	int ret;
	int sys_errno;

	ret = tstream_disconnect_recv(subreq, &sys_errno);
	TALLOC_FREE(subreq);

	if (conn->sockets.active == conn->sockets.raw) {
		TALLOC_FREE(conn->sockets.tls);
		TALLOC_FREE(conn->sockets.sasl);
		TALLOC_FREE(conn->sockets.raw);
		stream_terminate_connection(conn->connection,
					    conn->limits.reason);
		return;
	}

	TALLOC_FREE(conn->sockets.tls);
	TALLOC_FREE(conn->sockets.sasl);
	conn->sockets.active = conn->sockets.raw;

	subreq = tstream_disconnect_send(conn,
					 conn->connection->event.ctx,
					 conn->sockets.active);
	if (subreq == NULL) {
		TALLOC_FREE(conn->sockets.raw);
		stream_terminate_connection(conn->connection,
					    conn->limits.reason);
		return;
	}
	tevent_req_set_endtime(subreq,
			       conn->connection->event.ctx,
			       conn->limits.endtime);
	tevent_req_set_callback(subreq, ldapsrv_terminate_connection_done, conn);
}

/*
  called when a LDAP socket becomes readable
*/
void ldapsrv_recv(struct stream_connection *c, uint16_t flags)
{
	smb_panic(__location__);
}

/*
  called when a LDAP socket becomes writable
*/
static void ldapsrv_send(struct stream_connection *c, uint16_t flags)
{
	smb_panic(__location__);
}

static int ldapsrv_load_limits(struct ldapsrv_connection *conn)
{
	TALLOC_CTX *tmp_ctx;
	const char *attrs[] = { "configurationNamingContext", NULL };
	const char *attrs2[] = { "lDAPAdminLimits", NULL };
	struct ldb_message_element *el;
	struct ldb_result *res = NULL;
	struct ldb_dn *basedn;
	struct ldb_dn *conf_dn;
	struct ldb_dn *policy_dn;
	unsigned int i;
	int ret;

	/* set defaults limits in case of failure */
	conn->limits.initial_timeout = 120;
	conn->limits.conn_idle_time = 900;
	conn->limits.max_page_size = 1000;
	conn->limits.search_timeout = 120;


	tmp_ctx = talloc_new(conn);
	if (tmp_ctx == NULL) {
		return -1;
	}

	basedn = ldb_dn_new(tmp_ctx, conn->ldb, NULL);
	if (basedn == NULL) {
		goto failed;
	}

	ret = ldb_search(conn->ldb, tmp_ctx, &res, basedn, LDB_SCOPE_BASE, attrs, NULL);
	if (ret != LDB_SUCCESS) {
		goto failed;
	}

	if (res->count != 1) {
		goto failed;
	}

	conf_dn = ldb_msg_find_attr_as_dn(conn->ldb, tmp_ctx, res->msgs[0], "configurationNamingContext");
	if (conf_dn == NULL) {
		goto failed;
	}

	policy_dn = ldb_dn_copy(tmp_ctx, conf_dn);
	ldb_dn_add_child_fmt(policy_dn, "CN=Default Query Policy,CN=Query-Policies,CN=Directory Service,CN=Windows NT,CN=Services");
	if (policy_dn == NULL) {
		goto failed;
	}

	ret = ldb_search(conn->ldb, tmp_ctx, &res, policy_dn, LDB_SCOPE_BASE, attrs2, NULL);
	if (ret != LDB_SUCCESS) {
		goto failed;
	}

	if (res->count != 1) {
		goto failed;
	}

	el = ldb_msg_find_element(res->msgs[0], "lDAPAdminLimits");
	if (el == NULL) {
		goto failed;
	}

	for (i = 0; i < el->num_values; i++) {
		char policy_name[256];
		int policy_value, s;

		s = sscanf((const char *)el->values[i].data, "%255[^=]=%d", policy_name, &policy_value);
		if (ret != 2 || policy_value == 0)
			continue;

		if (strcasecmp("InitRecvTimeout", policy_name) == 0) {
			conn->limits.initial_timeout = policy_value;
			continue;
		}
		if (strcasecmp("MaxConnIdleTime", policy_name) == 0) {
			conn->limits.conn_idle_time = policy_value;
			continue;
		}
		if (strcasecmp("MaxPageSize", policy_name) == 0) {
			conn->limits.max_page_size = policy_value;
			continue;
		}
		if (strcasecmp("MaxQueryDuration", policy_name) == 0) {
			conn->limits.search_timeout = policy_value;
			continue;
		}
	}

	return 0;

failed:
	DEBUG(0, ("Failed to load ldap server query policies\n"));
	talloc_free(tmp_ctx);
	return -1;
}

static struct tevent_req *ldapsrv_process_call_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct tevent_queue *call_queue,
						    struct ldapsrv_call *call);
static NTSTATUS ldapsrv_process_call_recv(struct tevent_req *req);

static bool ldapsrv_call_read_next(struct ldapsrv_connection *conn);
static void ldapsrv_accept_tls_done(struct tevent_req *subreq);

/*
  initialise a server_context from a open socket and register a event handler
  for reading from that socket
*/
static void ldapsrv_accept(struct stream_connection *c,
			   struct auth_session_info *session_info,
			   bool is_privileged)
{
	struct ldapsrv_service *ldapsrv_service = 
		talloc_get_type(c->private_data, struct ldapsrv_service);
	struct ldapsrv_connection *conn;
	struct cli_credentials *server_credentials;
	struct socket_address *socket_address;
	NTSTATUS status;
	int port;
	int ret;
	struct tevent_req *subreq;
	struct timeval endtime;

	conn = talloc_zero(c, struct ldapsrv_connection);
	if (!conn) {
		stream_terminate_connection(c, "ldapsrv_accept: out of memory");
		return;
	}
	conn->is_privileged = is_privileged;

	conn->sockets.send_queue = tevent_queue_create(conn, "ldapsev send queue");
	if (conn->sockets.send_queue == NULL) {
		stream_terminate_connection(c,
					    "ldapsrv_accept: tevent_queue_create failed");
		return;
	}

	TALLOC_FREE(c->event.fde);

	ret = tstream_bsd_existing_socket(conn,
					  socket_get_fd(c->socket),
					  &conn->sockets.raw);
	if (ret == -1) {
		stream_terminate_connection(c,
					    "ldapsrv_accept: out of memory");
		return;
	}
	socket_set_flags(c->socket, SOCKET_FLAG_NOCLOSE);

	conn->connection  = c;
	conn->service     = ldapsrv_service;
	conn->lp_ctx      = ldapsrv_service->task->lp_ctx;

	c->private_data   = conn;

	socket_address = socket_get_my_addr(c->socket, conn);
	if (!socket_address) {
		ldapsrv_terminate_connection(conn, "ldapsrv_accept: failed to obtain local socket address!");
		return;
	}
	port = socket_address->port;
	talloc_free(socket_address);
	if (port == 3268 || port == 3269) /* Global catalog */ {
		conn->global_catalog = true;
	}

	server_credentials = cli_credentials_init(conn);
	if (!server_credentials) {
		stream_terminate_connection(c, "Failed to init server credentials\n");
		return;
	}

	cli_credentials_set_conf(server_credentials, conn->lp_ctx);
	status = cli_credentials_set_machine_account(server_credentials, conn->lp_ctx);
	if (!NT_STATUS_IS_OK(status)) {
		stream_terminate_connection(c, talloc_asprintf(conn, "Failed to obtain server credentials, perhaps a standalone server?: %s\n", nt_errstr(status)));
		return;
	}
	conn->server_credentials = server_credentials;

	conn->session_info = session_info;

	if (!NT_STATUS_IS_OK(ldapsrv_backend_Init(conn))) {
		ldapsrv_terminate_connection(conn, "backend Init failed");
		return;
	}

	/* load limits from the conf partition */
	ldapsrv_load_limits(conn); /* should we fail on error ? */

	/* register the server */	
	irpc_add_name(c->msg_ctx, "ldap_server");

	conn->sockets.active = conn->sockets.raw;

	if (port != 636 && port != 3269) {
		ldapsrv_call_read_next(conn);
		return;
	}

	endtime = timeval_current_ofs(conn->limits.conn_idle_time, 0);

	subreq = tstream_tls_accept_send(conn,
					 conn->connection->event.ctx,
					 conn->sockets.raw,
					 conn->service->tls_params);
	if (subreq == NULL) {
		ldapsrv_terminate_connection(conn, "ldapsrv_accept: "
				"no memory for tstream_tls_accept_send");
		return;
	}
	tevent_req_set_endtime(subreq,
			       conn->connection->event.ctx,
			       endtime);
	tevent_req_set_callback(subreq, ldapsrv_accept_tls_done, conn);
}

static void ldapsrv_accept_tls_done(struct tevent_req *subreq)
{
	struct ldapsrv_connection *conn =
		tevent_req_callback_data(subreq,
		struct ldapsrv_connection);
	int ret;
	int sys_errno;

	ret = tstream_tls_accept_recv(subreq, &sys_errno,
				      conn, &conn->sockets.tls);
	TALLOC_FREE(subreq);
	if (ret == -1) {
		const char *reason;

		reason = talloc_asprintf(conn, "ldapsrv_accept_tls_loop: "
					 "tstream_tls_accept_recv() - %d:%s",
					 sys_errno, strerror(sys_errno));
		if (!reason) {
			reason = "ldapsrv_accept_tls_loop: "
				 "tstream_tls_accept_recv() - failed";
		}

		ldapsrv_terminate_connection(conn, reason);
		return;
	}

	conn->sockets.active = conn->sockets.tls;
	ldapsrv_call_read_next(conn);
}

static void ldapsrv_call_read_done(struct tevent_req *subreq);

static bool ldapsrv_call_read_next(struct ldapsrv_connection *conn)
{
	struct tevent_req *subreq;

	if (timeval_is_zero(&conn->limits.endtime)) {
		conn->limits.endtime =
			timeval_current_ofs(conn->limits.initial_timeout, 0);
	} else {
		conn->limits.endtime =
			timeval_current_ofs(conn->limits.conn_idle_time, 0);
	}

	/*
	 * The minimun size of a LDAP pdu is 7 bytes
	 *
	 * dumpasn1 -hh ldap-unbind-min.dat
	 *
	 *     <30 05 02 01 09 42 00>
	 *    0    5: SEQUENCE {
	 *     <02 01 09>
	 *    2    1:   INTEGER 9
	 *     <42 00>
	 *    5    0:   [APPLICATION 2]
	 *          :     Error: Object has zero length.
	 *          :   }
	 *
	 * dumpasn1 -hh ldap-unbind-windows.dat
	 *
	 *     <30 84 00 00 00 05 02 01 09 42 00>
	 *    0    5: SEQUENCE {
	 *     <02 01 09>
	 *    6    1:   INTEGER 9
	 *     <42 00>
	 *    9    0:   [APPLICATION 2]
	 *          :     Error: Object has zero length.
	 *          :   }
	 *
	 * This means using an initial read size
	 * of 7 is ok.
	 */
	subreq = tstream_read_pdu_blob_send(conn,
					    conn->connection->event.ctx,
					    conn->sockets.active,
					    7, /* initial_read_size */
					    ldap_full_packet,
					    conn);
	if (subreq == NULL) {
		ldapsrv_terminate_connection(conn, "ldapsrv_call_read_next: "
				"no memory for tstream_read_pdu_blob_send");
		return false;
	}
	tevent_req_set_endtime(subreq,
			       conn->connection->event.ctx,
			       conn->limits.endtime);
	tevent_req_set_callback(subreq, ldapsrv_call_read_done, conn);
	return true;
}

static void ldapsrv_call_process_done(struct tevent_req *subreq);

static void ldapsrv_call_read_done(struct tevent_req *subreq)
{
	struct ldapsrv_connection *conn =
		tevent_req_callback_data(subreq,
		struct ldapsrv_connection);
	NTSTATUS status;
	struct ldapsrv_call *call;
	struct asn1_data *asn1;
	DATA_BLOB blob;

	call = talloc_zero(conn, struct ldapsrv_call);
	if (!call) {
		ldapsrv_terminate_connection(conn, "no memory");
		return;
	}

	call->conn = conn;

	status = tstream_read_pdu_blob_recv(subreq,
					    call,
					    &blob);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		const char *reason;

		reason = talloc_asprintf(call, "ldapsrv_call_loop: "
					 "tstream_read_pdu_blob_recv() - %s",
					 nt_errstr(status));
		if (!reason) {
			reason = nt_errstr(status);
		}

		ldapsrv_terminate_connection(conn, reason);
		return;
	}

	asn1 = asn1_init(call);
	if (asn1 == NULL) {
		ldapsrv_terminate_connection(conn, "no memory");
		return;
	}

	call->request = talloc(call, struct ldap_message);
	if (call->request == NULL) {
		ldapsrv_terminate_connection(conn, "no memory");
		return;
	}

	if (!asn1_load(asn1, blob)) {
		ldapsrv_terminate_connection(conn, "asn1_load failed");
		return;
	}

	status = ldap_decode(asn1, samba_ldap_control_handlers(),
			     call->request);
	if (!NT_STATUS_IS_OK(status)) {
		ldapsrv_terminate_connection(conn, nt_errstr(status));
		return;
	}

	data_blob_free(&blob);


	/* queue the call in the global queue */
	subreq = ldapsrv_process_call_send(call,
					   conn->connection->event.ctx,
					   conn->service->call_queue,
					   call);
	if (subreq == NULL) {
		ldapsrv_terminate_connection(conn, "ldapsrv_process_call_send failed");
		return;
	}
	tevent_req_set_callback(subreq, ldapsrv_call_process_done, call);
	conn->active_call = subreq;
}

static void ldapsrv_call_writev_done(struct tevent_req *subreq);

static void ldapsrv_call_process_done(struct tevent_req *subreq)
{
	struct ldapsrv_call *call =
		tevent_req_callback_data(subreq,
		struct ldapsrv_call);
	struct ldapsrv_connection *conn = call->conn;
	NTSTATUS status;
	DATA_BLOB blob = data_blob_null;

	conn->active_call = NULL;

	status = ldapsrv_process_call_recv(subreq);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		ldapsrv_terminate_connection(conn, nt_errstr(status));
		return;
	}

	/* build all the replies into a single blob */
	while (call->replies) {
		DATA_BLOB b;
		bool ret;

		if (!ldap_encode(call->replies->msg, samba_ldap_control_handlers(), &b, call)) {
			DEBUG(0,("Failed to encode ldap reply of type %d\n",
				 call->replies->msg->type));
			ldapsrv_terminate_connection(conn, "ldap_encode failed");
			return;
		}

		ret = data_blob_append(call, &blob, b.data, b.length);
		data_blob_free(&b);

		talloc_set_name_const(blob.data, "Outgoing, encoded LDAP packet");

		if (!ret) {
			ldapsrv_terminate_connection(conn, "data_blob_append failed");
			return;
		}

		DLIST_REMOVE(call->replies, call->replies);
	}

	if (blob.length == 0) {
		TALLOC_FREE(call);

		ldapsrv_call_read_next(conn);
		return;
	}

	call->out_iov.iov_base = blob.data;
	call->out_iov.iov_len = blob.length;

	subreq = tstream_writev_queue_send(call,
					   conn->connection->event.ctx,
					   conn->sockets.active,
					   conn->sockets.send_queue,
					   &call->out_iov, 1);
	if (subreq == NULL) {
		ldapsrv_terminate_connection(conn, "stream_writev_queue_send failed");
		return;
	}
	tevent_req_set_callback(subreq, ldapsrv_call_writev_done, call);
}

static void ldapsrv_call_postprocess_done(struct tevent_req *subreq);

static void ldapsrv_call_writev_done(struct tevent_req *subreq)
{
	struct ldapsrv_call *call =
		tevent_req_callback_data(subreq,
		struct ldapsrv_call);
	struct ldapsrv_connection *conn = call->conn;
	int sys_errno;
	int rc;

	rc = tstream_writev_queue_recv(subreq, &sys_errno);
	TALLOC_FREE(subreq);
	if (rc == -1) {
		const char *reason;

		reason = talloc_asprintf(call, "ldapsrv_call_writev_done: "
					 "tstream_writev_queue_recv() - %d:%s",
					 sys_errno, strerror(sys_errno));
		if (reason == NULL) {
			reason = "ldapsrv_call_writev_done: "
				 "tstream_writev_queue_recv() failed";
		}

		ldapsrv_terminate_connection(conn, reason);
		return;
	}

	if (call->postprocess_send) {
		subreq = call->postprocess_send(call,
						conn->connection->event.ctx,
						call->postprocess_private);
		if (subreq == NULL) {
			ldapsrv_terminate_connection(conn, "ldapsrv_call_writev_done: "
					"call->postprocess_send - no memory");
			return;
		}
		tevent_req_set_callback(subreq,
					ldapsrv_call_postprocess_done,
					call);
		return;
	}

	TALLOC_FREE(call);

	ldapsrv_call_read_next(conn);
}

static void ldapsrv_call_postprocess_done(struct tevent_req *subreq)
{
	struct ldapsrv_call *call =
		tevent_req_callback_data(subreq,
		struct ldapsrv_call);
	struct ldapsrv_connection *conn = call->conn;
	NTSTATUS status;

	status = call->postprocess_recv(subreq);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		const char *reason;

		reason = talloc_asprintf(call, "ldapsrv_call_postprocess_done: "
					 "call->postprocess_recv() - %s",
					 nt_errstr(status));
		if (reason == NULL) {
			reason = nt_errstr(status);
		}

		ldapsrv_terminate_connection(conn, reason);
		return;
	}

	TALLOC_FREE(call);

	ldapsrv_call_read_next(conn);
}

struct ldapsrv_process_call_state {
	struct ldapsrv_call *call;
};

static void ldapsrv_process_call_trigger(struct tevent_req *req,
					 void *private_data);

static struct tevent_req *ldapsrv_process_call_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct tevent_queue *call_queue,
						    struct ldapsrv_call *call)
{
	struct tevent_req *req;
	struct ldapsrv_process_call_state *state;
	bool ok;

	req = tevent_req_create(mem_ctx, &state,
				struct ldapsrv_process_call_state);
	if (req == NULL) {
		return req;
	}

	state->call = call;

	ok = tevent_queue_add(call_queue, ev, req,
			      ldapsrv_process_call_trigger, NULL);
	if (!ok) {
		tevent_req_nomem(NULL, req);
		return tevent_req_post(req, ev);
	}

	return req;
}

static void ldapsrv_process_call_trigger(struct tevent_req *req,
					 void *private_data)
{
	struct ldapsrv_process_call_state *state =
		tevent_req_data(req,
		struct ldapsrv_process_call_state);
	NTSTATUS status;

	/* make the call */
	status = ldapsrv_do_call(state->call);
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}

	tevent_req_done(req);
}

static NTSTATUS ldapsrv_process_call_recv(struct tevent_req *req)
{
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		tevent_req_received(req);
		return status;
	}

	tevent_req_received(req);
	return NT_STATUS_OK;
}

static void ldapsrv_accept_nonpriv(struct stream_connection *c)
{
	struct ldapsrv_service *ldapsrv_service = talloc_get_type_abort(
		c->private_data, struct ldapsrv_service);
	struct auth_session_info *session_info;
	NTSTATUS status;

	status = auth_anonymous_session_info(
		c, ldapsrv_service->task->lp_ctx, &session_info);
	if (!NT_STATUS_IS_OK(status)) {
		stream_terminate_connection(c, "failed to setup anonymous "
					    "session info");
		return;
	}
	ldapsrv_accept(c, session_info, false);
}

static const struct stream_server_ops ldap_stream_nonpriv_ops = {
	.name			= "ldap",
	.accept_connection	= ldapsrv_accept_nonpriv,
	.recv_handler		= ldapsrv_recv,
	.send_handler		= ldapsrv_send,
};

/* The feature removed behind an #ifdef until we can do it properly
 * with an EXTERNAL bind. */

#define WITH_LDAPI_PRIV_SOCKET

#ifdef WITH_LDAPI_PRIV_SOCKET
static void ldapsrv_accept_priv(struct stream_connection *c)
{
	struct ldapsrv_service *ldapsrv_service = talloc_get_type_abort(
		c->private_data, struct ldapsrv_service);
	struct auth_session_info *session_info;

	session_info = system_session(ldapsrv_service->task->lp_ctx);
	if (!session_info) {
		stream_terminate_connection(c, "failed to setup system "
					    "session info");
		return;
	}
	ldapsrv_accept(c, session_info, true);
}

static const struct stream_server_ops ldap_stream_priv_ops = {
	.name			= "ldap",
	.accept_connection	= ldapsrv_accept_priv,
	.recv_handler		= ldapsrv_recv,
	.send_handler		= ldapsrv_send,
};

#endif


/*
  add a socket address to the list of events, one event per port
*/
static NTSTATUS add_socket(struct task_server *task,
			   struct loadparm_context *lp_ctx,
			   const struct model_ops *model_ops,
			   const char *address, struct ldapsrv_service *ldap_service)
{
	uint16_t port = 389;
	NTSTATUS status;
	struct ldb_context *ldb;

	status = stream_setup_socket(task, task->event_ctx, lp_ctx,
				     model_ops, &ldap_stream_nonpriv_ops,
				     "ipv4", address, &port, 
				     lpcfg_socket_options(lp_ctx),
				     ldap_service);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("ldapsrv failed to bind to %s:%u - %s\n",
			 address, port, nt_errstr(status)));
		return status;
	}

	if (tstream_tls_params_enabled(ldap_service->tls_params)) {
		/* add ldaps server */
		port = 636;
		status = stream_setup_socket(task, task->event_ctx, lp_ctx,
					     model_ops,
					     &ldap_stream_nonpriv_ops,
					     "ipv4", address, &port, 
					     lpcfg_socket_options(lp_ctx),
					     ldap_service);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0,("ldapsrv failed to bind to %s:%u - %s\n",
				 address, port, nt_errstr(status)));
			return status;
		}
	}

	/* Load LDAP database, but only to read our settings */
	ldb = samdb_connect(ldap_service, ldap_service->task->event_ctx, 
			    lp_ctx, system_session(lp_ctx), 0);
	if (!ldb) {
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	if (samdb_is_gc(ldb)) {
		port = 3268;
		status = stream_setup_socket(task, task->event_ctx, lp_ctx,
					     model_ops,
					     &ldap_stream_nonpriv_ops,
					     "ipv4", address, &port, 
					     lpcfg_socket_options(lp_ctx),
					     ldap_service);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0,("ldapsrv failed to bind to %s:%u - %s\n",
				 address, port, nt_errstr(status)));
			return status;
		}
		if (tstream_tls_params_enabled(ldap_service->tls_params)) {
			/* add ldaps server for the global catalog */
			port = 3269;
			status = stream_setup_socket(task, task->event_ctx, lp_ctx,
						     model_ops,
						     &ldap_stream_nonpriv_ops,
						     "ipv4", address, &port,
						     lpcfg_socket_options(lp_ctx),
						     ldap_service);
			if (!NT_STATUS_IS_OK(status)) {
				DEBUG(0,("ldapsrv failed to bind to %s:%u - %s\n",
					 address, port, nt_errstr(status)));
				return status;
			}
		}
	}

	/* And once we are bound, free the temporary ldb, it will
	 * connect again on each incoming LDAP connection */
	talloc_unlink(ldap_service, ldb);

	return NT_STATUS_OK;
}

/*
  open the ldap server sockets
*/
static void ldapsrv_task_init(struct task_server *task)
{	
	char *ldapi_path;
#ifdef WITH_LDAPI_PRIV_SOCKET
	char *priv_dir;
#endif
	const char *dns_host_name;
	struct ldapsrv_service *ldap_service;
	NTSTATUS status;
	const struct model_ops *model_ops;

	switch (lpcfg_server_role(task->lp_ctx)) {
	case ROLE_STANDALONE:
		task_server_terminate(task, "ldap_server: no LDAP server required in standalone configuration", 
				      false);
		return;
	case ROLE_DOMAIN_MEMBER:
		task_server_terminate(task, "ldap_server: no LDAP server required in member server configuration", 
				      false);
		return;
	case ROLE_DOMAIN_CONTROLLER:
		/* Yes, we want an LDAP server */
		break;
	}

	task_server_set_title(task, "task[ldapsrv]");

	/* run the ldap server as a single process */
	model_ops = process_model_startup("single");
	if (!model_ops) goto failed;

	ldap_service = talloc_zero(task, struct ldapsrv_service);
	if (ldap_service == NULL) goto failed;

	ldap_service->task = task;

	dns_host_name = talloc_asprintf(ldap_service, "%s.%s",
					lpcfg_netbios_name(task->lp_ctx),
					lpcfg_dnsdomain(task->lp_ctx));
	if (dns_host_name == NULL) goto failed;

	status = tstream_tls_params_server(ldap_service,
					   dns_host_name,
					   lpcfg_tls_enabled(task->lp_ctx),
					   lpcfg_tls_keyfile(ldap_service, task->lp_ctx),
					   lpcfg_tls_certfile(ldap_service, task->lp_ctx),
					   lpcfg_tls_cafile(ldap_service, task->lp_ctx),
					   lpcfg_tls_crlfile(ldap_service, task->lp_ctx),
					   lpcfg_tls_dhpfile(ldap_service, task->lp_ctx),
					   &ldap_service->tls_params);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("ldapsrv failed tstream_tls_patams_server - %s\n",
			 nt_errstr(status)));
		goto failed;
	}

	ldap_service->call_queue = tevent_queue_create(ldap_service, "ldapsrv_call_queue");
	if (ldap_service->call_queue == NULL) goto failed;

	if (lpcfg_interfaces(task->lp_ctx) && lpcfg_bind_interfaces_only(task->lp_ctx)) {
		struct interface *ifaces;
		int num_interfaces;
		int i;

		load_interfaces(task, lpcfg_interfaces(task->lp_ctx), &ifaces);
		num_interfaces = iface_count(ifaces);

		/* We have been given an interfaces line, and been 
		   told to only bind to those interfaces. Create a
		   socket per interface and bind to only these.
		*/
		for(i = 0; i < num_interfaces; i++) {
			const char *address = iface_n_ip(ifaces, i);
			status = add_socket(task, task->lp_ctx, model_ops, address, ldap_service);
			if (!NT_STATUS_IS_OK(status)) goto failed;
		}
	} else {
		status = add_socket(task, task->lp_ctx, model_ops,
				    lpcfg_socket_address(task->lp_ctx), ldap_service);
		if (!NT_STATUS_IS_OK(status)) goto failed;
	}

	ldapi_path = private_path(ldap_service, task->lp_ctx, "ldapi");
	if (!ldapi_path) {
		goto failed;
	}

	status = stream_setup_socket(task, task->event_ctx, task->lp_ctx,
				     model_ops, &ldap_stream_nonpriv_ops,
				     "unix", ldapi_path, NULL, 
				     lpcfg_socket_options(task->lp_ctx),
				     ldap_service);
	talloc_free(ldapi_path);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("ldapsrv failed to bind to %s - %s\n",
			 ldapi_path, nt_errstr(status)));
	}

#ifdef WITH_LDAPI_PRIV_SOCKET
	priv_dir = private_path(ldap_service, task->lp_ctx, "ldap_priv");
	if (priv_dir == NULL) {
		goto failed;
	}
	/*
	 * Make sure the directory for the privileged ldapi socket exists, and
	 * is of the correct permissions
	 */
	if (!directory_create_or_exist(priv_dir, geteuid(), 0750)) {
		task_server_terminate(task, "Cannot create ldap "
				      "privileged ldapi directory", true);
		return;
	}
	ldapi_path = talloc_asprintf(ldap_service, "%s/ldapi", priv_dir);
	talloc_free(priv_dir);
	if (ldapi_path == NULL) {
		goto failed;
	}

	status = stream_setup_socket(task, task->event_ctx, task->lp_ctx,
				     model_ops, &ldap_stream_priv_ops,
				     "unix", ldapi_path, NULL,
				     lpcfg_socket_options(task->lp_ctx),
				     ldap_service);
	talloc_free(ldapi_path);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("ldapsrv failed to bind to %s - %s\n",
			 ldapi_path, nt_errstr(status)));
	}

#endif
	return;

failed:
	task_server_terminate(task, "Failed to startup ldap server task", true);
}


NTSTATUS server_service_ldap_init(void)
{
	return register_server_service("ldap", ldapsrv_task_init);
}
