/*
 * Copyright (C) 2000 Lennert Buytenhek
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef _LIBBRIDGE_PRIVATE_H
#define _LIBBRIDGE_PRIVATE_H

#include "config.h"

#include <linux/sockios.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <linux/if_bridge.h>
#include <asm/param.h>

#define MAX_BRIDGES	1024
#define MAX_PORTS	1024

#define dprintf(fmt,arg...)

#ifdef HAVE_LIBSYSFS
#include <sysfs/libsysfs.h>

#ifndef SYSFS_BRIDGE_PORT_ATTR
#error Using wrong kernel headers if_bridge.h is out of date.
#endif

#ifndef SIOCBRADDBR
#error Using wrong kernel headers sockios.h is out of date.
#endif

#else
struct sysfs_class { const char *name; };

static inline struct sysfs_class *sysfs_open_class(const char *name)
{
	return NULL;
}

static inline void sysfs_close_class(struct sysfs_class *class)
{
}
#endif

extern int br_socket_fd;
extern struct sysfs_class *br_class_net;

static inline unsigned long __tv_to_jiffies(const struct timeval *tv)
{
	unsigned long long jif;

	jif = 1000000ULL * tv->tv_sec + tv->tv_usec;

	return (HZ*jif)/1000000;
}

static inline void __jiffies_to_tv(struct timeval *tv, unsigned long jiffies)
{
	unsigned long long tvusec;

	tvusec = (1000000ULL*jiffies)/HZ;
	tv->tv_sec = tvusec/1000000;
	tv->tv_usec = tvusec - 1000000 * tv->tv_sec;
}
#endif
