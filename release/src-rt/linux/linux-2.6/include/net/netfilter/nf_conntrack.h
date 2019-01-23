/*
 * Connection state tracking for netfilter.  This is separated from,
 * but required by, the (future) NAT layer; it can also be used by an iptables
 * extension.
 *
 * 16 Dec 2003: Yasuyuki Kozakai @USAGI <yasuyuki.kozakai@toshiba.co.jp>
 *	- generalize L3 protocol dependent part.
 *
 * Derived from include/linux/netfiter_ipv4/ip_conntrack.h
 */

#ifndef _NF_CONNTRACK_H
#define _NF_CONNTRACK_H

#include <linux/netfilter/nf_conntrack_common.h>

#ifdef __KERNEL__
#include <linux/bitops.h>
#include <linux/compiler.h>
#include <asm/atomic.h>

#include <linux/netfilter/nf_conntrack_tcp.h>
#include <linux/netfilter/nf_conntrack_sctp.h>
#include <linux/netfilter/nf_conntrack_proto_gre.h>
#include <net/netfilter/ipv4/nf_conntrack_icmp.h>
#include <net/netfilter/ipv6/nf_conntrack_icmpv6.h>

#include <net/netfilter/nf_conntrack_tuple.h>

#if defined(CONFIG_NETFILTER_XT_MATCH_LAYER7) || \
    defined(CONFIG_NETFILTER_XT_MATCH_LAYER7_MODULE)
struct ip_ct_layer7
{
	/*
	 * e.g. "http". NULL before decision. "unknown" after decision
	 * if no match.
	 */
	char *app_proto;
	/*
	 * application layer data so far. NULL after match decision.
	 */
	char *app_data;
	unsigned int app_data_len;
};
#endif

/* per conntrack: protocol private data */
union nf_conntrack_proto {
	/* insert conntrack proto private data here */
	struct ip_ct_sctp sctp;
	struct ip_ct_tcp tcp;
	struct ip_ct_icmp icmp;
	struct nf_ct_icmpv6 icmpv6;
	struct nf_ct_gre gre;
};

union nf_conntrack_expect_proto {
	/* insert expect proto private data here */
};

/* Add protocol helper include file here */
#include <linux/netfilter/nf_conntrack_ftp.h>
#include <linux/netfilter/nf_conntrack_pptp.h>
#include <linux/netfilter/nf_conntrack_h323.h>
#include <linux/netfilter/nf_conntrack_sane.h>
#include <linux/netfilter/nf_conntrack_autofw.h>

/* per conntrack: application helper private data */
union nf_conntrack_help {
	/* insert conntrack helper private data (master) here */
	struct nf_ct_ftp_master ct_ftp_info;
	struct nf_ct_pptp_master ct_pptp_info;
	struct nf_ct_h323_master ct_h323_info;
	struct nf_ct_sane_master ct_sane_info;
	struct nf_ct_autofw_master ct_autofw_info;
};

#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/timer.h>

#ifdef CONFIG_NETFILTER_DEBUG
#define NF_CT_ASSERT(x)							\
do {									\
	if (!(x))							\
		/* Wooah!  I'm tripping my conntrack in a frenzy of	\
		   netplay... */					\
		printk("NF_CT_ASSERT: %s:%i(%s)\n",			\
		       __FILE__, __LINE__, __FUNCTION__);		\
} while(0)
#else
#define NF_CT_ASSERT(x)
#endif

struct nf_conntrack_helper;

/* nf_conn feature for connections that have a helper */
struct nf_conn_help {
	/* Helper. if any */
	struct nf_conntrack_helper *helper;

	union nf_conntrack_help help;

	/* Current number of expected connections */
	unsigned int expecting;
};

#ifdef HNDCTF
#define CTF_FLAGS_CACHED	(1 << 31)	/* Indicates cached connection */
#define CTF_FLAGS_EXCLUDED	(1 << 30)
#define CTF_FLAGS_DNAT_CACHED	(1 << 29)
#define CTF_FLAGS_SNAT_CACHED	(1 << 28)
#define CTF_FLAGS_ROUTE_CACHED	(1 << 27)
#define CTF_FLAGS_REPLY_CACHED	(1 << 1)
#define CTF_FLAGS_ORG_CACHED	(1 << 0)
#endif

#include <net/netfilter/ipv4/nf_conntrack_ipv4.h>
#include <net/netfilter/ipv6/nf_conntrack_ipv6.h>

struct nf_conn
{
	/* Usage count in here is 1 for hash table/destruct timer, 1 per skb,
           plus 1 for any connection(s) we are `master' for */
	struct nf_conntrack ct_general;

	/* XXX should I move this to the tail ? - Y.K */
	/* These are my tuples; original and reply */
	struct nf_conntrack_tuple_hash tuplehash[IP_CT_DIR_MAX];

	/* Have we seen traffic both ways yet? (bitset) */
	unsigned long status;

	/* If we were expected by an expectation, this will be it */
	struct nf_conn *master;

	/* Timer function; drops refcnt when it goes off. */
	struct timer_list timeout;

#ifdef CONFIG_NF_CT_ACCT
	/* Accounting Information (same cache line as other written members) */
	struct ip_conntrack_counter counters[IP_CT_DIR_MAX];
#endif

	/* Unique ID that identifies this conntrack*/
	unsigned int id;

	/* features - nat, helper, ... used by allocating system */
	u_int32_t features;

#if defined(CONFIG_NF_CONNTRACK_MARK)
	u_int32_t mark;
#endif

#ifdef CONFIG_NF_CONNTRACK_SECMARK
	u_int32_t secmark;
#endif

#ifdef HNDCTF
	/* Timeout for the connection */
	u_int32_t expire_jiffies;

	/* Flags for connection attributes */
	u_int32_t ctf_flags;
#endif /* HNDCTF */

	/* Storage reserved for other modules: */
	union nf_conntrack_proto proto;

#if defined(CONFIG_IP_NF_TARGET_BCOUNT) || defined(CONFIG_IP_NF_TARGET_BCOUNT_MODULE)
	u_int32_t bcount;
#endif
#if defined(CONFIG_IP_NF_TARGET_MACSAVE) || defined(CONFIG_IP_NF_TARGET_MACSAVE_MODULE)
	unsigned char macsave[6];
#endif

#if defined(CONFIG_NETFILTER_XT_MATCH_LAYER7) || \
    defined(CONFIG_NETFILTER_XT_MATCH_LAYER7_MODULE)
	struct ip_ct_layer7 layer7;
#endif

	/* features dynamically at the end: helper, nat (both optional) */
	char data[0];
};

static inline struct nf_conn *
nf_ct_tuplehash_to_ctrack(const struct nf_conntrack_tuple_hash *hash)
{
	return container_of(hash, struct nf_conn,
			    tuplehash[hash->tuple.dst.dir]);
}

/* get master conntrack via master expectation */
#define master_ct(conntr) (conntr->master)

/* Alter reply tuple (maybe alter helper). */
extern void
nf_conntrack_alter_reply(struct nf_conn *conntrack,
			 const struct nf_conntrack_tuple *newreply);

/* Is this tuple taken? (ignoring any belonging to the given
   conntrack). */
extern int
nf_conntrack_tuple_taken(const struct nf_conntrack_tuple *tuple,
			 const struct nf_conn *ignored_conntrack);

/* Return conntrack_info and tuple hash for given skb. */
static inline struct nf_conn *
nf_ct_get(const struct sk_buff *skb, enum ip_conntrack_info *ctinfo)
{
	*ctinfo = skb->nfctinfo;
	return (struct nf_conn *)skb->nfct;
}

/* decrement reference count on a conntrack */
static inline void nf_ct_put(struct nf_conn *ct)
{
	NF_CT_ASSERT(ct);
	nf_conntrack_put(&ct->ct_general);
}

/* Protocol module loading */
extern int nf_ct_l3proto_try_module_get(unsigned short l3proto);
extern void nf_ct_l3proto_module_put(unsigned short l3proto);

extern struct nf_conntrack_tuple_hash *
__nf_conntrack_find(const struct nf_conntrack_tuple *tuple);

extern void nf_conntrack_hash_insert(struct nf_conn *ct);

extern void nf_conntrack_flush(void);

extern int nf_ct_invert_tuplepr(struct nf_conntrack_tuple *inverse,
				const struct nf_conntrack_tuple *orig);

extern void __nf_ct_refresh_acct(struct nf_conn *ct,
				 enum ip_conntrack_info ctinfo,
				 const struct sk_buff *skb,
				 unsigned long extra_jiffies,
				 int do_acct);

/* Refresh conntrack for this many jiffies and do accounting */
static inline void nf_ct_refresh_acct(struct nf_conn *ct,
				      enum ip_conntrack_info ctinfo,
				      const struct sk_buff *skb,
				      unsigned long extra_jiffies)
{
	__nf_ct_refresh_acct(ct, ctinfo, skb, extra_jiffies, 1);
}

/* Refresh conntrack for this many jiffies */
static inline void nf_ct_refresh(struct nf_conn *ct,
				 const struct sk_buff *skb,
				 unsigned long extra_jiffies)
{
	__nf_ct_refresh_acct(ct, 0, skb, extra_jiffies, 0);
}

/* These are for NAT.  Icky. */
/* Update TCP window tracking data when NAT mangles the packet */
extern void nf_conntrack_tcp_update(struct sk_buff *skb,
				    unsigned int dataoff,
				    struct nf_conn *conntrack,
				    int dir);

/* Call me when a conntrack is destroyed. */
extern void (*nf_conntrack_destroyed)(struct nf_conn *conntrack);

/* Fake conntrack entry for untracked connections */
extern struct nf_conn nf_conntrack_untracked;

extern int nf_ct_no_defrag;

/* Iterate over all conntracks: if iter returns true, it's deleted. */
extern void
nf_ct_iterate_cleanup(int (*iter)(struct nf_conn *i, void *data), void *data);
extern void nf_conntrack_free(struct nf_conn *ct);
extern struct nf_conn *
nf_conntrack_alloc(const struct nf_conntrack_tuple *orig,
		   const struct nf_conntrack_tuple *repl);

/* It's confirmed if it is, or has been in the hash table. */
static inline int nf_ct_is_confirmed(struct nf_conn *ct)
{
	return test_bit(IPS_CONFIRMED_BIT, &ct->status);
}

static inline int nf_ct_is_dying(struct nf_conn *ct)
{
	return test_bit(IPS_DYING_BIT, &ct->status);
}

static inline int nf_ct_is_untracked(const struct sk_buff *skb)
{
	return (skb->nfct == &nf_conntrack_untracked.ct_general);
}

extern unsigned int nf_conntrack_htable_size;
extern int nf_conntrack_checksum;
extern atomic_t nf_conntrack_count;
extern int nf_conntrack_max;

DECLARE_PER_CPU(struct ip_conntrack_stat, nf_conntrack_stat);
#define NF_CT_STAT_INC(count) (__get_cpu_var(nf_conntrack_stat).count++)
#define NF_CT_STAT_INC_ATOMIC(count)			\
do {							\
	local_bh_disable();				\
	__get_cpu_var(nf_conntrack_stat).count++;	\
	local_bh_enable();				\
} while (0)

/* no helper, no nat */
#define	NF_CT_F_BASIC	0
/* for helper */
#define	NF_CT_F_HELP	1
/* for nat. */
#define	NF_CT_F_NAT	2
#define NF_CT_F_NUM	4

extern int
nf_conntrack_register_cache(u_int32_t features, const char *name, size_t size);
extern void
nf_conntrack_unregister_cache(u_int32_t features);

/* valid combinations:
 * basic: nf_conn, nf_conn .. nf_conn_help
 * nat: nf_conn .. nf_conn_nat, nf_conn .. nf_conn_nat .. nf_conn help
 */
#ifdef CONFIG_NF_NAT_NEEDED
static inline struct nf_conn_nat *nfct_nat(const struct nf_conn *ct)
{
	unsigned int offset = sizeof(struct nf_conn);

	if (!(ct->features & NF_CT_F_NAT))
		return NULL;

	offset = ALIGN(offset, __alignof__(struct nf_conn_nat));
	return (struct nf_conn_nat *) ((void *)ct + offset);
}

static inline struct nf_conn_help *nfct_help(const struct nf_conn *ct)
{
	unsigned int offset = sizeof(struct nf_conn);

	if (!(ct->features & NF_CT_F_HELP))
		return NULL;
	if (ct->features & NF_CT_F_NAT) {
		offset = ALIGN(offset, __alignof__(struct nf_conn_nat));
		offset += sizeof(struct nf_conn_nat);
	}

	offset = ALIGN(offset, __alignof__(struct nf_conn_help));
	return (struct nf_conn_help *) ((void *)ct + offset);
}
#else /* No NAT */
static inline struct nf_conn_help *nfct_help(const struct nf_conn *ct)
{
	unsigned int offset = sizeof(struct nf_conn);

	if (!(ct->features & NF_CT_F_HELP))
		return NULL;

	offset = ALIGN(offset, __alignof__(struct nf_conn_help));
	return (struct nf_conn_help *) ((void *)ct + offset);
}
#endif /* CONFIG_NF_NAT_NEEDED */
#endif /* __KERNEL__ */
#endif /* _NF_CONNTRACK_H */
