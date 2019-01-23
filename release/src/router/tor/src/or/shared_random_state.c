/* Copyright (c) 2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file shared_random_state.c
 *
 * \brief Functions and data structures for the state of the random protocol
 *        as defined in proposal #250.
 **/

#define SHARED_RANDOM_STATE_PRIVATE

#include "or.h"
#include "shared_random.h"
#include "config.h"
#include "confparse.h"
#include "dirvote.h"
#include "networkstatus.h"
#include "router.h"
#include "shared_random_state.h"

/* Default filename of the shared random state on disk. */
static const char default_fname[] = "sr-state";

/* String representation of a protocol phase. */
static const char *phase_str[] = { "unknown", "commit", "reveal" };

/* Our shared random protocol state. There is only one possible state per
 * protocol run so this is the global state which is reset at every run once
 * the shared random value has been computed. */
static sr_state_t *sr_state = NULL;

/* Representation of our persistent state on disk. The sr_state above
 * contains the data parsed from this state. When we save to disk, we
 * translate the sr_state to this sr_disk_state. */
static sr_disk_state_t *sr_disk_state = NULL;

/* Disk state file keys. */
static const char dstate_commit_key[] = "Commit";
static const char dstate_prev_srv_key[] = "SharedRandPreviousValue";
static const char dstate_cur_srv_key[] = "SharedRandCurrentValue";

/* These next two are duplicates or near-duplicates from config.c */
#define VAR(name, conftype, member, initvalue)                              \
  { name, CONFIG_TYPE_ ## conftype, STRUCT_OFFSET(sr_disk_state_t, member), \
    initvalue }
/* As VAR, but the option name and member name are the same. */
#define V(member, conftype, initvalue) \
  VAR(#member, conftype, member, initvalue)
/* Our persistent state magic number. */
#define SR_DISK_STATE_MAGIC 0x98AB1254
/* Each protocol phase has 12 rounds  */
#define SHARED_RANDOM_N_ROUNDS 12
/* Number of phase we have in a protocol. */
#define SHARED_RANDOM_N_PHASES 2

static int
disk_state_validate_cb(void *old_state, void *state, void *default_state,
                       int from_setconf, char **msg);

/* Array of variables that are saved to disk as a persistent state. */
static config_var_t state_vars[] = {
  V(Version,                    UINT, "0"),
  V(TorVersion,                 STRING, NULL),
  V(ValidAfter,                 ISOTIME, NULL),
  V(ValidUntil,                 ISOTIME, NULL),

  V(Commit,                     LINELIST, NULL),

  V(SharedRandValues,           LINELIST_V, NULL),
  VAR("SharedRandPreviousValue",LINELIST_S, SharedRandValues, NULL),
  VAR("SharedRandCurrentValue", LINELIST_S, SharedRandValues, NULL),
  { NULL, CONFIG_TYPE_OBSOLETE, 0, NULL }
};

/* "Extra" variable in the state that receives lines we can't parse. This
 * lets us preserve options from versions of Tor newer than us. */
static config_var_t state_extra_var = {
  "__extra", CONFIG_TYPE_LINELIST,
  STRUCT_OFFSET(sr_disk_state_t, ExtraLines), NULL
};

/* Configuration format of sr_disk_state_t. */
static const config_format_t state_format = {
  sizeof(sr_disk_state_t),
  SR_DISK_STATE_MAGIC,
  STRUCT_OFFSET(sr_disk_state_t, magic_),
  NULL,
  NULL,
  state_vars,
  disk_state_validate_cb,
  &state_extra_var,
};

/* Return a string representation of a protocol phase. */
STATIC const char *
get_phase_str(sr_phase_t phase)
{
  const char *the_string = NULL;

  switch (phase) {
  case SR_PHASE_COMMIT:
  case SR_PHASE_REVEAL:
    the_string = phase_str[phase];
    break;
  default:
    /* Unknown phase shouldn't be possible. */
    tor_assert(0);
  }

  return the_string;
}

/* Return the voting interval of the tor vote subsystem. */
static int
get_voting_interval(void)
{
  int interval;
  networkstatus_t *consensus = networkstatus_get_live_consensus(time(NULL));

  if (consensus) {
    interval = (int)(consensus->fresh_until - consensus->valid_after);
  } else {
    /* Same for both a testing and real network. We voluntarily ignore the
     * InitialVotingInterval since it complexifies things and it doesn't
     * affect the SR protocol. */
    interval = get_options()->V3AuthVotingInterval;
  }
  tor_assert(interval > 0);
  return interval;
}

/* Given the time <b>now</b>, return the start time of the current round of
 * the SR protocol. For example, if it's 23:47:08, the current round thus
 * started at 23:47:00 for a voting interval of 10 seconds. */
static time_t
get_start_time_of_current_round(time_t now)
{
  const or_options_t *options = get_options();
  int voting_interval = get_voting_interval();
  voting_schedule_t *new_voting_schedule =
    get_voting_schedule(options, now, LOG_INFO);
  tor_assert(new_voting_schedule);

  /* First, get the start time of the next round */
  time_t next_start = new_voting_schedule->interval_starts;
  /* Now roll back next_start by a voting interval to find the start time of
     the current round. */
  time_t curr_start = dirvote_get_start_of_next_interval(
                                     next_start - voting_interval - 1,
                                     voting_interval,
                                     options->TestingV3AuthVotingStartOffset);

  voting_schedule_free(new_voting_schedule);

  return curr_start;
}

/* Return the time we should expire the state file created at <b>now</b>.
 * We expire the state file in the beginning of the next protocol run. */
STATIC time_t
get_state_valid_until_time(time_t now)
{
  int total_rounds = SHARED_RANDOM_N_ROUNDS * SHARED_RANDOM_N_PHASES;
  int current_round, voting_interval, rounds_left;
  time_t valid_until, beginning_of_current_round;

  voting_interval = get_voting_interval();
  /* Find the time the current round started. */
  beginning_of_current_round = get_start_time_of_current_round(now);

  /* Find how many rounds are left till the end of the protocol run */
  current_round = (now / voting_interval) % total_rounds;
  rounds_left = total_rounds - current_round;

  /* To find the valid-until time now, take the start time of the current
   * round and add to it the time it takes for the leftover rounds to
   * complete. */
  valid_until = beginning_of_current_round + (rounds_left * voting_interval);

  { /* Logging */
    char tbuf[ISO_TIME_LEN + 1];
    format_iso_time(tbuf, valid_until);
    log_debug(LD_DIR, "SR: Valid until time for state set to %s.", tbuf);
  }

  return valid_until;
}

/* Given the consensus 'valid-after' time, return the protocol phase we should
 * be in. */
STATIC sr_phase_t
get_sr_protocol_phase(time_t valid_after)
{
  /* Shared random protocol has two phases, commit and reveal. */
  int total_periods = SHARED_RANDOM_N_ROUNDS * SHARED_RANDOM_N_PHASES;
  int current_slot;

  /* Split time into slots of size 'voting_interval'. See which slot we are
   * currently into, and find which phase it corresponds to. */
  current_slot = (valid_after / get_voting_interval()) % total_periods;

  if (current_slot < SHARED_RANDOM_N_ROUNDS) {
    return SR_PHASE_COMMIT;
  } else {
    return SR_PHASE_REVEAL;
  }
}

/* Add the given <b>commit</b> to <b>state</b>. It MUST be a valid commit
 * and there shouldn't be a commit from the same authority in the state
 * already else verification hasn't been done prior. This takes ownership of
 * the commit once in our state. */
static void
commit_add_to_state(sr_commit_t *commit, sr_state_t *state)
{
  sr_commit_t *saved_commit;

  tor_assert(commit);
  tor_assert(state);

  saved_commit = digestmap_set(state->commits, commit->rsa_identity,
                               commit);
  if (saved_commit != NULL) {
    /* This means we already have that commit in our state so adding twice
     * the same commit is either a code flow error, a corrupted disk state
     * or some new unknown issue. */
    log_warn(LD_DIR, "SR: Commit from %s exists in our state while "
                     "adding it: '%s'", sr_commit_get_rsa_fpr(commit),
                     commit->encoded_commit);
    sr_commit_free(saved_commit);
  }
}

/* Helper: deallocate a commit object. (Used with digestmap_free(), which
 * requires a function pointer whose argument is void *). */
static void
commit_free_(void *p)
{
  sr_commit_free(p);
}

/* Free a state that was allocated with state_new(). */
static void
state_free(sr_state_t *state)
{
  if (state == NULL) {
    return;
  }
  tor_free(state->fname);
  digestmap_free(state->commits, commit_free_);
  tor_free(state->current_srv);
  tor_free(state->previous_srv);
  tor_free(state);
}

/* Allocate an sr_state_t object and returns it. If no <b>fname</b>, the
 * default file name is used. This function does NOT initialize the state
 * timestamp, phase or shared random value. NULL is never returned. */
static sr_state_t *
state_new(const char *fname, time_t now)
{
  sr_state_t *new_state = tor_malloc_zero(sizeof(*new_state));
  /* If file name is not provided, use default. */
  if (fname == NULL) {
    fname = default_fname;
  }
  new_state->fname = tor_strdup(fname);
  new_state->version = SR_PROTO_VERSION;
  new_state->commits = digestmap_new();
  new_state->phase = get_sr_protocol_phase(now);
  new_state->valid_until = get_state_valid_until_time(now);
  return new_state;
}

/* Set our global state pointer with the one given. */
static void
state_set(sr_state_t *state)
{
  tor_assert(state);
  if (sr_state != NULL) {
    state_free(sr_state);
  }
  sr_state = state;
}

/* Free an allocated disk state. */
static void
disk_state_free(sr_disk_state_t *state)
{
  if (state == NULL) {
    return;
  }
  config_free(&state_format, state);
}

/* Allocate a new disk state, initialize it and return it. */
static sr_disk_state_t *
disk_state_new(time_t now)
{
  sr_disk_state_t *new_state = tor_malloc_zero(sizeof(*new_state));

  new_state->magic_ = SR_DISK_STATE_MAGIC;
  new_state->Version = SR_PROTO_VERSION;
  new_state->TorVersion = tor_strdup(get_version());
  new_state->ValidUntil = get_state_valid_until_time(now);
  new_state->ValidAfter = now;

  /* Init config format. */
  config_init(&state_format, new_state);
  return new_state;
}

/* Set our global disk state with the given state. */
static void
disk_state_set(sr_disk_state_t *state)
{
  tor_assert(state);
  if (sr_disk_state != NULL) {
    disk_state_free(sr_disk_state);
  }
  sr_disk_state = state;
}

/* Return -1 if the disk state is invalid (something in there that we can't or
 * shouldn't use). Return 0 if everything checks out. */
static int
disk_state_validate(const sr_disk_state_t *state)
{
  time_t now;

  tor_assert(state);

  /* Do we support the protocol version in the state or is it 0 meaning
   * Version wasn't found in the state file or bad anyway ? */
  if (state->Version == 0 || state->Version > SR_PROTO_VERSION) {
    goto invalid;
  }

  /* If the valid until time is before now, we shouldn't use that state. */
  now = time(NULL);
  if (state->ValidUntil < now) {
    log_info(LD_DIR, "SR: Disk state has expired. Ignoring it.");
    goto invalid;
  }

  /* Make sure we don't have a valid after time that is earlier than a valid
   * until time which would make things not work well. */
  if (state->ValidAfter >= state->ValidUntil) {
    log_info(LD_DIR, "SR: Disk state valid after/until times are invalid.");
    goto invalid;
  }

  return 0;

 invalid:
  return -1;
}

/* Validate the disk state (NOP for now). */
static int
disk_state_validate_cb(void *old_state, void *state, void *default_state,
                       int from_setconf, char **msg)
{
  /* We don't use these; only options do. */
  (void) from_setconf;
  (void) default_state;
  (void) old_state;

  /* This is called by config_dump which is just before we are about to
   * write it to disk. At that point, our global memory state has been
   * copied to the disk state so it's fair to assume it's trustable. */
  (void) state;
  (void) msg;
  return 0;
}

/* Parse the Commit line(s) in the disk state and translate them to the
 * the memory state. Return 0 on success else -1 on error. */
static int
disk_state_parse_commits(sr_state_t *state,
                         const sr_disk_state_t *disk_state)
{
  config_line_t *line;
  smartlist_t *args = NULL;

  tor_assert(state);
  tor_assert(disk_state);

  for (line = disk_state->Commit; line; line = line->next) {
    sr_commit_t *commit = NULL;

    /* Extra safety. */
    if (strcasecmp(line->key, dstate_commit_key) ||
        line->value == NULL) {
      /* Ignore any lines that are not commits. */
      tor_fragile_assert();
      continue;
    }
    args = smartlist_new();
    smartlist_split_string(args, line->value, " ",
                           SPLIT_SKIP_SPACE|SPLIT_IGNORE_BLANK, 0);
    if (smartlist_len(args) < 3) {
      log_warn(LD_BUG, "SR: Too few arguments in Commit Line: %s",
               escaped(line->value));
      goto error;
    }
    commit = sr_parse_commit(args);
    if (commit == NULL) {
      /* Ignore badly formed commit. It could also be a authority
       * fingerprint that we don't know about so it shouldn't be used. */
      continue;
    }
    /* We consider parseable commit from our disk state to be valid because
     * they need to be in the first place to get in there. */
    commit->valid = 1;
    /* Add commit to our state pointer. */
    commit_add_to_state(commit, state);

    SMARTLIST_FOREACH(args, char *, cp, tor_free(cp));
    smartlist_free(args);
  }

  return 0;

 error:
  SMARTLIST_FOREACH(args, char *, cp, tor_free(cp));
  smartlist_free(args);
  return -1;
}

/* Parse a share random value line from the disk state and save it to dst
 * which is an allocated srv object. Return 0 on success else -1. */
static int
disk_state_parse_srv(const char *value, sr_srv_t *dst)
{
  int ret = -1;
  smartlist_t *args;
  sr_srv_t *srv;

  tor_assert(value);
  tor_assert(dst);

  args = smartlist_new();
  smartlist_split_string(args, value, " ",
                         SPLIT_SKIP_SPACE|SPLIT_IGNORE_BLANK, 0);
  if (smartlist_len(args) < 2) {
    log_warn(LD_BUG, "SR: Too few arguments in shared random value. "
             "Line: %s", escaped(value));
    goto error;
  }
  srv = sr_parse_srv(args);
  if (srv == NULL) {
    goto error;
  }
  dst->num_reveals = srv->num_reveals;
  memcpy(dst->value, srv->value, sizeof(dst->value));
  tor_free(srv);
  ret = 0;

 error:
  SMARTLIST_FOREACH(args, char *, s, tor_free(s));
  smartlist_free(args);
  return ret;
}

/* Parse both SharedRandCurrentValue and SharedRandPreviousValue line from
 * the state. Return 0 on success else -1. */
static int
disk_state_parse_sr_values(sr_state_t *state,
                           const sr_disk_state_t *disk_state)
{
  /* Only one value per type (current or previous) is allowed so we keep
   * track of it with these flag. */
  unsigned int seen_previous = 0, seen_current = 0;
  config_line_t *line;
  sr_srv_t *srv = NULL;

  tor_assert(state);
  tor_assert(disk_state);

  for (line = disk_state->SharedRandValues; line; line = line->next) {
    if (line->value == NULL) {
      continue;
    }
    srv = tor_malloc_zero(sizeof(*srv));
    if (disk_state_parse_srv(line->value, srv) < 0) {
      log_warn(LD_BUG, "SR: Broken current SRV line in state %s",
               escaped(line->value));
      goto bad;
    }
    if (!strcasecmp(line->key, dstate_prev_srv_key)) {
      if (seen_previous) {
        log_warn(LD_DIR, "SR: Second previous SRV value seen. Bad state");
        goto bad;
      }
      state->previous_srv = srv;
      seen_previous = 1;
    } else if (!strcasecmp(line->key, dstate_cur_srv_key)) {
      if (seen_current) {
        log_warn(LD_DIR, "SR: Second current SRV value seen. Bad state");
        goto bad;
      }
      state->current_srv = srv;
      seen_current = 1;
    } else {
      /* Unknown key. Ignoring. */
      tor_free(srv);
    }
  }

  return 0;
 bad:
  tor_free(srv);
  return -1;
}

/* Parse the given disk state and set a newly allocated state. On success,
 * return that state else NULL. */
static sr_state_t *
disk_state_parse(const sr_disk_state_t *new_disk_state)
{
  sr_state_t *new_state = state_new(default_fname, time(NULL));

  tor_assert(new_disk_state);

  new_state->version = new_disk_state->Version;
  new_state->valid_until = new_disk_state->ValidUntil;
  new_state->valid_after = new_disk_state->ValidAfter;

  /* Set our current phase according to the valid-after time in our disk
   * state. The disk state we are parsing contains everything for the phase
   * starting at valid_after so make sure our phase reflects that. */
  new_state->phase = get_sr_protocol_phase(new_state->valid_after);

  /* Parse the shared random values. */
  if (disk_state_parse_sr_values(new_state, new_disk_state) < 0) {
    goto error;
  }
  /* Parse the commits. */
  if (disk_state_parse_commits(new_state, new_disk_state) < 0) {
    goto error;
  }
  /* Great! This new state contains everything we had on disk. */
  return new_state;

 error:
  state_free(new_state);
  return NULL;
}

/* From a valid commit object and an allocated config line, set the line's
 * value to the state string representation of a commit. */
static void
disk_state_put_commit_line(const sr_commit_t *commit, config_line_t *line)
{
  char *reveal_str = NULL;

  tor_assert(commit);
  tor_assert(line);

  if (!tor_mem_is_zero(commit->encoded_reveal,
                       sizeof(commit->encoded_reveal))) {
    /* Add extra whitespace so we can format the line correctly. */
    tor_asprintf(&reveal_str, " %s", commit->encoded_reveal);
  }
  tor_asprintf(&line->value, "%u %s %s %s%s",
               SR_PROTO_VERSION,
               crypto_digest_algorithm_get_name(commit->alg),
               sr_commit_get_rsa_fpr(commit),
               commit->encoded_commit,
               reveal_str != NULL ? reveal_str : "");
  if (reveal_str != NULL) {
    memwipe(reveal_str, 0, strlen(reveal_str));
    tor_free(reveal_str);
  }
}

/* From a valid srv object and an allocated config line, set the line's
 * value to the state string representation of a shared random value. */
static void
disk_state_put_srv_line(const sr_srv_t *srv, config_line_t *line)
{
  char encoded[SR_SRV_VALUE_BASE64_LEN + 1];

  tor_assert(line);

  /* No SRV value thus don't add the line. This is possible since we might
   * not have a current or previous SRV value in our state. */
  if (srv == NULL) {
    return;
  }
  sr_srv_encode(encoded, sizeof(encoded), srv);
  tor_asprintf(&line->value, "%" PRIu64 " %s", srv->num_reveals, encoded);
}

/* Reset disk state that is free allocated memory and zeroed the object. */
static void
disk_state_reset(void)
{
  /* Free allocated memory */
  config_free_lines(sr_disk_state->Commit);
  config_free_lines(sr_disk_state->SharedRandValues);
  config_free_lines(sr_disk_state->ExtraLines);
  tor_free(sr_disk_state->TorVersion);

  /* Clean up the struct */
  memset(sr_disk_state, 0, sizeof(*sr_disk_state));

  /* Reset it with useful data */
  sr_disk_state->magic_ = SR_DISK_STATE_MAGIC;
  sr_disk_state->TorVersion = tor_strdup(get_version());
}

/* Update our disk state based on our global SR state. */
static void
disk_state_update(void)
{
  config_line_t **next, *line;

  tor_assert(sr_disk_state);
  tor_assert(sr_state);

  /* Reset current disk state. */
  disk_state_reset();

  /* First, update elements that we don't need to do a construction. */
  sr_disk_state->Version = sr_state->version;
  sr_disk_state->ValidUntil = sr_state->valid_until;
  sr_disk_state->ValidAfter = sr_state->valid_after;

  /* Shared random values. */
  next = &sr_disk_state->SharedRandValues;
  if (sr_state->previous_srv != NULL) {
    *next = line = tor_malloc_zero(sizeof(config_line_t));
    line->key = tor_strdup(dstate_prev_srv_key);
    disk_state_put_srv_line(sr_state->previous_srv, line);
    /* Go to the next shared random value. */
    next = &(line->next);
  }
  if (sr_state->current_srv != NULL) {
    *next = line = tor_malloc_zero(sizeof(*line));
    line->key = tor_strdup(dstate_cur_srv_key);
    disk_state_put_srv_line(sr_state->current_srv, line);
  }

  /* Parse the commits and construct config line(s). */
  next = &sr_disk_state->Commit;
  DIGESTMAP_FOREACH(sr_state->commits, key, sr_commit_t *, commit) {
    *next = line = tor_malloc_zero(sizeof(*line));
    line->key = tor_strdup(dstate_commit_key);
    disk_state_put_commit_line(commit, line);
    next = &(line->next);
  } DIGESTMAP_FOREACH_END;
}

/* Load state from disk and put it into our disk state. If the state passes
 * validation, our global state will be updated with it. Return 0 on
 * success. On error, -EINVAL is returned if the state on disk did contained
 * something malformed or is unreadable. -ENOENT is returned indicating that
 * the state file is either empty of non existing. */
static int
disk_state_load_from_disk(void)
{
  int ret;
  char *fname;

  fname = get_datadir_fname(default_fname);
  ret = disk_state_load_from_disk_impl(fname);
  tor_free(fname);

  return ret;
}

/* Helper for disk_state_load_from_disk(). */
STATIC int
disk_state_load_from_disk_impl(const char *fname)
{
  int ret;
  char *content = NULL;
  sr_state_t *parsed_state = NULL;
  sr_disk_state_t *disk_state = NULL;

  /* Read content of file so we can parse it. */
  if ((content = read_file_to_str(fname, 0, NULL)) == NULL) {
    log_warn(LD_FS, "SR: Unable to read SR state file %s",
             escaped(fname));
    ret = -errno;
    goto error;
  }

  {
    config_line_t *lines = NULL;
    char *errmsg = NULL;

    /* Every error in this code path will return EINVAL. */
    ret = -EINVAL;
    if (config_get_lines(content, &lines, 0) < 0) {
      config_free_lines(lines);
      goto error;
    }

    disk_state = disk_state_new(time(NULL));
    config_assign(&state_format, disk_state, lines, 0, &errmsg);
    config_free_lines(lines);
    if (errmsg) {
      log_warn(LD_DIR, "SR: Reading state error: %s", errmsg);
      tor_free(errmsg);
      goto error;
    }
  }

  /* So far so good, we've loaded our state file into our disk state. Let's
   * validate it and then parse it. */
  if (disk_state_validate(disk_state) < 0) {
    ret = -EINVAL;
    goto error;
  }

  parsed_state = disk_state_parse(disk_state);
  if (parsed_state == NULL) {
    ret = -EINVAL;
    goto error;
  }
  state_set(parsed_state);
  disk_state_set(disk_state);
  tor_free(content);
  log_info(LD_DIR, "SR: State loaded successfully from file %s", fname);
  return 0;

 error:
  disk_state_free(disk_state);
  tor_free(content);
  return ret;
}

/* Save the disk state to disk but before that update it from the current
 * state so we always have the latest. Return 0 on success else -1. */
static int
disk_state_save_to_disk(void)
{
  int ret;
  char *state, *content = NULL, *fname = NULL;
  char tbuf[ISO_TIME_LEN + 1];
  time_t now = time(NULL);

  /* If we didn't have the opportunity to setup an internal disk state,
   * don't bother saving something to disk. */
  if (sr_disk_state == NULL) {
    ret = 0;
    goto done;
  }

  /* Make sure that our disk state is up to date with our memory state
   * before saving it to disk. */
  disk_state_update();
  state = config_dump(&state_format, NULL, sr_disk_state, 0, 0);
  format_local_iso_time(tbuf, now);
  tor_asprintf(&content,
               "# Tor shared random state file last generated on %s "
               "local time\n"
               "# Other times below are in UTC\n"
               "# Please *do not* edit this file.\n\n%s",
               tbuf, state);
  tor_free(state);
  fname = get_datadir_fname(default_fname);
  if (write_str_to_file(fname, content, 0) < 0) {
    log_warn(LD_FS, "SR: Unable to write SR state to file %s", fname);
    ret = -1;
    goto done;
  }
  ret = 0;
  log_debug(LD_DIR, "SR: Saved state to file %s", fname);

 done:
  tor_free(fname);
  tor_free(content);
  return ret;
}

/* Reset our state to prepare for a new protocol run. Once this returns, all
 * commits in the state will be removed and freed. */
STATIC void
reset_state_for_new_protocol_run(time_t valid_after)
{
  tor_assert(sr_state);

  /* Keep counters in track */
  sr_state->n_reveal_rounds = 0;
  sr_state->n_commit_rounds = 0;
  sr_state->n_protocol_runs++;

  /* Reset valid-until */
  sr_state->valid_until = get_state_valid_until_time(valid_after);
  sr_state->valid_after = valid_after;

  /* We are in a new protocol run so cleanup commits. */
  sr_state_delete_commits();
}

/* This is the first round of the new protocol run starting at
 * <b>valid_after</b>. Do the necessary housekeeping. */
STATIC void
new_protocol_run(time_t valid_after)
{
  sr_commit_t *our_commitment = NULL;

  /* Only compute the srv at the end of the reveal phase. */
  if (sr_state->phase == SR_PHASE_REVEAL) {
    /* We are about to compute a new shared random value that will be set in
     * our state as the current value so rotate values. */
    state_rotate_srv();
    /* Compute the shared randomness value of the day. */
    sr_compute_srv();
  }

  /* Prepare for the new protocol run by reseting the state */
  reset_state_for_new_protocol_run(valid_after);

  /* Do some logging */
  log_info(LD_DIR, "SR: Protocol run #%" PRIu64 " starting!",
           sr_state->n_protocol_runs);

  /* Generate fresh commitments for this protocol run */
  our_commitment = sr_generate_our_commit(valid_after,
                                          get_my_v3_authority_cert());
  if (our_commitment) {
    /* Add our commitment to our state. In case we are unable to create one
     * (highly unlikely), we won't vote for this protocol run since our
     * commitment won't be in our state. */
    sr_state_add_commit(our_commitment);
  }
}

/* Return 1 iff the <b>next_phase</b> is a phase transition from the current
 * phase that is it's different. */
STATIC int
is_phase_transition(sr_phase_t next_phase)
{
  return sr_state->phase != next_phase;
}

/* Helper function: return a commit using the RSA fingerprint of the
 * authority or NULL if no such commit is known. */
static sr_commit_t *
state_query_get_commit(const char *rsa_fpr)
{
  tor_assert(rsa_fpr);
  return digestmap_get(sr_state->commits, rsa_fpr);
}

/* Helper function: This handles the GET state action using an
 * <b>obj_type</b> and <b>data</b> needed for the action. */
static void *
state_query_get_(sr_state_object_t obj_type, const void *data)
{
  void *obj = NULL;

  switch (obj_type) {
  case SR_STATE_OBJ_COMMIT:
  {
    obj = state_query_get_commit(data);
    break;
  }
  case SR_STATE_OBJ_COMMITS:
    obj = sr_state->commits;
    break;
  case SR_STATE_OBJ_CURSRV:
    obj = sr_state->current_srv;
    break;
  case SR_STATE_OBJ_PREVSRV:
    obj = sr_state->previous_srv;
    break;
  case SR_STATE_OBJ_PHASE:
    obj = &sr_state->phase;
    break;
  case SR_STATE_OBJ_VALID_AFTER:
  default:
    tor_assert(0);
  }
  return obj;
}

/* Helper function: This handles the PUT state action using an
 * <b>obj_type</b> and <b>data</b> needed for the action. */
static void
state_query_put_(sr_state_object_t obj_type, void *data)
{
  switch (obj_type) {
  case SR_STATE_OBJ_COMMIT:
  {
    sr_commit_t *commit = data;
    tor_assert(commit);
    commit_add_to_state(commit, sr_state);
    break;
  }
  case SR_STATE_OBJ_CURSRV:
    sr_state->current_srv = (sr_srv_t *) data;
    break;
  case SR_STATE_OBJ_PREVSRV:
    sr_state->previous_srv = (sr_srv_t *) data;
    break;
  case SR_STATE_OBJ_VALID_AFTER:
    sr_state->valid_after = *((time_t *) data);
    break;
  /* It's not allowed to change the phase nor the full commitments map from
   * the state. The phase is decided during a strict process post voting and
   * the commits should be put individually. */
  case SR_STATE_OBJ_PHASE:
  case SR_STATE_OBJ_COMMITS:
  default:
    tor_assert(0);
  }
}

/* Helper function: This handles the DEL_ALL state action using an
 * <b>obj_type</b> and <b>data</b> needed for the action. */
static void
state_query_del_all_(sr_state_object_t obj_type)
{
  switch (obj_type) {
  case SR_STATE_OBJ_COMMIT:
  {
    /* We are in a new protocol run so cleanup commitments. */
    DIGESTMAP_FOREACH_MODIFY(sr_state->commits, key, sr_commit_t *, c) {
      sr_commit_free(c);
      MAP_DEL_CURRENT(key);
    } DIGESTMAP_FOREACH_END;
    break;
  }
  /* The following object are _NOT_ suppose to be removed. */
  case SR_STATE_OBJ_CURSRV:
  case SR_STATE_OBJ_PREVSRV:
  case SR_STATE_OBJ_PHASE:
  case SR_STATE_OBJ_COMMITS:
  case SR_STATE_OBJ_VALID_AFTER:
  default:
    tor_assert(0);
  }
}

/* Helper function: This handles the DEL state action using an
 * <b>obj_type</b> and <b>data</b> needed for the action. */
static void
state_query_del_(sr_state_object_t obj_type, void *data)
{
  (void) data;

  switch (obj_type) {
  case SR_STATE_OBJ_PREVSRV:
    tor_free(sr_state->previous_srv);
    break;
  case SR_STATE_OBJ_CURSRV:
    tor_free(sr_state->current_srv);
    break;
  case SR_STATE_OBJ_COMMIT:
  case SR_STATE_OBJ_COMMITS:
  case SR_STATE_OBJ_PHASE:
  case SR_STATE_OBJ_VALID_AFTER:
  default:
    tor_assert(0);
  }
}

/* Query state using an <b>action</b> for an object type <b>obj_type</b>.
 * The <b>data</b> pointer needs to point to an object that the action needs
 * to use and if anything is required to be returned, it is stored in
 * <b>out</b>.
 *
 * This mechanism exists so we have one single point where we synchronized
 * our memory state with our disk state for every actions that changes it.
 * We then trigger a write on disk immediately.
 *
 * This should be the only entry point to our memory state. It's used by all
 * our state accessors and should be in the future. */
static void
state_query(sr_state_action_t action, sr_state_object_t obj_type,
            void *data, void **out)
{
  switch (action) {
  case SR_STATE_ACTION_GET:
    *out = state_query_get_(obj_type, data);
    break;
  case SR_STATE_ACTION_PUT:
    state_query_put_(obj_type, data);
    break;
  case SR_STATE_ACTION_DEL:
    state_query_del_(obj_type, data);
    break;
  case SR_STATE_ACTION_DEL_ALL:
    state_query_del_all_(obj_type);
    break;
  case SR_STATE_ACTION_SAVE:
    /* Only trigger a disk state save. */
    break;
  default:
    tor_assert(0);
  }

  /* If the action actually changes the state, immediately save it to disk.
   * The following will sync the state -> disk state and then save it. */
  if (action != SR_STATE_ACTION_GET) {
    disk_state_save_to_disk();
  }
}

/* Delete the current SRV value from the state freeing it and the value is set
 * to NULL meaning empty. */
static void
state_del_current_srv(void)
{
  state_query(SR_STATE_ACTION_DEL, SR_STATE_OBJ_CURSRV, NULL, NULL);
}

/* Delete the previous SRV value from the state freeing it and the value is
 * set to NULL meaning empty. */
static void
state_del_previous_srv(void)
{
  state_query(SR_STATE_ACTION_DEL, SR_STATE_OBJ_PREVSRV, NULL, NULL);
}

/* Rotate SRV value by freeing the previous value, assigning the current
 * value to the previous one and nullifying the current one. */
STATIC void
state_rotate_srv(void)
{
  /* First delete previous SRV from the state. Object will be freed. */
  state_del_previous_srv();
  /* Set previous SRV with the current one. */
  sr_state_set_previous_srv(sr_state_get_current_srv());
  /* Nullify the current srv. */
  sr_state_set_current_srv(NULL);
}

/* Set valid after time in the our state. */
void
sr_state_set_valid_after(time_t valid_after)
{
  state_query(SR_STATE_ACTION_PUT, SR_STATE_OBJ_VALID_AFTER,
              (void *) &valid_after, NULL);
}

/* Return the phase we are currently in according to our state. */
sr_phase_t
sr_state_get_phase(void)
{
  void *ptr;
  state_query(SR_STATE_ACTION_GET, SR_STATE_OBJ_PHASE, NULL, &ptr);
  return *(sr_phase_t *) ptr;
}

/* Return the previous SRV value from our state. Value CAN be NULL. */
const sr_srv_t *
sr_state_get_previous_srv(void)
{
  const sr_srv_t *srv;
  state_query(SR_STATE_ACTION_GET, SR_STATE_OBJ_PREVSRV, NULL,
              (void *) &srv);
  return srv;
}

/* Set the current SRV value from our state. Value CAN be NULL. The srv
 * object ownership is transfered to the state object. */
void
sr_state_set_previous_srv(const sr_srv_t *srv)
{
  state_query(SR_STATE_ACTION_PUT, SR_STATE_OBJ_PREVSRV, (void *) srv,
              NULL);
}

/* Return the current SRV value from our state. Value CAN be NULL. */
const sr_srv_t *
sr_state_get_current_srv(void)
{
  const sr_srv_t *srv;
  state_query(SR_STATE_ACTION_GET, SR_STATE_OBJ_CURSRV, NULL,
              (void *) &srv);
  return srv;
}

/* Set the current SRV value from our state. Value CAN be NULL. The srv
 * object ownership is transfered to the state object. */
void
sr_state_set_current_srv(const sr_srv_t *srv)
{
  state_query(SR_STATE_ACTION_PUT, SR_STATE_OBJ_CURSRV, (void *) srv,
              NULL);
}

/* Clean all the SRVs in our state. */
void
sr_state_clean_srvs(void)
{
  /* Remove SRVs from state. They will be set to NULL as "empty". */
  state_del_previous_srv();
  state_del_current_srv();
}

/* Return a pointer to the commits map from our state. CANNOT be NULL. */
digestmap_t *
sr_state_get_commits(void)
{
  digestmap_t *commits;
  state_query(SR_STATE_ACTION_GET, SR_STATE_OBJ_COMMITS,
              NULL, (void *) &commits);
  tor_assert(commits);
  return commits;
}

/* Update the current SR state as needed for the upcoming voting round at
 * <b>valid_after</b>. */
void
sr_state_update(time_t valid_after)
{
  sr_phase_t next_phase;

  tor_assert(sr_state);

  /* Don't call this function twice in the same voting period. */
  if (valid_after <= sr_state->valid_after) {
    log_info(LD_DIR, "SR: Asked to update state twice. Ignoring.");
    return;
  }

  /* Get phase of upcoming round. */
  next_phase = get_sr_protocol_phase(valid_after);

  /* If we are transitioning to a new protocol phase, prepare the stage. */
  if (is_phase_transition(next_phase)) {
    if (next_phase == SR_PHASE_COMMIT) {
      /* Going into commit phase means we are starting a new protocol run. */
      new_protocol_run(valid_after);
    }
    /* Set the new phase for this round */
    sr_state->phase = next_phase;
  } else if (sr_state->phase == SR_PHASE_COMMIT &&
             digestmap_size(sr_state->commits) == 0) {
    /* We are _NOT_ in a transition phase so if we are in the commit phase
     * and have no commit, generate one. Chances are that we are booting up
     * so let's have a commit in our state for the next voting period. */
    sr_commit_t *our_commit =
      sr_generate_our_commit(valid_after, get_my_v3_authority_cert());
    if (our_commit) {
      /* Add our commitment to our state. In case we are unable to create one
       * (highly unlikely), we won't vote for this protocol run since our
       * commitment won't be in our state. */
      sr_state_add_commit(our_commit);
    }
  }

  sr_state_set_valid_after(valid_after);

  /* Count the current round */
  if (sr_state->phase == SR_PHASE_COMMIT) {
    /* invariant check: we've not entered reveal phase yet */
    tor_assert(sr_state->n_reveal_rounds == 0);
    sr_state->n_commit_rounds++;
  } else {
    sr_state->n_reveal_rounds++;
  }

  { /* Debugging. */
    char tbuf[ISO_TIME_LEN + 1];
    format_iso_time(tbuf, valid_after);
    log_info(LD_DIR, "SR: State prepared for upcoming voting period (%s). "
             "Upcoming phase is %s (counters: %d commit & %d reveal rounds).",
             tbuf, get_phase_str(sr_state->phase),
             sr_state->n_commit_rounds, sr_state->n_reveal_rounds);
  }
}

/* Return commit object from the given authority digest <b>rsa_identity</b>.
 * Return NULL if not found. */
sr_commit_t *
sr_state_get_commit(const char *rsa_identity)
{
  sr_commit_t *commit;

  tor_assert(rsa_identity);

  state_query(SR_STATE_ACTION_GET, SR_STATE_OBJ_COMMIT,
              (void *) rsa_identity, (void *) &commit);
  return commit;
}

/* Add <b>commit</b> to the permanent state. The commit object ownership is
 * transfered to the state so the caller MUST not free it. */
void
sr_state_add_commit(sr_commit_t *commit)
{
  tor_assert(commit);

  /* Put the commit to the global state. */
  state_query(SR_STATE_ACTION_PUT, SR_STATE_OBJ_COMMIT,
              (void *) commit, NULL);

  log_debug(LD_DIR, "SR: Commit from %s has been added to our state.",
            sr_commit_get_rsa_fpr(commit));
}

/* Remove all commits from our state. */
void
sr_state_delete_commits(void)
{
  state_query(SR_STATE_ACTION_DEL_ALL, SR_STATE_OBJ_COMMIT, NULL, NULL);
}

/* Copy the reveal information from <b>commit</b> into <b>saved_commit</b>.
 * This <b>saved_commit</b> MUST come from our current SR state. Once modified,
 * the disk state is updated. */
void
sr_state_copy_reveal_info(sr_commit_t *saved_commit, const sr_commit_t *commit)
{
  tor_assert(saved_commit);
  tor_assert(commit);

  saved_commit->reveal_ts = commit->reveal_ts;
  memcpy(saved_commit->random_number, commit->random_number,
         sizeof(saved_commit->random_number));

  strlcpy(saved_commit->encoded_reveal, commit->encoded_reveal,
          sizeof(saved_commit->encoded_reveal));
  state_query(SR_STATE_ACTION_SAVE, 0, NULL, NULL);
  log_debug(LD_DIR, "SR: Reveal value learned %s (for commit %s) from %s",
            saved_commit->encoded_reveal, saved_commit->encoded_commit,
            sr_commit_get_rsa_fpr(saved_commit));
}

/* Set the fresh SRV flag from our state. This doesn't need to trigger a
 * disk state synchronization so we directly change the state. */
void
sr_state_set_fresh_srv(void)
{
  sr_state->is_srv_fresh = 1;
}

/* Unset the fresh SRV flag from our state. This doesn't need to trigger a
 * disk state synchronization so we directly change the state. */
void
sr_state_unset_fresh_srv(void)
{
  sr_state->is_srv_fresh = 0;
}

/* Return the value of the fresh SRV flag. */
unsigned int
sr_state_srv_is_fresh(void)
{
  return sr_state->is_srv_fresh;
}

/* Cleanup and free our disk and memory state. */
void
sr_state_free(void)
{
  state_free(sr_state);
  disk_state_free(sr_disk_state);
  /* Nullify our global state. */
  sr_state = NULL;
  sr_disk_state = NULL;
}

/* Save our current state in memory to disk. */
void
sr_state_save(void)
{
  /* Query a SAVE action on our current state so it's synced and saved. */
  state_query(SR_STATE_ACTION_SAVE, 0, NULL, NULL);
}

/* Return 1 iff the state has been initialized that is it exists in memory.
 * Return 0 otherwise. */
int
sr_state_is_initialized(void)
{
  return sr_state == NULL ? 0 : 1;
}

/* Initialize the disk and memory state.
 *
 * If save_to_disk is set to 1, the state is immediately saved to disk after
 * creation else it's not thus only kept in memory.
 * If read_from_disk is set to 1, we try to load the state from the disk and
 * if not found, a new state is created.
 *
 * Return 0 on success else a negative value on error. */
int
sr_state_init(int save_to_disk, int read_from_disk)
{
  int ret = -ENOENT;
  time_t now = time(NULL);

  /* We shouldn't have those assigned. */
  tor_assert(sr_disk_state == NULL);
  tor_assert(sr_state == NULL);

  /* First, try to load the state from disk. */
  if (read_from_disk) {
    ret = disk_state_load_from_disk();
  }

  if (ret < 0) {
    switch (-ret) {
    case EINVAL:
      /* We have a state on disk but it contains something we couldn't parse
       * or an invalid entry in the state file. Let's remove it since it's
       * obviously unusable and replace it by an new fresh state below. */
    case ENOENT:
      {
        /* No state on disk so allocate our states for the first time. */
        sr_state_t *new_state = state_new(default_fname, now);
        sr_disk_state_t *new_disk_state = disk_state_new(now);
        state_set(new_state);
        /* It's important to set our disk state pointer since the save call
         * below uses it to synchronized it with our memory state.  */
        disk_state_set(new_disk_state);
        /* No entry, let's save our new state to disk. */
        if (save_to_disk && disk_state_save_to_disk() < 0) {
          goto error;
        }
        break;
      }
    default:
      /* Big problem. Not possible. */
      tor_assert(0);
    }
  }
  /* We have a state in memory, let's make sure it's updated for the current
   * and next voting round. */
  {
    time_t valid_after = get_next_valid_after_time(now);
    sr_state_update(valid_after);
  }
  return 0;

 error:
  return -1;
}

#ifdef TOR_UNIT_TESTS

/* Set the current phase of the protocol. Used only by unit tests. */
void
set_sr_phase(sr_phase_t phase)
{
  tor_assert(sr_state);
  sr_state->phase = phase;
}

/* Get the SR state. Used only by unit tests */
sr_state_t *
get_sr_state(void)
{
  return sr_state;
}

#endif /* TOR_UNIT_TESTS */

