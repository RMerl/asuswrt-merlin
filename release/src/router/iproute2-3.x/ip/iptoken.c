/*
 * iptoken.c    "ip token"
 *
 *              This program is free software; you can redistribute it and/or
 *              modify it under the terms of the GNU General Public License
 *              as published by the Free Software Foundation; either version
 *              2 of the License, or (at your option) any later version.
 *
 * Authors:     Daniel Borkmann, <borkmann@redhat.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <syslog.h>
#include <fcntl.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <linux/types.h>
#include <linux/if.h>

#include "rt_names.h"
#include "utils.h"
#include "ip_common.h"

extern struct rtnl_handle rth;

struct rtnl_dump_args {
	FILE *fp;
	int ifindex;
};

static void usage(void) __attribute__((noreturn));

static void usage(void)
{
	fprintf(stderr, "Usage: ip token [ list | set | get ] [ TOKEN ] [ dev DEV ]\n");
	exit(-1);
}

static int print_token(const struct sockaddr_nl *who, struct nlmsghdr *n, void *arg)
{
	struct rtnl_dump_args *args = arg;
	FILE *fp = args->fp;
	int ifindex = args->ifindex;
	struct ifinfomsg *ifi = NLMSG_DATA(n);
	int len = n->nlmsg_len;
	struct rtattr *tb[IFLA_MAX + 1];
	struct rtattr *ltb[IFLA_INET6_MAX + 1];
	char abuf[256];

	if (n->nlmsg_type != RTM_NEWLINK)
		return -1;

	len -= NLMSG_LENGTH(sizeof(*ifi));
	if (len < 0)
		return -1;

	if (ifi->ifi_family != AF_INET6)
		return -1;
	if (ifi->ifi_index == 0)
		return -1;
	if (ifindex > 0 && ifi->ifi_index != ifindex)
		return 0;
	if (ifi->ifi_flags & (IFF_LOOPBACK | IFF_NOARP))
		return 0;

	parse_rtattr(tb, IFLA_MAX, IFLA_RTA(ifi), len);
	if (!tb[IFLA_PROTINFO])
		return -1;

	parse_rtattr_nested(ltb, IFLA_INET6_MAX, tb[IFLA_PROTINFO]);
	if (!ltb[IFLA_INET6_TOKEN]) {
		fprintf(stderr, "Seems there's no support for IPv6 token!\n");
		return -1;
	}

	fprintf(fp, "token %s ",
		format_host(ifi->ifi_family,
			    RTA_PAYLOAD(ltb[IFLA_INET6_TOKEN]),
			    RTA_DATA(ltb[IFLA_INET6_TOKEN]),
			    abuf, sizeof(abuf)));
	fprintf(fp, "dev %s ", ll_index_to_name(ifi->ifi_index));
	fprintf(fp, "\n");
	fflush(fp);

	return 0;
}

static int iptoken_list(int argc, char **argv)
{
	int af = AF_INET6;
	struct rtnl_dump_args da;
	const struct rtnl_dump_filter_arg a[2] = {
		{ .filter = print_token, .arg1 = &da, },
		{ .filter = NULL, .arg1 = NULL, },
	};

	memset(&da, 0, sizeof(da));
	da.fp = stdout;

	while (argc > 0) {
		if (strcmp(*argv, "dev") == 0) {
			NEXT_ARG();
			if ((da.ifindex = ll_name_to_index(*argv)) == 0)
				invarg("dev is invalid\n", *argv);
			break;
		}
		argc--; argv++;
	}

	if (rtnl_wilddump_request(&rth, af, RTM_GETLINK) < 0) {
		perror("Cannot send dump request");
		return -1;
	}

	if (rtnl_dump_filter_l(&rth, a) < 0) {
		fprintf(stderr, "Dump terminated\n");
		return -1;
	}

	return 0;
}

static int iptoken_set(int argc, char **argv)
{
	struct {
		struct nlmsghdr n;
		struct ifinfomsg ifi;
		char buf[512];
	} req;
	struct rtattr *afs, *afs6;
	bool have_token = false, have_dev = false;
	inet_prefix addr;

	memset(&addr, 0, sizeof(addr));
	memset(&req, 0, sizeof(req));

	req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg));
	req.n.nlmsg_flags = NLM_F_REQUEST;
	req.n.nlmsg_type = RTM_SETLINK;
	req.ifi.ifi_family = AF_INET6;

	while (argc > 0) {
		if (strcmp(*argv, "dev") == 0) {
			NEXT_ARG();
			if (!have_dev) {
				if ((req.ifi.ifi_index =
				     ll_name_to_index(*argv)) == 0)
					invarg("dev is invalid\n", *argv);
				have_dev = true;
			}
		} else {
			if (matches(*argv, "help") == 0)
				usage();
			if (!have_token) {
				afs = addattr_nest(&req.n, sizeof(req), IFLA_AF_SPEC);
				afs6 = addattr_nest(&req.n, sizeof(req), AF_INET6);
				get_prefix(&addr, *argv, req.ifi.ifi_family);
				addattr_l(&req.n, sizeof(req), IFLA_INET6_TOKEN,
					  &addr.data, addr.bytelen);
				addattr_nest_end(&req.n, afs6);
				addattr_nest_end(&req.n, afs);
				have_token = true;
			}
		}
		argc--; argv++;
	}

	if (!have_token) {
		fprintf(stderr, "Not enough information: token "
			"is required.\n");
		return -1;
	}
	if (!have_dev) {
		fprintf(stderr, "Not enough information: \"dev\" "
			"argument is required.\n");
		return -1;
	}

	if (rtnl_talk(&rth, &req.n, 0, 0, NULL) < 0)
		return -2;

	return 0;
}

int do_iptoken(int argc, char **argv)
{
	ll_init_map(&rth);

	if (argc < 1) {
		return iptoken_list(0, NULL);
	} else if (matches(argv[0], "list") == 0 ||
		   matches(argv[0], "show") == 0) {
		return iptoken_list(argc - 1, argv + 1);
	} else if (matches(argv[0], "set") == 0 ||
		   matches(argv[0], "add") == 0) {
		return iptoken_set(argc - 1, argv + 1);
	} else if (matches(argv[0], "get") == 0) {
		return iptoken_list(argc - 1, argv + 1);
	} else if (matches(argv[0], "help") == 0)
		usage();

	fprintf(stderr, "Command \"%s\" is unknown, try \"ip token help\".\n", *argv);
	exit(-1);
}
