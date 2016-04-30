/*
 * Get/set/delete bridge with netlink
 *
 * Authors:	Stephen Hemminger <shemminger@vyatta.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <string.h>

#include "SNAPSHOT.h"
#include "utils.h"
#include "br_common.h"
#include "namespace.h"

struct rtnl_handle rth = { .fd = -1 };
int preferred_family = AF_UNSPEC;
int resolve_hosts;
int oneline = 0;
int show_stats;
int show_details;
int timestamp;
char * _SL_ = NULL;

static void usage(void) __attribute__((noreturn));

static void usage(void)
{
	fprintf(stderr,
"Usage: bridge [ OPTIONS ] OBJECT { COMMAND | help }\n"
"where  OBJECT := { link | fdb | mdb | vlan | monitor }\n"
"       OPTIONS := { -V[ersion] | -s[tatistics] | -d[etails] |\n"
"                    -o[neline] | -t[imestamp] | -n[etns] name }\n");
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
	{ "link", 	do_link },
	{ "fdb", 	do_fdb },
	{ "mdb", 	do_mdb },
	{ "vlan",	do_vlan },
	{ "monitor",	do_monitor },
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

	fprintf(stderr, "Object \"%s\" is unknown, try \"bridge help\".\n", argv0);
	return -1;
}

int
main(int argc, char **argv)
{
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

		if (matches(opt, "-help") == 0) {
			usage();
		} else if (matches(opt, "-Version") == 0) {
			printf("bridge utility, 0.0\n");
			exit(0);
		} else if (matches(opt, "-stats") == 0 ||
			   matches(opt, "-statistics") == 0) {
			++show_stats;
		} else if (matches(opt, "-details") == 0) {
			++show_details;
		} else if (matches(opt, "-oneline") == 0) {
			++oneline;
		} else if (matches(opt, "-timestamp") == 0) {
			++timestamp;
                } else if (matches(opt, "-family") == 0) {
			argc--;
			argv++;
			if (argc <= 1)
				usage();
			if (strcmp(argv[1], "inet") == 0)
				preferred_family = AF_INET;
			else if (strcmp(argv[1], "inet6") == 0)
				preferred_family = AF_INET6;
			else if (strcmp(argv[1], "help") == 0)
				usage();
			else
				invarg("invalid protocol family", argv[1]);
		} else if (strcmp(opt, "-4") == 0) {
			preferred_family = AF_INET;
		} else if (strcmp(opt, "-6") == 0) {
			preferred_family = AF_INET6;
		} else if (matches(opt, "-netns") == 0) {
			NEXT_ARG();
			if (netns_switch(argv[1]))
				exit(-1);
		} else {
			fprintf(stderr, "Option \"%s\" is unknown, try \"bridge help\".\n", opt);
			exit(-1);
		}
		argc--;	argv++;
	}

	_SL_ = oneline ? "\\" : "\n" ;

	if (rtnl_open(&rth, 0) < 0)
		exit(1);

	if (argc > 1)
		return do_cmd(argv[1], argc-1, argv+1);

	rtnl_close(&rth);
	usage();
}
