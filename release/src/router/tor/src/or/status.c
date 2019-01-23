/* Copyright (c) 2010-2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file status.c
 * \brief Collect status information and log heartbeat messages.
 *
 * This module is responsible for implementing the heartbeat log messages,
 * which periodically inform users and operators about basic facts to
 * do with their Tor instance.  The log_heartbeat() function, invoked from
 * main.c, is the principle entry point.  It collects data from elsewhere
 * in Tor, and logs it in a human-readable format.
 **/

#define STATUS_PRIVATE

#include "or.h"
#include "circuituse.h"
#include "config.h"
#include "status.h"
#include "nodelist.h"
#include "relay.h"
#include "router.h"
#include "circuitlist.h"
#include "main.h"
#include "rephist.h"
#include "hibernate.h"
#include "rephist.h"
#include "statefile.h"

static void log_accounting(const time_t now, const or_options_t *options);
#include "geoip.h"

/** Return the total number of circuits. */
STATIC int
count_circuits(void)
{
  return smartlist_len(circuit_get_global_list());
}

/** Take seconds <b>secs</b> and return a newly allocated human-readable
 * uptime string */
STATIC char *
secs_to_uptime(long secs)
{
  long int days = secs / 86400;
  int hours = (int)((secs - (days * 86400)) / 3600);
  int minutes = (int)((secs - (days * 86400) - (hours * 3600)) / 60);
  char *uptime_string = NULL;

  switch (days) {
  case 0:
    tor_asprintf(&uptime_string, "%d:%02d hours", hours, minutes);
    break;
  case 1:
    tor_asprintf(&uptime_string, "%ld day %d:%02d hours",
                 days, hours, minutes);
    break;
  default:
    tor_asprintf(&uptime_string, "%ld days %d:%02d hours",
                 days, hours, minutes);
    break;
  }

  return uptime_string;
}

/** Take <b>bytes</b> and returns a newly allocated human-readable usage
 * string. */
STATIC char *
bytes_to_usage(uint64_t bytes)
{
  char *bw_string = NULL;

  if (bytes < (1<<20)) { /* Less than a megabyte. */
    tor_asprintf(&bw_string, U64_FORMAT" kB", U64_PRINTF_ARG(bytes>>10));
  } else if (bytes < (1<<30)) { /* Megabytes. Let's add some precision. */
    double bw = U64_TO_DBL(bytes);
    tor_asprintf(&bw_string, "%.2f MB", bw/(1<<20));
  } else { /* Gigabytes. */
    double bw = U64_TO_DBL(bytes);
    tor_asprintf(&bw_string, "%.2f GB", bw/(1<<30));
  }

  return bw_string;
}

/** Log a "heartbeat" message describing Tor's status and history so that the
 * user can know that there is indeed a running Tor.  Return 0 on success and
 * -1 on failure. */
int
log_heartbeat(time_t now)
{
  char *bw_sent = NULL;
  char *bw_rcvd = NULL;
  char *uptime = NULL;
  const routerinfo_t *me;
  double r = tls_get_write_overhead_ratio();
  const int hibernating = we_are_hibernating();

  const or_options_t *options = get_options();

  if (public_server_mode(options) && !hibernating) {
    /* Let's check if we are in the current cached consensus. */
    if (!(me = router_get_my_routerinfo()))
      return -1; /* Something stinks, we won't even attempt this. */
    else
      if (!node_get_by_id(me->cache_info.identity_digest))
        log_fn(LOG_NOTICE, LD_HEARTBEAT, "Heartbeat: It seems like we are not "
               "in the cached consensus.");
  }

  uptime = secs_to_uptime(get_uptime());
  bw_rcvd = bytes_to_usage(get_bytes_read());
  bw_sent = bytes_to_usage(get_bytes_written());

  log_fn(LOG_NOTICE, LD_HEARTBEAT, "Heartbeat: Tor's uptime is %s, with %d "
         "circuits open. I've sent %s and received %s.%s",
         uptime, count_circuits(), bw_sent, bw_rcvd,
         hibernating?" We are currently hibernating.":"");

  if (server_mode(options) && accounting_is_enabled(options) && !hibernating) {
    log_accounting(now, options);
  }

  double fullness_pct = 100;
  if (stats_n_data_cells_packaged && !hibernating) {
    fullness_pct =
      100*(U64_TO_DBL(stats_n_data_bytes_packaged) /
           U64_TO_DBL(stats_n_data_cells_packaged*RELAY_PAYLOAD_SIZE));
  }
  const double overhead_pct = ( r - 1.0 ) * 100.0;

#define FULLNESS_PCT_THRESHOLD 80
#define TLS_OVERHEAD_THRESHOLD 15

  const int severity = (fullness_pct < FULLNESS_PCT_THRESHOLD ||
                        overhead_pct > TLS_OVERHEAD_THRESHOLD)
    ? LOG_NOTICE : LOG_INFO;

  log_fn(severity, LD_HEARTBEAT,
         "Average packaged cell fullness: %2.3f%%. "
         "TLS write overhead: %.f%%", fullness_pct, overhead_pct);

  if (public_server_mode(options)) {
    rep_hist_log_circuit_handshake_stats(now);
    rep_hist_log_link_protocol_counts();
  }

  circuit_log_ancient_one_hop_circuits(1800);

  if (options->BridgeRelay) {
    char *msg = NULL;
    msg = format_client_stats_heartbeat(now);
    if (msg)
      log_notice(LD_HEARTBEAT, "%s", msg);
    tor_free(msg);
  }

  tor_free(uptime);
  tor_free(bw_sent);
  tor_free(bw_rcvd);

  return 0;
}

static void
log_accounting(const time_t now, const or_options_t *options)
{
  or_state_t *state = get_or_state();
  char *acc_rcvd = bytes_to_usage(state->AccountingBytesReadInInterval);
  char *acc_sent = bytes_to_usage(state->AccountingBytesWrittenInInterval);
  char *acc_used = bytes_to_usage(get_accounting_bytes());
  uint64_t acc_bytes = options->AccountingMax;
  char *acc_max;
  time_t interval_end = accounting_get_end_time();
  char end_buf[ISO_TIME_LEN + 1];
  char *remaining = NULL;
  acc_max = bytes_to_usage(acc_bytes);
  format_local_iso_time(end_buf, interval_end);
  remaining = secs_to_uptime(interval_end - now);

  const char *acc_rule;
  switch (options->AccountingRule) {
    case ACCT_MAX: acc_rule = "max";
    break;
    case ACCT_SUM: acc_rule = "sum";
    break;
    case ACCT_OUT: acc_rule = "out";
    break;
    case ACCT_IN: acc_rule = "in";
    break;
    default: acc_rule = "max";
    break;
  }

  log_notice(LD_HEARTBEAT, "Heartbeat: Accounting enabled. "
      "Sent: %s, Received: %s, Used: %s / %s, Rule: %s. The "
      "current accounting interval ends on %s, in %s.",
      acc_sent, acc_rcvd, acc_used, acc_max, acc_rule, end_buf, remaining);

  tor_free(acc_rcvd);
  tor_free(acc_sent);
  tor_free(acc_used);
  tor_free(acc_max);
  tor_free(remaining);
}

