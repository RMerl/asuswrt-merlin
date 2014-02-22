/*
 * Copyright (C)2006 USAGI/WIDE Project
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
 * split from ip_tunnel.c
 */
/*
 * Author:
 *	Masahide NAKAMURA @USAGI
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <linux/if.h>
#include <linux/ip.h>
#include <linux/if_tunnel.h>

#include "utils.h"
#include "tunnel.h"

const char *tnl_strproto(__u8 proto)
{
	static char buf[16];

	switch (proto) {
	case IPPROTO_IPIP:
		strcpy(buf, "ip");
		break;
	case IPPROTO_GRE:
		strcpy(buf, "gre");
		break;
	case IPPROTO_IPV6:
		strcpy(buf, "ipv6");
		break;
	case 0:
		strcpy(buf, "any");
		break;
	default:
		strcpy(buf, "unknown");
		break;
	}

	return buf;
}

int tnl_get_ioctl(const char *basedev, void *p)
{
	struct ifreq ifr;
	int fd;
	int err;

	strncpy(ifr.ifr_name, basedev, IFNAMSIZ);
	ifr.ifr_ifru.ifru_data = (void*)p;
	fd = socket(preferred_family, SOCK_DGRAM, 0);
	err = ioctl(fd, SIOCGETTUNNEL, &ifr);
	if (err)
		fprintf(stderr, "get tunnel %s failed: %s\n", basedev, 
			strerror(errno));

	close(fd);
	return err;
}

int tnl_add_ioctl(int cmd, const char *basedev, const char *name, void *p)
{
	struct ifreq ifr;
	int fd;
	int err;

	if (cmd == SIOCCHGTUNNEL && name[0])
		strncpy(ifr.ifr_name, name, IFNAMSIZ);
	else
		strncpy(ifr.ifr_name, basedev, IFNAMSIZ);
	ifr.ifr_ifru.ifru_data = p;
	fd = socket(preferred_family, SOCK_DGRAM, 0);
	err = ioctl(fd, cmd, &ifr);
	if (err)
		fprintf(stderr, "add tunnel %s failed: %s\n", ifr.ifr_name,
			strerror(errno));
	close(fd);
	return err;
}

int tnl_del_ioctl(const char *basedev, const char *name, void *p)
{
	struct ifreq ifr;
	int fd;
	int err;

	if (name[0])
		strncpy(ifr.ifr_name, name, IFNAMSIZ);
	else
		strncpy(ifr.ifr_name, basedev, IFNAMSIZ);

	ifr.ifr_ifru.ifru_data = p;
	fd = socket(preferred_family, SOCK_DGRAM, 0);
	err = ioctl(fd, SIOCDELTUNNEL, &ifr);
	if (err)
		fprintf(stderr, "delete tunnel %s failed: %s\n",
			ifr.ifr_name, strerror(errno));
	close(fd);
	return err;
}

static int tnl_gen_ioctl(int cmd, const char *name, 
			 void *p, int skiperr)
{
	struct ifreq ifr;
	int fd;
	int err;

	strncpy(ifr.ifr_name, name, IFNAMSIZ);
	ifr.ifr_ifru.ifru_data = p;
	fd = socket(preferred_family, SOCK_DGRAM, 0);
	err = ioctl(fd, cmd, &ifr);
	if (err && errno != skiperr)
		fprintf(stderr, "%s: ioctl %x failed: %s\n", name,
			cmd, strerror(errno));
	close(fd);
	return err;
}

int tnl_prl_ioctl(int cmd, const char *name, void *p)
{
	return tnl_gen_ioctl(cmd, name, p, -1);
}

#ifndef NO_IPV6
int tnl_6rd_ioctl(int cmd, const char *name, void *p)
{
	return tnl_gen_ioctl(cmd, name, p, -1);
}

int tnl_ioctl_get_6rd(const char *name, void *p)
{
	return tnl_gen_ioctl(SIOCGET6RD, name, p, EINVAL);
}
#endif	// NO_IPV6
