/*
 * brstate.c	RTnetlink port state change
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:	Stephen Hemminger <shemminger@osdl.org>
 *
 *              Modified by Srinivas Aji <Aji_Srinivas@emc.com> for use
 *              in RSTP daemon. - 2006-09-01
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <net/if.h>
#include <netinet/in.h>
#include <linux/if_bridge.h>
#include <string.h>

#include "libnetlink.h"

static int br_set_state(struct rtnl_handle *rth, unsigned ifindex, __u8 state)
{
	struct {
		struct nlmsghdr n;
		struct ifinfomsg ifi;
		char buf[256];
	} req;

	memset(&req, 0, sizeof(req));

	req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg));
	req.n.nlmsg_flags = NLM_F_REQUEST | NLM_F_REPLACE;
	req.n.nlmsg_type = RTM_SETLINK;
	req.ifi.ifi_family = AF_BRIDGE;
	req.ifi.ifi_index = ifindex;

	addattr_l(&req.n, sizeof(req.buf), IFLA_PROTINFO, &state, sizeof(state));

	return rtnl_talk(rth, &req.n, 0, 0, NULL, NULL, NULL);
}

#include "bridge_ctl.h"

extern struct rtnl_handle rth_state;

int bridge_set_state(int ifindex, int brstate)
{
	int err = br_set_state(&rth_state, ifindex, brstate);
	if (err < 0) {
		fprintf(stderr,
			"Couldn't set bridge state, ifindex %d, state %d\n",
			ifindex, brstate);
		return -1;
	}
	return 0;
}
