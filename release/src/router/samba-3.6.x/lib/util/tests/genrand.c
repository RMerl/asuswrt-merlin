/* 
   Unix SMB/CIFS implementation.

   local testing of random data routines.

   Copyright (C) Jelmer Vernooij <jelmer@samba.org>
   
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

static void dummy_reseed(void *userdata, int *d)
{
	*d = 42;
}

static bool test_reseed_callback(struct torture_context *tctx)
{
	set_rand_reseed_callback(dummy_reseed, NULL);
	return true;
}

static bool test_check_password_quality(struct torture_context *tctx)
{
	torture_assert(tctx, !check_password_quality(""), "empty password");
	torture_assert(tctx, !check_password_quality("a"), "one char password");
	torture_assert(tctx, !check_password_quality("aaaaaaaaaaaa"), "same char password");
	torture_assert(tctx, !check_password_quality("BLA"), "multiple upcases password");
	torture_assert(tctx, !check_password_quality("123"), "digits only");
	torture_assert(tctx, !check_password_quality("matthiéu"), "not enough high symbols");
	torture_assert(tctx, check_password_quality("abcdééàçè"), "valid");
	torture_assert(tctx, check_password_quality("A2e"), "valid");
	torture_assert(tctx, check_password_quality("BA2eLi443"), "valid");
	return true;
}

static bool test_generate_random_str(struct torture_context *tctx)
{
	TALLOC_CTX *mem_ctx = talloc_init(__FUNCTION__);
	char *r = generate_random_str(mem_ctx, 10);
	torture_assert_int_equal(tctx, strlen(r), 10, "right length generated");
	r = generate_random_str(mem_ctx, 5);
	torture_assert_int_equal(tctx, strlen(r), 5, "right length generated");
	return true;
}

struct torture_suite *torture_local_genrand(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, "genrand");
	torture_suite_add_simple_test(suite, "reseed_callback", test_reseed_callback);
	torture_suite_add_simple_test(suite, "check_password_quality", test_check_password_quality);
	torture_suite_add_simple_test(suite, "generate_random_str", test_generate_random_str);
	return suite;
}
