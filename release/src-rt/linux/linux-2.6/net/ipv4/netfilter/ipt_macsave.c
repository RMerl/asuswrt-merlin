/*

	macsave match
	Copyright (C) 2006 Jonathan Zarate

	Licensed under GNU GPL v2 or later.

*/

#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/version.h>
#include <net/sock.h>

#include <net/netfilter/nf_conntrack.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/netfilter_ipv4/ipt_macsave.h>

//#define DEBUG	1

#ifdef DEBUG
#define DLOG			printk
#else
#define DLOG(...)	do { } while (0);
#endif


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
	const struct ipt_macsave_match_info *info;
	struct nf_conn *ct;
	enum ip_conntrack_info ctinfo;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,28)
	info = matchinfo;
#else
	info = par->matchinfo;
#endif
	ct = nf_ct_get((struct sk_buff *)skb, &ctinfo);	// note about cast: nf_ct_get() will not modify skb
	if (ct) return (memcmp(ct->macsave, info->mac, sizeof(ct->macsave)) == 0) ^ info->invert;
	return info->invert;
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

static struct xt_match macsave_match = {
	.name		= "macsave",
	.family		= AF_INET,
	.match		= &match,
	.matchsize	= sizeof(struct ipt_macsave_match_info),
	.checkentry	= &checkentry,
	.destroy	= NULL,
	.me		= THIS_MODULE
};

static int __init init(void)
{
	DLOG(KERN_INFO "macsave match init " __DATE__ " " __TIME__ "\n");
	return xt_register_match(&macsave_match);
}

static void __exit fini(void)
{
	xt_unregister_match(&macsave_match);
}

module_init(init);
module_exit(fini);
MODULE_LICENSE("GPL");
