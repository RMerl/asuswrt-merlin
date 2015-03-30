/* Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file hibernate.c
 * \brief Functions to close listeners, stop allowing new circuits,
 * etc in preparation for closing down or going dormant; and to track
 * bandwidth and time intervals to know when to hibernate and when to
 * stop hibernating.
 **/

/*
hibernating, phase 1:
  - send destroy in response to create cells
  - send end (policy failed) in response to begin cells
  - close an OR conn when it has no circuits

hibernating, phase 2:
  (entered when bandwidth hard limit reached)
  - close all OR/AP/exit conns)
*/

#define HIBERNATE_PRIVATE
#include "or.h"
#include "channel.h"
#include "channeltls.h"
#include "config.h"
#include "connection.h"
#include "connection_edge.h"
#include "hibernate.h"
#include "main.h"
#include "router.h"
#include "statefile.h"

extern long stats_n_seconds_working; /* published uptime */

/** Are we currently awake, asleep, running out of bandwidth, or shutting
 * down? */
static hibernate_state_t hibernate_state = HIBERNATE_STATE_INITIAL;
/** If are hibernating, when do we plan to wake up? Set to 0 if we
 * aren't hibernating. */
static time_t hibernate_end_time = 0;
/** If we are shutting down, when do we plan finally exit? Set to 0 if
 * we aren't shutting down. */
static time_t shutdown_time = 0;

/** Possible accounting periods. */
typedef enum {
  UNIT_MONTH=1, UNIT_WEEK=2, UNIT_DAY=3,
} time_unit_t;

/* Fields for accounting logic.  Accounting overview:
 *
 * Accounting is designed to ensure that no more than N bytes are sent in
 * either direction over a given interval (currently, one month, one week, or
 * one day) We could
 * try to do this by choking our bandwidth to a trickle, but that
 * would make our streams useless.  Instead, we estimate what our
 * bandwidth usage will be, and guess how long we'll be able to
 * provide that much bandwidth before hitting our limit.  We then
 * choose a random time within the accounting interval to come up (so
 * that we don't get 50 Tors running on the 1st of the month and none
 * on the 30th).
 *
 * Each interval runs as follows:
 *
 * 1. We guess our bandwidth usage, based on how much we used
 *     last time.  We choose a "wakeup time" within the interval to come up.
 * 2. Until the chosen wakeup time, we hibernate.
 * 3. We come up at the wakeup time, and provide bandwidth until we are
 *    "very close" to running out.
 * 4. Then we go into low-bandwidth mode, and stop accepting new
 *    connections, but provide bandwidth until we run out.
 * 5. Then we hibernate until the end of the interval.
 *
 * If the interval ends before we run out of bandwidth, we go back to
 * step one.
 */

/** How many bytes have we read in this accounting interval? */
static uint64_t n_bytes_read_in_interval = 0;
/** How many bytes have we written in this accounting interval? */
static uint64_t n_bytes_written_in_interval = 0;
/** How many seconds have we been running this interval? */
static uint32_t n_seconds_active_in_interval = 0;
/** How many seconds were we active in this interval before we hit our soft
 * limit? */
static int n_seconds_to_hit_soft_limit = 0;
/** When in this interval was the soft limit hit. */
static time_t soft_limit_hit_at = 0;
/** How many bytes had we read/written when we hit the soft limit? */
static uint64_t n_bytes_at_soft_limit = 0;
/** When did this accounting interval start? */
static time_t interval_start_time = 0;
/** When will this accounting interval end? */
static time_t interval_end_time = 0;
/** How far into the accounting interval should we hibernate? */
static time_t interval_wakeup_time = 0;
/** How much bandwidth do we 'expect' to use per minute?  (0 if we have no
 * info from the last period.) */
static uint64_t expected_bandwidth_usage = 0;
/** What unit are we using for our accounting? */
static time_unit_t cfg_unit = UNIT_MONTH;

/** How many days,hours,minutes into each unit does our accounting interval
 * start? */
/** @{ */
static int cfg_start_day = 0,
           cfg_start_hour = 0,
           cfg_start_min = 0;
/** @} */

static void reset_accounting(time_t now);
static int read_bandwidth_usage(void);
static time_t start_of_accounting_period_after(time_t now);
static time_t start_of_accounting_period_containing(time_t now);
static void accounting_set_wakeup_time(void);

/* ************
 * Functions for bandwidth accounting.
 * ************/

/** Configure accounting start/end time settings based on
 * options->AccountingStart.  Return 0 on success, -1 on failure. If
 * <b>validate_only</b> is true, do not change the current settings. */
int
accounting_parse_options(const or_options_t *options, int validate_only)
{
  time_unit_t unit;
  int ok, idx;
  long d,h,m;
  smartlist_t *items;
  const char *v = options->AccountingStart;
  const char *s;
  char *cp;

  if (!v) {
    if (!validate_only) {
      cfg_unit = UNIT_MONTH;
      cfg_start_day = 1;
      cfg_start_hour = 0;
      cfg_start_min = 0;
    }
    return 0;
  }

  items = smartlist_new();
  smartlist_split_string(items, v, NULL,
                         SPLIT_SKIP_SPACE|SPLIT_IGNORE_BLANK,0);
  if (smartlist_len(items)<2) {
    log_warn(LD_CONFIG, "Too few arguments to AccountingStart");
    goto err;
  }
  s = smartlist_get(items,0);
  if (0==strcasecmp(s, "month")) {
    unit = UNIT_MONTH;
  } else if (0==strcasecmp(s, "week")) {
    unit = UNIT_WEEK;
  } else if (0==strcasecmp(s, "day")) {
    unit = UNIT_DAY;
  } else {
    log_warn(LD_CONFIG,
             "Unrecognized accounting unit '%s': only 'month', 'week',"
             " and 'day' are supported.", s);
    goto err;
  }

  switch (unit) {
  case UNIT_WEEK:
    d = tor_parse_long(smartlist_get(items,1), 10, 1, 7, &ok, NULL);
    if (!ok) {
      log_warn(LD_CONFIG, "Weekly accounting must begin on a day between "
               "1 (Monday) and 7 (Sunday)");
      goto err;
    }
    break;
  case UNIT_MONTH:
    d = tor_parse_long(smartlist_get(items,1), 10, 1, 28, &ok, NULL);
    if (!ok) {
      log_warn(LD_CONFIG, "Monthly accounting must begin on a day between "
               "1 and 28");
      goto err;
    }
    break;
  case UNIT_DAY:
    d = 0;
    break;
    /* Coverity dislikes unreachable default cases; some compilers warn on
     * switch statements missing a case.  Tell Coverity not to worry. */
    /* coverity[dead_error_begin] */
  default:
    tor_assert(0);
  }

  idx = unit==UNIT_DAY?1:2;
  if (smartlist_len(items) != (idx+1)) {
    log_warn(LD_CONFIG,"Accounting unit '%s' requires %d argument%s.",
             s, idx, (idx>1)?"s":"");
    goto err;
  }
  s = smartlist_get(items, idx);
  h = tor_parse_long(s, 10, 0, 23, &ok, &cp);
  if (!ok) {
    log_warn(LD_CONFIG,"Accounting start time not parseable: bad hour.");
    goto err;
  }
  if (!cp || *cp!=':') {
    log_warn(LD_CONFIG,
             "Accounting start time not parseable: not in HH:MM format");
    goto err;
  }
  m = tor_parse_long(cp+1, 10, 0, 59, &ok, &cp);
  if (!ok) {
    log_warn(LD_CONFIG, "Accounting start time not parseable: bad minute");
    goto err;
  }
  if (!cp || *cp!='\0') {
    log_warn(LD_CONFIG,
             "Accounting start time not parseable: not in HH:MM format");
    goto err;
  }

  if (!validate_only) {
    cfg_unit = unit;
    cfg_start_day = (int)d;
    cfg_start_hour = (int)h;
    cfg_start_min = (int)m;
  }
  SMARTLIST_FOREACH(items, char *, item, tor_free(item));
  smartlist_free(items);
  return 0;
 err:
  SMARTLIST_FOREACH(items, char *, item, tor_free(item));
  smartlist_free(items);
  return -1;
}

/** If we want to manage the accounting system and potentially
 * hibernate, return 1, else return 0.
 */
MOCK_IMPL(int,
accounting_is_enabled,(const or_options_t *options))
{
  if (options->AccountingMax)
    return 1;
  return 0;
}

/** If accounting is enabled, return how long (in seconds) this
 * interval lasts. */
int
accounting_get_interval_length(void)
{
  return (int)(interval_end_time - interval_start_time);
}

/** Return the time at which the current accounting interval will end. */
MOCK_IMPL(time_t,
accounting_get_end_time,(void))
{
  return interval_end_time;
}

/** Called from main.c to tell us that <b>seconds</b> seconds have
 * passed, <b>n_read</b> bytes have been read, and <b>n_written</b>
 * bytes have been written. */
void
accounting_add_bytes(size_t n_read, size_t n_written, int seconds)
{
  n_bytes_read_in_interval += n_read;
  n_bytes_written_in_interval += n_written;
  /* If we haven't been called in 10 seconds, we're probably jumping
   * around in time. */
  n_seconds_active_in_interval += (seconds < 10) ? seconds : 0;
}

/** If get_end, return the end of the accounting period that contains
 * the time <b>now</b>.  Else, return the start of the accounting
 * period that contains the time <b>now</b> */
static time_t
edge_of_accounting_period_containing(time_t now, int get_end)
{
  int before;
  struct tm tm;
  tor_localtime_r(&now, &tm);

  /* Set 'before' to true iff the current time is before the hh:mm
   * changeover time for today. */
  before = tm.tm_hour < cfg_start_hour ||
    (tm.tm_hour == cfg_start_hour && tm.tm_min < cfg_start_min);

  /* Dispatch by unit.  First, find the start day of the given period;
   * then, if get_end is true, increment to the end day. */
  switch (cfg_unit)
    {
    case UNIT_MONTH: {
      /* If this is before the Nth, we want the Nth of last month. */
      if (tm.tm_mday < cfg_start_day ||
          (tm.tm_mday < cfg_start_day && before)) {
        --tm.tm_mon;
      }
      /* Otherwise, the month is correct. */
      tm.tm_mday = cfg_start_day;
      if (get_end)
        ++tm.tm_mon;
      break;
    }
    case UNIT_WEEK: {
      /* What is the 'target' day of the week in struct tm format? (We
         say Sunday==7; struct tm says Sunday==0.) */
      int wday = cfg_start_day % 7;
      /* How many days do we subtract from today to get to the right day? */
      int delta = (7+tm.tm_wday-wday)%7;
      /* If we are on the right day, but the changeover hasn't happened yet,
       * then subtract a whole week. */
      if (delta == 0 && before)
        delta = 7;
      tm.tm_mday -= delta;
      if (get_end)
        tm.tm_mday += 7;
      break;
    }
    case UNIT_DAY:
      if (before)
        --tm.tm_mday;
      if (get_end)
        ++tm.tm_mday;
      break;
    default:
      tor_assert(0);
  }

  tm.tm_hour = cfg_start_hour;
  tm.tm_min = cfg_start_min;
  tm.tm_sec = 0;
  tm.tm_isdst = -1; /* Autodetect DST */
  return mktime(&tm);
}

/** Return the start of the accounting period containing the time
 * <b>now</b>. */
static time_t
start_of_accounting_period_containing(time_t now)
{
  return edge_of_accounting_period_containing(now, 0);
}

/** Return the start of the accounting period that comes after the one
 * containing the time <b>now</b>. */
static time_t
start_of_accounting_period_after(time_t now)
{
  return edge_of_accounting_period_containing(now, 1);
}

/** Return the length of the accounting period containing the time
 * <b>now</b>. */
static long
length_of_accounting_period_containing(time_t now)
{
  return edge_of_accounting_period_containing(now, 1) -
    edge_of_accounting_period_containing(now, 0);
}

/** Initialize the accounting subsystem. */
void
configure_accounting(time_t now)
{
  time_t s_now;
  /* Try to remember our recorded usage. */
  if (!interval_start_time)
    read_bandwidth_usage(); /* If we fail, we'll leave values at zero, and
                             * reset below.*/

  s_now = start_of_accounting_period_containing(now);

  if (!interval_start_time) {
    /* We didn't have recorded usage; Start a new interval. */
    log_info(LD_ACCT, "Starting new accounting interval.");
    reset_accounting(now);
  } else if (s_now == interval_start_time) {
    log_info(LD_ACCT, "Continuing accounting interval.");
    /* We are in the interval we thought we were in. Do nothing.*/
    interval_end_time = start_of_accounting_period_after(interval_start_time);
  } else {
    long duration =
      length_of_accounting_period_containing(interval_start_time);
    double delta = ((double)(s_now - interval_start_time)) / duration;
    if (-0.50 <= delta && delta <= 0.50) {
      /* The start of the period is now a little later or earlier than we
       * remembered.  That's fine; we might lose some bytes we could otherwise
       * have written, but better to err on the side of obeying people's
       * accounting settings. */
      log_info(LD_ACCT, "Accounting interval moved by %.02f%%; "
               "that's fine.", delta*100);
      interval_end_time = start_of_accounting_period_after(now);
    } else if (delta >= 0.99) {
      /* This is the regular time-moved-forward case; don't be too noisy
       * about it or people will complain */
      log_info(LD_ACCT, "Accounting interval elapsed; starting a new one");
      reset_accounting(now);
    } else {
      log_warn(LD_ACCT,
               "Mismatched accounting interval: moved by %.02f%%. "
               "Starting a fresh one.", delta*100);
      reset_accounting(now);
    }
  }
  accounting_set_wakeup_time();
}

/** Set expected_bandwidth_usage based on how much we sent/received
 * per minute last interval (if we were up for at least 30 minutes),
 * or based on our declared bandwidth otherwise. */
static void
update_expected_bandwidth(void)
{
  uint64_t expected;
  const or_options_t *options= get_options();
  uint64_t max_configured = (options->RelayBandwidthRate > 0 ?
                             options->RelayBandwidthRate :
                             options->BandwidthRate) * 60;

#define MIN_TIME_FOR_MEASUREMENT (1800)

  if (soft_limit_hit_at > interval_start_time && n_bytes_at_soft_limit &&
      (soft_limit_hit_at - interval_start_time) > MIN_TIME_FOR_MEASUREMENT) {
    /* If we hit our soft limit last time, only count the bytes up to that
     * time. This is a better predictor of our actual bandwidth than
     * considering the entirety of the last interval, since we likely started
     * using bytes very slowly once we hit our soft limit. */
    expected = n_bytes_at_soft_limit /
      (soft_limit_hit_at - interval_start_time);
    expected /= 60;
  } else if (n_seconds_active_in_interval >= MIN_TIME_FOR_MEASUREMENT) {
    /* Otherwise, we either measured enough time in the last interval but
     * never hit our soft limit, or we're using a state file from a Tor that
     * doesn't know to store soft-limit info.  Just take rate at which
     * we were reading/writing in the last interval as our expected rate.
     */
    uint64_t used = MAX(n_bytes_written_in_interval,
                        n_bytes_read_in_interval);
    expected = used / (n_seconds_active_in_interval / 60);
  } else {
    /* If we haven't gotten enough data last interval, set 'expected'
     * to 0.  This will set our wakeup to the start of the interval.
     * Next interval, we'll choose our starting time based on how much
     * we sent this interval.
     */
    expected = 0;
  }
  if (expected > max_configured)
    expected = max_configured;
  expected_bandwidth_usage = expected;
}

/** Called at the start of a new accounting interval: reset our
 * expected bandwidth usage based on what happened last time, set up
 * the start and end of the interval, and clear byte/time totals.
 */
static void
reset_accounting(time_t now)
{
  log_info(LD_ACCT, "Starting new accounting interval.");
  update_expected_bandwidth();
  interval_start_time = start_of_accounting_period_containing(now);
  interval_end_time = start_of_accounting_period_after(interval_start_time);
  n_bytes_read_in_interval = 0;
  n_bytes_written_in_interval = 0;
  n_seconds_active_in_interval = 0;
  n_bytes_at_soft_limit = 0;
  soft_limit_hit_at = 0;
  n_seconds_to_hit_soft_limit = 0;
}

/** Return true iff we should save our bandwidth usage to disk. */
static INLINE int
time_to_record_bandwidth_usage(time_t now)
{
  /* Note every 600 sec */
#define NOTE_INTERVAL (600)
  /* Or every 20 megabytes */
#define NOTE_BYTES 20*(1024*1024)
  static uint64_t last_read_bytes_noted = 0;
  static uint64_t last_written_bytes_noted = 0;
  static time_t last_time_noted = 0;

  if (last_time_noted + NOTE_INTERVAL <= now ||
      last_read_bytes_noted + NOTE_BYTES <= n_bytes_read_in_interval ||
      last_written_bytes_noted + NOTE_BYTES <= n_bytes_written_in_interval ||
      (interval_end_time && interval_end_time <= now)) {
    last_time_noted = now;
    last_read_bytes_noted = n_bytes_read_in_interval;
    last_written_bytes_noted = n_bytes_written_in_interval;
    return 1;
  }
  return 0;
}

/** Invoked once per second.  Checks whether it is time to hibernate,
 * record bandwidth used, etc.  */
void
accounting_run_housekeeping(time_t now)
{
  if (now >= interval_end_time) {
    configure_accounting(now);
  }
  if (time_to_record_bandwidth_usage(now)) {
    if (accounting_record_bandwidth_usage(now, get_or_state())) {
      log_warn(LD_FS, "Couldn't record bandwidth usage to disk.");
    }
  }
}

/** Based on our interval and our estimated bandwidth, choose a
 * deterministic (but random-ish) time to wake up. */
static void
accounting_set_wakeup_time(void)
{
  char digest[DIGEST_LEN];
  crypto_digest_t *d_env;
  uint64_t time_to_exhaust_bw;
  int time_to_consider;

  if (! server_identity_key_is_set()) {
    if (init_keys() < 0) {
      log_err(LD_BUG, "Error initializing keys");
      tor_assert(0);
    }
  }

  if (server_identity_key_is_set()) {
    char buf[ISO_TIME_LEN+1];
    format_iso_time(buf, interval_start_time);

    crypto_pk_get_digest(get_server_identity_key(), digest);

    d_env = crypto_digest_new();
    crypto_digest_add_bytes(d_env, buf, ISO_TIME_LEN);
    crypto_digest_add_bytes(d_env, digest, DIGEST_LEN);
    crypto_digest_get_digest(d_env, digest, DIGEST_LEN);
    crypto_digest_free(d_env);
  } else {
    crypto_rand(digest, DIGEST_LEN);
  }

  if (!expected_bandwidth_usage) {
    char buf1[ISO_TIME_LEN+1];
    char buf2[ISO_TIME_LEN+1];
    format_local_iso_time(buf1, interval_start_time);
    format_local_iso_time(buf2, interval_end_time);
    interval_wakeup_time = interval_start_time;

    log_notice(LD_ACCT,
           "Configured hibernation. This interval begins at %s "
           "and ends at %s. We have no prior estimate for bandwidth, so "
           "we will start out awake and hibernate when we exhaust our quota.",
           buf1, buf2);
    return;
  }

  time_to_exhaust_bw =
    (get_options()->AccountingMax/expected_bandwidth_usage)*60;
  if (time_to_exhaust_bw > INT_MAX) {
    time_to_exhaust_bw = INT_MAX;
    time_to_consider = 0;
  } else {
    time_to_consider = accounting_get_interval_length() -
                       (int)time_to_exhaust_bw;
  }

  if (time_to_consider<=0) {
    interval_wakeup_time = interval_start_time;
  } else {
    /* XXX can we simplify this just by picking a random (non-deterministic)
     * time to be up? If we go down and come up, then we pick a new one. Is
     * that good enough? -RD */

    /* This is not a perfectly unbiased conversion, but it is good enough:
     * in the worst case, the first half of the day is 0.06 percent likelier
     * to be chosen than the last half. */
    interval_wakeup_time = interval_start_time +
      (get_uint32(digest) % time_to_consider);
  }

  {
    char buf1[ISO_TIME_LEN+1];
    char buf2[ISO_TIME_LEN+1];
    char buf3[ISO_TIME_LEN+1];
    char buf4[ISO_TIME_LEN+1];
    time_t down_time;
    if (interval_wakeup_time+time_to_exhaust_bw > TIME_MAX)
      down_time = TIME_MAX;
    else
      down_time = (time_t)(interval_wakeup_time+time_to_exhaust_bw);
    if (down_time>interval_end_time)
      down_time = interval_end_time;
    format_local_iso_time(buf1, interval_start_time);
    format_local_iso_time(buf2, interval_wakeup_time);
    format_local_iso_time(buf3, down_time);
    format_local_iso_time(buf4, interval_end_time);

    log_notice(LD_ACCT,
           "Configured hibernation.  This interval began at %s; "
           "the scheduled wake-up time %s %s; "
           "we expect%s to exhaust our quota for this interval around %s; "
           "the next interval begins at %s (all times local)",
           buf1,
           time(NULL)<interval_wakeup_time?"is":"was", buf2,
           time(NULL)<down_time?"":"ed", buf3,
           buf4);
  }
}

/* This rounds 0 up to 1000, but that's actually a feature. */
#define ROUND_UP(x) (((x) + 0x3ff) & ~0x3ff)
/** Save all our bandwidth tracking information to disk. Return 0 on
 * success, -1 on failure. */
int
accounting_record_bandwidth_usage(time_t now, or_state_t *state)
{
  /* Just update the state */
  state->AccountingIntervalStart = interval_start_time;
  state->AccountingBytesReadInInterval = ROUND_UP(n_bytes_read_in_interval);
  state->AccountingBytesWrittenInInterval =
    ROUND_UP(n_bytes_written_in_interval);
  state->AccountingSecondsActive = n_seconds_active_in_interval;
  state->AccountingExpectedUsage = expected_bandwidth_usage;

  state->AccountingSecondsToReachSoftLimit = n_seconds_to_hit_soft_limit;
  state->AccountingSoftLimitHitAt = soft_limit_hit_at;
  state->AccountingBytesAtSoftLimit = n_bytes_at_soft_limit;

  or_state_mark_dirty(state,
                      now+(get_options()->AvoidDiskWrites ? 7200 : 60));

  return 0;
}
#undef ROUND_UP

/** Read stored accounting information from disk. Return 0 on success;
 * return -1 and change nothing on failure. */
static int
read_bandwidth_usage(void)
{
  or_state_t *state = get_or_state();

  {
    char *fname = get_datadir_fname("bw_accounting");
    int res;

    res = unlink(fname);
    if (res != 0) {
      log_warn(LD_FS,
               "Failed to unlink %s: %s",
               fname, strerror(errno));
    }

    tor_free(fname);
  }

  if (!state)
    return -1;

  log_info(LD_ACCT, "Reading bandwidth accounting data from state file");
  n_bytes_read_in_interval = state->AccountingBytesReadInInterval;
  n_bytes_written_in_interval = state->AccountingBytesWrittenInInterval;
  n_seconds_active_in_interval = state->AccountingSecondsActive;
  interval_start_time = state->AccountingIntervalStart;
  expected_bandwidth_usage = state->AccountingExpectedUsage;

  /* Older versions of Tor (before 0.2.2.17-alpha or so) didn't generate these
   * fields. If you switch back and forth, you might get an
   * AccountingSoftLimitHitAt value from long before the most recent
   * interval_start_time.  If that's so, then ignore the softlimit-related
   * values. */
  if (state->AccountingSoftLimitHitAt > interval_start_time) {
    soft_limit_hit_at =  state->AccountingSoftLimitHitAt;
    n_bytes_at_soft_limit = state->AccountingBytesAtSoftLimit;
    n_seconds_to_hit_soft_limit = state->AccountingSecondsToReachSoftLimit;
  } else {
    soft_limit_hit_at = 0;
    n_bytes_at_soft_limit = 0;
    n_seconds_to_hit_soft_limit = 0;
  }

  {
    char tbuf1[ISO_TIME_LEN+1];
    char tbuf2[ISO_TIME_LEN+1];
    format_iso_time(tbuf1, state->LastWritten);
    format_iso_time(tbuf2, state->AccountingIntervalStart);

    log_info(LD_ACCT,
       "Successfully read bandwidth accounting info from state written at %s "
       "for interval starting at %s.  We have been active for %lu seconds in "
       "this interval.  At the start of the interval, we expected to use "
       "about %lu KB per second. ("U64_FORMAT" bytes read so far, "
       U64_FORMAT" bytes written so far)",
       tbuf1, tbuf2,
       (unsigned long)n_seconds_active_in_interval,
       (unsigned long)(expected_bandwidth_usage*1024/60),
       U64_PRINTF_ARG(n_bytes_read_in_interval),
       U64_PRINTF_ARG(n_bytes_written_in_interval));
  }

  return 0;
}

/** Return true iff we have sent/received all the bytes we are willing
 * to send/receive this interval. */
static int
hibernate_hard_limit_reached(void)
{
  uint64_t hard_limit = get_options()->AccountingMax;
  if (!hard_limit)
    return 0;
  return n_bytes_read_in_interval >= hard_limit
    || n_bytes_written_in_interval >= hard_limit;
}

/** Return true iff we have sent/received almost all the bytes we are willing
 * to send/receive this interval. */
static int
hibernate_soft_limit_reached(void)
{
  const uint64_t acct_max = get_options()->AccountingMax;
#define SOFT_LIM_PCT (.95)
#define SOFT_LIM_BYTES (500*1024*1024)
#define SOFT_LIM_MINUTES (3*60)
  /* The 'soft limit' is a fair bit more complicated now than once it was.
   * We want to stop accepting connections when ALL of the following are true:
   *   - We expect to use up the remaining bytes in under 3 hours
   *   - We have used up 95% of our bytes.
   *   - We have less than 500MB of bytes left.
   */
  uint64_t soft_limit = DBL_TO_U64(U64_TO_DBL(acct_max) * SOFT_LIM_PCT);
  if (acct_max > SOFT_LIM_BYTES && acct_max - SOFT_LIM_BYTES > soft_limit) {
    soft_limit = acct_max - SOFT_LIM_BYTES;
  }
  if (expected_bandwidth_usage) {
    const uint64_t expected_usage =
      expected_bandwidth_usage * SOFT_LIM_MINUTES;
    if (acct_max > expected_usage && acct_max - expected_usage > soft_limit)
      soft_limit = acct_max - expected_usage;
  }

  if (!soft_limit)
    return 0;
  return n_bytes_read_in_interval >= soft_limit
    || n_bytes_written_in_interval >= soft_limit;
}

/** Called when we get a SIGINT, or when bandwidth soft limit is
 * reached. Puts us into "loose hibernation": we don't accept new
 * connections, but we continue handling old ones. */
static void
hibernate_begin(hibernate_state_t new_state, time_t now)
{
  const or_options_t *options = get_options();

  if (new_state == HIBERNATE_STATE_EXITING &&
      hibernate_state != HIBERNATE_STATE_LIVE) {
    log_notice(LD_GENERAL,"SIGINT received %s; exiting now.",
               hibernate_state == HIBERNATE_STATE_EXITING ?
               "a second time" : "while hibernating");
    tor_cleanup();
    exit(0);
  }

  if (new_state == HIBERNATE_STATE_LOWBANDWIDTH &&
      hibernate_state == HIBERNATE_STATE_LIVE) {
    soft_limit_hit_at = now;
    n_seconds_to_hit_soft_limit = n_seconds_active_in_interval;
    n_bytes_at_soft_limit = MAX(n_bytes_read_in_interval,
                                n_bytes_written_in_interval);
  }

  /* close listeners. leave control listener(s). */
  connection_mark_all_noncontrol_listeners();

  /* XXX kill intro point circs */
  /* XXX upload rendezvous service descriptors with no intro points */

  if (new_state == HIBERNATE_STATE_EXITING) {
    log_notice(LD_GENERAL,"Interrupt: we have stopped accepting new "
               "connections, and will shut down in %d seconds. Interrupt "
               "again to exit now.", options->ShutdownWaitLength);
    shutdown_time = time(NULL) + options->ShutdownWaitLength;
  } else { /* soft limit reached */
    hibernate_end_time = interval_end_time;
  }

  hibernate_state = new_state;
  accounting_record_bandwidth_usage(now, get_or_state());

  or_state_mark_dirty(get_or_state(),
                      get_options()->AvoidDiskWrites ? now+600 : 0);
}

/** Called when we've been hibernating and our timeout is reached. */
static void
hibernate_end(hibernate_state_t new_state)
{
  tor_assert(hibernate_state == HIBERNATE_STATE_LOWBANDWIDTH ||
             hibernate_state == HIBERNATE_STATE_DORMANT ||
             hibernate_state == HIBERNATE_STATE_INITIAL);

  /* listeners will be relaunched in run_scheduled_events() in main.c */
  if (hibernate_state != HIBERNATE_STATE_INITIAL)
    log_notice(LD_ACCT,"Hibernation period ended. Resuming normal activity.");

  hibernate_state = new_state;
  hibernate_end_time = 0; /* no longer hibernating */
  stats_n_seconds_working = 0; /* reset published uptime */
}

/** A wrapper around hibernate_begin, for when we get SIGINT. */
void
hibernate_begin_shutdown(void)
{
  hibernate_begin(HIBERNATE_STATE_EXITING, time(NULL));
}

/** Return true iff we are currently hibernating. */
MOCK_IMPL(int,
we_are_hibernating,(void))
{
  return hibernate_state != HIBERNATE_STATE_LIVE;
}

/** If we aren't currently dormant, close all connections and become
 * dormant. */
static void
hibernate_go_dormant(time_t now)
{
  connection_t *conn;

  if (hibernate_state == HIBERNATE_STATE_DORMANT)
    return;
  else if (hibernate_state == HIBERNATE_STATE_LOWBANDWIDTH)
    hibernate_state = HIBERNATE_STATE_DORMANT;
  else
    hibernate_begin(HIBERNATE_STATE_DORMANT, now);

  log_notice(LD_ACCT,"Going dormant. Blowing away remaining connections.");

  /* Close all OR/AP/exit conns. Leave dir conns because we still want
   * to be able to upload server descriptors so people know we're still
   * running, and download directories so we can detect if we're obsolete.
   * Leave control conns because we still want to be controllable.
   */
  while ((conn = connection_get_by_type(CONN_TYPE_OR)) ||
         (conn = connection_get_by_type(CONN_TYPE_AP)) ||
         (conn = connection_get_by_type(CONN_TYPE_EXIT))) {
    if (CONN_IS_EDGE(conn))
      connection_edge_end(TO_EDGE_CONN(conn), END_STREAM_REASON_HIBERNATING);
    log_info(LD_NET,"Closing conn type %d", conn->type);
    if (conn->type == CONN_TYPE_AP) /* send socks failure if needed */
      connection_mark_unattached_ap(TO_ENTRY_CONN(conn),
                                    END_STREAM_REASON_HIBERNATING);
    else if (conn->type == CONN_TYPE_OR) {
      if (TO_OR_CONN(conn)->chan) {
        channel_mark_for_close(TLS_CHAN_TO_BASE(TO_OR_CONN(conn)->chan));
      } else {
         connection_mark_for_close(conn);
      }
    } else
      connection_mark_for_close(conn);
  }

  if (now < interval_wakeup_time)
    hibernate_end_time = interval_wakeup_time;
  else
    hibernate_end_time = interval_end_time;

  accounting_record_bandwidth_usage(now, get_or_state());

  or_state_mark_dirty(get_or_state(),
                      get_options()->AvoidDiskWrites ? now+600 : 0);
}

/** Called when hibernate_end_time has arrived. */
static void
hibernate_end_time_elapsed(time_t now)
{
  char buf[ISO_TIME_LEN+1];

  /* The interval has ended, or it is wakeup time.  Find out which. */
  accounting_run_housekeeping(now);
  if (interval_wakeup_time <= now) {
    /* The interval hasn't changed, but interval_wakeup_time has passed.
     * It's time to wake up and start being a server. */
    hibernate_end(HIBERNATE_STATE_LIVE);
    return;
  } else {
    /* The interval has changed, and it isn't time to wake up yet. */
    hibernate_end_time = interval_wakeup_time;
    format_iso_time(buf,interval_wakeup_time);
    if (hibernate_state != HIBERNATE_STATE_DORMANT) {
      /* We weren't sleeping before; we should sleep now. */
      log_notice(LD_ACCT,
                 "Accounting period ended. Commencing hibernation until "
                 "%s UTC", buf);
      hibernate_go_dormant(now);
    } else {
      log_notice(LD_ACCT,
             "Accounting period ended. This period, we will hibernate"
             " until %s UTC",buf);
    }
  }
}

/** Consider our environment and decide if it's time
 * to start/stop hibernating.
 */
void
consider_hibernation(time_t now)
{
  int accounting_enabled = get_options()->AccountingMax != 0;
  char buf[ISO_TIME_LEN+1];

  /* If we're in 'exiting' mode, then we just shut down after the interval
   * elapses. */
  if (hibernate_state == HIBERNATE_STATE_EXITING) {
    tor_assert(shutdown_time);
    if (shutdown_time <= now) {
      log_notice(LD_GENERAL, "Clean shutdown finished. Exiting.");
      tor_cleanup();
      exit(0);
    }
    return; /* if exiting soon, don't worry about bandwidth limits */
  }

  if (hibernate_state == HIBERNATE_STATE_DORMANT) {
    /* We've been hibernating because of bandwidth accounting. */
    tor_assert(hibernate_end_time);
    if (hibernate_end_time > now && accounting_enabled) {
      /* If we're hibernating, don't wake up until it's time, regardless of
       * whether we're in a new interval. */
      return ;
    } else {
      hibernate_end_time_elapsed(now);
    }
  }

  /* Else, we aren't hibernating. See if it's time to start hibernating, or to
   * go dormant. */
  if (hibernate_state == HIBERNATE_STATE_LIVE ||
      hibernate_state == HIBERNATE_STATE_INITIAL) {
    if (hibernate_soft_limit_reached()) {
      log_notice(LD_ACCT,
                 "Bandwidth soft limit reached; commencing hibernation. "
                 "No new connections will be accepted");
      hibernate_begin(HIBERNATE_STATE_LOWBANDWIDTH, now);
    } else if (accounting_enabled && now < interval_wakeup_time) {
      format_local_iso_time(buf,interval_wakeup_time);
      log_notice(LD_ACCT,
                 "Commencing hibernation. We will wake up at %s local time.",
                 buf);
      hibernate_go_dormant(now);
    } else if (hibernate_state == HIBERNATE_STATE_INITIAL) {
      hibernate_end(HIBERNATE_STATE_LIVE);
    }
  }

  if (hibernate_state == HIBERNATE_STATE_LOWBANDWIDTH) {
    if (!accounting_enabled) {
      hibernate_end_time_elapsed(now);
    } else if (hibernate_hard_limit_reached()) {
      hibernate_go_dormant(now);
    } else if (hibernate_end_time <= now) {
      /* The hibernation period ended while we were still in lowbandwidth.*/
      hibernate_end_time_elapsed(now);
    }
  }
}

/** Helper function: called when we get a GETINFO request for an
 * accounting-related key on the control connection <b>conn</b>.  If we can
 * answer the request for <b>question</b>, then set *<b>answer</b> to a newly
 * allocated string holding the result.  Otherwise, set *<b>answer</b> to
 * NULL. */
int
getinfo_helper_accounting(control_connection_t *conn,
                          const char *question, char **answer,
                          const char **errmsg)
{
  (void) conn;
  (void) errmsg;
  if (!strcmp(question, "accounting/enabled")) {
    *answer = tor_strdup(accounting_is_enabled(get_options()) ? "1" : "0");
  } else if (!strcmp(question, "accounting/hibernating")) {
    if (hibernate_state == HIBERNATE_STATE_DORMANT)
      *answer = tor_strdup("hard");
    else if (hibernate_state == HIBERNATE_STATE_LOWBANDWIDTH)
      *answer = tor_strdup("soft");
    else
      *answer = tor_strdup("awake");
  } else if (!strcmp(question, "accounting/bytes")) {
    tor_asprintf(answer, U64_FORMAT" "U64_FORMAT,
                 U64_PRINTF_ARG(n_bytes_read_in_interval),
                 U64_PRINTF_ARG(n_bytes_written_in_interval));
  } else if (!strcmp(question, "accounting/bytes-left")) {
    uint64_t limit = get_options()->AccountingMax;
    uint64_t read_left = 0, write_left = 0;
    if (n_bytes_read_in_interval < limit)
      read_left = limit - n_bytes_read_in_interval;
    if (n_bytes_written_in_interval < limit)
      write_left = limit - n_bytes_written_in_interval;
    tor_asprintf(answer, U64_FORMAT" "U64_FORMAT,
                 U64_PRINTF_ARG(read_left), U64_PRINTF_ARG(write_left));
  } else if (!strcmp(question, "accounting/interval-start")) {
    *answer = tor_malloc(ISO_TIME_LEN+1);
    format_iso_time(*answer, interval_start_time);
  } else if (!strcmp(question, "accounting/interval-wake")) {
    *answer = tor_malloc(ISO_TIME_LEN+1);
    format_iso_time(*answer, interval_wakeup_time);
  } else if (!strcmp(question, "accounting/interval-end")) {
    *answer = tor_malloc(ISO_TIME_LEN+1);
    format_iso_time(*answer, interval_end_time);
  } else {
    *answer = NULL;
  }
  return 0;
}

#ifdef TOR_UNIT_TESTS
/**
 * Manually change the hibernation state.  Private; used only by the unit
 * tests.
 */
void
hibernate_set_state_for_testing_(hibernate_state_t newstate)
{
  hibernate_state = newstate;
}
#endif

