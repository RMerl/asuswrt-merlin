/* Header file for iptables ip6t_ROUTE target
 *
 * (C) 2003 by Cédric de Launois <delaunois@info.ucl.ac.be>
 *
 * This software is distributed under GNU GPL v2, 1991
 */
#ifndef _IPT_ROUTE_H_target
#define _IPT_ROUTE_H_target

#define IP6T_ROUTE_IFNAMSIZ 16

struct ip6t_route_target_info {
	char      oif[IP6T_ROUTE_IFNAMSIZ];     /* Output Interface Name */
	char      iif[IP6T_ROUTE_IFNAMSIZ];     /* Input Interface Name  */
	u_int32_t gw[4];                        /* IPv6 address of gateway */
	u_int8_t  flags;
};

/* Values for "flags" field */
#define IP6T_ROUTE_CONTINUE        0x01
#define IP6T_ROUTE_TEE             0x02

#endif /*_IP6T_ROUTE_H_target*/
