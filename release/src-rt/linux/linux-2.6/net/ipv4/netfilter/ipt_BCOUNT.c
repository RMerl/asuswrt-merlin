/*

	BCOUNT target
	Copyright (C) 2006 Jonathan Zarate

	Licensed under GNU GPL v2 or later.

*/
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/version.h>
#include <linux/if_ether.h>

#include <net/netfilter/nf_conntrack.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/netfilter_ipv4/ipt_BCOUNT.h>

//	#define DEBUG_BCOUNT

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
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
	struct sk_buff *skb = *pskb;
#endif
	struct nf_conn *ct;
	enum ip_conntrack_info ctinfo;

	ct = nf_ct_get(skb, &ctinfo);
	if (ct) {
		ct->bcount += (skb)->len;
		if (ct->bcount >= 0x0FFFFFFF) ct->bcount = 0x0FFFFFFF;
#ifdef DEBUG_BCOUNT
		if (net_ratelimit())
			printf(KERN_DEBUG "BCOUNT %lx %lx\n", (skb)->len, ct->bcount);
#endif
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
	return 1;
}

static struct ipt_target BCOUNT_target = { 
	.name = "BCOUNT",
	.family = AF_INET,
	.target = target,
	.targetsize = sizeof(struct ipt_BCOUNT_target),
	.checkentry = checkentry,
	.me = THIS_MODULE,
};

static int __init init(void)
{
	return xt_register_target(&BCOUNT_target);
}

static void __exit fini(void)
{
	xt_unregister_target(&BCOUNT_target);
}

module_init(init);
module_exit(fini);


MODULE_AUTHOR("Jonathan Zarate");
MODULE_DESCRIPTION("BCOUNT target");
MODULE_LICENSE("GPL");
