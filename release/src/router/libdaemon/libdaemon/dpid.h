#ifndef foodaemonpidhfoo
#define foodaemonpidhfoo

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

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/** \file
 *
 * Contains an API for manipulating PID files.
 */

/** Prototype of a function for generating the name of a PID file.
 */
typedef const char* (*daemon_pid_file_proc_t)(void);

/** Identification string for the PID file name, only used when
 * daemon_pid_file_proc is set to daemon_pid_file_proc_default(). Use
 * daemon_ident_from_argv0() to generate an identification string from
 * argv[0]
 */
extern const char *daemon_pid_file_ident;

/** A function pointer which is used to generate the name of the PID
 * file to manipulate. Points to daemon_pid_file_proc_default() by
 * default.
 */
extern daemon_pid_file_proc_t daemon_pid_file_proc;

/** A function for creating a pid file name from
 * daemon_pid_file_ident
 * @return The PID file path
 */
const char *daemon_pid_file_proc_default(void);

/** Creates PID pid file for the current process
 * @return zero on success, nonzero on failure
 */
int daemon_pid_file_create(void);

/** Removes the PID file of the current process
 * @return zero on success, nonzero on failure
 */
int daemon_pid_file_remove(void);

/** Returns the PID file of a running daemon, if available
 * @return The PID or negative on failure
 */
pid_t daemon_pid_file_is_running(void);

/** Kills a running daemon, if available
 * @param s The signal to send
 * @return zero on success, nonzero on failure
 */
int daemon_pid_file_kill(int s);

/** This variable is defined to 1 iff daemon_pid_file_kill_wait() is supported.
 * @since 0.3
 * @see daemon_pid_file_kill_wait() */
#define DAEMON_PID_FILE_KILL_WAIT_AVAILABLE 1

/** Similar to daemon_pid_file_kill() but waits until the process
 * died.  This functions is new in libdaemon 0.3. The macro
 * DAEMON_PID_FILE_KILL_WAIT_AVAILABLE is defined iff libdaemon
 * supports this function.
 *
 * @param s The signal to send
 * @param m Seconds to wait at maximum
 * @return zero on success, nonzero on failure (timeout condition is considered a failure)
 * @since 0.3
 * @see DAEMON_PID_FILE_KILL_WAIT_AVAILABLE
 */
int daemon_pid_file_kill_wait(int s, int m);

#ifdef __cplusplus
}
#endif

#endif
