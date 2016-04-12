/*
 * m_estimator.c	Parse/print estimator module options.
 *
 *		This program is free software; you can u32istribute it and/or
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

#include "utils.h"
#include "tc_util.h"
#include "tc_common.h"

static void est_help(void);

static void est_help(void)
{
	fprintf(stderr, "Usage: ... estimator INTERVAL TIME-CONST\n");
	fprintf(stderr, "  INTERVAL is interval between measurements\n");
	fprintf(stderr, "  TIME-CONST is averaging time constant\n");
	fprintf(stderr, "Example: ... est 1sec 8sec\n");
}

int parse_estimator(int *p_argc, char ***p_argv, struct tc_estimator *est)
{
	int argc = *p_argc;
	char **argv = *p_argv;
	unsigned A, time_const;

	NEXT_ARG();
	if (est->ewma_log)
		duparg("estimator", *argv);
	if (matches(*argv, "help") == 0)
		est_help();
	if (get_time(&A, *argv))
		invarg("estimator", "invalid estimator interval");
	NEXT_ARG();
	if (matches(*argv, "help") == 0)
		est_help();
	if (get_time(&time_const, *argv))
		invarg("estimator", "invalid estimator time constant");
	if (tc_setup_estimator(A, time_const, est) < 0) {
		fprintf(stderr, "Error: estimator parameters are out of range.\n");
		return -1;
	}
	if (show_raw)
		fprintf(stderr, "[estimator i=%u e=%u]\n", est->interval, est->ewma_log);
	*p_argc = argc;
	*p_argv = argv;
	return 0;
}
