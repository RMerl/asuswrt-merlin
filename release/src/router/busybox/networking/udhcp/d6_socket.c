/* vi: set sw=4 ts=4: */
/*
 * Copyright (C) 2011 Denys Vlasenko.
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */
#include "common.h"
#include "d6_common.h"
#include <net/if.h>

int FAST_FUNC d6_listen_socket(int port, const char *inf)
{
	int fd;
	struct sockaddr_in6 addr;

	log1("Opening listen socket on *:%d %s", port, inf);
	fd = xsocket(PF_INET6, SOCK_DGRAM, IPPROTO_UDP);

	setsockopt_reuseaddr(fd);
	if (setsockopt_broadcast(fd) == -1)
		bb_perror_msg_and_die("SO_BROADCAST");

	/* NB: bug 1032 says this doesn't work on ethernet aliases (ethN:M) */
	if (setsockopt_bindtodevice(fd, inf))
		xfunc_die(); /* warning is already printed */

	memset(&addr, 0, sizeof(addr));
	addr.sin6_family = AF_INET6;
	addr.sin6_port = htons(port);
	/* addr.sin6_addr is all-zeros */
	xbind(fd, (struct sockaddr *)&addr, sizeof(addr));

	return fd;
}
