/*
   Unix SMB/CIFS implementation.

   local testing of DLIST_*() macros

   Copyright (C) Andrew Tridgell 2010

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
#include "lib/util/dlinklist.h"

struct listel {
	struct listel *next, *prev;
};

static bool torture_local_dlinklist_simple(struct torture_context *tctx)
{
	TALLOC_CTX *mem_ctx = talloc_new(tctx);
	struct listel *l1 = NULL, *l2 = NULL, *el, *el2;
	int i;

	torture_comment(tctx, "add 5 elements at front\n");
	for (i=0; i<5; i++) {
		el = talloc(mem_ctx, struct listel);
		DLIST_ADD(l1, el);
	}

	torture_comment(tctx, "add 5 elements at end\n");
	for (i=0; i<5; i++) {
		el = talloc(mem_ctx, struct listel);
		DLIST_ADD_END(l1, el, NULL);
	}

	torture_comment(tctx, "delete 3 from front\n");
	for (i=0; i < 3; i++) {
		el = l1;
		DLIST_REMOVE(l1, l1);
		DLIST_ADD(l2, el);
	}

	torture_comment(tctx, "delete 3 from back\n");
	for (i=0; i < 3; i++) {
		el = DLIST_TAIL(l1);
		DLIST_REMOVE(l1, el);
		DLIST_ADD_END(l2, el, NULL);
	}

	torture_comment(tctx, "count forward\n");
	for (i=0,el=l1; el; el=el->next) i++;
	torture_assert_int_equal(tctx, i, 4, "should have 4 elements");

	torture_comment(tctx, "count backwards\n");
	for (i=0,el=DLIST_TAIL(l1); el; el=DLIST_PREV(el)) i++;
	torture_assert_int_equal(tctx, i, 4, "should have 4 elements");

	torture_comment(tctx, "check DLIST_HEAD\n");
	el = DLIST_TAIL(l1);
	DLIST_HEAD(el, el2);
	torture_assert(tctx, el2 == l1, "should find head");

	torture_comment(tctx, "check DLIST_ADD_AFTER\n");
	el  = talloc(mem_ctx, struct listel);
	el2 = talloc(mem_ctx, struct listel);
	DLIST_ADD_AFTER(l1, el, l1);
	DLIST_ADD_AFTER(l1, el2, el);
	torture_assert(tctx, l1->next == el, "2nd in list");
	torture_assert(tctx, el->next == el2, "3rd in list");

	torture_comment(tctx, "check DLIST_PROMOTE\n");
	DLIST_PROMOTE(l1, el2);
	torture_assert(tctx, el2==l1, "1st in list");
	torture_assert(tctx, el2->next->next == el, "3rd in list");

	torture_comment(tctx, "check DLIST_DEMOTE\n");
	DLIST_DEMOTE(l1, el, NULL);
	torture_assert(tctx, el->next == NULL, "last in list");
	torture_assert(tctx, el2->prev == el, "backlink from head");

	torture_comment(tctx, "count forward\n");
	for (i=0,el=l1; el; el=el->next) i++;
	torture_assert_int_equal(tctx, i, 6, "should have 6 elements");

	torture_comment(tctx, "count backwards\n");
	for (i=0,el=DLIST_TAIL(l1); el; el=DLIST_PREV(el)) i++;
	torture_assert_int_equal(tctx, i, 6, "should have 6 elements");

	torture_comment(tctx, "check DLIST_CONCATENATE\n");
	DLIST_CONCATENATE(l1, l2, NULL);
	torture_comment(tctx, "count forward\n");
	for (i=0,el=l1; el; el=el->next) i++;
	torture_assert_int_equal(tctx, i, 12, "should have 12 elements");

	torture_comment(tctx, "count backwards\n");
	for (i=0,el=DLIST_TAIL(l1); el; el=DLIST_PREV(el)) i++;
	torture_assert_int_equal(tctx, i, 12, "should have 12 elements");

	torture_comment(tctx, "free forwards\n");
	for (el=l1; el; el=el2) {
		el2 = el->next;
		DLIST_REMOVE(l1, el);
		talloc_free(el);
	}

	torture_assert(tctx, l1 == NULL, "list empty");
	torture_assert_int_equal(tctx, talloc_total_blocks(mem_ctx), 1, "1 block");

	talloc_free(mem_ctx);
	return true;
}

struct torture_suite *torture_local_dlinklist(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, "dlinklist");
	torture_suite_add_simple_test(suite, "dlinklist", torture_local_dlinklist_simple);
	return suite;
}
