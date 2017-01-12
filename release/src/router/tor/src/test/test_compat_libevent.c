/* Copyright (c) 2010-2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#define COMPAT_LIBEVENT_PRIVATE
#include "orconfig.h"
#include "or.h"

#include "test.h"

#include "compat_libevent.h"

#include <event2/event.h>
#include <event2/thread.h>

#include "log_test_helpers.h"

#define NS_MODULE compat_libevent

static void
test_compat_libevent_logging_callback(void *ignored)
{
  (void)ignored;
  setup_full_capture_of_logs(LOG_DEBUG);

  libevent_logging_callback(_EVENT_LOG_DEBUG, "hello world");
  expect_log_msg("Message from libevent: hello world\n");
  expect_log_severity(LOG_DEBUG);
  tt_int_op(smartlist_len(mock_saved_logs()), OP_EQ, 1);

  mock_clean_saved_logs();
  libevent_logging_callback(_EVENT_LOG_MSG, "hello world another time");
  expect_log_msg("Message from libevent: hello world another time\n");
  expect_log_severity(LOG_INFO);
  tt_int_op(smartlist_len(mock_saved_logs()), OP_EQ, 1);

  mock_clean_saved_logs();
  libevent_logging_callback(_EVENT_LOG_WARN, "hello world a third time");
  expect_log_msg("Warning from libevent: hello world a third time\n");
  expect_log_severity(LOG_WARN);
  tt_int_op(smartlist_len(mock_saved_logs()), OP_EQ, 1);

  mock_clean_saved_logs();
  libevent_logging_callback(_EVENT_LOG_ERR, "hello world a fourth time");
  expect_log_msg("Error from libevent: hello world a fourth time\n");
  expect_log_severity(LOG_ERR);
  tt_int_op(smartlist_len(mock_saved_logs()), OP_EQ, 1);

  mock_clean_saved_logs();
  libevent_logging_callback(42, "hello world a fifth time");
  expect_log_msg("Message [42] from libevent: hello world a fifth time\n");
  expect_log_severity(LOG_WARN);
  tt_int_op(smartlist_len(mock_saved_logs()), OP_EQ, 1);

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
  tt_int_op(smartlist_len(mock_saved_logs()), OP_EQ, 1);

  mock_clean_saved_logs();
  libevent_logging_callback(42, "xxx\n");
  expect_log_msg("Message [42] from libevent: xxx\n");
  expect_log_severity(LOG_WARN);
  tt_int_op(smartlist_len(mock_saved_logs()), OP_EQ, 1);

  suppress_libevent_log_msg("something");
  mock_clean_saved_logs();
  libevent_logging_callback(_EVENT_LOG_MSG, "hello there");
  expect_log_msg("Message from libevent: hello there\n");
  expect_log_severity(LOG_INFO);
  tt_int_op(smartlist_len(mock_saved_logs()), OP_EQ, 1);

  mock_clean_saved_logs();
  libevent_logging_callback(_EVENT_LOG_MSG, "hello there something else");
  expect_no_log_msg("hello there something else");
  if (mock_saved_logs())
    tt_int_op(smartlist_len(mock_saved_logs()), OP_EQ, 0);

  // No way of verifying the result of this, it seems =/
  configure_libevent_logging();

 done:
  suppress_libevent_log_msg(NULL);
  teardown_capture_of_logs();
}

static void
test_compat_libevent_header_version(void *ignored)
{
  (void)ignored;
  const char *res;

  res = tor_libevent_get_header_version_str();
  tt_str_op(res, OP_EQ, LIBEVENT_VERSION);

 done:
  (void)0;
}

struct testcase_t compat_libevent_tests[] = {
  { "logging_callback", test_compat_libevent_logging_callback,
    TT_FORK, NULL, NULL },
  { "header_version", test_compat_libevent_header_version, 0, NULL, NULL },
  END_OF_TESTCASES
};

