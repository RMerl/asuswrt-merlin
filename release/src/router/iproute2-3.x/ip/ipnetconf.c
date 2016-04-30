/*
 * ipnetconf.c		"ip netconf".
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:	Nicolas Dichtel, <nicolas.dichtel@6wind.com>
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <fcntl.h>
#include <string.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "rt_names.h"
#include "utils.h"
#include "ip_common.h"

static struct
{
	int family;
	int ifindex;
} filter;

static void usage(void) __attribute__((noreturn));

static void usage(void)
{
	fprintf(stderr, "Usage: ip netconf show [ dev STRING ]\n");
	exit(-1);
}

#define NETCONF_RTA(r)	((struct rtattr*)(((char*)(r)) + NLMSG_ALIGN(sizeof(struct netconfmsg))))

int print_netconf(const struct sockaddr_nl *who, struct nlmsghdr *n, void *arg)
{
	FILE *fp = (FILE*)arg;
	struct netconfmsg *ncm = NLMSG_DATA(n);
	int len = n->nlmsg_len;
	struct rtattr *tb[NETCONFA_MAX+1];

	if (n->nlmsg_type == NLMSG_ERROR)
		return -1;
	if (n->nlmsg_type != RTM_NEWNETCONF) {
		fprintf(stderr, "Not RTM_NEWNETCONF: %08x %08x %08x\n",
			n->nlmsg_len, n->nlmsg_type, n->nlmsg_flags);

		return -1;
	}
	len -= NLMSG_SPACE(sizeof(*ncm));
	if (len < 0) {
		fprintf(stderr, "BUG: wrong nlmsg len %d\n", len);
		return -1;
	}

	if (filter.family && filter.family != ncm->ncm_family)
		return 0;

	parse_rtattr(tb, NETCONFA_MAX, NETCONF_RTA(ncm),
		     NLMSG_PAYLOAD(n, sizeof(*ncm)));

	switch (ncm->ncm_family) {
	case AF_INET:
		fprintf(fp, "ipv4 ");
		break;
	case AF_INET6:
		fprintf(fp, "ipv6 ");
		break;
	default:
		fprintf(fp, "unknown ");
		break;
	}

	if (tb[NETCONFA_IFINDEX]) {
		int *ifindex = (int *)RTA_DATA(tb[NETCONFA_IFINDEX]);

		switch (*ifindex) {
		case NETCONFA_IFINDEX_ALL:
			fprintf(fp, "all ");
			break;
		case NETCONFA_IFINDEX_DEFAULT:
			fprintf(fp, "default ");
			break;
		default:
			fprintf(fp, "dev %s ", ll_index_to_name(*ifindex));
			break;
		}
	}

	if (tb[NETCONFA_FORWARDING])
		fprintf(fp, "forwarding %s ",
			*(int *)RTA_DATA(tb[NETCONFA_FORWARDING])?"on":"off");
	if (tb[NETCONFA_RP_FILTER]) {
		int rp_filter = *(int *)RTA_DATA(tb[NETCONFA_RP_FILTER]);

		if (rp_filter == 0)
			fprintf(fp, "rp_filter off ");
		else if (rp_filter == 1)
			fprintf(fp, "rp_filter strict ");
		else if (rp_filter == 2)
			fprintf(fp, "rp_filter loose ");
		else
			fprintf(fp, "rp_filter unknown mode ");
	}
	if (tb[NETCONFA_MC_FORWARDING])
		fprintf(fp, "mc_forwarding %d ",
			*(int *)RTA_DATA(tb[NETCONFA_MC_FORWARDING]));

	if (tb[NETCONFA_PROXY_NEIGH])
		fprintf(fp, "proxy_neigh %s ",
			*(int *)RTA_DATA(tb[NETCONFA_PROXY_NEIGH])?"on":"off");

	fprintf(fp, "\n");
	fflush(fp);
	return 0;
}

void ipnetconf_reset_filter(int ifindex)
{
	memset(&filter, 0, sizeof(filter));
	filter.ifindex = ifindex;
}

static int do_show(int argc, char **argv)
{
	struct {
		struct nlmsghdr		n;
		struct netconfmsg	ncm;
		char			buf[1024];
	} req;

	ipnetconf_reset_filter(0);
	filter.family = preferred_family;
	if (filter.family == AF_UNSPEC)
		filter.family = AF_INET;

	while (argc > 0) {
		if (strcmp(*argv, "dev") == 0) {
			NEXT_ARG();
			filter.ifindex = ll_name_to_index(*argv);
			if (filter.ifindex <= 0) {
				fprintf(stderr, "Device \"%s\" does not exist.\n",
					*argv);
				return -1;
			}
		}
		argv++; argc--;
	}

	ll_init_map(&rth);
	if (filter.ifindex) {
		memset(&req, 0, sizeof(req));
		req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct netconfmsg));
		req.n.nlmsg_flags = NLM_F_REQUEST|NLM_F_ACK;
		req.n.nlmsg_type = RTM_GETNETCONF;
		req.ncm.ncm_family = filter.family;
		if (filter.ifindex)
			addattr_l(&req.n, sizeof(req), NETCONFA_IFINDEX,
				  &filter.ifindex, sizeof(filter.ifindex));

		if (rtnl_send(&rth, &req.n, req.n.nlmsg_len) < 0) {
			perror("Can not send request");
			exit(1);
		}
		rtnl_listen(&rth, print_netconf, stdout);
	} else {
dump:
		if (rtnl_wilddump_request(&rth, filter.family, RTM_GETNETCONF) < 0) {
			perror("Cannot send dump request");
			exit(1);
		}
		if (rtnl_dump_filter(&rth, print_netconf, stdout) < 0) {
			fprintf(stderr, "Dump terminated\n");
			exit(1);
		}
		if (preferred_family == AF_UNSPEC) {
			preferred_family = AF_INET6;
			filter.family = AF_INET6;
			goto dump;
		}
	}
	return 0;
}

int do_ipnetconf(int argc, char **argv)
{
	if (argc > 0) {
		if (matches(*argv, "show") == 0 ||
		    matches(*argv, "lst") == 0 ||
		    matches(*argv, "list") == 0)
			return do_show(argc-1, argv+1);
		if (matches(*argv, "help") == 0)
			usage();
	} else
		return do_show(0, NULL);

	fprintf(stderr, "Command \"%s\" is unknown, try \"ip netconf help\".\n", *argv);
	exit(-1);
}
