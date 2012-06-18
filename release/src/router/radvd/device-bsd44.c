/*
 *
 *   Authors:
 *    Craig Metz		<cmetz@inner.net>
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
#include "includes.h"
#include "radvd.h"
#include "defaults.h"
#include "pathnames.h"		/* for PATH_PROC_NET_IF_INET6 */

static uint8_t ll_prefix[] = { 0xfe, 0x80 };

/*
 * this function gets the hardware type and address of an interface,
 * determines the link layer token length and checks it against
 * the defined prefixes
 */
int
setup_deviceinfo(struct Interface *iface)
{
	struct ifaddrs *addresses = 0, *ifa;

	struct ifreq ifr;
	struct AdvPrefix *prefix;
	char zero[sizeof(iface->if_addr)];

	if(if_nametoindex(iface->Name) == 0){
		flog(LOG_ERR, "%s not found: %s", iface->Name, strerror(errno));
		goto ret;
	}

 	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, iface->Name, IFNAMSIZ-1);
	ifr.ifr_name[IFNAMSIZ-1] = '\0';

	if (ioctl(sock, SIOCGIFMTU, &ifr) < 0) {
		flog(LOG_ERR, "ioctl(SIOCGIFMTU) failed for %s: %s", iface->Name, strerror(errno));
		goto ret;
	}

	dlog(LOG_DEBUG, 3, "mtu for %s is %d", iface->Name, ifr.ifr_mtu);
	iface->if_maxmtu = ifr.ifr_mtu;

	if (getifaddrs(&addresses) != 0)
	{
		flog(LOG_ERR, "getifaddrs failed: %s(%d)", strerror(errno), errno);
		goto ret;
	}

	for (ifa = addresses; ifa != NULL; ifa = ifa->ifa_next)
	{
		if (strcmp(ifa->ifa_name, iface->Name) != 0)
			continue;

		if (ifa->ifa_addr == NULL)
			continue;

		if (ifa->ifa_addr->sa_family != AF_LINK)
			continue;

		struct sockaddr_dl *dl = (struct sockaddr_dl*)ifa->ifa_addr;


		if (dl->sdl_alen > sizeof(iface->if_addr))
		{
			flog(LOG_ERR, "address length %d too big for",
				dl->sdl_alen,
				iface->Name);
			goto ret;
		}

		memcpy(iface->if_hwaddr, LLADDR(dl), dl->sdl_alen);
		iface->if_hwaddr_len = dl->sdl_alen << 3;

		switch(dl->sdl_type) {
		case IFT_ETHER:
		case IFT_ISO88023:
			iface->if_prefix_len = 64;
			break;
		case IFT_FDDI:
			iface->if_prefix_len = 64;
			break;
		default:
			iface->if_prefix_len = -1;
			iface->if_maxmtu = -1;
			break;
		}

		dlog(LOG_DEBUG, 3, "link layer token length for %s is %d", iface->Name,
			iface->if_hwaddr_len);

		dlog(LOG_DEBUG, 3, "prefix length for %s is %d", iface->Name,
			iface->if_prefix_len);

		if (iface->if_prefix_len != -1) {
			memset(zero, 0, dl->sdl_alen);
			if (!memcmp(iface->if_hwaddr, zero, dl->sdl_alen))
				flog(LOG_WARNING, "WARNING, MAC address on %s is all zero!",
					iface->Name);
		}

		prefix = iface->AdvPrefixList;
		while (prefix)
		{
			if ((iface->if_prefix_len != -1) &&
				(iface->if_prefix_len != prefix->PrefixLen))
			{
				flog(LOG_WARNING, "prefix length should be %d for %s",
					iface->if_prefix_len, iface->Name);
			}

			prefix = prefix->next;
		}

		freeifaddrs(addresses);
		return 0;
	}


ret:
	iface->if_maxmtu = -1;
	iface->if_hwaddr_len = -1;
	iface->if_prefix_len = -1;
	if (addresses != 0)
		freeifaddrs(addresses);
	return -1;
}

/*
 * Saves the first link local address seen on the specified interface to iface->if_addr
 *
 */
int setup_linklocal_addr(struct Interface *iface)
{
	struct ifaddrs *addresses = 0, *ifa;

	if (getifaddrs(&addresses) != 0)
	{
		flog(LOG_ERR, "getifaddrs failed: %s(%d)", strerror(errno), errno);
		goto ret;
	}

	for (ifa = addresses; ifa != NULL; ifa = ifa->ifa_next)
	{
		if (strcmp(ifa->ifa_name, iface->Name) != 0)
			continue;

		if (ifa->ifa_addr == NULL)
			continue;

		if (ifa->ifa_addr->sa_family == AF_LINK) {
			struct sockaddr_dl *dl = (struct sockaddr_dl*)ifa->ifa_addr;
			if (memcmp(iface->Name, dl->sdl_data, dl->sdl_nlen) == 0)
				iface->if_index = dl->sdl_index;
			continue;
		}

		if (ifa->ifa_addr->sa_family != AF_INET6)
			continue;

		struct sockaddr_in6 *a6 = (struct sockaddr_in6*)ifa->ifa_addr;

		/* Skip if it is not a linklocal address */
		if (memcmp(&(a6->sin6_addr), ll_prefix, sizeof(ll_prefix)) != 0)
			continue;

		memcpy(&iface->if_addr, &(a6->sin6_addr), sizeof(struct in6_addr));
		freeifaddrs(addresses);
		return 0;
	}

ret:
	if(addresses)
		freeifaddrs(addresses);
	flog(LOG_ERR, "no linklocal address configured for %s", iface->Name);
	return -1;
}

int setup_allrouters_membership(struct Interface *iface)
{
	return (0);
}

int check_allrouters_membership(struct Interface *iface)
{
	return (0);
}

int
set_interface_linkmtu(const char *iface, uint32_t mtu)
{
	dlog(LOG_DEBUG, 4, "setting LinkMTU (%u) for %s is not supported",
	     mtu, iface);
	return -1;
}

int
set_interface_curhlim(const char *iface, uint8_t hlim)
{
	dlog(LOG_DEBUG, 4, "setting CurHopLimit (%u) for %s is not supported",
	     hlim, iface);
	return -1;
}

int
set_interface_reachtime(const char *iface, uint32_t rtime)
{
	dlog(LOG_DEBUG, 4, "setting BaseReachableTime (%u) for %s is not supported",
	     rtime, iface);
	return -1;
}

int
set_interface_retranstimer(const char *iface, uint32_t rettimer)
{
	dlog(LOG_DEBUG, 4, "setting RetransTimer (%u) for %s is not supported",
	     rettimer, iface);
	return -1;
}

