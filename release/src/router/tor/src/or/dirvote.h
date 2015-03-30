/* Copyright (c) 2001 Matej Pfajfar.
 * Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file dirvote.h
 * \brief Header file for dirvote.c.
 **/

#ifndef TOR_DIRVOTE_H
#define TOR_DIRVOTE_H

#include "testsupport.h"

/** Lowest allowable value for VoteSeconds. */
#define MIN_VOTE_SECONDS 2
/** Lowest allowable value for DistSeconds. */
#define MIN_DIST_SECONDS 2
/** Smallest allowable voting interval. */
#define MIN_VOTE_INTERVAL 300

/** The highest consensus method that we currently support. */
#define MAX_SUPPORTED_CONSENSUS_METHOD 18

/** Lowest consensus method that contains a 'directory-footer' marker */
#define MIN_METHOD_FOR_FOOTER 9

/** Lowest consensus method that contains bandwidth weights */
#define MIN_METHOD_FOR_BW_WEIGHTS 9

/** Lowest consensus method that contains consensus params */
#define MIN_METHOD_FOR_PARAMS 7

/** Lowest consensus method that generates microdescriptors */
#define MIN_METHOD_FOR_MICRODESC 8

/** Lowest consensus method that doesn't count bad exits as exits for weight */
#define MIN_METHOD_TO_CUT_BADEXIT_WEIGHT 11

/** Lowest consensus method that ensures a majority of authorities voted
  * for a param. */
#define MIN_METHOD_FOR_MAJORITY_PARAMS 12

/** Lowest consensus method where microdesc consensuses omit any entry
 * with no microdesc. */
#define MIN_METHOD_FOR_MANDATORY_MICRODESC 13

/** Lowest consensus method that contains "a" lines. */
#define MIN_METHOD_FOR_A_LINES 14

/** Lowest consensus method where microdescs may include a "p6" line. */
#define MIN_METHOD_FOR_P6_LINES 15

/** Lowest consensus method where microdescs may include an onion-key-ntor
 * line */
#define MIN_METHOD_FOR_NTOR_KEY 16

/** Lowest consensus method that ensures that authorities output an
 * Unmeasured=1 flag for unmeasured bandwidths */
#define MIN_METHOD_TO_CLIP_UNMEASURED_BW 17

/** Lowest consensus method where authorities may include an "id" line in
 * microdescriptors. */
#define MIN_METHOD_FOR_ID_HASH_IN_MD 18

/** Default bandwidth to clip unmeasured bandwidths to using method >=
 * MIN_METHOD_TO_CLIP_UNMEASURED_BW */
#define DEFAULT_MAX_UNMEASURED_BW_KB 20

void dirvote_free_all(void);

/* vote manipulation */
char *networkstatus_compute_consensus(smartlist_t *votes,
                                      int total_authorities,
                                      crypto_pk_t *identity_key,
                                      crypto_pk_t *signing_key,
                                      const char *legacy_identity_key_digest,
                                      crypto_pk_t *legacy_signing_key,
                                      consensus_flavor_t flavor);
int networkstatus_add_detached_signatures(networkstatus_t *target,
                                          ns_detached_signatures_t *sigs,
                                          const char *source,
                                          int severity,
                                          const char **msg_out);
char *networkstatus_get_detached_signatures(smartlist_t *consensuses);
void ns_detached_signatures_free(ns_detached_signatures_t *s);

/* cert manipulation */
authority_cert_t *authority_cert_dup(authority_cert_t *cert);

/* vote scheduling */
void dirvote_get_preferred_voting_intervals(vote_timing_t *timing_out);
time_t dirvote_get_start_of_next_interval(time_t now,
                                          int interval,
                                          int offset);
void dirvote_recalculate_timing(const or_options_t *options, time_t now);
void dirvote_act(const or_options_t *options, time_t now);

/* invoked on timers and by outside triggers. */
struct pending_vote_t * dirvote_add_vote(const char *vote_body,
                                         const char **msg_out,
                                         int *status_out);
int dirvote_add_signatures(const char *detached_signatures_body,
                           const char *source,
                           const char **msg_out);

/* Item access */
const char *dirvote_get_pending_consensus(consensus_flavor_t flav);
const char *dirvote_get_pending_detached_signatures(void);
#define DGV_BY_ID 1
#define DGV_INCLUDE_PENDING 2
#define DGV_INCLUDE_PREVIOUS 4
const cached_dir_t *dirvote_get_vote(const char *fp, int flags);
void set_routerstatus_from_routerinfo(routerstatus_t *rs,
                                      node_t *node,
                                      routerinfo_t *ri, time_t now,
                                      int naming, int listbadexits,
                                      int listbaddirs, int vote_on_hsdirs);
networkstatus_t *
dirserv_generate_networkstatus_vote_obj(crypto_pk_t *private_key,
                                        authority_cert_t *cert);

microdesc_t *dirvote_create_microdescriptor(const routerinfo_t *ri,
                                            int consensus_method);
ssize_t dirvote_format_microdesc_vote_line(char *out, size_t out_len,
                                           const microdesc_t *md,
                                           int consensus_method_low,
                                           int consensus_method_high);
vote_microdesc_hash_t *dirvote_format_all_microdesc_vote_lines(
                                        const routerinfo_t *ri,
                                        time_t now,
                                        smartlist_t *microdescriptors_out);

int vote_routerstatus_find_microdesc_hash(char *digest256_out,
                                          const vote_routerstatus_t *vrs,
                                          int method,
                                          digest_algorithm_t alg);
document_signature_t *voter_get_sig_by_algorithm(
                           const networkstatus_voter_info_t *voter,
                           digest_algorithm_t alg);

#ifdef DIRVOTE_PRIVATE
STATIC char *format_networkstatus_vote(crypto_pk_t *private_key,
                                 networkstatus_t *v3_ns);
STATIC char *dirvote_compute_params(smartlist_t *votes, int method,
                             int total_authorities);
#endif

#endif

