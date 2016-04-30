/*
 * brmonitor.c		"bridge monitor"
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:	Stephen Hemminger <shemminger@vyatta.com>
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <net/if.h>
#include <netinet/in.h>
#include <linux/if_bridge.h>
#include <linux/neighbour.h>
#include <string.h>

#include "utils.h"
#include "br_common.h"


static void usage(void) __attribute__((noreturn));
int prefix_banner;

static void usage(void)
{
	fprintf(stderr, "Usage: bridge monitor [file | link | fdb | mdb | all]\n");
	exit(-1);
}

static int accept_msg(const struct sockaddr_nl *who,
		      struct nlmsghdr *n, void *arg)
{
	FILE *fp = arg;

	if (timestamp)
		print_timestamp(fp);

	switch (n->nlmsg_type) {
	case RTM_NEWLINK:
	case RTM_DELLINK:
		if (prefix_banner)
			fprintf(fp, "[LINK]");

		return print_linkinfo(who, n, arg);

	case RTM_NEWNEIGH:
	case RTM_DELNEIGH:
		if (prefix_banner)
			fprintf(fp, "[NEIGH]");
		return print_fdb(who, n, arg);

	case RTM_NEWMDB:
	case RTM_DELMDB:
		if (prefix_banner)
			fprintf(fp, "[MDB]");
		return print_mdb(who, n, arg);

	case NLMSG_TSTAMP:
		print_nlmsg_timestamp(fp, n);
		return 0;

	default:
		return 0;
	}
}

int do_monitor(int argc, char **argv)
{
	char *file = NULL;
	unsigned groups = ~RTMGRP_TC;
	int llink=0;
	int lneigh=0;
	int lmdb=0;

	rtnl_close(&rth);

	while (argc > 0) {
		if (matches(*argv, "file") == 0) {
			NEXT_ARG();
			file = *argv;
		} else if (matches(*argv, "link") == 0) {
			llink=1;
			groups = 0;
		} else if (matches(*argv, "fdb") == 0) {
			lneigh = 1;
			groups = 0;
		} else if (matches(*argv, "mdb") == 0) {
			lmdb = 1;
			groups = 0;
		} else if (strcmp(*argv, "all") == 0) {
			groups = ~RTMGRP_TC;
			prefix_banner=1;
		} else if (matches(*argv, "help") == 0) {
			usage();
		} else {
			fprintf(stderr, "Argument \"%s\" is unknown, try \"bridge monitor help\".\n", *argv);
			exit(-1);
		}
		argc--;	argv++;
	}

	if (llink)
		groups |= nl_mgrp(RTNLGRP_LINK);

	if (lneigh) {
		groups |= nl_mgrp(RTNLGRP_NEIGH);
	}

	if (lmdb) {
		groups |= nl_mgrp(RTNLGRP_MDB);
	}

	if (file) {
		FILE *fp;
		int err;
		fp = fopen(file, "r");
		if (fp == NULL) {
			perror("Cannot fopen");
			exit(-1);
		}
		err = rtnl_from_file(fp, accept_msg, stdout);
		fclose(fp);
		return err;
	}

	if (rtnl_open(&rth, groups) < 0)
		exit(1);
	ll_init_map(&rth);

	if (rtnl_listen(&rth, accept_msg, stdout) < 0)
		exit(2);

	return 0;
}

