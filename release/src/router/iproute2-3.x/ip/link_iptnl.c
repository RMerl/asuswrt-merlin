/*
 * link_iptnl.c	ipip and sit driver module
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:	Nicolas Dichtel <nicolas.dichtel@6wind.com>
 *
 */

#include <string.h>
#include <net/if.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <linux/ip.h>
#include <linux/if_tunnel.h>
#include "rt_names.h"
#include "utils.h"
#include "ip_common.h"
#include "tunnel.h"

static void print_usage(FILE *f, int sit)
{
	fprintf(f, "Usage: ip link { add | set | change | replace | del } NAME\n");
	fprintf(f, "          type { ipip | sit } [ remote ADDR ] [ local ADDR ]\n");
	fprintf(f, "          [ ttl TTL ] [ tos TOS ] [ [no]pmtudisc ] [ dev PHYS_DEV ]\n");
	fprintf(f, "          [ 6rd-prefix ADDR ] [ 6rd-relay_prefix ADDR ] [ 6rd-reset ]\n");
	fprintf(f, "          [ noencap ] [ encap { fou | gue | none } ]\n");
	fprintf(f, "          [ encap-sport PORT ] [ encap-dport PORT ]\n");
	fprintf(f, "          [ [no]encap-csum ] [ [no]encap-csum6 ] [ [no]encap-remcsum ]\n");
	if (sit) {
		fprintf(f, "          [ mode { ip6ip | ipip | any } ]\n");
		fprintf(f, "          [ isatap ]\n");
	}
	fprintf(f, "\n");
	fprintf(f, "Where: NAME := STRING\n");
	fprintf(f, "       ADDR := { IP_ADDRESS | any }\n");
	fprintf(f, "       TOS  := { NUMBER | inherit }\n");
	fprintf(f, "       TTL  := { 1..255 | inherit }\n");
}

static void usage(int sit) __attribute__((noreturn));
static void usage(int sit)
{
	print_usage(stderr, sit);
	exit(-1);
}

static int iptunnel_parse_opt(struct link_util *lu, int argc, char **argv,
			      struct nlmsghdr *n)
{
	struct {
		struct nlmsghdr n;
		struct ifinfomsg i;
		char buf[2048];
	} req;
	struct ifinfomsg *ifi = (struct ifinfomsg *)(n + 1);
	struct rtattr *tb[IFLA_MAX + 1];
	struct rtattr *linkinfo[IFLA_INFO_MAX+1];
	struct rtattr *iptuninfo[IFLA_IPTUN_MAX + 1];
	int len;
	__u32 link = 0;
	__u32 laddr = 0;
	__u32 raddr = 0;
	__u8 ttl = 0;
	__u8 tos = 0;
	__u8 pmtudisc = 1;
	__u16 iflags = 0;
	__u8 proto = 0;
	struct in6_addr ip6rdprefix;
	__u16 ip6rdprefixlen = 0;
	__u32 ip6rdrelayprefix = 0;
	__u16 ip6rdrelayprefixlen = 0;
	__u16 encaptype = 0;
	__u16 encapflags = 0;
	__u16 encapsport = 0;
	__u16 encapdport = 0;

	memset(&ip6rdprefix, 0, sizeof(ip6rdprefix));

	if (!(n->nlmsg_flags & NLM_F_CREATE)) {
		memset(&req, 0, sizeof(req));

		req.n.nlmsg_len = NLMSG_LENGTH(sizeof(*ifi));
		req.n.nlmsg_flags = NLM_F_REQUEST;
		req.n.nlmsg_type = RTM_GETLINK;
		req.i.ifi_family = preferred_family;
		req.i.ifi_index = ifi->ifi_index;

		if (rtnl_talk(&rth, &req.n, 0, 0, &req.n) < 0) {
get_failed:
			fprintf(stderr,
				"Failed to get existing tunnel info.\n");
			return -1;
		}

		len = req.n.nlmsg_len;
		len -= NLMSG_LENGTH(sizeof(*ifi));
		if (len < 0)
			goto get_failed;

		parse_rtattr(tb, IFLA_MAX, IFLA_RTA(&req.i), len);

		if (!tb[IFLA_LINKINFO])
			goto get_failed;

		parse_rtattr_nested(linkinfo, IFLA_INFO_MAX, tb[IFLA_LINKINFO]);

		if (!linkinfo[IFLA_INFO_DATA])
			goto get_failed;

		parse_rtattr_nested(iptuninfo, IFLA_IPTUN_MAX,
				    linkinfo[IFLA_INFO_DATA]);

		if (iptuninfo[IFLA_IPTUN_LOCAL])
			laddr = rta_getattr_u32(iptuninfo[IFLA_IPTUN_LOCAL]);

		if (iptuninfo[IFLA_IPTUN_REMOTE])
			raddr = rta_getattr_u32(iptuninfo[IFLA_IPTUN_REMOTE]);

		if (iptuninfo[IFLA_IPTUN_TTL])
			ttl = rta_getattr_u8(iptuninfo[IFLA_IPTUN_TTL]);

		if (iptuninfo[IFLA_IPTUN_TOS])
			tos = rta_getattr_u8(iptuninfo[IFLA_IPTUN_TOS]);

		if (iptuninfo[IFLA_IPTUN_PMTUDISC])
			pmtudisc =
				rta_getattr_u8(iptuninfo[IFLA_IPTUN_PMTUDISC]);

		if (iptuninfo[IFLA_IPTUN_FLAGS])
			iflags = rta_getattr_u16(iptuninfo[IFLA_IPTUN_FLAGS]);

		if (iptuninfo[IFLA_IPTUN_LINK])
			link = rta_getattr_u32(iptuninfo[IFLA_IPTUN_LINK]);

		if (iptuninfo[IFLA_IPTUN_PROTO])
			proto = rta_getattr_u8(iptuninfo[IFLA_IPTUN_PROTO]);

		if (iptuninfo[IFLA_IPTUN_ENCAP_TYPE])
			encaptype = rta_getattr_u16(iptuninfo[IFLA_IPTUN_ENCAP_TYPE]);
		if (iptuninfo[IFLA_IPTUN_ENCAP_FLAGS])
			encapflags = rta_getattr_u16(iptuninfo[IFLA_IPTUN_ENCAP_FLAGS]);
		if (iptuninfo[IFLA_IPTUN_ENCAP_SPORT])
			encapsport = rta_getattr_u16(iptuninfo[IFLA_IPTUN_ENCAP_SPORT]);
		if (iptuninfo[IFLA_IPTUN_ENCAP_DPORT])
			encapdport = rta_getattr_u16(iptuninfo[IFLA_IPTUN_ENCAP_DPORT]);
		if (iptuninfo[IFLA_IPTUN_6RD_PREFIX])
			memcpy(&ip6rdprefix,
			       RTA_DATA(iptuninfo[IFLA_IPTUN_6RD_PREFIX]),
			       sizeof(laddr));

		if (iptuninfo[IFLA_IPTUN_6RD_PREFIXLEN])
			ip6rdprefixlen =
				rta_getattr_u16(iptuninfo[IFLA_IPTUN_6RD_PREFIXLEN]);

		if (iptuninfo[IFLA_IPTUN_6RD_RELAY_PREFIX])
			ip6rdrelayprefix =
				rta_getattr_u32(iptuninfo[IFLA_IPTUN_6RD_RELAY_PREFIX]);

		if (iptuninfo[IFLA_IPTUN_6RD_RELAY_PREFIXLEN])
			ip6rdrelayprefixlen =
				rta_getattr_u16(iptuninfo[IFLA_IPTUN_6RD_RELAY_PREFIXLEN]);
	}

	while (argc > 0) {
		if (strcmp(*argv, "remote") == 0) {
			NEXT_ARG();
			if (strcmp(*argv, "any"))
				raddr = get_addr32(*argv);
			else
				raddr = 0;
		} else if (strcmp(*argv, "local") == 0) {
			NEXT_ARG();
			if (strcmp(*argv, "any"))
				laddr = get_addr32(*argv);
			else
				laddr = 0;
		} else if (matches(*argv, "dev") == 0) {
			NEXT_ARG();
			link = if_nametoindex(*argv);
			if (link == 0)
				invarg("\"dev\" is invalid", *argv);
		} else if (strcmp(*argv, "ttl") == 0 ||
			   strcmp(*argv, "hoplimit") == 0) {
			NEXT_ARG();
			if (strcmp(*argv, "inherit") != 0) {
				if (get_u8(&ttl, *argv, 0))
					invarg("invalid TTL\n", *argv);
			} else
				ttl = 0;
		} else if (strcmp(*argv, "tos") == 0 ||
			   strcmp(*argv, "tclass") == 0 ||
			   matches(*argv, "dsfield") == 0) {
			__u32 uval;
			NEXT_ARG();
			if (strcmp(*argv, "inherit") != 0) {
				if (rtnl_dsfield_a2n(&uval, *argv))
					invarg("bad TOS value", *argv);
				tos = uval;
			} else
				tos = 1;
		} else if (strcmp(*argv, "nopmtudisc") == 0) {
			pmtudisc = 0;
		} else if (strcmp(*argv, "pmtudisc") == 0) {
			pmtudisc = 1;
		} else if (strcmp(lu->id, "sit") == 0 &&
			   strcmp(*argv, "isatap") == 0) {
			iflags |= SIT_ISATAP;
		} else if (strcmp(lu->id, "sit") == 0 &&
			   strcmp(*argv, "mode") == 0) {
			NEXT_ARG();
			if (strcmp(*argv, "ipv6/ipv4") == 0 ||
			    strcmp(*argv, "ip6ip") == 0)
				proto = IPPROTO_IPV6;
			else if (strcmp(*argv, "ipv4/ipv4") == 0 ||
				 strcmp(*argv, "ipip") == 0 ||
				 strcmp(*argv, "ip4ip4") == 0)
				proto = IPPROTO_IPIP;
			else if (strcmp(*argv, "any/ipv4") == 0 ||
				 strcmp(*argv, "any") == 0)
				proto = 0;
			else
				invarg("Cannot guess tunnel mode.", *argv);
		} else if (strcmp(*argv, "noencap") == 0) {
			encaptype = TUNNEL_ENCAP_NONE;
		} else if (strcmp(*argv, "encap") == 0) {
			NEXT_ARG();
			if (strcmp(*argv, "fou") == 0)
				encaptype = TUNNEL_ENCAP_FOU;
			else if (strcmp(*argv, "gue") == 0)
				encaptype = TUNNEL_ENCAP_GUE;
			else if (strcmp(*argv, "none") == 0)
				encaptype = TUNNEL_ENCAP_NONE;
			else
				invarg("Invalid encap type.", *argv);
		} else if (strcmp(*argv, "encap-sport") == 0) {
			NEXT_ARG();
			if (strcmp(*argv, "auto") == 0)
				encapsport = 0;
			else if (get_u16(&encapsport, *argv, 0))
				invarg("Invalid source port.", *argv);
		} else if (strcmp(*argv, "encap-dport") == 0) {
			NEXT_ARG();
			if (get_u16(&encapdport, *argv, 0))
				invarg("Invalid destination port.", *argv);
		} else if (strcmp(*argv, "encap-csum") == 0) {
			encapflags |= TUNNEL_ENCAP_FLAG_CSUM;
		} else if (strcmp(*argv, "noencap-csum") == 0) {
			encapflags &= ~TUNNEL_ENCAP_FLAG_CSUM;
		} else if (strcmp(*argv, "encap-udp6-csum") == 0) {
			encapflags |= TUNNEL_ENCAP_FLAG_CSUM6;
		} else if (strcmp(*argv, "noencap-udp6-csum") == 0) {
			encapflags &= ~TUNNEL_ENCAP_FLAG_CSUM6;
		} else if (strcmp(*argv, "encap-remcsum") == 0) {
			encapflags |= TUNNEL_ENCAP_FLAG_REMCSUM;
		} else if (strcmp(*argv, "noencap-remcsum") == 0) {
			encapflags &= ~TUNNEL_ENCAP_FLAG_REMCSUM;
		} else if (strcmp(*argv, "6rd-prefix") == 0) {
			inet_prefix prefix;
			NEXT_ARG();
			if (get_prefix(&prefix, *argv, AF_INET6))
				invarg("invalid 6rd_prefix\n", *argv);
			memcpy(&ip6rdprefix, prefix.data, 16);
			ip6rdprefixlen = prefix.bitlen;
		} else if (strcmp(*argv, "6rd-relay_prefix") == 0) {
			inet_prefix prefix;
			NEXT_ARG();
			if (get_prefix(&prefix, *argv, AF_INET))
				invarg("invalid 6rd-relay_prefix\n", *argv);
			memcpy(&ip6rdrelayprefix, prefix.data, 4);
			ip6rdrelayprefixlen = prefix.bitlen;
		} else if (strcmp(*argv, "6rd-reset") == 0) {
			inet_prefix prefix;
			get_prefix(&prefix, "2002::", AF_INET6);
			memcpy(&ip6rdprefix, prefix.data, 16);
			ip6rdprefixlen = 16;
			ip6rdrelayprefix = 0;
			ip6rdrelayprefixlen = 0;
		} else
			usage(strcmp(lu->id, "sit") == 0);
		argc--, argv++;
	}

	if (ttl && pmtudisc == 0) {
		fprintf(stderr, "ttl != 0 and nopmtudisc are incompatible\n");
		exit(-1);
	}

	addattr32(n, 1024, IFLA_IPTUN_LINK, link);
	addattr32(n, 1024, IFLA_IPTUN_LOCAL, laddr);
	addattr32(n, 1024, IFLA_IPTUN_REMOTE, raddr);
	addattr8(n, 1024, IFLA_IPTUN_TTL, ttl);
	addattr8(n, 1024, IFLA_IPTUN_TOS, tos);
	addattr8(n, 1024, IFLA_IPTUN_PMTUDISC, pmtudisc);

	addattr16(n, 1024, IFLA_IPTUN_ENCAP_TYPE, encaptype);
	addattr16(n, 1024, IFLA_IPTUN_ENCAP_FLAGS, encapflags);
	addattr16(n, 1024, IFLA_IPTUN_ENCAP_SPORT, htons(encapsport));
	addattr16(n, 1024, IFLA_IPTUN_ENCAP_DPORT, htons(encapdport));

	if (strcmp(lu->id, "sit") == 0) {
		addattr16(n, 1024, IFLA_IPTUN_FLAGS, iflags);
		addattr8(n, 1024, IFLA_IPTUN_PROTO, proto);
		if (ip6rdprefixlen) {
			addattr_l(n, 1024, IFLA_IPTUN_6RD_PREFIX,
				  &ip6rdprefix, sizeof(ip6rdprefix));
			addattr16(n, 1024, IFLA_IPTUN_6RD_PREFIXLEN,
				  ip6rdprefixlen);
			addattr32(n, 1024, IFLA_IPTUN_6RD_RELAY_PREFIX,
				  ip6rdrelayprefix);
			addattr16(n, 1024, IFLA_IPTUN_6RD_RELAY_PREFIXLEN,
				  ip6rdrelayprefixlen);
		}
	}

	return 0;
}

static void iptunnel_print_opt(struct link_util *lu, FILE *f, struct rtattr *tb[])
{
	char s1[1024];
	char s2[64];
	const char *local = "any";
	const char *remote = "any";

	if (!tb)
		return;

	if (tb[IFLA_IPTUN_REMOTE]) {
		unsigned addr = rta_getattr_u32(tb[IFLA_IPTUN_REMOTE]);

		if (addr)
			remote = format_host(AF_INET, 4, &addr, s1, sizeof(s1));
	}

	fprintf(f, "remote %s ", remote);

	if (tb[IFLA_IPTUN_LOCAL]) {
		unsigned addr = rta_getattr_u32(tb[IFLA_IPTUN_LOCAL]);

		if (addr)
			local = format_host(AF_INET, 4, &addr, s1, sizeof(s1));
	}

	fprintf(f, "local %s ", local);

	if (tb[IFLA_IPTUN_LINK] && rta_getattr_u32(tb[IFLA_IPTUN_LINK])) {
		unsigned link = rta_getattr_u32(tb[IFLA_IPTUN_LINK]);
		const char *n = if_indextoname(link, s2);

		if (n)
			fprintf(f, "dev %s ", n);
		else
			fprintf(f, "dev %u ", link);
	}

	if (tb[IFLA_IPTUN_TTL] && rta_getattr_u8(tb[IFLA_IPTUN_TTL]))
		fprintf(f, "ttl %d ", rta_getattr_u8(tb[IFLA_IPTUN_TTL]));
	else
		fprintf(f, "ttl inherit ");

	if (tb[IFLA_IPTUN_TOS] && rta_getattr_u8(tb[IFLA_IPTUN_TOS])) {
		int tos = rta_getattr_u8(tb[IFLA_IPTUN_TOS]);

		fputs("tos ", f);
		if (tos == 1)
			fputs("inherit ", f);
		else
			fprintf(f, "0x%x ", tos);
	}

	if (tb[IFLA_IPTUN_PMTUDISC] && rta_getattr_u8(tb[IFLA_IPTUN_PMTUDISC]))
		fprintf(f, "pmtudisc ");
	else
		fprintf(f, "nopmtudisc ");

	if (tb[IFLA_IPTUN_FLAGS]) {
		__u16 iflags = rta_getattr_u16(tb[IFLA_IPTUN_FLAGS]);

		if (iflags & SIT_ISATAP)
			fprintf(f, "isatap ");
	}

	if (tb[IFLA_IPTUN_6RD_PREFIXLEN] &&
	    *(__u16 *)RTA_DATA(tb[IFLA_IPTUN_6RD_PREFIXLEN])) {
		__u16 prefixlen = rta_getattr_u16(tb[IFLA_IPTUN_6RD_PREFIXLEN]);
		__u16 relayprefixlen =
			rta_getattr_u16(tb[IFLA_IPTUN_6RD_RELAY_PREFIXLEN]);
		__u32 relayprefix =
			rta_getattr_u32(tb[IFLA_IPTUN_6RD_RELAY_PREFIX]);

		printf("6rd-prefix %s/%u ",
		       inet_ntop(AF_INET6, RTA_DATA(tb[IFLA_IPTUN_6RD_PREFIX]),
				 s1, sizeof(s1)),
		       prefixlen);
		if (relayprefix) {
			printf("6rd-relay_prefix %s/%u ",
			       format_host(AF_INET, 4, &relayprefix, s1,
					   sizeof(s1)),
			       relayprefixlen);
		}
	}

	if (tb[IFLA_IPTUN_ENCAP_TYPE] &&
	    *(__u16 *)RTA_DATA(tb[IFLA_IPTUN_ENCAP_TYPE]) != TUNNEL_ENCAP_NONE) {
		__u16 type = rta_getattr_u16(tb[IFLA_IPTUN_ENCAP_TYPE]);
		__u16 flags = rta_getattr_u16(tb[IFLA_IPTUN_ENCAP_FLAGS]);
		__u16 sport = rta_getattr_u16(tb[IFLA_IPTUN_ENCAP_SPORT]);
		__u16 dport = rta_getattr_u16(tb[IFLA_IPTUN_ENCAP_DPORT]);

		fputs("encap ", f);
		switch (type) {
		case TUNNEL_ENCAP_FOU:
			fputs("fou ", f);
			break;
		case TUNNEL_ENCAP_GUE:
			fputs("gue ", f);
			break;
		default:
			fputs("unknown ", f);
			break;
		}

		if (sport == 0)
			fputs("encap-sport auto ", f);
		else
			fprintf(f, "encap-sport %u", ntohs(sport));

		fprintf(f, "encap-dport %u ", ntohs(dport));

		if (flags & TUNNEL_ENCAP_FLAG_CSUM)
			fputs("encap-csum ", f);
		else
			fputs("noencap-csum ", f);

		if (flags & TUNNEL_ENCAP_FLAG_CSUM6)
			fputs("encap-csum6 ", f);
		else
			fputs("noencap-csum6 ", f);

		if (flags & TUNNEL_ENCAP_FLAG_REMCSUM)
			fputs("encap-remcsum ", f);
		else
			fputs("noencap-remcsum ", f);
	}
}

static void iptunnel_print_help(struct link_util *lu, int argc, char **argv,
	FILE *f)
{
	print_usage(f, strcmp(lu->id, "sit") == 0);
}

struct link_util ipip_link_util = {
	.id = "ipip",
	.maxattr = IFLA_IPTUN_MAX,
	.parse_opt = iptunnel_parse_opt,
	.print_opt = iptunnel_print_opt,
	.print_help = iptunnel_print_help,
};

struct link_util sit_link_util = {
	.id = "sit",
	.maxattr = IFLA_IPTUN_MAX,
	.parse_opt = iptunnel_parse_opt,
	.print_opt = iptunnel_print_opt,
	.print_help = iptunnel_print_help,
};
