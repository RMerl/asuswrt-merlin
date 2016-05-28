/* 
   Unix SMB/CIFS implementation.

   local testing of talloc routines.

   Copyright (C) Andrew Tridgell 2004
   
     ** NOTE! The following LGPL license applies to the talloc
     ** library. This does NOT imply that all of Samba is released
     ** under the LGPL
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "replace.h"
#include "system/time.h"
#include "talloc.h"

static struct timeval timeval_current(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv;
}

static double timeval_elapsed(struct timeval *tv)
{
	struct timeval tv2 = timeval_current();
	return (tv2.tv_sec - tv->tv_sec) + 
	       (tv2.tv_usec - tv->tv_usec)*1.0e-6;
}

#define torture_assert(test, expr, str) if (!(expr)) { \
	printf("failure: %s [\n%s: Expression %s failed: %s\n]\n", \
		test, __location__, #expr, str); \
	return false; \
}

#define torture_assert_str_equal(test, arg1, arg2, desc) \
	if (strcmp(arg1, arg2)) { \
		printf("failure: %s [\n%s: Expected %s, got %s: %s\n]\n", \
		   test, __location__, arg1, arg2, desc); \
		return false; \
	}

#if _SAMBA_BUILD_==3
#ifdef malloc
#undef malloc
#endif
#ifdef strdup
#undef strdup
#endif
#endif

#define CHECK_SIZE(test, ptr, tsize) do { \
	if (talloc_total_size(ptr) != (tsize)) { \
		printf("failed: %s [\nwrong '%s' tree size: got %u  expected %u\n]\n", \
		       test, #ptr, \
		       (unsigned)talloc_total_size(ptr), \
		       (unsigned)tsize); \
		talloc_report_full(ptr, stdout); \
		return false; \
	} \
} while (0)

#define CHECK_BLOCKS(test, ptr, tblocks) do { \
	if (talloc_total_blocks(ptr) != (tblocks)) { \
		printf("failed: %s [\nwrong '%s' tree blocks: got %u  expected %u\n]\n", \
		       test, #ptr, \
		       (unsigned)talloc_total_blocks(ptr), \
		       (unsigned)tblocks); \
		talloc_report_full(ptr, stdout); \
		return false; \
	} \
} while (0)

#define CHECK_PARENT(test, ptr, parent) do { \
	if (talloc_parent(ptr) != (parent)) { \
		printf("failed: %s [\n'%s' has wrong parent: got %p  expected %p\n]\n", \
		       test, #ptr, \
		       talloc_parent(ptr), \
		       (parent)); \
		talloc_report_full(ptr, stdout); \
		talloc_report_full(parent, stdout); \
		talloc_report_full(NULL, stdout); \
		return false; \
	} \
} while (0)


/*
  test references 
*/
static bool test_ref1(void)
{
	void *root, *p1, *p2, *ref, *r1;

	printf("test: ref1 [\nSINGLE REFERENCE FREE\n]\n");

	root = talloc_named_const(NULL, 0, "root");
	p1 = talloc_named_const(root, 1, "p1");
	p2 = talloc_named_const(p1, 1, "p2");
	talloc_named_const(p1, 1, "x1");
	talloc_named_const(p1, 2, "x2");
	talloc_named_const(p1, 3, "x3");

	r1 = talloc_named_const(root, 1, "r1");	
	ref = talloc_reference(r1, p2);
	talloc_report_full(root, stderr);

	CHECK_BLOCKS("ref1", p1, 5);
	CHECK_BLOCKS("ref1", p2, 1);
	CHECK_BLOCKS("ref1", r1, 2);

	fprintf(stderr, "Freeing p2\n");
	talloc_free(p2);
	talloc_report_full(root, stderr);

	CHECK_BLOCKS("ref1", p1, 5);
	CHECK_BLOCKS("ref1", p2, 1);
	CHECK_BLOCKS("ref1", r1, 1);

	fprintf(stderr, "Freeing p1\n");
	talloc_free(p1);
	talloc_report_full(root, stderr);

	CHECK_BLOCKS("ref1", r1, 1);

	fprintf(stderr, "Freeing r1\n");
	talloc_free(r1);
	talloc_report_full(NULL, stderr);

	fprintf(stderr, "Testing NULL\n");
	if (talloc_reference(root, NULL)) {
		return false;
	}

	CHECK_BLOCKS("ref1", root, 1);

	CHECK_SIZE("ref1", root, 0);

	talloc_free(root);
	printf("success: ref1\n");
	return true;
}

/*
  test references 
*/
static bool test_ref2(void)
{
	void *root, *p1, *p2, *ref, *r1;

	printf("test: ref2 [\nDOUBLE REFERENCE FREE\n]\n");
	root = talloc_named_const(NULL, 0, "root");
	p1 = talloc_named_const(root, 1, "p1");
	talloc_named_const(p1, 1, "x1");
	talloc_named_const(p1, 1, "x2");
	talloc_named_const(p1, 1, "x3");
	p2 = talloc_named_const(p1, 1, "p2");

	r1 = talloc_named_const(root, 1, "r1");	
	ref = talloc_reference(r1, p2);
	talloc_report_full(root, stderr);

	CHECK_BLOCKS("ref2", p1, 5);
	CHECK_BLOCKS("ref2", p2, 1);
	CHECK_BLOCKS("ref2", r1, 2);

	fprintf(stderr, "Freeing ref\n");
	talloc_free(ref);
	talloc_report_full(root, stderr);

	CHECK_BLOCKS("ref2", p1, 5);
	CHECK_BLOCKS("ref2", p2, 1);
	CHECK_BLOCKS("ref2", r1, 1);

	fprintf(stderr, "Freeing p2\n");
	talloc_free(p2);
	talloc_report_full(root, stderr);

	CHECK_BLOCKS("ref2", p1, 4);
	CHECK_BLOCKS("ref2", r1, 1);

	fprintf(stderr, "Freeing p1\n");
	talloc_free(p1);
	talloc_report_full(root, stderr);

	CHECK_BLOCKS("ref2", r1, 1);

	fprintf(stderr, "Freeing r1\n");
	talloc_free(r1);
	talloc_report_full(root, stderr);

	CHECK_SIZE("ref2", root, 0);

	talloc_free(root);
	printf("success: ref2\n");
	return true;
}

/*
  test references 
*/
static bool test_ref3(void)
{
	void *root, *p1, *p2, *ref, *r1;

	printf("test: ref3 [\nPARENT REFERENCE FREE\n]\n");

	root = talloc_named_const(NULL, 0, "root");
	p1 = talloc_named_const(root, 1, "p1");
	p2 = talloc_named_const(root, 1, "p2");
	r1 = talloc_named_const(p1, 1, "r1");
	ref = talloc_reference(p2, r1);
	talloc_report_full(root, stderr);

	CHECK_BLOCKS("ref3", p1, 2);
	CHECK_BLOCKS("ref3", p2, 2);
	CHECK_BLOCKS("ref3", r1, 1);

	fprintf(stderr, "Freeing p1\n");
	talloc_free(p1);
	talloc_report_full(root, stderr);

	CHECK_BLOCKS("ref3", p2, 2);
	CHECK_BLOCKS("ref3", r1, 1);

	fprintf(stderr, "Freeing p2\n");
	talloc_free(p2);
	talloc_report_full(root, stderr);

	CHECK_SIZE("ref3", root, 0);

	talloc_free(root);

	printf("success: ref3\n");
	return true;
}

/*
  test references 
*/
static bool test_ref4(void)
{
	void *root, *p1, *p2, *ref, *r1;

	printf("test: ref4 [\nREFERRER REFERENCE FREE\n]\n");

	root = talloc_named_const(NULL, 0, "root");
	p1 = talloc_named_const(root, 1, "p1");
	talloc_named_const(p1, 1, "x1");
	talloc_named_const(p1, 1, "x2");
	talloc_named_const(p1, 1, "x3");
	p2 = talloc_named_const(p1, 1, "p2");

	r1 = talloc_named_const(root, 1, "r1");	
	ref = talloc_reference(r1, p2);
	talloc_report_full(root, stderr);

	CHECK_BLOCKS("ref4", p1, 5);
	CHECK_BLOCKS("ref4", p2, 1);
	CHECK_BLOCKS("ref4", r1, 2);

	fprintf(stderr, "Freeing r1\n");
	talloc_free(r1);
	talloc_report_full(root, stderr);

	CHECK_BLOCKS("ref4", p1, 5);
	CHECK_BLOCKS("ref4", p2, 1);

	fprintf(stderr, "Freeing p2\n");
	talloc_free(p2);
	talloc_report_full(root, stderr);

	CHECK_BLOCKS("ref4", p1, 4);

	fprintf(stderr, "Freeing p1\n");
	talloc_free(p1);
	talloc_report_full(root, stderr);

	CHECK_SIZE("ref4", root, 0);

	talloc_free(root);

	printf("success: ref4\n");
	return true;
}


/*
  test references 
*/
static bool test_unlink1(void)
{
	void *root, *p1, *p2, *ref, *r1;

	printf("test: unlink [\nUNLINK\n]\n");

	root = talloc_named_const(NULL, 0, "root");
	p1 = talloc_named_const(root, 1, "p1");
	talloc_named_const(p1, 1, "x1");
	talloc_named_const(p1, 1, "x2");
	talloc_named_const(p1, 1, "x3");
	p2 = talloc_named_const(p1, 1, "p2");

	r1 = talloc_named_const(p1, 1, "r1");	
	ref = talloc_reference(r1, p2);
	talloc_report_full(root, stderr);

	CHECK_BLOCKS("unlink", p1, 7);
	CHECK_BLOCKS("unlink", p2, 1);
	CHECK_BLOCKS("unlink", r1, 2);

	fprintf(stderr, "Unreferencing r1\n");
	talloc_unlink(r1, p2);
	talloc_report_full(root, stderr);

	CHECK_BLOCKS("unlink", p1, 6);
	CHECK_BLOCKS("unlink", p2, 1);
	CHECK_BLOCKS("unlink", r1, 1);

	fprintf(stderr, "Freeing p1\n");
	talloc_free(p1);
	talloc_report_full(root, stderr);

	CHECK_SIZE("unlink", root, 0);

	talloc_free(root);

	printf("success: unlink\n");
	return true;
}

static int fail_destructor(void *ptr)
{
	return -1;
}

/*
  miscellaneous tests to try to get a higher test coverage percentage
*/
static bool test_misc(void)
{
	void *root, *p1;
	char *p2;
	double *d;
	const char *name;

	printf("test: misc [\nMISCELLANEOUS\n]\n");

	root = talloc_new(NULL);

	p1 = talloc_size(root, 0x7fffffff);
	torture_assert("misc", !p1, "failed: large talloc allowed\n");

	p1 = talloc_strdup(root, "foo");
	talloc_increase_ref_count(p1);
	talloc_increase_ref_count(p1);
	talloc_increase_ref_count(p1);
	CHECK_BLOCKS("misc", p1, 1);
	CHECK_BLOCKS("misc", root, 2);
	talloc_free(p1);
	CHECK_BLOCKS("misc", p1, 1);
	CHECK_BLOCKS("misc", root, 2);
	talloc_unlink(NULL, p1);
	CHECK_BLOCKS("misc", p1, 1);
	CHECK_BLOCKS("misc", root, 2);
	p2 = talloc_strdup(p1, "foo");
	torture_assert("misc", talloc_unlink(root, p2) == -1,
				   "failed: talloc_unlink() of non-reference context should return -1\n");
	torture_assert("misc", talloc_unlink(p1, p2) == 0,
		"failed: talloc_unlink() of parent should succeed\n");
	talloc_free(p1);
	CHECK_BLOCKS("misc", p1, 1);
	CHECK_BLOCKS("misc", root, 2);

	name = talloc_set_name(p1, "my name is %s", "foo");
	torture_assert_str_equal("misc", talloc_get_name(p1), "my name is foo",
		"failed: wrong name after talloc_set_name(my name is foo)");
	CHECK_BLOCKS("misc", p1, 2);
	CHECK_BLOCKS("misc", root, 3);

	talloc_set_name_const(p1, NULL);
	torture_assert_str_equal ("misc", talloc_get_name(p1), "UNNAMED",
		"failed: wrong name after talloc_set_name(NULL)");
	CHECK_BLOCKS("misc", p1, 2);
	CHECK_BLOCKS("misc", root, 3);

	torture_assert("misc", talloc_free(NULL) == -1, 
				   "talloc_free(NULL) should give -1\n");

	talloc_set_destructor(p1, fail_destructor);
	torture_assert("misc", talloc_free(p1) == -1, 
		"Failed destructor should cause talloc_free to fail\n");
	talloc_set_destructor(p1, NULL);

	talloc_report(root, stderr);


	p2 = (char *)talloc_zero_size(p1, 20);
	torture_assert("misc", p2[19] == 0, "Failed to give zero memory\n");
	talloc_free(p2);

	torture_assert("misc", talloc_strdup(root, NULL) == NULL,
		"failed: strdup on NULL should give NULL\n");

	p2 = talloc_strndup(p1, "foo", 2);
	torture_assert("misc", strcmp("fo", p2) == 0, 
				   "strndup doesn't work\n");
	p2 = talloc_asprintf_append(p2, "o%c", 'd');
	torture_assert("misc", strcmp("food", p2) == 0, 
				   "talloc_asprintf_append doesn't work\n");
	CHECK_BLOCKS("misc", p2, 1);
	CHECK_BLOCKS("misc", p1, 3);

	p2 = talloc_asprintf_append(NULL, "hello %s", "world");
	torture_assert("misc", strcmp("hello world", p2) == 0,
		"talloc_asprintf_append doesn't work\n");
	CHECK_BLOCKS("misc", p2, 1);
	CHECK_BLOCKS("misc", p1, 3);
	talloc_free(p2);

	d = talloc_array(p1, double, 0x20000000);
	torture_assert("misc", !d, "failed: integer overflow not detected\n");

	d = talloc_realloc(p1, d, double, 0x20000000);
	torture_assert("misc", !d, "failed: integer overflow not detected\n");

	talloc_free(p1);
	CHECK_BLOCKS("misc", root, 1);

	p1 = talloc_named(root, 100, "%d bytes", 100);
	CHECK_BLOCKS("misc", p1, 2);
	CHECK_BLOCKS("misc", root, 3);
	talloc_unlink(root, p1);

	p1 = talloc_init("%d bytes", 200);
	p2 = talloc_asprintf(p1, "my test '%s'", "string");
	torture_assert_str_equal("misc", p2, "my test 'string'",
		"failed: talloc_asprintf(\"my test '%%s'\", \"string\") gave: \"%s\"");
	CHECK_BLOCKS("misc", p1, 3);
	CHECK_SIZE("misc", p2, 17);
	CHECK_BLOCKS("misc", root, 1);
	talloc_unlink(NULL, p1);

	p1 = talloc_named_const(root, 10, "p1");
	p2 = (char *)talloc_named_const(root, 20, "p2");
	(void)talloc_reference(p1, p2);
	talloc_report_full(root, stderr);
	talloc_unlink(root, p2);
	talloc_report_full(root, stderr);
	CHECK_BLOCKS("misc", p2, 1);
	CHECK_BLOCKS("misc", p1, 2);
	CHECK_BLOCKS("misc", root, 3);
	talloc_unlink(p1, p2);
	talloc_unlink(root, p1);

	p1 = talloc_named_const(root, 10, "p1");
	p2 = (char *)talloc_named_const(root, 20, "p2");
	(void)talloc_reference(NULL, p2);
	talloc_report_full(root, stderr);
	talloc_unlink(root, p2);
	talloc_report_full(root, stderr);
	CHECK_BLOCKS("misc", p2, 1);
	CHECK_BLOCKS("misc", p1, 1);
	CHECK_BLOCKS("misc", root, 2);
	talloc_unlink(NULL, p2);
	talloc_unlink(root, p1);

	/* Test that talloc_unlink is a no-op */

	torture_assert("misc", talloc_unlink(root, NULL) == -1,
		"failed: talloc_unlink(root, NULL) == -1\n");

	talloc_report(root, stderr);
	talloc_report(NULL, stderr);

	CHECK_SIZE("misc", root, 0);

	talloc_free(root);

	CHECK_SIZE("misc", NULL, 0);

	talloc_enable_leak_report();
	talloc_enable_leak_report_full();

	printf("success: misc\n");

	return true;
}


/*
  test realloc
*/
static bool test_realloc(void)
{
	void *root, *p1, *p2;

	printf("test: realloc [\nREALLOC\n]\n");

	root = talloc_new(NULL);

	p1 = talloc_size(root, 10);
	CHECK_SIZE("realloc", p1, 10);

	p1 = talloc_realloc_size(NULL, p1, 20);
	CHECK_SIZE("realloc", p1, 20);

	talloc_new(p1);

	p2 = talloc_realloc_size(p1, NULL, 30);

	talloc_new(p1);

	p2 = talloc_realloc_size(p1, p2, 40);

	CHECK_SIZE("realloc", p2, 40);
	CHECK_SIZE("realloc", root, 60);
	CHECK_BLOCKS("realloc", p1, 4);

	p1 = talloc_realloc_size(NULL, p1, 20);
	CHECK_SIZE("realloc", p1, 60);

	talloc_increase_ref_count(p2);
	torture_assert("realloc", talloc_realloc_size(NULL, p2, 5) == NULL,
		"failed: talloc_realloc() on a referenced pointer should fail\n");
	CHECK_BLOCKS("realloc", p1, 4);

	talloc_realloc_size(NULL, p2, 0);
	talloc_realloc_size(NULL, p2, 0);
	CHECK_BLOCKS("realloc", p1, 3);

	torture_assert("realloc", talloc_realloc_size(NULL, p1, 0x7fffffff) == NULL,
		"failed: oversize talloc should fail\n");

	talloc_realloc_size(NULL, p1, 0);

	CHECK_BLOCKS("realloc", root, 1);
	CHECK_SIZE("realloc", root, 0);

	talloc_free(root);

	printf("success: REALLOC\n");

	return true;
}

/*
  test realloc with a child
*/
static bool test_realloc_child(void)
{
	void *root;
	struct el2 {
		const char *name;
	} *el2;	
	struct el1 {
		int count;
		struct el2 **list, **list2, **list3;
	} *el1;

	printf("test: REALLOC WITH CHILD\n");

	root = talloc_new(NULL);

	el1 = talloc(root, struct el1);
	el1->list = talloc(el1, struct el2 *);
	el1->list[0] = talloc(el1->list, struct el2);
	el1->list[0]->name = talloc_strdup(el1->list[0], "testing");

	el1->list2 = talloc(el1, struct el2 *);
	el1->list2[0] = talloc(el1->list2, struct el2);
	el1->list2[0]->name = talloc_strdup(el1->list2[0], "testing2");

	el1->list3 = talloc(el1, struct el2 *);
	el1->list3[0] = talloc(el1->list3, struct el2);
	el1->list3[0]->name = talloc_strdup(el1->list3[0], "testing2");
	
	el2 = talloc(el1->list, struct el2);
	el2 = talloc(el1->list2, struct el2);
	el2 = talloc(el1->list3, struct el2);

	el1->list = talloc_realloc(el1, el1->list, struct el2 *, 100);
	el1->list2 = talloc_realloc(el1, el1->list2, struct el2 *, 200);
	el1->list3 = talloc_realloc(el1, el1->list3, struct el2 *, 300);

	talloc_free(root);

	printf("success: REALLOC WITH CHILD\n");
	return true;
}

/*
  test type checking
*/
static bool test_type(void)
{
	void *root;
	struct el1 {
		int count;
	};
	struct el2 {
		int count;
	};
	struct el1 *el1;

	printf("test: type [\ntalloc type checking\n]\n");

	root = talloc_new(NULL);

	el1 = talloc(root, struct el1);

	el1->count = 1;

	torture_assert("type", talloc_get_type(el1, struct el1) == el1,
		"type check failed on el1\n");
	torture_assert("type", talloc_get_type(el1, struct el2) == NULL,
		"type check failed on el1 with el2\n");
	talloc_set_type(el1, struct el2);
	torture_assert("type", talloc_get_type(el1, struct el2) == (struct el2 *)el1,
		"type set failed on el1 with el2\n");

	talloc_free(root);

	printf("success: type\n");
	return true;
}

/*
  test steal
*/
static bool test_steal(void)
{
	void *root, *p1, *p2;

	printf("test: steal [\nSTEAL\n]\n");

	root = talloc_new(NULL);

	p1 = talloc_array(root, char, 10);
	CHECK_SIZE("steal", p1, 10);

	p2 = talloc_realloc(root, NULL, char, 20);
	CHECK_SIZE("steal", p1, 10);
	CHECK_SIZE("steal", root, 30);

	torture_assert("steal", talloc_steal(p1, NULL) == NULL,
		"failed: stealing NULL should give NULL\n");

	torture_assert("steal", talloc_steal(p1, p1) == p1,
		"failed: stealing to ourselves is a nop\n");
	CHECK_BLOCKS("steal", root, 3);
	CHECK_SIZE("steal", root, 30);

	talloc_steal(NULL, p1);
	talloc_steal(NULL, p2);
	CHECK_BLOCKS("steal", root, 1);
	CHECK_SIZE("steal", root, 0);

	talloc_free(p1);
	talloc_steal(root, p2);
	CHECK_BLOCKS("steal", root, 2);
	CHECK_SIZE("steal", root, 20);
	
	talloc_free(p2);

	CHECK_BLOCKS("steal", root, 1);
	CHECK_SIZE("steal", root, 0);

	talloc_free(root);

	p1 = talloc_size(NULL, 3);
	talloc_report_full(NULL, stderr);
	CHECK_SIZE("steal", NULL, 3);
	talloc_free(p1);

	printf("success: steal\n");
	return true;
}

/*
  test move
*/
static bool test_move(void)
{
	void *root;
	struct t_move {
		char *p;
		int *x;
	} *t1, *t2;

	printf("test: move [\nMOVE\n]\n");

	root = talloc_new(NULL);

	t1 = talloc(root, struct t_move);
	t2 = talloc(root, struct t_move);
	t1->p = talloc_strdup(t1, "foo");
	t1->x = talloc(t1, int);
	*t1->x = 42;

	t2->p = talloc_move(t2, &t1->p);
	t2->x = talloc_move(t2, &t1->x);
	torture_assert("move", t1->p == NULL && t1->x == NULL &&
	    strcmp(t2->p, "foo") == 0 && *t2->x == 42,
		"talloc move failed");

	talloc_free(root);

	printf("success: move\n");

	return true;
}

/*
  test talloc_realloc_fn
*/
static bool test_realloc_fn(void)
{
	void *root, *p1;

	printf("test: realloc_fn [\ntalloc_realloc_fn\n]\n");

	root = talloc_new(NULL);

	p1 = talloc_realloc_fn(root, NULL, 10);
	CHECK_BLOCKS("realloc_fn", root, 2);
	CHECK_SIZE("realloc_fn", root, 10);
	p1 = talloc_realloc_fn(root, p1, 20);
	CHECK_BLOCKS("realloc_fn", root, 2);
	CHECK_SIZE("realloc_fn", root, 20);
	p1 = talloc_realloc_fn(root, p1, 0);
	CHECK_BLOCKS("realloc_fn", root, 1);
	CHECK_SIZE("realloc_fn", root, 0);

	talloc_free(root);

	printf("success: realloc_fn\n");
	return true;
}


static bool test_unref_reparent(void)
{
	void *root, *p1, *p2, *c1;

	printf("test: unref_reparent [\nUNREFERENCE AFTER PARENT FREED\n]\n");

	root = talloc_named_const(NULL, 0, "root");
	p1 = talloc_named_const(root, 1, "orig parent");
	p2 = talloc_named_const(root, 1, "parent by reference");

	c1 = talloc_named_const(p1, 1, "child");
	talloc_reference(p2, c1);

	CHECK_PARENT("unref_reparent", c1, p1);

	talloc_free(p1);

	CHECK_PARENT("unref_reparent", c1, p2);

	talloc_unlink(p2, c1);

	CHECK_SIZE("unref_reparent", root, 1);

	talloc_free(p2);
	talloc_free(root);

	printf("success: unref_reparent\n");
	return true;
}

/*
  measure the speed of talloc versus malloc
*/
static bool test_speed(void)
{
	void *ctx = talloc_new(NULL);
	unsigned count;
	const int loop = 1000;
	int i;
	struct timeval tv;

	printf("test: speed [\nTALLOC VS MALLOC SPEED\n]\n");

	tv = timeval_current();
	count = 0;
	do {
		void *p1, *p2, *p3;
		for (i=0;i<loop;i++) {
			p1 = talloc_size(ctx, loop % 100);
			p2 = talloc_strdup(p1, "foo bar");
			p3 = talloc_size(p1, 300);
			talloc_free(p1);
		}
		count += 3 * loop;
	} while (timeval_elapsed(&tv) < 5.0);

	fprintf(stderr, "talloc: %.0f ops/sec\n", count/timeval_elapsed(&tv));

	talloc_free(ctx);

	tv = timeval_current();
	count = 0;
	do {
		void *p1, *p2, *p3;
		for (i=0;i<loop;i++) {
			p1 = malloc(loop % 100);
			p2 = strdup("foo bar");
			p3 = malloc(300);
			free(p1);
			free(p2);
			free(p3);
		}
		count += 3 * loop;
	} while (timeval_elapsed(&tv) < 5.0);
	fprintf(stderr, "malloc: %.0f ops/sec\n", count/timeval_elapsed(&tv));

	printf("success: speed\n");

	return true;
}

static bool test_lifeless(void)
{
	void *top = talloc_new(NULL);
	char *parent, *child; 
	void *child_owner = talloc_new(NULL);

	printf("test: lifeless [\nTALLOC_UNLINK LOOP\n]\n");

	parent = talloc_strdup(top, "parent");
	child = talloc_strdup(parent, "child");  
	(void)talloc_reference(child, parent);
	(void)talloc_reference(child_owner, child); 
	talloc_report_full(top, stderr);
	talloc_unlink(top, parent);
	talloc_free(child);
	talloc_report_full(top, stderr);
	talloc_free(top);
	talloc_free(child_owner);
	talloc_free(child);

	printf("success: lifeless\n");
	return true;
}

static int loop_destructor_count;

static int test_loop_destructor(char *ptr)
{
	loop_destructor_count++;
	return 0;
}

static bool test_loop(void)
{
	void *top = talloc_new(NULL);
	char *parent;
	struct req1 {
		char *req2, *req3;
	} *req1;

	printf("test: loop [\nTALLOC LOOP DESTRUCTION\n]\n");

	parent = talloc_strdup(top, "parent");
	req1 = talloc(parent, struct req1);
	req1->req2 = talloc_strdup(req1, "req2");  
	talloc_set_destructor(req1->req2, test_loop_destructor);
	req1->req3 = talloc_strdup(req1, "req3");
	(void)talloc_reference(req1->req3, req1);
	talloc_report_full(top, stderr);
	talloc_free(parent);
	talloc_report_full(top, stderr);
	talloc_report_full(NULL, stderr);
	talloc_free(top);

	torture_assert("loop", loop_destructor_count == 1, 
				   "FAILED TO FIRE LOOP DESTRUCTOR\n");
	loop_destructor_count = 0;

	printf("success: loop\n");
	return true;
}

static int fail_destructor_str(char *ptr)
{
	return -1;
}

static bool test_free_parent_deny_child(void)
{
	void *top = talloc_new(NULL);
	char *level1;
	char *level2;
	char *level3;

	printf("test: free_parent_deny_child [\nTALLOC FREE PARENT DENY CHILD\n]\n");

	level1 = talloc_strdup(top, "level1");
	level2 = talloc_strdup(level1, "level2");
	level3 = talloc_strdup(level2, "level3");

	talloc_set_destructor(level3, fail_destructor_str);
	talloc_free(level1);
	talloc_set_destructor(level3, NULL);

	CHECK_PARENT("free_parent_deny_child", level3, top);

	talloc_free(top);

	printf("success: free_parent_deny_child\n");
	return true;
}

static bool test_talloc_ptrtype(void)
{
	void *top = talloc_new(NULL);
	struct struct1 {
		int foo;
		int bar;
	} *s1, *s2, **s3, ***s4;
	const char *location1;
	const char *location2;
	const char *location3;
	const char *location4;

	printf("test: ptrtype [\nTALLOC PTRTYPE\n]\n");

	s1 = talloc_ptrtype(top, s1);location1 = __location__;

	if (talloc_get_size(s1) != sizeof(struct struct1)) {
		printf("failure: ptrtype [\n"
		  "talloc_ptrtype() allocated the wrong size %lu (should be %lu)\n"
		  "]\n", (unsigned long)talloc_get_size(s1),
		           (unsigned long)sizeof(struct struct1));
		return false;
	}

	if (strcmp(location1, talloc_get_name(s1)) != 0) {
		printf("failure: ptrtype [\n"
		  "talloc_ptrtype() sets the wrong name '%s' (should be '%s')\n]\n",
			talloc_get_name(s1), location1);
		return false;
	}

	s2 = talloc_array_ptrtype(top, s2, 10);location2 = __location__;

	if (talloc_get_size(s2) != (sizeof(struct struct1) * 10)) {
		printf("failure: ptrtype [\n"
			   "talloc_array_ptrtype() allocated the wrong size "
		       "%lu (should be %lu)\n]\n",
			(unsigned long)talloc_get_size(s2),
		    (unsigned long)(sizeof(struct struct1)*10));
		return false;
	}

	if (strcmp(location2, talloc_get_name(s2)) != 0) {
		printf("failure: ptrtype [\n"
		"talloc_array_ptrtype() sets the wrong name '%s' (should be '%s')\n]\n",
			talloc_get_name(s2), location2);
		return false;
	}

	s3 = talloc_array_ptrtype(top, s3, 10);location3 = __location__;

	if (talloc_get_size(s3) != (sizeof(struct struct1 *) * 10)) {
		printf("failure: ptrtype [\n"
			   "talloc_array_ptrtype() allocated the wrong size "
		       "%lu (should be %lu)\n]\n",
			   (unsigned long)talloc_get_size(s3),
		       (unsigned long)(sizeof(struct struct1 *)*10));
		return false;
	}

	torture_assert_str_equal("ptrtype", location3, talloc_get_name(s3),
		"talloc_array_ptrtype() sets the wrong name");

	s4 = talloc_array_ptrtype(top, s4, 10);location4 = __location__;

	if (talloc_get_size(s4) != (sizeof(struct struct1 **) * 10)) {
		printf("failure: TALLOC PTRTYPE [\n"
		      "talloc_array_ptrtype() allocated the wrong size "
		       "%lu (should be %lu)\n]\n",
			   (unsigned long)talloc_get_size(s4),
		       (unsigned long)(sizeof(struct struct1 **)*10));
		return false;
	}

	torture_assert_str_equal("ptrtype", location4, talloc_get_name(s4),
		"talloc_array_ptrtype() sets the wrong name");

	talloc_free(top);

	printf("success: ptrtype\n");
	return true;
}

static bool test_autofree(void)
{
	void *p;
	printf("test: autofree [\nTALLOC AUTOFREE CONTEXT\n]\n");

	p = talloc_autofree_context();
	talloc_free(p);

	p = talloc_autofree_context();
	talloc_free(p);

	printf("success: autofree\n");
	return true;
}

int main(void)
{
	bool ret = true;

	talloc_disable_null_tracking();
	talloc_enable_null_tracking();

	ret &= test_ref1();
	ret &= test_ref2();
	ret &= test_ref3();
	ret &= test_ref4();
	ret &= test_unlink1(); 
	ret &= test_misc();
	ret &= test_realloc();
	ret &= test_realloc_child(); 
	ret &= test_steal(); 
	ret &= test_move(); 
	ret &= test_unref_reparent();
	ret &= test_realloc_fn(); 
	ret &= test_type();
	ret &= test_lifeless(); 
	ret &= test_loop();
	ret &= test_free_parent_deny_child(); 
	ret &= test_talloc_ptrtype();

	if (ret) {
		ret &= test_speed();
	}
	ret &= test_autofree();

	if (!ret)
		return -1;
	return 0;
}
