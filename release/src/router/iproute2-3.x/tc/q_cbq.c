/*
 * q_cbq.c		CBQ.
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
#include "tc_util.h"
#include "tc_cbq.h"

static void explain_class(void)
{
	fprintf(stderr, "Usage: ... cbq bandwidth BPS rate BPS maxburst PKTS [ avpkt BYTES ]\n");
	fprintf(stderr, "               [ minburst PKTS ] [ bounded ] [ isolated ]\n");
	fprintf(stderr, "               [ allot BYTES ] [ mpu BYTES ] [ weight RATE ]\n");
	fprintf(stderr, "               [ prio NUMBER ] [ cell BYTES ] [ ewma LOG ]\n");
	fprintf(stderr, "               [ estimator INTERVAL TIME_CONSTANT ]\n");
	fprintf(stderr, "               [ split CLASSID ] [ defmap MASK/CHANGE ]\n");
	fprintf(stderr, "               [ overhead BYTES ] [ linklayer TYPE ]\n");
}

static void explain(void)
{
	fprintf(stderr, "Usage: ... cbq bandwidth BPS avpkt BYTES [ mpu BYTES ]\n");
	fprintf(stderr, "               [ cell BYTES ] [ ewma LOG ]\n");
}

static void explain1(char *arg)
{
	fprintf(stderr, "Illegal \"%s\"\n", arg);
}


static int cbq_parse_opt(struct qdisc_util *qu, int argc, char **argv, struct nlmsghdr *n)
{
	struct tc_ratespec r;
	struct tc_cbq_lssopt lss;
	__u32 rtab[256];
	unsigned mpu=0, avpkt=0, allot=0;
	unsigned short overhead=0;
	unsigned int linklayer = LINKLAYER_ETHERNET; /* Assume ethernet */
	int cell_log=-1;
	int ewma_log=-1;
	struct rtattr *tail;

	memset(&lss, 0, sizeof(lss));
	memset(&r, 0, sizeof(r));

	while (argc > 0) {
		if (matches(*argv, "bandwidth") == 0 ||
		    matches(*argv, "rate") == 0) {
			NEXT_ARG();
			if (get_rate(&r.rate, *argv)) {
				explain1("bandwidth");
				return -1;
			}
		} else if (matches(*argv, "ewma") == 0) {
			NEXT_ARG();
			if (get_integer(&ewma_log, *argv, 0)) {
				explain1("ewma");
				return -1;
			}
			if (ewma_log > 31) {
				fprintf(stderr, "ewma_log must be < 32\n");
				return -1;
			}
		} else if (matches(*argv, "cell") == 0) {
			unsigned cell;
			int i;
			NEXT_ARG();
			if (get_size(&cell, *argv)) {
				explain1("cell");
				return -1;
			}
			for (i=0; i<32; i++)
				if ((1<<i) == cell)
					break;
			if (i>=32) {
				fprintf(stderr, "cell must be 2^n\n");
				return -1;
			}
			cell_log = i;
		} else if (matches(*argv, "avpkt") == 0) {
			NEXT_ARG();
			if (get_size(&avpkt, *argv)) {
				explain1("avpkt");
				return -1;
			}
		} else if (matches(*argv, "mpu") == 0) {
			NEXT_ARG();
			if (get_size(&mpu, *argv)) {
				explain1("mpu");
				return -1;
			}
		} else if (matches(*argv, "allot") == 0) {
			NEXT_ARG();
			/* Accept and ignore "allot" for backward compatibility */
			if (get_size(&allot, *argv)) {
				explain1("allot");
				return -1;
			}
		} else if (matches(*argv, "overhead") == 0) {
			NEXT_ARG();
			if (get_u16(&overhead, *argv, 10)) {
				explain1("overhead"); return -1;
			}
		} else if (matches(*argv, "linklayer") == 0) {
			NEXT_ARG();
			if (get_linklayer(&linklayer, *argv)) {
				explain1("linklayer"); return -1;
			}
		} else if (matches(*argv, "help") == 0) {
			explain();
			return -1;
		} else {
			fprintf(stderr, "What is \"%s\"?\n", *argv);
			explain();
			return -1;
		}
		argc--; argv++;
	}

	/* OK. All options are parsed. */

	if (r.rate == 0) {
		fprintf(stderr, "CBQ: bandwidth is required parameter.\n");
		return -1;
	}
	if (avpkt == 0) {
		fprintf(stderr, "CBQ: \"avpkt\" is required.\n");
		return -1;
	}
	if (allot < (avpkt*3)/2)
		allot = (avpkt*3)/2;

	r.mpu = mpu;
	r.overhead = overhead;
	if (tc_calc_rtable(&r, rtab, cell_log, allot, linklayer) < 0) {
		fprintf(stderr, "CBQ: failed to calculate rate table.\n");
		return -1;
	}

	if (ewma_log < 0)
		ewma_log = TC_CBQ_DEF_EWMA;
	lss.ewma_log = ewma_log;
	lss.maxidle = tc_calc_xmittime(r.rate, avpkt);
	lss.change = TCF_CBQ_LSS_MAXIDLE|TCF_CBQ_LSS_EWMA|TCF_CBQ_LSS_AVPKT;
	lss.avpkt = avpkt;

	tail = NLMSG_TAIL(n);
	addattr_l(n, 1024, TCA_OPTIONS, NULL, 0);
	addattr_l(n, 1024, TCA_CBQ_RATE, &r, sizeof(r));
	addattr_l(n, 1024, TCA_CBQ_LSSOPT, &lss, sizeof(lss));
	addattr_l(n, 3024, TCA_CBQ_RTAB, rtab, 1024);
	if (show_raw) {
		int i;
		for (i=0; i<256; i++)
			printf("%u ", rtab[i]);
		printf("\n");
	}
	tail->rta_len = (void *) NLMSG_TAIL(n) - (void *) tail;
	return 0;
}

static int cbq_parse_class_opt(struct qdisc_util *qu, int argc, char **argv, struct nlmsghdr *n)
{
	int wrr_ok=0, fopt_ok=0;
	struct tc_ratespec r;
	struct tc_cbq_lssopt lss;
	struct tc_cbq_wrropt wrr;
	struct tc_cbq_fopt fopt;
	struct tc_cbq_ovl ovl;
	__u32 rtab[256];
	unsigned mpu=0;
	int cell_log=-1;
	int ewma_log=-1;
	unsigned bndw = 0;
	unsigned minburst=0, maxburst=0;
	unsigned short overhead=0;
	unsigned int linklayer = LINKLAYER_ETHERNET; /* Assume ethernet */
	struct rtattr *tail;

	memset(&r, 0, sizeof(r));
	memset(&lss, 0, sizeof(lss));
	memset(&wrr, 0, sizeof(wrr));
	memset(&fopt, 0, sizeof(fopt));
	memset(&ovl, 0, sizeof(ovl));

	while (argc > 0) {
		if (matches(*argv, "rate") == 0) {
			NEXT_ARG();
			if (get_rate(&r.rate, *argv)) {
				explain1("rate");
				return -1;
			}
		} else if (matches(*argv, "bandwidth") == 0) {
			NEXT_ARG();
			if (get_rate(&bndw, *argv)) {
				explain1("bandwidth");
				return -1;
			}
		} else if (matches(*argv, "minidle") == 0) {
			NEXT_ARG();
			if (get_u32(&lss.minidle, *argv, 0)) {
				explain1("minidle");
				return -1;
			}
			lss.change |= TCF_CBQ_LSS_MINIDLE;
		} else if (matches(*argv, "minburst") == 0) {
			NEXT_ARG();
			if (get_u32(&minburst, *argv, 0)) {
				explain1("minburst");
				return -1;
			}
			lss.change |= TCF_CBQ_LSS_OFFTIME;
		} else if (matches(*argv, "maxburst") == 0) {
			NEXT_ARG();
			if (get_u32(&maxburst, *argv, 0)) {
				explain1("maxburst");
				return -1;
			}
			lss.change |= TCF_CBQ_LSS_MAXIDLE;
		} else if (matches(*argv, "bounded") == 0) {
			lss.flags |= TCF_CBQ_LSS_BOUNDED;
			lss.change |= TCF_CBQ_LSS_FLAGS;
		} else if (matches(*argv, "borrow") == 0) {
			lss.flags &= ~TCF_CBQ_LSS_BOUNDED;
			lss.change |= TCF_CBQ_LSS_FLAGS;
		} else if (matches(*argv, "isolated") == 0) {
			lss.flags |= TCF_CBQ_LSS_ISOLATED;
			lss.change |= TCF_CBQ_LSS_FLAGS;
		} else if (matches(*argv, "sharing") == 0) {
			lss.flags &= ~TCF_CBQ_LSS_ISOLATED;
			lss.change |= TCF_CBQ_LSS_FLAGS;
		} else if (matches(*argv, "ewma") == 0) {
			NEXT_ARG();
			if (get_integer(&ewma_log, *argv, 0)) {
				explain1("ewma");
				return -1;
			}
			if (ewma_log > 31) {
				fprintf(stderr, "ewma_log must be < 32\n");
				return -1;
			}
			lss.change |= TCF_CBQ_LSS_EWMA;
		} else if (matches(*argv, "cell") == 0) {
			unsigned cell;
			int i;
			NEXT_ARG();
			if (get_size(&cell, *argv)) {
				explain1("cell");
				return -1;
			}
			for (i=0; i<32; i++)
				if ((1<<i) == cell)
					break;
			if (i>=32) {
				fprintf(stderr, "cell must be 2^n\n");
				return -1;
			}
			cell_log = i;
		} else if (matches(*argv, "prio") == 0) {
			unsigned prio;
			NEXT_ARG();
			if (get_u32(&prio, *argv, 0)) {
				explain1("prio");
				return -1;
			}
			if (prio > TC_CBQ_MAXPRIO) {
				fprintf(stderr, "\"prio\" must be number in the range 1...%d\n", TC_CBQ_MAXPRIO);
				return -1;
			}
			wrr.priority = prio;
			wrr_ok++;
		} else if (matches(*argv, "allot") == 0) {
			NEXT_ARG();
			if (get_size(&wrr.allot, *argv)) {
				explain1("allot");
				return -1;
			}
		} else if (matches(*argv, "avpkt") == 0) {
			NEXT_ARG();
			if (get_size(&lss.avpkt, *argv)) {
				explain1("avpkt");
				return -1;
			}
			lss.change |= TCF_CBQ_LSS_AVPKT;
		} else if (matches(*argv, "mpu") == 0) {
			NEXT_ARG();
			if (get_size(&mpu, *argv)) {
				explain1("mpu");
				return -1;
			}
		} else if (matches(*argv, "weight") == 0) {
			NEXT_ARG();
			if (get_size(&wrr.weight, *argv)) {
				explain1("weight");
				return -1;
			}
			wrr_ok++;
		} else if (matches(*argv, "split") == 0) {
			NEXT_ARG();
			if (get_tc_classid(&fopt.split, *argv)) {
				fprintf(stderr, "Invalid split node ID.\n");
				return -1;
			}
			fopt_ok++;
		} else if (matches(*argv, "defmap") == 0) {
			int err;
			NEXT_ARG();
			err = sscanf(*argv, "%08x/%08x", &fopt.defmap, &fopt.defchange);
			if (err < 1) {
				fprintf(stderr, "Invalid defmap, should be MASK32[/MASK]\n");
				return -1;
			}
			if (err == 1)
				fopt.defchange = ~0;
			fopt_ok++;
		} else if (matches(*argv, "overhead") == 0) {
			NEXT_ARG();
			if (get_u16(&overhead, *argv, 10)) {
				explain1("overhead"); return -1;
			}
		} else if (matches(*argv, "linklayer") == 0) {
			NEXT_ARG();
			if (get_linklayer(&linklayer, *argv)) {
				explain1("linklayer"); return -1;
			}
		} else if (matches(*argv, "help") == 0) {
			explain_class();
			return -1;
		} else {
			fprintf(stderr, "What is \"%s\"?\n", *argv);
			explain_class();
			return -1;
		}
		argc--; argv++;
	}

	/* OK. All options are parsed. */

	/* 1. Prepare link sharing scheduler parameters */
	if (r.rate) {
		unsigned pktsize = wrr.allot;
		if (wrr.allot < (lss.avpkt*3)/2)
			wrr.allot = (lss.avpkt*3)/2;
		r.mpu = mpu;
		r.overhead = overhead;
		if (tc_calc_rtable(&r, rtab, cell_log, pktsize, linklayer) < 0) {
			fprintf(stderr, "CBQ: failed to calculate rate table.\n");
			return -1;
		}
	}
	if (ewma_log < 0)
		ewma_log = TC_CBQ_DEF_EWMA;
	lss.ewma_log = ewma_log;
	if (lss.change&(TCF_CBQ_LSS_OFFTIME|TCF_CBQ_LSS_MAXIDLE)) {
		if (lss.avpkt == 0) {
			fprintf(stderr, "CBQ: avpkt is required for max/minburst.\n");
			return -1;
		}
		if (bndw==0 || r.rate == 0) {
			fprintf(stderr, "CBQ: bandwidth&rate are required for max/minburst.\n");
			return -1;
		}
	}
	if (wrr.priority == 0 && (n->nlmsg_flags&NLM_F_EXCL)) {
		wrr_ok = 1;
		wrr.priority = TC_CBQ_MAXPRIO;
		if (wrr.allot == 0)
			wrr.allot = (lss.avpkt*3)/2;
	}
	if (wrr_ok) {
		if (wrr.weight == 0)
			wrr.weight = (wrr.priority == TC_CBQ_MAXPRIO) ? 1 : r.rate;
		if (wrr.allot == 0) {
			fprintf(stderr, "CBQ: \"allot\" is required to set WRR parameters.\n");
			return -1;
		}
	}
	if (lss.change&TCF_CBQ_LSS_MAXIDLE) {
		lss.maxidle = tc_cbq_calc_maxidle(bndw, r.rate, lss.avpkt, ewma_log, maxburst);
		lss.change |= TCF_CBQ_LSS_MAXIDLE;
		lss.change |= TCF_CBQ_LSS_EWMA|TCF_CBQ_LSS_AVPKT;
	}
	if (lss.change&TCF_CBQ_LSS_OFFTIME) {
		lss.offtime = tc_cbq_calc_offtime(bndw, r.rate, lss.avpkt, ewma_log, minburst);
		lss.change |= TCF_CBQ_LSS_OFFTIME;
		lss.change |= TCF_CBQ_LSS_EWMA|TCF_CBQ_LSS_AVPKT;
	}
	if (lss.change&TCF_CBQ_LSS_MINIDLE) {
		lss.minidle <<= lss.ewma_log;
		lss.change |= TCF_CBQ_LSS_EWMA;
	}

	tail = NLMSG_TAIL(n);
	addattr_l(n, 1024, TCA_OPTIONS, NULL, 0);
	if (lss.change) {
		lss.change |= TCF_CBQ_LSS_FLAGS;
		addattr_l(n, 1024, TCA_CBQ_LSSOPT, &lss, sizeof(lss));
	}
	if (wrr_ok)
		addattr_l(n, 1024, TCA_CBQ_WRROPT, &wrr, sizeof(wrr));
	if (fopt_ok)
		addattr_l(n, 1024, TCA_CBQ_FOPT, &fopt, sizeof(fopt));
	if (r.rate) {
		addattr_l(n, 1024, TCA_CBQ_RATE, &r, sizeof(r));
		addattr_l(n, 3024, TCA_CBQ_RTAB, rtab, 1024);
		if (show_raw) {
			int i;
			for (i=0; i<256; i++)
				printf("%u ", rtab[i]);
			printf("\n");
		}
	}
	tail->rta_len = (void *) NLMSG_TAIL(n) - (void *) tail;
	return 0;
}


static int cbq_print_opt(struct qdisc_util *qu, FILE *f, struct rtattr *opt)
{
	struct rtattr *tb[TCA_CBQ_MAX+1];
	struct tc_ratespec *r = NULL;
	struct tc_cbq_lssopt *lss = NULL;
	struct tc_cbq_wrropt *wrr = NULL;
	struct tc_cbq_fopt *fopt = NULL;
	struct tc_cbq_ovl *ovl = NULL;
	SPRINT_BUF(b1);

	if (opt == NULL)
		return 0;

	parse_rtattr_nested(tb, TCA_CBQ_MAX, opt);

	if (tb[TCA_CBQ_RATE]) {
		if (RTA_PAYLOAD(tb[TCA_CBQ_RATE]) < sizeof(*r))
			fprintf(stderr, "CBQ: too short rate opt\n");
		else
			r = RTA_DATA(tb[TCA_CBQ_RATE]);
	}
	if (tb[TCA_CBQ_LSSOPT]) {
		if (RTA_PAYLOAD(tb[TCA_CBQ_LSSOPT]) < sizeof(*lss))
			fprintf(stderr, "CBQ: too short lss opt\n");
		else
			lss = RTA_DATA(tb[TCA_CBQ_LSSOPT]);
	}
	if (tb[TCA_CBQ_WRROPT]) {
		if (RTA_PAYLOAD(tb[TCA_CBQ_WRROPT]) < sizeof(*wrr))
			fprintf(stderr, "CBQ: too short wrr opt\n");
		else
			wrr = RTA_DATA(tb[TCA_CBQ_WRROPT]);
	}
	if (tb[TCA_CBQ_FOPT]) {
		if (RTA_PAYLOAD(tb[TCA_CBQ_FOPT]) < sizeof(*fopt))
			fprintf(stderr, "CBQ: too short fopt\n");
		else
			fopt = RTA_DATA(tb[TCA_CBQ_FOPT]);
	}
	if (tb[TCA_CBQ_OVL_STRATEGY]) {
		if (RTA_PAYLOAD(tb[TCA_CBQ_OVL_STRATEGY]) < sizeof(*ovl))
			fprintf(stderr, "CBQ: too short overlimit strategy %u/%u\n",
				(unsigned) RTA_PAYLOAD(tb[TCA_CBQ_OVL_STRATEGY]),
				(unsigned) sizeof(*ovl));
		else
			ovl = RTA_DATA(tb[TCA_CBQ_OVL_STRATEGY]);
	}

	if (r) {
		char buf[64];
		print_rate(buf, sizeof(buf), r->rate);
		fprintf(f, "rate %s ", buf);
		if (show_details) {
			fprintf(f, "cell %ub ", 1<<r->cell_log);
			if (r->mpu)
				fprintf(f, "mpu %ub ", r->mpu);
			if (r->overhead)
				fprintf(f, "overhead %ub ", r->overhead);
		}
	}
	if (lss && lss->flags) {
		int comma=0;
		fprintf(f, "(");
		if (lss->flags&TCF_CBQ_LSS_BOUNDED) {
			fprintf(f, "bounded");
			comma=1;
		}
		if (lss->flags&TCF_CBQ_LSS_ISOLATED) {
			if (comma)
				fprintf(f, ",");
			fprintf(f, "isolated");
		}
		fprintf(f, ") ");
	}
	if (wrr) {
		if (wrr->priority != TC_CBQ_MAXPRIO)
			fprintf(f, "prio %u", wrr->priority);
		else
			fprintf(f, "prio no-transmit");
		if (show_details) {
			char buf[64];
			fprintf(f, "/%u ", wrr->cpriority);
			if (wrr->weight != 1) {
				print_rate(buf, sizeof(buf), wrr->weight);
				fprintf(f, "weight %s ", buf);
			}
			if (wrr->allot)
				fprintf(f, "allot %ub ", wrr->allot);
		}
	}
	if (lss && show_details) {
		fprintf(f, "\nlevel %u ewma %u avpkt %ub ", lss->level, lss->ewma_log, lss->avpkt);
		if (lss->maxidle) {
			fprintf(f, "maxidle %s ", sprint_ticks(lss->maxidle>>lss->ewma_log, b1));
			if (show_raw)
				fprintf(f, "[%08x] ", lss->maxidle);
		}
		if (lss->minidle!=0x7fffffff) {
			fprintf(f, "minidle %s ", sprint_ticks(lss->minidle>>lss->ewma_log, b1));
			if (show_raw)
				fprintf(f, "[%08x] ", lss->minidle);
		}
		if (lss->offtime) {
			fprintf(f, "offtime %s ", sprint_ticks(lss->offtime, b1));
			if (show_raw)
				fprintf(f, "[%08x] ", lss->offtime);
		}
	}
	if (fopt && show_details) {
		char buf[64];
		print_tc_classid(buf, sizeof(buf), fopt->split);
		fprintf(f, "\nsplit %s ", buf);
		if (fopt->defmap) {
			fprintf(f, "defmap %08x", fopt->defmap);
		}
	}
	return 0;
}

static int cbq_print_xstats(struct qdisc_util *qu, FILE *f, struct rtattr *xstats)
{
	struct tc_cbq_xstats *st;

	if (xstats == NULL)
		return 0;

	if (RTA_PAYLOAD(xstats) < sizeof(*st))
		return -1;

	st = RTA_DATA(xstats);
	fprintf(f, "  borrowed %u overactions %u avgidle %g undertime %g", st->borrows,
		st->overactions, (double)st->avgidle, (double)st->undertime);
	return 0;
}

struct qdisc_util cbq_qdisc_util = {
	.id		= "cbq",
	.parse_qopt	= cbq_parse_opt,
	.print_qopt	= cbq_print_opt,
	.print_xstats	= cbq_print_xstats,
	.parse_copt	= cbq_parse_class_opt,
	.print_copt	= cbq_print_opt,
};

