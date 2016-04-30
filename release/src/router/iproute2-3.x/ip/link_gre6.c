/*
 * link_gre6.c	gre driver module
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:	Dmitry Kozlov <xeb@mail.ru>
 *
 */

#include <string.h>
#include <net/if.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <linux/ip.h>
#include <linux/if_tunnel.h>
#include <linux/ip6_tunnel.h>

#include "rt_names.h"
#include "utils.h"
#include "ip_common.h"
#include "tunnel.h"

#define IP6_FLOWINFO_TCLASS	htonl(0x0FF00000)
#define IP6_FLOWINFO_FLOWLABEL	htonl(0x000FFFFF)

#define DEFAULT_TNL_HOP_LIMIT	(64)

static void print_usage(FILE *f)
{
	fprintf(f, "Usage: ip link { add | set | change | replace | del } NAME\n");
	fprintf(f, "          type { ip6gre | ip6gretap } [ remote ADDR ] [ local ADDR ]\n");
	fprintf(f, "          [ [i|o]seq ] [ [i|o]key KEY ] [ [i|o]csum ]\n");
	fprintf(f, "          [ hoplimit TTL ] [ encaplimit ELIM ]\n");
	fprintf(f, "          [ tclass TCLASS ] [ flowlabel FLOWLABEL ]\n");
	fprintf(f, "          [ dscp inherit ] [ dev PHYS_DEV ]\n");
	fprintf(f, "\n");
	fprintf(f, "Where: NAME      := STRING\n");
	fprintf(f, "       ADDR      := IPV6_ADDRESS\n");
	fprintf(f, "       TTL       := { 0..255 } (default=%d)\n",
		DEFAULT_TNL_HOP_LIMIT);
	fprintf(f, "       KEY       := { DOTTED_QUAD | NUMBER }\n");
	fprintf(f, "       ELIM      := { none | 0..255 }(default=%d)\n",
		IPV6_DEFAULT_TNL_ENCAP_LIMIT);
	fprintf(f, "       TCLASS    := { 0x0..0xff | inherit }\n");
	fprintf(f, "       FLOWLABEL := { 0x0..0xfffff | inherit }\n");
}

static void usage(void) __attribute__((noreturn));
static void usage(void)
{
	print_usage(stderr);
	exit(-1);
}

static int gre_parse_opt(struct link_util *lu, int argc, char **argv,
			 struct nlmsghdr *n)
{
	struct {
		struct nlmsghdr n;
		struct ifinfomsg i;
		char buf[1024];
	} req;
	struct ifinfomsg *ifi = (struct ifinfomsg *)(n + 1);
	struct rtattr *tb[IFLA_MAX + 1];
	struct rtattr *linkinfo[IFLA_INFO_MAX+1];
	struct rtattr *greinfo[IFLA_GRE_MAX + 1];
	__u16 iflags = 0;
	__u16 oflags = 0;
	unsigned ikey = 0;
	unsigned okey = 0;
	struct in6_addr raddr = IN6ADDR_ANY_INIT;
	struct in6_addr laddr = IN6ADDR_ANY_INIT;
	unsigned link = 0;
	unsigned flowinfo = 0;
	unsigned flags = 0;
	__u8 hop_limit = DEFAULT_TNL_HOP_LIMIT;
	__u8 encap_limit = IPV6_DEFAULT_TNL_ENCAP_LIMIT;
	int len;

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

		parse_rtattr_nested(greinfo, IFLA_GRE_MAX,
				    linkinfo[IFLA_INFO_DATA]);

		if (greinfo[IFLA_GRE_IKEY])
			ikey = rta_getattr_u32(greinfo[IFLA_GRE_IKEY]);

		if (greinfo[IFLA_GRE_OKEY])
			okey = rta_getattr_u32(greinfo[IFLA_GRE_OKEY]);

		if (greinfo[IFLA_GRE_IFLAGS])
			iflags = rta_getattr_u16(greinfo[IFLA_GRE_IFLAGS]);

		if (greinfo[IFLA_GRE_OFLAGS])
			oflags = rta_getattr_u16(greinfo[IFLA_GRE_OFLAGS]);

		if (greinfo[IFLA_GRE_LOCAL])
			memcpy(&laddr, RTA_DATA(greinfo[IFLA_GRE_LOCAL]), sizeof(laddr));

		if (greinfo[IFLA_GRE_REMOTE])
			memcpy(&raddr, RTA_DATA(greinfo[IFLA_GRE_REMOTE]), sizeof(raddr));

		if (greinfo[IFLA_GRE_TTL])
			hop_limit = rta_getattr_u8(greinfo[IFLA_GRE_TTL]);

		if (greinfo[IFLA_GRE_LINK])
			link = rta_getattr_u32(greinfo[IFLA_GRE_LINK]);

		if (greinfo[IFLA_GRE_ENCAP_LIMIT])
			encap_limit = rta_getattr_u8(greinfo[IFLA_GRE_ENCAP_LIMIT]);

		if (greinfo[IFLA_GRE_FLOWINFO])
			flowinfo = rta_getattr_u32(greinfo[IFLA_GRE_FLOWINFO]);

		if (greinfo[IFLA_GRE_FLAGS])
			flags = rta_getattr_u32(greinfo[IFLA_GRE_FLAGS]);
	}

	while (argc > 0) {
		if (!matches(*argv, "key")) {
			unsigned uval;

			NEXT_ARG();
			iflags |= GRE_KEY;
			oflags |= GRE_KEY;
			if (strchr(*argv, '.'))
				uval = get_addr32(*argv);
			else {
				if (get_unsigned(&uval, *argv, 0) < 0) {
					fprintf(stderr,
						"Invalid value for \"key\"\n");
					exit(-1);
				}
				uval = htonl(uval);
			}

			ikey = okey = uval;
		} else if (!matches(*argv, "ikey")) {
			unsigned uval;

			NEXT_ARG();
			iflags |= GRE_KEY;
			if (strchr(*argv, '.'))
				uval = get_addr32(*argv);
			else {
				if (get_unsigned(&uval, *argv, 0)<0) {
					fprintf(stderr, "invalid value of \"ikey\"\n");
					exit(-1);
				}
				uval = htonl(uval);
			}
			ikey = uval;
		} else if (!matches(*argv, "okey")) {
			unsigned uval;

			NEXT_ARG();
			oflags |= GRE_KEY;
			if (strchr(*argv, '.'))
				uval = get_addr32(*argv);
			else {
				if (get_unsigned(&uval, *argv, 0)<0) {
					fprintf(stderr, "invalid value of \"okey\"\n");
					exit(-1);
				}
				uval = htonl(uval);
			}
			okey = uval;
		} else if (!matches(*argv, "seq")) {
			iflags |= GRE_SEQ;
			oflags |= GRE_SEQ;
		} else if (!matches(*argv, "iseq")) {
			iflags |= GRE_SEQ;
		} else if (!matches(*argv, "oseq")) {
			oflags |= GRE_SEQ;
		} else if (!matches(*argv, "csum")) {
			iflags |= GRE_CSUM;
			oflags |= GRE_CSUM;
		} else if (!matches(*argv, "icsum")) {
			iflags |= GRE_CSUM;
		} else if (!matches(*argv, "ocsum")) {
			oflags |= GRE_CSUM;
		} else if (!matches(*argv, "remote")) {
			inet_prefix addr;
			NEXT_ARG();
			get_prefix(&addr, *argv, preferred_family);
			if (addr.family == AF_UNSPEC)
				invarg("\"remote\" address family is AF_UNSPEC", *argv);
			memcpy(&raddr, &addr.data, sizeof(raddr));
		} else if (!matches(*argv, "local")) {
			inet_prefix addr;
			NEXT_ARG();
			get_prefix(&addr, *argv, preferred_family);
			if (addr.family == AF_UNSPEC)
				invarg("\"local\" address family is AF_UNSPEC", *argv);
			memcpy(&laddr, &addr.data, sizeof(laddr));
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
			__u8 uval;
			NEXT_ARG();
			if (get_u8(&uval, *argv, 0))
				invarg("invalid TTL", *argv);
			hop_limit = uval;
		} else if (!matches(*argv, "tos") ||
			   !matches(*argv, "tclass") ||
			   !matches(*argv, "dsfield")) {
			__u8 uval;
			NEXT_ARG();
			if (strcmp(*argv, "inherit") == 0)
				flags |= IP6_TNL_F_USE_ORIG_TCLASS;
			else {
				if (get_u8(&uval, *argv, 16))
					invarg("invalid TClass", *argv);
				flowinfo |= htonl((__u32)uval << 20) & IP6_FLOWINFO_TCLASS;
				flags &= ~IP6_TNL_F_USE_ORIG_TCLASS;
			}
		} else if (strcmp(*argv, "flowlabel") == 0 ||
			   strcmp(*argv, "fl") == 0) {
			__u32 uval;
			NEXT_ARG();
			if (strcmp(*argv, "inherit") == 0)
				flags |= IP6_TNL_F_USE_ORIG_FLOWLABEL;
			else {
				if (get_u32(&uval, *argv, 16))
					invarg("invalid Flowlabel", *argv);
				if (uval > 0xFFFFF)
					invarg("invalid Flowlabel", *argv);
				flowinfo |= htonl(uval) & IP6_FLOWINFO_FLOWLABEL;
				flags &= ~IP6_TNL_F_USE_ORIG_FLOWLABEL;
			}
		} else if (strcmp(*argv, "dscp") == 0) {
			NEXT_ARG();
			if (strcmp(*argv, "inherit") != 0)
				invarg("not inherit", *argv);
			flags |= IP6_TNL_F_RCV_DSCP_COPY;
		} else
			usage();
		argc--; argv++;
	}

	addattr32(n, 1024, IFLA_GRE_IKEY, ikey);
	addattr32(n, 1024, IFLA_GRE_OKEY, okey);
	addattr_l(n, 1024, IFLA_GRE_IFLAGS, &iflags, 2);
	addattr_l(n, 1024, IFLA_GRE_OFLAGS, &oflags, 2);
	addattr_l(n, 1024, IFLA_GRE_LOCAL, &laddr, sizeof(laddr));
	addattr_l(n, 1024, IFLA_GRE_REMOTE, &raddr, sizeof(raddr));
	if (link)
		addattr32(n, 1024, IFLA_GRE_LINK, link);
	addattr_l(n, 1024, IFLA_GRE_TTL, &hop_limit, 1);
	addattr_l(n, 1024, IFLA_GRE_ENCAP_LIMIT, &encap_limit, 1);
	addattr_l(n, 1024, IFLA_GRE_FLOWINFO, &flowinfo, 4);
	addattr_l(n, 1024, IFLA_GRE_FLAGS, &flowinfo, 4);

	return 0;
}

static void gre_print_opt(struct link_util *lu, FILE *f, struct rtattr *tb[])
{
	char s1[1024];
	char s2[64];
	const char *local = "any";
	const char *remote = "any";
	unsigned iflags = 0;
	unsigned oflags = 0;
	unsigned flags = 0;
	unsigned flowinfo = 0;
	struct in6_addr in6_addr_any = IN6ADDR_ANY_INIT;

	if (!tb)
		return;

	if (tb[IFLA_GRE_FLAGS])
		flags = rta_getattr_u32(tb[IFLA_GRE_FLAGS]);

	if (tb[IFLA_GRE_FLOWINFO])
		flags = rta_getattr_u32(tb[IFLA_GRE_FLOWINFO]);

	if (tb[IFLA_GRE_REMOTE]) {
		struct in6_addr addr;
		memcpy(&addr, RTA_DATA(tb[IFLA_GRE_REMOTE]), sizeof(addr));

		if (memcmp(&addr, &in6_addr_any, sizeof(addr)))
			remote = format_host(AF_INET6, sizeof(addr), &addr, s1, sizeof(s1));
	}

	fprintf(f, "remote %s ", remote);

	if (tb[IFLA_GRE_LOCAL]) {
		struct in6_addr addr;
		memcpy(&addr, RTA_DATA(tb[IFLA_GRE_LOCAL]), sizeof(addr));

		if (memcmp(&addr, &in6_addr_any, sizeof(addr)))
			local = format_host(AF_INET6, sizeof(addr), &addr, s1, sizeof(s1));
	}

	fprintf(f, "local %s ", local);

	if (tb[IFLA_GRE_LINK] && rta_getattr_u32(tb[IFLA_GRE_LINK])) {
		unsigned link = rta_getattr_u32(tb[IFLA_GRE_LINK]);
		const char *n = if_indextoname(link, s2);

		if (n)
			fprintf(f, "dev %s ", n);
		else
			fprintf(f, "dev %u ", link);
	}

	if (tb[IFLA_GRE_TTL] && rta_getattr_u8(tb[IFLA_GRE_TTL]))
		fprintf(f, "hoplimit %d ", rta_getattr_u8(tb[IFLA_GRE_TTL]));

	if (flags & IP6_TNL_F_IGN_ENCAP_LIMIT)
		fprintf(f, "encaplimit none ");
	else if (tb[IFLA_GRE_ENCAP_LIMIT]) {
		int encap_limit = rta_getattr_u8(tb[IFLA_GRE_ENCAP_LIMIT]);

		fprintf(f, "encaplimit %d ", encap_limit);
	}

	if (flags & IP6_TNL_F_USE_ORIG_FLOWLABEL)
		fprintf(f, "flowlabel inherit ");
	else
		fprintf(f, "flowlabel 0x%05x ", ntohl(flowinfo & IP6_FLOWINFO_FLOWLABEL));

	if (flags & IP6_TNL_F_RCV_DSCP_COPY)
		fprintf(f, "dscp inherit ");

	if (tb[IFLA_GRE_IFLAGS])
		iflags = rta_getattr_u16(tb[IFLA_GRE_IFLAGS]);

	if (tb[IFLA_GRE_OFLAGS])
		oflags = rta_getattr_u16(tb[IFLA_GRE_OFLAGS]);

	if ((iflags & GRE_KEY) && tb[IFLA_GRE_IKEY]) {
		inet_ntop(AF_INET, RTA_DATA(tb[IFLA_GRE_IKEY]), s2, sizeof(s2));
		fprintf(f, "ikey %s ", s2);
	}

	if ((oflags & GRE_KEY) && tb[IFLA_GRE_OKEY]) {
		inet_ntop(AF_INET, RTA_DATA(tb[IFLA_GRE_OKEY]), s2, sizeof(s2));
		fprintf(f, "okey %s ", s2);
	}

	if (iflags & GRE_SEQ)
		fputs("iseq ", f);
	if (oflags & GRE_SEQ)
		fputs("oseq ", f);
	if (iflags & GRE_CSUM)
		fputs("icsum ", f);
	if (oflags & GRE_CSUM)
		fputs("ocsum ", f);
}

static void gre_print_help(struct link_util *lu, int argc, char **argv,
	FILE *f)
{
	print_usage(f);
}

struct link_util ip6gre_link_util = {
	.id = "ip6gre",
	.maxattr = IFLA_GRE_MAX,
	.parse_opt = gre_parse_opt,
	.print_opt = gre_print_opt,
	.print_help = gre_print_help,
};

struct link_util ip6gretap_link_util = {
	.id = "ip6gretap",
	.maxattr = IFLA_GRE_MAX,
	.parse_opt = gre_parse_opt,
	.print_opt = gre_print_opt,
	.print_help = gre_print_help,
};
