/*
	Copyright 2003, CyberTAN  Inc.  All Rights Reserved

	This is UNPUBLISHED PROPRIETARY SOURCE CODE of CyberTAN Inc.
	the contents of this file may not be disclosed to third parties,
	copied or duplicated in any form without the prior written
	permission of CyberTAN Inc.

	This software should be used as a reference only, and it not
	intended for production use!

	THIS SOFTWARE IS OFFERED "AS IS", AND CYBERTAN GRANTS NO WARRANTIES OF ANY
	KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE.  CYBERTAN
	SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
	FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <net/if.h>
#include <dirent.h>
#include <unistd.h>
#include <ctype.h>
#include <syslog.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <sys/ioctl.h>
#include <sys/sysinfo.h>

#include <utils.h>
#include <bcmnvram.h>
#include <shutils.h>
#include <bcmdevs.h>
#include <net/route.h>
#include <bcmdevs.h>

#include <linux/if_ether.h>
#include <linux/sockios.h>

#include "shared.h"


// -----------------------------------------------------------------------------


#if 0	// beyond here are obsolete functions

struct wl_assoc_mac *get_wl_assoc_mac(int *c)
{
	FILE *f;
	struct wl_assoc_mac *wlmac;
	int count;
	char line[80];
	char mac[20];

	wlmac = NULL;
    count = 0;

	if ((f = popen("wl assoclist", "r")) != NULL) {
		// assoclist 00:11:22:33:44:55
		while ((fgets(line, sizeof(line), f)) != NULL) {
			if (sscanf(line,"assoclist %17s", mac) != 1) continue;

			wlmac = realloc(wlmac, sizeof(wlmac[0]) * (count + 1));
			if (!wlmac) {
				count = 0;
				break;
			}

			memset(&wlmac[count], 0, sizeof(wlmac[0]));
			strlcpy(wlmac[count].mac, mac, sizeof(wlmac[0].mac));
			++count;
		}

		pclose(f);
	}

	*c = count;
	return wlmac;
}


/*
void
show_hw_type(int type)
{
	if(type == BCM4702_CHIP)
		cprintf("The chipset is BCM4702\n");
	else if(type == BCM5325E_CHIP)
		cprintf("The chipset is BCM4712L + BCM5325E\n");
	else if(type == BCM4704_BCM5325F_CHIP)
		cprintf("The chipset is BCM4704 + BCM5325F\n");
	else if(type == BCM5352E_CHIP)
		cprintf("The chipset is BCM5352E\n");
	else if(type == BCM4712_CHIP)
		cprintf("The chipset is BCM4712 + ADMtek\n");
	else
		cprintf("The chipset is not defined\n");
}
*/


#define SIOCGMIIREG	0x8948          /* Read MII PHY register.       */
#define SIOCSMIIREG	0x8949          /* Write MII PHY register.      */

struct mii_ioctl_data {
	unsigned short		phy_id;
	unsigned short		reg_num;
	unsigned short		val_in;
	unsigned short		val_out;
};


unsigned long get_register_value(unsigned short id, unsigned short num)
{
    struct ifreq ifr;
    struct mii_ioctl_data stats;

    stats.phy_id = id;
    stats.reg_num = num;
    stats.val_in = 0;
    stats.val_out = 0;

    ifr.ifr_data = (void *)&stats;

    sys_netdev_ioctl(AF_INET, -1, get_device_name(), SIOCGMIIREG, &ifr);

    return ((stats.val_in << 16) | stats.val_out);
}

// xref: set_register_value
static char *get_device_name(void)
{
	switch (check_hw_type()){
	case BCM5325E_CHIP:
	case BCM4704_BCM5325F_CHIP:
	case BCM5352E_CHIP:
		return "eth0";
	case BCM4702_CHIP:
	case BCM4712_CHIP:
	default:
		return "qos0";
	}
}

// xref: sys_netdev_ioctl
static int sockets_open(int domain, int type, int protocol)
{
    int fd = socket(domain, type, protocol);
    if (fd < 0)
		cprintf("sockets_open: no usable address was found.\n");
    return fd;
}

// xref: set_register_value
static int sys_netdev_ioctl(int family, int socket, char *if_name, int cmd, struct ifreq *ifr)
{
    int rc, s;

    if ((s = socket) < 0) {
		if ((s = sockets_open(family, family == AF_PACKET ? SOCK_PACKET : SOCK_DGRAM,
			family == AF_PACKET ? htons(ETH_P_ALL) : 0)) < 0) {
			cprintf("sys_netdev_ioctl: failed\n");
			return -1;
		}
    }
    strcpy(ifr->ifr_name, if_name);
    rc = ioctl(s, cmd, ifr);
    if (socket < 0) close(s);
    return rc;
}

// xref: rc
int set_register_value(unsigned short port_addr, unsigned short option_content)
{
    struct ifreq ifr;
    struct mii_ioctl_data stats;

    stats.phy_id = port_addr;
    stats.val_in = option_content;

    ifr.ifr_data = (void *)&stats;

    if (sys_netdev_ioctl(AF_INET, -1, get_device_name(), SIOCSMIIREG, &ifr) < 0)
		return -1;

    return 0;
}

#endif	// if 0
