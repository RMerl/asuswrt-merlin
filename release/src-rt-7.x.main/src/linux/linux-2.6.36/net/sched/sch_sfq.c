/*
 * net/sched/sch_sfq.c	Stochastic Fairness Queueing discipline.
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:	Alexey Kuznetsov, <kuznet@ms2.inr.ac.ru>
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/jiffies.h>
#include <linux/string.h>
#include <linux/in.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/ipv6.h>
#include <linux/skbuff.h>
#include <linux/jhash.h>
#include <linux/slab.h>
#include <net/ip.h>
#include <net/netlink.h>
#include <net/pkt_sched.h>


/*	Stochastic Fairness Queuing algorithm.
	=======================================

	Source:
	Paul E. McKenney "Stochastic Fairness Queuing",
	IEEE INFOCOMM'90 Proceedings, San Francisco, 1990.

	Paul E. McKenney "Stochastic Fairness Queuing",
	"Interworking: Research and Experience", v.2, 1991, p.113-131.


	See also:
	M. Shreedhar and George Varghese "Efficient Fair
	Queuing using Deficit Round Robin", Proc. SIGCOMM 95.


	This is not the thing that is usually called (W)FQ nowadays.
	It does not use any timestamp mechanism, but instead
	processes queues in round-robin order.

	ADVANTAGE:

	- It is very cheap. Both CPU and memory requirements are minimal.

	DRAWBACKS:

	- "Stochastic" -> It is not 100% fair.
	When hash collisions occur, several flows are considered as one.

	- "Round-robin" -> It introduces larger delays than virtual clock
	based schemes, and should not be used for isolating interactive
	traffic	from non-interactive. It means, that this scheduler
	should be used as leaf of CBQ or P3, which put interactive traffic
	to higher priority band.

	We still need true WFQ for top level CSZ, but using WFQ
	for the best effort traffic is absolutely pointless:
	SFQ is superior for this purpose.

	IMPLEMENTATION:
	This implementation limits maximal queue length to 128;
	maximal mtu to 2^15-1; number of hash buckets to 1024.
	The only goal of this restrictions was that all data
	fit into one 4K page :-). Struct sfq_sched_data is
	organized in anti-cache manner: all the data for a bucket
	are scattered over different locations. This is not good,
	but it allowed me to put it into 4K.

	It is easy to increase these values, but not in flight.  */

#define SFQ_DEPTH		128
#define SFQ_HASH_DIVISOR	1024

/* This type should contain at least SFQ_DEPTH*2 values */
typedef unsigned char sfq_index;

struct sfq_head
{
	sfq_index	next;
	sfq_index	prev;
};

struct sfq_sched_data
{
/* Parameters */
	int		perturb_period;
	unsigned	quantum;	/* Allotment per round: MUST BE >= MTU */
	int		limit;

/* Variables */
	struct tcf_proto *filter_list;
	struct timer_list perturb_timer;
	u32		perturbation;
	sfq_index	tail;		/* Index of current slot in round */
	sfq_index	max_depth;	/* Maximal depth */

	sfq_index	ht[SFQ_HASH_DIVISOR];	/* Hash table */
	sfq_index	next[SFQ_DEPTH];	/* Active slots link */
	short		allot[SFQ_DEPTH];	/* Current allotment per slot */
	unsigned short	hash[SFQ_DEPTH];	/* Hash value indexed by slots */
	struct sk_buff_head	qs[SFQ_DEPTH];		/* Slot queue */
	struct sfq_head	dep[SFQ_DEPTH*2];	/* Linked list of slots, indexed by depth */
};

static __inline__ unsigned sfq_fold_hash(struct sfq_sched_data *q, u32 h, u32 h1)
{
	return jhash_2words(h, h1, q->perturbation) & (SFQ_HASH_DIVISOR - 1);
}

static unsigned sfq_hash(struct sfq_sched_data *q, struct sk_buff *skb)
{
	u32 h, h2;

	switch (skb->protocol) {
	case htons(ETH_P_IP):
	{
		const struct iphdr *iph;

		if (!pskb_network_may_pull(skb, sizeof(*iph)))
			goto err;
		iph = ip_hdr(skb);
		h = (__force u32)iph->daddr;
		h2 = (__force u32)iph->saddr ^ iph->protocol;
		if (!(iph->frag_off&htons(IP_MF|IP_OFFSET)) &&
		    (iph->protocol == IPPROTO_TCP ||
		     iph->protocol == IPPROTO_UDP ||
		     iph->protocol == IPPROTO_UDPLITE ||
		     iph->protocol == IPPROTO_SCTP ||
		     iph->protocol == IPPROTO_DCCP ||
		     iph->protocol == IPPROTO_ESP) &&
		     pskb_network_may_pull(skb, iph->ihl * 4 + 4))
			h2 ^= *(((u32*)iph) + iph->ihl);
		break;
	}
	case htons(ETH_P_IPV6):
	{
		struct ipv6hdr *iph;

		if (!pskb_network_may_pull(skb, sizeof(*iph)))
			goto err;
		iph = ipv6_hdr(skb);
		h = (__force u32)iph->daddr.s6_addr32[3];
		h2 = (__force u32)iph->saddr.s6_addr32[3] ^ iph->nexthdr;
		if ((iph->nexthdr == IPPROTO_TCP ||
		     iph->nexthdr == IPPROTO_UDP ||
		     iph->nexthdr == IPPROTO_UDPLITE ||
		     iph->nexthdr == IPPROTO_SCTP ||
		     iph->nexthdr == IPPROTO_DCCP ||
		     iph->nexthdr == IPPROTO_ESP) &&
		    pskb_network_may_pull(skb, sizeof(*iph) + 4))
			h2 ^= *(u32*)&iph[1];
		break;
	}
	default:
err:
		h = (unsigned long)skb_dst(skb) ^ (__force u32)skb->protocol;
		h2 = (unsigned long)skb->sk;
	}

	return sfq_fold_hash(q, h, h2);
}

static unsigned int sfq_classify(struct sk_buff *skb, struct Qdisc *sch,
				 int *qerr)
{
	struct sfq_sched_data *q = qdisc_priv(sch);
	struct tcf_result res;
	int result;

	if (TC_H_MAJ(skb->priority) == sch->handle &&
	    TC_H_MIN(skb->priority) > 0 &&
	    TC_H_MIN(skb->priority) <= SFQ_HASH_DIVISOR)
		return TC_H_MIN(skb->priority);

	if (!q->filter_list)
		return sfq_hash(q, skb) + 1;

	*qerr = NET_XMIT_SUCCESS | __NET_XMIT_BYPASS;
	result = tc_classify(skb, q->filter_list, &res);
	if (result >= 0) {
#ifdef CONFIG_NET_CLS_ACT
		switch (result) {
		case TC_ACT_STOLEN:
		case TC_ACT_QUEUED:
			*qerr = NET_XMIT_SUCCESS | __NET_XMIT_STOLEN;
		case TC_ACT_SHOT:
			return 0;
		}
#endif
		if (TC_H_MIN(res.classid) <= SFQ_HASH_DIVISOR)
			return TC_H_MIN(res.classid);
	}
	return 0;
}

static inline void sfq_link(struct sfq_sched_data *q, sfq_index x)
{
	sfq_index p, n;
	int d = q->qs[x].qlen + SFQ_DEPTH;

	p = d;
	n = q->dep[d].next;
	q->dep[x].next = n;
	q->dep[x].prev = p;
	q->dep[p].next = q->dep[n].prev = x;
}

static inline void sfq_dec(struct sfq_sched_data *q, sfq_index x)
{
	sfq_index p, n;

	n = q->dep[x].next;
	p = q->dep[x].prev;
	q->dep[p].next = n;
	q->dep[n].prev = p;

	if (n == p && q->max_depth == q->qs[x].qlen + 1)
		q->max_depth--;

	sfq_link(q, x);
}

static inline void sfq_inc(struct sfq_sched_data *q, sfq_index x)
{
	sfq_index p, n;
	int d;

	n = q->dep[x].next;
	p = q->dep[x].prev;
	q->dep[p].next = n;
	q->dep[n].prev = p;
	d = q->qs[x].qlen;
	if (q->max_depth < d)
		q->max_depth = d;

	sfq_link(q, x);
}

static unsigned int sfq_drop(struct Qdisc *sch)
{
	struct sfq_sched_data *q = qdisc_priv(sch);
	sfq_index d = q->max_depth;
	struct sk_buff *skb;
	unsigned int len;

	/* Queue is full! Find the longest slot and
	   drop a packet from it */

	if (d > 1) {
		sfq_index x = q->dep[d + SFQ_DEPTH].next;
		skb = q->qs[x].prev;
		len = qdisc_pkt_len(skb);
		__skb_unlink(skb, &q->qs[x]);
		kfree_skb(skb);
		sfq_dec(q, x);
		sch->q.qlen--;
		sch->qstats.drops++;
		sch->qstats.backlog -= len;
		return len;
	}

	if (d == 1) {
		/* It is difficult to believe, but ALL THE SLOTS HAVE LENGTH 1. */
		d = q->next[q->tail];
		q->next[q->tail] = q->next[d];
		q->allot[q->next[d]] += q->quantum;
		skb = q->qs[d].prev;
		len = qdisc_pkt_len(skb);
		__skb_unlink(skb, &q->qs[d]);
		kfree_skb(skb);
		sfq_dec(q, d);
		sch->q.qlen--;
		q->ht[q->hash[d]] = SFQ_DEPTH;
		sch->qstats.drops++;
		sch->qstats.backlog -= len;
		return len;
	}

	return 0;
}

static int
sfq_enqueue(struct sk_buff *skb, struct Qdisc *sch)
{
	struct sfq_sched_data *q = qdisc_priv(sch);
	unsigned int hash;
	sfq_index x;
	int uninitialized_var(ret);

	hash = sfq_classify(skb, sch, &ret);
	if (hash == 0) {
		if (ret & __NET_XMIT_BYPASS)
			sch->qstats.drops++;
		kfree_skb(skb);
		return ret;
	}
	hash--;

	x = q->ht[hash];
	if (x == SFQ_DEPTH) {
		q->ht[hash] = x = q->dep[SFQ_DEPTH].next;
		q->hash[x] = hash;
	}

	/* If selected queue has length q->limit, this means that
	 * all another queues are empty and that we do simple tail drop,
	 * i.e. drop _this_ packet.
	 */
	if (q->qs[x].qlen >= q->limit)
		return qdisc_drop(skb, sch);

	sch->qstats.backlog += qdisc_pkt_len(skb);
	__skb_queue_tail(&q->qs[x], skb);
	sfq_inc(q, x);
	if (q->qs[x].qlen == 1) {		/* The flow is new */
		if (q->tail == SFQ_DEPTH) {	/* It is the first flow */
			q->tail = x;
			q->next[x] = x;
			q->allot[x] = q->quantum;
		} else {
			q->next[x] = q->next[q->tail];
			q->next[q->tail] = x;
			q->tail = x;
		}
	}
	if (++sch->q.qlen <= q->limit) {
		sch->bstats.bytes += qdisc_pkt_len(skb);
		sch->bstats.packets++;
		return NET_XMIT_SUCCESS;
	}

	sfq_drop(sch);
	return NET_XMIT_CN;
}

static struct sk_buff *
sfq_peek(struct Qdisc *sch)
{
	struct sfq_sched_data *q = qdisc_priv(sch);
	sfq_index a;

	/* No active slots */
	if (q->tail == SFQ_DEPTH)
		return NULL;

	a = q->next[q->tail];
	return skb_peek(&q->qs[a]);
}

static struct sk_buff *
sfq_dequeue(struct Qdisc *sch)
{
	struct sfq_sched_data *q = qdisc_priv(sch);
	struct sk_buff *skb;
	sfq_index a, old_a;

	/* No active slots */
	if (q->tail == SFQ_DEPTH)
		return NULL;

	a = old_a = q->next[q->tail];

	/* Grab packet */
	skb = __skb_dequeue(&q->qs[a]);
	sfq_dec(q, a);
	sch->q.qlen--;
	sch->qstats.backlog -= qdisc_pkt_len(skb);

	/* Is the slot empty? */
	if (q->qs[a].qlen == 0) {
		q->ht[q->hash[a]] = SFQ_DEPTH;
		a = q->next[a];
		if (a == old_a) {
			q->tail = SFQ_DEPTH;
			return skb;
		}
		q->next[q->tail] = a;
		q->allot[a] += q->quantum;
	} else if ((q->allot[a] -= qdisc_pkt_len(skb)) <= 0) {
		q->tail = a;
		a = q->next[a];
		q->allot[a] += q->quantum;
	}
	return skb;
}

static void
sfq_reset(struct Qdisc *sch)
{
	struct sk_buff *skb;

	while ((skb = sfq_dequeue(sch)) != NULL)
		kfree_skb(skb);
}

static void sfq_perturbation(unsigned long arg)
{
	struct Qdisc *sch = (struct Qdisc *)arg;
	struct sfq_sched_data *q = qdisc_priv(sch);

	q->perturbation = net_random();

	if (q->perturb_period)
		mod_timer(&q->perturb_timer, jiffies + q->perturb_period);
}

static int sfq_change(struct Qdisc *sch, struct nlattr *opt)
{
	struct sfq_sched_data *q = qdisc_priv(sch);
	struct tc_sfq_qopt *ctl = nla_data(opt);
	unsigned int qlen;

	if (opt->nla_len < nla_attr_size(sizeof(*ctl)))
		return -EINVAL;

	sch_tree_lock(sch);
	q->quantum = ctl->quantum ? : psched_mtu(qdisc_dev(sch));
	q->perturb_period = ctl->perturb_period * HZ;
	if (ctl->limit)
		q->limit = min_t(u32, ctl->limit, SFQ_DEPTH - 1);

	qlen = sch->q.qlen;
	while (sch->q.qlen > q->limit)
		sfq_drop(sch);
	qdisc_tree_decrease_qlen(sch, qlen - sch->q.qlen);

	del_timer(&q->perturb_timer);
	if (q->perturb_period) {
		mod_timer(&q->perturb_timer, jiffies + q->perturb_period);
		q->perturbation = net_random();
	}
	sch_tree_unlock(sch);
	return 0;
}

static int sfq_init(struct Qdisc *sch, struct nlattr *opt)
{
	struct sfq_sched_data *q = qdisc_priv(sch);
	int i;

	q->perturb_timer.function = sfq_perturbation;
	q->perturb_timer.data = (unsigned long)sch;
	init_timer_deferrable(&q->perturb_timer);

	for (i = 0; i < SFQ_HASH_DIVISOR; i++)
		q->ht[i] = SFQ_DEPTH;

	for (i = 0; i < SFQ_DEPTH; i++) {
		skb_queue_head_init(&q->qs[i]);
		q->dep[i + SFQ_DEPTH].next = i + SFQ_DEPTH;
		q->dep[i + SFQ_DEPTH].prev = i + SFQ_DEPTH;
	}

	q->limit = SFQ_DEPTH - 1;
	q->max_depth = 0;
	q->tail = SFQ_DEPTH;
	if (opt == NULL) {
		q->quantum = psched_mtu(qdisc_dev(sch));
		q->perturb_period = 0;
		q->perturbation = net_random();
	} else {
		int err = sfq_change(sch, opt);
		if (err)
			return err;
	}

	for (i = 0; i < SFQ_DEPTH; i++)
		sfq_link(q, i);
	return 0;
}

static void sfq_destroy(struct Qdisc *sch)
{
	struct sfq_sched_data *q = qdisc_priv(sch);

	tcf_destroy_chain(&q->filter_list);
	q->perturb_period = 0;
	del_timer_sync(&q->perturb_timer);
}

static int sfq_dump(struct Qdisc *sch, struct sk_buff *skb)
{
	struct sfq_sched_data *q = qdisc_priv(sch);
	unsigned char *b = skb_tail_pointer(skb);
	struct tc_sfq_qopt opt;

	opt.quantum = q->quantum;
	opt.perturb_period = q->perturb_period / HZ;

	opt.limit = q->limit;
	opt.divisor = SFQ_HASH_DIVISOR;
	opt.flows = q->limit;

	NLA_PUT(skb, TCA_OPTIONS, sizeof(opt), &opt);

	return skb->len;

nla_put_failure:
	nlmsg_trim(skb, b);
	return -1;
}

static struct Qdisc *sfq_leaf(struct Qdisc *sch, unsigned long arg)
{
	return NULL;
}

static unsigned long sfq_get(struct Qdisc *sch, u32 classid)
{
	return 0;
}

static unsigned long sfq_bind(struct Qdisc *sch, unsigned long parent,
			      u32 classid)
{
	return 0;
}

static void sfq_put(struct Qdisc *q, unsigned long cl)
{
}

static struct tcf_proto **sfq_find_tcf(struct Qdisc *sch, unsigned long cl)
{
	struct sfq_sched_data *q = qdisc_priv(sch);

	if (cl)
		return NULL;
	return &q->filter_list;
}

static int sfq_dump_class(struct Qdisc *sch, unsigned long cl,
			  struct sk_buff *skb, struct tcmsg *tcm)
{
	tcm->tcm_handle |= TC_H_MIN(cl);
	return 0;
}

static int sfq_dump_class_stats(struct Qdisc *sch, unsigned long cl,
				struct gnet_dump *d)
{
	struct sfq_sched_data *q = qdisc_priv(sch);
	sfq_index idx = q->ht[cl-1];
	struct gnet_stats_queue qs = { .qlen = q->qs[idx].qlen };
	struct tc_sfq_xstats xstats = { .allot = q->allot[idx] };

	if (gnet_stats_copy_queue(d, &qs) < 0)
		return -1;
	return gnet_stats_copy_app(d, &xstats, sizeof(xstats));
}

static void sfq_walk(struct Qdisc *sch, struct qdisc_walker *arg)
{
	struct sfq_sched_data *q = qdisc_priv(sch);
	unsigned int i;

	if (arg->stop)
		return;

	for (i = 0; i < SFQ_HASH_DIVISOR; i++) {
		if (q->ht[i] == SFQ_DEPTH ||
		    arg->count < arg->skip) {
			arg->count++;
			continue;
		}
		if (arg->fn(sch, i + 1, arg) < 0) {
			arg->stop = 1;
			break;
		}
		arg->count++;
	}
}

static const struct Qdisc_class_ops sfq_class_ops = {
	.leaf		=	sfq_leaf,
	.get		=	sfq_get,
	.put		=	sfq_put,
	.tcf_chain	=	sfq_find_tcf,
	.bind_tcf	=	sfq_bind,
	.unbind_tcf	=	sfq_put,
	.dump		=	sfq_dump_class,
	.dump_stats	=	sfq_dump_class_stats,
	.walk		=	sfq_walk,
};

static struct Qdisc_ops sfq_qdisc_ops __read_mostly = {
	.cl_ops		=	&sfq_class_ops,
	.id		=	"sfq",
	.priv_size	=	sizeof(struct sfq_sched_data),
	.enqueue	=	sfq_enqueue,
	.dequeue	=	sfq_dequeue,
	.peek		=	sfq_peek,
	.drop		=	sfq_drop,
	.init		=	sfq_init,
	.reset		=	sfq_reset,
	.destroy	=	sfq_destroy,
	.change		=	NULL,
	.dump		=	sfq_dump,
	.owner		=	THIS_MODULE,
};

static int __init sfq_module_init(void)
{
	return register_qdisc(&sfq_qdisc_ops);
}
static void __exit sfq_module_exit(void)
{
	unregister_qdisc(&sfq_qdisc_ops);
}
module_init(sfq_module_init)
module_exit(sfq_module_exit)
MODULE_LICENSE("GPL");
