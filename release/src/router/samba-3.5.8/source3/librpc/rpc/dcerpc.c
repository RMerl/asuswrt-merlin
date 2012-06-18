/* 
   Unix SMB/CIFS implementation.
   Samba 4-compatible DCE/RPC API on top of the Samba 3 DCE/RPC client library.
   Copyright (C) Jelmer Vernooij <jelmer@samba.org> 2008
   
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
#include "librpc/rpc/dcerpc.h"

/** 
 * Send a struct-based RPC request using the Samba 3 RPC client library.
 */
struct rpc_request *dcerpc_ndr_request_send(struct dcerpc_pipe *p, const struct GUID *object, 
					    const struct ndr_interface_table *table, uint32_t opnum, 
					    TALLOC_CTX *mem_ctx, void *r)
{
	const struct ndr_interface_call *call;
	struct ndr_push *push;
	struct rpc_request *ret = talloc(mem_ctx, struct rpc_request);
	enum ndr_err_code ndr_err;
	DATA_BLOB blob;

	if (ret == NULL)
		return NULL;

	SMB_ASSERT(p->table->num_calls > opnum);

	call = &p->table->calls[opnum];

	ret->call = call;
	ret->r = r;

	push = ndr_push_init_ctx(mem_ctx, NULL);
	if (!push) {
		return NULL;
	}

	ndr_err = call->ndr_push(push, NDR_IN, r);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		/* FIXME: ndr_map_error2ntstatus(ndr_err); */
		return NULL;
	}

	blob = ndr_push_blob(push);

	if (!prs_init_data_blob(&ret->q_ps, &blob, mem_ctx)) {
		return NULL;
	}

	talloc_free(push);

	ret->opnum = opnum;

	ret->pipe = p;

	return ret;
}

#if 0

Completely unfinished and unused -- vl :-)

/**
 * Wait for a DCE/RPC request. 
 *
 * @note at the moment this is still sync, even though the API is async.
 */
NTSTATUS dcerpc_ndr_request_recv(struct rpc_request *req)
{
	prs_struct r_ps;
	struct ndr_pull *pull;
	NTSTATUS status;
	DATA_BLOB blob;
	enum ndr_err_code ndr_err;

	prs_init_empty( &r_ps, req, UNMARSHALL );

	status = rpc_api_pipe_req(req, req->pipe->rpc_cli, req->opnum,
				  &req->q_ps, &r_ps);

	prs_mem_free( &req->q_ps );

	if (!NT_STATUS_IS_OK(status)) {
		prs_mem_free( &r_ps );
		return status;
	}

	if (!prs_data_blob(&r_ps, &blob, req)) {
		prs_mem_free( &r_ps );
		return NT_STATUS_NO_MEMORY;
	}

	prs_mem_free( &r_ps );

	pull = ndr_pull_init_blob(&blob, req, NULL);
	if (pull == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	/* have the ndr parser alloc memory for us */
	pull->flags |= LIBNDR_FLAG_REF_ALLOC;
	ndr_err = req->call->ndr_pull(pull, NDR_OUT, req->r);
	talloc_free(pull);

	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return ndr_map_error2ntstatus(ndr_err);
	}

	return NT_STATUS_OK;
}

/**
 * Connect to a DCE/RPC interface.
 * 
 * @note lp_ctx and ev are ignored at the moment but present
 * 	for API compatibility.
 */
_PUBLIC_ NTSTATUS dcerpc_pipe_connect(TALLOC_CTX *parent_ctx, struct dcerpc_pipe **pp, 
				      const char *binding_string, const struct ndr_interface_table *table, 
				      struct cli_credentials *credentials, struct event_context *ev, 
				      struct loadparm_context *lp_ctx)
{
	struct dcerpc_pipe *p = talloc(parent_ctx, struct dcerpc_pipe);
	struct dcerpc_binding *binding;
	NTSTATUS nt_status;

	nt_status = dcerpc_parse_binding(p, binding_string, &binding);

	if (NT_STATUS_IS_ERR(nt_status)) {
		DEBUG(1, ("Unable to parse binding string '%s'", binding_string));
		talloc_free(p);
		return nt_status;
	}

	if (binding->transport != NCACN_NP) {
		DEBUG(0, ("Only ncacn_np supported"));
		talloc_free(p);
		return NT_STATUS_NOT_SUPPORTED;
	}

	/* FIXME: Actually use loadparm_context.. */

	/* FIXME: actually use credentials */

	nt_status = cli_full_connection(&p->cli, global_myname(), binding->host,
					NULL, 0, 
					"IPC$", "IPC",
					get_cmdline_auth_info_username(),
					lp_workgroup(),
					get_cmdline_auth_info_password(),
					get_cmdline_auth_info_use_kerberos() ? CLI_FULL_CONNECTION_USE_KERBEROS : 0,
					get_cmdline_auth_info_signing_state(), NULL);

	if (NT_STATUS_IS_ERR(nt_status)) {
		talloc_free(p);
		return nt_status;
	}

	nt_status = cli_rpc_pipe_open_noauth(p->cli, &table->syntax_id,
					     &p->rpc_cli);

	if (!NT_STATUS_IS_OK(nt_status)) {
		talloc_free(p);
		return nt_status;
	}

	p->table = table;

	*pp = p;

	return nt_status;
}

#endif
