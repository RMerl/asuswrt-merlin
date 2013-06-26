/*
   Unix SMB/CIFS implementation.

   test suite for behaviour of object uuids in rpc requests

   Copyright (C) Stefan Metzmacher 2008

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
#include "librpc/gen_ndr/ndr_dssetup.h"
#include "librpc/gen_ndr/ndr_lsa.h"
#include "torture/rpc/torture_rpc.h"

/*
  this tests the send object uuids in the dcerpc request
*/

static bool test_random_uuid(struct torture_context *torture)
{
	NTSTATUS status;
	struct dcerpc_pipe *p1, *p2;
	struct GUID uuid;
	struct dssetup_DsRoleGetPrimaryDomainInformation r1;
	struct lsa_GetUserName r2;
	struct lsa_String *authority_name_p = NULL;
	struct lsa_String *account_name_p = NULL;

	torture_comment(torture, "RPC-OBJECTUUID-RANDOM\n");

	status = torture_rpc_connection(torture, &p1, &ndr_table_dssetup);
	torture_assert_ntstatus_ok(torture, status, "opening dsetup pipe1");

	status = torture_rpc_connection(torture, &p2, &ndr_table_lsarpc);
	torture_assert_ntstatus_ok(torture, status, "opening lsa pipe1");

	uuid = GUID_random();

	r1.in.level = DS_ROLE_BASIC_INFORMATION;
	status = dcerpc_binding_handle_call(p1->binding_handle,
				    &uuid,
				    &ndr_table_dssetup,
				    NDR_DSSETUP_DSROLEGETPRIMARYDOMAININFORMATION,
				    torture, &r1);
	torture_assert_ntstatus_ok(torture, status, "DsRoleGetPrimaryDomainInformation failed");
	torture_assert_werr_ok(torture, r1.out.result, "DsRoleGetPrimaryDomainInformation failed");

	uuid = GUID_random();

	r2.in.system_name = "\\";
	r2.in.account_name = &account_name_p;
	r2.in.authority_name = &authority_name_p;
	r2.out.account_name = &account_name_p;
	r2.out.authority_name = &authority_name_p;

	status = dcerpc_binding_handle_call(p2->binding_handle,
				    &uuid,
				    &ndr_table_lsarpc,
				    NDR_LSA_GETUSERNAME,
				    torture, &r2);
	torture_assert_ntstatus_ok(torture, status, "lsaClose failed");
	torture_assert_ntstatus_ok(torture, r2.out.result, "lsaClose failed");

	return true;
}

struct torture_suite *torture_rpc_object_uuid(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite;
	suite = torture_suite_create(mem_ctx, "objectuuid");
	torture_suite_add_simple_test(suite, "random-uuid", test_random_uuid);
	return suite;
}
