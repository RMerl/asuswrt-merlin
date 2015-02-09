/*
 * IPVS:        Locality-Based Least-Connection scheduling module
 *
 * Authors:     Wensong Zhang <wensong@gnuchina.org>
 *
 *              This program is free software; you can redistribute it and/or
 *              modify it under the terms of the GNU General Public License
 *              as published by the Free Software Foundation; either version
 *              2 of the License, or (at your option) any later version.
 *
 * Changes:
 *     Martin Hamilton         :    fixed the terrible locking bugs
 *                                   *lock(tbl->lock) ==> *lock(&tbl->lock)
 *     Wensong Zhang           :    fixed the uninitialized tbl->lock bug
 *     Wensong Zhang           :    added doing full expiration check to
 *                                   collect stale entries of 24+ hours when
 *                                   no partial expire check in a half hour
 *     Julian Anastasov        :    replaced del_timer call with del_timer_sync
 *                                   to avoid the possible race between timer
 *                                   handler and del_timer thread in SMP
 *
 */

/*
 * The lblc algorithm is as follows (pseudo code):
 *
 *       if cachenode[dest_ip] is null then
 *               n, cachenode[dest_ip] <- {weighted least-conn node};
 *       else
 *               n <- cachenode[dest_ip];
 *               if (n is dead) OR
 *                  (n.conns>n.weight AND
 *                   there is a node m with m.conns<m.weight/2) then
 *                 n, cachenode[dest_ip] <- {weighted least-conn node};
 *
 *       return n;
 *
 * Thanks must go to Wenzhuo Zhang for talking WCCP to me and pushing
 * me to write this module.
 */

#define KMSG_COMPONENT "IPVS"
#define pr_fmt(fmt) KMSG_COMPONENT ": " fmt

#include <linux/ip.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/jiffies.h>

/* for sysctl */
#include <linux/fs.h>
#include <linux/sysctl.h>

#include <net/ip_vs.h>


/*
 *    It is for garbage collection of stale IPVS lblc entries,
 *    when the table is full.
 */
#define CHECK_EXPIRE_INTERVAL   (60*HZ)
#define ENTRY_TIMEOUT           (6*60*HZ)

/*
 *    It is for full expiration check.
 *    When there is no partial expiration check (garbage collection)
 *    in a half hour, do a full expiration check to collect stale
 *    entries that haven't been touched for a day.
 */
#define COUNT_FOR_FULL_EXPIRATION   30
static int sysctl_ip_vs_lblc_expiration = 24*60*60*HZ;


/*
 *     for IPVS lblc entry hash table
 */
#ifndef CONFIG_IP_VS_LBLC_TAB_BITS
#define CONFIG_IP_VS_LBLC_TAB_BITS      10
#endif
#define IP_VS_LBLC_TAB_BITS     CONFIG_IP_VS_LBLC_TAB_BITS
#define IP_VS_LBLC_TAB_SIZE     (1 << IP_VS_LBLC_TAB_BITS)
#define IP_VS_LBLC_TAB_MASK     (IP_VS_LBLC_TAB_SIZE - 1)


/*
 *      IPVS lblc entry represents an association between destination
 *      IP address and its destination server
 */
struct ip_vs_lblc_entry {
	struct list_head        list;
	int			af;		/* address family */
	union nf_inet_addr      addr;           /* destination IP address */
	struct ip_vs_dest       *dest;          /* real server (cache) */
	unsigned long           lastuse;        /* last used time */
};


/*
 *      IPVS lblc hash table
 */
struct ip_vs_lblc_table {
	struct list_head        bucket[IP_VS_LBLC_TAB_SIZE];  /* hash bucket */
	atomic_t                entries;        /* number of entries */
	int                     max_size;       /* maximum size of entries */
	struct timer_list       periodic_timer; /* collect stale entries */
	int                     rover;          /* rover for expire check */
	int                     counter;        /* counter for no expire */
};


/*
 *      IPVS LBLC sysctl table
 */

static ctl_table vs_vars_table[] = {
	{
		.procname	= "lblc_expiration",
		.data		= &sysctl_ip_vs_lblc_expiration,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_jiffies,
	},
	{ }
};

static struct ctl_table_header * sysctl_header;

static inline void ip_vs_lblc_free(struct ip_vs_lblc_entry *en)
{
	list_del(&en->list);
	/*
	 * We don't kfree dest because it is refered either by its service
	 * or the trash dest list.
	 */
	atomic_dec(&en->dest->refcnt);
	kfree(en);
}


/*
 *	Returns hash value for IPVS LBLC entry
 */
static inline unsigned
ip_vs_lblc_hashkey(int af, const union nf_inet_addr *addr)
{
	__be32 addr_fold = addr->ip;

#ifdef CONFIG_IP_VS_IPV6
	if (af == AF_INET6)
		addr_fold = addr->ip6[0]^addr->ip6[1]^
			    addr->ip6[2]^addr->ip6[3];
#endif
	return (ntohl(addr_fold)*2654435761UL) & IP_VS_LBLC_TAB_MASK;
}


/*
 *	Hash an entry in the ip_vs_lblc_table.
 *	returns bool success.
 */
static void
ip_vs_lblc_hash(struct ip_vs_lblc_table *tbl, struct ip_vs_lblc_entry *en)
{
	unsigned hash = ip_vs_lblc_hashkey(en->af, &en->addr);

	list_add(&en->list, &tbl->bucket[hash]);
	atomic_inc(&tbl->entries);
}


/*
 *  Get ip_vs_lblc_entry associated with supplied parameters. Called under read
 *  lock
 */
static inline struct ip_vs_lblc_entry *
ip_vs_lblc_get(int af, struct ip_vs_lblc_table *tbl,
	       const union nf_inet_addr *addr)
{
	unsigned hash = ip_vs_lblc_hashkey(af, addr);
	struct ip_vs_lblc_entry *en;

	list_for_each_entry(en, &tbl->bucket[hash], list)
		if (ip_vs_addr_equal(af, &en->addr, addr))
			return en;

	return NULL;
}


/*
 * Create or update an ip_vs_lblc_entry, which is a mapping of a destination IP
 * address to a server. Called under write lock.
 */
static inline struct ip_vs_lblc_entry *
ip_vs_lblc_new(struct ip_vs_lblc_table *tbl, const union nf_inet_addr *daddr,
	       struct ip_vs_dest *dest)
{
	struct ip_vs_lblc_entry *en;

	en = ip_vs_lblc_get(dest->af, tbl, daddr);
	if (!en) {
		en = kmalloc(sizeof(*en), GFP_ATOMIC);
		if (!en) {
			pr_err("%s(): no memory\n", __func__);
			return NULL;
		}

		en->af = dest->af;
		ip_vs_addr_copy(dest->af, &en->addr, daddr);
		en->lastuse = jiffies;

		atomic_inc(&dest->refcnt);
		en->dest = dest;

		ip_vs_lblc_hash(tbl, en);
	} else if (en->dest != dest) {
		atomic_dec(&en->dest->refcnt);
		atomic_inc(&dest->refcnt);
		en->dest = dest;
	}

	return en;
}


/*
 *      Flush all the entries of the specified table.
 */
static void ip_vs_lblc_flush(struct ip_vs_lblc_table *tbl)
{
	struct ip_vs_lblc_entry *en, *nxt;
	int i;

	for (i=0; i<IP_VS_LBLC_TAB_SIZE; i++) {
		list_for_each_entry_safe(en, nxt, &tbl->bucket[i], list) {
			ip_vs_lblc_free(en);
			atomic_dec(&tbl->entries);
		}
	}
}


static inline void ip_vs_lblc_full_check(struct ip_vs_service *svc)
{
	struct ip_vs_lblc_table *tbl = svc->sched_data;
	struct ip_vs_lblc_entry *en, *nxt;
	unsigned long now = jiffies;
	int i, j;

	for (i=0, j=tbl->rover; i<IP_VS_LBLC_TAB_SIZE; i++) {
		j = (j + 1) & IP_VS_LBLC_TAB_MASK;

		write_lock(&svc->sched_lock);
		list_for_each_entry_safe(en, nxt, &tbl->bucket[j], list) {
			if (time_before(now,
					en->lastuse + sysctl_ip_vs_lblc_expiration))
				continue;

			ip_vs_lblc_free(en);
			atomic_dec(&tbl->entries);
		}
		write_unlock(&svc->sched_lock);
	}
	tbl->rover = j;
}


static void ip_vs_lblc_check_expire(unsigned long data)
{
	struct ip_vs_service *svc = (struct ip_vs_service *) data;
	struct ip_vs_lblc_table *tbl = svc->sched_data;
	unsigned long now = jiffies;
	int goal;
	int i, j;
	struct ip_vs_lblc_entry *en, *nxt;

	if ((tbl->counter % COUNT_FOR_FULL_EXPIRATION) == 0) {
		/* do full expiration check */
		ip_vs_lblc_full_check(svc);
		tbl->counter = 1;
		goto out;
	}

	if (atomic_read(&tbl->entries) <= tbl->max_size) {
		tbl->counter++;
		goto out;
	}

	goal = (atomic_read(&tbl->entries) - tbl->max_size)*4/3;
	if (goal > tbl->max_size/2)
		goal = tbl->max_size/2;

	for (i=0, j=tbl->rover; i<IP_VS_LBLC_TAB_SIZE; i++) {
		j = (j + 1) & IP_VS_LBLC_TAB_MASK;

		write_lock(&svc->sched_lock);
		list_for_each_entry_safe(en, nxt, &tbl->bucket[j], list) {
			if (time_before(now, en->lastuse + ENTRY_TIMEOUT))
				continue;

			ip_vs_lblc_free(en);
			atomic_dec(&tbl->entries);
			goal--;
		}
		write_unlock(&svc->sched_lock);
		if (goal <= 0)
			break;
	}
	tbl->rover = j;

  out:
	mod_timer(&tbl->periodic_timer, jiffies+CHECK_EXPIRE_INTERVAL);
}


static int ip_vs_lblc_init_svc(struct ip_vs_service *svc)
{
	int i;
	struct ip_vs_lblc_table *tbl;

	/*
	 *    Allocate the ip_vs_lblc_table for this service
	 */
	tbl = kmalloc(sizeof(*tbl), GFP_ATOMIC);
	if (tbl == NULL) {
		pr_err("%s(): no memory\n", __func__);
		return -ENOMEM;
	}
	svc->sched_data = tbl;
	IP_VS_DBG(6, "LBLC hash table (memory=%Zdbytes) allocated for "
		  "current service\n", sizeof(*tbl));

	/*
	 *    Initialize the hash buckets
	 */
	for (i=0; i<IP_VS_LBLC_TAB_SIZE; i++) {
		INIT_LIST_HEAD(&tbl->bucket[i]);
	}
	tbl->max_size = IP_VS_LBLC_TAB_SIZE*16;
	tbl->rover = 0;
	tbl->counter = 1;

	/*
	 *    Hook periodic timer for garbage collection
	 */
	setup_timer(&tbl->periodic_timer, ip_vs_lblc_check_expire,
			(unsigned long)svc);
	mod_timer(&tbl->periodic_timer, jiffies + CHECK_EXPIRE_INTERVAL);

	return 0;
}


static int ip_vs_lblc_done_svc(struct ip_vs_service *svc)
{
	struct ip_vs_lblc_table *tbl = svc->sched_data;

	/* remove periodic timer */
	del_timer_sync(&tbl->periodic_timer);

	/* got to clean up table entries here */
	ip_vs_lblc_flush(tbl);

	/* release the table itself */
	kfree(tbl);
	IP_VS_DBG(6, "LBLC hash table (memory=%Zdbytes) released\n",
		  sizeof(*tbl));

	return 0;
}


static inline struct ip_vs_dest *
__ip_vs_lblc_schedule(struct ip_vs_service *svc)
{
	struct ip_vs_dest *dest, *least;
	int loh, doh;

	/*
	 * We think the overhead of processing active connections is fifty
	 * times higher than that of inactive connections in average. (This
	 * fifty times might not be accurate, we will change it later.) We
	 * use the following formula to estimate the overhead:
	 *                dest->activeconns*50 + dest->inactconns
	 * and the load:
	 *                (dest overhead) / dest->weight
	 *
	 * Remember -- no floats in kernel mode!!!
	 * The comparison of h1*w2 > h2*w1 is equivalent to that of
	 *                h1/w1 > h2/w2
	 * if every weight is larger than zero.
	 *
	 * The server with weight=0 is quiesced and will not receive any
	 * new connection.
	 */
	list_for_each_entry(dest, &svc->destinations, n_list) {
		if (dest->flags & IP_VS_DEST_F_OVERLOAD)
			continue;
		if (atomic_read(&dest->weight) > 0) {
			least = dest;
			loh = atomic_read(&least->activeconns) * 50
				+ atomic_read(&least->inactconns);
			goto nextstage;
		}
	}
	return NULL;

	/*
	 *    Find the destination with the least load.
	 */
  nextstage:
	list_for_each_entry_continue(dest, &svc->destinations, n_list) {
		if (dest->flags & IP_VS_DEST_F_OVERLOAD)
			continue;

		doh = atomic_read(&dest->activeconns) * 50
			+ atomic_read(&dest->inactconns);
		if (loh * atomic_read(&dest->weight) >
		    doh * atomic_read(&least->weight)) {
			least = dest;
			loh = doh;
		}
	}

	IP_VS_DBG_BUF(6, "LBLC: server %s:%d "
		      "activeconns %d refcnt %d weight %d overhead %d\n",
		      IP_VS_DBG_ADDR(least->af, &least->addr),
		      ntohs(least->port),
		      atomic_read(&least->activeconns),
		      atomic_read(&least->refcnt),
		      atomic_read(&least->weight), loh);

	return least;
}


/*
 *   If this destination server is overloaded and there is a less loaded
 *   server, then return true.
 */
static inline int
is_overloaded(struct ip_vs_dest *dest, struct ip_vs_service *svc)
{
	if (atomic_read(&dest->activeconns) > atomic_read(&dest->weight)) {
		struct ip_vs_dest *d;

		list_for_each_entry(d, &svc->destinations, n_list) {
			if (atomic_read(&d->activeconns)*2
			    < atomic_read(&d->weight)) {
				return 1;
			}
		}
	}
	return 0;
}


/*
 *    Locality-Based (weighted) Least-Connection scheduling
 */
static struct ip_vs_dest *
ip_vs_lblc_schedule(struct ip_vs_service *svc, const struct sk_buff *skb)
{
	struct ip_vs_lblc_table *tbl = svc->sched_data;
	struct ip_vs_iphdr iph;
	struct ip_vs_dest *dest = NULL;
	struct ip_vs_lblc_entry *en;

	ip_vs_fill_iphdr(svc->af, skb_network_header(skb), &iph);

	IP_VS_DBG(6, "%s(): Scheduling...\n", __func__);

	/* First look in our cache */
	read_lock(&svc->sched_lock);
	en = ip_vs_lblc_get(svc->af, tbl, &iph.daddr);
	if (en) {
		/* We only hold a read lock, but this is atomic */
		en->lastuse = jiffies;

		/*
		 * If the destination is not available, i.e. it's in the trash,
		 * we must ignore it, as it may be removed from under our feet,
		 * if someone drops our reference count. Our caller only makes
		 * sure that destinations, that are not in the trash, are not
		 * moved to the trash, while we are scheduling. But anyone can
		 * free up entries from the trash at any time.
		 */

		if (en->dest->flags & IP_VS_DEST_F_AVAILABLE)
			dest = en->dest;
	}
	read_unlock(&svc->sched_lock);

	/* If the destination has a weight and is not overloaded, use it */
	if (dest && atomic_read(&dest->weight) > 0 && !is_overloaded(dest, svc))
		goto out;

	/* No cache entry or it is invalid, time to schedule */
	dest = __ip_vs_lblc_schedule(svc);
	if (!dest) {
		IP_VS_ERR_RL("LBLC: no destination available\n");
		return NULL;
	}

	/* If we fail to create a cache entry, we'll just use the valid dest */
	write_lock(&svc->sched_lock);
	ip_vs_lblc_new(tbl, &iph.daddr, dest);
	write_unlock(&svc->sched_lock);

out:
	IP_VS_DBG_BUF(6, "LBLC: destination IP address %s --> server %s:%d\n",
		      IP_VS_DBG_ADDR(svc->af, &iph.daddr),
		      IP_VS_DBG_ADDR(svc->af, &dest->addr), ntohs(dest->port));

	return dest;
}


/*
 *      IPVS LBLC Scheduler structure
 */
static struct ip_vs_scheduler ip_vs_lblc_scheduler =
{
	.name =			"lblc",
	.refcnt =		ATOMIC_INIT(0),
	.module =		THIS_MODULE,
	.n_list =		LIST_HEAD_INIT(ip_vs_lblc_scheduler.n_list),
	.init_service =		ip_vs_lblc_init_svc,
	.done_service =		ip_vs_lblc_done_svc,
	.schedule =		ip_vs_lblc_schedule,
};


static int __init ip_vs_lblc_init(void)
{
	int ret;

	sysctl_header = register_sysctl_paths(net_vs_ctl_path, vs_vars_table);
	ret = register_ip_vs_scheduler(&ip_vs_lblc_scheduler);
	if (ret)
		unregister_sysctl_table(sysctl_header);
	return ret;
}


static void __exit ip_vs_lblc_cleanup(void)
{
	unregister_sysctl_table(sysctl_header);
	unregister_ip_vs_scheduler(&ip_vs_lblc_scheduler);
}


module_init(ip_vs_lblc_init);
module_exit(ip_vs_lblc_cleanup);
MODULE_LICENSE("GPL");
