/**
 * Copyright (C) 2012-2014 Steven Barth <steven@midlink.org>
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

#include <fcntl.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <stddef.h>
#include <stdbool.h>
#include <syslog.h>
#include <unistd.h>
#include <resolv.h>
#include <alloca.h>

#include <net/if.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/icmp6.h>

#include <linux/rtnetlink.h>

#ifndef SOL_NETLINK
#define SOL_NETLINK 270
#endif

#ifndef NETLINK_ADD_MEMBERSHIP
#define NETLINK_ADD_MEMBERSHIP 1
#endif

#ifndef IFF_LOWER_UP
#define IFF_LOWER_UP 0x10000
#endif

#include "odhcp6c.h"
#include "ra.h"


static bool nocarrier = false;

static int sock = -1, rtnl = -1;
static int if_index = 0;
static char if_name[IF_NAMESIZE] = {0};
static volatile int rs_attempt = 0;
static struct in6_addr lladdr = IN6ADDR_ANY_INIT;

struct {
	struct icmp6_hdr hdr;
	struct icmpv6_opt lladdr;
} rs = {
	.hdr = {ND_ROUTER_SOLICIT, 0, 0, {{0}}},
	.lladdr = {ND_OPT_SOURCE_LINKADDR, 1, {0}},
};


static void ra_send_rs(int signal __attribute__((unused)));

int ra_init(const char *ifname, const struct in6_addr *ifid)
{
	const pid_t ourpid = getpid();
#ifdef SOCK_CLOEXEC
	sock = socket(AF_INET6, SOCK_RAW | SOCK_CLOEXEC, IPPROTO_ICMPV6);
#else
	sock = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
	sock = fflags(sock, O_CLOEXEC);
#endif
	if (sock < 0)
		return -1;

	if_index = if_nametoindex(ifname);
	if (!if_index)
		return -1;

	strncpy(if_name, ifname, sizeof(if_name) - 1);
	lladdr = *ifid;

#ifdef SOCK_CLOEXEC
	rtnl = socket(AF_NETLINK, SOCK_DGRAM | SOCK_CLOEXEC, NETLINK_ROUTE);
#else
	rtnl = socket(AF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE);
	rtnl = fflags(rtnl, O_CLOEXEC);
#endif
	if (rtnl < 0)
		return -1;

	struct sockaddr_nl rtnl_kernel = { .nl_family = AF_NETLINK };
	if (connect(rtnl, (const struct sockaddr*)&rtnl_kernel, sizeof(rtnl_kernel)) < 0)
		return -1;

	int val = RTNLGRP_LINK;
	setsockopt(rtnl, SOL_NETLINK, NETLINK_ADD_MEMBERSHIP, &val, sizeof(val));
	fcntl(rtnl, F_SETOWN, ourpid);
	fcntl(rtnl, F_SETFL, fcntl(sock, F_GETFL) | O_ASYNC);

	struct {
		struct nlmsghdr hdr;
		struct ifinfomsg ifi;
	} req = {
		.hdr = {sizeof(req), RTM_GETLINK, NLM_F_REQUEST, 1, 0},
		.ifi = {.ifi_index = if_index}
	};
	send(rtnl, &req, sizeof(req), 0);
	ra_link_up();

	// Filter ICMPv6 package types
	struct icmp6_filter filt;
	ICMP6_FILTER_SETBLOCKALL(&filt);
	ICMP6_FILTER_SETPASS(ND_ROUTER_ADVERT, &filt);
	setsockopt(sock, IPPROTO_ICMPV6, ICMP6_FILTER, &filt, sizeof(filt));

	// Bind to all-nodes
	struct ipv6_mreq an = {ALL_IPV6_NODES, if_index};
	setsockopt(sock, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, &an, sizeof(an));

	// Let the kernel compute our checksums
	val = 2;
	setsockopt(sock, IPPROTO_RAW, IPV6_CHECKSUM, &val, sizeof(val));

	// This is required by RFC 4861
	val = 255;
	setsockopt(sock, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &val, sizeof(val));

	// Receive multicast hops
	val = 1;
	setsockopt(sock, IPPROTO_IPV6, IPV6_RECVHOPLIMIT, &val, sizeof(val));

	// Bind to one device
	setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, ifname, strlen(ifname));

	// Add async-mode
	fcntl(sock, F_SETOWN, ourpid);
	fcntl(sock, F_SETFL, fcntl(sock, F_GETFL) | O_ASYNC);

	// Send RS
	signal(SIGALRM, ra_send_rs);
	ra_send_rs(SIGALRM);

	return 0;
}


static void ra_send_rs(int signal __attribute__((unused)))
{
	const struct sockaddr_in6 dest = {AF_INET6, 0, 0, ALL_IPV6_ROUTERS, if_index};
	const struct icmpv6_opt llnull = {ND_OPT_SOURCE_LINKADDR, 1, {0}};
	size_t len;

	if ((rs_attempt % 2 == 0) && memcmp(&rs.lladdr, &llnull, sizeof(llnull)))
		len = sizeof(rs);
	else
		len = sizeof(struct icmp6_hdr);

	sendto(sock, &rs, len, MSG_DONTWAIT, (struct sockaddr*)&dest, sizeof(dest));

	if (++rs_attempt <= 3)
		alarm(4);
}


static int16_t pref_to_priority(uint8_t flags)
{
	flags = (flags >> 3) & 0x03;
	return (flags == 0x0) ? 512 : (flags == 0x1) ? 384 :
			(flags == 0x3) ? 640 : -1;
}


bool ra_link_up(void)
{
	static bool firstcall = true;
	struct {
		struct nlmsghdr hdr;
		struct ifinfomsg msg;
		uint8_t pad[4000];
	} resp;

	bool ret = false;
	ssize_t read;

	do {
		read = recv(rtnl, &resp, sizeof(resp), MSG_DONTWAIT);
		if (read < 0 || !NLMSG_OK(&resp.hdr, (size_t)read) ||
				resp.hdr.nlmsg_type != RTM_NEWLINK ||
				resp.msg.ifi_index != if_index)
			continue;

		ssize_t alen = NLMSG_PAYLOAD(&resp.hdr, sizeof(resp.msg));
		for (struct rtattr *rta = (struct rtattr*)(resp.pad);
				RTA_OK(rta, alen); rta = RTA_NEXT(rta, alen)) {
			if (rta->rta_type == IFLA_ADDRESS &&
					RTA_PAYLOAD(rta) >= sizeof(rs.lladdr.data))
				memcpy(rs.lladdr.data, RTA_DATA(rta), sizeof(rs.lladdr.data));
		}

		bool hascarrier = resp.msg.ifi_flags & IFF_LOWER_UP;
		if (!firstcall && nocarrier != !hascarrier)
			ret = true;

		nocarrier = !hascarrier;
		firstcall = false;
	} while (read > 0);

	if (ret) {
		syslog(LOG_NOTICE, "carrier => %i event on %s", (int)!nocarrier, if_name);

		rs_attempt = 0;
		ra_send_rs(SIGALRM);
	}

	return ret;
}

static bool ra_icmpv6_valid(struct sockaddr_in6 *source, int hlim, uint8_t *data, size_t len)
{
	struct icmp6_hdr *hdr = (struct icmp6_hdr*)data;
	struct icmpv6_opt *opt, *end = (struct icmpv6_opt*)&data[len];

	if (hlim != 255 || len < sizeof(*hdr) || hdr->icmp6_code)
		return false;

	switch (hdr->icmp6_type) {
	case ND_ROUTER_ADVERT:
		if (!IN6_IS_ADDR_LINKLOCAL(&source->sin6_addr))
			return false;

		opt = (struct icmpv6_opt*)((struct nd_router_advert*)data + 1);
		break;

	default:
		return false;
	}

	icmpv6_for_each_option(opt, opt, end)
		;

	return opt == end;
}

int ra_conf_hoplimit(int newvalue)
{
	static int value = 0;
	if (newvalue > 0)
		value = newvalue;
	return value;
}

int ra_conf_mtu(int newvalue)
{
	static int value = 0;
	if (newvalue >= 1280 && newvalue <= 65535)
		value = newvalue;
	return value;
}

int ra_conf_reachable(int newvalue)
{
	static int value = 0;
	if (newvalue > 0 && newvalue <= 3600000)
		value = newvalue;
	return value;
}

int ra_conf_retransmit(int newvalue)
{
	static int value = 0;
	if (newvalue > 0 && newvalue <= 60000)
		value = newvalue;
	return value;
}

bool ra_process(void)
{
	bool found = false;
	bool changed = false;
	uint8_t buf[1500], cmsg_buf[128];
	struct nd_router_advert *adv = (struct nd_router_advert*)buf;
	struct odhcp6c_entry *entry = alloca(sizeof(*entry) + 256);
	const struct in6_addr any = IN6ADDR_ANY_INIT;

	memset(entry, 0, sizeof(*entry));

	if (IN6_IS_ADDR_UNSPECIFIED(&lladdr)) {
		struct sockaddr_in6 addr = {AF_INET6, 0, 0, ALL_IPV6_ROUTERS, if_index};
		socklen_t alen = sizeof(addr);
		int sock = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);

		if (!connect(sock, (struct sockaddr*)&addr, sizeof(addr)) &&
				!getsockname(sock, (struct sockaddr*)&addr, &alen))
			lladdr = addr.sin6_addr;

		close(sock);
	}

	while (true) {
		struct sockaddr_in6 from;
		struct iovec iov = {buf, sizeof(buf)};
		struct msghdr msg = {
			.msg_name = (void *) &from,
			.msg_namelen = sizeof(from),
			.msg_iov = &iov,
			.msg_iovlen = 1,
			.msg_control = cmsg_buf,
			.msg_controllen = sizeof(cmsg_buf),
			.msg_flags = 0
		};

		ssize_t len = recvmsg(sock, &msg, MSG_DONTWAIT);
		if (len <= 0)
			break;

		if (IN6_IS_ADDR_UNSPECIFIED(&lladdr))
			continue;

		int hlim = 0;
		for (struct cmsghdr *ch = CMSG_FIRSTHDR(&msg); ch != NULL;
				ch = CMSG_NXTHDR(&msg, ch))
			if (ch->cmsg_level == IPPROTO_IPV6 &&
					ch->cmsg_type == IPV6_HOPLIMIT)
				memcpy(&hlim, CMSG_DATA(ch), sizeof(hlim));

		if (!ra_icmpv6_valid(&from, hlim, buf, len))
			continue;

		if (!found) {
			odhcp6c_expire();
			found = true;
		}
		uint32_t router_valid = ntohs(adv->nd_ra_router_lifetime);

		// Stop sending solicits
		if (rs_attempt > 0 && router_valid) {
			alarm(0);
			rs_attempt = 0;
		}

		// Parse default route
		entry->target = any;
		entry->length = 0;
		entry->router = from.sin6_addr;
		entry->priority = pref_to_priority(adv->nd_ra_flags_reserved);
		if (entry->priority < 0)
			entry->priority = pref_to_priority(0);
		entry->valid = router_valid;
		entry->preferred = entry->valid;
		changed |= odhcp6c_update_entry(STATE_RA_ROUTE, entry, 0, true);

		// Parse hoplimit
		ra_conf_hoplimit(adv->nd_ra_curhoplimit);

		// Parse ND parameters
		ra_conf_reachable(ntohl(adv->nd_ra_reachable));
		ra_conf_retransmit(ntohl(adv->nd_ra_retransmit));

		// Evaluate options
		struct icmpv6_opt *opt;
		icmpv6_for_each_option(opt, &adv[1], &buf[len]) {
			if (opt->type == ND_OPT_MTU) {
				uint32_t *mtu = (uint32_t*)&opt->data[2];
				ra_conf_mtu(ntohl(*mtu));
			} else if (opt->type == ND_OPT_ROUTE_INFORMATION && opt->len <= 3) {
				entry->router = from.sin6_addr;
				entry->target = any;
				entry->priority = pref_to_priority(opt->data[1]);
				entry->length = opt->data[0];
				uint32_t *valid = (uint32_t*)&opt->data[2];
				entry->valid = ntohl(*valid);
				memcpy(&entry->target, &opt->data[6], (opt->len - 1) * 8);

				if (entry->length > 128 || IN6_IS_ADDR_LINKLOCAL(&entry->target)
						|| IN6_IS_ADDR_LOOPBACK(&entry->target)
						|| IN6_IS_ADDR_MULTICAST(&entry->target))
					continue;

				if (entry->priority > 0)
					changed |= odhcp6c_update_entry(STATE_RA_ROUTE, entry, 0, true);
			} else if (opt->type == ND_OPT_PREFIX_INFORMATION && opt->len == 4) {
				struct nd_opt_prefix_info *pinfo = (struct nd_opt_prefix_info*)opt;
				entry->router = any;
				entry->target = pinfo->nd_opt_pi_prefix;
				entry->priority = 256;
				entry->length = pinfo->nd_opt_pi_prefix_len;
				entry->valid = ntohl(pinfo->nd_opt_pi_valid_time);
				entry->preferred = ntohl(pinfo->nd_opt_pi_preferred_time);

				if (entry->length > 128 || IN6_IS_ADDR_LINKLOCAL(&entry->target)
						|| IN6_IS_ADDR_LOOPBACK(&entry->target)
						|| IN6_IS_ADDR_MULTICAST(&entry->target)
						|| entry->valid < entry->preferred)
					continue;

				if (pinfo->nd_opt_pi_flags_reserved & ND_OPT_PI_FLAG_ONLINK)
					changed |= odhcp6c_update_entry(STATE_RA_ROUTE, entry, 7200, true);

				if (!(pinfo->nd_opt_pi_flags_reserved & ND_OPT_PI_FLAG_AUTO) ||
						pinfo->nd_opt_pi_prefix_len != 64)
					continue;

				entry->target.s6_addr32[2] = lladdr.s6_addr32[2];
				entry->target.s6_addr32[3] = lladdr.s6_addr32[3];

				changed |= odhcp6c_update_entry(STATE_RA_PREFIX, entry, 7200, true);
			} else if (opt->type == ND_OPT_RECURSIVE_DNS && opt->len > 2) {
				entry->router = from.sin6_addr;
				entry->priority = 0;
				entry->length = 128;
				uint32_t *valid = (uint32_t*)&opt->data[2];
				entry->valid = ntohl(*valid);
				entry->preferred = 0;

				for (ssize_t i = 0; i < (opt->len - 1) / 2; ++i) {
					memcpy(&entry->target, &opt->data[6 + i * sizeof(entry->target)],
							sizeof(entry->target));
					changed |= odhcp6c_update_entry(STATE_RA_DNS, entry, 0, true);
				}
			} else if (opt->type == ND_OPT_DNSSL && opt->len > 1) {
				uint32_t *valid = (uint32_t*)&opt->data[2];
				uint8_t *buf = &opt->data[6];
				uint8_t *end = &buf[(opt->len - 1) * 8];

				entry->router = from.sin6_addr;
				entry->valid = ntohl(*valid);

				while (buf < end) {
					int len = dn_expand(buf, end, buf, (char*)entry->auxtarget, 256);
					if (len < 1)
						break;

					buf = &buf[len];
					entry->auxlen = strlen((char*)entry->auxtarget);

					if (entry->auxlen == 0)
						continue;

					changed |= odhcp6c_update_entry(STATE_RA_SEARCH, entry, 0, true);
					entry->auxlen = 0;
				}
			}
		}

		int states[2] = {STATE_RA_DNS, STATE_RA_SEARCH};
		for (size_t i = 0; i < 2; ++i) {
			size_t ra_dns_len;
			uint8_t *start = odhcp6c_get_state(states[i], &ra_dns_len);
			for (struct odhcp6c_entry *c = (struct odhcp6c_entry*)start;
						(uint8_t*)c < &start[ra_dns_len] && &c->auxtarget[c->auxlen] <= &start[ra_dns_len];
						c = (struct odhcp6c_entry*)(&c->auxtarget[c->auxlen]))
				if (IN6_ARE_ADDR_EQUAL(&c->router, &from.sin6_addr) &&
						c->valid > router_valid)
					c->valid = router_valid;
		}
	}

	if (found)
		odhcp6c_expire();

	return found && changed;
}
