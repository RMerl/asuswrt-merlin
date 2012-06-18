/*

	MACSAVE target
	Copyright (C) 2006 Jonathan Zarate

	Licensed under GNU GPL v2 or later.

*/
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/version.h>
#include <linux/if_ether.h>

#include <net/netfilter/nf_conntrack.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/netfilter_ipv4/ipt_MACSAVE.h>

static unsigned int
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
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
//	const struct ipt_MACSAVE_target_info *info = targinfo;
#else
//	const struct ipt_MACSAVE_target_info *info = par->targinfo;
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
	struct sk_buff *skb = *pskb;
#endif
	struct nf_conn *ct;
	enum ip_conntrack_info ctinfo;
	
	if ((skb_mac_header(skb) >= skb->head) && ((skb_mac_header(skb) + ETH_HLEN) <= skb->data)) {
		ct = nf_ct_get(skb, &ctinfo);
		if (ct) {
			memcpy(ct->macsave, eth_hdr(skb)->h_source, sizeof(ct->macsave));
		}
	}
	return IPT_CONTINUE;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,23)
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
	unsigned int hook_mask = par->hook_mask;
#endif
	if (hook_mask & ~((1 << NF_IP_PRE_ROUTING) | (1 << NF_IP_FORWARD) | (1 << NF_IP_LOCAL_IN))) {
		printk(KERN_ERR "MACSAVE: Valid only in PREROUTING, FORWARD and INPUT\n");
		return 0;
	}
	return 1;
}

static struct ipt_target macsave_target = { 
	.name = "MACSAVE",
	.family = AF_INET,
	.target = target,
	.targetsize = sizeof(struct ipt_MACSAVE_target_info),
	.checkentry = checkentry,
	.me = THIS_MODULE,
};

static int __init init(void)
{
	return xt_register_target(&macsave_target);
}

static void __exit fini(void)
{
	xt_unregister_target(&macsave_target);
}

module_init(init);
module_exit(fini);
MODULE_LICENSE("GPL");
