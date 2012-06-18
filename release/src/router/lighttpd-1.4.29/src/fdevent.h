#ifndef _FDEVENT_H_
#define _FDEVENT_H_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "settings.h"
#include "bitset.h"

#if defined HAVE_STDINT_H
# include <stdint.h>
#elif defined HAVE_INTTYPES_H
# include <inttypes.h>
#endif

#include <sys/types.h>

/* select event-system */

#if defined(HAVE_EPOLL_CTL) && defined(HAVE_SYS_EPOLL_H)
# define USE_LINUX_EPOLL
# include <sys/epoll.h>
#endif

/* MacOS 10.3.x has poll.h under /usr/include/, all other unixes
 * under /usr/include/sys/ */
#if defined HAVE_POLL && (defined(HAVE_SYS_POLL_H) || defined(HAVE_POLL_H))
# define USE_POLL
# ifdef HAVE_POLL_H
#  include <poll.h>
# else
#  include <sys/poll.h>
# endif
#endif

#if defined HAVE_SELECT
# ifdef __WIN32
#  include <winsock2.h>
# endif
# define USE_SELECT
# ifdef HAVE_SYS_SELECT_H
#  include <sys/select.h>
# endif
#endif

#if defined HAVE_SYS_DEVPOLL_H && defined(__sun)
# define USE_SOLARIS_DEVPOLL
# include <sys/devpoll.h>
#endif

#if defined HAVE_PORT_H && defined HAVE_PORT_CREATE && defined(__sun)
# define USE_SOLARIS_PORT
# include <port.h>
#endif

#if defined HAVE_SYS_EVENT_H && defined HAVE_KQUEUE
# define USE_FREEBSD_KQUEUE
# include <sys/event.h>
#endif

#if defined HAVE_LIBEV
# define USE_LIBEV
# include <ev.h>
#endif

struct server;

typedef handler_t (*fdevent_handler)(struct server *srv, void *ctx, int revents);

/* these are the POLL* values from <bits/poll.h> (linux poll)
 */

#define FDEVENT_IN     BV(0)
#define FDEVENT_PRI    BV(1)
#define FDEVENT_OUT    BV(2)
#define FDEVENT_ERR    BV(3)
#define FDEVENT_HUP    BV(4)
#define FDEVENT_NVAL   BV(5)

typedef enum { FD_EVENT_TYPE_UNSET = -1,
		FD_EVENT_TYPE_CONNECTION,
		FD_EVENT_TYPE_FCGI_CONNECTION,
		FD_EVENT_TYPE_DIRWATCH,
		FD_EVENT_TYPE_CGI_CONNECTION
} fd_event_t;

typedef enum { FDEVENT_HANDLER_UNSET,
		FDEVENT_HANDLER_SELECT,
		FDEVENT_HANDLER_POLL,
		FDEVENT_HANDLER_LINUX_SYSEPOLL,
		FDEVENT_HANDLER_SOLARIS_DEVPOLL,
		FDEVENT_HANDLER_SOLARIS_PORT,
		FDEVENT_HANDLER_FREEBSD_KQUEUE,
		FDEVENT_HANDLER_LIBEV
} fdevent_handler_t;


typedef struct _fdnode {
	fdevent_handler handler;
	void *ctx;
	void *handler_ctx;
	int fd;
	int events;
} fdnode;

/**
 * array of unused fd's
 *
 */

typedef struct {
	int *ptr;

	size_t used;
	size_t size;
} buffer_int;

/**
 * fd-event handler for select(), poll() and rt-signals on Linux 2.4
 *
 */
typedef struct fdevents {
	struct server *srv;
	fdevent_handler_t type;

	fdnode **fdarray;
	size_t maxfds;

#ifdef USE_LINUX_EPOLL
	int epoll_fd;
	struct epoll_event *epoll_events;
#endif
#ifdef USE_POLL
	struct pollfd *pollfds;

	size_t size;
	size_t used;

	buffer_int unused;
#endif
#ifdef USE_SELECT
	fd_set select_read;
	fd_set select_write;
	fd_set select_error;

	fd_set select_set_read;
	fd_set select_set_write;
	fd_set select_set_error;

	int select_max_fd;
#endif
#ifdef USE_SOLARIS_DEVPOLL
	int devpoll_fd;
	struct pollfd *devpollfds;
#endif
#ifdef USE_SOLARIS_PORT
	port_event_t *port_events;
#endif
#ifdef USE_FREEBSD_KQUEUE
	int kq_fd;
	struct kevent *kq_results;
#endif
#ifdef USE_SOLARIS_PORT
	int port_fd;
#endif
#ifdef USE_LIBEV
	struct ev_loop *libev_loop;
#endif
	int (*reset)(struct fdevents *ev);
	void (*free)(struct fdevents *ev);

	int (*event_set)(struct fdevents *ev, int fde_ndx, int fd, int events);
	int (*event_del)(struct fdevents *ev, int fde_ndx, int fd);
	int (*event_get_revent)(struct fdevents *ev, size_t ndx);
	int (*event_get_fd)(struct fdevents *ev, size_t ndx);

	int (*event_next_fdndx)(struct fdevents *ev, int ndx);

	int (*poll)(struct fdevents *ev, int timeout_ms);

	int (*fcntl_set)(struct fdevents *ev, int fd);
} fdevents;

fdevents *fdevent_init(struct server *srv, size_t maxfds, fdevent_handler_t type);
int fdevent_reset(fdevents *ev); /* "init" after fork() */
void fdevent_free(fdevents *ev);

int fdevent_event_set(fdevents *ev, int *fde_ndx, int fd, int events); /* events can be FDEVENT_IN, FDEVENT_OUT or FDEVENT_IN | FDEVENT_OUT */
int fdevent_event_del(fdevents *ev, int *fde_ndx, int fd);
int fdevent_event_get_revent(fdevents *ev, size_t ndx);
int fdevent_event_get_fd(fdevents *ev, size_t ndx);
fdevent_handler fdevent_get_handler(fdevents *ev, int fd);
void * fdevent_get_context(fdevents *ev, int fd);

int fdevent_event_next_fdndx(fdevents *ev, int ndx);

int fdevent_poll(fdevents *ev, int timeout_ms);

int fdevent_register(fdevents *ev, int fd, fdevent_handler handler, void *ctx);
int fdevent_unregister(fdevents *ev, int fd);

int fdevent_fcntl_set(fdevents *ev, int fd);

int fdevent_select_init(fdevents *ev);
int fdevent_poll_init(fdevents *ev);
int fdevent_linux_sysepoll_init(fdevents *ev);
int fdevent_solaris_devpoll_init(fdevents *ev);
int fdevent_solaris_port_init(fdevents *ev);
int fdevent_freebsd_kqueue_init(fdevents *ev);
int fdevent_libev_init(fdevents *ev);

#endif
