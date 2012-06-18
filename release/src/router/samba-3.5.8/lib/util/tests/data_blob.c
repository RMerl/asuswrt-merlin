/* 
   Unix SMB/CIFS implementation.

   data blob testing

   Copyright (C) Jelmer Vernooij <jelmer@samba.org> 2008
   
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
#include "torture/torture.h"

static bool test_string(struct torture_context *tctx)
{
	DATA_BLOB blob = data_blob_string_const("bla");	

	torture_assert_int_equal(tctx, blob.length, 3, "blob length");
	torture_assert_str_equal(tctx, (char *)blob.data, "bla", "blob data");

	return true;
}

static bool test_string_null(struct torture_context *tctx)
{
	DATA_BLOB blob = data_blob_string_const_null("bla");	

	torture_assert_int_equal(tctx, blob.length, 4, "blob length");
	torture_assert_str_equal(tctx, (char *)blob.data, "bla", "blob data");

	return true;
}

static bool test_zero(struct torture_context *tctx)
{
	int i;
	DATA_BLOB z = data_blob_talloc_zero(tctx, 4);
	torture_assert_int_equal(tctx, z.length, 4, "length");
	for (i = 0; i < z.length; i++)
		torture_assert_int_equal(tctx, z.data[i], 0, "contents");
	data_blob_free(&z);
	return true;
}


static bool test_clear(struct torture_context *tctx)
{
	int i;
	DATA_BLOB z = data_blob("lalala", 6);
	torture_assert_int_equal(tctx, z.length, 6, "length");
	data_blob_clear(&z);
	for (i = 0; i < z.length; i++)
		torture_assert_int_equal(tctx, z.data[i], 0, "contents");
	data_blob_free(&z);
	return true;
}

static bool test_cmp(struct torture_context *tctx)
{
	DATA_BLOB a = data_blob_string_const("bla");
	DATA_BLOB b = data_blob_string_const("blae");
	torture_assert(tctx, data_blob_cmp(&a, &b) != 0, "cmp different");
	torture_assert(tctx, data_blob_cmp(&a, &a) == 0, "cmp self");
	return true;
}

static bool test_hex_string(struct torture_context *tctx)
{
	DATA_BLOB a = data_blob_string_const("\xC\xA\xF\xE");
	torture_assert_str_equal(tctx, data_blob_hex_string(tctx, &a), "0C0A0F0E", "hex string");
	return true;
}

struct torture_suite *torture_local_util_data_blob(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, "DATABLOB");

	torture_suite_add_simple_test(suite, "string", test_string);
	torture_suite_add_simple_test(suite, "string_null", test_string_null);
	torture_suite_add_simple_test(suite, "zero", test_zero);;
	torture_suite_add_simple_test(suite, "clear", test_clear);
	torture_suite_add_simple_test(suite, "cmp", test_cmp);
	torture_suite_add_simple_test(suite, "hex string", test_hex_string);

	return suite;
}
