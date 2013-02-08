/* $Id: getroute.h,v 1.1 2012/06/23 22:59:07 nanard Exp $ */
/* MiniUPnP project
 * http://miniupnp.free.fr/ or http://miniupnp.tuxfamily.org/
 * (c) 2006-2012 Thomas Bernard
 * This software is subject to the conditions detailed
 * in the LICENCE file provided within the distribution */

#ifndef  __GETROUTE_H__
#define  __GETROUTE_H__

int
get_src_for_route_to(const struct sockaddr * dst,
                     void * src, size_t * src_len);

#endif

