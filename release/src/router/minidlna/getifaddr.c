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

#include "upnpglobalvars.h"
#include "getifaddr.h"
#include "log.h"

int
getifaddr(const char * ifname, char * buf, int len)
{
	int s;
	struct ifreq ifr;
	int ifrlen;
	struct sockaddr_in * addr;
	uint32_t mask;
	int i;

	ifrlen = sizeof(ifr);
	s = socket(PF_INET, SOCK_DGRAM, 0);
	if(s < 0)
	{
		DPRINTF(E_ERROR, L_GENERAL, "socket: %s\n", strerror(errno));
		return -1;
	}
	strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
	if(ioctl(s, SIOCGIFADDR, &ifr, &ifrlen) < 0)
	{
		/* Interface is up but has no address (unconfigured) */
		DPRINTF(E_INFO, L_GENERAL, "SIOCGIFADDR %s: %s\n", ifname, strerror(errno));
		close(s);
		return -1;
	}
	addr = (struct sockaddr_in *)&ifr.ifr_addr;
	if(!inet_ntop(AF_INET, &addr->sin_addr, buf, len))
	{
		DPRINTF(E_ERROR, L_GENERAL, "inet_ntop(): %s\n", strerror(errno));
		close(s);
		return -1;
	}
	if(ioctl(s, SIOCGIFNETMASK, &ifr, &ifrlen) == 0)
	{
		addr = (struct sockaddr_in *)&ifr.ifr_netmask;
		mask = ntohl(addr->sin_addr.s_addr);
		for (i = 0; i < 32; i++)
		{
			if ((mask >> i) & 1)
				break;
		}
		mask = 32 - i;
		if (mask)
		{
			i = strlen(buf);
			snprintf(buf+i, len-i, "/%u", mask);
		}
	}
	else
		DPRINTF(E_ERROR, L_GENERAL, "ioctl(s, SIOCGIFNETMASK, ...): %s\n", strerror(errno));
	close(s);
	return 0;
}

int
getsysaddrs(void)
{
	int s = socket(PF_INET, SOCK_STREAM, 0);
	struct sockaddr_in addr;
	struct ifconf ifc;
	struct ifreq *ifr;
	char buf[8192];
	int n, i;

	memset(&ifc, '\0', sizeof(ifc));
	ifc.ifc_buf = buf;
	ifc.ifc_len = sizeof(buf);

	if( ioctl(s, SIOCGIFCONF, &ifc) < 0 )
	{
		DPRINTF(E_ERROR, L_GENERAL, "SIOCGIFCONF: %s\n", strerror(errno));
		close(s);
		return -1;
	}

	n = ifc.ifc_len / sizeof(struct ifreq);
	for (i=0; i < n; i++)
	{
		ifr = &ifc.ifc_req[i];
		if( ioctl(s, SIOCGIFFLAGS, ifr) < 0 ||
		    ifr->ifr_ifru.ifru_flags & IFF_LOOPBACK)
			continue;
		if( ioctl(s, SIOCGIFADDR, ifr) < 0 )
			continue;
		memcpy(&addr, &(ifr->ifr_addr), sizeof(addr));
		memcpy(&lan_addr[n_lan_addr].addr, &addr.sin_addr, sizeof(lan_addr[n_lan_addr].addr));
		if( !inet_ntop(AF_INET, &addr.sin_addr, lan_addr[n_lan_addr].str, sizeof(lan_addr[0].str)) )
		{
			DPRINTF(E_ERROR, L_GENERAL, "inet_ntop(): %s\n", strerror(errno));
			close(s);
			continue;
		}
		if(ioctl(s, SIOCGIFNETMASK, ifr) < 0)
			continue;
		memcpy(&addr, &(ifr->ifr_addr), sizeof(addr));
		memcpy(&lan_addr[n_lan_addr].mask, &addr.sin_addr, sizeof(addr));
		n_lan_addr++;
		if (n_lan_addr >= MAX_LAN_ADDR)
			break;
	}
	close(s);

	return(n_lan_addr);
}

int
getsyshwaddr(char * buf, int len)
{
	struct if_nameindex *ifaces, *if_idx;
	unsigned char mac[6];
	struct ifreq ifr;
	int fd;
	int ret = -1;

	memset(&mac, '\0', sizeof(mac));
	/* Get the spatially unique node identifier */
	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if( fd < 0 )
		return(ret);

	ifaces = if_nameindex();
	if(!ifaces)
		return(ret);

	for(if_idx = ifaces; if_idx->if_index; if_idx++)
	{
		strncpy(ifr.ifr_name, if_idx->if_name, IFNAMSIZ);
		if(ioctl(fd, SIOCGIFFLAGS, &ifr) < 0)
			continue;
		if(ifr.ifr_ifru.ifru_flags & IFF_LOOPBACK)
			continue;
		if( ioctl(fd, SIOCGIFHWADDR, &ifr) < 0 )
			continue;
		if( MACADDR_IS_ZERO(ifr.ifr_hwaddr.sa_data) )
			continue;
		ret = 0;
		break;
	}
	if_freenameindex(ifaces);
	close(fd);

	if(ret == 0)
	{
		if(len > 12)
		{
			memmove(mac, ifr.ifr_hwaddr.sa_data, 6);
			sprintf(buf, "%02x%02x%02x%02x%02x%02x",
			        mac[0]&0xFF, mac[1]&0xFF, mac[2]&0xFF,
			        mac[3]&0xFF, mac[4]&0xFF, mac[5]&0xFF);
		}
		else if(len == 6)
		{
			memmove(buf, ifr.ifr_hwaddr.sa_data, 6);
		}
	}
	return ret;
}

int
get_remote_mac(struct in_addr ip_addr, unsigned char * mac)
{
	struct in_addr arp_ent;
	FILE * arp;
	char remote_ip[16];
	int matches, hwtype, flags;
	memset(mac, 0xFF, 6);

 	arp = fopen("/proc/net/arp", "r");
	if( !arp )
		return 1;
	while( !feof(arp) )
	{
	        matches = fscanf(arp, "%15s 0x%8X 0x%8X %2hhx:%2hhx:%2hhx:%2hhx:%2hhx:%2hhx",
		                      remote_ip, &hwtype, &flags,
		                      &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);
		if( matches != 9 )
			continue;
		inet_pton(AF_INET, remote_ip, &arp_ent);
		if( ip_addr.s_addr == arp_ent.s_addr )
			break;
		mac[0] = 0xFF;
	}
	fclose(arp);

	if( mac[0] == 0xFF )
	{
		memset(mac, 0xFF, 6);
		return 1;
	}

	return 0;
}
