/*

	bcount match (experimental)
	Copyright (C) 2006 Jonathan Zarate

	Licensed under GNU GPL v2 or later.

*/
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/version.h>
#include <net/sock.h>
#include <net/netfilter/nf_conntrack.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/netfilter_ipv4/ipt_bcount.h>

//	#define LOG			printk
#define LOG(...)	do { } while (0);


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
	const struct ipt_bcount_match *info;
	struct nf_conn *ct;
	enum ip_conntrack_info ctinfo;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,28)
	info = matchinfo;
#else
	info = par->matchinfo;
#endif
	ct = nf_ct_get((struct sk_buff *)skb, &ctinfo);
	if (!ct) return !info->invert;
	return ((ct->bcount >= info->min) && (ct->bcount <= info->max)) ^ info->invert;
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

static struct xt_match bcount_match = {
	.name		= "bcount",
	.family		= AF_INET,
	.match		= &match,
	.matchsize	= sizeof(struct ipt_bcount_match),
	.checkentry	= &checkentry,
	.destroy	= NULL,
	.me		= THIS_MODULE
};

static int __init init(void)
{
	LOG(KERN_INFO "ipt_bcount <" __DATE__ " " __TIME__ "> loaded\n");
	return xt_register_match(&bcount_match);
}

static void __exit fini(void)
{
	xt_unregister_match(&bcount_match);
}

module_init(init);
module_exit(fini);


MODULE_AUTHOR("Jonathan Zarate");
MODULE_DESCRIPTION("bcount match");
MODULE_LICENSE("GPL");
