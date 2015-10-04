/* MiniUPnP project
 * http://miniupnp.free.fr/ or http://miniupnp.tuxfamily.org/
 *
 * Copyright (c) 2006, Thomas Bernard
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * The name of the author may not be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#if defined(sun)
#include <sys/sockio.h>
#endif

#include "config.h"
#if HAVE_GETIFADDRS
# include <ifaddrs.h>
# ifdef __linux__
#  ifndef AF_LINK
#   define AF_LINK AF_INET
#  endif
# else
#  include <net/if_dl.h>
# endif
# ifndef IFF_SLAVE
#  define IFF_SLAVE 0
# endif
#endif
#ifdef HAVE_NETLINK
# include <linux/rtnetlink.h>
# include <linux/netlink.h>
#endif
#include "upnpglobalvars.h"
#include "getifaddr.h"
#include "minissdp.h"
#include "utils.h"
#include "log.h"
#ifdef BCMARM
#include "ifaddrs.c"
#endif

static int
getifaddr(const char *ifname)
{
#if HAVE_GETIFADDRS
	struct ifaddrs *ifap, *p;
	struct sockaddr_in *addr_in;

	if (getifaddrs(&ifap) != 0)
	{
		DPRINTF(E_ERROR, L_GENERAL, "getifaddrs(): %s\n", strerror(errno));
		return -1;
	}

	for (p = ifap; p != NULL; p = p->ifa_next)
	{
		if (!p->ifa_addr || p->ifa_addr->sa_family != AF_INET)
			continue;
		if (ifname && strcmp(p->ifa_name, ifname) != 0)
			continue;
		addr_in = (struct sockaddr_in *)p->ifa_addr;
		if (!ifname && (p->ifa_flags & (IFF_LOOPBACK | IFF_SLAVE)))
			continue;
		memcpy(&lan_addr[n_lan_addr].addr, &addr_in->sin_addr, sizeof(lan_addr[n_lan_addr].addr));
		if (!inet_ntop(AF_INET, &addr_in->sin_addr, lan_addr[n_lan_addr].str, sizeof(lan_addr[0].str)) )
		{
			DPRINTF(E_ERROR, L_GENERAL, "inet_ntop(): %s\n", strerror(errno));
			continue;
		}
		addr_in = (struct sockaddr_in *)p->ifa_netmask;
		memcpy(&lan_addr[n_lan_addr].mask, &addr_in->sin_addr, sizeof(lan_addr[n_lan_addr].mask));
		lan_addr[n_lan_addr].ifindex = if_nametoindex(p->ifa_name);
		lan_addr[n_lan_addr].snotify = OpenAndConfSSDPNotifySocket(&lan_addr[n_lan_addr]);
		if (lan_addr[n_lan_addr].snotify >= 0)
			n_lan_addr++;
		if (ifname || n_lan_addr >= MAX_LAN_ADDR)
			break;
	}
	freeifaddrs(ifap);
	if (ifname && !p)
	{
		DPRINTF(E_ERROR, L_GENERAL, "Network interface %s not found\n", ifname);
		return -1;
	}
#else
	int s = socket(PF_INET, SOCK_STREAM, 0);
	struct sockaddr_in addr;
	struct ifconf ifc;
	struct ifreq *ifr;
	char buf[8192];
	int i, n;

	memset(&ifc, '\0', sizeof(ifc));
	ifc.ifc_buf = buf;
	ifc.ifc_len = sizeof(buf);

	if (ioctl(s, SIOCGIFCONF, &ifc) < 0)
	{
		DPRINTF(E_ERROR, L_GENERAL, "SIOCGIFCONF: %s\n", strerror(errno));
		close(s);
		return -1;
	}

	n = ifc.ifc_len / sizeof(struct ifreq);
	for (i = 0; i < n; i++)
	{
		ifr = &ifc.ifc_req[i];
		if (ifname && strcmp(ifr->ifr_name, ifname) != 0)
			continue;
		if (!ifname &&
		    (ioctl(s, SIOCGIFFLAGS, ifr) < 0 || ifr->ifr_ifru.ifru_flags & IFF_LOOPBACK))
			continue;
		if (ioctl(s, SIOCGIFADDR, ifr) < 0)
			continue;
		memcpy(&addr, &(ifr->ifr_addr), sizeof(addr));
		memcpy(&lan_addr[n_lan_addr].addr, &addr.sin_addr, sizeof(lan_addr[n_lan_addr].addr));
		if (!inet_ntop(AF_INET, &addr.sin_addr, lan_addr[n_lan_addr].str, sizeof(lan_addr[0].str)))
		{
			DPRINTF(E_ERROR, L_GENERAL, "inet_ntop(): %s\n", strerror(errno));
			close(s);
			continue;
		}
		if (ioctl(s, SIOCGIFNETMASK, ifr) < 0)
			continue;
		memcpy(&addr, &(ifr->ifr_addr), sizeof(addr));
		memcpy(&lan_addr[n_lan_addr].mask, &addr.sin_addr, sizeof(addr));
		lan_addr[n_lan_addr].ifindex = if_nametoindex(ifr->ifr_name);
		lan_addr[n_lan_addr].snotify = OpenAndConfSSDPNotifySocket(&lan_addr[n_lan_addr]);
		if (lan_addr[n_lan_addr].snotify >= 0)
			n_lan_addr++;
		if (ifname || n_lan_addr >= MAX_LAN_ADDR)
			break;
	}
	close(s);
	if (ifname && i == n)
	{
		DPRINTF(E_ERROR, L_GENERAL, "Network interface %s not found\n", ifname);
		return -1;
	}
#endif
	return n_lan_addr;
}

int
getsyshwaddr(char *buf, int len)
{
	unsigned char mac[6];
	int ret = -1;
#if defined(HAVE_GETIFADDRS) && !defined (__linux__) && !defined (__sun__)
	struct ifaddrs *ifap, *p;
	struct sockaddr_in *addr_in;
	uint8_t a;

	if (getifaddrs(&ifap) != 0)
	{
		DPRINTF(E_ERROR, L_GENERAL, "getifaddrs(): %s\n", strerror(errno));
		return -1;
	}
	for (p = ifap; p != NULL; p = p->ifa_next)
	{
		if (p->ifa_addr && p->ifa_addr->sa_family == AF_LINK)
		{
			addr_in = (struct sockaddr_in *)p->ifa_addr;
			a = (htonl(addr_in->sin_addr.s_addr) >> 0x18) & 0xFF;
			if (a == 127)
				continue;
#if defined(__linux__)
			struct ifreq ifr;
			int fd;
			fd = socket(AF_INET, SOCK_DGRAM, 0);
			if (fd < 0)
				continue;
			strncpy(ifr.ifr_name, p->ifa_name, IFNAMSIZ);
			ret = ioctl(fd, SIOCGIFHWADDR, &ifr);
			close(fd);
			if (ret < 0)
				continue;
			memcpy(mac, ifr.ifr_hwaddr.sa_data, 6);
#else
			struct sockaddr_dl *sdl;
			sdl = (struct sockaddr_dl*)p->ifa_addr;
			memcpy(mac, LLADDR(sdl), sdl->sdl_alen);
#endif
			if (MACADDR_IS_ZERO(mac))
				continue;
			ret = 0;
			break;
		}
	}
	freeifaddrs(ifap);
#else
	struct if_nameindex *ifaces, *if_idx;
	struct ifreq ifr;
	int fd;

	memset(&mac, '\0', sizeof(mac));
	/* Get the spatially unique node identifier */
	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0)
		return ret;

	ifaces = if_nameindex();
	if (!ifaces)
	{
		close(fd);
		return ret;
	}

	for (if_idx = ifaces; if_idx->if_index; if_idx++)
	{
		strncpyt(ifr.ifr_name, if_idx->if_name, IFNAMSIZ);
		if (ioctl(fd, SIOCGIFFLAGS, &ifr) < 0)
			continue;
		if (ifr.ifr_ifru.ifru_flags & IFF_LOOPBACK)
			continue;
		if (ioctl(fd, SIOCGIFHWADDR, &ifr) < 0)
			continue;
#ifdef __sun__
		if (MACADDR_IS_ZERO(ifr.ifr_addr.sa_data))
			continue;
		memcpy(mac, ifr.ifr_addr.sa_data, 6);
#else
		if (MACADDR_IS_ZERO(ifr.ifr_hwaddr.sa_data))
			continue;
		memcpy(mac, ifr.ifr_hwaddr.sa_data, 6);
#endif
		ret = 0;
		break;
	}
	if_freenameindex(ifaces);
	close(fd);
#endif
	if (ret == 0)
	{
		if (len > 12)
			sprintf(buf, "%02x%02x%02x%02x%02x%02x",
			        mac[0]&0xFF, mac[1]&0xFF, mac[2]&0xFF,
			        mac[3]&0xFF, mac[4]&0xFF, mac[5]&0xFF);
		else if (len == 6)
			memmove(buf, mac, 6);
	}
	return ret;
}

int
get_remote_mac(struct in_addr ip_addr, unsigned char *mac)
{
	struct in_addr arp_ent;
	FILE * arp;
	char remote_ip[16];
	int matches, hwtype, flags;
	memset(mac, 0xFF, 6);

	arp = fopen("/proc/net/arp", "r");
	if (!arp)
		return 1;
	while (!feof(arp))
	{
		matches = fscanf(arp, "%15s 0x%8X 0x%8X %2hhx:%2hhx:%2hhx:%2hhx:%2hhx:%2hhx",
		                      remote_ip, &hwtype, &flags,
		                      &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);
		if (matches != 9)
			continue;
		inet_pton(AF_INET, remote_ip, &arp_ent);
		if (ip_addr.s_addr == arp_ent.s_addr)
			break;
		mac[0] = 0xFF;
	}
	fclose(arp);

	if (mac[0] == 0xFF)
	{
		memset(mac, 0xFF, 6);
		return 1;
	}

	return 0;
}

void
reload_ifaces(int force_notify)
{
	struct in_addr old_addr[MAX_LAN_ADDR];
	int i, j;

	memset(&old_addr, 0xFF, sizeof(old_addr));
	for (i = 0; i < n_lan_addr; i++)
	{
		memcpy(&old_addr[i], &lan_addr[i].addr, sizeof(struct in_addr));
		close(lan_addr[i].snotify);
	}
	n_lan_addr = 0;

	i = 0;
	do {
		getifaddr(runtime_vars.ifaces[i]);
		i++;
	} while (i < MAX_LAN_ADDR && runtime_vars.ifaces[i]);

	for (i = 0; i < n_lan_addr; i++)
	{
		for (j = 0; j < MAX_LAN_ADDR; j++)
		{
			if (memcmp(&lan_addr[i].addr, &old_addr[j], sizeof(struct in_addr)) == 0)
				break;
		}
		/* Send out startup notifies if it's a new interface, or on SIGHUP */
		if (force_notify || j == MAX_LAN_ADDR)
		{
			DPRINTF(E_INFO, L_GENERAL, "Enabling interface %s/%s\n",
				lan_addr[i].str, inet_ntoa(lan_addr[i].mask));
			SendSSDPGoodbyes(lan_addr[i].snotify);
			SendSSDPNotifies(lan_addr[i].snotify, lan_addr[i].str,
					runtime_vars.port, runtime_vars.notify_interval);
		}
	}
}

int
OpenAndConfMonitorSocket(void)
{
#ifdef HAVE_NETLINK
	struct sockaddr_nl addr;
	int s;
	int ret;

	s = socket(PF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	if (s < 0)
	{
		perror("couldn't open NETLINK_ROUTE socket");
		return -1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.nl_family = AF_NETLINK;
	addr.nl_groups = RTMGRP_IPV4_IFADDR;

	ret = bind(s, (struct sockaddr*)&addr, sizeof(addr));
	if (ret < 0)
	{
		perror("couldn't bind");
		close(s);
		return -1;
	}

	return s;
#else
	return -1;
#endif
}

void
ProcessMonitorEvent(int s)
{
#ifdef HAVE_NETLINK
	int len;
	char buf[4096];
	struct nlmsghdr *nlh;
	int changed = 0;

	nlh = (struct nlmsghdr*)buf;

	len = recv(s, nlh, sizeof(buf), 0);
	if (len <= 0)
		return;
	while ((NLMSG_OK(nlh, len)) && (nlh->nlmsg_type != NLMSG_DONE))
	{
		if (nlh->nlmsg_type == RTM_NEWADDR ||
		    nlh->nlmsg_type == RTM_DELADDR)
		{
			changed = 1;
		}
		nlh = NLMSG_NEXT(nlh, len);
	}
	if (changed)
		reload_ifaces(0);
#endif
}
