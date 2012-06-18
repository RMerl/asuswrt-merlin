/* 
   Unix SMB/CIFS implementation.

   libndr interface

   Copyright (C) Jelmer Vernooij 2006
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

struct cli_do_rpc_ndr_state {
	const struct ndr_interface_call *call;
	prs_struct q_ps, r_ps;
	void *r;
};

static void cli_do_rpc_ndr_done(struct tevent_req *subreq);

struct tevent_req *cli_do_rpc_ndr_send(TALLOC_CTX *mem_ctx,
				       struct tevent_context *ev,
				       struct rpc_pipe_client *cli,
				       const struct ndr_interface_table *table,
				       uint32_t opnum,
				       void *r)
{
	struct tevent_req *req, *subreq;
	struct cli_do_rpc_ndr_state *state;
	struct ndr_push *push;
	DATA_BLOB blob;
	enum ndr_err_code ndr_err;
	bool ret;

	req = tevent_req_create(mem_ctx, &state,
				struct cli_do_rpc_ndr_state);
	if (req == NULL) {
		return NULL;
	}

	if (!ndr_syntax_id_equal(&table->syntax_id, &cli->abstract_syntax)
	    || (opnum >= table->num_calls)) {
		tevent_req_nterror(req, NT_STATUS_INVALID_PARAMETER);
		return tevent_req_post(req, ev);
	}

	state->r = r;
	state->call = &table->calls[opnum];

	if (DEBUGLEVEL >= 10) {
		ndr_print_function_debug(state->call->ndr_print,
					 state->call->name, NDR_IN, r);
	}

	push = ndr_push_init_ctx(talloc_tos(), NULL);
	if (tevent_req_nomem(push, req)) {
		return tevent_req_post(req, ev);
	}

	ndr_err = state->call->ndr_push(push, NDR_IN, r);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		tevent_req_nterror(req, ndr_map_error2ntstatus(ndr_err));
		TALLOC_FREE(push);
		return tevent_req_post(req, ev);
	}

	blob = ndr_push_blob(push);
	ret = prs_init_data_blob(&state->q_ps, &blob, state);
	TALLOC_FREE(push);

	if (!ret) {
		tevent_req_nterror(req, NT_STATUS_NO_MEMORY);
		return tevent_req_post(req, ev);
	}

	subreq = rpc_api_pipe_req_send(state, ev, cli, opnum, &state->q_ps);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, cli_do_rpc_ndr_done, req);
	return req;
}

static void cli_do_rpc_ndr_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct cli_do_rpc_ndr_state *state = tevent_req_data(
		req, struct cli_do_rpc_ndr_state);
	NTSTATUS status;

	status = rpc_api_pipe_req_recv(subreq, state, &state->r_ps);
	TALLOC_FREE(subreq);
	prs_mem_free(&state->q_ps);
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}
	tevent_req_done(req);
}

NTSTATUS cli_do_rpc_ndr_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx)
{
	struct cli_do_rpc_ndr_state *state = tevent_req_data(
		req, struct cli_do_rpc_ndr_state);
	struct ndr_pull *pull;
	enum ndr_err_code ndr_err;
	NTSTATUS status;
	DATA_BLOB blob;
	bool ret;

	if (tevent_req_is_nterror(req, &status)) {
		return status;
	}

	ret = prs_data_blob(&state->r_ps, &blob, talloc_tos());
	prs_mem_free(&state->r_ps);
	if (!ret) {
		return NT_STATUS_NO_MEMORY;
	}

	pull = ndr_pull_init_blob(&blob, mem_ctx, NULL);
	if (pull == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	/* have the ndr parser alloc memory for us */
	pull->flags |= LIBNDR_FLAG_REF_ALLOC;
	ndr_err = state->call->ndr_pull(pull, NDR_OUT, state->r);
	TALLOC_FREE(pull);

	if (NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		if (DEBUGLEVEL >= 10) {
			ndr_print_function_debug(state->call->ndr_print,
						 state->call->name, NDR_OUT,
						 state->r);
		}
	} else {
		return ndr_map_error2ntstatus(ndr_err);
	}

	return NT_STATUS_OK;
}

NTSTATUS cli_do_rpc_ndr(struct rpc_pipe_client *cli,
			TALLOC_CTX *mem_ctx,
			const struct ndr_interface_table *table,
			uint32_t opnum, void *r)
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

	req = cli_do_rpc_ndr_send(frame, ev, cli, table, opnum, r);
	if (req == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	if (!tevent_req_poll(req, ev)) {
		status = map_nt_error_from_unix(errno);
		goto fail;
	}

	status = cli_do_rpc_ndr_recv(req, mem_ctx);

 fail:
	TALLOC_FREE(frame);
	return status;
}
