/*
 * This is the 1999 rewrite of IP Firewalling, aiming for kernel 2.3.x.
 *
 * Copyright (C) 1999 Paul `Rusty' Russell & Michael J. Neuling
 * Copyright (C) 2000-2004 Netfilter Core Team <coreteam@netfilter.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/netfilter_ipv6/ip6_tables.h>
#include <linux/slab.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Netfilter Core Team <coreteam@netfilter.org>");
MODULE_DESCRIPTION("ip6tables filter table");

#define FILTER_VALID_HOOKS ((1 << NF_INET_LOCAL_IN) | \
			    (1 << NF_INET_FORWARD) | \
			    (1 << NF_INET_LOCAL_OUT))

static const struct xt_table packet_filter = {
	.name		= "filter",
	.valid_hooks	= FILTER_VALID_HOOKS,
	.me		= THIS_MODULE,
	.af		= NFPROTO_IPV6,
	.priority	= NF_IP6_PRI_FILTER,
};

/* The work comes in here from netfilter.c. */
static unsigned int
ip6table_filter_hook(unsigned int hook, struct sk_buff *skb,
		     const struct net_device *in, const struct net_device *out,
		     int (*okfn)(struct sk_buff *))
{
	const struct net *net = dev_net((in != NULL) ? in : out);

	return ip6t_do_table(skb, hook, in, out, net->ipv6.ip6table_filter);
}

static struct nf_hook_ops *filter_ops __read_mostly;

/* Default to forward because I got too much mail already. */
static int forward = NF_ACCEPT;
module_param(forward, bool, 0000);

static int __net_init ip6table_filter_net_init(struct net *net)
{
	struct ip6t_replace *repl;

	repl = ip6t_alloc_initial_table(&packet_filter);
	if (repl == NULL)
		return -ENOMEM;
	/* Entry 1 is the FORWARD hook */
	((struct ip6t_standard *)repl->entries)[1].target.verdict =
		-forward - 1;

	net->ipv6.ip6table_filter =
		ip6t_register_table(net, &packet_filter, repl);
	kfree(repl);
	if (IS_ERR(net->ipv6.ip6table_filter))
		return PTR_ERR(net->ipv6.ip6table_filter);
	return 0;
}

static void __net_exit ip6table_filter_net_exit(struct net *net)
{
	ip6t_unregister_table(net, net->ipv6.ip6table_filter);
}

static struct pernet_operations ip6table_filter_net_ops = {
	.init = ip6table_filter_net_init,
	.exit = ip6table_filter_net_exit,
};

static int __init ip6table_filter_init(void)
{
	int ret;

	if (forward < 0 || forward > NF_MAX_VERDICT) {
		pr_err("iptables forward must be 0 or 1\n");
		return -EINVAL;
	}

	ret = register_pernet_subsys(&ip6table_filter_net_ops);
	if (ret < 0)
		return ret;

	/* Register hooks */
	filter_ops = xt_hook_link(&packet_filter, ip6table_filter_hook);
	if (IS_ERR(filter_ops)) {
		ret = PTR_ERR(filter_ops);
		goto cleanup_table;
	}

	return ret;

 cleanup_table:
	unregister_pernet_subsys(&ip6table_filter_net_ops);
	return ret;
}

static void __exit ip6table_filter_fini(void)
{
	xt_hook_unlink(&packet_filter, filter_ops);
	unregister_pernet_subsys(&ip6table_filter_net_ops);
}

module_init(ip6table_filter_init);
module_exit(ip6table_filter_fini);
