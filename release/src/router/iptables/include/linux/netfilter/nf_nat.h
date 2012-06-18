#ifndef _NF_NAT_H
#define _NF_NAT_H
#include <linux/netfilter_ipv4.h>
#include <linux/netfilter/nf_conntrack_tuple.h>

#define NF_NAT_MAPPING_TYPE_MAX_NAMELEN 16

enum nf_nat_manip_type
{
	IP_NAT_MANIP_SRC,
	IP_NAT_MANIP_DST
};

/* SRC manip occurs POST_ROUTING or LOCAL_IN */
#define HOOK2MANIP(hooknum) ((hooknum) != NF_IP_POST_ROUTING && (hooknum) != NF_IP_LOCAL_IN)

#define IP_NAT_RANGE_MAP_IPS 1
#define IP_NAT_RANGE_PROTO_SPECIFIED 2
#define IP_NAT_RANGE_PROTO_RANDOM 4

/* Single range specification. */
struct nf_nat_range
{
	/* Set to OR of flags above. */
	unsigned int flags;

	/* Inclusive: network order. */
	__be32 min_ip, max_ip;

	/* Inclusive: network order */
	union nf_conntrack_man_proto min, max;
};

/* For backwards compat: don't use in modern code. */
struct nf_nat_multi_range_compat
{
	unsigned int rangesize; /* Must be 1. */

	/* hangs off end. */
	struct nf_nat_range range[1];
};

#define ip_nat_range nf_nat_range
#define ip_nat_multi_range nf_nat_multi_range_compat
#endif
