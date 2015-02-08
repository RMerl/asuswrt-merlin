/*
 * IP tables module for matching the value of the incoming ether port
 * for Ralink SoC platform.
 *
 * (C) 2009 by Y.Y. Huang <yy_huang@ralinktech.com.tw>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <linux/ipv6.h>

#include <linux/netfilter/xt_ethport.h>
#include <linux/netfilter/x_tables.h>

MODULE_AUTHOR("Y.Y. Huang <yy_huang@ralinktech.com.tw>");
MODULE_DESCRIPTION("x_tables Ra SoC Ethernet incoming port matching module");
MODULE_LICENSE("GPL");
MODULE_ALIAS("ipt_ethport");
MODULE_ALIAS("ip6t_ethport");

static bool match(const struct sk_buff *skb, struct xt_action_param *par)
{
	const struct xt_ethport_info *info = par->matchinfo;
	u_int8_t portnum = skb->priority;

	return (portnum == info->portnum) ^ !!info->invert;
}

static bool match6(const struct sk_buff *skb, struct xt_action_param *par)
{
	const struct xt_ethport_info *info = par->matchinfo;
	u_int8_t portnum = skb->priority;

	return (portnum == info->portnum) ^ !!info->invert;
}

static int checkentry(const struct xt_mtchk_param *par)
{
	const u_int8_t portnum = ((struct xt_ethport_info *)(par->matchinfo))->portnum;

	if (portnum > XT_ETHPORT_MAX) {
		printk(KERN_ERR "xt_ethport: port number %x is out of range(%x)\n", portnum, XT_ETHPORT_MAX);
		return -EINVAL;
	}

	return 0;
}

static struct xt_match xt_ethport_match[] = {
	{
		.name		= "ethport",
		.family		= AF_INET,
		.checkentry	= checkentry,
		.match		= match,
		.matchsize	= sizeof(struct xt_ethport_info),
		.me		= THIS_MODULE,
	},
	{
		.name		= "ethport",
		.family		= AF_INET6,
		.checkentry	= checkentry,
		.match		= match6,
		.matchsize	= sizeof(struct xt_ethport_info),
		.me		= THIS_MODULE,
	},
};

static int __init xt_ethport_match_init(void)
{
	return xt_register_matches(xt_ethport_match, ARRAY_SIZE(xt_ethport_match));
}

static void __exit xt_ethport_match_fini(void)
{
	xt_unregister_matches(xt_ethport_match, ARRAY_SIZE(xt_ethport_match));
}

module_init(xt_ethport_match_init);
module_exit(xt_ethport_match_fini);
