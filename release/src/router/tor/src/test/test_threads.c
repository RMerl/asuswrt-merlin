/* Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#include "orconfig.h"
#include "or.h"
#include "compat_threads.h"
#include "test.h"

/** mutex for thread test to stop the threads hitting data at the same time. */
static tor_mutex_t *thread_test_mutex_ = NULL;
/** mutexes for the thread test to make sure that the threads have to
 * interleave somewhat. */
static tor_mutex_t *thread_test_start1_ = NULL,
                   *thread_test_start2_ = NULL;
/** Shared strmap for the thread test. */
static strmap_t *thread_test_strmap_ = NULL;
/** The name of thread1 for the thread test */
static char *thread1_name_ = NULL;
/** The name of thread2 for the thread test */
static char *thread2_name_ = NULL;

static int thread_fns_failed = 0;

static unsigned long thread_fn_tid1, thread_fn_tid2;

static void thread_test_func_(void* _s) ATTR_NORETURN;

/** How many iterations have the threads in the unit test run? */
static tor_threadlocal_t count;

/** Helper function for threading unit tests: This function runs in a
 * subthread. It grabs its own mutex (start1 or start2) to make sure that it
 * should start, then it repeatedly alters _test_thread_strmap protected by
 * thread_test_mutex_. */
static void
thread_test_func_(void* _s)
{
  char *s = _s;
  int i;
  tor_mutex_t *m;
  char buf[64];
  char **cp;
  int *mycount = tor_malloc_zero(sizeof(int));
  tor_threadlocal_set(&count, mycount);
  if (!strcmp(s, "thread 1")) {
    m = thread_test_start1_;
    cp = &thread1_name_;
    thread_fn_tid1 = tor_get_thread_id();
  } else {
    m = thread_test_start2_;
    cp = &thread2_name_;
    thread_fn_tid2 = tor_get_thread_id();
  }

  tor_snprintf(buf, sizeof(buf), "%lu", tor_get_thread_id());
  *cp = tor_strdup(buf);

  tor_mutex_acquire(m);

  for (i=0; i<10000; ++i) {
    tor_mutex_acquire(thread_test_mutex_);
    strmap_set(thread_test_strmap_, "last to run", *cp);
    tor_mutex_release(thread_test_mutex_);
    int *tls_count = tor_threadlocal_get(&count);
    tor_assert(tls_count == mycount);
    ++*tls_count;
  }
  tor_mutex_acquire(thread_test_mutex_);
  strmap_set(thread_test_strmap_, s, *cp);
  if (in_main_thread())
    ++thread_fns_failed;
  tor_mutex_release(thread_test_mutex_);

  tor_free(mycount);

  tor_mutex_release(m);

  spawn_exit();
}

/** Run unit tests for threading logic. */
static void
test_threads_basic(void *arg)
{
  char *s1 = NULL, *s2 = NULL;
  int done = 0, timedout = 0;
  time_t started;
  (void) arg;
  tt_int_op(tor_threadlocal_init(&count), OP_EQ, 0);

  set_main_thread();

  thread_test_mutex_ = tor_mutex_new();
  thread_test_start1_ = tor_mutex_new();
  thread_test_start2_ = tor_mutex_new();
  thread_test_strmap_ = strmap_new();
  s1 = tor_strdup("thread 1");
  s2 = tor_strdup("thread 2");
  tor_mutex_acquire(thread_test_start1_);
  tor_mutex_acquire(thread_test_start2_);
  spawn_func(thread_test_func_, s1);
  spawn_func(thread_test_func_, s2);
  tor_mutex_release(thread_test_start2_);
  tor_mutex_release(thread_test_start1_);
  started = time(NULL);
  while (!done) {
    tor_mutex_acquire(thread_test_mutex_);
    strmap_assert_ok(thread_test_strmap_);
    if (strmap_get(thread_test_strmap_, "thread 1") &&
        strmap_get(thread_test_strmap_, "thread 2")) {
      done = 1;
    } else if (time(NULL) > started + 150) {
      timedout = done = 1;
    }
    tor_mutex_release(thread_test_mutex_);
    /* Prevent the main thread from starving the worker threads. */
    tor_sleep_msec(10);
  }
  tor_mutex_acquire(thread_test_start1_);
  tor_mutex_release(thread_test_start1_);
  tor_mutex_acquire(thread_test_start2_);
  tor_mutex_release(thread_test_start2_);

  tor_mutex_free(thread_test_mutex_);

  if (timedout) {
    tt_assert(strmap_get(thread_test_strmap_, "thread 1"));
    tt_assert(strmap_get(thread_test_strmap_, "thread 2"));
    tt_assert(!timedout);
  }

  /* different thread IDs. */
  tt_assert(strcmp(strmap_get(thread_test_strmap_, "thread 1"),
                     strmap_get(thread_test_strmap_, "thread 2")));
  tt_assert(!strcmp(strmap_get(thread_test_strmap_, "thread 1"),
                      strmap_get(thread_test_strmap_, "last to run")) ||
              !strcmp(strmap_get(thread_test_strmap_, "thread 2"),
                      strmap_get(thread_test_strmap_, "last to run")));

  tt_int_op(thread_fns_failed, ==, 0);
  tt_int_op(thread_fn_tid1, !=, thread_fn_tid2);

 done:
  tor_free(s1);
  tor_free(s2);
  tor_free(thread1_name_);
  tor_free(thread2_name_);
  if (thread_test_strmap_)
    strmap_free(thread_test_strmap_, NULL);
  if (thread_test_start1_)
    tor_mutex_free(thread_test_start1_);
  if (thread_test_start2_)
    tor_mutex_free(thread_test_start2_);
}

typedef struct cv_testinfo_s {
  tor_cond_t *cond;
  tor_mutex_t *mutex;
  int value;
  int addend;
  int shutdown;
  int n_shutdown;
  int n_wakeups;
  int n_timeouts;
  int n_threads;
  const struct timeval *tv;
} cv_testinfo_t;

static cv_testinfo_t *
cv_testinfo_new(void)
{
  cv_testinfo_t *i = tor_malloc_zero(sizeof(*i));
  i->cond = tor_cond_new();
  i->mutex = tor_mutex_new_nonrecursive();
  return i;
}

static void
cv_testinfo_free(cv_testinfo_t *i)
{
  if (!i)
    return;
  tor_cond_free(i->cond);
  tor_mutex_free(i->mutex);
  tor_free(i);
}

static void cv_test_thr_fn_(void *arg) ATTR_NORETURN;

static void
cv_test_thr_fn_(void *arg)
{
  cv_testinfo_t *i = arg;
  int tid, r;

  tor_mutex_acquire(i->mutex);
  tid = i->n_threads++;
  tor_mutex_release(i->mutex);
  (void) tid;

  tor_mutex_acquire(i->mutex);
  while (1) {
    if (i->addend) {
      i->value += i->addend;
      i->addend = 0;
    }

    if (i->shutdown) {
      ++i->n_shutdown;
      i->shutdown = 0;
      tor_mutex_release(i->mutex);
      spawn_exit();
    }
    r = tor_cond_wait(i->cond, i->mutex, i->tv);
    ++i->n_wakeups;
    if (r == 1) {
      ++i->n_timeouts;
      tor_mutex_release(i->mutex);
      spawn_exit();
    }
  }
}

static void
test_threads_conditionvar(void *arg)
{
  cv_testinfo_t *ti=NULL;
  const struct timeval msec100 = { 0, 100*1000 };
  const int timeout = !strcmp(arg, "tv");

  ti = cv_testinfo_new();
  if (timeout) {
    ti->tv = &msec100;
  }
  spawn_func(cv_test_thr_fn_, ti);
  spawn_func(cv_test_thr_fn_, ti);
  spawn_func(cv_test_thr_fn_, ti);
  spawn_func(cv_test_thr_fn_, ti);

  tor_mutex_acquire(ti->mutex);
  ti->addend = 7;
  ti->shutdown = 1;
  tor_cond_signal_one(ti->cond);
  tor_mutex_release(ti->mutex);

#define SPIN()                                  \
  while (1) {                                   \
    tor_mutex_acquire(ti->mutex);               \
    if (ti->addend == 0) {                      \
      break;                                    \
    }                                           \
    tor_mutex_release(ti->mutex);               \
  }

  SPIN();

  ti->addend = 30;
  ti->shutdown = 1;
  tor_cond_signal_all(ti->cond);
  tor_mutex_release(ti->mutex);
  SPIN();

  ti->addend = 1000;
  if (! timeout) ti->shutdown = 1;
  tor_cond_signal_one(ti->cond);
  tor_mutex_release(ti->mutex);
  SPIN();
  ti->addend = 300;
  if (! timeout) ti->shutdown = 1;
  tor_cond_signal_all(ti->cond);
  tor_mutex_release(ti->mutex);

  SPIN();
  tor_mutex_release(ti->mutex);

  tt_int_op(ti->value, ==, 1337);
  if (!timeout) {
    tt_int_op(ti->n_shutdown, ==, 4);
  } else {
    tor_sleep_msec(200);
    tor_mutex_acquire(ti->mutex);
    tt_int_op(ti->n_shutdown, ==, 2);
    tt_int_op(ti->n_timeouts, ==, 2);
    tor_mutex_release(ti->mutex);
  }

 done:
  cv_testinfo_free(ti);
}

#define THREAD_TEST(name)                                               \
  { #name, test_threads_##name, TT_FORK, NULL, NULL }

struct testcase_t thread_tests[] = {
  THREAD_TEST(basic),
  { "conditionvar", test_threads_conditionvar, TT_FORK,
    &passthrough_setup, (void*)"no-tv" },
  { "conditionvar_timeout", test_threads_conditionvar, TT_FORK,
    &passthrough_setup, (void*)"tv" },
  END_OF_TESTCASES
};

