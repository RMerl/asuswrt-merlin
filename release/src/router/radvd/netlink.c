/*
 *
 *   Authors:
 *    Lars Fenneberg		<lf@elemental.net>	 
 *    Reuben Hawkins		<reubenhwk@gmail.com>
 *
 *   This software is Copyright 1996,1997 by the above mentioned author(s), 
 *   All Rights Reserved.
 *
 *   The license which is distributed with this software in the file COPYRIGHT
 *   applies to this software. If your distribution is missing this file, you
 *   may request it from <pekkas@netcore.fi>.
 *
 */

#include "config.h"
#include "radvd.h"
#include "log.h"
#include "netlink.h"

#include <asm/types.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <net/if.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

void process_netlink_msg(int sock)
{
	int len;
	char buf[4096];
	struct iovec iov = { buf, sizeof(buf) };
	struct sockaddr_nl sa;
	struct msghdr msg = { (void *)&sa, sizeof(sa), &iov, 1, NULL, 0, 0 };
	struct nlmsghdr *nh;
	struct ifinfomsg * ifinfo;
	char ifname[IF_NAMESIZE] = {""};

	len = recvmsg (sock, &msg, 0);
	if (len == -1) {
		flog(LOG_ERR, "recvmsg failed: %s", strerror(errno));
	}

	for (nh = (struct nlmsghdr *) buf; NLMSG_OK (nh, len); nh = NLMSG_NEXT (nh, len)) {
		/* The end of multipart message. */
		if (nh->nlmsg_type == NLMSG_DONE)
			return;

		if (nh->nlmsg_type == NLMSG_ERROR) {
			flog(LOG_ERR, "%s:%d Some type of netlink error.\n", __FILE__, __LINE__);
			abort();
		}

		/* Continue with parsing payload. */
		ifinfo = NLMSG_DATA(nh);
		if_indextoname(ifinfo->ifi_index, ifname);
		if (ifinfo->ifi_flags & IFF_RUNNING) {
			dlog(LOG_DEBUG, 3, "%s, ifindex %d, flags is running", ifname, ifinfo->ifi_index);
		}
		else {
			dlog(LOG_DEBUG, 3, "%s, ifindex %d, flags is *NOT* running", ifname, ifinfo->ifi_index);
		}
		reload_config();
	}
}

int netlink_socket(void)
{
	int rc, sock;
	struct sockaddr_nl snl;

	sock = socket(PF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	if (sock == -1) {
		flog(LOG_ERR, "Unable to open netlink socket: %s", strerror(errno));
	}

	memset(&snl, 0, sizeof(snl));
	snl.nl_family = AF_NETLINK;
	snl.nl_groups = RTMGRP_LINK;

	rc = bind(sock, (struct sockaddr*)&snl, sizeof(snl));
	if (rc == -1) {
		flog(LOG_ERR, "Unable to bind netlink socket: %s", strerror(errno));
		close(sock);
		sock = -1;
	}

	return sock;
}

