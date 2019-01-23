/* Copyright (c) 2010-2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#define UTIL_PROCESS_PRIVATE
#include "orconfig.h"
#include "or.h"

#include "test.h"

#include "util_process.h"

#include "log_test_helpers.h"

#ifndef _WIN32
#define NS_MODULE util_process

static void
temp_callback(int r, void *s)
{
  (void)r;
  (void)s;
}

static void
test_util_process_set_waitpid_callback(void *ignored)
{
  (void)ignored;
  waitpid_callback_t *res1 = NULL, *res2 = NULL;
  setup_full_capture_of_logs(LOG_WARN);
  pid_t pid = (pid_t)42;

  res1 = set_waitpid_callback(pid, temp_callback, NULL);
  tt_assert(res1);

  res2 = set_waitpid_callback(pid, temp_callback, NULL);
  tt_assert(res2);
  expect_single_log_msg(
            "Replaced a waitpid monitor on pid 42. That should be "
            "impossible.\n");

 done:
  teardown_capture_of_logs();
  clear_waitpid_callback(res1);
  clear_waitpid_callback(res2);
}

static void
test_util_process_clear_waitpid_callback(void *ignored)
{
  (void)ignored;
  waitpid_callback_t *res;
  setup_capture_of_logs(LOG_WARN);
  pid_t pid = (pid_t)43;

  clear_waitpid_callback(NULL);

  res = set_waitpid_callback(pid, temp_callback, NULL);
  clear_waitpid_callback(res);
  expect_no_log_entry();

#if 0
  /* No.  This is use-after-free.  We don't _do_ that. XXXX */
  clear_waitpid_callback(res);
  expect_log_msg("Couldn't remove waitpid monitor for pid 43.\n");
#endif

 done:
  teardown_capture_of_logs();
}
#endif /* _WIN32 */

#ifndef _WIN32
#define TEST(name) { #name, test_util_process_##name, 0, NULL, NULL }
#else
#define TEST(name) { #name, NULL, TT_SKIP, NULL, NULL }
#endif

struct testcase_t util_process_tests[] = {
  TEST(set_waitpid_callback),
  TEST(clear_waitpid_callback),
  END_OF_TESTCASES
};

