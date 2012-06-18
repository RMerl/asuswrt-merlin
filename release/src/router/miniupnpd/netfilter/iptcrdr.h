/* $Id: iptcrdr.h,v 1.14 2007/12/18 10:14:12 nanard Exp $ */
/* MiniUPnP project
 * http://miniupnp.free.fr/ or http://miniupnp.tuxfamily.org/
 * (c) 2006 Thomas Bernard 
 * This software is subject to the conditions detailed
 * in the LICENCE file provided within the distribution */

#ifndef __IPTCRDR_H__
#define __IPTCRDR_H__

#include "../commonrdr.h"

int
add_redirect_rule2(const char * ifname, unsigned short eport,
                   const char * iaddr, unsigned short iport, int proto,
				   const char * desc);

int
add_filter_rule2(const char * ifname, const char * iaddr,
                 unsigned short eport, unsigned short iport,
                 int proto, const char * desc);

int
delete_redirect_and_filter_rules(unsigned short eport, int proto);

/* for debug */
int
list_redirect_rule(const char * ifname);

int
addnatrule(int proto, unsigned short eport,
               const char * iaddr, unsigned short iport);

int
add_filter_rule(int proto, const char * iaddr, unsigned short iport);

#endif

