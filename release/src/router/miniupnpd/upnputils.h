/* $Id: upnputils.h,v 1.2 2012/02/06 16:21:24 nanard Exp $ */
/* MiniUPnP project
 * http://miniupnp.free.fr/ or http://miniupnp.tuxfamily.org/
 * (c) 2011-2012 Thomas Bernard
 * This software is subject to the conditions detailed
 * in the LICENCE file provided within the distribution */

#ifndef __UPNPUTILS_H__
#define __UPNPUTILS_H__

/**
 * convert a struct sockaddr to a human readable string.
 * [ipv6]:port or ipv4:port
 * return the number of characters used (as snprintf)
 */
int
sockaddr_to_string(const struct sockaddr * addr, char * str, size_t size);

/**
 * set the file description as non blocking
 * return 0 in case of failure, 1 in case of success
 */
int
set_non_blocking(int fd);

#endif

