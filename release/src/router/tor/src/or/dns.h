/* Copyright (c) 2001 Matej Pfajfar.
 * Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file dns.h
 * \brief Header file for dns.c.
 **/

#ifndef TOR_DNS_H
#define TOR_DNS_H

int dns_init(void);
int has_dns_init_failed(void);
void dns_free_all(void);
uint32_t dns_clip_ttl(uint32_t ttl);
int dns_reset(void);
void connection_dns_remove(edge_connection_t *conn);
void assert_connection_edge_not_dns_pending(edge_connection_t *conn);
void assert_all_pending_dns_resolves_ok(void);
MOCK_DECL(void,dns_cancel_pending_resolve,(const char *question));
int dns_resolve(edge_connection_t *exitconn);
void dns_launch_correctness_checks(void);
int dns_seems_to_be_broken(void);
int dns_seems_to_be_broken_for_ipv6(void);
void dns_reset_correctness_checks(void);
void dump_dns_mem_usage(int severity);

#ifdef DNS_PRIVATE
#include "dns_structs.h"

STATIC uint32_t dns_get_expiry_ttl(uint32_t ttl);

MOCK_DECL(STATIC int,dns_resolve_impl,(edge_connection_t *exitconn,
int is_resolve,or_circuit_t *oncirc, char **hostname_out,
int *made_connection_pending_out, cached_resolve_t **resolve_out));

MOCK_DECL(STATIC void,send_resolved_cell,(edge_connection_t *conn,
uint8_t answer_type,const cached_resolve_t *resolved));

MOCK_DECL(STATIC void,send_resolved_hostname_cell,(edge_connection_t *conn,
const char *hostname));

cached_resolve_t *dns_get_cache_entry(cached_resolve_t *query);
void dns_insert_cache_entry(cached_resolve_t *new_entry);

MOCK_DECL(STATIC int,
set_exitconn_info_from_resolve,(edge_connection_t *exitconn,
                                const cached_resolve_t *resolve,
                                char **hostname_out));

MOCK_DECL(STATIC int,
launch_resolve,(cached_resolve_t *resolve));

#endif

#endif

