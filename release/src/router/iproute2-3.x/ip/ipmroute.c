/*
 * ipmroute.c		"ip mroute".
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:	Alexey Kuznetsov, <kuznet@ms2.inr.ac.ru>
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <fcntl.h>
#include <inttypes.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

#include <linux/netdevice.h>
#include <linux/if.h>
#include <linux/if_arp.h>
#include <linux/sockios.h>

#include <rt_names.h>
#include "utils.h"
#include "ip_common.h"

static void usage(void) __attribute__((noreturn));

static void usage(void)
{
	fprintf(stderr, "Usage: ip mroute show [ [ to ] PREFIX ] [ from PREFIX ] [ iif DEVICE ]\n");
	fprintf(stderr, "                      [ table TABLE_ID ]\n");
	fprintf(stderr, "TABLE_ID := [ local | main | default | all | NUMBER ]\n");
#if 0
	fprintf(stderr, "Usage: ip mroute [ add | del ] DESTINATION from SOURCE [ iif DEVICE ] [ oif DEVICE ]\n");
#endif
	exit(-1);
}

struct rtfilter
{
	int tb;
	int af;
	int iif;
	inet_prefix mdst;
	inet_prefix msrc;
} filter;

int print_mroute(const struct sockaddr_nl *who, struct nlmsghdr *n, void *arg)
{
	FILE *fp = (FILE*)arg;
	struct rtmsg *r = NLMSG_DATA(n);
	int len = n->nlmsg_len;
	struct rtattr * tb[RTA_MAX+1];
	char abuf[256];
	char obuf[256];
	SPRINT_BUF(b1);
	__u32 table;
	int iif = 0;
	int family;

	if ((n->nlmsg_type != RTM_NEWROUTE &&
	     n->nlmsg_type != RTM_DELROUTE) ||
	    !(n->nlmsg_flags & NLM_F_MULTI)) {
		fprintf(stderr, "Not a multicast route: %08x %08x %08x\n",
			n->nlmsg_len, n->nlmsg_type, n->nlmsg_flags);
		return 0;
	}
	len -= NLMSG_LENGTH(sizeof(*r));
	if (len < 0) {
		fprintf(stderr, "BUG: wrong nlmsg len %d\n", len);
		return -1;
	}
	if (r->rtm_type != RTN_MULTICAST) {
		fprintf(stderr, "Not a multicast route (type: %s)\n",
			rtnl_rtntype_n2a(r->rtm_type, b1, sizeof(b1)));
		return 0;
	}

	parse_rtattr(tb, RTA_MAX, RTM_RTA(r), len);
	table = rtm_get_table(r, tb);

	if (filter.tb > 0 && filter.tb != table)
		return 0;

	if (tb[RTA_IIF])
		iif = *(int*)RTA_DATA(tb[RTA_IIF]);
	if (filter.iif && filter.iif != iif)
		return 0;

	if (filter.af && filter.af != r->rtm_family)
		return 0;

	if (tb[RTA_DST] &&
	    filter.mdst.bitlen > 0 &&
	    inet_addr_match(RTA_DATA(tb[RTA_DST]), &filter.mdst, filter.mdst.bitlen))
		return 0;

	if (tb[RTA_SRC] &&
	    filter.msrc.bitlen > 0 &&
	    inet_addr_match(RTA_DATA(tb[RTA_SRC]), &filter.msrc, filter.msrc.bitlen))
		return 0;

	family = r->rtm_family == RTNL_FAMILY_IPMR ? AF_INET : AF_INET6;

	if (n->nlmsg_type == RTM_DELROUTE)
		fprintf(fp, "Deleted ");

	if (tb[RTA_SRC])
		len = snprintf(obuf, sizeof(obuf),
			       "(%s, ", rt_addr_n2a(family,
						    RTA_DATA(tb[RTA_SRC]),
						    abuf, sizeof(abuf)));
	else
		len = sprintf(obuf, "(unknown, ");
	if (tb[RTA_DST])
		snprintf(obuf + len, sizeof(obuf) - len,
			 "%s)", rt_addr_n2a(family,
					    RTA_DATA(tb[RTA_DST]),
					    abuf, sizeof(abuf)));
	else
		snprintf(obuf + len, sizeof(obuf) - len, "unknown) ");

	fprintf(fp, "%-32s Iif: ", obuf);
	if (iif)
		fprintf(fp, "%-10s ", ll_index_to_name(iif));
	else
		fprintf(fp, "unresolved ");

	if (tb[RTA_MULTIPATH]) {
		struct rtnexthop *nh = RTA_DATA(tb[RTA_MULTIPATH]);
		int first = 1;

		len = RTA_PAYLOAD(tb[RTA_MULTIPATH]);

		for (;;) {
			if (len < sizeof(*nh))
				break;
			if (nh->rtnh_len > len)
				break;

			if (first) {
				fprintf(fp, "Oifs: ");
				first = 0;
			}
			fprintf(fp, "%s", ll_index_to_name(nh->rtnh_ifindex));
			if (nh->rtnh_hops > 1)
				fprintf(fp, "(ttl %d) ", nh->rtnh_hops);
			else
				fprintf(fp, " ");
			len -= NLMSG_ALIGN(nh->rtnh_len);
			nh = RTNH_NEXT(nh);
		}
	}
	if (show_stats && tb[RTA_MFC_STATS]) {
		struct rta_mfc_stats *mfcs = RTA_DATA(tb[RTA_MFC_STATS]);

		fprintf(fp, "%s  %"PRIu64" packets, %"PRIu64" bytes", _SL_,
			(uint64_t)mfcs->mfcs_packets,
			(uint64_t)mfcs->mfcs_bytes);
		if (mfcs->mfcs_wrong_if)
			fprintf(fp, ", %"PRIu64" arrived on wrong iif.",
				(uint64_t)mfcs->mfcs_wrong_if);
	}
	fprintf(fp, "\n");
	fflush(fp);
	return 0;
}

void ipmroute_reset_filter(int ifindex)
{
	memset(&filter, 0, sizeof(filter));
	filter.mdst.bitlen = -1;
	filter.msrc.bitlen = -1;
	filter.iif = ifindex;
}

static int mroute_list(int argc, char **argv)
{
	char *id = NULL;
	int family;

	ipmroute_reset_filter(0);
	if (preferred_family == AF_UNSPEC)
		family = AF_INET;
	else
		family = AF_INET6;
	if (family == AF_INET) {
		filter.af = RTNL_FAMILY_IPMR;
		filter.tb = RT_TABLE_DEFAULT;  /* for backward compatibility */
	} else
		filter.af = RTNL_FAMILY_IP6MR;

	while (argc > 0) {
		if (matches(*argv, "table") == 0) {
			__u32 tid;
			NEXT_ARG();
			if (rtnl_rttable_a2n(&tid, *argv)) {
				if (strcmp(*argv, "all") == 0) {
					filter.tb = 0;
				} else if (strcmp(*argv, "help") == 0) {
					usage();
				} else {
					invarg("table id value is invalid\n", *argv);
				}
			} else
				filter.tb = tid;
		} else if (strcmp(*argv, "iif") == 0) {
			NEXT_ARG();
			id = *argv;
		} else if (matches(*argv, "from") == 0) {
			NEXT_ARG();
			get_prefix(&filter.msrc, *argv, family);
		} else {
			if (strcmp(*argv, "to") == 0) {
				NEXT_ARG();
			}
			if (matches(*argv, "help") == 0)
				usage();
			get_prefix(&filter.mdst, *argv, family);
		}
		argc--; argv++;
	}

	ll_init_map(&rth);

	if (id)  {
		int idx;

		if ((idx = ll_name_to_index(id)) == 0) {
			fprintf(stderr, "Cannot find device \"%s\"\n", id);
			return -1;
		}
		filter.iif = idx;
	}

	if (rtnl_wilddump_request(&rth, filter.af, RTM_GETROUTE) < 0) {
		perror("Cannot send dump request");
		return 1;
	}

	if (rtnl_dump_filter(&rth, print_mroute, stdout) < 0) {
		fprintf(stderr, "Dump terminated\n");
		exit(1);
	}

	exit(0);
}

int do_multiroute(int argc, char **argv)
{
	if (argc < 1)
		return mroute_list(0, NULL);
#if 0
	if (matches(*argv, "add") == 0)
		return mroute_modify(RTM_NEWADDR, argc-1, argv+1);
	if (matches(*argv, "delete") == 0)
		return mroute_modify(RTM_DELADDR, argc-1, argv+1);
	if (matches(*argv, "get") == 0)
		return mroute_get(argc-1, argv+1);
#endif
	if (matches(*argv, "list") == 0 || matches(*argv, "show") == 0
	    || matches(*argv, "lst") == 0)
		return mroute_list(argc-1, argv+1);
	if (matches(*argv, "help") == 0)
		usage();
	fprintf(stderr, "Command \"%s\" is unknown, try \"ip mroute help\".\n", *argv);
	exit(-1);
}
