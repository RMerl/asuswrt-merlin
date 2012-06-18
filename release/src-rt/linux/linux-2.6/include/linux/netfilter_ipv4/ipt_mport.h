#ifndef _IPT_MPORT_H
#define _IPT_MPORT_H
#include <linux/netfilter_ipv4/ip_tables.h>

#define IPT_MPORT_SOURCE (1<<0)
#define IPT_MPORT_DESTINATION (1<<1)
#define IPT_MPORT_EITHER (IPT_MPORT_SOURCE|IPT_MPORT_DESTINATION)

#define IPT_MULTI_PORTS	15

/* Must fit inside union ipt_matchinfo: 32 bytes */
/* every entry in ports[] except for the last one has one bit in pflags
 * associated with it. If this bit is set, the port is the first port of
 * a portrange, with the next entry being the last.
 * End of list is marked with pflags bit set and port=65535.
 * If 14 ports are used (last one does not have a pflag), the last port
 * is repeated to fill the last entry in ports[] */
struct ipt_mport
{
	u_int8_t flags:2;			/* Type of comparison */
	u_int16_t pflags:14;			/* Port flags */
	u_int16_t ports[IPT_MULTI_PORTS];	/* Ports */
};
#endif /*_IPT_MPORT_H*/
