/* Copyright (c) 2001 Matej Pfajfar.
 * Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2015, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file onion.c
 * \brief Functions to queue create cells, wrap the various onionskin types,
 * and parse and create the CREATE cell and its allies.
 **/

#include "or.h"
#include "circuitlist.h"
#include "config.h"
#include "cpuworker.h"
#include "networkstatus.h"
#include "onion.h"
#include "onion_fast.h"
#include "onion_ntor.h"
#include "onion_tap.h"
#include "relay.h"
#include "rephist.h"
#include "router.h"

/** Type for a linked list of circuits that are waiting for a free CPU worker
 * to process a waiting onion handshake. */
typedef struct onion_queue_t {
  TOR_TAILQ_ENTRY(onion_queue_t) next;
  or_circuit_t *circ;
  uint16_t handshake_type;
  create_cell_t *onionskin;
  time_t when_added;
} onion_queue_t;

/** 5 seconds on the onion queue til we just send back a destroy */
#define ONIONQUEUE_WAIT_CUTOFF 5

/** Array of queues of circuits waiting for CPU workers. An element is NULL
 * if that queue is empty.*/
TOR_TAILQ_HEAD(onion_queue_head_t, onion_queue_t)
              ol_list[MAX_ONION_HANDSHAKE_TYPE+1] = {
  TOR_TAILQ_HEAD_INITIALIZER(ol_list[0]), /* tap */
  TOR_TAILQ_HEAD_INITIALIZER(ol_list[1]), /* fast */
  TOR_TAILQ_HEAD_INITIALIZER(ol_list[2]), /* ntor */
};

/** Number of entries of each type currently in each element of ol_list[]. */
static int ol_entries[MAX_ONION_HANDSHAKE_TYPE+1];

static int num_ntors_per_tap(void);
static void onion_queue_entry_remove(onion_queue_t *victim);

/* XXXX024 Check lengths vs MAX_ONIONSKIN_{CHALLENGE,REPLY}_LEN.
 *
 * (By which I think I meant, "make sure that no
 * X_ONIONSKIN_CHALLENGE/REPLY_LEN is greater than
 * MAX_ONIONSKIN_CHALLENGE/REPLY_LEN."  Also, make sure that we can pass
 * over-large values via EXTEND2/EXTENDED2, for future-compatibility.*/

/** Return true iff we have room to queue another onionskin of type
 * <b>type</b>. */
static int
have_room_for_onionskin(uint16_t type)
{
  const or_options_t *options = get_options();
  int num_cpus;
  uint64_t tap_usec, ntor_usec;
  uint64_t ntor_during_tap_usec, tap_during_ntor_usec;

  /* If we've got fewer than 50 entries, we always have room for one more. */
  if (ol_entries[type] < 50)
    return 1;
  num_cpus = get_num_cpus(options);
  /* Compute how many microseconds we'd expect to need to clear all
   * onionskins in various combinations of the queues. */

  /* How long would it take to process all the TAP cells in the queue? */
  tap_usec  = estimated_usec_for_onionskins(
                                    ol_entries[ONION_HANDSHAKE_TYPE_TAP],
                                    ONION_HANDSHAKE_TYPE_TAP) / num_cpus;

  /* How long would it take to process all the NTor cells in the queue? */
  ntor_usec = estimated_usec_for_onionskins(
                                    ol_entries[ONION_HANDSHAKE_TYPE_NTOR],
                                    ONION_HANDSHAKE_TYPE_NTOR) / num_cpus;

  /* How long would it take to process the tap cells that we expect to
   * process while draining the ntor queue? */
  tap_during_ntor_usec  = estimated_usec_for_onionskins(
    MIN(ol_entries[ONION_HANDSHAKE_TYPE_TAP],
        ol_entries[ONION_HANDSHAKE_TYPE_NTOR] / num_ntors_per_tap()),
                                    ONION_HANDSHAKE_TYPE_TAP) / num_cpus;

  /* How long would it take to process the ntor cells that we expect to
   * process while draining the tap queue? */
  ntor_during_tap_usec  = estimated_usec_for_onionskins(
    MIN(ol_entries[ONION_HANDSHAKE_TYPE_NTOR],
        ol_entries[ONION_HANDSHAKE_TYPE_TAP] * num_ntors_per_tap()),
                                    ONION_HANDSHAKE_TYPE_NTOR) / num_cpus;

  /* See whether that exceeds MaxOnionQueueDelay. If so, we can't queue
   * this. */
  if (type == ONION_HANDSHAKE_TYPE_NTOR &&
      (ntor_usec + tap_during_ntor_usec) / 1000 >
       (uint64_t)options->MaxOnionQueueDelay)
    return 0;

  if (type == ONION_HANDSHAKE_TYPE_TAP &&
      (tap_usec + ntor_during_tap_usec) / 1000 >
       (uint64_t)options->MaxOnionQueueDelay)
    return 0;

  /* If we support the ntor handshake, then don't let TAP handshakes use
   * more than 2/3 of the space on the queue. */
  if (type == ONION_HANDSHAKE_TYPE_TAP &&
      tap_usec / 1000 > (uint64_t)options->MaxOnionQueueDelay * 2 / 3)
    return 0;

  return 1;
}

/** Add <b>circ</b> to the end of ol_list and return 0, except
 * if ol_list is too long, in which case do nothing and return -1.
 */
int
onion_pending_add(or_circuit_t *circ, create_cell_t *onionskin)
{
  onion_queue_t *tmp;
  time_t now = time(NULL);

  if (onionskin->handshake_type > MAX_ONION_HANDSHAKE_TYPE) {
    log_warn(LD_BUG, "Handshake %d out of range! Dropping.",
             onionskin->handshake_type);
    return -1;
  }

  tmp = tor_malloc_zero(sizeof(onion_queue_t));
  tmp->circ = circ;
  tmp->handshake_type = onionskin->handshake_type;
  tmp->onionskin = onionskin;
  tmp->when_added = now;

  if (!have_room_for_onionskin(onionskin->handshake_type)) {
#define WARN_TOO_MANY_CIRC_CREATIONS_INTERVAL (60)
    static ratelim_t last_warned =
      RATELIM_INIT(WARN_TOO_MANY_CIRC_CREATIONS_INTERVAL);
    char *m;
    if (onionskin->handshake_type == ONION_HANDSHAKE_TYPE_NTOR &&
        (m = rate_limit_log(&last_warned, approx_time()))) {
      log_warn(LD_GENERAL,
               "Your computer is too slow to handle this many circuit "
               "creation requests! Please consider using the "
               "MaxAdvertisedBandwidth config option or choosing a more "
               "restricted exit policy.%s",m);
      tor_free(m);
    }
    tor_free(tmp);
    return -1;
  }

  ++ol_entries[onionskin->handshake_type];
  log_info(LD_OR, "New create (%s). Queues now ntor=%d and tap=%d.",
    onionskin->handshake_type == ONION_HANDSHAKE_TYPE_NTOR ? "ntor" : "tap",
    ol_entries[ONION_HANDSHAKE_TYPE_NTOR],
    ol_entries[ONION_HANDSHAKE_TYPE_TAP]);

  circ->onionqueue_entry = tmp;
  TOR_TAILQ_INSERT_TAIL(&ol_list[onionskin->handshake_type], tmp, next);

  /* cull elderly requests. */
  while (1) {
    onion_queue_t *head = TOR_TAILQ_FIRST(&ol_list[onionskin->handshake_type]);
    if (now - head->when_added < (time_t)ONIONQUEUE_WAIT_CUTOFF)
      break;

    circ = head->circ;
    circ->onionqueue_entry = NULL;
    onion_queue_entry_remove(head);
    log_info(LD_CIRC,
             "Circuit create request is too old; canceling due to overload.");
    circuit_mark_for_close(TO_CIRCUIT(circ), END_CIRC_REASON_RESOURCELIMIT);
  }
  return 0;
}

/** Return a fairness parameter, to prefer processing NTOR style
 * handshakes but still slowly drain the TAP queue so we don't starve
 * it entirely. */
static int
num_ntors_per_tap(void)
{
#define DEFAULT_NUM_NTORS_PER_TAP 10
#define MIN_NUM_NTORS_PER_TAP 1
#define MAX_NUM_NTORS_PER_TAP 100000

  return networkstatus_get_param(NULL, "NumNTorsPerTAP",
                                 DEFAULT_NUM_NTORS_PER_TAP,
                                 MIN_NUM_NTORS_PER_TAP,
                                 MAX_NUM_NTORS_PER_TAP);
}

/** Choose which onion queue we'll pull from next. If one is empty choose
 * the other; if they both have elements, load balance across them but
 * favoring NTOR. */
static uint16_t
decide_next_handshake_type(void)
{
  /* The number of times we've chosen ntor lately when both were available. */
  static int recently_chosen_ntors = 0;

  if (!ol_entries[ONION_HANDSHAKE_TYPE_NTOR])
    return ONION_HANDSHAKE_TYPE_TAP; /* no ntors? try tap */

  if (!ol_entries[ONION_HANDSHAKE_TYPE_TAP]) {

    /* Nick wants us to prioritize new tap requests when there aren't
     * any in the queue and we've processed k ntor cells since the last
     * tap cell. This strategy is maybe a good idea, since it starves tap
     * less in the case where tap is rare, or maybe a poor idea, since it
     * makes the new tap cell unfairly jump in front of ntor cells that
     * got here first. In any case this edge case will only become relevant
     * once tap is rare. We should reevaluate whether we like this decision
     * once tap gets more rare. */
    if (ol_entries[ONION_HANDSHAKE_TYPE_NTOR] &&
        recently_chosen_ntors <= num_ntors_per_tap())
      ++recently_chosen_ntors;

    return ONION_HANDSHAKE_TYPE_NTOR; /* no taps? try ntor */
  }

  /* They both have something queued. Pick ntor if we haven't done that
   * too much lately. */
  if (++recently_chosen_ntors <= num_ntors_per_tap()) {
    return ONION_HANDSHAKE_TYPE_NTOR;
  }

  /* Else, it's time to let tap have its turn. */
  recently_chosen_ntors = 0;
  return ONION_HANDSHAKE_TYPE_TAP;
}

/** Remove the highest priority item from ol_list[] and return it, or
 * return NULL if the lists are empty.
 */
or_circuit_t *
onion_next_task(create_cell_t **onionskin_out)
{
  or_circuit_t *circ;
  uint16_t handshake_to_choose = decide_next_handshake_type();
  onion_queue_t *head = TOR_TAILQ_FIRST(&ol_list[handshake_to_choose]);

  if (!head)
    return NULL; /* no onions pending, we're done */

  tor_assert(head->circ);
  tor_assert(head->handshake_type <= MAX_ONION_HANDSHAKE_TYPE);
//  tor_assert(head->circ->p_chan); /* make sure it's still valid */
/* XXX I only commented out the above line to make the unit tests
 * more manageable. That's probably not good long-term. -RD */
  circ = head->circ;
  if (head->onionskin)
    --ol_entries[head->handshake_type];
  log_info(LD_OR, "Processing create (%s). Queues now ntor=%d and tap=%d.",
    head->handshake_type == ONION_HANDSHAKE_TYPE_NTOR ? "ntor" : "tap",
    ol_entries[ONION_HANDSHAKE_TYPE_NTOR],
    ol_entries[ONION_HANDSHAKE_TYPE_TAP]);

  *onionskin_out = head->onionskin;
  head->onionskin = NULL; /* prevent free. */
  circ->onionqueue_entry = NULL;
  onion_queue_entry_remove(head);
  return circ;
}

/** Return the number of <b>handshake_type</b>-style create requests pending.
 */
int
onion_num_pending(uint16_t handshake_type)
{
  return ol_entries[handshake_type];
}

/** Go through ol_list, find the onion_queue_t element which points to
 * circ, remove and free that element. Leave circ itself alone.
 */
void
onion_pending_remove(or_circuit_t *circ)
{
  onion_queue_t *victim;

  if (!circ)
    return;

  victim = circ->onionqueue_entry;
  if (victim)
    onion_queue_entry_remove(victim);

  cpuworker_cancel_circ_handshake(circ);
}

/** Remove a queue entry <b>victim</b> from the queue, unlinking it from
 * its circuit and freeing it and any structures it owns.*/
static void
onion_queue_entry_remove(onion_queue_t *victim)
{
  if (victim->handshake_type > MAX_ONION_HANDSHAKE_TYPE) {
    log_warn(LD_BUG, "Handshake %d out of range! Dropping.",
             victim->handshake_type);
    /* XXX leaks */
    return;
  }

  TOR_TAILQ_REMOVE(&ol_list[victim->handshake_type], victim, next);

  if (victim->circ)
    victim->circ->onionqueue_entry = NULL;

  if (victim->onionskin)
    --ol_entries[victim->handshake_type];

  tor_free(victim->onionskin);
  tor_free(victim);
}

/** Remove all circuits from the pending list.  Called from tor_free_all. */
void
clear_pending_onions(void)
{
  onion_queue_t *victim, *next;
  int i;
  for (i=0; i<=MAX_ONION_HANDSHAKE_TYPE; i++) {
    for (victim = TOR_TAILQ_FIRST(&ol_list[i]); victim; victim = next) {
      next = TOR_TAILQ_NEXT(victim,next);
      onion_queue_entry_remove(victim);
    }
    tor_assert(TOR_TAILQ_EMPTY(&ol_list[i]));
  }
  memset(ol_entries, 0, sizeof(ol_entries));
}

/* ============================================================ */

/** Return a new server_onion_keys_t object with all of the keys
 * and other info we might need to do onion handshakes.  (We make a copy of
 * our keys for each cpuworker to avoid race conditions with the main thread,
 * and to avoid locking) */
server_onion_keys_t *
server_onion_keys_new(void)
{
  server_onion_keys_t *keys = tor_malloc_zero(sizeof(server_onion_keys_t));
  memcpy(keys->my_identity, router_get_my_id_digest(), DIGEST_LEN);
  dup_onion_keys(&keys->onion_key, &keys->last_onion_key);
  keys->curve25519_key_map = construct_ntor_key_map();
  keys->junk_keypair = tor_malloc_zero(sizeof(curve25519_keypair_t));
  curve25519_keypair_generate(keys->junk_keypair, 0);
  return keys;
}

/** Release all storage held in <b>keys</b>. */
void
server_onion_keys_free(server_onion_keys_t *keys)
{
  if (! keys)
    return;

  crypto_pk_free(keys->onion_key);
  crypto_pk_free(keys->last_onion_key);
  ntor_key_map_free(keys->curve25519_key_map);
  tor_free(keys->junk_keypair);
  memwipe(keys, 0, sizeof(server_onion_keys_t));
  tor_free(keys);
}

/** Release whatever storage is held in <b>state</b>, depending on its
 * type, and clear its pointer. */
void
onion_handshake_state_release(onion_handshake_state_t *state)
{
  switch (state->tag) {
  case ONION_HANDSHAKE_TYPE_TAP:
    crypto_dh_free(state->u.tap);
    state->u.tap = NULL;
    break;
  case ONION_HANDSHAKE_TYPE_FAST:
    fast_handshake_state_free(state->u.fast);
    state->u.fast = NULL;
    break;
  case ONION_HANDSHAKE_TYPE_NTOR:
    ntor_handshake_state_free(state->u.ntor);
    state->u.ntor = NULL;
    break;
  default:
    log_warn(LD_BUG, "called with unknown handshake state type %d",
             (int)state->tag);
    tor_fragile_assert();
  }
}

/** Perform the first step of a circuit-creation handshake of type <b>type</b>
 * (one of ONION_HANDSHAKE_TYPE_*): generate the initial "onion skin" in
 * <b>onion_skin_out</b>, and store any state information in <b>state_out</b>.
 * Return -1 on failure, and the length of the onionskin on acceptance.
 */
int
onion_skin_create(int type,
                  const extend_info_t *node,
                  onion_handshake_state_t *state_out,
                  uint8_t *onion_skin_out)
{
  int r = -1;

  switch (type) {
  case ONION_HANDSHAKE_TYPE_TAP:
    if (!node->onion_key)
      return -1;

    if (onion_skin_TAP_create(node->onion_key,
                              &state_out->u.tap,
                              (char*)onion_skin_out) < 0)
      return -1;

    r = TAP_ONIONSKIN_CHALLENGE_LEN;
    break;
  case ONION_HANDSHAKE_TYPE_FAST:
    if (fast_onionskin_create(&state_out->u.fast, onion_skin_out) < 0)
      return -1;

    r = CREATE_FAST_LEN;
    break;
  case ONION_HANDSHAKE_TYPE_NTOR:
    if (tor_mem_is_zero((const char*)node->curve25519_onion_key.public_key,
                        CURVE25519_PUBKEY_LEN))
      return -1;
    if (onion_skin_ntor_create((const uint8_t*)node->identity_digest,
                               &node->curve25519_onion_key,
                               &state_out->u.ntor,
                               onion_skin_out) < 0)
      return -1;

    r = NTOR_ONIONSKIN_LEN;
    break;
  default:
    log_warn(LD_BUG, "called with unknown handshake state type %d", type);
    tor_fragile_assert();
    r = -1;
  }

  if (r > 0)
    state_out->tag = (uint16_t) type;

  return r;
}

/** Perform the second (server-side) step of a circuit-creation handshake of
 * type <b>type</b>, responding to the client request in <b>onion_skin</b>
 * using the keys in <b>keys</b>.  On success, write our response into
 * <b>reply_out</b>, generate <b>keys_out_len</b> bytes worth of key material
 * in <b>keys_out_len</b>, a hidden service nonce to <b>rend_nonce_out</b>,
 * and return the length of the reply. On failure, return -1.
 */
int
onion_skin_server_handshake(int type,
                      const uint8_t *onion_skin, size_t onionskin_len,
                      const server_onion_keys_t *keys,
                      uint8_t *reply_out,
                      uint8_t *keys_out, size_t keys_out_len,
                      uint8_t *rend_nonce_out)
{
  int r = -1;

  switch (type) {
  case ONION_HANDSHAKE_TYPE_TAP:
    if (onionskin_len != TAP_ONIONSKIN_CHALLENGE_LEN)
      return -1;
    if (onion_skin_TAP_server_handshake((const char*)onion_skin,
                                        keys->onion_key, keys->last_onion_key,
                                        (char*)reply_out,
                                        (char*)keys_out, keys_out_len)<0)
      return -1;
    r = TAP_ONIONSKIN_REPLY_LEN;
    memcpy(rend_nonce_out, reply_out+DH_KEY_LEN, DIGEST_LEN);
    break;
  case ONION_HANDSHAKE_TYPE_FAST:
    if (onionskin_len != CREATE_FAST_LEN)
      return -1;
    if (fast_server_handshake(onion_skin, reply_out, keys_out, keys_out_len)<0)
      return -1;
    r = CREATED_FAST_LEN;
    memcpy(rend_nonce_out, reply_out+DIGEST_LEN, DIGEST_LEN);
    break;
  case ONION_HANDSHAKE_TYPE_NTOR:
    if (onionskin_len < NTOR_ONIONSKIN_LEN)
      return -1;
    {
      size_t keys_tmp_len = keys_out_len + DIGEST_LEN;
      uint8_t *keys_tmp = tor_malloc(keys_out_len + DIGEST_LEN);

      if (onion_skin_ntor_server_handshake(
                                   onion_skin, keys->curve25519_key_map,
                                   keys->junk_keypair,
                                   keys->my_identity,
                                   reply_out, keys_tmp, keys_tmp_len)<0) {
        tor_free(keys_tmp);
        return -1;
      }
      memcpy(keys_out, keys_tmp, keys_out_len);
      memcpy(rend_nonce_out, keys_tmp+keys_out_len, DIGEST_LEN);
      memwipe(keys_tmp, 0, keys_tmp_len);
      tor_free(keys_tmp);
      r = NTOR_REPLY_LEN;
    }
    break;
  default:
    log_warn(LD_BUG, "called with unknown handshake state type %d", type);
    tor_fragile_assert();
    return -1;
  }

  return r;
}

/** Perform the final (client-side) step of a circuit-creation handshake of
 * type <b>type</b>, using our state in <b>handshake_state</b> and the
 * server's response in <b>reply</b>. On success, generate <b>keys_out_len</b>
 * bytes worth of key material in <b>keys_out_len</b>, set
 * <b>rend_authenticator_out</b> to the "KH" field that can be used to
 * establish introduction points at this hop, and return 0. On failure,
 * return -1, and set *msg_out to an error message if this is worth
 * complaining to the usre about. */
int
onion_skin_client_handshake(int type,
                      const onion_handshake_state_t *handshake_state,
                      const uint8_t *reply, size_t reply_len,
                      uint8_t *keys_out, size_t keys_out_len,
                      uint8_t *rend_authenticator_out,
                      const char **msg_out)
{
  if (handshake_state->tag != type)
    return -1;

  switch (type) {
  case ONION_HANDSHAKE_TYPE_TAP:
    if (reply_len != TAP_ONIONSKIN_REPLY_LEN) {
      if (msg_out)
        *msg_out = "TAP reply was not of the correct length.";
      return -1;
    }
    if (onion_skin_TAP_client_handshake(handshake_state->u.tap,
                                        (const char*)reply,
                                        (char *)keys_out, keys_out_len,
                                        msg_out) < 0)
      return -1;

    memcpy(rend_authenticator_out, reply+DH_KEY_LEN, DIGEST_LEN);

    return 0;
  case ONION_HANDSHAKE_TYPE_FAST:
    if (reply_len != CREATED_FAST_LEN) {
      if (msg_out)
        *msg_out = "TAP reply was not of the correct length.";
      return -1;
    }
    if (fast_client_handshake(handshake_state->u.fast, reply,
                              keys_out, keys_out_len, msg_out) < 0)
      return -1;

    memcpy(rend_authenticator_out, reply+DIGEST_LEN, DIGEST_LEN);
    return 0;
  case ONION_HANDSHAKE_TYPE_NTOR:
    if (reply_len < NTOR_REPLY_LEN) {
      if (msg_out)
        *msg_out = "ntor reply was not of the correct length.";
      return -1;
    }
    {
      size_t keys_tmp_len = keys_out_len + DIGEST_LEN;
      uint8_t *keys_tmp = tor_malloc(keys_tmp_len);
      if (onion_skin_ntor_client_handshake(handshake_state->u.ntor,
                                        reply,
                                        keys_tmp, keys_tmp_len, msg_out) < 0) {
        tor_free(keys_tmp);
        return -1;
      }
      memcpy(keys_out, keys_tmp, keys_out_len);
      memcpy(rend_authenticator_out, keys_tmp + keys_out_len, DIGEST_LEN);
      memwipe(keys_tmp, 0, keys_tmp_len);
      tor_free(keys_tmp);
    }
    return 0;
  default:
    log_warn(LD_BUG, "called with unknown handshake state type %d", type);
    tor_fragile_assert();
    return -1;
  }
}

/** Helper: return 0 if <b>cell</b> appears valid, -1 otherwise. If
 * <b>unknown_ok</b> is true, allow cells with handshake types we don't
 * recognize. */
static int
check_create_cell(const create_cell_t *cell, int unknown_ok)
{
  switch (cell->cell_type) {
  case CELL_CREATE:
    if (cell->handshake_type != ONION_HANDSHAKE_TYPE_TAP &&
        cell->handshake_type != ONION_HANDSHAKE_TYPE_NTOR)
      return -1;
    break;
  case CELL_CREATE_FAST:
    if (cell->handshake_type != ONION_HANDSHAKE_TYPE_FAST)
      return -1;
    break;
  case CELL_CREATE2:
    break;
  default:
    return -1;
  }

  switch (cell->handshake_type) {
  case ONION_HANDSHAKE_TYPE_TAP:
    if (cell->handshake_len != TAP_ONIONSKIN_CHALLENGE_LEN)
      return -1;
    break;
  case ONION_HANDSHAKE_TYPE_FAST:
    if (cell->handshake_len != CREATE_FAST_LEN)
      return -1;
    break;
  case ONION_HANDSHAKE_TYPE_NTOR:
    if (cell->handshake_len != NTOR_ONIONSKIN_LEN)
      return -1;
    break;
  default:
    if (! unknown_ok)
      return -1;
  }

  return 0;
}

/** Write the various parameters into the create cell. Separate from
 * create_cell_parse() to make unit testing easier.
 */
void
create_cell_init(create_cell_t *cell_out, uint8_t cell_type,
                 uint16_t handshake_type, uint16_t handshake_len,
                 const uint8_t *onionskin)
{
  memset(cell_out, 0, sizeof(*cell_out));

  cell_out->cell_type = cell_type;
  cell_out->handshake_type = handshake_type;
  cell_out->handshake_len = handshake_len;
  memcpy(cell_out->onionskin, onionskin, handshake_len);
}

/** Helper: parse the CREATE2 payload at <b>p</b>, which could be up to
 * <b>p_len</b> bytes long, and use it to fill the fields of
 * <b>cell_out</b>. Return 0 on success and -1 on failure.
 *
 * Note that part of the body of an EXTEND2 cell is a CREATE2 payload, so
 * this function is also used for parsing those.
 */
static int
parse_create2_payload(create_cell_t *cell_out, const uint8_t *p, size_t p_len)
{
  uint16_t handshake_type, handshake_len;

  if (p_len < 4)
    return -1;

  handshake_type = ntohs(get_uint16(p));
  handshake_len = ntohs(get_uint16(p+2));

  if (handshake_len > CELL_PAYLOAD_SIZE - 4 || handshake_len > p_len - 4)
    return -1;
  if (handshake_type == ONION_HANDSHAKE_TYPE_FAST)
    return -1;

  create_cell_init(cell_out, CELL_CREATE2, handshake_type, handshake_len,
                   p+4);
  return 0;
}

/** Magic string which, in a CREATE or EXTEND cell, indicates that a seeming
 * TAP payload is really an ntor payload.  We'd do away with this if every
 * relay supported EXTEND2, but we want to be able to extend from A to B with
 * ntor even when A doesn't understand EXTEND2 and so can't generate a
 * CREATE2 cell.
 **/
#define NTOR_CREATE_MAGIC "ntorNTORntorNTOR"

/** Parse a CREATE, CREATE_FAST, or CREATE2 cell from <b>cell_in</b> into
 * <b>cell_out</b>. Return 0 on success, -1 on failure. (We reject some
 * syntactically valid CREATE2 cells that we can't generate or react to.) */
int
create_cell_parse(create_cell_t *cell_out, const cell_t *cell_in)
{
  switch (cell_in->command) {
  case CELL_CREATE:
    if (tor_memeq(cell_in->payload, NTOR_CREATE_MAGIC, 16)) {
      create_cell_init(cell_out, CELL_CREATE, ONION_HANDSHAKE_TYPE_NTOR,
                       NTOR_ONIONSKIN_LEN, cell_in->payload+16);
    } else {
      create_cell_init(cell_out, CELL_CREATE, ONION_HANDSHAKE_TYPE_TAP,
                       TAP_ONIONSKIN_CHALLENGE_LEN, cell_in->payload);
    }
    break;
  case CELL_CREATE_FAST:
    create_cell_init(cell_out, CELL_CREATE_FAST, ONION_HANDSHAKE_TYPE_FAST,
                     CREATE_FAST_LEN, cell_in->payload);
    break;
  case CELL_CREATE2:
    if (parse_create2_payload(cell_out, cell_in->payload,
                              CELL_PAYLOAD_SIZE) < 0)
      return -1;
    break;
  default:
    return -1;
  }

  return check_create_cell(cell_out, 0);
}

/** Helper: return 0 if <b>cell</b> appears valid, -1 otherwise. */
static int
check_created_cell(const created_cell_t *cell)
{
  switch (cell->cell_type) {
  case CELL_CREATED:
    if (cell->handshake_len != TAP_ONIONSKIN_REPLY_LEN &&
        cell->handshake_len != NTOR_REPLY_LEN)
      return -1;
    break;
  case CELL_CREATED_FAST:
    if (cell->handshake_len != CREATED_FAST_LEN)
      return -1;
    break;
  case CELL_CREATED2:
    if (cell->handshake_len > RELAY_PAYLOAD_SIZE-2)
      return -1;
    break;
  }

  return 0;
}

/** Parse a CREATED, CREATED_FAST, or CREATED2 cell from <b>cell_in</b> into
 * <b>cell_out</b>. Return 0 on success, -1 on failure. */
int
created_cell_parse(created_cell_t *cell_out, const cell_t *cell_in)
{
  memset(cell_out, 0, sizeof(*cell_out));

  switch (cell_in->command) {
  case CELL_CREATED:
    cell_out->cell_type = CELL_CREATED;
    cell_out->handshake_len = TAP_ONIONSKIN_REPLY_LEN;
    memcpy(cell_out->reply, cell_in->payload, TAP_ONIONSKIN_REPLY_LEN);
    break;
  case CELL_CREATED_FAST:
    cell_out->cell_type = CELL_CREATED_FAST;
    cell_out->handshake_len = CREATED_FAST_LEN;
    memcpy(cell_out->reply, cell_in->payload, CREATED_FAST_LEN);
    break;
  case CELL_CREATED2:
    {
      const uint8_t *p = cell_in->payload;
      cell_out->cell_type = CELL_CREATED2;
      cell_out->handshake_len = ntohs(get_uint16(p));
      if (cell_out->handshake_len > CELL_PAYLOAD_SIZE - 2)
        return -1;
      memcpy(cell_out->reply, p+2, cell_out->handshake_len);
      break;
    }
  }

  return check_created_cell(cell_out);
}

/** Helper: return 0 if <b>cell</b> appears valid, -1 otherwise. */
static int
check_extend_cell(const extend_cell_t *cell)
{
  if (tor_digest_is_zero((const char*)cell->node_id))
    return -1;
  /* We don't currently allow EXTEND2 cells without an IPv4 address */
  if (tor_addr_family(&cell->orport_ipv4.addr) == AF_UNSPEC)
    return -1;
  if (cell->create_cell.cell_type == CELL_CREATE) {
    if (cell->cell_type != RELAY_COMMAND_EXTEND)
      return -1;
  } else if (cell->create_cell.cell_type == CELL_CREATE2) {
    if (cell->cell_type != RELAY_COMMAND_EXTEND2 &&
        cell->cell_type != RELAY_COMMAND_EXTEND)
      return -1;
  } else {
    /* In particular, no CREATE_FAST cells are allowed */
    return -1;
  }
  if (cell->create_cell.handshake_type == ONION_HANDSHAKE_TYPE_FAST)
    return -1;

  return check_create_cell(&cell->create_cell, 1);
}

/** Protocol constants for specifier types in EXTEND2
 * @{
 */
#define SPECTYPE_IPV4 0
#define SPECTYPE_IPV6 1
#define SPECTYPE_LEGACY_ID 2
/** @} */

/** Parse an EXTEND or EXTEND2 cell (according to <b>command</b>) from the
 * <b>payload_length</b> bytes of <b>payload</b> into <b>cell_out</b>. Return
 * 0 on success, -1 on failure. */
int
extend_cell_parse(extend_cell_t *cell_out, const uint8_t command,
                  const uint8_t *payload, size_t payload_length)
{
  const uint8_t *eop;

  memset(cell_out, 0, sizeof(*cell_out));
  if (payload_length > RELAY_PAYLOAD_SIZE)
    return -1;
  eop = payload + payload_length;

  switch (command) {
  case RELAY_COMMAND_EXTEND:
    {
      if (payload_length != 6 + TAP_ONIONSKIN_CHALLENGE_LEN + DIGEST_LEN)
        return -1;

      cell_out->cell_type = RELAY_COMMAND_EXTEND;
      tor_addr_from_ipv4n(&cell_out->orport_ipv4.addr, get_uint32(payload));
      cell_out->orport_ipv4.port = ntohs(get_uint16(payload+4));
      tor_addr_make_unspec(&cell_out->orport_ipv6.addr);
      if (tor_memeq(payload + 6, NTOR_CREATE_MAGIC, 16)) {
        cell_out->create_cell.cell_type = CELL_CREATE2;
        cell_out->create_cell.handshake_type = ONION_HANDSHAKE_TYPE_NTOR;
        cell_out->create_cell.handshake_len = NTOR_ONIONSKIN_LEN;
        memcpy(cell_out->create_cell.onionskin, payload + 22,
               NTOR_ONIONSKIN_LEN);
      } else {
        cell_out->create_cell.cell_type = CELL_CREATE;
        cell_out->create_cell.handshake_type = ONION_HANDSHAKE_TYPE_TAP;
        cell_out->create_cell.handshake_len = TAP_ONIONSKIN_CHALLENGE_LEN;
        memcpy(cell_out->create_cell.onionskin, payload + 6,
               TAP_ONIONSKIN_CHALLENGE_LEN);
      }
      memcpy(cell_out->node_id, payload + 6 + TAP_ONIONSKIN_CHALLENGE_LEN,
             DIGEST_LEN);
      break;
    }
  case RELAY_COMMAND_EXTEND2:
    {
      uint8_t n_specs, spectype, speclen;
      int i;
      int found_ipv4 = 0, found_ipv6 = 0, found_id = 0;
      tor_addr_make_unspec(&cell_out->orport_ipv4.addr);
      tor_addr_make_unspec(&cell_out->orport_ipv6.addr);

      if (payload_length == 0)
        return -1;

      cell_out->cell_type = RELAY_COMMAND_EXTEND2;
      n_specs = *payload++;
      /* Parse the specifiers. We'll only take the first IPv4 and first IPv6
       * address, and the node ID, and ignore everything else */
      for (i = 0; i < n_specs; ++i) {
        if (eop - payload < 2)
          return -1;
        spectype = payload[0];
        speclen = payload[1];
        payload += 2;
        if (eop - payload < speclen)
          return -1;
        switch (spectype) {
        case SPECTYPE_IPV4:
          if (speclen != 6)
            return -1;
          if (!found_ipv4) {
            tor_addr_from_ipv4n(&cell_out->orport_ipv4.addr,
                                get_uint32(payload));
            cell_out->orport_ipv4.port = ntohs(get_uint16(payload+4));
            found_ipv4 = 1;
          }
          break;
        case SPECTYPE_IPV6:
          if (speclen != 18)
            return -1;
          if (!found_ipv6) {
            tor_addr_from_ipv6_bytes(&cell_out->orport_ipv6.addr,
                                     (const char*)payload);
            cell_out->orport_ipv6.port = ntohs(get_uint16(payload+16));
            found_ipv6 = 1;
          }
          break;
        case SPECTYPE_LEGACY_ID:
          if (speclen != 20)
            return -1;
          if (found_id)
            return -1;
          memcpy(cell_out->node_id, payload, 20);
          found_id = 1;
          break;
        }
        payload += speclen;
      }
      if (!found_id || !found_ipv4)
        return -1;
      if (parse_create2_payload(&cell_out->create_cell,payload,eop-payload)<0)
        return -1;
      break;
    }
  default:
    return -1;
  }

  return check_extend_cell(cell_out);
}

/** Helper: return 0 if <b>cell</b> appears valid, -1 otherwise. */
static int
check_extended_cell(const extended_cell_t *cell)
{
  if (cell->created_cell.cell_type == CELL_CREATED) {
    if (cell->cell_type != RELAY_COMMAND_EXTENDED)
      return -1;
  } else if (cell->created_cell.cell_type == CELL_CREATED2) {
    if (cell->cell_type != RELAY_COMMAND_EXTENDED2)
      return -1;
  } else {
    return -1;
  }

  return check_created_cell(&cell->created_cell);
}

/** Parse an EXTENDED or EXTENDED2 cell (according to <b>command</b>) from the
 * <b>payload_length</b> bytes of <b>payload</b> into <b>cell_out</b>. Return
 * 0 on success, -1 on failure. */
int
extended_cell_parse(extended_cell_t *cell_out,
                    const uint8_t command, const uint8_t *payload,
                    size_t payload_len)
{
  memset(cell_out, 0, sizeof(*cell_out));
  if (payload_len > RELAY_PAYLOAD_SIZE)
    return -1;

  switch (command) {
  case RELAY_COMMAND_EXTENDED:
    if (payload_len != TAP_ONIONSKIN_REPLY_LEN)
      return -1;
    cell_out->cell_type = RELAY_COMMAND_EXTENDED;
    cell_out->created_cell.cell_type = CELL_CREATED;
    cell_out->created_cell.handshake_len = TAP_ONIONSKIN_REPLY_LEN;
    memcpy(cell_out->created_cell.reply, payload, TAP_ONIONSKIN_REPLY_LEN);
    break;
  case RELAY_COMMAND_EXTENDED2:
    {
      cell_out->cell_type = RELAY_COMMAND_EXTENDED2;
      cell_out->created_cell.cell_type = CELL_CREATED2;
      cell_out->created_cell.handshake_len = ntohs(get_uint16(payload));
      if (cell_out->created_cell.handshake_len > RELAY_PAYLOAD_SIZE - 2 ||
          cell_out->created_cell.handshake_len > payload_len - 2)
        return -1;
      memcpy(cell_out->created_cell.reply, payload+2,
             cell_out->created_cell.handshake_len);
    }
    break;
  default:
    return -1;
  }

  return check_extended_cell(cell_out);
}

/** Fill <b>cell_out</b> with a correctly formatted version of the
 * CREATE{,_FAST,2} cell in <b>cell_in</b>. Return 0 on success, -1 on
 * failure.  This is a cell we didn't originate if <b>relayed</b> is true. */
static int
create_cell_format_impl(cell_t *cell_out, const create_cell_t *cell_in,
                        int relayed)
{
  uint8_t *p;
  size_t space;
  if (check_create_cell(cell_in, relayed) < 0)
    return -1;

  memset(cell_out->payload, 0, sizeof(cell_out->payload));
  cell_out->command = cell_in->cell_type;

  p = cell_out->payload;
  space = sizeof(cell_out->payload);

  switch (cell_in->cell_type) {
  case CELL_CREATE:
    if (cell_in->handshake_type == ONION_HANDSHAKE_TYPE_NTOR) {
      memcpy(p, NTOR_CREATE_MAGIC, 16);
      p += 16;
      space -= 16;
    }
    /* Fall through */
  case CELL_CREATE_FAST:
    tor_assert(cell_in->handshake_len <= space);
    memcpy(p, cell_in->onionskin, cell_in->handshake_len);
    break;
  case CELL_CREATE2:
    tor_assert(cell_in->handshake_len <= sizeof(cell_out->payload)-4);
    set_uint16(cell_out->payload, htons(cell_in->handshake_type));
    set_uint16(cell_out->payload+2, htons(cell_in->handshake_len));
    memcpy(cell_out->payload + 4, cell_in->onionskin, cell_in->handshake_len);
    break;
  default:
    return -1;
  }

  return 0;
}

int
create_cell_format(cell_t *cell_out, const create_cell_t *cell_in)
{
  return create_cell_format_impl(cell_out, cell_in, 0);
}

int
create_cell_format_relayed(cell_t *cell_out, const create_cell_t *cell_in)
{
  return create_cell_format_impl(cell_out, cell_in, 1);
}

/** Fill <b>cell_out</b> with a correctly formatted version of the
 * CREATED{,_FAST,2} cell in <b>cell_in</b>. Return 0 on success, -1 on
 * failure. */
int
created_cell_format(cell_t *cell_out, const created_cell_t *cell_in)
{
  if (check_created_cell(cell_in) < 0)
    return -1;

  memset(cell_out->payload, 0, sizeof(cell_out->payload));
  cell_out->command = cell_in->cell_type;

  switch (cell_in->cell_type) {
  case CELL_CREATED:
  case CELL_CREATED_FAST:
    tor_assert(cell_in->handshake_len <= sizeof(cell_out->payload));
    memcpy(cell_out->payload, cell_in->reply, cell_in->handshake_len);
    break;
  case CELL_CREATED2:
    tor_assert(cell_in->handshake_len <= sizeof(cell_out->payload)-2);
    set_uint16(cell_out->payload, htons(cell_in->handshake_len));
    memcpy(cell_out->payload + 2, cell_in->reply, cell_in->handshake_len);
    break;
  default:
    return -1;
  }
  return 0;
}

/** Format the EXTEND{,2} cell in <b>cell_in</b>, storing its relay payload in
 * <b>payload_out</b>, the number of bytes used in *<b>len_out</b>, and the
 * relay command in *<b>command_out</b>. The <b>payload_out</b> must have
 * RELAY_PAYLOAD_SIZE bytes available.  Return 0 on success, -1 on failure. */
int
extend_cell_format(uint8_t *command_out, uint16_t *len_out,
                   uint8_t *payload_out, const extend_cell_t *cell_in)
{
  uint8_t *p, *eop;
  if (check_extend_cell(cell_in) < 0)
    return -1;

  p = payload_out;
  eop = payload_out + RELAY_PAYLOAD_SIZE;

  memset(p, 0, RELAY_PAYLOAD_SIZE);

  switch (cell_in->cell_type) {
  case RELAY_COMMAND_EXTEND:
    {
      *command_out = RELAY_COMMAND_EXTEND;
      *len_out = 6 + TAP_ONIONSKIN_CHALLENGE_LEN + DIGEST_LEN;
      set_uint32(p, tor_addr_to_ipv4n(&cell_in->orport_ipv4.addr));
      set_uint16(p+4, ntohs(cell_in->orport_ipv4.port));
      if (cell_in->create_cell.handshake_type == ONION_HANDSHAKE_TYPE_NTOR) {
        memcpy(p+6, NTOR_CREATE_MAGIC, 16);
        memcpy(p+22, cell_in->create_cell.onionskin, NTOR_ONIONSKIN_LEN);
      } else {
        memcpy(p+6, cell_in->create_cell.onionskin,
               TAP_ONIONSKIN_CHALLENGE_LEN);
      }
      memcpy(p+6+TAP_ONIONSKIN_CHALLENGE_LEN, cell_in->node_id, DIGEST_LEN);
    }
    break;
  case RELAY_COMMAND_EXTEND2:
    {
      uint8_t n = 2;
      *command_out = RELAY_COMMAND_EXTEND2;

      *p++ = n; /* 2 identifiers */
      *p++ = SPECTYPE_IPV4; /* First is IPV4. */
      *p++ = 6; /* It's 6 bytes long. */
      set_uint32(p, tor_addr_to_ipv4n(&cell_in->orport_ipv4.addr));
      set_uint16(p+4, htons(cell_in->orport_ipv4.port));
      p += 6;
      *p++ = SPECTYPE_LEGACY_ID; /* Next is an identity digest. */
      *p++ = 20; /* It's 20 bytes long */
      memcpy(p, cell_in->node_id, DIGEST_LEN);
      p += 20;

      /* Now we can send the handshake */
      set_uint16(p, htons(cell_in->create_cell.handshake_type));
      set_uint16(p+2, htons(cell_in->create_cell.handshake_len));
      p += 4;

      if (cell_in->create_cell.handshake_len > eop - p)
        return -1;

      memcpy(p, cell_in->create_cell.onionskin,
             cell_in->create_cell.handshake_len);

      p += cell_in->create_cell.handshake_len;
      *len_out = p - payload_out;
    }
    break;
  default:
    return -1;
  }

  return 0;
}

/** Format the EXTENDED{,2} cell in <b>cell_in</b>, storing its relay payload
 * in <b>payload_out</b>, the number of bytes used in *<b>len_out</b>, and the
 * relay command in *<b>command_out</b>. The <b>payload_out</b> must have
 * RELAY_PAYLOAD_SIZE bytes available.  Return 0 on success, -1 on failure. */
int
extended_cell_format(uint8_t *command_out, uint16_t *len_out,
                     uint8_t *payload_out, const extended_cell_t *cell_in)
{
  uint8_t *p;
  if (check_extended_cell(cell_in) < 0)
    return -1;

  p = payload_out;
  memset(p, 0, RELAY_PAYLOAD_SIZE);

  switch (cell_in->cell_type) {
  case RELAY_COMMAND_EXTENDED:
    {
      *command_out = RELAY_COMMAND_EXTENDED;
      *len_out = TAP_ONIONSKIN_REPLY_LEN;
      memcpy(payload_out, cell_in->created_cell.reply,
             TAP_ONIONSKIN_REPLY_LEN);
    }
    break;
  case RELAY_COMMAND_EXTENDED2:
    {
      *command_out = RELAY_COMMAND_EXTENDED2;
      *len_out = 2 + cell_in->created_cell.handshake_len;
      set_uint16(payload_out, htons(cell_in->created_cell.handshake_len));
      if (2+cell_in->created_cell.handshake_len > RELAY_PAYLOAD_SIZE)
        return -1;
      memcpy(payload_out+2, cell_in->created_cell.reply,
             cell_in->created_cell.handshake_len);
    }
    break;
  default:
    return -1;
  }

  return 0;
}

