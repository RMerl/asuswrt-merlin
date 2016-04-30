/*
 * q_qfq.c	QFQ.
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:	Stephen Hemminger <shemminger@vyatta.com>
 *		Fabio Checconi <fabio@gandalf.sssup.it>
 *
 */
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
	fprintf(stderr, "Usage: ... qfq\n");
}

static void explain1(const char *arg)
{
	fprintf(stderr, "Illegal \"%s\"\n", arg);
}

static void explain_class(void)
{
	fprintf(stderr, "Usage: ... qfq weight NUMBER maxpkt BYTES\n");
}

static int qfq_parse_opt(struct qdisc_util *qu, int argc, char **argv,
			 struct nlmsghdr *n)
{
	while (argc > 0) {
		if (matches(*argv, "help") == 0) {
			explain();
			return -1;
		} else {
			fprintf(stderr, "What is \"%s\"?\n", *argv);
			explain();
			return -1;
		}
		argc--; argv++;
	}

	return 0;
}

static int qfq_parse_class_opt(struct qdisc_util *qu, int argc, char **argv,
			       struct nlmsghdr *n)
{
	struct rtattr *tail;
	__u32 tmp;

	tail = NLMSG_TAIL(n);
	addattr_l(n, 4096, TCA_OPTIONS, NULL, 0);

	while (argc > 0) {
		if (matches(*argv, "weight") == 0) {
			NEXT_ARG();
			if (get_u32(&tmp, *argv, 10)) {
				explain1("weight"); return -1;
			}
			addattr32(n, 4096, TCA_QFQ_WEIGHT, tmp);
		} else if (matches(*argv, "maxpkt") == 0) {
			NEXT_ARG();
			if (get_u32(&tmp, *argv, 10)) {
				explain1("maxpkt"); return -1;
			}
			addattr32(n, 4096, TCA_QFQ_LMAX, tmp);
		} else if (strcmp(*argv, "help") == 0) {
			explain_class();
			return -1;
		} else {
			fprintf(stderr, "What is \"%s\"?\n", *argv);
			explain_class();
			return -1;
		}
		argc--; argv++;
	}

	tail->rta_len = (void *)NLMSG_TAIL(n) - (void *)tail;

	return 0;
}

static int qfq_print_opt(struct qdisc_util *qu, FILE *f, struct rtattr *opt)
{
	struct rtattr *tb[TCA_QFQ_MAX + 1];

	if (opt == NULL)
		return 0;

	parse_rtattr_nested(tb, TCA_QFQ_MAX, opt);

	if (tb[TCA_QFQ_WEIGHT]) {
		fprintf(f, "weight %u ",
			rta_getattr_u32(tb[TCA_QFQ_WEIGHT]));
	}

	if (tb[TCA_QFQ_LMAX]) {
		fprintf(f, "maxpkt %u ",
			rta_getattr_u32(tb[TCA_QFQ_LMAX]));
	}

	return 0;
}

struct qdisc_util qfq_qdisc_util = {
	.id		= "qfq",
	.parse_qopt	= qfq_parse_opt,
	.print_qopt	= qfq_print_opt,
	.parse_copt	= qfq_parse_class_opt,
	.print_copt	= qfq_print_opt,
};
