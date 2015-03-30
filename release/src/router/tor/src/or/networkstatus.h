/* Copyright (c) 2001 Matej Pfajfar.
 * Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file networkstatus.h
 * \brief Header file for networkstatus.c.
 **/

#ifndef TOR_NETWORKSTATUS_H
#define TOR_NETWORKSTATUS_H

void networkstatus_reset_warnings(void);
void networkstatus_reset_download_failures(void);
int router_reload_consensus_networkstatus(void);
void routerstatus_free(routerstatus_t *rs);
void networkstatus_vote_free(networkstatus_t *ns);
networkstatus_voter_info_t *networkstatus_get_voter_by_id(
                                       networkstatus_t *vote,
                                       const char *identity);
int networkstatus_check_consensus_signature(networkstatus_t *consensus,
                                            int warn);
int networkstatus_check_document_signature(const networkstatus_t *consensus,
                                           document_signature_t *sig,
                                           const authority_cert_t *cert);
int compare_digest_to_routerstatus_entry(const void *_key,
                                         const void **_member);
int compare_digest_to_vote_routerstatus_entry(const void *_key,
                                              const void **_member);
const routerstatus_t *networkstatus_vote_find_entry(networkstatus_t *ns,
                                              const char *digest);
routerstatus_t *networkstatus_vote_find_mutable_entry(networkstatus_t *ns,
                                              const char *digest);
int networkstatus_vote_find_entry_idx(networkstatus_t *ns,
                                      const char *digest, int *found_out);
download_status_t *router_get_dl_status_by_descriptor_digest(const char *d);
const routerstatus_t *router_get_consensus_status_by_id(const char *digest);
routerstatus_t *router_get_mutable_consensus_status_by_id(
                                   const char *digest);
const routerstatus_t *router_get_consensus_status_by_descriptor_digest(
                                   networkstatus_t *consensus,
                                   const char *digest);
routerstatus_t *router_get_mutable_consensus_status_by_descriptor_digest(
                                   networkstatus_t *consensus,
                                   const char *digest);
const routerstatus_t *router_get_consensus_status_by_nickname(
                                   const char *nickname,
                                   int warn_if_unnamed);
const char *networkstatus_get_router_digest_by_nickname(const char *nickname);
int networkstatus_nickname_is_unnamed(const char *nickname);
void networkstatus_consensus_download_failed(int status_code,
                                             const char *flavname);
void update_consensus_networkstatus_fetch_time(time_t now);
int should_delay_dir_fetches(const or_options_t *options,const char **msg_out);
void update_networkstatus_downloads(time_t now);
void update_certificate_downloads(time_t now);
int consensus_is_waiting_for_certs(void);
int client_would_use_router(const routerstatus_t *rs, time_t now,
                            const or_options_t *options);
networkstatus_t *networkstatus_get_latest_consensus(void);
networkstatus_t *networkstatus_get_latest_consensus_by_flavor(
                                                  consensus_flavor_t f);
networkstatus_t *networkstatus_get_live_consensus(time_t now);
networkstatus_t *networkstatus_get_reasonably_live_consensus(time_t now,
                                                             int flavor);
#define NSSET_FROM_CACHE 1
#define NSSET_WAS_WAITING_FOR_CERTS 2
#define NSSET_DONT_DOWNLOAD_CERTS 4
#define NSSET_ACCEPT_OBSOLETE 8
#define NSSET_REQUIRE_FLAVOR 16
int networkstatus_set_current_consensus(const char *consensus,
                                        const char *flavor,
                                        unsigned flags);
void networkstatus_note_certs_arrived(void);
void routers_update_all_from_networkstatus(time_t now, int dir_version);
void routers_update_status_from_consensus_networkstatus(smartlist_t *routers,
                                                        int reset_failures);
void signed_descs_update_status_from_consensus_networkstatus(
                                                         smartlist_t *descs);

char *networkstatus_getinfo_helper_single(const routerstatus_t *rs);
char *networkstatus_getinfo_by_purpose(const char *purpose_string, time_t now);
void networkstatus_dump_bridge_status_to_file(time_t now);
int32_t networkstatus_get_param(const networkstatus_t *ns,
                                const char *param_name,
                                int32_t default_val, int32_t min_val,
                                int32_t max_val);
int getinfo_helper_networkstatus(control_connection_t *conn,
                                 const char *question, char **answer,
                                 const char **errmsg);
int32_t networkstatus_get_bw_weight(networkstatus_t *ns, const char *weight,
                                    int32_t default_val);
const char *networkstatus_get_flavor_name(consensus_flavor_t flav);
int networkstatus_parse_flavor_name(const char *flavname);
void document_signature_free(document_signature_t *sig);
document_signature_t *document_signature_dup(const document_signature_t *sig);
void networkstatus_free_all(void);
int networkstatus_get_weight_scale_param(networkstatus_t *ns);

#ifdef NETWORKSTATUS_PRIVATE
STATIC void vote_routerstatus_free(vote_routerstatus_t *rs);
#endif

#endif

