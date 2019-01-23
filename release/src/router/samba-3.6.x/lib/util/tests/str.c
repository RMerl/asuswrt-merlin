/* 
   Unix SMB/CIFS implementation.

   util_str testing

   Copyright (C) Jelmer Vernooij <jelmer@samba.org> 2007
   
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

static bool test_string_sub_simple(struct torture_context *tctx)
{
	char tmp[100];
	safe_strcpy(tmp, "foobar", sizeof(tmp));
	string_sub(tmp, "foo", "bar", sizeof(tmp));
	torture_assert_str_equal(tctx, tmp, "barbar", "invalid sub");
	return true;
}

static bool test_string_sub_multiple(struct torture_context *tctx)
{
	char tmp[100];
	safe_strcpy(tmp, "fooblafoo", sizeof(tmp));
	string_sub(tmp, "foo", "bar", sizeof(tmp));
	torture_assert_str_equal(tctx, tmp, "barblabar", "invalid sub");
	return true;
}

static bool test_string_sub_longer(struct torture_context *tctx)
{
	char tmp[100];
	safe_strcpy(tmp, "foobla", sizeof(tmp));
	string_sub(tmp, "foo", "blie", sizeof(tmp));
	torture_assert_str_equal(tctx, tmp, "bliebla", "invalid sub");
	return true;
}

static bool test_string_sub_shorter(struct torture_context *tctx)
{
	char tmp[100];
	safe_strcpy(tmp, "foobla", sizeof(tmp));
	string_sub(tmp, "foo", "bl", sizeof(tmp));
	torture_assert_str_equal(tctx, tmp, "blbla", "invalid sub");
	return true;
}

static bool test_string_sub_special_char(struct torture_context *tctx)
{
	char tmp[100];
	safe_strcpy(tmp, "foobla", sizeof(tmp));
	string_sub(tmp, "foo", "%b;l", sizeof(tmp));
	torture_assert_str_equal(tctx, tmp, "_b_lbla", "invalid sub");
	return true;
}

static bool test_string_sub_talloc_simple(struct torture_context *tctx)
{
	char *t;
	
	t = string_sub_talloc(tctx, "foobla", "foo", "bl");

	torture_assert_str_equal(tctx, t, "blbla", "invalid sub");

	return true;
}

static bool test_string_sub_talloc_multiple(struct torture_context *tctx)
{
	char *t;
	
	t = string_sub_talloc(tctx, "fooblafoo", "foo", "aapnootmies");

	torture_assert_str_equal(tctx, t, "aapnootmiesblaaapnootmies", 
				 "invalid sub");

	return true;
}



struct torture_suite *torture_local_util_str(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, "str");

	torture_suite_add_simple_test(suite, "string_sub_simple", 
				      test_string_sub_simple);

	torture_suite_add_simple_test(suite, "string_sub_multiple", 
				      test_string_sub_multiple);

	torture_suite_add_simple_test(suite, "string_sub_shorter", 
				      test_string_sub_shorter);

	torture_suite_add_simple_test(suite, "string_sub_longer", 
				      test_string_sub_longer);

	torture_suite_add_simple_test(suite, "string_sub_special_chars", 
				      test_string_sub_special_char);

	torture_suite_add_simple_test(suite, "string_sub_talloc_simple", 
				      test_string_sub_talloc_simple);

	torture_suite_add_simple_test(suite, "string_sub_talloc_multiple", 
				      test_string_sub_talloc_multiple);

	return suite;
}
