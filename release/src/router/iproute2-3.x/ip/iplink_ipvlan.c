/* iplink_ipvlan.c	IPVLAN device support
 *
 *              This program is free software; you can redistribute it and/or
 *              modify it under the terms of the GNU General Public License
 *              as published by the Free Software Foundation; either version
 *              2 of the License, or (at your option) any later version.
 *
 * Authors:     Mahesh Bandewar <maheshb@google.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <linux/if_link.h>

#include "rt_names.h"
#include "utils.h"
#include "ip_common.h"

static void ipvlan_explain(FILE *f)
{
	fprintf(f, "Usage: ... ipvlan [ mode { l2 | l3 } ]\n");
}

static void explain(void)
{
	ipvlan_explain(stderr);
}

static int mode_arg(void)
{
	fprintf(stderr, "Error: argument of \"mode\" must be either \"l2\", "
		"or \"l3\"\n");
	return -1;
}

static int ipvlan_parse_opt(struct link_util *lu, int argc, char **argv,
			    struct nlmsghdr *n)
{
	while (argc > 0) {
		if (matches(*argv, "mode") == 0) {
			__u16 mode = 0;
			NEXT_ARG();

			if (strcmp(*argv, "l2") == 0)
				mode = IPVLAN_MODE_L2;
			else if (strcmp(*argv, "l3") == 0)
				mode = IPVLAN_MODE_L3;
			else
				return mode_arg();

			addattr16(n, 1024, IFLA_IPVLAN_MODE, mode);
		} else if (matches(*argv, "help") == 0) {
			explain();
			return -1;
		} else {
			fprintf(stderr, "ipvlan: unknown option \"%s\"?\n",
				*argv);
			explain();
			return -1;
		}
		argc--, argv++;
	}

	return 0;
}

static void ipvlan_print_opt(struct link_util *lu, FILE *f, struct rtattr *tb[])
{

	if (!tb)
		return;

	if (tb[IFLA_IPVLAN_MODE]) {
		if (RTA_PAYLOAD(tb[IFLA_IPVLAN_MODE]) == sizeof(__u16)) {
			__u16 mode = rta_getattr_u16(tb[IFLA_IPVLAN_MODE]);

			fprintf(f, " mode %s ",
				mode == IPVLAN_MODE_L2 ? "l2" :
				mode == IPVLAN_MODE_L3 ? "l3" : "unknown");
		}
	}
}

static void ipvlan_print_help(struct link_util *lu, int argc, char **argv,
			      FILE *f)
{
	ipvlan_explain(f);
}

struct link_util ipvlan_link_util = {
	.id		= "ipvlan",
	.maxattr	= IFLA_IPVLAN_MAX,
	.parse_opt	= ipvlan_parse_opt,
	.print_opt	= ipvlan_print_opt,
	.print_help	= ipvlan_print_help,
};
