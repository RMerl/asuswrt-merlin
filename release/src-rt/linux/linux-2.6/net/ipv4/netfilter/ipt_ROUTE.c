/*
 * This implements the ROUTE target, which enables you to setup unusual
 * routes not supported by the standard kernel routing table.
 *
 * Copyright (C) 2002 Cedric de Launois <delaunois@info.ucl.ac.be>
 *
 * v 1.11 2004/11/23
 *
 * This software is distributed under GNU GPL v2, 1991
 */

#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/version.h>
#include <linux/ip.h>
#include <linux/netfilter_ipv4/ip_tables.h>

#include <linux/netfilter_ipv4/ipt_ROUTE.h>
#include <linux/netdevice.h>
#include <linux/route.h>
#include <linux/version.h>
#include <linux/if_arp.h>
#include <net/ip.h>
#include <net/route.h>
#include <net/icmp.h>
#include <net/checksum.h>

#include <net/netfilter/nf_conntrack.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)
#include <net/net_namespace.h>
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26)
#define NF_INET_PRE_ROUTING NF_IP_PRE_ROUTING
#define NF_INET_LOCAL_IN NF_IP_LOCAL_IN
#define NF_INET_FORWARD NF_IP_FORWARD
#define NF_INET_LOCAL_OUT NF_IP_LOCAL_OUT
#define NF_INET_POST_ROUTING NF_IP_POST_ROUTING
#endif

#if 0
#define DEBUGP printk
#else
#define DEBUGP(format, args...)
#endif

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Cedric de Launois <delaunois@info.ucl.ac.be>");
MODULE_DESCRIPTION("iptables ROUTE target module");

/* Try to route the packet according to the routing keys specified in
 * route_info. Keys are :
 *  - ifindex : 
 *      0 if no oif preferred, 
 *      otherwise set to the index of the desired oif
 *  - route_info->gw :
 *      0 if no gateway specified,
 *      otherwise set to the next host to which the pkt must be routed
 * If success, skb->dev is the output device to which the packet must 
 * be sent and skb->dst is not NULL
 *
 * RETURN: -1 if an error occured
 *          1 if the packet was succesfully routed to the 
 *            destination desired
 *          0 if the kernel routing table could not route the packet
 *            according to the keys specified
 */
static int route(struct sk_buff *skb,
		 unsigned int ifindex,
		 const struct ipt_route_target_info *route_info)
{
	int err;
	struct rtable *rt;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22)
	struct iphdr *iph = ip_hdr(skb);
#else
	struct iphdr *iph = skb->nh.iph;
#endif
	struct flowi fl = {
		.oif = ifindex,
		.nl_u = {
			.ip4_u = {
				.daddr = iph->daddr,
				.saddr = 0,
				.tos = RT_TOS(iph->tos),
				.scope = RT_SCOPE_UNIVERSE,
			}
		} 
	};
	
	/* The destination address may be overloaded by the target */
	if (route_info->gw)
		fl.fl4_dst = route_info->gw;
	
	/* Trying to route the packet using the standard routing table. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26)
	if ((err = ip_route_output_key(&init_net, &rt, &fl))) {
#else
	if ((err = ip_route_output_key(&rt, &fl))) {
#endif
		if (net_ratelimit()) 
			DEBUGP("ipt_ROUTE: couldn't route pkt (err: %i)",err);
		return -1;
	}
	
	/* Drop old route. */
	dst_release(skb->dst);
	skb->dst = NULL;

	/* Success if no oif specified or if the oif correspond to the 
	 * one desired */
	if (!ifindex || rt->u.dst.dev->ifindex == ifindex) {
		skb->dst = &rt->u.dst;
		skb->dev = skb->dst->dev;
		skb->protocol = htons(ETH_P_IP);
		return 1;
	}
	
	/* The interface selected by the routing table is not the one
	 * specified by the user. This may happen because the dst address
	 * is one of our own addresses.
	 */
	if (net_ratelimit()) 
		DEBUGP("ipt_ROUTE: failed to route as desired gw=%u.%u.%u.%u oif=%i (got oif=%i)\n", 
		       NIPQUAD(route_info->gw), ifindex, rt->u.dst.dev->ifindex);
	
	return 0;
}


/* Stolen from ip_finish_output2
 * PRE : skb->dev is set to the device we are leaving by
 *       skb->dst is not NULL
 * POST: the packet is sent with the link layer header pushed
 *       the packet is destroyed
 */
static void ip_direct_send(struct sk_buff *skb)
{
	struct dst_entry *dst = skb->dst;
	struct hh_cache *hh = dst->hh;
	struct net_device *dev = dst->dev;
	int hh_len = LL_RESERVED_SPACE(dev);
	unsigned seq;

	/* Be paranoid, rather than too clever. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)
	if (unlikely(skb_headroom(skb) < hh_len && dev->header_ops)) {
#else
	if (unlikely(skb_headroom(skb) < hh_len && dev->hard_header)) {
#endif
		struct sk_buff *skb2;

		skb2 = skb_realloc_headroom(skb, LL_RESERVED_SPACE(dev));
		if (skb2 == NULL) {
			kfree_skb(skb);
			return;
		}
		if (skb->sk)
			skb_set_owner_w(skb2, skb->sk);
		kfree_skb(skb);
		skb = skb2;
	}

	if (hh) {
		do {
			int hh_alen;

			seq = read_seqbegin(&hh->hh_lock);
			hh_alen = HH_DATA_ALIGN(hh->hh_len);
			memcpy(skb->data - hh_alen, hh->hh_data, hh_alen);
		} while (read_seqretry(&hh->hh_lock, seq));
		skb_push(skb, hh->hh_len);
		hh->hh_output(skb);
	} else if (dst->neighbour)
		dst->neighbour->output(skb);
	else {
		if (net_ratelimit())
			DEBUGP(KERN_DEBUG "ipt_ROUTE: no hdr & no neighbour cache!\n");
		kfree_skb(skb);
	}
}


/* PRE : skb->dev is set to the device we are leaving by
 * POST: - the packet is directly sent to the skb->dev device, without 
 *         pushing the link layer header.
 *       - the packet is destroyed
 */
static inline int dev_direct_send(struct sk_buff *skb)
{
	return dev_queue_xmit(skb);
}


static unsigned int route_oif(const struct ipt_route_target_info *route_info,
			      struct sk_buff *skb) 
{
	unsigned int ifindex = 0;
	struct net_device *dev_out = NULL;

	/* The user set the interface name to use.
	 * Getting the current interface index.
	 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)
	if ((dev_out = dev_get_by_name(&init_net, route_info->oif))) {
#else
	if ((dev_out = dev_get_by_name(route_info->oif))) {
#endif
		ifindex = dev_out->ifindex;
	} else {
		/* Unknown interface name : packet dropped */
		if (net_ratelimit()) 
			DEBUGP("ipt_ROUTE: oif interface %s not found\n", route_info->oif);
		return NF_DROP;
	}

	/* Trying the standard way of routing packets */
	switch (route(skb, ifindex, route_info)) {
	case 1:
		dev_put(dev_out);
		if (route_info->flags & IPT_ROUTE_CONTINUE)
			return IPT_CONTINUE;

		ip_direct_send(skb);
		return NF_STOLEN;

	case 0:
		/* Failed to send to oif. Trying the hard way */
		if (route_info->flags & IPT_ROUTE_CONTINUE)
			return NF_DROP;

		if (net_ratelimit()) 
			DEBUGP("ipt_ROUTE: forcing the use of %i\n",
			       ifindex);

		/* We have to force the use of an interface.
		 * This interface must be a tunnel interface since
		 * otherwise we can't guess the hw address for
		 * the packet. For a tunnel interface, no hw address
		 * is needed.
		 */
		if ((dev_out->type != ARPHRD_TUNNEL)
		    && (dev_out->type != ARPHRD_IPGRE)) {
			if (net_ratelimit()) 
				DEBUGP("ipt_ROUTE: can't guess the hw addr !\n");
			dev_put(dev_out);
			return NF_DROP;
		}
	
		/* Send the packet. This will also free skb
		 * Do not go through the POST_ROUTING hook because 
		 * skb->dst is not set and because it will probably
		 * get confused by the destination IP address.
		 */
		skb->dev = dev_out;
		dev_direct_send(skb);
		dev_put(dev_out);
		return NF_STOLEN;
		
	default:
		/* Unexpected error */
		dev_put(dev_out);
		return NF_DROP;
	}
}


static unsigned int route_iif(const struct ipt_route_target_info *route_info,
			      struct sk_buff *skb) 
{
	struct net_device *dev_in = NULL;

	/* Getting the current interface index. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)
	if (!(dev_in = dev_get_by_name(&init_net, route_info->iif))) {
#else
	if (!(dev_in = dev_get_by_name(route_info->iif))) {
#endif
		if (net_ratelimit()) 
			DEBUGP("ipt_ROUTE: iif interface %s not found\n", route_info->iif);
		return NF_DROP;
	}

	skb->dev = dev_in;
	dst_release(skb->dst);
	skb->dst = NULL;

	netif_rx(skb);
	dev_put(dev_in);
	return NF_STOLEN;
}


static unsigned int route_gw(const struct ipt_route_target_info *route_info,
			     struct sk_buff *skb) 
{
	if (route(skb, 0, route_info)!=1)
		return NF_DROP;

	if (route_info->flags & IPT_ROUTE_CONTINUE)
		return IPT_CONTINUE;

	ip_direct_send(skb);
	return NF_STOLEN;
}


/* To detect and deter routed packet loopback when using the --tee option,
 * we take a page out of the raw.patch book: on the copied skb, we set up
 * a fake ->nfct entry, pointing to the local &route_tee_track. We skip
 * routing packets when we see they already have that ->nfct.
 */

static struct nf_conn route_tee_track;

static unsigned int
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
target(struct sk_buff **pskb,
       unsigned int hooknum,
       const struct net_device *in,
       const struct net_device *out,
       const void *targinfo,
       void *userinfo)
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2,6,17)
target(struct sk_buff **pskb,
       const struct net_device *in,
       const struct net_device *out,
       unsigned int hooknum,
       const void *targinfo,
       void *userinfo)
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
target(struct sk_buff **pskb,
       const struct net_device *in,
       const struct net_device *out,
       unsigned int hooknum,
       const struct xt_target *target,
       const void *targinfo,
       void *userinfo)
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
target(struct sk_buff **pskb,
       const struct net_device *in,
       const struct net_device *out,
       unsigned int hooknum,
       const struct xt_target *target,
       const void *targinfo)
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2,6,28)
target(struct sk_buff *skb,
       const struct net_device *in,
       const struct net_device *out,
       unsigned int hooknum,
       const struct xt_target *target,
       const void *targinfo)
#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,28) */
target(struct sk_buff *skb,
       const struct xt_target_param *par)
#endif
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,28)
	const struct ipt_route_target_info *route_info = targinfo;
#else
	const struct ipt_route_target_info *route_info = par->targinfo;
	unsigned int hooknum = par->hooknum;
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
	struct sk_buff *skb = *pskb;
#endif
	unsigned int res;

	if (skb->nfct == &route_tee_track.ct_general) {
		/* Loopback - a packet we already routed, is to be
		 * routed another time. Avoid that, now.
		 */
		if (net_ratelimit()) 
			DEBUGP(KERN_DEBUG "ipt_ROUTE: loopback - DROP!\n");
		return NF_DROP;
	}

	/* If we are at PREROUTING or INPUT hook
	 * the TTL isn't decreased by the IP stack
	 */
	if (hooknum == NF_INET_PRE_ROUTING ||
	    hooknum == NF_INET_LOCAL_IN) {

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22)
		struct iphdr *iph = ip_hdr(skb);
#else
		struct iphdr *iph = skb->nh.iph;
#endif
		if (iph->ttl <= 1) {
			struct rtable *rt;
			struct flowi fl = {
				.oif = 0,
				.nl_u = {
					.ip4_u = {
						.daddr = iph->daddr,
						.saddr = iph->saddr,
						.tos = RT_TOS(iph->tos),
						.scope = ((iph->tos & RTO_ONLINK) ?
							  RT_SCOPE_LINK :
							  RT_SCOPE_UNIVERSE)
					}
				} 
			};

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26)
			if (ip_route_output_key(&init_net, &rt, &fl)) {
#else
			if (ip_route_output_key(&rt, &fl)) {
#endif
				return NF_DROP;
			}

			if (skb->dev == rt->u.dst.dev) {
				/* Drop old route. */
				dst_release(skb->dst);
				skb->dst = &rt->u.dst;

				/* this will traverse normal stack, and 
				 * thus call conntrack on the icmp packet */
				icmp_send(skb, ICMP_TIME_EXCEEDED, 
					  ICMP_EXC_TTL, 0);
			}

			return NF_DROP;
		}

		/*
		 * If we are at INPUT the checksum must be recalculated since
		 * the length could change as the result of a defragmentation.
		 */
		if(hooknum == NF_INET_LOCAL_IN) {
			iph->ttl = iph->ttl - 1;
			iph->check = 0;
			iph->check = ip_fast_csum((unsigned char *)iph, iph->ihl);
		} else {
			ip_decrease_ttl(iph);
		}
	}

	if ((route_info->flags & IPT_ROUTE_TEE)) {
		/*
		 * Copy the skb, and route the copy. Will later return
		 * IPT_CONTINUE for the original skb, which should continue
		 * on its way as if nothing happened. The copy should be
		 * independantly delivered to the ROUTE --gw.
		 */
		skb = skb_copy(skb, GFP_ATOMIC);
		if (!skb) {
			if (net_ratelimit()) 
				DEBUGP(KERN_DEBUG "ipt_ROUTE: copy failed!\n");
			return IPT_CONTINUE;
		}
	}

	/* Tell conntrack to forget this packet since it may get confused 
	 * when a packet is leaving with dst address == our address.
	 * Good idea ? Dunno. Need advice.
	 *
	 * NEW: mark the skb with our &route_tee_track, so we avoid looping
	 * on any already routed packet.
	 */
	if (!(route_info->flags & IPT_ROUTE_CONTINUE)) {
		nf_conntrack_put(skb->nfct);
		skb->nfct = &route_tee_track.ct_general;
		skb->nfctinfo = IP_CT_NEW;
		nf_conntrack_get(skb->nfct);
	}

	if (route_info->oif[0] != '\0') {
		res = route_oif(route_info, skb);
	} else if (route_info->iif[0] != '\0') {
		res = route_iif(route_info, skb);
	} else if (route_info->gw) {
		res = route_gw(route_info, skb);
	} else {
		if (net_ratelimit()) 
			DEBUGP(KERN_DEBUG "ipt_ROUTE: no parameter !\n");
		res = IPT_CONTINUE;
	}

	if ((route_info->flags & IPT_ROUTE_TEE))
		res = IPT_CONTINUE;

	return res;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,16)
static int
checkentry(const char *tablename,
	   const struct ipt_entry *e,
	   void *targinfo,
	   unsigned int targinfosize,
	   unsigned int hook_mask)
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2,6,17)
static int
checkentry(const char *tablename,
	   const void *e,
	   void *targinfo,
	   unsigned int targinfosize,
	   unsigned int hook_mask)
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
static int
checkentry(const char *tablename,
	   const void *e,
	   const struct xt_target *target,
	   void *targinfo,
	   unsigned int targinfosize,
	   unsigned int hook_mask)
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2,6,23)
static int
checkentry(const char *tablename,
	   const void *e,
	   const struct xt_target *target,
	   void *targinfo,
	   unsigned int hook_mask)
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2,6,28)
static bool
checkentry(const char *tablename,
	   const void *e,
	   const struct xt_target *target,
	   void *targinfo,
	   unsigned int hook_mask)
#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,28) */
static bool
checkentry(const struct xt_tgchk_param *par)
#endif
{

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,28)
	const char *tablename = par->table;
	unsigned int hook_mask = par->hook_mask;
#endif

	if (strcmp(tablename, "mangle") != 0) {
		printk("ipt_ROUTE: bad table `%s', use the `mangle' table.\n",
		       tablename);
		return 0;
	}

	if (hook_mask & ~(  (1 << NF_INET_PRE_ROUTING)
			    | (1 << NF_INET_LOCAL_IN)
			    | (1 << NF_INET_FORWARD)
			    | (1 << NF_INET_LOCAL_OUT)
			    | (1 << NF_INET_POST_ROUTING))) {
		printk("ipt_ROUTE: bad hook\n");
		return 0;
	}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
	if (targinfosize != IPT_ALIGN(sizeof(struct ipt_route_target_info))) {
		printk(KERN_WARNING "ipt_ROUTE: targinfosize %u != %Zu\n",
		       targinfosize,
		       IPT_ALIGN(sizeof(struct ipt_route_target_info)));
		return 0;
	}
#endif

	return 1;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,21)
static struct ipt_target xt_route_reg = { 
#else
static struct ipt_target ipt_route_reg = { 
#endif
	.name = "ROUTE",
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,21)
	.family		= AF_INET,
#endif
	.target = target,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,17)
	.targetsize = sizeof(struct ipt_route_target_info),
#endif
	.checkentry = checkentry,
	.me = THIS_MODULE,
};

static int __init init(void)
{
	/* Set up fake conntrack (stolen from raw.patch):
	    - to never be deleted, not in any hashes */
	atomic_set(&route_tee_track.ct_general.use, 1);
	/*  - and look it like as a confirmed connection */
	set_bit(IPS_CONFIRMED_BIT, &route_tee_track.status);
	/* Initialize fake conntrack so that NAT will skip it */
	route_tee_track.status |= IPS_NAT_DONE_MASK;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,21)
	return xt_register_target(&xt_route_reg);
#else
	return ipt_register_target(&ipt_route_reg);
#endif
}


static void __exit fini(void)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,21)
	xt_unregister_target(&xt_route_reg);
#else
	ipt_unregister_target(&ipt_route_reg);
#endif
}

module_init(init);
module_exit(fini);
