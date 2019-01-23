/* Copyright (c) 2007-2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file test_nodelist.c
 * \brief Unit tests for nodelist related functions.
 **/

#include "or.h"
#include "nodelist.h"
#include "test.h"

/** Test the case when node_get_by_id() returns NULL,
 * node_get_verbose_nickname_by_id should return the base 16 encoding
 * of the id.
 */
static void
test_nodelist_node_get_verbose_nickname_by_id_null_node(void *arg)
{
  char vname[MAX_VERBOSE_NICKNAME_LEN+1];
  const char ID[] = "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
                    "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA";
  (void) arg;

  /* make sure node_get_by_id returns NULL */
  tt_assert(!node_get_by_id(ID));
  node_get_verbose_nickname_by_id(ID, vname);
  tt_str_op(vname,OP_EQ, "$AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");
 done:
  return;
}

/** For routers without named flag, get_verbose_nickname should return
 * "Fingerprint~Nickname"
 */
static void
test_nodelist_node_get_verbose_nickname_not_named(void *arg)
{
  node_t mock_node;
  routerstatus_t mock_rs;

  char vname[MAX_VERBOSE_NICKNAME_LEN+1];

  (void) arg;

  memset(&mock_node, 0, sizeof(node_t));
  memset(&mock_rs, 0, sizeof(routerstatus_t));

  /* verbose nickname should use ~ instead of = for unnamed routers */
  strlcpy(mock_rs.nickname, "TestOR", sizeof(mock_rs.nickname));
  mock_node.rs = &mock_rs;
  memcpy(mock_node.identity,
          "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
          "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA",
          DIGEST_LEN);
  node_get_verbose_nickname(&mock_node, vname);
  tt_str_op(vname,OP_EQ, "$AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA~TestOR");

 done:
  return;
}

/** A node should be considered a directory server if it has an open dirport
 * of it accepts tunnelled directory requests.
 */
static void
test_nodelist_node_is_dir(void *arg)
{
  (void)arg;

  routerstatus_t rs;
  routerinfo_t ri;
  node_t node;
  memset(&node, 0, sizeof(node_t));
  memset(&rs, 0, sizeof(routerstatus_t));
  memset(&ri, 0, sizeof(routerinfo_t));

  tt_assert(!node_is_dir(&node));

  node.rs = &rs;
  tt_assert(!node_is_dir(&node));

  rs.is_v2_dir = 1;
  tt_assert(node_is_dir(&node));

  rs.is_v2_dir = 0;
  rs.dir_port = 1;
  tt_assert(! node_is_dir(&node));

  node.rs = NULL;
  tt_assert(!node_is_dir(&node));
  node.ri = &ri;
  ri.supports_tunnelled_dir_requests = 1;
  tt_assert(node_is_dir(&node));
  ri.supports_tunnelled_dir_requests = 0;
  ri.dir_port = 1;
  tt_assert(! node_is_dir(&node));

 done:
  return;
}

#define NODE(name, flags) \
  { #name, test_nodelist_##name, (flags), NULL, NULL }

struct testcase_t nodelist_tests[] = {
  NODE(node_get_verbose_nickname_by_id_null_node, TT_FORK),
  NODE(node_get_verbose_nickname_not_named, TT_FORK),
  NODE(node_is_dir, TT_FORK),
  END_OF_TESTCASES
};

