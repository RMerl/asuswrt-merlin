#include "base.h"
#include "log.h"

#include <sys/types.h>

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <assert.h>


fdevents *fdevent_init(server *srv, size_t maxfds, fdevent_handler_t type) {
	fdevents *ev;

	ev = calloc(1, sizeof(*ev));
	ev->srv = srv;
	ev->fdarray = calloc(maxfds, sizeof(*ev->fdarray));
	ev->maxfds = maxfds;

	switch(type) {
	case FDEVENT_HANDLER_POLL:
		if (0 != fdevent_poll_init(ev)) {
			log_error_write(ev->srv, __FILE__, __LINE__, "S",
				"event-handler poll failed");

			return NULL;
		}
		return ev;
	case FDEVENT_HANDLER_SELECT:
		if (0 != fdevent_select_init(ev)) {
			log_error_write(ev->srv, __FILE__, __LINE__, "S",
				"event-handler select failed");
			return NULL;
		}
		return ev;
	case FDEVENT_HANDLER_LINUX_SYSEPOLL:
		if (0 != fdevent_linux_sysepoll_init(ev)) {
			log_error_write(ev->srv, __FILE__, __LINE__, "S",
				"event-handler linux-sysepoll failed, try to set server.event-handler = \"poll\" or \"select\"");
			return NULL;
		}
		return ev;
	case FDEVENT_HANDLER_SOLARIS_DEVPOLL:
		if (0 != fdevent_solaris_devpoll_init(ev)) {
			log_error_write(ev->srv, __FILE__, __LINE__, "S",
				"event-handler solaris-devpoll failed, try to set server.event-handler = \"poll\" or \"select\"");
			return NULL;
		}
		return ev;
	case FDEVENT_HANDLER_SOLARIS_PORT:
		if (0 != fdevent_solaris_port_init(ev)) {
			log_error_write(ev->srv, __FILE__, __LINE__, "S",
				"event-handler solaris-eventports failed, try to set server.event-handler = \"poll\" or \"select\"");
			return NULL;
		}
		return ev;
	case FDEVENT_HANDLER_FREEBSD_KQUEUE:
		if (0 != fdevent_freebsd_kqueue_init(ev)) {
			log_error_write(ev->srv, __FILE__, __LINE__, "S",
				"event-handler freebsd-kqueue failed, try to set server.event-handler = \"poll\" or \"select\"");
			return NULL;
		}
		return ev;
	case FDEVENT_HANDLER_LIBEV:
		if (0 != fdevent_libev_init(ev)) {
			log_error_write(ev->srv, __FILE__, __LINE__, "S",
				"event-handler libev failed, try to set server.event-handler = \"poll\" or \"select\"");
			return NULL;
		}
		return ev;
	case FDEVENT_HANDLER_UNSET:
		break;
	}

	log_error_write(ev->srv, __FILE__, __LINE__, "S",
		"event-handler is unknown, try to set server.event-handler = \"poll\" or \"select\"");
	return NULL;
}

void fdevent_free(fdevents *ev) {
	size_t i;
	if (!ev) return;

	if (ev->free) ev->free(ev);

	for (i = 0; i < ev->maxfds; i++) {
		if (ev->fdarray[i]) free(ev->fdarray[i]);
	}

	free(ev->fdarray);
	free(ev);
}

int fdevent_reset(fdevents *ev) {
	if (ev->reset) return ev->reset(ev);

	return 0;
}

static fdnode *fdnode_init() {
	fdnode *fdn;

	fdn = calloc(1, sizeof(*fdn));
	fdn->fd = -1;
	return fdn;
}

static void fdnode_free(fdnode *fdn) {
	free(fdn);
}

int fdevent_register(fdevents *ev, int fd, fdevent_handler handler, void *ctx) {
	fdnode *fdn;

	fdn = fdnode_init();
	fdn->handler = handler;
	fdn->fd      = fd;
	fdn->ctx     = ctx;
	fdn->handler_ctx = NULL;
	fdn->events  = 0;

	ev->fdarray[fd] = fdn;

	return 0;
}

int fdevent_unregister(fdevents *ev, int fd) {
	fdnode *fdn;

	if (!ev) return 0;
	fdn = ev->fdarray[fd];

	assert(fdn->events == 0);

	fdnode_free(fdn);

	ev->fdarray[fd] = NULL;

	return 0;
}

int fdevent_event_del(fdevents *ev, int *fde_ndx, int fd) {
	int fde = fde_ndx ? *fde_ndx : -1;

	if (NULL == ev->fdarray[fd]) return 0;

	if (ev->event_del) fde = ev->event_del(ev, fde, fd);
	ev->fdarray[fd]->events = 0;

	if (fde_ndx) *fde_ndx = fde;

	return 0;
}

int fdevent_event_set(fdevents *ev, int *fde_ndx, int fd, int events) {
	int fde = fde_ndx ? *fde_ndx : -1;

	if (ev->event_set) fde = ev->event_set(ev, fde, fd, events);
	ev->fdarray[fd]->events = events;

	if (fde_ndx) *fde_ndx = fde;

	return 0;
}

int fdevent_poll(fdevents *ev, int timeout_ms) {
	if (ev->poll == NULL) SEGFAULT();
	return ev->poll(ev, timeout_ms);
}

int fdevent_event_get_revent(fdevents *ev, size_t ndx) {
	if (ev->event_get_revent == NULL) SEGFAULT();

	return ev->event_get_revent(ev, ndx);
}

int fdevent_event_get_fd(fdevents *ev, size_t ndx) {
	if (ev->event_get_fd == NULL) SEGFAULT();

	return ev->event_get_fd(ev, ndx);
}

fdevent_handler fdevent_get_handler(fdevents *ev, int fd) {
	if (ev->fdarray[fd] == NULL) SEGFAULT();
	if (ev->fdarray[fd]->fd != fd) SEGFAULT();

	return ev->fdarray[fd]->handler;
}

void * fdevent_get_context(fdevents *ev, int fd) {
	if (ev->fdarray[fd] == NULL) SEGFAULT();
	if (ev->fdarray[fd]->fd != fd) SEGFAULT();

	return ev->fdarray[fd]->ctx;
}

int fdevent_fcntl_set(fdevents *ev, int fd) {
#ifdef FD_CLOEXEC
	/* close fd on exec (cgi) */
	fcntl(fd, F_SETFD, FD_CLOEXEC);
#endif
	if ((ev) && (ev->fcntl_set)) return ev->fcntl_set(ev, fd);
#ifdef O_NONBLOCK
	return fcntl(fd, F_SETFL, O_NONBLOCK | O_RDWR);
#else
	return 0;
#endif
}


int fdevent_event_next_fdndx(fdevents *ev, int ndx) {
	if (ev->event_next_fdndx) return ev->event_next_fdndx(ev, ndx);

	return -1;
}

