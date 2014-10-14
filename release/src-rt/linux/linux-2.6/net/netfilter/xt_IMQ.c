/*
 * This target marks packets to be enqueued to an imq device
 */
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/netfilter/x_tables.h>
#include <linux/netfilter/xt_IMQ.h>
#include <linux/imq.h>

static unsigned int imq_target(struct sk_buff **pskb,
			       const struct net_device *in,
			       const struct net_device *out,
			       unsigned int hooknum,
			       const struct xt_target *target,
			       const void *targinfo)
{
	struct xt_imq_info *mr = (struct xt_imq_info*)targinfo;

	(*pskb)->imq_flags = mr->todev | IMQ_F_ENQUEUE;

	return XT_CONTINUE;
}

static int imq_checkentry(const char *tablename,
			  const void *e,
			  const struct xt_target *target,
			  void *targinfo,
			  unsigned int hook_mask)
{
	struct xt_imq_info *mr = (struct xt_imq_info*)targinfo;

	if (mr->todev > IMQ_MAX_DEVS) {
		printk(KERN_WARNING
		       "IMQ: invalid device specified, highest is %u\n",
		       IMQ_MAX_DEVS);
		return 0;
	}

	return 1;
}

static struct xt_target xt_imq_reg[] __read_mostly = {
	{
		.name		= "IMQ",
		.family		= AF_INET,
		.target		= imq_target,
		.targetsize	= sizeof(struct xt_imq_info),
		.checkentry	= imq_checkentry,
		.me		= THIS_MODULE,
		.table		= "mangle"
	},
	{
		.name           = "IMQ",
		.family		= AF_INET6,
		.target         = imq_target,
		.targetsize	= sizeof(struct xt_imq_info),
		.table		= "mangle",
		.checkentry     = imq_checkentry,
		.me             = THIS_MODULE
	},
};

static int __init init(void)
{
	return xt_register_targets(xt_imq_reg, ARRAY_SIZE(xt_imq_reg));
}

static void __exit fini(void)
{
	xt_unregister_targets(xt_imq_reg, ARRAY_SIZE(xt_imq_reg));
}

module_init(init);
module_exit(fini);

MODULE_ALIAS("ipt_IMQ");
MODULE_ALIAS("ip6t_IMQ");
MODULE_AUTHOR("http://www.linuximq.net");
MODULE_DESCRIPTION("Pseudo-driver for the intermediate queue device. See http://www.linuximq.net/ for more information.");
MODULE_LICENSE("GPL");
