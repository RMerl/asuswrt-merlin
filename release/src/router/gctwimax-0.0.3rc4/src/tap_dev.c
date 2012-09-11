/*
 * This is a reverse-engineered driver for mobile WiMAX (802.16e) devices
 * based on GCT Semiconductor GDM7213 & GDM7205 chip.
 * Copyright (�) 2010 Yaroslav Levandovsky <leyarx@gmail.com>
 *
 * Based on  madWiMAX driver writed by Alexander Gordeev
 * Copyright (C) 2008-2009 Alexander Gordeev <lasaine@lvk.cs.msu.su>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/if.h>
#include <linux/if_arp.h>
#include <linux/if_ether.h>
#include <linux/if_tun.h>

#include "wimax.h"


/*
 * Allocate TUN device, returns opened fd.
 * Stores dev name in the first arg(must be large enough).
 */
static int tun_open_common0(char *dev, int istun)
{
	char tunname[14];
	int i, fd, err;

	if( *dev ) {
		sprintf(tunname, "/dev/%s", dev);
		return open(tunname, O_RDWR);
	}

	sprintf(tunname, "/dev/%s", istun ? "tun" : "tap");
	err = 0;
	for(i=0; i < 255; i++){
		sprintf(tunname + 8, "%d", i);
		/* Open device */
		if( (fd=open(tunname, O_RDWR)) > 0 ) {
			strcpy(dev, tunname + 5);
			return fd;
		}
		else if (errno != ENOENT)
			err = errno;
		else if (i)	/* don't try all 256 devices */
			break;
	}
	if (err)
	errno = err;
	return -1;
}

#ifndef OTUNSETNOCSUM
/* pre 2.4.6 compatibility */
#define OTUNSETNOCSUM	(('T'<< 8) | 200)
#define OTUNSETDEBUG	(('T'<< 8) | 201)
#define OTUNSETIFF	(('T'<< 8) | 202)
#define OTUNSETPERSIST	(('T'<< 8) | 203)
#define OTUNSETOWNER	(('T'<< 8) | 204)
#endif

static int tun_open_common(char *dev, int istun)
{
	struct ifreq ifr;
	int fd;

	if ((fd = open("/dev/net/tun", O_RDWR)) < 0)
		return tun_open_common0(dev, istun);

	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_flags = (istun ? IFF_TUN : IFF_TAP) | IFF_NO_PI;
	if (*dev)
		strncpy(ifr.ifr_name, dev, IFNAMSIZ);
	if (ioctl(fd, TUNSETIFF, (void *) &ifr) < 0) {
		if (errno == EBADFD) {
			/* Try old ioctl */
 			if (ioctl(fd, OTUNSETIFF, (void *) &ifr) < 0)
				goto failed;
		} else
			goto failed;
	}

	strcpy(dev, ifr.ifr_name);
	return fd;

failed:
	close(fd);
	return -1;
}


int tap_open(char *dev) { return tun_open_common(dev, 0); }

int tap_close(int fd, char *dev) { return close(fd); }

/* Read/write frames from TAP device */
int tap_write(int fd, const void *buf, int len) { return write(fd, buf, len); }

int tap_read(int fd, void *buf, int len) { return read(fd, buf, len); }


/* Like strncpy but make sure the resulting string is always 0 terminated. */
static char *safe_strncpy(char *dst, const char *src, int size)
{
	dst[size-1] = '\0';
	return strncpy(dst,src,size-1);
}

/* Set interface hw adress */
int tap_set_hwaddr(int fd, const char *dev, unsigned char *hwaddr)
{
	struct ifreq ifr;

	fd = socket(PF_INET, SOCK_DGRAM, 0);

	/* Fill in the structure */
	safe_strncpy(ifr.ifr_name, dev, IFNAMSIZ);
	ifr.ifr_hwaddr.sa_family = ARPHRD_ETHER;
	memcpy(&ifr.ifr_hwaddr.sa_data, hwaddr, ETH_ALEN);

	/* call the IOCTL */
	if (ioctl(fd, SIOCSIFHWADDR, &ifr) < 0) {
		perror("SIOCSIFHWADDR");
		return -1;
	}

	close(fd);
	return 0;
}

/* Set interface mtu */
int tap_set_mtu(int fd, const char *dev, int mtu)
{
	struct ifreq ifr;

	fd = socket(PF_INET, SOCK_DGRAM, 0);

	/* Fill in the structure */
	safe_strncpy(ifr.ifr_name, dev, IFNAMSIZ);
	ifr.ifr_mtu = mtu;

	/* call the IOCTL */
	if ((ioctl(fd, SIOCSIFMTU, &ifr)) < 0) {
		perror("SIOCSIFMTU");
		return -1;
	}

	close(fd);
	return 0;
}

/* Set a certain interface flags. */
static int tap_set_flags(const char *dev, short flags)
{
	struct ifreq ifr;
	int fd;

	fd = socket(PF_INET, SOCK_DGRAM, 0);

	safe_strncpy(ifr.ifr_name, dev, IFNAMSIZ);
	if (ioctl(fd, SIOCGIFFLAGS, &ifr) < 0) {
		fprintf(stderr, "%s: ERROR while getting interface flags: %s\n",
			dev, strerror(errno));
		return (-1);
	}

	safe_strncpy(ifr.ifr_name, dev, IFNAMSIZ);
	ifr.ifr_flags |= flags;
	if (ioctl(fd, SIOCSIFFLAGS, &ifr) < 0) {
		perror("SIOCSIFFLAGS");
		return -1;
	}

	close(fd);
	return (0);
}

/* Clear a certain interface flags. */
static int tap_clr_flags(const char *dev, short flags)
{
	struct ifreq ifr;
	int fd;

	fd = socket(PF_INET, SOCK_DGRAM, 0);

	safe_strncpy(ifr.ifr_name, dev, IFNAMSIZ);
	if (ioctl(fd, SIOCGIFFLAGS, &ifr) < 0) {
		fprintf(stderr, "%s: ERROR while getting interface flags: %s\n",
			dev, strerror(errno));
		return -1;
	}

	safe_strncpy(ifr.ifr_name, dev, IFNAMSIZ);
	ifr.ifr_flags &= ~flags;
	if (ioctl(fd, SIOCSIFFLAGS, &ifr) < 0) {
		perror("SIOCSIFFLAGS");
		return -1;
	}

	close(fd);
	return (0);
}

/* Test if a specified flags is set *//*
static int tap_test_flag(const char *dev, short flags)
{
	struct ifreq ifr;
	int fd;

	fd = socket(PF_INET, SOCK_DGRAM, 0);

	safe_strncpy(ifr.ifr_name, dev, IFNAMSIZ);
	if (ioctl(fd, SIOCGIFFLAGS, &ifr) < 0) {
		fprintf(stderr, "%s: ERROR while testing interface flags: %s\n",
			dev, strerror(errno));
		return -1;
	}

	close(fd);
	return (ifr.ifr_flags & flags);
}*/

int tap_bring_up(int fd, const char *dev)
{
	return tap_set_flags(dev, IFF_UP);
}

int tap_bring_down(int fd, const char *dev)
{
	return tap_clr_flags(dev, IFF_UP);
}

