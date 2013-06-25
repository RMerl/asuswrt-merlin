/* x_tables module for matching the IPv4/IPv6 DSCP field
 *
 * (C) 2002 Harald Welte <laforge@gnumonks.org>
 * This software is distributed under GNU GPL v2, 1991
 *
 * xt_ethport.h, 2009/06/09 
*/
#ifndef _XT_ETHPORT_H
#define _XT_ETHPORT_H

#define XT_SKB_CB_OFFSET	0x24	/* store port number info in skb->cb[0x24] */
#define XT_ETHPORT_MAX		0x8		/* port 0-5 && cpu port(port6)*/

/* match info */
struct xt_ethport_info {
	u_int8_t portnum;
	u_int8_t invert;
};

#endif /* _XT_ETHPORT_H */
