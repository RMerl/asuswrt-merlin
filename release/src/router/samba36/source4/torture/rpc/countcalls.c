/* 
   Unix SMB/CIFS implementation.

   count number of calls on an interface

   Copyright (C) Andrew Tridgell 2003
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2007
   
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
#include "librpc/ndr/libndr.h"
#include "librpc/ndr/ndr_table.h"
#include "torture/rpc/torture_rpc.h"
#include "param/param.h"


	
bool count_calls(struct torture_context *tctx,
		 TALLOC_CTX *mem_ctx,
		 const struct ndr_interface_table *iface,
	bool all) 
{
	struct dcerpc_pipe *p;
	DATA_BLOB stub_in, stub_out;
	int i;
	NTSTATUS status = torture_rpc_connection(tctx, &p, iface);
	if (NT_STATUS_EQUAL(NT_STATUS_OBJECT_NAME_NOT_FOUND, status)
	    || NT_STATUS_IS_RPC(status)
	    || NT_STATUS_EQUAL(NT_STATUS_PORT_UNREACHABLE, status)
	    || NT_STATUS_EQUAL(NT_STATUS_ACCESS_DENIED, status)) {
		if (all) {
			/* Not fatal if looking for all pipes */
			return true;
		} else {
			printf("Failed to open '%s' to count calls - %s\n", iface->name, nt_errstr(status));
			return false;
		}
	} else if (!NT_STATUS_IS_OK(status)) {
		printf("Failed to open '%s' to count calls - %s\n", iface->name, nt_errstr(status));
		return false;
	}

	stub_in = data_blob_null;

	printf("\nScanning pipe '%s'\n", iface->name);

	for (i=0;i<500;i++) {
		uint32_t out_flags = 0;

		status = dcerpc_binding_handle_raw_call(p->binding_handle,
							NULL, i,
							0, /* in_flags */
							stub_in.data,
							stub_in.length,
							mem_ctx,
							&stub_out.data,
							&stub_out.length,
							&out_flags);
		if (NT_STATUS_EQUAL(status, NT_STATUS_RPC_PROCNUM_OUT_OF_RANGE)) {
			i--;
			break;
		}
		if (NT_STATUS_EQUAL(status, NT_STATUS_CONNECTION_DISCONNECTED)) {
			i--;
			break;
		}
		if (NT_STATUS_EQUAL(status, NT_STATUS_PIPE_DISCONNECTED)) {
			i--;
			break;
		}
		if (NT_STATUS_EQUAL(status, NT_STATUS_ACCESS_DENIED)) {
			i--;
			break;
		}
		if (NT_STATUS_EQUAL(status, NT_STATUS_LOGON_FAILURE)) {
			i--;
			break;
		}
	}
	
	if (i==500) {
		talloc_free(p);
		printf("no limit on calls: %s!?\n", nt_errstr(status));
		return false;
	}

	printf("Found %d calls\n", i);

	talloc_free(p);
	
	return true;

}

bool torture_rpc_countcalls(struct torture_context *torture)
{
	const struct ndr_interface_table *iface;
	const char *iface_name;
	bool ret = true;
	const struct ndr_interface_list *l;
	iface_name = lpcfg_parm_string(torture->lp_ctx, NULL, "countcalls", "interface");
	if (iface_name != NULL) {
		iface = ndr_table_by_name(iface_name);
		if (!iface) {
			printf("Unknown interface '%s'\n", iface_name);
			return false;
		}
		return count_calls(torture, torture, iface, false);
	}

	for (l=ndr_table_list();l;l=l->next) {		
		TALLOC_CTX *loop_ctx;
		loop_ctx = talloc_named(torture, 0, "torture_rpc_councalls loop context");
		ret &= count_calls(torture, loop_ctx, l->table, true);
		talloc_free(loop_ctx);
	}
	return ret;
}
