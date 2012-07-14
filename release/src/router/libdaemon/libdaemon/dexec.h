#ifndef foodexechfoo
#define foodexechfoo

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

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/** \file
 *
 * Contains a robust API for running sub processes with STDOUT and
 * STDERR redirected to syslog
 */

/** This variable is defined to 1 iff daemon_exec() is supported.
 * @since 0.4
 * @see daemon_exec() */
#define DAEMON_EXEC_AVAILABLE 1

#if defined(__GNUC__) && ! defined(DAEMON_GCC_SENTINEL)
#define DAEMON_GCC_SENTINEL __attribute__ ((sentinel))
#else
/** A macro for making use of GCCs printf compilation warnings */
#define DAEMON_GCC_SENTINEL
#endif

/** Run the specified executable with the specified arguments in the
 * specified directory and return the return value of the program in
 * the specified pointer. The calling process is blocked until the
 * child finishes and all child output (either STDOUT or STDIN) has
 * been written to syslog. Running this function requires that
 * daemon_signal() has been called with SIGCHLD as argument.
 *
 * @param dir Working directory for the process.
 * @param ret A pointer to an integer to write the return value of the program to.
 * @param prog The path to the executable
 * @param ... The arguments to be passed to the program, followed by a (char *) NULL
 * @return Nonzero on failure, zero on success
 * @since 0.4
 * @see DAEMON_EXEC_AVAILABLE
 */
int daemon_exec(const char *dir, int *ret, const char *prog, ...) DAEMON_GCC_SENTINEL;

/** This variable is defined to 1 iff daemon_execv() is supported.
 * @since 0.11
 * @see daemon_execv() */
#define DAEMON_EXECV_AVAILABLE 1

/** The same as daemon_exec, but without variadic arguments
 * @since 0.11
 * @see DAEMON_EXECV_AVAILABLE */
int daemon_execv(const char *dir, int *ret, const char *prog, va_list ap);

#ifdef __cplusplus
}
#endif

#endif
