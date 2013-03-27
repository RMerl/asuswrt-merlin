/*
 * q_htb.c		HTB.
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:	Martin Devera, devik@cdi.cz
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

#define HTB_TC_VER 0x30003
#if HTB_TC_VER >> 16 != TC_HTB_PROTOVER
#error "Different kernel and TC HTB versions"
#endif

static void explain(void)
{
	fprintf(stderr, "Usage: ... qdisc add ... htb [default N] [r2q N]\n"
		" default  minor id of class to which unclassified packets are sent {0}\n"
		" r2q      DRR quantums are computed as rate in Bps/r2q {10}\n"
		" debug    string of 16 numbers each 0-3 {0}\n\n"
		"... class add ... htb rate R1 [burst B1] [mpu B] [overhead O]\n"
		"                      [prio P] [slot S] [pslot PS]\n"
		"                      [ceil R2] [cburst B2] [mtu MTU] [quantum Q]\n"
		" rate     rate allocated to this class (class can still borrow)\n"
		" burst    max bytes burst which can be accumulated during idle period {computed}\n"
		" mpu      minimum packet size used in rate computations\n"
		" overhead per-packet size overhead used in rate computations\n"
		" linklay  adapting to a linklayer e.g. atm\n"
		" ceil     definite upper class rate (no borrows) {rate}\n"
		" cburst   burst but for ceil {computed}\n"
		" mtu      max packet size we create rate map for {1600}\n"
		" prio     priority of leaf; lower are served first {0}\n"
		" quantum  how much bytes to serve from leaf at once {use r2q}\n"
		"\nTC HTB version %d.%d\n",HTB_TC_VER>>16,HTB_TC_VER&0xffff
		);
}

static void explain1(char *arg)
{
    fprintf(stderr, "Illegal \"%s\"\n", arg);
    explain();
}


static int htb_parse_opt(struct qdisc_util *qu, int argc, char **argv, struct nlmsghdr *n)
{
	struct tc_htb_glob opt;
	struct rtattr *tail;
	unsigned i; char *p;
	memset(&opt,0,sizeof(opt));
	opt.rate2quantum = 10;
	opt.version = 3;

	while (argc > 0) {
		if (matches(*argv, "r2q") == 0) {
		    NEXT_ARG();
		    if (get_u32(&opt.rate2quantum, *argv, 10)) {
			explain1("r2q"); return -1;
		    }
		} else if (matches(*argv, "default") == 0) {
		    NEXT_ARG();
		    if (get_u32(&opt.defcls, *argv, 16)) {
			explain1("default"); return -1;
		    }
		} else if (matches(*argv, "debug") == 0) {
		    NEXT_ARG(); p = *argv;
		    for (i=0; i<16; i++,p++) {
			if (*p<'0' || *p>'3') break;
			opt.debug |= (*p-'0')<<(2*i);
		    }
		} else {
			fprintf(stderr, "What is \"%s\"?\n", *argv);
			explain();
			return -1;
		}
		argc--; argv++;
	}
	tail = NLMSG_TAIL(n);
	addattr_l(n, 1024, TCA_OPTIONS, NULL, 0);
	addattr_l(n, 2024, TCA_HTB_INIT, &opt, NLMSG_ALIGN(sizeof(opt)));
	tail->rta_len = (void *) NLMSG_TAIL(n) - (void *) tail;
	return 0;
}

static int htb_parse_class_opt(struct qdisc_util *qu, int argc, char **argv, struct nlmsghdr *n)
{
	int ok=0;
	struct tc_htb_opt opt;
	__u32 rtab[256],ctab[256];
	unsigned buffer=0,cbuffer=0;
	int cell_log=-1,ccell_log = -1;
	unsigned mtu;
	unsigned short mpu = 0;
	unsigned short overhead = 0;
	unsigned int linklayer  = LINKLAYER_ETHERNET; /* Assume ethernet */
	struct rtattr *tail;

	memset(&opt, 0, sizeof(opt)); mtu = 1600; /* eth packet len */

	while (argc > 0) {
		if (matches(*argv, "prio") == 0) {
			NEXT_ARG();
			if (get_u32(&opt.prio, *argv, 10)) {
				explain1("prio"); return -1;
			}
			ok++;
		} else if (matches(*argv, "mtu") == 0) {
			NEXT_ARG();
			if (get_u32(&mtu, *argv, 10)) {
				explain1("mtu"); return -1;
			}
		} else if (matches(*argv, "mpu") == 0) {
			NEXT_ARG();
			if (get_u16(&mpu, *argv, 10)) {
				explain1("mpu"); return -1;
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
		} else if (matches(*argv, "quantum") == 0) {
			NEXT_ARG();
			if (get_u32(&opt.quantum, *argv, 10)) {
				explain1("quantum"); return -1;
			}
		} else if (matches(*argv, "burst") == 0 ||
			strcmp(*argv, "buffer") == 0 ||
			strcmp(*argv, "maxburst") == 0) {
			NEXT_ARG();
			if (get_size_and_cell(&buffer, &cell_log, *argv) < 0) {
				explain1("buffer");
				return -1;
			}
			ok++;
		} else if (matches(*argv, "cburst") == 0 ||
			strcmp(*argv, "cbuffer") == 0 ||
			strcmp(*argv, "cmaxburst") == 0) {
			NEXT_ARG();
			if (get_size_and_cell(&cbuffer, &ccell_log, *argv) < 0) {
				explain1("cbuffer");
				return -1;
			}
			ok++;
		} else if (strcmp(*argv, "ceil") == 0) {
			NEXT_ARG();
			if (opt.ceil.rate) {
				fprintf(stderr, "Double \"ceil\" spec\n");
				return -1;
			}
			if (get_rate(&opt.ceil.rate, *argv)) {
				explain1("ceil");
				return -1;
			}
			ok++;
		} else if (strcmp(*argv, "rate") == 0) {
			NEXT_ARG();
			if (opt.rate.rate) {
				fprintf(stderr, "Double \"rate\" spec\n");
				return -1;
			}
			if (get_rate(&opt.rate.rate, *argv)) {
				explain1("rate");
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

/*	if (!ok)
		return 0;*/

	if (opt.rate.rate == 0) {
		fprintf(stderr, "\"rate\" is required.\n");
		return -1;
	}
	/* if ceil params are missing, use the same as rate */
	if (!opt.ceil.rate) opt.ceil = opt.rate;

	/* compute minimal allowed burst from rate; mtu is added here to make
	   sute that buffer is larger than mtu and to have some safeguard space */
	if (!buffer) buffer = opt.rate.rate / get_hz() + mtu;
	if (!cbuffer) cbuffer = opt.ceil.rate / get_hz() + mtu;

	opt.ceil.overhead = overhead;
	opt.rate.overhead = overhead;

	opt.ceil.mpu = mpu;
	opt.rate.mpu = mpu;

	if (tc_calc_rtable(&opt.rate, rtab, cell_log, mtu, linklayer) < 0) {
		fprintf(stderr, "htb: failed to calculate rate table.\n");
		return -1;
	}
	opt.buffer = tc_calc_xmittime(opt.rate.rate, buffer);

	if (tc_calc_rtable(&opt.ceil, ctab, ccell_log, mtu, linklayer) < 0) {
		fprintf(stderr, "htb: failed to calculate ceil rate table.\n");
		return -1;
	}
	opt.cbuffer = tc_calc_xmittime(opt.ceil.rate, cbuffer);

	tail = NLMSG_TAIL(n);
	addattr_l(n, 1024, TCA_OPTIONS, NULL, 0);
	addattr_l(n, 2024, TCA_HTB_PARMS, &opt, sizeof(opt));
	addattr_l(n, 3024, TCA_HTB_RTAB, rtab, 1024);
	addattr_l(n, 4024, TCA_HTB_CTAB, ctab, 1024);
	tail->rta_len = (void *) NLMSG_TAIL(n) - (void *) tail;
	return 0;
}

static int htb_print_opt(struct qdisc_util *qu, FILE *f, struct rtattr *opt)
{
	struct rtattr *tb[TCA_HTB_RTAB+1];
	struct tc_htb_opt *hopt;
	struct tc_htb_glob *gopt;
	double buffer,cbuffer;
	SPRINT_BUF(b1);
	SPRINT_BUF(b2);
	SPRINT_BUF(b3);

	if (opt == NULL)
		return 0;

	parse_rtattr_nested(tb, TCA_HTB_RTAB, opt);

	if (tb[TCA_HTB_PARMS]) {

	    hopt = RTA_DATA(tb[TCA_HTB_PARMS]);
	    if (RTA_PAYLOAD(tb[TCA_HTB_PARMS])  < sizeof(*hopt)) return -1;

		if (!hopt->level) {
			fprintf(f, "prio %d ", (int)hopt->prio);
			if (show_details)
				fprintf(f, "quantum %d ", (int)hopt->quantum);
		}
	    fprintf(f, "rate %s ", sprint_rate(hopt->rate.rate, b1));
	    buffer = tc_calc_xmitsize(hopt->rate.rate, hopt->buffer);
	    fprintf(f, "ceil %s ", sprint_rate(hopt->ceil.rate, b1));
	    cbuffer = tc_calc_xmitsize(hopt->ceil.rate, hopt->cbuffer);
	    if (show_details) {
		fprintf(f, "burst %s/%u mpu %s overhead %s ",
			sprint_size(buffer, b1),
			1<<hopt->rate.cell_log,
			sprint_size(hopt->rate.mpu&0xFF, b2),
			sprint_size((hopt->rate.mpu>>8)&0xFF, b3));
		fprintf(f, "cburst %s/%u mpu %s overhead %s ",
			sprint_size(cbuffer, b1),
			1<<hopt->ceil.cell_log,
			sprint_size(hopt->ceil.mpu&0xFF, b2),
			sprint_size((hopt->ceil.mpu>>8)&0xFF, b3));
		fprintf(f, "level %d ", (int)hopt->level);
	    } else {
		fprintf(f, "burst %s ", sprint_size(buffer, b1));
		fprintf(f, "cburst %s ", sprint_size(cbuffer, b1));
	    }
	    if (show_raw)
		fprintf(f, "buffer [%08x] cbuffer [%08x] ",
			hopt->buffer,hopt->cbuffer);
	}
	if (tb[TCA_HTB_INIT]) {
	    gopt = RTA_DATA(tb[TCA_HTB_INIT]);
	    if (RTA_PAYLOAD(tb[TCA_HTB_INIT])  < sizeof(*gopt)) return -1;

	    fprintf(f, "r2q %d default %x direct_packets_stat %u",
		    gopt->rate2quantum,gopt->defcls,gopt->direct_pkts);
		if (show_details)
			fprintf(f," ver %d.%d",gopt->version >> 16,gopt->version & 0xffff);
	}
	return 0;
}

static int htb_print_xstats(struct qdisc_util *qu, FILE *f, struct rtattr *xstats)
{
	struct tc_htb_xstats *st;
	if (xstats == NULL)
		return 0;

	if (RTA_PAYLOAD(xstats) < sizeof(*st))
		return -1;

	st = RTA_DATA(xstats);
	fprintf(f, " lended: %u borrowed: %u giants: %u\n",
		st->lends,st->borrows,st->giants);
	fprintf(f, " tokens: %d ctokens: %d\n", st->tokens,st->ctokens);
	return 0;
}

struct qdisc_util htb_qdisc_util = {
	.id 		= "htb",
	.parse_qopt	= htb_parse_opt,
	.print_qopt	= htb_print_opt,
	.print_xstats 	= htb_print_xstats,
	.parse_copt	= htb_parse_class_opt,
	.print_copt	= htb_print_opt,
};

/* for testing of old one */
struct qdisc_util htb2_qdisc_util = {
	.id		=  "htb2",
	.parse_qopt	= htb_parse_opt,
	.print_qopt	= htb_print_opt,
	.print_xstats 	= htb_print_xstats,
	.parse_copt	= htb_parse_class_opt,
	.print_copt	= htb_print_opt,
};
