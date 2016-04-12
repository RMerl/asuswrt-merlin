/* Copyright (c) 2001 Matej Pfajfar.
 * Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2015, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#include "or.h"
#include "channel.h"
#include "circpathbias.h"
#include "circuitbuild.h"
#include "circuitlist.h"
#include "circuituse.h"
#include "circuitstats.h"
#include "connection_edge.h"
#include "config.h"
#include "entrynodes.h"
#include "networkstatus.h"
#include "relay.h"

static void pathbias_count_successful_close(origin_circuit_t *circ);
static void pathbias_count_collapse(origin_circuit_t *circ);
static void pathbias_count_use_failed(origin_circuit_t *circ);
static void pathbias_measure_use_rate(entry_guard_t *guard);
static void pathbias_measure_close_rate(entry_guard_t *guard);
static void pathbias_scale_use_rates(entry_guard_t *guard);
static void pathbias_scale_close_rates(entry_guard_t *guard);
static int entry_guard_inc_circ_attempt_count(entry_guard_t *guard);

/** Increment the number of times we successfully extended a circuit to
 * <b>guard</b>, first checking if the failure rate is high enough that
 * we should eliminate the guard. Return -1 if the guard looks no good;
 * return 0 if the guard looks fine.
 */
static int
entry_guard_inc_circ_attempt_count(entry_guard_t *guard)
{
  entry_guards_changed();

  pathbias_measure_close_rate(guard);

  if (guard->path_bias_disabled)
    return -1;

  pathbias_scale_close_rates(guard);
  guard->circ_attempts++;

  log_info(LD_CIRC, "Got success count %f/%f for guard %s ($%s)",
           guard->circ_successes, guard->circ_attempts, guard->nickname,
           hex_str(guard->identity, DIGEST_LEN));
  return 0;
}

/** The minimum number of circuit attempts before we start
  * thinking about warning about path bias and dropping guards */
static int
pathbias_get_min_circs(const or_options_t *options)
{
#define DFLT_PATH_BIAS_MIN_CIRC 150
  if (options->PathBiasCircThreshold >= 5)
    return options->PathBiasCircThreshold;
  else
    return networkstatus_get_param(NULL, "pb_mincircs",
                                   DFLT_PATH_BIAS_MIN_CIRC,
                                   5, INT32_MAX);
}

/** The circuit success rate below which we issue a notice */
static double
pathbias_get_notice_rate(const or_options_t *options)
{
#define DFLT_PATH_BIAS_NOTICE_PCT 70
  if (options->PathBiasNoticeRate >= 0.0)
    return options->PathBiasNoticeRate;
  else
    return networkstatus_get_param(NULL, "pb_noticepct",
                                   DFLT_PATH_BIAS_NOTICE_PCT, 0, 100)/100.0;
}

/* XXXX024 I'd like to have this be static again, but entrynodes.c needs it. */
/** The circuit success rate below which we issue a warn */
static double
pathbias_get_warn_rate(const or_options_t *options)
{
#define DFLT_PATH_BIAS_WARN_PCT 50
  if (options->PathBiasWarnRate >= 0.0)
    return options->PathBiasWarnRate;
  else
    return networkstatus_get_param(NULL, "pb_warnpct",
                                   DFLT_PATH_BIAS_WARN_PCT, 0, 100)/100.0;
}

/* XXXX024 I'd like to have this be static again, but entrynodes.c needs it. */
/**
 * The extreme rate is the rate at which we would drop the guard,
 * if pb_dropguard is also set. Otherwise we just warn.
 */
double
pathbias_get_extreme_rate(const or_options_t *options)
{
#define DFLT_PATH_BIAS_EXTREME_PCT 30
  if (options->PathBiasExtremeRate >= 0.0)
    return options->PathBiasExtremeRate;
  else
    return networkstatus_get_param(NULL, "pb_extremepct",
                                   DFLT_PATH_BIAS_EXTREME_PCT, 0, 100)/100.0;
}

/* XXXX024 I'd like to have this be static again, but entrynodes.c needs it. */
/**
 * If 1, we actually disable use of guards that fall below
 * the extreme_pct.
 */
int
pathbias_get_dropguards(const or_options_t *options)
{
#define DFLT_PATH_BIAS_DROP_GUARDS 0
  if (options->PathBiasDropGuards >= 0)
    return options->PathBiasDropGuards;
  else
    return networkstatus_get_param(NULL, "pb_dropguards",
                                   DFLT_PATH_BIAS_DROP_GUARDS, 0, 1);
}

/**
 * This is the number of circuits at which we scale our
 * counts by mult_factor/scale_factor. Note, this count is
 * not exact, as we only perform the scaling in the event
 * of no integer truncation.
 */
static int
pathbias_get_scale_threshold(const or_options_t *options)
{
#define DFLT_PATH_BIAS_SCALE_THRESHOLD 300
  if (options->PathBiasScaleThreshold >= 10)
    return options->PathBiasScaleThreshold;
  else
    return networkstatus_get_param(NULL, "pb_scalecircs",
                                   DFLT_PATH_BIAS_SCALE_THRESHOLD, 10,
                                   INT32_MAX);
}

/**
 * Compute the path bias scaling ratio from the consensus
 * parameters pb_multfactor/pb_scalefactor.
 *
 * Returns a value in (0, 1.0] which we multiply our pathbias
 * counts with to scale them down.
 */
static double
pathbias_get_scale_ratio(const or_options_t *options)
{
  /*
   * The scale factor is the denominator for our scaling
   * of circuit counts for our path bias window.
   *
   * Note that our use of doubles for the path bias state
   * file means that powers of 2 work best here.
   */
  int denominator = networkstatus_get_param(NULL, "pb_scalefactor",
                              2, 2, INT32_MAX);
  (void) options;
  /**
   * The mult factor is the numerator for our scaling
   * of circuit counts for our path bias window. It
   * allows us to scale by fractions.
   */
  return networkstatus_get_param(NULL, "pb_multfactor",
                              1, 1, denominator)/((double)denominator);
}

/** The minimum number of circuit usage attempts before we start
  * thinking about warning about path use bias and dropping guards */
static int
pathbias_get_min_use(const or_options_t *options)
{
#define DFLT_PATH_BIAS_MIN_USE 20
  if (options->PathBiasUseThreshold >= 3)
    return options->PathBiasUseThreshold;
  else
    return networkstatus_get_param(NULL, "pb_minuse",
                                   DFLT_PATH_BIAS_MIN_USE,
                                   3, INT32_MAX);
}

/** The circuit use success rate below which we issue a notice */
static double
pathbias_get_notice_use_rate(const or_options_t *options)
{
#define DFLT_PATH_BIAS_NOTICE_USE_PCT 80
  if (options->PathBiasNoticeUseRate >= 0.0)
    return options->PathBiasNoticeUseRate;
  else
    return networkstatus_get_param(NULL, "pb_noticeusepct",
                                   DFLT_PATH_BIAS_NOTICE_USE_PCT,
                                   0, 100)/100.0;
}

/**
 * The extreme use rate is the rate at which we would drop the guard,
 * if pb_dropguard is also set. Otherwise we just warn.
 */
double
pathbias_get_extreme_use_rate(const or_options_t *options)
{
#define DFLT_PATH_BIAS_EXTREME_USE_PCT 60
  if (options->PathBiasExtremeUseRate >= 0.0)
    return options->PathBiasExtremeUseRate;
  else
    return networkstatus_get_param(NULL, "pb_extremeusepct",
                                   DFLT_PATH_BIAS_EXTREME_USE_PCT,
                                   0, 100)/100.0;
}

/**
 * This is the number of circuits at which we scale our
 * use counts by mult_factor/scale_factor. Note, this count is
 * not exact, as we only perform the scaling in the event
 * of no integer truncation.
 */
static int
pathbias_get_scale_use_threshold(const or_options_t *options)
{
#define DFLT_PATH_BIAS_SCALE_USE_THRESHOLD 100
  if (options->PathBiasScaleUseThreshold >= 10)
    return options->PathBiasScaleUseThreshold;
  else
    return networkstatus_get_param(NULL, "pb_scaleuse",
                                   DFLT_PATH_BIAS_SCALE_USE_THRESHOLD,
                                   10, INT32_MAX);
}

/**
 * Convert a Guard's path state to string.
 */
const char *
pathbias_state_to_string(path_state_t state)
{
  switch (state) {
    case PATH_STATE_NEW_CIRC:
      return "new";
    case PATH_STATE_BUILD_ATTEMPTED:
      return "build attempted";
    case PATH_STATE_BUILD_SUCCEEDED:
      return "build succeeded";
    case PATH_STATE_USE_ATTEMPTED:
      return "use attempted";
    case PATH_STATE_USE_SUCCEEDED:
      return "use succeeded";
    case PATH_STATE_USE_FAILED:
      return "use failed";
    case PATH_STATE_ALREADY_COUNTED:
      return "already counted";
  }

  return "unknown";
}

/**
 * This function decides if a circuit has progressed far enough to count
 * as a circuit "attempt". As long as end-to-end tagging is possible,
 * we assume the adversary will use it over hop-to-hop failure. Therefore,
 * we only need to account bias for the last hop. This should make us
 * much more resilient to ambient circuit failure, and also make that
 * failure easier to measure (we only need to measure Exit failure rates).
 */
static int
pathbias_is_new_circ_attempt(origin_circuit_t *circ)
{
#define N2N_TAGGING_IS_POSSIBLE
#ifdef N2N_TAGGING_IS_POSSIBLE
  /* cpath is a circular list. We want circs with more than one hop,
   * and the second hop must be waiting for keys still (it's just
   * about to get them). */
  return circ->cpath &&
         circ->cpath->next != circ->cpath &&
         circ->cpath->next->state == CPATH_STATE_AWAITING_KEYS;
#else
  /* If tagging attacks are no longer possible, we probably want to
   * count bias from the first hop. However, one could argue that
   * timing-based tagging is still more useful than per-hop failure.
   * In which case, we'd never want to use this.
   */
  return circ->cpath &&
         circ->cpath->state == CPATH_STATE_AWAITING_KEYS;
#endif
}

/**
 * Decide if the path bias code should count a circuit.
 *
 * @returns 1 if we should count it, 0 otherwise.
 */
static int
pathbias_should_count(origin_circuit_t *circ)
{
#define PATHBIAS_COUNT_INTERVAL (600)
  static ratelim_t count_limit =
    RATELIM_INIT(PATHBIAS_COUNT_INTERVAL);
  char *rate_msg = NULL;

  /* We can't do path bias accounting without entry guards.
   * Testing and controller circuits also have no guards.
   *
   * We also don't count server-side rends, because their
   * endpoint could be chosen maliciously.
   * Similarly, we can't count client-side intro attempts,
   * because clients can be manipulated into connecting to
   * malicious intro points. */
  if (get_options()->UseEntryGuards == 0 ||
          circ->base_.purpose == CIRCUIT_PURPOSE_TESTING ||
          circ->base_.purpose == CIRCUIT_PURPOSE_CONTROLLER ||
          circ->base_.purpose == CIRCUIT_PURPOSE_S_CONNECT_REND ||
          circ->base_.purpose == CIRCUIT_PURPOSE_S_REND_JOINED ||
          (circ->base_.purpose >= CIRCUIT_PURPOSE_C_INTRODUCING &&
           circ->base_.purpose <= CIRCUIT_PURPOSE_C_INTRODUCE_ACKED)) {

    /* Check to see if the shouldcount result has changed due to a
     * unexpected purpose change that would affect our results.
     *
     * The reason we check the path state too here is because for the
     * cannibalized versions of these purposes, we count them as successful
     * before their purpose change.
     */
    if (circ->pathbias_shouldcount == PATHBIAS_SHOULDCOUNT_COUNTED
            && circ->path_state != PATH_STATE_ALREADY_COUNTED) {
      log_info(LD_BUG,
               "Circuit %d is now being ignored despite being counted "
               "in the past. Purpose is %s, path state is %s",
               circ->global_identifier,
               circuit_purpose_to_string(circ->base_.purpose),
               pathbias_state_to_string(circ->path_state));
    }
    circ->pathbias_shouldcount = PATHBIAS_SHOULDCOUNT_IGNORED;
    return 0;
  }

  /* Completely ignore one hop circuits */
  if (circ->build_state->onehop_tunnel ||
      circ->build_state->desired_path_len == 1) {
    /* Check for inconsistency */
    if (circ->build_state->desired_path_len != 1 ||
        !circ->build_state->onehop_tunnel) {
      if ((rate_msg = rate_limit_log(&count_limit, approx_time()))) {
        log_info(LD_BUG,
               "One-hop circuit has length %d. Path state is %s. "
               "Circuit is a %s currently %s.%s",
               circ->build_state->desired_path_len,
               pathbias_state_to_string(circ->path_state),
               circuit_purpose_to_string(circ->base_.purpose),
               circuit_state_to_string(circ->base_.state),
               rate_msg);
        tor_free(rate_msg);
      }
      tor_fragile_assert();
    }

    /* Check to see if the shouldcount result has changed due to a
     * unexpected change that would affect our results */
    if (circ->pathbias_shouldcount == PATHBIAS_SHOULDCOUNT_COUNTED) {
      log_info(LD_BUG,
               "One-hop circuit %d is now being ignored despite being counted "
               "in the past. Purpose is %s, path state is %s",
               circ->global_identifier,
               circuit_purpose_to_string(circ->base_.purpose),
               pathbias_state_to_string(circ->path_state));
    }
    circ->pathbias_shouldcount = PATHBIAS_SHOULDCOUNT_IGNORED;
    return 0;
  }

  /* Check to see if the shouldcount result has changed due to a
   * unexpected purpose change that would affect our results */
  if (circ->pathbias_shouldcount == PATHBIAS_SHOULDCOUNT_IGNORED) {
      log_info(LD_BUG,
              "Circuit %d is now being counted despite being ignored "
              "in the past. Purpose is %s, path state is %s",
              circ->global_identifier,
              circuit_purpose_to_string(circ->base_.purpose),
              pathbias_state_to_string(circ->path_state));
  }
  circ->pathbias_shouldcount = PATHBIAS_SHOULDCOUNT_COUNTED;

  return 1;
}

/**
 * Check our circuit state to see if this is a successful circuit attempt.
 * If so, record it in the current guard's path bias circ_attempt count.
 *
 * Also check for several potential error cases for bug #6475.
 */
int
pathbias_count_build_attempt(origin_circuit_t *circ)
{
#define CIRC_ATTEMPT_NOTICE_INTERVAL (600)
  static ratelim_t circ_attempt_notice_limit =
    RATELIM_INIT(CIRC_ATTEMPT_NOTICE_INTERVAL);
  char *rate_msg = NULL;

  if (!pathbias_should_count(circ)) {
    return 0;
  }

  if (pathbias_is_new_circ_attempt(circ)) {
    /* Help track down the real cause of bug #6475: */
    if (circ->has_opened && circ->path_state != PATH_STATE_BUILD_ATTEMPTED) {
      if ((rate_msg = rate_limit_log(&circ_attempt_notice_limit,
                                     approx_time()))) {
        log_info(LD_BUG,
                "Opened circuit is in strange path state %s. "
                "Circuit is a %s currently %s.%s",
                pathbias_state_to_string(circ->path_state),
                circuit_purpose_to_string(circ->base_.purpose),
                circuit_state_to_string(circ->base_.state),
                rate_msg);
        tor_free(rate_msg);
      }
    }

    /* Don't re-count cannibalized circs.. */
    if (!circ->has_opened) {
      entry_guard_t *guard = NULL;

      if (circ->cpath && circ->cpath->extend_info) {
        guard = entry_guard_get_by_id_digest(
                  circ->cpath->extend_info->identity_digest);
      } else if (circ->base_.n_chan) {
        guard =
          entry_guard_get_by_id_digest(circ->base_.n_chan->identity_digest);
      }

      if (guard) {
        if (circ->path_state == PATH_STATE_NEW_CIRC) {
          circ->path_state = PATH_STATE_BUILD_ATTEMPTED;

          if (entry_guard_inc_circ_attempt_count(guard) < 0) {
            /* Bogus guard; we already warned. */
            return -END_CIRC_REASON_TORPROTOCOL;
          }
        } else {
          if ((rate_msg = rate_limit_log(&circ_attempt_notice_limit,
                  approx_time()))) {
            log_info(LD_BUG,
                   "Unopened circuit has strange path state %s. "
                   "Circuit is a %s currently %s.%s",
                   pathbias_state_to_string(circ->path_state),
                   circuit_purpose_to_string(circ->base_.purpose),
                   circuit_state_to_string(circ->base_.state),
                   rate_msg);
            tor_free(rate_msg);
          }
        }
      } else {
        if ((rate_msg = rate_limit_log(&circ_attempt_notice_limit,
                approx_time()))) {
          log_info(LD_CIRC,
              "Unopened circuit has no known guard. "
              "Circuit is a %s currently %s.%s",
              circuit_purpose_to_string(circ->base_.purpose),
              circuit_state_to_string(circ->base_.state),
              rate_msg);
          tor_free(rate_msg);
        }
      }
    }
  }

  return 0;
}

/**
 * Check our circuit state to see if this is a successful circuit
 * completion. If so, record it in the current guard's path bias
 * success count.
 *
 * Also check for several potential error cases for bug #6475.
 */
void
pathbias_count_build_success(origin_circuit_t *circ)
{
#define SUCCESS_NOTICE_INTERVAL (600)
  static ratelim_t success_notice_limit =
    RATELIM_INIT(SUCCESS_NOTICE_INTERVAL);
  char *rate_msg = NULL;
  entry_guard_t *guard = NULL;

  if (!pathbias_should_count(circ)) {
    return;
  }

  /* Don't count cannibalized/reused circs for path bias
   * "build" success, since they get counted under "use" success. */
  if (!circ->has_opened) {
    if (circ->cpath && circ->cpath->extend_info) {
      guard = entry_guard_get_by_id_digest(
                circ->cpath->extend_info->identity_digest);
    }

    if (guard) {
      if (circ->path_state == PATH_STATE_BUILD_ATTEMPTED) {
        circ->path_state = PATH_STATE_BUILD_SUCCEEDED;
        guard->circ_successes++;
        entry_guards_changed();

        log_info(LD_CIRC, "Got success count %f/%f for guard %s ($%s)",
                 guard->circ_successes, guard->circ_attempts,
                 guard->nickname, hex_str(guard->identity, DIGEST_LEN));
      } else {
        if ((rate_msg = rate_limit_log(&success_notice_limit,
                approx_time()))) {
          log_info(LD_BUG,
              "Succeeded circuit is in strange path state %s. "
              "Circuit is a %s currently %s.%s",
              pathbias_state_to_string(circ->path_state),
              circuit_purpose_to_string(circ->base_.purpose),
              circuit_state_to_string(circ->base_.state),
              rate_msg);
          tor_free(rate_msg);
        }
      }

      if (guard->circ_attempts < guard->circ_successes) {
        log_notice(LD_BUG, "Unexpectedly high successes counts (%f/%f) "
                 "for guard %s ($%s)",
                 guard->circ_successes, guard->circ_attempts,
                 guard->nickname, hex_str(guard->identity, DIGEST_LEN));
      }
    /* In rare cases, CIRCUIT_PURPOSE_TESTING can get converted to
     * CIRCUIT_PURPOSE_C_MEASURE_TIMEOUT and have no guards here.
     * No need to log that case. */
    } else if (circ->base_.purpose != CIRCUIT_PURPOSE_C_MEASURE_TIMEOUT) {
      if ((rate_msg = rate_limit_log(&success_notice_limit,
              approx_time()))) {
        log_info(LD_CIRC,
            "Completed circuit has no known guard. "
            "Circuit is a %s currently %s.%s",
            circuit_purpose_to_string(circ->base_.purpose),
            circuit_state_to_string(circ->base_.state),
            rate_msg);
        tor_free(rate_msg);
      }
    }
  } else {
    if (circ->path_state < PATH_STATE_BUILD_SUCCEEDED) {
      if ((rate_msg = rate_limit_log(&success_notice_limit,
              approx_time()))) {
        log_info(LD_BUG,
            "Opened circuit is in strange path state %s. "
            "Circuit is a %s currently %s.%s",
            pathbias_state_to_string(circ->path_state),
            circuit_purpose_to_string(circ->base_.purpose),
            circuit_state_to_string(circ->base_.state),
            rate_msg);
        tor_free(rate_msg);
      }
    }
  }
}

/**
 * Record an attempt to use a circuit. Changes the circuit's
 * path state and update its guard's usage counter.
 *
 * Used for path bias usage accounting.
 */
void
pathbias_count_use_attempt(origin_circuit_t *circ)
{
  entry_guard_t *guard;

  if (!pathbias_should_count(circ)) {
    return;
  }

  if (circ->path_state < PATH_STATE_BUILD_SUCCEEDED) {
    log_notice(LD_BUG,
        "Used circuit is in strange path state %s. "
        "Circuit is a %s currently %s.",
        pathbias_state_to_string(circ->path_state),
        circuit_purpose_to_string(circ->base_.purpose),
        circuit_state_to_string(circ->base_.state));
  } else if (circ->path_state < PATH_STATE_USE_ATTEMPTED) {
    guard = entry_guard_get_by_id_digest(
                circ->cpath->extend_info->identity_digest);
    if (guard) {
      pathbias_measure_use_rate(guard);
      pathbias_scale_use_rates(guard);
      guard->use_attempts++;
      entry_guards_changed();

      log_debug(LD_CIRC,
               "Marked circuit %d (%f/%f) as used for guard %s ($%s).",
               circ->global_identifier,
               guard->use_successes, guard->use_attempts,
               guard->nickname, hex_str(guard->identity, DIGEST_LEN));
    }

    circ->path_state = PATH_STATE_USE_ATTEMPTED;
  } else {
    /* Harmless but educational log message */
    log_info(LD_CIRC,
        "Used circuit %d is already in path state %s. "
        "Circuit is a %s currently %s.",
        circ->global_identifier,
        pathbias_state_to_string(circ->path_state),
        circuit_purpose_to_string(circ->base_.purpose),
        circuit_state_to_string(circ->base_.state));
  }

  return;
}

/**
 * Check the circuit's path state is appropriate and mark it as
 * successfully used. Used for path bias usage accounting.
 *
 * We don't actually increment the guard's counters until
 * pathbias_check_close(), because the circuit can still transition
 * back to PATH_STATE_USE_ATTEMPTED if a stream fails later (this
 * is done so we can probe the circuit for liveness at close).
 */
void
pathbias_mark_use_success(origin_circuit_t *circ)
{
  if (!pathbias_should_count(circ)) {
    return;
  }

  if (circ->path_state < PATH_STATE_USE_ATTEMPTED) {
    log_notice(LD_BUG,
        "Used circuit %d is in strange path state %s. "
        "Circuit is a %s currently %s.",
        circ->global_identifier,
        pathbias_state_to_string(circ->path_state),
        circuit_purpose_to_string(circ->base_.purpose),
        circuit_state_to_string(circ->base_.state));

    pathbias_count_use_attempt(circ);
  }

  /* We don't do any accounting at the guard until actual circuit close */
  circ->path_state = PATH_STATE_USE_SUCCEEDED;

  return;
}

/**
 * If a stream ever detatches from a circuit in a retriable way,
 * we need to mark this circuit as still needing either another
 * successful stream, or in need of a probe.
 *
 * An adversary could let the first stream request succeed (ie the
 * resolve), but then tag and timeout the remainder (via cell
 * dropping), forcing them on new circuits.
 *
 * Rolling back the state will cause us to probe such circuits, which
 * should lead to probe failures in the event of such tagging due to
 * either unrecognized cells coming in while we wait for the probe,
 * or the cipher state getting out of sync in the case of dropped cells.
 */
void
pathbias_mark_use_rollback(origin_circuit_t *circ)
{
  if (circ->path_state == PATH_STATE_USE_SUCCEEDED) {
    log_info(LD_CIRC,
             "Rolling back pathbias use state to 'attempted' for detached "
             "circuit %d", circ->global_identifier);
    circ->path_state = PATH_STATE_USE_ATTEMPTED;
  }
}

/**
 * Actually count a circuit success towards a guard's usage counters
 * if the path state is appropriate.
 */
static void
pathbias_count_use_success(origin_circuit_t *circ)
{
  entry_guard_t *guard;

  if (!pathbias_should_count(circ)) {
    return;
  }

  if (circ->path_state != PATH_STATE_USE_SUCCEEDED) {
    log_notice(LD_BUG,
        "Successfully used circuit %d is in strange path state %s. "
        "Circuit is a %s currently %s.",
        circ->global_identifier,
        pathbias_state_to_string(circ->path_state),
        circuit_purpose_to_string(circ->base_.purpose),
        circuit_state_to_string(circ->base_.state));
  } else {
    guard = entry_guard_get_by_id_digest(
                circ->cpath->extend_info->identity_digest);
    if (guard) {
      guard->use_successes++;
      entry_guards_changed();

      if (guard->use_attempts < guard->use_successes) {
        log_notice(LD_BUG, "Unexpectedly high use successes counts (%f/%f) "
                 "for guard %s=%s",
                 guard->use_successes, guard->use_attempts,
                 guard->nickname, hex_str(guard->identity, DIGEST_LEN));
      }

      log_debug(LD_CIRC,
                "Marked circuit %d (%f/%f) as used successfully for guard "
                "%s ($%s).",
                circ->global_identifier, guard->use_successes,
                guard->use_attempts, guard->nickname,
                hex_str(guard->identity, DIGEST_LEN));
    }
  }

  return;
}

/**
 * Send a probe down a circuit that the client attempted to use,
 * but for which the stream timed out/failed. The probe is a
 * RELAY_BEGIN cell with a 0.a.b.c destination address, which
 * the exit will reject and reply back, echoing that address.
 *
 * The reason for such probes is because it is possible to bias
 * a user's paths simply by causing timeouts, and these timeouts
 * are not possible to differentiate from unresponsive servers.
 *
 * The probe is sent at the end of the circuit lifetime for two
 * reasons: to prevent cryptographic taggers from being able to
 * drop cells to cause timeouts, and to prevent easy recognition
 * of probes before any real client traffic happens.
 *
 * Returns -1 if we couldn't probe, 0 otherwise.
 */
static int
pathbias_send_usable_probe(circuit_t *circ)
{
  /* Based on connection_ap_handshake_send_begin() */
  char payload[CELL_PAYLOAD_SIZE];
  int payload_len;
  origin_circuit_t *ocirc = TO_ORIGIN_CIRCUIT(circ);
  crypt_path_t *cpath_layer = NULL;
  char *probe_nonce = NULL;

  tor_assert(ocirc);

  cpath_layer = ocirc->cpath->prev;

  if (cpath_layer->state != CPATH_STATE_OPEN) {
    /* This can happen for cannibalized circuits. Their
     * last hop isn't yet open */
    log_info(LD_CIRC,
             "Got pathbias probe request for unopened circuit %d. "
             "Opened %d, len %d", ocirc->global_identifier,
             ocirc->has_opened, ocirc->build_state->desired_path_len);
    return -1;
  }

  /* We already went down this road. */
  if (circ->purpose == CIRCUIT_PURPOSE_PATH_BIAS_TESTING &&
      ocirc->pathbias_probe_id) {
    log_info(LD_CIRC,
             "Got pathbias probe request for circuit %d with "
             "outstanding probe", ocirc->global_identifier);
    return -1;
  }

  /* Can't probe if the channel isn't open */
  if (circ->n_chan == NULL ||
      (!CHANNEL_IS_OPEN(circ->n_chan)
       && !CHANNEL_IS_MAINT(circ->n_chan))) {
    log_info(LD_CIRC,
             "Skipping pathbias probe for circuit %d: Channel is not open.",
             ocirc->global_identifier);
    return -1;
  }

  circuit_change_purpose(circ, CIRCUIT_PURPOSE_PATH_BIAS_TESTING);

  /* Update timestamp for when circuit_expire_building() should kill us */
  tor_gettimeofday(&circ->timestamp_began);

  /* Generate a random address for the nonce */
  crypto_rand((char*)&ocirc->pathbias_probe_nonce,
              sizeof(ocirc->pathbias_probe_nonce));
  ocirc->pathbias_probe_nonce &= 0x00ffffff;
  probe_nonce = tor_dup_ip(ocirc->pathbias_probe_nonce);

  tor_snprintf(payload,RELAY_PAYLOAD_SIZE, "%s:25", probe_nonce);
  payload_len = (int)strlen(payload)+1;

  // XXX: need this? Can we assume ipv4 will always be supported?
  // If not, how do we tell?
  //if (payload_len <= RELAY_PAYLOAD_SIZE - 4 && edge_conn->begincell_flags) {
  //  set_uint32(payload + payload_len, htonl(edge_conn->begincell_flags));
  //  payload_len += 4;
  //}

  /* Generate+Store stream id, make sure it's non-zero */
  ocirc->pathbias_probe_id = get_unique_stream_id_by_circ(ocirc);

  if (ocirc->pathbias_probe_id==0) {
    log_warn(LD_CIRC,
             "Ran out of stream IDs on circuit %u during "
             "pathbias probe attempt.", ocirc->global_identifier);
    tor_free(probe_nonce);
    return -1;
  }

  log_info(LD_CIRC,
           "Sending pathbias testing cell to %s:25 on stream %d for circ %d.",
           probe_nonce, ocirc->pathbias_probe_id, ocirc->global_identifier);
  tor_free(probe_nonce);

  /* Send a test relay cell */
  if (relay_send_command_from_edge(ocirc->pathbias_probe_id, circ,
                               RELAY_COMMAND_BEGIN, payload,
                               payload_len, cpath_layer) < 0) {
    log_notice(LD_CIRC,
               "Failed to send pathbias probe cell on circuit %d.",
               ocirc->global_identifier);
    return -1;
  }

  /* Mark it freshly dirty so it doesn't get expired in the meantime */
  circ->timestamp_dirty = time(NULL);

  return 0;
}

/**
 * Check the response to a pathbias probe, to ensure the
 * cell is recognized and the nonce and other probe
 * characteristics are as expected.
 *
 * If the response is valid, return 0. Otherwise return < 0.
 */
int
pathbias_check_probe_response(circuit_t *circ, const cell_t *cell)
{
  /* Based on connection_edge_process_relay_cell() */
  relay_header_t rh;
  int reason;
  uint32_t ipv4_host;
  origin_circuit_t *ocirc = TO_ORIGIN_CIRCUIT(circ);

  tor_assert(cell);
  tor_assert(ocirc);
  tor_assert(circ->purpose == CIRCUIT_PURPOSE_PATH_BIAS_TESTING);

  relay_header_unpack(&rh, cell->payload);

  reason = rh.length > 0 ?
        get_uint8(cell->payload+RELAY_HEADER_SIZE) : END_STREAM_REASON_MISC;

  if (rh.command == RELAY_COMMAND_END &&
      reason == END_STREAM_REASON_EXITPOLICY &&
      ocirc->pathbias_probe_id == rh.stream_id) {

    /* Check length+extract host: It is in network order after the reason code.
     * See connection_edge_end(). */
    if (rh.length < 9) { /* reason+ipv4+dns_ttl */
      log_notice(LD_PROTOCOL,
             "Short path bias probe response length field (%d).", rh.length);
      return - END_CIRC_REASON_TORPROTOCOL;
    }

    ipv4_host = ntohl(get_uint32(cell->payload+RELAY_HEADER_SIZE+1));

    /* Check nonce */
    if (ipv4_host == ocirc->pathbias_probe_nonce) {
      pathbias_mark_use_success(ocirc);
      circuit_mark_for_close(circ, END_CIRC_REASON_FINISHED);
      log_info(LD_CIRC,
               "Got valid path bias probe back for circ %d, stream %d.",
               ocirc->global_identifier, ocirc->pathbias_probe_id);
      return 0;
    } else {
      log_notice(LD_CIRC,
               "Got strange probe value 0x%x vs 0x%x back for circ %d, "
               "stream %d.", ipv4_host, ocirc->pathbias_probe_nonce,
               ocirc->global_identifier, ocirc->pathbias_probe_id);
      return -1;
    }
  }
  log_info(LD_CIRC,
             "Got another cell back back on pathbias probe circuit %d: "
             "Command: %d, Reason: %d, Stream-id: %d",
             ocirc->global_identifier, rh.command, reason, rh.stream_id);
  return -1;
}

/**
 * Check if a circuit was used and/or closed successfully.
 *
 * If we attempted to use the circuit to carry a stream but failed
 * for whatever reason, or if the circuit mysteriously died before
 * we could attach any streams, record these two cases.
 *
 * If we *have* successfully used the circuit, or it appears to
 * have been closed by us locally, count it as a success.
 *
 * Returns 0 if we're done making decisions with the circ,
 * or -1 if we want to probe it first.
 */
int
pathbias_check_close(origin_circuit_t *ocirc, int reason)
{
  circuit_t *circ = &ocirc->base_;

  if (!pathbias_should_count(ocirc)) {
    return 0;
  }

  switch (ocirc->path_state) {
    /* If the circuit was closed after building, but before use, we need
     * to ensure we were the ones who tried to close it (and not a remote
     * actor). */
    case PATH_STATE_BUILD_SUCCEEDED:
      if (reason & END_CIRC_REASON_FLAG_REMOTE) {
        /* Remote circ close reasons on an unused circuit all could be bias */
        log_info(LD_CIRC,
            "Circuit %d remote-closed without successful use for reason %d. "
            "Circuit purpose %d currently %d,%s. Len %d.",
            ocirc->global_identifier,
            reason, circ->purpose, ocirc->has_opened,
            circuit_state_to_string(circ->state),
            ocirc->build_state->desired_path_len);
        pathbias_count_collapse(ocirc);
      } else if ((reason & ~END_CIRC_REASON_FLAG_REMOTE)
                  == END_CIRC_REASON_CHANNEL_CLOSED &&
                 circ->n_chan &&
                 circ->n_chan->reason_for_closing
                  != CHANNEL_CLOSE_REQUESTED) {
        /* If we didn't close the channel ourselves, it could be bias */
        /* XXX: Only count bias if the network is live?
         * What about clock jumps/suspends? */
        log_info(LD_CIRC,
            "Circuit %d's channel closed without successful use for reason "
            "%d, channel reason %d. Circuit purpose %d currently %d,%s. Len "
            "%d.", ocirc->global_identifier,
            reason, circ->n_chan->reason_for_closing,
            circ->purpose, ocirc->has_opened,
            circuit_state_to_string(circ->state),
            ocirc->build_state->desired_path_len);
        pathbias_count_collapse(ocirc);
      } else {
        pathbias_count_successful_close(ocirc);
      }
      break;

    /* If we tried to use a circuit but failed, we should probe it to ensure
     * it has not been tampered with. */
    case PATH_STATE_USE_ATTEMPTED:
      /* XXX: Only probe and/or count failure if the network is live?
       * What about clock jumps/suspends? */
      if (pathbias_send_usable_probe(circ) == 0)
        return -1;
      else
        pathbias_count_use_failed(ocirc);

      /* Any circuit where there were attempted streams but no successful
       * streams could be bias */
      log_info(LD_CIRC,
            "Circuit %d closed without successful use for reason %d. "
            "Circuit purpose %d currently %d,%s. Len %d.",
            ocirc->global_identifier,
            reason, circ->purpose, ocirc->has_opened,
            circuit_state_to_string(circ->state),
            ocirc->build_state->desired_path_len);
      break;

    case PATH_STATE_USE_SUCCEEDED:
      pathbias_count_successful_close(ocirc);
      pathbias_count_use_success(ocirc);
      break;

    case PATH_STATE_USE_FAILED:
      pathbias_count_use_failed(ocirc);
      break;

    case PATH_STATE_NEW_CIRC:
    case PATH_STATE_BUILD_ATTEMPTED:
    case PATH_STATE_ALREADY_COUNTED:
    default:
      // Other states are uninteresting. No stats to count.
      break;
  }

  ocirc->path_state = PATH_STATE_ALREADY_COUNTED;

  return 0;
}

/**
 * Count a successfully closed circuit.
 */
static void
pathbias_count_successful_close(origin_circuit_t *circ)
{
  entry_guard_t *guard = NULL;
  if (!pathbias_should_count(circ)) {
    return;
  }

  if (circ->cpath && circ->cpath->extend_info) {
    guard = entry_guard_get_by_id_digest(
              circ->cpath->extend_info->identity_digest);
  }

  if (guard) {
    /* In the long run: circuit_success ~= successful_circuit_close +
     *                                     circ_failure + stream_failure */
    guard->successful_circuits_closed++;
    entry_guards_changed();
  } else if (circ->base_.purpose != CIRCUIT_PURPOSE_C_MEASURE_TIMEOUT) {
   /* In rare cases, CIRCUIT_PURPOSE_TESTING can get converted to
    * CIRCUIT_PURPOSE_C_MEASURE_TIMEOUT and have no guards here.
    * No need to log that case. */
    log_info(LD_CIRC,
        "Successfully closed circuit has no known guard. "
        "Circuit is a %s currently %s",
        circuit_purpose_to_string(circ->base_.purpose),
        circuit_state_to_string(circ->base_.state));
  }
}

/**
 * Count a circuit that fails after it is built, but before it can
 * carry any traffic.
 *
 * This is needed because there are ways to destroy a
 * circuit after it has successfully completed. Right now, this is
 * used for purely informational/debugging purposes.
 */
static void
pathbias_count_collapse(origin_circuit_t *circ)
{
  entry_guard_t *guard = NULL;

  if (!pathbias_should_count(circ)) {
    return;
  }

  if (circ->cpath && circ->cpath->extend_info) {
    guard = entry_guard_get_by_id_digest(
              circ->cpath->extend_info->identity_digest);
  }

  if (guard) {
    guard->collapsed_circuits++;
    entry_guards_changed();
  } else if (circ->base_.purpose != CIRCUIT_PURPOSE_C_MEASURE_TIMEOUT) {
   /* In rare cases, CIRCUIT_PURPOSE_TESTING can get converted to
    * CIRCUIT_PURPOSE_C_MEASURE_TIMEOUT and have no guards here.
    * No need to log that case. */
    log_info(LD_CIRC,
        "Destroyed circuit has no known guard. "
        "Circuit is a %s currently %s",
        circuit_purpose_to_string(circ->base_.purpose),
        circuit_state_to_string(circ->base_.state));
  }
}

/**
 * Count a known failed circuit (because we could not probe it).
 *
 * This counter is informational.
 */
static void
pathbias_count_use_failed(origin_circuit_t *circ)
{
  entry_guard_t *guard = NULL;
  if (!pathbias_should_count(circ)) {
    return;
  }

  if (circ->cpath && circ->cpath->extend_info) {
    guard = entry_guard_get_by_id_digest(
              circ->cpath->extend_info->identity_digest);
  }

  if (guard) {
    guard->unusable_circuits++;
    entry_guards_changed();
  } else if (circ->base_.purpose != CIRCUIT_PURPOSE_C_MEASURE_TIMEOUT) {
   /* In rare cases, CIRCUIT_PURPOSE_TESTING can get converted to
    * CIRCUIT_PURPOSE_C_MEASURE_TIMEOUT and have no guards here.
    * No need to log that case. */
    /* XXX note cut-and-paste code in this function compared to nearby
     * functions. Would be nice to refactor. -RD */
    log_info(LD_CIRC,
        "Stream-failing circuit has no known guard. "
        "Circuit is a %s currently %s",
        circuit_purpose_to_string(circ->base_.purpose),
        circuit_state_to_string(circ->base_.state));
  }
}

/**
 * Count timeouts for path bias log messages.
 *
 * These counts are purely informational.
 */
void
pathbias_count_timeout(origin_circuit_t *circ)
{
  entry_guard_t *guard = NULL;

  if (!pathbias_should_count(circ)) {
    return;
  }

  /* For hidden service circs, they can actually be used
   * successfully and then time out later (because
   * the other side declines to use them). */
  if (circ->path_state == PATH_STATE_USE_SUCCEEDED) {
    return;
  }

  if (circ->cpath && circ->cpath->extend_info) {
    guard = entry_guard_get_by_id_digest(
              circ->cpath->extend_info->identity_digest);
  }

  if (guard) {
    guard->timeouts++;
    entry_guards_changed();
  }
}

/**
 * Helper function to count all of the currently opened circuits
 * for a guard that are in a given path state range. The state
 * range is inclusive on both ends.
 */
static int
pathbias_count_circs_in_states(entry_guard_t *guard,
                              path_state_t from,
                              path_state_t to)
{
  int open_circuits = 0;

  /* Count currently open circuits. Give them the benefit of the doubt. */
  SMARTLIST_FOREACH_BEGIN(circuit_get_global_list(), circuit_t *, circ) {
    origin_circuit_t *ocirc = NULL;
    if (!CIRCUIT_IS_ORIGIN(circ) || /* didn't originate here */
        circ->marked_for_close) /* already counted */
      continue;

    ocirc = TO_ORIGIN_CIRCUIT(circ);

    if (!ocirc->cpath || !ocirc->cpath->extend_info)
      continue;

    if (ocirc->path_state >= from &&
        ocirc->path_state <= to &&
        pathbias_should_count(ocirc) &&
        fast_memeq(guard->identity,
                   ocirc->cpath->extend_info->identity_digest,
                   DIGEST_LEN)) {
      log_debug(LD_CIRC, "Found opened circuit %d in path_state %s",
                ocirc->global_identifier,
                pathbias_state_to_string(ocirc->path_state));
      open_circuits++;
    }
  }
  SMARTLIST_FOREACH_END(circ);

  return open_circuits;
}

/**
 * Return the number of circuits counted as successfully closed for
 * this guard.
 *
 * Also add in the currently open circuits to give them the benefit
 * of the doubt.
 */
double
pathbias_get_close_success_count(entry_guard_t *guard)
{
  return guard->successful_circuits_closed +
         pathbias_count_circs_in_states(guard,
                       PATH_STATE_BUILD_SUCCEEDED,
                       PATH_STATE_USE_SUCCEEDED);
}

/**
 * Return the number of circuits counted as successfully used
 * this guard.
 *
 * Also add in the currently open circuits that we are attempting
 * to use to give them the benefit of the doubt.
 */
double
pathbias_get_use_success_count(entry_guard_t *guard)
{
  return guard->use_successes +
         pathbias_count_circs_in_states(guard,
                       PATH_STATE_USE_ATTEMPTED,
                       PATH_STATE_USE_SUCCEEDED);
}

/**
 * Check the path bias use rate against our consensus parameter limits.
 *
 * Emits a log message if the use success rates are too low.
 *
 * If pathbias_get_dropguards() is set, we also disable the use of
 * very failure prone guards.
 */
static void
pathbias_measure_use_rate(entry_guard_t *guard)
{
  const or_options_t *options = get_options();

  if (guard->use_attempts > pathbias_get_min_use(options)) {
    /* Note: We rely on the < comparison here to allow us to set a 0
     * rate and disable the feature entirely. If refactoring, don't
     * change to <= */
    if (pathbias_get_use_success_count(guard)/guard->use_attempts
        < pathbias_get_extreme_use_rate(options)) {
      /* Dropping is currently disabled by default. */
      if (pathbias_get_dropguards(options)) {
        if (!guard->path_bias_disabled) {
          log_warn(LD_CIRC,
                 "Your Guard %s ($%s) is failing to carry an extremely large "
                 "amount of stream on its circuits. "
                 "To avoid potential route manipulation attacks, Tor has "
                 "disabled use of this guard. "
                 "Use counts are %ld/%ld. Success counts are %ld/%ld. "
                 "%ld circuits completed, %ld were unusable, %ld collapsed, "
                 "and %ld timed out. "
                 "For reference, your timeout cutoff is %ld seconds.",
                 guard->nickname, hex_str(guard->identity, DIGEST_LEN),
                 tor_lround(pathbias_get_use_success_count(guard)),
                 tor_lround(guard->use_attempts),
                 tor_lround(pathbias_get_close_success_count(guard)),
                 tor_lround(guard->circ_attempts),
                 tor_lround(guard->circ_successes),
                 tor_lround(guard->unusable_circuits),
                 tor_lround(guard->collapsed_circuits),
                 tor_lround(guard->timeouts),
                 tor_lround(get_circuit_build_close_time_ms()/1000));
          guard->path_bias_disabled = 1;
          guard->bad_since = approx_time();
          entry_guards_changed();
          return;
        }
      } else if (!guard->path_bias_use_extreme) {
        guard->path_bias_use_extreme = 1;
        log_warn(LD_CIRC,
                 "Your Guard %s ($%s) is failing to carry an extremely large "
                 "amount of streams on its circuits. "
                 "This could indicate a route manipulation attack, network "
                 "overload, bad local network connectivity, or a bug. "
                 "Use counts are %ld/%ld. Success counts are %ld/%ld. "
                 "%ld circuits completed, %ld were unusable, %ld collapsed, "
                 "and %ld timed out. "
                 "For reference, your timeout cutoff is %ld seconds.",
                 guard->nickname, hex_str(guard->identity, DIGEST_LEN),
                 tor_lround(pathbias_get_use_success_count(guard)),
                 tor_lround(guard->use_attempts),
                 tor_lround(pathbias_get_close_success_count(guard)),
                 tor_lround(guard->circ_attempts),
                 tor_lround(guard->circ_successes),
                 tor_lround(guard->unusable_circuits),
                 tor_lround(guard->collapsed_circuits),
                 tor_lround(guard->timeouts),
                 tor_lround(get_circuit_build_close_time_ms()/1000));
      }
    } else if (pathbias_get_use_success_count(guard)/guard->use_attempts
               < pathbias_get_notice_use_rate(options)) {
      if (!guard->path_bias_use_noticed) {
        guard->path_bias_use_noticed = 1;
        log_notice(LD_CIRC,
                 "Your Guard %s ($%s) is failing to carry more streams on its "
                 "circuits than usual. "
                 "Most likely this means the Tor network is overloaded "
                 "or your network connection is poor. "
                 "Use counts are %ld/%ld. Success counts are %ld/%ld. "
                 "%ld circuits completed, %ld were unusable, %ld collapsed, "
                 "and %ld timed out. "
                 "For reference, your timeout cutoff is %ld seconds.",
                 guard->nickname, hex_str(guard->identity, DIGEST_LEN),
                 tor_lround(pathbias_get_use_success_count(guard)),
                 tor_lround(guard->use_attempts),
                 tor_lround(pathbias_get_close_success_count(guard)),
                 tor_lround(guard->circ_attempts),
                 tor_lround(guard->circ_successes),
                 tor_lround(guard->unusable_circuits),
                 tor_lround(guard->collapsed_circuits),
                 tor_lround(guard->timeouts),
                 tor_lround(get_circuit_build_close_time_ms()/1000));
      }
    }
  }
}

/**
 * Check the path bias circuit close status rates against our consensus
 * parameter limits.
 *
 * Emits a log message if the use success rates are too low.
 *
 * If pathbias_get_dropguards() is set, we also disable the use of
 * very failure prone guards.
 *
 * XXX: This function shares similar log messages and checks to
 * pathbias_measure_use_rate(). It may be possible to combine them
 * eventually, especially if we can ever remove the need for 3
 * levels of closure warns (if the overall circuit failure rate
 * goes down with ntor). One way to do so would be to multiply
 * the build rate with the use rate to get an idea of the total
 * fraction of the total network paths the user is able to use.
 * See ticket #8159.
 */
static void
pathbias_measure_close_rate(entry_guard_t *guard)
{
  const or_options_t *options = get_options();

  if (guard->circ_attempts > pathbias_get_min_circs(options)) {
    /* Note: We rely on the < comparison here to allow us to set a 0
     * rate and disable the feature entirely. If refactoring, don't
     * change to <= */
    if (pathbias_get_close_success_count(guard)/guard->circ_attempts
        < pathbias_get_extreme_rate(options)) {
      /* Dropping is currently disabled by default. */
      if (pathbias_get_dropguards(options)) {
        if (!guard->path_bias_disabled) {
          log_warn(LD_CIRC,
                 "Your Guard %s ($%s) is failing an extremely large "
                 "amount of circuits. "
                 "To avoid potential route manipulation attacks, Tor has "
                 "disabled use of this guard. "
                 "Success counts are %ld/%ld. Use counts are %ld/%ld. "
                 "%ld circuits completed, %ld were unusable, %ld collapsed, "
                 "and %ld timed out. "
                 "For reference, your timeout cutoff is %ld seconds.",
                 guard->nickname, hex_str(guard->identity, DIGEST_LEN),
                 tor_lround(pathbias_get_close_success_count(guard)),
                 tor_lround(guard->circ_attempts),
                 tor_lround(pathbias_get_use_success_count(guard)),
                 tor_lround(guard->use_attempts),
                 tor_lround(guard->circ_successes),
                 tor_lround(guard->unusable_circuits),
                 tor_lround(guard->collapsed_circuits),
                 tor_lround(guard->timeouts),
                 tor_lround(get_circuit_build_close_time_ms()/1000));
          guard->path_bias_disabled = 1;
          guard->bad_since = approx_time();
          entry_guards_changed();
          return;
        }
      } else if (!guard->path_bias_extreme) {
        guard->path_bias_extreme = 1;
        log_warn(LD_CIRC,
                 "Your Guard %s ($%s) is failing an extremely large "
                 "amount of circuits. "
                 "This could indicate a route manipulation attack, "
                 "extreme network overload, or a bug. "
                 "Success counts are %ld/%ld. Use counts are %ld/%ld. "
                 "%ld circuits completed, %ld were unusable, %ld collapsed, "
                 "and %ld timed out. "
                 "For reference, your timeout cutoff is %ld seconds.",
                 guard->nickname, hex_str(guard->identity, DIGEST_LEN),
                 tor_lround(pathbias_get_close_success_count(guard)),
                 tor_lround(guard->circ_attempts),
                 tor_lround(pathbias_get_use_success_count(guard)),
                 tor_lround(guard->use_attempts),
                 tor_lround(guard->circ_successes),
                 tor_lround(guard->unusable_circuits),
                 tor_lround(guard->collapsed_circuits),
                 tor_lround(guard->timeouts),
                 tor_lround(get_circuit_build_close_time_ms()/1000));
      }
    } else if (pathbias_get_close_success_count(guard)/guard->circ_attempts
                < pathbias_get_warn_rate(options)) {
      if (!guard->path_bias_warned) {
        guard->path_bias_warned = 1;
        log_warn(LD_CIRC,
                 "Your Guard %s ($%s) is failing a very large "
                 "amount of circuits. "
                 "Most likely this means the Tor network is "
                 "overloaded, but it could also mean an attack against "
                 "you or potentially the guard itself. "
                 "Success counts are %ld/%ld. Use counts are %ld/%ld. "
                 "%ld circuits completed, %ld were unusable, %ld collapsed, "
                 "and %ld timed out. "
                 "For reference, your timeout cutoff is %ld seconds.",
                 guard->nickname, hex_str(guard->identity, DIGEST_LEN),
                 tor_lround(pathbias_get_close_success_count(guard)),
                 tor_lround(guard->circ_attempts),
                 tor_lround(pathbias_get_use_success_count(guard)),
                 tor_lround(guard->use_attempts),
                 tor_lround(guard->circ_successes),
                 tor_lround(guard->unusable_circuits),
                 tor_lround(guard->collapsed_circuits),
                 tor_lround(guard->timeouts),
                 tor_lround(get_circuit_build_close_time_ms()/1000));
      }
    } else if (pathbias_get_close_success_count(guard)/guard->circ_attempts
               < pathbias_get_notice_rate(options)) {
      if (!guard->path_bias_noticed) {
        guard->path_bias_noticed = 1;
        log_notice(LD_CIRC,
                 "Your Guard %s ($%s) is failing more circuits than "
                 "usual. "
                 "Most likely this means the Tor network is overloaded. "
                 "Success counts are %ld/%ld. Use counts are %ld/%ld. "
                 "%ld circuits completed, %ld were unusable, %ld collapsed, "
                 "and %ld timed out. "
                 "For reference, your timeout cutoff is %ld seconds.",
                 guard->nickname, hex_str(guard->identity, DIGEST_LEN),
                 tor_lround(pathbias_get_close_success_count(guard)),
                 tor_lround(guard->circ_attempts),
                 tor_lround(pathbias_get_use_success_count(guard)),
                 tor_lround(guard->use_attempts),
                 tor_lround(guard->circ_successes),
                 tor_lround(guard->unusable_circuits),
                 tor_lround(guard->collapsed_circuits),
                 tor_lround(guard->timeouts),
                 tor_lround(get_circuit_build_close_time_ms()/1000));
      }
    }
  }
}

/**
 * This function scales the path bias use rates if we have
 * more data than the scaling threshold. This allows us to
 * be more sensitive to recent measurements.
 *
 * XXX: The attempt count transfer stuff here might be done
 * better by keeping separate pending counters that get
 * transfered at circuit close. See ticket #8160.
 */
static void
pathbias_scale_close_rates(entry_guard_t *guard)
{
  const or_options_t *options = get_options();

  /* If we get a ton of circuits, just scale everything down */
  if (guard->circ_attempts > pathbias_get_scale_threshold(options)) {
    double scale_ratio = pathbias_get_scale_ratio(options);
    int opened_attempts = pathbias_count_circs_in_states(guard,
            PATH_STATE_BUILD_ATTEMPTED, PATH_STATE_BUILD_ATTEMPTED);
    int opened_built = pathbias_count_circs_in_states(guard,
                        PATH_STATE_BUILD_SUCCEEDED,
                        PATH_STATE_USE_FAILED);
    /* Verify that the counts are sane before and after scaling */
    int counts_are_sane = (guard->circ_attempts >= guard->circ_successes);

    guard->circ_attempts -= (opened_attempts+opened_built);
    guard->circ_successes -= opened_built;

    guard->circ_attempts *= scale_ratio;
    guard->circ_successes *= scale_ratio;
    guard->timeouts *= scale_ratio;
    guard->successful_circuits_closed *= scale_ratio;
    guard->collapsed_circuits *= scale_ratio;
    guard->unusable_circuits *= scale_ratio;

    guard->circ_attempts += (opened_attempts+opened_built);
    guard->circ_successes += opened_built;

    entry_guards_changed();

    log_info(LD_CIRC,
             "Scaled pathbias counts to (%f,%f)/%f (%d/%d open) for guard "
             "%s ($%s)",
             guard->circ_successes, guard->successful_circuits_closed,
             guard->circ_attempts, opened_built, opened_attempts,
             guard->nickname, hex_str(guard->identity, DIGEST_LEN));

    /* Have the counts just become invalid by this scaling attempt? */
    if (counts_are_sane && guard->circ_attempts < guard->circ_successes) {
      log_notice(LD_BUG,
               "Scaling has mangled pathbias counts to %f/%f (%d/%d open) "
               "for guard %s ($%s)",
               guard->circ_successes, guard->circ_attempts, opened_built,
               opened_attempts, guard->nickname,
               hex_str(guard->identity, DIGEST_LEN));
    }
  }
}

/**
 * This function scales the path bias circuit close rates if we have
 * more data than the scaling threshold. This allows us to be more
 * sensitive to recent measurements.
 *
 * XXX: The attempt count transfer stuff here might be done
 * better by keeping separate pending counters that get
 * transfered at circuit close. See ticket #8160.
 */
void
pathbias_scale_use_rates(entry_guard_t *guard)
{
  const or_options_t *options = get_options();

  /* If we get a ton of circuits, just scale everything down */
  if (guard->use_attempts > pathbias_get_scale_use_threshold(options)) {
    double scale_ratio = pathbias_get_scale_ratio(options);
    int opened_attempts = pathbias_count_circs_in_states(guard,
            PATH_STATE_USE_ATTEMPTED, PATH_STATE_USE_SUCCEEDED);
    /* Verify that the counts are sane before and after scaling */
    int counts_are_sane = (guard->use_attempts >= guard->use_successes);

    guard->use_attempts -= opened_attempts;

    guard->use_attempts *= scale_ratio;
    guard->use_successes *= scale_ratio;

    guard->use_attempts += opened_attempts;

    log_info(LD_CIRC,
           "Scaled pathbias use counts to %f/%f (%d open) for guard %s ($%s)",
           guard->use_successes, guard->use_attempts, opened_attempts,
           guard->nickname, hex_str(guard->identity, DIGEST_LEN));

    /* Have the counts just become invalid by this scaling attempt? */
    if (counts_are_sane && guard->use_attempts < guard->use_successes) {
      log_notice(LD_BUG,
               "Scaling has mangled pathbias usage counts to %f/%f "
               "(%d open) for guard %s ($%s)",
               guard->circ_successes, guard->circ_attempts,
               opened_attempts, guard->nickname,
               hex_str(guard->identity, DIGEST_LEN));
    }

    entry_guards_changed();
  }
}

