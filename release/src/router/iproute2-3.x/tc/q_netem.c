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
#include <math.h>
#include <ctype.h>
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
"                 [ corrupt PERCENT [CORRELATION]] \n" \
"                 [ duplicate PERCENT [CORRELATION]]\n" \
"                 [ loss random PERCENT [CORRELATION]]\n" \
"                 [ loss state P13 [P31 [P32 [P23 P14]]]\n" \
"                 [ loss gemodel PERCENT [R [1-H [1-K]]]\n" \
"                 [ ecn ]\n" \
"                 [ reorder PRECENT [CORRELATION] [ gap DISTANCE ]]\n" \
"                 [ rate RATE [PACKETOVERHEAD] [CELLSIZE] [CELLOVERHEAD]]\n");
}

static void explain1(const char *arg)
{
	fprintf(stderr, "Illegal \"%s\"\n", arg);
}

/* Upper bound on size of distribution
 *  really (TCA_BUF_MAX - other headers) / sizeof (__s16)
 */
#define MAX_DIST	(16*1024)

static const double max_percent_value = 0xffffffff;

/* scaled value used to percent of maximum. */
static void set_percent(__u32 *percent, double per)
{
	*percent = (unsigned) rint(per * max_percent_value);
}


/* Parse either a fraction '.3' or percent '30%
 * return: 0 = ok, -1 = error, 1 = out of range
 */
static int parse_percent(double *val, const char *str)
{
	char *p;

	*val = strtod(str, &p) / 100.;
	if (*p && strcmp(p, "%") )
		return -1;

	return 0;
}

static int get_percent(__u32 *percent, const char *str)
{
	double per;

	if (parse_percent(&per, str))
		return -1;

	set_percent(percent, per);
	return 0;
}

static void print_percent(char *buf, int len, __u32 per)
{
	snprintf(buf, len, "%g%%", 100. * (double) per / max_percent_value);
}

static char * sprint_percent(__u32 per, char *buf)
{
	print_percent(buf, SPRINT_BSIZE-1, per);
	return buf;
}

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

#define NEXT_IS_NUMBER() (NEXT_ARG_OK() && isdigit(argv[1][0]))
#define NEXT_IS_SIGNED_NUMBER() \
	(NEXT_ARG_OK() && (isdigit(argv[1][0]) || argv[1][0] == '-'))

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
	int dist_size = 0;
	struct rtattr *tail;
	struct tc_netem_qopt opt = { .limit = 1000 };
	struct tc_netem_corr cor;
	struct tc_netem_reorder reorder;
	struct tc_netem_corrupt corrupt;
	struct tc_netem_gimodel gimodel;
	struct tc_netem_gemodel gemodel;
	struct tc_netem_rate rate;
	__s16 *dist_data = NULL;
	__u16 loss_type = NETEM_LOSS_UNSPEC;
	int present[__TCA_NETEM_MAX];
	__u64 rate64 = 0;

	memset(&cor, 0, sizeof(cor));
	memset(&reorder, 0, sizeof(reorder));
	memset(&corrupt, 0, sizeof(corrupt));
	memset(&rate, 0, sizeof(rate));
	memset(present, 0, sizeof(present));

	for( ; argc > 0; --argc, ++argv) {
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
					if (get_percent(&cor.delay_corr, *argv)) {
						explain1("latency");
						return -1;
					}
				}
			}
		} else if (matches(*argv, "loss") == 0 ||
			   matches(*argv, "drop") == 0) {
			if (opt.loss > 0 || loss_type != NETEM_LOSS_UNSPEC) {
				explain1("duplicate loss argument\n");
				return -1;
			}

			NEXT_ARG();
			/* Old (deprecated) random loss model syntax */
			if (isdigit(argv[0][0]))
				goto random_loss_model;

			if (!strcmp(*argv, "random")) {
				NEXT_ARG();
	random_loss_model:
				if (get_percent(&opt.loss, *argv)) {
					explain1("loss percent");
					return -1;
				}
				if (NEXT_IS_NUMBER()) {
					NEXT_ARG();
					++present[TCA_NETEM_CORR];
					if (get_percent(&cor.loss_corr, *argv)) {
						explain1("loss correllation");
						return -1;
					}
				}
			} else if (!strcmp(*argv, "state")) {
				double p13;

				NEXT_ARG();
				if (parse_percent(&p13, *argv)) {
					explain1("loss p13");
					return -1;
				}

				/* set defaults */
				set_percent(&gimodel.p13, p13);
				set_percent(&gimodel.p31, 1. - p13);
				set_percent(&gimodel.p32, 0);
				set_percent(&gimodel.p23, 1.);
				set_percent(&gimodel.p14, 0);
				loss_type = NETEM_LOSS_GI;

				if (!NEXT_IS_NUMBER())
					continue;
				NEXT_ARG();
				if (get_percent(&gimodel.p31, *argv)) {
					explain1("loss p31");
					return -1;
				}

				if (!NEXT_IS_NUMBER())
					continue;
				NEXT_ARG();
				if (get_percent(&gimodel.p32, *argv)) {
					explain1("loss p32");
					return -1;
				}

				if (!NEXT_IS_NUMBER())
					continue;
				NEXT_ARG();
				if (get_percent(&gimodel.p23, *argv)) {
					explain1("loss p23");
					return -1;
				}
				if (!NEXT_IS_NUMBER())
					continue;
				NEXT_ARG();
				if (get_percent(&gimodel.p14, *argv)) {
					explain1("loss p14");
					return -1;
				}

			} else if (!strcmp(*argv, "gemodel")) {
				NEXT_ARG();
				if (get_percent(&gemodel.p, *argv)) {
					explain1("loss gemodel p");
					return -1;
				}

				/* set defaults */
				set_percent(&gemodel.r, 1.);
				set_percent(&gemodel.h, 0);
				set_percent(&gemodel.k1, 0);
				loss_type = NETEM_LOSS_GE;

				if (!NEXT_IS_NUMBER())
					continue;
				NEXT_ARG();
				if (get_percent(&gemodel.r, *argv)) {
					explain1("loss gemodel r");
					return -1;
				}

				if (!NEXT_IS_NUMBER())
					continue;
				NEXT_ARG();
				if (get_percent(&gemodel.h, *argv)) {
					explain1("loss gemodel h");
					return -1;
				}
				/* netem option is "1-h" but kernel
				 * expects "h".
				 */
				gemodel.h = max_percent_value - gemodel.h;

				if (!NEXT_IS_NUMBER())
					continue;
				NEXT_ARG();
				if (get_percent(&gemodel.k1, *argv)) {
					explain1("loss gemodel k");
					return -1;
				}
			} else {
				fprintf(stderr, "Unknown loss parameter: %s\n",
					*argv);
				return -1;
			}
		} else if (matches(*argv, "ecn") == 0) {
				present[TCA_NETEM_ECN] = 1;
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
		} else if (matches(*argv, "rate") == 0) {
			++present[TCA_NETEM_RATE];
			NEXT_ARG();
			if (get_rate64(&rate64, *argv)) {
				explain1("rate");
				return -1;
			}
			if (NEXT_IS_SIGNED_NUMBER()) {
				NEXT_ARG();
				if (get_s32(&rate.packet_overhead, *argv, 0)) {
					explain1("rate");
					return -1;
				}
			}
			if (NEXT_IS_NUMBER()) {
				NEXT_ARG();
				if (get_u32(&rate.cell_size, *argv, 0)) {
					explain1("rate");
					return -1;
				}
			}
			if (NEXT_IS_SIGNED_NUMBER()) {
				NEXT_ARG();
				if (get_s32(&rate.cell_overhead, *argv, 0)) {
					explain1("rate");
					return -1;
				}
			}
		} else if (strcmp(*argv, "help") == 0) {
			explain();
			return -1;
		} else {
			fprintf(stderr, "What is \"%s\"?\n", *argv);
			explain();
			return -1;
		}
	}

	tail = NLMSG_TAIL(n);

	if (reorder.probability) {
		if (opt.latency == 0) {
			fprintf(stderr, "reordering not possible without specifying some delay\n");
			explain();
			return -1;
		}
		if (opt.gap == 0)
			opt.gap = 1;
	} else if (opt.gap > 0) {
		fprintf(stderr, "gap specified without reorder probability\n");
		explain();
		return -1;
	}

	if (present[TCA_NETEM_ECN]) {
		if (opt.loss <= 0 && loss_type == NETEM_LOSS_UNSPEC) {
			fprintf(stderr, "ecn requested without loss model\n");
			explain();
			return -1;
		}
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

	if (present[TCA_NETEM_ECN] &&
	    addattr_l(n, 1024, TCA_NETEM_ECN, &present[TCA_NETEM_ECN],
		      sizeof(present[TCA_NETEM_ECN])) < 0)
			return -1;

	if (present[TCA_NETEM_CORRUPT] &&
	    addattr_l(n, 1024, TCA_NETEM_CORRUPT, &corrupt, sizeof(corrupt)) < 0)
		return -1;

	if (loss_type != NETEM_LOSS_UNSPEC) {
		struct rtattr *start;

		start = addattr_nest(n, 1024, TCA_NETEM_LOSS | NLA_F_NESTED);
		if (loss_type == NETEM_LOSS_GI) {
			if (addattr_l(n, 1024, NETEM_LOSS_GI,
				      &gimodel, sizeof(gimodel)) < 0)
			    return -1;
		} else if (loss_type == NETEM_LOSS_GE) {
			if (addattr_l(n, 1024, NETEM_LOSS_GE,
				      &gemodel, sizeof(gemodel)) < 0)
			    return -1;
		} else {
			fprintf(stderr, "loss in the weeds!\n");
			return -1;
		}

		addattr_nest_end(n, start);
	}

	if (present[TCA_NETEM_RATE]) {
		if (rate64 >= (1ULL << 32)) {
			if (addattr_l(n, 1024,
				      TCA_NETEM_RATE64, &rate64, sizeof(rate64)) < 0)
				return -1;
			rate.rate = ~0U;
		} else {
			rate.rate = rate64;
		}
		if (addattr_l(n, 1024, TCA_NETEM_RATE, &rate, sizeof(rate)) < 0)
			return -1;
	}

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
	const struct tc_netem_gimodel *gimodel = NULL;
	const struct tc_netem_gemodel *gemodel = NULL;
	int *ecn = NULL;
	struct tc_netem_qopt qopt;
	const struct tc_netem_rate *rate = NULL;
	int len = RTA_PAYLOAD(opt) - sizeof(qopt);
	__u64 rate64 = 0;
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
		if (tb[TCA_NETEM_LOSS]) {
			struct rtattr *lb[NETEM_LOSS_MAX + 1];

			parse_rtattr_nested(lb, NETEM_LOSS_MAX, tb[TCA_NETEM_LOSS]);
			if (lb[NETEM_LOSS_GI])
				gimodel = RTA_DATA(lb[NETEM_LOSS_GI]);
			if (lb[NETEM_LOSS_GE])
				gemodel = RTA_DATA(lb[NETEM_LOSS_GE]);
		}
		if (tb[TCA_NETEM_RATE]) {
			if (RTA_PAYLOAD(tb[TCA_NETEM_RATE]) < sizeof(*rate))
				return -1;
			rate = RTA_DATA(tb[TCA_NETEM_RATE]);
		}
		if (tb[TCA_NETEM_ECN]) {
			if (RTA_PAYLOAD(tb[TCA_NETEM_ECN]) < sizeof(*ecn))
				return -1;
			ecn = RTA_DATA(tb[TCA_NETEM_ECN]);
		}
		if (tb[TCA_NETEM_RATE64]) {
			if (RTA_PAYLOAD(tb[TCA_NETEM_RATE64]) < sizeof(rate64))
				return -1;
			rate64 = rta_getattr_u64(tb[TCA_NETEM_RATE64]);
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

	if (gimodel) {
		fprintf(f, " loss state p13 %s", sprint_percent(gimodel->p13, b1));
		fprintf(f, " p31 %s", sprint_percent(gimodel->p31, b1));
		fprintf(f, " p32 %s", sprint_percent(gimodel->p32, b1));
		fprintf(f, " p23 %s", sprint_percent(gimodel->p23, b1));
		fprintf(f, " p14 %s", sprint_percent(gimodel->p14, b1));
	}

	if (gemodel) {
		fprintf(f, " loss gemodel p %s",
			sprint_percent(gemodel->p, b1));
		fprintf(f, " r %s", sprint_percent(gemodel->r, b1));
		fprintf(f, " 1-h %s", sprint_percent(max_percent_value -
						     gemodel->h, b1));
		fprintf(f, " 1-k %s", sprint_percent(gemodel->k1, b1));
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

	if (rate && rate->rate) {
		if (rate64)
			fprintf(f, " rate %s", sprint_rate(rate64, b1));
		else
			fprintf(f, " rate %s", sprint_rate(rate->rate, b1));
		if (rate->packet_overhead)
			fprintf(f, " packetoverhead %d", rate->packet_overhead);
		if (rate->cell_size)
			fprintf(f, " cellsize %u", rate->cell_size);
		if (rate->cell_overhead)
			fprintf(f, " celloverhead %d", rate->cell_overhead);
	}

	if (ecn)
		fprintf(f, " ecn ");

	if (qopt.gap)
		fprintf(f, " gap %lu", (unsigned long)qopt.gap);


	return 0;
}

struct qdisc_util netem_qdisc_util = {
	.id	   	= "netem",
	.parse_qopt	= netem_parse_opt,
	.print_qopt	= netem_print_opt,
};

