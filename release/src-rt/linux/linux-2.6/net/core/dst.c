/*
 * net/core/dst.c	Protocol independent destination cache.
 *
 * Authors:		Alexey Kuznetsov, <kuznet@ms2.inr.ac.ru>
 *
 */

#include <linux/bitops.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/workqueue.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/string.h>
#include <linux/types.h>
#include <net/net_namespace.h>
#include <linux/sched.h>

#include <net/dst.h>

/*
 * Theory of operations:
 * 1) We use a list, protected by a spinlock, to add
 *    new entries from both BH and non-BH context.
 * 2) In order to keep spinlock held for a small delay,
 *    we use a second list where are stored long lived
 *    entries, that are handled by the garbage collect thread
 *    fired by a workqueue.
 * 3) This list is guarded by a mutex,
 *    so that the gc_task and dst_dev_event() can be synchronized.
 */
#if RT_CACHE_DEBUG >= 2
static atomic_t			 dst_total = ATOMIC_INIT(0);
#endif

/*
 * We want to keep lock & list close together
 * to dirty as few cache lines as possible in __dst_free().
 * As this is not a very strong hint, we dont force an alignment on SMP.
 */
static struct {
	spinlock_t		lock;
	struct dst_entry	*list;
	unsigned long		timer_inc;
	unsigned long		timer_expires;
} dst_garbage = {
	.lock = __SPIN_LOCK_UNLOCKED(dst_garbage.lock),
	.timer_inc = DST_GC_MAX,
};
static void dst_gc_task(struct work_struct *work);
static void ___dst_free(struct dst_entry *dst);

static DECLARE_DELAYED_WORK(dst_gc_work, dst_gc_task);

static DEFINE_MUTEX(dst_gc_mutex);
/*
 * long lived entries are maintained in this list, guarded by dst_gc_mutex
 */
static struct dst_entry         *dst_busy_list;

static void dst_gc_task(struct work_struct *work)
{
	int    delayed = 0;
	int    work_performed = 0;
	unsigned long expires = ~0L;
	struct dst_entry *dst, *next, head;
	struct dst_entry *last = &head;
#if RT_CACHE_DEBUG >= 2
	ktime_t time_start = ktime_get();
	struct timespec elapsed;
#endif

	mutex_lock(&dst_gc_mutex);
	next = dst_busy_list;

loop:
	while ((dst = next) != NULL) {
		next = dst->next;
		prefetch(&next->next);
		cond_resched();
		if (likely(atomic_read(&dst->__refcnt))) {
			last->next = dst;
			last = dst;
			delayed++;
			continue;
		}
		work_performed++;

		dst = dst_destroy(dst);
		if (dst) {
			/* NOHASH and still referenced. Unless it is already
			 * on gc list, invalidate it and add to gc list.
			 *
			 * Note: this is temporary. Actually, NOHASH dst's
			 * must be obsoleted when parent is obsoleted.
			 * But we do not have state "obsoleted, but
			 * referenced by parent", so it is right.
			 */
			if (dst->obsolete > 1)
				continue;

			___dst_free(dst);
			dst->next = next;
			next = dst;
		}
	}

	spin_lock_bh(&dst_garbage.lock);
	next = dst_garbage.list;
	if (next) {
		dst_garbage.list = NULL;
		spin_unlock_bh(&dst_garbage.lock);
		goto loop;
	}
	last->next = NULL;
	dst_busy_list = head.next;
	if (!dst_busy_list)
		dst_garbage.timer_inc = DST_GC_MAX;
	else {
		/*
		 * if we freed less than 1/10 of delayed entries,
		 * we can sleep longer.
		 */
		if (work_performed <= delayed/10) {
			dst_garbage.timer_expires += dst_garbage.timer_inc;
			if (dst_garbage.timer_expires > DST_GC_MAX)
				dst_garbage.timer_expires = DST_GC_MAX;
			dst_garbage.timer_inc += DST_GC_INC;
		} else {
			dst_garbage.timer_inc = DST_GC_INC;
			dst_garbage.timer_expires = DST_GC_MIN;
		}
		expires = dst_garbage.timer_expires;
		/*
		 * if the next desired timer is more than 4 seconds in the
		 * future then round the timer to whole seconds
		 */
		if (expires > 4*HZ)
			expires = round_jiffies_relative(expires);
		schedule_delayed_work(&dst_gc_work, expires);
	}

	spin_unlock_bh(&dst_garbage.lock);
	mutex_unlock(&dst_gc_mutex);
#if RT_CACHE_DEBUG >= 2
	elapsed = ktime_to_timespec(ktime_sub(ktime_get(), time_start));
	printk(KERN_DEBUG "dst_total: %d delayed: %d work_perf: %d"
		" expires: %lu elapsed: %lu us\n",
		atomic_read(&dst_total), delayed, work_performed,
		expires,
		elapsed.tv_sec * USEC_PER_SEC +
		  elapsed.tv_nsec / NSEC_PER_USEC);
#endif
}

int dst_discard(struct sk_buff *skb)
{
	kfree_skb(skb);
	return 0;
}
EXPORT_SYMBOL(dst_discard);

const u32 dst_default_metrics[RTAX_MAX];

void *dst_alloc(struct dst_ops *ops, int initial_ref)
{
	struct dst_entry *dst;

	if (ops->gc && dst_entries_get_fast(ops) > ops->gc_thresh) {
		if (ops->gc(ops))
			return NULL;
	}
	dst = kmem_cache_zalloc(ops->kmem_cachep, GFP_ATOMIC);
	if (!dst)
		return NULL;
	atomic_set(&dst->__refcnt, initial_ref);
	dst->ops = ops;
	dst->lastuse = jiffies;
	dst->path = dst;
	dst->input = dst->output = dst_discard;
	dst_init_metrics(dst, dst_default_metrics, true);
#if RT_CACHE_DEBUG >= 2
	atomic_inc(&dst_total);
#endif
	dst_entries_add(ops, 1);
	return dst;
}
EXPORT_SYMBOL(dst_alloc);

static void ___dst_free(struct dst_entry *dst)
{
	/* The first case (dev==NULL) is required, when
	   protocol module is unloaded.
	 */
	if (dst->dev == NULL || !(dst->dev->flags&IFF_UP))
		dst->input = dst->output = dst_discard;
	dst->obsolete = 2;
}

void __dst_free(struct dst_entry *dst)
{
	spin_lock_bh(&dst_garbage.lock);
	___dst_free(dst);
	dst->next = dst_garbage.list;
	dst_garbage.list = dst;
	if (dst_garbage.timer_inc > DST_GC_INC) {
		dst_garbage.timer_inc = DST_GC_INC;
		dst_garbage.timer_expires = DST_GC_MIN;
		cancel_delayed_work(&dst_gc_work);
		schedule_delayed_work(&dst_gc_work, dst_garbage.timer_expires);
	}
	spin_unlock_bh(&dst_garbage.lock);
}
EXPORT_SYMBOL(__dst_free);

struct dst_entry *dst_destroy(struct dst_entry * dst)
{
	struct dst_entry *child;
	struct neighbour *neigh;
	struct hh_cache *hh;

	smp_rmb();

again:
	neigh = dst->neighbour;
	hh = dst->hh;
	child = dst->child;

	dst->hh = NULL;
	if (hh)
		hh_cache_put(hh);

	if (neigh) {
		dst->neighbour = NULL;
		neigh_release(neigh);
	}

	dst_entries_add(dst->ops, -1);

	if (dst->ops->destroy)
		dst->ops->destroy(dst);
	if (dst->dev)
		dev_put(dst->dev);
#if RT_CACHE_DEBUG >= 2
	atomic_dec(&dst_total);
#endif
	kmem_cache_free(dst->ops->kmem_cachep, dst);

	dst = child;
	if (dst) {
		int nohash = dst->flags & DST_NOHASH;

		if (atomic_dec_and_test(&dst->__refcnt)) {
			/* We were real parent of this dst, so kill child. */
			if (nohash)
				goto again;
		} else {
			/* Child is still referenced, return it for freeing. */
			if (nohash)
				return dst;
			/* Child is still in his hash table */
		}
	}
	return NULL;
}
EXPORT_SYMBOL(dst_destroy);

void dst_release(struct dst_entry *dst)
{
	if (dst) {
		int newrefcnt;

		newrefcnt = atomic_dec_return(&dst->__refcnt);
		WARN_ON(newrefcnt < 0);
		if (unlikely(dst->flags & DST_NOCACHE) && !newrefcnt) {
			dst = dst_destroy(dst);
			if (dst)
				__dst_free(dst);
		}
	}
}
EXPORT_SYMBOL(dst_release);

u32 *dst_cow_metrics_generic(struct dst_entry *dst, unsigned long old)
{
	u32 *p = kmalloc(sizeof(u32) * RTAX_MAX, GFP_ATOMIC);

	if (p) {
		u32 *old_p = __DST_METRICS_PTR(old);
		unsigned long prev, new;

		memcpy(p, old_p, sizeof(u32) * RTAX_MAX);

		new = (unsigned long) p;
		prev = cmpxchg(&dst->_metrics, old, new);

		if (prev != old) {
			kfree(p);
			p = __DST_METRICS_PTR(prev);
			if (prev & DST_METRICS_READ_ONLY)
				p = NULL;
		}
	}
	return p;
}
EXPORT_SYMBOL(dst_cow_metrics_generic);

/* Caller asserts that dst_metrics_read_only(dst) is false.  */
void __dst_destroy_metrics_generic(struct dst_entry *dst, unsigned long old)
{
	unsigned long prev, new;

	new = ((unsigned long) dst_default_metrics) | DST_METRICS_READ_ONLY;
	prev = cmpxchg(&dst->_metrics, old, new);
	if (prev == old)
		kfree(__DST_METRICS_PTR(old));
}
EXPORT_SYMBOL(__dst_destroy_metrics_generic);

/**
 * skb_dst_set_noref - sets skb dst, without a reference
 * @skb: buffer
 * @dst: dst entry
 *
 * Sets skb dst, assuming a reference was not taken on dst
 * skb_dst_drop() should not dst_release() this dst
 */
void skb_dst_set_noref(struct sk_buff *skb, struct dst_entry *dst)
{
	WARN_ON(!rcu_read_lock_held() && !rcu_read_lock_bh_held());
	/* If dst not in cache, we must take a reference, because
	 * dst_release() will destroy dst as soon as its refcount becomes zero
	 */
	if (unlikely(dst->flags & DST_NOCACHE)) {
		dst_hold(dst);
		skb_dst_set(skb, dst);
	} else {
		skb->_skb_refdst = (unsigned long)dst | SKB_DST_NOREF;
	}
}
EXPORT_SYMBOL(skb_dst_set_noref);

/* Dirty hack. We did it in 2.2 (in __dst_free),
 * we have _very_ good reasons not to repeat
 * this mistake in 2.3, but we have no choice
 * now. _It_ _is_ _explicit_ _deliberate_
 * _race_ _condition_.
 *
 * Commented and originally written by Alexey.
 */
static void dst_ifdown(struct dst_entry *dst, struct net_device *dev,
		       int unregister)
{
	if (dst->ops->ifdown)
		dst->ops->ifdown(dst, dev, unregister);

	if (dev != dst->dev)
		return;

	if (!unregister) {
		dst->input = dst->output = dst_discard;
	} else {
		dst->dev = dev_net(dst->dev)->loopback_dev;
		dev_hold(dst->dev);
		dev_put(dev);
		if (dst->neighbour && dst->neighbour->dev == dev) {
			dst->neighbour->dev = dst->dev;
			dev_hold(dst->dev);
			dev_put(dev);
		}
	}
}

static int dst_dev_event(struct notifier_block *this, unsigned long event,
			 void *ptr)
{
	struct net_device *dev = ptr;
	struct dst_entry *dst, *last = NULL;

	switch (event) {
	case NETDEV_UNREGISTER:
	case NETDEV_DOWN:
		mutex_lock(&dst_gc_mutex);
		for (dst = dst_busy_list; dst; dst = dst->next) {
			last = dst;
			dst_ifdown(dst, dev, event != NETDEV_DOWN);
		}

		spin_lock_bh(&dst_garbage.lock);
		dst = dst_garbage.list;
		dst_garbage.list = NULL;
		spin_unlock_bh(&dst_garbage.lock);

		if (last)
			last->next = dst;
		else
			dst_busy_list = dst;
		for (; dst; dst = dst->next)
			dst_ifdown(dst, dev, event != NETDEV_DOWN);
		mutex_unlock(&dst_gc_mutex);
		break;
	}
	return NOTIFY_DONE;
}

static struct notifier_block dst_dev_notifier = {
	.notifier_call	= dst_dev_event,
	.priority = -10, /* must be called after other network notifiers */
};

void __init dst_init(void)
{
	register_netdevice_notifier(&dst_dev_notifier);
}
