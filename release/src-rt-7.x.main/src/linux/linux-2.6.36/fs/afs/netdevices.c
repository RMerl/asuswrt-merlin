/* AFS network device helpers
 *
 * Copyright (c) 2007 Patrick McHardy <kaber@trash.net>
 */

#include <linux/string.h>
#include <linux/rtnetlink.h>
#include <linux/inetdevice.h>
#include <linux/netdevice.h>
#include <linux/if_arp.h>
#include <net/net_namespace.h>
#include "internal.h"

/*
 * get a MAC address from a random ethernet interface that has a real one
 * - the buffer will normally be 6 bytes in size
 */
int afs_get_MAC_address(u8 *mac, size_t maclen)
{
	struct net_device *dev;
	int ret = -ENODEV;

	BUG_ON(maclen != ETH_ALEN);

	rtnl_lock();
	dev = __dev_getfirstbyhwtype(&init_net, ARPHRD_ETHER);
	if (dev) {
		memcpy(mac, dev->dev_addr, maclen);
		ret = 0;
	}
	rtnl_unlock();
	return ret;
}

/*
 * get a list of this system's interface IPv4 addresses, netmasks and MTUs
 * - maxbufs must be at least 1
 * - returns the number of interface records in the buffer
 */
int afs_get_ipv4_interfaces(struct afs_interface *bufs, size_t maxbufs,
			    bool wantloopback)
{
	struct net_device *dev;
	struct in_device *idev;
	int n = 0;

	ASSERT(maxbufs > 0);

	rtnl_lock();
	for_each_netdev(&init_net, dev) {
		if (dev->type == ARPHRD_LOOPBACK && !wantloopback)
			continue;
		idev = __in_dev_get_rtnl(dev);
		if (!idev)
			continue;
		for_primary_ifa(idev) {
			bufs[n].address.s_addr = ifa->ifa_address;
			bufs[n].netmask.s_addr = ifa->ifa_mask;
			bufs[n].mtu = dev->mtu;
			n++;
			if (n >= maxbufs)
				goto out;
		} endfor_ifa(idev);
	}
out:
	rtnl_unlock();
	return n;
}
