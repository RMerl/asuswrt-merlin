/*
 *
 *	Generic internet FLOW.
 *
 */

#ifndef _NET_FLOW_H
#define _NET_FLOW_H

#include <linux/in6.h>
#include <asm/atomic.h>

struct flowi_common {
	int	flowic_oif;
	int	flowic_iif;
	__u32	flowic_mark;
	__u8	flowic_tos;
	__u8	flowic_scope;
	__u8	flowic_proto;
	__u8	flowic_flags;
#define FLOWI_FLAG_ANYSRC		0x01
#define FLOWI_FLAG_PRECOW_METRICS	0x02
#define FLOWI_FLAG_CAN_SLEEP		0x04
	__u32	flowic_secid;
};

union flowi_uli {
	struct {
		__be16	sport;
		__be16	dport;
	} ports;

	struct {
		__u8	type;
		__u8	code;
	} icmpt;

	struct {
		__le16	sport;
		__le16	dport;
	} dnports;

	__be32		spi;
	__be32		gre_key;

	struct {
		__u8	type;
	} mht;
};

struct flowi4 {
	struct flowi_common	__fl_common;
#define flowi4_oif		__fl_common.flowic_oif
#define flowi4_iif		__fl_common.flowic_iif
#define flowi4_mark		__fl_common.flowic_mark
#define flowi4_tos		__fl_common.flowic_tos
#define flowi4_scope		__fl_common.flowic_scope
#define flowi4_proto		__fl_common.flowic_proto
#define flowi4_flags		__fl_common.flowic_flags
#define flowi4_secid		__fl_common.flowic_secid
	__be32			daddr;
	__be32			saddr;
	union flowi_uli		uli;
#define fl4_sport		uli.ports.sport
#define fl4_dport		uli.ports.dport
#define fl4_icmp_type		uli.icmpt.type
#define fl4_icmp_code		uli.icmpt.code
#define fl4_ipsec_spi		uli.spi
#define fl4_mh_type		uli.mht.type
#define fl4_gre_key		uli.gre_key
};

struct flowi6 {
	struct flowi_common	__fl_common;
#define flowi6_oif		__fl_common.flowic_oif
#define flowi6_iif		__fl_common.flowic_iif
#define flowi6_mark		__fl_common.flowic_mark
#define flowi6_tos		__fl_common.flowic_tos
#define flowi6_scope		__fl_common.flowic_scope
#define flowi6_proto		__fl_common.flowic_proto
#define flowi6_flags		__fl_common.flowic_flags
#define flowi6_secid		__fl_common.flowic_secid
	struct in6_addr		daddr;
	struct in6_addr		saddr;
	__be32			flowlabel;
	union flowi_uli		uli;
#define fl6_sport		uli.ports.sport
#define fl6_dport		uli.ports.dport
#define fl6_icmp_type		uli.icmpt.type
#define fl6_icmp_code		uli.icmpt.code
#define fl6_ipsec_spi		uli.spi
#define fl6_mh_type		uli.mht.type
#define fl6_gre_key		uli.gre_key
};

struct flowidn {
	struct flowi_common	__fl_common;
#define flowidn_oif		__fl_common.flowic_oif
#define flowidn_iif		__fl_common.flowic_iif
#define flowidn_mark		__fl_common.flowic_mark
#define flowidn_scope		__fl_common.flowic_scope
#define flowidn_proto		__fl_common.flowic_proto
#define flowidn_flags		__fl_common.flowic_flags
	__le16			daddr;
	__le16			saddr;
	union flowi_uli		uli;
#define fld_sport		uli.ports.sport
#define fld_dport		uli.ports.dport
};

struct flowi {
	union {
		struct flowi_common	__fl_common;
		struct flowi4		ip4;
		struct flowi6		ip6;
		struct flowidn		dn;
	} u;
#define flowi_oif	u.__fl_common.flowic_oif
#define flowi_iif	u.__fl_common.flowic_iif
#define flowi_mark	u.__fl_common.flowic_mark
#define flowi_tos	u.__fl_common.flowic_tos
#define flowi_scope	u.__fl_common.flowic_scope
#define flowi_proto	u.__fl_common.flowic_proto
#define flowi_flags	u.__fl_common.flowic_flags
#define flowi_secid	u.__fl_common.flowic_secid
} __attribute__((__aligned__(BITS_PER_LONG/8)));

static inline struct flowi *flowi4_to_flowi(struct flowi4 *fl4)
{
	return container_of(fl4, struct flowi, u.ip4);
}

static inline struct flowi *flowi6_to_flowi(struct flowi6 *fl6)
{
	return container_of(fl6, struct flowi, u.ip6);
}

static inline struct flowi *flowidn_to_flowi(struct flowidn *fldn)
{
	return container_of(fldn, struct flowi, u.dn);
}

#define FLOW_DIR_IN	0
#define FLOW_DIR_OUT	1
#define FLOW_DIR_FWD	2

struct net;
struct sock;
struct flow_cache_ops;

struct flow_cache_object {
	const struct flow_cache_ops *ops;
};

struct flow_cache_ops {
	struct flow_cache_object *(*get)(struct flow_cache_object *);
	int (*check)(struct flow_cache_object *);
	void (*delete)(struct flow_cache_object *);
};

typedef struct flow_cache_object *(*flow_resolve_t)(
		struct net *net, const struct flowi *key, u16 family,
		u8 dir, struct flow_cache_object *oldobj, void *ctx);

extern struct flow_cache_object *flow_cache_lookup(
		struct net *net, const struct flowi *key, u16 family,
		u8 dir, flow_resolve_t resolver, void *ctx);

extern void flow_cache_flush(void);
extern atomic_t flow_cache_genid;

#endif
