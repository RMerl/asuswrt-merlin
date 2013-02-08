/* $Id: iptpinhole.h,v 1.5 2012/05/08 20:41:45 nanard Exp $ */
/* MiniUPnP project
 * http://miniupnp.free.fr/ or http://miniupnp.tuxfamily.org/
 * (c) 2012 Thomas Bernard
 * This software is subject to the conditions detailed
 * in the LICENCE file provided within the distribution */
#ifndef __IPTPINHOLE_H__
#define __IPTPINHOLE_H__

#ifdef ENABLE_6FC_SERVICE
int add_pinhole(const char * ifname,
                const char * rem_host, unsigned short rem_port,
                const char * int_client, unsigned short int_port,
                int proto, unsigned int timestamp);

int update_pinhole(unsigned short uid, unsigned int timestamp);

int delete_pinhole(unsigned short uid);

int
get_pinhole_info(unsigned short uid,
                 char * rem_host, int rem_hostlen, unsigned short * rem_port,
                 char * int_client, int int_clientlen, unsigned short * int_port,
                 int * proto, unsigned int * timestamp,
                 u_int64_t * packets, u_int64_t * bytes);

int clean_pinhole_list(unsigned int * next_timestamp);

#endif

#endif
