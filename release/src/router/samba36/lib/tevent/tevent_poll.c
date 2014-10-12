/*
   Unix SMB/CIFS implementation.
   main select loop and event handling
   Copyright (C) Andrew Tridgell	2003-2005
   Copyright (C) Stefan Metzmacher	2005-2009

     ** NOTE! The following LGPL license applies to the tevent
     ** library. This does NOT imply that all of Samba is released
     ** under the LGPL

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/

#include "replace.h"
#include "system/filesys.h"
#include "system/select.h"
#include "tevent.h"
#include "tevent_util.h"
#include "tevent_internal.h"

struct poll_event_context {
	/*
	 * These two arrays are maintained together.
	 */
	struct pollfd *fds;
	struct tevent_fd **fd_events;
	uint64_t num_fds;

	/* information for exiting from the event loop */
	int exit_code;
};

/*
  create a select_event_context structure.
*/
static int poll_event_context_init(struct tevent_context *ev)
{
	struct poll_event_context *poll_ev;

	poll_ev = talloc_zero(ev, struct poll_event_context);
	if (poll_ev == NULL) {
		return -1;
	}
	ev->additional_data = poll_ev;
	return 0;
}

/*
  destroy an fd_event
*/
static int poll_event_fd_destructor(struct tevent_fd *fde)
{
	struct tevent_context *ev = fde->event_ctx;
	struct poll_event_context *poll_ev = NULL;
	struct tevent_fd *moved_fde;
	uint64_t del_idx = fde->additional_flags;

	if (ev == NULL) {
		goto done;
	}

	poll_ev = talloc_get_type_abort(
		ev->additional_data, struct poll_event_context);

	moved_fde = poll_ev->fd_events[poll_ev->num_fds-1];
	poll_ev->fd_events[del_idx] = moved_fde;
	poll_ev->fds[del_idx] = poll_ev->fds[poll_ev->num_fds-1];
	moved_fde->additional_flags = del_idx;

	poll_ev->num_fds -= 1;
done:
	return tevent_common_fd_destructor(fde);
}

/*
  add a fd based event
  return NULL on failure (memory allocation error)
*/
static struct tevent_fd *poll_event_add_fd(struct tevent_context *ev,
					   TALLOC_CTX *mem_ctx,
					   int fd, uint16_t flags,
					   tevent_fd_handler_t handler,
					   void *private_data,
					   const char *handler_name,
					   const char *location)
{
	struct poll_event_context *poll_ev = talloc_get_type_abort(
		ev->additional_data, struct poll_event_context);
	struct pollfd *pfd;
	struct tevent_fd *fde;

	fde = tevent_common_add_fd(ev, mem_ctx, fd, flags,
				   handler, private_data,
				   handler_name, location);
	if (fde == NULL) {
		return NULL;
	}

	/* we allocate 16 slots to avoid a lot of reallocations */
	if (talloc_array_length(poll_ev->fds) == poll_ev->num_fds) {
		struct pollfd *tmp_fds;
		struct tevent_fd **tmp_fd_events;
		tmp_fds = talloc_realloc(
			poll_ev, poll_ev->fds, struct pollfd,
			poll_ev->num_fds + 16);
		if (tmp_fds == NULL) {
			TALLOC_FREE(fde);
			return NULL;
		}
		poll_ev->fds = tmp_fds;

		tmp_fd_events = talloc_realloc(
			poll_ev, poll_ev->fd_events, struct tevent_fd *,
			poll_ev->num_fds + 16);
		if (tmp_fd_events == NULL) {
			TALLOC_FREE(fde);
			return NULL;
		}
		poll_ev->fd_events = tmp_fd_events;
	}

	pfd = &poll_ev->fds[poll_ev->num_fds];

	pfd->fd = fd;

	pfd->events = 0;
	pfd->revents = 0;

	if (flags & TEVENT_FD_READ) {
		pfd->events |= (POLLIN|POLLHUP);
	}
	if (flags & TEVENT_FD_WRITE) {
		pfd->events |= (POLLOUT);
	}

	fde->additional_flags = poll_ev->num_fds;
	poll_ev->fd_events[poll_ev->num_fds] = fde;

	poll_ev->num_fds += 1;

	talloc_set_destructor(fde, poll_event_fd_destructor);

	return fde;
}

/*
  set the fd event flags
*/
static void poll_event_set_fd_flags(struct tevent_fd *fde, uint16_t flags)
{
	struct poll_event_context *poll_ev = talloc_get_type_abort(
		fde->event_ctx->additional_data, struct poll_event_context);
	uint64_t idx = fde->additional_flags;
	uint16_t pollflags = 0;

	if (flags & TEVENT_FD_READ) {
		pollflags |= (POLLIN|POLLHUP);
	}
	if (flags & TEVENT_FD_WRITE) {
		pollflags |= (POLLOUT);
	}

	poll_ev->fds[idx].events = pollflags;

	fde->flags = flags;
}

/*
  event loop handling using select()
*/
static int poll_event_loop_poll(struct tevent_context *ev,
				struct timeval *tvalp)
{
	struct poll_event_context *poll_ev = talloc_get_type_abort(
		ev->additional_data, struct poll_event_context);
	struct tevent_fd *fde;
	int pollrtn;
	int timeout = -1;

	if (ev->signal_events && tevent_common_check_signal(ev)) {
		return 0;
	}

	if (tvalp != NULL) {
		timeout = tvalp->tv_sec * 1000;
		timeout += (tvalp->tv_usec + 999) / 1000;
	}

	pollrtn = poll(poll_ev->fds, poll_ev->num_fds, timeout);

	if (pollrtn == -1 && errno == EINTR && ev->signal_events) {
		tevent_common_check_signal(ev);
		return 0;
	}

	if (pollrtn == -1 && errno == EBADF) {
		/* the socket is dead! this should never
		   happen as the socket should have first been
		   made readable and that should have removed
		   the event, so this must be a bug. This is a
		   fatal error. */
		tevent_debug(ev, TEVENT_DEBUG_FATAL,
			     "ERROR: EBADF on poll_event_loop_once\n");
		poll_ev->exit_code = EBADF;
		return -1;
	}

	if (pollrtn == 0 && tvalp) {
		/* we don't care about a possible delay here */
		tevent_common_loop_timer_delay(ev);
		return 0;
	}

	if (pollrtn > 0) {
		/* at least one file descriptor is ready - check
		   which ones and call the handler, being careful to allow
		   the handler to remove itself when called */
		for (fde = ev->fd_events; fde; fde = fde->next) {
			struct pollfd *pfd;
			uint64_t pfd_idx = fde->additional_flags;
			uint16_t flags = 0;

			pfd = &poll_ev->fds[pfd_idx];

			if (pfd->revents & (POLLHUP|POLLERR)) {
				/* If we only wait for TEVENT_FD_WRITE, we
				   should not tell the event handler about it,
				   and remove the writable flag, as we only
				   report errors when waiting for read events
				   to match the select behavior. */
				if (!(fde->flags & TEVENT_FD_READ)) {
					TEVENT_FD_NOT_WRITEABLE(fde);
					continue;
				}
				flags |= TEVENT_FD_READ;
			}
			if (pfd->revents & POLLIN) {
				flags |= TEVENT_FD_READ;
			}
			if (pfd->revents & POLLOUT) {
				flags |= TEVENT_FD_WRITE;
			}
			if (flags != 0) {
				fde->handler(ev, fde, flags,
					     fde->private_data);
				break;
			}
		}
	}

	return 0;
}

/*
  do a single event loop using the events defined in ev
*/
static int poll_event_loop_once(struct tevent_context *ev,
				const char *location)
{
	struct timeval tval;

	if (ev->signal_events &&
	    tevent_common_check_signal(ev)) {
		return 0;
	}

	if (ev->immediate_events &&
	    tevent_common_loop_immediate(ev)) {
		return 0;
	}

	tval = tevent_common_loop_timer_delay(ev);
	if (tevent_timeval_is_zero(&tval)) {
		return 0;
	}

	return poll_event_loop_poll(ev, &tval);
}

static const struct tevent_ops poll_event_ops = {
	.context_init		= poll_event_context_init,
	.add_fd			= poll_event_add_fd,
	.set_fd_close_fn	= tevent_common_fd_set_close_fn,
	.get_fd_flags		= tevent_common_fd_get_flags,
	.set_fd_flags		= poll_event_set_fd_flags,
	.add_timer		= tevent_common_add_timer,
	.schedule_immediate	= tevent_common_schedule_immediate,
	.add_signal		= tevent_common_add_signal,
	.loop_once		= poll_event_loop_once,
	.loop_wait		= tevent_common_loop_wait,
};

_PRIVATE_ bool tevent_poll_init(void)
{
	return tevent_register_backend("poll", &poll_event_ops);
}
