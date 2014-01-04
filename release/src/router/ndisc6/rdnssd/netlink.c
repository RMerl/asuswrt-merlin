/**
 * netlink.c - NetLink source for ICMPv6 RDNSS
 * $Id: netlink.c 584 2008-01-30 16:54:05Z remi $
 */

/*************************************************************************
 *  Copyright © 2007 Pierre Ynard, Rémi Denis-Courmont.                  *
 *  This program is free software: you can redistribute and/or modify    *
 *  it under the terms of the GNU General Public License as published by *
 *  the Free Software Foundation, versions 2 or 3 of the license.        *
 *                                                                       *
 *  This program is distributed in the hope that it will be useful,      *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of       *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        *
 *  GNU General Public License for more details.                         *
 *                                                                       *
 *  You should have received a copy of the GNU General Public License    *
 *  along with this program. If not, see <http://www.gnu.org/licenses/>. *
 *************************************************************************/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef __linux__

#include <string.h>

#include <errno.h>
#include <syslog.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/icmp6.h>
#include <sys/utsname.h>
#include <linux/rtnetlink.h>

#include "rdnssd.h"
#include "gettext.h"


#ifndef RTNLGRP_ND_USEROPT
# warning You need to update your Linux kernel headers (>= 2.6.24)
/* Belongs in <linux/rtnetlink.h> */

struct nduseroptmsg
{
	unsigned char	nduseropt_family;
	unsigned char	nduseropt_pad1;
	unsigned short	nduseropt_opts_len; /* Total length of options */
	int		nduseropt_ifindex;
	__u8		nduseropt_icmp_type;
	__u8		nduseropt_icmp_code;
	unsigned short	nduseropt_pad2;
	unsigned int	nduseropt_pad3;
	/* Followed by one or more ND options */
};

# define RTNLGRP_ND_USEROPT 20
#endif

static int nl_recv (int fd)
{
	unsigned int buf_size = NLMSG_SPACE(65536 - sizeof(struct icmp6_hdr));
	uint8_t buf[buf_size];
	size_t msg_size;
	struct nduseroptmsg *ndmsg;

	memset(buf, 0, buf_size);
	msg_size = recv(fd, buf, buf_size, 0);
	if (msg_size == (size_t)(-1))
		return -1;

	if (msg_size < NLMSG_SPACE(sizeof(struct nduseroptmsg)))
		return -1;

	ndmsg = (struct nduseroptmsg *) NLMSG_DATA((struct nlmsghdr *) buf);

	if (ndmsg->nduseropt_family != AF_INET6
		|| ndmsg->nduseropt_icmp_type != ND_ROUTER_ADVERT
		|| ndmsg->nduseropt_icmp_code != 0)
		return 0;

	if (msg_size < NLMSG_SPACE(sizeof(struct nduseroptmsg) + ndmsg->nduseropt_opts_len))
		return -1;

	return parse_nd_opts((struct nd_opt_hdr *) (ndmsg + 1), ndmsg->nduseropt_opts_len, ndmsg->nduseropt_ifindex);

}


static int nl_socket (void)
{
#if 0
	struct sockaddr_nl saddr;
	struct utsname uts;
	int fd;

	/* Netlink RDNSS support starts with 2.6.24 */
	uname (&uts);
	if (strverscmp (uts.release, "2.6.24") < 0)
	{
		errno = ENOSYS;
		return -1;
	}

	fd = socket(AF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE);
	if (fd == -1)
	{
		syslog(LOG_CRIT, _("cannot open netlink socket"));
		return fd;
	}

	memset(&saddr, 0, sizeof(struct sockaddr_nl));
	saddr.nl_family = AF_NETLINK;
	saddr.nl_pid = getpid();
	saddr.nl_groups = 1 << (RTNLGRP_ND_USEROPT - 1);

	if (bind (fd, (struct sockaddr *) &saddr, sizeof (struct sockaddr_nl)))
		return -1;

	return fd;
#else
	return -1;
#endif
}

const rdnss_src_t rdnss_netlink = { nl_socket, nl_recv };

#endif /* __linux__ */
