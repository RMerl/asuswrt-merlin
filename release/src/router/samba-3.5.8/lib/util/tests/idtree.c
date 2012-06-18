/* 
   Unix SMB/CIFS implementation.

   local testing of idtree routines.

   Copyright (C) Andrew Tridgell 2004
   
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

static bool torture_local_idtree_simple(struct torture_context *tctx)
{
	struct idr_context *idr;
	int i, ret;
	int *ids;
	int *present;
	extern int torture_numops;
	int n = torture_numops;
	TALLOC_CTX *mem_ctx = tctx;

	idr = idr_init(mem_ctx);

	ids = talloc_zero_array(mem_ctx, int, n);
	present = talloc_zero_array(mem_ctx, int, n);

	for (i=0;i<n;i++) {
		ids[i] = -1;
	}

	for (i=0;i<n;i++) {
		int ii = random() % n;
		void *p = idr_find(idr, ids[ii]);
		if (present[ii]) {
			if (p != &ids[ii]) {
				torture_fail(tctx, talloc_asprintf(tctx, 
						"wrong ptr at %d - %p should be %p", 
				       ii, p, &ids[ii]));
			}
			if (random() % 7 == 0) {
				if (idr_remove(idr, ids[ii]) != 0) {
					torture_fail(tctx, talloc_asprintf(tctx,
						"remove failed at %d (id=%d)", 
					       i, ids[ii]));
				}
				present[ii] = 0;
				ids[ii] = -1;
			}
		} else {
			if (p != NULL) {
				torture_fail(tctx, 
					     talloc_asprintf(tctx,
							     "non-present at %d gave %p (would be %d)", 
							     ii, p, 
							     (int)((((char *)p) - (char *)(&ids[0])) / sizeof(int))));
			}
			if (random() % 5) {
				ids[ii] = idr_get_new(idr, &ids[ii], n);
				if (ids[ii] < 0) {
					torture_fail(tctx, talloc_asprintf(tctx,
						"alloc failure at %d (ret=%d)", 
					       ii, ids[ii]));
				} else {
					present[ii] = 1;
				}
			}
		}
	}

	torture_comment(tctx, "done %d random ops\n", i);

	for (i=0;i<n;i++) {
		if (present[i]) {
			if (idr_remove(idr, ids[i]) != 0) {
				torture_fail(tctx, talloc_asprintf(tctx,
						"delete failed on cleanup at %d (id=%d)", 
				       i, ids[i]));
			}
		}
	}

	/* now test some limits */
	for (i=0;i<25000;i++) {
		ret = idr_get_new_above(idr, &ids[0], random() % 25000, 0x10000-3);
		torture_assert(tctx, ret != -1, "idr_get_new_above failed");
	}

	ret = idr_get_new_above(idr, &ids[0], 0x10000-2, 0x10000);
	torture_assert_int_equal(tctx, ret, 0x10000-2, "idr_get_new_above failed");
	ret = idr_get_new_above(idr, &ids[0], 0x10000-1, 0x10000);
	torture_assert_int_equal(tctx, ret, 0x10000-1, "idr_get_new_above failed");
	ret = idr_get_new_above(idr, &ids[0], 0x10000, 0x10000);
	torture_assert_int_equal(tctx, ret, 0x10000, "idr_get_new_above failed");
	ret = idr_get_new_above(idr, &ids[0], 0x10000+1, 0x10000);
	torture_assert_int_equal(tctx, ret, -1, "idr_get_new_above succeeded above limit");
	ret = idr_get_new_above(idr, &ids[0], 0x10000+2, 0x10000);
	torture_assert_int_equal(tctx, ret, -1, "idr_get_new_above succeeded above limit");

	torture_comment(tctx, "cleaned up\n");
	return true;
}

struct torture_suite *torture_local_idtree(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, "IDTREE");
	torture_suite_add_simple_test(suite, "idtree", torture_local_idtree_simple);
	return suite;
}
