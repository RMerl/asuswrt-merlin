/*
 *
 *   Authors:
 *    Lars Fenneberg		<lf@elemental.net>
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

#ifndef IPV6_ADDR_LINKLOCAL
#define IPV6_ADDR_LINKLOCAL   0x0020U
#endif

/*
 * this function gets the hardware type and address of an interface,
 * determines the link layer token length and checks it against
 * the defined prefixes
 */
int
setup_deviceinfo(struct Interface *iface)
{
	struct ifreq	ifr;
	struct AdvPrefix *prefix;
	char zero[sizeof(iface->if_addr)];

	strncpy(ifr.ifr_name, iface->Name, IFNAMSIZ-1);
	ifr.ifr_name[IFNAMSIZ-1] = '\0';

	if (ioctl(sock, SIOCGIFMTU, &ifr) < 0) {
		flog(LOG_ERR, "ioctl(SIOCGIFMTU) failed for %s: %s",
			iface->Name, strerror(errno));
		return (-1);
	}

	dlog(LOG_DEBUG, 3, "mtu for %s is %d", iface->Name, ifr.ifr_mtu);
	iface->if_maxmtu = ifr.ifr_mtu;

	if (ioctl(sock, SIOCGIFHWADDR, &ifr) < 0)
	{
		flog(LOG_ERR, "ioctl(SIOCGIFHWADDR) failed for %s: %s",
			iface->Name, strerror(errno));
		return (-1);
	}

	dlog(LOG_DEBUG, 3, "hardware type for %s is %d", iface->Name,
		ifr.ifr_hwaddr.sa_family);

	switch(ifr.ifr_hwaddr.sa_family)
        {
	case ARPHRD_ETHER:
		iface->if_hwaddr_len = 48;
		iface->if_prefix_len = 64;
		break;
#ifdef ARPHRD_FDDI
	case ARPHRD_FDDI:
		iface->if_hwaddr_len = 48;
		iface->if_prefix_len = 64;
		break;
#endif /* ARPHDR_FDDI */
#ifdef ARPHRD_ARCNET
	case ARPHRD_ARCNET:
		iface->if_hwaddr_len = 8;
		iface->if_prefix_len = -1;
		iface->if_maxmtu = -1;
		break;
#endif /* ARPHDR_ARCNET */
	default:
		iface->if_hwaddr_len = -1;
		iface->if_prefix_len = -1;
		iface->if_maxmtu = -1;
		break;
	}

	dlog(LOG_DEBUG, 3, "link layer token length for %s is %d", iface->Name,
		iface->if_hwaddr_len);

	dlog(LOG_DEBUG, 3, "prefix length for %s is %d", iface->Name,
		iface->if_prefix_len);

	if (iface->if_hwaddr_len != -1) {
		unsigned int if_hwaddr_len_bytes = (iface->if_hwaddr_len + 7) >> 3;

		if (if_hwaddr_len_bytes > sizeof(iface->if_hwaddr)) {
			flog(LOG_ERR, "address length %d too big for %s", if_hwaddr_len_bytes, iface->Name);
			return(-2);
		}
		memcpy(iface->if_hwaddr, ifr.ifr_hwaddr.sa_data, if_hwaddr_len_bytes);

		memset(zero, 0, sizeof(zero));
		if (!memcmp(iface->if_hwaddr, zero, if_hwaddr_len_bytes))
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

	return (0);
}

/*
 * this function extracts the link local address and interface index
 * from PATH_PROC_NET_IF_INET6.  Note: 'sock' unused in Linux.
 */
int setup_linklocal_addr(struct Interface *iface)
{
	FILE *fp;
	char str_addr[40];
	unsigned int plen, scope, dad_status, if_idx;
	char devname[IFNAMSIZ];

	if ((fp = fopen(PATH_PROC_NET_IF_INET6, "r")) == NULL)
	{
		flog(LOG_ERR, "can't open %s: %s", PATH_PROC_NET_IF_INET6,
			strerror(errno));
		return (-1);
	}

	while (fscanf(fp, "%32s %x %02x %02x %02x %15s\n",
		      str_addr, &if_idx, &plen, &scope, &dad_status,
		      devname) != EOF)
	{
		if (scope == IPV6_ADDR_LINKLOCAL &&
		    strcmp(devname, iface->Name) == 0)
		{
			struct in6_addr addr;
			unsigned int ap;
			int i;

			for (i=0; i<16; i++)
			{
				sscanf(str_addr + i * 2, "%02x", &ap);
				addr.s6_addr[i] = (unsigned char)ap;
			}
			memcpy(&iface->if_addr, &addr, sizeof(iface->if_addr));

			iface->if_index = if_idx;
			fclose(fp);
			return 0;
		}
	}

	flog(LOG_ERR, "no linklocal address configured for %s", iface->Name);
	fclose(fp);
	return (-1);
}

int setup_allrouters_membership(struct Interface *iface)
{
	struct ipv6_mreq mreq;

	memset(&mreq, 0, sizeof(mreq));
	mreq.ipv6mr_interface = iface->if_index;

	/* ipv6-allrouters: ff02::2 */
	mreq.ipv6mr_multiaddr.s6_addr32[0] = htonl(0xFF020000);
	mreq.ipv6mr_multiaddr.s6_addr32[3] = htonl(0x2);

	if (setsockopt(sock, SOL_IPV6, IPV6_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
	{
		/* linux-2.6.12-bk4 returns error with HUP signal but keep listening */
		if (errno != EADDRINUSE)
		{
			flog(LOG_ERR, "can't join ipv6-allrouters on %s", iface->Name);
			return (-1);
		}
	}

	return (0);
}

int check_allrouters_membership(struct Interface *iface)
{
	#define ALL_ROUTERS_MCAST "ff020000000000000000000000000002"

	FILE *fp;
	unsigned int if_idx, allrouters_ok=0;
	char addr[32+1];
	char buffer[301] = {""}, *str;
	int ret=0;

	if ((fp = fopen(PATH_PROC_NET_IGMP6, "r")) == NULL)
	{
		flog(LOG_ERR, "can't open %s: %s", PATH_PROC_NET_IGMP6,
			strerror(errno));
		return (-1);
	}

	str = fgets(buffer, 300, fp);

	while (str && (ret = sscanf(str, "%u %*s %32[0-9A-Fa-f]", &if_idx, addr)) ) {
		if (ret == 2) {
			if (iface->if_index == if_idx) {
				if (strncmp(addr, ALL_ROUTERS_MCAST, sizeof(addr)) == 0){
					allrouters_ok = 1;
					break;
				}
			}
		}
		str = fgets(buffer, 300, fp);
	}

	fclose(fp);

	if (!allrouters_ok) {
		flog(LOG_WARNING, "resetting ipv6-allrouters membership on %s", iface->Name);
		return setup_allrouters_membership(iface);
	}

	return(0);
}

/* note: also called from the root context */
int
set_interface_var(const char *iface,
		  const char *var, const char *name,
		  uint32_t val)
{
	FILE *fp;
	char spath[64+IFNAMSIZ];	/* XXX: magic constant */
	if (snprintf(spath, sizeof(spath), var, iface) >= sizeof(spath))
		return -1;

	/* No path traversal */
	if (!iface[0] || !strcmp(iface, ".") || !strcmp(iface, "..") || strchr(iface, '/'))
		return -1;

	if (access(spath, F_OK) != 0)
		return -1;

	fp = fopen(spath, "w");
	if (!fp) {
		if (name)
			flog(LOG_ERR, "failed to set %s (%u) for %s: %s",
			     name, val, iface, strerror(errno));
		return -1;
	}
	fprintf(fp, "%u", val);
	fclose(fp);

	return 0;
}

int
set_interface_linkmtu(const char *iface, uint32_t mtu)
{
	return privsep_interface_linkmtu(iface, mtu);
}

int
set_interface_curhlim(const char *iface, uint8_t hlim)
{
	return privsep_interface_curhlim(iface, hlim);
}

int
set_interface_reachtime(const char *iface, uint32_t rtime)
{
	return privsep_interface_reachtime(iface, rtime);
}

int
set_interface_retranstimer(const char *iface, uint32_t rettimer)
{
	return privsep_interface_retranstimer(iface, rettimer);
}

