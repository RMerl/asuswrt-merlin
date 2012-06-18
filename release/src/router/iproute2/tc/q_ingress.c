/*
 *
 * q_ingress.c             INGRESS.
 *
 *              This program is free software; you can redistribute it and/or
 *              modify it under the terms of the GNU General Public License
 *              as published by the Free Software Foundation; either version
 *              2 of the License, or (at your option) any later version.
 *
 * Authors:    J Hadi Salim
 *
 * This is here just in case it is needed
 * useless right now; might be useful in the future
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
	fprintf(stderr, "Usage: ... ingress \n");
}

#define usage() return(-1)

static int ingress_parse_opt(struct qdisc_util *qu, int argc, char **argv, struct nlmsghdr *n)
{

	if (argc > 0) {
		while (argc > 0) {

			if (strcmp(*argv, "handle") == 0) {
				NEXT_ARG();
				argc--; argv++;
			} else {
				fprintf(stderr, "What is \"%s\"?\n", *argv);
				explain();
				return -1;
			}
		}
	}

	addattr_l(n, 1024, TCA_OPTIONS, NULL, 0);
	return 0;
}

static int ingress_print_opt(struct qdisc_util *qu, FILE *f, struct rtattr *opt)
{

		fprintf(f, "---------------- ");
	return 0;
}

struct qdisc_util ingress_qdisc_util = {
	.id		= "ingress",
	.parse_qopt	= ingress_parse_opt,
	.print_qopt	= ingress_print_opt,
};
