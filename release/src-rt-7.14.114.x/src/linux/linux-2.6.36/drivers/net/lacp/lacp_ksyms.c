#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
#include <linux/config.h>
#endif
#include <linux/module.h>
extern void lacp_get_hostmac; EXPORT_SYMBOL(lacp_get_hostmac);
extern void lacp_get_linksts; EXPORT_SYMBOL(lacp_get_linksts);
extern void lacp_get_portsts; EXPORT_SYMBOL(lacp_get_portsts);
extern void lacp_send; EXPORT_SYMBOL(lacp_send);
extern void lacp_set_pid_report; EXPORT_SYMBOL(lacp_set_pid_report);
extern void lacp_tsk_lock; EXPORT_SYMBOL(lacp_tsk_lock);
extern void lacp_tsk_start; EXPORT_SYMBOL(lacp_tsk_start);
extern void lacp_tsk_stop; EXPORT_SYMBOL(lacp_tsk_stop);
extern void lacp_tsk_unlock; EXPORT_SYMBOL(lacp_tsk_unlock);
extern void lacp_update_agg; EXPORT_SYMBOL(lacp_update_agg);
