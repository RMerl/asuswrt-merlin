/* 
   Unix SMB/CIFS implementation.

   test suite for dssetup rpc operations

   Copyright (C) Andrew Tridgell 2004
   
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
#include "librpc/gen_ndr/ndr_dssetup_c.h"
#include "torture/rpc/torture_rpc.h"


bool test_DsRoleGetPrimaryDomainInformation_ext(struct torture_context *tctx, 
						struct dcerpc_pipe *p,
						NTSTATUS ext_status)
{
	struct dssetup_DsRoleGetPrimaryDomainInformation r;
	NTSTATUS status;
	int i;
	struct dcerpc_binding_handle *b = p->binding_handle;

	for (i=DS_ROLE_BASIC_INFORMATION; i <= DS_ROLE_OP_STATUS; i++) {
		r.in.level = i;
		torture_comment(tctx, "dcerpc_dssetup_DsRoleGetPrimaryDomainInformation level %d\n", i);

		status = dcerpc_dssetup_DsRoleGetPrimaryDomainInformation_r(b, tctx, &r);
		torture_assert_ntstatus_equal(tctx, ext_status, status, "DsRoleGetPrimaryDomainInformation failed");
		if (NT_STATUS_IS_OK(ext_status)) {
			torture_assert_werr_ok(tctx, r.out.result, "DsRoleGetPrimaryDomainInformation failed");
		}
	}

	return true;
}

bool test_DsRoleGetPrimaryDomainInformation(struct torture_context *tctx, 
					    struct dcerpc_pipe *p)
{
	return test_DsRoleGetPrimaryDomainInformation_ext(tctx, p, NT_STATUS_OK);
}

struct torture_suite *torture_rpc_dssetup(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, "dssetup");
	struct torture_rpc_tcase *tcase = torture_suite_add_rpc_iface_tcase(suite, "dssetup", &ndr_table_dssetup);

	torture_rpc_tcase_add_test(tcase, "DsRoleGetPrimaryDomainInformation", test_DsRoleGetPrimaryDomainInformation);

	return suite;
}
