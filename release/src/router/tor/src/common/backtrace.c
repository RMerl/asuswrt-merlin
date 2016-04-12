/* Copyright (c) 2013-2015, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#define __USE_GNU
#define _GNU_SOURCE 1

#include "orconfig.h"
#include "compat.h"
#include "util.h"
#include "torlog.h"

#ifdef HAVE_EXECINFO_H
#include <execinfo.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif

#ifdef HAVE_CYGWIN_SIGNAL_H
#include <cygwin/signal.h>
#elif defined(HAVE_SYS_UCONTEXT_H)
#include <sys/ucontext.h>
#elif defined(HAVE_UCONTEXT_H)
#include <ucontext.h>
#endif

#define EXPOSE_CLEAN_BACKTRACE
#include "backtrace.h"

#if defined(HAVE_EXECINFO_H) && defined(HAVE_BACKTRACE) && \
  defined(HAVE_BACKTRACE_SYMBOLS_FD) && defined(HAVE_SIGACTION)
#define USE_BACKTRACE
#endif

#if !defined(USE_BACKTRACE)
#define NO_BACKTRACE_IMPL
#endif

/** Version of Tor to report in backtrace messages. */
static char *bt_version = NULL;

#ifdef USE_BACKTRACE
/** Largest stack depth to try to dump. */
#define MAX_DEPTH 256
/** Static allocation of stack to dump. This is static so we avoid stack
 * pressure. */
static void *cb_buf[MAX_DEPTH];
/** Protects cb_buf from concurrent access */
static tor_mutex_t cb_buf_mutex;

/** Change a stacktrace in <b>stack</b> of depth <b>depth</b> so that it will
 * log the correct function from which a signal was received with context
 * <b>ctx</b>.  (When we get a signal, the current function will not have
 * called any other function, and will therefore have not pushed its address
 * onto the stack.  Fortunately, we usually have the program counter in the
 * ucontext_t structure.
 */
void
clean_backtrace(void **stack, int depth, const ucontext_t *ctx)
{
#ifdef PC_FROM_UCONTEXT
#if defined(__linux__)
  const int n = 1;
#elif defined(__darwin__) || defined(__APPLE__) || defined(__OpenBSD__) \
  || defined(__FreeBSD__)
  const int n = 2;
#else
  const int n = 1;
#endif
  if (depth <= n)
    return;

  stack[n] = (void*) ctx->PC_FROM_UCONTEXT;
#else
  (void) depth;
  (void) ctx;
  (void) stack;
#endif
}

/** Log a message <b>msg</b> at <b>severity</b> in <b>domain</b>, and follow
 * that with a backtrace log. */
void
log_backtrace(int severity, int domain, const char *msg)
{
  int depth;
  char **symbols;
  int i;

  tor_mutex_acquire(&cb_buf_mutex);

  depth = backtrace(cb_buf, MAX_DEPTH);
  symbols = backtrace_symbols(cb_buf, depth);

  tor_log(severity, domain, "%s. Stack trace:", msg);
  if (!symbols) {
    tor_log(severity, domain, "    Unable to generate backtrace.");
    goto done;
  }
  for (i=0; i < depth; ++i) {
    tor_log(severity, domain, "    %s", symbols[i]);
  }
  free(symbols);

 done:
  tor_mutex_release(&cb_buf_mutex);
}

static void crash_handler(int sig, siginfo_t *si, void *ctx_)
  __attribute__((noreturn));

/** Signal handler: write a crash message with a stack trace, and die. */
static void
crash_handler(int sig, siginfo_t *si, void *ctx_)
{
  char buf[40];
  int depth;
  ucontext_t *ctx = (ucontext_t *) ctx_;
  int n_fds, i;
  const int *fds = NULL;

  (void) si;

  depth = backtrace(cb_buf, MAX_DEPTH);
  /* Clean up the top stack frame so we get the real function
   * name for the most recently failing function. */
  clean_backtrace(cb_buf, depth, ctx);

  format_dec_number_sigsafe((unsigned)sig, buf, sizeof(buf));

  tor_log_err_sigsafe(bt_version, " died: Caught signal ", buf, "\n",
                      NULL);

  n_fds = tor_log_get_sigsafe_err_fds(&fds);
  for (i=0; i < n_fds; ++i)
    backtrace_symbols_fd(cb_buf, depth, fds[i]);

  abort();
}

/** Install signal handlers as needed so that when we crash, we produce a
 * useful stack trace. Return 0 on success, -1 on failure. */
static int
install_bt_handler(void)
{
  int trap_signals[] = { SIGSEGV, SIGILL, SIGFPE, SIGBUS, SIGSYS,
                         SIGIO, -1 };
  int i, rv=0;

  struct sigaction sa;

  tor_mutex_init(&cb_buf_mutex);

  memset(&sa, 0, sizeof(sa));
  sa.sa_sigaction = crash_handler;
  sa.sa_flags = SA_SIGINFO;
  sigfillset(&sa.sa_mask);

  for (i = 0; trap_signals[i] >= 0; ++i) {
    if (sigaction(trap_signals[i], &sa, NULL) == -1) {
      log_warn(LD_BUG, "Sigaction failed: %s", strerror(errno));
      rv = -1;
    }
  }

  {
    /* Now, generate (but do not log) a backtrace.  This ensures that
     * libc has pre-loaded the symbols we need to dump things, so that later
     * reads won't be denied by the sandbox code */
    char **symbols;
    int depth = backtrace(cb_buf, MAX_DEPTH);
    symbols = backtrace_symbols(cb_buf, depth);
    if (symbols)
      free(symbols);
  }

  return rv;
}

/** Uninstall crash handlers. */
static void
remove_bt_handler(void)
{
  tor_mutex_uninit(&cb_buf_mutex);
}
#endif

#ifdef NO_BACKTRACE_IMPL
void
log_backtrace(int severity, int domain, const char *msg)
{
  tor_log(severity, domain, "%s. (Stack trace not available)", msg);
}

static int
install_bt_handler(void)
{
  return 0;
}

static void
remove_bt_handler(void)
{
}
#endif

/** Set up code to handle generating error messages on crashes. */
int
configure_backtrace_handler(const char *tor_version)
{
  tor_free(bt_version);
  if (!tor_version)
    tor_version = "";
  tor_asprintf(&bt_version, "Tor %s", tor_version);

  return install_bt_handler();
}

/** Perform end-of-process cleanup for code that generates error messages on
 * crashes.  */
void
clean_up_backtrace_handler(void)
{
  remove_bt_handler();

  tor_free(bt_version);
}

