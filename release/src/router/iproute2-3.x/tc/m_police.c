/*
 * m_police.c		Parse/print policing module options.
 *
 *		This program is free software; you can u32istribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:	Alexey Kuznetsov, <kuznet@ms2.inr.ac.ru>
 * FIXES:       19990619 - J Hadi Salim (hadi@cyberus.ca)
 *		simple addattr packaging fix.
 *		2002: J Hadi Salim - Add tc action extensions syntax
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

struct action_util police_action_util = {
	.id = "police",
	.parse_aopt = act_parse_police,
	.print_aopt = print_police,
};

static void usage(void)
{
	fprintf(stderr, "Usage: ... police rate BPS burst BYTES[/BYTES] [ mtu BYTES[/BYTES] ]\n");
	fprintf(stderr, "                [ peakrate BPS ] [ avrate BPS ] [ overhead BYTES ]\n");
	fprintf(stderr, "                [ linklayer TYPE ] [ ACTIONTERM ]\n");

	fprintf(stderr, "Old Syntax ACTIONTERM := action <EXCEEDACT>[/NOTEXCEEDACT] \n");
	fprintf(stderr, "New Syntax ACTIONTERM := conform-exceed <EXCEEDACT>[/NOTEXCEEDACT] \n");
	fprintf(stderr, "Where: *EXCEEDACT := pipe | ok | reclassify | drop | continue \n");
	fprintf(stderr, "Where:  pipe is only valid for new syntax \n");
	exit(-1);
}

static void explain1(char *arg)
{
	fprintf(stderr, "Illegal \"%s\"\n", arg);
}

char *police_action_n2a(int action, char *buf, int len)
{
	switch (action) {
	case -1:
		return "continue";
		break;
	case TC_POLICE_OK:
		return "pass";
		break;
	case TC_POLICE_SHOT:
		return "drop";
		break;
	case TC_POLICE_RECLASSIFY:
		return "reclassify";
	case TC_POLICE_PIPE:
		return "pipe";
	default:
		snprintf(buf, len, "%d", action);
		return buf;
	}
}

int police_action_a2n(char *arg, int *result)
{
	int res;

	if (matches(arg, "continue") == 0)
		res = -1;
	else if (matches(arg, "drop") == 0)
		res = TC_POLICE_SHOT;
	else if (matches(arg, "shot") == 0)
		res = TC_POLICE_SHOT;
	else if (matches(arg, "pass") == 0)
		res = TC_POLICE_OK;
	else if (strcmp(arg, "ok") == 0)
		res = TC_POLICE_OK;
	else if (matches(arg, "reclassify") == 0)
		res = TC_POLICE_RECLASSIFY;
	else if (matches(arg, "pipe") == 0)
		res = TC_POLICE_PIPE;
	else {
		char dummy;
		if (sscanf(arg, "%d%c", &res, &dummy) != 1)
			return -1;
	}
	*result = res;
	return 0;
}


int get_police_result(int *action, int *result, char *arg)
{
	char *p = strchr(arg, '/');

	if (p)
		*p = 0;

	if (police_action_a2n(arg, action)) {
		if (p)
			*p = '/';
		return -1;
	}

	if (p) {
		*p = '/';
		if (police_action_a2n(p+1, result))
			return -1;
	}
	return 0;
}


int act_parse_police(struct action_util *a,int *argc_p, char ***argv_p, int tca_id, struct nlmsghdr *n)
{
	int argc = *argc_p;
	char **argv = *argv_p;
	int res = -1;
	int ok=0;
	struct tc_police p;
	__u32 rtab[256];
	__u32 ptab[256];
	__u32 avrate = 0;
	int presult = 0;
	unsigned buffer=0, mtu=0, mpu=0;
	unsigned short overhead=0;
	unsigned int linklayer = LINKLAYER_ETHERNET; /* Assume ethernet */
	int Rcell_log=-1, Pcell_log = -1;
	struct rtattr *tail;

	memset(&p, 0, sizeof(p));
	p.action = TC_POLICE_RECLASSIFY;

	if (a) /* new way of doing things */
		NEXT_ARG();

	if (argc <= 0)
		return -1;

	while (argc > 0) {

		if (matches(*argv, "index") == 0) {
			NEXT_ARG();
			if (get_u32(&p.index, *argv, 10)) {
				fprintf(stderr, "Illegal \"index\"\n");
				return -1;
			}
		} else if (matches(*argv, "burst") == 0 ||
			strcmp(*argv, "buffer") == 0 ||
			strcmp(*argv, "maxburst") == 0) {
			NEXT_ARG();
			if (buffer) {
				fprintf(stderr, "Double \"buffer/burst\" spec\n");
				return -1;
			}
			if (get_size_and_cell(&buffer, &Rcell_log, *argv) < 0) {
				explain1("buffer");
				return -1;
			}
		} else if (strcmp(*argv, "mtu") == 0 ||
			   strcmp(*argv, "minburst") == 0) {
			NEXT_ARG();
			if (mtu) {
				fprintf(stderr, "Double \"mtu/minburst\" spec\n");
				return -1;
			}
			if (get_size_and_cell(&mtu, &Pcell_log, *argv) < 0) {
				explain1("mtu");
				return -1;
			}
		} else if (strcmp(*argv, "mpu") == 0) {
			NEXT_ARG();
			if (mpu) {
				fprintf(stderr, "Double \"mpu\" spec\n");
				return -1;
			}
			if (get_size(&mpu, *argv)) {
				explain1("mpu");
				return -1;
			}
		} else if (strcmp(*argv, "rate") == 0) {
			NEXT_ARG();
			if (p.rate.rate) {
				fprintf(stderr, "Double \"rate\" spec\n");
				return -1;
			}
			if (get_rate(&p.rate.rate, *argv)) {
				explain1("rate");
				return -1;
			}
		} else if (strcmp(*argv, "avrate") == 0) {
			NEXT_ARG();
			if (avrate) {
				fprintf(stderr, "Double \"avrate\" spec\n");
				return -1;
			}
			if (get_rate(&avrate, *argv)) {
				explain1("avrate");
				return -1;
			}
		} else if (matches(*argv, "peakrate") == 0) {
			NEXT_ARG();
			if (p.peakrate.rate) {
				fprintf(stderr, "Double \"peakrate\" spec\n");
				return -1;
			}
			if (get_rate(&p.peakrate.rate, *argv)) {
				explain1("peakrate");
				return -1;
			}
		} else if (matches(*argv, "reclassify") == 0) {
			p.action = TC_POLICE_RECLASSIFY;
		} else if (matches(*argv, "drop") == 0 ||
			   matches(*argv, "shot") == 0) {
			p.action = TC_POLICE_SHOT;
		} else if (matches(*argv, "continue") == 0) {
			p.action = TC_POLICE_UNSPEC;
		} else if (matches(*argv, "pass") == 0) {
			p.action = TC_POLICE_OK;
		} else if (matches(*argv, "pipe") == 0) {
			p.action = TC_POLICE_PIPE;
		} else if (strcmp(*argv, "action") == 0 ||
			   strcmp(*argv, "conform-exceed") == 0) {
			NEXT_ARG();
			if (get_police_result(&p.action, &presult, *argv)) {
				fprintf(stderr, "Illegal \"action\"\n");
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
		} else if (strcmp(*argv, "help") == 0) {
			usage();
		} else {
			break;
		}
		ok++;
		argc--; argv++;
	}

	if (!ok)
		return -1;

	if (p.rate.rate && !buffer) {
		fprintf(stderr, "\"burst\" requires \"rate\".\n");
		return -1;
	}
	if (p.peakrate.rate) {
		if (!p.rate.rate) {
			fprintf(stderr, "\"peakrate\" requires \"rate\".\n");
			return -1;
		}
		if (!mtu) {
			fprintf(stderr, "\"mtu\" is required, if \"peakrate\" is requested.\n");
			return -1;
		}
	}

	if (p.rate.rate) {
		p.rate.mpu = mpu;
		p.rate.overhead = overhead;
		if (tc_calc_rtable(&p.rate, rtab, Rcell_log, mtu, linklayer) < 0) {
			fprintf(stderr, "TBF: failed to calculate rate table.\n");
			return -1;
		}
		p.burst = tc_calc_xmittime(p.rate.rate, buffer);
	}
	p.mtu = mtu;
	if (p.peakrate.rate) {
		p.peakrate.mpu = mpu;
		p.peakrate.overhead = overhead;
		if (tc_calc_rtable(&p.peakrate, ptab, Pcell_log, mtu, linklayer) < 0) {
			fprintf(stderr, "POLICE: failed to calculate peak rate table.\n");
			return -1;
		}
	}

	tail = NLMSG_TAIL(n);
	addattr_l(n, MAX_MSG, tca_id, NULL, 0);
	addattr_l(n, MAX_MSG, TCA_POLICE_TBF, &p, sizeof(p));
	if (p.rate.rate)
		addattr_l(n, MAX_MSG, TCA_POLICE_RATE, rtab, 1024);
	if (p.peakrate.rate)
                addattr_l(n, MAX_MSG, TCA_POLICE_PEAKRATE, ptab, 1024);
	if (avrate)
		addattr32(n, MAX_MSG, TCA_POLICE_AVRATE, avrate);
	if (presult)
		addattr32(n, MAX_MSG, TCA_POLICE_RESULT, presult);

	tail->rta_len = (void *) NLMSG_TAIL(n) - (void *) tail;
	res = 0;

	*argc_p = argc;
	*argv_p = argv;
	return res;
}

int parse_police(int *argc_p, char ***argv_p, int tca_id, struct nlmsghdr *n)
{
	return act_parse_police(NULL,argc_p,argv_p,tca_id,n);
}

int
print_police(struct action_util *a, FILE *f, struct rtattr *arg)
{
	SPRINT_BUF(b1);
	struct tc_police *p;
	struct rtattr *tb[TCA_POLICE_MAX+1];
	unsigned buffer;

	if (arg == NULL)
		return 0;

	parse_rtattr_nested(tb, TCA_POLICE_MAX, arg);

	if (tb[TCA_POLICE_TBF] == NULL) {
		fprintf(f, "[NULL police tbf]");
		return 0;
	}
#ifndef STOOPID_8BYTE
	if (RTA_PAYLOAD(tb[TCA_POLICE_TBF])  < sizeof(*p)) {
		fprintf(f, "[truncated police tbf]");
		return -1;
	}
#endif
	p = RTA_DATA(tb[TCA_POLICE_TBF]);

	fprintf(f, " police 0x%x ", p->index);
	fprintf(f, "rate %s ", sprint_rate(p->rate.rate, b1));
	buffer = tc_calc_xmitsize(p->rate.rate, p->burst);
	fprintf(f, "burst %s ", sprint_size(buffer, b1));
	fprintf(f, "mtu %s ", sprint_size(p->mtu, b1));
	if (show_raw)
		fprintf(f, "[%08x] ", p->burst);
	if (p->peakrate.rate)
		fprintf(f, "peakrate %s ", sprint_rate(p->peakrate.rate, b1));
	if (tb[TCA_POLICE_AVRATE])
		fprintf(f, "avrate %s ", sprint_rate(*(__u32*)RTA_DATA(tb[TCA_POLICE_AVRATE]), b1));
	fprintf(f, "action %s", police_action_n2a(p->action, b1, sizeof(b1)));
	if (tb[TCA_POLICE_RESULT]) {
		fprintf(f, "/%s ", police_action_n2a(*(int*)RTA_DATA(tb[TCA_POLICE_RESULT]), b1, sizeof(b1)));
	} else
		fprintf(f, " ");
	fprintf(f, "overhead %ub ", p->rate.overhead);
	fprintf(f, "\nref %d bind %d\n",p->refcnt, p->bindcnt);

	return 0;
}

int
tc_print_police(FILE *f, struct rtattr *arg) {
	return print_police(&police_action_util,f,arg);
}
