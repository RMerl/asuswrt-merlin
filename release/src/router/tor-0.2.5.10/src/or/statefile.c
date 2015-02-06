/* Copyright (c) 2001 Matej Pfajfar.
 * Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#define STATEFILE_PRIVATE
#include "or.h"
#include "circuitstats.h"
#include "config.h"
#include "confparse.h"
#include "entrynodes.h"
#include "hibernate.h"
#include "rephist.h"
#include "router.h"
#include "sandbox.h"
#include "statefile.h"

/** A list of state-file "abbreviations," for compatibility. */
static config_abbrev_t state_abbrevs_[] = {
  { "AccountingBytesReadInterval", "AccountingBytesReadInInterval", 0, 0 },
  { "HelperNode", "EntryGuard", 0, 0 },
  { "HelperNodeDownSince", "EntryGuardDownSince", 0, 0 },
  { "HelperNodeUnlistedSince", "EntryGuardUnlistedSince", 0, 0 },
  { "EntryNode", "EntryGuard", 0, 0 },
  { "EntryNodeDownSince", "EntryGuardDownSince", 0, 0 },
  { "EntryNodeUnlistedSince", "EntryGuardUnlistedSince", 0, 0 },
  { NULL, NULL, 0, 0},
};

/*XXXX these next two are duplicates or near-duplicates from config.c */
#define VAR(name,conftype,member,initvalue)                             \
  { name, CONFIG_TYPE_ ## conftype, STRUCT_OFFSET(or_state_t, member),  \
      initvalue }
/** As VAR, but the option name and member name are the same. */
#define V(member,conftype,initvalue)                                    \
  VAR(#member, conftype, member, initvalue)

/** Array of "state" variables saved to the ~/.tor/state file. */
static config_var_t state_vars_[] = {
  /* Remember to document these in state-contents.txt ! */

  V(AccountingBytesReadInInterval,    MEMUNIT,  NULL),
  V(AccountingBytesWrittenInInterval, MEMUNIT,  NULL),
  V(AccountingExpectedUsage,          MEMUNIT,  NULL),
  V(AccountingIntervalStart,          ISOTIME,  NULL),
  V(AccountingSecondsActive,          INTERVAL, NULL),
  V(AccountingSecondsToReachSoftLimit,INTERVAL, NULL),
  V(AccountingSoftLimitHitAt,         ISOTIME,  NULL),
  V(AccountingBytesAtSoftLimit,       MEMUNIT,  NULL),

  VAR("EntryGuard",              LINELIST_S,  EntryGuards,             NULL),
  VAR("EntryGuardDownSince",     LINELIST_S,  EntryGuards,             NULL),
  VAR("EntryGuardUnlistedSince", LINELIST_S,  EntryGuards,             NULL),
  VAR("EntryGuardAddedBy",       LINELIST_S,  EntryGuards,             NULL),
  VAR("EntryGuardPathBias",      LINELIST_S,  EntryGuards,             NULL),
  VAR("EntryGuardPathUseBias",   LINELIST_S,  EntryGuards,             NULL),
  V(EntryGuards,                 LINELIST_V,  NULL),

  VAR("TransportProxy",               LINELIST_S, TransportProxies, NULL),
  V(TransportProxies,                 LINELIST_V, NULL),

  V(BWHistoryReadEnds,                ISOTIME,  NULL),
  V(BWHistoryReadInterval,            UINT,     "900"),
  V(BWHistoryReadValues,              CSV,      ""),
  V(BWHistoryReadMaxima,              CSV,      ""),
  V(BWHistoryWriteEnds,               ISOTIME,  NULL),
  V(BWHistoryWriteInterval,           UINT,     "900"),
  V(BWHistoryWriteValues,             CSV,      ""),
  V(BWHistoryWriteMaxima,             CSV,      ""),
  V(BWHistoryDirReadEnds,             ISOTIME,  NULL),
  V(BWHistoryDirReadInterval,         UINT,     "900"),
  V(BWHistoryDirReadValues,           CSV,      ""),
  V(BWHistoryDirReadMaxima,           CSV,      ""),
  V(BWHistoryDirWriteEnds,            ISOTIME,  NULL),
  V(BWHistoryDirWriteInterval,        UINT,     "900"),
  V(BWHistoryDirWriteValues,          CSV,      ""),
  V(BWHistoryDirWriteMaxima,          CSV,      ""),

  V(TorVersion,                       STRING,   NULL),

  V(LastRotatedOnionKey,              ISOTIME,  NULL),
  V(LastWritten,                      ISOTIME,  NULL),

  V(TotalBuildTimes,                  UINT,     NULL),
  V(CircuitBuildAbandonedCount,       UINT,     "0"),
  VAR("CircuitBuildTimeBin",          LINELIST_S, BuildtimeHistogram, NULL),
  VAR("BuildtimeHistogram",           LINELIST_V, BuildtimeHistogram, NULL),
  { NULL, CONFIG_TYPE_OBSOLETE, 0, NULL }
};

#undef VAR
#undef V

static int or_state_validate(or_state_t *state, char **msg);

static int or_state_validate_cb(void *old_options, void *options,
                                void *default_options,
                                int from_setconf, char **msg);

/** Magic value for or_state_t. */
#define OR_STATE_MAGIC 0x57A73f57

/** "Extra" variable in the state that receives lines we can't parse. This
 * lets us preserve options from versions of Tor newer than us. */
static config_var_t state_extra_var = {
  "__extra", CONFIG_TYPE_LINELIST, STRUCT_OFFSET(or_state_t, ExtraLines), NULL
};

/** Configuration format for or_state_t. */
static const config_format_t state_format = {
  sizeof(or_state_t),
  OR_STATE_MAGIC,
  STRUCT_OFFSET(or_state_t, magic_),
  state_abbrevs_,
  state_vars_,
  or_state_validate_cb,
  &state_extra_var,
};

/** Persistent serialized state. */
static or_state_t *global_state = NULL;

/** Return the persistent state struct for this Tor. */
MOCK_IMPL(or_state_t *,
get_or_state, (void))
{
  tor_assert(global_state);
  return global_state;
}

/** Return true iff we have loaded the global state for this Tor */
int
or_state_loaded(void)
{
  return global_state != NULL;
}

/** Return true if <b>line</b> is a valid state TransportProxy line.
 *  Return false otherwise. */
static int
state_transport_line_is_valid(const char *line)
{
  smartlist_t *items = NULL;
  char *addrport=NULL;
  tor_addr_t addr;
  uint16_t port = 0;
  int r;

  items = smartlist_new();
  smartlist_split_string(items, line, NULL,
                         SPLIT_SKIP_SPACE|SPLIT_IGNORE_BLANK, -1);

  if (smartlist_len(items) != 2) {
    log_warn(LD_CONFIG, "state: Not enough arguments in TransportProxy line.");
    goto err;
  }

  addrport = smartlist_get(items, 1);
  if (tor_addr_port_lookup(addrport, &addr, &port) < 0) {
    log_warn(LD_CONFIG, "state: Could not parse addrport.");
    goto err;
  }

  if (!port) {
    log_warn(LD_CONFIG, "state: Transport line did not contain port.");
    goto err;
  }

  r = 1;
  goto done;

 err:
  r = 0;

 done:
  SMARTLIST_FOREACH(items, char*, s, tor_free(s));
  smartlist_free(items);
  return r;
}

/** Return 0 if all TransportProxy lines in <b>state</b> are well
 *  formed. Otherwise, return -1. */
static int
validate_transports_in_state(or_state_t *state)
{
  int broken = 0;
  config_line_t *line;

  for (line = state->TransportProxies ; line ; line = line->next) {
    tor_assert(!strcmp(line->key, "TransportProxy"));
    if (!state_transport_line_is_valid(line->value))
      broken = 1;
  }

  if (broken)
    log_warn(LD_CONFIG, "state: State file seems to be broken.");

  return 0;
}

static int
or_state_validate_cb(void *old_state, void *state, void *default_state,
                     int from_setconf, char **msg)
{
  /* We don't use these; only options do. Still, we need to match that
   * signature. */
  (void) from_setconf;
  (void) default_state;
  (void) old_state;

  return or_state_validate(state, msg);
}

/** Return 0 if every setting in <b>state</b> is reasonable, and a
 * permissible transition from <b>old_state</b>.  Else warn and return -1.
 * Should have no side effects, except for normalizing the contents of
 * <b>state</b>.
 */
static int
or_state_validate(or_state_t *state, char **msg)
{
  if (entry_guards_parse_state(state, 0, msg)<0)
    return -1;

  if (validate_transports_in_state(state)<0)
    return -1;

  return 0;
}

/** Replace the current persistent state with <b>new_state</b> */
static int
or_state_set(or_state_t *new_state)
{
  char *err = NULL;
  int ret = 0;
  tor_assert(new_state);
  config_free(&state_format, global_state);
  global_state = new_state;
  if (entry_guards_parse_state(global_state, 1, &err)<0) {
    log_warn(LD_GENERAL,"%s",err);
    tor_free(err);
    ret = -1;
  }
  if (rep_hist_load_state(global_state, &err)<0) {
    log_warn(LD_GENERAL,"Unparseable bandwidth history state: %s",err);
    tor_free(err);
    ret = -1;
  }
  if (circuit_build_times_parse_state(
      get_circuit_build_times_mutable(),global_state) < 0) {
    ret = -1;
  }
  return ret;
}

/**
 * Save a broken state file to a backup location.
 */
static void
or_state_save_broken(char *fname)
{
  int i, res;
  file_status_t status;
  char *fname2 = NULL;
  for (i = 0; i < 100; ++i) {
    tor_asprintf(&fname2, "%s.%d", fname, i);
    status = file_status(fname2);
    if (status == FN_NOENT)
      break;
    tor_free(fname2);
  }
  if (i == 100) {
    log_warn(LD_BUG, "Unable to parse state in \"%s\"; too many saved bad "
             "state files to move aside. Discarding the old state file.",
             fname);
    res = unlink(fname);
    if (res != 0) {
      log_warn(LD_FS,
               "Also couldn't discard old state file \"%s\" because "
               "unlink() failed: %s",
               fname, strerror(errno));
    }
  } else {
    log_warn(LD_BUG, "Unable to parse state in \"%s\". Moving it aside "
             "to \"%s\".  This could be a bug in Tor; please tell "
             "the developers.", fname, fname2);
    if (tor_rename(fname, fname2) < 0) {//XXXX sandbox prohibits
      log_warn(LD_BUG, "Weirdly, I couldn't even move the state aside. The "
               "OS gave an error of %s", strerror(errno));
    }
  }
  tor_free(fname2);
}

STATIC or_state_t *
or_state_new(void)
{
  or_state_t *new_state = tor_malloc_zero(sizeof(or_state_t));
  new_state->magic_ = OR_STATE_MAGIC;
  config_init(&state_format, new_state);

  return new_state;
}

/** Reload the persistent state from disk, generating a new state as needed.
 * Return 0 on success, less than 0 on failure.
 */
int
or_state_load(void)
{
  or_state_t *new_state = NULL;
  char *contents = NULL, *fname;
  char *errmsg = NULL;
  int r = -1, badstate = 0;

  fname = get_datadir_fname("state");
  switch (file_status(fname)) {
    case FN_FILE:
      if (!(contents = read_file_to_str(fname, 0, NULL))) {
        log_warn(LD_FS, "Unable to read state file \"%s\"", fname);
        goto done;
      }
      break;
    case FN_NOENT:
      break;
    case FN_ERROR:
    case FN_DIR:
    default:
      log_warn(LD_GENERAL,"State file \"%s\" is not a file? Failing.", fname);
      goto done;
  }
  new_state = or_state_new();
  if (contents) {
    config_line_t *lines=NULL;
    int assign_retval;
    if (config_get_lines(contents, &lines, 0)<0)
      goto done;
    assign_retval = config_assign(&state_format, new_state,
                                  lines, 0, 0, &errmsg);
    config_free_lines(lines);
    if (assign_retval<0)
      badstate = 1;
    if (errmsg) {
      log_warn(LD_GENERAL, "%s", errmsg);
      tor_free(errmsg);
    }
  }

  if (!badstate && or_state_validate(new_state, &errmsg) < 0)
    badstate = 1;

  if (errmsg) {
    log_warn(LD_GENERAL, "%s", errmsg);
    tor_free(errmsg);
  }

  if (badstate && !contents) {
    log_warn(LD_BUG, "Uh oh.  We couldn't even validate our own default state."
             " This is a bug in Tor.");
    goto done;
  } else if (badstate && contents) {
    or_state_save_broken(fname);

    tor_free(contents);
    config_free(&state_format, new_state);

    new_state = or_state_new();
  } else if (contents) {
    log_info(LD_GENERAL, "Loaded state from \"%s\"", fname);
  } else {
    log_info(LD_GENERAL, "Initialized state");
  }
  if (or_state_set(new_state) == -1) {
    or_state_save_broken(fname);
  }
  new_state = NULL;
  if (!contents) {
    global_state->next_write = 0;
    or_state_save(time(NULL));
  }
  r = 0;

 done:
  tor_free(fname);
  tor_free(contents);
  if (new_state)
    config_free(&state_format, new_state);

  return r;
}

/** Did the last time we tried to write the state file fail? If so, we
 * should consider disabling such features as preemptive circuit generation
 * to compute circuit-build-time. */
static int last_state_file_write_failed = 0;

/** Return whether the state file failed to write last time we tried. */
int
did_last_state_file_write_fail(void)
{
  return last_state_file_write_failed;
}

/** If writing the state to disk fails, try again after this many seconds. */
#define STATE_WRITE_RETRY_INTERVAL 3600

/** If we're a relay, how often should we checkpoint our state file even
 * if nothing else dirties it? This will checkpoint ongoing stats like
 * bandwidth used, per-country user stats, etc. */
#define STATE_RELAY_CHECKPOINT_INTERVAL (12*60*60)

/** Write the persistent state to disk. Return 0 for success, <0 on failure. */
int
or_state_save(time_t now)
{
  char *state, *contents;
  char tbuf[ISO_TIME_LEN+1];
  char *fname;

  tor_assert(global_state);

  if (global_state->next_write > now)
    return 0;

  /* Call everything else that might dirty the state even more, in order
   * to avoid redundant writes. */
  entry_guards_update_state(global_state);
  rep_hist_update_state(global_state);
  circuit_build_times_update_state(get_circuit_build_times(), global_state);
  if (accounting_is_enabled(get_options()))
    accounting_run_housekeeping(now);

  global_state->LastWritten = now;

  tor_free(global_state->TorVersion);
  tor_asprintf(&global_state->TorVersion, "Tor %s", get_version());

  state = config_dump(&state_format, NULL, global_state, 1, 0);
  format_local_iso_time(tbuf, now);
  tor_asprintf(&contents,
               "# Tor state file last generated on %s local time\n"
               "# Other times below are in UTC\n"
               "# You *do not* need to edit this file.\n\n%s",
               tbuf, state);
  tor_free(state);
  fname = get_datadir_fname("state");
  if (write_str_to_file(fname, contents, 0)<0) {
    log_warn(LD_FS, "Unable to write state to file \"%s\"; "
             "will try again later", fname);
    last_state_file_write_failed = 1;
    tor_free(fname);
    tor_free(contents);
    /* Try again after STATE_WRITE_RETRY_INTERVAL (or sooner, if the state
     * changes sooner). */
    global_state->next_write = now + STATE_WRITE_RETRY_INTERVAL;
    return -1;
  }

  last_state_file_write_failed = 0;
  log_info(LD_GENERAL, "Saved state to \"%s\"", fname);
  tor_free(fname);
  tor_free(contents);

  if (server_mode(get_options()))
    global_state->next_write = now + STATE_RELAY_CHECKPOINT_INTERVAL;
  else
    global_state->next_write = TIME_MAX;

  return 0;
}

/** Return the config line for transport <b>transport</b> in the current state.
 *  Return NULL if there is no config line for <b>transport</b>. */
STATIC config_line_t *
get_transport_in_state_by_name(const char *transport)
{
  or_state_t *or_state = get_or_state();
  config_line_t *line;
  config_line_t *ret = NULL;
  smartlist_t *items = NULL;

  for (line = or_state->TransportProxies ; line ; line = line->next) {
    tor_assert(!strcmp(line->key, "TransportProxy"));

    items = smartlist_new();
    smartlist_split_string(items, line->value, NULL,
                           SPLIT_SKIP_SPACE|SPLIT_IGNORE_BLANK, -1);
    if (smartlist_len(items) != 2) /* broken state */
      goto done;

    if (!strcmp(smartlist_get(items, 0), transport)) {
      ret = line;
      goto done;
    }

    SMARTLIST_FOREACH(items, char*, s, tor_free(s));
    smartlist_free(items);
    items = NULL;
  }

 done:
  if (items) {
    SMARTLIST_FOREACH(items, char*, s, tor_free(s));
    smartlist_free(items);
  }
  return ret;
}

/** Return string containing the address:port part of the
 *  TransportProxy <b>line</b> for transport <b>transport</b>.
 *  If the line is corrupted, return NULL. */
static const char *
get_transport_bindaddr(const char *line, const char *transport)
{
  char *line_tmp = NULL;

  if (strlen(line) < strlen(transport) + 2) {
    goto broken_state;
  } else {
    /* line should start with the name of the transport and a space.
       (for example, "obfs2 127.0.0.1:47245") */
    tor_asprintf(&line_tmp, "%s ", transport);
    if (strcmpstart(line, line_tmp))
      goto broken_state;

    tor_free(line_tmp);
    return (line+strlen(transport)+1);
  }

 broken_state:
  tor_free(line_tmp);
  return NULL;
}

/** Return a string containing the address:port that a proxy transport
 *  should bind on. The string is stored on the heap and must be freed
 *  by the caller of this function. */
char *
get_stored_bindaddr_for_server_transport(const char *transport)
{
  char *default_addrport = NULL;
  const char *stored_bindaddr = NULL;
  config_line_t *line = NULL;

  {
    /* See if the user explicitly asked for a specific listening
       address for this transport. */
    char *conf_bindaddr = get_transport_bindaddr_from_config(transport);
    if (conf_bindaddr)
      return conf_bindaddr;
  }

  line = get_transport_in_state_by_name(transport);
  if (!line) /* Found no references in state for this transport. */
    goto no_bindaddr_found;

  stored_bindaddr = get_transport_bindaddr(line->value, transport);
  if (stored_bindaddr) /* found stored bindaddr in state file. */
    return tor_strdup(stored_bindaddr);

 no_bindaddr_found:
  /** If we didn't find references for this pluggable transport in the
      state file, we should instruct the pluggable transport proxy to
      listen on INADDR_ANY on a random ephemeral port. */
  tor_asprintf(&default_addrport, "%s:%s", fmt_addr32(INADDR_ANY), "0");
  return default_addrport;
}

/** Save <b>transport</b> listening on <b>addr</b>:<b>port</b> to
    state */
void
save_transport_to_state(const char *transport,
                        const tor_addr_t *addr, uint16_t port)
{
  or_state_t *state = get_or_state();

  char *transport_addrport=NULL;

  /** find where to write on the state */
  config_line_t **next, *line;

  /* see if this transport is already stored in state */
  config_line_t *transport_line =
    get_transport_in_state_by_name(transport);

  if (transport_line) { /* if transport already exists in state... */
    const char *prev_bindaddr = /* get its addrport... */
      get_transport_bindaddr(transport_line->value, transport);
    transport_addrport = tor_strdup(fmt_addrport(addr, port));

    /* if transport in state has the same address as this one, life is good */
    if (!strcmp(prev_bindaddr, transport_addrport)) {
      log_info(LD_CONFIG, "Transport seems to have spawned on its usual "
               "address:port.");
      goto done;
    } else { /* if addrport in state is different than the one we got */
      log_info(LD_CONFIG, "Transport seems to have spawned on different "
               "address:port. Let's update the state file with the new "
               "address:port");
      tor_free(transport_line->value); /* free the old line */
      /* replace old addrport line with new line */
      tor_asprintf(&transport_line->value, "%s %s", transport,
                   fmt_addrport(addr, port));
    }
  } else { /* never seen this one before; save it in state for next time */
    log_info(LD_CONFIG, "It's the first time we see this transport. "
             "Let's save its address:port");
    next = &state->TransportProxies;
    /* find the last TransportProxy line in the state and point 'next'
       right after it  */
    line = state->TransportProxies;
    while (line) {
      next = &(line->next);
      line = line->next;
    }

    /* allocate space for the new line and fill it in */
    *next = line = tor_malloc_zero(sizeof(config_line_t));
    line->key = tor_strdup("TransportProxy");
    tor_asprintf(&line->value, "%s %s", transport, fmt_addrport(addr, port));

    next = &(line->next);
  }

  if (!get_options()->AvoidDiskWrites)
    or_state_mark_dirty(state, 0);

 done:
  tor_free(transport_addrport);
}

STATIC void
or_state_free(or_state_t *state)
{
  if (!state)
    return;

  config_free(&state_format, state);
}

void
or_state_free_all(void)
{
  or_state_free(global_state);
  global_state = NULL;
}

