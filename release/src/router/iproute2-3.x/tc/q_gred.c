/*
 * q_gred.c		GRED.
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:    J Hadi Salim(hadi@nortelnetworks.com)
 *             code ruthlessly ripped from
 *	       Alexey Kuznetsov, <kuznet@ms2.inr.ac.ru>
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

#include "tc_red.h"


#if 0
#define DPRINTF(format,args...) fprintf(stderr,format,##args)
#else
#define DPRINTF(format,args...)
#endif

static void explain(void)
{
	fprintf(stderr, "Usage: ... gred DP drop-probability limit BYTES "
	    "min BYTES max BYTES\n");
	fprintf(stderr, "    avpkt BYTES burst PACKETS probability PROBABILITY "
	    "bandwidth KBPS\n");
	fprintf(stderr, "    [prio value]\n");
	fprintf(stderr," OR ...\n");
	fprintf(stderr," gred setup DPs <num of DPs> default <default DP> "
	    "[grio]\n");
}

static int init_gred(struct qdisc_util *qu, int argc, char **argv, 
		     struct nlmsghdr *n)
{

	struct rtattr *tail;
	struct tc_gred_sopt opt = { 0 };
	int dps = 0;
	int def_dp = -1;

	while (argc > 0) {
		DPRINTF(stderr,"init_gred: invoked with %s\n",*argv);
		if (strcmp(*argv, "DPs") == 0) {
			NEXT_ARG();
			DPRINTF(stderr,"init_gred: next_arg with %s\n",*argv);
			dps = strtol(*argv, (char **)NULL, 10);
			if (dps < 0 || dps >MAX_DPs) {
				fprintf(stderr, "DPs =%d\n", dps);
				fprintf(stderr, "Illegal \"DPs\"\n");
				fprintf(stderr, "GRED: only %d DPs are "
					"currently supported\n",MAX_DPs);
				return -1;
			}
		} else if (strcmp(*argv, "default") == 0) {
			NEXT_ARG();
			def_dp = strtol(*argv, (char **)NULL, 10);
			if (dps == 0) {
				fprintf(stderr, "\"default DP\" must be "
					"defined after DPs\n");
				return -1;
			}
			if (def_dp < 0 || def_dp > dps) {
				fprintf(stderr, 
					"\"default DP\" must be less than %d\n",
					opt.DPs);
				return -1;
			}
		} else if (strcmp(*argv, "grio") == 0) {
			opt.grio = 1;
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

	if (!dps || def_dp == -1) {
		fprintf(stderr, "Illegal gred setup parameters \n");
		return -1;
	}

	opt.DPs = dps;
	opt.def_DP = def_dp;

	DPRINTF("TC_GRED: sending DPs=%d default=%d\n",opt.DPs,opt.def_DP);
	n->nlmsg_flags|=NLM_F_CREATE;
	tail = NLMSG_TAIL(n);
	addattr_l(n, 1024, TCA_OPTIONS, NULL, 0);
	addattr_l(n, 1024, TCA_GRED_DPS, &opt, sizeof(struct tc_gred_sopt));
	tail->rta_len = (void *) NLMSG_TAIL(n) - (void *) tail;
	return 0;
}
/*
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
*/
static int gred_parse_opt(struct qdisc_util *qu, int argc, char **argv, struct nlmsghdr *n)
{
	int ok=0;
	struct tc_gred_qopt opt;
	unsigned burst = 0;
	unsigned avpkt = 0;
	double probability = 0.02;
	unsigned rate = 0;
	int wlog;
	__u8 sbuf[256];
	struct rtattr *tail;

	memset(&opt, 0, sizeof(opt));

	while (argc > 0) {
		if (strcmp(*argv, "limit") == 0) {
			NEXT_ARG();
			if (get_size(&opt.limit, *argv)) {
				fprintf(stderr, "Illegal \"limit\"\n");
				return -1;
			}
			ok++;
		} else if (strcmp(*argv, "setup") == 0) {
			if (ok) {
				fprintf(stderr, "Illegal \"setup\"\n");
				return -1;
			}
		return init_gred(qu,argc-1, argv+1,n);

		} else if (strcmp(*argv, "min") == 0) {
			NEXT_ARG();
			if (get_size(&opt.qth_min, *argv)) {
				fprintf(stderr, "Illegal \"min\"\n");
				return -1;
			}
			ok++;
		} else if (strcmp(*argv, "max") == 0) {
			NEXT_ARG();
			if (get_size(&opt.qth_max, *argv)) {
				fprintf(stderr, "Illegal \"max\"\n");
				return -1;
			}
			ok++;
		} else if (strcmp(*argv, "DP") == 0) {
			NEXT_ARG();
			opt.DP=strtol(*argv, (char **)NULL, 10);
			DPRINTF ("\n ******* DP =%u\n",opt.DP);
			if (opt.DP >MAX_DPs) { /* need a better error check */
				fprintf(stderr, "DP =%u \n",opt.DP);
				fprintf(stderr, "Illegal \"DP\"\n");
				fprintf(stderr, "GRED: only %d DPs are currently supported\n",MAX_DPs);
				return -1;
			}
			ok++;
		} else if (strcmp(*argv, "burst") == 0) {
			NEXT_ARG();
                        if (get_unsigned(&burst, *argv, 0)) {
				fprintf(stderr, "Illegal \"burst\"\n");
				return -1;
			}
			ok++;
		} else if (strcmp(*argv, "avpkt") == 0) {
			NEXT_ARG();
			if (get_size(&avpkt, *argv)) {
				fprintf(stderr, "Illegal \"avpkt\"\n");
				return -1;
			}
			ok++;
		} else if (strcmp(*argv, "probability") == 0) {
			NEXT_ARG();
			if (sscanf(*argv, "%lg", &probability) != 1) {
				fprintf(stderr, "Illegal \"probability\"\n");
				return -1;
			}
			ok++;
		} else if (strcmp(*argv, "prio") == 0) {
			NEXT_ARG();
			opt.prio=strtol(*argv, (char **)NULL, 10);
			/* some error check here */
			ok++;
		} else if (strcmp(*argv, "bandwidth") == 0) {
			NEXT_ARG();
			if (get_rate(&rate, *argv)) {
				fprintf(stderr, "Illegal \"bandwidth\"\n");
				return -1;
			}
			ok++;
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

	if (rate == 0)
		get_rate(&rate, "10Mbit");

	if (!opt.qth_min || !opt.qth_max || !burst || !opt.limit || !avpkt ||
	    (opt.DP<0)) {
		fprintf(stderr, "Required parameter (min, max, burst, limit, "
		    "avpkt, DP) is missing\n");
		return -1;
	}

	if ((wlog = tc_red_eval_ewma(opt.qth_min, burst, avpkt)) < 0) {
		fprintf(stderr, "GRED: failed to calculate EWMA constant.\n");
		return -1;
	}
	if (wlog >= 10)
		fprintf(stderr, "GRED: WARNING. Burst %d seems to be to "
		    "large.\n", burst);
	opt.Wlog = wlog;
	if ((wlog = tc_red_eval_P(opt.qth_min, opt.qth_max, probability)) < 0) {
		fprintf(stderr, "GRED: failed to calculate probability.\n");
		return -1;
	}
	opt.Plog = wlog;
	if ((wlog = tc_red_eval_idle_damping(opt.Wlog, avpkt, rate, sbuf)) < 0)
	    {
		fprintf(stderr, "GRED: failed to calculate idle damping "
		    "table.\n");
		return -1;
	}
	opt.Scell_log = wlog;

	tail = NLMSG_TAIL(n);
	addattr_l(n, 1024, TCA_OPTIONS, NULL, 0);
	addattr_l(n, 1024, TCA_GRED_PARMS, &opt, sizeof(opt));
	addattr_l(n, 1024, TCA_GRED_STAB, sbuf, 256);
	tail->rta_len = (void *) NLMSG_TAIL(n) - (void *) tail;
	return 0;
}

static int gred_print_opt(struct qdisc_util *qu, FILE *f, struct rtattr *opt)
{
	struct rtattr *tb[TCA_GRED_STAB+1];
	struct tc_gred_qopt *qopt;
	int i;
	SPRINT_BUF(b1);
	SPRINT_BUF(b2);
	SPRINT_BUF(b3);
	SPRINT_BUF(b4);
	SPRINT_BUF(b5);

	if (opt == NULL)
		return 0;

	parse_rtattr_nested(tb, TCA_GRED_STAB, opt);

	if (tb[TCA_GRED_PARMS] == NULL)
		return -1;

	qopt = RTA_DATA(tb[TCA_GRED_PARMS]);
	if (RTA_PAYLOAD(tb[TCA_GRED_PARMS])  < sizeof(*qopt)*MAX_DPs) {
		fprintf(f,"\n GRED received message smaller than expected\n");
		return -1;
		}

/* Bad hack! should really return a proper message as shown above*/

	for (i=0;i<MAX_DPs;i++, qopt++) {
		if (qopt->DP >= MAX_DPs) continue;
		fprintf(f, "\n DP:%d (prio %d) Average Queue %s Measured "
		    "Queue %s  ",
			qopt->DP,
			qopt->prio,
			sprint_size(qopt->qave, b4),
			sprint_size(qopt->backlog, b5));
		fprintf(f, "\n\t Packet drops: %d (forced %d early %d)  ",
			qopt->forced+qopt->early,
			qopt->forced,
			qopt->early);
		fprintf(f, "\n\t Packet totals: %u (bytes %u)  ",
			qopt->packets,
			qopt->bytesin);
		if (show_details)
			fprintf(f, "\n limit %s min %s max %s ",
				sprint_size(qopt->limit, b1),
				sprint_size(qopt->qth_min, b2),
				sprint_size(qopt->qth_max, b3));
				fprintf(f, "ewma %u Plog %u Scell_log %u",
				    qopt->Wlog, qopt->Plog, qopt->Scell_log);
	}
	return 0;
}

struct qdisc_util gred_qdisc_util = {
	.id		= "gred",
	.parse_qopt	= gred_parse_opt,
	.print_qopt	= gred_print_opt,
};
