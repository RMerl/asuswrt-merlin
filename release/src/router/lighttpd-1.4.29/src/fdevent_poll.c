#include "fdevent.h"
#include "buffer.h"
#include "log.h"

#include <sys/types.h>

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>

#ifdef USE_POLL
static void fdevent_poll_free(fdevents *ev) {
	free(ev->pollfds);
	if (ev->unused.ptr) free(ev->unused.ptr);
}

static int fdevent_poll_event_del(fdevents *ev, int fde_ndx, int fd) {
	if (fde_ndx < 0) return -1;

	if ((size_t)fde_ndx >= ev->used) {
		log_error_write(ev->srv, __FILE__, __LINE__, "SdD",
			"del! out of range ", fde_ndx, (int) ev->used);
		SEGFAULT();
	}

	if (ev->pollfds[fde_ndx].fd == fd) {
		size_t k = fde_ndx;

		ev->pollfds[k].fd = -1;
		/* ev->pollfds[k].events = 0; */
		/* ev->pollfds[k].revents = 0; */

		if (ev->unused.size == 0) {
			ev->unused.size = 16;
			ev->unused.ptr = malloc(sizeof(*(ev->unused.ptr)) * ev->unused.size);
		} else if (ev->unused.size == ev->unused.used) {
			ev->unused.size += 16;
			ev->unused.ptr = realloc(ev->unused.ptr, sizeof(*(ev->unused.ptr)) * ev->unused.size);
		}

		ev->unused.ptr[ev->unused.used++] = k;
	} else {
		log_error_write(ev->srv, __FILE__, __LINE__, "SdD",
			"del! ", ev->pollfds[fde_ndx].fd, fd);

		SEGFAULT();
	}

	return -1;
}

#if 0
static int fdevent_poll_event_compress(fdevents *ev) {
	size_t j;

	if (ev->used == 0) return 0;
	if (ev->unused.used != 0) return 0;

	for (j = ev->used - 1; j + 1 > 0 && ev->pollfds[j].fd == -1; j--) ev->used--;

	return 0;
}
#endif

static int fdevent_poll_event_set(fdevents *ev, int fde_ndx, int fd, int events) {
	int pevents = 0;
	if (events & FDEVENT_IN)  pevents |= POLLIN;
	if (events & FDEVENT_OUT) pevents |= POLLOUT;

	/* known index */

	if (fde_ndx != -1) {
		if (ev->pollfds[fde_ndx].fd == fd) {
			ev->pollfds[fde_ndx].events = pevents;

			return fde_ndx;
		}
		log_error_write(ev->srv, __FILE__, __LINE__, "SdD",
			"set: ", fde_ndx, ev->pollfds[fde_ndx].fd);
		SEGFAULT();
	}

	if (ev->unused.used > 0) {
		int k = ev->unused.ptr[--ev->unused.used];

		ev->pollfds[k].fd = fd;
		ev->pollfds[k].events = pevents;

		return k;
	} else {
		if (ev->size == 0) {
			ev->size = 16;
			ev->pollfds = malloc(sizeof(*ev->pollfds) * ev->size);
		} else if (ev->size == ev->used) {
			ev->size += 16;
			ev->pollfds = realloc(ev->pollfds, sizeof(*ev->pollfds) * ev->size);
		}

		ev->pollfds[ev->used].fd = fd;
		ev->pollfds[ev->used].events = pevents;

		return ev->used++;
	}
}

static int fdevent_poll_poll(fdevents *ev, int timeout_ms) {
#if 0
	fdevent_poll_event_compress(ev);
#endif
	return poll(ev->pollfds, ev->used, timeout_ms);
}

static int fdevent_poll_event_get_revent(fdevents *ev, size_t ndx) {
	int r, poll_r;

	if (ndx >= ev->used) {
		log_error_write(ev->srv, __FILE__, __LINE__, "sii",
			"dying because: event: ", (int) ndx, (int) ev->used);

		SEGFAULT();

		return 0;
	}

	if (ev->pollfds[ndx].revents & POLLNVAL) {
		/* should never happen */
		SEGFAULT();
	}

	r = 0;
	poll_r = ev->pollfds[ndx].revents;

	/* map POLL* to FDEVEN_*; they are probably the same, but still. */

	if (poll_r & POLLIN) r |= FDEVENT_IN;
	if (poll_r & POLLOUT) r |= FDEVENT_OUT;
	if (poll_r & POLLERR) r |= FDEVENT_ERR;
	if (poll_r & POLLHUP) r |= FDEVENT_HUP;
	if (poll_r & POLLNVAL) r |= FDEVENT_NVAL;
	if (poll_r & POLLPRI) r |= FDEVENT_PRI;

	return r;
}

static int fdevent_poll_event_get_fd(fdevents *ev, size_t ndx) {
	return ev->pollfds[ndx].fd;
}

static int fdevent_poll_event_next_fdndx(fdevents *ev, int ndx) {
	size_t i;

	i = (ndx < 0) ? 0 : ndx + 1;
	for (; i < ev->used; i++) {
		if (ev->pollfds[i].revents) return i;
	}

	return -1;
}

int fdevent_poll_init(fdevents *ev) {
	ev->type = FDEVENT_HANDLER_POLL;
#define SET(x) \
	ev->x = fdevent_poll_##x;

	SET(free);
	SET(poll);

	SET(event_del);
	SET(event_set);

	SET(event_next_fdndx);
	SET(event_get_fd);
	SET(event_get_revent);

	return 0;
}




#else
int fdevent_poll_init(fdevents *ev) {
	UNUSED(ev);

	log_error_write(srv, __FILE__, __LINE__,
		"s", "poll is not supported, try to set server.event-handler = \"select\"");

	return -1;
}
#endif
