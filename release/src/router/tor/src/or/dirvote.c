/* Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#define DIRVOTE_PRIVATE
#include "or.h"
#include "config.h"
#include "directory.h"
#include "dirserv.h"
#include "dirvote.h"
#include "microdesc.h"
#include "networkstatus.h"
#include "policies.h"
#include "rephist.h"
#include "router.h"
#include "routerlist.h"
#include "routerparse.h"

/**
 * \file dirvote.c
 * \brief Functions to compute directory consensus, and schedule voting.
 **/

/** A consensus that we have built and are appending signatures to.  Once it's
 * time to publish it, it will become an active consensus if it accumulates
 * enough signatures. */
typedef struct pending_consensus_t {
  /** The body of the consensus that we're currently building.  Once we
   * have it built, it goes into dirserv.c */
  char *body;
  /** The parsed in-progress consensus document. */
  networkstatus_t *consensus;
} pending_consensus_t;

/* DOCDOC dirvote_add_signatures_to_all_pending_consensuses */
static int dirvote_add_signatures_to_all_pending_consensuses(
                       const char *detached_signatures_body,
                       const char *source,
                       const char **msg_out);
static int dirvote_add_signatures_to_pending_consensus(
                       pending_consensus_t *pc,
                       ns_detached_signatures_t *sigs,
                       const char *source,
                       int severity,
                       const char **msg_out);
static char *list_v3_auth_ids(void);
static void dirvote_fetch_missing_votes(void);
static void dirvote_fetch_missing_signatures(void);
static int dirvote_perform_vote(void);
static void dirvote_clear_votes(int all_votes);
static int dirvote_compute_consensuses(void);
static int dirvote_publish_consensus(void);
static char *make_consensus_method_list(int low, int high, const char *sep);

/* =====
 * Voting
 * =====*/

/** Return a new string containing the string representation of the vote in
 * <b>v3_ns</b>, signed with our v3 signing key <b>private_signing_key</b>.
 * For v3 authorities. */
STATIC char *
format_networkstatus_vote(crypto_pk_t *private_signing_key,
                          networkstatus_t *v3_ns)
{
  smartlist_t *chunks;
  const char *client_versions = NULL, *server_versions = NULL;
  char fingerprint[FINGERPRINT_LEN+1];
  char digest[DIGEST_LEN];
  uint32_t addr;
  char *client_versions_line = NULL, *server_versions_line = NULL;
  networkstatus_voter_info_t *voter;
  char *status = NULL;

  tor_assert(private_signing_key);
  tor_assert(v3_ns->type == NS_TYPE_VOTE || v3_ns->type == NS_TYPE_OPINION);

  voter = smartlist_get(v3_ns->voters, 0);

  addr = voter->addr;

  base16_encode(fingerprint, sizeof(fingerprint),
                v3_ns->cert->cache_info.identity_digest, DIGEST_LEN);
  client_versions = v3_ns->client_versions;
  server_versions = v3_ns->server_versions;

  if (client_versions) {
    tor_asprintf(&client_versions_line, "client-versions %s\n",
                 client_versions);
  } else {
    client_versions_line = tor_strdup("");
  }
  if (server_versions) {
    tor_asprintf(&server_versions_line, "server-versions %s\n",
                 server_versions);
  } else {
    server_versions_line = tor_strdup("");
  }

  chunks = smartlist_new();
  {
    char published[ISO_TIME_LEN+1];
    char va[ISO_TIME_LEN+1];
    char fu[ISO_TIME_LEN+1];
    char vu[ISO_TIME_LEN+1];
    char *flags = smartlist_join_strings(v3_ns->known_flags, " ", 0, NULL);
    /* XXXX Abstraction violation: should be pulling a field out of v3_ns.*/
    char *flag_thresholds = dirserv_get_flag_thresholds_line();
    char *params;
    authority_cert_t *cert = v3_ns->cert;
    char *methods =
      make_consensus_method_list(1, MAX_SUPPORTED_CONSENSUS_METHOD, " ");
    format_iso_time(published, v3_ns->published);
    format_iso_time(va, v3_ns->valid_after);
    format_iso_time(fu, v3_ns->fresh_until);
    format_iso_time(vu, v3_ns->valid_until);

    if (v3_ns->net_params)
      params = smartlist_join_strings(v3_ns->net_params, " ", 0, NULL);
    else
      params = tor_strdup("");

    tor_assert(cert);
    smartlist_add_asprintf(chunks,
                 "network-status-version 3\n"
                 "vote-status %s\n"
                 "consensus-methods %s\n"
                 "published %s\n"
                 "valid-after %s\n"
                 "fresh-until %s\n"
                 "valid-until %s\n"
                 "voting-delay %d %d\n"
                 "%s%s" /* versions */
                 "known-flags %s\n"
                 "flag-thresholds %s\n"
                 "params %s\n"
                 "dir-source %s %s %s %s %d %d\n"
                 "contact %s\n",
                 v3_ns->type == NS_TYPE_VOTE ? "vote" : "opinion",
                 methods,
                 published, va, fu, vu,
                 v3_ns->vote_seconds, v3_ns->dist_seconds,
                 client_versions_line,
                 server_versions_line,
                 flags,
                 flag_thresholds,
                 params,
                 voter->nickname, fingerprint, voter->address,
                 fmt_addr32(addr), voter->dir_port, voter->or_port,
                 voter->contact);

    tor_free(params);
    tor_free(flags);
    tor_free(flag_thresholds);
    tor_free(methods);

    if (!tor_digest_is_zero(voter->legacy_id_digest)) {
      char fpbuf[HEX_DIGEST_LEN+1];
      base16_encode(fpbuf, sizeof(fpbuf), voter->legacy_id_digest, DIGEST_LEN);
      smartlist_add_asprintf(chunks, "legacy-dir-key %s\n", fpbuf);
    }

    smartlist_add(chunks, tor_strndup(cert->cache_info.signed_descriptor_body,
                                      cert->cache_info.signed_descriptor_len));
  }

  SMARTLIST_FOREACH_BEGIN(v3_ns->routerstatus_list, vote_routerstatus_t *,
                          vrs) {
    char *rsf;
    vote_microdesc_hash_t *h;
    rsf = routerstatus_format_entry(&vrs->status,
                                    vrs->version, NS_V3_VOTE, vrs);
    if (rsf)
      smartlist_add(chunks, rsf);

    for (h = vrs->microdesc; h; h = h->next) {
      smartlist_add(chunks, tor_strdup(h->microdesc_hash_line));
    }
  } SMARTLIST_FOREACH_END(vrs);

  smartlist_add(chunks, tor_strdup("directory-footer\n"));

  /* The digest includes everything up through the space after
   * directory-signature.  (Yuck.) */
  crypto_digest_smartlist(digest, DIGEST_LEN, chunks,
                          "directory-signature ", DIGEST_SHA1);

  {
    char signing_key_fingerprint[FINGERPRINT_LEN+1];
    if (crypto_pk_get_fingerprint(private_signing_key,
                                  signing_key_fingerprint, 0)<0) {
      log_warn(LD_BUG, "Unable to get fingerprint for signing key");
      goto err;
    }

    smartlist_add_asprintf(chunks, "directory-signature %s %s\n", fingerprint,
                           signing_key_fingerprint);
  }

  note_crypto_pk_op(SIGN_DIR);
  {
    char *sig = router_get_dirobj_signature(digest, DIGEST_LEN,
                                            private_signing_key);
    if (!sig) {
      log_warn(LD_BUG, "Unable to sign networkstatus vote.");
      goto err;
    }
    smartlist_add(chunks, sig);
  }

  status = smartlist_join_strings(chunks, "", 0, NULL);

  {
    networkstatus_t *v;
    if (!(v = networkstatus_parse_vote_from_string(status, NULL,
                                                   v3_ns->type))) {
      log_err(LD_BUG,"Generated a networkstatus %s we couldn't parse: "
              "<<%s>>",
              v3_ns->type == NS_TYPE_VOTE ? "vote" : "opinion", status);
      goto err;
    }
    networkstatus_vote_free(v);
  }

  goto done;

 err:
  tor_free(status);
 done:
  tor_free(client_versions_line);
  tor_free(server_versions_line);
  if (chunks) {
    SMARTLIST_FOREACH(chunks, char *, cp, tor_free(cp));
    smartlist_free(chunks);
  }
  return status;
}

/* =====
 * Consensus generation
 * ===== */

/** Given a vote <b>vote</b> (not a consensus!), return its associated
 * networkstatus_voter_info_t. */
static networkstatus_voter_info_t *
get_voter(const networkstatus_t *vote)
{
  tor_assert(vote);
  tor_assert(vote->type == NS_TYPE_VOTE);
  tor_assert(vote->voters);
  tor_assert(smartlist_len(vote->voters) == 1);
  return smartlist_get(vote->voters, 0);
}

/** Return the signature made by <b>voter</b> using the algorithm
 * <b>alg</b>, or NULL if none is found. */
document_signature_t *
voter_get_sig_by_algorithm(const networkstatus_voter_info_t *voter,
                           digest_algorithm_t alg)
{
  if (!voter->sigs)
    return NULL;
  SMARTLIST_FOREACH(voter->sigs, document_signature_t *, sig,
    if (sig->alg == alg)
      return sig);
  return NULL;
}

/** Temporary structure used in constructing a list of dir-source entries
 * for a consensus.  One of these is generated for every vote, and one more
 * for every legacy key in each vote. */
typedef struct dir_src_ent_t {
  networkstatus_t *v;
  const char *digest;
  int is_legacy;
} dir_src_ent_t;

/** Helper for sorting networkstatus_t votes (not consensuses) by the
 * hash of their voters' identity digests. */
static int
compare_votes_by_authority_id_(const void **_a, const void **_b)
{
  const networkstatus_t *a = *_a, *b = *_b;
  return fast_memcmp(get_voter(a)->identity_digest,
                get_voter(b)->identity_digest, DIGEST_LEN);
}

/** Helper: Compare the dir_src_ent_ts in *<b>_a</b> and *<b>_b</b> by
 * their identity digests, and return -1, 0, or 1 depending on their
 * ordering */
static int
compare_dir_src_ents_by_authority_id_(const void **_a, const void **_b)
{
  const dir_src_ent_t *a = *_a, *b = *_b;
  const networkstatus_voter_info_t *a_v = get_voter(a->v),
    *b_v = get_voter(b->v);
  const char *a_id, *b_id;
  a_id = a->is_legacy ? a_v->legacy_id_digest : a_v->identity_digest;
  b_id = b->is_legacy ? b_v->legacy_id_digest : b_v->identity_digest;

  return fast_memcmp(a_id, b_id, DIGEST_LEN);
}

/** Given a sorted list of strings <b>in</b>, add every member to <b>out</b>
 * that occurs more than <b>min</b> times. */
static void
get_frequent_members(smartlist_t *out, smartlist_t *in, int min)
{
  char *cur = NULL;
  int count = 0;
  SMARTLIST_FOREACH_BEGIN(in, char *, cp) {
    if (cur && !strcmp(cp, cur)) {
      ++count;
    } else {
      if (count > min)
        smartlist_add(out, cur);
      cur = cp;
      count = 1;
    }
  } SMARTLIST_FOREACH_END(cp);
  if (count > min)
    smartlist_add(out, cur);
}

/** Given a sorted list of strings <b>lst</b>, return the member that appears
 * most.  Break ties in favor of later-occurring members. */
#define get_most_frequent_member(lst)           \
  smartlist_get_most_frequent_string(lst)

/** Return 0 if and only if <b>a</b> and <b>b</b> are routerstatuses
 * that come from the same routerinfo, with the same derived elements.
 */
static int
compare_vote_rs(const vote_routerstatus_t *a, const vote_routerstatus_t *b)
{
  int r;
  tor_assert(a);
  tor_assert(b);

  if ((r = fast_memcmp(a->status.identity_digest, b->status.identity_digest,
                  DIGEST_LEN)))
    return r;
  if ((r = fast_memcmp(a->status.descriptor_digest,
                       b->status.descriptor_digest,
                       DIGEST_LEN)))
    return r;
  if ((r = (int)(b->status.published_on - a->status.published_on)))
    return r;
  if ((r = strcmp(b->status.nickname, a->status.nickname)))
    return r;
  if ((r = (((int)b->status.addr) - ((int)a->status.addr))))
    return r;
  if ((r = (((int)b->status.or_port) - ((int)a->status.or_port))))
    return r;
  if ((r = (((int)b->status.dir_port) - ((int)a->status.dir_port))))
    return r;
  return 0;
}

/** Helper for sorting routerlists based on compare_vote_rs. */
static int
compare_vote_rs_(const void **_a, const void **_b)
{
  const vote_routerstatus_t *a = *_a, *b = *_b;
  return compare_vote_rs(a,b);
}

/** Helper for sorting OR ports. */
static int
compare_orports_(const void **_a, const void **_b)
{
  const tor_addr_port_t *a = *_a, *b = *_b;
  int r;

  if ((r = tor_addr_compare(&a->addr, &b->addr, CMP_EXACT)))
    return r;
  if ((r = (((int) b->port) - ((int) a->port))))
    return r;

  return 0;
}

/** Given a list of vote_routerstatus_t, all for the same router identity,
 * return whichever is most frequent, breaking ties in favor of more
 * recently published vote_routerstatus_t and in case of ties there,
 * in favor of smaller descriptor digest.
 */
static vote_routerstatus_t *
compute_routerstatus_consensus(smartlist_t *votes, int consensus_method,
                               char *microdesc_digest256_out,
                               tor_addr_port_t *best_alt_orport_out)
{
  vote_routerstatus_t *most = NULL, *cur = NULL;
  int most_n = 0, cur_n = 0;
  time_t most_published = 0;

  /* compare_vote_rs_() sorts the items by identity digest (all the same),
   * then by SD digest.  That way, if we have a tie that the published_on
   * date cannot tie, we use the descriptor with the smaller digest.
   */
  smartlist_sort(votes, compare_vote_rs_);
  SMARTLIST_FOREACH_BEGIN(votes, vote_routerstatus_t *, rs) {
    if (cur && !compare_vote_rs(cur, rs)) {
      ++cur_n;
    } else {
      if (cur && (cur_n > most_n ||
                  (cur_n == most_n &&
                   cur->status.published_on > most_published))) {
        most = cur;
        most_n = cur_n;
        most_published = cur->status.published_on;
      }
      cur_n = 1;
      cur = rs;
    }
  } SMARTLIST_FOREACH_END(rs);

  if (cur_n > most_n ||
      (cur && cur_n == most_n && cur->status.published_on > most_published)) {
    most = cur;
    most_n = cur_n;
    most_published = cur->status.published_on;
  }

  tor_assert(most);

  /* If we're producing "a" lines, vote on potential alternative (sets
   * of) OR port(s) in the winning routerstatuses.
   *
   * XXX prop186 There's at most one alternative OR port (_the_ IPv6
   * port) for now. */
  if (consensus_method >= MIN_METHOD_FOR_A_LINES && best_alt_orport_out) {
    smartlist_t *alt_orports = smartlist_new();
    const tor_addr_port_t *most_alt_orport = NULL;

    SMARTLIST_FOREACH_BEGIN(votes, vote_routerstatus_t *, rs) {
      tor_assert(rs);
      if (compare_vote_rs(most, rs) == 0 &&
          !tor_addr_is_null(&rs->status.ipv6_addr)
          && rs->status.ipv6_orport) {
        smartlist_add(alt_orports, tor_addr_port_new(&rs->status.ipv6_addr,
                                                     rs->status.ipv6_orport));
      }
    } SMARTLIST_FOREACH_END(rs);

    smartlist_sort(alt_orports, compare_orports_);
    most_alt_orport = smartlist_get_most_frequent(alt_orports,
                                                  compare_orports_);
    if (most_alt_orport) {
      memcpy(best_alt_orport_out, most_alt_orport, sizeof(tor_addr_port_t));
      log_debug(LD_DIR, "\"a\" line winner for %s is %s",
                most->status.nickname,
                fmt_addrport(&most_alt_orport->addr, most_alt_orport->port));
    }

    SMARTLIST_FOREACH(alt_orports, tor_addr_port_t *, ap, tor_free(ap));
    smartlist_free(alt_orports);
  }

  if (consensus_method >= MIN_METHOD_FOR_MICRODESC &&
      microdesc_digest256_out) {
    smartlist_t *digests = smartlist_new();
    const char *best_microdesc_digest;
    SMARTLIST_FOREACH_BEGIN(votes, vote_routerstatus_t *, rs) {
        char d[DIGEST256_LEN];
        if (compare_vote_rs(rs, most))
          continue;
        if (!vote_routerstatus_find_microdesc_hash(d, rs, consensus_method,
                                                   DIGEST_SHA256))
          smartlist_add(digests, tor_memdup(d, sizeof(d)));
    } SMARTLIST_FOREACH_END(rs);
    smartlist_sort_digests256(digests);
    best_microdesc_digest = smartlist_get_most_frequent_digest256(digests);
    if (best_microdesc_digest)
      memcpy(microdesc_digest256_out, best_microdesc_digest, DIGEST256_LEN);
    SMARTLIST_FOREACH(digests, char *, cp, tor_free(cp));
    smartlist_free(digests);
  }

  return most;
}

/** Sorting helper: compare two strings based on their values as base-ten
 * positive integers. (Non-integers are treated as prior to all integers, and
 * compared lexically.) */
static int
cmp_int_strings_(const void **_a, const void **_b)
{
  const char *a = *_a, *b = *_b;
  int ai = (int)tor_parse_long(a, 10, 1, INT_MAX, NULL, NULL);
  int bi = (int)tor_parse_long(b, 10, 1, INT_MAX, NULL, NULL);
  if (ai<bi) {
    return -1;
  } else if (ai==bi) {
    if (ai == 0) /* Parsing failed. */
      return strcmp(a, b);
    return 0;
  } else {
    return 1;
  }
}

/** Given a list of networkstatus_t votes, determine and return the number of
 * the highest consensus method that is supported by 2/3 of the voters. */
static int
compute_consensus_method(smartlist_t *votes)
{
  smartlist_t *all_methods = smartlist_new();
  smartlist_t *acceptable_methods = smartlist_new();
  smartlist_t *tmp = smartlist_new();
  int min = (smartlist_len(votes) * 2) / 3;
  int n_ok;
  int result;
  SMARTLIST_FOREACH(votes, networkstatus_t *, vote,
  {
    tor_assert(vote->supported_methods);
    smartlist_add_all(tmp, vote->supported_methods);
    smartlist_sort(tmp, cmp_int_strings_);
    smartlist_uniq(tmp, cmp_int_strings_, NULL);
    smartlist_add_all(all_methods, tmp);
    smartlist_clear(tmp);
  });

  smartlist_sort(all_methods, cmp_int_strings_);
  get_frequent_members(acceptable_methods, all_methods, min);
  n_ok = smartlist_len(acceptable_methods);
  if (n_ok) {
    const char *best = smartlist_get(acceptable_methods, n_ok-1);
    result = (int)tor_parse_long(best, 10, 1, INT_MAX, NULL, NULL);
  } else {
    result = 1;
  }
  smartlist_free(tmp);
  smartlist_free(all_methods);
  smartlist_free(acceptable_methods);
  return result;
}

/** Return true iff <b>method</b> is a consensus method that we support. */
static int
consensus_method_is_supported(int method)
{
  return (method >= 1) && (method <= MAX_SUPPORTED_CONSENSUS_METHOD);
}

/** Return a newly allocated string holding the numbers between low and high
 * (inclusive) that are supported consensus methods. */
static char *
make_consensus_method_list(int low, int high, const char *separator)
{
  char *list;

  int i;
  smartlist_t *lst;
  lst = smartlist_new();
  for (i = low; i <= high; ++i) {
    if (!consensus_method_is_supported(i))
      continue;
    smartlist_add_asprintf(lst, "%d", i);
  }
  list = smartlist_join_strings(lst, separator, 0, NULL);
  tor_assert(list);
  SMARTLIST_FOREACH(lst, char *, cp, tor_free(cp));
  smartlist_free(lst);
  return list;
}

/** Helper: given <b>lst</b>, a list of version strings such that every
 * version appears once for every versioning voter who recommends it, return a
 * newly allocated string holding the resulting client-versions or
 * server-versions list. May change contents of <b>lst</b> */
static char *
compute_consensus_versions_list(smartlist_t *lst, int n_versioning)
{
  int min = n_versioning / 2;
  smartlist_t *good = smartlist_new();
  char *result;
  sort_version_list(lst, 0);
  get_frequent_members(good, lst, min);
  result = smartlist_join_strings(good, ",", 0, NULL);
  smartlist_free(good);
  return result;
}

/** Minimum number of directory authorities voting for a parameter to
 * include it in the consensus, if consensus method 12 or later is to be
 * used. See proposal 178 for details. */
#define MIN_VOTES_FOR_PARAM 3

/** Helper: given a list of valid networkstatus_t, return a new string
 * containing the contents of the consensus network parameter set.
 */
STATIC char *
dirvote_compute_params(smartlist_t *votes, int method, int total_authorities)
{
  int i;
  int32_t *vals;

  int cur_param_len;
  const char *cur_param;
  const char *eq;
  char *result;

  const int n_votes = smartlist_len(votes);
  smartlist_t *output;
  smartlist_t *param_list = smartlist_new();

  /* We require that the parameter lists in the votes are well-formed: that
     is, that their keywords are unique and sorted, and that their values are
     between INT32_MIN and INT32_MAX inclusive.  This should be guaranteed by
     the parsing code. */

  vals = tor_malloc(sizeof(int)*n_votes);

  SMARTLIST_FOREACH_BEGIN(votes, networkstatus_t *, v) {
    if (!v->net_params)
      continue;
    smartlist_add_all(param_list, v->net_params);
  } SMARTLIST_FOREACH_END(v);

  if (smartlist_len(param_list) == 0) {
    tor_free(vals);
    smartlist_free(param_list);
    return NULL;
  }

  smartlist_sort_strings(param_list);
  i = 0;
  cur_param = smartlist_get(param_list, 0);
  eq = strchr(cur_param, '=');
  tor_assert(eq);
  cur_param_len = (int)(eq+1 - cur_param);

  output = smartlist_new();

  SMARTLIST_FOREACH_BEGIN(param_list, const char *, param) {
    const char *next_param;
    int ok=0;
    eq = strchr(param, '=');
    tor_assert(i<n_votes); /* Make sure we prevented vote-stuffing. */
    vals[i++] = (int32_t)
      tor_parse_long(eq+1, 10, INT32_MIN, INT32_MAX, &ok, NULL);
    tor_assert(ok); /* Already checked these when parsing. */

    if (param_sl_idx+1 == smartlist_len(param_list))
      next_param = NULL;
    else
      next_param = smartlist_get(param_list, param_sl_idx+1);
    if (!next_param || strncmp(next_param, param, cur_param_len)) {
      /* We've reached the end of a series. */
      /* Make sure enough authorities voted on this param, unless the
       * the consensus method we use is too old for that. */
      if (method < MIN_METHOD_FOR_MAJORITY_PARAMS ||
          i > total_authorities/2 ||
          i >= MIN_VOTES_FOR_PARAM) {
        int32_t median = median_int32(vals, i);
        char *out_string = tor_malloc(64+cur_param_len);
        memcpy(out_string, param, cur_param_len);
        tor_snprintf(out_string+cur_param_len,64, "%ld", (long)median);
        smartlist_add(output, out_string);
      }

      i = 0;
      if (next_param) {
        eq = strchr(next_param, '=');
        cur_param_len = (int)(eq+1 - next_param);
      }
    }
  } SMARTLIST_FOREACH_END(param);

  result = smartlist_join_strings(output, " ", 0, NULL);
  SMARTLIST_FOREACH(output, char *, cp, tor_free(cp));
  smartlist_free(output);
  smartlist_free(param_list);
  tor_free(vals);
  return result;
}

#define RANGE_CHECK(a,b,c,d,e,f,g,mx) \
       ((a) >= 0 && (a) <= (mx) && (b) >= 0 && (b) <= (mx) && \
        (c) >= 0 && (c) <= (mx) && (d) >= 0 && (d) <= (mx) && \
        (e) >= 0 && (e) <= (mx) && (f) >= 0 && (f) <= (mx) && \
        (g) >= 0 && (g) <= (mx))

#define CHECK_EQ(a, b, margin) \
     ((a)-(b) >= 0 ? (a)-(b) <= (margin) : (b)-(a) <= (margin))

typedef enum {
 BW_WEIGHTS_NO_ERROR = 0,
 BW_WEIGHTS_RANGE_ERROR = 1,
 BW_WEIGHTS_SUMG_ERROR = 2,
 BW_WEIGHTS_SUME_ERROR = 3,
 BW_WEIGHTS_SUMD_ERROR = 4,
 BW_WEIGHTS_BALANCE_MID_ERROR = 5,
 BW_WEIGHTS_BALANCE_EG_ERROR = 6
} bw_weights_error_t;

/**
 * Verify that any weightings satisfy the balanced formulas.
 */
static bw_weights_error_t
networkstatus_check_weights(int64_t Wgg, int64_t Wgd, int64_t Wmg,
                            int64_t Wme, int64_t Wmd, int64_t Wee,
                            int64_t Wed, int64_t scale, int64_t G,
                            int64_t M, int64_t E, int64_t D, int64_t T,
                            int64_t margin, int do_balance) {
  bw_weights_error_t berr = BW_WEIGHTS_NO_ERROR;

  // Wed + Wmd + Wgd == 1
  if (!CHECK_EQ(Wed + Wmd + Wgd, scale, margin)) {
    berr = BW_WEIGHTS_SUMD_ERROR;
    goto out;
  }

  // Wmg + Wgg == 1
  if (!CHECK_EQ(Wmg + Wgg, scale, margin)) {
    berr = BW_WEIGHTS_SUMG_ERROR;
    goto out;
  }

  // Wme + Wee == 1
  if (!CHECK_EQ(Wme + Wee, scale, margin)) {
    berr = BW_WEIGHTS_SUME_ERROR;
    goto out;
  }

  // Verify weights within range 0->1
  if (!RANGE_CHECK(Wgg, Wgd, Wmg, Wme, Wmd, Wed, Wee, scale)) {
    berr = BW_WEIGHTS_RANGE_ERROR;
    goto out;
  }

  if (do_balance) {
    // Wgg*G + Wgd*D == Wee*E + Wed*D, already scaled
    if (!CHECK_EQ(Wgg*G + Wgd*D, Wee*E + Wed*D, (margin*T)/3)) {
      berr = BW_WEIGHTS_BALANCE_EG_ERROR;
      goto out;
    }

    // Wgg*G + Wgd*D == M*scale + Wmd*D + Wme*E + Wmg*G, already scaled
    if (!CHECK_EQ(Wgg*G + Wgd*D, M*scale + Wmd*D + Wme*E + Wmg*G,
                (margin*T)/3)) {
      berr = BW_WEIGHTS_BALANCE_MID_ERROR;
      goto out;
    }
  }

 out:
  if (berr) {
    log_info(LD_DIR,
             "Bw weight mismatch %d. G="I64_FORMAT" M="I64_FORMAT
             " E="I64_FORMAT" D="I64_FORMAT" T="I64_FORMAT
             " Wmd=%d Wme=%d Wmg=%d Wed=%d Wee=%d"
             " Wgd=%d Wgg=%d Wme=%d Wmg=%d",
             berr,
             I64_PRINTF_ARG(G), I64_PRINTF_ARG(M), I64_PRINTF_ARG(E),
             I64_PRINTF_ARG(D), I64_PRINTF_ARG(T),
             (int)Wmd, (int)Wme, (int)Wmg, (int)Wed, (int)Wee,
             (int)Wgd, (int)Wgg, (int)Wme, (int)Wmg);
  }

  return berr;
}

/**
 * This function computes the bandwidth weights for consensus method 10.
 *
 * It returns true if weights could be computed, false otherwise.
 */
static int
networkstatus_compute_bw_weights_v10(smartlist_t *chunks, int64_t G,
                                     int64_t M, int64_t E, int64_t D,
                                     int64_t T, int64_t weight_scale)
{
  bw_weights_error_t berr = 0;
  int64_t Wgg = -1, Wgd = -1;
  int64_t Wmg = -1, Wme = -1, Wmd = -1;
  int64_t Wed = -1, Wee = -1;
  const char *casename;

  if (G <= 0 || M <= 0 || E <= 0 || D <= 0) {
    log_warn(LD_DIR, "Consensus with empty bandwidth: "
                     "G="I64_FORMAT" M="I64_FORMAT" E="I64_FORMAT
                     " D="I64_FORMAT" T="I64_FORMAT,
             I64_PRINTF_ARG(G), I64_PRINTF_ARG(M), I64_PRINTF_ARG(E),
             I64_PRINTF_ARG(D), I64_PRINTF_ARG(T));
    return 0;
  }

  /*
   * Computed from cases in 3.4.3 of dir-spec.txt
   *
   * 1. Neither are scarce
   * 2. Both Guard and Exit are scarce
   *    a. R+D <= S
   *    b. R+D > S
   * 3. One of Guard or Exit is scarce
   *    a. S+D < T/3
   *    b. S+D >= T/3
   */
  if (3*E >= T && 3*G >= T) { // E >= T/3 && G >= T/3
    /* Case 1: Neither are scarce.  */
    casename = "Case 1 (Wgd=Wmd=Wed)";
    Wgd = weight_scale/3;
    Wed = weight_scale/3;
    Wmd = weight_scale/3;
    Wee = (weight_scale*(E+G+M))/(3*E);
    Wme = weight_scale - Wee;
    Wmg = (weight_scale*(2*G-E-M))/(3*G);
    Wgg = weight_scale - Wmg;

    berr = networkstatus_check_weights(Wgg, Wgd, Wmg, Wme, Wmd, Wee, Wed,
                                       weight_scale, G, M, E, D, T, 10, 1);

    if (berr) {
      log_warn(LD_DIR,
             "Bw Weights error %d for %s v10. G="I64_FORMAT" M="I64_FORMAT
             " E="I64_FORMAT" D="I64_FORMAT" T="I64_FORMAT
             " Wmd=%d Wme=%d Wmg=%d Wed=%d Wee=%d"
             " Wgd=%d Wgg=%d Wme=%d Wmg=%d weight_scale=%d",
             berr, casename,
             I64_PRINTF_ARG(G), I64_PRINTF_ARG(M), I64_PRINTF_ARG(E),
             I64_PRINTF_ARG(D), I64_PRINTF_ARG(T),
             (int)Wmd, (int)Wme, (int)Wmg, (int)Wed, (int)Wee,
             (int)Wgd, (int)Wgg, (int)Wme, (int)Wmg, (int)weight_scale);
      return 0;
    }
  } else if (3*E < T && 3*G < T) { // E < T/3 && G < T/3
    int64_t R = MIN(E, G);
    int64_t S = MAX(E, G);
    /*
     * Case 2: Both Guards and Exits are scarce
     * Balance D between E and G, depending upon
     * D capacity and scarcity.
     */
    if (R+D < S) { // Subcase a
      Wgg = weight_scale;
      Wee = weight_scale;
      Wmg = 0;
      Wme = 0;
      Wmd = 0;
      if (E < G) {
        casename = "Case 2a (E scarce)";
        Wed = weight_scale;
        Wgd = 0;
      } else { /* E >= G */
        casename = "Case 2a (G scarce)";
        Wed = 0;
        Wgd = weight_scale;
      }
    } else { // Subcase b: R+D >= S
      casename = "Case 2b1 (Wgg=1, Wmd=Wgd)";
      Wee = (weight_scale*(E - G + M))/E;
      Wed = (weight_scale*(D - 2*E + 4*G - 2*M))/(3*D);
      Wme = (weight_scale*(G-M))/E;
      Wmg = 0;
      Wgg = weight_scale;
      Wmd = (weight_scale - Wed)/2;
      Wgd = (weight_scale - Wed)/2;

      berr = networkstatus_check_weights(Wgg, Wgd, Wmg, Wme, Wmd, Wee, Wed,
                                       weight_scale, G, M, E, D, T, 10, 1);

      if (berr) {
        casename = "Case 2b2 (Wgg=1, Wee=1)";
        Wgg = weight_scale;
        Wee = weight_scale;
        Wed = (weight_scale*(D - 2*E + G + M))/(3*D);
        Wmd = (weight_scale*(D - 2*M + G + E))/(3*D);
        Wme = 0;
        Wmg = 0;

        if (Wmd < 0) { // Can happen if M > T/3
          casename = "Case 2b3 (Wmd=0)";
          Wmd = 0;
          log_warn(LD_DIR,
                   "Too much Middle bandwidth on the network to calculate "
                   "balanced bandwidth-weights. Consider increasing the "
                   "number of Guard nodes by lowering the requirements.");
        }
        Wgd = weight_scale - Wed - Wmd;
        berr = networkstatus_check_weights(Wgg, Wgd, Wmg, Wme, Wmd, Wee,
                  Wed, weight_scale, G, M, E, D, T, 10, 1);
      }
      if (berr != BW_WEIGHTS_NO_ERROR &&
              berr != BW_WEIGHTS_BALANCE_MID_ERROR) {
        log_warn(LD_DIR,
             "Bw Weights error %d for %s v10. G="I64_FORMAT" M="I64_FORMAT
             " E="I64_FORMAT" D="I64_FORMAT" T="I64_FORMAT
             " Wmd=%d Wme=%d Wmg=%d Wed=%d Wee=%d"
             " Wgd=%d Wgg=%d Wme=%d Wmg=%d weight_scale=%d",
             berr, casename,
             I64_PRINTF_ARG(G), I64_PRINTF_ARG(M), I64_PRINTF_ARG(E),
             I64_PRINTF_ARG(D), I64_PRINTF_ARG(T),
             (int)Wmd, (int)Wme, (int)Wmg, (int)Wed, (int)Wee,
             (int)Wgd, (int)Wgg, (int)Wme, (int)Wmg, (int)weight_scale);
        return 0;
      }
    }
  } else { // if (E < T/3 || G < T/3) {
    int64_t S = MIN(E, G);
    // Case 3: Exactly one of Guard or Exit is scarce
    if (!(3*E < T || 3*G < T) || !(3*G >= T || 3*E >= T)) {
      log_warn(LD_BUG,
           "Bw-Weights Case 3 v10 but with G="I64_FORMAT" M="
           I64_FORMAT" E="I64_FORMAT" D="I64_FORMAT" T="I64_FORMAT,
               I64_PRINTF_ARG(G), I64_PRINTF_ARG(M), I64_PRINTF_ARG(E),
               I64_PRINTF_ARG(D), I64_PRINTF_ARG(T));
    }

    if (3*(S+D) < T) { // Subcase a: S+D < T/3
      if (G < E) {
        casename = "Case 3a (G scarce)";
        Wgg = Wgd = weight_scale;
        Wmd = Wed = Wmg = 0;
        // Minor subcase, if E is more scarce than M,
        // keep its bandwidth in place.
        if (E < M) Wme = 0;
        else Wme = (weight_scale*(E-M))/(2*E);
        Wee = weight_scale-Wme;
      } else { // G >= E
        casename = "Case 3a (E scarce)";
        Wee = Wed = weight_scale;
        Wmd = Wgd = Wme = 0;
        // Minor subcase, if G is more scarce than M,
        // keep its bandwidth in place.
        if (G < M) Wmg = 0;
        else Wmg = (weight_scale*(G-M))/(2*G);
        Wgg = weight_scale-Wmg;
      }
    } else { // Subcase b: S+D >= T/3
      // D != 0 because S+D >= T/3
      if (G < E) {
        casename = "Case 3bg (G scarce, Wgg=1, Wmd == Wed)";
        Wgg = weight_scale;
        Wgd = (weight_scale*(D - 2*G + E + M))/(3*D);
        Wmg = 0;
        Wee = (weight_scale*(E+M))/(2*E);
        Wme = weight_scale - Wee;
        Wmd = (weight_scale - Wgd)/2;
        Wed = (weight_scale - Wgd)/2;

        berr = networkstatus_check_weights(Wgg, Wgd, Wmg, Wme, Wmd, Wee,
                    Wed, weight_scale, G, M, E, D, T, 10, 1);
      } else { // G >= E
        casename = "Case 3be (E scarce, Wee=1, Wmd == Wgd)";
        Wee = weight_scale;
        Wed = (weight_scale*(D - 2*E + G + M))/(3*D);
        Wme = 0;
        Wgg = (weight_scale*(G+M))/(2*G);
        Wmg = weight_scale - Wgg;
        Wmd = (weight_scale - Wed)/2;
        Wgd = (weight_scale - Wed)/2;

        berr = networkstatus_check_weights(Wgg, Wgd, Wmg, Wme, Wmd, Wee,
                      Wed, weight_scale, G, M, E, D, T, 10, 1);
      }
      if (berr) {
        log_warn(LD_DIR,
             "Bw Weights error %d for %s v10. G="I64_FORMAT" M="I64_FORMAT
             " E="I64_FORMAT" D="I64_FORMAT" T="I64_FORMAT
             " Wmd=%d Wme=%d Wmg=%d Wed=%d Wee=%d"
             " Wgd=%d Wgg=%d Wme=%d Wmg=%d weight_scale=%d",
             berr, casename,
             I64_PRINTF_ARG(G), I64_PRINTF_ARG(M), I64_PRINTF_ARG(E),
             I64_PRINTF_ARG(D), I64_PRINTF_ARG(T),
             (int)Wmd, (int)Wme, (int)Wmg, (int)Wed, (int)Wee,
             (int)Wgd, (int)Wgg, (int)Wme, (int)Wmg, (int)weight_scale);
        return 0;
      }
    }
  }

  /* We cast down the weights to 32 bit ints on the assumption that
   * weight_scale is ~= 10000. We need to ensure a rogue authority
   * doesn't break this assumption to rig our weights */
  tor_assert(0 < weight_scale && weight_scale <= INT32_MAX);

  /*
   * Provide Wgm=Wgg, Wmm=1, Wem=Wee, Weg=Wed. May later determine
   * that middle nodes need different bandwidth weights for dirport traffic,
   * or that weird exit policies need special weight, or that bridges
   * need special weight.
   *
   * NOTE: This list is sorted.
   */
  smartlist_add_asprintf(chunks,
     "bandwidth-weights Wbd=%d Wbe=%d Wbg=%d Wbm=%d "
     "Wdb=%d "
     "Web=%d Wed=%d Wee=%d Weg=%d Wem=%d "
     "Wgb=%d Wgd=%d Wgg=%d Wgm=%d "
     "Wmb=%d Wmd=%d Wme=%d Wmg=%d Wmm=%d\n",
     (int)Wmd, (int)Wme, (int)Wmg, (int)weight_scale,
     (int)weight_scale,
     (int)weight_scale, (int)Wed, (int)Wee, (int)Wed, (int)Wee,
     (int)weight_scale, (int)Wgd, (int)Wgg, (int)Wgg,
     (int)weight_scale, (int)Wmd, (int)Wme, (int)Wmg, (int)weight_scale);

  log_notice(LD_CIRC, "Computed bandwidth weights for %s with v10: "
             "G="I64_FORMAT" M="I64_FORMAT" E="I64_FORMAT" D="I64_FORMAT
             " T="I64_FORMAT,
             casename,
             I64_PRINTF_ARG(G), I64_PRINTF_ARG(M), I64_PRINTF_ARG(E),
             I64_PRINTF_ARG(D), I64_PRINTF_ARG(T));
  return 1;
}
/**
 * This function computes the bandwidth weights for consensus method 9.
 *
 * It has been obsoleted in favor of consensus method 10.
 */
static void
networkstatus_compute_bw_weights_v9(smartlist_t *chunks, int64_t G, int64_t M,
                              int64_t E, int64_t D, int64_t T,
                              int64_t weight_scale)
{
  int64_t Wgg = -1, Wgd = -1;
  int64_t Wmg = -1, Wme = -1, Wmd = -1;
  int64_t Wed = -1, Wee = -1;
  const char *casename;

  if (G <= 0 || M <= 0 || E <= 0 || D <= 0) {
    log_warn(LD_DIR, "Consensus with empty bandwidth: "
                     "G="I64_FORMAT" M="I64_FORMAT" E="I64_FORMAT
                     " D="I64_FORMAT" T="I64_FORMAT,
             I64_PRINTF_ARG(G), I64_PRINTF_ARG(M), I64_PRINTF_ARG(E),
             I64_PRINTF_ARG(D), I64_PRINTF_ARG(T));
    return;
  }

  /*
   * Computed from cases in 3.4.3 of dir-spec.txt
   *
   * 1. Neither are scarce
   * 2. Both Guard and Exit are scarce
   *    a. R+D <= S
   *    b. R+D > S
   * 3. One of Guard or Exit is scarce
   *    a. S+D < T/3
   *    b. S+D >= T/3
   */
  if (3*E >= T && 3*G >= T) { // E >= T/3 && G >= T/3
    bw_weights_error_t berr = 0;
    /* Case 1: Neither are scarce.
     *
     * Attempt to ensure that we have a large amount of exit bandwidth
     * in the middle position.
     */
    casename = "Case 1 (Wme*E = Wmd*D)";
    Wgg = (weight_scale*(D+E+G+M))/(3*G);
    if (D==0) Wmd = 0;
    else Wmd = (weight_scale*(2*D + 2*E - G - M))/(6*D);
    Wme = (weight_scale*(2*D + 2*E - G - M))/(6*E);
    Wee = (weight_scale*(-2*D + 4*E + G + M))/(6*E);
    Wgd = 0;
    Wmg = weight_scale - Wgg;
    Wed = weight_scale - Wmd;

    berr = networkstatus_check_weights(Wgg, Wgd, Wmg, Wme, Wmd, Wee, Wed,
                                       weight_scale, G, M, E, D, T, 10, 1);

    if (berr) {
      log_warn(LD_DIR, "Bw Weights error %d for case %s. "
                       "G="I64_FORMAT" M="I64_FORMAT" E="I64_FORMAT
                       " D="I64_FORMAT" T="I64_FORMAT,
               berr, casename,
               I64_PRINTF_ARG(G), I64_PRINTF_ARG(M), I64_PRINTF_ARG(E),
               I64_PRINTF_ARG(D), I64_PRINTF_ARG(T));
    }
  } else if (3*E < T && 3*G < T) { // E < T/3 && G < T/3
    int64_t R = MIN(E, G);
    int64_t S = MAX(E, G);
    /*
     * Case 2: Both Guards and Exits are scarce
     * Balance D between E and G, depending upon
     * D capacity and scarcity.
     */
    if (R+D < S) { // Subcase a
      Wgg = weight_scale;
      Wee = weight_scale;
      Wmg = 0;
      Wme = 0;
      Wmd = 0;
      if (E < G) {
        casename = "Case 2a (E scarce)";
        Wed = weight_scale;
        Wgd = 0;
      } else { /* E >= G */
        casename = "Case 2a (G scarce)";
        Wed = 0;
        Wgd = weight_scale;
      }
    } else { // Subcase b: R+D > S
      bw_weights_error_t berr = 0;
      casename = "Case 2b (Wme*E == Wmd*D)";
      if (D != 0) {
        Wgg = weight_scale;
        Wgd = (weight_scale*(D + E - 2*G + M))/(3*D); // T/3 >= G (Ok)
        Wmd = (weight_scale*(D + E + G - 2*M))/(6*D); // T/3 >= M
        Wme = (weight_scale*(D + E + G - 2*M))/(6*E);
        Wee = (weight_scale*(-D + 5*E - G + 2*M))/(6*E); // 2E+M >= T/3
        Wmg = 0;
        Wed = weight_scale - Wgd - Wmd;

        berr = networkstatus_check_weights(Wgg, Wgd, Wmg, Wme, Wmd, Wee, Wed,
                                       weight_scale, G, M, E, D, T, 10, 1);
      }

      if (D == 0 || berr) { // Can happen if M > T/3
        casename = "Case 2b (E=G)";
        Wgg = weight_scale;
        Wee = weight_scale;
        Wmg = 0;
        Wme = 0;
        Wmd = 0;
        if (D == 0) Wgd = 0;
        else Wgd = (weight_scale*(D+E-G))/(2*D);
        Wed = weight_scale - Wgd;
        berr = networkstatus_check_weights(Wgg, Wgd, Wmg, Wme, Wmd, Wee,
                Wed, weight_scale, G, M, E, D, T, 10, 1);
      }
      if (berr != BW_WEIGHTS_NO_ERROR &&
              berr != BW_WEIGHTS_BALANCE_MID_ERROR) {
        log_warn(LD_DIR, "Bw Weights error %d for case %s. "
                         "G="I64_FORMAT" M="I64_FORMAT" E="I64_FORMAT
                         " D="I64_FORMAT" T="I64_FORMAT,
                 berr, casename,
                 I64_PRINTF_ARG(G), I64_PRINTF_ARG(M), I64_PRINTF_ARG(E),
                 I64_PRINTF_ARG(D), I64_PRINTF_ARG(T));
      }
    }
  } else { // if (E < T/3 || G < T/3) {
    int64_t S = MIN(E, G);
    // Case 3: Exactly one of Guard or Exit is scarce
    if (!(3*E < T || 3*G < T) || !(3*G >= T || 3*E >= T)) {
      log_warn(LD_BUG,
           "Bw-Weights Case 3 but with G="I64_FORMAT" M="
           I64_FORMAT" E="I64_FORMAT" D="I64_FORMAT" T="I64_FORMAT,
               I64_PRINTF_ARG(G), I64_PRINTF_ARG(M), I64_PRINTF_ARG(E),
               I64_PRINTF_ARG(D), I64_PRINTF_ARG(T));
    }

    if (3*(S+D) < T) { // Subcase a: S+D < T/3
      if (G < E) {
        casename = "Case 3a (G scarce)";
        Wgg = Wgd = weight_scale;
        Wmd = Wed = Wmg = 0;
        // Minor subcase, if E is more scarce than M,
        // keep its bandwidth in place.
        if (E < M) Wme = 0;
        else Wme = (weight_scale*(E-M))/(2*E);
        Wee = weight_scale-Wme;
      } else { // G >= E
        casename = "Case 3a (E scarce)";
        Wee = Wed = weight_scale;
        Wmd = Wgd = Wme = 0;
        // Minor subcase, if G is more scarce than M,
        // keep its bandwidth in place.
        if (G < M) Wmg = 0;
        else Wmg = (weight_scale*(G-M))/(2*G);
        Wgg = weight_scale-Wmg;
      }
    } else { // Subcase b: S+D >= T/3
      bw_weights_error_t berr = 0;
      // D != 0 because S+D >= T/3
      if (G < E) {
        casename = "Case 3b (G scarce, Wme*E == Wmd*D)";
        Wgd = (weight_scale*(D + E - 2*G + M))/(3*D);
        Wmd = (weight_scale*(D + E + G - 2*M))/(6*D);
        Wme = (weight_scale*(D + E + G - 2*M))/(6*E);
        Wee = (weight_scale*(-D + 5*E - G + 2*M))/(6*E);
        Wgg = weight_scale;
        Wmg = 0;
        Wed = weight_scale - Wgd - Wmd;

        berr = networkstatus_check_weights(Wgg, Wgd, Wmg, Wme, Wmd, Wee,
                    Wed, weight_scale, G, M, E, D, T, 10, 1);
      } else { // G >= E
        casename = "Case 3b (E scarce, Wme*E == Wmd*D)";
        Wgg = (weight_scale*(D + E + G + M))/(3*G);
        Wmd = (weight_scale*(2*D + 2*E - G - M))/(6*D);
        Wme = (weight_scale*(2*D + 2*E - G - M))/(6*E);
        Wee = (weight_scale*(-2*D + 4*E + G + M))/(6*E);
        Wgd = 0;
        Wmg = weight_scale - Wgg;
        Wed = weight_scale - Wmd;

        berr = networkstatus_check_weights(Wgg, Wgd, Wmg, Wme, Wmd, Wee,
                      Wed, weight_scale, G, M, E, D, T, 10, 1);
      }
      if (berr) {
        log_warn(LD_DIR, "Bw Weights error %d for case %s. "
                         "G="I64_FORMAT" M="I64_FORMAT
                         " E="I64_FORMAT" D="I64_FORMAT" T="I64_FORMAT,
                 berr, casename,
               I64_PRINTF_ARG(G), I64_PRINTF_ARG(M), I64_PRINTF_ARG(E),
               I64_PRINTF_ARG(D), I64_PRINTF_ARG(T));
      }
    }
  }

  /* We cast down the weights to 32 bit ints on the assumption that
   * weight_scale is ~= 10000. We need to ensure a rogue authority
   * doesn't break this assumption to rig our weights */
  tor_assert(0 < weight_scale && weight_scale <= INT32_MAX);

  if (Wgg < 0 || Wgg > weight_scale) {
    log_warn(LD_DIR, "Bw %s: Wgg="I64_FORMAT"! G="I64_FORMAT
            " M="I64_FORMAT" E="I64_FORMAT" D="I64_FORMAT
            " T="I64_FORMAT,
             casename, I64_PRINTF_ARG(Wgg),
             I64_PRINTF_ARG(G), I64_PRINTF_ARG(M), I64_PRINTF_ARG(E),
             I64_PRINTF_ARG(D), I64_PRINTF_ARG(T));

    Wgg = MAX(MIN(Wgg, weight_scale), 0);
  }
  if (Wgd < 0 || Wgd > weight_scale) {
    log_warn(LD_DIR, "Bw %s: Wgd="I64_FORMAT"! G="I64_FORMAT
            " M="I64_FORMAT" E="I64_FORMAT" D="I64_FORMAT
            " T="I64_FORMAT,
             casename, I64_PRINTF_ARG(Wgd),
             I64_PRINTF_ARG(G), I64_PRINTF_ARG(M), I64_PRINTF_ARG(E),
             I64_PRINTF_ARG(D), I64_PRINTF_ARG(T));
    Wgd = MAX(MIN(Wgd, weight_scale), 0);
  }
  if (Wmg < 0 || Wmg > weight_scale) {
    log_warn(LD_DIR, "Bw %s: Wmg="I64_FORMAT"! G="I64_FORMAT
            " M="I64_FORMAT" E="I64_FORMAT" D="I64_FORMAT
            " T="I64_FORMAT,
             casename, I64_PRINTF_ARG(Wmg),
             I64_PRINTF_ARG(G), I64_PRINTF_ARG(M), I64_PRINTF_ARG(E),
             I64_PRINTF_ARG(D), I64_PRINTF_ARG(T));
    Wmg = MAX(MIN(Wmg, weight_scale), 0);
  }
  if (Wme < 0 || Wme > weight_scale) {
    log_warn(LD_DIR, "Bw %s: Wme="I64_FORMAT"! G="I64_FORMAT
            " M="I64_FORMAT" E="I64_FORMAT" D="I64_FORMAT
            " T="I64_FORMAT,
             casename, I64_PRINTF_ARG(Wme),
             I64_PRINTF_ARG(G), I64_PRINTF_ARG(M), I64_PRINTF_ARG(E),
             I64_PRINTF_ARG(D), I64_PRINTF_ARG(T));
    Wme = MAX(MIN(Wme, weight_scale), 0);
  }
  if (Wmd < 0 || Wmd > weight_scale) {
    log_warn(LD_DIR, "Bw %s: Wmd="I64_FORMAT"! G="I64_FORMAT
            " M="I64_FORMAT" E="I64_FORMAT" D="I64_FORMAT
            " T="I64_FORMAT,
             casename, I64_PRINTF_ARG(Wmd),
             I64_PRINTF_ARG(G), I64_PRINTF_ARG(M), I64_PRINTF_ARG(E),
             I64_PRINTF_ARG(D), I64_PRINTF_ARG(T));
    Wmd = MAX(MIN(Wmd, weight_scale), 0);
  }
  if (Wee < 0 || Wee > weight_scale) {
    log_warn(LD_DIR, "Bw %s: Wee="I64_FORMAT"! G="I64_FORMAT
            " M="I64_FORMAT" E="I64_FORMAT" D="I64_FORMAT
            " T="I64_FORMAT,
             casename, I64_PRINTF_ARG(Wee),
             I64_PRINTF_ARG(G), I64_PRINTF_ARG(M), I64_PRINTF_ARG(E),
             I64_PRINTF_ARG(D), I64_PRINTF_ARG(T));
    Wee = MAX(MIN(Wee, weight_scale), 0);
  }
  if (Wed < 0 || Wed > weight_scale) {
    log_warn(LD_DIR, "Bw %s: Wed="I64_FORMAT"! G="I64_FORMAT
            " M="I64_FORMAT" E="I64_FORMAT" D="I64_FORMAT
            " T="I64_FORMAT,
             casename, I64_PRINTF_ARG(Wed),
             I64_PRINTF_ARG(G), I64_PRINTF_ARG(M), I64_PRINTF_ARG(E),
             I64_PRINTF_ARG(D), I64_PRINTF_ARG(T));
    Wed = MAX(MIN(Wed, weight_scale), 0);
  }

  // Add consensus weight keywords
  smartlist_add(chunks, tor_strdup("bandwidth-weights "));
  /*
   * Provide Wgm=Wgg, Wmm=1, Wem=Wee, Weg=Wed. May later determine
   * that middle nodes need different bandwidth weights for dirport traffic,
   * or that weird exit policies need special weight, or that bridges
   * need special weight.
   *
   * NOTE: This list is sorted.
   */
  smartlist_add_asprintf(chunks,
     "Wbd=%d Wbe=%d Wbg=%d Wbm=%d "
     "Wdb=%d "
     "Web=%d Wed=%d Wee=%d Weg=%d Wem=%d "
     "Wgb=%d Wgd=%d Wgg=%d Wgm=%d "
     "Wmb=%d Wmd=%d Wme=%d Wmg=%d Wmm=%d\n",
     (int)Wmd, (int)Wme, (int)Wmg, (int)weight_scale,
     (int)weight_scale,
     (int)weight_scale, (int)Wed, (int)Wee, (int)Wed, (int)Wee,
     (int)weight_scale, (int)Wgd, (int)Wgg, (int)Wgg,
     (int)weight_scale, (int)Wmd, (int)Wme, (int)Wmg, (int)weight_scale);

  log_notice(LD_CIRC, "Computed bandwidth weights for %s with v9: "
             "G="I64_FORMAT" M="I64_FORMAT" E="I64_FORMAT" D="I64_FORMAT
             " T="I64_FORMAT,
             casename,
             I64_PRINTF_ARG(G), I64_PRINTF_ARG(M), I64_PRINTF_ARG(E),
             I64_PRINTF_ARG(D), I64_PRINTF_ARG(T));
}

/** Given a list of vote networkstatus_t in <b>votes</b>, our public
 * authority <b>identity_key</b>, our private authority <b>signing_key</b>,
 * and the number of <b>total_authorities</b> that we believe exist in our
 * voting quorum, generate the text of a new v3 consensus vote, and return the
 * value in a newly allocated string.
 *
 * Note: this function DOES NOT check whether the votes are from
 * recognized authorities.   (dirvote_add_vote does that.) */
char *
networkstatus_compute_consensus(smartlist_t *votes,
                                int total_authorities,
                                crypto_pk_t *identity_key,
                                crypto_pk_t *signing_key,
                                const char *legacy_id_key_digest,
                                crypto_pk_t *legacy_signing_key,
                                consensus_flavor_t flavor)
{
  smartlist_t *chunks;
  char *result = NULL;
  int consensus_method;
  time_t valid_after, fresh_until, valid_until;
  int vote_seconds, dist_seconds;
  char *client_versions = NULL, *server_versions = NULL;
  smartlist_t *flags;
  const char *flavor_name;
  uint32_t max_unmeasured_bw_kb = DEFAULT_MAX_UNMEASURED_BW_KB;
  int64_t G=0, M=0, E=0, D=0, T=0; /* For bandwidth weights */
  const routerstatus_format_type_t rs_format =
    flavor == FLAV_NS ? NS_V3_CONSENSUS : NS_V3_CONSENSUS_MICRODESC;
  char *params = NULL;
  int added_weights = 0;
  tor_assert(flavor == FLAV_NS || flavor == FLAV_MICRODESC);
  tor_assert(total_authorities >= smartlist_len(votes));

  flavor_name = networkstatus_get_flavor_name(flavor);

  if (!smartlist_len(votes)) {
    log_warn(LD_DIR, "Can't compute a consensus from no votes.");
    return NULL;
  }
  flags = smartlist_new();

  consensus_method = compute_consensus_method(votes);
  if (consensus_method_is_supported(consensus_method)) {
    log_info(LD_DIR, "Generating consensus using method %d.",
             consensus_method);
  } else {
    log_warn(LD_DIR, "The other authorities will use consensus method %d, "
             "which I don't support.  Maybe I should upgrade!",
             consensus_method);
    consensus_method = 1;
  }

  /* Compute medians of time-related things, and figure out how many
   * routers we might need to talk about. */
  {
    int n_votes = smartlist_len(votes);
    time_t *va_times = tor_malloc(n_votes * sizeof(time_t));
    time_t *fu_times = tor_malloc(n_votes * sizeof(time_t));
    time_t *vu_times = tor_malloc(n_votes * sizeof(time_t));
    int *votesec_list = tor_malloc(n_votes * sizeof(int));
    int *distsec_list = tor_malloc(n_votes * sizeof(int));
    int n_versioning_clients = 0, n_versioning_servers = 0;
    smartlist_t *combined_client_versions = smartlist_new();
    smartlist_t *combined_server_versions = smartlist_new();

    SMARTLIST_FOREACH_BEGIN(votes, networkstatus_t *, v) {
      tor_assert(v->type == NS_TYPE_VOTE);
      va_times[v_sl_idx] = v->valid_after;
      fu_times[v_sl_idx] = v->fresh_until;
      vu_times[v_sl_idx] = v->valid_until;
      votesec_list[v_sl_idx] = v->vote_seconds;
      distsec_list[v_sl_idx] = v->dist_seconds;
      if (v->client_versions) {
        smartlist_t *cv = smartlist_new();
        ++n_versioning_clients;
        smartlist_split_string(cv, v->client_versions, ",",
                               SPLIT_SKIP_SPACE|SPLIT_IGNORE_BLANK, 0);
        sort_version_list(cv, 1);
        smartlist_add_all(combined_client_versions, cv);
        smartlist_free(cv); /* elements get freed later. */
      }
      if (v->server_versions) {
        smartlist_t *sv = smartlist_new();
        ++n_versioning_servers;
        smartlist_split_string(sv, v->server_versions, ",",
                               SPLIT_SKIP_SPACE|SPLIT_IGNORE_BLANK, 0);
        sort_version_list(sv, 1);
        smartlist_add_all(combined_server_versions, sv);
        smartlist_free(sv); /* elements get freed later. */
      }
      SMARTLIST_FOREACH(v->known_flags, const char *, cp,
                        smartlist_add(flags, tor_strdup(cp)));
    } SMARTLIST_FOREACH_END(v);
    valid_after = median_time(va_times, n_votes);
    fresh_until = median_time(fu_times, n_votes);
    valid_until = median_time(vu_times, n_votes);
    vote_seconds = median_int(votesec_list, n_votes);
    dist_seconds = median_int(distsec_list, n_votes);

    tor_assert(valid_after+MIN_VOTE_INTERVAL <= fresh_until);
    tor_assert(fresh_until+MIN_VOTE_INTERVAL <= valid_until);
    tor_assert(vote_seconds >= MIN_VOTE_SECONDS);
    tor_assert(dist_seconds >= MIN_DIST_SECONDS);

    server_versions = compute_consensus_versions_list(combined_server_versions,
                                                      n_versioning_servers);
    client_versions = compute_consensus_versions_list(combined_client_versions,
                                                      n_versioning_clients);

    SMARTLIST_FOREACH(combined_server_versions, char *, cp, tor_free(cp));
    SMARTLIST_FOREACH(combined_client_versions, char *, cp, tor_free(cp));
    smartlist_free(combined_server_versions);
    smartlist_free(combined_client_versions);

    smartlist_sort_strings(flags);
    smartlist_uniq_strings(flags);

    tor_free(va_times);
    tor_free(fu_times);
    tor_free(vu_times);
    tor_free(votesec_list);
    tor_free(distsec_list);
  }

  chunks = smartlist_new();

  {
    char va_buf[ISO_TIME_LEN+1], fu_buf[ISO_TIME_LEN+1],
      vu_buf[ISO_TIME_LEN+1];
    char *flaglist;
    format_iso_time(va_buf, valid_after);
    format_iso_time(fu_buf, fresh_until);
    format_iso_time(vu_buf, valid_until);
    flaglist = smartlist_join_strings(flags, " ", 0, NULL);

    smartlist_add_asprintf(chunks, "network-status-version 3%s%s\n"
                 "vote-status consensus\n",
                 flavor == FLAV_NS ? "" : " ",
                 flavor == FLAV_NS ? "" : flavor_name);

    if (consensus_method >= 2) {
      smartlist_add_asprintf(chunks, "consensus-method %d\n",
                   consensus_method);
    }

    smartlist_add_asprintf(chunks,
                 "valid-after %s\n"
                 "fresh-until %s\n"
                 "valid-until %s\n"
                 "voting-delay %d %d\n"
                 "client-versions %s\n"
                 "server-versions %s\n"
                 "known-flags %s\n",
                 va_buf, fu_buf, vu_buf,
                 vote_seconds, dist_seconds,
                 client_versions, server_versions, flaglist);

    tor_free(flaglist);
  }

  if (consensus_method >= MIN_METHOD_FOR_PARAMS) {
    params = dirvote_compute_params(votes, consensus_method,
                                    total_authorities);
    if (params) {
      smartlist_add(chunks, tor_strdup("params "));
      smartlist_add(chunks, params);
      smartlist_add(chunks, tor_strdup("\n"));
    }
  }

  /* Sort the votes. */
  smartlist_sort(votes, compare_votes_by_authority_id_);
  /* Add the authority sections. */
  {
    smartlist_t *dir_sources = smartlist_new();
    SMARTLIST_FOREACH_BEGIN(votes, networkstatus_t *, v) {
      dir_src_ent_t *e = tor_malloc_zero(sizeof(dir_src_ent_t));
      e->v = v;
      e->digest = get_voter(v)->identity_digest;
      e->is_legacy = 0;
      smartlist_add(dir_sources, e);
      if (consensus_method >= 3 &&
          !tor_digest_is_zero(get_voter(v)->legacy_id_digest)) {
        dir_src_ent_t *e_legacy = tor_malloc_zero(sizeof(dir_src_ent_t));
        e_legacy->v = v;
        e_legacy->digest = get_voter(v)->legacy_id_digest;
        e_legacy->is_legacy = 1;
        smartlist_add(dir_sources, e_legacy);
      }
    } SMARTLIST_FOREACH_END(v);
    smartlist_sort(dir_sources, compare_dir_src_ents_by_authority_id_);

    SMARTLIST_FOREACH_BEGIN(dir_sources, const dir_src_ent_t *, e) {
      char fingerprint[HEX_DIGEST_LEN+1];
      char votedigest[HEX_DIGEST_LEN+1];
      networkstatus_t *v = e->v;
      networkstatus_voter_info_t *voter = get_voter(v);

      if (e->is_legacy)
        tor_assert(consensus_method >= 2);

      base16_encode(fingerprint, sizeof(fingerprint), e->digest, DIGEST_LEN);
      base16_encode(votedigest, sizeof(votedigest), voter->vote_digest,
                    DIGEST_LEN);

      smartlist_add_asprintf(chunks,
                   "dir-source %s%s %s %s %s %d %d\n",
                   voter->nickname, e->is_legacy ? "-legacy" : "",
                   fingerprint, voter->address, fmt_addr32(voter->addr),
                   voter->dir_port,
                   voter->or_port);
      if (! e->is_legacy) {
        smartlist_add_asprintf(chunks,
                     "contact %s\n"
                     "vote-digest %s\n",
                     voter->contact,
                     votedigest);
      }
    } SMARTLIST_FOREACH_END(e);
    SMARTLIST_FOREACH(dir_sources, dir_src_ent_t *, e, tor_free(e));
    smartlist_free(dir_sources);
  }

  if (consensus_method >= MIN_METHOD_TO_CLIP_UNMEASURED_BW) {
    char *max_unmeasured_param = NULL;
    /* XXXX Extract this code into a common function */
    if (params) {
      if (strcmpstart(params, "maxunmeasuredbw=") == 0)
        max_unmeasured_param = params;
      else
        max_unmeasured_param = strstr(params, " maxunmeasuredbw=");
    }
    if (max_unmeasured_param) {
      int ok = 0;
      char *eq = strchr(max_unmeasured_param, '=');
      if (eq) {
        max_unmeasured_bw_kb = (uint32_t)
          tor_parse_ulong(eq+1, 10, 1, UINT32_MAX, &ok, NULL);
        if (!ok) {
          log_warn(LD_DIR, "Bad element '%s' in max unmeasured bw param",
                   escaped(max_unmeasured_param));
          max_unmeasured_bw_kb = DEFAULT_MAX_UNMEASURED_BW_KB;
        }
      }
    }
  }

  /* Add the actual router entries. */
  {
    int *index; /* index[j] is the current index into votes[j]. */
    int *size; /* size[j] is the number of routerstatuses in votes[j]. */
    int *flag_counts; /* The number of voters that list flag[j] for the
                       * currently considered router. */
    int i;
    smartlist_t *matching_descs = smartlist_new();
    smartlist_t *chosen_flags = smartlist_new();
    smartlist_t *versions = smartlist_new();
    smartlist_t *exitsummaries = smartlist_new();
    uint32_t *bandwidths_kb = tor_malloc(sizeof(uint32_t) *
                                         smartlist_len(votes));
    uint32_t *measured_bws_kb = tor_malloc(sizeof(uint32_t) *
                                           smartlist_len(votes));
    int num_bandwidths;
    int num_mbws;

    int *n_voter_flags; /* n_voter_flags[j] is the number of flags that
                         * votes[j] knows about. */
    int *n_flag_voters; /* n_flag_voters[f] is the number of votes that care
                         * about flags[f]. */
    int **flag_map; /* flag_map[j][b] is an index f such that flag_map[f]
                     * is the same flag as votes[j]->known_flags[b]. */
    int *named_flag; /* Index of the flag "Named" for votes[j] */
    int *unnamed_flag; /* Index of the flag "Unnamed" for votes[j] */
    int chosen_named_idx;
    int n_authorities_measuring_bandwidth;

    strmap_t *name_to_id_map = strmap_new();
    char conflict[DIGEST_LEN];
    char unknown[DIGEST_LEN];
    memset(conflict, 0, sizeof(conflict));
    memset(unknown, 0xff, sizeof(conflict));

    index = tor_malloc_zero(sizeof(int)*smartlist_len(votes));
    size = tor_malloc_zero(sizeof(int)*smartlist_len(votes));
    n_voter_flags = tor_malloc_zero(sizeof(int) * smartlist_len(votes));
    n_flag_voters = tor_malloc_zero(sizeof(int) * smartlist_len(flags));
    flag_map = tor_malloc_zero(sizeof(int*) * smartlist_len(votes));
    named_flag = tor_malloc_zero(sizeof(int) * smartlist_len(votes));
    unnamed_flag = tor_malloc_zero(sizeof(int) * smartlist_len(votes));
    for (i = 0; i < smartlist_len(votes); ++i)
      unnamed_flag[i] = named_flag[i] = -1;
    chosen_named_idx = smartlist_string_pos(flags, "Named");

    /* Build the flag indexes. Note that no vote can have more than 64 members
     * for known_flags, so no value will be greater than 63, so it's safe to
     * do U64_LITERAL(1) << index on these values.  But note also that
     * named_flag and unnamed_flag are initialized to -1, so we need to check
     * that they're actually set before doing U64_LITERAL(1) << index with
     * them.*/
    SMARTLIST_FOREACH_BEGIN(votes, networkstatus_t *, v) {
      flag_map[v_sl_idx] = tor_malloc_zero(
                           sizeof(int)*smartlist_len(v->known_flags));
      if (smartlist_len(v->known_flags) > MAX_KNOWN_FLAGS_IN_VOTE) {
        log_warn(LD_BUG, "Somehow, a vote has %d entries in known_flags",
                 smartlist_len(v->known_flags));
      }
      SMARTLIST_FOREACH_BEGIN(v->known_flags, const char *, fl) {
        int p = smartlist_string_pos(flags, fl);
        tor_assert(p >= 0);
        flag_map[v_sl_idx][fl_sl_idx] = p;
        ++n_flag_voters[p];
        if (!strcmp(fl, "Named"))
          named_flag[v_sl_idx] = fl_sl_idx;
        if (!strcmp(fl, "Unnamed"))
          unnamed_flag[v_sl_idx] = fl_sl_idx;
      } SMARTLIST_FOREACH_END(fl);
      n_voter_flags[v_sl_idx] = smartlist_len(v->known_flags);
      size[v_sl_idx] = smartlist_len(v->routerstatus_list);
    } SMARTLIST_FOREACH_END(v);

    /* Named and Unnamed get treated specially */
    if (consensus_method >= 2) {
      SMARTLIST_FOREACH_BEGIN(votes, networkstatus_t *, v) {
        uint64_t nf;
        if (named_flag[v_sl_idx]<0)
          continue;
        nf = U64_LITERAL(1) << named_flag[v_sl_idx];
        SMARTLIST_FOREACH_BEGIN(v->routerstatus_list,
                                vote_routerstatus_t *, rs) {

          if ((rs->flags & nf) != 0) {
            const char *d = strmap_get_lc(name_to_id_map, rs->status.nickname);
            if (!d) {
              /* We have no name officially mapped to this digest. */
              strmap_set_lc(name_to_id_map, rs->status.nickname,
                            rs->status.identity_digest);
            } else if (d != conflict &&
                fast_memcmp(d, rs->status.identity_digest, DIGEST_LEN)) {
              /* Authorities disagree about this nickname. */
              strmap_set_lc(name_to_id_map, rs->status.nickname, conflict);
            } else {
              /* It's already a conflict, or it's already this ID. */
            }
          }
        } SMARTLIST_FOREACH_END(rs);
      } SMARTLIST_FOREACH_END(v);

      SMARTLIST_FOREACH_BEGIN(votes, networkstatus_t *, v) {
        uint64_t uf;
        if (unnamed_flag[v_sl_idx]<0)
          continue;
        uf = U64_LITERAL(1) << unnamed_flag[v_sl_idx];
        SMARTLIST_FOREACH_BEGIN(v->routerstatus_list,
                                vote_routerstatus_t *, rs) {
          if ((rs->flags & uf) != 0) {
            const char *d = strmap_get_lc(name_to_id_map, rs->status.nickname);
            if (d == conflict || d == unknown) {
              /* Leave it alone; we know what it is. */
            } else if (!d) {
              /* We have no name officially mapped to this digest. */
              strmap_set_lc(name_to_id_map, rs->status.nickname, unknown);
            } else if (fast_memeq(d, rs->status.identity_digest, DIGEST_LEN)) {
              /* Authorities disagree about this nickname. */
              strmap_set_lc(name_to_id_map, rs->status.nickname, conflict);
            } else {
              /* It's mapped to a different name. */
            }
          }
        } SMARTLIST_FOREACH_END(rs);
      } SMARTLIST_FOREACH_END(v);
    }

    /* We need to know how many votes measure bandwidth. */
    n_authorities_measuring_bandwidth = 0;
    SMARTLIST_FOREACH(votes, networkstatus_t *, v,
       if (v->has_measured_bws) {
         ++n_authorities_measuring_bandwidth;
       }
    );

    /* Now go through all the votes */
    flag_counts = tor_malloc(sizeof(int) * smartlist_len(flags));
    while (1) {
      vote_routerstatus_t *rs;
      routerstatus_t rs_out;
      const char *lowest_id = NULL;
      const char *chosen_version;
      const char *chosen_name = NULL;
      int exitsummary_disagreement = 0;
      int is_named = 0, is_unnamed = 0, is_running = 0;
      int is_guard = 0, is_exit = 0, is_bad_exit = 0;
      int naming_conflict = 0;
      int n_listing = 0;
      int i;
      char microdesc_digest[DIGEST256_LEN];
      tor_addr_port_t alt_orport = {TOR_ADDR_NULL, 0};

      /* Of the next-to-be-considered digest in each voter, which is first? */
      SMARTLIST_FOREACH(votes, networkstatus_t *, v, {
        if (index[v_sl_idx] < size[v_sl_idx]) {
          rs = smartlist_get(v->routerstatus_list, index[v_sl_idx]);
          if (!lowest_id ||
              fast_memcmp(rs->status.identity_digest,
                          lowest_id, DIGEST_LEN) < 0)
            lowest_id = rs->status.identity_digest;
        }
      });
      if (!lowest_id) /* we're out of routers. */
        break;

      memset(flag_counts, 0, sizeof(int)*smartlist_len(flags));
      smartlist_clear(matching_descs);
      smartlist_clear(chosen_flags);
      smartlist_clear(versions);
      num_bandwidths = 0;
      num_mbws = 0;

      /* Okay, go through all the entries for this digest. */
      SMARTLIST_FOREACH_BEGIN(votes, networkstatus_t *, v) {
        if (index[v_sl_idx] >= size[v_sl_idx])
          continue; /* out of entries. */
        rs = smartlist_get(v->routerstatus_list, index[v_sl_idx]);
        if (fast_memcmp(rs->status.identity_digest, lowest_id, DIGEST_LEN))
          continue; /* doesn't include this router. */
        /* At this point, we know that we're looking at a routerstatus with
         * identity "lowest".
         */
        ++index[v_sl_idx];
        ++n_listing;

        smartlist_add(matching_descs, rs);
        if (rs->version && rs->version[0])
          smartlist_add(versions, rs->version);

        /* Tally up all the flags. */
        for (i = 0; i < n_voter_flags[v_sl_idx]; ++i) {
          if (rs->flags & (U64_LITERAL(1) << i))
            ++flag_counts[flag_map[v_sl_idx][i]];
        }
        if (named_flag[v_sl_idx] >= 0 &&
            (rs->flags & (U64_LITERAL(1) << named_flag[v_sl_idx]))) {
          if (chosen_name && strcmp(chosen_name, rs->status.nickname)) {
            log_notice(LD_DIR, "Conflict on naming for router: %s vs %s",
                       chosen_name, rs->status.nickname);
            naming_conflict = 1;
          }
          chosen_name = rs->status.nickname;
        }

        /* count bandwidths */
        if (rs->has_measured_bw)
          measured_bws_kb[num_mbws++] = rs->measured_bw_kb;

        if (rs->status.has_bandwidth)
          bandwidths_kb[num_bandwidths++] = rs->status.bandwidth_kb;
      } SMARTLIST_FOREACH_END(v);

      /* We don't include this router at all unless more than half of
       * the authorities we believe in list it. */
      if (n_listing <= total_authorities/2)
        continue;

      /* Figure out the most popular opinion of what the most recent
       * routerinfo and its contents are. */
      memset(microdesc_digest, 0, sizeof(microdesc_digest));
      rs = compute_routerstatus_consensus(matching_descs, consensus_method,
                                          microdesc_digest, &alt_orport);
      /* Copy bits of that into rs_out. */
      memset(&rs_out, 0, sizeof(rs_out));
      tor_assert(fast_memeq(lowest_id, rs->status.identity_digest,DIGEST_LEN));
      memcpy(rs_out.identity_digest, lowest_id, DIGEST_LEN);
      memcpy(rs_out.descriptor_digest, rs->status.descriptor_digest,
             DIGEST_LEN);
      rs_out.addr = rs->status.addr;
      rs_out.published_on = rs->status.published_on;
      rs_out.dir_port = rs->status.dir_port;
      rs_out.or_port = rs->status.or_port;
      if (consensus_method >= MIN_METHOD_FOR_A_LINES) {
        tor_addr_copy(&rs_out.ipv6_addr, &alt_orport.addr);
        rs_out.ipv6_orport = alt_orport.port;
      }
      rs_out.has_bandwidth = 0;
      rs_out.has_exitsummary = 0;

      if (chosen_name && !naming_conflict) {
        strlcpy(rs_out.nickname, chosen_name, sizeof(rs_out.nickname));
      } else {
        strlcpy(rs_out.nickname, rs->status.nickname, sizeof(rs_out.nickname));
      }

      if (consensus_method == 1) {
        is_named = chosen_named_idx >= 0 &&
          (!naming_conflict && flag_counts[chosen_named_idx]);
      } else {
        const char *d = strmap_get_lc(name_to_id_map, rs_out.nickname);
        if (!d) {
          is_named = is_unnamed = 0;
        } else if (fast_memeq(d, lowest_id, DIGEST_LEN)) {
          is_named = 1; is_unnamed = 0;
        } else {
          is_named = 0; is_unnamed = 1;
        }
      }

      /* Set the flags. */
      smartlist_add(chosen_flags, (char*)"s"); /* for the start of the line. */
      SMARTLIST_FOREACH_BEGIN(flags, const char *, fl) {
        if (!strcmp(fl, "Named")) {
          if (is_named)
            smartlist_add(chosen_flags, (char*)fl);
        } else if (!strcmp(fl, "Unnamed") && consensus_method >= 2) {
          if (is_unnamed)
            smartlist_add(chosen_flags, (char*)fl);
        } else {
          if (flag_counts[fl_sl_idx] > n_flag_voters[fl_sl_idx]/2) {
            smartlist_add(chosen_flags, (char*)fl);
            if (!strcmp(fl, "Exit"))
              is_exit = 1;
            else if (!strcmp(fl, "Guard"))
              is_guard = 1;
            else if (!strcmp(fl, "Running"))
              is_running = 1;
            else if (!strcmp(fl, "BadExit"))
              is_bad_exit = 1;
          }
        }
      } SMARTLIST_FOREACH_END(fl);

      /* Starting with consensus method 4 we do not list servers
       * that are not running in a consensus.  See Proposal 138 */
      if (consensus_method >= 4 && !is_running)
        continue;

      /* Pick the version. */
      if (smartlist_len(versions)) {
        sort_version_list(versions, 0);
        chosen_version = get_most_frequent_member(versions);
      } else {
        chosen_version = NULL;
      }

      /* Pick a bandwidth */
      if (consensus_method >= 6 && num_mbws > 2) {
        rs_out.has_bandwidth = 1;
        rs_out.bw_is_unmeasured = 0;
        rs_out.bandwidth_kb = median_uint32(measured_bws_kb, num_mbws);
      } else if (consensus_method >= 5 && num_bandwidths > 0) {
        rs_out.has_bandwidth = 1;
        rs_out.bw_is_unmeasured = 1;
        rs_out.bandwidth_kb = median_uint32(bandwidths_kb, num_bandwidths);
        if (consensus_method >= MIN_METHOD_TO_CLIP_UNMEASURED_BW &&
            n_authorities_measuring_bandwidth > 2) {
          /* Cap non-measured bandwidths. */
          if (rs_out.bandwidth_kb > max_unmeasured_bw_kb) {
            rs_out.bandwidth_kb = max_unmeasured_bw_kb;
          }
        }
      }

      /* Fix bug 2203: Do not count BadExit nodes as Exits for bw weights */
      if (consensus_method >= MIN_METHOD_TO_CUT_BADEXIT_WEIGHT) {
        is_exit = is_exit && !is_bad_exit;
      }

      if (consensus_method >= MIN_METHOD_FOR_BW_WEIGHTS) {
        if (rs_out.has_bandwidth) {
          T += rs_out.bandwidth_kb;
          if (is_exit && is_guard)
            D += rs_out.bandwidth_kb;
          else if (is_exit)
            E += rs_out.bandwidth_kb;
          else if (is_guard)
            G += rs_out.bandwidth_kb;
          else
            M += rs_out.bandwidth_kb;
        } else {
          log_warn(LD_BUG, "Missing consensus bandwidth for router %s",
              rs_out.nickname);
        }
      }

      /* Ok, we already picked a descriptor digest we want to list
       * previously.  Now we want to use the exit policy summary from
       * that descriptor.  If everybody plays nice all the voters who
       * listed that descriptor will have the same summary.  If not then
       * something is fishy and we'll use the most common one (breaking
       * ties in favor of lexicographically larger one (only because it
       * lets me reuse more existing code)).
       *
       * The other case that can happen is that no authority that voted
       * for that descriptor has an exit policy summary.  That's
       * probably quite unlikely but can happen.  In that case we use
       * the policy that was most often listed in votes, again breaking
       * ties like in the previous case.
       */
      if (consensus_method >= 5) {
        /* Okay, go through all the votes for this router.  We prepared
         * that list previously */
        const char *chosen_exitsummary = NULL;
        smartlist_clear(exitsummaries);
        SMARTLIST_FOREACH_BEGIN(matching_descs, vote_routerstatus_t *, vsr) {
          /* Check if the vote where this status comes from had the
           * proper descriptor */
          tor_assert(fast_memeq(rs_out.identity_digest,
                             vsr->status.identity_digest,
                             DIGEST_LEN));
          if (vsr->status.has_exitsummary &&
               fast_memeq(rs_out.descriptor_digest,
                       vsr->status.descriptor_digest,
                       DIGEST_LEN)) {
            tor_assert(vsr->status.exitsummary);
            smartlist_add(exitsummaries, vsr->status.exitsummary);
            if (!chosen_exitsummary) {
              chosen_exitsummary = vsr->status.exitsummary;
            } else if (strcmp(chosen_exitsummary, vsr->status.exitsummary)) {
              /* Great.  There's disagreement among the voters.  That
               * really shouldn't be */
              exitsummary_disagreement = 1;
            }
          }
        } SMARTLIST_FOREACH_END(vsr);

        if (exitsummary_disagreement) {
          char id[HEX_DIGEST_LEN+1];
          char dd[HEX_DIGEST_LEN+1];
          base16_encode(id, sizeof(dd), rs_out.identity_digest, DIGEST_LEN);
          base16_encode(dd, sizeof(dd), rs_out.descriptor_digest, DIGEST_LEN);
          log_warn(LD_DIR, "The voters disagreed on the exit policy summary "
                   " for router %s with descriptor %s.  This really shouldn't"
                   " have happened.", id, dd);

          smartlist_sort_strings(exitsummaries);
          chosen_exitsummary = get_most_frequent_member(exitsummaries);
        } else if (!chosen_exitsummary) {
          char id[HEX_DIGEST_LEN+1];
          char dd[HEX_DIGEST_LEN+1];
          base16_encode(id, sizeof(dd), rs_out.identity_digest, DIGEST_LEN);
          base16_encode(dd, sizeof(dd), rs_out.descriptor_digest, DIGEST_LEN);
          log_warn(LD_DIR, "Not one of the voters that made us select"
                   "descriptor %s for router %s had an exit policy"
                   "summary", dd, id);

          /* Ok, none of those voting for the digest we chose had an
           * exit policy for us.  Well, that kinda sucks.
           */
          smartlist_clear(exitsummaries);
          SMARTLIST_FOREACH(matching_descs, vote_routerstatus_t *, vsr, {
            if (vsr->status.has_exitsummary)
              smartlist_add(exitsummaries, vsr->status.exitsummary);
          });
          smartlist_sort_strings(exitsummaries);
          chosen_exitsummary = get_most_frequent_member(exitsummaries);

          if (!chosen_exitsummary)
            log_warn(LD_DIR, "Wow, not one of the voters had an exit "
                     "policy summary for %s.  Wow.", id);
        }

        if (chosen_exitsummary) {
          rs_out.has_exitsummary = 1;
          /* yea, discards the const */
          rs_out.exitsummary = (char *)chosen_exitsummary;
        }
      }

      if (flavor == FLAV_MICRODESC &&
          consensus_method >= MIN_METHOD_FOR_MANDATORY_MICRODESC &&
          tor_digest256_is_zero(microdesc_digest)) {
        /* With no microdescriptor digest, we omit the entry entirely. */
        continue;
      }

      {
        char *buf;
        /* Okay!! Now we can write the descriptor... */
        /*     First line goes into "buf". */
        buf = routerstatus_format_entry(&rs_out, NULL, rs_format, NULL);
        if (buf)
          smartlist_add(chunks, buf);
      }
      /*     Now an m line, if applicable. */
      if (flavor == FLAV_MICRODESC &&
          !tor_digest256_is_zero(microdesc_digest)) {
        char m[BASE64_DIGEST256_LEN+1];
        digest256_to_base64(m, microdesc_digest);
        smartlist_add_asprintf(chunks, "m %s\n", m);
      }
      /*     Next line is all flags.  The "\n" is missing. */
      smartlist_add(chunks,
                    smartlist_join_strings(chosen_flags, " ", 0, NULL));
      /*     Now the version line. */
      if (chosen_version) {
        smartlist_add(chunks, tor_strdup("\nv "));
        smartlist_add(chunks, tor_strdup(chosen_version));
      }
      smartlist_add(chunks, tor_strdup("\n"));
      /*     Now the weight line. */
      if (rs_out.has_bandwidth) {
        int unmeasured = rs_out.bw_is_unmeasured &&
          consensus_method >= MIN_METHOD_TO_CLIP_UNMEASURED_BW;
        smartlist_add_asprintf(chunks, "w Bandwidth=%d%s\n",
                               rs_out.bandwidth_kb,
                               unmeasured?" Unmeasured=1":"");
      }

      /*     Now the exitpolicy summary line. */
      if (rs_out.has_exitsummary && flavor == FLAV_NS) {
        smartlist_add_asprintf(chunks, "p %s\n", rs_out.exitsummary);
      }

      /* And the loop is over and we move on to the next router */
    }

    tor_free(index);
    tor_free(size);
    tor_free(n_voter_flags);
    tor_free(n_flag_voters);
    for (i = 0; i < smartlist_len(votes); ++i)
      tor_free(flag_map[i]);
    tor_free(flag_map);
    tor_free(flag_counts);
    tor_free(named_flag);
    tor_free(unnamed_flag);
    strmap_free(name_to_id_map, NULL);
    smartlist_free(matching_descs);
    smartlist_free(chosen_flags);
    smartlist_free(versions);
    smartlist_free(exitsummaries);
    tor_free(bandwidths_kb);
    tor_free(measured_bws_kb);
  }

  if (consensus_method >= MIN_METHOD_FOR_FOOTER) {
    /* Starting with consensus method 9, we clearly mark the directory
     * footer region */
    smartlist_add(chunks, tor_strdup("directory-footer\n"));
  }

  if (consensus_method >= MIN_METHOD_FOR_BW_WEIGHTS) {
    int64_t weight_scale = BW_WEIGHT_SCALE;
    char *bw_weight_param = NULL;

    // Parse params, extract BW_WEIGHT_SCALE if present
    // DO NOT use consensus_param_bw_weight_scale() in this code!
    // The consensus is not formed yet!
    /* XXXX Extract this code into a common function */
    if (params) {
      if (strcmpstart(params, "bwweightscale=") == 0)
        bw_weight_param = params;
      else
        bw_weight_param = strstr(params, " bwweightscale=");
    }

    if (bw_weight_param) {
      int ok=0;
      char *eq = strchr(bw_weight_param, '=');
      if (eq) {
        weight_scale = tor_parse_long(eq+1, 10, 1, INT32_MAX, &ok,
                                         NULL);
        if (!ok) {
          log_warn(LD_DIR, "Bad element '%s' in bw weight param",
              escaped(bw_weight_param));
          weight_scale = BW_WEIGHT_SCALE;
        }
      } else {
        log_warn(LD_DIR, "Bad element '%s' in bw weight param",
            escaped(bw_weight_param));
        weight_scale = BW_WEIGHT_SCALE;
      }
    }

    if (consensus_method < 10) {
      networkstatus_compute_bw_weights_v9(chunks, G, M, E, D, T, weight_scale);
      added_weights = 1;
    } else {
      added_weights = networkstatus_compute_bw_weights_v10(chunks, G, M, E, D,
                                                           T, weight_scale);
    }
  }

  /* Add a signature. */
  {
    char digest[DIGEST256_LEN];
    char fingerprint[HEX_DIGEST_LEN+1];
    char signing_key_fingerprint[HEX_DIGEST_LEN+1];
    digest_algorithm_t digest_alg =
      flavor == FLAV_NS ? DIGEST_SHA1 : DIGEST_SHA256;
    size_t digest_len =
      flavor == FLAV_NS ? DIGEST_LEN : DIGEST256_LEN;
    const char *algname = crypto_digest_algorithm_get_name(digest_alg);
    char *signature;

    smartlist_add(chunks, tor_strdup("directory-signature "));

    /* Compute the hash of the chunks. */
    crypto_digest_smartlist(digest, digest_len, chunks, "", digest_alg);

    /* Get the fingerprints */
    crypto_pk_get_fingerprint(identity_key, fingerprint, 0);
    crypto_pk_get_fingerprint(signing_key, signing_key_fingerprint, 0);

    /* add the junk that will go at the end of the line. */
    if (flavor == FLAV_NS) {
      smartlist_add_asprintf(chunks, "%s %s\n", fingerprint,
                   signing_key_fingerprint);
    } else {
      smartlist_add_asprintf(chunks, "%s %s %s\n",
                   algname, fingerprint,
                   signing_key_fingerprint);
    }
    /* And the signature. */
    if (!(signature = router_get_dirobj_signature(digest, digest_len,
                                                  signing_key))) {
      log_warn(LD_BUG, "Couldn't sign consensus networkstatus.");
      goto done;
    }
    smartlist_add(chunks, signature);

    if (legacy_id_key_digest && legacy_signing_key && consensus_method >= 3) {
      smartlist_add(chunks, tor_strdup("directory-signature "));
      base16_encode(fingerprint, sizeof(fingerprint),
                    legacy_id_key_digest, DIGEST_LEN);
      crypto_pk_get_fingerprint(legacy_signing_key,
                                signing_key_fingerprint, 0);
      if (flavor == FLAV_NS) {
        smartlist_add_asprintf(chunks, "%s %s\n", fingerprint,
                     signing_key_fingerprint);
      } else {
        smartlist_add_asprintf(chunks, "%s %s %s\n",
                     algname, fingerprint,
                     signing_key_fingerprint);
      }

      if (!(signature = router_get_dirobj_signature(digest, digest_len,
                                                    legacy_signing_key))) {
        log_warn(LD_BUG, "Couldn't sign consensus networkstatus.");
        goto done;
      }
      smartlist_add(chunks, signature);
    }
  }

  result = smartlist_join_strings(chunks, "", 0, NULL);

  {
    networkstatus_t *c;
    if (!(c = networkstatus_parse_vote_from_string(result, NULL,
                                                   NS_TYPE_CONSENSUS))) {
      log_err(LD_BUG, "Generated a networkstatus consensus we couldn't "
              "parse.");
      tor_free(result);
      goto done;
    }
    // Verify balancing parameters
    if (consensus_method >= MIN_METHOD_FOR_BW_WEIGHTS && added_weights) {
      networkstatus_verify_bw_weights(c, consensus_method);
    }
    networkstatus_vote_free(c);
  }

 done:

  tor_free(client_versions);
  tor_free(server_versions);
  SMARTLIST_FOREACH(flags, char *, cp, tor_free(cp));
  smartlist_free(flags);
  SMARTLIST_FOREACH(chunks, char *, cp, tor_free(cp));
  smartlist_free(chunks);

  return result;
}

/** Given a consensus vote <b>target</b> and a set of detached signatures in
 * <b>sigs</b> that correspond to the same consensus, check whether there are
 * any new signatures in <b>src_voter_list</b> that should be added to
 * <b>target</b>. (A signature should be added if we have no signature for that
 * voter in <b>target</b> yet, or if we have no verifiable signature and the
 * new signature is verifiable.)  Return the number of signatures added or
 * changed, or -1 if the document signed by <b>sigs</b> isn't the same
 * document as <b>target</b>. */
int
networkstatus_add_detached_signatures(networkstatus_t *target,
                                      ns_detached_signatures_t *sigs,
                                      const char *source,
                                      int severity,
                                      const char **msg_out)
{
  int r = 0;
  const char *flavor;
  smartlist_t *siglist;
  tor_assert(sigs);
  tor_assert(target);
  tor_assert(target->type == NS_TYPE_CONSENSUS);

  flavor = networkstatus_get_flavor_name(target->flavor);

  /* Do the times seem right? */
  if (target->valid_after != sigs->valid_after) {
    *msg_out = "Valid-After times do not match "
      "when adding detached signatures to consensus";
    return -1;
  }
  if (target->fresh_until != sigs->fresh_until) {
    *msg_out = "Fresh-until times do not match "
      "when adding detached signatures to consensus";
    return -1;
  }
  if (target->valid_until != sigs->valid_until) {
    *msg_out = "Valid-until times do not match "
      "when adding detached signatures to consensus";
    return -1;
  }
  siglist = strmap_get(sigs->signatures, flavor);
  if (!siglist) {
    *msg_out = "No signatures for given consensus flavor";
    return -1;
  }

  /** Make sure all the digests we know match, and at least one matches. */
  {
    digests_t *digests = strmap_get(sigs->digests, flavor);
    int n_matches = 0;
    int alg;
    if (!digests) {
      *msg_out = "No digests for given consensus flavor";
      return -1;
    }
    for (alg = DIGEST_SHA1; alg < N_DIGEST_ALGORITHMS; ++alg) {
      if (!tor_mem_is_zero(digests->d[alg], DIGEST256_LEN)) {
        if (fast_memeq(target->digests.d[alg], digests->d[alg],
                       DIGEST256_LEN)) {
          ++n_matches;
        } else {
          *msg_out = "Mismatched digest.";
          return -1;
        }
      }
    }
    if (!n_matches) {
      *msg_out = "No regognized digests for given consensus flavor";
    }
  }

  /* For each voter in src... */
  SMARTLIST_FOREACH_BEGIN(siglist, document_signature_t *, sig) {
    char voter_identity[HEX_DIGEST_LEN+1];
    networkstatus_voter_info_t *target_voter =
      networkstatus_get_voter_by_id(target, sig->identity_digest);
    authority_cert_t *cert = NULL;
    const char *algorithm;
    document_signature_t *old_sig = NULL;

    algorithm = crypto_digest_algorithm_get_name(sig->alg);

    base16_encode(voter_identity, sizeof(voter_identity),
                  sig->identity_digest, DIGEST_LEN);
    log_info(LD_DIR, "Looking at signature from %s using %s", voter_identity,
             algorithm);
    /* If the target doesn't know about this voter, then forget it. */
    if (!target_voter) {
      log_info(LD_DIR, "We do not know any voter with ID %s", voter_identity);
      continue;
    }

    old_sig = voter_get_sig_by_algorithm(target_voter, sig->alg);

    /* If the target already has a good signature from this voter, then skip
     * this one. */
    if (old_sig && old_sig->good_signature) {
      log_info(LD_DIR, "We already have a good signature from %s using %s",
               voter_identity, algorithm);
      continue;
    }

    /* Try checking the signature if we haven't already. */
    if (!sig->good_signature && !sig->bad_signature) {
      cert = authority_cert_get_by_digests(sig->identity_digest,
                                           sig->signing_key_digest);
      if (cert)
        networkstatus_check_document_signature(target, sig, cert);
    }

    /* If this signature is good, or we don't have any signature yet,
     * then maybe add it. */
    if (sig->good_signature || !old_sig || old_sig->bad_signature) {
      log_info(LD_DIR, "Adding signature from %s with %s", voter_identity,
               algorithm);
      tor_log(severity, LD_DIR, "Added a signature for %s from %s.",
          target_voter->nickname, source);
      ++r;
      if (old_sig) {
        smartlist_remove(target_voter->sigs, old_sig);
        document_signature_free(old_sig);
      }
      smartlist_add(target_voter->sigs, document_signature_dup(sig));
    } else {
      log_info(LD_DIR, "Not adding signature from %s", voter_identity);
    }
  } SMARTLIST_FOREACH_END(sig);

  return r;
}

/** Return a newly allocated string containing all the signatures on
 * <b>consensus</b> by all voters. If <b>for_detached_signatures</b> is true,
 * then the signatures will be put in a detached signatures document, so
 * prefix any non-NS-flavored signatures with "additional-signature" rather
 * than "directory-signature". */
static char *
networkstatus_format_signatures(networkstatus_t *consensus,
                                int for_detached_signatures)
{
  smartlist_t *elements;
  char buf[4096];
  char *result = NULL;
  int n_sigs = 0;
  const consensus_flavor_t flavor = consensus->flavor;
  const char *flavor_name = networkstatus_get_flavor_name(flavor);
  const char *keyword;

  if (for_detached_signatures && flavor != FLAV_NS)
    keyword = "additional-signature";
  else
    keyword = "directory-signature";

  elements = smartlist_new();

  SMARTLIST_FOREACH_BEGIN(consensus->voters, networkstatus_voter_info_t *, v) {
    SMARTLIST_FOREACH_BEGIN(v->sigs, document_signature_t *, sig) {
      char sk[HEX_DIGEST_LEN+1];
      char id[HEX_DIGEST_LEN+1];
      if (!sig->signature || sig->bad_signature)
        continue;
      ++n_sigs;
      base16_encode(sk, sizeof(sk), sig->signing_key_digest, DIGEST_LEN);
      base16_encode(id, sizeof(id), sig->identity_digest, DIGEST_LEN);
      if (flavor == FLAV_NS) {
        smartlist_add_asprintf(elements,
                     "%s %s %s\n-----BEGIN SIGNATURE-----\n",
                     keyword, id, sk);
      } else {
        const char *digest_name =
          crypto_digest_algorithm_get_name(sig->alg);
        smartlist_add_asprintf(elements,
                     "%s%s%s %s %s %s\n-----BEGIN SIGNATURE-----\n",
                     keyword,
                     for_detached_signatures ? " " : "",
                     for_detached_signatures ? flavor_name : "",
                     digest_name, id, sk);
      }
      base64_encode(buf, sizeof(buf), sig->signature, sig->signature_len);
      strlcat(buf, "-----END SIGNATURE-----\n", sizeof(buf));
      smartlist_add(elements, tor_strdup(buf));
    } SMARTLIST_FOREACH_END(sig);
  } SMARTLIST_FOREACH_END(v);

  result = smartlist_join_strings(elements, "", 0, NULL);
  SMARTLIST_FOREACH(elements, char *, cp, tor_free(cp));
  smartlist_free(elements);
  if (!n_sigs)
    tor_free(result);
  return result;
}

/** Return a newly allocated string holding the detached-signatures document
 * corresponding to the signatures on <b>consensuses</b>, which must contain
 * exactly one FLAV_NS consensus, and no more than one consensus for each
 * other flavor. */
char *
networkstatus_get_detached_signatures(smartlist_t *consensuses)
{
  smartlist_t *elements;
  char *result = NULL, *sigs = NULL;
  networkstatus_t *consensus_ns = NULL;
  tor_assert(consensuses);

  SMARTLIST_FOREACH(consensuses, networkstatus_t *, ns, {
      tor_assert(ns);
      tor_assert(ns->type == NS_TYPE_CONSENSUS);
      if (ns && ns->flavor == FLAV_NS)
        consensus_ns = ns;
  });
  if (!consensus_ns) {
    log_warn(LD_BUG, "No NS consensus given.");
    return NULL;
  }

  elements = smartlist_new();

  {
    char va_buf[ISO_TIME_LEN+1], fu_buf[ISO_TIME_LEN+1],
      vu_buf[ISO_TIME_LEN+1];
    char d[HEX_DIGEST_LEN+1];

    base16_encode(d, sizeof(d),
                  consensus_ns->digests.d[DIGEST_SHA1], DIGEST_LEN);
    format_iso_time(va_buf, consensus_ns->valid_after);
    format_iso_time(fu_buf, consensus_ns->fresh_until);
    format_iso_time(vu_buf, consensus_ns->valid_until);

    smartlist_add_asprintf(elements,
                 "consensus-digest %s\n"
                 "valid-after %s\n"
                 "fresh-until %s\n"
                 "valid-until %s\n", d, va_buf, fu_buf, vu_buf);
  }

  /* Get all the digests for the non-FLAV_NS consensuses */
  SMARTLIST_FOREACH_BEGIN(consensuses, networkstatus_t *, ns) {
    const char *flavor_name = networkstatus_get_flavor_name(ns->flavor);
    int alg;
    if (ns->flavor == FLAV_NS)
      continue;

    /* start with SHA256; we don't include SHA1 for anything but the basic
     * consensus. */
    for (alg = DIGEST_SHA256; alg < N_DIGEST_ALGORITHMS; ++alg) {
      char d[HEX_DIGEST256_LEN+1];
      const char *alg_name =
        crypto_digest_algorithm_get_name(alg);
      if (tor_mem_is_zero(ns->digests.d[alg], DIGEST256_LEN))
        continue;
      base16_encode(d, sizeof(d), ns->digests.d[alg], DIGEST256_LEN);
      smartlist_add_asprintf(elements, "additional-digest %s %s %s\n",
                   flavor_name, alg_name, d);
    }
  } SMARTLIST_FOREACH_END(ns);

  /* Now get all the sigs for non-FLAV_NS consensuses */
  SMARTLIST_FOREACH_BEGIN(consensuses, networkstatus_t *, ns) {
    char *sigs;
    if (ns->flavor == FLAV_NS)
      continue;
    sigs = networkstatus_format_signatures(ns, 1);
    if (!sigs) {
      log_warn(LD_DIR, "Couldn't format signatures");
      goto err;
    }
    smartlist_add(elements, sigs);
  } SMARTLIST_FOREACH_END(ns);

  /* Now add the FLAV_NS consensus signatrures. */
  sigs = networkstatus_format_signatures(consensus_ns, 1);
  if (!sigs)
    goto err;
  smartlist_add(elements, sigs);

  result = smartlist_join_strings(elements, "", 0, NULL);
 err:
  SMARTLIST_FOREACH(elements, char *, cp, tor_free(cp));
  smartlist_free(elements);
  return result;
}

/** Return a newly allocated string holding a detached-signatures document for
 * all of the in-progress consensuses in the <b>n_flavors</b>-element array at
 * <b>pending</b>. */
static char *
get_detached_signatures_from_pending_consensuses(pending_consensus_t *pending,
                                                 int n_flavors)
{
  int flav;
  char *signatures;
  smartlist_t *c = smartlist_new();
  for (flav = 0; flav < n_flavors; ++flav) {
    if (pending[flav].consensus)
      smartlist_add(c, pending[flav].consensus);
  }
  signatures = networkstatus_get_detached_signatures(c);
  smartlist_free(c);
  return signatures;
}

/** Release all storage held in <b>s</b>. */
void
ns_detached_signatures_free(ns_detached_signatures_t *s)
{
  if (!s)
    return;
  if (s->signatures) {
    STRMAP_FOREACH(s->signatures, flavor, smartlist_t *, sigs) {
      SMARTLIST_FOREACH(sigs, document_signature_t *, sig,
                        document_signature_free(sig));
      smartlist_free(sigs);
    } STRMAP_FOREACH_END;
    strmap_free(s->signatures, NULL);
    strmap_free(s->digests, tor_free_);
  }

  tor_free(s);
}

/* =====
 * Certificate functions
 * ===== */

/** Allocate and return a new authority_cert_t with the same contents as
 * <b>cert</b>. */
authority_cert_t *
authority_cert_dup(authority_cert_t *cert)
{
  authority_cert_t *out = tor_malloc(sizeof(authority_cert_t));
  tor_assert(cert);

  memcpy(out, cert, sizeof(authority_cert_t));
  /* Now copy pointed-to things. */
  out->cache_info.signed_descriptor_body =
    tor_strndup(cert->cache_info.signed_descriptor_body,
                cert->cache_info.signed_descriptor_len);
  out->cache_info.saved_location = SAVED_NOWHERE;
  out->identity_key = crypto_pk_dup_key(cert->identity_key);
  out->signing_key = crypto_pk_dup_key(cert->signing_key);

  return out;
}

/* =====
 * Vote scheduling
 * ===== */

/** Set *<b>timing_out</b> to the intervals at which we would like to vote.
 * Note that these aren't the intervals we'll use to vote; they're the ones
 * that we'll vote to use. */
void
dirvote_get_preferred_voting_intervals(vote_timing_t *timing_out)
{
  const or_options_t *options = get_options();

  tor_assert(timing_out);

  timing_out->vote_interval = options->V3AuthVotingInterval;
  timing_out->n_intervals_valid = options->V3AuthNIntervalsValid;
  timing_out->vote_delay = options->V3AuthVoteDelay;
  timing_out->dist_delay = options->V3AuthDistDelay;
}

/** Return the start of the next interval of size <b>interval</b> (in
 * seconds) after <b>now</b>, plus <b>offset</b>. Midnight always
 * starts a fresh interval, and if the last interval of a day would be
 * truncated to less than half its size, it is rolled into the
 * previous interval. */
time_t
dirvote_get_start_of_next_interval(time_t now, int interval, int offset)
{
  struct tm tm;
  time_t midnight_today=0;
  time_t midnight_tomorrow;
  time_t next;

  tor_gmtime_r(&now, &tm);
  tm.tm_hour = 0;
  tm.tm_min = 0;
  tm.tm_sec = 0;

  if (tor_timegm(&tm, &midnight_today) < 0) {
    log_warn(LD_BUG, "Ran into an invalid time when trying to find midnight.");
  }
  midnight_tomorrow = midnight_today + (24*60*60);

  next = midnight_today + ((now-midnight_today)/interval + 1)*interval;

  /* Intervals never cross midnight. */
  if (next > midnight_tomorrow)
    next = midnight_tomorrow;

  /* If the interval would only last half as long as it's supposed to, then
   * skip over to the next day. */
  if (next + interval/2 > midnight_tomorrow)
    next = midnight_tomorrow;

  next += offset;
  if (next - interval > now)
    next -= interval;

  return next;
}

/** Scheduling information for a voting interval. */
static struct {
  /** When do we generate and distribute our vote for this interval? */
  time_t voting_starts;
  /** When do we send an HTTP request for any votes that we haven't
   * been posted yet?*/
  time_t fetch_missing_votes;
  /** When do we give up on getting more votes and generate a consensus? */
  time_t voting_ends;
  /** When do we send an HTTP request for any signatures we're expecting to
   * see on the consensus? */
  time_t fetch_missing_signatures;
  /** When do we publish the consensus? */
  time_t interval_starts;

  /* True iff we have generated and distributed our vote. */
  int have_voted;
  /* True iff we've requested missing votes. */
  int have_fetched_missing_votes;
  /* True iff we have built a consensus and sent the signatures around. */
  int have_built_consensus;
  /* True iff we've fetched missing signatures. */
  int have_fetched_missing_signatures;
  /* True iff we have published our consensus. */
  int have_published_consensus;
} voting_schedule = {0,0,0,0,0,0,0,0,0,0};

/** Set voting_schedule to hold the timing for the next vote we should be
 * doing. */
void
dirvote_recalculate_timing(const or_options_t *options, time_t now)
{
  int interval, vote_delay, dist_delay;
  time_t start;
  time_t end;
  networkstatus_t *consensus;

  if (!authdir_mode_v3(options))
    return;

  consensus = networkstatus_get_live_consensus(now);

  memset(&voting_schedule, 0, sizeof(voting_schedule));

  if (consensus) {
    interval = (int)( consensus->fresh_until - consensus->valid_after );
    vote_delay = consensus->vote_seconds;
    dist_delay = consensus->dist_seconds;
  } else {
    interval = options->TestingV3AuthInitialVotingInterval;
    vote_delay = options->TestingV3AuthInitialVoteDelay;
    dist_delay = options->TestingV3AuthInitialDistDelay;
  }

  tor_assert(interval > 0);

  if (vote_delay + dist_delay > interval/2)
    vote_delay = dist_delay = interval / 4;

  start = voting_schedule.interval_starts =
    dirvote_get_start_of_next_interval(now,interval,
                                      options->TestingV3AuthVotingStartOffset);
  end = dirvote_get_start_of_next_interval(start+1, interval,
                                      options->TestingV3AuthVotingStartOffset);

  tor_assert(end > start);

  voting_schedule.fetch_missing_signatures = start - (dist_delay/2);
  voting_schedule.voting_ends = start - dist_delay;
  voting_schedule.fetch_missing_votes = start - dist_delay - (vote_delay/2);
  voting_schedule.voting_starts = start - dist_delay - vote_delay;

  {
    char tbuf[ISO_TIME_LEN+1];
    format_iso_time(tbuf, voting_schedule.interval_starts);
    log_notice(LD_DIR,"Choosing expected valid-after time as %s: "
               "consensus_set=%d, interval=%d",
               tbuf, consensus?1:0, interval);
  }
}

/** Entry point: Take whatever voting actions are pending as of <b>now</b>. */
void
dirvote_act(const or_options_t *options, time_t now)
{
  if (!authdir_mode_v3(options))
    return;
  if (!voting_schedule.voting_starts) {
    char *keys = list_v3_auth_ids();
    authority_cert_t *c = get_my_v3_authority_cert();
    log_notice(LD_DIR, "Scheduling voting.  Known authority IDs are %s. "
               "Mine is %s.",
               keys, hex_str(c->cache_info.identity_digest, DIGEST_LEN));
    tor_free(keys);
    dirvote_recalculate_timing(options, now);
  }
  if (voting_schedule.voting_starts < now && !voting_schedule.have_voted) {
    log_notice(LD_DIR, "Time to vote.");
    dirvote_perform_vote();
    voting_schedule.have_voted = 1;
  }
  if (voting_schedule.fetch_missing_votes < now &&
      !voting_schedule.have_fetched_missing_votes) {
    log_notice(LD_DIR, "Time to fetch any votes that we're missing.");
    dirvote_fetch_missing_votes();
    voting_schedule.have_fetched_missing_votes = 1;
  }
  if (voting_schedule.voting_ends < now &&
      !voting_schedule.have_built_consensus) {
    log_notice(LD_DIR, "Time to compute a consensus.");
    dirvote_compute_consensuses();
    /* XXXX We will want to try again later if we haven't got enough
     * votes yet.  Implement this if it turns out to ever happen. */
    voting_schedule.have_built_consensus = 1;
  }
  if (voting_schedule.fetch_missing_signatures < now &&
      !voting_schedule.have_fetched_missing_signatures) {
    log_notice(LD_DIR, "Time to fetch any signatures that we're missing.");
    dirvote_fetch_missing_signatures();
    voting_schedule.have_fetched_missing_signatures = 1;
  }
  if (voting_schedule.interval_starts < now &&
      !voting_schedule.have_published_consensus) {
    log_notice(LD_DIR, "Time to publish the consensus and discard old votes");
    dirvote_publish_consensus();
    dirvote_clear_votes(0);
    voting_schedule.have_published_consensus = 1;
    /* XXXX We will want to try again later if we haven't got enough
     * signatures yet.  Implement this if it turns out to ever happen. */
    dirvote_recalculate_timing(options, now);
  }
}

/** A vote networkstatus_t and its unparsed body: held around so we can
 * use it to generate a consensus (at voting_ends) and so we can serve it to
 * other authorities that might want it. */
typedef struct pending_vote_t {
  cached_dir_t *vote_body;
  networkstatus_t *vote;
} pending_vote_t;

/** List of pending_vote_t for the current vote.  Before we've used them to
 * build a consensus, the votes go here. */
static smartlist_t *pending_vote_list = NULL;
/** List of pending_vote_t for the previous vote.  After we've used them to
 * build a consensus, the votes go here for the next period. */
static smartlist_t *previous_vote_list = NULL;

/* DOCDOC pending_consensuses */
static pending_consensus_t pending_consensuses[N_CONSENSUS_FLAVORS];

/** The detached signatures for the consensus that we're currently
 * building. */
static char *pending_consensus_signatures = NULL;

/** List of ns_detached_signatures_t: hold signatures that get posted to us
 * before we have generated the consensus on our own. */
static smartlist_t *pending_consensus_signature_list = NULL;

/** Generate a networkstatus vote and post it to all the v3 authorities.
 * (V3 Authority only) */
static int
dirvote_perform_vote(void)
{
  crypto_pk_t *key = get_my_v3_authority_signing_key();
  authority_cert_t *cert = get_my_v3_authority_cert();
  networkstatus_t *ns;
  char *contents;
  pending_vote_t *pending_vote;
  time_t now = time(NULL);

  int status;
  const char *msg = "";

  if (!cert || !key) {
    log_warn(LD_NET, "Didn't find key/certificate to generate v3 vote");
    return -1;
  } else if (cert->expires < now) {
    log_warn(LD_NET, "Can't generate v3 vote with expired certificate");
    return -1;
  }
  if (!(ns = dirserv_generate_networkstatus_vote_obj(key, cert)))
    return -1;

  contents = format_networkstatus_vote(key, ns);
  networkstatus_vote_free(ns);
  if (!contents)
    return -1;

  pending_vote = dirvote_add_vote(contents, &msg, &status);
  tor_free(contents);
  if (!pending_vote) {
    log_warn(LD_DIR, "Couldn't store my own vote! (I told myself, '%s'.)",
             msg);
    return -1;
  }

  directory_post_to_dirservers(DIR_PURPOSE_UPLOAD_VOTE,
                               ROUTER_PURPOSE_GENERAL,
                               V3_DIRINFO,
                               pending_vote->vote_body->dir,
                               pending_vote->vote_body->dir_len, 0);
  log_notice(LD_DIR, "Vote posted.");
  return 0;
}

/** Send an HTTP request to every other v3 authority, for the votes of every
 * authority for which we haven't received a vote yet in this period. (V3
 * authority only) */
static void
dirvote_fetch_missing_votes(void)
{
  smartlist_t *missing_fps = smartlist_new();
  char *resource;

  SMARTLIST_FOREACH_BEGIN(router_get_trusted_dir_servers(),
                          dir_server_t *, ds) {
      if (!(ds->type & V3_DIRINFO))
        continue;
      if (!dirvote_get_vote(ds->v3_identity_digest,
                            DGV_BY_ID|DGV_INCLUDE_PENDING)) {
        char *cp = tor_malloc(HEX_DIGEST_LEN+1);
        base16_encode(cp, HEX_DIGEST_LEN+1, ds->v3_identity_digest,
                      DIGEST_LEN);
        smartlist_add(missing_fps, cp);
      }
  } SMARTLIST_FOREACH_END(ds);

  if (!smartlist_len(missing_fps)) {
    smartlist_free(missing_fps);
    return;
  }
  {
    char *tmp = smartlist_join_strings(missing_fps, " ", 0, NULL);
    log_notice(LOG_NOTICE, "We're missing votes from %d authorities (%s). "
               "Asking every other authority for a copy.",
               smartlist_len(missing_fps), tmp);
    tor_free(tmp);
  }
  resource = smartlist_join_strings(missing_fps, "+", 0, NULL);
  directory_get_from_all_authorities(DIR_PURPOSE_FETCH_STATUS_VOTE,
                                     0, resource);
  tor_free(resource);
  SMARTLIST_FOREACH(missing_fps, char *, cp, tor_free(cp));
  smartlist_free(missing_fps);
}

/** Send a request to every other authority for its detached signatures,
 * unless we have signatures from all other v3 authorities already. */
static void
dirvote_fetch_missing_signatures(void)
{
  int need_any = 0;
  int i;
  for (i=0; i < N_CONSENSUS_FLAVORS; ++i) {
    networkstatus_t *consensus = pending_consensuses[i].consensus;
    if (!consensus ||
        networkstatus_check_consensus_signature(consensus, -1) == 1) {
      /* We have no consensus, or we have one that's signed by everybody. */
      continue;
    }
    need_any = 1;
  }
  if (!need_any)
    return;

  directory_get_from_all_authorities(DIR_PURPOSE_FETCH_DETACHED_SIGNATURES,
                                     0, NULL);
}

/** Release all storage held by pending consensuses (those waiting for
 * signatures). */
static void
dirvote_clear_pending_consensuses(void)
{
  int i;
  for (i = 0; i < N_CONSENSUS_FLAVORS; ++i) {
    pending_consensus_t *pc = &pending_consensuses[i];
    tor_free(pc->body);

    networkstatus_vote_free(pc->consensus);
    pc->consensus = NULL;
  }
}

/** Drop all currently pending votes, consensus, and detached signatures. */
static void
dirvote_clear_votes(int all_votes)
{
  if (!previous_vote_list)
    previous_vote_list = smartlist_new();
  if (!pending_vote_list)
    pending_vote_list = smartlist_new();

  /* All "previous" votes are now junk. */
  SMARTLIST_FOREACH(previous_vote_list, pending_vote_t *, v, {
      cached_dir_decref(v->vote_body);
      v->vote_body = NULL;
      networkstatus_vote_free(v->vote);
      tor_free(v);
    });
  smartlist_clear(previous_vote_list);

  if (all_votes) {
    /* If we're dumping all the votes, we delete the pending ones. */
    SMARTLIST_FOREACH(pending_vote_list, pending_vote_t *, v, {
        cached_dir_decref(v->vote_body);
        v->vote_body = NULL;
        networkstatus_vote_free(v->vote);
        tor_free(v);
      });
  } else {
    /* Otherwise, we move them into "previous". */
    smartlist_add_all(previous_vote_list, pending_vote_list);
  }
  smartlist_clear(pending_vote_list);

  if (pending_consensus_signature_list) {
    SMARTLIST_FOREACH(pending_consensus_signature_list, char *, cp,
                      tor_free(cp));
    smartlist_clear(pending_consensus_signature_list);
  }
  tor_free(pending_consensus_signatures);
  dirvote_clear_pending_consensuses();
}

/** Return a newly allocated string containing the hex-encoded v3 authority
    identity digest of every recognized v3 authority. */
static char *
list_v3_auth_ids(void)
{
  smartlist_t *known_v3_keys = smartlist_new();
  char *keys;
  SMARTLIST_FOREACH(router_get_trusted_dir_servers(),
                    dir_server_t *, ds,
    if ((ds->type & V3_DIRINFO) &&
        !tor_digest_is_zero(ds->v3_identity_digest))
      smartlist_add(known_v3_keys,
                    tor_strdup(hex_str(ds->v3_identity_digest, DIGEST_LEN))));
  keys = smartlist_join_strings(known_v3_keys, ", ", 0, NULL);
  SMARTLIST_FOREACH(known_v3_keys, char *, cp, tor_free(cp));
  smartlist_free(known_v3_keys);
  return keys;
}

/** Called when we have received a networkstatus vote in <b>vote_body</b>.
 * Parse and validate it, and on success store it as a pending vote (which we
 * then return).  Return NULL on failure.  Sets *<b>msg_out</b> and
 * *<b>status_out</b> to an HTTP response and status code.  (V3 authority
 * only) */
pending_vote_t *
dirvote_add_vote(const char *vote_body, const char **msg_out, int *status_out)
{
  networkstatus_t *vote;
  networkstatus_voter_info_t *vi;
  dir_server_t *ds;
  pending_vote_t *pending_vote = NULL;
  const char *end_of_vote = NULL;
  int any_failed = 0;
  tor_assert(vote_body);
  tor_assert(msg_out);
  tor_assert(status_out);

  if (!pending_vote_list)
    pending_vote_list = smartlist_new();
  *status_out = 0;
  *msg_out = NULL;

 again:
  vote = networkstatus_parse_vote_from_string(vote_body, &end_of_vote,
                                              NS_TYPE_VOTE);
  if (!end_of_vote)
    end_of_vote = vote_body + strlen(vote_body);
  if (!vote) {
    log_warn(LD_DIR, "Couldn't parse vote: length was %d",
             (int)strlen(vote_body));
    *msg_out = "Unable to parse vote";
    goto err;
  }
  tor_assert(smartlist_len(vote->voters) == 1);
  vi = get_voter(vote);
  {
    int any_sig_good = 0;
    SMARTLIST_FOREACH(vi->sigs, document_signature_t *, sig,
                      if (sig->good_signature)
                        any_sig_good = 1);
    tor_assert(any_sig_good);
  }
  ds = trusteddirserver_get_by_v3_auth_digest(vi->identity_digest);
  if (!ds) {
    char *keys = list_v3_auth_ids();
    log_warn(LD_DIR, "Got a vote from an authority (nickname %s, address %s) "
             "with authority key ID %s. "
             "This key ID is not recognized.  Known v3 key IDs are: %s",
             vi->nickname, vi->address,
             hex_str(vi->identity_digest, DIGEST_LEN), keys);
    tor_free(keys);
    *msg_out = "Vote not from a recognized v3 authority";
    goto err;
  }
  tor_assert(vote->cert);
  if (!authority_cert_get_by_digests(vote->cert->cache_info.identity_digest,
                                     vote->cert->signing_key_digest)) {
    /* Hey, it's a new cert! */
    trusted_dirs_load_certs_from_string(
                               vote->cert->cache_info.signed_descriptor_body,
                               TRUSTED_DIRS_CERTS_SRC_FROM_VOTE, 1 /*flush*/);
    if (!authority_cert_get_by_digests(vote->cert->cache_info.identity_digest,
                                       vote->cert->signing_key_digest)) {
      log_warn(LD_BUG, "We added a cert, but still couldn't find it.");
    }
  }

  /* Is it for the right period? */
  if (vote->valid_after != voting_schedule.interval_starts) {
    char tbuf1[ISO_TIME_LEN+1], tbuf2[ISO_TIME_LEN+1];
    format_iso_time(tbuf1, vote->valid_after);
    format_iso_time(tbuf2, voting_schedule.interval_starts);
    log_warn(LD_DIR, "Rejecting vote from %s with valid-after time of %s; "
             "we were expecting %s", vi->address, tbuf1, tbuf2);
    *msg_out = "Bad valid-after time";
    goto err;
  }

  /* Fetch any new router descriptors we just learned about */
  update_consensus_router_descriptor_downloads(time(NULL), 1, vote);

  /* Now see whether we already have a vote from this authority. */
  SMARTLIST_FOREACH_BEGIN(pending_vote_list, pending_vote_t *, v) {
      if (fast_memeq(v->vote->cert->cache_info.identity_digest,
                   vote->cert->cache_info.identity_digest,
                   DIGEST_LEN)) {
        networkstatus_voter_info_t *vi_old = get_voter(v->vote);
        if (fast_memeq(vi_old->vote_digest, vi->vote_digest, DIGEST_LEN)) {
          /* Ah, it's the same vote. Not a problem. */
          log_info(LD_DIR, "Discarding a vote we already have (from %s).",
                   vi->address);
          if (*status_out < 200)
            *status_out = 200;
          goto discard;
        } else if (v->vote->published < vote->published) {
          log_notice(LD_DIR, "Replacing an older pending vote from this "
                     "directory.");
          cached_dir_decref(v->vote_body);
          networkstatus_vote_free(v->vote);
          v->vote_body = new_cached_dir(tor_strndup(vote_body,
                                                    end_of_vote-vote_body),
                                        vote->published);
          v->vote = vote;
          if (end_of_vote &&
              !strcmpstart(end_of_vote, "network-status-version"))
            goto again;

          if (*status_out < 200)
            *status_out = 200;
          if (!*msg_out)
            *msg_out = "OK";
          return v;
        } else {
          *msg_out = "Already have a newer pending vote";
          goto err;
        }
      }
  } SMARTLIST_FOREACH_END(v);

  pending_vote = tor_malloc_zero(sizeof(pending_vote_t));
  pending_vote->vote_body = new_cached_dir(tor_strndup(vote_body,
                                                       end_of_vote-vote_body),
                                           vote->published);
  pending_vote->vote = vote;
  smartlist_add(pending_vote_list, pending_vote);

  if (!strcmpstart(end_of_vote, "network-status-version ")) {
    vote_body = end_of_vote;
    goto again;
  }

  goto done;

 err:
  any_failed = 1;
  if (!*msg_out)
    *msg_out = "Error adding vote";
  if (*status_out < 400)
    *status_out = 400;

 discard:
  networkstatus_vote_free(vote);

  if (end_of_vote && !strcmpstart(end_of_vote, "network-status-version ")) {
    vote_body = end_of_vote;
    goto again;
  }

 done:

  if (*status_out < 200)
    *status_out = 200;
  if (!*msg_out) {
    if (!any_failed && !pending_vote) {
      *msg_out = "Duplicate discarded";
    } else {
      *msg_out = "ok";
    }
  }

  return any_failed ? NULL : pending_vote;
}

/** Try to compute a v3 networkstatus consensus from the currently pending
 * votes.  Return 0 on success, -1 on failure.  Store the consensus in
 * pending_consensus: it won't be ready to be published until we have
 * everybody else's signatures collected too. (V3 Authority only) */
static int
dirvote_compute_consensuses(void)
{
  /* Have we got enough votes to try? */
  int n_votes, n_voters, n_vote_running = 0;
  smartlist_t *votes = NULL, *votestrings = NULL;
  char *consensus_body = NULL, *signatures = NULL, *votefile;
  networkstatus_t *consensus = NULL;
  authority_cert_t *my_cert;
  pending_consensus_t pending[N_CONSENSUS_FLAVORS];
  int flav;

  memset(pending, 0, sizeof(pending));

  if (!pending_vote_list)
    pending_vote_list = smartlist_new();

  n_voters = get_n_authorities(V3_DIRINFO);
  n_votes = smartlist_len(pending_vote_list);
  if (n_votes <= n_voters/2) {
    log_warn(LD_DIR, "We don't have enough votes to generate a consensus: "
             "%d of %d", n_votes, n_voters/2+1);
    goto err;
  }
  tor_assert(pending_vote_list);
  SMARTLIST_FOREACH(pending_vote_list, pending_vote_t *, v, {
    if (smartlist_contains_string(v->vote->known_flags, "Running"))
      n_vote_running++;
  });
  if (!n_vote_running) {
    /* See task 1066. */
    log_warn(LD_DIR, "Nobody has voted on the Running flag. Generating "
                     "and publishing a consensus without Running nodes "
                     "would make many clients stop working. Not "
                     "generating a consensus!");
    goto err;
  }

  if (!(my_cert = get_my_v3_authority_cert())) {
    log_warn(LD_DIR, "Can't generate consensus without a certificate.");
    goto err;
  }

  votes = smartlist_new();
  votestrings = smartlist_new();
  SMARTLIST_FOREACH(pending_vote_list, pending_vote_t *, v,
    {
      sized_chunk_t *c = tor_malloc(sizeof(sized_chunk_t));
      c->bytes = v->vote_body->dir;
      c->len = v->vote_body->dir_len;
      smartlist_add(votestrings, c); /* collect strings to write to disk */

      smartlist_add(votes, v->vote); /* collect votes to compute consensus */
    });

  votefile = get_datadir_fname("v3-status-votes");
  write_chunks_to_file(votefile, votestrings, 0, 0);
  tor_free(votefile);
  SMARTLIST_FOREACH(votestrings, sized_chunk_t *, c, tor_free(c));
  smartlist_free(votestrings);

  {
    char legacy_dbuf[DIGEST_LEN];
    crypto_pk_t *legacy_sign=NULL;
    char *legacy_id_digest = NULL;
    int n_generated = 0;
    if (get_options()->V3AuthUseLegacyKey) {
      authority_cert_t *cert = get_my_v3_legacy_cert();
      legacy_sign = get_my_v3_legacy_signing_key();
      if (cert) {
        if (crypto_pk_get_digest(cert->identity_key, legacy_dbuf)) {
          log_warn(LD_BUG,
                   "Unable to compute digest of legacy v3 identity key");
        } else {
          legacy_id_digest = legacy_dbuf;
        }
      }
    }

    for (flav = 0; flav < N_CONSENSUS_FLAVORS; ++flav) {
      const char *flavor_name = networkstatus_get_flavor_name(flav);
      consensus_body = networkstatus_compute_consensus(
        votes, n_voters,
        my_cert->identity_key,
        get_my_v3_authority_signing_key(), legacy_id_digest, legacy_sign,
        flav);

      if (!consensus_body) {
        log_warn(LD_DIR, "Couldn't generate a %s consensus at all!",
                 flavor_name);
        continue;
      }
      consensus = networkstatus_parse_vote_from_string(consensus_body, NULL,
                                                       NS_TYPE_CONSENSUS);
      if (!consensus) {
        log_warn(LD_DIR, "Couldn't parse %s consensus we generated!",
                 flavor_name);
        tor_free(consensus_body);
        continue;
      }

      /* 'Check' our own signature, to mark it valid. */
      networkstatus_check_consensus_signature(consensus, -1);

      pending[flav].body = consensus_body;
      pending[flav].consensus = consensus;
      n_generated++;
      consensus_body = NULL;
      consensus = NULL;
    }
    if (!n_generated) {
      log_warn(LD_DIR, "Couldn't generate any consensus flavors at all.");
      goto err;
    }
  }

  signatures = get_detached_signatures_from_pending_consensuses(
       pending, N_CONSENSUS_FLAVORS);

  if (!signatures) {
    log_warn(LD_DIR, "Couldn't extract signatures.");
    goto err;
  }

  dirvote_clear_pending_consensuses();
  memcpy(pending_consensuses, pending, sizeof(pending));

  tor_free(pending_consensus_signatures);
  pending_consensus_signatures = signatures;

  if (pending_consensus_signature_list) {
    int n_sigs = 0;
    /* we may have gotten signatures for this consensus before we built
     * it ourself.  Add them now. */
    SMARTLIST_FOREACH_BEGIN(pending_consensus_signature_list, char *, sig) {
        const char *msg = NULL;
        int r = dirvote_add_signatures_to_all_pending_consensuses(sig,
                                                     "pending", &msg);
        if (r >= 0)
          n_sigs += r;
        else
          log_warn(LD_DIR,
                   "Could not add queued signature to new consensus: %s",
                   msg);
        tor_free(sig);
    } SMARTLIST_FOREACH_END(sig);
    if (n_sigs)
      log_notice(LD_DIR, "Added %d pending signatures while building "
                 "consensus.", n_sigs);
    smartlist_clear(pending_consensus_signature_list);
  }

  log_notice(LD_DIR, "Consensus computed; uploading signature(s)");

  directory_post_to_dirservers(DIR_PURPOSE_UPLOAD_SIGNATURES,
                               ROUTER_PURPOSE_GENERAL,
                               V3_DIRINFO,
                               pending_consensus_signatures,
                               strlen(pending_consensus_signatures), 0);
  log_notice(LD_DIR, "Signature(s) posted.");

  smartlist_free(votes);
  return 0;
 err:
  smartlist_free(votes);
  tor_free(consensus_body);
  tor_free(signatures);
  networkstatus_vote_free(consensus);

  return -1;
}

/** Helper: we just got the <b>detached_signatures_body</b> sent to us as
 * signatures on the currently pending consensus.  Add them to <b>pc</b>
 * as appropriate.  Return the number of signatures added. (?) */
static int
dirvote_add_signatures_to_pending_consensus(
                       pending_consensus_t *pc,
                       ns_detached_signatures_t *sigs,
                       const char *source,
                       int severity,
                       const char **msg_out)
{
  const char *flavor_name;
  int r = -1;

  /* Only call if we have a pending consensus right now. */
  tor_assert(pc->consensus);
  tor_assert(pc->body);
  tor_assert(pending_consensus_signatures);

  flavor_name = networkstatus_get_flavor_name(pc->consensus->flavor);
  *msg_out = NULL;

  {
    smartlist_t *sig_list = strmap_get(sigs->signatures, flavor_name);
    log_info(LD_DIR, "Have %d signatures for adding to %s consensus.",
             sig_list ? smartlist_len(sig_list) : 0, flavor_name);
  }
  r = networkstatus_add_detached_signatures(pc->consensus, sigs,
                                            source, severity, msg_out);
  log_info(LD_DIR,"Added %d signatures to consensus.", r);

  if (r >= 1) {
    char *new_signatures =
      networkstatus_format_signatures(pc->consensus, 0);
    char *dst, *dst_end;
    size_t new_consensus_len;
    if (!new_signatures) {
      *msg_out = "No signatures to add";
      goto err;
    }
    new_consensus_len =
      strlen(pc->body) + strlen(new_signatures) + 1;
    pc->body = tor_realloc(pc->body, new_consensus_len);
    dst_end = pc->body + new_consensus_len;
    dst = strstr(pc->body, "directory-signature ");
    tor_assert(dst);
    strlcpy(dst, new_signatures, dst_end-dst);

    /* We remove this block once it has failed to crash for a while.  But
     * unless it shows up in profiles, we're probably better leaving it in,
     * just in case we break detached signature processing at some point. */
    {
      networkstatus_t *v = networkstatus_parse_vote_from_string(
                                             pc->body, NULL,
                                             NS_TYPE_CONSENSUS);
      tor_assert(v);
      networkstatus_vote_free(v);
    }
    *msg_out = "Signatures added";
    tor_free(new_signatures);
  } else if (r == 0) {
    *msg_out = "Signatures ignored";
  } else {
    goto err;
  }

  goto done;
 err:
  if (!*msg_out)
    *msg_out = "Unrecognized error while adding detached signatures.";
 done:
  return r;
}

static int
dirvote_add_signatures_to_all_pending_consensuses(
                       const char *detached_signatures_body,
                       const char *source,
                       const char **msg_out)
{
  int r=0, i, n_added = 0, errors = 0;
  ns_detached_signatures_t *sigs;
  tor_assert(detached_signatures_body);
  tor_assert(msg_out);
  tor_assert(pending_consensus_signatures);

  if (!(sigs = networkstatus_parse_detached_signatures(
                               detached_signatures_body, NULL))) {
    *msg_out = "Couldn't parse detached signatures.";
    goto err;
  }

  for (i = 0; i < N_CONSENSUS_FLAVORS; ++i) {
    int res;
    int severity = i == FLAV_NS ? LOG_NOTICE : LOG_INFO;
    pending_consensus_t *pc = &pending_consensuses[i];
    if (!pc->consensus)
      continue;
    res = dirvote_add_signatures_to_pending_consensus(pc, sigs, source,
                                                      severity, msg_out);
    if (res < 0)
      errors++;
    else
      n_added += res;
  }

  if (errors && !n_added) {
    r = -1;
    goto err;
  }

  if (n_added && pending_consensuses[FLAV_NS].consensus) {
    char *new_detached =
      get_detached_signatures_from_pending_consensuses(
                      pending_consensuses, N_CONSENSUS_FLAVORS);
    if (new_detached) {
      tor_free(pending_consensus_signatures);
      pending_consensus_signatures = new_detached;
    }
  }

  r = n_added;
  goto done;
 err:
  if (!*msg_out)
    *msg_out = "Unrecognized error while adding detached signatures.";
 done:
  ns_detached_signatures_free(sigs);
  /* XXXX NM Check how return is used.  We can now have an error *and*
     signatures added. */
  return r;
}

/** Helper: we just got the <b>detached_signatures_body</b> sent to us as
 * signatures on the currently pending consensus.  Add them to the pending
 * consensus (if we have one); otherwise queue them until we have a
 * consensus.  Return negative on failure, nonnegative on success. */
int
dirvote_add_signatures(const char *detached_signatures_body,
                       const char *source,
                       const char **msg)
{
  if (pending_consensuses[FLAV_NS].consensus) {
    log_notice(LD_DIR, "Got a signature from %s. "
                       "Adding it to the pending consensus.", source);
    return dirvote_add_signatures_to_all_pending_consensuses(
                                     detached_signatures_body, source, msg);
  } else {
    log_notice(LD_DIR, "Got a signature from %s. "
                       "Queuing it for the next consensus.", source);
    if (!pending_consensus_signature_list)
      pending_consensus_signature_list = smartlist_new();
    smartlist_add(pending_consensus_signature_list,
                  tor_strdup(detached_signatures_body));
    *msg = "Signature queued";
    return 0;
  }
}

/** Replace the consensus that we're currently serving with the one that we've
 * been building. (V3 Authority only) */
static int
dirvote_publish_consensus(void)
{
  int i;

  /* Now remember all the other consensuses as if we were a directory cache. */
  for (i = 0; i < N_CONSENSUS_FLAVORS; ++i) {
    pending_consensus_t *pending = &pending_consensuses[i];
    const char *name;
    name = networkstatus_get_flavor_name(i);
    tor_assert(name);
    if (!pending->consensus ||
      networkstatus_check_consensus_signature(pending->consensus, 1)<0) {
      log_warn(LD_DIR, "Not enough info to publish pending %s consensus",name);
      continue;
    }

    if (networkstatus_set_current_consensus(pending->body, name, 0))
      log_warn(LD_DIR, "Error publishing %s consensus", name);
    else
      log_notice(LD_DIR, "Published %s consensus", name);
  }

  return 0;
}

/** Release all static storage held in dirvote.c */
void
dirvote_free_all(void)
{
  dirvote_clear_votes(1);
  /* now empty as a result of dirvote_clear_votes(). */
  smartlist_free(pending_vote_list);
  pending_vote_list = NULL;
  smartlist_free(previous_vote_list);
  previous_vote_list = NULL;

  dirvote_clear_pending_consensuses();
  tor_free(pending_consensus_signatures);
  if (pending_consensus_signature_list) {
    /* now empty as a result of dirvote_clear_votes(). */
    smartlist_free(pending_consensus_signature_list);
    pending_consensus_signature_list = NULL;
  }
}

/* ====
 * Access to pending items.
 * ==== */

/** Return the body of the consensus that we're currently trying to build. */
const char *
dirvote_get_pending_consensus(consensus_flavor_t flav)
{
  tor_assert(((int)flav) >= 0 && (int)flav < N_CONSENSUS_FLAVORS);
  return pending_consensuses[flav].body;
}

/** Return the signatures that we know for the consensus that we're currently
 * trying to build. */
const char *
dirvote_get_pending_detached_signatures(void)
{
  return pending_consensus_signatures;
}

/** Return a given vote specified by <b>fp</b>.  If <b>by_id</b>, return the
 * vote for the authority with the v3 authority identity key digest <b>fp</b>;
 * if <b>by_id</b> is false, return the vote whose digest is <b>fp</b>.  If
 * <b>fp</b> is NULL, return our own vote.  If <b>include_previous</b> is
 * false, do not consider any votes for a consensus that's already been built.
 * If <b>include_pending</b> is false, do not consider any votes for the
 * consensus that's in progress.  May return NULL if we have no vote for the
 * authority in question. */
const cached_dir_t *
dirvote_get_vote(const char *fp, int flags)
{
  int by_id = flags & DGV_BY_ID;
  const int include_pending = flags & DGV_INCLUDE_PENDING;
  const int include_previous = flags & DGV_INCLUDE_PREVIOUS;

  if (!pending_vote_list && !previous_vote_list)
    return NULL;
  if (fp == NULL) {
    authority_cert_t *c = get_my_v3_authority_cert();
    if (c) {
      fp = c->cache_info.identity_digest;
      by_id = 1;
    } else
      return NULL;
  }
  if (by_id) {
    if (pending_vote_list && include_pending) {
      SMARTLIST_FOREACH(pending_vote_list, pending_vote_t *, pv,
        if (fast_memeq(get_voter(pv->vote)->identity_digest, fp, DIGEST_LEN))
          return pv->vote_body);
    }
    if (previous_vote_list && include_previous) {
      SMARTLIST_FOREACH(previous_vote_list, pending_vote_t *, pv,
        if (fast_memeq(get_voter(pv->vote)->identity_digest, fp, DIGEST_LEN))
          return pv->vote_body);
    }
  } else {
    if (pending_vote_list && include_pending) {
      SMARTLIST_FOREACH(pending_vote_list, pending_vote_t *, pv,
        if (fast_memeq(pv->vote->digests.d[DIGEST_SHA1], fp, DIGEST_LEN))
          return pv->vote_body);
    }
    if (previous_vote_list && include_previous) {
      SMARTLIST_FOREACH(previous_vote_list, pending_vote_t *, pv,
        if (fast_memeq(pv->vote->digests.d[DIGEST_SHA1], fp, DIGEST_LEN))
          return pv->vote_body);
    }
  }
  return NULL;
}

/** Construct and return a new microdescriptor from a routerinfo <b>ri</b>
 * according to <b>consensus_method</b>.
 **/
microdesc_t *
dirvote_create_microdescriptor(const routerinfo_t *ri, int consensus_method)
{
  microdesc_t *result = NULL;
  char *key = NULL, *summary = NULL, *family = NULL;
  size_t keylen;
  smartlist_t *chunks = smartlist_new();
  char *output = NULL;

  if (crypto_pk_write_public_key_to_string(ri->onion_pkey, &key, &keylen)<0)
    goto done;
  summary = policy_summarize(ri->exit_policy, AF_INET);
  if (ri->declared_family)
    family = smartlist_join_strings(ri->declared_family, " ", 0, NULL);

  smartlist_add_asprintf(chunks, "onion-key\n%s", key);

  if (consensus_method >= MIN_METHOD_FOR_NTOR_KEY &&
      ri->onion_curve25519_pkey) {
    char kbuf[128];
    base64_encode(kbuf, sizeof(kbuf),
                  (const char*)ri->onion_curve25519_pkey->public_key,
                  CURVE25519_PUBKEY_LEN);
    smartlist_add_asprintf(chunks, "ntor-onion-key %s", kbuf);
  }

  if (consensus_method >= MIN_METHOD_FOR_A_LINES &&
      !tor_addr_is_null(&ri->ipv6_addr) && ri->ipv6_orport)
    smartlist_add_asprintf(chunks, "a %s\n",
                           fmt_addrport(&ri->ipv6_addr, ri->ipv6_orport));

  if (family)
    smartlist_add_asprintf(chunks, "family %s\n", family);

  if (summary && strcmp(summary, "reject 1-65535"))
    smartlist_add_asprintf(chunks, "p %s\n", summary);

  if (consensus_method >= MIN_METHOD_FOR_P6_LINES &&
      ri->ipv6_exit_policy) {
    /* XXXX024 This doesn't match proposal 208, which says these should
     * be taken unchanged from the routerinfo.  That's bogosity, IMO:
     * the proposal should have said to do this instead.*/
    char *p6 = write_short_policy(ri->ipv6_exit_policy);
    if (p6 && strcmp(p6, "reject 1-65535"))
      smartlist_add_asprintf(chunks, "p6 %s\n", p6);
    tor_free(p6);
  }

  if (consensus_method >= MIN_METHOD_FOR_ID_HASH_IN_MD) {
    char idbuf[BASE64_DIGEST_LEN+1];
    digest_to_base64(idbuf, ri->cache_info.identity_digest);
    smartlist_add_asprintf(chunks, "id rsa1024 %s\n", idbuf);
  }

  output = smartlist_join_strings(chunks, "", 0, NULL);

  {
    smartlist_t *lst = microdescs_parse_from_string(output,
                                                 output+strlen(output), 0,
                                                    SAVED_NOWHERE);
    if (smartlist_len(lst) != 1) {
      log_warn(LD_DIR, "We generated a microdescriptor we couldn't parse.");
      SMARTLIST_FOREACH(lst, microdesc_t *, md, microdesc_free(md));
      smartlist_free(lst);
      goto done;
    }
    result = smartlist_get(lst, 0);
    smartlist_free(lst);
  }

 done:
  tor_free(output);
  tor_free(key);
  tor_free(summary);
  tor_free(family);
  if (chunks) {
    SMARTLIST_FOREACH(chunks, char *, cp, tor_free(cp));
    smartlist_free(chunks);
  }
  return result;
}

/** Format the appropriate vote line to describe the microdescriptor <b>md</b>
 * in a consensus vote document.  Write it into the <b>out_len</b>-byte buffer
 * in <b>out</b>.  Return -1 on failure and the number of characters written
 * on success. */
ssize_t
dirvote_format_microdesc_vote_line(char *out_buf, size_t out_buf_len,
                                   const microdesc_t *md,
                                   int consensus_method_low,
                                   int consensus_method_high)
{
  ssize_t ret = -1;
  char d64[BASE64_DIGEST256_LEN+1];
  char *microdesc_consensus_methods =
    make_consensus_method_list(consensus_method_low,
                               consensus_method_high,
                               ",");
  tor_assert(microdesc_consensus_methods);

  if (digest256_to_base64(d64, md->digest)<0)
    goto out;

  if (tor_snprintf(out_buf, out_buf_len, "m %s sha256=%s\n",
                   microdesc_consensus_methods, d64)<0)
    goto out;

  ret = strlen(out_buf);

 out:
  tor_free(microdesc_consensus_methods);
  return ret;
}

/** Array of start and end of consensus methods used for supported
    microdescriptor formats. */
static const struct consensus_method_range_t {
  int low;
  int high;
} microdesc_consensus_methods[] = {
  {MIN_METHOD_FOR_MICRODESC, MIN_METHOD_FOR_A_LINES - 1},
  {MIN_METHOD_FOR_A_LINES, MIN_METHOD_FOR_P6_LINES - 1},
  {MIN_METHOD_FOR_P6_LINES, MIN_METHOD_FOR_NTOR_KEY - 1},
  {MIN_METHOD_FOR_NTOR_KEY, MIN_METHOD_FOR_ID_HASH_IN_MD - 1},
  {MIN_METHOD_FOR_ID_HASH_IN_MD,  MAX_SUPPORTED_CONSENSUS_METHOD},
  {-1, -1}
};

/** Helper type used when generating the microdescriptor lines in a directory
 * vote. */
typedef struct microdesc_vote_line_t {
  int low;
  int high;
  microdesc_t *md;
  struct microdesc_vote_line_t *next;
} microdesc_vote_line_t;

/** Generate and return a linked list of all the lines that should appear to
 * describe a router's microdescriptor versions in a directory vote.
 * Add the generated microdescriptors to <b>microdescriptors_out</b>. */
vote_microdesc_hash_t *
dirvote_format_all_microdesc_vote_lines(const routerinfo_t *ri, time_t now,
                                        smartlist_t *microdescriptors_out)
{
  const struct consensus_method_range_t *cmr;
  microdesc_vote_line_t *entries = NULL, *ep;
  vote_microdesc_hash_t *result = NULL;

  /* Generate the microdescriptors. */
  for (cmr = microdesc_consensus_methods;
       cmr->low != -1 && cmr->high != -1;
       cmr++) {
    microdesc_t *md = dirvote_create_microdescriptor(ri, cmr->low);
    if (md) {
      microdesc_vote_line_t *e =
        tor_malloc_zero(sizeof(microdesc_vote_line_t));
      e->md = md;
      e->low = cmr->low;
      e->high = cmr->high;
      e->next = entries;
      entries = e;
    }
  }

  /* Compress adjacent identical ones */
  for (ep = entries; ep; ep = ep->next) {
    while (ep->next &&
           fast_memeq(ep->md->digest, ep->next->md->digest, DIGEST256_LEN) &&
           ep->low == ep->next->high + 1) {
      microdesc_vote_line_t *next = ep->next;
      ep->low = next->low;
      microdesc_free(next->md);
      ep->next = next->next;
      tor_free(next);
    }
  }

  /* Format them into vote_microdesc_hash_t, and add to microdescriptors_out.*/
  while ((ep = entries)) {
    char buf[128];
    vote_microdesc_hash_t *h;
    dirvote_format_microdesc_vote_line(buf, sizeof(buf), ep->md,
                                       ep->low, ep->high);
    h = tor_malloc_zero(sizeof(vote_microdesc_hash_t));
    h->microdesc_hash_line = tor_strdup(buf);
    h->next = result;
    result = h;
    ep->md->last_listed = now;
    smartlist_add(microdescriptors_out, ep->md);
    entries = ep->next;
    tor_free(ep);
  }

  return result;
}

/** If <b>vrs</b> has a hash made for the consensus method <b>method</b> with
 * the digest algorithm <b>alg</b>, decode it and copy it into
 * <b>digest256_out</b> and return 0.  Otherwise return -1. */
int
vote_routerstatus_find_microdesc_hash(char *digest256_out,
                                      const vote_routerstatus_t *vrs,
                                      int method,
                                      digest_algorithm_t alg)
{
  /* XXXX only returns the sha256 method. */
  const vote_microdesc_hash_t *h;
  char mstr[64];
  size_t mlen;
  char dstr[64];

  tor_snprintf(mstr, sizeof(mstr), "%d", method);
  mlen = strlen(mstr);
  tor_snprintf(dstr, sizeof(dstr), " %s=",
               crypto_digest_algorithm_get_name(alg));

  for (h = vrs->microdesc; h; h = h->next) {
    const char *cp = h->microdesc_hash_line;
    size_t num_len;
    /* cp looks like \d+(,\d+)* (digesttype=val )+ .  Let's hunt for mstr in
     * the first part. */
    while (1) {
      num_len = strspn(cp, "1234567890");
      if (num_len == mlen && fast_memeq(mstr, cp, mlen)) {
        /* This is the line. */
        char buf[BASE64_DIGEST256_LEN+1];
        /* XXXX ignores extraneous stuff if the digest is too long.  This
         * seems harmless enough, right? */
        cp = strstr(cp, dstr);
        if (!cp)
          return -1;
        cp += strlen(dstr);
        strlcpy(buf, cp, sizeof(buf));
        return digest256_from_base64(digest256_out, buf);
      }
      if (num_len == 0 || cp[num_len] != ',')
        break;
      cp += num_len + 1;
    }
  }
  return -1;
}

