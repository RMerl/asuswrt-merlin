/*
 *  Unix SMB/CIFS implementation.
 *  RPC client transport over a socket
 *  Copyright (C) Volker Lendecke 2009
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

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_RPC_CLI

struct rpc_transport_sock_state {
	int fd;
	int timeout;
};

static void rpc_sock_disconnect(struct rpc_transport_sock_state *s)
{
	if (s->fd != -1) {
		close(s->fd);
		s->fd = -1;
	}
}

static int rpc_transport_sock_state_destructor(struct rpc_transport_sock_state *s)
{
	rpc_sock_disconnect(s);
	return 0;
}

static bool rpc_sock_is_connected(void *priv)
{
	struct rpc_transport_sock_state *sock_transp = talloc_get_type_abort(
		priv, struct rpc_transport_sock_state);

	if (sock_transp->fd == -1) {
		return false;
	}

	return true;
}

static unsigned int rpc_sock_set_timeout(void *priv, unsigned int timeout)
{
	struct rpc_transport_sock_state *sock_transp = talloc_get_type_abort(
		priv, struct rpc_transport_sock_state);
	int orig_timeout;
	bool ok;

	ok = rpc_sock_is_connected(sock_transp);
	if (!ok) {
		return 0;
	}

	orig_timeout = sock_transp->timeout;

	sock_transp->timeout = timeout;

	return orig_timeout;
}

struct rpc_sock_read_state {
	struct rpc_transport_sock_state *transp;
	ssize_t received;
};

static void rpc_sock_read_done(struct tevent_req *subreq);

static struct tevent_req *rpc_sock_read_send(TALLOC_CTX *mem_ctx,
					     struct event_context *ev,
					     uint8_t *data, size_t size,
					     void *priv)
{
	struct rpc_transport_sock_state *sock_transp = talloc_get_type_abort(
		priv, struct rpc_transport_sock_state);
	struct tevent_req *req, *subreq;
	struct rpc_sock_read_state *state;
	struct timeval endtime;

	req = tevent_req_create(mem_ctx, &state, struct rpc_sock_read_state);
	if (req == NULL) {
		return NULL;
	}
	if (!rpc_sock_is_connected(sock_transp)) {
		tevent_req_nterror(req, NT_STATUS_CONNECTION_INVALID);
		return tevent_req_post(req, ev);
	}
	state->transp = sock_transp;
	endtime = timeval_current_ofs(0, sock_transp->timeout * 1000);
	subreq = async_recv_send(state, ev, sock_transp->fd, data, size, 0);
	if (subreq == NULL) {
		goto fail;
	}

	if (!tevent_req_set_endtime(subreq, ev, endtime)) {
		goto fail;
	}

	tevent_req_set_callback(subreq, rpc_sock_read_done, req);
	return req;
 fail:
	TALLOC_FREE(req);
	return NULL;
}

static void rpc_sock_read_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct rpc_sock_read_state *state = tevent_req_data(
		req, struct rpc_sock_read_state);
	int err;

	/* We must free subreq in this function as there is
	  a timer event attached to it. */

	state->received = async_recv_recv(subreq, &err);

	if (state->received == -1) {
		TALLOC_FREE(subreq);
		rpc_sock_disconnect(state->transp);
		tevent_req_nterror(req, map_nt_error_from_unix(err));
		return;
	}
	TALLOC_FREE(subreq);
	tevent_req_done(req);
}

static NTSTATUS rpc_sock_read_recv(struct tevent_req *req, ssize_t *preceived)
{
	struct rpc_sock_read_state *state = tevent_req_data(
		req, struct rpc_sock_read_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		return status;
	}
	*preceived = state->received;
	return NT_STATUS_OK;
}

struct rpc_sock_write_state {
	struct rpc_transport_sock_state *transp;
	ssize_t sent;
};

static void rpc_sock_write_done(struct tevent_req *subreq);

static struct tevent_req *rpc_sock_write_send(TALLOC_CTX *mem_ctx,
					      struct event_context *ev,
					      const uint8_t *data, size_t size,
					      void *priv)
{
	struct rpc_transport_sock_state *sock_transp = talloc_get_type_abort(
		priv, struct rpc_transport_sock_state);
	struct tevent_req *req, *subreq;
	struct rpc_sock_write_state *state;
	struct timeval endtime;

	req = tevent_req_create(mem_ctx, &state, struct rpc_sock_write_state);
	if (req == NULL) {
		return NULL;
	}
	if (!rpc_sock_is_connected(sock_transp)) {
		tevent_req_nterror(req, NT_STATUS_CONNECTION_INVALID);
		return tevent_req_post(req, ev);
	}
	state->transp = sock_transp;
	endtime = timeval_current_ofs(0, sock_transp->timeout * 1000);
	subreq = async_send_send(state, ev, sock_transp->fd, data, size, 0);
	if (subreq == NULL) {
		goto fail;
	}

	if (!tevent_req_set_endtime(subreq, ev, endtime)) {
		goto fail;
	}

	tevent_req_set_callback(subreq, rpc_sock_write_done, req);
	return req;
 fail:
	TALLOC_FREE(req);
	return NULL;
}

static void rpc_sock_write_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct rpc_sock_write_state *state = tevent_req_data(
		req, struct rpc_sock_write_state);
	int err;

	/* We must free subreq in this function as there is
	  a timer event attached to it. */

	state->sent = async_send_recv(subreq, &err);

	if (state->sent == -1) {
		TALLOC_FREE(subreq);
		rpc_sock_disconnect(state->transp);
		tevent_req_nterror(req, map_nt_error_from_unix(err));
		return;
	}
	TALLOC_FREE(subreq);
	tevent_req_done(req);
}

static NTSTATUS rpc_sock_write_recv(struct tevent_req *req, ssize_t *psent)
{
	struct rpc_sock_write_state *state = tevent_req_data(
		req, struct rpc_sock_write_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		return status;
	}
	*psent = state->sent;
	return NT_STATUS_OK;
}

NTSTATUS rpc_transport_sock_init(TALLOC_CTX *mem_ctx, int fd,
				 struct rpc_cli_transport **presult)
{
	struct rpc_cli_transport *result;
	struct rpc_transport_sock_state *state;

	result = talloc(mem_ctx, struct rpc_cli_transport);
	if (result == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	state = talloc(result, struct rpc_transport_sock_state);
	if (state == NULL) {
		TALLOC_FREE(result);
		return NT_STATUS_NO_MEMORY;
	}
	result->priv = state;

	state->fd = fd;
	state->timeout = 10000; /* 10 seconds. */
	talloc_set_destructor(state, rpc_transport_sock_state_destructor);

	result->trans_send = NULL;
	result->trans_recv = NULL;
	result->write_send = rpc_sock_write_send;
	result->write_recv = rpc_sock_write_recv;
	result->read_send = rpc_sock_read_send;
	result->read_recv = rpc_sock_read_recv;
	result->is_connected = rpc_sock_is_connected;
	result->set_timeout = rpc_sock_set_timeout;

	*presult = result;
	return NT_STATUS_OK;
}
