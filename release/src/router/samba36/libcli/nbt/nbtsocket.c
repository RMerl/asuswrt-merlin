/*
   Unix SMB/CIFS implementation.

   low level socket handling for nbt requests

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
#include "../libcli/nbt/libnbt.h"
#include "../libcli/nbt/nbt_proto.h"
#include "lib/socket/socket.h"
#include "librpc/gen_ndr/ndr_nbt.h"
#include "param/param.h"

#define NBT_MAX_REPLIES 1000

/*
  destroy a pending request
*/
static int nbt_name_request_destructor(struct nbt_name_request *req)
{
	if (req->state == NBT_REQUEST_SEND) {
		DLIST_REMOVE(req->nbtsock->send_queue, req);
	}
	if (req->state == NBT_REQUEST_WAIT) {
		req->nbtsock->num_pending--;
	}
	if (req->name_trn_id != 0 && !req->is_reply) {
		idr_remove(req->nbtsock->idr, req->name_trn_id);
		req->name_trn_id = 0;
	}
	if (req->te) {
		talloc_free(req->te);
		req->te = NULL;
	}
	if (req->nbtsock->send_queue == NULL) {
		EVENT_FD_NOT_WRITEABLE(req->nbtsock->fde);
	}
	if (req->nbtsock->num_pending == 0 &&
	    req->nbtsock->incoming.handler == NULL) {
		EVENT_FD_NOT_READABLE(req->nbtsock->fde);
	}
	return 0;
}


/*
  handle send events on a nbt name socket
*/
static void nbt_name_socket_send(struct nbt_name_socket *nbtsock)
{
	struct nbt_name_request *req = nbtsock->send_queue;
	TALLOC_CTX *tmp_ctx = talloc_new(nbtsock);
	NTSTATUS status;

	while ((req = nbtsock->send_queue)) {
		size_t len;

		len = req->encoded.length;
		status = socket_sendto(nbtsock->sock, &req->encoded, &len,
				       req->dest);
		if (NT_STATUS_IS_ERR(status)) goto failed;

		if (!NT_STATUS_IS_OK(status)) {
			talloc_free(tmp_ctx);
			return;
		}

		DLIST_REMOVE(nbtsock->send_queue, req);
		req->state = NBT_REQUEST_WAIT;
		if (req->is_reply) {
			talloc_free(req);
		} else {
			EVENT_FD_READABLE(nbtsock->fde);
			nbtsock->num_pending++;
		}
	}

	EVENT_FD_NOT_WRITEABLE(nbtsock->fde);
	talloc_free(tmp_ctx);
	return;

failed:
	DLIST_REMOVE(nbtsock->send_queue, req);
	nbt_name_request_destructor(req);
	req->status = status;
	req->state = NBT_REQUEST_ERROR;
	talloc_free(tmp_ctx);
	if (req->async.fn) {
		req->async.fn(req);
	} else if (req->is_reply) {
		talloc_free(req);
	}
	return;
}


/*
  handle a request timeout
*/
static void nbt_name_socket_timeout(struct tevent_context *ev, struct tevent_timer *te,
				    struct timeval t, void *private_data)
{
	struct nbt_name_request *req = talloc_get_type(private_data,
						       struct nbt_name_request);

	if (req->num_retries != 0) {
		req->num_retries--;
		req->te = event_add_timed(req->nbtsock->event_ctx, req,
					  timeval_add(&t, req->timeout, 0),
					  nbt_name_socket_timeout, req);
		if (req->state != NBT_REQUEST_SEND) {
			req->state = NBT_REQUEST_SEND;
			DLIST_ADD_END(req->nbtsock->send_queue, req,
				      struct nbt_name_request *);
		}
		EVENT_FD_WRITEABLE(req->nbtsock->fde);
		return;
	}

	nbt_name_request_destructor(req);
	if (req->num_replies == 0) {
		req->state = NBT_REQUEST_TIMEOUT;
		req->status = NT_STATUS_IO_TIMEOUT;
	} else {
		req->state = NBT_REQUEST_DONE;
		req->status = NT_STATUS_OK;
	}
	if (req->async.fn) {
		req->async.fn(req);
	} else if (req->is_reply) {
		talloc_free(req);
	}
}



/**
  handle recv events on a nbt name socket
*/
static void nbt_name_socket_recv(struct nbt_name_socket *nbtsock)
{
	TALLOC_CTX *tmp_ctx = talloc_new(nbtsock);
	NTSTATUS status;
	enum ndr_err_code ndr_err;
	struct socket_address *src;
	DATA_BLOB blob;
	size_t nread, dsize;
	struct nbt_name_packet *packet;
	struct nbt_name_request *req;

	status = socket_pending(nbtsock->sock, &dsize);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(tmp_ctx);
		return;
	}

	blob = data_blob_talloc(tmp_ctx, NULL, dsize);
	if (blob.data == NULL) {
		talloc_free(tmp_ctx);
		return;
	}

	status = socket_recvfrom(nbtsock->sock, blob.data, blob.length, &nread,
				 tmp_ctx, &src);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(tmp_ctx);
		return;
	}

	packet = talloc(tmp_ctx, struct nbt_name_packet);
	if (packet == NULL) {
		talloc_free(tmp_ctx);
		return;
	}

	/* parse the request */
	ndr_err = ndr_pull_struct_blob(&blob, packet, packet,
				       (ndr_pull_flags_fn_t)ndr_pull_nbt_name_packet);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		status = ndr_map_error2ntstatus(ndr_err);
		DEBUG(2,("Failed to parse incoming NBT name packet - %s\n",
			 nt_errstr(status)));
		talloc_free(tmp_ctx);
		return;
	}

	if (DEBUGLVL(10)) {
		DEBUG(10,("Received nbt packet of length %d from %s:%d\n",
			  (int)blob.length, src->addr, src->port));
		NDR_PRINT_DEBUG(nbt_name_packet, packet);
	}

	/* if its not a reply then pass it off to the incoming request
	   handler, if any */
	if (!(packet->operation & NBT_FLAG_REPLY)) {
		if (nbtsock->incoming.handler) {
			nbtsock->incoming.handler(nbtsock, packet, src);
		}
		talloc_free(tmp_ctx);
		return;
	}

	/* find the matching request */
	req = (struct nbt_name_request *)idr_find(nbtsock->idr,
						  packet->name_trn_id);
	if (req == NULL) {
		if (nbtsock->unexpected.handler) {
			nbtsock->unexpected.handler(nbtsock, packet, src);
		} else {
			DEBUG(10,("Failed to match request for incoming name packet id 0x%04x on %p\n",
				 packet->name_trn_id, nbtsock));
		}
		talloc_free(tmp_ctx);
		return;
	}

	talloc_steal(req, packet);
	talloc_steal(req, src);
	talloc_free(tmp_ctx);
	nbt_name_socket_handle_response_packet(req, packet, src);
}

void nbt_name_socket_handle_response_packet(struct nbt_name_request *req,
					    struct nbt_name_packet *packet,
					    struct socket_address *src)
{
	/* if this is a WACK response, this we need to go back to waiting,
	   but perhaps increase the timeout */
	if ((packet->operation & NBT_OPCODE) == NBT_OPCODE_WACK) {
		uint32_t ttl;
		if (req->received_wack || packet->ancount < 1) {
			nbt_name_request_destructor(req);
			req->status = NT_STATUS_INVALID_NETWORK_RESPONSE;
			req->state  = NBT_REQUEST_ERROR;
			goto done;
		}
		talloc_free(req->te);
		/* we know we won't need any more retries - the server
		   has received our request */
		req->num_retries   = 0;
		req->received_wack = true;
		/*
		 * there is a timeout in the packet,
		 * it is 5 + 4 * num_old_addresses
		 *
		 * although w2k3 screws it up
		 * and uses num_old_addresses = 0
		 *
		 * so we better fallback to the maximum
		 * of num_old_addresses = 25 if we got
		 * a timeout of less than 9s (5 + 4*1)
		 * or more than 105s (5 + 4*25).
		 */
		ttl = packet->answers[0].ttl;
		if ((ttl < (5 + 4*1)) || (ttl > (5 + 4*25))) {
			ttl = 5 + 4*25;
		}
		req->timeout = ttl;
		req->te = event_add_timed(req->nbtsock->event_ctx, req,
					  timeval_current_ofs(req->timeout, 0),
					  nbt_name_socket_timeout, req);
		return;
	}


	req->replies = talloc_realloc(req, req->replies, struct nbt_name_reply, req->num_replies+1);
	if (req->replies == NULL) {
		nbt_name_request_destructor(req);
		req->state  = NBT_REQUEST_ERROR;
		req->status = NT_STATUS_NO_MEMORY;
		goto done;
	}

	talloc_steal(req, src);
	req->replies[req->num_replies].dest   = src;
	talloc_steal(req, packet);
	req->replies[req->num_replies].packet = packet;
	req->num_replies++;

	/* if we don't want multiple replies then we are done */
	if (req->allow_multiple_replies &&
	    req->num_replies < NBT_MAX_REPLIES) {
		return;
	}

	nbt_name_request_destructor(req);
	req->state  = NBT_REQUEST_DONE;
	req->status = NT_STATUS_OK;

done:
	if (req->async.fn) {
		req->async.fn(req);
	}
}

/*
  handle fd events on a nbt_name_socket
*/
static void nbt_name_socket_handler(struct tevent_context *ev, struct tevent_fd *fde,
				    uint16_t flags, void *private_data)
{
	struct nbt_name_socket *nbtsock = talloc_get_type(private_data,
							  struct nbt_name_socket);
	if (flags & EVENT_FD_WRITE) {
		nbt_name_socket_send(nbtsock);
	}
	if (flags & EVENT_FD_READ) {
		nbt_name_socket_recv(nbtsock);
	}
}


/*
  initialise a nbt_name_socket. The event_ctx is optional, if provided
  then operations will use that event context
*/
_PUBLIC_ struct nbt_name_socket *nbt_name_socket_init(TALLOC_CTX *mem_ctx,
					     struct tevent_context *event_ctx)
{
	struct nbt_name_socket *nbtsock;
	NTSTATUS status;

	nbtsock = talloc(mem_ctx, struct nbt_name_socket);
	if (nbtsock == NULL) goto failed;

	nbtsock->event_ctx = event_ctx;
	if (nbtsock->event_ctx == NULL) goto failed;

	status = socket_create("ip", SOCKET_TYPE_DGRAM, &nbtsock->sock, 0);
	if (!NT_STATUS_IS_OK(status)) goto failed;

	socket_set_option(nbtsock->sock, "SO_BROADCAST", "1");

	talloc_steal(nbtsock, nbtsock->sock);

	nbtsock->idr = idr_init(nbtsock);
	if (nbtsock->idr == NULL) goto failed;

	nbtsock->send_queue = NULL;
	nbtsock->num_pending = 0;
	nbtsock->incoming.handler = NULL;
	nbtsock->unexpected.handler = NULL;

	nbtsock->fde = event_add_fd(nbtsock->event_ctx, nbtsock,
				    socket_get_fd(nbtsock->sock), 0,
				    nbt_name_socket_handler, nbtsock);

	return nbtsock;

failed:
	talloc_free(nbtsock);
	return NULL;
}

/*
  send off a nbt name request
*/
struct nbt_name_request *nbt_name_request_send(struct nbt_name_socket *nbtsock,
					       struct socket_address *dest,
					       struct nbt_name_packet *request,
					       int timeout, int retries,
					       bool allow_multiple_replies)
{
	struct nbt_name_request *req;
	int id;
	enum ndr_err_code ndr_err;

	req = talloc_zero(nbtsock, struct nbt_name_request);
	if (req == NULL) goto failed;

	req->nbtsock                = nbtsock;
	req->allow_multiple_replies = allow_multiple_replies;
	req->state                  = NBT_REQUEST_SEND;
	req->is_reply               = false;
	req->timeout                = timeout;
	req->num_retries            = retries;
	req->dest                   = dest;
	if (talloc_reference(req, dest) == NULL) goto failed;

	/* we select a random transaction id unless the user supplied one */
	if (request->name_trn_id == 0) {
		id = idr_get_new_random(req->nbtsock->idr, req, UINT16_MAX);
	} else {
		if (idr_find(req->nbtsock->idr, request->name_trn_id)) goto failed;
		id = idr_get_new_above(req->nbtsock->idr, req, request->name_trn_id,
				       UINT16_MAX);
	}
	if (id == -1) goto failed;

	request->name_trn_id = id;
	req->name_trn_id     = id;

	req->te = event_add_timed(nbtsock->event_ctx, req,
				  timeval_current_ofs(req->timeout, 0),
				  nbt_name_socket_timeout, req);

	talloc_set_destructor(req, nbt_name_request_destructor);

	ndr_err = ndr_push_struct_blob(&req->encoded, req,
				       request,
				       (ndr_push_flags_fn_t)ndr_push_nbt_name_packet);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) goto failed;

	DLIST_ADD_END(nbtsock->send_queue, req, struct nbt_name_request *);

	if (DEBUGLVL(10)) {
		DEBUG(10,("Queueing nbt packet to %s:%d\n",
			  req->dest->addr, req->dest->port));
		NDR_PRINT_DEBUG(nbt_name_packet, request);
	}

	EVENT_FD_WRITEABLE(nbtsock->fde);

	return req;

failed:
	talloc_free(req);
	return NULL;
}


/*
  send off a nbt name reply
*/
_PUBLIC_ NTSTATUS nbt_name_reply_send(struct nbt_name_socket *nbtsock,
			     struct socket_address *dest,
			     struct nbt_name_packet *request)
{
	struct nbt_name_request *req;
	enum ndr_err_code ndr_err;

	req = talloc_zero(nbtsock, struct nbt_name_request);
	NT_STATUS_HAVE_NO_MEMORY(req);

	req->nbtsock   = nbtsock;
	req->dest = dest;
	if (talloc_reference(req, dest) == NULL) goto failed;
	req->state     = NBT_REQUEST_SEND;
	req->is_reply = true;

	talloc_set_destructor(req, nbt_name_request_destructor);

	if (DEBUGLVL(10)) {
		NDR_PRINT_DEBUG(nbt_name_packet, request);
	}

	ndr_err = ndr_push_struct_blob(&req->encoded, req,
				       request,
				       (ndr_push_flags_fn_t)ndr_push_nbt_name_packet);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(req);
		return ndr_map_error2ntstatus(ndr_err);
	}

	DLIST_ADD_END(nbtsock->send_queue, req, struct nbt_name_request *);

	EVENT_FD_WRITEABLE(nbtsock->fde);

	return NT_STATUS_OK;

failed:
	talloc_free(req);
	return NT_STATUS_NO_MEMORY;
}

/*
  wait for a nbt request to complete
*/
NTSTATUS nbt_name_request_recv(struct nbt_name_request *req)
{
	if (!req) return NT_STATUS_NO_MEMORY;

	while (req->state < NBT_REQUEST_DONE) {
		if (event_loop_once(req->nbtsock->event_ctx) != 0) {
			req->state = NBT_REQUEST_ERROR;
			req->status = NT_STATUS_UNEXPECTED_NETWORK_ERROR;
			break;
		}
	}
	return req->status;
}


/*
  setup a handler for incoming requests
*/
_PUBLIC_ NTSTATUS nbt_set_incoming_handler(struct nbt_name_socket *nbtsock,
				  void (*handler)(struct nbt_name_socket *, struct nbt_name_packet *,
						  struct socket_address *),
				  void *private_data)
{
	nbtsock->incoming.handler = handler;
	nbtsock->incoming.private_data = private_data;
	EVENT_FD_READABLE(nbtsock->fde);
	return NT_STATUS_OK;
}

/*
  setup a handler for unexpected requests
*/
NTSTATUS nbt_set_unexpected_handler(struct nbt_name_socket *nbtsock,
				    void (*handler)(struct nbt_name_socket *, struct nbt_name_packet *,
						    struct socket_address *),
				    void *private_data)
{
	nbtsock->unexpected.handler = handler;
	nbtsock->unexpected.private_data = private_data;
	EVENT_FD_READABLE(nbtsock->fde);
	return NT_STATUS_OK;
}

/*
  turn a NBT rcode into a NTSTATUS
*/
_PUBLIC_ NTSTATUS nbt_rcode_to_ntstatus(uint8_t rcode)
{
	int i;
	struct {
		enum nbt_rcode rcode;
		NTSTATUS status;
	} map[] = {
		{ NBT_RCODE_FMT, NT_STATUS_INVALID_PARAMETER },
		{ NBT_RCODE_SVR, NT_STATUS_SERVER_DISABLED },
		{ NBT_RCODE_NAM, NT_STATUS_OBJECT_NAME_NOT_FOUND },
		{ NBT_RCODE_IMP, NT_STATUS_NOT_SUPPORTED },
		{ NBT_RCODE_RFS, NT_STATUS_ACCESS_DENIED },
		{ NBT_RCODE_ACT, NT_STATUS_ADDRESS_ALREADY_EXISTS },
		{ NBT_RCODE_CFT, NT_STATUS_CONFLICTING_ADDRESSES }
	};
	for (i=0;i<ARRAY_SIZE(map);i++) {
		if (map[i].rcode == rcode) {
			return map[i].status;
		}
	}
	return NT_STATUS_UNSUCCESSFUL;
}
