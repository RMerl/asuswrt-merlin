/*
   Unix SMB/CIFS implementation.

   anonymous_shared testing

   Copyright (C) Stefan Metzmacher 2011

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
#include "torture/local/proto.h"

static bool test_anonymous_shared_simple(struct torture_context *tctx)
{
	void *ptr;
	size_t len;

	torture_comment(tctx, "anonymous_shared_free(NULL)\n");
	anonymous_shared_free(NULL);

	len = 500;
	torture_comment(tctx, "anonymous_shared_allocate(%llu)\n",
			(unsigned long long)len);
	ptr = anonymous_shared_allocate(len);
	torture_assert(tctx, ptr, "valid pointer");
	memset(ptr, 0xfe, len);
	torture_comment(tctx, "anonymous_shared_free(ptr)\n");
	anonymous_shared_free(ptr);

	len = 50000;
	torture_comment(tctx, "anonymous_shared_allocate(%llu)\n",
			(unsigned long long)len);
	ptr = anonymous_shared_allocate(len);
	torture_assert(tctx, ptr, "valid pointer");
	memset(ptr, 0xfe, len);
	torture_comment(tctx, "anonymous_shared_free(ptr)\n");
	anonymous_shared_free(ptr);

	memset(&len, 0xFF, sizeof(len));
	torture_comment(tctx, "anonymous_shared_allocate(%llu)\n",
			(unsigned long long)len);
	ptr = anonymous_shared_allocate(len);
	torture_assert(tctx, ptr == NULL, "null pointer");

	return true;
}

/* local.anonymous_shared test suite creation */
struct torture_suite *torture_local_util_anonymous_shared(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, "anonymous_shared");

	torture_suite_add_simple_test(suite, "simple",
				      test_anonymous_shared_simple);

	return suite;
}
