/* 
   Unix SMB/CIFS implementation.
   SMB torture tester
   Copyright (C) Jelmer Vernooij 2006
   
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
#include "libcli/smb2/smb2.h"

#include "torture/smbtorture.h"
#include "torture/smb2/proto.h"
#include "../lib/util/dlinklist.h"

static bool wrap_simple_1smb2_test(struct torture_context *torture_ctx,
				   struct torture_tcase *tcase,
				   struct torture_test *test)
{
	bool (*fn) (struct torture_context *, struct smb2_tree *);
	bool ret;

	struct smb2_tree *tree1;

	if (!torture_smb2_connection(torture_ctx, &tree1))
		return false;

	fn = test->fn;

	ret = fn(torture_ctx, tree1);

	talloc_free(tree1);

	return ret;
}

struct torture_test *torture_suite_add_1smb2_test(struct torture_suite *suite,
						  const char *name,
						  bool (*run)(struct torture_context *,
							      struct smb2_tree *))
{
	struct torture_test *test; 
	struct torture_tcase *tcase;
	
	tcase = torture_suite_add_tcase(suite, name);

	test = talloc(tcase, struct torture_test);

	test->name = talloc_strdup(test, name);
	test->description = NULL;
	test->run = wrap_simple_1smb2_test;
	test->fn = run;
	test->dangerous = false;

	DLIST_ADD_END(tcase->tests, test, struct torture_test *);

	return test;
}


static bool wrap_simple_2smb2_test(struct torture_context *torture_ctx,
				   struct torture_tcase *tcase,
				   struct torture_test *test)
{
	bool (*fn) (struct torture_context *, struct smb2_tree *, struct smb2_tree *);
	bool ret;

	struct smb2_tree *tree1;
	struct smb2_tree *tree2;
	TALLOC_CTX *mem_ctx = talloc_new(torture_ctx);

	if (!torture_smb2_connection(torture_ctx, &tree1) ||
	    !torture_smb2_connection(torture_ctx, &tree2)) {
		return false;
	}

	talloc_steal(mem_ctx, tree1);
	talloc_steal(mem_ctx, tree2);

	fn = test->fn;

	ret = fn(torture_ctx, tree1, tree2);

	/* the test may already closed some of the connections */
	talloc_free(mem_ctx);

	return ret;
}


struct torture_test *torture_suite_add_2smb2_test(struct torture_suite *suite,
						  const char *name,
						  bool (*run)(struct torture_context *,
							      struct smb2_tree *,
							      struct smb2_tree *))
{
	struct torture_test *test;
	struct torture_tcase *tcase;

	tcase = torture_suite_add_tcase(suite, name);

	test = talloc(tcase, struct torture_test);

	test->name = talloc_strdup(test, name);
	test->description = NULL;
	test->run = wrap_simple_2smb2_test;
	test->fn = run;
	test->dangerous = false;

	DLIST_ADD_END(tcase->tests, test, struct torture_test *);

	return test;
}

NTSTATUS torture_smb2_init(void)
{
	struct torture_suite *suite = torture_suite_create(talloc_autofree_context(), "smb2");
	torture_suite_add_simple_test(suite, "connect", torture_smb2_connect);
	torture_suite_add_simple_test(suite, "scan", torture_smb2_scan);
	torture_suite_add_simple_test(suite, "scangetinfo", torture_smb2_getinfo_scan);
	torture_suite_add_simple_test(suite, "scansetinfo", torture_smb2_setinfo_scan);
	torture_suite_add_simple_test(suite, "scanfind", torture_smb2_find_scan);
	torture_suite_add_simple_test(suite, "getinfo", torture_smb2_getinfo);
	torture_suite_add_simple_test(suite, "setinfo", torture_smb2_setinfo);
	torture_suite_add_suite(suite, torture_smb2_lock_init());
	torture_suite_add_suite(suite, torture_smb2_read_init());
	torture_suite_add_suite(suite, torture_smb2_create_init());
	torture_suite_add_suite(suite, torture_smb2_acls_init());
	torture_suite_add_suite(suite, torture_smb2_notify_init());
	torture_suite_add_suite(suite, torture_smb2_durable_open_init());
	torture_suite_add_suite(suite, torture_smb2_dir_init());
	torture_suite_add_suite(suite, torture_smb2_lease_init());
	torture_suite_add_suite(suite, torture_smb2_compound_init());
	torture_suite_add_suite(suite, torture_smb2_oplocks_init());
	torture_suite_add_suite(suite, torture_smb2_streams_init());
	torture_suite_add_1smb2_test(suite, "bench-oplock", test_smb2_bench_oplock);
	torture_suite_add_1smb2_test(suite, "hold-oplock", test_smb2_hold_oplock);

	suite->description = talloc_strdup(suite, "SMB2-specific tests");

	torture_register_suite(suite);

	return NT_STATUS_OK;
}
