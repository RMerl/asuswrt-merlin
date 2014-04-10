/* $Id: iptcrdr.h,v 1.18 2012/03/05 20:36:20 nanard Exp $ */
/* MiniUPnP project
 * http://miniupnp.free.fr/ or http://miniupnp.tuxfamily.org/
 * (c) 2006-2011 Thomas Bernard
 * This software is subject to the conditions detailed
 * in the LICENCE file provided within the distribution */

#ifndef IPTCRDR_H_INCLUDED
#define IPTCRDR_H_INCLUDED

#include "../commonrdr.h"

int
add_redirect_rule2(const char * ifname,
                   const char * rhost, unsigned short eport,
                   const char * iaddr, unsigned short iport, int proto,
                   const char * desc, unsigned int timestamp);

int
add_peer_redirect_rule2(const char * ifname,
                   const char * rhost, unsigned short rport,
                   const char * eaddr, unsigned short eport,
                   const char * iaddr, unsigned short iport, int proto,
                   const char * desc, unsigned int timestamp);

int
add_filter_rule2(const char * ifname,
                 const char * rhost, const char * iaddr,
                 unsigned short eport, unsigned short iport,
                 int proto, const char * desc);

int
delete_redirect_and_filter_rules(unsigned short eport, int proto);

int
add_peer_dscp_rule2(const char * ifname,
                   const char * rhost, unsigned short rport,
                   unsigned char dscp,
                   const char * iaddr, unsigned short iport, int proto,
                   const char * desc, unsigned int timestamp);

int get_nat_ext_addr(struct sockaddr* src, struct sockaddr *dst, uint8_t proto,
                     struct sockaddr* ret_ext);
int
get_peer_rule_by_index(int index,
                           char * ifname, unsigned short * eport,
                           char * iaddr, int iaddrlen, unsigned short * iport,
                           int * proto, char * desc, int desclen,
                           char * rhost, int rhostlen, unsigned short * rport,
                           unsigned int * timestamp,
                           u_int64_t * packets, u_int64_t * bytes);
int
get_nat_redirect_rule(const char * nat_chain_name, const char * ifname, unsigned short eport, int proto,
                  char * iaddr, int iaddrlen, unsigned short * iport,
                  char * desc, int desclen,
                  char * rhost, int rhostlen,
                  unsigned int * timestamp,
                  u_int64_t * packets, u_int64_t * bytes);

/* for debug */
int
list_redirect_rule(const char * ifname);

#endif

