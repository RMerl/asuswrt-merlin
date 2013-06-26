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

struct tdgram_sendto_queue_state {
	/* this structs are owned by the caller */
	struct {
		struct tevent_context *ev;
		struct tdgram_context *dgram;
		const uint8_t *buf;
		size_t len;
		const struct tsocket_address *dst;
	} caller;
	ssize_t ret;
};

static void tdgram_sendto_queue_trigger(struct tevent_req *req,
					 void *private_data);
static void tdgram_sendto_queue_done(struct tevent_req *subreq);

struct tevent_req *tdgram_sendto_queue_send(TALLOC_CTX *mem_ctx,
					    struct tevent_context *ev,
					    struct tdgram_context *dgram,
					    struct tevent_queue *queue,
					    const uint8_t *buf,
					    size_t len,
					    struct tsocket_address *dst)
{
	struct tevent_req *req;
	struct tdgram_sendto_queue_state *state;
	bool ok;

	req = tevent_req_create(mem_ctx, &state,
				struct tdgram_sendto_queue_state);
	if (!req) {
		return NULL;
	}

	state->caller.ev	= ev;
	state->caller.dgram	= dgram;
	state->caller.buf	= buf;
	state->caller.len	= len;
	state->caller.dst	= dst;
	state->ret		= -1;

	ok = tevent_queue_add(queue,
			      ev,
			      req,
			      tdgram_sendto_queue_trigger,
			      NULL);
	if (!ok) {
		tevent_req_nomem(NULL, req);
		goto post;
	}

	return req;

 post:
	tevent_req_post(req, ev);
	return req;
}

static void tdgram_sendto_queue_trigger(struct tevent_req *req,
					 void *private_data)
{
	struct tdgram_sendto_queue_state *state = tevent_req_data(req,
					struct tdgram_sendto_queue_state);
	struct tevent_req *subreq;

	subreq = tdgram_sendto_send(state,
				    state->caller.ev,
				    state->caller.dgram,
				    state->caller.buf,
				    state->caller.len,
				    state->caller.dst);
	if (tevent_req_nomem(subreq, req)) {
		return;
	}
	tevent_req_set_callback(subreq, tdgram_sendto_queue_done, req);
}

static void tdgram_sendto_queue_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(subreq,
				 struct tevent_req);
	struct tdgram_sendto_queue_state *state = tevent_req_data(req,
					struct tdgram_sendto_queue_state);
	ssize_t ret;
	int sys_errno;

	ret = tdgram_sendto_recv(subreq, &sys_errno);
	talloc_free(subreq);
	if (ret == -1) {
		tevent_req_error(req, sys_errno);
		return;
	}
	state->ret = ret;

	tevent_req_done(req);
}

ssize_t tdgram_sendto_queue_recv(struct tevent_req *req, int *perrno)
{
	struct tdgram_sendto_queue_state *state = tevent_req_data(req,
					struct tdgram_sendto_queue_state);
	ssize_t ret;

	ret = tsocket_simple_int_recv(req, perrno);
	if (ret == 0) {
		ret = state->ret;
	}

	tevent_req_received(req);
	return ret;
}

struct tstream_readv_pdu_state {
	/* this structs are owned by the caller */
	struct {
		struct tevent_context *ev;
		struct tstream_context *stream;
		tstream_readv_pdu_next_vector_t next_vector_fn;
		void *next_vector_private;
	} caller;

	/*
	 * Each call to the callback resets iov and count
	 * the callback allocated the iov as child of our state,
	 * that means we are allowed to modify and free it.
	 *
	 * we should call the callback every time we filled the given
	 * vector and ask for a new vector. We return if the callback
	 * ask for 0 bytes.
	 */
	struct iovec *vector;
	size_t count;

	/*
	 * the total number of bytes we read,
	 * the return value of the _recv function
	 */
	int total_read;
};

static void tstream_readv_pdu_ask_for_next_vector(struct tevent_req *req);
static void tstream_readv_pdu_readv_done(struct tevent_req *subreq);

struct tevent_req *tstream_readv_pdu_send(TALLOC_CTX *mem_ctx,
				struct tevent_context *ev,
				struct tstream_context *stream,
				tstream_readv_pdu_next_vector_t next_vector_fn,
				void *next_vector_private)
{
	struct tevent_req *req;
	struct tstream_readv_pdu_state *state;

	req = tevent_req_create(mem_ctx, &state,
				struct tstream_readv_pdu_state);
	if (!req) {
		return NULL;
	}

	state->caller.ev			= ev;
	state->caller.stream			= stream;
	state->caller.next_vector_fn		= next_vector_fn;
	state->caller.next_vector_private	= next_vector_private;

	state->vector		= NULL;
	state->count		= 0;
	state->total_read	= 0;

	tstream_readv_pdu_ask_for_next_vector(req);
	if (!tevent_req_is_in_progress(req)) {
		goto post;
	}

	return req;

 post:
	return tevent_req_post(req, ev);
}

static void tstream_readv_pdu_ask_for_next_vector(struct tevent_req *req)
{
	struct tstream_readv_pdu_state *state = tevent_req_data(req,
					    struct tstream_readv_pdu_state);
	int ret;
	size_t to_read = 0;
	size_t i;
	struct tevent_req *subreq;
	bool optimize = false;
	bool save_optimize = false;

	if (state->count > 0) {
		/*
		 * This is not the first time we asked for a vector,
		 * which means parts of the pdu already arrived.
		 *
		 * In this case it make sense to enable
		 * a syscall/performance optimization if the
		 * low level tstream implementation supports it.
		 */
		optimize = true;
	}

	TALLOC_FREE(state->vector);
	state->count = 0;

	ret = state->caller.next_vector_fn(state->caller.stream,
					   state->caller.next_vector_private,
					   state, &state->vector, &state->count);
	if (ret == -1) {
		tevent_req_error(req, errno);
		return;
	}

	if (state->count == 0) {
		tevent_req_done(req);
		return;
	}

	for (i=0; i < state->count; i++) {
		size_t tmp = to_read;
		tmp += state->vector[i].iov_len;

		if (tmp < to_read) {
			tevent_req_error(req, EMSGSIZE);
			return;
		}

		to_read = tmp;
	}

	/*
	 * this is invalid the next vector function should have
	 * reported count == 0.
	 */
	if (to_read == 0) {
		tevent_req_error(req, EINVAL);
		return;
	}

	if (state->total_read + to_read < state->total_read) {
		tevent_req_error(req, EMSGSIZE);
		return;
	}

	if (optimize) {
		/*
		 * If the low level stream is a bsd socket
		 * we will get syscall optimization.
		 *
		 * If it is not a bsd socket
		 * tstream_bsd_optimize_readv() just returns.
		 */
		save_optimize = tstream_bsd_optimize_readv(state->caller.stream,
							   true);
	}
	subreq = tstream_readv_send(state,
				    state->caller.ev,
				    state->caller.stream,
				    state->vector,
				    state->count);
	if (optimize) {
		tstream_bsd_optimize_readv(state->caller.stream,
					   save_optimize);
	}
	if (tevent_req_nomem(subreq, req)) {
		return;
	}
	tevent_req_set_callback(subreq, tstream_readv_pdu_readv_done, req);
}

static void tstream_readv_pdu_readv_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(subreq,
				 struct tevent_req);
	struct tstream_readv_pdu_state *state = tevent_req_data(req,
					    struct tstream_readv_pdu_state);
	int ret;
	int sys_errno;

	ret = tstream_readv_recv(subreq, &sys_errno);
	if (ret == -1) {
		tevent_req_error(req, sys_errno);
		return;
	}

	state->total_read += ret;

	/* ask the callback for a new vector we should fill */
	tstream_readv_pdu_ask_for_next_vector(req);
}

int tstream_readv_pdu_recv(struct tevent_req *req, int *perrno)
{
	struct tstream_readv_pdu_state *state = tevent_req_data(req,
					    struct tstream_readv_pdu_state);
	int ret;

	ret = tsocket_simple_int_recv(req, perrno);
	if (ret == 0) {
		ret = state->total_read;
	}

	tevent_req_received(req);
	return ret;
}

struct tstream_readv_pdu_queue_state {
	/* this structs are owned by the caller */
	struct {
		struct tevent_context *ev;
		struct tstream_context *stream;
		tstream_readv_pdu_next_vector_t next_vector_fn;
		void *next_vector_private;
	} caller;
	int ret;
};

static void tstream_readv_pdu_queue_trigger(struct tevent_req *req,
					 void *private_data);
static void tstream_readv_pdu_queue_done(struct tevent_req *subreq);

struct tevent_req *tstream_readv_pdu_queue_send(TALLOC_CTX *mem_ctx,
				struct tevent_context *ev,
				struct tstream_context *stream,
				struct tevent_queue *queue,
				tstream_readv_pdu_next_vector_t next_vector_fn,
				void *next_vector_private)
{
	struct tevent_req *req;
	struct tstream_readv_pdu_queue_state *state;
	bool ok;

	req = tevent_req_create(mem_ctx, &state,
				struct tstream_readv_pdu_queue_state);
	if (!req) {
		return NULL;
	}

	state->caller.ev			= ev;
	state->caller.stream			= stream;
	state->caller.next_vector_fn		= next_vector_fn;
	state->caller.next_vector_private	= next_vector_private;
	state->ret				= -1;

	ok = tevent_queue_add(queue,
			      ev,
			      req,
			      tstream_readv_pdu_queue_trigger,
			      NULL);
	if (!ok) {
		tevent_req_nomem(NULL, req);
		goto post;
	}

	return req;

 post:
	return tevent_req_post(req, ev);
}

static void tstream_readv_pdu_queue_trigger(struct tevent_req *req,
					 void *private_data)
{
	struct tstream_readv_pdu_queue_state *state = tevent_req_data(req,
					struct tstream_readv_pdu_queue_state);
	struct tevent_req *subreq;

	subreq = tstream_readv_pdu_send(state,
					state->caller.ev,
					state->caller.stream,
					state->caller.next_vector_fn,
					state->caller.next_vector_private);
	if (tevent_req_nomem(subreq, req)) {
		return;
	}
	tevent_req_set_callback(subreq, tstream_readv_pdu_queue_done ,req);
}

static void tstream_readv_pdu_queue_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(subreq,
				 struct tevent_req);
	struct tstream_readv_pdu_queue_state *state = tevent_req_data(req,
					struct tstream_readv_pdu_queue_state);
	int ret;
	int sys_errno;

	ret = tstream_readv_pdu_recv(subreq, &sys_errno);
	talloc_free(subreq);
	if (ret == -1) {
		tevent_req_error(req, sys_errno);
		return;
	}
	state->ret = ret;

	tevent_req_done(req);
}

int tstream_readv_pdu_queue_recv(struct tevent_req *req, int *perrno)
{
	struct tstream_readv_pdu_queue_state *state = tevent_req_data(req,
					struct tstream_readv_pdu_queue_state);
	int ret;

	ret = tsocket_simple_int_recv(req, perrno);
	if (ret == 0) {
		ret = state->ret;
	}

	tevent_req_received(req);
	return ret;
}

struct tstream_writev_queue_state {
	/* this structs are owned by the caller */
	struct {
		struct tevent_context *ev;
		struct tstream_context *stream;
		const struct iovec *vector;
		size_t count;
	} caller;
	int ret;
};

static void tstream_writev_queue_trigger(struct tevent_req *req,
					 void *private_data);
static void tstream_writev_queue_done(struct tevent_req *subreq);

struct tevent_req *tstream_writev_queue_send(TALLOC_CTX *mem_ctx,
					     struct tevent_context *ev,
					     struct tstream_context *stream,
					     struct tevent_queue *queue,
					     const struct iovec *vector,
					     size_t count)
{
	struct tevent_req *req;
	struct tstream_writev_queue_state *state;
	bool ok;

	req = tevent_req_create(mem_ctx, &state,
				struct tstream_writev_queue_state);
	if (!req) {
		return NULL;
	}

	state->caller.ev	= ev;
	state->caller.stream	= stream;
	state->caller.vector	= vector;
	state->caller.count	= count;
	state->ret		= -1;

	ok = tevent_queue_add(queue,
			      ev,
			      req,
			      tstream_writev_queue_trigger,
			      NULL);
	if (!ok) {
		tevent_req_nomem(NULL, req);
		goto post;
	}

	return req;

 post:
	return tevent_req_post(req, ev);
}

static void tstream_writev_queue_trigger(struct tevent_req *req,
					 void *private_data)
{
	struct tstream_writev_queue_state *state = tevent_req_data(req,
					struct tstream_writev_queue_state);
	struct tevent_req *subreq;

	subreq = tstream_writev_send(state,
				     state->caller.ev,
				     state->caller.stream,
				     state->caller.vector,
				     state->caller.count);
	if (tevent_req_nomem(subreq, req)) {
		return;
	}
	tevent_req_set_callback(subreq, tstream_writev_queue_done ,req);
}

static void tstream_writev_queue_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(subreq,
				 struct tevent_req);
	struct tstream_writev_queue_state *state = tevent_req_data(req,
					struct tstream_writev_queue_state);
	int ret;
	int sys_errno;

	ret = tstream_writev_recv(subreq, &sys_errno);
	talloc_free(subreq);
	if (ret == -1) {
		tevent_req_error(req, sys_errno);
		return;
	}
	state->ret = ret;

	tevent_req_done(req);
}

int tstream_writev_queue_recv(struct tevent_req *req, int *perrno)
{
	struct tstream_writev_queue_state *state = tevent_req_data(req,
					struct tstream_writev_queue_state);
	int ret;

	ret = tsocket_simple_int_recv(req, perrno);
	if (ret == 0) {
		ret = state->ret;
	}

	tevent_req_received(req);
	return ret;
}

