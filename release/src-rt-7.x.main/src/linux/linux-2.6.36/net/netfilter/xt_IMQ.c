/*
 * This target marks packets to be enqueued to an imq device
 */
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/netfilter/x_tables.h>
#include <linux/netfilter/xt_IMQ.h>
#include <linux/imq.h>

static unsigned int imq_target(struct sk_buff *pskb,
				const struct xt_action_param *par)
{
	const struct xt_imq_info *mr = par->targinfo;

	pskb->imq_flags = (mr->todev & IMQ_F_IFMASK) | IMQ_F_ENQUEUE;

	return XT_CONTINUE;
}

static int imq_checkentry(const struct xt_tgchk_param *par)
{
	struct xt_imq_info *mr = par->targinfo;

	if (mr->todev > IMQ_MAX_DEVS - 1) {
		printk(KERN_WARNING
		       "IMQ: invalid device specified, highest is %u\n",
		       IMQ_MAX_DEVS - 1);
		return -EINVAL;
	}

	return 0;
}

static struct xt_target xt_imq_reg[] __read_mostly = {
	{
		.name           = "IMQ",
		.family		= AF_INET,
		.checkentry     = imq_checkentry,
		.target         = imq_target,
		.targetsize	= sizeof(struct xt_imq_info),
		.table		= "mangle",
		.me             = THIS_MODULE
	},
	{
		.name           = "IMQ",
		.family		= AF_INET6,
		.checkentry     = imq_checkentry,
		.target         = imq_target,
		.targetsize	= sizeof(struct xt_imq_info),
		.table		= "mangle",
		.me             = THIS_MODULE
	},
};

static int __init imq_init(void)
{
	return xt_register_targets(xt_imq_reg, ARRAY_SIZE(xt_imq_reg));
}

static void __exit imq_fini(void)
{
	xt_unregister_targets(xt_imq_reg, ARRAY_SIZE(xt_imq_reg));
}

module_init(imq_init);
module_exit(imq_fini);

MODULE_AUTHOR("http://www.linuximq.net");
MODULE_DESCRIPTION("Pseudo-driver for the intermediate queue device. "
		   "See http://www.linuximq.net/ for more information.");
MODULE_LICENSE("GPL");
MODULE_ALIAS("ipt_IMQ");
MODULE_ALIAS("ip6t_IMQ");

