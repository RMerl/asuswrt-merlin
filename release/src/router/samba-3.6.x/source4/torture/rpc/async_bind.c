/* 
   Unix SMB/CIFS implementation.

   dcerpc torture tests

   Copyright (C) Andrew Tridgell 2003
   Copyright (C) Rafal Szczesniak 2006

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
#include "librpc/gen_ndr/ndr_lsa.h"
#include "lib/cmdline/popt_common.h"
#include "torture/rpc/torture_rpc.h"

/*
  This test initiates multiple rpc bind requests and verifies
  whether all of them are served.
*/


bool torture_async_bind(struct torture_context *torture)
{
	NTSTATUS status;
	TALLOC_CTX *mem_ctx;
	int i;
	const char *binding_string;
	struct cli_credentials *creds;
	extern int torture_numasync;

	struct composite_context **bind_req;
	struct dcerpc_pipe **pipe;
	const struct ndr_interface_table **table;

	if (!torture_setting_bool(torture, "async", false)) {
		printf("async bind test disabled - enable async tests to use\n");
		return true;
	}
	
	binding_string = torture_setting_string(torture, "binding", NULL);

	/* talloc context */
	mem_ctx = talloc_init("torture_async_bind");
	if (mem_ctx == NULL) return false;

	bind_req = talloc_array(torture, struct composite_context*, torture_numasync);
	if (bind_req == NULL) return false;
	pipe     = talloc_array(torture, struct dcerpc_pipe*, torture_numasync);
	if (pipe == NULL) return false;
	table    = talloc_array(torture, const struct ndr_interface_table*, torture_numasync);
	if (table == NULL) return false;
	
	/* credentials */
	creds = cmdline_credentials;

	/* send bind requests */
	for (i = 0; i < torture_numasync; i++) {
		table[i] = &ndr_table_lsarpc;
		bind_req[i] = dcerpc_pipe_connect_send(mem_ctx, binding_string,
						       table[i], creds, torture->ev, torture->lp_ctx);
	}

	/* recv bind requests */
	for (i = 0; i < torture_numasync; i++) {
		status = dcerpc_pipe_connect_recv(bind_req[i], mem_ctx, &pipe[i]);
		if (!NT_STATUS_IS_OK(status)) {
			printf("async rpc connection failed: %s\n", nt_errstr(status));
			return false;
		}
	}

	talloc_free(mem_ctx);
	return true;
}
