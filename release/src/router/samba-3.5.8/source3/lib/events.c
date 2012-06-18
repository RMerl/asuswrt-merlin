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
#include <tevent_internal.h>

void event_fd_set_writeable(struct tevent_fd *fde)
{
	TEVENT_FD_WRITEABLE(fde);
}

void event_fd_set_not_writeable(struct tevent_fd *fde)
{
	TEVENT_FD_NOT_WRITEABLE(fde);
}

void event_fd_set_readable(struct tevent_fd *fde)
{
	TEVENT_FD_READABLE(fde);
}

void event_fd_set_not_readable(struct tevent_fd *fde)
{
	TEVENT_FD_NOT_READABLE(fde);
}

/*
 * Return if there's something in the queue
 */

bool event_add_to_select_args(struct tevent_context *ev,
			      const struct timeval *now,
			      fd_set *read_fds, fd_set *write_fds,
			      struct timeval *timeout, int *maxfd)
{
	struct tevent_fd *fde;
	struct timeval diff;
	bool ret = false;

	for (fde = ev->fd_events; fde; fde = fde->next) {
		if (fde->fd < 0 || fde->fd >= FD_SETSIZE) {
			/* We ignore here, as it shouldn't be
			   possible to add an invalid fde->fd
			   but we don't want FD_SET to see an
			   invalid fd. */
			continue;
		}

		if (fde->flags & EVENT_FD_READ) {
			FD_SET(fde->fd, read_fds);
			ret = true;
		}
		if (fde->flags & EVENT_FD_WRITE) {
			FD_SET(fde->fd, write_fds);
			ret = true;
		}

		if ((fde->flags & (EVENT_FD_READ|EVENT_FD_WRITE))
		    && (fde->fd > *maxfd)) {
			*maxfd = fde->fd;
		}
	}

	if (ev->immediate_events != NULL) {
		*timeout = timeval_zero();
		return true;
	}

	if (ev->timer_events == NULL) {
		return ret;
	}

	diff = timeval_until(now, &ev->timer_events->next_event);
	*timeout = timeval_min(timeout, &diff);

	return true;
}

bool run_events(struct tevent_context *ev,
		int selrtn, fd_set *read_fds, fd_set *write_fds)
{
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

	if (selrtn <= 0) {
		/*
		 * No fd ready
		 */
		return false;
	}

	for (fde = ev->fd_events; fde; fde = fde->next) {
		uint16 flags = 0;

		if (FD_ISSET(fde->fd, read_fds)) flags |= EVENT_FD_READ;
		if (FD_ISSET(fde->fd, write_fds)) flags |= EVENT_FD_WRITE;

		if (flags & fde->flags) {
			DLIST_DEMOTE(ev->fd_events, fde, struct tevent_fd *);
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
	struct timeval now, to;
	fd_set r_fds, w_fds;
	int maxfd = 0;
	int ret;

	FD_ZERO(&r_fds);
	FD_ZERO(&w_fds);

	to.tv_sec = 9999;	/* Max timeout */
	to.tv_usec = 0;

	if (run_events(ev, 0, NULL, NULL)) {
		return 0;
	}

	GetTimeOfDay(&now);

	if (!event_add_to_select_args(ev, &now, &r_fds, &w_fds, &to, &maxfd)) {
		return -1;
	}

	ret = sys_select(maxfd+1, &r_fds, &w_fds, NULL, &to);

	if (ret == -1 && errno != EINTR) {
		tevent_debug(ev, TEVENT_DEBUG_FATAL,
			     "sys_select() failed: %d:%s\n",
			     errno, strerror(errno));
		return -1;
	}

	run_events(ev, ret, &r_fds, &w_fds);
	return 0;
}

void event_context_reinit(struct tevent_context *ev)
{
	tevent_common_context_destructor(ev);
	return;
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

