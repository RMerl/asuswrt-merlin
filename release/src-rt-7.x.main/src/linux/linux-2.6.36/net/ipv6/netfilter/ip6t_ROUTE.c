/*
 * This implements the ROUTE v6 target, which enables you to setup unusual
 * routes not supported by the standard kernel routing table.
 *
 * Copyright (C) 2003 Cedric de Launois <delaunois@info.ucl.ac.be>
 *
 * v 1.1 2004/11/23
 *
 * This software is distributed under GNU GPL v2, 1991
 */

#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/version.h>

#include <linux/ipv6.h>
#include <linux/netfilter_ipv6/ip6_tables.h>
#include <linux/netfilter_ipv6/ip6t_ROUTE.h>
#include <linux/netdevice.h>
#include <linux/version.h>
#include <net/ipv6.h>
#include <net/ndisc.h>
#include <net/ip6_route.h>
#include <linux/icmpv6.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22)
#define ipv6_hdr(s) (s->nh.ipv6h)
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)
#include <net/net_namespace.h>
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26)
#define NF_INET_PRE_ROUTING NF_IP6_PRE_ROUTING
#define NF_INET_LOCAL_IN NF_IP6_LOCAL_IN
#define NF_INET_FORWARD NF_IP6_FORWARD
#define NF_INET_LOCAL_OUT NF_IP6_LOCAL_OUT
#define NF_INET_POST_ROUTING NF_IP6_POST_ROUTING
#endif

#if 1
#define DEBUGP printk
#else
#define DEBUGP(format, args...)
#endif

#define NIP6(addr) \
	ntohs((addr).s6_addr16[0]), \
	ntohs((addr).s6_addr16[1]), \
	ntohs((addr).s6_addr16[2]), \
	ntohs((addr).s6_addr16[3]), \
	ntohs((addr).s6_addr16[4]), \
	ntohs((addr).s6_addr16[5]), \
	ntohs((addr).s6_addr16[6]), \
	ntohs((addr).s6_addr16[7])

/* Route the packet according to the routing keys specified in
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
 * RETURN:  1 if the packet was succesfully routed to the 
 *            destination desired
 *          0 if the kernel routing table could not route the packet
 *            according to the keys specified
 */
static int 
route6(struct sk_buff *skb,
       unsigned int ifindex,
       const struct ip6t_route_target_info *route_info)
{
	struct rt6_info *rt = NULL;
	struct ipv6hdr *ipv6h = ipv6_hdr(skb);
	struct in6_addr *gw = (struct in6_addr*)&route_info->gw;

	DEBUGP("ip6t_ROUTE: called with: ");
	DEBUGP("DST=%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x ", NIP6(ipv6h->daddr));
	DEBUGP("GATEWAY=%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x ", NIP6(*gw));
	DEBUGP("OUT=%s\n", route_info->oif);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26)
	if (ipv6_addr_any(gw))
		rt = rt6_lookup(&init_net, &ipv6h->daddr, &ipv6h->saddr, ifindex, 1);
	else
		rt = rt6_lookup(&init_net, gw, &ipv6h->saddr, ifindex, 1);
#else
	if (ipv6_addr_any(gw))
		rt = rt6_lookup(&ipv6h->daddr, &ipv6h->saddr, ifindex, 1);
	else
		rt = rt6_lookup(gw, &ipv6h->saddr, ifindex, 1);
#endif

	if (!rt)
		goto no_route;

	DEBUGP("ip6t_ROUTE: routing gives: ");
	DEBUGP("DST=%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x ", NIP6(rt->rt6i_dst.addr));
	DEBUGP("GATEWAY=%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x ", NIP6(rt->rt6i_gateway));
	DEBUGP("OUT=%s\n", rt->rt6i_dev->name);

	if (ifindex && rt->rt6i_dev->ifindex!=ifindex)
		goto wrong_route;
	
	if (!rt->rt6i_nexthop) {
		DEBUGP("ip6t_ROUTE: discovering neighbor\n");
		rt->rt6i_nexthop = ndisc_get_neigh(rt->rt6i_dev, &rt->rt6i_dst.addr);
	}

	/* Drop old route. */
	//dst_release(skb->dst);
	//skb->dst = &rt->u.dst;
	skb->dev = rt->rt6i_dev;
	return 1;

 wrong_route:
	dst_release(&rt->dst);
 no_route:
	if (!net_ratelimit())
		return 0;

	printk("ip6t_ROUTE: no explicit route found ");
	if (ifindex)
		printk("via interface %s ", route_info->oif);
	if (!ipv6_addr_any(gw))
		printk("via gateway %04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x", NIP6(*gw));
	printk("\n");
	return 0;
}


/* Stolen from ip6_output_finish
 * PRE : skb->dev is set to the device we are leaving by
 *       skb->dst is not NULL
 * POST: the packet is sent with the link layer header pushed
 *       the packet is destroyed
 */
static void ip_direct_send(struct sk_buff *skb)
{
	struct dst_entry *dst = skb_dst(skb);
	struct hh_cache *hh = dst->hh;
	unsigned seq;

	if (hh) {
		do {
			seq = read_seqbegin(&hh->hh_lock);
			memcpy(skb->data - 16, hh->hh_data, 16);
		} while (read_seqretry(&hh->hh_lock, seq));
		skb_push(skb, hh->hh_len);
		hh->hh_output(skb);
	} else if (dst->neighbour)
		dst->neighbour->output(skb);
	else {
		if (net_ratelimit())
			DEBUGP(KERN_DEBUG "ip6t_ROUTE: no hdr & no neighbour cache!\n");
		kfree_skb(skb);
	}
}


static unsigned int 
route6_oif(const struct ip6t_route_target_info *route_info,
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
			DEBUGP("ip6t_ROUTE: oif interface %s not found\n", route_info->oif);

		if (route_info->flags & IP6T_ROUTE_CONTINUE)
			return IP6T_CONTINUE;
		else
			return NF_DROP;
	}

	/* Trying the standard way of routing packets */
	if (route6(skb, ifindex, route_info)) {
		dev_put(dev_out);
		if (route_info->flags & IP6T_ROUTE_CONTINUE)
			return IP6T_CONTINUE;
		
		ip_direct_send(skb);
		return NF_STOLEN;
	} else 
		return NF_DROP;
}


static unsigned int 
route6_gw(const struct ip6t_route_target_info *route_info,
	  struct sk_buff *skb) 
{
	if (route6(skb, 0, route_info)) {
		if (route_info->flags & IP6T_ROUTE_CONTINUE)
			return IP6T_CONTINUE;

		ip_direct_send(skb);
		return NF_STOLEN;
	} else
		return NF_DROP;
}

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
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
target(struct sk_buff *skb,
       const struct xt_target_param *par)
#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36) */
target(struct sk_buff *skb,
       const struct xt_action_param *par)
#endif
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,28)
	const struct ip6t_route_target_info *route_info = targinfo;
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
	const struct ip6t_route_target_info *route_info = par->targinfo;
	unsigned int hooknum = par->hooknum;
#else
	const struct ip6t_route_target_info *route_info = par->targinfo;
	unsigned int hooknum = par->hooknum;
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
	struct sk_buff *skb = *pskb;
#endif
	struct in6_addr *gw = (struct in6_addr*)&route_info->gw;
	unsigned int res;

	if (route_info->flags & IP6T_ROUTE_CONTINUE)
		goto do_it;

	/* If we are at PREROUTING or INPUT hook
	 * the TTL isn't decreased by the IP stack
	 */
	if (hooknum == NF_INET_PRE_ROUTING ||
	    hooknum == NF_INET_LOCAL_IN) {

		struct ipv6hdr *ipv6h = ipv6_hdr(skb);

		if (ipv6h->hop_limit <= 1) {
			/* Force OUTPUT device used as source address */
			skb->dev = skb_dst(skb)->dev;

			icmpv6_send(skb, ICMPV6_TIME_EXCEED, 
				    ICMPV6_EXC_HOPLIMIT, 0);

			return NF_DROP;
		}

		ipv6h->hop_limit--;
	}

	if ((route_info->flags & IP6T_ROUTE_TEE)) {
		/*
		 * Copy the skb, and route the copy. Will later return
		 * IP6T_CONTINUE for the original skb, which should continue
		 * on its way as if nothing happened. The copy should be
		 * independantly delivered to the ROUTE --gw.
		 */
		skb = skb_copy(skb, GFP_ATOMIC);
		if (!skb) {
			if (net_ratelimit()) 
				DEBUGP(KERN_DEBUG "ip6t_ROUTE: copy failed!\n");
			return IP6T_CONTINUE;
		}
	}

do_it:
	if (route_info->oif[0]) {
		res = route6_oif(route_info, skb);
	} else if (!ipv6_addr_any(gw)) {
		res = route6_gw(route_info, skb);
	} else {
		if (net_ratelimit()) 
			DEBUGP(KERN_DEBUG "ip6t_ROUTE: no parameter !\n");
		res = IP6T_CONTINUE;
	}

	if ((route_info->flags & IP6T_ROUTE_TEE))
		res = IP6T_CONTINUE;

	return res;
}


#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,16)
static int
checkentry(const char *tablename,
	   const struct ip6t_entry *e,
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
#endif

	if (strcmp(tablename, "mangle") != 0) {
		printk("ip6t_ROUTE: can only be called from \"mangle\" table.\n");
		return 0;
	}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
	if (targinfosize != IP6T_ALIGN(sizeof(struct ip6t_route_target_info))) {
		printk(KERN_WARNING "ip6t_ROUTE: targinfosize %u != %Zu\n",
		       targinfosize,
		       IP6T_ALIGN(sizeof(struct ip6t_route_target_info)));
		return 0;
	}
#endif

	return 1;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,21)
static struct xt_target ip6t_route_reg = {
#else
static struct ip6t_target ip6t_route_reg = {
#endif
	.name		= "ROUTE",
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,21)
	.family		= AF_INET6,
#endif
	.target		= target,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,17)
	.targetsize	= sizeof(struct ip6t_route_target_info),
#endif
	.checkentry	= checkentry,
	.me		= THIS_MODULE
};


static int __init init(void)
{
	printk(KERN_DEBUG "registering ipv6 ROUTE target\n");
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,21)
	if (xt_register_target(&ip6t_route_reg))
#else
	if (ip6t_register_target(&ip6t_route_reg))
#endif
		return -EINVAL;

	return 0;
}


static void __exit fini(void)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,21)
	xt_unregister_target(&ip6t_route_reg);
#else
	ip6t_unregister_target(&ip6t_route_reg);
#endif
}

module_init(init);
module_exit(fini);
MODULE_LICENSE("GPL");
