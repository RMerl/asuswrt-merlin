/* Copyright (c) 2001, Matej Pfajfar.
 * Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file torlog.h
 *
 * \brief Headers for log.c
 **/

#ifndef TOR_TORLOG_H

#include "compat.h"
#include "testsupport.h"

#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#define LOG_WARN LOG_WARNING
#if LOG_DEBUG < LOG_ERR
#error "Your syslog.h thinks high numbers are more important.  " \
       "We aren't prepared to deal with that."
#endif
#else
/* Note: Syslog's logging code refers to priorities, with 0 being the most
 * important.  Thus, all our comparisons needed to be reversed when we added
 * syslog support.
 *
 * The upshot of this is that comments about log levels may be messed up: for
 * "maximum severity" read "most severe" and "numerically *lowest* severity".
 */

/** Debug-level severity: for hyper-verbose messages of no interest to
 * anybody but developers. */
#define LOG_DEBUG   7
/** Info-level severity: for messages that appear frequently during normal
 * operation. */
#define LOG_INFO    6
/** Notice-level severity: for messages that appear infrequently
 * during normal operation; that the user will probably care about;
 * and that are not errors.
 */
#define LOG_NOTICE  5
/** Warn-level severity: for messages that only appear when something has gone
 * wrong. */
#define LOG_WARN    4
/** Error-level severity: for messages that only appear when something has gone
 * very wrong, and the Tor process can no longer proceed. */
#define LOG_ERR     3
#endif

/* Logging domains */

/** Catch-all for miscellaneous events and fatal errors. */
#define LD_GENERAL  (1u<<0)
/** The cryptography subsystem. */
#define LD_CRYPTO   (1u<<1)
/** Networking. */
#define LD_NET      (1u<<2)
/** Parsing and acting on our configuration. */
#define LD_CONFIG   (1u<<3)
/** Reading and writing from the filesystem. */
#define LD_FS       (1u<<4)
/** Other servers' (non)compliance with the Tor protocol. */
#define LD_PROTOCOL (1u<<5)
/** Memory management. */
#define LD_MM       (1u<<6)
/** HTTP implementation. */
#define LD_HTTP     (1u<<7)
/** Application (socks) requests. */
#define LD_APP      (1u<<8)
/** Communication via the controller protocol. */
#define LD_CONTROL  (1u<<9)
/** Building, using, and managing circuits. */
#define LD_CIRC     (1u<<10)
/** Hidden services. */
#define LD_REND     (1u<<11)
/** Internal errors in this Tor process. */
#define LD_BUG      (1u<<12)
/** Learning and using information about Tor servers. */
#define LD_DIR      (1u<<13)
/** Learning and using information about Tor servers. */
#define LD_DIRSERV  (1u<<14)
/** Onion routing protocol. */
#define LD_OR       (1u<<15)
/** Generic edge-connection functionality. */
#define LD_EDGE     (1u<<16)
#define LD_EXIT     LD_EDGE
/** Bandwidth accounting. */
#define LD_ACCT     (1u<<17)
/** Router history */
#define LD_HIST     (1u<<18)
/** OR handshaking */
#define LD_HANDSHAKE (1u<<19)
/** Heartbeat messages */
#define LD_HEARTBEAT (1u<<20)
/** Abstract channel_t code */
#define LD_CHANNEL   (1u<<21)
/** Scheduler */
#define LD_SCHED     (1u<<22)
/** Number of logging domains in the code. */
#define N_LOGGING_DOMAINS 23

/** This log message is not safe to send to a callback-based logger
 * immediately.  Used as a flag, not a log domain. */
#define LD_NOCB (1u<<31)
/** This log message should not include a function name, even if it otherwise
 * would. Used as a flag, not a log domain. */
#define LD_NOFUNCNAME (1u<<30)

#ifdef TOR_UNIT_TESTS
/** This log message should not be intercepted by mock_saving_logv */
#define LD_NO_MOCK (1u<<29)
#endif

/** Mask of zero or more log domains, OR'd together. */
typedef uint32_t log_domain_mask_t;

/** Configures which severities are logged for each logging domain for a given
 * log target. */
typedef struct log_severity_list_t {
  /** For each log severity, a bitmask of which domains a given logger is
   * logging. */
  log_domain_mask_t masks[LOG_DEBUG-LOG_ERR+1];
} log_severity_list_t;

/** Callback type used for add_callback_log. */
typedef void (*log_callback)(int severity, uint32_t domain, const char *msg);

void init_logging(int disable_startup_queue);
int parse_log_level(const char *level);
const char *log_level_to_string(int level);
int parse_log_severity_config(const char **cfg,
                              log_severity_list_t *severity_out);
void set_log_severity_config(int minSeverity, int maxSeverity,
                             log_severity_list_t *severity_out);
void add_stream_log(const log_severity_list_t *severity, const char *name,
                    int fd);
int add_file_log(const log_severity_list_t *severity, const char *filename,
                 const int truncate);
#ifdef HAVE_SYSLOG_H
int add_syslog_log(const log_severity_list_t *severity,
                   const char* syslog_identity_tag);
#endif
int add_callback_log(const log_severity_list_t *severity, log_callback cb);
void logs_set_domain_logging(int enabled);
int get_min_log_level(void);
void switch_logs_debug(void);
void logs_free_all(void);
void add_temp_log(int min_severity);
void close_temp_logs(void);
void rollback_log_changes(void);
void mark_logs_temp(void);
void change_callback_log_severity(int loglevelMin, int loglevelMax,
                                  log_callback cb);
void flush_pending_log_callbacks(void);
void flush_log_messages_from_startup(void);
void log_set_application_name(const char *name);
void set_log_time_granularity(int granularity_msec);
void truncate_logs(void);

void tor_log(int severity, log_domain_mask_t domain, const char *format, ...)
  CHECK_PRINTF(3,4);

void tor_log_err_sigsafe(const char *m, ...);
int tor_log_get_sigsafe_err_fds(const int **out);
void tor_log_update_sigsafe_err_fds(void);

struct smartlist_t;
void tor_log_get_logfile_names(struct smartlist_t *out);

extern int log_global_min_severity_;

void log_fn_(int severity, log_domain_mask_t domain,
             const char *funcname, const char *format, ...)
  CHECK_PRINTF(4,5);
struct ratelim_t;
void log_fn_ratelim_(struct ratelim_t *ratelim, int severity,
                     log_domain_mask_t domain, const char *funcname,
                     const char *format, ...)
  CHECK_PRINTF(5,6);

#if defined(__GNUC__) && __GNUC__ <= 3

/* These are the GCC varidaic macros, so that older versions of GCC don't
 * break. */

/** Log a message at level <b>severity</b>, using a pretty-printed version
 * of the current function name. */
#define log_fn(severity, domain, args...)               \
  log_fn_(severity, domain, __FUNCTION__, args)
/** As log_fn, but use <b>ratelim</b> (an instance of ratelim_t) to control
 * the frequency at which messages can appear.
 */
#define log_fn_ratelim(ratelim, severity, domain, args...)      \
  log_fn_ratelim_(ratelim, severity, domain, __FUNCTION__, args)
#define log_debug(domain, args...)                                      \
  STMT_BEGIN                                                            \
    if (PREDICT_UNLIKELY(log_global_min_severity_ == LOG_DEBUG))        \
      log_fn_(LOG_DEBUG, domain, __FUNCTION__, args);            \
  STMT_END
#define log_info(domain, args...)                           \
  log_fn_(LOG_INFO, domain, __FUNCTION__, args)
#define log_notice(domain, args...)                         \
  log_fn_(LOG_NOTICE, domain, __FUNCTION__, args)
#define log_warn(domain, args...)                           \
  log_fn_(LOG_WARN, domain, __FUNCTION__, args)
#define log_err(domain, args...)                            \
  log_fn_(LOG_ERR, domain, __FUNCTION__, args)

#else /* ! defined(__GNUC__) */

/* Here are the c99 variadic macros, to work with non-GCC compilers */

#define log_debug(domain, args, ...)                                        \
  STMT_BEGIN                                                                \
    if (PREDICT_UNLIKELY(log_global_min_severity_ == LOG_DEBUG))            \
      log_fn_(LOG_DEBUG, domain, __FUNCTION__, args, ##__VA_ARGS__); \
  STMT_END
#define log_info(domain, args,...)                                      \
  log_fn_(LOG_INFO, domain, __FUNCTION__, args, ##__VA_ARGS__)
#define log_notice(domain, args,...)                                    \
  log_fn_(LOG_NOTICE, domain, __FUNCTION__, args, ##__VA_ARGS__)
#define log_warn(domain, args,...)                                      \
  log_fn_(LOG_WARN, domain, __FUNCTION__, args, ##__VA_ARGS__)
#define log_err(domain, args,...)                                       \
  log_fn_(LOG_ERR, domain, __FUNCTION__, args, ##__VA_ARGS__)
/** Log a message at level <b>severity</b>, using a pretty-printed version
 * of the current function name. */
#define log_fn(severity, domain, args,...)                              \
  log_fn_(severity, domain, __FUNCTION__, args, ##__VA_ARGS__)
/** As log_fn, but use <b>ratelim</b> (an instance of ratelim_t) to control
 * the frequency at which messages can appear.
 */
#define log_fn_ratelim(ratelim, severity, domain, args,...)      \
  log_fn_ratelim_(ratelim, severity, domain, __FUNCTION__, \
                  args, ##__VA_ARGS__)
#endif

#ifdef LOG_PRIVATE
MOCK_DECL(STATIC void, logv, (int severity, log_domain_mask_t domain,
    const char *funcname, const char *suffix, const char *format,
    va_list ap) CHECK_PRINTF(5,0));
#endif

# define TOR_TORLOG_H
#endif

