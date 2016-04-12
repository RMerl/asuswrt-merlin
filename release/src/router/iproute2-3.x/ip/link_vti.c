/*
 * link_vti.c	VTI driver module
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:	Herbert Xu <herbert@gondor.apana.org.au>
 *          Saurabh Mohan <saurabh.mohan@vyatta.com> Modified link_gre.c for VTI
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


static void print_usage(FILE *f)
{
	fprintf(f, "Usage: ip link { add | set | change | replace | del } NAME\n");
	fprintf(f, "          type { vti } [ remote ADDR ] [ local ADDR ]\n");
	fprintf(f, "          [ [i|o]key KEY ]\n");
	fprintf(f, "          [ dev PHYS_DEV ]\n");
	fprintf(f, "\n");
	fprintf(f, "Where: NAME := STRING\n");
	fprintf(f, "       ADDR := { IP_ADDRESS }\n");
	fprintf(f, "       KEY  := { DOTTED_QUAD | NUMBER }\n");
}

static void usage(void) __attribute__((noreturn));
static void usage(void)
{
	print_usage(stderr);
	exit(-1);
}

static int vti_parse_opt(struct link_util *lu, int argc, char **argv,
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
	struct rtattr *vtiinfo[IFLA_VTI_MAX + 1];
	unsigned ikey = 0;
	unsigned okey = 0;
	unsigned saddr = 0;
	unsigned daddr = 0;
	unsigned link = 0;
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

		parse_rtattr_nested(vtiinfo, IFLA_VTI_MAX,
				    linkinfo[IFLA_INFO_DATA]);

		if (vtiinfo[IFLA_VTI_IKEY])
			ikey = *(__u32 *)RTA_DATA(vtiinfo[IFLA_VTI_IKEY]);

		if (vtiinfo[IFLA_VTI_OKEY])
			okey = *(__u32 *)RTA_DATA(vtiinfo[IFLA_VTI_OKEY]);

		if (vtiinfo[IFLA_VTI_LOCAL])
			saddr = *(__u32 *)RTA_DATA(vtiinfo[IFLA_VTI_LOCAL]);

		if (vtiinfo[IFLA_VTI_REMOTE])
			daddr = *(__u32 *)RTA_DATA(vtiinfo[IFLA_VTI_REMOTE]);

		if (vtiinfo[IFLA_VTI_LINK])
			link = *(__u8 *)RTA_DATA(vtiinfo[IFLA_VTI_LINK]);
	}

	while (argc > 0) {
		if (!matches(*argv, "key")) {
			unsigned uval;

			NEXT_ARG();
			if (strchr(*argv, '.'))
				uval = get_addr32(*argv);
			else {
				if (get_unsigned(&uval, *argv, 0) < 0) {
					fprintf(stderr,
						"Invalid value for \"key\": \"%s\"; it should be an unsigned integer\n", *argv);
					exit(-1);
				}
				uval = htonl(uval);
			}

			ikey = okey = uval;
		} else if (!matches(*argv, "ikey")) {
			unsigned uval;

			NEXT_ARG();
			if (strchr(*argv, '.'))
				uval = get_addr32(*argv);
			else {
				if (get_unsigned(&uval, *argv, 0) < 0) {
					fprintf(stderr, "invalid value for \"ikey\": \"%s\"; it should be an unsigned integer\n", *argv);
					exit(-1);
				}
				uval = htonl(uval);
			}
			ikey = uval;
		} else if (!matches(*argv, "okey")) {
			unsigned uval;

			NEXT_ARG();
			if (strchr(*argv, '.'))
				uval = get_addr32(*argv);
			else {
				if (get_unsigned(&uval, *argv, 0) < 0) {
					fprintf(stderr, "invalid value for \"okey\": \"%s\"; it should be an unsigned integer\n", *argv);
					exit(-1);
				}
				uval = htonl(uval);
			}
			okey = uval;
		} else if (!matches(*argv, "remote")) {
			NEXT_ARG();
			if (!strcmp(*argv, "any")) {
				fprintf(stderr, "invalid value for \"remote\": \"%s\"\n", *argv);
				exit(-1);
			} else {
				daddr = get_addr32(*argv);
			}
		} else if (!matches(*argv, "local")) {
			NEXT_ARG();
			if (!strcmp(*argv, "any")) {
				fprintf(stderr, "invalid value for \"local\": \"%s\"\n", *argv);
				exit(-1);
			} else {
				saddr = get_addr32(*argv);
			}
		} else if (!matches(*argv, "dev")) {
			NEXT_ARG();
			link = if_nametoindex(*argv);
			if (link == 0) {
				fprintf(stderr, "Cannot find device \"%s\"\n",
					*argv);
				exit(-1);
			}
		} else
			usage();
		argc--; argv++;
	}

	addattr32(n, 1024, IFLA_VTI_IKEY, ikey);
	addattr32(n, 1024, IFLA_VTI_OKEY, okey);
	addattr_l(n, 1024, IFLA_VTI_LOCAL, &saddr, 4);
	addattr_l(n, 1024, IFLA_VTI_REMOTE, &daddr, 4);
	if (link)
		addattr32(n, 1024, IFLA_VTI_LINK, link);

	return 0;
}

static void vti_print_opt(struct link_util *lu, FILE *f, struct rtattr *tb[])
{
	char s1[1024];
	char s2[64];
	const char *local = "any";
	const char *remote = "any";

	if (!tb)
		return;

	if (tb[IFLA_VTI_REMOTE]) {
		unsigned addr = *(__u32 *)RTA_DATA(tb[IFLA_VTI_REMOTE]);

		if (addr)
			remote = format_host(AF_INET, 4, &addr, s1, sizeof(s1));
	}

	fprintf(f, "remote %s ", remote);

	if (tb[IFLA_VTI_LOCAL]) {
		unsigned addr = *(__u32 *)RTA_DATA(tb[IFLA_VTI_LOCAL]);

		if (addr)
			local = format_host(AF_INET, 4, &addr, s1, sizeof(s1));
	}

	fprintf(f, "local %s ", local);

	if (tb[IFLA_VTI_LINK] && *(__u32 *)RTA_DATA(tb[IFLA_VTI_LINK])) {
		unsigned link = *(__u32 *)RTA_DATA(tb[IFLA_VTI_LINK]);
		const char *n = if_indextoname(link, s2);

		if (n)
			fprintf(f, "dev %s ", n);
		else
			fprintf(f, "dev %u ", link);
	}

	if (tb[IFLA_VTI_IKEY]) {
		inet_ntop(AF_INET, RTA_DATA(tb[IFLA_VTI_IKEY]), s2, sizeof(s2));
		fprintf(f, "ikey %s ", s2);
	}

	if (tb[IFLA_VTI_OKEY]) {
		inet_ntop(AF_INET, RTA_DATA(tb[IFLA_VTI_OKEY]), s2, sizeof(s2));
		fprintf(f, "okey %s ", s2);
	}
}

static void vti_print_help(struct link_util *lu, int argc, char **argv,
	FILE *f)
{
	print_usage(f);
}

struct link_util vti_link_util = {
	.id = "vti",
	.maxattr = IFLA_VTI_MAX,
	.parse_opt = vti_parse_opt,
	.print_opt = vti_print_opt,
	.print_help = vti_print_help,
};
