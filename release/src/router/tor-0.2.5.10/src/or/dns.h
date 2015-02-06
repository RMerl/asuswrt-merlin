/* Copyright (c) 2001 Matej Pfajfar.
 * Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2013, The Tor Project, Inc. */
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
void dns_cancel_pending_resolve(const char *question);
int dns_resolve(edge_connection_t *exitconn);
void dns_launch_correctness_checks(void);
int dns_seems_to_be_broken(void);
int dns_seems_to_be_broken_for_ipv6(void);
void dns_reset_correctness_checks(void);
void dump_dns_mem_usage(int severity);

#endif

