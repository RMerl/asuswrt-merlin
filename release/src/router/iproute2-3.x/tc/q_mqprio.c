/*
 * q_mqprio.c	MQ prio qdisc
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Author:	John Fastabend, <john.r.fastabend@intel.com>
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
	fprintf(stderr, "Usage: ... mqprio [num_tc NUMBER] [map P0 P1 ...]\n");
	fprintf(stderr, "                  [queues count1@offset1 count2@offset2 ...] ");
	fprintf(stderr, "[hw 1|0]\n");
}

static int mqprio_parse_opt(struct qdisc_util *qu, int argc,
			    char **argv, struct nlmsghdr *n)
{
	int idx;
	struct tc_mqprio_qopt opt = {
				     8,
				     {0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 1, 1, 3, 3, 3, 3},
				     1,
				    };

	while (argc > 0) {
		idx = 0;
		if (strcmp(*argv, "num_tc") == 0) {
			NEXT_ARG();
			if (get_u8(&opt.num_tc, *argv, 10)) {
				fprintf(stderr, "Illegal \"num_tc\"\n");
				return -1;
			}
		} else if (strcmp(*argv, "map") == 0) {
			while (idx < TC_QOPT_MAX_QUEUE && NEXT_ARG_OK()) {
				NEXT_ARG();
				if (get_u8(&opt.prio_tc_map[idx], *argv, 10)) {
					PREV_ARG();
					break;
				}
				idx++;
			}
			for ( ; idx < TC_QOPT_MAX_QUEUE; idx++)
				opt.prio_tc_map[idx] = 0;
		} else if (strcmp(*argv, "queues") == 0) {
			char *tmp, *tok;

			while (idx < TC_QOPT_MAX_QUEUE && NEXT_ARG_OK()) {
				NEXT_ARG();

				tmp = strdup(*argv);
				if (!tmp)
					break;

				tok = strtok(tmp, "@");
				if (get_u16(&opt.count[idx], tok, 10)) {
					free(tmp);
					PREV_ARG();
					break;
				}
				tok = strtok(NULL, "@");
				if (get_u16(&opt.offset[idx], tok, 10)) {
					free(tmp);
					PREV_ARG();
					break;
				}
				free(tmp);
				idx++;
			}
		} else if (strcmp(*argv, "hw") == 0) {
			NEXT_ARG();
			if (get_u8(&opt.hw, *argv, 10)) {
				fprintf(stderr, "Illegal \"hw\"\n");
				return -1;
			}
			idx++;
		} else if (strcmp(*argv, "help") == 0) {
			explain();
			return -1;
		} else {
			fprintf(stderr, "Unknown argument\n");
			return -1;
		}
		argc--; argv++;
	}

	addattr_l(n, 1024, TCA_OPTIONS, &opt, sizeof(opt));
	return 0;
}

int mqprio_print_opt(struct qdisc_util *qu, FILE *f, struct rtattr *opt)
{
	int i;
	struct tc_mqprio_qopt *qopt;

	if (opt == NULL)
		return 0;

	qopt = RTA_DATA(opt);

	fprintf(f, " tc %u map ", qopt->num_tc);
	for (i = 0; i <= TC_PRIO_MAX; i++)
		fprintf(f, "%d ", qopt->prio_tc_map[i]);
	fprintf(f, "\n             queues:");
	for (i = 0; i < qopt->num_tc; i++)
		fprintf(f, "(%i:%i) ", qopt->offset[i],
			qopt->offset[i] + qopt->count[i] - 1);
	return 0;
}

struct qdisc_util mqprio_qdisc_util = {
	.id		= "mqprio",
	.parse_qopt	= mqprio_parse_opt,
	.print_qopt	= mqprio_print_opt,
};
