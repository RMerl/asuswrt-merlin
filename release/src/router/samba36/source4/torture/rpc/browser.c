/*
   Unix SMB/CIFS implementation.

   test suite for browser rpc operations

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
#include "librpc/gen_ndr/ndr_browser_c.h"
#include "torture/rpc/torture_rpc.h"

bool test_BrowserrQueryOtherDomains(struct torture_context *tctx,
				    struct dcerpc_pipe *p)
{
	struct dcerpc_binding_handle *b = p->binding_handle;
	struct BrowserrQueryOtherDomains r;
	struct BrowserrSrvInfo info;
	struct BrowserrSrvInfo100Ctr ctr100;
	struct srvsvc_NetSrvInfo100 entries100[1];
	struct BrowserrSrvInfo101Ctr ctr101;
	struct srvsvc_NetSrvInfo101 entries101[1];
	uint32_t total_entries;
	NTSTATUS status;

	torture_comment(tctx, "dcerpc_BrowserrQueryOtherDomains\n");

	ZERO_STRUCT(r);
	ZERO_STRUCT(info);
	ZERO_STRUCT(ctr100);
	ZERO_STRUCT(entries100);
	ZERO_STRUCT(ctr101);
	ZERO_STRUCT(entries101);
	total_entries = 0;

	r.in.server_unc = talloc_asprintf(tctx,"\\\\%s",dcerpc_server_name(p));
	r.in.info = &info;
	r.out.info = &info;
	r.out.total_entries = &total_entries;

	info.level = 100;
	info.info.info100 = &ctr100;

	status = dcerpc_BrowserrQueryOtherDomains_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, "BrowserrQueryOtherDomains failed");
	torture_assert_werr_ok(tctx, r.out.result, "BrowserrQueryOtherDomains failed");
	torture_assert_int_equal(tctx, *r.out.total_entries, 0, "BrowserrQueryOtherDomains");

	info.info.info100 = &ctr100;
	ctr100.entries_read = ARRAY_SIZE(entries100);
	ctr100.entries = entries100;

	status = dcerpc_BrowserrQueryOtherDomains_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, "BrowserrQueryOtherDomains failed");
	torture_assert_werr_ok(tctx, r.out.result, "BrowserrQueryOtherDomains failed");
	torture_assert_int_equal(tctx, *r.out.total_entries, 0, "BrowserrQueryOtherDomains");

	info.info.info100 = NULL;
	status = dcerpc_BrowserrQueryOtherDomains_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, "BrowserrQueryOtherDomains failed");
	torture_assert_werr_equal(tctx, WERR_INVALID_PARAM, r.out.result,
				  "BrowserrQueryOtherDomains failed");

	info.level = 101;
	info.info.info101 = &ctr101;

	status = dcerpc_BrowserrQueryOtherDomains_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, "BrowserrQueryOtherDomains failed");
	torture_assert_werr_equal(tctx, WERR_UNKNOWN_LEVEL, r.out.result,
				  "BrowserrQueryOtherDomains");

	info.info.info101 = &ctr101;
	ctr101.entries_read = ARRAY_SIZE(entries101);
	ctr101.entries = entries101;

	status = dcerpc_BrowserrQueryOtherDomains_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, "BrowserrQueryOtherDomains failed");
	torture_assert_werr_equal(tctx, WERR_UNKNOWN_LEVEL, r.out.result,
				  "BrowserrQueryOtherDomains");

	info.info.info101 = NULL;
	status = dcerpc_BrowserrQueryOtherDomains_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, "BrowserrQueryOtherDomains failed");
	torture_assert_werr_equal(tctx, WERR_UNKNOWN_LEVEL, r.out.result,
				  "BrowserrQueryOtherDomains");

	info.level = 102;
	status = dcerpc_BrowserrQueryOtherDomains_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, "BrowserrQueryOtherDomains failed");
	torture_assert_werr_equal(tctx, WERR_UNKNOWN_LEVEL, r.out.result,
				  "BrowserrQueryOtherDomains");

	info.level = 0;
	status = dcerpc_BrowserrQueryOtherDomains_r(b, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, "BrowserrQueryOtherDomains failed");
	torture_assert_werr_equal(tctx, WERR_UNKNOWN_LEVEL, r.out.result,
				  "BrowserrQueryOtherDomains");

	return true;
}

struct torture_suite *torture_rpc_browser(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, "browser");
	struct torture_rpc_tcase *tcase = torture_suite_add_rpc_iface_tcase(suite, "browser", &ndr_table_browser);

	torture_rpc_tcase_add_test(tcase, "BrowserrQueryOtherDomains", test_BrowserrQueryOtherDomains);

	return suite;
}

