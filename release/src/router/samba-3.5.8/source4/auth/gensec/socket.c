/* 
   Unix SMB/CIFS implementation.

   GENSEC socket interface

   Copyright (C) Andrew Bartlett 2006
 
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
#include "lib/socket/socket.h"
#include "lib/stream/packet.h"
#include "auth/gensec/gensec.h"
#include "auth/gensec/gensec_proto.h"

static const struct socket_ops gensec_socket_ops;

struct gensec_socket {
	struct gensec_security *gensec_security;
	struct socket_context *socket;
	struct tevent_context *ev;
	struct packet_context *packet;
	DATA_BLOB read_buffer;  /* SASL packets are turned into liniarlised data here, for reading */
	size_t orig_send_len;
	bool eof;
	NTSTATUS error;
	bool interrupted;
	void (*recv_handler)(void *, uint16_t);
	void *recv_private;
	int in_extra_read;
	bool wrap; /* Should we be wrapping on this socket at all? */
};

static NTSTATUS gensec_socket_init_fn(struct socket_context *sock)
{
	switch (sock->type) {
	case SOCKET_TYPE_STREAM:
		break;
	default:
		return NT_STATUS_INVALID_PARAMETER;
	}

	sock->backend_name = "gensec";

	return NT_STATUS_OK;
}

/* These functions are for use here only (public because SPNEGO must
 * use them for recursion) */
_PUBLIC_ NTSTATUS gensec_wrap_packets(struct gensec_security *gensec_security, 
			     TALLOC_CTX *mem_ctx, 
			     const DATA_BLOB *in, 
			     DATA_BLOB *out,
			     size_t *len_processed) 
{
	if (!gensec_security->ops->wrap_packets) {
		NTSTATUS nt_status;
		size_t max_input_size;
		DATA_BLOB unwrapped, wrapped;
		max_input_size = gensec_max_input_size(gensec_security);
		unwrapped = data_blob_const(in->data, MIN(max_input_size, (size_t)in->length));
		
		nt_status = gensec_wrap(gensec_security, 
					mem_ctx,
					&unwrapped, &wrapped);
		if (!NT_STATUS_IS_OK(nt_status)) {
			talloc_free(mem_ctx);
			return nt_status;
		}
		
		*out = data_blob_talloc(mem_ctx, NULL, 4);
		if (!out->data) {
			return NT_STATUS_NO_MEMORY;
		}
		RSIVAL(out->data, 0, wrapped.length);
		
		if (!data_blob_append(mem_ctx, out, wrapped.data, wrapped.length)) {
			return NT_STATUS_NO_MEMORY;
		}
		*len_processed = unwrapped.length;
		return NT_STATUS_OK;
	}
	return gensec_security->ops->wrap_packets(gensec_security, mem_ctx, in, out,
						  len_processed);
}

/* These functions are for use here only (public because SPNEGO must
 * use them for recursion) */
NTSTATUS gensec_unwrap_packets(struct gensec_security *gensec_security, 
					TALLOC_CTX *mem_ctx, 
					const DATA_BLOB *in, 
					DATA_BLOB *out,
					size_t *len_processed) 
{
	if (!gensec_security->ops->unwrap_packets) {
		DATA_BLOB wrapped;
		NTSTATUS nt_status;
		size_t packet_size;
		if (in->length < 4) {
			/* Missing the header we already had! */
			DEBUG(0, ("Asked to unwrap packet of bogus length!  How did we get the short packet?!\n"));
			return NT_STATUS_INVALID_PARAMETER;
		}
		
		packet_size = RIVAL(in->data, 0);
		
		wrapped = data_blob_const(in->data + 4, packet_size);
		
		if (wrapped.length > (in->length - 4)) {
			DEBUG(0, ("Asked to unwrap packed of bogus length %d > %d!  How did we get this?!\n",
				  (int)wrapped.length, (int)(in->length - 4)));
			return NT_STATUS_INTERNAL_ERROR;
		}
		
		nt_status = gensec_unwrap(gensec_security, 
					  mem_ctx,
					  &wrapped, out);
		if (!NT_STATUS_IS_OK(nt_status)) {
			return nt_status;
		}
		
		*len_processed = packet_size + 4;
		return nt_status;
	}
	return gensec_security->ops->unwrap_packets(gensec_security, mem_ctx, in, out,
						    len_processed);
}

/* These functions are for use here only (public because SPNEGO must
 * use them for recursion) */
NTSTATUS gensec_packet_full_request(struct gensec_security *gensec_security,
				    DATA_BLOB blob, size_t *size) 
{
	if (gensec_security->ops->packet_full_request) {
		return gensec_security->ops->packet_full_request(gensec_security,
								 blob, size);
	}
	if (gensec_security->ops->unwrap_packets) {
		if (blob.length) {
			*size = blob.length;
			return NT_STATUS_OK;
		}
		return STATUS_MORE_ENTRIES;
	}
	return packet_full_request_u32(NULL, blob, size);
}

static NTSTATUS gensec_socket_full_request(void *private_data, DATA_BLOB blob, size_t *size)
{
	struct gensec_socket *gensec_socket = talloc_get_type(private_data, struct gensec_socket);
	struct gensec_security *gensec_security = gensec_socket->gensec_security;
	return gensec_packet_full_request(gensec_security, blob, size);
}

/* Try to figure out how much data is waiting to be read */
static NTSTATUS gensec_socket_pending(struct socket_context *sock, size_t *npending) 
{
	struct gensec_socket *gensec_socket = talloc_get_type(sock->private_data, struct gensec_socket);
	if (!gensec_socket->wrap) {
		return socket_pending(gensec_socket->socket, npending);
	}

	if (gensec_socket->read_buffer.length > 0) {
		*npending = gensec_socket->read_buffer.length;
		return NT_STATUS_OK;
	}

	/* This is a lie.  We hope the decrypted data will always be
	 * less than this value, so the application just gets a short
	 * read.  Without reading and decrypting it, we can't tell.
	 * If the SASL mech does compression, then we just need to
	 * manually trigger read events */
	return socket_pending(gensec_socket->socket, npending);
}      

/* Note if an error occours, so we can return it up the stack */
static void gensec_socket_error_handler(void *private_data, NTSTATUS status)
{
	struct gensec_socket *gensec_socket = talloc_get_type(private_data, struct gensec_socket);
	if (NT_STATUS_EQUAL(status, NT_STATUS_END_OF_FILE)) {
		gensec_socket->eof = true;
	} else {
		gensec_socket->error = status;
	}
}

static void gensec_socket_trigger_read(struct tevent_context *ev, 
				       struct tevent_timer *te, 
				       struct timeval t, void *private_data)
{
	struct gensec_socket *gensec_socket = talloc_get_type(private_data, struct gensec_socket);

	gensec_socket->in_extra_read++;
	gensec_socket->recv_handler(gensec_socket->recv_private, EVENT_FD_READ);
	gensec_socket->in_extra_read--;

	/* It may well be that, having run the recv handler, we still
	 * have even more data waiting for us! 
	 */
	if (gensec_socket->read_buffer.length && gensec_socket->recv_handler) {
		/* Schedule this funcion to run again */
		event_add_timed(gensec_socket->ev, gensec_socket, timeval_zero(), 
				gensec_socket_trigger_read, gensec_socket);
	}
}

/* These two routines could be changed to use a circular buffer of
 * some kind, or linked lists, or ... */
static NTSTATUS gensec_socket_recv(struct socket_context *sock, void *buf,
				   size_t wantlen, size_t *nread) 
{
	struct gensec_socket *gensec_socket = talloc_get_type(sock->private_data, struct gensec_socket);

	if (!gensec_socket->wrap) {
		return socket_recv(gensec_socket->socket, buf, wantlen, nread);
	}

	gensec_socket->error = NT_STATUS_OK;

	if (gensec_socket->read_buffer.length == 0) {
		/* Process any data on the socket, into the read buffer. At
		 * this point, the socket is not available for read any
		 * longer */
		packet_recv(gensec_socket->packet);

		if (gensec_socket->eof) {
			*nread = 0;
			return NT_STATUS_OK;
		}
		
		if (!NT_STATUS_IS_OK(gensec_socket->error)) {
			return gensec_socket->error;
		}

		if (gensec_socket->read_buffer.length == 0) {
			/* Clearly we don't have the entire SASL packet yet,
			 * so it has not been written into the buffer */
			*nread = 0;
			return STATUS_MORE_ENTRIES;
		}
	}


	*nread = MIN(wantlen, gensec_socket->read_buffer.length);
	memcpy(buf, gensec_socket->read_buffer.data, *nread);

	if (gensec_socket->read_buffer.length > *nread) {
		memmove(gensec_socket->read_buffer.data, 
			gensec_socket->read_buffer.data + *nread, 
			gensec_socket->read_buffer.length - *nread);
	}

	gensec_socket->read_buffer.length -= *nread;
	gensec_socket->read_buffer.data = talloc_realloc(gensec_socket, 
							 gensec_socket->read_buffer.data, 
							 uint8_t, 
							 gensec_socket->read_buffer.length);

	if (gensec_socket->read_buffer.length && 
	    gensec_socket->in_extra_read == 0 && 
	    gensec_socket->recv_handler) {
		/* Manually call a read event, to get this moving
		 * again (as the socket should be dry, so the normal
		 * event handler won't trigger) */
		event_add_timed(gensec_socket->ev, gensec_socket, timeval_zero(), 
				gensec_socket_trigger_read, gensec_socket);
	}

	return NT_STATUS_OK;
}

/* Completed SASL packet callback.  When we have a 'whole' SASL
 * packet, decrypt it, and add it to the read buffer
 *
 * This function (and anything under it) MUST NOT call the event system
 */
static NTSTATUS gensec_socket_unwrap(void *private_data, DATA_BLOB blob)
{
	struct gensec_socket *gensec_socket = talloc_get_type(private_data, struct gensec_socket);
	DATA_BLOB unwrapped;
	NTSTATUS nt_status;
	TALLOC_CTX *mem_ctx;
	size_t packet_size;

	mem_ctx = talloc_new(gensec_socket);
	if (!mem_ctx) {
		return NT_STATUS_NO_MEMORY;
	}
	nt_status = gensec_unwrap_packets(gensec_socket->gensec_security, 
					  mem_ctx,
					  &blob, &unwrapped, 
					  &packet_size);
	if (!NT_STATUS_IS_OK(nt_status)) {
		talloc_free(mem_ctx);
		return nt_status;
	}

	if (packet_size != blob.length) {
		DEBUG(0, ("gensec_socket_unwrap: Did not consume entire packet!\n"));
		talloc_free(mem_ctx);
		return NT_STATUS_INTERNAL_ERROR;
	}

	/* We could change this into a linked list, and have
	 * gensec_socket_recv() and gensec_socket_pending() walk the
	 * linked list */

	if (!data_blob_append(gensec_socket, &gensec_socket->read_buffer, 
				     unwrapped.data, unwrapped.length)) {
		talloc_free(mem_ctx);
		return NT_STATUS_NO_MEMORY;
	}

	talloc_free(mem_ctx);
	return NT_STATUS_OK;
}

/* when the data is sent, we know we have not been interrupted */
static void send_callback(void *private_data)
{
	struct gensec_socket *gensec_socket = talloc_get_type(private_data, struct gensec_socket);
	gensec_socket->interrupted = false;
}

/*
  send data, but only as much as we allow in one packet.  

  If this returns STATUS_MORE_ENTRIES, the caller must retry with
  exactly the same data, or a NULL blob.
*/
static NTSTATUS gensec_socket_send(struct socket_context *sock, 
				   const DATA_BLOB *blob, size_t *sendlen)
{
	NTSTATUS nt_status;
	struct gensec_socket *gensec_socket = talloc_get_type(sock->private_data, struct gensec_socket);
	DATA_BLOB wrapped;
	TALLOC_CTX *mem_ctx;

	if (!gensec_socket->wrap) {
		return socket_send(gensec_socket->socket, blob, sendlen);
	}

	*sendlen = 0;

	/* We have have been interupted, so the caller should be
	 * giving us the same data again.  */
	if (gensec_socket->interrupted) {
		packet_queue_run(gensec_socket->packet);

		if (!NT_STATUS_IS_OK(gensec_socket->error)) {
			return gensec_socket->error;
		} else if (gensec_socket->interrupted) {
			return STATUS_MORE_ENTRIES;
		} else {
			*sendlen = gensec_socket->orig_send_len;
			gensec_socket->orig_send_len = 0;
			return NT_STATUS_OK;
		}
	}

	mem_ctx = talloc_new(gensec_socket);
	if (!mem_ctx) {
		return NT_STATUS_NO_MEMORY;
	}

	nt_status = gensec_wrap_packets(gensec_socket->gensec_security, 
					mem_ctx,
					blob, &wrapped, 
					&gensec_socket->orig_send_len);
	if (!NT_STATUS_IS_OK(nt_status)) {
		talloc_free(mem_ctx);
		return nt_status;
	}
	
	gensec_socket->interrupted = true;
	gensec_socket->error = NT_STATUS_OK;

	nt_status = packet_send_callback(gensec_socket->packet, 
					 wrapped,
					 send_callback, gensec_socket);

	talloc_free(mem_ctx);

	packet_queue_run(gensec_socket->packet);

	if (!NT_STATUS_IS_OK(gensec_socket->error)) {
		return gensec_socket->error;
	} else if (gensec_socket->interrupted) {
		return STATUS_MORE_ENTRIES;
	} else {
		*sendlen = gensec_socket->orig_send_len;
		gensec_socket->orig_send_len = 0;
		return NT_STATUS_OK;
	}
}

/* Turn a normal socket into a potentially GENSEC wrapped socket */
/* CAREFUL: this function will steal 'current_socket' */

NTSTATUS gensec_socket_init(struct gensec_security *gensec_security,
			    TALLOC_CTX *mem_ctx,
			    struct socket_context *current_socket,
			    struct tevent_context *ev,
			    void (*recv_handler)(void *, uint16_t),
			    void *recv_private,
			    struct socket_context **new_socket)
{
	struct gensec_socket *gensec_socket;
	struct socket_context *new_sock;
	NTSTATUS nt_status;

	nt_status = socket_create_with_ops(mem_ctx, &gensec_socket_ops, &new_sock, 
					   SOCKET_TYPE_STREAM, current_socket->flags | SOCKET_FLAG_ENCRYPT);
	if (!NT_STATUS_IS_OK(nt_status)) {
		*new_socket = NULL;
		return nt_status;
	}

	new_sock->state = current_socket->state;

	gensec_socket = talloc(new_sock, struct gensec_socket);
	if (gensec_socket == NULL) {
		*new_socket = NULL;
		talloc_free(new_sock);
		return NT_STATUS_NO_MEMORY;
	}

	new_sock->private_data       = gensec_socket;
	gensec_socket->socket        = current_socket;

	/* Nothing to do here, if we are not actually wrapping on this socket */
	if (!gensec_have_feature(gensec_security, GENSEC_FEATURE_SEAL) &&
	    !gensec_have_feature(gensec_security, GENSEC_FEATURE_SIGN)) {
		
		gensec_socket->wrap = false;
		talloc_steal(gensec_socket, current_socket);
		*new_socket = new_sock;
		return NT_STATUS_OK;
	}

	gensec_socket->gensec_security = gensec_security;

	gensec_socket->wrap          = true;
	gensec_socket->eof           = false;
	gensec_socket->error         = NT_STATUS_OK;
	gensec_socket->interrupted   = false;
	gensec_socket->in_extra_read = 0;

	gensec_socket->read_buffer   = data_blob(NULL, 0);

	gensec_socket->recv_handler  = recv_handler;
	gensec_socket->recv_private  = recv_private;
	gensec_socket->ev            = ev;

	gensec_socket->packet = packet_init(gensec_socket);
	if (gensec_socket->packet == NULL) {
		*new_socket = NULL;
		talloc_free(new_sock);
		return NT_STATUS_NO_MEMORY;
	}

	packet_set_private(gensec_socket->packet, gensec_socket);
	packet_set_socket(gensec_socket->packet, gensec_socket->socket);
	packet_set_callback(gensec_socket->packet, gensec_socket_unwrap);
	packet_set_full_request(gensec_socket->packet, gensec_socket_full_request);
	packet_set_error_handler(gensec_socket->packet, gensec_socket_error_handler);
	packet_set_serialise(gensec_socket->packet);

	/* TODO: full-request that knows about maximum packet size */

	talloc_steal(gensec_socket, current_socket);
	*new_socket = new_sock;
	return NT_STATUS_OK;
}


static NTSTATUS gensec_socket_set_option(struct socket_context *sock, const char *option, const char *val)
{
	set_socket_options(socket_get_fd(sock), option);
	return NT_STATUS_OK;
}

static char *gensec_socket_get_peer_name(struct socket_context *sock, TALLOC_CTX *mem_ctx)
{
	struct gensec_socket *gensec = talloc_get_type(sock->private_data, struct gensec_socket);
	return socket_get_peer_name(gensec->socket, mem_ctx);
}

static struct socket_address *gensec_socket_get_peer_addr(struct socket_context *sock, TALLOC_CTX *mem_ctx)
{
	struct gensec_socket *gensec = talloc_get_type(sock->private_data, struct gensec_socket);
	return socket_get_peer_addr(gensec->socket, mem_ctx);
}

static struct socket_address *gensec_socket_get_my_addr(struct socket_context *sock, TALLOC_CTX *mem_ctx)
{
	struct gensec_socket *gensec = talloc_get_type(sock->private_data, struct gensec_socket);
	return socket_get_my_addr(gensec->socket, mem_ctx);
}

static int gensec_socket_get_fd(struct socket_context *sock)
{
	struct gensec_socket *gensec = talloc_get_type(sock->private_data, struct gensec_socket);
	return socket_get_fd(gensec->socket);
}

static const struct socket_ops gensec_socket_ops = {
	.name			= "gensec",
	.fn_init		= gensec_socket_init_fn,
	.fn_recv		= gensec_socket_recv,
	.fn_send		= gensec_socket_send,
	.fn_pending		= gensec_socket_pending,

	.fn_set_option		= gensec_socket_set_option,

	.fn_get_peer_name	= gensec_socket_get_peer_name,
	.fn_get_peer_addr	= gensec_socket_get_peer_addr,
	.fn_get_my_addr		= gensec_socket_get_my_addr,
	.fn_get_fd		= gensec_socket_get_fd
};

