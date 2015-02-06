/* * Copyright (c) 2012-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file channel.c
 * \brief OR-to-OR channel abstraction layer
 **/

/*
 * Define this so channel.h gives us things only channel_t subclasses
 * should touch.
 */

#define TOR_CHANNEL_INTERNAL_

#include "or.h"
#include "channel.h"
#include "channeltls.h"
#include "circuitbuild.h"
#include "circuitlist.h"
#include "circuitstats.h"
#include "config.h"
#include "connection_or.h" /* For var_cell_free() */
#include "circuitmux.h"
#include "entrynodes.h"
#include "geoip.h"
#include "nodelist.h"
#include "relay.h"
#include "rephist.h"
#include "router.h"
#include "routerlist.h"

/* Cell queue structure */

typedef struct cell_queue_entry_s cell_queue_entry_t;
struct cell_queue_entry_s {
  TOR_SIMPLEQ_ENTRY(cell_queue_entry_s) next;
  enum {
    CELL_QUEUE_FIXED,
    CELL_QUEUE_VAR,
    CELL_QUEUE_PACKED
  } type;
  union {
    struct {
      cell_t *cell;
    } fixed;
    struct {
      var_cell_t *var_cell;
    } var;
    struct {
      packed_cell_t *packed_cell;
    } packed;
  } u;
};

/* Global lists of channels */

/* All channel_t instances */
static smartlist_t *all_channels = NULL;

/* All channel_t instances not in ERROR or CLOSED states */
static smartlist_t *active_channels = NULL;

/* All channel_t instances in ERROR or CLOSED states */
static smartlist_t *finished_channels = NULL;

/* All channel_listener_t instances */
static smartlist_t *all_listeners = NULL;

/* All channel_listener_t instances in LISTENING state */
static smartlist_t *active_listeners = NULL;

/* All channel_listener_t instances in LISTENING state */
static smartlist_t *finished_listeners = NULL;

/* Counter for ID numbers */
static uint64_t n_channels_allocated = 0;

/* Digest->channel map
 *
 * Similar to the one used in connection_or.c, this maps from the identity
 * digest of a remote endpoint to a channel_t to that endpoint.  Channels
 * should be placed here when registered and removed when they close or error.
 * If more than one channel exists, follow the next_with_same_id pointer
 * as a linked list.
 */
HT_HEAD(channel_idmap, channel_idmap_entry_s) channel_identity_map =
  HT_INITIALIZER();

typedef struct channel_idmap_entry_s {
  HT_ENTRY(channel_idmap_entry_s) node;
  uint8_t digest[DIGEST_LEN];
  TOR_LIST_HEAD(channel_list_s, channel_s) channel_list;
} channel_idmap_entry_t;

static INLINE unsigned
channel_idmap_hash(const channel_idmap_entry_t *ent)
{
  return (unsigned) siphash24g(ent->digest, DIGEST_LEN);
}

static INLINE int
channel_idmap_eq(const channel_idmap_entry_t *a,
                  const channel_idmap_entry_t *b)
{
  return tor_memeq(a->digest, b->digest, DIGEST_LEN);
}

HT_PROTOTYPE(channel_idmap, channel_idmap_entry_s, node, channel_idmap_hash,
             channel_idmap_eq);
HT_GENERATE(channel_idmap, channel_idmap_entry_s, node, channel_idmap_hash,
            channel_idmap_eq, 0.5, tor_malloc, tor_realloc, tor_free_);

static cell_queue_entry_t * cell_queue_entry_dup(cell_queue_entry_t *q);
static void cell_queue_entry_free(cell_queue_entry_t *q, int handed_off);
#if 0
static int cell_queue_entry_is_padding(cell_queue_entry_t *q);
#endif
static cell_queue_entry_t *
cell_queue_entry_new_fixed(cell_t *cell);
static cell_queue_entry_t *
cell_queue_entry_new_var(var_cell_t *var_cell);
static int is_destroy_cell(channel_t *chan,
                           const cell_queue_entry_t *q, circid_t *circid_out);

/* Functions to maintain the digest map */
static void channel_add_to_digest_map(channel_t *chan);
static void channel_remove_from_digest_map(channel_t *chan);

/*
 * Flush cells from just the outgoing queue without trying to get them
 * from circuits; used internall by channel_flush_some_cells().
 */
static ssize_t
channel_flush_some_cells_from_outgoing_queue(channel_t *chan,
                                             ssize_t num_cells);
static void channel_force_free(channel_t *chan);
static void
channel_free_list(smartlist_t *channels, int mark_for_close);
static void
channel_listener_free_list(smartlist_t *channels, int mark_for_close);
static void channel_listener_force_free(channel_listener_t *chan_l);
static void
channel_write_cell_queue_entry(channel_t *chan, cell_queue_entry_t *q);

/***********************************
 * Channel state utility functions *
 **********************************/

/**
 * Indicate whether a given channel state is valid
 */

int
channel_state_is_valid(channel_state_t state)
{
  int is_valid;

  switch (state) {
    case CHANNEL_STATE_CLOSED:
    case CHANNEL_STATE_CLOSING:
    case CHANNEL_STATE_ERROR:
    case CHANNEL_STATE_MAINT:
    case CHANNEL_STATE_OPENING:
    case CHANNEL_STATE_OPEN:
      is_valid = 1;
      break;
    case CHANNEL_STATE_LAST:
    default:
      is_valid = 0;
  }

  return is_valid;
}

/**
 * Indicate whether a given channel listener state is valid
 */

int
channel_listener_state_is_valid(channel_listener_state_t state)
{
  int is_valid;

  switch (state) {
    case CHANNEL_LISTENER_STATE_CLOSED:
    case CHANNEL_LISTENER_STATE_LISTENING:
    case CHANNEL_LISTENER_STATE_CLOSING:
    case CHANNEL_LISTENER_STATE_ERROR:
      is_valid = 1;
      break;
    case CHANNEL_LISTENER_STATE_LAST:
    default:
      is_valid = 0;
  }

  return is_valid;
}

/**
 * Indicate whether a channel state transition is valid
 *
 * This function takes two channel states and indicates whether a
 * transition between them is permitted (see the state definitions and
 * transition table in or.h at the channel_state_t typedef).
 */

int
channel_state_can_transition(channel_state_t from, channel_state_t to)
{
  int is_valid;

  switch (from) {
    case CHANNEL_STATE_CLOSED:
      is_valid = (to == CHANNEL_STATE_OPENING);
      break;
    case CHANNEL_STATE_CLOSING:
      is_valid = (to == CHANNEL_STATE_CLOSED ||
                  to == CHANNEL_STATE_ERROR);
      break;
    case CHANNEL_STATE_ERROR:
      is_valid = 0;
      break;
    case CHANNEL_STATE_MAINT:
      is_valid = (to == CHANNEL_STATE_CLOSING ||
                  to == CHANNEL_STATE_ERROR ||
                  to == CHANNEL_STATE_OPEN);
      break;
    case CHANNEL_STATE_OPENING:
      is_valid = (to == CHANNEL_STATE_CLOSING ||
                  to == CHANNEL_STATE_ERROR ||
                  to == CHANNEL_STATE_OPEN);
      break;
    case CHANNEL_STATE_OPEN:
      is_valid = (to == CHANNEL_STATE_CLOSING ||
                  to == CHANNEL_STATE_ERROR ||
                  to == CHANNEL_STATE_MAINT);
      break;
    case CHANNEL_STATE_LAST:
    default:
      is_valid = 0;
  }

  return is_valid;
}

/**
 * Indicate whether a channel listener state transition is valid
 *
 * This function takes two channel listener states and indicates whether a
 * transition between them is permitted (see the state definitions and
 * transition table in or.h at the channel_listener_state_t typedef).
 */

int
channel_listener_state_can_transition(channel_listener_state_t from,
                                      channel_listener_state_t to)
{
  int is_valid;

  switch (from) {
    case CHANNEL_LISTENER_STATE_CLOSED:
      is_valid = (to == CHANNEL_LISTENER_STATE_LISTENING);
      break;
    case CHANNEL_LISTENER_STATE_CLOSING:
      is_valid = (to == CHANNEL_LISTENER_STATE_CLOSED ||
                  to == CHANNEL_LISTENER_STATE_ERROR);
      break;
    case CHANNEL_LISTENER_STATE_ERROR:
      is_valid = 0;
      break;
    case CHANNEL_LISTENER_STATE_LISTENING:
      is_valid = (to == CHANNEL_LISTENER_STATE_CLOSING ||
                  to == CHANNEL_LISTENER_STATE_ERROR);
      break;
    case CHANNEL_LISTENER_STATE_LAST:
    default:
      is_valid = 0;
  }

  return is_valid;
}

/**
 * Return a human-readable description for a channel state
 */

const char *
channel_state_to_string(channel_state_t state)
{
  const char *descr;

  switch (state) {
    case CHANNEL_STATE_CLOSED:
      descr = "closed";
      break;
    case CHANNEL_STATE_CLOSING:
      descr = "closing";
      break;
    case CHANNEL_STATE_ERROR:
      descr = "channel error";
      break;
    case CHANNEL_STATE_MAINT:
      descr = "temporarily suspended for maintenance";
      break;
    case CHANNEL_STATE_OPENING:
      descr = "opening";
      break;
    case CHANNEL_STATE_OPEN:
      descr = "open";
      break;
    case CHANNEL_STATE_LAST:
    default:
      descr = "unknown or invalid channel state";
  }

  return descr;
}

/**
 * Return a human-readable description for a channel listenier state
 */

const char *
channel_listener_state_to_string(channel_listener_state_t state)
{
  const char *descr;

  switch (state) {
    case CHANNEL_LISTENER_STATE_CLOSED:
      descr = "closed";
      break;
    case CHANNEL_LISTENER_STATE_CLOSING:
      descr = "closing";
      break;
    case CHANNEL_LISTENER_STATE_ERROR:
      descr = "channel listener error";
      break;
    case CHANNEL_LISTENER_STATE_LISTENING:
      descr = "listening";
      break;
    case CHANNEL_LISTENER_STATE_LAST:
    default:
      descr = "unknown or invalid channel listener state";
  }

  return descr;
}

/***************************************
 * Channel registration/unregistration *
 ***************************************/

/**
 * Register a channel
 *
 * This function registers a newly created channel in the global lists/maps
 * of active channels.
 */

void
channel_register(channel_t *chan)
{
  tor_assert(chan);

  /* No-op if already registered */
  if (chan->registered) return;

  log_debug(LD_CHANNEL,
            "Registering channel %p (ID " U64_FORMAT ") "
            "in state %s (%d) with digest %s",
            chan, U64_PRINTF_ARG(chan->global_identifier),
            channel_state_to_string(chan->state), chan->state,
            hex_str(chan->identity_digest, DIGEST_LEN));

  /* Make sure we have all_channels, then add it */
  if (!all_channels) all_channels = smartlist_new();
  smartlist_add(all_channels, chan);

  /* Is it finished? */
  if (chan->state == CHANNEL_STATE_CLOSED ||
      chan->state == CHANNEL_STATE_ERROR) {
    /* Put it in the finished list, creating it if necessary */
    if (!finished_channels) finished_channels = smartlist_new();
    smartlist_add(finished_channels, chan);
  } else {
    /* Put it in the active list, creating it if necessary */
    if (!active_channels) active_channels = smartlist_new();
    smartlist_add(active_channels, chan);

    if (chan->state != CHANNEL_STATE_CLOSING) {
      /* It should have a digest set */
      if (!tor_digest_is_zero(chan->identity_digest)) {
        /* Yeah, we're good, add it to the map */
        channel_add_to_digest_map(chan);
      } else {
        log_info(LD_CHANNEL,
                "Channel %p (global ID " U64_FORMAT ") "
                "in state %s (%d) registered with no identity digest",
                chan, U64_PRINTF_ARG(chan->global_identifier),
                channel_state_to_string(chan->state), chan->state);
      }
    }
  }

  /* Mark it as registered */
  chan->registered = 1;
}

/**
 * Unregister a channel
 *
 * This function removes a channel from the global lists and maps and is used
 * when freeing a closed/errored channel.
 */

void
channel_unregister(channel_t *chan)
{
  tor_assert(chan);

  /* No-op if not registered */
  if (!(chan->registered)) return;

  /* Is it finished? */
  if (chan->state == CHANNEL_STATE_CLOSED ||
      chan->state == CHANNEL_STATE_ERROR) {
    /* Get it out of the finished list */
    if (finished_channels) smartlist_remove(finished_channels, chan);
  } else {
    /* Get it out of the active list */
    if (active_channels) smartlist_remove(active_channels, chan);
  }

  /* Get it out of all_channels */
 if (all_channels) smartlist_remove(all_channels, chan);

  /* Mark it as unregistered */
  chan->registered = 0;

  /* Should it be in the digest map? */
  if (!tor_digest_is_zero(chan->identity_digest) &&
      !(chan->state == CHANNEL_STATE_CLOSING ||
        chan->state == CHANNEL_STATE_CLOSED ||
        chan->state == CHANNEL_STATE_ERROR)) {
    /* Remove it */
    channel_remove_from_digest_map(chan);
  }
}

/**
 * Register a channel listener
 *
 * This function registers a newly created channel listner in the global
 * lists/maps of active channel listeners.
 */

void
channel_listener_register(channel_listener_t *chan_l)
{
  tor_assert(chan_l);

  /* No-op if already registered */
  if (chan_l->registered) return;

  log_debug(LD_CHANNEL,
            "Registering channel listener %p (ID " U64_FORMAT ") "
            "in state %s (%d)",
            chan_l, U64_PRINTF_ARG(chan_l->global_identifier),
            channel_listener_state_to_string(chan_l->state),
            chan_l->state);

  /* Make sure we have all_channels, then add it */
  if (!all_listeners) all_listeners = smartlist_new();
  smartlist_add(all_listeners, chan_l);

  /* Is it finished? */
  if (chan_l->state == CHANNEL_LISTENER_STATE_CLOSED ||
      chan_l->state == CHANNEL_LISTENER_STATE_ERROR) {
    /* Put it in the finished list, creating it if necessary */
    if (!finished_listeners) finished_listeners = smartlist_new();
    smartlist_add(finished_listeners, chan_l);
  } else {
    /* Put it in the active list, creating it if necessary */
    if (!active_listeners) active_listeners = smartlist_new();
    smartlist_add(active_listeners, chan_l);
  }

  /* Mark it as registered */
  chan_l->registered = 1;
}

/**
 * Unregister a channel listener
 *
 * This function removes a channel listener from the global lists and maps
 * and is used when freeing a closed/errored channel listener.
 */

void
channel_listener_unregister(channel_listener_t *chan_l)
{
  tor_assert(chan_l);

  /* No-op if not registered */
  if (!(chan_l->registered)) return;

  /* Is it finished? */
  if (chan_l->state == CHANNEL_LISTENER_STATE_CLOSED ||
      chan_l->state == CHANNEL_LISTENER_STATE_ERROR) {
    /* Get it out of the finished list */
    if (finished_listeners) smartlist_remove(finished_listeners, chan_l);
  } else {
    /* Get it out of the active list */
    if (active_listeners) smartlist_remove(active_listeners, chan_l);
  }

  /* Get it out of all_channels */
 if (all_listeners) smartlist_remove(all_listeners, chan_l);

  /* Mark it as unregistered */
  chan_l->registered = 0;
}

/*********************************
 * Channel digest map maintenance
 *********************************/

/**
 * Add a channel to the digest map
 *
 * This function adds a channel to the digest map and inserts it into the
 * correct linked list if channels with that remote endpoint identity digest
 * already exist.
 */

static void
channel_add_to_digest_map(channel_t *chan)
{
  channel_idmap_entry_t *ent, search;

  tor_assert(chan);

  /* Assert that the state makes sense */
  tor_assert(!(chan->state == CHANNEL_STATE_CLOSING ||
               chan->state == CHANNEL_STATE_CLOSED ||
               chan->state == CHANNEL_STATE_ERROR));

  /* Assert that there is a digest */
  tor_assert(!tor_digest_is_zero(chan->identity_digest));

  memcpy(search.digest, chan->identity_digest, DIGEST_LEN);
  ent = HT_FIND(channel_idmap, &channel_identity_map, &search);
  if (! ent) {
    ent = tor_malloc(sizeof(channel_idmap_entry_t));
    memcpy(ent->digest, chan->identity_digest, DIGEST_LEN);
    TOR_LIST_INIT(&ent->channel_list);
    HT_INSERT(channel_idmap, &channel_identity_map, ent);
  }
  TOR_LIST_INSERT_HEAD(&ent->channel_list, chan, next_with_same_id);

  log_debug(LD_CHANNEL,
            "Added channel %p (global ID " U64_FORMAT ") "
            "to identity map in state %s (%d) with digest %s",
            chan, U64_PRINTF_ARG(chan->global_identifier),
            channel_state_to_string(chan->state), chan->state,
            hex_str(chan->identity_digest, DIGEST_LEN));
}

/**
 * Remove a channel from the digest map
 *
 * This function removes a channel from the digest map and the linked list of
 * channels for that digest if more than one exists.
 */

static void
channel_remove_from_digest_map(channel_t *chan)
{
  channel_idmap_entry_t *ent, search;

  tor_assert(chan);

  /* Assert that there is a digest */
  tor_assert(!tor_digest_is_zero(chan->identity_digest));

#if 0
  /* Make sure we have a map */
  if (!channel_identity_map) {
    /*
     * No identity map, so we can't find it by definition.  This
     * case is similar to digestmap_get() failing below.
     */
    log_warn(LD_BUG,
             "Trying to remove channel %p (global ID " U64_FORMAT ") "
             "with digest %s from identity map, but didn't have any identity "
             "map",
             chan, U64_PRINTF_ARG(chan->global_identifier),
             hex_str(chan->identity_digest, DIGEST_LEN));
    /* Clear out its next/prev pointers */
    if (chan->next_with_same_id) {
      chan->next_with_same_id->prev_with_same_id = chan->prev_with_same_id;
    }
    if (chan->prev_with_same_id) {
      chan->prev_with_same_id->next_with_same_id = chan->next_with_same_id;
    }
    chan->next_with_same_id = NULL;
    chan->prev_with_same_id = NULL;

    return;
  }
#endif

  /* Pull it out of its list, wherever that list is */
  TOR_LIST_REMOVE(chan, next_with_same_id);

  memcpy(search.digest, chan->identity_digest, DIGEST_LEN);
  ent = HT_FIND(channel_idmap, &channel_identity_map, &search);

  /* Look for it in the map */
  if (ent) {
    /* Okay, it's here */

    if (TOR_LIST_EMPTY(&ent->channel_list)) {
      HT_REMOVE(channel_idmap, &channel_identity_map, ent);
      tor_free(ent);
    }

    log_debug(LD_CHANNEL,
              "Removed channel %p (global ID " U64_FORMAT ") from "
              "identity map in state %s (%d) with digest %s",
              chan, U64_PRINTF_ARG(chan->global_identifier),
              channel_state_to_string(chan->state), chan->state,
              hex_str(chan->identity_digest, DIGEST_LEN));
  } else {
    /* Shouldn't happen */
    log_warn(LD_BUG,
             "Trying to remove channel %p (global ID " U64_FORMAT ") with "
             "digest %s from identity map, but couldn't find any with "
             "that digest",
             chan, U64_PRINTF_ARG(chan->global_identifier),
             hex_str(chan->identity_digest, DIGEST_LEN));
  }
}

/****************************
 * Channel lookup functions *
 ***************************/

/**
 * Find channel by global ID
 *
 * This function searches for a channel by the global_identifier assigned
 * at initialization time.  This identifier is unique for the lifetime of the
 * Tor process.
 */

channel_t *
channel_find_by_global_id(uint64_t global_identifier)
{
  channel_t *rv = NULL;

  if (all_channels && smartlist_len(all_channels) > 0) {
    SMARTLIST_FOREACH_BEGIN(all_channels, channel_t *, curr) {
      if (curr->global_identifier == global_identifier) {
        rv = curr;
        break;
      }
    } SMARTLIST_FOREACH_END(curr);
  }

  return rv;
}

/**
 * Find channel by digest of the remote endpoint
 *
 * This function looks up a channel by the digest of its remote endpoint in
 * the channel digest map.  It's possible that more than one channel to a
 * given endpoint exists.  Use channel_next_with_digest() to walk the list.
 */

channel_t *
channel_find_by_remote_digest(const char *identity_digest)
{
  channel_t *rv = NULL;
  channel_idmap_entry_t *ent, search;

  tor_assert(identity_digest);

  memcpy(search.digest, identity_digest, DIGEST_LEN);
  ent = HT_FIND(channel_idmap, &channel_identity_map, &search);
  if (ent) {
    rv = TOR_LIST_FIRST(&ent->channel_list);
  }

  return rv;
}

/**
 * Get next channel with digest
 *
 * This function takes a channel and finds the next channel in the list
 * with the same digest.
 */

channel_t *
channel_next_with_digest(channel_t *chan)
{
  tor_assert(chan);

  return TOR_LIST_NEXT(chan, next_with_same_id);
}

/**
 * Initialize a channel
 *
 * This function should be called by subclasses to set up some per-channel
 * variables.  I.e., this is the superclass constructor.  Before this, the
 * channel should be allocated with tor_malloc_zero().
 */

void
channel_init(channel_t *chan)
{
  tor_assert(chan);

  /* Assign an ID and bump the counter */
  chan->global_identifier = n_channels_allocated++;

  /* Init timestamp */
  chan->timestamp_last_had_circuits = time(NULL);

  /* Warn about exhausted circuit IDs no more than hourly. */
  chan->last_warned_circ_ids_exhausted.rate = 3600;

  /* Initialize queues. */
  TOR_SIMPLEQ_INIT(&chan->incoming_queue);
  TOR_SIMPLEQ_INIT(&chan->outgoing_queue);

  /* Initialize list entries. */
  memset(&chan->next_with_same_id, 0, sizeof(chan->next_with_same_id));

  /* Timestamp it */
  channel_timestamp_created(chan);

  /* It hasn't been open yet. */
  chan->has_been_open = 0;
}

/**
 * Initialize a channel listener
 *
 * This function should be called by subclasses to set up some per-channel
 * variables.  I.e., this is the superclass constructor.  Before this, the
 * channel listener should be allocated with tor_malloc_zero().
 */

void
channel_init_listener(channel_listener_t *chan_l)
{
  tor_assert(chan_l);

  /* Assign an ID and bump the counter */
  chan_l->global_identifier = n_channels_allocated++;

  /* Timestamp it */
  channel_listener_timestamp_created(chan_l);
}

/**
 * Free a channel; nothing outside of channel.c and subclasses should call
 * this - it frees channels after they have closed and been unregistered.
 */

void
channel_free(channel_t *chan)
{
  if (!chan) return;

  /* It must be closed or errored */
  tor_assert(chan->state == CHANNEL_STATE_CLOSED ||
             chan->state == CHANNEL_STATE_ERROR);
  /* It must be deregistered */
  tor_assert(!(chan->registered));

  log_debug(LD_CHANNEL,
            "Freeing channel " U64_FORMAT " at %p",
            U64_PRINTF_ARG(chan->global_identifier), chan);

  /*
   * Get rid of cmux policy before we do anything, so cmux policies don't
   * see channels in weird half-freed states.
   */
  if (chan->cmux) {
    circuitmux_set_policy(chan->cmux, NULL);
  }

  /* Call a free method if there is one */
  if (chan->free) chan->free(chan);

  channel_clear_remote_end(chan);

  /* Get rid of cmux */
  if (chan->cmux) {
    circuitmux_detach_all_circuits(chan->cmux, NULL);
    circuitmux_mark_destroyed_circids_usable(chan->cmux, chan);
    circuitmux_free(chan->cmux);
    chan->cmux = NULL;
  }

  /* We're in CLOSED or ERROR, so the cell queue is already empty */

  tor_free(chan);
}

/**
 * Free a channel listener; nothing outside of channel.c and subclasses
 * should call this - it frees channel listeners after they have closed and
 * been unregistered.
 */

void
channel_listener_free(channel_listener_t *chan_l)
{
  if (!chan_l) return;

  log_debug(LD_CHANNEL,
            "Freeing channel_listener_t " U64_FORMAT " at %p",
            U64_PRINTF_ARG(chan_l->global_identifier),
            chan_l);

  /* It must be closed or errored */
  tor_assert(chan_l->state == CHANNEL_LISTENER_STATE_CLOSED ||
             chan_l->state == CHANNEL_LISTENER_STATE_ERROR);
  /* It must be deregistered */
  tor_assert(!(chan_l->registered));

  /* Call a free method if there is one */
  if (chan_l->free) chan_l->free(chan_l);

  /*
   * We're in CLOSED or ERROR, so the incoming channel queue is already
   * empty.
   */

  tor_free(chan_l);
}

/**
 * Free a channel and skip the state/registration asserts; this internal-
 * use-only function should be called only from channel_free_all() when
 * shutting down the Tor process.
 */

static void
channel_force_free(channel_t *chan)
{
  cell_queue_entry_t *cell, *cell_tmp;
  tor_assert(chan);

  log_debug(LD_CHANNEL,
            "Force-freeing channel " U64_FORMAT " at %p",
            U64_PRINTF_ARG(chan->global_identifier), chan);

  /*
   * Get rid of cmux policy before we do anything, so cmux policies don't
   * see channels in weird half-freed states.
   */
  if (chan->cmux) {
    circuitmux_set_policy(chan->cmux, NULL);
  }

  /* Call a free method if there is one */
  if (chan->free) chan->free(chan);

  channel_clear_remote_end(chan);

  /* Get rid of cmux */
  if (chan->cmux) {
    circuitmux_free(chan->cmux);
    chan->cmux = NULL;
  }

  /* We might still have a cell queue; kill it */
  TOR_SIMPLEQ_FOREACH_SAFE(cell, &chan->incoming_queue, next, cell_tmp) {
      cell_queue_entry_free(cell, 0);
  }
  TOR_SIMPLEQ_INIT(&chan->incoming_queue);

  /* Outgoing cell queue is similar, but we can have to free packed cells */
  TOR_SIMPLEQ_FOREACH_SAFE(cell, &chan->outgoing_queue, next, cell_tmp) {
    cell_queue_entry_free(cell, 0);
  }
  TOR_SIMPLEQ_INIT(&chan->outgoing_queue);

  tor_free(chan);
}

/**
 * Free a channel listener and skip the state/reigstration asserts; this
 * internal-use-only function should be called only from channel_free_all()
 * when shutting down the Tor process.
 */

static void
channel_listener_force_free(channel_listener_t *chan_l)
{
  tor_assert(chan_l);

  log_debug(LD_CHANNEL,
            "Force-freeing channel_listener_t " U64_FORMAT " at %p",
            U64_PRINTF_ARG(chan_l->global_identifier),
            chan_l);

  /* Call a free method if there is one */
  if (chan_l->free) chan_l->free(chan_l);

  /*
   * The incoming list just gets emptied and freed; we request close on
   * any channels we find there, but since we got called while shutting
   * down they will get deregistered and freed elsewhere anyway.
   */
  if (chan_l->incoming_list) {
    SMARTLIST_FOREACH_BEGIN(chan_l->incoming_list,
                            channel_t *, qchan) {
      channel_mark_for_close(qchan);
    } SMARTLIST_FOREACH_END(qchan);

    smartlist_free(chan_l->incoming_list);
    chan_l->incoming_list = NULL;
  }

  tor_free(chan_l);
}

/**
 * Return the current registered listener for a channel listener
 *
 * This function returns a function pointer to the current registered
 * handler for new incoming channels on a channel listener.
 */

channel_listener_fn_ptr
channel_listener_get_listener_fn(channel_listener_t *chan_l)
{
  tor_assert(chan_l);

  if (chan_l->state == CHANNEL_LISTENER_STATE_LISTENING)
    return chan_l->listener;

  return NULL;
}

/**
 * Set the listener for a channel listener
 *
 * This function sets the handler for new incoming channels on a channel
 * listener.
 */

void
channel_listener_set_listener_fn(channel_listener_t *chan_l,
                                channel_listener_fn_ptr listener)
{
  tor_assert(chan_l);
  tor_assert(chan_l->state == CHANNEL_LISTENER_STATE_LISTENING);

  log_debug(LD_CHANNEL,
           "Setting listener callback for channel listener %p "
           "(global ID " U64_FORMAT ") to %p",
           chan_l, U64_PRINTF_ARG(chan_l->global_identifier),
           listener);

  chan_l->listener = listener;
  if (chan_l->listener) channel_listener_process_incoming(chan_l);
}

/**
 * Return the fixed-length cell handler for a channel
 *
 * This function gets the handler for incoming fixed-length cells installed
 * on a channel.
 */

channel_cell_handler_fn_ptr
channel_get_cell_handler(channel_t *chan)
{
  tor_assert(chan);

  if (chan->state == CHANNEL_STATE_OPENING ||
      chan->state == CHANNEL_STATE_OPEN ||
      chan->state == CHANNEL_STATE_MAINT)
    return chan->cell_handler;

  return NULL;
}

/**
 * Return the variable-length cell handler for a channel
 *
 * This function gets the handler for incoming variable-length cells
 * installed on a channel.
 */

channel_var_cell_handler_fn_ptr
channel_get_var_cell_handler(channel_t *chan)
{
  tor_assert(chan);

  if (chan->state == CHANNEL_STATE_OPENING ||
      chan->state == CHANNEL_STATE_OPEN ||
      chan->state == CHANNEL_STATE_MAINT)
    return chan->var_cell_handler;

  return NULL;
}

/**
 * Set both cell handlers for a channel
 *
 * This function sets both the fixed-length and variable length cell handlers
 * for a channel and processes any incoming cells that had been blocked in the
 * queue because none were available.
 */

void
channel_set_cell_handlers(channel_t *chan,
                          channel_cell_handler_fn_ptr cell_handler,
                          channel_var_cell_handler_fn_ptr
                            var_cell_handler)
{
  int try_again = 0;

  tor_assert(chan);
  tor_assert(chan->state == CHANNEL_STATE_OPENING ||
             chan->state == CHANNEL_STATE_OPEN ||
             chan->state == CHANNEL_STATE_MAINT);

  log_debug(LD_CHANNEL,
           "Setting cell_handler callback for channel %p to %p",
           chan, cell_handler);
  log_debug(LD_CHANNEL,
           "Setting var_cell_handler callback for channel %p to %p",
           chan, var_cell_handler);

  /* Should we try the queue? */
  if (cell_handler &&
      cell_handler != chan->cell_handler) try_again = 1;
  if (var_cell_handler &&
      var_cell_handler != chan->var_cell_handler) try_again = 1;

  /* Change them */
  chan->cell_handler = cell_handler;
  chan->var_cell_handler = var_cell_handler;

  /* Re-run the queue if we have one and there's any reason to */
  if (! TOR_SIMPLEQ_EMPTY(&chan->incoming_queue) &&
      try_again &&
      (chan->cell_handler ||
       chan->var_cell_handler)) channel_process_cells(chan);
}

/*
 * On closing channels
 *
 * There are three functions that close channels, for use in
 * different circumstances:
 *
 *  - Use channel_mark_for_close() for most cases
 *  - Use channel_close_from_lower_layer() if you are connection_or.c
 *    and the other end closes the underlying connection.
 *  - Use channel_close_for_error() if you are connection_or.c and
 *    some sort of error has occurred.
 */

/**
 * Mark a channel for closure
 *
 * This function tries to close a channel_t; it will go into the CLOSING
 * state, and eventually the lower layer should put it into the CLOSED or
 * ERROR state.  Then, channel_run_cleanup() will eventually free it.
 */

void
channel_mark_for_close(channel_t *chan)
{
  tor_assert(chan != NULL);
  tor_assert(chan->close != NULL);

  /* If it's already in CLOSING, CLOSED or ERROR, this is a no-op */
  if (chan->state == CHANNEL_STATE_CLOSING ||
      chan->state == CHANNEL_STATE_CLOSED ||
      chan->state == CHANNEL_STATE_ERROR) return;

  log_debug(LD_CHANNEL,
            "Closing channel %p (global ID " U64_FORMAT ") "
            "by request",
            chan, U64_PRINTF_ARG(chan->global_identifier));

  /* Note closing by request from above */
  chan->reason_for_closing = CHANNEL_CLOSE_REQUESTED;

  /* Change state to CLOSING */
  channel_change_state(chan, CHANNEL_STATE_CLOSING);

  /* Tell the lower layer */
  chan->close(chan);

  /*
   * It's up to the lower layer to change state to CLOSED or ERROR when we're
   * ready; we'll try to free channels that are in the finished list from
   * channel_run_cleanup().  The lower layer should do this by calling
   * channel_closed().
   */
}

/**
 * Mark a channel listener for closure
 *
 * This function tries to close a channel_listener_t; it will go into the
 * CLOSING state, and eventually the lower layer should put it into the CLOSED
 * or ERROR state.  Then, channel_run_cleanup() will eventually free it.
 */

void
channel_listener_mark_for_close(channel_listener_t *chan_l)
{
  tor_assert(chan_l != NULL);
  tor_assert(chan_l->close != NULL);

  /* If it's already in CLOSING, CLOSED or ERROR, this is a no-op */
  if (chan_l->state == CHANNEL_LISTENER_STATE_CLOSING ||
      chan_l->state == CHANNEL_LISTENER_STATE_CLOSED ||
      chan_l->state == CHANNEL_LISTENER_STATE_ERROR) return;

  log_debug(LD_CHANNEL,
            "Closing channel listener %p (global ID " U64_FORMAT ") "
            "by request",
            chan_l, U64_PRINTF_ARG(chan_l->global_identifier));

  /* Note closing by request from above */
  chan_l->reason_for_closing = CHANNEL_LISTENER_CLOSE_REQUESTED;

  /* Change state to CLOSING */
  channel_listener_change_state(chan_l, CHANNEL_LISTENER_STATE_CLOSING);

  /* Tell the lower layer */
  chan_l->close(chan_l);

  /*
   * It's up to the lower layer to change state to CLOSED or ERROR when we're
   * ready; we'll try to free channels that are in the finished list from
   * channel_run_cleanup().  The lower layer should do this by calling
   * channel_listener_closed().
   */
}

/**
 * Close a channel from the lower layer
 *
 * Notify the channel code that the channel is being closed due to a non-error
 * condition in the lower layer.  This does not call the close() method, since
 * the lower layer already knows.
 */

void
channel_close_from_lower_layer(channel_t *chan)
{
  tor_assert(chan != NULL);

  /* If it's already in CLOSING, CLOSED or ERROR, this is a no-op */
  if (chan->state == CHANNEL_STATE_CLOSING ||
      chan->state == CHANNEL_STATE_CLOSED ||
      chan->state == CHANNEL_STATE_ERROR) return;

  log_debug(LD_CHANNEL,
            "Closing channel %p (global ID " U64_FORMAT ") "
            "due to lower-layer event",
            chan, U64_PRINTF_ARG(chan->global_identifier));

  /* Note closing by event from below */
  chan->reason_for_closing = CHANNEL_CLOSE_FROM_BELOW;

  /* Change state to CLOSING */
  channel_change_state(chan, CHANNEL_STATE_CLOSING);
}

/**
 * Close a channel listener from the lower layer
 *
 * Notify the channel code that the channel listener is being closed due to a
 * non-error condition in the lower layer.  This does not call the close()
 * method, since the lower layer already knows.
 */

void
channel_listener_close_from_lower_layer(channel_listener_t *chan_l)
{
  tor_assert(chan_l != NULL);

  /* If it's already in CLOSING, CLOSED or ERROR, this is a no-op */
  if (chan_l->state == CHANNEL_LISTENER_STATE_CLOSING ||
      chan_l->state == CHANNEL_LISTENER_STATE_CLOSED ||
      chan_l->state == CHANNEL_LISTENER_STATE_ERROR) return;

  log_debug(LD_CHANNEL,
            "Closing channel listener %p (global ID " U64_FORMAT ") "
            "due to lower-layer event",
            chan_l, U64_PRINTF_ARG(chan_l->global_identifier));

  /* Note closing by event from below */
  chan_l->reason_for_closing = CHANNEL_LISTENER_CLOSE_FROM_BELOW;

  /* Change state to CLOSING */
  channel_listener_change_state(chan_l, CHANNEL_LISTENER_STATE_CLOSING);
}

/**
 * Notify that the channel is being closed due to an error condition
 *
 * This function is called by the lower layer implementing the transport
 * when a channel must be closed due to an error condition.  This does not
 * call the channel's close method, since the lower layer already knows.
 */

void
channel_close_for_error(channel_t *chan)
{
  tor_assert(chan != NULL);

  /* If it's already in CLOSING, CLOSED or ERROR, this is a no-op */
  if (chan->state == CHANNEL_STATE_CLOSING ||
      chan->state == CHANNEL_STATE_CLOSED ||
      chan->state == CHANNEL_STATE_ERROR) return;

  log_debug(LD_CHANNEL,
            "Closing channel %p due to lower-layer error",
            chan);

  /* Note closing by event from below */
  chan->reason_for_closing = CHANNEL_CLOSE_FOR_ERROR;

  /* Change state to CLOSING */
  channel_change_state(chan, CHANNEL_STATE_CLOSING);
}

/**
 * Notify that the channel listener is being closed due to an error condition
 *
 * This function is called by the lower layer implementing the transport
 * when a channel listener must be closed due to an error condition.  This
 * does not call the channel listener's close method, since the lower layer
 * already knows.
 */

void
channel_listener_close_for_error(channel_listener_t *chan_l)
{
  tor_assert(chan_l != NULL);

  /* If it's already in CLOSING, CLOSED or ERROR, this is a no-op */
  if (chan_l->state == CHANNEL_LISTENER_STATE_CLOSING ||
      chan_l->state == CHANNEL_LISTENER_STATE_CLOSED ||
      chan_l->state == CHANNEL_LISTENER_STATE_ERROR) return;

  log_debug(LD_CHANNEL,
            "Closing channel listener %p (global ID " U64_FORMAT ") "
            "due to lower-layer error",
            chan_l, U64_PRINTF_ARG(chan_l->global_identifier));

  /* Note closing by event from below */
  chan_l->reason_for_closing = CHANNEL_LISTENER_CLOSE_FOR_ERROR;

  /* Change state to CLOSING */
  channel_listener_change_state(chan_l, CHANNEL_LISTENER_STATE_CLOSING);
}

/**
 * Notify that the lower layer is finished closing the channel
 *
 * This function should be called by the lower layer when a channel
 * is finished closing and it should be regarded as inactive and
 * freed by the channel code.
 */

void
channel_closed(channel_t *chan)
{
  tor_assert(chan);
  tor_assert(chan->state == CHANNEL_STATE_CLOSING ||
             chan->state == CHANNEL_STATE_CLOSED ||
             chan->state == CHANNEL_STATE_ERROR);

  /* No-op if already inactive */
  if (chan->state == CHANNEL_STATE_CLOSED ||
      chan->state == CHANNEL_STATE_ERROR) return;

  /* Inform any pending (not attached) circs that they should
   * give up. */
  if (! chan->has_been_open)
    circuit_n_chan_done(chan, 0);

  /* Now close all the attached circuits on it. */
  circuit_unlink_all_from_channel(chan, END_CIRC_REASON_CHANNEL_CLOSED);

  if (chan->reason_for_closing != CHANNEL_CLOSE_FOR_ERROR) {
    channel_change_state(chan, CHANNEL_STATE_CLOSED);
  } else {
    channel_change_state(chan, CHANNEL_STATE_ERROR);
  }
}

/**
 * Notify that the lower layer is finished closing the channel listener
 *
 * This function should be called by the lower layer when a channel listener
 * is finished closing and it should be regarded as inactive and
 * freed by the channel code.
 */

void
channel_listener_closed(channel_listener_t *chan_l)
{
  tor_assert(chan_l);
  tor_assert(chan_l->state == CHANNEL_LISTENER_STATE_CLOSING ||
             chan_l->state == CHANNEL_LISTENER_STATE_CLOSED ||
             chan_l->state == CHANNEL_LISTENER_STATE_ERROR);

  /* No-op if already inactive */
  if (chan_l->state == CHANNEL_LISTENER_STATE_CLOSED ||
      chan_l->state == CHANNEL_LISTENER_STATE_ERROR) return;

  if (chan_l->reason_for_closing != CHANNEL_LISTENER_CLOSE_FOR_ERROR) {
    channel_listener_change_state(chan_l, CHANNEL_LISTENER_STATE_CLOSED);
  } else {
    channel_listener_change_state(chan_l, CHANNEL_LISTENER_STATE_ERROR);
  }
}

/**
 * Clear the identity_digest of a channel
 *
 * This function clears the identity digest of the remote endpoint for a
 * channel; this is intended for use by the lower layer.
 */

void
channel_clear_identity_digest(channel_t *chan)
{
  int state_not_in_map;

  tor_assert(chan);

  log_debug(LD_CHANNEL,
            "Clearing remote endpoint digest on channel %p with "
            "global ID " U64_FORMAT,
            chan, U64_PRINTF_ARG(chan->global_identifier));

  state_not_in_map =
    (chan->state == CHANNEL_STATE_CLOSING ||
     chan->state == CHANNEL_STATE_CLOSED ||
     chan->state == CHANNEL_STATE_ERROR);

  if (!state_not_in_map && chan->registered &&
      !tor_digest_is_zero(chan->identity_digest))
    /* if it's registered get it out of the digest map */
    channel_remove_from_digest_map(chan);

  memset(chan->identity_digest, 0,
         sizeof(chan->identity_digest));
}

/**
 * Set the identity_digest of a channel
 *
 * This function sets the identity digest of the remote endpoint for a
 * channel; this is intended for use by the lower layer.
 */

void
channel_set_identity_digest(channel_t *chan,
                            const char *identity_digest)
{
  int was_in_digest_map, should_be_in_digest_map, state_not_in_map;

  tor_assert(chan);

  log_debug(LD_CHANNEL,
            "Setting remote endpoint digest on channel %p with "
            "global ID " U64_FORMAT " to digest %s",
            chan, U64_PRINTF_ARG(chan->global_identifier),
            identity_digest ?
              hex_str(identity_digest, DIGEST_LEN) : "(null)");

  state_not_in_map =
    (chan->state == CHANNEL_STATE_CLOSING ||
     chan->state == CHANNEL_STATE_CLOSED ||
     chan->state == CHANNEL_STATE_ERROR);
  was_in_digest_map =
    !state_not_in_map &&
    chan->registered &&
    !tor_digest_is_zero(chan->identity_digest);
  should_be_in_digest_map =
    !state_not_in_map &&
    chan->registered &&
    (identity_digest &&
     !tor_digest_is_zero(identity_digest));

  if (was_in_digest_map)
    /* We should always remove it; we'll add it back if we're writing
     * in a new digest.
     */
    channel_remove_from_digest_map(chan);

  if (identity_digest) {
    memcpy(chan->identity_digest,
           identity_digest,
           sizeof(chan->identity_digest));
  } else {
    memset(chan->identity_digest, 0,
           sizeof(chan->identity_digest));
  }

  /* Put it in the digest map if we should */
  if (should_be_in_digest_map)
    channel_add_to_digest_map(chan);
}

/**
 * Clear the remote end metadata (identity_digest/nickname) of a channel
 *
 * This function clears all the remote end info from a channel; this is
 * intended for use by the lower layer.
 */

void
channel_clear_remote_end(channel_t *chan)
{
  int state_not_in_map;

  tor_assert(chan);

  log_debug(LD_CHANNEL,
            "Clearing remote endpoint identity on channel %p with "
            "global ID " U64_FORMAT,
            chan, U64_PRINTF_ARG(chan->global_identifier));

  state_not_in_map =
    (chan->state == CHANNEL_STATE_CLOSING ||
     chan->state == CHANNEL_STATE_CLOSED ||
     chan->state == CHANNEL_STATE_ERROR);

  if (!state_not_in_map && chan->registered &&
      !tor_digest_is_zero(chan->identity_digest))
    /* if it's registered get it out of the digest map */
    channel_remove_from_digest_map(chan);

  memset(chan->identity_digest, 0,
         sizeof(chan->identity_digest));
  tor_free(chan->nickname);
}

/**
 * Set the remote end metadata (identity_digest/nickname) of a channel
 *
 * This function sets new remote end info on a channel; this is intended
 * for use by the lower layer.
 */

void
channel_set_remote_end(channel_t *chan,
                       const char *identity_digest,
                       const char *nickname)
{
  int was_in_digest_map, should_be_in_digest_map, state_not_in_map;

  tor_assert(chan);

  log_debug(LD_CHANNEL,
            "Setting remote endpoint identity on channel %p with "
            "global ID " U64_FORMAT " to nickname %s, digest %s",
            chan, U64_PRINTF_ARG(chan->global_identifier),
            nickname ? nickname : "(null)",
            identity_digest ?
              hex_str(identity_digest, DIGEST_LEN) : "(null)");

  state_not_in_map =
    (chan->state == CHANNEL_STATE_CLOSING ||
     chan->state == CHANNEL_STATE_CLOSED ||
     chan->state == CHANNEL_STATE_ERROR);
  was_in_digest_map =
    !state_not_in_map &&
    chan->registered &&
    !tor_digest_is_zero(chan->identity_digest);
  should_be_in_digest_map =
    !state_not_in_map &&
    chan->registered &&
    (identity_digest &&
     !tor_digest_is_zero(identity_digest));

  if (was_in_digest_map)
    /* We should always remove it; we'll add it back if we're writing
     * in a new digest.
     */
    channel_remove_from_digest_map(chan);

  if (identity_digest) {
    memcpy(chan->identity_digest,
           identity_digest,
           sizeof(chan->identity_digest));

  } else {
    memset(chan->identity_digest, 0,
           sizeof(chan->identity_digest));
  }

  tor_free(chan->nickname);
  if (nickname)
    chan->nickname = tor_strdup(nickname);

  /* Put it in the digest map if we should */
  if (should_be_in_digest_map)
    channel_add_to_digest_map(chan);
}

/**
 * Duplicate a cell queue entry; this is a shallow copy intended for use
 * in channel_write_cell_queue_entry().
 */

static cell_queue_entry_t *
cell_queue_entry_dup(cell_queue_entry_t *q)
{
  cell_queue_entry_t *rv = NULL;

  tor_assert(q);

  rv = tor_malloc(sizeof(*rv));
  memcpy(rv, q, sizeof(*rv));

  return rv;
}

/**
 * Free a cell_queue_entry_t; the handed_off parameter indicates whether
 * the contents were passed to the lower layer (it is responsible for
 * them) or not (we should free).
 */

static void
cell_queue_entry_free(cell_queue_entry_t *q, int handed_off)
{
  if (!q) return;

  if (!handed_off) {
    /*
     * If we handed it off, the recipient becomes responsible (or
     * with packed cells the channel_t subclass calls packed_cell
     * free after writing out its contents; see, e.g.,
     * channel_tls_write_packed_cell_method().  Otherwise, we have
     * to take care of it here if possible.
     */
    switch (q->type) {
      case CELL_QUEUE_FIXED:
        if (q->u.fixed.cell) {
          /*
           * There doesn't seem to be a cell_free() function anywhere in the
           * pre-channel code; just use tor_free()
           */
          tor_free(q->u.fixed.cell);
        }
        break;
      case CELL_QUEUE_PACKED:
        if (q->u.packed.packed_cell) {
          packed_cell_free(q->u.packed.packed_cell);
        }
        break;
      case CELL_QUEUE_VAR:
        if (q->u.var.var_cell) {
          /*
           * This one's in connection_or.c; it'd be nice to figure out the
           * whole flow of cells from one end to the other and factor the
           * cell memory management functions like this out of the specific
           * TLS lower layer.
           */
          var_cell_free(q->u.var.var_cell);
        }
        break;
      default:
        /*
         * Nothing we can do if we don't know the type; this will
         * have been warned about elsewhere.
         */
        break;
    }
  }
  tor_free(q);
}

#if 0
/**
 * Check whether a cell queue entry is padding; this is a helper function
 * for channel_write_cell_queue_entry()
 */

static int
cell_queue_entry_is_padding(cell_queue_entry_t *q)
{
  tor_assert(q);

  if (q->type == CELL_QUEUE_FIXED) {
    if (q->u.fixed.cell) {
      if (q->u.fixed.cell->command == CELL_PADDING ||
          q->u.fixed.cell->command == CELL_VPADDING) {
        return 1;
      }
    }
  } else if (q->type == CELL_QUEUE_VAR) {
    if (q->u.var.var_cell) {
      if (q->u.var.var_cell->command == CELL_PADDING ||
          q->u.var.var_cell->command == CELL_VPADDING) {
        return 1;
      }
    }
  }

  return 0;
}
#endif

/**
 * Allocate a new cell queue entry for a fixed-size cell
 */

static cell_queue_entry_t *
cell_queue_entry_new_fixed(cell_t *cell)
{
  cell_queue_entry_t *q = NULL;

  tor_assert(cell);

  q = tor_malloc(sizeof(*q));
  q->type = CELL_QUEUE_FIXED;
  q->u.fixed.cell = cell;

  return q;
}

/**
 * Allocate a new cell queue entry for a variable-size cell
 */

static cell_queue_entry_t *
cell_queue_entry_new_var(var_cell_t *var_cell)
{
  cell_queue_entry_t *q = NULL;

  tor_assert(var_cell);

  q = tor_malloc(sizeof(*q));
  q->type = CELL_QUEUE_VAR;
  q->u.var.var_cell = var_cell;

  return q;
}

/**
 * Write to a channel based on a cell_queue_entry_t
 *
 * Given a cell_queue_entry_t filled out by the caller, try to send the cell
 * and queue it if we can't.
 */

static void
channel_write_cell_queue_entry(channel_t *chan, cell_queue_entry_t *q)
{
  int result = 0, sent = 0;
  cell_queue_entry_t *tmp = NULL;

  tor_assert(chan);
  tor_assert(q);

  /* Assert that the state makes sense for a cell write */
  tor_assert(chan->state == CHANNEL_STATE_OPENING ||
             chan->state == CHANNEL_STATE_OPEN ||
             chan->state == CHANNEL_STATE_MAINT);

  {
    circid_t circ_id;
    if (is_destroy_cell(chan, q, &circ_id)) {
      channel_note_destroy_not_pending(chan, circ_id);
    }
  }

  /* Can we send it right out?  If so, try */
  if (TOR_SIMPLEQ_EMPTY(&chan->outgoing_queue) &&
      chan->state == CHANNEL_STATE_OPEN) {
    /* Pick the right write function for this cell type and save the result */
    switch (q->type) {
      case CELL_QUEUE_FIXED:
        tor_assert(chan->write_cell);
        tor_assert(q->u.fixed.cell);
        result = chan->write_cell(chan, q->u.fixed.cell);
        break;
      case CELL_QUEUE_PACKED:
        tor_assert(chan->write_packed_cell);
        tor_assert(q->u.packed.packed_cell);
        result = chan->write_packed_cell(chan, q->u.packed.packed_cell);
        break;
      case CELL_QUEUE_VAR:
        tor_assert(chan->write_var_cell);
        tor_assert(q->u.var.var_cell);
        result = chan->write_var_cell(chan, q->u.var.var_cell);
        break;
      default:
        tor_assert(1);
    }

    /* Check if we got it out */
    if (result > 0) {
      sent = 1;
      /* Timestamp for transmission */
      channel_timestamp_xmit(chan);
      /* If we're here the queue is empty, so it's drained too */
      channel_timestamp_drained(chan);
      /* Update the counter */
      ++(chan->n_cells_xmitted);
    }
  }

  if (!sent) {
    /* Not sent, queue it */
    /*
     * We have to copy the queue entry passed in, since the caller probably
     * used the stack.
     */
    tmp = cell_queue_entry_dup(q);
    TOR_SIMPLEQ_INSERT_TAIL(&chan->outgoing_queue, tmp, next);
    /* Try to process the queue? */
    if (chan->state == CHANNEL_STATE_OPEN) channel_flush_cells(chan);
  }
}

/**
 * Write a cell to a channel
 *
 * Write a fixed-length cell to a channel using the write_cell() method.
 * This is equivalent to the pre-channels connection_or_write_cell_to_buf();
 * it is called by the transport-independent code to deliver a cell to a
 * channel for transmission.
 */

void
channel_write_cell(channel_t *chan, cell_t *cell)
{
  cell_queue_entry_t q;

  tor_assert(chan);
  tor_assert(cell);

  if (chan->state == CHANNEL_STATE_CLOSING) {
    log_debug(LD_CHANNEL, "Discarding cell_t %p on closing channel %p with "
              "global ID "U64_FORMAT, cell, chan,
              U64_PRINTF_ARG(chan->global_identifier));
    tor_free(cell);
    return;
  }

  log_debug(LD_CHANNEL,
            "Writing cell_t %p to channel %p with global ID "
            U64_FORMAT,
            cell, chan, U64_PRINTF_ARG(chan->global_identifier));

  q.type = CELL_QUEUE_FIXED;
  q.u.fixed.cell = cell;
  channel_write_cell_queue_entry(chan, &q);
}

/**
 * Write a packed cell to a channel
 *
 * Write a packed cell to a channel using the write_cell() method.  This is
 * called by the transport-independent code to deliver a packed cell to a
 * channel for transmission.
 */

void
channel_write_packed_cell(channel_t *chan, packed_cell_t *packed_cell)
{
  cell_queue_entry_t q;

  tor_assert(chan);
  tor_assert(packed_cell);

  if (chan->state == CHANNEL_STATE_CLOSING) {
    log_debug(LD_CHANNEL, "Discarding packed_cell_t %p on closing channel %p "
              "with global ID "U64_FORMAT, packed_cell, chan,
              U64_PRINTF_ARG(chan->global_identifier));
    packed_cell_free(packed_cell);
    return;
  }

  log_debug(LD_CHANNEL,
            "Writing packed_cell_t %p to channel %p with global ID "
            U64_FORMAT,
            packed_cell, chan,
            U64_PRINTF_ARG(chan->global_identifier));

  q.type = CELL_QUEUE_PACKED;
  q.u.packed.packed_cell = packed_cell;
  channel_write_cell_queue_entry(chan, &q);
}

/**
 * Write a variable-length cell to a channel
 *
 * Write a variable-length cell to a channel using the write_cell() method.
 * This is equivalent to the pre-channels
 * connection_or_write_var_cell_to_buf(); it's called by the transport-
 * independent code to deliver a var_cell to a channel for transmission.
 */

void
channel_write_var_cell(channel_t *chan, var_cell_t *var_cell)
{
  cell_queue_entry_t q;

  tor_assert(chan);
  tor_assert(var_cell);

  if (chan->state == CHANNEL_STATE_CLOSING) {
    log_debug(LD_CHANNEL, "Discarding var_cell_t %p on closing channel %p "
              "with global ID "U64_FORMAT, var_cell, chan,
              U64_PRINTF_ARG(chan->global_identifier));
    var_cell_free(var_cell);
    return;
  }

  log_debug(LD_CHANNEL,
            "Writing var_cell_t %p to channel %p with global ID "
            U64_FORMAT,
            var_cell, chan,
            U64_PRINTF_ARG(chan->global_identifier));

  q.type = CELL_QUEUE_VAR;
  q.u.var.var_cell = var_cell;
  channel_write_cell_queue_entry(chan, &q);
}

/**
 * Change channel state
 *
 * This internal and subclass use only function is used to change channel
 * state, performing all transition validity checks and whatever actions
 * are appropriate to the state transition in question.
 */

void
channel_change_state(channel_t *chan, channel_state_t to_state)
{
  channel_state_t from_state;
  unsigned char was_active, is_active;
  unsigned char was_in_id_map, is_in_id_map;

  tor_assert(chan);
  from_state = chan->state;

  tor_assert(channel_state_is_valid(from_state));
  tor_assert(channel_state_is_valid(to_state));
  tor_assert(channel_state_can_transition(chan->state, to_state));

  /* Check for no-op transitions */
  if (from_state == to_state) {
    log_debug(LD_CHANNEL,
              "Got no-op transition from \"%s\" to itself on channel %p"
              "(global ID " U64_FORMAT ")",
              channel_state_to_string(to_state),
              chan, U64_PRINTF_ARG(chan->global_identifier));
    return;
  }

  /* If we're going to a closing or closed state, we must have a reason set */
  if (to_state == CHANNEL_STATE_CLOSING ||
      to_state == CHANNEL_STATE_CLOSED ||
      to_state == CHANNEL_STATE_ERROR) {
    tor_assert(chan->reason_for_closing != CHANNEL_NOT_CLOSING);
  }

  /*
   * We need to maintain the queues here for some transitions:
   * when we enter CHANNEL_STATE_OPEN (especially from CHANNEL_STATE_MAINT)
   * we may have a backlog of cells to transmit, so drain the queues in
   * that case, and when going to CHANNEL_STATE_CLOSED the subclass
   * should have made sure to finish sending things (or gone to
   * CHANNEL_STATE_ERROR if not possible), so we assert for that here.
   */

  log_debug(LD_CHANNEL,
            "Changing state of channel %p (global ID " U64_FORMAT
            ") from \"%s\" to \"%s\"",
            chan,
            U64_PRINTF_ARG(chan->global_identifier),
            channel_state_to_string(chan->state),
            channel_state_to_string(to_state));

  chan->state = to_state;

  /* Need to add to the right lists if the channel is registered */
  if (chan->registered) {
    was_active = !(from_state == CHANNEL_STATE_CLOSED ||
                   from_state == CHANNEL_STATE_ERROR);
    is_active = !(to_state == CHANNEL_STATE_CLOSED ||
                  to_state == CHANNEL_STATE_ERROR);

    /* Need to take off active list and put on finished list? */
    if (was_active && !is_active) {
      if (active_channels) smartlist_remove(active_channels, chan);
      if (!finished_channels) finished_channels = smartlist_new();
      smartlist_add(finished_channels, chan);
    }
    /* Need to put on active list? */
    else if (!was_active && is_active) {
      if (finished_channels) smartlist_remove(finished_channels, chan);
      if (!active_channels) active_channels = smartlist_new();
      smartlist_add(active_channels, chan);
    }

    if (!tor_digest_is_zero(chan->identity_digest)) {
      /* Now we need to handle the identity map */
      was_in_id_map = !(from_state == CHANNEL_STATE_CLOSING ||
                        from_state == CHANNEL_STATE_CLOSED ||
                        from_state == CHANNEL_STATE_ERROR);
      is_in_id_map = !(to_state == CHANNEL_STATE_CLOSING ||
                       to_state == CHANNEL_STATE_CLOSED ||
                       to_state == CHANNEL_STATE_ERROR);

      if (!was_in_id_map && is_in_id_map) channel_add_to_digest_map(chan);
      else if (was_in_id_map && !is_in_id_map)
        channel_remove_from_digest_map(chan);
    }
  }

  /* Tell circuits if we opened and stuff */
  if (to_state == CHANNEL_STATE_OPEN) {
    channel_do_open_actions(chan);
    chan->has_been_open = 1;

    /* Check for queued cells to process */
    if (! TOR_SIMPLEQ_EMPTY(&chan->incoming_queue))
      channel_process_cells(chan);
    if (! TOR_SIMPLEQ_EMPTY(&chan->outgoing_queue))
      channel_flush_cells(chan);
  } else if (to_state == CHANNEL_STATE_CLOSED ||
             to_state == CHANNEL_STATE_ERROR) {
    /* Assert that all queues are empty */
    tor_assert(TOR_SIMPLEQ_EMPTY(&chan->incoming_queue));
    tor_assert(TOR_SIMPLEQ_EMPTY(&chan->outgoing_queue));
  }
}

/**
 * Change channel listener state
 *
 * This internal and subclass use only function is used to change channel
 * listener state, performing all transition validity checks and whatever
 * actions are appropriate to the state transition in question.
 */

void
channel_listener_change_state(channel_listener_t *chan_l,
                              channel_listener_state_t to_state)
{
  channel_listener_state_t from_state;
  unsigned char was_active, is_active;

  tor_assert(chan_l);
  from_state = chan_l->state;

  tor_assert(channel_listener_state_is_valid(from_state));
  tor_assert(channel_listener_state_is_valid(to_state));
  tor_assert(channel_listener_state_can_transition(chan_l->state, to_state));

  /* Check for no-op transitions */
  if (from_state == to_state) {
    log_debug(LD_CHANNEL,
              "Got no-op transition from \"%s\" to itself on channel "
              "listener %p (global ID " U64_FORMAT ")",
              channel_listener_state_to_string(to_state),
              chan_l, U64_PRINTF_ARG(chan_l->global_identifier));
    return;
  }

  /* If we're going to a closing or closed state, we must have a reason set */
  if (to_state == CHANNEL_LISTENER_STATE_CLOSING ||
      to_state == CHANNEL_LISTENER_STATE_CLOSED ||
      to_state == CHANNEL_LISTENER_STATE_ERROR) {
    tor_assert(chan_l->reason_for_closing != CHANNEL_LISTENER_NOT_CLOSING);
  }

  /*
   * We need to maintain the queues here for some transitions:
   * when we enter CHANNEL_STATE_OPEN (especially from CHANNEL_STATE_MAINT)
   * we may have a backlog of cells to transmit, so drain the queues in
   * that case, and when going to CHANNEL_STATE_CLOSED the subclass
   * should have made sure to finish sending things (or gone to
   * CHANNEL_STATE_ERROR if not possible), so we assert for that here.
   */

  log_debug(LD_CHANNEL,
            "Changing state of channel listener %p (global ID " U64_FORMAT
            "from \"%s\" to \"%s\"",
            chan_l, U64_PRINTF_ARG(chan_l->global_identifier),
            channel_listener_state_to_string(chan_l->state),
            channel_listener_state_to_string(to_state));

  chan_l->state = to_state;

  /* Need to add to the right lists if the channel listener is registered */
  if (chan_l->registered) {
    was_active = !(from_state == CHANNEL_LISTENER_STATE_CLOSED ||
                   from_state == CHANNEL_LISTENER_STATE_ERROR);
    is_active = !(to_state == CHANNEL_LISTENER_STATE_CLOSED ||
                  to_state == CHANNEL_LISTENER_STATE_ERROR);

    /* Need to take off active list and put on finished list? */
    if (was_active && !is_active) {
      if (active_listeners) smartlist_remove(active_listeners, chan_l);
      if (!finished_listeners) finished_listeners = smartlist_new();
      smartlist_add(finished_listeners, chan_l);
    }
    /* Need to put on active list? */
    else if (!was_active && is_active) {
      if (finished_listeners) smartlist_remove(finished_listeners, chan_l);
      if (!active_listeners) active_listeners = smartlist_new();
      smartlist_add(active_listeners, chan_l);
    }
  }

  if (to_state == CHANNEL_LISTENER_STATE_CLOSED ||
      to_state == CHANNEL_LISTENER_STATE_ERROR) {
    /* Assert that the queue is empty */
    tor_assert(!(chan_l->incoming_list) ||
                smartlist_len(chan_l->incoming_list) == 0);
  }
}

/**
 * Try to flush cells to the lower layer
 *
 * this is called by the lower layer to indicate that it wants more cells;
 * it will try to write up to num_cells cells from the channel's cell queue or
 * from circuits active on that channel, or as many as it has available if
 * num_cells == -1.
 */

#define MAX_CELLS_TO_GET_FROM_CIRCUITS_FOR_UNLIMITED 256

ssize_t
channel_flush_some_cells(channel_t *chan, ssize_t num_cells)
{
  unsigned int unlimited = 0;
  ssize_t flushed = 0;
  int num_cells_from_circs, clamped_num_cells;

  tor_assert(chan);

  if (num_cells < 0) unlimited = 1;
  if (!unlimited && num_cells <= flushed) goto done;

  /* If we aren't in CHANNEL_STATE_OPEN, nothing goes through */
  if (chan->state == CHANNEL_STATE_OPEN) {
    /* Try to flush as much as we can that's already queued */
    flushed += channel_flush_some_cells_from_outgoing_queue(chan,
        (unlimited ? -1 : num_cells - flushed));
    if (!unlimited && num_cells <= flushed) goto done;

    if (circuitmux_num_cells(chan->cmux) > 0) {
      /* Calculate number of cells, including clamp */
      if (unlimited) {
        clamped_num_cells = MAX_CELLS_TO_GET_FROM_CIRCUITS_FOR_UNLIMITED;
      } else {
        if (num_cells - flushed >
            MAX_CELLS_TO_GET_FROM_CIRCUITS_FOR_UNLIMITED) {
          clamped_num_cells = MAX_CELLS_TO_GET_FROM_CIRCUITS_FOR_UNLIMITED;
        } else {
          clamped_num_cells = (int)(num_cells - flushed);
        }
      }
      /* Try to get more cells from any active circuits */
      num_cells_from_circs = channel_flush_from_first_active_circuit(
          chan, clamped_num_cells);

      /* If it claims we got some, process the queue again */
      if (num_cells_from_circs > 0) {
        flushed += channel_flush_some_cells_from_outgoing_queue(chan,
          (unlimited ? -1 : num_cells - flushed));
      }
    }
  }

 done:
  return flushed;
}

/**
 * Flush cells from just the channel's outgoing cell queue
 *
 * This gets called from channel_flush_some_cells() above to flush cells
 * just from the queue without trying for active_circuits.
 */

static ssize_t
channel_flush_some_cells_from_outgoing_queue(channel_t *chan,
                                             ssize_t num_cells)
{
  unsigned int unlimited = 0;
  ssize_t flushed = 0;
  cell_queue_entry_t *q = NULL;

  tor_assert(chan);
  tor_assert(chan->write_cell);
  tor_assert(chan->write_packed_cell);
  tor_assert(chan->write_var_cell);

  if (num_cells < 0) unlimited = 1;
  if (!unlimited && num_cells <= flushed) return 0;

  /* If we aren't in CHANNEL_STATE_OPEN, nothing goes through */
  if (chan->state == CHANNEL_STATE_OPEN) {
    while ((unlimited || num_cells > flushed) &&
           NULL != (q = TOR_SIMPLEQ_FIRST(&chan->outgoing_queue))) {

      if (1) {
        /*
         * Okay, we have a good queue entry, try to give it to the lower
         * layer.
         */
        switch (q->type) {
          case CELL_QUEUE_FIXED:
            if (q->u.fixed.cell) {
              if (chan->write_cell(chan,
                    q->u.fixed.cell)) {
                ++flushed;
                channel_timestamp_xmit(chan);
                ++(chan->n_cells_xmitted);
                cell_queue_entry_free(q, 1);
                q = NULL;
              }
              /* Else couldn't write it; leave it on the queue */
            } else {
              /* This shouldn't happen */
              log_info(LD_CHANNEL,
                       "Saw broken cell queue entry of type CELL_QUEUE_FIXED "
                       "with no cell on channel %p "
                       "(global ID " U64_FORMAT ").",
                       chan, U64_PRINTF_ARG(chan->global_identifier));
              /* Throw it away */
              cell_queue_entry_free(q, 0);
              q = NULL;
            }
            break;
         case CELL_QUEUE_PACKED:
            if (q->u.packed.packed_cell) {
              if (chan->write_packed_cell(chan,
                    q->u.packed.packed_cell)) {
                ++flushed;
                channel_timestamp_xmit(chan);
                ++(chan->n_cells_xmitted);
                cell_queue_entry_free(q, 1);
                q = NULL;
              }
              /* Else couldn't write it; leave it on the queue */
            } else {
              /* This shouldn't happen */
              log_info(LD_CHANNEL,
                       "Saw broken cell queue entry of type CELL_QUEUE_PACKED "
                       "with no cell on channel %p "
                       "(global ID " U64_FORMAT ").",
                       chan, U64_PRINTF_ARG(chan->global_identifier));
              /* Throw it away */
              cell_queue_entry_free(q, 0);
              q = NULL;
            }
            break;
         case CELL_QUEUE_VAR:
            if (q->u.var.var_cell) {
              if (chan->write_var_cell(chan,
                    q->u.var.var_cell)) {
                ++flushed;
                channel_timestamp_xmit(chan);
                ++(chan->n_cells_xmitted);
                cell_queue_entry_free(q, 1);
                q = NULL;
              }
              /* Else couldn't write it; leave it on the queue */
            } else {
              /* This shouldn't happen */
              log_info(LD_CHANNEL,
                       "Saw broken cell queue entry of type CELL_QUEUE_VAR "
                       "with no cell on channel %p "
                       "(global ID " U64_FORMAT ").",
                       chan, U64_PRINTF_ARG(chan->global_identifier));
              /* Throw it away */
              cell_queue_entry_free(q, 0);
              q = NULL;
            }
            break;
          default:
            /* Unknown type, log and free it */
            log_info(LD_CHANNEL,
                     "Saw an unknown cell queue entry type %d on channel %p "
                     "(global ID " U64_FORMAT "; ignoring it."
                     "  Someone should fix this.",
                     q->type, chan, U64_PRINTF_ARG(chan->global_identifier));
            cell_queue_entry_free(q, 0);
            q = NULL;
        }

        /* if q got NULLed out, we used it and should remove the queue entry */
        if (!q) TOR_SIMPLEQ_REMOVE_HEAD(&chan->outgoing_queue, next);
        /* No cell removed from list, so we can't go on any further */
        else break;
      }
    }
  }

  /* Did we drain the queue? */
  if (TOR_SIMPLEQ_EMPTY(&chan->outgoing_queue)) {
    channel_timestamp_drained(chan);
  }

  return flushed;
}

/**
 * Flush as many cells as we possibly can from the queue
 *
 * This tries to flush as many cells from the queue as the lower layer
 * will take.  It just calls channel_flush_some_cells_from_outgoing_queue()
 * in unlimited mode.
 */

void
channel_flush_cells(channel_t *chan)
{
  channel_flush_some_cells_from_outgoing_queue(chan, -1);
}

/**
 * Check if any cells are available
 *
 * This gets used from the lower layer to check if any more cells are
 * available.
 */

int
channel_more_to_flush(channel_t *chan)
{
  tor_assert(chan);

  /* Check if we have any queued */
  if (! TOR_SIMPLEQ_EMPTY(&chan->incoming_queue))
      return 1;

  /* Check if any circuits would like to queue some */
  if (circuitmux_num_cells(chan->cmux) > 0) return 1;

  /* Else no */
  return 0;
}

/**
 * Notify the channel we're done flushing the output in the lower layer
 *
 * Connection.c will call this when we've flushed the output; there's some
 * dirreq-related maintenance to do.
 */

void
channel_notify_flushed(channel_t *chan)
{
  tor_assert(chan);

  if (chan->dirreq_id != 0)
    geoip_change_dirreq_state(chan->dirreq_id,
                              DIRREQ_TUNNELED,
                              DIRREQ_CHANNEL_BUFFER_FLUSHED);
}

/**
 * Process the queue of incoming channels on a listener
 *
 * Use a listener's registered callback to process as many entries in the
 * queue of incoming channels as possible.
 */

void
channel_listener_process_incoming(channel_listener_t *listener)
{
  tor_assert(listener);

  /*
   * CHANNEL_LISTENER_STATE_CLOSING permitted because we drain the queue
   * while closing a listener.
   */
  tor_assert(listener->state == CHANNEL_LISTENER_STATE_LISTENING ||
             listener->state == CHANNEL_LISTENER_STATE_CLOSING);
  tor_assert(listener->listener);

  log_debug(LD_CHANNEL,
            "Processing queue of incoming connections for channel "
            "listener %p (global ID " U64_FORMAT ")",
            listener, U64_PRINTF_ARG(listener->global_identifier));

  if (!(listener->incoming_list)) return;

  SMARTLIST_FOREACH_BEGIN(listener->incoming_list,
                          channel_t *, chan) {
    tor_assert(chan);

    log_debug(LD_CHANNEL,
              "Handling incoming channel %p (" U64_FORMAT ") "
              "for listener %p (" U64_FORMAT ")",
              chan,
              U64_PRINTF_ARG(chan->global_identifier),
              listener,
              U64_PRINTF_ARG(listener->global_identifier));
    /* Make sure this is set correctly */
    channel_mark_incoming(chan);
    listener->listener(listener, chan);
  } SMARTLIST_FOREACH_END(chan);

  smartlist_free(listener->incoming_list);
  listener->incoming_list = NULL;
}

/**
 * Take actions required when a channel becomes open
 *
 * Handle actions we should do when we know a channel is open; a lot of
 * this comes from the old connection_or_set_state_open() of connection_or.c.
 *
 * Because of this mechanism, future channel_t subclasses should take care
 * not to change a channel to from CHANNEL_STATE_OPENING to CHANNEL_STATE_OPEN
 * until there is positive confirmation that the network is operational.
 * In particular, anything UDP-based should not make this transition until a
 * packet is received from the other side.
 */

void
channel_do_open_actions(channel_t *chan)
{
  tor_addr_t remote_addr;
  int started_here, not_using = 0;
  time_t now = time(NULL);

  tor_assert(chan);

  started_here = channel_is_outgoing(chan);

  if (started_here) {
    circuit_build_times_network_is_live(get_circuit_build_times_mutable());
    rep_hist_note_connect_succeeded(chan->identity_digest, now);
    if (entry_guard_register_connect_status(
          chan->identity_digest, 1, 0, now) < 0) {
      /* Close any circuits pending on this channel. We leave it in state
       * 'open' though, because it didn't actually *fail* -- we just
       * chose not to use it. */
      log_debug(LD_OR,
                "New entry guard was reachable, but closing this "
                "connection so we can retry the earlier entry guards.");
      circuit_n_chan_done(chan, 0);
      not_using = 1;
    }
    router_set_status(chan->identity_digest, 1);
  } else {
    /* only report it to the geoip module if it's not a known router */
    if (!router_get_by_id_digest(chan->identity_digest)) {
      if (channel_get_addr_if_possible(chan, &remote_addr)) {
        char *transport_name = NULL;
        if (chan->get_transport_name(chan, &transport_name) < 0)
          transport_name = NULL;

        geoip_note_client_seen(GEOIP_CLIENT_CONNECT,
                               &remote_addr, transport_name,
                               now);
        tor_free(transport_name);
      }
      /* Otherwise the underlying transport can't tell us this, so skip it */
    }
  }

  if (!not_using) circuit_n_chan_done(chan, 1);
}

/**
 * Queue an incoming channel on a listener
 *
 * Internal and subclass use only function to queue an incoming channel from
 * a listener.  A subclass of channel_listener_t should call this when a new
 * incoming channel is created.
 */

void
channel_listener_queue_incoming(channel_listener_t *listener,
                                channel_t *incoming)
{
  int need_to_queue = 0;

  tor_assert(listener);
  tor_assert(listener->state == CHANNEL_LISTENER_STATE_LISTENING);
  tor_assert(incoming);

  log_debug(LD_CHANNEL,
            "Queueing incoming channel %p (global ID " U64_FORMAT ") on "
            "channel listener %p (global ID " U64_FORMAT ")",
            incoming, U64_PRINTF_ARG(incoming->global_identifier),
            listener, U64_PRINTF_ARG(listener->global_identifier));

  /* Do we need to queue it, or can we just call the listener right away? */
  if (!(listener->listener)) need_to_queue = 1;
  if (listener->incoming_list &&
      (smartlist_len(listener->incoming_list) > 0))
    need_to_queue = 1;

  /* If we need to queue and have no queue, create one */
  if (need_to_queue && !(listener->incoming_list)) {
    listener->incoming_list = smartlist_new();
  }

  /* Bump the counter and timestamp it */
  channel_listener_timestamp_active(listener);
  channel_listener_timestamp_accepted(listener);
  ++(listener->n_accepted);

  /* If we don't need to queue, process it right away */
  if (!need_to_queue) {
    tor_assert(listener->listener);
    listener->listener(listener, incoming);
  }
  /*
   * Otherwise, we need to queue; queue and then process the queue if
   * we can.
   */
  else {
    tor_assert(listener->incoming_list);
    smartlist_add(listener->incoming_list, incoming);
    if (listener->listener) channel_listener_process_incoming(listener);
  }
}

/**
 * Process queued incoming cells
 *
 * Process as many queued cells as we can from the incoming
 * cell queue.
 */

void
channel_process_cells(channel_t *chan)
{
  cell_queue_entry_t *q;
  tor_assert(chan);
  tor_assert(chan->state == CHANNEL_STATE_CLOSING ||
             chan->state == CHANNEL_STATE_MAINT ||
             chan->state == CHANNEL_STATE_OPEN);

  log_debug(LD_CHANNEL,
            "Processing as many incoming cells as we can for channel %p",
            chan);

  /* Nothing we can do if we have no registered cell handlers */
  if (!(chan->cell_handler ||
        chan->var_cell_handler)) return;
  /* Nothing we can do if we have no cells */
  if (TOR_SIMPLEQ_EMPTY(&chan->incoming_queue)) return;

  /*
   * Process cells until we're done or find one we have no current handler
   * for.
   */
  while (NULL != (q = TOR_SIMPLEQ_FIRST(&chan->incoming_queue))) {
    tor_assert(q);
    tor_assert(q->type == CELL_QUEUE_FIXED ||
               q->type == CELL_QUEUE_VAR);

    if (q->type == CELL_QUEUE_FIXED &&
        chan->cell_handler) {
      /* Handle a fixed-length cell */
      TOR_SIMPLEQ_REMOVE_HEAD(&chan->incoming_queue, next);
      tor_assert(q->u.fixed.cell);
      log_debug(LD_CHANNEL,
                "Processing incoming cell_t %p for channel %p (global ID "
                U64_FORMAT ")",
                q->u.fixed.cell, chan,
                U64_PRINTF_ARG(chan->global_identifier));
      chan->cell_handler(chan, q->u.fixed.cell);
      tor_free(q);
    } else if (q->type == CELL_QUEUE_VAR &&
               chan->var_cell_handler) {
      /* Handle a variable-length cell */
      TOR_SIMPLEQ_REMOVE_HEAD(&chan->incoming_queue, next);
      tor_assert(q->u.var.var_cell);
      log_debug(LD_CHANNEL,
                "Processing incoming var_cell_t %p for channel %p (global ID "
                U64_FORMAT ")",
                q->u.var.var_cell, chan,
                U64_PRINTF_ARG(chan->global_identifier));
      chan->var_cell_handler(chan, q->u.var.var_cell);
      tor_free(q);
    } else {
      /* Can't handle this one */
      break;
    }
  }
}

/**
 * Queue incoming cell
 *
 * This should be called by a channel_t subclass to queue an incoming fixed-
 * length cell for processing, and process it if possible.
 */

void
channel_queue_cell(channel_t *chan, cell_t *cell)
{
  int need_to_queue = 0;
  cell_queue_entry_t *q;

  tor_assert(chan);
  tor_assert(cell);
  tor_assert(chan->state == CHANNEL_STATE_OPEN);

  /* Do we need to queue it, or can we just call the handler right away? */
  if (!(chan->cell_handler)) need_to_queue = 1;
  if (! TOR_SIMPLEQ_EMPTY(&chan->incoming_queue))
    need_to_queue = 1;

  /* Timestamp for receiving */
  channel_timestamp_recv(chan);

  /* Update the counter */
  ++(chan->n_cells_recved);

  /* If we don't need to queue we can just call cell_handler */
  if (!need_to_queue) {
    tor_assert(chan->cell_handler);
    log_debug(LD_CHANNEL,
              "Directly handling incoming cell_t %p for channel %p "
              "(global ID " U64_FORMAT ")",
              cell, chan,
              U64_PRINTF_ARG(chan->global_identifier));
    chan->cell_handler(chan, cell);
  } else {
    /* Otherwise queue it and then process the queue if possible. */
    q = cell_queue_entry_new_fixed(cell);
    log_debug(LD_CHANNEL,
              "Queueing incoming cell_t %p for channel %p "
              "(global ID " U64_FORMAT ")",
              cell, chan,
              U64_PRINTF_ARG(chan->global_identifier));
    TOR_SIMPLEQ_INSERT_TAIL(&chan->incoming_queue, q, next);
    if (chan->cell_handler ||
        chan->var_cell_handler) {
      channel_process_cells(chan);
    }
  }
}

/**
 * Queue incoming variable-length cell
 *
 * This should be called by a channel_t subclass to queue an incoming
 * variable-length cell for processing, and process it if possible.
 */

void
channel_queue_var_cell(channel_t *chan, var_cell_t *var_cell)
{
  int need_to_queue = 0;
  cell_queue_entry_t *q;

  tor_assert(chan);
  tor_assert(var_cell);
  tor_assert(chan->state == CHANNEL_STATE_OPEN);

  /* Do we need to queue it, or can we just call the handler right away? */
  if (!(chan->var_cell_handler)) need_to_queue = 1;
  if (! TOR_SIMPLEQ_EMPTY(&chan->incoming_queue))
    need_to_queue = 1;

  /* Timestamp for receiving */
  channel_timestamp_recv(chan);

  /* Update the counter */
  ++(chan->n_cells_recved);

  /* If we don't need to queue we can just call cell_handler */
  if (!need_to_queue) {
    tor_assert(chan->var_cell_handler);
    log_debug(LD_CHANNEL,
              "Directly handling incoming var_cell_t %p for channel %p "
              "(global ID " U64_FORMAT ")",
              var_cell, chan,
              U64_PRINTF_ARG(chan->global_identifier));
    chan->var_cell_handler(chan, var_cell);
  } else {
    /* Otherwise queue it and then process the queue if possible. */
    q = cell_queue_entry_new_var(var_cell);
    log_debug(LD_CHANNEL,
              "Queueing incoming var_cell_t %p for channel %p "
              "(global ID " U64_FORMAT ")",
              var_cell, chan,
              U64_PRINTF_ARG(chan->global_identifier));
    TOR_SIMPLEQ_INSERT_TAIL(&chan->incoming_queue, q, next);
    if (chan->cell_handler ||
        chan->var_cell_handler) {
      channel_process_cells(chan);
    }
  }
}

/** If <b>packed_cell</b> on <b>chan</b> is a destroy cell, then set
 * *<b>circid_out</b> to its circuit ID, and return true.  Otherwise, return
 * false. */
/* XXXX Move this function. */
int
packed_cell_is_destroy(channel_t *chan,
                       const packed_cell_t *packed_cell,
                       circid_t *circid_out)
{
  if (chan->wide_circ_ids) {
    if (packed_cell->body[4] == CELL_DESTROY) {
      *circid_out = ntohl(get_uint32(packed_cell->body));
      return 1;
    }
  } else {
    if (packed_cell->body[2] == CELL_DESTROY) {
      *circid_out = ntohs(get_uint16(packed_cell->body));
      return 1;
    }
  }
  return 0;
}

/** DOCDOC */
static int
is_destroy_cell(channel_t *chan,
                const cell_queue_entry_t *q, circid_t *circid_out)
{
  *circid_out = 0;
  switch (q->type) {
    case CELL_QUEUE_FIXED:
      if (q->u.fixed.cell->command == CELL_DESTROY) {
        *circid_out = q->u.fixed.cell->circ_id;
        return 1;
      }
      break;
    case CELL_QUEUE_VAR:
      if (q->u.var.var_cell->command == CELL_DESTROY) {
        *circid_out = q->u.var.var_cell->circ_id;
        return 1;
      }
      break;
    case CELL_QUEUE_PACKED:
      return packed_cell_is_destroy(chan, q->u.packed.packed_cell, circid_out);
  }
  return 0;
}

/**
 * Send destroy cell on a channel
 *
 * Write a destroy cell with circ ID <b>circ_id</b> and reason <b>reason</b>
 * onto channel <b>chan</b>.  Don't perform range-checking on reason:
 * we may want to propagate reasons from other cells.
 */

int
channel_send_destroy(circid_t circ_id, channel_t *chan, int reason)
{
  tor_assert(chan);
  if (circ_id == 0) {
    log_warn(LD_BUG, "Attempted to send a destroy cell for circID 0 "
             "on a channel " U64_FORMAT " at %p in state %s (%d)",
             U64_PRINTF_ARG(chan->global_identifier),
             chan, channel_state_to_string(chan->state),
             chan->state);
    return 0;
  }

  /* Check to make sure we can send on this channel first */
  if (!(chan->state == CHANNEL_STATE_CLOSING ||
        chan->state == CHANNEL_STATE_CLOSED ||
        chan->state == CHANNEL_STATE_ERROR) &&
      chan->cmux) {
    channel_note_destroy_pending(chan, circ_id);
    circuitmux_append_destroy_cell(chan, chan->cmux, circ_id, reason);
    log_debug(LD_OR,
              "Sending destroy (circID %u) on channel %p "
              "(global ID " U64_FORMAT ")",
              (unsigned)circ_id, chan,
              U64_PRINTF_ARG(chan->global_identifier));
  } else {
    log_warn(LD_BUG,
             "Someone called channel_send_destroy() for circID %u "
             "on a channel " U64_FORMAT " at %p in state %s (%d)",
             (unsigned)circ_id, U64_PRINTF_ARG(chan->global_identifier),
             chan, channel_state_to_string(chan->state),
             chan->state);
  }

  return 0;
}

/**
 * Dump channel statistics to the log
 *
 * This is called from dumpstats() in main.c and spams the log with
 * statistics on channels.
 */

void
channel_dumpstats(int severity)
{
  if (all_channels && smartlist_len(all_channels) > 0) {
    tor_log(severity, LD_GENERAL,
        "Dumping statistics about %d channels:",
        smartlist_len(all_channels));
    tor_log(severity, LD_GENERAL,
        "%d are active, and %d are done and waiting for cleanup",
        (active_channels != NULL) ?
          smartlist_len(active_channels) : 0,
        (finished_channels != NULL) ?
          smartlist_len(finished_channels) : 0);

    SMARTLIST_FOREACH(all_channels, channel_t *, chan,
                      channel_dump_statistics(chan, severity));

    tor_log(severity, LD_GENERAL,
        "Done spamming about channels now");
  } else {
    tor_log(severity, LD_GENERAL,
        "No channels to dump");
  }
}

/**
 * Dump channel listener statistics to the log
 *
 * This is called from dumpstats() in main.c and spams the log with
 * statistics on channel listeners.
 */

void
channel_listener_dumpstats(int severity)
{
  if (all_listeners && smartlist_len(all_listeners) > 0) {
    tor_log(severity, LD_GENERAL,
        "Dumping statistics about %d channel listeners:",
        smartlist_len(all_listeners));
    tor_log(severity, LD_GENERAL,
        "%d are active and %d are done and waiting for cleanup",
        (active_listeners != NULL) ?
          smartlist_len(active_listeners) : 0,
        (finished_listeners != NULL) ?
          smartlist_len(finished_listeners) : 0);

    SMARTLIST_FOREACH(all_listeners, channel_listener_t *, chan_l,
                      channel_listener_dump_statistics(chan_l, severity));

    tor_log(severity, LD_GENERAL,
        "Done spamming about channel listeners now");
  } else {
    tor_log(severity, LD_GENERAL,
        "No channel listeners to dump");
  }
}

/**
 * Set the cmux policy on all active channels
 */

void
channel_set_cmux_policy_everywhere(circuitmux_policy_t *pol)
{
  if (!active_channels) return;

  SMARTLIST_FOREACH_BEGIN(active_channels, channel_t *, curr) {
    if (curr->cmux) {
      circuitmux_set_policy(curr->cmux, pol);
    }
  } SMARTLIST_FOREACH_END(curr);
}

/**
 * Clean up channels
 *
 * This gets called periodically from run_scheduled_events() in main.c;
 * it cleans up after closed channels.
 */

void
channel_run_cleanup(void)
{
  channel_t *tmp = NULL;

  /* Check if we need to do anything */
  if (!finished_channels || smartlist_len(finished_channels) == 0) return;

  /* Iterate through finished_channels and get rid of them */
  SMARTLIST_FOREACH_BEGIN(finished_channels, channel_t *, curr) {
    tmp = curr;
    /* Remove it from the list */
    SMARTLIST_DEL_CURRENT(finished_channels, curr);
    /* Also unregister it */
    channel_unregister(tmp);
    /* ... and free it */
    channel_free(tmp);
  } SMARTLIST_FOREACH_END(curr);
}

/**
 * Clean up channel listeners
 *
 * This gets called periodically from run_scheduled_events() in main.c;
 * it cleans up after closed channel listeners.
 */

void
channel_listener_run_cleanup(void)
{
  channel_listener_t *tmp = NULL;

  /* Check if we need to do anything */
  if (!finished_listeners || smartlist_len(finished_listeners) == 0) return;

  /* Iterate through finished_channels and get rid of them */
  SMARTLIST_FOREACH_BEGIN(finished_listeners, channel_listener_t *, curr) {
    tmp = curr;
    /* Remove it from the list */
    SMARTLIST_DEL_CURRENT(finished_listeners, curr);
    /* Also unregister it */
    channel_listener_unregister(tmp);
    /* ... and free it */
    channel_listener_free(tmp);
  } SMARTLIST_FOREACH_END(curr);
}

/**
 * Free a list of channels for channel_free_all()
 */

static void
channel_free_list(smartlist_t *channels, int mark_for_close)
{
  if (!channels) return;

  SMARTLIST_FOREACH_BEGIN(channels, channel_t *, curr) {
    /* Deregister and free it */
    tor_assert(curr);
    log_debug(LD_CHANNEL,
              "Cleaning up channel %p (global ID " U64_FORMAT ") "
              "in state %s (%d)",
              curr, U64_PRINTF_ARG(curr->global_identifier),
              channel_state_to_string(curr->state), curr->state);
    /* Detach circuits early so they can find the channel */
    if (curr->cmux) {
      circuitmux_detach_all_circuits(curr->cmux, NULL);
    }
    channel_unregister(curr);
    if (mark_for_close) {
      if (!(curr->state == CHANNEL_STATE_CLOSING ||
            curr->state == CHANNEL_STATE_CLOSED ||
            curr->state == CHANNEL_STATE_ERROR)) {
        channel_mark_for_close(curr);
      }
      channel_force_free(curr);
    } else channel_free(curr);
  } SMARTLIST_FOREACH_END(curr);
}

/**
 * Free a list of channel listeners for channel_free_all()
 */

static void
channel_listener_free_list(smartlist_t *listeners, int mark_for_close)
{
  if (!listeners) return;

  SMARTLIST_FOREACH_BEGIN(listeners, channel_listener_t *, curr) {
    /* Deregister and free it */
    tor_assert(curr);
    log_debug(LD_CHANNEL,
              "Cleaning up channel listener %p (global ID " U64_FORMAT ") "
              "in state %s (%d)",
              curr, U64_PRINTF_ARG(curr->global_identifier),
              channel_listener_state_to_string(curr->state), curr->state);
    channel_listener_unregister(curr);
    if (mark_for_close) {
      if (!(curr->state == CHANNEL_LISTENER_STATE_CLOSING ||
            curr->state == CHANNEL_LISTENER_STATE_CLOSED ||
            curr->state == CHANNEL_LISTENER_STATE_ERROR)) {
        channel_listener_mark_for_close(curr);
      }
      channel_listener_force_free(curr);
    } else channel_listener_free(curr);
  } SMARTLIST_FOREACH_END(curr);
}

/**
 * Close all channels and free everything
 *
 * This gets called from tor_free_all() in main.c to clean up on exit.
 * It will close all registered channels and free associated storage,
 * then free the all_channels, active_channels, listening_channels and
 * finished_channels lists and also channel_identity_map.
 */

void
channel_free_all(void)
{
  log_debug(LD_CHANNEL,
            "Shutting down channels...");

  /* First, let's go for finished channels */
  if (finished_channels) {
    channel_free_list(finished_channels, 0);
    smartlist_free(finished_channels);
    finished_channels = NULL;
  }

  /* Now the finished listeners */
  if (finished_listeners) {
    channel_listener_free_list(finished_listeners, 0);
    smartlist_free(finished_listeners);
    finished_listeners = NULL;
  }

  /* Now all active channels */
  if (active_channels) {
    channel_free_list(active_channels, 1);
    smartlist_free(active_channels);
    active_channels = NULL;
  }

  /* Now all active listeners */
  if (active_listeners) {
    channel_listener_free_list(active_listeners, 1);
    smartlist_free(active_listeners);
    active_listeners = NULL;
  }

  /* Now all channels, in case any are left over */
  if (all_channels) {
    channel_free_list(all_channels, 1);
    smartlist_free(all_channels);
    all_channels = NULL;
  }

  /* Now all listeners, in case any are left over */
  if (all_listeners) {
    channel_listener_free_list(all_listeners, 1);
    smartlist_free(all_listeners);
    all_listeners = NULL;
  }

  /* Now free channel_identity_map */
  log_debug(LD_CHANNEL,
            "Freeing channel_identity_map");
  /* Geez, anything still left over just won't die ... let it leak then */
  HT_CLEAR(channel_idmap, &channel_identity_map);

  log_debug(LD_CHANNEL,
            "Done cleaning up after channels");
}

/**
 * Connect to a given addr/port/digest
 *
 * This sets up a new outgoing channel; in the future if multiple
 * channel_t subclasses are available, this is where the selection policy
 * should go.  It may also be desirable to fold port into tor_addr_t
 * or make a new type including a tor_addr_t and port, so we have a
 * single abstract object encapsulating all the protocol details of
 * how to contact an OR.
 */

channel_t *
channel_connect(const tor_addr_t *addr, uint16_t port,
                const char *id_digest)
{
  return channel_tls_connect(addr, port, id_digest);
}

/**
 * Decide which of two channels to prefer for extending a circuit
 *
 * This function is called while extending a circuit and returns true iff
 * a is 'better' than b.  The most important criterion here is that a
 * canonical channel is always better than a non-canonical one, but the
 * number of circuits and the age are used as tie-breakers.
 *
 * This is based on the former connection_or_is_better() of connection_or.c
 */

int
channel_is_better(time_t now, channel_t *a, channel_t *b,
                  int forgive_new_connections)
{
  int a_grace, b_grace;
  int a_is_canonical, b_is_canonical;
  int a_has_circs, b_has_circs;

  /*
   * Do not definitively deprecate a new channel with no circuits on it
   * until this much time has passed.
   */
#define NEW_CHAN_GRACE_PERIOD (15*60)

  tor_assert(a);
  tor_assert(b);

  /* Check if one is canonical and the other isn't first */
  a_is_canonical = channel_is_canonical(a);
  b_is_canonical = channel_is_canonical(b);

  if (a_is_canonical && !b_is_canonical) return 1;
  if (!a_is_canonical && b_is_canonical) return 0;

  /*
   * Okay, if we're here they tied on canonicity. Next we check if
   * they have any circuits, and if one does and the other doesn't,
   * we prefer the one that does, unless we are forgiving and the
   * one that has no circuits is in its grace period.
   */

  a_has_circs = (channel_num_circuits(a) > 0);
  b_has_circs = (channel_num_circuits(b) > 0);
  a_grace = (forgive_new_connections &&
             (now < channel_when_created(a) + NEW_CHAN_GRACE_PERIOD));
  b_grace = (forgive_new_connections &&
             (now < channel_when_created(b) + NEW_CHAN_GRACE_PERIOD));

  if (a_has_circs && !b_has_circs && !b_grace) return 1;
  if (!a_has_circs && b_has_circs && !a_grace) return 0;

  /* They tied on circuits too; just prefer whichever is newer */

  if (channel_when_created(a) > channel_when_created(b)) return 1;
  else return 0;
}

/**
 * Get a channel to extend a circuit
 *
 * Pick a suitable channel to extend a circuit to given the desired digest
 * the address we believe is correct for that digest; this tries to see
 * if we already have one for the requested endpoint, but if there is no good
 * channel, set *msg_out to a message describing the channel's state
 * and our next action, and set *launch_out to a boolean indicated whether
 * the caller should try to launch a new channel with channel_connect().
 */

channel_t *
channel_get_for_extend(const char *digest,
                       const tor_addr_t *target_addr,
                       const char **msg_out,
                       int *launch_out)
{
  channel_t *chan, *best = NULL;
  int n_inprogress_goodaddr = 0, n_old = 0;
  int n_noncanonical = 0, n_possible = 0;
  time_t now = approx_time();

  tor_assert(msg_out);
  tor_assert(launch_out);

  chan = channel_find_by_remote_digest(digest);

  /* Walk the list, unrefing the old one and refing the new at each
   * iteration.
   */
  for (; chan; chan = channel_next_with_digest(chan)) {
    tor_assert(tor_memeq(chan->identity_digest,
                         digest, DIGEST_LEN));

    if (chan->state == CHANNEL_STATE_CLOSING ||
        chan->state == CHANNEL_STATE_CLOSED ||
        chan->state == CHANNEL_STATE_ERROR)
      continue;

    /* Never return a channel on which the other end appears to be
     * a client. */
    if (channel_is_client(chan)) {
      continue;
    }

    /* Never return a non-open connection. */
    if (chan->state != CHANNEL_STATE_OPEN) {
      /* If the address matches, don't launch a new connection for this
       * circuit. */
      if (channel_matches_target_addr_for_extend(chan, target_addr))
        ++n_inprogress_goodaddr;
      continue;
    }

    /* Never return a connection that shouldn't be used for circs. */
    if (channel_is_bad_for_new_circs(chan)) {
      ++n_old;
      continue;
    }

    /* Never return a non-canonical connection using a recent link protocol
     * if the address is not what we wanted.
     *
     * The channel_is_canonical_is_reliable() function asks the lower layer
     * if we should trust channel_is_canonical().  The below is from the
     * comments of the old circuit_or_get_for_extend() and applies when
     * the lower-layer transport is channel_tls_t.
     *
     * (For old link protocols, we can't rely on is_canonical getting
     * set properly if we're talking to the right address, since we might
     * have an out-of-date descriptor, and we will get no NETINFO cell to
     * tell us about the right address.)
     */
    if (!channel_is_canonical(chan) &&
         channel_is_canonical_is_reliable(chan) &&
        !channel_matches_target_addr_for_extend(chan, target_addr)) {
      ++n_noncanonical;
      continue;
    }

    ++n_possible;

    if (!best) {
      best = chan; /* If we have no 'best' so far, this one is good enough. */
      continue;
    }

    if (channel_is_better(now, chan, best, 0))
      best = chan;
  }

  if (best) {
    *msg_out = "Connection is fine; using it.";
    *launch_out = 0;
    return best;
  } else if (n_inprogress_goodaddr) {
    *msg_out = "Connection in progress; waiting.";
    *launch_out = 0;
    return NULL;
  } else if (n_old || n_noncanonical) {
    *msg_out = "Connections all too old, or too non-canonical. "
               " Launching a new one.";
    *launch_out = 1;
    return NULL;
  } else {
    *msg_out = "Not connected. Connecting.";
    *launch_out = 1;
    return NULL;
  }
}

/**
 * Describe the transport subclass for a channel
 *
 * Invoke a method to get a string description of the lower-layer
 * transport for this channel.
 */

const char *
channel_describe_transport(channel_t *chan)
{
  tor_assert(chan);
  tor_assert(chan->describe_transport);

  return chan->describe_transport(chan);
}

/**
 * Describe the transport subclass for a channel listener
 *
 * Invoke a method to get a string description of the lower-layer
 * transport for this channel listener.
 */

const char *
channel_listener_describe_transport(channel_listener_t *chan_l)
{
  tor_assert(chan_l);
  tor_assert(chan_l->describe_transport);

  return chan_l->describe_transport(chan_l);
}

/**
 * Return the number of entries in <b>queue</b>
 */
static int
chan_cell_queue_len(const chan_cell_queue_t *queue)
{
  int r = 0;
  cell_queue_entry_t *cell;
  TOR_SIMPLEQ_FOREACH(cell, queue, next)
    ++r;
  return r;
}

/**
 * Dump channel statistics
 *
 * Dump statistics for one channel to the log
 */

void
channel_dump_statistics(channel_t *chan, int severity)
{
  double avg, interval, age;
  time_t now = time(NULL);
  tor_addr_t remote_addr;
  int have_remote_addr;
  char *remote_addr_str;

  tor_assert(chan);

  age = (double)(now - chan->timestamp_created);

  tor_log(severity, LD_GENERAL,
      "Channel " U64_FORMAT " (at %p) with transport %s is in state "
      "%s (%d)",
      U64_PRINTF_ARG(chan->global_identifier), chan,
      channel_describe_transport(chan),
      channel_state_to_string(chan->state), chan->state);
  tor_log(severity, LD_GENERAL,
      " * Channel " U64_FORMAT " was created at " U64_FORMAT
      " (" U64_FORMAT " seconds ago) "
      "and last active at " U64_FORMAT " (" U64_FORMAT " seconds ago)",
      U64_PRINTF_ARG(chan->global_identifier),
      U64_PRINTF_ARG(chan->timestamp_created),
      U64_PRINTF_ARG(now - chan->timestamp_created),
      U64_PRINTF_ARG(chan->timestamp_active),
      U64_PRINTF_ARG(now - chan->timestamp_active));

  /* Handle digest and nickname */
  if (!tor_digest_is_zero(chan->identity_digest)) {
    if (chan->nickname) {
      tor_log(severity, LD_GENERAL,
          " * Channel " U64_FORMAT " says it is connected "
          "to an OR with digest %s and nickname %s",
          U64_PRINTF_ARG(chan->global_identifier),
          hex_str(chan->identity_digest, DIGEST_LEN),
          chan->nickname);
    } else {
      tor_log(severity, LD_GENERAL,
          " * Channel " U64_FORMAT " says it is connected "
          "to an OR with digest %s and no known nickname",
          U64_PRINTF_ARG(chan->global_identifier),
          hex_str(chan->identity_digest, DIGEST_LEN));
    }
  } else {
    if (chan->nickname) {
      tor_log(severity, LD_GENERAL,
          " * Channel " U64_FORMAT " does not know the digest"
          " of the OR it is connected to, but reports its nickname is %s",
          U64_PRINTF_ARG(chan->global_identifier),
          chan->nickname);
    } else {
      tor_log(severity, LD_GENERAL,
          " * Channel " U64_FORMAT " does not know the digest"
          " or the nickname of the OR it is connected to",
          U64_PRINTF_ARG(chan->global_identifier));
    }
  }

  /* Handle remote address and descriptions */
  have_remote_addr = channel_get_addr_if_possible(chan, &remote_addr);
  if (have_remote_addr) {
    char *actual = tor_strdup(channel_get_actual_remote_descr(chan));
    remote_addr_str = tor_dup_addr(&remote_addr);
    tor_log(severity, LD_GENERAL,
        " * Channel " U64_FORMAT " says its remote address"
        " is %s, and gives a canonical description of \"%s\" and an "
        "actual description of \"%s\"",
        U64_PRINTF_ARG(chan->global_identifier),
        safe_str(remote_addr_str),
        safe_str(channel_get_canonical_remote_descr(chan)),
        safe_str(actual));
    tor_free(remote_addr_str);
    tor_free(actual);
  } else {
    char *actual = tor_strdup(channel_get_actual_remote_descr(chan));
    tor_log(severity, LD_GENERAL,
        " * Channel " U64_FORMAT " does not know its remote "
        "address, but gives a canonical description of \"%s\" and an "
        "actual description of \"%s\"",
        U64_PRINTF_ARG(chan->global_identifier),
        channel_get_canonical_remote_descr(chan),
        actual);
    tor_free(actual);
  }

  /* Handle marks */
  tor_log(severity, LD_GENERAL,
      " * Channel " U64_FORMAT " has these marks: %s %s %s "
      "%s %s %s",
      U64_PRINTF_ARG(chan->global_identifier),
      channel_is_bad_for_new_circs(chan) ?
        "bad_for_new_circs" : "!bad_for_new_circs",
      channel_is_canonical(chan) ?
        "canonical" : "!canonical",
      channel_is_canonical_is_reliable(chan) ?
        "is_canonical_is_reliable" :
        "!is_canonical_is_reliable",
      channel_is_client(chan) ?
        "client" : "!client",
      channel_is_local(chan) ?
        "local" : "!local",
      channel_is_incoming(chan) ?
        "incoming" : "outgoing");

  /* Describe queues */
  tor_log(severity, LD_GENERAL,
      " * Channel " U64_FORMAT " has %d queued incoming cells"
      " and %d queued outgoing cells",
      U64_PRINTF_ARG(chan->global_identifier),
      chan_cell_queue_len(&chan->incoming_queue),
      chan_cell_queue_len(&chan->outgoing_queue));

  /* Describe circuits */
  tor_log(severity, LD_GENERAL,
      " * Channel " U64_FORMAT " has %d active circuits out of"
      " %d in total",
      U64_PRINTF_ARG(chan->global_identifier),
      (chan->cmux != NULL) ?
         circuitmux_num_active_circuits(chan->cmux) : 0,
      (chan->cmux != NULL) ?
         circuitmux_num_circuits(chan->cmux) : 0);

  /* Describe timestamps */
  tor_log(severity, LD_GENERAL,
      " * Channel " U64_FORMAT " was last used by a "
      "client at " U64_FORMAT " (" U64_FORMAT " seconds ago)",
      U64_PRINTF_ARG(chan->global_identifier),
      U64_PRINTF_ARG(chan->timestamp_client),
      U64_PRINTF_ARG(now - chan->timestamp_client));
  tor_log(severity, LD_GENERAL,
      " * Channel " U64_FORMAT " was last drained at "
      U64_FORMAT " (" U64_FORMAT " seconds ago)",
      U64_PRINTF_ARG(chan->global_identifier),
      U64_PRINTF_ARG(chan->timestamp_drained),
      U64_PRINTF_ARG(now - chan->timestamp_drained));
  tor_log(severity, LD_GENERAL,
      " * Channel " U64_FORMAT " last received a cell "
      "at " U64_FORMAT " (" U64_FORMAT " seconds ago)",
      U64_PRINTF_ARG(chan->global_identifier),
      U64_PRINTF_ARG(chan->timestamp_recv),
      U64_PRINTF_ARG(now - chan->timestamp_recv));
  tor_log(severity, LD_GENERAL,
      " * Channel " U64_FORMAT " last transmitted a cell "
      "at " U64_FORMAT " (" U64_FORMAT " seconds ago)",
      U64_PRINTF_ARG(chan->global_identifier),
      U64_PRINTF_ARG(chan->timestamp_xmit),
      U64_PRINTF_ARG(now - chan->timestamp_xmit));

  /* Describe counters and rates */
  tor_log(severity, LD_GENERAL,
      " * Channel " U64_FORMAT " has received "
      U64_FORMAT " cells and transmitted " U64_FORMAT,
      U64_PRINTF_ARG(chan->global_identifier),
      U64_PRINTF_ARG(chan->n_cells_recved),
      U64_PRINTF_ARG(chan->n_cells_xmitted));
  if (now > chan->timestamp_created &&
      chan->timestamp_created > 0) {
    if (chan->n_cells_recved > 0) {
      avg = (double)(chan->n_cells_recved) / age;
      if (avg >= 1.0) {
        tor_log(severity, LD_GENERAL,
            " * Channel " U64_FORMAT " has averaged %f "
            "cells received per second",
            U64_PRINTF_ARG(chan->global_identifier), avg);
      } else if (avg >= 0.0) {
        interval = 1.0 / avg;
        tor_log(severity, LD_GENERAL,
            " * Channel " U64_FORMAT " has averaged %f "
            "seconds between received cells",
            U64_PRINTF_ARG(chan->global_identifier), interval);
      }
    }
    if (chan->n_cells_xmitted > 0) {
      avg = (double)(chan->n_cells_xmitted) / age;
      if (avg >= 1.0) {
        tor_log(severity, LD_GENERAL,
            " * Channel " U64_FORMAT " has averaged %f "
            "cells transmitted per second",
            U64_PRINTF_ARG(chan->global_identifier), avg);
      } else if (avg >= 0.0) {
        interval = 1.0 / avg;
        tor_log(severity, LD_GENERAL,
            " * Channel " U64_FORMAT " has averaged %f "
            "seconds between transmitted cells",
            U64_PRINTF_ARG(chan->global_identifier), interval);
      }
    }
  }

  /* Dump anything the lower layer has to say */
  channel_dump_transport_statistics(chan, severity);
}

/**
 * Dump channel listener statistics
 *
 * Dump statistics for one channel listener to the log
 */

void
channel_listener_dump_statistics(channel_listener_t *chan_l, int severity)
{
  double avg, interval, age;
  time_t now = time(NULL);

  tor_assert(chan_l);

  age = (double)(now - chan_l->timestamp_created);

  tor_log(severity, LD_GENERAL,
      "Channel listener " U64_FORMAT " (at %p) with transport %s is in "
      "state %s (%d)",
      U64_PRINTF_ARG(chan_l->global_identifier), chan_l,
      channel_listener_describe_transport(chan_l),
      channel_listener_state_to_string(chan_l->state), chan_l->state);
  tor_log(severity, LD_GENERAL,
      " * Channel listener " U64_FORMAT " was created at " U64_FORMAT
      " (" U64_FORMAT " seconds ago) "
      "and last active at " U64_FORMAT " (" U64_FORMAT " seconds ago)",
      U64_PRINTF_ARG(chan_l->global_identifier),
      U64_PRINTF_ARG(chan_l->timestamp_created),
      U64_PRINTF_ARG(now - chan_l->timestamp_created),
      U64_PRINTF_ARG(chan_l->timestamp_active),
      U64_PRINTF_ARG(now - chan_l->timestamp_active));

  tor_log(severity, LD_GENERAL,
      " * Channel listener " U64_FORMAT " last accepted an incoming "
        "channel at " U64_FORMAT " (" U64_FORMAT " seconds ago) "
        "and has accepted " U64_FORMAT " channels in total",
        U64_PRINTF_ARG(chan_l->global_identifier),
        U64_PRINTF_ARG(chan_l->timestamp_accepted),
        U64_PRINTF_ARG(now - chan_l->timestamp_accepted),
        U64_PRINTF_ARG(chan_l->n_accepted));

  /*
   * If it's sensible to do so, get the rate of incoming channels on this
   * listener
   */
  if (now > chan_l->timestamp_created &&
      chan_l->timestamp_created > 0 &&
      chan_l->n_accepted > 0) {
    avg = (double)(chan_l->n_accepted) / age;
    if (avg >= 1.0) {
      tor_log(severity, LD_GENERAL,
          " * Channel listener " U64_FORMAT " has averaged %f incoming "
          "channels per second",
          U64_PRINTF_ARG(chan_l->global_identifier), avg);
    } else if (avg >= 0.0) {
      interval = 1.0 / avg;
      tor_log(severity, LD_GENERAL,
          " * Channel listener " U64_FORMAT " has averaged %f seconds "
          "between incoming channels",
          U64_PRINTF_ARG(chan_l->global_identifier), interval);
    }
  }

  /* Dump anything the lower layer has to say */
  channel_listener_dump_transport_statistics(chan_l, severity);
}

/**
 * Invoke transport-specific stats dump for channel
 *
 * If there is a lower-layer statistics dump method, invoke it
 */

void
channel_dump_transport_statistics(channel_t *chan, int severity)
{
  tor_assert(chan);

  if (chan->dumpstats) chan->dumpstats(chan, severity);
}

/**
 * Invoke transport-specific stats dump for channel listener
 *
 * If there is a lower-layer statistics dump method, invoke it
 */

void
channel_listener_dump_transport_statistics(channel_listener_t *chan_l,
                                           int severity)
{
  tor_assert(chan_l);

  if (chan_l->dumpstats) chan_l->dumpstats(chan_l, severity);
}

/**
 * Return text description of the remote endpoint
 *
 * This function return a test provided by the lower layer of the remote
 * endpoint for this channel; it should specify the actual address connected
 * to/from.
 *
 * Subsequent calls to channel_get_{actual,canonical}_remote_{address,descr}
 * may invalidate the return value from this function.
 */
const char *
channel_get_actual_remote_descr(channel_t *chan)
{
  tor_assert(chan);
  tor_assert(chan->get_remote_descr);

  /* Param 1 indicates the actual description */
  return chan->get_remote_descr(chan, GRD_FLAG_ORIGINAL);
}

/**
 * Return the text address of the remote endpoint.
 *
 * Subsequent calls to channel_get_{actual,canonical}_remote_{address,descr}
 * may invalidate the return value from this function.
 */
const char *
channel_get_actual_remote_address(channel_t *chan)
{
  /* Param 1 indicates the actual description */
  return chan->get_remote_descr(chan, GRD_FLAG_ORIGINAL|GRD_FLAG_ADDR_ONLY);
}

/**
 * Return text description of the remote endpoint canonical address
 *
 * This function return a test provided by the lower layer of the remote
 * endpoint for this channel; it should use the known canonical address for
 * this OR's identity digest if possible.
 *
 * Subsequent calls to channel_get_{actual,canonical}_remote_{address,descr}
 * may invalidate the return value from this function.
 */
const char *
channel_get_canonical_remote_descr(channel_t *chan)
{
  tor_assert(chan);
  tor_assert(chan->get_remote_descr);

  /* Param 0 indicates the canonicalized description */
  return chan->get_remote_descr(chan, 0);
}

/**
 * Get remote address if possible.
 *
 * Write the remote address out to a tor_addr_t if the underlying transport
 * supports this operation, and return 1.  Return 0 if the underlying transport
 * doesn't let us do this.
 */
int
channel_get_addr_if_possible(channel_t *chan, tor_addr_t *addr_out)
{
  tor_assert(chan);
  tor_assert(addr_out);

  if (chan->get_remote_addr)
    return chan->get_remote_addr(chan, addr_out);
  /* Else no support, method not implemented */
  else return 0;
}

/**
 * Check if there are outgoing queue writes on this channel
 *
 * Indicate if either we have queued cells, or if not, whether the underlying
 * lower-layer transport thinks it has an output queue.
 */

int
channel_has_queued_writes(channel_t *chan)
{
  int has_writes = 0;

  tor_assert(chan);
  tor_assert(chan->has_queued_writes);

  if (! TOR_SIMPLEQ_EMPTY(&chan->outgoing_queue)) {
    has_writes = 1;
  } else {
    /* Check with the lower layer */
    has_writes = chan->has_queued_writes(chan);
  }

  return has_writes;
}

/**
 * Check the is_bad_for_new_circs flag
 *
 * This function returns the is_bad_for_new_circs flag of the specified
 * channel.
 */

int
channel_is_bad_for_new_circs(channel_t *chan)
{
  tor_assert(chan);

  return chan->is_bad_for_new_circs;
}

/**
 * Mark a channel as bad for new circuits
 *
 * Set the is_bad_for_new_circs_flag on chan.
 */

void
channel_mark_bad_for_new_circs(channel_t *chan)
{
  tor_assert(chan);

  chan->is_bad_for_new_circs = 1;
}

/**
 * Get the client flag
 *
 * This returns the client flag of a channel, which will be set if
 * command_process_create_cell() in command.c thinks this is a connection
 * from a client.
 */

int
channel_is_client(channel_t *chan)
{
  tor_assert(chan);

  return chan->is_client;
}

/**
 * Set the client flag
 *
 * Mark a channel as being from a client
 */

void
channel_mark_client(channel_t *chan)
{
  tor_assert(chan);

  chan->is_client = 1;
}

/**
 * Get the canonical flag for a channel
 *
 * This returns the is_canonical for a channel; this flag is determined by
 * the lower layer and can't be set in a transport-independent way.
 */

int
channel_is_canonical(channel_t *chan)
{
  tor_assert(chan);
  tor_assert(chan->is_canonical);

  return chan->is_canonical(chan, 0);
}

/**
 * Test if the canonical flag is reliable
 *
 * This function asks if the lower layer thinks it's safe to trust the
 * result of channel_is_canonical()
 */

int
channel_is_canonical_is_reliable(channel_t *chan)
{
  tor_assert(chan);
  tor_assert(chan->is_canonical);

  return chan->is_canonical(chan, 1);
}

/**
 * Test incoming flag
 *
 * This function gets the incoming flag; this is set when a listener spawns
 * a channel.  If this returns true the channel was remotely initiated.
 */

int
channel_is_incoming(channel_t *chan)
{
  tor_assert(chan);

  return chan->is_incoming;
}

/**
 * Set the incoming flag
 *
 * This function is called when a channel arrives on a listening channel
 * to mark it as incoming.
 */

void
channel_mark_incoming(channel_t *chan)
{
  tor_assert(chan);

  chan->is_incoming = 1;
}

/**
 * Test local flag
 *
 * This function gets the local flag; the lower layer should set this when
 * setting up the channel if is_local_addr() is true for all of the
 * destinations it will communicate with on behalf of this channel.  It's
 * used to decide whether to declare the network reachable when seeing incoming
 * traffic on the channel.
 */

int
channel_is_local(channel_t *chan)
{
  tor_assert(chan);

  return chan->is_local;
}

/**
 * Set the local flag
 *
 * This internal-only function should be called by the lower layer if the
 * channel is to a local address.  See channel_is_local() above or the
 * description of the is_local bit in channel.h
 */

void
channel_mark_local(channel_t *chan)
{
  tor_assert(chan);

  chan->is_local = 1;
}

/**
 * Mark a channel as remote
 *
 * This internal-only function should be called by the lower layer if the
 * channel is not to a local address but has previously been marked local.
 * See channel_is_local() above or the description of the is_local bit in
 * channel.h
 */

void
channel_mark_remote(channel_t *chan)
{
  tor_assert(chan);

  chan->is_local = 0;
}

/**
 * Test outgoing flag
 *
 * This function gets the outgoing flag; this is the inverse of the incoming
 * bit set when a listener spawns a channel.  If this returns true the channel
 * was locally initiated.
 */

int
channel_is_outgoing(channel_t *chan)
{
  tor_assert(chan);

  return !(chan->is_incoming);
}

/**
 * Mark a channel as outgoing
 *
 * This function clears the incoming flag and thus marks a channel as
 * outgoing.
 */

void
channel_mark_outgoing(channel_t *chan)
{
  tor_assert(chan);

  chan->is_incoming = 0;
}

/*********************
 * Timestamp updates *
 ********************/

/**
 * Update the created timestamp for a channel
 *
 * This updates the channel's created timestamp and should only be called
 * from channel_init().
 */

void
channel_timestamp_created(channel_t *chan)
{
  time_t now = time(NULL);

  tor_assert(chan);

  chan->timestamp_created = now;
}

/**
 * Update the created timestamp for a channel listener
 *
 * This updates the channel listener's created timestamp and should only be
 * called from channel_init_listener().
 */

void
channel_listener_timestamp_created(channel_listener_t *chan_l)
{
  time_t now = time(NULL);

  tor_assert(chan_l);

  chan_l->timestamp_created = now;
}

/**
 * Update the last active timestamp for a channel
 *
 * This function updates the channel's last active timestamp; it should be
 * called by the lower layer whenever there is activity on the channel which
 * does not lead to a cell being transmitted or received; the active timestamp
 * is also updated from channel_timestamp_recv() and channel_timestamp_xmit(),
 * but it should be updated for things like the v3 handshake and stuff that
 * produce activity only visible to the lower layer.
 */

void
channel_timestamp_active(channel_t *chan)
{
  time_t now = time(NULL);

  tor_assert(chan);

  chan->timestamp_active = now;
}

/**
 * Update the last active timestamp for a channel listener
 */

void
channel_listener_timestamp_active(channel_listener_t *chan_l)
{
  time_t now = time(NULL);

  tor_assert(chan_l);

  chan_l->timestamp_active = now;
}

/**
 * Update the last accepted timestamp.
 *
 * This function updates the channel listener's last accepted timestamp; it
 * should be called whenever a new incoming channel is accepted on a
 * listener.
 */

void
channel_listener_timestamp_accepted(channel_listener_t *chan_l)
{
  time_t now = time(NULL);

  tor_assert(chan_l);

  chan_l->timestamp_active = now;
  chan_l->timestamp_accepted = now;
}

/**
 * Update client timestamp
 *
 * This function is called by relay.c to timestamp a channel that appears to
 * be used as a client.
 */

void
channel_timestamp_client(channel_t *chan)
{
  time_t now = time(NULL);

  tor_assert(chan);

  chan->timestamp_client = now;
}

/**
 * Update the last drained timestamp
 *
 * This is called whenever we transmit a cell which leaves the outgoing cell
 * queue completely empty.  It also updates the xmit time and the active time.
 */

void
channel_timestamp_drained(channel_t *chan)
{
  time_t now = time(NULL);

  tor_assert(chan);

  chan->timestamp_active = now;
  chan->timestamp_drained = now;
  chan->timestamp_xmit = now;
}

/**
 * Update the recv timestamp
 *
 * This is called whenever we get an incoming cell from the lower layer.
 * This also updates the active timestamp.
 */

void
channel_timestamp_recv(channel_t *chan)
{
  time_t now = time(NULL);

  tor_assert(chan);

  chan->timestamp_active = now;
  chan->timestamp_recv = now;
}

/**
 * Update the xmit timestamp
 * This is called whenever we pass an outgoing cell to the lower layer.  This
 * also updates the active timestamp.
 */

void
channel_timestamp_xmit(channel_t *chan)
{
  time_t now = time(NULL);

  tor_assert(chan);

  chan->timestamp_active = now;
  chan->timestamp_xmit = now;
}

/***************************************************************
 * Timestamp queries - see above for definitions of timestamps *
 **************************************************************/

/**
 * Query created timestamp for a channel
 */

time_t
channel_when_created(channel_t *chan)
{
  tor_assert(chan);

  return chan->timestamp_created;
}

/**
 * Query created timestamp for a channel listener
 */

time_t
channel_listener_when_created(channel_listener_t *chan_l)
{
  tor_assert(chan_l);

  return chan_l->timestamp_created;
}

/**
 * Query last active timestamp for a channel
 */

time_t
channel_when_last_active(channel_t *chan)
{
  tor_assert(chan);

  return chan->timestamp_active;
}

/**
 * Query last active timestamp for a channel listener
 */

time_t
channel_listener_when_last_active(channel_listener_t *chan_l)
{
  tor_assert(chan_l);

  return chan_l->timestamp_active;
}

/**
 * Query last accepted timestamp for a channel listener
 */

time_t
channel_listener_when_last_accepted(channel_listener_t *chan_l)
{
  tor_assert(chan_l);

  return chan_l->timestamp_accepted;
}

/**
 * Query client timestamp
 */

time_t
channel_when_last_client(channel_t *chan)
{
  tor_assert(chan);

  return chan->timestamp_client;
}

/**
 * Query drained timestamp
 */

time_t
channel_when_last_drained(channel_t *chan)
{
  tor_assert(chan);

  return chan->timestamp_drained;
}

/**
 * Query recv timestamp
 */

time_t
channel_when_last_recv(channel_t *chan)
{
  tor_assert(chan);

  return chan->timestamp_recv;
}

/**
 * Query xmit timestamp
 */

time_t
channel_when_last_xmit(channel_t *chan)
{
  tor_assert(chan);

  return chan->timestamp_xmit;
}

/**
 * Query accepted counter
 */

uint64_t
channel_listener_count_accepted(channel_listener_t *chan_l)
{
  tor_assert(chan_l);

  return chan_l->n_accepted;
}

/**
 * Query received cell counter
 */

uint64_t
channel_count_recved(channel_t *chan)
{
  tor_assert(chan);

  return chan->n_cells_recved;
}

/**
 * Query transmitted cell counter
 */

uint64_t
channel_count_xmitted(channel_t *chan)
{
  tor_assert(chan);

  return chan->n_cells_xmitted;
}

/**
 * Check if a channel matches an extend_info_t
 *
 * This function calls the lower layer and asks if this channel matches a
 * given extend_info_t.
 */

int
channel_matches_extend_info(channel_t *chan, extend_info_t *extend_info)
{
  tor_assert(chan);
  tor_assert(chan->matches_extend_info);
  tor_assert(extend_info);

  return chan->matches_extend_info(chan, extend_info);
}

/**
 * Check if a channel matches a given target address; return true iff we do.
 *
 * This function calls into the lower layer and asks if this channel thinks
 * it matches a given target address for circuit extension purposes.
 */

int
channel_matches_target_addr_for_extend(channel_t *chan,
                                       const tor_addr_t *target)
{
  tor_assert(chan);
  tor_assert(chan->matches_target);
  tor_assert(target);

  return chan->matches_target(chan, target);
}

/**
 * Return the total number of circuits used by a channel
 *
 * @param chan Channel to query
 * @return Number of circuits using this as n_chan or p_chan
 */

unsigned int
channel_num_circuits(channel_t *chan)
{
  tor_assert(chan);

  return chan->num_n_circuits +
         chan->num_p_circuits;
}

/**
 * Set up circuit ID generation
 *
 * This is called when setting up a channel and replaces the old
 * connection_or_set_circid_type()
 */
void
channel_set_circid_type(channel_t *chan,
                        crypto_pk_t *identity_rcvd,
                        int consider_identity)
{
  int started_here;
  crypto_pk_t *our_identity;

  tor_assert(chan);

  started_here = channel_is_outgoing(chan);

  if (! consider_identity) {
    if (started_here)
      chan->circ_id_type = CIRC_ID_TYPE_HIGHER;
    else
      chan->circ_id_type = CIRC_ID_TYPE_LOWER;
    return;
  }

  our_identity = started_here ?
    get_tlsclient_identity_key() : get_server_identity_key();

  if (identity_rcvd) {
    if (crypto_pk_cmp_keys(our_identity, identity_rcvd) < 0) {
      chan->circ_id_type = CIRC_ID_TYPE_LOWER;
    } else {
      chan->circ_id_type = CIRC_ID_TYPE_HIGHER;
    }
  } else {
    chan->circ_id_type = CIRC_ID_TYPE_NEITHER;
  }
}

