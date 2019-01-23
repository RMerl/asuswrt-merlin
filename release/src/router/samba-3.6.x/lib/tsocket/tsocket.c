/*
   Unix SMB/CIFS implementation.

   Copyright (C) Stefan Metzmacher 2009

     ** NOTE! The following LGPL license applies to the tsocket
     ** library. This does NOT imply that all of Samba is released
     ** under the LGPL

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/

#include "replace.h"
#include "system/filesys.h"
#include "tsocket.h"
#include "tsocket_internal.h"

int tsocket_simple_int_recv(struct tevent_req *req, int *perrno)
{
	enum tevent_req_state state;
	uint64_t error;

	if (!tevent_req_is_error(req, &state, &error)) {
		return 0;
	}

	switch (state) {
	case TEVENT_REQ_NO_MEMORY:
		*perrno = ENOMEM;
		return -1;
	case TEVENT_REQ_TIMED_OUT:
		*perrno = ETIMEDOUT;
		return -1;
	case TEVENT_REQ_USER_ERROR:
		*perrno = (int)error;
		return -1;
	default:
		break;
	}

	*perrno = EIO;
	return -1;
}

struct tsocket_address *_tsocket_address_create(TALLOC_CTX *mem_ctx,
						const struct tsocket_address_ops *ops,
						void *pstate,
						size_t psize,
						const char *type,
						const char *location)
{
	void **ppstate = (void **)pstate;
	struct tsocket_address *addr;

	addr = talloc_zero(mem_ctx, struct tsocket_address);
	if (!addr) {
		return NULL;
	}
	addr->ops = ops;
	addr->location = location;
	addr->private_data = talloc_size(addr, psize);
	if (!addr->private_data) {
		talloc_free(addr);
		return NULL;
	}
	talloc_set_name_const(addr->private_data, type);

	*ppstate = addr->private_data;
	return addr;
}

char *tsocket_address_string(const struct tsocket_address *addr,
			     TALLOC_CTX *mem_ctx)
{
	if (!addr) {
		return talloc_strdup(mem_ctx, "NULL");
	}
	return addr->ops->string(addr, mem_ctx);
}

struct tsocket_address *_tsocket_address_copy(const struct tsocket_address *addr,
					      TALLOC_CTX *mem_ctx,
					      const char *location)
{
	return addr->ops->copy(addr, mem_ctx, location);
}

struct tdgram_context {
	const char *location;
	const struct tdgram_context_ops *ops;
	void *private_data;

	struct tevent_req *recvfrom_req;
	struct tevent_req *sendto_req;
};

static int tdgram_context_destructor(struct tdgram_context *dgram)
{
	if (dgram->recvfrom_req) {
		tevent_req_received(dgram->recvfrom_req);
	}

	if (dgram->sendto_req) {
		tevent_req_received(dgram->sendto_req);
	}

	return 0;
}

struct tdgram_context *_tdgram_context_create(TALLOC_CTX *mem_ctx,
					const struct tdgram_context_ops *ops,
					void *pstate,
					size_t psize,
					const char *type,
					const char *location)
{
	struct tdgram_context *dgram;
	void **ppstate = (void **)pstate;
	void *state;

	dgram = talloc(mem_ctx, struct tdgram_context);
	if (dgram == NULL) {
		return NULL;
	}
	dgram->location		= location;
	dgram->ops		= ops;
	dgram->recvfrom_req	= NULL;
	dgram->sendto_req	= NULL;

	state = talloc_size(dgram, psize);
	if (state == NULL) {
		talloc_free(dgram);
		return NULL;
	}
	talloc_set_name_const(state, type);

	dgram->private_data = state;

	talloc_set_destructor(dgram, tdgram_context_destructor);

	*ppstate = state;
	return dgram;
}

void *_tdgram_context_data(struct tdgram_context *dgram)
{
	return dgram->private_data;
}

struct tdgram_recvfrom_state {
	const struct tdgram_context_ops *ops;
	struct tdgram_context *dgram;
	uint8_t *buf;
	size_t len;
	struct tsocket_address *src;
};

static int tdgram_recvfrom_destructor(struct tdgram_recvfrom_state *state)
{
	if (state->dgram) {
		state->dgram->recvfrom_req = NULL;
	}

	return 0;
}

static void tdgram_recvfrom_done(struct tevent_req *subreq);

struct tevent_req *tdgram_recvfrom_send(TALLOC_CTX *mem_ctx,
					struct tevent_context *ev,
					struct tdgram_context *dgram)
{
	struct tevent_req *req;
	struct tdgram_recvfrom_state *state;
	struct tevent_req *subreq;

	req = tevent_req_create(mem_ctx, &state,
				struct tdgram_recvfrom_state);
	if (req == NULL) {
		return NULL;
	}

	state->ops = dgram->ops;
	state->dgram = dgram;
	state->buf = NULL;
	state->len = 0;
	state->src = NULL;

	if (dgram->recvfrom_req) {
		tevent_req_error(req, EBUSY);
		goto post;
	}
	dgram->recvfrom_req = req;

	talloc_set_destructor(state, tdgram_recvfrom_destructor);

	subreq = state->ops->recvfrom_send(state, ev, dgram);
	if (tevent_req_nomem(subreq, req)) {
		goto post;
	}
	tevent_req_set_callback(subreq, tdgram_recvfrom_done, req);

	return req;

 post:
	tevent_req_post(req, ev);
	return req;
}

static void tdgram_recvfrom_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(subreq,
				 struct tevent_req);
	struct tdgram_recvfrom_state *state = tevent_req_data(req,
					      struct tdgram_recvfrom_state);
	ssize_t ret;
	int sys_errno;

	ret = state->ops->recvfrom_recv(subreq, &sys_errno, state,
					&state->buf, &state->src);
	if (ret == -1) {
		tevent_req_error(req, sys_errno);
		return;
	}

	state->len = ret;

	tevent_req_done(req);
}

ssize_t tdgram_recvfrom_recv(struct tevent_req *req,
			     int *perrno,
			     TALLOC_CTX *mem_ctx,
			     uint8_t **buf,
			     struct tsocket_address **src)
{
	struct tdgram_recvfrom_state *state = tevent_req_data(req,
					      struct tdgram_recvfrom_state);
	ssize_t ret;

	ret = tsocket_simple_int_recv(req, perrno);
	if (ret == 0) {
		*buf = talloc_move(mem_ctx, &state->buf);
		ret = state->len;
		if (src) {
			*src = talloc_move(mem_ctx, &state->src);
		}
	}

	tevent_req_received(req);
	return ret;
}

struct tdgram_sendto_state {
	const struct tdgram_context_ops *ops;
	struct tdgram_context *dgram;
	ssize_t ret;
};

static int tdgram_sendto_destructor(struct tdgram_sendto_state *state)
{
	if (state->dgram) {
		state->dgram->sendto_req = NULL;
	}

	return 0;
}

static void tdgram_sendto_done(struct tevent_req *subreq);

struct tevent_req *tdgram_sendto_send(TALLOC_CTX *mem_ctx,
				      struct tevent_context *ev,
				      struct tdgram_context *dgram,
				      const uint8_t *buf, size_t len,
				      const struct tsocket_address *dst)
{
	struct tevent_req *req;
	struct tdgram_sendto_state *state;
	struct tevent_req *subreq;

	req = tevent_req_create(mem_ctx, &state,
				struct tdgram_sendto_state);
	if (req == NULL) {
		return NULL;
	}

	state->ops = dgram->ops;
	state->dgram = dgram;
	state->ret = -1;

	if (len == 0) {
		tevent_req_error(req, EINVAL);
		goto post;
	}

	if (dgram->sendto_req) {
		tevent_req_error(req, EBUSY);
		goto post;
	}
	dgram->sendto_req = req;

	talloc_set_destructor(state, tdgram_sendto_destructor);

	subreq = state->ops->sendto_send(state, ev, dgram,
					 buf, len, dst);
	if (tevent_req_nomem(subreq, req)) {
		goto post;
	}
	tevent_req_set_callback(subreq, tdgram_sendto_done, req);

	return req;

 post:
	tevent_req_post(req, ev);
	return req;
}

static void tdgram_sendto_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(subreq,
				 struct tevent_req);
	struct tdgram_sendto_state *state = tevent_req_data(req,
					    struct tdgram_sendto_state);
	ssize_t ret;
	int sys_errno;

	ret = state->ops->sendto_recv(subreq, &sys_errno);
	if (ret == -1) {
		tevent_req_error(req, sys_errno);
		return;
	}

	state->ret = ret;

	tevent_req_done(req);
}

ssize_t tdgram_sendto_recv(struct tevent_req *req,
			   int *perrno)
{
	struct tdgram_sendto_state *state = tevent_req_data(req,
					    struct tdgram_sendto_state);
	ssize_t ret;

	ret = tsocket_simple_int_recv(req, perrno);
	if (ret == 0) {
		ret = state->ret;
	}

	tevent_req_received(req);
	return ret;
}

struct tdgram_disconnect_state {
	const struct tdgram_context_ops *ops;
};

static void tdgram_disconnect_done(struct tevent_req *subreq);

struct tevent_req *tdgram_disconnect_send(TALLOC_CTX *mem_ctx,
					  struct tevent_context *ev,
					  struct tdgram_context *dgram)
{
	struct tevent_req *req;
	struct tdgram_disconnect_state *state;
	struct tevent_req *subreq;

	req = tevent_req_create(mem_ctx, &state,
				struct tdgram_disconnect_state);
	if (req == NULL) {
		return NULL;
	}

	state->ops = dgram->ops;

	if (dgram->recvfrom_req || dgram->sendto_req) {
		tevent_req_error(req, EBUSY);
		goto post;
	}

	subreq = state->ops->disconnect_send(state, ev, dgram);
	if (tevent_req_nomem(subreq, req)) {
		goto post;
	}
	tevent_req_set_callback(subreq, tdgram_disconnect_done, req);

	return req;

 post:
	tevent_req_post(req, ev);
	return req;
}

static void tdgram_disconnect_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(subreq,
				 struct tevent_req);
	struct tdgram_disconnect_state *state = tevent_req_data(req,
						struct tdgram_disconnect_state);
	int ret;
	int sys_errno;

	ret = state->ops->disconnect_recv(subreq, &sys_errno);
	if (ret == -1) {
		tevent_req_error(req, sys_errno);
		return;
	}

	tevent_req_done(req);
}

int tdgram_disconnect_recv(struct tevent_req *req,
			   int *perrno)
{
	int ret;

	ret = tsocket_simple_int_recv(req, perrno);

	tevent_req_received(req);
	return ret;
}

struct tstream_context {
	const char *location;
	const struct tstream_context_ops *ops;
	void *private_data;

	struct tevent_req *readv_req;
	struct tevent_req *writev_req;
};

static int tstream_context_destructor(struct tstream_context *stream)
{
	if (stream->readv_req) {
		tevent_req_received(stream->readv_req);
	}

	if (stream->writev_req) {
		tevent_req_received(stream->writev_req);
	}

	return 0;
}

struct tstream_context *_tstream_context_create(TALLOC_CTX *mem_ctx,
					const struct tstream_context_ops *ops,
					void *pstate,
					size_t psize,
					const char *type,
					const char *location)
{
	struct tstream_context *stream;
	void **ppstate = (void **)pstate;
	void *state;

	stream = talloc(mem_ctx, struct tstream_context);
	if (stream == NULL) {
		return NULL;
	}
	stream->location	= location;
	stream->ops		= ops;
	stream->readv_req	= NULL;
	stream->writev_req	= NULL;

	state = talloc_size(stream, psize);
	if (state == NULL) {
		talloc_free(stream);
		return NULL;
	}
	talloc_set_name_const(state, type);

	stream->private_data = state;

	talloc_set_destructor(stream, tstream_context_destructor);

	*ppstate = state;
	return stream;
}

void *_tstream_context_data(struct tstream_context *stream)
{
	return stream->private_data;
}

ssize_t tstream_pending_bytes(struct tstream_context *stream)
{
	return stream->ops->pending_bytes(stream);
}

struct tstream_readv_state {
	const struct tstream_context_ops *ops;
	struct tstream_context *stream;
	int ret;
};

static int tstream_readv_destructor(struct tstream_readv_state *state)
{
	if (state->stream) {
		state->stream->readv_req = NULL;
	}

	return 0;
}

static void tstream_readv_done(struct tevent_req *subreq);

struct tevent_req *tstream_readv_send(TALLOC_CTX *mem_ctx,
				      struct tevent_context *ev,
				      struct tstream_context *stream,
				      struct iovec *vector,
				      size_t count)
{
	struct tevent_req *req;
	struct tstream_readv_state *state;
	struct tevent_req *subreq;
	int to_read = 0;
	size_t i;

	req = tevent_req_create(mem_ctx, &state,
				struct tstream_readv_state);
	if (req == NULL) {
		return NULL;
	}

	state->ops = stream->ops;
	state->stream = stream;
	state->ret = -1;

	/* first check if the input is ok */
#ifdef IOV_MAX
	if (count > IOV_MAX) {
		tevent_req_error(req, EMSGSIZE);
		goto post;
	}
#endif

	for (i=0; i < count; i++) {
		int tmp = to_read;
		tmp += vector[i].iov_len;

		if (tmp < to_read) {
			tevent_req_error(req, EMSGSIZE);
			goto post;
		}

		to_read = tmp;
	}

	if (to_read == 0) {
		tevent_req_error(req, EINVAL);
		goto post;
	}

	if (stream->readv_req) {
		tevent_req_error(req, EBUSY);
		goto post;
	}
	stream->readv_req = req;

	talloc_set_destructor(state, tstream_readv_destructor);

	subreq = state->ops->readv_send(state, ev, stream, vector, count);
	if (tevent_req_nomem(subreq, req)) {
		goto post;
	}
	tevent_req_set_callback(subreq, tstream_readv_done, req);

	return req;

 post:
	tevent_req_post(req, ev);
	return req;
}

static void tstream_readv_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(subreq,
				 struct tevent_req);
	struct tstream_readv_state *state = tevent_req_data(req,
					    struct tstream_readv_state);
	ssize_t ret;
	int sys_errno;

	ret = state->ops->readv_recv(subreq, &sys_errno);
	TALLOC_FREE(subreq);
	if (ret == -1) {
		tevent_req_error(req, sys_errno);
		return;
	}

	state->ret = ret;

	tevent_req_done(req);
}

int tstream_readv_recv(struct tevent_req *req,
		       int *perrno)
{
	struct tstream_readv_state *state = tevent_req_data(req,
					    struct tstream_readv_state);
	int ret;

	ret = tsocket_simple_int_recv(req, perrno);
	if (ret == 0) {
		ret = state->ret;
	}

	tevent_req_received(req);
	return ret;
}

struct tstream_writev_state {
	const struct tstream_context_ops *ops;
	struct tstream_context *stream;
	int ret;
};

static int tstream_writev_destructor(struct tstream_writev_state *state)
{
	if (state->stream) {
		state->stream->writev_req = NULL;
	}

	return 0;
}

static void tstream_writev_done(struct tevent_req *subreq);

struct tevent_req *tstream_writev_send(TALLOC_CTX *mem_ctx,
				       struct tevent_context *ev,
				       struct tstream_context *stream,
				       const struct iovec *vector,
				       size_t count)
{
	struct tevent_req *req;
	struct tstream_writev_state *state;
	struct tevent_req *subreq;
	int to_write = 0;
	size_t i;

	req = tevent_req_create(mem_ctx, &state,
				struct tstream_writev_state);
	if (req == NULL) {
		return NULL;
	}

	state->ops = stream->ops;
	state->stream = stream;
	state->ret = -1;

	/* first check if the input is ok */
#ifdef IOV_MAX
	if (count > IOV_MAX) {
		tevent_req_error(req, EMSGSIZE);
		goto post;
	}
#endif

	for (i=0; i < count; i++) {
		int tmp = to_write;
		tmp += vector[i].iov_len;

		if (tmp < to_write) {
			tevent_req_error(req, EMSGSIZE);
			goto post;
		}

		to_write = tmp;
	}

	if (to_write == 0) {
		tevent_req_error(req, EINVAL);
		goto post;
	}

	if (stream->writev_req) {
		tevent_req_error(req, EBUSY);
		goto post;
	}
	stream->writev_req = req;

	talloc_set_destructor(state, tstream_writev_destructor);

	subreq = state->ops->writev_send(state, ev, stream, vector, count);
	if (tevent_req_nomem(subreq, req)) {
		goto post;
	}
	tevent_req_set_callback(subreq, tstream_writev_done, req);

	return req;

 post:
	tevent_req_post(req, ev);
	return req;
}

static void tstream_writev_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(subreq,
				 struct tevent_req);
	struct tstream_writev_state *state = tevent_req_data(req,
					     struct tstream_writev_state);
	ssize_t ret;
	int sys_errno;

	ret = state->ops->writev_recv(subreq, &sys_errno);
	if (ret == -1) {
		tevent_req_error(req, sys_errno);
		return;
	}

	state->ret = ret;

	tevent_req_done(req);
}

int tstream_writev_recv(struct tevent_req *req,
		       int *perrno)
{
	struct tstream_writev_state *state = tevent_req_data(req,
					     struct tstream_writev_state);
	int ret;

	ret = tsocket_simple_int_recv(req, perrno);
	if (ret == 0) {
		ret = state->ret;
	}

	tevent_req_received(req);
	return ret;
}

struct tstream_disconnect_state {
	const struct tstream_context_ops *ops;
};

static void tstream_disconnect_done(struct tevent_req *subreq);

struct tevent_req *tstream_disconnect_send(TALLOC_CTX *mem_ctx,
					   struct tevent_context *ev,
					   struct tstream_context *stream)
{
	struct tevent_req *req;
	struct tstream_disconnect_state *state;
	struct tevent_req *subreq;

	req = tevent_req_create(mem_ctx, &state,
				struct tstream_disconnect_state);
	if (req == NULL) {
		return NULL;
	}

	state->ops = stream->ops;

	if (stream->readv_req || stream->writev_req) {
		tevent_req_error(req, EBUSY);
		goto post;
	}

	subreq = state->ops->disconnect_send(state, ev, stream);
	if (tevent_req_nomem(subreq, req)) {
		goto post;
	}
	tevent_req_set_callback(subreq, tstream_disconnect_done, req);

	return req;

 post:
	tevent_req_post(req, ev);
	return req;
}

static void tstream_disconnect_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(subreq,
				 struct tevent_req);
	struct tstream_disconnect_state *state = tevent_req_data(req,
						 struct tstream_disconnect_state);
	int ret;
	int sys_errno;

	ret = state->ops->disconnect_recv(subreq, &sys_errno);
	if (ret == -1) {
		tevent_req_error(req, sys_errno);
		return;
	}

	tevent_req_done(req);
}

int tstream_disconnect_recv(struct tevent_req *req,
			   int *perrno)
{
	int ret;

	ret = tsocket_simple_int_recv(req, perrno);

	tevent_req_received(req);
	return ret;
}

