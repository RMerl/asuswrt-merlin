/* Copyright (c) 2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#include "orconfig.h"
#include "test.h"

#include "util.h"
#include "handles.h"

typedef struct demo_t {
  HANDLE_ENTRY(demo, demo_t);
  int val;
} demo_t;

HANDLE_DECL(demo, demo_t, static)
HANDLE_IMPL(demo, demo_t, static)

static demo_t *
demo_new(int val)
{
  demo_t *d = tor_malloc_zero(sizeof(demo_t));
  d->val = val;
  return d;
}

static void
demo_free(demo_t *d)
{
  if (d == NULL)
    return;
  demo_handles_clear(d);
  tor_free(d);
}

static void
test_handle_basic(void *arg)
{
  (void) arg;
  demo_t *d1 = NULL, *d2 = NULL;
  demo_handle_t *wr1 = NULL, *wr2 = NULL, *wr3 = NULL, *wr4 = NULL;

  d1 = demo_new(9000);
  d2 = demo_new(9009);

  wr1 = demo_handle_new(d1);
  wr2 = demo_handle_new(d1);
  wr3 = demo_handle_new(d1);
  wr4 = demo_handle_new(d2);

  tt_assert(wr1);
  tt_assert(wr2);
  tt_assert(wr3);
  tt_assert(wr4);

  tt_ptr_op(demo_handle_get(wr1), OP_EQ, d1);
  tt_ptr_op(demo_handle_get(wr2), OP_EQ, d1);
  tt_ptr_op(demo_handle_get(wr3), OP_EQ, d1);
  tt_ptr_op(demo_handle_get(wr4), OP_EQ, d2);

  demo_handle_free(wr1);
  wr1 = NULL;
  tt_ptr_op(demo_handle_get(wr2), OP_EQ, d1);
  tt_ptr_op(demo_handle_get(wr3), OP_EQ, d1);
  tt_ptr_op(demo_handle_get(wr4), OP_EQ, d2);

  demo_free(d1);
  d1 = NULL;
  tt_ptr_op(demo_handle_get(wr2), OP_EQ, NULL);
  tt_ptr_op(demo_handle_get(wr3), OP_EQ, NULL);
  tt_ptr_op(demo_handle_get(wr4), OP_EQ, d2);

  demo_handle_free(wr2);
  wr2 = NULL;
  tt_ptr_op(demo_handle_get(wr3), OP_EQ, NULL);
  tt_ptr_op(demo_handle_get(wr4), OP_EQ, d2);

  demo_handle_free(wr3);
  wr3 = NULL;
 done:
  demo_handle_free(wr1);
  demo_handle_free(wr2);
  demo_handle_free(wr3);
  demo_handle_free(wr4);
  demo_free(d1);
  demo_free(d2);
}

#define HANDLE_TEST(name, flags)                       \
  { #name, test_handle_ ##name, (flags), NULL, NULL }

struct testcase_t handle_tests[] = {
  HANDLE_TEST(basic, 0),
  END_OF_TESTCASES
};

