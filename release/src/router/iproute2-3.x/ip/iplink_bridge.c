/*
 * iplink_bridge.c	Bridge device support
 *
 *              This program is free software; you can redistribute it and/or
 *              modify it under the terms of the GNU General Public License
 *              as published by the Free Software Foundation; either version
 *              2 of the License, or (at your option) any later version.
 *
 * Authors:     Jiri Pirko <jiri@resnulli.us>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/if_link.h>

#include "utils.h"
#include "ip_common.h"

static void explain(void)
{
	fprintf(stderr,
		"Usage: ... bridge [ forward_delay FORWARD_DELAY ]\n"
		"                  [ hello_time HELLO_TIME ]\n"
		"                  [ max_age MAX_AGE ]\n"
	);
}

static int bridge_parse_opt(struct link_util *lu, int argc, char **argv,
			    struct nlmsghdr *n)
{
	__u32 val;

	while (argc > 0) {
		if (matches(*argv, "forward_delay") == 0) {
			NEXT_ARG();
			if (get_u32(&val, *argv, 0)) {
				invarg("invalid forward_delay", *argv);
				return -1;
			}
			addattr32(n, 1024, IFLA_BR_FORWARD_DELAY, val);
		} else if (matches(*argv, "hello_time") == 0) {
			NEXT_ARG();
			if (get_u32(&val, *argv, 0)) {
				invarg("invalid hello_time", *argv);
				return -1;
			}
			addattr32(n, 1024, IFLA_BR_HELLO_TIME, val);
		} else if (matches(*argv, "max_age") == 0) {
			NEXT_ARG();
			if (get_u32(&val, *argv, 0)) {
				invarg("invalid max_age", *argv);
				return -1;
			}
			addattr32(n, 1024, IFLA_BR_MAX_AGE, val);
		} else if (matches(*argv, "help") == 0) {
			explain();
			return -1;
		} else {
			fprintf(stderr, "bridge: unknown command \"%s\"?\n", *argv);
			explain();
			return -1;
		}
		argc--, argv++;
	}

	return 0;
}

static void bridge_print_opt(struct link_util *lu, FILE *f, struct rtattr *tb[])
{
	if (!tb)
		return;

	if (tb[IFLA_BR_FORWARD_DELAY])
		fprintf(f, "forward_delay %u ",
			rta_getattr_u32(tb[IFLA_BR_FORWARD_DELAY]));

	if (tb[IFLA_BR_HELLO_TIME])
		fprintf(f, "hello_time %u ",
			rta_getattr_u32(tb[IFLA_BR_HELLO_TIME]));

	if (tb[IFLA_BR_MAX_AGE])
		fprintf(f, "max_age %u ",
			rta_getattr_u32(tb[IFLA_BR_MAX_AGE]));
}

struct link_util bridge_link_util = {
	.id		= "bridge",
	.maxattr	= IFLA_BR_MAX,
	.parse_opt	= bridge_parse_opt,
	.print_opt	= bridge_print_opt,
};
