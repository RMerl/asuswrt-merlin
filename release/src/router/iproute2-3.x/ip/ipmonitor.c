/*
 * ipmonitor.c		"ip monitor".
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
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <time.h>

#include "utils.h"
#include "ip_common.h"

static void usage(void) __attribute__((noreturn));
int prefix_banner;

static void usage(void)
{
	fprintf(stderr, "Usage: ip monitor [ all | LISTofOBJECTS ]\n");
	exit(-1);
}


int accept_msg(const struct sockaddr_nl *who,
	       struct nlmsghdr *n, void *arg)
{
	FILE *fp = (FILE*)arg;

	if (timestamp)
		print_timestamp(fp);

	if (n->nlmsg_type == RTM_NEWROUTE || n->nlmsg_type == RTM_DELROUTE) {
		if (prefix_banner)
			fprintf(fp, "[ROUTE]");
		print_route(who, n, arg);
		return 0;
	}
	if (n->nlmsg_type == RTM_NEWLINK || n->nlmsg_type == RTM_DELLINK) {
		ll_remember_index(who, n, NULL);
		if (prefix_banner)
			fprintf(fp, "[LINK]");
		print_linkinfo(who, n, arg);
		return 0;
	}
	if (n->nlmsg_type == RTM_NEWADDR || n->nlmsg_type == RTM_DELADDR) {
		if (prefix_banner)
			fprintf(fp, "[ADDR]");
		print_addrinfo(who, n, arg);
		return 0;
	}
	if (n->nlmsg_type == RTM_NEWADDRLABEL || n->nlmsg_type == RTM_DELADDRLABEL) {
		if (prefix_banner)
			fprintf(fp, "[ADDRLABEL]");
		print_addrlabel(who, n, arg);
		return 0;
	}
	if (n->nlmsg_type == RTM_NEWNEIGH || n->nlmsg_type == RTM_DELNEIGH) {
		if (prefix_banner)
			fprintf(fp, "[NEIGH]");
		print_neigh(who, n, arg);
		return 0;
	}
	if (n->nlmsg_type == RTM_NEWPREFIX) {
		if (prefix_banner)
			fprintf(fp, "[PREFIX]");
		print_prefix(who, n, arg);
		return 0;
	}
	if (n->nlmsg_type == RTM_NEWRULE || n->nlmsg_type == RTM_DELRULE) {
		if (prefix_banner)
			fprintf(fp, "[RULE]");
		print_rule(who, n, arg);
		return 0;
	}
	if (n->nlmsg_type == 15) {
		char *tstr;
		time_t secs = ((__u32*)NLMSG_DATA(n))[0];
		long usecs = ((__u32*)NLMSG_DATA(n))[1];
		tstr = asctime(localtime(&secs));
		tstr[strlen(tstr)-1] = 0;
		fprintf(fp, "Timestamp: %s %lu us\n", tstr, usecs);
		return 0;
	}
	if (n->nlmsg_type == RTM_NEWQDISC ||
	    n->nlmsg_type == RTM_DELQDISC ||
	    n->nlmsg_type == RTM_NEWTCLASS ||
	    n->nlmsg_type == RTM_DELTCLASS ||
	    n->nlmsg_type == RTM_NEWTFILTER ||
	    n->nlmsg_type == RTM_DELTFILTER)
		return 0;
	if (n->nlmsg_type != NLMSG_ERROR && n->nlmsg_type != NLMSG_NOOP &&
	    n->nlmsg_type != NLMSG_DONE) {
		fprintf(fp, "Unknown message: %08x %08x %08x\n",
			n->nlmsg_len, n->nlmsg_type, n->nlmsg_flags);
	}
	return 0;
}

int do_ipmonitor(int argc, char **argv)
{
	char *file = NULL;
	unsigned groups = ~RTMGRP_TC;
	int llink=0;
	int laddr=0;
	int lroute=0;
	int lprefix=0;
	int lneigh=0;

	rtnl_close(&rth);
	ipaddr_reset_filter(1);
	iproute_reset_filter();
	ipneigh_reset_filter();

	while (argc > 0) {
		if (matches(*argv, "file") == 0) {
			NEXT_ARG();
			file = *argv;
		} else if (matches(*argv, "link") == 0) {
			llink=1;
			groups = 0;
		} else if (matches(*argv, "address") == 0) {
			laddr=1;
			groups = 0;
		} else if (matches(*argv, "route") == 0) {
			lroute=1;
			groups = 0;
		} else if (matches(*argv, "prefix") == 0) {
			lprefix=1;
			groups = 0;
		} else if (matches(*argv, "neigh") == 0) {
			lneigh = 1;
			groups = 0;
		} else if (strcmp(*argv, "all") == 0) {
			groups = ~RTMGRP_TC;
			prefix_banner=1;
		} else if (matches(*argv, "help") == 0) {
			usage();
		} else {
			fprintf(stderr, "Argument \"%s\" is unknown, try \"ip monitor help\".\n", *argv);
			exit(-1);
		}
		argc--;	argv++;
	}

	if (llink)
		groups |= nl_mgrp(RTNLGRP_LINK);
	if (laddr) {
		if (!preferred_family || preferred_family == AF_INET)
			groups |= nl_mgrp(RTNLGRP_IPV4_IFADDR);
		if (!preferred_family || preferred_family == AF_INET6)
			groups |= nl_mgrp(RTNLGRP_IPV6_IFADDR);
	}
	if (lroute) {
		if (!preferred_family || preferred_family == AF_INET)
			groups |= nl_mgrp(RTNLGRP_IPV4_ROUTE);
		if (!preferred_family || preferred_family == AF_INET6)
			groups |= nl_mgrp(RTNLGRP_IPV6_ROUTE);
	}
	if (lprefix) {
		if (!preferred_family || preferred_family == AF_INET6)
			groups |= nl_mgrp(RTNLGRP_IPV6_PREFIX);
	}
	if (lneigh) {
		groups |= nl_mgrp(RTNLGRP_NEIGH);
	}
	if (file) {
		FILE *fp;
		fp = fopen(file, "r");
		if (fp == NULL) {
			perror("Cannot fopen");
			exit(-1);
		}
		return rtnl_from_file(fp, accept_msg, stdout);
	}

	if (rtnl_open(&rth, groups) < 0)
		exit(1);
	ll_init_map(&rth);

	if (rtnl_listen(&rth, accept_msg, stdout) < 0)
		exit(2);

	return 0;
}
