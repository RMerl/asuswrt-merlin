/* Copyright (c) 2001 Matej Pfajfar.
 * Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file nodelist.h
 * \brief Header file for nodelist.c.
 **/

#ifndef TOR_NODELIST_H
#define TOR_NODELIST_H

#define node_assert_ok(n) STMT_BEGIN {                          \
    tor_assert((n)->ri || (n)->rs);                             \
  } STMT_END

node_t *node_get_mutable_by_id(const char *identity_digest);
MOCK_DECL(const node_t *, node_get_by_id, (const char *identity_digest));
const node_t *node_get_by_hex_id(const char *identity_digest);
node_t *nodelist_set_routerinfo(routerinfo_t *ri, routerinfo_t **ri_old_out);
node_t *nodelist_add_microdesc(microdesc_t *md);
void nodelist_set_consensus(networkstatus_t *ns);

void nodelist_remove_microdesc(const char *identity_digest, microdesc_t *md);
void nodelist_remove_routerinfo(routerinfo_t *ri);
void nodelist_purge(void);
smartlist_t *nodelist_find_nodes_with_microdesc(const microdesc_t *md);

void nodelist_free_all(void);
void nodelist_assert_ok(void);

const node_t *node_get_by_nickname(const char *nickname, int warn_if_unnamed);
void node_get_verbose_nickname(const node_t *node,
                               char *verbose_name_out);
void node_get_verbose_nickname_by_id(const char *id_digest,
                                char *verbose_name_out);
int node_is_named(const node_t *node);
int node_is_dir(const node_t *node);
int node_has_descriptor(const node_t *node);
int node_get_purpose(const node_t *node);
#define node_is_bridge(node) \
  (node_get_purpose((node)) == ROUTER_PURPOSE_BRIDGE)
int node_is_me(const node_t *node);
int node_exit_policy_rejects_all(const node_t *node);
int node_exit_policy_is_exact(const node_t *node, sa_family_t family);
smartlist_t *node_get_all_orports(const node_t *node);
int node_allows_single_hop_exits(const node_t *node);
const char *node_get_nickname(const node_t *node);
const char *node_get_platform(const node_t *node);
uint32_t node_get_prim_addr_ipv4h(const node_t *node);
void node_get_address_string(const node_t *node, char *cp, size_t len);
long node_get_declared_uptime(const node_t *node);
time_t node_get_published_on(const node_t *node);
const smartlist_t *node_get_declared_family(const node_t *node);
int node_ipv6_preferred(const node_t *node);
int node_get_prim_orport(const node_t *node, tor_addr_port_t *ap_out);
void node_get_pref_orport(const node_t *node, tor_addr_port_t *ap_out);
void node_get_pref_ipv6_orport(const node_t *node, tor_addr_port_t *ap_out);
int node_has_curve25519_onion_key(const node_t *node);

smartlist_t *nodelist_get_list(void);

/* Temporary during transition to multiple addresses.  */
void node_get_addr(const node_t *node, tor_addr_t *addr_out);
#define node_get_addr_ipv4h(n) node_get_prim_addr_ipv4h((n))

void nodelist_refresh_countries(void);
void node_set_country(node_t *node);
void nodelist_add_node_and_family(smartlist_t *nodes, const node_t *node);
int nodes_in_same_family(const node_t *node1, const node_t *node2);

const node_t *router_find_exact_exit_enclave(const char *address,
                                             uint16_t port);
int node_is_unreliable(const node_t *router, int need_uptime,
                         int need_capacity, int need_guard);
int router_exit_policy_all_nodes_reject(const tor_addr_t *addr, uint16_t port,
                                        int need_uptime);
void router_set_status(const char *digest, int up);
int router_have_minimum_dir_info(void);
void router_dir_info_changed(void);
const char *get_dir_info_status_string(void);
int count_loading_descriptors_progress(void);

#endif

