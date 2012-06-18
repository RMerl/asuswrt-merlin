/*
 * TTL modification target for IP tables
 * (C) 2000,2005 by Harald Welte <laforge@netfilter.org>
 *
 * Hop Limit modification target for ip6tables
 * Maciej Soltysiak <solt@dns.toxicfilms.tv>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <net/checksum.h>

#include <linux/netfilter/x_tables.h>
#include <linux/netfilter_ipv4/ipt_TTL.h>
#include <linux/netfilter_ipv6/ip6t_HL.h>

MODULE_AUTHOR("Harald Welte <laforge@netfilter.org>");
MODULE_AUTHOR("Maciej Soltysiak <solt@dns.toxicfilms.tv>");
MODULE_DESCRIPTION("Xtables: Hoplimit/TTL Limit field modification target");
MODULE_LICENSE("GPL");

static unsigned int
ttl_tg(struct sk_buff **pskb,
       const struct net_device *in, const struct net_device *out,
       unsigned int hooknum, const struct xt_target *target,
       const void *targinfo)
{
	struct iphdr *iph;
	const struct ipt_TTL_info *info = targinfo;
	int new_ttl;

	if (!skb_make_writable(pskb, (*pskb)->len))
		return NF_DROP;

	iph = ip_hdr(*pskb);

	switch (info->mode) {
		case IPT_TTL_SET:
			new_ttl = info->ttl;
			break;
		case IPT_TTL_INC:
			new_ttl = iph->ttl + info->ttl;
			if (new_ttl > 255)
				new_ttl = 255;
			break;
		case IPT_TTL_DEC:
			new_ttl = iph->ttl - info->ttl;
			if (new_ttl < 0)
				new_ttl = 0;
			break;
		default:
			new_ttl = iph->ttl;
			break;
	}

	if (new_ttl != iph->ttl) {
		nf_csum_replace2(&iph->check, htons(iph->ttl << 8),
					      htons(new_ttl << 8));
		iph->ttl = new_ttl;
	}

	return XT_CONTINUE;
}

static unsigned int
hl_tg6(struct sk_buff **pskb,
       const struct net_device *in, const struct net_device *out,
       unsigned int hooknum, const struct xt_target *target,
       const void *targinfo)
{
	struct ipv6hdr *ip6h;
	const struct ip6t_HL_info *info = targinfo;
	int new_hl;

	if (!skb_make_writable(pskb, (*pskb)->len))
		return NF_DROP;

	ip6h = ipv6_hdr(*pskb);

	switch (info->mode) {
		case IP6T_HL_SET:
			new_hl = info->hop_limit;
			break;
		case IP6T_HL_INC:
			new_hl = ip6h->hop_limit + info->hop_limit;
			if (new_hl > 255)
				new_hl = 255;
			break;
		case IP6T_HL_DEC:
			new_hl = ip6h->hop_limit - info->hop_limit;
			if (new_hl < 0)
				new_hl = 0;
			break;
		default:
			new_hl = ip6h->hop_limit;
			break;
	}

	ip6h->hop_limit = new_hl;

	return XT_CONTINUE;
}

static int ttl_tg_check(const char *tablename,
			const void *e,
			const struct xt_target *target,
			void *targinfo,
			unsigned int hook_mask)
{
	const struct ipt_TTL_info *info = targinfo;

	if (info->mode > IPT_TTL_MAXMODE) {
		printk(KERN_WARNING "ipt_TTL: invalid or unknown Mode %u\n",
			info->mode);
		return 0;
	}
	if (info->mode != IPT_TTL_SET && info->ttl == 0)
		return 0;
	return 1;
}

static int hl_tg6_check(const char *tablename,
			const void *e,
			const struct xt_target *target,
			void *targinfo,
			unsigned int hook_mask)
{
	const struct ip6t_HL_info *info = targinfo;

	if (info->mode > IP6T_HL_MAXMODE) {
		printk(KERN_WARNING "ip6t_HL: invalid or unknown Mode %u\n",
			info->mode);
		return 0;
	}
	if (info->mode != IP6T_HL_SET && info->hop_limit == 0) {
		printk(KERN_WARNING "ip6t_HL: increment/decrement doesn't "
			"make sense with value 0\n");
		return 0;
	}
	return 1;
}

static struct xt_target hl_tg_reg[] __read_mostly = {
	{
		.name       = "TTL",
		.revision   = 0,
		.family     = AF_INET,
		.target     = ttl_tg,
		.targetsize = sizeof(struct ipt_TTL_info),
		.table      = "mangle",
		.checkentry = ttl_tg_check,
		.me         = THIS_MODULE,
	},
	{
		.name       = "HL",
		.revision   = 0,
		.family     = AF_INET6,
		.target     = hl_tg6,
		.targetsize = sizeof(struct ip6t_HL_info),
		.table      = "mangle",
		.checkentry = hl_tg6_check,
		.me         = THIS_MODULE,
	},
};

static int __init hl_tg_init(void)
{
	return xt_register_targets(hl_tg_reg, ARRAY_SIZE(hl_tg_reg));
}

static void __exit hl_tg_exit(void)
{
	xt_unregister_targets(hl_tg_reg, ARRAY_SIZE(hl_tg_reg));
}

module_init(hl_tg_init);
module_exit(hl_tg_exit);
MODULE_ALIAS("ipt_TTL");
MODULE_ALIAS("ip6t_HL");
