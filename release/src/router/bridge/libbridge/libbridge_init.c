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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "libbridge.h"
#include "libbridge_private.h"

int br_socket_fd = -1;
struct sysfs_class *br_class_net;

int br_init(void)
{
	if ((br_socket_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		return errno;

	br_class_net = sysfs_open_class("net");
	return 0;
}

int br_refresh(void)
{
	if (br_class_net) {
		sysfs_close_class(br_class_net);
		br_class_net = sysfs_open_class("net");
	}
	
	return 0;
}

void br_shutdown(void)
{
	sysfs_close_class(br_class_net);
	br_class_net = NULL;
	close(br_socket_fd);
	br_socket_fd = -1;
}

#ifdef HAVE_LIBSYSFS
/* If /sys/class/net/XXX/bridge exists then it must be a bridge */
static int isbridge(const struct sysfs_class_device *dev) 
{
	char path[SYSFS_PATH_MAX];

	snprintf(path, sizeof(path), "%s/bridge", dev->path);
	return !sysfs_path_is_dir(path);
}

/*
 * New interface uses sysfs to find bridges
 */
static int new_foreach_bridge(int (*iterator)(const char *name, void *),
			      void *arg)
{
	struct sysfs_class_device *dev;
	struct dlist *devlist;
	int count = 0;

	if (!br_class_net) {
		dprintf("no class /sys/class/net\n");
		return -EOPNOTSUPP;
	}

	devlist = sysfs_get_class_devices(br_class_net);
	if (!devlist) {
		dprintf("Can't read devices from sysfs\n");
		return -errno;
	}

	dlist_for_each_data(devlist, dev, struct sysfs_class_device) {
		if (isbridge(dev)) {
			++count;
			if (iterator(dev->name, arg))
				break;
		}
	}

	return count;
}
#endif

/*
 * Old interface uses ioctl
 */
static int old_foreach_bridge(int (*iterator)(const char *, void *), 
			      void *iarg)
{
	int i, ret=0, num;
	char ifname[IFNAMSIZ];
	int ifindices[MAX_BRIDGES];
	unsigned long args[3] = { BRCTL_GET_BRIDGES, 
				 (unsigned long)ifindices, MAX_BRIDGES };

	num = ioctl(br_socket_fd, SIOCGIFBR, args);
	if (num < 0) {
		dprintf("Get bridge indices failed: %s\n",
			strerror(errno));
		return -errno;
	}

	for (i = 0; i < num; i++) {
		if (!if_indextoname(ifindices[i], ifname)) {
			dprintf("get find name for ifindex %d\n",
				ifindices[i]);
			return -errno;
		}

		++ret;
		if(iterator(ifname, iarg)) 
			break;
		
	}

	return ret;

}

/*
 * Go over all bridges and call iterator function.
 * if iterator returns non-zero then stop.
 */
int br_foreach_bridge(int (*iterator)(const char *, void *), 
		     void *arg)
{
	int ret;
#ifdef HAVE_LIBSYSFS

	ret = new_foreach_bridge(iterator, arg);
	if (ret <= 0)
#endif
		ret = old_foreach_bridge(iterator, arg);

	return ret;
}

/* 
 * Only used if sysfs is not available.
 */
static int old_foreach_port(const char *brname,
			    int (*iterator)(const char *br, const char *port, 
					    void *arg),
			    void *arg)
{
	int i, err, count;
	struct ifreq ifr;
	char ifname[IFNAMSIZ];
	int ifindices[MAX_PORTS];
	unsigned long args[4] = { BRCTL_GET_PORT_LIST,
				  (unsigned long)ifindices, MAX_PORTS, 0 };

	memset(ifindices, 0, sizeof(ifindices));
	strncpy(ifr.ifr_name, brname, IFNAMSIZ);
	ifr.ifr_data = (char *) &args;

	err = ioctl(br_socket_fd, SIOCDEVPRIVATE, &ifr);
	if (err < 0) {
		dprintf("list ports for bridge:'%s' failed: %s\n",
			brname, strerror(errno));
		return -errno;
	}

	count = 0;
	for (i = 0; i < MAX_PORTS; i++) {
		if (!ifindices[i])
			continue;

		if (!if_indextoname(ifindices[i], ifname)) {
			dprintf("can't find name for ifindex:%d\n",
				ifindices[i]);
			continue;
		}

		++count;
		if (iterator(brname, ifname, arg))
			break;
	}

	return count;
}
	
/*
 * Iterate over all ports in bridge (using sysfs).
 */
int br_foreach_port(const char *brname,
		    int (*iterator)(const char *br, const char *port, void *arg),
		    void *arg)
{
#ifdef HAVE_LIBSYSFS
	struct sysfs_class_device *dev;
	struct sysfs_directory *dir;
	struct sysfs_link *plink;
	struct dlist *links;
	int err = 0;
	char path[SYSFS_PATH_MAX];

	if (!br_class_net ||
	    !(dev = sysfs_get_class_device(br_class_net, (char *) brname)))
		goto old;

	snprintf(path, sizeof(path), "%s/%s", 
		 dev->path, SYSFS_BRIDGE_PORT_SUBDIR);

	dprintf("path=%s\n", path);
	dir = sysfs_open_directory(path);
	if (!dir) {
		/* no /sys/class/net/ethX/brif subdirectory
		 * either: old kernel, or not really a bridge
		 */
		goto old;
	}

	links = sysfs_get_dir_links(dir);
	if (!links) {
		err = -ENOSYS;
		goto out;
	}

	err = 0;
	dlist_for_each_data(links, plink, struct sysfs_link) {
		++err;
		if (iterator(brname, plink->name, arg))
			break;
	}
 out:
	sysfs_close_directory(dir);
	return err;

 old:
#endif
	return old_foreach_port(brname, iterator, arg);
	
}
