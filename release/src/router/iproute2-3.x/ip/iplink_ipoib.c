/*
 * iplink_ipoib.c	IPoIB device support
 *
 *              This program is free software; you can redistribute it and/or
 *              modify it under the terms of the GNU General Public License
 *              as published by the Free Software Foundation; either version
 *              2 of the License, or (at your option) any later version.
 *
 * Authors:     Or Gerlitz <ogerlitz@mellanox.com>
 *		copied iflink_vlan.c authored by Patrick McHardy <kaber@trash.net>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/if_link.h>

#include "rt_names.h"
#include "utils.h"
#include "ip_common.h"

static void print_explain(FILE *f)
{
	fprintf(f,
		"Usage: ... ipoib [pkey PKEY] [mode {datagram | connected}]"
		"[umcast {0|1}]\n"
		"\n"
		"PKEY  := 0x8001-0xffff\n"
	);
}

static void explain(void)
{
	print_explain(stderr);
}

static int mode_arg(void)
{
	fprintf(stderr, "Error: argument of \"mode\" must be \"datagram\""
		"or \"connected\"\n");
	return -1;
}

static int ipoib_parse_opt(struct link_util *lu, int argc, char **argv,
			  struct nlmsghdr *n)
{
	__u16 pkey, mode, umcast;

	while (argc > 0) {
		if (matches(*argv, "pkey") == 0) {
			NEXT_ARG();
			if (get_u16(&pkey, *argv, 0))
				invarg("pkey is invalid", *argv);
			addattr_l(n, 1024, IFLA_IPOIB_PKEY, &pkey, 2);
		} else if (matches(*argv, "mode") == 0) {
			NEXT_ARG();
			if (strcmp(*argv, "datagram") == 0)
				mode = IPOIB_MODE_DATAGRAM;
			else if (strcmp(*argv, "connected") == 0)
				mode = IPOIB_MODE_CONNECTED;
			else
				return mode_arg();
			addattr_l(n, 1024, IFLA_IPOIB_MODE, &mode, 2);
		} else if (matches(*argv, "umcast") == 0) {
			NEXT_ARG();
			if (get_u16(&umcast, *argv, 0))
				invarg("umcast is invalid", *argv);
			addattr_l(n, 1024, IFLA_IPOIB_UMCAST, &umcast, 2);
		} else if (matches(*argv, "help") == 0) {
			explain();
			return -1;
		} else {
			fprintf(stderr, "ipoib: unknown option \"%s\"?\n", *argv);
			explain();
			return -1;
		}
		argc--, argv++;
	}

	return 0;
}

static void ipoib_print_opt(struct link_util *lu, FILE *f, struct rtattr *tb[])
{
	__u16 mode;

	if (!tb)
		return;

	if (!tb[IFLA_IPOIB_PKEY] ||
	    RTA_PAYLOAD(tb[IFLA_IPOIB_PKEY]) < sizeof(__u16))
		return;

	fprintf(f, "pkey  %#.4x ", rta_getattr_u16(tb[IFLA_IPOIB_PKEY]));

	if (!tb[IFLA_IPOIB_MODE] ||
	    RTA_PAYLOAD(tb[IFLA_IPOIB_MODE]) < sizeof(__u16))
		return;

	mode = rta_getattr_u16(tb[IFLA_IPOIB_MODE]);
	fprintf(f, "mode  %s ",
		mode == IPOIB_MODE_DATAGRAM ? "datagram" :
		mode == IPOIB_MODE_CONNECTED ? "connected" :
		"unknown");

	if (!tb[IFLA_IPOIB_UMCAST] ||
	    RTA_PAYLOAD(tb[IFLA_IPOIB_UMCAST]) < sizeof(__u16))
		return;

	fprintf(f, "umcast  %.4x ", rta_getattr_u16(tb[IFLA_IPOIB_UMCAST]));
}

static void ipoib_print_help(struct link_util *lu, int argc, char **argv,
	FILE *f)
{
	print_explain(f);
}

struct link_util ipoib_link_util = {
	.id		= "ipoib",
	.maxattr	= IFLA_IPOIB_MAX,
	.parse_opt	= ipoib_parse_opt,
	.print_opt	= ipoib_print_opt,
	.print_help	= ipoib_print_help,
};
