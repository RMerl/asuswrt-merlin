/*
 * q_multiq.c		Multiqueue aware qdisc
 *
 * Copyright (c) 2008, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place - Suite 330, Boston, MA 02111-1307 USA.
 *
 * Author: Alexander Duyck <alexander.h.duyck@intel.com>
 *
 * Original Authors:	PJ Waskiewicz, <peter.p.waskiewicz.jr@intel.com> (RR)
 * 			Alexey Kuznetsov, <kuznet@ms2.inr.ac.ru> (from PRIO)
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
	fprintf(stderr, "Usage: ... multiq [help]\n");
}

static int multiq_parse_opt(struct qdisc_util *qu, int argc, char **argv,
			    struct nlmsghdr *n)
{
	struct tc_multiq_qopt opt;

	if (argc) {
		if (strcmp(*argv, "help") == 0) {
			explain();
			return -1;
		} else {
			fprintf(stderr, "What is \"%s\"?\n", *argv);
			explain();
			return -1;
		}
	}

	addattr_l(n, 1024, TCA_OPTIONS, &opt, sizeof(opt));
	return 0;
}

int multiq_print_opt(struct qdisc_util *qu, FILE *f, struct rtattr *opt)
{
	struct tc_multiq_qopt *qopt;

	if (opt == NULL)
		return 0;
	if (RTA_PAYLOAD(opt) < sizeof(*qopt))
		return 0;

	qopt = RTA_DATA(opt);

	fprintf(f, "bands %u/%u ", qopt->bands, qopt->max_bands);

	return 0;
}

struct qdisc_util multiq_qdisc_util = {
	.id	 	= "multiq",
	.parse_qopt	= multiq_parse_opt,
	.print_qopt	= multiq_print_opt,
};
