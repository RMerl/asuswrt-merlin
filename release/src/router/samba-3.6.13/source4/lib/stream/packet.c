/* 
   Unix SMB/CIFS mplementation.

   helper layer for breaking up streams into discrete requests
   
   Copyright (C) Andrew Tridgell  2005
    
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
#include "../lib/util/dlinklist.h"
#include "lib/events/events.h"
#include "lib/socket/socket.h"
#include "lib/stream/packet.h"
#include "libcli/raw/smb.h"

struct packet_context {
	packet_callback_fn_t callback;
	packet_full_request_fn_t full_request;
	packet_error_handler_fn_t error_handler;
	DATA_BLOB partial;
	uint32_t num_read;
	uint32_t initial_read;
	struct socket_context *sock;
	struct tevent_context *ev;
	size_t packet_size;
	void *private_data;
	struct tevent_fd *fde;
	bool serialise;
	int processing;
	bool recv_disable;
	bool recv_need_enable;
	bool nofree;

	bool busy;
	bool destructor_called;

	bool unreliable_select;

	struct send_element {
		struct send_element *next, *prev;
		DATA_BLOB blob;
		size_t nsent;
		packet_send_callback_fn_t send_callback;
		void *send_callback_private;
	} *send_queue;
};

/*
  a destructor used when we are processing packets to prevent freeing of this
  context while it is being used
*/
static int packet_destructor(struct packet_context *pc)
{
	if (pc->busy) {
		pc->destructor_called = true;
		/* now we refuse the talloc_free() request. The free will
		   happen again in the packet_recv() code */
		return -1;
	}

	return 0;
}


/*
  initialise a packet receiver
*/
_PUBLIC_ struct packet_context *packet_init(TALLOC_CTX *mem_ctx)
{
	struct packet_context *pc = talloc_zero(mem_ctx, struct packet_context);
	if (pc != NULL) {
		talloc_set_destructor(pc, packet_destructor);
	}
	return pc;
}


/*
  set the request callback, called when a full request is ready
*/
_PUBLIC_ void packet_set_callback(struct packet_context *pc, packet_callback_fn_t callback)
{
	pc->callback = callback;
}

/*
  set the error handler
*/
_PUBLIC_ void packet_set_error_handler(struct packet_context *pc, packet_error_handler_fn_t handler)
{
	pc->error_handler = handler;
}

/*
  set the private pointer passed to the callback functions
*/
_PUBLIC_ void packet_set_private(struct packet_context *pc, void *private_data)
{
	pc->private_data = private_data;
}

/*
  set the full request callback. Should return as follows:
     NT_STATUS_OK == blob is a full request.
     STATUS_MORE_ENTRIES == blob is not complete yet
     any error == blob is not a valid 
*/
_PUBLIC_ void packet_set_full_request(struct packet_context *pc, packet_full_request_fn_t callback)
{
	pc->full_request = callback;
}

/*
  set a socket context to use. You must set a socket_context
*/
_PUBLIC_ void packet_set_socket(struct packet_context *pc, struct socket_context *sock)
{
	pc->sock = sock;
}

/*
  set an event context. If this is set then the code will ensure that
  packets arrive with separate events, by creating a immediate event
  for any secondary packets when more than one packet is read at one
  time on a socket. This can matter for code that relies on not
  getting more than one packet per event
*/
_PUBLIC_ void packet_set_event_context(struct packet_context *pc, struct tevent_context *ev)
{
	pc->ev = ev;
}

/*
  tell the packet layer the fde for the socket
*/
_PUBLIC_ void packet_set_fde(struct packet_context *pc, struct tevent_fd *fde)
{
	pc->fde = fde;
}

/*
  tell the packet layer to serialise requests, so we don't process two
  requests at once on one connection. You must have set the
  event_context and fde
*/
_PUBLIC_ void packet_set_serialise(struct packet_context *pc)
{
	pc->serialise = true;
}

/*
  tell the packet layer how much to read when starting a new packet
  this ensures it doesn't overread
*/
_PUBLIC_ void packet_set_initial_read(struct packet_context *pc, uint32_t initial_read)
{
	pc->initial_read = initial_read;
}

/*
  tell the packet system not to steal/free blobs given to packet_send()
*/
_PUBLIC_ void packet_set_nofree(struct packet_context *pc)
{
	pc->nofree = true;
}

/*
  tell the packet system that select/poll/epoll on the underlying
  socket may not be a reliable way to determine if data is available
  for receive. This happens with underlying socket systems such as the
  one implemented on top of GNUTLS, where there may be data in
  encryption/compression buffers that could be received by
  socket_recv(), while there is no data waiting at the real socket
  level as seen by select/poll/epoll. The GNUTLS library is supposed
  to cope with this by always leaving some data sitting in the socket
  buffer, but it does not seem to be reliable.
 */
_PUBLIC_ void packet_set_unreliable_select(struct packet_context *pc)
{
	pc->unreliable_select = true;
}

/*
  tell the caller we have an error
*/
static void packet_error(struct packet_context *pc, NTSTATUS status)
{
	pc->sock = NULL;
	if (pc->error_handler) {
		pc->error_handler(pc->private_data, status);
		return;
	}
	/* default error handler is to free the callers private pointer */
	if (!NT_STATUS_EQUAL(status, NT_STATUS_END_OF_FILE)) {
		DEBUG(0,("packet_error on %s - %s\n", 
			 talloc_get_name(pc->private_data), nt_errstr(status)));
	}
	talloc_free(pc->private_data);
	return;
}


/*
  tell the caller we have EOF
*/
static void packet_eof(struct packet_context *pc)
{
	packet_error(pc, NT_STATUS_END_OF_FILE);
}


/*
  used to put packets on event boundaries
*/
static void packet_next_event(struct tevent_context *ev, struct tevent_timer *te, 
			      struct timeval t, void *private_data)
{
	struct packet_context *pc = talloc_get_type(private_data, struct packet_context);
	if (pc->num_read != 0 && pc->packet_size != 0 &&
	    pc->packet_size <= pc->num_read) {
		packet_recv(pc);
	}
}


/*
  call this when the socket becomes readable to kick off the whole
  stream parsing process
*/
_PUBLIC_ void packet_recv(struct packet_context *pc)
{
	size_t npending;
	NTSTATUS status;
	size_t nread = 0;
	DATA_BLOB blob;
	bool recv_retry = false;

	if (pc->processing) {
		EVENT_FD_NOT_READABLE(pc->fde);
		pc->processing++;
		return;
	}

	if (pc->recv_disable) {
		pc->recv_need_enable = true;
		EVENT_FD_NOT_READABLE(pc->fde);
		return;
	}

	if (pc->packet_size != 0 && pc->num_read >= pc->packet_size) {
		goto next_partial;
	}

	if (pc->packet_size != 0) {
		/* we've already worked out how long this next packet is, so skip the
		   socket_pending() call */
		npending = pc->packet_size - pc->num_read;
	} else if (pc->initial_read != 0) {
		npending = pc->initial_read - pc->num_read;
	} else {
		if (pc->sock) {
			status = socket_pending(pc->sock, &npending);
		} else {
			status = NT_STATUS_CONNECTION_DISCONNECTED;
		}
		if (!NT_STATUS_IS_OK(status)) {
			packet_error(pc, status);
			return;
		}
	}

	if (npending == 0) {
		packet_eof(pc);
		return;
	}

again:

	if (npending + pc->num_read < npending) {
		packet_error(pc, NT_STATUS_INVALID_PARAMETER);
		return;
	}

	if (npending + pc->num_read < pc->num_read) {
		packet_error(pc, NT_STATUS_INVALID_PARAMETER);
		return;
	}

	/* possibly expand the partial packet buffer */
	if (npending + pc->num_read > pc->partial.length) {
		if (!data_blob_realloc(pc, &pc->partial, npending+pc->num_read)) {
			packet_error(pc, NT_STATUS_NO_MEMORY);
			return;
		}
	}

	if (pc->partial.length < pc->num_read + npending) {
		packet_error(pc, NT_STATUS_INVALID_PARAMETER);
		return;
	}

	if ((uint8_t *)pc->partial.data + pc->num_read < (uint8_t *)pc->partial.data) {
		packet_error(pc, NT_STATUS_INVALID_PARAMETER);
		return;
	}
	if ((uint8_t *)pc->partial.data + pc->num_read + npending < (uint8_t *)pc->partial.data) {
		packet_error(pc, NT_STATUS_INVALID_PARAMETER);
		return;
	}

	status = socket_recv(pc->sock, pc->partial.data + pc->num_read, 
			     npending, &nread);

	if (NT_STATUS_IS_ERR(status)) {
		packet_error(pc, status);
		return;
	}
	if (recv_retry && NT_STATUS_EQUAL(status, STATUS_MORE_ENTRIES)) {
		nread = 0;
		status = NT_STATUS_OK;
	}
	if (!NT_STATUS_IS_OK(status)) {
		return;
	}

	if (nread == 0 && !recv_retry) {
		packet_eof(pc);
		return;
	}

	pc->num_read += nread;

	if (pc->unreliable_select && nread != 0) {
		recv_retry = true;
		status = socket_pending(pc->sock, &npending);
		if (!NT_STATUS_IS_OK(status)) {
			packet_error(pc, status);
			return;
		}
		if (npending != 0) {
			goto again;
		}
	}

next_partial:
	if (pc->partial.length != pc->num_read) {
		if (!data_blob_realloc(pc, &pc->partial, pc->num_read)) {
			packet_error(pc, NT_STATUS_NO_MEMORY);
			return;
		}
	}

	/* see if its a full request */
	blob = pc->partial;
	blob.length = pc->num_read;
	status = pc->full_request(pc->private_data, blob, &pc->packet_size);
	if (NT_STATUS_IS_ERR(status)) {
		packet_error(pc, status);
		return;
	}
	if (!NT_STATUS_IS_OK(status)) {
		return;
	}

	if (pc->packet_size > pc->num_read) {
		/* the caller made an error */
		DEBUG(0,("Invalid packet_size %lu greater than num_read %lu\n",
			 (long)pc->packet_size, (long)pc->num_read));
		packet_error(pc, NT_STATUS_INVALID_PARAMETER);
		return;
	}

	/* it is a full request - give it to the caller */
	blob = pc->partial;
	blob.length = pc->num_read;

	if (pc->packet_size < pc->num_read) {
		pc->partial = data_blob_talloc(pc, blob.data + pc->packet_size, 
					       pc->num_read - pc->packet_size);
		if (pc->partial.data == NULL) {
			packet_error(pc, NT_STATUS_NO_MEMORY);
			return;
		}
		/* Trunate the blob sent to the caller to only the packet length */
		if (!data_blob_realloc(pc, &blob, pc->packet_size)) {
			packet_error(pc, NT_STATUS_NO_MEMORY);
			return;
		}
	} else {
		pc->partial = data_blob(NULL, 0);
	}
	pc->num_read -= pc->packet_size;
	pc->packet_size = 0;
	
	if (pc->serialise) {
		pc->processing = 1;
	}

	pc->busy = true;

	status = pc->callback(pc->private_data, blob);

	pc->busy = false;

	if (pc->destructor_called) {
		talloc_free(pc);
		return;
	}

	if (pc->processing) {
		if (pc->processing > 1) {
			EVENT_FD_READABLE(pc->fde);
		}
		pc->processing = 0;
	}

	if (!NT_STATUS_IS_OK(status)) {
		packet_error(pc, status);
		return;
	}

	/* Have we consumed the whole buffer yet? */
	if (pc->partial.length == 0) {
		return;
	}

	/* we got multiple packets in one tcp read */
	if (pc->ev == NULL) {
		goto next_partial;
	}

	blob = pc->partial;
	blob.length = pc->num_read;

	status = pc->full_request(pc->private_data, blob, &pc->packet_size);
	if (NT_STATUS_IS_ERR(status)) {
		packet_error(pc, status);
		return;
	}

	if (!NT_STATUS_IS_OK(status)) {
		return;
	}

	event_add_timed(pc->ev, pc, timeval_zero(), packet_next_event, pc);
}


/*
  temporarily disable receiving 
*/
_PUBLIC_ void packet_recv_disable(struct packet_context *pc)
{
	pc->recv_disable = true;
}

/*
  re-enable receiving 
*/
_PUBLIC_ void packet_recv_enable(struct packet_context *pc)
{
	if (pc->recv_need_enable) {
		pc->recv_need_enable = false;
		EVENT_FD_READABLE(pc->fde);
	}
	pc->recv_disable = false;
	if (pc->num_read != 0 && pc->packet_size >= pc->num_read) {
		event_add_timed(pc->ev, pc, timeval_zero(), packet_next_event, pc);
	}
}

/*
  trigger a run of the send queue
*/
_PUBLIC_ void packet_queue_run(struct packet_context *pc)
{
	while (pc->send_queue) {
		struct send_element *el = pc->send_queue;
		NTSTATUS status;
		size_t nwritten;
		DATA_BLOB blob = data_blob_const(el->blob.data + el->nsent,
						 el->blob.length - el->nsent);

		status = socket_send(pc->sock, &blob, &nwritten);

		if (NT_STATUS_IS_ERR(status)) {
			packet_error(pc, status);
			return;
		}
		if (!NT_STATUS_IS_OK(status)) {
			return;
		}
		el->nsent += nwritten;
		if (el->nsent == el->blob.length) {
			DLIST_REMOVE(pc->send_queue, el);
			if (el->send_callback) {
				pc->busy = true;
				el->send_callback(el->send_callback_private);
				pc->busy = false;
				if (pc->destructor_called) {
					talloc_free(pc);
					return;
				}
			}
			talloc_free(el);
		}
	}

	/* we're out of requests to send, so don't wait for write
	   events any more */
	EVENT_FD_NOT_WRITEABLE(pc->fde);
}

/*
  put a packet in the send queue.  When the packet is actually sent,
  call send_callback.  

  Useful for operations that must occur after sending a message, such
  as the switch to SASL encryption after as sucessful LDAP bind relpy.
*/
_PUBLIC_ NTSTATUS packet_send_callback(struct packet_context *pc, DATA_BLOB blob,
				       packet_send_callback_fn_t send_callback, 
				       void *private_data)
{
	struct send_element *el;
	el = talloc(pc, struct send_element);
	NT_STATUS_HAVE_NO_MEMORY(el);

	DLIST_ADD_END(pc->send_queue, el, struct send_element *);
	el->blob = blob;
	el->nsent = 0;
	el->send_callback = send_callback;
	el->send_callback_private = private_data;

	/* if we aren't going to free the packet then we must reference it
	   to ensure it doesn't disappear before going out */
	if (pc->nofree) {
		if (!talloc_reference(el, blob.data)) {
			return NT_STATUS_NO_MEMORY;
		}
	} else {
		talloc_steal(el, blob.data);
	}

	if (private_data && !talloc_reference(el, private_data)) {
		return NT_STATUS_NO_MEMORY;
	}

	EVENT_FD_WRITEABLE(pc->fde);

	return NT_STATUS_OK;
}

/*
  put a packet in the send queue
*/
_PUBLIC_ NTSTATUS packet_send(struct packet_context *pc, DATA_BLOB blob)
{
	return packet_send_callback(pc, blob, NULL, NULL);
}


/*
  a full request checker for NBT formatted packets (first 3 bytes are length)
*/
_PUBLIC_ NTSTATUS packet_full_request_nbt(void *private_data, DATA_BLOB blob, size_t *size)
{
	if (blob.length < 4) {
		return STATUS_MORE_ENTRIES;
	}
	*size = 4 + smb_len(blob.data);
	if (*size > blob.length) {
		return STATUS_MORE_ENTRIES;
	}
	return NT_STATUS_OK;
}


/*
  work out if a packet is complete for protocols that use a 32 bit network byte
  order length
*/
_PUBLIC_ NTSTATUS packet_full_request_u32(void *private_data, DATA_BLOB blob, size_t *size)
{
	if (blob.length < 4) {
		return STATUS_MORE_ENTRIES;
	}
	*size = 4 + RIVAL(blob.data, 0);
	if (*size > blob.length) {
		return STATUS_MORE_ENTRIES;
	}
	return NT_STATUS_OK;
}
