/*
 * q_drr.c		DRR.
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:	Patrick McHardy <kaber@trash.net>
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
#include "tc_util.h"

static void explain(void)
{
	fprintf(stderr, "Usage: ... drr\n");
}

static void explain2(void)
{
	fprintf(stderr, "Usage: ... drr quantum SIZE\n");
}


static int drr_parse_opt(struct qdisc_util *qu, int argc, char **argv, struct nlmsghdr *n)
{
	while (argc) {
		if (strcmp(*argv, "help") == 0) {
			explain();
			return -1;
		} else {
			fprintf(stderr, "What is \"%s\"?\n", *argv);
			explain();
			return -1;
		}
	}
	return 0;
}

static int drr_parse_class_opt(struct qdisc_util *qu, int argc, char **argv,
			       struct nlmsghdr *n)
{
	struct rtattr *tail;
	__u32 tmp;

	tail = NLMSG_TAIL(n);
	addattr_l(n, 1024, TCA_OPTIONS, NULL, 0);

	while (argc > 0) {
		if (strcmp(*argv, "quantum") == 0) {
			NEXT_ARG();
			if (get_size(&tmp, *argv)) {
				fprintf(stderr, "Illegal \"quantum\"\n");
				return -1;
			}
			addattr_l(n, 1024, TCA_DRR_QUANTUM, &tmp, sizeof(tmp));
		} else if (strcmp(*argv, "help") == 0) {
			explain2();
			return -1;
		} else {
			fprintf(stderr, "What is \"%s\"?\n", *argv);
			explain2();
			return -1;
		}
		argc--; argv++;
	}

	tail->rta_len = (void *) NLMSG_TAIL(n) - (void *)tail;
	return 0;
}

static int drr_print_opt(struct qdisc_util *qu, FILE *f, struct rtattr *opt)
{
	struct rtattr *tb[TCA_DRR_MAX + 1];
	SPRINT_BUF(b1);

	if (opt == NULL)
		return 0;

	parse_rtattr_nested(tb, TCA_DRR_MAX, opt);

	if (tb[TCA_DRR_QUANTUM])
		fprintf(f, "quantum %s ",
			sprint_size(*(__u32 *)RTA_DATA(tb[TCA_DRR_QUANTUM]), b1));
	return 0;
}

static int drr_print_xstats(struct qdisc_util *qu, FILE *f, struct rtattr *xstats)
{
	struct tc_drr_stats *x;
	SPRINT_BUF(b1);

	if (xstats == NULL)
		return 0;
	if (RTA_PAYLOAD(xstats) < sizeof(*x))
		return -1;
	x = RTA_DATA(xstats);

	fprintf(f, " deficit %s ", sprint_size(x->deficit, b1));
	return 0;
}

struct qdisc_util drr_qdisc_util = {
	.id		= "drr",
	.parse_qopt	= drr_parse_opt,
	.print_qopt	= drr_print_opt,
	.print_xstats	= drr_print_xstats,
	.parse_copt	= drr_parse_class_opt,
	.print_copt	= drr_print_opt,
};
