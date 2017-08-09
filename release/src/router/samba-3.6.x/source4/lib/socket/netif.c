/* 
   Unix SMB/CIFS implementation.
   return a list of network interfaces
   Copyright (C) Andrew Tridgell 1998
   Copyright (C) Jeremy Allison 2007
   Copyright (C) Jelmer Vernooij 2007
   
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


/* working out the interfaces for a OS is an incredibly non-portable
   thing. We have several possible implementations below, and autoconf
   tries each of them to see what works

   Note that this file does _not_ include includes.h. That is so this code
   can be called directly from the autoconf tests. That also means
   this code cannot use any of the normal Samba debug stuff or defines.
   This is standalone code.

*/

#include "includes.h"
#include "system/network.h"
#include "netif.h"
#include "lib/util/tsort.h"

/****************************************************************************
 Try the "standard" getifaddrs/freeifaddrs interfaces.
 Also gets IPv6 interfaces.
****************************************************************************/

/****************************************************************************
 Get the netmask address for a local interface.
****************************************************************************/

static int _get_interfaces(struct iface_struct *ifaces, int max_interfaces)
{
	struct ifaddrs *iflist = NULL;
	struct ifaddrs *ifptr = NULL;
	int total = 0;

	if (getifaddrs(&iflist) < 0) {
		return -1;
	}

	/* Loop through interfaces, looking for given IP address */
	for (ifptr = iflist, total = 0;
			ifptr != NULL && total < max_interfaces;
			ifptr = ifptr->ifa_next) {

		memset(&ifaces[total], '\0', sizeof(ifaces[total]));

		if (!ifptr->ifa_addr || !ifptr->ifa_netmask) {
			continue;
		}

		/* Check the interface is up. */
		if (!(ifptr->ifa_flags & IFF_UP)) {
			continue;
		}

		/* We don't support IPv6 *yet* */
		if (ifptr->ifa_addr->sa_family != AF_INET) {
			continue;
		}

		ifaces[total].ip = ((struct sockaddr_in *)ifptr->ifa_addr)->sin_addr;
		ifaces[total].netmask = ((struct sockaddr_in *)ifptr->ifa_netmask)->sin_addr;

		strlcpy(ifaces[total].name, ifptr->ifa_name,
			sizeof(ifaces[total].name));
		total++;
	}

	freeifaddrs(iflist);

	return total;
}

static int iface_comp(struct iface_struct *i1, struct iface_struct *i2)
{
	int r;
	r = strcmp(i1->name, i2->name);
	if (r) return r;
	r = ntohl(i1->ip.s_addr) - ntohl(i2->ip.s_addr);
	if (r) return r;
	r = ntohl(i1->netmask.s_addr) - ntohl(i2->netmask.s_addr);
	return r;
}

/* this wrapper is used to remove duplicates from the interface list generated
   above */
int get_interfaces(struct iface_struct *ifaces, int max_interfaces)
{
	int total, i, j;

	total = _get_interfaces(ifaces, max_interfaces);
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

	return total;
}
