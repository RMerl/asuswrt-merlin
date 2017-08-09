/*
   Unix SMB/CIFS implementation.
   Connect avahi to lib/tevents
   Copyright (C) Volker Lendecke 2009

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

#include <avahi-common/watch.h>

struct avahi_poll_context {
	struct tevent_context *ev;
	AvahiWatch **watches;
	AvahiTimeout **timeouts;
};

struct AvahiWatch {
	struct avahi_poll_context *ctx;
	struct tevent_fd *fde;
	int fd;
	AvahiWatchEvent latest_event;
	AvahiWatchCallback callback;
	void *userdata;
};

struct AvahiTimeout {
	struct avahi_poll_context *ctx;
	struct tevent_timer *te;
	AvahiTimeoutCallback callback;
	void *userdata;
};

static uint16_t avahi_flags_map_to_tevent(AvahiWatchEvent event)
{
	return ((event & AVAHI_WATCH_IN) ? TEVENT_FD_READ : 0)
		| ((event & AVAHI_WATCH_OUT) ? TEVENT_FD_WRITE : 0);
}

static void avahi_fd_handler(struct tevent_context *ev,
			     struct tevent_fd *fde, uint16_t flags,
			     void *private_data);

static AvahiWatch *avahi_watch_new(const AvahiPoll *api, int fd,
				   AvahiWatchEvent event,
				   AvahiWatchCallback callback,
				   void *userdata)
{
	struct avahi_poll_context *ctx = talloc_get_type_abort(
		api->userdata, struct avahi_poll_context);
	int num_watches = talloc_array_length(ctx->watches);
	AvahiWatch **tmp, *watch_ctx;

	tmp = talloc_realloc(ctx, ctx->watches, AvahiWatch *, num_watches + 1);
	if (tmp == NULL) {
		return NULL;
	}
	ctx->watches = tmp;

	watch_ctx = talloc(tmp, AvahiWatch);
	if (watch_ctx == NULL) {
		goto fail;
	}
	ctx->watches[num_watches] = watch_ctx;

	watch_ctx->ctx = ctx;
	watch_ctx->fde = tevent_add_fd(ctx->ev, watch_ctx, fd,
				       avahi_flags_map_to_tevent(event),
				       avahi_fd_handler, watch_ctx);
	if (watch_ctx->fde == NULL) {
		goto fail;
	}
	watch_ctx->callback = callback;
	watch_ctx->userdata = userdata;
	return watch_ctx;

 fail:
	TALLOC_FREE(watch_ctx);
	ctx->watches = talloc_realloc(ctx, ctx->watches, AvahiWatch *,
				      num_watches);
	return NULL;
}

static void avahi_fd_handler(struct tevent_context *ev,
			     struct tevent_fd *fde, uint16_t flags,
			     void *private_data)
{
	AvahiWatch *watch_ctx = talloc_get_type_abort(private_data, AvahiWatch);

	watch_ctx->latest_event =
		((flags & TEVENT_FD_READ) ? AVAHI_WATCH_IN : 0)
		| ((flags & TEVENT_FD_WRITE) ? AVAHI_WATCH_OUT : 0);

	watch_ctx->callback(watch_ctx, watch_ctx->fd, watch_ctx->latest_event,
			    watch_ctx->userdata);
}

static void avahi_watch_update(AvahiWatch *w, AvahiWatchEvent event)
{
	tevent_fd_set_flags(w->fde, avahi_flags_map_to_tevent(event));
}

static AvahiWatchEvent avahi_watch_get_events(AvahiWatch *w)
{
	return w->latest_event;
}

static void avahi_watch_free(AvahiWatch *w)
{
	int i, num_watches;
	AvahiWatch **watches = w->ctx->watches;
	struct avahi_poll_context *ctx;

	num_watches = talloc_array_length(watches);

	for (i=0; i<num_watches; i++) {
		if (w == watches[i]) {
			break;
		}
	}
	if (i == num_watches) {
		return;
	}
	ctx = w->ctx;
	TALLOC_FREE(w);
	memmove(&watches[i], &watches[i+1],
		sizeof(*watches) * (num_watches - i - 1));
	ctx->watches = talloc_realloc(ctx, watches, AvahiWatch *,
				      num_watches - 1);
}

static void avahi_timeout_handler(struct tevent_context *ev,
				  struct tevent_timer *te,
				  struct timeval current_time,
				  void *private_data);

static AvahiTimeout *avahi_timeout_new(const AvahiPoll *api,
				       const struct timeval *tv,
				       AvahiTimeoutCallback callback,
				       void *userdata)
{
	struct avahi_poll_context *ctx = talloc_get_type_abort(
		api->userdata, struct avahi_poll_context);
	int num_timeouts = talloc_array_length(ctx->timeouts);
	AvahiTimeout **tmp, *timeout_ctx;

	tmp = talloc_realloc(ctx, ctx->timeouts, AvahiTimeout *,
			     num_timeouts + 1);
	if (tmp == NULL) {
		return NULL;
	}
	ctx->timeouts = tmp;

	timeout_ctx = talloc(tmp, AvahiTimeout);
	if (timeout_ctx == NULL) {
		goto fail;
	}
	ctx->timeouts[num_timeouts] = timeout_ctx;

	timeout_ctx->ctx = ctx;
	if (tv == NULL) {
		timeout_ctx->te = NULL;
	} else {
		timeout_ctx->te = tevent_add_timer(ctx->ev, timeout_ctx,
						   *tv, avahi_timeout_handler,
						   timeout_ctx);
		if (timeout_ctx->te == NULL) {
			goto fail;
		}
	}
	timeout_ctx->callback = callback;
	timeout_ctx->userdata = userdata;
	return timeout_ctx;

 fail:
	TALLOC_FREE(timeout_ctx);
	ctx->timeouts = talloc_realloc(ctx, ctx->timeouts, AvahiTimeout *,
				       num_timeouts);
	return NULL;
}

static void avahi_timeout_handler(struct tevent_context *ev,
				  struct tevent_timer *te,
				  struct timeval current_time,
				  void *private_data)
{
	AvahiTimeout *timeout_ctx = talloc_get_type_abort(
		private_data, AvahiTimeout);

	TALLOC_FREE(timeout_ctx->te);
	timeout_ctx->callback(timeout_ctx, timeout_ctx->userdata);
}

static void avahi_timeout_update(AvahiTimeout *t, const struct timeval *tv)
{
	TALLOC_FREE(t->te);

	if (tv == NULL) {
		/*
		 * Disable this timer
		 */
		return;
	}

	t->te = tevent_add_timer(t->ctx->ev, t, *tv, avahi_timeout_handler, t);
	/*
	 * No failure mode defined here
	 */
	SMB_ASSERT(t->te != NULL);
}

static void avahi_timeout_free(AvahiTimeout *t)
{
	int i, num_timeouts;
	AvahiTimeout **timeouts = t->ctx->timeouts;
	struct avahi_poll_context *ctx;

	num_timeouts = talloc_array_length(timeouts);

	for (i=0; i<num_timeouts; i++) {
		if (t == timeouts[i]) {
			break;
		}
	}
	if (i == num_timeouts) {
		return;
	}
	ctx = t->ctx;
	TALLOC_FREE(t);
	memmove(&timeouts[i], &timeouts[i+1],
		sizeof(*timeouts) * (num_timeouts - i - 1));
	ctx->timeouts = talloc_realloc(ctx, timeouts, AvahiTimeout *,
				       num_timeouts - 1);
}

struct AvahiPoll *tevent_avahi_poll(TALLOC_CTX *mem_ctx,
				    struct tevent_context *ev)
{
	struct AvahiPoll *result;
	struct avahi_poll_context *ctx;

	result = talloc(mem_ctx, struct AvahiPoll);
	if (result == NULL) {
		return result;
	}
	ctx = talloc_zero(result, struct avahi_poll_context);
	if (ctx == NULL) {
		TALLOC_FREE(result);
		return NULL;
	}
	ctx->ev = ev;

	result->watch_new		= avahi_watch_new;
	result->watch_update		= avahi_watch_update;
	result->watch_get_events	= avahi_watch_get_events;
	result->watch_free		= avahi_watch_free;
	result->timeout_new		= avahi_timeout_new;
	result->timeout_update		= avahi_timeout_update;
	result->timeout_free		= avahi_timeout_free;
	result->userdata		= ctx;

	return result;
}
