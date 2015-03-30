/* Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file rephist.c
 * \brief Basic history and "reputation" functionality to remember
 *    which servers have worked in the past, how much bandwidth we've
 *    been using, which ports we tend to want, and so on; further,
 *    exit port statistics, cell statistics, and connection statistics.
 **/

#include "or.h"
#include "circuitlist.h"
#include "circuituse.h"
#include "config.h"
#include "networkstatus.h"
#include "nodelist.h"
#include "rephist.h"
#include "router.h"
#include "routerlist.h"
#include "ht.h"

static void bw_arrays_init(void);
static void predicted_ports_init(void);

/** Total number of bytes currently allocated in fields used by rephist.c. */
uint64_t rephist_total_alloc=0;
/** Number of or_history_t objects currently allocated. */
uint32_t rephist_total_num=0;

/** If the total weighted run count of all runs for a router ever falls
 * below this amount, the router can be treated as having 0 MTBF. */
#define STABILITY_EPSILON   0.0001
/** Value by which to discount all old intervals for MTBF purposes.  This
 * is compounded every STABILITY_INTERVAL. */
#define STABILITY_ALPHA     0.95
/** Interval at which to discount all old intervals for MTBF purposes. */
#define STABILITY_INTERVAL  (12*60*60)
/* (This combination of ALPHA, INTERVAL, and EPSILON makes it so that an
 * interval that just ended counts twice as much as one that ended a week ago,
 * 20X as much as one that ended a month ago, and routers that have had no
 * uptime data for about half a year will get forgotten.) */

/** History of an OR-\>OR link. */
typedef struct link_history_t {
  /** When did we start tracking this list? */
  time_t since;
  /** When did we most recently note a change to this link */
  time_t changed;
  /** How many times did extending from OR1 to OR2 succeed? */
  unsigned long n_extend_ok;
  /** How many times did extending from OR1 to OR2 fail? */
  unsigned long n_extend_fail;
} link_history_t;

/** History of an OR. */
typedef struct or_history_t {
  /** When did we start tracking this OR? */
  time_t since;
  /** When did we most recently note a change to this OR? */
  time_t changed;
  /** How many times did we successfully connect? */
  unsigned long n_conn_ok;
  /** How many times did we try to connect and fail?*/
  unsigned long n_conn_fail;
  /** How many seconds have we been connected to this OR before
   * 'up_since'? */
  unsigned long uptime;
  /** How many seconds have we been unable to connect to this OR before
   * 'down_since'? */
  unsigned long downtime;
  /** If nonzero, we have been connected since this time. */
  time_t up_since;
  /** If nonzero, we have been unable to connect since this time. */
  time_t down_since;

  /** The address at which we most recently connected to this OR
   * successfully. */
  tor_addr_t last_reached_addr;

  /** The port at which we most recently connected to this OR successfully */
  uint16_t last_reached_port;

  /* === For MTBF tracking: */
  /** Weighted sum total of all times that this router has been online.
   */
  unsigned long weighted_run_length;
  /** If the router is now online (according to stability-checking rules),
   * when did it come online? */
  time_t start_of_run;
  /** Sum of weights for runs in weighted_run_length. */
  double total_run_weights;
  /* === For fractional uptime tracking: */
  time_t start_of_downtime;
  unsigned long weighted_uptime;
  unsigned long total_weighted_time;

  /** Map from hex OR2 identity digest to a link_history_t for the link
   * from this OR to OR2. */
  digestmap_t *link_history_map;
} or_history_t;

/** When did we last multiply all routers' weighted_run_length and
 * total_run_weights by STABILITY_ALPHA? */
static time_t stability_last_downrated = 0;

/**  */
static time_t started_tracking_stability = 0;

/** Map from hex OR identity digest to or_history_t. */
static digestmap_t *history_map = NULL;

/** Return the or_history_t for the OR with identity digest <b>id</b>,
 * creating it if necessary. */
static or_history_t *
get_or_history(const char* id)
{
  or_history_t *hist;

  if (tor_digest_is_zero(id))
    return NULL;

  hist = digestmap_get(history_map, id);
  if (!hist) {
    hist = tor_malloc_zero(sizeof(or_history_t));
    rephist_total_alloc += sizeof(or_history_t);
    rephist_total_num++;
    hist->link_history_map = digestmap_new();
    hist->since = hist->changed = time(NULL);
    tor_addr_make_unspec(&hist->last_reached_addr);
    digestmap_set(history_map, id, hist);
  }
  return hist;
}

/** Return the link_history_t for the link from the first named OR to
 * the second, creating it if necessary. (ORs are identified by
 * identity digest.)
 */
static link_history_t *
get_link_history(const char *from_id, const char *to_id)
{
  or_history_t *orhist;
  link_history_t *lhist;
  orhist = get_or_history(from_id);
  if (!orhist)
    return NULL;
  if (tor_digest_is_zero(to_id))
    return NULL;
  lhist = (link_history_t*) digestmap_get(orhist->link_history_map, to_id);
  if (!lhist) {
    lhist = tor_malloc_zero(sizeof(link_history_t));
    rephist_total_alloc += sizeof(link_history_t);
    lhist->since = lhist->changed = time(NULL);
    digestmap_set(orhist->link_history_map, to_id, lhist);
  }
  return lhist;
}

/** Helper: free storage held by a single link history entry. */
static void
free_link_history_(void *val)
{
  rephist_total_alloc -= sizeof(link_history_t);
  tor_free(val);
}

/** Helper: free storage held by a single OR history entry. */
static void
free_or_history(void *_hist)
{
  or_history_t *hist = _hist;
  digestmap_free(hist->link_history_map, free_link_history_);
  rephist_total_alloc -= sizeof(or_history_t);
  rephist_total_num--;
  tor_free(hist);
}

/** Update an or_history_t object <b>hist</b> so that its uptime/downtime
 * count is up-to-date as of <b>when</b>.
 */
static void
update_or_history(or_history_t *hist, time_t when)
{
  tor_assert(hist);
  if (hist->up_since) {
    tor_assert(!hist->down_since);
    hist->uptime += (when - hist->up_since);
    hist->up_since = when;
  } else if (hist->down_since) {
    hist->downtime += (when - hist->down_since);
    hist->down_since = when;
  }
}

/** Initialize the static data structures for tracking history. */
void
rep_hist_init(void)
{
  history_map = digestmap_new();
  bw_arrays_init();
  predicted_ports_init();
}

/** Helper: note that we are no longer connected to the router with history
 * <b>hist</b>.  If <b>failed</b>, the connection failed; otherwise, it was
 * closed correctly. */
static void
mark_or_down(or_history_t *hist, time_t when, int failed)
{
  if (hist->up_since) {
    hist->uptime += (when - hist->up_since);
    hist->up_since = 0;
  }
  if (failed && !hist->down_since) {
    hist->down_since = when;
  }
}

/** Helper: note that we are connected to the router with history
 * <b>hist</b>. */
static void
mark_or_up(or_history_t *hist, time_t when)
{
  if (hist->down_since) {
    hist->downtime += (when - hist->down_since);
    hist->down_since = 0;
  }
  if (!hist->up_since) {
    hist->up_since = when;
  }
}

/** Remember that an attempt to connect to the OR with identity digest
 * <b>id</b> failed at <b>when</b>.
 */
void
rep_hist_note_connect_failed(const char* id, time_t when)
{
  or_history_t *hist;
  hist = get_or_history(id);
  if (!hist)
    return;
  ++hist->n_conn_fail;
  mark_or_down(hist, when, 1);
  hist->changed = when;
}

/** Remember that an attempt to connect to the OR with identity digest
 * <b>id</b> succeeded at <b>when</b>.
 */
void
rep_hist_note_connect_succeeded(const char* id, time_t when)
{
  or_history_t *hist;
  hist = get_or_history(id);
  if (!hist)
    return;
  ++hist->n_conn_ok;
  mark_or_up(hist, when);
  hist->changed = when;
}

/** Remember that we intentionally closed our connection to the OR
 * with identity digest <b>id</b> at <b>when</b>.
 */
void
rep_hist_note_disconnect(const char* id, time_t when)
{
  or_history_t *hist;
  hist = get_or_history(id);
  if (!hist)
    return;
  mark_or_down(hist, when, 0);
  hist->changed = when;
}

/** Remember that our connection to the OR with identity digest
 * <b>id</b> had an error and stopped working at <b>when</b>.
 */
void
rep_hist_note_connection_died(const char* id, time_t when)
{
  or_history_t *hist;
  if (!id) {
    /* If conn has no identity, it didn't complete its handshake, or something
     * went wrong.  Ignore it.
     */
    return;
  }
  hist = get_or_history(id);
  if (!hist)
    return;
  mark_or_down(hist, when, 1);
  hist->changed = when;
}

/** We have just decided that this router with identity digest <b>id</b> is
 * reachable, meaning we will give it a "Running" flag for the next while. */
void
rep_hist_note_router_reachable(const char *id, const tor_addr_t *at_addr,
                               const uint16_t at_port, time_t when)
{
  or_history_t *hist = get_or_history(id);
  int was_in_run = 1;
  char tbuf[ISO_TIME_LEN+1];
  int addr_changed, port_changed;

  tor_assert(hist);
  tor_assert((!at_addr && !at_port) || (at_addr && at_port));

  addr_changed = at_addr && !tor_addr_is_null(&hist->last_reached_addr) &&
    tor_addr_compare(at_addr, &hist->last_reached_addr, CMP_EXACT) != 0;
  port_changed = at_port && hist->last_reached_port &&
                 at_port != hist->last_reached_port;

  if (!started_tracking_stability)
    started_tracking_stability = time(NULL);
  if (!hist->start_of_run) {
    hist->start_of_run = when;
    was_in_run = 0;
  }
  if (hist->start_of_downtime) {
    long down_length;

    format_local_iso_time(tbuf, hist->start_of_downtime);
    log_info(LD_HIST, "Router %s is now Running; it had been down since %s.",
             hex_str(id, DIGEST_LEN), tbuf);
    if (was_in_run)
      log_info(LD_HIST, "  (Paradoxically, it was already Running too.)");

    down_length = when - hist->start_of_downtime;
    hist->total_weighted_time += down_length;
    hist->start_of_downtime = 0;
  } else if (addr_changed || port_changed) {
    /* If we're reachable, but the address changed, treat this as some
     * downtime. */
    int penalty = get_options()->TestingTorNetwork ? 240 : 3600;
    networkstatus_t *ns;

    if ((ns = networkstatus_get_latest_consensus())) {
      int fresh_interval = (int)(ns->fresh_until - ns->valid_after);
      int live_interval = (int)(ns->valid_until - ns->valid_after);
      /* on average, a descriptor addr change takes .5 intervals to make it
       * into a consensus, and half a liveness period to make it to
       * clients. */
      penalty = (int)(fresh_interval + live_interval) / 2;
    }
    format_local_iso_time(tbuf, hist->start_of_run);
    log_info(LD_HIST,"Router %s still seems Running, but its address appears "
             "to have changed since the last time it was reachable.  I'm "
             "going to treat it as having been down for %d seconds",
             hex_str(id, DIGEST_LEN), penalty);
    rep_hist_note_router_unreachable(id, when-penalty);
    rep_hist_note_router_reachable(id, NULL, 0, when);
  } else {
    format_local_iso_time(tbuf, hist->start_of_run);
    if (was_in_run)
      log_debug(LD_HIST, "Router %s is still Running; it has been Running "
                "since %s", hex_str(id, DIGEST_LEN), tbuf);
    else
      log_info(LD_HIST,"Router %s is now Running; it was previously untracked",
               hex_str(id, DIGEST_LEN));
  }
  if (at_addr)
    tor_addr_copy(&hist->last_reached_addr, at_addr);
  if (at_port)
    hist->last_reached_port = at_port;
}

/** We have just decided that this router is unreachable, meaning
 * we are taking away its "Running" flag. */
void
rep_hist_note_router_unreachable(const char *id, time_t when)
{
  or_history_t *hist = get_or_history(id);
  char tbuf[ISO_TIME_LEN+1];
  int was_running = 0;
  if (!started_tracking_stability)
    started_tracking_stability = time(NULL);

  tor_assert(hist);
  if (hist->start_of_run) {
    /*XXXX We could treat failed connections differently from failed
     * connect attempts. */
    long run_length = when - hist->start_of_run;
    format_local_iso_time(tbuf, hist->start_of_run);

    hist->total_run_weights += 1.0;
    hist->start_of_run = 0;
    if (run_length < 0) {
      unsigned long penalty = -run_length;
#define SUBTRACT_CLAMPED(var, penalty) \
      do { (var) = (var) < (penalty) ? 0 : (var) - (penalty); } while (0)

      SUBTRACT_CLAMPED(hist->weighted_run_length, penalty);
      SUBTRACT_CLAMPED(hist->weighted_uptime, penalty);
    } else {
      hist->weighted_run_length += run_length;
      hist->weighted_uptime += run_length;
      hist->total_weighted_time += run_length;
    }
    was_running = 1;
    log_info(LD_HIST, "Router %s is now non-Running: it had previously been "
             "Running since %s.  Its total weighted uptime is %lu/%lu.",
             hex_str(id, DIGEST_LEN), tbuf, hist->weighted_uptime,
             hist->total_weighted_time);
  }
  if (!hist->start_of_downtime) {
    hist->start_of_downtime = when;

    if (!was_running)
      log_info(LD_HIST, "Router %s is now non-Running; it was previously "
               "untracked.", hex_str(id, DIGEST_LEN));
  } else {
    if (!was_running) {
      format_local_iso_time(tbuf, hist->start_of_downtime);

      log_info(LD_HIST, "Router %s is still non-Running; it has been "
               "non-Running since %s.", hex_str(id, DIGEST_LEN), tbuf);
    }
  }
}

/** Mark a router with ID <b>id</b> as non-Running, and retroactively declare
 * that it has never been running: give it no stability and no WFU. */
void
rep_hist_make_router_pessimal(const char *id, time_t when)
{
  or_history_t *hist = get_or_history(id);
  tor_assert(hist);

  rep_hist_note_router_unreachable(id, when);
  mark_or_down(hist, when, 1);

  hist->weighted_run_length = 0;
  hist->weighted_uptime = 0;
}

/** Helper: Discount all old MTBF data, if it is time to do so.  Return
 * the time at which we should next discount MTBF data. */
time_t
rep_hist_downrate_old_runs(time_t now)
{
  digestmap_iter_t *orhist_it;
  const char *digest1;
  or_history_t *hist;
  void *hist_p;
  double alpha = 1.0;

  if (!history_map)
    history_map = digestmap_new();
  if (!stability_last_downrated)
    stability_last_downrated = now;
  if (stability_last_downrated + STABILITY_INTERVAL > now)
    return stability_last_downrated + STABILITY_INTERVAL;

  /* Okay, we should downrate the data.  By how much? */
  while (stability_last_downrated + STABILITY_INTERVAL < now) {
    stability_last_downrated += STABILITY_INTERVAL;
    alpha *= STABILITY_ALPHA;
  }

  log_info(LD_HIST, "Discounting all old stability info by a factor of %f",
           alpha);

  /* Multiply every w_r_l, t_r_w pair by alpha. */
  for (orhist_it = digestmap_iter_init(history_map);
       !digestmap_iter_done(orhist_it);
       orhist_it = digestmap_iter_next(history_map,orhist_it)) {
    digestmap_iter_get(orhist_it, &digest1, &hist_p);
    hist = hist_p;

    hist->weighted_run_length =
      (unsigned long)(hist->weighted_run_length * alpha);
    hist->total_run_weights *= alpha;

    hist->weighted_uptime = (unsigned long)(hist->weighted_uptime * alpha);
    hist->total_weighted_time = (unsigned long)
      (hist->total_weighted_time * alpha);
  }

  return stability_last_downrated + STABILITY_INTERVAL;
}

/** Helper: Return the weighted MTBF of the router with history <b>hist</b>. */
static double
get_stability(or_history_t *hist, time_t when)
{
  long total = hist->weighted_run_length;
  double total_weights = hist->total_run_weights;

  if (hist->start_of_run) {
    /* We're currently in a run.  Let total and total_weights hold the values
     * they would hold if the current run were to end now. */
    total += (when-hist->start_of_run);
    total_weights += 1.0;
  }
  if (total_weights < STABILITY_EPSILON) {
    /* Round down to zero, and avoid divide-by-zero. */
    return 0.0;
  }

  return total / total_weights;
}

/** Return the total amount of time we've been observing, with each run of
 * time downrated by the appropriate factor. */
static long
get_total_weighted_time(or_history_t *hist, time_t when)
{
  long total = hist->total_weighted_time;
  if (hist->start_of_run) {
    total += (when - hist->start_of_run);
  } else if (hist->start_of_downtime) {
    total += (when - hist->start_of_downtime);
  }
  return total;
}

/** Helper: Return the weighted percent-of-time-online of the router with
 * history <b>hist</b>. */
static double
get_weighted_fractional_uptime(or_history_t *hist, time_t when)
{
  long total = hist->total_weighted_time;
  long up = hist->weighted_uptime;

  if (hist->start_of_run) {
    long run_length = (when - hist->start_of_run);
    up += run_length;
    total += run_length;
  } else if (hist->start_of_downtime) {
    total += (when - hist->start_of_downtime);
  }

  if (!total) {
    /* Avoid calling anybody's uptime infinity (which should be impossible if
     * the code is working), or NaN (which can happen for any router we haven't
     * observed up or down yet). */
    return 0.0;
  }

  return ((double) up) / total;
}

/** Return how long the router whose identity digest is <b>id</b> has
 *  been reachable. Return 0 if the router is unknown or currently deemed
 *  unreachable. */
long
rep_hist_get_uptime(const char *id, time_t when)
{
  or_history_t *hist = get_or_history(id);
  if (!hist)
    return 0;
  if (!hist->start_of_run || when < hist->start_of_run)
    return 0;
  return when - hist->start_of_run;
}

/** Return an estimated MTBF for the router whose identity digest is
 * <b>id</b>. Return 0 if the router is unknown. */
double
rep_hist_get_stability(const char *id, time_t when)
{
  or_history_t *hist = get_or_history(id);
  if (!hist)
    return 0.0;

  return get_stability(hist, when);
}

/** Return an estimated percent-of-time-online for the router whose identity
 * digest is <b>id</b>. Return 0 if the router is unknown. */
double
rep_hist_get_weighted_fractional_uptime(const char *id, time_t when)
{
  or_history_t *hist = get_or_history(id);
  if (!hist)
    return 0.0;

  return get_weighted_fractional_uptime(hist, when);
}

/** Return a number representing how long we've known about the router whose
 * digest is <b>id</b>. Return 0 if the router is unknown.
 *
 * Be careful: this measure increases monotonically as we know the router for
 * longer and longer, but it doesn't increase linearly.
 */
long
rep_hist_get_weighted_time_known(const char *id, time_t when)
{
  or_history_t *hist = get_or_history(id);
  if (!hist)
    return 0;

  return get_total_weighted_time(hist, when);
}

/** Return true if we've been measuring MTBFs for long enough to
 * pronounce on Stability. */
int
rep_hist_have_measured_enough_stability(void)
{
  /* XXXX023 This doesn't do so well when we change our opinion
   * as to whether we're tracking router stability. */
  return started_tracking_stability < time(NULL) - 4*60*60;
}

/** Remember that we successfully extended from the OR with identity
 * digest <b>from_id</b> to the OR with identity digest
 * <b>to_name</b>.
 */
void
rep_hist_note_extend_succeeded(const char *from_id, const char *to_id)
{
  link_history_t *hist;
  /* log_fn(LOG_WARN, "EXTEND SUCCEEDED: %s->%s",from_name,to_name); */
  hist = get_link_history(from_id, to_id);
  if (!hist)
    return;
  ++hist->n_extend_ok;
  hist->changed = time(NULL);
}

/** Remember that we tried to extend from the OR with identity digest
 * <b>from_id</b> to the OR with identity digest <b>to_name</b>, but
 * failed.
 */
void
rep_hist_note_extend_failed(const char *from_id, const char *to_id)
{
  link_history_t *hist;
  /* log_fn(LOG_WARN, "EXTEND FAILED: %s->%s",from_name,to_name); */
  hist = get_link_history(from_id, to_id);
  if (!hist)
    return;
  ++hist->n_extend_fail;
  hist->changed = time(NULL);
}

/** Log all the reliability data we have remembered, with the chosen
 * severity.
 */
void
rep_hist_dump_stats(time_t now, int severity)
{
  digestmap_iter_t *lhist_it;
  digestmap_iter_t *orhist_it;
  const char *name1, *name2, *digest1, *digest2;
  char hexdigest1[HEX_DIGEST_LEN+1];
  char hexdigest2[HEX_DIGEST_LEN+1];
  or_history_t *or_history;
  link_history_t *link_history;
  void *or_history_p, *link_history_p;
  double uptime;
  char buffer[2048];
  size_t len;
  int ret;
  unsigned long upt, downt;
  const node_t *node;

  rep_history_clean(now - get_options()->RephistTrackTime);

  tor_log(severity, LD_HIST, "--------------- Dumping history information:");

  for (orhist_it = digestmap_iter_init(history_map);
       !digestmap_iter_done(orhist_it);
       orhist_it = digestmap_iter_next(history_map,orhist_it)) {
    double s;
    long stability;
    digestmap_iter_get(orhist_it, &digest1, &or_history_p);
    or_history = (or_history_t*) or_history_p;

    if ((node = node_get_by_id(digest1)) && node_get_nickname(node))
      name1 = node_get_nickname(node);
    else
      name1 = "(unknown)";
    base16_encode(hexdigest1, sizeof(hexdigest1), digest1, DIGEST_LEN);
    update_or_history(or_history, now);
    upt = or_history->uptime;
    downt = or_history->downtime;
    s = get_stability(or_history, now);
    stability = (long)s;
    if (upt+downt) {
      uptime = ((double)upt) / (upt+downt);
    } else {
      uptime=1.0;
    }
    tor_log(severity, LD_HIST,
        "OR %s [%s]: %ld/%ld good connections; uptime %ld/%ld sec (%.2f%%); "
        "wmtbf %lu:%02lu:%02lu",
        name1, hexdigest1,
        or_history->n_conn_ok, or_history->n_conn_fail+or_history->n_conn_ok,
        upt, upt+downt, uptime*100.0,
        stability/3600, (stability/60)%60, stability%60);

    if (!digestmap_isempty(or_history->link_history_map)) {
      strlcpy(buffer, "    Extend attempts: ", sizeof(buffer));
      len = strlen(buffer);
      for (lhist_it = digestmap_iter_init(or_history->link_history_map);
           !digestmap_iter_done(lhist_it);
           lhist_it = digestmap_iter_next(or_history->link_history_map,
                                          lhist_it)) {
        digestmap_iter_get(lhist_it, &digest2, &link_history_p);
        if ((node = node_get_by_id(digest2)) && node_get_nickname(node))
          name2 = node_get_nickname(node);
        else
          name2 = "(unknown)";

        link_history = (link_history_t*) link_history_p;

        base16_encode(hexdigest2, sizeof(hexdigest2), digest2, DIGEST_LEN);
        ret = tor_snprintf(buffer+len, 2048-len, "%s [%s](%ld/%ld); ",
                        name2,
                        hexdigest2,
                        link_history->n_extend_ok,
                        link_history->n_extend_ok+link_history->n_extend_fail);
        if (ret<0)
          break;
        else
          len += ret;
      }
      tor_log(severity, LD_HIST, "%s", buffer);
    }
  }
}

/** Remove history info for routers/links that haven't changed since
 * <b>before</b>.
 */
void
rep_history_clean(time_t before)
{
  int authority = authdir_mode(get_options());
  or_history_t *or_history;
  link_history_t *link_history;
  void *or_history_p, *link_history_p;
  digestmap_iter_t *orhist_it, *lhist_it;
  const char *d1, *d2;

  orhist_it = digestmap_iter_init(history_map);
  while (!digestmap_iter_done(orhist_it)) {
    int remove;
    digestmap_iter_get(orhist_it, &d1, &or_history_p);
    or_history = or_history_p;

    remove = authority ? (or_history->total_run_weights < STABILITY_EPSILON &&
                          !or_history->start_of_run)
                       : (or_history->changed < before);
    if (remove) {
      orhist_it = digestmap_iter_next_rmv(history_map, orhist_it);
      free_or_history(or_history);
      continue;
    }
    for (lhist_it = digestmap_iter_init(or_history->link_history_map);
         !digestmap_iter_done(lhist_it); ) {
      digestmap_iter_get(lhist_it, &d2, &link_history_p);
      link_history = link_history_p;
      if (link_history->changed < before) {
        lhist_it = digestmap_iter_next_rmv(or_history->link_history_map,
                                           lhist_it);
        rephist_total_alloc -= sizeof(link_history_t);
        tor_free(link_history);
        continue;
      }
      lhist_it = digestmap_iter_next(or_history->link_history_map,lhist_it);
    }
    orhist_it = digestmap_iter_next(history_map, orhist_it);
  }
}

/** Write MTBF data to disk. Return 0 on success, negative on failure.
 *
 * If <b>missing_means_down</b>, then if we're about to write an entry
 * that is still considered up but isn't in our routerlist, consider it
 * to be down. */
int
rep_hist_record_mtbf_data(time_t now, int missing_means_down)
{
  char time_buf[ISO_TIME_LEN+1];

  digestmap_iter_t *orhist_it;
  const char *digest;
  void *or_history_p;
  or_history_t *hist;
  open_file_t *open_file = NULL;
  FILE *f;

  {
    char *filename = get_datadir_fname("router-stability");
    f = start_writing_to_stdio_file(filename, OPEN_FLAGS_REPLACE|O_TEXT, 0600,
                                    &open_file);
    tor_free(filename);
    if (!f)
      return -1;
  }

  /* File format is:
   *   FormatLine *KeywordLine Data
   *
   *   FormatLine = "format 1" NL
   *   KeywordLine = Keyword SP Arguments NL
   *   Data = "data" NL *RouterMTBFLine "." NL
   *   RouterMTBFLine = Fingerprint SP WeightedRunLen SP
   *           TotalRunWeights [SP S=StartRunTime] NL
   */
#define PUT(s) STMT_BEGIN if (fputs((s),f)<0) goto err; STMT_END
#define PRINTF(args) STMT_BEGIN if (fprintf args <0) goto err; STMT_END

  PUT("format 2\n");

  format_iso_time(time_buf, time(NULL));
  PRINTF((f, "stored-at %s\n", time_buf));

  if (started_tracking_stability) {
    format_iso_time(time_buf, started_tracking_stability);
    PRINTF((f, "tracked-since %s\n", time_buf));
  }
  if (stability_last_downrated) {
    format_iso_time(time_buf, stability_last_downrated);
    PRINTF((f, "last-downrated %s\n", time_buf));
  }

  PUT("data\n");

  /* XXX Nick: now bridge auths record this for all routers too.
   * Should we make them record it only for bridge routers? -RD
   * Not for 0.2.0. -NM */
  for (orhist_it = digestmap_iter_init(history_map);
       !digestmap_iter_done(orhist_it);
       orhist_it = digestmap_iter_next(history_map,orhist_it)) {
    char dbuf[HEX_DIGEST_LEN+1];
    const char *t = NULL;
    digestmap_iter_get(orhist_it, &digest, &or_history_p);
    hist = (or_history_t*) or_history_p;

    base16_encode(dbuf, sizeof(dbuf), digest, DIGEST_LEN);

    if (missing_means_down && hist->start_of_run &&
        !router_get_by_id_digest(digest)) {
      /* We think this relay is running, but it's not listed in our
       * routerlist. Somehow it fell out without telling us it went
       * down. Complain and also correct it. */
      log_info(LD_HIST,
               "Relay '%s' is listed as up in rephist, but it's not in "
               "our routerlist. Correcting.", dbuf);
      rep_hist_note_router_unreachable(digest, now);
    }

    PRINTF((f, "R %s\n", dbuf));
    if (hist->start_of_run > 0) {
      format_iso_time(time_buf, hist->start_of_run);
      t = time_buf;
    }
    PRINTF((f, "+MTBF %lu %.5f%s%s\n",
            hist->weighted_run_length, hist->total_run_weights,
            t ? " S=" : "", t ? t : ""));
    t = NULL;
    if (hist->start_of_downtime > 0) {
      format_iso_time(time_buf, hist->start_of_downtime);
      t = time_buf;
    }
    PRINTF((f, "+WFU %lu %lu%s%s\n",
            hist->weighted_uptime, hist->total_weighted_time,
            t ? " S=" : "", t ? t : ""));
  }

  PUT(".\n");

#undef PUT
#undef PRINTF

  return finish_writing_to_file(open_file);
 err:
  abort_writing_to_file(open_file);
  return -1;
}

/** Helper: return the first j >= i such that !strcmpstart(sl[j], prefix) and
 * such that no line sl[k] with i <= k < j starts with "R ".  Return -1 if no
 * such line exists. */
static int
find_next_with(smartlist_t *sl, int i, const char *prefix)
{
  for ( ; i < smartlist_len(sl); ++i) {
    const char *line = smartlist_get(sl, i);
    if (!strcmpstart(line, prefix))
      return i;
    if (!strcmpstart(line, "R "))
      return -1;
  }
  return -1;
}

/** How many bad times has parse_possibly_bad_iso_time() parsed? */
static int n_bogus_times = 0;
/** Parse the ISO-formatted time in <b>s</b> into *<b>time_out</b>, but
 * round any pre-1970 date to Jan 1, 1970. */
static int
parse_possibly_bad_iso_time(const char *s, time_t *time_out)
{
  int year;
  char b[5];
  strlcpy(b, s, sizeof(b));
  b[4] = '\0';
  year = (int)tor_parse_long(b, 10, 0, INT_MAX, NULL, NULL);
  if (year < 1970) {
    *time_out = 0;
    ++n_bogus_times;
    return 0;
  } else
    return parse_iso_time(s, time_out);
}

/** We've read a time <b>t</b> from a file stored at <b>stored_at</b>, which
 * says we started measuring at <b>started_measuring</b>.  Return a new number
 * that's about as much before <b>now</b> as <b>t</b> was before
 * <b>stored_at</b>.
 */
static INLINE time_t
correct_time(time_t t, time_t now, time_t stored_at, time_t started_measuring)
{
  if (t < started_measuring - 24*60*60*365)
    return 0;
  else if (t < started_measuring)
    return started_measuring;
  else if (t > stored_at)
    return 0;
  else {
    long run_length = stored_at - t;
    t = (time_t)(now - run_length);
    if (t < started_measuring)
      t = started_measuring;
    return t;
  }
}

/** Load MTBF data from disk.  Returns 0 on success or recoverable error, -1
 * on failure. */
int
rep_hist_load_mtbf_data(time_t now)
{
  /* XXXX won't handle being called while history is already populated. */
  smartlist_t *lines;
  const char *line = NULL;
  int r=0, i;
  time_t last_downrated = 0, stored_at = 0, tracked_since = 0;
  time_t latest_possible_start = now;
  long format = -1;

  {
    char *filename = get_datadir_fname("router-stability");
    char *d = read_file_to_str(filename, RFTS_IGNORE_MISSING, NULL);
    tor_free(filename);
    if (!d)
      return -1;
    lines = smartlist_new();
    smartlist_split_string(lines, d, "\n", SPLIT_SKIP_SPACE, 0);
    tor_free(d);
  }

  {
    const char *firstline;
    if (smartlist_len(lines)>4) {
      firstline = smartlist_get(lines, 0);
      if (!strcmpstart(firstline, "format "))
        format = tor_parse_long(firstline+strlen("format "),
                                10, -1, LONG_MAX, NULL, NULL);
    }
  }
  if (format != 1 && format != 2) {
    log_warn(LD_HIST,
             "Unrecognized format in mtbf history file. Skipping.");
    goto err;
  }
  for (i = 1; i < smartlist_len(lines); ++i) {
    line = smartlist_get(lines, i);
    if (!strcmp(line, "data"))
      break;
    if (!strcmpstart(line, "last-downrated ")) {
      if (parse_iso_time(line+strlen("last-downrated "), &last_downrated)<0)
        log_warn(LD_HIST,"Couldn't parse downrate time in mtbf "
                 "history file.");
    }
    if (!strcmpstart(line, "stored-at ")) {
      if (parse_iso_time(line+strlen("stored-at "), &stored_at)<0)
        log_warn(LD_HIST,"Couldn't parse stored time in mtbf "
                 "history file.");
    }
    if (!strcmpstart(line, "tracked-since ")) {
      if (parse_iso_time(line+strlen("tracked-since "), &tracked_since)<0)
        log_warn(LD_HIST,"Couldn't parse started-tracking time in mtbf "
                 "history file.");
    }
  }
  if (last_downrated > now)
    last_downrated = now;
  if (tracked_since > now)
    tracked_since = now;

  if (!stored_at) {
    log_warn(LD_HIST, "No stored time recorded.");
    goto err;
  }

  if (line && !strcmp(line, "data"))
    ++i;

  n_bogus_times = 0;

  for (; i < smartlist_len(lines); ++i) {
    char digest[DIGEST_LEN];
    char hexbuf[HEX_DIGEST_LEN+1];
    char mtbf_timebuf[ISO_TIME_LEN+1];
    char wfu_timebuf[ISO_TIME_LEN+1];
    time_t start_of_run = 0;
    time_t start_of_downtime = 0;
    int have_mtbf = 0, have_wfu = 0;
    long wrl = 0;
    double trw = 0;
    long wt_uptime = 0, total_wt_time = 0;
    int n;
    or_history_t *hist;
    line = smartlist_get(lines, i);
    if (!strcmp(line, "."))
      break;

    mtbf_timebuf[0] = '\0';
    wfu_timebuf[0] = '\0';

    if (format == 1) {
      n = tor_sscanf(line, "%40s %ld %lf S=%10s %8s",
                 hexbuf, &wrl, &trw, mtbf_timebuf, mtbf_timebuf+11);
      if (n != 3 && n != 5) {
        log_warn(LD_HIST, "Couldn't scan line %s", escaped(line));
        continue;
      }
      have_mtbf = 1;
    } else {
      // format == 2.
      int mtbf_idx, wfu_idx;
      if (strcmpstart(line, "R ") || strlen(line) < 2+HEX_DIGEST_LEN)
        continue;
      strlcpy(hexbuf, line+2, sizeof(hexbuf));
      mtbf_idx = find_next_with(lines, i+1, "+MTBF ");
      wfu_idx = find_next_with(lines, i+1, "+WFU ");
      if (mtbf_idx >= 0) {
        const char *mtbfline = smartlist_get(lines, mtbf_idx);
        n = tor_sscanf(mtbfline, "+MTBF %lu %lf S=%10s %8s",
                   &wrl, &trw, mtbf_timebuf, mtbf_timebuf+11);
        if (n == 2 || n == 4) {
          have_mtbf = 1;
        } else {
          log_warn(LD_HIST, "Couldn't scan +MTBF line %s",
                   escaped(mtbfline));
        }
      }
      if (wfu_idx >= 0) {
        const char *wfuline = smartlist_get(lines, wfu_idx);
        n = tor_sscanf(wfuline, "+WFU %lu %lu S=%10s %8s",
                   &wt_uptime, &total_wt_time,
                   wfu_timebuf, wfu_timebuf+11);
        if (n == 2 || n == 4) {
          have_wfu = 1;
        } else {
          log_warn(LD_HIST, "Couldn't scan +WFU line %s", escaped(wfuline));
        }
      }
      if (wfu_idx > i)
        i = wfu_idx;
      if (mtbf_idx > i)
        i = mtbf_idx;
    }
    if (base16_decode(digest, DIGEST_LEN, hexbuf, HEX_DIGEST_LEN) < 0) {
      log_warn(LD_HIST, "Couldn't hex string %s", escaped(hexbuf));
      continue;
    }
    hist = get_or_history(digest);
    if (!hist)
      continue;

    if (have_mtbf) {
      if (mtbf_timebuf[0]) {
        mtbf_timebuf[10] = ' ';
        if (parse_possibly_bad_iso_time(mtbf_timebuf, &start_of_run)<0)
          log_warn(LD_HIST, "Couldn't parse time %s",
                   escaped(mtbf_timebuf));
      }
      hist->start_of_run = correct_time(start_of_run, now, stored_at,
                                        tracked_since);
      if (hist->start_of_run < latest_possible_start + wrl)
        latest_possible_start = (time_t)(hist->start_of_run - wrl);

      hist->weighted_run_length = wrl;
      hist->total_run_weights = trw;
    }
    if (have_wfu) {
      if (wfu_timebuf[0]) {
        wfu_timebuf[10] = ' ';
        if (parse_possibly_bad_iso_time(wfu_timebuf, &start_of_downtime)<0)
          log_warn(LD_HIST, "Couldn't parse time %s", escaped(wfu_timebuf));
      }
    }
    hist->start_of_downtime = correct_time(start_of_downtime, now, stored_at,
                                           tracked_since);
    hist->weighted_uptime = wt_uptime;
    hist->total_weighted_time = total_wt_time;
  }
  if (strcmp(line, "."))
    log_warn(LD_HIST, "Truncated MTBF file.");

  if (tracked_since < 86400*365) /* Recover from insanely early value. */
    tracked_since = latest_possible_start;

  stability_last_downrated = last_downrated;
  started_tracking_stability = tracked_since;

  goto done;
 err:
  r = -1;
 done:
  SMARTLIST_FOREACH(lines, char *, cp, tor_free(cp));
  smartlist_free(lines);
  return r;
}

/** For how many seconds do we keep track of individual per-second bandwidth
 * totals? */
#define NUM_SECS_ROLLING_MEASURE 10
/** How large are the intervals for which we track and report bandwidth use? */
#define NUM_SECS_BW_SUM_INTERVAL (4*60*60)
/** How far in the past do we remember and publish bandwidth use? */
#define NUM_SECS_BW_SUM_IS_VALID (24*60*60)
/** How many bandwidth usage intervals do we remember? (derived) */
#define NUM_TOTALS (NUM_SECS_BW_SUM_IS_VALID/NUM_SECS_BW_SUM_INTERVAL)

/** Structure to track bandwidth use, and remember the maxima for a given
 * time period.
 */
typedef struct bw_array_t {
  /** Observation array: Total number of bytes transferred in each of the last
   * NUM_SECS_ROLLING_MEASURE seconds. This is used as a circular array. */
  uint64_t obs[NUM_SECS_ROLLING_MEASURE];
  int cur_obs_idx; /**< Current position in obs. */
  time_t cur_obs_time; /**< Time represented in obs[cur_obs_idx] */
  uint64_t total_obs; /**< Total for all members of obs except
                       * obs[cur_obs_idx] */
  uint64_t max_total; /**< Largest value that total_obs has taken on in the
                       * current period. */
  uint64_t total_in_period; /**< Total bytes transferred in the current
                             * period. */

  /** When does the next period begin? */
  time_t next_period;
  /** Where in 'maxima' should the maximum bandwidth usage for the current
   * period be stored? */
  int next_max_idx;
  /** How many values in maxima/totals have been set ever? */
  int num_maxes_set;
  /** Circular array of the maximum
   * bandwidth-per-NUM_SECS_ROLLING_MEASURE usage for the last
   * NUM_TOTALS periods */
  uint64_t maxima[NUM_TOTALS];
  /** Circular array of the total bandwidth usage for the last NUM_TOTALS
   * periods */
  uint64_t totals[NUM_TOTALS];
} bw_array_t;

/** Shift the current period of b forward by one. */
static void
commit_max(bw_array_t *b)
{
  /* Store total from current period. */
  b->totals[b->next_max_idx] = b->total_in_period;
  /* Store maximum from current period. */
  b->maxima[b->next_max_idx++] = b->max_total;
  /* Advance next_period and next_max_idx */
  b->next_period += NUM_SECS_BW_SUM_INTERVAL;
  if (b->next_max_idx == NUM_TOTALS)
    b->next_max_idx = 0;
  if (b->num_maxes_set < NUM_TOTALS)
    ++b->num_maxes_set;
  /* Reset max_total. */
  b->max_total = 0;
  /* Reset total_in_period. */
  b->total_in_period = 0;
}

/** Shift the current observation time of <b>b</b> forward by one second. */
static INLINE void
advance_obs(bw_array_t *b)
{
  int nextidx;
  uint64_t total;

  /* Calculate the total bandwidth for the last NUM_SECS_ROLLING_MEASURE
   * seconds; adjust max_total as needed.*/
  total = b->total_obs + b->obs[b->cur_obs_idx];
  if (total > b->max_total)
    b->max_total = total;

  nextidx = b->cur_obs_idx+1;
  if (nextidx == NUM_SECS_ROLLING_MEASURE)
    nextidx = 0;

  b->total_obs = total - b->obs[nextidx];
  b->obs[nextidx]=0;
  b->cur_obs_idx = nextidx;

  if (++b->cur_obs_time >= b->next_period)
    commit_max(b);
}

/** Add <b>n</b> bytes to the number of bytes in <b>b</b> for second
 * <b>when</b>. */
static INLINE void
add_obs(bw_array_t *b, time_t when, uint64_t n)
{
  if (when < b->cur_obs_time)
    return; /* Don't record data in the past. */

  /* If we're currently adding observations for an earlier second than
   * 'when', advance b->cur_obs_time and b->cur_obs_idx by an
   * appropriate number of seconds, and do all the other housekeeping. */
  while (when > b->cur_obs_time) {
    /* Doing this one second at a time is potentially inefficient, if we start
       with a state file that is very old.  Fortunately, it doesn't seem to
       show up in profiles, so we can just ignore it for now. */
    advance_obs(b);
  }

  b->obs[b->cur_obs_idx] += n;
  b->total_in_period += n;
}

/** Allocate, initialize, and return a new bw_array. */
static bw_array_t *
bw_array_new(void)
{
  bw_array_t *b;
  time_t start;
  b = tor_malloc_zero(sizeof(bw_array_t));
  rephist_total_alloc += sizeof(bw_array_t);
  start = time(NULL);
  b->cur_obs_time = start;
  b->next_period = start + NUM_SECS_BW_SUM_INTERVAL;
  return b;
}

/** Recent history of bandwidth observations for read operations. */
static bw_array_t *read_array = NULL;
/** Recent history of bandwidth observations for write operations. */
static bw_array_t *write_array = NULL;
/** Recent history of bandwidth observations for read operations for the
    directory protocol. */
static bw_array_t *dir_read_array = NULL;
/** Recent history of bandwidth observations for write operations for the
    directory protocol. */
static bw_array_t *dir_write_array = NULL;

/** Set up [dir-]read_array and [dir-]write_array, freeing them if they
 * already exist. */
static void
bw_arrays_init(void)
{
  tor_free(read_array);
  tor_free(write_array);
  tor_free(dir_read_array);
  tor_free(dir_write_array);
  read_array = bw_array_new();
  write_array = bw_array_new();
  dir_read_array = bw_array_new();
  dir_write_array = bw_array_new();
}

/** Remember that we read <b>num_bytes</b> bytes in second <b>when</b>.
 *
 * Add num_bytes to the current running total for <b>when</b>.
 *
 * <b>when</b> can go back to time, but it's safe to ignore calls
 * earlier than the latest <b>when</b> you've heard of.
 */
void
rep_hist_note_bytes_written(size_t num_bytes, time_t when)
{
/* Maybe a circular array for recent seconds, and step to a new point
 * every time a new second shows up. Or simpler is to just to have
 * a normal array and push down each item every second; it's short.
 */
/* When a new second has rolled over, compute the sum of the bytes we've
 * seen over when-1 to when-1-NUM_SECS_ROLLING_MEASURE, and stick it
 * somewhere. See rep_hist_bandwidth_assess() below.
 */
  add_obs(write_array, when, num_bytes);
}

/** Remember that we wrote <b>num_bytes</b> bytes in second <b>when</b>.
 * (like rep_hist_note_bytes_written() above)
 */
void
rep_hist_note_bytes_read(size_t num_bytes, time_t when)
{
/* if we're smart, we can make this func and the one above share code */
  add_obs(read_array, when, num_bytes);
}

/** Remember that we wrote <b>num_bytes</b> directory bytes in second
 * <b>when</b>. (like rep_hist_note_bytes_written() above)
 */
void
rep_hist_note_dir_bytes_written(size_t num_bytes, time_t when)
{
  add_obs(dir_write_array, when, num_bytes);
}

/** Remember that we read <b>num_bytes</b> directory bytes in second
 * <b>when</b>. (like rep_hist_note_bytes_written() above)
 */
void
rep_hist_note_dir_bytes_read(size_t num_bytes, time_t when)
{
  add_obs(dir_read_array, when, num_bytes);
}

/** Helper: Return the largest value in b->maxima.  (This is equal to the
 * most bandwidth used in any NUM_SECS_ROLLING_MEASURE period for the last
 * NUM_SECS_BW_SUM_IS_VALID seconds.)
 */
static uint64_t
find_largest_max(bw_array_t *b)
{
  int i;
  uint64_t max;
  max=0;
  for (i=0; i<NUM_TOTALS; ++i) {
    if (b->maxima[i]>max)
      max = b->maxima[i];
  }
  return max;
}

/** Find the largest sums in the past NUM_SECS_BW_SUM_IS_VALID (roughly)
 * seconds. Find one sum for reading and one for writing. They don't have
 * to be at the same time.
 *
 * Return the smaller of these sums, divided by NUM_SECS_ROLLING_MEASURE.
 */
int
rep_hist_bandwidth_assess(void)
{
  uint64_t w,r;
  r = find_largest_max(read_array);
  w = find_largest_max(write_array);
  if (r>w)
    return (int)(U64_TO_DBL(w)/NUM_SECS_ROLLING_MEASURE);
  else
    return (int)(U64_TO_DBL(r)/NUM_SECS_ROLLING_MEASURE);
}

/** Print the bandwidth history of b (either [dir-]read_array or
 * [dir-]write_array) into the buffer pointed to by buf.  The format is
 * simply comma separated numbers, from oldest to newest.
 *
 * It returns the number of bytes written.
 */
static size_t
rep_hist_fill_bandwidth_history(char *buf, size_t len, const bw_array_t *b)
{
  char *cp = buf;
  int i, n;
  const or_options_t *options = get_options();
  uint64_t cutoff;

  if (b->num_maxes_set <= b->next_max_idx) {
    /* We haven't been through the circular array yet; time starts at i=0.*/
    i = 0;
  } else {
    /* We've been around the array at least once.  The next i to be
       overwritten is the oldest. */
    i = b->next_max_idx;
  }

  if (options->RelayBandwidthRate) {
    /* We don't want to report that we used more bandwidth than the max we're
     * willing to relay; otherwise everybody will know how much traffic
     * we used ourself. */
    cutoff = options->RelayBandwidthRate * NUM_SECS_BW_SUM_INTERVAL;
  } else {
    cutoff = UINT64_MAX;
  }

  for (n=0; n<b->num_maxes_set; ++n,++i) {
    uint64_t total;
    if (i >= NUM_TOTALS)
      i -= NUM_TOTALS;
    tor_assert(i < NUM_TOTALS);
    /* Round the bandwidth used down to the nearest 1k. */
    total = b->totals[i] & ~0x3ff;
    if (total > cutoff)
      total = cutoff;

    if (n==(b->num_maxes_set-1))
      tor_snprintf(cp, len-(cp-buf), U64_FORMAT, U64_PRINTF_ARG(total));
    else
      tor_snprintf(cp, len-(cp-buf), U64_FORMAT",", U64_PRINTF_ARG(total));
    cp += strlen(cp);
  }
  return cp-buf;
}

/** Allocate and return lines for representing this server's bandwidth
 * history in its descriptor. We publish these lines in our extra-info
 * descriptor.
 */
char *
rep_hist_get_bandwidth_lines(void)
{
  char *buf, *cp;
  char t[ISO_TIME_LEN+1];
  int r;
  bw_array_t *b = NULL;
  const char *desc = NULL;
  size_t len;

  /* [dirreq-](read|write)-history yyyy-mm-dd HH:MM:SS (n s) n,n,n... */
/* The n,n,n part above. Largest representation of a uint64_t is 20 chars
 * long, plus the comma. */
#define MAX_HIST_VALUE_LEN (21*NUM_TOTALS)
  len = (67+MAX_HIST_VALUE_LEN)*4;
  buf = tor_malloc_zero(len);
  cp = buf;
  for (r=0;r<4;++r) {
    char tmp[MAX_HIST_VALUE_LEN];
    size_t slen;
    switch (r) {
      case 0:
        b = write_array;
        desc = "write-history";
        break;
      case 1:
        b = read_array;
        desc = "read-history";
        break;
      case 2:
        b = dir_write_array;
        desc = "dirreq-write-history";
        break;
      case 3:
        b = dir_read_array;
        desc = "dirreq-read-history";
        break;
    }
    tor_assert(b);
    slen = rep_hist_fill_bandwidth_history(tmp, MAX_HIST_VALUE_LEN, b);
    /* If we don't have anything to write, skip to the next entry. */
    if (slen == 0)
      continue;
    format_iso_time(t, b->next_period-NUM_SECS_BW_SUM_INTERVAL);
    tor_snprintf(cp, len-(cp-buf), "%s %s (%d s) ",
                 desc, t, NUM_SECS_BW_SUM_INTERVAL);
    cp += strlen(cp);
    strlcat(cp, tmp, len-(cp-buf));
    cp += slen;
    strlcat(cp, "\n", len-(cp-buf));
    ++cp;
  }
  return buf;
}

/** Write a single bw_array_t into the Values, Ends, Interval, and Maximum
 * entries of an or_state_t. Done before writing out a new state file. */
static void
rep_hist_update_bwhist_state_section(or_state_t *state,
                                     const bw_array_t *b,
                                     smartlist_t **s_values,
                                     smartlist_t **s_maxima,
                                     time_t *s_begins,
                                     int *s_interval)
{
  int i,j;
  uint64_t maxval;

  if (*s_values) {
    SMARTLIST_FOREACH(*s_values, char *, val, tor_free(val));
    smartlist_free(*s_values);
  }
  if (*s_maxima) {
    SMARTLIST_FOREACH(*s_maxima, char *, val, tor_free(val));
    smartlist_free(*s_maxima);
  }
  if (! server_mode(get_options())) {
    /* Clients don't need to store bandwidth history persistently;
     * force these values to the defaults. */
    /* FFFF we should pull the default out of config.c's state table,
     * so we don't have two defaults. */
    if (*s_begins != 0 || *s_interval != 900) {
      time_t now = time(NULL);
      time_t save_at = get_options()->AvoidDiskWrites ? now+3600 : now+600;
      or_state_mark_dirty(state, save_at);
    }
    *s_begins = 0;
    *s_interval = 900;
    *s_values = smartlist_new();
    *s_maxima = smartlist_new();
    return;
  }
  *s_begins = b->next_period;
  *s_interval = NUM_SECS_BW_SUM_INTERVAL;

  *s_values = smartlist_new();
  *s_maxima = smartlist_new();
  /* Set i to first position in circular array */
  i = (b->num_maxes_set <= b->next_max_idx) ? 0 : b->next_max_idx;
  for (j=0; j < b->num_maxes_set; ++j,++i) {
    if (i >= NUM_TOTALS)
      i = 0;
    smartlist_add_asprintf(*s_values, U64_FORMAT,
                           U64_PRINTF_ARG(b->totals[i] & ~0x3ff));
    maxval = b->maxima[i] / NUM_SECS_ROLLING_MEASURE;
    smartlist_add_asprintf(*s_maxima, U64_FORMAT,
                           U64_PRINTF_ARG(maxval & ~0x3ff));
  }
  smartlist_add_asprintf(*s_values, U64_FORMAT,
                         U64_PRINTF_ARG(b->total_in_period & ~0x3ff));
  maxval = b->max_total / NUM_SECS_ROLLING_MEASURE;
  smartlist_add_asprintf(*s_maxima, U64_FORMAT,
                         U64_PRINTF_ARG(maxval & ~0x3ff));
}

/** Update <b>state</b> with the newest bandwidth history. Done before
 * writing out a new state file. */
void
rep_hist_update_state(or_state_t *state)
{
#define UPDATE(arrname,st) \
  rep_hist_update_bwhist_state_section(state,\
                                       (arrname),\
                                       &state->BWHistory ## st ## Values, \
                                       &state->BWHistory ## st ## Maxima, \
                                       &state->BWHistory ## st ## Ends, \
                                       &state->BWHistory ## st ## Interval)

  UPDATE(write_array, Write);
  UPDATE(read_array, Read);
  UPDATE(dir_write_array, DirWrite);
  UPDATE(dir_read_array, DirRead);

  if (server_mode(get_options())) {
    or_state_mark_dirty(state, time(NULL)+(2*3600));
  }
#undef UPDATE
}

/** Load a single bw_array_t from its Values, Ends, Maxima, and Interval
 * entries in an or_state_t. Done while reading the state file. */
static int
rep_hist_load_bwhist_state_section(bw_array_t *b,
                                   const smartlist_t *s_values,
                                   const smartlist_t *s_maxima,
                                   const time_t s_begins,
                                   const int s_interval)
{
  time_t now = time(NULL);
  int retval = 0;
  time_t start;

  uint64_t v, mv;
  int i,ok,ok_m = 0;
  int have_maxima = s_maxima && s_values &&
    (smartlist_len(s_values) == smartlist_len(s_maxima));

  if (s_values && s_begins >= now - NUM_SECS_BW_SUM_INTERVAL*NUM_TOTALS) {
    start = s_begins - s_interval*(smartlist_len(s_values));
    if (start > now)
      return 0;
    b->cur_obs_time = start;
    b->next_period = start + NUM_SECS_BW_SUM_INTERVAL;
    SMARTLIST_FOREACH_BEGIN(s_values, const char *, cp) {
        const char *maxstr = NULL;
        v = tor_parse_uint64(cp, 10, 0, UINT64_MAX, &ok, NULL);
        if (have_maxima) {
          maxstr = smartlist_get(s_maxima, cp_sl_idx);
          mv = tor_parse_uint64(maxstr, 10, 0, UINT64_MAX, &ok_m, NULL);
          mv *= NUM_SECS_ROLLING_MEASURE;
        } else {
          /* No maxima known; guess average rate to be conservative. */
          mv = (v / s_interval) * NUM_SECS_ROLLING_MEASURE;
        }
        if (!ok) {
          retval = -1;
          log_notice(LD_HIST, "Could not parse value '%s' into a number.'",cp);
        }
        if (maxstr && !ok_m) {
          retval = -1;
          log_notice(LD_HIST, "Could not parse maximum '%s' into a number.'",
                     maxstr);
        }

        if (start < now) {
          time_t cur_start = start;
          time_t actual_interval_len = s_interval;
          uint64_t cur_val = 0;
          /* Calculate the average per second. This is the best we can do
           * because our state file doesn't have per-second resolution. */
          if (start + s_interval > now)
            actual_interval_len = now - start;
          cur_val = v / actual_interval_len;
          /* This is potentially inefficient, but since we don't do it very
           * often it should be ok. */
          while (cur_start < start + actual_interval_len) {
            add_obs(b, cur_start, cur_val);
            ++cur_start;
          }
          b->max_total = mv;
          /* This will result in some fairly choppy history if s_interval
           * is not the same as NUM_SECS_BW_SUM_INTERVAL. XXXX */
          start += actual_interval_len;
        }
    } SMARTLIST_FOREACH_END(cp);
  }

  /* Clean up maxima and observed */
  for (i=0; i<NUM_SECS_ROLLING_MEASURE; ++i) {
    b->obs[i] = 0;
  }
  b->total_obs = 0;

  return retval;
}

/** Set bandwidth history from the state file we just loaded. */
int
rep_hist_load_state(or_state_t *state, char **err)
{
  int all_ok = 1;

  /* Assert they already have been malloced */
  tor_assert(read_array && write_array);
  tor_assert(dir_read_array && dir_write_array);

#define LOAD(arrname,st)                                                \
  if (rep_hist_load_bwhist_state_section(                               \
                                (arrname),                              \
                                state->BWHistory ## st ## Values,       \
                                state->BWHistory ## st ## Maxima,       \
                                state->BWHistory ## st ## Ends,         \
                                state->BWHistory ## st ## Interval)<0)  \
    all_ok = 0

  LOAD(write_array, Write);
  LOAD(read_array, Read);
  LOAD(dir_write_array, DirWrite);
  LOAD(dir_read_array, DirRead);

#undef LOAD
  if (!all_ok) {
    *err = tor_strdup("Parsing of bandwidth history values failed");
    /* and create fresh arrays */
    bw_arrays_init();
    return -1;
  }
  return 0;
}

/*********************************************************************/

/** A single predicted port: used to remember which ports we've made
 * connections to, so that we can try to keep making circuits that can handle
 * those ports. */
typedef struct predicted_port_t {
  /** The port we connected to */
  uint16_t port;
  /** The time at which we last used it */
  time_t time;
} predicted_port_t;

/** A list of port numbers that have been used recently. */
static smartlist_t *predicted_ports_list=NULL;

/** We just got an application request for a connection with
 * port <b>port</b>. Remember it for the future, so we can keep
 * some circuits open that will exit to this port.
 */
static void
add_predicted_port(time_t now, uint16_t port)
{
  predicted_port_t *pp = tor_malloc(sizeof(predicted_port_t));
  pp->port = port;
  pp->time = now;
  rephist_total_alloc += sizeof(*pp);
  smartlist_add(predicted_ports_list, pp);
}

/** Initialize whatever memory and structs are needed for predicting
 * which ports will be used. Also seed it with port 80, so we'll build
 * circuits on start-up.
 */
static void
predicted_ports_init(void)
{
  predicted_ports_list = smartlist_new();
  add_predicted_port(time(NULL), 80); /* add one to kickstart us */
}

/** Free whatever memory is needed for predicting which ports will
 * be used.
 */
static void
predicted_ports_free(void)
{
  rephist_total_alloc -=
    smartlist_len(predicted_ports_list)*sizeof(predicted_port_t);
  SMARTLIST_FOREACH(predicted_ports_list, predicted_port_t *,
                    pp, tor_free(pp));
  smartlist_free(predicted_ports_list);
}

/** Remember that <b>port</b> has been asked for as of time <b>now</b>.
 * This is used for predicting what sorts of streams we'll make in the
 * future and making exit circuits to anticipate that.
 */
void
rep_hist_note_used_port(time_t now, uint16_t port)
{
  tor_assert(predicted_ports_list);

  if (!port) /* record nothing */
    return;

  SMARTLIST_FOREACH_BEGIN(predicted_ports_list, predicted_port_t *, pp) {
    if (pp->port == port) {
      pp->time = now;
      return;
    }
  } SMARTLIST_FOREACH_END(pp);
  /* it's not there yet; we need to add it */
  add_predicted_port(now, port);
}

/** Return a newly allocated pointer to a list of uint16_t * for ports that
 * are likely to be asked for in the near future.
 */
smartlist_t *
rep_hist_get_predicted_ports(time_t now)
{
  int predicted_circs_relevance_time;
  smartlist_t *out = smartlist_new();
  tor_assert(predicted_ports_list);
  predicted_circs_relevance_time = get_options()->PredictedPortsRelevanceTime;

  /* clean out obsolete entries */
  SMARTLIST_FOREACH_BEGIN(predicted_ports_list, predicted_port_t *, pp) {
    if (pp->time + predicted_circs_relevance_time < now) {
      log_debug(LD_CIRC, "Expiring predicted port %d", pp->port);

      rephist_total_alloc -= sizeof(predicted_port_t);
      tor_free(pp);
      SMARTLIST_DEL_CURRENT(predicted_ports_list, pp);
    } else {
      smartlist_add(out, tor_memdup(&pp->port, sizeof(uint16_t)));
    }
  } SMARTLIST_FOREACH_END(pp);
  return out;
}

/**
 * Take a list of uint16_t *, and remove every port in the list from the
 * current list of predicted ports.
 */
void
rep_hist_remove_predicted_ports(const smartlist_t *rmv_ports)
{
  /* Let's do this on O(N), not O(N^2). */
  bitarray_t *remove_ports = bitarray_init_zero(UINT16_MAX);
  SMARTLIST_FOREACH(rmv_ports, const uint16_t *, p,
                    bitarray_set(remove_ports, *p));
  SMARTLIST_FOREACH_BEGIN(predicted_ports_list, predicted_port_t *, pp) {
    if (bitarray_is_set(remove_ports, pp->port)) {
      tor_free(pp);
      SMARTLIST_DEL_CURRENT(predicted_ports_list, pp);
    }
  } SMARTLIST_FOREACH_END(pp);
  bitarray_free(remove_ports);
}

/** The user asked us to do a resolve. Rather than keeping track of
 * timings and such of resolves, we fake it for now by treating
 * it the same way as a connection to port 80. This way we will continue
 * to have circuits lying around if the user only uses Tor for resolves.
 */
void
rep_hist_note_used_resolve(time_t now)
{
  rep_hist_note_used_port(now, 80);
}

/** The last time at which we needed an internal circ. */
static time_t predicted_internal_time = 0;
/** The last time we needed an internal circ with good uptime. */
static time_t predicted_internal_uptime_time = 0;
/** The last time we needed an internal circ with good capacity. */
static time_t predicted_internal_capacity_time = 0;

/** Remember that we used an internal circ at time <b>now</b>. */
void
rep_hist_note_used_internal(time_t now, int need_uptime, int need_capacity)
{
  predicted_internal_time = now;
  if (need_uptime)
    predicted_internal_uptime_time = now;
  if (need_capacity)
    predicted_internal_capacity_time = now;
}

/** Return 1 if we've used an internal circ recently; else return 0. */
int
rep_hist_get_predicted_internal(time_t now, int *need_uptime,
                                int *need_capacity)
{
  int predicted_circs_relevance_time;
  predicted_circs_relevance_time = get_options()->PredictedPortsRelevanceTime;

  if (!predicted_internal_time) { /* initialize it */
    predicted_internal_time = now;
    predicted_internal_uptime_time = now;
    predicted_internal_capacity_time = now;
  }
  if (predicted_internal_time + predicted_circs_relevance_time < now)
    return 0; /* too long ago */
  if (predicted_internal_uptime_time + predicted_circs_relevance_time >= now)
    *need_uptime = 1;
  // Always predict that we need capacity.
  *need_capacity = 1;
  return 1;
}

/** Any ports used lately? These are pre-seeded if we just started
 * up or if we're running a hidden service. */
int
any_predicted_circuits(time_t now)
{
  int predicted_circs_relevance_time;
  predicted_circs_relevance_time = get_options()->PredictedPortsRelevanceTime;

  return smartlist_len(predicted_ports_list) ||
         predicted_internal_time + predicted_circs_relevance_time >= now;
}

/** Return 1 if we have no need for circuits currently, else return 0. */
int
rep_hist_circbuilding_dormant(time_t now)
{
  if (any_predicted_circuits(now))
    return 0;

  /* see if we'll still need to build testing circuits */
  if (server_mode(get_options()) &&
      (!check_whether_orport_reachable() || !circuit_enough_testing_circs()))
    return 0;
  if (!check_whether_dirport_reachable())
    return 0;

  return 1;
}

/** Structure to track how many times we've done each public key operation. */
static struct {
  /** How many directory objects have we signed? */
  unsigned long n_signed_dir_objs;
  /** How many routerdescs have we signed? */
  unsigned long n_signed_routerdescs;
  /** How many directory objects have we verified? */
  unsigned long n_verified_dir_objs;
  /** How many routerdescs have we verified */
  unsigned long n_verified_routerdescs;
  /** How many onionskins have we encrypted to build circuits? */
  unsigned long n_onionskins_encrypted;
  /** How many onionskins have we decrypted to do circuit build requests? */
  unsigned long n_onionskins_decrypted;
  /** How many times have we done the TLS handshake as a client? */
  unsigned long n_tls_client_handshakes;
  /** How many times have we done the TLS handshake as a server? */
  unsigned long n_tls_server_handshakes;
  /** How many PK operations have we done as a hidden service client? */
  unsigned long n_rend_client_ops;
  /** How many PK operations have we done as a hidden service midpoint? */
  unsigned long n_rend_mid_ops;
  /** How many PK operations have we done as a hidden service provider? */
  unsigned long n_rend_server_ops;
} pk_op_counts = {0,0,0,0,0,0,0,0,0,0,0};

/** Increment the count of the number of times we've done <b>operation</b>. */
void
note_crypto_pk_op(pk_op_t operation)
{
  switch (operation)
    {
    case SIGN_DIR:
      pk_op_counts.n_signed_dir_objs++;
      break;
    case SIGN_RTR:
      pk_op_counts.n_signed_routerdescs++;
      break;
    case VERIFY_DIR:
      pk_op_counts.n_verified_dir_objs++;
      break;
    case VERIFY_RTR:
      pk_op_counts.n_verified_routerdescs++;
      break;
    case ENC_ONIONSKIN:
      pk_op_counts.n_onionskins_encrypted++;
      break;
    case DEC_ONIONSKIN:
      pk_op_counts.n_onionskins_decrypted++;
      break;
    case TLS_HANDSHAKE_C:
      pk_op_counts.n_tls_client_handshakes++;
      break;
    case TLS_HANDSHAKE_S:
      pk_op_counts.n_tls_server_handshakes++;
      break;
    case REND_CLIENT:
      pk_op_counts.n_rend_client_ops++;
      break;
    case REND_MID:
      pk_op_counts.n_rend_mid_ops++;
      break;
    case REND_SERVER:
      pk_op_counts.n_rend_server_ops++;
      break;
    default:
      log_warn(LD_BUG, "Unknown pk operation %d", operation);
  }
}

/** Log the number of times we've done each public/private-key operation. */
void
dump_pk_ops(int severity)
{
  tor_log(severity, LD_HIST,
      "PK operations: %lu directory objects signed, "
      "%lu directory objects verified, "
      "%lu routerdescs signed, "
      "%lu routerdescs verified, "
      "%lu onionskins encrypted, "
      "%lu onionskins decrypted, "
      "%lu client-side TLS handshakes, "
      "%lu server-side TLS handshakes, "
      "%lu rendezvous client operations, "
      "%lu rendezvous middle operations, "
      "%lu rendezvous server operations.",
      pk_op_counts.n_signed_dir_objs,
      pk_op_counts.n_verified_dir_objs,
      pk_op_counts.n_signed_routerdescs,
      pk_op_counts.n_verified_routerdescs,
      pk_op_counts.n_onionskins_encrypted,
      pk_op_counts.n_onionskins_decrypted,
      pk_op_counts.n_tls_client_handshakes,
      pk_op_counts.n_tls_server_handshakes,
      pk_op_counts.n_rend_client_ops,
      pk_op_counts.n_rend_mid_ops,
      pk_op_counts.n_rend_server_ops);
}

/*** Exit port statistics ***/

/* Some constants */
/** To what multiple should byte numbers be rounded up? */
#define EXIT_STATS_ROUND_UP_BYTES 1024
/** To what multiple should stream counts be rounded up? */
#define EXIT_STATS_ROUND_UP_STREAMS 4
/** Number of TCP ports */
#define EXIT_STATS_NUM_PORTS 65536
/** Top n ports that will be included in exit stats. */
#define EXIT_STATS_TOP_N_PORTS 10

/* The following data structures are arrays and no fancy smartlists or maps,
 * so that all write operations can be done in constant time. This comes at
 * the price of some memory (1.25 MB) and linear complexity when writing
 * stats for measuring relays. */
/** Number of bytes read in current period by exit port */
static uint64_t *exit_bytes_read = NULL;
/** Number of bytes written in current period by exit port */
static uint64_t *exit_bytes_written = NULL;
/** Number of streams opened in current period by exit port */
static uint32_t *exit_streams = NULL;

/** Start time of exit stats or 0 if we're not collecting exit stats. */
static time_t start_of_exit_stats_interval;

/** Initialize exit port stats. */
void
rep_hist_exit_stats_init(time_t now)
{
  start_of_exit_stats_interval = now;
  exit_bytes_read = tor_malloc_zero(EXIT_STATS_NUM_PORTS *
                                    sizeof(uint64_t));
  exit_bytes_written = tor_malloc_zero(EXIT_STATS_NUM_PORTS *
                                       sizeof(uint64_t));
  exit_streams = tor_malloc_zero(EXIT_STATS_NUM_PORTS *
                                 sizeof(uint32_t));
}

/** Reset counters for exit port statistics. */
void
rep_hist_reset_exit_stats(time_t now)
{
  start_of_exit_stats_interval = now;
  memset(exit_bytes_read, 0, EXIT_STATS_NUM_PORTS * sizeof(uint64_t));
  memset(exit_bytes_written, 0, EXIT_STATS_NUM_PORTS * sizeof(uint64_t));
  memset(exit_streams, 0, EXIT_STATS_NUM_PORTS * sizeof(uint32_t));
}

/** Stop collecting exit port stats in a way that we can re-start doing
 * so in rep_hist_exit_stats_init(). */
void
rep_hist_exit_stats_term(void)
{
  start_of_exit_stats_interval = 0;
  tor_free(exit_bytes_read);
  tor_free(exit_bytes_written);
  tor_free(exit_streams);
}

/** Helper for qsort: compare two ints.  Does not handle overflow properly,
 * but works fine for sorting an array of port numbers, which is what we use
 * it for. */
static int
compare_int_(const void *x, const void *y)
{
  return (*(int*)x - *(int*)y);
}

/** Return a newly allocated string containing the exit port statistics
 * until <b>now</b>, or NULL if we're not collecting exit stats. Caller
 * must ensure start_of_exit_stats_interval is in the past. */
char *
rep_hist_format_exit_stats(time_t now)
{
  int i, j, top_elements = 0, cur_min_idx = 0, cur_port;
  uint64_t top_bytes[EXIT_STATS_TOP_N_PORTS];
  int top_ports[EXIT_STATS_TOP_N_PORTS];
  uint64_t cur_bytes = 0, other_read = 0, other_written = 0,
           total_read = 0, total_written = 0;
  uint32_t total_streams = 0, other_streams = 0;
  smartlist_t *written_strings, *read_strings, *streams_strings;
  char *written_string, *read_string, *streams_string;
  char t[ISO_TIME_LEN+1];
  char *result;

  if (!start_of_exit_stats_interval)
    return NULL; /* Not initialized. */

  tor_assert(now >= start_of_exit_stats_interval);

  /* Go through all ports to find the n ports that saw most written and
   * read bytes.
   *
   * Invariant: at the end of the loop for iteration i,
   *    total_read is the sum of all exit_bytes_read[0..i]
   *    total_written is the sum of all exit_bytes_written[0..i]
   *    total_stream is the sum of all exit_streams[0..i]
   *
   *    top_elements = MAX(EXIT_STATS_TOP_N_PORTS,
   *                  #{j | 0 <= j <= i && volume(i) > 0})
   *
   *    For all 0 <= j < top_elements,
   *        top_bytes[j] > 0
   *        0 <= top_ports[j] <= 65535
   *        top_bytes[j] = volume(top_ports[j])
   *
   *    There is no j in 0..i and k in 0..top_elements such that:
   *        volume(j) > top_bytes[k] AND j is not in top_ports[0..top_elements]
   *
   *    There is no j!=cur_min_idx in 0..top_elements such that:
   *        top_bytes[j] < top_bytes[cur_min_idx]
   *
   * where volume(x) == exit_bytes_read[x]+exit_bytes_written[x]
   *
   * Worst case: O(EXIT_STATS_NUM_PORTS * EXIT_STATS_TOP_N_PORTS)
   */
  for (i = 1; i < EXIT_STATS_NUM_PORTS; i++) {
    total_read += exit_bytes_read[i];
    total_written += exit_bytes_written[i];
    total_streams += exit_streams[i];
    cur_bytes = exit_bytes_read[i] + exit_bytes_written[i];
    if (cur_bytes == 0) {
      continue;
    }
    if (top_elements < EXIT_STATS_TOP_N_PORTS) {
      top_bytes[top_elements] = cur_bytes;
      top_ports[top_elements++] = i;
    } else if (cur_bytes > top_bytes[cur_min_idx]) {
      top_bytes[cur_min_idx] = cur_bytes;
      top_ports[cur_min_idx] = i;
    } else {
      continue;
    }
    cur_min_idx = 0;
    for (j = 1; j < top_elements; j++) {
      if (top_bytes[j] < top_bytes[cur_min_idx]) {
        cur_min_idx = j;
      }
    }
  }

  /* Add observations of top ports to smartlists. */
  written_strings = smartlist_new();
  read_strings = smartlist_new();
  streams_strings = smartlist_new();
  other_read = total_read;
  other_written = total_written;
  other_streams = total_streams;
  /* Sort the ports; this puts them out of sync with top_bytes, but we
   * won't be using top_bytes again anyway */
  qsort(top_ports, top_elements, sizeof(int), compare_int_);
  for (j = 0; j < top_elements; j++) {
    cur_port = top_ports[j];
    if (exit_bytes_written[cur_port] > 0) {
      uint64_t num = round_uint64_to_next_multiple_of(
                     exit_bytes_written[cur_port],
                     EXIT_STATS_ROUND_UP_BYTES);
      num /= 1024;
      smartlist_add_asprintf(written_strings, "%d="U64_FORMAT,
                             cur_port, U64_PRINTF_ARG(num));
      other_written -= exit_bytes_written[cur_port];
    }
    if (exit_bytes_read[cur_port] > 0) {
      uint64_t num = round_uint64_to_next_multiple_of(
                     exit_bytes_read[cur_port],
                     EXIT_STATS_ROUND_UP_BYTES);
      num /= 1024;
      smartlist_add_asprintf(read_strings, "%d="U64_FORMAT,
                             cur_port, U64_PRINTF_ARG(num));
      other_read -= exit_bytes_read[cur_port];
    }
    if (exit_streams[cur_port] > 0) {
      uint32_t num = round_uint32_to_next_multiple_of(
                     exit_streams[cur_port],
                     EXIT_STATS_ROUND_UP_STREAMS);
      smartlist_add_asprintf(streams_strings, "%d=%u", cur_port, num);
      other_streams -= exit_streams[cur_port];
    }
  }

  /* Add observations of other ports in a single element. */
  other_written = round_uint64_to_next_multiple_of(other_written,
                  EXIT_STATS_ROUND_UP_BYTES);
  other_written /= 1024;
  smartlist_add_asprintf(written_strings, "other="U64_FORMAT,
                         U64_PRINTF_ARG(other_written));
  other_read = round_uint64_to_next_multiple_of(other_read,
               EXIT_STATS_ROUND_UP_BYTES);
  other_read /= 1024;
  smartlist_add_asprintf(read_strings, "other="U64_FORMAT,
                         U64_PRINTF_ARG(other_read));
  other_streams = round_uint32_to_next_multiple_of(other_streams,
                  EXIT_STATS_ROUND_UP_STREAMS);
  smartlist_add_asprintf(streams_strings, "other=%u", other_streams);

  /* Join all observations in single strings. */
  written_string = smartlist_join_strings(written_strings, ",", 0, NULL);
  read_string = smartlist_join_strings(read_strings, ",", 0, NULL);
  streams_string = smartlist_join_strings(streams_strings, ",", 0, NULL);
  SMARTLIST_FOREACH(written_strings, char *, cp, tor_free(cp));
  SMARTLIST_FOREACH(read_strings, char *, cp, tor_free(cp));
  SMARTLIST_FOREACH(streams_strings, char *, cp, tor_free(cp));
  smartlist_free(written_strings);
  smartlist_free(read_strings);
  smartlist_free(streams_strings);

  /* Put everything together. */
  format_iso_time(t, now);
  tor_asprintf(&result, "exit-stats-end %s (%d s)\n"
               "exit-kibibytes-written %s\n"
               "exit-kibibytes-read %s\n"
               "exit-streams-opened %s\n",
               t, (unsigned) (now - start_of_exit_stats_interval),
               written_string,
               read_string,
               streams_string);
  tor_free(written_string);
  tor_free(read_string);
  tor_free(streams_string);
  return result;
}

/** If 24 hours have passed since the beginning of the current exit port
 * stats period, write exit stats to $DATADIR/stats/exit-stats (possibly
 * overwriting an existing file) and reset counters.  Return when we would
 * next want to write exit stats or 0 if we never want to write. */
time_t
rep_hist_exit_stats_write(time_t now)
{
  char *str = NULL;

  if (!start_of_exit_stats_interval)
    return 0; /* Not initialized. */
  if (start_of_exit_stats_interval + WRITE_STATS_INTERVAL > now)
    goto done; /* Not ready to write. */

  log_info(LD_HIST, "Writing exit port statistics to disk.");

  /* Generate history string. */
  str = rep_hist_format_exit_stats(now);

  /* Reset counters. */
  rep_hist_reset_exit_stats(now);

  /* Try to write to disk. */
  if (!check_or_create_data_subdir("stats")) {
    write_to_data_subdir("stats", "exit-stats", str, "exit port statistics");
  }

 done:
  tor_free(str);
  return start_of_exit_stats_interval + WRITE_STATS_INTERVAL;
}

/** Note that we wrote <b>num_written</b> bytes and read <b>num_read</b>
 * bytes to/from an exit connection to <b>port</b>. */
void
rep_hist_note_exit_bytes(uint16_t port, size_t num_written,
                         size_t num_read)
{
  if (!start_of_exit_stats_interval)
    return; /* Not initialized. */
  exit_bytes_written[port] += num_written;
  exit_bytes_read[port] += num_read;
  log_debug(LD_HIST, "Written %lu bytes and read %lu bytes to/from an "
            "exit connection to port %d.",
            (unsigned long)num_written, (unsigned long)num_read, port);
}

/** Note that we opened an exit stream to <b>port</b>. */
void
rep_hist_note_exit_stream_opened(uint16_t port)
{
  if (!start_of_exit_stats_interval)
    return; /* Not initialized. */
  exit_streams[port]++;
  log_debug(LD_HIST, "Opened exit stream to port %d", port);
}

/*** cell statistics ***/

/** Start of the current buffer stats interval or 0 if we're not
 * collecting buffer statistics. */
static time_t start_of_buffer_stats_interval;

/** Initialize buffer stats. */
void
rep_hist_buffer_stats_init(time_t now)
{
  start_of_buffer_stats_interval = now;
}

/** Statistics from a single circuit.  Collected when the circuit closes, or
 * when we flush statistics to disk. */
typedef struct circ_buffer_stats_t {
  /** Average number of cells in the circuit's queue */
  double mean_num_cells_in_queue;
  /** Average time a cell waits in the queue. */
  double mean_time_cells_in_queue;
  /** Total number of cells sent over this circuit */
  uint32_t processed_cells;
} circ_buffer_stats_t;

/** List of circ_buffer_stats_t. */
static smartlist_t *circuits_for_buffer_stats = NULL;

/** Remember cell statistics <b>mean_num_cells_in_queue</b>,
 * <b>mean_time_cells_in_queue</b>, and <b>processed_cells</b> of a
 * circuit. */
void
rep_hist_add_buffer_stats(double mean_num_cells_in_queue,
    double mean_time_cells_in_queue, uint32_t processed_cells)
{
  circ_buffer_stats_t *stat;
  if (!start_of_buffer_stats_interval)
    return; /* Not initialized. */
  stat = tor_malloc_zero(sizeof(circ_buffer_stats_t));
  stat->mean_num_cells_in_queue = mean_num_cells_in_queue;
  stat->mean_time_cells_in_queue = mean_time_cells_in_queue;
  stat->processed_cells = processed_cells;
  if (!circuits_for_buffer_stats)
    circuits_for_buffer_stats = smartlist_new();
  smartlist_add(circuits_for_buffer_stats, stat);
}

/** Remember cell statistics for circuit <b>circ</b> at time
 * <b>end_of_interval</b> and reset cell counters in case the circuit
 * remains open in the next measurement interval. */
void
rep_hist_buffer_stats_add_circ(circuit_t *circ, time_t end_of_interval)
{
  time_t start_of_interval;
  int interval_length;
  or_circuit_t *orcirc;
  double mean_num_cells_in_queue, mean_time_cells_in_queue;
  uint32_t processed_cells;
  if (CIRCUIT_IS_ORIGIN(circ))
    return;
  orcirc = TO_OR_CIRCUIT(circ);
  if (!orcirc->processed_cells)
    return;
  start_of_interval = (circ->timestamp_created.tv_sec >
                       start_of_buffer_stats_interval) ?
        (time_t)circ->timestamp_created.tv_sec :
        start_of_buffer_stats_interval;
  interval_length = (int) (end_of_interval - start_of_interval);
  if (interval_length <= 0)
    return;
  processed_cells = orcirc->processed_cells;
  /* 1000.0 for s -> ms; 2.0 because of app-ward and exit-ward queues */
  mean_num_cells_in_queue = (double) orcirc->total_cell_waiting_time /
      (double) interval_length / 1000.0 / 2.0;
  mean_time_cells_in_queue =
      (double) orcirc->total_cell_waiting_time /
      (double) orcirc->processed_cells;
  orcirc->total_cell_waiting_time = 0;
  orcirc->processed_cells = 0;
  rep_hist_add_buffer_stats(mean_num_cells_in_queue,
                            mean_time_cells_in_queue,
                            processed_cells);
}

/** Sorting helper: return -1, 1, or 0 based on comparison of two
 * circ_buffer_stats_t */
static int
buffer_stats_compare_entries_(const void **_a, const void **_b)
{
  const circ_buffer_stats_t *a = *_a, *b = *_b;
  if (a->processed_cells < b->processed_cells)
    return 1;
  else if (a->processed_cells > b->processed_cells)
    return -1;
  else
    return 0;
}

/** Stop collecting cell stats in a way that we can re-start doing so in
 * rep_hist_buffer_stats_init(). */
void
rep_hist_buffer_stats_term(void)
{
  rep_hist_reset_buffer_stats(0);
}

/** Clear history of circuit statistics and set the measurement interval
 * start to <b>now</b>. */
void
rep_hist_reset_buffer_stats(time_t now)
{
  if (!circuits_for_buffer_stats)
    circuits_for_buffer_stats = smartlist_new();
  SMARTLIST_FOREACH(circuits_for_buffer_stats, circ_buffer_stats_t *,
      stat, tor_free(stat));
  smartlist_clear(circuits_for_buffer_stats);
  start_of_buffer_stats_interval = now;
}

/** Return a newly allocated string containing the buffer statistics until
 * <b>now</b>, or NULL if we're not collecting buffer stats. Caller must
 * ensure start_of_buffer_stats_interval is in the past. */
char *
rep_hist_format_buffer_stats(time_t now)
{
#define SHARES 10
  uint64_t processed_cells[SHARES];
  uint32_t circs_in_share[SHARES];
  int number_of_circuits, i;
  double queued_cells[SHARES], time_in_queue[SHARES];
  smartlist_t *processed_cells_strings, *queued_cells_strings,
              *time_in_queue_strings;
  char *processed_cells_string, *queued_cells_string,
       *time_in_queue_string;
  char t[ISO_TIME_LEN+1];
  char *result;

  if (!start_of_buffer_stats_interval)
    return NULL; /* Not initialized. */

  tor_assert(now >= start_of_buffer_stats_interval);

  /* Calculate deciles if we saw at least one circuit. */
  memset(processed_cells, 0, SHARES * sizeof(uint64_t));
  memset(circs_in_share, 0, SHARES * sizeof(uint32_t));
  memset(queued_cells, 0, SHARES * sizeof(double));
  memset(time_in_queue, 0, SHARES * sizeof(double));
  if (!circuits_for_buffer_stats)
    circuits_for_buffer_stats = smartlist_new();
  number_of_circuits = smartlist_len(circuits_for_buffer_stats);
  if (number_of_circuits > 0) {
    smartlist_sort(circuits_for_buffer_stats,
                   buffer_stats_compare_entries_);
    i = 0;
    SMARTLIST_FOREACH_BEGIN(circuits_for_buffer_stats,
                            circ_buffer_stats_t *, stat)
    {
      int share = i++ * SHARES / number_of_circuits;
      processed_cells[share] += stat->processed_cells;
      queued_cells[share] += stat->mean_num_cells_in_queue;
      time_in_queue[share] += stat->mean_time_cells_in_queue;
      circs_in_share[share]++;
    }
    SMARTLIST_FOREACH_END(stat);
  }

  /* Write deciles to strings. */
  processed_cells_strings = smartlist_new();
  queued_cells_strings = smartlist_new();
  time_in_queue_strings = smartlist_new();
  for (i = 0; i < SHARES; i++) {
    smartlist_add_asprintf(processed_cells_strings,
                           U64_FORMAT, !circs_in_share[i] ? 0 :
                           U64_PRINTF_ARG(processed_cells[i] /
                           circs_in_share[i]));
  }
  for (i = 0; i < SHARES; i++) {
    smartlist_add_asprintf(queued_cells_strings, "%.2f",
                           circs_in_share[i] == 0 ? 0.0 :
                             queued_cells[i] / (double) circs_in_share[i]);
  }
  for (i = 0; i < SHARES; i++) {
    smartlist_add_asprintf(time_in_queue_strings, "%.0f",
                           circs_in_share[i] == 0 ? 0.0 :
                             time_in_queue[i] / (double) circs_in_share[i]);
  }

  /* Join all observations in single strings. */
  processed_cells_string = smartlist_join_strings(processed_cells_strings,
                                                  ",", 0, NULL);
  queued_cells_string = smartlist_join_strings(queued_cells_strings,
                                               ",", 0, NULL);
  time_in_queue_string = smartlist_join_strings(time_in_queue_strings,
                                                ",", 0, NULL);
  SMARTLIST_FOREACH(processed_cells_strings, char *, cp, tor_free(cp));
  SMARTLIST_FOREACH(queued_cells_strings, char *, cp, tor_free(cp));
  SMARTLIST_FOREACH(time_in_queue_strings, char *, cp, tor_free(cp));
  smartlist_free(processed_cells_strings);
  smartlist_free(queued_cells_strings);
  smartlist_free(time_in_queue_strings);

  /* Put everything together. */
  format_iso_time(t, now);
  tor_asprintf(&result, "cell-stats-end %s (%d s)\n"
               "cell-processed-cells %s\n"
               "cell-queued-cells %s\n"
               "cell-time-in-queue %s\n"
               "cell-circuits-per-decile %d\n",
               t, (unsigned) (now - start_of_buffer_stats_interval),
               processed_cells_string,
               queued_cells_string,
               time_in_queue_string,
               (number_of_circuits + SHARES - 1) / SHARES);
  tor_free(processed_cells_string);
  tor_free(queued_cells_string);
  tor_free(time_in_queue_string);
  return result;
#undef SHARES
}

/** If 24 hours have passed since the beginning of the current buffer
 * stats period, write buffer stats to $DATADIR/stats/buffer-stats
 * (possibly overwriting an existing file) and reset counters.  Return
 * when we would next want to write buffer stats or 0 if we never want to
 * write. */
time_t
rep_hist_buffer_stats_write(time_t now)
{
  circuit_t *circ;
  char *str = NULL;

  if (!start_of_buffer_stats_interval)
    return 0; /* Not initialized. */
  if (start_of_buffer_stats_interval + WRITE_STATS_INTERVAL > now)
    goto done; /* Not ready to write */

  /* Add open circuits to the history. */
  TOR_LIST_FOREACH(circ, circuit_get_global_list(), head) {
    rep_hist_buffer_stats_add_circ(circ, now);
  }

  /* Generate history string. */
  str = rep_hist_format_buffer_stats(now);

  /* Reset both buffer history and counters of open circuits. */
  rep_hist_reset_buffer_stats(now);

  /* Try to write to disk. */
  if (!check_or_create_data_subdir("stats")) {
    write_to_data_subdir("stats", "buffer-stats", str, "buffer statistics");
  }

 done:
  tor_free(str);
  return start_of_buffer_stats_interval + WRITE_STATS_INTERVAL;
}

/*** Descriptor serving statistics ***/

/** Digestmap to track which descriptors were downloaded this stats
 *  collection interval. It maps descriptor digest to pointers to 1,
 *  effectively turning this into a list. */
static digestmap_t *served_descs = NULL;

/** Number of how many descriptors were downloaded in total during this
 * interval. */
static unsigned long total_descriptor_downloads;

/** Start time of served descs stats or 0 if we're not collecting those. */
static time_t start_of_served_descs_stats_interval;

/** Initialize descriptor stats. */
void
rep_hist_desc_stats_init(time_t now)
{
  if (served_descs) {
    log_warn(LD_BUG, "Called rep_hist_desc_stats_init() when desc stats were "
             "already initialized. This is probably harmless.");
    return; // Already initialized
  }
  served_descs = digestmap_new();
  total_descriptor_downloads = 0;
  start_of_served_descs_stats_interval = now;
}

/** Reset served descs stats to empty, starting a new interval <b>now</b>. */
static void
rep_hist_reset_desc_stats(time_t now)
{
  rep_hist_desc_stats_term();
  rep_hist_desc_stats_init(now);
}

/** Stop collecting served descs stats, so that rep_hist_desc_stats_init() is
 * safe to be called again. */
void
rep_hist_desc_stats_term(void)
{
  digestmap_free(served_descs, NULL);
  served_descs = NULL;
  start_of_served_descs_stats_interval = 0;
  total_descriptor_downloads = 0;
}

/** Helper for rep_hist_desc_stats_write(). Return a newly allocated string
 * containing the served desc statistics until now, or NULL if we're not
 * collecting served desc stats. Caller must ensure that now is not before
 * start_of_served_descs_stats_interval. */
static char *
rep_hist_format_desc_stats(time_t now)
{
  char t[ISO_TIME_LEN+1];
  char *result;

  digestmap_iter_t *iter;
  const char *key;
  void *val;
  unsigned size;
  int *vals, max = 0, q3 = 0, md = 0, q1 = 0, min = 0;
  int n = 0;

  if (!start_of_served_descs_stats_interval)
    return NULL;

  size = digestmap_size(served_descs);
  if (size > 0) {
    vals = tor_malloc(size * sizeof(int));
    for (iter = digestmap_iter_init(served_descs);
         !digestmap_iter_done(iter);
         iter = digestmap_iter_next(served_descs, iter)) {
      uintptr_t count;
      digestmap_iter_get(iter, &key, &val);
      count = (uintptr_t)val;
      vals[n++] = (int)count;
      (void)key;
    }
    max = find_nth_int(vals, size, size-1);
    q3 = find_nth_int(vals, size, (3*size-1)/4);
    md = find_nth_int(vals, size, (size-1)/2);
    q1 = find_nth_int(vals, size, (size-1)/4);
    min = find_nth_int(vals, size, 0);
    tor_free(vals);
  }

  format_iso_time(t, now);

  tor_asprintf(&result,
               "served-descs-stats-end %s (%d s) total=%lu unique=%u "
               "max=%d q3=%d md=%d q1=%d min=%d\n",
               t,
               (unsigned) (now - start_of_served_descs_stats_interval),
               total_descriptor_downloads,
               size, max, q3, md, q1, min);

  return result;
}

/** If WRITE_STATS_INTERVAL seconds have passed since the beginning of
 * the current served desc stats interval, write the stats to
 * $DATADIR/stats/served-desc-stats (possibly appending to an existing file)
 * and reset the state for the next interval. Return when we would next want
 * to write served desc stats or 0 if we won't want to write. */
time_t
rep_hist_desc_stats_write(time_t now)
{
  char *filename = NULL, *str = NULL;

  if (!start_of_served_descs_stats_interval)
    return 0; /* We're not collecting stats. */
  if (start_of_served_descs_stats_interval + WRITE_STATS_INTERVAL > now)
    return start_of_served_descs_stats_interval + WRITE_STATS_INTERVAL;

  str = rep_hist_format_desc_stats(now);
  tor_assert(str != NULL);

  if (check_or_create_data_subdir("stats") < 0) {
    goto done;
  }
  filename = get_datadir_fname2("stats", "served-desc-stats");
  if (append_bytes_to_file(filename, str, strlen(str), 0) < 0)
    log_warn(LD_HIST, "Unable to write served descs statistics to disk!");

  rep_hist_reset_desc_stats(now);

 done:
  tor_free(filename);
  tor_free(str);
  return start_of_served_descs_stats_interval + WRITE_STATS_INTERVAL;
}

/* DOCDOC rep_hist_note_desc_served */
void
rep_hist_note_desc_served(const char * desc)
{
  void *val;
  uintptr_t count;
  if (!served_descs)
    return; // We're not collecting stats
  val = digestmap_get(served_descs, desc);
  count = (uintptr_t)val;
  if (count != INT_MAX)
    ++count;
  digestmap_set(served_descs, desc, (void*)count);
  total_descriptor_downloads++;
}

/*** Connection statistics ***/

/** Start of the current connection stats interval or 0 if we're not
 * collecting connection statistics. */
static time_t start_of_conn_stats_interval;

/** Initialize connection stats. */
void
rep_hist_conn_stats_init(time_t now)
{
  start_of_conn_stats_interval = now;
}

/* Count connections that we read and wrote less than these many bytes
 * from/to as below threshold. */
#define BIDI_THRESHOLD 20480

/* Count connections that we read or wrote at least this factor as many
 * bytes from/to than we wrote or read to/from as mostly reading or
 * writing. */
#define BIDI_FACTOR 10

/* Interval length in seconds for considering read and written bytes for
 * connection stats. */
#define BIDI_INTERVAL 10

/** Start of next BIDI_INTERVAL second interval. */
static time_t bidi_next_interval = 0;

/** Number of connections that we read and wrote less than BIDI_THRESHOLD
 * bytes from/to in BIDI_INTERVAL seconds. */
static uint32_t below_threshold = 0;

/** Number of connections that we read at least BIDI_FACTOR times more
 * bytes from than we wrote to in BIDI_INTERVAL seconds. */
static uint32_t mostly_read = 0;

/** Number of connections that we wrote at least BIDI_FACTOR times more
 * bytes to than we read from in BIDI_INTERVAL seconds. */
static uint32_t mostly_written = 0;

/** Number of connections that we read and wrote at least BIDI_THRESHOLD
 * bytes from/to, but not BIDI_FACTOR times more in either direction in
 * BIDI_INTERVAL seconds. */
static uint32_t both_read_and_written = 0;

/** Entry in a map from connection ID to the number of read and written
 * bytes on this connection in a BIDI_INTERVAL second interval. */
typedef struct bidi_map_entry_t {
  HT_ENTRY(bidi_map_entry_t) node;
  uint64_t conn_id; /**< Connection ID */
  size_t read; /**< Number of read bytes */
  size_t written; /**< Number of written bytes */
} bidi_map_entry_t;

/** Map of OR connections together with the number of read and written
 * bytes in the current BIDI_INTERVAL second interval. */
static HT_HEAD(bidimap, bidi_map_entry_t) bidi_map =
     HT_INITIALIZER();

static int
bidi_map_ent_eq(const bidi_map_entry_t *a, const bidi_map_entry_t *b)
{
  return a->conn_id == b->conn_id;
}

/* DOCDOC bidi_map_ent_hash */
static unsigned
bidi_map_ent_hash(const bidi_map_entry_t *entry)
{
  return (unsigned) entry->conn_id;
}

HT_PROTOTYPE(bidimap, bidi_map_entry_t, node, bidi_map_ent_hash,
             bidi_map_ent_eq);
HT_GENERATE(bidimap, bidi_map_entry_t, node, bidi_map_ent_hash,
            bidi_map_ent_eq, 0.6, malloc, realloc, free);

/* DOCDOC bidi_map_free */
static void
bidi_map_free(void)
{
  bidi_map_entry_t **ptr, **next, *ent;
  for (ptr = HT_START(bidimap, &bidi_map); ptr; ptr = next) {
    ent = *ptr;
    next = HT_NEXT_RMV(bidimap, &bidi_map, ptr);
    tor_free(ent);
  }
  HT_CLEAR(bidimap, &bidi_map);
}

/** Reset counters for conn statistics. */
void
rep_hist_reset_conn_stats(time_t now)
{
  start_of_conn_stats_interval = now;
  below_threshold = 0;
  mostly_read = 0;
  mostly_written = 0;
  both_read_and_written = 0;
  bidi_map_free();
}

/** Stop collecting connection stats in a way that we can re-start doing
 * so in rep_hist_conn_stats_init(). */
void
rep_hist_conn_stats_term(void)
{
  rep_hist_reset_conn_stats(0);
}

/** We read <b>num_read</b> bytes and wrote <b>num_written</b> from/to OR
 * connection <b>conn_id</b> in second <b>when</b>. If this is the first
 * observation in a new interval, sum up the last observations. Add bytes
 * for this connection. */
void
rep_hist_note_or_conn_bytes(uint64_t conn_id, size_t num_read,
                            size_t num_written, time_t when)
{
  if (!start_of_conn_stats_interval)
    return;
  /* Initialize */
  if (bidi_next_interval == 0)
    bidi_next_interval = when + BIDI_INTERVAL;
  /* Sum up last period's statistics */
  if (when >= bidi_next_interval) {
    bidi_map_entry_t **ptr, **next, *ent;
    for (ptr = HT_START(bidimap, &bidi_map); ptr; ptr = next) {
      ent = *ptr;
      if (ent->read + ent->written < BIDI_THRESHOLD)
        below_threshold++;
      else if (ent->read >= ent->written * BIDI_FACTOR)
        mostly_read++;
      else if (ent->written >= ent->read * BIDI_FACTOR)
        mostly_written++;
      else
        both_read_and_written++;
      next = HT_NEXT_RMV(bidimap, &bidi_map, ptr);
      tor_free(ent);
    }
    while (when >= bidi_next_interval)
      bidi_next_interval += BIDI_INTERVAL;
    log_info(LD_GENERAL, "%d below threshold, %d mostly read, "
             "%d mostly written, %d both read and written.",
             below_threshold, mostly_read, mostly_written,
             both_read_and_written);
  }
  /* Add this connection's bytes. */
  if (num_read > 0 || num_written > 0) {
    bidi_map_entry_t *entry, lookup;
    lookup.conn_id = conn_id;
    entry = HT_FIND(bidimap, &bidi_map, &lookup);
    if (entry) {
      entry->written += num_written;
      entry->read += num_read;
    } else {
      entry = tor_malloc_zero(sizeof(bidi_map_entry_t));
      entry->conn_id = conn_id;
      entry->written = num_written;
      entry->read = num_read;
      HT_INSERT(bidimap, &bidi_map, entry);
    }
  }
}

/** Return a newly allocated string containing the connection statistics
 * until <b>now</b>, or NULL if we're not collecting conn stats. Caller must
 * ensure start_of_conn_stats_interval is in the past. */
char *
rep_hist_format_conn_stats(time_t now)
{
  char *result, written[ISO_TIME_LEN+1];

  if (!start_of_conn_stats_interval)
    return NULL; /* Not initialized. */

  tor_assert(now >= start_of_conn_stats_interval);

  format_iso_time(written, now);
  tor_asprintf(&result, "conn-bi-direct %s (%d s) %d,%d,%d,%d\n",
               written,
               (unsigned) (now - start_of_conn_stats_interval),
               below_threshold,
               mostly_read,
               mostly_written,
               both_read_and_written);
  return result;
}

/** If 24 hours have passed since the beginning of the current conn stats
 * period, write conn stats to $DATADIR/stats/conn-stats (possibly
 * overwriting an existing file) and reset counters.  Return when we would
 * next want to write conn stats or 0 if we never want to write. */
time_t
rep_hist_conn_stats_write(time_t now)
{
  char *str = NULL;

  if (!start_of_conn_stats_interval)
    return 0; /* Not initialized. */
  if (start_of_conn_stats_interval + WRITE_STATS_INTERVAL > now)
    goto done; /* Not ready to write */

  /* Generate history string. */
  str = rep_hist_format_conn_stats(now);

  /* Reset counters. */
  rep_hist_reset_conn_stats(now);

  /* Try to write to disk. */
  if (!check_or_create_data_subdir("stats")) {
    write_to_data_subdir("stats", "conn-stats", str, "connection statistics");
  }

 done:
  tor_free(str);
  return start_of_conn_stats_interval + WRITE_STATS_INTERVAL;
}

/** Internal statistics to track how many requests of each type of
 * handshake we've received, and how many we've assigned to cpuworkers.
 * Useful for seeing trends in cpu load.
 * @{ */
STATIC int onion_handshakes_requested[MAX_ONION_HANDSHAKE_TYPE+1] = {0};
STATIC int onion_handshakes_assigned[MAX_ONION_HANDSHAKE_TYPE+1] = {0};
/**@}*/

/** A new onionskin (using the <b>type</b> handshake) has arrived. */
void
rep_hist_note_circuit_handshake_requested(uint16_t type)
{
  if (type <= MAX_ONION_HANDSHAKE_TYPE)
    onion_handshakes_requested[type]++;
}

/** We've sent an onionskin (using the <b>type</b> handshake) to a
 * cpuworker. */
void
rep_hist_note_circuit_handshake_assigned(uint16_t type)
{
  if (type <= MAX_ONION_HANDSHAKE_TYPE)
    onion_handshakes_assigned[type]++;
}

/** Log our onionskin statistics since the last time we were called. */
void
rep_hist_log_circuit_handshake_stats(time_t now)
{
  (void)now;
  log_notice(LD_HEARTBEAT, "Circuit handshake stats since last time: "
             "%d/%d TAP, %d/%d NTor.",
             onion_handshakes_assigned[ONION_HANDSHAKE_TYPE_TAP],
             onion_handshakes_requested[ONION_HANDSHAKE_TYPE_TAP],
             onion_handshakes_assigned[ONION_HANDSHAKE_TYPE_NTOR],
             onion_handshakes_requested[ONION_HANDSHAKE_TYPE_NTOR]);
  memset(onion_handshakes_assigned, 0, sizeof(onion_handshakes_assigned));
  memset(onion_handshakes_requested, 0, sizeof(onion_handshakes_requested));
}

/** Free all storage held by the OR/link history caches, by the
 * bandwidth history arrays, by the port history, or by statistics . */
void
rep_hist_free_all(void)
{
  digestmap_free(history_map, free_or_history);
  tor_free(read_array);
  tor_free(write_array);
  tor_free(dir_read_array);
  tor_free(dir_write_array);
  tor_free(exit_bytes_read);
  tor_free(exit_bytes_written);
  tor_free(exit_streams);
  predicted_ports_free();
  bidi_map_free();

  if (circuits_for_buffer_stats) {
    SMARTLIST_FOREACH(circuits_for_buffer_stats, circ_buffer_stats_t *, s,
                      tor_free(s));
    smartlist_free(circuits_for_buffer_stats);
    circuits_for_buffer_stats = NULL;
  }
  rep_hist_desc_stats_term();
  total_descriptor_downloads = 0;
}

