/* Copyright (c) 2013-2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#ifndef TOR_WORKQUEUE_H
#define TOR_WORKQUEUE_H

#include "compat.h"

/** A replyqueue is used to tell the main thread about the outcome of
 * work that we queued for the workers. */
typedef struct replyqueue_s replyqueue_t;
/** A thread-pool manages starting threads and passing work to them. */
typedef struct threadpool_s threadpool_t;
/** A workqueue entry represents a request that has been passed to a thread
 * pool. */
typedef struct workqueue_entry_s workqueue_entry_t;

/** Possible return value from a work function: */
typedef enum {
  WQ_RPL_REPLY = 0, /** indicates success */
  WQ_RPL_ERROR = 1, /** indicates fatal error */
  WQ_RPL_SHUTDOWN = 2, /** indicates thread is shutting down */
} workqueue_reply_t;

workqueue_entry_t *threadpool_queue_work(threadpool_t *pool,
                                         workqueue_reply_t (*fn)(void *,
                                                                 void *),
                                         void (*reply_fn)(void *),
                                         void *arg);

int threadpool_queue_update(threadpool_t *pool,
                            void *(*dup_fn)(void *),
                            workqueue_reply_t (*fn)(void *, void *),
                            void (*free_fn)(void *),
                            void *arg);
void *workqueue_entry_cancel(workqueue_entry_t *pending_work);
threadpool_t *threadpool_new(int n_threads,
                             replyqueue_t *replyqueue,
                             void *(*new_thread_state_fn)(void*),
                             void (*free_thread_state_fn)(void*),
                             void *arg);
replyqueue_t *threadpool_get_replyqueue(threadpool_t *tp);

replyqueue_t *replyqueue_new(uint32_t alertsocks_flags);
tor_socket_t replyqueue_get_socket(replyqueue_t *rq);
void replyqueue_process(replyqueue_t *queue);

#endif

