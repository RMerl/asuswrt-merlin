/* Copyright (C) 2000-2002 Joakim Axelsson <gozem@linux.nu>
 *                         Patrick Schaaf <bof@bof.de>
 *                         Martin Josefsson <gandalf@wlug.westbo.se>
 * Copyright (C) 2003-2004 Jozsef Kadlecsik <kadlec@blackhole.kfki.hu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/* ipt_SET.c - netfilter target to manipulate IP sets */

#include <linux/module.h>
#include <linux/ip.h>
#include <linux/skbuff.h>
#include <linux/version.h>

#include <linux/netfilter_ipv4.h>
#include <linux/netfilter/x_tables.h>
#include <linux/netfilter_ipv4/ipt_set.h>

static unsigned int
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,28)
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
	const struct ipt_set_info_target *info = targinfo;
#else
	const struct ipt_set_info_target *info = par->targinfo;
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22)
	struct sk_buff *skb = *pskb;
#endif

	
	if (info->add_set.index != IP_SET_INVALID_ID)
		ip_set_addip_kernel(info->add_set.index,
				    skb,
				    info->add_set.flags);
	if (info->del_set.index != IP_SET_INVALID_ID)
		ip_set_delip_kernel(info->del_set.index,
				    skb,
				    info->del_set.flags);

	return XT_CONTINUE;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,28)
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
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,28)
	const struct ipt_set_info_target *info = targinfo;
#else
	const struct ipt_set_info_target *info = par->targinfo;
#endif
	ip_set_id_t index;

	if (info->add_set.index != IP_SET_INVALID_ID) {
		index = ip_set_get_byindex(info->add_set.index);
		if (index == IP_SET_INVALID_ID) {
			ip_set_printk("cannot find add_set index %u as target",
				      info->add_set.index);
			return 0;	/* error */
		}
	}

	if (info->del_set.index != IP_SET_INVALID_ID) {
		index = ip_set_get_byindex(info->del_set.index);
		if (index == IP_SET_INVALID_ID) {
			ip_set_printk("cannot find del_set index %u as target",
				      info->del_set.index);
			return 0;	/* error */
		}
	}
	if (info->add_set.flags[IP_SET_MAX_BINDINGS] != 0
	    || info->del_set.flags[IP_SET_MAX_BINDINGS] != 0) {
		ip_set_printk("That's nasty!");
		return 0;	/* error */
	}

	return 1;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,28)
static void destroy(const struct xt_target *target,
		    void *targetinfo)
#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,28) */
static void destroy(const struct xt_tgdtor_param *par)
#endif
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,28)
	const struct ipt_set_info_target *info = targetinfo;
#else
	const struct ipt_set_info_target *info = par->targinfo;
#endif

	if (info->add_set.index != IP_SET_INVALID_ID)
		ip_set_put_byindex(info->add_set.index);
	if (info->del_set.index != IP_SET_INVALID_ID)
		ip_set_put_byindex(info->del_set.index);
}

static struct xt_target SET_target = {
	.name 		= "SET",
	.family		= AF_INET,
	.target 	= target,
	.targetsize	= sizeof(struct ipt_set_info_target),
	.checkentry 	= checkentry,
	.destroy 	= destroy,
	.me 		= THIS_MODULE
};

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jozsef Kadlecsik <kadlec@blackhole.kfki.hu>");
MODULE_DESCRIPTION("iptables IP set target module");

static int __init ipt_SET_init(void)
{
	return xt_register_target(&SET_target);
}

static void __exit ipt_SET_fini(void)
{
	xt_unregister_target(&SET_target);
}

module_init(ipt_SET_init);
module_exit(ipt_SET_fini);
