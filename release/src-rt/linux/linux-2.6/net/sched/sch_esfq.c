/*
 * net/sched/sch_esfq.c	Extended Stochastic Fairness Queueing discipline.
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:	Alexey Kuznetsov, <kuznet@ms2.inr.ac.ru>
 *
 * Changes:	Alexander Atanasov, <alex@ssi.bg>
 *		Added dynamic depth,limit,divisor,hash_kind options.
 *		Added dst and src hashes.
 *
 * 		Alexander Clouter, <alex@digriz.org.uk>
 *		Ported ESFQ to Linux 2.6.
 *
 * 		Corey Hickey, <bugfood-c@fatooh.org>
 *		Maintenance of the Linux 2.6 port.
 *		Added fwmark hash (thanks to Robert Kurjata).
 *		Added usage of jhash.
 *		Added conntrack support.
 *		Added ctnatchg hash (thanks to Ben Pfountz).
 */

#include <linux/module.h>
#include <asm/uaccess.h>
#include <asm/system.h>
#include <linux/bitops.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/jiffies.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/socket.h>
#include <linux/sockios.h>
#include <linux/in.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/if_ether.h>
#include <linux/inet.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/notifier.h>
#include <linux/init.h>
#include <net/ip.h>
#include <net/netlink.h>
#include <linux/ipv6.h>
#include <net/route.h>
#include <linux/skbuff.h>
#include <net/sock.h>
#include <net/pkt_sched.h>
#include <linux/jhash.h>
#include <net/netfilter/nf_conntrack.h>

/*	Stochastic Fairness Queuing algorithm.
	For more comments look at sch_sfq.c.
	The difference is that you can change limit, depth,
	hash table size and choose alternate hash types.
	
	classic:	same as in sch_sfq.c
	dst:		destination IP address
	src:		source IP address
	fwmark:		netfilter mark value
	ctorigdst:	original destination IP address
	ctorigsrc:	original source IP address
	ctrepldst:	reply destination IP address
	ctreplsrc:	reply source IP 
	
*/

#define ESFQ_HEAD 0
#define ESFQ_TAIL 1

/* This type should contain at least SFQ_DEPTH*2 values */
typedef unsigned int esfq_index;

struct esfq_head
{
	esfq_index	next;
	esfq_index	prev;
};

struct esfq_sched_data
{
/* Parameters */
	int		perturb_period;
	unsigned	quantum;	/* Allotment per round: MUST BE >= MTU */
	int		limit;
	unsigned	depth;
	unsigned	hash_divisor;
	unsigned	hash_kind;
/* Variables */
	struct timer_list perturb_timer;
	int		perturbation;
	esfq_index	tail;		/* Index of current slot in round */
	esfq_index	max_depth;	/* Maximal depth */

	esfq_index	*ht;			/* Hash table */
	esfq_index	*next;			/* Active slots link */
	short		*allot;			/* Current allotment per slot */
	unsigned short	*hash;			/* Hash value indexed by slots */
	struct sk_buff_head	*qs;		/* Slot queue */
	struct esfq_head	*dep;		/* Linked list of slots, indexed by depth */
};

/* This contains the info we will hash. */
struct esfq_packet_info
{
	u32	proto;		/* protocol or port */
	u32	src;		/* source from packet header */
	u32	dst;		/* destination from packet header */
	u32	ctorigsrc;	/* original source from conntrack */
	u32	ctorigdst;	/* original destination from conntrack */
	u32	ctreplsrc;	/* reply source from conntrack */
	u32	ctrepldst;	/* reply destination from conntrack */
	u32	mark;		/* netfilter mark (fwmark) */
};

static __inline__ unsigned esfq_jhash_1word(struct esfq_sched_data *q,u32 a)
{
	return jhash_1word(a, q->perturbation) & (q->hash_divisor-1);
}

static __inline__ unsigned esfq_jhash_2words(struct esfq_sched_data *q, u32 a, u32 b)
{
	return jhash_2words(a, b, q->perturbation) & (q->hash_divisor-1);
}

static __inline__ unsigned esfq_jhash_3words(struct esfq_sched_data *q, u32 a, u32 b, u32 c)
{
	return jhash_3words(a, b, c, q->perturbation) & (q->hash_divisor-1);
}

static unsigned esfq_hash(struct esfq_sched_data *q, struct sk_buff *skb)
{
	struct esfq_packet_info info;
#ifdef CONFIG_NET_SCH_ESFQ_NFCT
	enum ip_conntrack_info ctinfo;
	struct nf_conn *ct = nf_ct_get(skb, &ctinfo);
#endif
	
	switch (skb->protocol) {
	case __constant_htons(ETH_P_IP):
	{
		struct iphdr *iph = ip_hdr(skb);
		info.dst = iph->daddr;
		info.src = iph->saddr;
		if (!(iph->frag_off&htons(IP_MF|IP_OFFSET)) &&
		    (iph->protocol == IPPROTO_TCP ||
		     iph->protocol == IPPROTO_UDP ||
		     iph->protocol == IPPROTO_SCTP ||
		     iph->protocol == IPPROTO_DCCP ||
		     iph->protocol == IPPROTO_ESP))
			info.proto = *(((u32*)iph) + iph->ihl);
		else
			info.proto = iph->protocol;
		break;
	}
	case __constant_htons(ETH_P_IPV6):
	{
		struct ipv6hdr *iph = ipv6_hdr(skb);
		/* Hash ipv6 addresses into a u32. This isn't ideal,
		 * but the code is simple. */
		info.dst = jhash2(iph->daddr.s6_addr32, 4, q->perturbation);
		info.src = jhash2(iph->saddr.s6_addr32, 4, q->perturbation);
		if (iph->nexthdr == IPPROTO_TCP ||
		    iph->nexthdr == IPPROTO_UDP ||
		    iph->nexthdr == IPPROTO_SCTP ||
		    iph->nexthdr == IPPROTO_DCCP ||
		    iph->nexthdr == IPPROTO_ESP)
			info.proto = *(u32*)&iph[1];
		else
			info.proto = iph->nexthdr;
		break;
	}
	default:
		info.dst   = (u32)(unsigned long)skb->dst;
		info.src   = (u32)(unsigned long)skb->sk;
		info.proto = skb->protocol;
	}

	info.mark = skb->mark;

#ifdef CONFIG_NET_SCH_ESFQ_NFCT
	/* defaults if there is no conntrack info */
	info.ctorigsrc = info.src;
	info.ctorigdst = info.dst;
	info.ctreplsrc = info.dst;
	info.ctrepldst = info.src;
	/* collect conntrack info */
	if (ct && ct != &nf_conntrack_untracked) {
		if (skb->protocol == __constant_htons(ETH_P_IP)) {
			info.ctorigsrc = ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple.src.u3.ip;
			info.ctorigdst = ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple.dst.u3.ip;
			info.ctreplsrc = ct->tuplehash[IP_CT_DIR_REPLY].tuple.src.u3.ip;
			info.ctrepldst = ct->tuplehash[IP_CT_DIR_REPLY].tuple.dst.u3.ip;
		}
		else if (skb->protocol == __constant_htons(ETH_P_IPV6)) {
			/* Again, hash ipv6 addresses into a single u32. */
			info.ctorigsrc = jhash2(ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple.src.u3.ip6, 4, q->perturbation);
			info.ctorigdst = jhash2(ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple.dst.u3.ip6, 4, q->perturbation);
			info.ctreplsrc = jhash2(ct->tuplehash[IP_CT_DIR_REPLY].tuple.src.u3.ip6, 4, q->perturbation);
			info.ctrepldst = jhash2(ct->tuplehash[IP_CT_DIR_REPLY].tuple.dst.u3.ip6, 4, q->perturbation);
		}

	}
#endif

	switch(q->hash_kind) {
	case TCA_SFQ_HASH_CLASSIC:
		return esfq_jhash_3words(q, info.dst, info.src, info.proto);
	case TCA_SFQ_HASH_DST:
		return esfq_jhash_1word(q, info.dst);
	case TCA_SFQ_HASH_SRC:
		return esfq_jhash_1word(q, info.src);
	case TCA_SFQ_HASH_FWMARK:
		return esfq_jhash_1word(q, info.mark);
#ifdef CONFIG_NET_SCH_ESFQ_NFCT
	case TCA_SFQ_HASH_CTORIGDST:
		return esfq_jhash_1word(q, info.ctorigdst);
	case TCA_SFQ_HASH_CTORIGSRC:
		return esfq_jhash_1word(q, info.ctorigsrc);
	case TCA_SFQ_HASH_CTREPLDST:
		return esfq_jhash_1word(q, info.ctrepldst);
	case TCA_SFQ_HASH_CTREPLSRC:
		return esfq_jhash_1word(q, info.ctreplsrc);
	case TCA_SFQ_HASH_CTNATCHG:
	{
		if (info.ctorigdst == info.ctreplsrc)
			return esfq_jhash_1word(q, info.ctorigsrc);
		return esfq_jhash_1word(q, info.ctreplsrc);
	}
#endif
	default:
		if (net_ratelimit())
			printk(KERN_WARNING "ESFQ: Unknown hash method. Falling back to classic.\n");
	}
	return esfq_jhash_3words(q, info.dst, info.src, info.proto);
}

static inline void esfq_link(struct esfq_sched_data *q, esfq_index x)
{
	esfq_index p, n;
	int d = q->qs[x].qlen + q->depth;

	p = d;
	n = q->dep[d].next;
	q->dep[x].next = n;
	q->dep[x].prev = p;
	q->dep[p].next = q->dep[n].prev = x;
}

static inline void esfq_dec(struct esfq_sched_data *q, esfq_index x)
{
	esfq_index p, n;

	n = q->dep[x].next;
	p = q->dep[x].prev;
	q->dep[p].next = n;
	q->dep[n].prev = p;

	if (n == p && q->max_depth == q->qs[x].qlen + 1)
		q->max_depth--;

	esfq_link(q, x);
}

static inline void esfq_inc(struct esfq_sched_data *q, esfq_index x)
{
	esfq_index p, n;
	int d;

	n = q->dep[x].next;
	p = q->dep[x].prev;
	q->dep[p].next = n;
	q->dep[n].prev = p;
	d = q->qs[x].qlen;
	if (q->max_depth < d)
		q->max_depth = d;

	esfq_link(q, x);
}

static unsigned int esfq_drop(struct Qdisc *sch)
{
	struct esfq_sched_data *q = qdisc_priv(sch);
	esfq_index d = q->max_depth;
	struct sk_buff *skb;
	unsigned int len;

	/* Queue is full! Find the longest slot and
	   drop a packet from it */

	if (d > 1) {
		esfq_index x = q->dep[d+q->depth].next;
		skb = q->qs[x].prev;
		len = skb->len;
		__skb_unlink(skb, &q->qs[x]);
		kfree_skb(skb);
		esfq_dec(q, x);
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
		len = skb->len;
		__skb_unlink(skb, &q->qs[d]);
		kfree_skb(skb);
		esfq_dec(q, d);
		sch->q.qlen--;
		q->ht[q->hash[d]] = q->depth;
		sch->qstats.drops++;
		sch->qstats.backlog -= len;
		return len;
	}

	return 0;
}

static void esfq_q_enqueue(struct sk_buff *skb, struct esfq_sched_data *q, unsigned int end)
{
	unsigned hash = esfq_hash(q, skb);
	unsigned depth = q->depth;
	esfq_index x;

	x = q->ht[hash];
	if (x == depth) {
		q->ht[hash] = x = q->dep[depth].next;
		q->hash[x] = hash;
	}

	if (end == ESFQ_TAIL)
		__skb_queue_tail(&q->qs[x], skb);
	else
		__skb_queue_head(&q->qs[x], skb);

	esfq_inc(q, x);
	if (q->qs[x].qlen == 1) {		/* The flow is new */
		if (q->tail == depth) {	/* It is the first flow */
			q->tail = x;
			q->next[x] = x;
			q->allot[x] = q->quantum;
		} else {
			q->next[x] = q->next[q->tail];
			q->next[q->tail] = x;
			q->tail = x;
		}
	}
}

static int esfq_enqueue(struct sk_buff *skb, struct Qdisc* sch)
{
	struct esfq_sched_data *q = qdisc_priv(sch);
	esfq_q_enqueue(skb, q, ESFQ_TAIL);
	sch->qstats.backlog += skb->len;
	if (++sch->q.qlen < q->limit-1) {
		sch->bstats.bytes += skb->len;
		sch->bstats.packets++;
		return 0;
	}

	sch->qstats.drops++;
	esfq_drop(sch);
	return NET_XMIT_CN;
}


static int esfq_requeue(struct sk_buff *skb, struct Qdisc* sch)
{
	struct esfq_sched_data *q = qdisc_priv(sch);
	esfq_q_enqueue(skb, q, ESFQ_HEAD);
	sch->qstats.backlog += skb->len;
	if (++sch->q.qlen < q->limit - 1) {
		sch->qstats.requeues++;
		return 0;
	}

	sch->qstats.drops++;
	esfq_drop(sch);
	return NET_XMIT_CN;
}

static struct sk_buff *esfq_q_dequeue(struct esfq_sched_data *q)
{
	struct sk_buff *skb;
	unsigned depth = q->depth;
	esfq_index a, old_a;

	/* No active slots */
	if (q->tail == depth)
		return NULL;
	
	a = old_a = q->next[q->tail];
	
	/* Grab packet */
	skb = __skb_dequeue(&q->qs[a]);
	esfq_dec(q, a);
	
	/* Is the slot empty? */
	if (q->qs[a].qlen == 0) {
		q->ht[q->hash[a]] = depth;
		a = q->next[a];
		if (a == old_a) {
			q->tail = depth;
			return skb;
		}
		q->next[q->tail] = a;
		q->allot[a] += q->quantum;
	} else if ((q->allot[a] -= skb->len) <= 0) {
		q->tail = a;
		a = q->next[a];
		q->allot[a] += q->quantum;
	}
	
	return skb;
}

static struct sk_buff *esfq_dequeue(struct Qdisc* sch)
{
	struct esfq_sched_data *q = qdisc_priv(sch);
	struct sk_buff *skb;

	skb = esfq_q_dequeue(q);
	if (skb == NULL)
		return NULL;
	sch->q.qlen--;
	sch->qstats.backlog -= skb->len;
	return skb;
}

static void esfq_q_destroy(struct esfq_sched_data *q)
{
	del_timer(&q->perturb_timer);
	if(q->ht)
		kfree(q->ht);
	if(q->dep)
		kfree(q->dep);
	if(q->next)
		kfree(q->next);
	if(q->allot)
		kfree(q->allot);
	if(q->hash)
		kfree(q->hash);
	if(q->qs)
		kfree(q->qs);
}

static void esfq_destroy(struct Qdisc *sch)
{
	struct esfq_sched_data *q = qdisc_priv(sch);
	esfq_q_destroy(q);
}


static void esfq_reset(struct Qdisc* sch)
{
	struct sk_buff *skb;

	while ((skb = esfq_dequeue(sch)) != NULL)
		kfree_skb(skb);
}

static void esfq_perturbation(unsigned long arg)
{
	struct Qdisc *sch = (struct Qdisc*)arg;
	struct esfq_sched_data *q = qdisc_priv(sch);

	q->perturbation = net_random()&0x1F;

	if (q->perturb_period) {
		q->perturb_timer.expires = jiffies + q->perturb_period;
		add_timer(&q->perturb_timer);
	}
}

static unsigned int esfq_check_hash(unsigned int kind)
{
	switch (kind) {
	case TCA_SFQ_HASH_CTORIGDST:
	case TCA_SFQ_HASH_CTORIGSRC:
	case TCA_SFQ_HASH_CTREPLDST:
	case TCA_SFQ_HASH_CTREPLSRC:
	case TCA_SFQ_HASH_CTNATCHG:
#ifndef CONFIG_NET_SCH_ESFQ_NFCT
	{
		if (net_ratelimit())
			printk(KERN_WARNING "ESFQ: Conntrack hash types disabled in kernel config. Falling back to classic.\n");
		return TCA_SFQ_HASH_CLASSIC;
	}
#endif
	case TCA_SFQ_HASH_CLASSIC:
	case TCA_SFQ_HASH_DST:
	case TCA_SFQ_HASH_SRC:
	case TCA_SFQ_HASH_FWMARK:
		return kind;
	default:
	{
		if (net_ratelimit())
			printk(KERN_WARNING "ESFQ: Unknown hash type. Falling back to classic.\n");
		return TCA_SFQ_HASH_CLASSIC;
	}
	}
}
	
static int esfq_q_init(struct esfq_sched_data *q, struct rtattr *opt)
{
	struct tc_esfq_qopt *ctl = RTA_DATA(opt);
	esfq_index p = ~0U/2;
	int i;
	
	if (opt && opt->rta_len < RTA_LENGTH(sizeof(*ctl)))
		return -EINVAL;

	q->perturbation = 0;
	q->hash_kind = TCA_SFQ_HASH_CLASSIC;
	q->max_depth = 0;
	if (opt == NULL) {
		q->perturb_period = 0;
		q->hash_divisor = 1024;
		q->tail = q->limit = q->depth = 128;
		
	} else {
		struct tc_esfq_qopt *ctl = RTA_DATA(opt);
		if (ctl->quantum)
			q->quantum = ctl->quantum;
		q->perturb_period = ctl->perturb_period*HZ;
		q->hash_divisor = ctl->divisor ? : 1024;
		q->tail = q->limit = q->depth = ctl->flows ? : 128;
		
		if ( q->depth > p - 1 )
			return -EINVAL;
		
		if (ctl->limit)
			q->limit = min_t(u32, ctl->limit, q->depth);
		
		if (ctl->hash_kind) {
			q->hash_kind = esfq_check_hash(ctl->hash_kind);
		}
	}
	
	q->ht = kmalloc(q->hash_divisor*sizeof(esfq_index), GFP_KERNEL);
	if (!q->ht)
		goto err_case;
	q->dep = kmalloc((1+q->depth*2)*sizeof(struct esfq_head), GFP_KERNEL);
	if (!q->dep)
		goto err_case;
	q->next = kmalloc(q->depth*sizeof(esfq_index), GFP_KERNEL);
	if (!q->next)
		goto err_case;
	q->allot = kmalloc(q->depth*sizeof(short), GFP_KERNEL);
	if (!q->allot)
		goto err_case;
	q->hash = kmalloc(q->depth*sizeof(unsigned short), GFP_KERNEL);
	if (!q->hash)
		goto err_case;
	q->qs = kmalloc(q->depth*sizeof(struct sk_buff_head), GFP_KERNEL);
	if (!q->qs)
		goto err_case;
	
	for (i=0; i< q->hash_divisor; i++)
		q->ht[i] = q->depth;
	for (i=0; i<q->depth; i++) {
		skb_queue_head_init(&q->qs[i]);
		q->dep[i+q->depth].next = i+q->depth;
		q->dep[i+q->depth].prev = i+q->depth;
	}
	
	for (i=0; i<q->depth; i++)
		esfq_link(q, i);
	return 0;
err_case:
	esfq_q_destroy(q);
	return -ENOBUFS;
}

static int esfq_init(struct Qdisc *sch, struct rtattr *opt)
{
	struct esfq_sched_data *q = qdisc_priv(sch);
	int err;
	
	q->quantum = psched_mtu(sch->dev); /* default */
	if ((err = esfq_q_init(q, opt)))
		return err;

	init_timer(&q->perturb_timer);
	q->perturb_timer.data = (unsigned long)sch;
	q->perturb_timer.function = esfq_perturbation;
	if (q->perturb_period) {
		q->perturb_timer.expires = jiffies + q->perturb_period;
		add_timer(&q->perturb_timer);
	}
	
	return 0;
}

static int esfq_change(struct Qdisc *sch, struct rtattr *opt)
{
	struct esfq_sched_data *q = qdisc_priv(sch);
	struct esfq_sched_data new;
	struct sk_buff *skb;
	int err;
	
	/* set up new queue */
	memset(&new, 0, sizeof(struct esfq_sched_data));
	new.quantum = psched_mtu(sch->dev); /* default */
	if ((err = esfq_q_init(&new, opt)))
		return err;

	/* copy all packets from the old queue to the new queue */
	sch_tree_lock(sch);
	while ((skb = esfq_q_dequeue(q)) != NULL)
		esfq_q_enqueue(skb, &new, ESFQ_TAIL);
	
	/* clean up the old queue */
	esfq_q_destroy(q);

	/* copy elements of the new queue into the old queue */
	q->perturb_period = new.perturb_period;
	q->quantum        = new.quantum;
	q->limit          = new.limit;
	q->depth          = new.depth;
	q->hash_divisor   = new.hash_divisor;
	q->hash_kind      = new.hash_kind;
	q->tail           = new.tail;
	q->max_depth      = new.max_depth;
	q->ht    = new.ht;
	q->dep   = new.dep;
	q->next  = new.next;
	q->allot = new.allot;
	q->hash  = new.hash;
	q->qs    = new.qs;

	/* finish up */
	if (q->perturb_period) {
		q->perturb_timer.expires = jiffies + q->perturb_period;
		add_timer(&q->perturb_timer);
	} else {
		q->perturbation = 0;
	}
	sch_tree_unlock(sch);
	return 0;
}

static int esfq_dump(struct Qdisc *sch, struct sk_buff *skb)
{
	struct esfq_sched_data *q = qdisc_priv(sch);
	unsigned char *b = skb_tail_pointer(skb);
	struct tc_esfq_qopt opt;

	opt.quantum = q->quantum;
	opt.perturb_period = q->perturb_period/HZ;

	opt.limit = q->limit;
	opt.divisor = q->hash_divisor;
	opt.flows = q->depth;
	opt.hash_kind = q->hash_kind;

	RTA_PUT(skb, TCA_OPTIONS, sizeof(opt), &opt);

	return skb->len;

rtattr_failure:
	nlmsg_trim(skb, b);
	return -1;
}

static struct Qdisc_ops esfq_qdisc_ops =
{
	.next		=	NULL,
	.cl_ops		=	NULL,
	.id		=	"esfq",
	.priv_size	=	sizeof(struct esfq_sched_data),
	.enqueue	=	esfq_enqueue,
	.dequeue	=	esfq_dequeue,
	.requeue	=	esfq_requeue,
	.drop		=	esfq_drop,
	.init		=	esfq_init,
	.reset		=	esfq_reset,
	.destroy	=	esfq_destroy,
	.change		=	esfq_change,
	.dump		=	esfq_dump,
	.owner		=	THIS_MODULE,
};

static int __init esfq_module_init(void)
{
	return register_qdisc(&esfq_qdisc_ops);
}
static void __exit esfq_module_exit(void) 
{
	unregister_qdisc(&esfq_qdisc_ops);
}
module_init(esfq_module_init)
module_exit(esfq_module_exit)
MODULE_LICENSE("GPL");
