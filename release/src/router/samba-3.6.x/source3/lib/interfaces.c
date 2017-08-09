/*
   Unix SMB/CIFS implementation.
   return a list of network interfaces
   Copyright (C) Andrew Tridgell 1998
   Copyright (C) Jeremy Allison 2007

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "includes.h"
#include "interfaces.h"

/****************************************************************************
 Create a struct sockaddr_storage with the netmask bits set to 1.
****************************************************************************/

bool make_netmask(struct sockaddr_storage *pss_out,
			const struct sockaddr_storage *pss_in,
			unsigned long masklen)
{
	*pss_out = *pss_in;
	/* Now apply masklen bits of mask. */
#if defined(HAVE_IPV6)
	if (pss_in->ss_family == AF_INET6) {
		char *p = (char *)&((struct sockaddr_in6 *)pss_out)->sin6_addr;
		unsigned int i;

		if (masklen > 128) {
			return false;
		}
		for (i = 0; masklen >= 8; masklen -= 8, i++) {
			*p++ = 0xff;
		}
		/* Deal with the partial byte. */
		*p++ &= (0xff & ~(0xff>>masklen));
		i++;
		for (;i < sizeof(struct in6_addr); i++) {
			*p++ = '\0';
		}
		return true;
	}
#endif
	if (pss_in->ss_family == AF_INET) {
		if (masklen > 32) {
			return false;
		}
		((struct sockaddr_in *)pss_out)->sin_addr.s_addr =
			htonl(((0xFFFFFFFFL >> masklen) ^ 0xFFFFFFFFL));
		return true;
	}
	return false;
}

/****************************************************************************
 Create a struct sockaddr_storage set to the broadcast or network adress from
 an incoming sockaddr_storage.
****************************************************************************/

static void make_bcast_or_net(struct sockaddr_storage *pss_out,
			const struct sockaddr_storage *pss_in,
			const struct sockaddr_storage *nmask,
			bool make_bcast_p)
{
	unsigned int i = 0, len = 0;
	char *pmask = NULL;
	char *p = NULL;
	*pss_out = *pss_in;

	/* Set all zero netmask bits to 1. */
#if defined(HAVE_IPV6)
	if (pss_in->ss_family == AF_INET6) {
		p = (char *)&((struct sockaddr_in6 *)pss_out)->sin6_addr;
		pmask = (char *)&((struct sockaddr_in6 *)nmask)->sin6_addr;
		len = 16;
	}
#endif
	if (pss_in->ss_family == AF_INET) {
		p = (char *)&((struct sockaddr_in *)pss_out)->sin_addr;
		pmask = (char *)&((struct sockaddr_in *)nmask)->sin_addr;
		len = 4;
	}

	for (i = 0; i < len; i++, p++, pmask++) {
		if (make_bcast_p) {
			*p = (*p & *pmask) | (*pmask ^ 0xff);
		} else {
			/* make_net */
			*p = (*p & *pmask);
		}
	}
}

void make_bcast(struct sockaddr_storage *pss_out,
			const struct sockaddr_storage *pss_in,
			const struct sockaddr_storage *nmask)
{
	make_bcast_or_net(pss_out, pss_in, nmask, true);
}

void make_net(struct sockaddr_storage *pss_out,
			const struct sockaddr_storage *pss_in,
			const struct sockaddr_storage *nmask)
{
	make_bcast_or_net(pss_out, pss_in, nmask, false);
}

/****************************************************************************
 Try the "standard" getifaddrs/freeifaddrs interfaces.
 Also gets IPv6 interfaces.
****************************************************************************/

/****************************************************************************
 Get the netmask address for a local interface.
****************************************************************************/

static int _get_interfaces(TALLOC_CTX *mem_ctx, struct iface_struct **pifaces)
{
	struct iface_struct *ifaces;
	struct ifaddrs *iflist = NULL;
	struct ifaddrs *ifptr = NULL;
	int count;
	int total = 0;
	size_t copy_size;

	if (getifaddrs(&iflist) < 0) {
		return -1;
	}

	count = 0;
	for (ifptr = iflist; ifptr != NULL; ifptr = ifptr->ifa_next) {
		if (!ifptr->ifa_addr || !ifptr->ifa_netmask) {
			continue;
		}
		if (!(ifptr->ifa_flags & IFF_UP)) {
			continue;
		}
		count += 1;
	}

	ifaces = talloc_array(mem_ctx, struct iface_struct, count);
	if (ifaces == NULL) {
		errno = ENOMEM;
		return -1;
	}

	/* Loop through interfaces, looking for given IP address */
	for (ifptr = iflist; ifptr != NULL; ifptr = ifptr->ifa_next) {

		if (!ifptr->ifa_addr || !ifptr->ifa_netmask) {
			continue;
		}

		/* Check the interface is up. */
		if (!(ifptr->ifa_flags & IFF_UP)) {
			continue;
		}

		memset(&ifaces[total], '\0', sizeof(ifaces[total]));

		copy_size = sizeof(struct sockaddr_in);

		ifaces[total].flags = ifptr->ifa_flags;

#if defined(HAVE_IPV6)
		if (ifptr->ifa_addr->sa_family == AF_INET6) {
			copy_size = sizeof(struct sockaddr_in6);
		}
#endif

		memcpy(&ifaces[total].ip, ifptr->ifa_addr, copy_size);
		memcpy(&ifaces[total].netmask, ifptr->ifa_netmask, copy_size);

		if (ifaces[total].flags & (IFF_BROADCAST|IFF_LOOPBACK)) {
			make_bcast(&ifaces[total].bcast,
				&ifaces[total].ip,
				&ifaces[total].netmask);
		} else if ((ifaces[total].flags & IFF_POINTOPOINT) &&
			       ifptr->ifa_dstaddr ) {
			memcpy(&ifaces[total].bcast,
				ifptr->ifa_dstaddr,
				copy_size);
		} else {
			continue;
		}

		strlcpy(ifaces[total].name, ifptr->ifa_name,
			sizeof(ifaces[total].name));
		total++;
	}

	freeifaddrs(iflist);

	*pifaces = ifaces;
	return total;
}

static int iface_comp(struct iface_struct *i1, struct iface_struct *i2)
{
	int r;

#if defined(HAVE_IPV6)
	/*
	 * If we have IPv6 - sort these interfaces lower
	 * than any IPv4 ones.
	 */
	if (i1->ip.ss_family == AF_INET6 &&
			i2->ip.ss_family == AF_INET) {
		return -1;
	} else if (i1->ip.ss_family == AF_INET &&
			i2->ip.ss_family == AF_INET6) {
		return 1;
	}

	if (i1->ip.ss_family == AF_INET6) {
		struct sockaddr_in6 *s1 = (struct sockaddr_in6 *)&i1->ip;
		struct sockaddr_in6 *s2 = (struct sockaddr_in6 *)&i2->ip;

		r = memcmp(&s1->sin6_addr,
				&s2->sin6_addr,
				sizeof(struct in6_addr));
		if (r) {
			return r;
		}

		s1 = (struct sockaddr_in6 *)&i1->netmask;
		s2 = (struct sockaddr_in6 *)&i2->netmask;

		r = memcmp(&s1->sin6_addr,
				&s2->sin6_addr,
				sizeof(struct in6_addr));
		if (r) {
			return r;
		}
	}
#endif

	/* AIX uses __ss_family instead of ss_family inside of
	   sockaddr_storage. Instead of trying to figure out which field to
	   use, we can just cast it to a sockaddr.
	 */

	if (((struct sockaddr *)&i1->ip)->sa_family == AF_INET) {
		struct sockaddr_in *s1 = (struct sockaddr_in *)&i1->ip;
		struct sockaddr_in *s2 = (struct sockaddr_in *)&i2->ip;

		r = ntohl(s1->sin_addr.s_addr) -
			ntohl(s2->sin_addr.s_addr);
		if (r) {
			return r;
		}

		s1 = (struct sockaddr_in *)&i1->netmask;
		s2 = (struct sockaddr_in *)&i2->netmask;

		return ntohl(s1->sin_addr.s_addr) -
			ntohl(s2->sin_addr.s_addr);
	}
	return 0;
}

/* this wrapper is used to remove duplicates from the interface list generated
   above */
int get_interfaces(TALLOC_CTX *mem_ctx, struct iface_struct **pifaces)
{
	struct iface_struct *ifaces;
	int total, i, j;

	total = _get_interfaces(mem_ctx, &ifaces);
	if (total <= 0) return total;

	/* now we need to remove duplicates */
	TYPESAFE_QSORT(ifaces, total, iface_comp);

	for (i=1;i<total;) {
		if (iface_comp(&ifaces[i-1], &ifaces[i]) == 0) {
			for (j=i-1;j<total-1;j++) {
				ifaces[j] = ifaces[j+1];
			}
			total--;
		} else {
			i++;
		}
	}

	*pifaces = ifaces;
	return total;
}

