/*
 * q_sfb.c	Stochastic Fair Blue.
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:	Juliusz Chroboczek <jch@pps.jussieu.fr>
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
	fprintf(stderr,
		"Usage: ... sfb [ rehash SECS ] [ db SECS ]\n"
		"	    [ limit PACKETS ] [ max PACKETS ] [ target PACKETS ]\n"
		"	    [ increment FLOAT ] [ decrement FLOAT ]\n"
		"	    [ penalty_rate PPS ] [ penalty_burst PACKETS ]\n");
}

static int get_prob(__u32 *val, const char *arg)
{
	double d;
	char *ptr;

	if (!arg || !*arg)
		return -1;
	d = strtod(arg, &ptr);
	if (!ptr || ptr == arg || d < 0.0 || d > 1.0)
		return -1;
	*val = (__u32)(d * SFB_MAX_PROB + 0.5);
	return 0;
}

static int sfb_parse_opt(struct qdisc_util *qu, int argc, char **argv,
			 struct nlmsghdr *n)
{
	struct tc_sfb_qopt opt;
	struct rtattr *tail;

	memset(&opt, 0, sizeof(opt));
	opt.rehash_interval = 600*1000;
	opt.warmup_time = 60*1000;
	opt.penalty_rate = 10;
	opt.penalty_burst = 20;
	opt.increment = (SFB_MAX_PROB + 1000) / 2000;
	opt.decrement = (SFB_MAX_PROB + 10000) / 20000;

	while (argc > 0) {
	    if (strcmp(*argv, "rehash") == 0) {
			NEXT_ARG();
			if (get_u32(&opt.rehash_interval, *argv, 0)) {
				fprintf(stderr, "Illegal \"rehash\"\n");
				return -1;
			}
		} else if (strcmp(*argv, "db") == 0) {
			NEXT_ARG();
			if (get_u32(&opt.warmup_time, *argv, 0)) {
				fprintf(stderr, "Illegal \"db\"\n");
				return -1;
			}
		} else if (strcmp(*argv, "limit") == 0) {
			NEXT_ARG();
			if (get_u32(&opt.limit, *argv, 0)) {
				fprintf(stderr, "Illegal \"limit\"\n");
				return -1;
			}
		} else if (strcmp(*argv, "max") == 0) {
			NEXT_ARG();
			if (get_u32(&opt.max, *argv, 0)) {
				fprintf(stderr, "Illegal \"max\"\n");
				return -1;
			}
		} else if (strcmp(*argv, "target") == 0) {
			NEXT_ARG();
			if (get_u32(&opt.bin_size, *argv, 0)) {
				fprintf(stderr, "Illegal \"target\"\n");
				return -1;
			}
		} else if (strcmp(*argv, "increment") == 0) {
			NEXT_ARG();
			if (get_prob(&opt.increment, *argv)) {
				fprintf(stderr, "Illegal \"increment\"\n");
				return -1;
			}
		} else if (strcmp(*argv, "decrement") == 0) {
			NEXT_ARG();
			if (get_prob(&opt.decrement, *argv)) {
				fprintf(stderr, "Illegal \"decrement\"\n");
				return -1;
			}
		} else if (strcmp(*argv, "penalty_rate") == 0) {
			NEXT_ARG();
			if (get_u32(&opt.penalty_rate, *argv, 0)) {
				fprintf(stderr, "Illegal \"penalty_rate\"\n");
				return -1;
			}
		} else if (strcmp(*argv, "penalty_burst") == 0) {
			NEXT_ARG();
			if (get_u32(&opt.penalty_burst, *argv, 0)) {
				fprintf(stderr, "Illegal \"penalty_burst\"\n");
				return -1;
			}
		} else {
			fprintf(stderr, "What is \"%s\"?\n", *argv);
			explain();
			return -1;
		}
		argc--; argv++;
	}

	if (opt.max == 0) {
		if (opt.bin_size >= 1)
			opt.max = (opt.bin_size * 5 + 1) / 4;
		else
			opt.max = 25;
	}
	if (opt.bin_size == 0)
		opt.bin_size = (opt.max * 4 + 3) / 5;

	tail = NLMSG_TAIL(n);
	addattr_l(n, 1024, TCA_OPTIONS, NULL, 0);
	addattr_l(n, 1024, TCA_SFB_PARMS, &opt, sizeof(opt));
	tail->rta_len = (void *) NLMSG_TAIL(n) - (void *) tail;
	return 0;
}

static int sfb_print_opt(struct qdisc_util *qu, FILE *f, struct rtattr *opt)
{
	struct rtattr *tb[__TCA_SFB_MAX];
	struct tc_sfb_qopt *qopt;

	if (opt == NULL)
		return 0;

	parse_rtattr_nested(tb, TCA_SFB_MAX, opt);
	if (tb[TCA_SFB_PARMS] == NULL)
		return -1;
	qopt = RTA_DATA(tb[TCA_SFB_PARMS]);
	if (RTA_PAYLOAD(tb[TCA_SFB_PARMS]) < sizeof(*qopt))
		return -1;

	fprintf(f,
		"limit %d max %d target %d\n"
		"  increment %.5f decrement %.5f penalty rate %d burst %d "
		"(%ums %ums)",
		qopt->limit, qopt->max, qopt->bin_size,
		(double)qopt->increment / SFB_MAX_PROB,
		(double)qopt->decrement / SFB_MAX_PROB,
		qopt->penalty_rate, qopt->penalty_burst,
		qopt->rehash_interval, qopt->warmup_time);

	return 0;
}

static int sfb_print_xstats(struct qdisc_util *qu, FILE *f,
			    struct rtattr *xstats)
{
    struct tc_sfb_xstats *st;

    if (xstats == NULL)
	    return 0;

    if (RTA_PAYLOAD(xstats) < sizeof(*st))
	    return -1;

    st = RTA_DATA(xstats);
    fprintf(f,
	    "  earlydrop %u penaltydrop %u bucketdrop %u queuedrop %u childdrop %u marked %u\n"
	    "  maxqlen %u maxprob %.5f avgprob %.5f ",
	    st->earlydrop, st->penaltydrop, st->bucketdrop, st->queuedrop, st->childdrop,
	    st->marked,
	    st->maxqlen, (double)st->maxprob / SFB_MAX_PROB,
		(double)st->avgprob / SFB_MAX_PROB);

    return 0;
}

struct qdisc_util sfb_qdisc_util = {
	.id		= "sfb",
	.parse_qopt	= sfb_parse_opt,
	.print_qopt	= sfb_print_opt,
	.print_xstats	= sfb_print_xstats,
};
