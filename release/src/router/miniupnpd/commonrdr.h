/* $Id: commonrdr.h,v 1.10 2016/02/12 12:34:39 nanard Exp $ */
/* MiniUPnP project
 * (c) 2006-2016 Thomas Bernard
 * http://miniupnp.free.fr/ or http://miniupnp.tuxfamily.org/
 * This software is subject to the conditions detailed
 * in the LICENCE file provided within the distribution */
#ifndef COMMONRDR_H_INCLUDED
#define COMMONRDR_H_INCLUDED

#include "config.h"

/* init and shutdown functions */
/* init_redirect() return values :
 *  0 : OK
 * -1 : error */
int
init_redirect(void);

void
shutdown_redirect(void);

/* get_redirect_rule() gets internal IP and port from
 * interface, external port and protocol
 * return value :
 *  0 success (rule found)
 * -1 error or rule not found
 */
int
get_redirect_rule(const char * ifname, unsigned short eport, int proto,
                  char * iaddr, int iaddrlen, unsigned short * iport,
                  char * desc, int desclen,
                  char * rhost, int rhostlen,
                  unsigned int * timestamp,
                  u_int64_t * packets, u_int64_t * bytes);

/* get_redirect_rule_by_index()
 * return values :
 *  0 success (rule found)
 * -1 error or rule not found */
int
get_redirect_rule_by_index(int index,
                           char * ifname, unsigned short * eport,
                           char * iaddr, int iaddrlen, unsigned short * iport,
                           int * proto, char * desc, int desclen,
                           char * rhost, int rhostlen,
                           unsigned int * timestamp,
                           u_int64_t * packets, u_int64_t * bytes);

/* return an (malloc'ed) array of "external" port for which there is
 * a port mapping. number is the size of the array */
unsigned short *
get_portmappings_in_range(unsigned short startport, unsigned short endport,
                          int proto, unsigned int * number);

/* update the port mapping internal port, decription and timestamp */
int
update_portmapping(const char * ifname, unsigned short eport, int proto,
                   unsigned short iport, const char * desc,
                   unsigned int timestamp);

/* update the port mapping decription and timestamp */
int
update_portmapping_desc_timestamp(const char * ifname,
                   unsigned short eport, int proto,
                   const char * desc, unsigned int timestamp);

#endif
