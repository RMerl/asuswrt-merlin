

#include <linux/module.h>

static int __init wlp_subsys_init(void)
{
	return 0;
}
module_init(wlp_subsys_init);

static void __exit wlp_subsys_exit(void)
{
	return;
}
module_exit(wlp_subsys_exit);

MODULE_AUTHOR("Reinette Chatre <reinette.chatre@intel.com>");
MODULE_DESCRIPTION("WiMedia Logical Link Control Protocol (WLP)");
MODULE_LICENSE("GPL");
