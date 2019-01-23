/* Copyright (c) 2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file test_pubsub.c
 * \brief Unit tests for publish-subscribe abstraction.
 **/

#include "or.h"
#include "test.h"
#include "pubsub.h"

DECLARE_PUBSUB_STRUCT_TYPES(foobar)
DECLARE_PUBSUB_TOPIC(foobar)
DECLARE_NOTIFY_PUBSUB_TOPIC(static, foobar)
IMPLEMENT_PUBSUB_TOPIC(static, foobar)

struct foobar_event_data_t {
  unsigned u;
  const char *s;
};

struct foobar_subscriber_data_t {
  const char *name;
  long l;
};

static int
foobar_sub1(foobar_event_data_t *ev, foobar_subscriber_data_t *mine)
{
  ev->u += 10;
  mine->l += 100;
  return 0;
}

static int
foobar_sub2(foobar_event_data_t *ev, foobar_subscriber_data_t *mine)
{
  ev->u += 5;
  mine->l += 50;
  return 0;
}

static void
test_pubsub_basic(void *arg)
{
  (void)arg;
  foobar_subscriber_data_t subdata1 = { "hi", 0 };
  foobar_subscriber_data_t subdata2 = { "wow", 0 };
  const foobar_subscriber_t *sub1;
  const foobar_subscriber_t *sub2;
  foobar_event_data_t ed = { 0, "x" };
  foobar_event_data_t ed2 = { 0, "y" };
  sub1 = foobar_subscribe(foobar_sub1, &subdata1, SUBSCRIBE_ATSTART, 100);
  tt_assert(sub1);

  foobar_notify(&ed, 0);
  tt_int_op(subdata1.l, OP_EQ, 100);
  tt_int_op(subdata2.l, OP_EQ, 0);
  tt_int_op(ed.u, OP_EQ, 10);

  sub2 = foobar_subscribe(foobar_sub2, &subdata2, 0, 5);
  tt_assert(sub2);

  foobar_notify(&ed2, 0);
  tt_int_op(subdata1.l, OP_EQ, 200);
  tt_int_op(subdata2.l, OP_EQ, 50);
  tt_int_op(ed2.u, OP_EQ, 15);

  foobar_unsubscribe(sub1);

  foobar_notify(&ed, 0);
  tt_int_op(subdata1.l, OP_EQ, 200);
  tt_int_op(subdata2.l, OP_EQ, 100);
  tt_int_op(ed.u, OP_EQ, 15);

 done:
  foobar_clear();
}

struct testcase_t pubsub_tests[] = {
  { "pubsub_basic", test_pubsub_basic, TT_FORK, NULL, NULL },
  END_OF_TESTCASES
};

