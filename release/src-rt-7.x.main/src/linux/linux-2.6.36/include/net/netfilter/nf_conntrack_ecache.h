/*
 * connection tracking event cache.
 */

#ifndef _NF_CONNTRACK_ECACHE_H
#define _NF_CONNTRACK_ECACHE_H
#include <net/netfilter/nf_conntrack.h>

#include <net/net_namespace.h>
#include <net/netfilter/nf_conntrack_expect.h>
#include <linux/netfilter/nf_conntrack_common.h>
#include <linux/netfilter/nf_conntrack_tuple_common.h>
#include <net/netfilter/nf_conntrack_extend.h>

struct nf_conntrack_ecache {
	unsigned long cache;	/* bitops want long */
	unsigned long missed;	/* missed events */
	u16 ctmask;		/* bitmask of ct events to be delivered */
	u16 expmask;		/* bitmask of expect events to be delivered */
	u32 pid;		/* netlink pid of destroyer */
};

static inline struct nf_conntrack_ecache *
nf_ct_ecache_find(const struct nf_conn *ct)
{
	return nf_ct_ext_find(ct, NF_CT_EXT_ECACHE);
}

static inline struct nf_conntrack_ecache *
nf_ct_ecache_ext_add(struct nf_conn *ct, u16 ctmask, u16 expmask, gfp_t gfp)
{
	struct net *net = nf_ct_net(ct);
	struct nf_conntrack_ecache *e;

	if (!ctmask && !expmask && net->ct.sysctl_events) {
		ctmask = ~0;
		expmask = ~0;
	}
	if (!ctmask && !expmask)
		return NULL;

	e = nf_ct_ext_add(ct, NF_CT_EXT_ECACHE, gfp);
	if (e) {
		e->ctmask  = ctmask;
		e->expmask = expmask;
	}
	return e;
};

#ifdef CONFIG_NF_CONNTRACK_EVENTS
/* This structure is passed to event handler */
struct nf_ct_event {
	struct nf_conn *ct;
	u32 pid;
	int report;
};

struct nf_ct_event_notifier {
	int (*fcn)(unsigned int events, struct nf_ct_event *item);
};

extern struct nf_ct_event_notifier *nf_conntrack_event_cb;
extern int nf_conntrack_register_notifier(struct nf_ct_event_notifier *nb);
extern void nf_conntrack_unregister_notifier(struct nf_ct_event_notifier *nb);

extern void nf_ct_deliver_cached_events(struct nf_conn *ct);

static inline void
nf_conntrack_event_cache(enum ip_conntrack_events event, struct nf_conn *ct)
{
	struct nf_conntrack_ecache *e;

	if (nf_conntrack_event_cb == NULL)
		return;

	e = nf_ct_ecache_find(ct);
	if (e == NULL)
		return;

	if (!(e->ctmask & (1 << event)))
		return;

	set_bit(event, &e->cache);
}

static inline int
nf_conntrack_eventmask_report(unsigned int eventmask,
			      struct nf_conn *ct,
			      u32 pid,
			      int report)
{
	int ret = 0;
	struct nf_ct_event_notifier *notify;
	struct nf_conntrack_ecache *e;

	rcu_read_lock();
	notify = rcu_dereference(nf_conntrack_event_cb);
	if (notify == NULL)
		goto out_unlock;

	e = nf_ct_ecache_find(ct);
	if (e == NULL)
		goto out_unlock;

	if (nf_ct_is_confirmed(ct) && !nf_ct_is_dying(ct)) {
		struct nf_ct_event item = {
			.ct 	= ct,
			.pid	= e->pid ? e->pid : pid,
			.report = report
		};
		/* This is a resent of a destroy event? If so, skip missed */
		unsigned long missed = e->pid ? 0 : e->missed;

		if (!((eventmask | missed) & e->ctmask))
			goto out_unlock;

		ret = notify->fcn(eventmask | missed, &item);
		if (unlikely(ret < 0 || missed)) {
			spin_lock_bh(&ct->lock);
			if (ret < 0) {
				/* This is a destroy event that has been
				 * triggered by a process, we store the PID
				 * to include it in the retransmission. */
				if (eventmask & (1 << IPCT_DESTROY) &&
				    e->pid == 0 && pid != 0)
					e->pid = pid;
				else
					e->missed |= eventmask;
			} else
				e->missed &= ~missed;
			spin_unlock_bh(&ct->lock);
		}
	}
out_unlock:
	rcu_read_unlock();
	return ret;
}

static inline int
nf_conntrack_event_report(enum ip_conntrack_events event, struct nf_conn *ct,
			  u32 pid, int report)
{
	return nf_conntrack_eventmask_report(1 << event, ct, pid, report);
}

static inline int
nf_conntrack_event(enum ip_conntrack_events event, struct nf_conn *ct)
{
	return nf_conntrack_eventmask_report(1 << event, ct, 0, 0);
}

struct nf_exp_event {
	struct nf_conntrack_expect *exp;
	u32 pid;
	int report;
};

struct nf_exp_event_notifier {
	int (*fcn)(unsigned int events, struct nf_exp_event *item);
};

extern struct nf_exp_event_notifier *nf_expect_event_cb;
extern int nf_ct_expect_register_notifier(struct nf_exp_event_notifier *nb);
extern void nf_ct_expect_unregister_notifier(struct nf_exp_event_notifier *nb);

static inline void
nf_ct_expect_event_report(enum ip_conntrack_expect_events event,
			  struct nf_conntrack_expect *exp,
			  u32 pid,
			  int report)
{
	struct nf_exp_event_notifier *notify;
	struct nf_conntrack_ecache *e;

	rcu_read_lock();
	notify = rcu_dereference(nf_expect_event_cb);
	if (notify == NULL)
		goto out_unlock;

	e = nf_ct_ecache_find(exp->master);
	if (e == NULL)
		goto out_unlock;

	if (e->expmask & (1 << event)) {
		struct nf_exp_event item = {
			.exp	= exp,
			.pid	= pid,
			.report = report
		};
		notify->fcn(1 << event, &item);
	}
out_unlock:
	rcu_read_unlock();
}

static inline void
nf_ct_expect_event(enum ip_conntrack_expect_events event,
		   struct nf_conntrack_expect *exp)
{
	nf_ct_expect_event_report(event, exp, 0, 0);
}

extern int nf_conntrack_ecache_init(struct net *net);
extern void nf_conntrack_ecache_fini(struct net *net);

#else /* CONFIG_NF_CONNTRACK_EVENTS */

static inline void nf_conntrack_event_cache(enum ip_conntrack_events event,
					    struct nf_conn *ct) {}
static inline int nf_conntrack_eventmask_report(unsigned int eventmask,
						struct nf_conn *ct,
						u32 pid,
						int report) { return 0; }
static inline int nf_conntrack_event(enum ip_conntrack_events event,
				     struct nf_conn *ct) { return 0; }
static inline int nf_conntrack_event_report(enum ip_conntrack_events event,
					    struct nf_conn *ct,
					    u32 pid,
					    int report) { return 0; }
static inline void nf_ct_deliver_cached_events(const struct nf_conn *ct) {}
static inline void nf_ct_expect_event(enum ip_conntrack_expect_events event,
				      struct nf_conntrack_expect *exp) {}
static inline void nf_ct_expect_event_report(enum ip_conntrack_expect_events e,
					     struct nf_conntrack_expect *exp,
 					     u32 pid,
 					     int report) {}

static inline int nf_conntrack_ecache_init(struct net *net)
{
	return 0;
}

static inline void nf_conntrack_ecache_fini(struct net *net)
{
}
#endif /* CONFIG_NF_CONNTRACK_EVENTS */

#endif /*_NF_CONNTRACK_ECACHE_H*/
