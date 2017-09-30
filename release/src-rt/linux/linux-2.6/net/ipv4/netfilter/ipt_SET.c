/* Copyright (C) 2000-2002 Joakim Axelsson <gozem@linux.nu>
 *                         Patrick Schaaf <bof@bof.de>
 *                         Martin Josefsson <gandalf@wlug.westbo.se>
 * Copyright (C) 2003-2004 Jozsef Kadlecsik <kadlec@blackhole.kfki.hu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/* Kernel module to match an IP set. */

#include <linux/module.h>
#include <linux/ip.h>
#include <linux/skbuff.h>
#include <linux/version.h>

#include <linux/netfilter/x_tables.h>
#include <linux/netfilter_ipv4/ip_set.h>
#include <linux/netfilter_ipv4/ipt_set.h>

static inline int
match_set(const struct ipt_set_info *info,
	  const struct sk_buff *skb,
	  int inv)
{	
	if (ip_set_testip_kernel(info->index, skb, info->flags))
		inv = !inv;
	return inv;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,28)
static bool
match(const struct sk_buff *skb,
      const struct net_device *in,
      const struct net_device *out,
      const struct xt_match *match,
      const void *matchinfo,
      int offset, 
      unsigned int protoff, 
      bool *hotdrop)
#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,28) */
static bool
match(const struct sk_buff *skb,
      const struct xt_match_param *par)
#endif
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,28)
	const struct ipt_set_info_match *info = matchinfo;
#else
	const struct ipt_set_info_match *info = par->matchinfo;
#endif
		
	return match_set(&info->match_set,
			 skb,
			 info->match_set.flags[0] & IPSET_MATCH_INV);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,28)
static bool
checkentry(const char *tablename,
	   const void *inf,
	   const struct xt_match *match,
	   void *matchinfo,
	   unsigned int hook_mask)
#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,28) */
static bool
checkentry(const struct xt_mtchk_param *par)
#endif
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,28)
	struct ipt_set_info_match *info = matchinfo;
#else
	struct ipt_set_info_match *info = par->matchinfo;
#endif
	ip_set_id_t index;

	index = ip_set_get_byindex(info->match_set.index);
		
	if (index == IP_SET_INVALID_ID) {
		ip_set_printk("Cannot find set indentified by id %u to match",
			      info->match_set.index);
		return 0;	/* error */
	}
	if (info->match_set.flags[IP_SET_MAX_BINDINGS] != 0) {
		ip_set_printk("That's nasty!");
		return 0;	/* error */
	}

	return 1;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,28)
static void destroy(const struct xt_match *match,
		    void *matchinfo)
#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,28) */
static void destroy(const struct xt_mtdtor_param *par)
#endif
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,28)
	struct ipt_set_info_match *info = matchinfo;
#else
	struct ipt_set_info_match *info = par->matchinfo;
#endif

	ip_set_put_byindex(info->match_set.index);
}

static struct xt_match set_match = {
	.name		= "set",
	.family		= AF_INET,
	.match		= &match,
	.matchsize	= sizeof(struct ipt_set_info_match),
	.checkentry	= &checkentry,
	.destroy	= &destroy,
	.me		= THIS_MODULE
};

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jozsef Kadlecsik <kadlec@blackhole.kfki.hu>");
MODULE_DESCRIPTION("iptables IP set match module");

static int __init ipt_ipset_init(void)
{
	return xt_register_match(&set_match);
}

static void __exit ipt_ipset_fini(void)
{
	xt_unregister_match(&set_match);
}

module_init(ipt_ipset_init);
module_exit(ipt_ipset_fini);
