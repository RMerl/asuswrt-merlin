/*
   Unix SMB/CIFS implementation.

   tstream based generic authentication interface

   Copyright (c) 2010 Stefan Metzmacher
   Copyright (c) 2010 Andreas Schneider <asn@redhat.com>

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
#include "system/network.h"
#include "auth/gensec/gensec.h"
#include "auth/gensec/gensec_proto.h"
#include "auth/gensec/gensec_tstream.h"
#include "lib/tsocket/tsocket.h"
#include "lib/tsocket/tsocket_internal.h"


static const struct tstream_context_ops tstream_gensec_ops;

struct tstream_gensec {
	struct tstream_context *plain_stream;

	struct gensec_security *gensec_security;

	int error;

	struct {
		size_t max_unwrapped_size;
		size_t max_wrapped_size;
	} write;

	struct {
		off_t ofs;
		size_t left;
		DATA_BLOB unwrapped;
	} read;
};

_PUBLIC_ NTSTATUS _gensec_create_tstream(TALLOC_CTX *mem_ctx,
					 struct gensec_security *gensec_security,
					 struct tstream_context *plain_stream,
					 struct tstream_context **_gensec_stream,
					 const char *location)
{
	struct tstream_context *gensec_stream;
	struct tstream_gensec *tgss;

	gensec_stream = tstream_context_create(mem_ctx,
					       &tstream_gensec_ops,
					       &tgss,
					       struct tstream_gensec,
					       location);
	if (gensec_stream == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	tgss->plain_stream = plain_stream;
	tgss->gensec_security = gensec_security;
	tgss->error = 0;

	if (!gensec_have_feature(gensec_security, GENSEC_FEATURE_SIGN) &&
	    !gensec_have_feature(gensec_security, GENSEC_FEATURE_SEAL)) {
		talloc_free(gensec_stream);
		return NT_STATUS_INVALID_PARAMETER;
	}

	tgss->write.max_unwrapped_size = gensec_max_input_size(gensec_security);
	tgss->write.max_wrapped_size = gensec_max_wrapped_size(gensec_security);

	ZERO_STRUCT(tgss->read);

	*_gensec_stream = gensec_stream;
	return NT_STATUS_OK;
}

static ssize_t tstream_gensec_pending_bytes(struct tstream_context *stream)
{
	struct tstream_gensec *tgss =
		tstream_context_data(stream,
		struct tstream_gensec);

	if (tgss->error != 0) {
		errno = tgss->error;
		return -1;
	}

	return tgss->read.left;
}

struct tstream_gensec_readv_state {
	struct tevent_context *ev;
	struct tstream_context *stream;

	struct iovec *vector;
	int count;

	struct {
		bool asked_for_hdr;
		uint8_t hdr[4];
		bool asked_for_blob;
		DATA_BLOB blob;
	} wrapped;

	int ret;
};

static void tstream_gensec_readv_wrapped_next(struct tevent_req *req);

static struct tevent_req *tstream_gensec_readv_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct tstream_context *stream,
						    struct iovec *vector,
						    size_t count)
{
	struct tstream_gensec *tgss =
		tstream_context_data(stream,
		struct tstream_gensec);
	struct tevent_req *req;
	struct tstream_gensec_readv_state *state;

	req = tevent_req_create(mem_ctx, &state,
				struct tstream_gensec_readv_state);
	if (!req) {
		return NULL;
	}

	if (tgss->error != 0) {
		tevent_req_error(req, tgss->error);
		return tevent_req_post(req, ev);
	}

	state->ev = ev;
	state->stream = stream;
	state->ret = 0;

	/*
	 * we make a copy of the vector so we can change the structure
	 */
	state->vector = talloc_array(state, struct iovec, count);
	if (tevent_req_nomem(state->vector, req)) {
		return tevent_req_post(req, ev);
	}
	memcpy(state->vector, vector, sizeof(struct iovec) * count);
	state->count = count;

	tstream_gensec_readv_wrapped_next(req);
	if (!tevent_req_is_in_progress(req)) {
		return tevent_req_post(req, ev);
	}

	return req;
}

static int tstream_gensec_readv_next_vector(struct tstream_context *unix_stream,
					    void *private_data,
					    TALLOC_CTX *mem_ctx,
					    struct iovec **_vector,
					    size_t *_count);
static void tstream_gensec_readv_wrapped_done(struct tevent_req *subreq);

static void tstream_gensec_readv_wrapped_next(struct tevent_req *req)
{
	struct tstream_gensec_readv_state *state =
		tevent_req_data(req,
		struct tstream_gensec_readv_state);
	struct tstream_gensec *tgss =
		tstream_context_data(state->stream,
		struct tstream_gensec);
	struct tevent_req *subreq;

	/*
	 * copy the pending buffer first
	 */
	while (tgss->read.left > 0 && state->count > 0) {
		uint8_t *base = (uint8_t *)state->vector[0].iov_base;
		size_t len = MIN(tgss->read.left, state->vector[0].iov_len);

		memcpy(base, tgss->read.unwrapped.data + tgss->read.ofs, len);

		base += len;
		state->vector[0].iov_base = (char *) base;
		state->vector[0].iov_len -= len;

		tgss->read.ofs += len;
		tgss->read.left -= len;

		if (state->vector[0].iov_len == 0) {
			state->vector += 1;
			state->count -= 1;
		}

		state->ret += len;
	}

	if (state->count == 0) {
		tevent_req_done(req);
		return;
	}

	data_blob_free(&tgss->read.unwrapped);
	ZERO_STRUCT(state->wrapped);

	subreq = tstream_readv_pdu_send(state, state->ev,
					tgss->plain_stream,
					tstream_gensec_readv_next_vector,
					state);
	if (tevent_req_nomem(subreq, req)) {
		return;
	}
	tevent_req_set_callback(subreq, tstream_gensec_readv_wrapped_done, req);
}

static int tstream_gensec_readv_next_vector(struct tstream_context *unix_stream,
					    void *private_data,
					    TALLOC_CTX *mem_ctx,
					    struct iovec **_vector,
					    size_t *_count)
{
	struct tstream_gensec_readv_state *state =
		talloc_get_type_abort(private_data,
		struct tstream_gensec_readv_state);
	struct iovec *vector;
	size_t count = 1;

	/* we need to get a message header */
	vector = talloc_array(mem_ctx, struct iovec, count);
	if (!vector) {
		return -1;
	}

	if (!state->wrapped.asked_for_hdr) {
		state->wrapped.asked_for_hdr = true;
		vector[0].iov_base = (char *)state->wrapped.hdr;
		vector[0].iov_len = sizeof(state->wrapped.hdr);
	} else if (!state->wrapped.asked_for_blob) {
		state->wrapped.asked_for_blob = true;
		uint32_t msg_len;

		msg_len = RIVAL(state->wrapped.hdr, 0);

		if (msg_len > 0x00FFFFFF) {
			errno = EMSGSIZE;
			return -1;
		}

		if (msg_len == 0) {
			errno = EMSGSIZE;
			return -1;
		}

		state->wrapped.blob = data_blob_talloc(state, NULL, msg_len);
		if (state->wrapped.blob.data == NULL) {
			return -1;
		}

		vector[0].iov_base = (char *)state->wrapped.blob.data;
		vector[0].iov_len = state->wrapped.blob.length;
	} else {
		*_vector = NULL;
		*_count = 0;
		return 0;
	}

	*_vector = vector;
	*_count = count;
	return 0;
}

static void tstream_gensec_readv_wrapped_done(struct tevent_req *subreq)
{
	struct tevent_req *req =
		tevent_req_callback_data(subreq,
		struct tevent_req);
	struct tstream_gensec_readv_state *state =
		tevent_req_data(req,
		struct tstream_gensec_readv_state);
	struct tstream_gensec *tgss =
		tstream_context_data(state->stream,
		struct tstream_gensec);
	int ret;
	int sys_errno;
	NTSTATUS status;

	ret = tstream_readv_pdu_recv(subreq, &sys_errno);
	TALLOC_FREE(subreq);
	if (ret == -1) {
		tgss->error = sys_errno;
		tevent_req_error(req, sys_errno);
		return;
	}

	status = gensec_unwrap(tgss->gensec_security,
			       state,
			       &state->wrapped.blob,
			       &tgss->read.unwrapped);
	if (!NT_STATUS_IS_OK(status)) {
		tgss->error = EIO;
		tevent_req_error(req, tgss->error);
		return;
	}

	data_blob_free(&state->wrapped.blob);

	talloc_steal(tgss, tgss->read.unwrapped.data);
	tgss->read.left = tgss->read.unwrapped.length;
	tgss->read.ofs = 0;

	tstream_gensec_readv_wrapped_next(req);
}

static int tstream_gensec_readv_recv(struct tevent_req *req, int *perrno)
{
	struct tstream_gensec_readv_state *state =
		tevent_req_data(req,
		struct tstream_gensec_readv_state);
	int ret;

	ret = tsocket_simple_int_recv(req, perrno);
	if (ret == 0) {
		ret = state->ret;
	}

	tevent_req_received(req);
	return ret;
}

struct tstream_gensec_writev_state {
	struct tevent_context *ev;
	struct tstream_context *stream;

	struct iovec *vector;
	int count;

	struct {
		off_t ofs;
		size_t left;
		DATA_BLOB blob;
	} unwrapped;

	struct {
		uint8_t hdr[4];
		DATA_BLOB blob;
		struct iovec iov[2];
	} wrapped;

	int ret;
};

static void tstream_gensec_writev_wrapped_next(struct tevent_req *req);

static struct tevent_req *tstream_gensec_writev_send(TALLOC_CTX *mem_ctx,
					struct tevent_context *ev,
					struct tstream_context *stream,
					const struct iovec *vector,
					size_t count)
{
	struct tstream_gensec *tgss =
		tstream_context_data(stream,
		struct tstream_gensec);
	struct tevent_req *req;
	struct tstream_gensec_writev_state *state;
	int i;
	int total;
	int chunk;

	req = tevent_req_create(mem_ctx, &state,
				struct tstream_gensec_writev_state);
	if (req == NULL) {
		return NULL;
	}

	if (tgss->error != 0) {
		tevent_req_error(req, tgss->error);
		return tevent_req_post(req, ev);
	}

	state->ev = ev;
	state->stream = stream;
	state->ret = 0;

	/*
	 * we make a copy of the vector so we can change the structure
	 */
	state->vector = talloc_array(state, struct iovec, count);
	if (tevent_req_nomem(state->vector, req)) {
		return tevent_req_post(req, ev);
	}
	memcpy(state->vector, vector, sizeof(struct iovec) * count);
	state->count = count;

	total = 0;
	for (i = 0; i < count; i++) {
		/*
		 * the generic tstream code makes sure that
		 * this never wraps.
		 */
		total += vector[i].iov_len;
	}

	/*
	 * We may need to send data in chunks.
	 */
	chunk = MIN(total, tgss->write.max_unwrapped_size);

	state->unwrapped.blob = data_blob_talloc(state, NULL, chunk);
	if (tevent_req_nomem(state->unwrapped.blob.data, req)) {
		return tevent_req_post(req, ev);
	}

	tstream_gensec_writev_wrapped_next(req);
	if (!tevent_req_is_in_progress(req)) {
		return tevent_req_post(req, ev);
	}

	return req;
}

static void tstream_gensec_writev_wrapped_done(struct tevent_req *subreq);

static void tstream_gensec_writev_wrapped_next(struct tevent_req *req)
{
	struct tstream_gensec_writev_state *state =
		tevent_req_data(req,
		struct tstream_gensec_writev_state);
	struct tstream_gensec *tgss =
		tstream_context_data(state->stream,
		struct tstream_gensec);
	struct tevent_req *subreq;
	NTSTATUS status;

	data_blob_free(&state->wrapped.blob);

	state->unwrapped.left = state->unwrapped.blob.length;
	state->unwrapped.ofs = 0;

	/*
	 * first fill our buffer
	 */
	while (state->unwrapped.left > 0 && state->count > 0) {
		uint8_t *base = (uint8_t *)state->vector[0].iov_base;
		size_t len = MIN(state->unwrapped.left, state->vector[0].iov_len);

		memcpy(state->unwrapped.blob.data + state->unwrapped.ofs, base, len);

		base += len;
		state->vector[0].iov_base = (char *) base;
		state->vector[0].iov_len -= len;

		state->unwrapped.ofs += len;
		state->unwrapped.left -= len;

		if (state->vector[0].iov_len == 0) {
			state->vector += 1;
			state->count -= 1;
		}

		state->ret += len;
	}

	if (state->unwrapped.ofs == 0) {
		tevent_req_done(req);
		return;
	}

	state->unwrapped.blob.length = state->unwrapped.ofs;

	status = gensec_wrap(tgss->gensec_security,
			     state,
			     &state->unwrapped.blob,
			     &state->wrapped.blob);
	if (!NT_STATUS_IS_OK(status)) {
		tgss->error = EIO;
		tevent_req_error(req, tgss->error);
		return;
	}

	RSIVAL(state->wrapped.hdr, 0, state->wrapped.blob.length);

	state->wrapped.iov[0].iov_base = (void *)state->wrapped.hdr;
	state->wrapped.iov[0].iov_len = sizeof(state->wrapped.hdr);
	state->wrapped.iov[1].iov_base = (void *)state->wrapped.blob.data;
	state->wrapped.iov[1].iov_len = state->wrapped.blob.length;

	subreq = tstream_writev_send(state, state->ev,
				      tgss->plain_stream,
				      state->wrapped.iov, 2);
	if (tevent_req_nomem(subreq, req)) {
		return;
	}
	tevent_req_set_callback(subreq,
				tstream_gensec_writev_wrapped_done,
				req);
}

static void tstream_gensec_writev_wrapped_done(struct tevent_req *subreq)
{
	struct tevent_req *req =
		tevent_req_callback_data(subreq,
		struct tevent_req);
	struct tstream_gensec_writev_state *state =
		tevent_req_data(req,
		struct tstream_gensec_writev_state);
	struct tstream_gensec *tgss =
		tstream_context_data(state->stream,
		struct tstream_gensec);
	int sys_errno;
	int ret;

	ret = tstream_writev_recv(subreq, &sys_errno);
	TALLOC_FREE(subreq);
	if (ret == -1) {
		tgss->error = sys_errno;
		tevent_req_error(req, sys_errno);
		return;
	}

	tstream_gensec_writev_wrapped_next(req);
}

static int tstream_gensec_writev_recv(struct tevent_req *req,
				   int *perrno)
{
	struct tstream_gensec_writev_state *state =
		tevent_req_data(req,
		struct tstream_gensec_writev_state);
	int ret;

	ret = tsocket_simple_int_recv(req, perrno);
	if (ret == 0) {
		ret = state->ret;
	}

	tevent_req_received(req);
	return ret;
}

struct tstream_gensec_disconnect_state {
	uint8_t _dummy;
};

static struct tevent_req *tstream_gensec_disconnect_send(TALLOC_CTX *mem_ctx,
							 struct tevent_context *ev,
							 struct tstream_context *stream)
{
	struct tstream_gensec *tgss =
		tstream_context_data(stream,
		struct tstream_gensec);
	struct tevent_req *req;
	struct tstream_gensec_disconnect_state *state;

	req = tevent_req_create(mem_ctx, &state,
				struct tstream_gensec_disconnect_state);
	if (req == NULL) {
		return NULL;
	}

	if (tgss->error != 0) {
		tevent_req_error(req, tgss->error);
		return tevent_req_post(req, ev);
	}

	/*
	 * The caller is responsible to do the real disconnect
	 * on the plain stream!
	 */
	tgss->plain_stream = NULL;
	tgss->error = ENOTCONN;

	tevent_req_done(req);
	return tevent_req_post(req, ev);
}

static int tstream_gensec_disconnect_recv(struct tevent_req *req,
				       int *perrno)
{
	int ret;

	ret = tsocket_simple_int_recv(req, perrno);

	tevent_req_received(req);
	return ret;
}

static const struct tstream_context_ops tstream_gensec_ops = {
	.name                   = "gensec",

	.pending_bytes          = tstream_gensec_pending_bytes,

	.readv_send             = tstream_gensec_readv_send,
	.readv_recv             = tstream_gensec_readv_recv,

	.writev_send            = tstream_gensec_writev_send,
	.writev_recv            = tstream_gensec_writev_recv,

	.disconnect_send        = tstream_gensec_disconnect_send,
	.disconnect_recv        = tstream_gensec_disconnect_recv,
};
