/* Copyright (c) 2001 Matej Pfajfar.
 * Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file circuitbuild.c
 * \brief The actual details of building circuits.
 **/

#define CIRCUITBUILD_PRIVATE

#include "or.h"
#include "channel.h"
#include "circpathbias.h"
#include "circuitbuild.h"
#include "circuitlist.h"
#include "circuitstats.h"
#include "circuituse.h"
#include "command.h"
#include "config.h"
#include "confparse.h"
#include "connection.h"
#include "connection_edge.h"
#include "connection_or.h"
#include "control.h"
#include "directory.h"
#include "entrynodes.h"
#include "main.h"
#include "microdesc.h"
#include "networkstatus.h"
#include "nodelist.h"
#include "onion.h"
#include "onion_tap.h"
#include "onion_fast.h"
#include "policies.h"
#include "transports.h"
#include "relay.h"
#include "rephist.h"
#include "router.h"
#include "routerlist.h"
#include "routerparse.h"
#include "routerset.h"
#include "crypto.h"

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif

static channel_t * channel_connect_for_circuit(const tor_addr_t *addr,
                                               uint16_t port,
                                               const char *id_digest);
static int circuit_deliver_create_cell(circuit_t *circ,
                                       const create_cell_t *create_cell,
                                       int relayed);
static int onion_pick_cpath_exit(origin_circuit_t *circ, extend_info_t *exit);
static crypt_path_t *onion_next_hop_in_cpath(crypt_path_t *cpath);
static int onion_extend_cpath(origin_circuit_t *circ);
static int count_acceptable_nodes(smartlist_t *routers);
static int onion_append_hop(crypt_path_t **head_ptr, extend_info_t *choice);
#ifdef CURVE25519_ENABLED
static int circuits_can_use_ntor(void);
#endif

/** This function tries to get a channel to the specified endpoint,
 * and then calls command_setup_channel() to give it the right
 * callbacks.
 */
static channel_t *
channel_connect_for_circuit(const tor_addr_t *addr, uint16_t port,
                            const char *id_digest)
{
  channel_t *chan;

  chan = channel_connect(addr, port, id_digest);
  if (chan) command_setup_channel(chan);

  return chan;
}

/** Search for a value for circ_id that we can use on <b>chan</b> for an
 * outbound circuit, until we get a circ_id that is not in use by any other
 * circuit on that conn.
 *
 * Return it, or 0 if can't get a unique circ_id.
 */
STATIC circid_t
get_unique_circ_id_by_chan(channel_t *chan)
{
/* This number is chosen somewhat arbitrarily; see comment below for more
 * info.  When the space is 80% full, it gives a one-in-a-million failure
 * chance; when the space is 90% full, it gives a one-in-850 chance; and when
 * the space is 95% full, it gives a one-in-26 failure chance.  That seems
 * okay, though you could make a case IMO for anything between N=32 and
 * N=256. */
#define MAX_CIRCID_ATTEMPTS 64
  int in_use;
  unsigned n_with_circ = 0, n_pending_destroy = 0, n_weird_pending_destroy = 0;
  circid_t test_circ_id;
  circid_t attempts=0;
  circid_t high_bit, max_range, mask;
  int64_t pending_destroy_time_total = 0;
  int64_t pending_destroy_time_max = 0;

  tor_assert(chan);

  if (chan->circ_id_type == CIRC_ID_TYPE_NEITHER) {
    log_warn(LD_BUG,
             "Trying to pick a circuit ID for a connection from "
             "a client with no identity.");
    return 0;
  }
  max_range = (chan->wide_circ_ids) ? (1u<<31) : (1u<<15);
  mask = max_range - 1;
  high_bit = (chan->circ_id_type == CIRC_ID_TYPE_HIGHER) ? max_range : 0;
  do {
    if (++attempts > MAX_CIRCID_ATTEMPTS) {
      /* Make sure we don't loop forever because all circuit IDs are used.
       *
       * Once, we would try until we had tried every possible circuit ID.  But
       * that's quite expensive.  Instead, we try MAX_CIRCID_ATTEMPTS random
       * circuit IDs, and then give up.
       *
       * This potentially causes us to give up early if our circuit ID space
       * is nearly full.  If we have N circuit IDs in use, then we will reject
       * a new circuit with probability (N / max_range) ^ MAX_CIRCID_ATTEMPTS.
       * This means that in practice, a few percent of our circuit ID capacity
       * will go unused.
       *
       * The alternative here, though, is to do a linear search over the
       * whole circuit ID space every time we extend a circuit, which is
       * not so great either.
       */
      int64_t queued_destroys;
      char *m = rate_limit_log(&chan->last_warned_circ_ids_exhausted,
                               approx_time());
      if (m == NULL)
        return 0; /* This message has been rate-limited away. */
      if (n_pending_destroy)
        pending_destroy_time_total /= n_pending_destroy;
      log_warn(LD_CIRC,"No unused circIDs found on channel %s wide "
                 "circID support, with %u inbound and %u outbound circuits. "
                 "Found %u circuit IDs in use by circuits, and %u with "
                 "pending destroy cells. (%u of those were marked bogusly.) "
                 "The ones with pending destroy cells "
                 "have been marked unusable for an average of %ld seconds "
                 "and a maximum of %ld seconds. This channel is %ld seconds "
                 "old. Failing a circuit.%s",
                 chan->wide_circ_ids ? "with" : "without",
                 chan->num_p_circuits, chan->num_n_circuits,
                 n_with_circ, n_pending_destroy, n_weird_pending_destroy,
                 (long)pending_destroy_time_total,
                 (long)pending_destroy_time_max,
                 (long)(approx_time() - chan->timestamp_created),
                 m);
      tor_free(m);

      if (!chan->cmux) {
        /* This warning should be impossible. */
        log_warn(LD_BUG, "  This channel somehow has no cmux on it!");
        return 0;
      }

      /* analysis so far on 12184 suggests that we're running out of circuit
         IDs because it looks like we have too many pending destroy
         cells. Let's see how many we really have pending.
      */
      queued_destroys = circuitmux_count_queued_destroy_cells(chan,
                                                              chan->cmux);

      log_warn(LD_CIRC, "  Circuitmux on this channel has %u circuits, "
               "of which %u are active. It says it has "I64_FORMAT
               " destroy cells queued.",
               circuitmux_num_circuits(chan->cmux),
               circuitmux_num_active_circuits(chan->cmux),
               I64_PRINTF_ARG(queued_destroys));

      /* Change this into "if (1)" in order to get more information about
       * possible failure modes here.  You'll need to know how to use gdb with
       * Tor: this will make Tor exit with an assertion failure if the cmux is
       * corrupt. */
      if (0)
        circuitmux_assert_okay(chan->cmux);

      channel_dump_statistics(chan, LOG_WARN);

      return 0;
    }

    do {
      crypto_rand((char*) &test_circ_id, sizeof(test_circ_id));
      test_circ_id &= mask;
    } while (test_circ_id == 0);

    test_circ_id |= high_bit;

    in_use = circuit_id_in_use_on_channel(test_circ_id, chan);
    if (in_use == 1)
      ++n_with_circ;
    else if (in_use == 2) {
      time_t since_when;
      ++n_pending_destroy;
      since_when =
        circuit_id_when_marked_unusable_on_channel(test_circ_id, chan);
      if (since_when) {
        time_t waiting = approx_time() - since_when;
        pending_destroy_time_total += waiting;
        if (waiting > pending_destroy_time_max)
          pending_destroy_time_max = waiting;
      } else {
        ++n_weird_pending_destroy;
      }
    }
  } while (in_use);
  return test_circ_id;
}

/** If <b>verbose</b> is false, allocate and return a comma-separated list of
 * the currently built elements of <b>circ</b>. If <b>verbose</b> is true, also
 * list information about link status in a more verbose format using spaces.
 * If <b>verbose_names</b> is false, give nicknames for Named routers and hex
 * digests for others; if <b>verbose_names</b> is true, use $DIGEST=Name style
 * names.
 */
static char *
circuit_list_path_impl(origin_circuit_t *circ, int verbose, int verbose_names)
{
  crypt_path_t *hop;
  smartlist_t *elements;
  const char *states[] = {"closed", "waiting for keys", "open"};
  char *s;

  elements = smartlist_new();

  if (verbose) {
    const char *nickname = build_state_get_exit_nickname(circ->build_state);
    smartlist_add_asprintf(elements, "%s%s circ (length %d%s%s):",
                 circ->build_state->is_internal ? "internal" : "exit",
                 circ->build_state->need_uptime ? " (high-uptime)" : "",
                 circ->build_state->desired_path_len,
                 circ->base_.state == CIRCUIT_STATE_OPEN ? "" : ", last hop ",
                 circ->base_.state == CIRCUIT_STATE_OPEN ? "" :
                 (nickname?nickname:"*unnamed*"));
  }

  hop = circ->cpath;
  do {
    char *elt;
    const char *id;
    const node_t *node;
    if (!hop)
      break;
    if (!verbose && hop->state != CPATH_STATE_OPEN)
      break;
    if (!hop->extend_info)
      break;
    id = hop->extend_info->identity_digest;
    if (verbose_names) {
      elt = tor_malloc(MAX_VERBOSE_NICKNAME_LEN+1);
      if ((node = node_get_by_id(id))) {
        node_get_verbose_nickname(node, elt);
      } else if (is_legal_nickname(hop->extend_info->nickname)) {
        elt[0] = '$';
        base16_encode(elt+1, HEX_DIGEST_LEN+1, id, DIGEST_LEN);
        elt[HEX_DIGEST_LEN+1]= '~';
        strlcpy(elt+HEX_DIGEST_LEN+2,
                hop->extend_info->nickname, MAX_NICKNAME_LEN+1);
      } else {
        elt[0] = '$';
        base16_encode(elt+1, HEX_DIGEST_LEN+1, id, DIGEST_LEN);
      }
    } else { /* ! verbose_names */
      node = node_get_by_id(id);
      if (node && node_is_named(node)) {
        elt = tor_strdup(node_get_nickname(node));
      } else {
        elt = tor_malloc(HEX_DIGEST_LEN+2);
        elt[0] = '$';
        base16_encode(elt+1, HEX_DIGEST_LEN+1, id, DIGEST_LEN);
      }
    }
    tor_assert(elt);
    if (verbose) {
      tor_assert(hop->state <= 2);
      smartlist_add_asprintf(elements,"%s(%s)",elt,states[hop->state]);
      tor_free(elt);
    } else {
      smartlist_add(elements, elt);
    }
    hop = hop->next;
  } while (hop != circ->cpath);

  s = smartlist_join_strings(elements, verbose?" ":",", 0, NULL);
  SMARTLIST_FOREACH(elements, char*, cp, tor_free(cp));
  smartlist_free(elements);
  return s;
}

/** If <b>verbose</b> is false, allocate and return a comma-separated
 * list of the currently built elements of <b>circ</b>.  If
 * <b>verbose</b> is true, also list information about link status in
 * a more verbose format using spaces.
 */
char *
circuit_list_path(origin_circuit_t *circ, int verbose)
{
  return circuit_list_path_impl(circ, verbose, 0);
}

/** Allocate and return a comma-separated list of the currently built elements
 * of <b>circ</b>, giving each as a verbose nickname.
 */
char *
circuit_list_path_for_controller(origin_circuit_t *circ)
{
  return circuit_list_path_impl(circ, 0, 1);
}

/** Log, at severity <b>severity</b>, the nicknames of each router in
 * <b>circ</b>'s cpath. Also log the length of the cpath, and the intended
 * exit point.
 */
void
circuit_log_path(int severity, unsigned int domain, origin_circuit_t *circ)
{
  char *s = circuit_list_path(circ,1);
  tor_log(severity,domain,"%s",s);
  tor_free(s);
}

/** Tell the rep(utation)hist(ory) module about the status of the links
 * in <b>circ</b>.  Hops that have become OPEN are marked as successfully
 * extended; the _first_ hop that isn't open (if any) is marked as
 * unable to extend.
 */
/* XXXX Someday we should learn from OR circuits too. */
void
circuit_rep_hist_note_result(origin_circuit_t *circ)
{
  crypt_path_t *hop;
  const char *prev_digest = NULL;
  hop = circ->cpath;
  if (!hop) /* circuit hasn't started building yet. */
    return;
  if (server_mode(get_options())) {
    const routerinfo_t *me = router_get_my_routerinfo();
    if (!me)
      return;
    prev_digest = me->cache_info.identity_digest;
  }
  do {
    const node_t *node = node_get_by_id(hop->extend_info->identity_digest);
    if (node) { /* Why do we check this?  We know the identity. -NM XXXX */
      if (prev_digest) {
        if (hop->state == CPATH_STATE_OPEN)
          rep_hist_note_extend_succeeded(prev_digest, node->identity);
        else {
          rep_hist_note_extend_failed(prev_digest, node->identity);
          break;
        }
      }
      prev_digest = node->identity;
    } else {
      prev_digest = NULL;
    }
    hop=hop->next;
  } while (hop!=circ->cpath);
}

#ifdef CURVE25519_ENABLED
/** Return 1 iff at least one node in circ's cpath supports ntor. */
static int
circuit_cpath_supports_ntor(const origin_circuit_t *circ)
{
  crypt_path_t *head, *cpath;

  cpath = head = circ->cpath;
  do {
    if (cpath->extend_info &&
        !tor_mem_is_zero(
            (const char*)cpath->extend_info->curve25519_onion_key.public_key,
            CURVE25519_PUBKEY_LEN))
      return 1;

    cpath = cpath->next;
  } while (cpath != head);

  return 0;
}
#else
#define circuit_cpath_supports_ntor(circ) 0
#endif

/** Pick all the entries in our cpath. Stop and return 0 when we're
 * happy, or return -1 if an error occurs. */
static int
onion_populate_cpath(origin_circuit_t *circ)
{
  int n_tries = 0;
#ifdef CURVE25519_ENABLED
  const int using_ntor = circuits_can_use_ntor();
#else
  const int using_ntor = 0;
#endif

#define MAX_POPULATE_ATTEMPTS 32

  while (1) {
    int r = onion_extend_cpath(circ);
    if (r < 0) {
      log_info(LD_CIRC,"Generating cpath hop failed.");
      return -1;
    }
    if (r == 1) {
      /* This circuit doesn't need/shouldn't be forced to have an ntor hop */
      if (circ->build_state->desired_path_len <= 1 || ! using_ntor)
        return 0;

      /* This circuit has an ntor hop. great! */
      if (circuit_cpath_supports_ntor(circ))
        return 0;

      /* No node in the circuit supports ntor.  Have we already tried too many
       * times? */
      if (++n_tries >= MAX_POPULATE_ATTEMPTS)
        break;

      /* Clear the path and retry */
      circuit_clear_cpath(circ);
    }
  }
  log_warn(LD_CIRC, "I tried for %d times, but I couldn't build a %d-hop "
           "circuit with at least one node that supports ntor.",
           MAX_POPULATE_ATTEMPTS,
           circ->build_state->desired_path_len);

  return -1;
}

/** Create and return a new origin circuit. Initialize its purpose and
 * build-state based on our arguments.  The <b>flags</b> argument is a
 * bitfield of CIRCLAUNCH_* flags. */
origin_circuit_t *
origin_circuit_init(uint8_t purpose, int flags)
{
  /* sets circ->p_circ_id and circ->p_chan */
  origin_circuit_t *circ = origin_circuit_new();
  circuit_set_state(TO_CIRCUIT(circ), CIRCUIT_STATE_CHAN_WAIT);
  circ->build_state = tor_malloc_zero(sizeof(cpath_build_state_t));
  circ->build_state->onehop_tunnel =
    ((flags & CIRCLAUNCH_ONEHOP_TUNNEL) ? 1 : 0);
  circ->build_state->need_uptime =
    ((flags & CIRCLAUNCH_NEED_UPTIME) ? 1 : 0);
  circ->build_state->need_capacity =
    ((flags & CIRCLAUNCH_NEED_CAPACITY) ? 1 : 0);
  circ->build_state->is_internal =
    ((flags & CIRCLAUNCH_IS_INTERNAL) ? 1 : 0);
  circ->base_.purpose = purpose;
  return circ;
}

/** Build a new circuit for <b>purpose</b>. If <b>exit</b>
 * is defined, then use that as your exit router, else choose a suitable
 * exit node.
 *
 * Also launch a connection to the first OR in the chosen path, if
 * it's not open already.
 */
origin_circuit_t *
circuit_establish_circuit(uint8_t purpose, extend_info_t *exit, int flags)
{
  origin_circuit_t *circ;
  int err_reason = 0;

  circ = origin_circuit_init(purpose, flags);

  if (onion_pick_cpath_exit(circ, exit) < 0 ||
      onion_populate_cpath(circ) < 0) {
    circuit_mark_for_close(TO_CIRCUIT(circ), END_CIRC_REASON_NOPATH);
    return NULL;
  }

  control_event_circuit_status(circ, CIRC_EVENT_LAUNCHED, 0);

  if ((err_reason = circuit_handle_first_hop(circ)) < 0) {
    circuit_mark_for_close(TO_CIRCUIT(circ), -err_reason);
    return NULL;
  }
  return circ;
}

/** Start establishing the first hop of our circuit. Figure out what
 * OR we should connect to, and if necessary start the connection to
 * it. If we're already connected, then send the 'create' cell.
 * Return 0 for ok, -reason if circ should be marked-for-close. */
int
circuit_handle_first_hop(origin_circuit_t *circ)
{
  crypt_path_t *firsthop;
  channel_t *n_chan;
  int err_reason = 0;
  const char *msg = NULL;
  int should_launch = 0;

  firsthop = onion_next_hop_in_cpath(circ->cpath);
  tor_assert(firsthop);
  tor_assert(firsthop->extend_info);

  /* now see if we're already connected to the first OR in 'route' */
  log_debug(LD_CIRC,"Looking for firsthop '%s'",
            fmt_addrport(&firsthop->extend_info->addr,
                         firsthop->extend_info->port));

  n_chan = channel_get_for_extend(firsthop->extend_info->identity_digest,
                                  &firsthop->extend_info->addr,
                                  &msg,
                                  &should_launch);

  if (!n_chan) {
    /* not currently connected in a useful way. */
    log_info(LD_CIRC, "Next router is %s: %s",
             safe_str_client(extend_info_describe(firsthop->extend_info)),
             msg?msg:"???");
    circ->base_.n_hop = extend_info_dup(firsthop->extend_info);

    if (should_launch) {
      if (circ->build_state->onehop_tunnel)
        control_event_bootstrap(BOOTSTRAP_STATUS_CONN_DIR, 0);
      n_chan = channel_connect_for_circuit(
          &firsthop->extend_info->addr,
          firsthop->extend_info->port,
          firsthop->extend_info->identity_digest);
      if (!n_chan) { /* connect failed, forget the whole thing */
        log_info(LD_CIRC,"connect to firsthop failed. Closing.");
        return -END_CIRC_REASON_CONNECTFAILED;
      }
    }

    log_debug(LD_CIRC,"connecting in progress (or finished). Good.");
    /* return success. The onion/circuit/etc will be taken care of
     * automatically (may already have been) whenever n_chan reaches
     * OR_CONN_STATE_OPEN.
     */
    return 0;
  } else { /* it's already open. use it. */
    tor_assert(!circ->base_.n_hop);
    circ->base_.n_chan = n_chan;
    log_debug(LD_CIRC,"Conn open. Delivering first onion skin.");
    if ((err_reason = circuit_send_next_onion_skin(circ)) < 0) {
      log_info(LD_CIRC,"circuit_send_next_onion_skin failed.");
      circ->base_.n_chan = NULL;
      return err_reason;
    }
  }
  return 0;
}

/** Find any circuits that are waiting on <b>or_conn</b> to become
 * open and get them to send their create cells forward.
 *
 * Status is 1 if connect succeeded, or 0 if connect failed.
 */
void
circuit_n_chan_done(channel_t *chan, int status)
{
  smartlist_t *pending_circs;
  int err_reason = 0;

  tor_assert(chan);

  log_debug(LD_CIRC,"chan to %s/%s, status=%d",
            chan->nickname ? chan->nickname : "NULL",
            channel_get_canonical_remote_descr(chan), status);

  pending_circs = smartlist_new();
  circuit_get_all_pending_on_channel(pending_circs, chan);

  SMARTLIST_FOREACH_BEGIN(pending_circs, circuit_t *, circ)
    {
      /* These checks are redundant wrt get_all_pending_on_or_conn, but I'm
       * leaving them in in case it's possible for the status of a circuit to
       * change as we're going down the list. */
      if (circ->marked_for_close || circ->n_chan || !circ->n_hop ||
          circ->state != CIRCUIT_STATE_CHAN_WAIT)
        continue;

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
      if (!status) { /* chan failed; close circ */
        log_info(LD_CIRC,"Channel failed; closing circ.");
        circuit_mark_for_close(circ, END_CIRC_REASON_CHANNEL_CLOSED);
        continue;
      }
      log_debug(LD_CIRC, "Found circ, sending create cell.");
      /* circuit_deliver_create_cell will set n_circ_id and add us to
       * chan_circuid_circuit_map, so we don't need to call
       * set_circid_chan here. */
      circ->n_chan = chan;
      extend_info_free(circ->n_hop);
      circ->n_hop = NULL;

      if (CIRCUIT_IS_ORIGIN(circ)) {
        if ((err_reason =
             circuit_send_next_onion_skin(TO_ORIGIN_CIRCUIT(circ))) < 0) {
          log_info(LD_CIRC,
                   "send_next_onion_skin failed; circuit marked for closing.");
          circuit_mark_for_close(circ, -err_reason);
          continue;
          /* XXX could this be bad, eg if next_onion_skin failed because conn
           *     died? */
        }
      } else {
        /* pull the create cell out of circ->n_chan_create_cell, and send it */
        tor_assert(circ->n_chan_create_cell);
        if (circuit_deliver_create_cell(circ, circ->n_chan_create_cell, 1)<0) {
          circuit_mark_for_close(circ, END_CIRC_REASON_RESOURCELIMIT);
          continue;
        }
        tor_free(circ->n_chan_create_cell);
        circuit_set_state(circ, CIRCUIT_STATE_OPEN);
      }
    }
  SMARTLIST_FOREACH_END(circ);

  smartlist_free(pending_circs);
}

/** Find a new circid that isn't currently in use on the circ->n_chan
 * for the outgoing
 * circuit <b>circ</b>, and deliver the cell <b>create_cell</b> to this
 * circuit.  If <b>relayed</b> is true, this is a create cell somebody
 * gave us via an EXTEND cell, so we shouldn't worry if we don't understand
 * it. Return -1 if we failed to find a suitable circid, else return 0.
 */
static int
circuit_deliver_create_cell(circuit_t *circ, const create_cell_t *create_cell,
                            int relayed)
{
  cell_t cell;
  circid_t id;
  int r;

  tor_assert(circ);
  tor_assert(circ->n_chan);
  tor_assert(create_cell);
  tor_assert(create_cell->cell_type == CELL_CREATE ||
             create_cell->cell_type == CELL_CREATE_FAST ||
             create_cell->cell_type == CELL_CREATE2);

  id = get_unique_circ_id_by_chan(circ->n_chan);
  if (!id) {
    static ratelim_t circid_warning_limit = RATELIM_INIT(9600);
    log_fn_ratelim(&circid_warning_limit, LOG_WARN, LD_CIRC,
                   "failed to get unique circID.");
    goto error;
  }

  memset(&cell, 0, sizeof(cell_t));
  r = relayed ? create_cell_format_relayed(&cell, create_cell)
              : create_cell_format(&cell, create_cell);
  if (r < 0) {
    log_warn(LD_CIRC,"Couldn't format create cell");
    goto error;
  }
  log_debug(LD_CIRC,"Chosen circID %u.", (unsigned)id);
  circuit_set_n_circid_chan(circ, id, circ->n_chan);
  cell.circ_id = circ->n_circ_id;

  append_cell_to_circuit_queue(circ, circ->n_chan, &cell,
                               CELL_DIRECTION_OUT, 0);

  if (CIRCUIT_IS_ORIGIN(circ)) {
    /* Update began timestamp for circuits starting their first hop */
    if (TO_ORIGIN_CIRCUIT(circ)->cpath->state == CPATH_STATE_CLOSED) {
      if (circ->n_chan->state != CHANNEL_STATE_OPEN) {
        log_warn(LD_CIRC,
                 "Got first hop for a circuit without an opened channel. "
                 "State: %s.", channel_state_to_string(circ->n_chan->state));
        tor_fragile_assert();
      }

      tor_gettimeofday(&circ->timestamp_began);
    }

    /* mark it so it gets better rate limiting treatment. */
    channel_timestamp_client(circ->n_chan);
  }

  return 0;
 error:
  circ->n_chan = NULL;
  return -1;
}

/** We've decided to start our reachability testing. If all
 * is set, log this to the user. Return 1 if we did, or 0 if
 * we chose not to log anything. */
int
inform_testing_reachability(void)
{
  char dirbuf[128];
  char *address;
  const routerinfo_t *me = router_get_my_routerinfo();
  if (!me)
    return 0;
  address = tor_dup_ip(me->addr);
  control_event_server_status(LOG_NOTICE,
                              "CHECKING_REACHABILITY ORADDRESS=%s:%d",
                              address, me->or_port);
  if (me->dir_port) {
    tor_snprintf(dirbuf, sizeof(dirbuf), " and DirPort %s:%d",
                 address, me->dir_port);
    control_event_server_status(LOG_NOTICE,
                                "CHECKING_REACHABILITY DIRADDRESS=%s:%d",
                                address, me->dir_port);
  }
  log_notice(LD_OR, "Now checking whether ORPort %s:%d%s %s reachable... "
                         "(this may take up to %d minutes -- look for log "
                         "messages indicating success)",
      address, me->or_port,
      me->dir_port ? dirbuf : "",
      me->dir_port ? "are" : "is",
      TIMEOUT_UNTIL_UNREACHABILITY_COMPLAINT/60);

  tor_free(address);
  return 1;
}

/** Return true iff we should send a create_fast cell to start building a given
 * circuit */
static INLINE int
should_use_create_fast_for_circuit(origin_circuit_t *circ)
{
  const or_options_t *options = get_options();
  tor_assert(circ->cpath);
  tor_assert(circ->cpath->extend_info);

  if (!circ->cpath->extend_info->onion_key)
    return 1; /* our hand is forced: only a create_fast will work. */
  if (public_server_mode(options)) {
    /* We're a server, and we know an onion key. We can choose.
     * Prefer to blend our circuit into the other circuits we are
     * creating on behalf of others. */
    return 0;
  }
  if (options->FastFirstHopPK == -1) {
    /* option is "auto", so look at the consensus. */
    return networkstatus_get_param(NULL, "usecreatefast", 1, 0, 1);
  }

  return options->FastFirstHopPK;
}

/** Return true if <b>circ</b> is the type of circuit we want to count
 * timeouts from. In particular, we want it to have not completed yet
 * (already completing indicates we cannibalized it), and we want it to
 * have exactly three hops.
 */
int
circuit_timeout_want_to_count_circ(origin_circuit_t *circ)
{
  return !circ->has_opened
          && circ->build_state->desired_path_len == DEFAULT_ROUTE_LEN;
}

#ifdef CURVE25519_ENABLED
/** Return true if the ntor handshake is enabled in the configuration, or if
 * it's been set to "auto" in the configuration and it's enabled in the
 * consensus. */
static int
circuits_can_use_ntor(void)
{
  const or_options_t *options = get_options();
  if (options->UseNTorHandshake != -1)
    return options->UseNTorHandshake;
  return networkstatus_get_param(NULL, "UseNTorHandshake", 0, 0, 1);
}
#endif

/** Decide whether to use a TAP or ntor handshake for connecting to <b>ei</b>
 * directly, and set *<b>cell_type_out</b> and *<b>handshake_type_out</b>
 * accordingly. */
static void
circuit_pick_create_handshake(uint8_t *cell_type_out,
                              uint16_t *handshake_type_out,
                              const extend_info_t *ei)
{
#ifdef CURVE25519_ENABLED
  if (!tor_mem_is_zero((const char*)ei->curve25519_onion_key.public_key,
                       CURVE25519_PUBKEY_LEN) &&
      circuits_can_use_ntor()) {
    *cell_type_out = CELL_CREATE2;
    *handshake_type_out = ONION_HANDSHAKE_TYPE_NTOR;
    return;
  }
#else
  (void) ei;
#endif

  *cell_type_out = CELL_CREATE;
  *handshake_type_out = ONION_HANDSHAKE_TYPE_TAP;
}

/** Decide whether to use a TAP or ntor handshake for connecting to <b>ei</b>
 * directly, and set *<b>handshake_type_out</b> accordingly. Decide whether,
 * in extending through <b>node</b> to do so, we should use an EXTEND2 or an
 * EXTEND cell to do so, and set *<b>cell_type_out</b> and
 * *<b>create_cell_type_out</b> accordingly. */
static void
circuit_pick_extend_handshake(uint8_t *cell_type_out,
                              uint8_t *create_cell_type_out,
                              uint16_t *handshake_type_out,
                              const node_t *node_prev,
                              const extend_info_t *ei)
{
  uint8_t t;
  circuit_pick_create_handshake(&t, handshake_type_out, ei);
  /* XXXX024 The check for whether the node has a curve25519 key is a bad
   * proxy for whether it can do extend2 cells; once a version that
   * handles extend2 cells is out, remove it. */
  if (node_prev &&
      *handshake_type_out != ONION_HANDSHAKE_TYPE_TAP &&
      (node_has_curve25519_onion_key(node_prev) ||
       (node_prev->rs && node_prev->rs->version_supports_extend2_cells))) {
    *cell_type_out = RELAY_COMMAND_EXTEND2;
    *create_cell_type_out = CELL_CREATE2;
  } else {
    *cell_type_out = RELAY_COMMAND_EXTEND;
    *create_cell_type_out = CELL_CREATE;
  }
}

/** This is the backbone function for building circuits.
 *
 * If circ's first hop is closed, then we need to build a create
 * cell and send it forward.
 *
 * Otherwise, we need to build a relay extend cell and send it
 * forward.
 *
 * Return -reason if we want to tear down circ, else return 0.
 */
int
circuit_send_next_onion_skin(origin_circuit_t *circ)
{
  crypt_path_t *hop;
  const node_t *node;

  tor_assert(circ);

  if (circ->cpath->state == CPATH_STATE_CLOSED) {
    /* This is the first hop. */
    create_cell_t cc;
    int fast;
    int len;
    log_debug(LD_CIRC,"First skin; sending create cell.");
    memset(&cc, 0, sizeof(cc));
    if (circ->build_state->onehop_tunnel)
      control_event_bootstrap(BOOTSTRAP_STATUS_ONEHOP_CREATE, 0);
    else
      control_event_bootstrap(BOOTSTRAP_STATUS_CIRCUIT_CREATE, 0);

    node = node_get_by_id(circ->base_.n_chan->identity_digest);
    fast = should_use_create_fast_for_circuit(circ);
    if (!fast) {
      /* We are an OR and we know the right onion key: we should
       * send a create cell.
       */
      circuit_pick_create_handshake(&cc.cell_type, &cc.handshake_type,
                                    circ->cpath->extend_info);
      note_request("cell: create", 1);
    } else {
      /* We are not an OR, and we're building the first hop of a circuit to a
       * new OR: we can be speedy and use CREATE_FAST to save an RSA operation
       * and a DH operation. */
      cc.cell_type = CELL_CREATE_FAST;
      cc.handshake_type = ONION_HANDSHAKE_TYPE_FAST;
      note_request("cell: create fast", 1);
    }

    len = onion_skin_create(cc.handshake_type,
                            circ->cpath->extend_info,
                            &circ->cpath->handshake_state,
                            cc.onionskin);
    if (len < 0) {
      log_warn(LD_CIRC,"onion_skin_create (first hop) failed.");
      return - END_CIRC_REASON_INTERNAL;
    }
    cc.handshake_len = len;

    if (circuit_deliver_create_cell(TO_CIRCUIT(circ), &cc, 0) < 0)
      return - END_CIRC_REASON_RESOURCELIMIT;

    circ->cpath->state = CPATH_STATE_AWAITING_KEYS;
    circuit_set_state(TO_CIRCUIT(circ), CIRCUIT_STATE_BUILDING);
    log_info(LD_CIRC,"First hop: finished sending %s cell to '%s'",
             fast ? "CREATE_FAST" : "CREATE",
             node ? node_describe(node) : "<unnamed>");
  } else {
    extend_cell_t ec;
    int len;
    tor_assert(circ->cpath->state == CPATH_STATE_OPEN);
    tor_assert(circ->base_.state == CIRCUIT_STATE_BUILDING);
    log_debug(LD_CIRC,"starting to send subsequent skin.");
    hop = onion_next_hop_in_cpath(circ->cpath);
    memset(&ec, 0, sizeof(ec));
    if (!hop) {
      /* done building the circuit. whew. */
      circuit_set_state(TO_CIRCUIT(circ), CIRCUIT_STATE_OPEN);
      if (circuit_timeout_want_to_count_circ(circ)) {
        struct timeval end;
        long timediff;
        tor_gettimeofday(&end);
        timediff = tv_mdiff(&circ->base_.timestamp_began, &end);

        /*
         * If the circuit build time is much greater than we would have cut
         * it off at, we probably had a suspend event along this codepath,
         * and we should discard the value.
         */
        if (timediff < 0 ||
            timediff > 2*get_circuit_build_close_time_ms()+1000) {
          log_notice(LD_CIRC, "Strange value for circuit build time: %ldmsec. "
                              "Assuming clock jump. Purpose %d (%s)", timediff,
                     circ->base_.purpose,
                     circuit_purpose_to_string(circ->base_.purpose));
        } else if (!circuit_build_times_disabled()) {
          /* Only count circuit times if the network is live */
          if (circuit_build_times_network_check_live(
              get_circuit_build_times())) {
            circuit_build_times_add_time(get_circuit_build_times_mutable(),
                (build_time_t)timediff);
            circuit_build_times_set_timeout(get_circuit_build_times_mutable());
          }

          if (circ->base_.purpose != CIRCUIT_PURPOSE_C_MEASURE_TIMEOUT) {
            circuit_build_times_network_circ_success(
                get_circuit_build_times_mutable());
          }
        }
      }
      log_info(LD_CIRC,"circuit built!");
      circuit_reset_failure_count(0);

      if (circ->build_state->onehop_tunnel || circ->has_opened) {
        control_event_bootstrap(BOOTSTRAP_STATUS_REQUESTING_STATUS, 0);
      }

      pathbias_count_build_success(circ);
      circuit_rep_hist_note_result(circ);
      circuit_has_opened(circ); /* do other actions as necessary */

      if (!can_complete_circuit && !circ->build_state->onehop_tunnel) {
        const or_options_t *options = get_options();
        can_complete_circuit=1;
        /* FFFF Log a count of known routers here */
        log_notice(LD_GENERAL,
            "Tor has successfully opened a circuit. "
            "Looks like client functionality is working.");
        control_event_bootstrap(BOOTSTRAP_STATUS_DONE, 0);
        control_event_client_status(LOG_NOTICE, "CIRCUIT_ESTABLISHED");
        clear_broken_connection_map(1);
        if (server_mode(options) && !check_whether_orport_reachable()) {
          inform_testing_reachability();
          consider_testing_reachability(1, 1);
        }
      }

      /* We're done with measurement circuits here. Just close them */
      if (circ->base_.purpose == CIRCUIT_PURPOSE_C_MEASURE_TIMEOUT) {
        circuit_mark_for_close(TO_CIRCUIT(circ), END_CIRC_REASON_FINISHED);
      }
      return 0;
    }

    if (tor_addr_family(&hop->extend_info->addr) != AF_INET) {
      log_warn(LD_BUG, "Trying to extend to a non-IPv4 address.");
      return - END_CIRC_REASON_INTERNAL;
    }

    {
      const node_t *prev_node;
      prev_node = node_get_by_id(hop->prev->extend_info->identity_digest);
      circuit_pick_extend_handshake(&ec.cell_type,
                                    &ec.create_cell.cell_type,
                                    &ec.create_cell.handshake_type,
                                    prev_node,
                                    hop->extend_info);
    }

    tor_addr_copy(&ec.orport_ipv4.addr, &hop->extend_info->addr);
    ec.orport_ipv4.port = hop->extend_info->port;
    tor_addr_make_unspec(&ec.orport_ipv6.addr);
    memcpy(ec.node_id, hop->extend_info->identity_digest, DIGEST_LEN);

    len = onion_skin_create(ec.create_cell.handshake_type,
                            hop->extend_info,
                            &hop->handshake_state,
                            ec.create_cell.onionskin);
    if (len < 0) {
      log_warn(LD_CIRC,"onion_skin_create failed.");
      return - END_CIRC_REASON_INTERNAL;
    }
    ec.create_cell.handshake_len = len;

    log_info(LD_CIRC,"Sending extend relay cell.");
    note_request("cell: extend", 1);
    {
      uint8_t command = 0;
      uint16_t payload_len=0;
      uint8_t payload[RELAY_PAYLOAD_SIZE];
      if (extend_cell_format(&command, &payload_len, payload, &ec)<0) {
        log_warn(LD_CIRC,"Couldn't format extend cell");
        return -END_CIRC_REASON_INTERNAL;
      }

      /* send it to hop->prev, because it will transfer
       * it to a create cell and then send to hop */
      if (relay_send_command_from_edge(0, TO_CIRCUIT(circ),
                                       command,
                                       (char*)payload, payload_len,
                                       hop->prev) < 0)
        return 0; /* circuit is closed */
    }
    hop->state = CPATH_STATE_AWAITING_KEYS;
  }
  return 0;
}

/** Our clock just jumped by <b>seconds_elapsed</b>. Assume
 * something has also gone wrong with our network: notify the user,
 * and abandon all not-yet-used circuits. */
void
circuit_note_clock_jumped(int seconds_elapsed)
{
  int severity = server_mode(get_options()) ? LOG_WARN : LOG_NOTICE;
  tor_log(severity, LD_GENERAL, "Your system clock just jumped %d seconds %s; "
      "assuming established circuits no longer work.",
      seconds_elapsed >=0 ? seconds_elapsed : -seconds_elapsed,
      seconds_elapsed >=0 ? "forward" : "backward");
  control_event_general_status(LOG_WARN, "CLOCK_JUMPED TIME=%d",
                               seconds_elapsed);
  can_complete_circuit=0; /* so it'll log when it works again */
  control_event_client_status(severity, "CIRCUIT_NOT_ESTABLISHED REASON=%s",
                              "CLOCK_JUMPED");
  circuit_mark_all_unused_circs();
  circuit_mark_all_dirty_circs_as_unusable();
}

/** Take the 'extend' <b>cell</b>, pull out addr/port plus the onion
 * skin and identity digest for the next hop. If we're already connected,
 * pass the onion skin to the next hop using a create cell; otherwise
 * launch a new OR connection, and <b>circ</b> will notice when the
 * connection succeeds or fails.
 *
 * Return -1 if we want to warn and tear down the circuit, else return 0.
 */
int
circuit_extend(cell_t *cell, circuit_t *circ)
{
  channel_t *n_chan;
  relay_header_t rh;
  extend_cell_t ec;
  const char *msg = NULL;
  int should_launch = 0;

  if (circ->n_chan) {
    log_fn(LOG_PROTOCOL_WARN, LD_PROTOCOL,
           "n_chan already set. Bug/attack. Closing.");
    return -1;
  }
  if (circ->n_hop) {
    log_fn(LOG_PROTOCOL_WARN, LD_PROTOCOL,
           "conn to next hop already launched. Bug/attack. Closing.");
    return -1;
  }

  if (!server_mode(get_options())) {
    log_fn(LOG_PROTOCOL_WARN, LD_PROTOCOL,
           "Got an extend cell, but running as a client. Closing.");
    return -1;
  }

  relay_header_unpack(&rh, cell->payload);

  if (extend_cell_parse(&ec, rh.command,
                        cell->payload+RELAY_HEADER_SIZE,
                        rh.length) < 0) {
    log_fn(LOG_PROTOCOL_WARN, LD_PROTOCOL,
           "Can't parse extend cell. Closing circuit.");
    return -1;
  }

  if (!ec.orport_ipv4.port || tor_addr_is_null(&ec.orport_ipv4.addr)) {
    log_fn(LOG_PROTOCOL_WARN, LD_PROTOCOL,
           "Client asked me to extend to zero destination port or addr.");
    return -1;
  }

  if (tor_addr_is_internal(&ec.orport_ipv4.addr, 0) &&
      !get_options()->ExtendAllowPrivateAddresses) {
    log_fn(LOG_PROTOCOL_WARN, LD_PROTOCOL,
           "Client asked me to extend to a private address");
    return -1;
  }

  /* Check if they asked us for 0000..0000. We support using
   * an empty fingerprint for the first hop (e.g. for a bridge relay),
   * but we don't want to let people send us extend cells for empty
   * fingerprints -- a) because it opens the user up to a mitm attack,
   * and b) because it lets an attacker force the relay to hold open a
   * new TLS connection for each extend request. */
  if (tor_digest_is_zero((const char*)ec.node_id)) {
    log_fn(LOG_PROTOCOL_WARN, LD_PROTOCOL,
           "Client asked me to extend without specifying an id_digest.");
    return -1;
  }

  /* Next, check if we're being asked to connect to the hop that the
   * extend cell came from. There isn't any reason for that, and it can
   * assist circular-path attacks. */
  if (tor_memeq(ec.node_id,
                TO_OR_CIRCUIT(circ)->p_chan->identity_digest,
                DIGEST_LEN)) {
    log_fn(LOG_PROTOCOL_WARN, LD_PROTOCOL,
           "Client asked me to extend back to the previous hop.");
    return -1;
  }

  n_chan = channel_get_for_extend((const char*)ec.node_id,
                                  &ec.orport_ipv4.addr,
                                  &msg,
                                  &should_launch);

  if (!n_chan) {
    log_debug(LD_CIRC|LD_OR,"Next router (%s): %s",
              fmt_addrport(&ec.orport_ipv4.addr,ec.orport_ipv4.port),
              msg?msg:"????");

    circ->n_hop = extend_info_new(NULL /*nickname*/,
                                  (const char*)ec.node_id,
                                  NULL /*onion_key*/,
                                  NULL /*curve25519_key*/,
                                  &ec.orport_ipv4.addr,
                                  ec.orport_ipv4.port);

    circ->n_chan_create_cell = tor_memdup(&ec.create_cell,
                                          sizeof(ec.create_cell));

    circuit_set_state(circ, CIRCUIT_STATE_CHAN_WAIT);

    if (should_launch) {
      /* we should try to open a connection */
      n_chan = channel_connect_for_circuit(&ec.orport_ipv4.addr,
                                           ec.orport_ipv4.port,
                                           (const char*)ec.node_id);
      if (!n_chan) {
        log_info(LD_CIRC,"Launching n_chan failed. Closing circuit.");
        circuit_mark_for_close(circ, END_CIRC_REASON_CONNECTFAILED);
        return 0;
      }
      log_debug(LD_CIRC,"connecting in progress (or finished). Good.");
    }
    /* return success. The onion/circuit/etc will be taken care of
     * automatically (may already have been) whenever n_chan reaches
     * OR_CONN_STATE_OPEN.
     */
    return 0;
  }

  tor_assert(!circ->n_hop); /* Connection is already established. */
  circ->n_chan = n_chan;
  log_debug(LD_CIRC,
            "n_chan is %s",
            channel_get_canonical_remote_descr(n_chan));

  if (circuit_deliver_create_cell(circ, &ec.create_cell, 1) < 0)
    return -1;

  return 0;
}

/** Initialize cpath-\>{f|b}_{crypto|digest} from the key material in
 * key_data.  key_data must contain CPATH_KEY_MATERIAL bytes, which are
 * used as follows:
 *   - 20 to initialize f_digest
 *   - 20 to initialize b_digest
 *   - 16 to key f_crypto
 *   - 16 to key b_crypto
 *
 * (If 'reverse' is true, then f_XX and b_XX are swapped.)
 */
int
circuit_init_cpath_crypto(crypt_path_t *cpath, const char *key_data,
                          int reverse)
{
  crypto_digest_t *tmp_digest;
  crypto_cipher_t *tmp_crypto;

  tor_assert(cpath);
  tor_assert(key_data);
  tor_assert(!(cpath->f_crypto || cpath->b_crypto ||
             cpath->f_digest || cpath->b_digest));

  cpath->f_digest = crypto_digest_new();
  crypto_digest_add_bytes(cpath->f_digest, key_data, DIGEST_LEN);
  cpath->b_digest = crypto_digest_new();
  crypto_digest_add_bytes(cpath->b_digest, key_data+DIGEST_LEN, DIGEST_LEN);

  if (!(cpath->f_crypto =
        crypto_cipher_new(key_data+(2*DIGEST_LEN)))) {
    log_warn(LD_BUG,"Forward cipher initialization failed.");
    return -1;
  }
  if (!(cpath->b_crypto =
        crypto_cipher_new(key_data+(2*DIGEST_LEN)+CIPHER_KEY_LEN))) {
    log_warn(LD_BUG,"Backward cipher initialization failed.");
    return -1;
  }

  if (reverse) {
    tmp_digest = cpath->f_digest;
    cpath->f_digest = cpath->b_digest;
    cpath->b_digest = tmp_digest;
    tmp_crypto = cpath->f_crypto;
    cpath->f_crypto = cpath->b_crypto;
    cpath->b_crypto = tmp_crypto;
  }

  return 0;
}

/** A "created" cell <b>reply</b> came back to us on circuit <b>circ</b>.
 * (The body of <b>reply</b> varies depending on what sort of handshake
 * this is.)
 *
 * Calculate the appropriate keys and digests, make sure KH is
 * correct, and initialize this hop of the cpath.
 *
 * Return - reason if we want to mark circ for close, else return 0.
 */
int
circuit_finish_handshake(origin_circuit_t *circ,
                         const created_cell_t *reply)
{
  char keys[CPATH_KEY_MATERIAL_LEN];
  crypt_path_t *hop;
  int rv;

  if ((rv = pathbias_count_build_attempt(circ)) < 0)
    return rv;

  if (circ->cpath->state == CPATH_STATE_AWAITING_KEYS) {
    hop = circ->cpath;
  } else {
    hop = onion_next_hop_in_cpath(circ->cpath);
    if (!hop) { /* got an extended when we're all done? */
      log_warn(LD_PROTOCOL,"got extended when circ already built? Closing.");
      return - END_CIRC_REASON_TORPROTOCOL;
    }
  }
  tor_assert(hop->state == CPATH_STATE_AWAITING_KEYS);

  {
    if (onion_skin_client_handshake(hop->handshake_state.tag,
                                    &hop->handshake_state,
                                    reply->reply, reply->handshake_len,
                                    (uint8_t*)keys, sizeof(keys),
                                    (uint8_t*)hop->rend_circ_nonce) < 0) {
      log_warn(LD_CIRC,"onion_skin_client_handshake failed.");
      return -END_CIRC_REASON_TORPROTOCOL;
    }
  }

  onion_handshake_state_release(&hop->handshake_state);

  if (circuit_init_cpath_crypto(hop, keys, 0)<0) {
    return -END_CIRC_REASON_TORPROTOCOL;
  }

  hop->state = CPATH_STATE_OPEN;
  log_info(LD_CIRC,"Finished building circuit hop:");
  circuit_log_path(LOG_INFO,LD_CIRC,circ);
  control_event_circuit_status(circ, CIRC_EVENT_EXTENDED, 0);

  return 0;
}

/** We received a relay truncated cell on circ.
 *
 * Since we don't send truncates currently, getting a truncated
 * means that a connection broke or an extend failed. For now,
 * just give up: force circ to close, and return 0.
 */
int
circuit_truncated(origin_circuit_t *circ, crypt_path_t *layer, int reason)
{
//  crypt_path_t *victim;
//  connection_t *stream;

  tor_assert(circ);
  tor_assert(layer);

  /* XXX Since we don't send truncates currently, getting a truncated
   *     means that a connection broke or an extend failed. For now,
   *     just give up.
   */
  circuit_mark_for_close(TO_CIRCUIT(circ),
          END_CIRC_REASON_FLAG_REMOTE|reason);
  return 0;

#if 0
  while (layer->next != circ->cpath) {
    /* we need to clear out layer->next */
    victim = layer->next;
    log_debug(LD_CIRC, "Killing a layer of the cpath.");

    for (stream = circ->p_streams; stream; stream=stream->next_stream) {
      if (stream->cpath_layer == victim) {
        log_info(LD_APP, "Marking stream %d for close because of truncate.",
                 stream->stream_id);
        /* no need to send 'end' relay cells,
         * because the other side's already dead
         */
        connection_mark_unattached_ap(stream, END_STREAM_REASON_DESTROY);
      }
    }

    layer->next = victim->next;
    circuit_free_cpath_node(victim);
  }

  log_info(LD_CIRC, "finished");
  return 0;
#endif
}

/** Given a response payload and keys, initialize, then send a created
 * cell back.
 */
int
onionskin_answer(or_circuit_t *circ,
                 const created_cell_t *created_cell,
                 const char *keys,
                 const uint8_t *rend_circ_nonce)
{
  cell_t cell;
  crypt_path_t *tmp_cpath;

  if (created_cell_format(&cell, created_cell) < 0) {
    log_warn(LD_BUG,"couldn't format created cell (type=%d, len=%d)",
             (int)created_cell->cell_type, (int)created_cell->handshake_len);
    return -1;
  }
  cell.circ_id = circ->p_circ_id;

  tmp_cpath = tor_malloc_zero(sizeof(crypt_path_t));
  tmp_cpath->magic = CRYPT_PATH_MAGIC;

  circuit_set_state(TO_CIRCUIT(circ), CIRCUIT_STATE_OPEN);

  log_debug(LD_CIRC,"init digest forward 0x%.8x, backward 0x%.8x.",
            (unsigned int)get_uint32(keys),
            (unsigned int)get_uint32(keys+20));
  if (circuit_init_cpath_crypto(tmp_cpath, keys, 0)<0) {
    log_warn(LD_BUG,"Circuit initialization failed");
    tor_free(tmp_cpath);
    return -1;
  }
  circ->n_digest = tmp_cpath->f_digest;
  circ->n_crypto = tmp_cpath->f_crypto;
  circ->p_digest = tmp_cpath->b_digest;
  circ->p_crypto = tmp_cpath->b_crypto;
  tmp_cpath->magic = 0;
  tor_free(tmp_cpath);

  memcpy(circ->rend_circ_nonce, rend_circ_nonce, DIGEST_LEN);

  circ->is_first_hop = (created_cell->cell_type == CELL_CREATED_FAST);

  append_cell_to_circuit_queue(TO_CIRCUIT(circ),
                               circ->p_chan, &cell, CELL_DIRECTION_IN, 0);
  log_debug(LD_CIRC,"Finished sending '%s' cell.",
            circ->is_first_hop ? "created_fast" : "created");

  if (!channel_is_local(circ->p_chan) &&
      !channel_is_outgoing(circ->p_chan)) {
    /* record that we could process create cells from a non-local conn
     * that we didn't initiate; presumably this means that create cells
     * can reach us too. */
    router_orport_found_reachable();
  }

  return 0;
}

/** Choose a length for a circuit of purpose <b>purpose</b>: three + the
 * number of endpoints that would give something away about our destination.
 *
 * If the routerlist <b>nodes</b> doesn't have enough routers
 * to handle the desired path length, return -1.
 */
static int
new_route_len(uint8_t purpose, extend_info_t *exit, smartlist_t *nodes)
{
  int num_acceptable_routers;
  int routelen;

  tor_assert(nodes);

  routelen = DEFAULT_ROUTE_LEN;
  if (exit &&
      purpose != CIRCUIT_PURPOSE_TESTING &&
      purpose != CIRCUIT_PURPOSE_S_ESTABLISH_INTRO)
    routelen++;

  num_acceptable_routers = count_acceptable_nodes(nodes);

  log_debug(LD_CIRC,"Chosen route length %d (%d/%d routers suitable).",
            routelen, num_acceptable_routers, smartlist_len(nodes));

  if (num_acceptable_routers < routelen) {
    log_info(LD_CIRC,
             "Not enough acceptable routers (%d/%d). Discarding this circuit.",
             num_acceptable_routers, routelen);
    return -1;
  }

  return routelen;
}

/** Return a newly allocated list of uint16_t * for each predicted port not
 * handled by a current circuit. */
static smartlist_t *
circuit_get_unhandled_ports(time_t now)
{
  smartlist_t *dest = rep_hist_get_predicted_ports(now);
  circuit_remove_handled_ports(dest);
  return dest;
}

/** Return 1 if we already have circuits present or on the way for
 * all anticipated ports. Return 0 if we should make more.
 *
 * If we're returning 0, set need_uptime and need_capacity to
 * indicate any requirements that the unhandled ports have.
 */
int
circuit_all_predicted_ports_handled(time_t now, int *need_uptime,
                                    int *need_capacity)
{
  int i, enough;
  uint16_t *port;
  smartlist_t *sl = circuit_get_unhandled_ports(now);
  smartlist_t *LongLivedServices = get_options()->LongLivedPorts;
  tor_assert(need_uptime);
  tor_assert(need_capacity);
  // Always predict need_capacity
  *need_capacity = 1;
  enough = (smartlist_len(sl) == 0);
  for (i = 0; i < smartlist_len(sl); ++i) {
    port = smartlist_get(sl, i);
    if (smartlist_contains_int_as_string(LongLivedServices, *port))
      *need_uptime = 1;
    tor_free(port);
  }
  smartlist_free(sl);
  return enough;
}

/** Return 1 if <b>node</b> can handle one or more of the ports in
 * <b>needed_ports</b>, else return 0.
 */
static int
node_handles_some_port(const node_t *node, smartlist_t *needed_ports)
{ /* XXXX MOVE */
  int i;
  uint16_t port;

  for (i = 0; i < smartlist_len(needed_ports); ++i) {
    addr_policy_result_t r;
    /* alignment issues aren't a worry for this dereference, since
       needed_ports is explicitly a smartlist of uint16_t's */
    port = *(uint16_t *)smartlist_get(needed_ports, i);
    tor_assert(port);
    if (node)
      r = compare_tor_addr_to_node_policy(NULL, port, node);
    else
      continue;
    if (r != ADDR_POLICY_REJECTED && r != ADDR_POLICY_PROBABLY_REJECTED)
      return 1;
  }
  return 0;
}

/** Return true iff <b>conn</b> needs another general circuit to be
 * built. */
static int
ap_stream_wants_exit_attention(connection_t *conn)
{
  entry_connection_t *entry;
  if (conn->type != CONN_TYPE_AP)
    return 0;
  entry = TO_ENTRY_CONN(conn);

  if (conn->state == AP_CONN_STATE_CIRCUIT_WAIT &&
      !conn->marked_for_close &&
      !(entry->want_onehop) && /* ignore one-hop streams */
      !(entry->use_begindir) && /* ignore targeted dir fetches */
      !(entry->chosen_exit_name) && /* ignore defined streams */
      !connection_edge_is_rendezvous_stream(TO_EDGE_CONN(conn)) &&
      !circuit_stream_is_being_handled(TO_ENTRY_CONN(conn), 0,
                                       MIN_CIRCUITS_HANDLING_STREAM))
    return 1;
  return 0;
}

/** Return a pointer to a suitable router to be the exit node for the
 * general-purpose circuit we're about to build.
 *
 * Look through the connection array, and choose a router that maximizes
 * the number of pending streams that can exit from this router.
 *
 * Return NULL if we can't find any suitable routers.
 */
static const node_t *
choose_good_exit_server_general(int need_uptime, int need_capacity)
{
  int *n_supported;
  int n_pending_connections = 0;
  smartlist_t *connections;
  int best_support = -1;
  int n_best_support=0;
  const or_options_t *options = get_options();
  const smartlist_t *the_nodes;
  const node_t *node=NULL;

  connections = get_connection_array();

  /* Count how many connections are waiting for a circuit to be built.
   * We use this for log messages now, but in the future we may depend on it.
   */
  SMARTLIST_FOREACH(connections, connection_t *, conn,
  {
    if (ap_stream_wants_exit_attention(conn))
      ++n_pending_connections;
  });
//  log_fn(LOG_DEBUG, "Choosing exit node; %d connections are pending",
//         n_pending_connections);
  /* Now we count, for each of the routers in the directory, how many
   * of the pending connections could possibly exit from that
   * router (n_supported[i]). (We can't be sure about cases where we
   * don't know the IP address of the pending connection.)
   *
   * -1 means "Don't use this router at all."
   */
  the_nodes = nodelist_get_list();
  n_supported = tor_malloc(sizeof(int)*smartlist_len(the_nodes));
  SMARTLIST_FOREACH_BEGIN(the_nodes, const node_t *, node) {
    const int i = node_sl_idx;
    if (router_digest_is_me(node->identity)) {
      n_supported[i] = -1;
//      log_fn(LOG_DEBUG,"Skipping node %s -- it's me.", router->nickname);
      /* XXX there's probably a reverse predecessor attack here, but
       * it's slow. should we take this out? -RD
       */
      continue;
    }
    if (!node_has_descriptor(node)) {
      n_supported[i] = -1;
      continue;
    }
    if (!node->is_running || node->is_bad_exit) {
      n_supported[i] = -1;
      continue; /* skip routers that are known to be down or bad exits */
    }
    if (node_get_purpose(node) != ROUTER_PURPOSE_GENERAL) {
      /* never pick a non-general node as a random exit. */
      n_supported[i] = -1;
      continue;
    }
    if (routerset_contains_node(options->ExcludeExitNodesUnion_, node)) {
      n_supported[i] = -1;
      continue; /* user asked us not to use it, no matter what */
    }
    if (options->ExitNodes &&
        !routerset_contains_node(options->ExitNodes, node)) {
      n_supported[i] = -1;
      continue; /* not one of our chosen exit nodes */
    }

    if (node_is_unreliable(node, need_uptime, need_capacity, 0)) {
      n_supported[i] = -1;
      continue; /* skip routers that are not suitable.  Don't worry if
                 * this makes us reject all the possible routers: if so,
                 * we'll retry later in this function with need_update and
                 * need_capacity set to 0. */
    }
    if (!(node->is_valid || options->AllowInvalid_ & ALLOW_INVALID_EXIT)) {
      /* if it's invalid and we don't want it */
      n_supported[i] = -1;
//      log_fn(LOG_DEBUG,"Skipping node %s (index %d) -- invalid router.",
//             router->nickname, i);
      continue; /* skip invalid routers */
    }
    if (options->ExcludeSingleHopRelays &&
        node_allows_single_hop_exits(node)) {
      n_supported[i] = -1;
      continue;
    }
    if (node_exit_policy_rejects_all(node)) {
      n_supported[i] = -1;
//      log_fn(LOG_DEBUG,"Skipping node %s (index %d) -- it rejects all.",
//             router->nickname, i);
      continue; /* skip routers that reject all */
    }
    n_supported[i] = 0;
    /* iterate over connections */
    SMARTLIST_FOREACH_BEGIN(connections, connection_t *, conn) {
      if (!ap_stream_wants_exit_attention(conn))
        continue; /* Skip everything but APs in CIRCUIT_WAIT */
      if (connection_ap_can_use_exit(TO_ENTRY_CONN(conn), node)) {
        ++n_supported[i];
//        log_fn(LOG_DEBUG,"%s is supported. n_supported[%d] now %d.",
//               router->nickname, i, n_supported[i]);
      } else {
//        log_fn(LOG_DEBUG,"%s (index %d) would reject this stream.",
//               router->nickname, i);
      }
    } SMARTLIST_FOREACH_END(conn);
    if (n_pending_connections > 0 && n_supported[i] == 0) {
      /* Leave best_support at -1 if that's where it is, so we can
       * distinguish it later. */
      continue;
    }
    if (n_supported[i] > best_support) {
      /* If this router is better than previous ones, remember its index
       * and goodness, and start counting how many routers are this good. */
      best_support = n_supported[i]; n_best_support=1;
//      log_fn(LOG_DEBUG,"%s is new best supported option so far.",
//             router->nickname);
    } else if (n_supported[i] == best_support) {
      /* If this router is _as good_ as the best one, just increment the
       * count of equally good routers.*/
      ++n_best_support;
    }
  } SMARTLIST_FOREACH_END(node);
  log_info(LD_CIRC,
           "Found %d servers that might support %d/%d pending connections.",
           n_best_support, best_support >= 0 ? best_support : 0,
           n_pending_connections);

  /* If any routers definitely support any pending connections, choose one
   * at random. */
  if (best_support > 0) {
    smartlist_t *supporting = smartlist_new();

    SMARTLIST_FOREACH(the_nodes, const node_t *, node, {
      if (n_supported[node_sl_idx] == best_support)
        smartlist_add(supporting, (void*)node);
    });

    node = node_sl_choose_by_bandwidth(supporting, WEIGHT_FOR_EXIT);
    smartlist_free(supporting);
  } else {
    /* Either there are no pending connections, or no routers even seem to
     * possibly support any of them.  Choose a router at random that satisfies
     * at least one predicted exit port. */

    int attempt;
    smartlist_t *needed_ports, *supporting;

    if (best_support == -1) {
      if (need_uptime || need_capacity) {
        log_info(LD_CIRC,
                 "We couldn't find any live%s%s routers; falling back "
                 "to list of all routers.",
                 need_capacity?", fast":"",
                 need_uptime?", stable":"");
        tor_free(n_supported);
        return choose_good_exit_server_general(0, 0);
      }
      log_notice(LD_CIRC, "All routers are down or won't exit%s -- "
                 "choosing a doomed exit at random.",
                 options->ExcludeExitNodesUnion_ ? " or are Excluded" : "");
    }
    supporting = smartlist_new();
    needed_ports = circuit_get_unhandled_ports(time(NULL));
    for (attempt = 0; attempt < 2; attempt++) {
      /* try once to pick only from routers that satisfy a needed port,
       * then if there are none, pick from any that support exiting. */
      SMARTLIST_FOREACH_BEGIN(the_nodes, const node_t *, node) {
        if (n_supported[node_sl_idx] != -1 &&
            (attempt || node_handles_some_port(node, needed_ports))) {
//          log_fn(LOG_DEBUG,"Try %d: '%s' is a possibility.",
//                 try, router->nickname);
          smartlist_add(supporting, (void*)node);
        }
      } SMARTLIST_FOREACH_END(node);

      node = node_sl_choose_by_bandwidth(supporting, WEIGHT_FOR_EXIT);
      if (node)
        break;
      smartlist_clear(supporting);
      /* If we reach this point, we can't actually support any unhandled
       * predicted ports, so clear all the remaining ones. */
      if (smartlist_len(needed_ports))
        rep_hist_remove_predicted_ports(needed_ports);
    }
    SMARTLIST_FOREACH(needed_ports, uint16_t *, cp, tor_free(cp));
    smartlist_free(needed_ports);
    smartlist_free(supporting);
  }

  tor_free(n_supported);
  if (node) {
    log_info(LD_CIRC, "Chose exit server '%s'", node_describe(node));
    return node;
  }
  if (options->ExitNodes) {
    log_warn(LD_CIRC,
             "No specified %sexit routers seem to be running: "
             "can't choose an exit.",
             options->ExcludeExitNodesUnion_ ? "non-excluded " : "");
  }
  return NULL;
}

/** Return a pointer to a suitable router to be the exit node for the
 * circuit of purpose <b>purpose</b> that we're about to build (or NULL
 * if no router is suitable).
 *
 * For general-purpose circuits, pass it off to
 * choose_good_exit_server_general()
 *
 * For client-side rendezvous circuits, choose a random node, weighted
 * toward the preferences in 'options'.
 */
static const node_t *
choose_good_exit_server(uint8_t purpose,
                        int need_uptime, int need_capacity, int is_internal)
{
  const or_options_t *options = get_options();
  router_crn_flags_t flags = CRN_NEED_DESC;
  if (need_uptime)
    flags |= CRN_NEED_UPTIME;
  if (need_capacity)
    flags |= CRN_NEED_CAPACITY;

  switch (purpose) {
    case CIRCUIT_PURPOSE_C_GENERAL:
      if (options->AllowInvalid_ & ALLOW_INVALID_MIDDLE)
        flags |= CRN_ALLOW_INVALID;
      if (is_internal) /* pick it like a middle hop */
        return router_choose_random_node(NULL, options->ExcludeNodes, flags);
      else
        return choose_good_exit_server_general(need_uptime,need_capacity);
    case CIRCUIT_PURPOSE_C_ESTABLISH_REND:
      if (options->AllowInvalid_ & ALLOW_INVALID_RENDEZVOUS)
        flags |= CRN_ALLOW_INVALID;
      return router_choose_random_node(NULL, options->ExcludeNodes, flags);
  }
  log_warn(LD_BUG,"Unhandled purpose %d", purpose);
  tor_fragile_assert();
  return NULL;
}

/** Log a warning if the user specified an exit for the circuit that
 * has been excluded from use by ExcludeNodes or ExcludeExitNodes. */
static void
warn_if_last_router_excluded(origin_circuit_t *circ, const extend_info_t *exit)
{
  const or_options_t *options = get_options();
  routerset_t *rs = options->ExcludeNodes;
  const char *description;
  uint8_t purpose = circ->base_.purpose;

  if (circ->build_state->onehop_tunnel)
    return;

  switch (purpose)
    {
    default:
    case CIRCUIT_PURPOSE_OR:
    case CIRCUIT_PURPOSE_INTRO_POINT:
    case CIRCUIT_PURPOSE_REND_POINT_WAITING:
    case CIRCUIT_PURPOSE_REND_ESTABLISHED:
      log_warn(LD_BUG, "Called on non-origin circuit (purpose %d, %s)",
               (int)purpose,
               circuit_purpose_to_string(purpose));
      return;
    case CIRCUIT_PURPOSE_C_GENERAL:
      if (circ->build_state->is_internal)
        return;
      description = "requested exit node";
      rs = options->ExcludeExitNodesUnion_;
      break;
    case CIRCUIT_PURPOSE_C_INTRODUCING:
    case CIRCUIT_PURPOSE_C_INTRODUCE_ACK_WAIT:
    case CIRCUIT_PURPOSE_C_INTRODUCE_ACKED:
    case CIRCUIT_PURPOSE_S_ESTABLISH_INTRO:
    case CIRCUIT_PURPOSE_S_CONNECT_REND:
    case CIRCUIT_PURPOSE_S_REND_JOINED:
    case CIRCUIT_PURPOSE_TESTING:
      return;
    case CIRCUIT_PURPOSE_C_ESTABLISH_REND:
    case CIRCUIT_PURPOSE_C_REND_READY:
    case CIRCUIT_PURPOSE_C_REND_READY_INTRO_ACKED:
    case CIRCUIT_PURPOSE_C_REND_JOINED:
      description = "chosen rendezvous point";
      break;
    case CIRCUIT_PURPOSE_CONTROLLER:
      rs = options->ExcludeExitNodesUnion_;
      description = "controller-selected circuit target";
      break;
    }

  if (routerset_contains_extendinfo(rs, exit)) {
    /* We should never get here if StrictNodes is set to 1. */
    if (options->StrictNodes) {
      log_warn(LD_BUG, "Using %s '%s' which is listed in ExcludeNodes%s, "
               "even though StrictNodes is set. Please report. "
               "(Circuit purpose: %s)",
               description, extend_info_describe(exit),
               rs==options->ExcludeNodes?"":" or ExcludeExitNodes",
               circuit_purpose_to_string(purpose));
    } else {
      log_warn(LD_CIRC, "Using %s '%s' which is listed in "
               "ExcludeNodes%s, because no better options were available. To "
               "prevent this (and possibly break your Tor functionality), "
               "set the StrictNodes configuration option. "
               "(Circuit purpose: %s)",
               description, extend_info_describe(exit),
               rs==options->ExcludeNodes?"":" or ExcludeExitNodes",
               circuit_purpose_to_string(purpose));
    }
    circuit_log_path(LOG_WARN, LD_CIRC, circ);
  }

  return;
}

/** Decide a suitable length for circ's cpath, and pick an exit
 * router (or use <b>exit</b> if provided). Store these in the
 * cpath. Return 0 if ok, -1 if circuit should be closed. */
static int
onion_pick_cpath_exit(origin_circuit_t *circ, extend_info_t *exit)
{
  cpath_build_state_t *state = circ->build_state;

  if (state->onehop_tunnel) {
    log_debug(LD_CIRC, "Launching a one-hop circuit for dir tunnel.");
    state->desired_path_len = 1;
  } else {
    int r = new_route_len(circ->base_.purpose, exit, nodelist_get_list());
    if (r < 1) /* must be at least 1 */
      return -1;
    state->desired_path_len = r;
  }

  if (exit) { /* the circuit-builder pre-requested one */
    warn_if_last_router_excluded(circ, exit);
    log_info(LD_CIRC,"Using requested exit node '%s'",
             extend_info_describe(exit));
    exit = extend_info_dup(exit);
  } else { /* we have to decide one */
    const node_t *node =
      choose_good_exit_server(circ->base_.purpose, state->need_uptime,
                              state->need_capacity, state->is_internal);
    if (!node) {
      log_warn(LD_CIRC,"failed to choose an exit server");
      return -1;
    }
    exit = extend_info_from_node(node, 0);
    tor_assert(exit);
  }
  state->chosen_exit = exit;
  return 0;
}

/** Give <b>circ</b> a new exit destination to <b>exit</b>, and add a
 * hop to the cpath reflecting this. Don't send the next extend cell --
 * the caller will do this if it wants to.
 */
int
circuit_append_new_exit(origin_circuit_t *circ, extend_info_t *exit)
{
  cpath_build_state_t *state;
  tor_assert(exit);
  tor_assert(circ);

  state = circ->build_state;
  tor_assert(state);
  extend_info_free(state->chosen_exit);
  state->chosen_exit = extend_info_dup(exit);

  ++circ->build_state->desired_path_len;
  onion_append_hop(&circ->cpath, exit);
  return 0;
}

/** Take an open <b>circ</b>, and add a new hop at the end, based on
 * <b>info</b>. Set its state back to CIRCUIT_STATE_BUILDING, and then
 * send the next extend cell to begin connecting to that hop.
 */
int
circuit_extend_to_new_exit(origin_circuit_t *circ, extend_info_t *exit)
{
  int err_reason = 0;
  warn_if_last_router_excluded(circ, exit);

  tor_gettimeofday(&circ->base_.timestamp_began);

  circuit_append_new_exit(circ, exit);
  circuit_set_state(TO_CIRCUIT(circ), CIRCUIT_STATE_BUILDING);
  if ((err_reason = circuit_send_next_onion_skin(circ))<0) {
    log_warn(LD_CIRC, "Couldn't extend circuit to new point %s.",
             extend_info_describe(exit));
    circuit_mark_for_close(TO_CIRCUIT(circ), -err_reason);
    return -1;
  }

  // XXX: Should cannibalized circuits be dirty or not? Not easy to say..

  return 0;
}

/** Return the number of routers in <b>routers</b> that are currently up
 * and available for building circuits through.
 */
static int
count_acceptable_nodes(smartlist_t *nodes)
{
  int num=0;

  SMARTLIST_FOREACH_BEGIN(nodes, const node_t *, node) {
    //    log_debug(LD_CIRC,
//              "Contemplating whether router %d (%s) is a new option.",
//              i, r->nickname);
    if (! node->is_running)
//      log_debug(LD_CIRC,"Nope, the directory says %d is not running.",i);
      continue;
    if (! node->is_valid)
//      log_debug(LD_CIRC,"Nope, the directory says %d is not valid.",i);
      continue;
    if (! node_has_descriptor(node))
      continue;
      /* XXX This clause makes us count incorrectly: if AllowInvalidRouters
       * allows this node in some places, then we're getting an inaccurate
       * count. For now, be conservative and don't count it. But later we
       * should try to be smarter. */
    ++num;
  } SMARTLIST_FOREACH_END(node);

//    log_debug(LD_CIRC,"I like %d. num_acceptable_routers now %d.",i, num);

  return num;
}

/** Add <b>new_hop</b> to the end of the doubly-linked-list <b>head_ptr</b>.
 * This function is used to extend cpath by another hop.
 */
void
onion_append_to_cpath(crypt_path_t **head_ptr, crypt_path_t *new_hop)
{
  if (*head_ptr) {
    new_hop->next = (*head_ptr);
    new_hop->prev = (*head_ptr)->prev;
    (*head_ptr)->prev->next = new_hop;
    (*head_ptr)->prev = new_hop;
  } else {
    *head_ptr = new_hop;
    new_hop->prev = new_hop->next = new_hop;
  }
}

/** A helper function used by onion_extend_cpath(). Use <b>purpose</b>
 * and <b>state</b> and the cpath <b>head</b> (currently populated only
 * to length <b>cur_len</b> to decide a suitable middle hop for a
 * circuit. In particular, make sure we don't pick the exit node or its
 * family, and make sure we don't duplicate any previous nodes or their
 * families. */
static const node_t *
choose_good_middle_server(uint8_t purpose,
                          cpath_build_state_t *state,
                          crypt_path_t *head,
                          int cur_len)
{
  int i;
  const node_t *r, *choice;
  crypt_path_t *cpath;
  smartlist_t *excluded;
  const or_options_t *options = get_options();
  router_crn_flags_t flags = CRN_NEED_DESC;
  tor_assert(CIRCUIT_PURPOSE_MIN_ <= purpose &&
             purpose <= CIRCUIT_PURPOSE_MAX_);

  log_debug(LD_CIRC, "Contemplating intermediate hop: random choice.");
  excluded = smartlist_new();
  if ((r = build_state_get_exit_node(state))) {
    nodelist_add_node_and_family(excluded, r);
  }
  for (i = 0, cpath = head; i < cur_len; ++i, cpath=cpath->next) {
    if ((r = node_get_by_id(cpath->extend_info->identity_digest))) {
      nodelist_add_node_and_family(excluded, r);
    }
  }

  if (state->need_uptime)
    flags |= CRN_NEED_UPTIME;
  if (state->need_capacity)
    flags |= CRN_NEED_CAPACITY;
  if (options->AllowInvalid_ & ALLOW_INVALID_MIDDLE)
    flags |= CRN_ALLOW_INVALID;
  choice = router_choose_random_node(excluded, options->ExcludeNodes, flags);
  smartlist_free(excluded);
  return choice;
}

/** Pick a good entry server for the circuit to be built according to
 * <b>state</b>.  Don't reuse a chosen exit (if any), don't use this
 * router (if we're an OR), and respect firewall settings; if we're
 * configured to use entry guards, return one.
 *
 * If <b>state</b> is NULL, we're choosing a router to serve as an entry
 * guard, not for any particular circuit.
 */
/* XXXX024 I'd like to have this be static again, but entrynodes.c needs it. */
const node_t *
choose_good_entry_server(uint8_t purpose, cpath_build_state_t *state)
{
  const node_t *choice;
  smartlist_t *excluded;
  const or_options_t *options = get_options();
  router_crn_flags_t flags = CRN_NEED_GUARD|CRN_NEED_DESC;
  const node_t *node;

  if (state && options->UseEntryGuards &&
      (purpose != CIRCUIT_PURPOSE_TESTING || options->BridgeRelay)) {
    /* This request is for an entry server to use for a regular circuit,
     * and we use entry guard nodes.  Just return one of the guard nodes.  */
    return choose_random_entry(state);
  }

  excluded = smartlist_new();

  if (state && (node = build_state_get_exit_node(state))) {
    /* Exclude the exit node from the state, if we have one.  Also exclude its
     * family. */
    nodelist_add_node_and_family(excluded, node);
  }
  if (firewall_is_fascist_or()) {
    /* Exclude all ORs that we can't reach through our firewall */
    smartlist_t *nodes = nodelist_get_list();
    SMARTLIST_FOREACH(nodes, const node_t *, node, {
      if (!fascist_firewall_allows_node(node))
        smartlist_add(excluded, (void*)node);
    });
  }
  /* and exclude current entry guards and their families, if applicable */
  /*XXXX025 use the using_as_guard flag to accomplish this.*/
  if (options->UseEntryGuards) {
    SMARTLIST_FOREACH(get_entry_guards(), const entry_guard_t *, entry,
      {
        if ((node = node_get_by_id(entry->identity))) {
          nodelist_add_node_and_family(excluded, node);
        }
      });
  }

  if (state) {
    if (state->need_uptime)
      flags |= CRN_NEED_UPTIME;
    if (state->need_capacity)
      flags |= CRN_NEED_CAPACITY;
  }
  if (options->AllowInvalid_ & ALLOW_INVALID_ENTRY)
    flags |= CRN_ALLOW_INVALID;

  choice = router_choose_random_node(excluded, options->ExcludeNodes, flags);
  smartlist_free(excluded);
  return choice;
}

/** Return the first non-open hop in cpath, or return NULL if all
 * hops are open. */
static crypt_path_t *
onion_next_hop_in_cpath(crypt_path_t *cpath)
{
  crypt_path_t *hop = cpath;
  do {
    if (hop->state != CPATH_STATE_OPEN)
      return hop;
    hop = hop->next;
  } while (hop != cpath);
  return NULL;
}

/** Choose a suitable next hop in the cpath <b>head_ptr</b>,
 * based on <b>state</b>. Append the hop info to head_ptr.
 *
 * Return 1 if the path is complete, 0 if we successfully added a hop,
 * and -1 on error.
 */
static int
onion_extend_cpath(origin_circuit_t *circ)
{
  uint8_t purpose = circ->base_.purpose;
  cpath_build_state_t *state = circ->build_state;
  int cur_len = circuit_get_cpath_len(circ);
  extend_info_t *info = NULL;

  if (cur_len >= state->desired_path_len) {
    log_debug(LD_CIRC, "Path is complete: %d steps long",
              state->desired_path_len);
    return 1;
  }

  log_debug(LD_CIRC, "Path is %d long; we want %d", cur_len,
            state->desired_path_len);

  if (cur_len == state->desired_path_len - 1) { /* Picking last node */
    info = extend_info_dup(state->chosen_exit);
  } else if (cur_len == 0) { /* picking first node */
    const node_t *r = choose_good_entry_server(purpose, state);
    if (r) {
      /* If we're a client, use the preferred address rather than the
         primary address, for potentially connecting to an IPv6 OR
         port. */
      info = extend_info_from_node(r, server_mode(get_options()) == 0);
      tor_assert(info);
    }
  } else {
    const node_t *r =
      choose_good_middle_server(purpose, state, circ->cpath, cur_len);
    if (r) {
      info = extend_info_from_node(r, 0);
      tor_assert(info);
    }
  }

  if (!info) {
    log_warn(LD_CIRC,"Failed to find node for hop %d of our path. Discarding "
             "this circuit.", cur_len);
    return -1;
  }

  log_debug(LD_CIRC,"Chose router %s for hop %d (exit is %s)",
            extend_info_describe(info),
            cur_len+1, build_state_get_exit_nickname(state));

  onion_append_hop(&circ->cpath, info);
  extend_info_free(info);
  return 0;
}

/** Create a new hop, annotate it with information about its
 * corresponding router <b>choice</b>, and append it to the
 * end of the cpath <b>head_ptr</b>. */
static int
onion_append_hop(crypt_path_t **head_ptr, extend_info_t *choice)
{
  crypt_path_t *hop = tor_malloc_zero(sizeof(crypt_path_t));

  /* link hop into the cpath, at the end. */
  onion_append_to_cpath(head_ptr, hop);

  hop->magic = CRYPT_PATH_MAGIC;
  hop->state = CPATH_STATE_CLOSED;

  hop->extend_info = extend_info_dup(choice);

  hop->package_window = circuit_initial_package_window();
  hop->deliver_window = CIRCWINDOW_START;

  return 0;
}

/** Allocate a new extend_info object based on the various arguments. */
extend_info_t *
extend_info_new(const char *nickname, const char *digest,
                crypto_pk_t *onion_key,
                const curve25519_public_key_t *curve25519_key,
                const tor_addr_t *addr, uint16_t port)
{
  extend_info_t *info = tor_malloc_zero(sizeof(extend_info_t));
  memcpy(info->identity_digest, digest, DIGEST_LEN);
  if (nickname)
    strlcpy(info->nickname, nickname, sizeof(info->nickname));
  if (onion_key)
    info->onion_key = crypto_pk_dup_key(onion_key);
#ifdef CURVE25519_ENABLED
  if (curve25519_key)
    memcpy(&info->curve25519_onion_key, curve25519_key,
           sizeof(curve25519_public_key_t));
#else
  (void)curve25519_key;
#endif
  tor_addr_copy(&info->addr, addr);
  info->port = port;
  return info;
}

/** Allocate and return a new extend_info that can be used to build a
 * circuit to or through the node <b>node</b>. Use the primary address
 * of the node (i.e. its IPv4 address) unless
 * <b>for_direct_connect</b> is true, in which case the preferred
 * address is used instead. May return NULL if there is not enough
 * info about <b>node</b> to extend to it--for example, if there is no
 * routerinfo_t or microdesc_t.
 **/
extend_info_t *
extend_info_from_node(const node_t *node, int for_direct_connect)
{
  tor_addr_port_t ap;

  if (node->ri == NULL && (node->rs == NULL || node->md == NULL))
    return NULL;

  if (for_direct_connect)
    node_get_pref_orport(node, &ap);
  else
    node_get_prim_orport(node, &ap);

  log_debug(LD_CIRC, "using %s for %s",
            fmt_addrport(&ap.addr, ap.port),
            node->ri ? node->ri->nickname : node->rs->nickname);

  if (node->ri)
    return extend_info_new(node->ri->nickname,
                             node->identity,
                             node->ri->onion_pkey,
                             node->ri->onion_curve25519_pkey,
                             &ap.addr,
                             ap.port);
  else if (node->rs && node->md)
    return extend_info_new(node->rs->nickname,
                             node->identity,
                             node->md->onion_pkey,
                             node->md->onion_curve25519_pkey,
                             &ap.addr,
                             ap.port);
  else
    return NULL;
}

/** Release storage held by an extend_info_t struct. */
void
extend_info_free(extend_info_t *info)
{
  if (!info)
    return;
  crypto_pk_free(info->onion_key);
  tor_free(info);
}

/** Allocate and return a new extend_info_t with the same contents as
 * <b>info</b>. */
extend_info_t *
extend_info_dup(extend_info_t *info)
{
  extend_info_t *newinfo;
  tor_assert(info);
  newinfo = tor_malloc(sizeof(extend_info_t));
  memcpy(newinfo, info, sizeof(extend_info_t));
  if (info->onion_key)
    newinfo->onion_key = crypto_pk_dup_key(info->onion_key);
  else
    newinfo->onion_key = NULL;
  return newinfo;
}

/** Return the routerinfo_t for the chosen exit router in <b>state</b>.
 * If there is no chosen exit, or if we don't know the routerinfo_t for
 * the chosen exit, return NULL.
 */
const node_t *
build_state_get_exit_node(cpath_build_state_t *state)
{
  if (!state || !state->chosen_exit)
    return NULL;
  return node_get_by_id(state->chosen_exit->identity_digest);
}

/** Return the nickname for the chosen exit router in <b>state</b>. If
 * there is no chosen exit, or if we don't know the routerinfo_t for the
 * chosen exit, return NULL.
 */
const char *
build_state_get_exit_nickname(cpath_build_state_t *state)
{
  if (!state || !state->chosen_exit)
    return NULL;
  return state->chosen_exit->nickname;
}

