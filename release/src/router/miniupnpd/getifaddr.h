/* $Id: getifaddr.h,v 1.5 2011/05/15 08:59:27 nanard Exp $ */
/* MiniUPnP project
 * http://miniupnp.free.fr/ or http://miniupnp.tuxfamily.org/
 * (c) 2006-2011 Thomas Bernard 
 * This software is subject to the conditions detailed
 * in the LICENCE file provided within the distribution */

#ifndef __GETIFADDR_H__
#define __GETIFADDR_H__

/* getifaddr()
 * take a network interface name and write the
 * ip v4 address as text in the buffer
 * returns: 0 success, -1 failure */
int
getifaddr(const char * ifname, char * buf, int len);

/* find a non link local IP v6 address for the interface.
 * if ifname is NULL, look for all interfaces */
int
find_ipv6_addr(const char * ifname,
               char * dst, int n);

#endif

