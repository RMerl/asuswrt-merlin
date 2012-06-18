/* Header file for kernel module to match connection tracking information.
 * GPL (C) 2001  Marc Boucher (marc@mbsi.ca).
 */

#ifndef _IPT_CONNTRACK_H
#define _IPT_CONNTRACK_H

#include <linux/netfilter/nf_conntrack_common.h>

/* backwards compatibility crap. only exists in userspace - HW */
#include <linux/version.h>
#ifndef KERNEL_VERSION
#define KERNEL_VERSION(a,b,c) (((a) << 16) | ((b) << 8) | (c))
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,4,18)) || !defined IPS_EXPECTED
#define IPS_EXPECTED	(1 << 0)
#define IPS_SEEN_REPLY	(1 << 1)
#define IPS_ASSURED	(1 << 2)
#define IP_CT_DIR_ORIGINAL	0
#define IP_CT_DIR_REPLY		1
#define IP_CT_DIR_MAX		2
#endif

#define IPT_CONNTRACK_STATE_BIT(ctinfo) (1 << ((ctinfo)%IP_CT_IS_REPLY+1))
#define IPT_CONNTRACK_STATE_INVALID (1 << 0)

#define IPT_CONNTRACK_STATE_SNAT (1 << (IP_CT_NUMBER + 1))
#define IPT_CONNTRACK_STATE_DNAT (1 << (IP_CT_NUMBER + 2))
#define IPT_CONNTRACK_STATE_UNTRACKED (1 << (IP_CT_NUMBER + 3))

/* flags, invflags: */
#define IPT_CONNTRACK_STATE	0x01
#define IPT_CONNTRACK_PROTO	0x02
#define IPT_CONNTRACK_ORIGSRC	0x04
#define IPT_CONNTRACK_ORIGDST	0x08
#define IPT_CONNTRACK_REPLSRC	0x10
#define IPT_CONNTRACK_REPLDST	0x20
#define IPT_CONNTRACK_STATUS	0x40
#define IPT_CONNTRACK_EXPIRES	0x80

/* This is exposed to userspace, so remains frozen in time. */
struct ip_conntrack_old_tuple
{
	struct {
		u_int32_t ip;
		union {
			u_int16_t all;
		} u;
	} src;

	struct {
		u_int32_t ip;
		union {
			u_int16_t all;
		} u;

		/* The protocol. */
		u_int16_t protonum;
	} dst;
};

struct ipt_conntrack_info
{
	unsigned int statemask, statusmask;

	struct ip_conntrack_old_tuple tuple[IP_CT_DIR_MAX];
	struct in_addr sipmsk[IP_CT_DIR_MAX], dipmsk[IP_CT_DIR_MAX];

#ifdef KERNEL_64_USERSPACE_32
	unsigned long long expires_min, expires_max;
#else
	unsigned long expires_min, expires_max;
#endif

	/* Flags word */
	u_int8_t flags;
	/* Inverse flags */
	u_int8_t invflags;
};
#endif /*_IPT_CONNTRACK_H*/
