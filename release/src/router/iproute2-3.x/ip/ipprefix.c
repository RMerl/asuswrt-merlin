/*
 * Copyright (C)2005 USAGI/WIDE Project
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
/*
 * based on ip.c, iproute.c
 */
/*
 * Authors:
 *	Masahide NAKAMURA @USAGI
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/icmp6.h>
#include "utils.h"

/* prefix flags; see kernel's net/ipv6/addrconf.c and include/net/if_inet6.h */
#define IF_PREFIX_ONLINK	0x01
#define IF_PREFIX_AUTOCONF	0x02

int print_prefix(const struct sockaddr_nl *who, struct nlmsghdr *n, void *arg)
{
	FILE *fp = (FILE*)arg;
	struct prefixmsg *prefix = NLMSG_DATA(n);
	int len = n->nlmsg_len;
	struct rtattr * tb[RTA_MAX+1];
	int family = preferred_family;

	if (n->nlmsg_type != RTM_NEWPREFIX) {
		fprintf(stderr, "Not a prefix: %08x %08x %08x\n",
			n->nlmsg_len, n->nlmsg_type, n->nlmsg_flags);
		return 0;
	}

	len -= NLMSG_LENGTH(sizeof(*prefix));
	if (len < 0) {
		fprintf(stderr, "BUG: wrong nlmsg len %d\n", len);
		return -1;
	}

	if (family == AF_UNSPEC)
		family = AF_INET6;
	if (family != AF_INET6)
		return 0;

	if (prefix->prefix_family != AF_INET6) {
		fprintf(stderr, "wrong family %d\n", prefix->prefix_family);
		return 0;
	}
	if (prefix->prefix_type != ND_OPT_PREFIX_INFORMATION) {
		fprintf(stderr, "wrong ND type %d\n", prefix->prefix_type);
		return 0;
	}

	parse_rtattr(tb, RTA_MAX, RTM_RTA(prefix), len);

	fprintf(fp, "prefix ");

	if (tb[PREFIX_ADDRESS]) {
		struct in6_addr *pfx;
		char abuf[256];

		pfx = (struct in6_addr *)RTA_DATA(tb[PREFIX_ADDRESS]);

		memset(abuf, '\0', sizeof(abuf));
		fprintf(fp, "%s", rt_addr_n2a(family, sizeof(*pfx), pfx,
					      abuf, sizeof(abuf)));
	}
	fprintf(fp, "/%u ", prefix->prefix_len);

	fprintf(fp, "dev %s ", ll_index_to_name(prefix->prefix_ifindex));

	if (prefix->prefix_flags & IF_PREFIX_ONLINK)
		fprintf(fp, "onlink ");
	if (prefix->prefix_flags & IF_PREFIX_AUTOCONF)
		fprintf(fp, "autoconf ");

	if (tb[PREFIX_CACHEINFO]) {
		struct prefix_cacheinfo *pc;
		pc = (struct prefix_cacheinfo *)RTA_DATA(tb[PREFIX_CACHEINFO]);

		fprintf(fp, "valid %u ", pc->valid_time);
		fprintf(fp, "preferred %u ", pc->preferred_time);
	}

	fprintf(fp, "\n");
	fflush(fp);

	return 0;
}

