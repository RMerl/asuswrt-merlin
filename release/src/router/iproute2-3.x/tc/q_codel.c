/*
 * Codel - The Controlled-Delay Active Queue Management algorithm
 *
 *  Copyright (C) 2011-2012 Kathleen Nichols <nichols@pollere.com>
 *  Copyright (C) 2011-2012 Van Jacobson <van@pollere.com>
 *  Copyright (C) 2012 Michael D. Taht <dave.taht@bufferbloat.net>
 *  Copyright (C) 2012 Eric Dumazet <edumazet@google.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The names of the authors may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * Alternatively, provided that this notice is retained in full, this
 * software may be distributed under the terms of the GNU General
 * Public License ("GPL") version 2, in which case the provisions of the
 * GPL apply INSTEAD OF those given above.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
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
	fprintf(stderr, "Usage: ... codel [ limit PACKETS ] [ target TIME]\n");
	fprintf(stderr, "                 [ interval TIME ] [ ecn | noecn ]\n");
}

static int codel_parse_opt(struct qdisc_util *qu, int argc, char **argv,
			   struct nlmsghdr *n)
{
	unsigned limit = 0;
	unsigned target = 0;
	unsigned interval = 0;
	int ecn = -1;
	struct rtattr *tail;

	while (argc > 0) {
		if (strcmp(*argv, "limit") == 0) {
			NEXT_ARG();
			if (get_unsigned(&limit, *argv, 0)) {
				fprintf(stderr, "Illegal \"limit\"\n");
				return -1;
			}
		} else if (strcmp(*argv, "target") == 0) {
			NEXT_ARG();
			if (get_time(&target, *argv)) {
				fprintf(stderr, "Illegal \"target\"\n");
				return -1;
			}
		} else if (strcmp(*argv, "interval") == 0) {
			NEXT_ARG();
			if (get_time(&interval, *argv)) {
				fprintf(stderr, "Illegal \"interval\"\n");
				return -1;
			}
		} else if (strcmp(*argv, "ecn") == 0) {
			ecn = 1;
		} else if (strcmp(*argv, "noecn") == 0) {
			ecn = 0;
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

	tail = NLMSG_TAIL(n);
	addattr_l(n, 1024, TCA_OPTIONS, NULL, 0);
	if (limit)
		addattr_l(n, 1024, TCA_CODEL_LIMIT, &limit, sizeof(limit));
	if (interval)
		addattr_l(n, 1024, TCA_CODEL_INTERVAL, &interval, sizeof(interval));
	if (target)
		addattr_l(n, 1024, TCA_CODEL_TARGET, &target, sizeof(target));
	if (ecn != -1)
		addattr_l(n, 1024, TCA_CODEL_ECN, &ecn, sizeof(ecn));
	tail->rta_len = (void *) NLMSG_TAIL(n) - (void *) tail;
	return 0;
}

static int codel_print_opt(struct qdisc_util *qu, FILE *f, struct rtattr *opt)
{
	struct rtattr *tb[TCA_CODEL_MAX + 1];
	unsigned limit;
	unsigned interval;
	unsigned target;
	unsigned ecn;
	SPRINT_BUF(b1);

	if (opt == NULL)
		return 0;

	parse_rtattr_nested(tb, TCA_CODEL_MAX, opt);

	if (tb[TCA_CODEL_LIMIT] &&
	    RTA_PAYLOAD(tb[TCA_CODEL_LIMIT]) >= sizeof(__u32)) {
		limit = rta_getattr_u32(tb[TCA_CODEL_LIMIT]);
		fprintf(f, "limit %up ", limit);
	}
	if (tb[TCA_CODEL_TARGET] &&
	    RTA_PAYLOAD(tb[TCA_CODEL_TARGET]) >= sizeof(__u32)) {
		target = rta_getattr_u32(tb[TCA_CODEL_TARGET]);
		fprintf(f, "target %s ", sprint_time(target, b1));
	}
	if (tb[TCA_CODEL_INTERVAL] &&
	    RTA_PAYLOAD(tb[TCA_CODEL_INTERVAL]) >= sizeof(__u32)) {
		interval = rta_getattr_u32(tb[TCA_CODEL_INTERVAL]);
		fprintf(f, "interval %s ", sprint_time(interval, b1));
	}
	if (tb[TCA_CODEL_ECN] &&
	    RTA_PAYLOAD(tb[TCA_CODEL_ECN]) >= sizeof(__u32)) {
		ecn = rta_getattr_u32(tb[TCA_CODEL_ECN]);
		if (ecn)
			fprintf(f, "ecn ");
	}

	return 0;
}

static int codel_print_xstats(struct qdisc_util *qu, FILE *f,
			      struct rtattr *xstats)
{
	struct tc_codel_xstats *st;
	SPRINT_BUF(b1);

	if (xstats == NULL)
		return 0;

	if (RTA_PAYLOAD(xstats) < sizeof(*st))
		return -1;

	st = RTA_DATA(xstats);
	fprintf(f, "  count %u lastcount %u ldelay %s",
		st->count, st->lastcount, sprint_time(st->ldelay, b1));
	if (st->dropping)
		fprintf(f, " dropping");
	if (st->drop_next < 0)
		fprintf(f, " drop_next -%s", sprint_time(-st->drop_next, b1));
	else
		fprintf(f, " drop_next %s", sprint_time(st->drop_next, b1));
	fprintf(f, "\n  maxpacket %u ecn_mark %u drop_overlimit %u",
		st->maxpacket, st->ecn_mark, st->drop_overlimit);
	return 0;

}

struct qdisc_util codel_qdisc_util = {
	.id		= "codel",
	.parse_qopt	= codel_parse_opt,
	.print_qopt	= codel_print_opt,
	.print_xstats	= codel_print_xstats,
};
