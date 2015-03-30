/* Copyright 2001 Matej Pfajfar.
 * Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file circuitlist.c
 * \brief Manage the global circuit list.
 **/
#define CIRCUITLIST_PRIVATE
#include "or.h"
#include "channel.h"
#include "circpathbias.h"
#include "circuitbuild.h"
#include "circuitlist.h"
#include "circuituse.h"
#include "circuitstats.h"
#include "connection.h"
#include "config.h"
#include "connection_edge.h"
#include "connection_or.h"
#include "control.h"
#include "networkstatus.h"
#include "nodelist.h"
#include "onion.h"
#include "onion_fast.h"
#include "policies.h"
#include "relay.h"
#include "rendclient.h"
#include "rendcommon.h"
#include "rephist.h"
#include "routerlist.h"
#include "routerset.h"

#include "ht.h"

/********* START VARIABLES **********/

/** A global list of all circuits at this hop. */
struct global_circuitlist_s global_circuitlist =
  TOR_LIST_HEAD_INITIALIZER(global_circuitlist);

/** A list of all the circuits in CIRCUIT_STATE_CHAN_WAIT. */
static smartlist_t *circuits_pending_chans = NULL;

static void circuit_free_cpath_node(crypt_path_t *victim);
static void cpath_ref_decref(crypt_path_reference_t *cpath_ref);
//static void circuit_set_rend_token(or_circuit_t *circ, int is_rend_circ,
//                                   const uint8_t *token);
static void circuit_clear_rend_token(or_circuit_t *circ);

/********* END VARIABLES ************/

/** A map from channel and circuit ID to circuit.  (Lookup performance is
 * very important here, since we need to do it every time a cell arrives.) */
typedef struct chan_circid_circuit_map_t {
  HT_ENTRY(chan_circid_circuit_map_t) node;
  channel_t *chan;
  circid_t circ_id;
  circuit_t *circuit;
  /* For debugging 12184: when was this placeholder item added? */
  time_t made_placeholder_at;
} chan_circid_circuit_map_t;

/** Helper for hash tables: compare the channel and circuit ID for a and
 * b, and return less than, equal to, or greater than zero appropriately.
 */
static INLINE int
chan_circid_entries_eq_(chan_circid_circuit_map_t *a,
                        chan_circid_circuit_map_t *b)
{
  return a->chan == b->chan && a->circ_id == b->circ_id;
}

/** Helper: return a hash based on circuit ID and the pointer value of
 * chan in <b>a</b>. */
static INLINE unsigned int
chan_circid_entry_hash_(chan_circid_circuit_map_t *a)
{
  /* Try to squeze the siphash input into 8 bytes to save any extra siphash
   * rounds.  This hash function is in the critical path. */
  uintptr_t chan = (uintptr_t) (void*) a->chan;
  uint32_t array[2];
  array[0] = a->circ_id;
  /* The low bits of the channel pointer are uninteresting, since the channel
   * is a pretty big structure. */
  array[1] = (uint32_t) (chan >> 6);
  return (unsigned) siphash24g(array, sizeof(array));
}

/** Map from [chan,circid] to circuit. */
static HT_HEAD(chan_circid_map, chan_circid_circuit_map_t)
     chan_circid_map = HT_INITIALIZER();
HT_PROTOTYPE(chan_circid_map, chan_circid_circuit_map_t, node,
             chan_circid_entry_hash_, chan_circid_entries_eq_)
HT_GENERATE(chan_circid_map, chan_circid_circuit_map_t, node,
            chan_circid_entry_hash_, chan_circid_entries_eq_, 0.6,
            malloc, realloc, free)

/** The most recently returned entry from circuit_get_by_circid_chan;
 * used to improve performance when many cells arrive in a row from the
 * same circuit.
 */
chan_circid_circuit_map_t *_last_circid_chan_ent = NULL;

/** Implementation helper for circuit_set_{p,n}_circid_channel: A circuit ID
 * and/or channel for circ has just changed from <b>old_chan, old_id</b>
 * to <b>chan, id</b>.  Adjust the chan,circid map as appropriate, removing
 * the old entry (if any) and adding a new one. */
static void
circuit_set_circid_chan_helper(circuit_t *circ, int direction,
                               circid_t id,
                               channel_t *chan)
{
  chan_circid_circuit_map_t search;
  chan_circid_circuit_map_t *found;
  channel_t *old_chan, **chan_ptr;
  circid_t old_id, *circid_ptr;
  int make_active, attached = 0;

  if (direction == CELL_DIRECTION_OUT) {
    chan_ptr = &circ->n_chan;
    circid_ptr = &circ->n_circ_id;
    make_active = circ->n_chan_cells.n > 0;
  } else {
    or_circuit_t *c = TO_OR_CIRCUIT(circ);
    chan_ptr = &c->p_chan;
    circid_ptr = &c->p_circ_id;
    make_active = c->p_chan_cells.n > 0;
  }
  old_chan = *chan_ptr;
  old_id = *circid_ptr;

  if (id == old_id && chan == old_chan)
    return;

  if (_last_circid_chan_ent &&
      ((old_id == _last_circid_chan_ent->circ_id &&
        old_chan == _last_circid_chan_ent->chan) ||
       (id == _last_circid_chan_ent->circ_id &&
        chan == _last_circid_chan_ent->chan))) {
    _last_circid_chan_ent = NULL;
  }

  if (old_chan) {
    /*
     * If we're changing channels or ID and had an old channel and a non
     * zero old ID and weren't marked for close (i.e., we should have been
     * attached), detach the circuit. ID changes require this because
     * circuitmux hashes on (channel_id, circuit_id).
     */
    if (old_id != 0 && (old_chan != chan || old_id != id) &&
        !(circ->marked_for_close)) {
      tor_assert(old_chan->cmux);
      circuitmux_detach_circuit(old_chan->cmux, circ);
    }

    /* we may need to remove it from the conn-circid map */
    search.circ_id = old_id;
    search.chan = old_chan;
    found = HT_REMOVE(chan_circid_map, &chan_circid_map, &search);
    if (found) {
      tor_free(found);
      if (direction == CELL_DIRECTION_OUT) {
        /* One fewer circuits use old_chan as n_chan */
        --(old_chan->num_n_circuits);
      } else {
        /* One fewer circuits use old_chan as p_chan */
        --(old_chan->num_p_circuits);
      }
    }
  }

  /* Change the values only after we have possibly made the circuit inactive
   * on the previous chan. */
  *chan_ptr = chan;
  *circid_ptr = id;

  if (chan == NULL)
    return;

  /* now add the new one to the conn-circid map */
  search.circ_id = id;
  search.chan = chan;
  found = HT_FIND(chan_circid_map, &chan_circid_map, &search);
  if (found) {
    found->circuit = circ;
    found->made_placeholder_at = 0;
  } else {
    found = tor_malloc_zero(sizeof(chan_circid_circuit_map_t));
    found->circ_id = id;
    found->chan = chan;
    found->circuit = circ;
    HT_INSERT(chan_circid_map, &chan_circid_map, found);
  }

  /*
   * Attach to the circuitmux if we're changing channels or IDs and
   * have a new channel and ID to use and the circuit is not marked for
   * close.
   */
  if (chan && id != 0 && (old_chan != chan || old_id != id) &&
      !(circ->marked_for_close)) {
    tor_assert(chan->cmux);
    circuitmux_attach_circuit(chan->cmux, circ, direction);
    attached = 1;
  }

  /*
   * This is a no-op if we have no cells, but if we do it marks us active to
   * the circuitmux
   */
  if (make_active && attached)
    update_circuit_on_cmux(circ, direction);

  /* Adjust circuit counts on new channel */
  if (direction == CELL_DIRECTION_OUT) {
    ++chan->num_n_circuits;
  } else {
    ++chan->num_p_circuits;
  }
}

/** Mark that circuit id <b>id</b> shouldn't be used on channel <b>chan</b>,
 * even if there is no circuit on the channel. We use this to keep the
 * circuit id from getting re-used while we have queued but not yet sent
 * a destroy cell. */
void
channel_mark_circid_unusable(channel_t *chan, circid_t id)
{
  chan_circid_circuit_map_t search;
  chan_circid_circuit_map_t *ent;

  /* See if there's an entry there. That wouldn't be good. */
  memset(&search, 0, sizeof(search));
  search.chan = chan;
  search.circ_id = id;
  ent = HT_FIND(chan_circid_map, &chan_circid_map, &search);

  if (ent && ent->circuit) {
    /* we have a problem. */
    log_warn(LD_BUG, "Tried to mark %u unusable on %p, but there was already "
             "a circuit there.", (unsigned)id, chan);
  } else if (ent) {
    /* It's already marked. */
    if (!ent->made_placeholder_at)
      ent->made_placeholder_at = approx_time();
  } else {
    ent = tor_malloc_zero(sizeof(chan_circid_circuit_map_t));
    ent->chan = chan;
    ent->circ_id = id;
    /* leave circuit at NULL. */
    ent->made_placeholder_at = approx_time();
    HT_INSERT(chan_circid_map, &chan_circid_map, ent);
  }
}

/** Mark that a circuit id <b>id</b> can be used again on <b>chan</b>.
 * We use this to re-enable the circuit ID after we've sent a destroy cell.
 */
void
channel_mark_circid_usable(channel_t *chan, circid_t id)
{
  chan_circid_circuit_map_t search;
  chan_circid_circuit_map_t *ent;

  /* See if there's an entry there. That wouldn't be good. */
  memset(&search, 0, sizeof(search));
  search.chan = chan;
  search.circ_id = id;
  ent = HT_REMOVE(chan_circid_map, &chan_circid_map, &search);
  if (ent && ent->circuit) {
    log_warn(LD_BUG, "Tried to mark %u usable on %p, but there was already "
             "a circuit there.", (unsigned)id, chan);
    return;
  }
  if (_last_circid_chan_ent == ent)
    _last_circid_chan_ent = NULL;
  tor_free(ent);
}

/** Called to indicate that a DESTROY is pending on <b>chan</b> with
 * circuit ID <b>id</b>, but hasn't been sent yet. */
void
channel_note_destroy_pending(channel_t *chan, circid_t id)
{
  circuit_t *circ = circuit_get_by_circid_channel_even_if_marked(id,chan);
  if (circ) {
    if (circ->n_chan == chan && circ->n_circ_id == id) {
      circ->n_delete_pending = 1;
    } else {
      or_circuit_t *orcirc = TO_OR_CIRCUIT(circ);
      if (orcirc->p_chan == chan && orcirc->p_circ_id == id) {
        circ->p_delete_pending = 1;
      }
    }
    return;
  }
  channel_mark_circid_unusable(chan, id);
}

/** Called to indicate that a DESTROY is no longer pending on <b>chan</b> with
 * circuit ID <b>id</b> -- typically, because it has been sent. */
void
channel_note_destroy_not_pending(channel_t *chan, circid_t id)
{
  circuit_t *circ = circuit_get_by_circid_channel_even_if_marked(id,chan);
  if (circ) {
    if (circ->n_chan == chan && circ->n_circ_id == id) {
      circ->n_delete_pending = 0;
    } else {
      or_circuit_t *orcirc = TO_OR_CIRCUIT(circ);
      if (orcirc->p_chan == chan && orcirc->p_circ_id == id) {
        circ->p_delete_pending = 0;
      }
    }
    /* XXXX this shouldn't happen; log a bug here. */
    return;
  }
  channel_mark_circid_usable(chan, id);
}

/** Set the p_conn field of a circuit <b>circ</b>, along
 * with the corresponding circuit ID, and add the circuit as appropriate
 * to the (chan,id)-\>circuit map. */
void
circuit_set_p_circid_chan(or_circuit_t *or_circ, circid_t id,
                          channel_t *chan)
{
  circuit_t *circ = TO_CIRCUIT(or_circ);
  channel_t *old_chan = or_circ->p_chan;
  circid_t old_id = or_circ->p_circ_id;

  circuit_set_circid_chan_helper(circ, CELL_DIRECTION_IN, id, chan);

  if (chan) {
    tor_assert(bool_eq(or_circ->p_chan_cells.n,
                       or_circ->next_active_on_p_chan));

    chan->timestamp_last_had_circuits = approx_time();
  }

  if (circ->p_delete_pending && old_chan) {
    channel_mark_circid_unusable(old_chan, old_id);
    circ->p_delete_pending = 0;
  }
}

/** Set the n_conn field of a circuit <b>circ</b>, along
 * with the corresponding circuit ID, and add the circuit as appropriate
 * to the (chan,id)-\>circuit map. */
void
circuit_set_n_circid_chan(circuit_t *circ, circid_t id,
                          channel_t *chan)
{
  channel_t *old_chan = circ->n_chan;
  circid_t old_id = circ->n_circ_id;

  circuit_set_circid_chan_helper(circ, CELL_DIRECTION_OUT, id, chan);

  if (chan) {
    tor_assert(bool_eq(circ->n_chan_cells.n, circ->next_active_on_n_chan));

    chan->timestamp_last_had_circuits = approx_time();
  }

  if (circ->n_delete_pending && old_chan) {
    channel_mark_circid_unusable(old_chan, old_id);
    circ->n_delete_pending = 0;
  }
}

/** Change the state of <b>circ</b> to <b>state</b>, adding it to or removing
 * it from lists as appropriate. */
void
circuit_set_state(circuit_t *circ, uint8_t state)
{
  tor_assert(circ);
  if (state == circ->state)
    return;
  if (!circuits_pending_chans)
    circuits_pending_chans = smartlist_new();
  if (circ->state == CIRCUIT_STATE_CHAN_WAIT) {
    /* remove from waiting-circuit list. */
    smartlist_remove(circuits_pending_chans, circ);
  }
  if (state == CIRCUIT_STATE_CHAN_WAIT) {
    /* add to waiting-circuit list. */
    smartlist_add(circuits_pending_chans, circ);
  }
  if (state == CIRCUIT_STATE_OPEN)
    tor_assert(!circ->n_chan_create_cell);
  circ->state = state;
}

/** Append to <b>out</b> all circuits in state CHAN_WAIT waiting for
 * the given connection. */
void
circuit_get_all_pending_on_channel(smartlist_t *out, channel_t *chan)
{
  tor_assert(out);
  tor_assert(chan);

  if (!circuits_pending_chans)
    return;

  SMARTLIST_FOREACH_BEGIN(circuits_pending_chans, circuit_t *, circ) {
    if (circ->marked_for_close)
      continue;
    if (!circ->n_hop)
      continue;
    tor_assert(circ->state == CIRCUIT_STATE_CHAN_WAIT);
    if (tor_digest_is_zero(circ->n_hop->identity_digest)) {
      /* Look at addr/port. This is an unkeyed connection. */
      if (!channel_matches_extend_info(chan, circ->n_hop))
        continue;
    } else {
      /* We expected a key. See if it's the right one. */
      if (tor_memneq(chan->identity_digest,
                     circ->n_hop->identity_digest, DIGEST_LEN))
        continue;
    }
    smartlist_add(out, circ);
  } SMARTLIST_FOREACH_END(circ);
}

/** Return the number of circuits in state CHAN_WAIT, waiting for the given
 * channel. */
int
circuit_count_pending_on_channel(channel_t *chan)
{
  int cnt;
  smartlist_t *sl = smartlist_new();

  tor_assert(chan);

  circuit_get_all_pending_on_channel(sl, chan);
  cnt = smartlist_len(sl);
  smartlist_free(sl);
  log_debug(LD_CIRC,"or_conn to %s at %s, %d pending circs",
            chan->nickname ? chan->nickname : "NULL",
            channel_get_canonical_remote_descr(chan),
            cnt);
  return cnt;
}

/** Detach from the global circuit list, and deallocate, all
 * circuits that have been marked for close.
 */
void
circuit_close_all_marked(void)
{
  circuit_t *circ, *tmp;
  TOR_LIST_FOREACH_SAFE(circ, &global_circuitlist, head, tmp)
    if (circ->marked_for_close)
      circuit_free(circ);
}

/** Return the head of the global linked list of circuits. */
MOCK_IMPL(struct global_circuitlist_s *,
circuit_get_global_list,(void))
{
  return &global_circuitlist;
}

/** Function to make circ-\>state human-readable */
const char *
circuit_state_to_string(int state)
{
  static char buf[64];
  switch (state) {
    case CIRCUIT_STATE_BUILDING: return "doing handshakes";
    case CIRCUIT_STATE_ONIONSKIN_PENDING: return "processing the onion";
    case CIRCUIT_STATE_CHAN_WAIT: return "connecting to server";
    case CIRCUIT_STATE_OPEN: return "open";
    default:
      log_warn(LD_BUG, "Unknown circuit state %d", state);
      tor_snprintf(buf, sizeof(buf), "unknown state [%d]", state);
      return buf;
  }
}

/** Map a circuit purpose to a string suitable to be displayed to a
 * controller. */
const char *
circuit_purpose_to_controller_string(uint8_t purpose)
{
  static char buf[32];
  switch (purpose) {
    case CIRCUIT_PURPOSE_OR:
    case CIRCUIT_PURPOSE_INTRO_POINT:
    case CIRCUIT_PURPOSE_REND_POINT_WAITING:
    case CIRCUIT_PURPOSE_REND_ESTABLISHED:
      return "SERVER"; /* A controller should never see these, actually. */

    case CIRCUIT_PURPOSE_C_GENERAL:
      return "GENERAL";
    case CIRCUIT_PURPOSE_C_INTRODUCING:
    case CIRCUIT_PURPOSE_C_INTRODUCE_ACK_WAIT:
    case CIRCUIT_PURPOSE_C_INTRODUCE_ACKED:
      return "HS_CLIENT_INTRO";

    case CIRCUIT_PURPOSE_C_ESTABLISH_REND:
    case CIRCUIT_PURPOSE_C_REND_READY:
    case CIRCUIT_PURPOSE_C_REND_READY_INTRO_ACKED:
    case CIRCUIT_PURPOSE_C_REND_JOINED:
      return "HS_CLIENT_REND";

    case CIRCUIT_PURPOSE_S_ESTABLISH_INTRO:
    case CIRCUIT_PURPOSE_S_INTRO:
      return "HS_SERVICE_INTRO";

    case CIRCUIT_PURPOSE_S_CONNECT_REND:
    case CIRCUIT_PURPOSE_S_REND_JOINED:
      return "HS_SERVICE_REND";

    case CIRCUIT_PURPOSE_TESTING:
      return "TESTING";
    case CIRCUIT_PURPOSE_C_MEASURE_TIMEOUT:
      return "MEASURE_TIMEOUT";
    case CIRCUIT_PURPOSE_CONTROLLER:
      return "CONTROLLER";
    case CIRCUIT_PURPOSE_PATH_BIAS_TESTING:
      return "PATH_BIAS_TESTING";

    default:
      tor_snprintf(buf, sizeof(buf), "UNKNOWN_%d", (int)purpose);
      return buf;
  }
}

/** Return a string specifying the state of the hidden-service circuit
 * purpose <b>purpose</b>, or NULL if <b>purpose</b> is not a
 * hidden-service-related circuit purpose. */
const char *
circuit_purpose_to_controller_hs_state_string(uint8_t purpose)
{
  switch (purpose)
    {
    default:
      log_fn(LOG_WARN, LD_BUG,
             "Unrecognized circuit purpose: %d",
             (int)purpose);
      tor_fragile_assert();
      /* fall through */

    case CIRCUIT_PURPOSE_OR:
    case CIRCUIT_PURPOSE_C_GENERAL:
    case CIRCUIT_PURPOSE_C_MEASURE_TIMEOUT:
    case CIRCUIT_PURPOSE_TESTING:
    case CIRCUIT_PURPOSE_CONTROLLER:
    case CIRCUIT_PURPOSE_PATH_BIAS_TESTING:
      return NULL;

    case CIRCUIT_PURPOSE_INTRO_POINT:
      return "OR_HSSI_ESTABLISHED";
    case CIRCUIT_PURPOSE_REND_POINT_WAITING:
      return "OR_HSCR_ESTABLISHED";
    case CIRCUIT_PURPOSE_REND_ESTABLISHED:
      return "OR_HS_R_JOINED";

    case CIRCUIT_PURPOSE_C_INTRODUCING:
      return "HSCI_CONNECTING";
    case CIRCUIT_PURPOSE_C_INTRODUCE_ACK_WAIT:
      return "HSCI_INTRO_SENT";
    case CIRCUIT_PURPOSE_C_INTRODUCE_ACKED:
      return "HSCI_DONE";

    case CIRCUIT_PURPOSE_C_ESTABLISH_REND:
      return "HSCR_CONNECTING";
    case CIRCUIT_PURPOSE_C_REND_READY:
      return "HSCR_ESTABLISHED_IDLE";
    case CIRCUIT_PURPOSE_C_REND_READY_INTRO_ACKED:
      return "HSCR_ESTABLISHED_WAITING";
    case CIRCUIT_PURPOSE_C_REND_JOINED:
      return "HSCR_JOINED";

    case CIRCUIT_PURPOSE_S_ESTABLISH_INTRO:
      return "HSSI_CONNECTING";
    case CIRCUIT_PURPOSE_S_INTRO:
      return "HSSI_ESTABLISHED";

    case CIRCUIT_PURPOSE_S_CONNECT_REND:
      return "HSSR_CONNECTING";
    case CIRCUIT_PURPOSE_S_REND_JOINED:
      return "HSSR_JOINED";
    }
}

/** Return a human-readable string for the circuit purpose <b>purpose</b>. */
const char *
circuit_purpose_to_string(uint8_t purpose)
{
  static char buf[32];

  switch (purpose)
    {
    case CIRCUIT_PURPOSE_OR:
      return "Circuit at relay";
    case CIRCUIT_PURPOSE_INTRO_POINT:
      return "Acting as intro point";
    case CIRCUIT_PURPOSE_REND_POINT_WAITING:
      return "Acting as rendevous (pending)";
    case CIRCUIT_PURPOSE_REND_ESTABLISHED:
      return "Acting as rendevous (established)";
    case CIRCUIT_PURPOSE_C_GENERAL:
      return "General-purpose client";
    case CIRCUIT_PURPOSE_C_INTRODUCING:
      return "Hidden service client: Connecting to intro point";
    case CIRCUIT_PURPOSE_C_INTRODUCE_ACK_WAIT:
      return "Hidden service client: Waiting for ack from intro point";
    case CIRCUIT_PURPOSE_C_INTRODUCE_ACKED:
      return "Hidden service client: Received ack from intro point";
    case CIRCUIT_PURPOSE_C_ESTABLISH_REND:
      return "Hidden service client: Establishing rendezvous point";
    case CIRCUIT_PURPOSE_C_REND_READY:
      return "Hidden service client: Pending rendezvous point";
    case CIRCUIT_PURPOSE_C_REND_READY_INTRO_ACKED:
      return "Hidden service client: Pending rendezvous point (ack received)";
    case CIRCUIT_PURPOSE_C_REND_JOINED:
      return "Hidden service client: Active rendezvous point";
    case CIRCUIT_PURPOSE_C_MEASURE_TIMEOUT:
      return "Measuring circuit timeout";

    case CIRCUIT_PURPOSE_S_ESTABLISH_INTRO:
      return "Hidden service: Establishing introduction point";
    case CIRCUIT_PURPOSE_S_INTRO:
      return "Hidden service: Introduction point";
    case CIRCUIT_PURPOSE_S_CONNECT_REND:
      return "Hidden service: Connecting to rendezvous point";
    case CIRCUIT_PURPOSE_S_REND_JOINED:
      return "Hidden service: Active rendezvous point";

    case CIRCUIT_PURPOSE_TESTING:
      return "Testing circuit";

    case CIRCUIT_PURPOSE_CONTROLLER:
      return "Circuit made by controller";

    case CIRCUIT_PURPOSE_PATH_BIAS_TESTING:
      return "Path-bias testing circuit";

    default:
      tor_snprintf(buf, sizeof(buf), "UNKNOWN_%d", (int)purpose);
      return buf;
  }
}

/** Pick a reasonable package_window to start out for our circuits.
 * Originally this was hard-coded at 1000, but now the consensus votes
 * on the answer. See proposal 168. */
int32_t
circuit_initial_package_window(void)
{
  int32_t num = networkstatus_get_param(NULL, "circwindow", CIRCWINDOW_START,
                                        CIRCWINDOW_START_MIN,
                                        CIRCWINDOW_START_MAX);
  /* If the consensus tells us a negative number, we'd assert. */
  if (num < 0)
    num = CIRCWINDOW_START;
  return num;
}

/** Initialize the common elements in a circuit_t, and add it to the global
 * list. */
static void
init_circuit_base(circuit_t *circ)
{
  tor_gettimeofday(&circ->timestamp_created);

  // Gets reset when we send CREATE_FAST.
  // circuit_expire_building() expects these to be equal
  // until the orconn is built.
  circ->timestamp_began = circ->timestamp_created;

  circ->package_window = circuit_initial_package_window();
  circ->deliver_window = CIRCWINDOW_START;
  cell_queue_init(&circ->n_chan_cells);

  TOR_LIST_INSERT_HEAD(&global_circuitlist, circ, head);
}

/** Allocate space for a new circuit, initializing with <b>p_circ_id</b>
 * and <b>p_conn</b>. Add it to the global circuit list.
 */
origin_circuit_t *
origin_circuit_new(void)
{
  origin_circuit_t *circ;
  /* never zero, since a global ID of 0 is treated specially by the
   * controller */
  static uint32_t n_circuits_allocated = 1;

  circ = tor_malloc_zero(sizeof(origin_circuit_t));
  circ->base_.magic = ORIGIN_CIRCUIT_MAGIC;

  circ->next_stream_id = crypto_rand_int(1<<16);
  circ->global_identifier = n_circuits_allocated++;
  circ->remaining_relay_early_cells = MAX_RELAY_EARLY_CELLS_PER_CIRCUIT;
  circ->remaining_relay_early_cells -= crypto_rand_int(2);

  init_circuit_base(TO_CIRCUIT(circ));

  circuit_build_times_update_last_circ(get_circuit_build_times_mutable());

  return circ;
}

/** Allocate a new or_circuit_t, connected to <b>p_conn</b> as
 * <b>p_circ_id</b>.  If <b>p_conn</b> is NULL, the circuit is unattached. */
or_circuit_t *
or_circuit_new(circid_t p_circ_id, channel_t *p_chan)
{
  /* CircIDs */
  or_circuit_t *circ;

  circ = tor_malloc_zero(sizeof(or_circuit_t));
  circ->base_.magic = OR_CIRCUIT_MAGIC;

  if (p_chan)
    circuit_set_p_circid_chan(circ, p_circ_id, p_chan);

  circ->remaining_relay_early_cells = MAX_RELAY_EARLY_CELLS_PER_CIRCUIT;
  cell_queue_init(&circ->p_chan_cells);

  init_circuit_base(TO_CIRCUIT(circ));

  return circ;
}

/** Deallocate space associated with circ.
 */
STATIC void
circuit_free(circuit_t *circ)
{
  void *mem;
  size_t memlen;
  if (!circ)
    return;

  if (CIRCUIT_IS_ORIGIN(circ)) {
    origin_circuit_t *ocirc = TO_ORIGIN_CIRCUIT(circ);
    mem = ocirc;
    memlen = sizeof(origin_circuit_t);
    tor_assert(circ->magic == ORIGIN_CIRCUIT_MAGIC);
    if (ocirc->build_state) {
        extend_info_free(ocirc->build_state->chosen_exit);
        circuit_free_cpath_node(ocirc->build_state->pending_final_cpath);
        cpath_ref_decref(ocirc->build_state->service_pending_final_cpath_ref);
    }
    tor_free(ocirc->build_state);

    circuit_clear_cpath(ocirc);

    crypto_pk_free(ocirc->intro_key);
    rend_data_free(ocirc->rend_data);

    tor_free(ocirc->dest_address);
    if (ocirc->socks_username) {
      memwipe(ocirc->socks_username, 0x12, ocirc->socks_username_len);
      tor_free(ocirc->socks_username);
    }
    if (ocirc->socks_password) {
      memwipe(ocirc->socks_password, 0x06, ocirc->socks_password_len);
      tor_free(ocirc->socks_password);
    }
    addr_policy_list_free(ocirc->prepend_policy);
  } else {
    or_circuit_t *ocirc = TO_OR_CIRCUIT(circ);
    /* Remember cell statistics for this circuit before deallocating. */
    if (get_options()->CellStatistics)
      rep_hist_buffer_stats_add_circ(circ, time(NULL));
    mem = ocirc;
    memlen = sizeof(or_circuit_t);
    tor_assert(circ->magic == OR_CIRCUIT_MAGIC);

    crypto_cipher_free(ocirc->p_crypto);
    crypto_digest_free(ocirc->p_digest);
    crypto_cipher_free(ocirc->n_crypto);
    crypto_digest_free(ocirc->n_digest);

    circuit_clear_rend_token(ocirc);

    if (ocirc->rend_splice) {
      or_circuit_t *other = ocirc->rend_splice;
      tor_assert(other->base_.magic == OR_CIRCUIT_MAGIC);
      other->rend_splice = NULL;
    }

    /* remove from map. */
    circuit_set_p_circid_chan(ocirc, 0, NULL);

    /* Clear cell queue _after_ removing it from the map.  Otherwise our
     * "active" checks will be violated. */
    cell_queue_clear(&ocirc->p_chan_cells);
  }

  extend_info_free(circ->n_hop);
  tor_free(circ->n_chan_create_cell);

  TOR_LIST_REMOVE(circ, head);

  /* Remove from map. */
  circuit_set_n_circid_chan(circ, 0, NULL);

  /* Clear cell queue _after_ removing it from the map.  Otherwise our
   * "active" checks will be violated. */
  cell_queue_clear(&circ->n_chan_cells);

  memwipe(mem, 0xAA, memlen); /* poison memory */
  tor_free(mem);
}

/** Deallocate the linked list circ-><b>cpath</b>, and remove the cpath from
 * <b>circ</b>. */
void
circuit_clear_cpath(origin_circuit_t *circ)
{
  crypt_path_t *victim, *head, *cpath;

  head = cpath = circ->cpath;

  if (!cpath)
    return;

  /* it's a circular list, so we have to notice when we've
   * gone through it once. */
  while (cpath->next && cpath->next != head) {
    victim = cpath;
    cpath = victim->next;
    circuit_free_cpath_node(victim);
  }

  circuit_free_cpath_node(cpath);

  circ->cpath = NULL;
}

/** Release all storage held by circuits. */
void
circuit_free_all(void)
{
  circuit_t *tmp, *tmp2;

  TOR_LIST_FOREACH_SAFE(tmp, &global_circuitlist, head, tmp2) {
    if (! CIRCUIT_IS_ORIGIN(tmp)) {
      or_circuit_t *or_circ = TO_OR_CIRCUIT(tmp);
      while (or_circ->resolving_streams) {
        edge_connection_t *next_conn;
        next_conn = or_circ->resolving_streams->next_stream;
        connection_free(TO_CONN(or_circ->resolving_streams));
        or_circ->resolving_streams = next_conn;
      }
    }
    circuit_free(tmp);
  }

  smartlist_free(circuits_pending_chans);
  circuits_pending_chans = NULL;

  {
    chan_circid_circuit_map_t **elt, **next, *c;
    for (elt = HT_START(chan_circid_map, &chan_circid_map);
         elt;
         elt = next) {
      c = *elt;
      next = HT_NEXT_RMV(chan_circid_map, &chan_circid_map, elt);

      tor_assert(c->circuit == NULL);
      tor_free(c);
    }
  }
  HT_CLEAR(chan_circid_map, &chan_circid_map);
}

/** Deallocate space associated with the cpath node <b>victim</b>. */
static void
circuit_free_cpath_node(crypt_path_t *victim)
{
  if (!victim)
    return;

  crypto_cipher_free(victim->f_crypto);
  crypto_cipher_free(victim->b_crypto);
  crypto_digest_free(victim->f_digest);
  crypto_digest_free(victim->b_digest);
  onion_handshake_state_release(&victim->handshake_state);
  crypto_dh_free(victim->rend_dh_handshake_state);
  extend_info_free(victim->extend_info);

  memwipe(victim, 0xBB, sizeof(crypt_path_t)); /* poison memory */
  tor_free(victim);
}

/** Release a crypt_path_reference_t*, which may be NULL. */
static void
cpath_ref_decref(crypt_path_reference_t *cpath_ref)
{
  if (cpath_ref != NULL) {
    if (--(cpath_ref->refcount) == 0) {
      circuit_free_cpath_node(cpath_ref->cpath);
      tor_free(cpath_ref);
    }
  }
}

/** A helper function for circuit_dump_by_conn() below. Log a bunch
 * of information about circuit <b>circ</b>.
 */
static void
circuit_dump_conn_details(int severity,
                          circuit_t *circ,
                          int conn_array_index,
                          const char *type,
                          circid_t this_circid,
                          circid_t other_circid)
{
  tor_log(severity, LD_CIRC, "Conn %d has %s circuit: circID %u "
      "(other side %u), state %d (%s), born %ld:",
      conn_array_index, type, (unsigned)this_circid, (unsigned)other_circid,
      circ->state, circuit_state_to_string(circ->state),
      (long)circ->timestamp_began.tv_sec);
  if (CIRCUIT_IS_ORIGIN(circ)) { /* circ starts at this node */
    circuit_log_path(severity, LD_CIRC, TO_ORIGIN_CIRCUIT(circ));
  }
}

/** Log, at severity <b>severity</b>, information about each circuit
 * that is connected to <b>conn</b>.
 */
void
circuit_dump_by_conn(connection_t *conn, int severity)
{
  circuit_t *circ;
  edge_connection_t *tmpconn;

  TOR_LIST_FOREACH(circ, &global_circuitlist, head) {
    circid_t n_circ_id = circ->n_circ_id, p_circ_id = 0;

    if (circ->marked_for_close) {
      continue;
    }

    if (!CIRCUIT_IS_ORIGIN(circ)) {
      p_circ_id = TO_OR_CIRCUIT(circ)->p_circ_id;
    }

    if (CIRCUIT_IS_ORIGIN(circ)) {
      for (tmpconn=TO_ORIGIN_CIRCUIT(circ)->p_streams; tmpconn;
           tmpconn=tmpconn->next_stream) {
        if (TO_CONN(tmpconn) == conn) {
          circuit_dump_conn_details(severity, circ, conn->conn_array_index,
                                    "App-ward", p_circ_id, n_circ_id);
        }
      }
    }

    if (! CIRCUIT_IS_ORIGIN(circ)) {
      for (tmpconn=TO_OR_CIRCUIT(circ)->n_streams; tmpconn;
           tmpconn=tmpconn->next_stream) {
        if (TO_CONN(tmpconn) == conn) {
          circuit_dump_conn_details(severity, circ, conn->conn_array_index,
                                    "Exit-ward", n_circ_id, p_circ_id);
        }
      }
    }
  }
}

/** Return the circuit whose global ID is <b>id</b>, or NULL if no
 * such circuit exists. */
origin_circuit_t *
circuit_get_by_global_id(uint32_t id)
{
  circuit_t *circ;
  TOR_LIST_FOREACH(circ, &global_circuitlist, head) {
    if (CIRCUIT_IS_ORIGIN(circ) &&
        TO_ORIGIN_CIRCUIT(circ)->global_identifier == id) {
      if (circ->marked_for_close)
        return NULL;
      else
        return TO_ORIGIN_CIRCUIT(circ);
    }
  }
  return NULL;
}

/** Return a circ such that:
 *  - circ-\>n_circ_id or circ-\>p_circ_id is equal to <b>circ_id</b>, and
 *  - circ is attached to <b>chan</b>, either as p_chan or n_chan.
 * Return NULL if no such circuit exists.
 *
 * If <b>found_entry_out</b> is provided, set it to true if we have a
 * placeholder entry for circid/chan, and leave it unset otherwise.
 */
static INLINE circuit_t *
circuit_get_by_circid_channel_impl(circid_t circ_id, channel_t *chan,
                                   int *found_entry_out)
{
  chan_circid_circuit_map_t search;
  chan_circid_circuit_map_t *found;

  if (_last_circid_chan_ent &&
      circ_id == _last_circid_chan_ent->circ_id &&
      chan == _last_circid_chan_ent->chan) {
    found = _last_circid_chan_ent;
  } else {
    search.circ_id = circ_id;
    search.chan = chan;
    found = HT_FIND(chan_circid_map, &chan_circid_map, &search);
    _last_circid_chan_ent = found;
  }
  if (found && found->circuit) {
    log_debug(LD_CIRC,
              "circuit_get_by_circid_channel_impl() returning circuit %p for"
              " circ_id %u, channel ID " U64_FORMAT " (%p)",
              found->circuit, (unsigned)circ_id,
              U64_PRINTF_ARG(chan->global_identifier), chan);
    if (found_entry_out)
      *found_entry_out = 1;
    return found->circuit;
  }

  log_debug(LD_CIRC,
            "circuit_get_by_circid_channel_impl() found %s for"
            " circ_id %u, channel ID " U64_FORMAT " (%p)",
            found ? "placeholder" : "nothing",
            (unsigned)circ_id,
            U64_PRINTF_ARG(chan->global_identifier), chan);

  if (found_entry_out)
    *found_entry_out = found ? 1 : 0;

  return NULL;
  /* The rest of this checks for bugs. Disabled by default. */
  /* We comment it out because coverity complains otherwise.
  {
    circuit_t *circ;
    TOR_LIST_FOREACH(circ, &global_circuitlist, head) {
      if (! CIRCUIT_IS_ORIGIN(circ)) {
        or_circuit_t *or_circ = TO_OR_CIRCUIT(circ);
        if (or_circ->p_chan == chan && or_circ->p_circ_id == circ_id) {
          log_warn(LD_BUG,
                   "circuit matches p_chan, but not in hash table (Bug!)");
          return circ;
        }
      }
      if (circ->n_chan == chan && circ->n_circ_id == circ_id) {
        log_warn(LD_BUG,
                 "circuit matches n_chan, but not in hash table (Bug!)");
        return circ;
      }
    }
    return NULL;
  } */
}

/** Return a circ such that:
 *  - circ-\>n_circ_id or circ-\>p_circ_id is equal to <b>circ_id</b>, and
 *  - circ is attached to <b>chan</b>, either as p_chan or n_chan.
 *  - circ is not marked for close.
 * Return NULL if no such circuit exists.
 */
circuit_t *
circuit_get_by_circid_channel(circid_t circ_id, channel_t *chan)
{
  circuit_t *circ = circuit_get_by_circid_channel_impl(circ_id, chan, NULL);
  if (!circ || circ->marked_for_close)
    return NULL;
  else
    return circ;
}

/** Return a circ such that:
 *  - circ-\>n_circ_id or circ-\>p_circ_id is equal to <b>circ_id</b>, and
 *  - circ is attached to <b>chan</b>, either as p_chan or n_chan.
 * Return NULL if no such circuit exists.
 */
circuit_t *
circuit_get_by_circid_channel_even_if_marked(circid_t circ_id,
                                             channel_t *chan)
{
  return circuit_get_by_circid_channel_impl(circ_id, chan, NULL);
}

/** Return true iff the circuit ID <b>circ_id</b> is currently used by a
 * circuit, marked or not, on <b>chan</b>, or if the circ ID is reserved until
 * a queued destroy cell can be sent.
 *
 * (Return 1 if the circuit is present, marked or not; Return 2
 * if the circuit ID is pending a destroy.)
 **/
int
circuit_id_in_use_on_channel(circid_t circ_id, channel_t *chan)
{
  int found = 0;
  if (circuit_get_by_circid_channel_impl(circ_id, chan, &found) != NULL)
    return 1;
  if (found)
    return 2;
  return 0;
}

/** Helper for debugging 12184.  Returns the time since which 'circ_id' has
 * been marked unusable on 'chan'. */
time_t
circuit_id_when_marked_unusable_on_channel(circid_t circ_id, channel_t *chan)
{
  chan_circid_circuit_map_t search;
  chan_circid_circuit_map_t *found;

  memset(&search, 0, sizeof(search));
  search.circ_id = circ_id;
  search.chan = chan;

  found = HT_FIND(chan_circid_map, &chan_circid_map, &search);

  if (! found || found->circuit)
    return 0;

  return found->made_placeholder_at;
}

/** Return the circuit that a given edge connection is using. */
circuit_t *
circuit_get_by_edge_conn(edge_connection_t *conn)
{
  circuit_t *circ;

  circ = conn->on_circuit;
  tor_assert(!circ ||
             (CIRCUIT_IS_ORIGIN(circ) ? circ->magic == ORIGIN_CIRCUIT_MAGIC
                                      : circ->magic == OR_CIRCUIT_MAGIC));

  return circ;
}

/** For each circuit that has <b>chan</b> as n_chan or p_chan, unlink the
 * circuit from the chan,circid map, and mark it for close if it hasn't
 * been marked already.
 */
void
circuit_unlink_all_from_channel(channel_t *chan, int reason)
{
  smartlist_t *detached = smartlist_new();

/* #define DEBUG_CIRCUIT_UNLINK_ALL */

  channel_unlink_all_circuits(chan, detached);

#ifdef DEBUG_CIRCUIT_UNLINK_ALL
  {
    circuit_t *circ;
    smartlist_t *detached_2 = smartlist_new();
    int mismatch = 0, badlen = 0;

    TOR_LIST_FOREACH(circ, &global_circuitlist, head) {
      if (circ->n_chan == chan ||
          (!CIRCUIT_IS_ORIGIN(circ) &&
           TO_OR_CIRCUIT(circ)->p_chan == chan)) {
        smartlist_add(detached_2, circ);
      }
    }

    if (smartlist_len(detached) != smartlist_len(detached_2)) {
       log_warn(LD_BUG, "List of detached circuits had the wrong length! "
                "(got %d, should have gotten %d)",
                (int)smartlist_len(detached),
                (int)smartlist_len(detached_2));
       badlen = 1;
    }
    smartlist_sort_pointers(detached);
    smartlist_sort_pointers(detached_2);

    SMARTLIST_FOREACH(detached, circuit_t *, c,
        if (c != smartlist_get(detached_2, c_sl_idx))
          mismatch = 1;
    );

    if (mismatch)
      log_warn(LD_BUG, "Mismatch in list of detached circuits.");

    if (badlen || mismatch) {
      smartlist_free(detached);
      detached = detached_2;
    } else {
      log_notice(LD_CIRC, "List of %d circuits was as expected.",
                (int)smartlist_len(detached));
      smartlist_free(detached_2);
    }
  }
#endif

  SMARTLIST_FOREACH_BEGIN(detached, circuit_t *, circ) {
    int mark = 0;
    if (circ->n_chan == chan) {

      circuit_set_n_circid_chan(circ, 0, NULL);
      mark = 1;

      /* If we didn't request this closure, pass the remote
       * bit to mark_for_close. */
      if (chan->reason_for_closing != CHANNEL_CLOSE_REQUESTED)
        reason |= END_CIRC_REASON_FLAG_REMOTE;
    }
    if (! CIRCUIT_IS_ORIGIN(circ)) {
      or_circuit_t *or_circ = TO_OR_CIRCUIT(circ);
      if (or_circ->p_chan == chan) {
        circuit_set_p_circid_chan(or_circ, 0, NULL);
        mark = 1;
      }
    }
    if (!mark) {
      log_warn(LD_BUG, "Circuit on detached list which I had no reason "
          "to mark");
      continue;
    }
    if (!circ->marked_for_close)
      circuit_mark_for_close(circ, reason);
  } SMARTLIST_FOREACH_END(circ);

  smartlist_free(detached);
}

/** Return a circ such that
 *  - circ-\>rend_data-\>onion_address is equal to
 *    <b>rend_data</b>-\>onion_address,
 *  - circ-\>rend_data-\>rend_cookie is equal to
 *    <b>rend_data</b>-\>rend_cookie, and
 *  - circ-\>purpose is equal to CIRCUIT_PURPOSE_C_REND_READY.
 *
 * Return NULL if no such circuit exists.
 */
origin_circuit_t *
circuit_get_ready_rend_circ_by_rend_data(const rend_data_t *rend_data)
{
  circuit_t *circ;
  TOR_LIST_FOREACH(circ, &global_circuitlist, head) {
    if (!circ->marked_for_close &&
        circ->purpose == CIRCUIT_PURPOSE_C_REND_READY) {
      origin_circuit_t *ocirc = TO_ORIGIN_CIRCUIT(circ);
      if (ocirc->rend_data &&
          !rend_cmp_service_ids(rend_data->onion_address,
                                ocirc->rend_data->onion_address) &&
          tor_memeq(ocirc->rend_data->rend_cookie,
                    rend_data->rend_cookie,
                    REND_COOKIE_LEN))
        return ocirc;
    }
  }
  return NULL;
}

/** Return the first circuit originating here in global_circuitlist after
 * <b>start</b> whose purpose is <b>purpose</b>, and where
 * <b>digest</b> (if set) matches the rend_pk_digest field. Return NULL if no
 * circuit is found.  If <b>start</b> is NULL, begin at the start of the list.
 */
origin_circuit_t *
circuit_get_next_by_pk_and_purpose(origin_circuit_t *start,
                                   const char *digest, uint8_t purpose)
{
  circuit_t *circ;
  tor_assert(CIRCUIT_PURPOSE_IS_ORIGIN(purpose));
  if (start == NULL)
    circ = TOR_LIST_FIRST(&global_circuitlist);
  else
    circ = TOR_LIST_NEXT(TO_CIRCUIT(start), head);

  for ( ; circ; circ = TOR_LIST_NEXT(circ, head)) {
    if (circ->marked_for_close)
      continue;
    if (circ->purpose != purpose)
      continue;
    if (!digest)
      return TO_ORIGIN_CIRCUIT(circ);
    else if (TO_ORIGIN_CIRCUIT(circ)->rend_data &&
             tor_memeq(TO_ORIGIN_CIRCUIT(circ)->rend_data->rend_pk_digest,
                     digest, DIGEST_LEN))
      return TO_ORIGIN_CIRCUIT(circ);
  }
  return NULL;
}

/** Map from rendezvous cookie to or_circuit_t */
static digestmap_t *rend_cookie_map = NULL;

/** Map from introduction point digest to or_circuit_t */
static digestmap_t *intro_digest_map = NULL;

/** Return the OR circuit whose purpose is <b>purpose</b>, and whose
 * rend_token is the REND_TOKEN_LEN-byte <b>token</b>. If <b>is_rend_circ</b>,
 * look for rendezvous point circuits; otherwise look for introduction point
 * circuits. */
static or_circuit_t *
circuit_get_by_rend_token_and_purpose(uint8_t purpose, int is_rend_circ,
                                      const char *token)
{
  or_circuit_t *circ;
  digestmap_t *map = is_rend_circ ? rend_cookie_map : intro_digest_map;

  if (!map)
    return NULL;

  circ = digestmap_get(map, token);
  if (!circ ||
      circ->base_.purpose != purpose ||
      circ->base_.marked_for_close)
    return NULL;

  if (!circ->rendinfo) {
    char *t = tor_strdup(hex_str(token, REND_TOKEN_LEN));
    log_warn(LD_BUG, "Wanted a circuit with %s:%d, but lookup returned a "
             "circuit with no rendinfo set.",
             safe_str(t), is_rend_circ);
    tor_free(t);
    return NULL;
  }

  if (! bool_eq(circ->rendinfo->is_rend_circ, is_rend_circ) ||
      tor_memneq(circ->rendinfo->rend_token, token, REND_TOKEN_LEN)) {
    char *t = tor_strdup(hex_str(token, REND_TOKEN_LEN));
    log_warn(LD_BUG, "Wanted a circuit with %s:%d, but lookup returned %s:%d",
             safe_str(t), is_rend_circ,
             safe_str(hex_str(circ->rendinfo->rend_token, REND_TOKEN_LEN)),
             (int)circ->rendinfo->is_rend_circ);
    tor_free(t);
    return NULL;
  }

  return circ;
}

/** Clear the rendezvous cookie or introduction point key digest that's
 * configured on <b>circ</b>, if any, and remove it from any such maps. */
static void
circuit_clear_rend_token(or_circuit_t *circ)
{
  or_circuit_t *found_circ;
  digestmap_t *map;

  if (!circ || !circ->rendinfo)
    return;

  map = circ->rendinfo->is_rend_circ ? rend_cookie_map : intro_digest_map;

  if (!map) {
    log_warn(LD_BUG, "Tried to clear rend token on circuit, but found no map");
    return;
  }

  found_circ = digestmap_get(map, circ->rendinfo->rend_token);
  if (found_circ == circ) {
    /* Great, this is the right one. */
    digestmap_remove(map, circ->rendinfo->rend_token);
  } else if (found_circ) {
    log_warn(LD_BUG, "Tried to clear rend token on circuit, but "
             "it was already replaced in the map.");
  } else {
    log_warn(LD_BUG, "Tried to clear rend token on circuit, but "
             "it not in the map at all.");
  }

  tor_free(circ->rendinfo); /* Sets it to NULL too */
}

/** Set the rendezvous cookie (if is_rend_circ), or the introduction point
 * digest (if ! is_rend_circ) of <b>circ</b> to the REND_TOKEN_LEN-byte value
 * in <b>token</b>, and add it to the appropriate map.  If it previously had a
 * token, clear it.  If another circuit previously had the same
 * cookie/intro-digest, mark that circuit and remove it from the map. */
static void
circuit_set_rend_token(or_circuit_t *circ, int is_rend_circ,
                       const uint8_t *token)
{
  digestmap_t **map_p, *map;
  or_circuit_t *found_circ;

  /* Find the right map, creating it as needed */
  map_p = is_rend_circ ? &rend_cookie_map : &intro_digest_map;

  if (!*map_p)
    *map_p = digestmap_new();

  map = *map_p;

  /* If this circuit already has a token, we need to remove that. */
  if (circ->rendinfo)
    circuit_clear_rend_token(circ);

  if (token == NULL) {
    /* We were only trying to remove this token, not set a new one. */
    return;
  }

  found_circ = digestmap_get(map, (const char *)token);
  if (found_circ) {
    tor_assert(found_circ != circ);
    circuit_clear_rend_token(found_circ);
    if (! found_circ->base_.marked_for_close) {
      circuit_mark_for_close(TO_CIRCUIT(found_circ), END_CIRC_REASON_FINISHED);
      if (is_rend_circ) {
        log_fn(LOG_PROTOCOL_WARN, LD_REND,
               "Duplicate rendezvous cookie (%s...) used on two circuits",
               hex_str((const char*)token, 4)); /* only log first 4 chars */
      }
    }
  }

  /* Now set up the rendinfo */
  circ->rendinfo = tor_malloc(sizeof(*circ->rendinfo));
  memcpy(circ->rendinfo->rend_token, token, REND_TOKEN_LEN);
  circ->rendinfo->is_rend_circ = is_rend_circ ? 1 : 0;

  digestmap_set(map, (const char *)token, circ);
}

/** Return the circuit waiting for a rendezvous with the provided cookie.
 * Return NULL if no such circuit is found.
 */
or_circuit_t *
circuit_get_rendezvous(const uint8_t *cookie)
{
  return circuit_get_by_rend_token_and_purpose(
                                     CIRCUIT_PURPOSE_REND_POINT_WAITING,
                                     1, (const char*)cookie);
}

/** Return the circuit waiting for intro cells of the given digest.
 * Return NULL if no such circuit is found.
 */
or_circuit_t *
circuit_get_intro_point(const uint8_t *digest)
{
  return circuit_get_by_rend_token_and_purpose(
                                     CIRCUIT_PURPOSE_INTRO_POINT, 0,
                                     (const char *)digest);
}

/** Set the rendezvous cookie of <b>circ</b> to <b>cookie</b>.  If another
 * circuit previously had that cookie, mark it. */
void
circuit_set_rendezvous_cookie(or_circuit_t *circ, const uint8_t *cookie)
{
  circuit_set_rend_token(circ, 1, cookie);
}

/** Set the intro point key digest of <b>circ</b> to <b>cookie</b>.  If another
 * circuit previously had that intro point digest, mark it. */
void
circuit_set_intro_point_digest(or_circuit_t *circ, const uint8_t *digest)
{
  circuit_set_rend_token(circ, 0, digest);
}

/** Return a circuit that is open, is CIRCUIT_PURPOSE_C_GENERAL,
 * has a timestamp_dirty value of 0, has flags matching the CIRCLAUNCH_*
 * flags in <b>flags</b>, and if info is defined, does not already use info
 * as any of its hops; or NULL if no circuit fits this description.
 *
 * The <b>purpose</b> argument (currently ignored) refers to the purpose of
 * the circuit we want to create, not the purpose of the circuit we want to
 * cannibalize.
 *
 * If !CIRCLAUNCH_NEED_UPTIME, prefer returning non-uptime circuits.
 */
origin_circuit_t *
circuit_find_to_cannibalize(uint8_t purpose, extend_info_t *info,
                            int flags)
{
  circuit_t *circ_;
  origin_circuit_t *best=NULL;
  int need_uptime = (flags & CIRCLAUNCH_NEED_UPTIME) != 0;
  int need_capacity = (flags & CIRCLAUNCH_NEED_CAPACITY) != 0;
  int internal = (flags & CIRCLAUNCH_IS_INTERNAL) != 0;
  const or_options_t *options = get_options();

  /* Make sure we're not trying to create a onehop circ by
   * cannibalization. */
  tor_assert(!(flags & CIRCLAUNCH_ONEHOP_TUNNEL));

  log_debug(LD_CIRC,
            "Hunting for a circ to cannibalize: purpose %d, uptime %d, "
            "capacity %d, internal %d",
            purpose, need_uptime, need_capacity, internal);

  TOR_LIST_FOREACH(circ_, &global_circuitlist, head) {
    if (CIRCUIT_IS_ORIGIN(circ_) &&
        circ_->state == CIRCUIT_STATE_OPEN &&
        !circ_->marked_for_close &&
        circ_->purpose == CIRCUIT_PURPOSE_C_GENERAL &&
        !circ_->timestamp_dirty) {
      origin_circuit_t *circ = TO_ORIGIN_CIRCUIT(circ_);
      if ((!need_uptime || circ->build_state->need_uptime) &&
          (!need_capacity || circ->build_state->need_capacity) &&
          (internal == circ->build_state->is_internal) &&
          !circ->unusable_for_new_conns &&
          circ->remaining_relay_early_cells &&
          circ->build_state->desired_path_len == DEFAULT_ROUTE_LEN &&
          !circ->build_state->onehop_tunnel &&
          !circ->isolation_values_set) {
        if (info) {
          /* need to make sure we don't duplicate hops */
          crypt_path_t *hop = circ->cpath;
          const node_t *ri1 = node_get_by_id(info->identity_digest);
          do {
            const node_t *ri2;
            if (tor_memeq(hop->extend_info->identity_digest,
                        info->identity_digest, DIGEST_LEN))
              goto next;
            if (ri1 &&
                (ri2 = node_get_by_id(hop->extend_info->identity_digest))
                && nodes_in_same_family(ri1, ri2))
              goto next;
            hop=hop->next;
          } while (hop!=circ->cpath);
        }
        if (options->ExcludeNodes) {
          /* Make sure no existing nodes in the circuit are excluded for
           * general use.  (This may be possible if StrictNodes is 0, and we
           * thought we needed to use an otherwise excluded node for, say, a
           * directory operation.) */
          crypt_path_t *hop = circ->cpath;
          do {
            if (routerset_contains_extendinfo(options->ExcludeNodes,
                                              hop->extend_info))
              goto next;
            hop = hop->next;
          } while (hop != circ->cpath);
        }
        if (!best || (best->build_state->need_uptime && !need_uptime))
          best = circ;
      next: ;
      }
    }
  }
  return best;
}

/** Return the number of hops in circuit's path. */
int
circuit_get_cpath_len(origin_circuit_t *circ)
{
  int n = 0;
  if (circ && circ->cpath) {
    crypt_path_t *cpath, *cpath_next = NULL;
    for (cpath = circ->cpath; cpath_next != circ->cpath; cpath = cpath_next) {
      cpath_next = cpath->next;
      ++n;
    }
  }
  return n;
}

/** Return the <b>hopnum</b>th hop in <b>circ</b>->cpath, or NULL if there
 * aren't that many hops in the list. */
crypt_path_t *
circuit_get_cpath_hop(origin_circuit_t *circ, int hopnum)
{
  if (circ && circ->cpath && hopnum > 0) {
    crypt_path_t *cpath, *cpath_next = NULL;
    for (cpath = circ->cpath; cpath_next != circ->cpath; cpath = cpath_next) {
      cpath_next = cpath->next;
      if (--hopnum <= 0)
        return cpath;
    }
  }
  return NULL;
}

/** Go through the circuitlist; mark-for-close each circuit that starts
 *  at us but has not yet been used. */
void
circuit_mark_all_unused_circs(void)
{
  circuit_t *circ;
  TOR_LIST_FOREACH(circ, &global_circuitlist, head) {
    if (CIRCUIT_IS_ORIGIN(circ) &&
        !circ->marked_for_close &&
        !circ->timestamp_dirty)
      circuit_mark_for_close(circ, END_CIRC_REASON_FINISHED);
  }
}

/** Go through the circuitlist; for each circuit that starts at us
 * and is dirty, frob its timestamp_dirty so we won't use it for any
 * new streams.
 *
 * This is useful for letting the user change pseudonyms, so new
 * streams will not be linkable to old streams.
 */
void
circuit_mark_all_dirty_circs_as_unusable(void)
{
  circuit_t *circ;
  TOR_LIST_FOREACH(circ, &global_circuitlist, head) {
    if (CIRCUIT_IS_ORIGIN(circ) &&
        !circ->marked_for_close &&
        circ->timestamp_dirty) {
      mark_circuit_unusable_for_new_conns(TO_ORIGIN_CIRCUIT(circ));
    }
  }
}

/** Mark <b>circ</b> to be closed next time we call
 * circuit_close_all_marked(). Do any cleanup needed:
 *   - If state is onionskin_pending, remove circ from the onion_pending
 *     list.
 *   - If circ isn't open yet: call circuit_build_failed() if we're
 *     the origin, and in either case call circuit_rep_hist_note_result()
 *     to note stats.
 *   - If purpose is C_INTRODUCE_ACK_WAIT, report the intro point
 *     failure we just had to the hidden service client module.
 *   - If purpose is C_INTRODUCING and <b>reason</b> isn't TIMEOUT,
 *     report to the hidden service client module that the intro point
 *     we just tried may be unreachable.
 *   - Send appropriate destroys and edge_destroys for conns and
 *     streams attached to circ.
 *   - If circ->rend_splice is set (we are the midpoint of a joined
 *     rendezvous stream), then mark the other circuit to close as well.
 */
MOCK_IMPL(void,
circuit_mark_for_close_, (circuit_t *circ, int reason, int line,
                          const char *file))
{
  int orig_reason = reason; /* Passed to the controller */
  assert_circuit_ok(circ);
  tor_assert(line);
  tor_assert(file);

  if (circ->marked_for_close) {
    log_warn(LD_BUG,
        "Duplicate call to circuit_mark_for_close at %s:%d"
        " (first at %s:%d)", file, line,
        circ->marked_for_close_file, circ->marked_for_close);
    return;
  }
  if (reason == END_CIRC_AT_ORIGIN) {
    if (!CIRCUIT_IS_ORIGIN(circ)) {
      log_warn(LD_BUG, "Specified 'at-origin' non-reason for ending circuit, "
               "but circuit was not at origin. (called %s:%d, purpose=%d)",
               file, line, circ->purpose);
    }
    reason = END_CIRC_REASON_NONE;
  }

  if (CIRCUIT_IS_ORIGIN(circ)) {
    if (pathbias_check_close(TO_ORIGIN_CIRCUIT(circ), reason) == -1) {
      /* Don't close it yet, we need to test it first */
      return;
    }

    /* We don't send reasons when closing circuits at the origin. */
    reason = END_CIRC_REASON_NONE;
  }

  if (reason & END_CIRC_REASON_FLAG_REMOTE)
    reason &= ~END_CIRC_REASON_FLAG_REMOTE;

  if (reason < END_CIRC_REASON_MIN_ || reason > END_CIRC_REASON_MAX_) {
    if (!(orig_reason & END_CIRC_REASON_FLAG_REMOTE))
      log_warn(LD_BUG, "Reason %d out of range at %s:%d", reason, file, line);
    reason = END_CIRC_REASON_NONE;
  }

  if (circ->state == CIRCUIT_STATE_ONIONSKIN_PENDING) {
    onion_pending_remove(TO_OR_CIRCUIT(circ));
  }
  /* If the circuit ever became OPEN, we sent it to the reputation history
   * module then.  If it isn't OPEN, we send it there now to remember which
   * links worked and which didn't.
   */
  if (circ->state != CIRCUIT_STATE_OPEN) {
    if (CIRCUIT_IS_ORIGIN(circ)) {
      origin_circuit_t *ocirc = TO_ORIGIN_CIRCUIT(circ);
      circuit_build_failed(ocirc); /* take actions if necessary */
      circuit_rep_hist_note_result(ocirc);
    }
  }
  if (circ->state == CIRCUIT_STATE_CHAN_WAIT) {
    if (circuits_pending_chans)
      smartlist_remove(circuits_pending_chans, circ);
  }
  if (CIRCUIT_IS_ORIGIN(circ)) {
    control_event_circuit_status(TO_ORIGIN_CIRCUIT(circ),
     (circ->state == CIRCUIT_STATE_OPEN)?CIRC_EVENT_CLOSED:CIRC_EVENT_FAILED,
     orig_reason);
  }
  if (circ->purpose == CIRCUIT_PURPOSE_C_INTRODUCE_ACK_WAIT) {
    origin_circuit_t *ocirc = TO_ORIGIN_CIRCUIT(circ);
    int timed_out = (reason == END_CIRC_REASON_TIMEOUT);
    tor_assert(circ->state == CIRCUIT_STATE_OPEN);
    tor_assert(ocirc->build_state->chosen_exit);
    tor_assert(ocirc->rend_data);
    /* treat this like getting a nack from it */
    log_info(LD_REND, "Failed intro circ %s to %s (awaiting ack). %s",
           safe_str_client(ocirc->rend_data->onion_address),
           safe_str_client(build_state_get_exit_nickname(ocirc->build_state)),
           timed_out ? "Recording timeout." : "Removing from descriptor.");
    rend_client_report_intro_point_failure(ocirc->build_state->chosen_exit,
                                           ocirc->rend_data,
                                           timed_out ?
                                           INTRO_POINT_FAILURE_TIMEOUT :
                                           INTRO_POINT_FAILURE_GENERIC);
  } else if (circ->purpose == CIRCUIT_PURPOSE_C_INTRODUCING &&
             reason != END_CIRC_REASON_TIMEOUT) {
    origin_circuit_t *ocirc = TO_ORIGIN_CIRCUIT(circ);
    if (ocirc->build_state->chosen_exit && ocirc->rend_data) {
      log_info(LD_REND, "Failed intro circ %s to %s "
               "(building circuit to intro point). "
               "Marking intro point as possibly unreachable.",
               safe_str_client(ocirc->rend_data->onion_address),
           safe_str_client(build_state_get_exit_nickname(ocirc->build_state)));
      rend_client_report_intro_point_failure(ocirc->build_state->chosen_exit,
                                             ocirc->rend_data,
                                             INTRO_POINT_FAILURE_UNREACHABLE);
    }
  }
  if (circ->n_chan) {
    circuit_clear_cell_queue(circ, circ->n_chan);
    /* Only send destroy if the channel isn't closing anyway */
    if (!(circ->n_chan->state == CHANNEL_STATE_CLOSING ||
          circ->n_chan->state == CHANNEL_STATE_CLOSED ||
          circ->n_chan->state == CHANNEL_STATE_ERROR)) {
      channel_send_destroy(circ->n_circ_id, circ->n_chan, reason);
    }
    circuitmux_detach_circuit(circ->n_chan->cmux, circ);
    circuit_set_n_circid_chan(circ, 0, NULL);
  }

  if (! CIRCUIT_IS_ORIGIN(circ)) {
    or_circuit_t *or_circ = TO_OR_CIRCUIT(circ);
    edge_connection_t *conn;
    for (conn=or_circ->n_streams; conn; conn=conn->next_stream)
      connection_edge_destroy(or_circ->p_circ_id, conn);
    or_circ->n_streams = NULL;

    while (or_circ->resolving_streams) {
      conn = or_circ->resolving_streams;
      or_circ->resolving_streams = conn->next_stream;
      if (!conn->base_.marked_for_close) {
        /* The client will see a DESTROY, and infer that the connections
         * are closing because the circuit is getting torn down.  No need
         * to send an end cell. */
        conn->edge_has_sent_end = 1;
        conn->end_reason = END_STREAM_REASON_DESTROY;
        conn->end_reason |= END_STREAM_REASON_FLAG_ALREADY_SENT_CLOSED;
        connection_mark_for_close(TO_CONN(conn));
      }
      conn->on_circuit = NULL;
    }

    if (or_circ->p_chan) {
      circuit_clear_cell_queue(circ, or_circ->p_chan);
      /* Only send destroy if the channel isn't closing anyway */
      if (!(or_circ->p_chan->state == CHANNEL_STATE_CLOSING ||
            or_circ->p_chan->state == CHANNEL_STATE_CLOSED ||
            or_circ->p_chan->state == CHANNEL_STATE_ERROR)) {
        channel_send_destroy(or_circ->p_circ_id, or_circ->p_chan, reason);
      }
      circuitmux_detach_circuit(or_circ->p_chan->cmux, circ);
      circuit_set_p_circid_chan(or_circ, 0, NULL);
    }
  } else {
    origin_circuit_t *ocirc = TO_ORIGIN_CIRCUIT(circ);
    edge_connection_t *conn;
    for (conn=ocirc->p_streams; conn; conn=conn->next_stream)
      connection_edge_destroy(circ->n_circ_id, conn);
    ocirc->p_streams = NULL;
  }

  circ->marked_for_close = line;
  circ->marked_for_close_file = file;

  if (!CIRCUIT_IS_ORIGIN(circ)) {
    or_circuit_t *or_circ = TO_OR_CIRCUIT(circ);
    if (or_circ->rend_splice) {
      if (!or_circ->rend_splice->base_.marked_for_close) {
        /* do this after marking this circuit, to avoid infinite recursion. */
        circuit_mark_for_close(TO_CIRCUIT(or_circ->rend_splice), reason);
      }
      or_circ->rend_splice = NULL;
    }
  }
}

/** Given a marked circuit <b>circ</b>, aggressively free its cell queues to
 * recover memory. */
static void
marked_circuit_free_cells(circuit_t *circ)
{
  if (!circ->marked_for_close) {
    log_warn(LD_BUG, "Called on non-marked circuit");
    return;
  }
  cell_queue_clear(&circ->n_chan_cells);
  if (! CIRCUIT_IS_ORIGIN(circ))
    cell_queue_clear(& TO_OR_CIRCUIT(circ)->p_chan_cells);
}

/** Aggressively free buffer contents on all the buffers of all streams in the
 * list starting at <b>stream</b>. Return the number of bytes recovered. */
static size_t
marked_circuit_streams_free_bytes(edge_connection_t *stream)
{
  size_t result = 0;
  for ( ; stream; stream = stream->next_stream) {
    connection_t *conn = TO_CONN(stream);
    if (conn->inbuf) {
      result += buf_allocation(conn->inbuf);
      buf_clear(conn->inbuf);
    }
    if (conn->outbuf) {
      result += buf_allocation(conn->outbuf);
      buf_clear(conn->outbuf);
    }
  }
  return result;
}

/** Aggressively free buffer contents on all the buffers of all streams on
 * circuit <b>c</b>. Return the number of bytes recovered. */
static size_t
marked_circuit_free_stream_bytes(circuit_t *c)
{
  if (CIRCUIT_IS_ORIGIN(c)) {
    return marked_circuit_streams_free_bytes(TO_ORIGIN_CIRCUIT(c)->p_streams);
  } else {
    return marked_circuit_streams_free_bytes(TO_OR_CIRCUIT(c)->n_streams);
  }
}

/** Return the number of cells used by the circuit <b>c</b>'s cell queues. */
STATIC size_t
n_cells_in_circ_queues(const circuit_t *c)
{
  size_t n = c->n_chan_cells.n;
  if (! CIRCUIT_IS_ORIGIN(c)) {
    circuit_t *cc = (circuit_t *) c;
    n += TO_OR_CIRCUIT(cc)->p_chan_cells.n;
  }
  return n;
}

/**
 * Return the age of the oldest cell queued on <b>c</b>, in milliseconds.
 * Return 0 if there are no cells queued on c.  Requires that <b>now</b> be
 * the current time in milliseconds since the epoch, truncated.
 *
 * This function will return incorrect results if the oldest cell queued on
 * the circuit is older than 2**32 msec (about 49 days) old.
 */
STATIC uint32_t
circuit_max_queued_cell_age(const circuit_t *c, uint32_t now)
{
  uint32_t age = 0;
  packed_cell_t *cell;

  if (NULL != (cell = TOR_SIMPLEQ_FIRST(&c->n_chan_cells.head)))
    age = now - cell->inserted_time;

  if (! CIRCUIT_IS_ORIGIN(c)) {
    const or_circuit_t *orcirc = CONST_TO_OR_CIRCUIT(c);
    if (NULL != (cell = TOR_SIMPLEQ_FIRST(&orcirc->p_chan_cells.head))) {
      uint32_t age2 = now - cell->inserted_time;
      if (age2 > age)
        return age2;
    }
  }
  return age;
}

/** Return the age in milliseconds of the oldest buffer chunk on any stream in
 * the linked list <b>stream</b>, where age is taken in milliseconds before
 * the time <b>now</b> (in truncated milliseconds since the epoch). */
static uint32_t
circuit_get_streams_max_data_age(const edge_connection_t *stream, uint32_t now)
{
  uint32_t age = 0, age2;
  for (; stream; stream = stream->next_stream) {
    const connection_t *conn = TO_CONN(stream);
    if (conn->outbuf) {
      age2 = buf_get_oldest_chunk_timestamp(conn->outbuf, now);
      if (age2 > age)
        age = age2;
    }
    if (conn->inbuf) {
      age2 = buf_get_oldest_chunk_timestamp(conn->inbuf, now);
      if (age2 > age)
        age = age2;
    }
  }

  return age;
}

/** Return the age in milliseconds of the oldest buffer chunk on any stream
 * attached to the circuit <b>c</b>, where age is taken in milliseconds before
 * the time <b>now</b> (in truncated milliseconds since the epoch). */
STATIC uint32_t
circuit_max_queued_data_age(const circuit_t *c, uint32_t now)
{
  if (CIRCUIT_IS_ORIGIN(c)) {
    return circuit_get_streams_max_data_age(
        CONST_TO_ORIGIN_CIRCUIT(c)->p_streams, now);
  } else {
    return circuit_get_streams_max_data_age(
        CONST_TO_OR_CIRCUIT(c)->n_streams, now);
  }
}

/** Return the age of the oldest cell or stream buffer chunk on the circuit
 * <b>c</b>, where age is taken in milliseconds before the time <b>now</b> (in
 * truncated milliseconds since the epoch). */
STATIC uint32_t
circuit_max_queued_item_age(const circuit_t *c, uint32_t now)
{
  uint32_t cell_age = circuit_max_queued_cell_age(c, now);
  uint32_t data_age = circuit_max_queued_data_age(c, now);
  if (cell_age > data_age)
    return cell_age;
  else
    return data_age;
}

/** Helper to sort a list of circuit_t by age of oldest item, in descending
 * order. */
static int
circuits_compare_by_oldest_queued_item_(const void **a_, const void **b_)
{
  const circuit_t *a = *a_;
  const circuit_t *b = *b_;
  uint32_t age_a = a->age_tmp;
  uint32_t age_b = b->age_tmp;

  if (age_a < age_b)
    return 1;
  else if (age_a == age_b)
    return 0;
  else
    return -1;
}

#define FRACTION_OF_DATA_TO_RETAIN_ON_OOM 0.90

/** We're out of memory for cells, having allocated <b>current_allocation</b>
 * bytes' worth.  Kill the 'worst' circuits until we're under
 * FRACTION_OF_DATA_TO_RETAIN_ON_OOM of our maximum usage. */
void
circuits_handle_oom(size_t current_allocation)
{
  /* Let's hope there's enough slack space for this allocation here... */
  smartlist_t *circlist = smartlist_new();
  circuit_t *circ;
  size_t mem_to_recover;
  size_t mem_recovered=0;
  int n_circuits_killed=0;
  struct timeval now;
  uint32_t now_ms;
  log_notice(LD_GENERAL, "We're low on memory.  Killing circuits with "
             "over-long queues. (This behavior is controlled by "
             "MaxMemInQueues.)");

  {
    const size_t recovered = buf_shrink_freelists(1);
    if (recovered >= current_allocation) {
      log_warn(LD_BUG, "We somehow recovered more memory from freelists "
               "than we thought we had allocated");
      current_allocation = 0;
    } else {
      current_allocation -= recovered;
    }
  }

  {
    size_t mem_target = (size_t)(get_options()->MaxMemInQueues *
                                 FRACTION_OF_DATA_TO_RETAIN_ON_OOM);
    if (current_allocation <= mem_target)
      return;
    mem_to_recover = current_allocation - mem_target;
  }

  tor_gettimeofday_cached_monotonic(&now);
  now_ms = (uint32_t)tv_to_msec(&now);

  /* This algorithm itself assumes that you've got enough memory slack
   * to actually run it. */
  TOR_LIST_FOREACH(circ, &global_circuitlist, head) {
    circ->age_tmp = circuit_max_queued_item_age(circ, now_ms);
    smartlist_add(circlist, circ);
  }

  /* This is O(n log n); there are faster algorithms we could use instead.
   * Let's hope this doesn't happen enough to be in the critical path. */
  smartlist_sort(circlist, circuits_compare_by_oldest_queued_item_);

  /* Okay, now the worst circuits are at the front of the list. Let's mark
   * them, and reclaim their storage aggressively. */
  SMARTLIST_FOREACH_BEGIN(circlist, circuit_t *, circ) {
    size_t n = n_cells_in_circ_queues(circ);
    size_t freed;
    if (! circ->marked_for_close) {
      circuit_mark_for_close(circ, END_CIRC_REASON_RESOURCELIMIT);
    }
    marked_circuit_free_cells(circ);
    freed = marked_circuit_free_stream_bytes(circ);

    ++n_circuits_killed;

    mem_recovered += n * packed_cell_mem_cost();
    mem_recovered += freed;

    if (mem_recovered >= mem_to_recover)
      break;
  } SMARTLIST_FOREACH_END(circ);

#ifdef ENABLE_MEMPOOLS
  clean_cell_pool(); /* In case this helps. */
#endif /* ENABLE_MEMPOOLS */
  buf_shrink_freelists(1); /* This is necessary to actually release buffer
                              chunks. */

  log_notice(LD_GENERAL, "Removed "U64_FORMAT" bytes by killing %d circuits; "
             "%d circuits remain alive.",
             U64_PRINTF_ARG(mem_recovered),
             n_circuits_killed,
             smartlist_len(circlist) - n_circuits_killed);

  smartlist_free(circlist);
}

/** Verify that cpath layer <b>cp</b> has all of its invariants
 * correct. Trigger an assert if anything is invalid.
 */
void
assert_cpath_layer_ok(const crypt_path_t *cp)
{
//  tor_assert(cp->addr); /* these are zero for rendezvous extra-hops */
//  tor_assert(cp->port);
  tor_assert(cp);
  tor_assert(cp->magic == CRYPT_PATH_MAGIC);
  switch (cp->state)
    {
    case CPATH_STATE_OPEN:
      tor_assert(cp->f_crypto);
      tor_assert(cp->b_crypto);
      /* fall through */
    case CPATH_STATE_CLOSED:
      /*XXXX Assert that there's no handshake_state either. */
      tor_assert(!cp->rend_dh_handshake_state);
      break;
    case CPATH_STATE_AWAITING_KEYS:
      /* tor_assert(cp->dh_handshake_state); */
      break;
    default:
      log_fn(LOG_ERR, LD_BUG, "Unexpected state %d", cp->state);
      tor_assert(0);
    }
  tor_assert(cp->package_window >= 0);
  tor_assert(cp->deliver_window >= 0);
}

/** Verify that cpath <b>cp</b> has all of its invariants
 * correct. Trigger an assert if anything is invalid.
 */
static void
assert_cpath_ok(const crypt_path_t *cp)
{
  const crypt_path_t *start = cp;

  do {
    assert_cpath_layer_ok(cp);
    /* layers must be in sequence of: "open* awaiting? closed*" */
    if (cp != start) {
      if (cp->state == CPATH_STATE_AWAITING_KEYS) {
        tor_assert(cp->prev->state == CPATH_STATE_OPEN);
      } else if (cp->state == CPATH_STATE_OPEN) {
        tor_assert(cp->prev->state == CPATH_STATE_OPEN);
      }
    }
    cp = cp->next;
    tor_assert(cp);
  } while (cp != start);
}

/** Verify that circuit <b>c</b> has all of its invariants
 * correct. Trigger an assert if anything is invalid.
 */
void
assert_circuit_ok(const circuit_t *c)
{
  edge_connection_t *conn;
  const or_circuit_t *or_circ = NULL;
  const origin_circuit_t *origin_circ = NULL;

  tor_assert(c);
  tor_assert(c->magic == ORIGIN_CIRCUIT_MAGIC || c->magic == OR_CIRCUIT_MAGIC);
  tor_assert(c->purpose >= CIRCUIT_PURPOSE_MIN_ &&
             c->purpose <= CIRCUIT_PURPOSE_MAX_);

  if (CIRCUIT_IS_ORIGIN(c))
    origin_circ = CONST_TO_ORIGIN_CIRCUIT(c);
  else
    or_circ = CONST_TO_OR_CIRCUIT(c);

  if (c->n_chan) {
    tor_assert(!c->n_hop);

    if (c->n_circ_id) {
      /* We use the _impl variant here to make sure we don't fail on marked
       * circuits, which would not be returned by the regular function. */
      circuit_t *c2 = circuit_get_by_circid_channel_impl(c->n_circ_id,
                                                         c->n_chan, NULL);
      tor_assert(c == c2);
    }
  }
  if (or_circ && or_circ->p_chan) {
    if (or_circ->p_circ_id) {
      /* ibid */
      circuit_t *c2 =
        circuit_get_by_circid_channel_impl(or_circ->p_circ_id,
                                           or_circ->p_chan, NULL);
      tor_assert(c == c2);
    }
  }
  if (or_circ)
    for (conn = or_circ->n_streams; conn; conn = conn->next_stream)
      tor_assert(conn->base_.type == CONN_TYPE_EXIT);

  tor_assert(c->deliver_window >= 0);
  tor_assert(c->package_window >= 0);
  if (c->state == CIRCUIT_STATE_OPEN) {
    tor_assert(!c->n_chan_create_cell);
    if (or_circ) {
      tor_assert(or_circ->n_crypto);
      tor_assert(or_circ->p_crypto);
      tor_assert(or_circ->n_digest);
      tor_assert(or_circ->p_digest);
    }
  }
  if (c->state == CIRCUIT_STATE_CHAN_WAIT && !c->marked_for_close) {
    tor_assert(circuits_pending_chans &&
               smartlist_contains(circuits_pending_chans, c));
  } else {
    tor_assert(!circuits_pending_chans ||
               !smartlist_contains(circuits_pending_chans, c));
  }
  if (origin_circ && origin_circ->cpath) {
    assert_cpath_ok(origin_circ->cpath);
  }
  if (c->purpose == CIRCUIT_PURPOSE_REND_ESTABLISHED) {
    tor_assert(or_circ);
    if (!c->marked_for_close) {
      tor_assert(or_circ->rend_splice);
      tor_assert(or_circ->rend_splice->rend_splice == or_circ);
    }
    tor_assert(or_circ->rend_splice != or_circ);
  } else {
    tor_assert(!or_circ || !or_circ->rend_splice);
  }
}

