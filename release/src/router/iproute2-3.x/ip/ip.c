/*
 * ip.c		"ip" utility frontend.
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:	Alexey Kuznetsov, <kuznet@ms2.inr.ac.ru>
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <errno.h>

#include "SNAPSHOT.h"
#include "utils.h"
#include "ip_common.h"

int preferred_family = AF_UNSPEC;
int show_stats = 0;
int show_details = 0;
int resolve_hosts = 0;
int oneline = 0;
int timestamp = 0;
char * _SL_ = NULL;
char *batch_file = NULL;
int force = 0;
int max_flush_loops = 10;

struct rtnl_handle rth = { .fd = -1 };

static void usage(void) __attribute__((noreturn));

static void usage(void)
{
	fprintf(stderr,
"Usage: ip [ OPTIONS ] OBJECT { COMMAND | help }\n"
"       ip [ -force ] -batch filename\n"
"where  OBJECT := { link | addr | addrlabel | route | rule | neigh | ntable |\n"
"                   tunnel | tuntap | maddr | mroute | mrule | monitor | xfrm }\n"
"       OPTIONS := { -V[ersion] | -s[tatistics] | -d[etails] | -r[esolve] |\n"
"                    -f[amily] { inet | inet6 | ipx | dnet | link } |\n"
"                    -l[oops] { maximum-addr-flush-attempts } |\n"
"                    -o[neline] | -t[imestamp] | -b[atch] [filename] |\n"
"                    -rc[vbuf] [size]}\n");
	exit(-1);
}

static int do_help(int argc, char **argv)
{
	usage();
}

static const struct cmd {
	const char *cmd;
	int (*func)(int argc, char **argv);
} cmds[] = {
	{ "address", 	do_ipaddr },
	{ "addrlabel",	do_ipaddrlabel },
	{ "maddress",	do_multiaddr },
	{ "route",	do_iproute },
	{ "rule",	do_iprule },
	{ "neighbor",	do_ipneigh },
	{ "neighbour",	do_ipneigh },
	{ "ntable",	do_ipntable },
	{ "ntbl",	do_ipntable },
	{ "link",	do_iplink },
	{ "tunnel",	do_iptunnel },
	{ "tunl",	do_iptunnel },
	{ "tuntap",	do_iptuntap },
	{ "tap",	do_iptuntap },
	{ "monitor",	do_ipmonitor },
	{ "xfrm",	do_xfrm },
	{ "mroute",	do_multiroute },
	{ "mrule",	do_multirule },
	{ "help",	do_help },
	{ 0 }
};

static int do_cmd(const char *argv0, int argc, char **argv)
{
	const struct cmd *c;

	for (c = cmds; c->cmd; ++c) {
		if (matches(argv0, c->cmd) == 0)
			return c->func(argc-1, argv+1);
	}

	fprintf(stderr, "Object \"%s\" is unknown, try \"ip help\".\n", argv0);
	return -1;
}

static int batch(const char *name)
{
	char *line = NULL;
	size_t len = 0;
	int ret = 0;

	if (name && strcmp(name, "-") != 0) {
		if (freopen(name, "r", stdin) == NULL) {
			fprintf(stderr, "Cannot open file \"%s\" for reading: %s\n",
				name, strerror(errno));
			return -1;
		}
	}

	if (rtnl_open(&rth, 0) < 0) {
		fprintf(stderr, "Cannot open rtnetlink\n");
		return -1;
	}

	cmdlineno = 0;
	while (getcmdline(&line, &len, stdin) != -1) {
		char *largv[100];
		int largc;

		largc = makeargs(line, largv, 100);
		if (largc == 0)
			continue;	/* blank line */

		if (do_cmd(largv[0], largc, largv)) {
			fprintf(stderr, "Command failed %s:%d\n", name, cmdlineno);
			ret = 1;
			if (!force)
				break;
		}
	}
	if (line)
		free(line);

	rtnl_close(&rth);
	return ret;
}


int main(int argc, char **argv)
{
	char *basename;

	basename = strrchr(argv[0], '/');
	if (basename == NULL)
		basename = argv[0];
	else
		basename++;

	while (argc > 1) {
		char *opt = argv[1];
		if (strcmp(opt,"--") == 0) {
			argc--; argv++;
			break;
		}
		if (opt[0] != '-')
			break;
		if (opt[1] == '-')
			opt++;
		if (matches(opt, "-loops") == 0) {
			argc--;
			argv++;
			if (argc <= 1)
				usage();
                        max_flush_loops = atoi(argv[1]);
                } else if (matches(opt, "-family") == 0) {
			argc--;
			argv++;
			if (argc <= 1)
				usage();
			if (strcmp(argv[1], "inet") == 0)
				preferred_family = AF_INET;
			else if (strcmp(argv[1], "inet6") == 0)
				preferred_family = AF_INET6;
			else if (strcmp(argv[1], "dnet") == 0)
				preferred_family = AF_DECnet;
			else if (strcmp(argv[1], "link") == 0)
				preferred_family = AF_PACKET;
			else if (strcmp(argv[1], "ipx") == 0)
				preferred_family = AF_IPX;
			else if (strcmp(argv[1], "help") == 0)
				usage();
			else
				invarg(argv[1], "invalid protocol family");
		} else if (strcmp(opt, "-4") == 0) {
			preferred_family = AF_INET;
		} else if (strcmp(opt, "-6") == 0) {
			preferred_family = AF_INET6;
		} else if (strcmp(opt, "-0") == 0) {
			preferred_family = AF_PACKET;
		} else if (strcmp(opt, "-I") == 0) {
			preferred_family = AF_IPX;
		} else if (strcmp(opt, "-D") == 0) {
			preferred_family = AF_DECnet;
		} else if (matches(opt, "-stats") == 0 ||
			   matches(opt, "-statistics") == 0) {
			++show_stats;
		} else if (matches(opt, "-details") == 0) {
			++show_details;
		} else if (matches(opt, "-resolve") == 0) {
			++resolve_hosts;
		} else if (matches(opt, "-oneline") == 0) {
			++oneline;
		} else if (matches(opt, "-timestamp") == 0) {
			++timestamp;
#if 0
		} else if (matches(opt, "-numeric") == 0) {
			rtnl_names_numeric++;
#endif
		} else if (matches(opt, "-Version") == 0) {
			printf("ip utility, iproute2-ss%s\n", SNAPSHOT);
			exit(0);
		} else if (matches(opt, "-force") == 0) {
			++force;
		} else if (matches(opt, "-batch") == 0) {
			argc--;
			argv++;
			if (argc <= 1)
				usage();
			batch_file = argv[1];
		} else if (matches(opt, "-rcvbuf") == 0) {
			unsigned int size;

			argc--;
			argv++;
			if (argc <= 1)
				usage();
			if (get_unsigned(&size, argv[1], 0)) {
				fprintf(stderr, "Invalid rcvbuf size '%s'\n",
					argv[1]);
				exit(-1);
			}
			rcvbuf = size;
		} else if (matches(opt, "-help") == 0) {
			usage();
		} else {
			fprintf(stderr, "Option \"%s\" is unknown, try \"ip -help\".\n", opt);
			exit(-1);
		}
		argc--;	argv++;
	}

	_SL_ = oneline ? "\\" : "\n" ;

	if (batch_file)
		return batch(batch_file);

	if (rtnl_open(&rth, 0) < 0)
		exit(1);

	if (strlen(basename) > 2)
		return do_cmd(basename+2, argc, argv);

	if (argc > 1)
		return do_cmd(argv[1], argc-1, argv+1);

	rtnl_close(&rth);
	usage();
}
