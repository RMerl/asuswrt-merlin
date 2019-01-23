/* Copyright (c) 2003-2004, Roger Dingledine
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file compat_pthreads.c
 *
 * \brief Implementation for the pthreads-based multithreading backend
 * functions.
 */

#include "orconfig.h"
#include <pthread.h>
#include <signal.h>
#include <time.h>

#include "compat.h"
#include "torlog.h"
#include "util.h"

/** Wraps a void (*)(void*) function and its argument so we can
 * invoke them in a way pthreads would expect.
 */
typedef struct tor_pthread_data_t {
  void (*func)(void *);
  void *data;
} tor_pthread_data_t;
/** Given a tor_pthread_data_t <b>_data</b>, call _data-&gt;func(d-&gt;data)
 * and free _data.  Used to make sure we can call functions the way pthread
 * expects. */
static void *
tor_pthread_helper_fn(void *_data)
{
  tor_pthread_data_t *data = _data;
  void (*func)(void*);
  void *arg;
  /* mask signals to worker threads to avoid SIGPIPE, etc */
  sigset_t sigs;
  /* We're in a subthread; don't handle any signals here. */
  sigfillset(&sigs);
  pthread_sigmask(SIG_SETMASK, &sigs, NULL);

  func = data->func;
  arg = data->data;
  tor_free(_data);
  func(arg);
  return NULL;
}
/**
 * A pthread attribute to make threads start detached.
 */
static pthread_attr_t attr_detached;
/** True iff we've called tor_threads_init() */
static int threads_initialized = 0;

/** Minimalist interface to run a void function in the background.  On
 * Unix calls pthread_create, on win32 calls beginthread.  Returns -1 on
 * failure.
 * func should not return, but rather should call spawn_exit.
 *
 * NOTE: if <b>data</b> is used, it should not be allocated on the stack,
 * since in a multithreaded environment, there is no way to be sure that
 * the caller's stack will still be around when the called function is
 * running.
 */
int
spawn_func(void (*func)(void *), void *data)
{
  pthread_t thread;
  tor_pthread_data_t *d;
  if (PREDICT_UNLIKELY(!threads_initialized)) {
    tor_threads_init();
  }
  d = tor_malloc(sizeof(tor_pthread_data_t));
  d->data = data;
  d->func = func;
  if (pthread_create(&thread, &attr_detached, tor_pthread_helper_fn, d)) {
    tor_free(d);
    return -1;
  }

  return 0;
}

/** End the current thread/process.
 */
void
spawn_exit(void)
{
  pthread_exit(NULL);
}

/** A mutex attribute that we're going to use to tell pthreads that we want
 * "recursive" mutexes (i.e., once we can re-lock if we're already holding
 * them.) */
static pthread_mutexattr_t attr_recursive;

/** Initialize <b>mutex</b> so it can be locked.  Every mutex must be set
 * up with tor_mutex_init() or tor_mutex_new(); not both. */
void
tor_mutex_init(tor_mutex_t *mutex)
{
  if (PREDICT_UNLIKELY(!threads_initialized))
    tor_threads_init(); // LCOV_EXCL_LINE
  const int err = pthread_mutex_init(&mutex->mutex, &attr_recursive);
  if (PREDICT_UNLIKELY(err)) {
    // LCOV_EXCL_START
    log_err(LD_GENERAL, "Error %d creating a mutex.", err);
    tor_assert_unreached();
    // LCOV_EXCL_STOP
  }
}

/** As tor_mutex_init, but initialize a mutex suitable that may be
 * non-recursive, if the OS supports that. */
void
tor_mutex_init_nonrecursive(tor_mutex_t *mutex)
{
  int err;
  if (!threads_initialized)
    tor_threads_init(); // LCOV_EXCL_LINE
  err = pthread_mutex_init(&mutex->mutex, NULL);
  if (PREDICT_UNLIKELY(err)) {
    // LCOV_EXCL_START
    log_err(LD_GENERAL, "Error %d creating a mutex.", err);
    tor_assert_unreached();
    // LCOV_EXCL_STOP
  }
}

/** Wait until <b>m</b> is free, then acquire it. */
void
tor_mutex_acquire(tor_mutex_t *m)
{
  int err;
  tor_assert(m);
  err = pthread_mutex_lock(&m->mutex);
  if (PREDICT_UNLIKELY(err)) {
    // LCOV_EXCL_START
    log_err(LD_GENERAL, "Error %d locking a mutex.", err);
    tor_assert_unreached();
    // LCOV_EXCL_STOP
  }
}
/** Release the lock <b>m</b> so another thread can have it. */
void
tor_mutex_release(tor_mutex_t *m)
{
  int err;
  tor_assert(m);
  err = pthread_mutex_unlock(&m->mutex);
  if (PREDICT_UNLIKELY(err)) {
    // LCOV_EXCL_START
    log_err(LD_GENERAL, "Error %d unlocking a mutex.", err);
    tor_assert_unreached();
    // LCOV_EXCL_STOP
  }
}
/** Clean up the mutex <b>m</b> so that it no longer uses any system
 * resources.  Does not free <b>m</b>.  This function must only be called on
 * mutexes from tor_mutex_init(). */
void
tor_mutex_uninit(tor_mutex_t *m)
{
  int err;
  tor_assert(m);
  err = pthread_mutex_destroy(&m->mutex);
  if (PREDICT_UNLIKELY(err)) {
    // LCOV_EXCL_START
    log_err(LD_GENERAL, "Error %d destroying a mutex.", err);
    tor_assert_unreached();
    // LCOV_EXCL_STOP
  }
}
/** Return an integer representing this thread. */
unsigned long
tor_get_thread_id(void)
{
  union {
    pthread_t thr;
    unsigned long id;
  } r;
  r.thr = pthread_self();
  return r.id;
}

/* Conditions. */

/** Initialize an already-allocated condition variable. */
int
tor_cond_init(tor_cond_t *cond)
{
  pthread_condattr_t condattr;

  memset(cond, 0, sizeof(tor_cond_t));
  /* Default condition attribute. Might be used if clock monotonic is
   * available else this won't affect anything. */
  if (pthread_condattr_init(&condattr)) {
    return -1;
  }

#if defined(HAVE_CLOCK_GETTIME)
#if defined(CLOCK_MONOTONIC) && defined(HAVE_PTHREAD_CONDATTR_SETCLOCK)
  /* Use monotonic time so when we timedwait() on it, any clock adjustment
   * won't affect the timeout value. */
  if (pthread_condattr_setclock(&condattr, CLOCK_MONOTONIC)) {
    return -1;
  }
#define USE_COND_CLOCK CLOCK_MONOTONIC
#else /* !defined HAVE_PTHREAD_CONDATTR_SETCLOCK */
  /* On OSX Sierra, there is no pthread_condattr_setclock, so we are stuck
   * with the realtime clock.
   */
#define USE_COND_CLOCK CLOCK_REALTIME
#endif /* which clock to use */
#endif /* HAVE_CLOCK_GETTIME */
  if (pthread_cond_init(&cond->cond, &condattr)) {
    return -1;
  }
  return 0;
}

/** Release all resources held by <b>cond</b>, but do not free <b>cond</b>
 * itself. */
void
tor_cond_uninit(tor_cond_t *cond)
{
  if (pthread_cond_destroy(&cond->cond)) {
    // LCOV_EXCL_START
    log_warn(LD_GENERAL,"Error freeing condition: %s", strerror(errno));
    return;
    // LCOV_EXCL_STOP
  }
}
/** Wait until one of the tor_cond_signal functions is called on <b>cond</b>.
 * (If <b>tv</b> is set, and that amount of time passes with no signal to
 * <b>cond</b>, return anyway.  All waiters on the condition must wait holding
 * the same <b>mutex</b>.  All signallers should hold that mutex.  The mutex
 * needs to have been allocated with tor_mutex_init_for_cond().
 *
 * Returns 0 on success, -1 on failure, 1 on timeout. */
int
tor_cond_wait(tor_cond_t *cond, tor_mutex_t *mutex, const struct timeval *tv)
{
  int r;
  if (tv == NULL) {
    while (1) {
      r = pthread_cond_wait(&cond->cond, &mutex->mutex);
      if (r == EINTR) {
        /* EINTR should be impossible according to POSIX, but POSIX, like the
         * Pirate's Code, is apparently treated "more like what you'd call
         * guidelines than actual rules." */
        continue; // LCOV_EXCL_LINE
      }
      return r ? -1 : 0;
    }
  } else {
    struct timeval tvnow, tvsum;
    struct timespec ts;
    while (1) {
#if defined(HAVE_CLOCK_GETTIME) && defined(USE_COND_CLOCK)
      if (clock_gettime(USE_COND_CLOCK, &ts) < 0) {
        return -1;
      }
      tvnow.tv_sec = ts.tv_sec;
      tvnow.tv_usec = (int)(ts.tv_nsec / 1000);
      timeradd(tv, &tvnow, &tvsum);
#else
      if (gettimeofday(&tvnow, NULL) < 0)
        return -1;
      timeradd(tv, &tvnow, &tvsum);
#endif /* HAVE_CLOCK_GETTIME, CLOCK_MONOTONIC */

      ts.tv_sec = tvsum.tv_sec;
      ts.tv_nsec = tvsum.tv_usec * 1000;

      r = pthread_cond_timedwait(&cond->cond, &mutex->mutex, &ts);
      if (r == 0)
        return 0;
      else if (r == ETIMEDOUT)
        return 1;
      else if (r == EINTR)
        continue;
      else
        return -1;
    }
  }
}
/** Wake up one of the waiters on <b>cond</b>. */
void
tor_cond_signal_one(tor_cond_t *cond)
{
  pthread_cond_signal(&cond->cond);
}
/** Wake up all of the waiters on <b>cond</b>. */
void
tor_cond_signal_all(tor_cond_t *cond)
{
  pthread_cond_broadcast(&cond->cond);
}

int
tor_threadlocal_init(tor_threadlocal_t *threadlocal)
{
  int err = pthread_key_create(&threadlocal->key, NULL);
  return err ? -1 : 0;
}

void
tor_threadlocal_destroy(tor_threadlocal_t *threadlocal)
{
  pthread_key_delete(threadlocal->key);
  memset(threadlocal, 0, sizeof(tor_threadlocal_t));
}

void *
tor_threadlocal_get(tor_threadlocal_t *threadlocal)
{
  return pthread_getspecific(threadlocal->key);
}

void
tor_threadlocal_set(tor_threadlocal_t *threadlocal, void *value)
{
  int err = pthread_setspecific(threadlocal->key, value);
  tor_assert(err == 0);
}

/** Set up common structures for use by threading. */
void
tor_threads_init(void)
{
  if (!threads_initialized) {
    pthread_mutexattr_init(&attr_recursive);
    pthread_mutexattr_settype(&attr_recursive, PTHREAD_MUTEX_RECURSIVE);
    const int ret1 = pthread_attr_init(&attr_detached);
    tor_assert(ret1 == 0);
#ifndef PTHREAD_CREATE_DETACHED
#define PTHREAD_CREATE_DETACHED 1
#endif
    const int ret2 =
      pthread_attr_setdetachstate(&attr_detached, PTHREAD_CREATE_DETACHED);
    tor_assert(ret2 == 0);
    threads_initialized = 1;
    set_main_thread();
  }
}

