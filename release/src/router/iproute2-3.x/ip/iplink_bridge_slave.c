/*
 * iplink_bridge_slave.c	Bridge slave device support
 *
 *              This program is free software; you can redistribute it and/or
 *              modify it under the terms of the GNU General Public License
 *              as published by the Free Software Foundation; either version
 *              2 of the License, or (at your option) any later version.
 *
 * Authors:     Jiri Pirko <jiri@resnulli.us>
 */

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <linux/if_link.h>
#include <linux/if_bridge.h>

#include "rt_names.h"
#include "utils.h"
#include "ip_common.h"

static void explain(void)
{
	fprintf(stderr,
		"Usage: ... bridge_slave [ state STATE ] [ priority PRIO ] [cost COST ]\n"
		"                        [ guard {on | off} ]\n"
		"                        [ hairpin {on | off} ] \n"
		"                        [ fastleave {on | off} ]\n"
		"                        [ root_block {on | off} ]\n"
		"                        [ learning {on | off} ]\n"
		"                        [ flood {on | off} ]\n"
	);
}

static const char *port_states[] = {
	[BR_STATE_DISABLED] = "disabled",
	[BR_STATE_LISTENING] = "listening",
	[BR_STATE_LEARNING] = "learning",
	[BR_STATE_FORWARDING] = "forwarding",
	[BR_STATE_BLOCKING] = "blocking",
};

static void print_portstate(FILE *f, __u8 state)
{
	if (state <= BR_STATE_BLOCKING)
		fprintf(f, "state %s ", port_states[state]);
	else
		fprintf(f, "state (%d) ", state);
}

static void print_onoff(FILE *f, char *flag, __u8 val)
{
	fprintf(f, "%s %s ", flag, val ? "on" : "off");
}

static void bridge_slave_print_opt(struct link_util *lu, FILE *f,
				   struct rtattr *tb[])
{
	if (!tb)
		return;

	if (tb[IFLA_BRPORT_STATE])
		print_portstate(f, rta_getattr_u8(tb[IFLA_BRPORT_STATE]));

	if (tb[IFLA_BRPORT_PRIORITY])
		fprintf(f, "priority %d ",
			rta_getattr_u16(tb[IFLA_BRPORT_PRIORITY]));

	if (tb[IFLA_BRPORT_COST])
		fprintf(f, "cost %d ",
			rta_getattr_u32(tb[IFLA_BRPORT_COST]));

	if (tb[IFLA_BRPORT_MODE])
		print_onoff(f, "hairpin",
			    rta_getattr_u8(tb[IFLA_BRPORT_MODE]));

	if (tb[IFLA_BRPORT_GUARD])
		print_onoff(f, "guard",
			    rta_getattr_u8(tb[IFLA_BRPORT_GUARD]));

	if (tb[IFLA_BRPORT_PROTECT])
		print_onoff(f, "root_block",
			    rta_getattr_u8(tb[IFLA_BRPORT_PROTECT]));

	if (tb[IFLA_BRPORT_FAST_LEAVE])
		print_onoff(f, "fastleave",
			    rta_getattr_u8(tb[IFLA_BRPORT_FAST_LEAVE]));

	if (tb[IFLA_BRPORT_LEARNING])
		print_onoff(f, "learning",
			rta_getattr_u8(tb[IFLA_BRPORT_LEARNING]));

	if (tb[IFLA_BRPORT_UNICAST_FLOOD])
		print_onoff(f, "flood",
			rta_getattr_u8(tb[IFLA_BRPORT_UNICAST_FLOOD]));
}

static void bridge_slave_parse_on_off(char *arg_name, char *arg_val,
				      struct nlmsghdr *n, int type)
{
	__u8 val;

	if (strcmp(arg_val, "on") == 0)
		val = 1;
	else if (strcmp(arg_val, "off") == 0)
		val = 0;
	else
		invarg("should be \"on\" or \"off\"", arg_name);

	addattr8(n, 1024, type, val);
}

static int bridge_slave_parse_opt(struct link_util *lu, int argc, char **argv,
				  struct nlmsghdr *n)
{
	__u8 state;
	__u16 priority;
	__u32 cost;

	while (argc > 0) {
		if (matches(*argv, "state") == 0) {
			NEXT_ARG();
			if (get_u8(&state, *argv, 0))
				invarg("state is invalid", *argv);
			addattr8(n, 1024, IFLA_BRPORT_STATE, state);
		} else if (matches(*argv, "priority") == 0) {
			NEXT_ARG();
			if (get_u16(&priority, *argv, 0))
				invarg("priority is invalid", *argv);
			addattr16(n, 1024, IFLA_BRPORT_PRIORITY, priority);
		} else if (matches(*argv, "cost") == 0) {
			NEXT_ARG();
			if (get_u32(&cost, *argv, 0))
				invarg("cost is invalid", *argv);
			addattr32(n, 1024, IFLA_BRPORT_COST, cost);
		} else if (matches(*argv, "hairpin") == 0) {
			NEXT_ARG();
			bridge_slave_parse_on_off("hairpin", *argv, n,
						  IFLA_BRPORT_MODE);
		} else if (matches(*argv, "guard") == 0) {
			NEXT_ARG();
			bridge_slave_parse_on_off("guard", *argv, n,
						  IFLA_BRPORT_GUARD);
		} else if (matches(*argv, "root_block") == 0) {
			NEXT_ARG();
			bridge_slave_parse_on_off("root_block", *argv, n,
						  IFLA_BRPORT_PROTECT);
		} else if (matches(*argv, "fastleave") == 0) {
			NEXT_ARG();
			bridge_slave_parse_on_off("fastleave", *argv, n,
						  IFLA_BRPORT_FAST_LEAVE);
		} else if (matches(*argv, "learning") == 0) {
			NEXT_ARG();
			bridge_slave_parse_on_off("learning", *argv, n,
						  IFLA_BRPORT_LEARNING);
		} else if (matches(*argv, "flood") == 0) {
			NEXT_ARG();
			bridge_slave_parse_on_off("flood", *argv, n,
						  IFLA_BRPORT_UNICAST_FLOOD);
		} else if (matches(*argv, "help") == 0) {
			explain();
			return -1;
		} else {
			fprintf(stderr, "bridge_slave: unknown option \"%s\"?\n",
				*argv);
			explain();
			return -1;
		}
		argc--, argv++;
	}

	return 0;
}

struct link_util bridge_slave_link_util = {
	.id		= "bridge",
	.maxattr	= IFLA_BRPORT_MAX,
	.print_opt	= bridge_slave_print_opt,
	.parse_opt	= bridge_slave_parse_opt,
	.slave		= true,
};
