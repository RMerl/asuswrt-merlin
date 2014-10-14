/*
 * INET		An implementation of the TCP/IP protocol suite for the LINUX
 *		operating system.  INET is implemented using the BSD Socket
 *		interface as the means of communication with the user level.
 *
 *		Definitions for the FDDI handlers.
 *
 * Version:	@(#)fddidevice.h	1.0.0	08/12/96
 *
 * Author:	Lawrence V. Stefani, <stefani@lkg.dec.com>
 *
 *		fddidevice.h is based on previous trdevice.h work by
 *			Ross Biro
 *			Fred N. van Kempen, <waltje@uWalt.NL.Mugnet.ORG>
 *			Alan Cox, <gw4pts@gw4pts.ampr.org>
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 */
#ifndef _LINUX_FDDIDEVICE_H
#define _LINUX_FDDIDEVICE_H

#include <linux/if_fddi.h>

#ifdef __KERNEL__
extern __be16	fddi_type_trans(struct sk_buff *skb,
				struct net_device *dev);
extern int fddi_change_mtu(struct net_device *dev, int new_mtu);
extern struct net_device *alloc_fddidev(int sizeof_priv);
#endif

#endif	/* _LINUX_FDDIDEVICE_H */
