#include "fdevent.h"
#include "buffer.h"
#include "log.h"

#include <sys/time.h>
#include <sys/types.h>

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <assert.h>

#ifdef USE_SELECT

static int fdevent_select_reset(fdevents *ev) {
	FD_ZERO(&(ev->select_set_read));
	FD_ZERO(&(ev->select_set_write));
	FD_ZERO(&(ev->select_set_error));
	ev->select_max_fd = -1;

	return 0;
}

static int fdevent_select_event_del(fdevents *ev, int fde_ndx, int fd) {
	if (fde_ndx < 0) return -1;

	FD_CLR(fd, &(ev->select_set_read));
	FD_CLR(fd, &(ev->select_set_write));
	FD_CLR(fd, &(ev->select_set_error));

	return -1;
}

static int fdevent_select_event_set(fdevents *ev, int fde_ndx, int fd, int events) {
	UNUSED(fde_ndx);

	/* we should be protected by max-fds, but you never know */
	assert(fd < ((int)FD_SETSIZE));

	if (events & FDEVENT_IN) {
		FD_SET(fd, &(ev->select_set_read));
	} else {
		FD_CLR(fd, &(ev->select_set_read));
	}
	if (events & FDEVENT_OUT) {
		FD_SET(fd, &(ev->select_set_write));
	} else {
		FD_CLR(fd, &(ev->select_set_write));
	}
	FD_SET(fd, &(ev->select_set_error));

	if (fd > ev->select_max_fd) ev->select_max_fd = fd;

	return fd;
}

static int fdevent_select_poll(fdevents *ev, int timeout_ms) {
	struct timeval tv;

	tv.tv_sec =  timeout_ms / 1000;
	tv.tv_usec = (timeout_ms % 1000) * 1000;

	ev->select_read = ev->select_set_read;
	ev->select_write = ev->select_set_write;
	ev->select_error = ev->select_set_error;

	return select(ev->select_max_fd + 1, &(ev->select_read), &(ev->select_write), &(ev->select_error), &tv);
}

static int fdevent_select_event_get_revent(fdevents *ev, size_t ndx) {
	int revents = 0;

	if (FD_ISSET(ndx, &(ev->select_read))) {
		revents |= FDEVENT_IN;
	}
	if (FD_ISSET(ndx, &(ev->select_write))) {
		revents |= FDEVENT_OUT;
	}
	if (FD_ISSET(ndx, &(ev->select_error))) {
		revents |= FDEVENT_ERR;
	}

	return revents;
}

static int fdevent_select_event_get_fd(fdevents *ev, size_t ndx) {
	UNUSED(ev);

	return ndx;
}

static int fdevent_select_event_next_fdndx(fdevents *ev, int ndx) {
	int i;

	i = (ndx < 0) ? 0 : ndx + 1;

	for (; i < ev->select_max_fd + 1; i++) {
		if (FD_ISSET(i, &(ev->select_read))) return i;
		if (FD_ISSET(i, &(ev->select_write))) return i;
		if (FD_ISSET(i, &(ev->select_error))) return i;
	}

	return -1;
}

int fdevent_select_init(fdevents *ev) {
	ev->type = FDEVENT_HANDLER_SELECT;
#define SET(x) \
	ev->x = fdevent_select_##x;

	SET(reset);
	SET(poll);

	SET(event_del);
	SET(event_set);

	SET(event_next_fdndx);
	SET(event_get_fd);
	SET(event_get_revent);

	return 0;
}

#else
int fdevent_select_init(fdevents *ev) {
	UNUSED(ev);

	return -1;
}
#endif
