/* 
   Unix SMB/CIFS implementation.
   SMB client transport context management functions

   Copyright (C) Andrew Tridgell 1994-2005
   Copyright (C) James Myers 2003 <myersjj@samba.org>
   
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
#include "libcli/raw/libcliraw.h"
#include "libcli/raw/raw_proto.h"
#include "lib/socket/socket.h"
#include "../lib/util/dlinklist.h"
#include "lib/events/events.h"
#include "lib/stream/packet.h"
#include "librpc/gen_ndr/ndr_nbt.h"
#include "../libcli/nbt/libnbt.h"


/*
  an event has happened on the socket
*/
static void smbcli_transport_event_handler(struct tevent_context *ev, 
					   struct tevent_fd *fde, 
					   uint16_t flags, void *private_data)
{
	struct smbcli_transport *transport = talloc_get_type(private_data,
							     struct smbcli_transport);
	if (flags & EVENT_FD_READ) {
		packet_recv(transport->packet);
		return;
	}
	if (flags & EVENT_FD_WRITE) {
		packet_queue_run(transport->packet);
	}
}

/*
  destroy a transport
 */
static int transport_destructor(struct smbcli_transport *transport)
{
	smbcli_transport_dead(transport, NT_STATUS_LOCAL_DISCONNECT);
	return 0;
}


/*
  handle receive errors
*/
static void smbcli_transport_error(void *private_data, NTSTATUS status)
{
	struct smbcli_transport *transport = talloc_get_type(private_data, struct smbcli_transport);
	smbcli_transport_dead(transport, status);
}

static NTSTATUS smbcli_transport_finish_recv(void *private_data, DATA_BLOB blob);

/*
  create a transport structure based on an established socket
*/
struct smbcli_transport *smbcli_transport_init(struct smbcli_socket *sock,
					       TALLOC_CTX *parent_ctx, 
					       bool primary, 
					       struct smbcli_options *options)
{
	struct smbcli_transport *transport;

	transport = talloc_zero(parent_ctx, struct smbcli_transport);
	if (!transport) return NULL;

	if (primary) {
		transport->socket = talloc_steal(transport, sock);
	} else {
		transport->socket = talloc_reference(transport, sock);
	}
	transport->negotiate.protocol = PROTOCOL_NT1;
	transport->options = *options;
	transport->negotiate.max_xmit = transport->options.max_xmit;

	/* setup the stream -> packet parser */
	transport->packet = packet_init(transport);
	if (transport->packet == NULL) {
		talloc_free(transport);
		return NULL;
	}
	packet_set_private(transport->packet, transport);
	packet_set_socket(transport->packet, transport->socket->sock);
	packet_set_callback(transport->packet, smbcli_transport_finish_recv);
	packet_set_full_request(transport->packet, packet_full_request_nbt);
	packet_set_error_handler(transport->packet, smbcli_transport_error);
	packet_set_event_context(transport->packet, transport->socket->event.ctx);
	packet_set_nofree(transport->packet);
	packet_set_initial_read(transport->packet, 4);

	smbcli_init_signing(transport);

	ZERO_STRUCT(transport->called);

	/* take over event handling from the socket layer - it only
	   handles events up until we are connected */
	talloc_free(transport->socket->event.fde);
	transport->socket->event.fde = event_add_fd(transport->socket->event.ctx,
						    transport->socket->sock,
						    socket_get_fd(transport->socket->sock),
						    EVENT_FD_READ,
						    smbcli_transport_event_handler,
						    transport);

	packet_set_fde(transport->packet, transport->socket->event.fde);
	packet_set_serialise(transport->packet);
	talloc_set_destructor(transport, transport_destructor);

	return transport;
}

/*
  mark the transport as dead
*/
void smbcli_transport_dead(struct smbcli_transport *transport, NTSTATUS status)
{
	smbcli_sock_dead(transport->socket);

	if (NT_STATUS_EQUAL(NT_STATUS_UNSUCCESSFUL, status)) {
		status = NT_STATUS_UNEXPECTED_NETWORK_ERROR;
	}

	/* kill only the first pending receive - this is so that if
	 that async function frees the connection we don't die trying
	 to use old memory. The caller has to cope with only one
	 network error */
	if (transport->pending_recv) {
		struct smbcli_request *req = transport->pending_recv;
		req->state = SMBCLI_REQUEST_ERROR;
		req->status = status;
		DLIST_REMOVE(transport->pending_recv, req);
		if (req->async.fn) {
			req->async.fn(req);
		}
	}
}


/*
  send a session request
*/
struct smbcli_request *smbcli_transport_connect_send(struct smbcli_transport *transport,
						     struct nbt_name *calling, 
						     struct nbt_name *called)
{
	uint8_t *p;
	struct smbcli_request *req;
	DATA_BLOB calling_blob, called_blob;
	TALLOC_CTX *tmp_ctx = talloc_new(transport);
	NTSTATUS status;

	status = nbt_name_dup(transport, called, &transport->called);
	if (!NT_STATUS_IS_OK(status)) goto failed;
	
	status = nbt_name_to_blob(tmp_ctx, &calling_blob, calling);
	if (!NT_STATUS_IS_OK(status)) goto failed;

	status = nbt_name_to_blob(tmp_ctx, &called_blob, called);
	if (!NT_STATUS_IS_OK(status)) goto failed;

  	/* allocate output buffer */
	req = smbcli_request_setup_nonsmb(transport, 
					  NBT_HDR_SIZE + 
					  calling_blob.length + called_blob.length);
	if (req == NULL) goto failed;

	/* put in the destination name */
	p = req->out.buffer + NBT_HDR_SIZE;
	memcpy(p, called_blob.data, called_blob.length);
	p += called_blob.length;

	memcpy(p, calling_blob.data, calling_blob.length);
	p += calling_blob.length;

	_smb_setlen(req->out.buffer, PTR_DIFF(p, req->out.buffer) - NBT_HDR_SIZE);
	SCVAL(req->out.buffer,0,0x81);

	if (!smbcli_request_send(req)) {
		smbcli_request_destroy(req);
		goto failed;
	}

	talloc_free(tmp_ctx);
	return req;

failed:
	talloc_free(tmp_ctx);
	return NULL;
}

/*
  map a session request error to a NTSTATUS
 */
static NTSTATUS map_session_refused_error(uint8_t error)
{
	switch (error) {
	case 0x80:
	case 0x81:
		return NT_STATUS_REMOTE_NOT_LISTENING;
	case 0x82:
		return NT_STATUS_RESOURCE_NAME_NOT_FOUND;
	case 0x83:
		return NT_STATUS_REMOTE_RESOURCES;
	}
	return NT_STATUS_UNEXPECTED_IO_ERROR;
}


/*
  finish a smbcli_transport_connect()
*/
NTSTATUS smbcli_transport_connect_recv(struct smbcli_request *req)
{
	NTSTATUS status;

	if (!smbcli_request_receive(req)) {
		smbcli_request_destroy(req);
		return NT_STATUS_UNEXPECTED_NETWORK_ERROR;
	}

	switch (CVAL(req->in.buffer,0)) {
	case 0x82:
		status = NT_STATUS_OK;
		break;
	case 0x83:
		status = map_session_refused_error(CVAL(req->in.buffer,4));
		break;
	case 0x84:
		DEBUG(1,("Warning: session retarget not supported\n"));
		status = NT_STATUS_NOT_SUPPORTED;
		break;
	default:
		status = NT_STATUS_UNEXPECTED_IO_ERROR;
		break;
	}

	smbcli_request_destroy(req);
	return status;
}


/*
  send a session request (if needed)
*/
bool smbcli_transport_connect(struct smbcli_transport *transport,
			      struct nbt_name *calling, 
			      struct nbt_name *called)
{
	struct smbcli_request *req;
	NTSTATUS status;

	if (transport->socket->port == 445) {
		return true;
	}

	req = smbcli_transport_connect_send(transport, 
					    calling, called);
	status = smbcli_transport_connect_recv(req);
	return NT_STATUS_IS_OK(status);
}

/****************************************************************************
get next mid in sequence
****************************************************************************/
uint16_t smbcli_transport_next_mid(struct smbcli_transport *transport)
{
	uint16_t mid;
	struct smbcli_request *req;

	mid = transport->next_mid;

again:
	/* now check to see if this mid is being used by one of the 
	   pending requests. This is quite efficient because the list is
	   usually very short */

	/* the zero mid is reserved for requests that don't have a mid */
	if (mid == 0) mid = 1;

	for (req=transport->pending_recv; req; req=req->next) {
		if (req->mid == mid) {
			mid++;
			goto again;
		}
	}

	transport->next_mid = mid+1;
	return mid;
}

static void idle_handler(struct tevent_context *ev, 
			 struct tevent_timer *te, struct timeval t, void *private_data)
{
	struct smbcli_transport *transport = talloc_get_type(private_data,
							     struct smbcli_transport);
	struct timeval next = timeval_add(&t, 0, transport->idle.period);
	transport->socket->event.te = event_add_timed(transport->socket->event.ctx, 
						      transport,
						      next,
						      idle_handler, transport);
	transport->idle.func(transport, transport->idle.private_data);
}

/*
  setup the idle handler for a transport
  the period is in microseconds
*/
_PUBLIC_ void smbcli_transport_idle_handler(struct smbcli_transport *transport, 
				   void (*idle_func)(struct smbcli_transport *, void *),
				   uint64_t period,
				   void *private_data)
{
	transport->idle.func = idle_func;
	transport->idle.private_data = private_data;
	transport->idle.period = period;

	if (transport->socket->event.te != NULL) {
		talloc_free(transport->socket->event.te);
	}

	transport->socket->event.te = event_add_timed(transport->socket->event.ctx, 
						      transport,
						      timeval_current_ofs(0, period),
						      idle_handler, transport);
}

/*
  we have a full request in our receive buffer - match it to a pending request
  and process
 */
static NTSTATUS smbcli_transport_finish_recv(void *private_data, DATA_BLOB blob)
{
	struct smbcli_transport *transport = talloc_get_type(private_data,
							     struct smbcli_transport);
	uint8_t *buffer, *hdr, *vwv;
	int len;
	uint16_t wct=0, mid = 0, op = 0;
	struct smbcli_request *req = NULL;

	buffer = blob.data;
	len = blob.length;

	hdr = buffer+NBT_HDR_SIZE;
	vwv = hdr + HDR_VWV;

	/* see if it could be an oplock break request */
	if (smbcli_handle_oplock_break(transport, len, hdr, vwv)) {
		talloc_free(buffer);
		return NT_STATUS_OK;
	}

	/* at this point we need to check for a readbraw reply, as
	   these can be any length */
	if (transport->readbraw_pending) {
		transport->readbraw_pending = 0;

		/* it must match the first entry in the pending queue
		   as the client is not allowed to have outstanding
		   readbraw requests */
		req = transport->pending_recv;
		if (!req) goto error;

		req->in.buffer = buffer;
		talloc_steal(req, buffer);
		req->in.size = len;
		req->in.allocated = req->in.size;
		goto async;
	}

	if (len >= MIN_SMB_SIZE) {
		/* extract the mid for matching to pending requests */
		mid = SVAL(hdr, HDR_MID);
		wct = CVAL(hdr, HDR_WCT);
		op  = CVAL(hdr, HDR_COM);
	}

	/* match the incoming request against the list of pending requests */
	for (req=transport->pending_recv; req; req=req->next) {
		if (req->mid == mid) break;
	}

	/* see if it's a ntcancel reply for the current MID */
	req = smbcli_handle_ntcancel_reply(req, len, hdr);

	if (!req) {
		DEBUG(1,("Discarding unmatched reply with mid %d op %d\n", mid, op));
		goto error;
	}

	/* fill in the 'in' portion of the matching request */
	req->in.buffer = buffer;
	talloc_steal(req, buffer);
	req->in.size = len;
	req->in.allocated = req->in.size;

	/* handle NBT session replies */
	if (req->in.size >= 4 && req->in.buffer[0] != 0) {
		req->status = NT_STATUS_OK;
		goto async;
	}

	/* handle non-SMB replies */
	if (req->in.size < NBT_HDR_SIZE + MIN_SMB_SIZE) {
		req->state = SMBCLI_REQUEST_ERROR;
		goto error;
	}

	if (req->in.size < NBT_HDR_SIZE + MIN_SMB_SIZE + VWV(wct)) {
		DEBUG(2,("bad reply size for mid %d\n", mid));
		req->status = NT_STATUS_UNSUCCESSFUL;
		req->state = SMBCLI_REQUEST_ERROR;
		goto error;
	}

	req->in.hdr = hdr;
	req->in.vwv = vwv;
	req->in.wct = wct;
	if (req->in.size >= NBT_HDR_SIZE + MIN_SMB_SIZE + VWV(wct)) {
		req->in.data = req->in.vwv + VWV(wct) + 2;
		req->in.data_size = SVAL(req->in.vwv, VWV(wct));
		if (req->in.size < NBT_HDR_SIZE + MIN_SMB_SIZE + VWV(wct) + req->in.data_size) {
			DEBUG(3,("bad data size for mid %d\n", mid));
			/* blergh - w2k3 gives a bogus data size values in some
			   openX replies */
			req->in.data_size = req->in.size - (NBT_HDR_SIZE + MIN_SMB_SIZE + VWV(wct));
		}
	}
	req->in.ptr = req->in.data;
	req->flags2 = SVAL(req->in.hdr, HDR_FLG2);

	smb_setup_bufinfo(req);

	if (!(req->flags2 & FLAGS2_32_BIT_ERROR_CODES)) {
		int eclass = CVAL(req->in.hdr,HDR_RCLS);
		int code = SVAL(req->in.hdr,HDR_ERR);
		if (eclass == 0 && code == 0) {
			transport->error.e.nt_status = NT_STATUS_OK;
		} else {
			transport->error.e.nt_status = NT_STATUS_DOS(eclass, code);
		}
	} else {
		transport->error.e.nt_status = NT_STATUS(IVAL(req->in.hdr, HDR_RCLS));
	}

	req->status = transport->error.e.nt_status;
	if (NT_STATUS_IS_OK(req->status)) {
		transport->error.etype = ETYPE_NONE;
	} else {
		transport->error.etype = ETYPE_SMB;
	}

	if (!smbcli_request_check_sign_mac(req)) {
		transport->error.etype = ETYPE_SOCKET;
		transport->error.e.socket_error = SOCKET_READ_BAD_SIG;
		req->state = SMBCLI_REQUEST_ERROR;
		req->status = NT_STATUS_ACCESS_DENIED;
		goto error;
	};

async:
	/* if this request has an async handler then call that to
	   notify that the reply has been received. This might destroy
	   the request so it must happen last */

	req->state = SMBCLI_REQUEST_DONE;

	if (req->recv_helper.fn) {
		/*
		 * let the recv helper decide in
		 * what state the request really is
		 */
		req->state = req->recv_helper.fn(req);

		/* if more parts are needed, wait for them */
		if (req->state <= SMBCLI_REQUEST_RECV) {
			return NT_STATUS_OK;
		}
	}
	DLIST_REMOVE(transport->pending_recv, req);
	if (req->async.fn) {
		req->async.fn(req);
	}
	return NT_STATUS_OK;

error:
	if (req) {
		DLIST_REMOVE(transport->pending_recv, req);
		req->state = SMBCLI_REQUEST_ERROR;
		if (req->async.fn) {
			req->async.fn(req);
		}
	} else {
		talloc_free(buffer);
	}
	return NT_STATUS_OK;
}

/*
  process some read/write requests that are pending
  return false if the socket is dead
*/
_PUBLIC_ bool smbcli_transport_process(struct smbcli_transport *transport)
{
	NTSTATUS status;
	size_t npending;

	packet_queue_run(transport->packet);
	if (transport->socket->sock == NULL) {
		return false;
	}

	status = socket_pending(transport->socket->sock, &npending);
	if (NT_STATUS_IS_OK(status) && npending > 0) {
		packet_recv(transport->packet);
	}
	if (transport->socket->sock == NULL) {
		return false;
	}
	return true;
}

/*
  handle timeouts of individual smb requests
*/
static void smbcli_timeout_handler(struct tevent_context *ev, struct tevent_timer *te, 
				   struct timeval t, void *private_data)
{
	struct smbcli_request *req = talloc_get_type(private_data, struct smbcli_request);

	if (req->state == SMBCLI_REQUEST_RECV) {
		DLIST_REMOVE(req->transport->pending_recv, req);
	}
	req->status = NT_STATUS_IO_TIMEOUT;
	req->state = SMBCLI_REQUEST_ERROR;
	if (req->async.fn) {
		req->async.fn(req);
	}
}


/*
  destroy a request
*/
static int smbcli_request_destructor(struct smbcli_request *req)
{
	if (req->state == SMBCLI_REQUEST_RECV) {
		DLIST_REMOVE(req->transport->pending_recv, req);
	}
	return 0;
}


/*
  put a request into the send queue
*/
void smbcli_transport_send(struct smbcli_request *req)
{
	DATA_BLOB blob;
	NTSTATUS status;

	/* check if the transport is dead */
	if (req->transport->socket->sock == NULL) {
		req->state = SMBCLI_REQUEST_ERROR;
		req->status = NT_STATUS_NET_WRITE_FAULT;
		return;
	}

	blob = data_blob_const(req->out.buffer, req->out.size);
	status = packet_send(req->transport->packet, blob);
	if (!NT_STATUS_IS_OK(status)) {
		req->state = SMBCLI_REQUEST_ERROR;
		req->status = status;
		return;
	}

	packet_queue_run(req->transport->packet);
	if (req->transport->socket->sock == NULL) {
		req->state = SMBCLI_REQUEST_ERROR;
		req->status = NT_STATUS_NET_WRITE_FAULT;
		return;
	}

	if (req->one_way_request) {
		req->state = SMBCLI_REQUEST_DONE;
		smbcli_request_destroy(req);
		return;
	}

	req->state = SMBCLI_REQUEST_RECV;
	DLIST_ADD(req->transport->pending_recv, req);

	/* add a timeout */
	if (req->transport->options.request_timeout) {
		event_add_timed(req->transport->socket->event.ctx, req, 
				timeval_current_ofs(req->transport->options.request_timeout, 0), 
				smbcli_timeout_handler, req);
	}

	talloc_set_destructor(req, smbcli_request_destructor);
}


/****************************************************************************
 Send an SMBecho (async send)
*****************************************************************************/
_PUBLIC_ struct smbcli_request *smb_raw_echo_send(struct smbcli_transport *transport,
					 struct smb_echo *p)
{
	struct smbcli_request *req;

	req = smbcli_request_setup_transport(transport, SMBecho, 1, p->in.size);
	if (!req) return NULL;

	SSVAL(req->out.vwv, VWV(0), p->in.repeat_count);

	memcpy(req->out.data, p->in.data, p->in.size);

	ZERO_STRUCT(p->out);

	if (!smbcli_request_send(req)) {
		smbcli_request_destroy(req);
		return NULL;
	}

	return req;
}

/****************************************************************************
 raw echo interface (async recv)
****************************************************************************/
NTSTATUS smb_raw_echo_recv(struct smbcli_request *req, TALLOC_CTX *mem_ctx,
			   struct smb_echo *p)
{
	if (!smbcli_request_receive(req) ||
	    smbcli_request_is_error(req)) {
		goto failed;
	}

	SMBCLI_CHECK_WCT(req, 1);
	p->out.count++;
	p->out.sequence_number = SVAL(req->in.vwv, VWV(0));
	p->out.size = req->in.data_size;
	talloc_free(p->out.data);
	p->out.data = talloc_array(mem_ctx, uint8_t, p->out.size);
	NT_STATUS_HAVE_NO_MEMORY(p->out.data);

	if (!smbcli_raw_pull_data(&req->in.bufinfo, req->in.data, p->out.size, p->out.data)) {
		req->status = NT_STATUS_BUFFER_TOO_SMALL;
	}

	if (p->out.count == p->in.repeat_count) {
		return smbcli_request_destroy(req);
	}

	return NT_STATUS_OK;

failed:
	return smbcli_request_destroy(req);
}

/****************************************************************************
 Send a echo (sync interface)
*****************************************************************************/
NTSTATUS smb_raw_echo(struct smbcli_transport *transport, struct smb_echo *p)
{
	struct smbcli_request *req = smb_raw_echo_send(transport, p);
	return smbcli_request_simple_recv(req);
}
