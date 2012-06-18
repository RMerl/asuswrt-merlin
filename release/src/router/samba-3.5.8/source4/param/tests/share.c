/* 
   Unix SMB/CIFS implementation.

   local testing of share code

   Copyright (C) Jelmer Vernooij 2007
   
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
#include "param/share.h"
#include "param/param.h"
#include "torture/torture.h"

static bool test_list_empty(struct torture_context *tctx, 
			    const void *tcase_data, 
			    const void *test_data)
{
	struct share_context *ctx = (struct share_context *)discard_const(tcase_data);
	int count;
	const char **names;

	torture_assert_ntstatus_ok(tctx, share_list_all(tctx, ctx, &count, &names),
							   "share_list_all failed");

	return true;
}

static bool test_create(struct torture_context *tctx, 
			const void *tcase_data, 
			const void *test_data)
{
	struct share_context *ctx = (struct share_context *)discard_const(tcase_data);
	int count;
	const char **names;
	int i;
	bool found = false;
	struct share_info inf[] = { 
		{ SHARE_INFO_STRING, SHARE_TYPE, discard_const_p(void *, "IPC$") },
		{ SHARE_INFO_STRING, SHARE_PATH, discard_const_p(void *, "/tmp/bla") }
	};
	NTSTATUS status;

	status = share_create(ctx, "bloe", inf, 2);

	if (NT_STATUS_EQUAL(status, NT_STATUS_NOT_IMPLEMENTED))
		torture_skip(tctx, "Not supported by backend");

	torture_assert_ntstatus_ok(tctx, status, "create_share failed");

	torture_assert_ntstatus_ok(tctx, share_list_all(tctx, ctx, &count, &names),
							   "share_list_all failed");

	torture_assert(tctx, count >= 1, "creating share failed");


	for (i = 0; i < count; i++) {
		found |= strcmp(names[i], "bloe") == 0;
	}

	torture_assert(tctx, found, "created share found");

	return true;
}


static bool test_create_invalid(struct torture_context *tctx, 
				const void *tcase_data, 
				const void *test_data)
{
	struct share_context *ctx = (struct share_context *)discard_const(tcase_data);
	NTSTATUS status;

	status = share_create(ctx, "bla", NULL, 0);

	if (NT_STATUS_EQUAL(status, NT_STATUS_NOT_IMPLEMENTED))
		torture_skip(tctx, "Not supported by backend");

	torture_assert_ntstatus_equal(tctx, NT_STATUS_INVALID_PARAMETER, 
				      status,
				      "create_share failed");

	torture_assert_ntstatus_equal(tctx, NT_STATUS_INVALID_PARAMETER, 
				      share_create(ctx, NULL, NULL, 0),
				      "create_share failed");

	return true;
}

static bool test_share_remove_invalid(struct torture_context *tctx, 
				      const void *tcase_data, 
				      const void *test_data)
{
	struct share_context *ctx = (struct share_context *)discard_const(tcase_data);
	NTSTATUS status;

	status = share_remove(ctx, "nonexistant");

	if (NT_STATUS_EQUAL(status, NT_STATUS_NOT_IMPLEMENTED))
		torture_skip(tctx, "Not supported by backend");

	torture_assert_ntstatus_equal(tctx, status, NT_STATUS_UNSUCCESSFUL, "remove fails");

	return true;
}



static bool test_share_remove(struct torture_context *tctx, 
			      const void *tcase_data, 
			      const void *test_data)
{
	struct share_context *ctx = (struct share_context *)discard_const(tcase_data);
	struct share_info inf[] = { 
		{ SHARE_INFO_STRING, SHARE_TYPE, discard_const_p(void *, "IPC$") },
		{ SHARE_INFO_STRING, SHARE_PATH, discard_const_p(void *, "/tmp/bla") }
	};
	NTSTATUS status;

	status = share_create(ctx, "blie", inf, 2);

	if (NT_STATUS_EQUAL(status, NT_STATUS_NOT_IMPLEMENTED))
		torture_skip(tctx, "Not supported by backend");

	torture_assert_ntstatus_ok(tctx, status, "create_share failed");

	torture_assert_ntstatus_ok(tctx, share_remove(ctx, "blie"), "remove failed");

	return true;
}

static bool test_double_create(struct torture_context *tctx, 
			       const void *tcase_data, 
			       const void *test_data)
{
	struct share_context *ctx = (struct share_context *)discard_const(tcase_data);
	struct share_info inf[] = { 
		{ SHARE_INFO_STRING, SHARE_TYPE, discard_const_p(void *, "IPC$") },
		{ SHARE_INFO_STRING, SHARE_PATH, discard_const_p(void *, "/tmp/bla") }
	};
	NTSTATUS status;

	status = share_create(ctx, "bla", inf, 2);

	if (NT_STATUS_EQUAL(status, NT_STATUS_NOT_IMPLEMENTED))
		torture_skip(tctx, "Not supported by backend");

	torture_assert_ntstatus_ok(tctx, status, "create_share failed");

	torture_assert_ntstatus_equal(tctx, NT_STATUS_OBJECT_NAME_COLLISION,
				      share_create(ctx, "bla", inf, 2),
				      "create_share failed");

	return true;
}

static void tcase_add_share_tests(struct torture_tcase *tcase)
{
	torture_tcase_add_test_const(tcase, "list_empty", test_list_empty,NULL);
	torture_tcase_add_test_const(tcase, "share_create", test_create, NULL);
	torture_tcase_add_test_const(tcase, "share_remove", test_share_remove,
			NULL);
	torture_tcase_add_test_const(tcase, "share_remove_invalid",
			test_share_remove_invalid, NULL);
	torture_tcase_add_test_const(tcase, "share_create_invalid",
			test_create_invalid, NULL);
	torture_tcase_add_test_const(tcase, "share_double_create",
			test_double_create, NULL);
}

static bool setup_ldb(struct torture_context *tctx, void **data)
{
	return NT_STATUS_IS_OK(share_get_context_by_name(tctx, "ldb", tctx->ev, tctx->lp_ctx, (struct share_context **)data));
}

static bool setup_classic(struct torture_context *tctx, void **data)
{
	return NT_STATUS_IS_OK(share_get_context_by_name(tctx, "classic", tctx->ev, tctx->lp_ctx, (struct share_context **)data));
}

static bool teardown(struct torture_context *tctx, void *data)
{
	talloc_free(data);
	return true;
}

struct torture_suite *torture_local_share(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, "SHARE");
	struct torture_tcase *tcase;

	share_init();

	tcase = torture_suite_add_tcase(suite, "ldb");
	torture_tcase_set_fixture(tcase, setup_ldb, teardown);
	tcase_add_share_tests(tcase);

	tcase = torture_suite_add_tcase(suite, "classic");
	torture_tcase_set_fixture(tcase, setup_classic, teardown);
	tcase_add_share_tests(tcase);

	return suite;
}
