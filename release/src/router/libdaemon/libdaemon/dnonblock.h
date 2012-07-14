#ifndef foodaemonnonblockhfoo
#define foodaemonnonblockhfoo

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
 * Contains a single function used to change a file descriptor to
 * non-blocking mode using fcntl().
 */

/** Change the passed file descriptor to non-blocking or blocking
 * mode, depending on b.
 * @param fd The file descriptor to manipulation
 * @param b TRUE if non-blocking mode should be enabled, FALSE if it
 * should be disabled
 * @return Zero on success, nonzero on failure.
 */
int daemon_nonblock(int fd, int b);

#ifdef __cplusplus
}
#endif

#endif
