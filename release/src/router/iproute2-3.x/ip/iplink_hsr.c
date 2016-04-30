/*
 * iplink_hsr.c	HSR device support
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:	Arvid Brodin <arvid.brodin@alten.se>
 *
 *		Based on iplink_vlan.c by Patrick McHardy <kaber@trash.net>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>  /* Needed by linux/if.h for some reason */
#include <linux/if.h>
#include <linux/if_arp.h>
#include "rt_names.h"
#include "utils.h"
#include "ip_common.h"

static void print_usage(FILE *f)
{
	fprintf(f,
"Usage:\tip link add name NAME type hsr slave1 SLAVE1-IF slave2 SLAVE2-IF\n"
"\t[ supervision ADDR-BYTE ]\n"
"\n"
"NAME\n"
"	name of new hsr device (e.g. hsr0)\n"
"SLAVE1-IF, SLAVE2-IF\n"
"	the two slave devices bound to the HSR device\n"
"ADDR-BYTE\n"
"	0-255; the last byte of the multicast address used for HSR supervision\n"
"	frames (default = 0)\n");
}

static void usage(void)
{
	print_usage(stderr);
}

static int hsr_parse_opt(struct link_util *lu, int argc, char **argv,
			 struct nlmsghdr *n)
{
	int ifindex;
	unsigned char multicast_spec;

	while (argc > 0) {
		if (matches(*argv, "supervision") == 0) {
			NEXT_ARG();
			if (get_u8(&multicast_spec, *argv, 0))
				invarg("ADDR-BYTE is invalid", *argv);
			addattr_l(n, 1024, IFLA_HSR_MULTICAST_SPEC,
				  &multicast_spec, 1);
		} else if (matches(*argv, "slave1") == 0) {
			NEXT_ARG();
			ifindex = ll_name_to_index(*argv);
			if (ifindex == 0)
				invarg("No such interface", *argv);
			addattr_l(n, 1024, IFLA_HSR_SLAVE1, &ifindex, 4);
		} else if (matches(*argv, "slave2") == 0) {
			NEXT_ARG();
			ifindex = ll_name_to_index(*argv);
			if (ifindex == 0)
				invarg("No such interface", *argv);
			addattr_l(n, 1024, IFLA_HSR_SLAVE2, &ifindex, 4);
		} else if (matches(*argv, "help") == 0) {
			usage();
			return -1;
		} else {
			fprintf(stderr, "hsr: what is \"%s\"?\n", *argv);
			usage();
			return -1;
		}
		argc--, argv++;
	}

	return 0;
}

static void hsr_print_opt(struct link_util *lu, FILE *f, struct rtattr *tb[])
{
	SPRINT_BUF(b1);

	if (!tb)
		return;

	if (tb[IFLA_HSR_SLAVE1] &&
	    RTA_PAYLOAD(tb[IFLA_HSR_SLAVE1]) < sizeof(__u32))
		return;
	if (tb[IFLA_HSR_SLAVE2] &&
	    RTA_PAYLOAD(tb[IFLA_HSR_SLAVE2]) < sizeof(__u32))
		return;
	if (tb[IFLA_HSR_SEQ_NR] &&
	    RTA_PAYLOAD(tb[IFLA_HSR_SEQ_NR]) < sizeof(__u16))
		return;
	if (tb[IFLA_HSR_SUPERVISION_ADDR] &&
	    RTA_PAYLOAD(tb[IFLA_HSR_SUPERVISION_ADDR]) < ETH_ALEN)
		return;

	fprintf(f, "slave1 ");
	if (tb[IFLA_HSR_SLAVE1])
		fprintf(f, "%s ",
			ll_index_to_name(rta_getattr_u32(tb[IFLA_HSR_SLAVE1])));
	else
		fprintf(f, "<none> ");

	fprintf(f, "slave2 ");
	if (tb[IFLA_HSR_SLAVE2])
		fprintf(f, "%s ",
			ll_index_to_name(rta_getattr_u32(tb[IFLA_HSR_SLAVE2])));
	else
		fprintf(f, "<none> ");

	if (tb[IFLA_HSR_SEQ_NR])
		fprintf(f, "sequence %d ",
			rta_getattr_u16(tb[IFLA_HSR_SEQ_NR]));

	if (tb[IFLA_HSR_SUPERVISION_ADDR])
		fprintf(f, "supervision %s ",
			ll_addr_n2a(RTA_DATA(tb[IFLA_HSR_SUPERVISION_ADDR]),
				    RTA_PAYLOAD(tb[IFLA_HSR_SUPERVISION_ADDR]),
				    ARPHRD_VOID,
				    b1, sizeof(b1)));
}

static void hsr_print_help(struct link_util *lu, int argc, char **argv,
	FILE *f)
{
	print_usage(f);
}

struct link_util hsr_link_util = {
	.id		= "hsr",
	.maxattr	= IFLA_VLAN_MAX,
	.parse_opt	= hsr_parse_opt,
	.print_opt	= hsr_print_opt,
	.print_help	= hsr_print_help,
};
