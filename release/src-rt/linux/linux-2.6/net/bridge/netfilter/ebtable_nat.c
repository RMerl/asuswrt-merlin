/*
 *  ebtable_nat
 *
 *	Authors:
 *	Bart De Schuymer <bdschuym@pandora.be>
 *
 *  April, 2002
 *
 */

#include <linux/netfilter_bridge/ebtables.h>
#include <linux/module.h>

#define NAT_VALID_HOOKS ((1 << NF_BR_PRE_ROUTING) | (1 << NF_BR_LOCAL_OUT) | \
   (1 << NF_BR_POST_ROUTING))

static struct ebt_entries initial_chains[] =
{
	{
		.name	= "PREROUTING",
		.policy	= EBT_ACCEPT,
	},
	{
		.name	= "OUTPUT",
		.policy	= EBT_ACCEPT,
	},
	{
		.name	= "POSTROUTING",
		.policy	= EBT_ACCEPT,
	}
};

static struct ebt_replace_kernel initial_table =
{
	.name		= "nat",
	.valid_hooks	= NAT_VALID_HOOKS,
	.entries_size	= 3 * sizeof(struct ebt_entries),
	.hook_entry	= {
		[NF_BR_PRE_ROUTING]	= &initial_chains[0],
		[NF_BR_LOCAL_OUT]	= &initial_chains[1],
		[NF_BR_POST_ROUTING]	= &initial_chains[2],
	},
	.entries	= (char *)initial_chains,
};

static int check(const struct ebt_table_info *info, unsigned int valid_hooks)
{
	if (valid_hooks & ~NAT_VALID_HOOKS)
		return -EINVAL;
	return 0;
}

static struct ebt_table frame_nat =
{
	.name		= "nat",
	.table		= &initial_table,
	.valid_hooks	= NAT_VALID_HOOKS,
	.check		= check,
	.me		= THIS_MODULE,
};

static unsigned int
ebt_nat_in(unsigned int hook, struct sk_buff *skb, const struct net_device *in
   , const struct net_device *out, int (*okfn)(struct sk_buff *))
{
	return ebt_do_table(hook, skb, in, out, dev_net(in)->xt.frame_nat);
}

static unsigned int
ebt_nat_out(unsigned int hook, struct sk_buff *skb, const struct net_device *in
   , const struct net_device *out, int (*okfn)(struct sk_buff *))
{
	return ebt_do_table(hook, skb, in, out, dev_net(out)->xt.frame_nat);
}

static struct nf_hook_ops ebt_ops_nat[] __read_mostly = {
	{
		.hook		= ebt_nat_out,
		.owner		= THIS_MODULE,
		.pf		= NFPROTO_BRIDGE,
		.hooknum	= NF_BR_LOCAL_OUT,
		.priority	= NF_BR_PRI_NAT_DST_OTHER,
	},
	{
		.hook		= ebt_nat_out,
		.owner		= THIS_MODULE,
		.pf		= NFPROTO_BRIDGE,
		.hooknum	= NF_BR_POST_ROUTING,
		.priority	= NF_BR_PRI_NAT_SRC,
	},
	{
		.hook		= ebt_nat_in,
		.owner		= THIS_MODULE,
		.pf		= NFPROTO_BRIDGE,
		.hooknum	= NF_BR_PRE_ROUTING,
		.priority	= NF_BR_PRI_NAT_DST_BRIDGED,
	},
};

static int __net_init frame_nat_net_init(struct net *net)
{
	net->xt.frame_nat = ebt_register_table(net, &frame_nat);
	if (IS_ERR(net->xt.frame_nat))
		return PTR_ERR(net->xt.frame_nat);
	return 0;
}

static void __net_exit frame_nat_net_exit(struct net *net)
{
	ebt_unregister_table(net, net->xt.frame_nat);
}

static struct pernet_operations frame_nat_net_ops = {
	.init = frame_nat_net_init,
	.exit = frame_nat_net_exit,
};

static int __init ebtable_nat_init(void)
{
	int ret;

	ret = register_pernet_subsys(&frame_nat_net_ops);
	if (ret < 0)
		return ret;
	ret = nf_register_hooks(ebt_ops_nat, ARRAY_SIZE(ebt_ops_nat));
	if (ret < 0)
		unregister_pernet_subsys(&frame_nat_net_ops);
	return ret;
}

static void __exit ebtable_nat_fini(void)
{
	nf_unregister_hooks(ebt_ops_nat, ARRAY_SIZE(ebt_ops_nat));
	unregister_pernet_subsys(&frame_nat_net_ops);
}

module_init(ebtable_nat_init);
module_exit(ebtable_nat_fini);
MODULE_LICENSE("GPL");
