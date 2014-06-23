/**
 * icmp.c - raw socket source for ICMPv6 RDNSS
 * $Id: icmp.c 618 2008-04-24 17:13:07Z remi $
 */

/*************************************************************************
 *  Copyright © 2007-2008 Pierre Ynard, Rémi Denis-Courmont.             *
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

#include <stdint.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/icmp6.h>
#include <syslog.h>
#include <stdio.h>
#include <stdlib.h>
#include <bcmnvram.h>
#include <shared.h>
#ifdef BCMARM
#include "nvram_linux_arm.c"
#endif

#include "rdnssd.h"
#include "gettext.h"

#ifndef IPV6_RECVHOPLIMIT
# warning using RFC2922 instead of RFC3542
# define IPV6_RECVHOPLIMIT IPV6_HOPLIMIT
#endif

#ifndef SOL_IPV6
# define SOL_IPV6 IPPROTO_IPV6
#endif
#ifndef SOL_ICMPV6
# define SOL_ICMPV6 IPPROTO_ICMPV6
#endif

#define dbg(fmt, args...) do { FILE *fp = fopen("/dev/console", "w"); if (fp) { fprintf(fp, fmt, ## args); fclose(fp); } else fprintf(stderr, fmt, ## args); } while (0)
#define mylog(fmt, args...) { dbg(fmt, ## args); dbg("\n"); syslog(0, fmt, ## args); }
static int icmp_recv (int fd)
{
		static unsigned last_flag = 1;
		unsigned flags;
		struct nd_router_advert icmp6;
		uint8_t buf[65536 - sizeof (icmp6)], cbuf[CMSG_SPACE (sizeof (int))];
		struct iovec iov[2] =
		{
			{ .iov_base = &icmp6, .iov_len = sizeof (icmp6) },
			{ .iov_base = buf, .iov_len = sizeof (buf) }
		};
		struct sockaddr_in6 src;
		struct msghdr msg =
		{
			.msg_iov = iov,
			.msg_iovlen = sizeof (iov) / sizeof (iov[0]),
			.msg_name = &src,
			.msg_namelen = sizeof (src),
			.msg_control = cbuf,
			.msg_controllen = sizeof (cbuf)
		};

		ssize_t len = recvmsg (fd, &msg, 0);

		/* Sanity checks */
		if ((len < (ssize_t)sizeof (icmp6)) /* error or too small packet */
		 || (msg.msg_flags & (MSG_TRUNC | MSG_CTRUNC)) /* truncated packet */
		 || !IN6_IS_ADDR_LINKLOCAL (&src.sin6_addr) /* bad source address */
		 || (icmp6.nd_ra_code != 0)) /* unknown ICMPv6 code */
			return -1;

		for (struct cmsghdr *cmsg = CMSG_FIRSTHDR (&msg);
		     cmsg != NULL;
		     cmsg = CMSG_NXTHDR (&msg, cmsg))
		{
			if ((cmsg->cmsg_level == IPPROTO_IPV6)
			 && (cmsg->cmsg_type == IPV6_HOPLIMIT)
			 && (255 != *(int *)CMSG_DATA (cmsg)))  /* illegal hop limit */
				return -1;
		}

		/* Parses RA prefix flag, check m & o bit are set or not (IPv6 spec 1.12) */
		if (icmp6.nd_ra_router_lifetime == 0)
		{
			mylog("IPv6 router lifetime reach 0");
			system("rc rc_service stop_ipv6");
			last_flag = 1;
			return 0;
		}

		flags = icmp6.nd_ra_flags_reserved & (ND_RA_FLAG_MANAGED | ND_RA_FLAG_OTHER);
		if (flags != last_flag)
		{
			if (last_flag != 1)
				system("rc rc_service stop_ipv6");
			mylog("Get IPv6 address from %s & DNS from %s",
				(flags & ND_RA_FLAG_MANAGED) ? "DHCPv6" : "RA",
				(flags & (ND_RA_FLAG_MANAGED | ND_RA_FLAG_OTHER)) ? "DHCPv6" : "RA");
			if (flags & ND_RA_FLAG_MANAGED)
				nvram_set("ipv6_ra_conf", "mset");
			else if (flags & ND_RA_FLAG_OTHER)
				nvram_set("ipv6_ra_conf", "oset");
			else
				nvram_set("ipv6_ra_conf", "noneset");
			system("rc rc_service start_dhcp6c");
			last_flag = flags;

			if ((flags & (ND_RA_FLAG_MANAGED | ND_RA_FLAG_OTHER)) == 0 &&
			    nvram_match("ipv6_dnsenable", "1"))
			{

		/* Parses RA options */
		len -= sizeof (icmp6);
		return parse_nd_opts((struct nd_opt_hdr *) buf, len, src.sin6_scope_id);

			}
		}

		return 0;
}

static int icmp_socket()
{
	int fd = socket (AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
	if (fd == -1)
	{
		syslog (LOG_CRIT, _("cannot open ICMPv6 socket"));
		return -1;
	}

	/* set ICMPv6 filter */
	{
		struct icmp6_filter f;

		ICMP6_FILTER_SETBLOCKALL (&f);
		ICMP6_FILTER_SETPASS (ND_ROUTER_ADVERT, &f);
		setsockopt (fd, SOL_ICMPV6, ICMP6_FILTER, &f, sizeof (f));
	}

	setsockopt (fd, SOL_IPV6, IPV6_RECVHOPLIMIT, &(int){ 1 }, sizeof (int));

	return fd;
}

const rdnss_src_t rdnss_icmp = { icmp_socket, icmp_recv };
