/* 
   Unix SMB/CIFS implementation.

   low level WINS replication client code

   Copyright (C) Andrew Tridgell 2005
   Copyright (C) Stefan Metzmacher 2005-2010
   
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
#include "../lib/util/dlinklist.h"
#include "libcli/wrepl/winsrepl.h"
#include "librpc/gen_ndr/ndr_winsrepl.h"
#include "lib/stream/packet.h"
#include "system/network.h"
#include "lib/socket/netif.h"
#include "param/param.h"
#include "lib/util/tevent_ntstatus.h"
#include "lib/tsocket/tsocket.h"
#include "libcli/util/tstream.h"

/*
  main context structure for the wins replication client library
*/
struct wrepl_socket {
	struct {
		struct tevent_context *ctx;
	} event;

	/* the default timeout for requests, 0 means no timeout */
#define WREPL_SOCKET_REQUEST_TIMEOUT	(60)
	uint32_t request_timeout;

	struct tevent_queue *request_queue;

	struct tstream_context *stream;
};

bool wrepl_socket_is_connected(struct wrepl_socket *wrepl_sock)
{
	if (!wrepl_sock) {
		return false;
	}

	if (!wrepl_sock->stream) {
		return false;
	}

	return true;
}

/*
  initialise a wrepl_socket. The event_ctx is optional, if provided then
  operations will use that event context
*/
struct wrepl_socket *wrepl_socket_init(TALLOC_CTX *mem_ctx,
				       struct tevent_context *event_ctx)
{
	struct wrepl_socket *wrepl_socket;

	wrepl_socket = talloc_zero(mem_ctx, struct wrepl_socket);
	if (!wrepl_socket) {
		return NULL;
	}

	wrepl_socket->event.ctx = event_ctx;
	if (!wrepl_socket->event.ctx) {
		goto failed;
	}

	wrepl_socket->request_queue = tevent_queue_create(wrepl_socket,
							  "wrepl request queue");
	if (wrepl_socket->request_queue == NULL) {
		goto failed;
	}

	wrepl_socket->request_timeout	= WREPL_SOCKET_REQUEST_TIMEOUT;

	return wrepl_socket;

failed:
	talloc_free(wrepl_socket);
	return NULL;
}

/*
  initialise a wrepl_socket from an already existing connection
*/
NTSTATUS wrepl_socket_donate_stream(struct wrepl_socket *wrepl_socket,
				    struct tstream_context **stream)
{
	if (wrepl_socket->stream) {
		return NT_STATUS_CONNECTION_ACTIVE;
	}

	wrepl_socket->stream = talloc_move(wrepl_socket, stream);
	return NT_STATUS_OK;
}

/*
  initialise a wrepl_socket from an already existing connection
*/
NTSTATUS wrepl_socket_split_stream(struct wrepl_socket *wrepl_socket,
				   TALLOC_CTX *mem_ctx,
				   struct tstream_context **stream)
{
	size_t num_requests;

	if (!wrepl_socket->stream) {
		return NT_STATUS_CONNECTION_INVALID;
	}

	num_requests = tevent_queue_length(wrepl_socket->request_queue);
	if (num_requests > 0) {
		return NT_STATUS_CONNECTION_IN_USE;
	}

	*stream = talloc_move(wrepl_socket, &wrepl_socket->stream);
	return NT_STATUS_OK;
}

const char *wrepl_best_ip(struct loadparm_context *lp_ctx, const char *peer_ip)
{
	struct interface *ifaces;
	load_interfaces(lp_ctx, lpcfg_interfaces(lp_ctx), &ifaces);
	return iface_best_ip(ifaces, peer_ip);
}

struct wrepl_connect_state {
	struct {
		struct wrepl_socket *wrepl_socket;
		struct tevent_context *ev;
	} caller;
	struct tsocket_address *local_address;
	struct tsocket_address *remote_address;
	struct tstream_context *stream;
};

static void wrepl_connect_trigger(struct tevent_req *req,
				  void *private_date);

struct tevent_req *wrepl_connect_send(TALLOC_CTX *mem_ctx,
				      struct tevent_context *ev,
				      struct wrepl_socket *wrepl_socket,
				      const char *our_ip, const char *peer_ip)
{
	struct tevent_req *req;
	struct wrepl_connect_state *state;
	int ret;
	bool ok;

	req = tevent_req_create(mem_ctx, &state,
				struct wrepl_connect_state);
	if (req == NULL) {
		return NULL;
	}

	state->caller.wrepl_socket = wrepl_socket;
	state->caller.ev = ev;

	if (wrepl_socket->stream) {
		tevent_req_nterror(req, NT_STATUS_CONNECTION_ACTIVE);
		return tevent_req_post(req, ev);
	}

	ret = tsocket_address_inet_from_strings(state, "ipv4",
						our_ip, 0,
						&state->local_address);
	if (ret != 0) {
		NTSTATUS status = map_nt_error_from_unix(errno);
		tevent_req_nterror(req, status);
		return tevent_req_post(req, ev);
	}

	ret = tsocket_address_inet_from_strings(state, "ipv4",
						peer_ip, WINS_REPLICATION_PORT,
						&state->remote_address);
	if (ret != 0) {
		NTSTATUS status = map_nt_error_from_unix(errno);
		tevent_req_nterror(req, status);
		return tevent_req_post(req, ev);
	}

	ok = tevent_queue_add(wrepl_socket->request_queue,
			      ev,
			      req,
			      wrepl_connect_trigger,
			      NULL);
	if (!ok) {
		tevent_req_nomem(NULL, req);
		return tevent_req_post(req, ev);
	}

	if (wrepl_socket->request_timeout > 0) {
		struct timeval endtime;
		endtime = tevent_timeval_current_ofs(wrepl_socket->request_timeout, 0);
		ok = tevent_req_set_endtime(req, ev, endtime);
		if (!ok) {
			return tevent_req_post(req, ev);
		}
	}

	return req;
}

static void wrepl_connect_done(struct tevent_req *subreq);

static void wrepl_connect_trigger(struct tevent_req *req,
				  void *private_date)
{
	struct wrepl_connect_state *state = tevent_req_data(req,
					    struct wrepl_connect_state);
	struct tevent_req *subreq;

	subreq = tstream_inet_tcp_connect_send(state,
					       state->caller.ev,
					       state->local_address,
					       state->remote_address);
	if (tevent_req_nomem(subreq, req)) {
		return;
	}
	tevent_req_set_callback(subreq, wrepl_connect_done, req);

	return;
}

static void wrepl_connect_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(subreq,
				 struct tevent_req);
	struct wrepl_connect_state *state = tevent_req_data(req,
					    struct wrepl_connect_state);
	int ret;
	int sys_errno;

	ret = tstream_inet_tcp_connect_recv(subreq, &sys_errno,
					    state, &state->stream, NULL);
	if (ret != 0) {
		NTSTATUS status = map_nt_error_from_unix(sys_errno);
		tevent_req_nterror(req, status);
		return;
	}

	tevent_req_done(req);
}

/*
  connect a wrepl_socket to a WINS server - recv side
*/
NTSTATUS wrepl_connect_recv(struct tevent_req *req)
{
	struct wrepl_connect_state *state = tevent_req_data(req,
					    struct wrepl_connect_state);
	struct wrepl_socket *wrepl_socket = state->caller.wrepl_socket;
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		tevent_req_received(req);
		return status;
	}

	wrepl_socket->stream = talloc_move(wrepl_socket, &state->stream);

	tevent_req_received(req);
	return NT_STATUS_OK;
}

/*
  connect a wrepl_socket to a WINS server - sync API
*/
NTSTATUS wrepl_connect(struct wrepl_socket *wrepl_socket,
		       const char *our_ip, const char *peer_ip)
{
	struct tevent_req *subreq;
	bool ok;
	NTSTATUS status;

	subreq = wrepl_connect_send(wrepl_socket, wrepl_socket->event.ctx,
				    wrepl_socket, our_ip, peer_ip);
	NT_STATUS_HAVE_NO_MEMORY(subreq);

	ok = tevent_req_poll(subreq, wrepl_socket->event.ctx);
	if (!ok) {
		TALLOC_FREE(subreq);
		return NT_STATUS_INTERNAL_ERROR;
	}

	status = wrepl_connect_recv(subreq);
	TALLOC_FREE(subreq);
	NT_STATUS_NOT_OK_RETURN(status);

	return NT_STATUS_OK;
}

struct wrepl_request_state {
	struct {
		struct wrepl_socket *wrepl_socket;
		struct tevent_context *ev;
	} caller;
	struct wrepl_send_ctrl ctrl;
	struct {
		struct wrepl_wrap wrap;
		DATA_BLOB blob;
		struct iovec iov;
	} req;
	bool one_way;
	struct {
		DATA_BLOB blob;
		struct wrepl_packet *packet;
	} rep;
};

static void wrepl_request_trigger(struct tevent_req *req,
				  void *private_data);

struct tevent_req *wrepl_request_send(TALLOC_CTX *mem_ctx,
				      struct tevent_context *ev,
				      struct wrepl_socket *wrepl_socket,
				      const struct wrepl_packet *packet,
				      const struct wrepl_send_ctrl *ctrl)
{
	struct tevent_req *req;
	struct wrepl_request_state *state;
	NTSTATUS status;
	enum ndr_err_code ndr_err;
	bool ok;

	if (wrepl_socket->event.ctx != ev) {
		/* TODO: remove wrepl_socket->event.ctx !!! */
		smb_panic("wrepl_associate_stop_send event context mismatch!");
		return NULL;
	}

	req = tevent_req_create(mem_ctx, &state,
				struct wrepl_request_state);
	if (req == NULL) {
		return NULL;
	}

	state->caller.wrepl_socket = wrepl_socket;
	state->caller.ev = ev;

	if (ctrl) {
		state->ctrl = *ctrl;
	}

	if (wrepl_socket->stream == NULL) {
		tevent_req_nterror(req, NT_STATUS_INVALID_CONNECTION);
		return tevent_req_post(req, ev);
	}

	state->req.wrap.packet = *packet;
	ndr_err = ndr_push_struct_blob(&state->req.blob, state,
				       &state->req.wrap,
				       (ndr_push_flags_fn_t)ndr_push_wrepl_wrap);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		status = ndr_map_error2ntstatus(ndr_err);
		tevent_req_nterror(req, status);
		return tevent_req_post(req, ev);
	}

	state->req.iov.iov_base = (char *) state->req.blob.data;
	state->req.iov.iov_len = state->req.blob.length;

	ok = tevent_queue_add(wrepl_socket->request_queue,
			      ev,
			      req,
			      wrepl_request_trigger,
			      NULL);
	if (!ok) {
		tevent_req_nomem(NULL, req);
		return tevent_req_post(req, ev);
	}

	if (wrepl_socket->request_timeout > 0) {
		struct timeval endtime;
		endtime = tevent_timeval_current_ofs(wrepl_socket->request_timeout, 0);
		ok = tevent_req_set_endtime(req, ev, endtime);
		if (!ok) {
			return tevent_req_post(req, ev);
		}
	}

	return req;
}

static void wrepl_request_writev_done(struct tevent_req *subreq);

static void wrepl_request_trigger(struct tevent_req *req,
				  void *private_data)
{
	struct wrepl_request_state *state = tevent_req_data(req,
					    struct wrepl_request_state);
	struct tevent_req *subreq;

	if (state->caller.wrepl_socket->stream == NULL) {
		tevent_req_nterror(req, NT_STATUS_INVALID_CONNECTION);
		return;
	}

	if (DEBUGLVL(10)) {
		DEBUG(10,("Sending WINS packet of length %u\n",
			  (unsigned)state->req.blob.length));
		NDR_PRINT_DEBUG(wrepl_packet, &state->req.wrap.packet);
	}

	subreq = tstream_writev_send(state,
				     state->caller.ev,
				     state->caller.wrepl_socket->stream,
				     &state->req.iov, 1);
	if (tevent_req_nomem(subreq, req)) {
		return;
	}
	tevent_req_set_callback(subreq, wrepl_request_writev_done, req);
}

static void wrepl_request_disconnect_done(struct tevent_req *subreq);
static void wrepl_request_read_pdu_done(struct tevent_req *subreq);

static void wrepl_request_writev_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(subreq,
				 struct tevent_req);
	struct wrepl_request_state *state = tevent_req_data(req,
					    struct wrepl_request_state);
	int ret;
	int sys_errno;

	ret = tstream_writev_recv(subreq, &sys_errno);
	TALLOC_FREE(subreq);
	if (ret == -1) {
		NTSTATUS status = map_nt_error_from_unix(sys_errno);
		TALLOC_FREE(state->caller.wrepl_socket->stream);
		tevent_req_nterror(req, status);
		return;
	}

	if (state->caller.wrepl_socket->stream == NULL) {
		tevent_req_nterror(req, NT_STATUS_INVALID_CONNECTION);
		return;
	}

	if (state->ctrl.disconnect_after_send) {
		subreq = tstream_disconnect_send(state,
						 state->caller.ev,
						 state->caller.wrepl_socket->stream);
		if (tevent_req_nomem(subreq, req)) {
			return;
		}
		tevent_req_set_callback(subreq, wrepl_request_disconnect_done, req);
		return;
	}

	if (state->ctrl.send_only) {
		tevent_req_done(req);
		return;
	}

	subreq = tstream_read_pdu_blob_send(state,
					    state->caller.ev,
					    state->caller.wrepl_socket->stream,
					    4, /* initial_read_size */
					    packet_full_request_u32,
					    NULL);
	if (tevent_req_nomem(subreq, req)) {
		return;
	}
	tevent_req_set_callback(subreq, wrepl_request_read_pdu_done, req);
}

static void wrepl_request_disconnect_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(subreq,
				 struct tevent_req);
	struct wrepl_request_state *state = tevent_req_data(req,
					    struct wrepl_request_state);
	int ret;
	int sys_errno;

	ret = tstream_disconnect_recv(subreq, &sys_errno);
	TALLOC_FREE(subreq);
	if (ret == -1) {
		NTSTATUS status = map_nt_error_from_unix(sys_errno);
		TALLOC_FREE(state->caller.wrepl_socket->stream);
		tevent_req_nterror(req, status);
		return;
	}

	DEBUG(10,("WINS connection disconnected\n"));
	TALLOC_FREE(state->caller.wrepl_socket->stream);

	tevent_req_done(req);
}

static void wrepl_request_read_pdu_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(subreq,
				 struct tevent_req);
	struct wrepl_request_state *state = tevent_req_data(req,
					    struct wrepl_request_state);
	NTSTATUS status;
	DATA_BLOB blob;
	enum ndr_err_code ndr_err;

	status = tstream_read_pdu_blob_recv(subreq, state, &state->rep.blob);
	if (!NT_STATUS_IS_OK(status)) {
		TALLOC_FREE(state->caller.wrepl_socket->stream);
		tevent_req_nterror(req, status);
		return;
	}

	state->rep.packet = talloc(state, struct wrepl_packet);
	if (tevent_req_nomem(state->rep.packet, req)) {
		return;
	}

	blob.data = state->rep.blob.data + 4;
	blob.length = state->rep.blob.length - 4;

	/* we have a full request - parse it */
	ndr_err = ndr_pull_struct_blob(&blob,
				       state->rep.packet,
				       state->rep.packet,
				       (ndr_pull_flags_fn_t)ndr_pull_wrepl_packet);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		status = ndr_map_error2ntstatus(ndr_err);
		tevent_req_nterror(req, status);
		return;
	}

	if (DEBUGLVL(10)) {
		DEBUG(10,("Received WINS packet of length %u\n",
			  (unsigned)state->rep.blob.length));
		NDR_PRINT_DEBUG(wrepl_packet, state->rep.packet);
	}

	tevent_req_done(req);
}

NTSTATUS wrepl_request_recv(struct tevent_req *req,
			    TALLOC_CTX *mem_ctx,
			    struct wrepl_packet **packet)
{
	struct wrepl_request_state *state = tevent_req_data(req,
					    struct wrepl_request_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		TALLOC_FREE(state->caller.wrepl_socket->stream);
		tevent_req_received(req);
		return status;
	}

	if (packet) {
		*packet = talloc_move(mem_ctx, &state->rep.packet);
	}

	tevent_req_received(req);
	return NT_STATUS_OK;
}

/*
  a full WINS replication request/response
*/
NTSTATUS wrepl_request(struct wrepl_socket *wrepl_socket,
		       TALLOC_CTX *mem_ctx,
		       const struct wrepl_packet *req_packet,
		       struct wrepl_packet **reply_packet)
{
	struct tevent_req *subreq;
	bool ok;
	NTSTATUS status;

	subreq = wrepl_request_send(mem_ctx, wrepl_socket->event.ctx,
				    wrepl_socket, req_packet, NULL);
	NT_STATUS_HAVE_NO_MEMORY(subreq);

	ok = tevent_req_poll(subreq, wrepl_socket->event.ctx);
	if (!ok) {
		TALLOC_FREE(subreq);
		return NT_STATUS_INTERNAL_ERROR;
	}

	status = wrepl_request_recv(subreq, mem_ctx, reply_packet);
	TALLOC_FREE(subreq);
	NT_STATUS_NOT_OK_RETURN(status);

	return NT_STATUS_OK;
}


struct wrepl_associate_state {
	struct wrepl_packet packet;
	uint32_t assoc_ctx;
	uint16_t major_version;
};

static void wrepl_associate_done(struct tevent_req *subreq);

struct tevent_req *wrepl_associate_send(TALLOC_CTX *mem_ctx,
					struct tevent_context *ev,
					struct wrepl_socket *wrepl_socket,
					const struct wrepl_associate *io)
{
	struct tevent_req *req;
	struct wrepl_associate_state *state;
	struct tevent_req *subreq;

	if (wrepl_socket->event.ctx != ev) {
		/* TODO: remove wrepl_socket->event.ctx !!! */
		smb_panic("wrepl_associate_send event context mismatch!");
		return NULL;
	}

	req = tevent_req_create(mem_ctx, &state,
				struct wrepl_associate_state);
	if (req == NULL) {
		return NULL;
	};

	state->packet.opcode				= WREPL_OPCODE_BITS;
	state->packet.mess_type				= WREPL_START_ASSOCIATION;
	state->packet.message.start.minor_version	= 2;
	state->packet.message.start.major_version	= 5;

	/*
	 * nt4 uses 41 bytes for the start_association call
	 * so do it the same and as we don't know th emeanings of this bytes
	 * we just send zeros and nt4, w2k and w2k3 seems to be happy with this
	 *
	 * if we don't do this nt4 uses an old version of the wins replication protocol
	 * and that would break nt4 <-> samba replication
	 */
	state->packet.padding	= data_blob_talloc(state, NULL, 21);
	if (tevent_req_nomem(state->packet.padding.data, req)) {
		return tevent_req_post(req, ev);
	}
	memset(state->packet.padding.data, 0, state->packet.padding.length);

	subreq = wrepl_request_send(state, ev, wrepl_socket, &state->packet, NULL);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, wrepl_associate_done, req);

	return req;
}

static void wrepl_associate_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(subreq,
				 struct tevent_req);
	struct wrepl_associate_state *state = tevent_req_data(req,
					      struct wrepl_associate_state);
	NTSTATUS status;
	struct wrepl_packet *packet;

	status = wrepl_request_recv(subreq, state, &packet);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}

	if (packet->mess_type != WREPL_START_ASSOCIATION_REPLY) {
		tevent_req_nterror(req, NT_STATUS_INVALID_NETWORK_RESPONSE);
		return;
	}

	state->assoc_ctx = packet->message.start_reply.assoc_ctx;
	state->major_version = packet->message.start_reply.major_version;

	tevent_req_done(req);
}

/*
  setup an association - recv
*/
NTSTATUS wrepl_associate_recv(struct tevent_req *req,
			      struct wrepl_associate *io)
{
	struct wrepl_associate_state *state = tevent_req_data(req,
					      struct wrepl_associate_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		tevent_req_received(req);
		return status;
	}

	io->out.assoc_ctx = state->assoc_ctx;
	io->out.major_version = state->major_version;

	tevent_req_received(req);
	return NT_STATUS_OK;
}

/*
  setup an association - sync api
*/
NTSTATUS wrepl_associate(struct wrepl_socket *wrepl_socket,
			 struct wrepl_associate *io)
{
	struct tevent_req *subreq;
	bool ok;
	NTSTATUS status;

	subreq = wrepl_associate_send(wrepl_socket, wrepl_socket->event.ctx,
				      wrepl_socket, io);
	NT_STATUS_HAVE_NO_MEMORY(subreq);

	ok = tevent_req_poll(subreq, wrepl_socket->event.ctx);
	if (!ok) {
		TALLOC_FREE(subreq);
		return NT_STATUS_INTERNAL_ERROR;
	}

	status = wrepl_associate_recv(subreq, io);
	TALLOC_FREE(subreq);
	NT_STATUS_NOT_OK_RETURN(status);

	return NT_STATUS_OK;
}

struct wrepl_associate_stop_state {
	struct wrepl_packet packet;
	struct wrepl_send_ctrl ctrl;
};

static void wrepl_associate_stop_done(struct tevent_req *subreq);

struct tevent_req *wrepl_associate_stop_send(TALLOC_CTX *mem_ctx,
					     struct tevent_context *ev,
					     struct wrepl_socket *wrepl_socket,
					     const struct wrepl_associate_stop *io)
{
	struct tevent_req *req;
	struct wrepl_associate_stop_state *state;
	struct tevent_req *subreq;

	if (wrepl_socket->event.ctx != ev) {
		/* TODO: remove wrepl_socket->event.ctx !!! */
		smb_panic("wrepl_associate_stop_send event context mismatch!");
		return NULL;
	}

	req = tevent_req_create(mem_ctx, &state,
				struct wrepl_associate_stop_state);
	if (req == NULL) {
		return NULL;
	};

	state->packet.opcode			= WREPL_OPCODE_BITS;
	state->packet.assoc_ctx			= io->in.assoc_ctx;
	state->packet.mess_type			= WREPL_STOP_ASSOCIATION;
	state->packet.message.stop.reason	= io->in.reason;

	if (io->in.reason == 0) {
		state->ctrl.send_only			= true;
		state->ctrl.disconnect_after_send	= true;
	}

	subreq = wrepl_request_send(state, ev, wrepl_socket, &state->packet, &state->ctrl);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, wrepl_associate_stop_done, req);

	return req;
}

static void wrepl_associate_stop_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(subreq,
				 struct tevent_req);
	struct wrepl_associate_stop_state *state = tevent_req_data(req,
						   struct wrepl_associate_stop_state);
	NTSTATUS status;

	/* currently we don't care about a possible response */
	status = wrepl_request_recv(subreq, state, NULL);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}

	tevent_req_done(req);
}

/*
  stop an association - recv
*/
NTSTATUS wrepl_associate_stop_recv(struct tevent_req *req,
				   struct wrepl_associate_stop *io)
{
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		tevent_req_received(req);
		return status;
	}

	tevent_req_received(req);
	return NT_STATUS_OK;
}

/*
  setup an association - sync api
*/
NTSTATUS wrepl_associate_stop(struct wrepl_socket *wrepl_socket,
			      struct wrepl_associate_stop *io)
{
	struct tevent_req *subreq;
	bool ok;
	NTSTATUS status;

	subreq = wrepl_associate_stop_send(wrepl_socket, wrepl_socket->event.ctx,
					   wrepl_socket, io);
	NT_STATUS_HAVE_NO_MEMORY(subreq);

	ok = tevent_req_poll(subreq, wrepl_socket->event.ctx);
	if (!ok) {
		TALLOC_FREE(subreq);
		return NT_STATUS_INTERNAL_ERROR;
	}

	status = wrepl_associate_stop_recv(subreq, io);
	TALLOC_FREE(subreq);
	NT_STATUS_NOT_OK_RETURN(status);

	return NT_STATUS_OK;
}

struct wrepl_pull_table_state {
	struct wrepl_packet packet;
	uint32_t num_partners;
	struct wrepl_wins_owner *partners;
};

static void wrepl_pull_table_done(struct tevent_req *subreq);

struct tevent_req *wrepl_pull_table_send(TALLOC_CTX *mem_ctx,
					 struct tevent_context *ev,
					 struct wrepl_socket *wrepl_socket,
					 const struct wrepl_pull_table *io)
{
	struct tevent_req *req;
	struct wrepl_pull_table_state *state;
	struct tevent_req *subreq;

	if (wrepl_socket->event.ctx != ev) {
		/* TODO: remove wrepl_socket->event.ctx !!! */
		smb_panic("wrepl_pull_table_send event context mismatch!");
		return NULL;
	}

	req = tevent_req_create(mem_ctx, &state,
				struct wrepl_pull_table_state);
	if (req == NULL) {
		return NULL;
	};

	state->packet.opcode				= WREPL_OPCODE_BITS;
	state->packet.assoc_ctx				= io->in.assoc_ctx;
	state->packet.mess_type				= WREPL_REPLICATION;
	state->packet.message.replication.command	= WREPL_REPL_TABLE_QUERY;

	subreq = wrepl_request_send(state, ev, wrepl_socket, &state->packet, NULL);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, wrepl_pull_table_done, req);

	return req;
}

static void wrepl_pull_table_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(subreq,
				 struct tevent_req);
	struct wrepl_pull_table_state *state = tevent_req_data(req,
					       struct wrepl_pull_table_state);
	NTSTATUS status;
	struct wrepl_packet *packet;
	struct wrepl_table *table;

	status = wrepl_request_recv(subreq, state, &packet);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}

	if (packet->mess_type != WREPL_REPLICATION) {
		tevent_req_nterror(req, NT_STATUS_NETWORK_ACCESS_DENIED);
		return;
	}

	if (packet->message.replication.command != WREPL_REPL_TABLE_REPLY) {
		tevent_req_nterror(req, NT_STATUS_INVALID_NETWORK_RESPONSE);
		return;
	}

	table = &packet->message.replication.info.table;

	state->num_partners = table->partner_count;
	state->partners = talloc_move(state, &table->partners);

	tevent_req_done(req);
}

/*
  fetch the partner tables - recv
*/
NTSTATUS wrepl_pull_table_recv(struct tevent_req *req,
			       TALLOC_CTX *mem_ctx,
			       struct wrepl_pull_table *io)
{
	struct wrepl_pull_table_state *state = tevent_req_data(req,
					       struct wrepl_pull_table_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		tevent_req_received(req);
		return status;
	}

	io->out.num_partners = state->num_partners;
	io->out.partners = talloc_move(mem_ctx, &state->partners);

	tevent_req_received(req);
	return NT_STATUS_OK;
}

/*
  fetch the partner table - sync api
*/
NTSTATUS wrepl_pull_table(struct wrepl_socket *wrepl_socket,
			  TALLOC_CTX *mem_ctx,
			  struct wrepl_pull_table *io)
{
	struct tevent_req *subreq;
	bool ok;
	NTSTATUS status;

	subreq = wrepl_pull_table_send(mem_ctx, wrepl_socket->event.ctx,
				       wrepl_socket, io);
	NT_STATUS_HAVE_NO_MEMORY(subreq);

	ok = tevent_req_poll(subreq, wrepl_socket->event.ctx);
	if (!ok) {
		TALLOC_FREE(subreq);
		return NT_STATUS_INTERNAL_ERROR;
	}

	status = wrepl_pull_table_recv(subreq, mem_ctx, io);
	TALLOC_FREE(subreq);
	NT_STATUS_NOT_OK_RETURN(status);

	return NT_STATUS_OK;
}


struct wrepl_pull_names_state {
	struct {
		const struct wrepl_pull_names *io;
	} caller;
	struct wrepl_packet packet;
	uint32_t num_names;
	struct wrepl_name *names;
};

static void wrepl_pull_names_done(struct tevent_req *subreq);

struct tevent_req *wrepl_pull_names_send(TALLOC_CTX *mem_ctx,
					 struct tevent_context *ev,
					 struct wrepl_socket *wrepl_socket,
					 const struct wrepl_pull_names *io)
{
	struct tevent_req *req;
	struct wrepl_pull_names_state *state;
	struct tevent_req *subreq;

	if (wrepl_socket->event.ctx != ev) {
		/* TODO: remove wrepl_socket->event.ctx !!! */
		smb_panic("wrepl_pull_names_send event context mismatch!");
		return NULL;
	}

	req = tevent_req_create(mem_ctx, &state,
				struct wrepl_pull_names_state);
	if (req == NULL) {
		return NULL;
	};
	state->caller.io = io;

	state->packet.opcode				= WREPL_OPCODE_BITS;
	state->packet.assoc_ctx				= io->in.assoc_ctx;
	state->packet.mess_type				= WREPL_REPLICATION;
	state->packet.message.replication.command	= WREPL_REPL_SEND_REQUEST;
	state->packet.message.replication.info.owner	= io->in.partner;

	subreq = wrepl_request_send(state, ev, wrepl_socket, &state->packet, NULL);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, wrepl_pull_names_done, req);

	return req;
}

static void wrepl_pull_names_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(subreq,
				 struct tevent_req);
	struct wrepl_pull_names_state *state = tevent_req_data(req,
					       struct wrepl_pull_names_state);
	NTSTATUS status;
	struct wrepl_packet *packet;
	uint32_t i;

	status = wrepl_request_recv(subreq, state, &packet);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}

	if (packet->mess_type != WREPL_REPLICATION) {
		tevent_req_nterror(req, NT_STATUS_NETWORK_ACCESS_DENIED);
		return;
	}

	if (packet->message.replication.command != WREPL_REPL_SEND_REPLY) {
		tevent_req_nterror(req, NT_STATUS_INVALID_NETWORK_RESPONSE);
		return;
	}

	state->num_names = packet->message.replication.info.reply.num_names;

	state->names = talloc_array(state, struct wrepl_name, state->num_names);
	if (tevent_req_nomem(state->names, req)) {
		return;
	}

	/* convert the list of names and addresses to a sane format */
	for (i=0; i < state->num_names; i++) {
		struct wrepl_wins_name *wname = &packet->message.replication.info.reply.names[i];
		struct wrepl_name *name = &state->names[i];

		name->name	= *wname->name;
		talloc_steal(state->names, wname->name);
		name->type	= WREPL_NAME_TYPE(wname->flags);
		name->state	= WREPL_NAME_STATE(wname->flags);
		name->node	= WREPL_NAME_NODE(wname->flags);
		name->is_static	= WREPL_NAME_IS_STATIC(wname->flags);
		name->raw_flags	= wname->flags;
		name->version_id= wname->id;
		name->owner	= talloc_strdup(state->names,
						state->caller.io->in.partner.address);
		if (tevent_req_nomem(name->owner, req)) {
			return;
		}

		/* trying to save 1 or 2 bytes on the wire isn't a good idea */
		if (wname->flags & 2) {
			uint32_t j;

			name->num_addresses = wname->addresses.addresses.num_ips;
			name->addresses = talloc_array(state->names,
						       struct wrepl_address,
						       name->num_addresses);
			if (tevent_req_nomem(name->addresses, req)) {
				return;
			}

			for (j=0;j<name->num_addresses;j++) {
				name->addresses[j].owner =
					talloc_move(name->addresses,
						    &wname->addresses.addresses.ips[j].owner);
				name->addresses[j].address = 
					talloc_move(name->addresses,
						    &wname->addresses.addresses.ips[j].ip);
			}
		} else {
			name->num_addresses = 1;
			name->addresses = talloc_array(state->names,
						       struct wrepl_address,
						       name->num_addresses);
			if (tevent_req_nomem(name->addresses, req)) {
				return;
			}

			name->addresses[0].owner = talloc_strdup(name->addresses, name->owner);
			if (tevent_req_nomem(name->addresses[0].owner, req)) {
				return;
			}
			name->addresses[0].address = talloc_move(name->addresses,
								 &wname->addresses.ip);
		}
	}

	tevent_req_done(req);
}

/*
  fetch the names for a WINS partner - recv
*/
NTSTATUS wrepl_pull_names_recv(struct tevent_req *req,
			       TALLOC_CTX *mem_ctx,
			       struct wrepl_pull_names *io)
{
	struct wrepl_pull_names_state *state = tevent_req_data(req,
					       struct wrepl_pull_names_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		tevent_req_received(req);
		return status;
	}

	io->out.num_names = state->num_names;
	io->out.names = talloc_move(mem_ctx, &state->names);

	tevent_req_received(req);
	return NT_STATUS_OK;
}



/*
  fetch the names for a WINS partner - sync api
*/
NTSTATUS wrepl_pull_names(struct wrepl_socket *wrepl_socket,
			  TALLOC_CTX *mem_ctx,
			  struct wrepl_pull_names *io)
{
	struct tevent_req *subreq;
	bool ok;
	NTSTATUS status;

	subreq = wrepl_pull_names_send(mem_ctx, wrepl_socket->event.ctx,
				       wrepl_socket, io);
	NT_STATUS_HAVE_NO_MEMORY(subreq);

	ok = tevent_req_poll(subreq, wrepl_socket->event.ctx);
	if (!ok) {
		TALLOC_FREE(subreq);
		return NT_STATUS_INTERNAL_ERROR;
	}

	status = wrepl_pull_names_recv(subreq, mem_ctx, io);
	TALLOC_FREE(subreq);
	NT_STATUS_NOT_OK_RETURN(status);

	return NT_STATUS_OK;
}
