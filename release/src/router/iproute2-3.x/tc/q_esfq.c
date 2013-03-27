/*
 * q_esfq.c		ESFQ.
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:	Alexey Kuznetsov, <kuznet@ms2.inr.ac.ru>
 *
 * Changes:	Alexander Atanasov, <alex@ssi.bg>
 *		Alexander Clouter, <alex@digriz.org.uk>
 *		Corey Hickey, <bugfood-c@fatooh.org>
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <fcntl.h>
#include <math.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

#include "utils.h"
#include "tc_util.h"

static void explain(void)
{
	fprintf(stderr, "Usage: ... esfq [ perturb SECS ] [ quantum BYTES ] [ depth FLOWS ]\n\t[ divisor HASHBITS ] [ limit PKTS ] [ hash HASHTYPE]\n");
	fprintf(stderr,"Where: \n");
	fprintf(stderr,"HASHTYPE := { classic | src | dst | fwmark | ctorigdst | ctorigsrc | ctrepldst | ctreplsrc | ctnatchg }\n");
}

static void explain1(char *arg)
{
    fprintf(stderr, "Illegal \"%s\"\n", arg);
    explain();
}

#define usage() return(-1)

static int esfq_parse_opt(struct qdisc_util *qu, int argc, char **argv, struct nlmsghdr *n)
{
	int ok=0;
	struct tc_esfq_qopt opt;

	memset(&opt, 0, sizeof(opt));

	opt.hash_kind= TCA_SFQ_HASH_CLASSIC;

	while (argc > 0) {
		if (strcmp(*argv, "quantum") == 0) {
			NEXT_ARG();
			if (get_size(&opt.quantum, *argv)) {
				explain1("quantum"); return -1;
			}
			ok++;
		} else if (strcmp(*argv, "perturb") == 0) {
			NEXT_ARG();
			if (get_integer(&opt.perturb_period, *argv, 0)) {
				explain1("perturb"); return -1;
			}
			ok++;
		} else if (strcmp(*argv, "depth") == 0) {
			NEXT_ARG();
			if (get_integer((int *) &opt.flows, *argv, 0)) {
				explain1("depth"); return -1;
			}
			ok++;
		} else if (strcmp(*argv, "divisor") == 0) {
			NEXT_ARG();
			if (get_integer((int *) &opt.divisor, *argv, 0)) {
				explain1("divisor"); return -1;
			}
			if(opt.divisor >= 14) {
				fprintf(stderr, "Illegal \"divisor\": must be < 14\n");
				return -1;
			}
			opt.divisor=pow(2,opt.divisor);
			ok++;
		} else if (strcmp(*argv, "limit") == 0) {
			NEXT_ARG();
			if (get_integer((int *) &opt.limit, *argv, 0)) {
				explain1("limit"); return -1;
			}
			ok++;
		} else if (strcmp(*argv, "hash") == 0) {
			NEXT_ARG();
			if(strcmp(*argv, "classic") == 0) {
				opt.hash_kind= TCA_SFQ_HASH_CLASSIC;
			} else
			if(strcmp(*argv, "dst") == 0) {
				opt.hash_kind= TCA_SFQ_HASH_DST;
			} else
			if(strcmp(*argv, "src") == 0) {
				opt.hash_kind= TCA_SFQ_HASH_SRC;
			} else
			if(strcmp(*argv, "fwmark") == 0) {
				opt.hash_kind= TCA_SFQ_HASH_FWMARK;
			} else
			if(strcmp(*argv, "ctorigsrc") == 0) {
				opt.hash_kind= TCA_SFQ_HASH_CTORIGSRC;
			} else
			if(strcmp(*argv, "ctorigdst") == 0) {
				opt.hash_kind= TCA_SFQ_HASH_CTORIGDST;
			} else
			if(strcmp(*argv, "ctreplsrc") == 0) {
				opt.hash_kind= TCA_SFQ_HASH_CTREPLSRC;
			} else
			if(strcmp(*argv, "ctrepldst") == 0) {
				opt.hash_kind= TCA_SFQ_HASH_CTREPLDST;
			} else
			if(strcmp(*argv, "ctnatchg") == 0) {
				opt.hash_kind= TCA_SFQ_HASH_CTNATCHG;
			} else {
				explain1("hash"); return -1;
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

	if (ok)
		addattr_l(n, 1024, TCA_OPTIONS, &opt, sizeof(opt));
	return 0;
}

static int esfq_print_opt(struct qdisc_util *qu, FILE *f, struct rtattr *opt)
{
	struct tc_esfq_qopt *qopt;
	SPRINT_BUF(b1);

	if (opt == NULL)
		return 0;

	if (RTA_PAYLOAD(opt)  < sizeof(*qopt))
		return -1;
	qopt = RTA_DATA(opt);
	fprintf(f, "quantum %s ", sprint_size(qopt->quantum, b1));
	if (show_details) {
		fprintf(f, "limit %up flows %u/%u ",
			qopt->limit, qopt->flows, qopt->divisor);
	}
	if (qopt->perturb_period)
		fprintf(f, "perturb %dsec ", qopt->perturb_period);

		fprintf(f,"hash: ");
	switch(qopt->hash_kind)
	{
	case TCA_SFQ_HASH_CLASSIC:
		fprintf(f,"classic");
		break;
	case TCA_SFQ_HASH_DST:
		fprintf(f,"dst");
		break;
	case TCA_SFQ_HASH_SRC:
		fprintf(f,"src");
		break;
	case TCA_SFQ_HASH_FWMARK:
		fprintf(f,"fwmark");
		break;
	case TCA_SFQ_HASH_CTORIGSRC:
		fprintf(f,"ctorigsrc");
		break;
	case TCA_SFQ_HASH_CTORIGDST:
		fprintf(f,"ctorigdst");
		break;
	case TCA_SFQ_HASH_CTREPLSRC:
		fprintf(f,"ctreplsrc");
		break;
	case TCA_SFQ_HASH_CTREPLDST:
		fprintf(f,"ctrepldst");
		break;
	case TCA_SFQ_HASH_CTNATCHG:
		fprintf(f,"ctnatchg");
		break;
	default:
		fprintf(f,"Unknown");
	}
	return 0;
}

static int esfq_print_xstats(struct qdisc_util *qu, FILE *f, struct rtattr *xstats)
{
	return 0;
}


struct qdisc_util esfq_util = {
	NULL,
	"esfq",
	esfq_parse_opt,
	esfq_print_opt,
	esfq_print_xstats,
};
