/*
 * iprule.c		"ip rule".
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:	Alexey Kuznetsov, <kuznet@ms2.inr.ac.ru>
 *
 *
 * Changes:
 *
 * Rani Assaf <rani@magic.metawire.com> 980929:	resolve addresses
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <string.h>

#include "rt_names.h"
#include "utils.h"

extern struct rtnl_handle rth;

static void usage(void) __attribute__((noreturn));

static void usage(void)
{
	fprintf(stderr, "Usage: ip rule [ list | add | del | flush ] SELECTOR ACTION\n");
	fprintf(stderr, "SELECTOR := [ from PREFIX ] [ to PREFIX ] [ tos TOS ] [ fwmark FWMARK ]\n");
	fprintf(stderr, "            [ dev STRING ] [ pref NUMBER ]\n");
	fprintf(stderr, "ACTION := [ table TABLE_ID ]\n");
	fprintf(stderr, "          [ prohibit | reject | unreachable ]\n");
	fprintf(stderr, "          [ realms [SRCREALM/]DSTREALM ]\n");
	fprintf(stderr, "TABLE_ID := [ local | main | default | NUMBER ]\n");
	exit(-1);
}

static int print_rule(const struct sockaddr_nl *who, struct nlmsghdr *n,
		      void *arg)
{
	FILE *fp = (FILE*)arg;
	struct rtmsg *r = NLMSG_DATA(n);
	int len = n->nlmsg_len;
	int host_len = -1;
	struct rtattr * tb[RTA_MAX+1];
	char abuf[256];
	SPRINT_BUF(b1);

	if (n->nlmsg_type != RTM_NEWRULE)
		return 0;

	len -= NLMSG_LENGTH(sizeof(*r));
	if (len < 0)
		return -1;

	parse_rtattr(tb, RTA_MAX, RTM_RTA(r), len);

	if (r->rtm_family == AF_INET)
		host_len = 32;
	else if (r->rtm_family == AF_INET6)
		host_len = 128;
	else if (r->rtm_family == AF_DECnet)
		host_len = 16;
	else if (r->rtm_family == AF_IPX)
		host_len = 80;

	if (tb[RTA_PRIORITY])
		fprintf(fp, "%u:\t", *(unsigned*)RTA_DATA(tb[RTA_PRIORITY]));
	else
		fprintf(fp, "0:\t");

	if (tb[RTA_SRC]) {
		if (r->rtm_src_len != host_len) {
			fprintf(fp, "from %s/%u ", rt_addr_n2a(r->rtm_family,
							 RTA_PAYLOAD(tb[RTA_SRC]),
							 RTA_DATA(tb[RTA_SRC]),
							 abuf, sizeof(abuf)),
				r->rtm_src_len
				);
		} else {
			fprintf(fp, "from %s ", format_host(r->rtm_family,
						       RTA_PAYLOAD(tb[RTA_SRC]),
						       RTA_DATA(tb[RTA_SRC]),
						       abuf, sizeof(abuf))
				);
		}
	} else if (r->rtm_src_len) {
		fprintf(fp, "from 0/%d ", r->rtm_src_len);
	} else {
		fprintf(fp, "from all ");
	}

	if (tb[RTA_DST]) {
		if (r->rtm_dst_len != host_len) {
			fprintf(fp, "to %s/%u ", rt_addr_n2a(r->rtm_family,
							 RTA_PAYLOAD(tb[RTA_DST]),
							 RTA_DATA(tb[RTA_DST]),
							 abuf, sizeof(abuf)),
				r->rtm_dst_len
				);
		} else {
			fprintf(fp, "to %s ", format_host(r->rtm_family,
						       RTA_PAYLOAD(tb[RTA_DST]),
						       RTA_DATA(tb[RTA_DST]),
						       abuf, sizeof(abuf)));
		}
	} else if (r->rtm_dst_len) {
		fprintf(fp, "to 0/%d ", r->rtm_dst_len);
	}

	if (r->rtm_tos) {
		SPRINT_BUF(b1);
		fprintf(fp, "tos %s ", rtnl_dsfield_n2a(r->rtm_tos, b1, sizeof(b1)));
	}
	if (tb[RTA_PROTOINFO]) {
		fprintf(fp, "fwmark %#x ", *(__u32*)RTA_DATA(tb[RTA_PROTOINFO]));
	}

	if (tb[RTA_IIF]) {
		fprintf(fp, "iif %s ", (char*)RTA_DATA(tb[RTA_IIF]));
	}

	if (r->rtm_table)
		fprintf(fp, "lookup %s ", rtnl_rttable_n2a(r->rtm_table, b1, sizeof(b1)));

	if (tb[RTA_FLOW]) {
		__u32 to = *(__u32*)RTA_DATA(tb[RTA_FLOW]);
		__u32 from = to>>16;
		to &= 0xFFFF;
		if (from) {
			fprintf(fp, "realms %s/",
				rtnl_rtrealm_n2a(from, b1, sizeof(b1)));
		}
		fprintf(fp, "%s ",
			rtnl_rtrealm_n2a(to, b1, sizeof(b1)));
	}

	if (r->rtm_type == RTN_NAT) {
		if (tb[RTA_GATEWAY]) {
			fprintf(fp, "map-to %s ", 
				format_host(r->rtm_family,
					    RTA_PAYLOAD(tb[RTA_GATEWAY]),
					    RTA_DATA(tb[RTA_GATEWAY]),
					    abuf, sizeof(abuf)));
		} else
			fprintf(fp, "masquerade");
	} else if (r->rtm_type != RTN_UNICAST)
		fprintf(fp, "%s", rtnl_rtntype_n2a(r->rtm_type, b1, sizeof(b1)));

	fprintf(fp, "\n");
	fflush(fp);
	return 0;
}

static int iprule_list(int argc, char **argv)
{
	int af = preferred_family;

	if (af == AF_UNSPEC)
		af = AF_INET;

	if (argc > 0) {
		fprintf(stderr, "\"ip rule show\" does not take any arguments.\n");
		return -1;
	}

	if (rtnl_wilddump_request(&rth, af, RTM_GETRULE) < 0) {
		perror("Cannot send dump request");
		return 1;
	}

	if (rtnl_dump_filter(&rth, print_rule, stdout, NULL, NULL) < 0) {
		fprintf(stderr, "Dump terminated\n");
		return 1;
	}

	return 0;
}


static int iprule_modify(int cmd, int argc, char **argv)
{
	int table_ok = 0;
	struct {
		struct nlmsghdr 	n;
		struct rtmsg 		r;
		char   			buf[1024];
	} req;

	memset(&req, 0, sizeof(req));

	req.n.nlmsg_type = cmd;
	req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
	req.n.nlmsg_flags = NLM_F_REQUEST;
	req.r.rtm_family = preferred_family;
	req.r.rtm_protocol = RTPROT_BOOT;
	req.r.rtm_scope = RT_SCOPE_UNIVERSE;
	req.r.rtm_table = 0;
	req.r.rtm_type = RTN_UNSPEC;

	if (cmd == RTM_NEWRULE) {
		req.n.nlmsg_flags |= NLM_F_CREATE|NLM_F_EXCL;
		req.r.rtm_type = RTN_UNICAST;
	}

	while (argc > 0) {
		if (strcmp(*argv, "from") == 0) {
			inet_prefix dst;
			NEXT_ARG();
			get_prefix(&dst, *argv, req.r.rtm_family);
			req.r.rtm_src_len = dst.bitlen;
			addattr_l(&req.n, sizeof(req), RTA_SRC, &dst.data, dst.bytelen);
		} else if (strcmp(*argv, "to") == 0) {
			inet_prefix dst;
			NEXT_ARG();
			get_prefix(&dst, *argv, req.r.rtm_family);
			req.r.rtm_dst_len = dst.bitlen;
			addattr_l(&req.n, sizeof(req), RTA_DST, &dst.data, dst.bytelen);
		} else if (matches(*argv, "preference") == 0 ||
			   matches(*argv, "order") == 0 ||
			   matches(*argv, "priority") == 0) {
			__u32 pref;
			NEXT_ARG();
			if (get_u32(&pref, *argv, 0))
				invarg("preference value is invalid\n", *argv);
			addattr32(&req.n, sizeof(req), RTA_PRIORITY, pref);
		} else if (strcmp(*argv, "tos") == 0) {
			__u32 tos;
			NEXT_ARG();
			if (rtnl_dsfield_a2n(&tos, *argv))
				invarg("TOS value is invalid\n", *argv);
			req.r.rtm_tos = tos;
		} else if (strcmp(*argv, "fwmark") == 0) {
			__u32 fwmark;
			NEXT_ARG();
			if (get_u32(&fwmark, *argv, 0))
				invarg("fwmark value is invalid\n", *argv);
			addattr32(&req.n, sizeof(req), RTA_PROTOINFO, fwmark);
		} else if (matches(*argv, "realms") == 0) {
			__u32 realm;
			NEXT_ARG();
			if (get_rt_realms(&realm, *argv))
				invarg("invalid realms\n", *argv);
			addattr32(&req.n, sizeof(req), RTA_FLOW, realm);
		} else if (matches(*argv, "table") == 0 ||
			   strcmp(*argv, "lookup") == 0) {
			__u32 tid;
			NEXT_ARG();
			if (rtnl_rttable_a2n(&tid, *argv))
				invarg("invalid table ID\n", *argv);
			req.r.rtm_table = tid;
			table_ok = 1;
		} else if (strcmp(*argv, "dev") == 0 ||
			   strcmp(*argv, "iif") == 0) {
			NEXT_ARG();
			addattr_l(&req.n, sizeof(req), RTA_IIF, *argv, strlen(*argv)+1);
		} else if (strcmp(*argv, "nat") == 0 ||
			   matches(*argv, "map-to") == 0) {
			NEXT_ARG();
			fprintf(stderr, "Warning: route NAT is deprecated\n");
			addattr32(&req.n, sizeof(req), RTA_GATEWAY, get_addr32(*argv));
			req.r.rtm_type = RTN_NAT;
		} else {
			int type;

			if (strcmp(*argv, "type") == 0) {
				NEXT_ARG();
			}
			if (matches(*argv, "help") == 0)
				usage();
			if (rtnl_rtntype_a2n(&type, *argv))
				invarg("Failed to parse rule type", *argv);
			req.r.rtm_type = type;
		}
		argc--;
		argv++;
	}

	if (req.r.rtm_family == AF_UNSPEC)
		req.r.rtm_family = AF_INET;

	if (!table_ok && cmd == RTM_NEWRULE)
		req.r.rtm_table = RT_TABLE_MAIN;

	if (rtnl_talk(&rth, &req.n, 0, 0, NULL, NULL, NULL) < 0)
		return 2;

	return 0;
}


static int flush_rule(const struct sockaddr_nl *who, struct nlmsghdr *n, void *arg)
{
	struct rtnl_handle rth2;
	struct rtmsg *r = NLMSG_DATA(n);
	int len = n->nlmsg_len;
	struct rtattr * tb[RTA_MAX+1];

	len -= NLMSG_LENGTH(sizeof(*r));
	if (len < 0)
		return -1;

	parse_rtattr(tb, RTA_MAX, RTM_RTA(r), len);

	if (tb[RTA_PRIORITY]) {
		n->nlmsg_type = RTM_DELRULE;
		n->nlmsg_flags = NLM_F_REQUEST;

		if (rtnl_open(&rth2, 0) < 0)
			return -1;

		if (rtnl_talk(&rth2, n, 0, 0, NULL, NULL, NULL) < 0)
			return -2;

		rtnl_close(&rth2);
	}

	return 0;
}

static int iprule_flush(int argc, char **argv)
{
	int af = preferred_family;

	if (af == AF_UNSPEC)
		af = AF_INET;

	if (argc > 0) {
		fprintf(stderr, "\"ip rule flush\" does not allow arguments\n");
		return -1;
	}

	if (rtnl_wilddump_request(&rth, af, RTM_GETRULE) < 0) {
		perror("Cannot send dump request");
		return 1;
	}

	if (rtnl_dump_filter(&rth, flush_rule, NULL, NULL, NULL) < 0) {
		fprintf(stderr, "Flush terminated\n");
		return 1;
	}

	return 0;
}

int do_iprule(int argc, char **argv)
{
	if (argc < 1) {
		return iprule_list(0, NULL);
	} else if (matches(argv[0], "list") == 0 ||
		   matches(argv[0], "lst") == 0 ||
		   matches(argv[0], "show") == 0) {
		return iprule_list(argc-1, argv+1);
	} else if (matches(argv[0], "add") == 0) {
		return iprule_modify(RTM_NEWRULE, argc-1, argv+1);
	} else if (matches(argv[0], "delete") == 0) {
		return iprule_modify(RTM_DELRULE, argc-1, argv+1);
	} else if (matches(argv[0], "flush") == 0) {
		return iprule_flush(argc-1, argv+1);
	} else if (matches(argv[0], "help") == 0)
		usage();

	fprintf(stderr, "Command \"%s\" is unknown, try \"ip rule help\".\n", *argv);
	exit(-1);
}

