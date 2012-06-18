/* 
   Unix SMB/CIFS implementation.
   RPC pipe client

   Copyright (C) Volker Lendecke 2005

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
#include "rpcclient.h"
#include "../librpc/gen_ndr/cli_lsa.h"

static NTSTATUS cmd_testme(struct rpc_pipe_client *cli, TALLOC_CTX *mem_ctx,
			   int argc, const char **argv)
{
	struct rpc_pipe_client *lsa_pipe = NULL, *samr_pipe = NULL;
	NTSTATUS status = NT_STATUS_UNSUCCESSFUL;
	struct policy_handle pol;

	d_printf("testme\n");

	status = cli_rpc_pipe_open_noauth(rpc_pipe_np_smb_conn(cli),
					  &ndr_table_lsarpc.syntax_id,
					  &lsa_pipe);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	status = cli_rpc_pipe_open_noauth(rpc_pipe_np_smb_conn(cli),
					  &ndr_table_samr.syntax_id,
					  &samr_pipe);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	status = rpccli_lsa_open_policy(lsa_pipe, mem_ctx, False,
					KEY_QUERY_VALUE, &pol);

	if (!NT_STATUS_IS_OK(status))
		goto done;

	status = rpccli_lsa_Close(lsa_pipe, mem_ctx, &pol);

	if (!NT_STATUS_IS_OK(status))
		goto done;

 done:
	TALLOC_FREE(lsa_pipe);
	TALLOC_FREE(samr_pipe);

	return status;
}

/* List of commands exported by this module */

struct cmd_set test_commands[] = {

	{ "TESTING" },

	{ "testme", RPC_RTYPE_NTSTATUS, cmd_testme, NULL,
	  NULL, NULL, "Sample test", "testme" },

	{ NULL }
};
