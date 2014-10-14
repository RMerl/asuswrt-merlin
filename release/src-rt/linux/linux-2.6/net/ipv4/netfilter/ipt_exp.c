/*

	Experimental Netfilter Crap
	Copyright (C) 2006 Jonathan Zarate

*/
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/version.h>
#include <linux/file.h>
#include <net/sock.h>

#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/netfilter_ipv4/ipt_exp.h>
#include "../../bridge/br_private.h"


#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,23)
static int
#else
static bool
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,28)
match(const struct sk_buff *skb, const struct net_device *in, const struct net_device *out,
	const struct xt_match *match, const void *matchinfo, int offset,
	unsigned int protoff, int *hotdrop)
#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,28) */
match(const struct sk_buff *skb, const struct xt_match_param *par)
#endif
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,28)
//	const struct ipt_exp_info *info = matchinfo;
#else
//	const struct ipt_exp_info *info = par->matchinfo;
#endif

	if ((skb_mac_header(skb) >= skb->head) && ((skb_mac_header(skb) + ETH_HLEN) <= skb->data)) {
		printk(KERN_INFO "exp src=%02X:%02X:%02X:%02X:%02X:%02X dst=%02X:%02X:%02X:%02X:%02X:%02X\n", 
			eth_hdr(skb)->h_source[0], eth_hdr(skb)->h_source[1], eth_hdr(skb)->h_source[2], 
			eth_hdr(skb)->h_source[3], eth_hdr(skb)->h_source[4], eth_hdr(skb)->h_source[5], 
			eth_hdr(skb)->h_dest[0], eth_hdr(skb)->h_dest[1], eth_hdr(skb)->h_dest[2], 
			eth_hdr(skb)->h_dest[3], eth_hdr(skb)->h_dest[4], eth_hdr(skb)->h_dest[5]);
		return 1;
	}
	printk(KERN_INFO "exp mac=%p head=%p in=%p\n", skb_mac_header(skb), skb->head, in);
	return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,23)
static int
#else
static bool
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,28)
checkentry(const char *tablename, const void *inf, const struct xt_match *match,
	void *matchinfo, unsigned int hook_mask)
#else
checkentry(const struct xt_mtchk_param *par)
#endif
{
	return 1;
}

static struct xt_match exp_match = {
	.name		= "exp",
	.family		= AF_INET,
	.match		= &match,
	.matchsize	= sizeof(struct ipt_exp_info),
	.checkentry	= &checkentry,
	.destroy	= NULL,
	.me		= THIS_MODULE
};

static int __init init(void)
{
	printk(KERN_INFO "exp init " __DATE__ " " __TIME__ "\n");
	return xt_register_match(&exp_match);
}

static void __exit fini(void)
{
	printk(KERN_INFO "exp fini\n");
	xt_unregister_match(&exp_match);
}

module_init(init);
module_exit(fini);
MODULE_LICENSE("GPL");
