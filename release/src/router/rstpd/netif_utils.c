/*****************************************************************************
  Copyright (c) 2006 EMC Corporation.

  This program is free software; you can redistribute it and/or modify it 
  under the terms of the GNU General Public License as published by the Free 
  Software Foundation; either version 2 of the License, or (at your option) 
  any later version.
  
  This program is distributed in the hope that it will be useful, but WITHOUT 
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or 
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for 
  more details.
  
  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc., 59 
  Temple Place - Suite 330, Boston, MA  02111-1307, USA.
  
  The full GNU General Public License is included in this distribution in the
  file called LICENSE.

  Authors: Srinivas Aji <Aji_Srinivas@emc.com>

******************************************************************************/

#include "netif_utils.h"

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <limits.h>

#include <net/if.h>
#include <linux/if_ether.h>
#include <linux/ethtool.h>
#include <linux/sockios.h>

#include "log.h"

int netsock = -1;

int netsock_init(void)
{
	LOG("");
	netsock = socket(AF_INET, SOCK_DGRAM, 0);
	if (netsock < 0) {
		ERROR("Couldn't open inet socket for ioctls: %m\n");
		return -1;
	}
	return 0;
}

int get_hwaddr(char *ifname, unsigned char *hwaddr)
{
	struct ifreq ifr;
	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
	if (ioctl(netsock, SIOCGIFHWADDR, &ifr) < 0) {
		ERROR("%s: get hw address failed: %m", ifname);
		return -1;
	}
	memcpy(hwaddr, ifr.ifr_hwaddr.sa_data, ETH_ALEN);
	return 0;
}

int ethtool_get_speed_duplex(char *ifname, int *speed, int *duplex)
{
	struct ifreq ifr;
	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
	struct ethtool_cmd ecmd;

	ecmd.cmd = ETHTOOL_GSET;
	ifr.ifr_data = (caddr_t) & ecmd;
	if (ioctl(netsock, SIOCETHTOOL, &ifr) < 0) {
		ERROR("Cannot get link status for %s: %m\n", ifname);
		return -1;
	}
	*speed = ecmd.speed;	/* Ethtool speed is in Mbps */
	*duplex = ecmd.duplex;	/* We have same convention as ethtool.
				   0 = half, 1 = full */
	return 0;
}

/********* Sysfs based utility functions *************/

/* This sysfs stuff might break with interface renames */
int is_bridge(char *if_name)
{
	char path[32 + IFNAMSIZ];
	sprintf(path, "/sys/class/net/%s/bridge", if_name);
	return (access(path, R_OK) == 0);
}

/* This will work even less if the bridge port is renamed after being
   joined to the bridge.
*/
int is_bridge_slave(char *br_name, char *if_name)
{
	char path[32 + 2 * IFNAMSIZ];
	sprintf(path, "/sys/class/net/%s/brif/%s", br_name, if_name);
	return (access(path, R_OK) == 0);
}

int get_bridge_portno(char *if_name)
{
	char path[32 + IFNAMSIZ];
	sprintf(path, "/sys/class/net/%s/brport/port_no", if_name);
	char buf[128];
	int fd;
	long res = -1;
	TSTM((fd = open(path, O_RDONLY)) >= 0, -1, "%m");
	int l;
	TSTM((l = read(fd, buf, sizeof(buf) - 1)) >= 0, -1, "%m");
	if (l == 0) {
		ERROR("Empty port index file");
		goto out;
	} else if (l == sizeof(buf) - 1) {
		ERROR("port_index file too long");
		goto out;
	}
	buf[l] = 0;
	if (buf[l - 1] == '\n')
		buf[l - 1] = 0;
	char *end;
	res = strtoul(buf, &end, 0);
	if (*end != 0 || res > INT_MAX) {
		ERROR("Invalid port index %s", buf);
		res = -1;
	}
      out:
	close(fd);
	return res;
}
