/*
 * iplink_bond_slave.c	Bonding slave device support
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
#include <linux/if_bonding.h>

#include "rt_names.h"
#include "utils.h"
#include "ip_common.h"

static const char *slave_states[] = {
	[BOND_STATE_ACTIVE] = "ACTIVE",
	[BOND_STATE_BACKUP] = "BACKUP",
};

static void print_slave_state(FILE *f, struct rtattr *tb)
{
	unsigned int state = rta_getattr_u8(tb);

	if (state >= sizeof(slave_states) / sizeof(slave_states[0]))
		fprintf(f, "state %d ", state);
	else
		fprintf(f, "state %s ", slave_states[state]);
}

static const char *slave_mii_status[] = {
	[BOND_LINK_UP] = "UP",
	[BOND_LINK_FAIL] = "GOING_DOWN",
	[BOND_LINK_DOWN] = "DOWN",
	[BOND_LINK_BACK] = "GOING_BACK",
};

static void print_slave_mii_status(FILE *f, struct rtattr *tb)
{
	unsigned int status = rta_getattr_u8(tb);

	if (status >= sizeof(slave_mii_status) / sizeof(slave_mii_status[0]))
		fprintf(f, "mii_status %d ", status);
	else
		fprintf(f, "mii_status %s ", slave_mii_status[status]);
}

static void bond_slave_print_opt(struct link_util *lu, FILE *f, struct rtattr *tb[])
{
	SPRINT_BUF(b1);
	if (!tb)
		return;

	if (tb[IFLA_BOND_SLAVE_STATE])
		print_slave_state(f, tb[IFLA_BOND_SLAVE_STATE]);

	if (tb[IFLA_BOND_SLAVE_MII_STATUS])
		print_slave_mii_status(f, tb[IFLA_BOND_SLAVE_MII_STATUS]);

	if (tb[IFLA_BOND_SLAVE_LINK_FAILURE_COUNT])
		fprintf(f, "link_failure_count %d ",
			rta_getattr_u32(tb[IFLA_BOND_SLAVE_LINK_FAILURE_COUNT]));

	if (tb[IFLA_BOND_SLAVE_PERM_HWADDR])
		fprintf(f, "perm_hwaddr %s ",
			ll_addr_n2a(RTA_DATA(tb[IFLA_BOND_SLAVE_PERM_HWADDR]),
				    RTA_PAYLOAD(tb[IFLA_BOND_SLAVE_PERM_HWADDR]),
				    0, b1, sizeof(b1)));

	if (tb[IFLA_BOND_SLAVE_QUEUE_ID])
		fprintf(f, "queue_id %d ",
			rta_getattr_u16(tb[IFLA_BOND_SLAVE_QUEUE_ID]));

	if (tb[IFLA_BOND_SLAVE_AD_AGGREGATOR_ID])
		fprintf(f, "ad_aggregator_id %d ",
			rta_getattr_u16(tb[IFLA_BOND_SLAVE_AD_AGGREGATOR_ID]));
}

static int bond_slave_parse_opt(struct link_util *lu, int argc, char **argv,
				struct nlmsghdr *n)
{
	__u16 queue_id;

	while (argc > 0) {
		if (matches(*argv, "queue_id") == 0) {
			NEXT_ARG();
			if (get_u16(&queue_id, *argv, 0))
				invarg("queue_id is invalid", *argv);
			addattr16(n, 1024, IFLA_BOND_SLAVE_QUEUE_ID, queue_id);
		}
		argc--, argv++;
	}

	return 0;
}

struct link_util bond_slave_link_util = {
	.id		= "bond",
	.maxattr	= IFLA_BOND_SLAVE_MAX,
	.print_opt	= bond_slave_print_opt,
	.parse_opt	= bond_slave_parse_opt,
	.slave		= true,
};

