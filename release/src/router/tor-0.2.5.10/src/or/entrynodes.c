/* Copyright (c) 2001 Matej Pfajfar.
 * Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file entrynodes.c
 * \brief Code to manage our fixed first nodes for various functions.
 *
 * Entry nodes can be guards (for general use) or bridges (for censorship
 * circumvention).
 **/

#include "or.h"
#include "circpathbias.h"
#include "circuitbuild.h"
#include "circuitstats.h"
#include "config.h"
#include "confparse.h"
#include "connection.h"
#include "connection_or.h"
#include "control.h"
#include "directory.h"
#include "entrynodes.h"
#include "main.h"
#include "microdesc.h"
#include "networkstatus.h"
#include "nodelist.h"
#include "policies.h"
#include "router.h"
#include "routerlist.h"
#include "routerparse.h"
#include "routerset.h"
#include "transports.h"
#include "statefile.h"

/** Information about a configured bridge. Currently this just matches the
 * ones in the torrc file, but one day we may be able to learn about new
 * bridges on our own, and remember them in the state file. */
typedef struct {
  /** Address of the bridge. */
  tor_addr_t addr;
  /** TLS port for the bridge. */
  uint16_t port;
  /** Boolean: We are re-parsing our bridge list, and we are going to remove
   * this one if we don't find it in the list of configured bridges. */
  unsigned marked_for_removal : 1;
  /** Expected identity digest, or all zero bytes if we don't know what the
   * digest should be. */
  char identity[DIGEST_LEN];

  /** Name of pluggable transport protocol taken from its config line. */
  char *transport_name;

  /** When should we next try to fetch a descriptor for this bridge? */
  download_status_t fetch_status;

  /** A smartlist of k=v values to be passed to the SOCKS proxy, if
      transports are used for this bridge. */
  smartlist_t *socks_args;
} bridge_info_t;

/** A list of our chosen entry guards. */
static smartlist_t *entry_guards = NULL;
/** A value of 1 means that the entry_guards list has changed
 * and those changes need to be flushed to disk. */
static int entry_guards_dirty = 0;

static void bridge_free(bridge_info_t *bridge);
static const node_t *choose_random_entry_impl(cpath_build_state_t *state,
                                              int for_directory,
                                              dirinfo_type_t dirtype,
                                              int *n_options_out);
static int num_bridges_usable(void);

/** Return the list of entry guards, creating it if necessary. */
const smartlist_t *
get_entry_guards(void)
{
  if (! entry_guards)
    entry_guards = smartlist_new();
  return entry_guards;
}

/** Check whether the entry guard <b>e</b> is usable, given the directory
 * authorities' opinion about the router (stored in <b>ri</b>) and the user's
 * configuration (in <b>options</b>). Set <b>e</b>-&gt;bad_since
 * accordingly. Return true iff the entry guard's status changes.
 *
 * If it's not usable, set *<b>reason</b> to a static string explaining why.
 */
static int
entry_guard_set_status(entry_guard_t *e, const node_t *node,
                       time_t now, const or_options_t *options,
                       const char **reason)
{
  char buf[HEX_DIGEST_LEN+1];
  int changed = 0;

  *reason = NULL;

  /* Do we want to mark this guard as bad? */
  if (!node)
    *reason = "unlisted";
  else if (!node->is_running)
    *reason = "down";
  else if (options->UseBridges && (!node->ri ||
                                   node->ri->purpose != ROUTER_PURPOSE_BRIDGE))
    *reason = "not a bridge";
  else if (options->UseBridges && !node_is_a_configured_bridge(node))
    *reason = "not a configured bridge";
  else if (!options->UseBridges && !node->is_possible_guard &&
           !routerset_contains_node(options->EntryNodes,node))
    *reason = "not recommended as a guard";
  else if (routerset_contains_node(options->ExcludeNodes, node))
    *reason = "excluded";
  else if (e->path_bias_disabled)
    *reason = "path-biased";

  if (*reason && ! e->bad_since) {
    /* Router is newly bad. */
    base16_encode(buf, sizeof(buf), e->identity, DIGEST_LEN);
    log_info(LD_CIRC, "Entry guard %s (%s) is %s: marking as unusable.",
             e->nickname, buf, *reason);

    e->bad_since = now;
    control_event_guard(e->nickname, e->identity, "BAD");
    changed = 1;
  } else if (!*reason && e->bad_since) {
    /* There's nothing wrong with the router any more. */
    base16_encode(buf, sizeof(buf), e->identity, DIGEST_LEN);
    log_info(LD_CIRC, "Entry guard %s (%s) is no longer unusable: "
             "marking as ok.", e->nickname, buf);

    e->bad_since = 0;
    control_event_guard(e->nickname, e->identity, "GOOD");
    changed = 1;
  }

  if (node) {
    int is_dir = node_is_dir(node) && node->rs &&
      node->rs->version_supports_microdesc_cache;
    if (options->UseBridges && node_is_a_configured_bridge(node))
      is_dir = 1;
    if (e->is_dir_cache != is_dir) {
      e->is_dir_cache = is_dir;
      changed = 1;
    }
  }

  return changed;
}

/** Return true iff enough time has passed since we last tried to connect
 * to the unreachable guard <b>e</b> that we're willing to try again. */
static int
entry_is_time_to_retry(entry_guard_t *e, time_t now)
{
  long diff;
  if (e->last_attempted < e->unreachable_since)
    return 1;
  diff = now - e->unreachable_since;
  if (diff < 6*60*60)
    return now > (e->last_attempted + 60*60);
  else if (diff < 3*24*60*60)
    return now > (e->last_attempted + 4*60*60);
  else if (diff < 7*24*60*60)
    return now > (e->last_attempted + 18*60*60);
  else
    return now > (e->last_attempted + 36*60*60);
}

/** Return the node corresponding to <b>e</b>, if <b>e</b> is
 * working well enough that we are willing to use it as an entry
 * right now. (Else return NULL.) In particular, it must be
 * - Listed as either up or never yet contacted;
 * - Present in the routerlist;
 * - Listed as 'stable' or 'fast' by the current dirserver consensus,
 *   if demanded by <b>need_uptime</b> or <b>need_capacity</b>
 *   (unless it's a configured EntryNode);
 * - Allowed by our current ReachableORAddresses config option; and
 * - Currently thought to be reachable by us (unless <b>assume_reachable</b>
 *   is true).
 *
 * If the answer is no, set *<b>msg</b> to an explanation of why.
 *
 * If need_descriptor is true, only return the node if we currently have
 * a descriptor (routerinfo or microdesc) for it.
 */
static INLINE const node_t *
entry_is_live(entry_guard_t *e, int need_uptime, int need_capacity,
              int assume_reachable, int need_descriptor, const char **msg)
{
  const node_t *node;
  const or_options_t *options = get_options();
  tor_assert(msg);

  if (e->path_bias_disabled) {
    *msg = "path-biased";
    return NULL;
  }
  if (e->bad_since) {
    *msg = "bad";
    return NULL;
  }
  /* no good if it's unreachable, unless assume_unreachable or can_retry. */
  if (!assume_reachable && !e->can_retry &&
      e->unreachable_since && !entry_is_time_to_retry(e, time(NULL))) {
    *msg = "unreachable";
    return NULL;
  }
  node = node_get_by_id(e->identity);
  if (!node) {
    *msg = "no node info";
    return NULL;
  }
  if (need_descriptor && !node_has_descriptor(node)) {
    *msg = "no descriptor";
    return NULL;
  }
  if (get_options()->UseBridges) {
    if (node_get_purpose(node) != ROUTER_PURPOSE_BRIDGE) {
      *msg = "not a bridge";
      return NULL;
    }
    if (!node_is_a_configured_bridge(node)) {
      *msg = "not a configured bridge";
      return NULL;
    }
  } else { /* !get_options()->UseBridges */
    if (node_get_purpose(node) != ROUTER_PURPOSE_GENERAL) {
      *msg = "not general-purpose";
      return NULL;
    }
  }
  if (routerset_contains_node(options->EntryNodes, node)) {
    /* they asked for it, they get it */
    need_uptime = need_capacity = 0;
  }
  if (node_is_unreliable(node, need_uptime, need_capacity, 0)) {
    *msg = "not fast/stable";
    return NULL;
  }
  if (!fascist_firewall_allows_node(node)) {
    *msg = "unreachable by config";
    return NULL;
  }
  return node;
}

/** Return the number of entry guards that we think are usable. */
int
num_live_entry_guards(int for_directory)
{
  int n = 0;
  const char *msg;
  if (! entry_guards)
    return 0;
  SMARTLIST_FOREACH_BEGIN(entry_guards, entry_guard_t *, entry) {
      if (for_directory && !entry->is_dir_cache)
        continue;
      if (entry_is_live(entry, 0, 1, 0, !for_directory, &msg))
        ++n;
  } SMARTLIST_FOREACH_END(entry);
  return n;
}

/** If <b>digest</b> matches the identity of any node in the
 * entry_guards list, return that node. Else return NULL. */
entry_guard_t *
entry_guard_get_by_id_digest(const char *digest)
{
  SMARTLIST_FOREACH(entry_guards, entry_guard_t *, entry,
                    if (tor_memeq(digest, entry->identity, DIGEST_LEN))
                      return entry;
                   );
  return NULL;
}

/** Dump a description of our list of entry guards to the log at level
 * <b>severity</b>. */
static void
log_entry_guards(int severity)
{
  smartlist_t *elements = smartlist_new();
  char *s;

  SMARTLIST_FOREACH_BEGIN(entry_guards, entry_guard_t *, e)
    {
      const char *msg = NULL;
      if (entry_is_live(e, 0, 1, 0, 0, &msg))
        smartlist_add_asprintf(elements, "%s [%s] (up %s)",
                     e->nickname,
                     hex_str(e->identity, DIGEST_LEN),
                     e->made_contact ? "made-contact" : "never-contacted");
      else
        smartlist_add_asprintf(elements, "%s [%s] (%s, %s)",
                     e->nickname,
                     hex_str(e->identity, DIGEST_LEN),
                     msg,
                     e->made_contact ? "made-contact" : "never-contacted");
    }
  SMARTLIST_FOREACH_END(e);

  s = smartlist_join_strings(elements, ",", 0, NULL);
  SMARTLIST_FOREACH(elements, char*, cp, tor_free(cp));
  smartlist_free(elements);
  log_fn(severity,LD_CIRC,"%s",s);
  tor_free(s);
}

/** Called when one or more guards that we would previously have used for some
 * purpose are no longer in use because a higher-priority guard has become
 * usable again. */
static void
control_event_guard_deferred(void)
{
  /* XXXX We don't actually have a good way to figure out _how many_ entries
   * are live for some purpose.  We need an entry_is_even_slightly_live()
   * function for this to work right.  NumEntryGuards isn't reliable: if we
   * need guards with weird properties, we can have more than that number
   * live.
   **/
#if 0
  int n = 0;
  const char *msg;
  const or_options_t *options = get_options();
  if (!entry_guards)
    return;
  SMARTLIST_FOREACH(entry_guards, entry_guard_t *, entry,
    {
      if (entry_is_live(entry, 0, 1, 0, &msg)) {
        if (n++ == options->NumEntryGuards) {
          control_event_guard(entry->nickname, entry->identity, "DEFERRED");
          return;
        }
      }
    });
#endif
}

/** Largest amount that we'll backdate chosen_on_date */
#define CHOSEN_ON_DATE_SLOP (30*86400)

/** Add a new (preferably stable and fast) router to our
 * entry_guards list. Return a pointer to the router if we succeed,
 * or NULL if we can't find any more suitable entries.
 *
 * If <b>chosen</b> is defined, use that one, and if it's not
 * already in our entry_guards list, put it at the *beginning*.
 * Else, put the one we pick at the end of the list. */
static const node_t *
add_an_entry_guard(const node_t *chosen, int reset_status, int prepend,
                   int for_discovery, int for_directory)
{
  const node_t *node;
  entry_guard_t *entry;

  if (chosen) {
    node = chosen;
    entry = entry_guard_get_by_id_digest(node->identity);
    if (entry) {
      if (reset_status) {
        entry->bad_since = 0;
        entry->can_retry = 1;
      }
      entry->is_dir_cache = node->rs &&
                            node->rs->version_supports_microdesc_cache;
      if (get_options()->UseBridges && node_is_a_configured_bridge(node))
        entry->is_dir_cache = 1;
      return NULL;
    }
  } else if (!for_directory) {
    node = choose_good_entry_server(CIRCUIT_PURPOSE_C_GENERAL, NULL);
    if (!node)
      return NULL;
  } else {
    const routerstatus_t *rs;
    rs = router_pick_directory_server(MICRODESC_DIRINFO|V3_DIRINFO,
                                      PDS_FOR_GUARD);
    if (!rs)
      return NULL;
    node = node_get_by_id(rs->identity_digest);
    if (!node)
      return NULL;
  }
  if (node->using_as_guard)
    return NULL;
  if (entry_guard_get_by_id_digest(node->identity) != NULL) {
    log_info(LD_CIRC, "I was about to add a duplicate entry guard.");
    /* This can happen if we choose a guard, then the node goes away, then
     * comes back. */
    ((node_t*) node)->using_as_guard = 1;
    return NULL;
  }
  entry = tor_malloc_zero(sizeof(entry_guard_t));
  log_info(LD_CIRC, "Chose %s as new entry guard.",
           node_describe(node));
  strlcpy(entry->nickname, node_get_nickname(node), sizeof(entry->nickname));
  memcpy(entry->identity, node->identity, DIGEST_LEN);
  entry->is_dir_cache = node_is_dir(node) && node->rs &&
                        node->rs->version_supports_microdesc_cache;
  if (get_options()->UseBridges && node_is_a_configured_bridge(node))
    entry->is_dir_cache = 1;

  /* Choose expiry time smudged over the past month. The goal here
   * is to a) spread out when Tor clients rotate their guards, so they
   * don't all select them on the same day, and b) avoid leaving a
   * precise timestamp in the state file about when we first picked
   * this guard. For details, see the Jan 2010 or-dev thread. */
  entry->chosen_on_date = time(NULL) - crypto_rand_int(3600*24*30);
  entry->chosen_by_version = tor_strdup(VERSION);

  /* Are we picking this guard because all of our current guards are
   * down so we need another one (for_discovery is 1), or because we
   * decided we need more variety in our guard list (for_discovery is 0)?
   *
   * Currently we hack this behavior into place by setting "made_contact"
   * for guards of the latter variety, so we'll be willing to use any of
   * them right off the bat.
   */
  if (!for_discovery)
    entry->made_contact = 1;

  ((node_t*)node)->using_as_guard = 1;
  if (prepend)
    smartlist_insert(entry_guards, 0, entry);
  else
    smartlist_add(entry_guards, entry);
  control_event_guard(entry->nickname, entry->identity, "NEW");
  control_event_guard_deferred();
  log_entry_guards(LOG_INFO);
  return node;
}

/** Choose how many entry guards or directory guards we'll use. If
 * <b>for_directory</b> is true, we return how many directory guards to
 * use; else we return how many entry guards to use. */
static int
decide_num_guards(const or_options_t *options, int for_directory)
{
  if (for_directory) {
    int answer;
    if (options->NumDirectoryGuards != 0)
      return options->NumDirectoryGuards;
    answer = networkstatus_get_param(NULL, "NumDirectoryGuards", 0, 0, 10);
    if (answer) /* non-zero means use the consensus value */
      return answer;
  }

  if (options->NumEntryGuards)
    return options->NumEntryGuards;

  /* Use the value from the consensus, or 3 if no guidance. */
  return networkstatus_get_param(NULL, "NumEntryGuards", 3, 1, 10);
}

/** If the use of entry guards is configured, choose more entry guards
 * until we have enough in the list. */
static void
pick_entry_guards(const or_options_t *options, int for_directory)
{
  int changed = 0;
  const int num_needed = decide_num_guards(options, for_directory);

  tor_assert(entry_guards);

  while (num_live_entry_guards(for_directory) < num_needed) {
    if (!add_an_entry_guard(NULL, 0, 0, 0, for_directory))
      break;
    changed = 1;
  }
  if (changed)
    entry_guards_changed();
}

/** How long (in seconds) do we allow an entry guard to be nonfunctional,
 * unlisted, excluded, or otherwise nonusable before we give up on it? */
#define ENTRY_GUARD_REMOVE_AFTER (30*24*60*60)

/** Release all storage held by <b>e</b>. */
static void
entry_guard_free(entry_guard_t *e)
{
  if (!e)
    return;
  tor_free(e->chosen_by_version);
  tor_free(e);
}

/**
 * Return the minimum lifetime of working entry guard, in seconds,
 * as given in the consensus networkstatus.  (Plus CHOSEN_ON_DATE_SLOP,
 * so that we can do the chosen_on_date randomization while achieving the
 * desired minimum lifetime.)
 */
static int32_t
guards_get_lifetime(void)
{
  const or_options_t *options = get_options();
#define DFLT_GUARD_LIFETIME (86400 * 60)   /* Two months. */
#define MIN_GUARD_LIFETIME  (86400 * 30)   /* One months. */
#define MAX_GUARD_LIFETIME  (86400 * 1826) /* Five years. */

  if (options->GuardLifetime >= 1) {
    return CLAMP(MIN_GUARD_LIFETIME,
                 options->GuardLifetime,
                 MAX_GUARD_LIFETIME) + CHOSEN_ON_DATE_SLOP;
  }

  return networkstatus_get_param(NULL, "GuardLifetime",
                                 DFLT_GUARD_LIFETIME,
                                 MIN_GUARD_LIFETIME,
                                 MAX_GUARD_LIFETIME) + CHOSEN_ON_DATE_SLOP;
}

/** Remove any entry guard which was selected by an unknown version of Tor,
 * or which was selected by a version of Tor that's known to select
 * entry guards badly, or which was selected more 2 months ago. */
/* XXXX The "obsolete guards" and "chosen long ago guards" things should
 * probably be different functions. */
static int
remove_obsolete_entry_guards(time_t now)
{
  int changed = 0, i;
  int32_t guard_lifetime = guards_get_lifetime();

  for (i = 0; i < smartlist_len(entry_guards); ++i) {
    entry_guard_t *entry = smartlist_get(entry_guards, i);
    const char *ver = entry->chosen_by_version;
    const char *msg = NULL;
    tor_version_t v;
    int version_is_bad = 0, date_is_bad = 0;
    if (!ver) {
      msg = "does not say what version of Tor it was selected by";
      version_is_bad = 1;
    } else if (tor_version_parse(ver, &v)) {
      msg = "does not seem to be from any recognized version of Tor";
      version_is_bad = 1;
    } else {
      char *tor_ver = NULL;
      tor_asprintf(&tor_ver, "Tor %s", ver);
      if ((tor_version_as_new_as(tor_ver, "0.1.0.10-alpha") &&
           !tor_version_as_new_as(tor_ver, "0.1.2.16-dev")) ||
          (tor_version_as_new_as(tor_ver, "0.2.0.0-alpha") &&
           !tor_version_as_new_as(tor_ver, "0.2.0.6-alpha")) ||
          /* above are bug 440; below are bug 1217 */
          (tor_version_as_new_as(tor_ver, "0.2.1.3-alpha") &&
           !tor_version_as_new_as(tor_ver, "0.2.1.23")) ||
          (tor_version_as_new_as(tor_ver, "0.2.2.0-alpha") &&
           !tor_version_as_new_as(tor_ver, "0.2.2.7-alpha"))) {
        msg = "was selected without regard for guard bandwidth";
        version_is_bad = 1;
      }
      tor_free(tor_ver);
    }
    if (!version_is_bad && entry->chosen_on_date + guard_lifetime < now) {
      /* It's been too long since the date listed in our state file. */
      msg = "was selected several months ago";
      date_is_bad = 1;
    }

    if (version_is_bad || date_is_bad) { /* we need to drop it */
      char dbuf[HEX_DIGEST_LEN+1];
      tor_assert(msg);
      base16_encode(dbuf, sizeof(dbuf), entry->identity, DIGEST_LEN);
      log_fn(version_is_bad ? LOG_NOTICE : LOG_INFO, LD_CIRC,
             "Entry guard '%s' (%s) %s. (Version=%s.) Replacing it.",
             entry->nickname, dbuf, msg, ver?escaped(ver):"none");
      control_event_guard(entry->nickname, entry->identity, "DROPPED");
      entry_guard_free(entry);
      smartlist_del_keeporder(entry_guards, i--);
      log_entry_guards(LOG_INFO);
      changed = 1;
    }
  }

  return changed ? 1 : 0;
}

/** Remove all entry guards that have been down or unlisted for so
 * long that we don't think they'll come up again. Return 1 if we
 * removed any, or 0 if we did nothing. */
static int
remove_dead_entry_guards(time_t now)
{
  char dbuf[HEX_DIGEST_LEN+1];
  char tbuf[ISO_TIME_LEN+1];
  int i;
  int changed = 0;

  for (i = 0; i < smartlist_len(entry_guards); ) {
    entry_guard_t *entry = smartlist_get(entry_guards, i);
    if (entry->bad_since &&
        ! entry->path_bias_disabled &&
        entry->bad_since + ENTRY_GUARD_REMOVE_AFTER < now) {

      base16_encode(dbuf, sizeof(dbuf), entry->identity, DIGEST_LEN);
      format_local_iso_time(tbuf, entry->bad_since);
      log_info(LD_CIRC, "Entry guard '%s' (%s) has been down or unlisted "
               "since %s local time; removing.",
               entry->nickname, dbuf, tbuf);
      control_event_guard(entry->nickname, entry->identity, "DROPPED");
      entry_guard_free(entry);
      smartlist_del_keeporder(entry_guards, i);
      log_entry_guards(LOG_INFO);
      changed = 1;
    } else
      ++i;
  }
  return changed ? 1 : 0;
}

/** Remove all currently listed entry guards. So new ones will be chosen. */
void
remove_all_entry_guards(void)
{
  char dbuf[HEX_DIGEST_LEN+1];

  while (smartlist_len(entry_guards)) {
    entry_guard_t *entry = smartlist_get(entry_guards, 0);
    base16_encode(dbuf, sizeof(dbuf), entry->identity, DIGEST_LEN);
    log_info(LD_CIRC, "Entry guard '%s' (%s) has been dropped.",
             entry->nickname, dbuf);
    control_event_guard(entry->nickname, entry->identity, "DROPPED");
    entry_guard_free(entry);
    smartlist_del(entry_guards, 0);
  }
  log_entry_guards(LOG_INFO);
  entry_guards_changed();
}

/** A new directory or router-status has arrived; update the down/listed
 * status of the entry guards.
 *
 * An entry is 'down' if the directory lists it as nonrunning.
 * An entry is 'unlisted' if the directory doesn't include it.
 *
 * Don't call this on startup; only on a fresh download. Otherwise we'll
 * think that things are unlisted.
 */
void
entry_guards_compute_status(const or_options_t *options, time_t now)
{
  int changed = 0;
  digestmap_t *reasons;

  if (! entry_guards)
    return;

  if (options->EntryNodes) /* reshuffle the entry guard list if needed */
    entry_nodes_should_be_added();

  reasons = digestmap_new();
  SMARTLIST_FOREACH_BEGIN(entry_guards, entry_guard_t *, entry)
    {
      const node_t *r = node_get_by_id(entry->identity);
      const char *reason = NULL;
      if (entry_guard_set_status(entry, r, now, options, &reason))
        changed = 1;

      if (entry->bad_since)
        tor_assert(reason);
      if (reason)
        digestmap_set(reasons, entry->identity, (char*)reason);
    }
  SMARTLIST_FOREACH_END(entry);

  if (remove_dead_entry_guards(now))
    changed = 1;
  if (remove_obsolete_entry_guards(now))
    changed = 1;

  if (changed) {
    SMARTLIST_FOREACH_BEGIN(entry_guards, entry_guard_t *, entry) {
      const char *reason = digestmap_get(reasons, entry->identity);
      const char *live_msg = "";
      const node_t *r = entry_is_live(entry, 0, 1, 0, 0, &live_msg);
      log_info(LD_CIRC, "Summary: Entry %s [%s] is %s, %s%s%s, and %s%s.",
               entry->nickname,
               hex_str(entry->identity, DIGEST_LEN),
               entry->unreachable_since ? "unreachable" : "reachable",
               entry->bad_since ? "unusable" : "usable",
               reason ? ", ": "",
               reason ? reason : "",
               r ? "live" : "not live / ",
               r ? "" : live_msg);
    } SMARTLIST_FOREACH_END(entry);
    log_info(LD_CIRC, "    (%d/%d entry guards are usable/new)",
             num_live_entry_guards(0), smartlist_len(entry_guards));
    log_entry_guards(LOG_INFO);
    entry_guards_changed();
  }

  digestmap_free(reasons, NULL);
}

/** Called when a connection to an OR with the identity digest <b>digest</b>
 * is established (<b>succeeded</b>==1) or has failed (<b>succeeded</b>==0).
 * If the OR is an entry, change that entry's up/down status.
 * Return 0 normally, or -1 if we want to tear down the new connection.
 *
 * If <b>mark_relay_status</b>, also call router_set_status() on this
 * relay.
 *
 * XXX024 change succeeded and mark_relay_status into 'int flags'.
 */
int
entry_guard_register_connect_status(const char *digest, int succeeded,
                                    int mark_relay_status, time_t now)
{
  int changed = 0;
  int refuse_conn = 0;
  int first_contact = 0;
  entry_guard_t *entry = NULL;
  int idx = -1;
  char buf[HEX_DIGEST_LEN+1];

  if (! entry_guards)
    return 0;

  SMARTLIST_FOREACH_BEGIN(entry_guards, entry_guard_t *, e) {
    tor_assert(e);
    if (tor_memeq(e->identity, digest, DIGEST_LEN)) {
      entry = e;
      idx = e_sl_idx;
      break;
    }
  } SMARTLIST_FOREACH_END(e);

  if (!entry)
    return 0;

  base16_encode(buf, sizeof(buf), entry->identity, DIGEST_LEN);

  if (succeeded) {
    if (entry->unreachable_since) {
      log_info(LD_CIRC, "Entry guard '%s' (%s) is now reachable again. Good.",
               entry->nickname, buf);
      entry->can_retry = 0;
      entry->unreachable_since = 0;
      entry->last_attempted = now;
      control_event_guard(entry->nickname, entry->identity, "UP");
      changed = 1;
    }
    if (!entry->made_contact) {
      entry->made_contact = 1;
      first_contact = changed = 1;
    }
  } else { /* ! succeeded */
    if (!entry->made_contact) {
      /* We've never connected to this one. */
      log_info(LD_CIRC,
               "Connection to never-contacted entry guard '%s' (%s) failed. "
               "Removing from the list. %d/%d entry guards usable/new.",
               entry->nickname, buf,
               num_live_entry_guards(0)-1, smartlist_len(entry_guards)-1);
      control_event_guard(entry->nickname, entry->identity, "DROPPED");
      entry_guard_free(entry);
      smartlist_del_keeporder(entry_guards, idx);
      log_entry_guards(LOG_INFO);
      changed = 1;
    } else if (!entry->unreachable_since) {
      log_info(LD_CIRC, "Unable to connect to entry guard '%s' (%s). "
               "Marking as unreachable.", entry->nickname, buf);
      entry->unreachable_since = entry->last_attempted = now;
      control_event_guard(entry->nickname, entry->identity, "DOWN");
      changed = 1;
      entry->can_retry = 0; /* We gave it an early chance; no good. */
    } else {
      char tbuf[ISO_TIME_LEN+1];
      format_iso_time(tbuf, entry->unreachable_since);
      log_debug(LD_CIRC, "Failed to connect to unreachable entry guard "
                "'%s' (%s).  It has been unreachable since %s.",
                entry->nickname, buf, tbuf);
      entry->last_attempted = now;
      entry->can_retry = 0; /* We gave it an early chance; no good. */
    }
  }

  /* if the caller asked us to, also update the is_running flags for this
   * relay */
  if (mark_relay_status)
    router_set_status(digest, succeeded);

  if (first_contact) {
    /* We've just added a new long-term entry guard. Perhaps the network just
     * came back? We should give our earlier entries another try too,
     * and close this connection so we don't use it before we've given
     * the others a shot. */
    SMARTLIST_FOREACH_BEGIN(entry_guards, entry_guard_t *, e) {
        if (e == entry)
          break;
        if (e->made_contact) {
          const char *msg;
          const node_t *r = entry_is_live(e, 0, 1, 1, 0, &msg);
          if (r && e->unreachable_since) {
            refuse_conn = 1;
            e->can_retry = 1;
          }
        }
    } SMARTLIST_FOREACH_END(e);
    if (refuse_conn) {
      log_info(LD_CIRC,
               "Connected to new entry guard '%s' (%s). Marking earlier "
               "entry guards up. %d/%d entry guards usable/new.",
               entry->nickname, buf,
               num_live_entry_guards(0), smartlist_len(entry_guards));
      log_entry_guards(LOG_INFO);
      changed = 1;
    }
  }

  if (changed)
    entry_guards_changed();
  return refuse_conn ? -1 : 0;
}

/** When we try to choose an entry guard, should we parse and add
 * config's EntryNodes first? */
static int should_add_entry_nodes = 0;

/** Called when the value of EntryNodes changes in our configuration. */
void
entry_nodes_should_be_added(void)
{
  log_info(LD_CIRC, "EntryNodes config option set. Putting configured "
           "relays at the front of the entry guard list.");
  should_add_entry_nodes = 1;
}

/** Update the using_as_guard fields of all the nodes. We do this after we
 * remove entry guards from the list: This is the only function that clears
 * the using_as_guard field. */
static void
update_node_guard_status(void)
{
  smartlist_t *nodes = nodelist_get_list();
  SMARTLIST_FOREACH(nodes, node_t *, node, node->using_as_guard = 0);
  SMARTLIST_FOREACH_BEGIN(entry_guards, entry_guard_t *, entry) {
    node_t *node = node_get_mutable_by_id(entry->identity);
    if (node)
      node->using_as_guard = 1;
  } SMARTLIST_FOREACH_END(entry);
}

/** Adjust the entry guards list so that it only contains entries from
 * EntryNodes, adding new entries from EntryNodes to the list as needed. */
static void
entry_guards_set_from_config(const or_options_t *options)
{
  smartlist_t *entry_nodes, *worse_entry_nodes, *entry_fps;
  smartlist_t *old_entry_guards_on_list, *old_entry_guards_not_on_list;
  const int numentryguards = decide_num_guards(options, 0);
  tor_assert(entry_guards);

  should_add_entry_nodes = 0;

  if (!options->EntryNodes) {
    /* It's possible that a controller set EntryNodes, thus making
     * should_add_entry_nodes set, then cleared it again, all before the
     * call to choose_random_entry() that triggered us. If so, just return.
     */
    return;
  }

  {
    char *string = routerset_to_string(options->EntryNodes);
    log_info(LD_CIRC,"Adding configured EntryNodes '%s'.", string);
    tor_free(string);
  }

  entry_nodes = smartlist_new();
  worse_entry_nodes = smartlist_new();
  entry_fps = smartlist_new();
  old_entry_guards_on_list = smartlist_new();
  old_entry_guards_not_on_list = smartlist_new();

  /* Split entry guards into those on the list and those not. */

  routerset_get_all_nodes(entry_nodes, options->EntryNodes,
                          options->ExcludeNodes, 0);
  SMARTLIST_FOREACH(entry_nodes, const node_t *,node,
                    smartlist_add(entry_fps, (void*)node->identity));

  SMARTLIST_FOREACH(entry_guards, entry_guard_t *, e, {
    if (smartlist_contains_digest(entry_fps, e->identity))
      smartlist_add(old_entry_guards_on_list, e);
    else
      smartlist_add(old_entry_guards_not_on_list, e);
  });

  /* Remove all currently configured guard nodes, excluded nodes, unreachable
   * nodes, or non-Guard nodes from entry_nodes. */
  SMARTLIST_FOREACH_BEGIN(entry_nodes, const node_t *, node) {
    if (entry_guard_get_by_id_digest(node->identity)) {
      SMARTLIST_DEL_CURRENT(entry_nodes, node);
      continue;
    } else if (routerset_contains_node(options->ExcludeNodes, node)) {
      SMARTLIST_DEL_CURRENT(entry_nodes, node);
      continue;
    } else if (!fascist_firewall_allows_node(node)) {
      SMARTLIST_DEL_CURRENT(entry_nodes, node);
      continue;
    } else if (! node->is_possible_guard) {
      smartlist_add(worse_entry_nodes, (node_t*)node);
      SMARTLIST_DEL_CURRENT(entry_nodes, node);
    }
  } SMARTLIST_FOREACH_END(node);

  /* Now build the new entry_guards list. */
  smartlist_clear(entry_guards);
  /* First, the previously configured guards that are in EntryNodes. */
  smartlist_add_all(entry_guards, old_entry_guards_on_list);
  /* Next, scramble the rest of EntryNodes, putting the guards first. */
  smartlist_shuffle(entry_nodes);
  smartlist_shuffle(worse_entry_nodes);
  smartlist_add_all(entry_nodes, worse_entry_nodes);

  /* Next, the rest of EntryNodes */
  SMARTLIST_FOREACH_BEGIN(entry_nodes, const node_t *, node) {
    add_an_entry_guard(node, 0, 0, 1, 0);
    if (smartlist_len(entry_guards) > numentryguards * 10)
      break;
  } SMARTLIST_FOREACH_END(node);
  log_notice(LD_GENERAL, "%d entries in guards", smartlist_len(entry_guards));
  /* Finally, free the remaining previously configured guards that are not in
   * EntryNodes. */
  SMARTLIST_FOREACH(old_entry_guards_not_on_list, entry_guard_t *, e,
                    entry_guard_free(e));

  update_node_guard_status();

  smartlist_free(entry_nodes);
  smartlist_free(worse_entry_nodes);
  smartlist_free(entry_fps);
  smartlist_free(old_entry_guards_on_list);
  smartlist_free(old_entry_guards_not_on_list);
  entry_guards_changed();
}

/** Return 0 if we're fine adding arbitrary routers out of the
 * directory to our entry guard list, or return 1 if we have a
 * list already and we must stick to it.
 */
int
entry_list_is_constrained(const or_options_t *options)
{
  if (options->EntryNodes)
    return 1;
  if (options->UseBridges)
    return 1;
  return 0;
}

/** Return true iff this node can answer directory questions about
 * microdescriptors. */
static int
node_understands_microdescriptors(const node_t *node)
{
  tor_assert(node);
  if (node->rs && node->rs->version_supports_microdesc_cache)
    return 1;
  if (node->ri && tor_version_supports_microdescriptors(node->ri->platform))
    return 1;
  return 0;
}

/** Return true iff <b>node</b> is able to answer directory questions
 * of type <b>dirinfo</b>. */
static int
node_can_handle_dirinfo(const node_t *node, dirinfo_type_t dirinfo)
{
  /* Checking dirinfo for any type other than microdescriptors isn't required
     yet, since we only choose directory guards that can support microdescs,
     routerinfos, and networkstatuses, AND we don't use directory guards if
     we're configured to do direct downloads of anything else. The only case
     where we might have a guard that doesn't know about a type of directory
     information is when we're retrieving directory information from a
     bridge. */

  if ((dirinfo & MICRODESC_DIRINFO) &&
      !node_understands_microdescriptors(node))
    return 0;
  return 1;
}

/** Pick a live (up and listed) entry guard from entry_guards. If
 * <b>state</b> is non-NULL, this is for a specific circuit --
 * make sure not to pick this circuit's exit or any node in the
 * exit's family. If <b>state</b> is NULL, we're looking for a random
 * guard (likely a bridge).  If <b>dirinfo</b> is not NO_DIRINFO, then
 * only select from nodes that know how to answer directory questions
 * of that type. */
const node_t *
choose_random_entry(cpath_build_state_t *state)
{
  return choose_random_entry_impl(state, 0, 0, NULL);
}

/** Pick a live (up and listed) directory guard from entry_guards for
 * downloading information of type <b>type</b>. */
const node_t *
choose_random_dirguard(dirinfo_type_t type)
{
  return choose_random_entry_impl(NULL, 1, type, NULL);
}

/** Helper for choose_random{entry,dirguard}. */
static const node_t *
choose_random_entry_impl(cpath_build_state_t *state, int for_directory,
                         dirinfo_type_t dirinfo_type, int *n_options_out)
{
  const or_options_t *options = get_options();
  smartlist_t *live_entry_guards = smartlist_new();
  smartlist_t *exit_family = smartlist_new();
  const node_t *chosen_exit =
    state?build_state_get_exit_node(state) : NULL;
  const node_t *node = NULL;
  int need_uptime = state ? state->need_uptime : 0;
  int need_capacity = state ? state->need_capacity : 0;
  int preferred_min, consider_exit_family = 0;
  int need_descriptor = !for_directory;
  const int num_needed = decide_num_guards(options, for_directory);

  if (n_options_out)
    *n_options_out = 0;

  if (chosen_exit) {
    nodelist_add_node_and_family(exit_family, chosen_exit);
    consider_exit_family = 1;
  }

  if (!entry_guards)
    entry_guards = smartlist_new();

  if (should_add_entry_nodes)
    entry_guards_set_from_config(options);

  if (!entry_list_is_constrained(options) &&
      smartlist_len(entry_guards) < num_needed)
    pick_entry_guards(options, for_directory);

 retry:
  smartlist_clear(live_entry_guards);
  SMARTLIST_FOREACH_BEGIN(entry_guards, entry_guard_t *, entry) {
      const char *msg;
      node = entry_is_live(entry, need_uptime, need_capacity, 0,
                           need_descriptor, &msg);
      if (!node)
        continue; /* down, no point */
      if (for_directory) {
        if (!entry->is_dir_cache)
          continue; /* We need a directory and didn't get one. */
      }
      if (node == chosen_exit)
        continue; /* don't pick the same node for entry and exit */
      if (consider_exit_family && smartlist_contains(exit_family, node))
        continue; /* avoid relays that are family members of our exit */
      if (dirinfo_type != NO_DIRINFO &&
          !node_can_handle_dirinfo(node, dirinfo_type))
        continue; /* this node won't be able to answer our dir questions */
#if 0 /* since EntryNodes is always strict now, this clause is moot */
      if (options->EntryNodes &&
          !routerset_contains_node(options->EntryNodes, node)) {
        /* We've come to the end of our preferred entry nodes. */
        if (smartlist_len(live_entry_guards))
          goto choose_and_finish; /* only choose from the ones we like */
        if (options->StrictNodes) {
          /* in theory this case should never happen, since
           * entry_guards_set_from_config() drops unwanted relays */
          tor_fragile_assert();
        } else {
          log_info(LD_CIRC,
                   "No relays from EntryNodes available. Using others.");
        }
      }
#endif
      smartlist_add(live_entry_guards, (void*)node);
      if (!entry->made_contact) {
        /* Always start with the first not-yet-contacted entry
         * guard. Otherwise we might add several new ones, pick
         * the second new one, and now we've expanded our entry
         * guard list without needing to. */
        goto choose_and_finish;
      }
      if (smartlist_len(live_entry_guards) >= num_needed)
        goto choose_and_finish; /* we have enough */
  } SMARTLIST_FOREACH_END(entry);

  if (entry_list_is_constrained(options)) {
    /* If we prefer the entry nodes we've got, and we have at least
     * one choice, that's great. Use it. */
    preferred_min = 1;
  } else {
    /* Try to have at least 2 choices available. This way we don't
     * get stuck with a single live-but-crummy entry and just keep
     * using him.
     * (We might get 2 live-but-crummy entry guards, but so be it.) */
    preferred_min = 2;
  }

  if (smartlist_len(live_entry_guards) < preferred_min) {
    if (!entry_list_is_constrained(options)) {
      /* still no? try adding a new entry then */
      /* XXX if guard doesn't imply fast and stable, then we need
       * to tell add_an_entry_guard below what we want, or it might
       * be a long time til we get it. -RD */
      node = add_an_entry_guard(NULL, 0, 0, 1, for_directory);
      if (node) {
        entry_guards_changed();
        /* XXX we start over here in case the new node we added shares
         * a family with our exit node. There's a chance that we'll just
         * load up on entry guards here, if the network we're using is
         * one big family. Perhaps we should teach add_an_entry_guard()
         * to understand nodes-to-avoid-if-possible? -RD */
        goto retry;
      }
    }
    if (!node && need_uptime) {
      need_uptime = 0; /* try without that requirement */
      goto retry;
    }
    if (!node && need_capacity) {
      /* still no? last attempt, try without requiring capacity */
      need_capacity = 0;
      goto retry;
    }
#if 0
    /* Removing this retry logic: if we only allow one exit, and it is in the
       same family as all our entries, then we are just plain not going to win
       here. */
    if (!node && entry_list_is_constrained(options) && consider_exit_family) {
      /* still no? if we're using bridges or have strictentrynodes
       * set, and our chosen exit is in the same family as all our
       * bridges/entry guards, then be flexible about families. */
      consider_exit_family = 0;
      goto retry;
    }
#endif
    /* live_entry_guards may be empty below. Oh well, we tried. */
  }

 choose_and_finish:
  if (entry_list_is_constrained(options)) {
    /* We need to weight by bandwidth, because our bridges or entryguards
     * were not already selected proportional to their bandwidth. */
    node = node_sl_choose_by_bandwidth(live_entry_guards, WEIGHT_FOR_GUARD);
  } else {
    /* We choose uniformly at random here, because choose_good_entry_server()
     * already weights its choices by bandwidth, so we don't want to
     * *double*-weight our guard selection. */
    node = smartlist_choose(live_entry_guards);
  }
  if (n_options_out)
    *n_options_out = smartlist_len(live_entry_guards);
  smartlist_free(live_entry_guards);
  smartlist_free(exit_family);
  return node;
}

/** Parse <b>state</b> and learn about the entry guards it describes.
 * If <b>set</b> is true, and there are no errors, replace the global
 * entry_list with what we find.
 * On success, return 0. On failure, alloc into *<b>msg</b> a string
 * describing the error, and return -1.
 */
int
entry_guards_parse_state(or_state_t *state, int set, char **msg)
{
  entry_guard_t *node = NULL;
  smartlist_t *new_entry_guards = smartlist_new();
  config_line_t *line;
  time_t now = time(NULL);
  const char *state_version = state->TorVersion;
  digestmap_t *added_by = digestmap_new();

  *msg = NULL;
  for (line = state->EntryGuards; line; line = line->next) {
    if (!strcasecmp(line->key, "EntryGuard")) {
      smartlist_t *args = smartlist_new();
      node = tor_malloc_zero(sizeof(entry_guard_t));
      /* all entry guards on disk have been contacted */
      node->made_contact = 1;
      smartlist_add(new_entry_guards, node);
      smartlist_split_string(args, line->value, " ",
                             SPLIT_SKIP_SPACE|SPLIT_IGNORE_BLANK, 0);
      if (smartlist_len(args)<2) {
        *msg = tor_strdup("Unable to parse entry nodes: "
                          "Too few arguments to EntryGuard");
      } else if (!is_legal_nickname(smartlist_get(args,0))) {
        *msg = tor_strdup("Unable to parse entry nodes: "
                          "Bad nickname for EntryGuard");
      } else {
        strlcpy(node->nickname, smartlist_get(args,0), MAX_NICKNAME_LEN+1);
        if (base16_decode(node->identity, DIGEST_LEN, smartlist_get(args,1),
                          strlen(smartlist_get(args,1)))<0) {
          *msg = tor_strdup("Unable to parse entry nodes: "
                            "Bad hex digest for EntryGuard");
        }
      }
      if (smartlist_len(args) >= 3) {
        const char *is_cache = smartlist_get(args, 2);
        if (!strcasecmp(is_cache, "DirCache")) {
          node->is_dir_cache = 1;
        } else if (!strcasecmp(is_cache, "NoDirCache")) {
          node->is_dir_cache = 0;
        } else {
          log_warn(LD_CONFIG, "Bogus third argument to EntryGuard line: %s",
                   escaped(is_cache));
        }
      }
      SMARTLIST_FOREACH(args, char*, cp, tor_free(cp));
      smartlist_free(args);
      if (*msg)
        break;
    } else if (!strcasecmp(line->key, "EntryGuardDownSince") ||
               !strcasecmp(line->key, "EntryGuardUnlistedSince")) {
      time_t when;
      time_t last_try = 0;
      if (!node) {
        *msg = tor_strdup("Unable to parse entry nodes: "
               "EntryGuardDownSince/UnlistedSince without EntryGuard");
        break;
      }
      if (parse_iso_time(line->value, &when)<0) {
        *msg = tor_strdup("Unable to parse entry nodes: "
                          "Bad time in EntryGuardDownSince/UnlistedSince");
        break;
      }
      if (when > now) {
        /* It's a bad idea to believe info in the future: you can wind
         * up with timeouts that aren't allowed to happen for years. */
        continue;
      }
      if (strlen(line->value) >= ISO_TIME_LEN+ISO_TIME_LEN+1) {
        /* ignore failure */
        (void) parse_iso_time(line->value+ISO_TIME_LEN+1, &last_try);
      }
      if (!strcasecmp(line->key, "EntryGuardDownSince")) {
        node->unreachable_since = when;
        node->last_attempted = last_try;
      } else {
        node->bad_since = when;
      }
    } else if (!strcasecmp(line->key, "EntryGuardAddedBy")) {
      char d[DIGEST_LEN];
      /* format is digest version date */
      if (strlen(line->value) < HEX_DIGEST_LEN+1+1+1+ISO_TIME_LEN) {
        log_warn(LD_BUG, "EntryGuardAddedBy line is not long enough.");
        continue;
      }
      if (base16_decode(d, sizeof(d), line->value, HEX_DIGEST_LEN)<0 ||
          line->value[HEX_DIGEST_LEN] != ' ') {
        log_warn(LD_BUG, "EntryGuardAddedBy line %s does not begin with "
                 "hex digest", escaped(line->value));
        continue;
      }
      digestmap_set(added_by, d, tor_strdup(line->value+HEX_DIGEST_LEN+1));
    } else if (!strcasecmp(line->key, "EntryGuardPathUseBias")) {
      const or_options_t *options = get_options();
      double use_cnt, success_cnt;

      if (!node) {
        *msg = tor_strdup("Unable to parse entry nodes: "
               "EntryGuardPathUseBias without EntryGuard");
        break;
      }

      if (tor_sscanf(line->value, "%lf %lf",
                     &use_cnt, &success_cnt) != 2) {
        log_info(LD_GENERAL, "Malformed path use bias line for node %s",
                 node->nickname);
        continue;
      }

      if (use_cnt < success_cnt) {
        int severity = LOG_INFO;
        /* If this state file was written by a Tor that would have
         * already fixed it, then the overcounting bug is still there.. */
        if (tor_version_as_new_as(state_version, "0.2.4.13-alpha")) {
          severity = LOG_NOTICE;
        }
        log_fn(severity, LD_BUG,
                   "State file contains unexpectedly high usage success "
                   "counts %lf/%lf for Guard %s ($%s)",
                   success_cnt, use_cnt,
                   node->nickname, hex_str(node->identity, DIGEST_LEN));
        success_cnt = use_cnt;
      }

      node->use_attempts = use_cnt;
      node->use_successes = success_cnt;

      log_info(LD_GENERAL, "Read %f/%f path use bias for node %s",
               node->use_successes, node->use_attempts, node->nickname);

      /* Note: We rely on the < comparison here to allow us to set a 0
       * rate and disable the feature entirely. If refactoring, don't
       * change to <= */
      if (pathbias_get_use_success_count(node)/node->use_attempts
            < pathbias_get_extreme_use_rate(options) &&
          pathbias_get_dropguards(options)) {
        node->path_bias_disabled = 1;
        log_info(LD_GENERAL,
                 "Path use bias is too high (%f/%f); disabling node %s",
                 node->circ_successes, node->circ_attempts, node->nickname);
      }
    } else if (!strcasecmp(line->key, "EntryGuardPathBias")) {
      const or_options_t *options = get_options();
      double hop_cnt, success_cnt, timeouts, collapsed, successful_closed,
               unusable;

      if (!node) {
        *msg = tor_strdup("Unable to parse entry nodes: "
               "EntryGuardPathBias without EntryGuard");
        break;
      }

      /* First try 3 params, then 2. */
      /* In the long run: circuit_success ~= successful_circuit_close +
       *                                     collapsed_circuits +
       *                                     unusable_circuits */
      if (tor_sscanf(line->value, "%lf %lf %lf %lf %lf %lf",
                  &hop_cnt, &success_cnt, &successful_closed,
                  &collapsed, &unusable, &timeouts) != 6) {
        int old_success, old_hops;
        if (tor_sscanf(line->value, "%u %u", &old_success, &old_hops) != 2) {
          continue;
        }
        log_info(LD_GENERAL, "Reading old-style EntryGuardPathBias %s",
                 escaped(line->value));

        success_cnt = old_success;
        successful_closed = old_success;
        hop_cnt = old_hops;
        timeouts = 0;
        collapsed = 0;
        unusable = 0;
      }

      if (hop_cnt < success_cnt) {
        int severity = LOG_INFO;
        /* If this state file was written by a Tor that would have
         * already fixed it, then the overcounting bug is still there.. */
        if (tor_version_as_new_as(state_version, "0.2.4.13-alpha")) {
          severity = LOG_NOTICE;
        }
        log_fn(severity, LD_BUG,
                "State file contains unexpectedly high success counts "
                "%lf/%lf for Guard %s ($%s)",
                success_cnt, hop_cnt,
                node->nickname, hex_str(node->identity, DIGEST_LEN));
        success_cnt = hop_cnt;
      }

      node->circ_attempts = hop_cnt;
      node->circ_successes = success_cnt;

      node->successful_circuits_closed = successful_closed;
      node->timeouts = timeouts;
      node->collapsed_circuits = collapsed;
      node->unusable_circuits = unusable;

      log_info(LD_GENERAL, "Read %f/%f path bias for node %s",
               node->circ_successes, node->circ_attempts, node->nickname);
      /* Note: We rely on the < comparison here to allow us to set a 0
       * rate and disable the feature entirely. If refactoring, don't
       * change to <= */
      if (pathbias_get_close_success_count(node)/node->circ_attempts
            < pathbias_get_extreme_rate(options) &&
          pathbias_get_dropguards(options)) {
        node->path_bias_disabled = 1;
        log_info(LD_GENERAL,
                 "Path bias is too high (%f/%f); disabling node %s",
                 node->circ_successes, node->circ_attempts, node->nickname);
      }

    } else {
      log_warn(LD_BUG, "Unexpected key %s", line->key);
    }
  }

  SMARTLIST_FOREACH_BEGIN(new_entry_guards, entry_guard_t *, e) {
     char *sp;
     char *val = digestmap_get(added_by, e->identity);
     if (val && (sp = strchr(val, ' '))) {
       time_t when;
       *sp++ = '\0';
       if (parse_iso_time(sp, &when)<0) {
         log_warn(LD_BUG, "Can't read time %s in EntryGuardAddedBy", sp);
       } else {
         e->chosen_by_version = tor_strdup(val);
         e->chosen_on_date = when;
       }
     } else {
       if (state_version) {
         e->chosen_by_version = tor_strdup(state_version);
         e->chosen_on_date = time(NULL) - crypto_rand_int(3600*24*30);
       }
     }
     if (e->path_bias_disabled && !e->bad_since)
       e->bad_since = time(NULL);
    }
  SMARTLIST_FOREACH_END(e);

  if (*msg || !set) {
    SMARTLIST_FOREACH(new_entry_guards, entry_guard_t *, e,
                      entry_guard_free(e));
    smartlist_free(new_entry_guards);
  } else { /* !err && set */
    if (entry_guards) {
      SMARTLIST_FOREACH(entry_guards, entry_guard_t *, e,
                        entry_guard_free(e));
      smartlist_free(entry_guards);
    }
    entry_guards = new_entry_guards;
    entry_guards_dirty = 0;
    /* XXX024 hand new_entry_guards to this func, and move it up a
     * few lines, so we don't have to re-dirty it */
    if (remove_obsolete_entry_guards(now))
      entry_guards_dirty = 1;

    update_node_guard_status();
  }
  digestmap_free(added_by, tor_free_);
  return *msg ? -1 : 0;
}

/** Our list of entry guards has changed, or some element of one
 * of our entry guards has changed. Write the changes to disk within
 * the next few minutes.
 */
void
entry_guards_changed(void)
{
  time_t when;
  entry_guards_dirty = 1;

  /* or_state_save() will call entry_guards_update_state(). */
  when = get_options()->AvoidDiskWrites ? time(NULL) + 3600 : time(NULL)+600;
  or_state_mark_dirty(get_or_state(), when);
}

/** If the entry guard info has not changed, do nothing and return.
 * Otherwise, free the EntryGuards piece of <b>state</b> and create
 * a new one out of the global entry_guards list, and then mark
 * <b>state</b> dirty so it will get saved to disk.
 */
void
entry_guards_update_state(or_state_t *state)
{
  config_line_t **next, *line;
  if (! entry_guards_dirty)
    return;

  config_free_lines(state->EntryGuards);
  next = &state->EntryGuards;
  *next = NULL;
  if (!entry_guards)
    entry_guards = smartlist_new();
  SMARTLIST_FOREACH_BEGIN(entry_guards, entry_guard_t *, e) {
      char dbuf[HEX_DIGEST_LEN+1];
      if (!e->made_contact)
        continue; /* don't write this one to disk */
      *next = line = tor_malloc_zero(sizeof(config_line_t));
      line->key = tor_strdup("EntryGuard");
      base16_encode(dbuf, sizeof(dbuf), e->identity, DIGEST_LEN);
      tor_asprintf(&line->value, "%s %s %sDirCache", e->nickname, dbuf,
                   e->is_dir_cache ? "" : "No");
      next = &(line->next);
      if (e->unreachable_since) {
        *next = line = tor_malloc_zero(sizeof(config_line_t));
        line->key = tor_strdup("EntryGuardDownSince");
        line->value = tor_malloc(ISO_TIME_LEN+1+ISO_TIME_LEN+1);
        format_iso_time(line->value, e->unreachable_since);
        if (e->last_attempted) {
          line->value[ISO_TIME_LEN] = ' ';
          format_iso_time(line->value+ISO_TIME_LEN+1, e->last_attempted);
        }
        next = &(line->next);
      }
      if (e->bad_since) {
        *next = line = tor_malloc_zero(sizeof(config_line_t));
        line->key = tor_strdup("EntryGuardUnlistedSince");
        line->value = tor_malloc(ISO_TIME_LEN+1);
        format_iso_time(line->value, e->bad_since);
        next = &(line->next);
      }
      if (e->chosen_on_date && e->chosen_by_version &&
          !strchr(e->chosen_by_version, ' ')) {
        char d[HEX_DIGEST_LEN+1];
        char t[ISO_TIME_LEN+1];
        *next = line = tor_malloc_zero(sizeof(config_line_t));
        line->key = tor_strdup("EntryGuardAddedBy");
        base16_encode(d, sizeof(d), e->identity, DIGEST_LEN);
        format_iso_time(t, e->chosen_on_date);
        tor_asprintf(&line->value, "%s %s %s",
                     d, e->chosen_by_version, t);
        next = &(line->next);
      }
      if (e->circ_attempts > 0) {
        *next = line = tor_malloc_zero(sizeof(config_line_t));
        line->key = tor_strdup("EntryGuardPathBias");
        /* In the long run: circuit_success ~= successful_circuit_close +
         *                                     collapsed_circuits +
         *                                     unusable_circuits */
        tor_asprintf(&line->value, "%f %f %f %f %f %f",
                     e->circ_attempts, e->circ_successes,
                     pathbias_get_close_success_count(e),
                     e->collapsed_circuits,
                     e->unusable_circuits, e->timeouts);
        next = &(line->next);
      }
      if (e->use_attempts > 0) {
        *next = line = tor_malloc_zero(sizeof(config_line_t));
        line->key = tor_strdup("EntryGuardPathUseBias");

        tor_asprintf(&line->value, "%f %f",
                     e->use_attempts,
                     pathbias_get_use_success_count(e));
        next = &(line->next);
      }

  } SMARTLIST_FOREACH_END(e);
  if (!get_options()->AvoidDiskWrites)
    or_state_mark_dirty(get_or_state(), 0);
  entry_guards_dirty = 0;
}

/** If <b>question</b> is the string "entry-guards", then dump
 * to *<b>answer</b> a newly allocated string describing all of
 * the nodes in the global entry_guards list. See control-spec.txt
 * for details.
 * For backward compatibility, we also handle the string "helper-nodes".
 * */
int
getinfo_helper_entry_guards(control_connection_t *conn,
                            const char *question, char **answer,
                            const char **errmsg)
{
  (void) conn;
  (void) errmsg;

  if (!strcmp(question,"entry-guards") ||
      !strcmp(question,"helper-nodes")) {
    smartlist_t *sl = smartlist_new();
    char tbuf[ISO_TIME_LEN+1];
    char nbuf[MAX_VERBOSE_NICKNAME_LEN+1];
    if (!entry_guards)
      entry_guards = smartlist_new();
    SMARTLIST_FOREACH_BEGIN(entry_guards, entry_guard_t *, e) {
        const char *status = NULL;
        time_t when = 0;
        const node_t *node;

        if (!e->made_contact) {
          status = "never-connected";
        } else if (e->bad_since) {
          when = e->bad_since;
          status = "unusable";
        } else {
          status = "up";
        }

        node = node_get_by_id(e->identity);
        if (node) {
          node_get_verbose_nickname(node, nbuf);
        } else {
          nbuf[0] = '$';
          base16_encode(nbuf+1, sizeof(nbuf)-1, e->identity, DIGEST_LEN);
          /* e->nickname field is not very reliable if we don't know about
           * this router any longer; don't include it. */
        }

        if (when) {
          format_iso_time(tbuf, when);
          smartlist_add_asprintf(sl, "%s %s %s\n", nbuf, status, tbuf);
        } else {
          smartlist_add_asprintf(sl, "%s %s\n", nbuf, status);
        }
    } SMARTLIST_FOREACH_END(e);
    *answer = smartlist_join_strings(sl, "", 0, NULL);
    SMARTLIST_FOREACH(sl, char *, c, tor_free(c));
    smartlist_free(sl);
  }
  return 0;
}

/** A list of configured bridges. Whenever we actually get a descriptor
 * for one, we add it as an entry guard.  Note that the order of bridges
 * in this list does not necessarily correspond to the order of bridges
 * in the torrc. */
static smartlist_t *bridge_list = NULL;

/** Mark every entry of the bridge list to be removed on our next call to
 * sweep_bridge_list unless it has first been un-marked. */
void
mark_bridge_list(void)
{
  if (!bridge_list)
    bridge_list = smartlist_new();
  SMARTLIST_FOREACH(bridge_list, bridge_info_t *, b,
                    b->marked_for_removal = 1);
}

/** Remove every entry of the bridge list that was marked with
 * mark_bridge_list if it has not subsequently been un-marked. */
void
sweep_bridge_list(void)
{
  if (!bridge_list)
    bridge_list = smartlist_new();
  SMARTLIST_FOREACH_BEGIN(bridge_list, bridge_info_t *, b) {
    if (b->marked_for_removal) {
      SMARTLIST_DEL_CURRENT(bridge_list, b);
      bridge_free(b);
    }
  } SMARTLIST_FOREACH_END(b);
}

/** Initialize the bridge list to empty, creating it if needed. */
static void
clear_bridge_list(void)
{
  if (!bridge_list)
    bridge_list = smartlist_new();
  SMARTLIST_FOREACH(bridge_list, bridge_info_t *, b, bridge_free(b));
  smartlist_clear(bridge_list);
}

/** Free the bridge <b>bridge</b>. */
static void
bridge_free(bridge_info_t *bridge)
{
  if (!bridge)
    return;

  tor_free(bridge->transport_name);
  if (bridge->socks_args) {
    SMARTLIST_FOREACH(bridge->socks_args, char*, s, tor_free(s));
    smartlist_free(bridge->socks_args);
  }

  tor_free(bridge);
}

/** If we have a bridge configured whose digest matches <b>digest</b>, or a
 * bridge with no known digest whose address matches any of the
 * tor_addr_port_t's in <b>orports</b>, return that bridge.  Else return
 * NULL. */
static bridge_info_t *
get_configured_bridge_by_orports_digest(const char *digest,
                                        const smartlist_t *orports)
{
  if (!bridge_list)
    return NULL;
  SMARTLIST_FOREACH_BEGIN(bridge_list, bridge_info_t *, bridge)
    {
      if (tor_digest_is_zero(bridge->identity)) {
        SMARTLIST_FOREACH_BEGIN(orports, tor_addr_port_t *, ap)
          {
            if (tor_addr_compare(&bridge->addr, &ap->addr, CMP_EXACT) == 0 &&
                bridge->port == ap->port)
              return bridge;
          }
        SMARTLIST_FOREACH_END(ap);
      }
      if (digest && tor_memeq(bridge->identity, digest, DIGEST_LEN))
        return bridge;
    }
  SMARTLIST_FOREACH_END(bridge);
  return NULL;
}

/** If we have a bridge configured whose digest matches <b>digest</b>, or a
 * bridge with no known digest whose address matches <b>addr</b>:<b>/port</b>,
 * return that bridge.  Else return NULL. If <b>digest</b> is NULL, check for
 * address/port matches only. */
static bridge_info_t *
get_configured_bridge_by_addr_port_digest(const tor_addr_t *addr,
                                          uint16_t port,
                                          const char *digest)
{
  if (!bridge_list)
    return NULL;
  SMARTLIST_FOREACH_BEGIN(bridge_list, bridge_info_t *, bridge)
    {
      if ((tor_digest_is_zero(bridge->identity) || digest == NULL) &&
          !tor_addr_compare(&bridge->addr, addr, CMP_EXACT) &&
          bridge->port == port)
        return bridge;
      if (digest && tor_memeq(bridge->identity, digest, DIGEST_LEN))
        return bridge;
    }
  SMARTLIST_FOREACH_END(bridge);
  return NULL;
}

/** Wrapper around get_configured_bridge_by_addr_port_digest() to look
 * it up via router descriptor <b>ri</b>. */
static bridge_info_t *
get_configured_bridge_by_routerinfo(const routerinfo_t *ri)
{
  bridge_info_t *bi = NULL;
  smartlist_t *orports = router_get_all_orports(ri);
  bi = get_configured_bridge_by_orports_digest(ri->cache_info.identity_digest,
                                               orports);
  SMARTLIST_FOREACH(orports, tor_addr_port_t *, p, tor_free(p));
  smartlist_free(orports);
  return bi;
}

/** Return 1 if <b>ri</b> is one of our known bridges, else 0. */
int
routerinfo_is_a_configured_bridge(const routerinfo_t *ri)
{
  return get_configured_bridge_by_routerinfo(ri) ? 1 : 0;
}

/** Return 1 if <b>node</b> is one of our configured bridges, else 0. */
int
node_is_a_configured_bridge(const node_t *node)
{
  int retval = 0;
  smartlist_t *orports = node_get_all_orports(node);
  retval = get_configured_bridge_by_orports_digest(node->identity,
                                                   orports) != NULL;
  SMARTLIST_FOREACH(orports, tor_addr_port_t *, p, tor_free(p));
  smartlist_free(orports);
  return retval;
}

/** We made a connection to a router at <b>addr</b>:<b>port</b>
 * without knowing its digest. Its digest turned out to be <b>digest</b>.
 * If it was a bridge, and we still don't know its digest, record it.
 */
void
learned_router_identity(const tor_addr_t *addr, uint16_t port,
                        const char *digest)
{
  bridge_info_t *bridge =
    get_configured_bridge_by_addr_port_digest(addr, port, digest);
  if (bridge && tor_digest_is_zero(bridge->identity)) {
    char *transport_info = NULL;
    const char *transport_name =
      find_transport_name_by_bridge_addrport(addr, port);
    if (transport_name)
      tor_asprintf(&transport_info, " (with transport '%s')", transport_name);

    memcpy(bridge->identity, digest, DIGEST_LEN);
    log_notice(LD_DIR, "Learned fingerprint %s for bridge %s%s.",
               hex_str(digest, DIGEST_LEN), fmt_addrport(addr, port),
               transport_info ? transport_info : "");
    tor_free(transport_info);
  }
}

/** Return true if <b>bridge</b> has the same identity digest as
 *  <b>digest</b>. If <b>digest</b> is NULL, it matches
 *  bridges with unspecified identity digests. */
static int
bridge_has_digest(const bridge_info_t *bridge, const char *digest)
{
  if (digest)
    return tor_memeq(digest, bridge->identity, DIGEST_LEN);
  else
    return tor_digest_is_zero(bridge->identity);
}

/** We are about to add a new bridge at <b>addr</b>:<b>port</b>, with optional
 * <b>digest</b> and <b>transport_name</b>. Mark for removal any previously
 * existing bridge with the same address and port, and warn the user as
 * appropriate.
 */
static void
bridge_resolve_conflicts(const tor_addr_t *addr, uint16_t port,
                         const char *digest, const char *transport_name)
{
  /* Iterate the already-registered bridge list:

     If you find a bridge with the same adress and port, mark it for
     removal. It doesn't make sense to have two active bridges with
     the same IP:PORT. If the bridge in question has a different
     digest or transport than <b>digest</b>/<b>transport_name</b>,
     it's probably a misconfiguration and we should warn the user.
  */
  SMARTLIST_FOREACH_BEGIN(bridge_list, bridge_info_t *, bridge) {
    if (bridge->marked_for_removal)
      continue;

    if (tor_addr_eq(&bridge->addr, addr) && (bridge->port == port)) {

      bridge->marked_for_removal = 1;

      if (!bridge_has_digest(bridge, digest) ||
          strcmp_opt(bridge->transport_name, transport_name)) {
        /* warn the user */
        char *bridge_description_new, *bridge_description_old;
        tor_asprintf(&bridge_description_new, "%s:%s:%s",
                     fmt_addrport(addr, port),
                     digest ? hex_str(digest, DIGEST_LEN) : "",
                     transport_name ? transport_name : "");
        tor_asprintf(&bridge_description_old, "%s:%s:%s",
                     fmt_addrport(&bridge->addr, bridge->port),
                     tor_digest_is_zero(bridge->identity) ?
                     "" : hex_str(bridge->identity,DIGEST_LEN),
                     bridge->transport_name ? bridge->transport_name : "");

        log_warn(LD_GENERAL,"Tried to add bridge '%s', but we found a conflict"
                 " with the already registered bridge '%s'. We will discard"
                 " the old bridge and keep '%s'. If this is not what you"
                 " wanted, please change your configuration file accordingly.",
                 bridge_description_new, bridge_description_old,
                 bridge_description_new);

        tor_free(bridge_description_new);
        tor_free(bridge_description_old);
      }
    }
  } SMARTLIST_FOREACH_END(bridge);
}

/** Return True if we have a bridge that uses a transport with name
 *  <b>transport_name</b>. */
int
transport_is_needed(const char *transport_name)
{
  if (!bridge_list)
    return 0;

  SMARTLIST_FOREACH_BEGIN(bridge_list, const bridge_info_t *, bridge) {
    if (bridge->transport_name &&
        !strcmp(bridge->transport_name, transport_name))
      return 1;
  } SMARTLIST_FOREACH_END(bridge);

  return 0;
}

/** Register the bridge information in <b>bridge_line</b> to the
 *  bridge subsystem. Steals reference of <b>bridge_line</b>. */
void
bridge_add_from_config(bridge_line_t *bridge_line)
{
  bridge_info_t *b;

  { /* Log the bridge we are about to register: */
    log_debug(LD_GENERAL, "Registering bridge at %s (transport: %s) (%s)",
              fmt_addrport(&bridge_line->addr, bridge_line->port),
              bridge_line->transport_name ?
              bridge_line->transport_name : "no transport",
              tor_digest_is_zero(bridge_line->digest) ?
              "no key listed" : hex_str(bridge_line->digest, DIGEST_LEN));

    if (bridge_line->socks_args) { /* print socks arguments */
      int i = 0;

      tor_assert(smartlist_len(bridge_line->socks_args) > 0);

      log_debug(LD_GENERAL, "Bridge uses %d SOCKS arguments:",
                smartlist_len(bridge_line->socks_args));
      SMARTLIST_FOREACH(bridge_line->socks_args, const char *, arg,
                        log_debug(LD_CONFIG, "%d: %s", ++i, arg));
    }
  }

  bridge_resolve_conflicts(&bridge_line->addr,
                           bridge_line->port,
                           bridge_line->digest,
                           bridge_line->transport_name);

  b = tor_malloc_zero(sizeof(bridge_info_t));
  tor_addr_copy(&b->addr, &bridge_line->addr);
  b->port = bridge_line->port;
  memcpy(b->identity, bridge_line->digest, DIGEST_LEN);
  if (bridge_line->transport_name)
    b->transport_name = bridge_line->transport_name;
  b->fetch_status.schedule = DL_SCHED_BRIDGE;
  b->socks_args = bridge_line->socks_args;
  if (!bridge_list)
    bridge_list = smartlist_new();

  tor_free(bridge_line); /* Deallocate bridge_line now. */

  smartlist_add(bridge_list, b);
}

/** Return true iff <b>routerset</b> contains the bridge <b>bridge</b>. */
static int
routerset_contains_bridge(const routerset_t *routerset,
                          const bridge_info_t *bridge)
{
  int result;
  extend_info_t *extinfo;
  tor_assert(bridge);
  if (!routerset)
    return 0;

  extinfo = extend_info_new(
         NULL, bridge->identity, NULL, NULL, &bridge->addr, bridge->port);
  result = routerset_contains_extendinfo(routerset, extinfo);
  extend_info_free(extinfo);
  return result;
}

/** If <b>digest</b> is one of our known bridges, return it. */
static bridge_info_t *
find_bridge_by_digest(const char *digest)
{
  SMARTLIST_FOREACH(bridge_list, bridge_info_t *, bridge,
    {
      if (tor_memeq(bridge->identity, digest, DIGEST_LEN))
        return bridge;
    });
  return NULL;
}

/** Given the <b>addr</b> and <b>port</b> of a bridge, if that bridge
 *  supports a pluggable transport, return its name. Otherwise, return
 *  NULL. */
const char *
find_transport_name_by_bridge_addrport(const tor_addr_t *addr, uint16_t port)
{
  if (!bridge_list)
    return NULL;

  SMARTLIST_FOREACH_BEGIN(bridge_list, const bridge_info_t *, bridge) {
    if (tor_addr_eq(&bridge->addr, addr) &&
        (bridge->port == port))
      return bridge->transport_name;
  } SMARTLIST_FOREACH_END(bridge);

  return NULL;
}

/** If <b>addr</b> and <b>port</b> match the address and port of a
 * bridge of ours that uses pluggable transports, place its transport
 * in <b>transport</b>.
 *
 * Return 0 on success (found a transport, or found a bridge with no
 * transport, or found no bridge); return -1 if we should be using a
 * transport, but the transport could not be found.
 */
int
get_transport_by_bridge_addrport(const tor_addr_t *addr, uint16_t port,
                                  const transport_t **transport)
{
  *transport = NULL;
  if (!bridge_list)
    return 0;

  SMARTLIST_FOREACH_BEGIN(bridge_list, const bridge_info_t *, bridge) {
    if (tor_addr_eq(&bridge->addr, addr) &&
        (bridge->port == port)) { /* bridge matched */
      if (bridge->transport_name) { /* it also uses pluggable transports */
        *transport = transport_get_by_name(bridge->transport_name);
        if (*transport == NULL) { /* it uses pluggable transports, but
                                     the transport could not be found! */
          return -1;
        }
        return 0;
      } else { /* bridge matched, but it doesn't use transports. */
        break;
      }
    }
  } SMARTLIST_FOREACH_END(bridge);

  *transport = NULL;
  return 0;
}

/** Return a smartlist containing all the SOCKS arguments that we
 *  should pass to the SOCKS proxy. */
const smartlist_t *
get_socks_args_by_bridge_addrport(const tor_addr_t *addr, uint16_t port)
{
  bridge_info_t *bridge = get_configured_bridge_by_addr_port_digest(addr,
                                                                    port,
                                                                    NULL);
  return bridge ? bridge->socks_args : NULL;
}

/** We need to ask <b>bridge</b> for its server descriptor. */
static void
launch_direct_bridge_descriptor_fetch(bridge_info_t *bridge)
{
  const or_options_t *options = get_options();

  if (connection_get_by_type_addr_port_purpose(
      CONN_TYPE_DIR, &bridge->addr, bridge->port,
      DIR_PURPOSE_FETCH_SERVERDESC))
    return; /* it's already on the way */

  if (routerset_contains_bridge(options->ExcludeNodes, bridge)) {
    download_status_mark_impossible(&bridge->fetch_status);
    log_warn(LD_APP, "Not using bridge at %s: it is in ExcludeNodes.",
             safe_str_client(fmt_and_decorate_addr(&bridge->addr)));
    return;
  }

  directory_initiate_command(&bridge->addr,
                             bridge->port, 0/*no dirport*/,
                             bridge->identity,
                             DIR_PURPOSE_FETCH_SERVERDESC,
                             ROUTER_PURPOSE_BRIDGE,
                             DIRIND_ONEHOP, "authority.z", NULL, 0, 0);
}

/** Fetching the bridge descriptor from the bridge authority returned a
 * "not found". Fall back to trying a direct fetch. */
void
retry_bridge_descriptor_fetch_directly(const char *digest)
{
  bridge_info_t *bridge = find_bridge_by_digest(digest);
  if (!bridge)
    return; /* not found? oh well. */

  launch_direct_bridge_descriptor_fetch(bridge);
}

/** For each bridge in our list for which we don't currently have a
 * descriptor, fetch a new copy of its descriptor -- either directly
 * from the bridge or via a bridge authority. */
void
fetch_bridge_descriptors(const or_options_t *options, time_t now)
{
  int num_bridge_auths = get_n_authorities(BRIDGE_DIRINFO);
  int ask_bridge_directly;
  int can_use_bridge_authority;

  if (!bridge_list)
    return;

  /* If we still have unconfigured managed proxies, don't go and
     connect to a bridge. */
  if (pt_proxies_configuration_pending())
    return;

  SMARTLIST_FOREACH_BEGIN(bridge_list, bridge_info_t *, bridge)
    {
      if (!download_status_is_ready(&bridge->fetch_status, now,
                                    IMPOSSIBLE_TO_DOWNLOAD))
        continue; /* don't bother, no need to retry yet */
      if (routerset_contains_bridge(options->ExcludeNodes, bridge)) {
        download_status_mark_impossible(&bridge->fetch_status);
        log_warn(LD_APP, "Not using bridge at %s: it is in ExcludeNodes.",
                 safe_str_client(fmt_and_decorate_addr(&bridge->addr)));
        continue;
      }

      /* schedule another fetch as if this one will fail, in case it does */
      download_status_failed(&bridge->fetch_status, 0);

      can_use_bridge_authority = !tor_digest_is_zero(bridge->identity) &&
                                 num_bridge_auths;
      ask_bridge_directly = !can_use_bridge_authority ||
                            !options->UpdateBridgesFromAuthority;
      log_debug(LD_DIR, "ask_bridge_directly=%d (%d, %d, %d)",
                ask_bridge_directly, tor_digest_is_zero(bridge->identity),
                !options->UpdateBridgesFromAuthority, !num_bridge_auths);

      if (ask_bridge_directly &&
          !fascist_firewall_allows_address_or(&bridge->addr, bridge->port)) {
        log_notice(LD_DIR, "Bridge at '%s' isn't reachable by our "
                   "firewall policy. %s.",
                   fmt_addrport(&bridge->addr, bridge->port),
                   can_use_bridge_authority ?
                     "Asking bridge authority instead" : "Skipping");
        if (can_use_bridge_authority)
          ask_bridge_directly = 0;
        else
          continue;
      }

      if (ask_bridge_directly) {
        /* we need to ask the bridge itself for its descriptor. */
        launch_direct_bridge_descriptor_fetch(bridge);
      } else {
        /* We have a digest and we want to ask an authority. We could
         * combine all the requests into one, but that may give more
         * hints to the bridge authority than we want to give. */
        char resource[10 + HEX_DIGEST_LEN];
        memcpy(resource, "fp/", 3);
        base16_encode(resource+3, HEX_DIGEST_LEN+1,
                      bridge->identity, DIGEST_LEN);
        memcpy(resource+3+HEX_DIGEST_LEN, ".z", 3);
        log_info(LD_DIR, "Fetching bridge info '%s' from bridge authority.",
                 resource);
        directory_get_from_dirserver(DIR_PURPOSE_FETCH_SERVERDESC,
                ROUTER_PURPOSE_BRIDGE, resource, 0);
      }
    }
  SMARTLIST_FOREACH_END(bridge);
}

/** If our <b>bridge</b> is configured to be a different address than
 * the bridge gives in <b>node</b>, rewrite the routerinfo
 * we received to use the address we meant to use. Now we handle
 * multihomed bridges better.
 */
static void
rewrite_node_address_for_bridge(const bridge_info_t *bridge, node_t *node)
{
  /* XXXX move this function. */
  /* XXXX overridden addresses should really live in the node_t, so that the
   *   routerinfo_t and the microdesc_t can be immutable.  But we can only
   *   do that safely if we know that no function that connects to an OR
   *   does so through an address from any source other than node_get_addr().
   */
  tor_addr_t addr;

  if (node->ri) {
    routerinfo_t *ri = node->ri;
    tor_addr_from_ipv4h(&addr, ri->addr);

    if ((!tor_addr_compare(&bridge->addr, &addr, CMP_EXACT) &&
         bridge->port == ri->or_port) ||
        (!tor_addr_compare(&bridge->addr, &ri->ipv6_addr, CMP_EXACT) &&
         bridge->port == ri->ipv6_orport)) {
      /* they match, so no need to do anything */
    } else {
      if (tor_addr_family(&bridge->addr) == AF_INET) {
        ri->addr = tor_addr_to_ipv4h(&bridge->addr);
        ri->or_port = bridge->port;
        log_info(LD_DIR,
                 "Adjusted bridge routerinfo for '%s' to match configured "
                 "address %s:%d.",
                 ri->nickname, fmt_addr32(ri->addr), ri->or_port);
      } else if (tor_addr_family(&bridge->addr) == AF_INET6) {
        tor_addr_copy(&ri->ipv6_addr, &bridge->addr);
        ri->ipv6_orport = bridge->port;
        log_info(LD_DIR,
                 "Adjusted bridge routerinfo for '%s' to match configured "
                 "address %s.",
                 ri->nickname, fmt_addrport(&ri->ipv6_addr, ri->ipv6_orport));
      } else {
        log_err(LD_BUG, "Address family not supported: %d.",
                tor_addr_family(&bridge->addr));
        return;
      }
    }

    /* Mark which address to use based on which bridge_t we got. */
    node->ipv6_preferred = (tor_addr_family(&bridge->addr) == AF_INET6 &&
                            !tor_addr_is_null(&node->ri->ipv6_addr));

    /* XXXipv6 we lack support for falling back to another address for
       the same relay, warn the user */
    if (!tor_addr_is_null(&ri->ipv6_addr)) {
      tor_addr_port_t ap;
      node_get_pref_orport(node, &ap);
      log_notice(LD_CONFIG,
                 "Bridge '%s' has both an IPv4 and an IPv6 address.  "
                 "Will prefer using its %s address (%s).",
                 ri->nickname,
                 tor_addr_family(&ap.addr) == AF_INET6 ? "IPv6" : "IPv4",
                 fmt_addrport(&ap.addr, ap.port));
    }
  }
  if (node->rs) {
    routerstatus_t *rs = node->rs;
    tor_addr_from_ipv4h(&addr, rs->addr);

    if (!tor_addr_compare(&bridge->addr, &addr, CMP_EXACT) &&
        bridge->port == rs->or_port) {
      /* they match, so no need to do anything */
    } else {
      rs->addr = tor_addr_to_ipv4h(&bridge->addr);
      rs->or_port = bridge->port;
      log_info(LD_DIR,
               "Adjusted bridge routerstatus for '%s' to match "
               "configured address %s.",
               rs->nickname, fmt_addrport(&bridge->addr, rs->or_port));
    }
  }
}

/** We just learned a descriptor for a bridge. See if that
 * digest is in our entry guard list, and add it if not. */
void
learned_bridge_descriptor(routerinfo_t *ri, int from_cache)
{
  tor_assert(ri);
  tor_assert(ri->purpose == ROUTER_PURPOSE_BRIDGE);
  if (get_options()->UseBridges) {
    int first = num_bridges_usable() <= 1;
    bridge_info_t *bridge = get_configured_bridge_by_routerinfo(ri);
    time_t now = time(NULL);
    router_set_status(ri->cache_info.identity_digest, 1);

    if (bridge) { /* if we actually want to use this one */
      node_t *node;
      /* it's here; schedule its re-fetch for a long time from now. */
      if (!from_cache)
        download_status_reset(&bridge->fetch_status);

      node = node_get_mutable_by_id(ri->cache_info.identity_digest);
      tor_assert(node);
      rewrite_node_address_for_bridge(bridge, node);
      add_an_entry_guard(node, 1, 1, 0, 0);

      log_notice(LD_DIR, "new bridge descriptor '%s' (%s): %s", ri->nickname,
                 from_cache ? "cached" : "fresh", router_describe(ri));
      /* set entry->made_contact so if it goes down we don't drop it from
       * our entry node list */
      entry_guard_register_connect_status(ri->cache_info.identity_digest,
                                          1, 0, now);
      if (first) {
        routerlist_retry_directory_downloads(now);
      }
    }
  }
}

/** Return the number of bridges that have descriptors that
 * are marked with purpose 'bridge' and are running.
 *
 * We use this function to decide if we're ready to start building
 * circuits through our bridges, or if we need to wait until the
 * directory "server/authority" requests finish. */
int
any_bridge_descriptors_known(void)
{
  tor_assert(get_options()->UseBridges);
  return choose_random_entry(NULL) != NULL;
}

/** Return the number of bridges that have descriptors that are marked with
 * purpose 'bridge' and are running.
 */
static int
num_bridges_usable(void)
{
  int n_options = 0;
  tor_assert(get_options()->UseBridges);
  (void) choose_random_entry_impl(NULL, 0, 0, &n_options);
  return n_options;
}

/** Return 1 if we have at least one descriptor for an entry guard
 * (bridge or member of EntryNodes) and all descriptors we know are
 * down. Else return 0. If <b>act</b> is 1, then mark the down guards
 * up; else just observe and report. */
static int
entries_retry_helper(const or_options_t *options, int act)
{
  const node_t *node;
  int any_known = 0;
  int any_running = 0;
  int need_bridges = options->UseBridges != 0;
  if (!entry_guards)
    entry_guards = smartlist_new();
  SMARTLIST_FOREACH_BEGIN(entry_guards, entry_guard_t *, e) {
      node = node_get_by_id(e->identity);
      if (node && node_has_descriptor(node) &&
          node_is_bridge(node) == need_bridges) {
        any_known = 1;
        if (node->is_running)
          any_running = 1; /* some entry is both known and running */
        else if (act) {
          /* Mark all current connections to this OR as unhealthy, since
           * otherwise there could be one that started 30 seconds
           * ago, and in 30 seconds it will time out, causing us to mark
           * the node down and undermine the retry attempt. We mark even
           * the established conns, since if the network just came back
           * we'll want to attach circuits to fresh conns. */
          connection_or_set_bad_connections(node->identity, 1);

          /* mark this entry node for retry */
          router_set_status(node->identity, 1);
          e->can_retry = 1;
          e->bad_since = 0;
        }
      }
  } SMARTLIST_FOREACH_END(e);
  log_debug(LD_DIR, "%d: any_known %d, any_running %d",
            act, any_known, any_running);
  return any_known && !any_running;
}

/** Do we know any descriptors for our bridges / entrynodes, and are
 * all the ones we have descriptors for down? */
int
entries_known_but_down(const or_options_t *options)
{
  tor_assert(entry_list_is_constrained(options));
  return entries_retry_helper(options, 0);
}

/** Mark all down known bridges / entrynodes up. */
void
entries_retry_all(const or_options_t *options)
{
  tor_assert(entry_list_is_constrained(options));
  entries_retry_helper(options, 1);
}

/** Return true if at least one of our bridges runs a Tor version that can
 * provide microdescriptors to us. If not, we'll fall back to asking for
 * full descriptors. */
int
any_bridge_supports_microdescriptors(void)
{
  const node_t *node;
  if (!get_options()->UseBridges || !entry_guards)
    return 0;
  SMARTLIST_FOREACH_BEGIN(entry_guards, entry_guard_t *, e) {
    node = node_get_by_id(e->identity);
    if (node && node->is_running &&
        node_is_bridge(node) && node_is_a_configured_bridge(node) &&
        node_understands_microdescriptors(node)) {
      /* This is one of our current bridges, and we know enough about
       * it to know that it will be able to answer our microdescriptor
       * questions. */
       return 1;
    }
  } SMARTLIST_FOREACH_END(e);
  return 0;
}

/** Release all storage held by the list of entry guards and related
 * memory structs. */
void
entry_guards_free_all(void)
{
  if (entry_guards) {
    SMARTLIST_FOREACH(entry_guards, entry_guard_t *, e,
                      entry_guard_free(e));
    smartlist_free(entry_guards);
    entry_guards = NULL;
  }
  clear_bridge_list();
  smartlist_free(bridge_list);
  bridge_list = NULL;
  circuit_build_times_free_timeouts(get_circuit_build_times_mutable());
}

