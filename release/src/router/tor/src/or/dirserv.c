/* Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#define DIRSERV_PRIVATE
#include "or.h"
#include "buffers.h"
#include "config.h"
#include "confparse.h"
#include "channel.h"
#include "channeltls.h"
#include "command.h"
#include "connection.h"
#include "connection_or.h"
#include "control.h"
#include "directory.h"
#include "dirserv.h"
#include "dirvote.h"
#include "hibernate.h"
#include "keypin.h"
#include "microdesc.h"
#include "networkstatus.h"
#include "nodelist.h"
#include "policies.h"
#include "rephist.h"
#include "router.h"
#include "routerlist.h"
#include "routerparse.h"
#include "routerset.h"
#include "torcert.h"

/**
 * \file dirserv.c
 * \brief Directory server core implementation. Manages directory
 * contents and generates directories.
 */

/** How far in the future do we allow a router to get? (seconds) */
#define ROUTER_ALLOW_SKEW (60*60*12)
/** How many seconds do we wait before regenerating the directory? */
#define DIR_REGEN_SLACK_TIME 30
/** If we're a cache, keep this many networkstatuses around from non-trusted
 * directory authorities. */
#define MAX_UNTRUSTED_NETWORKSTATUSES 16

extern time_t time_of_process_start; /* from main.c */

extern long stats_n_seconds_working; /* from main.c */

/** Total number of routers with measured bandwidth; this is set by
 * dirserv_count_measured_bws() before the loop in
 * dirserv_generate_networkstatus_vote_obj() and checked by
 * dirserv_get_credible_bandwidth() and
 * dirserv_compute_performance_thresholds() */
static int routers_with_measured_bw = 0;

static void directory_remove_invalid(void);
static char *format_versions_list(config_line_t *ln);
struct authdir_config_t;
static uint32_t
dirserv_get_status_impl(const char *fp, const char *nickname,
                        uint32_t addr, uint16_t or_port,
                        const char *platform, const char **msg,
                        int severity);
static void clear_cached_dir(cached_dir_t *d);
static const signed_descriptor_t *get_signed_descriptor_by_fp(
                                                        const char *fp,
                                                        int extrainfo,
                                                        time_t publish_cutoff);
static was_router_added_t dirserv_add_extrainfo(extrainfo_t *ei,
                                                const char **msg);
static uint32_t dirserv_get_bandwidth_for_router_kb(const routerinfo_t *ri);
static uint32_t dirserv_get_credible_bandwidth_kb(const routerinfo_t *ri);

/************** Fingerprint handling code ************/

/*                 1  Historically used to indicate Named */
#define FP_INVALID 2  /**< Believed invalid. */
#define FP_REJECT  4  /**< We will not publish this router. */
/*                 8  Historically used to avoid using this as a dir. */
#define FP_BADEXIT 16 /**< We'll tell clients not to use this as an exit. */
/*                 32 Historically used to indicade Unnamed */

/** Target of status_by_digest map. */
typedef uint32_t router_status_t;

static void add_fingerprint_to_dir(const char *fp,
                                   struct authdir_config_t *list,
                                   router_status_t add_status);

/** List of nickname-\>identity fingerprint mappings for all the routers
 * that we name.  Used to prevent router impersonation. */
typedef struct authdir_config_t {
  strmap_t *fp_by_name; /**< Map from lc nickname to fingerprint. */
  digestmap_t *status_by_digest; /**< Map from digest to router_status_t. */
} authdir_config_t;

/** Should be static; exposed for testing. */
static authdir_config_t *fingerprint_list = NULL;

/** Allocate and return a new, empty, authdir_config_t. */
static authdir_config_t *
authdir_config_new(void)
{
  authdir_config_t *list = tor_malloc_zero(sizeof(authdir_config_t));
  list->fp_by_name = strmap_new();
  list->status_by_digest = digestmap_new();
  return list;
}

/** Add the fingerprint <b>fp</b> to the smartlist of fingerprint_entry_t's
 * <b>list</b>, or-ing the currently set status flags with
 * <b>add_status</b>.
 */
/* static */ void
add_fingerprint_to_dir(const char *fp, authdir_config_t *list,
                       router_status_t add_status)
{
  char *fingerprint;
  char d[DIGEST_LEN];
  router_status_t *status;
  tor_assert(fp);
  tor_assert(list);

  fingerprint = tor_strdup(fp);
  tor_strstrip(fingerprint, " ");
  if (base16_decode(d, DIGEST_LEN, fingerprint, strlen(fingerprint))) {
    log_warn(LD_DIRSERV, "Couldn't decode fingerprint \"%s\"",
             escaped(fp));
    tor_free(fingerprint);
    return;
  }

  status = digestmap_get(list->status_by_digest, d);
  if (!status) {
    status = tor_malloc_zero(sizeof(router_status_t));
    digestmap_set(list->status_by_digest, d, status);
  }

  tor_free(fingerprint);
  *status |= add_status;
  return;
}

/** Add the fingerprint for this OR to the global list of recognized
 * identity key fingerprints. */
int
dirserv_add_own_fingerprint(crypto_pk_t *pk)
{
  char fp[FINGERPRINT_LEN+1];
  if (crypto_pk_get_fingerprint(pk, fp, 0)<0) {
    log_err(LD_BUG, "Error computing fingerprint");
    return -1;
  }
  if (!fingerprint_list)
    fingerprint_list = authdir_config_new();
  add_fingerprint_to_dir(fp, fingerprint_list, 0);
  return 0;
}

/** Load the nickname-\>fingerprint mappings stored in the approved-routers
 * file.  The file format is line-based, with each non-blank holding one
 * nickname, some space, and a fingerprint for that nickname.  On success,
 * replace the current fingerprint list with the new list and return 0.  On
 * failure, leave the current fingerprint list untouched, and return -1. */
int
dirserv_load_fingerprint_file(void)
{
  char *fname;
  char *cf;
  char *nickname, *fingerprint;
  authdir_config_t *fingerprint_list_new;
  int result;
  config_line_t *front=NULL, *list;

  fname = get_datadir_fname("approved-routers");
  log_info(LD_GENERAL,
           "Reloading approved fingerprints from \"%s\"...", fname);

  cf = read_file_to_str(fname, RFTS_IGNORE_MISSING, NULL);
  if (!cf) {
    log_warn(LD_FS, "Cannot open fingerprint file '%s'. That's ok.", fname);
    tor_free(fname);
    return 0;
  }
  tor_free(fname);

  result = config_get_lines(cf, &front, 0);
  tor_free(cf);
  if (result < 0) {
    log_warn(LD_CONFIG, "Error reading from fingerprint file");
    return -1;
  }

  fingerprint_list_new = authdir_config_new();

  for (list=front; list; list=list->next) {
    char digest_tmp[DIGEST_LEN];
    router_status_t add_status = 0;
    nickname = list->key; fingerprint = list->value;
    tor_strstrip(fingerprint, " "); /* remove spaces */
    if (strlen(fingerprint) != HEX_DIGEST_LEN ||
        base16_decode(digest_tmp, sizeof(digest_tmp),
                      fingerprint, HEX_DIGEST_LEN) < 0) {
      log_notice(LD_CONFIG,
                 "Invalid fingerprint (nickname '%s', "
                 "fingerprint %s). Skipping.",
                 nickname, fingerprint);
      continue;
    }
    if (!strcasecmp(nickname, "!reject")) {
        add_status = FP_REJECT;
    } else if (!strcasecmp(nickname, "!badexit")) {
        add_status = FP_BADEXIT;
    } else if (!strcasecmp(nickname, "!invalid")) {
        add_status = FP_INVALID;
    }
    add_fingerprint_to_dir(fingerprint, fingerprint_list_new, add_status);
  }

  config_free_lines(front);
  dirserv_free_fingerprint_list();
  fingerprint_list = fingerprint_list_new;
  /* Delete any routers whose fingerprints we no longer recognize */
  directory_remove_invalid();
  return 0;
}

/* If this is set, then we don't allow routers that have advertised an Ed25519
 * identity to stop doing so.  This is going to be essential for good identity
 * security: otherwise anybody who can attack RSA-1024 but not Ed25519 could
 * just sign fake descriptors missing the Ed25519 key.  But we won't actually
 * be able to prevent that kind of thing until we're confident that there
 * isn't actually a legit reason to downgrade to 0.2.5.  So for now, we have
 * to leave this #undef.
 */
#undef DISABLE_DISABLING_ED25519

/** Check whether <b>router</b> has a nickname/identity key combination that
 * we recognize from the fingerprint list, or an IP we automatically act on
 * according to our configuration.  Return the appropriate router status.
 *
 * If the status is 'FP_REJECT' and <b>msg</b> is provided, set
 * *<b>msg</b> to an explanation of why. */
uint32_t
dirserv_router_get_status(const routerinfo_t *router, const char **msg,
                          int severity)
{
  char d[DIGEST_LEN];
  const int key_pinning = get_options()->AuthDirPinKeys;

  if (crypto_pk_get_digest(router->identity_pkey, d)) {
    log_warn(LD_BUG,"Error computing fingerprint");
    if (msg)
      *msg = "Bug: Error computing fingerprint";
    return FP_REJECT;
  }

  if (router->cache_info.signing_key_cert) {
    /* This has an ed25519 identity key. */
    if (KEYPIN_MISMATCH ==
        keypin_check((const uint8_t*)router->cache_info.identity_digest,
                   router->cache_info.signing_key_cert->signing_key.pubkey)) {
      log_fn(severity, LD_DIR,
             "Descriptor from router %s has an Ed25519 key, "
               "but the <rsa,ed25519> keys don't match what they were before.",
               router_describe(router));
      if (key_pinning) {
        if (msg) {
          *msg = "Ed25519 identity key or RSA identity key has changed.";
        }
        return FP_REJECT;
      }
    }
  } else {
    /* No ed25519 key */
    if (KEYPIN_MISMATCH == keypin_check_lone_rsa(
                        (const uint8_t*)router->cache_info.identity_digest)) {
      log_fn(severity, LD_DIR,
               "Descriptor from router %s has no Ed25519 key, "
               "when we previously knew an Ed25519 for it. Ignoring for now, "
               "since Ed25519 keys are fairly new.",
               router_describe(router));
#ifdef DISABLE_DISABLING_ED25519
      if (key_pinning) {
        if (msg) {
          *msg = "Ed25519 identity key has disappeared.";
        }
        return FP_REJECT;
      }
#endif
    }
  }

  return dirserv_get_status_impl(d, router->nickname,
                                 router->addr, router->or_port,
                                 router->platform, msg, severity);
}

/** Return true if there is no point in downloading the router described by
 * <b>rs</b> because this directory would reject it. */
int
dirserv_would_reject_router(const routerstatus_t *rs)
{
  uint32_t res;

  res = dirserv_get_status_impl(rs->identity_digest, rs->nickname,
                                rs->addr, rs->or_port,
                                NULL, NULL, LOG_DEBUG);

  return (res & FP_REJECT) != 0;
}

/** Helper: As dirserv_router_get_status, but takes the router fingerprint
 * (hex, no spaces), nickname, address (used for logging only), IP address, OR
 * port and platform (logging only) as arguments.
 *
 * Log messages at 'severity'. (There's not much point in
 * logging that we're rejecting servers we'll not download.)
 */
static uint32_t
dirserv_get_status_impl(const char *id_digest, const char *nickname,
                        uint32_t addr, uint16_t or_port,
                        const char *platform, const char **msg, int severity)
{
  uint32_t result = 0;
  router_status_t *status_by_digest;

  if (!fingerprint_list)
    fingerprint_list = authdir_config_new();

  log_debug(LD_DIRSERV, "%d fingerprints, %d digests known.",
            strmap_size(fingerprint_list->fp_by_name),
            digestmap_size(fingerprint_list->status_by_digest));

  /* Versions before Tor 0.2.4.18-rc are too old to support, and are
   * missing some important security fixes too. Disable them. */
  if (platform && !tor_version_as_new_as(platform,"0.2.4.18-rc")) {
    if (msg)
      *msg = "Tor version is insecure or unsupported. Please upgrade!";
    return FP_REJECT;
  }

  status_by_digest = digestmap_get(fingerprint_list->status_by_digest,
                                   id_digest);
  if (status_by_digest)
    result |= *status_by_digest;

  if (result & FP_REJECT) {
    if (msg)
      *msg = "Fingerprint is marked rejected";
    return FP_REJECT;
  } else if (result & FP_INVALID) {
    if (msg)
      *msg = "Fingerprint is marked invalid";
  }

  if (authdir_policy_badexit_address(addr, or_port)) {
    log_fn(severity, LD_DIRSERV,
           "Marking '%s' as bad exit because of address '%s'",
               nickname, fmt_addr32(addr));
    result |= FP_BADEXIT;
  }

  if (!authdir_policy_permits_address(addr, or_port)) {
    log_fn(severity, LD_DIRSERV, "Rejecting '%s' because of address '%s'",
               nickname, fmt_addr32(addr));
    if (msg)
      *msg = "Authdir is rejecting routers in this range.";
    return FP_REJECT;
  }
  if (!authdir_policy_valid_address(addr, or_port)) {
    log_fn(severity, LD_DIRSERV,
           "Not marking '%s' valid because of address '%s'",
               nickname, fmt_addr32(addr));
    result |= FP_INVALID;
  }

  return result;
}

/** Clear the current fingerprint list. */
void
dirserv_free_fingerprint_list(void)
{
  if (!fingerprint_list)
    return;

  strmap_free(fingerprint_list->fp_by_name, tor_free_);
  digestmap_free(fingerprint_list->status_by_digest, tor_free_);
  tor_free(fingerprint_list);
}

/*
 *    Descriptor list
 */

/** Return -1 if <b>ri</b> has a private or otherwise bad address,
 * unless we're configured to not care. Return 0 if all ok. */
static int
dirserv_router_has_valid_address(routerinfo_t *ri)
{
  tor_addr_t addr;
  if (get_options()->DirAllowPrivateAddresses)
    return 0; /* whatever it is, we're fine with it */
  tor_addr_from_ipv4h(&addr, ri->addr);

  if (tor_addr_is_internal(&addr, 0)) {
    log_info(LD_DIRSERV,
             "Router %s published internal IP address. Refusing.",
             router_describe(ri));
    return -1; /* it's a private IP, we should reject it */
  }
  return 0;
}

/** Check whether we, as a directory server, want to accept <b>ri</b>.  If so,
 * set its is_valid,running fields and return 0.  Otherwise, return -1.
 *
 * If the router is rejected, set *<b>msg</b> to an explanation of why.
 *
 * If <b>complain</b> then explain at log-level 'notice' why we refused
 * a descriptor; else explain at log-level 'info'.
 */
int
authdir_wants_to_reject_router(routerinfo_t *ri, const char **msg,
                               int complain, int *valid_out)
{
  /* Okay.  Now check whether the fingerprint is recognized. */
  time_t now;
  int severity = (complain && ri->contact_info) ? LOG_NOTICE : LOG_INFO;
  uint32_t status = dirserv_router_get_status(ri, msg, severity);
  tor_assert(msg);
  if (status & FP_REJECT)
    return -1; /* msg is already set. */

  /* Is there too much clock skew? */
  now = time(NULL);
  if (ri->cache_info.published_on > now+ROUTER_ALLOW_SKEW) {
    log_fn(severity, LD_DIRSERV, "Publication time for %s is too "
           "far (%d minutes) in the future; possible clock skew. Not adding "
           "(%s)",
           router_describe(ri),
           (int)((ri->cache_info.published_on-now)/60),
           esc_router_info(ri));
    *msg = "Rejected: Your clock is set too far in the future, or your "
      "timezone is not correct.";
    return -1;
  }
  if (ri->cache_info.published_on < now-ROUTER_MAX_AGE_TO_PUBLISH) {
    log_fn(severity, LD_DIRSERV,
           "Publication time for %s is too far "
           "(%d minutes) in the past. Not adding (%s)",
           router_describe(ri),
           (int)((now-ri->cache_info.published_on)/60),
           esc_router_info(ri));
    *msg = "Rejected: Server is expired, or your clock is too far in the past,"
      " or your timezone is not correct.";
    return -1;
  }
  if (dirserv_router_has_valid_address(ri) < 0) {
    log_fn(severity, LD_DIRSERV,
           "Router %s has invalid address. Not adding (%s).",
           router_describe(ri),
           esc_router_info(ri));
    *msg = "Rejected: Address is a private address.";
    return -1;
  }

  *valid_out = ! (status & FP_INVALID);

  return 0;
}

/** Update the relevant flags of <b>node</b> based on our opinion as a
 * directory authority in <b>authstatus</b>, as returned by
 * dirserv_router_get_status or equivalent.  */
void
dirserv_set_node_flags_from_authoritative_status(node_t *node,
                                                 uint32_t authstatus)
{
  node->is_valid = (authstatus & FP_INVALID) ? 0 : 1;
  node->is_bad_exit = (authstatus & FP_BADEXIT) ? 1 : 0;
}

/** True iff <b>a</b> is more severe than <b>b</b>. */
static int
WRA_MORE_SEVERE(was_router_added_t a, was_router_added_t b)
{
  return a < b;
}

/** As for dirserv_add_descriptor(), but accepts multiple documents, and
 * returns the most severe error that occurred for any one of them. */
was_router_added_t
dirserv_add_multiple_descriptors(const char *desc, uint8_t purpose,
                                 const char *source,
                                 const char **msg)
{
  was_router_added_t r, r_tmp;
  const char *msg_out;
  smartlist_t *list;
  const char *s;
  int n_parsed = 0;
  time_t now = time(NULL);
  char annotation_buf[ROUTER_ANNOTATION_BUF_LEN];
  char time_buf[ISO_TIME_LEN+1];
  int general = purpose == ROUTER_PURPOSE_GENERAL;
  tor_assert(msg);

  r=ROUTER_ADDED_SUCCESSFULLY; /*Least severe return value. */

  format_iso_time(time_buf, now);
  if (tor_snprintf(annotation_buf, sizeof(annotation_buf),
                   "@uploaded-at %s\n"
                   "@source %s\n"
                   "%s%s%s", time_buf, escaped(source),
                   !general ? "@purpose " : "",
                   !general ? router_purpose_to_string(purpose) : "",
                   !general ? "\n" : "")<0) {
    *msg = "Couldn't format annotations";
    return -1;
  }

  s = desc;
  list = smartlist_new();
  if (!router_parse_list_from_string(&s, NULL, list, SAVED_NOWHERE, 0, 0,
                                     annotation_buf, NULL)) {
    SMARTLIST_FOREACH(list, routerinfo_t *, ri, {
        msg_out = NULL;
        tor_assert(ri->purpose == purpose);
        r_tmp = dirserv_add_descriptor(ri, &msg_out, source);
        if (WRA_MORE_SEVERE(r_tmp, r)) {
          r = r_tmp;
          *msg = msg_out;
        }
      });
  }
  n_parsed += smartlist_len(list);
  smartlist_clear(list);

  s = desc;
  if (!router_parse_list_from_string(&s, NULL, list, SAVED_NOWHERE, 1, 0,
                                     NULL, NULL)) {
    SMARTLIST_FOREACH(list, extrainfo_t *, ei, {
        msg_out = NULL;

        r_tmp = dirserv_add_extrainfo(ei, &msg_out);
        if (WRA_MORE_SEVERE(r_tmp, r)) {
          r = r_tmp;
          *msg = msg_out;
        }
      });
  }
  n_parsed += smartlist_len(list);
  smartlist_free(list);

  if (! *msg) {
    if (!n_parsed) {
      *msg = "No descriptors found in your POST.";
      if (WRA_WAS_ADDED(r))
        r = ROUTER_IS_ALREADY_KNOWN;
    } else {
      *msg = "(no message)";
    }
  }

  return r;
}

/** Examine the parsed server descriptor in <b>ri</b> and maybe insert it into
 * the list of server descriptors. Set *<b>msg</b> to a message that should be
 * passed back to the origin of this descriptor, or NULL if there is no such
 * message. Use <b>source</b> to produce better log messages.
 *
 * Return the status of the operation
 *
 * This function is only called when fresh descriptors are posted, not when
 * we re-load the cache.
 */
was_router_added_t
dirserv_add_descriptor(routerinfo_t *ri, const char **msg, const char *source)
{
  was_router_added_t r;
  routerinfo_t *ri_old;
  char *desc, *nickname;
  const size_t desclen = ri->cache_info.signed_descriptor_len +
      ri->cache_info.annotations_len;
  const int key_pinning = get_options()->AuthDirPinKeys;
  *msg = NULL;

  /* If it's too big, refuse it now. Otherwise we'll cache it all over the
   * network and it'll clog everything up. */
  if (ri->cache_info.signed_descriptor_len > MAX_DESCRIPTOR_UPLOAD_SIZE) {
    log_notice(LD_DIR, "Somebody attempted to publish a router descriptor '%s'"
               " (source: %s) with size %d. Either this is an attack, or the "
               "MAX_DESCRIPTOR_UPLOAD_SIZE (%d) constant is too low.",
               ri->nickname, source, (int)ri->cache_info.signed_descriptor_len,
               MAX_DESCRIPTOR_UPLOAD_SIZE);
    *msg = "Router descriptor was too large.";
    control_event_or_authdir_new_descriptor("REJECTED",
               ri->cache_info.signed_descriptor_body,
                                            desclen, *msg);
    routerinfo_free(ri);
    return ROUTER_AUTHDIR_REJECTS;
  }

  /* Check whether this descriptor is semantically identical to the last one
   * from this server.  (We do this here and not in router_add_to_routerlist
   * because we want to be able to accept the newest router descriptor that
   * another authority has, so we all converge on the same one.) */
  ri_old = router_get_mutable_by_digest(ri->cache_info.identity_digest);
  if (ri_old && ri_old->cache_info.published_on < ri->cache_info.published_on
      && router_differences_are_cosmetic(ri_old, ri)
      && !router_is_me(ri)) {
    log_info(LD_DIRSERV,
             "Not replacing descriptor from %s (source: %s); "
             "differences are cosmetic.",
             router_describe(ri), source);
    *msg = "Not replacing router descriptor; no information has changed since "
      "the last one with this identity.";
    control_event_or_authdir_new_descriptor("DROPPED",
                         ri->cache_info.signed_descriptor_body,
                                            desclen, *msg);
    routerinfo_free(ri);
    return ROUTER_IS_ALREADY_KNOWN;
  }

  /* Do keypinning again ... this time, to add the pin if appropriate */
  int keypin_status;
  if (ri->cache_info.signing_key_cert) {
    keypin_status = keypin_check_and_add(
      (const uint8_t*)ri->cache_info.identity_digest,
      ri->cache_info.signing_key_cert->signing_key.pubkey,
      ! key_pinning);
  } else {
    keypin_status = keypin_check_lone_rsa(
      (const uint8_t*)ri->cache_info.identity_digest);
#ifndef DISABLE_DISABLING_ED25519
    if (keypin_status == KEYPIN_MISMATCH)
      keypin_status = KEYPIN_NOT_FOUND;
#endif
  }
  if (keypin_status == KEYPIN_MISMATCH && key_pinning) {
    log_info(LD_DIRSERV, "Dropping descriptor from %s (source: %s) because "
             "its key did not match an older RSA/Ed25519 keypair",
             router_describe(ri), source);
    *msg = "Looks like your keypair does not match its older value.";
    return ROUTER_AUTHDIR_REJECTS;
  }

  /* Make a copy of desc, since router_add_to_routerlist might free
   * ri and its associated signed_descriptor_t. */
  desc = tor_strndup(ri->cache_info.signed_descriptor_body, desclen);
  nickname = tor_strdup(ri->nickname);

  /* Tell if we're about to need to launch a test if we add this. */
  ri->needs_retest_if_added =
    dirserv_should_launch_reachability_test(ri, ri_old);

  r = router_add_to_routerlist(ri, msg, 0, 0);
  if (!WRA_WAS_ADDED(r)) {
    /* unless the routerinfo was fine, just out-of-date */
    if (WRA_WAS_REJECTED(r))
      control_event_or_authdir_new_descriptor("REJECTED", desc, desclen, *msg);
    log_info(LD_DIRSERV,
             "Did not add descriptor from '%s' (source: %s): %s.",
             nickname, source, *msg ? *msg : "(no message)");
  } else {
    smartlist_t *changed;
    control_event_or_authdir_new_descriptor("ACCEPTED", desc, desclen, *msg);

    changed = smartlist_new();
    smartlist_add(changed, ri);
    routerlist_descriptors_added(changed, 0);
    smartlist_free(changed);
    if (!*msg) {
      *msg =  "Descriptor accepted";
    }
    log_info(LD_DIRSERV,
             "Added descriptor from '%s' (source: %s): %s.",
             nickname, source, *msg);
  }
  tor_free(desc);
  tor_free(nickname);
  return r;
}

/** As dirserv_add_descriptor, but for an extrainfo_t <b>ei</b>. */
static was_router_added_t
dirserv_add_extrainfo(extrainfo_t *ei, const char **msg)
{
  routerinfo_t *ri;
  int r;
  tor_assert(msg);
  *msg = NULL;

  /* Needs to be mutable so routerinfo_incompatible_with_extrainfo
   * can mess with some of the flags in ri->cache_info. */
  ri = router_get_mutable_by_digest(ei->cache_info.identity_digest);
  if (!ri) {
    *msg = "No corresponding router descriptor for extra-info descriptor";
    extrainfo_free(ei);
    return ROUTER_BAD_EI;
  }

  /* If it's too big, refuse it now. Otherwise we'll cache it all over the
   * network and it'll clog everything up. */
  if (ei->cache_info.signed_descriptor_len > MAX_EXTRAINFO_UPLOAD_SIZE) {
    log_notice(LD_DIR, "Somebody attempted to publish an extrainfo "
               "with size %d. Either this is an attack, or the "
               "MAX_EXTRAINFO_UPLOAD_SIZE (%d) constant is too low.",
               (int)ei->cache_info.signed_descriptor_len,
               MAX_EXTRAINFO_UPLOAD_SIZE);
    *msg = "Extrainfo document was too large";
    extrainfo_free(ei);
    return ROUTER_BAD_EI;
  }

  if ((r = routerinfo_incompatible_with_extrainfo(ri->identity_pkey, ei,
                                                  &ri->cache_info, msg))) {
    extrainfo_free(ei);
    return r < 0 ? ROUTER_IS_ALREADY_KNOWN : ROUTER_BAD_EI;
  }
  router_add_extrainfo_to_routerlist(ei, msg, 0, 0);
  return ROUTER_ADDED_SUCCESSFULLY;
}

/** Remove all descriptors whose nicknames or fingerprints no longer
 * are allowed by our fingerprint list. (Descriptors that used to be
 * good can become bad when we reload the fingerprint list.)
 */
static void
directory_remove_invalid(void)
{
  routerlist_t *rl = router_get_routerlist();
  smartlist_t *nodes = smartlist_new();
  smartlist_add_all(nodes, nodelist_get_list());

  SMARTLIST_FOREACH_BEGIN(nodes, node_t *, node) {
    const char *msg = NULL;
    routerinfo_t *ent = node->ri;
    char description[NODE_DESC_BUF_LEN];
    uint32_t r;
    if (!ent)
      continue;
    r = dirserv_router_get_status(ent, &msg, LOG_INFO);
    router_get_description(description, ent);
    if (r & FP_REJECT) {
      log_info(LD_DIRSERV, "Router %s is now rejected: %s",
               description, msg?msg:"");
      routerlist_remove(rl, ent, 0, time(NULL));
      continue;
    }
    if (bool_neq((r & FP_INVALID), !node->is_valid)) {
      log_info(LD_DIRSERV, "Router '%s' is now %svalid.", description,
               (r&FP_INVALID) ? "in" : "");
      node->is_valid = (r&FP_INVALID)?0:1;
    }
    if (bool_neq((r & FP_BADEXIT), node->is_bad_exit)) {
      log_info(LD_DIRSERV, "Router '%s' is now a %s exit", description,
               (r & FP_BADEXIT) ? "bad" : "good");
      node->is_bad_exit = (r&FP_BADEXIT) ? 1: 0;
    }
  } SMARTLIST_FOREACH_END(node);

  routerlist_assert_ok(rl);
  smartlist_free(nodes);
}

/**
 * Allocate and return a description of the status of the server <b>desc</b>,
 * for use in a v1-style router-status line.  The server is listed
 * as running iff <b>is_live</b> is true.
 */
static char *
list_single_server_status(const routerinfo_t *desc, int is_live)
{
  char buf[MAX_NICKNAME_LEN+HEX_DIGEST_LEN+4]; /* !nickname=$hexdigest\0 */
  char *cp;
  const node_t *node;

  tor_assert(desc);

  cp = buf;
  if (!is_live) {
    *cp++ = '!';
  }
  node = node_get_by_id(desc->cache_info.identity_digest);
  if (node && node->is_valid) {
    strlcpy(cp, desc->nickname, sizeof(buf)-(cp-buf));
    cp += strlen(cp);
    *cp++ = '=';
  }
  *cp++ = '$';
  base16_encode(cp, HEX_DIGEST_LEN+1, desc->cache_info.identity_digest,
                DIGEST_LEN);
  return tor_strdup(buf);
}

/* DOCDOC running_long_enough_to_decide_unreachable */
static inline int
running_long_enough_to_decide_unreachable(void)
{
  return time_of_process_start
    + get_options()->TestingAuthDirTimeToLearnReachability < approx_time();
}

/** Each server needs to have passed a reachability test no more
 * than this number of seconds ago, or it is listed as down in
 * the directory. */
#define REACHABLE_TIMEOUT (45*60)

/** If we tested a router and found it reachable _at least this long_ after it
 * declared itself hibernating, it is probably done hibernating and we just
 * missed a descriptor from it. */
#define HIBERNATION_PUBLICATION_SKEW (60*60)

/** Treat a router as alive if
 *    - It's me, and I'm not hibernating.
 * or - We've found it reachable recently. */
void
dirserv_set_router_is_running(routerinfo_t *router, time_t now)
{
  /*XXXX024 This function is a mess.  Separate out the part that calculates
    whether it's reachable and the part that tells rephist that the router was
    unreachable.
   */
  int answer;
  const or_options_t *options = get_options();
  node_t *node = node_get_mutable_by_id(router->cache_info.identity_digest);
  tor_assert(node);

  if (router_is_me(router)) {
    /* We always know if we are down ourselves. */
    answer = ! we_are_hibernating();
  } else if (router->is_hibernating &&
             (router->cache_info.published_on +
              HIBERNATION_PUBLICATION_SKEW) > node->last_reachable) {
    /* A hibernating router is down unless we (somehow) had contact with it
     * since it declared itself to be hibernating. */
    answer = 0;
  } else if (options->AssumeReachable) {
    /* If AssumeReachable, everybody is up unless they say they are down! */
    answer = 1;
  } else {
    /* Otherwise, a router counts as up if we found all announced OR
       ports reachable in the last REACHABLE_TIMEOUT seconds.

       XXX prop186 For now there's always one IPv4 and at most one
       IPv6 OR port.

       If we're not on IPv6, don't consider reachability of potential
       IPv6 OR port since that'd kill all dual stack relays until a
       majority of the dir auths have IPv6 connectivity. */
    answer = (now < node->last_reachable + REACHABLE_TIMEOUT &&
              (options->AuthDirHasIPv6Connectivity != 1 ||
               tor_addr_is_null(&router->ipv6_addr) ||
               now < node->last_reachable6 + REACHABLE_TIMEOUT));
  }

  if (!answer && running_long_enough_to_decide_unreachable()) {
    /* Not considered reachable. tell rephist about that.

       Because we launch a reachability test for each router every
       REACHABILITY_TEST_CYCLE_PERIOD seconds, then the router has probably
       been down since at least that time after we last successfully reached
       it.

       XXX ipv6
     */
    time_t when = now;
    if (node->last_reachable &&
        node->last_reachable + REACHABILITY_TEST_CYCLE_PERIOD < now)
      when = node->last_reachable + REACHABILITY_TEST_CYCLE_PERIOD;
    rep_hist_note_router_unreachable(router->cache_info.identity_digest, when);
  }

  node->is_running = answer;
}

/** Based on the routerinfo_ts in <b>routers</b>, allocate the
 * contents of a v1-style router-status line, and store it in
 * *<b>router_status_out</b>.  Return 0 on success, -1 on failure.
 *
 * If for_controller is true, include the routers with very old descriptors.
 */
int
list_server_status_v1(smartlist_t *routers, char **router_status_out,
                      int for_controller)
{
  /* List of entries in a router-status style: An optional !, then an optional
   * equals-suffixed nickname, then a dollar-prefixed hexdigest. */
  smartlist_t *rs_entries;
  time_t now = time(NULL);
  time_t cutoff = now - ROUTER_MAX_AGE_TO_PUBLISH;
  const or_options_t *options = get_options();
  /* We include v2 dir auths here too, because they need to answer
   * controllers. Eventually we'll deprecate this whole function;
   * see also networkstatus_getinfo_by_purpose(). */
  int authdir = authdir_mode_publishes_statuses(options);
  tor_assert(router_status_out);

  rs_entries = smartlist_new();

  SMARTLIST_FOREACH_BEGIN(routers, routerinfo_t *, ri) {
    const node_t *node = node_get_by_id(ri->cache_info.identity_digest);
    tor_assert(node);
    if (authdir) {
      /* Update router status in routerinfo_t. */
      dirserv_set_router_is_running(ri, now);
    }
    if (for_controller) {
      char name_buf[MAX_VERBOSE_NICKNAME_LEN+2];
      char *cp = name_buf;
      if (!node->is_running)
        *cp++ = '!';
      router_get_verbose_nickname(cp, ri);
      smartlist_add(rs_entries, tor_strdup(name_buf));
    } else if (ri->cache_info.published_on >= cutoff) {
      smartlist_add(rs_entries, list_single_server_status(ri,
                                                          node->is_running));
    }
  } SMARTLIST_FOREACH_END(ri);

  *router_status_out = smartlist_join_strings(rs_entries, " ", 0, NULL);

  SMARTLIST_FOREACH(rs_entries, char *, cp, tor_free(cp));
  smartlist_free(rs_entries);

  return 0;
}

/** Given a (possibly empty) list of config_line_t, each line of which contains
 * a list of comma-separated version numbers surrounded by optional space,
 * allocate and return a new string containing the version numbers, in order,
 * separated by commas.  Used to generate Recommended(Client|Server)?Versions
 */
static char *
format_versions_list(config_line_t *ln)
{
  smartlist_t *versions;
  char *result;
  versions = smartlist_new();
  for ( ; ln; ln = ln->next) {
    smartlist_split_string(versions, ln->value, ",",
                           SPLIT_SKIP_SPACE|SPLIT_IGNORE_BLANK, 0);
  }
  sort_version_list(versions, 1);
  result = smartlist_join_strings(versions,",",0,NULL);
  SMARTLIST_FOREACH(versions,char *,s,tor_free(s));
  smartlist_free(versions);
  return result;
}

/** Return 1 if <b>ri</b>'s descriptor is "active" -- running, valid,
 * not hibernating, having observed bw greater 0, and not too old. Else
 * return 0.
 */
static int
router_is_active(const routerinfo_t *ri, const node_t *node, time_t now)
{
  time_t cutoff = now - ROUTER_MAX_AGE_TO_PUBLISH;
  if (ri->cache_info.published_on < cutoff) {
    return 0;
  }
  if (!node->is_running || !node->is_valid || ri->is_hibernating) {
    return 0;
  }
  /* Only require bandwith capacity in non-test networks, or
   * if TestingTorNetwork, and TestingMinExitFlagThreshold is non-zero */
  if (!ri->bandwidthcapacity) {
    if (get_options()->TestingTorNetwork) {
      if (get_options()->TestingMinExitFlagThreshold > 0) {
        /* If we're in a TestingTorNetwork, and TestingMinExitFlagThreshold is,
         * then require bandwidthcapacity */
        return 0;
      }
    } else {
      /* If we're not in a TestingTorNetwork, then require bandwidthcapacity */
      return 0;
    }
  }
  return 1;
}

/** Generate a new v1 directory and write it into a newly allocated string.
 * Point *<b>dir_out</b> to the allocated string.  Sign the
 * directory with <b>private_key</b>.  Return 0 on success, -1 on
 * failure. If <b>complete</b> is set, give us all the descriptors;
 * otherwise leave out non-running and non-valid ones.
 */
int
dirserv_dump_directory_to_string(char **dir_out,
                                 crypto_pk_t *private_key)
{
  /* XXXX 024 Get rid of this function if we can confirm that nobody's
   * fetching these any longer */
  char *cp;
  char *identity_pkey; /* Identity key, DER64-encoded. */
  char *recommended_versions;
  char digest[DIGEST_LEN];
  char published[ISO_TIME_LEN+1];
  char *buf = NULL;
  size_t buf_len;
  size_t identity_pkey_len;
  time_t now = time(NULL);

  tor_assert(dir_out);
  *dir_out = NULL;

  if (crypto_pk_write_public_key_to_string(private_key,&identity_pkey,
                                           &identity_pkey_len)<0) {
    log_warn(LD_BUG,"write identity_pkey to string failed!");
    return -1;
  }

  recommended_versions =
    format_versions_list(get_options()->RecommendedVersions);

  format_iso_time(published, now);

  buf_len = 2048+strlen(recommended_versions);

  buf = tor_malloc(buf_len);
  /* We'll be comparing against buf_len throughout the rest of the
     function, though strictly speaking we shouldn't be able to exceed
     it.  This is C, after all, so we may as well check for buffer
     overruns.*/

  tor_snprintf(buf, buf_len,
               "signed-directory\n"
               "published %s\n"
               "recommended-software %s\n"
               "router-status %s\n"
               "dir-signing-key\n%s\n",
               published, recommended_versions, "",
               identity_pkey);

  tor_free(recommended_versions);
  tor_free(identity_pkey);

  cp = buf + strlen(buf);
  *cp = '\0';

  /* These multiple strlcat calls are inefficient, but dwarfed by the RSA
     signature. */
  if (strlcat(buf, "directory-signature ", buf_len) >= buf_len)
    goto truncated;
  if (strlcat(buf, get_options()->Nickname, buf_len) >= buf_len)
    goto truncated;
  if (strlcat(buf, "\n", buf_len) >= buf_len)
    goto truncated;

  if (router_get_dir_hash(buf,digest)) {
    log_warn(LD_BUG,"couldn't compute digest");
    tor_free(buf);
    return -1;
  }
  note_crypto_pk_op(SIGN_DIR);
  if (router_append_dirobj_signature(buf,buf_len,digest,DIGEST_LEN,
                                     private_key)<0) {
    tor_free(buf);
    return -1;
  }

  *dir_out = buf;
  return 0;
 truncated:
  log_warn(LD_BUG,"tried to exceed string length.");
  tor_free(buf);
  return -1;
}

/********************************************************************/

/* A set of functions to answer questions about how we'd like to behave
 * as a directory mirror/client. */

/** Return 1 if we fetch our directory material directly from the
 * authorities, rather than from a mirror. */
int
directory_fetches_from_authorities(const or_options_t *options)
{
  const routerinfo_t *me;
  uint32_t addr;
  int refuseunknown;
  if (options->FetchDirInfoEarly)
    return 1;
  if (options->BridgeRelay == 1)
    return 0;
  if (server_mode(options) && router_pick_published_address(options, &addr)<0)
    return 1; /* we don't know our IP address; ask an authority. */
  refuseunknown = ! router_my_exit_policy_is_reject_star() &&
    should_refuse_unknown_exits(options);
  if (!dir_server_mode(options) && !refuseunknown)
    return 0;
  if (!server_mode(options) || !advertised_server_mode())
    return 0;
  me = router_get_my_routerinfo();
  if (!me || (!me->supports_tunnelled_dir_requests && !refuseunknown))
    return 0; /* if we don't service directory requests, return 0 too */
  return 1;
}

/** Return 1 if we should fetch new networkstatuses, descriptors, etc
 * on the "mirror" schedule rather than the "client" schedule.
 */
int
directory_fetches_dir_info_early(const or_options_t *options)
{
  return directory_fetches_from_authorities(options);
}

/** Return 1 if we should fetch new networkstatuses, descriptors, etc
 * on a very passive schedule -- waiting long enough for ordinary clients
 * to probably have the info we want. These would include bridge users,
 * and maybe others in the future e.g. if a Tor client uses another Tor
 * client as a directory guard.
 */
int
directory_fetches_dir_info_later(const or_options_t *options)
{
  return options->UseBridges != 0;
}

/** Return true iff we want to fetch and keep certificates for authorities
 * that we don't acknowledge as authorities ourself.
 */
int
directory_caches_unknown_auth_certs(const or_options_t *options)
{
  return dir_server_mode(options) || options->BridgeRelay;
}

/** Return 1 if we want to keep descriptors, networkstatuses, etc around.
 * Else return 0.
 * Check options->DirPort_set and directory_permits_begindir_requests()
 * to see if we are willing to serve these directory documents to others via
 * the DirPort and begindir-over-ORPort, respectively.
 */
int
directory_caches_dir_info(const or_options_t *options)
{
  if (options->BridgeRelay || dir_server_mode(options))
    return 1;
  if (!server_mode(options) || !advertised_server_mode())
    return 0;
  /* We need an up-to-date view of network info if we're going to try to
   * block exit attempts from unknown relays. */
  return ! router_my_exit_policy_is_reject_star() &&
    should_refuse_unknown_exits(options);
}

/** Return 1 if we want to allow remote people to ask us directory
 * requests via the "begin_dir" interface, which doesn't require
 * having any separate port open. */
int
directory_permits_begindir_requests(const or_options_t *options)
{
  return options->BridgeRelay != 0 || dir_server_mode(options);
}

/** Return 1 if we have no need to fetch new descriptors. This generally
 * happens when we're not a dir cache and we haven't built any circuits
 * lately.
 */
int
directory_too_idle_to_fetch_descriptors(const or_options_t *options,
                                        time_t now)
{
  return !directory_caches_dir_info(options) &&
         !options->FetchUselessDescriptors &&
         rep_hist_circbuilding_dormant(now);
}

/********************************************************************/

/** Map from flavor name to the cached_dir_t for the v3 consensuses that we're
 * currently serving. */
static strmap_t *cached_consensuses = NULL;

/** Decrement the reference count on <b>d</b>, and free it if it no longer has
 * any references. */
void
cached_dir_decref(cached_dir_t *d)
{
  if (!d || --d->refcnt > 0)
    return;
  clear_cached_dir(d);
  tor_free(d);
}

/** Allocate and return a new cached_dir_t containing the string <b>s</b>,
 * published at <b>published</b>. */
cached_dir_t *
new_cached_dir(char *s, time_t published)
{
  cached_dir_t *d = tor_malloc_zero(sizeof(cached_dir_t));
  d->refcnt = 1;
  d->dir = s;
  d->dir_len = strlen(s);
  d->published = published;
  if (tor_gzip_compress(&(d->dir_z), &(d->dir_z_len), d->dir, d->dir_len,
                        ZLIB_METHOD)) {
    log_warn(LD_BUG, "Error compressing directory");
  }
  return d;
}

/** Remove all storage held in <b>d</b>, but do not free <b>d</b> itself. */
static void
clear_cached_dir(cached_dir_t *d)
{
  tor_free(d->dir);
  tor_free(d->dir_z);
  memset(d, 0, sizeof(cached_dir_t));
}

/** Free all storage held by the cached_dir_t in <b>d</b>. */
static void
free_cached_dir_(void *_d)
{
  cached_dir_t *d;
  if (!_d)
    return;

  d = (cached_dir_t *)_d;
  cached_dir_decref(d);
}

/** Replace the v3 consensus networkstatus of type <b>flavor_name</b> that
 * we're serving with <b>networkstatus</b>, published at <b>published</b>.  No
 * validation is performed. */
void
dirserv_set_cached_consensus_networkstatus(const char *networkstatus,
                                           const char *flavor_name,
                                           const common_digests_t *digests,
                                           time_t published)
{
  cached_dir_t *new_networkstatus;
  cached_dir_t *old_networkstatus;
  if (!cached_consensuses)
    cached_consensuses = strmap_new();

  new_networkstatus = new_cached_dir(tor_strdup(networkstatus), published);
  memcpy(&new_networkstatus->digests, digests, sizeof(common_digests_t));
  old_networkstatus = strmap_set(cached_consensuses, flavor_name,
                                 new_networkstatus);
  if (old_networkstatus)
    cached_dir_decref(old_networkstatus);
}

/** Return the latest downloaded consensus networkstatus in encoded, signed,
 * optionally compressed format, suitable for sending to clients. */
cached_dir_t *
dirserv_get_consensus(const char *flavor_name)
{
  if (!cached_consensuses)
    return NULL;
  return strmap_get(cached_consensuses, flavor_name);
}

/** If a router's uptime is at least this value, then it is always
 * considered stable, regardless of the rest of the network. This
 * way we resist attacks where an attacker doubles the size of the
 * network using allegedly high-uptime nodes, displacing all the
 * current guards. */
#define UPTIME_TO_GUARANTEE_STABLE (3600*24*30)
/** If a router's MTBF is at least this value, then it is always stable.
 * See above.  (Corresponds to about 7 days for current decay rates.) */
#define MTBF_TO_GUARANTEE_STABLE (60*60*24*5)
/** Similarly, every node with at least this much weighted time known can be
 * considered familiar enough to be a guard.  Corresponds to about 20 days for
 * current decay rates.
 */
#define TIME_KNOWN_TO_GUARANTEE_FAMILIAR (8*24*60*60)
/** Similarly, every node with sufficient WFU is around enough to be a guard.
 */
#define WFU_TO_GUARANTEE_GUARD (0.98)

/* Thresholds for server performance: set by
 * dirserv_compute_performance_thresholds, and used by
 * generate_v2_networkstatus */

/** Any router with an uptime of at least this value is stable. */
static uint32_t stable_uptime = 0; /* start at a safe value */
/** Any router with an mtbf of at least this value is stable. */
static double stable_mtbf = 0.0;
/** If true, we have measured enough mtbf info to look at stable_mtbf rather
 * than stable_uptime. */
static int enough_mtbf_info = 0;
/** Any router with a weighted fractional uptime of at least this much might
 * be good as a guard. */
static double guard_wfu = 0.0;
/** Don't call a router a guard unless we've known about it for at least this
 * many seconds. */
static long guard_tk = 0;
/** Any router with a bandwidth at least this high is "Fast" */
static uint32_t fast_bandwidth_kb = 0;
/** If exits can be guards, then all guards must have a bandwidth this
 * high. */
static uint32_t guard_bandwidth_including_exits_kb = 0;
/** If exits can't be guards, then all guards must have a bandwidth this
 * high. */
static uint32_t guard_bandwidth_excluding_exits_kb = 0;

/** Helper: estimate the uptime of a router given its stated uptime and the
 * amount of time since it last stated its stated uptime. */
static inline long
real_uptime(const routerinfo_t *router, time_t now)
{
  if (now < router->cache_info.published_on)
    return router->uptime;
  else
    return router->uptime + (now - router->cache_info.published_on);
}

/** Return 1 if <b>router</b> is not suitable for these parameters, else 0.
 * If <b>need_uptime</b> is non-zero, we require a minimum uptime.
 * If <b>need_capacity</b> is non-zero, we require a minimum advertised
 * bandwidth.
 */
static int
dirserv_thinks_router_is_unreliable(time_t now,
                                    routerinfo_t *router,
                                    int need_uptime, int need_capacity)
{
  if (need_uptime) {
    if (!enough_mtbf_info) {
      /* XXX024 Once most authorities are on v3, we should change the rule from
       * "use uptime if we don't have mtbf data" to "don't advertise Stable on
       * v3 if we don't have enough mtbf data."  Or maybe not, since if we ever
       * hit a point where we need to reset a lot of authorities at once,
       * none of them would be in a position to declare Stable.
       */
      long uptime = real_uptime(router, now);
      if ((unsigned)uptime < stable_uptime &&
          (unsigned)uptime < UPTIME_TO_GUARANTEE_STABLE)
        return 1;
    } else {
      double mtbf =
        rep_hist_get_stability(router->cache_info.identity_digest, now);
      if (mtbf < stable_mtbf &&
          mtbf < MTBF_TO_GUARANTEE_STABLE)
        return 1;
    }
  }
  if (need_capacity) {
    uint32_t bw_kb = dirserv_get_credible_bandwidth_kb(router);
    if (bw_kb < fast_bandwidth_kb)
      return 1;
  }
  return 0;
}

/** Return true iff <b>router</b> should be assigned the "HSDir" flag.
 *
 * Right now this means it advertises support for it, it has a high uptime,
 * it's a directory cache, it has the Stable and Fast flags, and it's currently
 * considered Running.
 *
 * This function needs to be called after router-\>is_running has
 * been set.
 */
static int
dirserv_thinks_router_is_hs_dir(const routerinfo_t *router,
                                const node_t *node, time_t now)
{

  long uptime;

  /* If we haven't been running for at least
   * get_options()->MinUptimeHidServDirectoryV2 seconds, we can't
   * have accurate data telling us a relay has been up for at least
   * that long. We also want to allow a bit of slack: Reachability
   * tests aren't instant. If we haven't been running long enough,
   * trust the relay. */

  if (stats_n_seconds_working >
      get_options()->MinUptimeHidServDirectoryV2 * 1.1)
    uptime = MIN(rep_hist_get_uptime(router->cache_info.identity_digest, now),
                 real_uptime(router, now));
  else
    uptime = real_uptime(router, now);

  return (router->wants_to_be_hs_dir &&
          router->supports_tunnelled_dir_requests &&
          node->is_stable && node->is_fast &&
          uptime >= get_options()->MinUptimeHidServDirectoryV2 &&
          router_is_active(router, node, now));
}

/** Don't consider routers with less bandwidth than this when computing
 * thresholds. */
#define ABSOLUTE_MIN_BW_VALUE_TO_CONSIDER_KB 4

/** Helper for dirserv_compute_performance_thresholds(): Decide whether to
 * include a router in our calculations, and return true iff we should; the
 * require_mbw parameter is passed in by
 * dirserv_compute_performance_thresholds() and controls whether we ever
 * count routers with only advertised bandwidths */
static int
router_counts_toward_thresholds(const node_t *node, time_t now,
                                const digestmap_t *omit_as_sybil,
                                int require_mbw)
{
  /* Have measured bw? */
  int have_mbw =
    dirserv_has_measured_bw(node->identity);
  uint64_t min_bw_kb = ABSOLUTE_MIN_BW_VALUE_TO_CONSIDER_KB;
  const or_options_t *options = get_options();

  if (options->TestingTorNetwork) {
    min_bw_kb = (int64_t)options->TestingMinExitFlagThreshold / 1000;
  }

  return node->ri && router_is_active(node->ri, node, now) &&
    !digestmap_get(omit_as_sybil, node->identity) &&
    (dirserv_get_credible_bandwidth_kb(node->ri) >= min_bw_kb) &&
    (have_mbw || !require_mbw);
}

/** Look through the routerlist, the Mean Time Between Failure history, and
 * the Weighted Fractional Uptime history, and use them to set thresholds for
 * the Stable, Fast, and Guard flags.  Update the fields stable_uptime,
 * stable_mtbf, enough_mtbf_info, guard_wfu, guard_tk, fast_bandwidth,
 * guard_bandwidth_including_exits, and guard_bandwidth_excluding_exits.
 *
 * Also, set the is_exit flag of each router appropriately. */
static void
dirserv_compute_performance_thresholds(digestmap_t *omit_as_sybil)
{
  int n_active, n_active_nonexit, n_familiar;
  uint32_t *uptimes, *bandwidths_kb, *bandwidths_excluding_exits_kb;
  long *tks;
  double *mtbfs, *wfus;
  smartlist_t *nodelist;
  time_t now = time(NULL);
  const or_options_t *options = get_options();

  /* Require mbw? */
  int require_mbw =
    (routers_with_measured_bw >
     options->MinMeasuredBWsForAuthToIgnoreAdvertised) ? 1 : 0;

  /* initialize these all here, in case there are no routers */
  stable_uptime = 0;
  stable_mtbf = 0;
  fast_bandwidth_kb = 0;
  guard_bandwidth_including_exits_kb = 0;
  guard_bandwidth_excluding_exits_kb = 0;
  guard_tk = 0;
  guard_wfu = 0;

  nodelist_assert_ok();
  nodelist = nodelist_get_list();

  /* Initialize arrays that will hold values for each router.  We'll
   * sort them and use that to compute thresholds. */
  n_active = n_active_nonexit = 0;
  /* Uptime for every active router. */
  uptimes = tor_calloc(smartlist_len(nodelist), sizeof(uint32_t));
  /* Bandwidth for every active router. */
  bandwidths_kb = tor_calloc(smartlist_len(nodelist), sizeof(uint32_t));
  /* Bandwidth for every active non-exit router. */
  bandwidths_excluding_exits_kb =
    tor_calloc(smartlist_len(nodelist), sizeof(uint32_t));
  /* Weighted mean time between failure for each active router. */
  mtbfs = tor_calloc(smartlist_len(nodelist), sizeof(double));
  /* Time-known for each active router. */
  tks = tor_calloc(smartlist_len(nodelist), sizeof(long));
  /* Weighted fractional uptime for each active router. */
  wfus = tor_calloc(smartlist_len(nodelist), sizeof(double));

  /* Now, fill in the arrays. */
  SMARTLIST_FOREACH_BEGIN(nodelist, node_t *, node) {
    if (options->BridgeAuthoritativeDir &&
        node->ri &&
        node->ri->purpose != ROUTER_PURPOSE_BRIDGE)
      continue;
    if (router_counts_toward_thresholds(node, now, omit_as_sybil,
                                        require_mbw)) {
      routerinfo_t *ri = node->ri;
      const char *id = node->identity;
      uint32_t bw_kb;
      /* resolve spurious clang shallow analysis null pointer errors */
      tor_assert(ri);
      node->is_exit = (!router_exit_policy_rejects_all(ri) &&
                       exit_policy_is_general_exit(ri->exit_policy));
      uptimes[n_active] = (uint32_t)real_uptime(ri, now);
      mtbfs[n_active] = rep_hist_get_stability(id, now);
      tks  [n_active] = rep_hist_get_weighted_time_known(id, now);
      bandwidths_kb[n_active] = bw_kb = dirserv_get_credible_bandwidth_kb(ri);
      if (!node->is_exit || node->is_bad_exit) {
        bandwidths_excluding_exits_kb[n_active_nonexit] = bw_kb;
        ++n_active_nonexit;
      }
      ++n_active;
    }
  } SMARTLIST_FOREACH_END(node);

  /* Now, compute thresholds. */
  if (n_active) {
    /* The median uptime is stable. */
    stable_uptime = median_uint32(uptimes, n_active);
    /* The median mtbf is stable, if we have enough mtbf info */
    stable_mtbf = median_double(mtbfs, n_active);
    /* The 12.5th percentile bandwidth is fast. */
    fast_bandwidth_kb = find_nth_uint32(bandwidths_kb, n_active, n_active/8);
    /* (Now bandwidths is sorted.) */
    if (fast_bandwidth_kb < RELAY_REQUIRED_MIN_BANDWIDTH/(2 * 1000))
      fast_bandwidth_kb = bandwidths_kb[n_active/4];
    guard_bandwidth_including_exits_kb =
      third_quartile_uint32(bandwidths_kb, n_active);
    guard_tk = find_nth_long(tks, n_active, n_active/8);
  }

  if (guard_tk > TIME_KNOWN_TO_GUARANTEE_FAMILIAR)
    guard_tk = TIME_KNOWN_TO_GUARANTEE_FAMILIAR;

  {
    /* We can vote on a parameter for the minimum and maximum. */
#define ABSOLUTE_MIN_VALUE_FOR_FAST_FLAG 4
    int32_t min_fast_kb, max_fast_kb, min_fast, max_fast;
    min_fast = networkstatus_get_param(NULL, "FastFlagMinThreshold",
      ABSOLUTE_MIN_VALUE_FOR_FAST_FLAG,
      ABSOLUTE_MIN_VALUE_FOR_FAST_FLAG,
      INT32_MAX);
    if (options->TestingTorNetwork) {
      min_fast = (int32_t)options->TestingMinFastFlagThreshold;
    }
    max_fast = networkstatus_get_param(NULL, "FastFlagMaxThreshold",
                                       INT32_MAX, min_fast, INT32_MAX);
    min_fast_kb = min_fast / 1000;
    max_fast_kb = max_fast / 1000;

    if (fast_bandwidth_kb < (uint32_t)min_fast_kb)
      fast_bandwidth_kb = min_fast_kb;
    if (fast_bandwidth_kb > (uint32_t)max_fast_kb)
      fast_bandwidth_kb = max_fast_kb;
  }
  /* Protect sufficiently fast nodes from being pushed out of the set
   * of Fast nodes. */
  if (options->AuthDirFastGuarantee &&
      fast_bandwidth_kb > options->AuthDirFastGuarantee/1000)
    fast_bandwidth_kb = (uint32_t)options->AuthDirFastGuarantee/1000;

  /* Now that we have a time-known that 7/8 routers are known longer than,
   * fill wfus with the wfu of every such "familiar" router. */
  n_familiar = 0;

  SMARTLIST_FOREACH_BEGIN(nodelist, node_t *, node) {
      if (router_counts_toward_thresholds(node, now,
                                          omit_as_sybil, require_mbw)) {
        routerinfo_t *ri = node->ri;
        const char *id = ri->cache_info.identity_digest;
        long tk = rep_hist_get_weighted_time_known(id, now);
        if (tk < guard_tk)
          continue;
        wfus[n_familiar++] = rep_hist_get_weighted_fractional_uptime(id, now);
      }
  } SMARTLIST_FOREACH_END(node);
  if (n_familiar)
    guard_wfu = median_double(wfus, n_familiar);
  if (guard_wfu > WFU_TO_GUARANTEE_GUARD)
    guard_wfu = WFU_TO_GUARANTEE_GUARD;

  enough_mtbf_info = rep_hist_have_measured_enough_stability();

  if (n_active_nonexit) {
    guard_bandwidth_excluding_exits_kb =
      find_nth_uint32(bandwidths_excluding_exits_kb,
                      n_active_nonexit, n_active_nonexit*3/4);
  }

  log_info(LD_DIRSERV,
      "Cutoffs: For Stable, %lu sec uptime, %lu sec MTBF. "
      "For Fast: %lu kilobytes/sec. "
      "For Guard: WFU %.03f%%, time-known %lu sec, "
      "and bandwidth %lu or %lu kilobytes/sec. "
      "We%s have enough stability data.",
      (unsigned long)stable_uptime,
      (unsigned long)stable_mtbf,
      (unsigned long)fast_bandwidth_kb,
      guard_wfu*100,
      (unsigned long)guard_tk,
      (unsigned long)guard_bandwidth_including_exits_kb,
      (unsigned long)guard_bandwidth_excluding_exits_kb,
      enough_mtbf_info ? "" : " don't");

  tor_free(uptimes);
  tor_free(mtbfs);
  tor_free(bandwidths_kb);
  tor_free(bandwidths_excluding_exits_kb);
  tor_free(tks);
  tor_free(wfus);
}

/* Use dirserv_compute_performance_thresholds() to compute the thresholds
 * for the status flags, specifically for bridges.
 *
 * This is only called by a Bridge Authority from
 * networkstatus_getinfo_by_purpose().
 */
void
dirserv_compute_bridge_flag_thresholds(void)
{
  digestmap_t *omit_as_sybil = digestmap_new();
  dirserv_compute_performance_thresholds(omit_as_sybil);
  digestmap_free(omit_as_sybil, NULL);
}

/** Measured bandwidth cache entry */
typedef struct mbw_cache_entry_s {
  long mbw_kb;
  time_t as_of;
} mbw_cache_entry_t;

/** Measured bandwidth cache - keys are identity_digests, values are
 * mbw_cache_entry_t *. */
static digestmap_t *mbw_cache = NULL;

/** Store a measured bandwidth cache entry when reading the measured
 * bandwidths file. */
STATIC void
dirserv_cache_measured_bw(const measured_bw_line_t *parsed_line,
                          time_t as_of)
{
  mbw_cache_entry_t *e = NULL;

  tor_assert(parsed_line);

  /* Allocate a cache if we need */
  if (!mbw_cache) mbw_cache = digestmap_new();

  /* Check if we have an existing entry */
  e = digestmap_get(mbw_cache, parsed_line->node_id);
  /* If we do, we can re-use it */
  if (e) {
    /* Check that we really are newer, and update */
    if (as_of > e->as_of) {
      e->mbw_kb = parsed_line->bw_kb;
      e->as_of = as_of;
    }
  } else {
    /* We'll have to insert a new entry */
    e = tor_malloc(sizeof(*e));
    e->mbw_kb = parsed_line->bw_kb;
    e->as_of = as_of;
    digestmap_set(mbw_cache, parsed_line->node_id, e);
  }
}

/** Clear and free the measured bandwidth cache */
STATIC void
dirserv_clear_measured_bw_cache(void)
{
  if (mbw_cache) {
    /* Free the map and all entries */
    digestmap_free(mbw_cache, tor_free_);
    mbw_cache = NULL;
  }
}

/** Scan the measured bandwidth cache and remove expired entries */
STATIC void
dirserv_expire_measured_bw_cache(time_t now)
{

  if (mbw_cache) {
    /* Iterate through the cache and check each entry */
    DIGESTMAP_FOREACH_MODIFY(mbw_cache, k, mbw_cache_entry_t *, e) {
      if (now > e->as_of + MAX_MEASUREMENT_AGE) {
        tor_free(e);
        MAP_DEL_CURRENT(k);
      }
    } DIGESTMAP_FOREACH_END;

    /* Check if we cleared the whole thing and free if so */
    if (digestmap_size(mbw_cache) == 0) {
      digestmap_free(mbw_cache, tor_free_);
      mbw_cache = 0;
    }
  }
}

/** Get the current size of the measured bandwidth cache */
STATIC int
dirserv_get_measured_bw_cache_size(void)
{
  if (mbw_cache) return digestmap_size(mbw_cache);
  else return 0;
}

/** Query the cache by identity digest, return value indicates whether
 * we found it. The bw_out and as_of_out pointers receive the cached
 * bandwidth value and the time it was cached if not NULL. */
STATIC int
dirserv_query_measured_bw_cache_kb(const char *node_id, long *bw_kb_out,
                                   time_t *as_of_out)
{
  mbw_cache_entry_t *v = NULL;
  int rv = 0;

  if (mbw_cache && node_id) {
    v = digestmap_get(mbw_cache, node_id);
    if (v) {
      /* Found something */
      rv = 1;
      if (bw_kb_out) *bw_kb_out = v->mbw_kb;
      if (as_of_out) *as_of_out = v->as_of;
    }
  }

  return rv;
}

/** Predicate wrapper for dirserv_query_measured_bw_cache() */
STATIC int
dirserv_has_measured_bw(const char *node_id)
{
  return dirserv_query_measured_bw_cache_kb(node_id, NULL, NULL);
}

/** Get the best estimate of a router's bandwidth for dirauth purposes,
 * preferring measured to advertised values if available. */

static uint32_t
dirserv_get_bandwidth_for_router_kb(const routerinfo_t *ri)
{
  uint32_t bw_kb = 0;
  /*
   * Yeah, measured bandwidths in measured_bw_line_t are (implicitly
   * signed) longs and the ones router_get_advertised_bandwidth() returns
   * are uint32_t.
   */
  long mbw_kb = 0;

  if (ri) {
    /*
   * * First try to see if we have a measured bandwidth; don't bother with
     * as_of_out here, on the theory that a stale measured bandwidth is still
     * better to trust than an advertised one.
     */
    if (dirserv_query_measured_bw_cache_kb(ri->cache_info.identity_digest,
                                        &mbw_kb, NULL)) {
      /* Got one! */
      bw_kb = (uint32_t)mbw_kb;
    } else {
      /* If not, fall back to advertised */
      bw_kb = router_get_advertised_bandwidth(ri) / 1000;
    }
  }

  return bw_kb;
}

/** Look through the routerlist, and using the measured bandwidth cache count
 * how many measured bandwidths we know.  This is used to decide whether we
 * ever trust advertised bandwidths for purposes of assigning flags. */
static void
dirserv_count_measured_bws(const smartlist_t *routers)
{
  /* Initialize this first */
  routers_with_measured_bw = 0;

  /* Iterate over the routerlist and count measured bandwidths */
  SMARTLIST_FOREACH_BEGIN(routers, const routerinfo_t *, ri) {
    /* Check if we know a measured bandwidth for this one */
    if (dirserv_has_measured_bw(ri->cache_info.identity_digest)) {
      ++routers_with_measured_bw;
    }
  } SMARTLIST_FOREACH_END(ri);
}

/** Return the bandwidth we believe for assigning flags; prefer measured
 * over advertised, and if we have above a threshold quantity of measured
 * bandwidths, we don't want to ever give flags to unmeasured routers, so
 * return 0. */
static uint32_t
dirserv_get_credible_bandwidth_kb(const routerinfo_t *ri)
{
  int threshold;
  uint32_t bw_kb = 0;
  long mbw_kb;

  tor_assert(ri);
  /* Check if we have a measured bandwidth, and check the threshold if not */
  if (!(dirserv_query_measured_bw_cache_kb(ri->cache_info.identity_digest,
                                       &mbw_kb, NULL))) {
    threshold = get_options()->MinMeasuredBWsForAuthToIgnoreAdvertised;
    if (routers_with_measured_bw > threshold) {
      /* Return zero for unmeasured bandwidth if we are above threshold */
      bw_kb = 0;
    } else {
      /* Return an advertised bandwidth otherwise */
      bw_kb = router_get_advertised_bandwidth_capped(ri) / 1000;
    }
  } else {
    /* We have the measured bandwidth in mbw */
    bw_kb = (uint32_t)mbw_kb;
  }

  return bw_kb;
}

/** Give a statement of our current performance thresholds for inclusion
 * in a vote document. */
char *
dirserv_get_flag_thresholds_line(void)
{
  char *result=NULL;
  const int measured_threshold =
    get_options()->MinMeasuredBWsForAuthToIgnoreAdvertised;
  const int enough_measured_bw = routers_with_measured_bw > measured_threshold;

  tor_asprintf(&result,
      "stable-uptime=%lu stable-mtbf=%lu "
      "fast-speed=%lu "
      "guard-wfu=%.03f%% guard-tk=%lu "
      "guard-bw-inc-exits=%lu guard-bw-exc-exits=%lu "
      "enough-mtbf=%d ignoring-advertised-bws=%d",
      (unsigned long)stable_uptime,
      (unsigned long)stable_mtbf,
      (unsigned long)fast_bandwidth_kb*1000,
      guard_wfu*100,
      (unsigned long)guard_tk,
      (unsigned long)guard_bandwidth_including_exits_kb*1000,
      (unsigned long)guard_bandwidth_excluding_exits_kb*1000,
      enough_mtbf_info ? 1 : 0,
      enough_measured_bw ? 1 : 0);

  return result;
}

/** Given a platform string as in a routerinfo_t (possibly null), return a
 * newly allocated version string for a networkstatus document, or NULL if the
 * platform doesn't give a Tor version. */
static char *
version_from_platform(const char *platform)
{
  if (platform && !strcmpstart(platform, "Tor ")) {
    const char *eos = find_whitespace(platform+4);
    if (eos && !strcmpstart(eos, " (r")) {
      /* XXXX Unify this logic with the other version extraction
       * logic in routerparse.c. */
      eos = find_whitespace(eos+1);
    }
    if (eos) {
      return tor_strndup(platform, eos-platform);
    }
  }
  return NULL;
}

/** Helper: write the router-status information in <b>rs</b> into a newly
 * allocated character buffer.  Use the same format as in network-status
 * documents.  If <b>version</b> is non-NULL, add a "v" line for the platform.
 * Return 0 on success, -1 on failure.
 *
 * The format argument has one of the following values:
 *   NS_V2 - Output an entry suitable for a V2 NS opinion document
 *   NS_V3_CONSENSUS - Output the first portion of a V3 NS consensus entry
 *   NS_V3_CONSENSUS_MICRODESC - Output the first portion of a V3 microdesc
 *        consensus entry.
 *   NS_V3_VOTE - Output a complete V3 NS vote. If <b>vrs</b> is present,
 *        it contains additional information for the vote.
 *   NS_CONTROL_PORT - Output a NS document for the control port
 */
char *
routerstatus_format_entry(const routerstatus_t *rs, const char *version,
                          routerstatus_format_type_t format,
                          const vote_routerstatus_t *vrs)
{
  char *summary;
  char *result = NULL;

  char published[ISO_TIME_LEN+1];
  char identity64[BASE64_DIGEST_LEN+1];
  char digest64[BASE64_DIGEST_LEN+1];
  smartlist_t *chunks = smartlist_new();

  format_iso_time(published, rs->published_on);
  digest_to_base64(identity64, rs->identity_digest);
  digest_to_base64(digest64, rs->descriptor_digest);

  smartlist_add_asprintf(chunks,
                   "r %s %s %s%s%s %s %d %d\n",
                   rs->nickname,
                   identity64,
                   (format==NS_V3_CONSENSUS_MICRODESC)?"":digest64,
                   (format==NS_V3_CONSENSUS_MICRODESC)?"":" ",
                   published,
                   fmt_addr32(rs->addr),
                   (int)rs->or_port,
                   (int)rs->dir_port);

  /* TODO: Maybe we want to pass in what we need to build the rest of
   * this here, instead of in the caller. Then we could use the
   * networkstatus_type_t values, with an additional control port value
   * added -MP */

  /* V3 microdesc consensuses don't have "a" lines. */
  if (format == NS_V3_CONSENSUS_MICRODESC)
    goto done;

  /* Possible "a" line. At most one for now. */
  if (!tor_addr_is_null(&rs->ipv6_addr)) {
    smartlist_add_asprintf(chunks, "a %s\n",
                           fmt_addrport(&rs->ipv6_addr, rs->ipv6_orport));
  }

  if (format == NS_V3_CONSENSUS)
    goto done;

  smartlist_add_asprintf(chunks,
                   "s%s%s%s%s%s%s%s%s%s%s\n",
                  /* These must stay in alphabetical order. */
                   rs->is_authority?" Authority":"",
                   rs->is_bad_exit?" BadExit":"",
                   rs->is_exit?" Exit":"",
                   rs->is_fast?" Fast":"",
                   rs->is_possible_guard?" Guard":"",
                   rs->is_hs_dir?" HSDir":"",
                   rs->is_flagged_running?" Running":"",
                   rs->is_stable?" Stable":"",
                   rs->is_v2_dir?" V2Dir":"",
                   rs->is_valid?" Valid":"");

  /* length of "opt v \n" */
#define V_LINE_OVERHEAD 7
  if (version && strlen(version) < MAX_V_LINE_LEN - V_LINE_OVERHEAD) {
    smartlist_add_asprintf(chunks, "v %s\n", version);
  }

  if (format != NS_V2) {
    const routerinfo_t* desc = router_get_by_id_digest(rs->identity_digest);
    uint32_t bw_kb;

    if (format != NS_CONTROL_PORT) {
      /* Blow up more or less nicely if we didn't get anything or not the
       * thing we expected.
       */
      if (!desc) {
        char id[HEX_DIGEST_LEN+1];
        char dd[HEX_DIGEST_LEN+1];

        base16_encode(id, sizeof(id), rs->identity_digest, DIGEST_LEN);
        base16_encode(dd, sizeof(dd), rs->descriptor_digest, DIGEST_LEN);
        log_warn(LD_BUG, "Cannot get any descriptor for %s "
            "(wanted descriptor %s).",
            id, dd);
        goto err;
      }

      /* This assert could fire for the control port, because
       * it can request NS documents before all descriptors
       * have been fetched. Therefore, we only do this test when
       * format != NS_CONTROL_PORT. */
      if (tor_memneq(desc->cache_info.signed_descriptor_digest,
            rs->descriptor_digest,
            DIGEST_LEN)) {
        char rl_d[HEX_DIGEST_LEN+1];
        char rs_d[HEX_DIGEST_LEN+1];
        char id[HEX_DIGEST_LEN+1];

        base16_encode(rl_d, sizeof(rl_d),
            desc->cache_info.signed_descriptor_digest, DIGEST_LEN);
        base16_encode(rs_d, sizeof(rs_d), rs->descriptor_digest, DIGEST_LEN);
        base16_encode(id, sizeof(id), rs->identity_digest, DIGEST_LEN);
        log_err(LD_BUG, "descriptor digest in routerlist does not match "
            "the one in routerstatus: %s vs %s "
            "(router %s)\n",
            rl_d, rs_d, id);

        tor_assert(tor_memeq(desc->cache_info.signed_descriptor_digest,
              rs->descriptor_digest,
              DIGEST_LEN));
      }
    }

    if (format == NS_CONTROL_PORT && rs->has_bandwidth) {
      bw_kb = rs->bandwidth_kb;
    } else {
      tor_assert(desc);
      bw_kb = router_get_advertised_bandwidth_capped(desc) / 1000;
    }
    smartlist_add_asprintf(chunks,
                     "w Bandwidth=%d", bw_kb);

    if (format == NS_V3_VOTE && vrs && vrs->has_measured_bw) {
      smartlist_add_asprintf(chunks,
                       " Measured=%d", vrs->measured_bw_kb);
    }
    /* Write down guardfraction information if we have it. */
    if (format == NS_V3_VOTE && vrs && vrs->status.has_guardfraction) {
      smartlist_add_asprintf(chunks,
                             " GuardFraction=%d",
                             vrs->status.guardfraction_percentage);
    }

    smartlist_add(chunks, tor_strdup("\n"));

    if (desc) {
      summary = policy_summarize(desc->exit_policy, AF_INET);
      smartlist_add_asprintf(chunks, "p %s\n", summary);
      tor_free(summary);
    }

    if (format == NS_V3_VOTE && vrs) {
      if (tor_mem_is_zero((char*)vrs->ed25519_id, ED25519_PUBKEY_LEN)) {
        smartlist_add(chunks, tor_strdup("id ed25519 none\n"));
      } else {
        char ed_b64[BASE64_DIGEST256_LEN+1];
        digest256_to_base64(ed_b64, (const char*)vrs->ed25519_id);
        smartlist_add_asprintf(chunks, "id ed25519 %s\n", ed_b64);
      }
    }
  }

 done:
  result = smartlist_join_strings(chunks, "", 0, NULL);

 err:
  SMARTLIST_FOREACH(chunks, char *, cp, tor_free(cp));
  smartlist_free(chunks);

  return result;
}

/** Helper for sorting: compares two routerinfos first by address, and then by
 * descending order of "usefulness".  (An authority is more useful than a
 * non-authority; a running router is more useful than a non-running router;
 * and a router with more bandwidth is more useful than one with less.)
 **/
static int
compare_routerinfo_by_ip_and_bw_(const void **a, const void **b)
{
  routerinfo_t *first = *(routerinfo_t **)a, *second = *(routerinfo_t **)b;
  int first_is_auth, second_is_auth;
  uint32_t bw_kb_first, bw_kb_second;
  const node_t *node_first, *node_second;
  int first_is_running, second_is_running;

  /* we return -1 if first should appear before second... that is,
   * if first is a better router. */
  if (first->addr < second->addr)
    return -1;
  else if (first->addr > second->addr)
    return 1;

  /* Potentially, this next bit could cause k n lg n memeq calls.  But in
   * reality, we will almost never get here, since addresses will usually be
   * different. */

  first_is_auth =
    router_digest_is_trusted_dir(first->cache_info.identity_digest);
  second_is_auth =
    router_digest_is_trusted_dir(second->cache_info.identity_digest);

  if (first_is_auth && !second_is_auth)
    return -1;
  else if (!first_is_auth && second_is_auth)
    return 1;

  node_first = node_get_by_id(first->cache_info.identity_digest);
  node_second = node_get_by_id(second->cache_info.identity_digest);
  first_is_running = node_first && node_first->is_running;
  second_is_running = node_second && node_second->is_running;

  if (first_is_running && !second_is_running)
    return -1;
  else if (!first_is_running && second_is_running)
    return 1;

  bw_kb_first = dirserv_get_bandwidth_for_router_kb(first);
  bw_kb_second = dirserv_get_bandwidth_for_router_kb(second);

  if (bw_kb_first > bw_kb_second)
     return -1;
  else if (bw_kb_first < bw_kb_second)
    return 1;

  /* They're equal! Compare by identity digest, so there's a
   * deterministic order and we avoid flapping. */
  return fast_memcmp(first->cache_info.identity_digest,
                     second->cache_info.identity_digest,
                     DIGEST_LEN);
}

/** Given a list of routerinfo_t in <b>routers</b>, return a new digestmap_t
 * whose keys are the identity digests of those routers that we're going to
 * exclude for Sybil-like appearance. */
static digestmap_t *
get_possible_sybil_list(const smartlist_t *routers)
{
  const or_options_t *options = get_options();
  digestmap_t *omit_as_sybil;
  smartlist_t *routers_by_ip = smartlist_new();
  uint32_t last_addr;
  int addr_count;
  /* Allow at most this number of Tor servers on a single IP address, ... */
  int max_with_same_addr = options->AuthDirMaxServersPerAddr;
  /* ... unless it's a directory authority, in which case allow more. */
  int max_with_same_addr_on_authority = options->AuthDirMaxServersPerAuthAddr;
  if (max_with_same_addr <= 0)
    max_with_same_addr = INT_MAX;
  if (max_with_same_addr_on_authority <= 0)
    max_with_same_addr_on_authority = INT_MAX;

  smartlist_add_all(routers_by_ip, routers);
  smartlist_sort(routers_by_ip, compare_routerinfo_by_ip_and_bw_);
  omit_as_sybil = digestmap_new();

  last_addr = 0;
  addr_count = 0;
  SMARTLIST_FOREACH_BEGIN(routers_by_ip, routerinfo_t *, ri) {
      if (last_addr != ri->addr) {
        last_addr = ri->addr;
        addr_count = 1;
      } else if (++addr_count > max_with_same_addr) {
        if (!router_addr_is_trusted_dir(ri->addr) ||
            addr_count > max_with_same_addr_on_authority)
          digestmap_set(omit_as_sybil, ri->cache_info.identity_digest, ri);
      }
  } SMARTLIST_FOREACH_END(ri);

  smartlist_free(routers_by_ip);
  return omit_as_sybil;
}

/** If there are entries in <b>routers</b> with exactly the same ed25519 keys,
 * remove the older one.  If they are exactly the same age, remove the one
 * with the greater descriptor digest. May alter the order of the list. */
static void
routers_make_ed_keys_unique(smartlist_t *routers)
{
  routerinfo_t *ri2;
  digest256map_t *by_ed_key = digest256map_new();

  SMARTLIST_FOREACH_BEGIN(routers, routerinfo_t *, ri) {
    ri->omit_from_vote = 0;
    if (ri->cache_info.signing_key_cert == NULL)
      continue; /* No ed key */
    const uint8_t *pk = ri->cache_info.signing_key_cert->signing_key.pubkey;
    if ((ri2 = digest256map_get(by_ed_key, pk))) {
      /* Duplicate; must omit one.  Set the omit_from_vote flag in whichever
       * one has the earlier published_on. */
      const time_t ri_pub = ri->cache_info.published_on;
      const time_t ri2_pub = ri2->cache_info.published_on;
      if (ri2_pub < ri_pub ||
          (ri2_pub == ri_pub &&
           memcmp(ri->cache_info.signed_descriptor_digest,
                  ri2->cache_info.signed_descriptor_digest,DIGEST_LEN)<0)) {
        digest256map_set(by_ed_key, pk, ri);
        ri2->omit_from_vote = 1;
      } else {
        ri->omit_from_vote = 1;
      }
    } else {
      /* Add to map */
      digest256map_set(by_ed_key, pk, ri);
    }
  } SMARTLIST_FOREACH_END(ri);

  digest256map_free(by_ed_key, NULL);

  /* Now remove every router where the omit_from_vote flag got set. */
  SMARTLIST_FOREACH_BEGIN(routers, const routerinfo_t *, ri) {
    if (ri->omit_from_vote) {
      SMARTLIST_DEL_CURRENT(routers, ri);
    }
  } SMARTLIST_FOREACH_END(ri);
}

/** Extract status information from <b>ri</b> and from other authority
 * functions and store it in <b>rs</b>>.
 *
 * We assume that ri-\>is_running has already been set, e.g. by
 *   dirserv_set_router_is_running(ri, now);
 */
void
set_routerstatus_from_routerinfo(routerstatus_t *rs,
                                 node_t *node,
                                 routerinfo_t *ri,
                                 time_t now,
                                 int listbadexits)
{
  const or_options_t *options = get_options();
  uint32_t routerbw_kb = dirserv_get_credible_bandwidth_kb(ri);

  memset(rs, 0, sizeof(routerstatus_t));

  rs->is_authority =
    router_digest_is_trusted_dir(ri->cache_info.identity_digest);

  /* Already set by compute_performance_thresholds. */
  rs->is_exit = node->is_exit;
  rs->is_stable = node->is_stable =
    !dirserv_thinks_router_is_unreliable(now, ri, 1, 0);
  rs->is_fast = node->is_fast =
    !dirserv_thinks_router_is_unreliable(now, ri, 0, 1);
  rs->is_flagged_running = node->is_running; /* computed above */

  rs->is_valid = node->is_valid;

  if (node->is_fast &&
      ((options->AuthDirGuardBWGuarantee &&
        routerbw_kb >= options->AuthDirGuardBWGuarantee/1000) ||
       routerbw_kb >= MIN(guard_bandwidth_including_exits_kb,
                          guard_bandwidth_excluding_exits_kb))) {
    long tk = rep_hist_get_weighted_time_known(
                                      node->identity, now);
    double wfu = rep_hist_get_weighted_fractional_uptime(
                                      node->identity, now);
    rs->is_possible_guard = (wfu >= guard_wfu && tk >= guard_tk) ? 1 : 0;
  } else {
    rs->is_possible_guard = 0;
  }

  rs->is_bad_exit = listbadexits && node->is_bad_exit;
  rs->is_hs_dir = node->is_hs_dir =
    dirserv_thinks_router_is_hs_dir(ri, node, now);

  rs->is_named = rs->is_unnamed = 0;

  rs->published_on = ri->cache_info.published_on;
  memcpy(rs->identity_digest, node->identity, DIGEST_LEN);
  memcpy(rs->descriptor_digest, ri->cache_info.signed_descriptor_digest,
         DIGEST_LEN);
  rs->addr = ri->addr;
  strlcpy(rs->nickname, ri->nickname, sizeof(rs->nickname));
  rs->or_port = ri->or_port;
  rs->dir_port = ri->dir_port;
  rs->is_v2_dir = ri->supports_tunnelled_dir_requests;
  if (options->AuthDirHasIPv6Connectivity == 1 &&
      !tor_addr_is_null(&ri->ipv6_addr) &&
      node->last_reachable6 >= now - REACHABLE_TIMEOUT) {
    /* We're configured as having IPv6 connectivity. There's an IPv6
       OR port and it's reachable so copy it to the routerstatus.  */
    tor_addr_copy(&rs->ipv6_addr, &ri->ipv6_addr);
    rs->ipv6_orport = ri->ipv6_orport;
  }

  if (options->TestingTorNetwork) {
    dirserv_set_routerstatus_testing(rs);
  }
}

/** Use TestingDirAuthVoteExit, TestingDirAuthVoteGuard, and
 * TestingDirAuthVoteHSDir to give out the Exit, Guard, and HSDir flags,
 * respectively. But don't set the corresponding node flags.
 * Should only be called if TestingTorNetwork is set. */
STATIC void
dirserv_set_routerstatus_testing(routerstatus_t *rs)
{
  const or_options_t *options = get_options();

  tor_assert(options->TestingTorNetwork);

  if (routerset_contains_routerstatus(options->TestingDirAuthVoteExit,
                                      rs, 0)) {
    rs->is_exit = 1;
  } else if (options->TestingDirAuthVoteExitIsStrict) {
    rs->is_exit = 0;
  }

  if (routerset_contains_routerstatus(options->TestingDirAuthVoteGuard,
                                      rs, 0)) {
    rs->is_possible_guard = 1;
  } else if (options->TestingDirAuthVoteGuardIsStrict) {
    rs->is_possible_guard = 0;
  }

  if (routerset_contains_routerstatus(options->TestingDirAuthVoteHSDir,
                                      rs, 0)) {
    rs->is_hs_dir = 1;
  } else if (options->TestingDirAuthVoteHSDirIsStrict) {
    rs->is_hs_dir = 0;
  }
}

/** Routerstatus <b>rs</b> is part of a group of routers that are on
 * too narrow an IP-space. Clear out its flags: we don't want people
 * using it.
 */
static void
clear_status_flags_on_sybil(routerstatus_t *rs)
{
  rs->is_authority = rs->is_exit = rs->is_stable = rs->is_fast =
    rs->is_flagged_running = rs->is_named = rs->is_valid =
    rs->is_hs_dir = rs->is_possible_guard = rs->is_bad_exit = 0;
  /* FFFF we might want some mechanism to check later on if we
   * missed zeroing any flags: it's easy to add a new flag but
   * forget to add it to this clause. */
}

/** The guardfraction of the guard with identity fingerprint <b>guard_id</b>
 *  is <b>guardfraction_percentage</b>. See if we have a vote routerstatus for
 *  this guard in <b>vote_routerstatuses</b>, and if we do, register the
 *  information to it.
 *
 *  Return 1 if we applied the information and 0 if we couldn't find a
 *  matching guard.
 *
 * Requires that <b>vote_routerstatuses</b> be sorted.
 */
static int
guardfraction_line_apply(const char *guard_id,
                      uint32_t guardfraction_percentage,
                      smartlist_t *vote_routerstatuses)
{
  vote_routerstatus_t *vrs = NULL;

  tor_assert(vote_routerstatuses);

  vrs = smartlist_bsearch(vote_routerstatuses, guard_id,
                         compare_digest_to_vote_routerstatus_entry);

  if (!vrs) {
    return 0;
  }

  vrs->status.has_guardfraction = 1;
  vrs->status.guardfraction_percentage = guardfraction_percentage;

  return 1;
}

/* Given a guard line from a guardfraction file, parse it and register
 * its information to <b>vote_routerstatuses</b>.
 *
 * Return:
 * * 1 if the line was proper and its information got registered.
 * * 0 if the line was proper but no currently active guard was found
 *     to register the guardfraction information to.
 * * -1 if the line could not be parsed and set <b>err_msg</b> to a
      newly allocated string containing the error message.
 */
static int
guardfraction_file_parse_guard_line(const char *guard_line,
                                    smartlist_t *vote_routerstatuses,
                                    char **err_msg)
{
  char guard_id[DIGEST_LEN];
  uint32_t guardfraction;
  char *inputs_tmp = NULL;
  int num_ok = 1;

  smartlist_t *sl = smartlist_new();
  int retval = -1;

  tor_assert(err_msg);

  /* guard_line should contain something like this:
     <hex digest> <guardfraction> <appearances> */
  smartlist_split_string(sl, guard_line, " ",
                         SPLIT_SKIP_SPACE|SPLIT_IGNORE_BLANK, 3);
  if (smartlist_len(sl) < 3) {
    tor_asprintf(err_msg, "bad line '%s'", guard_line);
    goto done;
  }

  inputs_tmp = smartlist_get(sl, 0);
  if (strlen(inputs_tmp) != HEX_DIGEST_LEN ||
      base16_decode(guard_id, DIGEST_LEN, inputs_tmp, HEX_DIGEST_LEN)) {
    tor_asprintf(err_msg, "bad digest '%s'", inputs_tmp);
    goto done;
  }

  inputs_tmp = smartlist_get(sl, 1);
  /* Guardfraction is an integer in [0, 100]. */
  guardfraction =
    (uint32_t) tor_parse_long(inputs_tmp, 10, 0, 100, &num_ok, NULL);
  if (!num_ok) {
    tor_asprintf(err_msg, "wrong percentage '%s'", inputs_tmp);
    goto done;
  }

  /* If routerstatuses were provided, apply this info to actual routers. */
  if (vote_routerstatuses) {
    retval = guardfraction_line_apply(guard_id, guardfraction,
                                      vote_routerstatuses);
  } else {
    retval = 0; /* If we got this far, line was correctly formatted. */
  }

 done:

  SMARTLIST_FOREACH(sl, char *, cp, tor_free(cp));
  smartlist_free(sl);

  return retval;
}

/** Given an inputs line from a guardfraction file, parse it and
 *  register its information to <b>total_consensuses</b> and
 *  <b>total_days</b>.
 *
 *  Return 0 if it parsed well. Return -1 if there was an error, and
 *  set <b>err_msg</b> to a newly allocated string containing the
 *  error message.
 */
static int
guardfraction_file_parse_inputs_line(const char *inputs_line,
                                     int *total_consensuses,
                                     int *total_days,
                                     char **err_msg)
{
  int retval = -1;
  char *inputs_tmp = NULL;
  int num_ok = 1;
  smartlist_t *sl = smartlist_new();

  tor_assert(err_msg);

  /* Second line is inputs information:
   *   n-inputs <total_consensuses> <total_days>. */
  smartlist_split_string(sl, inputs_line, " ",
                         SPLIT_SKIP_SPACE|SPLIT_IGNORE_BLANK, 3);
  if (smartlist_len(sl) < 2) {
    tor_asprintf(err_msg, "incomplete line '%s'", inputs_line);
    goto done;
  }

  inputs_tmp = smartlist_get(sl, 0);
  *total_consensuses =
    (int) tor_parse_long(inputs_tmp, 10, 0, INT_MAX, &num_ok, NULL);
  if (!num_ok) {
    tor_asprintf(err_msg, "unparseable consensus '%s'", inputs_tmp);
    goto done;
  }

  inputs_tmp = smartlist_get(sl, 1);
  *total_days =
    (int) tor_parse_long(inputs_tmp, 10, 0, INT_MAX, &num_ok, NULL);
  if (!num_ok) {
    tor_asprintf(err_msg, "unparseable days '%s'", inputs_tmp);
    goto done;
  }

  retval = 0;

 done:
  SMARTLIST_FOREACH(sl, char *, cp, tor_free(cp));
  smartlist_free(sl);

  return retval;
}

/* Maximum age of a guardfraction file that we are willing to accept. */
#define MAX_GUARDFRACTION_FILE_AGE (7*24*60*60) /* approx a week */

/** Static strings of guardfraction files. */
#define GUARDFRACTION_DATE_STR "written-at"
#define GUARDFRACTION_INPUTS "n-inputs"
#define GUARDFRACTION_GUARD "guard-seen"
#define GUARDFRACTION_VERSION "guardfraction-file-version"

/** Given a guardfraction file in a string, parse it and register the
 *  guardfraction information to the provided vote routerstatuses.
 *
 *  This is the rough format of the guardfraction file:
 *
 *      guardfraction-file-version 1
 *      written-at <date and time>
 *      n-inputs <number of consesuses parsed> <number of days considered>
 *
 *      guard-seen <fpr 1> <guardfraction percentage> <consensus appearances>
 *      guard-seen <fpr 2> <guardfraction percentage> <consensus appearances>
 *      guard-seen <fpr 3> <guardfraction percentage> <consensus appearances>
 *      guard-seen <fpr 4> <guardfraction percentage> <consensus appearances>
 *      guard-seen <fpr 5> <guardfraction percentage> <consensus appearances>
 *      ...
 *
 *  Return -1 if the parsing failed and 0 if it went smoothly. Parsing
 *  should tolerate errors in all lines but the written-at header.
 */
STATIC int
dirserv_read_guardfraction_file_from_str(const char *guardfraction_file_str,
                                      smartlist_t *vote_routerstatuses)
{
  config_line_t *front=NULL, *line;
  int ret_tmp;
  int retval = -1;
  int current_line_n = 0; /* line counter for better log messages */

  /* Guardfraction info to be parsed */
  int total_consensuses = 0;
  int total_days = 0;

  /* Stats */
  int guards_read_n = 0;
  int guards_applied_n = 0;

  /* Parse file and split it in lines */
  ret_tmp = config_get_lines(guardfraction_file_str, &front, 0);
  if (ret_tmp < 0) {
    log_warn(LD_CONFIG, "Error reading from guardfraction file");
    goto done;
  }

  /* Sort routerstatuses (needed later when applying guardfraction info) */
  if (vote_routerstatuses)
    smartlist_sort(vote_routerstatuses, compare_vote_routerstatus_entries);

  for (line = front; line; line=line->next) {
    current_line_n++;

    if (!strcmp(line->key, GUARDFRACTION_VERSION)) {
      int num_ok = 1;
      unsigned int version;

      version =
        (unsigned int) tor_parse_long(line->value,
                                      10, 0, INT_MAX, &num_ok, NULL);

      if (!num_ok || version != 1) {
        log_warn(LD_GENERAL, "Got unknown guardfraction version %d.", version);
        goto done;
      }
    } else if (!strcmp(line->key, GUARDFRACTION_DATE_STR)) {
      time_t file_written_at;
      time_t now = time(NULL);

      /* First line is 'written-at <date>' */
      if (parse_iso_time(line->value, &file_written_at) < 0) {
        log_warn(LD_CONFIG, "Guardfraction:%d: Bad date '%s'. Ignoring",
                 current_line_n, line->value);
        goto done; /* don't tolerate failure here. */
      }
      if (file_written_at < now - MAX_GUARDFRACTION_FILE_AGE) {
        log_warn(LD_CONFIG, "Guardfraction:%d: was written very long ago '%s'",
                 current_line_n, line->value);
        goto done; /* don't tolerate failure here. */
      }
    } else if (!strcmp(line->key, GUARDFRACTION_INPUTS)) {
      char *err_msg = NULL;

      if (guardfraction_file_parse_inputs_line(line->value,
                                               &total_consensuses,
                                               &total_days,
                                               &err_msg) < 0) {
        log_warn(LD_CONFIG, "Guardfraction:%d: %s",
                 current_line_n, err_msg);
        tor_free(err_msg);
        continue;
      }

    } else if (!strcmp(line->key, GUARDFRACTION_GUARD)) {
      char *err_msg = NULL;

      ret_tmp = guardfraction_file_parse_guard_line(line->value,
                                                    vote_routerstatuses,
                                                    &err_msg);
      if (ret_tmp < 0) { /* failed while parsing the guard line */
        log_warn(LD_CONFIG, "Guardfraction:%d: %s",
                 current_line_n, err_msg);
        tor_free(err_msg);
        continue;
      }

      /* Successfully parsed guard line. Check if it was applied properly. */
      guards_read_n++;
      if (ret_tmp > 0) {
        guards_applied_n++;
      }
    } else {
      log_warn(LD_CONFIG, "Unknown guardfraction line %d (%s %s)",
               current_line_n, line->key, line->value);
    }
  }

  retval = 0;

  log_info(LD_CONFIG,
           "Successfully parsed guardfraction file with %d consensuses over "
           "%d days. Parsed %d nodes and applied %d of them%s.",
           total_consensuses, total_days, guards_read_n, guards_applied_n,
           vote_routerstatuses ? "" : " (no routerstatus provided)" );

 done:
  config_free_lines(front);

  if (retval < 0) {
    return retval;
  } else {
    return guards_read_n;
  }
}

/** Read a guardfraction file at <b>fname</b> and load all its
 *  information to <b>vote_routerstatuses</b>. */
int
dirserv_read_guardfraction_file(const char *fname,
                             smartlist_t *vote_routerstatuses)
{
  char *guardfraction_file_str;

  /* Read file to a string */
  guardfraction_file_str = read_file_to_str(fname, RFTS_IGNORE_MISSING, NULL);
  if (!guardfraction_file_str) {
      log_warn(LD_FS, "Cannot open guardfraction file '%s'. Failing.", fname);
      return -1;
  }

  return dirserv_read_guardfraction_file_from_str(guardfraction_file_str,
                                               vote_routerstatuses);
}

/**
 * Helper function to parse out a line in the measured bandwidth file
 * into a measured_bw_line_t output structure. Returns -1 on failure
 * or 0 on success.
 */
STATIC int
measured_bw_line_parse(measured_bw_line_t *out, const char *orig_line)
{
  char *line = tor_strdup(orig_line);
  char *cp = line;
  int got_bw = 0;
  int got_node_id = 0;
  char *strtok_state; /* lame sauce d'jour */
  cp = tor_strtok_r(cp, " \t", &strtok_state);

  if (!cp) {
    log_warn(LD_DIRSERV, "Invalid line in bandwidth file: %s",
             escaped(orig_line));
    tor_free(line);
    return -1;
  }

  if (orig_line[strlen(orig_line)-1] != '\n') {
    log_warn(LD_DIRSERV, "Incomplete line in bandwidth file: %s",
             escaped(orig_line));
    tor_free(line);
    return -1;
  }

  do {
    if (strcmpstart(cp, "bw=") == 0) {
      int parse_ok = 0;
      char *endptr;
      if (got_bw) {
        log_warn(LD_DIRSERV, "Double bw= in bandwidth file line: %s",
                 escaped(orig_line));
        tor_free(line);
        return -1;
      }
      cp+=strlen("bw=");

      out->bw_kb = tor_parse_long(cp, 0, 0, LONG_MAX, &parse_ok, &endptr);
      if (!parse_ok || (*endptr && !TOR_ISSPACE(*endptr))) {
        log_warn(LD_DIRSERV, "Invalid bandwidth in bandwidth file line: %s",
                 escaped(orig_line));
        tor_free(line);
        return -1;
      }
      got_bw=1;
    } else if (strcmpstart(cp, "node_id=$") == 0) {
      if (got_node_id) {
        log_warn(LD_DIRSERV, "Double node_id= in bandwidth file line: %s",
                 escaped(orig_line));
        tor_free(line);
        return -1;
      }
      cp+=strlen("node_id=$");

      if (strlen(cp) != HEX_DIGEST_LEN ||
          base16_decode(out->node_id, DIGEST_LEN, cp, HEX_DIGEST_LEN)) {
        log_warn(LD_DIRSERV, "Invalid node_id in bandwidth file line: %s",
                 escaped(orig_line));
        tor_free(line);
        return -1;
      }
      strlcpy(out->node_hex, cp, sizeof(out->node_hex));
      got_node_id=1;
    }
  } while ((cp = tor_strtok_r(NULL, " \t", &strtok_state)));

  if (got_bw && got_node_id) {
    tor_free(line);
    return 0;
  } else {
    log_warn(LD_DIRSERV, "Incomplete line in bandwidth file: %s",
             escaped(orig_line));
    tor_free(line);
    return -1;
  }
}

/**
 * Helper function to apply a parsed measurement line to a list
 * of bandwidth statuses. Returns true if a line is found,
 * false otherwise.
 */
STATIC int
measured_bw_line_apply(measured_bw_line_t *parsed_line,
                       smartlist_t *routerstatuses)
{
  vote_routerstatus_t *rs = NULL;
  if (!routerstatuses)
    return 0;

  rs = smartlist_bsearch(routerstatuses, parsed_line->node_id,
                         compare_digest_to_vote_routerstatus_entry);

  if (rs) {
    rs->has_measured_bw = 1;
    rs->measured_bw_kb = (uint32_t)parsed_line->bw_kb;
  } else {
    log_info(LD_DIRSERV, "Node ID %s not found in routerstatus list",
             parsed_line->node_hex);
  }

  return rs != NULL;
}

/**
 * Read the measured bandwidth file and apply it to the list of
 * vote_routerstatus_t. Returns -1 on error, 0 otherwise.
 */
int
dirserv_read_measured_bandwidths(const char *from_file,
                                 smartlist_t *routerstatuses)
{
  char line[512];
  FILE *fp = tor_fopen_cloexec(from_file, "r");
  int applied_lines = 0;
  time_t file_time, now;
  int ok;

  if (fp == NULL) {
    log_warn(LD_CONFIG, "Can't open bandwidth file at configured location: %s",
             from_file);
    return -1;
  }

  if (!fgets(line, sizeof(line), fp)
          || !strlen(line) || line[strlen(line)-1] != '\n') {
    log_warn(LD_DIRSERV, "Long or truncated time in bandwidth file: %s",
             escaped(line));
    fclose(fp);
    return -1;
  }

  line[strlen(line)-1] = '\0';
  file_time = (time_t)tor_parse_ulong(line, 10, 0, ULONG_MAX, &ok, NULL);
  if (!ok) {
    log_warn(LD_DIRSERV, "Non-integer time in bandwidth file: %s",
             escaped(line));
    fclose(fp);
    return -1;
  }

  now = time(NULL);
  if ((now - file_time) > MAX_MEASUREMENT_AGE) {
    log_warn(LD_DIRSERV, "Bandwidth measurement file stale. Age: %u",
             (unsigned)(time(NULL) - file_time));
    fclose(fp);
    return -1;
  }

  if (routerstatuses)
    smartlist_sort(routerstatuses, compare_vote_routerstatus_entries);

  while (!feof(fp)) {
    measured_bw_line_t parsed_line;
    if (fgets(line, sizeof(line), fp) && strlen(line)) {
      if (measured_bw_line_parse(&parsed_line, line) != -1) {
        /* Also cache the line for dirserv_get_bandwidth_for_router() */
        dirserv_cache_measured_bw(&parsed_line, file_time);
        if (measured_bw_line_apply(&parsed_line, routerstatuses) > 0)
          applied_lines++;
      }
    }
  }

  /* Now would be a nice time to clean the cache, too */
  dirserv_expire_measured_bw_cache(now);

  fclose(fp);
  log_info(LD_DIRSERV,
           "Bandwidth measurement file successfully read. "
           "Applied %d measurements.", applied_lines);
  return 0;
}

/** Return a new networkstatus_t* containing our current opinion. (For v3
 * authorities) */
networkstatus_t *
dirserv_generate_networkstatus_vote_obj(crypto_pk_t *private_key,
                                        authority_cert_t *cert)
{
  const or_options_t *options = get_options();
  networkstatus_t *v3_out = NULL;
  uint32_t addr;
  char *hostname = NULL, *client_versions = NULL, *server_versions = NULL;
  const char *contact;
  smartlist_t *routers, *routerstatuses;
  char identity_digest[DIGEST_LEN];
  char signing_key_digest[DIGEST_LEN];
  int listbadexits = options->AuthDirListBadExits;
  routerlist_t *rl = router_get_routerlist();
  time_t now = time(NULL);
  time_t cutoff = now - ROUTER_MAX_AGE_TO_PUBLISH;
  networkstatus_voter_info_t *voter = NULL;
  vote_timing_t timing;
  digestmap_t *omit_as_sybil = NULL;
  const int vote_on_reachability = running_long_enough_to_decide_unreachable();
  smartlist_t *microdescriptors = NULL;

  tor_assert(private_key);
  tor_assert(cert);

  if (crypto_pk_get_digest(private_key, signing_key_digest)<0) {
    log_err(LD_BUG, "Error computing signing key digest");
    return NULL;
  }
  if (crypto_pk_get_digest(cert->identity_key, identity_digest)<0) {
    log_err(LD_BUG, "Error computing identity key digest");
    return NULL;
  }
  if (resolve_my_address(LOG_WARN, options, &addr, NULL, &hostname)<0) {
    log_warn(LD_NET, "Couldn't resolve my hostname");
    return NULL;
  }
  if (!hostname || !strchr(hostname, '.')) {
    tor_free(hostname);
    hostname = tor_dup_ip(addr);
  }

  if (options->VersioningAuthoritativeDir) {
    client_versions = format_versions_list(options->RecommendedClientVersions);
    server_versions = format_versions_list(options->RecommendedServerVersions);
  }

  contact = get_options()->ContactInfo;
  if (!contact)
    contact = "(none)";

  /*
   * Do this so dirserv_compute_performance_thresholds() and
   * set_routerstatus_from_routerinfo() see up-to-date bandwidth info.
   */
  if (options->V3BandwidthsFile) {
    dirserv_read_measured_bandwidths(options->V3BandwidthsFile, NULL);
  } else {
    /*
     * No bandwidths file; clear the measured bandwidth cache in case we had
     * one last time around.
     */
    if (dirserv_get_measured_bw_cache_size() > 0) {
      dirserv_clear_measured_bw_cache();
    }
  }

  /* precompute this part, since we need it to decide what "stable"
   * means. */
  SMARTLIST_FOREACH(rl->routers, routerinfo_t *, ri, {
    dirserv_set_router_is_running(ri, now);
  });

  routers = smartlist_new();
  smartlist_add_all(routers, rl->routers);
  routers_make_ed_keys_unique(routers);
  /* After this point, don't use rl->routers; use 'routers' instead. */
  routers_sort_by_identity(routers);
  omit_as_sybil = get_possible_sybil_list(routers);

  DIGESTMAP_FOREACH(omit_as_sybil, sybil_id, void *, ignore) {
    (void) ignore;
    rep_hist_make_router_pessimal(sybil_id, now);
  } DIGESTMAP_FOREACH_END;

  /* Count how many have measured bandwidths so we know how to assign flags;
   * this must come before dirserv_compute_performance_thresholds() */
  dirserv_count_measured_bws(routers);

  dirserv_compute_performance_thresholds(omit_as_sybil);

  routerstatuses = smartlist_new();
  microdescriptors = smartlist_new();

  SMARTLIST_FOREACH_BEGIN(routers, routerinfo_t *, ri) {
    if (ri->cache_info.published_on >= cutoff) {
      routerstatus_t *rs;
      vote_routerstatus_t *vrs;
      node_t *node = node_get_mutable_by_id(ri->cache_info.identity_digest);
      if (!node)
        continue;

      vrs = tor_malloc_zero(sizeof(vote_routerstatus_t));
      rs = &vrs->status;
      set_routerstatus_from_routerinfo(rs, node, ri, now,
                                       listbadexits);

      if (ri->cache_info.signing_key_cert) {
        memcpy(vrs->ed25519_id,
               ri->cache_info.signing_key_cert->signing_key.pubkey,
               ED25519_PUBKEY_LEN);
      }

      if (digestmap_get(omit_as_sybil, ri->cache_info.identity_digest))
        clear_status_flags_on_sybil(rs);

      if (!vote_on_reachability)
        rs->is_flagged_running = 0;

      vrs->version = version_from_platform(ri->platform);
      vrs->microdesc = dirvote_format_all_microdesc_vote_lines(ri, now,
                                                        microdescriptors);

      smartlist_add(routerstatuses, vrs);
    }
  } SMARTLIST_FOREACH_END(ri);

  {
    smartlist_t *added =
      microdescs_add_list_to_cache(get_microdesc_cache(),
                                   microdescriptors, SAVED_NOWHERE, 0);
    smartlist_free(added);
    smartlist_free(microdescriptors);
  }

  smartlist_free(routers);
  digestmap_free(omit_as_sybil, NULL);

  /* Apply guardfraction information to routerstatuses. */
  if (options->GuardfractionFile) {
    dirserv_read_guardfraction_file(options->GuardfractionFile,
                                    routerstatuses);
  }

  /* This pass through applies the measured bw lines to the routerstatuses */
  if (options->V3BandwidthsFile) {
    dirserv_read_measured_bandwidths(options->V3BandwidthsFile,
                                     routerstatuses);
  } else {
    /*
     * No bandwidths file; clear the measured bandwidth cache in case we had
     * one last time around.
     */
    if (dirserv_get_measured_bw_cache_size() > 0) {
      dirserv_clear_measured_bw_cache();
    }
  }

  v3_out = tor_malloc_zero(sizeof(networkstatus_t));

  v3_out->type = NS_TYPE_VOTE;
  dirvote_get_preferred_voting_intervals(&timing);
  v3_out->published = now;
  {
    char tbuf[ISO_TIME_LEN+1];
    networkstatus_t *current_consensus =
      networkstatus_get_live_consensus(now);
    long last_consensus_interval; /* only used to pick a valid_after */
    if (current_consensus)
      last_consensus_interval = current_consensus->fresh_until -
        current_consensus->valid_after;
    else
      last_consensus_interval = options->TestingV3AuthInitialVotingInterval;
    v3_out->valid_after =
      dirvote_get_start_of_next_interval(now, (int)last_consensus_interval,
                                      options->TestingV3AuthVotingStartOffset);
    format_iso_time(tbuf, v3_out->valid_after);
    log_notice(LD_DIR,"Choosing valid-after time in vote as %s: "
               "consensus_set=%d, last_interval=%d",
               tbuf, current_consensus?1:0, (int)last_consensus_interval);
  }
  v3_out->fresh_until = v3_out->valid_after + timing.vote_interval;
  v3_out->valid_until = v3_out->valid_after +
    (timing.vote_interval * timing.n_intervals_valid);
  v3_out->vote_seconds = timing.vote_delay;
  v3_out->dist_seconds = timing.dist_delay;
  tor_assert(v3_out->vote_seconds > 0);
  tor_assert(v3_out->dist_seconds > 0);
  tor_assert(timing.n_intervals_valid > 0);

  v3_out->client_versions = client_versions;
  v3_out->server_versions = server_versions;
  v3_out->package_lines = smartlist_new();
  {
    config_line_t *cl;
    for (cl = get_options()->RecommendedPackages; cl; cl = cl->next) {
      if (validate_recommended_package_line(cl->value))
        smartlist_add(v3_out->package_lines, tor_strdup(cl->value));
    }
  }

  v3_out->known_flags = smartlist_new();
  smartlist_split_string(v3_out->known_flags,
                "Authority Exit Fast Guard Stable V2Dir Valid HSDir",
                0, SPLIT_SKIP_SPACE|SPLIT_IGNORE_BLANK, 0);
  if (vote_on_reachability)
    smartlist_add(v3_out->known_flags, tor_strdup("Running"));
  if (listbadexits)
    smartlist_add(v3_out->known_flags, tor_strdup("BadExit"));
  smartlist_sort_strings(v3_out->known_flags);

  if (options->ConsensusParams) {
    v3_out->net_params = smartlist_new();
    smartlist_split_string(v3_out->net_params,
                           options->ConsensusParams, NULL, 0, 0);
    smartlist_sort_strings(v3_out->net_params);
  }

  voter = tor_malloc_zero(sizeof(networkstatus_voter_info_t));
  voter->nickname = tor_strdup(options->Nickname);
  memcpy(voter->identity_digest, identity_digest, DIGEST_LEN);
  voter->sigs = smartlist_new();
  voter->address = hostname;
  voter->addr = addr;
  voter->dir_port = router_get_advertised_dir_port(options, 0);
  voter->or_port = router_get_advertised_or_port(options);
  voter->contact = tor_strdup(contact);
  if (options->V3AuthUseLegacyKey) {
    authority_cert_t *c = get_my_v3_legacy_cert();
    if (c) {
      if (crypto_pk_get_digest(c->identity_key, voter->legacy_id_digest)) {
        log_warn(LD_BUG, "Unable to compute digest of legacy v3 identity key");
        memset(voter->legacy_id_digest, 0, DIGEST_LEN);
      }
    }
  }

  v3_out->voters = smartlist_new();
  smartlist_add(v3_out->voters, voter);
  v3_out->cert = authority_cert_dup(cert);
  v3_out->routerstatus_list = routerstatuses;
  /* Note: networkstatus_digest is unset; it won't get set until we actually
   * format the vote. */

  return v3_out;
}

/** As dirserv_get_routerdescs(), but instead of getting signed_descriptor_t
 * pointers, adds copies of digests to fps_out, and doesn't use the
 * /tor/server/ prefix.  For a /d/ request, adds descriptor digests; for other
 * requests, adds identity digests.
 */
int
dirserv_get_routerdesc_fingerprints(smartlist_t *fps_out, const char *key,
                                    const char **msg, int for_unencrypted_conn,
                                    int is_extrainfo)
{
  int by_id = 1;
  *msg = NULL;

  if (!strcmp(key, "all")) {
    routerlist_t *rl = router_get_routerlist();
    SMARTLIST_FOREACH(rl->routers, routerinfo_t *, r,
                      smartlist_add(fps_out,
                      tor_memdup(r->cache_info.identity_digest, DIGEST_LEN)));
    /* Treat "all" requests as if they were unencrypted */
    for_unencrypted_conn = 1;
  } else if (!strcmp(key, "authority")) {
    const routerinfo_t *ri = router_get_my_routerinfo();
    if (ri)
      smartlist_add(fps_out,
                    tor_memdup(ri->cache_info.identity_digest, DIGEST_LEN));
  } else if (!strcmpstart(key, "d/")) {
    by_id = 0;
    key += strlen("d/");
    dir_split_resource_into_fingerprints(key, fps_out, NULL,
                                         DSR_HEX|DSR_SORT_UNIQ);
  } else if (!strcmpstart(key, "fp/")) {
    key += strlen("fp/");
    dir_split_resource_into_fingerprints(key, fps_out, NULL,
                                         DSR_HEX|DSR_SORT_UNIQ);
  } else {
    *msg = "Key not recognized";
    return -1;
  }

  if (for_unencrypted_conn) {
    /* Remove anything that insists it not be sent unencrypted. */
    SMARTLIST_FOREACH_BEGIN(fps_out, char *, cp) {
        const signed_descriptor_t *sd;
        if (by_id)
          sd = get_signed_descriptor_by_fp(cp,is_extrainfo,0);
        else if (is_extrainfo)
          sd = extrainfo_get_by_descriptor_digest(cp);
        else
          sd = router_get_by_descriptor_digest(cp);
        if (sd && !sd->send_unencrypted) {
          tor_free(cp);
          SMARTLIST_DEL_CURRENT(fps_out, cp);
        }
    } SMARTLIST_FOREACH_END(cp);
  }

  if (!smartlist_len(fps_out)) {
    *msg = "Servers unavailable";
    return -1;
  }
  return 0;
}

/** Add a signed_descriptor_t to <b>descs_out</b> for each router matching
 * <b>key</b>.  The key should be either
 *   - "/tor/server/authority" for our own routerinfo;
 *   - "/tor/server/all" for all the routerinfos we have, concatenated;
 *   - "/tor/server/fp/FP" where FP is a plus-separated sequence of
 *     hex identity digests; or
 *   - "/tor/server/d/D" where D is a plus-separated sequence
 *     of server descriptor digests, in hex.
 *
 * Return 0 if we found some matching descriptors, or -1 if we do not
 * have any descriptors, no matching descriptors, or if we did not
 * recognize the key (URL).
 * If -1 is returned *<b>msg</b> will be set to an appropriate error
 * message.
 *
 * XXXX rename this function.  It's only called from the controller.
 * XXXX in fact, refactor this function, merging as much as possible.
 */
int
dirserv_get_routerdescs(smartlist_t *descs_out, const char *key,
                        const char **msg)
{
  *msg = NULL;

  if (!strcmp(key, "/tor/server/all")) {
    routerlist_t *rl = router_get_routerlist();
    SMARTLIST_FOREACH(rl->routers, routerinfo_t *, r,
                      smartlist_add(descs_out, &(r->cache_info)));
  } else if (!strcmp(key, "/tor/server/authority")) {
    const routerinfo_t *ri = router_get_my_routerinfo();
    if (ri)
      smartlist_add(descs_out, (void*) &(ri->cache_info));
  } else if (!strcmpstart(key, "/tor/server/d/")) {
    smartlist_t *digests = smartlist_new();
    key += strlen("/tor/server/d/");
    dir_split_resource_into_fingerprints(key, digests, NULL,
                                         DSR_HEX|DSR_SORT_UNIQ);
    SMARTLIST_FOREACH(digests, const char *, d,
       {
         signed_descriptor_t *sd = router_get_by_descriptor_digest(d);
         if (sd)
           smartlist_add(descs_out,sd);
       });
    SMARTLIST_FOREACH(digests, char *, d, tor_free(d));
    smartlist_free(digests);
  } else if (!strcmpstart(key, "/tor/server/fp/")) {
    smartlist_t *digests = smartlist_new();
    time_t cutoff = time(NULL) - ROUTER_MAX_AGE_TO_PUBLISH;
    key += strlen("/tor/server/fp/");
    dir_split_resource_into_fingerprints(key, digests, NULL,
                                         DSR_HEX|DSR_SORT_UNIQ);
    SMARTLIST_FOREACH_BEGIN(digests, const char *, d) {
         if (router_digest_is_me(d)) {
           /* calling router_get_my_routerinfo() to make sure it exists */
           const routerinfo_t *ri = router_get_my_routerinfo();
           if (ri)
             smartlist_add(descs_out, (void*) &(ri->cache_info));
         } else {
           const routerinfo_t *ri = router_get_by_id_digest(d);
           /* Don't actually serve a descriptor that everyone will think is
            * expired.  This is an (ugly) workaround to keep buggy 0.1.1.10
            * Tors from downloading descriptors that they will throw away.
            */
           if (ri && ri->cache_info.published_on > cutoff)
             smartlist_add(descs_out, (void*) &(ri->cache_info));
         }
    } SMARTLIST_FOREACH_END(d);
    SMARTLIST_FOREACH(digests, char *, d, tor_free(d));
    smartlist_free(digests);
  } else {
    *msg = "Key not recognized";
    return -1;
  }

  if (!smartlist_len(descs_out)) {
    *msg = "Servers unavailable";
    return -1;
  }
  return 0;
}

/** Called when a TLS handshake has completed successfully with a
 * router listening at <b>address</b>:<b>or_port</b>, and has yielded
 * a certificate with digest <b>digest_rcvd</b>.
 *
 * Inform the reachability checker that we could get to this relay.
 */
void
dirserv_orconn_tls_done(const tor_addr_t *addr,
                        uint16_t or_port,
                        const char *digest_rcvd)
{
  node_t *node = NULL;
  tor_addr_port_t orport;
  routerinfo_t *ri = NULL;
  time_t now = time(NULL);
  tor_assert(addr);
  tor_assert(digest_rcvd);

  node = node_get_mutable_by_id(digest_rcvd);
  if (node == NULL || node->ri == NULL)
    return;
  ri = node->ri;

  tor_addr_copy(&orport.addr, addr);
  orport.port = or_port;
  if (router_has_orport(ri, &orport)) {
    /* Found the right router.  */
    if (!authdir_mode_bridge(get_options()) ||
        ri->purpose == ROUTER_PURPOSE_BRIDGE) {
      char addrstr[TOR_ADDR_BUF_LEN];
      /* This is a bridge or we're not a bridge authorititative --
         mark it as reachable.  */
      log_info(LD_DIRSERV, "Found router %s to be reachable at %s:%d. Yay.",
               router_describe(ri),
               tor_addr_to_str(addrstr, addr, sizeof(addrstr), 1),
               ri->or_port);
      if (tor_addr_family(addr) == AF_INET) {
        rep_hist_note_router_reachable(digest_rcvd, addr, or_port, now);
        node->last_reachable = now;
      } else if (tor_addr_family(addr) == AF_INET6) {
        /* No rephist for IPv6.  */
        node->last_reachable6 = now;
      }
    }
  }
}

/** Called when we, as an authority, receive a new router descriptor either as
 * an upload or a download.  Used to decide whether to relaunch reachability
 * testing for the server. */
int
dirserv_should_launch_reachability_test(const routerinfo_t *ri,
                                        const routerinfo_t *ri_old)
{
  if (!authdir_mode_handles_descs(get_options(), ri->purpose))
    return 0;
  if (!ri_old) {
    /* New router: Launch an immediate reachability test, so we will have an
     * opinion soon in case we're generating a consensus soon */
    return 1;
  }
  if (ri_old->is_hibernating && !ri->is_hibernating) {
    /* It just came out of hibernation; launch a reachability test */
    return 1;
  }
  if (! routers_have_same_or_addrs(ri, ri_old)) {
    /* Address or port changed; launch a reachability test */
    return 1;
  }
  return 0;
}

/** Helper function for dirserv_test_reachability(). Start a TLS
 * connection to <b>router</b>, and annotate it with when we started
 * the test. */
void
dirserv_single_reachability_test(time_t now, routerinfo_t *router)
{
  channel_t *chan = NULL;
  node_t *node = NULL;
  tor_addr_t router_addr;
  (void) now;

  tor_assert(router);
  node = node_get_mutable_by_id(router->cache_info.identity_digest);
  tor_assert(node);

  /* IPv4. */
  log_debug(LD_OR,"Testing reachability of %s at %s:%u.",
            router->nickname, fmt_addr32(router->addr), router->or_port);
  tor_addr_from_ipv4h(&router_addr, router->addr);
  chan = channel_tls_connect(&router_addr, router->or_port,
                             router->cache_info.identity_digest);
  if (chan) command_setup_channel(chan);

  /* Possible IPv6. */
  if (get_options()->AuthDirHasIPv6Connectivity == 1 &&
      !tor_addr_is_null(&router->ipv6_addr)) {
    char addrstr[TOR_ADDR_BUF_LEN];
    log_debug(LD_OR, "Testing reachability of %s at %s:%u.",
              router->nickname,
              tor_addr_to_str(addrstr, &router->ipv6_addr, sizeof(addrstr), 1),
              router->ipv6_orport);
    chan = channel_tls_connect(&router->ipv6_addr, router->ipv6_orport,
                               router->cache_info.identity_digest);
    if (chan) command_setup_channel(chan);
  }
}

/** Auth dir server only: load balance such that we only
 * try a few connections per call.
 *
 * The load balancing is such that if we get called once every ten
 * seconds, we will cycle through all the tests in
 * REACHABILITY_TEST_CYCLE_PERIOD seconds (a bit over 20 minutes).
 */
void
dirserv_test_reachability(time_t now)
{
  /* XXX decide what to do here; see or-talk thread "purging old router
   * information, revocation." -NM
   * We can't afford to mess with this in 0.1.2.x. The reason is that
   * if we stop doing reachability tests on some of routerlist, then
   * we'll for-sure think they're down, which may have unexpected
   * effects in other parts of the code. It doesn't hurt much to do
   * the testing, and directory authorities are easy to upgrade. Let's
   * wait til 0.2.0. -RD */
//  time_t cutoff = now - ROUTER_MAX_AGE_TO_PUBLISH;
  routerlist_t *rl = router_get_routerlist();
  static char ctr = 0;
  int bridge_auth = authdir_mode_bridge(get_options());

  SMARTLIST_FOREACH_BEGIN(rl->routers, routerinfo_t *, router) {
    const char *id_digest = router->cache_info.identity_digest;
    if (router_is_me(router))
      continue;
    if (bridge_auth && router->purpose != ROUTER_PURPOSE_BRIDGE)
      continue; /* bridge authorities only test reachability on bridges */
//    if (router->cache_info.published_on > cutoff)
//      continue;
    if ((((uint8_t)id_digest[0]) % REACHABILITY_MODULO_PER_TEST) == ctr) {
      dirserv_single_reachability_test(now, router);
    }
  } SMARTLIST_FOREACH_END(router);
  ctr = (ctr + 1) % REACHABILITY_MODULO_PER_TEST; /* increment ctr */
}

/** Given a fingerprint <b>fp</b> which is either set if we're looking for a
 * v2 status, or zeroes if we're looking for a v3 status, or a NUL-padded
 * flavor name if we want a flavored v3 status, return a pointer to the
 * appropriate cached dir object, or NULL if there isn't one available. */
static cached_dir_t *
lookup_cached_dir_by_fp(const char *fp)
{
  cached_dir_t *d = NULL;
  if (tor_digest_is_zero(fp) && cached_consensuses) {
    d = strmap_get(cached_consensuses, "ns");
  } else if (memchr(fp, '\0', DIGEST_LEN) && cached_consensuses &&
           (d = strmap_get(cached_consensuses, fp))) {
    /* this here interface is a nasty hack XXXX024 */;
  }
  return d;
}

/** Remove from <b>fps</b> every networkstatus key where both
 * a) we have a networkstatus document and
 * b) it is not newer than <b>cutoff</b>.
 *
 * Return 1 if any items were present at all; else return 0.
 */
int
dirserv_remove_old_statuses(smartlist_t *fps, time_t cutoff)
{
  int found_any = 0;
  SMARTLIST_FOREACH_BEGIN(fps, char *, digest) {
    cached_dir_t *d = lookup_cached_dir_by_fp(digest);
    if (!d)
      continue;
    found_any = 1;
    if (d->published <= cutoff) {
      tor_free(digest);
      SMARTLIST_DEL_CURRENT(fps, digest);
    }
  } SMARTLIST_FOREACH_END(digest);

  return found_any;
}

/** Return the cache-info for identity fingerprint <b>fp</b>, or
 * its extra-info document if <b>extrainfo</b> is true. Return
 * NULL if not found or if the descriptor is older than
 * <b>publish_cutoff</b>. */
static const signed_descriptor_t *
get_signed_descriptor_by_fp(const char *fp, int extrainfo,
                            time_t publish_cutoff)
{
  if (router_digest_is_me(fp)) {
    if (extrainfo)
      return &(router_get_my_extrainfo()->cache_info);
    else
      return &(router_get_my_routerinfo()->cache_info);
  } else {
    const routerinfo_t *ri = router_get_by_id_digest(fp);
    if (ri &&
        ri->cache_info.published_on > publish_cutoff) {
      if (extrainfo)
        return extrainfo_get_by_descriptor_digest(
                                     ri->cache_info.extra_info_digest);
      else
        return &ri->cache_info;
    }
  }
  return NULL;
}

/** Return true iff we have any of the documents (extrainfo or routerdesc)
 * specified by the fingerprints in <b>fps</b> and <b>spool_src</b>.  Used to
 * decide whether to send a 404.  */
int
dirserv_have_any_serverdesc(smartlist_t *fps, int spool_src)
{
  time_t publish_cutoff = time(NULL)-ROUTER_MAX_AGE_TO_PUBLISH;
  SMARTLIST_FOREACH_BEGIN(fps, const char *, fp) {
      switch (spool_src)
      {
        case DIR_SPOOL_EXTRA_BY_DIGEST:
          if (extrainfo_get_by_descriptor_digest(fp)) return 1;
          break;
        case DIR_SPOOL_SERVER_BY_DIGEST:
          if (router_get_by_descriptor_digest(fp)) return 1;
          break;
        case DIR_SPOOL_EXTRA_BY_FP:
        case DIR_SPOOL_SERVER_BY_FP:
          if (get_signed_descriptor_by_fp(fp,
                spool_src == DIR_SPOOL_EXTRA_BY_FP, publish_cutoff))
            return 1;
          break;
      }
  } SMARTLIST_FOREACH_END(fp);
  return 0;
}

/** Return true iff any of the 256-bit elements in <b>fps</b> is the digest of
 * a microdescriptor we have. */
int
dirserv_have_any_microdesc(const smartlist_t *fps)
{
  microdesc_cache_t *cache = get_microdesc_cache();
  SMARTLIST_FOREACH(fps, const char *, fp,
                    if (microdesc_cache_lookup_by_digest256(cache, fp))
                      return 1);
  return 0;
}

/** Return an approximate estimate of the number of bytes that will
 * be needed to transmit the server descriptors (if is_serverdescs --
 * they can be either d/ or fp/ queries) or networkstatus objects (if
 * !is_serverdescs) listed in <b>fps</b>.  If <b>compressed</b> is set,
 * we guess how large the data will be after compression.
 *
 * The return value is an estimate; it might be larger or smaller.
 **/
size_t
dirserv_estimate_data_size(smartlist_t *fps, int is_serverdescs,
                           int compressed)
{
  size_t result;
  tor_assert(fps);
  if (is_serverdescs) {
    int n = smartlist_len(fps);
    const routerinfo_t *me = router_get_my_routerinfo();
    result = (me?me->cache_info.signed_descriptor_len:2048) * n;
    if (compressed)
      result /= 2; /* observed compressibility is between 35 and 55%. */
  } else {
    result = 0;
    SMARTLIST_FOREACH(fps, const char *, digest, {
        cached_dir_t *dir = lookup_cached_dir_by_fp(digest);
        if (dir)
          result += compressed ? dir->dir_z_len : dir->dir_len;
      });
  }
  return result;
}

/** Given a list of microdescriptor hashes, guess how many bytes will be
 * needed to transmit them, and return the guess. */
size_t
dirserv_estimate_microdesc_size(const smartlist_t *fps, int compressed)
{
  size_t result = smartlist_len(fps) * microdesc_average_size(NULL);
  if (compressed)
    result /= 2;
  return result;
}

/** When we're spooling data onto our outbuf, add more whenever we dip
 * below this threshold. */
#define DIRSERV_BUFFER_MIN 16384

/** Spooling helper: called when we have no more data to spool to <b>conn</b>.
 * Flushes any remaining data to be (un)compressed, and changes the spool
 * source to NONE.  Returns 0 on success, negative on failure. */
static int
connection_dirserv_finish_spooling(dir_connection_t *conn)
{
  if (conn->zlib_state) {
    connection_write_to_buf_zlib("", 0, conn, 1);
    tor_zlib_free(conn->zlib_state);
    conn->zlib_state = NULL;
  }
  conn->dir_spool_src = DIR_SPOOL_NONE;
  return 0;
}

/** Spooling helper: called when we're sending a bunch of server descriptors,
 * and the outbuf has become too empty. Pulls some entries from
 * fingerprint_stack, and writes the corresponding servers onto outbuf.  If we
 * run out of entries, flushes the zlib state and sets the spool source to
 * NONE.  Returns 0 on success, negative on failure.
 */
static int
connection_dirserv_add_servers_to_outbuf(dir_connection_t *conn)
{
  int by_fp = (conn->dir_spool_src == DIR_SPOOL_SERVER_BY_FP ||
               conn->dir_spool_src == DIR_SPOOL_EXTRA_BY_FP);
  int extra = (conn->dir_spool_src == DIR_SPOOL_EXTRA_BY_FP ||
               conn->dir_spool_src == DIR_SPOOL_EXTRA_BY_DIGEST);
  time_t publish_cutoff = time(NULL)-ROUTER_MAX_AGE_TO_PUBLISH;

  const or_options_t *options = get_options();

  while (smartlist_len(conn->fingerprint_stack) &&
         connection_get_outbuf_len(TO_CONN(conn)) < DIRSERV_BUFFER_MIN) {
    const char *body;
    char *fp = smartlist_pop_last(conn->fingerprint_stack);
    const signed_descriptor_t *sd = NULL;
    if (by_fp) {
      sd = get_signed_descriptor_by_fp(fp, extra, publish_cutoff);
    } else {
      sd = extra ? extrainfo_get_by_descriptor_digest(fp)
        : router_get_by_descriptor_digest(fp);
    }
    tor_free(fp);
    if (!sd)
      continue;
    if (!connection_dir_is_encrypted(conn) && !sd->send_unencrypted) {
      /* we did this check once before (so we could have an accurate size
       * estimate and maybe send a 404 if somebody asked for only bridges on a
       * connection), but we need to do it again in case a previously
       * unknown bridge descriptor has shown up between then and now. */
      continue;
    }

    /** If we are the bridge authority and the descriptor is a bridge
     * descriptor, remember that we served this descriptor for desc stats. */
    if (options->BridgeAuthoritativeDir && by_fp) {
      const routerinfo_t *router =
          router_get_by_id_digest(sd->identity_digest);
      /* router can be NULL here when the bridge auth is asked for its own
       * descriptor. */
      if (router && router->purpose == ROUTER_PURPOSE_BRIDGE)
        rep_hist_note_desc_served(sd->identity_digest);
    }
    body = signed_descriptor_get_body(sd);
    if (conn->zlib_state) {
      int last = ! smartlist_len(conn->fingerprint_stack);
      connection_write_to_buf_zlib(body, sd->signed_descriptor_len, conn,
                                   last);
      if (last) {
        tor_zlib_free(conn->zlib_state);
        conn->zlib_state = NULL;
      }
    } else {
      connection_write_to_buf(body,
                              sd->signed_descriptor_len,
                              TO_CONN(conn));
    }
  }

  if (!smartlist_len(conn->fingerprint_stack)) {
    /* We just wrote the last one; finish up. */
    if (conn->zlib_state) {
      connection_write_to_buf_zlib("", 0, conn, 1);
      tor_zlib_free(conn->zlib_state);
      conn->zlib_state = NULL;
    }
    conn->dir_spool_src = DIR_SPOOL_NONE;
    smartlist_free(conn->fingerprint_stack);
    conn->fingerprint_stack = NULL;
  }
  return 0;
}

/** Spooling helper: called when we're sending a bunch of microdescriptors,
 * and the outbuf has become too empty. Pulls some entries from
 * fingerprint_stack, and writes the corresponding microdescs onto outbuf.  If
 * we run out of entries, flushes the zlib state and sets the spool source to
 * NONE.  Returns 0 on success, negative on failure.
 */
static int
connection_dirserv_add_microdescs_to_outbuf(dir_connection_t *conn)
{
  microdesc_cache_t *cache = get_microdesc_cache();
  while (smartlist_len(conn->fingerprint_stack) &&
         connection_get_outbuf_len(TO_CONN(conn)) < DIRSERV_BUFFER_MIN) {
    char *fp256 = smartlist_pop_last(conn->fingerprint_stack);
    microdesc_t *md = microdesc_cache_lookup_by_digest256(cache, fp256);
    tor_free(fp256);
    if (!md || !md->body)
      continue;
    if (conn->zlib_state) {
      int last = !smartlist_len(conn->fingerprint_stack);
      connection_write_to_buf_zlib(md->body, md->bodylen, conn, last);
      if (last) {
        tor_zlib_free(conn->zlib_state);
        conn->zlib_state = NULL;
      }
    } else {
      connection_write_to_buf(md->body, md->bodylen, TO_CONN(conn));
    }
  }
  if (!smartlist_len(conn->fingerprint_stack)) {
    if (conn->zlib_state) {
      connection_write_to_buf_zlib("", 0, conn, 1);
      tor_zlib_free(conn->zlib_state);
      conn->zlib_state = NULL;
    }
    conn->dir_spool_src = DIR_SPOOL_NONE;
    smartlist_free(conn->fingerprint_stack);
    conn->fingerprint_stack = NULL;
  }
  return 0;
}

/** Spooling helper: Called when we're sending a directory or networkstatus,
 * and the outbuf has become too empty.  Pulls some bytes from
 * <b>conn</b>-\>cached_dir-\>dir_z, uncompresses them if appropriate, and
 * puts them on the outbuf.  If we run out of entries, flushes the zlib state
 * and sets the spool source to NONE.  Returns 0 on success, negative on
 * failure. */
static int
connection_dirserv_add_dir_bytes_to_outbuf(dir_connection_t *conn)
{
  ssize_t bytes;
  int64_t remaining;

  bytes = DIRSERV_BUFFER_MIN - connection_get_outbuf_len(TO_CONN(conn));
  tor_assert(bytes > 0);
  tor_assert(conn->cached_dir);
  if (bytes < 8192)
    bytes = 8192;
  remaining = conn->cached_dir->dir_z_len - conn->cached_dir_offset;
  if (bytes > remaining)
    bytes = (ssize_t) remaining;

  if (conn->zlib_state) {
    connection_write_to_buf_zlib(
                             conn->cached_dir->dir_z + conn->cached_dir_offset,
                             bytes, conn, bytes == remaining);
  } else {
    connection_write_to_buf(conn->cached_dir->dir_z + conn->cached_dir_offset,
                            bytes, TO_CONN(conn));
  }
  conn->cached_dir_offset += bytes;
  if (conn->cached_dir_offset == (int)conn->cached_dir->dir_z_len) {
    /* We just wrote the last one; finish up. */
    connection_dirserv_finish_spooling(conn);
    cached_dir_decref(conn->cached_dir);
    conn->cached_dir = NULL;
  }
  return 0;
}

/** Spooling helper: Called when we're spooling networkstatus objects on
 * <b>conn</b>, and the outbuf has become too empty.  If the current
 * networkstatus object (in <b>conn</b>-\>cached_dir) has more data, pull data
 * from there.  Otherwise, pop the next fingerprint from fingerprint_stack,
 * and start spooling the next networkstatus.  (A digest of all 0 bytes is
 * treated as a request for the current consensus.) If we run out of entries,
 * flushes the zlib state and sets the spool source to NONE.  Returns 0 on
 * success, negative on failure. */
static int
connection_dirserv_add_networkstatus_bytes_to_outbuf(dir_connection_t *conn)
{

  while (connection_get_outbuf_len(TO_CONN(conn)) < DIRSERV_BUFFER_MIN) {
    if (conn->cached_dir) {
      int uncompressing = (conn->zlib_state != NULL);
      int r = connection_dirserv_add_dir_bytes_to_outbuf(conn);
      if (conn->dir_spool_src == DIR_SPOOL_NONE) {
        /* add_dir_bytes thinks we're done with the cached_dir.  But we
         * may have more cached_dirs! */
        conn->dir_spool_src = DIR_SPOOL_NETWORKSTATUS;
        /* This bit is tricky.  If we were uncompressing the last
         * networkstatus, we may need to make a new zlib object to
         * uncompress the next one. */
        if (uncompressing && ! conn->zlib_state &&
            conn->fingerprint_stack &&
            smartlist_len(conn->fingerprint_stack)) {
          conn->zlib_state = tor_zlib_new(0, ZLIB_METHOD, HIGH_COMPRESSION);
        }
      }
      if (r) return r;
    } else if (conn->fingerprint_stack &&
               smartlist_len(conn->fingerprint_stack)) {
      /* Add another networkstatus; start serving it. */
      char *fp = smartlist_pop_last(conn->fingerprint_stack);
      cached_dir_t *d = lookup_cached_dir_by_fp(fp);
      tor_free(fp);
      if (d) {
        ++d->refcnt;
        conn->cached_dir = d;
        conn->cached_dir_offset = 0;
      }
    } else {
      connection_dirserv_finish_spooling(conn);
      smartlist_free(conn->fingerprint_stack);
      conn->fingerprint_stack = NULL;
      return 0;
    }
  }
  return 0;
}

/** Called whenever we have flushed some directory data in state
 * SERVER_WRITING. */
int
connection_dirserv_flushed_some(dir_connection_t *conn)
{
  tor_assert(conn->base_.state == DIR_CONN_STATE_SERVER_WRITING);

  if (connection_get_outbuf_len(TO_CONN(conn)) >= DIRSERV_BUFFER_MIN)
    return 0;

  switch (conn->dir_spool_src) {
    case DIR_SPOOL_EXTRA_BY_DIGEST:
    case DIR_SPOOL_EXTRA_BY_FP:
    case DIR_SPOOL_SERVER_BY_DIGEST:
    case DIR_SPOOL_SERVER_BY_FP:
      return connection_dirserv_add_servers_to_outbuf(conn);
    case DIR_SPOOL_MICRODESC:
      return connection_dirserv_add_microdescs_to_outbuf(conn);
    case DIR_SPOOL_CACHED_DIR:
      return connection_dirserv_add_dir_bytes_to_outbuf(conn);
    case DIR_SPOOL_NETWORKSTATUS:
      return connection_dirserv_add_networkstatus_bytes_to_outbuf(conn);
    case DIR_SPOOL_NONE:
    default:
      return 0;
  }
}

/** Return true iff <b>line</b> is a valid RecommendedPackages line.
 */
/*
  The grammar is:

    "package" SP PACKAGENAME SP VERSION SP URL SP DIGESTS NL

      PACKAGENAME = NONSPACE
      VERSION = NONSPACE
      URL = NONSPACE
      DIGESTS = DIGEST | DIGESTS SP DIGEST
      DIGEST = DIGESTTYPE "=" DIGESTVAL

      NONSPACE = one or more non-space printing characters

      DIGESTVAL = DIGESTTYPE = one or more non-=, non-" " characters.

      SP = " "
      NL = a newline

 */
int
validate_recommended_package_line(const char *line)
{
  const char *cp = line;

#define WORD()                                  \
  do {                                          \
    if (*cp == ' ')                             \
      return 0;                                 \
    cp = strchr(cp, ' ');                       \
    if (!cp)                                    \
      return 0;                                 \
  } while (0)

  WORD(); /* skip packagename */
  ++cp;
  WORD(); /* skip version */
  ++cp;
  WORD(); /* Skip URL */
  ++cp;

  /* Skip digesttype=digestval + */
  int n_entries = 0;
  while (1) {
    const char *start_of_word = cp;
    const char *end_of_word = strchr(cp, ' ');
    if (! end_of_word)
      end_of_word = cp + strlen(cp);

    if (start_of_word == end_of_word)
      return 0;

    const char *eq = memchr(start_of_word, '=', end_of_word - start_of_word);

    if (!eq)
      return 0;
    if (eq == start_of_word)
      return 0;
    if (eq == end_of_word - 1)
      return 0;
    if (memchr(eq+1, '=', end_of_word - (eq+1)))
      return 0;

    ++n_entries;
    if (0 == *end_of_word)
      break;

    cp = end_of_word + 1;
  }

  /* If we reach this point, we have at least 1 entry. */
  tor_assert(n_entries > 0);
  return 1;
}

/** Release all storage used by the directory server. */
void
dirserv_free_all(void)
{
  dirserv_free_fingerprint_list();

  strmap_free(cached_consensuses, free_cached_dir_);
  cached_consensuses = NULL;

  dirserv_clear_measured_bw_cache();
}

