/*
 * rtmon.c		RTnetlink listener.
 *
 *		This program is free software; you can redistribute it and/or
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
#include <sys/time.h>
#include <netinet/in.h>
#include <string.h>

#include "SNAPSHOT.h"

#include "utils.h"
#include "libnetlink.h"

int resolve_hosts = 0;
static int init_phase = 1;

static void write_stamp(FILE *fp)
{
	char buf[128];
	struct nlmsghdr *n1 = (void*)buf;
	struct timeval tv;

	n1->nlmsg_type = 15;
	n1->nlmsg_flags = 0;
	n1->nlmsg_seq = 0;
	n1->nlmsg_pid = 0;
	n1->nlmsg_len = NLMSG_LENGTH(4*2);
	gettimeofday(&tv, NULL);
	((__u32*)NLMSG_DATA(n1))[0] = tv.tv_sec;
	((__u32*)NLMSG_DATA(n1))[1] = tv.tv_usec;
	fwrite((void*)n1, 1, NLMSG_ALIGN(n1->nlmsg_len), fp);
}

static int dump_msg(const struct sockaddr_nl *who, struct nlmsghdr *n,
		    void *arg)
{
	FILE *fp = (FILE*)arg;
	if (!init_phase)
		write_stamp(fp);
	fwrite((void*)n, 1, NLMSG_ALIGN(n->nlmsg_len), fp);
	fflush(fp);
	return 0;
}

void usage(void)
{
	fprintf(stderr, "Usage: rtmon file FILE [ all | LISTofOBJECTS]\n");
	fprintf(stderr, "LISTofOBJECTS := [ link ] [ address ] [ route ]\n");
	exit(-1);
}

int
main(int argc, char **argv)
{
	FILE *fp;
	struct rtnl_handle rth;
	int family = AF_UNSPEC;
	unsigned groups = ~0U;
	int llink = 0;
	int laddr = 0;
	int lroute = 0;
	char *file = NULL;

	while (argc > 1) {
		if (matches(argv[1], "-family") == 0) {
			argc--;
			argv++;
			if (argc <= 1)
				usage();
			if (strcmp(argv[1], "inet") == 0)
				family = AF_INET;
			else if (strcmp(argv[1], "inet6") == 0)
				family = AF_INET6;
			else if (strcmp(argv[1], "link") == 0)
				family = AF_INET6;
			else if (strcmp(argv[1], "help") == 0)
				usage();
			else {
				fprintf(stderr, "Protocol ID \"%s\" is unknown, try \"rtmon help\".\n", argv[1]);
				exit(-1);
			}
		} else if (strcmp(argv[1], "-4") == 0) {
			family = AF_INET;
		} else if (strcmp(argv[1], "-6") == 0) {
			family = AF_INET6;
		} else if (strcmp(argv[1], "-0") == 0) {
			family = AF_PACKET;
		} else if (matches(argv[1], "-Version") == 0) {
			printf("rtmon utility, iproute2-ss%s\n", SNAPSHOT);
			exit(0);
		} else if (matches(argv[1], "file") == 0) {
			argc--;
			argv++;
			if (argc <= 1)
				usage();
			file = argv[1];
		} else if (matches(argv[1], "link") == 0) {
			llink=1;
			groups = 0;
		} else if (matches(argv[1], "address") == 0) {
			laddr=1;
			groups = 0;
		} else if (matches(argv[1], "route") == 0) {
			lroute=1;
			groups = 0;
		} else if (strcmp(argv[1], "all") == 0) {
			groups = ~0U;
		} else if (matches(argv[1], "help") == 0) {
			usage();
		} else {
			fprintf(stderr, "Argument \"%s\" is unknown, try \"rtmon help\".\n", argv[1]);
			exit(-1);
		}
		argc--;	argv++;
	}

	if (file == NULL) {
		fprintf(stderr, "Not enough information: argument \"file\" is required\n");
		exit(-1);
	}
	if (llink)
		groups |= nl_mgrp(RTNLGRP_LINK);
	if (laddr) {
		if (!family || family == AF_INET)
			groups |= nl_mgrp(RTNLGRP_IPV4_IFADDR);
		if (!family || family == AF_INET6)
			groups |= nl_mgrp(RTNLGRP_IPV6_IFADDR);
	}
	if (lroute) {
		if (!family || family == AF_INET)
			groups |= nl_mgrp(RTNLGRP_IPV4_ROUTE);
		if (!family || family == AF_INET6)
			groups |= nl_mgrp(RTNLGRP_IPV6_ROUTE);
	}

	fp = fopen(file, "w");
	if (fp == NULL) {
		perror("Cannot fopen");
		exit(-1);
	}

	if (rtnl_open(&rth, groups) < 0)
		exit(1);

	if (rtnl_wilddump_request(&rth, AF_UNSPEC, RTM_GETLINK) < 0) {
		perror("Cannot send dump request");
		exit(1);
	}

	write_stamp(fp);

	if (rtnl_dump_filter(&rth, dump_msg, fp, NULL, NULL) < 0) {
		fprintf(stderr, "Dump terminated\n");
		return 1;
	}

	init_phase = 0;

	if (rtnl_listen(&rth, dump_msg, (void*)fp) < 0)
		exit(2);

	exit(0);
}
