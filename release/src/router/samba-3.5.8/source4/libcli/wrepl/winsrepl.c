/* 
   Unix SMB/CIFS implementation.

   low level WINS replication client code

   Copyright (C) Andrew Tridgell 2005
   
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
#include "lib/socket/socket.h"
#include "libcli/wrepl/winsrepl.h"
#include "librpc/gen_ndr/ndr_winsrepl.h"
#include "lib/stream/packet.h"
#include "libcli/composite/composite.h"
#include "system/network.h"
#include "lib/socket/netif.h"
#include "param/param.h"

static struct wrepl_request *wrepl_request_finished(struct wrepl_request *req, NTSTATUS status);

/*
  mark all pending requests as dead - called when a socket error happens
*/
static void wrepl_socket_dead(struct wrepl_socket *wrepl_socket, NTSTATUS status)
{
	wrepl_socket->dead = true;

	if (wrepl_socket->packet) {
		packet_recv_disable(wrepl_socket->packet);
		packet_set_fde(wrepl_socket->packet, NULL);
		packet_set_socket(wrepl_socket->packet, NULL);
	}

	if (wrepl_socket->event.fde) {
		talloc_free(wrepl_socket->event.fde);
		wrepl_socket->event.fde = NULL;
	}

	if (wrepl_socket->sock) {
		talloc_free(wrepl_socket->sock);
		wrepl_socket->sock = NULL;
	}

	if (NT_STATUS_EQUAL(NT_STATUS_UNSUCCESSFUL, status)) {
		status = NT_STATUS_UNEXPECTED_NETWORK_ERROR;
	}
	while (wrepl_socket->recv_queue) {
		struct wrepl_request *req = wrepl_socket->recv_queue;
		DLIST_REMOVE(wrepl_socket->recv_queue, req);
		wrepl_request_finished(req, status);
	}

	talloc_set_destructor(wrepl_socket, NULL);
	if (wrepl_socket->free_skipped) {
		talloc_free(wrepl_socket);
	}
}

static void wrepl_request_timeout_handler(struct tevent_context *ev, struct tevent_timer *te,
					  struct timeval t, void *ptr)
{
	struct wrepl_request *req = talloc_get_type(ptr, struct wrepl_request);
	wrepl_socket_dead(req->wrepl_socket, NT_STATUS_IO_TIMEOUT);
}

/*
  handle recv events 
*/
static NTSTATUS wrepl_finish_recv(void *private_data, DATA_BLOB packet_blob_in)
{
	struct wrepl_socket *wrepl_socket = talloc_get_type(private_data, struct wrepl_socket);
	struct wrepl_request *req = wrepl_socket->recv_queue;
	DATA_BLOB blob;
	enum ndr_err_code ndr_err;

	if (!req) {
		DEBUG(1,("Received unexpected WINS packet of length %u!\n", 
			 (unsigned)packet_blob_in.length));
		return NT_STATUS_INVALID_NETWORK_RESPONSE;
	}

	req->packet = talloc(req, struct wrepl_packet);
	NT_STATUS_HAVE_NO_MEMORY(req->packet);

	blob.data = packet_blob_in.data + 4;
	blob.length = packet_blob_in.length - 4;
	
	/* we have a full request - parse it */
	ndr_err = ndr_pull_struct_blob(&blob, req->packet, wrepl_socket->iconv_convenience, req->packet,
				       (ndr_pull_flags_fn_t)ndr_pull_wrepl_packet);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		NTSTATUS status = ndr_map_error2ntstatus(ndr_err);
		wrepl_request_finished(req, status);
		return NT_STATUS_OK;
	}

	if (DEBUGLVL(10)) {
		DEBUG(10,("Received WINS packet of length %u\n", 
			  (unsigned)packet_blob_in.length));
		NDR_PRINT_DEBUG(wrepl_packet, req->packet);
	}

	wrepl_request_finished(req, NT_STATUS_OK);
	return NT_STATUS_OK;
}

/*
  handler for winrepl events
*/
static void wrepl_handler(struct tevent_context *ev, struct tevent_fd *fde, 
			  uint16_t flags, void *private_data)
{
	struct wrepl_socket *wrepl_socket = talloc_get_type(private_data,
							    struct wrepl_socket);
	if (flags & EVENT_FD_READ) {
		packet_recv(wrepl_socket->packet);
		return;
	}
	if (flags & EVENT_FD_WRITE) {
		packet_queue_run(wrepl_socket->packet);
	}
}

static void wrepl_error(void *private_data, NTSTATUS status)
{
	struct wrepl_socket *wrepl_socket = talloc_get_type(private_data,
							    struct wrepl_socket);
	wrepl_socket_dead(wrepl_socket, status);
}


/*
  destroy a wrepl_socket destructor
*/
static int wrepl_socket_destructor(struct wrepl_socket *sock)
{
	if (sock->dead) {
		sock->free_skipped = true;
		return -1;
	}
	wrepl_socket_dead(sock, NT_STATUS_LOCAL_DISCONNECT);
	return 0;
}

/*
  initialise a wrepl_socket. The event_ctx is optional, if provided then
  operations will use that event context
*/
struct wrepl_socket *wrepl_socket_init(TALLOC_CTX *mem_ctx, 
				       struct tevent_context *event_ctx,
				       struct smb_iconv_convenience *iconv_convenience)
{
	struct wrepl_socket *wrepl_socket;
	NTSTATUS status;

	wrepl_socket = talloc_zero(mem_ctx, struct wrepl_socket);
	if (!wrepl_socket) return NULL;

	wrepl_socket->event.ctx = event_ctx;
	if (!wrepl_socket->event.ctx) goto failed;

	wrepl_socket->iconv_convenience = iconv_convenience;

	status = socket_create("ip", SOCKET_TYPE_STREAM, &wrepl_socket->sock, 0);
	if (!NT_STATUS_IS_OK(status)) goto failed;

	talloc_steal(wrepl_socket, wrepl_socket->sock);

	wrepl_socket->request_timeout	= WREPL_SOCKET_REQUEST_TIMEOUT;

	talloc_set_destructor(wrepl_socket, wrepl_socket_destructor);

	return wrepl_socket;

failed:
	talloc_free(wrepl_socket);
	return NULL;
}

/*
  initialise a wrepl_socket from an already existing connection
*/
struct wrepl_socket *wrepl_socket_merge(TALLOC_CTX *mem_ctx, 
				        struct tevent_context *event_ctx,
					struct socket_context *sock,
					struct packet_context *pack)
{
	struct wrepl_socket *wrepl_socket;

	wrepl_socket = talloc_zero(mem_ctx, struct wrepl_socket);
	if (wrepl_socket == NULL) goto failed;

	wrepl_socket->event.ctx = event_ctx;
	if (wrepl_socket->event.ctx == NULL) goto failed;

	wrepl_socket->sock = sock;
	talloc_steal(wrepl_socket, wrepl_socket->sock);


	wrepl_socket->request_timeout	= WREPL_SOCKET_REQUEST_TIMEOUT;

	wrepl_socket->event.fde = event_add_fd(wrepl_socket->event.ctx, wrepl_socket,
					       socket_get_fd(wrepl_socket->sock), 
					       EVENT_FD_READ,
					       wrepl_handler, wrepl_socket);
	if (wrepl_socket->event.fde == NULL) {
		goto failed;
	}

	wrepl_socket->packet = pack;
	talloc_steal(wrepl_socket, wrepl_socket->packet);
	packet_set_private(wrepl_socket->packet, wrepl_socket);
	packet_set_socket(wrepl_socket->packet, wrepl_socket->sock);
	packet_set_callback(wrepl_socket->packet, wrepl_finish_recv);
	packet_set_full_request(wrepl_socket->packet, packet_full_request_u32);
	packet_set_error_handler(wrepl_socket->packet, wrepl_error);
	packet_set_event_context(wrepl_socket->packet, wrepl_socket->event.ctx);
	packet_set_fde(wrepl_socket->packet, wrepl_socket->event.fde);
	packet_set_serialise(wrepl_socket->packet);

	talloc_set_destructor(wrepl_socket, wrepl_socket_destructor);
	
	return wrepl_socket;

failed:
	talloc_free(wrepl_socket);
	return NULL;
}

/*
  destroy a wrepl_request
*/
static int wrepl_request_destructor(struct wrepl_request *req)
{
	if (req->state == WREPL_REQUEST_RECV) {
		DLIST_REMOVE(req->wrepl_socket->recv_queue, req);
	}
	req->state = WREPL_REQUEST_ERROR;
	return 0;
}

/*
  wait for a request to complete
*/
static NTSTATUS wrepl_request_wait(struct wrepl_request *req)
{
	NT_STATUS_HAVE_NO_MEMORY(req);
	while (req->state < WREPL_REQUEST_DONE) {
		event_loop_once(req->wrepl_socket->event.ctx);
	}
	return req->status;
}

struct wrepl_connect_state {
	struct composite_context *result;
	struct wrepl_socket *wrepl_socket;
	struct composite_context *creq;
};

/*
  handler for winrepl connection completion
*/
static void wrepl_connect_handler(struct composite_context *creq)
{
	struct wrepl_connect_state *state = talloc_get_type(creq->async.private_data, 
					    struct wrepl_connect_state);
	struct wrepl_socket *wrepl_socket = state->wrepl_socket;
	struct composite_context *result = state->result;

	result->status = socket_connect_recv(state->creq);
	if (!composite_is_ok(result)) return;

	wrepl_socket->event.fde = event_add_fd(wrepl_socket->event.ctx, wrepl_socket, 
					       socket_get_fd(wrepl_socket->sock), 
					       EVENT_FD_READ,
					       wrepl_handler, wrepl_socket);
	if (composite_nomem(wrepl_socket->event.fde, result)) return;

	/* setup the stream -> packet parser */
	wrepl_socket->packet = packet_init(wrepl_socket);
	if (composite_nomem(wrepl_socket->packet, result)) return;
	packet_set_private(wrepl_socket->packet, wrepl_socket);
	packet_set_socket(wrepl_socket->packet, wrepl_socket->sock);
	packet_set_callback(wrepl_socket->packet, wrepl_finish_recv);
	packet_set_full_request(wrepl_socket->packet, packet_full_request_u32);
	packet_set_error_handler(wrepl_socket->packet, wrepl_error);
	packet_set_event_context(wrepl_socket->packet, wrepl_socket->event.ctx);
	packet_set_fde(wrepl_socket->packet, wrepl_socket->event.fde);
	packet_set_serialise(wrepl_socket->packet);

	composite_done(result);
}

const char *wrepl_best_ip(struct loadparm_context *lp_ctx, const char *peer_ip)
{
	struct interface *ifaces;
	load_interfaces(lp_ctx, lp_interfaces(lp_ctx), &ifaces);
	return iface_best_ip(ifaces, peer_ip);
}


/*
  connect a wrepl_socket to a WINS server
*/
struct composite_context *wrepl_connect_send(struct wrepl_socket *wrepl_socket,
					     const char *our_ip, const char *peer_ip)
{
	struct composite_context *result;
	struct wrepl_connect_state *state;
	struct socket_address *peer, *us;

	result = talloc_zero(wrepl_socket, struct composite_context);
	if (!result) return NULL;

	result->state		= COMPOSITE_STATE_IN_PROGRESS;
	result->event_ctx	= wrepl_socket->event.ctx;

	state = talloc_zero(result, struct wrepl_connect_state);
	if (composite_nomem(state, result)) return result;
	result->private_data	= state;
	state->result		= result;
	state->wrepl_socket	= wrepl_socket;

	us = socket_address_from_strings(state, wrepl_socket->sock->backend_name, 
					 our_ip, 0);
	if (composite_nomem(us, result)) return result;

	peer = socket_address_from_strings(state, wrepl_socket->sock->backend_name, 
					   peer_ip, WINS_REPLICATION_PORT);
	if (composite_nomem(peer, result)) return result;

	state->creq = socket_connect_send(wrepl_socket->sock, us, peer,
					  0, wrepl_socket->event.ctx);
	composite_continue(result, state->creq, wrepl_connect_handler, state);
	return result;
}

/*
  connect a wrepl_socket to a WINS server - recv side
*/
NTSTATUS wrepl_connect_recv(struct composite_context *result)
{
	struct wrepl_connect_state *state = talloc_get_type(result->private_data,
					    struct wrepl_connect_state);
	struct wrepl_socket *wrepl_socket = state->wrepl_socket;
	NTSTATUS status = composite_wait(result);

	if (!NT_STATUS_IS_OK(status)) {
		wrepl_socket_dead(wrepl_socket, status);
	}

	talloc_free(result);
	return status;
}

/*
  connect a wrepl_socket to a WINS server - sync API
*/
NTSTATUS wrepl_connect(struct wrepl_socket *wrepl_socket,
		       const char *our_ip, const char *peer_ip)
{
	struct composite_context *c_req = wrepl_connect_send(wrepl_socket, our_ip, peer_ip);
	return wrepl_connect_recv(c_req);
}

/* 
   callback from wrepl_request_trigger() 
*/
static void wrepl_request_trigger_handler(struct tevent_context *ev, struct tevent_timer *te,
					  struct timeval t, void *ptr)
{
	struct wrepl_request *req = talloc_get_type(ptr, struct wrepl_request);
	if (req->async.fn) {
		req->async.fn(req);
	}
}

/*
  trigger an immediate event on a wrepl_request
  the return value should only be used in wrepl_request_send()
  this is the only place where req->trigger is true
*/
static struct wrepl_request *wrepl_request_finished(struct wrepl_request *req, NTSTATUS status)
{
	struct tevent_timer *te;

	if (req->state == WREPL_REQUEST_RECV) {
		DLIST_REMOVE(req->wrepl_socket->recv_queue, req);
	}

	if (!NT_STATUS_IS_OK(status)) {
		req->state	= WREPL_REQUEST_ERROR;
	} else {
		req->state	= WREPL_REQUEST_DONE;
	}

	req->status	= status;

	if (req->trigger) {
		req->trigger = false;
		/* a zero timeout means immediate */
		te = event_add_timed(req->wrepl_socket->event.ctx,
				     req, timeval_zero(),
				     wrepl_request_trigger_handler, req);
		if (!te) {
			talloc_free(req);
			return NULL;
		}
		return req;
	}

	if (req->async.fn) {
		req->async.fn(req);
	}
	return NULL;
}

struct wrepl_send_ctrl_state {
	struct wrepl_send_ctrl ctrl;
	struct wrepl_request *req;
	struct wrepl_socket *wrepl_sock;
};

static int wrepl_send_ctrl_destructor(struct wrepl_send_ctrl_state *s)
{
	struct wrepl_request *req = s->wrepl_sock->recv_queue;

	/* check if the request is still in WREPL_STATE_RECV,
	 * we need this here because the caller has may called 
	 * talloc_free(req) and wrepl_send_ctrl_state isn't
	 * a talloc child of the request, so our s->req pointer
	 * is maybe invalid!
	 */
	for (; req; req = req->next) {
		if (req == s->req) break;
	}
	if (!req) return 0;

	/* here, we need to make sure the async request handler is called
	 * later in the next event_loop and now now
	 */
	req->trigger = true;
	wrepl_request_finished(req, NT_STATUS_OK);

	if (s->ctrl.disconnect_after_send) {
		wrepl_socket_dead(s->wrepl_sock, NT_STATUS_LOCAL_DISCONNECT);
	}

	return 0;
}

/*
  send a generic wins replication request
*/
struct wrepl_request *wrepl_request_send(struct wrepl_socket *wrepl_socket,
					 struct wrepl_packet *packet,
					 struct wrepl_send_ctrl *ctrl)
{
	struct wrepl_request *req;
	struct wrepl_wrap wrap;
	DATA_BLOB blob;
	NTSTATUS status;
	enum ndr_err_code ndr_err;

	req = talloc_zero(wrepl_socket, struct wrepl_request);
	if (!req) return NULL;
	req->wrepl_socket = wrepl_socket;
	req->state        = WREPL_REQUEST_RECV;
	req->trigger      = true;

	DLIST_ADD_END(wrepl_socket->recv_queue, req, struct wrepl_request *);
	talloc_set_destructor(req, wrepl_request_destructor);

	if (wrepl_socket->dead) {
		return wrepl_request_finished(req, NT_STATUS_INVALID_CONNECTION);
	}

	wrap.packet = *packet;
	ndr_err = ndr_push_struct_blob(&blob, req, wrepl_socket->iconv_convenience, &wrap, 
				       (ndr_push_flags_fn_t)ndr_push_wrepl_wrap);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		status = ndr_map_error2ntstatus(ndr_err);
		return wrepl_request_finished(req, status);
	}

	if (DEBUGLVL(10)) {
		DEBUG(10,("Sending WINS packet of length %u\n", 
			  (unsigned)blob.length));
		NDR_PRINT_DEBUG(wrepl_packet, &wrap.packet);
	}

	if (wrepl_socket->request_timeout > 0) {
		req->te = event_add_timed(wrepl_socket->event.ctx, req, 
					  timeval_current_ofs(wrepl_socket->request_timeout, 0), 
					  wrepl_request_timeout_handler, req);
		if (!req->te) return wrepl_request_finished(req, NT_STATUS_NO_MEMORY);
	}

	if (ctrl && (ctrl->send_only || ctrl->disconnect_after_send)) {
		struct wrepl_send_ctrl_state *s = talloc(blob.data, struct wrepl_send_ctrl_state);
		if (!s) return wrepl_request_finished(req, NT_STATUS_NO_MEMORY);
		s->ctrl		= *ctrl;
		s->req		= req;
		s->wrepl_sock	= wrepl_socket;
		talloc_set_destructor(s, wrepl_send_ctrl_destructor);
	}

	status = packet_send(wrepl_socket->packet, blob);
	if (!NT_STATUS_IS_OK(status)) {
		return wrepl_request_finished(req, status);
	}

	req->trigger = false;
	return req;
}

/*
  receive a generic WINS replication reply
*/
NTSTATUS wrepl_request_recv(struct wrepl_request *req,
			    TALLOC_CTX *mem_ctx,
			    struct wrepl_packet **packet)
{
	NTSTATUS status = wrepl_request_wait(req);
	if (NT_STATUS_IS_OK(status) && packet) {
		*packet = talloc_steal(mem_ctx, req->packet);
	}
	talloc_free(req);
	return status;
}

/*
  a full WINS replication request/response
*/
NTSTATUS wrepl_request(struct wrepl_socket *wrepl_socket,
		       TALLOC_CTX *mem_ctx,
		       struct wrepl_packet *req_packet,
		       struct wrepl_packet **reply_packet)
{
	struct wrepl_request *req = wrepl_request_send(wrepl_socket, req_packet, NULL);
	return wrepl_request_recv(req, mem_ctx, reply_packet);
}


/*
  setup an association - send
*/
struct wrepl_request *wrepl_associate_send(struct wrepl_socket *wrepl_socket,
					   struct wrepl_associate *io)
{
	struct wrepl_packet *packet;
	struct wrepl_request *req;

	packet = talloc_zero(wrepl_socket, struct wrepl_packet);
	if (packet == NULL) return NULL;

	packet->opcode                      = WREPL_OPCODE_BITS;
	packet->mess_type                   = WREPL_START_ASSOCIATION;
	packet->message.start.minor_version = 2;
	packet->message.start.major_version = 5;

	/*
	 * nt4 uses 41 bytes for the start_association call
	 * so do it the same and as we don't know th emeanings of this bytes
	 * we just send zeros and nt4, w2k and w2k3 seems to be happy with this
	 *
	 * if we don't do this nt4 uses an old version of the wins replication protocol
	 * and that would break nt4 <-> samba replication
	 */
	packet->padding	= data_blob_talloc(packet, NULL, 21);
	if (packet->padding.data == NULL) {
		talloc_free(packet);
		return NULL;
	}
	memset(packet->padding.data, 0, packet->padding.length);

	req = wrepl_request_send(wrepl_socket, packet, NULL);

	talloc_free(packet);

	return req;	
}

/*
  setup an association - recv
*/
NTSTATUS wrepl_associate_recv(struct wrepl_request *req,
			      struct wrepl_associate *io)
{
	struct wrepl_packet *packet=NULL;
	NTSTATUS status;
	status = wrepl_request_recv(req, req->wrepl_socket, &packet);
	NT_STATUS_NOT_OK_RETURN(status);
	if (packet->mess_type != WREPL_START_ASSOCIATION_REPLY) {
		status = NT_STATUS_UNEXPECTED_NETWORK_ERROR;
	}
	if (NT_STATUS_IS_OK(status)) {
		io->out.assoc_ctx = packet->message.start_reply.assoc_ctx;
		io->out.major_version = packet->message.start_reply.major_version;
	}
	talloc_free(packet);
	return status;
}

/*
  setup an association - sync api
*/
NTSTATUS wrepl_associate(struct wrepl_socket *wrepl_socket,
			 struct wrepl_associate *io)
{
	struct wrepl_request *req = wrepl_associate_send(wrepl_socket, io);
	return wrepl_associate_recv(req, io);
}


/*
  stop an association - send
*/
struct wrepl_request *wrepl_associate_stop_send(struct wrepl_socket *wrepl_socket,
						struct wrepl_associate_stop *io)
{
	struct wrepl_packet *packet;
	struct wrepl_request *req;
	struct wrepl_send_ctrl ctrl;

	packet = talloc_zero(wrepl_socket, struct wrepl_packet);
	if (packet == NULL) return NULL;

	packet->opcode			= WREPL_OPCODE_BITS;
	packet->assoc_ctx		= io->in.assoc_ctx;
	packet->mess_type		= WREPL_STOP_ASSOCIATION;
	packet->message.stop.reason	= io->in.reason;

	ZERO_STRUCT(ctrl);
	if (io->in.reason == 0) {
		ctrl.send_only			= true;
		ctrl.disconnect_after_send	= true;
	}

	req = wrepl_request_send(wrepl_socket, packet, &ctrl);

	talloc_free(packet);

	return req;	
}

/*
  stop an association - recv
*/
NTSTATUS wrepl_associate_stop_recv(struct wrepl_request *req,
				   struct wrepl_associate_stop *io)
{
	struct wrepl_packet *packet=NULL;
	NTSTATUS status;
	status = wrepl_request_recv(req, req->wrepl_socket, &packet);
	NT_STATUS_NOT_OK_RETURN(status);
	talloc_free(packet);
	return status;
}

/*
  setup an association - sync api
*/
NTSTATUS wrepl_associate_stop(struct wrepl_socket *wrepl_socket,
			      struct wrepl_associate_stop *io)
{
	struct wrepl_request *req = wrepl_associate_stop_send(wrepl_socket, io);
	return wrepl_associate_stop_recv(req, io);
}

/*
  fetch the partner tables - send
*/
struct wrepl_request *wrepl_pull_table_send(struct wrepl_socket *wrepl_socket,
					    struct wrepl_pull_table *io)
{
	struct wrepl_packet *packet;
	struct wrepl_request *req;

	packet = talloc_zero(wrepl_socket, struct wrepl_packet);
	if (packet == NULL) return NULL;

	packet->opcode                      = WREPL_OPCODE_BITS;
	packet->assoc_ctx                   = io->in.assoc_ctx;
	packet->mess_type                   = WREPL_REPLICATION;
	packet->message.replication.command = WREPL_REPL_TABLE_QUERY;

	req = wrepl_request_send(wrepl_socket, packet, NULL);

	talloc_free(packet);

	return req;	
}


/*
  fetch the partner tables - recv
*/
NTSTATUS wrepl_pull_table_recv(struct wrepl_request *req,
			       TALLOC_CTX *mem_ctx,
			       struct wrepl_pull_table *io)
{
	struct wrepl_packet *packet=NULL;
	NTSTATUS status;
	struct wrepl_table *table;
	int i;

	status = wrepl_request_recv(req, req->wrepl_socket, &packet);
	NT_STATUS_NOT_OK_RETURN(status);
	if (packet->mess_type != WREPL_REPLICATION) {
		status = NT_STATUS_NETWORK_ACCESS_DENIED;
	} else if (packet->message.replication.command != WREPL_REPL_TABLE_REPLY) {
		status = NT_STATUS_UNEXPECTED_NETWORK_ERROR;
	}
	if (!NT_STATUS_IS_OK(status)) goto failed;

	table = &packet->message.replication.info.table;
	io->out.num_partners = table->partner_count;
	io->out.partners = talloc_steal(mem_ctx, table->partners);
	for (i=0;i<io->out.num_partners;i++) {
		talloc_steal(io->out.partners, io->out.partners[i].address);
	}

failed:
	talloc_free(packet);
	return status;
}


/*
  fetch the partner table - sync api
*/
NTSTATUS wrepl_pull_table(struct wrepl_socket *wrepl_socket,
			  TALLOC_CTX *mem_ctx,
			  struct wrepl_pull_table *io)
{
	struct wrepl_request *req = wrepl_pull_table_send(wrepl_socket, io);
	return wrepl_pull_table_recv(req, mem_ctx, io);
}


/*
  fetch the names for a WINS partner - send
*/
struct wrepl_request *wrepl_pull_names_send(struct wrepl_socket *wrepl_socket,
					    struct wrepl_pull_names *io)
{
	struct wrepl_packet *packet;
	struct wrepl_request *req;

	packet = talloc_zero(wrepl_socket, struct wrepl_packet);
	if (packet == NULL) return NULL;

	packet->opcode                         = WREPL_OPCODE_BITS;
	packet->assoc_ctx                      = io->in.assoc_ctx;
	packet->mess_type                      = WREPL_REPLICATION;
	packet->message.replication.command    = WREPL_REPL_SEND_REQUEST;
	packet->message.replication.info.owner = io->in.partner;

	req = wrepl_request_send(wrepl_socket, packet, NULL);

	talloc_free(packet);

	return req;	
}

/*
  fetch the names for a WINS partner - recv
*/
NTSTATUS wrepl_pull_names_recv(struct wrepl_request *req,
			       TALLOC_CTX *mem_ctx,
			       struct wrepl_pull_names *io)
{
	struct wrepl_packet *packet=NULL;
	NTSTATUS status;
	int i;

	status = wrepl_request_recv(req, req->wrepl_socket, &packet);
	NT_STATUS_NOT_OK_RETURN(status);
	if (packet->mess_type != WREPL_REPLICATION ||
	    packet->message.replication.command != WREPL_REPL_SEND_REPLY) {
		status = NT_STATUS_UNEXPECTED_NETWORK_ERROR;
	}
	if (!NT_STATUS_IS_OK(status)) goto failed;

	io->out.num_names = packet->message.replication.info.reply.num_names;

	io->out.names = talloc_array(packet, struct wrepl_name, io->out.num_names);
	if (io->out.names == NULL) goto nomem;

	/* convert the list of names and addresses to a sane format */
	for (i=0;i<io->out.num_names;i++) {
		struct wrepl_wins_name *wname = &packet->message.replication.info.reply.names[i];
		struct wrepl_name *name = &io->out.names[i];

		name->name	= *wname->name;
		talloc_steal(io->out.names, wname->name);
		name->type	= WREPL_NAME_TYPE(wname->flags);
		name->state	= WREPL_NAME_STATE(wname->flags);
		name->node	= WREPL_NAME_NODE(wname->flags);
		name->is_static	= WREPL_NAME_IS_STATIC(wname->flags);
		name->raw_flags	= wname->flags;
		name->version_id= wname->id;
		name->owner	= talloc_strdup(io->out.names, io->in.partner.address);
		if (name->owner == NULL) goto nomem;

		/* trying to save 1 or 2 bytes on the wire isn't a good idea */
		if (wname->flags & 2) {
			int j;

			name->num_addresses = wname->addresses.addresses.num_ips;
			name->addresses = talloc_array(io->out.names, 
						       struct wrepl_address, 
						       name->num_addresses);
			if (name->addresses == NULL) goto nomem;
			for (j=0;j<name->num_addresses;j++) {
				name->addresses[j].owner = 
					talloc_steal(name->addresses, 
						     wname->addresses.addresses.ips[j].owner);
				name->addresses[j].address = 
					talloc_steal(name->addresses, 
						     wname->addresses.addresses.ips[j].ip);
			}
		} else {
			name->num_addresses = 1;
			name->addresses = talloc(io->out.names, struct wrepl_address);
			if (name->addresses == NULL) goto nomem;
			name->addresses[0].owner = talloc_strdup(name->addresses,io->in.partner.address);
			if (name->addresses[0].owner == NULL) goto nomem;
			name->addresses[0].address = talloc_steal(name->addresses,
								  wname->addresses.ip);
		}
	}

	talloc_steal(mem_ctx, io->out.names);
	talloc_free(packet);
	return NT_STATUS_OK;
nomem:
	status = NT_STATUS_NO_MEMORY;
failed:
	talloc_free(packet);
	return status;
}



/*
  fetch the names for a WINS partner - sync api
*/
NTSTATUS wrepl_pull_names(struct wrepl_socket *wrepl_socket,
			  TALLOC_CTX *mem_ctx,
			  struct wrepl_pull_names *io)
{
	struct wrepl_request *req = wrepl_pull_names_send(wrepl_socket, io);
	return wrepl_pull_names_recv(req, mem_ctx, io);
}
