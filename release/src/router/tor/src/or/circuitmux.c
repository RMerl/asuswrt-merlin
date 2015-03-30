/* * Copyright (c) 2012-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file circuitmux.c
 * \brief Circuit mux/cell selection abstraction
 **/

#include "or.h"
#include "channel.h"
#include "circuitlist.h"
#include "circuitmux.h"
#include "relay.h"

/*
 * Private typedefs for circuitmux.c
 */

/*
 * Map of muxinfos for circuitmux_t to use; struct is defined below (name
 * of struct must match HT_HEAD line).
 */
typedef struct chanid_circid_muxinfo_map chanid_circid_muxinfo_map_t;

/*
 * Hash table entry (yeah, calling it chanid_circid_muxinfo_s seems to
 * break the hash table code).
 */
typedef struct chanid_circid_muxinfo_t chanid_circid_muxinfo_t;

/*
 * Anything the mux wants to store per-circuit in the map; right now just
 * a count of queued cells.
 */

typedef struct circuit_muxinfo_s circuit_muxinfo_t;

/*
 * Structures for circuitmux.c
 */

/*
 * A circuitmux is a collection of circuits; it tracks which subset
 * of the attached circuits are 'active' (i.e., have cells available
 * to transmit) and how many cells on each.  It expoes three distinct
 * interfaces to other components:
 *
 * To channels, which each have a circuitmux_t, the supported operations
 * are:
 *
 *   circuitmux_get_first_active_circuit():
 *
 *     Pick one of the circuitmux's active circuits to send cells from.
 *
 *   circuitmux_notify_xmit_cells():
 *
 *     Notify the circuitmux that cells have been sent on a circuit.
 *
 * To circuits, the exposed operations are:
 *
 *   circuitmux_attach_circuit():
 *
 *     Attach a circuit to the circuitmux; this will allocate any policy-
 *     specific data wanted for this circuit and add it to the active
 *     circuits list if it has queued cells.
 *
 *   circuitmux_detach_circuit():
 *
 *     Detach a circuit from the circuitmux, freeing associated structures.
 *
 *   circuitmux_clear_num_cells():
 *
 *     Clear the circuitmux's cell counter for this circuit.
 *
 *   circuitmux_set_num_cells():
 *
 *     Set the circuitmux's cell counter for this circuit.
 *
 * See circuitmux.h for the circuitmux_policy_t data structure, which contains
 * a table of function pointers implementing a circuit selection policy, and
 * circuitmux_ewma.c for an example of a circuitmux policy.  Circuitmux
 * policies can be manipulated with:
 *
 *   circuitmux_get_policy():
 *
 *     Return the current policy for a circuitmux_t, if any.
 *
 *   circuitmux_clear_policy():
 *
 *     Remove a policy installed on a circuitmux_t, freeing all associated
 *     data.  The circuitmux will revert to the built-in round-robin behavior.
 *
 *   circuitmux_set_policy():
 *
 *     Install a policy on a circuitmux_t; the appropriate callbacks will be
 *     made to attach all existing circuits to the new policy.
 *
 */

struct circuitmux_s {
  /* Keep count of attached, active circuits */
  unsigned int n_circuits, n_active_circuits;

  /* Total number of queued cells on all circuits */
  unsigned int n_cells;

  /*
   * Map from (channel ID, circuit ID) pairs to circuit_muxinfo_t
   */
  chanid_circid_muxinfo_map_t *chanid_circid_map;

  /*
   * Double-linked ring of circuits with queued cells waiting for room to
   * free up on this connection's outbuf.  Every time we pull cells from
   * a circuit, we advance this pointer to the next circuit in the ring.
   */
  struct circuit_t *active_circuits_head, *active_circuits_tail;

  /** List of queued destroy cells */
  cell_queue_t destroy_cell_queue;
  /** Boolean: True iff the last cell to circuitmux_get_first_active_circuit
   * returned the destroy queue. Used to force alternation between
   * destroy/non-destroy cells.
   *
   * XXXX There is no reason to think that alternating is a particularly good
   * approach -- it's just designed to prevent destroys from starving other
   * cells completely.
   */
  unsigned int last_cell_was_destroy : 1;
  /** Destroy counter: increment this when a destroy gets queued, decrement
   * when we unqueue it, so we can test to make sure they don't starve.
   */
  int64_t destroy_ctr;

  /*
   * Circuitmux policy; if this is non-NULL, it can override the built-
   * in round-robin active circuits behavior.  This is how EWMA works in
   * the new circuitmux_t world.
   */
  const circuitmux_policy_t *policy;

  /* Policy-specific data */
  circuitmux_policy_data_t *policy_data;
};

/*
 * This struct holds whatever we want to store per attached circuit on a
 * circuitmux_t; right now, just the count of queued cells and the direction.
 */

struct circuit_muxinfo_s {
  /* Count of cells on this circuit at last update */
  unsigned int cell_count;
  /* Direction of flow */
  cell_direction_t direction;
  /* Policy-specific data */
  circuitmux_policy_circ_data_t *policy_data;
  /* Mark bit for consistency checker */
  unsigned int mark:1;
};

/*
 * A map from channel ID and circuit ID to a circuit_muxinfo_t for that
 * circuit.
 */

struct chanid_circid_muxinfo_t {
  HT_ENTRY(chanid_circid_muxinfo_t) node;
  uint64_t chan_id;
  circid_t circ_id;
  circuit_muxinfo_t muxinfo;
};

/*
 * Internal-use #defines
 */

#ifdef CMUX_PARANOIA
#define circuitmux_assert_okay_paranoid(cmux) \
  circuitmux_assert_okay(cmux)
#else
#define circuitmux_assert_okay_paranoid(cmux)
#endif

/*
 * Static function declarations
 */

static INLINE int
chanid_circid_entries_eq(chanid_circid_muxinfo_t *a,
                         chanid_circid_muxinfo_t *b);
static INLINE unsigned int
chanid_circid_entry_hash(chanid_circid_muxinfo_t *a);
static chanid_circid_muxinfo_t *
circuitmux_find_map_entry(circuitmux_t *cmux, circuit_t *circ);
static void
circuitmux_make_circuit_active(circuitmux_t *cmux, circuit_t *circ,
                               cell_direction_t direction);
static void
circuitmux_make_circuit_inactive(circuitmux_t *cmux, circuit_t *circ,
                                 cell_direction_t direction);
static INLINE void
circuitmux_move_active_circ_to_tail(circuitmux_t *cmux, circuit_t *circ,
                                    cell_direction_t direction);
static INLINE circuit_t **
circuitmux_next_active_circ_p(circuitmux_t *cmux, circuit_t *circ);
static INLINE circuit_t **
circuitmux_prev_active_circ_p(circuitmux_t *cmux, circuit_t *circ);
static void circuitmux_assert_okay_pass_one(circuitmux_t *cmux);
static void circuitmux_assert_okay_pass_two(circuitmux_t *cmux);
static void circuitmux_assert_okay_pass_three(circuitmux_t *cmux);

/* Static global variables */

/** Count the destroy balance to debug destroy queue logic */
static int64_t global_destroy_ctr = 0;

/* Function definitions */

/**
 * Linked list helpers
 */

/**
 * Move an active circuit to the tail of the cmux's active circuits list;
 * used by circuitmux_notify_xmit_cells().
 */

static INLINE void
circuitmux_move_active_circ_to_tail(circuitmux_t *cmux, circuit_t *circ,
                                    cell_direction_t direction)
{
  circuit_t **next_p = NULL, **prev_p = NULL;
  circuit_t **next_prev = NULL, **prev_next = NULL;
  circuit_t **tail_next = NULL;
  or_circuit_t *or_circ = NULL;

  tor_assert(cmux);
  tor_assert(circ);

  circuitmux_assert_okay_paranoid(cmux);

  /* Figure out our next_p and prev_p for this cmux/direction */
  if (direction) {
    if (direction == CELL_DIRECTION_OUT) {
      tor_assert(circ->n_mux == cmux);
      next_p = &(circ->next_active_on_n_chan);
      prev_p = &(circ->prev_active_on_n_chan);
    } else {
      or_circ = TO_OR_CIRCUIT(circ);
      tor_assert(or_circ->p_mux == cmux);
      next_p = &(or_circ->next_active_on_p_chan);
      prev_p = &(or_circ->prev_active_on_p_chan);
    }
  } else {
    if (circ->n_mux == cmux) {
      next_p = &(circ->next_active_on_n_chan);
      prev_p = &(circ->prev_active_on_n_chan);
      direction = CELL_DIRECTION_OUT;
    } else {
      or_circ = TO_OR_CIRCUIT(circ);
      tor_assert(or_circ->p_mux == cmux);
      next_p = &(or_circ->next_active_on_p_chan);
      prev_p = &(or_circ->prev_active_on_p_chan);
      direction = CELL_DIRECTION_IN;
    }
  }
  tor_assert(next_p);
  tor_assert(prev_p);

  /* Check if this really is an active circuit */
  if ((*next_p == NULL && *prev_p == NULL) &&
      !(circ == cmux->active_circuits_head ||
        circ == cmux->active_circuits_tail)) {
    /* Not active, no-op */
    return;
  }

  /* Check if this is already the tail */
  if (circ == cmux->active_circuits_tail) return;

  /* Okay, we have to move it; figure out next_prev and prev_next */
  if (*next_p) next_prev = circuitmux_prev_active_circ_p(cmux, *next_p);
  if (*prev_p) prev_next = circuitmux_next_active_circ_p(cmux, *prev_p);
  /* Adjust the previous node's next pointer, if any */
  if (prev_next) *prev_next = *next_p;
  /* Otherwise, we were the head */
  else cmux->active_circuits_head = *next_p;
  /* Adjust the next node's previous pointer, if any */
  if (next_prev) *next_prev = *prev_p;
  /* We're out of the list; now re-attach at the tail */
  /* Adjust our next and prev pointers */
  *next_p = NULL;
  *prev_p = cmux->active_circuits_tail;
  /* Set the next pointer of the tail, or the head if none */
  if (cmux->active_circuits_tail) {
    tail_next = circuitmux_next_active_circ_p(cmux,
                                              cmux->active_circuits_tail);
    *tail_next = circ;
  } else {
    cmux->active_circuits_head = circ;
  }
  /* Set the tail to this circuit */
  cmux->active_circuits_tail = circ;

  circuitmux_assert_okay_paranoid(cmux);
}

static INLINE circuit_t **
circuitmux_next_active_circ_p(circuitmux_t *cmux, circuit_t *circ)
{
  tor_assert(cmux);
  tor_assert(circ);

  if (circ->n_mux == cmux) return &(circ->next_active_on_n_chan);
  else {
    tor_assert(TO_OR_CIRCUIT(circ)->p_mux == cmux);
    return &(TO_OR_CIRCUIT(circ)->next_active_on_p_chan);
  }
}

static INLINE circuit_t **
circuitmux_prev_active_circ_p(circuitmux_t *cmux, circuit_t *circ)
{
  tor_assert(cmux);
  tor_assert(circ);

  if (circ->n_mux == cmux) return &(circ->prev_active_on_n_chan);
  else {
    tor_assert(TO_OR_CIRCUIT(circ)->p_mux == cmux);
    return &(TO_OR_CIRCUIT(circ)->prev_active_on_p_chan);
  }
}

/**
 * Helper for chanid_circid_cell_count_map_t hash table: compare the channel
 * ID and circuit ID for a and b, and return less than, equal to, or greater
 * than zero appropriately.
 */

static INLINE int
chanid_circid_entries_eq(chanid_circid_muxinfo_t *a,
                         chanid_circid_muxinfo_t *b)
{
    return a->chan_id == b->chan_id && a->circ_id == b->circ_id;
}

/**
 * Helper: return a hash based on circuit ID and channel ID in a.
 */

static INLINE unsigned int
chanid_circid_entry_hash(chanid_circid_muxinfo_t *a)
{
    return (((unsigned int)(a->circ_id) << 8) ^
            ((unsigned int)((a->chan_id >> 32) & 0xffffffff)) ^
            ((unsigned int)(a->chan_id & 0xffffffff)));
}

/* Declare the struct chanid_circid_muxinfo_map type */
HT_HEAD(chanid_circid_muxinfo_map, chanid_circid_muxinfo_t);

/* Emit a bunch of hash table stuff */
HT_PROTOTYPE(chanid_circid_muxinfo_map, chanid_circid_muxinfo_t, node,
             chanid_circid_entry_hash, chanid_circid_entries_eq);
HT_GENERATE(chanid_circid_muxinfo_map, chanid_circid_muxinfo_t, node,
            chanid_circid_entry_hash, chanid_circid_entries_eq, 0.6,
            malloc, realloc, free);

/*
 * Circuitmux alloc/free functions
 */

/**
 * Allocate a new circuitmux_t
 */

circuitmux_t *
circuitmux_alloc(void)
{
  circuitmux_t *rv = NULL;

  rv = tor_malloc_zero(sizeof(*rv));
  rv->chanid_circid_map = tor_malloc_zero(sizeof(*( rv->chanid_circid_map)));
  HT_INIT(chanid_circid_muxinfo_map, rv->chanid_circid_map);
  cell_queue_init(&rv->destroy_cell_queue);

  return rv;
}

/**
 * Detach all circuits from a circuitmux (use before circuitmux_free())
 *
 * If <b>detached_out</b> is non-NULL, add every detached circuit_t to
 * detached_out.
 */

void
circuitmux_detach_all_circuits(circuitmux_t *cmux, smartlist_t *detached_out)
{
  chanid_circid_muxinfo_t **i = NULL, *to_remove;
  channel_t *chan = NULL;
  circuit_t *circ = NULL;

  tor_assert(cmux);
  /*
   * Don't circuitmux_assert_okay_paranoid() here; this gets called when
   * channels are being freed and have already been unregistered, so
   * the channel ID lookups it does will fail.
   */

  i = HT_START(chanid_circid_muxinfo_map, cmux->chanid_circid_map);
  while (i) {
    to_remove = *i;

    if (! to_remove) {
      log_warn(LD_BUG, "Somehow, an HT iterator gave us a NULL pointer.");
      break;
    } else {
      /* Find a channel and circuit */
      chan = channel_find_by_global_id(to_remove->chan_id);
      if (chan) {
        circ =
          circuit_get_by_circid_channel_even_if_marked(to_remove->circ_id,
                                                       chan);
        if (circ) {
          /* Clear the circuit's mux for this direction */
          if (to_remove->muxinfo.direction == CELL_DIRECTION_OUT) {
            /*
             * Update active_circuits et al.; this does policy notifies, so
             * comes before freeing policy data
             */

            if (to_remove->muxinfo.cell_count > 0) {
              circuitmux_make_circuit_inactive(cmux, circ, CELL_DIRECTION_OUT);
            }

            /* Clear n_mux */
            circ->n_mux = NULL;

            if (detached_out)
              smartlist_add(detached_out, circ);
          } else if (circ->magic == OR_CIRCUIT_MAGIC) {
            /*
             * Update active_circuits et al.; this does policy notifies, so
             * comes before freeing policy data
             */

            if (to_remove->muxinfo.cell_count > 0) {
              circuitmux_make_circuit_inactive(cmux, circ, CELL_DIRECTION_IN);
            }

            /*
             * It has a sensible p_chan and direction == CELL_DIRECTION_IN,
             * so clear p_mux.
             */
            TO_OR_CIRCUIT(circ)->p_mux = NULL;

            if (detached_out)
              smartlist_add(detached_out, circ);
          } else {
            /* Complain and move on */
            log_warn(LD_CIRC,
                     "Circuit %u/channel " U64_FORMAT " had direction == "
                     "CELL_DIRECTION_IN, but isn't an or_circuit_t",
                     (unsigned)to_remove->circ_id,
                     U64_PRINTF_ARG(to_remove->chan_id));
          }

          /* Free policy-specific data if we have it */
          if (to_remove->muxinfo.policy_data) {
            /*
             * If we have policy data, assert that we have the means to
             * free it
             */
            tor_assert(cmux->policy);
            tor_assert(cmux->policy->free_circ_data);
            /* Call free_circ_data() */
            cmux->policy->free_circ_data(cmux,
                                         cmux->policy_data,
                                         circ,
                                         to_remove->muxinfo.policy_data);
            to_remove->muxinfo.policy_data = NULL;
          }
        } else {
          /* Complain and move on */
          log_warn(LD_CIRC,
                   "Couldn't find circuit %u (for channel " U64_FORMAT ")",
                   (unsigned)to_remove->circ_id,
                   U64_PRINTF_ARG(to_remove->chan_id));
        }
      } else {
        /* Complain and move on */
        log_warn(LD_CIRC,
                 "Couldn't find channel " U64_FORMAT " (for circuit id %u)",
                 U64_PRINTF_ARG(to_remove->chan_id),
                 (unsigned)to_remove->circ_id);
      }

      /* Assert that we don't have un-freed policy data for this circuit */
      tor_assert(to_remove->muxinfo.policy_data == NULL);
    }

    i = HT_NEXT_RMV(chanid_circid_muxinfo_map, cmux->chanid_circid_map, i);

    /* Free it */
    tor_free(to_remove);
  }

  cmux->n_circuits = 0;
  cmux->n_active_circuits = 0;
  cmux->n_cells = 0;
}

/** Reclaim all circuit IDs currently marked as unusable on <b>chan</b> because
 * of pending destroy cells in <b>cmux</b>.
 *
 * This function must be called AFTER circuits are unlinked from the (channel,
 * circuid-id) map with circuit_unlink_all_from_channel(), but before calling
 * circuitmux_free().
 */
void
circuitmux_mark_destroyed_circids_usable(circuitmux_t *cmux, channel_t *chan)
{
  packed_cell_t *cell;
  int n_bad = 0;
  TOR_SIMPLEQ_FOREACH(cell, &cmux->destroy_cell_queue.head, next) {
    circid_t circid = 0;
    if (packed_cell_is_destroy(chan, cell, &circid)) {
      channel_mark_circid_usable(chan, circid);
    } else {
      ++n_bad;
    }
  }
  if (n_bad)
    log_warn(LD_BUG, "%d cell(s) on destroy queue did not look like a "
             "DESTROY cell.", n_bad);
}

/**
 * Free a circuitmux_t; the circuits must be detached first with
 * circuitmux_detach_all_circuits().
 */

void
circuitmux_free(circuitmux_t *cmux)
{
  if (!cmux) return;

  tor_assert(cmux->n_circuits == 0);
  tor_assert(cmux->n_active_circuits == 0);

  /*
   * Free policy-specific data if we have any; we don't
   * need to do circuitmux_set_policy(cmux, NULL) to cover
   * the circuits because they would have been handled in
   * circuitmux_detach_all_circuits() before this was
   * called.
   */
  if (cmux->policy && cmux->policy->free_cmux_data) {
    if (cmux->policy_data) {
      cmux->policy->free_cmux_data(cmux, cmux->policy_data);
      cmux->policy_data = NULL;
    }
  } else tor_assert(cmux->policy_data == NULL);

  if (cmux->chanid_circid_map) {
    HT_CLEAR(chanid_circid_muxinfo_map, cmux->chanid_circid_map);
    tor_free(cmux->chanid_circid_map);
  }

  /*
   * We're throwing away some destroys; log the counter and
   * adjust the global counter by the queue size.
   */
  if (cmux->destroy_cell_queue.n > 0) {
    cmux->destroy_ctr -= cmux->destroy_cell_queue.n;
    global_destroy_ctr -= cmux->destroy_cell_queue.n;
    log_debug(LD_CIRC,
              "Freeing cmux at %p with %u queued destroys; the last cmux "
              "destroy balance was "I64_FORMAT", global is "I64_FORMAT,
              cmux, cmux->destroy_cell_queue.n,
              I64_PRINTF_ARG(cmux->destroy_ctr),
              I64_PRINTF_ARG(global_destroy_ctr));
  } else {
    log_debug(LD_CIRC,
              "Freeing cmux at %p with no queued destroys, the cmux destroy "
              "balance was "I64_FORMAT", global is "I64_FORMAT,
              cmux,
              I64_PRINTF_ARG(cmux->destroy_ctr),
              I64_PRINTF_ARG(global_destroy_ctr));
  }

  cell_queue_clear(&cmux->destroy_cell_queue);

  tor_free(cmux);
}

/*
 * Circuitmux policy control functions
 */

/**
 * Remove any policy installed on cmux; all policy data will be freed and
 * cmux behavior will revert to the built-in round-robin active_circuits
 * mechanism.
 */

void
circuitmux_clear_policy(circuitmux_t *cmux)
{
  tor_assert(cmux);

  /* Internally, this is just setting policy to NULL */
  if (cmux->policy) {
    circuitmux_set_policy(cmux, NULL);
  }
}

/**
 * Return the policy currently installed on a circuitmux_t
 */

const circuitmux_policy_t *
circuitmux_get_policy(circuitmux_t *cmux)
{
  tor_assert(cmux);

  return cmux->policy;
}

/**
 * Set policy; allocate for new policy, detach all circuits from old policy
 * if any, attach them to new policy, and free old policy data.
 */

void
circuitmux_set_policy(circuitmux_t *cmux,
                      const circuitmux_policy_t *pol)
{
  const circuitmux_policy_t *old_pol = NULL, *new_pol = NULL;
  circuitmux_policy_data_t *old_pol_data = NULL, *new_pol_data = NULL;
  chanid_circid_muxinfo_t **i = NULL;
  channel_t *chan = NULL;
  uint64_t last_chan_id_searched = 0;
  circuit_t *circ = NULL;

  tor_assert(cmux);

  /* Set up variables */
  old_pol = cmux->policy;
  old_pol_data = cmux->policy_data;
  new_pol = pol;

  /* Check if this is the trivial case */
  if (old_pol == new_pol) return;

  /* Allocate data for new policy, if any */
  if (new_pol && new_pol->alloc_cmux_data) {
    /*
     * If alloc_cmux_data is not null, then we expect to get some policy
     * data.  Assert that we also have free_cmux_data so we can free it
     * when the time comes, and allocate it.
     */
    tor_assert(new_pol->free_cmux_data);
    new_pol_data = new_pol->alloc_cmux_data(cmux);
    tor_assert(new_pol_data);
  }

  /* Install new policy and new policy data on cmux */
  cmux->policy = new_pol;
  cmux->policy_data = new_pol_data;

  /* Iterate over all circuits, attaching/detaching each one */
  i = HT_START(chanid_circid_muxinfo_map, cmux->chanid_circid_map);
  while (i) {
    /* Assert that this entry isn't NULL */
    tor_assert(*i);

    /*
     * Get the channel; since normal case is all circuits on the mux share a
     * channel, we cache last_chan_id_searched
     */
    if (!chan || last_chan_id_searched != (*i)->chan_id) {
      chan = channel_find_by_global_id((*i)->chan_id);
      last_chan_id_searched = (*i)->chan_id;
    }
    tor_assert(chan);

    /* Get the circuit */
    circ = circuit_get_by_circid_channel_even_if_marked((*i)->circ_id, chan);
    tor_assert(circ);

    /* Need to tell old policy it becomes inactive (i.e., it is active) ? */
    if (old_pol && old_pol->notify_circ_inactive &&
        (*i)->muxinfo.cell_count > 0) {
      old_pol->notify_circ_inactive(cmux, old_pol_data, circ,
                                    (*i)->muxinfo.policy_data);
    }

    /* Need to free old policy data? */
    if ((*i)->muxinfo.policy_data) {
      /* Assert that we have the means to free it if we have policy data */
      tor_assert(old_pol);
      tor_assert(old_pol->free_circ_data);
      /* Free it */
      old_pol->free_circ_data(cmux, old_pol_data, circ,
                             (*i)->muxinfo.policy_data);
      (*i)->muxinfo.policy_data = NULL;
    }

    /* Need to allocate new policy data? */
    if (new_pol && new_pol->alloc_circ_data) {
      /*
       * If alloc_circ_data is not null, we expect to get some per-circuit
       * policy data.  Assert that we also have free_circ_data so we can
       * free it when the time comes, and allocate it.
       */
      tor_assert(new_pol->free_circ_data);
      (*i)->muxinfo.policy_data =
        new_pol->alloc_circ_data(cmux, new_pol_data, circ,
                                 (*i)->muxinfo.direction,
                                 (*i)->muxinfo.cell_count);
    }

    /* Need to make active on new policy? */
    if (new_pol && new_pol->notify_circ_active &&
        (*i)->muxinfo.cell_count > 0) {
      new_pol->notify_circ_active(cmux, new_pol_data, circ,
                                  (*i)->muxinfo.policy_data);
    }

    /* Advance to next circuit map entry */
    i = HT_NEXT(chanid_circid_muxinfo_map, cmux->chanid_circid_map, i);
  }

  /* Free data for old policy, if any */
  if (old_pol_data) {
    /*
     * If we had old policy data, we should have an old policy and a free
     * function for it.
     */
    tor_assert(old_pol);
    tor_assert(old_pol->free_cmux_data);
    old_pol->free_cmux_data(cmux, old_pol_data);
    old_pol_data = NULL;
  }
}

/*
 * Circuitmux/circuit attachment status inquiry functions
 */

/**
 * Query the direction of an attached circuit
 */

cell_direction_t
circuitmux_attached_circuit_direction(circuitmux_t *cmux, circuit_t *circ)
{
  chanid_circid_muxinfo_t *hashent = NULL;

  /* Try to find a map entry */
  hashent = circuitmux_find_map_entry(cmux, circ);

  /*
   * This function should only be called on attached circuits; assert that
   * we had a map entry.
   */
  tor_assert(hashent);

  /* Return the direction from the map entry */
  return hashent->muxinfo.direction;
}

/**
 * Find an entry in the cmux's map for this circuit or return NULL if there
 * is none.
 */

static chanid_circid_muxinfo_t *
circuitmux_find_map_entry(circuitmux_t *cmux, circuit_t *circ)
{
  chanid_circid_muxinfo_t search, *hashent = NULL;

  /* Sanity-check parameters */
  tor_assert(cmux);
  tor_assert(cmux->chanid_circid_map);
  tor_assert(circ);

  /* Check if we have n_chan */
  if (circ->n_chan) {
    /* Okay, let's see if it's attached for n_chan/n_circ_id */
    search.chan_id = circ->n_chan->global_identifier;
    search.circ_id = circ->n_circ_id;

    /* Query */
    hashent = HT_FIND(chanid_circid_muxinfo_map, cmux->chanid_circid_map,
                      &search);
  }

  /* Found something? */
  if (hashent) {
    /*
     * Assert that the direction makes sense for a hashent we found by
     * n_chan/n_circ_id before we return it.
     */
    tor_assert(hashent->muxinfo.direction == CELL_DIRECTION_OUT);
  } else {
    /* Not there, have we got a p_chan/p_circ_id to try? */
    if (circ->magic == OR_CIRCUIT_MAGIC) {
      search.circ_id = TO_OR_CIRCUIT(circ)->p_circ_id;
      /* Check for p_chan */
      if (TO_OR_CIRCUIT(circ)->p_chan) {
        search.chan_id = TO_OR_CIRCUIT(circ)->p_chan->global_identifier;
        /* Okay, search for that */
        hashent = HT_FIND(chanid_circid_muxinfo_map, cmux->chanid_circid_map,
                          &search);
        /* Find anything? */
        if (hashent) {
          /* Assert that the direction makes sense before we return it */
          tor_assert(hashent->muxinfo.direction == CELL_DIRECTION_IN);
        }
      }
    }
  }

  /* Okay, hashent is it if it was there */
  return hashent;
}

/**
 * Query whether a circuit is attached to a circuitmux
 */

int
circuitmux_is_circuit_attached(circuitmux_t *cmux, circuit_t *circ)
{
  chanid_circid_muxinfo_t *hashent = NULL;

  /* Look if it's in the circuit map */
  hashent = circuitmux_find_map_entry(cmux, circ);

  return (hashent != NULL);
}

/**
 * Query whether a circuit is active on a circuitmux
 */

int
circuitmux_is_circuit_active(circuitmux_t *cmux, circuit_t *circ)
{
  chanid_circid_muxinfo_t *hashent = NULL;
  int is_active = 0;

  tor_assert(cmux);
  tor_assert(circ);

  /* Look if it's in the circuit map */
  hashent = circuitmux_find_map_entry(cmux, circ);
  if (hashent) {
    /* Check the number of cells on this circuit */
    is_active = (hashent->muxinfo.cell_count > 0);
  }
  /* else not attached, so not active */

  return is_active;
}

/**
 * Query number of available cells for a circuit on a circuitmux
 */

unsigned int
circuitmux_num_cells_for_circuit(circuitmux_t *cmux, circuit_t *circ)
{
  chanid_circid_muxinfo_t *hashent = NULL;
  unsigned int n_cells = 0;

  tor_assert(cmux);
  tor_assert(circ);

  /* Look if it's in the circuit map */
  hashent = circuitmux_find_map_entry(cmux, circ);
  if (hashent) {
    /* Just get the cell count for this circuit */
    n_cells = hashent->muxinfo.cell_count;
  }
  /* else not attached, so 0 cells */

  return n_cells;
}

/**
 * Query total number of available cells on a circuitmux
 */

unsigned int
circuitmux_num_cells(circuitmux_t *cmux)
{
  tor_assert(cmux);

  return cmux->n_cells + cmux->destroy_cell_queue.n;
}

/**
 * Query total number of circuits active on a circuitmux
 */

unsigned int
circuitmux_num_active_circuits(circuitmux_t *cmux)
{
  tor_assert(cmux);

  return cmux->n_active_circuits;
}

/**
 * Query total number of circuits attached to a circuitmux
 */

unsigned int
circuitmux_num_circuits(circuitmux_t *cmux)
{
  tor_assert(cmux);

  return cmux->n_circuits;
}

/*
 * Functions for circuit code to call to update circuit status
 */

/**
 * Attach a circuit to a circuitmux, for the specified direction.
 */

MOCK_IMPL(void,
circuitmux_attach_circuit,(circuitmux_t *cmux, circuit_t *circ,
                           cell_direction_t direction))
{
  channel_t *chan = NULL;
  uint64_t channel_id;
  circid_t circ_id;
  chanid_circid_muxinfo_t search, *hashent = NULL;
  unsigned int cell_count;

  tor_assert(cmux);
  tor_assert(circ);
  tor_assert(direction == CELL_DIRECTION_IN ||
             direction == CELL_DIRECTION_OUT);
  circuitmux_assert_okay_paranoid(cmux);

  /*
   * Figure out which channel we're using, and get the circuit's current
   * cell count and circuit ID; assert that the circuit is not already
   * attached to another mux.
   */
  if (direction == CELL_DIRECTION_OUT) {
    /* It's n_chan */
    chan = circ->n_chan;
    cell_count = circ->n_chan_cells.n;
    circ_id = circ->n_circ_id;
  } else {
    /* We want p_chan */
    chan = TO_OR_CIRCUIT(circ)->p_chan;
    cell_count = TO_OR_CIRCUIT(circ)->p_chan_cells.n;
    circ_id = TO_OR_CIRCUIT(circ)->p_circ_id;
  }
  /* Assert that we did get a channel */
  tor_assert(chan);
  /* Assert that the circuit ID is sensible */
  tor_assert(circ_id != 0);

  /* Get the channel ID */
  channel_id = chan->global_identifier;

  /* See if we already have this one */
  search.chan_id = channel_id;
  search.circ_id = circ_id;
  hashent = HT_FIND(chanid_circid_muxinfo_map, cmux->chanid_circid_map,
                    &search);

  if (hashent) {
    /*
     * This circuit was already attached to this cmux; make sure the
     * directions match and update the cell count and active circuit count.
     */
    log_info(LD_CIRC,
             "Circuit %u on channel " U64_FORMAT " was already attached to "
             "cmux %p (trying to attach to %p)",
             (unsigned)circ_id, U64_PRINTF_ARG(channel_id),
             ((direction == CELL_DIRECTION_OUT) ?
                circ->n_mux : TO_OR_CIRCUIT(circ)->p_mux),
             cmux);

    /*
     * The mux pointer on this circuit and the direction in result should
     * match; otherwise assert.
     */
    if (direction == CELL_DIRECTION_OUT) tor_assert(circ->n_mux == cmux);
    else tor_assert(TO_OR_CIRCUIT(circ)->p_mux == cmux);
    tor_assert(hashent->muxinfo.direction == direction);

    /*
     * Looks okay; just update the cell count and active circuits if we must
     */
    if (hashent->muxinfo.cell_count > 0 && cell_count == 0) {
      --(cmux->n_active_circuits);
      circuitmux_make_circuit_inactive(cmux, circ, direction);
    } else if (hashent->muxinfo.cell_count == 0 && cell_count > 0) {
      ++(cmux->n_active_circuits);
      circuitmux_make_circuit_active(cmux, circ, direction);
    }
    cmux->n_cells -= hashent->muxinfo.cell_count;
    cmux->n_cells += cell_count;
    hashent->muxinfo.cell_count = cell_count;
  } else {
    /*
     * New circuit; add an entry and update the circuit/active circuit
     * counts.
     */
    log_debug(LD_CIRC,
             "Attaching circuit %u on channel " U64_FORMAT " to cmux %p",
              (unsigned)circ_id, U64_PRINTF_ARG(channel_id), cmux);

    /*
     * Assert that the circuit doesn't already have a mux for this
     * direction.
     */
    if (direction == CELL_DIRECTION_OUT) tor_assert(circ->n_mux == NULL);
    else tor_assert(TO_OR_CIRCUIT(circ)->p_mux == NULL);

    /* Insert it in the map */
    hashent = tor_malloc_zero(sizeof(*hashent));
    hashent->chan_id = channel_id;
    hashent->circ_id = circ_id;
    hashent->muxinfo.cell_count = cell_count;
    hashent->muxinfo.direction = direction;
    /* Allocate policy specific circuit data if we need it */
    if (cmux->policy && cmux->policy->alloc_circ_data) {
      /* Assert that we have the means to free policy-specific data */
      tor_assert(cmux->policy->free_circ_data);
      /* Allocate it */
      hashent->muxinfo.policy_data =
        cmux->policy->alloc_circ_data(cmux,
                                      cmux->policy_data,
                                      circ,
                                      direction,
                                      cell_count);
      /* If we wanted policy data, it's an error  not to get any */
      tor_assert(hashent->muxinfo.policy_data);
    }
    HT_INSERT(chanid_circid_muxinfo_map, cmux->chanid_circid_map,
              hashent);

    /* Set the circuit's mux for this direction */
    if (direction == CELL_DIRECTION_OUT) circ->n_mux = cmux;
    else TO_OR_CIRCUIT(circ)->p_mux = cmux;

    /* Make sure the next/prev pointers are NULL */
    if (direction == CELL_DIRECTION_OUT) {
      circ->next_active_on_n_chan = NULL;
      circ->prev_active_on_n_chan = NULL;
    } else {
      TO_OR_CIRCUIT(circ)->next_active_on_p_chan = NULL;
      TO_OR_CIRCUIT(circ)->prev_active_on_p_chan = NULL;
    }

    /* Update counters */
    ++(cmux->n_circuits);
    if (cell_count > 0) {
      ++(cmux->n_active_circuits);
      circuitmux_make_circuit_active(cmux, circ, direction);
    }
    cmux->n_cells += cell_count;
  }

  circuitmux_assert_okay_paranoid(cmux);
}

/**
 * Detach a circuit from a circuitmux and update all counters as needed;
 * no-op if not attached.
 */

MOCK_IMPL(void,
circuitmux_detach_circuit,(circuitmux_t *cmux, circuit_t *circ))
{
  chanid_circid_muxinfo_t search, *hashent = NULL;
  /*
   * Use this to keep track of whether we found it for n_chan or
   * p_chan for consistency checking.
   *
   * The 0 initializer is not a valid cell_direction_t value.
   * We assert that it has been replaced with a valid value before it is used.
   */
  cell_direction_t last_searched_direction = 0;

  tor_assert(cmux);
  tor_assert(cmux->chanid_circid_map);
  tor_assert(circ);
  circuitmux_assert_okay_paranoid(cmux);

  /* See if we have it for n_chan/n_circ_id */
  if (circ->n_chan) {
    search.chan_id = circ->n_chan->global_identifier;
    search.circ_id = circ->n_circ_id;
    hashent = HT_FIND(chanid_circid_muxinfo_map, cmux->chanid_circid_map,
                        &search);
    last_searched_direction = CELL_DIRECTION_OUT;
  }

  /* Got one? If not, see if it's an or_circuit_t and try p_chan/p_circ_id */
  if (!hashent) {
    if (circ->magic == OR_CIRCUIT_MAGIC) {
      search.circ_id = TO_OR_CIRCUIT(circ)->p_circ_id;
      if (TO_OR_CIRCUIT(circ)->p_chan) {
        search.chan_id = TO_OR_CIRCUIT(circ)->p_chan->global_identifier;
        hashent = HT_FIND(chanid_circid_muxinfo_map,
                            cmux->chanid_circid_map,
                            &search);
        last_searched_direction = CELL_DIRECTION_IN;
      }
    }
  }

  tor_assert(last_searched_direction == CELL_DIRECTION_OUT
             || last_searched_direction == CELL_DIRECTION_IN);

  /*
   * If hashent isn't NULL, we have a circuit to detach; don't remove it from
   * the map until later of circuitmux_make_circuit_inactive() breaks.
   */
  if (hashent) {
    /* Update counters */
    --(cmux->n_circuits);
    if (hashent->muxinfo.cell_count > 0) {
      --(cmux->n_active_circuits);
      /* This does policy notifies, so comes before freeing policy data */
      circuitmux_make_circuit_inactive(cmux, circ, last_searched_direction);
    }
    cmux->n_cells -= hashent->muxinfo.cell_count;

    /* Free policy-specific data if we have it */
    if (hashent->muxinfo.policy_data) {
      /* If we have policy data, assert that we have the means to free it */
      tor_assert(cmux->policy);
      tor_assert(cmux->policy->free_circ_data);
      /* Call free_circ_data() */
      cmux->policy->free_circ_data(cmux,
                                   cmux->policy_data,
                                   circ,
                                   hashent->muxinfo.policy_data);
      hashent->muxinfo.policy_data = NULL;
    }

    /* Consistency check: the direction must match the direction searched */
    tor_assert(last_searched_direction == hashent->muxinfo.direction);
    /* Clear the circuit's mux for this direction */
    if (last_searched_direction == CELL_DIRECTION_OUT) circ->n_mux = NULL;
    else TO_OR_CIRCUIT(circ)->p_mux = NULL;

    /* Now remove it from the map */
    HT_REMOVE(chanid_circid_muxinfo_map, cmux->chanid_circid_map, hashent);

    /* Free the hash entry */
    tor_free(hashent);
  }

  circuitmux_assert_okay_paranoid(cmux);
}

/**
 * Make a circuit active; update active list and policy-specific info, but
 * we don't mess with the counters or hash table here.
 */

static void
circuitmux_make_circuit_active(circuitmux_t *cmux, circuit_t *circ,
                               cell_direction_t direction)
{
  circuit_t **next_active = NULL, **prev_active = NULL, **next_prev = NULL;
  circuitmux_t *circuit_cmux = NULL;
  chanid_circid_muxinfo_t *hashent = NULL;
  channel_t *chan = NULL;
  circid_t circ_id;
  int already_active;

  tor_assert(cmux);
  tor_assert(circ);
  tor_assert(direction == CELL_DIRECTION_OUT ||
             direction == CELL_DIRECTION_IN);
  /*
   * Don't circuitmux_assert_okay_paranoid(cmux) here because the cell count
   * already got changed and we have to update the list for it to be consistent
   * again.
   */

  /* Get the right set of active list links for this direction */
  if (direction == CELL_DIRECTION_OUT) {
    next_active = &(circ->next_active_on_n_chan);
    prev_active = &(circ->prev_active_on_n_chan);
    circuit_cmux = circ->n_mux;
    chan = circ->n_chan;
    circ_id = circ->n_circ_id;
  } else {
    next_active = &(TO_OR_CIRCUIT(circ)->next_active_on_p_chan);
    prev_active = &(TO_OR_CIRCUIT(circ)->prev_active_on_p_chan);
    circuit_cmux = TO_OR_CIRCUIT(circ)->p_mux;
    chan = TO_OR_CIRCUIT(circ)->p_chan;
    circ_id = TO_OR_CIRCUIT(circ)->p_circ_id;
  }

  /* Assert that it is attached to this mux and a channel */
  tor_assert(cmux == circuit_cmux);
  tor_assert(chan != NULL);

  /*
   * Check if the circuit really was inactive; if it's active, at least one
   * of the next_active and prev_active pointers will not be NULL, or this
   * circuit will be either the head or tail of the list for this cmux.
   */
  already_active = (*prev_active != NULL || *next_active != NULL ||
                    cmux->active_circuits_head == circ ||
                    cmux->active_circuits_tail == circ);

  /* If we're already active, log a warning and finish */
  if (already_active) {
    log_warn(LD_CIRC,
             "Circuit %u on channel " U64_FORMAT " was already active",
             (unsigned)circ_id, U64_PRINTF_ARG(chan->global_identifier));
    return;
  }

  /*
   * This is going at the head of the list; if the old head is not NULL,
   * then its prev pointer should point to this.
   */
  *next_active = cmux->active_circuits_head; /* Next is old head */
  *prev_active = NULL; /* Prev is NULL (this will be the head) */
  if (cmux->active_circuits_head) {
    /* The list had an old head; update its prev pointer */
    next_prev =
      circuitmux_prev_active_circ_p(cmux, cmux->active_circuits_head);
    tor_assert(next_prev);
    *next_prev = circ;
  } else {
    /* The list was empty; this becomes the tail as well */
    cmux->active_circuits_tail = circ;
  }
  /* This becomes the new head of the list */
  cmux->active_circuits_head = circ;

  /* Policy-specific notification */
  if (cmux->policy &&
      cmux->policy->notify_circ_active) {
    /* Okay, we need to check the circuit for policy data now */
    hashent = circuitmux_find_map_entry(cmux, circ);
    /* We should have found something */
    tor_assert(hashent);
    /* Notify */
    cmux->policy->notify_circ_active(cmux, cmux->policy_data,
                                     circ, hashent->muxinfo.policy_data);
  }

  circuitmux_assert_okay_paranoid(cmux);
}

/**
 * Make a circuit inactive; update active list and policy-specific info, but
 * we don't mess with the counters or hash table here.
 */

static void
circuitmux_make_circuit_inactive(circuitmux_t *cmux, circuit_t *circ,
                                 cell_direction_t direction)
{
  circuit_t **next_active = NULL, **prev_active = NULL;
  circuit_t **next_prev = NULL, **prev_next = NULL;
  circuitmux_t *circuit_cmux = NULL;
  chanid_circid_muxinfo_t *hashent = NULL;
  channel_t *chan = NULL;
  circid_t circ_id;
  int already_inactive;

  tor_assert(cmux);
  tor_assert(circ);
  tor_assert(direction == CELL_DIRECTION_OUT ||
             direction == CELL_DIRECTION_IN);
  /*
   * Don't circuitmux_assert_okay_paranoid(cmux) here because the cell count
   * already got changed and we have to update the list for it to be consistent
   * again.
   */

  /* Get the right set of active list links for this direction */
  if (direction == CELL_DIRECTION_OUT) {
    next_active = &(circ->next_active_on_n_chan);
    prev_active = &(circ->prev_active_on_n_chan);
    circuit_cmux = circ->n_mux;
    chan = circ->n_chan;
    circ_id = circ->n_circ_id;
  } else {
    next_active = &(TO_OR_CIRCUIT(circ)->next_active_on_p_chan);
    prev_active = &(TO_OR_CIRCUIT(circ)->prev_active_on_p_chan);
    circuit_cmux = TO_OR_CIRCUIT(circ)->p_mux;
    chan = TO_OR_CIRCUIT(circ)->p_chan;
    circ_id = TO_OR_CIRCUIT(circ)->p_circ_id;
  }

  /* Assert that it is attached to this mux and a channel */
  tor_assert(cmux == circuit_cmux);
  tor_assert(chan != NULL);

  /*
   * Check if the circuit really was active; if it's inactive, the
   * next_active and prev_active pointers will be NULL and this circuit
   * will not be the head or tail of the list for this cmux.
   */
  already_inactive = (*prev_active == NULL && *next_active == NULL &&
                      cmux->active_circuits_head != circ &&
                      cmux->active_circuits_tail != circ);

  /* If we're already inactive, log a warning and finish */
  if (already_inactive) {
    log_warn(LD_CIRC,
             "Circuit %d on channel " U64_FORMAT " was already inactive",
             (unsigned)circ_id, U64_PRINTF_ARG(chan->global_identifier));
    return;
  }

  /* Remove from the list; first get next_prev and prev_next */
  if (*next_active) {
    /*
     * If there's a next circuit, its previous circuit becomes this
     * circuit's previous circuit.
     */
    next_prev = circuitmux_prev_active_circ_p(cmux, *next_active);
  } else {
    /* Else, the tail becomes this circuit's previous circuit */
    next_prev = &(cmux->active_circuits_tail);
  }

  /* Got next_prev, now prev_next */
  if (*prev_active) {
    /*
     * If there's a previous circuit, its next circuit becomes this circuit's
     * next circuit.
     */
    prev_next = circuitmux_next_active_circ_p(cmux, *prev_active);
  } else {
    /* Else, the head becomes this circuit's next circuit */
    prev_next = &(cmux->active_circuits_head);
  }

  /* Assert that we got sensible values for the next/prev pointers */
  tor_assert(next_prev != NULL);
  tor_assert(prev_next != NULL);

  /* Update the next/prev pointers - this removes circ from the list */
  *next_prev = *prev_active;
  *prev_next = *next_active;

  /* Now null out prev_active/next_active */
  *prev_active = NULL;
  *next_active = NULL;

  /* Policy-specific notification */
  if (cmux->policy &&
      cmux->policy->notify_circ_inactive) {
    /* Okay, we need to check the circuit for policy data now */
    hashent = circuitmux_find_map_entry(cmux, circ);
    /* We should have found something */
    tor_assert(hashent);
    /* Notify */
    cmux->policy->notify_circ_inactive(cmux, cmux->policy_data,
                                       circ, hashent->muxinfo.policy_data);
  }

  circuitmux_assert_okay_paranoid(cmux);
}

/**
 * Clear the cell counter for a circuit on a circuitmux
 */

void
circuitmux_clear_num_cells(circuitmux_t *cmux, circuit_t *circ)
{
  /* This is the same as setting the cell count to zero */
  circuitmux_set_num_cells(cmux, circ, 0);
}

/**
 * Set the cell counter for a circuit on a circuitmux
 */

void
circuitmux_set_num_cells(circuitmux_t *cmux, circuit_t *circ,
                         unsigned int n_cells)
{
  chanid_circid_muxinfo_t *hashent = NULL;

  tor_assert(cmux);
  tor_assert(circ);

  circuitmux_assert_okay_paranoid(cmux);

  /* Search for this circuit's entry */
  hashent = circuitmux_find_map_entry(cmux, circ);
  /* Assert that we found one */
  tor_assert(hashent);

  /* Update cmux cell counter */
  cmux->n_cells -= hashent->muxinfo.cell_count;
  cmux->n_cells += n_cells;

  /* Do we need to notify a cmux policy? */
  if (cmux->policy && cmux->policy->notify_set_n_cells) {
    /* Call notify_set_n_cells */
    cmux->policy->notify_set_n_cells(cmux,
                                     cmux->policy_data,
                                     circ,
                                     hashent->muxinfo.policy_data,
                                     n_cells);
  }

  /*
   * Update cmux active circuit counter: is the old cell count > 0 and the
   * new cell count == 0 ?
   */
  if (hashent->muxinfo.cell_count > 0 && n_cells == 0) {
    --(cmux->n_active_circuits);
    hashent->muxinfo.cell_count = n_cells;
    circuitmux_make_circuit_inactive(cmux, circ, hashent->muxinfo.direction);
  /* Is the old cell count == 0 and the new cell count > 0 ? */
  } else if (hashent->muxinfo.cell_count == 0 && n_cells > 0) {
    ++(cmux->n_active_circuits);
    hashent->muxinfo.cell_count = n_cells;
    circuitmux_make_circuit_active(cmux, circ, hashent->muxinfo.direction);
  } else {
    /*
     * Update the entry cell count like this so we can put a
     * circuitmux_assert_okay_paranoid inside make_circuit_(in)active() too.
     */
    hashent->muxinfo.cell_count = n_cells;
  }

  circuitmux_assert_okay_paranoid(cmux);
}

/*
 * Functions for channel code to call to get a circuit to transmit from or
 * notify that cells have been transmitted.
 */

/**
 * Pick a circuit to send from, using the active circuits list or a
 * circuitmux policy if one is available.  This is called from channel.c.
 *
 * If we would rather send a destroy cell, return NULL and set
 * *<b>destroy_queue_out</b> to the destroy queue.
 *
 * If we have nothing to send, set *<b>destroy_queue_out</b> to NULL and
 * return NULL.
 */

circuit_t *
circuitmux_get_first_active_circuit(circuitmux_t *cmux,
                                    cell_queue_t **destroy_queue_out)
{
  circuit_t *circ = NULL;

  tor_assert(cmux);
  tor_assert(destroy_queue_out);

  *destroy_queue_out = NULL;

  if (cmux->destroy_cell_queue.n &&
        (!cmux->last_cell_was_destroy || cmux->n_active_circuits == 0)) {
    /* We have destroy cells to send, and either we just sent a relay cell,
     * or we have no relay cells to send. */

    /* XXXX We should let the cmux policy have some say in this eventually. */
    /* XXXX Alternating is not a terribly brilliant approach here. */
    *destroy_queue_out = &cmux->destroy_cell_queue;

    cmux->last_cell_was_destroy = 1;
  } else if (cmux->n_active_circuits > 0) {
    /* We also must have a cell available for this to be the case */
    tor_assert(cmux->n_cells > 0);
    /* Do we have a policy-provided circuit selector? */
    if (cmux->policy && cmux->policy->pick_active_circuit) {
      circ = cmux->policy->pick_active_circuit(cmux, cmux->policy_data);
    }
    /* Fall back on the head of the active circuits list */
    if (!circ) {
      tor_assert(cmux->active_circuits_head);
      circ = cmux->active_circuits_head;
    }
    cmux->last_cell_was_destroy = 0;
  } else {
    tor_assert(cmux->n_cells == 0);
    tor_assert(cmux->destroy_cell_queue.n == 0);
  }

  return circ;
}

/**
 * Notify the circuitmux that cells have been sent on a circuit; this
 * is called from channel.c.
 */

void
circuitmux_notify_xmit_cells(circuitmux_t *cmux, circuit_t *circ,
                             unsigned int n_cells)
{
  chanid_circid_muxinfo_t *hashent = NULL;
  int becomes_inactive = 0;

  tor_assert(cmux);
  tor_assert(circ);
  circuitmux_assert_okay_paranoid(cmux);

  if (n_cells == 0) return;

  /*
   * To handle this, we have to:
   *
   * 1.) Adjust the circuit's cell counter in the cmux hash table
   * 2.) Move the circuit to the tail of the active_circuits linked list
   *     for this cmux, or make the circuit inactive if the cell count
   *     went to zero.
   * 3.) Call cmux->policy->notify_xmit_cells(), if any
   */

  /* Find the hash entry */
  hashent = circuitmux_find_map_entry(cmux, circ);
  /* Assert that we found one */
  tor_assert(hashent);

  /* Adjust the cell counter and assert that we had that many cells to send */
  tor_assert(n_cells <= hashent->muxinfo.cell_count);
  hashent->muxinfo.cell_count -= n_cells;
  /* Do we need to make the circuit inactive? */
  if (hashent->muxinfo.cell_count == 0) becomes_inactive = 1;
  /* Adjust the mux cell counter */
  cmux->n_cells -= n_cells;

  /* If we aren't making it inactive later, move it to the tail of the list */
  if (!becomes_inactive) {
    circuitmux_move_active_circ_to_tail(cmux, circ,
                                        hashent->muxinfo.direction);
  }

  /*
   * We call notify_xmit_cells() before making the circuit inactive if needed,
   * so the policy can always count on this coming in on an active circuit.
   */
  if (cmux->policy && cmux->policy->notify_xmit_cells) {
    cmux->policy->notify_xmit_cells(cmux, cmux->policy_data, circ,
                                    hashent->muxinfo.policy_data,
                                    n_cells);
  }

  /*
   * Now make the circuit inactive if needed; this will call the policy's
   * notify_circ_inactive() if present.
   */
  if (becomes_inactive) {
    --(cmux->n_active_circuits);
    circuitmux_make_circuit_inactive(cmux, circ, hashent->muxinfo.direction);
  }

  circuitmux_assert_okay_paranoid(cmux);
}

/**
 * Notify the circuitmux that a destroy was sent, so we can update
 * the counter.
 */

void
circuitmux_notify_xmit_destroy(circuitmux_t *cmux)
{
  tor_assert(cmux);

  --(cmux->destroy_ctr);
  --(global_destroy_ctr);
  log_debug(LD_CIRC,
            "Cmux at %p sent a destroy, cmux counter is now "I64_FORMAT", "
            "global counter is now "I64_FORMAT,
            cmux,
            I64_PRINTF_ARG(cmux->destroy_ctr),
            I64_PRINTF_ARG(global_destroy_ctr));
}

/*
 * Circuitmux consistency checking assertions
 */

/**
 * Check that circuitmux data structures are consistent and fail with an
 * assert if not.
 */

void
circuitmux_assert_okay(circuitmux_t *cmux)
{
  tor_assert(cmux);

  /*
   * Pass 1: iterate the hash table; for each entry:
   *  a) Check that the circuit has this cmux for n_mux or p_mux
   *  b) If the cell_count is > 0, set the mark bit; otherwise clear it
   *  c) Also check activeness (cell_count > 0 should be active)
   *  d) Count the number of circuits, active circuits and queued cells
   *     and at the end check that they match the counters in the cmux.
   *
   * Pass 2: iterate the active circuits list; for each entry,
   *         make sure the circuit is attached to this mux and appears
   *         in the hash table.  Make sure the mark bit is 1, and clear
   *         it in the hash table entry.  Consistency-check the linked
   *         list pointers.
   *
   * Pass 3: iterate the hash table again; assert if any active circuits
   *         (mark bit set to 1) are discovered that weren't cleared in pass 2
   *         (don't appear in the linked list).
   */

  circuitmux_assert_okay_pass_one(cmux);
  circuitmux_assert_okay_pass_two(cmux);
  circuitmux_assert_okay_pass_three(cmux);
}

/**
 * Do the first pass of circuitmux_assert_okay(); see the comment in that
 * function.
 */

static void
circuitmux_assert_okay_pass_one(circuitmux_t *cmux)
{
  chanid_circid_muxinfo_t **i = NULL;
  uint64_t chan_id;
  channel_t *chan;
  circid_t circ_id;
  circuit_t *circ;
  or_circuit_t *or_circ;
  unsigned int circ_is_active;
  circuit_t **next_p, **prev_p;
  unsigned int n_circuits, n_active_circuits, n_cells;

  tor_assert(cmux);
  tor_assert(cmux->chanid_circid_map);

  /* Reset the counters */
  n_circuits = n_active_circuits = n_cells = 0;
  /* Start iterating the hash table */
  i = HT_START(chanid_circid_muxinfo_map, cmux->chanid_circid_map);
  while (i) {
    /* Assert that the hash table entry isn't null */
    tor_assert(*i);

    /* Get the channel and circuit id */
    chan_id = (*i)->chan_id;
    circ_id = (*i)->circ_id;

    /* Find the channel and circuit, assert that they exist */
    chan = channel_find_by_global_id(chan_id);
    tor_assert(chan);
    circ = circuit_get_by_circid_channel_even_if_marked(circ_id, chan);
    tor_assert(circ);
    /* Clear the circ_is_active bit to start */
    circ_is_active = 0;

    /* Assert that we know which direction this is going */
    tor_assert((*i)->muxinfo.direction == CELL_DIRECTION_OUT ||
               (*i)->muxinfo.direction == CELL_DIRECTION_IN);

    if ((*i)->muxinfo.direction == CELL_DIRECTION_OUT) {
      /* We should be n_mux on this circuit */
      tor_assert(cmux == circ->n_mux);
      tor_assert(chan == circ->n_chan);
      /* Get next and prev for next test */
      next_p = &(circ->next_active_on_n_chan);
      prev_p = &(circ->prev_active_on_n_chan);
    } else {
      /* This should be an or_circuit_t and we should be p_mux */
      or_circ = TO_OR_CIRCUIT(circ);
      tor_assert(cmux == or_circ->p_mux);
      tor_assert(chan == or_circ->p_chan);
      /* Get next and prev for next test */
      next_p = &(or_circ->next_active_on_p_chan);
      prev_p = &(or_circ->prev_active_on_p_chan);
    }

    /*
     * Should this circuit be active?  I.e., does the mux know about > 0
     * cells on it?
     */
    circ_is_active = ((*i)->muxinfo.cell_count > 0);

    /* It should be in the linked list iff it's active */
    if (circ_is_active) {
      /* Either we have a next link or we are the tail */
      tor_assert(*next_p || (circ == cmux->active_circuits_tail));
      /* Either we have a prev link or we are the head */
      tor_assert(*prev_p || (circ == cmux->active_circuits_head));
      /* Increment the active circuits counter */
      ++n_active_circuits;
    } else {
      /* Shouldn't be in list, so no next or prev link */
      tor_assert(!(*next_p));
      tor_assert(!(*prev_p));
      /* And can't be head or tail */
      tor_assert(circ != cmux->active_circuits_head);
      tor_assert(circ != cmux->active_circuits_tail);
    }

    /* Increment the circuits counter */
    ++n_circuits;
    /* Adjust the cell counter */
    n_cells += (*i)->muxinfo.cell_count;

    /* Set the mark bit to circ_is_active */
    (*i)->muxinfo.mark = circ_is_active;

    /* Advance to the next entry */
    i = HT_NEXT(chanid_circid_muxinfo_map, cmux->chanid_circid_map, i);
  }

  /* Now check the counters */
  tor_assert(n_cells == cmux->n_cells);
  tor_assert(n_circuits == cmux->n_circuits);
  tor_assert(n_active_circuits == cmux->n_active_circuits);
}

/**
 * Do the second pass of circuitmux_assert_okay(); see the comment in that
 * function.
 */

static void
circuitmux_assert_okay_pass_two(circuitmux_t *cmux)
{
  circuit_t *curr_circ, *prev_circ = NULL, *next_circ;
  or_circuit_t *curr_or_circ;
  uint64_t curr_chan_id;
  circid_t curr_circ_id;
  circuit_t **next_p, **prev_p;
  channel_t *chan;
  unsigned int n_active_circuits = 0;
  cell_direction_t direction;
  chanid_circid_muxinfo_t search, *hashent = NULL;

  tor_assert(cmux);
  tor_assert(cmux->chanid_circid_map);

  /*
   * Walk the linked list of active circuits in cmux; keep track of the
   * previous circuit seen for consistency checking purposes.  Count them
   * to make sure the number in the linked list matches
   * cmux->n_active_circuits.
   */
  curr_circ = cmux->active_circuits_head;
  while (curr_circ) {
    /* Reset some things */
    chan = NULL;
    curr_or_circ = NULL;
    next_circ = NULL;
    next_p = prev_p = NULL;
    direction = 0;

    /* Figure out if this is n_mux or p_mux */
    if (cmux == curr_circ->n_mux) {
      /* Get next_p and prev_p */
      next_p = &(curr_circ->next_active_on_n_chan);
      prev_p = &(curr_circ->prev_active_on_n_chan);
      /* Get the channel */
      chan = curr_circ->n_chan;
      /* Get the circuit id */
      curr_circ_id = curr_circ->n_circ_id;
      /* Remember the direction */
      direction = CELL_DIRECTION_OUT;
    } else {
      /* We must be p_mux and this must be an or_circuit_t */
      curr_or_circ = TO_OR_CIRCUIT(curr_circ);
      tor_assert(cmux == curr_or_circ->p_mux);
      /* Get next_p and prev_p */
      next_p = &(curr_or_circ->next_active_on_p_chan);
      prev_p = &(curr_or_circ->prev_active_on_p_chan);
      /* Get the channel */
      chan = curr_or_circ->p_chan;
      /* Get the circuit id */
      curr_circ_id = curr_or_circ->p_circ_id;
      /* Remember the direction */
      direction = CELL_DIRECTION_IN;
    }

    /* Assert that we got a channel and get the channel ID */
    tor_assert(chan);
    curr_chan_id = chan->global_identifier;

    /* Assert that prev_p points to last circuit we saw */
    tor_assert(*prev_p == prev_circ);
    /* If that's NULL, assert that we are the head */
    if (!(*prev_p)) tor_assert(curr_circ == cmux->active_circuits_head);

    /* Get the next circuit */
    next_circ = *next_p;
    /* If it's NULL, assert that we are the tail */
    if (!(*next_p)) tor_assert(curr_circ == cmux->active_circuits_tail);

    /* Now find the hash table entry for this circuit */
    search.chan_id = curr_chan_id;
    search.circ_id = curr_circ_id;
    hashent = HT_FIND(chanid_circid_muxinfo_map, cmux->chanid_circid_map,
                      &search);

    /* Assert that we have one */
    tor_assert(hashent);

    /* Assert that the direction matches */
    tor_assert(direction == hashent->muxinfo.direction);

    /* Assert that the hash entry got marked in pass one */
    tor_assert(hashent->muxinfo.mark);

    /* Clear the mark */
    hashent->muxinfo.mark = 0;

    /* Increment the counter */
    ++n_active_circuits;

    /* Advance to the next active circuit and update prev_circ */
    prev_circ = curr_circ;
    curr_circ = next_circ;
  }

  /* Assert that the counter matches the cmux */
  tor_assert(n_active_circuits == cmux->n_active_circuits);
}

/**
 * Do the third pass of circuitmux_assert_okay(); see the comment in that
 * function.
 */

static void
circuitmux_assert_okay_pass_three(circuitmux_t *cmux)
{
  chanid_circid_muxinfo_t **i = NULL;

  tor_assert(cmux);
  tor_assert(cmux->chanid_circid_map);

  /* Start iterating the hash table */
  i = HT_START(chanid_circid_muxinfo_map, cmux->chanid_circid_map);

  /* Advance through each entry */
  while (i) {
    /* Assert that it isn't null */
    tor_assert(*i);

    /*
     * Assert that this entry is not marked - i.e., that either we didn't
     * think it should be active in pass one or we saw it in the active
     * circuits linked list.
     */
    tor_assert(!((*i)->muxinfo.mark));

    /* Advance to the next entry */
    i = HT_NEXT(chanid_circid_muxinfo_map, cmux->chanid_circid_map, i);
  }
}

/*DOCDOC */
void
circuitmux_append_destroy_cell(channel_t *chan,
                               circuitmux_t *cmux,
                               circid_t circ_id,
                               uint8_t reason)
{
  cell_t cell;
  memset(&cell, 0, sizeof(cell_t));
  cell.circ_id = circ_id;
  cell.command = CELL_DESTROY;
  cell.payload[0] = (uint8_t) reason;

  cell_queue_append_packed_copy(NULL, &cmux->destroy_cell_queue, 0, &cell,
                                chan->wide_circ_ids, 0);

  /* Destroy entering the queue, update counters */
  ++(cmux->destroy_ctr);
  ++global_destroy_ctr;
  log_debug(LD_CIRC,
            "Cmux at %p queued a destroy for circ %u, cmux counter is now "
            I64_FORMAT", global counter is now "I64_FORMAT,
            cmux, circ_id,
            I64_PRINTF_ARG(cmux->destroy_ctr),
            I64_PRINTF_ARG(global_destroy_ctr));

  /* XXXX Duplicate code from append_cell_to_circuit_queue */
  if (!channel_has_queued_writes(chan)) {
    /* There is no data at all waiting to be sent on the outbuf.  Add a
     * cell, so that we can notice when it gets flushed, flushed_some can
     * get called, and we can start putting more data onto the buffer then.
     */
    log_debug(LD_GENERAL, "Primed a buffer.");
    channel_flush_from_first_active_circuit(chan, 1);
  }
}

/*DOCDOC; for debugging 12184.  This runs slowly. */
int64_t
circuitmux_count_queued_destroy_cells(const channel_t *chan,
                                      const circuitmux_t *cmux)
{
  int64_t n_destroy_cells = cmux->destroy_ctr;
  int64_t destroy_queue_size = cmux->destroy_cell_queue.n;

  int64_t manual_total = 0;
  int64_t manual_total_in_map = 0;
  packed_cell_t *cell;

  TOR_SIMPLEQ_FOREACH(cell, &cmux->destroy_cell_queue.head, next) {
    circid_t id;
    ++manual_total;

    id = packed_cell_get_circid(cell, chan->wide_circ_ids);
    if (circuit_id_in_use_on_channel(id, (channel_t*)chan))
      ++manual_total_in_map;
  }

  if (n_destroy_cells != destroy_queue_size ||
      n_destroy_cells != manual_total ||
      n_destroy_cells != manual_total_in_map) {
    log_warn(LD_BUG, "  Discrepancy in counts for queued destroy cells on "
             "circuitmux. n="I64_FORMAT". queue_size="I64_FORMAT". "
             "manual_total="I64_FORMAT". manual_total_in_map="I64_FORMAT".",
             I64_PRINTF_ARG(n_destroy_cells),
             I64_PRINTF_ARG(destroy_queue_size),
             I64_PRINTF_ARG(manual_total),
             I64_PRINTF_ARG(manual_total_in_map));
  }

  return n_destroy_cells;
}

