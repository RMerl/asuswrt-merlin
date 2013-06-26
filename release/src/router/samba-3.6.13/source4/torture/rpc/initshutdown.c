/* 
   Unix SMB/CIFS implementation.
   test suite for initshutdown operations

   Copyright (C) Tim Potter 2003
   Copyright (C) Jelmer Vernooij 2004-2005
   
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
#include "librpc/gen_ndr/ndr_initshutdown_c.h"
#include "torture/rpc/torture_rpc.h"

static void init_lsa_StringLarge(struct lsa_StringLarge *name, const char *s)
{
	name->string = s;
}


static bool test_Abort(struct torture_context *tctx, 
					   struct dcerpc_pipe *p)
{
	struct initshutdown_Abort r;
	NTSTATUS status;
	uint16_t server = 0x0;
	struct dcerpc_binding_handle *b = p->binding_handle;

	r.in.server = &server;
	
	status = dcerpc_initshutdown_Abort_r(b, tctx, &r);

	torture_assert_ntstatus_ok(tctx, status, 
							   "initshutdown_Abort failed");

	torture_assert_werr_ok(tctx, r.out.result, "initshutdown_Abort failed");

	return true;
}

static bool test_Init(struct torture_context *tctx, 
		      struct dcerpc_pipe *p)
{
	struct initshutdown_Init r;
	NTSTATUS status;
	uint16_t hostname = 0x0;
	struct dcerpc_binding_handle *b = p->binding_handle;

	r.in.hostname = &hostname;
	r.in.message = talloc(tctx, struct lsa_StringLarge);
	init_lsa_StringLarge(r.in.message, "spottyfood");
	r.in.force_apps = 1;
	r.in.timeout = 30;
	r.in.do_reboot = 1;

	status = dcerpc_initshutdown_Init_r(b, tctx, &r);

	torture_assert_ntstatus_ok(tctx, status, "initshutdown_Init failed");
	torture_assert_werr_ok(tctx, r.out.result, "initshutdown_Init failed");

	return test_Abort(tctx, p);
}

static bool test_InitEx(struct torture_context *tctx, 
						struct dcerpc_pipe *p)
{
	struct initshutdown_InitEx r;
	NTSTATUS status;
	uint16_t hostname = 0x0;
	struct dcerpc_binding_handle *b = p->binding_handle;

	r.in.hostname = &hostname;
	r.in.message = talloc(tctx, struct lsa_StringLarge);
	init_lsa_StringLarge(r.in.message, "spottyfood");
	r.in.force_apps = 1;
	r.in.timeout = 30;
	r.in.do_reboot = 1;
	r.in.reason = 0;

	status = dcerpc_initshutdown_InitEx_r(b, tctx, &r);

	torture_assert_ntstatus_ok(tctx, status, "initshutdown_InitEx failed");

	torture_assert_werr_ok(tctx, r.out.result, "initshutdown_InitEx failed");

	return test_Abort(tctx, p);
}


struct torture_suite *torture_rpc_initshutdown(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, "initshutdown");
	struct torture_rpc_tcase *tcase;
	struct torture_test *test;

	tcase = torture_suite_add_rpc_iface_tcase(suite, "initshutdown", 
						  &ndr_table_initshutdown);

	test = torture_rpc_tcase_add_test(tcase, "Init", test_Init);
	test->dangerous = true;
	test = torture_rpc_tcase_add_test(tcase, "InitEx", test_InitEx);
	test->dangerous = true;

	return suite;
}
