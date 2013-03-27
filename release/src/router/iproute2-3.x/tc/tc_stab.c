/*
 * tc_stab.c		"tc qdisc ... stab *".
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:	Jussi Kivilinna, <jussi.kivilinna@mbnet.fi>
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
#include <malloc.h>

#include "utils.h"
#include "tc_util.h"
#include "tc_core.h"
#include "tc_common.h"

static void stab_help(void)
{
	fprintf(stderr,
		"Usage: ... stab [ mtu BYTES ] [ tsize SLOTS ] [ mpu BYTES ] \n"
		"                [ overhead BYTES ] [ linklayer TYPE ] ...\n"
		"   mtu       : max packet size we create rate map for {2047}\n"
		"   tsize     : how many slots should size table have {512}\n"
		"   mpu       : minimum packet size used in rate computations\n"
		"   overhead  : per-packet size overhead used in rate computations\n"
		"   linklayer : adapting to a linklayer e.g. atm\n"
		"Example: ... stab overhead 20 linklayer atm\n");

	return;
}

int check_size_table_opts(struct tc_sizespec *s)
{
	return s->linklayer >= LINKLAYER_ETHERNET || s->mpu != 0 ||
							s->overhead != 0;
}

int parse_size_table(int *argcp, char ***argvp, struct tc_sizespec *sp)
{
	char **argv = *argvp;
	int argc = *argcp;
	struct tc_sizespec s;

	memset(&s, 0, sizeof(s));

	NEXT_ARG();
	if (matches(*argv, "help") == 0) {
		stab_help();
		return -1;
	}
	while (argc > 0) {
		if (matches(*argv, "mtu") == 0) {
			NEXT_ARG();
			if (s.mtu)
				duparg("mtu", *argv);
			if (get_u32(&s.mtu, *argv, 10)) {
				invarg("mtu", "invalid mtu");
				return -1;
			}
		} else if (matches(*argv, "mpu") == 0) {
			NEXT_ARG();
			if (s.mpu)
				duparg("mpu", *argv);
			if (get_u32(&s.mpu, *argv, 10)) {
				invarg("mpu", "invalid mpu");
				return -1;
			}
		} else if (matches(*argv, "overhead") == 0) {
			NEXT_ARG();
			if (s.overhead)
				duparg("overhead", *argv);
			if (get_integer(&s.overhead, *argv, 10)) {
				invarg("overhead", "invalid overhead");
				return -1;
			}
		} else if (matches(*argv, "tsize") == 0) {
			NEXT_ARG();
			if (s.tsize)
				duparg("tsize", *argv);
			if (get_u32(&s.tsize, *argv, 10)) {
				invarg("tsize", "invalid table size");
				return -1;
			}
		} else if (matches(*argv, "linklayer") == 0) {
			NEXT_ARG();
			if (s.linklayer != LINKLAYER_UNSPEC)
				duparg("linklayer", *argv);
			if (get_linklayer(&s.linklayer, *argv)) {
				invarg("linklayer", "invalid linklayer");
				return -1;
			}
		} else
			break;
		argc--; argv++;
	}

	if (!check_size_table_opts(&s))
		return -1;

	*sp = s;
	*argvp = argv;
	*argcp = argc;
	return 0;
}

void print_size_table(FILE *fp, const char *prefix, struct rtattr *rta)
{
	struct rtattr *tb[TCA_STAB_MAX + 1];
	SPRINT_BUF(b1);

	parse_rtattr_nested(tb, TCA_STAB_MAX, rta);

	if (tb[TCA_STAB_BASE]) {
		struct tc_sizespec s = {0};
		memcpy(&s, RTA_DATA(tb[TCA_STAB_BASE]),
				MIN(RTA_PAYLOAD(tb[TCA_STAB_BASE]), sizeof(s)));

		fprintf(fp, "%s", prefix);
		if (s.linklayer)
			fprintf(fp, "linklayer %s ",
					sprint_linklayer(s.linklayer, b1));
		if (s.overhead)
			fprintf(fp, "overhead %d ", s.overhead);
		if (s.mpu)
			fprintf(fp, "mpu %u ", s.mpu);
		if (s.mtu)
			fprintf(fp, "mtu %u ", s.mtu);
		if (s.tsize)
			fprintf(fp, "tsize %u ", s.tsize);
	}

#if 0
	if (tb[TCA_STAB_DATA]) {
		unsigned i, j, dlen;
		__u16 *data = RTA_DATA(tb[TCA_STAB_DATA]);
		dlen = RTA_PAYLOAD(tb[TCA_STAB_DATA]) / sizeof(__u16);

		fprintf(fp, "\n%sstab data:", prefix);
		for (i = 0; i < dlen/12; i++) {
			fprintf(fp, "\n%s %3u:", prefix, i * 12);
			for (j = 0; i * 12 + j < dlen; j++)
				fprintf(fp, " %05x", data[i * 12 + j]);
		}
	}
#endif
}

