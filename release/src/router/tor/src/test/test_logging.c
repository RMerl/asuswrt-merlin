/* Copyright (c) 2013-2015, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#include "orconfig.h"
#include "or.h"
#include "torlog.h"
#include "test.h"

static void
dummy_cb_fn(int severity, uint32_t domain, const char *msg)
{
  (void)severity; (void)domain; (void)msg;
}

static void
test_get_sigsafe_err_fds(void *arg)
{
  const int *fds;
  int n;
  log_severity_list_t include_bug, no_bug, no_bug2;
  (void) arg;
  init_logging(1);

  n = tor_log_get_sigsafe_err_fds(&fds);
  tt_int_op(n, OP_EQ, 1);
  tt_int_op(fds[0], OP_EQ, STDERR_FILENO);

  set_log_severity_config(LOG_WARN, LOG_ERR, &include_bug);
  set_log_severity_config(LOG_WARN, LOG_ERR, &no_bug);
  no_bug.masks[0] &= ~(LD_BUG|LD_GENERAL);
  set_log_severity_config(LOG_INFO, LOG_NOTICE, &no_bug2);

  /* Add some logs; make sure the output is as expected. */
  mark_logs_temp();
  add_stream_log(&include_bug, "dummy-1", 3);
  add_stream_log(&no_bug, "dummy-2", 4);
  add_stream_log(&no_bug2, "dummy-3", 5);
  add_callback_log(&include_bug, dummy_cb_fn);
  close_temp_logs();
  tor_log_update_sigsafe_err_fds();

  n = tor_log_get_sigsafe_err_fds(&fds);
  tt_int_op(n, OP_EQ, 2);
  tt_int_op(fds[0], OP_EQ, STDERR_FILENO);
  tt_int_op(fds[1], OP_EQ, 3);

  /* Allow STDOUT to replace STDERR. */
  add_stream_log(&include_bug, "dummy-4", STDOUT_FILENO);
  tor_log_update_sigsafe_err_fds();
  n = tor_log_get_sigsafe_err_fds(&fds);
  tt_int_op(n, OP_EQ, 2);
  tt_int_op(fds[0], OP_EQ, 3);
  tt_int_op(fds[1], OP_EQ, STDOUT_FILENO);

  /* But don't allow it to replace explicit STDERR. */
  add_stream_log(&include_bug, "dummy-5", STDERR_FILENO);
  tor_log_update_sigsafe_err_fds();
  n = tor_log_get_sigsafe_err_fds(&fds);
  tt_int_op(n, OP_EQ, 3);
  tt_int_op(fds[0], OP_EQ, STDERR_FILENO);
  tt_int_op(fds[1], OP_EQ, STDOUT_FILENO);
  tt_int_op(fds[2], OP_EQ, 3);

  /* Don't overflow the array. */
  {
    int i;
    for (i=5; i<20; ++i) {
      add_stream_log(&include_bug, "x-dummy", i);
    }
  }
  tor_log_update_sigsafe_err_fds();
  n = tor_log_get_sigsafe_err_fds(&fds);
  tt_int_op(n, OP_EQ, 8);

 done:
  ;
}

static void
test_sigsafe_err(void *arg)
{
  const char *fn=get_fname("sigsafe_err_log");
  char *content=NULL;
  log_severity_list_t include_bug;
  smartlist_t *lines = smartlist_new();
  (void)arg;

  set_log_severity_config(LOG_WARN, LOG_ERR, &include_bug);

  init_logging(1);
  mark_logs_temp();
  add_file_log(&include_bug, fn, 0);
  tor_log_update_sigsafe_err_fds();
  close_temp_logs();

  close(STDERR_FILENO);
  log_err(LD_BUG, "Say, this isn't too cool.");
  tor_log_err_sigsafe("Minimal.\n", NULL);

  set_log_time_granularity(100*1000);
  tor_log_err_sigsafe("Testing any ",
                      "attempt to manually log ",
                      "from a signal.\n",
                      NULL);
  mark_logs_temp();
  close_temp_logs();
  close(STDERR_FILENO);
  content = read_file_to_str(fn, 0, NULL);

  tt_assert(content != NULL);
  tor_split_lines(lines, content, (int)strlen(content));
  tt_int_op(smartlist_len(lines), OP_GE, 5);

  if (strstr(smartlist_get(lines, 0), "opening new log file"))
    smartlist_del_keeporder(lines, 0);
  tt_assert(strstr(smartlist_get(lines, 0), "Say, this isn't too cool"));
  /* Next line is blank. */
  tt_assert(!strcmpstart(smartlist_get(lines, 1), "=============="));
  tt_assert(!strcmpstart(smartlist_get(lines, 2), "Minimal."));
  /* Next line is blank. */
  tt_assert(!strcmpstart(smartlist_get(lines, 3), "=============="));
  tt_str_op(smartlist_get(lines, 4), OP_EQ,
            "Testing any attempt to manually log from a signal.");

 done:
  tor_free(content);
  smartlist_free(lines);
}

struct testcase_t logging_tests[] = {
  { "sigsafe_err_fds", test_get_sigsafe_err_fds, TT_FORK, NULL, NULL },
  { "sigsafe_err", test_sigsafe_err, TT_FORK, NULL, NULL },
  END_OF_TESTCASES
};

