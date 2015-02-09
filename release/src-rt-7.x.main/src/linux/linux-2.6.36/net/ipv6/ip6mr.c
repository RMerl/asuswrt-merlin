/*
 *	Linux IPv6 multicast routing support for BSD pim6sd
 *	Based on net/ipv4/ipmr.c.
 *
 *	(c) 2004 Mickael Hoerdt, <hoerdt@clarinet.u-strasbg.fr>
 *		LSIIT Laboratory, Strasbourg, France
 *	(c) 2004 Jean-Philippe Andriot, <jean-philippe.andriot@6WIND.com>
 *		6WIND, Paris, France
 *	Copyright (C)2007,2008 USAGI/WIDE Project
 *		YOSHIFUJI Hideaki <yoshfuji@linux-ipv6.org>
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 *
 */

#include <asm/system.h>
#include <asm/uaccess.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/timer.h>
#include <linux/mm.h>
#include <linux/kernel.h>
#include <linux/fcntl.h>
#include <linux/stat.h>
#include <linux/socket.h>
#include <linux/inet.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <net/protocol.h>
#include <linux/skbuff.h>
#include <net/sock.h>
#include <net/raw.h>
#include <linux/notifier.h>
#include <linux/if_arp.h>
#include <net/checksum.h>
#include <net/netlink.h>
#include <net/fib_rules.h>

#include <net/ipv6.h>
#include <net/ip6_route.h>
#include <linux/mroute6.h>
#include <linux/pim.h>
#include <net/addrconf.h>
#include <linux/netfilter_ipv6.h>
#include <net/ip6_checksum.h>

struct mr6_table {
	struct list_head	list;
#ifdef CONFIG_NET_NS
	struct net		*net;
#endif
	u32			id;
	struct sock		*mroute6_sk;
	struct timer_list	ipmr_expire_timer;
	struct list_head	mfc6_unres_queue;
	struct list_head	mfc6_cache_array[MFC6_LINES];
	struct mif_device	vif6_table[MAXMIFS];
	int			maxvif;
	atomic_t		cache_resolve_queue_len;
	int			mroute_do_assert;
	int			mroute_do_pim;
#ifdef CONFIG_IPV6_PIMSM_V2
	int			mroute_reg_vif_num;
#endif
};

struct ip6mr_rule {
	struct fib_rule		common;
};

struct ip6mr_result {
	struct mr6_table	*mrt;
};

/* Big lock, protecting vif table, mrt cache and mroute socket state.
   Note that the changes are semaphored via rtnl_lock.
 */

static DEFINE_RWLOCK(mrt_lock);

/*
 *	Multicast router control variables
 */

#define MIF_EXISTS(_mrt, _idx) ((_mrt)->vif6_table[_idx].dev != NULL)

/* Special spinlock for queue of unresolved entries */
static DEFINE_SPINLOCK(mfc_unres_lock);

/* We return to original Alan's scheme. Hash table of resolved
   entries is changed only in process context and protected
   with weak lock mrt_lock. Queue of unresolved entries is protected
   with strong spinlock mfc_unres_lock.

   In this case data path is free of exclusive locks at all.
 */

static struct kmem_cache *mrt_cachep __read_mostly;

static struct mr6_table *ip6mr_new_table(struct net *net, u32 id);
static void ip6mr_free_table(struct mr6_table *mrt);

static int ip6_mr_forward(struct net *net, struct mr6_table *mrt,
			  struct sk_buff *skb, struct mfc6_cache *cache);
static int ip6mr_cache_report(struct mr6_table *mrt, struct sk_buff *pkt,
			      mifi_t mifi, int assert);
static int __ip6mr_fill_mroute(struct mr6_table *mrt, struct sk_buff *skb,
			       struct mfc6_cache *c, struct rtmsg *rtm);
static int ip6mr_rtm_dumproute(struct sk_buff *skb,
			       struct netlink_callback *cb);
static void mroute_clean_tables(struct mr6_table *mrt);
static void ipmr_expire_process(unsigned long arg);

#ifdef CONFIG_IPV6_MROUTE_MULTIPLE_TABLES
#define ip6mr_for_each_table(mrt, net) \
	list_for_each_entry_rcu(mrt, &net->ipv6.mr6_tables, list)

static struct mr6_table *ip6mr_get_table(struct net *net, u32 id)
{
	struct mr6_table *mrt;

	ip6mr_for_each_table(mrt, net) {
		if (mrt->id == id)
			return mrt;
	}
	return NULL;
}

static int ip6mr_fib_lookup(struct net *net, struct flowi *flp,
			    struct mr6_table **mrt)
{
	struct ip6mr_result res;
	struct fib_lookup_arg arg = { .result = &res, };
	int err;

	err = fib_rules_lookup(net->ipv6.mr6_rules_ops, flp, 0, &arg);
	if (err < 0)
		return err;
	*mrt = res.mrt;
	return 0;
}

static int ip6mr_rule_action(struct fib_rule *rule, struct flowi *flp,
			     int flags, struct fib_lookup_arg *arg)
{
	struct ip6mr_result *res = arg->result;
	struct mr6_table *mrt;

	switch (rule->action) {
	case FR_ACT_TO_TBL:
		break;
	case FR_ACT_UNREACHABLE:
		return -ENETUNREACH;
	case FR_ACT_PROHIBIT:
		return -EACCES;
	case FR_ACT_BLACKHOLE:
	default:
		return -EINVAL;
	}

	mrt = ip6mr_get_table(rule->fr_net, rule->table);
	if (mrt == NULL)
		return -EAGAIN;
	res->mrt = mrt;
	return 0;
}

static int ip6mr_rule_match(struct fib_rule *rule, struct flowi *flp, int flags)
{
	return 1;
}

static const struct nla_policy ip6mr_rule_policy[FRA_MAX + 1] = {
	FRA_GENERIC_POLICY,
};

static int ip6mr_rule_configure(struct fib_rule *rule, struct sk_buff *skb,
				struct fib_rule_hdr *frh, struct nlattr **tb)
{
	return 0;
}

static int ip6mr_rule_compare(struct fib_rule *rule, struct fib_rule_hdr *frh,
			      struct nlattr **tb)
{
	return 1;
}

static int ip6mr_rule_fill(struct fib_rule *rule, struct sk_buff *skb,
			   struct fib_rule_hdr *frh)
{
	frh->dst_len = 0;
	frh->src_len = 0;
	frh->tos     = 0;
	return 0;
}

static const struct fib_rules_ops __net_initdata ip6mr_rules_ops_template = {
	.family		= RTNL_FAMILY_IP6MR,
	.rule_size	= sizeof(struct ip6mr_rule),
	.addr_size	= sizeof(struct in6_addr),
	.action		= ip6mr_rule_action,
	.match		= ip6mr_rule_match,
	.configure	= ip6mr_rule_configure,
	.compare	= ip6mr_rule_compare,
	.default_pref	= fib_default_rule_pref,
	.fill		= ip6mr_rule_fill,
	.nlgroup	= RTNLGRP_IPV6_RULE,
	.policy		= ip6mr_rule_policy,
	.owner		= THIS_MODULE,
};

static int __net_init ip6mr_rules_init(struct net *net)
{
	struct fib_rules_ops *ops;
	struct mr6_table *mrt;
	int err;

	ops = fib_rules_register(&ip6mr_rules_ops_template, net);
	if (IS_ERR(ops))
		return PTR_ERR(ops);

	INIT_LIST_HEAD(&net->ipv6.mr6_tables);

	mrt = ip6mr_new_table(net, RT6_TABLE_DFLT);
	if (mrt == NULL) {
		err = -ENOMEM;
		goto err1;
	}

	err = fib_default_rule_add(ops, 0x7fff, RT6_TABLE_DFLT, 0);
	if (err < 0)
		goto err2;

	net->ipv6.mr6_rules_ops = ops;
	return 0;

err2:
	kfree(mrt);
err1:
	fib_rules_unregister(ops);
	return err;
}

static void __net_exit ip6mr_rules_exit(struct net *net)
{
	struct mr6_table *mrt, *next;

	list_for_each_entry_safe(mrt, next, &net->ipv6.mr6_tables, list) {
		list_del(&mrt->list);
		ip6mr_free_table(mrt);
	}
	fib_rules_unregister(net->ipv6.mr6_rules_ops);
}
#else
#define ip6mr_for_each_table(mrt, net) \
	for (mrt = net->ipv6.mrt6; mrt; mrt = NULL)

static struct mr6_table *ip6mr_get_table(struct net *net, u32 id)
{
	return net->ipv6.mrt6;
}

static int ip6mr_fib_lookup(struct net *net, struct flowi *flp,
			    struct mr6_table **mrt)
{
	*mrt = net->ipv6.mrt6;
	return 0;
}

static int __net_init ip6mr_rules_init(struct net *net)
{
	net->ipv6.mrt6 = ip6mr_new_table(net, RT6_TABLE_DFLT);
	return net->ipv6.mrt6 ? 0 : -ENOMEM;
}

static void __net_exit ip6mr_rules_exit(struct net *net)
{
	ip6mr_free_table(net->ipv6.mrt6);
}
#endif

static struct mr6_table *ip6mr_new_table(struct net *net, u32 id)
{
	struct mr6_table *mrt;
	unsigned int i;

	mrt = ip6mr_get_table(net, id);
	if (mrt != NULL)
		return mrt;

	mrt = kzalloc(sizeof(*mrt), GFP_KERNEL);
	if (mrt == NULL)
		return NULL;
	mrt->id = id;
	write_pnet(&mrt->net, net);

	/* Forwarding cache */
	for (i = 0; i < MFC6_LINES; i++)
		INIT_LIST_HEAD(&mrt->mfc6_cache_array[i]);

	INIT_LIST_HEAD(&mrt->mfc6_unres_queue);

	setup_timer(&mrt->ipmr_expire_timer, ipmr_expire_process,
		    (unsigned long)mrt);

#ifdef CONFIG_IPV6_PIMSM_V2
	mrt->mroute_reg_vif_num = -1;
#endif
#ifdef CONFIG_IPV6_MROUTE_MULTIPLE_TABLES
	list_add_tail_rcu(&mrt->list, &net->ipv6.mr6_tables);
#endif
	return mrt;
}

static void ip6mr_free_table(struct mr6_table *mrt)
{
	del_timer(&mrt->ipmr_expire_timer);
	mroute_clean_tables(mrt);
	kfree(mrt);
}

#ifdef CONFIG_PROC_FS

struct ipmr_mfc_iter {
	struct seq_net_private p;
	struct mr6_table *mrt;
	struct list_head *cache;
	int ct;
};


static struct mfc6_cache *ipmr_mfc_seq_idx(struct net *net,
					   struct ipmr_mfc_iter *it, loff_t pos)
{
	struct mr6_table *mrt = it->mrt;
	struct mfc6_cache *mfc;

	read_lock(&mrt_lock);
	for (it->ct = 0; it->ct < MFC6_LINES; it->ct++) {
		it->cache = &mrt->mfc6_cache_array[it->ct];
		list_for_each_entry(mfc, it->cache, list)
			if (pos-- == 0)
				return mfc;
	}
	read_unlock(&mrt_lock);

	spin_lock_bh(&mfc_unres_lock);
	it->cache = &mrt->mfc6_unres_queue;
	list_for_each_entry(mfc, it->cache, list)
		if (pos-- == 0)
			return mfc;
	spin_unlock_bh(&mfc_unres_lock);

	it->cache = NULL;
	return NULL;
}

/*
 *	The /proc interfaces to multicast routing /proc/ip6_mr_cache /proc/ip6_mr_vif
 */

struct ipmr_vif_iter {
	struct seq_net_private p;
	struct mr6_table *mrt;
	int ct;
};

static struct mif_device *ip6mr_vif_seq_idx(struct net *net,
					    struct ipmr_vif_iter *iter,
					    loff_t pos)
{
	struct mr6_table *mrt = iter->mrt;

	for (iter->ct = 0; iter->ct < mrt->maxvif; ++iter->ct) {
		if (!MIF_EXISTS(mrt, iter->ct))
			continue;
		if (pos-- == 0)
			return &mrt->vif6_table[iter->ct];
	}
	return NULL;
}

static void *ip6mr_vif_seq_start(struct seq_file *seq, loff_t *pos)
	__acquires(mrt_lock)
{
	struct ipmr_vif_iter *iter = seq->private;
	struct net *net = seq_file_net(seq);
	struct mr6_table *mrt;

	mrt = ip6mr_get_table(net, RT6_TABLE_DFLT);
	if (mrt == NULL)
		return ERR_PTR(-ENOENT);

	iter->mrt = mrt;

	read_lock(&mrt_lock);
	return *pos ? ip6mr_vif_seq_idx(net, seq->private, *pos - 1)
		: SEQ_START_TOKEN;
}

static void *ip6mr_vif_seq_next(struct seq_file *seq, void *v, loff_t *pos)
{
	struct ipmr_vif_iter *iter = seq->private;
	struct net *net = seq_file_net(seq);
	struct mr6_table *mrt = iter->mrt;

	++*pos;
	if (v == SEQ_START_TOKEN)
		return ip6mr_vif_seq_idx(net, iter, 0);

	while (++iter->ct < mrt->maxvif) {
		if (!MIF_EXISTS(mrt, iter->ct))
			continue;
		return &mrt->vif6_table[iter->ct];
	}
	return NULL;
}

static void ip6mr_vif_seq_stop(struct seq_file *seq, void *v)
	__releases(mrt_lock)
{
	read_unlock(&mrt_lock);
}

static int ip6mr_vif_seq_show(struct seq_file *seq, void *v)
{
	struct ipmr_vif_iter *iter = seq->private;
	struct mr6_table *mrt = iter->mrt;

	if (v == SEQ_START_TOKEN) {
		seq_puts(seq,
			 "Interface      BytesIn  PktsIn  BytesOut PktsOut Flags\n");
	} else {
		const struct mif_device *vif = v;
		const char *name = vif->dev ? vif->dev->name : "none";

		seq_printf(seq,
			   "%2td %-10s %8ld %7ld  %8ld %7ld %05X\n",
			   vif - mrt->vif6_table,
			   name, vif->bytes_in, vif->pkt_in,
			   vif->bytes_out, vif->pkt_out,
			   vif->flags);
	}
	return 0;
}

static const struct seq_operations ip6mr_vif_seq_ops = {
	.start = ip6mr_vif_seq_start,
	.next  = ip6mr_vif_seq_next,
	.stop  = ip6mr_vif_seq_stop,
	.show  = ip6mr_vif_seq_show,
};

static int ip6mr_vif_open(struct inode *inode, struct file *file)
{
	return seq_open_net(inode, file, &ip6mr_vif_seq_ops,
			    sizeof(struct ipmr_vif_iter));
}

static const struct file_operations ip6mr_vif_fops = {
	.owner	 = THIS_MODULE,
	.open    = ip6mr_vif_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = seq_release_net,
};

static void *ipmr_mfc_seq_start(struct seq_file *seq, loff_t *pos)
{
	struct ipmr_mfc_iter *it = seq->private;
	struct net *net = seq_file_net(seq);
	struct mr6_table *mrt;

	mrt = ip6mr_get_table(net, RT6_TABLE_DFLT);
	if (mrt == NULL)
		return ERR_PTR(-ENOENT);

	it->mrt = mrt;
	return *pos ? ipmr_mfc_seq_idx(net, seq->private, *pos - 1)
		: SEQ_START_TOKEN;
}

static void *ipmr_mfc_seq_next(struct seq_file *seq, void *v, loff_t *pos)
{
	struct mfc6_cache *mfc = v;
	struct ipmr_mfc_iter *it = seq->private;
	struct net *net = seq_file_net(seq);
	struct mr6_table *mrt = it->mrt;

	++*pos;

	if (v == SEQ_START_TOKEN)
		return ipmr_mfc_seq_idx(net, seq->private, 0);

	if (mfc->list.next != it->cache)
		return list_entry(mfc->list.next, struct mfc6_cache, list);

	if (it->cache == &mrt->mfc6_unres_queue)
		goto end_of_list;

	BUG_ON(it->cache != &mrt->mfc6_cache_array[it->ct]);

	while (++it->ct < MFC6_LINES) {
		it->cache = &mrt->mfc6_cache_array[it->ct];
		if (list_empty(it->cache))
			continue;
		return list_first_entry(it->cache, struct mfc6_cache, list);
	}

	/* exhausted cache_array, show unresolved */
	read_unlock(&mrt_lock);
	it->cache = &mrt->mfc6_unres_queue;
	it->ct = 0;

	spin_lock_bh(&mfc_unres_lock);
	if (!list_empty(it->cache))
		return list_first_entry(it->cache, struct mfc6_cache, list);

 end_of_list:
	spin_unlock_bh(&mfc_unres_lock);
	it->cache = NULL;

	return NULL;
}

static void ipmr_mfc_seq_stop(struct seq_file *seq, void *v)
{
	struct ipmr_mfc_iter *it = seq->private;
	struct mr6_table *mrt = it->mrt;

	if (it->cache == &mrt->mfc6_unres_queue)
		spin_unlock_bh(&mfc_unres_lock);
	else if (it->cache == mrt->mfc6_cache_array)
		read_unlock(&mrt_lock);
}

static int ipmr_mfc_seq_show(struct seq_file *seq, void *v)
{
	int n;

	if (v == SEQ_START_TOKEN) {
		seq_puts(seq,
			 "Group                            "
			 "Origin                           "
			 "Iif      Pkts  Bytes     Wrong  Oifs\n");
	} else {
		const struct mfc6_cache *mfc = v;
		const struct ipmr_mfc_iter *it = seq->private;
		struct mr6_table *mrt = it->mrt;

		seq_printf(seq, "%pI6 %pI6 %-3hd",
			   &mfc->mf6c_mcastgrp, &mfc->mf6c_origin,
			   mfc->mf6c_parent);

		if (it->cache != &mrt->mfc6_unres_queue) {
			seq_printf(seq, " %8lu %8lu %8lu",
				   mfc->mfc_un.res.pkt,
				   mfc->mfc_un.res.bytes,
				   mfc->mfc_un.res.wrong_if);
			for (n = mfc->mfc_un.res.minvif;
			     n < mfc->mfc_un.res.maxvif; n++) {
				if (MIF_EXISTS(mrt, n) &&
				    mfc->mfc_un.res.ttls[n] < 255)
					seq_printf(seq,
						   " %2d:%-3d",
						   n, mfc->mfc_un.res.ttls[n]);
			}
		} else {
			/* unresolved mfc_caches don't contain
			 * pkt, bytes and wrong_if values
			 */
			seq_printf(seq, " %8lu %8lu %8lu", 0ul, 0ul, 0ul);
		}
		seq_putc(seq, '\n');
	}
	return 0;
}

static const struct seq_operations ipmr_mfc_seq_ops = {
	.start = ipmr_mfc_seq_start,
	.next  = ipmr_mfc_seq_next,
	.stop  = ipmr_mfc_seq_stop,
	.show  = ipmr_mfc_seq_show,
};

static int ipmr_mfc_open(struct inode *inode, struct file *file)
{
	return seq_open_net(inode, file, &ipmr_mfc_seq_ops,
			    sizeof(struct ipmr_mfc_iter));
}

static const struct file_operations ip6mr_mfc_fops = {
	.owner	 = THIS_MODULE,
	.open    = ipmr_mfc_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = seq_release_net,
};
#endif

#ifdef CONFIG_IPV6_PIMSM_V2

static int pim6_rcv(struct sk_buff *skb)
{
	struct pimreghdr *pim;
	struct ipv6hdr   *encap;
	struct net_device  *reg_dev = NULL;
	struct net *net = dev_net(skb->dev);
	struct mr6_table *mrt;
	struct flowi fl = {
		.iif	= skb->dev->ifindex,
		.mark	= skb->mark,
	};
	int reg_vif_num;

	if (!pskb_may_pull(skb, sizeof(*pim) + sizeof(*encap)))
		goto drop;

	pim = (struct pimreghdr *)skb_transport_header(skb);
	if (pim->type != ((PIM_VERSION << 4) | PIM_REGISTER) ||
	    (pim->flags & PIM_NULL_REGISTER) ||
	    (csum_ipv6_magic(&ipv6_hdr(skb)->saddr, &ipv6_hdr(skb)->daddr,
			     sizeof(*pim), IPPROTO_PIM,
			     csum_partial((void *)pim, sizeof(*pim), 0)) &&
	     csum_fold(skb_checksum(skb, 0, skb->len, 0))))
		goto drop;

	/* check if the inner packet is destined to mcast group */
	encap = (struct ipv6hdr *)(skb_transport_header(skb) +
				   sizeof(*pim));

	if (!ipv6_addr_is_multicast(&encap->daddr) ||
	    encap->payload_len == 0 ||
	    ntohs(encap->payload_len) + sizeof(*pim) > skb->len)
		goto drop;

	if (ip6mr_fib_lookup(net, &fl, &mrt) < 0)
		goto drop;
	reg_vif_num = mrt->mroute_reg_vif_num;

	read_lock(&mrt_lock);
	if (reg_vif_num >= 0)
		reg_dev = mrt->vif6_table[reg_vif_num].dev;
	if (reg_dev)
		dev_hold(reg_dev);
	read_unlock(&mrt_lock);

	if (reg_dev == NULL)
		goto drop;

	skb->mac_header = skb->network_header;
	skb_pull(skb, (u8 *)encap - skb->data);
	skb_reset_network_header(skb);
	skb->protocol = htons(ETH_P_IPV6);
	skb->ip_summed = 0;
	skb->pkt_type = PACKET_HOST;

	skb_tunnel_rx(skb, reg_dev);

	netif_rx(skb);
	dev_put(reg_dev);
	return 0;
 drop:
	kfree_skb(skb);
	return 0;
}

static const struct inet6_protocol pim6_protocol = {
	.handler	=	pim6_rcv,
};

/* Service routines creating virtual interfaces: PIMREG */

static netdev_tx_t reg_vif_xmit(struct sk_buff *skb,
				      struct net_device *dev)
{
	struct net *net = dev_net(dev);
	struct mr6_table *mrt;
	struct flowi fl = {
		.oif		= dev->ifindex,
		.iif		= skb->skb_iif,
		.mark		= skb->mark,
	};
	int err;

	err = ip6mr_fib_lookup(net, &fl, &mrt);
	if (err < 0)
		return err;

	read_lock(&mrt_lock);
	dev->stats.tx_bytes += skb->len;
	dev->stats.tx_packets++;
	ip6mr_cache_report(mrt, skb, mrt->mroute_reg_vif_num, MRT6MSG_WHOLEPKT);
	read_unlock(&mrt_lock);
	kfree_skb(skb);
	return NETDEV_TX_OK;
}

static const struct net_device_ops reg_vif_netdev_ops = {
	.ndo_start_xmit	= reg_vif_xmit,
};

static void reg_vif_setup(struct net_device *dev)
{
	dev->type		= ARPHRD_PIMREG;
	dev->mtu		= 1500 - sizeof(struct ipv6hdr) - 8;
	dev->flags		= IFF_NOARP;
	dev->netdev_ops		= &reg_vif_netdev_ops;
	dev->destructor		= free_netdev;
	dev->features		|= NETIF_F_NETNS_LOCAL;
}

static struct net_device *ip6mr_reg_vif(struct net *net, struct mr6_table *mrt)
{
	struct net_device *dev;
	char name[IFNAMSIZ];

	if (mrt->id == RT6_TABLE_DFLT)
		sprintf(name, "pim6reg");
	else
		sprintf(name, "pim6reg%u", mrt->id);

	dev = alloc_netdev(0, name, reg_vif_setup);
	if (dev == NULL)
		return NULL;

	dev_net_set(dev, net);

	if (register_netdevice(dev)) {
		free_netdev(dev);
		return NULL;
	}
	dev->iflink = 0;

	if (dev_open(dev))
		goto failure;

	dev_hold(dev);
	return dev;

failure:
	/* allow the register to be completed before unregistering. */
	rtnl_unlock();
	rtnl_lock();

	unregister_netdevice(dev);
	return NULL;
}
#endif

/*
 *	Delete a VIF entry
 */

static int mif6_delete(struct mr6_table *mrt, int vifi, struct list_head *head)
{
	struct mif_device *v;
	struct net_device *dev;
	struct inet6_dev *in6_dev;

	if (vifi < 0 || vifi >= mrt->maxvif)
		return -EADDRNOTAVAIL;

	v = &mrt->vif6_table[vifi];

	write_lock_bh(&mrt_lock);
	dev = v->dev;
	v->dev = NULL;

	if (!dev) {
		write_unlock_bh(&mrt_lock);
		return -EADDRNOTAVAIL;
	}

#ifdef CONFIG_IPV6_PIMSM_V2
	if (vifi == mrt->mroute_reg_vif_num)
		mrt->mroute_reg_vif_num = -1;
#endif

	if (vifi + 1 == mrt->maxvif) {
		int tmp;
		for (tmp = vifi - 1; tmp >= 0; tmp--) {
			if (MIF_EXISTS(mrt, tmp))
				break;
		}
		mrt->maxvif = tmp + 1;
	}

	write_unlock_bh(&mrt_lock);

	dev_set_allmulti(dev, -1);

	in6_dev = __in6_dev_get(dev);
	if (in6_dev)
		in6_dev->cnf.mc_forwarding--;

	if (v->flags & MIFF_REGISTER)
		unregister_netdevice_queue(dev, head);

	dev_put(dev);
	return 0;
}

static inline void ip6mr_cache_free(struct mfc6_cache *c)
{
	kmem_cache_free(mrt_cachep, c);
}

/* Destroy an unresolved cache entry, killing queued skbs
   and reporting error to netlink readers.
 */

static void ip6mr_destroy_unres(struct mr6_table *mrt, struct mfc6_cache *c)
{
	struct net *net = read_pnet(&mrt->net);
	struct sk_buff *skb;

	atomic_dec(&mrt->cache_resolve_queue_len);

	while((skb = skb_dequeue(&c->mfc_un.unres.unresolved)) != NULL) {
		if (ipv6_hdr(skb)->version == 0) {
			struct nlmsghdr *nlh = (struct nlmsghdr *)skb_pull(skb, sizeof(struct ipv6hdr));
			nlh->nlmsg_type = NLMSG_ERROR;
			nlh->nlmsg_len = NLMSG_LENGTH(sizeof(struct nlmsgerr));
			skb_trim(skb, nlh->nlmsg_len);
			((struct nlmsgerr *)NLMSG_DATA(nlh))->error = -ETIMEDOUT;
			rtnl_unicast(skb, net, NETLINK_CB(skb).pid);
		} else
			kfree_skb(skb);
	}

	ip6mr_cache_free(c);
}


/* Timer process for all the unresolved queue. */

static void ipmr_do_expire_process(struct mr6_table *mrt)
{
	unsigned long now = jiffies;
	unsigned long expires = 10 * HZ;
	struct mfc6_cache *c, *next;

	list_for_each_entry_safe(c, next, &mrt->mfc6_unres_queue, list) {
		if (time_after(c->mfc_un.unres.expires, now)) {
			/* not yet... */
			unsigned long interval = c->mfc_un.unres.expires - now;
			if (interval < expires)
				expires = interval;
			continue;
		}

		list_del(&c->list);
		ip6mr_destroy_unres(mrt, c);
	}

	if (!list_empty(&mrt->mfc6_unres_queue))
		mod_timer(&mrt->ipmr_expire_timer, jiffies + expires);
}

static void ipmr_expire_process(unsigned long arg)
{
	struct mr6_table *mrt = (struct mr6_table *)arg;

	if (!spin_trylock(&mfc_unres_lock)) {
		mod_timer(&mrt->ipmr_expire_timer, jiffies + 1);
		return;
	}

	if (!list_empty(&mrt->mfc6_unres_queue))
		ipmr_do_expire_process(mrt);

	spin_unlock(&mfc_unres_lock);
}

/* Fill oifs list. It is called under write locked mrt_lock. */

static void ip6mr_update_thresholds(struct mr6_table *mrt, struct mfc6_cache *cache,
				    unsigned char *ttls)
{
	int vifi;

	cache->mfc_un.res.minvif = MAXMIFS;
	cache->mfc_un.res.maxvif = 0;
	memset(cache->mfc_un.res.ttls, 255, MAXMIFS);

	for (vifi = 0; vifi < mrt->maxvif; vifi++) {
		if (MIF_EXISTS(mrt, vifi) &&
		    ttls[vifi] && ttls[vifi] < 255) {
			cache->mfc_un.res.ttls[vifi] = ttls[vifi];
			if (cache->mfc_un.res.minvif > vifi)
				cache->mfc_un.res.minvif = vifi;
			if (cache->mfc_un.res.maxvif <= vifi)
				cache->mfc_un.res.maxvif = vifi + 1;
		}
	}
}

static int mif6_add(struct net *net, struct mr6_table *mrt,
		    struct mif6ctl *vifc, int mrtsock)
{
	int vifi = vifc->mif6c_mifi;
	struct mif_device *v = &mrt->vif6_table[vifi];
	struct net_device *dev;
	struct inet6_dev *in6_dev;
	int err;

	/* Is vif busy ? */
	if (MIF_EXISTS(mrt, vifi))
		return -EADDRINUSE;

	switch (vifc->mif6c_flags) {
#ifdef CONFIG_IPV6_PIMSM_V2
	case MIFF_REGISTER:
		/*
		 * Special Purpose VIF in PIM
		 * All the packets will be sent to the daemon
		 */
		if (mrt->mroute_reg_vif_num >= 0)
			return -EADDRINUSE;
		dev = ip6mr_reg_vif(net, mrt);
		if (!dev)
			return -ENOBUFS;
		err = dev_set_allmulti(dev, 1);
		if (err) {
			unregister_netdevice(dev);
			dev_put(dev);
			return err;
		}
		break;
#endif
	case 0:
		dev = dev_get_by_index(net, vifc->mif6c_pifi);
		if (!dev)
			return -EADDRNOTAVAIL;
		err = dev_set_allmulti(dev, 1);
		if (err) {
			dev_put(dev);
			return err;
		}
		break;
	default:
		return -EINVAL;
	}

	in6_dev = __in6_dev_get(dev);
	if (in6_dev)
		in6_dev->cnf.mc_forwarding++;

	/*
	 *	Fill in the VIF structures
	 */
	v->rate_limit = vifc->vifc_rate_limit;
	v->flags = vifc->mif6c_flags;
	if (!mrtsock)
		v->flags |= VIFF_STATIC;
	v->threshold = vifc->vifc_threshold;
	v->bytes_in = 0;
	v->bytes_out = 0;
	v->pkt_in = 0;
	v->pkt_out = 0;
	v->link = dev->ifindex;
	if (v->flags & MIFF_REGISTER)
		v->link = dev->iflink;

	/* And finish update writing critical data */
	write_lock_bh(&mrt_lock);
	v->dev = dev;
#ifdef CONFIG_IPV6_PIMSM_V2
	if (v->flags & MIFF_REGISTER)
		mrt->mroute_reg_vif_num = vifi;
#endif
	if (vifi + 1 > mrt->maxvif)
		mrt->maxvif = vifi + 1;
	write_unlock_bh(&mrt_lock);
	return 0;
}

static struct mfc6_cache *ip6mr_cache_find(struct mr6_table *mrt,
					   struct in6_addr *origin,
					   struct in6_addr *mcastgrp)
{
	int line = MFC6_HASH(mcastgrp, origin);
	struct mfc6_cache *c;

	list_for_each_entry(c, &mrt->mfc6_cache_array[line], list) {
		if (ipv6_addr_equal(&c->mf6c_origin, origin) &&
		    ipv6_addr_equal(&c->mf6c_mcastgrp, mcastgrp))
			return c;
	}
	return NULL;
}

/*
 *	Allocate a multicast cache entry
 */
static struct mfc6_cache *ip6mr_cache_alloc(void)
{
	struct mfc6_cache *c = kmem_cache_zalloc(mrt_cachep, GFP_KERNEL);
	if (c == NULL)
		return NULL;
	c->mfc_un.res.minvif = MAXMIFS;
	return c;
}

static struct mfc6_cache *ip6mr_cache_alloc_unres(void)
{
	struct mfc6_cache *c = kmem_cache_zalloc(mrt_cachep, GFP_ATOMIC);
	if (c == NULL)
		return NULL;
	skb_queue_head_init(&c->mfc_un.unres.unresolved);
	c->mfc_un.unres.expires = jiffies + 10 * HZ;
	return c;
}

/*
 *	A cache entry has gone into a resolved state from queued
 */

static void ip6mr_cache_resolve(struct net *net, struct mr6_table *mrt,
				struct mfc6_cache *uc, struct mfc6_cache *c)
{
	struct sk_buff *skb;

	/*
	 *	Play the pending entries through our router
	 */

	while((skb = __skb_dequeue(&uc->mfc_un.unres.unresolved))) {
		if (ipv6_hdr(skb)->version == 0) {
			int err;
			struct nlmsghdr *nlh = (struct nlmsghdr *)skb_pull(skb, sizeof(struct ipv6hdr));

			if (__ip6mr_fill_mroute(mrt, skb, c, NLMSG_DATA(nlh)) > 0) {
				nlh->nlmsg_len = skb_tail_pointer(skb) - (u8 *)nlh;
			} else {
				nlh->nlmsg_type = NLMSG_ERROR;
				nlh->nlmsg_len = NLMSG_LENGTH(sizeof(struct nlmsgerr));
				skb_trim(skb, nlh->nlmsg_len);
				((struct nlmsgerr *)NLMSG_DATA(nlh))->error = -EMSGSIZE;
			}
			err = rtnl_unicast(skb, net, NETLINK_CB(skb).pid);
		} else
			ip6_mr_forward(net, mrt, skb, c);
	}
}

/*
 *	Bounce a cache query up to pim6sd. We could use netlink for this but pim6sd
 *	expects the following bizarre scheme.
 *
 *	Called under mrt_lock.
 */

static int ip6mr_cache_report(struct mr6_table *mrt, struct sk_buff *pkt,
			      mifi_t mifi, int assert)
{
	struct sk_buff *skb;
	struct mrt6msg *msg;
	int ret;

#ifdef CONFIG_IPV6_PIMSM_V2
	if (assert == MRT6MSG_WHOLEPKT)
		skb = skb_realloc_headroom(pkt, -skb_network_offset(pkt)
						+sizeof(*msg));
	else
#endif
		skb = alloc_skb(sizeof(struct ipv6hdr) + sizeof(*msg), GFP_ATOMIC);

	if (!skb)
		return -ENOBUFS;

	/* I suppose that internal messages
	 * do not require checksums */

	skb->ip_summed = CHECKSUM_UNNECESSARY;

#ifdef CONFIG_IPV6_PIMSM_V2
	if (assert == MRT6MSG_WHOLEPKT) {
		/* Ugly, but we have no choice with this interface.
		   Duplicate old header, fix length etc.
		   And all this only to mangle msg->im6_msgtype and
		   to set msg->im6_mbz to "mbz" :-)
		 */
		skb_push(skb, -skb_network_offset(pkt));

		skb_push(skb, sizeof(*msg));
		skb_reset_transport_header(skb);
		msg = (struct mrt6msg *)skb_transport_header(skb);
		msg->im6_mbz = 0;
		msg->im6_msgtype = MRT6MSG_WHOLEPKT;
		msg->im6_mif = mrt->mroute_reg_vif_num;
		msg->im6_pad = 0;
		ipv6_addr_copy(&msg->im6_src, &ipv6_hdr(pkt)->saddr);
		ipv6_addr_copy(&msg->im6_dst, &ipv6_hdr(pkt)->daddr);

		skb->ip_summed = CHECKSUM_UNNECESSARY;
	} else
#endif
	{
	/*
	 *	Copy the IP header
	 */

	skb_put(skb, sizeof(struct ipv6hdr));
	skb_reset_network_header(skb);
	skb_copy_to_linear_data(skb, ipv6_hdr(pkt), sizeof(struct ipv6hdr));

	/*
	 *	Add our header
	 */
	skb_put(skb, sizeof(*msg));
	skb_reset_transport_header(skb);
	msg = (struct mrt6msg *)skb_transport_header(skb);

	msg->im6_mbz = 0;
	msg->im6_msgtype = assert;
	msg->im6_mif = mifi;
	msg->im6_pad = 0;
	ipv6_addr_copy(&msg->im6_src, &ipv6_hdr(pkt)->saddr);
	ipv6_addr_copy(&msg->im6_dst, &ipv6_hdr(pkt)->daddr);

	skb_dst_set(skb, dst_clone(skb_dst(pkt)));
	skb->ip_summed = CHECKSUM_UNNECESSARY;
	}

	if (mrt->mroute6_sk == NULL) {
		kfree_skb(skb);
		return -EINVAL;
	}

	/*
	 *	Deliver to user space multicast routing algorithms
	 */
	ret = sock_queue_rcv_skb(mrt->mroute6_sk, skb);
	if (ret < 0) {
		if (net_ratelimit())
			printk(KERN_WARNING "mroute6: pending queue full, dropping entries.\n");
		kfree_skb(skb);
	}

	return ret;
}

/*
 *	Queue a packet for resolution. It gets locked cache entry!
 */

static int
ip6mr_cache_unresolved(struct mr6_table *mrt, mifi_t mifi, struct sk_buff *skb)
{
	bool found = false;
	int err;
	struct mfc6_cache *c;

	spin_lock_bh(&mfc_unres_lock);
	list_for_each_entry(c, &mrt->mfc6_unres_queue, list) {
		if (ipv6_addr_equal(&c->mf6c_mcastgrp, &ipv6_hdr(skb)->daddr) &&
		    ipv6_addr_equal(&c->mf6c_origin, &ipv6_hdr(skb)->saddr)) {
			found = true;
			break;
		}
	}

	if (!found) {
		/*
		 *	Create a new entry if allowable
		 */

		if (atomic_read(&mrt->cache_resolve_queue_len) >= 10 ||
		    (c = ip6mr_cache_alloc_unres()) == NULL) {
			spin_unlock_bh(&mfc_unres_lock);

			kfree_skb(skb);
			return -ENOBUFS;
		}

		/*
		 *	Fill in the new cache entry
		 */
		c->mf6c_parent = -1;
		c->mf6c_origin = ipv6_hdr(skb)->saddr;
		c->mf6c_mcastgrp = ipv6_hdr(skb)->daddr;

		/*
		 *	Reflect first query at pim6sd
		 */
		err = ip6mr_cache_report(mrt, skb, mifi, MRT6MSG_NOCACHE);
		if (err < 0) {
			/* If the report failed throw the cache entry
			   out - Brad Parker
			 */
			spin_unlock_bh(&mfc_unres_lock);

			ip6mr_cache_free(c);
			kfree_skb(skb);
			return err;
		}

		atomic_inc(&mrt->cache_resolve_queue_len);
		list_add(&c->list, &mrt->mfc6_unres_queue);

		ipmr_do_expire_process(mrt);
	}

	/*
	 *	See if we can append the packet
	 */
	if (c->mfc_un.unres.unresolved.qlen > 3) {
		kfree_skb(skb);
		err = -ENOBUFS;
	} else {
		skb_queue_tail(&c->mfc_un.unres.unresolved, skb);
		err = 0;
	}

	spin_unlock_bh(&mfc_unres_lock);
	return err;
}

/*
 *	MFC6 cache manipulation by user space
 */

static int ip6mr_mfc_delete(struct mr6_table *mrt, struct mf6cctl *mfc)
{
	int line;
	struct mfc6_cache *c, *next;

	line = MFC6_HASH(&mfc->mf6cc_mcastgrp.sin6_addr, &mfc->mf6cc_origin.sin6_addr);

	list_for_each_entry_safe(c, next, &mrt->mfc6_cache_array[line], list) {
		if (ipv6_addr_equal(&c->mf6c_origin, &mfc->mf6cc_origin.sin6_addr) &&
		    ipv6_addr_equal(&c->mf6c_mcastgrp, &mfc->mf6cc_mcastgrp.sin6_addr)) {
			write_lock_bh(&mrt_lock);
			list_del(&c->list);
			write_unlock_bh(&mrt_lock);

			ip6mr_cache_free(c);
			return 0;
		}
	}
	return -ENOENT;
}

static int ip6mr_device_event(struct notifier_block *this,
			      unsigned long event, void *ptr)
{
	struct net_device *dev = ptr;
	struct net *net = dev_net(dev);
	struct mr6_table *mrt;
	struct mif_device *v;
	int ct;
	LIST_HEAD(list);

	if (event != NETDEV_UNREGISTER)
		return NOTIFY_DONE;

	ip6mr_for_each_table(mrt, net) {
		v = &mrt->vif6_table[0];
		for (ct = 0; ct < mrt->maxvif; ct++, v++) {
			if (v->dev == dev)
				mif6_delete(mrt, ct, &list);
		}
	}
	unregister_netdevice_many(&list);

	return NOTIFY_DONE;
}

static struct notifier_block ip6_mr_notifier = {
	.notifier_call = ip6mr_device_event
};

/*
 *	Setup for IP multicast routing
 */

static int __net_init ip6mr_net_init(struct net *net)
{
	int err;

	err = ip6mr_rules_init(net);
	if (err < 0)
		goto fail;

#ifdef CONFIG_PROC_FS
	err = -ENOMEM;
	if (!proc_net_fops_create(net, "ip6_mr_vif", 0, &ip6mr_vif_fops))
		goto proc_vif_fail;
	if (!proc_net_fops_create(net, "ip6_mr_cache", 0, &ip6mr_mfc_fops))
		goto proc_cache_fail;
#endif

	return 0;

#ifdef CONFIG_PROC_FS
proc_cache_fail:
	proc_net_remove(net, "ip6_mr_vif");
proc_vif_fail:
	ip6mr_rules_exit(net);
#endif
fail:
	return err;
}

static void __net_exit ip6mr_net_exit(struct net *net)
{
#ifdef CONFIG_PROC_FS
	proc_net_remove(net, "ip6_mr_cache");
	proc_net_remove(net, "ip6_mr_vif");
#endif
	ip6mr_rules_exit(net);
}

static struct pernet_operations ip6mr_net_ops = {
	.init = ip6mr_net_init,
	.exit = ip6mr_net_exit,
};

int __init ip6_mr_init(void)
{
	int err;

	mrt_cachep = kmem_cache_create("ip6_mrt_cache",
				       sizeof(struct mfc6_cache),
				       0, SLAB_HWCACHE_ALIGN,
				       NULL);
	if (!mrt_cachep)
		return -ENOMEM;

	err = register_pernet_subsys(&ip6mr_net_ops);
	if (err)
		goto reg_pernet_fail;

	err = register_netdevice_notifier(&ip6_mr_notifier);
	if (err)
		goto reg_notif_fail;
#ifdef CONFIG_IPV6_PIMSM_V2
	if (inet6_add_protocol(&pim6_protocol, IPPROTO_PIM) < 0) {
		printk(KERN_ERR "ip6_mr_init: can't add PIM protocol\n");
		err = -EAGAIN;
		goto add_proto_fail;
	}
#endif
	rtnl_register(RTNL_FAMILY_IP6MR, RTM_GETROUTE, NULL, ip6mr_rtm_dumproute);
	return 0;
#ifdef CONFIG_IPV6_PIMSM_V2
add_proto_fail:
	unregister_netdevice_notifier(&ip6_mr_notifier);
#endif
reg_notif_fail:
	unregister_pernet_subsys(&ip6mr_net_ops);
reg_pernet_fail:
	kmem_cache_destroy(mrt_cachep);
	return err;
}

void ip6_mr_cleanup(void)
{
	unregister_netdevice_notifier(&ip6_mr_notifier);
	unregister_pernet_subsys(&ip6mr_net_ops);
	kmem_cache_destroy(mrt_cachep);
}

static int ip6mr_mfc_add(struct net *net, struct mr6_table *mrt,
			 struct mf6cctl *mfc, int mrtsock)
{
	bool found = false;
	int line;
	struct mfc6_cache *uc, *c;
	unsigned char ttls[MAXMIFS];
	int i;

	if (mfc->mf6cc_parent >= MAXMIFS)
		return -ENFILE;

	memset(ttls, 255, MAXMIFS);
	for (i = 0; i < MAXMIFS; i++) {
		if (IF_ISSET(i, &mfc->mf6cc_ifset))
			ttls[i] = 1;

	}

	line = MFC6_HASH(&mfc->mf6cc_mcastgrp.sin6_addr, &mfc->mf6cc_origin.sin6_addr);

	list_for_each_entry(c, &mrt->mfc6_cache_array[line], list) {
		if (ipv6_addr_equal(&c->mf6c_origin, &mfc->mf6cc_origin.sin6_addr) &&
		    ipv6_addr_equal(&c->mf6c_mcastgrp, &mfc->mf6cc_mcastgrp.sin6_addr)) {
			found = true;
			break;
		}
	}

	if (found) {
		write_lock_bh(&mrt_lock);
		c->mf6c_parent = mfc->mf6cc_parent;
		ip6mr_update_thresholds(mrt, c, ttls);
		if (!mrtsock)
			c->mfc_flags |= MFC_STATIC;
		write_unlock_bh(&mrt_lock);
		return 0;
	}

	if (!ipv6_addr_is_multicast(&mfc->mf6cc_mcastgrp.sin6_addr))
		return -EINVAL;

	c = ip6mr_cache_alloc();
	if (c == NULL)
		return -ENOMEM;

	c->mf6c_origin = mfc->mf6cc_origin.sin6_addr;
	c->mf6c_mcastgrp = mfc->mf6cc_mcastgrp.sin6_addr;
	c->mf6c_parent = mfc->mf6cc_parent;
	ip6mr_update_thresholds(mrt, c, ttls);
	if (!mrtsock)
		c->mfc_flags |= MFC_STATIC;

	write_lock_bh(&mrt_lock);
	list_add(&c->list, &mrt->mfc6_cache_array[line]);
	write_unlock_bh(&mrt_lock);

	/*
	 *	Check to see if we resolved a queued list. If so we
	 *	need to send on the frames and tidy up.
	 */
	found = false;
	spin_lock_bh(&mfc_unres_lock);
	list_for_each_entry(uc, &mrt->mfc6_unres_queue, list) {
		if (ipv6_addr_equal(&uc->mf6c_origin, &c->mf6c_origin) &&
		    ipv6_addr_equal(&uc->mf6c_mcastgrp, &c->mf6c_mcastgrp)) {
			list_del(&uc->list);
			atomic_dec(&mrt->cache_resolve_queue_len);
			found = true;
			break;
		}
	}
	if (list_empty(&mrt->mfc6_unres_queue))
		del_timer(&mrt->ipmr_expire_timer);
	spin_unlock_bh(&mfc_unres_lock);

	if (found) {
		ip6mr_cache_resolve(net, mrt, uc, c);
		ip6mr_cache_free(uc);
	}
	return 0;
}

/*
 *	Close the multicast socket, and clear the vif tables etc
 */

static void mroute_clean_tables(struct mr6_table *mrt)
{
	int i;
	LIST_HEAD(list);
	struct mfc6_cache *c, *next;

	/*
	 *	Shut down all active vif entries
	 */
	for (i = 0; i < mrt->maxvif; i++) {
		if (!(mrt->vif6_table[i].flags & VIFF_STATIC))
			mif6_delete(mrt, i, &list);
	}
	unregister_netdevice_many(&list);

	/*
	 *	Wipe the cache
	 */
	for (i = 0; i < MFC6_LINES; i++) {
		list_for_each_entry_safe(c, next, &mrt->mfc6_cache_array[i], list) {
			if (c->mfc_flags & MFC_STATIC)
				continue;
			write_lock_bh(&mrt_lock);
			list_del(&c->list);
			write_unlock_bh(&mrt_lock);

			ip6mr_cache_free(c);
		}
	}

	if (atomic_read(&mrt->cache_resolve_queue_len) != 0) {
		spin_lock_bh(&mfc_unres_lock);
		list_for_each_entry_safe(c, next, &mrt->mfc6_unres_queue, list) {
			list_del(&c->list);
			ip6mr_destroy_unres(mrt, c);
		}
		spin_unlock_bh(&mfc_unres_lock);
	}
}

static int ip6mr_sk_init(struct mr6_table *mrt, struct sock *sk)
{
	int err = 0;
	struct net *net = sock_net(sk);

	rtnl_lock();
	write_lock_bh(&mrt_lock);
	if (likely(mrt->mroute6_sk == NULL)) {
		mrt->mroute6_sk = sk;
		net->ipv6.devconf_all->mc_forwarding++;
	}
	else
		err = -EADDRINUSE;
	write_unlock_bh(&mrt_lock);

	rtnl_unlock();

	return err;
}

int ip6mr_sk_done(struct sock *sk)
{
	int err = -EACCES;
	struct net *net = sock_net(sk);
	struct mr6_table *mrt;

	rtnl_lock();
	ip6mr_for_each_table(mrt, net) {
		if (sk == mrt->mroute6_sk) {
			write_lock_bh(&mrt_lock);
			mrt->mroute6_sk = NULL;
			net->ipv6.devconf_all->mc_forwarding--;
			write_unlock_bh(&mrt_lock);

			mroute_clean_tables(mrt);
			err = 0;
			break;
		}
	}
	rtnl_unlock();

	return err;
}

struct sock *mroute6_socket(struct net *net, struct sk_buff *skb)
{
	struct mr6_table *mrt;
	struct flowi fl = {
		.iif	= skb->skb_iif,
		.oif	= skb->dev->ifindex,
		.mark	= skb->mark,
	};

	if (ip6mr_fib_lookup(net, &fl, &mrt) < 0)
		return NULL;

	return mrt->mroute6_sk;
}

/*
 *	Socket options and virtual interface manipulation. The whole
 *	virtual interface system is a complete heap, but unfortunately
 *	that's how BSD mrouted happens to think. Maybe one day with a proper
 *	MOSPF/PIM router set up we can clean this up.
 */

int ip6_mroute_setsockopt(struct sock *sk, int optname, char __user *optval, unsigned int optlen)
{
	int ret;
	struct mif6ctl vif;
	struct mf6cctl mfc;
	mifi_t mifi;
	struct net *net = sock_net(sk);
	struct mr6_table *mrt;

	mrt = ip6mr_get_table(net, raw6_sk(sk)->ip6mr_table ? : RT6_TABLE_DFLT);
	if (mrt == NULL)
		return -ENOENT;

	if (optname != MRT6_INIT) {
		if (sk != mrt->mroute6_sk && !capable(CAP_NET_ADMIN))
			return -EACCES;
	}

	switch (optname) {
	case MRT6_INIT:
		if (sk->sk_type != SOCK_RAW ||
		    inet_sk(sk)->inet_num != IPPROTO_ICMPV6)
			return -EOPNOTSUPP;
		if (optlen < sizeof(int))
			return -EINVAL;

		return ip6mr_sk_init(mrt, sk);

	case MRT6_DONE:
		return ip6mr_sk_done(sk);

	case MRT6_ADD_MIF:
		if (optlen < sizeof(vif))
			return -EINVAL;
		if (copy_from_user(&vif, optval, sizeof(vif)))
			return -EFAULT;
		if (vif.mif6c_mifi >= MAXMIFS)
			return -ENFILE;
		rtnl_lock();
		ret = mif6_add(net, mrt, &vif, sk == mrt->mroute6_sk);
		rtnl_unlock();
		return ret;

	case MRT6_DEL_MIF:
		if (optlen < sizeof(mifi_t))
			return -EINVAL;
		if (copy_from_user(&mifi, optval, sizeof(mifi_t)))
			return -EFAULT;
		rtnl_lock();
		ret = mif6_delete(mrt, mifi, NULL);
		rtnl_unlock();
		return ret;

	/*
	 *	Manipulate the forwarding caches. These live
	 *	in a sort of kernel/user symbiosis.
	 */
	case MRT6_ADD_MFC:
	case MRT6_DEL_MFC:
		if (optlen < sizeof(mfc))
			return -EINVAL;
		if (copy_from_user(&mfc, optval, sizeof(mfc)))
			return -EFAULT;
		rtnl_lock();
		if (optname == MRT6_DEL_MFC)
			ret = ip6mr_mfc_delete(mrt, &mfc);
		else
			ret = ip6mr_mfc_add(net, mrt, &mfc, sk == mrt->mroute6_sk);
		rtnl_unlock();
		return ret;

	/*
	 *	Control PIM assert (to activate pim will activate assert)
	 */
	case MRT6_ASSERT:
	{
		int v;
		if (get_user(v, (int __user *)optval))
			return -EFAULT;
		mrt->mroute_do_assert = !!v;
		return 0;
	}

#ifdef CONFIG_IPV6_PIMSM_V2
	case MRT6_PIM:
	{
		int v;
		if (get_user(v, (int __user *)optval))
			return -EFAULT;
		v = !!v;
		rtnl_lock();
		ret = 0;
		if (v != mrt->mroute_do_pim) {
			mrt->mroute_do_pim = v;
			mrt->mroute_do_assert = v;
		}
		rtnl_unlock();
		return ret;
	}

#endif
#ifdef CONFIG_IPV6_MROUTE_MULTIPLE_TABLES
	case MRT6_TABLE:
	{
		u32 v;

		if (optlen != sizeof(u32))
			return -EINVAL;
		if (get_user(v, (u32 __user *)optval))
			return -EFAULT;
		if (sk == mrt->mroute6_sk)
			return -EBUSY;

		rtnl_lock();
		ret = 0;
		if (!ip6mr_new_table(net, v))
			ret = -ENOMEM;
		raw6_sk(sk)->ip6mr_table = v;
		rtnl_unlock();
		return ret;
	}
#endif
	/*
	 *	Spurious command, or MRT6_VERSION which you cannot
	 *	set.
	 */
	default:
		return -ENOPROTOOPT;
	}
}

/*
 *	Getsock opt support for the multicast routing system.
 */

int ip6_mroute_getsockopt(struct sock *sk, int optname, char __user *optval,
			  int __user *optlen)
{
	int olr;
	int val;
	struct net *net = sock_net(sk);
	struct mr6_table *mrt;

	mrt = ip6mr_get_table(net, raw6_sk(sk)->ip6mr_table ? : RT6_TABLE_DFLT);
	if (mrt == NULL)
		return -ENOENT;

	switch (optname) {
	case MRT6_VERSION:
		val = 0x0305;
		break;
#ifdef CONFIG_IPV6_PIMSM_V2
	case MRT6_PIM:
		val = mrt->mroute_do_pim;
		break;
#endif
	case MRT6_ASSERT:
		val = mrt->mroute_do_assert;
		break;
	default:
		return -ENOPROTOOPT;
	}

	if (get_user(olr, optlen))
		return -EFAULT;

	olr = min_t(int, olr, sizeof(int));
	if (olr < 0)
		return -EINVAL;

	if (put_user(olr, optlen))
		return -EFAULT;
	if (copy_to_user(optval, &val, olr))
		return -EFAULT;
	return 0;
}

/*
 *	The IP multicast ioctl support routines.
 */

int ip6mr_ioctl(struct sock *sk, int cmd, void __user *arg)
{
	struct sioc_sg_req6 sr;
	struct sioc_mif_req6 vr;
	struct mif_device *vif;
	struct mfc6_cache *c;
	struct net *net = sock_net(sk);
	struct mr6_table *mrt;

	mrt = ip6mr_get_table(net, raw6_sk(sk)->ip6mr_table ? : RT6_TABLE_DFLT);
	if (mrt == NULL)
		return -ENOENT;

	switch (cmd) {
	case SIOCGETMIFCNT_IN6:
		if (copy_from_user(&vr, arg, sizeof(vr)))
			return -EFAULT;
		if (vr.mifi >= mrt->maxvif)
			return -EINVAL;
		read_lock(&mrt_lock);
		vif = &mrt->vif6_table[vr.mifi];
		if (MIF_EXISTS(mrt, vr.mifi)) {
			vr.icount = vif->pkt_in;
			vr.ocount = vif->pkt_out;
			vr.ibytes = vif->bytes_in;
			vr.obytes = vif->bytes_out;
			read_unlock(&mrt_lock);

			if (copy_to_user(arg, &vr, sizeof(vr)))
				return -EFAULT;
			return 0;
		}
		read_unlock(&mrt_lock);
		return -EADDRNOTAVAIL;
	case SIOCGETSGCNT_IN6:
		if (copy_from_user(&sr, arg, sizeof(sr)))
			return -EFAULT;

		read_lock(&mrt_lock);
		c = ip6mr_cache_find(mrt, &sr.src.sin6_addr, &sr.grp.sin6_addr);
		if (c) {
			sr.pktcnt = c->mfc_un.res.pkt;
			sr.bytecnt = c->mfc_un.res.bytes;
			sr.wrong_if = c->mfc_un.res.wrong_if;
			read_unlock(&mrt_lock);

			if (copy_to_user(arg, &sr, sizeof(sr)))
				return -EFAULT;
			return 0;
		}
		read_unlock(&mrt_lock);
		return -EADDRNOTAVAIL;
	default:
		return -ENOIOCTLCMD;
	}
}


static inline int ip6mr_forward2_finish(struct sk_buff *skb)
{
	IP6_INC_STATS_BH(dev_net(skb_dst(skb)->dev), ip6_dst_idev(skb_dst(skb)),
			 IPSTATS_MIB_OUTFORWDATAGRAMS);
	return dst_output(skb);
}

/*
 *	Processing handlers for ip6mr_forward
 */

static int ip6mr_forward2(struct net *net, struct mr6_table *mrt,
			  struct sk_buff *skb, struct mfc6_cache *c, int vifi)
{
	struct ipv6hdr *ipv6h;
	struct mif_device *vif = &mrt->vif6_table[vifi];
	struct net_device *dev;
	struct dst_entry *dst;
	struct flowi fl;

	if (vif->dev == NULL)
		goto out_free;

#ifdef CONFIG_IPV6_PIMSM_V2
	if (vif->flags & MIFF_REGISTER) {
		vif->pkt_out++;
		vif->bytes_out += skb->len;
		vif->dev->stats.tx_bytes += skb->len;
		vif->dev->stats.tx_packets++;
		ip6mr_cache_report(mrt, skb, vifi, MRT6MSG_WHOLEPKT);
		goto out_free;
	}
#endif

	ipv6h = ipv6_hdr(skb);

	fl = (struct flowi) {
		.oif = vif->link,
		.nl_u = { .ip6_u =
				{ .daddr = ipv6h->daddr, }
		}
	};

	dst = ip6_route_output(net, NULL, &fl);
	if (!dst)
		goto out_free;

	skb_dst_drop(skb);
	skb_dst_set(skb, dst);

	/*
	 * RFC1584 teaches, that DVMRP/PIM router must deliver packets locally
	 * not only before forwarding, but after forwarding on all output
	 * interfaces. It is clear, if mrouter runs a multicasting
	 * program, it should receive packets not depending to what interface
	 * program is joined.
	 * If we will not make it, the program will have to join on all
	 * interfaces. On the other hand, multihoming host (or router, but
	 * not mrouter) cannot join to more than one interface - it will
	 * result in receiving multiple packets.
	 */
	dev = vif->dev;
	skb->dev = dev;
	vif->pkt_out++;
	vif->bytes_out += skb->len;

	/* We are about to write */
	if (skb_cow(skb, sizeof(*ipv6h) + LL_RESERVED_SPACE(dev)))
		goto out_free;

	ipv6h = ipv6_hdr(skb);
	ipv6h->hop_limit--;

	IP6CB(skb)->flags |= IP6SKB_FORWARDED;

	return NF_HOOK(NFPROTO_IPV6, NF_INET_FORWARD, skb, skb->dev, dev,
		       ip6mr_forward2_finish);

out_free:
	kfree_skb(skb);
	return 0;
}

static int ip6mr_find_vif(struct mr6_table *mrt, struct net_device *dev)
{
	int ct;

	for (ct = mrt->maxvif - 1; ct >= 0; ct--) {
		if (mrt->vif6_table[ct].dev == dev)
			break;
	}
	return ct;
}

static int ip6_mr_forward(struct net *net, struct mr6_table *mrt,
			  struct sk_buff *skb, struct mfc6_cache *cache)
{
	int psend = -1;
	int vif, ct;

	vif = cache->mf6c_parent;
	cache->mfc_un.res.pkt++;
	cache->mfc_un.res.bytes += skb->len;

	/*
	 * Wrong interface: drop packet and (maybe) send PIM assert.
	 */
	if (mrt->vif6_table[vif].dev != skb->dev) {
		int true_vifi;

		cache->mfc_un.res.wrong_if++;
		true_vifi = ip6mr_find_vif(mrt, skb->dev);

		if (true_vifi >= 0 && mrt->mroute_do_assert &&
		    /* pimsm uses asserts, when switching from RPT to SPT,
		       so that we cannot check that packet arrived on an oif.
		       It is bad, but otherwise we would need to move pretty
		       large chunk of pimd to kernel. Ough... --ANK
		     */
		    (mrt->mroute_do_pim ||
		     cache->mfc_un.res.ttls[true_vifi] < 255) &&
		    time_after(jiffies,
			       cache->mfc_un.res.last_assert + MFC_ASSERT_THRESH)) {
			cache->mfc_un.res.last_assert = jiffies;
			ip6mr_cache_report(mrt, skb, true_vifi, MRT6MSG_WRONGMIF);
		}
		goto dont_forward;
	}

	mrt->vif6_table[vif].pkt_in++;
	mrt->vif6_table[vif].bytes_in += skb->len;

	/*
	 *	Forward the frame
	 */
	for (ct = cache->mfc_un.res.maxvif - 1; ct >= cache->mfc_un.res.minvif; ct--) {
		if (ipv6_hdr(skb)->hop_limit > cache->mfc_un.res.ttls[ct]) {
			if (psend != -1) {
				struct sk_buff *skb2 = skb_clone(skb, GFP_ATOMIC);
				if (skb2)
					ip6mr_forward2(net, mrt, skb2, cache, psend);
			}
			psend = ct;
		}
	}
	if (psend != -1) {
		ip6mr_forward2(net, mrt, skb, cache, psend);
		return 0;
	}

dont_forward:
	kfree_skb(skb);
	return 0;
}


/*
 *	Multicast packets for forwarding arrive here
 */

int ip6_mr_input(struct sk_buff *skb)
{
	struct mfc6_cache *cache;
	struct net *net = dev_net(skb->dev);
	struct mr6_table *mrt;
	struct flowi fl = {
		.iif	= skb->dev->ifindex,
		.mark	= skb->mark,
	};
	int err;

	err = ip6mr_fib_lookup(net, &fl, &mrt);
	if (err < 0)
		return err;

	read_lock(&mrt_lock);
	cache = ip6mr_cache_find(mrt,
				 &ipv6_hdr(skb)->saddr, &ipv6_hdr(skb)->daddr);

	/*
	 *	No usable cache entry
	 */
	if (cache == NULL) {
		int vif;

		vif = ip6mr_find_vif(mrt, skb->dev);
		if (vif >= 0) {
			int err = ip6mr_cache_unresolved(mrt, vif, skb);
			read_unlock(&mrt_lock);

			return err;
		}
		read_unlock(&mrt_lock);
		kfree_skb(skb);
		return -ENODEV;
	}

	ip6_mr_forward(net, mrt, skb, cache);

	read_unlock(&mrt_lock);

	return 0;
}


static int __ip6mr_fill_mroute(struct mr6_table *mrt, struct sk_buff *skb,
			       struct mfc6_cache *c, struct rtmsg *rtm)
{
	int ct;
	struct rtnexthop *nhp;
	u8 *b = skb_tail_pointer(skb);
	struct rtattr *mp_head;

	/* If cache is unresolved, don't try to parse IIF and OIF */
	if (c->mf6c_parent >= MAXMIFS)
		return -ENOENT;

	if (MIF_EXISTS(mrt, c->mf6c_parent))
		RTA_PUT(skb, RTA_IIF, 4, &mrt->vif6_table[c->mf6c_parent].dev->ifindex);

	mp_head = (struct rtattr *)skb_put(skb, RTA_LENGTH(0));

	for (ct = c->mfc_un.res.minvif; ct < c->mfc_un.res.maxvif; ct++) {
		if (MIF_EXISTS(mrt, ct) && c->mfc_un.res.ttls[ct] < 255) {
			if (skb_tailroom(skb) < RTA_ALIGN(RTA_ALIGN(sizeof(*nhp)) + 4))
				goto rtattr_failure;
			nhp = (struct rtnexthop *)skb_put(skb, RTA_ALIGN(sizeof(*nhp)));
			nhp->rtnh_flags = 0;
			nhp->rtnh_hops = c->mfc_un.res.ttls[ct];
			nhp->rtnh_ifindex = mrt->vif6_table[ct].dev->ifindex;
			nhp->rtnh_len = sizeof(*nhp);
		}
	}
	mp_head->rta_type = RTA_MULTIPATH;
	mp_head->rta_len = skb_tail_pointer(skb) - (u8 *)mp_head;
	rtm->rtm_type = RTN_MULTICAST;
	return 1;

rtattr_failure:
	nlmsg_trim(skb, b);
	return -EMSGSIZE;
}

int ip6mr_get_route(struct net *net,
		    struct sk_buff *skb, struct rtmsg *rtm, int nowait)
{
	int err;
	struct mr6_table *mrt;
	struct mfc6_cache *cache;
	struct rt6_info *rt = (struct rt6_info *)skb_dst(skb);

	mrt = ip6mr_get_table(net, RT6_TABLE_DFLT);
	if (mrt == NULL)
		return -ENOENT;

	read_lock(&mrt_lock);
	cache = ip6mr_cache_find(mrt, &rt->rt6i_src.addr, &rt->rt6i_dst.addr);

	if (!cache) {
		struct sk_buff *skb2;
		struct ipv6hdr *iph;
		struct net_device *dev;
		int vif;

		if (nowait) {
			read_unlock(&mrt_lock);
			return -EAGAIN;
		}

		dev = skb->dev;
		if (dev == NULL || (vif = ip6mr_find_vif(mrt, dev)) < 0) {
			read_unlock(&mrt_lock);
			return -ENODEV;
		}

		/* really correct? */
		skb2 = alloc_skb(sizeof(struct ipv6hdr), GFP_ATOMIC);
		if (!skb2) {
			read_unlock(&mrt_lock);
			return -ENOMEM;
		}

		skb_reset_transport_header(skb2);

		skb_put(skb2, sizeof(struct ipv6hdr));
		skb_reset_network_header(skb2);

		iph = ipv6_hdr(skb2);
		iph->version = 0;
		iph->priority = 0;
		iph->flow_lbl[0] = 0;
		iph->flow_lbl[1] = 0;
		iph->flow_lbl[2] = 0;
		iph->payload_len = 0;
		iph->nexthdr = IPPROTO_NONE;
		iph->hop_limit = 0;
		ipv6_addr_copy(&iph->saddr, &rt->rt6i_src.addr);
		ipv6_addr_copy(&iph->daddr, &rt->rt6i_dst.addr);

		err = ip6mr_cache_unresolved(mrt, vif, skb2);
		read_unlock(&mrt_lock);

		return err;
	}

	if (!nowait && (rtm->rtm_flags&RTM_F_NOTIFY))
		cache->mfc_flags |= MFC_NOTIFY;

	err = __ip6mr_fill_mroute(mrt, skb, cache, rtm);
	read_unlock(&mrt_lock);
	return err;
}

static int ip6mr_fill_mroute(struct mr6_table *mrt, struct sk_buff *skb,
			     u32 pid, u32 seq, struct mfc6_cache *c)
{
	struct nlmsghdr *nlh;
	struct rtmsg *rtm;

	nlh = nlmsg_put(skb, pid, seq, RTM_NEWROUTE, sizeof(*rtm), NLM_F_MULTI);
	if (nlh == NULL)
		return -EMSGSIZE;

	rtm = nlmsg_data(nlh);
	rtm->rtm_family   = RTNL_FAMILY_IPMR;
	rtm->rtm_dst_len  = 128;
	rtm->rtm_src_len  = 128;
	rtm->rtm_tos      = 0;
	rtm->rtm_table    = mrt->id;
	NLA_PUT_U32(skb, RTA_TABLE, mrt->id);
	rtm->rtm_scope    = RT_SCOPE_UNIVERSE;
	rtm->rtm_protocol = RTPROT_UNSPEC;
	rtm->rtm_flags    = 0;

	NLA_PUT(skb, RTA_SRC, 16, &c->mf6c_origin);
	NLA_PUT(skb, RTA_DST, 16, &c->mf6c_mcastgrp);

	if (__ip6mr_fill_mroute(mrt, skb, c, rtm) < 0)
		goto nla_put_failure;

	return nlmsg_end(skb, nlh);

nla_put_failure:
	nlmsg_cancel(skb, nlh);
	return -EMSGSIZE;
}

static int ip6mr_rtm_dumproute(struct sk_buff *skb, struct netlink_callback *cb)
{
	struct net *net = sock_net(skb->sk);
	struct mr6_table *mrt;
	struct mfc6_cache *mfc;
	unsigned int t = 0, s_t;
	unsigned int h = 0, s_h;
	unsigned int e = 0, s_e;

	s_t = cb->args[0];
	s_h = cb->args[1];
	s_e = cb->args[2];

	read_lock(&mrt_lock);
	ip6mr_for_each_table(mrt, net) {
		if (t < s_t)
			goto next_table;
		if (t > s_t)
			s_h = 0;
		for (h = s_h; h < MFC6_LINES; h++) {
			list_for_each_entry(mfc, &mrt->mfc6_cache_array[h], list) {
				if (e < s_e)
					goto next_entry;
				if (ip6mr_fill_mroute(mrt, skb,
						      NETLINK_CB(cb->skb).pid,
						      cb->nlh->nlmsg_seq,
						      mfc) < 0)
					goto done;
next_entry:
				e++;
			}
			e = s_e = 0;
		}
		s_h = 0;
next_table:
		t++;
	}
done:
	read_unlock(&mrt_lock);

	cb->args[2] = e;
	cb->args[1] = h;
	cb->args[0] = t;

	return skb->len;
}
