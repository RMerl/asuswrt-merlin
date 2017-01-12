/* Copyright (c) 2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file pubsub.h
 * \brief Macros to implement publish/subscribe abstractions.
 *
 * To use these macros, call DECLARE_PUBSUB_TOPIC() with an identifier to use
 * as your topic.  Below, I'm going to assume you say DECLARE_PUBSUB_TOPIC(T).
 *
 * Doing this will declare the following types:
 *   typedef struct T_event_data_t T_event_data_t; // you define this struct
 *   typedef struct T_subscriber_data_t T_subscriber_data_t; // this one too.
 *   typedef struct T_subscriber_t T_subscriber_t; // opaque
 *   typedef int (*T_subscriber_fn_t)(T_event_data_t*, T_subscriber_data_t*);
 *
 * and it will declare the following functions:
 *     const T_subscriber_t *T_subscribe(T_subscriber_fn_t,
 *                                       T_subscriber_data_t *,
 *                                       unsigned flags,
 *                                       unsigned priority);
 *     int T_unsubscribe(const T_subscriber_t *)
 *
 * Elsewhere you can say DECLARE_NOTIFY_PUBSUB_TOPIC(static, T), which
 * declares:
 *
 *    static int T_notify(T_event_data_t *, unsigned notify_flags);
 *    static void T_clear(void);
 *
 * And in some C file, you would define these functions with:
 *    IMPLEMENT_PUBSUB_TOPIC(static, T).
 *
 * The implementations will be small typesafe wrappers over generic versions
 * of the above functions.
 *
 * To use the typesafe functions, you add any number of subscribers with
 * T_subscribe().  Each has an associated function pointer, data pointer,
 * and priority. Later, you can invoke T_notify() to declare that the
 * event has occurred. Each of the subscribers will be invoked once.
 **/

#ifndef TOR_PUBSUB_H
#define TOR_PUBSUB_H

#include "torint.h"

/**
 * Flag for T_subscribe: die with an assertion failure if the event
 * have ever been published before.  Used when a subscriber must absolutely
 * never have missed an event.
 */
#define SUBSCRIBE_ATSTART (1u<<0)

#define DECLARE_PUBSUB_STRUCT_TYPES(name)                               \
  /* You define this type. */                                           \
  typedef struct name ## _event_data_t name ## _event_data_t;           \
  /* You define this type. */                                           \
  typedef struct name ## _subscriber_data_t name ## _subscriber_data_t;

#define DECLARE_PUBSUB_TOPIC(name)                                      \
  /* This type is opaque. */                                            \
  typedef struct name ## _subscriber_t name ## _subscriber_t;           \
  /* You declare functions matching this type. */                       \
  typedef int (*name ## _subscriber_fn_t)(                              \
                                    name ## _event_data_t *data,        \
                                    name ## _subscriber_data_t *extra); \
  /* Call this function to subscribe to a topic. */                     \
  const name ## _subscriber_t *name ## _subscribe(                      \
                         name##_subscriber_fn_t subscriber,             \
                         name##_subscriber_data_t *extra_data,          \
                         unsigned flags,                                \
                         unsigned priority);                            \
  /* Call this function to unsubscribe from a topic. */                 \
  int name ## _unsubscribe(const name##_subscriber_t *s);

#define DECLARE_NOTIFY_PUBSUB_TOPIC(linkage, name)                          \
  /* Call this function to notify all subscribers. Flags not yet used. */   \
  linkage int name ## _notify(name ## _event_data_t *data, unsigned flags); \
  /* Call this function to release storage held by the topic. */            \
  linkage void name ## _clear(void);

/**
 * Type used to hold a generic function for a subscriber.
 *
 * [Yes, it is safe to cast to this, so long as we cast back to the original
 * type before calling.  From C99: "A pointer to a function of one type may be
 * converted to a pointer to a function of another type and back again; the
 * result shall compare equal to the original pointer."]
*/
typedef int (*pubsub_subscriber_fn_t)(void *, void *);

/**
 * Helper type to implement pubsub abstraction. Don't use this directly.
 * It represents a subscriber.
 */
typedef struct pubsub_subscriber_t {
  /** Function to invoke when the event triggers. */
  pubsub_subscriber_fn_t fn;
  /** Data associated with this subscriber. */
  void *subscriber_data;
  /** Priority for this subscriber. Low priorities happen first. */
  unsigned priority;
  /** Flags set on this subscriber. Not yet used.*/
  unsigned subscriber_flags;
} pubsub_subscriber_t;

/**
 * Helper type to implement pubsub abstraction. Don't use this directly.
 * It represents a topic, and keeps a record of subscribers.
 */
typedef struct pubsub_topic_t {
  /** List of subscribers to this topic. May be NULL. */
  struct smartlist_t *subscribers;
  /** Total number of times that pubsub_notify_() has ever been called on this
   * topic. */
  uint64_t n_events_fired;
  /** True iff we're running 'notify' on this topic, and shouldn't allow
   * any concurrent modifications or events. */
  unsigned locked;
} pubsub_topic_t;

const pubsub_subscriber_t *pubsub_subscribe_(pubsub_topic_t *topic,
                                             pubsub_subscriber_fn_t fn,
                                             void *subscriber_data,
                                             unsigned subscribe_flags,
                                             unsigned priority);
int pubsub_unsubscribe_(pubsub_topic_t *topic, const pubsub_subscriber_t *sub);
void pubsub_clear_(pubsub_topic_t *topic);
typedef int (*pubsub_notify_fn_t)(pubsub_subscriber_t *subscriber,
                                  void *notify_data);
int pubsub_notify_(pubsub_topic_t *topic, pubsub_notify_fn_t notify_fn,
                   void *notify_data, unsigned notify_flags);

#define IMPLEMENT_PUBSUB_TOPIC(notify_linkage, name)                    \
  static pubsub_topic_t name ## _topic_ = { NULL, 0, 0 };               \
  const name ## _subscriber_t *                                         \
  name ## _subscribe(name##_subscriber_fn_t subscriber,                 \
                     name##_subscriber_data_t *extra_data,              \
                     unsigned flags,                                    \
                     unsigned priority)                                 \
  {                                                                     \
    const pubsub_subscriber_t *s;                                       \
    s = pubsub_subscribe_(&name##_topic_,                               \
                          (pubsub_subscriber_fn_t)subscriber,           \
                          extra_data,                                   \
                          flags,                                        \
                          priority);                                    \
    return (const name##_subscriber_t *)s;                              \
  }                                                                     \
  int                                                                   \
  name ## _unsubscribe(const name##_subscriber_t *subscriber)           \
  {                                                                     \
    return pubsub_unsubscribe_(&name##_topic_,                          \
                               (const pubsub_subscriber_t *)subscriber); \
  }                                                                     \
  static int                                                            \
  name##_call_the_notify_fn_(pubsub_subscriber_t *subscriber,           \
                             void *notify_data)                         \
  {                                                                     \
    name ## _subscriber_fn_t fn;                                        \
    fn = (name ## _subscriber_fn_t) subscriber->fn;                     \
    return fn(notify_data, subscriber->subscriber_data);                \
  }                                                                     \
  notify_linkage int                                                    \
  name ## _notify(name ## _event_data_t *event_data, unsigned flags)    \
  {                                                                     \
    return pubsub_notify_(&name##_topic_,                               \
                          name##_call_the_notify_fn_,                   \
                          event_data,                                   \
                          flags);                                       \
  }                                                                     \
  notify_linkage void                                                   \
  name ## _clear(void)                                                  \
  {                                                                     \
    pubsub_clear_(&name##_topic_);                                      \
  }

#endif /* TOR_PUBSUB_H */

