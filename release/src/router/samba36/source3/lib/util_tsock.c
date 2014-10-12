/*
   Unix SMB/CIFS implementation.
   Utilities around tsocket
   Copyright (C) Volker Lendecke 2009

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
#include "../lib/tsocket/tsocket.h"
#include "../lib/util/tevent_unix.h"

struct tstream_read_packet_state {
	struct tevent_context *ev;
	struct tstream_context *stream;
	ssize_t (*more)(uint8_t *buf, size_t buflen, void *private_data);
	void *private_data;
	uint8_t *buf;
	struct iovec iov;
};

static void tstream_read_packet_done(struct tevent_req *subreq);

struct tevent_req *tstream_read_packet_send(TALLOC_CTX *mem_ctx,
					    struct tevent_context *ev,
					    struct tstream_context *stream,
					    size_t initial,
					    ssize_t (*more)(uint8_t *buf,
							    size_t buflen,
							    void *private_data),
					    void *private_data)
{
	struct tevent_req *req, *subreq;
	struct tstream_read_packet_state *state;

	req = tevent_req_create(mem_ctx, &state,
				struct tstream_read_packet_state);
	if (req == NULL) {
		return NULL;
	}
	state->buf = talloc_array(state, uint8_t, initial);
	if (tevent_req_nomem(state->buf, req)) {
		return tevent_req_post(req, ev);
	}
	state->iov.iov_base = (void *)state->buf;
	state->iov.iov_len = initial;

	state->ev = ev;
	state->stream = stream;
	state->more = more;
	state->private_data = private_data;

	subreq = tstream_readv_send(state, ev, stream, &state->iov, 1);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, tstream_read_packet_done, req);

	return req;
}

static void tstream_read_packet_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct tstream_read_packet_state *state = tevent_req_data(
		req, struct tstream_read_packet_state);
	int ret, err;
	size_t total;
	ssize_t more;
	uint8_t *tmp;

	ret = tstream_readv_recv(subreq, &err);
	TALLOC_FREE(subreq);
	if (ret == 0) {
		err = EPIPE;
	}
	if (ret <= 0) {
		tevent_req_error(req, err);
		return;
	}

	if (state->more == NULL) {
		/* Nobody to ask, this is a async read_data */
		tevent_req_done(req);
		return;
	}
	total = talloc_array_length(state->buf);

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

	if (total + more < total) {
		tevent_req_error(req, EMSGSIZE);
		return;
	}

	tmp = talloc_realloc(state, state->buf, uint8_t, total+more);
	if (tevent_req_nomem(tmp, req)) {
		return;
	}
	state->buf = tmp;

	state->iov.iov_base = (void *)(state->buf + total);
	state->iov.iov_len = more;

	subreq = tstream_readv_send(state, state->ev, state->stream,
				    &state->iov, 1);
	if (tevent_req_nomem(subreq, req)) {
		return;
	}
	tevent_req_set_callback(subreq, tstream_read_packet_done, req);
}

ssize_t tstream_read_packet_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx,
				 uint8_t **pbuf, int *perrno)
{
	struct tstream_read_packet_state *state =
		tevent_req_data(req, struct tstream_read_packet_state);

	if (tevent_req_is_unix_error(req, perrno)) {
		return -1;
	}
	*pbuf = talloc_move(mem_ctx, &state->buf);
	return talloc_array_length(*pbuf);
}
