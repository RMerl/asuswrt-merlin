/* Copyright (c) 2010-2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#define COMPAT_LIBEVENT_PRIVATE
#include "orconfig.h"
#include "or.h"

#include "test.h"

#include "compat_libevent.h"

#ifdef HAVE_EVENT2_EVENT_H
#include <event2/event.h>
#include <event2/thread.h>
#ifdef USE_BUFFEREVENTS
#include <event2/bufferevent.h>
#endif
#else
#include <event.h>
#endif

#include "log_test_helpers.h"

#define NS_MODULE compat_libevent

static void
test_compat_libevent_logging_callback(void *ignored)
{
  (void)ignored;
  int previous_log = setup_capture_of_logs(LOG_DEBUG);

  libevent_logging_callback(_EVENT_LOG_DEBUG, "hello world");
  expect_log_msg("Message from libevent: hello world\n");
  expect_log_severity(LOG_DEBUG);

  mock_clean_saved_logs();
  libevent_logging_callback(_EVENT_LOG_MSG, "hello world another time");
  expect_log_msg("Message from libevent: hello world another time\n");
  expect_log_severity(LOG_INFO);

  mock_clean_saved_logs();
  libevent_logging_callback(_EVENT_LOG_WARN, "hello world a third time");
  expect_log_msg("Warning from libevent: hello world a third time\n");
  expect_log_severity(LOG_WARN);

  mock_clean_saved_logs();
  libevent_logging_callback(_EVENT_LOG_ERR, "hello world a fourth time");
  expect_log_msg("Error from libevent: hello world a fourth time\n");
  expect_log_severity(LOG_ERR);

  mock_clean_saved_logs();
  libevent_logging_callback(42, "hello world a fifth time");
  expect_log_msg("Message [42] from libevent: hello world a fifth time\n");
  expect_log_severity(LOG_WARN);

  mock_clean_saved_logs();
  libevent_logging_callback(_EVENT_LOG_DEBUG,
                            "012345678901234567890123456789"
                            "012345678901234567890123456789"
                            "012345678901234567890123456789"
                            "012345678901234567890123456789"
                            "012345678901234567890123456789"
                            "012345678901234567890123456789"
                            "012345678901234567890123456789"
                            "012345678901234567890123456789"
                            "012345678901234567890123456789"
                            "012345678901234567890123456789"
                            "012345678901234567890123456789"
                            "012345678901234567890123456789"
                            );
  expect_log_msg("Message from libevent: "
                            "012345678901234567890123456789"
                            "012345678901234567890123456789"
                            "012345678901234567890123456789"
                            "012345678901234567890123456789"
                            "012345678901234567890123456789"
                            "012345678901234567890123456789"
                            "012345678901234567890123456789"
                            "012345678901234567890123456789"
                            "012345678901234567890123456789"
                            "012345678901234567890123456789"
                            "012345678901234567890123456789"
            "012345678901234567890123456789\n");
  expect_log_severity(LOG_DEBUG);

  mock_clean_saved_logs();
  libevent_logging_callback(42, "xxx\n");
  expect_log_msg("Message [42] from libevent: xxx\n");
  expect_log_severity(LOG_WARN);

  suppress_libevent_log_msg("something");
  mock_clean_saved_logs();
  libevent_logging_callback(_EVENT_LOG_MSG, "hello there");
  expect_log_msg("Message from libevent: hello there\n");
  expect_log_severity(LOG_INFO);

  mock_clean_saved_logs();
  libevent_logging_callback(_EVENT_LOG_MSG, "hello there something else");
  expect_no_log_msg("hello there something else");

  // No way of verifying the result of this, it seems =/
  configure_libevent_logging();

 done:
  suppress_libevent_log_msg(NULL);
  teardown_capture_of_logs(previous_log);
}

static void
test_compat_libevent_le_versions_compatibility(void *ignored)
{
  (void)ignored;
  int res;

  res = le_versions_compatibility(LE_OTHER);
  tt_int_op(res, OP_EQ, 0);

  res = le_versions_compatibility(V_OLD(0,9,'c'));
  tt_int_op(res, OP_EQ, 1);

  res = le_versions_compatibility(V(1,3,98));
  tt_int_op(res, OP_EQ, 2);

  res = le_versions_compatibility(V(1,4,98));
  tt_int_op(res, OP_EQ, 3);

  res = le_versions_compatibility(V(1,5,0));
  tt_int_op(res, OP_EQ, 4);

  res = le_versions_compatibility(V(2,0,0));
  tt_int_op(res, OP_EQ, 4);

  res = le_versions_compatibility(V(2,0,2));
  tt_int_op(res, OP_EQ, 5);

 done:
  (void)0;
}

static void
test_compat_libevent_tor_decode_libevent_version(void *ignored)
{
  (void)ignored;
  le_version_t res;

  res = tor_decode_libevent_version("SOMETHING WRONG");
  tt_int_op(res, OP_EQ, LE_OTHER);

  res = tor_decode_libevent_version("1.4.11");
  tt_int_op(res, OP_EQ, V(1,4,11));

  res = tor_decode_libevent_version("1.4.12b-stable");
  tt_int_op(res, OP_EQ, V(1,4,12));

  res = tor_decode_libevent_version("1.4.17b_stable");
  tt_int_op(res, OP_EQ, V(1,4,17));

  res = tor_decode_libevent_version("1.4.12!stable");
  tt_int_op(res, OP_EQ, LE_OTHER);

  res = tor_decode_libevent_version("1.4.12b!stable");
  tt_int_op(res, OP_EQ, LE_OTHER);

  res = tor_decode_libevent_version("1.4.13-");
  tt_int_op(res, OP_EQ, V(1,4,13));

  res = tor_decode_libevent_version("1.4.14_");
  tt_int_op(res, OP_EQ, V(1,4,14));

  res = tor_decode_libevent_version("1.4.15c-");
  tt_int_op(res, OP_EQ, V(1,4,15));

  res = tor_decode_libevent_version("1.4.16c_");
  tt_int_op(res, OP_EQ, V(1,4,16));

  res = tor_decode_libevent_version("1.4.17-s");
  tt_int_op(res, OP_EQ, V(1,4,17));

  res = tor_decode_libevent_version("1.5");
  tt_int_op(res, OP_EQ, V(1,5,0));

  res = tor_decode_libevent_version("1.2");
  tt_int_op(res, OP_EQ, V(1,2,0));

  res = tor_decode_libevent_version("1.2-");
  tt_int_op(res, OP_EQ, LE_OTHER);

  res = tor_decode_libevent_version("1.6e");
  tt_int_op(res, OP_EQ, V_OLD(1,6,'e'));

 done:
  (void)0;
}

#if defined(LIBEVENT_VERSION)
#define HEADER_VERSION LIBEVENT_VERSION
#elif defined(_EVENT_VERSION)
#define HEADER_VERSION _EVENT_VERSION
#endif

static void
test_compat_libevent_header_version(void *ignored)
{
  (void)ignored;
  const char *res;

  res = tor_libevent_get_header_version_str();
  tt_str_op(res, OP_EQ, HEADER_VERSION);

 done:
  (void)0;
}

struct testcase_t compat_libevent_tests[] = {
  { "logging_callback", test_compat_libevent_logging_callback,
    TT_FORK, NULL, NULL },
  { "le_versions_compatibility",
    test_compat_libevent_le_versions_compatibility, 0, NULL, NULL },
  { "tor_decode_libevent_version",
    test_compat_libevent_tor_decode_libevent_version, 0, NULL, NULL },
  { "header_version", test_compat_libevent_header_version, 0, NULL, NULL },
  END_OF_TESTCASES
};

