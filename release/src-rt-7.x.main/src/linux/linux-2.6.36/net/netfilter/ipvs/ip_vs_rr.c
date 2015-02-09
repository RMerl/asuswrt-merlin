/*
 * IPVS:        Round-Robin Scheduling module
 *
 * Authors:     Wensong Zhang <wensong@linuxvirtualserver.org>
 *              Peter Kese <peter.kese@ijs.si>
 *
 *              This program is free software; you can redistribute it and/or
 *              modify it under the terms of the GNU General Public License
 *              as published by the Free Software Foundation; either version
 *              2 of the License, or (at your option) any later version.
 *
 * Fixes/Changes:
 *     Wensong Zhang            :     changed the ip_vs_rr_schedule to return dest
 *     Julian Anastasov         :     fixed the NULL pointer access bug in debugging
 *     Wensong Zhang            :     changed some comestics things for debugging
 *     Wensong Zhang            :     changed for the d-linked destination list
 *     Wensong Zhang            :     added the ip_vs_rr_update_svc
 *     Wensong Zhang            :     added any dest with weight=0 is quiesced
 *
 */

#define KMSG_COMPONENT "IPVS"
#define pr_fmt(fmt) KMSG_COMPONENT ": " fmt

#include <linux/module.h>
#include <linux/kernel.h>

#include <net/ip_vs.h>


static int ip_vs_rr_init_svc(struct ip_vs_service *svc)
{
	svc->sched_data = &svc->destinations;
	return 0;
}


static int ip_vs_rr_update_svc(struct ip_vs_service *svc)
{
	svc->sched_data = &svc->destinations;
	return 0;
}


/*
 * Round-Robin Scheduling
 */
static struct ip_vs_dest *
ip_vs_rr_schedule(struct ip_vs_service *svc, const struct sk_buff *skb)
{
	struct list_head *p, *q;
	struct ip_vs_dest *dest;

	IP_VS_DBG(6, "%s(): Scheduling...\n", __func__);

	write_lock(&svc->sched_lock);
	p = (struct list_head *)svc->sched_data;
	p = p->next;
	q = p;
	do {
		/* skip list head */
		if (q == &svc->destinations) {
			q = q->next;
			continue;
		}

		dest = list_entry(q, struct ip_vs_dest, n_list);
		if (!(dest->flags & IP_VS_DEST_F_OVERLOAD) &&
		    atomic_read(&dest->weight) > 0)
			/* HIT */
			goto out;
		q = q->next;
	} while (q != p);
	write_unlock(&svc->sched_lock);
	IP_VS_ERR_RL("RR: no destination available\n");
	return NULL;

  out:
	svc->sched_data = q;
	write_unlock(&svc->sched_lock);
	IP_VS_DBG_BUF(6, "RR: server %s:%u "
		      "activeconns %d refcnt %d weight %d\n",
		      IP_VS_DBG_ADDR(svc->af, &dest->addr), ntohs(dest->port),
		      atomic_read(&dest->activeconns),
		      atomic_read(&dest->refcnt), atomic_read(&dest->weight));

	return dest;
}


static struct ip_vs_scheduler ip_vs_rr_scheduler = {
	.name =			"rr",			/* name */
	.refcnt =		ATOMIC_INIT(0),
	.module =		THIS_MODULE,
	.n_list =		LIST_HEAD_INIT(ip_vs_rr_scheduler.n_list),
	.init_service =		ip_vs_rr_init_svc,
	.update_service =	ip_vs_rr_update_svc,
	.schedule =		ip_vs_rr_schedule,
};

static int __init ip_vs_rr_init(void)
{
	return register_ip_vs_scheduler(&ip_vs_rr_scheduler);
}

static void __exit ip_vs_rr_cleanup(void)
{
	unregister_ip_vs_scheduler(&ip_vs_rr_scheduler);
}

module_init(ip_vs_rr_init);
module_exit(ip_vs_rr_cleanup);
MODULE_LICENSE("GPL");
