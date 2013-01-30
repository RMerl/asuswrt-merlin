/* $Id: upnppinhole.h,v 1.1 2012/05/01 20:08:22 nanard Exp $ */
/* MiniUPnP project
 * http://miniupnp.free.fr/ or http://miniupnp.tuxfamily.org/
 * (c) 2006-2012 Thomas Bernard
 * This software is subject to the conditions detailed
 * in the LICENCE file provided within the distribution */

#ifndef __UPNPPINHOLE_H__
#define __UPNPPINHOLE_H__

#include "config.h"

#ifdef ENABLE_6FC_SERVICE
/* functions to be used by WANIPv6_FirewallControl implementation */

#if 0
/* retrieve outbound pinhole timeout */
int
upnp_check_outbound_pinhole(int proto, int * timeout);
#endif

/* add an inbound pinehole
 * return value :
 *  1 = success
 * -1 = Pinhole space exhausted
 * .. = error */
int
upnp_add_inboundpinhole(const char * raddr, unsigned short rport,
              const char * iaddr, unsigned short iport,
              int proto, unsigned int leasetime, int * uid);

/*
 * return values :
 *  -1 not found
 * */
int
upnp_get_pinhole_info(unsigned short uid,
                      char * raddr, int raddrlen,
                      unsigned short * rport,
                      char * iaddr, int iaddrlen,
                      unsigned short * iport,
                      int * proto,
                      unsigned int * leasetime,
                      unsigned int * packets);

/* update the lease time */
int
upnp_update_inboundpinhole(unsigned short uid, unsigned int leasetime);

/* remove the inbound pinhole */
int
upnp_delete_inboundpinhole(unsigned short uid);

/* ... */
#if 0
int
upnp_check_pinhole_working(const char * uid, char * eaddr, char * iaddr, unsigned short * eport, unsigned short * iport, char * protocol, int * rulenum_used);
#endif

/* ? */
int
upnp_clean_expired_pinholes(unsigned int * next_timestamp);

#endif /* ENABLE_6FC_SERVICE */

#endif


