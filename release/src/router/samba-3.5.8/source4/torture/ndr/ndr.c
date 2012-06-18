/* 
   Unix SMB/CIFS implementation.
   test suite for winreg ndr operations

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
#include "torture/ndr/ndr.h"
#include "torture/ndr/proto.h"
#include "torture/torture.h"
#include "../lib/util/dlinklist.h"
#include "param/param.h"

struct ndr_pull_test_data {
	DATA_BLOB data;
	size_t struct_size;
	ndr_pull_flags_fn_t pull_fn;
	int ndr_flags;
};

static bool wrap_ndr_pull_test(struct torture_context *tctx,
							   struct torture_tcase *tcase,
							   struct torture_test *test)
{
	bool (*check_fn) (struct torture_context *ctx, void *data) = test->fn;
	const struct ndr_pull_test_data *data = (const struct ndr_pull_test_data *)test->data;
	void *ds = talloc_zero_size(tctx, data->struct_size);
	struct ndr_pull *ndr = ndr_pull_init_blob(&(data->data), tctx, lp_iconv_convenience(tctx->lp_ctx));

	ndr->flags |= LIBNDR_FLAG_REF_ALLOC;

	torture_assert_ndr_success(tctx, data->pull_fn(ndr, data->ndr_flags, ds),
				   "pulling");

	torture_assert(tctx, ndr->offset == ndr->data_size, 
				   talloc_asprintf(tctx, 
					   "%d unread bytes", ndr->data_size - ndr->offset));

	if (check_fn != NULL) 
		return check_fn(tctx, ds);
	else
		return true;
}

_PUBLIC_ struct torture_test *_torture_suite_add_ndr_pull_test(
					struct torture_suite *suite, 
					const char *name, ndr_pull_flags_fn_t pull_fn,
					DATA_BLOB db, 
					size_t struct_size,
					int ndr_flags,
					bool (*check_fn) (struct torture_context *ctx, void *data))
{
	struct torture_test *test; 
	struct torture_tcase *tcase;
	struct ndr_pull_test_data *data;
	
	tcase = torture_suite_add_tcase(suite, name);

	test = talloc(tcase, struct torture_test);

	test->name = talloc_strdup(test, name);
	test->description = NULL;
	test->run = wrap_ndr_pull_test;
	data = talloc(test, struct ndr_pull_test_data);
	data->data = db;
	data->ndr_flags = ndr_flags;
	data->struct_size = struct_size;
	data->pull_fn = pull_fn;
	test->data = data;
	test->fn = check_fn;
	test->dangerous = false;

	DLIST_ADD_END(tcase->tests, test, struct torture_test *);

	return test;
}

static bool test_check_string_terminator(struct torture_context *tctx)
{
	struct ndr_pull *ndr;
	DATA_BLOB blob;
	TALLOC_CTX *mem_ctx = tctx;

	/* Simple test */
	blob = strhex_to_data_blob(tctx, "0000");
	
	ndr = ndr_pull_init_blob(&blob, mem_ctx, lp_iconv_convenience(tctx->lp_ctx));

	torture_assert_ndr_success(tctx, ndr_check_string_terminator(ndr, 1, 2),
				   "simple check_string_terminator test failed");

	torture_assert(tctx, ndr->offset == 0,
		"check_string_terminator did not reset offset");

	if (NDR_ERR_CODE_IS_SUCCESS(ndr_check_string_terminator(ndr, 1, 3))) {
		torture_fail(tctx, "check_string_terminator checked beyond string boundaries");
	}

	torture_assert(tctx, ndr->offset == 0, 
		"check_string_terminator did not reset offset");

	talloc_free(ndr);

	blob = strhex_to_data_blob(tctx, "11220000");
	ndr = ndr_pull_init_blob(&blob, mem_ctx, lp_iconv_convenience(tctx->lp_ctx));

	torture_assert_ndr_success(tctx,
		ndr_check_string_terminator(ndr, 4, 1),
		"check_string_terminator failed to recognize terminator");

	torture_assert_ndr_success(tctx,
		ndr_check_string_terminator(ndr, 3, 1),
		"check_string_terminator failed to recognize terminator");

	if (NDR_ERR_CODE_IS_SUCCESS(ndr_check_string_terminator(ndr, 2, 1))) {
		torture_fail(tctx, "check_string_terminator erroneously reported terminator");
	}

	torture_assert(tctx, ndr->offset == 0,
		"check_string_terminator did not reset offset");
	return true;
}

static bool test_guid_from_string_valid(struct torture_context *tctx)
{
	/* FIXME */
	return true;
}

static bool test_guid_from_string_null(struct torture_context *tctx)
{
	struct GUID guid;
	torture_assert_ntstatus_equal(tctx, NT_STATUS_INVALID_PARAMETER, 
								  GUID_from_string(NULL, &guid), 
								  "NULL failed");
	return true;
}

static bool test_guid_from_string_invalid(struct torture_context *tctx)
{
	struct GUID g1;
	torture_assert_ntstatus_equal(tctx, NT_STATUS_INVALID_PARAMETER, 
								  GUID_from_string("bla", &g1),
								  "parameter not invalid");
	return true;
}	

static bool test_guid_from_string(struct torture_context *tctx)
{
	struct GUID g1, exp;
	torture_assert_ntstatus_ok(tctx,
							   GUID_from_string("00000001-0002-0003-0405-060708090a0b", &g1),
							   "invalid return code");
	exp.time_low = 1;
	exp.time_mid = 2;
	exp.time_hi_and_version = 3;
	exp.clock_seq[0] = 4;
	exp.clock_seq[1] = 5;
	exp.node[0] = 6;
	exp.node[1] = 7;
	exp.node[2] = 8;
	exp.node[3] = 9;
	exp.node[4] = 10;
	exp.node[5] = 11;
	torture_assert(tctx, GUID_equal(&g1, &exp), "UUID parsed incorrectly");
	torture_assert_ntstatus_ok(tctx,
							   GUID_from_string("{00000001-0002-0003-0405-060708090a0b}", &g1),
							   "invalid return code");
	torture_assert(tctx, GUID_equal(&g1, &exp), "UUID parsed incorrectly");

	return true;
}

static bool test_guid_string_valid(struct torture_context *tctx)
{
	struct GUID g;
	g.time_low = 1;
	g.time_mid = 2;
	g.time_hi_and_version = 3;
	g.clock_seq[0] = 4;
	g.clock_seq[1] = 5;
	g.node[0] = 6;
	g.node[1] = 7;
	g.node[2] = 8;
	g.node[3] = 9;
	g.node[4] = 10;
	g.node[5] = 11;
	torture_assert_str_equal(tctx, "00000001-0002-0003-0405-060708090a0b", GUID_string(tctx, &g), 
							 "parsing guid failed");
	return true;
}

static bool test_guid_string2_valid(struct torture_context *tctx)
{
	struct GUID g;
	g.time_low = 1;
	g.time_mid = 2;
	g.time_hi_and_version = 3;
	g.clock_seq[0] = 4;
	g.clock_seq[1] = 5;
	g.node[0] = 6;
	g.node[1] = 7;
	g.node[2] = 8;
	g.node[3] = 9;
	g.node[4] = 10;
	g.node[5] = 11;
	torture_assert_str_equal(tctx, "{00000001-0002-0003-0405-060708090a0b}", GUID_string2(tctx, &g), 
							 "parsing guid failed");
	return true;
}

static bool test_compare_uuid(struct torture_context *tctx)
{
	struct GUID g1, g2;
	ZERO_STRUCT(g1); ZERO_STRUCT(g2);
	torture_assert_int_equal(tctx, 0, GUID_compare(&g1, &g2), 
							 "GUIDs not equal");
	g1.time_low = 1;
	torture_assert_int_equal(tctx, 1, GUID_compare(&g1, &g2), 
							 "GUID diff invalid");

	g1.time_low = 10;
	torture_assert_int_equal(tctx, 10, GUID_compare(&g1, &g2), 
							 "GUID diff invalid");

	g1.time_low = 0;
	g1.clock_seq[1] = 20;
	torture_assert_int_equal(tctx, 20, GUID_compare(&g1, &g2), 
							 "GUID diff invalid");
	return true;
}

struct torture_suite *torture_local_ndr(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, "NDR");

	torture_suite_add_suite(suite, ndr_winreg_suite(suite));
	torture_suite_add_suite(suite, ndr_atsvc_suite(suite));
	torture_suite_add_suite(suite, ndr_lsa_suite(suite));
	torture_suite_add_suite(suite, ndr_epmap_suite(suite));
	torture_suite_add_suite(suite, ndr_dfs_suite(suite));
	torture_suite_add_suite(suite, ndr_netlogon_suite(suite));
	torture_suite_add_suite(suite, ndr_drsuapi_suite(suite));
	torture_suite_add_suite(suite, ndr_spoolss_suite(suite));
	torture_suite_add_suite(suite, ndr_samr_suite(suite));

	torture_suite_add_simple_test(suite, "string terminator", 
								   test_check_string_terminator);

	torture_suite_add_simple_test(suite, "guid_from_string_null", 
								   test_guid_from_string_null);

	torture_suite_add_simple_test(suite, "guid_from_string", 
								   test_guid_from_string);

	torture_suite_add_simple_test(suite, "guid_from_string_invalid", 
								   test_guid_from_string_invalid);

	torture_suite_add_simple_test(suite, "guid_string_valid", 
								   test_guid_string_valid);

	torture_suite_add_simple_test(suite, "guid_string2_valid", 
								   test_guid_string2_valid);

	torture_suite_add_simple_test(suite, "guid_from_string_valid", 
								   test_guid_from_string_valid);

	torture_suite_add_simple_test(suite, "compare_uuid", 
								   test_compare_uuid);

	return suite;
}

