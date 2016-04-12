/*
 * iplink_vxlan.c	VXLAN device support
 *
 *              This program is free software; you can redistribute it and/or
 *              modify it under the terms of the GNU General Public License
 *              as published by the Free Software Foundation; either version
 *              2 of the License, or (at your option) any later version.
 *
 * Authors:     Stephen Hemminger <shemminger@vyatta.com
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <net/if.h>
#include <linux/ip.h>
#include <linux/if_link.h>
#include <arpa/inet.h>

#include "rt_names.h"
#include "utils.h"
#include "ip_common.h"

static void print_explain(FILE *f)
{
	fprintf(f, "Usage: ... vxlan id VNI [ { group | remote } ADDR ] [ local ADDR ]\n");
	fprintf(f, "                 [ ttl TTL ] [ tos TOS ] [ dev PHYS_DEV ]\n");
	fprintf(f, "                 [ dstport PORT ] [ srcport MIN MAX ]\n");
	fprintf(f, "                 [ [no]learning ] [ [no]proxy ] [ [no]rsc ]\n");
	fprintf(f, "                 [ [no]l2miss ] [ [no]l3miss ]\n");
	fprintf(f, "                 [ ageing SECONDS ] [ maxaddress NUMBER ]\n");
	fprintf(f, "                 [ [no]udpcsum ] [ [no]udp6zerocsumtx ] [ [no]udp6zerocsumrx ]\n");
	fprintf(f, "\n");
	fprintf(f, "Where: VNI := 0-16777215\n");
	fprintf(f, "       ADDR := { IP_ADDRESS | any }\n");
	fprintf(f, "       TOS  := { NUMBER | inherit }\n");
	fprintf(f, "       TTL  := { 1..255 | inherit }\n");
}

static void explain(void)
{
	print_explain(stderr);
}

static int vxlan_parse_opt(struct link_util *lu, int argc, char **argv,
			  struct nlmsghdr *n)
{
	__u32 vni = 0;
	int vni_set = 0;
	__u32 saddr = 0;
	__u32 gaddr = 0;
	__u32 daddr = 0;
	struct in6_addr saddr6 = IN6ADDR_ANY_INIT;
	struct in6_addr gaddr6 = IN6ADDR_ANY_INIT;
	struct in6_addr daddr6 = IN6ADDR_ANY_INIT;
	unsigned link = 0;
	__u8 tos = 0;
	__u8 ttl = 0;
	__u8 learning = 1;
	__u8 proxy = 0;
	__u8 rsc = 0;
	__u8 l2miss = 0;
	__u8 l3miss = 0;
	__u8 noage = 0;
	__u32 age = 0;
	__u32 maxaddr = 0;
	__u16 dstport = 0;
	__u8 udpcsum = 0;
	__u8 udp6zerocsumtx = 0;
	__u8 udp6zerocsumrx = 0;
	int dst_port_set = 0;
	struct ifla_vxlan_port_range range = { 0, 0 };

	while (argc > 0) {
		if (!matches(*argv, "id") ||
		    !matches(*argv, "vni")) {
			NEXT_ARG();
			if (get_u32(&vni, *argv, 0) ||
			    vni >= 1u << 24)
				invarg("invalid id", *argv);
			vni_set = 1;
		} else if (!matches(*argv, "group")) {
			NEXT_ARG();
			if (!inet_get_addr(*argv, &gaddr, &gaddr6)) {
				fprintf(stderr, "Invalid address \"%s\"\n", *argv);
				return -1;
			}
			if (!IN6_IS_ADDR_MULTICAST(&gaddr6) && !IN_MULTICAST(ntohl(gaddr)))
				invarg("invalid group address", *argv);
		} else if (!matches(*argv, "remote")) {
			NEXT_ARG();
			if (!inet_get_addr(*argv, &daddr, &daddr6)) {
				fprintf(stderr, "Invalid address \"%s\"\n", *argv);
				return -1;
			}
			if (IN6_IS_ADDR_MULTICAST(&daddr6) || IN_MULTICAST(ntohl(daddr)))
				invarg("invalid remote address", *argv);
		} else if (!matches(*argv, "local")) {
			NEXT_ARG();
			if (strcmp(*argv, "any")) {
				if (!inet_get_addr(*argv, &saddr, &saddr6)) {
					fprintf(stderr, "Invalid address \"%s\"\n", *argv);
					return -1;
				}
			}

			if (IN_MULTICAST(ntohl(saddr)) || IN6_IS_ADDR_MULTICAST(&saddr6))
				invarg("invalid local address", *argv);
		} else if (!matches(*argv, "dev")) {
			NEXT_ARG();
			link = if_nametoindex(*argv);
			if (link == 0) {
				fprintf(stderr, "Cannot find device \"%s\"\n",
					*argv);
				exit(-1);
			}
		} else if (!matches(*argv, "ttl") ||
			   !matches(*argv, "hoplimit")) {
			unsigned uval;

			NEXT_ARG();
			if (strcmp(*argv, "inherit") != 0) {
				if (get_unsigned(&uval, *argv, 0))
					invarg("invalid TTL", *argv);
				if (uval > 255)
					invarg("TTL must be <= 255", *argv);
				ttl = uval;
			}
		} else if (!matches(*argv, "tos") ||
			   !matches(*argv, "dsfield")) {
			__u32 uval;

			NEXT_ARG();
			if (strcmp(*argv, "inherit") != 0) {
				if (rtnl_dsfield_a2n(&uval, *argv))
					invarg("bad TOS value", *argv);
				tos = uval;
			} else
				tos = 1;
		} else if (!matches(*argv, "ageing")) {
			NEXT_ARG();
			if (strcmp(*argv, "none") == 0)
				noage = 1;
			else if (get_u32(&age, *argv, 0))
				invarg("ageing timer", *argv);
		} else if (!matches(*argv, "maxaddress")) {
			NEXT_ARG();
			if (strcmp(*argv, "unlimited") == 0)
				maxaddr = 0;
			else if (get_u32(&maxaddr, *argv, 0))
				invarg("max addresses", *argv);
		} else if (!matches(*argv, "port") ||
			   !matches(*argv, "srcport")) {
			__u16 minport, maxport;
			NEXT_ARG();
			if (get_u16(&minport, *argv, 0))
				invarg("min port", *argv);
			NEXT_ARG();
			if (get_u16(&maxport, *argv, 0))
				invarg("max port", *argv);
			range.low = htons(minport);
			range.high = htons(maxport);
		} else if (!matches(*argv, "dstport")){
			NEXT_ARG();
			if (get_u16(&dstport, *argv, 0))
				invarg("dst port", *argv);
			dst_port_set = 1;
		} else if (!matches(*argv, "nolearning")) {
			learning = 0;
		} else if (!matches(*argv, "learning")) {
			learning = 1;
		} else if (!matches(*argv, "noproxy")) {
			proxy = 0;
		} else if (!matches(*argv, "proxy")) {
			proxy = 1;
		} else if (!matches(*argv, "norsc")) {
			rsc = 0;
		} else if (!matches(*argv, "rsc")) {
			rsc = 1;
		} else if (!matches(*argv, "nol2miss")) {
			l2miss = 0;
		} else if (!matches(*argv, "l2miss")) {
			l2miss = 1;
		} else if (!matches(*argv, "nol3miss")) {
			l3miss = 0;
		} else if (!matches(*argv, "l3miss")) {
			l3miss = 1;
		} else if (!matches(*argv, "udpcsum")) {
			udpcsum = 1;
		} else if (!matches(*argv, "noudpcsum")) {
			udpcsum = 0;
		} else if (!matches(*argv, "udp6zerocsumtx")) {
			udp6zerocsumtx = 1;
		} else if (!matches(*argv, "noudp6zerocsumtx")) {
			udp6zerocsumtx = 0;
		} else if (!matches(*argv, "udp6zerocsumrx")) {
			udp6zerocsumrx = 1;
		} else if (!matches(*argv, "noudp6zerocsumrx")) {
			udp6zerocsumrx = 0;
		} else if (matches(*argv, "help") == 0) {
			explain();
			return -1;
		} else {
			fprintf(stderr, "vxlan: unknown command \"%s\"?\n", *argv);
			explain();
			return -1;
		}
		argc--, argv++;
	}

	if (!vni_set) {
		fprintf(stderr, "vxlan: missing virtual network identifier\n");
		return -1;
	}

	if ((gaddr && daddr) ||
		(memcmp(&gaddr6, &in6addr_any, sizeof(gaddr6)) &&
		 memcmp(&daddr6, &in6addr_any, sizeof(daddr6)))) {
		fprintf(stderr, "vxlan: both group and remote cannot be specified\n");
		return -1;
	}

	if (!dst_port_set) {
		fprintf(stderr, "vxlan: destination port not specified\n"
			"Will use Linux kernel default (non-standard value)\n");
		fprintf(stderr,
			"Use 'dstport 4789' to get the IANA assigned value\n"
			"Use 'dstport 0' to get default and quiet this message\n");
	}

	addattr32(n, 1024, IFLA_VXLAN_ID, vni);
	if (gaddr)
		addattr_l(n, 1024, IFLA_VXLAN_GROUP, &gaddr, 4);
	else if (daddr)
		addattr_l(n, 1024, IFLA_VXLAN_GROUP, &daddr, 4);
	if (memcmp(&gaddr6, &in6addr_any, sizeof(gaddr6)) != 0)
		addattr_l(n, 1024, IFLA_VXLAN_GROUP6, &gaddr6, sizeof(struct in6_addr));
	else if (memcmp(&daddr6, &in6addr_any, sizeof(daddr6)) != 0)
		addattr_l(n, 1024, IFLA_VXLAN_GROUP6, &daddr6, sizeof(struct in6_addr));

	if (saddr)
		addattr_l(n, 1024, IFLA_VXLAN_LOCAL, &saddr, 4);
	else if (memcmp(&saddr6, &in6addr_any, sizeof(saddr6)) != 0)
		addattr_l(n, 1024, IFLA_VXLAN_LOCAL6, &saddr6, sizeof(struct in6_addr));

	if (link)
		addattr32(n, 1024, IFLA_VXLAN_LINK, link);
	addattr8(n, 1024, IFLA_VXLAN_TTL, ttl);
	addattr8(n, 1024, IFLA_VXLAN_TOS, tos);
	addattr8(n, 1024, IFLA_VXLAN_LEARNING, learning);
	addattr8(n, 1024, IFLA_VXLAN_PROXY, proxy);
	addattr8(n, 1024, IFLA_VXLAN_RSC, rsc);
	addattr8(n, 1024, IFLA_VXLAN_L2MISS, l2miss);
	addattr8(n, 1024, IFLA_VXLAN_L3MISS, l3miss);
	addattr8(n, 1024, IFLA_VXLAN_UDP_CSUM, udpcsum);
	addattr8(n, 1024, IFLA_VXLAN_UDP_ZERO_CSUM6_TX, udp6zerocsumtx);
	addattr8(n, 1024, IFLA_VXLAN_UDP_ZERO_CSUM6_RX, udp6zerocsumrx);

	if (noage)
		addattr32(n, 1024, IFLA_VXLAN_AGEING, 0);
	else if (age)
		addattr32(n, 1024, IFLA_VXLAN_AGEING, age);
	if (maxaddr)
		addattr32(n, 1024, IFLA_VXLAN_LIMIT, maxaddr);
	if (range.low || range.high)
		addattr_l(n, 1024, IFLA_VXLAN_PORT_RANGE,
			  &range, sizeof(range));
	if (dstport)
		addattr16(n, 1024, IFLA_VXLAN_PORT, htons(dstport));

	return 0;
}

static void vxlan_print_opt(struct link_util *lu, FILE *f, struct rtattr *tb[])
{
	__u32 vni;
	unsigned link;
	__u8 tos;
	__u32 maxaddr;
	char s1[1024];
	char s2[64];

	if (!tb)
		return;

	if (!tb[IFLA_VXLAN_ID] ||
	    RTA_PAYLOAD(tb[IFLA_VXLAN_ID]) < sizeof(__u32))
		return;

	vni = rta_getattr_u32(tb[IFLA_VXLAN_ID]);
	fprintf(f, "id %u ", vni);

	if (tb[IFLA_VXLAN_GROUP]) {
		__be32 addr = rta_getattr_u32(tb[IFLA_VXLAN_GROUP]);
		if (addr) {
			if (IN_MULTICAST(ntohl(addr)))
				fprintf(f, "group %s ",
					format_host(AF_INET, 4, &addr, s1, sizeof(s1)));
			else
				fprintf(f, "remote %s ",
					format_host(AF_INET, 4, &addr, s1, sizeof(s1)));
		}
	} else if (tb[IFLA_VXLAN_GROUP6]) {
		struct in6_addr addr;
		memcpy(&addr, RTA_DATA(tb[IFLA_VXLAN_GROUP6]), sizeof(struct in6_addr));
		if (memcmp(&addr, &in6addr_any, sizeof(addr)) != 0) {
			if (IN6_IS_ADDR_MULTICAST(&addr))
				fprintf(f, "group %s ",
					format_host(AF_INET6, sizeof(struct in6_addr), &addr, s1, sizeof(s1)));
			else
				fprintf(f, "remote %s ",
					format_host(AF_INET6, sizeof(struct in6_addr), &addr, s1, sizeof(s1)));
		}
	}

	if (tb[IFLA_VXLAN_LOCAL]) {
		__be32 addr = rta_getattr_u32(tb[IFLA_VXLAN_LOCAL]);
		if (addr)
			fprintf(f, "local %s ",
				format_host(AF_INET, 4, &addr, s1, sizeof(s1)));
	} else if (tb[IFLA_VXLAN_LOCAL6]) {
		struct in6_addr addr;
		memcpy(&addr, RTA_DATA(tb[IFLA_VXLAN_LOCAL6]), sizeof(struct in6_addr));
		if (memcmp(&addr, &in6addr_any, sizeof(addr)) != 0)
			fprintf(f, "local %s ",
				format_host(AF_INET6, sizeof(struct in6_addr), &addr, s1, sizeof(s1)));
	}

	if (tb[IFLA_VXLAN_LINK] &&
	    (link = rta_getattr_u32(tb[IFLA_VXLAN_LINK]))) {
		const char *n = if_indextoname(link, s2);

		if (n)
			fprintf(f, "dev %s ", n);
		else
			fprintf(f, "dev %u ", link);
	}

	if (tb[IFLA_VXLAN_PORT_RANGE]) {
		const struct ifla_vxlan_port_range *r
			= RTA_DATA(tb[IFLA_VXLAN_PORT_RANGE]);
		fprintf(f, "srcport %u %u ", ntohs(r->low), ntohs(r->high));
	}

	if (tb[IFLA_VXLAN_PORT])
		fprintf(f, "dstport %u ",
			ntohs(rta_getattr_u16(tb[IFLA_VXLAN_PORT])));

	if (tb[IFLA_VXLAN_LEARNING] &&
	    !rta_getattr_u8(tb[IFLA_VXLAN_LEARNING]))
		fputs("nolearning ", f);

	if (tb[IFLA_VXLAN_PROXY] && rta_getattr_u8(tb[IFLA_VXLAN_PROXY]))
		fputs("proxy ", f);

	if (tb[IFLA_VXLAN_RSC] && rta_getattr_u8(tb[IFLA_VXLAN_RSC]))
		fputs("rsc ", f);

	if (tb[IFLA_VXLAN_L2MISS] && rta_getattr_u8(tb[IFLA_VXLAN_L2MISS]))
		fputs("l2miss ", f);

	if (tb[IFLA_VXLAN_L3MISS] && rta_getattr_u8(tb[IFLA_VXLAN_L3MISS]))
		fputs("l3miss ", f);

	if (tb[IFLA_VXLAN_TOS] &&
	    (tos = rta_getattr_u8(tb[IFLA_VXLAN_TOS]))) {
		if (tos == 1)
			fprintf(f, "tos inherit ");
		else
			fprintf(f, "tos %#x ", tos);
	}

	if (tb[IFLA_VXLAN_TTL]) {
		__u8 ttl = rta_getattr_u8(tb[IFLA_VXLAN_TTL]);
		if (ttl)
			fprintf(f, "ttl %d ", ttl);
	}

	if (tb[IFLA_VXLAN_AGEING]) {
		__u32 age = rta_getattr_u32(tb[IFLA_VXLAN_AGEING]);
		if (age == 0)
			fprintf(f, "ageing none ");
		else
			fprintf(f, "ageing %u ", age);
	}

	if (tb[IFLA_VXLAN_LIMIT] &&
	    ((maxaddr = rta_getattr_u32(tb[IFLA_VXLAN_LIMIT])) != 0))
		    fprintf(f, "maxaddr %u ", maxaddr);

	if (tb[IFLA_VXLAN_UDP_CSUM] && rta_getattr_u8(tb[IFLA_VXLAN_UDP_CSUM]))
		fputs("udpcsum ", f);

	if (tb[IFLA_VXLAN_UDP_ZERO_CSUM6_TX] &&
	    rta_getattr_u8(tb[IFLA_VXLAN_UDP_ZERO_CSUM6_TX]))
		fputs("udp6zerocsumtx ", f);

	if (tb[IFLA_VXLAN_UDP_ZERO_CSUM6_RX] &&
	    rta_getattr_u8(tb[IFLA_VXLAN_UDP_ZERO_CSUM6_RX]))
		fputs("udp6zerocsumrx ", f);
}

static void vxlan_print_help(struct link_util *lu, int argc, char **argv,
	FILE *f)
{
	print_explain(f);
}

struct link_util vxlan_link_util = {
	.id		= "vxlan",
	.maxattr	= IFLA_VXLAN_MAX,
	.parse_opt	= vxlan_parse_opt,
	.print_opt	= vxlan_print_opt,
	.print_help	= vxlan_print_help,
};
