/*
 * iplink_macvtap.c	macvtap device support
 *
 *              This program is free software; you can redistribute it and/or
 *              modify it under the terms of the GNU General Public License
 *              as published by the Free Software Foundation; either version
 *              2 of the License, or (at your option) any later version.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <linux/if_link.h>

#include "rt_names.h"
#include "utils.h"
#include "ip_common.h"

static void explain(void)
{
	fprintf(stderr,
		"Usage: ... macvtap mode { private | vepa | bridge | passthru }\n"
	);
}

static int mode_arg(void)
{
        fprintf(stderr, "Error: argument of \"mode\" must be \"private\", "
		"\"vepa\", \"bridge\" or \"passthru\" \n");
        return -1;
}

static int macvtap_parse_opt(struct link_util *lu, int argc, char **argv,
			  struct nlmsghdr *n)
{
	while (argc > 0) {
		if (matches(*argv, "mode") == 0) {
			__u32 mode = 0;
			NEXT_ARG();

			if (strcmp(*argv, "private") == 0)
				mode = MACVLAN_MODE_PRIVATE;
			else if (strcmp(*argv, "vepa") == 0)
				mode = MACVLAN_MODE_VEPA;
			else if (strcmp(*argv, "bridge") == 0)
				mode = MACVLAN_MODE_BRIDGE;
			else if (strcmp(*argv, "passthru") == 0)
				mode = MACVLAN_MODE_PASSTHRU;
			else
				return mode_arg();

			addattr32(n, 1024, IFLA_MACVLAN_MODE, mode);
		} else if (matches(*argv, "help") == 0) {
			explain();
			return -1;
		} else {
			fprintf(stderr, "macvtap: what is \"%s\"?\n", *argv);
			explain();
			return -1;
		}
		argc--, argv++;
	}

	return 0;
}

static void macvtap_print_opt(struct link_util *lu, FILE *f, struct rtattr *tb[])
{
	__u32 mode;

	if (!tb)
		return;

	if (!tb[IFLA_MACVLAN_MODE] ||
	    RTA_PAYLOAD(tb[IFLA_MACVLAN_MODE]) < sizeof(__u32))
		return;

	mode = *(__u32 *)RTA_DATA(tb[IFLA_VLAN_ID]);
	fprintf(f, " mode %s ",
		  mode == MACVLAN_MODE_PRIVATE ? "private"
		: mode == MACVLAN_MODE_VEPA    ? "vepa"
		: mode == MACVLAN_MODE_BRIDGE  ? "bridge"
		: mode == MACVLAN_MODE_PASSTHRU  ? "passthru"
		:				 "unknown");
}

struct link_util macvtap_link_util = {
	.id		= "macvtap",
	.maxattr	= IFLA_MACVLAN_MAX,
	.parse_opt	= macvtap_parse_opt,
	.print_opt	= macvtap_print_opt,
};
