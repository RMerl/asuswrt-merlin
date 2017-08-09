/*
   Unix SMB/CIFS implementation.

   Async transfer of winbindd_request and _response structs

   Copyright (C) Volker Lendecke 2008

     ** NOTE! The following LGPL license applies to the wbclient
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
#include "system/filesys.h"
#include "system/network.h"
#include <talloc.h>
#include <tevent.h>
#include "lib/async_req/async_sock.h"
#include "lib/util/tevent_unix.h"
#include "nsswitch/winbind_struct_protocol.h"
#include "nsswitch/libwbclient/wbclient.h"
#include "nsswitch/wb_reqtrans.h"

/* can't use DEBUG here... */
#define DEBUG(a,b)

struct req_read_state {
	struct winbindd_request *wb_req;
	size_t max_extra_data;
	ssize_t ret;
};

static ssize_t wb_req_more(uint8_t *buf, size_t buflen, void *private_data);
static void wb_req_read_done(struct tevent_req *subreq);

struct tevent_req *wb_req_read_send(TALLOC_CTX *mem_ctx,
				    struct tevent_context *ev,
				    int fd, size_t max_extra_data)
{
	struct tevent_req *req, *subreq;
	struct req_read_state *state;

	req = tevent_req_create(mem_ctx, &state, struct req_read_state);
	if (req == NULL) {
		return NULL;
	}
	state->max_extra_data = max_extra_data;

	subreq = read_packet_send(state, ev, fd, 4, wb_req_more, state);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, wb_req_read_done, req);
	return req;
}

static ssize_t wb_req_more(uint8_t *buf, size_t buflen, void *private_data)
{
	struct req_read_state *state = talloc_get_type_abort(
		private_data, struct req_read_state);
	struct winbindd_request *req = (struct winbindd_request *)buf;

	if (buflen == 4) {
		if (req->length != sizeof(struct winbindd_request)) {
			DEBUG(0, ("wb_req_read_len: Invalid request size "
				  "received: %d (expected %d)\n",
				  (int)req->length,
				  (int)sizeof(struct winbindd_request)));
			return -1;
		}
		return sizeof(struct winbindd_request) - 4;
	}

	if (buflen > sizeof(struct winbindd_request)) {
		/* We've been here, we're done */
		return 0;
	}

	if ((state->max_extra_data != 0)
	    && (req->extra_len > state->max_extra_data)) {
		DEBUG(3, ("Got request with %d bytes extra data on "
			  "unprivileged socket\n", (int)req->extra_len));
		return -1;
	}

	return req->extra_len;
}

static void wb_req_read_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct req_read_state *state = tevent_req_data(
		req, struct req_read_state);
	int err;
	uint8_t *buf;

	state->ret = read_packet_recv(subreq, state, &buf, &err);
	TALLOC_FREE(subreq);
	if (state->ret == -1) {
		tevent_req_error(req, err);
		return;
	}

	state->wb_req = (struct winbindd_request *)buf;

	if (state->wb_req->extra_len != 0) {
		state->wb_req->extra_data.data =
			(char *)buf + sizeof(struct winbindd_request);
	} else {
		state->wb_req->extra_data.data = NULL;
	}
	tevent_req_done(req);
}

ssize_t wb_req_read_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx,
			 struct winbindd_request **preq, int *err)
{
	struct req_read_state *state = tevent_req_data(
		req, struct req_read_state);

	if (tevent_req_is_unix_error(req, err)) {
		return -1;
	}
	*preq = talloc_move(mem_ctx, &state->wb_req);
	return state->ret;
}

struct req_write_state {
	struct iovec iov[2];
	ssize_t ret;
};

static void wb_req_write_done(struct tevent_req *subreq);

struct tevent_req *wb_req_write_send(TALLOC_CTX *mem_ctx,
				     struct tevent_context *ev,
				     struct tevent_queue *queue, int fd,
				     struct winbindd_request *wb_req)
{
	struct tevent_req *req, *subreq;
	struct req_write_state *state;
	int count = 1;

	req = tevent_req_create(mem_ctx, &state, struct req_write_state);
	if (req == NULL) {
		return NULL;
	}

	state->iov[0].iov_base = (void *)wb_req;
	state->iov[0].iov_len = sizeof(struct winbindd_request);

	if (wb_req->extra_len != 0) {
		state->iov[1].iov_base = (void *)wb_req->extra_data.data;
		state->iov[1].iov_len = wb_req->extra_len;
		count = 2;
	}

	subreq = writev_send(state, ev, queue, fd, true, state->iov, count);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, wb_req_write_done, req);
	return req;
}

static void wb_req_write_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct req_write_state *state = tevent_req_data(
		req, struct req_write_state);
	int err;

	state->ret = writev_recv(subreq, &err);
	TALLOC_FREE(subreq);
	if (state->ret < 0) {
		tevent_req_error(req, err);
		return;
	}
	tevent_req_done(req);
}

ssize_t wb_req_write_recv(struct tevent_req *req, int *err)
{
	struct req_write_state *state = tevent_req_data(
		req, struct req_write_state);

	if (tevent_req_is_unix_error(req, err)) {
		return -1;
	}
	return state->ret;
}

struct resp_read_state {
	struct winbindd_response *wb_resp;
	ssize_t ret;
};

static ssize_t wb_resp_more(uint8_t *buf, size_t buflen, void *private_data);
static void wb_resp_read_done(struct tevent_req *subreq);

struct tevent_req *wb_resp_read_send(TALLOC_CTX *mem_ctx,
				     struct tevent_context *ev, int fd)
{
	struct tevent_req *req, *subreq;
	struct resp_read_state *state;

	req = tevent_req_create(mem_ctx, &state, struct resp_read_state);
	if (req == NULL) {
		return NULL;
	}

	subreq = read_packet_send(state, ev, fd, 4, wb_resp_more, state);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, wb_resp_read_done, req);
	return req;
}

static ssize_t wb_resp_more(uint8_t *buf, size_t buflen, void *private_data)
{
	struct winbindd_response *resp = (struct winbindd_response *)buf;

	if (buflen == 4) {
		if (resp->length < sizeof(struct winbindd_response)) {
			DEBUG(0, ("wb_resp_read_len: Invalid response size "
				  "received: %d (expected at least%d)\n",
				  (int)resp->length,
				  (int)sizeof(struct winbindd_response)));
			return -1;
		}
	}
	return resp->length - buflen;
}

static void wb_resp_read_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct resp_read_state *state = tevent_req_data(
		req, struct resp_read_state);
	uint8_t *buf;
	int err;

	state->ret = read_packet_recv(subreq, state, &buf, &err);
	TALLOC_FREE(subreq);
	if (state->ret == -1) {
		tevent_req_error(req, err);
		return;
	}

	state->wb_resp = (struct winbindd_response *)buf;

	if (state->wb_resp->length > sizeof(struct winbindd_response)) {
		state->wb_resp->extra_data.data =
			(char *)buf + sizeof(struct winbindd_response);
	} else {
		state->wb_resp->extra_data.data = NULL;
	}
	tevent_req_done(req);
}

ssize_t wb_resp_read_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx,
			  struct winbindd_response **presp, int *err)
{
	struct resp_read_state *state = tevent_req_data(
		req, struct resp_read_state);

	if (tevent_req_is_unix_error(req, err)) {
		return -1;
	}
	*presp = talloc_move(mem_ctx, &state->wb_resp);
	return state->ret;
}

struct resp_write_state {
	struct iovec iov[2];
	ssize_t ret;
};

static void wb_resp_write_done(struct tevent_req *subreq);

struct tevent_req *wb_resp_write_send(TALLOC_CTX *mem_ctx,
				      struct tevent_context *ev,
				      struct tevent_queue *queue, int fd,
				      struct winbindd_response *wb_resp)
{
	struct tevent_req *req, *subreq;
	struct resp_write_state *state;
	int count = 1;

	req = tevent_req_create(mem_ctx, &state, struct resp_write_state);
	if (req == NULL) {
		return NULL;
	}

	state->iov[0].iov_base = (void *)wb_resp;
	state->iov[0].iov_len = sizeof(struct winbindd_response);

	if (wb_resp->length > sizeof(struct winbindd_response)) {
		state->iov[1].iov_base = (void *)wb_resp->extra_data.data;
		state->iov[1].iov_len =
			wb_resp->length - sizeof(struct winbindd_response);
		count = 2;
	}

	subreq = writev_send(state, ev, queue, fd, true, state->iov, count);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, wb_resp_write_done, req);
	return req;
}

static void wb_resp_write_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct resp_write_state *state = tevent_req_data(
		req, struct resp_write_state);
	int err;

	state->ret = writev_recv(subreq, &err);
	TALLOC_FREE(subreq);
	if (state->ret < 0) {
		tevent_req_error(req, err);
		return;
	}
	tevent_req_done(req);
}

ssize_t wb_resp_write_recv(struct tevent_req *req, int *err)
{
	struct resp_write_state *state = tevent_req_data(
		req, struct resp_write_state);

	if (tevent_req_is_unix_error(req, err)) {
		return -1;
	}
	return state->ret;
}

struct wb_simple_trans_state {
	struct tevent_context *ev;
	int fd;
	struct winbindd_response *wb_resp;
};

static void wb_simple_trans_write_done(struct tevent_req *subreq);
static void wb_simple_trans_read_done(struct tevent_req *subreq);

struct tevent_req *wb_simple_trans_send(TALLOC_CTX *mem_ctx,
					struct tevent_context *ev,
					struct tevent_queue *queue, int fd,
					struct winbindd_request *wb_req)
{
	struct tevent_req *req, *subreq;
	struct wb_simple_trans_state *state;

	req = tevent_req_create(mem_ctx, &state, struct wb_simple_trans_state);
	if (req == NULL) {
		return NULL;
	}

	wb_req->length = sizeof(struct winbindd_request);

	state->ev = ev;
	state->fd = fd;

	subreq = wb_req_write_send(state, ev, queue, fd, wb_req);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, wb_simple_trans_write_done, req);

	return req;
}

static void wb_simple_trans_write_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct wb_simple_trans_state *state = tevent_req_data(
		req, struct wb_simple_trans_state);
	ssize_t ret;
	int err;

	ret = wb_req_write_recv(subreq, &err);
	TALLOC_FREE(subreq);
	if (ret == -1) {
		tevent_req_error(req, err);
		return;
	}
	subreq = wb_resp_read_send(state, state->ev, state->fd);
	if (tevent_req_nomem(subreq, req)) {
		return;
	}
	tevent_req_set_callback(subreq, wb_simple_trans_read_done, req);
}

static void wb_simple_trans_read_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct wb_simple_trans_state *state = tevent_req_data(
		req, struct wb_simple_trans_state);
	ssize_t ret;
	int err;

	ret = wb_resp_read_recv(subreq, state, &state->wb_resp, &err);
	TALLOC_FREE(subreq);
	if (ret == -1) {
		tevent_req_error(req, err);
		return;
	}

	tevent_req_done(req);
}

int wb_simple_trans_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx,
			 struct winbindd_response **presponse, int *err)
{
	struct wb_simple_trans_state *state = tevent_req_data(
		req, struct wb_simple_trans_state);

	if (tevent_req_is_unix_error(req, err)) {
		return -1;
	}
	*presponse = talloc_move(mem_ctx, &state->wb_resp);
	return 0;
}
