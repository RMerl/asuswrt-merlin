/*
 * q_netem.c		NETEM.
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:	Stephen Hemminger <shemminger@linux-foundation.org>
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
#include <errno.h>

#include "utils.h"
#include "tc_util.h"
#include "tc_common.h"

static void explain(void)
{
	fprintf(stderr,
"Usage: ... netem [ limit PACKETS ] \n" \
"                 [ delay TIME [ JITTER [CORRELATION]]]\n" \
"                 [ distribution {uniform|normal|pareto|paretonormal} ]\n" \
"                 [ drop PERCENT [CORRELATION]] \n" \
"                 [ corrupt PERCENT [CORRELATION]] \n" \
"                 [ duplicate PERCENT [CORRELATION]]\n" \
"                 [ reorder PRECENT [CORRELATION] [ gap DISTANCE ]]\n");
}

static void explain1(const char *arg)
{
	fprintf(stderr, "Illegal \"%s\"\n", arg);
}

/* Upper bound on size of distribution 
 *  really (TCA_BUF_MAX - other headers) / sizeof (__s16)
 */
#define MAX_DIST	(16*1024)

/*
 * Simplistic file parser for distrbution data.
 * Format is:
 *	# comment line(s)
 *	data0 data1 ...
 */
static int get_distribution(const char *type, __s16 *data, int maxdata)
{
	FILE *f;
	int n;
	long x;
	size_t len;
	char *line = NULL;
	char name[128];

	snprintf(name, sizeof(name), "%s/%s.dist", get_tc_lib(), type);
	if ((f = fopen(name, "r")) == NULL) {
		fprintf(stderr, "No distribution data for %s (%s: %s)\n",
			type, name, strerror(errno));
		return -1;
	}

	n = 0;
	while (getline(&line, &len, f) != -1) {
		char *p, *endp;
		if (*line == '\n' || *line == '#')
			continue;

		for (p = line; ; p = endp) {
			x = strtol(p, &endp, 0);
			if (endp == p)
				break;

			if (n >= maxdata) {
				fprintf(stderr, "%s: too much data\n",
					name);
				n = -1;
				goto error;
			}
			data[n++] = x;
		}
	}
 error:
	free(line);
	fclose(f);
	return n;
}

static int isnumber(const char *arg)
{
	char *p;

	return strtod(arg, &p) != 0 || p != arg;
}

#define NEXT_IS_NUMBER() (NEXT_ARG_OK() && isnumber(argv[1]))

/* Adjust for the fact that psched_ticks aren't always usecs
   (based on kernel PSCHED_CLOCK configuration */
static int get_ticks(__u32 *ticks, const char *str)
{
	unsigned t;

	if(get_time(&t, str))
		return -1;

	if (tc_core_time2big(t)) {
		fprintf(stderr, "Illegal %u time (too large)\n", t);
		return -1;
	}

	*ticks = tc_core_time2tick(t);
	return 0;
}

static int netem_parse_opt(struct qdisc_util *qu, int argc, char **argv,
			   struct nlmsghdr *n)
{
	size_t dist_size = 0;
	struct rtattr *tail;
	struct tc_netem_qopt opt;
	struct tc_netem_corr cor;
	struct tc_netem_reorder reorder;
	struct tc_netem_corrupt corrupt;
	__s16 *dist_data = NULL;
	int present[__TCA_NETEM_MAX];

	memset(&opt, 0, sizeof(opt));
	opt.limit = 1000;
	memset(&cor, 0, sizeof(cor));
	memset(&reorder, 0, sizeof(reorder));
	memset(&corrupt, 0, sizeof(corrupt));
	memset(present, 0, sizeof(present));

	while (argc > 0) {
		if (matches(*argv, "limit") == 0) {
			NEXT_ARG();
			if (get_size(&opt.limit, *argv)) {
				explain1("limit");
				return -1;
			}
		} else if (matches(*argv, "latency") == 0 ||
			   matches(*argv, "delay") == 0) {
			NEXT_ARG();
			if (get_ticks(&opt.latency, *argv)) {
				explain1("latency");
				return -1;
			}

			if (NEXT_IS_NUMBER()) {
				NEXT_ARG();
				if (get_ticks(&opt.jitter, *argv)) {
					explain1("latency");
					return -1;
				}

				if (NEXT_IS_NUMBER()) {
					NEXT_ARG();
					++present[TCA_NETEM_CORR];
					if (get_percent(&cor.delay_corr,							*argv)) {
						explain1("latency");
						return -1;
					}
				}
			}
		} else if (matches(*argv, "loss") == 0 ||
			   matches(*argv, "drop") == 0) {
			NEXT_ARG();
			if (get_percent(&opt.loss, *argv)) {
				explain1("loss");
				return -1;
			}
			if (NEXT_IS_NUMBER()) {
				NEXT_ARG();
				++present[TCA_NETEM_CORR];
				if (get_percent(&cor.loss_corr, *argv)) {
					explain1("loss");
					return -1;
				}
			}
		} else if (matches(*argv, "reorder") == 0) {
			NEXT_ARG();
			present[TCA_NETEM_REORDER] = 1;
			if (get_percent(&reorder.probability, *argv)) {
				explain1("reorder");
				return -1;
			}
			if (NEXT_IS_NUMBER()) {
				NEXT_ARG();
				++present[TCA_NETEM_CORR];
				if (get_percent(&reorder.correlation, *argv)) {
					explain1("reorder");
					return -1;
				}
			}
		} else if (matches(*argv, "corrupt") == 0) {
			NEXT_ARG();
			present[TCA_NETEM_CORRUPT] = 1;
			if (get_percent(&corrupt.probability, *argv)) {
				explain1("corrupt");
				return -1;
			}
			if (NEXT_IS_NUMBER()) {
				NEXT_ARG();
				++present[TCA_NETEM_CORR];
				if (get_percent(&corrupt.correlation, *argv)) {
					explain1("corrupt");
					return -1;
				}
			}
		} else if (matches(*argv, "gap") == 0) {
			NEXT_ARG();
			if (get_u32(&opt.gap, *argv, 0)) {
				explain1("gap");
				return -1;
			}
		} else if (matches(*argv, "duplicate") == 0) {
			NEXT_ARG();
			if (get_percent(&opt.duplicate, *argv)) {
				explain1("duplicate");
				return -1;
			}
			if (NEXT_IS_NUMBER()) {
				NEXT_ARG();
				if (get_percent(&cor.dup_corr, *argv)) {
					explain1("duplicate");
					return -1;
				}
			}
		} else if (matches(*argv, "distribution") == 0) {
			NEXT_ARG();
			dist_data = calloc(sizeof(dist_data[0]), MAX_DIST);
			dist_size = get_distribution(*argv, dist_data, MAX_DIST);
			if (dist_size <= 0) {
				free(dist_data);
				return -1;
			}
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

	if (reorder.probability) {
		if (opt.latency == 0) {
			fprintf(stderr, "reordering not possible without specifying some delay\n");
		}
		if (opt.gap == 0)
			opt.gap = 1;
	} else if (opt.gap > 0) {
		fprintf(stderr, "gap specified without reorder probability\n");
		explain();
		return -1;
	}

	if (dist_data && (opt.latency == 0 || opt.jitter == 0)) {
		fprintf(stderr, "distribution specified but no latency and jitter values\n");
		explain();
		return -1;
	}

	if (addattr_l(n, 1024, TCA_OPTIONS, &opt, sizeof(opt)) < 0)
		return -1;

	if (present[TCA_NETEM_CORR] &&
	    addattr_l(n, 1024, TCA_NETEM_CORR, &cor, sizeof(cor)) < 0)
			return -1;

	if (present[TCA_NETEM_REORDER] && 
	    addattr_l(n, 1024, TCA_NETEM_REORDER, &reorder, sizeof(reorder)) < 0)
		return -1;

	if (present[TCA_NETEM_CORRUPT] &&
	    addattr_l(n, 1024, TCA_NETEM_CORRUPT, &corrupt, sizeof(corrupt)) < 0)
		return -1;

	if (dist_data) {
		if (addattr_l(n, MAX_DIST * sizeof(dist_data[0]),
			      TCA_NETEM_DELAY_DIST,
			      dist_data, dist_size * sizeof(dist_data[0])) < 0)
			return -1;
		free(dist_data);
	}
	tail->rta_len = (void *) NLMSG_TAIL(n) - (void *) tail;
	return 0;
}

static int netem_print_opt(struct qdisc_util *qu, FILE *f, struct rtattr *opt)
{
	const struct tc_netem_corr *cor = NULL;
	const struct tc_netem_reorder *reorder = NULL;
	const struct tc_netem_corrupt *corrupt = NULL;
	struct tc_netem_qopt qopt;
	int len = RTA_PAYLOAD(opt) - sizeof(qopt);
	SPRINT_BUF(b1);

	if (opt == NULL)
		return 0;

	if (len < 0) {
		fprintf(stderr, "options size error\n");
		return -1;
	}
	memcpy(&qopt, RTA_DATA(opt), sizeof(qopt));

	if (len > 0) {
		struct rtattr *tb[TCA_NETEM_MAX+1];
		parse_rtattr(tb, TCA_NETEM_MAX, RTA_DATA(opt) + sizeof(qopt),
			     len);

		if (tb[TCA_NETEM_CORR]) {
			if (RTA_PAYLOAD(tb[TCA_NETEM_CORR]) < sizeof(*cor))
				return -1;
			cor = RTA_DATA(tb[TCA_NETEM_CORR]);
		}
		if (tb[TCA_NETEM_REORDER]) {
			if (RTA_PAYLOAD(tb[TCA_NETEM_REORDER]) < sizeof(*reorder))
				return -1;
			reorder = RTA_DATA(tb[TCA_NETEM_REORDER]);
		}
		if (tb[TCA_NETEM_CORRUPT]) {
			if (RTA_PAYLOAD(tb[TCA_NETEM_CORRUPT]) < sizeof(*corrupt))
				return -1;
			corrupt = RTA_DATA(tb[TCA_NETEM_CORRUPT]);
		}
	}

	fprintf(f, "limit %d", qopt.limit);

	if (qopt.latency) {
		fprintf(f, " delay %s", sprint_ticks(qopt.latency, b1));

		if (qopt.jitter) {
			fprintf(f, "  %s", sprint_ticks(qopt.jitter, b1));
			if (cor && cor->delay_corr)
				fprintf(f, " %s", sprint_percent(cor->delay_corr, b1));
		}
	}

	if (qopt.loss) {
		fprintf(f, " loss %s", sprint_percent(qopt.loss, b1));
		if (cor && cor->loss_corr)
			fprintf(f, " %s", sprint_percent(cor->loss_corr, b1));
	}

	if (qopt.duplicate) {
		fprintf(f, " duplicate %s",
			sprint_percent(qopt.duplicate, b1));
		if (cor && cor->dup_corr)
			fprintf(f, " %s", sprint_percent(cor->dup_corr, b1));
	}

	if (reorder && reorder->probability) {
		fprintf(f, " reorder %s",
			sprint_percent(reorder->probability, b1));
		if (reorder->correlation)
			fprintf(f, " %s",
				sprint_percent(reorder->correlation, b1));
	}

	if (corrupt && corrupt->probability) {
		fprintf(f, " corrupt %s",
			sprint_percent(corrupt->probability, b1));
		if (corrupt->correlation)
			fprintf(f, " %s",
				sprint_percent(corrupt->correlation, b1));
	}

	if (qopt.gap)
		fprintf(f, " gap %lu", (unsigned long)qopt.gap);

	return 0;
}

struct qdisc_util netem_qdisc_util = {
	.id	   	= "netem",
	.parse_qopt	= netem_parse_opt,
	.print_qopt	= netem_print_opt,
};

