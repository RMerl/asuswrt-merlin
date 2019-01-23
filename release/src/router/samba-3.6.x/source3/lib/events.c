/*
   Unix SMB/CIFS implementation.
   Timed event library.
   Copyright (C) Andrew Tridgell 1992-1998
   Copyright (C) Volker Lendecke 2005

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
#include "lib/tevent/tevent_internal.h"
#include "../lib/util/select.h"
#include "system/select.h"

struct tevent_poll_private {
	/*
	 * Index from file descriptor into the pollfd array
	 */
	int *pollfd_idx;

	/*
	 * Cache for s3_event_loop_once to avoid reallocs
	 */
	struct pollfd *pfds;
};

static struct tevent_poll_private *tevent_get_poll_private(
	struct tevent_context *ev)
{
	struct tevent_poll_private *state;

	state = (struct tevent_poll_private *)ev->additional_data;
	if (state == NULL) {
		state = TALLOC_ZERO_P(ev, struct tevent_poll_private);
		ev->additional_data = (void *)state;
		if (state == NULL) {
			DEBUG(10, ("talloc failed\n"));
		}
	}
	return state;
}

static void count_fds(struct tevent_context *ev,
		      int *pnum_fds, int *pmax_fd)
{
	struct tevent_fd *fde;
	int num_fds = 0;
	int max_fd = 0;

	for (fde = ev->fd_events; fde != NULL; fde = fde->next) {
		if (fde->flags & (EVENT_FD_READ|EVENT_FD_WRITE)) {
			num_fds += 1;
			if (fde->fd > max_fd) {
				max_fd = fde->fd;
			}
		}
	}
	*pnum_fds = num_fds;
	*pmax_fd = max_fd;
}

bool event_add_to_poll_args(struct tevent_context *ev, TALLOC_CTX *mem_ctx,
			    struct pollfd **pfds, int *pnum_pfds,
			    int *ptimeout)
{
	struct tevent_poll_private *state;
	struct tevent_fd *fde;
	int i, num_fds, max_fd, num_pollfds, idx_len;
	struct pollfd *fds;
	struct timeval now, diff;
	int timeout;

	state = tevent_get_poll_private(ev);
	if (state == NULL) {
		return false;
	}
	count_fds(ev, &num_fds, &max_fd);

	idx_len = max_fd+1;

	if (talloc_array_length(state->pollfd_idx) < idx_len) {
		state->pollfd_idx = TALLOC_REALLOC_ARRAY(
			state, state->pollfd_idx, int, idx_len);
		if (state->pollfd_idx == NULL) {
			DEBUG(10, ("talloc_realloc failed\n"));
			return false;
		}
	}

	fds = *pfds;
	num_pollfds = *pnum_pfds;

	/*
	 * The +1 is for the sys_poll calling convention. It expects
	 * an array 1 longer for the signal pipe
	 */

	if (talloc_array_length(fds) < num_pollfds + num_fds + 1) {
		fds = TALLOC_REALLOC_ARRAY(mem_ctx, fds, struct pollfd,
					   num_pollfds + num_fds + 1);
		if (fds == NULL) {
			DEBUG(10, ("talloc_realloc failed\n"));
			return false;
		}
	}

	memset(&fds[num_pollfds], 0, sizeof(struct pollfd) * num_fds);

	/*
	 * This needs tuning. We need to cope with multiple fde's for a file
	 * descriptor. The problem is that we need to re-use pollfd_idx across
	 * calls for efficiency. One way would be a direct bitmask that might
	 * be initialized quicker, but our bitmap_init implementation is
	 * pretty heavy-weight as well.
	 */
	for (i=0; i<idx_len; i++) {
		state->pollfd_idx[i] = -1;
	}

	for (fde = ev->fd_events; fde; fde = fde->next) {
		struct pollfd *pfd;

		if ((fde->flags & (EVENT_FD_READ|EVENT_FD_WRITE)) == 0) {
			continue;
		}

		if (state->pollfd_idx[fde->fd] == -1) {
			/*
			 * We haven't seen this fd yet. Allocate a new pollfd.
			 */
			state->pollfd_idx[fde->fd] = num_pollfds;
			pfd = &fds[num_pollfds];
			num_pollfds += 1;
		} else {
			/*
			 * We have already seen this fd. OR in the flags.
			 */
			pfd = &fds[state->pollfd_idx[fde->fd]];
		}

		pfd->fd = fde->fd;

		if (fde->flags & EVENT_FD_READ) {
			pfd->events |= (POLLIN|POLLHUP);
		}
		if (fde->flags & EVENT_FD_WRITE) {
			pfd->events |= POLLOUT;
		}
	}
	*pfds = fds;
	*pnum_pfds = num_pollfds;

	if (ev->immediate_events != NULL) {
		*ptimeout = 0;
		return true;
	}
	if (ev->timer_events == NULL) {
		*ptimeout = MIN(*ptimeout, INT_MAX);
		return true;
	}

	now = timeval_current();
	diff = timeval_until(&now, &ev->timer_events->next_event);
	timeout = timeval_to_msec(diff);

	if (timeout < *ptimeout) {
		*ptimeout = timeout;
	}

	return true;
}

bool run_events_poll(struct tevent_context *ev, int pollrtn,
		     struct pollfd *pfds, int num_pfds)
{
	struct tevent_poll_private *state;
	int *pollfd_idx;
	struct tevent_fd *fde;
	struct timeval now;

	if (ev->signal_events &&
	    tevent_common_check_signal(ev)) {
		return true;
	}

	if (ev->immediate_events &&
	    tevent_common_loop_immediate(ev)) {
		return true;
	}

	GetTimeOfDay(&now);

	if ((ev->timer_events != NULL)
	    && (timeval_compare(&now, &ev->timer_events->next_event) >= 0)) {
		/* this older events system did not auto-free timed
		   events on running them, and had a race condition
		   where the event could be called twice if the
		   talloc_free of the te happened after the callback
		   made a call which invoked the event loop. To avoid
		   this while still allowing old code which frees the
		   te, we need to create a temporary context which
		   will be used to ensure the te is freed. We also
		   remove the te from the timed event list before we
		   call the handler, to ensure we can't loop */

		struct tevent_timer *te = ev->timer_events;
		TALLOC_CTX *tmp_ctx = talloc_new(ev);

		DEBUG(10, ("Running timed event \"%s\" %p\n",
			   ev->timer_events->handler_name, ev->timer_events));

		DLIST_REMOVE(ev->timer_events, te);
		talloc_steal(tmp_ctx, te);

		te->handler(ev, te, now, te->private_data);

		talloc_free(tmp_ctx);
		return true;
	}

	if (pollrtn <= 0) {
		/*
		 * No fd ready
		 */
		return false;
	}

	state = (struct tevent_poll_private *)ev->additional_data;
	pollfd_idx = state->pollfd_idx;

	for (fde = ev->fd_events; fde; fde = fde->next) {
		struct pollfd *pfd;
		uint16 flags = 0;

		if ((fde->flags & (EVENT_FD_READ|EVENT_FD_WRITE)) == 0) {
			continue;
		}

		if (pollfd_idx[fde->fd] >= num_pfds) {
			DEBUG(1, ("internal error: pollfd_idx[fde->fd] (%d) "
				  ">= num_pfds (%d)\n", pollfd_idx[fde->fd],
				  num_pfds));
			return false;
		}
		pfd = &pfds[pollfd_idx[fde->fd]];

		if (pfd->fd != fde->fd) {
			DEBUG(1, ("internal error: pfd->fd (%d) "
				  "!= fde->fd (%d)\n", pollfd_idx[fde->fd],
                                  num_pfds));
			return false;
		}

		if (pfd->revents & (POLLHUP|POLLERR)) {
			/* If we only wait for EVENT_FD_WRITE, we
			   should not tell the event handler about it,
			   and remove the writable flag, as we only
			   report errors when waiting for read events
			   to match the select behavior. */
			if (!(fde->flags & EVENT_FD_READ)) {
				EVENT_FD_NOT_WRITEABLE(fde);
				continue;
			}
			flags |= EVENT_FD_READ;
		}

		if (pfd->revents & POLLIN) {
			flags |= EVENT_FD_READ;
		}
		if (pfd->revents & POLLOUT) {
			flags |= EVENT_FD_WRITE;
		}
		if (flags & fde->flags) {
			DLIST_DEMOTE(ev->fd_events, fde, struct tevent_fd);
			fde->handler(ev, fde, flags, fde->private_data);
			return true;
		}
	}

	return false;
}

struct timeval *get_timed_events_timeout(struct tevent_context *ev,
					 struct timeval *to_ret)
{
	struct timeval now;

	if ((ev->timer_events == NULL) && (ev->immediate_events == NULL)) {
		return NULL;
	}
	if (ev->immediate_events != NULL) {
		*to_ret = timeval_zero();
		return to_ret;
	}

	now = timeval_current();
	*to_ret = timeval_until(&now, &ev->timer_events->next_event);

	DEBUG(10, ("timed_events_timeout: %d/%d\n", (int)to_ret->tv_sec,
		(int)to_ret->tv_usec));

	return to_ret;
}

static int s3_event_loop_once(struct tevent_context *ev, const char *location)
{
	struct tevent_poll_private *state;
	int timeout;
	int num_pfds;
	int ret;

	timeout = INT_MAX;

	state = tevent_get_poll_private(ev);
	if (state == NULL) {
		errno = ENOMEM;
		return -1;
	}

	if (run_events_poll(ev, 0, NULL, 0)) {
		return 0;
	}

	num_pfds = 0;
	if (!event_add_to_poll_args(ev, state,
				    &state->pfds, &num_pfds, &timeout)) {
		return -1;
	}

	ret = sys_poll(state->pfds, num_pfds, timeout);
	if (ret == -1 && errno != EINTR) {
		tevent_debug(ev, TEVENT_DEBUG_FATAL,
			     "poll() failed: %d:%s\n",
			     errno, strerror(errno));
		return -1;
	}

	run_events_poll(ev, ret, state->pfds, num_pfds);
	return 0;
}

static int s3_event_context_init(struct tevent_context *ev)
{
	return 0;
}

void dump_event_list(struct tevent_context *ev)
{
	struct tevent_timer *te;
	struct tevent_fd *fe;
	struct timeval evt, now;

	if (!ev) {
		return;
	}

	now = timeval_current();

	DEBUG(10,("dump_event_list:\n"));

	for (te = ev->timer_events; te; te = te->next) {

		evt = timeval_until(&now, &te->next_event);

		DEBUGADD(10,("Timed Event \"%s\" %p handled in %d seconds (at %s)\n",
			   te->handler_name,
			   te,
			   (int)evt.tv_sec,
			   http_timestring(talloc_tos(), te->next_event.tv_sec)));
	}

	for (fe = ev->fd_events; fe; fe = fe->next) {

		DEBUGADD(10,("FD Event %d %p, flags: 0x%04x\n",
			   fe->fd,
			   fe,
			   fe->flags));
	}
}

static const struct tevent_ops s3_event_ops = {
	.context_init		= s3_event_context_init,
	.add_fd			= tevent_common_add_fd,
	.set_fd_close_fn	= tevent_common_fd_set_close_fn,
	.get_fd_flags		= tevent_common_fd_get_flags,
	.set_fd_flags		= tevent_common_fd_set_flags,
	.add_timer		= tevent_common_add_timer,
	.schedule_immediate	= tevent_common_schedule_immediate,
	.add_signal		= tevent_common_add_signal,
	.loop_once		= s3_event_loop_once,
	.loop_wait		= tevent_common_loop_wait,
};

static bool s3_tevent_init(void)
{
	static bool initialized;
	if (initialized) {
		return true;
	}
	initialized = tevent_register_backend("s3", &s3_event_ops);
	tevent_set_default_backend("s3");
	return initialized;
}

/*
  this is used to catch debug messages from events
*/
static void s3_event_debug(void *context, enum tevent_debug_level level,
			   const char *fmt, va_list ap)  PRINTF_ATTRIBUTE(3,0);

static void s3_event_debug(void *context, enum tevent_debug_level level,
			   const char *fmt, va_list ap)
{
	int samba_level = -1;
	char *s = NULL;
	switch (level) {
	case TEVENT_DEBUG_FATAL:
		samba_level = 0;
		break;
	case TEVENT_DEBUG_ERROR:
		samba_level = 1;
		break;
	case TEVENT_DEBUG_WARNING:
		samba_level = 2;
		break;
	case TEVENT_DEBUG_TRACE:
		samba_level = 11;
		break;

	};
	if (vasprintf(&s, fmt, ap) == -1) {
		return;
	}
	DEBUG(samba_level, ("s3_event: %s", s));
	free(s);
}

struct tevent_context *s3_tevent_context_init(TALLOC_CTX *mem_ctx)
{
	struct tevent_context *ev;

	s3_tevent_init();

	ev = tevent_context_init_byname(mem_ctx, "s3");
	if (ev) {
		tevent_set_debug(ev, s3_event_debug, NULL);
	}

	return ev;
}

