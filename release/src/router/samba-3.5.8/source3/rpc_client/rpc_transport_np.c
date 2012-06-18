/*
 *  Unix SMB/CIFS implementation.
 *  RPC client transport over named pipes
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

struct rpc_transport_np_state {
	struct cli_state *cli;
	const char *pipe_name;
	uint16_t fnum;
};

static bool rpc_np_is_connected(void *priv)
{
	struct rpc_transport_np_state *np_transport = talloc_get_type_abort(
		priv, struct rpc_transport_np_state);
	bool ok;

	if (np_transport->cli == NULL) {
		return false;
	}

	ok = cli_state_is_connected(np_transport->cli);
	if (!ok) {
		np_transport->cli = NULL;
		return false;
	}

	return true;
}

static unsigned int rpc_np_set_timeout(void *priv, unsigned int timeout)
{
	struct rpc_transport_np_state *np_transport = talloc_get_type_abort(
		priv, struct rpc_transport_np_state);
	bool ok;

	if (np_transport->cli == NULL) {
		return false;
	}

	ok = rpc_np_is_connected(np_transport);
	if (!ok) {
		return 0;
	}

	return cli_set_timeout(np_transport->cli, timeout);
}

static int rpc_transport_np_state_destructor(struct rpc_transport_np_state *s)
{
	if (!rpc_np_is_connected(s)) {
		DEBUG(10, ("socket was closed, no need to send close request.\n"));
		return 0;
	}

	/* TODO: do not use a sync call with a destructor!!! */
	if (!NT_STATUS_IS_OK(cli_close(s->cli, s->fnum))) {
		DEBUG(1, ("rpc_transport_np_state_destructor: cli_close "
			  "failed on pipe %s. Error was %s\n", s->pipe_name,
			  cli_errstr(s->cli)));
	}
	DEBUG(10, ("rpc_pipe_destructor: closed %s\n", s->pipe_name));
	/*
	 * We can't do much on failure
	 */
	return 0;
}

struct rpc_np_write_state {
	struct rpc_transport_np_state *np_transport;
	size_t size;
	size_t written;
};

static void rpc_np_write_done(struct tevent_req *subreq);

static struct tevent_req *rpc_np_write_send(TALLOC_CTX *mem_ctx,
					    struct event_context *ev,
					    const uint8_t *data, size_t size,
					    void *priv)
{
	struct rpc_transport_np_state *np_transport = talloc_get_type_abort(
		priv, struct rpc_transport_np_state);
	struct tevent_req *req, *subreq;
	struct rpc_np_write_state *state;
	bool ok;

	req = tevent_req_create(mem_ctx, &state, struct rpc_np_write_state);
	if (req == NULL) {
		return NULL;
	}

	ok = rpc_np_is_connected(np_transport);
	if (!ok) {
		tevent_req_nterror(req, NT_STATUS_CONNECTION_INVALID);
		return tevent_req_post(req, ev);
	}

	state->np_transport = np_transport;
	state->size = size;


	subreq = cli_write_andx_send(mem_ctx, ev, np_transport->cli,
				     np_transport->fnum,
				     8, /* 8 means message mode. */
				     data, 0, size);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, rpc_np_write_done, req);
	return req;
}

static void rpc_np_write_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct rpc_np_write_state *state = tevent_req_data(
		req, struct rpc_np_write_state);
	NTSTATUS status;

	status = cli_write_andx_recv(subreq, &state->written);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		state->np_transport->cli = NULL;
		tevent_req_nterror(req, status);
		return;
	}
	tevent_req_done(req);
}

static NTSTATUS rpc_np_write_recv(struct tevent_req *req, ssize_t *pwritten)
{
	struct rpc_np_write_state *state = tevent_req_data(
		req, struct rpc_np_write_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		return status;
	}
	*pwritten = state->written;
	return NT_STATUS_OK;
}

struct rpc_np_read_state {
	struct rpc_transport_np_state *np_transport;
	uint8_t *data;
	size_t size;
	ssize_t received;
};

static void rpc_np_read_done(struct tevent_req *subreq);

static struct tevent_req *rpc_np_read_send(TALLOC_CTX *mem_ctx,
					   struct event_context *ev,
					   uint8_t *data, size_t size,
					   void *priv)
{
	struct rpc_transport_np_state *np_transport = talloc_get_type_abort(
		priv, struct rpc_transport_np_state);
	struct tevent_req *req, *subreq;
	struct rpc_np_read_state *state;
	bool ok;

	req = tevent_req_create(mem_ctx, &state, struct rpc_np_read_state);
	if (req == NULL) {
		return NULL;
	}

	ok = rpc_np_is_connected(np_transport);
	if (!ok) {
		tevent_req_nterror(req, NT_STATUS_CONNECTION_INVALID);
		return tevent_req_post(req, ev);
	}

	state->np_transport = np_transport;
	state->data = data;
	state->size = size;

	subreq = cli_read_andx_send(mem_ctx, ev, np_transport->cli,
				    np_transport->fnum, 0, size);
	if (subreq == NULL) {
		goto fail;
	}
	tevent_req_set_callback(subreq, rpc_np_read_done, req);
	return req;
 fail:
	TALLOC_FREE(req);
	return NULL;
}

static void rpc_np_read_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct rpc_np_read_state *state = tevent_req_data(
		req, struct rpc_np_read_state);
	NTSTATUS status;
	uint8_t *rcvbuf;

	/* We must free subreq in this function as there is
	   a timer event attached to it. */

	status = cli_read_andx_recv(subreq, &state->received, &rcvbuf);
	/*
	 * We can't TALLOC_FREE(subreq) as usual here, as rcvbuf still is a
	 * child of that.
	 */
	if (NT_STATUS_EQUAL(status, NT_STATUS_BUFFER_TOO_SMALL)) {
		status = NT_STATUS_OK;
	}
	if (!NT_STATUS_IS_OK(status)) {
		TALLOC_FREE(subreq);
		state->np_transport->cli = NULL;
		tevent_req_nterror(req, status);
		return;
	}

	if (state->received > state->size) {
		TALLOC_FREE(subreq);
		state->np_transport->cli = NULL;
		tevent_req_nterror(req, NT_STATUS_INVALID_NETWORK_RESPONSE);
		return;
	}

	if (state->received == 0) {
		TALLOC_FREE(subreq);
		state->np_transport->cli = NULL;
		tevent_req_nterror(req, NT_STATUS_PIPE_BROKEN);
		return;
	}

	memcpy(state->data, rcvbuf, state->received);
	TALLOC_FREE(subreq);
	tevent_req_done(req);
}

static NTSTATUS rpc_np_read_recv(struct tevent_req *req, ssize_t *preceived)
{
	struct rpc_np_read_state *state = tevent_req_data(
		req, struct rpc_np_read_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		return status;
	}
	*preceived = state->received;
	return NT_STATUS_OK;
}

struct rpc_np_trans_state {
	struct rpc_transport_np_state *np_transport;
	uint16_t setup[2];
	uint32_t max_rdata_len;
	uint8_t *rdata;
	uint32_t rdata_len;
};

static void rpc_np_trans_done(struct tevent_req *subreq);

static struct tevent_req *rpc_np_trans_send(TALLOC_CTX *mem_ctx,
					    struct event_context *ev,
					    uint8_t *data, size_t data_len,
					    uint32_t max_rdata_len,
					    void *priv)
{
	struct rpc_transport_np_state *np_transport = talloc_get_type_abort(
		priv, struct rpc_transport_np_state);
	struct tevent_req *req, *subreq;
	struct rpc_np_trans_state *state;
	bool ok;

	req = tevent_req_create(mem_ctx, &state, struct rpc_np_trans_state);
	if (req == NULL) {
		return NULL;
	}

	ok = rpc_np_is_connected(np_transport);
	if (!ok) {
		tevent_req_nterror(req, NT_STATUS_CONNECTION_INVALID);
		return tevent_req_post(req, ev);
	}

	state->np_transport = np_transport;
	state->max_rdata_len = max_rdata_len;

	SSVAL(state->setup+0, 0, TRANSACT_DCERPCCMD);
	SSVAL(state->setup+1, 0, np_transport->fnum);

	subreq = cli_trans_send(
		state, ev, np_transport->cli, SMBtrans,
		"\\PIPE\\", 0, 0, 0, state->setup, 2, 0,
		NULL, 0, 0, data, data_len, max_rdata_len);
	if (subreq == NULL) {
		goto fail;
	}
	tevent_req_set_callback(subreq, rpc_np_trans_done, req);
	return req;

 fail:
	TALLOC_FREE(req);
	return NULL;
}

static void rpc_np_trans_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct rpc_np_trans_state *state = tevent_req_data(
		req, struct rpc_np_trans_state);
	NTSTATUS status;

	status = cli_trans_recv(subreq, state, NULL, NULL, NULL, NULL,
				&state->rdata, &state->rdata_len);
	TALLOC_FREE(subreq);
	if (NT_STATUS_EQUAL(status, NT_STATUS_BUFFER_TOO_SMALL)) {
		status = NT_STATUS_OK;
	}
	if (!NT_STATUS_IS_OK(status)) {
		state->np_transport->cli = NULL;
		tevent_req_nterror(req, status);
		return;
	}

	if (state->rdata_len > state->max_rdata_len) {
		state->np_transport->cli = NULL;
		tevent_req_nterror(req, NT_STATUS_INVALID_NETWORK_RESPONSE);
		return;
	}

	if (state->rdata_len == 0) {
		state->np_transport->cli = NULL;
		tevent_req_nterror(req, NT_STATUS_PIPE_BROKEN);
		return;
	}

	tevent_req_done(req);
}

static NTSTATUS rpc_np_trans_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx,
				  uint8_t **prdata, uint32_t *prdata_len)
{
	struct rpc_np_trans_state *state = tevent_req_data(
		req, struct rpc_np_trans_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		return status;
	}
	*prdata = talloc_move(mem_ctx, &state->rdata);
	*prdata_len = state->rdata_len;
	return NT_STATUS_OK;
}

struct rpc_transport_np_init_state {
	struct rpc_cli_transport *transport;
	struct rpc_transport_np_state *transport_np;
};

static void rpc_transport_np_init_pipe_open(struct tevent_req *subreq);

struct tevent_req *rpc_transport_np_init_send(TALLOC_CTX *mem_ctx,
					      struct event_context *ev,
					      struct cli_state *cli,
					      const struct ndr_syntax_id *abstract_syntax)
{
	struct tevent_req *req, *subreq;
	struct rpc_transport_np_init_state *state;
	bool ok;

	req = tevent_req_create(mem_ctx, &state,
				struct rpc_transport_np_init_state);
	if (req == NULL) {
		return NULL;
	}

	ok = cli_state_is_connected(cli);
	if (!ok) {
		tevent_req_nterror(req, NT_STATUS_CONNECTION_INVALID);
		return tevent_req_post(req, ev);
	}

	state->transport = talloc(state, struct rpc_cli_transport);
	if (tevent_req_nomem(state->transport, req)) {
		return tevent_req_post(req, ev);
	}
	state->transport_np = talloc(state->transport,
				     struct rpc_transport_np_state);
	if (tevent_req_nomem(state->transport_np, req)) {
		return tevent_req_post(req, ev);
	}
	state->transport->priv = state->transport_np;

	state->transport_np->pipe_name = get_pipe_name_from_syntax(
		state->transport_np, abstract_syntax);
	state->transport_np->cli = cli;

	subreq = cli_ntcreate_send(
		state, ev, cli, state->transport_np->pipe_name,	0,
		DESIRED_ACCESS_PIPE, 0, FILE_SHARE_READ|FILE_SHARE_WRITE,
		FILE_OPEN, 0, 0);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, rpc_transport_np_init_pipe_open,
				req);
	return req;
}

static void rpc_transport_np_init_pipe_open(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct rpc_transport_np_init_state *state = tevent_req_data(
		req, struct rpc_transport_np_init_state);
	NTSTATUS status;

	status = cli_ntcreate_recv(subreq, &state->transport_np->fnum);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}

	talloc_set_destructor(state->transport_np,
			      rpc_transport_np_state_destructor);
	tevent_req_done(req);
}

NTSTATUS rpc_transport_np_init_recv(struct tevent_req *req,
				    TALLOC_CTX *mem_ctx,
				    struct rpc_cli_transport **presult)
{
	struct rpc_transport_np_init_state *state = tevent_req_data(
		req, struct rpc_transport_np_init_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		return status;
	}

	state->transport->write_send = rpc_np_write_send;
	state->transport->write_recv = rpc_np_write_recv;
	state->transport->read_send = rpc_np_read_send;
	state->transport->read_recv = rpc_np_read_recv;
	state->transport->trans_send = rpc_np_trans_send;
	state->transport->trans_recv = rpc_np_trans_recv;
	state->transport->is_connected = rpc_np_is_connected;
	state->transport->set_timeout = rpc_np_set_timeout;

	*presult = talloc_move(mem_ctx, &state->transport);
	return NT_STATUS_OK;
}

NTSTATUS rpc_transport_np_init(TALLOC_CTX *mem_ctx, struct cli_state *cli,
			       const struct ndr_syntax_id *abstract_syntax,
			       struct rpc_cli_transport **presult)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct event_context *ev;
	struct tevent_req *req;
	NTSTATUS status = NT_STATUS_OK;

	ev = event_context_init(frame);
	if (ev == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	req = rpc_transport_np_init_send(frame, ev, cli, abstract_syntax);
	if (req == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	if (!tevent_req_poll(req, ev)) {
		status = map_nt_error_from_unix(errno);
		goto fail;
	}

	status = rpc_transport_np_init_recv(req, mem_ctx, presult);
 fail:
	TALLOC_FREE(frame);
	return status;
}

struct cli_state *rpc_pipe_np_smb_conn(struct rpc_pipe_client *p)
{
	struct rpc_transport_np_state *state = talloc_get_type(
		p->transport->priv, struct rpc_transport_np_state);

	if (state == NULL) {
		return NULL;
	}
	return state->cli;
}
