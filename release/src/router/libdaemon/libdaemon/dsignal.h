#ifndef foodaemonsignalhfoo
#define foodaemonsignalhfoo

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

#ifdef __cplusplus
extern "C" {
#endif

/** \file
 *
 * Contains the API for serializing signals to a pipe for
 * usage with select() or poll().
 *
 * You should register all signals you
 * wish to handle with select() in your main loop with
 * daemon_signal_init() or daemon_signal_install(). After that you
 * should sleep on the file descriptor returned by daemon_signal_fd()
 * and get the next signal recieved with daemon_signal_next(). You
 * should call daemon_signal_done() before exiting.
 */

/** Installs signal handlers for the specified signals
 * @param s, ... The signals to install handlers for. The list should be terminated by 0
 * @return zero on success, nonzero on failure
 */
int daemon_signal_init(int s, ...);

/** Install a  signal handler for the specified signal
 * @param s The signalto install handler for
 * @return zero onsuccess,nonzero on failure
 */
int daemon_signal_install(int s);

/** Free resources of signal handling, should be called before daemon exit
 */
void daemon_signal_done(void);

/** Return the next signal recieved. This function will not
 * block. Instead it returns 0 if no signal is queued.
 * @return The next queued signal if one is queued, zero if none is
 * queued, negative on failure.
 */
int daemon_signal_next(void);

/** Return the file descriptor the daemon should select() on for
 * reading. Whenever the descriptor is ready you should call
 * daemon_signal_next() to get the next signal queued.
 * @return The file descriptor or negative on failure
 */
int daemon_signal_fd(void);

#ifdef __cplusplus
}
#endif

#endif
