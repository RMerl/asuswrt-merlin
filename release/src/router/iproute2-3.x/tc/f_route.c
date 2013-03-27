/*
 * f_route.c		ROUTE filter.
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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

#include "utils.h"
#include "rt_names.h"
#include "tc_common.h"
#include "tc_util.h"

static void explain(void)
{
	fprintf(stderr, "Usage: ... route [ from REALM | fromif TAG ] [ to REALM ]\n");
	fprintf(stderr, "                [ flowid CLASSID ] [ police POLICE_SPEC ]\n");
	fprintf(stderr, "       POLICE_SPEC := ... look at TBF\n");
	fprintf(stderr, "       CLASSID := X:Y\n");
	fprintf(stderr, "\nNOTE: CLASSID is parsed as hexadecimal input.\n");
}

static int route_parse_opt(struct filter_util *qu, char *handle, int argc, char **argv, struct nlmsghdr *n)
{
	struct tc_police tp;
	struct tcmsg *t = NLMSG_DATA(n);
	struct rtattr *tail;
	__u32 fh = 0xFFFF8000;
	__u32 order = 0;

	memset(&tp, 0, sizeof(tp));

	if (handle) {
		if (get_u32(&t->tcm_handle, handle, 0)) {
			fprintf(stderr, "Illegal \"handle\"\n");
			return -1;
		}
	}

	if (argc == 0)
		return 0;

	tail = NLMSG_TAIL(n);
	addattr_l(n, 4096, TCA_OPTIONS, NULL, 0);

	while (argc > 0) {
		if (matches(*argv, "to") == 0) {
			__u32 id;
			NEXT_ARG();
			if (rtnl_rtrealm_a2n(&id, *argv)) {
				fprintf(stderr, "Illegal \"to\"\n");
				return -1;
			}
			addattr_l(n, 4096, TCA_ROUTE4_TO, &id, 4);
			fh &= ~0x80FF;
			fh |= id&0xFF;
		} else if (matches(*argv, "from") == 0) {
			__u32 id;
			NEXT_ARG();
			if (rtnl_rtrealm_a2n(&id, *argv)) {
				fprintf(stderr, "Illegal \"from\"\n");
				return -1;
			}
			addattr_l(n, 4096, TCA_ROUTE4_FROM, &id, 4);
			fh &= 0xFFFF;
			fh |= id<<16;
		} else if (matches(*argv, "fromif") == 0) {
			__u32 id;
			NEXT_ARG();
			ll_init_map(&rth);
			if ((id=ll_name_to_index(*argv)) <= 0) {
				fprintf(stderr, "Illegal \"fromif\"\n");
				return -1;
			}
			addattr_l(n, 4096, TCA_ROUTE4_IIF, &id, 4);
			fh &= 0xFFFF;
			fh |= (0x8000|id)<<16;
		} else if (matches(*argv, "classid") == 0 ||
			   strcmp(*argv, "flowid") == 0) {
			unsigned handle;
			NEXT_ARG();
			if (get_tc_classid(&handle, *argv)) {
				fprintf(stderr, "Illegal \"classid\"\n");
				return -1;
			}
			addattr_l(n, 4096, TCA_ROUTE4_CLASSID, &handle, 4);
		} else if (matches(*argv, "police") == 0) {
			NEXT_ARG();
			if (parse_police(&argc, &argv, TCA_ROUTE4_POLICE, n)) {
				fprintf(stderr, "Illegal \"police\"\n");
				return -1;
			}
			continue;
		} else if (matches(*argv, "order") == 0) {
			NEXT_ARG();
			if (get_u32(&order, *argv, 0)) {
				fprintf(stderr, "Illegal \"order\"\n");
				return -1;
			}
		} else if (strcmp(*argv, "help") == 0) {
			explain();
			return -1;
		} else {
			fprintf(stderr, "What is \"%s\"?\n", *argv);
			explain();
			return -1;
		}
		argc--; argv++;
	}
	tail->rta_len = (void *) NLMSG_TAIL(n) - (void *) tail;
	if (order) {
		fh &= ~0x7F00;
		fh |= (order<<8)&0x7F00;
	}
	if (!t->tcm_handle)
		t->tcm_handle = fh;
	return 0;
}

static int route_print_opt(struct filter_util *qu, FILE *f, struct rtattr *opt, __u32 handle)
{
	struct rtattr *tb[TCA_ROUTE4_MAX+1];
	SPRINT_BUF(b1);

	if (opt == NULL)
		return 0;

	parse_rtattr_nested(tb, TCA_ROUTE4_MAX, opt);

	if (handle)
		fprintf(f, "fh 0x%08x ", handle);
	if (handle&0x7F00)
		fprintf(f, "order %d ", (handle>>8)&0x7F);

	if (tb[TCA_ROUTE4_CLASSID]) {
		SPRINT_BUF(b1);
		fprintf(f, "flowid %s ", sprint_tc_classid(*(__u32*)RTA_DATA(tb[TCA_ROUTE4_CLASSID]), b1));
	}
	if (tb[TCA_ROUTE4_TO])
		fprintf(f, "to %s ", rtnl_rtrealm_n2a(*(__u32*)RTA_DATA(tb[TCA_ROUTE4_TO]), b1, sizeof(b1)));
	if (tb[TCA_ROUTE4_FROM])
		fprintf(f, "from %s ", rtnl_rtrealm_n2a(*(__u32*)RTA_DATA(tb[TCA_ROUTE4_FROM]), b1, sizeof(b1)));
	if (tb[TCA_ROUTE4_IIF])
		fprintf(f, "fromif %s", ll_index_to_name(*(int*)RTA_DATA(tb[TCA_ROUTE4_IIF])));
	if (tb[TCA_ROUTE4_POLICE])
		tc_print_police(f, tb[TCA_ROUTE4_POLICE]);
	return 0;
}

struct filter_util route_filter_util = {
	.id = "route",
	.parse_fopt = route_parse_opt,
	.print_fopt = route_print_opt,
};
