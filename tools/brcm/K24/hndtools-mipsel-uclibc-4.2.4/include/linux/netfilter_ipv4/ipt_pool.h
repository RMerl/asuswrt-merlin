#ifndef _IPT_POOL_H
#define _IPT_POOL_H

#include <linux/netfilter_ipv4/ip_pool.h>

#define IPT_POOL_INV_SRC	0x00000001
#define IPT_POOL_INV_DST	0x00000002
#define IPT_POOL_DEL_SRC	0x00000004
#define IPT_POOL_DEL_DST	0x00000008
#define IPT_POOL_INV_MOD_SRC	0x00000010
#define IPT_POOL_INV_MOD_DST	0x00000020
#define IPT_POOL_MOD_SRC_ACCEPT	0x00000040
#define IPT_POOL_MOD_DST_ACCEPT	0x00000080
#define IPT_POOL_MOD_SRC_DROP	0x00000100
#define IPT_POOL_MOD_DST_DROP	0x00000200

/* match info */
struct ipt_pool_info
{
	ip_pool_t src;
	ip_pool_t dst;
	unsigned flags;
};

#endif /*_IPT_POOL_H*/
