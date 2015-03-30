/* Copyright (c) 2001 Matej Pfajfar.
 * Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#define CIRCUITSTATS_PRIVATE

#include "or.h"
#include "circuitbuild.h"
#include "circuitstats.h"
#include "config.h"
#include "confparse.h"
#include "control.h"
#include "main.h"
#include "networkstatus.h"
#include "statefile.h"

#undef log
#include <math.h>

static void cbt_control_event_buildtimeout_set(
                                  const circuit_build_times_t *cbt,
                                  buildtimeout_set_event_t type);

#define CBT_BIN_TO_MS(bin) ((bin)*CBT_BIN_WIDTH + (CBT_BIN_WIDTH/2))

/** Global list of circuit build times */
// XXXX: Add this as a member for entry_guard_t instead of global?
// Then we could do per-guard statistics, as guards are likely to
// vary in their own latency. The downside of this is that guards
// can change frequently, so we'd be building a lot more circuits
// most likely.
static circuit_build_times_t circ_times;

#ifdef TOR_UNIT_TESTS
/** If set, we're running the unit tests: we should avoid clobbering
 * our state file or accessing get_options() or get_or_state() */
static int unit_tests = 0;
#else
#define unit_tests 0
#endif

/** Return a pointer to the data structure describing our current circuit
 * build time history and computations. */
const circuit_build_times_t *
get_circuit_build_times(void)
{
  return &circ_times;
}

/** As get_circuit_build_times, but return a mutable pointer. */
circuit_build_times_t *
get_circuit_build_times_mutable(void)
{
  return &circ_times;
}

/** Return the time to wait before actually closing an under-construction, in
 * milliseconds. */
double
get_circuit_build_close_time_ms(void)
{
  return circ_times.close_ms;
}

/** Return the time to wait before giving up on an under-construction circuit,
 * in milliseconds. */
double
get_circuit_build_timeout_ms(void)
{
  return circ_times.timeout_ms;
}

/**
 * This function decides if CBT learning should be disabled. It returns
 * true if one or more of the following four conditions are met:
 *
 *  1. If the cbtdisabled consensus parameter is set.
 *  2. If the torrc option LearnCircuitBuildTimeout is false.
 *  3. If we are a directory authority
 *  4. If we fail to write circuit build time history to our state file.
 */
int
circuit_build_times_disabled(void)
{
  if (unit_tests) {
    return 0;
  } else {
    int consensus_disabled = networkstatus_get_param(NULL, "cbtdisabled",
                                                     0, 0, 1);
    int config_disabled = !get_options()->LearnCircuitBuildTimeout;
    int dirauth_disabled = get_options()->AuthoritativeDir;
    int state_disabled = did_last_state_file_write_fail() ? 1 : 0;

    if (consensus_disabled || config_disabled || dirauth_disabled ||
           state_disabled) {
#if 0
      log_debug(LD_CIRC,
               "CircuitBuildTime learning is disabled. "
               "Consensus=%d, Config=%d, AuthDir=%d, StateFile=%d",
               consensus_disabled, config_disabled, dirauth_disabled,
               state_disabled);
#endif
      return 1;
    } else {
#if 0
      log_debug(LD_CIRC,
                "CircuitBuildTime learning is not disabled. "
                "Consensus=%d, Config=%d, AuthDir=%d, StateFile=%d",
                consensus_disabled, config_disabled, dirauth_disabled,
                state_disabled);
#endif
      return 0;
    }
  }
}

/**
 * Retrieve and bounds-check the cbtmaxtimeouts consensus paramter.
 *
 * Effect: When this many timeouts happen in the last 'cbtrecentcount'
 * circuit attempts, the client should discard all of its history and
 * begin learning a fresh timeout value.
 */
static int32_t
circuit_build_times_max_timeouts(void)
{
  int32_t cbt_maxtimeouts;

  cbt_maxtimeouts = networkstatus_get_param(NULL, "cbtmaxtimeouts",
                                 CBT_DEFAULT_MAX_RECENT_TIMEOUT_COUNT,
                                 CBT_MIN_MAX_RECENT_TIMEOUT_COUNT,
                                 CBT_MAX_MAX_RECENT_TIMEOUT_COUNT);

  if (!(get_options()->LearnCircuitBuildTimeout)) {
    log_debug(LD_BUG,
              "circuit_build_times_max_timeouts() called, cbtmaxtimeouts is"
              " %d",
              cbt_maxtimeouts);
  }

  return cbt_maxtimeouts;
}

/**
 * Retrieve and bounds-check the cbtnummodes consensus paramter.
 *
 * Effect: This value governs how many modes to use in the weighted
 * average calculation of Pareto parameter Xm. A value of 3 introduces
 * some bias (2-5% of CDF) under ideal conditions, but allows for better
 * performance in the event that a client chooses guard nodes of radically
 * different performance characteristics.
 */
static int32_t
circuit_build_times_default_num_xm_modes(void)
{
  int32_t num = networkstatus_get_param(NULL, "cbtnummodes",
                                        CBT_DEFAULT_NUM_XM_MODES,
                                        CBT_MIN_NUM_XM_MODES,
                                        CBT_MAX_NUM_XM_MODES);

  if (!(get_options()->LearnCircuitBuildTimeout)) {
    log_debug(LD_BUG,
              "circuit_build_times_default_num_xm_modes() called, cbtnummodes"
              " is %d",
              num);
  }

  return num;
}

/**
 * Retrieve and bounds-check the cbtmincircs consensus paramter.
 *
 * Effect: This is the minimum number of circuits to build before
 * computing a timeout.
 */
static int32_t
circuit_build_times_min_circs_to_observe(void)
{
  int32_t num = networkstatus_get_param(NULL, "cbtmincircs",
                                        CBT_DEFAULT_MIN_CIRCUITS_TO_OBSERVE,
                                        CBT_MIN_MIN_CIRCUITS_TO_OBSERVE,
                                        CBT_MAX_MIN_CIRCUITS_TO_OBSERVE);

  if (!(get_options()->LearnCircuitBuildTimeout)) {
    log_debug(LD_BUG,
              "circuit_build_times_min_circs_to_observe() called, cbtmincircs"
              " is %d",
              num);
  }

  return num;
}

/** Return true iff <b>cbt</b> has recorded enough build times that we
 * want to start acting on the timeout it implies. */
int
circuit_build_times_enough_to_compute(const circuit_build_times_t *cbt)
{
  return cbt->total_build_times >= circuit_build_times_min_circs_to_observe();
}

/**
 * Retrieve and bounds-check the cbtquantile consensus paramter.
 *
 * Effect: This is the position on the quantile curve to use to set the
 * timeout value. It is a percent (10-99).
 */
double
circuit_build_times_quantile_cutoff(void)
{
  int32_t num = networkstatus_get_param(NULL, "cbtquantile",
                                        CBT_DEFAULT_QUANTILE_CUTOFF,
                                        CBT_MIN_QUANTILE_CUTOFF,
                                        CBT_MAX_QUANTILE_CUTOFF);

  if (!(get_options()->LearnCircuitBuildTimeout)) {
    log_debug(LD_BUG,
              "circuit_build_times_quantile_cutoff() called, cbtquantile"
              " is %d",
              num);
  }

  return num/100.0;
}

/**
 * Retrieve and bounds-check the cbtclosequantile consensus paramter.
 *
 * Effect: This is the position on the quantile curve to use to set the
 * timeout value to use to actually close circuits. It is a percent
 * (0-99).
 */
static double
circuit_build_times_close_quantile(void)
{
  int32_t param;
  /* Cast is safe - circuit_build_times_quantile_cutoff() is capped */
  int32_t min = (int)tor_lround(100*circuit_build_times_quantile_cutoff());
  param = networkstatus_get_param(NULL, "cbtclosequantile",
             CBT_DEFAULT_CLOSE_QUANTILE,
             CBT_MIN_CLOSE_QUANTILE,
             CBT_MAX_CLOSE_QUANTILE);

  if (!(get_options()->LearnCircuitBuildTimeout)) {
    log_debug(LD_BUG,
              "circuit_build_times_close_quantile() called, cbtclosequantile"
              " is %d", param);
  }

  if (param < min) {
    log_warn(LD_DIR, "Consensus parameter cbtclosequantile is "
             "too small, raising to %d", min);
    param = min;
  }
  return param / 100.0;
}

/**
 * Retrieve and bounds-check the cbttestfreq consensus paramter.
 *
 * Effect: Describes how often in seconds to build a test circuit to
 * gather timeout values. Only applies if less than 'cbtmincircs'
 * have been recorded.
 */
static int32_t
circuit_build_times_test_frequency(void)
{
  int32_t num = networkstatus_get_param(NULL, "cbttestfreq",
                                        CBT_DEFAULT_TEST_FREQUENCY,
                                        CBT_MIN_TEST_FREQUENCY,
                                        CBT_MAX_TEST_FREQUENCY);

  if (!(get_options()->LearnCircuitBuildTimeout)) {
    log_debug(LD_BUG,
              "circuit_build_times_test_frequency() called, cbttestfreq is %d",
              num);
  }

  return num;
}

/**
 * Retrieve and bounds-check the cbtmintimeout consensus parameter.
 *
 * Effect: This is the minimum allowed timeout value in milliseconds.
 * The minimum is to prevent rounding to 0 (we only check once
 * per second).
 */
static int32_t
circuit_build_times_min_timeout(void)
{
  int32_t num = networkstatus_get_param(NULL, "cbtmintimeout",
                                        CBT_DEFAULT_TIMEOUT_MIN_VALUE,
                                        CBT_MIN_TIMEOUT_MIN_VALUE,
                                        CBT_MAX_TIMEOUT_MIN_VALUE);

  if (!(get_options()->LearnCircuitBuildTimeout)) {
    log_debug(LD_BUG,
              "circuit_build_times_min_timeout() called, cbtmintimeout is %d",
              num);
  }

  return num;
}

/**
 * Retrieve and bounds-check the cbtinitialtimeout consensus paramter.
 *
 * Effect: This is the timeout value to use before computing a timeout,
 * in milliseconds.
 */
int32_t
circuit_build_times_initial_timeout(void)
{
  int32_t min = circuit_build_times_min_timeout();
  int32_t param = networkstatus_get_param(NULL, "cbtinitialtimeout",
                                          CBT_DEFAULT_TIMEOUT_INITIAL_VALUE,
                                          CBT_MIN_TIMEOUT_INITIAL_VALUE,
                                          CBT_MAX_TIMEOUT_INITIAL_VALUE);

  if (!(get_options()->LearnCircuitBuildTimeout)) {
    log_debug(LD_BUG,
              "circuit_build_times_initial_timeout() called, "
              "cbtinitialtimeout is %d",
              param);
  }

  if (param < min) {
    log_warn(LD_DIR, "Consensus parameter cbtinitialtimeout is too small, "
             "raising to %d", min);
    param = min;
  }
  return param;
}

/**
 * Retrieve and bounds-check the cbtrecentcount consensus paramter.
 *
 * Effect: This is the number of circuit build times to keep track of
 * for deciding if we hit cbtmaxtimeouts and need to reset our state
 * and learn a new timeout.
 */
static int32_t
circuit_build_times_recent_circuit_count(networkstatus_t *ns)
{
  int32_t num;
  num = networkstatus_get_param(ns, "cbtrecentcount",
                                CBT_DEFAULT_RECENT_CIRCUITS,
                                CBT_MIN_RECENT_CIRCUITS,
                                CBT_MAX_RECENT_CIRCUITS);

  if (!(get_options()->LearnCircuitBuildTimeout)) {
    log_debug(LD_BUG,
              "circuit_build_times_recent_circuit_count() called, "
              "cbtrecentcount is %d",
              num);
  }

  return num;
}

/**
 * This function is called when we get a consensus update.
 *
 * It checks to see if we have changed any consensus parameters
 * that require reallocation or discard of previous stats.
 */
void
circuit_build_times_new_consensus_params(circuit_build_times_t *cbt,
                                         networkstatus_t *ns)
{
  int32_t num;

  /*
   * First check if we're doing adaptive timeouts at all; nothing to
   * update if we aren't.
   */

  if (!circuit_build_times_disabled()) {
    num = circuit_build_times_recent_circuit_count(ns);

    if (num > 0) {
      if (num != cbt->liveness.num_recent_circs) {
        int8_t *recent_circs;
        log_notice(LD_CIRC, "The Tor Directory Consensus has changed how many "
                   "circuits we must track to detect network failures from %d "
                   "to %d.", cbt->liveness.num_recent_circs, num);

        tor_assert(cbt->liveness.timeouts_after_firsthop ||
                   cbt->liveness.num_recent_circs == 0);

        /*
         * Technically this is a circular array that we are reallocating
         * and memcopying. However, since it only consists of either 1s
         * or 0s, and is only used in a statistical test to determine when
         * we should discard our history after a sufficient number of 1's
         * have been reached, it is fine if order is not preserved or
         * elements are lost.
         *
         * cbtrecentcount should only be changing in cases of severe network
         * distress anyway, so memory correctness here is paramount over
         * doing acrobatics to preserve the array.
         */
        recent_circs = tor_malloc_zero(sizeof(int8_t)*num);
        if (cbt->liveness.timeouts_after_firsthop &&
            cbt->liveness.num_recent_circs > 0) {
          memcpy(recent_circs, cbt->liveness.timeouts_after_firsthop,
                 sizeof(int8_t)*MIN(num, cbt->liveness.num_recent_circs));
        }

        // Adjust the index if it needs it.
        if (num < cbt->liveness.num_recent_circs) {
          cbt->liveness.after_firsthop_idx = MIN(num-1,
                  cbt->liveness.after_firsthop_idx);
        }

        tor_free(cbt->liveness.timeouts_after_firsthop);
        cbt->liveness.timeouts_after_firsthop = recent_circs;
        cbt->liveness.num_recent_circs = num;
      }
      /* else no change, nothing to do */
    } else { /* num == 0 */
      /*
       * Weird.  This probably shouldn't happen, so log a warning, but try
       * to do something sensible anyway.
       */

      log_warn(LD_CIRC,
               "The cbtrecentcircs consensus parameter came back zero!  "
               "This disables adaptive timeouts since we can't keep track of "
               "any recent circuits.");

      circuit_build_times_free_timeouts(cbt);
    }
  } else {
    /*
     * Adaptive timeouts are disabled; this might be because of the
     * LearnCircuitBuildTimes config parameter, and hence permanent, or
     * the cbtdisabled consensus parameter, so it may be a new condition.
     * Treat it like getting num == 0 above and free the circuit history
     * if we have any.
     */

    circuit_build_times_free_timeouts(cbt);
  }
}

/**
 * Return the initial default or configured timeout in milliseconds
 */
static double
circuit_build_times_get_initial_timeout(void)
{
  double timeout;

  /*
   * Check if we have LearnCircuitBuildTimeout, and if we don't,
   * always use CircuitBuildTimeout, no questions asked.
   */
  if (!unit_tests && get_options()->CircuitBuildTimeout) {
    timeout = get_options()->CircuitBuildTimeout*1000;
    if (get_options()->LearnCircuitBuildTimeout &&
        timeout < circuit_build_times_min_timeout()) {
      log_warn(LD_CIRC, "Config CircuitBuildTimeout too low. Setting to %ds",
               circuit_build_times_min_timeout()/1000);
      timeout = circuit_build_times_min_timeout();
    }
  } else {
    timeout = circuit_build_times_initial_timeout();
  }

  return timeout;
}

/**
 * Reset the build time state.
 *
 * Leave estimated parameters, timeout and network liveness intact
 * for future use.
 */
STATIC void
circuit_build_times_reset(circuit_build_times_t *cbt)
{
  memset(cbt->circuit_build_times, 0, sizeof(cbt->circuit_build_times));
  cbt->total_build_times = 0;
  cbt->build_times_idx = 0;
  cbt->have_computed_timeout = 0;
}

/**
 * Initialize the buildtimes structure for first use.
 *
 * Sets the initial timeout values based on either the config setting,
 * the consensus param, or the default (CBT_DEFAULT_TIMEOUT_INITIAL_VALUE).
 */
void
circuit_build_times_init(circuit_build_times_t *cbt)
{
  memset(cbt, 0, sizeof(*cbt));
  /*
   * Check if we really are using adaptive timeouts, and don't keep
   * track of this stuff if not.
   */
  if (!circuit_build_times_disabled()) {
    cbt->liveness.num_recent_circs =
      circuit_build_times_recent_circuit_count(NULL);
    cbt->liveness.timeouts_after_firsthop =
      tor_malloc_zero(sizeof(int8_t)*cbt->liveness.num_recent_circs);
  } else {
    cbt->liveness.num_recent_circs = 0;
    cbt->liveness.timeouts_after_firsthop = NULL;
  }
  cbt->close_ms = cbt->timeout_ms = circuit_build_times_get_initial_timeout();
  cbt_control_event_buildtimeout_set(cbt, BUILDTIMEOUT_SET_EVENT_RESET);
}

/**
 * Free the saved timeouts, if the cbtdisabled consensus parameter got turned
 * on or something.
 */

void
circuit_build_times_free_timeouts(circuit_build_times_t *cbt)
{
  if (!cbt) return;

  if (cbt->liveness.timeouts_after_firsthop) {
    tor_free(cbt->liveness.timeouts_after_firsthop);
  }

  cbt->liveness.num_recent_circs = 0;
}

#if 0
/**
 * Rewind our build time history by n positions.
 */
static void
circuit_build_times_rewind_history(circuit_build_times_t *cbt, int n)
{
  int i = 0;

  cbt->build_times_idx -= n;
  cbt->build_times_idx %= CBT_NCIRCUITS_TO_OBSERVE;

  for (i = 0; i < n; i++) {
    cbt->circuit_build_times[(i+cbt->build_times_idx)
                             %CBT_NCIRCUITS_TO_OBSERVE]=0;
  }

  if (cbt->total_build_times > n) {
    cbt->total_build_times -= n;
  } else {
    cbt->total_build_times = 0;
  }

  log_info(LD_CIRC,
          "Rewound history by %d places. Current index: %d. "
          "Total: %d", n, cbt->build_times_idx, cbt->total_build_times);
}
#endif

/**
 * Add a new build time value <b>time</b> to the set of build times. Time
 * units are milliseconds.
 *
 * circuit_build_times <b>cbt</b> is a circular array, so loop around when
 * array is full.
 */
int
circuit_build_times_add_time(circuit_build_times_t *cbt, build_time_t time)
{
  if (time <= 0 || time > CBT_BUILD_TIME_MAX) {
    log_warn(LD_BUG, "Circuit build time is too large (%u)."
                      "This is probably a bug.", time);
    tor_fragile_assert();
    return -1;
  }

  log_debug(LD_CIRC, "Adding circuit build time %u", time);

  cbt->circuit_build_times[cbt->build_times_idx] = time;
  cbt->build_times_idx = (cbt->build_times_idx + 1) % CBT_NCIRCUITS_TO_OBSERVE;
  if (cbt->total_build_times < CBT_NCIRCUITS_TO_OBSERVE)
    cbt->total_build_times++;

  if ((cbt->total_build_times % CBT_SAVE_STATE_EVERY) == 0) {
    /* Save state every n circuit builds */
    if (!unit_tests && !get_options()->AvoidDiskWrites)
      or_state_mark_dirty(get_or_state(), 0);
  }

  return 0;
}

/**
 * Return maximum circuit build time
 */
static build_time_t
circuit_build_times_max(const circuit_build_times_t *cbt)
{
  int i = 0;
  build_time_t max_build_time = 0;
  for (i = 0; i < CBT_NCIRCUITS_TO_OBSERVE; i++) {
    if (cbt->circuit_build_times[i] > max_build_time
            && cbt->circuit_build_times[i] != CBT_BUILD_ABANDONED)
      max_build_time = cbt->circuit_build_times[i];
  }
  return max_build_time;
}

#if 0
/** Return minimum circuit build time */
build_time_t
circuit_build_times_min(circuit_build_times_t *cbt)
{
  int i = 0;
  build_time_t min_build_time = CBT_BUILD_TIME_MAX;
  for (i = 0; i < CBT_NCIRCUITS_TO_OBSERVE; i++) {
    if (cbt->circuit_build_times[i] && /* 0 <-> uninitialized */
        cbt->circuit_build_times[i] < min_build_time)
      min_build_time = cbt->circuit_build_times[i];
  }
  if (min_build_time == CBT_BUILD_TIME_MAX) {
    log_warn(LD_CIRC, "No build times less than CBT_BUILD_TIME_MAX!");
  }
  return min_build_time;
}
#endif

/**
 * Calculate and return a histogram for the set of build times.
 *
 * Returns an allocated array of histrogram bins representing
 * the frequency of index*CBT_BIN_WIDTH millisecond
 * build times. Also outputs the number of bins in nbins.
 *
 * The return value must be freed by the caller.
 */
static uint32_t *
circuit_build_times_create_histogram(const circuit_build_times_t *cbt,
                                     build_time_t *nbins)
{
  uint32_t *histogram;
  build_time_t max_build_time = circuit_build_times_max(cbt);
  int i, c;

  *nbins = 1 + (max_build_time / CBT_BIN_WIDTH);
  histogram = tor_malloc_zero(*nbins * sizeof(build_time_t));

  // calculate histogram
  for (i = 0; i < CBT_NCIRCUITS_TO_OBSERVE; i++) {
    if (cbt->circuit_build_times[i] == 0
            || cbt->circuit_build_times[i] == CBT_BUILD_ABANDONED)
      continue; /* 0 <-> uninitialized */

    c = (cbt->circuit_build_times[i] / CBT_BIN_WIDTH);
    histogram[c]++;
  }

  return histogram;
}

/**
 * Return the Pareto start-of-curve parameter Xm.
 *
 * Because we are not a true Pareto curve, we compute this as the
 * weighted average of the N most frequent build time bins. N is either
 * 1 if we don't have enough circuit build time data collected, or
 * determined by the consensus parameter cbtnummodes (default 3).
 */
static build_time_t
circuit_build_times_get_xm(circuit_build_times_t *cbt)
{
  build_time_t i, nbins;
  build_time_t *nth_max_bin;
  int32_t bin_counts=0;
  build_time_t ret = 0;
  uint32_t *histogram = circuit_build_times_create_histogram(cbt, &nbins);
  int n=0;
  int num_modes = circuit_build_times_default_num_xm_modes();

  tor_assert(nbins > 0);
  tor_assert(num_modes > 0);

  // Only use one mode if < 1000 buildtimes. Not enough data
  // for multiple.
  if (cbt->total_build_times < CBT_NCIRCUITS_TO_OBSERVE)
    num_modes = 1;

  nth_max_bin = (build_time_t*)tor_malloc_zero(num_modes*sizeof(build_time_t));

  /* Determine the N most common build times */
  for (i = 0; i < nbins; i++) {
    if (histogram[i] >= histogram[nth_max_bin[0]]) {
      nth_max_bin[0] = i;
    }

    for (n = 1; n < num_modes; n++) {
      if (histogram[i] >= histogram[nth_max_bin[n]] &&
           (!histogram[nth_max_bin[n-1]]
               || histogram[i] < histogram[nth_max_bin[n-1]])) {
        nth_max_bin[n] = i;
      }
    }
  }

  for (n = 0; n < num_modes; n++) {
    bin_counts += histogram[nth_max_bin[n]];
    ret += CBT_BIN_TO_MS(nth_max_bin[n])*histogram[nth_max_bin[n]];
    log_info(LD_CIRC, "Xm mode #%d: %u %u", n, CBT_BIN_TO_MS(nth_max_bin[n]),
             histogram[nth_max_bin[n]]);
  }

  /* The following assert is safe, because we don't get called when we
   * haven't observed at least CBT_MIN_MIN_CIRCUITS_TO_OBSERVE circuits. */
  tor_assert(bin_counts > 0);

  ret /= bin_counts;
  tor_free(histogram);
  tor_free(nth_max_bin);

  return ret;
}

/**
 * Output a histogram of current circuit build times to
 * the or_state_t state structure.
 */
void
circuit_build_times_update_state(const circuit_build_times_t *cbt,
                                 or_state_t *state)
{
  uint32_t *histogram;
  build_time_t i = 0;
  build_time_t nbins = 0;
  config_line_t **next, *line;

  histogram = circuit_build_times_create_histogram(cbt, &nbins);
  // write to state
  config_free_lines(state->BuildtimeHistogram);
  next = &state->BuildtimeHistogram;
  *next = NULL;

  state->TotalBuildTimes = cbt->total_build_times;
  state->CircuitBuildAbandonedCount = 0;

  for (i = 0; i < CBT_NCIRCUITS_TO_OBSERVE; i++) {
    if (cbt->circuit_build_times[i] == CBT_BUILD_ABANDONED)
      state->CircuitBuildAbandonedCount++;
  }

  for (i = 0; i < nbins; i++) {
    // compress the histogram by skipping the blanks
    if (histogram[i] == 0) continue;
    *next = line = tor_malloc_zero(sizeof(config_line_t));
    line->key = tor_strdup("CircuitBuildTimeBin");
    tor_asprintf(&line->value, "%d %d",
            CBT_BIN_TO_MS(i), histogram[i]);
    next = &(line->next);
  }

  if (!unit_tests) {
    if (!get_options()->AvoidDiskWrites)
      or_state_mark_dirty(get_or_state(), 0);
  }

  tor_free(histogram);
}

/**
 * Shuffle the build times array.
 *
 * Adapted from http://en.wikipedia.org/wiki/Fisher-Yates_shuffle
 */
static void
circuit_build_times_shuffle_and_store_array(circuit_build_times_t *cbt,
                                            build_time_t *raw_times,
                                            uint32_t num_times)
{
  uint32_t n = num_times;
  if (num_times > CBT_NCIRCUITS_TO_OBSERVE) {
    log_notice(LD_CIRC, "The number of circuit times that this Tor version "
               "uses to calculate build times is less than the number stored "
               "in your state file. Decreasing the circuit time history from "
               "%lu to %d.", (unsigned long)num_times,
               CBT_NCIRCUITS_TO_OBSERVE);
  }

  if (n > INT_MAX-1) {
    log_warn(LD_CIRC, "For some insane reasons, you had %lu circuit build "
             "observations in your state file. That's far too many; probably "
             "there's a bug here.", (unsigned long)n);
    n = INT_MAX-1;
  }

  /* This code can only be run on a compact array */
  while (n-- > 1) {
    int k = crypto_rand_int(n + 1); /* 0 <= k <= n. */
    build_time_t tmp = raw_times[k];
    raw_times[k] = raw_times[n];
    raw_times[n] = tmp;
  }

  /* Since the times are now shuffled, take a random CBT_NCIRCUITS_TO_OBSERVE
   * subset (ie the first CBT_NCIRCUITS_TO_OBSERVE values) */
  for (n = 0; n < MIN(num_times, CBT_NCIRCUITS_TO_OBSERVE); n++) {
    circuit_build_times_add_time(cbt, raw_times[n]);
  }
}

/**
 * Filter old synthetic timeouts that were created before the
 * new right-censored Pareto calculation was deployed.
 *
 * Once all clients before 0.2.1.13-alpha are gone, this code
 * will be unused.
 */
static int
circuit_build_times_filter_timeouts(circuit_build_times_t *cbt)
{
  int num_filtered=0, i=0;
  double timeout_rate = 0;
  build_time_t max_timeout = 0;

  timeout_rate = circuit_build_times_timeout_rate(cbt);
  max_timeout = (build_time_t)cbt->close_ms;

  for (i = 0; i < CBT_NCIRCUITS_TO_OBSERVE; i++) {
    if (cbt->circuit_build_times[i] > max_timeout) {
      build_time_t replaced = cbt->circuit_build_times[i];
      num_filtered++;
      cbt->circuit_build_times[i] = CBT_BUILD_ABANDONED;

      log_debug(LD_CIRC, "Replaced timeout %d with %d", replaced,
               cbt->circuit_build_times[i]);
    }
  }

  log_info(LD_CIRC,
           "We had %d timeouts out of %d build times, "
           "and filtered %d above the max of %u",
          (int)(cbt->total_build_times*timeout_rate),
          cbt->total_build_times, num_filtered, max_timeout);

  return num_filtered;
}

/**
 * Load histogram from <b>state</b>, shuffling the resulting array
 * after we do so. Use this result to estimate parameters and
 * calculate the timeout.
 *
 * Return -1 on error.
 */
int
circuit_build_times_parse_state(circuit_build_times_t *cbt,
                                or_state_t *state)
{
  int tot_values = 0;
  uint32_t loaded_cnt = 0, N = 0;
  config_line_t *line;
  unsigned int i;
  build_time_t *loaded_times;
  int err = 0;
  circuit_build_times_init(cbt);

  if (circuit_build_times_disabled()) {
    return 0;
  }

  /* build_time_t 0 means uninitialized */
  loaded_times = tor_malloc_zero(sizeof(build_time_t)*state->TotalBuildTimes);

  for (line = state->BuildtimeHistogram; line; line = line->next) {
    smartlist_t *args = smartlist_new();
    smartlist_split_string(args, line->value, " ",
                           SPLIT_SKIP_SPACE|SPLIT_IGNORE_BLANK, 0);
    if (smartlist_len(args) < 2) {
      log_warn(LD_GENERAL, "Unable to parse circuit build times: "
                           "Too few arguments to CircuitBuildTime");
      err = 1;
      SMARTLIST_FOREACH(args, char*, cp, tor_free(cp));
      smartlist_free(args);
      break;
    } else {
      const char *ms_str = smartlist_get(args,0);
      const char *count_str = smartlist_get(args,1);
      uint32_t count, k;
      build_time_t ms;
      int ok;
      ms = (build_time_t)tor_parse_ulong(ms_str, 0, 0,
                                         CBT_BUILD_TIME_MAX, &ok, NULL);
      if (!ok) {
        log_warn(LD_GENERAL, "Unable to parse circuit build times: "
                             "Unparsable bin number");
        err = 1;
        SMARTLIST_FOREACH(args, char*, cp, tor_free(cp));
        smartlist_free(args);
        break;
      }
      count = (uint32_t)tor_parse_ulong(count_str, 0, 0,
                                        UINT32_MAX, &ok, NULL);
      if (!ok) {
        log_warn(LD_GENERAL, "Unable to parse circuit build times: "
                             "Unparsable bin count");
        err = 1;
        SMARTLIST_FOREACH(args, char*, cp, tor_free(cp));
        smartlist_free(args);
        break;
      }

      if (loaded_cnt+count+state->CircuitBuildAbandonedCount
            > state->TotalBuildTimes) {
        log_warn(LD_CIRC,
                 "Too many build times in state file. "
                 "Stopping short before %d",
                 loaded_cnt+count);
        SMARTLIST_FOREACH(args, char*, cp, tor_free(cp));
        smartlist_free(args);
        break;
      }

      for (k = 0; k < count; k++) {
        loaded_times[loaded_cnt++] = ms;
      }
      N++;
      SMARTLIST_FOREACH(args, char*, cp, tor_free(cp));
      smartlist_free(args);
    }
  }

  log_info(LD_CIRC,
           "Adding %d timeouts.", state->CircuitBuildAbandonedCount);
  for (i=0; i < state->CircuitBuildAbandonedCount; i++) {
    loaded_times[loaded_cnt++] = CBT_BUILD_ABANDONED;
  }

  if (loaded_cnt != state->TotalBuildTimes) {
    log_warn(LD_CIRC,
            "Corrupt state file? Build times count mismatch. "
            "Read %d times, but file says %d", loaded_cnt,
            state->TotalBuildTimes);
    err = 1;
    circuit_build_times_reset(cbt);
    goto done;
  }

  circuit_build_times_shuffle_and_store_array(cbt, loaded_times, loaded_cnt);

  /* Verify that we didn't overwrite any indexes */
  for (i=0; i < CBT_NCIRCUITS_TO_OBSERVE; i++) {
    if (!cbt->circuit_build_times[i])
      break;
    tot_values++;
  }
  log_info(LD_CIRC,
           "Loaded %d/%d values from %d lines in circuit time histogram",
           tot_values, cbt->total_build_times, N);

  if (cbt->total_build_times != tot_values
        || cbt->total_build_times > CBT_NCIRCUITS_TO_OBSERVE) {
    log_warn(LD_CIRC,
            "Corrupt state file? Shuffled build times mismatch. "
            "Read %d times, but file says %d", tot_values,
            state->TotalBuildTimes);
    err = 1;
    circuit_build_times_reset(cbt);
    goto done;
  }

  circuit_build_times_set_timeout(cbt);

  if (!state->CircuitBuildAbandonedCount && cbt->total_build_times) {
    circuit_build_times_filter_timeouts(cbt);
  }

 done:
  tor_free(loaded_times);
  return err ? -1 : 0;
}

/**
 * Estimates the Xm and Alpha parameters using
 * http://en.wikipedia.org/wiki/Pareto_distribution#Parameter_estimation
 *
 * The notable difference is that we use mode instead of min to estimate Xm.
 * This is because our distribution is frechet-like. We claim this is
 * an acceptable approximation because we are only concerned with the
 * accuracy of the CDF of the tail.
 */
STATIC int
circuit_build_times_update_alpha(circuit_build_times_t *cbt)
{
  build_time_t *x=cbt->circuit_build_times;
  double a = 0;
  int n=0,i=0,abandoned_count=0;
  build_time_t max_time=0;

  /* http://en.wikipedia.org/wiki/Pareto_distribution#Parameter_estimation */
  /* We sort of cheat here and make our samples slightly more pareto-like
   * and less frechet-like. */
  cbt->Xm = circuit_build_times_get_xm(cbt);

  tor_assert(cbt->Xm > 0);

  for (i=0; i< CBT_NCIRCUITS_TO_OBSERVE; i++) {
    if (!x[i]) {
      continue;
    }

    if (x[i] < cbt->Xm) {
      a += tor_mathlog(cbt->Xm);
    } else if (x[i] == CBT_BUILD_ABANDONED) {
      abandoned_count++;
    } else {
      a += tor_mathlog(x[i]);
      if (x[i] > max_time)
        max_time = x[i];
    }
    n++;
  }

  /*
   * We are erring and asserting here because this can only happen
   * in codepaths other than startup. The startup state parsing code
   * performs this same check, and resets state if it hits it. If we
   * hit it at runtime, something serious has gone wrong.
   */
  if (n!=cbt->total_build_times) {
    log_err(LD_CIRC, "Discrepancy in build times count: %d vs %d", n,
            cbt->total_build_times);
  }
  tor_assert(n==cbt->total_build_times);

  if (max_time <= 0) {
    /* This can happen if Xm is actually the *maximum* value in the set.
     * It can also happen if we've abandoned every single circuit somehow.
     * In either case, tell the caller not to compute a new build timeout. */
    log_warn(LD_BUG,
             "Could not determine largest build time (%d). "
             "Xm is %dms and we've abandoned %d out of %d circuits.", max_time,
             cbt->Xm, abandoned_count, n);
    return 0;
  }

  a += abandoned_count*tor_mathlog(max_time);

  a -= n*tor_mathlog(cbt->Xm);
  // Estimator comes from Eq #4 in:
  // "Bayesian estimation based on trimmed samples from Pareto populations"
  // by Arturo J. Fernández. We are right-censored only.
  a = (n-abandoned_count)/a;

  cbt->alpha = a;

  return 1;
}

/**
 * This is the Pareto Quantile Function. It calculates the point x
 * in the distribution such that F(x) = quantile (ie quantile*100%
 * of the mass of the density function is below x on the curve).
 *
 * We use it to calculate the timeout and also to generate synthetic
 * values of time for circuits that timeout before completion.
 *
 * See http://en.wikipedia.org/wiki/Quantile_function,
 * http://en.wikipedia.org/wiki/Inverse_transform_sampling and
 * http://en.wikipedia.org/wiki/Pareto_distribution#Generating_a_
 *     random_sample_from_Pareto_distribution
 * That's right. I'll cite wikipedia all day long.
 *
 * Return value is in milliseconds.
 */
STATIC double
circuit_build_times_calculate_timeout(circuit_build_times_t *cbt,
                                      double quantile)
{
  double ret;
  tor_assert(quantile >= 0);
  tor_assert(1.0-quantile > 0);
  tor_assert(cbt->Xm > 0);

  ret = cbt->Xm/pow(1.0-quantile,1.0/cbt->alpha);
  if (ret > INT32_MAX) {
    ret = INT32_MAX;
  }
  tor_assert(ret > 0);
  return ret;
}

#ifdef TOR_UNIT_TESTS
/** Pareto CDF */
double
circuit_build_times_cdf(circuit_build_times_t *cbt, double x)
{
  double ret;
  tor_assert(cbt->Xm > 0);
  ret = 1.0-pow(cbt->Xm/x,cbt->alpha);
  tor_assert(0 <= ret && ret <= 1.0);
  return ret;
}
#endif

#ifdef TOR_UNIT_TESTS
/**
 * Generate a synthetic time using our distribution parameters.
 *
 * The return value will be within the [q_lo, q_hi) quantile points
 * on the CDF.
 */
build_time_t
circuit_build_times_generate_sample(circuit_build_times_t *cbt,
                                    double q_lo, double q_hi)
{
  double randval = crypto_rand_double();
  build_time_t ret;
  double u;

  /* Generate between [q_lo, q_hi) */
  /*XXXX This is what nextafter is supposed to be for; we should use it on the
   * platforms that support it. */
  q_hi -= 1.0/(INT32_MAX);

  tor_assert(q_lo >= 0);
  tor_assert(q_hi < 1);
  tor_assert(q_lo < q_hi);

  u = q_lo + (q_hi-q_lo)*randval;

  tor_assert(0 <= u && u < 1.0);
  /* circuit_build_times_calculate_timeout returns <= INT32_MAX */
  ret = (build_time_t)
    tor_lround(circuit_build_times_calculate_timeout(cbt, u));
  tor_assert(ret > 0);
  return ret;
}
#endif

#ifdef TOR_UNIT_TESTS
/**
 * Estimate an initial alpha parameter by solving the quantile
 * function with a quantile point and a specific timeout value.
 */
void
circuit_build_times_initial_alpha(circuit_build_times_t *cbt,
                                  double quantile, double timeout_ms)
{
  // Q(u) = Xm/((1-u)^(1/a))
  // Q(0.8) = Xm/((1-0.8))^(1/a)) = CircBuildTimeout
  // CircBuildTimeout = Xm/((1-0.8))^(1/a))
  // CircBuildTimeout = Xm*((1-0.8))^(-1/a))
  // ln(CircBuildTimeout) = ln(Xm)+ln(((1-0.8)))*(-1/a)
  // -ln(1-0.8)/(ln(CircBuildTimeout)-ln(Xm))=a
  tor_assert(quantile >= 0);
  tor_assert(cbt->Xm > 0);
  cbt->alpha = tor_mathlog(1.0-quantile)/
    (tor_mathlog(cbt->Xm)-tor_mathlog(timeout_ms));
  tor_assert(cbt->alpha > 0);
}
#endif

/**
 * Returns true if we need circuits to be built
 */
int
circuit_build_times_needs_circuits(const circuit_build_times_t *cbt)
{
  /* Return true if < MIN_CIRCUITS_TO_OBSERVE */
  return !circuit_build_times_enough_to_compute(cbt);
}

/**
 * Returns true if we should build a timeout test circuit
 * right now.
 */
int
circuit_build_times_needs_circuits_now(const circuit_build_times_t *cbt)
{
  return circuit_build_times_needs_circuits(cbt) &&
    approx_time()-cbt->last_circ_at > circuit_build_times_test_frequency();
}

/**
 * How long should we be unreachable before we think we need to check if
 * our published IP address has changed.
 */
#define CIRCUIT_TIMEOUT_BEFORE_RECHECK_IP (60*3)

/**
 * Called to indicate that the network showed some signs of liveness,
 * i.e. we received a cell.
 *
 * This is used by circuit_build_times_network_check_live() to decide
 * if we should record the circuit build timeout or not.
 *
 * This function is called every time we receive a cell. Avoid
 * syscalls, events, and other high-intensity work.
 */
void
circuit_build_times_network_is_live(circuit_build_times_t *cbt)
{
  time_t now = approx_time();
  if (cbt->liveness.nonlive_timeouts > 0) {
    time_t time_since_live = now - cbt->liveness.network_last_live;
    log_notice(LD_CIRC,
               "Tor now sees network activity. Restoring circuit build "
               "timeout recording. Network was down for %d seconds "
               "during %d circuit attempts.",
               (int)time_since_live,
               cbt->liveness.nonlive_timeouts);
    if (time_since_live > CIRCUIT_TIMEOUT_BEFORE_RECHECK_IP)
      reschedule_descriptor_update_check();
  }
  cbt->liveness.network_last_live = now;
  cbt->liveness.nonlive_timeouts = 0;
}

/**
 * Called to indicate that we completed a circuit. Because this circuit
 * succeeded, it doesn't count as a timeout-after-the-first-hop.
 *
 * This is used by circuit_build_times_network_check_changed() to determine
 * if we had too many recent timeouts and need to reset our learned timeout
 * to something higher.
 */
void
circuit_build_times_network_circ_success(circuit_build_times_t *cbt)
{
  /* Check for NULLness because we might not be using adaptive timeouts */
  if (cbt->liveness.timeouts_after_firsthop &&
      cbt->liveness.num_recent_circs > 0) {
    cbt->liveness.timeouts_after_firsthop[cbt->liveness.after_firsthop_idx]
      = 0;
    cbt->liveness.after_firsthop_idx++;
    cbt->liveness.after_firsthop_idx %= cbt->liveness.num_recent_circs;
  }
}

/**
 * A circuit just timed out. If it failed after the first hop, record it
 * in our history for later deciding if the network speed has changed.
 *
 * This is used by circuit_build_times_network_check_changed() to determine
 * if we had too many recent timeouts and need to reset our learned timeout
 * to something higher.
 */
static void
circuit_build_times_network_timeout(circuit_build_times_t *cbt,
                                    int did_onehop)
{
  /* Check for NULLness because we might not be using adaptive timeouts */
  if (cbt->liveness.timeouts_after_firsthop &&
      cbt->liveness.num_recent_circs > 0) {
    if (did_onehop) {
      cbt->liveness.timeouts_after_firsthop[cbt->liveness.after_firsthop_idx]
        = 1;
      cbt->liveness.after_firsthop_idx++;
      cbt->liveness.after_firsthop_idx %= cbt->liveness.num_recent_circs;
    }
  }
}

/**
 * A circuit was just forcibly closed. If there has been no recent network
 * activity at all, but this circuit was launched back when we thought the
 * network was live, increment the number of "nonlive" circuit timeouts.
 *
 * This is used by circuit_build_times_network_check_live() to decide
 * if we should record the circuit build timeout or not.
 */
static void
circuit_build_times_network_close(circuit_build_times_t *cbt,
                                    int did_onehop, time_t start_time)
{
  time_t now = time(NULL);
  /*
   * Check if this is a timeout that was for a circuit that spent its
   * entire existence during a time where we have had no network activity.
   */
  if (cbt->liveness.network_last_live < start_time) {
    if (did_onehop) {
      char last_live_buf[ISO_TIME_LEN+1];
      char start_time_buf[ISO_TIME_LEN+1];
      char now_buf[ISO_TIME_LEN+1];
      format_local_iso_time(last_live_buf, cbt->liveness.network_last_live);
      format_local_iso_time(start_time_buf, start_time);
      format_local_iso_time(now_buf, now);
      log_notice(LD_CIRC,
               "A circuit somehow completed a hop while the network was "
               "not live. The network was last live at %s, but the circuit "
               "launched at %s. It's now %s. This could mean your clock "
               "changed.", last_live_buf, start_time_buf, now_buf);
    }
    cbt->liveness.nonlive_timeouts++;
    if (cbt->liveness.nonlive_timeouts == 1) {
      log_notice(LD_CIRC,
                 "Tor has not observed any network activity for the past %d "
                 "seconds. Disabling circuit build timeout recording.",
                 (int)(now - cbt->liveness.network_last_live));
    } else {
      log_info(LD_CIRC,
             "Got non-live timeout. Current count is: %d",
             cbt->liveness.nonlive_timeouts);
    }
  }
}

/**
 * When the network is not live, we do not record circuit build times.
 *
 * The network is considered not live if there has been at least one
 * circuit build that began and ended (had its close_ms measurement
 * period expire) since we last received a cell.
 *
 * Also has the side effect of rewinding the circuit time history
 * in the case of recent liveness changes.
 */
int
circuit_build_times_network_check_live(const circuit_build_times_t *cbt)
{
  if (cbt->liveness.nonlive_timeouts > 0) {
    return 0;
  }

  return 1;
}

/**
 * Returns true if we have seen more than MAX_RECENT_TIMEOUT_COUNT of
 * the past RECENT_CIRCUITS time out after the first hop. Used to detect
 * if the network connection has changed significantly, and if so,
 * resets our circuit build timeout to the default.
 *
 * Also resets the entire timeout history in this case and causes us
 * to restart the process of building test circuits and estimating a
 * new timeout.
 */
STATIC int
circuit_build_times_network_check_changed(circuit_build_times_t *cbt)
{
  int total_build_times = cbt->total_build_times;
  int timeout_count=0;
  int i;

  if (cbt->liveness.timeouts_after_firsthop &&
      cbt->liveness.num_recent_circs > 0) {
    /* how many of our recent circuits made it to the first hop but then
     * timed out? */
    for (i = 0; i < cbt->liveness.num_recent_circs; i++) {
      timeout_count += cbt->liveness.timeouts_after_firsthop[i];
    }
  }

  /* If 80% of our recent circuits are timing out after the first hop,
   * we need to re-estimate a new initial alpha and timeout. */
  if (timeout_count < circuit_build_times_max_timeouts()) {
    return 0;
  }

  circuit_build_times_reset(cbt);
  if (cbt->liveness.timeouts_after_firsthop &&
      cbt->liveness.num_recent_circs > 0) {
    memset(cbt->liveness.timeouts_after_firsthop, 0,
            sizeof(*cbt->liveness.timeouts_after_firsthop)*
            cbt->liveness.num_recent_circs);
  }
  cbt->liveness.after_firsthop_idx = 0;

  /* Check to see if this has happened before. If so, double the timeout
   * to give people on abysmally bad network connections a shot at access */
  if (cbt->timeout_ms >= circuit_build_times_get_initial_timeout()) {
    if (cbt->timeout_ms > INT32_MAX/2 || cbt->close_ms > INT32_MAX/2) {
      log_warn(LD_CIRC, "Insanely large circuit build timeout value. "
              "(timeout = %fmsec, close = %fmsec)",
               cbt->timeout_ms, cbt->close_ms);
    } else {
      cbt->timeout_ms *= 2;
      cbt->close_ms *= 2;
    }
  } else {
    cbt->close_ms = cbt->timeout_ms
                  = circuit_build_times_get_initial_timeout();
  }

  cbt_control_event_buildtimeout_set(cbt, BUILDTIMEOUT_SET_EVENT_RESET);

  log_notice(LD_CIRC,
            "Your network connection speed appears to have changed. Resetting "
            "timeout to %lds after %d timeouts and %d buildtimes.",
            tor_lround(cbt->timeout_ms/1000), timeout_count,
            total_build_times);

  return 1;
}

/**
 * Count the number of timeouts in a set of cbt data.
 */
double
circuit_build_times_timeout_rate(const circuit_build_times_t *cbt)
{
  int i=0,timeouts=0;
  for (i = 0; i < CBT_NCIRCUITS_TO_OBSERVE; i++) {
    if (cbt->circuit_build_times[i] >= cbt->timeout_ms) {
       timeouts++;
    }
  }

  if (!cbt->total_build_times)
    return 0;

  return ((double)timeouts)/cbt->total_build_times;
}

/**
 * Count the number of closed circuits in a set of cbt data.
 */
double
circuit_build_times_close_rate(const circuit_build_times_t *cbt)
{
  int i=0,closed=0;
  for (i = 0; i < CBT_NCIRCUITS_TO_OBSERVE; i++) {
    if (cbt->circuit_build_times[i] == CBT_BUILD_ABANDONED) {
       closed++;
    }
  }

  if (!cbt->total_build_times)
    return 0;

  return ((double)closed)/cbt->total_build_times;
}

/**
 * Store a timeout as a synthetic value.
 *
 * Returns true if the store was successful and we should possibly
 * update our timeout estimate.
 */
int
circuit_build_times_count_close(circuit_build_times_t *cbt,
                                int did_onehop,
                                time_t start_time)
{
  if (circuit_build_times_disabled()) {
    cbt->close_ms = cbt->timeout_ms
                  = circuit_build_times_get_initial_timeout();
    return 0;
  }

  /* Record this force-close to help determine if the network is dead */
  circuit_build_times_network_close(cbt, did_onehop, start_time);

  /* Only count timeouts if network is live.. */
  if (!circuit_build_times_network_check_live(cbt)) {
    return 0;
  }

  circuit_build_times_add_time(cbt, CBT_BUILD_ABANDONED);
  return 1;
}

/**
 * Update timeout counts to determine if we need to expire
 * our build time history due to excessive timeouts.
 *
 * We do not record any actual time values at this stage;
 * we are only interested in recording the fact that a timeout
 * happened. We record the time values via
 * circuit_build_times_count_close() and circuit_build_times_add_time().
 */
void
circuit_build_times_count_timeout(circuit_build_times_t *cbt,
                                  int did_onehop)
{
  if (circuit_build_times_disabled()) {
    cbt->close_ms = cbt->timeout_ms
                  = circuit_build_times_get_initial_timeout();
    return;
  }

  /* Register the fact that a timeout just occurred. */
  circuit_build_times_network_timeout(cbt, did_onehop);

  /* If there are a ton of timeouts, we should reset
   * the circuit build timeout. */
  circuit_build_times_network_check_changed(cbt);
}

/**
 * Estimate a new timeout based on history and set our timeout
 * variable accordingly.
 */
static int
circuit_build_times_set_timeout_worker(circuit_build_times_t *cbt)
{
  build_time_t max_time;
  if (!circuit_build_times_enough_to_compute(cbt))
    return 0;

  if (!circuit_build_times_update_alpha(cbt))
    return 0;

  cbt->timeout_ms = circuit_build_times_calculate_timeout(cbt,
                                circuit_build_times_quantile_cutoff());

  cbt->close_ms = circuit_build_times_calculate_timeout(cbt,
                                circuit_build_times_close_quantile());

  max_time = circuit_build_times_max(cbt);

  if (cbt->timeout_ms > max_time) {
    log_info(LD_CIRC,
               "Circuit build timeout of %dms is beyond the maximum build "
               "time we have ever observed. Capping it to %dms.",
               (int)cbt->timeout_ms, max_time);
    cbt->timeout_ms = max_time;
  }

  if (max_time < INT32_MAX/2 && cbt->close_ms > 2*max_time) {
    log_info(LD_CIRC,
               "Circuit build measurement period of %dms is more than twice "
               "the maximum build time we have ever observed. Capping it to "
               "%dms.", (int)cbt->close_ms, 2*max_time);
    cbt->close_ms = 2*max_time;
  }

  /* Sometimes really fast guard nodes give us such a steep curve
   * that this ends up being not that much greater than timeout_ms.
   * Make it be at least 1 min to handle this case. */
  cbt->close_ms = MAX(cbt->close_ms, circuit_build_times_initial_timeout());

  cbt->have_computed_timeout = 1;
  return 1;
}

/**
 * Exposed function to compute a new timeout. Dispatches events and
 * also filters out extremely high timeout values.
 */
void
circuit_build_times_set_timeout(circuit_build_times_t *cbt)
{
  long prev_timeout = tor_lround(cbt->timeout_ms/1000);
  double timeout_rate;

  /*
   * Just return if we aren't using adaptive timeouts
   */
  if (circuit_build_times_disabled())
    return;

  if (!circuit_build_times_set_timeout_worker(cbt))
    return;

  if (cbt->timeout_ms < circuit_build_times_min_timeout()) {
    log_info(LD_CIRC, "Set buildtimeout to low value %fms. Setting to %dms",
             cbt->timeout_ms, circuit_build_times_min_timeout());
    cbt->timeout_ms = circuit_build_times_min_timeout();
    if (cbt->close_ms < cbt->timeout_ms) {
      /* This shouldn't happen because of MAX() in timeout_worker above,
       * but doing it just in case */
      cbt->close_ms = circuit_build_times_initial_timeout();
    }
  }

  cbt_control_event_buildtimeout_set(cbt, BUILDTIMEOUT_SET_EVENT_COMPUTED);

  timeout_rate = circuit_build_times_timeout_rate(cbt);

  if (prev_timeout > tor_lround(cbt->timeout_ms/1000)) {
    log_info(LD_CIRC,
               "Based on %d circuit times, it looks like we don't need to "
               "wait so long for circuits to finish. We will now assume a "
               "circuit is too slow to use after waiting %ld seconds.",
               cbt->total_build_times,
               tor_lround(cbt->timeout_ms/1000));
    log_info(LD_CIRC,
             "Circuit timeout data: %fms, %fms, Xm: %d, a: %f, r: %f",
             cbt->timeout_ms, cbt->close_ms, cbt->Xm, cbt->alpha,
             timeout_rate);
  } else if (prev_timeout < tor_lround(cbt->timeout_ms/1000)) {
    log_info(LD_CIRC,
               "Based on %d circuit times, it looks like we need to wait "
               "longer for circuits to finish. We will now assume a "
               "circuit is too slow to use after waiting %ld seconds.",
               cbt->total_build_times,
               tor_lround(cbt->timeout_ms/1000));
    log_info(LD_CIRC,
             "Circuit timeout data: %fms, %fms, Xm: %d, a: %f, r: %f",
             cbt->timeout_ms, cbt->close_ms, cbt->Xm, cbt->alpha,
             timeout_rate);
  } else {
    log_info(LD_CIRC,
             "Set circuit build timeout to %lds (%fms, %fms, Xm: %d, a: %f,"
             " r: %f) based on %d circuit times",
             tor_lround(cbt->timeout_ms/1000),
             cbt->timeout_ms, cbt->close_ms, cbt->Xm, cbt->alpha, timeout_rate,
             cbt->total_build_times);
  }
}

#ifdef TOR_UNIT_TESTS
/** Make a note that we're running unit tests (rather than running Tor
 * itself), so we avoid clobbering our state file. */
void
circuitbuild_running_unit_tests(void)
{
  unit_tests = 1;
}
#endif

void
circuit_build_times_update_last_circ(circuit_build_times_t *cbt)
{
  cbt->last_circ_at = approx_time();
}

static void
cbt_control_event_buildtimeout_set(const circuit_build_times_t *cbt,
                                   buildtimeout_set_event_t type)
{
  char *args = NULL;
  double qnt;

  switch (type) {
    case BUILDTIMEOUT_SET_EVENT_RESET:
    case BUILDTIMEOUT_SET_EVENT_SUSPENDED:
    case BUILDTIMEOUT_SET_EVENT_DISCARD:
      qnt = 1.0;
      break;
    case BUILDTIMEOUT_SET_EVENT_COMPUTED:
    case BUILDTIMEOUT_SET_EVENT_RESUME:
    default:
      qnt = circuit_build_times_quantile_cutoff();
      break;
  }

  tor_asprintf(&args, "TOTAL_TIMES=%lu "
               "TIMEOUT_MS=%lu XM=%lu ALPHA=%f CUTOFF_QUANTILE=%f "
               "TIMEOUT_RATE=%f CLOSE_MS=%lu CLOSE_RATE=%f",
               (unsigned long)cbt->total_build_times,
               (unsigned long)cbt->timeout_ms,
               (unsigned long)cbt->Xm, cbt->alpha, qnt,
               circuit_build_times_timeout_rate(cbt),
               (unsigned long)cbt->close_ms,
               circuit_build_times_close_rate(cbt));

  control_event_buildtimeout_set(type, args);

  tor_free(args);
}

