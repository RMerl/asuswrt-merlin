/*
 *  Unix SMB/CIFS implementation.
 *  RPC client transport over tstream
 *  Copyright (C) Simo Sorce 2010
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "includes.h"
#include "../lib/util/tevent_ntstatus.h"
#include "rpc_client/rpc_transport.h"
#include "lib/tsocket/tsocket.h"
#include "libsmb/cli_np_tstream.h"
#include "cli_pipe.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_RPC_CLI

struct rpc_tstream_state {
	struct tstream_context *stream;
	struct tevent_queue *read_queue;
	struct tevent_queue *write_queue;
	unsigned int timeout;
};

static void rpc_tstream_disconnect(struct rpc_tstream_state *s)
{
	TALLOC_FREE(s->stream);
}

static bool rpc_tstream_is_connected(void *priv)
{
	struct rpc_tstream_state *transp =
		talloc_get_type_abort(priv, struct rpc_tstream_state);
	ssize_t ret;

	if (!transp->stream) {
		return false;
	}

	if (!tstream_is_cli_np(transp->stream)) {
		return true;
	}

	ret = tstream_pending_bytes(transp->stream);
	if (ret == -1) {
		return false;
	}

	return true;
}

static unsigned int rpc_tstream_set_timeout(void *priv, unsigned int timeout)
{
	struct rpc_tstream_state *transp =
		talloc_get_type_abort(priv, struct rpc_tstream_state);
	int orig_timeout;
	bool ok;

	ok = rpc_tstream_is_connected(transp);
	if (!ok) {
		return 0;
	}

	if (tstream_is_cli_np(transp->stream)) {
		transp->timeout = timeout;
		return tstream_cli_np_set_timeout(transp->stream, timeout);
	}

	orig_timeout = transp->timeout;

	transp->timeout = timeout;

	return orig_timeout;
}

struct rpc_tstream_next_vector_state {
	uint8_t *buf;
	size_t len;
	off_t ofs;
	size_t remaining;
};

static void rpc_tstream_next_vector_init(
				struct rpc_tstream_next_vector_state *s,
				uint8_t *buf, size_t len)
{
	ZERO_STRUCTP(s);

	s->buf = buf;
	s->len = MIN(len, UINT16_MAX);
}

static int rpc_tstream_next_vector(struct tstream_context *stream,
				   void *private_data,
				   TALLOC_CTX *mem_ctx,
				   struct iovec **_vector,
				   size_t *count)
{
	struct rpc_tstream_next_vector_state *state =
		(struct rpc_tstream_next_vector_state *)private_data;
	struct iovec *vector;
	ssize_t pending;
	size_t wanted;

	if (state->ofs == state->len) {
		*_vector = NULL;
		*count = 0;
		return 0;
	}

	pending = tstream_pending_bytes(stream);
	if (pending == -1) {
		return -1;
	}

	if (pending == 0 && state->ofs != 0) {
		/* return a short read */
		*_vector = NULL;
		*count = 0;
		return 0;
	}

	if (pending == 0) {
		/* we want at least one byte and recheck again */
		wanted = 1;
	} else {
		size_t missing = state->len - state->ofs;
		if (pending > missing) {
			/* there's more available */
			state->remaining = pending - missing;
			wanted = missing;
		} else {
			/* read what we can get and recheck in the next cycle */
			wanted = pending;
		}
	}

	vector = talloc_array(mem_ctx, struct iovec, 1);
	if (!vector) {
		return -1;
	}

	vector[0].iov_base = state->buf + state->ofs;
	vector[0].iov_len = wanted;

	state->ofs += wanted;

	*_vector = vector;
	*count = 1;
	return 0;
}

struct rpc_tstream_read_state {
	struct rpc_tstream_state *transp;
	struct rpc_tstream_next_vector_state next_vector;
	ssize_t nread;
};

static void rpc_tstream_read_done(struct tevent_req *subreq);

static struct tevent_req *rpc_tstream_read_send(TALLOC_CTX *mem_ctx,
					     struct event_context *ev,
					     uint8_t *data, size_t size,
					     void *priv)
{
	struct rpc_tstream_state *transp =
		talloc_get_type_abort(priv, struct rpc_tstream_state);
	struct tevent_req *req, *subreq;
	struct rpc_tstream_read_state *state;
	struct timeval endtime;

	req = tevent_req_create(mem_ctx, &state, struct rpc_tstream_read_state);
	if (req == NULL) {
		return NULL;
	}
	if (!rpc_tstream_is_connected(transp)) {
		tevent_req_nterror(req, NT_STATUS_CONNECTION_INVALID);
		return tevent_req_post(req, ev);
	}
	state->transp = transp;
	rpc_tstream_next_vector_init(&state->next_vector, data, size);

	subreq = tstream_readv_pdu_queue_send(state, ev,
					      transp->stream,
					      transp->read_queue,
					      rpc_tstream_next_vector,
					      &state->next_vector);
	if (subreq == NULL) {
		tevent_req_nterror(req, NT_STATUS_NO_MEMORY);
		return tevent_req_post(req, ev);
	}

	endtime = timeval_current_ofs(0, transp->timeout * 1000);
	if (!tevent_req_set_endtime(subreq, ev, endtime)) {
		goto fail;
	}

	tevent_req_set_callback(subreq, rpc_tstream_read_done, req);
	return req;
 fail:
	TALLOC_FREE(req);
	return NULL;
}

static void rpc_tstream_read_done(struct tevent_req *subreq)
{
	struct tevent_req *req =
		tevent_req_callback_data(subreq, struct tevent_req);
	struct rpc_tstream_read_state *state =
		tevent_req_data(req, struct rpc_tstream_read_state);
	int err;

	state->nread = tstream_readv_pdu_queue_recv(subreq, &err);
	TALLOC_FREE(subreq);
	if (state->nread < 0) {
		rpc_tstream_disconnect(state->transp);
		tevent_req_nterror(req, map_nt_error_from_unix(err));
		return;
	}
	tevent_req_done(req);
}

static NTSTATUS rpc_tstream_read_recv(struct tevent_req *req, ssize_t *size)
{
	struct rpc_tstream_read_state *state = tevent_req_data(
		req, struct rpc_tstream_read_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		return status;
	}
	*size = state->nread;
	return NT_STATUS_OK;
}

struct rpc_tstream_write_state {
	struct event_context *ev;
	struct rpc_tstream_state *transp;
	struct iovec iov;
	ssize_t nwritten;
};

static void rpc_tstream_write_done(struct tevent_req *subreq);

static struct tevent_req *rpc_tstream_write_send(TALLOC_CTX *mem_ctx,
					      struct event_context *ev,
					      const uint8_t *data, size_t size,
					      void *priv)
{
	struct rpc_tstream_state *transp =
		talloc_get_type_abort(priv, struct rpc_tstream_state);
	struct tevent_req *req, *subreq;
	struct rpc_tstream_write_state *state;
	struct timeval endtime;

	req = tevent_req_create(mem_ctx, &state, struct rpc_tstream_write_state);
	if (req == NULL) {
		return NULL;
	}
	if (!rpc_tstream_is_connected(transp)) {
		tevent_req_nterror(req, NT_STATUS_CONNECTION_INVALID);
		return tevent_req_post(req, ev);
	}
	state->ev = ev;
	state->transp = transp;
	state->iov.iov_base = discard_const_p(void *, data);
	state->iov.iov_len = size;

	subreq = tstream_writev_queue_send(state, ev,
					   transp->stream,
					   transp->write_queue,
					   &state->iov, 1);
	if (subreq == NULL) {
		goto fail;
	}

	endtime = timeval_current_ofs(0, transp->timeout * 1000);
	if (!tevent_req_set_endtime(subreq, ev, endtime)) {
		goto fail;
	}

	tevent_req_set_callback(subreq, rpc_tstream_write_done, req);
	return req;
 fail:
	TALLOC_FREE(req);
	return NULL;
}

static void rpc_tstream_write_done(struct tevent_req *subreq)
{
	struct tevent_req *req =
		tevent_req_callback_data(subreq, struct tevent_req);
	struct rpc_tstream_write_state *state =
		tevent_req_data(req, struct rpc_tstream_write_state);
	int err;

	state->nwritten = tstream_writev_queue_recv(subreq, &err);
	TALLOC_FREE(subreq);
	if (state->nwritten < 0) {
		rpc_tstream_disconnect(state->transp);
		tevent_req_nterror(req, map_nt_error_from_unix(err));
		return;
	}
	tevent_req_done(req);
}

static NTSTATUS rpc_tstream_write_recv(struct tevent_req *req, ssize_t *sent)
{
	struct rpc_tstream_write_state *state =
		tevent_req_data(req, struct rpc_tstream_write_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		return status;
	}
	*sent = state->nwritten;
	return NT_STATUS_OK;
}

struct rpc_tstream_trans_state {
	struct tevent_context *ev;
	struct rpc_tstream_state *transp;
	struct iovec req;
	uint32_t max_rdata_len;
	struct iovec rep;
};

static void rpc_tstream_trans_writev(struct tevent_req *subreq);
static void rpc_tstream_trans_readv_pdu(struct tevent_req *subreq);

static int rpc_tstream_trans_next_vector(struct tstream_context *stream,
					 void *private_data,
					 TALLOC_CTX *mem_ctx,
					 struct iovec **_vector,
					 size_t *count);

static struct tevent_req *rpc_tstream_trans_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 uint8_t *data, size_t data_len,
						 uint32_t max_rdata_len,
						 void *priv)
{
	struct rpc_tstream_state *transp =
		talloc_get_type_abort(priv, struct rpc_tstream_state);
	struct tevent_req *req, *subreq;
	struct rpc_tstream_trans_state *state;
	struct timeval endtime;

	req = tevent_req_create(mem_ctx, &state,
				struct rpc_tstream_trans_state);
	if (req == NULL) {
		return NULL;
	}

	if (!rpc_tstream_is_connected(transp)) {
		tevent_req_nterror(req, NT_STATUS_CONNECTION_INVALID);
		return tevent_req_post(req, ev);
	}
	state->ev = ev;
	state->transp = transp;
	state->req.iov_len = data_len;
	state->req.iov_base = discard_const_p(void *, data);
	state->max_rdata_len = max_rdata_len;

	endtime = timeval_current_ofs(0, transp->timeout * 1000);

	subreq = tstream_writev_queue_send(state, ev,
					   transp->stream,
					   transp->write_queue,
					   &state->req, 1);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	if (!tevent_req_set_endtime(subreq, ev, endtime)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, rpc_tstream_trans_writev, req);

	if (tstream_is_cli_np(transp->stream)) {
		tstream_cli_np_use_trans(transp->stream);
	}

	subreq = tstream_readv_pdu_queue_send(state, ev,
					      transp->stream,
					      transp->read_queue,
					      rpc_tstream_trans_next_vector,
					      state);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	if (!tevent_req_set_endtime(subreq, ev, endtime)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, rpc_tstream_trans_readv_pdu, req);

	return req;
}

static void rpc_tstream_trans_writev(struct tevent_req *subreq)
{
	struct tevent_req *req =
		tevent_req_callback_data(subreq,
		struct tevent_req);
	struct rpc_tstream_trans_state *state =
		tevent_req_data(req,
		struct rpc_tstream_trans_state);
	int ret;
	int err;

	ret = tstream_writev_queue_recv(subreq, &err);
	TALLOC_FREE(subreq);
	if (ret == -1) {
		rpc_tstream_disconnect(state->transp);
		tevent_req_nterror(req, map_nt_error_from_unix(err));
		return;
	}
}

static int rpc_tstream_trans_next_vector(struct tstream_context *stream,
					 void *private_data,
					 TALLOC_CTX *mem_ctx,
					 struct iovec **_vector,
					 size_t *count)
{
	struct rpc_tstream_trans_state *state =
		talloc_get_type_abort(private_data,
		struct rpc_tstream_trans_state);
	struct iovec *vector;

	if (state->max_rdata_len == state->rep.iov_len) {
		*_vector = NULL;
		*count = 0;
		return 0;
	}

	state->rep.iov_base = talloc_array(state, uint8_t,
					   state->max_rdata_len);
	if (state->rep.iov_base == NULL) {
		return -1;
	}
	state->rep.iov_len = state->max_rdata_len;

	vector = talloc_array(mem_ctx, struct iovec, 1);
	if (!vector) {
		return -1;
	}

	vector[0] = state->rep;

	*_vector = vector;
	*count = 1;
	return 0;
}

static void rpc_tstream_trans_readv_pdu(struct tevent_req *subreq)
{
	struct tevent_req *req =
		tevent_req_callback_data(subreq,
		struct tevent_req);
	struct rpc_tstream_trans_state *state =
		tevent_req_data(req,
		struct rpc_tstream_trans_state);
	int ret;
	int err;

	ret = tstream_readv_pdu_queue_recv(subreq, &err);
	TALLOC_FREE(subreq);
	if (ret == -1) {
		rpc_tstream_disconnect(state->transp);
		tevent_req_nterror(req, map_nt_error_from_unix(err));
		return;
	}

	tevent_req_done(req);
}

static NTSTATUS rpc_tstream_trans_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx,
				       uint8_t **prdata, uint32_t *prdata_len)
{
	struct rpc_tstream_trans_state *state =
		tevent_req_data(req,
		struct rpc_tstream_trans_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		return status;
	}

	*prdata = (uint8_t *)talloc_move(mem_ctx, &state->rep.iov_base);
	*prdata_len = state->rep.iov_len;
	return NT_STATUS_OK;
}

/**
* @brief Initialize a tstream transport facility
*	 NOTE: this function will talloc_steal, the stream and the queues.
*
* @param mem_ctx	- memory context used to allocate the transport
* @param stream		- a ready to use tstream
* @param presult	- the transport structure
*
* @return		- a NT Status error code.
*/
NTSTATUS rpc_transport_tstream_init(TALLOC_CTX *mem_ctx,
				struct tstream_context **stream,
				struct rpc_cli_transport **presult)
{
	struct rpc_cli_transport *result;
	struct rpc_tstream_state *state;

	result = talloc(mem_ctx, struct rpc_cli_transport);
	if (result == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	state = talloc(result, struct rpc_tstream_state);
	if (state == NULL) {
		TALLOC_FREE(result);
		return NT_STATUS_NO_MEMORY;
	}
	result->priv = state;

	state->read_queue = tevent_queue_create(state, "read_queue");
	if (state->read_queue == NULL) {
		TALLOC_FREE(result);
		return NT_STATUS_NO_MEMORY;
	}
	state->write_queue = tevent_queue_create(state, "write_queue");
	if (state->write_queue == NULL) {
		TALLOC_FREE(result);
		return NT_STATUS_NO_MEMORY;
	}

	state->stream = talloc_move(state, stream);
	state->timeout = 10000; /* 10 seconds. */

	if (tstream_is_cli_np(state->stream)) {
		result->trans_send = rpc_tstream_trans_send;
		result->trans_recv = rpc_tstream_trans_recv;
	} else {
		result->trans_send = NULL;
		result->trans_recv = NULL;
	}
	result->write_send = rpc_tstream_write_send;
	result->write_recv = rpc_tstream_write_recv;
	result->read_send = rpc_tstream_read_send;
	result->read_recv = rpc_tstream_read_recv;
	result->is_connected = rpc_tstream_is_connected;
	result->set_timeout = rpc_tstream_set_timeout;

	*presult = result;
	return NT_STATUS_OK;
}

struct cli_state *rpc_pipe_np_smb_conn(struct rpc_pipe_client *p)
{
	struct rpc_tstream_state *transp =
		talloc_get_type_abort(p->transport->priv,
		struct rpc_tstream_state);
	bool ok;

	ok = rpccli_is_connected(p);
	if (!ok) {
		return NULL;
	}

	if (!tstream_is_cli_np(transp->stream)) {
		return NULL;
	}

	return tstream_cli_np_get_cli_state(transp->stream);
}
