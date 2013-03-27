/*
 * tc_util.c		Misc TC utility functions.
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
#include <math.h>

#include "utils.h"
#include "tc_util.h"

#ifndef LIBDIR
#define LIBDIR "/usr/lib/"
#endif

const char *get_tc_lib(void)
{
	const char *lib_dir;

	lib_dir = getenv("TC_LIB_DIR");
	if (!lib_dir)
		lib_dir = LIBDIR "/tc/";

	return lib_dir;
}

int get_qdisc_handle(__u32 *h, const char *str)
{
	__u32 maj;
	char *p;

	maj = TC_H_UNSPEC;
	if (strcmp(str, "none") == 0)
		goto ok;
	maj = strtoul(str, &p, 16);
	if (p == str)
		return -1;
	maj <<= 16;
	if (*p != ':' && *p!=0)
		return -1;
ok:
	*h = maj;
	return 0;
}

int get_tc_classid(__u32 *h, const char *str)
{
	__u32 maj, min;
	char *p;

	maj = TC_H_ROOT;
	if (strcmp(str, "root") == 0)
		goto ok;
	maj = TC_H_UNSPEC;
	if (strcmp(str, "none") == 0)
		goto ok;
	maj = strtoul(str, &p, 16);
	if (p == str) {
		maj = 0;
		if (*p != ':')
			return -1;
	}
	if (*p == ':') {
		if (maj >= (1<<16))
			return -1;
		maj <<= 16;
		str = p+1;
		min = strtoul(str, &p, 16);
		if (*p != 0)
			return -1;
		if (min >= (1<<16))
			return -1;
		maj |= min;
	} else if (*p != 0)
		return -1;

ok:
	*h = maj;
	return 0;
}

int print_tc_classid(char *buf, int len, __u32 h)
{
	if (h == TC_H_ROOT)
		sprintf(buf, "root");
	else if (h == TC_H_UNSPEC)
		snprintf(buf, len, "none");
	else if (TC_H_MAJ(h) == 0)
		snprintf(buf, len, ":%x", TC_H_MIN(h));
	else if (TC_H_MIN(h) == 0)
		snprintf(buf, len, "%x:", TC_H_MAJ(h)>>16);
	else
		snprintf(buf, len, "%x:%x", TC_H_MAJ(h)>>16, TC_H_MIN(h));
	return 0;
}

char * sprint_tc_classid(__u32 h, char *buf)
{
	if (print_tc_classid(buf, SPRINT_BSIZE-1, h))
		strcpy(buf, "???");
	return buf;
}

/* See http://physics.nist.gov/cuu/Units/binary.html */
static const struct rate_suffix {
	const char *name;
	double scale;
} suffixes[] = {
	{ "bit",	1. },
	{ "Kibit",	1024. },
	{ "kbit",	1000. },
	{ "mibit",	1024.*1024. },
	{ "mbit",	1000000. },
	{ "gibit",	1024.*1024.*1024. },
	{ "gbit",	1000000000. },
	{ "tibit",	1024.*1024.*1024.*1024. },
	{ "tbit",	1000000000000. },
	{ "Bps",	8. },
	{ "KiBps",	8.*1024. },
	{ "KBps",	8000. },
	{ "MiBps",	8.*1024*1024. },
	{ "MBps",	8000000. },
	{ "GiBps",	8.*1024.*1024.*1024. },
	{ "GBps",	8000000000. },
	{ "TiBps",	8.*1024.*1024.*1024.*1024. },
	{ "TBps",	8000000000000. },
	{ NULL }
};


int get_rate(unsigned *rate, const char *str)
{
	char *p;
	double bps = strtod(str, &p);
	const struct rate_suffix *s;

	if (p == str)
		return -1;

	if (*p == '\0') {
		*rate = bps / 8.;	/* assume bytes/sec */
		return 0;
	}

	for (s = suffixes; s->name; ++s) {
		if (strcasecmp(s->name, p) == 0) {
			*rate = (bps * s->scale) / 8.;
			return 0;
		}
	}

	return -1;
}

int get_rate_and_cell(unsigned *rate, int *cell_log, char *str)
{
	char * slash = strchr(str, '/');

	if (slash)
		*slash = 0;

	if (get_rate(rate, str))
		return -1;

	if (slash) {
		int cell;
		int i;

		if (get_integer(&cell, slash+1, 0))
			return -1;
		*slash = '/';

		for (i=0; i<32; i++) {
			if ((1<<i) == cell) {
				*cell_log = i;
				return 0;
			}
		}
		return -1;
	}
	return 0;
}

void print_rate(char *buf, int len, __u32 rate)
{
	double tmp = (double)rate*8;
	extern int use_iec;

	if (use_iec) {
		if (tmp >= 1000.0*1024.0*1024.0)
			snprintf(buf, len, "%.0fMibit", tmp/1024.0*1024.0);
		else if (tmp >= 1000.0*1024)
			snprintf(buf, len, "%.0fKibit", tmp/1024);
		else
			snprintf(buf, len, "%.0fbit", tmp);
	} else {
		if (tmp >= 1000.0*1000000.0)
			snprintf(buf, len, "%.0fMbit", tmp/1000000.0);
		else if (tmp >= 1000.0 * 1000.0)
			snprintf(buf, len, "%.0fKbit", tmp/1000.0);
		else
			snprintf(buf, len, "%.0fbit",  tmp);
	}
}

char * sprint_rate(__u32 rate, char *buf)
{
	print_rate(buf, SPRINT_BSIZE-1, rate);
	return buf;
}

int get_time(unsigned *time, const char *str)
{
	double t;
	char *p;

	t = strtod(str, &p);
	if (p == str)
		return -1;

	if (*p) {
		if (strcasecmp(p, "s") == 0 || strcasecmp(p, "sec")==0 ||
		    strcasecmp(p, "secs")==0)
			t *= TIME_UNITS_PER_SEC;
		else if (strcasecmp(p, "ms") == 0 || strcasecmp(p, "msec")==0 ||
			 strcasecmp(p, "msecs") == 0)
			t *= TIME_UNITS_PER_SEC/1000;
		else if (strcasecmp(p, "us") == 0 || strcasecmp(p, "usec")==0 ||
			 strcasecmp(p, "usecs") == 0)
			t *= TIME_UNITS_PER_SEC/1000000;
		else
			return -1;
	}

	*time = t;
	return 0;
}


void print_time(char *buf, int len, __u32 time)
{
	double tmp = time;

	if (tmp >= TIME_UNITS_PER_SEC)
		snprintf(buf, len, "%.1fs", tmp/TIME_UNITS_PER_SEC);
	else if (tmp >= TIME_UNITS_PER_SEC/1000)
		snprintf(buf, len, "%.1fms", tmp/(TIME_UNITS_PER_SEC/1000));
	else
		snprintf(buf, len, "%uus", time);
}

char * sprint_time(__u32 time, char *buf)
{
	print_time(buf, SPRINT_BSIZE-1, time);
	return buf;
}

char * sprint_ticks(__u32 ticks, char *buf)
{
	return sprint_time(tc_core_tick2time(ticks), buf);
}

int get_size(unsigned *size, const char *str)
{
	double sz;
	char *p;

	sz = strtod(str, &p);
	if (p == str)
		return -1;

	if (*p) {
		if (strcasecmp(p, "kb") == 0 || strcasecmp(p, "k")==0)
			sz *= 1024;
		else if (strcasecmp(p, "gb") == 0 || strcasecmp(p, "g")==0)
			sz *= 1024*1024*1024;
		else if (strcasecmp(p, "gbit") == 0)
			sz *= 1024*1024*1024/8;
		else if (strcasecmp(p, "mb") == 0 || strcasecmp(p, "m")==0)
			sz *= 1024*1024;
		else if (strcasecmp(p, "mbit") == 0)
			sz *= 1024*1024/8;
		else if (strcasecmp(p, "kbit") == 0)
			sz *= 1024/8;
		else if (strcasecmp(p, "b") != 0)
			return -1;
	}

	*size = sz;
	return 0;
}

int get_size_and_cell(unsigned *size, int *cell_log, char *str)
{
	char * slash = strchr(str, '/');

	if (slash)
		*slash = 0;

	if (get_size(size, str))
		return -1;

	if (slash) {
		int cell;
		int i;

		if (get_integer(&cell, slash+1, 0))
			return -1;
		*slash = '/';

		for (i=0; i<32; i++) {
			if ((1<<i) == cell) {
				*cell_log = i;
				return 0;
			}
		}
		return -1;
	}
	return 0;
}

void print_size(char *buf, int len, __u32 sz)
{
	double tmp = sz;

	if (sz >= 1024*1024 && fabs(1024*1024*rint(tmp/(1024*1024)) - sz) < 1024)
		snprintf(buf, len, "%gMb", rint(tmp/(1024*1024)));
	else if (sz >= 1024 && fabs(1024*rint(tmp/1024) - sz) < 16)
		snprintf(buf, len, "%gKb", rint(tmp/1024));
	else
		snprintf(buf, len, "%ub", sz);
}

char * sprint_size(__u32 size, char *buf)
{
	print_size(buf, SPRINT_BSIZE-1, size);
	return buf;
}

static const double max_percent_value = 0xffffffff;

int get_percent(__u32 *percent, const char *str)
{
	char *p;
	double per = strtod(str, &p) / 100.;

	if (per > 1. || per < 0)
		return -1;
	if (*p && strcmp(p, "%"))
		return -1;

	*percent = (unsigned) rint(per * max_percent_value);
	return 0;
}

void print_percent(char *buf, int len, __u32 per)
{
	snprintf(buf, len, "%g%%", 100. * (double) per / max_percent_value);
}

char * sprint_percent(__u32 per, char *buf)
{
	print_percent(buf, SPRINT_BSIZE-1, per);
	return buf;
}

void print_qdisc_handle(char *buf, int len, __u32 h)
{
	snprintf(buf, len, "%x:", TC_H_MAJ(h)>>16);
}

char * sprint_qdisc_handle(__u32 h, char *buf)
{
	print_qdisc_handle(buf, SPRINT_BSIZE-1, h);
	return buf;
}

char * action_n2a(int action, char *buf, int len)
{
	switch (action) {
	case -1:
		return "continue";
		break;
	case TC_ACT_OK:
		return "pass";
		break;
	case TC_ACT_SHOT:
		return "drop";
		break;
	case TC_ACT_RECLASSIFY:
		return "reclassify";
	case TC_ACT_PIPE:
		return "pipe";
	case TC_ACT_STOLEN:
		return "stolen";
	default:
		snprintf(buf, len, "%d", action);
		return buf;
	}
}

int action_a2n(char *arg, int *result)
{
	int res;

	if (matches(arg, "continue") == 0)
		res = -1;
	else if (matches(arg, "drop") == 0)
		res = TC_ACT_SHOT;
	else if (matches(arg, "shot") == 0)
		res = TC_ACT_SHOT;
	else if (matches(arg, "pass") == 0)
		res = TC_ACT_OK;
	else if (strcmp(arg, "ok") == 0)
		res = TC_ACT_OK;
	else if (matches(arg, "reclassify") == 0)
		res = TC_ACT_RECLASSIFY;
	else {
		char dummy;
		if (sscanf(arg, "%d%c", &res, &dummy) != 1)
			return -1;
	}
	*result = res;
	return 0;
}

int get_linklayer(unsigned *val, const char *arg)
{
	int res;

	if (matches(arg, "ethernet") == 0)
		res = LINKLAYER_ETHERNET;
	else if (matches(arg, "atm") == 0)
		res = LINKLAYER_ATM;
	else if (matches(arg, "adsl") == 0)
		res = LINKLAYER_ATM;
	else
		return -1; /* Indicate error */

	*val = res;
	return 0;
}

void print_linklayer(char *buf, int len, unsigned linklayer)
{
	switch (linklayer) {
	case LINKLAYER_UNSPEC:
		snprintf(buf, len, "%s", "unspec");
		return;
	case LINKLAYER_ETHERNET:
		snprintf(buf, len, "%s", "ethernet");
		return;
	case LINKLAYER_ATM:
		snprintf(buf, len, "%s", "atm");
		return;
	default:
		snprintf(buf, len, "%s", "unknown");
		return;
	}
}

char *sprint_linklayer(unsigned linklayer, char *buf)
{
	print_linklayer(buf, SPRINT_BSIZE-1, linklayer);
	return buf;
}

void print_tm(FILE * f, const struct tcf_t *tm)
{
	int hz = get_user_hz();
	if (tm->install != 0)
		fprintf(f, " installed %u sec", (unsigned)(tm->install/hz));
	if (tm->lastuse != 0)
		fprintf(f, " used %u sec", (unsigned)(tm->lastuse/hz));
	if (tm->expires != 0)
		fprintf(f, " expires %u sec", (unsigned)(tm->expires/hz));
}

void print_tcstats2_attr(FILE *fp, struct rtattr *rta, char *prefix, struct rtattr **xstats)
{
	SPRINT_BUF(b1);
	struct rtattr *tbs[TCA_STATS_MAX + 1];

	parse_rtattr_nested(tbs, TCA_STATS_MAX, rta);

	if (tbs[TCA_STATS_BASIC]) {
		struct gnet_stats_basic bs = {0};
		memcpy(&bs, RTA_DATA(tbs[TCA_STATS_BASIC]), MIN(RTA_PAYLOAD(tbs[TCA_STATS_BASIC]), sizeof(bs)));
		fprintf(fp, "%sSent %llu bytes %u pkt",
			prefix, (unsigned long long) bs.bytes, bs.packets);
	}

	if (tbs[TCA_STATS_QUEUE]) {
		struct gnet_stats_queue q = {0};
		memcpy(&q, RTA_DATA(tbs[TCA_STATS_QUEUE]), MIN(RTA_PAYLOAD(tbs[TCA_STATS_QUEUE]), sizeof(q)));
		fprintf(fp, " (dropped %u, overlimits %u requeues %u) ",
			q.drops, q.overlimits, q.requeues);
	}

	if (tbs[TCA_STATS_RATE_EST]) {
		struct gnet_stats_rate_est re = {0};
		memcpy(&re, RTA_DATA(tbs[TCA_STATS_RATE_EST]), MIN(RTA_PAYLOAD(tbs[TCA_STATS_RATE_EST]), sizeof(re)));
		fprintf(fp, "\n%srate %s %upps ",
			prefix, sprint_rate(re.bps, b1), re.pps);
	}

	if (tbs[TCA_STATS_QUEUE]) {
		struct gnet_stats_queue q = {0};
		memcpy(&q, RTA_DATA(tbs[TCA_STATS_QUEUE]), MIN(RTA_PAYLOAD(tbs[TCA_STATS_QUEUE]), sizeof(q)));
		if (!tbs[TCA_STATS_RATE_EST])
			fprintf(fp, "\n%s", prefix);
		fprintf(fp, "backlog %s %up requeues %u ",
			sprint_size(q.backlog, b1), q.qlen, q.requeues);
	}

	if (xstats)
		*xstats = tbs[TCA_STATS_APP] ? : NULL;
}

void print_tcstats_attr(FILE *fp, struct rtattr *tb[], char *prefix, struct rtattr **xstats)
{
	SPRINT_BUF(b1);

	if (tb[TCA_STATS2]) {
		print_tcstats2_attr(fp, tb[TCA_STATS2], prefix, xstats);
		if (xstats && NULL == *xstats)
			goto compat_xstats;
		return;
	}
	/* backward compatibility */
	if (tb[TCA_STATS]) {
		struct tc_stats st;

		/* handle case where kernel returns more/less than we know about */
		memset(&st, 0, sizeof(st));
		memcpy(&st, RTA_DATA(tb[TCA_STATS]), MIN(RTA_PAYLOAD(tb[TCA_STATS]), sizeof(st)));

		fprintf(fp, "%sSent %llu bytes %u pkts (dropped %u, overlimits %u) ",
			prefix, (unsigned long long)st.bytes, st.packets, st.drops,
			st.overlimits);

		if (st.bps || st.pps || st.qlen || st.backlog) {
			fprintf(fp, "\n%s", prefix);
			if (st.bps || st.pps) {
				fprintf(fp, "rate ");
				if (st.bps)
					fprintf(fp, "%s ", sprint_rate(st.bps, b1));
				if (st.pps)
					fprintf(fp, "%upps ", st.pps);
			}
			if (st.qlen || st.backlog) {
				fprintf(fp, "backlog ");
				if (st.backlog)
					fprintf(fp, "%s ", sprint_size(st.backlog, b1));
				if (st.qlen)
					fprintf(fp, "%up ", st.qlen);
			}
		}
	}

compat_xstats:
	if (tb[TCA_XSTATS] && xstats)
		*xstats = tb[TCA_XSTATS];
}

