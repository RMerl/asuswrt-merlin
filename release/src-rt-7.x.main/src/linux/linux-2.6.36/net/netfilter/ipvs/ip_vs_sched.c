/*
 * IPVS         An implementation of the IP virtual server support for the
 *              LINUX operating system.  IPVS is now implemented as a module
 *              over the Netfilter framework. IPVS can be used to build a
 *              high-performance and highly available server based on a
 *              cluster of servers.
 *
 * Authors:     Wensong Zhang <wensong@linuxvirtualserver.org>
 *              Peter Kese <peter.kese@ijs.si>
 *
 *              This program is free software; you can redistribute it and/or
 *              modify it under the terms of the GNU General Public License
 *              as published by the Free Software Foundation; either version
 *              2 of the License, or (at your option) any later version.
 *
 * Changes:
 *
 */

#define KMSG_COMPONENT "IPVS"
#define pr_fmt(fmt) KMSG_COMPONENT ": " fmt

#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <asm/string.h>
#include <linux/kmod.h>
#include <linux/sysctl.h>

#include <net/ip_vs.h>

/*
 *  IPVS scheduler list
 */
static LIST_HEAD(ip_vs_schedulers);

/* lock for service table */
static DEFINE_RWLOCK(__ip_vs_sched_lock);


/*
 *  Bind a service with a scheduler
 */
int ip_vs_bind_scheduler(struct ip_vs_service *svc,
			 struct ip_vs_scheduler *scheduler)
{
	int ret;

	if (svc == NULL) {
		pr_err("%s(): svc arg NULL\n", __func__);
		return -EINVAL;
	}
	if (scheduler == NULL) {
		pr_err("%s(): scheduler arg NULL\n", __func__);
		return -EINVAL;
	}

	svc->scheduler = scheduler;

	if (scheduler->init_service) {
		ret = scheduler->init_service(svc);
		if (ret) {
			pr_err("%s(): init error\n", __func__);
			return ret;
		}
	}

	return 0;
}


/*
 *  Unbind a service with its scheduler
 */
int ip_vs_unbind_scheduler(struct ip_vs_service *svc)
{
	struct ip_vs_scheduler *sched;

	if (svc == NULL) {
		pr_err("%s(): svc arg NULL\n", __func__);
		return -EINVAL;
	}

	sched = svc->scheduler;
	if (sched == NULL) {
		pr_err("%s(): svc isn't bound\n", __func__);
		return -EINVAL;
	}

	if (sched->done_service) {
		if (sched->done_service(svc) != 0) {
			pr_err("%s(): done error\n", __func__);
			return -EINVAL;
		}
	}

	svc->scheduler = NULL;
	return 0;
}


/*
 *  Get scheduler in the scheduler list by name
 */
static struct ip_vs_scheduler *ip_vs_sched_getbyname(const char *sched_name)
{
	struct ip_vs_scheduler *sched;

	IP_VS_DBG(2, "%s(): sched_name \"%s\"\n", __func__, sched_name);

	read_lock_bh(&__ip_vs_sched_lock);

	list_for_each_entry(sched, &ip_vs_schedulers, n_list) {
		/*
		 * Test and get the modules atomically
		 */
		if (sched->module && !try_module_get(sched->module)) {
			/*
			 * This scheduler is just deleted
			 */
			continue;
		}
		if (strcmp(sched_name, sched->name)==0) {
			/* HIT */
			read_unlock_bh(&__ip_vs_sched_lock);
			return sched;
		}
		if (sched->module)
			module_put(sched->module);
	}

	read_unlock_bh(&__ip_vs_sched_lock);
	return NULL;
}


/*
 *  Lookup scheduler and try to load it if it doesn't exist
 */
struct ip_vs_scheduler *ip_vs_scheduler_get(const char *sched_name)
{
	struct ip_vs_scheduler *sched;

	/*
	 *  Search for the scheduler by sched_name
	 */
	sched = ip_vs_sched_getbyname(sched_name);

	/*
	 *  If scheduler not found, load the module and search again
	 */
	if (sched == NULL) {
		request_module("ip_vs_%s", sched_name);
		sched = ip_vs_sched_getbyname(sched_name);
	}

	return sched;
}

void ip_vs_scheduler_put(struct ip_vs_scheduler *scheduler)
{
	if (scheduler->module)
		module_put(scheduler->module);
}


/*
 *  Register a scheduler in the scheduler list
 */
int register_ip_vs_scheduler(struct ip_vs_scheduler *scheduler)
{
	struct ip_vs_scheduler *sched;

	if (!scheduler) {
		pr_err("%s(): NULL arg\n", __func__);
		return -EINVAL;
	}

	if (!scheduler->name) {
		pr_err("%s(): NULL scheduler_name\n", __func__);
		return -EINVAL;
	}

	/* increase the module use count */
	ip_vs_use_count_inc();

	write_lock_bh(&__ip_vs_sched_lock);

	if (!list_empty(&scheduler->n_list)) {
		write_unlock_bh(&__ip_vs_sched_lock);
		ip_vs_use_count_dec();
		pr_err("%s(): [%s] scheduler already linked\n",
		       __func__, scheduler->name);
		return -EINVAL;
	}

	/*
	 *  Make sure that the scheduler with this name doesn't exist
	 *  in the scheduler list.
	 */
	list_for_each_entry(sched, &ip_vs_schedulers, n_list) {
		if (strcmp(scheduler->name, sched->name) == 0) {
			write_unlock_bh(&__ip_vs_sched_lock);
			ip_vs_use_count_dec();
			pr_err("%s(): [%s] scheduler already existed "
			       "in the system\n", __func__, scheduler->name);
			return -EINVAL;
		}
	}
	/*
	 *	Add it into the d-linked scheduler list
	 */
	list_add(&scheduler->n_list, &ip_vs_schedulers);
	write_unlock_bh(&__ip_vs_sched_lock);

	pr_info("[%s] scheduler registered.\n", scheduler->name);

	return 0;
}


/*
 *  Unregister a scheduler from the scheduler list
 */
int unregister_ip_vs_scheduler(struct ip_vs_scheduler *scheduler)
{
	if (!scheduler) {
		pr_err("%s(): NULL arg\n", __func__);
		return -EINVAL;
	}

	write_lock_bh(&__ip_vs_sched_lock);
	if (list_empty(&scheduler->n_list)) {
		write_unlock_bh(&__ip_vs_sched_lock);
		pr_err("%s(): [%s] scheduler is not in the list. failed\n",
		       __func__, scheduler->name);
		return -EINVAL;
	}

	/*
	 *	Remove it from the d-linked scheduler list
	 */
	list_del(&scheduler->n_list);
	write_unlock_bh(&__ip_vs_sched_lock);

	/* decrease the module use count */
	ip_vs_use_count_dec();

	pr_info("[%s] scheduler unregistered.\n", scheduler->name);

	return 0;
}
