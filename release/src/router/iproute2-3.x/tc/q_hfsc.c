/*
 * q_hfsc.c	HFSC.
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:	Patrick McHardy, <kaber@trash.net>
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
#include <math.h>

#include "utils.h"
#include "tc_util.h"

static int hfsc_get_sc(int *, char ***, struct tc_service_curve *);


static void
explain_qdisc(void)
{
	fprintf(stderr,
		"Usage: ... hfsc [ default CLASSID ]\n"
		"\n"
		" default: default class for unclassified packets\n"
	);
}

static void
explain_class(void)
{
	fprintf(stderr,
		"Usage: ... hfsc [ [ rt SC ] [ ls SC ] | [ sc SC ] ] [ ul SC ]\n"
		"\n"
		"SC := [ [ m1 BPS ] [ d SEC ] m2 BPS\n"
		"\n"
		" m1 : slope of first segment\n"
		" d  : x-coordinate of intersection\n"
		" m2 : slope of second segment\n"
		"\n"
		"Alternative format:\n"
		"\n"
		"SC := [ [ umax BYTE ] dmax SEC ] rate BPS\n"
		"\n"
		" umax : maximum unit of work\n"
		" dmax : maximum delay\n"
		" rate : rate\n"
		"\n"
	);
}

static void
explain1(char *arg)
{
	fprintf(stderr, "HFSC: Illegal \"%s\"\n", arg);
}

static int
hfsc_parse_opt(struct qdisc_util *qu, int argc, char **argv, struct nlmsghdr *n)
{
	struct tc_hfsc_qopt qopt;

	memset(&qopt, 0, sizeof(qopt));

	while (argc > 0) {
		if (matches(*argv, "default") == 0) {
			NEXT_ARG();
			if (qopt.defcls != 0) {
				fprintf(stderr, "HFSC: Double \"default\"\n");
				return -1;
			}
			if (get_u16(&qopt.defcls, *argv, 16) < 0) {
				explain1("default");
				return -1;
			}
		} else if (matches(*argv, "help") == 0) {
			explain_qdisc();
			return -1;
		} else {
			fprintf(stderr, "HFSC: What is \"%s\" ?\n", *argv);
			explain_qdisc();
			return -1;
		}
		argc--, argv++;
	}

	addattr_l(n, 1024, TCA_OPTIONS, &qopt, sizeof(qopt));
	return 0;
}

static int
hfsc_print_opt(struct qdisc_util *qu, FILE *f, struct rtattr *opt)
{
	struct tc_hfsc_qopt *qopt;

	if (opt == NULL)
		return 0;
	if (RTA_PAYLOAD(opt) < sizeof(*qopt))
		return -1;
	qopt = RTA_DATA(opt);

	if (qopt->defcls != 0)
		fprintf(f, "default %x ", qopt->defcls);

	return 0;
}

static int
hfsc_print_xstats(struct qdisc_util *qu, FILE *f, struct rtattr *xstats)
{
	struct tc_hfsc_stats *st;

	if (xstats == NULL)
		return 0;
	if (RTA_PAYLOAD(xstats) < sizeof(*st))
		return -1;
	st = RTA_DATA(xstats);

	fprintf(f, " period %u ", st->period);
	if (st->work != 0)
		fprintf(f, "work %llu bytes ", (unsigned long long) st->work);
	if (st->rtwork != 0)
		fprintf(f, "rtwork %llu bytes ", (unsigned long long) st->rtwork);
	fprintf(f, "level %u ", st->level);
	fprintf(f, "\n");

	return 0;
}

static int
hfsc_parse_class_opt(struct qdisc_util *qu, int argc, char **argv,
                     struct nlmsghdr *n)
{
	struct tc_service_curve rsc, fsc, usc;
	int rsc_ok, fsc_ok, usc_ok;
	struct rtattr *tail;

	memset(&rsc, 0, sizeof(rsc));
	memset(&fsc, 0, sizeof(fsc));
	memset(&usc, 0, sizeof(usc));
	rsc_ok = fsc_ok = usc_ok = 0;

	while (argc > 0) {
		if (matches(*argv, "rt") == 0) {
			NEXT_ARG();
			if (hfsc_get_sc(&argc, &argv, &rsc) < 0) {
				explain1("rt");
				return -1;
			}
			rsc_ok = 1;
		} else if (matches(*argv, "ls") == 0) {
			NEXT_ARG();
			if (hfsc_get_sc(&argc, &argv, &fsc) < 0) {
				explain1("ls");
				return -1;
			}
			fsc_ok = 1;
		} else if (matches(*argv, "sc") == 0) {
			NEXT_ARG();
			if (hfsc_get_sc(&argc, &argv, &rsc) < 0) {
				explain1("sc");
				return -1;
			}
			memcpy(&fsc, &rsc, sizeof(fsc));
			rsc_ok = 1;
			fsc_ok = 1;
		} else if (matches(*argv, "ul") == 0) {
			NEXT_ARG();
			if (hfsc_get_sc(&argc, &argv, &usc) < 0) {
				explain1("ul");
				return -1;
			}
			usc_ok = 1;
		} else if (matches(*argv, "help") == 0) {
			explain_class();
			return -1;
		} else {
			fprintf(stderr, "HFSC: What is \"%s\" ?\n", *argv);
			explain_class();
			return -1;
		}
		argc--, argv++;
	}

	if (!(rsc_ok || fsc_ok || usc_ok)) {
		fprintf(stderr, "HFSC: no parameters given\n");
		explain_class();
		return -1;
	}
	if (usc_ok && !fsc_ok) {
		fprintf(stderr, "HFSC: Upper-limit Service Curve without "
		                "Link-Share Service Curve\n");
		explain_class();
		return -1;
	}

	tail = NLMSG_TAIL(n);

	addattr_l(n, 1024, TCA_OPTIONS, NULL, 0);
	if (rsc_ok)
		addattr_l(n, 1024, TCA_HFSC_RSC, &rsc, sizeof(rsc));
	if (fsc_ok)
		addattr_l(n, 1024, TCA_HFSC_FSC, &fsc, sizeof(fsc));
	if (usc_ok)
		addattr_l(n, 1024, TCA_HFSC_USC, &usc, sizeof(usc));

	tail->rta_len = (void *) NLMSG_TAIL(n) - (void *) tail;
	return 0;
}

static void
hfsc_print_sc(FILE *f, char *name, struct tc_service_curve *sc)
{
	SPRINT_BUF(b1);

	fprintf(f, "%s ", name);
	fprintf(f, "m1 %s ", sprint_rate(sc->m1, b1));
	fprintf(f, "d %s ", sprint_time(tc_core_ktime2time(sc->d), b1));
	fprintf(f, "m2 %s ", sprint_rate(sc->m2, b1));
}

static int
hfsc_print_class_opt(struct qdisc_util *qu, FILE *f, struct rtattr *opt)
{
	struct rtattr *tb[TCA_HFSC_MAX+1];
	struct tc_service_curve *rsc = NULL, *fsc = NULL, *usc = NULL;

	if (opt == NULL)
		return 0;

	parse_rtattr_nested(tb, TCA_HFSC_MAX, opt);

	if (tb[TCA_HFSC_RSC]) {
		if (RTA_PAYLOAD(tb[TCA_HFSC_RSC]) < sizeof(*rsc))
			fprintf(stderr, "HFSC: truncated realtime option\n");
		else
			rsc = RTA_DATA(tb[TCA_HFSC_RSC]);
	}
	if (tb[TCA_HFSC_FSC]) {
		if (RTA_PAYLOAD(tb[TCA_HFSC_FSC]) < sizeof(*fsc))
			fprintf(stderr, "HFSC: truncated linkshare option\n");
		else
			fsc = RTA_DATA(tb[TCA_HFSC_FSC]);
	}
	if (tb[TCA_HFSC_USC]) {
		if (RTA_PAYLOAD(tb[TCA_HFSC_USC]) < sizeof(*usc))
			fprintf(stderr, "HFSC: truncated upperlimit option\n");
		else
			usc = RTA_DATA(tb[TCA_HFSC_USC]);
	}


	if (rsc != NULL && fsc != NULL &&
	    memcmp(rsc, fsc, sizeof(*rsc)) == 0)
		hfsc_print_sc(f, "sc", rsc);
	else {
		if (rsc != NULL)
			hfsc_print_sc(f, "rt", rsc);
		if (fsc != NULL)
			hfsc_print_sc(f, "ls", fsc);
	}
	if (usc != NULL)
		hfsc_print_sc(f, "ul", usc);

	return 0;
}

struct qdisc_util hfsc_qdisc_util = {
	.id		= "hfsc",
	.parse_qopt	= hfsc_parse_opt,
	.print_qopt	= hfsc_print_opt,
	.print_xstats	= hfsc_print_xstats,
	.parse_copt	= hfsc_parse_class_opt,
	.print_copt	= hfsc_print_class_opt,
};

static int
hfsc_get_sc1(int *argcp, char ***argvp, struct tc_service_curve *sc)
{
	char **argv = *argvp;
	int argc = *argcp;
	unsigned int m1 = 0, d = 0, m2 = 0;

	if (matches(*argv, "m1") == 0) {
		NEXT_ARG();
		if (get_rate(&m1, *argv) < 0) {
			explain1("m1");
			return -1;
		}
		NEXT_ARG();
	}

	if (matches(*argv, "d") == 0) {
		NEXT_ARG();
		if (get_time(&d, *argv) < 0) {
			explain1("d");
			return -1;
		}
		NEXT_ARG();
	}

	if (matches(*argv, "m2") == 0) {
		NEXT_ARG();
		if (get_rate(&m2, *argv) < 0) {
			explain1("m2");
			return -1;
		}
	} else
		return -1;

	sc->m1 = m1;
	sc->d  = tc_core_time2ktime(d);
	sc->m2 = m2;

	*argvp = argv;
	*argcp = argc;
	return 0;
}

static int
hfsc_get_sc2(int *argcp, char ***argvp, struct tc_service_curve *sc)
{
	char **argv = *argvp;
	int argc = *argcp;
	unsigned int umax = 0, dmax = 0, rate = 0;

	if (matches(*argv, "umax") == 0) {
		NEXT_ARG();
		if (get_size(&umax, *argv) < 0) {
			explain1("umax");
			return -1;
		}
		NEXT_ARG();
	}

	if (matches(*argv, "dmax") == 0) {
		NEXT_ARG();
		if (get_time(&dmax, *argv) < 0) {
			explain1("dmax");
			return -1;
		}
		NEXT_ARG();
	}

	if (matches(*argv, "rate") == 0) {
		NEXT_ARG();
		if (get_rate(&rate, *argv) < 0) {
			explain1("rate");
			return -1;
		}
	} else
		return -1;

	if (umax != 0 && dmax == 0) {
		fprintf(stderr, "HFSC: umax given but dmax is zero.\n");
		return -1;
	}

	if (dmax != 0 && ceil(1.0 * umax * TIME_UNITS_PER_SEC / dmax) > rate) {
		/*
		 * concave curve, slope of first segment is umax/dmax,
		 * intersection is at dmax
		 */
		sc->m1 = ceil(1.0 * umax * TIME_UNITS_PER_SEC / dmax); /* in bps */
		sc->d  = tc_core_time2ktime(dmax);
		sc->m2 = rate;
	} else {
		/*
		 * convex curve, slope of first segment is 0, intersection
		 * is at dmax - umax / rate
		 */
		sc->m1 = 0;
		sc->d  = tc_core_time2ktime(ceil(dmax - umax * TIME_UNITS_PER_SEC / rate));
		sc->m2 = rate;
	}

	*argvp = argv;
	*argcp = argc;
	return 0;
}

static int
hfsc_get_sc(int *argcp, char ***argvp, struct tc_service_curve *sc)
{
	if (hfsc_get_sc1(argcp, argvp, sc) < 0 &&
	    hfsc_get_sc2(argcp, argvp, sc) < 0)
		return -1;

	if (sc->m1 == 0 && sc->m2 == 0) {
		fprintf(stderr, "HFSC: Service Curve has two zero slopes\n");
		return -1;
	}

	return 0;
}
