/* 
   Unix SMB/CIFS implementation.

   libndr interface

   Copyright (C) Jelmer Vernooij 2006
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "includes.h"


NTSTATUS cli_do_rpc_ndr(struct rpc_pipe_client *cli, TALLOC_CTX *mem_ctx, 
			int p_idx, int opnum, void *data, 
			ndr_pull_flags_fn_t pull_fn, ndr_push_flags_fn_t push_fn)
{
	prs_struct q_ps, r_ps;
	struct ndr_pull *pull;
	DATA_BLOB blob;
	struct ndr_push *push;
	NTSTATUS status;

	SMB_ASSERT(cli->pipe_idx == p_idx);

	push = ndr_push_init_ctx(mem_ctx);
	if (!push) {
		return NT_STATUS_NO_MEMORY;
	}

	status = push_fn(push, NDR_IN, data);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	blob = ndr_push_blob(push);

	if (!prs_init_data_blob(&q_ps, &blob, mem_ctx)) {
		return NT_STATUS_NO_MEMORY;
	}

	talloc_free(push);

	if (!prs_init( &r_ps, 0, mem_ctx, UNMARSHALL )) {
		prs_mem_free( &q_ps );
		return NT_STATUS_NO_MEMORY;
	}
	
	status = rpc_api_pipe_req(cli, opnum, &q_ps, &r_ps); 

	prs_mem_free( &q_ps );

	if (!NT_STATUS_IS_OK(status)) {
		prs_mem_free( &r_ps );
		return status;
	}

	if (!prs_data_blob(&r_ps, &blob, mem_ctx)) {
		prs_mem_free( &r_ps );
		return NT_STATUS_NO_MEMORY;
	}

	prs_mem_free( &r_ps );

	pull = ndr_pull_init_blob(&blob, mem_ctx);
	if (pull == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	/* have the ndr parser alloc memory for us */
	pull->flags |= LIBNDR_FLAG_REF_ALLOC;
	status = pull_fn(pull, NDR_OUT, data);
	talloc_free(pull);

	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	return NT_STATUS_OK;
}
