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

#ifdef USE_SOLARIS_PORT

static const int SOLARIS_PORT_POLL_READ       = POLLIN;
static const int SOLARIS_PORT_POLL_WRITE      = POLLOUT;
static const int SOLARIS_PORT_POLL_READ_WRITE = POLLIN & POLLOUT;

static int fdevent_solaris_port_event_del(fdevents *ev, int fde_ndx, int fd) {
	if (fde_ndx < 0) return -1;

	if (0 != port_dissociate(ev->port_fd, PORT_SOURCE_FD, fd)) {
		log_error_write(ev->srv, __FILE__, __LINE__, "SSS",
			"port_dissociate failed: ", strerror(errno), ", dying");

		SEGFAULT();

		return 0;
	}

	return -1;
}

static int fdevent_solaris_port_event_set(fdevents *ev, int fde_ndx, int fd, int events) {
	const int* user_data = NULL;

	if ((events & FDEVENT_IN) && (events & FDEVENT_OUT)) {
		user_data = &SOLARIS_PORT_POLL_READ_WRITE;
	} else if (events & FDEVENT_IN) {
		user_data = &SOLARIS_PORT_POLL_READ;
	} else if (events & FDEVENT_OUT) {
		user_data = &SOLARIS_PORT_POLL_WRITE;
	}

	if (0 != port_associate(ev->port_fd, PORT_SOURCE_FD, fd, *user_data, (void*) user_data)) {
		log_error_write(ev->srv, __FILE__, __LINE__, "SSS",
			"port_associate failed: ", strerror(errno), ", dying");

		SEGFAULT();

		return 0;
	}

	return fd;
}

static int fdevent_solaris_port_event_get_revent(fdevents *ev, size_t ndx) {
	int events = 0, e;

	e = ev->port_events[ndx].portev_events;
	if (e & POLLIN) events |= FDEVENT_IN;
	if (e & POLLOUT) events |= FDEVENT_OUT;
	if (e & POLLERR) events |= FDEVENT_ERR;
	if (e & POLLHUP) events |= FDEVENT_HUP;
	if (e & POLLPRI) events |= FDEVENT_PRI;
	if (e & POLLNVAL) events |= FDEVENT_NVAL;

	return e;
}

static int fdevent_solaris_port_event_get_fd(fdevents *ev, size_t ndx) {
	return ev->port_events[ndx].portev_object;
}

static int fdevent_solaris_port_event_next_fdndx(fdevents *ev, int ndx) {
	size_t i;

	UNUSED(ev);

	i = (ndx < 0) ? 0 : ndx + 1;

	return i;
}

static void fdevent_solaris_port_free(fdevents *ev) {
	close(ev->port_fd);
	free(ev->port_events);
}

/* if there is any error it will return the return values of port_getn, otherwise it will return number of events **/
static int fdevent_solaris_port_poll(fdevents *ev, int timeout_ms) {
	int i = 0;
	int ret;
	unsigned int available_events, wait_for_events = 0;
	const int *user_data;

	struct timespec  timeout;

	timeout.tv_sec  = timeout_ms/1000L;
	timeout.tv_nsec = (timeout_ms % 1000L) * 1000000L;

	/* get the number of file descriptors with events */
	if ((ret = port_getn(ev->port_fd, ev->port_events, 0, &wait_for_events, &timeout)) < 0) return ret;

	/* wait for at least one event */
	if (0 == wait_for_events) wait_for_events = 1;

	available_events = wait_for_events;

	/* get the events of the file descriptors */
	if ((ret = port_getn(ev->port_fd, ev->port_events, ev->maxfds, &available_events, &timeout)) < 0) {
		/* if errno == ETIME and available_event == wait_for_events we didn't get any events */
		/* for other errors we didn't get any events either */
		if (!(errno == ETIME && wait_for_events != available_events)) return ret;
	}

	for (i = 0; i < available_events; ++i) {
		user_data = (const int *) ev->port_events[i].portev_user;

		if ((ret = port_associate(ev->port_fd, PORT_SOURCE_FD, ev->port_events[i].portev_object,
			*user_data, (void*) user_data)) < 0) {
			log_error_write(ev->srv, __FILE__, __LINE__, "SSS",
				"port_associate failed: ", strerror(errno), ", dying");

			SEGFAULT();

			return 0;
		}
	}

	return available_events;
}

int fdevent_solaris_port_init(fdevents *ev) {
	ev->type = FDEVENT_HANDLER_SOLARIS_PORT;
#define SET(x) \
	ev->x = fdevent_solaris_port_##x;

	SET(free);
	SET(poll);

	SET(event_del);
	SET(event_set);

	SET(event_next_fdndx);
	SET(event_get_fd);
	SET(event_get_revent);

	if ((ev->port_fd = port_create()) < 0) {
		log_error_write(ev->srv, __FILE__, __LINE__, "SSS",
			"port_create() failed (", strerror(errno), "), try to set server.event-handler = \"poll\" or \"select\"");

		return -1;
	}

	ev->port_events = malloc(ev->maxfds * sizeof(*ev->port_events));

	return 0;
}

#else
int fdevent_solaris_port_init(fdevents *ev) {
	UNUSED(ev);

	log_error_write(ev->srv, __FILE__, __LINE__, "S",
		"solaris-eventports not supported, try to set server.event-handler = \"poll\" or \"select\"");

	return -1;
}
#endif
