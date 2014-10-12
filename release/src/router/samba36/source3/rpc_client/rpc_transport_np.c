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
#include "../lib/util/tevent_ntstatus.h"
#include "rpc_client/rpc_transport.h"
#include "libsmb/cli_np_tstream.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_RPC_CLI

struct rpc_transport_np_init_state {
	struct rpc_cli_transport *transport;
};

static void rpc_transport_np_init_pipe_open(struct tevent_req *subreq);

struct tevent_req *rpc_transport_np_init_send(TALLOC_CTX *mem_ctx,
					      struct event_context *ev,
					      struct cli_state *cli,
					      const struct ndr_syntax_id *abstract_syntax)
{
	struct tevent_req *req;
	struct rpc_transport_np_init_state *state;
	const char *pipe_name;
	struct tevent_req *subreq;

	req = tevent_req_create(mem_ctx, &state,
				struct rpc_transport_np_init_state);
	if (req == NULL) {
		return NULL;
	}

	pipe_name = get_pipe_name_from_syntax(state, abstract_syntax);
	if (tevent_req_nomem(pipe_name, req)) {
		return tevent_req_post(req, ev);
	}

	subreq = tstream_cli_np_open_send(state, ev, cli, pipe_name);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, rpc_transport_np_init_pipe_open, req);

	return req;
}

static void rpc_transport_np_init_pipe_open(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct rpc_transport_np_init_state *state = tevent_req_data(
		req, struct rpc_transport_np_init_state);
	NTSTATUS status;
	struct tstream_context *stream;

	status = tstream_cli_np_open_recv(subreq, state, &stream);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}

	status = rpc_transport_tstream_init(state,
					    &stream,
					    &state->transport);
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}

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
