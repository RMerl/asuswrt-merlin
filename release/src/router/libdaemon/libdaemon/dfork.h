#ifndef foodaemonforkhfoo
#define foodaemonforkhfoo

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

/** \mainpage libdaemon
 *
 * libdaemon
 *
 * For a brief explanation of libdaemons's purpose, have a look on the
 * README file. Thank you!
 *
 */

/** \example testd.c
 * This is an example for the usage of libdaemon
 */

/** \file
 *
 * Contains an API for doing a daemonizing fork().
 *
 * You may daemonize by calling daemon_fork(), a function similar to
 * the plain fork(). If you want to return a return value of the
 * initialization procedure of the child from the parent, you may use
 * the daemon_retval_xxx() functions.
 */

/** Does a daemonizing fork(). For the new daemon process STDIN,
 * STDOUT, STDERR are connected to /dev/null, the process is a session
 * leader, the current directory is changed to /, the umask is set to
 * 777.
 * @return On success, the PID of the child process is returned in the
 * parent's thread of execution, and a 0 is returned in the child's
 * thread of execution. On failure, -1 will be returned in the
 * parent's context, no child process will be created, and errno will
 * be set appropriately.
 */
pid_t daemon_fork(void);

/** Allocate and initialize resources required by the
 * daemon_retval_xxx() functions. These functions allow the child to
 * send a value to the parent after completing its initialisation.
 * Call this in the parent before forking.
 * @return zero on success, nonzero on failure.
 */
int daemon_retval_init(void);

/** Frees the resources allocated by daemon_retval_init(). This should
 * be called if neither daemon_retval_wait() nor daemon_retval_send()
 * is called in the current process. The resources allocated by
 * daemon_retval_init() should be freed in both parent and daemon
 * process. This may be achieved by using daemon_retval_wait()
 * resp. daemon_retval_send(), or by using daemon_retval_done().
 */
void daemon_retval_done(void);

/** Return the value sent by the child via the daemon_retval_send()
 * function, but wait only the specified number of seconds before
 * timing out and returning a negative number. Should be called just
 * once from the parent process only. A subsequent call to
 * daemon_retval_done() in the parent is ignored.
 *
 * @param timeout Thetimeout in seconds
 * @return The integer passed daemon_retval_send() in the daemon process, or -1 on failure.
 */
int daemon_retval_wait(int timeout);

/** Send the specified integer to the parent process. Do not send -1
 * because this signifies a library error. Should be called just once
 * from the daemon process only. A subsequent call to
 * daemon_retval_done() in the daemon is ignored.  @param s The
 * integer to pass to daemon_retval_wait() in the parent process
 * @return Zero on success, nonzero on failure.
 */
int daemon_retval_send(int s);

/** This variable is defined to 1 iff daemon_close_all() and
 * daemon_close_allv() are supported.
 * @since 0.11
 * @see daemon_close_all(), daemon_close_allv() */
#define DAEMON_CLOSE_ALL_AVAILABLE 1

/** Close all file descriptors except those passed. List needs to be
 * terminated by -1. FDs 0, 1, 2 will be kept open anyway.
 * @since 0.11
 * @see DAEMON_CLOSE_ALL_AVAILABLE */
int daemon_close_all(int except_fd, ...);

/** Same as daemon_close_all but takes an array of fds, terminated by
 * -1
 * @since 0.11
 * @see DAEMON_CLOSE_ALL_AVAILABLE */
int daemon_close_allv(const int except_fds[]);

/** This variable is defined to 1 iff daemon_unblock_sigs() and
 * daemon_unblock_sigsv() are supported.
 * @since 0.13
 * @see daemon_unblock_sigs(), daemon_unblock_sigsv()*/
#define DAEMON_UNBLOCK_SIGS_AVAILABLE 1

/** Unblock all signals except those passed. List needs to be
 * terminated by -1.
 * @since 0.13
 * @see DAEMON_UNBLOCK_SIGS_AVAILABLE */
int daemon_unblock_sigs(int except, ...);

/** Same as daemon_unblock_sigs() but takes an array of signals,
 * terminated by -1
 * @since 0.13
 * @see DAEMON_UNBLOCK_SIGS_AVAILABLE */
int daemon_unblock_sigsv(const int except[]);

/** This variable is defined to 1 iff daemon_reset_sigs() and
 * daemon_reset_sigsv() are supported.
 * @since 0.13
 * @see daemon_reset_sigs(), daemon_reset_sigsv() */
#define DAEMON_RESET_SIGS_AVAILABLE 1

/** Reset all signal handlers except those passed. List needs to be
 * terminated by -1.
 * @since 0.13
 * @see DAEMON_RESET_SIGS_AVAILABLE */
int daemon_reset_sigs(int except, ...);

/** Same as daemon_reset_sigs() but takes an array of signals,
 * terminated by -1
 * @since 0.13
 * @see DAEMON_RESET_SIGS_AVAILABLE */
int daemon_reset_sigsv(const int except[]);

#ifdef __cplusplus
}
#endif

#endif
