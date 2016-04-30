/*
 * link_veth.c	veth driver module
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:	Pavel Emelianov <xemul@openvz.org>
 *
 */

#include <string.h>
#include <net/if.h>
#include <linux/veth.h>

#include "utils.h"
#include "ip_common.h"

static void print_usage(FILE *f)
{
	printf("Usage: ip link <options> type veth [peer <options>]\n"
	       "To get <options> type 'ip link add help'\n");
}

static void usage(void)
{
	print_usage(stderr);
}

static int veth_parse_opt(struct link_util *lu, int argc, char **argv,
			  struct nlmsghdr *hdr)
{
	char *dev = NULL;
	char *name = NULL;
	char *link = NULL;
	char *type = NULL;
	int index = 0;
	int err, len;
	struct rtattr * data;
	int group;
	struct ifinfomsg *ifm, *peer_ifm;
	unsigned int ifi_flags, ifi_change;

	if (strcmp(argv[0], "peer") != 0) {
		usage();
		return -1;
	}

	ifm = NLMSG_DATA(hdr);
	ifi_flags = ifm->ifi_flags;
	ifi_change = ifm->ifi_change;
	ifm->ifi_flags = 0;
	ifm->ifi_change = 0;

	data = NLMSG_TAIL(hdr);
	addattr_l(hdr, 1024, VETH_INFO_PEER, NULL, 0);

	hdr->nlmsg_len += sizeof(struct ifinfomsg);

	err = iplink_parse(argc - 1, argv + 1, (struct iplink_req *)hdr,
			   &name, &type, &link, &dev, &group, &index);
	if (err < 0)
		return err;

	if (name) {
		len = strlen(name) + 1;
		if (len > IFNAMSIZ)
			invarg("\"name\" too long\n", *argv);
		addattr_l(hdr, 1024, IFLA_IFNAME, name, len);
	}

	peer_ifm = RTA_DATA(data);
	peer_ifm->ifi_index = index;
	peer_ifm->ifi_flags = ifm->ifi_flags;
	peer_ifm->ifi_change = ifm->ifi_change;
	ifm->ifi_flags = ifi_flags;
	ifm->ifi_change = ifi_change;

	if (group != -1)
		addattr32(hdr, 1024, IFLA_GROUP, group);

	data->rta_len = (void *)NLMSG_TAIL(hdr) - (void *)data;
	return argc - 1 - err;
}

static void veth_print_help(struct link_util *lu, int argc, char **argv,
	FILE *f)
{
	print_usage(f);
}

struct link_util veth_link_util = {
	.id = "veth",
	.parse_opt = veth_parse_opt,
	.print_help = veth_print_help,
};
