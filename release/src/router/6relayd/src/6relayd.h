/**
 * Copyright (C) 2012-2013 Steven Barth <steven@midlink.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#pragma once
#include <netinet/in.h>
#include <netinet/icmp6.h>
#include <net/if.h>
#include <stdbool.h>
#include <syslog.h>

#include "list.h"
#include "stubs.h"

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

// RFC 6106 defines this router advertisement option
#define ND_OPT_ROUTE_INFO 24
#define ND_OPT_RECURSIVE_DNS 25
#define ND_OPT_DNS_SEARCH 31

#define RELAYD_BUFFER_SIZE 8192
#define RELAYD_MAX_PREFIXES 8

#define _unused __attribute__((unused))
#define _packed __attribute__((packed))


#define ALL_IPV6_NODES {{{0xff, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,\
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01}}}

#define ALL_IPV6_ROUTERS {{{0xff, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,\
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02}}}


struct relayd_interface;

struct relayd_event {
	int socket;
	void (*handle_event)(struct relayd_event *event);
	void (*handle_dgram)(void *addr, void *data, size_t len,
			struct relayd_interface *iface);
};


struct relayd_ipaddr {
	struct in6_addr addr;
	uint8_t prefix;
	uint32_t preferred;
	uint32_t valid;
};


struct relayd_interface {
	int ifindex;
	char ifname[IF_NAMESIZE];
	uint8_t mac[6];
	bool external;

	struct relayd_event timer_rs;

	// IPv6 PD
	struct list_head pd_assignments;
	struct relayd_ipaddr pd_addr[8];
	size_t pd_addr_len;
	bool pd_reconf;
};

#define RELAYD_MANAGED_MFLAG	1
#define RELAYD_MANAGED_NO_AFLAG	2

struct relayd_config {
	// Config
	bool enable_router_discovery_relay;
	bool enable_router_discovery_server;
	bool enable_dhcpv6_relay;
	bool enable_dhcpv6_server;
	bool enable_ndp_relay;
	bool enable_route_learning;

	bool send_router_solicitation;
	bool always_rewrite_dns;
	bool always_announce_default_router;
	bool deprecate_ula_if_public_avail;
	bool ra_not_onlink;
	int ra_managed_mode;
	int ra_preference;

	struct in6_addr dnsaddr;
	struct relayd_interface master;
	struct relayd_interface *slaves;
	size_t slavecount;

	char *dhcpv6_cb;
	char *dhcpv6_statefile;
	char** dhcpv6_lease;
	size_t dhcpv6_lease_len;

	char** static_ndp;
	size_t static_ndp_len;
};


// Exported main functions
int relayd_open_rtnl_socket(void);
int relayd_register_event(struct relayd_event *event);
ssize_t relayd_forward_packet(int socket, struct sockaddr_in6 *dest,
		struct iovec *iov, size_t iov_len,
		const struct relayd_interface *iface);
ssize_t relayd_get_interface_addresses(int ifindex,
		struct relayd_ipaddr *addrs, size_t cnt);
struct relayd_interface* relayd_get_interface_by_name(const char *name);
int relayd_get_interface_mtu(const char *ifname);
int relayd_get_interface_mac(const char *ifname, uint8_t mac[6]);
struct relayd_interface* relayd_get_interface_by_index(int ifindex);
void relayd_urandom(void *data, size_t len);
void relayd_setup_route(const struct in6_addr *addr, int prefixlen,
		const struct relayd_interface *iface, const struct in6_addr *gw, bool add);


// Exported module initializers
int init_router_discovery_relay(const struct relayd_config *relayd_config);
int init_dhcpv6_relay(const struct relayd_config *relayd_config);
int init_ndp_proxy(const struct relayd_config *relayd_config);

void deinit_router_discovery_relay(void);
void deinit_ndp_proxy();
