/*
 * Fair Queue
 *
 *  Copyright (C) 2013 Eric Dumazet <edumazet@google.com>
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
#include <stdbool.h>

#include "utils.h"
#include "tc_util.h"

static void explain(void)
{
	fprintf(stderr, "Usage: ... fq [ limit PACKETS ] [ flow_limit PACKETS ]\n");
	fprintf(stderr, "              [ quantum BYTES ] [ initial_quantum BYTES ]\n");
	fprintf(stderr, "              [ maxrate RATE  ] [ buckets NUMBER ]\n");
	fprintf(stderr, "              [ [no]pacing ]\n");
}

static unsigned int ilog2(unsigned int val)
{
	unsigned int res = 0;

	val--;
	while (val) {
		res++;
		val >>= 1;
	}
	return res;
}

static int fq_parse_opt(struct qdisc_util *qu, int argc, char **argv,
			struct nlmsghdr *n)
{
	unsigned int plimit;
	unsigned int flow_plimit;
	unsigned int quantum;
	unsigned int initial_quantum;
	unsigned int buckets = 0;
	unsigned int maxrate;
	unsigned int defrate;
	bool set_plimit = false;
	bool set_flow_plimit = false;
	bool set_quantum = false;
	bool set_initial_quantum = false;
	bool set_maxrate = false;
	bool set_defrate = false;
	int pacing = -1;
	struct rtattr *tail;

	while (argc > 0) {
		if (strcmp(*argv, "limit") == 0) {
			NEXT_ARG();
			if (get_unsigned(&plimit, *argv, 0)) {
				fprintf(stderr, "Illegal \"limit\"\n");
				return -1;
			}
			set_plimit = true;
		} else if (strcmp(*argv, "flow_limit") == 0) {
			NEXT_ARG();
			if (get_unsigned(&flow_plimit, *argv, 0)) {
				fprintf(stderr, "Illegal \"flow_limit\"\n");
				return -1;
			}
			set_flow_plimit = true;
		} else if (strcmp(*argv, "buckets") == 0) {
			NEXT_ARG();
			if (get_unsigned(&buckets, *argv, 0)) {
				fprintf(stderr, "Illegal \"buckets\"\n");
				return -1;
			}
		} else if (strcmp(*argv, "maxrate") == 0) {
			NEXT_ARG();
			if (get_rate(&maxrate, *argv)) {
				fprintf(stderr, "Illegal \"maxrate\"\n");
				return -1;
			}
			set_maxrate = true;
		} else if (strcmp(*argv, "defrate") == 0) {
			NEXT_ARG();
			if (get_rate(&defrate, *argv)) {
				fprintf(stderr, "Illegal \"defrate\"\n");
				return -1;
			}
			set_defrate = true;
		} else if (strcmp(*argv, "quantum") == 0) {
			NEXT_ARG();
			if (get_unsigned(&quantum, *argv, 0)) {
				fprintf(stderr, "Illegal \"quantum\"\n");
				return -1;
			}
			set_quantum = true;
		} else if (strcmp(*argv, "initial_quantum") == 0) {
			NEXT_ARG();
			if (get_unsigned(&initial_quantum, *argv, 0)) {
				fprintf(stderr, "Illegal \"initial_quantum\"\n");
				return -1;
			}
			set_initial_quantum = true;
		} else if (strcmp(*argv, "pacing") == 0) {
			pacing = 1;
		} else if (strcmp(*argv, "nopacing") == 0) {
			pacing = 0;
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
	if (buckets) {
		unsigned int log = ilog2(buckets);

		addattr_l(n, 1024, TCA_FQ_BUCKETS_LOG,
			  &log, sizeof(log));
	}
	if (set_plimit)
		addattr_l(n, 1024, TCA_FQ_PLIMIT,
			  &plimit, sizeof(plimit));
	if (set_flow_plimit)
		addattr_l(n, 1024, TCA_FQ_FLOW_PLIMIT,
			  &flow_plimit, sizeof(flow_plimit));
	if (set_quantum)
		addattr_l(n, 1024, TCA_FQ_QUANTUM, &quantum, sizeof(quantum));
	if (set_initial_quantum)
		addattr_l(n, 1024, TCA_FQ_INITIAL_QUANTUM,
			  &initial_quantum, sizeof(initial_quantum));
	if (pacing != -1)
		addattr_l(n, 1024, TCA_FQ_RATE_ENABLE,
			  &pacing, sizeof(pacing));
	if (set_maxrate)
		addattr_l(n, 1024, TCA_FQ_FLOW_MAX_RATE,
			  &maxrate, sizeof(maxrate));
	if (set_defrate)
		addattr_l(n, 1024, TCA_FQ_FLOW_DEFAULT_RATE,
			  &defrate, sizeof(defrate));
	tail->rta_len = (void *) NLMSG_TAIL(n) - (void *) tail;
	return 0;
}

static int fq_print_opt(struct qdisc_util *qu, FILE *f, struct rtattr *opt)
{
	struct rtattr *tb[TCA_FQ_MAX + 1];
	unsigned int plimit, flow_plimit;
	unsigned int buckets_log;
	int pacing;
	unsigned int rate, quantum;
	SPRINT_BUF(b1);

	if (opt == NULL)
		return 0;

	parse_rtattr_nested(tb, TCA_FQ_MAX, opt);

	if (tb[TCA_FQ_PLIMIT] &&
	    RTA_PAYLOAD(tb[TCA_FQ_PLIMIT]) >= sizeof(__u32)) {
		plimit = rta_getattr_u32(tb[TCA_FQ_PLIMIT]);
		fprintf(f, "limit %up ", plimit);
	}
	if (tb[TCA_FQ_FLOW_PLIMIT] &&
	    RTA_PAYLOAD(tb[TCA_FQ_FLOW_PLIMIT]) >= sizeof(__u32)) {
		flow_plimit = rta_getattr_u32(tb[TCA_FQ_FLOW_PLIMIT]);
		fprintf(f, "flow_limit %up ", flow_plimit);
	}
	if (tb[TCA_FQ_BUCKETS_LOG] &&
	    RTA_PAYLOAD(tb[TCA_FQ_BUCKETS_LOG]) >= sizeof(__u32)) {
		buckets_log = rta_getattr_u32(tb[TCA_FQ_BUCKETS_LOG]);
		fprintf(f, "buckets %u ", 1U << buckets_log);
	}
	if (tb[TCA_FQ_RATE_ENABLE] &&
	    RTA_PAYLOAD(tb[TCA_FQ_RATE_ENABLE]) >= sizeof(int)) {
		pacing = rta_getattr_u32(tb[TCA_FQ_RATE_ENABLE]);
		if (pacing == 0)
			fprintf(f, "nopacing ");
	}
	if (tb[TCA_FQ_QUANTUM] &&
	    RTA_PAYLOAD(tb[TCA_FQ_QUANTUM]) >= sizeof(__u32)) {
		quantum = rta_getattr_u32(tb[TCA_FQ_QUANTUM]);
		fprintf(f, "quantum %u ", quantum);
	}
	if (tb[TCA_FQ_INITIAL_QUANTUM] &&
	    RTA_PAYLOAD(tb[TCA_FQ_INITIAL_QUANTUM]) >= sizeof(__u32)) {
		quantum = rta_getattr_u32(tb[TCA_FQ_INITIAL_QUANTUM]);
		fprintf(f, "initial_quantum %u ", quantum);
	}
	if (tb[TCA_FQ_FLOW_MAX_RATE] &&
	    RTA_PAYLOAD(tb[TCA_FQ_FLOW_MAX_RATE]) >= sizeof(__u32)) {
		rate = rta_getattr_u32(tb[TCA_FQ_FLOW_MAX_RATE]);

		if (rate != ~0U)
			fprintf(f, "maxrate %s ", sprint_rate(rate, b1));
	}
	if (tb[TCA_FQ_FLOW_DEFAULT_RATE] &&
	    RTA_PAYLOAD(tb[TCA_FQ_FLOW_DEFAULT_RATE]) >= sizeof(__u32)) {
		rate = rta_getattr_u32(tb[TCA_FQ_FLOW_DEFAULT_RATE]);

		if (rate != 0)
			fprintf(f, "defrate %s ", sprint_rate(rate, b1));
	}

	return 0;
}

static int fq_print_xstats(struct qdisc_util *qu, FILE *f,
			   struct rtattr *xstats)
{
	struct tc_fq_qd_stats *st;

	if (xstats == NULL)
		return 0;

	if (RTA_PAYLOAD(xstats) < sizeof(*st))
		return -1;

	st = RTA_DATA(xstats);

	fprintf(f, "  %u flows (%u inactive, %u throttled)",
		st->flows, st->inactive_flows, st->throttled_flows);

	if (st->time_next_delayed_flow > 0)
		fprintf(f, ", next packet delay %llu ns", st->time_next_delayed_flow);

	fprintf(f, "\n  %llu gc, %llu highprio",
		st->gc_flows, st->highprio_packets);

	if (st->tcp_retrans)
		fprintf(f, ", %llu retrans", st->tcp_retrans);

	fprintf(f, ", %llu throttled", st->throttled);

	if (st->flows_plimit)
		fprintf(f, ", %llu flows_plimit", st->flows_plimit);

	if (st->pkts_too_long || st->allocation_errors)
		fprintf(f, "\n  %llu too long pkts, %llu alloc errors\n",
			st->pkts_too_long, st->allocation_errors);

	return 0;
}

struct qdisc_util fq_qdisc_util = {
	.id		= "fq",
	.parse_qopt	= fq_parse_opt,
	.print_qopt	= fq_print_opt,
	.print_xstats	= fq_print_xstats,
};
