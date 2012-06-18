/***********************************************************************
*
* event_tcp.c -- implementation of event-driven socket I/O.
*
* Copyright (C) 2001 Roaring Penguin Software Inc.
*
* This program may be distributed according to the terms of the GNU
* General Public License, version 2 or (at your option) any later version.
*
* LIC: GPL
*
***********************************************************************/

static char const RCSID[] =
"$Id: event_tcp.c 3323 2011-09-21 18:45:48Z lly.dev $";

#include "event_tcp.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

static void free_state(EventTcpState *state);

typedef struct EventTcpConnectState_t {
    int fd;
    EventHandler *conn;
    EventTcpConnectFunc f;
    void *data;
} EventTcpConnectState;

/**********************************************************************
* %FUNCTION: handle_accept
* %ARGUMENTS:
*  es -- event selector
*  fd -- socket
*  flags -- ignored
*  data -- the accept callback function
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Calls accept; if a connection arrives, calls the accept callback
*  function with connected descriptor
***********************************************************************/
static void
handle_accept(EventSelector *es,
	      int fd,
	      unsigned int flags,
	      void *data)
{
    int conn;
    EventTcpAcceptFunc f;

    EVENT_DEBUG(("tcp_handle_accept(es=%p, fd=%d, flags=%u, data=%p)\n", es, fd, flags, data));
    conn = accept(fd, NULL, NULL);
    if (conn < 0) return;
    f = (EventTcpAcceptFunc) data;

    f(es, conn);
}

/**********************************************************************
* %FUNCTION: handle_connect
* %ARGUMENTS:
*  es -- event selector
*  fd -- socket
*  flags -- ignored
*  data -- the accept callback function
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Calls accept; if a connection arrives, calls the accept callback
*  function with connected descriptor
***********************************************************************/
static void
handle_connect(EventSelector *es,
	      int fd,
	      unsigned int flags,
	      void *data)
{
    int error = 0;
    socklen_t len = sizeof(error);
    EventTcpConnectState *state = (EventTcpConnectState *) data;

    EVENT_DEBUG(("tcp_handle_connect(es=%p, fd=%d, flags=%u, data=%p)\n", es, fd, flags, data));

    /* Cancel writable event */
    Event_DelHandler(es, state->conn);
    state->conn = NULL;

    /* Timeout? */
    if (flags & EVENT_FLAG_TIMEOUT) {
	errno = ETIMEDOUT;
	state->f(es, fd, EVENT_TCP_FLAG_TIMEOUT, state->data);
	free(state);
	return;
    }

    /* Check for pending error */
    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
	state->f(es, fd, EVENT_TCP_FLAG_IOERROR, state->data);
	free(state);
	return;
    }
    if (error) {
	errno = error;
	state->f(es, fd, EVENT_TCP_FLAG_IOERROR, state->data);
	free(state);
	return;
    }

    /* It looks cool! */
    state->f(es, fd, EVENT_TCP_FLAG_COMPLETE, state->data);
    free(state);
}

/**********************************************************************
* %FUNCTION: EventTcp_CreateAcceptor
* %ARGUMENTS:
*  es -- event selector
*  socket -- listening socket
*  f -- function to call when a connection is accepted
*  data -- extra data to pass to f.
* %RETURNS:
*  An event handler on success, NULL on failure.
* %DESCRIPTION:
*  Sets up an accepting socket and calls "f" whenever a new
*  connection arrives.
***********************************************************************/
EventHandler *
EventTcp_CreateAcceptor(EventSelector *es,
			int socket,
			EventTcpAcceptFunc f)
{
    int flags;

    EVENT_DEBUG(("EventTcp_CreateAcceptor(es=%p, socket=%d)\n", es, socket));
    /* Make sure socket is non-blocking */
    flags = fcntl(socket, F_GETFL, 0);
    if (flags == -1) {
	return NULL;
    }
    if (fcntl(socket, F_SETFL, flags | O_NONBLOCK) == -1) {
	return NULL;
    }

    return Event_AddHandler(es, socket, EVENT_FLAG_READABLE,
			    handle_accept, (void *) f);

}

/**********************************************************************
* %FUNCTION: free_state
* %ARGUMENTS:
*  state -- EventTcpState to free
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Frees all state associated with the TcpEvent.
***********************************************************************/
static void
free_state(EventTcpState *state)
{
    if (!state) return;
    EVENT_DEBUG(("tcp_free_state(state=%p)\n", state));
    if (state->buf) free(state->buf);
    if (state->eh) Event_DelHandler(state->es, state->eh);
    free(state);
}

/**********************************************************************
* %FUNCTION: handle_readable
* %ARGUMENTS:
*  es -- event selector
*  fd -- the readable socket
*  flags -- ignored
*  data -- the EventTcpState object
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Continues to fill buffer.  Calls callback when done.
***********************************************************************/
static void
handle_readable(EventSelector *es,
		int fd,
		unsigned int flags,
		void *data)
{
    EventTcpState *state = (EventTcpState *) data;
    int done = state->cur - state->buf;
    int togo = state->len - done;
    int nread = 0;
    int flag;

    EVENT_DEBUG(("tcp_handle_readable(es=%p, fd=%d, flags=%u, data=%p)\n", es, fd, flags, data));

    /* Timed out? */
    if (flags & EVENT_FLAG_TIMEOUT) {
	errno = ETIMEDOUT;
	(state->f)(es, state->socket, state->buf, done, EVENT_TCP_FLAG_TIMEOUT,
		   state->data);
	free_state(state);
	return;
    }
    if (state->delim < 0) {
	/* Not looking for a delimiter */
	/* togo had better not be zero here! */
	nread = read(fd, state->cur, togo);
	if (nread <= 0) {
	    /* Change connection reset to EOF if we have read at least
	       one char */
	    if (nread < 0 && errno == ECONNRESET && done > 0) {
		nread = 0;
	    }
	    flag = (nread) ? EVENT_TCP_FLAG_IOERROR : EVENT_TCP_FLAG_EOF;
	    /* error or EOF */
	    (state->f)(es, state->socket, state->buf, done, flag, state->data);
	    free_state(state);
	    return;
	}
	state->cur += nread;
	done += nread;
	if (done >= state->len) {
	    /* Read enough! */
	    (state->f)(es, state->socket, state->buf, done,
		       EVENT_TCP_FLAG_COMPLETE, state->data);
	    free_state(state);
	    return;
	}
    } else {
	/* Looking for a delimiter */
	while ( (togo > 0) && (nread = read(fd, state->cur, 1)) == 1) {
	    togo--;
	    done++;
	    state->cur++;
	    if (*(state->cur - 1) == state->delim) break;
	}

	if (nread <= 0) {
	    /* Error or EOF -- check for EAGAIN */
	    if (nread < 0 && errno == EAGAIN) return;
	}

	/* Some other error, or EOF, or delimiter, or read enough */
	if (nread < 0) {
	    flag = EVENT_TCP_FLAG_IOERROR;
	} else if (nread == 0) {
	    flag = EVENT_TCP_FLAG_EOF;
	} else {
	    flag = EVENT_TCP_FLAG_COMPLETE;
	}
	(state->f)(es, state->socket, state->buf, done, flag, state->data);
	free_state(state);
	return;
    }
}

/**********************************************************************
* %FUNCTION: handle_writeable
* %ARGUMENTS:
*  es -- event selector
*  fd -- the writeable socket
*  flags -- ignored
*  data -- the EventTcpState object
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Continues to fill buffer.  Calls callback when done.
***********************************************************************/
static void
handle_writeable(EventSelector *es,
		int fd,
		unsigned int flags,
		void *data)
{
    EventTcpState *state = (EventTcpState *) data;
    int done = state->cur - state->buf;
    int togo = state->len - done;
    int n;

    /* Timed out? */
    if (flags & EVENT_FLAG_TIMEOUT) {
	errno = ETIMEDOUT;
	if (state->f) {
	    (state->f)(es, state->socket, state->buf, done, EVENT_TCP_FLAG_TIMEOUT,
		       state->data);
	} else {
	    close(fd);
	}
	free_state(state);
	return;
    }

    /* togo had better not be zero here! */
    n = write(fd, state->cur, togo);

    EVENT_DEBUG(("tcp_handle_writeable(es=%p, fd=%d, flags=%u, data=%p)\n", es, fd, flags, data));
    if (n <= 0) {
	/* error */
	if (state->f) {
	    (state->f)(es, state->socket, state->buf, done,
		       EVENT_TCP_FLAG_IOERROR,
		       state->data);
	} else {
	    close(fd);
	}
	free_state(state);
	return;
    }
    state->cur += n;
    done += n;
    if (done >= state->len) {
	/* Written enough! */
	if (state->f) {
	    (state->f)(es, state->socket, state->buf, done,
		       EVENT_TCP_FLAG_COMPLETE, state->data);
	} else {
	    close(fd);
	}
	free_state(state);
	return;
    }

}

/**********************************************************************
* %FUNCTION: EventTcp_ReadBuf
* %ARGUMENTS:
*  es -- event selector
*  socket -- socket to read from
*  len -- maximum number of bytes to read
*  delim -- delimiter at which to stop reading, or -1 if we should
*           read exactly len bytes
*  f -- function to call on EOF or when all bytes have been read
*  timeout -- if non-zero, timeout in seconds after which we cancel
*             operation.
*  data -- extra data to pass to function f.
* %RETURNS:
*  A new EventTcpState token or NULL on error
* %DESCRIPTION:
*  Sets up a handler to fill a buffer from a socket.
***********************************************************************/
EventTcpState *
EventTcp_ReadBuf(EventSelector *es,
		 int socket,
		 int len,
		 int delim,
		 EventTcpIOFinishedFunc f,
		 int timeout,
		 void *data)
{
    EventTcpState *state;
    int flags;
    struct timeval t;

    EVENT_DEBUG(("EventTcp_ReadBuf(es=%p, socket=%d, len=%d, delim=%d, timeout=%d)\n", es, socket, len, delim, timeout));
    if (len <= 0) return NULL;
    if (socket < 0) return NULL;

    /* Make sure socket is non-blocking */
    flags = fcntl(socket, F_GETFL, 0);
    if (flags == -1) {
	return NULL;
    }
    if (fcntl(socket, F_SETFL, flags | O_NONBLOCK) == -1) {
	return NULL;
    }

    state = malloc(sizeof(EventTcpState));
    if (!state) return NULL;

    memset(state, 0, sizeof(EventTcpState));

    state->socket = socket;

    state->buf = malloc(len);
    if (!state->buf) {
	free_state(state);
	return NULL;
    }

    state->cur = state->buf;
    state->len = len;
    state->f = f;
    state->es = es;

    if (timeout <= 0) {
	t.tv_sec = -1;
	t.tv_usec = -1;
    } else {
	t.tv_sec = timeout;
	t.tv_usec = 0;
    }

    state->eh = Event_AddHandlerWithTimeout(es, socket, EVENT_FLAG_READABLE,
					    t, handle_readable,
					    (void *) state);
    if (!state->eh) {
	free_state(state);
	return NULL;
    }
    state->data = data;
    state->delim = delim;
    EVENT_DEBUG(("EventTcp_ReadBuf() -> %p\n", state));

    return state;
}

/**********************************************************************
* %FUNCTION: EventTcp_WriteBuf
* %ARGUMENTS:
*  es -- event selector
*  socket -- socket to read from
*  buf -- buffer to write
*  len -- number of bytes to write
*  f -- function to call on EOF or when all bytes have been read
*  timeout -- timeout after which to cancel operation
*  data -- extra data to pass to function f.
* %RETURNS:
*  A new EventTcpState token or NULL on error
* %DESCRIPTION:
*  Sets up a handler to fill a buffer from a socket.
***********************************************************************/
EventTcpState *
EventTcp_WriteBuf(EventSelector *es,
		  int socket,
		  char *buf,
		  int len,
		  EventTcpIOFinishedFunc f,
		  int timeout,
		  void *data)
{
    EventTcpState *state;
    int flags;
    struct timeval t;

    EVENT_DEBUG(("EventTcp_WriteBuf(es=%p, socket=%d, len=%d, timeout=%d)\n", es, socket, len, timeout));
    if (len <= 0) return NULL;
    if (socket < 0) return NULL;

    /* Make sure socket is non-blocking */
    flags = fcntl(socket, F_GETFL, 0);
    if (flags == -1) {
	return NULL;
    }
    if (fcntl(socket, F_SETFL, flags | O_NONBLOCK) == -1) {
	return NULL;
    }

    state = malloc(sizeof(EventTcpState));
    if (!state) return NULL;

    memset(state, 0, sizeof(EventTcpState));

    state->socket = socket;

    state->buf = malloc(len);
    if (!state->buf) {
	free_state(state);
	return NULL;
    }
    memcpy(state->buf, buf, len);

    state->cur = state->buf;
    state->len = len;
    state->f = f;
    state->es = es;

    if (timeout <= 0) {
	t.tv_sec = -1;
	t.tv_usec = -1;
    } else {
	t.tv_sec = timeout;
	t.tv_usec = 0;
    }

    state->eh = Event_AddHandlerWithTimeout(es, socket, EVENT_FLAG_WRITEABLE,
					    t, handle_writeable,
					    (void *) state);
    if (!state->eh) {
	free_state(state);
	return NULL;
    }

    state->data = data;
    state->delim = -1;
    EVENT_DEBUG(("EventTcp_WriteBuf() -> %p\n", state));
    return state;
}

/**********************************************************************
* %FUNCTION: EventTcp_Connect
* %ARGUMENTS:
*  es -- event selector
*  fd -- descriptor to connect
*  addr -- address to connect to
*  addrlen -- length of address
*  f -- function to call with connected socket
*  data -- extra data to pass to f
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Does a non-blocking connect on fd
***********************************************************************/
void
EventTcp_Connect(EventSelector *es,
		 int fd,
		 struct sockaddr const *addr,
		 socklen_t addrlen,
		 EventTcpConnectFunc f,
		 int timeout,
		 void *data)
{
    int flags;
    int n;
    EventTcpConnectState *state;
    struct timeval t;

    /* Make sure socket is non-blocking */
    flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
	f(es, fd, EVENT_TCP_FLAG_IOERROR, data);
	return;
    }

    n = connect(fd, addr, addrlen);
    if (n < 0) {
	if (errno != EINPROGRESS) {
	    f(es, fd, EVENT_TCP_FLAG_IOERROR, data);
	    return;
	}
    }

    if (n == 0) { /* Connect succeeded immediately */
	f(es, fd, EVENT_TCP_FLAG_COMPLETE, data);
	return;
    }

    state = malloc(sizeof(*state));
    if (!state) {
	f(es, fd, EVENT_TCP_FLAG_IOERROR, data);
	return;
    }
    state->f = f;
    state->fd = fd;
    state->data = data;

    if (timeout <= 0) {
	t.tv_sec = -1;
	t.tv_usec = -1;
    } else {
	t.tv_sec = timeout;
	t.tv_usec = 0;
    }

    state->conn = Event_AddHandlerWithTimeout(es, fd, EVENT_FLAG_WRITEABLE,
					      t, handle_connect,
					      (void *) state);
    if (!state->conn) {
	free(state);
	f(es, fd, EVENT_TCP_FLAG_IOERROR, data);
	return;
    }
}

/**********************************************************************
* %FUNCTION: EventTcp_CancelPending
* %ARGUMENTS:
*  s -- an EventTcpState
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Cancels the pending event handler
***********************************************************************/
void
EventTcp_CancelPending(EventTcpState *s)
{
    free_state(s);
}
