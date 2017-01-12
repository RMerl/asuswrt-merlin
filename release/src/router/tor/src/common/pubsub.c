/* Copyright (c) 2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file pubsub.c
 *
 * \brief DOCDOC
 */

#include "orconfig.h"
#include "pubsub.h"
#include "container.h"

/** Helper: insert <b>s</b> into <b>topic's</b> list of subscribers, keeping
 * them sorted in priority order. */
static void
subscriber_insert(pubsub_topic_t *topic, pubsub_subscriber_t *s)
{
  int i;
  smartlist_t *sl = topic->subscribers;
  for (i = 0; i < smartlist_len(sl); ++i) {
    pubsub_subscriber_t *other = smartlist_get(sl, i);
    if (s->priority < other->priority) {
      break;
    }
  }
  smartlist_insert(sl, i, s);
}

/**
 * Add a new subscriber to <b>topic</b>, where (when an event is triggered),
 * we'll notify the function <b>fn</b> by passing it <b>subscriber_data</b>.
 * Return a handle to the subscribe which can later be passed to
 * pubsub_unsubscribe_().
 *
 * Functions are called in priority order, from lowest to highest.
 *
 * See pubsub.h for <b>subscribe_flags</b>.
 */
const pubsub_subscriber_t *
pubsub_subscribe_(pubsub_topic_t *topic,
                  pubsub_subscriber_fn_t fn,
                  void *subscriber_data,
                  unsigned subscribe_flags,
                  unsigned priority)
{
  tor_assert(! topic->locked);
  if (subscribe_flags & SUBSCRIBE_ATSTART) {
    tor_assert(topic->n_events_fired == 0);
  }
  pubsub_subscriber_t *r = tor_malloc_zero(sizeof(*r));
  r->priority = priority;
  r->subscriber_flags = subscribe_flags;
  r->fn = fn;
  r->subscriber_data = subscriber_data;
  if (topic->subscribers == NULL) {
    topic->subscribers = smartlist_new();
  }
  subscriber_insert(topic, r);
  return r;
}

/**
 * Remove the subscriber <b>s</b> from <b>topic</b>.  After calling this
 * function, <b>s</b> may no longer be used.
 */
int
pubsub_unsubscribe_(pubsub_topic_t *topic,
                    const pubsub_subscriber_t *s)
{
  tor_assert(! topic->locked);
  smartlist_t *sl = topic->subscribers;
  if (sl == NULL)
    return -1;
  int i = smartlist_pos(sl, s);
  if (i == -1)
    return -1;
  pubsub_subscriber_t *tmp = smartlist_get(sl, i);
  tor_assert(tmp == s);
  smartlist_del_keeporder(sl, i);
  tor_free(tmp);
  return 0;
}

/**
 * For every subscriber s in <b>topic</b>, invoke notify_fn on s and
 * event_data.  Return 0 if there were no nonzero return values, and -1 if
 * there were any.
 */
int
pubsub_notify_(pubsub_topic_t *topic, pubsub_notify_fn_t notify_fn,
               void *event_data, unsigned notify_flags)
{
  tor_assert(! topic->locked);
  (void) notify_flags;
  smartlist_t *sl = topic->subscribers;
  int n_bad = 0;
  ++topic->n_events_fired;
  if (sl == NULL)
    return -1;
  topic->locked = 1;
  SMARTLIST_FOREACH_BEGIN(sl, pubsub_subscriber_t *, s) {
    int r = notify_fn(s, event_data);
    if (r != 0)
      ++n_bad;
  } SMARTLIST_FOREACH_END(s);
  topic->locked = 0;
  return (n_bad == 0) ? 0 : -1;
}

/**
 * Release all storage held by <b>topic</b>.
 */
void
pubsub_clear_(pubsub_topic_t *topic)
{
  tor_assert(! topic->locked);

  smartlist_t *sl = topic->subscribers;
  if (sl == NULL)
    return;
  SMARTLIST_FOREACH_BEGIN(sl, pubsub_subscriber_t *, s) {
    tor_free(s);
  } SMARTLIST_FOREACH_END(s);
  smartlist_free(sl);
  topic->subscribers = NULL;
  topic->n_events_fired = 0;
}

