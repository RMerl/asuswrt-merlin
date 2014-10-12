/*
   Unix SMB/Netbios implementation.
   Version 3.0
   Samba select/poll implementation
   Copyright (C) Andrew Tridgell 1992-1998

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "includes.h"
#include "system/filesys.h"
#include "system/select.h"
#include "lib/util/select.h"

/* This is here because it allows us to avoid a nasty race in signal handling.
   We need to guarantee that when we get a signal we get out of a select immediately
   but doing that involves a race condition. We can avoid the race by getting the
   signal handler to write to a pipe that is in the select/poll list

   This means all Samba signal handlers should call sys_select_signal().
*/

static pid_t initialised;
static int select_pipe[2];
static volatile unsigned pipe_written, pipe_read;

/*******************************************************************
 Call this from all Samba signal handlers if you want to avoid a
 nasty signal race condition.
********************************************************************/

void sys_select_signal(char c)
{
	int saved_errno = errno;

	if (!initialised) return;

	if (pipe_written > pipe_read+256) return;

	if (write(select_pipe[1], &c, 1) == 1) pipe_written++;

	errno = saved_errno;
}

/*
 * sys_poll expects pollfd's to be a talloc'ed array.
 *
 * It expects the talloc_array_length(fds) >= num_fds+1 to give space
 * to the signal pipe.
 */

int sys_poll(struct pollfd *fds, int num_fds, int timeout)
{
	int ret;

	if (talloc_array_length(fds) < num_fds+1) {
		errno = ENOSPC;
		return -1;
	}

	if (initialised != sys_getpid()) {
		if (pipe(select_pipe) == -1)
		{
			int saved_errno = errno;
			DEBUG(0, ("sys_poll: pipe failed (%s)\n",
				strerror(errno)));
			errno = saved_errno;
			return -1;
		}

		/*
		 * These next two lines seem to fix a bug with the Linux
		 * 2.0.x kernel (and probably other UNIXes as well) where
		 * the one byte read below can block even though the
		 * select returned that there is data in the pipe and
		 * the pipe_written variable was incremented. Thanks to
		 * HP for finding this one. JRA.
		 */

		if(set_blocking(select_pipe[0],0)==-1)
			smb_panic("select_pipe[0]: O_NONBLOCK failed");
		if(set_blocking(select_pipe[1],0)==-1)
			smb_panic("select_pipe[1]: O_NONBLOCK failed");

		initialised = sys_getpid();
	}

	ZERO_STRUCT(fds[num_fds]);
	fds[num_fds].fd = select_pipe[0];
	fds[num_fds].events = POLLIN|POLLHUP;

	errno = 0;
	ret = poll(fds, num_fds+1, timeout);

	if ((ret >= 0) && (fds[num_fds].revents & (POLLIN|POLLHUP|POLLERR))) {
		char c;
		int saved_errno = errno;

		if (read(select_pipe[0], &c, 1) == 1) {
			pipe_read += 1;

			/* Mark Weaver <mark-clist@npsl.co.uk> pointed out a critical
			   fix to ensure we don't lose signals. We must always
			   return -1 when the select pipe is set, otherwise if another
			   fd is also ready (so ret == 2) then we used to eat the
			   byte in the pipe and lose the signal. JRA.
			*/
			ret = -1;
#if 0
			/* JRA - we can use this to debug the signal messaging... */
			DEBUG(0,("select got %u signal\n", (unsigned int)c));
#endif
			errno = EINTR;
		} else {
			ret -= 1;
			errno = saved_errno;
		}
	}

	return ret;
}

int sys_poll_intr(struct pollfd *fds, int num_fds, int timeout)
{
	int orig_timeout = timeout;
	struct timespec start;
	int ret;

	clock_gettime_mono(&start);

	while (true) {
		struct timespec now;
		int64_t elapsed;

		ret = poll(fds, num_fds, timeout);
		if (ret != -1) {
			break;
		}
		if (errno != EINTR) {
			break;
		}
		clock_gettime_mono(&now);
		elapsed = nsec_time_diff(&now, &start);
		timeout = (orig_timeout - elapsed) / 1000000;
	};
	return ret;
}
