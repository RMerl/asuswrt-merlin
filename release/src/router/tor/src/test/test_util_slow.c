/* Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#include "orconfig.h"
#define UTIL_PRIVATE
#include "util.h"
#include "util_process.h"
#include "crypto.h"
#include "torlog.h"
#include "test.h"

#ifndef BUILDDIR
#define BUILDDIR "."
#endif

#ifdef _WIN32
#define notify_pending_waitpid_callbacks() STMT_NIL
#define TEST_CHILD "test-child.exe"
#define EOL "\r\n"
#else
#define TEST_CHILD (BUILDDIR "/src/test/test-child")
#define EOL "\n"
#endif

#ifdef _WIN32
/* I've assumed Windows doesn't have the gap between fork and exec
 * that causes the race condition on unix-like platforms */
#define MATCH_PROCESS_STATUS(s1,s2)         ((s1) == (s2))

#else
/* work around a race condition of the timing of SIGCHLD handler updates
 * to the process_handle's fields, and checks of those fields
 *
 * TODO: Once we can signal failure to exec, change PROCESS_STATUS_RUNNING to
 * PROCESS_STATUS_ERROR (and similarly with *_OR_NOTRUNNING) */
#define PROCESS_STATUS_RUNNING_OR_NOTRUNNING  (PROCESS_STATUS_RUNNING+1)
#define IS_RUNNING_OR_NOTRUNNING(s)           \
  ((s) == PROCESS_STATUS_RUNNING || (s) == PROCESS_STATUS_NOTRUNNING)
/* well, this is ugly */
#define MATCH_PROCESS_STATUS(s1,s2)           \
  (  (s1) == (s2)                                         \
     ||((s1) == PROCESS_STATUS_RUNNING_OR_NOTRUNNING      \
        && IS_RUNNING_OR_NOTRUNNING(s2))                  \
     ||((s2) == PROCESS_STATUS_RUNNING_OR_NOTRUNNING      \
        && IS_RUNNING_OR_NOTRUNNING(s1)))

#endif // _WIN32

/** Helper function for testing tor_spawn_background */
static void
run_util_spawn_background(const char *argv[], const char *expected_out,
                          const char *expected_err, int expected_exit,
                          int expected_status)
{
  int retval, exit_code;
  ssize_t pos;
  process_handle_t *process_handle=NULL;
  char stdout_buf[100], stderr_buf[100];
  int status;

  /* Start the program */
#ifdef _WIN32
  status = tor_spawn_background(NULL, argv, NULL, &process_handle);
#else
  status = tor_spawn_background(argv[0], argv, NULL, &process_handle);
#endif

  notify_pending_waitpid_callbacks();

  /* the race condition doesn't affect status,
   * because status isn't updated by the SIGCHLD handler,
   * but we still need to handle PROCESS_STATUS_RUNNING_OR_NOTRUNNING */
  tt_assert(MATCH_PROCESS_STATUS(expected_status, status));
  if (status == PROCESS_STATUS_ERROR) {
    tt_ptr_op(process_handle, OP_EQ, NULL);
    return;
  }

  tt_assert(process_handle != NULL);

  /* When a spawned process forks, fails, then exits very quickly,
   * (this typically occurs when exec fails)
   * there is a race condition between the SIGCHLD handler
   * updating the process_handle's fields, and this test
   * checking the process status in those fields.
   * The SIGCHLD update can occur before or after the code below executes.
   * This causes intermittent failures in spawn_background_fail(),
   * typically when the machine is under load.
   * We use PROCESS_STATUS_RUNNING_OR_NOTRUNNING to avoid this issue. */

  /* the race condition affects the change in
   * process_handle->status from RUNNING to NOTRUNNING */
  tt_assert(MATCH_PROCESS_STATUS(expected_status, process_handle->status));

#ifndef _WIN32
  notify_pending_waitpid_callbacks();
  /* the race condition affects the change in
   * process_handle->waitpid_cb to NULL,
   * so we skip the check if expected_status is ambiguous,
   * that is, PROCESS_STATUS_RUNNING_OR_NOTRUNNING */
  tt_assert(process_handle->waitpid_cb != NULL
              || expected_status == PROCESS_STATUS_RUNNING_OR_NOTRUNNING);
#endif

#ifdef _WIN32
  tt_assert(process_handle->stdout_pipe != INVALID_HANDLE_VALUE);
  tt_assert(process_handle->stderr_pipe != INVALID_HANDLE_VALUE);
  tt_assert(process_handle->stdin_pipe != INVALID_HANDLE_VALUE);
#else
  tt_assert(process_handle->stdout_pipe >= 0);
  tt_assert(process_handle->stderr_pipe >= 0);
  tt_assert(process_handle->stdin_pipe >= 0);
#endif

  /* Check stdout */
  pos = tor_read_all_from_process_stdout(process_handle, stdout_buf,
                                         sizeof(stdout_buf) - 1);
  tt_assert(pos >= 0);
  stdout_buf[pos] = '\0';
  tt_int_op(strlen(expected_out),OP_EQ, pos);
  tt_str_op(expected_out,OP_EQ, stdout_buf);

  notify_pending_waitpid_callbacks();

  /* Check it terminated correctly */
  retval = tor_get_exit_code(process_handle, 1, &exit_code);
  tt_int_op(PROCESS_EXIT_EXITED,OP_EQ, retval);
  tt_int_op(expected_exit,OP_EQ, exit_code);
  // TODO: Make test-child exit with something other than 0

#ifndef _WIN32
  notify_pending_waitpid_callbacks();
  tt_ptr_op(process_handle->waitpid_cb, OP_EQ, NULL);
#endif

  /* Check stderr */
  pos = tor_read_all_from_process_stderr(process_handle, stderr_buf,
                                         sizeof(stderr_buf) - 1);
  tt_assert(pos >= 0);
  stderr_buf[pos] = '\0';
  tt_str_op(expected_err,OP_EQ, stderr_buf);
  tt_int_op(strlen(expected_err),OP_EQ, pos);

  notify_pending_waitpid_callbacks();

 done:
  if (process_handle)
    tor_process_handle_destroy(process_handle, 1);
}

/** Check that we can launch a process and read the output */
static void
test_util_spawn_background_ok(void *ptr)
{
  const char *argv[] = {TEST_CHILD, "--test", NULL};
  const char *expected_out = "OUT"EOL "--test"EOL "SLEEPING"EOL "DONE" EOL;
  const char *expected_err = "ERR"EOL;

  (void)ptr;

  run_util_spawn_background(argv, expected_out, expected_err, 0,
      PROCESS_STATUS_RUNNING);
}

/** Check that failing to find the executable works as expected */
static void
test_util_spawn_background_fail(void *ptr)
{
  const char *argv[] = {BUILDDIR "/src/test/no-such-file", "--test", NULL};
  const char *expected_err = "";
  char expected_out[1024];
  char code[32];
#ifdef _WIN32
  const int expected_status = PROCESS_STATUS_ERROR;
#else
  /* TODO: Once we can signal failure to exec, set this to be
   * PROCESS_STATUS_RUNNING_OR_ERROR */
  const int expected_status = PROCESS_STATUS_RUNNING_OR_NOTRUNNING;
#endif

  memset(expected_out, 0xf0, sizeof(expected_out));
  memset(code, 0xf0, sizeof(code));

  (void)ptr;

  tor_snprintf(code, sizeof(code), "%x/%x",
    9 /* CHILD_STATE_FAILEXEC */ , ENOENT);
  tor_snprintf(expected_out, sizeof(expected_out),
    "ERR: Failed to spawn background process - code %s\n", code);

  run_util_spawn_background(argv, expected_out, expected_err, 255,
      expected_status);
}

/** Test that reading from a handle returns a partial read rather than
 * blocking */
static void
test_util_spawn_background_partial_read_impl(int exit_early)
{
  const int expected_exit = 0;
  const int expected_status = PROCESS_STATUS_RUNNING;

  int retval, exit_code;
  ssize_t pos = -1;
  process_handle_t *process_handle=NULL;
  int status;
  char stdout_buf[100], stderr_buf[100];

  const char *argv[] = {TEST_CHILD, "--test", NULL};
  const char *expected_out[] = { "OUT" EOL "--test" EOL "SLEEPING" EOL,
                                 "DONE" EOL,
                                 NULL };
  const char *expected_err = "ERR" EOL;

#ifndef _WIN32
  int eof = 0;
#endif
  int expected_out_ctr;

  if (exit_early) {
    argv[1] = "--hang";
    expected_out[0] = "OUT"EOL "--hang"EOL "SLEEPING" EOL;
  }

  /* Start the program */
#ifdef _WIN32
  status = tor_spawn_background(NULL, argv, NULL, &process_handle);
#else
  status = tor_spawn_background(argv[0], argv, NULL, &process_handle);
#endif
  tt_int_op(expected_status,OP_EQ, status);
  tt_assert(process_handle);
  tt_int_op(expected_status,OP_EQ, process_handle->status);

  /* Check stdout */
  for (expected_out_ctr = 0; expected_out[expected_out_ctr] != NULL;) {
#ifdef _WIN32
    pos = tor_read_all_handle(process_handle->stdout_pipe, stdout_buf,
                              sizeof(stdout_buf) - 1, NULL);
#else
    /* Check that we didn't read the end of file last time */
    tt_assert(!eof);
    pos = tor_read_all_handle(process_handle->stdout_handle, stdout_buf,
                              sizeof(stdout_buf) - 1, NULL, &eof);
#endif
    log_info(LD_GENERAL, "tor_read_all_handle() returned %d", (int)pos);

    /* We would have blocked, keep on trying */
    if (0 == pos)
      continue;

    tt_assert(pos > 0);
    stdout_buf[pos] = '\0';
    tt_str_op(expected_out[expected_out_ctr],OP_EQ, stdout_buf);
    tt_int_op(strlen(expected_out[expected_out_ctr]),OP_EQ, pos);
    expected_out_ctr++;
  }

  if (exit_early) {
    tor_process_handle_destroy(process_handle, 1);
    process_handle = NULL;
    goto done;
  }

  /* The process should have exited without writing more */
#ifdef _WIN32
  pos = tor_read_all_handle(process_handle->stdout_pipe, stdout_buf,
                            sizeof(stdout_buf) - 1,
                            process_handle);
  tt_int_op(0,OP_EQ, pos);
#else
  if (!eof) {
    /* We should have got all the data, but maybe not the EOF flag */
    pos = tor_read_all_handle(process_handle->stdout_handle, stdout_buf,
                              sizeof(stdout_buf) - 1,
                              process_handle, &eof);
    tt_int_op(0,OP_EQ, pos);
    tt_assert(eof);
  }
  /* Otherwise, we got the EOF on the last read */
#endif

  /* Check it terminated correctly */
  retval = tor_get_exit_code(process_handle, 1, &exit_code);
  tt_int_op(PROCESS_EXIT_EXITED,OP_EQ, retval);
  tt_int_op(expected_exit,OP_EQ, exit_code);

  // TODO: Make test-child exit with something other than 0

  /* Check stderr */
  pos = tor_read_all_from_process_stderr(process_handle, stderr_buf,
                                         sizeof(stderr_buf) - 1);
  tt_assert(pos >= 0);
  stderr_buf[pos] = '\0';
  tt_str_op(expected_err,OP_EQ, stderr_buf);
  tt_int_op(strlen(expected_err),OP_EQ, pos);

 done:
  tor_process_handle_destroy(process_handle, 1);
}

static void
test_util_spawn_background_partial_read(void *arg)
{
  (void)arg;
  test_util_spawn_background_partial_read_impl(0);
}

static void
test_util_spawn_background_exit_early(void *arg)
{
  (void)arg;
  test_util_spawn_background_partial_read_impl(1);
}

static void
test_util_spawn_background_waitpid_notify(void *arg)
{
  int retval, exit_code;
  process_handle_t *process_handle=NULL;
  int status;
  int ms_timer;

  const char *argv[] = {TEST_CHILD, "--fast", NULL};

  (void) arg;

#ifdef _WIN32
  status = tor_spawn_background(NULL, argv, NULL, &process_handle);
#else
  status = tor_spawn_background(argv[0], argv, NULL, &process_handle);
#endif

  tt_int_op(status, OP_EQ, PROCESS_STATUS_RUNNING);
  tt_ptr_op(process_handle, OP_NE, NULL);

  /* We're not going to look at the stdout/stderr output this time. Instead,
   * we're testing whether notify_pending_waitpid_calbacks() can report the
   * process exit (on unix) and/or whether tor_get_exit_code() can notice it
   * (on windows) */

#ifndef _WIN32
  ms_timer = 30*1000;
  tt_ptr_op(process_handle->waitpid_cb, OP_NE, NULL);
  while (process_handle->waitpid_cb && ms_timer > 0) {
    tor_sleep_msec(100);
    ms_timer -= 100;
    notify_pending_waitpid_callbacks();
  }
  tt_int_op(ms_timer, OP_GT, 0);
  tt_ptr_op(process_handle->waitpid_cb, OP_EQ, NULL);
#endif

  ms_timer = 30*1000;
  while (((retval = tor_get_exit_code(process_handle, 0, &exit_code))
                == PROCESS_EXIT_RUNNING) && ms_timer > 0) {
    tor_sleep_msec(100);
    ms_timer -= 100;
  }
  tt_int_op(ms_timer, OP_GT, 0);

  tt_int_op(retval, OP_EQ, PROCESS_EXIT_EXITED);

 done:
  tor_process_handle_destroy(process_handle, 1);
}

#undef TEST_CHILD
#undef EOL

#undef MATCH_PROCESS_STATUS

#ifndef _WIN32
#undef PROCESS_STATUS_RUNNING_OR_NOTRUNNING
#undef IS_RUNNING_OR_NOTRUNNING
#endif

#define UTIL_TEST(name, flags)                          \
  { #name, test_util_ ## name, flags, NULL, NULL }

struct testcase_t slow_util_tests[] = {
  UTIL_TEST(spawn_background_ok, 0),
  UTIL_TEST(spawn_background_fail, 0),
  UTIL_TEST(spawn_background_partial_read, 0),
  UTIL_TEST(spawn_background_exit_early, 0),
  UTIL_TEST(spawn_background_waitpid_notify, 0),
  END_OF_TESTCASES
};

