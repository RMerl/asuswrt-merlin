/* Copyright (c) 2003, Roger Dingledine
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file util_bug.c
 **/

#include "orconfig.h"
#include "util_bug.h"
#include "torlog.h"
#include "backtrace.h"
#include "container.h"

#ifdef TOR_UNIT_TESTS
static void (*failed_assertion_cb)(void) = NULL;
static int n_bugs_to_capture = 0;
static smartlist_t *bug_messages = NULL;
#define capturing_bugs() (bug_messages != NULL && n_bugs_to_capture)
void
tor_capture_bugs_(int n)
{
  tor_end_capture_bugs_();
  bug_messages = smartlist_new();
  n_bugs_to_capture = n;
}
void
tor_end_capture_bugs_(void)
{
  n_bugs_to_capture = 0;
  if (!bug_messages)
    return;
  SMARTLIST_FOREACH(bug_messages, char *, cp, tor_free(cp));
  smartlist_free(bug_messages);
  bug_messages = NULL;
}
const smartlist_t *
tor_get_captured_bug_log_(void)
{
  return bug_messages;
}
static void
add_captured_bug(const char *s)
{
  --n_bugs_to_capture;
  smartlist_add(bug_messages, tor_strdup(s));
}
/** Set a callback to be invoked when we get any tor_bug_occurred_
 * invocation. We use this in the unit tests so that a nonfatal
 * assertion failure can also count as a test failure.
 */
void
tor_set_failed_assertion_callback(void (*fn)(void))
{
  failed_assertion_cb = fn;
}
#else
#define capturing_bugs() (0)
#define add_captured_bug(s) do { } while (0)
#endif

/** Helper for tor_assert: report the assertion failure. */
void
tor_assertion_failed_(const char *fname, unsigned int line,
                      const char *func, const char *expr)
{
  char buf[256];
  log_err(LD_BUG, "%s:%u: %s: Assertion %s failed; aborting.",
          fname, line, func, expr);
  tor_snprintf(buf, sizeof(buf),
               "Assertion %s failed in %s at %s:%u",
               expr, func, fname, line);
  log_backtrace(LOG_ERR, LD_BUG, buf);
}

/** Helper for tor_assert_nonfatal: report the assertion failure. */
void
tor_bug_occurred_(const char *fname, unsigned int line,
                  const char *func, const char *expr,
                  int once)
{
  char buf[256];
  const char *once_str = once ?
    " (Future instances of this warning will be silenced.)": "";
  if (! expr) {
    if (capturing_bugs()) {
      add_captured_bug("This line should not have been reached.");
      return;
    }
    log_warn(LD_BUG, "%s:%u: %s: This line should not have been reached.%s",
             fname, line, func, once_str);
    tor_snprintf(buf, sizeof(buf),
                 "Line unexpectedly reached at %s at %s:%u",
                 func, fname, line);
  } else {
    if (capturing_bugs()) {
      add_captured_bug(expr);
      return;
    }
    log_warn(LD_BUG, "%s:%u: %s: Non-fatal assertion %s failed.%s",
             fname, line, func, expr, once_str);
    tor_snprintf(buf, sizeof(buf),
                 "Non-fatal assertion %s failed in %s at %s:%u",
                 expr, func, fname, line);
  }
  log_backtrace(LOG_WARN, LD_BUG, buf);

#ifdef TOR_UNIT_TESTS
  if (failed_assertion_cb) {
    failed_assertion_cb();
  }
#endif
}

