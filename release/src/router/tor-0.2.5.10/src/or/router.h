/* Copyright (c) 2001 Matej Pfajfar.
 * Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file router.h
 * \brief Header file for router.c.
 **/

#ifndef TOR_ROUTER_H
#define TOR_ROUTER_H

#include "testsupport.h"

crypto_pk_t *get_onion_key(void);
time_t get_onion_key_set_at(void);
void set_server_identity_key(crypto_pk_t *k);
crypto_pk_t *get_server_identity_key(void);
int server_identity_key_is_set(void);
void set_client_identity_key(crypto_pk_t *k);
crypto_pk_t *get_tlsclient_identity_key(void);
int client_identity_key_is_set(void);
authority_cert_t *get_my_v3_authority_cert(void);
crypto_pk_t *get_my_v3_authority_signing_key(void);
authority_cert_t *get_my_v3_legacy_cert(void);
crypto_pk_t *get_my_v3_legacy_signing_key(void);
void dup_onion_keys(crypto_pk_t **key, crypto_pk_t **last);
void rotate_onion_key(void);
crypto_pk_t *init_key_from_file(const char *fname, int generate,
                                    int severity);
void v3_authority_check_key_expiry(void);

#ifdef CURVE25519_ENABLED
di_digest256_map_t *construct_ntor_key_map(void);
void ntor_key_map_free(di_digest256_map_t *map);
#endif

int router_initialize_tls_context(void);
int init_keys(void);

int check_whether_orport_reachable(void);
int check_whether_dirport_reachable(void);
void consider_testing_reachability(int test_or, int test_dir);
void router_orport_found_reachable(void);
void router_dirport_found_reachable(void);
void router_perform_bandwidth_test(int num_circs, time_t now);

int net_is_disabled(void);

int authdir_mode(const or_options_t *options);
int authdir_mode_v3(const or_options_t *options);
int authdir_mode_any_main(const or_options_t *options);
int authdir_mode_any_nonhidserv(const or_options_t *options);
int authdir_mode_handles_descs(const or_options_t *options, int purpose);
int authdir_mode_publishes_statuses(const or_options_t *options);
int authdir_mode_tests_reachability(const or_options_t *options);
int authdir_mode_bridge(const or_options_t *options);

uint16_t router_get_active_listener_port_by_type_af(int listener_type,
                                                    sa_family_t family);
uint16_t router_get_advertised_or_port(const or_options_t *options);
uint16_t router_get_advertised_or_port_by_af(const or_options_t *options,
                                             sa_family_t family);
uint16_t router_get_advertised_dir_port(const or_options_t *options,
                                        uint16_t dirport);

MOCK_DECL(int, server_mode, (const or_options_t *options));
MOCK_DECL(int, public_server_mode, (const or_options_t *options));
int advertised_server_mode(void);
int proxy_mode(const or_options_t *options);
void consider_publishable_server(int force);
int should_refuse_unknown_exits(const or_options_t *options);

void router_upload_dir_desc_to_dirservers(int force);
void mark_my_descriptor_dirty_if_too_old(time_t now);
void mark_my_descriptor_dirty(const char *reason);
void check_descriptor_bandwidth_changed(time_t now);
void check_descriptor_ipaddress_changed(time_t now);
void router_new_address_suggestion(const char *suggestion,
                                   const dir_connection_t *d_conn);
int router_compare_to_my_exit_policy(const tor_addr_t *addr, uint16_t port);
int router_my_exit_policy_is_reject_star(void);
MOCK_DECL(const routerinfo_t *, router_get_my_routerinfo, (void));
extrainfo_t *router_get_my_extrainfo(void);
const char *router_get_my_descriptor(void);
const char *router_get_descriptor_gen_reason(void);
int router_digest_is_me(const char *digest);
const uint8_t *router_get_my_id_digest(void);
int router_extrainfo_digest_is_me(const char *digest);
int router_is_me(const routerinfo_t *router);
int router_pick_published_address(const or_options_t *options, uint32_t *addr);
int router_rebuild_descriptor(int force);
char *router_dump_router_to_string(routerinfo_t *router,
                                   crypto_pk_t *ident_key);
char *router_dump_exit_policy_to_string(const routerinfo_t *router,
                                         int include_ipv4,
                                         int include_ipv6);
void router_get_prim_orport(const routerinfo_t *router,
                            tor_addr_port_t *addr_port_out);
void router_get_pref_orport(const routerinfo_t *router,
                            tor_addr_port_t *addr_port_out);
void router_get_pref_ipv6_orport(const routerinfo_t *router,
                                 tor_addr_port_t *addr_port_out);
int router_ipv6_preferred(const routerinfo_t *router);
int router_has_addr(const routerinfo_t *router, const tor_addr_t *addr);
int router_has_orport(const routerinfo_t *router,
                      const tor_addr_port_t *orport);
int extrainfo_dump_to_string(char **s, extrainfo_t *extrainfo,
                             crypto_pk_t *ident_key);
int is_legal_nickname(const char *s);
int is_legal_nickname_or_hexdigest(const char *s);
int is_legal_hexdigest(const char *s);

/**
 * Longest allowed output of format_node_description, plus 1 character for
 * NUL.  This allows space for:
 * "$FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF~xxxxxxxxxxxxxxxxxxx at"
 * " [ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255]"
 * plus a terminating NUL.
 */
#define NODE_DESC_BUF_LEN (MAX_VERBOSE_NICKNAME_LEN+4+TOR_ADDR_BUF_LEN)
const char *format_node_description(char *buf,
                                    const char *id_digest,
                                    int is_named,
                                    const char *nickname,
                                    const tor_addr_t *addr,
                                    uint32_t addr32h);
const char *router_get_description(char *buf, const routerinfo_t *ri);
const char *node_get_description(char *buf, const node_t *node);
const char *routerstatus_get_description(char *buf, const routerstatus_t *rs);
const char *extend_info_get_description(char *buf, const extend_info_t *ei);
const char *router_describe(const routerinfo_t *ri);
const char *node_describe(const node_t *node);
const char *routerstatus_describe(const routerstatus_t *ri);
const char *extend_info_describe(const extend_info_t *ei);

void router_get_verbose_nickname(char *buf, const routerinfo_t *router);
void router_reset_warnings(void);
void router_reset_reachability(void);
void router_free_all(void);

const char *router_purpose_to_string(uint8_t p);
uint8_t router_purpose_from_string(const char *s);

smartlist_t *router_get_all_orports(const routerinfo_t *ri);

#ifdef ROUTER_PRIVATE
/* Used only by router.c and test.c */
STATIC void get_platform_str(char *platform, size_t len);
STATIC int router_write_fingerprint(int hashed);
#endif

#endif

