/* 
   Unix SMB/CIFS implementation.
   Samba internal messaging functions
   Copyright (C) 2007 by Volker Lendecke
   Copyright (C) 2007 by Andrew Tridgell

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
#include "util_tdb.h"

#ifdef CLUSTER_SUPPORT

#include "ctdbd_conn.h"
#include "packet.h"
#include "messages.h"

/* paths to these include files come from --with-ctdb= in configure */
#include "ctdb.h"
#include "ctdb_private.h"

struct ctdbd_connection {
	struct messaging_context *msg_ctx;
	uint32 reqid;
	uint32 our_vnn;
	uint64 rand_srvid;
	struct packet_context *pkt;
	struct fd_event *fde;

	void (*release_ip_handler)(const char *ip_addr, void *private_data);
	void *release_ip_priv;
};

static NTSTATUS ctdbd_control(struct ctdbd_connection *conn,
			      uint32_t vnn, uint32 opcode, 
			      uint64_t srvid, uint32_t flags, TDB_DATA data, 
			      TALLOC_CTX *mem_ctx, TDB_DATA *outdata,
			      int *cstatus);

/*
 * exit on fatal communications errors with the ctdbd daemon
 */
static void cluster_fatal(const char *why)
{
	DEBUG(0,("cluster fatal event: %s - exiting immediately\n", why));
	/* we don't use smb_panic() as we don't want to delay to write
	   a core file. We need to release this process id immediately
	   so that someone else can take over without getting sharing
	   violations */
	_exit(1);
}

/*
 *
 */
static void ctdb_packet_dump(struct ctdb_req_header *hdr)
{
	if (DEBUGLEVEL < 10) {
		return;
	}
	DEBUGADD(10, ("len=%d, magic=%x, vers=%d, gen=%d, op=%d, reqid=%d\n",
		      (int)hdr->length, (int)hdr->ctdb_magic,
		      (int)hdr->ctdb_version, (int)hdr->generation,
		      (int)hdr->operation, (int)hdr->reqid));
}

/*
 * Register a srvid with ctdbd
 */
static NTSTATUS register_with_ctdbd(struct ctdbd_connection *conn,
				    uint64_t srvid)
{

	int cstatus;
	return ctdbd_control(conn, CTDB_CURRENT_NODE,
			     CTDB_CONTROL_REGISTER_SRVID, srvid, 0,
			     tdb_null, NULL, NULL, &cstatus);
}

/*
 * get our vnn from the cluster
 */
static NTSTATUS get_cluster_vnn(struct ctdbd_connection *conn, uint32 *vnn)
{
	int32_t cstatus=-1;
	NTSTATUS status;
	status = ctdbd_control(conn,
			       CTDB_CURRENT_NODE, CTDB_CONTROL_GET_PNN, 0, 0,
			       tdb_null, NULL, NULL, &cstatus);
	if (!NT_STATUS_IS_OK(status)) {
		cluster_fatal("ctdbd_control failed\n");
	}
	*vnn = (uint32_t)cstatus;
	return status;
}

/*
 * Are we active (i.e. not banned or stopped?)
 */
static bool ctdbd_working(struct ctdbd_connection *conn, uint32_t vnn)
{
	int32_t cstatus=-1;
	NTSTATUS status;
	TDB_DATA outdata;
	struct ctdb_node_map *m;
	uint32_t failure_flags;
	bool ret = false;
	int i;

	status = ctdbd_control(conn, CTDB_CURRENT_NODE,
			       CTDB_CONTROL_GET_NODEMAP, 0, 0,
			       tdb_null, talloc_tos(), &outdata, &cstatus);
	if (!NT_STATUS_IS_OK(status)) {
		cluster_fatal("ctdbd_control failed\n");
	}
	if ((cstatus != 0) || (outdata.dptr == NULL)) {
		DEBUG(2, ("Received invalid ctdb data\n"));
		return false;
	}

	m = (struct ctdb_node_map *)outdata.dptr;

	for (i=0; i<m->num; i++) {
		if (vnn == m->nodes[i].pnn) {
			break;
		}
	}

	if (i == m->num) {
		DEBUG(2, ("Did not find ourselves (node %d) in nodemap\n",
			  (int)vnn));
		goto fail;
	}

	failure_flags = NODE_FLAGS_BANNED | NODE_FLAGS_DISCONNECTED
		| NODE_FLAGS_PERMANENTLY_DISABLED | NODE_FLAGS_STOPPED;

	if ((m->nodes[i].flags & failure_flags) != 0) {
		DEBUG(2, ("Node has status %x, not active\n",
			  (int)m->nodes[i].flags));
		goto fail;
	}

	ret = true;
fail:
	TALLOC_FREE(outdata.dptr);
	return ret;
}

uint32 ctdbd_vnn(const struct ctdbd_connection *conn)
{
	return conn->our_vnn;
}

/*
 * Get us a ctdb connection
 */

static NTSTATUS ctdbd_connect(TALLOC_CTX *mem_ctx,
			      struct packet_context **presult)
{
	struct packet_context *result;
	const char *sockname = lp_ctdbd_socket();
	struct sockaddr_un addr;
	int fd;

	if (!sockname || !*sockname) {
		sockname = CTDB_PATH;
	}

	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd == -1) {
		DEBUG(3, ("Could not create socket: %s\n", strerror(errno)));
		return map_nt_error_from_unix(errno);
	}

	ZERO_STRUCT(addr);
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, sockname, sizeof(addr.sun_path));

	if (sys_connect(fd, (struct sockaddr *)(void *)&addr) == -1) {
		DEBUG(1, ("connect(%s) failed: %s\n", sockname,
			  strerror(errno)));
		close(fd);
		return map_nt_error_from_unix(errno);
	}

	if (!(result = packet_init(mem_ctx, fd))) {
		close(fd);
		return NT_STATUS_NO_MEMORY;
	}

	*presult = result;
	return NT_STATUS_OK;
}

/*
 * Do we have a complete ctdb packet in the queue?
 */

static bool ctdb_req_complete(const uint8_t *buf, size_t available,
			      size_t *length,
			      void *private_data)
{
	uint32 msglen;

	if (available < sizeof(msglen)) {
		return False;
	}

	msglen = *((uint32 *)buf);

	DEBUG(10, ("msglen = %d\n", msglen));

	if (msglen < sizeof(struct ctdb_req_header)) {
		DEBUG(0, ("Got invalid msglen: %d, expected at least %d for "
			  "the req_header\n", (int)msglen,
			  (int)sizeof(struct ctdb_req_header)));
		cluster_fatal("ctdbd protocol error\n");
	}

	if (available < msglen) {
		return false;
	}

	*length = msglen;
	return true;
}

/*
 * State necessary to defer an incoming message while we are waiting for a
 * ctdb reply.
 */

struct deferred_msg_state {
	struct messaging_context *msg_ctx;
	struct messaging_rec *rec;
};

/*
 * Timed event handler for the deferred message
 */

static void deferred_message_dispatch(struct event_context *event_ctx,
				      struct timed_event *te,
				      struct timeval now,
				      void *private_data)
{
	struct deferred_msg_state *state = talloc_get_type_abort(
		private_data, struct deferred_msg_state);

	messaging_dispatch_rec(state->msg_ctx, state->rec);
	TALLOC_FREE(state);
	TALLOC_FREE(te);
}

struct req_pull_state {
	TALLOC_CTX *mem_ctx;
	DATA_BLOB req;
};

/*
 * Pull a ctdb request out of the incoming packet queue
 */

static NTSTATUS ctdb_req_pull(uint8_t *buf, size_t length,
			      void *private_data)
{
	struct req_pull_state *state = (struct req_pull_state *)private_data;

	state->req.data = talloc_move(state->mem_ctx, &buf);
	state->req.length = length;
	return NT_STATUS_OK;
}

/*
 * Fetch a messaging_rec from an incoming ctdb style message
 */

static struct messaging_rec *ctdb_pull_messaging_rec(TALLOC_CTX *mem_ctx,
						     size_t overall_length,
						     struct ctdb_req_message *msg)
{
	struct messaging_rec *result;
	DATA_BLOB blob;
	enum ndr_err_code ndr_err;

	if ((overall_length < offsetof(struct ctdb_req_message, data))
	    || (overall_length
		< offsetof(struct ctdb_req_message, data) + msg->datalen)) {

		cluster_fatal("got invalid msg length");
	}

	if (!(result = TALLOC_P(mem_ctx, struct messaging_rec))) {
		DEBUG(0, ("talloc failed\n"));
		return NULL;
	}

	blob = data_blob_const(msg->data, msg->datalen);

	ndr_err = ndr_pull_struct_blob(
		&blob, result, result,
		(ndr_pull_flags_fn_t)ndr_pull_messaging_rec);

	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		DEBUG(0, ("ndr_pull_struct_blob failed: %s\n",
			  ndr_errstr(ndr_err)));
		TALLOC_FREE(result);
		return NULL;
	}

	if (DEBUGLEVEL >= 10) {
		DEBUG(10, ("ctdb_pull_messaging_rec:\n"));
		NDR_PRINT_DEBUG(messaging_rec, result);
	}

	return result;
}

static NTSTATUS ctdb_packet_fd_read_sync(struct packet_context *ctx)
{
	int timeout = lp_ctdb_timeout();

	if (timeout == 0) {
		timeout = -1;
	}
	return packet_fd_read_sync(ctx, timeout);
}

/*
 * Read a full ctdbd request. If we have a messaging context, defer incoming
 * messages that might come in between.
 */

static NTSTATUS ctdb_read_req(struct ctdbd_connection *conn, uint32 reqid,
			      TALLOC_CTX *mem_ctx, void *result)
{
	struct ctdb_req_header *hdr;
	struct req_pull_state state;
	NTSTATUS status;

 again:

	status = ctdb_packet_fd_read_sync(conn->pkt);

	if (NT_STATUS_EQUAL(status, NT_STATUS_NETWORK_BUSY)) {
		/* EAGAIN */
		goto again;
	} else if (NT_STATUS_EQUAL(status, NT_STATUS_RETRY)) {
		/* EAGAIN */
		goto again;
	}

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("packet_fd_read failed: %s\n", nt_errstr(status)));
		cluster_fatal("ctdbd died\n");
	}

 next_pkt:

	ZERO_STRUCT(state);
	state.mem_ctx = mem_ctx;

	if (!packet_handler(conn->pkt, ctdb_req_complete, ctdb_req_pull,
			    &state, &status)) {
		/*
		 * Not enough data
		 */
		DEBUG(10, ("not enough data from ctdb socket, retrying\n"));
		goto again;
	}

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("Could not read packet: %s\n", nt_errstr(status)));
		cluster_fatal("ctdbd died\n");
	}

	hdr = (struct ctdb_req_header *)state.req.data;

	DEBUG(10, ("Received ctdb packet\n"));
	ctdb_packet_dump(hdr);

	if (hdr->operation == CTDB_REQ_MESSAGE) {
		struct timed_event *evt;
		struct deferred_msg_state *msg_state;
		struct ctdb_req_message *msg = (struct ctdb_req_message *)hdr;

		if (conn->msg_ctx == NULL) {
			DEBUG(1, ("Got a message without having a msg ctx, "
				  "dropping msg %llu\n",
				  (long long unsigned)msg->srvid));
			goto next_pkt;
		}

		if ((conn->release_ip_handler != NULL)
		    && (msg->srvid == CTDB_SRVID_RELEASE_IP)) {
			/* must be dispatched immediately */
			DEBUG(10, ("received CTDB_SRVID_RELEASE_IP\n"));
			conn->release_ip_handler((const char *)msg->data,
						 conn->release_ip_priv);
			TALLOC_FREE(hdr);
			goto next_pkt;
		}

		if ((msg->srvid == CTDB_SRVID_RECONFIGURE)
		    || (msg->srvid == CTDB_SRVID_SAMBA_NOTIFY)) {

			DEBUG(1, ("ctdb_read_req: Got %s message\n",
				  (msg->srvid == CTDB_SRVID_RECONFIGURE)
				  ? "cluster reconfigure" : "SAMBA_NOTIFY"));

			messaging_send(conn->msg_ctx,
				       messaging_server_id(conn->msg_ctx),
				       MSG_SMB_BRL_VALIDATE, &data_blob_null);
			messaging_send(conn->msg_ctx,
				       messaging_server_id(conn->msg_ctx),
				       MSG_DBWRAP_G_LOCK_RETRY,
				       &data_blob_null);
			TALLOC_FREE(hdr);
			goto next_pkt;
		}

		msg_state = TALLOC_P(NULL, struct deferred_msg_state);
		if (msg_state == NULL) {
			DEBUG(0, ("talloc failed\n"));
			TALLOC_FREE(hdr);
			goto next_pkt;
		}

		if (!(msg_state->rec = ctdb_pull_messaging_rec(
			      msg_state, state.req.length, msg))) {
			DEBUG(0, ("ctdbd_pull_messaging_rec failed\n"));
			TALLOC_FREE(msg_state);
			TALLOC_FREE(hdr);
			goto next_pkt;
		}

		TALLOC_FREE(hdr);

		msg_state->msg_ctx = conn->msg_ctx;

		/*
		 * We're waiting for a call reply, but an async message has
		 * crossed. Defer dispatching to the toplevel event loop.
		 */
		evt = event_add_timed(conn->msg_ctx->event_ctx,
				      conn->msg_ctx->event_ctx,
				      timeval_zero(),
				      deferred_message_dispatch,
				      msg_state);
		if (evt == NULL) {
			DEBUG(0, ("event_add_timed failed\n"));
			TALLOC_FREE(msg_state);
			TALLOC_FREE(hdr);
			goto next_pkt;
		}

		goto next_pkt;
	}

	if (hdr->reqid != reqid) {
		/* we got the wrong reply */
		DEBUG(0,("Discarding mismatched ctdb reqid %u should have "
			 "been %u\n", hdr->reqid, reqid));
		TALLOC_FREE(hdr);
		goto again;
	}

	*((void **)result) = talloc_move(mem_ctx, &hdr);

	return NT_STATUS_OK;
}

/*
 * Get us a ctdbd connection
 */

static NTSTATUS ctdbd_init_connection(TALLOC_CTX *mem_ctx,
				      struct ctdbd_connection **pconn)
{
	struct ctdbd_connection *conn;
	NTSTATUS status;

	if (!(conn = TALLOC_ZERO_P(mem_ctx, struct ctdbd_connection))) {
		DEBUG(0, ("talloc failed\n"));
		return NT_STATUS_NO_MEMORY;
	}

	status = ctdbd_connect(conn, &conn->pkt);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(10, ("ctdbd_connect failed: %s\n", nt_errstr(status)));
		goto fail;
	}

	status = get_cluster_vnn(conn, &conn->our_vnn);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(10, ("get_cluster_vnn failed: %s\n", nt_errstr(status)));
		goto fail;
	}

	if (!ctdbd_working(conn, conn->our_vnn)) {
		DEBUG(2, ("Node is not working, can not connect\n"));
		status = NT_STATUS_INTERNAL_DB_ERROR;
		goto fail;
	}

	generate_random_buffer((unsigned char *)&conn->rand_srvid,
			       sizeof(conn->rand_srvid));

	status = register_with_ctdbd(conn, conn->rand_srvid);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(5, ("Could not register random srvid: %s\n",
			  nt_errstr(status)));
		goto fail;
	}

	*pconn = conn;
	return NT_STATUS_OK;

 fail:
	TALLOC_FREE(conn);
	return status;
}

/*
 * Get us a ctdbd connection and register us as a process
 */

NTSTATUS ctdbd_messaging_connection(TALLOC_CTX *mem_ctx,
				    struct ctdbd_connection **pconn)
{
        struct ctdbd_connection *conn;
	NTSTATUS status;

	status = ctdbd_init_connection(mem_ctx, &conn);

	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	status = register_with_ctdbd(conn, (uint64_t)sys_getpid());
	if (!NT_STATUS_IS_OK(status)) {
		goto fail;
	}

	status = register_with_ctdbd(conn, MSG_SRVID_SAMBA);
	if (!NT_STATUS_IS_OK(status)) {
		goto fail;
	}

	status = register_with_ctdbd(conn, CTDB_SRVID_SAMBA_NOTIFY);
	if (!NT_STATUS_IS_OK(status)) {
		goto fail;
	}

	*pconn = conn;
	return NT_STATUS_OK;

 fail:
	TALLOC_FREE(conn);
	return status;
}

struct messaging_context *ctdb_conn_msg_ctx(struct ctdbd_connection *conn)
{
	return conn->msg_ctx;
}

int ctdbd_conn_get_fd(struct ctdbd_connection *conn)
{
	return packet_get_fd(conn->pkt);
}

/*
 * Packet handler to receive and handle a ctdb message
 */
static NTSTATUS ctdb_handle_message(uint8_t *buf, size_t length,
				    void *private_data)
{
	struct ctdbd_connection *conn = talloc_get_type_abort(
		private_data, struct ctdbd_connection);
	struct ctdb_req_message *msg;
	struct messaging_rec *msg_rec;

	msg = (struct ctdb_req_message *)buf;

	if (msg->hdr.operation != CTDB_REQ_MESSAGE) {
		DEBUG(0, ("Received async msg of type %u, discarding\n",
			  msg->hdr.operation));
		TALLOC_FREE(buf);
		return NT_STATUS_INVALID_PARAMETER;
	}

	if ((conn->release_ip_handler != NULL)
	    && (msg->srvid == CTDB_SRVID_RELEASE_IP)) {
		/* must be dispatched immediately */
		DEBUG(10, ("received CTDB_SRVID_RELEASE_IP\n"));
		conn->release_ip_handler((const char *)msg->data,
					 conn->release_ip_priv);
		TALLOC_FREE(buf);
		return NT_STATUS_OK;
	}

	SMB_ASSERT(conn->msg_ctx != NULL);

	if ((msg->srvid == CTDB_SRVID_RECONFIGURE)
	    || (msg->srvid == CTDB_SRVID_SAMBA_NOTIFY)){
		DEBUG(0,("Got cluster reconfigure message\n"));
		/*
		 * when the cluster is reconfigured or someone of the
		 * family has passed away (SAMBA_NOTIFY), we need to
		 * clean the brl database
		 */
		messaging_send(conn->msg_ctx,
			       messaging_server_id(conn->msg_ctx),
			       MSG_SMB_BRL_VALIDATE, &data_blob_null);

		messaging_send(conn->msg_ctx,
			       messaging_server_id(conn->msg_ctx),
			       MSG_DBWRAP_G_LOCK_RETRY,
			       &data_blob_null);

		TALLOC_FREE(buf);
		return NT_STATUS_OK;
	}

	/* only messages to our pid or the broadcast are valid here */
	if (msg->srvid != sys_getpid() && msg->srvid != MSG_SRVID_SAMBA) {
		DEBUG(0,("Got unexpected message with srvid=%llu\n", 
			 (unsigned long long)msg->srvid));
		TALLOC_FREE(buf);
		return NT_STATUS_OK;
	}

	if (!(msg_rec = ctdb_pull_messaging_rec(NULL, length, msg))) {
		DEBUG(10, ("ctdb_pull_messaging_rec failed\n"));
		TALLOC_FREE(buf);
		return NT_STATUS_NO_MEMORY;
	}

	messaging_dispatch_rec(conn->msg_ctx, msg_rec);

	TALLOC_FREE(msg_rec);
	TALLOC_FREE(buf);
	return NT_STATUS_OK;
}

/*
 * The ctdbd socket is readable asynchronuously
 */

static void ctdbd_socket_handler(struct event_context *event_ctx,
				 struct fd_event *event,
				 uint16 flags,
				 void *private_data)
{
	struct ctdbd_connection *conn = talloc_get_type_abort(
		private_data, struct ctdbd_connection);

	NTSTATUS status;

	status = packet_fd_read(conn->pkt);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("packet_fd_read failed: %s\n", nt_errstr(status)));
		cluster_fatal("ctdbd died\n");
	}

	while (packet_handler(conn->pkt, ctdb_req_complete,
			      ctdb_handle_message, conn, &status)) {
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(10, ("could not handle incoming message: %s\n",
				   nt_errstr(status)));
		}
	}
}

/*
 * Prepare a ctdbd connection to receive messages
 */

NTSTATUS ctdbd_register_msg_ctx(struct ctdbd_connection *conn,
				struct messaging_context *msg_ctx)
{
	SMB_ASSERT(conn->msg_ctx == NULL);
	SMB_ASSERT(conn->fde == NULL);

	if (!(conn->fde = event_add_fd(msg_ctx->event_ctx, conn,
				       packet_get_fd(conn->pkt),
				       EVENT_FD_READ,
				       ctdbd_socket_handler,
				       conn))) {
		DEBUG(0, ("event_add_fd failed\n"));
		return NT_STATUS_NO_MEMORY;
	}

	conn->msg_ctx = msg_ctx;

	return NT_STATUS_OK;
}

/*
 * Send a messaging message across a ctdbd
 */

NTSTATUS ctdbd_messaging_send(struct ctdbd_connection *conn,
			      uint32 dst_vnn, uint64 dst_srvid,
			      struct messaging_rec *msg)
{
	struct ctdb_req_message r;
	TALLOC_CTX *mem_ctx;
	DATA_BLOB blob;
	NTSTATUS status;
	enum ndr_err_code ndr_err;

	if (!(mem_ctx = talloc_init("ctdbd_messaging_send"))) {
		DEBUG(0, ("talloc failed\n"));
		return NT_STATUS_NO_MEMORY;
	}

	ndr_err = ndr_push_struct_blob(
		&blob, mem_ctx, msg,
		(ndr_push_flags_fn_t)ndr_push_messaging_rec);

	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		DEBUG(0, ("ndr_push_struct_blob failed: %s\n",
			  ndr_errstr(ndr_err)));
		status = ndr_map_error2ntstatus(ndr_err);
		goto fail;
	}

	r.hdr.length = offsetof(struct ctdb_req_message, data) + blob.length;
	r.hdr.ctdb_magic = CTDB_MAGIC;
	r.hdr.ctdb_version = CTDB_VERSION;
	r.hdr.generation = 1;
	r.hdr.operation  = CTDB_REQ_MESSAGE;
	r.hdr.destnode   = dst_vnn;
	r.hdr.srcnode    = conn->our_vnn;
	r.hdr.reqid      = 0;
	r.srvid          = dst_srvid;
	r.datalen        = blob.length;

	DEBUG(10, ("ctdbd_messaging_send: Sending ctdb packet\n"));
	ctdb_packet_dump(&r.hdr);

	status = packet_send(
		conn->pkt, 2,
		data_blob_const(&r, offsetof(struct ctdb_req_message, data)),
		blob);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("packet_send failed: %s\n", nt_errstr(status)));
		goto fail;
	}

	status = packet_flush(conn->pkt);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(3, ("write to ctdbd failed: %s\n", nt_errstr(status)));
		cluster_fatal("cluster dispatch daemon msg write error\n");
	}

	status = NT_STATUS_OK;
 fail:
	TALLOC_FREE(mem_ctx);
	return status;
}

/*
 * send/recv a generic ctdb control message
 */
static NTSTATUS ctdbd_control(struct ctdbd_connection *conn,
			      uint32_t vnn, uint32 opcode, 
			      uint64_t srvid, uint32_t flags, 
			      TDB_DATA data, 
			      TALLOC_CTX *mem_ctx, TDB_DATA *outdata,
			      int *cstatus)
{
	struct ctdb_req_control req;
	struct ctdb_reply_control *reply = NULL;
	struct ctdbd_connection *new_conn = NULL;
	NTSTATUS status;

	/* the samba3 ctdb code can't handle NOREPLY yet */
	flags &= ~CTDB_CTRL_FLAG_NOREPLY;

	if (conn == NULL) {
		status = ctdbd_init_connection(NULL, &new_conn);

		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(10, ("Could not init temp connection: %s\n",
				   nt_errstr(status)));
			goto fail;
		}

		conn = new_conn;
	}

	ZERO_STRUCT(req);
	req.hdr.length = offsetof(struct ctdb_req_control, data) + data.dsize;
	req.hdr.ctdb_magic   = CTDB_MAGIC;
	req.hdr.ctdb_version = CTDB_VERSION;
	req.hdr.operation    = CTDB_REQ_CONTROL;
	req.hdr.reqid        = ++conn->reqid;
	req.hdr.destnode     = vnn;
	req.opcode           = opcode;
	req.srvid            = srvid;
	req.datalen          = data.dsize;
	req.flags            = flags;

	DEBUG(10, ("ctdbd_control: Sending ctdb packet\n"));
	ctdb_packet_dump(&req.hdr);

	status = packet_send(
		conn->pkt, 2,
		data_blob_const(&req, offsetof(struct ctdb_req_control, data)),
		data_blob_const(data.dptr, data.dsize));

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(3, ("packet_send failed: %s\n", nt_errstr(status)));
		goto fail;
	}

	status = packet_flush(conn->pkt);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(3, ("write to ctdbd failed: %s\n", nt_errstr(status)));
		cluster_fatal("cluster dispatch daemon control write error\n");
	}

	if (flags & CTDB_CTRL_FLAG_NOREPLY) {
		TALLOC_FREE(new_conn);
		if (cstatus) {
			*cstatus = 0;
		}
		return NT_STATUS_OK;
	}

	status = ctdb_read_req(conn, req.hdr.reqid, NULL, (void *)&reply);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(10, ("ctdb_read_req failed: %s\n", nt_errstr(status)));
		goto fail;
	}

	if (reply->hdr.operation != CTDB_REPLY_CONTROL) {
		DEBUG(0, ("received invalid reply\n"));
		goto fail;
	}

	if (outdata) {
		if (!(outdata->dptr = (uint8 *)talloc_memdup(
			      mem_ctx, reply->data, reply->datalen))) {
			TALLOC_FREE(reply);
			return NT_STATUS_NO_MEMORY;
		}
		outdata->dsize = reply->datalen;
	}
	if (cstatus) {
		(*cstatus) = reply->status;
	}

	status = NT_STATUS_OK;

 fail:
	TALLOC_FREE(new_conn);
	TALLOC_FREE(reply);
	return status;
}

/*
 * see if a remote process exists
 */
bool ctdbd_process_exists(struct ctdbd_connection *conn, uint32 vnn, pid_t pid)
{
	NTSTATUS status;
	TDB_DATA data;
	int32_t cstatus;

	data.dptr = (uint8_t*)&pid;
	data.dsize = sizeof(pid);

	status = ctdbd_control(conn, vnn, CTDB_CONTROL_PROCESS_EXISTS, 0, 0,
			       data, NULL, NULL, &cstatus);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, (__location__ " ctdb_control for process_exists "
			  "failed\n"));
		return False;
	}

	return cstatus == 0;
}

/*
 * Get a db path
 */
char *ctdbd_dbpath(struct ctdbd_connection *conn,
		   TALLOC_CTX *mem_ctx, uint32_t db_id)
{
	NTSTATUS status;
	TDB_DATA data;
	int32_t cstatus;

	data.dptr = (uint8_t*)&db_id;
	data.dsize = sizeof(db_id);

	status = ctdbd_control(conn, CTDB_CURRENT_NODE,
			       CTDB_CONTROL_GETDBPATH, 0, 0, data, 
			       mem_ctx, &data, &cstatus);
	if (!NT_STATUS_IS_OK(status) || cstatus != 0) {
		DEBUG(0,(__location__ " ctdb_control for getdbpath failed\n"));
		return NULL;
	}

	return (char *)data.dptr;
}

/*
 * attach to a ctdb database
 */
NTSTATUS ctdbd_db_attach(struct ctdbd_connection *conn,
			 const char *name, uint32_t *db_id, int tdb_flags)
{
	NTSTATUS status;
	TDB_DATA data;
	int32_t cstatus;
	bool persistent = (tdb_flags & TDB_CLEAR_IF_FIRST) == 0;

	data.dptr = (uint8_t*)name;
	data.dsize = strlen(name)+1;

	status = ctdbd_control(conn, CTDB_CURRENT_NODE,
			       persistent
			       ? CTDB_CONTROL_DB_ATTACH_PERSISTENT
			       : CTDB_CONTROL_DB_ATTACH,
			       tdb_flags, 0, data, NULL, &data, &cstatus);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, (__location__ " ctdb_control for db_attach "
			  "failed: %s\n", nt_errstr(status)));
		return status;
	}

	if (cstatus != 0 || data.dsize != sizeof(uint32_t)) {
		DEBUG(0,(__location__ " ctdb_control for db_attach failed\n"));
		return NT_STATUS_INTERNAL_ERROR;
	}

	*db_id = *(uint32_t *)data.dptr;
	talloc_free(data.dptr);

	if (!(tdb_flags & TDB_SEQNUM)) {
		return NT_STATUS_OK;
	}

	data.dptr = (uint8_t *)db_id;
	data.dsize = sizeof(*db_id);

	status = ctdbd_control(conn, CTDB_CURRENT_NODE,
			       CTDB_CONTROL_ENABLE_SEQNUM, 0, 0, data, 
			       NULL, NULL, &cstatus);
	if (!NT_STATUS_IS_OK(status) || cstatus != 0) {
		DEBUG(0,(__location__ " ctdb_control for enable seqnum "
			 "failed\n"));
		return NT_STATUS_IS_OK(status) ? NT_STATUS_INTERNAL_ERROR :
			status;
	}

	return NT_STATUS_OK;
}

/*
 * force the migration of a record to this node
 */
NTSTATUS ctdbd_migrate(struct ctdbd_connection *conn, uint32 db_id,
		       TDB_DATA key)
{
	struct ctdb_req_call req;
	struct ctdb_reply_call *reply;
	NTSTATUS status;

	ZERO_STRUCT(req);

	req.hdr.length = offsetof(struct ctdb_req_call, data) + key.dsize;
	req.hdr.ctdb_magic   = CTDB_MAGIC;
	req.hdr.ctdb_version = CTDB_VERSION;
	req.hdr.operation    = CTDB_REQ_CALL;
	req.hdr.reqid        = ++conn->reqid;
	req.flags            = CTDB_IMMEDIATE_MIGRATION;
	req.callid           = CTDB_NULL_FUNC;
	req.db_id            = db_id;
	req.keylen           = key.dsize;

	DEBUG(10, ("ctdbd_migrate: Sending ctdb packet\n"));
	ctdb_packet_dump(&req.hdr);

	status = packet_send(
		conn->pkt, 2,
		data_blob_const(&req, offsetof(struct ctdb_req_call, data)),
		data_blob_const(key.dptr, key.dsize));

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(3, ("packet_send failed: %s\n", nt_errstr(status)));
		return status;
	}

	status = packet_flush(conn->pkt);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(3, ("write to ctdbd failed: %s\n", nt_errstr(status)));
		cluster_fatal("cluster dispatch daemon control write error\n");
	}

	status = ctdb_read_req(conn, req.hdr.reqid, NULL, (void *)&reply);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("ctdb_read_req failed: %s\n", nt_errstr(status)));
		goto fail;
	}

	if (reply->hdr.operation != CTDB_REPLY_CALL) {
		DEBUG(0, ("received invalid reply\n"));
		status = NT_STATUS_INTERNAL_ERROR;
		goto fail;
	}

	status = NT_STATUS_OK;
 fail:

	TALLOC_FREE(reply);
	return status;
}

/*
 * remotely fetch a record without locking it or forcing a migration
 */
NTSTATUS ctdbd_fetch(struct ctdbd_connection *conn, uint32 db_id,
		     TDB_DATA key, TALLOC_CTX *mem_ctx, TDB_DATA *data)
{
	struct ctdb_req_call req;
	struct ctdb_reply_call *reply;
	NTSTATUS status;

	ZERO_STRUCT(req);

	req.hdr.length = offsetof(struct ctdb_req_call, data) + key.dsize;
	req.hdr.ctdb_magic   = CTDB_MAGIC;
	req.hdr.ctdb_version = CTDB_VERSION;
	req.hdr.operation    = CTDB_REQ_CALL;
	req.hdr.reqid        = ++conn->reqid;
	req.flags            = 0;
	req.callid           = CTDB_FETCH_FUNC;
	req.db_id            = db_id;
	req.keylen           = key.dsize;

	status = packet_send(
		conn->pkt, 2,
		data_blob_const(&req, offsetof(struct ctdb_req_call, data)),
		data_blob_const(key.dptr, key.dsize));

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(3, ("packet_send failed: %s\n", nt_errstr(status)));
		return status;
	}

	status = packet_flush(conn->pkt);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(3, ("write to ctdbd failed: %s\n", nt_errstr(status)));
		cluster_fatal("cluster dispatch daemon control write error\n");
	}

	status = ctdb_read_req(conn, req.hdr.reqid, NULL, (void *)&reply);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("ctdb_read_req failed: %s\n", nt_errstr(status)));
		goto fail;
	}

	if (reply->hdr.operation != CTDB_REPLY_CALL) {
		DEBUG(0, ("received invalid reply\n"));
		status = NT_STATUS_INTERNAL_ERROR;
		goto fail;
	}

	data->dsize = reply->datalen;
	if (data->dsize == 0) {
		data->dptr = NULL;
		goto done;
	}

	data->dptr = (uint8 *)talloc_memdup(mem_ctx, &reply->data[0],
					    reply->datalen);
	if (data->dptr == NULL) {
		DEBUG(0, ("talloc failed\n"));
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

 done:
	status = NT_STATUS_OK;
 fail:
	TALLOC_FREE(reply);
	return status;
}

struct ctdbd_traverse_state {
	void (*fn)(TDB_DATA key, TDB_DATA data, void *private_data);
	void *private_data;
};

/*
 * Handle a traverse record coming in on the ctdbd connection
 */

static NTSTATUS ctdb_traverse_handler(uint8_t *buf, size_t length,
				      void *private_data)
{
	struct ctdbd_traverse_state *state =
		(struct ctdbd_traverse_state *)private_data;

	struct ctdb_req_message *m;
	struct ctdb_rec_data *d;
	TDB_DATA key, data;

	m = (struct ctdb_req_message *)buf;

	if (length < sizeof(*m) || m->hdr.length != length) {
		DEBUG(0, ("Got invalid message of length %d\n", (int)length));
		TALLOC_FREE(buf);
		return NT_STATUS_UNEXPECTED_IO_ERROR;
	}

	d = (struct ctdb_rec_data *)&m->data[0];
	if (m->datalen < sizeof(uint32_t) || m->datalen != d->length) {
		DEBUG(0, ("Got invalid traverse data of length %d\n",
			  (int)m->datalen));
		TALLOC_FREE(buf);
		return NT_STATUS_UNEXPECTED_IO_ERROR;
	}

	key.dsize = d->keylen;
	key.dptr  = &d->data[0];
	data.dsize = d->datalen;
	data.dptr = &d->data[d->keylen];		

	if (key.dsize == 0 && data.dsize == 0) {
		/* end of traverse */
		return NT_STATUS_END_OF_FILE;
	}

	if (data.dsize < sizeof(struct ctdb_ltdb_header)) {
		DEBUG(0, ("Got invalid ltdb header length %d\n",
			  (int)data.dsize));
		TALLOC_FREE(buf);
		return NT_STATUS_UNEXPECTED_IO_ERROR;
	}
	data.dsize -= sizeof(struct ctdb_ltdb_header);
	data.dptr += sizeof(struct ctdb_ltdb_header);

	if (state->fn) {
		state->fn(key, data, state->private_data);
	}

	TALLOC_FREE(buf);
	return NT_STATUS_OK;
}

/*
  Traverse a ctdb database. This uses a kind-of hackish way to open a second
  connection to ctdbd to avoid the hairy recursive and async problems with
  everything in-line.
*/

NTSTATUS ctdbd_traverse(uint32 db_id,
			void (*fn)(TDB_DATA key, TDB_DATA data,
				   void *private_data),
			void *private_data)
{
	struct ctdbd_connection *conn;
	NTSTATUS status;

	TDB_DATA data;
	struct ctdb_traverse_start t;
	int cstatus;
	struct ctdbd_traverse_state state;

	become_root();
	status = ctdbd_init_connection(NULL, &conn);
	unbecome_root();
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("ctdbd_init_connection failed: %s\n",
			  nt_errstr(status)));
		return status;
	}

	t.db_id = db_id;
	t.srvid = conn->rand_srvid;
	t.reqid = ++conn->reqid;

	data.dptr = (uint8_t *)&t;
	data.dsize = sizeof(t);

	status = ctdbd_control(conn, CTDB_CURRENT_NODE,
			       CTDB_CONTROL_TRAVERSE_START, conn->rand_srvid, 0,
			       data, NULL, NULL, &cstatus);

	if (!NT_STATUS_IS_OK(status) || (cstatus != 0)) {

		DEBUG(0,("ctdbd_control failed: %s, %d\n", nt_errstr(status),
			 cstatus));

		if (NT_STATUS_IS_OK(status)) {
			/*
			 * We need a mapping here
			 */
			status = NT_STATUS_UNSUCCESSFUL;
		}
		goto done;
	}

	state.fn = fn;
	state.private_data = private_data;

	while (True) {

		status = NT_STATUS_OK;

		if (packet_handler(conn->pkt, ctdb_req_complete,
				   ctdb_traverse_handler, &state, &status)) {

			if (NT_STATUS_EQUAL(status, NT_STATUS_END_OF_FILE)) {
				status = NT_STATUS_OK;
				break;
			}

			/*
			 * There might be more in the queue
			 */
			continue;
		}

		if (!NT_STATUS_IS_OK(status)) {
			break;
		}

		status = ctdb_packet_fd_read_sync(conn->pkt);

		if (NT_STATUS_EQUAL(status, NT_STATUS_RETRY)) {
			/*
			 * There might be more in the queue
			 */
			continue;
		}

		if (NT_STATUS_EQUAL(status, NT_STATUS_END_OF_FILE)) {
			status = NT_STATUS_OK;
			break;
		}

		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0, ("packet_fd_read_sync failed: %s\n", nt_errstr(status)));
			cluster_fatal("ctdbd died\n");
		}
	}

 done:
	TALLOC_FREE(conn);
	return status;
}

/*
   This is used to canonicalize a ctdb_sock_addr structure.
*/
static void smbd_ctdb_canonicalize_ip(const struct sockaddr_storage *in,
				      struct sockaddr_storage *out)
{
	memcpy(out, in, sizeof (*out));

#ifdef HAVE_IPV6
	if (in->ss_family == AF_INET6) {
		const char prefix[12] = { 0,0,0,0,0,0,0,0,0,0,0xff,0xff };
		const struct sockaddr_in6 *in6 = (struct sockaddr_in6 *)in;
		struct sockaddr_in *out4 = (struct sockaddr_in *)out;
		if (memcmp(&in6->sin6_addr, prefix, 12) == 0) {
			memset(out, 0, sizeof(*out));
#ifdef HAVE_SOCK_SIN_LEN
			out4->sin_len = sizeof(*out);
#endif
			out4->sin_family = AF_INET;
			out4->sin_port   = in6->sin6_port;
			memcpy(&out4->sin_addr, &in6->sin6_addr.s6_addr32[3], 4);
		}
	}
#endif
}

/*
 * Register us as a server for a particular tcp connection
 */

NTSTATUS ctdbd_register_ips(struct ctdbd_connection *conn,
			    const struct sockaddr_storage *_server,
			    const struct sockaddr_storage *_client,
			    void (*release_ip_handler)(const char *ip_addr,
						       void *private_data),
			    void *private_data)
{
	/*
	 * we still use ctdb_control_tcp for ipv4
	 * because we want to work against older ctdb
	 * versions at runtime
	 */
	struct ctdb_control_tcp p4;
#ifdef HAVE_STRUCT_CTDB_CONTROL_TCP_ADDR
	struct ctdb_control_tcp_addr p;
#endif
	TDB_DATA data;
	NTSTATUS status;
	struct sockaddr_storage client;
	struct sockaddr_storage server;

	/*
	 * Only one connection so far
	 */
	SMB_ASSERT(conn->release_ip_handler == NULL);

	smbd_ctdb_canonicalize_ip(_client, &client);
	smbd_ctdb_canonicalize_ip(_server, &server);

	switch (client.ss_family) {
	case AF_INET:
		p4.dest = *(struct sockaddr_in *)(void *)&server;
		p4.src = *(struct sockaddr_in *)(void *)&client;
		data.dptr = (uint8_t *)&p4;
		data.dsize = sizeof(p4);
		break;
#ifdef HAVE_STRUCT_CTDB_CONTROL_TCP_ADDR
	case AF_INET6:
		p.dest.ip6 = *(struct sockaddr_in6 *)(void *)&server;
		p.src.ip6 = *(struct sockaddr_in6 *)(void *)&client;
		data.dptr = (uint8_t *)&p;
		data.dsize = sizeof(p);
		break;
#endif
	default:
		return NT_STATUS_INTERNAL_ERROR;
	}

	conn->release_ip_handler = release_ip_handler;
	/*
	 * store the IP address of the server socket for later
	 * comparison in release_ip()
	 */
	conn->release_ip_priv = private_data;

	/*
	 * We want to be told about IP releases
	 */

	status = register_with_ctdbd(conn, CTDB_SRVID_RELEASE_IP);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	/*
	 * inform ctdb of our tcp connection, so if IP takeover happens ctdb
	 * can send an extra ack to trigger a reset for our client, so it
	 * immediately reconnects
	 */
	return ctdbd_control(conn, CTDB_CURRENT_NODE, 
			     CTDB_CONTROL_TCP_CLIENT, 0,
			     CTDB_CTRL_FLAG_NOREPLY, data, NULL, NULL, NULL);
}

/*
 * We want to handle reconfigure events
 */
NTSTATUS ctdbd_register_reconfigure(struct ctdbd_connection *conn)
{
	return register_with_ctdbd(conn, CTDB_SRVID_RECONFIGURE);
}

/*
  call a control on the local node
 */
NTSTATUS ctdbd_control_local(struct ctdbd_connection *conn, uint32 opcode, 
			     uint64_t srvid, uint32_t flags, TDB_DATA data, 
			     TALLOC_CTX *mem_ctx, TDB_DATA *outdata,
			     int *cstatus)
{
	return ctdbd_control(conn, CTDB_CURRENT_NODE, opcode, srvid, flags, data, mem_ctx, outdata, cstatus);
}

NTSTATUS ctdb_watch_us(struct ctdbd_connection *conn)
{
	struct ctdb_client_notify_register reg_data;
	size_t struct_len;
	NTSTATUS status;
	int cstatus;

	reg_data.srvid = CTDB_SRVID_SAMBA_NOTIFY;
	reg_data.len = 1;
	reg_data.notify_data[0] = 0;

	struct_len = offsetof(struct ctdb_client_notify_register,
			      notify_data) + reg_data.len;

	status = ctdbd_control_local(
		conn, CTDB_CONTROL_REGISTER_NOTIFY, conn->rand_srvid, 0,
		make_tdb_data((uint8_t *)&reg_data, struct_len),
		NULL, NULL, &cstatus);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("ctdbd_control_local failed: %s\n",
			  nt_errstr(status)));
	}
	return status;
}

NTSTATUS ctdb_unwatch(struct ctdbd_connection *conn)
{
	struct ctdb_client_notify_deregister dereg_data;
	NTSTATUS status;
	int cstatus;

	dereg_data.srvid = CTDB_SRVID_SAMBA_NOTIFY;

	status = ctdbd_control_local(
		conn, CTDB_CONTROL_DEREGISTER_NOTIFY, conn->rand_srvid, 0,
		make_tdb_data((uint8_t *)&dereg_data, sizeof(dereg_data)),
		NULL, NULL, &cstatus);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("ctdbd_control_local failed: %s\n",
			  nt_errstr(status)));
	}
	return status;
}

#endif
