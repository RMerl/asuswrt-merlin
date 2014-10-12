/* 
   Unix SMB/CIFS implementation.

   test suite for dcerpc alter_context operations

   Copyright (C) Andrew Tridgell 2005
   
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
#include "librpc/gen_ndr/ndr_dssetup.h"
#include "torture/rpc/torture_rpc.h"

bool torture_rpc_alter_context(struct torture_context *torture)
{
	NTSTATUS status;
	struct dcerpc_pipe *p, *p2, *p3;
	struct policy_handle *handle;
	struct ndr_interface_table tmptbl;
	struct ndr_syntax_id syntax;
	struct ndr_syntax_id transfer_syntax;
	bool ret = true;

	torture_comment(torture, "opening LSA connection\n");
	status = torture_rpc_connection(torture, &p, &ndr_table_lsarpc);
	torture_assert_ntstatus_ok(torture, status, "connecting");

	torture_comment(torture, "Testing change of primary context\n");
	status = dcerpc_alter_context(p, torture, &p->syntax, &p->transfer_syntax);
	torture_assert_ntstatus_ok(torture, status, "dcerpc_alter_context failed");

	if (!test_lsa_OpenPolicy2(p->binding_handle, torture, &handle)) {
		ret = false;
	}

	torture_comment(torture, "Testing change of primary context\n");
	status = dcerpc_alter_context(p, torture, &p->syntax, &p->transfer_syntax);
	torture_assert_ntstatus_ok(torture, status, "dcerpc_alter_context failed");

	torture_comment(torture, "Opening secondary DSSETUP context\n");
	status = dcerpc_secondary_context(p, &p2, &ndr_table_dssetup);
	torture_assert_ntstatus_ok(torture, status, "dcerpc_alter_context failed");

	torture_comment(torture, "Testing change of primary context\n");
	status = dcerpc_alter_context(p2, torture, &p2->syntax, &p2->transfer_syntax);
	torture_assert_ntstatus_ok(torture, status, "dcerpc_alter_context failed");

	tmptbl = ndr_table_dssetup;
	tmptbl.syntax_id.if_version += 100;
	torture_comment(torture, "Opening bad secondary connection\n");
	status = dcerpc_secondary_context(p, &p3, &tmptbl);
	torture_assert_ntstatus_equal(torture, status, NT_STATUS_RPC_UNSUPPORTED_NAME_SYNTAX,
				      "dcerpc_alter_context with wrong version should fail");

	torture_comment(torture, "Testing DSSETUP pipe operations\n");
	ret &= test_DsRoleGetPrimaryDomainInformation(torture, p2);

	if (handle) {
		ret &= test_lsa_Close(p->binding_handle, torture, handle);
	}

	syntax = p->syntax;
	transfer_syntax = p->transfer_syntax;

	torture_comment(torture, "Testing change of primary context\n");
	status = dcerpc_alter_context(p, torture, &p->syntax, &p->transfer_syntax);
	torture_assert_ntstatus_ok(torture, status, "dcerpc_alter_context failed");

	ret &= test_lsa_OpenPolicy2(p->binding_handle, torture, &handle);

	if (handle) {
		ret &= test_lsa_Close(p->binding_handle, torture, handle);
	}

	torture_comment(torture, "Testing change of primary context\n");
	status = dcerpc_alter_context(p, torture, &p2->syntax, &p2->transfer_syntax);
	if (NT_STATUS_EQUAL(status, NT_STATUS_RPC_PROTOCOL_ERROR)) {

		ret &= test_lsa_OpenPolicy2_ex(p->binding_handle, torture, &handle,
					       NT_STATUS_PIPE_DISCONNECTED);
		return ret;
	}
	torture_assert_ntstatus_ok(torture, status, "dcerpc_alter_context failed");

	torture_comment(torture, "Testing DSSETUP pipe operations - should fault\n");
	ret &= test_DsRoleGetPrimaryDomainInformation_ext(torture, p, NT_STATUS_RPC_BAD_STUB_DATA);

	ret &= test_lsa_OpenPolicy2(p->binding_handle, torture, &handle);

	if (handle) {
		ret &= test_lsa_Close(p->binding_handle, torture, handle);
	}

	torture_comment(torture, "Testing DSSETUP pipe operations\n");

	ret &= test_DsRoleGetPrimaryDomainInformation(torture, p2);

	return ret;
}
