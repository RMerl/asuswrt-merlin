/* Copyright (c) 2012-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#include "orconfig.h"
#include <stdio.h>
#include <stdlib.h>

#include "or.h"
#include "util.h"
#include "backtrace.h"
#include "torlog.h"

/* -1: no crash.
 *  0: crash with a segmentation fault.
 *  1x: crash with an assertion failure. */
static int crashtype = 0;

#ifdef __GNUC__
#define NOINLINE __attribute__((noinline))
#define NORETURN __attribute__((noreturn))
#endif

int crash(int x) NOINLINE;
int oh_what(int x) NOINLINE;
int a_tangled_web(int x) NOINLINE;
int we_weave(int x) NOINLINE;
static void abort_handler(int s) NORETURN;

int
crash(int x)
{
  if (crashtype == 0) {
    *(volatile int *)0 = 0;
  } else if (crashtype == 1) {
    tor_assert(1 == 0);
  } else if (crashtype == -1) {
    ;
  }

  crashtype *= x;
  return crashtype;
}

int
oh_what(int x)
{
  /* We call crash() twice here, so that the compiler won't try to do a
   * tail-call optimization.  Only the first call will actually happen, but
   * telling the compiler to maybe do the second call will prevent it from
   * replacing the first call with a jump. */
  return crash(x) + crash(x*2);
}

int
a_tangled_web(int x)
{
  return oh_what(x) * 99 + oh_what(x);
}

int
we_weave(int x)
{
  return a_tangled_web(x) + a_tangled_web(x+1);
}

static void
abort_handler(int s)
{
  (void)s;
  exit(0);
}

int
main(int argc, char **argv)
{
  log_severity_list_t severity;

  if (argc < 2) {
    puts("I take an argument. It should be \"assert\" or \"crash\" or "
         "\"none\"");
    return 1;
  }
  if (!strcmp(argv[1], "assert")) {
    crashtype = 1;
  } else if (!strcmp(argv[1], "crash")) {
    crashtype = 0;
  } else if (!strcmp(argv[1], "none")) {
    crashtype = -1;
  } else {
    puts("Argument should be \"assert\" or \"crash\" or \"none\"");
    return 1;
  }

  init_logging();
  set_log_severity_config(LOG_WARN, LOG_ERR, &severity);
  add_stream_log(&severity, "stdout", STDOUT_FILENO);
  tor_log_update_sigsafe_err_fds();

  configure_backtrace_handler(NULL);

  signal(SIGABRT, abort_handler);

  printf("%d\n", we_weave(2));

  clean_up_backtrace_handler();

  return 0;
}

