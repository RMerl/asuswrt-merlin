#ifndef foodaemonloghfoo
#define foodaemonloghfoo

/***
  This file is part of libdaemon.

  Copyright 2003-2008 Lennart Poettering

  libdaemon is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as
  published by the Free Software Foundation, either version 2.1 of the
  License, or (at your option) any later version.

  libdaemon is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with libdaemon. If not, see
  <http://www.gnu.org/licenses/>.
***/

#include <syslog.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/** \file
 *
 * Contains a robust API for logging messages
 */

/** Specifies where to send the log messages to. The global variable daemon_log_use takes values of this type.
 */
enum daemon_log_flags {
    DAEMON_LOG_SYSLOG = 1,   /**< Log messages are written to syslog */
    DAEMON_LOG_STDERR = 2,   /**< Log messages are written to STDERR */
    DAEMON_LOG_STDOUT = 4,   /**< Log messages are written to STDOUT */
    DAEMON_LOG_AUTO = 8      /**< If this is set a daemon_fork() will
                                  change this to DAEMON_LOG_SYSLOG in
                                  the daemon process. */
};

/** This variable is used to specify the log target(s) to
 * use. Defaults to DAEMON_LOG_STDERR|DAEMON_LOG_AUTO */
extern enum daemon_log_flags daemon_log_use;

/** Specifies the syslog identification, use daemon_ident_from_argv0()
 * to set this to a sensible value or generate your own. */
extern const char* daemon_log_ident;

#if defined(__GNUC__) && ! defined(DAEMON_GCC_PRINTF_ATTR)
#define DAEMON_GCC_PRINTF_ATTR(a,b) __attribute__ ((format (printf, a, b)))
#else
/** A macro for making use of GCCs printf compilation warnings */
#define DAEMON_GCC_PRINTF_ATTR(a,b)
#endif

/** Log a message using printf format strings using the specified syslog priority
 * @param prio The syslog priority (PRIO_xxx constants)
 * @param t,... The text message to log
 */
void daemon_log(int prio, const char* t, ...) DAEMON_GCC_PRINTF_ATTR(2,3);

/** This variable is defined to 1 iff daemon_logv() is supported.
 * @since 0.11
 * @see daemon_logv()
 */
#define DAEMON_LOGV_AVAILABLE 1

/** Same as daemon_log(), but without variadic arguments
 * @since 0.11
 * @see DAEMON_LOGV_AVAILABLE
 */
void daemon_logv(int prio, const char* t, va_list ap);

/** Return a sensible syslog identification for daemon_log_ident
 * generated from argv[0]. This will return a pointer to the file name
 * of argv[0], i.e. strrchr(argv[0], '\')+1
 * @param argv0 argv[0] as passed to main()
 * @return The identification string
 */
char *daemon_ident_from_argv0(char *argv0);

/** This variable is defined to 1 iff daemon_set_verbosity() is available.
 * @since 0.14
 * @see daemon_set_verbosity()
 */
#define DAEMON_SET_VERBOSITY_AVAILABLE 1

/** Setter for the verbosity level of standard output.
 *
 * @param verbosity_prio Minimum priority level for messages to output
 * on standard output/error
 *
 * Allows to decide which messages to output on standard output/error
 * streams. All messages are logged to syslog and this setting does
 * not influence that.
 *
 * The default value is LOG_WARNING.
 *
 * @since 0.14
 * @see DAEMON_SET_VERBOSITY_AVAILABLE
 */
void daemon_set_verbosity(int verbosity_prio);

#ifdef __cplusplus
}
#endif

#endif
