/*
 * Packet matching code.
 *
 * Copyright (C) 1999 Paul `Rusty' Russell & Michael J. Neuling
 * Copyright (C) 2009-2002 Netfilter core team <coreteam@netfilter.org>
 *
 * 19 Jan 2002 Harald Welte <laforge@gnumonks.org>
 * 	- increase module usage count as soon as we have rules inside
 * 	  a table
 */
#include <linux/config.h>
#include <linux/cache.h>
#include <linux/skbuff.h>
#include <linux/kmod.h>
#include <linux/vmalloc.h>
#include <linux/netdevice.h>
#include <linux/module.h>
#include <linux/ip.h>
#include <net/route.h>
#include <net/ip.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <net/netfilter/nf_nat_core.h>
#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_core.h>
#include <linux/netfilter/nf_conntrack_common.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#ifdef HNDCTF
#include <typedefs.h>
extern bool ip_conntrack_is_ipc_allowed(struct sk_buff *skb, u_int32_t hooknum);
extern void ip_conntrack_ipct_add(struct sk_buff *skb, u_int32_t hooknum,
                                  struct nf_conn *ct, enum ip_conntrack_info ci,
                                  struct nf_conntrack_tuple *manip);
#endif /* HNDCTF */


#define DEBUGP(format, args...)

typedef int (*bcmNatHitHook)(struct sk_buff *skb);
extern int bcm_nat_hit_hook_func(bcmNatHitHook hook_func);

typedef int (*bcmNatBindHook)(struct nf_conn *ct,enum ip_conntrack_info ctinfo,
	    						unsigned int hooknum, struct sk_buff **pskb);
extern int bcm_nat_bind_hook_func(bcmNatBindHook hook_func);

extern int
bcm_manip_pkt(u_int16_t proto,
	  struct sk_buff **pskb,
	  unsigned int iphdroff,
	  const struct nf_conntrack_tuple *target,
	  enum nf_nat_manip_type maniptype);

/* 
 * Send packets to output.
 */
static inline int bcm_fast_path_output(struct sk_buff *skb)
{
	int ret = 0;
	struct dst_entry *dst = skb->dst;
	struct hh_cache *hh = dst->hh;

	if (hh) {
		unsigned seq;
		int hh_len;

		do {
			int hh_alen;
			seq = read_seqbegin(&hh->hh_lock);
			hh_len = hh->hh_len;
			hh_alen = HH_DATA_ALIGN(hh_len);
			memcpy(skb->data - hh_alen, hh->hh_data, hh_alen);
			} while (read_seqretry(&hh->hh_lock, seq));

			skb_push(skb, hh_len);
		ret = hh->hh_output(skb); 
		if (ret==1) 
			return 0; /* Don't return 1 */
	} else if (dst->neighbour) {
		ret = dst->neighbour->output(skb);  
		if (ret==1) 
			return 0; /* Don't return 1 */
	}
	return ret;
}

static inline int ip_skb_dst_mtu(struct sk_buff *skb)
{
	struct inet_sock *inet = skb->sk ? inet_sk(skb->sk) : NULL;

	return (inet && inet->pmtudisc == IP_PMTUDISC_PROBE) ?
	       skb->dst->dev->mtu : dst_mtu(skb->dst);
}

static inline int bcm_fast_path(struct sk_buff *skb)
{
	if (skb->dst == NULL) {
		struct iphdr *iph = ip_hdr(skb);
		struct net_device *dev = skb->dev;

		if (ip_route_input(skb, iph->daddr, iph->saddr, iph->tos, dev)) {
			return NF_DROP;
		}
		/*  Change skb owner to output device */
		skb->dev = skb->dst->dev;
	}

	if (skb->dst) {
		if (skb->len > ip_skb_dst_mtu(skb) && !skb_is_gso(skb))
			return ip_fragment(skb, bcm_fast_path_output);
		else
			return bcm_fast_path_output(skb);
	}

	kfree_skb(skb);
	return -EINVAL;
}

static inline int
bcm_do_bindings(struct nf_conn *ct,
		enum ip_conntrack_info ctinfo,
		struct sk_buff **pskb,
		struct nf_conntrack_l3proto *l3proto,
		struct nf_conntrack_l4proto *l4proto)
{
	struct iphdr *iph = ip_hdr(*pskb);
	unsigned int i;
	static int hn[2] = {NF_IP_PRE_ROUTING, NF_IP_POST_ROUTING};
	enum ip_conntrack_dir dir = CTINFO2DIR(ctinfo);
#ifdef HNDCTF
	bool enabled = ip_conntrack_is_ipc_allowed(*pskb, NF_IP_PRE_ROUTING);
#endif /* HNDCTF */

	for (i = 0; i < 2; i++) {
		enum nf_nat_manip_type mtype = HOOK2MANIP(hn[i]);
		unsigned long statusbit;

		if (mtype == IP_NAT_MANIP_SRC)
			statusbit = IPS_SRC_NAT;
		else
			statusbit = IPS_DST_NAT;

		/* Invert if this is reply dir. */
		if (dir == IP_CT_DIR_REPLY)
			statusbit ^= IPS_NAT_MASK;

		if (ct->status & statusbit) {
			struct nf_conntrack_tuple target;

			if (skb_cloned(*pskb) && !(*pskb)->sk) {
				struct sk_buff *nskb = skb_copy(*pskb, GFP_ATOMIC);
				if (!nskb)
					return NF_DROP;

				kfree_skb(*pskb);
				*pskb = nskb;
			}

			if ((*pskb)->dst == NULL && mtype == IP_NAT_MANIP_SRC) {
				struct net_device *dev = (*pskb)->dev;
				if (ip_route_input((*pskb), iph->daddr, iph->saddr, iph->tos, dev))
					return NF_DROP;
				/* Change skb owner */
				(*pskb)->dev = (*pskb)->dst->dev;
			}

			/* We are aiming to look like inverse of other direction. */
			nf_ct_invert_tuple(&target, &ct->tuplehash[!dir].tuple, l3proto, l4proto);
#ifdef HNDCTF
			if (enabled)
				ip_conntrack_ipct_add(*pskb, hn[i], ct, ctinfo, &target);
#endif /* HNDCTF */
			if (!bcm_manip_pkt(target.dst.protonum, pskb, 0, &target, mtype))
				return NF_DROP;
		}
	}

	return NF_FAST_NAT;
}

static int __init bcm_nat_init(void)
{
	bcm_nat_hit_hook_func (bcm_fast_path);
	bcm_nat_bind_hook_func ((bcmNatBindHook)bcm_do_bindings);
	printk("BCM fast NAT: INIT\n");
	return 0;
}

static void __exit bcm_nat_fini(void)
{
	bcm_nat_hit_hook_func (NULL);
	bcm_nat_bind_hook_func (NULL);
}

module_init(bcm_nat_init);
module_exit(bcm_nat_fini);
MODULE_LICENSE("Proprietary");
