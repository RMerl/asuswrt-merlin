/* Copyright (c) 2003-2004, Roger Dingledine
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file compat_threads.c
 *
 * \brief Cross-platform threading and inter-thread communication logic.
 *  (Platform-specific parts are written in the other compat_*threads
 *  modules.)
 */

#include "orconfig.h"
#include <stdlib.h>
#include "compat.h"
#include "compat_threads.h"

#include "util.h"
#include "torlog.h"

#ifdef HAVE_SYS_EVENTFD_H
#include <sys/eventfd.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

/** Return a newly allocated, ready-for-use mutex. */
tor_mutex_t *
tor_mutex_new(void)
{
  tor_mutex_t *m = tor_malloc_zero(sizeof(tor_mutex_t));
  tor_mutex_init(m);
  return m;
}
/** Return a newly allocated, ready-for-use mutex.  This one might be
 * non-recursive, if that's faster. */
tor_mutex_t *
tor_mutex_new_nonrecursive(void)
{
  tor_mutex_t *m = tor_malloc_zero(sizeof(tor_mutex_t));
  tor_mutex_init_nonrecursive(m);
  return m;
}
/** Release all storage and system resources held by <b>m</b>. */
void
tor_mutex_free(tor_mutex_t *m)
{
  if (!m)
    return;
  tor_mutex_uninit(m);
  tor_free(m);
}

/** Allocate and return a new condition variable. */
tor_cond_t *
tor_cond_new(void)
{
  tor_cond_t *cond = tor_malloc(sizeof(tor_cond_t));
  if (BUG(tor_cond_init(cond)<0))
    tor_free(cond); // LCOV_EXCL_LINE
  return cond;
}

/** Free all storage held in <b>c</b>. */
void
tor_cond_free(tor_cond_t *c)
{
  if (!c)
    return;
  tor_cond_uninit(c);
  tor_free(c);
}

/** Identity of the "main" thread */
static unsigned long main_thread_id = -1;

/** Start considering the current thread to be the 'main thread'.  This has
 * no effect on anything besides in_main_thread(). */
void
set_main_thread(void)
{
  main_thread_id = tor_get_thread_id();
}
/** Return true iff called from the main thread. */
int
in_main_thread(void)
{
  return main_thread_id == tor_get_thread_id();
}

#if defined(HAVE_EVENTFD) || defined(HAVE_PIPE)
/* As write(), but retry on EINTR */
static int
write_ni(int fd, const void *buf, size_t n)
{
  int r;
 again:
  r = (int) write(fd, buf, n);
  if (r < 0 && errno == EINTR)
    goto again;
  return r;
}
/* As read(), but retry on EINTR */
static int
read_ni(int fd, void *buf, size_t n)
{
  int r;
 again:
  r = (int) read(fd, buf, n);
  if (r < 0 && errno == EINTR)
    goto again;
  return r;
}
#endif

/** As send(), but retry on EINTR. */
static int
send_ni(int fd, const void *buf, size_t n, int flags)
{
  int r;
 again:
  r = (int) send(fd, buf, n, flags);
  if (r < 0 && ERRNO_IS_EINTR(tor_socket_errno(fd)))
    goto again;
  return r;
}

/** As recv(), but retry on EINTR. */
static int
recv_ni(int fd, void *buf, size_t n, int flags)
{
  int r;
 again:
  r = (int) recv(fd, buf, n, flags);
  if (r < 0 && ERRNO_IS_EINTR(tor_socket_errno(fd)))
    goto again;
  return r;
}

#ifdef HAVE_EVENTFD
/* Increment the event count on an eventfd <b>fd</b> */
static int
eventfd_alert(int fd)
{
  uint64_t u = 1;
  int r = write_ni(fd, (void*)&u, sizeof(u));
  if (r < 0 && errno != EAGAIN)
    return -1;
  return 0;
}

/* Drain all events from an eventfd <b>fd</b>. */
static int
eventfd_drain(int fd)
{
  uint64_t u = 0;
  int r = read_ni(fd, (void*)&u, sizeof(u));
  if (r < 0 && errno != EAGAIN)
    return -1;
  return 0;
}
#endif

#ifdef HAVE_PIPE
/** Send a byte over a pipe. Return 0 on success or EAGAIN; -1 on error */
static int
pipe_alert(int fd)
{
  ssize_t r = write_ni(fd, "x", 1);
  if (r < 0 && errno != EAGAIN)
    return -1;
  return 0;
}

/** Drain all input from a pipe <b>fd</b> and ignore it.  Return 0 on
 * success, -1 on error. */
static int
pipe_drain(int fd)
{
  char buf[32];
  ssize_t r;
  do {
    r = read_ni(fd, buf, sizeof(buf));
  } while (r > 0);
  if (r < 0 && errno != EAGAIN)
    return -1;
  /* A value of r = 0 means EOF on the fd so successfully drained. */
  return 0;
}
#endif

/** Send a byte on socket <b>fd</b>t.  Return 0 on success or EAGAIN,
 * -1 on error. */
static int
sock_alert(tor_socket_t fd)
{
  ssize_t r = send_ni(fd, "x", 1, 0);
  if (r < 0 && !ERRNO_IS_EAGAIN(tor_socket_errno(fd)))
    return -1;
  return 0;
}

/** Drain all the input from a socket <b>fd</b>, and ignore it.  Return 0 on
 * success, -1 on error. */
static int
sock_drain(tor_socket_t fd)
{
  char buf[32];
  ssize_t r;
  do {
    r = recv_ni(fd, buf, sizeof(buf), 0);
  } while (r > 0);
  if (r < 0 && !ERRNO_IS_EAGAIN(tor_socket_errno(fd)))
    return -1;
  /* A value of r = 0 means EOF on the fd so successfully drained. */
  return 0;
}

/** Allocate a new set of alert sockets, and set the appropriate function
 * pointers, in <b>socks_out</b>. */
int
alert_sockets_create(alert_sockets_t *socks_out, uint32_t flags)
{
  tor_socket_t socks[2] = { TOR_INVALID_SOCKET, TOR_INVALID_SOCKET };

#ifdef HAVE_EVENTFD
  /* First, we try the Linux eventfd() syscall.  This gives a 64-bit counter
   * associated with a single file descriptor. */
#if defined(EFD_CLOEXEC) && defined(EFD_NONBLOCK)
  if (!(flags & ASOCKS_NOEVENTFD2))
    socks[0] = eventfd(0, EFD_CLOEXEC|EFD_NONBLOCK);
#endif
  if (socks[0] < 0 && !(flags & ASOCKS_NOEVENTFD)) {
    socks[0] = eventfd(0,0);
    if (socks[0] >= 0) {
      if (fcntl(socks[0], F_SETFD, FD_CLOEXEC) < 0 ||
          set_socket_nonblocking(socks[0]) < 0) {
        // LCOV_EXCL_START -- if eventfd succeeds, fcntl will.
        tor_assert_nonfatal_unreached();
        close(socks[0]);
        return -1;
        // LCOV_EXCL_STOP
      }
    }
  }
  if (socks[0] >= 0) {
    socks_out->read_fd = socks_out->write_fd = socks[0];
    socks_out->alert_fn = eventfd_alert;
    socks_out->drain_fn = eventfd_drain;
    return 0;
  }
#endif

#ifdef HAVE_PIPE2
  /* Now we're going to try pipes. First type the pipe2() syscall, if we
   * have it, so we can save some calls... */
  if (!(flags & ASOCKS_NOPIPE2) &&
      pipe2(socks, O_NONBLOCK|O_CLOEXEC) == 0) {
    socks_out->read_fd = socks[0];
    socks_out->write_fd = socks[1];
    socks_out->alert_fn = pipe_alert;
    socks_out->drain_fn = pipe_drain;
    return 0;
  }
#endif

#ifdef HAVE_PIPE
  /* Now try the regular pipe() syscall.  Pipes have a bit lower overhead than
   * socketpairs, fwict. */
  if (!(flags & ASOCKS_NOPIPE) &&
      pipe(socks) == 0) {
    if (fcntl(socks[0], F_SETFD, FD_CLOEXEC) < 0 ||
        fcntl(socks[1], F_SETFD, FD_CLOEXEC) < 0 ||
        set_socket_nonblocking(socks[0]) < 0 ||
        set_socket_nonblocking(socks[1]) < 0) {
      // LCOV_EXCL_START -- if pipe succeeds, you can fcntl the output
      tor_assert_nonfatal_unreached();
      close(socks[0]);
      close(socks[1]);
      return -1;
      // LCOV_EXCL_STOP
    }
    socks_out->read_fd = socks[0];
    socks_out->write_fd = socks[1];
    socks_out->alert_fn = pipe_alert;
    socks_out->drain_fn = pipe_drain;
    return 0;
  }
#endif

  /* If nothing else worked, fall back on socketpair(). */
  if (!(flags & ASOCKS_NOSOCKETPAIR) &&
      tor_socketpair(AF_UNIX, SOCK_STREAM, 0, socks) == 0) {
    if (set_socket_nonblocking(socks[0]) < 0 ||
        set_socket_nonblocking(socks[1])) {
      // LCOV_EXCL_START -- if socketpair worked, you can make it nonblocking.
      tor_assert_nonfatal_unreached();
      tor_close_socket(socks[0]);
      tor_close_socket(socks[1]);
      return -1;
      // LCOV_EXCL_STOP
    }
    socks_out->read_fd = socks[0];
    socks_out->write_fd = socks[1];
    socks_out->alert_fn = sock_alert;
    socks_out->drain_fn = sock_drain;
    return 0;
  }
  return -1;
}

/** Close the sockets in <b>socks</b>. */
void
alert_sockets_close(alert_sockets_t *socks)
{
  if (socks->alert_fn == sock_alert) {
    /* they are sockets. */
    tor_close_socket(socks->read_fd);
    tor_close_socket(socks->write_fd);
  } else {
    close(socks->read_fd);
    if (socks->write_fd != socks->read_fd)
      close(socks->write_fd);
  }
  socks->read_fd = socks->write_fd = -1;
}

