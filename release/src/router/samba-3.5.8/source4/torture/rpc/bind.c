/* 
   Unix SMB/CIFS implementation.

   dcerpc torture tests

   Copyright (C) Andrew Tridgell 2003
   Copyright (C) Andrew Bartlett <abartlet@samba.org 2004

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
#include "torture/torture.h"
#include "librpc/gen_ndr/ndr_lsa.h"
#include "librpc/gen_ndr/ndr_lsa_c.h"
#include "lib/cmdline/popt_common.h"
#include "librpc/rpc/dcerpc.h"
#include "torture/rpc/rpc.h"
#include "libcli/libcli.h"
#include "libcli/composite/composite.h"
#include "libcli/smb_composite/smb_composite.h"

/*
  This test is 'bogus' in that it doesn't actually perform to the
  spec.  We need to deal with other things inside the DCERPC layer,
  before we could have multiple binds.

  We should never pass this test, until such details are fixed in our
  client, and it looks like multible binds are never used anyway.

*/

bool torture_multi_bind(struct torture_context *torture) 
{
	struct dcerpc_pipe *p;
	struct dcerpc_binding *binding;
	TALLOC_CTX *mem_ctx;
	NTSTATUS status;
	bool ret;

	mem_ctx = talloc_init("torture_multi_bind");

	status = torture_rpc_binding(torture, &binding);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(mem_ctx);
		return false;
	}

	status = torture_rpc_connection(torture, &p, &ndr_table_lsarpc);
	
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(mem_ctx);
		return false;
	}

	status = dcerpc_pipe_auth(mem_ctx, &p, binding, &ndr_table_lsarpc, cmdline_credentials, 
				  torture->lp_ctx);

	if (NT_STATUS_IS_OK(status)) {
		printf("(incorrectly) allowed re-bind to uuid %s - %s\n", 
			GUID_string(mem_ctx, &ndr_table_lsarpc.syntax_id.uuid), nt_errstr(status));
		ret = false;
	} else {
		printf("\n");
		ret = true;
	}

	talloc_free(mem_ctx);

	return ret;
}
