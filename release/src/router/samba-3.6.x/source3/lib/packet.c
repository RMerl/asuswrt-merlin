/* 
   Unix SMB/CIFS implementation.
   Packet handling
   Copyright (C) Volker Lendecke 2007

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
#include "../lib/util/select.h"
#include "system/filesys.h"
#include "system/select.h"
#include "packet.h"

struct packet_context {
	int fd;
	DATA_BLOB in, out;
};

/*
 * Close the underlying fd
 */
static int packet_context_destructor(struct packet_context *ctx)
{
	return close(ctx->fd);
}

/*
 * Initialize a packet context. The fd is given to the packet context, meaning
 * that it is automatically closed when the packet context is freed.
 */
struct packet_context *packet_init(TALLOC_CTX *mem_ctx, int fd)
{
	struct packet_context *result;

	if (!(result = TALLOC_ZERO_P(mem_ctx, struct packet_context))) {
		return NULL;
	}

	result->fd = fd;
	talloc_set_destructor(result, packet_context_destructor);
	return result;
}

/*
 * Pull data from the fd
 */
NTSTATUS packet_fd_read(struct packet_context *ctx)
{
	int res, available;
	size_t new_size;
	uint8 *in;

	res = ioctl(ctx->fd, FIONREAD, &available);

	if (res == -1) {
		DEBUG(10, ("ioctl(FIONREAD) failed: %s\n", strerror(errno)));
		return map_nt_error_from_unix(errno);
	}

	SMB_ASSERT(available >= 0);

	if (available == 0) {
		return NT_STATUS_END_OF_FILE;
	}

	new_size = ctx->in.length + available;

	if (new_size < ctx->in.length) {
		DEBUG(0, ("integer wrap\n"));
		return NT_STATUS_NO_MEMORY;
	}

	if (!(in = TALLOC_REALLOC_ARRAY(ctx, ctx->in.data, uint8, new_size))) {
		DEBUG(10, ("talloc failed\n"));
		return NT_STATUS_NO_MEMORY;
	}

	ctx->in.data = in;

	res = recv(ctx->fd, in + ctx->in.length, available, 0);

	if (res < 0) {
		DEBUG(10, ("recv failed: %s\n", strerror(errno)));
		return map_nt_error_from_unix(errno);
	}

	if (res == 0) {
		return NT_STATUS_END_OF_FILE;
	}

	ctx->in.length += res;

	return NT_STATUS_OK;
}

NTSTATUS packet_fd_read_sync(struct packet_context *ctx, int timeout)
{
	int res, revents;

	res = poll_one_fd(ctx->fd, POLLIN|POLLHUP, timeout, &revents);
	if (res == 0) {
		DEBUG(10, ("poll timed out\n"));
		return NT_STATUS_IO_TIMEOUT;
	}

	if (res == -1) {
		DEBUG(10, ("poll returned %s\n", strerror(errno)));
		return map_nt_error_from_unix(errno);
	}
	if ((revents & (POLLIN|POLLHUP|POLLERR)) == 0) {
		DEBUG(10, ("socket not readable\n"));
		return NT_STATUS_IO_TIMEOUT;
	}

	return packet_fd_read(ctx);
}

bool packet_handler(struct packet_context *ctx,
		    bool (*full_req)(const uint8_t *buf,
				     size_t available,
				     size_t *length,
				     void *priv),
		    NTSTATUS (*callback)(uint8_t *buf, size_t length,
					 void *priv),
		    void *priv, NTSTATUS *status)
{
	size_t length;
	uint8_t *buf;

	if (!full_req(ctx->in.data, ctx->in.length, &length, priv)) {
		return False;
	}

	if (length > ctx->in.length) {
		*status = NT_STATUS_INTERNAL_ERROR;
		return true;
	}

	if (length == ctx->in.length) {
		buf = ctx->in.data;
		ctx->in.data = NULL;
		ctx->in.length = 0;
	} else {
		buf = (uint8_t *)TALLOC_MEMDUP(ctx, ctx->in.data, length);
		if (buf == NULL) {
			*status = NT_STATUS_NO_MEMORY;
			return true;
		}

		memmove(ctx->in.data, ctx->in.data + length,
			ctx->in.length - length);
		ctx->in.length -= length;
	}

	*status = callback(buf, length, priv);
	return True;
}

/*
 * How many bytes of outgoing data do we have pending?
 */
size_t packet_outgoing_bytes(struct packet_context *ctx)
{
	return ctx->out.length;
}

/*
 * Push data to the fd
 */
NTSTATUS packet_fd_write(struct packet_context *ctx)
{
	ssize_t sent;

	sent = send(ctx->fd, ctx->out.data, ctx->out.length, 0);

	if (sent == -1) {
		DEBUG(0, ("send failed: %s\n", strerror(errno)));
		return map_nt_error_from_unix(errno);
	}

	memmove(ctx->out.data, ctx->out.data + sent,
		ctx->out.length - sent);
	ctx->out.length -= sent;

	return NT_STATUS_OK;
}

/*
 * Sync flush all outgoing bytes
 */
NTSTATUS packet_flush(struct packet_context *ctx)
{
	while (ctx->out.length != 0) {
		NTSTATUS status = packet_fd_write(ctx);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}
	}
	return NT_STATUS_OK;
}

/*
 * Send a list of DATA_BLOBs
 *
 * Example:  packet_send(ctx, 2, data_blob_const(&size, sizeof(size)),
 *			 data_blob_const(buf, size));
 */
NTSTATUS packet_send(struct packet_context *ctx, int num_blobs, ...)
{
	va_list ap;
	int i;
	size_t len;
	uint8 *out;

	len = ctx->out.length;

	va_start(ap, num_blobs);
	for (i=0; i<num_blobs; i++) {
		size_t tmp;
		DATA_BLOB blob = va_arg(ap, DATA_BLOB);

		tmp = len + blob.length;
		if (tmp < len) {
			DEBUG(0, ("integer overflow\n"));
			va_end(ap);
			return NT_STATUS_NO_MEMORY;
		}
		len = tmp;
	}
	va_end(ap);

	if (len == 0) {
		return NT_STATUS_OK;
	}

	if (!(out = TALLOC_REALLOC_ARRAY(ctx, ctx->out.data, uint8, len))) {
		DEBUG(0, ("talloc failed\n"));
		return NT_STATUS_NO_MEMORY;
	}

	ctx->out.data = out;

	va_start(ap, num_blobs);
	for (i=0; i<num_blobs; i++) {
		DATA_BLOB blob = va_arg(ap, DATA_BLOB);

		memcpy(ctx->out.data+ctx->out.length, blob.data, blob.length);
		ctx->out.length += blob.length;
	}
	va_end(ap);

	SMB_ASSERT(ctx->out.length == len);
	return NT_STATUS_OK;
}

/*
 * Get the packet context's file descriptor
 */
int packet_get_fd(struct packet_context *ctx)
{
	return ctx->fd;
}

