/* Copyright (c) 2001, Matej Pfajfar.
 * Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file log.c
 * \brief Functions to send messages to log files or the console.
 **/

#include "orconfig.h"
#include <stdarg.h>
#include <assert.h>
// #include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include "compat.h"
#include "util.h"
#define LOG_PRIVATE
#include "torlog.h"
#include "container.h"

/** Given a severity, yields an index into log_severity_list_t.masks to use
 * for that severity. */
#define SEVERITY_MASK_IDX(sev) ((sev) - LOG_ERR)

/** @{ */
/** The string we stick at the end of a log message when it is too long,
 * and its length. */
#define TRUNCATED_STR "[...truncated]"
#define TRUNCATED_STR_LEN 14
/** @} */

#define raw_assert(x) assert(x) // assert OK

/** Information for a single logfile; only used in log.c */
typedef struct logfile_t {
  struct logfile_t *next; /**< Next logfile_t in the linked list. */
  char *filename; /**< Filename to open. */
  int fd; /**< fd to receive log messages, or -1 for none. */
  int seems_dead; /**< Boolean: true if the stream seems to be kaput. */
  int needs_close; /**< Boolean: true if the stream gets closed on shutdown. */
  int is_temporary; /**< Boolean: close after initializing logging subsystem.*/
  int is_syslog; /**< Boolean: send messages to syslog. */
  log_callback callback; /**< If not NULL, send messages to this function. */
  log_severity_list_t *severities; /**< Which severity of messages should we
                                    * log for each log domain? */
} logfile_t;

static void log_free(logfile_t *victim);

/** Helper: map a log severity to descriptive string. */
static inline const char *
sev_to_string(int severity)
{
  switch (severity) {
    case LOG_DEBUG:   return "debug";
    case LOG_INFO:    return "info";
    case LOG_NOTICE:  return "notice";
    case LOG_WARN:    return "warn";
    case LOG_ERR:     return "err";
    default:          /* Call assert, not tor_assert, since tor_assert
                       * calls log on failure. */
                      raw_assert(0); return "UNKNOWN"; // LCOV_EXCL_LINE
  }
}

/** Helper: decide whether to include the function name in the log message. */
static inline int
should_log_function_name(log_domain_mask_t domain, int severity)
{
  switch (severity) {
    case LOG_DEBUG:
    case LOG_INFO:
      /* All debugging messages occur in interesting places. */
      return (domain & LD_NOFUNCNAME) == 0;
    case LOG_NOTICE:
    case LOG_WARN:
    case LOG_ERR:
      /* We care about places where bugs occur. */
      return (domain & (LD_BUG|LD_NOFUNCNAME)) == LD_BUG;
    default:
      /* Call assert, not tor_assert, since tor_assert calls log on failure. */
      raw_assert(0); return 0; // LCOV_EXCL_LINE
  }
}

/** A mutex to guard changes to logfiles and logging. */
static tor_mutex_t log_mutex;
/** True iff we have initialized log_mutex */
static int log_mutex_initialized = 0;

/** Linked list of logfile_t. */
static logfile_t *logfiles = NULL;
/** Boolean: do we report logging domains? */
static int log_domains_are_logged = 0;

#ifdef HAVE_SYSLOG_H
/** The number of open syslog log handlers that we have.  When this reaches 0,
 * we can close our connection to the syslog facility. */
static int syslog_count = 0;
#endif

/** Represents a log message that we are going to send to callback-driven
 * loggers once we can do so in a non-reentrant way. */
typedef struct pending_log_message_t {
  int severity; /**< The severity of the message */
  log_domain_mask_t domain; /**< The domain of the message */
  char *fullmsg; /**< The message, with all decorations */
  char *msg; /**< The content of the message */
} pending_log_message_t;

/** Log messages waiting to be replayed onto callback-based logs */
static smartlist_t *pending_cb_messages = NULL;

/** Log messages waiting to be replayed once the logging system is initialized.
 */
static smartlist_t *pending_startup_messages = NULL;

/** Number of bytes of messages queued in pending_startup_messages.  (This is
 * the length of the messages, not the number of bytes used to store
 * them.) */
static size_t pending_startup_messages_len;

/** True iff we should store messages while waiting for the logs to get
 * configured. */
static int queue_startup_messages = 1;

/** True iff __PRETTY_FUNCTION__ includes parenthesized arguments. */
static int pretty_fn_has_parens = 0;

/** Don't store more than this many bytes of messages while waiting for the
 * logs to get configured. */
#define MAX_STARTUP_MSG_LEN (1<<16)

/** Lock the log_mutex to prevent others from changing the logfile_t list */
#define LOCK_LOGS() STMT_BEGIN                                          \
  tor_assert(log_mutex_initialized);                                    \
  tor_mutex_acquire(&log_mutex);                                        \
  STMT_END
/** Unlock the log_mutex */
#define UNLOCK_LOGS() STMT_BEGIN                                        \
  tor_assert(log_mutex_initialized);                                    \
  tor_mutex_release(&log_mutex);                                        \
  STMT_END

/** What's the lowest log level anybody cares about?  Checking this lets us
 * bail out early from log_debug if we aren't debugging.  */
int log_global_min_severity_ = LOG_NOTICE;

static void delete_log(logfile_t *victim);
static void close_log(logfile_t *victim);

static char *domain_to_string(log_domain_mask_t domain,
                             char *buf, size_t buflen);
static inline char *format_msg(char *buf, size_t buf_len,
           log_domain_mask_t domain, int severity, const char *funcname,
           const char *suffix,
           const char *format, va_list ap, size_t *msg_len_out)
  CHECK_PRINTF(7,0);

/** Name of the application: used to generate the message we write at the
 * start of each new log. */
static char *appname = NULL;

/** Set the "application name" for the logs to <b>name</b>: we'll use this
 * name in the message we write when starting up, and at the start of each new
 * log.
 *
 * Tor uses this string to write the version number to the log file. */
void
log_set_application_name(const char *name)
{
  tor_free(appname);
  appname = name ? tor_strdup(name) : NULL;
}

/** Log time granularity in milliseconds. */
static int log_time_granularity = 1;

/** Define log time granularity for all logs to be <b>granularity_msec</b>
 * milliseconds. */
void
set_log_time_granularity(int granularity_msec)
{
  log_time_granularity = granularity_msec;
}

/** Helper: Write the standard prefix for log lines to a
 * <b>buf_len</b> character buffer in <b>buf</b>.
 */
static inline size_t
log_prefix_(char *buf, size_t buf_len, int severity)
{
  time_t t;
  struct timeval now;
  struct tm tm;
  size_t n;
  int r, ms;

  tor_gettimeofday(&now);
  t = (time_t)now.tv_sec;
  ms = (int)now.tv_usec / 1000;
  if (log_time_granularity >= 1000) {
    t -= t % (log_time_granularity / 1000);
    ms = 0;
  } else {
    ms -= ((int)now.tv_usec / 1000) % log_time_granularity;
  }

  n = strftime(buf, buf_len, "%b %d %H:%M:%S", tor_localtime_r(&t, &tm));
  r = tor_snprintf(buf+n, buf_len-n, ".%.3i [%s] ", ms,
                   sev_to_string(severity));

  if (r<0)
    return buf_len-1;
  else
    return n+r;
}

/** If lf refers to an actual file that we have just opened, and the file
 * contains no data, log an "opening new logfile" message at the top.
 *
 * Return -1 if the log is broken and needs to be deleted, else return 0.
 */
static int
log_tor_version(logfile_t *lf, int reset)
{
  char buf[256];
  size_t n;
  int is_new;

  if (!lf->needs_close)
    /* If it doesn't get closed, it isn't really a file. */
    return 0;
  if (lf->is_temporary)
    /* If it's temporary, it isn't really a file. */
    return 0;

  is_new = lf->fd >= 0 && tor_fd_getpos(lf->fd) == 0;

  if (reset && !is_new)
    /* We are resetting, but we aren't at the start of the file; no
     * need to log again. */
    return 0;
  n = log_prefix_(buf, sizeof(buf), LOG_NOTICE);
  if (appname) {
    tor_snprintf(buf+n, sizeof(buf)-n,
                 "%s opening %slog file.\n", appname, is_new?"new ":"");
  } else {
    tor_snprintf(buf+n, sizeof(buf)-n,
                 "Tor %s opening %slog file.\n", VERSION, is_new?"new ":"");
  }
  if (write_all(lf->fd, buf, strlen(buf), 0) < 0) /* error */
    return -1; /* failed */
  return 0;
}

static const char bug_suffix[] = " (on Tor " VERSION
#ifndef _MSC_VER
  " "
#include "micro-revision.i"
#endif
  ")";

/** Helper: Format a log message into a fixed-sized buffer. (This is
 * factored out of <b>logv</b> so that we never format a message more
 * than once.)  Return a pointer to the first character of the message
 * portion of the formatted string.
 */
static inline char *
format_msg(char *buf, size_t buf_len,
           log_domain_mask_t domain, int severity, const char *funcname,
           const char *suffix,
           const char *format, va_list ap, size_t *msg_len_out)
{
  size_t n;
  int r;
  char *end_of_prefix;
  char *buf_end;

  raw_assert(buf_len >= 16); /* prevent integer underflow and stupidity */
  buf_len -= 2; /* subtract 2 characters so we have room for \n\0 */
  buf_end = buf+buf_len; /* point *after* the last char we can write to */

  n = log_prefix_(buf, buf_len, severity);
  end_of_prefix = buf+n;

  if (log_domains_are_logged) {
    char *cp = buf+n;
    if (cp == buf_end) goto format_msg_no_room_for_domains;
    *cp++ = '{';
    if (cp == buf_end) goto format_msg_no_room_for_domains;
    cp = domain_to_string(domain, cp, (buf+buf_len-cp));
    if (cp == buf_end) goto format_msg_no_room_for_domains;
    *cp++ = '}';
    if (cp == buf_end) goto format_msg_no_room_for_domains;
    *cp++ = ' ';
    if (cp == buf_end) goto format_msg_no_room_for_domains;
    end_of_prefix = cp;
    n = cp-buf;
  format_msg_no_room_for_domains:
    /* This will leave end_of_prefix and n unchanged, and thus cause
     * whatever log domain string we had written to be clobbered. */
    ;
  }

  if (funcname && should_log_function_name(domain, severity)) {
    r = tor_snprintf(buf+n, buf_len-n,
                     pretty_fn_has_parens ? "%s: " : "%s(): ",
                     funcname);
    if (r<0)
      n = strlen(buf);
    else
      n += r;
  }

  if (domain == LD_BUG && buf_len-n > 6) {
    memcpy(buf+n, "Bug: ", 6);
    n += 5;
  }

  r = tor_vsnprintf(buf+n,buf_len-n,format,ap);
  if (r < 0) {
    /* The message was too long; overwrite the end of the buffer with
     * "[...truncated]" */
    if (buf_len >= TRUNCATED_STR_LEN) {
      size_t offset = buf_len-TRUNCATED_STR_LEN;
      /* We have an extra 2 characters after buf_len to hold the \n\0,
       * so it's safe to add 1 to the size here. */
      strlcpy(buf+offset, TRUNCATED_STR, buf_len-offset+1);
    }
    /* Set 'n' to the end of the buffer, where we'll be writing \n\0.
     * Since we already subtracted 2 from buf_len, this is safe.*/
    n = buf_len;
  } else {
    n += r;
    if (suffix) {
      size_t suffix_len = strlen(suffix);
      if (buf_len-n >= suffix_len) {
        memcpy(buf+n, suffix, suffix_len);
        n += suffix_len;
      }
    }
  }

  if (domain == LD_BUG &&
      buf_len - n > strlen(bug_suffix)+1) {
    memcpy(buf+n, bug_suffix, strlen(bug_suffix));
    n += strlen(bug_suffix);
  }

  buf[n]='\n';
  buf[n+1]='\0';
  *msg_len_out = n+1;
  return end_of_prefix;
}

/* Create a new pending_log_message_t with appropriate values */
static pending_log_message_t *
pending_log_message_new(int severity, log_domain_mask_t domain,
                        const char *fullmsg, const char *shortmsg)
{
  pending_log_message_t *m = tor_malloc(sizeof(pending_log_message_t));
  m->severity = severity;
  m->domain = domain;
  m->fullmsg = fullmsg ? tor_strdup(fullmsg) : NULL;
  m->msg = tor_strdup(shortmsg);
  return m;
}

/** Release all storage held by <b>msg</b>. */
static void
pending_log_message_free(pending_log_message_t *msg)
{
  if (!msg)
    return;
  tor_free(msg->msg);
  tor_free(msg->fullmsg);
  tor_free(msg);
}

/** Return true iff <b>lf</b> would like to receive a message with the
 * specified <b>severity</b> in the specified <b>domain</b>.
 */
static inline int
logfile_wants_message(const logfile_t *lf, int severity,
                      log_domain_mask_t domain)
{
  if (! (lf->severities->masks[SEVERITY_MASK_IDX(severity)] & domain)) {
    return 0;
  }
  if (! (lf->fd >= 0 || lf->is_syslog || lf->callback)) {
    return 0;
  }
  if (lf->seems_dead) {
    return 0;
  }

  return 1;
}

/** Send a message to <b>lf</b>.  The full message, with time prefix and
 * severity, is in <b>buf</b>.  The message itself is in
 * <b>msg_after_prefix</b>.  If <b>callbacks_deferred</b> points to true, then
 * we already deferred this message for pending callbacks and don't need to do
 * it again.  Otherwise, if we need to do it, do it, and set
 * <b>callbacks_deferred</b> to 1. */
static inline void
logfile_deliver(logfile_t *lf, const char *buf, size_t msg_len,
                const char *msg_after_prefix, log_domain_mask_t domain,
                int severity, int *callbacks_deferred)
{

  if (lf->is_syslog) {
#ifdef HAVE_SYSLOG_H
#ifdef MAXLINE
    /* Some syslog implementations have limits on the length of what you can
     * pass them, and some very old ones do not detect overflow so well.
     * Regrettably, they call their maximum line length MAXLINE. */
#if MAXLINE < 64
#warn "MAXLINE is a very low number; it might not be from syslog.h after all"
#endif
    char *m = msg_after_prefix;
    if (msg_len >= MAXLINE)
      m = tor_strndup(msg_after_prefix, MAXLINE-1);
    syslog(severity, "%s", m);
    if (m != msg_after_prefix) {
      tor_free(m);
    }
#else
    /* We have syslog but not MAXLINE.  That's promising! */
    syslog(severity, "%s", msg_after_prefix);
#endif
#endif
  } else if (lf->callback) {
    if (domain & LD_NOCB) {
      if (!*callbacks_deferred && pending_cb_messages) {
        smartlist_add(pending_cb_messages,
            pending_log_message_new(severity,domain,NULL,msg_after_prefix));
        *callbacks_deferred = 1;
      }
    } else {
      lf->callback(severity, domain, msg_after_prefix);
    }
  } else {
    if (write_all(lf->fd, buf, msg_len, 0) < 0) { /* error */
      /* don't log the error! mark this log entry to be blown away, and
       * continue. */
      lf->seems_dead = 1;
    }
  }
}

/** Helper: sends a message to the appropriate logfiles, at loglevel
 * <b>severity</b>.  If provided, <b>funcname</b> is prepended to the
 * message.  The actual message is derived as from tor_snprintf(format,ap).
 */
MOCK_IMPL(STATIC void,
logv,(int severity, log_domain_mask_t domain, const char *funcname,
     const char *suffix, const char *format, va_list ap))
{
  char buf[10240];
  size_t msg_len = 0;
  int formatted = 0;
  logfile_t *lf;
  char *end_of_prefix=NULL;
  int callbacks_deferred = 0;

  /* Call assert, not tor_assert, since tor_assert calls log on failure. */
  raw_assert(format);
  /* check that severity is sane.  Overrunning the masks array leads to
   * interesting and hard to diagnose effects */
  raw_assert(severity >= LOG_ERR && severity <= LOG_DEBUG);
  /* check that we've initialised the log mutex before we try to lock it */
  raw_assert(log_mutex_initialized);
  LOCK_LOGS();

  if ((! (domain & LD_NOCB)) && pending_cb_messages
      && smartlist_len(pending_cb_messages))
    flush_pending_log_callbacks();

  if (queue_startup_messages &&
      pending_startup_messages_len < MAX_STARTUP_MSG_LEN) {
    end_of_prefix =
      format_msg(buf, sizeof(buf), domain, severity, funcname, suffix,
      format, ap, &msg_len);
    formatted = 1;

    smartlist_add(pending_startup_messages,
      pending_log_message_new(severity,domain,buf,end_of_prefix));
    pending_startup_messages_len += msg_len;
  }

  for (lf = logfiles; lf; lf = lf->next) {
    if (! logfile_wants_message(lf, severity, domain))
      continue;

    if (!formatted) {
      end_of_prefix =
        format_msg(buf, sizeof(buf), domain, severity, funcname, suffix,
                   format, ap, &msg_len);
      formatted = 1;
    }

    logfile_deliver(lf, buf, msg_len, end_of_prefix, domain, severity,
      &callbacks_deferred);
  }
  UNLOCK_LOGS();
}

/** Output a message to the log.  It gets logged to all logfiles that
 * care about messages with <b>severity</b> in <b>domain</b>. The content
 * is formatted printf-style based on <b>format</b> and extra arguments.
 * */
void
tor_log(int severity, log_domain_mask_t domain, const char *format, ...)
{
  va_list ap;
  if (severity > log_global_min_severity_)
    return;
  va_start(ap,format);
#ifdef TOR_UNIT_TESTS
  if (domain & LD_NO_MOCK)
    logv__real(severity, domain, NULL, NULL, format, ap);
  else
#endif
  logv(severity, domain, NULL, NULL, format, ap);
  va_end(ap);
}

/** Maximum number of fds that will get notifications if we crash */
#define MAX_SIGSAFE_FDS 8
/** Array of fds to log crash-style warnings to. */
static int sigsafe_log_fds[MAX_SIGSAFE_FDS] = { STDERR_FILENO };
/** The number of elements used in sigsafe_log_fds */
static int n_sigsafe_log_fds = 1;

/** Write <b>s</b> to each element of sigsafe_log_fds. Return 0 on success, -1
 * on failure. */
static int
tor_log_err_sigsafe_write(const char *s)
{
  int i;
  ssize_t r;
  size_t len = strlen(s);
  int err = 0;
  for (i=0; i < n_sigsafe_log_fds; ++i) {
    r = write(sigsafe_log_fds[i], s, len);
    err += (r != (ssize_t)len);
  }
  return err ? -1 : 0;
}

/** Given a list of string arguments ending with a NULL, writes them
 * to our logs and to stderr (if possible).  This function is safe to call
 * from within a signal handler. */
void
tor_log_err_sigsafe(const char *m, ...)
{
  va_list ap;
  const char *x;
  char timebuf[33];
  time_t now = time(NULL);

  if (!m)
    return;
  if (log_time_granularity >= 2000) {
    int g = log_time_granularity / 1000;
    now -= now % g;
  }
  timebuf[0] = now < 0 ? '-' : ' ';
  if (now < 0) now = -now;
  timebuf[1] = '\0';
  format_dec_number_sigsafe(now, timebuf+1, sizeof(timebuf)-1);
  tor_log_err_sigsafe_write("\n=========================================="
                             "================== T=");
  tor_log_err_sigsafe_write(timebuf);
  tor_log_err_sigsafe_write("\n");
  tor_log_err_sigsafe_write(m);
  va_start(ap, m);
  while ((x = va_arg(ap, const char*))) {
    tor_log_err_sigsafe_write(x);
  }
  va_end(ap);
}

/** Set *<b>out</b> to a pointer to an array of the fds to log errors to from
 * inside a signal handler. Return the number of elements in the array. */
int
tor_log_get_sigsafe_err_fds(const int **out)
{
  *out = sigsafe_log_fds;
  return n_sigsafe_log_fds;
}

/** Helper function; return true iff the <b>n</b>-element array <b>array</b>
 * contains <b>item</b>. */
static int
int_array_contains(const int *array, int n, int item)
{
  int j;
  for (j = 0; j < n; ++j) {
    if (array[j] == item)
      return 1;
  }
  return 0;
}

/** Function to call whenever the list of logs changes to get ready to log
 * from signal handlers. */
void
tor_log_update_sigsafe_err_fds(void)
{
  const logfile_t *lf;
  int found_real_stderr = 0;

  LOCK_LOGS();
  /* Reserve the first one for stderr. This is safe because when we daemonize,
   * we dup2 /dev/null to stderr, */
  sigsafe_log_fds[0] = STDERR_FILENO;
  n_sigsafe_log_fds = 1;

  for (lf = logfiles; lf; lf = lf->next) {
     /* Don't try callback to the control port, or syslogs: We can't
      * do them from a signal handler. Don't try stdout: we always do stderr.
      */
    if (lf->is_temporary || lf->is_syslog ||
        lf->callback || lf->seems_dead || lf->fd < 0)
      continue;
    if (lf->severities->masks[SEVERITY_MASK_IDX(LOG_ERR)] &
        (LD_BUG|LD_GENERAL)) {
      if (lf->fd == STDERR_FILENO)
        found_real_stderr = 1;
      /* Avoid duplicates */
      if (int_array_contains(sigsafe_log_fds, n_sigsafe_log_fds, lf->fd))
        continue;
      sigsafe_log_fds[n_sigsafe_log_fds++] = lf->fd;
      if (n_sigsafe_log_fds == MAX_SIGSAFE_FDS)
        break;
    }
  }

  if (!found_real_stderr &&
      int_array_contains(sigsafe_log_fds, n_sigsafe_log_fds, STDOUT_FILENO)) {
    /* Don't use a virtual stderr when we're also logging to stdout. */
    raw_assert(n_sigsafe_log_fds >= 2); /* Don't tor_assert inside log fns */
    sigsafe_log_fds[0] = sigsafe_log_fds[--n_sigsafe_log_fds];
  }

  UNLOCK_LOGS();
}

/** Add to <b>out</b> a copy of every currently configured log file name. Used
 * to enable access to these filenames with the sandbox code. */
void
tor_log_get_logfile_names(smartlist_t *out)
{
  logfile_t *lf;
  tor_assert(out);

  LOCK_LOGS();

  for (lf = logfiles; lf; lf = lf->next) {
    if (lf->is_temporary || lf->is_syslog || lf->callback)
      continue;
    if (lf->filename == NULL)
      continue;
    smartlist_add(out, tor_strdup(lf->filename));
  }

  UNLOCK_LOGS();
}

/** Implementation of the log_fn backend, used when we have
 * variadic macros. All arguments are as for log_fn, except for
 * <b>fn</b>, which is the name of the calling functions. */
void
log_fn_(int severity, log_domain_mask_t domain, const char *fn,
        const char *format, ...)
{
  va_list ap;
  if (severity > log_global_min_severity_)
    return;
  va_start(ap,format);
  logv(severity, domain, fn, NULL, format, ap);
  va_end(ap);
}
void
log_fn_ratelim_(ratelim_t *ratelim, int severity, log_domain_mask_t domain,
                const char *fn, const char *format, ...)
{
  va_list ap;
  char *m;
  if (severity > log_global_min_severity_)
    return;
  m = rate_limit_log(ratelim, approx_time());
  if (m == NULL)
      return;
  va_start(ap, format);
  logv(severity, domain, fn, m, format, ap);
  va_end(ap);
  tor_free(m);
}

/** Free all storage held by <b>victim</b>. */
static void
log_free(logfile_t *victim)
{
  if (!victim)
    return;
  tor_free(victim->severities);
  tor_free(victim->filename);
  tor_free(victim);
}

/** Close all open log files, and free other static memory. */
void
logs_free_all(void)
{
  logfile_t *victim, *next;
  smartlist_t *messages, *messages2;
  LOCK_LOGS();
  next = logfiles;
  logfiles = NULL;
  messages = pending_cb_messages;
  pending_cb_messages = NULL;
  messages2 = pending_startup_messages;
  pending_startup_messages = NULL;
  UNLOCK_LOGS();
  while (next) {
    victim = next;
    next = next->next;
    close_log(victim);
    log_free(victim);
  }
  tor_free(appname);

  SMARTLIST_FOREACH(messages, pending_log_message_t *, msg, {
      pending_log_message_free(msg);
    });
  smartlist_free(messages);

  if (messages2) {
    SMARTLIST_FOREACH(messages2, pending_log_message_t *, msg, {
        pending_log_message_free(msg);
      });
    smartlist_free(messages2);
  }

  /* We _could_ destroy the log mutex here, but that would screw up any logs
   * that happened between here and the end of execution. */
}

/** Remove and free the log entry <b>victim</b> from the linked-list
 * logfiles (it is probably present, but it might not be due to thread
 * racing issues). After this function is called, the caller shouldn't
 * refer to <b>victim</b> anymore.
 *
 * Long-term, we need to do something about races in the log subsystem
 * in general. See bug 222 for more details.
 */
static void
delete_log(logfile_t *victim)
{
  logfile_t *tmpl;
  if (victim == logfiles)
    logfiles = victim->next;
  else {
    for (tmpl = logfiles; tmpl && tmpl->next != victim; tmpl=tmpl->next) ;
//    tor_assert(tmpl);
//    tor_assert(tmpl->next == victim);
    if (!tmpl)
      return;
    tmpl->next = victim->next;
  }
  log_free(victim);
}

/** Helper: release system resources (but not memory) held by a single
 * logfile_t. */
static void
close_log(logfile_t *victim)
{
  if (victim->needs_close && victim->fd >= 0) {
    close(victim->fd);
    victim->fd = -1;
  } else if (victim->is_syslog) {
#ifdef HAVE_SYSLOG_H
    if (--syslog_count == 0) {
      /* There are no other syslogs; close the logging facility. */
      closelog();
    }
#endif
  }
}

/** Adjust a log severity configuration in <b>severity_out</b> to contain
 * every domain between <b>loglevelMin</b> and <b>loglevelMax</b>, inclusive.
 */
void
set_log_severity_config(int loglevelMin, int loglevelMax,
                        log_severity_list_t *severity_out)
{
  int i;
  tor_assert(loglevelMin >= loglevelMax);
  tor_assert(loglevelMin >= LOG_ERR && loglevelMin <= LOG_DEBUG);
  tor_assert(loglevelMax >= LOG_ERR && loglevelMax <= LOG_DEBUG);
  memset(severity_out, 0, sizeof(log_severity_list_t));
  for (i = loglevelMin; i >= loglevelMax; --i) {
    severity_out->masks[SEVERITY_MASK_IDX(i)] = ~0u;
  }
}

/** Add a log handler named <b>name</b> to send all messages in <b>severity</b>
 * to <b>fd</b>. Copies <b>severity</b>. Helper: does no locking. */
static void
add_stream_log_impl(const log_severity_list_t *severity,
                    const char *name, int fd)
{
  logfile_t *lf;
  lf = tor_malloc_zero(sizeof(logfile_t));
  lf->fd = fd;
  lf->filename = tor_strdup(name);
  lf->severities = tor_memdup(severity, sizeof(log_severity_list_t));
  lf->next = logfiles;

  logfiles = lf;
  log_global_min_severity_ = get_min_log_level();
}

/** Add a log handler named <b>name</b> to send all messages in <b>severity</b>
 * to <b>fd</b>. Steals a reference to <b>severity</b>; the caller must
 * not use it after calling this function. */
void
add_stream_log(const log_severity_list_t *severity, const char *name, int fd)
{
  LOCK_LOGS();
  add_stream_log_impl(severity, name, fd);
  UNLOCK_LOGS();
}

/** Initialize the global logging facility */
void
init_logging(int disable_startup_queue)
{
  if (!log_mutex_initialized) {
    tor_mutex_init(&log_mutex);
    log_mutex_initialized = 1;
  }
#ifdef __GNUC__
  if (strchr(__PRETTY_FUNCTION__, '(')) {
    pretty_fn_has_parens = 1;
  }
#endif
  if (pending_cb_messages == NULL)
    pending_cb_messages = smartlist_new();
  if (disable_startup_queue)
    queue_startup_messages = 0;
  if (pending_startup_messages == NULL && queue_startup_messages) {
    pending_startup_messages = smartlist_new();
  }
}

/** Set whether we report logging domains as a part of our log messages.
 */
void
logs_set_domain_logging(int enabled)
{
  LOCK_LOGS();
  log_domains_are_logged = enabled;
  UNLOCK_LOGS();
}

/** Add a log handler to receive messages during startup (before the real
 * logs are initialized).
 */
void
add_temp_log(int min_severity)
{
  log_severity_list_t *s = tor_malloc_zero(sizeof(log_severity_list_t));
  set_log_severity_config(min_severity, LOG_ERR, s);
  LOCK_LOGS();
  add_stream_log_impl(s, "<temp>", fileno(stdout));
  tor_free(s);
  logfiles->is_temporary = 1;
  UNLOCK_LOGS();
}

/**
 * Add a log handler to send messages in <b>severity</b>
 * to the function <b>cb</b>.
 */
int
add_callback_log(const log_severity_list_t *severity, log_callback cb)
{
  logfile_t *lf;
  lf = tor_malloc_zero(sizeof(logfile_t));
  lf->fd = -1;
  lf->severities = tor_memdup(severity, sizeof(log_severity_list_t));
  lf->filename = tor_strdup("<callback>");
  lf->callback = cb;
  lf->next = logfiles;

  LOCK_LOGS();
  logfiles = lf;
  log_global_min_severity_ = get_min_log_level();
  UNLOCK_LOGS();
  return 0;
}

/** Adjust the configured severity of any logs whose callback function is
 * <b>cb</b>. */
void
change_callback_log_severity(int loglevelMin, int loglevelMax,
                             log_callback cb)
{
  logfile_t *lf;
  log_severity_list_t severities;
  set_log_severity_config(loglevelMin, loglevelMax, &severities);
  LOCK_LOGS();
  for (lf = logfiles; lf; lf = lf->next) {
    if (lf->callback == cb) {
      memcpy(lf->severities, &severities, sizeof(severities));
    }
  }
  log_global_min_severity_ = get_min_log_level();
  UNLOCK_LOGS();
}

/** If there are any log messages that were generated with LD_NOCB waiting to
 * be sent to callback-based loggers, send them now. */
void
flush_pending_log_callbacks(void)
{
  logfile_t *lf;
  smartlist_t *messages, *messages_tmp;

  LOCK_LOGS();
  if (!pending_cb_messages || 0 == smartlist_len(pending_cb_messages)) {
    UNLOCK_LOGS();
    return;
  }

  messages = pending_cb_messages;
  pending_cb_messages = smartlist_new();
  do {
    SMARTLIST_FOREACH_BEGIN(messages, pending_log_message_t *, msg) {
      const int severity = msg->severity;
      const int domain = msg->domain;
      for (lf = logfiles; lf; lf = lf->next) {
        if (! lf->callback || lf->seems_dead ||
            ! (lf->severities->masks[SEVERITY_MASK_IDX(severity)] & domain)) {
          continue;
        }
        lf->callback(severity, domain, msg->msg);
      }
      pending_log_message_free(msg);
    } SMARTLIST_FOREACH_END(msg);
    smartlist_clear(messages);

    messages_tmp = pending_cb_messages;
    pending_cb_messages = messages;
    messages = messages_tmp;
  } while (smartlist_len(messages));

  smartlist_free(messages);

  UNLOCK_LOGS();
}

/** Flush all the messages we stored from startup while waiting for log
 * initialization.
 */
void
flush_log_messages_from_startup(void)
{
  logfile_t *lf;

  LOCK_LOGS();
  queue_startup_messages = 0;
  pending_startup_messages_len = 0;
  if (! pending_startup_messages)
    goto out;

  SMARTLIST_FOREACH_BEGIN(pending_startup_messages, pending_log_message_t *,
                          msg) {
    int callbacks_deferred = 0;
    for (lf = logfiles; lf; lf = lf->next) {
      if (! logfile_wants_message(lf, msg->severity, msg->domain))
        continue;

      /* We configure a temporary startup log that goes to stdout, so we
       * shouldn't replay to stdout/stderr*/
      if (lf->fd == STDOUT_FILENO || lf->fd == STDERR_FILENO) {
        continue;
      }

      logfile_deliver(lf, msg->fullmsg, strlen(msg->fullmsg), msg->msg,
                      msg->domain, msg->severity, &callbacks_deferred);
    }
    pending_log_message_free(msg);
  } SMARTLIST_FOREACH_END(msg);
  smartlist_free(pending_startup_messages);
  pending_startup_messages = NULL;

 out:
  UNLOCK_LOGS();
}

/** Close any log handlers added by add_temp_log() or marked by
 * mark_logs_temp(). */
void
close_temp_logs(void)
{
  logfile_t *lf, **p;

  LOCK_LOGS();
  for (p = &logfiles; *p; ) {
    if ((*p)->is_temporary) {
      lf = *p;
      /* we use *p here to handle the edge case of the head of the list */
      *p = (*p)->next;
      close_log(lf);
      log_free(lf);
    } else {
      p = &((*p)->next);
    }
  }

  log_global_min_severity_ = get_min_log_level();
  UNLOCK_LOGS();
}

/** Make all currently temporary logs (set to be closed by close_temp_logs)
 * live again, and close all non-temporary logs. */
void
rollback_log_changes(void)
{
  logfile_t *lf;
  LOCK_LOGS();
  for (lf = logfiles; lf; lf = lf->next)
    lf->is_temporary = ! lf->is_temporary;
  UNLOCK_LOGS();
  close_temp_logs();
}

/** Configure all log handles to be closed by close_temp_logs(). */
void
mark_logs_temp(void)
{
  logfile_t *lf;
  LOCK_LOGS();
  for (lf = logfiles; lf; lf = lf->next)
    lf->is_temporary = 1;
  UNLOCK_LOGS();
}

/**
 * Add a log handler to send messages to <b>filename</b>. If opening the
 * logfile fails, -1 is returned and errno is set appropriately (by open(2)).
 */
int
add_file_log(const log_severity_list_t *severity, const char *filename,
             const int truncate_log)
{
  int fd;
  logfile_t *lf;

  int open_flags = O_WRONLY|O_CREAT;
  open_flags |= truncate_log ? O_TRUNC : O_APPEND;

  fd = tor_open_cloexec(filename, open_flags, 0644);
  if (fd<0)
    return -1;
  if (tor_fd_seekend(fd)<0) {
    close(fd);
    return -1;
  }

  LOCK_LOGS();
  add_stream_log_impl(severity, filename, fd);
  logfiles->needs_close = 1;
  lf = logfiles;
  log_global_min_severity_ = get_min_log_level();

  if (log_tor_version(lf, 0) < 0) {
    delete_log(lf);
  }
  UNLOCK_LOGS();

  return 0;
}

#ifdef HAVE_SYSLOG_H
/**
 * Add a log handler to send messages to they system log facility.
 *
 * If this is the first log handler, opens syslog with ident Tor or
 * Tor-<syslog_identity_tag> if that is not NULL.
 */
int
add_syslog_log(const log_severity_list_t *severity,
               const char* syslog_identity_tag)
{
  logfile_t *lf;
  if (syslog_count++ == 0) {
    /* This is the first syslog. */
    static char buf[256];
    if (syslog_identity_tag) {
      tor_snprintf(buf, sizeof(buf), "Tor-%s", syslog_identity_tag);
    } else {
      tor_snprintf(buf, sizeof(buf), "Tor");
    }
    openlog(buf, LOG_PID | LOG_NDELAY, LOGFACILITY);
  }

  lf = tor_malloc_zero(sizeof(logfile_t));
  lf->fd = -1;
  lf->severities = tor_memdup(severity, sizeof(log_severity_list_t));
  lf->filename = tor_strdup("<syslog>");
  lf->is_syslog = 1;

  LOCK_LOGS();
  lf->next = logfiles;
  logfiles = lf;
  log_global_min_severity_ = get_min_log_level();
  UNLOCK_LOGS();
  return 0;
}
#endif

/** If <b>level</b> is a valid log severity, return the corresponding
 * numeric value.  Otherwise, return -1. */
int
parse_log_level(const char *level)
{
  if (!strcasecmp(level, "err"))
    return LOG_ERR;
  if (!strcasecmp(level, "warn"))
    return LOG_WARN;
  if (!strcasecmp(level, "notice"))
    return LOG_NOTICE;
  if (!strcasecmp(level, "info"))
    return LOG_INFO;
  if (!strcasecmp(level, "debug"))
    return LOG_DEBUG;
  return -1;
}

/** Return the string equivalent of a given log level. */
const char *
log_level_to_string(int level)
{
  return sev_to_string(level);
}

/** NULL-terminated array of names for log domains such that domain_list[dom]
 * is a description of <b>dom</b>. */
static const char *domain_list[] = {
  "GENERAL", "CRYPTO", "NET", "CONFIG", "FS", "PROTOCOL", "MM",
  "HTTP", "APP", "CONTROL", "CIRC", "REND", "BUG", "DIR", "DIRSERV",
  "OR", "EDGE", "ACCT", "HIST", "HANDSHAKE", "HEARTBEAT", "CHANNEL",
  "SCHED", NULL
};

/** Return a bitmask for the log domain for which <b>domain</b> is the name,
 * or 0 if there is no such name. */
static log_domain_mask_t
parse_log_domain(const char *domain)
{
  int i;
  for (i=0; domain_list[i]; ++i) {
    if (!strcasecmp(domain, domain_list[i]))
      return (1u<<i);
  }
  return 0;
}

/** Translate a bitmask of log domains to a string. */
static char *
domain_to_string(log_domain_mask_t domain, char *buf, size_t buflen)
{
  char *cp = buf;
  char *eos = buf+buflen;

  buf[0] = '\0';
  if (! domain)
    return buf;
  while (1) {
    const char *d;
    int bit = tor_log2(domain);
    size_t n;
    if ((unsigned)bit >= ARRAY_LENGTH(domain_list)-1 ||
        bit >= N_LOGGING_DOMAINS) {
      tor_snprintf(buf, buflen, "<BUG:Unknown domain %lx>", (long)domain);
      return buf+strlen(buf);
    }
    d = domain_list[bit];
    n = strlcpy(cp, d, eos-cp);
    if (n >= buflen) {
      tor_snprintf(buf, buflen, "<BUG:Truncating domain %lx>", (long)domain);
      return buf+strlen(buf);
    }
    cp += n;
    domain &= ~(1<<bit);

    if (domain == 0 || (eos-cp) < 2)
      return cp;

    memcpy(cp, ",", 2); /*Nul-terminated ,"*/
    cp++;
  }
}

/** Parse a log severity pattern in *<b>cfg_ptr</b>.  Advance cfg_ptr after
 * the end of the severityPattern.  Set the value of <b>severity_out</b> to
 * the parsed pattern.  Return 0 on success, -1 on failure.
 *
 * The syntax for a SeverityPattern is:
 * <pre>
 *   SeverityPattern = *(DomainSeverity SP)* DomainSeverity
 *   DomainSeverity = (DomainList SP)? SeverityRange
 *   SeverityRange = MinSeverity ("-" MaxSeverity )?
 *   DomainList = "[" (SP? DomainSpec SP? ",") SP? DomainSpec "]"
 *   DomainSpec = "*" | Domain | "~" Domain
 * </pre>
 * A missing MaxSeverity defaults to ERR.  Severities and domains are
 * case-insensitive.  "~" indicates negation for a domain; negation happens
 * last inside a DomainList.  Only one SeverityRange without a DomainList is
 * allowed per line.
 */
int
parse_log_severity_config(const char **cfg_ptr,
                          log_severity_list_t *severity_out)
{
  const char *cfg = *cfg_ptr;
  int got_anything = 0;
  int got_an_unqualified_range = 0;
  memset(severity_out, 0, sizeof(*severity_out));

  cfg = eat_whitespace(cfg);
  while (*cfg) {
    const char *dash, *space;
    char *sev_lo, *sev_hi;
    int low, high, i;
    log_domain_mask_t domains = ~0u;

    if (*cfg == '[') {
      int err = 0;
      char *domains_str;
      smartlist_t *domains_list;
      log_domain_mask_t neg_domains = 0;
      const char *closebracket = strchr(cfg, ']');
      if (!closebracket)
        return -1;
      domains = 0;
      domains_str = tor_strndup(cfg+1, closebracket-cfg-1);
      domains_list = smartlist_new();
      smartlist_split_string(domains_list, domains_str, ",", SPLIT_SKIP_SPACE,
                             -1);
      tor_free(domains_str);
      SMARTLIST_FOREACH_BEGIN(domains_list, const char *, domain) {
            if (!strcmp(domain, "*")) {
              domains = ~0u;
            } else {
              int d;
              int negate=0;
              if (*domain == '~') {
                negate = 1;
                ++domain;
              }
              d = parse_log_domain(domain);
              if (!d) {
                log_warn(LD_CONFIG, "No such logging domain as %s", domain);
                err = 1;
              } else {
                if (negate)
                  neg_domains |= d;
                else
                  domains |= d;
              }
            }
      } SMARTLIST_FOREACH_END(domain);
      SMARTLIST_FOREACH(domains_list, char *, d, tor_free(d));
      smartlist_free(domains_list);
      if (err)
        return -1;
      if (domains == 0 && neg_domains)
        domains = ~neg_domains;
      else
        domains &= ~neg_domains;
      cfg = eat_whitespace(closebracket+1);
    } else {
      ++got_an_unqualified_range;
    }
    if (!strcasecmpstart(cfg, "file") ||
        !strcasecmpstart(cfg, "stderr") ||
        !strcasecmpstart(cfg, "stdout") ||
        !strcasecmpstart(cfg, "syslog")) {
      goto done;
    }
    if (got_an_unqualified_range > 1)
      return -1;

    space = strchr(cfg, ' ');
    dash = strchr(cfg, '-');
    if (!space)
      space = strchr(cfg, '\0');
    if (dash && dash < space) {
      sev_lo = tor_strndup(cfg, dash-cfg);
      sev_hi = tor_strndup(dash+1, space-(dash+1));
    } else {
      sev_lo = tor_strndup(cfg, space-cfg);
      sev_hi = tor_strdup("ERR");
    }
    low = parse_log_level(sev_lo);
    high = parse_log_level(sev_hi);
    tor_free(sev_lo);
    tor_free(sev_hi);
    if (low == -1)
      return -1;
    if (high == -1)
      return -1;

    got_anything = 1;
    for (i=low; i >= high; --i)
      severity_out->masks[SEVERITY_MASK_IDX(i)] |= domains;

    cfg = eat_whitespace(space);
  }

 done:
  *cfg_ptr = cfg;
  return got_anything ? 0 : -1;
}

/** Return the least severe log level that any current log is interested in. */
int
get_min_log_level(void)
{
  logfile_t *lf;
  int i;
  int min = LOG_ERR;
  for (lf = logfiles; lf; lf = lf->next) {
    for (i = LOG_DEBUG; i > min; --i)
      if (lf->severities->masks[SEVERITY_MASK_IDX(i)])
        min = i;
  }
  return min;
}

/** Switch all logs to output at most verbose level. */
void
switch_logs_debug(void)
{
  logfile_t *lf;
  int i;
  LOCK_LOGS();
  for (lf = logfiles; lf; lf=lf->next) {
    for (i = LOG_DEBUG; i >= LOG_ERR; --i)
      lf->severities->masks[SEVERITY_MASK_IDX(i)] = ~0u;
  }
  log_global_min_severity_ = get_min_log_level();
  UNLOCK_LOGS();
}

/** Truncate all the log files. */
void
truncate_logs(void)
{
  logfile_t *lf;
  for (lf = logfiles; lf; lf = lf->next) {
    if (lf->fd >= 0) {
      tor_ftruncate(lf->fd);
    }
  }
}

