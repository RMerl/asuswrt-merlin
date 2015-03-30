/* Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file routerlist.h
 * \brief Header file for routerlist.c.
 **/

#ifndef TOR_ROUTERLIST_H
#define TOR_ROUTERLIST_H

#include "testsupport.h"

int get_n_authorities(dirinfo_type_t type);
int trusted_dirs_reload_certs(void);

/*
 * Pass one of these as source to trusted_dirs_load_certs_from_string()
 * to indicate whence string originates; this controls error handling
 * behavior such as marking downloads as failed.
 */

#define TRUSTED_DIRS_CERTS_SRC_SELF 0
#define TRUSTED_DIRS_CERTS_SRC_FROM_STORE 1
#define TRUSTED_DIRS_CERTS_SRC_DL_BY_ID_DIGEST 2
#define TRUSTED_DIRS_CERTS_SRC_DL_BY_ID_SK_DIGEST 3
#define TRUSTED_DIRS_CERTS_SRC_FROM_VOTE 4

int trusted_dirs_load_certs_from_string(const char *contents, int source,
                                        int flush);
void trusted_dirs_flush_certs_to_disk(void);
authority_cert_t *authority_cert_get_newest_by_id(const char *id_digest);
authority_cert_t *authority_cert_get_by_sk_digest(const char *sk_digest);
authority_cert_t *authority_cert_get_by_digests(const char *id_digest,
                                                const char *sk_digest);
void authority_cert_get_all(smartlist_t *certs_out);
void authority_cert_dl_failed(const char *id_digest,
                              const char *signing_key_digest, int status);
void authority_certs_fetch_missing(networkstatus_t *status, time_t now);
int router_reload_router_list(void);
int authority_cert_dl_looks_uncertain(const char *id_digest);
const smartlist_t *router_get_trusted_dir_servers(void);
const smartlist_t *router_get_fallback_dir_servers(void);
int authority_cert_is_blacklisted(const authority_cert_t *cert);

const routerstatus_t *router_pick_directory_server(dirinfo_type_t type,
                                                   int flags);
dir_server_t *router_get_trusteddirserver_by_digest(const char *d);
dir_server_t *router_get_fallback_dirserver_by_digest(
                                                   const char *digest);
dir_server_t *trusteddirserver_get_by_v3_auth_digest(const char *d);
const routerstatus_t *router_pick_trusteddirserver(dirinfo_type_t type,
                                                   int flags);
const routerstatus_t *router_pick_fallback_dirserver(dirinfo_type_t type,
                                                     int flags);
int router_get_my_share_of_directory_requests(double *v3_share_out);
void router_reset_status_download_failures(void);
int routers_have_same_or_addrs(const routerinfo_t *r1, const routerinfo_t *r2);
const routerinfo_t *routerlist_find_my_routerinfo(void);
uint32_t router_get_advertised_bandwidth(const routerinfo_t *router);
uint32_t router_get_advertised_bandwidth_capped(const routerinfo_t *router);

const node_t *node_sl_choose_by_bandwidth(const smartlist_t *sl,
                                          bandwidth_weight_rule_t rule);
double frac_nodes_with_descriptors(const smartlist_t *sl,
                                   bandwidth_weight_rule_t rule);

const node_t *router_choose_random_node(smartlist_t *excludedsmartlist,
                                        struct routerset_t *excludedset,
                                        router_crn_flags_t flags);

int router_is_named(const routerinfo_t *router);
int router_digest_is_trusted_dir_type(const char *digest,
                                      dirinfo_type_t type);
#define router_digest_is_trusted_dir(d) \
  router_digest_is_trusted_dir_type((d), NO_DIRINFO)

int router_addr_is_trusted_dir(uint32_t addr);
int hexdigest_to_digest(const char *hexdigest, char *digest);
const routerinfo_t *router_get_by_id_digest(const char *digest);
routerinfo_t *router_get_mutable_by_digest(const char *digest);
signed_descriptor_t *router_get_by_descriptor_digest(const char *digest);
signed_descriptor_t *router_get_by_extrainfo_digest(const char *digest);
signed_descriptor_t *extrainfo_get_by_descriptor_digest(const char *digest);
const char *signed_descriptor_get_body(const signed_descriptor_t *desc);
const char *signed_descriptor_get_annotations(const signed_descriptor_t *desc);
routerlist_t *router_get_routerlist(void);
void routerinfo_free(routerinfo_t *router);
void extrainfo_free(extrainfo_t *extrainfo);
void routerlist_free(routerlist_t *rl);
void dump_routerlist_mem_usage(int severity);
void routerlist_remove(routerlist_t *rl, routerinfo_t *ri, int make_old,
                       time_t now);
void routerlist_free_all(void);
void routerlist_reset_warnings(void);

static int WRA_WAS_ADDED(was_router_added_t s);
static int WRA_WAS_OUTDATED(was_router_added_t s);
static int WRA_WAS_REJECTED(was_router_added_t s);
/** Return true iff the outcome code in <b>s</b> indicates that the descriptor
 * was added. It might still be necessary to check whether the descriptor
 * generator should be notified.
 */
static INLINE int
WRA_WAS_ADDED(was_router_added_t s) {
  return s == ROUTER_ADDED_SUCCESSFULLY || s == ROUTER_ADDED_NOTIFY_GENERATOR;
}
/** Return true iff the outcome code in <b>s</b> indicates that the descriptor
 * was not added because it was either:
 * - not in the consensus
 * - neither in the consensus nor in any networkstatus document
 * - it was outdated.
 */
static INLINE int WRA_WAS_OUTDATED(was_router_added_t s)
{
  return (s == ROUTER_WAS_NOT_NEW ||
          s == ROUTER_NOT_IN_CONSENSUS ||
          s == ROUTER_NOT_IN_CONSENSUS_OR_NETWORKSTATUS);
}
/** Return true iff the outcome code in <b>s</b> indicates that the descriptor
 * was flat-out rejected. */
static INLINE int WRA_WAS_REJECTED(was_router_added_t s)
{
  return (s == ROUTER_AUTHDIR_REJECTS);
}
was_router_added_t router_add_to_routerlist(routerinfo_t *router,
                                            const char **msg,
                                            int from_cache,
                                            int from_fetch);
was_router_added_t router_add_extrainfo_to_routerlist(
                                        extrainfo_t *ei, const char **msg,
                                        int from_cache, int from_fetch);
void routerlist_descriptors_added(smartlist_t *sl, int from_cache);
void routerlist_remove_old_routers(void);
int router_load_single_router(const char *s, uint8_t purpose, int cache,
                              const char **msg);
int router_load_routers_from_string(const char *s, const char *eos,
                                     saved_location_t saved_location,
                                     smartlist_t *requested_fingerprints,
                                     int descriptor_digests,
                                     const char *prepend_annotations);
void router_load_extrainfo_from_string(const char *s, const char *eos,
                                       saved_location_t saved_location,
                                       smartlist_t *requested_fingerprints,
                                       int descriptor_digests);

void routerlist_retry_directory_downloads(time_t now);

int router_exit_policy_rejects_all(const routerinfo_t *router);

dir_server_t *trusted_dir_server_new(const char *nickname, const char *address,
                       uint16_t dir_port, uint16_t or_port,
                       const char *digest, const char *v3_auth_digest,
                       dirinfo_type_t type, double weight);
dir_server_t *fallback_dir_server_new(const tor_addr_t *addr,
                                      uint16_t dir_port, uint16_t or_port,
                                      const char *id_digest, double weight);
void dir_server_add(dir_server_t *ent);

void authority_cert_free(authority_cert_t *cert);
void clear_dir_servers(void);
void update_consensus_router_descriptor_downloads(time_t now, int is_vote,
                                                  networkstatus_t *consensus);
void update_router_descriptor_downloads(time_t now);
void update_all_descriptor_downloads(time_t now);
void update_extrainfo_downloads(time_t now);
void router_reset_descriptor_download_failures(void);
int router_differences_are_cosmetic(const routerinfo_t *r1,
                                    const routerinfo_t *r2);
int routerinfo_incompatible_with_extrainfo(const routerinfo_t *ri,
                                           extrainfo_t *ei,
                                           signed_descriptor_t *sd,
                                           const char **msg);

void routerlist_assert_ok(const routerlist_t *rl);
const char *esc_router_info(const routerinfo_t *router);
void routers_sort_by_identity(smartlist_t *routers);

void refresh_all_country_info(void);

int hid_serv_get_responsible_directories(smartlist_t *responsible_dirs,
                                         const char *id);
int hid_serv_acting_as_directory(void);
int hid_serv_responsible_for_desc_id(const char *id);

void list_pending_microdesc_downloads(digestmap_t *result);
void launch_descriptor_downloads(int purpose,
                                 smartlist_t *downloadable,
                                 const routerstatus_t *source,
                                 time_t now);

int hex_digest_nickname_decode(const char *hexdigest,
                               char *digest_out,
                               char *nickname_qualifier_out,
                               char *nickname_out);
int hex_digest_nickname_matches(const char *hexdigest,
                                const char *identity_digest,
                                const char *nickname, int is_named);

#ifdef ROUTERLIST_PRIVATE
/** Helper type for choosing routers by bandwidth: contains a union of
 * double and uint64_t. Before we call scale_array_elements_to_u64, it holds
 * a double; after, it holds a uint64_t. */
typedef union u64_dbl_t {
  uint64_t u64;
  double dbl;
} u64_dbl_t;

STATIC int choose_array_element_by_weight(const u64_dbl_t *entries,
                                          int n_entries);
STATIC void scale_array_elements_to_u64(u64_dbl_t *entries, int n_entries,
                                        uint64_t *total_out);
#endif

#endif

