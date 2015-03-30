/* Copyright (c) 2001 Matej Pfajfar.
 * Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file routerlist.c
 * \brief Code to
 * maintain and access the global list of routerinfos for known
 * servers.
 **/

#define ROUTERLIST_PRIVATE
#include "or.h"
#include "circuitstats.h"
#include "config.h"
#include "connection.h"
#include "control.h"
#include "directory.h"
#include "dirserv.h"
#include "dirvote.h"
#include "entrynodes.h"
#include "fp_pair.h"
#include "geoip.h"
#include "hibernate.h"
#include "main.h"
#include "microdesc.h"
#include "networkstatus.h"
#include "nodelist.h"
#include "policies.h"
#include "reasons.h"
#include "rendcommon.h"
#include "rendservice.h"
#include "rephist.h"
#include "router.h"
#include "routerlist.h"
#include "routerparse.h"
#include "routerset.h"
#include "../common/sandbox.h"
// #define DEBUG_ROUTERLIST

/****************************************************************************/

DECLARE_TYPED_DIGESTMAP_FNS(sdmap_, digest_sd_map_t, signed_descriptor_t)
DECLARE_TYPED_DIGESTMAP_FNS(rimap_, digest_ri_map_t, routerinfo_t)
DECLARE_TYPED_DIGESTMAP_FNS(eimap_, digest_ei_map_t, extrainfo_t)
DECLARE_TYPED_DIGESTMAP_FNS(dsmap_, digest_ds_map_t, download_status_t)
#define SDMAP_FOREACH(map, keyvar, valvar)                              \
  DIGESTMAP_FOREACH(sdmap_to_digestmap(map), keyvar, signed_descriptor_t *, \
                    valvar)
#define RIMAP_FOREACH(map, keyvar, valvar) \
  DIGESTMAP_FOREACH(rimap_to_digestmap(map), keyvar, routerinfo_t *, valvar)
#define EIMAP_FOREACH(map, keyvar, valvar) \
  DIGESTMAP_FOREACH(eimap_to_digestmap(map), keyvar, extrainfo_t *, valvar)
#define DSMAP_FOREACH(map, keyvar, valvar) \
  DIGESTMAP_FOREACH(dsmap_to_digestmap(map), keyvar, download_status_t *, \
                    valvar)

/* Forward declaration for cert_list_t */
typedef struct cert_list_t cert_list_t;

/* static function prototypes */
static int compute_weighted_bandwidths(const smartlist_t *sl,
                                       bandwidth_weight_rule_t rule,
                                       u64_dbl_t **bandwidths_out);
static const routerstatus_t *router_pick_directory_server_impl(
                                           dirinfo_type_t auth, int flags);
static const routerstatus_t *router_pick_trusteddirserver_impl(
                const smartlist_t *sourcelist, dirinfo_type_t auth,
                int flags, int *n_busy_out);
static const routerstatus_t *router_pick_dirserver_generic(
                              smartlist_t *sourcelist,
                              dirinfo_type_t type, int flags);
static void mark_all_dirservers_up(smartlist_t *server_list);
static void dir_server_free(dir_server_t *ds);
static int signed_desc_digest_is_recognized(signed_descriptor_t *desc);
static const char *signed_descriptor_get_body_impl(
                                              const signed_descriptor_t *desc,
                                              int with_annotations);
static void list_pending_downloads(digestmap_t *result,
                                   int purpose, const char *prefix);
static void list_pending_fpsk_downloads(fp_pair_map_t *result);
static void launch_dummy_descriptor_download_as_needed(time_t now,
                                   const or_options_t *options);
static void download_status_reset_by_sk_in_cl(cert_list_t *cl,
                                              const char *digest);
static int download_status_is_ready_by_sk_in_cl(cert_list_t *cl,
                                                const char *digest,
                                                time_t now, int max_failures);

/****************************************************************************/

/** Global list of a dir_server_t object for each directory
 * authority. */
static smartlist_t *trusted_dir_servers = NULL;
/** Global list of dir_server_t objects for all directory authorities
 * and all fallback directory servers. */
static smartlist_t *fallback_dir_servers = NULL;

/** List of certificates for a single authority, and download status for
 * latest certificate.
 */
struct cert_list_t {
  /*
   * The keys of download status map are cert->signing_key_digest for pending
   * downloads by (identity digest/signing key digest) pair; functions such
   * as authority_cert_get_by_digest() already assume these are unique.
   */
  struct digest_ds_map_t *dl_status_map;
  /* There is also a dlstatus for the download by identity key only */
  download_status_t dl_status_by_id;
  smartlist_t *certs;
};
/** Map from v3 identity key digest to cert_list_t. */
static digestmap_t *trusted_dir_certs = NULL;
/** True iff any key certificate in at least one member of
 * <b>trusted_dir_certs</b> has changed since we last flushed the
 * certificates to disk. */
static int trusted_dir_servers_certs_changed = 0;

/** Global list of all of the routers that we know about. */
static routerlist_t *routerlist = NULL;

/** List of strings for nicknames we've already warned about and that are
 * still unknown / unavailable. */
static smartlist_t *warned_nicknames = NULL;

/** The last time we tried to download any routerdesc, or 0 for "never".  We
 * use this to rate-limit download attempts when the number of routerdescs to
 * download is low. */
static time_t last_descriptor_download_attempted = 0;

/** Return the number of directory authorities whose type matches some bit set
 * in <b>type</b>  */
int
get_n_authorities(dirinfo_type_t type)
{
  int n = 0;
  if (!trusted_dir_servers)
    return 0;
  SMARTLIST_FOREACH(trusted_dir_servers, dir_server_t *, ds,
                    if (ds->type & type)
                      ++n);
  return n;
}

/** Reset the download status of a specified element in a dsmap */
static void
download_status_reset_by_sk_in_cl(cert_list_t *cl, const char *digest)
{
  download_status_t *dlstatus = NULL;

  tor_assert(cl);
  tor_assert(digest);

  /* Make sure we have a dsmap */
  if (!(cl->dl_status_map)) {
    cl->dl_status_map = dsmap_new();
  }
  /* Look for a download_status_t in the map with this digest */
  dlstatus = dsmap_get(cl->dl_status_map, digest);
  /* Got one? */
  if (!dlstatus) {
    /* Insert before we reset */
    dlstatus = tor_malloc_zero(sizeof(*dlstatus));
    dsmap_set(cl->dl_status_map, digest, dlstatus);
  }
  tor_assert(dlstatus);
  /* Go ahead and reset it */
  download_status_reset(dlstatus);
}

/**
 * Return true if the download for this signing key digest in cl is ready
 * to be re-attempted.
 */
static int
download_status_is_ready_by_sk_in_cl(cert_list_t *cl,
                                     const char *digest,
                                     time_t now, int max_failures)
{
  int rv = 0;
  download_status_t *dlstatus = NULL;

  tor_assert(cl);
  tor_assert(digest);

  /* Make sure we have a dsmap */
  if (!(cl->dl_status_map)) {
    cl->dl_status_map = dsmap_new();
  }
  /* Look for a download_status_t in the map with this digest */
  dlstatus = dsmap_get(cl->dl_status_map, digest);
  /* Got one? */
  if (dlstatus) {
    /* Use download_status_is_ready() */
    rv = download_status_is_ready(dlstatus, now, max_failures);
  } else {
    /*
     * If we don't know anything about it, return 1, since we haven't
     * tried this one before.  We need to create a new entry here,
     * too.
     */
    dlstatus = tor_malloc_zero(sizeof(*dlstatus));
    download_status_reset(dlstatus);
    dsmap_set(cl->dl_status_map, digest, dlstatus);
    rv = 1;
  }

  return rv;
}

/** Helper: Return the cert_list_t for an authority whose authority ID is
 * <b>id_digest</b>, allocating a new list if necessary. */
static cert_list_t *
get_cert_list(const char *id_digest)
{
  cert_list_t *cl;
  if (!trusted_dir_certs)
    trusted_dir_certs = digestmap_new();
  cl = digestmap_get(trusted_dir_certs, id_digest);
  if (!cl) {
    cl = tor_malloc_zero(sizeof(cert_list_t));
    cl->dl_status_by_id.schedule = DL_SCHED_CONSENSUS;
    cl->certs = smartlist_new();
    cl->dl_status_map = dsmap_new();
    digestmap_set(trusted_dir_certs, id_digest, cl);
  }
  return cl;
}

/** Release all space held by a cert_list_t */
static void
cert_list_free(cert_list_t *cl)
{
  if (!cl)
    return;

  SMARTLIST_FOREACH(cl->certs, authority_cert_t *, cert,
                    authority_cert_free(cert));
  smartlist_free(cl->certs);
  dsmap_free(cl->dl_status_map, tor_free_);
  tor_free(cl);
}

/** Wrapper for cert_list_free so we can pass it to digestmap_free */
static void
cert_list_free_(void *cl)
{
  cert_list_free(cl);
}

/** Reload the cached v3 key certificates from the cached-certs file in
 * the data directory. Return 0 on success, -1 on failure. */
int
trusted_dirs_reload_certs(void)
{
  char *filename;
  char *contents;
  int r;

  filename = get_datadir_fname("cached-certs");
  contents = read_file_to_str(filename, RFTS_IGNORE_MISSING, NULL);
  tor_free(filename);
  if (!contents)
    return 0;
  r = trusted_dirs_load_certs_from_string(
        contents,
        TRUSTED_DIRS_CERTS_SRC_FROM_STORE, 1);
  tor_free(contents);
  return r;
}

/** Helper: return true iff we already have loaded the exact cert
 * <b>cert</b>. */
static INLINE int
already_have_cert(authority_cert_t *cert)
{
  cert_list_t *cl = get_cert_list(cert->cache_info.identity_digest);

  SMARTLIST_FOREACH(cl->certs, authority_cert_t *, c,
  {
    if (tor_memeq(c->cache_info.signed_descriptor_digest,
                cert->cache_info.signed_descriptor_digest,
                DIGEST_LEN))
      return 1;
  });
  return 0;
}

/** Load a bunch of new key certificates from the string <b>contents</b>.  If
 * <b>source</b> is TRUSTED_DIRS_CERTS_SRC_FROM_STORE, the certificates are
 * from the cache, and we don't need to flush them to disk.  If we are a
 * dirauth loading our own cert, source is TRUSTED_DIRS_CERTS_SRC_SELF.
 * Otherwise, source is download type: TRUSTED_DIRS_CERTS_SRC_DL_BY_ID_DIGEST
 * or TRUSTED_DIRS_CERTS_SRC_DL_BY_ID_SK_DIGEST.  If <b>flush</b> is true, we
 * need to flush any changed certificates to disk now.  Return 0 on success,
 * -1 if any certs fail to parse.
 */

int
trusted_dirs_load_certs_from_string(const char *contents, int source,
                                    int flush)
{
  dir_server_t *ds;
  const char *s, *eos;
  int failure_code = 0;
  int from_store = (source == TRUSTED_DIRS_CERTS_SRC_FROM_STORE);

  for (s = contents; *s; s = eos) {
    authority_cert_t *cert = authority_cert_parse_from_string(s, &eos);
    cert_list_t *cl;
    if (!cert) {
      failure_code = -1;
      break;
    }
    ds = trusteddirserver_get_by_v3_auth_digest(
                                       cert->cache_info.identity_digest);
    log_debug(LD_DIR, "Parsed certificate for %s",
              ds ? ds->nickname : "unknown authority");

    if (already_have_cert(cert)) {
      /* we already have this one. continue. */
      log_info(LD_DIR, "Skipping %s certificate for %s that we "
               "already have.",
               from_store ? "cached" : "downloaded",
               ds ? ds->nickname : "an old or new authority");

      /*
       * A duplicate on download should be treated as a failure, so we call
       * authority_cert_dl_failed() to reset the download status to make sure
       * we can't try again.  Since we've implemented the fp-sk mechanism
       * to download certs by signing key, this should be much rarer than it
       * was and is perhaps cause for concern.
       */
      if (!from_store) {
        if (authdir_mode(get_options())) {
          log_warn(LD_DIR,
                   "Got a certificate for %s, but we already have it. "
                   "Maybe they haven't updated it. Waiting for a while.",
                   ds ? ds->nickname : "an old or new authority");
        } else {
          log_info(LD_DIR,
                   "Got a certificate for %s, but we already have it. "
                   "Maybe they haven't updated it. Waiting for a while.",
                   ds ? ds->nickname : "an old or new authority");
        }

        /*
         * This is where we care about the source; authority_cert_dl_failed()
         * needs to know whether the download was by fp or (fp,sk) pair to
         * twiddle the right bit in the download map.
         */
        if (source == TRUSTED_DIRS_CERTS_SRC_DL_BY_ID_DIGEST) {
          authority_cert_dl_failed(cert->cache_info.identity_digest,
                                   NULL, 404);
        } else if (source == TRUSTED_DIRS_CERTS_SRC_DL_BY_ID_SK_DIGEST) {
          authority_cert_dl_failed(cert->cache_info.identity_digest,
                                   cert->signing_key_digest, 404);
        }
      }

      authority_cert_free(cert);
      continue;
    }

    if (ds) {
      log_info(LD_DIR, "Adding %s certificate for directory authority %s with "
               "signing key %s", from_store ? "cached" : "downloaded",
               ds->nickname, hex_str(cert->signing_key_digest,DIGEST_LEN));
    } else {
      int adding = directory_caches_unknown_auth_certs(get_options());
      log_info(LD_DIR, "%s %s certificate for unrecognized directory "
               "authority with signing key %s",
               adding ? "Adding" : "Not adding",
               from_store ? "cached" : "downloaded",
               hex_str(cert->signing_key_digest,DIGEST_LEN));
      if (!adding) {
        authority_cert_free(cert);
        continue;
      }
    }

    cl = get_cert_list(cert->cache_info.identity_digest);
    smartlist_add(cl->certs, cert);
    if (ds && cert->cache_info.published_on > ds->addr_current_at) {
      /* Check to see whether we should update our view of the authority's
       * address. */
      if (cert->addr && cert->dir_port &&
          (ds->addr != cert->addr ||
           ds->dir_port != cert->dir_port)) {
        char *a = tor_dup_ip(cert->addr);
        log_notice(LD_DIR, "Updating address for directory authority %s "
                   "from %s:%d to %s:%d based on certificate.",
                   ds->nickname, ds->address, (int)ds->dir_port,
                   a, cert->dir_port);
        tor_free(a);
        ds->addr = cert->addr;
        ds->dir_port = cert->dir_port;
      }
      ds->addr_current_at = cert->cache_info.published_on;
    }

    if (!from_store)
      trusted_dir_servers_certs_changed = 1;
  }

  if (flush)
    trusted_dirs_flush_certs_to_disk();

  /* call this even if failure_code is <0, since some certs might have
   * succeeded. */
  networkstatus_note_certs_arrived();

  return failure_code;
}

/** Save all v3 key certificates to the cached-certs file. */
void
trusted_dirs_flush_certs_to_disk(void)
{
  char *filename;
  smartlist_t *chunks;

  if (!trusted_dir_servers_certs_changed || !trusted_dir_certs)
    return;

  chunks = smartlist_new();
  DIGESTMAP_FOREACH(trusted_dir_certs, key, cert_list_t *, cl) {
    SMARTLIST_FOREACH(cl->certs, authority_cert_t *, cert,
          {
            sized_chunk_t *c = tor_malloc(sizeof(sized_chunk_t));
            c->bytes = cert->cache_info.signed_descriptor_body;
            c->len = cert->cache_info.signed_descriptor_len;
            smartlist_add(chunks, c);
          });
  } DIGESTMAP_FOREACH_END;

  filename = get_datadir_fname("cached-certs");
  if (write_chunks_to_file(filename, chunks, 0, 0)) {
    log_warn(LD_FS, "Error writing certificates to disk.");
  }
  tor_free(filename);
  SMARTLIST_FOREACH(chunks, sized_chunk_t *, c, tor_free(c));
  smartlist_free(chunks);

  trusted_dir_servers_certs_changed = 0;
}

/** Remove all v3 authority certificates that have been superseded for more
 * than 48 hours.  (If the most recent cert was published more than 48 hours
 * ago, then we aren't going to get any consensuses signed with older
 * keys.) */
static void
trusted_dirs_remove_old_certs(void)
{
  time_t now = time(NULL);
#define DEAD_CERT_LIFETIME (2*24*60*60)
#define OLD_CERT_LIFETIME (7*24*60*60)
  if (!trusted_dir_certs)
    return;

  DIGESTMAP_FOREACH(trusted_dir_certs, key, cert_list_t *, cl) {
    authority_cert_t *newest = NULL;
    SMARTLIST_FOREACH(cl->certs, authority_cert_t *, cert,
          if (!newest || (cert->cache_info.published_on >
                          newest->cache_info.published_on))
            newest = cert);
    if (newest) {
      const time_t newest_published = newest->cache_info.published_on;
      SMARTLIST_FOREACH_BEGIN(cl->certs, authority_cert_t *, cert) {
        int expired;
        time_t cert_published;
        if (newest == cert)
          continue;
        expired = now > cert->expires;
        cert_published = cert->cache_info.published_on;
        /* Store expired certs for 48 hours after a newer arrives;
         */
        if (expired ?
            (newest_published + DEAD_CERT_LIFETIME < now) :
            (cert_published + OLD_CERT_LIFETIME < newest_published)) {
          SMARTLIST_DEL_CURRENT(cl->certs, cert);
          authority_cert_free(cert);
          trusted_dir_servers_certs_changed = 1;
        }
      } SMARTLIST_FOREACH_END(cert);
    }
  } DIGESTMAP_FOREACH_END;
#undef OLD_CERT_LIFETIME

  trusted_dirs_flush_certs_to_disk();
}

/** Return the newest v3 authority certificate whose v3 authority identity key
 * has digest <b>id_digest</b>.  Return NULL if no such authority is known,
 * or it has no certificate. */
authority_cert_t *
authority_cert_get_newest_by_id(const char *id_digest)
{
  cert_list_t *cl;
  authority_cert_t *best = NULL;
  if (!trusted_dir_certs ||
      !(cl = digestmap_get(trusted_dir_certs, id_digest)))
    return NULL;

  SMARTLIST_FOREACH(cl->certs, authority_cert_t *, cert,
  {
    if (!best || cert->cache_info.published_on > best->cache_info.published_on)
      best = cert;
  });
  return best;
}

/** Return the newest v3 authority certificate whose directory signing key has
 * digest <b>sk_digest</b>. Return NULL if no such certificate is known.
 */
authority_cert_t *
authority_cert_get_by_sk_digest(const char *sk_digest)
{
  authority_cert_t *c;
  if (!trusted_dir_certs)
    return NULL;

  if ((c = get_my_v3_authority_cert()) &&
      tor_memeq(c->signing_key_digest, sk_digest, DIGEST_LEN))
    return c;
  if ((c = get_my_v3_legacy_cert()) &&
      tor_memeq(c->signing_key_digest, sk_digest, DIGEST_LEN))
    return c;

  DIGESTMAP_FOREACH(trusted_dir_certs, key, cert_list_t *, cl) {
    SMARTLIST_FOREACH(cl->certs, authority_cert_t *, cert,
    {
      if (tor_memeq(cert->signing_key_digest, sk_digest, DIGEST_LEN))
        return cert;
    });
  } DIGESTMAP_FOREACH_END;
  return NULL;
}

/** Return the v3 authority certificate with signing key matching
 * <b>sk_digest</b>, for the authority with identity digest <b>id_digest</b>.
 * Return NULL if no such authority is known. */
authority_cert_t *
authority_cert_get_by_digests(const char *id_digest,
                              const char *sk_digest)
{
  cert_list_t *cl;
  if (!trusted_dir_certs ||
      !(cl = digestmap_get(trusted_dir_certs, id_digest)))
    return NULL;
  SMARTLIST_FOREACH(cl->certs, authority_cert_t *, cert,
    if (tor_memeq(cert->signing_key_digest, sk_digest, DIGEST_LEN))
      return cert; );

  return NULL;
}

/** Add every known authority_cert_t to <b>certs_out</b>. */
void
authority_cert_get_all(smartlist_t *certs_out)
{
  tor_assert(certs_out);
  if (!trusted_dir_certs)
    return;

  DIGESTMAP_FOREACH(trusted_dir_certs, key, cert_list_t *, cl) {
    SMARTLIST_FOREACH(cl->certs, authority_cert_t *, c,
                      smartlist_add(certs_out, c));
  } DIGESTMAP_FOREACH_END;
}

/** Called when an attempt to download a certificate with the authority with
 * ID <b>id_digest</b> and, if not NULL, signed with key signing_key_digest
 * fails with HTTP response code <b>status</b>: remember the failure, so we
 * don't try again immediately. */
void
authority_cert_dl_failed(const char *id_digest,
                         const char *signing_key_digest, int status)
{
  cert_list_t *cl;
  download_status_t *dlstatus = NULL;
  char id_digest_str[2*DIGEST_LEN+1];
  char sk_digest_str[2*DIGEST_LEN+1];

  if (!trusted_dir_certs ||
      !(cl = digestmap_get(trusted_dir_certs, id_digest)))
    return;

  /*
   * Are we noting a failed download of the latest cert for the id digest,
   * or of a download by (id, signing key) digest pair?
   */
  if (!signing_key_digest) {
    /* Just by id digest */
    download_status_failed(&cl->dl_status_by_id, status);
  } else {
    /* Reset by (id, signing key) digest pair
     *
     * Look for a download_status_t in the map with this digest
     */
    dlstatus = dsmap_get(cl->dl_status_map, signing_key_digest);
    /* Got one? */
    if (dlstatus) {
      download_status_failed(dlstatus, status);
    } else {
      /*
       * Do this rather than hex_str(), since hex_str clobbers
       * old results and we call twice in the param list.
       */
      base16_encode(id_digest_str, sizeof(id_digest_str),
                    id_digest, DIGEST_LEN);
      base16_encode(sk_digest_str, sizeof(sk_digest_str),
                    signing_key_digest, DIGEST_LEN);
      log_warn(LD_BUG,
               "Got failure for cert fetch with (fp,sk) = (%s,%s), with "
               "status %d, but knew nothing about the download.",
               id_digest_str, sk_digest_str, status);
    }
  }
}

static const char *BAD_SIGNING_KEYS[] = {
  "09CD84F751FD6E955E0F8ADB497D5401470D697E", // Expires 2015-01-11 16:26:31
  "0E7E9C07F0969D0468AD741E172A6109DC289F3C", // Expires 2014-08-12 10:18:26
  "57B85409891D3FB32137F642FDEDF8B7F8CDFDCD", // Expires 2015-02-11 17:19:09
  "87326329007AF781F587AF5B594E540B2B6C7630", // Expires 2014-07-17 11:10:09
  "98CC82342DE8D298CF99D3F1A396475901E0D38E", // Expires 2014-11-10 13:18:56
  "9904B52336713A5ADCB13E4FB14DC919E0D45571", // Expires 2014-04-20 20:01:01
  "9DCD8E3F1DD1597E2AD476BBA28A1A89F3095227", // Expires 2015-01-16 03:52:30
  "A61682F34B9BB9694AC98491FE1ABBFE61923941", // Expires 2014-06-11 09:25:09
  "B59F6E99C575113650C99F1C425BA7B20A8C071D", // Expires 2014-07-31 13:22:10
  "D27178388FA75B96D37FA36E0B015227DDDBDA51", // Expires 2014-08-04 04:01:57
  NULL,
};

/** DOCDOC */
int
authority_cert_is_blacklisted(const authority_cert_t *cert)
{
  char hex_digest[HEX_DIGEST_LEN+1];
  int i;
  base16_encode(hex_digest, sizeof(hex_digest),
                cert->signing_key_digest, sizeof(cert->signing_key_digest));

  for (i = 0; BAD_SIGNING_KEYS[i]; ++i) {
    if (!strcasecmp(hex_digest, BAD_SIGNING_KEYS[i])) {
      return 1;
    }
  }
  return 0;
}

/** Return true iff when we've been getting enough failures when trying to
 * download the certificate with ID digest <b>id_digest</b> that we're willing
 * to start bugging the user about it. */
int
authority_cert_dl_looks_uncertain(const char *id_digest)
{
#define N_AUTH_CERT_DL_FAILURES_TO_BUG_USER 2
  cert_list_t *cl;
  int n_failures;
  if (!trusted_dir_certs ||
      !(cl = digestmap_get(trusted_dir_certs, id_digest)))
    return 0;

  n_failures = download_status_get_n_failures(&cl->dl_status_by_id);
  return n_failures >= N_AUTH_CERT_DL_FAILURES_TO_BUG_USER;
}

/** Try to download any v3 authority certificates that we may be missing.  If
 * <b>status</b> is provided, try to get all the ones that were used to sign
 * <b>status</b>.  Additionally, try to have a non-expired certificate for
 * every V3 authority in trusted_dir_servers.  Don't fetch certificates we
 * already have.
 **/
void
authority_certs_fetch_missing(networkstatus_t *status, time_t now)
{
  /*
   * The pending_id digestmap tracks pending certificate downloads by
   * identity digest; the pending_cert digestmap tracks pending downloads
   * by (identity digest, signing key digest) pairs.
   */
  digestmap_t *pending_id;
  fp_pair_map_t *pending_cert;
  authority_cert_t *cert;
  /*
   * The missing_id_digests smartlist will hold a list of id digests
   * we want to fetch the newest cert for; the missing_cert_digests
   * smartlist will hold a list of fp_pair_t with an identity and
   * signing key digest.
   */
  smartlist_t *missing_cert_digests, *missing_id_digests;
  char *resource = NULL;
  cert_list_t *cl;
  const int cache = directory_caches_unknown_auth_certs(get_options());
  fp_pair_t *fp_tmp = NULL;
  char id_digest_str[2*DIGEST_LEN+1];
  char sk_digest_str[2*DIGEST_LEN+1];

  if (should_delay_dir_fetches(get_options(), NULL))
    return;

  pending_cert = fp_pair_map_new();
  pending_id = digestmap_new();
  missing_cert_digests = smartlist_new();
  missing_id_digests = smartlist_new();

  /*
   * First, we get the lists of already pending downloads so we don't
   * duplicate effort.
   */
  list_pending_downloads(pending_id, DIR_PURPOSE_FETCH_CERTIFICATE, "fp/");
  list_pending_fpsk_downloads(pending_cert);

  /*
   * Now, we download any trusted authority certs we don't have by
   * identity digest only.  This gets the latest cert for that authority.
   */
  SMARTLIST_FOREACH_BEGIN(trusted_dir_servers, dir_server_t *, ds) {
    int found = 0;
    if (!(ds->type & V3_DIRINFO))
      continue;
    if (smartlist_contains_digest(missing_id_digests,
                                  ds->v3_identity_digest))
      continue;
    cl = get_cert_list(ds->v3_identity_digest);
    SMARTLIST_FOREACH_BEGIN(cl->certs, authority_cert_t *, cert) {
      if (now < cert->expires) {
        /* It's not expired, and we weren't looking for something to
         * verify a consensus with.  Call it done. */
        download_status_reset(&(cl->dl_status_by_id));
        /* No sense trying to download it specifically by signing key hash */
        download_status_reset_by_sk_in_cl(cl, cert->signing_key_digest);
        found = 1;
        break;
      }
    } SMARTLIST_FOREACH_END(cert);
    if (!found &&
        download_status_is_ready(&(cl->dl_status_by_id), now,
                                 get_options()->TestingCertMaxDownloadTries) &&
        !digestmap_get(pending_id, ds->v3_identity_digest)) {
      log_info(LD_DIR,
               "No current certificate known for authority %s "
               "(ID digest %s); launching request.",
               ds->nickname, hex_str(ds->v3_identity_digest, DIGEST_LEN));
      smartlist_add(missing_id_digests, ds->v3_identity_digest);
    }
  } SMARTLIST_FOREACH_END(ds);

  /*
   * Next, if we have a consensus, scan through it and look for anything
   * signed with a key from a cert we don't have.  Those get downloaded
   * by (fp,sk) pair, but if we don't know any certs at all for the fp
   * (identity digest), and it's one of the trusted dir server certs
   * we started off above or a pending download in pending_id, don't
   * try to get it yet.  Most likely, the one we'll get for that will
   * have the right signing key too, and we'd just be downloading
   * redundantly.
   */
  if (status) {
    SMARTLIST_FOREACH_BEGIN(status->voters, networkstatus_voter_info_t *,
                            voter) {
      if (!smartlist_len(voter->sigs))
        continue; /* This authority never signed this consensus, so don't
                   * go looking for a cert with key digest 0000000000. */
      if (!cache &&
          !trusteddirserver_get_by_v3_auth_digest(voter->identity_digest))
        continue; /* We are not a cache, and we don't know this authority.*/

      /*
       * If we don't know *any* cert for this authority, and a download by ID
       * is pending or we added it to missing_id_digests above, skip this
       * one for now to avoid duplicate downloads.
       */
      cl = get_cert_list(voter->identity_digest);
      if (smartlist_len(cl->certs) == 0) {
        /* We have no certs at all for this one */

        /* Do we have a download of one pending? */
        if (digestmap_get(pending_id, voter->identity_digest))
          continue;

        /*
         * Are we about to launch a download of one due to the trusted
         * dir server check above?
         */
        if (smartlist_contains_digest(missing_id_digests,
                                      voter->identity_digest))
          continue;
      }

      SMARTLIST_FOREACH_BEGIN(voter->sigs, document_signature_t *, sig) {
        cert = authority_cert_get_by_digests(voter->identity_digest,
                                             sig->signing_key_digest);
        if (cert) {
          if (now < cert->expires)
            download_status_reset_by_sk_in_cl(cl, sig->signing_key_digest);
          continue;
        }
        if (download_status_is_ready_by_sk_in_cl(
              cl, sig->signing_key_digest,
              now, get_options()->TestingCertMaxDownloadTries) &&
            !fp_pair_map_get_by_digests(pending_cert,
                                        voter->identity_digest,
                                        sig->signing_key_digest)) {
          /*
           * Do this rather than hex_str(), since hex_str clobbers
           * old results and we call twice in the param list.
           */
          base16_encode(id_digest_str, sizeof(id_digest_str),
                        voter->identity_digest, DIGEST_LEN);
          base16_encode(sk_digest_str, sizeof(sk_digest_str),
                        sig->signing_key_digest, DIGEST_LEN);

          if (voter->nickname) {
            log_info(LD_DIR,
                     "We're missing a certificate from authority %s "
                     "(ID digest %s) with signing key %s: "
                     "launching request.",
                     voter->nickname, id_digest_str, sk_digest_str);
          } else {
            log_info(LD_DIR,
                     "We're missing a certificate from authority ID digest "
                     "%s with signing key %s: launching request.",
                     id_digest_str, sk_digest_str);
          }

          /* Allocate a new fp_pair_t to append */
          fp_tmp = tor_malloc(sizeof(*fp_tmp));
          memcpy(fp_tmp->first, voter->identity_digest, sizeof(fp_tmp->first));
          memcpy(fp_tmp->second, sig->signing_key_digest,
                 sizeof(fp_tmp->second));
          smartlist_add(missing_cert_digests, fp_tmp);
        }
      } SMARTLIST_FOREACH_END(sig);
    } SMARTLIST_FOREACH_END(voter);
  }

  /* Do downloads by identity digest */
  if (smartlist_len(missing_id_digests) > 0) {
    int need_plus = 0;
    smartlist_t *fps = smartlist_new();

    smartlist_add(fps, tor_strdup("fp/"));

    SMARTLIST_FOREACH_BEGIN(missing_id_digests, const char *, d) {
      char *fp = NULL;

      if (digestmap_get(pending_id, d))
        continue;

      base16_encode(id_digest_str, sizeof(id_digest_str),
                    d, DIGEST_LEN);

      if (need_plus) {
        tor_asprintf(&fp, "+%s", id_digest_str);
      } else {
        /* No need for tor_asprintf() in this case; first one gets no '+' */
        fp = tor_strdup(id_digest_str);
        need_plus = 1;
      }

      smartlist_add(fps, fp);
    } SMARTLIST_FOREACH_END(d);

    if (smartlist_len(fps) > 1) {
      resource = smartlist_join_strings(fps, "", 0, NULL);
      directory_get_from_dirserver(DIR_PURPOSE_FETCH_CERTIFICATE, 0,
                                   resource, PDS_RETRY_IF_NO_SERVERS);
      tor_free(resource);
    }
    /* else we didn't add any: they were all pending */

    SMARTLIST_FOREACH(fps, char *, cp, tor_free(cp));
    smartlist_free(fps);
  }

  /* Do downloads by identity digest/signing key pair */
  if (smartlist_len(missing_cert_digests) > 0) {
    int need_plus = 0;
    smartlist_t *fp_pairs = smartlist_new();

    smartlist_add(fp_pairs, tor_strdup("fp-sk/"));

    SMARTLIST_FOREACH_BEGIN(missing_cert_digests, const fp_pair_t *, d) {
      char *fp_pair = NULL;

      if (fp_pair_map_get(pending_cert, d))
        continue;

      /* Construct string encodings of the digests */
      base16_encode(id_digest_str, sizeof(id_digest_str),
                    d->first, DIGEST_LEN);
      base16_encode(sk_digest_str, sizeof(sk_digest_str),
                    d->second, DIGEST_LEN);

      /* Now tor_asprintf() */
      if (need_plus) {
        tor_asprintf(&fp_pair, "+%s-%s", id_digest_str, sk_digest_str);
      } else {
        /* First one in the list doesn't get a '+' */
        tor_asprintf(&fp_pair, "%s-%s", id_digest_str, sk_digest_str);
        need_plus = 1;
      }

      /* Add it to the list of pairs to request */
      smartlist_add(fp_pairs, fp_pair);
    } SMARTLIST_FOREACH_END(d);

    if (smartlist_len(fp_pairs) > 1) {
      resource = smartlist_join_strings(fp_pairs, "", 0, NULL);
      directory_get_from_dirserver(DIR_PURPOSE_FETCH_CERTIFICATE, 0,
                                   resource, PDS_RETRY_IF_NO_SERVERS);
      tor_free(resource);
    }
    /* else they were all pending */

    SMARTLIST_FOREACH(fp_pairs, char *, p, tor_free(p));
    smartlist_free(fp_pairs);
  }

  smartlist_free(missing_id_digests);
  SMARTLIST_FOREACH(missing_cert_digests, fp_pair_t *, p, tor_free(p));
  smartlist_free(missing_cert_digests);
  digestmap_free(pending_id, NULL);
  fp_pair_map_free(pending_cert, NULL);
}

/* Router descriptor storage.
 *
 * Routerdescs are stored in a big file, named "cached-descriptors".  As new
 * routerdescs arrive, we append them to a journal file named
 * "cached-descriptors.new".
 *
 * From time to time, we replace "cached-descriptors" with a new file
 * containing only the live, non-superseded descriptors, and clear
 * cached-routers.new.
 *
 * On startup, we read both files.
 */

/** Helper: return 1 iff the router log is so big we want to rebuild the
 * store. */
static int
router_should_rebuild_store(desc_store_t *store)
{
  if (store->store_len > (1<<16))
    return (store->journal_len > store->store_len / 2 ||
            store->bytes_dropped > store->store_len / 2);
  else
    return store->journal_len > (1<<15);
}

/** Return the desc_store_t in <b>rl</b> that should be used to store
 * <b>sd</b>. */
static INLINE desc_store_t *
desc_get_store(routerlist_t *rl, const signed_descriptor_t *sd)
{
  if (sd->is_extrainfo)
    return &rl->extrainfo_store;
  else
    return &rl->desc_store;
}

/** Add the signed_descriptor_t in <b>desc</b> to the router
 * journal; change its saved_location to SAVED_IN_JOURNAL and set its
 * offset appropriately. */
static int
signed_desc_append_to_journal(signed_descriptor_t *desc,
                              desc_store_t *store)
{
  char *fname = get_datadir_fname_suffix(store->fname_base, ".new");
  const char *body = signed_descriptor_get_body_impl(desc,1);
  size_t len = desc->signed_descriptor_len + desc->annotations_len;

  if (append_bytes_to_file(fname, body, len, 1)) {
    log_warn(LD_FS, "Unable to store router descriptor");
    tor_free(fname);
    return -1;
  }
  desc->saved_location = SAVED_IN_JOURNAL;
  tor_free(fname);

  desc->saved_offset = store->journal_len;
  store->journal_len += len;

  return 0;
}

/** Sorting helper: return &lt;0, 0, or &gt;0 depending on whether the
 * signed_descriptor_t* in *<b>a</b> is older, the same age as, or newer than
 * the signed_descriptor_t* in *<b>b</b>. */
static int
compare_signed_descriptors_by_age_(const void **_a, const void **_b)
{
  const signed_descriptor_t *r1 = *_a, *r2 = *_b;
  return (int)(r1->published_on - r2->published_on);
}

#define RRS_FORCE 1
#define RRS_DONT_REMOVE_OLD 2

/** If the journal of <b>store</b> is too long, or if RRS_FORCE is set in
 * <b>flags</b>, then atomically replace the saved router store with the
 * routers currently in our routerlist, and clear the journal.  Unless
 * RRS_DONT_REMOVE_OLD is set in <b>flags</b>, delete expired routers before
 * rebuilding the store.  Return 0 on success, -1 on failure.
 */
static int
router_rebuild_store(int flags, desc_store_t *store)
{
  smartlist_t *chunk_list = NULL;
  char *fname = NULL, *fname_tmp = NULL;
  int r = -1;
  off_t offset = 0;
  smartlist_t *signed_descriptors = NULL;
  int nocache=0;
  size_t total_expected_len = 0;
  int had_any;
  int force = flags & RRS_FORCE;

  if (!force && !router_should_rebuild_store(store)) {
    r = 0;
    goto done;
  }
  if (!routerlist) {
    r = 0;
    goto done;
  }

  if (store->type == EXTRAINFO_STORE)
    had_any = !eimap_isempty(routerlist->extra_info_map);
  else
    had_any = (smartlist_len(routerlist->routers)+
               smartlist_len(routerlist->old_routers))>0;

  /* Don't save deadweight. */
  if (!(flags & RRS_DONT_REMOVE_OLD))
    routerlist_remove_old_routers();

  log_info(LD_DIR, "Rebuilding %s cache", store->description);

  fname = get_datadir_fname(store->fname_base);
  fname_tmp = get_datadir_fname_suffix(store->fname_base, ".tmp");

  chunk_list = smartlist_new();

  /* We sort the routers by age to enhance locality on disk. */
  signed_descriptors = smartlist_new();
  if (store->type == EXTRAINFO_STORE) {
    eimap_iter_t *iter;
    for (iter = eimap_iter_init(routerlist->extra_info_map);
         !eimap_iter_done(iter);
         iter = eimap_iter_next(routerlist->extra_info_map, iter)) {
      const char *key;
      extrainfo_t *ei;
      eimap_iter_get(iter, &key, &ei);
      smartlist_add(signed_descriptors, &ei->cache_info);
    }
  } else {
    SMARTLIST_FOREACH(routerlist->old_routers, signed_descriptor_t *, sd,
                      smartlist_add(signed_descriptors, sd));
    SMARTLIST_FOREACH(routerlist->routers, routerinfo_t *, ri,
                      smartlist_add(signed_descriptors, &ri->cache_info));
  }

  smartlist_sort(signed_descriptors, compare_signed_descriptors_by_age_);

  /* Now, add the appropriate members to chunk_list */
  SMARTLIST_FOREACH_BEGIN(signed_descriptors, signed_descriptor_t *, sd) {
      sized_chunk_t *c;
      const char *body = signed_descriptor_get_body_impl(sd, 1);
      if (!body) {
        log_warn(LD_BUG, "No descriptor available for router.");
        goto done;
      }
      if (sd->do_not_cache) {
        ++nocache;
        continue;
      }
      c = tor_malloc(sizeof(sized_chunk_t));
      c->bytes = body;
      c->len = sd->signed_descriptor_len + sd->annotations_len;
      total_expected_len += c->len;
      smartlist_add(chunk_list, c);
  } SMARTLIST_FOREACH_END(sd);

  if (write_chunks_to_file(fname_tmp, chunk_list, 1, 1)<0) {
    log_warn(LD_FS, "Error writing router store to disk.");
    goto done;
  }

  /* Our mmap is now invalid. */
  if (store->mmap) {
    int res = tor_munmap_file(store->mmap);
    store->mmap = NULL;
    if (res != 0) {
      log_warn(LD_FS, "Unable to munmap route store in %s", fname);
    }
  }

  if (replace_file(fname_tmp, fname)<0) {
    log_warn(LD_FS, "Error replacing old router store: %s", strerror(errno));
    goto done;
  }

  errno = 0;
  store->mmap = tor_mmap_file(fname);
  if (! store->mmap) {
    if (errno == ERANGE) {
      /* empty store.*/
      if (total_expected_len) {
        log_warn(LD_FS, "We wrote some bytes to a new descriptor file at '%s',"
                 " but when we went to mmap it, it was empty!", fname);
      } else if (had_any) {
        log_info(LD_FS, "We just removed every descriptor in '%s'.  This is "
                 "okay if we're just starting up after a long time. "
                 "Otherwise, it's a bug.", fname);
      }
    } else {
      log_warn(LD_FS, "Unable to mmap new descriptor file at '%s'.",fname);
    }
  }

  log_info(LD_DIR, "Reconstructing pointers into cache");

  offset = 0;
  SMARTLIST_FOREACH_BEGIN(signed_descriptors, signed_descriptor_t *, sd) {
      if (sd->do_not_cache)
        continue;
      sd->saved_location = SAVED_IN_CACHE;
      if (store->mmap) {
        tor_free(sd->signed_descriptor_body); // sets it to null
        sd->saved_offset = offset;
      }
      offset += sd->signed_descriptor_len + sd->annotations_len;
      signed_descriptor_get_body(sd); /* reconstruct and assert */
  } SMARTLIST_FOREACH_END(sd);

  tor_free(fname);
  fname = get_datadir_fname_suffix(store->fname_base, ".new");
  write_str_to_file(fname, "", 1);

  r = 0;
  store->store_len = (size_t) offset;
  store->journal_len = 0;
  store->bytes_dropped = 0;
 done:
  smartlist_free(signed_descriptors);
  tor_free(fname);
  tor_free(fname_tmp);
  if (chunk_list) {
    SMARTLIST_FOREACH(chunk_list, sized_chunk_t *, c, tor_free(c));
    smartlist_free(chunk_list);
  }

  return r;
}

/** Helper: Reload a cache file and its associated journal, setting metadata
 * appropriately.  If <b>extrainfo</b> is true, reload the extrainfo store;
 * else reload the router descriptor store. */
static int
router_reload_router_list_impl(desc_store_t *store)
{
  char *fname = NULL, *contents = NULL;
  struct stat st;
  int extrainfo = (store->type == EXTRAINFO_STORE);
  store->journal_len = store->store_len = 0;

  fname = get_datadir_fname(store->fname_base);

  if (store->mmap) {
    /* get rid of it first */
    int res = tor_munmap_file(store->mmap);
    store->mmap = NULL;
    if (res != 0) {
      log_warn(LD_FS, "Failed to munmap %s", fname);
      tor_free(fname);
      return -1;
    }
  }

  store->mmap = tor_mmap_file(fname);
  if (store->mmap) {
    store->store_len = store->mmap->size;
    if (extrainfo)
      router_load_extrainfo_from_string(store->mmap->data,
                                        store->mmap->data+store->mmap->size,
                                        SAVED_IN_CACHE, NULL, 0);
    else
      router_load_routers_from_string(store->mmap->data,
                                      store->mmap->data+store->mmap->size,
                                      SAVED_IN_CACHE, NULL, 0, NULL);
  }

  tor_free(fname);
  fname = get_datadir_fname_suffix(store->fname_base, ".new");
  if (file_status(fname) == FN_FILE)
    contents = read_file_to_str(fname, RFTS_BIN|RFTS_IGNORE_MISSING, &st);
  if (contents) {
    if (extrainfo)
      router_load_extrainfo_from_string(contents, NULL,SAVED_IN_JOURNAL,
                                        NULL, 0);
    else
      router_load_routers_from_string(contents, NULL, SAVED_IN_JOURNAL,
                                      NULL, 0, NULL);
    store->journal_len = (size_t) st.st_size;
    tor_free(contents);
  }

  tor_free(fname);

  if (store->journal_len) {
    /* Always clear the journal on startup.*/
    router_rebuild_store(RRS_FORCE, store);
  } else if (!extrainfo) {
    /* Don't cache expired routers. (This is in an else because
     * router_rebuild_store() also calls remove_old_routers().) */
    routerlist_remove_old_routers();
  }

  return 0;
}

/** Load all cached router descriptors and extra-info documents from the
 * store. Return 0 on success and -1 on failure.
 */
int
router_reload_router_list(void)
{
  routerlist_t *rl = router_get_routerlist();
  if (router_reload_router_list_impl(&rl->desc_store))
    return -1;
  if (router_reload_router_list_impl(&rl->extrainfo_store))
    return -1;
  return 0;
}

/** Return a smartlist containing a list of dir_server_t * for all
 * known trusted dirservers.  Callers must not modify the list or its
 * contents.
 */
const smartlist_t *
router_get_trusted_dir_servers(void)
{
  if (!trusted_dir_servers)
    trusted_dir_servers = smartlist_new();

  return trusted_dir_servers;
}

const smartlist_t *
router_get_fallback_dir_servers(void)
{
  if (!fallback_dir_servers)
    fallback_dir_servers = smartlist_new();

  return fallback_dir_servers;
}

/** Try to find a running dirserver that supports operations of <b>type</b>.
 *
 * If there are no running dirservers in our routerlist and the
 * <b>PDS_RETRY_IF_NO_SERVERS</b> flag is set, set all the authoritative ones
 * as running again, and pick one.
 *
 * If the <b>PDS_IGNORE_FASCISTFIREWALL</b> flag is set, then include
 * dirservers that we can't reach.
 *
 * If the <b>PDS_ALLOW_SELF</b> flag is not set, then don't include ourself
 * (if we're a dirserver).
 *
 * Don't pick an authority if any non-authority is viable; try to avoid using
 * servers that have returned 503 recently.
 */
const routerstatus_t *
router_pick_directory_server(dirinfo_type_t type, int flags)
{
  const routerstatus_t *choice;

  if (!routerlist)
    return NULL;

  choice = router_pick_directory_server_impl(type, flags);
  if (choice || !(flags & PDS_RETRY_IF_NO_SERVERS))
    return choice;

  log_info(LD_DIR,
           "No reachable router entries for dirservers. "
           "Trying them all again.");
  /* mark all authdirservers as up again */
  mark_all_dirservers_up(fallback_dir_servers);
  /* try again */
  choice = router_pick_directory_server_impl(type, flags);
  return choice;
}

/** Return the dir_server_t for the directory authority whose identity
 * key hashes to <b>digest</b>, or NULL if no such authority is known.
 */
dir_server_t *
router_get_trusteddirserver_by_digest(const char *digest)
{
  if (!trusted_dir_servers)
    return NULL;

  SMARTLIST_FOREACH(trusted_dir_servers, dir_server_t *, ds,
     {
       if (tor_memeq(ds->digest, digest, DIGEST_LEN))
         return ds;
     });

  return NULL;
}

/** Return the dir_server_t for the fallback dirserver whose identity
 * key hashes to <b>digest</b>, or NULL if no such authority is known.
 */
dir_server_t *
router_get_fallback_dirserver_by_digest(const char *digest)
{
  if (!trusted_dir_servers)
    return NULL;

  SMARTLIST_FOREACH(trusted_dir_servers, dir_server_t *, ds,
     {
       if (tor_memeq(ds->digest, digest, DIGEST_LEN))
         return ds;
     });

  return NULL;
}

/** Return the dir_server_t for the directory authority whose
 * v3 identity key hashes to <b>digest</b>, or NULL if no such authority
 * is known.
 */
dir_server_t *
trusteddirserver_get_by_v3_auth_digest(const char *digest)
{
  if (!trusted_dir_servers)
    return NULL;

  SMARTLIST_FOREACH(trusted_dir_servers, dir_server_t *, ds,
     {
       if (tor_memeq(ds->v3_identity_digest, digest, DIGEST_LEN) &&
           (ds->type & V3_DIRINFO))
         return ds;
     });

  return NULL;
}

/** Try to find a running directory authority. Flags are as for
 * router_pick_directory_server.
 */
const routerstatus_t *
router_pick_trusteddirserver(dirinfo_type_t type, int flags)
{
  return router_pick_dirserver_generic(trusted_dir_servers, type, flags);
}

/** Try to find a running fallback directory. Flags are as for
 * router_pick_directory_server.
 */
const routerstatus_t *
router_pick_fallback_dirserver(dirinfo_type_t type, int flags)
{
  return router_pick_dirserver_generic(fallback_dir_servers, type, flags);
}

/** Try to find a running fallback directory. Flags are as for
 * router_pick_directory_server.
 */
static const routerstatus_t *
router_pick_dirserver_generic(smartlist_t *sourcelist,
                              dirinfo_type_t type, int flags)
{
  const routerstatus_t *choice;
  int busy = 0;

  choice = router_pick_trusteddirserver_impl(sourcelist, type, flags, &busy);
  if (choice || !(flags & PDS_RETRY_IF_NO_SERVERS))
    return choice;
  if (busy) {
    /* If the reason that we got no server is that servers are "busy",
     * we must be excluding good servers because we already have serverdesc
     * fetches with them.  Do not mark down servers up because of this. */
    tor_assert((flags & (PDS_NO_EXISTING_SERVERDESC_FETCH|
                         PDS_NO_EXISTING_MICRODESC_FETCH)));
    return NULL;
  }

  log_info(LD_DIR,
           "No dirservers are reachable. Trying them all again.");
  mark_all_dirservers_up(sourcelist);
  return router_pick_trusteddirserver_impl(sourcelist, type, flags, NULL);
}

/** How long do we avoid using a directory server after it's given us a 503? */
#define DIR_503_TIMEOUT (60*60)

/** Pick a random running valid directory server/mirror from our
 * routerlist.  Arguments are as for router_pick_directory_server(), except
 * that RETRY_IF_NO_SERVERS is ignored.
 */
static const routerstatus_t *
router_pick_directory_server_impl(dirinfo_type_t type, int flags)
{
  const or_options_t *options = get_options();
  const node_t *result;
  smartlist_t *direct, *tunnel;
  smartlist_t *trusted_direct, *trusted_tunnel;
  smartlist_t *overloaded_direct, *overloaded_tunnel;
  time_t now = time(NULL);
  const networkstatus_t *consensus = networkstatus_get_latest_consensus();
  int requireother = ! (flags & PDS_ALLOW_SELF);
  int fascistfirewall = ! (flags & PDS_IGNORE_FASCISTFIREWALL);
  int for_guard = (flags & PDS_FOR_GUARD);
  int try_excluding = 1, n_excluded = 0;

  if (!consensus)
    return NULL;

 retry_without_exclude:

  direct = smartlist_new();
  tunnel = smartlist_new();
  trusted_direct = smartlist_new();
  trusted_tunnel = smartlist_new();
  overloaded_direct = smartlist_new();
  overloaded_tunnel = smartlist_new();

  /* Find all the running dirservers we know about. */
  SMARTLIST_FOREACH_BEGIN(nodelist_get_list(), const node_t *, node) {
    int is_trusted;
    int is_overloaded;
    tor_addr_t addr;
    const routerstatus_t *status = node->rs;
    const country_t country = node->country;
    if (!status)
      continue;

    if (!node->is_running || !status->dir_port || !node->is_valid)
      continue;
    if (node->is_bad_directory)
      continue;
    if (requireother && router_digest_is_me(node->identity))
      continue;
    is_trusted = router_digest_is_trusted_dir(node->identity);
    if ((type & EXTRAINFO_DIRINFO) &&
        !router_supports_extrainfo(node->identity, 0))
      continue;
    if ((type & MICRODESC_DIRINFO) && !is_trusted &&
        !node->rs->version_supports_microdesc_cache)
      continue;
    if (for_guard && node->using_as_guard)
      continue; /* Don't make the same node a guard twice. */
    if (try_excluding &&
        routerset_contains_routerstatus(options->ExcludeNodes, status,
                                        country)) {
      ++n_excluded;
      continue;
    }

    /* XXXX IP6 proposal 118 */
    tor_addr_from_ipv4h(&addr, node->rs->addr);

    is_overloaded = status->last_dir_503_at + DIR_503_TIMEOUT > now;

    if ((!fascistfirewall ||
         fascist_firewall_allows_address_or(&addr, status->or_port)))
      smartlist_add(is_trusted ? trusted_tunnel :
                    is_overloaded ? overloaded_tunnel : tunnel, (void*)node);
    else if (!fascistfirewall ||
             fascist_firewall_allows_address_dir(&addr, status->dir_port))
      smartlist_add(is_trusted ? trusted_direct :
                    is_overloaded ? overloaded_direct : direct, (void*)node);
  } SMARTLIST_FOREACH_END(node);

  if (smartlist_len(tunnel)) {
    result = node_sl_choose_by_bandwidth(tunnel, WEIGHT_FOR_DIR);
  } else if (smartlist_len(overloaded_tunnel)) {
    result = node_sl_choose_by_bandwidth(overloaded_tunnel,
                                                 WEIGHT_FOR_DIR);
  } else if (smartlist_len(trusted_tunnel)) {
    /* FFFF We don't distinguish between trusteds and overloaded trusteds
     * yet. Maybe one day we should. */
    /* FFFF We also don't load balance over authorities yet. I think this
     * is a feature, but it could easily be a bug. -RD */
    result = smartlist_choose(trusted_tunnel);
  } else if (smartlist_len(direct)) {
    result = node_sl_choose_by_bandwidth(direct, WEIGHT_FOR_DIR);
  } else if (smartlist_len(overloaded_direct)) {
    result = node_sl_choose_by_bandwidth(overloaded_direct,
                                         WEIGHT_FOR_DIR);
  } else {
    result = smartlist_choose(trusted_direct);
  }
  smartlist_free(direct);
  smartlist_free(tunnel);
  smartlist_free(trusted_direct);
  smartlist_free(trusted_tunnel);
  smartlist_free(overloaded_direct);
  smartlist_free(overloaded_tunnel);

  if (result == NULL && try_excluding && !options->StrictNodes && n_excluded) {
    /* If we got no result, and we are excluding nodes, and StrictNodes is
     * not set, try again without excluding nodes. */
    try_excluding = 0;
    n_excluded = 0;
    goto retry_without_exclude;
  }

  return result ? result->rs : NULL;
}

/** Pick a random element from a list of dir_server_t, weighting by their
 * <b>weight</b> field. */
static const dir_server_t *
dirserver_choose_by_weight(const smartlist_t *servers, double authority_weight)
{
  int n = smartlist_len(servers);
  int i;
  u64_dbl_t *weights;
  const dir_server_t *ds;

  weights = tor_malloc(sizeof(u64_dbl_t) * n);
  for (i = 0; i < n; ++i) {
    ds = smartlist_get(servers, i);
    weights[i].dbl = ds->weight;
    if (ds->is_authority)
      weights[i].dbl *= authority_weight;
  }

  scale_array_elements_to_u64(weights, n, NULL);
  i = choose_array_element_by_weight(weights, n);
  tor_free(weights);
  return (i < 0) ? NULL : smartlist_get(servers, i);
}

/** Choose randomly from among the dir_server_ts in sourcelist that
 * are up. Flags are as for router_pick_directory_server_impl().
 */
static const routerstatus_t *
router_pick_trusteddirserver_impl(const smartlist_t *sourcelist,
                                  dirinfo_type_t type, int flags,
                                  int *n_busy_out)
{
  const or_options_t *options = get_options();
  smartlist_t *direct, *tunnel;
  smartlist_t *overloaded_direct, *overloaded_tunnel;
  const routerinfo_t *me = router_get_my_routerinfo();
  const routerstatus_t *result = NULL;
  time_t now = time(NULL);
  const int requireother = ! (flags & PDS_ALLOW_SELF);
  const int fascistfirewall = ! (flags & PDS_IGNORE_FASCISTFIREWALL);
  const int no_serverdesc_fetching =(flags & PDS_NO_EXISTING_SERVERDESC_FETCH);
  const int no_microdesc_fetching =(flags & PDS_NO_EXISTING_MICRODESC_FETCH);
  const double auth_weight = (sourcelist == fallback_dir_servers) ?
    options->DirAuthorityFallbackRate : 1.0;
  smartlist_t *pick_from;
  int n_busy = 0;
  int try_excluding = 1, n_excluded = 0;

  if (!sourcelist)
    return NULL;

 retry_without_exclude:

  direct = smartlist_new();
  tunnel = smartlist_new();
  overloaded_direct = smartlist_new();
  overloaded_tunnel = smartlist_new();

  SMARTLIST_FOREACH_BEGIN(sourcelist, const dir_server_t *, d)
    {
      int is_overloaded =
          d->fake_status.last_dir_503_at + DIR_503_TIMEOUT > now;
      tor_addr_t addr;
      if (!d->is_running) continue;
      if ((type & d->type) == 0)
        continue;
      if ((type & EXTRAINFO_DIRINFO) &&
          !router_supports_extrainfo(d->digest, 1))
        continue;
      if (requireother && me && router_digest_is_me(d->digest))
          continue;
      if (try_excluding &&
          routerset_contains_routerstatus(options->ExcludeNodes,
                                          &d->fake_status, -1)) {
        ++n_excluded;
        continue;
      }

      /* XXXX IP6 proposal 118 */
      tor_addr_from_ipv4h(&addr, d->addr);

      if (no_serverdesc_fetching) {
        if (connection_get_by_type_addr_port_purpose(
            CONN_TYPE_DIR, &addr, d->dir_port, DIR_PURPOSE_FETCH_SERVERDESC)
         || connection_get_by_type_addr_port_purpose(
             CONN_TYPE_DIR, &addr, d->dir_port, DIR_PURPOSE_FETCH_EXTRAINFO)) {
          //log_debug(LD_DIR, "We have an existing connection to fetch "
          //           "descriptor from %s; delaying",d->description);
          ++n_busy;
          continue;
        }
      }
      if (no_microdesc_fetching) {
        if (connection_get_by_type_addr_port_purpose(
             CONN_TYPE_DIR, &addr, d->dir_port, DIR_PURPOSE_FETCH_MICRODESC)) {
          ++n_busy;
          continue;
        }
      }

      if (d->or_port &&
          (!fascistfirewall ||
           fascist_firewall_allows_address_or(&addr, d->or_port)))
        smartlist_add(is_overloaded ? overloaded_tunnel : tunnel, (void*)d);
      else if (!fascistfirewall ||
               fascist_firewall_allows_address_dir(&addr, d->dir_port))
        smartlist_add(is_overloaded ? overloaded_direct : direct, (void*)d);
    }
  SMARTLIST_FOREACH_END(d);

  if (smartlist_len(tunnel)) {
    pick_from = tunnel;
  } else if (smartlist_len(overloaded_tunnel)) {
    pick_from = overloaded_tunnel;
  } else if (smartlist_len(direct)) {
    pick_from = direct;
  } else {
    pick_from = overloaded_direct;
  }

  {
    const dir_server_t *selection =
      dirserver_choose_by_weight(pick_from, auth_weight);

    if (selection)
      result = &selection->fake_status;
  }

  if (n_busy_out)
    *n_busy_out = n_busy;

  smartlist_free(direct);
  smartlist_free(tunnel);
  smartlist_free(overloaded_direct);
  smartlist_free(overloaded_tunnel);

  if (result == NULL && try_excluding && !options->StrictNodes && n_excluded) {
    /* If we got no result, and we are excluding nodes, and StrictNodes is
     * not set, try again without excluding nodes. */
    try_excluding = 0;
    n_excluded = 0;
    goto retry_without_exclude;
  }

  return result;
}

/** Mark as running every dir_server_t in <b>server_list</b>. */
static void
mark_all_dirservers_up(smartlist_t *server_list)
{
  if (server_list) {
    SMARTLIST_FOREACH_BEGIN(server_list, dir_server_t *, dir) {
      routerstatus_t *rs;
      node_t *node;
      dir->is_running = 1;
      node = node_get_mutable_by_id(dir->digest);
      if (node)
        node->is_running = 1;
      rs = router_get_mutable_consensus_status_by_id(dir->digest);
      if (rs) {
        rs->last_dir_503_at = 0;
        control_event_networkstatus_changed_single(rs);
      }
    } SMARTLIST_FOREACH_END(dir);
  }
  router_dir_info_changed();
}

/** Return true iff r1 and r2 have the same address and OR port. */
int
routers_have_same_or_addrs(const routerinfo_t *r1, const routerinfo_t *r2)
{
  return r1->addr == r2->addr && r1->or_port == r2->or_port &&
    tor_addr_eq(&r1->ipv6_addr, &r2->ipv6_addr) &&
    r1->ipv6_orport == r2->ipv6_orport;
}

/** Reset all internal variables used to count failed downloads of network
 * status objects. */
void
router_reset_status_download_failures(void)
{
  mark_all_dirservers_up(fallback_dir_servers);
}

/** Given a <b>router</b>, add every node_t in its family (including the
 * node itself!) to <b>sl</b>.
 *
 * Note the type mismatch: This function takes a routerinfo, but adds nodes
 * to the smartlist!
 */
static void
routerlist_add_node_and_family(smartlist_t *sl, const routerinfo_t *router)
{
  /* XXXX MOVE ? */
  node_t fake_node;
  const node_t *node = node_get_by_id(router->cache_info.identity_digest);;
  if (node == NULL) {
    memset(&fake_node, 0, sizeof(fake_node));
    fake_node.ri = (routerinfo_t *)router;
    memcpy(fake_node.identity, router->cache_info.identity_digest, DIGEST_LEN);
    node = &fake_node;
  }
  nodelist_add_node_and_family(sl, node);
}

/** Add every suitable node from our nodelist to <b>sl</b>, so that
 * we can pick a node for a circuit.
 */
static void
router_add_running_nodes_to_smartlist(smartlist_t *sl, int allow_invalid,
                                      int need_uptime, int need_capacity,
                                      int need_guard, int need_desc)
{ /* XXXX MOVE */
  SMARTLIST_FOREACH_BEGIN(nodelist_get_list(), const node_t *, node) {
    if (!node->is_running ||
        (!node->is_valid && !allow_invalid))
      continue;
    if (need_desc && !(node->ri || (node->rs && node->md)))
      continue;
    if (node->ri && node->ri->purpose != ROUTER_PURPOSE_GENERAL)
      continue;
    if (node_is_unreliable(node, need_uptime, need_capacity, need_guard))
      continue;

    smartlist_add(sl, (void *)node);
  } SMARTLIST_FOREACH_END(node);
}

/** Look through the routerlist until we find a router that has my key.
 Return it. */
const routerinfo_t *
routerlist_find_my_routerinfo(void)
{
  if (!routerlist)
    return NULL;

  SMARTLIST_FOREACH(routerlist->routers, routerinfo_t *, router,
  {
    if (router_is_me(router))
      return router;
  });
  return NULL;
}

/** Return the smaller of the router's configured BandwidthRate
 * and its advertised capacity. */
uint32_t
router_get_advertised_bandwidth(const routerinfo_t *router)
{
  if (router->bandwidthcapacity < router->bandwidthrate)
    return router->bandwidthcapacity;
  return router->bandwidthrate;
}

/** Do not weight any declared bandwidth more than this much when picking
 * routers by bandwidth. */
#define DEFAULT_MAX_BELIEVABLE_BANDWIDTH 10000000 /* 10 MB/sec */

/** Return the smaller of the router's configured BandwidthRate
 * and its advertised capacity, capped by max-believe-bw. */
uint32_t
router_get_advertised_bandwidth_capped(const routerinfo_t *router)
{
  uint32_t result = router->bandwidthcapacity;
  if (result > router->bandwidthrate)
    result = router->bandwidthrate;
  if (result > DEFAULT_MAX_BELIEVABLE_BANDWIDTH)
    result = DEFAULT_MAX_BELIEVABLE_BANDWIDTH;
  return result;
}

/** Given an array of double/uint64_t unions that are currently being used as
 * doubles, convert them to uint64_t, and try to scale them linearly so as to
 * much of the range of uint64_t. If <b>total_out</b> is provided, set it to
 * the sum of all elements in the array _before_ scaling. */
STATIC void
scale_array_elements_to_u64(u64_dbl_t *entries, int n_entries,
                            uint64_t *total_out)
{
  double total = 0.0;
  double scale_factor;
  int i;
  /* big, but far away from overflowing an int64_t */
#define SCALE_TO_U64_MAX (INT64_MAX / 4)

  for (i = 0; i < n_entries; ++i)
    total += entries[i].dbl;

  scale_factor = SCALE_TO_U64_MAX / total;

  for (i = 0; i < n_entries; ++i)
    entries[i].u64 = tor_llround(entries[i].dbl * scale_factor);

  if (total_out)
    *total_out = (uint64_t) total;

#undef SCALE_TO_U64_MAX
}

/** Time-invariant 64-bit greater-than; works on two integers in the range
 * (0,INT64_MAX). */
#if SIZEOF_VOID_P == 8
#define gt_i64_timei(a,b) ((a) > (b))
#else
static INLINE int
gt_i64_timei(uint64_t a, uint64_t b)
{
  int64_t diff = (int64_t) (b - a);
  int res = diff >> 63;
  return res & 1;
}
#endif

/** Pick a random element of <b>n_entries</b>-element array <b>entries</b>,
 * choosing each element with a probability proportional to its (uint64_t)
 * value, and return the index of that element.  If all elements are 0, choose
 * an index at random. Return -1 on error.
 */
STATIC int
choose_array_element_by_weight(const u64_dbl_t *entries, int n_entries)
{
  int i, i_chosen=-1, n_chosen=0;
  uint64_t total_so_far = 0;
  uint64_t rand_val;
  uint64_t total = 0;

  for (i = 0; i < n_entries; ++i)
    total += entries[i].u64;

  if (n_entries < 1)
    return -1;

  if (total == 0)
    return crypto_rand_int(n_entries);

  tor_assert(total < INT64_MAX);

  rand_val = crypto_rand_uint64(total);

  for (i = 0; i < n_entries; ++i) {
    total_so_far += entries[i].u64;
    if (gt_i64_timei(total_so_far, rand_val)) {
      i_chosen = i;
      n_chosen++;
      /* Set rand_val to INT64_MAX rather than stopping the loop. This way,
       * the time we spend in the loop does not leak which element we chose. */
      rand_val = INT64_MAX;
    }
  }
  tor_assert(total_so_far == total);
  tor_assert(n_chosen == 1);
  tor_assert(i_chosen >= 0);
  tor_assert(i_chosen < n_entries);

  return i_chosen;
}

/** When weighting bridges, enforce these values as lower and upper
 * bound for believable bandwidth, because there is no way for us
 * to verify a bridge's bandwidth currently. */
#define BRIDGE_MIN_BELIEVABLE_BANDWIDTH 20000  /* 20 kB/sec */
#define BRIDGE_MAX_BELIEVABLE_BANDWIDTH 100000 /* 100 kB/sec */

/** Return the smaller of the router's configured BandwidthRate
 * and its advertised capacity, making sure to stay within the
 * interval between bridge-min-believe-bw and
 * bridge-max-believe-bw. */
static uint32_t
bridge_get_advertised_bandwidth_bounded(routerinfo_t *router)
{
  uint32_t result = router->bandwidthcapacity;
  if (result > router->bandwidthrate)
    result = router->bandwidthrate;
  if (result > BRIDGE_MAX_BELIEVABLE_BANDWIDTH)
    result = BRIDGE_MAX_BELIEVABLE_BANDWIDTH;
  else if (result < BRIDGE_MIN_BELIEVABLE_BANDWIDTH)
    result = BRIDGE_MIN_BELIEVABLE_BANDWIDTH;
  return result;
}

/** Return bw*1000, unless bw*1000 would overflow, in which case return
 * INT32_MAX. */
static INLINE int32_t
kb_to_bytes(uint32_t bw)
{
  return (bw > (INT32_MAX/1000)) ? INT32_MAX : bw*1000;
}

/** Helper function:
 * choose a random element of smartlist <b>sl</b> of nodes, weighted by
 * the advertised bandwidth of each element using the consensus
 * bandwidth weights.
 *
 * If <b>rule</b>==WEIGHT_FOR_EXIT. we're picking an exit node: consider all
 * nodes' bandwidth equally regardless of their Exit status, since there may
 * be some in the list because they exit to obscure ports. If
 * <b>rule</b>==NO_WEIGHTING, we're picking a non-exit node: weight
 * exit-node's bandwidth less depending on the smallness of the fraction of
 * Exit-to-total bandwidth.  If <b>rule</b>==WEIGHT_FOR_GUARD, we're picking a
 * guard node: consider all guard's bandwidth equally. Otherwise, weight
 * guards proportionally less.
 */
static const node_t *
smartlist_choose_node_by_bandwidth_weights(const smartlist_t *sl,
                                           bandwidth_weight_rule_t rule)
{
  u64_dbl_t *bandwidths=NULL;

  if (compute_weighted_bandwidths(sl, rule, &bandwidths) < 0)
    return NULL;

  scale_array_elements_to_u64(bandwidths, smartlist_len(sl), NULL);

  {
    int idx = choose_array_element_by_weight(bandwidths,
                                             smartlist_len(sl));
    tor_free(bandwidths);
    return idx < 0 ? NULL : smartlist_get(sl, idx);
  }
}

/** Given a list of routers and a weighting rule as in
 * smartlist_choose_node_by_bandwidth_weights, compute weighted bandwidth
 * values for each node and store them in a freshly allocated
 * *<b>bandwidths_out</b> of the same length as <b>sl</b>, and holding results
 * as doubles. Return 0 on success, -1 on failure. */
static int
compute_weighted_bandwidths(const smartlist_t *sl,
                            bandwidth_weight_rule_t rule,
                            u64_dbl_t **bandwidths_out)
{
  int64_t weight_scale;
  double Wg = -1, Wm = -1, We = -1, Wd = -1;
  double Wgb = -1, Wmb = -1, Web = -1, Wdb = -1;
  uint64_t weighted_bw = 0;
  u64_dbl_t *bandwidths;

  /* Can't choose exit and guard at same time */
  tor_assert(rule == NO_WEIGHTING ||
             rule == WEIGHT_FOR_EXIT ||
             rule == WEIGHT_FOR_GUARD ||
             rule == WEIGHT_FOR_MID ||
             rule == WEIGHT_FOR_DIR);

  if (smartlist_len(sl) == 0) {
    log_info(LD_CIRC,
             "Empty routerlist passed in to consensus weight node "
             "selection for rule %s",
             bandwidth_weight_rule_to_string(rule));
    return -1;
  }

  weight_scale = networkstatus_get_weight_scale_param(NULL);

  if (rule == WEIGHT_FOR_GUARD) {
    Wg = networkstatus_get_bw_weight(NULL, "Wgg", -1);
    Wm = networkstatus_get_bw_weight(NULL, "Wgm", -1); /* Bridges */
    We = 0;
    Wd = networkstatus_get_bw_weight(NULL, "Wgd", -1);

    Wgb = networkstatus_get_bw_weight(NULL, "Wgb", -1);
    Wmb = networkstatus_get_bw_weight(NULL, "Wmb", -1);
    Web = networkstatus_get_bw_weight(NULL, "Web", -1);
    Wdb = networkstatus_get_bw_weight(NULL, "Wdb", -1);
  } else if (rule == WEIGHT_FOR_MID) {
    Wg = networkstatus_get_bw_weight(NULL, "Wmg", -1);
    Wm = networkstatus_get_bw_weight(NULL, "Wmm", -1);
    We = networkstatus_get_bw_weight(NULL, "Wme", -1);
    Wd = networkstatus_get_bw_weight(NULL, "Wmd", -1);

    Wgb = networkstatus_get_bw_weight(NULL, "Wgb", -1);
    Wmb = networkstatus_get_bw_weight(NULL, "Wmb", -1);
    Web = networkstatus_get_bw_weight(NULL, "Web", -1);
    Wdb = networkstatus_get_bw_weight(NULL, "Wdb", -1);
  } else if (rule == WEIGHT_FOR_EXIT) {
    // Guards CAN be exits if they have weird exit policies
    // They are d then I guess...
    We = networkstatus_get_bw_weight(NULL, "Wee", -1);
    Wm = networkstatus_get_bw_weight(NULL, "Wem", -1); /* Odd exit policies */
    Wd = networkstatus_get_bw_weight(NULL, "Wed", -1);
    Wg = networkstatus_get_bw_weight(NULL, "Weg", -1); /* Odd exit policies */

    Wgb = networkstatus_get_bw_weight(NULL, "Wgb", -1);
    Wmb = networkstatus_get_bw_weight(NULL, "Wmb", -1);
    Web = networkstatus_get_bw_weight(NULL, "Web", -1);
    Wdb = networkstatus_get_bw_weight(NULL, "Wdb", -1);
  } else if (rule == WEIGHT_FOR_DIR) {
    We = networkstatus_get_bw_weight(NULL, "Wbe", -1);
    Wm = networkstatus_get_bw_weight(NULL, "Wbm", -1);
    Wd = networkstatus_get_bw_weight(NULL, "Wbd", -1);
    Wg = networkstatus_get_bw_weight(NULL, "Wbg", -1);

    Wgb = Wmb = Web = Wdb = weight_scale;
  } else if (rule == NO_WEIGHTING) {
    Wg = Wm = We = Wd = weight_scale;
    Wgb = Wmb = Web = Wdb = weight_scale;
  }

  if (Wg < 0 || Wm < 0 || We < 0 || Wd < 0 || Wgb < 0 || Wmb < 0 || Wdb < 0
      || Web < 0) {
    log_debug(LD_CIRC,
              "Got negative bandwidth weights. Defaulting to old selection"
              " algorithm.");
    return -1; // Use old algorithm.
  }

  Wg /= weight_scale;
  Wm /= weight_scale;
  We /= weight_scale;
  Wd /= weight_scale;

  Wgb /= weight_scale;
  Wmb /= weight_scale;
  Web /= weight_scale;
  Wdb /= weight_scale;

  bandwidths = tor_malloc_zero(sizeof(u64_dbl_t)*smartlist_len(sl));

  // Cycle through smartlist and total the bandwidth.
  SMARTLIST_FOREACH_BEGIN(sl, const node_t *, node) {
    int is_exit = 0, is_guard = 0, is_dir = 0, this_bw = 0;
    double weight = 1;
    is_exit = node->is_exit && ! node->is_bad_exit;
    is_guard = node->is_possible_guard;
    is_dir = node_is_dir(node);
    if (node->rs) {
      if (!node->rs->has_bandwidth) {
        tor_free(bandwidths);
        /* This should never happen, unless all the authorites downgrade
         * to 0.2.0 or rogue routerstatuses get inserted into our consensus. */
        log_warn(LD_BUG,
                 "Consensus is not listing bandwidths. Defaulting back to "
                 "old router selection algorithm.");
        return -1;
      }
      this_bw = kb_to_bytes(node->rs->bandwidth_kb);
    } else if (node->ri) {
      /* bridge or other descriptor not in our consensus */
      this_bw = bridge_get_advertised_bandwidth_bounded(node->ri);
    } else {
      /* We can't use this one. */
      continue;
    }

    if (is_guard && is_exit) {
      weight = (is_dir ? Wdb*Wd : Wd);
    } else if (is_guard) {
      weight = (is_dir ? Wgb*Wg : Wg);
    } else if (is_exit) {
      weight = (is_dir ? Web*We : We);
    } else { // middle
      weight = (is_dir ? Wmb*Wm : Wm);
    }
    /* These should be impossible; but overflows here would be bad, so let's
     * make sure. */
    if (this_bw < 0)
      this_bw = 0;
    if (weight < 0.0)
      weight = 0.0;

    bandwidths[node_sl_idx].dbl = weight*this_bw + 0.5;
  } SMARTLIST_FOREACH_END(node);

  log_debug(LD_CIRC, "Generated weighted bandwidths for rule %s based "
            "on weights "
            "Wg=%f Wm=%f We=%f Wd=%f with total bw "U64_FORMAT,
            bandwidth_weight_rule_to_string(rule),
            Wg, Wm, We, Wd, U64_PRINTF_ARG(weighted_bw));

  *bandwidths_out = bandwidths;

  return 0;
}

/** For all nodes in <b>sl</b>, return the fraction of those nodes, weighted
 * by their weighted bandwidths with rule <b>rule</b>, for which we have
 * descriptors. */
double
frac_nodes_with_descriptors(const smartlist_t *sl,
                            bandwidth_weight_rule_t rule)
{
  u64_dbl_t *bandwidths = NULL;
  double total, present;

  if (smartlist_len(sl) == 0)
    return 0.0;

  if (compute_weighted_bandwidths(sl, rule, &bandwidths) < 0) {
    int n_with_descs = 0;
    SMARTLIST_FOREACH(sl, const node_t *, node, {
      if (node_has_descriptor(node))
        n_with_descs++;
    });
    return ((double)n_with_descs) / (double)smartlist_len(sl);
  }

  total = present = 0.0;
  SMARTLIST_FOREACH_BEGIN(sl, const node_t *, node) {
    const double bw = bandwidths[node_sl_idx].dbl;
    total += bw;
    if (node_has_descriptor(node))
      present += bw;
  } SMARTLIST_FOREACH_END(node);

  tor_free(bandwidths);

  if (total < 1.0)
    return 0;

  return present / total;
}

/** Helper function:
 * choose a random node_t element of smartlist <b>sl</b>, weighted by
 * the advertised bandwidth of each element.
 *
 * If <b>rule</b>==WEIGHT_FOR_EXIT. we're picking an exit node: consider all
 * nodes' bandwidth equally regardless of their Exit status, since there may
 * be some in the list because they exit to obscure ports. If
 * <b>rule</b>==NO_WEIGHTING, we're picking a non-exit node: weight
 * exit-node's bandwidth less depending on the smallness of the fraction of
 * Exit-to-total bandwidth.  If <b>rule</b>==WEIGHT_FOR_GUARD, we're picking a
 * guard node: consider all guard's bandwidth equally. Otherwise, weight
 * guards proportionally less.
 */
static const node_t *
smartlist_choose_node_by_bandwidth(const smartlist_t *sl,
                                   bandwidth_weight_rule_t rule)
{
  unsigned int i;
  u64_dbl_t *bandwidths;
  int is_exit;
  int is_guard;
  int is_fast;
  double total_nonexit_bw = 0, total_exit_bw = 0;
  double total_nonguard_bw = 0, total_guard_bw = 0;
  double exit_weight;
  double guard_weight;
  int n_unknown = 0;
  bitarray_t *fast_bits;
  bitarray_t *exit_bits;
  bitarray_t *guard_bits;

  // This function does not support WEIGHT_FOR_DIR
  // or WEIGHT_FOR_MID
  if (rule == WEIGHT_FOR_DIR || rule == WEIGHT_FOR_MID) {
    rule = NO_WEIGHTING;
  }

  /* Can't choose exit and guard at same time */
  tor_assert(rule == NO_WEIGHTING ||
             rule == WEIGHT_FOR_EXIT ||
             rule == WEIGHT_FOR_GUARD);

  if (smartlist_len(sl) == 0) {
    log_info(LD_CIRC,
             "Empty routerlist passed in to old node selection for rule %s",
             bandwidth_weight_rule_to_string(rule));
    return NULL;
  }

  /* First count the total bandwidth weight, and make a list
   * of each value.  We use UINT64_MAX to indicate "unknown". */
  bandwidths = tor_malloc_zero(sizeof(u64_dbl_t)*smartlist_len(sl));
  fast_bits = bitarray_init_zero(smartlist_len(sl));
  exit_bits = bitarray_init_zero(smartlist_len(sl));
  guard_bits = bitarray_init_zero(smartlist_len(sl));

  /* Iterate over all the routerinfo_t or routerstatus_t, and */
  SMARTLIST_FOREACH_BEGIN(sl, const node_t *, node) {
    /* first, learn what bandwidth we think i has */
    int is_known = 1;
    uint32_t this_bw = 0;
    i = node_sl_idx;

    is_exit = node->is_exit;
    is_guard = node->is_possible_guard;
    if (node->rs) {
      if (node->rs->has_bandwidth) {
        this_bw = kb_to_bytes(node->rs->bandwidth_kb);
      } else { /* guess */
        is_known = 0;
      }
    } else if (node->ri) {
      /* Must be a bridge if we're willing to use it */
      this_bw = bridge_get_advertised_bandwidth_bounded(node->ri);
    }

    if (is_exit)
      bitarray_set(exit_bits, i);
    if (is_guard)
      bitarray_set(guard_bits, i);
    if (node->is_fast)
      bitarray_set(fast_bits, i);

    if (is_known) {
      bandwidths[i].dbl = this_bw;
      if (is_guard)
        total_guard_bw += this_bw;
      else
        total_nonguard_bw += this_bw;
      if (is_exit)
        total_exit_bw += this_bw;
      else
        total_nonexit_bw += this_bw;
    } else {
      ++n_unknown;
      bandwidths[i].dbl = -1.0;
    }
  } SMARTLIST_FOREACH_END(node);

#define EPSILON .1

  /* Now, fill in the unknown values. */
  if (n_unknown) {
    int32_t avg_fast, avg_slow;
    if (total_exit_bw+total_nonexit_bw < EPSILON) {
      /* if there's some bandwidth, there's at least one known router,
       * so no worries about div by 0 here */
      int n_known = smartlist_len(sl)-n_unknown;
      avg_fast = avg_slow = (int32_t)
        ((total_exit_bw+total_nonexit_bw)/((uint64_t) n_known));
    } else {
      avg_fast = 40000;
      avg_slow = 20000;
    }
    for (i=0; i<(unsigned)smartlist_len(sl); ++i) {
      if (bandwidths[i].dbl >= 0.0)
        continue;
      is_fast = bitarray_is_set(fast_bits, i);
      is_exit = bitarray_is_set(exit_bits, i);
      is_guard = bitarray_is_set(guard_bits, i);
      bandwidths[i].dbl = is_fast ? avg_fast : avg_slow;
      if (is_exit)
        total_exit_bw += bandwidths[i].dbl;
      else
        total_nonexit_bw += bandwidths[i].dbl;
      if (is_guard)
        total_guard_bw += bandwidths[i].dbl;
      else
        total_nonguard_bw += bandwidths[i].dbl;
    }
  }

  /* If there's no bandwidth at all, pick at random. */
  if (total_exit_bw+total_nonexit_bw < EPSILON) {
    tor_free(bandwidths);
    tor_free(fast_bits);
    tor_free(exit_bits);
    tor_free(guard_bits);
    return smartlist_choose(sl);
  }

  /* Figure out how to weight exits and guards */
  {
    double all_bw = U64_TO_DBL(total_exit_bw+total_nonexit_bw);
    double exit_bw = U64_TO_DBL(total_exit_bw);
    double guard_bw = U64_TO_DBL(total_guard_bw);
    /*
     * For detailed derivation of this formula, see
     *   http://archives.seul.org/or/dev/Jul-2007/msg00056.html
     */
    if (rule == WEIGHT_FOR_EXIT || total_exit_bw<EPSILON)
      exit_weight = 1.0;
    else
      exit_weight = 1.0 - all_bw/(3.0*exit_bw);

    if (rule == WEIGHT_FOR_GUARD || total_guard_bw<EPSILON)
      guard_weight = 1.0;
    else
      guard_weight = 1.0 - all_bw/(3.0*guard_bw);

    if (exit_weight <= 0.0)
      exit_weight = 0.0;

    if (guard_weight <= 0.0)
      guard_weight = 0.0;

    for (i=0; i < (unsigned)smartlist_len(sl); i++) {
      tor_assert(bandwidths[i].dbl >= 0.0);

      is_exit = bitarray_is_set(exit_bits, i);
      is_guard = bitarray_is_set(guard_bits, i);
      if (is_exit && is_guard)
        bandwidths[i].dbl *= exit_weight * guard_weight;
      else if (is_guard)
        bandwidths[i].dbl *= guard_weight;
      else if (is_exit)
        bandwidths[i].dbl *= exit_weight;
    }
  }

#if 0
  log_debug(LD_CIRC, "Total weighted bw = "U64_FORMAT
            ", exit bw = "U64_FORMAT
            ", nonexit bw = "U64_FORMAT", exit weight = %f "
            "(for exit == %d)"
            ", guard bw = "U64_FORMAT
            ", nonguard bw = "U64_FORMAT", guard weight = %f "
            "(for guard == %d)",
            U64_PRINTF_ARG(total_bw),
            U64_PRINTF_ARG(total_exit_bw), U64_PRINTF_ARG(total_nonexit_bw),
            exit_weight, (int)(rule == WEIGHT_FOR_EXIT),
            U64_PRINTF_ARG(total_guard_bw), U64_PRINTF_ARG(total_nonguard_bw),
            guard_weight, (int)(rule == WEIGHT_FOR_GUARD));
#endif

  scale_array_elements_to_u64(bandwidths, smartlist_len(sl), NULL);

  {
    int idx = choose_array_element_by_weight(bandwidths,
                                             smartlist_len(sl));
    tor_free(bandwidths);
    tor_free(fast_bits);
    tor_free(exit_bits);
    tor_free(guard_bits);
    return idx < 0 ? NULL : smartlist_get(sl, idx);
  }
}

/** Choose a random element of status list <b>sl</b>, weighted by
 * the advertised bandwidth of each node */
const node_t *
node_sl_choose_by_bandwidth(const smartlist_t *sl,
                            bandwidth_weight_rule_t rule)
{ /*XXXX MOVE */
  const node_t *ret;
  if ((ret = smartlist_choose_node_by_bandwidth_weights(sl, rule))) {
    return ret;
  } else {
    return smartlist_choose_node_by_bandwidth(sl, rule);
  }
}

/** Return a random running node from the nodelist. Never
 * pick a node that is in
 * <b>excludedsmartlist</b>, or which matches <b>excludedset</b>,
 * even if they are the only nodes available.
 * If <b>CRN_NEED_UPTIME</b> is set in flags and any router has more than
 * a minimum uptime, return one of those.
 * If <b>CRN_NEED_CAPACITY</b> is set in flags, weight your choice by the
 * advertised capacity of each router.
 * If <b>CRN_ALLOW_INVALID</b> is not set in flags, consider only Valid
 * routers.
 * If <b>CRN_NEED_GUARD</b> is set in flags, consider only Guard routers.
 * If <b>CRN_WEIGHT_AS_EXIT</b> is set in flags, we weight bandwidths as if
 * picking an exit node, otherwise we weight bandwidths for picking a relay
 * node (that is, possibly discounting exit nodes).
 * If <b>CRN_NEED_DESC</b> is set in flags, we only consider nodes that
 * have a routerinfo or microdescriptor -- that is, enough info to be
 * used to build a circuit.
 */
const node_t *
router_choose_random_node(smartlist_t *excludedsmartlist,
                          routerset_t *excludedset,
                          router_crn_flags_t flags)
{ /* XXXX MOVE */
  const int need_uptime = (flags & CRN_NEED_UPTIME) != 0;
  const int need_capacity = (flags & CRN_NEED_CAPACITY) != 0;
  const int need_guard = (flags & CRN_NEED_GUARD) != 0;
  const int allow_invalid = (flags & CRN_ALLOW_INVALID) != 0;
  const int weight_for_exit = (flags & CRN_WEIGHT_AS_EXIT) != 0;
  const int need_desc = (flags & CRN_NEED_DESC) != 0;

  smartlist_t *sl=smartlist_new(),
    *excludednodes=smartlist_new();
  const node_t *choice = NULL;
  const routerinfo_t *r;
  bandwidth_weight_rule_t rule;

  tor_assert(!(weight_for_exit && need_guard));
  rule = weight_for_exit ? WEIGHT_FOR_EXIT :
    (need_guard ? WEIGHT_FOR_GUARD : WEIGHT_FOR_MID);

  /* Exclude relays that allow single hop exit circuits, if the user
   * wants to (such relays might be risky) */
  if (get_options()->ExcludeSingleHopRelays) {
    SMARTLIST_FOREACH(nodelist_get_list(), node_t *, node,
      if (node_allows_single_hop_exits(node)) {
        smartlist_add(excludednodes, node);
      });
  }

  if ((r = routerlist_find_my_routerinfo()))
    routerlist_add_node_and_family(excludednodes, r);

  router_add_running_nodes_to_smartlist(sl, allow_invalid,
                                        need_uptime, need_capacity,
                                        need_guard, need_desc);
  smartlist_subtract(sl,excludednodes);
  if (excludedsmartlist)
    smartlist_subtract(sl,excludedsmartlist);
  if (excludedset)
    routerset_subtract_nodes(sl,excludedset);

  // Always weight by bandwidth
  choice = node_sl_choose_by_bandwidth(sl, rule);

  smartlist_free(sl);
  if (!choice && (need_uptime || need_capacity || need_guard)) {
    /* try once more -- recurse but with fewer restrictions. */
    log_info(LD_CIRC,
             "We couldn't find any live%s%s%s routers; falling back "
             "to list of all routers.",
             need_capacity?", fast":"",
             need_uptime?", stable":"",
             need_guard?", guard":"");
    flags &= ~ (CRN_NEED_UPTIME|CRN_NEED_CAPACITY|CRN_NEED_GUARD);
    choice = router_choose_random_node(
                     excludedsmartlist, excludedset, flags);
  }
  smartlist_free(excludednodes);
  if (!choice) {
    log_warn(LD_CIRC,
             "No available nodes when trying to choose node. Failing.");
  }
  return choice;
}

/** Helper: given an extended nickname in <b>hexdigest</b> try to decode it.
 * Return 0 on success, -1 on failure.  Store the result into the
 * DIGEST_LEN-byte buffer at <b>digest_out</b>, the single character at
 * <b>nickname_qualifier_char_out</b>, and the MAXNICKNAME_LEN+1-byte buffer
 * at <b>nickname_out</b>.
 *
 * The recognized format is:
 *   HexName = Dollar? HexDigest NamePart?
 *   Dollar = '?'
 *   HexDigest = HexChar*20
 *   HexChar = 'a'..'f' | 'A'..'F' | '0'..'9'
 *   NamePart = QualChar Name
 *   QualChar = '=' | '~'
 *   Name = NameChar*(1..MAX_NICKNAME_LEN)
 *   NameChar = Any ASCII alphanumeric character
 */
int
hex_digest_nickname_decode(const char *hexdigest,
                           char *digest_out,
                           char *nickname_qualifier_char_out,
                           char *nickname_out)
{
  size_t len;

  tor_assert(hexdigest);
  if (hexdigest[0] == '$')
    ++hexdigest;

  len = strlen(hexdigest);
  if (len < HEX_DIGEST_LEN) {
    return -1;
  } else if (len > HEX_DIGEST_LEN && (hexdigest[HEX_DIGEST_LEN] == '=' ||
                                    hexdigest[HEX_DIGEST_LEN] == '~') &&
           len <= HEX_DIGEST_LEN+1+MAX_NICKNAME_LEN) {
    *nickname_qualifier_char_out = hexdigest[HEX_DIGEST_LEN];
    strlcpy(nickname_out, hexdigest+HEX_DIGEST_LEN+1 , MAX_NICKNAME_LEN+1);
  } else if (len == HEX_DIGEST_LEN) {
    ;
  } else {
    return -1;
  }

  if (base16_decode(digest_out, DIGEST_LEN, hexdigest, HEX_DIGEST_LEN)<0)
    return -1;
  return 0;
}

/** Helper: Return true iff the <b>identity_digest</b> and <b>nickname</b>
 * combination of a router, encoded in hexadecimal, matches <b>hexdigest</b>
 * (which is optionally prefixed with a single dollar sign).  Return false if
 * <b>hexdigest</b> is malformed, or it doesn't match.  */
int
hex_digest_nickname_matches(const char *hexdigest, const char *identity_digest,
                            const char *nickname, int is_named)
{
  char digest[DIGEST_LEN];
  char nn_char='\0';
  char nn_buf[MAX_NICKNAME_LEN+1];

  if (hex_digest_nickname_decode(hexdigest, digest, &nn_char, nn_buf) == -1)
    return 0;

  if (nn_char == '=' || nn_char == '~') {
    if (!nickname)
      return 0;
    if (strcasecmp(nn_buf, nickname))
      return 0;
    if (nn_char == '=' && !is_named)
      return 0;
  }

  return tor_memeq(digest, identity_digest, DIGEST_LEN);
}

/** Return true iff <b>router</b> is listed as named in the current
 * consensus. */
int
router_is_named(const routerinfo_t *router)
{
  const char *digest =
    networkstatus_get_router_digest_by_nickname(router->nickname);

  return (digest &&
          tor_memeq(digest, router->cache_info.identity_digest, DIGEST_LEN));
}

/** Return true iff <b>digest</b> is the digest of the identity key of a
 * trusted directory matching at least one bit of <b>type</b>.  If <b>type</b>
 * is zero, any authority is okay. */
int
router_digest_is_trusted_dir_type(const char *digest, dirinfo_type_t type)
{
  if (!trusted_dir_servers)
    return 0;
  if (authdir_mode(get_options()) && router_digest_is_me(digest))
    return 1;
  SMARTLIST_FOREACH(trusted_dir_servers, dir_server_t *, ent,
    if (tor_memeq(digest, ent->digest, DIGEST_LEN)) {
      return (!type) || ((type & ent->type) != 0);
    });
  return 0;
}

/** Return true iff <b>addr</b> is the address of one of our trusted
 * directory authorities. */
int
router_addr_is_trusted_dir(uint32_t addr)
{
  if (!trusted_dir_servers)
    return 0;
  SMARTLIST_FOREACH(trusted_dir_servers, dir_server_t *, ent,
    if (ent->addr == addr)
      return 1;
    );
  return 0;
}

/** If hexdigest is correctly formed, base16_decode it into
 * digest, which must have DIGEST_LEN space in it.
 * Return 0 on success, -1 on failure.
 */
int
hexdigest_to_digest(const char *hexdigest, char *digest)
{
  if (hexdigest[0]=='$')
    ++hexdigest;
  if (strlen(hexdigest) < HEX_DIGEST_LEN ||
      base16_decode(digest,DIGEST_LEN,hexdigest,HEX_DIGEST_LEN) < 0)
    return -1;
  return 0;
}

/** As router_get_by_id_digest,but return a pointer that you're allowed to
 * modify */
routerinfo_t *
router_get_mutable_by_digest(const char *digest)
{
  tor_assert(digest);

  if (!routerlist) return NULL;

  // routerlist_assert_ok(routerlist);

  return rimap_get(routerlist->identity_map, digest);
}

/** Return the router in our routerlist whose 20-byte key digest
 * is <b>digest</b>.  Return NULL if no such router is known. */
const routerinfo_t *
router_get_by_id_digest(const char *digest)
{
  return router_get_mutable_by_digest(digest);
}

/** Return the router in our routerlist whose 20-byte descriptor
 * is <b>digest</b>.  Return NULL if no such router is known. */
signed_descriptor_t *
router_get_by_descriptor_digest(const char *digest)
{
  tor_assert(digest);

  if (!routerlist) return NULL;

  return sdmap_get(routerlist->desc_digest_map, digest);
}

/** Return the signed descriptor for the router in our routerlist whose
 * 20-byte extra-info digest is <b>digest</b>.  Return NULL if no such router
 * is known. */
signed_descriptor_t *
router_get_by_extrainfo_digest(const char *digest)
{
  tor_assert(digest);

  if (!routerlist) return NULL;

  return sdmap_get(routerlist->desc_by_eid_map, digest);
}

/** Return the signed descriptor for the extrainfo_t in our routerlist whose
 * extra-info-digest is <b>digest</b>. Return NULL if no such extra-info
 * document is known. */
signed_descriptor_t *
extrainfo_get_by_descriptor_digest(const char *digest)
{
  extrainfo_t *ei;
  tor_assert(digest);
  if (!routerlist) return NULL;
  ei = eimap_get(routerlist->extra_info_map, digest);
  return ei ? &ei->cache_info : NULL;
}

/** Return a pointer to the signed textual representation of a descriptor.
 * The returned string is not guaranteed to be NUL-terminated: the string's
 * length will be in desc-\>signed_descriptor_len.
 *
 * If <b>with_annotations</b> is set, the returned string will include
 * the annotations
 * (if any) preceding the descriptor.  This will increase the length of the
 * string by desc-\>annotations_len.
 *
 * The caller must not free the string returned.
 */
static const char *
signed_descriptor_get_body_impl(const signed_descriptor_t *desc,
                                int with_annotations)
{
  const char *r = NULL;
  size_t len = desc->signed_descriptor_len;
  off_t offset = desc->saved_offset;
  if (with_annotations)
    len += desc->annotations_len;
  else
    offset += desc->annotations_len;

  tor_assert(len > 32);
  if (desc->saved_location == SAVED_IN_CACHE && routerlist) {
    desc_store_t *store = desc_get_store(router_get_routerlist(), desc);
    if (store && store->mmap) {
      tor_assert(desc->saved_offset + len <= store->mmap->size);
      r = store->mmap->data + offset;
    } else if (store) {
      log_err(LD_DIR, "We couldn't read a descriptor that is supposedly "
              "mmaped in our cache.  Is another process running in our data "
              "directory?  Exiting.");
      exit(1);
    }
  }
  if (!r) /* no mmap, or not in cache. */
    r = desc->signed_descriptor_body +
      (with_annotations ? 0 : desc->annotations_len);

  tor_assert(r);
  if (!with_annotations) {
    if (fast_memcmp("router ", r, 7) && fast_memcmp("extra-info ", r, 11)) {
      char *cp = tor_strndup(r, 64);
      log_err(LD_DIR, "descriptor at %p begins with unexpected string %s.  "
              "Is another process running in our data directory?  Exiting.",
              desc, escaped(cp));
      exit(1);
    }
  }

  return r;
}

/** Return a pointer to the signed textual representation of a descriptor.
 * The returned string is not guaranteed to be NUL-terminated: the string's
 * length will be in desc-\>signed_descriptor_len.
 *
 * The caller must not free the string returned.
 */
const char *
signed_descriptor_get_body(const signed_descriptor_t *desc)
{
  return signed_descriptor_get_body_impl(desc, 0);
}

/** As signed_descriptor_get_body(), but points to the beginning of the
 * annotations section rather than the beginning of the descriptor. */
const char *
signed_descriptor_get_annotations(const signed_descriptor_t *desc)
{
  return signed_descriptor_get_body_impl(desc, 1);
}

/** Return the current list of all known routers. */
routerlist_t *
router_get_routerlist(void)
{
  if (PREDICT_UNLIKELY(!routerlist)) {
    routerlist = tor_malloc_zero(sizeof(routerlist_t));
    routerlist->routers = smartlist_new();
    routerlist->old_routers = smartlist_new();
    routerlist->identity_map = rimap_new();
    routerlist->desc_digest_map = sdmap_new();
    routerlist->desc_by_eid_map = sdmap_new();
    routerlist->extra_info_map = eimap_new();

    routerlist->desc_store.fname_base = "cached-descriptors";
    routerlist->extrainfo_store.fname_base = "cached-extrainfo";

    routerlist->desc_store.type = ROUTER_STORE;
    routerlist->extrainfo_store.type = EXTRAINFO_STORE;

    routerlist->desc_store.description = "router descriptors";
    routerlist->extrainfo_store.description = "extra-info documents";
  }
  return routerlist;
}

/** Free all storage held by <b>router</b>. */
void
routerinfo_free(routerinfo_t *router)
{
  if (!router)
    return;

  tor_free(router->cache_info.signed_descriptor_body);
  tor_free(router->nickname);
  tor_free(router->platform);
  tor_free(router->contact_info);
  if (router->onion_pkey)
    crypto_pk_free(router->onion_pkey);
  tor_free(router->onion_curve25519_pkey);
  if (router->identity_pkey)
    crypto_pk_free(router->identity_pkey);
  if (router->declared_family) {
    SMARTLIST_FOREACH(router->declared_family, char *, s, tor_free(s));
    smartlist_free(router->declared_family);
  }
  addr_policy_list_free(router->exit_policy);
  short_policy_free(router->ipv6_exit_policy);

  memset(router, 77, sizeof(routerinfo_t));

  tor_free(router);
}

/** Release all storage held by <b>extrainfo</b> */
void
extrainfo_free(extrainfo_t *extrainfo)
{
  if (!extrainfo)
    return;
  tor_free(extrainfo->cache_info.signed_descriptor_body);
  tor_free(extrainfo->pending_sig);

  memset(extrainfo, 88, sizeof(extrainfo_t)); /* debug bad memory usage */
  tor_free(extrainfo);
}

/** Release storage held by <b>sd</b>. */
static void
signed_descriptor_free(signed_descriptor_t *sd)
{
  if (!sd)
    return;

  tor_free(sd->signed_descriptor_body);

  memset(sd, 99, sizeof(signed_descriptor_t)); /* Debug bad mem usage */
  tor_free(sd);
}

/** Extract a signed_descriptor_t from a general routerinfo, and free the
 * routerinfo.
 */
static signed_descriptor_t *
signed_descriptor_from_routerinfo(routerinfo_t *ri)
{
  signed_descriptor_t *sd;
  tor_assert(ri->purpose == ROUTER_PURPOSE_GENERAL);
  sd = tor_malloc_zero(sizeof(signed_descriptor_t));
  memcpy(sd, &(ri->cache_info), sizeof(signed_descriptor_t));
  sd->routerlist_index = -1;
  ri->cache_info.signed_descriptor_body = NULL;
  routerinfo_free(ri);
  return sd;
}

/** Helper: free the storage held by the extrainfo_t in <b>e</b>. */
static void
extrainfo_free_(void *e)
{
  extrainfo_free(e);
}

/** Free all storage held by a routerlist <b>rl</b>. */
void
routerlist_free(routerlist_t *rl)
{
  if (!rl)
    return;
  rimap_free(rl->identity_map, NULL);
  sdmap_free(rl->desc_digest_map, NULL);
  sdmap_free(rl->desc_by_eid_map, NULL);
  eimap_free(rl->extra_info_map, extrainfo_free_);
  SMARTLIST_FOREACH(rl->routers, routerinfo_t *, r,
                    routerinfo_free(r));
  SMARTLIST_FOREACH(rl->old_routers, signed_descriptor_t *, sd,
                    signed_descriptor_free(sd));
  smartlist_free(rl->routers);
  smartlist_free(rl->old_routers);
  if (rl->desc_store.mmap) {
    int res = tor_munmap_file(routerlist->desc_store.mmap);
    if (res != 0) {
      log_warn(LD_FS, "Failed to munmap routerlist->desc_store.mmap");
    }
  }
  if (rl->extrainfo_store.mmap) {
    int res = tor_munmap_file(routerlist->extrainfo_store.mmap);
    if (res != 0) {
      log_warn(LD_FS, "Failed to munmap routerlist->extrainfo_store.mmap");
    }
  }
  tor_free(rl);

  router_dir_info_changed();
}

/** Log information about how much memory is being used for routerlist,
 * at log level <b>severity</b>. */
void
dump_routerlist_mem_usage(int severity)
{
  uint64_t livedescs = 0;
  uint64_t olddescs = 0;
  if (!routerlist)
    return;
  SMARTLIST_FOREACH(routerlist->routers, routerinfo_t *, r,
                    livedescs += r->cache_info.signed_descriptor_len);
  SMARTLIST_FOREACH(routerlist->old_routers, signed_descriptor_t *, sd,
                    olddescs += sd->signed_descriptor_len);

  tor_log(severity, LD_DIR,
      "In %d live descriptors: "U64_FORMAT" bytes.  "
      "In %d old descriptors: "U64_FORMAT" bytes.",
      smartlist_len(routerlist->routers), U64_PRINTF_ARG(livedescs),
      smartlist_len(routerlist->old_routers), U64_PRINTF_ARG(olddescs));
}

/** Debugging helper: If <b>idx</b> is nonnegative, assert that <b>ri</b> is
 * in <b>sl</b> at position <b>idx</b>. Otherwise, search <b>sl</b> for
 * <b>ri</b>.  Return the index of <b>ri</b> in <b>sl</b>, or -1 if <b>ri</b>
 * is not in <b>sl</b>. */
static INLINE int
routerlist_find_elt_(smartlist_t *sl, void *ri, int idx)
{
  if (idx < 0) {
    idx = -1;
    SMARTLIST_FOREACH(sl, routerinfo_t *, r,
                      if (r == ri) {
                        idx = r_sl_idx;
                        break;
                      });
  } else {
    tor_assert(idx < smartlist_len(sl));
    tor_assert(smartlist_get(sl, idx) == ri);
  };
  return idx;
}

/** Insert an item <b>ri</b> into the routerlist <b>rl</b>, updating indices
 * as needed.  There must be no previous member of <b>rl</b> with the same
 * identity digest as <b>ri</b>: If there is, call routerlist_replace
 * instead.
 */
static void
routerlist_insert(routerlist_t *rl, routerinfo_t *ri)
{
  routerinfo_t *ri_old;
  signed_descriptor_t *sd_old;
  {
    const routerinfo_t *ri_generated = router_get_my_routerinfo();
    tor_assert(ri_generated != ri);
  }
  tor_assert(ri->cache_info.routerlist_index == -1);

  ri_old = rimap_set(rl->identity_map, ri->cache_info.identity_digest, ri);
  tor_assert(!ri_old);

  sd_old = sdmap_set(rl->desc_digest_map,
                     ri->cache_info.signed_descriptor_digest,
                     &(ri->cache_info));
  if (sd_old) {
    int idx = sd_old->routerlist_index;
    sd_old->routerlist_index = -1;
    smartlist_del(rl->old_routers, idx);
    if (idx < smartlist_len(rl->old_routers)) {
       signed_descriptor_t *d = smartlist_get(rl->old_routers, idx);
       d->routerlist_index = idx;
    }
    rl->desc_store.bytes_dropped += sd_old->signed_descriptor_len;
    sdmap_remove(rl->desc_by_eid_map, sd_old->extra_info_digest);
    signed_descriptor_free(sd_old);
  }

  if (!tor_digest_is_zero(ri->cache_info.extra_info_digest))
    sdmap_set(rl->desc_by_eid_map, ri->cache_info.extra_info_digest,
              &ri->cache_info);
  smartlist_add(rl->routers, ri);
  ri->cache_info.routerlist_index = smartlist_len(rl->routers) - 1;
  nodelist_set_routerinfo(ri, NULL);
  router_dir_info_changed();
#ifdef DEBUG_ROUTERLIST
  routerlist_assert_ok(rl);
#endif
}

/** Adds the extrainfo_t <b>ei</b> to the routerlist <b>rl</b>, if there is a
 * corresponding router in rl-\>routers or rl-\>old_routers.  Return true iff
 * we actually inserted <b>ei</b>.  Free <b>ei</b> if it isn't inserted. */
static int
extrainfo_insert(routerlist_t *rl, extrainfo_t *ei)
{
  int r = 0;
  routerinfo_t *ri = rimap_get(rl->identity_map,
                               ei->cache_info.identity_digest);
  signed_descriptor_t *sd =
    sdmap_get(rl->desc_by_eid_map, ei->cache_info.signed_descriptor_digest);
  extrainfo_t *ei_tmp;

  {
    extrainfo_t *ei_generated = router_get_my_extrainfo();
    tor_assert(ei_generated != ei);
  }

  if (!ri) {
    /* This router is unknown; we can't even verify the signature. Give up.*/
    goto done;
  }
  if (routerinfo_incompatible_with_extrainfo(ri, ei, sd, NULL)) {
    goto done;
  }

  /* Okay, if we make it here, we definitely have a router corresponding to
   * this extrainfo. */

  ei_tmp = eimap_set(rl->extra_info_map,
                     ei->cache_info.signed_descriptor_digest,
                     ei);
  r = 1;
  if (ei_tmp) {
    rl->extrainfo_store.bytes_dropped +=
      ei_tmp->cache_info.signed_descriptor_len;
    extrainfo_free(ei_tmp);
  }

 done:
  if (r == 0)
    extrainfo_free(ei);

#ifdef DEBUG_ROUTERLIST
  routerlist_assert_ok(rl);
#endif
  return r;
}

#define should_cache_old_descriptors() \
  directory_caches_dir_info(get_options())

/** If we're a directory cache and routerlist <b>rl</b> doesn't have
 * a copy of router <b>ri</b> yet, add it to the list of old (not
 * recommended but still served) descriptors. Else free it. */
static void
routerlist_insert_old(routerlist_t *rl, routerinfo_t *ri)
{
  {
    const routerinfo_t *ri_generated = router_get_my_routerinfo();
    tor_assert(ri_generated != ri);
  }
  tor_assert(ri->cache_info.routerlist_index == -1);

  if (should_cache_old_descriptors() &&
      ri->purpose == ROUTER_PURPOSE_GENERAL &&
      !sdmap_get(rl->desc_digest_map,
                 ri->cache_info.signed_descriptor_digest)) {
    signed_descriptor_t *sd = signed_descriptor_from_routerinfo(ri);
    sdmap_set(rl->desc_digest_map, sd->signed_descriptor_digest, sd);
    smartlist_add(rl->old_routers, sd);
    sd->routerlist_index = smartlist_len(rl->old_routers)-1;
    if (!tor_digest_is_zero(sd->extra_info_digest))
      sdmap_set(rl->desc_by_eid_map, sd->extra_info_digest, sd);
  } else {
    routerinfo_free(ri);
  }
#ifdef DEBUG_ROUTERLIST
  routerlist_assert_ok(rl);
#endif
}

/** Remove an item <b>ri</b> from the routerlist <b>rl</b>, updating indices
 * as needed. If <b>idx</b> is nonnegative and smartlist_get(rl-&gt;routers,
 * idx) == ri, we don't need to do a linear search over the list to decide
 * which to remove.  We fill the gap in rl-&gt;routers with a later element in
 * the list, if any exists. <b>ri</b> is freed.
 *
 * If <b>make_old</b> is true, instead of deleting the router, we try adding
 * it to rl-&gt;old_routers. */
void
routerlist_remove(routerlist_t *rl, routerinfo_t *ri, int make_old, time_t now)
{
  routerinfo_t *ri_tmp;
  extrainfo_t *ei_tmp;
  int idx = ri->cache_info.routerlist_index;
  tor_assert(0 <= idx && idx < smartlist_len(rl->routers));
  tor_assert(smartlist_get(rl->routers, idx) == ri);

  nodelist_remove_routerinfo(ri);

  /* make sure the rephist module knows that it's not running */
  rep_hist_note_router_unreachable(ri->cache_info.identity_digest, now);

  ri->cache_info.routerlist_index = -1;
  smartlist_del(rl->routers, idx);
  if (idx < smartlist_len(rl->routers)) {
    routerinfo_t *r = smartlist_get(rl->routers, idx);
    r->cache_info.routerlist_index = idx;
  }

  ri_tmp = rimap_remove(rl->identity_map, ri->cache_info.identity_digest);
  router_dir_info_changed();
  tor_assert(ri_tmp == ri);

  if (make_old && should_cache_old_descriptors() &&
      ri->purpose == ROUTER_PURPOSE_GENERAL) {
    signed_descriptor_t *sd;
    sd = signed_descriptor_from_routerinfo(ri);
    smartlist_add(rl->old_routers, sd);
    sd->routerlist_index = smartlist_len(rl->old_routers)-1;
    sdmap_set(rl->desc_digest_map, sd->signed_descriptor_digest, sd);
    if (!tor_digest_is_zero(sd->extra_info_digest))
      sdmap_set(rl->desc_by_eid_map, sd->extra_info_digest, sd);
  } else {
    signed_descriptor_t *sd_tmp;
    sd_tmp = sdmap_remove(rl->desc_digest_map,
                          ri->cache_info.signed_descriptor_digest);
    tor_assert(sd_tmp == &(ri->cache_info));
    rl->desc_store.bytes_dropped += ri->cache_info.signed_descriptor_len;
    ei_tmp = eimap_remove(rl->extra_info_map,
                          ri->cache_info.extra_info_digest);
    if (ei_tmp) {
      rl->extrainfo_store.bytes_dropped +=
        ei_tmp->cache_info.signed_descriptor_len;
      extrainfo_free(ei_tmp);
    }
    if (!tor_digest_is_zero(ri->cache_info.extra_info_digest))
      sdmap_remove(rl->desc_by_eid_map, ri->cache_info.extra_info_digest);
    routerinfo_free(ri);
  }
#ifdef DEBUG_ROUTERLIST
  routerlist_assert_ok(rl);
#endif
}

/** Remove a signed_descriptor_t <b>sd</b> from <b>rl</b>-\>old_routers, and
 * adjust <b>rl</b> as appropriate.  <b>idx</b> is -1, or the index of
 * <b>sd</b>. */
static void
routerlist_remove_old(routerlist_t *rl, signed_descriptor_t *sd, int idx)
{
  signed_descriptor_t *sd_tmp;
  extrainfo_t *ei_tmp;
  desc_store_t *store;
  if (idx == -1) {
    idx = sd->routerlist_index;
  }
  tor_assert(0 <= idx && idx < smartlist_len(rl->old_routers));
  /* XXXX edmanm's bridge relay triggered the following assert while
   * running 0.2.0.12-alpha.  If anybody triggers this again, see if we
   * can get a backtrace. */
  tor_assert(smartlist_get(rl->old_routers, idx) == sd);
  tor_assert(idx == sd->routerlist_index);

  sd->routerlist_index = -1;
  smartlist_del(rl->old_routers, idx);
  if (idx < smartlist_len(rl->old_routers)) {
    signed_descriptor_t *d = smartlist_get(rl->old_routers, idx);
    d->routerlist_index = idx;
  }
  sd_tmp = sdmap_remove(rl->desc_digest_map,
                        sd->signed_descriptor_digest);
  tor_assert(sd_tmp == sd);
  store = desc_get_store(rl, sd);
  if (store)
    store->bytes_dropped += sd->signed_descriptor_len;

  ei_tmp = eimap_remove(rl->extra_info_map,
                        sd->extra_info_digest);
  if (ei_tmp) {
    rl->extrainfo_store.bytes_dropped +=
      ei_tmp->cache_info.signed_descriptor_len;
    extrainfo_free(ei_tmp);
  }
  if (!tor_digest_is_zero(sd->extra_info_digest))
    sdmap_remove(rl->desc_by_eid_map, sd->extra_info_digest);

  signed_descriptor_free(sd);
#ifdef DEBUG_ROUTERLIST
  routerlist_assert_ok(rl);
#endif
}

/** Remove <b>ri_old</b> from the routerlist <b>rl</b>, and replace it with
 * <b>ri_new</b>, updating all index info.  If <b>idx</b> is nonnegative and
 * smartlist_get(rl-&gt;routers, idx) == ri, we don't need to do a linear
 * search over the list to decide which to remove.  We put ri_new in the same
 * index as ri_old, if possible.  ri is freed as appropriate.
 *
 * If should_cache_descriptors() is true, instead of deleting the router,
 * we add it to rl-&gt;old_routers. */
static void
routerlist_replace(routerlist_t *rl, routerinfo_t *ri_old,
                   routerinfo_t *ri_new)
{
  int idx;
  int same_descriptors;

  routerinfo_t *ri_tmp;
  extrainfo_t *ei_tmp;
  {
    const routerinfo_t *ri_generated = router_get_my_routerinfo();
    tor_assert(ri_generated != ri_new);
  }
  tor_assert(ri_old != ri_new);
  tor_assert(ri_new->cache_info.routerlist_index == -1);

  idx = ri_old->cache_info.routerlist_index;
  tor_assert(0 <= idx && idx < smartlist_len(rl->routers));
  tor_assert(smartlist_get(rl->routers, idx) == ri_old);

  {
    routerinfo_t *ri_old_tmp=NULL;
    nodelist_set_routerinfo(ri_new, &ri_old_tmp);
    tor_assert(ri_old == ri_old_tmp);
  }

  router_dir_info_changed();
  if (idx >= 0) {
    smartlist_set(rl->routers, idx, ri_new);
    ri_old->cache_info.routerlist_index = -1;
    ri_new->cache_info.routerlist_index = idx;
    /* Check that ri_old is not in rl->routers anymore: */
    tor_assert( routerlist_find_elt_(rl->routers, ri_old, -1) == -1 );
  } else {
    log_warn(LD_BUG, "Appending entry from routerlist_replace.");
    routerlist_insert(rl, ri_new);
    return;
  }
  if (tor_memneq(ri_old->cache_info.identity_digest,
             ri_new->cache_info.identity_digest, DIGEST_LEN)) {
    /* digests don't match; digestmap_set won't replace */
    rimap_remove(rl->identity_map, ri_old->cache_info.identity_digest);
  }
  ri_tmp = rimap_set(rl->identity_map,
                     ri_new->cache_info.identity_digest, ri_new);
  tor_assert(!ri_tmp || ri_tmp == ri_old);
  sdmap_set(rl->desc_digest_map,
            ri_new->cache_info.signed_descriptor_digest,
            &(ri_new->cache_info));

  if (!tor_digest_is_zero(ri_new->cache_info.extra_info_digest)) {
    sdmap_set(rl->desc_by_eid_map, ri_new->cache_info.extra_info_digest,
              &ri_new->cache_info);
  }

  same_descriptors = tor_memeq(ri_old->cache_info.signed_descriptor_digest,
                              ri_new->cache_info.signed_descriptor_digest,
                              DIGEST_LEN);

  if (should_cache_old_descriptors() &&
      ri_old->purpose == ROUTER_PURPOSE_GENERAL &&
      !same_descriptors) {
    /* ri_old is going to become a signed_descriptor_t and go into
     * old_routers */
    signed_descriptor_t *sd = signed_descriptor_from_routerinfo(ri_old);
    smartlist_add(rl->old_routers, sd);
    sd->routerlist_index = smartlist_len(rl->old_routers)-1;
    sdmap_set(rl->desc_digest_map, sd->signed_descriptor_digest, sd);
    if (!tor_digest_is_zero(sd->extra_info_digest))
      sdmap_set(rl->desc_by_eid_map, sd->extra_info_digest, sd);
  } else {
    /* We're dropping ri_old. */
    if (!same_descriptors) {
      /* digests don't match; The sdmap_set above didn't replace */
      sdmap_remove(rl->desc_digest_map,
                   ri_old->cache_info.signed_descriptor_digest);

      if (tor_memneq(ri_old->cache_info.extra_info_digest,
                 ri_new->cache_info.extra_info_digest, DIGEST_LEN)) {
        ei_tmp = eimap_remove(rl->extra_info_map,
                              ri_old->cache_info.extra_info_digest);
        if (ei_tmp) {
          rl->extrainfo_store.bytes_dropped +=
            ei_tmp->cache_info.signed_descriptor_len;
          extrainfo_free(ei_tmp);
        }
      }

      if (!tor_digest_is_zero(ri_old->cache_info.extra_info_digest)) {
        sdmap_remove(rl->desc_by_eid_map,
                     ri_old->cache_info.extra_info_digest);
      }
    }
    rl->desc_store.bytes_dropped += ri_old->cache_info.signed_descriptor_len;
    routerinfo_free(ri_old);
  }
#ifdef DEBUG_ROUTERLIST
  routerlist_assert_ok(rl);
#endif
}

/** Extract the descriptor <b>sd</b> from old_routerlist, and re-parse
 * it as a fresh routerinfo_t. */
static routerinfo_t *
routerlist_reparse_old(routerlist_t *rl, signed_descriptor_t *sd)
{
  routerinfo_t *ri;
  const char *body;

  body = signed_descriptor_get_annotations(sd);

  ri = router_parse_entry_from_string(body,
                         body+sd->signed_descriptor_len+sd->annotations_len,
                         0, 1, NULL);
  if (!ri)
    return NULL;
  memcpy(&ri->cache_info, sd, sizeof(signed_descriptor_t));
  sd->signed_descriptor_body = NULL; /* Steal reference. */
  ri->cache_info.routerlist_index = -1;

  routerlist_remove_old(rl, sd, -1);

  return ri;
}

/** Free all memory held by the routerlist module. */
void
routerlist_free_all(void)
{
  routerlist_free(routerlist);
  routerlist = NULL;
  if (warned_nicknames) {
    SMARTLIST_FOREACH(warned_nicknames, char *, cp, tor_free(cp));
    smartlist_free(warned_nicknames);
    warned_nicknames = NULL;
  }
  clear_dir_servers();
  smartlist_free(trusted_dir_servers);
  smartlist_free(fallback_dir_servers);
  trusted_dir_servers = fallback_dir_servers = NULL;
  if (trusted_dir_certs) {
    digestmap_free(trusted_dir_certs, cert_list_free_);
    trusted_dir_certs = NULL;
  }
}

/** Forget that we have issued any router-related warnings, so that we'll
 * warn again if we see the same errors. */
void
routerlist_reset_warnings(void)
{
  if (!warned_nicknames)
    warned_nicknames = smartlist_new();
  SMARTLIST_FOREACH(warned_nicknames, char *, cp, tor_free(cp));
  smartlist_clear(warned_nicknames); /* now the list is empty. */

  networkstatus_reset_warnings();
}

/** Add <b>router</b> to the routerlist, if we don't already have it.  Replace
 * older entries (if any) with the same key.  Note: Callers should not hold
 * their pointers to <b>router</b> if this function fails; <b>router</b>
 * will either be inserted into the routerlist or freed. Similarly, even
 * if this call succeeds, they should not hold their pointers to
 * <b>router</b> after subsequent calls with other routerinfo's -- they
 * might cause the original routerinfo to get freed.
 *
 * Returns the status for the operation. Might set *<b>msg</b> if it wants
 * the poster of the router to know something.
 *
 * If <b>from_cache</b>, this descriptor came from our disk cache. If
 * <b>from_fetch</b>, we received it in response to a request we made.
 * (If both are false, that means it was uploaded to us as an auth dir
 * server or via the controller.)
 *
 * This function should be called *after*
 * routers_update_status_from_consensus_networkstatus; subsequently, you
 * should call router_rebuild_store and routerlist_descriptors_added.
 */
was_router_added_t
router_add_to_routerlist(routerinfo_t *router, const char **msg,
                         int from_cache, int from_fetch)
{
  const char *id_digest;
  const or_options_t *options = get_options();
  int authdir = authdir_mode_handles_descs(options, router->purpose);
  int authdir_believes_valid = 0;
  routerinfo_t *old_router;
  networkstatus_t *consensus =
    networkstatus_get_latest_consensus_by_flavor(FLAV_NS);
  int in_consensus = 0;

  tor_assert(msg);

  if (!routerlist)
    router_get_routerlist();

  id_digest = router->cache_info.identity_digest;

  old_router = router_get_mutable_by_digest(id_digest);

  /* Make sure that we haven't already got this exact descriptor. */
  if (sdmap_get(routerlist->desc_digest_map,
                router->cache_info.signed_descriptor_digest)) {
    /* If we have this descriptor already and the new descriptor is a bridge
     * descriptor, replace it. If we had a bridge descriptor before and the
     * new one is not a bridge descriptor, don't replace it. */

    /* Only members of routerlist->identity_map can be bridges; we don't
     * put bridges in old_routers. */
    const int was_bridge = old_router &&
      old_router->purpose == ROUTER_PURPOSE_BRIDGE;

    if (routerinfo_is_a_configured_bridge(router) &&
        router->purpose == ROUTER_PURPOSE_BRIDGE &&
        !was_bridge) {
      log_info(LD_DIR, "Replacing non-bridge descriptor with bridge "
               "descriptor for router %s",
               router_describe(router));
    } else {
      log_info(LD_DIR,
               "Dropping descriptor that we already have for router %s",
               router_describe(router));
      *msg = "Router descriptor was not new.";
      routerinfo_free(router);
      return ROUTER_WAS_NOT_NEW;
    }
  }

  if (authdir) {
    if (authdir_wants_to_reject_router(router, msg,
                                       !from_cache && !from_fetch,
                                       &authdir_believes_valid)) {
      tor_assert(*msg);
      routerinfo_free(router);
      return ROUTER_AUTHDIR_REJECTS;
    }
  } else if (from_fetch) {
    /* Only check the descriptor digest against the network statuses when
     * we are receiving in response to a fetch. */

    if (!signed_desc_digest_is_recognized(&router->cache_info) &&
        !routerinfo_is_a_configured_bridge(router)) {
      /* We asked for it, so some networkstatus must have listed it when we
       * did.  Save it if we're a cache in case somebody else asks for it. */
      log_info(LD_DIR,
               "Received a no-longer-recognized descriptor for router %s",
               router_describe(router));
      *msg = "Router descriptor is not referenced by any network-status.";

      /* Only journal this desc if we'll be serving it. */
      if (!from_cache && should_cache_old_descriptors())
        signed_desc_append_to_journal(&router->cache_info,
                                      &routerlist->desc_store);
      routerlist_insert_old(routerlist, router);
      return ROUTER_NOT_IN_CONSENSUS_OR_NETWORKSTATUS;
    }
  }

  /* We no longer need a router with this descriptor digest. */
  if (consensus) {
    routerstatus_t *rs = networkstatus_vote_find_mutable_entry(
                                                     consensus, id_digest);
    if (rs && tor_memeq(rs->descriptor_digest,
                      router->cache_info.signed_descriptor_digest,
                      DIGEST_LEN)) {
      in_consensus = 1;
    }
  }

  if (router->purpose == ROUTER_PURPOSE_GENERAL &&
      consensus && !in_consensus && !authdir) {
    /* If it's a general router not listed in the consensus, then don't
     * consider replacing the latest router with it. */
    if (!from_cache && should_cache_old_descriptors())
      signed_desc_append_to_journal(&router->cache_info,
                                    &routerlist->desc_store);
    routerlist_insert_old(routerlist, router);
    *msg = "Skipping router descriptor: not in consensus.";
    return ROUTER_NOT_IN_CONSENSUS;
  }

  /* If we're reading a bridge descriptor from our cache, and we don't
   * recognize it as one of our currently configured bridges, drop the
   * descriptor. Otherwise we could end up using it as one of our entry
   * guards even if it isn't in our Bridge config lines. */
  if (router->purpose == ROUTER_PURPOSE_BRIDGE && from_cache &&
      !authdir_mode_bridge(options) &&
      !routerinfo_is_a_configured_bridge(router)) {
    log_info(LD_DIR, "Dropping bridge descriptor for %s because we have "
             "no bridge configured at that address.",
             safe_str_client(router_describe(router)));
    *msg = "Router descriptor was not a configured bridge.";
    routerinfo_free(router);
    return ROUTER_WAS_NOT_WANTED;
  }

  /* If we have a router with the same identity key, choose the newer one. */
  if (old_router) {
    if (!in_consensus && (router->cache_info.published_on <=
                          old_router->cache_info.published_on)) {
      /* Same key, but old.  This one is not listed in the consensus. */
      log_debug(LD_DIR, "Not-new descriptor for router %s",
                router_describe(router));
      /* Only journal this desc if we'll be serving it. */
      if (!from_cache && should_cache_old_descriptors())
        signed_desc_append_to_journal(&router->cache_info,
                                      &routerlist->desc_store);
      routerlist_insert_old(routerlist, router);
      *msg = "Router descriptor was not new.";
      return ROUTER_WAS_NOT_NEW;
    } else {
      /* Same key, and either new, or listed in the consensus. */
      log_debug(LD_DIR, "Replacing entry for router %s",
                router_describe(router));
      routerlist_replace(routerlist, old_router, router);
      if (!from_cache) {
        signed_desc_append_to_journal(&router->cache_info,
                                      &routerlist->desc_store);
      }
      *msg = authdir_believes_valid ? "Valid server updated" :
        ("Invalid server updated. (This dirserver is marking your "
         "server as unapproved.)");
      return ROUTER_ADDED_SUCCESSFULLY;
    }
  }

  if (!in_consensus && from_cache &&
      router->cache_info.published_on < time(NULL) - OLD_ROUTER_DESC_MAX_AGE) {
    *msg = "Router descriptor was really old.";
    routerinfo_free(router);
    return ROUTER_WAS_NOT_NEW;
  }

  /* We haven't seen a router with this identity before. Add it to the end of
   * the list. */
  routerlist_insert(routerlist, router);
  if (!from_cache) {
    signed_desc_append_to_journal(&router->cache_info,
                                  &routerlist->desc_store);
  }
  return ROUTER_ADDED_SUCCESSFULLY;
}

/** Insert <b>ei</b> into the routerlist, or free it. Other arguments are
 * as for router_add_to_routerlist().  Return ROUTER_ADDED_SUCCESSFULLY iff
 * we actually inserted it, ROUTER_BAD_EI otherwise.
 */
was_router_added_t
router_add_extrainfo_to_routerlist(extrainfo_t *ei, const char **msg,
                                   int from_cache, int from_fetch)
{
  int inserted;
  (void)from_fetch;
  if (msg) *msg = NULL;
  /*XXXX023 Do something with msg */

  inserted = extrainfo_insert(router_get_routerlist(), ei);

  if (inserted && !from_cache)
    signed_desc_append_to_journal(&ei->cache_info,
                                  &routerlist->extrainfo_store);

  if (inserted)
    return ROUTER_ADDED_SUCCESSFULLY;
  else
    return ROUTER_BAD_EI;
}

/** Sorting helper: return &lt;0, 0, or &gt;0 depending on whether the
 * signed_descriptor_t* in *<b>a</b> has an identity digest preceding, equal
 * to, or later than that of *<b>b</b>. */
static int
compare_old_routers_by_identity_(const void **_a, const void **_b)
{
  int i;
  const signed_descriptor_t *r1 = *_a, *r2 = *_b;
  if ((i = fast_memcmp(r1->identity_digest, r2->identity_digest, DIGEST_LEN)))
    return i;
  return (int)(r1->published_on - r2->published_on);
}

/** Internal type used to represent how long an old descriptor was valid,
 * where it appeared in the list of old descriptors, and whether it's extra
 * old. Used only by routerlist_remove_old_cached_routers_with_id(). */
struct duration_idx_t {
  int duration;
  int idx;
  int old;
};

/** Sorting helper: compare two duration_idx_t by their duration. */
static int
compare_duration_idx_(const void *_d1, const void *_d2)
{
  const struct duration_idx_t *d1 = _d1;
  const struct duration_idx_t *d2 = _d2;
  return d1->duration - d2->duration;
}

/** The range <b>lo</b> through <b>hi</b> inclusive of routerlist->old_routers
 * must contain routerinfo_t with the same identity and with publication time
 * in ascending order.  Remove members from this range until there are no more
 * than max_descriptors_per_router() remaining.  Start by removing the oldest
 * members from before <b>cutoff</b>, then remove members which were current
 * for the lowest amount of time.  The order of members of old_routers at
 * indices <b>lo</b> or higher may be changed.
 */
static void
routerlist_remove_old_cached_routers_with_id(time_t now,
                                             time_t cutoff, int lo, int hi,
                                             digestset_t *retain)
{
  int i, n = hi-lo+1;
  unsigned n_extra, n_rmv = 0;
  struct duration_idx_t *lifespans;
  uint8_t *rmv, *must_keep;
  smartlist_t *lst = routerlist->old_routers;
#if 1
  const char *ident;
  tor_assert(hi < smartlist_len(lst));
  tor_assert(lo <= hi);
  ident = ((signed_descriptor_t*)smartlist_get(lst, lo))->identity_digest;
  for (i = lo+1; i <= hi; ++i) {
    signed_descriptor_t *r = smartlist_get(lst, i);
    tor_assert(tor_memeq(ident, r->identity_digest, DIGEST_LEN));
  }
#endif
  /* Check whether we need to do anything at all. */
  {
    int mdpr = directory_caches_dir_info(get_options()) ? 2 : 1;
    if (n <= mdpr)
      return;
    n_extra = n - mdpr;
  }

  lifespans = tor_malloc_zero(sizeof(struct duration_idx_t)*n);
  rmv = tor_malloc_zero(sizeof(uint8_t)*n);
  must_keep = tor_malloc_zero(sizeof(uint8_t)*n);
  /* Set lifespans to contain the lifespan and index of each server. */
  /* Set rmv[i-lo]=1 if we're going to remove a server for being too old. */
  for (i = lo; i <= hi; ++i) {
    signed_descriptor_t *r = smartlist_get(lst, i);
    signed_descriptor_t *r_next;
    lifespans[i-lo].idx = i;
    if (r->last_listed_as_valid_until >= now ||
        (retain && digestset_contains(retain, r->signed_descriptor_digest))) {
      must_keep[i-lo] = 1;
    }
    if (i < hi) {
      r_next = smartlist_get(lst, i+1);
      tor_assert(r->published_on <= r_next->published_on);
      lifespans[i-lo].duration = (int)(r_next->published_on - r->published_on);
    } else {
      r_next = NULL;
      lifespans[i-lo].duration = INT_MAX;
    }
    if (!must_keep[i-lo] && r->published_on < cutoff && n_rmv < n_extra) {
      ++n_rmv;
      lifespans[i-lo].old = 1;
      rmv[i-lo] = 1;
    }
  }

  if (n_rmv < n_extra) {
    /**
     * We aren't removing enough servers for being old.  Sort lifespans by
     * the duration of liveness, and remove the ones we're not already going to
     * remove based on how long they were alive.
     **/
    qsort(lifespans, n, sizeof(struct duration_idx_t), compare_duration_idx_);
    for (i = 0; i < n && n_rmv < n_extra; ++i) {
      if (!must_keep[lifespans[i].idx-lo] && !lifespans[i].old) {
        rmv[lifespans[i].idx-lo] = 1;
        ++n_rmv;
      }
    }
  }

  i = hi;
  do {
    if (rmv[i-lo])
      routerlist_remove_old(routerlist, smartlist_get(lst, i), i);
  } while (--i >= lo);
  tor_free(must_keep);
  tor_free(rmv);
  tor_free(lifespans);
}

/** Deactivate any routers from the routerlist that are more than
 * ROUTER_MAX_AGE seconds old and not recommended by any networkstatuses;
 * remove old routers from the list of cached routers if we have too many.
 */
void
routerlist_remove_old_routers(void)
{
  int i, hi=-1;
  const char *cur_id = NULL;
  time_t now = time(NULL);
  time_t cutoff;
  routerinfo_t *router;
  signed_descriptor_t *sd;
  digestset_t *retain;
  const networkstatus_t *consensus = networkstatus_get_latest_consensus();

  trusted_dirs_remove_old_certs();

  if (!routerlist || !consensus)
    return;

  // routerlist_assert_ok(routerlist);

  /* We need to guess how many router descriptors we will wind up wanting to
     retain, so that we can be sure to allocate a large enough Bloom filter
     to hold the digest set.  Overestimating is fine; underestimating is bad.
  */
  {
    /* We'll probably retain everything in the consensus. */
    int n_max_retain = smartlist_len(consensus->routerstatus_list);
    retain = digestset_new(n_max_retain);
  }

  cutoff = now - OLD_ROUTER_DESC_MAX_AGE;
  /* Retain anything listed in the consensus. */
  if (consensus) {
    SMARTLIST_FOREACH(consensus->routerstatus_list, routerstatus_t *, rs,
        if (rs->published_on >= cutoff)
          digestset_add(retain, rs->descriptor_digest));
  }

  /* If we have a consensus, we should consider pruning current routers that
   * are too old and that nobody recommends.  (If we don't have a consensus,
   * then we should get one before we decide to kill routers.) */

  if (consensus) {
    cutoff = now - ROUTER_MAX_AGE;
    /* Remove too-old unrecommended members of routerlist->routers. */
    for (i = 0; i < smartlist_len(routerlist->routers); ++i) {
      router = smartlist_get(routerlist->routers, i);
      if (router->cache_info.published_on <= cutoff &&
          router->cache_info.last_listed_as_valid_until < now &&
          !digestset_contains(retain,
                          router->cache_info.signed_descriptor_digest)) {
        /* Too old: remove it.  (If we're a cache, just move it into
         * old_routers.) */
        log_info(LD_DIR,
                 "Forgetting obsolete (too old) routerinfo for router %s",
                 router_describe(router));
        routerlist_remove(routerlist, router, 1, now);
        i--;
      }
    }
  }

  //routerlist_assert_ok(routerlist);

  /* Remove far-too-old members of routerlist->old_routers. */
  cutoff = now - OLD_ROUTER_DESC_MAX_AGE;
  for (i = 0; i < smartlist_len(routerlist->old_routers); ++i) {
    sd = smartlist_get(routerlist->old_routers, i);
    if (sd->published_on <= cutoff &&
        sd->last_listed_as_valid_until < now &&
        !digestset_contains(retain, sd->signed_descriptor_digest)) {
      /* Too old.  Remove it. */
      routerlist_remove_old(routerlist, sd, i--);
    }
  }

  //routerlist_assert_ok(routerlist);

  log_info(LD_DIR, "We have %d live routers and %d old router descriptors.",
           smartlist_len(routerlist->routers),
           smartlist_len(routerlist->old_routers));

  /* Now we might have to look at routerlist->old_routers for extraneous
   * members. (We'd keep all the members if we could, but we need to save
   * space.) First, check whether we have too many router descriptors, total.
   * We're okay with having too many for some given router, so long as the
   * total number doesn't approach max_descriptors_per_router()*len(router).
   */
  if (smartlist_len(routerlist->old_routers) <
      smartlist_len(routerlist->routers))
    goto done;

  /* Sort by identity, then fix indices. */
  smartlist_sort(routerlist->old_routers, compare_old_routers_by_identity_);
  /* Fix indices. */
  for (i = 0; i < smartlist_len(routerlist->old_routers); ++i) {
    signed_descriptor_t *r = smartlist_get(routerlist->old_routers, i);
    r->routerlist_index = i;
  }

  /* Iterate through the list from back to front, so when we remove descriptors
   * we don't mess up groups we haven't gotten to. */
  for (i = smartlist_len(routerlist->old_routers)-1; i >= 0; --i) {
    signed_descriptor_t *r = smartlist_get(routerlist->old_routers, i);
    if (!cur_id) {
      cur_id = r->identity_digest;
      hi = i;
    }
    if (tor_memneq(cur_id, r->identity_digest, DIGEST_LEN)) {
      routerlist_remove_old_cached_routers_with_id(now,
                                                   cutoff, i+1, hi, retain);
      cur_id = r->identity_digest;
      hi = i;
    }
  }
  if (hi>=0)
    routerlist_remove_old_cached_routers_with_id(now, cutoff, 0, hi, retain);
  //routerlist_assert_ok(routerlist);

 done:
  digestset_free(retain);
  router_rebuild_store(RRS_DONT_REMOVE_OLD, &routerlist->desc_store);
  router_rebuild_store(RRS_DONT_REMOVE_OLD,&routerlist->extrainfo_store);
}

/** We just added a new set of descriptors. Take whatever extra steps
 * we need. */
void
routerlist_descriptors_added(smartlist_t *sl, int from_cache)
{
  tor_assert(sl);
  control_event_descriptors_changed(sl);
  SMARTLIST_FOREACH_BEGIN(sl, routerinfo_t *, ri) {
    if (ri->purpose == ROUTER_PURPOSE_BRIDGE)
      learned_bridge_descriptor(ri, from_cache);
    if (ri->needs_retest_if_added) {
      ri->needs_retest_if_added = 0;
      dirserv_single_reachability_test(approx_time(), ri);
    }
  } SMARTLIST_FOREACH_END(ri);
}

/**
 * Code to parse a single router descriptor and insert it into the
 * routerlist.  Return -1 if the descriptor was ill-formed; 0 if the
 * descriptor was well-formed but could not be added; and 1 if the
 * descriptor was added.
 *
 * If we don't add it and <b>msg</b> is not NULL, then assign to
 * *<b>msg</b> a static string describing the reason for refusing the
 * descriptor.
 *
 * This is used only by the controller.
 */
int
router_load_single_router(const char *s, uint8_t purpose, int cache,
                          const char **msg)
{
  routerinfo_t *ri;
  was_router_added_t r;
  smartlist_t *lst;
  char annotation_buf[ROUTER_ANNOTATION_BUF_LEN];
  tor_assert(msg);
  *msg = NULL;

  tor_snprintf(annotation_buf, sizeof(annotation_buf),
               "@source controller\n"
               "@purpose %s\n", router_purpose_to_string(purpose));

  if (!(ri = router_parse_entry_from_string(s, NULL, 1, 0, annotation_buf))) {
    log_warn(LD_DIR, "Error parsing router descriptor; dropping.");
    *msg = "Couldn't parse router descriptor.";
    return -1;
  }
  tor_assert(ri->purpose == purpose);
  if (router_is_me(ri)) {
    log_warn(LD_DIR, "Router's identity key matches mine; dropping.");
    *msg = "Router's identity key matches mine.";
    routerinfo_free(ri);
    return 0;
  }

  if (!cache) /* obey the preference of the controller */
    ri->cache_info.do_not_cache = 1;

  lst = smartlist_new();
  smartlist_add(lst, ri);
  routers_update_status_from_consensus_networkstatus(lst, 0);

  r = router_add_to_routerlist(ri, msg, 0, 0);
  if (!WRA_WAS_ADDED(r)) {
    /* we've already assigned to *msg now, and ri is already freed */
    tor_assert(*msg);
    if (r == ROUTER_AUTHDIR_REJECTS)
      log_warn(LD_DIR, "Couldn't add router to list: %s Dropping.", *msg);
    smartlist_free(lst);
    return 0;
  } else {
    routerlist_descriptors_added(lst, 0);
    smartlist_free(lst);
    log_debug(LD_DIR, "Added router to list");
    return 1;
  }
}

/** Given a string <b>s</b> containing some routerdescs, parse it and put the
 * routers into our directory.  If saved_location is SAVED_NOWHERE, the routers
 * are in response to a query to the network: cache them by adding them to
 * the journal.
 *
 * Return the number of routers actually added.
 *
 * If <b>requested_fingerprints</b> is provided, it must contain a list of
 * uppercased fingerprints.  Do not update any router whose
 * fingerprint is not on the list; after updating a router, remove its
 * fingerprint from the list.
 *
 * If <b>descriptor_digests</b> is non-zero, then the requested_fingerprints
 * are descriptor digests. Otherwise they are identity digests.
 */
int
router_load_routers_from_string(const char *s, const char *eos,
                                saved_location_t saved_location,
                                smartlist_t *requested_fingerprints,
                                int descriptor_digests,
                                const char *prepend_annotations)
{
  smartlist_t *routers = smartlist_new(), *changed = smartlist_new();
  char fp[HEX_DIGEST_LEN+1];
  const char *msg;
  int from_cache = (saved_location != SAVED_NOWHERE);
  int allow_annotations = (saved_location != SAVED_NOWHERE);
  int any_changed = 0;

  router_parse_list_from_string(&s, eos, routers, saved_location, 0,
                                allow_annotations, prepend_annotations);

  routers_update_status_from_consensus_networkstatus(routers, !from_cache);

  log_info(LD_DIR, "%d elements to add", smartlist_len(routers));

  SMARTLIST_FOREACH_BEGIN(routers, routerinfo_t *, ri) {
    was_router_added_t r;
    char d[DIGEST_LEN];
    if (requested_fingerprints) {
      base16_encode(fp, sizeof(fp), descriptor_digests ?
                      ri->cache_info.signed_descriptor_digest :
                      ri->cache_info.identity_digest,
                    DIGEST_LEN);
      if (smartlist_contains_string(requested_fingerprints, fp)) {
        smartlist_string_remove(requested_fingerprints, fp);
      } else {
        char *requested =
          smartlist_join_strings(requested_fingerprints," ",0,NULL);
        log_warn(LD_DIR,
                 "We received a router descriptor with a fingerprint (%s) "
                 "that we never requested. (We asked for: %s.) Dropping.",
                 fp, requested);
        tor_free(requested);
        routerinfo_free(ri);
        continue;
      }
    }

    memcpy(d, ri->cache_info.signed_descriptor_digest, DIGEST_LEN);
    r = router_add_to_routerlist(ri, &msg, from_cache, !from_cache);
    if (WRA_WAS_ADDED(r)) {
      any_changed++;
      smartlist_add(changed, ri);
      routerlist_descriptors_added(changed, from_cache);
      smartlist_clear(changed);
    } else if (WRA_WAS_REJECTED(r)) {
      download_status_t *dl_status;
      dl_status = router_get_dl_status_by_descriptor_digest(d);
      if (dl_status) {
        log_info(LD_GENERAL, "Marking router %s as never downloadable",
                 hex_str(d, DIGEST_LEN));
        download_status_mark_impossible(dl_status);
      }
    }
  } SMARTLIST_FOREACH_END(ri);

  routerlist_assert_ok(routerlist);

  if (any_changed)
    router_rebuild_store(0, &routerlist->desc_store);

  smartlist_free(routers);
  smartlist_free(changed);

  return any_changed;
}

/** Parse one or more extrainfos from <b>s</b> (ending immediately before
 * <b>eos</b> if <b>eos</b> is present).  Other arguments are as for
 * router_load_routers_from_string(). */
void
router_load_extrainfo_from_string(const char *s, const char *eos,
                                  saved_location_t saved_location,
                                  smartlist_t *requested_fingerprints,
                                  int descriptor_digests)
{
  smartlist_t *extrainfo_list = smartlist_new();
  const char *msg;
  int from_cache = (saved_location != SAVED_NOWHERE);

  router_parse_list_from_string(&s, eos, extrainfo_list, saved_location, 1, 0,
                                NULL);

  log_info(LD_DIR, "%d elements to add", smartlist_len(extrainfo_list));

  SMARTLIST_FOREACH_BEGIN(extrainfo_list, extrainfo_t *, ei) {
      was_router_added_t added =
        router_add_extrainfo_to_routerlist(ei, &msg, from_cache, !from_cache);
      if (WRA_WAS_ADDED(added) && requested_fingerprints) {
        char fp[HEX_DIGEST_LEN+1];
        base16_encode(fp, sizeof(fp), descriptor_digests ?
                        ei->cache_info.signed_descriptor_digest :
                        ei->cache_info.identity_digest,
                      DIGEST_LEN);
        smartlist_string_remove(requested_fingerprints, fp);
        /* We silently let people stuff us with extrainfos we didn't ask for,
         * so long as we would have wanted them anyway.  Since we always fetch
         * all the extrainfos we want, and we never actually act on them
         * inside Tor, this should be harmless. */
      }
  } SMARTLIST_FOREACH_END(ei);

  routerlist_assert_ok(routerlist);
  router_rebuild_store(0, &router_get_routerlist()->extrainfo_store);

  smartlist_free(extrainfo_list);
}

/** Return true iff any networkstatus includes a descriptor whose digest
 * is that of <b>desc</b>. */
static int
signed_desc_digest_is_recognized(signed_descriptor_t *desc)
{
  const routerstatus_t *rs;
  networkstatus_t *consensus = networkstatus_get_latest_consensus();

  if (consensus) {
    rs = networkstatus_vote_find_entry(consensus, desc->identity_digest);
    if (rs && tor_memeq(rs->descriptor_digest,
                      desc->signed_descriptor_digest, DIGEST_LEN))
      return 1;
  }
  return 0;
}

/** Update downloads for router descriptors and/or microdescriptors as
 * appropriate. */
void
update_all_descriptor_downloads(time_t now)
{
  if (get_options()->DisableNetwork)
    return;
  update_router_descriptor_downloads(now);
  update_microdesc_downloads(now);
  launch_dummy_descriptor_download_as_needed(now, get_options());
}

/** Clear all our timeouts for fetching v3 directory stuff, and then
 * give it all a try again. */
void
routerlist_retry_directory_downloads(time_t now)
{
  router_reset_status_download_failures();
  router_reset_descriptor_download_failures();
  if (get_options()->DisableNetwork)
    return;
  update_networkstatus_downloads(now);
  update_all_descriptor_downloads(now);
}

/** Return true iff <b>router</b> does not permit exit streams.
 */
int
router_exit_policy_rejects_all(const routerinfo_t *router)
{
  return router->policy_is_reject_star;
}

/** Create an directory server at <b>address</b>:<b>port</b>, with OR identity
 * key <b>digest</b>.  If <b>address</b> is NULL, add ourself.  If
 * <b>is_authority</b>, this is a directory authority.  Return the new
 * directory server entry on success or NULL on failure. */
static dir_server_t *
dir_server_new(int is_authority,
               const char *nickname,
               const tor_addr_t *addr,
               const char *hostname,
               uint16_t dir_port, uint16_t or_port,
               const char *digest, const char *v3_auth_digest,
               dirinfo_type_t type,
               double weight)
{
  dir_server_t *ent;
  uint32_t a;
  char *hostname_ = NULL;

  if (weight < 0)
    return NULL;

  if (tor_addr_family(addr) == AF_INET)
    a = tor_addr_to_ipv4h(addr);
  else
    return NULL; /*XXXX Support IPv6 */

  if (!hostname)
    hostname_ = tor_dup_addr(addr);
  else
    hostname_ = tor_strdup(hostname);

  ent = tor_malloc_zero(sizeof(dir_server_t));
  ent->nickname = nickname ? tor_strdup(nickname) : NULL;
  ent->address = hostname_;
  ent->addr = a;
  ent->dir_port = dir_port;
  ent->or_port = or_port;
  ent->is_running = 1;
  ent->is_authority = is_authority;
  ent->type = type;
  ent->weight = weight;
  memcpy(ent->digest, digest, DIGEST_LEN);
  if (v3_auth_digest && (type & V3_DIRINFO))
    memcpy(ent->v3_identity_digest, v3_auth_digest, DIGEST_LEN);

  if (nickname)
    tor_asprintf(&ent->description, "directory server \"%s\" at %s:%d",
                 nickname, hostname, (int)dir_port);
  else
    tor_asprintf(&ent->description, "directory server at %s:%d",
                 hostname, (int)dir_port);

  ent->fake_status.addr = ent->addr;
  memcpy(ent->fake_status.identity_digest, digest, DIGEST_LEN);
  if (nickname)
    strlcpy(ent->fake_status.nickname, nickname,
            sizeof(ent->fake_status.nickname));
  else
    ent->fake_status.nickname[0] = '\0';
  ent->fake_status.dir_port = ent->dir_port;
  ent->fake_status.or_port = ent->or_port;

  return ent;
}

/** Create an authoritative directory server at
 * <b>address</b>:<b>port</b>, with identity key <b>digest</b>.  If
 * <b>address</b> is NULL, add ourself.  Return the new trusted directory
 * server entry on success or NULL if we couldn't add it. */
dir_server_t *
trusted_dir_server_new(const char *nickname, const char *address,
                       uint16_t dir_port, uint16_t or_port,
                       const char *digest, const char *v3_auth_digest,
                       dirinfo_type_t type, double weight)
{
  uint32_t a;
  tor_addr_t addr;
  char *hostname=NULL;
  dir_server_t *result;

  if (!address) { /* The address is us; we should guess. */
    if (resolve_my_address(LOG_WARN, get_options(),
                           &a, NULL, &hostname) < 0) {
      log_warn(LD_CONFIG,
               "Couldn't find a suitable address when adding ourself as a "
               "trusted directory server.");
      return NULL;
    }
    if (!hostname)
      hostname = tor_dup_ip(a);
  } else {
    if (tor_lookup_hostname(address, &a)) {
      log_warn(LD_CONFIG,
               "Unable to lookup address for directory server at '%s'",
               address);
      return NULL;
    }
    hostname = tor_strdup(address);
  }
  tor_addr_from_ipv4h(&addr, a);

  result = dir_server_new(1, nickname, &addr, hostname,
                          dir_port, or_port, digest,
                          v3_auth_digest, type, weight);
  tor_free(hostname);
  return result;
}

/** Return a new dir_server_t for a fallback directory server at
 * <b>addr</b>:<b>or_port</b>/<b>dir_port</b>, with identity key digest
 * <b>id_digest</b> */
dir_server_t *
fallback_dir_server_new(const tor_addr_t *addr,
                        uint16_t dir_port, uint16_t or_port,
                        const char *id_digest, double weight)
{
  return dir_server_new(0, NULL, addr, NULL, dir_port, or_port, id_digest,
                        NULL, ALL_DIRINFO, weight);
}

/** Add a directory server to the global list(s). */
void
dir_server_add(dir_server_t *ent)
{
  if (!trusted_dir_servers)
    trusted_dir_servers = smartlist_new();
  if (!fallback_dir_servers)
    fallback_dir_servers = smartlist_new();

  if (ent->is_authority)
    smartlist_add(trusted_dir_servers, ent);

  smartlist_add(fallback_dir_servers, ent);
  router_dir_info_changed();
}

/** Free storage held in <b>cert</b>. */
void
authority_cert_free(authority_cert_t *cert)
{
  if (!cert)
    return;

  tor_free(cert->cache_info.signed_descriptor_body);
  crypto_pk_free(cert->signing_key);
  crypto_pk_free(cert->identity_key);

  tor_free(cert);
}

/** Free storage held in <b>ds</b>. */
static void
dir_server_free(dir_server_t *ds)
{
  if (!ds)
    return;

  tor_free(ds->nickname);
  tor_free(ds->description);
  tor_free(ds->address);
  tor_free(ds);
}

/** Remove all members from the list of dir servers. */
void
clear_dir_servers(void)
{
  if (fallback_dir_servers) {
    SMARTLIST_FOREACH(fallback_dir_servers, dir_server_t *, ent,
                      dir_server_free(ent));
    smartlist_clear(fallback_dir_servers);
  } else {
    fallback_dir_servers = smartlist_new();
  }
  if (trusted_dir_servers) {
    smartlist_clear(trusted_dir_servers);
  } else {
    trusted_dir_servers = smartlist_new();
  }
  router_dir_info_changed();
}

/** For every current directory connection whose purpose is <b>purpose</b>,
 * and where the resource being downloaded begins with <b>prefix</b>, split
 * rest of the resource into base16 fingerprints (or base64 fingerprints if
 * purpose==DIR_PURPPOSE_FETCH_MICRODESC), decode them, and set the
 * corresponding elements of <b>result</b> to a nonzero value.
 */
static void
list_pending_downloads(digestmap_t *result,
                       int purpose, const char *prefix)
{
  const size_t p_len = strlen(prefix);
  smartlist_t *tmp = smartlist_new();
  smartlist_t *conns = get_connection_array();
  int flags = DSR_HEX;
  if (purpose == DIR_PURPOSE_FETCH_MICRODESC)
    flags = DSR_DIGEST256|DSR_BASE64;

  tor_assert(result);

  SMARTLIST_FOREACH_BEGIN(conns, connection_t *, conn) {
    if (conn->type == CONN_TYPE_DIR &&
        conn->purpose == purpose &&
        !conn->marked_for_close) {
      const char *resource = TO_DIR_CONN(conn)->requested_resource;
      if (!strcmpstart(resource, prefix))
        dir_split_resource_into_fingerprints(resource + p_len,
                                             tmp, NULL, flags);
    }
  } SMARTLIST_FOREACH_END(conn);

  SMARTLIST_FOREACH(tmp, char *, d,
                    {
                      digestmap_set(result, d, (void*)1);
                      tor_free(d);
                    });
  smartlist_free(tmp);
}

/** For every router descriptor (or extra-info document if <b>extrainfo</b> is
 * true) we are currently downloading by descriptor digest, set result[d] to
 * (void*)1. */
static void
list_pending_descriptor_downloads(digestmap_t *result, int extrainfo)
{
  int purpose =
    extrainfo ? DIR_PURPOSE_FETCH_EXTRAINFO : DIR_PURPOSE_FETCH_SERVERDESC;
  list_pending_downloads(result, purpose, "d/");
}

/** For every microdescriptor we are currently downloading by descriptor
 * digest, set result[d] to (void*)1.   (Note that microdescriptor digests
 * are 256-bit, and digestmap_t only holds 160-bit digests, so we're only
 * getting the first 20 bytes of each digest here.)
 *
 * XXXX Let there be a digestmap256_t, and use that instead.
 */
void
list_pending_microdesc_downloads(digestmap_t *result)
{
  list_pending_downloads(result, DIR_PURPOSE_FETCH_MICRODESC, "d/");
}

/** For every certificate we are currently downloading by (identity digest,
 * signing key digest) pair, set result[fp_pair] to (void *1).
 */
static void
list_pending_fpsk_downloads(fp_pair_map_t *result)
{
  const char *pfx = "fp-sk/";
  smartlist_t *tmp;
  smartlist_t *conns;
  const char *resource;

  tor_assert(result);

  tmp = smartlist_new();
  conns = get_connection_array();

  SMARTLIST_FOREACH_BEGIN(conns, connection_t *, conn) {
    if (conn->type == CONN_TYPE_DIR &&
        conn->purpose == DIR_PURPOSE_FETCH_CERTIFICATE &&
        !conn->marked_for_close) {
      resource = TO_DIR_CONN(conn)->requested_resource;
      if (!strcmpstart(resource, pfx))
        dir_split_resource_into_fingerprint_pairs(resource + strlen(pfx),
                                                  tmp);
    }
  } SMARTLIST_FOREACH_END(conn);

  SMARTLIST_FOREACH_BEGIN(tmp, fp_pair_t *, fp) {
    fp_pair_map_set(result, fp, (void*)1);
    tor_free(fp);
  } SMARTLIST_FOREACH_END(fp);

  smartlist_free(tmp);
}

/** Launch downloads for all the descriptors whose digests or digests256
 * are listed as digests[i] for lo <= i < hi.  (Lo and hi may be out of
 * range.)  If <b>source</b> is given, download from <b>source</b>;
 * otherwise, download from an appropriate random directory server.
 */
static void
initiate_descriptor_downloads(const routerstatus_t *source,
                              int purpose,
                              smartlist_t *digests,
                              int lo, int hi, int pds_flags)
{
  int i, n = hi-lo;
  char *resource, *cp;
  size_t r_len;

  int digest_len = DIGEST_LEN, enc_digest_len = HEX_DIGEST_LEN;
  char sep = '+';
  int b64_256 = 0;

  if (purpose == DIR_PURPOSE_FETCH_MICRODESC) {
    /* Microdescriptors are downloaded by "-"-separated base64-encoded
     * 256-bit digests. */
    digest_len = DIGEST256_LEN;
    enc_digest_len = BASE64_DIGEST256_LEN;
    sep = '-';
    b64_256 = 1;
  }

  if (n <= 0)
    return;
  if (lo < 0)
    lo = 0;
  if (hi > smartlist_len(digests))
    hi = smartlist_len(digests);

  r_len = 8 + (enc_digest_len+1)*n;
  cp = resource = tor_malloc(r_len);
  memcpy(cp, "d/", 2);
  cp += 2;
  for (i = lo; i < hi; ++i) {
    if (b64_256) {
      digest256_to_base64(cp, smartlist_get(digests, i));
    } else {
      base16_encode(cp, r_len-(cp-resource),
                    smartlist_get(digests,i), digest_len);
    }
    cp += enc_digest_len;
    *cp++ = sep;
  }
  memcpy(cp-1, ".z", 3);

  if (source) {
    /* We know which authority we want. */
    directory_initiate_command_routerstatus(source, purpose,
                                            ROUTER_PURPOSE_GENERAL,
                                            DIRIND_ONEHOP,
                                            resource, NULL, 0, 0);
  } else {
    directory_get_from_dirserver(purpose, ROUTER_PURPOSE_GENERAL, resource,
                                 pds_flags);
  }
  tor_free(resource);
}

/** Max amount of hashes to download per request.
 * Since squid does not like URLs >= 4096 bytes we limit it to 96.
 *   4096 - strlen(http://255.255.255.255/tor/server/d/.z) == 4058
 *   4058/41 (40 for the hash and 1 for the + that separates them) => 98
 *   So use 96 because it's a nice number.
 */
#define MAX_DL_PER_REQUEST 96
#define MAX_MICRODESC_DL_PER_REQUEST 92
/** Don't split our requests so finely that we are requesting fewer than
 * this number per server. */
#define MIN_DL_PER_REQUEST 4
/** To prevent a single screwy cache from confusing us by selective reply,
 * try to split our requests into at least this many requests. */
#define MIN_REQUESTS 3
/** If we want fewer than this many descriptors, wait until we
 * want more, or until TestingClientMaxIntervalWithoutRequest has passed. */
#define MAX_DL_TO_DELAY 16

/** Given a <b>purpose</b> (FETCH_MICRODESC or FETCH_SERVERDESC) and a list of
 * router descriptor digests or microdescriptor digest256s in
 * <b>downloadable</b>, decide whether to delay fetching until we have more.
 * If we don't want to delay, launch one or more requests to the appropriate
 * directory authorities.
 */
void
launch_descriptor_downloads(int purpose,
                            smartlist_t *downloadable,
                            const routerstatus_t *source, time_t now)
{
  int should_delay = 0, n_downloadable;
  const or_options_t *options = get_options();
  const char *descname;

  tor_assert(purpose == DIR_PURPOSE_FETCH_SERVERDESC ||
             purpose == DIR_PURPOSE_FETCH_MICRODESC);

  descname = (purpose == DIR_PURPOSE_FETCH_SERVERDESC) ?
    "routerdesc" : "microdesc";

  n_downloadable = smartlist_len(downloadable);
  if (!directory_fetches_dir_info_early(options)) {
    if (n_downloadable >= MAX_DL_TO_DELAY) {
      log_debug(LD_DIR,
                "There are enough downloadable %ss to launch requests.",
                descname);
      should_delay = 0;
    } else {
      should_delay = (last_descriptor_download_attempted +
                      options->TestingClientMaxIntervalWithoutRequest) > now;
      if (!should_delay && n_downloadable) {
        if (last_descriptor_download_attempted) {
          log_info(LD_DIR,
                   "There are not many downloadable %ss, but we've "
                   "been waiting long enough (%d seconds). Downloading.",
                   descname,
                   (int)(now-last_descriptor_download_attempted));
        } else {
          log_info(LD_DIR,
                   "There are not many downloadable %ss, but we haven't "
                   "tried downloading descriptors recently. Downloading.",
                   descname);
        }
      }
    }
  }

  if (! should_delay && n_downloadable) {
    int i, n_per_request;
    const char *req_plural = "", *rtr_plural = "";
    int pds_flags = PDS_RETRY_IF_NO_SERVERS;
    if (! authdir_mode_any_nonhidserv(options)) {
      /* If we wind up going to the authorities, we want to only open one
       * connection to each authority at a time, so that we don't overload
       * them.  We do this by setting PDS_NO_EXISTING_SERVERDESC_FETCH
       * regardless of whether we're a cache or not; it gets ignored if we're
       * not calling router_pick_trusteddirserver.
       *
       * Setting this flag can make initiate_descriptor_downloads() ignore
       * requests.  We need to make sure that we do in fact call
       * update_router_descriptor_downloads() later on, once the connections
       * have succeeded or failed.
       */
      pds_flags |= (purpose == DIR_PURPOSE_FETCH_MICRODESC) ?
        PDS_NO_EXISTING_MICRODESC_FETCH :
        PDS_NO_EXISTING_SERVERDESC_FETCH;
    }

    n_per_request = CEIL_DIV(n_downloadable, MIN_REQUESTS);
    if (purpose == DIR_PURPOSE_FETCH_MICRODESC) {
      if (n_per_request > MAX_MICRODESC_DL_PER_REQUEST)
        n_per_request = MAX_MICRODESC_DL_PER_REQUEST;
    } else {
      if (n_per_request > MAX_DL_PER_REQUEST)
        n_per_request = MAX_DL_PER_REQUEST;
    }
    if (n_per_request < MIN_DL_PER_REQUEST)
      n_per_request = MIN_DL_PER_REQUEST;

    if (n_downloadable > n_per_request)
      req_plural = rtr_plural = "s";
    else if (n_downloadable > 1)
      rtr_plural = "s";

    log_info(LD_DIR,
             "Launching %d request%s for %d %s%s, %d at a time",
             CEIL_DIV(n_downloadable, n_per_request), req_plural,
             n_downloadable, descname, rtr_plural, n_per_request);
    smartlist_sort_digests(downloadable);
    for (i=0; i < n_downloadable; i += n_per_request) {
      initiate_descriptor_downloads(source, purpose,
                                    downloadable, i, i+n_per_request,
                                    pds_flags);
    }
    last_descriptor_download_attempted = now;
  }
}

/** For any descriptor that we want that's currently listed in
 * <b>consensus</b>, download it as appropriate. */
void
update_consensus_router_descriptor_downloads(time_t now, int is_vote,
                                             networkstatus_t *consensus)
{
  const or_options_t *options = get_options();
  digestmap_t *map = NULL;
  smartlist_t *no_longer_old = smartlist_new();
  smartlist_t *downloadable = smartlist_new();
  routerstatus_t *source = NULL;
  int authdir = authdir_mode(options);
  int n_delayed=0, n_have=0, n_would_reject=0, n_wouldnt_use=0,
    n_inprogress=0, n_in_oldrouters=0;

  if (directory_too_idle_to_fetch_descriptors(options, now))
    goto done;
  if (!consensus)
    goto done;

  if (is_vote) {
    /* where's it from, so we know whom to ask for descriptors */
    dir_server_t *ds;
    networkstatus_voter_info_t *voter = smartlist_get(consensus->voters, 0);
    tor_assert(voter);
    ds = trusteddirserver_get_by_v3_auth_digest(voter->identity_digest);
    if (ds)
      source = &(ds->fake_status);
    else
      log_warn(LD_DIR, "couldn't lookup source from vote?");
  }

  map = digestmap_new();
  list_pending_descriptor_downloads(map, 0);
  SMARTLIST_FOREACH_BEGIN(consensus->routerstatus_list, void *, rsp) {
      routerstatus_t *rs =
        is_vote ? &(((vote_routerstatus_t *)rsp)->status) : rsp;
      signed_descriptor_t *sd;
      if ((sd = router_get_by_descriptor_digest(rs->descriptor_digest))) {
        const routerinfo_t *ri;
        ++n_have;
        if (!(ri = router_get_by_id_digest(rs->identity_digest)) ||
            tor_memneq(ri->cache_info.signed_descriptor_digest,
                   sd->signed_descriptor_digest, DIGEST_LEN)) {
          /* We have a descriptor with this digest, but either there is no
           * entry in routerlist with the same ID (!ri), or there is one,
           * but the identity digest differs (memneq).
           */
          smartlist_add(no_longer_old, sd);
          ++n_in_oldrouters; /* We have it in old_routers. */
        }
        continue; /* We have it already. */
      }
      if (digestmap_get(map, rs->descriptor_digest)) {
        ++n_inprogress;
        continue; /* We have an in-progress download. */
      }
      if (!download_status_is_ready(&rs->dl_status, now,
                          options->TestingDescriptorMaxDownloadTries)) {
        ++n_delayed; /* Not ready for retry. */
        continue;
      }
      if (authdir && dirserv_would_reject_router(rs)) {
        ++n_would_reject;
        continue; /* We would throw it out immediately. */
      }
      if (!directory_caches_dir_info(options) &&
          !client_would_use_router(rs, now, options)) {
        ++n_wouldnt_use;
        continue; /* We would never use it ourself. */
      }
      if (is_vote && source) {
        char time_bufnew[ISO_TIME_LEN+1];
        char time_bufold[ISO_TIME_LEN+1];
        const routerinfo_t *oldrouter;
        oldrouter = router_get_by_id_digest(rs->identity_digest);
        format_iso_time(time_bufnew, rs->published_on);
        if (oldrouter)
          format_iso_time(time_bufold, oldrouter->cache_info.published_on);
        log_info(LD_DIR, "Learned about %s (%s vs %s) from %s's vote (%s)",
                 routerstatus_describe(rs),
                 time_bufnew,
                 oldrouter ? time_bufold : "none",
                 source->nickname, oldrouter ? "known" : "unknown");
      }
      smartlist_add(downloadable, rs->descriptor_digest);
  } SMARTLIST_FOREACH_END(rsp);

  if (!authdir_mode_handles_descs(options, ROUTER_PURPOSE_GENERAL)
      && smartlist_len(no_longer_old)) {
    routerlist_t *rl = router_get_routerlist();
    log_info(LD_DIR, "%d router descriptors listed in consensus are "
             "currently in old_routers; making them current.",
             smartlist_len(no_longer_old));
    SMARTLIST_FOREACH_BEGIN(no_longer_old, signed_descriptor_t *, sd) {
        const char *msg;
        was_router_added_t r;
        routerinfo_t *ri = routerlist_reparse_old(rl, sd);
        if (!ri) {
          log_warn(LD_BUG, "Failed to re-parse a router.");
          continue;
        }
        r = router_add_to_routerlist(ri, &msg, 1, 0);
        if (WRA_WAS_OUTDATED(r)) {
          log_warn(LD_DIR, "Couldn't add re-parsed router: %s",
                   msg?msg:"???");
        }
    } SMARTLIST_FOREACH_END(sd);
    routerlist_assert_ok(rl);
  }

  log_info(LD_DIR,
           "%d router descriptors downloadable. %d delayed; %d present "
           "(%d of those were in old_routers); %d would_reject; "
           "%d wouldnt_use; %d in progress.",
           smartlist_len(downloadable), n_delayed, n_have, n_in_oldrouters,
           n_would_reject, n_wouldnt_use, n_inprogress);

  launch_descriptor_downloads(DIR_PURPOSE_FETCH_SERVERDESC,
                              downloadable, source, now);

  digestmap_free(map, NULL);
 done:
  smartlist_free(downloadable);
  smartlist_free(no_longer_old);
}

/** How often should we launch a server/authority request to be sure of getting
 * a guess for our IP? */
/*XXXX024 this info should come from netinfo cells or something, or we should
 * do this only when we aren't seeing incoming data. see bug 652. */
#define DUMMY_DOWNLOAD_INTERVAL (20*60)

/** As needed, launch a dummy router descriptor fetch to see if our
 * address has changed. */
static void
launch_dummy_descriptor_download_as_needed(time_t now,
                                           const or_options_t *options)
{
  static time_t last_dummy_download = 0;
  /* XXXX024 we could be smarter here; see notes on bug 652. */
  /* If we're a server that doesn't have a configured address, we rely on
   * directory fetches to learn when our address changes.  So if we haven't
   * tried to get any routerdescs in a long time, try a dummy fetch now. */
  if (!options->Address &&
      server_mode(options) &&
      last_descriptor_download_attempted + DUMMY_DOWNLOAD_INTERVAL < now &&
      last_dummy_download + DUMMY_DOWNLOAD_INTERVAL < now) {
    last_dummy_download = now;
    directory_get_from_dirserver(DIR_PURPOSE_FETCH_SERVERDESC,
                                 ROUTER_PURPOSE_GENERAL, "authority.z",
                                 PDS_RETRY_IF_NO_SERVERS);
  }
}

/** Launch downloads for router status as needed. */
void
update_router_descriptor_downloads(time_t now)
{
  const or_options_t *options = get_options();
  if (should_delay_dir_fetches(options, NULL))
    return;
  if (!we_fetch_router_descriptors(options))
    return;

  update_consensus_router_descriptor_downloads(now, 0,
                  networkstatus_get_reasonably_live_consensus(now, FLAV_NS));
}

/** Launch extrainfo downloads as needed. */
void
update_extrainfo_downloads(time_t now)
{
  const or_options_t *options = get_options();
  routerlist_t *rl;
  smartlist_t *wanted;
  digestmap_t *pending;
  int old_routers, i;
  int n_no_ei = 0, n_pending = 0, n_have = 0, n_delay = 0;
  if (! options->DownloadExtraInfo)
    return;
  if (should_delay_dir_fetches(options, NULL))
    return;
  if (!router_have_minimum_dir_info())
    return;

  pending = digestmap_new();
  list_pending_descriptor_downloads(pending, 1);
  rl = router_get_routerlist();
  wanted = smartlist_new();
  for (old_routers = 0; old_routers < 2; ++old_routers) {
    smartlist_t *lst = old_routers ? rl->old_routers : rl->routers;
    for (i = 0; i < smartlist_len(lst); ++i) {
      signed_descriptor_t *sd;
      char *d;
      if (old_routers)
        sd = smartlist_get(lst, i);
      else
        sd = &((routerinfo_t*)smartlist_get(lst, i))->cache_info;
      if (sd->is_extrainfo)
        continue; /* This should never happen. */
      if (old_routers && !router_get_by_id_digest(sd->identity_digest))
        continue; /* Couldn't check the signature if we got it. */
      if (sd->extrainfo_is_bogus)
        continue;
      d = sd->extra_info_digest;
      if (tor_digest_is_zero(d)) {
        ++n_no_ei;
        continue;
      }
      if (eimap_get(rl->extra_info_map, d)) {
        ++n_have;
        continue;
      }
      if (!download_status_is_ready(&sd->ei_dl_status, now,
                          options->TestingDescriptorMaxDownloadTries)) {
        ++n_delay;
        continue;
      }
      if (digestmap_get(pending, d)) {
        ++n_pending;
        continue;
      }
      smartlist_add(wanted, d);
    }
  }
  digestmap_free(pending, NULL);

  log_info(LD_DIR, "Extrainfo download status: %d router with no ei, %d "
           "with present ei, %d delaying, %d pending, %d downloadable.",
           n_no_ei, n_have, n_delay, n_pending, smartlist_len(wanted));

  smartlist_shuffle(wanted);
  for (i = 0; i < smartlist_len(wanted); i += MAX_DL_PER_REQUEST) {
    initiate_descriptor_downloads(NULL, DIR_PURPOSE_FETCH_EXTRAINFO,
                                  wanted, i, i + MAX_DL_PER_REQUEST,
                PDS_RETRY_IF_NO_SERVERS|PDS_NO_EXISTING_SERVERDESC_FETCH);
  }

  smartlist_free(wanted);
}

/** Reset the descriptor download failure count on all routers, so that we
 * can retry any long-failed routers immediately.
 */
void
router_reset_descriptor_download_failures(void)
{
  networkstatus_reset_download_failures();
  last_descriptor_download_attempted = 0;
  if (!routerlist)
    return;
  SMARTLIST_FOREACH(routerlist->routers, routerinfo_t *, ri,
  {
    download_status_reset(&ri->cache_info.ei_dl_status);
  });
  SMARTLIST_FOREACH(routerlist->old_routers, signed_descriptor_t *, sd,
  {
    download_status_reset(&sd->ei_dl_status);
  });
}

/** Any changes in a router descriptor's publication time larger than this are
 * automatically non-cosmetic. */
#define ROUTER_MAX_COSMETIC_TIME_DIFFERENCE (2*60*60)

/** We allow uptime to vary from how much it ought to be by this much. */
#define ROUTER_ALLOW_UPTIME_DRIFT (6*60*60)

/** Return true iff the only differences between r1 and r2 are such that
 * would not cause a recent (post 0.1.1.6) dirserver to republish.
 */
int
router_differences_are_cosmetic(const routerinfo_t *r1, const routerinfo_t *r2)
{
  time_t r1pub, r2pub;
  long time_difference;
  tor_assert(r1 && r2);

  /* r1 should be the one that was published first. */
  if (r1->cache_info.published_on > r2->cache_info.published_on) {
    const routerinfo_t *ri_tmp = r2;
    r2 = r1;
    r1 = ri_tmp;
  }

  /* If any key fields differ, they're different. */
  if (r1->addr != r2->addr ||
      strcasecmp(r1->nickname, r2->nickname) ||
      r1->or_port != r2->or_port ||
      !tor_addr_eq(&r1->ipv6_addr, &r2->ipv6_addr) ||
      r1->ipv6_orport != r2->ipv6_orport ||
      r1->dir_port != r2->dir_port ||
      r1->purpose != r2->purpose ||
      !crypto_pk_eq_keys(r1->onion_pkey, r2->onion_pkey) ||
      !crypto_pk_eq_keys(r1->identity_pkey, r2->identity_pkey) ||
      strcasecmp(r1->platform, r2->platform) ||
      (r1->contact_info && !r2->contact_info) || /* contact_info is optional */
      (!r1->contact_info && r2->contact_info) ||
      (r1->contact_info && r2->contact_info &&
       strcasecmp(r1->contact_info, r2->contact_info)) ||
      r1->is_hibernating != r2->is_hibernating ||
      cmp_addr_policies(r1->exit_policy, r2->exit_policy))
    return 0;
  if ((r1->declared_family == NULL) != (r2->declared_family == NULL))
    return 0;
  if (r1->declared_family && r2->declared_family) {
    int i, n;
    if (smartlist_len(r1->declared_family)!=smartlist_len(r2->declared_family))
      return 0;
    n = smartlist_len(r1->declared_family);
    for (i=0; i < n; ++i) {
      if (strcasecmp(smartlist_get(r1->declared_family, i),
                     smartlist_get(r2->declared_family, i)))
        return 0;
    }
  }

  /* Did bandwidth change a lot? */
  if ((r1->bandwidthcapacity < r2->bandwidthcapacity/2) ||
      (r2->bandwidthcapacity < r1->bandwidthcapacity/2))
    return 0;

  /* Did the bandwidthrate or bandwidthburst change? */
  if ((r1->bandwidthrate != r2->bandwidthrate) ||
      (r1->bandwidthburst != r2->bandwidthburst))
    return 0;

  /* Did more than 12 hours pass? */
  if (r1->cache_info.published_on + ROUTER_MAX_COSMETIC_TIME_DIFFERENCE
      < r2->cache_info.published_on)
    return 0;

  /* Did uptime fail to increase by approximately the amount we would think,
   * give or take some slop? */
  r1pub = r1->cache_info.published_on;
  r2pub = r2->cache_info.published_on;
  time_difference = labs(r2->uptime - (r1->uptime + (r2pub - r1pub)));
  if (time_difference > ROUTER_ALLOW_UPTIME_DRIFT &&
      time_difference > r1->uptime * .05 &&
      time_difference > r2->uptime * .05)
    return 0;

  /* Otherwise, the difference is cosmetic. */
  return 1;
}

/** Check whether <b>ri</b> (a.k.a. sd) is a router compatible with the
 * extrainfo document
 * <b>ei</b>.  If no router is compatible with <b>ei</b>, <b>ei</b> should be
 * dropped.  Return 0 for "compatible", return 1 for "reject, and inform
 * whoever uploaded <b>ei</b>, and return -1 for "reject silently.".  If
 * <b>msg</b> is present, set *<b>msg</b> to a description of the
 * incompatibility (if any).
 **/
int
routerinfo_incompatible_with_extrainfo(const routerinfo_t *ri,
                                       extrainfo_t *ei,
                                       signed_descriptor_t *sd,
                                       const char **msg)
{
  int digest_matches, r=1;
  tor_assert(ri);
  tor_assert(ei);
  if (!sd)
    sd = (signed_descriptor_t*)&ri->cache_info;

  if (ei->bad_sig) {
    if (msg) *msg = "Extrainfo signature was bad, or signed with wrong key.";
    return 1;
  }

  digest_matches = tor_memeq(ei->cache_info.signed_descriptor_digest,
                           sd->extra_info_digest, DIGEST_LEN);

  /* The identity must match exactly to have been generated at the same time
   * by the same router. */
  if (tor_memneq(ri->cache_info.identity_digest,
                 ei->cache_info.identity_digest,
                 DIGEST_LEN)) {
    if (msg) *msg = "Extrainfo nickname or identity did not match routerinfo";
    goto err; /* different servers */
  }

  if (ei->pending_sig) {
    char signed_digest[128];
    if (crypto_pk_public_checksig(ri->identity_pkey,
                       signed_digest, sizeof(signed_digest),
                       ei->pending_sig, ei->pending_sig_len) != DIGEST_LEN ||
        tor_memneq(signed_digest, ei->cache_info.signed_descriptor_digest,
               DIGEST_LEN)) {
      ei->bad_sig = 1;
      tor_free(ei->pending_sig);
      if (msg) *msg = "Extrainfo signature bad, or signed with wrong key";
      goto err; /* Bad signature, or no match. */
    }

    ei->cache_info.send_unencrypted = ri->cache_info.send_unencrypted;
    tor_free(ei->pending_sig);
  }

  if (ei->cache_info.published_on < sd->published_on) {
    if (msg) *msg = "Extrainfo published time did not match routerdesc";
    goto err;
  } else if (ei->cache_info.published_on > sd->published_on) {
    if (msg) *msg = "Extrainfo published time did not match routerdesc";
    r = -1;
    goto err;
  }

  if (!digest_matches) {
    if (msg) *msg = "Extrainfo digest did not match value from routerdesc";
    goto err; /* Digest doesn't match declared value. */
  }

  return 0;
 err:
  if (digest_matches) {
    /* This signature was okay, and the digest was right: This is indeed the
     * corresponding extrainfo.  But insanely, it doesn't match the routerinfo
     * that lists it.  Don't try to fetch this one again. */
    sd->extrainfo_is_bogus = 1;
  }

  return r;
}

/** Assert that the internal representation of <b>rl</b> is
 * self-consistent. */
void
routerlist_assert_ok(const routerlist_t *rl)
{
  routerinfo_t *r2;
  signed_descriptor_t *sd2;
  if (!rl)
    return;
  SMARTLIST_FOREACH_BEGIN(rl->routers, routerinfo_t *, r) {
    r2 = rimap_get(rl->identity_map, r->cache_info.identity_digest);
    tor_assert(r == r2);
    sd2 = sdmap_get(rl->desc_digest_map,
                    r->cache_info.signed_descriptor_digest);
    tor_assert(&(r->cache_info) == sd2);
    tor_assert(r->cache_info.routerlist_index == r_sl_idx);
    /* XXXX
     *
     *   Hoo boy.  We need to fix this one, and the fix is a bit tricky, so
     * commenting this out is just a band-aid.
     *
     *   The problem is that, although well-behaved router descriptors
     * should never have the same value for their extra_info_digest, it's
     * possible for ill-behaved routers to claim whatever they like there.
     *
     *   The real answer is to trash desc_by_eid_map and instead have
     * something that indicates for a given extra-info digest we want,
     * what its download status is.  We'll do that as a part of routerlist
     * refactoring once consensus directories are in.  For now,
     * this rep violation is probably harmless: an adversary can make us
     * reset our retry count for an extrainfo, but that's not the end
     * of the world.  Changing the representation in 0.2.0.x would just
     * destabilize the codebase.
    if (!tor_digest_is_zero(r->cache_info.extra_info_digest)) {
      signed_descriptor_t *sd3 =
        sdmap_get(rl->desc_by_eid_map, r->cache_info.extra_info_digest);
      tor_assert(sd3 == &(r->cache_info));
    }
    */
  } SMARTLIST_FOREACH_END(r);
  SMARTLIST_FOREACH_BEGIN(rl->old_routers, signed_descriptor_t *, sd) {
    r2 = rimap_get(rl->identity_map, sd->identity_digest);
    tor_assert(!r2 || sd != &(r2->cache_info));
    sd2 = sdmap_get(rl->desc_digest_map, sd->signed_descriptor_digest);
    tor_assert(sd == sd2);
    tor_assert(sd->routerlist_index == sd_sl_idx);
    /* XXXX see above.
    if (!tor_digest_is_zero(sd->extra_info_digest)) {
      signed_descriptor_t *sd3 =
        sdmap_get(rl->desc_by_eid_map, sd->extra_info_digest);
      tor_assert(sd3 == sd);
    }
    */
  } SMARTLIST_FOREACH_END(sd);

  RIMAP_FOREACH(rl->identity_map, d, r) {
    tor_assert(tor_memeq(r->cache_info.identity_digest, d, DIGEST_LEN));
  } DIGESTMAP_FOREACH_END;
  SDMAP_FOREACH(rl->desc_digest_map, d, sd) {
    tor_assert(tor_memeq(sd->signed_descriptor_digest, d, DIGEST_LEN));
  } DIGESTMAP_FOREACH_END;
  SDMAP_FOREACH(rl->desc_by_eid_map, d, sd) {
    tor_assert(!tor_digest_is_zero(d));
    tor_assert(sd);
    tor_assert(tor_memeq(sd->extra_info_digest, d, DIGEST_LEN));
  } DIGESTMAP_FOREACH_END;
  EIMAP_FOREACH(rl->extra_info_map, d, ei) {
    signed_descriptor_t *sd;
    tor_assert(tor_memeq(ei->cache_info.signed_descriptor_digest,
                       d, DIGEST_LEN));
    sd = sdmap_get(rl->desc_by_eid_map,
                   ei->cache_info.signed_descriptor_digest);
    // tor_assert(sd); // XXXX see above
    if (sd) {
      tor_assert(tor_memeq(ei->cache_info.signed_descriptor_digest,
                         sd->extra_info_digest, DIGEST_LEN));
    }
  } DIGESTMAP_FOREACH_END;
}

/** Allocate and return a new string representing the contact info
 * and platform string for <b>router</b>,
 * surrounded by quotes and using standard C escapes.
 *
 * THIS FUNCTION IS NOT REENTRANT.  Don't call it from outside the main
 * thread.  Also, each call invalidates the last-returned value, so don't
 * try log_warn(LD_GENERAL, "%s %s", esc_router_info(a), esc_router_info(b));
 *
 * If <b>router</b> is NULL, it just frees its internal memory and returns.
 */
const char *
esc_router_info(const routerinfo_t *router)
{
  static char *info=NULL;
  char *esc_contact, *esc_platform;
  tor_free(info);

  if (!router)
    return NULL; /* we're exiting; just free the memory we use */

  esc_contact = esc_for_log(router->contact_info);
  esc_platform = esc_for_log(router->platform);

  tor_asprintf(&info, "Contact %s, Platform %s", esc_contact, esc_platform);
  tor_free(esc_contact);
  tor_free(esc_platform);

  return info;
}

/** Helper for sorting: compare two routerinfos by their identity
 * digest. */
static int
compare_routerinfo_by_id_digest_(const void **a, const void **b)
{
  routerinfo_t *first = *(routerinfo_t **)a, *second = *(routerinfo_t **)b;
  return fast_memcmp(first->cache_info.identity_digest,
                second->cache_info.identity_digest,
                DIGEST_LEN);
}

/** Sort a list of routerinfo_t in ascending order of identity digest. */
void
routers_sort_by_identity(smartlist_t *routers)
{
  smartlist_sort(routers, compare_routerinfo_by_id_digest_);
}

/** Called when we change a node set, or when we reload the geoip IPv4 list:
 * recompute all country info in all configuration node sets and in the
 * routerlist. */
void
refresh_all_country_info(void)
{
  const or_options_t *options = get_options();

  if (options->EntryNodes)
    routerset_refresh_countries(options->EntryNodes);
  if (options->ExitNodes)
    routerset_refresh_countries(options->ExitNodes);
  if (options->ExcludeNodes)
    routerset_refresh_countries(options->ExcludeNodes);
  if (options->ExcludeExitNodes)
    routerset_refresh_countries(options->ExcludeExitNodes);
  if (options->ExcludeExitNodesUnion_)
    routerset_refresh_countries(options->ExcludeExitNodesUnion_);

  nodelist_refresh_countries();
}

/** Determine the routers that are responsible for <b>id</b> (binary) and
 * add pointers to those routers' routerstatus_t to <b>responsible_dirs</b>.
 * Return -1 if we're returning an empty smartlist, else return 0.
 */
int
hid_serv_get_responsible_directories(smartlist_t *responsible_dirs,
                                     const char *id)
{
  int start, found, n_added = 0, i;
  networkstatus_t *c = networkstatus_get_latest_consensus();
  if (!c || !smartlist_len(c->routerstatus_list)) {
    log_warn(LD_REND, "We don't have a consensus, so we can't perform v2 "
             "rendezvous operations.");
    return -1;
  }
  tor_assert(id);
  start = networkstatus_vote_find_entry_idx(c, id, &found);
  if (start == smartlist_len(c->routerstatus_list)) start = 0;
  i = start;
  do {
    routerstatus_t *r = smartlist_get(c->routerstatus_list, i);
    if (r->is_hs_dir) {
      smartlist_add(responsible_dirs, r);
      if (++n_added == REND_NUMBER_OF_CONSECUTIVE_REPLICAS)
        return 0;
    }
    if (++i == smartlist_len(c->routerstatus_list))
      i = 0;
  } while (i != start);

  /* Even though we don't have the desired number of hidden service
   * directories, be happy if we got any. */
  return smartlist_len(responsible_dirs) ? 0 : -1;
}

/** Return true if this node is currently acting as hidden service
 * directory, false otherwise. */
int
hid_serv_acting_as_directory(void)
{
  const routerinfo_t *me = router_get_my_routerinfo();
  if (!me)
    return 0;
  if (!get_options()->HidServDirectoryV2) {
    log_info(LD_REND, "We are not acting as hidden service directory, "
                      "because we have not been configured as such.");
    return 0;
  }
  return 1;
}

/** Return true if this node is responsible for storing the descriptor ID
 * in <b>query</b> and false otherwise. */
int
hid_serv_responsible_for_desc_id(const char *query)
{
  const routerinfo_t *me;
  routerstatus_t *last_rs;
  const char *my_id, *last_id;
  int result;
  smartlist_t *responsible;
  if (!hid_serv_acting_as_directory())
    return 0;
  if (!(me = router_get_my_routerinfo()))
    return 0; /* This is redundant, but let's be paranoid. */
  my_id = me->cache_info.identity_digest;
  responsible = smartlist_new();
  if (hid_serv_get_responsible_directories(responsible, query) < 0) {
    smartlist_free(responsible);
    return 0;
  }
  last_rs = smartlist_get(responsible, smartlist_len(responsible)-1);
  last_id = last_rs->identity_digest;
  result = rend_id_is_in_interval(my_id, query, last_id);
  smartlist_free(responsible);
  return result;
}

