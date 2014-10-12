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

/*
  This is SAMBA's default event loop code

  - we try to use epoll if configure detected support for it
    otherwise we use select()
  - if epoll is broken on the system or the kernel doesn't support it
    at runtime we fallback to select()
*/

#include "replace.h"
#include "system/filesys.h"
#include "system/select.h"
#include "tevent.h"
#include "tevent_util.h"
#include "tevent_internal.h"

struct std_event_context {
	/* a pointer back to the generic event_context */
	struct tevent_context *ev;

	/* the maximum file descriptor number in fd_events */
	int maxfd;

	/* information for exiting from the event loop */
	int exit_code;

	/* when using epoll this is the handle from epoll_create */
	int epoll_fd;

	/* our pid at the time the epoll_fd was created */
	pid_t pid;
};

/* use epoll if it is available */
#if HAVE_EPOLL
/*
  called when a epoll call fails, and we should fallback
  to using select
*/
static void epoll_fallback_to_select(struct std_event_context *std_ev, const char *reason)
{
	tevent_debug(std_ev->ev, TEVENT_DEBUG_FATAL,
		     "%s (%s) - falling back to select()\n",
		     reason, strerror(errno));
	close(std_ev->epoll_fd);
	std_ev->epoll_fd = -1;
	talloc_set_destructor(std_ev, NULL);
}

/*
  map from TEVENT_FD_* to EPOLLIN/EPOLLOUT
*/
static uint32_t epoll_map_flags(uint16_t flags)
{
	uint32_t ret = 0;
	if (flags & TEVENT_FD_READ) ret |= (EPOLLIN | EPOLLERR | EPOLLHUP);
	if (flags & TEVENT_FD_WRITE) ret |= (EPOLLOUT | EPOLLERR | EPOLLHUP);
	return ret;
}

/*
 free the epoll fd
*/
static int epoll_ctx_destructor(struct std_event_context *std_ev)
{
	if (std_ev->epoll_fd != -1) {
		close(std_ev->epoll_fd);
	}
	std_ev->epoll_fd = -1;
	return 0;
}

/*
 init the epoll fd
*/
static void epoll_init_ctx(struct std_event_context *std_ev)
{
	std_ev->epoll_fd = epoll_create(64);
	std_ev->pid = getpid();
	talloc_set_destructor(std_ev, epoll_ctx_destructor);
}

static void epoll_add_event(struct std_event_context *std_ev, struct tevent_fd *fde);

/*
  reopen the epoll handle when our pid changes
  see http://junkcode.samba.org/ftp/unpacked/junkcode/epoll_fork.c for an 
  demonstration of why this is needed
 */
static void epoll_check_reopen(struct std_event_context *std_ev)
{
	struct tevent_fd *fde;

	if (std_ev->pid == getpid()) {
		return;
	}

	close(std_ev->epoll_fd);
	std_ev->epoll_fd = epoll_create(64);
	if (std_ev->epoll_fd == -1) {
		tevent_debug(std_ev->ev, TEVENT_DEBUG_FATAL,
			     "Failed to recreate epoll handle after fork\n");
		return;
	}
	std_ev->pid = getpid();
	for (fde=std_ev->ev->fd_events;fde;fde=fde->next) {
		epoll_add_event(std_ev, fde);
	}
}

#define EPOLL_ADDITIONAL_FD_FLAG_HAS_EVENT	(1<<0)
#define EPOLL_ADDITIONAL_FD_FLAG_REPORT_ERROR	(1<<1)
#define EPOLL_ADDITIONAL_FD_FLAG_GOT_ERROR	(1<<2)

/*
 add the epoll event to the given fd_event
*/
static void epoll_add_event(struct std_event_context *std_ev, struct tevent_fd *fde)
{
	struct epoll_event event;
	if (std_ev->epoll_fd == -1) return;

	fde->additional_flags &= ~EPOLL_ADDITIONAL_FD_FLAG_REPORT_ERROR;

	/* if we don't want events yet, don't add an epoll_event */
	if (fde->flags == 0) return;

	ZERO_STRUCT(event);
	event.events = epoll_map_flags(fde->flags);
	event.data.ptr = fde;
	if (epoll_ctl(std_ev->epoll_fd, EPOLL_CTL_ADD, fde->fd, &event) != 0) {
		epoll_fallback_to_select(std_ev, "EPOLL_CTL_ADD failed");
	}
	fde->additional_flags |= EPOLL_ADDITIONAL_FD_FLAG_HAS_EVENT;

	/* only if we want to read we want to tell the event handler about errors */
	if (fde->flags & TEVENT_FD_READ) {
		fde->additional_flags |= EPOLL_ADDITIONAL_FD_FLAG_REPORT_ERROR;
	}
}

/*
 delete the epoll event for given fd_event
*/
static void epoll_del_event(struct std_event_context *std_ev, struct tevent_fd *fde)
{
	struct epoll_event event;
	if (std_ev->epoll_fd == -1) return;

	fde->additional_flags &= ~EPOLL_ADDITIONAL_FD_FLAG_REPORT_ERROR;

	/* if there's no epoll_event, we don't need to delete it */
	if (!(fde->additional_flags & EPOLL_ADDITIONAL_FD_FLAG_HAS_EVENT)) return;

	ZERO_STRUCT(event);
	event.events = epoll_map_flags(fde->flags);
	event.data.ptr = fde;
	epoll_ctl(std_ev->epoll_fd, EPOLL_CTL_DEL, fde->fd, &event);
	fde->additional_flags &= ~EPOLL_ADDITIONAL_FD_FLAG_HAS_EVENT;
}

/*
 change the epoll event to the given fd_event
*/
static void epoll_mod_event(struct std_event_context *std_ev, struct tevent_fd *fde)
{
	struct epoll_event event;
	if (std_ev->epoll_fd == -1) return;

	fde->additional_flags &= ~EPOLL_ADDITIONAL_FD_FLAG_REPORT_ERROR;

	ZERO_STRUCT(event);
	event.events = epoll_map_flags(fde->flags);
	event.data.ptr = fde;
	if (epoll_ctl(std_ev->epoll_fd, EPOLL_CTL_MOD, fde->fd, &event) != 0) {
		epoll_fallback_to_select(std_ev, "EPOLL_CTL_MOD failed");
	}

	/* only if we want to read we want to tell the event handler about errors */
	if (fde->flags & TEVENT_FD_READ) {
		fde->additional_flags |= EPOLL_ADDITIONAL_FD_FLAG_REPORT_ERROR;
	}
}

static void epoll_change_event(struct std_event_context *std_ev, struct tevent_fd *fde)
{
	bool got_error = (fde->additional_flags & EPOLL_ADDITIONAL_FD_FLAG_GOT_ERROR);
	bool want_read = (fde->flags & TEVENT_FD_READ);
	bool want_write= (fde->flags & TEVENT_FD_WRITE);

	if (std_ev->epoll_fd == -1) return;

	fde->additional_flags &= ~EPOLL_ADDITIONAL_FD_FLAG_REPORT_ERROR;

	/* there's already an event */
	if (fde->additional_flags & EPOLL_ADDITIONAL_FD_FLAG_HAS_EVENT) {
		if (want_read || (want_write && !got_error)) {
			epoll_mod_event(std_ev, fde);
			return;
		}
		/* 
		 * if we want to match the select behavior, we need to remove the epoll_event
		 * when the caller isn't interested in events.
		 *
		 * this is because epoll reports EPOLLERR and EPOLLHUP, even without asking for them
		 */
		epoll_del_event(std_ev, fde);
		return;
	}

	/* there's no epoll_event attached to the fde */
	if (want_read || (want_write && !got_error)) {
		epoll_add_event(std_ev, fde);
		return;
	}
}

/*
  event loop handling using epoll
*/
static int epoll_event_loop(struct std_event_context *std_ev, struct timeval *tvalp)
{
	int ret, i;
#define MAXEVENTS 1
	struct epoll_event events[MAXEVENTS];
	int timeout = -1;

	if (std_ev->epoll_fd == -1) return -1;

	if (tvalp) {
		/* it's better to trigger timed events a bit later than to early */
		timeout = ((tvalp->tv_usec+999) / 1000) + (tvalp->tv_sec*1000);
	}

	if (std_ev->ev->signal_events &&
	    tevent_common_check_signal(std_ev->ev)) {
		return 0;
	}

	ret = epoll_wait(std_ev->epoll_fd, events, MAXEVENTS, timeout);

	if (ret == -1 && errno == EINTR && std_ev->ev->signal_events) {
		if (tevent_common_check_signal(std_ev->ev)) {
			return 0;
		}
	}

	if (ret == -1 && errno != EINTR) {
		epoll_fallback_to_select(std_ev, "epoll_wait() failed");
		return -1;
	}

	if (ret == 0 && tvalp) {
		/* we don't care about a possible delay here */
		tevent_common_loop_timer_delay(std_ev->ev);
		return 0;
	}

	for (i=0;i<ret;i++) {
		struct tevent_fd *fde = talloc_get_type(events[i].data.ptr, 
						       struct tevent_fd);
		uint16_t flags = 0;

		if (fde == NULL) {
			epoll_fallback_to_select(std_ev, "epoll_wait() gave bad data");
			return -1;
		}
		if (events[i].events & (EPOLLHUP|EPOLLERR)) {
			fde->additional_flags |= EPOLL_ADDITIONAL_FD_FLAG_GOT_ERROR;
			/*
			 * if we only wait for TEVENT_FD_WRITE, we should not tell the
			 * event handler about it, and remove the epoll_event,
			 * as we only report errors when waiting for read events,
			 * to match the select() behavior
			 */
			if (!(fde->additional_flags & EPOLL_ADDITIONAL_FD_FLAG_REPORT_ERROR)) {
				epoll_del_event(std_ev, fde);
				continue;
			}
			flags |= TEVENT_FD_READ;
		}
		if (events[i].events & EPOLLIN) flags |= TEVENT_FD_READ;
		if (events[i].events & EPOLLOUT) flags |= TEVENT_FD_WRITE;
		if (flags) {
			fde->handler(std_ev->ev, fde, flags, fde->private_data);
			break;
		}
	}

	return 0;
}
#else
#define epoll_init_ctx(std_ev) 
#define epoll_add_event(std_ev,fde)
#define epoll_del_event(std_ev,fde)
#define epoll_change_event(std_ev,fde)
#define epoll_event_loop(std_ev,tvalp) (-1)
#define epoll_check_reopen(std_ev)
#endif

/*
  create a std_event_context structure.
*/
static int std_event_context_init(struct tevent_context *ev)
{
	struct std_event_context *std_ev;

	std_ev = talloc_zero(ev, struct std_event_context);
	if (!std_ev) return -1;
	std_ev->ev = ev;
	std_ev->epoll_fd = -1;

	epoll_init_ctx(std_ev);

	ev->additional_data = std_ev;
	return 0;
}

/*
  recalculate the maxfd
*/
static void calc_maxfd(struct std_event_context *std_ev)
{
	struct tevent_fd *fde;

	std_ev->maxfd = 0;
	for (fde = std_ev->ev->fd_events; fde; fde = fde->next) {
		if (fde->fd > std_ev->maxfd) {
			std_ev->maxfd = fde->fd;
		}
	}
}


/* to mark the ev->maxfd invalid
 * this means we need to recalculate it
 */
#define EVENT_INVALID_MAXFD (-1)

/*
  destroy an fd_event
*/
static int std_event_fd_destructor(struct tevent_fd *fde)
{
	struct tevent_context *ev = fde->event_ctx;
	struct std_event_context *std_ev = NULL;

	if (ev) {
		std_ev = talloc_get_type(ev->additional_data,
					 struct std_event_context);

		epoll_check_reopen(std_ev);

		if (std_ev->maxfd == fde->fd) {
			std_ev->maxfd = EVENT_INVALID_MAXFD;
		}

		epoll_del_event(std_ev, fde);
	}

	return tevent_common_fd_destructor(fde);
}

/*
  add a fd based event
  return NULL on failure (memory allocation error)
*/
static struct tevent_fd *std_event_add_fd(struct tevent_context *ev, TALLOC_CTX *mem_ctx,
					  int fd, uint16_t flags,
					  tevent_fd_handler_t handler,
					  void *private_data,
					  const char *handler_name,
					  const char *location)
{
	struct std_event_context *std_ev = talloc_get_type(ev->additional_data,
							   struct std_event_context);
	struct tevent_fd *fde;

	epoll_check_reopen(std_ev);

	fde = tevent_common_add_fd(ev, mem_ctx, fd, flags,
				   handler, private_data,
				   handler_name, location);
	if (!fde) return NULL;

	if ((std_ev->maxfd != EVENT_INVALID_MAXFD)
	    && (fde->fd > std_ev->maxfd)) {
		std_ev->maxfd = fde->fd;
	}
	talloc_set_destructor(fde, std_event_fd_destructor);

	epoll_add_event(std_ev, fde);

	return fde;
}

/*
  set the fd event flags
*/
static void std_event_set_fd_flags(struct tevent_fd *fde, uint16_t flags)
{
	struct tevent_context *ev;
	struct std_event_context *std_ev;

	if (fde->flags == flags) return;

	ev = fde->event_ctx;
	std_ev = talloc_get_type(ev->additional_data, struct std_event_context);

	fde->flags = flags;

	epoll_check_reopen(std_ev);

	epoll_change_event(std_ev, fde);
}

/*
  event loop handling using select()
*/
static int std_event_loop_select(struct std_event_context *std_ev, struct timeval *tvalp)
{
	fd_set r_fds, w_fds;
	struct tevent_fd *fde;
	int selrtn;

	/* we maybe need to recalculate the maxfd */
	if (std_ev->maxfd == EVENT_INVALID_MAXFD) {
		calc_maxfd(std_ev);
	}

	FD_ZERO(&r_fds);
	FD_ZERO(&w_fds);

	/* setup any fd events */
	for (fde = std_ev->ev->fd_events; fde; fde = fde->next) {
		if (fde->fd < 0 || fde->fd >= FD_SETSIZE) {
			std_ev->exit_code = EBADF;
			return -1;
		}
		if (fde->flags & TEVENT_FD_READ) {
			FD_SET(fde->fd, &r_fds);
		}
		if (fde->flags & TEVENT_FD_WRITE) {
			FD_SET(fde->fd, &w_fds);
		}
	}

	if (std_ev->ev->signal_events &&
	    tevent_common_check_signal(std_ev->ev)) {
		return 0;
	}

	selrtn = select(std_ev->maxfd+1, &r_fds, &w_fds, NULL, tvalp);

	if (selrtn == -1 && errno == EINTR && 
	    std_ev->ev->signal_events) {
		tevent_common_check_signal(std_ev->ev);
		return 0;
	}

	if (selrtn == -1 && errno == EBADF) {
		/* the socket is dead! this should never
		   happen as the socket should have first been
		   made readable and that should have removed
		   the event, so this must be a bug. This is a
		   fatal error. */
		tevent_debug(std_ev->ev, TEVENT_DEBUG_FATAL,
			     "ERROR: EBADF on std_event_loop_once\n");
		std_ev->exit_code = EBADF;
		return -1;
	}

	if (selrtn == 0 && tvalp) {
		/* we don't care about a possible delay here */
		tevent_common_loop_timer_delay(std_ev->ev);
		return 0;
	}

	if (selrtn > 0) {
		/* at least one file descriptor is ready - check
		   which ones and call the handler, being careful to allow
		   the handler to remove itself when called */
		for (fde = std_ev->ev->fd_events; fde; fde = fde->next) {
			uint16_t flags = 0;

			if (FD_ISSET(fde->fd, &r_fds)) flags |= TEVENT_FD_READ;
			if (FD_ISSET(fde->fd, &w_fds)) flags |= TEVENT_FD_WRITE;
			if (flags & fde->flags) {
				fde->handler(std_ev->ev, fde, flags, fde->private_data);
				break;
			}
		}
	}

	return 0;
}		

/*
  do a single event loop using the events defined in ev 
*/
static int std_event_loop_once(struct tevent_context *ev, const char *location)
{
	struct std_event_context *std_ev = talloc_get_type(ev->additional_data,
		 					   struct std_event_context);
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

	epoll_check_reopen(std_ev);

	if (epoll_event_loop(std_ev, &tval) == 0) {
		return 0;
	}

	return std_event_loop_select(std_ev, &tval);
}

static const struct tevent_ops std_event_ops = {
	.context_init		= std_event_context_init,
	.add_fd			= std_event_add_fd,
	.set_fd_close_fn	= tevent_common_fd_set_close_fn,
	.get_fd_flags		= tevent_common_fd_get_flags,
	.set_fd_flags		= std_event_set_fd_flags,
	.add_timer		= tevent_common_add_timer,
	.schedule_immediate	= tevent_common_schedule_immediate,
	.add_signal		= tevent_common_add_signal,
	.loop_once		= std_event_loop_once,
	.loop_wait		= tevent_common_loop_wait,
};


_PRIVATE_ bool tevent_standard_init(void)
{
	return tevent_register_backend("standard", &std_event_ops);
}

