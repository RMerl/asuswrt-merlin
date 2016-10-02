/* Copyright (c) 2014-2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#include "or.h"

#ifndef TOR_LOG_TEST_HELPERS_H
#define TOR_LOG_TEST_HELPERS_H

typedef struct mock_saved_log_entry_t {
  int severity;
  const char *funcname;
  const char *suffix;
  const char *format;
  char *generated_msg;
  struct mock_saved_log_entry_t *next;
} mock_saved_log_entry_t;

void mock_saving_logv(int severity, log_domain_mask_t domain,
                      const char *funcname, const char *suffix,
                      const char *format, va_list ap)
  CHECK_PRINTF(5, 0);
void mock_clean_saved_logs(void);
const smartlist_t *mock_saved_logs(void);
int setup_capture_of_logs(int new_level);
void teardown_capture_of_logs(int prev);

int mock_saved_log_has_message(const char *msg);
int mock_saved_log_has_severity(int severity);
int mock_saved_log_has_entry(void);

#define expect_log_msg(str) \
  tt_assert_msg(mock_saved_log_has_message(str), \
                "expected log to contain " # str);

#define expect_no_log_msg(str) \
  tt_assert_msg(!mock_saved_log_has_message(str), \
                "expected log to not contain " # str);

#define expect_log_severity(severity) \
  tt_assert_msg(mock_saved_log_has_severity(severity), \
                "expected log to contain severity " # severity);

#define expect_no_log_severity(severity) \
  tt_assert_msg(!mock_saved_log_has_severity(severity), \
                "expected log to not contain severity " # severity);

#define expect_log_entry() \
  tt_assert_msg(mock_saved_log_has_entry(), \
                "expected log to contain entries");

#define expect_no_log_entry() \
  tt_assert_msg(!mock_saved_log_has_entry(), \
                "expected log to not contain entries");

#endif

