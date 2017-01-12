/* Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2016, The Tor Project, Inc. */
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
                                        int flush, const char *source_dir);
void trusted_dirs_flush_certs_to_disk(void);
authority_cert_t *authority_cert_get_newest_by_id(const char *id_digest);
authority_cert_t *authority_cert_get_by_sk_digest(const char *sk_digest);
authority_cert_t *authority_cert_get_by_digests(const char *id_digest,
                                                const char *sk_digest);
void authority_cert_get_all(smartlist_t *certs_out);
void authority_cert_dl_failed(const char *id_digest,
                              const char *signing_key_digest, int status);
void authority_certs_fetch_missing(networkstatus_t *status, time_t now,
                                   const char *dir_hint);
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
int router_digest_is_fallback_dir(const char *digest);
MOCK_DECL(dir_server_t *, trusteddirserver_get_by_v3_auth_digest,
          (const char *d));
const routerstatus_t *router_pick_trusteddirserver(dirinfo_type_t type,
                                                   int flags);
const routerstatus_t *router_pick_fallback_dirserver(dirinfo_type_t type,
                                                     int flags);
int router_skip_or_reachability(const or_options_t *options, int try_ip_pref);
int router_get_my_share_of_directory_requests(double *v3_share_out);
void router_reset_status_download_failures(void);
int routers_have_same_or_addrs(const routerinfo_t *r1, const routerinfo_t *r2);
void router_add_running_nodes_to_smartlist(smartlist_t *sl, int allow_invalid,
                                           int need_uptime, int need_capacity,
                                           int need_guard, int need_desc,
                                           int pref_addr, int direct_conn);

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
MOCK_DECL(signed_descriptor_t *,router_get_by_extrainfo_digest,
          (const char *digest));
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

MOCK_DECL(smartlist_t *, list_authority_ids_with_downloads, (void));
MOCK_DECL(download_status_t *, id_only_download_status_for_authority_id,
          (const char *digest));
MOCK_DECL(smartlist_t *, list_sk_digests_for_authority_id,
          (const char *digest));
MOCK_DECL(download_status_t *, download_status_for_authority_id_and_sk,
    (const char *id_digest, const char *sk_digest));

static int WRA_WAS_ADDED(was_router_added_t s);
static int WRA_WAS_OUTDATED(was_router_added_t s);
static int WRA_WAS_REJECTED(was_router_added_t s);
static int WRA_NEVER_DOWNLOADABLE(was_router_added_t s);
/** Return true iff the outcome code in <b>s</b> indicates that the descriptor
 * was added. It might still be necessary to check whether the descriptor
 * generator should be notified.
 */
static inline int
WRA_WAS_ADDED(was_router_added_t s) {
  return s == ROUTER_ADDED_SUCCESSFULLY || s == ROUTER_ADDED_NOTIFY_GENERATOR;
}
/** Return true iff the outcome code in <b>s</b> indicates that the descriptor
 * was not added because it was either:
 * - not in the consensus
 * - neither in the consensus nor in any networkstatus document
 * - it was outdated.
 * - its certificates were expired.
 */
static inline int WRA_WAS_OUTDATED(was_router_added_t s)
{
  return (s == ROUTER_WAS_TOO_OLD ||
          s == ROUTER_IS_ALREADY_KNOWN ||
          s == ROUTER_NOT_IN_CONSENSUS ||
          s == ROUTER_NOT_IN_CONSENSUS_OR_NETWORKSTATUS ||
          s == ROUTER_CERTS_EXPIRED);
}
/** Return true iff the outcome code in <b>s</b> indicates that the descriptor
 * was flat-out rejected. */
static inline int WRA_WAS_REJECTED(was_router_added_t s)
{
  return (s == ROUTER_AUTHDIR_REJECTS);
}
/** Return true iff the outcome code in <b>s</b> indicates that the descriptor
 * was flat-out rejected. */
static inline int WRA_NEVER_DOWNLOADABLE(was_router_added_t s)
{
  return (s == ROUTER_AUTHDIR_REJECTS ||
          s == ROUTER_BAD_EI ||
          s == ROUTER_WAS_TOO_OLD ||
          s == ROUTER_CERTS_EXPIRED);
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
                       const tor_addr_port_t *addrport_ipv6,
                       const char *digest, const char *v3_auth_digest,
                       dirinfo_type_t type, double weight);
dir_server_t *fallback_dir_server_new(const tor_addr_t *addr,
                                      uint16_t dir_port, uint16_t or_port,
                                      const tor_addr_port_t *addrport_ipv6,
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
int routerinfo_incompatible_with_extrainfo(const crypto_pk_t *ri,
                                           extrainfo_t *ei,
                                           signed_descriptor_t *sd,
                                           const char **msg);
int routerinfo_has_curve25519_onion_key(const routerinfo_t *ri);
int routerstatus_version_supports_extend2_cells(const routerstatus_t *rs,
                                                int allow_unknown_versions);

void routerlist_assert_ok(const routerlist_t *rl);
const char *esc_router_info(const routerinfo_t *router);
void routers_sort_by_identity(smartlist_t *routers);

void refresh_all_country_info(void);

void list_pending_microdesc_downloads(digest256map_t *result);
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
STATIC int choose_array_element_by_weight(const uint64_t *entries,
                                          int n_entries);
STATIC void scale_array_elements_to_u64(uint64_t *entries_out,
                                        const double *entries_in,
                                        int n_entries,
                                        uint64_t *total_out);
STATIC const routerstatus_t *router_pick_directory_server_impl(
                                           dirinfo_type_t auth, int flags,
                                           int *n_busy_out);

MOCK_DECL(int, router_descriptor_is_older_than, (const routerinfo_t *router,
                                                 int seconds));
MOCK_DECL(STATIC was_router_added_t, extrainfo_insert,
          (routerlist_t *rl, extrainfo_t *ei, int warn_if_incompatible));

MOCK_DECL(STATIC void, initiate_descriptor_downloads,
          (const routerstatus_t *source, int purpose, smartlist_t *digests,
           int lo, int hi, int pds_flags));
STATIC int router_is_already_dir_fetching(const tor_addr_port_t *ap,
                                          int serverdesc, int microdesc);

#endif

#endif

