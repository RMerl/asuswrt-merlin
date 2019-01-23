/* 
   Unix SMB/CIFS implementation.
   test suite for unixinfo rpc operations

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
#include "torture/rpc/torture_rpc.h"
#include "librpc/gen_ndr/ndr_unixinfo_c.h"
#include "libcli/security/security.h"

/**
  test the SidToUid interface
*/
static bool test_sidtouid(struct torture_context *tctx, struct dcerpc_pipe *p)
{
	struct unixinfo_SidToUid r;
	struct dom_sid *sid;
	uint64_t uid;
	struct dcerpc_binding_handle *b = p->binding_handle;
	
	sid = dom_sid_parse_talloc(tctx, "S-1-5-32-1234-5432");
	r.in.sid = *sid;
	r.out.uid = &uid;

	torture_assert_ntstatus_ok(tctx, dcerpc_unixinfo_SidToUid_r(b, tctx, &r),
		"SidToUid failed");
	if (NT_STATUS_EQUAL(NT_STATUS_NONE_MAPPED, r.out.result)) {
	} else torture_assert_ntstatus_ok(tctx, r.out.result, "SidToUid failed");

	return true;
}

/*
  test the UidToSid interface
*/
static bool test_uidtosid(struct torture_context *tctx, struct dcerpc_pipe *p)
{
	struct unixinfo_UidToSid r;
	struct dom_sid sid;
	struct dcerpc_binding_handle *b = p->binding_handle;

	r.in.uid = 1000;
	r.out.sid = &sid;

	torture_assert_ntstatus_ok(tctx, dcerpc_unixinfo_UidToSid_r(b, tctx, &r),
				   "UidToSid failed");
	torture_assert_ntstatus_ok(tctx, r.out.result, "UidToSid failed");
	return true;
}

static bool test_getpwuid(struct torture_context *tctx, 
			  struct dcerpc_pipe *p)
{
	uint64_t uids[512];
	uint32_t num_uids = ARRAY_SIZE(uids);
	uint32_t i;
	struct unixinfo_GetPWUid r;
	struct dcerpc_binding_handle *b = p->binding_handle;

	for (i=0; i<num_uids; i++) {
		uids[i] = i;
	}
	
	r.in.count = &num_uids;
	r.in.uids = uids;
	r.out.count = &num_uids;
	r.out.infos = talloc_array(tctx, struct unixinfo_GetPWUidInfo, num_uids);

	torture_assert_ntstatus_ok(tctx, dcerpc_unixinfo_GetPWUid_r(b, tctx, &r),
		"GetPWUid failed");

	torture_assert_ntstatus_ok(tctx, r.out.result, "GetPWUid failed");
	
	return true;
}

/*
  test the SidToGid interface
*/
static bool test_sidtogid(struct torture_context *tctx, struct dcerpc_pipe *p)
{
	struct unixinfo_SidToGid r;
	struct dom_sid *sid;
	uint64_t gid;
	struct dcerpc_binding_handle *b = p->binding_handle;

	sid = dom_sid_parse_talloc(tctx, "S-1-5-32-1234-5432");
	r.in.sid = *sid;
	r.out.gid = &gid;

	torture_assert_ntstatus_ok(tctx, dcerpc_unixinfo_SidToGid_r(b, tctx, &r),
		"SidToGid failed");
	if (NT_STATUS_EQUAL(NT_STATUS_NONE_MAPPED, r.out.result)) {
	} else torture_assert_ntstatus_ok(tctx, r.out.result, "SidToGid failed");

	return true;
}

/*
  test the GidToSid interface
*/
static bool test_gidtosid(struct torture_context *tctx, struct dcerpc_pipe *p)
{
	struct unixinfo_GidToSid r;
	struct dom_sid sid;
	struct dcerpc_binding_handle *b = p->binding_handle;

	r.in.gid = 1000;
	r.out.sid = &sid;

	torture_assert_ntstatus_ok(tctx, dcerpc_unixinfo_GidToSid_r(b, tctx, &r),
				   "GidToSid failed");
	torture_assert_ntstatus_ok(tctx, r.out.result, "GidToSid failed");

	return true;
}

struct torture_suite *torture_rpc_unixinfo(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite;
	struct torture_rpc_tcase *tcase;

	suite = torture_suite_create(mem_ctx, "unixinfo");
	tcase = torture_suite_add_rpc_iface_tcase(suite, "unixinfo", 
						  &ndr_table_unixinfo);

	torture_rpc_tcase_add_test(tcase, "sidtouid", test_sidtouid);
	torture_rpc_tcase_add_test(tcase, "uidtosid", test_uidtosid);
	torture_rpc_tcase_add_test(tcase, "getpwuid", test_getpwuid);
	torture_rpc_tcase_add_test(tcase, "sidtogid", test_sidtogid);
	torture_rpc_tcase_add_test(tcase, "gidtosid", test_gidtosid);

	return suite;
}
