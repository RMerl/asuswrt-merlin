/*
   Unix SMB/CIFS implementation.
   async socket syscalls
   Copyright (C) Volker Lendecke 2008

     ** NOTE! The following LGPL license applies to the async_sock
     ** library. This does NOT imply that all of Samba is released
     ** under the LGPL

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "replace.h"
#include "system/network.h"
#include "system/filesys.h"
#include <talloc.h>
#include <tevent.h>
#include "lib/async_req/async_sock.h"

/* Note: lib/util/ is currently GPL */
#include "lib/util/tevent_unix.h"
#include "lib/util/util.h"

#ifndef TALLOC_FREE
#define TALLOC_FREE(ctx) do { talloc_free(ctx); ctx=NULL; } while(0)
#endif

struct async_send_state {
	int fd;
	const void *buf;
	size_t len;
	int flags;
	ssize_t sent;
};

static void async_send_handler(struct tevent_context *ev,
			       struct tevent_fd *fde,
			       uint16_t flags, void *private_data);

struct tevent_req *async_send_send(TALLOC_CTX *mem_ctx,
				   struct tevent_context *ev,
				   int fd, const void *buf, size_t len,
				   int flags)
{
	struct tevent_req *result;
	struct async_send_state *state;
	struct tevent_fd *fde;

	result = tevent_req_create(mem_ctx, &state, struct async_send_state);
	if (result == NULL) {
		return result;
	}
	state->fd = fd;
	state->buf = buf;
	state->len = len;
	state->flags = flags;

	fde = tevent_add_fd(ev, state, fd, TEVENT_FD_WRITE, async_send_handler,
			    result);
	if (fde == NULL) {
		TALLOC_FREE(result);
		return NULL;
	}
	return result;
}

static void async_send_handler(struct tevent_context *ev,
			       struct tevent_fd *fde,
			       uint16_t flags, void *private_data)
{
	struct tevent_req *req = talloc_get_type_abort(
		private_data, struct tevent_req);
	struct async_send_state *state =
		tevent_req_data(req, struct async_send_state);

	state->sent = send(state->fd, state->buf, state->len, state->flags);
	if ((state->sent == -1) && (errno == EINTR)) {
		/* retry */
		return;
	}
	if (state->sent == -1) {
		tevent_req_error(req, errno);
		return;
	}
	tevent_req_done(req);
}

ssize_t async_send_recv(struct tevent_req *req, int *perrno)
{
	struct async_send_state *state =
		tevent_req_data(req, struct async_send_state);

	if (tevent_req_is_unix_error(req, perrno)) {
		return -1;
	}
	return state->sent;
}

struct async_recv_state {
	int fd;
	void *buf;
	size_t len;
	int flags;
	ssize_t received;
};

static void async_recv_handler(struct tevent_context *ev,
			       struct tevent_fd *fde,
			       uint16_t flags, void *private_data);

struct tevent_req *async_recv_send(TALLOC_CTX *mem_ctx,
				   struct tevent_context *ev,
				   int fd, void *buf, size_t len, int flags)
{
	struct tevent_req *result;
	struct async_recv_state *state;
	struct tevent_fd *fde;

	result = tevent_req_create(mem_ctx, &state, struct async_recv_state);
	if (result == NULL) {
		return result;
	}
	state->fd = fd;
	state->buf = buf;
	state->len = len;
	state->flags = flags;

	fde = tevent_add_fd(ev, state, fd, TEVENT_FD_READ, async_recv_handler,
			    result);
	if (fde == NULL) {
		TALLOC_FREE(result);
		return NULL;
	}
	return result;
}

static void async_recv_handler(struct tevent_context *ev,
			       struct tevent_fd *fde,
			       uint16_t flags, void *private_data)
{
	struct tevent_req *req = talloc_get_type_abort(
		private_data, struct tevent_req);
	struct async_recv_state *state =
		tevent_req_data(req, struct async_recv_state);

	state->received = recv(state->fd, state->buf, state->len,
			       state->flags);
	if ((state->received == -1) && (errno == EINTR)) {
		/* retry */
		return;
	}
	if (state->received == 0) {
		tevent_req_error(req, EPIPE);
		return;
	}
	if (state->received == -1) {
		tevent_req_error(req, errno);
		return;
	}
	tevent_req_done(req);
}

ssize_t async_recv_recv(struct tevent_req *req, int *perrno)
{
	struct async_recv_state *state =
		tevent_req_data(req, struct async_recv_state);

	if (tevent_req_is_unix_error(req, perrno)) {
		return -1;
	}
	return state->received;
}

struct async_connect_state {
	int fd;
	int result;
	int sys_errno;
	long old_sockflags;
	socklen_t address_len;
	struct sockaddr_storage address;
};

static void async_connect_connected(struct tevent_context *ev,
				    struct tevent_fd *fde, uint16_t flags,
				    void *priv);

/**
 * @brief async version of connect(2)
 * @param[in] mem_ctx	The memory context to hang the result off
 * @param[in] ev	The event context to work from
 * @param[in] fd	The socket to recv from
 * @param[in] address	Where to connect?
 * @param[in] address_len Length of *address
 * @retval The async request
 *
 * This function sets the socket into non-blocking state to be able to call
 * connect in an async state. This will be reset when the request is finished.
 */

struct tevent_req *async_connect_send(TALLOC_CTX *mem_ctx,
				      struct tevent_context *ev,
				      int fd, const struct sockaddr *address,
				      socklen_t address_len)
{
	struct tevent_req *result;
	struct async_connect_state *state;
	struct tevent_fd *fde;

	result = tevent_req_create(
		mem_ctx, &state, struct async_connect_state);
	if (result == NULL) {
		return NULL;
	}

	/**
	 * We have to set the socket to nonblocking for async connect(2). Keep
	 * the old sockflags around.
	 */

	state->fd = fd;
	state->sys_errno = 0;

	state->old_sockflags = fcntl(fd, F_GETFL, 0);
	if (state->old_sockflags == -1) {
		goto post_errno;
	}

	state->address_len = address_len;
	if (address_len > sizeof(state->address)) {
		errno = EINVAL;
		goto post_errno;
	}
	memcpy(&state->address, address, address_len);

	set_blocking(fd, false);

	state->result = connect(fd, address, address_len);
	if (state->result == 0) {
		tevent_req_done(result);
		goto done;
	}

	/**
	 * A number of error messages show that something good is progressing
	 * and that we have to wait for readability.
	 *
	 * If none of them are present, bail out.
	 */

	if (!(errno == EINPROGRESS || errno == EALREADY ||
#ifdef EISCONN
	      errno == EISCONN ||
#endif
	      errno == EAGAIN || errno == EINTR)) {
		state->sys_errno = errno;
		goto post_errno;
	}

	fde = tevent_add_fd(ev, state, fd, TEVENT_FD_READ | TEVENT_FD_WRITE,
			   async_connect_connected, result);
	if (fde == NULL) {
		state->sys_errno = ENOMEM;
		goto post_errno;
	}
	return result;

 post_errno:
	tevent_req_error(result, state->sys_errno);
 done:
	fcntl(fd, F_SETFL, state->old_sockflags);
	return tevent_req_post(result, ev);
}

/**
 * fde event handler for connect(2)
 * @param[in] ev	The event context that sent us here
 * @param[in] fde	The file descriptor event associated with the connect
 * @param[in] flags	Indicate read/writeability of the socket
 * @param[in] priv	private data, "struct async_req *" in this case
 */

static void async_connect_connected(struct tevent_context *ev,
				    struct tevent_fd *fde, uint16_t flags,
				    void *priv)
{
	struct tevent_req *req = talloc_get_type_abort(
		priv, struct tevent_req);
	struct async_connect_state *state =
		tevent_req_data(req, struct async_connect_state);

	/*
	 * Stevens, Network Programming says that if there's a
	 * successful connect, the socket is only writable. Upon an
	 * error, it's both readable and writable.
	 */
	if ((flags & (TEVENT_FD_READ|TEVENT_FD_WRITE))
	    == (TEVENT_FD_READ|TEVENT_FD_WRITE)) {
		int ret;

		ret = connect(state->fd,
			      (struct sockaddr *)(void *)&state->address,
			      state->address_len);
		if (ret == 0) {
			TALLOC_FREE(fde);
			tevent_req_done(req);
			return;
		}

		if (errno == EINPROGRESS) {
			/* Try again later, leave the fde around */
			return;
		}
		TALLOC_FREE(fde);
		tevent_req_error(req, errno);
		return;
	}

	state->sys_errno = 0;
	tevent_req_done(req);
}

int async_connect_recv(struct tevent_req *req, int *perrno)
{
	struct async_connect_state *state =
		tevent_req_data(req, struct async_connect_state);
	int err;

	fcntl(state->fd, F_SETFL, state->old_sockflags);

	if (tevent_req_is_unix_error(req, &err)) {
		*perrno = err;
		return -1;
	}

	if (state->sys_errno == 0) {
		return 0;
	}

	*perrno = state->sys_errno;
	return -1;
}

struct writev_state {
	struct tevent_context *ev;
	int fd;
	struct iovec *iov;
	int count;
	size_t total_size;
	uint16_t flags;
};

static void writev_trigger(struct tevent_req *req, void *private_data);
static void writev_handler(struct tevent_context *ev, struct tevent_fd *fde,
			   uint16_t flags, void *private_data);

struct tevent_req *writev_send(TALLOC_CTX *mem_ctx, struct tevent_context *ev,
			       struct tevent_queue *queue, int fd,
			       bool err_on_readability,
			       struct iovec *iov, int count)
{
	struct tevent_req *req;
	struct writev_state *state;

	req = tevent_req_create(mem_ctx, &state, struct writev_state);
	if (req == NULL) {
		return NULL;
	}
	state->ev = ev;
	state->fd = fd;
	state->total_size = 0;
	state->count = count;
	state->iov = (struct iovec *)talloc_memdup(
		state, iov, sizeof(struct iovec) * count);
	if (state->iov == NULL) {
		goto fail;
	}
	state->flags = TEVENT_FD_WRITE;
	if (err_on_readability) {
		state->flags |= TEVENT_FD_READ;
	}

	if (queue == NULL) {
		struct tevent_fd *fde;
		fde = tevent_add_fd(state->ev, state, state->fd,
				    state->flags, writev_handler, req);
		if (tevent_req_nomem(fde, req)) {
			return tevent_req_post(req, ev);
		}
		return req;
	}

	if (!tevent_queue_add(queue, ev, req, writev_trigger, NULL)) {
		goto fail;
	}
	return req;
 fail:
	TALLOC_FREE(req);
	return NULL;
}

static void writev_trigger(struct tevent_req *req, void *private_data)
{
	struct writev_state *state = tevent_req_data(req, struct writev_state);
	struct tevent_fd *fde;

	fde = tevent_add_fd(state->ev, state, state->fd, state->flags,
			    writev_handler, req);
	if (fde == NULL) {
		tevent_req_error(req, ENOMEM);
	}
}

static void writev_handler(struct tevent_context *ev, struct tevent_fd *fde,
			   uint16_t flags, void *private_data)
{
	struct tevent_req *req = talloc_get_type_abort(
		private_data, struct tevent_req);
	struct writev_state *state =
		tevent_req_data(req, struct writev_state);
	size_t to_write, written;
	int i;

	to_write = 0;

	if ((state->flags & TEVENT_FD_READ) && (flags & TEVENT_FD_READ)) {
		tevent_req_error(req, EPIPE);
		return;
	}

	for (i=0; i<state->count; i++) {
		to_write += state->iov[i].iov_len;
	}

	written = writev(state->fd, state->iov, state->count);
	if ((written == -1) && (errno == EINTR)) {
		/* retry */
		return;
	}
	if (written == -1) {
		tevent_req_error(req, errno);
		return;
	}
	if (written == 0) {
		tevent_req_error(req, EPIPE);
		return;
	}
	state->total_size += written;

	if (written == to_write) {
		tevent_req_done(req);
		return;
	}

	/*
	 * We've written less than we were asked to, drop stuff from
	 * state->iov.
	 */

	while (written > 0) {
		if (written < state->iov[0].iov_len) {
			state->iov[0].iov_base =
				(char *)state->iov[0].iov_base + written;
			state->iov[0].iov_len -= written;
			break;
		}
		written -= state->iov[0].iov_len;
		state->iov += 1;
		state->count -= 1;
	}
}

ssize_t writev_recv(struct tevent_req *req, int *perrno)
{
	struct writev_state *state =
		tevent_req_data(req, struct writev_state);

	if (tevent_req_is_unix_error(req, perrno)) {
		return -1;
	}
	return state->total_size;
}

struct read_packet_state {
	int fd;
	uint8_t *buf;
	size_t nread;
	ssize_t (*more)(uint8_t *buf, size_t buflen, void *private_data);
	void *private_data;
};

static void read_packet_handler(struct tevent_context *ev,
				struct tevent_fd *fde,
				uint16_t flags, void *private_data);

struct tevent_req *read_packet_send(TALLOC_CTX *mem_ctx,
				    struct tevent_context *ev,
				    int fd, size_t initial,
				    ssize_t (*more)(uint8_t *buf,
						    size_t buflen,
						    void *private_data),
				    void *private_data)
{
	struct tevent_req *result;
	struct read_packet_state *state;
	struct tevent_fd *fde;

	result = tevent_req_create(mem_ctx, &state, struct read_packet_state);
	if (result == NULL) {
		return NULL;
	}
	state->fd = fd;
	state->nread = 0;
	state->more = more;
	state->private_data = private_data;

	state->buf = talloc_array(state, uint8_t, initial);
	if (state->buf == NULL) {
		goto fail;
	}

	fde = tevent_add_fd(ev, state, fd, TEVENT_FD_READ, read_packet_handler,
			    result);
	if (fde == NULL) {
		goto fail;
	}
	return result;
 fail:
	TALLOC_FREE(result);
	return NULL;
}

static void read_packet_handler(struct tevent_context *ev,
				struct tevent_fd *fde,
				uint16_t flags, void *private_data)
{
	struct tevent_req *req = talloc_get_type_abort(
		private_data, struct tevent_req);
	struct read_packet_state *state =
		tevent_req_data(req, struct read_packet_state);
	size_t total = talloc_get_size(state->buf);
	ssize_t nread, more;
	uint8_t *tmp;

	nread = recv(state->fd, state->buf+state->nread, total-state->nread,
		     0);
	if ((nread == -1) && (errno == EINTR)) {
		/* retry */
		return;
	}
	if (nread == -1) {
		tevent_req_error(req, errno);
		return;
	}
	if (nread == 0) {
		tevent_req_error(req, EPIPE);
		return;
	}

	state->nread += nread;
	if (state->nread < total) {
		/* Come back later */
		return;
	}

	/*
	 * We got what was initially requested. See if "more" asks for -- more.
	 */
	if (state->more == NULL) {
		/* Nobody to ask, this is a async read_data */
		tevent_req_done(req);
		return;
	}

	more = state->more(state->buf, total, state->private_data);
	if (more == -1) {
		/* We got an invalid packet, tell the caller */
		tevent_req_error(req, EIO);
		return;
	}
	if (more == 0) {
		/* We're done, full packet received */
		tevent_req_done(req);
		return;
	}

	tmp = talloc_realloc(state, state->buf, uint8_t, total+more);
	if (tevent_req_nomem(tmp, req)) {
		return;
	}
	state->buf = tmp;
}

ssize_t read_packet_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx,
			 uint8_t **pbuf, int *perrno)
{
	struct read_packet_state *state =
		tevent_req_data(req, struct read_packet_state);

	if (tevent_req_is_unix_error(req, perrno)) {
		return -1;
	}
	*pbuf = talloc_move(mem_ctx, &state->buf);
	return talloc_get_size(*pbuf);
}
