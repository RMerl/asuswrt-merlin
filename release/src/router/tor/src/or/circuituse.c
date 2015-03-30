/* Copyright (c) 2001 Matej Pfajfar.
 * Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file circuituse.c
 * \brief Launch the right sort of circuits and attach streams to them.
 **/

#include "or.h"
#include "addressmap.h"
#include "channel.h"
#include "circpathbias.h"
#include "circuitbuild.h"
#include "circuitlist.h"
#include "circuitstats.h"
#include "circuituse.h"
#include "config.h"
#include "connection.h"
#include "connection_edge.h"
#include "control.h"
#include "entrynodes.h"
#include "nodelist.h"
#include "networkstatus.h"
#include "policies.h"
#include "rendclient.h"
#include "rendcommon.h"
#include "rendservice.h"
#include "rephist.h"
#include "router.h"
#include "routerlist.h"

static void circuit_expire_old_circuits_clientside(void);
static void circuit_increment_failure_count(void);

/** Return 1 if <b>circ</b> could be returned by circuit_get_best().
 * Else return 0.
 */
static int
circuit_is_acceptable(const origin_circuit_t *origin_circ,
                      const entry_connection_t *conn,
                      int must_be_open, uint8_t purpose,
                      int need_uptime, int need_internal,
                      time_t now)
{
  const circuit_t *circ = TO_CIRCUIT(origin_circ);
  const node_t *exitnode;
  cpath_build_state_t *build_state;
  tor_assert(circ);
  tor_assert(conn);
  tor_assert(conn->socks_request);

  if (must_be_open && (circ->state != CIRCUIT_STATE_OPEN || !circ->n_chan))
    return 0; /* ignore non-open circs */
  if (circ->marked_for_close)
    return 0;

  /* if this circ isn't our purpose, skip. */
  if (purpose == CIRCUIT_PURPOSE_C_REND_JOINED && !must_be_open) {
    if (circ->purpose != CIRCUIT_PURPOSE_C_ESTABLISH_REND &&
        circ->purpose != CIRCUIT_PURPOSE_C_REND_READY &&
        circ->purpose != CIRCUIT_PURPOSE_C_REND_READY_INTRO_ACKED &&
        circ->purpose != CIRCUIT_PURPOSE_C_REND_JOINED)
      return 0;
  } else if (purpose == CIRCUIT_PURPOSE_C_INTRODUCE_ACK_WAIT &&
             !must_be_open) {
    if (circ->purpose != CIRCUIT_PURPOSE_C_INTRODUCING &&
        circ->purpose != CIRCUIT_PURPOSE_C_INTRODUCE_ACK_WAIT)
      return 0;
  } else {
    if (purpose != circ->purpose)
      return 0;
  }

  /* If this is a timed-out hidden service circuit, skip it. */
  if (origin_circ->hs_circ_has_timed_out) {
    return 0;
  }

  if (purpose == CIRCUIT_PURPOSE_C_GENERAL ||
      purpose == CIRCUIT_PURPOSE_C_REND_JOINED) {
    if (circ->timestamp_dirty &&
       circ->timestamp_dirty+get_options()->MaxCircuitDirtiness <= now)
      return 0;
  }

  if (origin_circ->unusable_for_new_conns)
    return 0;

  /* decide if this circ is suitable for this conn */

  /* for rend circs, circ->cpath->prev is not the last router in the
   * circuit, it's the magical extra bob hop. so just check the nickname
   * of the one we meant to finish at.
   */
  build_state = origin_circ->build_state;
  exitnode = build_state_get_exit_node(build_state);

  if (need_uptime && !build_state->need_uptime)
    return 0;
  if (need_internal != build_state->is_internal)
    return 0;

  if (purpose == CIRCUIT_PURPOSE_C_GENERAL) {
    tor_addr_t addr;
    const int family = tor_addr_parse(&addr, conn->socks_request->address);
    if (!exitnode && !build_state->onehop_tunnel) {
      log_debug(LD_CIRC,"Not considering circuit with unknown router.");
      return 0; /* this circuit is screwed and doesn't know it yet,
                 * or is a rendezvous circuit. */
    }
    if (build_state->onehop_tunnel) {
      if (!conn->want_onehop) {
        log_debug(LD_CIRC,"Skipping one-hop circuit.");
        return 0;
      }
      tor_assert(conn->chosen_exit_name);
      if (build_state->chosen_exit) {
        char digest[DIGEST_LEN];
        if (hexdigest_to_digest(conn->chosen_exit_name, digest) < 0)
          return 0; /* broken digest, we don't want it */
        if (tor_memneq(digest, build_state->chosen_exit->identity_digest,
                          DIGEST_LEN))
          return 0; /* this is a circuit to somewhere else */
        if (tor_digest_is_zero(digest)) {
          /* we don't know the digest; have to compare addr:port */
          if (family < 0 ||
              !tor_addr_eq(&build_state->chosen_exit->addr, &addr) ||
              build_state->chosen_exit->port != conn->socks_request->port)
            return 0;
        }
      }
    } else {
      if (conn->want_onehop) {
        /* don't use three-hop circuits -- that could hurt our anonymity. */
        return 0;
      }
    }
    if (origin_circ->prepend_policy && family != -1) {
      int r = compare_tor_addr_to_addr_policy(&addr,
                                              conn->socks_request->port,
                                              origin_circ->prepend_policy);
      if (r == ADDR_POLICY_REJECTED)
        return 0;
    }
    if (exitnode && !connection_ap_can_use_exit(conn, exitnode)) {
      /* can't exit from this router */
      return 0;
    }
  } else { /* not general */
    const edge_connection_t *edge_conn = ENTRY_TO_EDGE_CONN(conn);
    if ((edge_conn->rend_data && !origin_circ->rend_data) ||
        (!edge_conn->rend_data && origin_circ->rend_data) ||
        (edge_conn->rend_data && origin_circ->rend_data &&
         rend_cmp_service_ids(edge_conn->rend_data->onion_address,
                              origin_circ->rend_data->onion_address))) {
      /* this circ is not for this conn */
      return 0;
    }
  }

  if (!connection_edge_compatible_with_circuit(conn, origin_circ)) {
    /* conn needs to be isolated from other conns that have already used
     * origin_circ */
    return 0;
  }

  return 1;
}

/** Return 1 if circuit <b>a</b> is better than circuit <b>b</b> for
 * <b>conn</b>, and return 0 otherwise. Used by circuit_get_best.
 */
static int
circuit_is_better(const origin_circuit_t *oa, const origin_circuit_t *ob,
                  const entry_connection_t *conn)
{
  const circuit_t *a = TO_CIRCUIT(oa);
  const circuit_t *b = TO_CIRCUIT(ob);
  const uint8_t purpose = ENTRY_TO_CONN(conn)->purpose;
  int a_bits, b_bits;

  /* If one of the circuits was allowed to live due to relaxing its timeout,
   * it is definitely worse (it's probably a much slower path). */
  if (oa->relaxed_timeout && !ob->relaxed_timeout)
    return 0; /* ob is better. It's not relaxed. */
  if (!oa->relaxed_timeout && ob->relaxed_timeout)
    return 1; /* oa is better. It's not relaxed. */

  switch (purpose) {
    case CIRCUIT_PURPOSE_C_GENERAL:
      /* if it's used but less dirty it's best;
       * else if it's more recently created it's best
       */
      if (b->timestamp_dirty) {
        if (a->timestamp_dirty &&
            a->timestamp_dirty > b->timestamp_dirty)
          return 1;
      } else {
        if (a->timestamp_dirty ||
            timercmp(&a->timestamp_began, &b->timestamp_began, >))
          return 1;
        if (ob->build_state->is_internal)
          /* XXX023 what the heck is this internal thing doing here. I
           * think we can get rid of it. circuit_is_acceptable() already
           * makes sure that is_internal is exactly what we need it to
           * be. -RD */
          return 1;
      }
      break;
    case CIRCUIT_PURPOSE_C_INTRODUCE_ACK_WAIT:
      /* the closer it is to ack_wait the better it is */
      if (a->purpose > b->purpose)
        return 1;
      break;
    case CIRCUIT_PURPOSE_C_REND_JOINED:
      /* the closer it is to rend_joined the better it is */
      if (a->purpose > b->purpose)
        return 1;
      break;
  }

  /* XXXX023 Maybe this check should get a higher priority to avoid
   *   using up circuits too rapidly. */

  a_bits = connection_edge_update_circuit_isolation(conn,
                                                    (origin_circuit_t*)oa, 1);
  b_bits = connection_edge_update_circuit_isolation(conn,
                                                    (origin_circuit_t*)ob, 1);
  /* if x_bits < 0, then we have not used x for anything; better not to dirty
   * a connection if we can help it. */
  if (a_bits < 0) {
    return 0;
  } else if (b_bits < 0) {
    return 1;
  }
  a_bits &= ~ oa->isolation_flags_mixed;
  a_bits &= ~ ob->isolation_flags_mixed;
  if (n_bits_set_u8(a_bits) < n_bits_set_u8(b_bits)) {
    /* The fewer new restrictions we need to make on a circuit for stream
     * isolation, the better. */
    return 1;
  }

  return 0;
}

/** Find the best circ that conn can use, preferably one which is
 * dirty. Circ must not be too old.
 *
 * Conn must be defined.
 *
 * If must_be_open, ignore circs not in CIRCUIT_STATE_OPEN.
 *
 * circ_purpose specifies what sort of circuit we must have.
 * It can be C_GENERAL, C_INTRODUCE_ACK_WAIT, or C_REND_JOINED.
 *
 * If it's REND_JOINED and must_be_open==0, then return the closest
 * rendezvous-purposed circuit that you can find.
 *
 * If it's INTRODUCE_ACK_WAIT and must_be_open==0, then return the
 * closest introduce-purposed circuit that you can find.
 */
static origin_circuit_t *
circuit_get_best(const entry_connection_t *conn,
                 int must_be_open, uint8_t purpose,
                 int need_uptime, int need_internal)
{
  circuit_t *circ;
  origin_circuit_t *best=NULL;
  struct timeval now;
  int intro_going_on_but_too_old = 0;

  tor_assert(conn);

  tor_assert(purpose == CIRCUIT_PURPOSE_C_GENERAL ||
             purpose == CIRCUIT_PURPOSE_C_INTRODUCE_ACK_WAIT ||
             purpose == CIRCUIT_PURPOSE_C_REND_JOINED);

  tor_gettimeofday(&now);

  TOR_LIST_FOREACH(circ, circuit_get_global_list(), head) {
    origin_circuit_t *origin_circ;
    if (!CIRCUIT_IS_ORIGIN(circ))
      continue;
    origin_circ = TO_ORIGIN_CIRCUIT(circ);

    /* Log an info message if we're going to launch a new intro circ in
     * parallel */
    if (purpose == CIRCUIT_PURPOSE_C_INTRODUCE_ACK_WAIT &&
        !must_be_open && origin_circ->hs_circ_has_timed_out) {
        intro_going_on_but_too_old = 1;
        continue;
    }

    if (!circuit_is_acceptable(origin_circ,conn,must_be_open,purpose,
                               need_uptime,need_internal, (time_t)now.tv_sec))
      continue;

    /* now this is an acceptable circ to hand back. but that doesn't
     * mean it's the *best* circ to hand back. try to decide.
     */
    if (!best || circuit_is_better(origin_circ,best,conn))
      best = origin_circ;
  }

  if (!best && intro_going_on_but_too_old)
    log_info(LD_REND|LD_CIRC, "There is an intro circuit being created "
             "right now, but it has already taken quite a while. Starting "
             "one in parallel.");

  return best;
}

/** Return the number of not-yet-open general-purpose origin circuits. */
static int
count_pending_general_client_circuits(void)
{
  const circuit_t *circ;

  int count = 0;

  TOR_LIST_FOREACH(circ, circuit_get_global_list(), head) {
    if (circ->marked_for_close ||
        circ->state == CIRCUIT_STATE_OPEN ||
        circ->purpose != CIRCUIT_PURPOSE_C_GENERAL ||
        !CIRCUIT_IS_ORIGIN(circ))
      continue;

    ++count;
  }

  return count;
}

#if 0
/** Check whether, according to the policies in <b>options</b>, the
 * circuit <b>circ</b> makes sense. */
/* XXXX currently only checks Exclude{Exit}Nodes; it should check more.
 * Also, it doesn't have the right definition of an exit circuit. Also,
 * it's never called. */
int
circuit_conforms_to_options(const origin_circuit_t *circ,
                            const or_options_t *options)
{
  const crypt_path_t *cpath, *cpath_next = NULL;

  /* first check if it includes any excluded nodes */
  for (cpath = circ->cpath; cpath_next != circ->cpath; cpath = cpath_next) {
    cpath_next = cpath->next;
    if (routerset_contains_extendinfo(options->ExcludeNodes,
                                      cpath->extend_info))
      return 0;
  }

  /* then consider the final hop */
  if (routerset_contains_extendinfo(options->ExcludeExitNodes,
                                    circ->cpath->prev->extend_info))
    return 0;

  return 1;
}
#endif

/** Close all circuits that start at us, aren't open, and were born
 * at least CircuitBuildTimeout seconds ago.
 */
void
circuit_expire_building(void)
{
  circuit_t *victim, *next_circ;
  /* circ_times.timeout_ms and circ_times.close_ms are from
   * circuit_build_times_get_initial_timeout() if we haven't computed
   * custom timeouts yet */
  struct timeval general_cutoff, begindir_cutoff, fourhop_cutoff,
    close_cutoff, extremely_old_cutoff, hs_extremely_old_cutoff,
    cannibalized_cutoff, c_intro_cutoff, s_intro_cutoff, stream_cutoff;
  const or_options_t *options = get_options();
  struct timeval now;
  cpath_build_state_t *build_state;
  int any_opened_circs = 0;

  tor_gettimeofday(&now);

  /* Check to see if we have any opened circuits. If we don't,
   * we want to be more lenient with timeouts, in case the
   * user has relocated and/or changed network connections.
   * See bug #3443. */
  TOR_LIST_FOREACH(next_circ, circuit_get_global_list(), head) {
    if (!CIRCUIT_IS_ORIGIN(next_circ) || /* didn't originate here */
        next_circ->marked_for_close) { /* don't mess with marked circs */
      continue;
    }

    if (TO_ORIGIN_CIRCUIT(next_circ)->has_opened &&
        next_circ->state == CIRCUIT_STATE_OPEN &&
        TO_ORIGIN_CIRCUIT(next_circ)->build_state &&
        TO_ORIGIN_CIRCUIT(next_circ)->build_state->desired_path_len
          == DEFAULT_ROUTE_LEN) {
      any_opened_circs = 1;
      break;
    }
  }

#define SET_CUTOFF(target, msec) do {                       \
    long ms = tor_lround(msec);                             \
    struct timeval diff;                                    \
    diff.tv_sec = ms / 1000;                                \
    diff.tv_usec = (int)((ms % 1000) * 1000);               \
    timersub(&now, &diff, &target);                         \
  } while (0)

  /**
   * Because circuit build timeout is calculated only based on 3 hop
   * general purpose circuit construction, we need to scale the timeout
   * to make it properly apply to longer circuits, and circuits of
   * certain usage types. The following diagram illustrates how we
   * derive the scaling below. In short, we calculate the number
   * of times our telescoping-based circuit construction causes cells
   * to traverse each link for the circuit purpose types in question,
   * and then assume each link is equivalent.
   *
   * OP --a--> A --b--> B --c--> C
   * OP --a--> A --b--> B --c--> C --d--> D
   *
   * Let h = a = b = c = d
   *
   * Three hops (general_cutoff)
   *   RTTs = 3a + 2b + c
   *   RTTs = 6h
   * Cannibalized:
   *   RTTs = a+b+c+d
   *   RTTs = 4h
   * Four hops:
   *   RTTs = 4a + 3b + 2c + d
   *   RTTs = 10h
   * Client INTRODUCE1+ACK: // XXX: correct?
   *   RTTs = 5a + 4b + 3c + 2d
   *   RTTs = 14h
   * Server intro:
   *   RTTs = 4a + 3b + 2c
   *   RTTs = 9h
   */
  SET_CUTOFF(general_cutoff, get_circuit_build_timeout_ms());
  SET_CUTOFF(begindir_cutoff, get_circuit_build_timeout_ms());

  /* > 3hop circs seem to have a 1.0 second delay on their cannibalized
   * 4th hop. */
  SET_CUTOFF(fourhop_cutoff, get_circuit_build_timeout_ms() * (10/6.0) + 1000);

  /* CIRCUIT_PURPOSE_C_ESTABLISH_REND behaves more like a RELAY cell.
   * Use the stream cutoff (more or less). */
  SET_CUTOFF(stream_cutoff, MAX(options->CircuitStreamTimeout,15)*1000 + 1000);

  /* Be lenient with cannibalized circs. They already survived the official
   * CBT, and they're usually not performance-critical. */
  SET_CUTOFF(cannibalized_cutoff,
             MAX(get_circuit_build_close_time_ms()*(4/6.0),
                 options->CircuitStreamTimeout * 1000) + 1000);

  /* Intro circs have an extra round trip (and are also 4 hops long) */
  SET_CUTOFF(c_intro_cutoff, get_circuit_build_timeout_ms() * (14/6.0) + 1000);

  /* Server intro circs have an extra round trip */
  SET_CUTOFF(s_intro_cutoff, get_circuit_build_timeout_ms() * (9/6.0) + 1000);

  SET_CUTOFF(close_cutoff, get_circuit_build_close_time_ms());
  SET_CUTOFF(extremely_old_cutoff, get_circuit_build_close_time_ms()*2 + 1000);

  SET_CUTOFF(hs_extremely_old_cutoff,
             MAX(get_circuit_build_close_time_ms()*2 + 1000,
                 options->SocksTimeout * 1000));

  TOR_LIST_FOREACH(next_circ, circuit_get_global_list(), head) {
    struct timeval cutoff;
    victim = next_circ;
    if (!CIRCUIT_IS_ORIGIN(victim) || /* didn't originate here */
        victim->marked_for_close)     /* don't mess with marked circs */
      continue;

    /* If we haven't yet started the first hop, it means we don't have
     * any orconns available, and thus have not started counting time yet
     * for this circuit. See circuit_deliver_create_cell() and uses of
     * timestamp_began.
     *
     * Continue to wait in this case. The ORConn should timeout
     * independently and kill us then.
     */
    if (TO_ORIGIN_CIRCUIT(victim)->cpath->state == CPATH_STATE_CLOSED) {
      continue;
    }

    build_state = TO_ORIGIN_CIRCUIT(victim)->build_state;
    if (build_state && build_state->onehop_tunnel)
      cutoff = begindir_cutoff;
    else if (victim->purpose == CIRCUIT_PURPOSE_C_MEASURE_TIMEOUT)
      cutoff = close_cutoff;
    else if (victim->purpose == CIRCUIT_PURPOSE_C_INTRODUCING ||
             victim->purpose == CIRCUIT_PURPOSE_C_INTRODUCE_ACK_WAIT)
      cutoff = c_intro_cutoff;
    else if (victim->purpose == CIRCUIT_PURPOSE_S_ESTABLISH_INTRO)
      cutoff = s_intro_cutoff;
    else if (victim->purpose == CIRCUIT_PURPOSE_C_ESTABLISH_REND)
      cutoff = stream_cutoff;
    else if (victim->purpose == CIRCUIT_PURPOSE_PATH_BIAS_TESTING)
      cutoff = close_cutoff;
    else if (TO_ORIGIN_CIRCUIT(victim)->has_opened &&
             victim->state != CIRCUIT_STATE_OPEN)
      cutoff = cannibalized_cutoff;
    else if (build_state && build_state->desired_path_len >= 4)
      cutoff = fourhop_cutoff;
    else
      cutoff = general_cutoff;

    if (TO_ORIGIN_CIRCUIT(victim)->hs_circ_has_timed_out)
      cutoff = hs_extremely_old_cutoff;

    if (timercmp(&victim->timestamp_began, &cutoff, >))
      continue; /* it's still young, leave it alone */

    /* We need to double-check the opened state here because
     * we don't want to consider opened 1-hop dircon circuits for
     * deciding when to relax the timeout, but we *do* want to relax
     * those circuits too if nothing else is opened *and* they still
     * aren't either. */
    if (!any_opened_circs && victim->state != CIRCUIT_STATE_OPEN) {
      /* It's still young enough that we wouldn't close it, right? */
      if (timercmp(&victim->timestamp_began, &close_cutoff, >)) {
        if (!TO_ORIGIN_CIRCUIT(victim)->relaxed_timeout) {
          int first_hop_succeeded = TO_ORIGIN_CIRCUIT(victim)->cpath->state
                                      == CPATH_STATE_OPEN;
          log_info(LD_CIRC,
                 "No circuits are opened. Relaxing timeout for circuit %d "
                 "(a %s %d-hop circuit in state %s with channel state %s). "
                 "%d guards are live.",
                 TO_ORIGIN_CIRCUIT(victim)->global_identifier,
                 circuit_purpose_to_string(victim->purpose),
                 TO_ORIGIN_CIRCUIT(victim)->build_state ?
                   TO_ORIGIN_CIRCUIT(victim)->build_state->desired_path_len :
                   -1,
                 circuit_state_to_string(victim->state),
                 channel_state_to_string(victim->n_chan->state),
                 num_live_entry_guards(0));

          /* We count the timeout here for CBT, because technically this
           * was a timeout, and the timeout value needs to reset if we
           * see enough of them. Note this means we also need to avoid
           * double-counting below, too. */
          circuit_build_times_count_timeout(get_circuit_build_times_mutable(),
              first_hop_succeeded);
          TO_ORIGIN_CIRCUIT(victim)->relaxed_timeout = 1;
        }
        continue;
      } else {
        static ratelim_t relax_timeout_limit = RATELIM_INIT(3600);
        const double build_close_ms = get_circuit_build_close_time_ms();
        log_fn_ratelim(&relax_timeout_limit, LOG_NOTICE, LD_CIRC,
                 "No circuits are opened. Relaxed timeout for circuit %d "
                 "(a %s %d-hop circuit in state %s with channel state %s) to "
                 "%ldms. However, it appears the circuit has timed out "
                 "anyway. %d guards are live.",
                 TO_ORIGIN_CIRCUIT(victim)->global_identifier,
                 circuit_purpose_to_string(victim->purpose),
                 TO_ORIGIN_CIRCUIT(victim)->build_state ?
                   TO_ORIGIN_CIRCUIT(victim)->build_state->desired_path_len :
                   -1,
                 circuit_state_to_string(victim->state),
                 channel_state_to_string(victim->n_chan->state),
                 (long)build_close_ms,
                 num_live_entry_guards(0));
      }
    }

#if 0
    /* some debug logs, to help track bugs */
    if (victim->purpose >= CIRCUIT_PURPOSE_C_INTRODUCING &&
        victim->purpose <= CIRCUIT_PURPOSE_C_REND_READY_INTRO_ACKED) {
      if (!victim->timestamp_dirty)
        log_fn(LOG_DEBUG,"Considering %sopen purpose %d to %s (circid %d)."
               "(clean).",
               victim->state == CIRCUIT_STATE_OPEN ? "" : "non",
               victim->purpose, victim->build_state->chosen_exit_name,
               victim->n_circ_id);
      else
        log_fn(LOG_DEBUG,"Considering %sopen purpose %d to %s (circid %d). "
               "%d secs since dirty.",
               victim->state == CIRCUIT_STATE_OPEN ? "" : "non",
               victim->purpose, victim->build_state->chosen_exit_name,
               victim->n_circ_id,
               (int)(now - victim->timestamp_dirty));
    }
#endif

    /* if circ is !open, or if it's open but purpose is a non-finished
     * intro or rend, then mark it for close */
    if (victim->state == CIRCUIT_STATE_OPEN) {
      switch (victim->purpose) {
        default: /* most open circuits can be left alone. */
          continue; /* yes, continue inside a switch refers to the nearest
                     * enclosing loop. C is smart. */
        case CIRCUIT_PURPOSE_S_ESTABLISH_INTRO:
          break; /* too old, need to die */
        case CIRCUIT_PURPOSE_C_REND_READY:
          /* it's a rend_ready circ -- has it already picked a query? */
          /* c_rend_ready circs measure age since timestamp_dirty,
           * because that's set when they switch purposes
           */
          if (TO_ORIGIN_CIRCUIT(victim)->rend_data ||
              victim->timestamp_dirty > cutoff.tv_sec)
            continue;
          break;
        case CIRCUIT_PURPOSE_PATH_BIAS_TESTING:
          /* Open path bias testing circuits are given a long
           * time to complete the test, but not forever */
          TO_ORIGIN_CIRCUIT(victim)->path_state = PATH_STATE_USE_FAILED;
          break;
        case CIRCUIT_PURPOSE_C_INTRODUCING:
          /* We keep old introducing circuits around for
           * a while in parallel, and they can end up "opened".
           * We decide below if we're going to mark them timed
           * out and eventually close them.
           */
          break;
        case CIRCUIT_PURPOSE_C_ESTABLISH_REND:
        case CIRCUIT_PURPOSE_C_REND_READY_INTRO_ACKED:
        case CIRCUIT_PURPOSE_C_INTRODUCE_ACK_WAIT:
          /* rend and intro circs become dirty each time they
           * make an introduction attempt. so timestamp_dirty
           * will reflect the time since the last attempt.
           */
          if (victim->timestamp_dirty > cutoff.tv_sec)
            continue;
          break;
      }
    } else { /* circuit not open, consider recording failure as timeout */
      int first_hop_succeeded = TO_ORIGIN_CIRCUIT(victim)->cpath &&
            TO_ORIGIN_CIRCUIT(victim)->cpath->state == CPATH_STATE_OPEN;

      if (TO_ORIGIN_CIRCUIT(victim)->p_streams != NULL) {
        log_warn(LD_BUG, "Circuit %d (purpose %d, %s) has timed out, "
                 "yet has attached streams!",
                 TO_ORIGIN_CIRCUIT(victim)->global_identifier,
                 victim->purpose,
                 circuit_purpose_to_string(victim->purpose));
        tor_fragile_assert();
        continue;
      }

      if (circuit_timeout_want_to_count_circ(TO_ORIGIN_CIRCUIT(victim)) &&
          circuit_build_times_enough_to_compute(get_circuit_build_times())) {
        /* Circuits are allowed to last longer for measurement.
         * Switch their purpose and wait. */
        if (victim->purpose != CIRCUIT_PURPOSE_C_MEASURE_TIMEOUT) {
          control_event_circuit_status(TO_ORIGIN_CIRCUIT(victim),
                                       CIRC_EVENT_FAILED,
                                       END_CIRC_REASON_TIMEOUT);
          circuit_change_purpose(victim, CIRCUIT_PURPOSE_C_MEASURE_TIMEOUT);
          /* Record this failure to check for too many timeouts
           * in a row. This function does not record a time value yet
           * (we do that later); it only counts the fact that we did
           * have a timeout. We also want to avoid double-counting
           * already "relaxed" circuits, which are counted above. */
          if (!TO_ORIGIN_CIRCUIT(victim)->relaxed_timeout) {
            circuit_build_times_count_timeout(
                                         get_circuit_build_times_mutable(),
                                         first_hop_succeeded);
          }
          continue;
        }

        /*
         * If the circuit build time is much greater than we would have cut
         * it off at, we probably had a suspend event along this codepath,
         * and we should discard the value.
         */
        if (timercmp(&victim->timestamp_began, &extremely_old_cutoff, <)) {
          log_notice(LD_CIRC,
                     "Extremely large value for circuit build timeout: %lds. "
                     "Assuming clock jump. Purpose %d (%s)",
                     (long)(now.tv_sec - victim->timestamp_began.tv_sec),
                     victim->purpose,
                     circuit_purpose_to_string(victim->purpose));
        } else if (circuit_build_times_count_close(
            get_circuit_build_times_mutable(),
            first_hop_succeeded,
            (time_t)victim->timestamp_created.tv_sec)) {
          circuit_build_times_set_timeout(get_circuit_build_times_mutable());
        }
      }
    }

    /* If this is a hidden service client circuit which is far enough
     * along in connecting to its destination, and we haven't already
     * flagged it as 'timed out', and the user has not told us to
     * close such circs immediately on timeout, flag it as 'timed out'
     * so we'll launch another intro or rend circ, but don't mark it
     * for close yet.
     *
     * (Circs flagged as 'timed out' are given a much longer timeout
     * period above, so we won't close them in the next call to
     * circuit_expire_building.) */
    if (!(options->CloseHSClientCircuitsImmediatelyOnTimeout) &&
        !(TO_ORIGIN_CIRCUIT(victim)->hs_circ_has_timed_out)) {
      switch (victim->purpose) {
      case CIRCUIT_PURPOSE_C_REND_READY:
        /* We only want to spare a rend circ if it has been specified in
         * an INTRODUCE1 cell sent to a hidden service.  A circ's
         * pending_final_cpath field is non-NULL iff it is a rend circ
         * and we have tried to send an INTRODUCE1 cell specifying it.
         * Thus, if the pending_final_cpath field *is* NULL, then we
         * want to not spare it. */
        if (TO_ORIGIN_CIRCUIT(victim)->build_state &&
            TO_ORIGIN_CIRCUIT(victim)->build_state->pending_final_cpath ==
            NULL)
          break;
        /* fallthrough! */
      case CIRCUIT_PURPOSE_C_INTRODUCING:
        /* connection_ap_handshake_attach_circuit() will relaunch for us */
      case CIRCUIT_PURPOSE_C_INTRODUCE_ACK_WAIT:
      case CIRCUIT_PURPOSE_C_REND_READY_INTRO_ACKED:
        /* If we have reached this line, we want to spare the circ for now. */
        log_info(LD_CIRC,"Marking circ %u (state %d:%s, purpose %d) "
                 "as timed-out HS circ",
                 (unsigned)victim->n_circ_id,
                 victim->state, circuit_state_to_string(victim->state),
                 victim->purpose);
        TO_ORIGIN_CIRCUIT(victim)->hs_circ_has_timed_out = 1;
        continue;
      default:
        break;
      }
    }

    /* If this is a service-side rendezvous circuit which is far
     * enough along in connecting to its destination, consider sparing
     * it. */
    if (!(options->CloseHSServiceRendCircuitsImmediatelyOnTimeout) &&
        !(TO_ORIGIN_CIRCUIT(victim)->hs_circ_has_timed_out) &&
        victim->purpose == CIRCUIT_PURPOSE_S_CONNECT_REND) {
      log_info(LD_CIRC,"Marking circ %u (state %d:%s, purpose %d) "
               "as timed-out HS circ; relaunching rendezvous attempt.",
               (unsigned)victim->n_circ_id,
               victim->state, circuit_state_to_string(victim->state),
               victim->purpose);
      TO_ORIGIN_CIRCUIT(victim)->hs_circ_has_timed_out = 1;
      rend_service_relaunch_rendezvous(TO_ORIGIN_CIRCUIT(victim));
      continue;
    }

    if (victim->n_chan)
      log_info(LD_CIRC,
               "Abandoning circ %u %s:%u (state %d,%d:%s, purpose %d, "
               "len %d)", TO_ORIGIN_CIRCUIT(victim)->global_identifier,
               channel_get_canonical_remote_descr(victim->n_chan),
               (unsigned)victim->n_circ_id,
               TO_ORIGIN_CIRCUIT(victim)->has_opened,
               victim->state, circuit_state_to_string(victim->state),
               victim->purpose,
               TO_ORIGIN_CIRCUIT(victim)->build_state ?
                 TO_ORIGIN_CIRCUIT(victim)->build_state->desired_path_len :
                 -1);
    else
      log_info(LD_CIRC,
               "Abandoning circ %u %u (state %d,%d:%s, purpose %d, len %d)",
               TO_ORIGIN_CIRCUIT(victim)->global_identifier,
               (unsigned)victim->n_circ_id,
               TO_ORIGIN_CIRCUIT(victim)->has_opened,
               victim->state,
               circuit_state_to_string(victim->state), victim->purpose,
               TO_ORIGIN_CIRCUIT(victim)->build_state ?
                 TO_ORIGIN_CIRCUIT(victim)->build_state->desired_path_len :
                 -1);

    circuit_log_path(LOG_INFO,LD_CIRC,TO_ORIGIN_CIRCUIT(victim));
    if (victim->purpose == CIRCUIT_PURPOSE_C_MEASURE_TIMEOUT)
      circuit_mark_for_close(victim, END_CIRC_REASON_MEASUREMENT_EXPIRED);
    else
      circuit_mark_for_close(victim, END_CIRC_REASON_TIMEOUT);

    pathbias_count_timeout(TO_ORIGIN_CIRCUIT(victim));
  }
}

/** For debugging #8387: track when we last called
 * circuit_expire_old_circuits_clientside. */
static time_t last_expired_clientside_circuits = 0;

/**
 * As a diagnostic for bug 8387, log information about how many one-hop
 * circuits we have around that have been there for at least <b>age</b>
 * seconds. Log a few of them.
 */
void
circuit_log_ancient_one_hop_circuits(int age)
{
#define MAX_ANCIENT_ONEHOP_CIRCUITS_TO_LOG 10
  time_t now = time(NULL);
  time_t cutoff = now - age;
  int n_found = 0;
  smartlist_t *log_these = smartlist_new();
  const circuit_t *circ;

  TOR_LIST_FOREACH(circ, circuit_get_global_list(), head) {
    const origin_circuit_t *ocirc;
    if (! CIRCUIT_IS_ORIGIN(circ))
      continue;
    if (circ->timestamp_created.tv_sec >= cutoff)
      continue;
    ocirc = CONST_TO_ORIGIN_CIRCUIT(circ);

    if (ocirc->build_state && ocirc->build_state->onehop_tunnel) {
      ++n_found;

      if (smartlist_len(log_these) < MAX_ANCIENT_ONEHOP_CIRCUITS_TO_LOG)
        smartlist_add(log_these, (origin_circuit_t*) ocirc);
    }
  }

  if (n_found == 0)
    goto done;

  log_notice(LD_HEARTBEAT,
             "Diagnostic for issue 8387: Found %d one-hop circuits more "
             "than %d seconds old! Logging %d...",
             n_found, age, smartlist_len(log_these));

  SMARTLIST_FOREACH_BEGIN(log_these, const origin_circuit_t *, ocirc) {
    char created[ISO_TIME_LEN+1];
    int stream_num;
    const edge_connection_t *conn;
    char *dirty = NULL;
    circ = TO_CIRCUIT(ocirc);

    format_local_iso_time(created,
                          (time_t)circ->timestamp_created.tv_sec);

    if (circ->timestamp_dirty) {
      char dirty_since[ISO_TIME_LEN+1];
      format_local_iso_time(dirty_since, circ->timestamp_dirty);

      tor_asprintf(&dirty, "Dirty since %s (%ld seconds vs %ld-second cutoff)",
                   dirty_since, (long)(now - circ->timestamp_dirty),
                   (long) get_options()->MaxCircuitDirtiness);
    } else {
      dirty = tor_strdup("Not marked dirty");
    }

    log_notice(LD_HEARTBEAT, "  #%d created at %s. %s, %s. %s for close. "
               "%s for new conns. %s.",
               ocirc_sl_idx,
               created,
               circuit_state_to_string(circ->state),
               circuit_purpose_to_string(circ->purpose),
               circ->marked_for_close ? "Marked" : "Not marked",
               ocirc->unusable_for_new_conns ? "Not usable" : "usable",
               dirty);
    tor_free(dirty);

    stream_num = 0;
    for (conn = ocirc->p_streams; conn; conn = conn->next_stream) {
      const connection_t *c = TO_CONN(conn);
      char stream_created[ISO_TIME_LEN+1];
      if (++stream_num >= 5)
        break;

      format_local_iso_time(stream_created, c->timestamp_created);

      log_notice(LD_HEARTBEAT, "     Stream#%d created at %s. "
                 "%s conn in state %s. "
                 "%s for close (%s:%d). Hold-open is %sset. "
                 "Has %ssent RELAY_END. %s on circuit.",
                 stream_num,
                 stream_created,
                 conn_type_to_string(c->type),
                 conn_state_to_string(c->type, c->state),
                 c->marked_for_close ? "Marked" : "Not marked",
                 c->marked_for_close_file ? c->marked_for_close_file : "--",
                 c->marked_for_close,
                 c->hold_open_until_flushed ? "" : "not ",
                 conn->edge_has_sent_end ? "" : "not ",
                 conn->edge_blocked_on_circ ? "Blocked" : "Not blocked");
      if (! c->linked_conn)
        continue;

      c = c->linked_conn;

      log_notice(LD_HEARTBEAT, "        Linked to %s connection in state %s "
                 "(Purpose %d). %s for close (%s:%d). Hold-open is %sset. ",
                 conn_type_to_string(c->type),
                 conn_state_to_string(c->type, c->state),
                 c->purpose,
                 c->marked_for_close ? "Marked" : "Not marked",
                 c->marked_for_close_file ? c->marked_for_close_file : "--",
                 c->marked_for_close,
                 c->hold_open_until_flushed ? "" : "not ");
    }
  } SMARTLIST_FOREACH_END(ocirc);

  log_notice(LD_HEARTBEAT, "It has been %ld seconds since I last called "
             "circuit_expire_old_circuits_clientside().",
             (long)(now - last_expired_clientside_circuits));

 done:
  smartlist_free(log_these);
}

/** Remove any elements in <b>needed_ports</b> that are handled by an
 * open or in-progress circuit.
 */
void
circuit_remove_handled_ports(smartlist_t *needed_ports)
{
  int i;
  uint16_t *port;

  for (i = 0; i < smartlist_len(needed_ports); ++i) {
    port = smartlist_get(needed_ports, i);
    tor_assert(*port);
    if (circuit_stream_is_being_handled(NULL, *port,
                                        MIN_CIRCUITS_HANDLING_STREAM)) {
//      log_debug(LD_CIRC,"Port %d is already being handled; removing.", port);
      smartlist_del(needed_ports, i--);
      tor_free(port);
    } else {
      log_debug(LD_CIRC,"Port %d is not handled.", *port);
    }
  }
}

/** Return 1 if at least <b>min</b> general-purpose non-internal circuits
 * will have an acceptable exit node for exit stream <b>conn</b> if it
 * is defined, else for "*:port".
 * Else return 0.
 */
int
circuit_stream_is_being_handled(entry_connection_t *conn,
                                uint16_t port, int min)
{
  circuit_t *circ;
  const node_t *exitnode;
  int num=0;
  time_t now = time(NULL);
  int need_uptime = smartlist_contains_int_as_string(
                                   get_options()->LongLivedPorts,
                                   conn ? conn->socks_request->port : port);

  TOR_LIST_FOREACH(circ, circuit_get_global_list(), head) {
    if (CIRCUIT_IS_ORIGIN(circ) &&
        !circ->marked_for_close &&
        circ->purpose == CIRCUIT_PURPOSE_C_GENERAL &&
        (!circ->timestamp_dirty ||
         circ->timestamp_dirty + get_options()->MaxCircuitDirtiness > now)) {
      origin_circuit_t *origin_circ = TO_ORIGIN_CIRCUIT(circ);
      cpath_build_state_t *build_state = origin_circ->build_state;
      if (build_state->is_internal || build_state->onehop_tunnel)
        continue;
      if (origin_circ->unusable_for_new_conns)
        continue;

      exitnode = build_state_get_exit_node(build_state);
      if (exitnode && (!need_uptime || build_state->need_uptime)) {
        int ok;
        if (conn) {
          ok = connection_ap_can_use_exit(conn, exitnode);
        } else {
          addr_policy_result_t r;
          r = compare_tor_addr_to_node_policy(NULL, port, exitnode);
          ok = r != ADDR_POLICY_REJECTED && r != ADDR_POLICY_PROBABLY_REJECTED;
        }
        if (ok) {
          if (++num >= min)
            return 1;
        }
      }
    }
  }
  return 0;
}

/** Don't keep more than this many unused open circuits around. */
#define MAX_UNUSED_OPEN_CIRCUITS 14

/** Figure out how many circuits we have open that are clean. Make
 * sure it's enough for all the upcoming behaviors we predict we'll have.
 * But put an upper bound on the total number of circuits.
 */
static void
circuit_predict_and_launch_new(void)
{
  circuit_t *circ;
  int num=0, num_internal=0, num_uptime_internal=0;
  int hidserv_needs_uptime=0, hidserv_needs_capacity=1;
  int port_needs_uptime=0, port_needs_capacity=1;
  time_t now = time(NULL);
  int flags = 0;

  /* First, count how many of each type of circuit we have already. */
  TOR_LIST_FOREACH(circ, circuit_get_global_list(), head) {
    cpath_build_state_t *build_state;
    origin_circuit_t *origin_circ;
    if (!CIRCUIT_IS_ORIGIN(circ))
      continue;
    if (circ->marked_for_close)
      continue; /* don't mess with marked circs */
    if (circ->timestamp_dirty)
      continue; /* only count clean circs */
    if (circ->purpose != CIRCUIT_PURPOSE_C_GENERAL)
      continue; /* only pay attention to general-purpose circs */
    origin_circ = TO_ORIGIN_CIRCUIT(circ);
    if (origin_circ->unusable_for_new_conns)
      continue;
    build_state = origin_circ->build_state;
    if (build_state->onehop_tunnel)
      continue;
    num++;
    if (build_state->is_internal)
      num_internal++;
    if (build_state->need_uptime && build_state->is_internal)
      num_uptime_internal++;
  }

  /* If that's enough, then stop now. */
  if (num >= MAX_UNUSED_OPEN_CIRCUITS)
    return; /* we already have many, making more probably will hurt */

  /* Second, see if we need any more exit circuits. */
  /* check if we know of a port that's been requested recently
   * and no circuit is currently available that can handle it. */
  if (!circuit_all_predicted_ports_handled(now, &port_needs_uptime,
                                           &port_needs_capacity)) {
    if (port_needs_uptime)
      flags |= CIRCLAUNCH_NEED_UPTIME;
    if (port_needs_capacity)
      flags |= CIRCLAUNCH_NEED_CAPACITY;
    log_info(LD_CIRC,
             "Have %d clean circs (%d internal), need another exit circ.",
             num, num_internal);
    circuit_launch(CIRCUIT_PURPOSE_C_GENERAL, flags);
    return;
  }

  /* Third, see if we need any more hidden service (server) circuits. */
  if (num_rend_services() && num_uptime_internal < 3) {
    flags = (CIRCLAUNCH_NEED_CAPACITY | CIRCLAUNCH_NEED_UPTIME |
             CIRCLAUNCH_IS_INTERNAL);
    log_info(LD_CIRC,
             "Have %d clean circs (%d internal), need another internal "
             "circ for my hidden service.",
             num, num_internal);
    circuit_launch(CIRCUIT_PURPOSE_C_GENERAL, flags);
    return;
  }

  /* Fourth, see if we need any more hidden service (client) circuits. */
  if (rep_hist_get_predicted_internal(now, &hidserv_needs_uptime,
                                      &hidserv_needs_capacity) &&
      ((num_uptime_internal<2 && hidserv_needs_uptime) ||
        num_internal<2)) {
    if (hidserv_needs_uptime)
      flags |= CIRCLAUNCH_NEED_UPTIME;
    if (hidserv_needs_capacity)
      flags |= CIRCLAUNCH_NEED_CAPACITY;
    flags |= CIRCLAUNCH_IS_INTERNAL;
    log_info(LD_CIRC,
             "Have %d clean circs (%d uptime-internal, %d internal), need"
             " another hidden service circ.",
             num, num_uptime_internal, num_internal);
    circuit_launch(CIRCUIT_PURPOSE_C_GENERAL, flags);
    return;
  }

  /* Finally, check to see if we still need more circuits to learn
   * a good build timeout. But if we're close to our max number we
   * want, don't do another -- we want to leave a few slots open so
   * we can still build circuits preemptively as needed. */
  if (num < MAX_UNUSED_OPEN_CIRCUITS-2 &&
      ! circuit_build_times_disabled() &&
      circuit_build_times_needs_circuits_now(get_circuit_build_times())) {
    flags = CIRCLAUNCH_NEED_CAPACITY;
    log_info(LD_CIRC,
             "Have %d clean circs need another buildtime test circ.", num);
    circuit_launch(CIRCUIT_PURPOSE_C_GENERAL, flags);
    return;
  }
}

/** Build a new test circuit every 5 minutes */
#define TESTING_CIRCUIT_INTERVAL 300

/** This function is called once a second, if router_have_min_dir_info() is
 * true. Its job is to make sure all services we offer have enough circuits
 * available. Some services just want enough circuits for current tasks,
 * whereas others want a minimum set of idle circuits hanging around.
 */
void
circuit_build_needed_circs(time_t now)
{
  const or_options_t *options = get_options();

  /* launch a new circ for any pending streams that need one */
  connection_ap_attach_pending();

  /* make sure any hidden services have enough intro points */
  rend_services_introduce();

  circuit_expire_old_circs_as_needed(now);

  if (!options->DisablePredictedCircuits)
    circuit_predict_and_launch_new();
}

/**
 * Called once a second either directly or from
 * circuit_build_needed_circs(). As appropriate (once per NewCircuitPeriod)
 * resets failure counts and expires old circuits.
 */
void
circuit_expire_old_circs_as_needed(time_t now)
{
  static time_t time_to_expire_and_reset = 0;

  if (time_to_expire_and_reset < now) {
    circuit_reset_failure_count(1);
    time_to_expire_and_reset = now + get_options()->NewCircuitPeriod;
    if (proxy_mode(get_options()))
      addressmap_clean(now);
    circuit_expire_old_circuits_clientside();

#if 0 /* disable for now, until predict-and-launch-new can cull leftovers */

    /* If we ever re-enable, this has to move into
     * circuit_build_needed_circs */

    circ = circuit_get_youngest_clean_open(CIRCUIT_PURPOSE_C_GENERAL);
    if (get_options()->RunTesting &&
        circ &&
        circ->timestamp_began.tv_sec + TESTING_CIRCUIT_INTERVAL < now) {
      log_fn(LOG_INFO,"Creating a new testing circuit.");
      circuit_launch(CIRCUIT_PURPOSE_C_GENERAL, 0);
    }
#endif
  }
}

/** If the stream <b>conn</b> is a member of any of the linked
 * lists of <b>circ</b>, then remove it from the list.
 */
void
circuit_detach_stream(circuit_t *circ, edge_connection_t *conn)
{
  edge_connection_t *prevconn;

  tor_assert(circ);
  tor_assert(conn);

  if (conn->base_.type == CONN_TYPE_AP) {
    entry_connection_t *entry_conn = EDGE_TO_ENTRY_CONN(conn);
    entry_conn->may_use_optimistic_data = 0;
  }
  conn->cpath_layer = NULL; /* don't keep a stale pointer */
  conn->on_circuit = NULL;

  if (CIRCUIT_IS_ORIGIN(circ)) {
    origin_circuit_t *origin_circ = TO_ORIGIN_CIRCUIT(circ);
    if (conn == origin_circ->p_streams) {
      origin_circ->p_streams = conn->next_stream;
      return;
    }

    for (prevconn = origin_circ->p_streams;
         prevconn && prevconn->next_stream && prevconn->next_stream != conn;
         prevconn = prevconn->next_stream)
      ;
    if (prevconn && prevconn->next_stream) {
      prevconn->next_stream = conn->next_stream;
      return;
    }
  } else {
    or_circuit_t *or_circ = TO_OR_CIRCUIT(circ);
    if (conn == or_circ->n_streams) {
      or_circ->n_streams = conn->next_stream;
      return;
    }
    if (conn == or_circ->resolving_streams) {
      or_circ->resolving_streams = conn->next_stream;
      return;
    }

    for (prevconn = or_circ->n_streams;
         prevconn && prevconn->next_stream && prevconn->next_stream != conn;
         prevconn = prevconn->next_stream)
      ;
    if (prevconn && prevconn->next_stream) {
      prevconn->next_stream = conn->next_stream;
      return;
    }

    for (prevconn = or_circ->resolving_streams;
         prevconn && prevconn->next_stream && prevconn->next_stream != conn;
         prevconn = prevconn->next_stream)
      ;
    if (prevconn && prevconn->next_stream) {
      prevconn->next_stream = conn->next_stream;
      return;
    }
  }

  log_warn(LD_BUG,"Edge connection not in circuit's list.");
  /* Don't give an error here; it's harmless. */
  tor_fragile_assert();
}

/** If we haven't yet decided on a good timeout value for circuit
 * building, we close idles circuits aggressively so we can get more
 * data points. */
#define IDLE_TIMEOUT_WHILE_LEARNING (10*60)

/** Find each circuit that has been unused for too long, or dirty
 * for too long and has no streams on it: mark it for close.
 */
static void
circuit_expire_old_circuits_clientside(void)
{
  circuit_t *circ;
  struct timeval cutoff, now;

  tor_gettimeofday(&now);
  cutoff = now;
  last_expired_clientside_circuits = now.tv_sec;

  if (! circuit_build_times_disabled() &&
      circuit_build_times_needs_circuits(get_circuit_build_times())) {
    /* Circuits should be shorter lived if we need more of them
     * for learning a good build timeout */
    cutoff.tv_sec -= IDLE_TIMEOUT_WHILE_LEARNING;
  } else {
    cutoff.tv_sec -= get_options()->CircuitIdleTimeout;
  }

  TOR_LIST_FOREACH(circ, circuit_get_global_list(), head) {
    if (circ->marked_for_close || !CIRCUIT_IS_ORIGIN(circ))
      continue;
    /* If the circuit has been dirty for too long, and there are no streams
     * on it, mark it for close.
     */
    if (circ->timestamp_dirty &&
        circ->timestamp_dirty + get_options()->MaxCircuitDirtiness <
          now.tv_sec &&
        !TO_ORIGIN_CIRCUIT(circ)->p_streams /* nothing attached */ ) {
      log_debug(LD_CIRC, "Closing n_circ_id %u (dirty %ld sec ago, "
                "purpose %d)",
                (unsigned)circ->n_circ_id,
                (long)(now.tv_sec - circ->timestamp_dirty),
                circ->purpose);
      /* Don't do this magic for testing circuits. Their death is governed
       * by circuit_expire_building */
      if (circ->purpose != CIRCUIT_PURPOSE_PATH_BIAS_TESTING)
        circuit_mark_for_close(circ, END_CIRC_REASON_FINISHED);
    } else if (!circ->timestamp_dirty && circ->state == CIRCUIT_STATE_OPEN) {
      if (timercmp(&circ->timestamp_began, &cutoff, <)) {
        if (circ->purpose == CIRCUIT_PURPOSE_C_GENERAL ||
                circ->purpose == CIRCUIT_PURPOSE_C_MEASURE_TIMEOUT ||
                circ->purpose == CIRCUIT_PURPOSE_S_ESTABLISH_INTRO ||
                circ->purpose == CIRCUIT_PURPOSE_TESTING ||
                (circ->purpose >= CIRCUIT_PURPOSE_C_INTRODUCING &&
                circ->purpose <= CIRCUIT_PURPOSE_C_REND_READY_INTRO_ACKED) ||
                circ->purpose == CIRCUIT_PURPOSE_S_CONNECT_REND) {
          log_debug(LD_CIRC,
                    "Closing circuit that has been unused for %ld msec.",
                    tv_mdiff(&circ->timestamp_began, &now));
          circuit_mark_for_close(circ, END_CIRC_REASON_FINISHED);
        } else if (!TO_ORIGIN_CIRCUIT(circ)->is_ancient) {
          /* Server-side rend joined circuits can end up really old, because
           * they are reused by clients for longer than normal. The client
           * controls their lifespan. (They never become dirty, because
           * connection_exit_begin_conn() never marks anything as dirty.)
           * Similarly, server-side intro circuits last a long time. */
          if (circ->purpose != CIRCUIT_PURPOSE_S_REND_JOINED &&
              circ->purpose != CIRCUIT_PURPOSE_S_INTRO) {
            log_notice(LD_CIRC,
                       "Ancient non-dirty circuit %d is still around after "
                       "%ld milliseconds. Purpose: %d (%s)",
                       TO_ORIGIN_CIRCUIT(circ)->global_identifier,
                       tv_mdiff(&circ->timestamp_began, &now),
                       circ->purpose,
                       circuit_purpose_to_string(circ->purpose));
            TO_ORIGIN_CIRCUIT(circ)->is_ancient = 1;
          }
        }
      }
    }
  }
}

/** How long do we wait before killing circuits with the properties
 * described below?
 *
 * Probably we could choose a number here as low as 5 to 10 seconds,
 * since these circs are used for begindir, and a) generally you either
 * ask another begindir question right after or you don't for a long time,
 * b) clients at least through 0.2.1.x choose from the whole set of
 * directory mirrors at each choice, and c) re-establishing a one-hop
 * circuit via create-fast is a light operation assuming the TLS conn is
 * still there.
 *
 * I expect "b" to go away one day when we move to using directory
 * guards, but I think "a" and "c" are good enough reasons that a low
 * number is safe even then.
 */
#define IDLE_ONE_HOP_CIRC_TIMEOUT 60

/** Find each non-origin circuit that has been unused for too long,
 * has no streams on it, used a create_fast, and ends here: mark it
 * for close.
 */
void
circuit_expire_old_circuits_serverside(time_t now)
{
  circuit_t *circ;
  or_circuit_t *or_circ;
  time_t cutoff = now - IDLE_ONE_HOP_CIRC_TIMEOUT;

  TOR_LIST_FOREACH(circ, circuit_get_global_list(), head) {
    if (circ->marked_for_close || CIRCUIT_IS_ORIGIN(circ))
      continue;
    or_circ = TO_OR_CIRCUIT(circ);
    /* If the circuit has been idle for too long, and there are no streams
     * on it, and it ends here, and it used a create_fast, mark it for close.
     */
    if (or_circ->is_first_hop && !circ->n_chan &&
        !or_circ->n_streams && !or_circ->resolving_streams &&
        or_circ->p_chan &&
        channel_when_last_xmit(or_circ->p_chan) <= cutoff) {
      log_info(LD_CIRC, "Closing circ_id %u (empty %d secs ago)",
               (unsigned)or_circ->p_circ_id,
               (int)(now - channel_when_last_xmit(or_circ->p_chan)));
      circuit_mark_for_close(circ, END_CIRC_REASON_FINISHED);
    }
  }
}

/** Number of testing circuits we want open before testing our bandwidth. */
#define NUM_PARALLEL_TESTING_CIRCS 4

/** True iff we've ever had enough testing circuits open to test our
 * bandwidth. */
static int have_performed_bandwidth_test = 0;

/** Reset have_performed_bandwidth_test, so we'll start building
 * testing circuits again so we can exercise our bandwidth. */
void
reset_bandwidth_test(void)
{
  have_performed_bandwidth_test = 0;
}

/** Return 1 if we've already exercised our bandwidth, or if we
 * have fewer than NUM_PARALLEL_TESTING_CIRCS testing circuits
 * established or on the way. Else return 0.
 */
int
circuit_enough_testing_circs(void)
{
  circuit_t *circ;
  int num = 0;

  if (have_performed_bandwidth_test)
    return 1;

  TOR_LIST_FOREACH(circ, circuit_get_global_list(), head) {
    if (!circ->marked_for_close && CIRCUIT_IS_ORIGIN(circ) &&
        circ->purpose == CIRCUIT_PURPOSE_TESTING &&
        circ->state == CIRCUIT_STATE_OPEN)
      num++;
  }
  return num >= NUM_PARALLEL_TESTING_CIRCS;
}

/** A testing circuit has completed. Take whatever stats we want.
 * Noticing reachability is taken care of in onionskin_answer(),
 * so there's no need to record anything here. But if we still want
 * to do the bandwidth test, and we now have enough testing circuits
 * open, do it.
 */
static void
circuit_testing_opened(origin_circuit_t *circ)
{
  if (have_performed_bandwidth_test ||
      !check_whether_orport_reachable()) {
    /* either we've already done everything we want with testing circuits,
     * or this testing circuit became open due to a fluke, e.g. we picked
     * a last hop where we already had the connection open due to an
     * outgoing local circuit. */
    circuit_mark_for_close(TO_CIRCUIT(circ), END_CIRC_AT_ORIGIN);
  } else if (circuit_enough_testing_circs()) {
    router_perform_bandwidth_test(NUM_PARALLEL_TESTING_CIRCS, time(NULL));
    have_performed_bandwidth_test = 1;
  } else
    consider_testing_reachability(1, 0);
}

/** A testing circuit has failed to build. Take whatever stats we want. */
static void
circuit_testing_failed(origin_circuit_t *circ, int at_last_hop)
{
  if (server_mode(get_options()) && check_whether_orport_reachable())
    return;

  log_info(LD_GENERAL,
           "Our testing circuit (to see if your ORPort is reachable) "
           "has failed. I'll try again later.");

  /* These aren't used yet. */
  (void)circ;
  (void)at_last_hop;
}

/** The circuit <b>circ</b> has just become open. Take the next
 * step: for rendezvous circuits, we pass circ to the appropriate
 * function in rendclient or rendservice. For general circuits, we
 * call connection_ap_attach_pending, which looks for pending streams
 * that could use circ.
 */
void
circuit_has_opened(origin_circuit_t *circ)
{
  control_event_circuit_status(circ, CIRC_EVENT_BUILT, 0);

  /* Remember that this circuit has finished building. Now if we start
   * it building again later (e.g. by extending it), we will know not
   * to consider its build time. */
  circ->has_opened = 1;

  switch (TO_CIRCUIT(circ)->purpose) {
    case CIRCUIT_PURPOSE_C_ESTABLISH_REND:
      rend_client_rendcirc_has_opened(circ);
      /* Start building an intro circ if we don't have one yet. */
      connection_ap_attach_pending();
      /* This isn't a call to circuit_try_attaching_streams because a
       * circuit in _C_ESTABLISH_REND state isn't connected to its
       * hidden service yet, thus we can't attach streams to it yet,
       * thus circuit_try_attaching_streams would always clear the
       * circuit's isolation state.  circuit_try_attaching_streams is
       * called later, when the rend circ enters _C_REND_JOINED
       * state. */
      break;
    case CIRCUIT_PURPOSE_C_INTRODUCING:
      rend_client_introcirc_has_opened(circ);
      break;
    case CIRCUIT_PURPOSE_C_GENERAL:
      /* Tell any AP connections that have been waiting for a new
       * circuit that one is ready. */
      circuit_try_attaching_streams(circ);
      break;
    case CIRCUIT_PURPOSE_S_ESTABLISH_INTRO:
      /* at Bob, waiting for introductions */
      rend_service_intro_has_opened(circ);
      break;
    case CIRCUIT_PURPOSE_S_CONNECT_REND:
      /* at Bob, connecting to rend point */
      rend_service_rendezvous_has_opened(circ);
      break;
    case CIRCUIT_PURPOSE_TESTING:
      circuit_testing_opened(circ);
      break;
    /* default:
     * This won't happen in normal operation, but might happen if the
     * controller did it. Just let it slide. */
  }
}

/** If the stream-isolation state of <b>circ</b> can be cleared, clear
 * it.  Return non-zero iff <b>circ</b>'s isolation state was cleared. */
static int
circuit_try_clearing_isolation_state(origin_circuit_t *circ)
{
  if (/* The circuit may have become non-open if it was cannibalized.*/
      circ->base_.state == CIRCUIT_STATE_OPEN &&
      /* If !isolation_values_set, there is nothing to clear. */
      circ->isolation_values_set &&
      /* It's not legal to clear a circuit's isolation info if it's ever had
       * streams attached */
      !circ->isolation_any_streams_attached) {
    /* If we have any isolation information set on this circuit, and
     * we didn't manage to attach any streams to it, then we can
     * and should clear it and try again. */
    circuit_clear_isolation(circ);
    return 1;
  } else {
    return 0;
  }
}

/** Called when a circuit becomes ready for streams to be attached to
 * it. */
void
circuit_try_attaching_streams(origin_circuit_t *circ)
{
  /* Attach streams to this circuit if we can. */
  connection_ap_attach_pending();

  /* The call to circuit_try_clearing_isolation_state here will do
   * nothing and return 0 if we didn't attach any streams to circ
   * above. */
  if (circuit_try_clearing_isolation_state(circ)) {
    /* Maybe *now* we can attach some streams to this circuit. */
    connection_ap_attach_pending();
  }
}

/** Called whenever a circuit could not be successfully built.
 */
void
circuit_build_failed(origin_circuit_t *circ)
{
  channel_t *n_chan = NULL;
  /* we should examine circ and see if it failed because of
   * the last hop or an earlier hop. then use this info below.
   */
  int failed_at_last_hop = 0;
  /* If the last hop isn't open, and the second-to-last is, we failed
   * at the last hop. */
  if (circ->cpath &&
      circ->cpath->prev->state != CPATH_STATE_OPEN &&
      circ->cpath->prev->prev->state == CPATH_STATE_OPEN) {
    failed_at_last_hop = 1;
  }
  if (circ->cpath &&
      circ->cpath->state != CPATH_STATE_OPEN &&
      ! circ->base_.received_destroy) {
    /* We failed at the first hop for some reason other than a DESTROY cell.
     * If there's an OR connection to blame, blame it. Also, avoid this relay
     * for a while, and fail any one-hop directory fetches destined for it. */
    const char *n_chan_id = circ->cpath->extend_info->identity_digest;
    int already_marked = 0;
    if (circ->base_.n_chan) {
      n_chan = circ->base_.n_chan;

      if (n_chan->is_bad_for_new_circs) {
        /* We only want to blame this router when a fresh healthy
         * connection fails. So don't mark this router as newly failed,
         * since maybe this was just an old circuit attempt that's
         * finally timing out now. Also, there's no need to blow away
         * circuits/streams/etc, since the failure of an unhealthy conn
         * doesn't tell us much about whether a healthy conn would
         * succeed. */
        already_marked = 1;
      }
      log_info(LD_OR,
               "Our circuit failed to get a response from the first hop "
               "(%s). I'm going to try to rotate to a better connection.",
               channel_get_canonical_remote_descr(n_chan));
      n_chan->is_bad_for_new_circs = 1;
    } else {
      log_info(LD_OR,
               "Our circuit died before the first hop with no connection");
    }
    if (n_chan_id && !already_marked) {
      entry_guard_register_connect_status(n_chan_id, 0, 1, time(NULL));
      /* if there are any one-hop streams waiting on this circuit, fail
       * them now so they can retry elsewhere. */
      connection_ap_fail_onehop(n_chan_id, circ->build_state);
    }
  }

  switch (circ->base_.purpose) {
    case CIRCUIT_PURPOSE_C_GENERAL:
      /* If we never built the circuit, note it as a failure. */
      circuit_increment_failure_count();
      if (failed_at_last_hop) {
        /* Make sure any streams that demand our last hop as their exit
         * know that it's unlikely to happen. */
        circuit_discard_optional_exit_enclaves(circ->cpath->prev->extend_info);
      }
      break;
    case CIRCUIT_PURPOSE_TESTING:
      circuit_testing_failed(circ, failed_at_last_hop);
      break;
    case CIRCUIT_PURPOSE_S_ESTABLISH_INTRO:
      /* at Bob, waiting for introductions */
      if (circ->base_.state != CIRCUIT_STATE_OPEN) {
        circuit_increment_failure_count();
      }
      /* no need to care here, because bob will rebuild intro
       * points periodically. */
      break;
    case CIRCUIT_PURPOSE_C_INTRODUCING:
      /* at Alice, connecting to intro point */
      /* Don't increment failure count, since Bob may have picked
       * the introduction point maliciously */
      /* Alice will pick a new intro point when this one dies, if
       * the stream in question still cares. No need to act here. */
      break;
    case CIRCUIT_PURPOSE_C_ESTABLISH_REND:
      /* at Alice, waiting for Bob */
      circuit_increment_failure_count();
      /* Alice will pick a new rend point when this one dies, if
       * the stream in question still cares. No need to act here. */
      break;
    case CIRCUIT_PURPOSE_S_CONNECT_REND:
      /* at Bob, connecting to rend point */
      /* Don't increment failure count, since Alice may have picked
       * the rendezvous point maliciously */
      log_info(LD_REND,
               "Couldn't connect to Alice's chosen rend point %s "
               "(%s hop failed).",
               escaped(build_state_get_exit_nickname(circ->build_state)),
               failed_at_last_hop?"last":"non-last");
      rend_service_relaunch_rendezvous(circ);
      break;
    /* default:
     * This won't happen in normal operation, but might happen if the
     * controller did it. Just let it slide. */
  }
}

/** Number of consecutive failures so far; should only be touched by
 * circuit_launch_new and circuit_*_failure_count.
 */
static int n_circuit_failures = 0;
/** Before the last time we called circuit_reset_failure_count(), were
 * there a lot of failures? */
static int did_circs_fail_last_period = 0;

/** Don't retry launching a new circuit if we try this many times with no
 * success. */
#define MAX_CIRCUIT_FAILURES 5

/** Launch a new circuit; see circuit_launch_by_extend_info() for
 * details on arguments. */
origin_circuit_t *
circuit_launch(uint8_t purpose, int flags)
{
  return circuit_launch_by_extend_info(purpose, NULL, flags);
}

/** Launch a new circuit with purpose <b>purpose</b> and exit node
 * <b>extend_info</b> (or NULL to select a random exit node).  If flags
 * contains CIRCLAUNCH_NEED_UPTIME, choose among routers with high uptime.  If
 * CIRCLAUNCH_NEED_CAPACITY is set, choose among routers with high bandwidth.
 * If CIRCLAUNCH_IS_INTERNAL is true, the last hop need not be an exit node.
 * If CIRCLAUNCH_ONEHOP_TUNNEL is set, the circuit will have only one hop.
 * Return the newly allocated circuit on success, or NULL on failure. */
origin_circuit_t *
circuit_launch_by_extend_info(uint8_t purpose,
                              extend_info_t *extend_info,
                              int flags)
{
  origin_circuit_t *circ;
  int onehop_tunnel = (flags & CIRCLAUNCH_ONEHOP_TUNNEL) != 0;

  if (!onehop_tunnel && !router_have_minimum_dir_info()) {
    log_debug(LD_CIRC,"Haven't fetched enough directory info yet; canceling "
              "circuit launch.");
    return NULL;
  }

  if ((extend_info || purpose != CIRCUIT_PURPOSE_C_GENERAL) &&
      purpose != CIRCUIT_PURPOSE_TESTING && !onehop_tunnel) {
    /* see if there are appropriate circs available to cannibalize. */
    /* XXX if we're planning to add a hop, perhaps we want to look for
     * internal circs rather than exit circs? -RD */
    circ = circuit_find_to_cannibalize(purpose, extend_info, flags);
    if (circ) {
      uint8_t old_purpose = circ->base_.purpose;
      struct timeval old_timestamp_began = circ->base_.timestamp_began;

      log_info(LD_CIRC,"Cannibalizing circ '%s' for purpose %d (%s)",
               build_state_get_exit_nickname(circ->build_state), purpose,
               circuit_purpose_to_string(purpose));

      if ((purpose == CIRCUIT_PURPOSE_S_CONNECT_REND ||
           purpose == CIRCUIT_PURPOSE_C_INTRODUCING) &&
          circ->path_state == PATH_STATE_BUILD_SUCCEEDED) {
        /* Path bias: Cannibalized rends pre-emptively count as a
         * successfully built but unused closed circuit. We don't
         * wait until the extend (or the close) because the rend
         * point could be malicious.
         *
         * Same deal goes for client side introductions. Clients
         * can be manipulated to connect repeatedly to them
         * (especially web clients).
         *
         * If we decide to probe the initial portion of these circs,
         * (up to the adversary's final hop), we need to remove this,
         * or somehow mark the circuit with a special path state.
         */

        /* This must be called before the purpose change */
        pathbias_check_close(circ, END_CIRC_REASON_FINISHED);
      }

      circuit_change_purpose(TO_CIRCUIT(circ), purpose);
      /* Reset the start date of this circ, else expire_building
       * will see it and think it's been trying to build since it
       * began.
       *
       * Technically, the code should reset this when the
       * create cell is finally sent, but we're close enough
       * here. */
      tor_gettimeofday(&circ->base_.timestamp_began);

      control_event_circuit_cannibalized(circ, old_purpose,
                                         &old_timestamp_began);

      switch (purpose) {
        case CIRCUIT_PURPOSE_C_ESTABLISH_REND:
        case CIRCUIT_PURPOSE_S_ESTABLISH_INTRO:
          /* it's ready right now */
          break;
        case CIRCUIT_PURPOSE_C_INTRODUCING:
        case CIRCUIT_PURPOSE_S_CONNECT_REND:
        case CIRCUIT_PURPOSE_C_GENERAL:
          /* need to add a new hop */
          tor_assert(extend_info);
          if (circuit_extend_to_new_exit(circ, extend_info) < 0)
            return NULL;
          break;
        default:
          log_warn(LD_BUG,
                   "unexpected purpose %d when cannibalizing a circ.",
                   purpose);
          tor_fragile_assert();
          return NULL;
      }
      return circ;
    }
  }

  if (did_circs_fail_last_period &&
      n_circuit_failures > MAX_CIRCUIT_FAILURES) {
    /* too many failed circs in a row. don't try. */
//    log_fn(LOG_INFO,"%d failures so far, not trying.",n_circuit_failures);
    return NULL;
  }

  /* try a circ. if it fails, circuit_mark_for_close will increment
   * n_circuit_failures */
  return circuit_establish_circuit(purpose, extend_info, flags);
}

/** Record another failure at opening a general circuit. When we have
 * too many, we'll stop trying for the remainder of this minute.
 */
static void
circuit_increment_failure_count(void)
{
  ++n_circuit_failures;
  log_debug(LD_CIRC,"n_circuit_failures now %d.",n_circuit_failures);
}

/** Reset the failure count for opening general circuits. This means
 * we will try MAX_CIRCUIT_FAILURES times more (if necessary) before
 * stopping again.
 */
void
circuit_reset_failure_count(int timeout)
{
  if (timeout && n_circuit_failures > MAX_CIRCUIT_FAILURES)
    did_circs_fail_last_period = 1;
  else
    did_circs_fail_last_period = 0;
  n_circuit_failures = 0;
}

/** Find an open circ that we're happy to use for <b>conn</b> and return 1. If
 * there isn't one, and there isn't one on the way, launch one and return
 * 0. If it will never work, return -1.
 *
 * Write the found or in-progress or launched circ into *circp.
 */
static int
circuit_get_open_circ_or_launch(entry_connection_t *conn,
                                uint8_t desired_circuit_purpose,
                                origin_circuit_t **circp)
{
  origin_circuit_t *circ;
  int check_exit_policy;
  int need_uptime, need_internal;
  int want_onehop;
  const or_options_t *options = get_options();

  tor_assert(conn);
  tor_assert(circp);
  tor_assert(ENTRY_TO_CONN(conn)->state == AP_CONN_STATE_CIRCUIT_WAIT);
  check_exit_policy =
      conn->socks_request->command == SOCKS_COMMAND_CONNECT &&
      !conn->use_begindir &&
      !connection_edge_is_rendezvous_stream(ENTRY_TO_EDGE_CONN(conn));
  want_onehop = conn->want_onehop;

  need_uptime = !conn->want_onehop && !conn->use_begindir &&
                smartlist_contains_int_as_string(options->LongLivedPorts,
                                          conn->socks_request->port);

  if (desired_circuit_purpose != CIRCUIT_PURPOSE_C_GENERAL)
    need_internal = 1;
  else if (conn->use_begindir || conn->want_onehop)
    need_internal = 1;
  else
    need_internal = 0;

  circ = circuit_get_best(conn, 1, desired_circuit_purpose,
                          need_uptime, need_internal);

  if (circ) {
    *circp = circ;
    return 1; /* we're happy */
  }

  if (!want_onehop && !router_have_minimum_dir_info()) {
    if (!connection_get_by_type(CONN_TYPE_DIR)) {
      int severity = LOG_NOTICE;
      /* FFFF if this is a tunneled directory fetch, don't yell
       * as loudly. the user doesn't even know it's happening. */
      if (entry_list_is_constrained(options) &&
          entries_known_but_down(options)) {
        log_fn(severity, LD_APP|LD_DIR,
               "Application request when we haven't used client functionality "
               "lately. Optimistically trying known %s again.",
               options->UseBridges ? "bridges" : "entrynodes");
        entries_retry_all(options);
      } else if (!options->UseBridges || any_bridge_descriptors_known()) {
        log_fn(severity, LD_APP|LD_DIR,
               "Application request when we haven't used client functionality "
               "lately. Optimistically trying directory fetches again.");
        routerlist_retry_directory_downloads(time(NULL));
      }
    }
    /* the stream will be dealt with when router_have_minimum_dir_info becomes
     * 1, or when all directory attempts fail and directory_all_unreachable()
     * kills it.
     */
    return 0;
  }

  /* Do we need to check exit policy? */
  if (check_exit_policy) {
    if (!conn->chosen_exit_name) {
      struct in_addr in;
      tor_addr_t addr, *addrp=NULL;
      if (tor_inet_aton(conn->socks_request->address, &in)) {
        tor_addr_from_in(&addr, &in);
        addrp = &addr;
      }
      if (router_exit_policy_all_nodes_reject(addrp,
                                              conn->socks_request->port,
                                              need_uptime)) {
        log_notice(LD_APP,
                   "No Tor server allows exit to %s:%d. Rejecting.",
                   safe_str_client(conn->socks_request->address),
                   conn->socks_request->port);
        return -1;
      }
    } else {
      /* XXXX024 Duplicates checks in connection_ap_handshake_attach_circuit:
       * refactor into a single function? */
      const node_t *node = node_get_by_nickname(conn->chosen_exit_name, 1);
      int opt = conn->chosen_exit_optional;
      if (node && !connection_ap_can_use_exit(conn, node)) {
        log_fn(opt ? LOG_INFO : LOG_WARN, LD_APP,
               "Requested exit point '%s' is excluded or "
               "would refuse request. %s.",
               conn->chosen_exit_name, opt ? "Trying others" : "Closing");
        if (opt) {
          conn->chosen_exit_optional = 0;
          tor_free(conn->chosen_exit_name);
          /* Try again. */
          return circuit_get_open_circ_or_launch(conn,
                                                 desired_circuit_purpose,
                                                 circp);
        }
        return -1;
      }
    }
  }

  /* is one already on the way? */
  circ = circuit_get_best(conn, 0, desired_circuit_purpose,
                          need_uptime, need_internal);
  if (circ)
    log_debug(LD_CIRC, "one on the way!");
  if (!circ) {
    extend_info_t *extend_info=NULL;
    uint8_t new_circ_purpose;
    const int n_pending = count_pending_general_client_circuits();

    if (n_pending >= options->MaxClientCircuitsPending) {
      static ratelim_t delay_limit = RATELIM_INIT(10*60);
      char *m;
      if ((m = rate_limit_log(&delay_limit, approx_time()))) {
        log_notice(LD_APP, "We'd like to launch a circuit to handle a "
                   "connection, but we already have %d general-purpose client "
                   "circuits pending. Waiting until some finish.%s",
                   n_pending, m);
        tor_free(m);
      }
      return 0;
    }

    if (desired_circuit_purpose == CIRCUIT_PURPOSE_C_INTRODUCE_ACK_WAIT) {
      /* need to pick an intro point */
      rend_data_t *rend_data = ENTRY_TO_EDGE_CONN(conn)->rend_data;
      tor_assert(rend_data);
      extend_info = rend_client_get_random_intro(rend_data);
      if (!extend_info) {
        log_info(LD_REND,
                 "No intro points for '%s': re-fetching service descriptor.",
                 safe_str_client(rend_data->onion_address));
        rend_client_refetch_v2_renddesc(rend_data);
        ENTRY_TO_CONN(conn)->state = AP_CONN_STATE_RENDDESC_WAIT;
        return 0;
      }
      log_info(LD_REND,"Chose %s as intro point for '%s'.",
               extend_info_describe(extend_info),
               safe_str_client(rend_data->onion_address));
    }

    /* If we have specified a particular exit node for our
     * connection, then be sure to open a circuit to that exit node.
     */
    if (desired_circuit_purpose == CIRCUIT_PURPOSE_C_GENERAL) {
      if (conn->chosen_exit_name) {
        const node_t *r;
        int opt = conn->chosen_exit_optional;
        r = node_get_by_nickname(conn->chosen_exit_name, 1);
        if (r && node_has_descriptor(r)) {
          /* We might want to connect to an IPv6 bridge for loading
             descriptors so we use the preferred address rather than
             the primary.  */
          extend_info = extend_info_from_node(r, conn->want_onehop ? 1 : 0);
        } else {
          log_debug(LD_DIR, "considering %d, %s",
                    want_onehop, conn->chosen_exit_name);
          if (want_onehop && conn->chosen_exit_name[0] == '$') {
            /* We're asking for a one-hop circuit to a router that
             * we don't have a routerinfo about. Make up an extend_info. */
            char digest[DIGEST_LEN];
            char *hexdigest = conn->chosen_exit_name+1;
            tor_addr_t addr;
            if (strlen(hexdigest) < HEX_DIGEST_LEN ||
                base16_decode(digest,DIGEST_LEN,hexdigest,HEX_DIGEST_LEN)<0) {
              log_info(LD_DIR, "Broken exit digest on tunnel conn. Closing.");
              return -1;
            }
            if (tor_addr_parse(&addr, conn->socks_request->address) < 0) {
              log_info(LD_DIR, "Broken address %s on tunnel conn. Closing.",
                       escaped_safe_str_client(conn->socks_request->address));
              return -1;
            }
            extend_info = extend_info_new(conn->chosen_exit_name+1,
                                          digest, NULL, NULL, &addr,
                                          conn->socks_request->port);
          } else {
            /* We will need an onion key for the router, and we
             * don't have one. Refuse or relax requirements. */
            log_fn(opt ? LOG_INFO : LOG_WARN, LD_APP,
                   "Requested exit point '%s' is not known. %s.",
                   conn->chosen_exit_name, opt ? "Trying others" : "Closing");
            if (opt) {
              conn->chosen_exit_optional = 0;
              tor_free(conn->chosen_exit_name);
              /* Try again with no requested exit */
              return circuit_get_open_circ_or_launch(conn,
                                                     desired_circuit_purpose,
                                                     circp);
            }
            return -1;
          }
        }
      }
    }

    if (desired_circuit_purpose == CIRCUIT_PURPOSE_C_REND_JOINED)
      new_circ_purpose = CIRCUIT_PURPOSE_C_ESTABLISH_REND;
    else if (desired_circuit_purpose == CIRCUIT_PURPOSE_C_INTRODUCE_ACK_WAIT)
      new_circ_purpose = CIRCUIT_PURPOSE_C_INTRODUCING;
    else
      new_circ_purpose = desired_circuit_purpose;

    if (options->Tor2webMode &&
        (new_circ_purpose == CIRCUIT_PURPOSE_C_ESTABLISH_REND ||
         new_circ_purpose == CIRCUIT_PURPOSE_C_INTRODUCING)) {
      want_onehop = 1;
    }

    {
      int flags = CIRCLAUNCH_NEED_CAPACITY;
      if (want_onehop) flags |= CIRCLAUNCH_ONEHOP_TUNNEL;
      if (need_uptime) flags |= CIRCLAUNCH_NEED_UPTIME;
      if (need_internal) flags |= CIRCLAUNCH_IS_INTERNAL;
      circ = circuit_launch_by_extend_info(new_circ_purpose, extend_info,
                                           flags);
    }

    extend_info_free(extend_info);

    if (desired_circuit_purpose == CIRCUIT_PURPOSE_C_GENERAL) {
      /* We just caused a circuit to get built because of this stream.
       * If this stream has caused a _lot_ of circuits to be built, that's
       * a bad sign: we should tell the user. */
      if (conn->num_circuits_launched < NUM_CIRCUITS_LAUNCHED_THRESHOLD &&
          ++conn->num_circuits_launched == NUM_CIRCUITS_LAUNCHED_THRESHOLD)
        log_info(LD_CIRC, "The application request to %s:%d has launched "
                 "%d circuits without finding one it likes.",
                 escaped_safe_str_client(conn->socks_request->address),
                 conn->socks_request->port,
                 conn->num_circuits_launched);
    } else {
      /* help predict this next time */
      rep_hist_note_used_internal(time(NULL), need_uptime, 1);
      if (circ) {
        /* write the service_id into circ */
        circ->rend_data = rend_data_dup(ENTRY_TO_EDGE_CONN(conn)->rend_data);
        if (circ->base_.purpose == CIRCUIT_PURPOSE_C_ESTABLISH_REND &&
            circ->base_.state == CIRCUIT_STATE_OPEN)
          rend_client_rendcirc_has_opened(circ);
      }
    }
  } /* endif (!circ) */
  if (circ) {
    /* Mark the circuit with the isolation fields for this connection.
     * When the circuit arrives, we'll clear these flags: this is
     * just some internal bookkeeping to make sure that we have
     * launched enough circuits.
     */
    connection_edge_update_circuit_isolation(conn, circ, 0);
  } else {
    log_info(LD_APP,
             "No safe circuit (purpose %d) ready for edge "
             "connection; delaying.",
             desired_circuit_purpose);
  }
  *circp = circ;
  return 0;
}

/** Return true iff <b>crypt_path</b> is one of the crypt_paths for
 * <b>circ</b>. */
static int
cpath_is_on_circuit(origin_circuit_t *circ, crypt_path_t *crypt_path)
{
  crypt_path_t *cpath, *cpath_next = NULL;
  for (cpath = circ->cpath; cpath_next != circ->cpath; cpath = cpath_next) {
    cpath_next = cpath->next;
    if (crypt_path == cpath)
      return 1;
  }
  return 0;
}

/** Return true iff client-side optimistic data is supported. */
static int
optimistic_data_enabled(void)
{
  const or_options_t *options = get_options();
  if (options->OptimisticData < 0) {
    /* XXX023 consider having auto default to 1 rather than 0 before
     * the 0.2.3 branch goes stable. See bug 3617. -RD */
    const int32_t enabled =
      networkstatus_get_param(NULL, "UseOptimisticData", 0, 0, 1);
    return (int)enabled;
  }
  return options->OptimisticData;
}

/** Attach the AP stream <b>apconn</b> to circ's linked list of
 * p_streams. Also set apconn's cpath_layer to <b>cpath</b>, or to the last
 * hop in circ's cpath if <b>cpath</b> is NULL.
 */
static void
link_apconn_to_circ(entry_connection_t *apconn, origin_circuit_t *circ,
                    crypt_path_t *cpath)
{
  const node_t *exitnode;

  /* add it into the linked list of streams on this circuit */
  log_debug(LD_APP|LD_CIRC, "attaching new conn to circ. n_circ_id %u.",
            (unsigned)circ->base_.n_circ_id);
  /* reset it, so we can measure circ timeouts */
  ENTRY_TO_CONN(apconn)->timestamp_lastread = time(NULL);
  ENTRY_TO_EDGE_CONN(apconn)->next_stream = circ->p_streams;
  ENTRY_TO_EDGE_CONN(apconn)->on_circuit = TO_CIRCUIT(circ);
  /* assert_connection_ok(conn, time(NULL)); */
  circ->p_streams = ENTRY_TO_EDGE_CONN(apconn);

  if (connection_edge_is_rendezvous_stream(ENTRY_TO_EDGE_CONN(apconn))) {
    /* We are attaching a stream to a rendezvous circuit.  That means
     * that an attempt to connect to a hidden service just
     * succeeded.  Tell rendclient.c. */
    rend_client_note_connection_attempt_ended(
                    ENTRY_TO_EDGE_CONN(apconn)->rend_data->onion_address);
  }

  if (cpath) { /* we were given one; use it */
    tor_assert(cpath_is_on_circuit(circ, cpath));
  } else {
    /* use the last hop in the circuit */
    tor_assert(circ->cpath);
    tor_assert(circ->cpath->prev);
    tor_assert(circ->cpath->prev->state == CPATH_STATE_OPEN);
    cpath = circ->cpath->prev;
  }
  ENTRY_TO_EDGE_CONN(apconn)->cpath_layer = cpath;

  circ->isolation_any_streams_attached = 1;
  connection_edge_update_circuit_isolation(apconn, circ, 0);

  /* See if we can use optimistic data on this circuit */
  if (cpath->extend_info &&
      (exitnode = node_get_by_id(cpath->extend_info->identity_digest)) &&
      exitnode->rs) {
    /* Okay; we know what exit node this is. */
    if (optimistic_data_enabled() &&
        circ->base_.purpose == CIRCUIT_PURPOSE_C_GENERAL &&
        exitnode->rs->version_supports_optimistic_data)
      apconn->may_use_optimistic_data = 1;
    else
      apconn->may_use_optimistic_data = 0;
    log_info(LD_APP, "Looks like completed circuit to %s %s allow "
             "optimistic data for connection to %s",
             safe_str_client(node_describe(exitnode)),
             apconn->may_use_optimistic_data ? "does" : "doesn't",
             safe_str_client(apconn->socks_request->address));
  }
}

/** Return true iff <b>address</b> is matched by one of the entries in
 * TrackHostExits. */
int
hostname_in_track_host_exits(const or_options_t *options, const char *address)
{
  if (!options->TrackHostExits)
    return 0;
  SMARTLIST_FOREACH_BEGIN(options->TrackHostExits, const char *, cp) {
    if (cp[0] == '.') { /* match end */
      if (cp[1] == '\0' ||
          !strcasecmpend(address, cp) ||
          !strcasecmp(address, &cp[1]))
        return 1;
    } else if (strcasecmp(cp, address) == 0) {
      return 1;
    }
  } SMARTLIST_FOREACH_END(cp);
  return 0;
}

/** If an exit wasn't explicitly specified for <b>conn</b>, consider saving
 * the exit that we *did* choose for use by future connections to
 * <b>conn</b>'s destination.
 */
static void
consider_recording_trackhost(const entry_connection_t *conn,
                             const origin_circuit_t *circ)
{
  const or_options_t *options = get_options();
  char *new_address = NULL;
  char fp[HEX_DIGEST_LEN+1];

  /* Search the addressmap for this conn's destination. */
  /* If he's not in the address map.. */
  if (!options->TrackHostExits ||
      addressmap_have_mapping(conn->socks_request->address,
                              options->TrackHostExitsExpire))
    return; /* nothing to track, or already mapped */

  if (!hostname_in_track_host_exits(options, conn->socks_request->address) ||
      !circ->build_state->chosen_exit)
    return;

  /* write down the fingerprint of the chosen exit, not the nickname,
   * because the chosen exit might not be named. */
  base16_encode(fp, sizeof(fp),
                circ->build_state->chosen_exit->identity_digest, DIGEST_LEN);

  /* Add this exit/hostname pair to the addressmap. */
  tor_asprintf(&new_address, "%s.%s.exit",
               conn->socks_request->address, fp);

  addressmap_register(conn->socks_request->address, new_address,
                      time(NULL) + options->TrackHostExitsExpire,
                      ADDRMAPSRC_TRACKEXIT, 0, 0);
}

/** Attempt to attach the connection <b>conn</b> to <b>circ</b>, and send a
 * begin or resolve cell as appropriate.  Return values are as for
 * connection_ap_handshake_attach_circuit.  The stream will exit from the hop
 * indicated by <b>cpath</b>, or from the last hop in circ's cpath if
 * <b>cpath</b> is NULL. */
int
connection_ap_handshake_attach_chosen_circuit(entry_connection_t *conn,
                                              origin_circuit_t *circ,
                                              crypt_path_t *cpath)
{
  connection_t *base_conn = ENTRY_TO_CONN(conn);
  tor_assert(conn);
  tor_assert(base_conn->state == AP_CONN_STATE_CIRCUIT_WAIT ||
             base_conn->state == AP_CONN_STATE_CONTROLLER_WAIT);
  tor_assert(conn->socks_request);
  tor_assert(circ);
  tor_assert(circ->base_.state == CIRCUIT_STATE_OPEN);

  base_conn->state = AP_CONN_STATE_CIRCUIT_WAIT;

  if (!circ->base_.timestamp_dirty)
    circ->base_.timestamp_dirty = time(NULL);

  pathbias_count_use_attempt(circ);

  link_apconn_to_circ(conn, circ, cpath);
  tor_assert(conn->socks_request);
  if (conn->socks_request->command == SOCKS_COMMAND_CONNECT) {
    if (!conn->use_begindir)
      consider_recording_trackhost(conn, circ);
    if (connection_ap_handshake_send_begin(conn) < 0)
      return -1;
  } else {
    if (connection_ap_handshake_send_resolve(conn) < 0)
      return -1;
  }

  return 1;
}

/** Try to find a safe live circuit for CONN_TYPE_AP connection conn. If
 * we don't find one: if conn cannot be handled by any known nodes,
 * warn and return -1 (conn needs to die, and is maybe already marked);
 * else launch new circuit (if necessary) and return 0.
 * Otherwise, associate conn with a safe live circuit, do the
 * right next step, and return 1.
 */
/* XXXX this function should mark for close whenever it returns -1;
 * its callers shouldn't have to worry about that. */
int
connection_ap_handshake_attach_circuit(entry_connection_t *conn)
{
  connection_t *base_conn = ENTRY_TO_CONN(conn);
  int retval;
  int conn_age;
  int want_onehop;

  tor_assert(conn);
  tor_assert(base_conn->state == AP_CONN_STATE_CIRCUIT_WAIT);
  tor_assert(conn->socks_request);
  want_onehop = conn->want_onehop;

  conn_age = (int)(time(NULL) - base_conn->timestamp_created);

  if (conn_age >= get_options()->SocksTimeout) {
    int severity = (tor_addr_is_null(&base_conn->addr) && !base_conn->port) ?
      LOG_INFO : LOG_NOTICE;
    log_fn(severity, LD_APP,
           "Tried for %d seconds to get a connection to %s:%d. Giving up.",
           conn_age, safe_str_client(conn->socks_request->address),
           conn->socks_request->port);
    return -1;
  }

  if (!connection_edge_is_rendezvous_stream(ENTRY_TO_EDGE_CONN(conn))) {
    /* we're a general conn */
    origin_circuit_t *circ=NULL;

    if (conn->chosen_exit_name) {
      const node_t *node = node_get_by_nickname(conn->chosen_exit_name, 1);
      int opt = conn->chosen_exit_optional;
      if (!node && !want_onehop) {
        /* We ran into this warning when trying to extend a circuit to a
         * hidden service directory for which we didn't have a router
         * descriptor. See flyspray task 767 for more details. We should
         * keep this in mind when deciding to use BEGIN_DIR cells for other
         * directory requests as well. -KL*/
        log_fn(opt ? LOG_INFO : LOG_WARN, LD_APP,
               "Requested exit point '%s' is not known. %s.",
               conn->chosen_exit_name, opt ? "Trying others" : "Closing");
        if (opt) {
          conn->chosen_exit_optional = 0;
          tor_free(conn->chosen_exit_name);
          return 0;
        }
        return -1;
      }
      if (node && !connection_ap_can_use_exit(conn, node)) {
        log_fn(opt ? LOG_INFO : LOG_WARN, LD_APP,
               "Requested exit point '%s' is excluded or "
               "would refuse request. %s.",
               conn->chosen_exit_name, opt ? "Trying others" : "Closing");
        if (opt) {
          conn->chosen_exit_optional = 0;
          tor_free(conn->chosen_exit_name);
          return 0;
        }
        return -1;
      }
    }

    /* find the circuit that we should use, if there is one. */
    retval = circuit_get_open_circ_or_launch(
        conn, CIRCUIT_PURPOSE_C_GENERAL, &circ);
    if (retval < 1) // XXX023 if we totally fail, this still returns 0 -RD
      return retval;

    log_debug(LD_APP|LD_CIRC,
              "Attaching apconn to circ %u (stream %d sec old).",
              (unsigned)circ->base_.n_circ_id, conn_age);
    /* print the circ's path, so people can figure out which circs are
     * sucking. */
    circuit_log_path(LOG_INFO,LD_APP|LD_CIRC,circ);

    /* We have found a suitable circuit for our conn. Hurray. */
    return connection_ap_handshake_attach_chosen_circuit(conn, circ, NULL);

  } else { /* we're a rendezvous conn */
    origin_circuit_t *rendcirc=NULL, *introcirc=NULL;

    tor_assert(!ENTRY_TO_EDGE_CONN(conn)->cpath_layer);

    /* start by finding a rendezvous circuit for us */

    retval = circuit_get_open_circ_or_launch(
       conn, CIRCUIT_PURPOSE_C_REND_JOINED, &rendcirc);
    if (retval < 0) return -1; /* failed */

    if (retval > 0) {
      tor_assert(rendcirc);
      /* one is already established, attach */
      log_info(LD_REND,
               "rend joined circ %d already here. attaching. "
               "(stream %d sec old)",
               (unsigned)rendcirc->base_.n_circ_id, conn_age);
      /* Mark rendezvous circuits as 'newly dirty' every time you use
       * them, since the process of rebuilding a rendezvous circ is so
       * expensive. There is a tradeoff between linkability and
       * feasibility, at this point.
       */
      rendcirc->base_.timestamp_dirty = time(NULL);

      /* We've also attempted to use them. If they fail, we need to
       * probe them for path bias */
      pathbias_count_use_attempt(rendcirc);

      link_apconn_to_circ(conn, rendcirc, NULL);
      if (connection_ap_handshake_send_begin(conn) < 0)
        return 0; /* already marked, let them fade away */
      return 1;
    }

    if (rendcirc && (rendcirc->base_.purpose ==
                     CIRCUIT_PURPOSE_C_REND_READY_INTRO_ACKED)) {
      log_info(LD_REND,
               "pending-join circ %u already here, with intro ack. "
               "Stalling. (stream %d sec old)",
               (unsigned)rendcirc->base_.n_circ_id, conn_age);
      return 0;
    }

    /* it's on its way. find an intro circ. */
    retval = circuit_get_open_circ_or_launch(
      conn, CIRCUIT_PURPOSE_C_INTRODUCE_ACK_WAIT, &introcirc);
    if (retval < 0) return -1; /* failed */

    if (retval > 0) {
      /* one has already sent the intro. keep waiting. */
      tor_assert(introcirc);
      log_info(LD_REND, "Intro circ %u present and awaiting ack (rend %u). "
               "Stalling. (stream %d sec old)",
               (unsigned)introcirc->base_.n_circ_id,
               rendcirc ? (unsigned)rendcirc->base_.n_circ_id : 0,
               conn_age);
      return 0;
    }

    /* now rendcirc and introcirc are each either undefined or not finished */

    if (rendcirc && introcirc &&
        rendcirc->base_.purpose == CIRCUIT_PURPOSE_C_REND_READY) {
      log_info(LD_REND,
               "ready rend circ %u already here (no intro-ack yet on "
               "intro %u). (stream %d sec old)",
               (unsigned)rendcirc->base_.n_circ_id,
               (unsigned)introcirc->base_.n_circ_id, conn_age);

      tor_assert(introcirc->base_.purpose == CIRCUIT_PURPOSE_C_INTRODUCING);
      if (introcirc->base_.state == CIRCUIT_STATE_OPEN) {
        log_info(LD_REND,"found open intro circ %u (rend %u); sending "
                 "introduction. (stream %d sec old)",
                 (unsigned)introcirc->base_.n_circ_id,
                 (unsigned)rendcirc->base_.n_circ_id,
                 conn_age);
        switch (rend_client_send_introduction(introcirc, rendcirc)) {
        case 0: /* success */
          rendcirc->base_.timestamp_dirty = time(NULL);
          introcirc->base_.timestamp_dirty = time(NULL);

          pathbias_count_use_attempt(introcirc);
          pathbias_count_use_attempt(rendcirc);

          assert_circuit_ok(TO_CIRCUIT(rendcirc));
          assert_circuit_ok(TO_CIRCUIT(introcirc));
          return 0;
        case -1: /* transient error */
          return 0;
        case -2: /* permanent error */
          return -1;
        default: /* oops */
          tor_fragile_assert();
          return -1;
        }
      }
    }

    log_info(LD_REND, "Intro (%u) and rend (%u) circs are not both ready. "
             "Stalling conn. (%d sec old)",
             introcirc ? (unsigned)introcirc->base_.n_circ_id : 0,
             rendcirc ? (unsigned)rendcirc->base_.n_circ_id : 0, conn_age);
    return 0;
  }
}

/** Change <b>circ</b>'s purpose to <b>new_purpose</b>. */
void
circuit_change_purpose(circuit_t *circ, uint8_t new_purpose)
{
  uint8_t old_purpose;
  /* Don't allow an OR circ to become an origin circ or vice versa. */
  tor_assert(!!(CIRCUIT_IS_ORIGIN(circ)) ==
             !!(CIRCUIT_PURPOSE_IS_ORIGIN(new_purpose)));

  if (circ->purpose == new_purpose) return;

  if (CIRCUIT_IS_ORIGIN(circ)) {
    char old_purpose_desc[80] = "";

    strncpy(old_purpose_desc, circuit_purpose_to_string(circ->purpose), 80-1);
    old_purpose_desc[80-1] = '\0';

    log_debug(LD_CIRC,
              "changing purpose of origin circ %d "
              "from \"%s\" (%d) to \"%s\" (%d)",
              TO_ORIGIN_CIRCUIT(circ)->global_identifier,
              old_purpose_desc,
              circ->purpose,
              circuit_purpose_to_string(new_purpose),
              new_purpose);
  }

  old_purpose = circ->purpose;
  circ->purpose = new_purpose;

  if (CIRCUIT_IS_ORIGIN(circ)) {
    control_event_circuit_purpose_changed(TO_ORIGIN_CIRCUIT(circ),
                                          old_purpose);
  }
}

/** Mark <b>circ</b> so that no more connections can be attached to it. */
void
mark_circuit_unusable_for_new_conns(origin_circuit_t *circ)
{
  const or_options_t *options = get_options();
  tor_assert(circ);

  /* XXXX025 This is a kludge; we're only keeping it around in case there's
   * something that doesn't check unusable_for_new_conns, and to avoid
   * deeper refactoring of our expiration logic. */
  if (! circ->base_.timestamp_dirty)
    circ->base_.timestamp_dirty = approx_time();
  if (options->MaxCircuitDirtiness >= circ->base_.timestamp_dirty)
    circ->base_.timestamp_dirty = 1; /* prevent underflow */
  else
    circ->base_.timestamp_dirty -= options->MaxCircuitDirtiness;

  circ->unusable_for_new_conns = 1;
}

