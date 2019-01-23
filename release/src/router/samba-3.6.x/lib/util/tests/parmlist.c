/* 
   Unix SMB/CIFS implementation.

   parmlist testing

   Copyright (C) Jelmer Vernooij 2009
   
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
#include "../lib/util/parmlist.h"

static bool test_get_int(struct torture_context *tctx)
{
	struct parmlist *pctx = talloc_zero(tctx, struct parmlist);
	parmlist_set_string(pctx, "bar", "3");
	parmlist_set_string(pctx, "notint", "bla");
	torture_assert_int_equal(tctx, 3, parmlist_get_int(pctx, "bar", 42), 
							 "existing");
	torture_assert_int_equal(tctx, 42, parmlist_get_int(pctx, "foo", 42),
							 "default");
	torture_assert_int_equal(tctx, 0, parmlist_get_int(pctx, "notint", 42),
							 "Not an integer");
	return true;
}

static bool test_get_string(struct torture_context *tctx)
{
	struct parmlist *pctx = talloc_zero(tctx, struct parmlist);
	parmlist_set_string(pctx, "bar", "mystring");
	torture_assert_str_equal(tctx, "mystring", 
		parmlist_get_string(pctx, "bar", "bla"), "existing");
	torture_assert_str_equal(tctx, "bla", 
		parmlist_get_string(pctx, "foo", "bla"), "default");
	return true;
}

static bool test_get(struct torture_context *tctx)
{
	struct parmlist *pctx = talloc_zero(tctx, struct parmlist);
	struct parmlist_entry *e;
	parmlist_set_string(pctx, "bar", "mystring");

	e = parmlist_get(pctx, "bar");
	torture_assert(tctx, e != NULL, "entry");
	torture_assert_str_equal(tctx, e->key, "bar", "key");
	torture_assert_str_equal(tctx, e->value, "mystring", "value");

	e = parmlist_get(pctx, "non-existent");
	torture_assert(tctx, e == NULL, "non-existent");
	return true;
}

static bool test_get_bool(struct torture_context *tctx)
{
	struct parmlist *pctx = talloc_zero(tctx, struct parmlist);
	parmlist_set_string(pctx, "bar", "true");
	parmlist_set_string(pctx, "gasoline", "invalid");

	torture_assert(tctx, parmlist_get_bool(pctx, "bar", false), "set");
	torture_assert(tctx, !parmlist_get_bool(pctx, "foo", false), "default");
	torture_assert(tctx, !parmlist_get_bool(pctx, "gasoline", false), 
				   "invalid");
	return true;
}

static bool test_get_string_list(struct torture_context *tctx)
{
	struct parmlist *pctx = talloc_zero(tctx, struct parmlist);
	const char **ret;
	parmlist_set_string(pctx, "bar", "true, false");

	ret = parmlist_get_string_list(pctx, "bar", NULL);
	torture_assert_int_equal(tctx, str_list_length(ret), 2, "length");
	torture_assert_str_equal(tctx, "true", ret[0], "ret[0]");
	torture_assert_str_equal(tctx, "false", ret[1], "ret[1]");
	torture_assert(tctx, NULL == parmlist_get_string_list(pctx, "non-existent", NULL), "non-existent");

	return true;
}

struct torture_suite *torture_local_util_parmlist(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, "parmlist");

	torture_suite_add_simple_test(suite, "get_int", test_get_int);
	torture_suite_add_simple_test(suite, "get_string", test_get_string);
	torture_suite_add_simple_test(suite, "get", test_get);
	torture_suite_add_simple_test(suite, "get_bool", test_get_bool);
	torture_suite_add_simple_test(suite, "get_string_list", test_get_string_list);

	return suite;
}
