/*
 *	IP multicast routing support for mrouted 3.6/3.8
 *
 *		(c) 1995 Alan Cox, <alan@lxorguk.ukuu.org.uk>
 *	  Linux Consultancy and Custom Driver Development
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 *
 *	Fixes:
 *	Michael Chastain	:	Incorrect size of copying.
 *	Alan Cox		:	Added the cache manager code
 *	Alan Cox		:	Fixed the clone/copy bug and device race.
 *	Mike McLagan		:	Routing by source
 *	Malcolm Beattie		:	Buffer handling fixes.
 *	Alexey Kuznetsov	:	Double buffer free and other fixes.
 *	SVR Anand		:	Fixed several multicast bugs and problems.
 *	Alexey Kuznetsov	:	Status, optimisations and more.
 *	Brad Parker		:	Better behaviour on mrouted upcall
 *					overflow.
 *      Carlos Picoto           :       PIMv1 Support
 *	Pavlin Ivanov Radoslavov:	PIMv2 Registers must checksum only PIM header
 *					Relax this requirement to work with older peers.
 *
 */

#include <asm/system.h>
#include <asm/uaccess.h>
#include <linux/types.h>
#include <linux/capability.h>
#include <linux/errno.h>
#include <linux/timer.h>
#include <linux/mm.h>
#include <linux/kernel.h>
#include <linux/fcntl.h>
#include <linux/stat.h>
#include <linux/socket.h>
#include <linux/in.h>
#include <linux/inet.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/igmp.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/mroute.h>
#include <linux/init.h>
#include <linux/if_ether.h>
#include <linux/slab.h>
#include <net/net_namespace.h>
#include <net/ip.h>
#include <net/protocol.h>
#include <linux/skbuff.h>
#include <net/route.h>
#include <net/sock.h>
#include <net/icmp.h>
#include <net/udp.h>
#include <net/raw.h>
#include <linux/notifier.h>
#include <linux/if_arp.h>
#include <linux/netfilter_ipv4.h>
#include <net/ipip.h>
#include <net/checksum.h>
#include <net/netlink.h>
#include <net/fib_rules.h>

#if defined(CONFIG_IP_PIMSM_V1) || defined(CONFIG_IP_PIMSM_V2)
#define CONFIG_IP_PIMSM	1
#endif

struct mr_table {
	struct list_head	list;
#ifdef CONFIG_NET_NS
	struct net		*net;
#endif
	u32			id;
	struct sock		*mroute_sk;
	struct timer_list	ipmr_expire_timer;
	struct list_head	mfc_unres_queue;
	struct list_head	mfc_cache_array[MFC_LINES];
	struct vif_device	vif_table[MAXVIFS];
	int			maxvif;
	atomic_t		cache_resolve_queue_len;
	int			mroute_do_assert;
	int			mroute_do_pim;
#if defined(CONFIG_IP_PIMSM_V1) || defined(CONFIG_IP_PIMSM_V2)
	int			mroute_reg_vif_num;
#endif
};

struct ipmr_rule {
	struct fib_rule		common;
};

struct ipmr_result {
	struct mr_table		*mrt;
};

/* Big lock, protecting vif table, mrt cache and mroute socket state.
   Note that the changes are semaphored via rtnl_lock.
 */

static DEFINE_RWLOCK(mrt_lock);

/*
 *	Multicast router control variables
 */

#define VIF_EXISTS(_mrt, _idx) ((_mrt)->vif_table[_idx].dev != NULL)

/* Special spinlock for queue of unresolved entries */
static DEFINE_SPINLOCK(mfc_unres_lock);

/* We return to original Alan's scheme. Hash table of resolved
   entries is changed only in process context and protected
   with weak lock mrt_lock. Queue of unresolved entries is protected
   with strong spinlock mfc_unres_lock.

   In this case data path is free of exclusive locks at all.
 */

static struct kmem_cache *mrt_cachep __read_mostly;

static struct mr_table *ipmr_new_table(struct net *net, u32 id);
static int ip_mr_forward(struct net *net, struct mr_table *mrt,
			 struct sk_buff *skb, struct mfc_cache *cache,
			 int local);
static int ipmr_cache_report(struct mr_table *mrt,
			     struct sk_buff *pkt, vifi_t vifi, int assert);
static int __ipmr_fill_mroute(struct mr_table *mrt, struct sk_buff *skb,
			      struct mfc_cache *c, struct rtmsg *rtm);
static void ipmr_expire_process(unsigned long arg);

#ifdef CONFIG_IP_MROUTE_MULTIPLE_TABLES
#define ipmr_for_each_table(mrt, net) \
	list_for_each_entry_rcu(mrt, &net->ipv4.mr_tables, list)

static struct mr_table *ipmr_get_table(struct net *net, u32 id)
{
	struct mr_table *mrt;

	ipmr_for_each_table(mrt, net) {
		if (mrt->id == id)
			return mrt;
	}
	return NULL;
}

static int ipmr_fib_lookup(struct net *net, struct flowi *flp,
			   struct mr_table **mrt)
{
	struct ipmr_result res;
	struct fib_lookup_arg arg = { .result = &res, };
	int err;

	err = fib_rules_lookup(net->ipv4.mr_rules_ops, flp, 0, &arg);
	if (err < 0)
		return err;
	*mrt = res.mrt;
	return 0;
}

static int ipmr_rule_action(struct fib_rule *rule, struct flowi *flp,
			    int flags, struct fib_lookup_arg *arg)
{
	struct ipmr_result *res = arg->result;
	struct mr_table *mrt;

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

	mrt = ipmr_get_table(rule->fr_net, rule->table);
	if (mrt == NULL)
		return -EAGAIN;
	res->mrt = mrt;
	return 0;
}

static int ipmr_rule_match(struct fib_rule *rule, struct flowi *fl, int flags)
{
	return 1;
}

static const struct nla_policy ipmr_rule_policy[FRA_MAX + 1] = {
	FRA_GENERIC_POLICY,
};

static int ipmr_rule_configure(struct fib_rule *rule, struct sk_buff *skb,
			       struct fib_rule_hdr *frh, struct nlattr **tb)
{
	return 0;
}

static int ipmr_rule_compare(struct fib_rule *rule, struct fib_rule_hdr *frh,
			     struct nlattr **tb)
{
	return 1;
}

static int ipmr_rule_fill(struct fib_rule *rule, struct sk_buff *skb,
			  struct fib_rule_hdr *frh)
{
	frh->dst_len = 0;
	frh->src_len = 0;
	frh->tos     = 0;
	return 0;
}

static const struct fib_rules_ops __net_initdata ipmr_rules_ops_template = {
	.family		= RTNL_FAMILY_IPMR,
	.rule_size	= sizeof(struct ipmr_rule),
	.addr_size	= sizeof(u32),
	.action		= ipmr_rule_action,
	.match		= ipmr_rule_match,
	.configure	= ipmr_rule_configure,
	.compare	= ipmr_rule_compare,
	.default_pref	= fib_default_rule_pref,
	.fill		= ipmr_rule_fill,
	.nlgroup	= RTNLGRP_IPV4_RULE,
	.policy		= ipmr_rule_policy,
	.owner		= THIS_MODULE,
};

static int __net_init ipmr_rules_init(struct net *net)
{
	struct fib_rules_ops *ops;
	struct mr_table *mrt;
	int err;

	ops = fib_rules_register(&ipmr_rules_ops_template, net);
	if (IS_ERR(ops))
		return PTR_ERR(ops);

	INIT_LIST_HEAD(&net->ipv4.mr_tables);

	mrt = ipmr_new_table(net, RT_TABLE_DEFAULT);
	if (mrt == NULL) {
		err = -ENOMEM;
		goto err1;
	}

	err = fib_default_rule_add(ops, 0x7fff, RT_TABLE_DEFAULT, 0);
	if (err < 0)
		goto err2;

	net->ipv4.mr_rules_ops = ops;
	return 0;

err2:
	kfree(mrt);
err1:
	fib_rules_unregister(ops);
	return err;
}

static void __net_exit ipmr_rules_exit(struct net *net)
{
	struct mr_table *mrt, *next;

	list_for_each_entry_safe(mrt, next, &net->ipv4.mr_tables, list) {
		list_del(&mrt->list);
		kfree(mrt);
	}
	fib_rules_unregister(net->ipv4.mr_rules_ops);
}
#else
#define ipmr_for_each_table(mrt, net) \
	for (mrt = net->ipv4.mrt; mrt; mrt = NULL)

static struct mr_table *ipmr_get_table(struct net *net, u32 id)
{
	return net->ipv4.mrt;
}

static int ipmr_fib_lookup(struct net *net, struct flowi *flp,
			   struct mr_table **mrt)
{
	*mrt = net->ipv4.mrt;
	return 0;
}

static int __net_init ipmr_rules_init(struct net *net)
{
	net->ipv4.mrt = ipmr_new_table(net, RT_TABLE_DEFAULT);
	return net->ipv4.mrt ? 0 : -ENOMEM;
}

static void __net_exit ipmr_rules_exit(struct net *net)
{
	kfree(net->ipv4.mrt);
}
#endif

static struct mr_table *ipmr_new_table(struct net *net, u32 id)
{
	struct mr_table *mrt;
	unsigned int i;

	mrt = ipmr_get_table(net, id);
	if (mrt != NULL)
		return mrt;

	mrt = kzalloc(sizeof(*mrt), GFP_KERNEL);
	if (mrt == NULL)
		return NULL;
	write_pnet(&mrt->net, net);
	mrt->id = id;

	/* Forwarding cache */
	for (i = 0; i < MFC_LINES; i++)
		INIT_LIST_HEAD(&mrt->mfc_cache_array[i]);

	INIT_LIST_HEAD(&mrt->mfc_unres_queue);

	setup_timer(&mrt->ipmr_expire_timer, ipmr_expire_process,
		    (unsigned long)mrt);

#ifdef CONFIG_IP_PIMSM
	mrt->mroute_reg_vif_num = -1;
#endif
#ifdef CONFIG_IP_MROUTE_MULTIPLE_TABLES
	list_add_tail_rcu(&mrt->list, &net->ipv4.mr_tables);
#endif
	return mrt;
}

/* Service routines creating virtual interfaces: DVMRP tunnels and PIMREG */

static void ipmr_del_tunnel(struct net_device *dev, struct vifctl *v)
{
	struct net *net = dev_net(dev);

	dev_close(dev);

	dev = __dev_get_by_name(net, "tunl0");
	if (dev) {
		const struct net_device_ops *ops = dev->netdev_ops;
		struct ifreq ifr;
		struct ip_tunnel_parm p;

		memset(&p, 0, sizeof(p));
		p.iph.daddr = v->vifc_rmt_addr.s_addr;
		p.iph.saddr = v->vifc_lcl_addr.s_addr;
		p.iph.version = 4;
		p.iph.ihl = 5;
		p.iph.protocol = IPPROTO_IPIP;
		sprintf(p.name, "dvmrp%d", v->vifc_vifi);
		ifr.ifr_ifru.ifru_data = (__force void __user *)&p;

		if (ops->ndo_do_ioctl) {
			mm_segment_t oldfs = get_fs();

			set_fs(KERNEL_DS);
			ops->ndo_do_ioctl(dev, &ifr, SIOCDELTUNNEL);
			set_fs(oldfs);
		}
	}
}

static
struct net_device *ipmr_new_tunnel(struct net *net, struct vifctl *v)
{
	struct net_device  *dev;

	dev = __dev_get_by_name(net, "tunl0");

	if (dev) {
		const struct net_device_ops *ops = dev->netdev_ops;
		int err;
		struct ifreq ifr;
		struct ip_tunnel_parm p;
		struct in_device  *in_dev;

		memset(&p, 0, sizeof(p));
		p.iph.daddr = v->vifc_rmt_addr.s_addr;
		p.iph.saddr = v->vifc_lcl_addr.s_addr;
		p.iph.version = 4;
		p.iph.ihl = 5;
		p.iph.protocol = IPPROTO_IPIP;
		sprintf(p.name, "dvmrp%d", v->vifc_vifi);
		ifr.ifr_ifru.ifru_data = (__force void __user *)&p;

		if (ops->ndo_do_ioctl) {
			mm_segment_t oldfs = get_fs();

			set_fs(KERNEL_DS);
			err = ops->ndo_do_ioctl(dev, &ifr, SIOCADDTUNNEL);
			set_fs(oldfs);
		} else
			err = -EOPNOTSUPP;

		dev = NULL;

		if (err == 0 &&
		    (dev = __dev_get_by_name(net, p.name)) != NULL) {
			dev->flags |= IFF_MULTICAST;

			in_dev = __in_dev_get_rtnl(dev);
			if (in_dev == NULL)
				goto failure;

			ipv4_devconf_setall(in_dev);
			IPV4_DEVCONF(in_dev->cnf, RP_FILTER) = 0;

			if (dev_open(dev))
				goto failure;
			dev_hold(dev);
		}
	}
	return dev;

failure:
	/* allow the register to be completed before unregistering. */
	rtnl_unlock();
	rtnl_lock();

	unregister_netdevice(dev);
	return NULL;
}

#ifdef CONFIG_IP_PIMSM

static netdev_tx_t reg_vif_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct net *net = dev_net(dev);
	struct mr_table *mrt;
	struct flowi fl = {
		.oif		= dev->ifindex,
		.iif		= skb->skb_iif,
		.mark		= skb->mark,
	};
	int err;

	err = ipmr_fib_lookup(net, &fl, &mrt);
	if (err < 0) {
		kfree_skb(skb);
		return err;
	}

	read_lock(&mrt_lock);
	dev->stats.tx_bytes += skb->len;
	dev->stats.tx_packets++;
	ipmr_cache_report(mrt, skb, mrt->mroute_reg_vif_num, IGMPMSG_WHOLEPKT);
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
	dev->mtu		= ETH_DATA_LEN - sizeof(struct iphdr) - 8;
	dev->flags		= IFF_NOARP;
	dev->netdev_ops		= &reg_vif_netdev_ops,
	dev->destructor		= free_netdev;
	dev->features		|= NETIF_F_NETNS_LOCAL;
}

static struct net_device *ipmr_reg_vif(struct net *net, struct mr_table *mrt)
{
	struct net_device *dev;
	struct in_device *in_dev;
	char name[IFNAMSIZ];

	if (mrt->id == RT_TABLE_DEFAULT)
		sprintf(name, "pimreg");
	else
		sprintf(name, "pimreg%u", mrt->id);

	dev = alloc_netdev(0, name, reg_vif_setup);

	if (dev == NULL)
		return NULL;

	dev_net_set(dev, net);

	if (register_netdevice(dev)) {
		free_netdev(dev);
		return NULL;
	}
	dev->iflink = 0;

	rcu_read_lock();
	if ((in_dev = __in_dev_get_rcu(dev)) == NULL) {
		rcu_read_unlock();
		goto failure;
	}

	ipv4_devconf_setall(in_dev);
	IPV4_DEVCONF(in_dev->cnf, RP_FILTER) = 0;
	rcu_read_unlock();

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
 *	@notify: Set to 1, if the caller is a notifier_call
 */

static int vif_delete(struct mr_table *mrt, int vifi, int notify,
		      struct list_head *head)
{
	struct vif_device *v;
	struct net_device *dev;
	struct in_device *in_dev;

	if (vifi < 0 || vifi >= mrt->maxvif)
		return -EADDRNOTAVAIL;

	v = &mrt->vif_table[vifi];

	write_lock_bh(&mrt_lock);
	dev = v->dev;
	v->dev = NULL;

	if (!dev) {
		write_unlock_bh(&mrt_lock);
		return -EADDRNOTAVAIL;
	}

#ifdef CONFIG_IP_PIMSM
	if (vifi == mrt->mroute_reg_vif_num)
		mrt->mroute_reg_vif_num = -1;
#endif

	if (vifi+1 == mrt->maxvif) {
		int tmp;
		for (tmp=vifi-1; tmp>=0; tmp--) {
			if (VIF_EXISTS(mrt, tmp))
				break;
		}
		mrt->maxvif = tmp+1;
	}

	write_unlock_bh(&mrt_lock);

	dev_set_allmulti(dev, -1);

	if ((in_dev = __in_dev_get_rtnl(dev)) != NULL) {
		IPV4_DEVCONF(in_dev->cnf, MC_FORWARDING)--;
		ip_rt_multicast_event(in_dev);
	}

	if (v->flags&(VIFF_TUNNEL|VIFF_REGISTER) && !notify)
		unregister_netdevice_queue(dev, head);

	dev_put(dev);
	return 0;
}

static inline void ipmr_cache_free(struct mfc_cache *c)
{
	kmem_cache_free(mrt_cachep, c);
}

/* Destroy an unresolved cache entry, killing queued skbs
   and reporting error to netlink readers.
 */

static void ipmr_destroy_unres(struct mr_table *mrt, struct mfc_cache *c)
{
	struct net *net = read_pnet(&mrt->net);
	struct sk_buff *skb;
	struct nlmsgerr *e;

	atomic_dec(&mrt->cache_resolve_queue_len);

	while ((skb = skb_dequeue(&c->mfc_un.unres.unresolved))) {
		if (ip_hdr(skb)->version == 0) {
			struct nlmsghdr *nlh = (struct nlmsghdr *)skb_pull(skb, sizeof(struct iphdr));
			nlh->nlmsg_type = NLMSG_ERROR;
			nlh->nlmsg_len = NLMSG_LENGTH(sizeof(struct nlmsgerr));
			skb_trim(skb, nlh->nlmsg_len);
			e = NLMSG_DATA(nlh);
			e->error = -ETIMEDOUT;
			memset(&e->msg, 0, sizeof(e->msg));

			rtnl_unicast(skb, net, NETLINK_CB(skb).pid);
		} else
			kfree_skb(skb);
	}

	ipmr_cache_free(c);
}


/* Timer process for the unresolved queue. */

static void ipmr_expire_process(unsigned long arg)
{
	struct mr_table *mrt = (struct mr_table *)arg;
	unsigned long now;
	unsigned long expires;
	struct mfc_cache *c, *next;

	if (!spin_trylock(&mfc_unres_lock)) {
		mod_timer(&mrt->ipmr_expire_timer, jiffies+HZ/10);
		return;
	}

	if (list_empty(&mrt->mfc_unres_queue))
		goto out;

	now = jiffies;
	expires = 10*HZ;

	list_for_each_entry_safe(c, next, &mrt->mfc_unres_queue, list) {
		if (time_after(c->mfc_un.unres.expires, now)) {
			unsigned long interval = c->mfc_un.unres.expires - now;
			if (interval < expires)
				expires = interval;
			continue;
		}

		list_del(&c->list);
		ipmr_destroy_unres(mrt, c);
	}

	if (!list_empty(&mrt->mfc_unres_queue))
		mod_timer(&mrt->ipmr_expire_timer, jiffies + expires);

out:
	spin_unlock(&mfc_unres_lock);
}

/* Fill oifs list. It is called under write locked mrt_lock. */

static void ipmr_update_thresholds(struct mr_table *mrt, struct mfc_cache *cache,
				   unsigned char *ttls)
{
	int vifi;

	cache->mfc_un.res.minvif = MAXVIFS;
	cache->mfc_un.res.maxvif = 0;
	memset(cache->mfc_un.res.ttls, 255, MAXVIFS);

	for (vifi = 0; vifi < mrt->maxvif; vifi++) {
		if (VIF_EXISTS(mrt, vifi) &&
		    ttls[vifi] && ttls[vifi] < 255) {
			cache->mfc_un.res.ttls[vifi] = ttls[vifi];
			if (cache->mfc_un.res.minvif > vifi)
				cache->mfc_un.res.minvif = vifi;
			if (cache->mfc_un.res.maxvif <= vifi)
				cache->mfc_un.res.maxvif = vifi + 1;
		}
	}
}

static int vif_add(struct net *net, struct mr_table *mrt,
		   struct vifctl *vifc, int mrtsock)
{
	int vifi = vifc->vifc_vifi;
	struct vif_device *v = &mrt->vif_table[vifi];
	struct net_device *dev;
	struct in_device *in_dev;
	int err;

	/* Is vif busy ? */
	if (VIF_EXISTS(mrt, vifi))
		return -EADDRINUSE;

	switch (vifc->vifc_flags) {
#ifdef CONFIG_IP_PIMSM
	case VIFF_REGISTER:
		/*
		 * Special Purpose VIF in PIM
		 * All the packets will be sent to the daemon
		 */
		if (mrt->mroute_reg_vif_num >= 0)
			return -EADDRINUSE;
		dev = ipmr_reg_vif(net, mrt);
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
	case VIFF_TUNNEL:
		dev = ipmr_new_tunnel(net, vifc);
		if (!dev)
			return -ENOBUFS;
		err = dev_set_allmulti(dev, 1);
		if (err) {
			ipmr_del_tunnel(dev, vifc);
			dev_put(dev);
			return err;
		}
		break;

	case VIFF_USE_IFINDEX:
	case 0:
		if (vifc->vifc_flags == VIFF_USE_IFINDEX) {
			dev = dev_get_by_index(net, vifc->vifc_lcl_ifindex);
			if (dev && dev->ip_ptr == NULL) {
				dev_put(dev);
				return -EADDRNOTAVAIL;
			}
		} else
			dev = ip_dev_find(net, vifc->vifc_lcl_addr.s_addr);

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

	if ((in_dev = __in_dev_get_rtnl(dev)) == NULL) {
		dev_put(dev);
		return -EADDRNOTAVAIL;
	}
	IPV4_DEVCONF(in_dev->cnf, MC_FORWARDING)++;
	ip_rt_multicast_event(in_dev);

	/*
	 *	Fill in the VIF structures
	 */
	v->rate_limit = vifc->vifc_rate_limit;
	v->local = vifc->vifc_lcl_addr.s_addr;
	v->remote = vifc->vifc_rmt_addr.s_addr;
	v->flags = vifc->vifc_flags;
	if (!mrtsock)
		v->flags |= VIFF_STATIC;
	v->threshold = vifc->vifc_threshold;
	v->bytes_in = 0;
	v->bytes_out = 0;
	v->pkt_in = 0;
	v->pkt_out = 0;
	v->link = dev->ifindex;
	if (v->flags&(VIFF_TUNNEL|VIFF_REGISTER))
		v->link = dev->iflink;

	/* And finish update writing critical data */
	write_lock_bh(&mrt_lock);
	v->dev = dev;
#ifdef CONFIG_IP_PIMSM
	if (v->flags&VIFF_REGISTER)
		mrt->mroute_reg_vif_num = vifi;
#endif
	if (vifi+1 > mrt->maxvif)
		mrt->maxvif = vifi+1;
	write_unlock_bh(&mrt_lock);
	return 0;
}

static struct mfc_cache *ipmr_cache_find(struct mr_table *mrt,
					 __be32 origin,
					 __be32 mcastgrp)
{
	int line = MFC_HASH(mcastgrp, origin);
	struct mfc_cache *c;

	list_for_each_entry(c, &mrt->mfc_cache_array[line], list) {
		if (c->mfc_origin == origin && c->mfc_mcastgrp == mcastgrp)
			return c;
	}
	return NULL;
}

/*
 *	Allocate a multicast cache entry
 */
static struct mfc_cache *ipmr_cache_alloc(void)
{
	struct mfc_cache *c = kmem_cache_zalloc(mrt_cachep, GFP_KERNEL);
	if (c == NULL)
		return NULL;
	c->mfc_un.res.minvif = MAXVIFS;
	return c;
}

static struct mfc_cache *ipmr_cache_alloc_unres(void)
{
	struct mfc_cache *c = kmem_cache_zalloc(mrt_cachep, GFP_ATOMIC);
	if (c == NULL)
		return NULL;
	skb_queue_head_init(&c->mfc_un.unres.unresolved);
	c->mfc_un.unres.expires = jiffies + 10*HZ;
	return c;
}

/*
 *	A cache entry has gone into a resolved state from queued
 */

static void ipmr_cache_resolve(struct net *net, struct mr_table *mrt,
			       struct mfc_cache *uc, struct mfc_cache *c)
{
	struct sk_buff *skb;
	struct nlmsgerr *e;

	/*
	 *	Play the pending entries through our router
	 */

	while ((skb = __skb_dequeue(&uc->mfc_un.unres.unresolved))) {
		if (ip_hdr(skb)->version == 0) {
			struct nlmsghdr *nlh = (struct nlmsghdr *)skb_pull(skb, sizeof(struct iphdr));

			if (__ipmr_fill_mroute(mrt, skb, c, NLMSG_DATA(nlh)) > 0) {
				nlh->nlmsg_len = (skb_tail_pointer(skb) -
						  (u8 *)nlh);
			} else {
				nlh->nlmsg_type = NLMSG_ERROR;
				nlh->nlmsg_len = NLMSG_LENGTH(sizeof(struct nlmsgerr));
				skb_trim(skb, nlh->nlmsg_len);
				e = NLMSG_DATA(nlh);
				e->error = -EMSGSIZE;
				memset(&e->msg, 0, sizeof(e->msg));
			}

			rtnl_unicast(skb, net, NETLINK_CB(skb).pid);
		} else
			ip_mr_forward(net, mrt, skb, c, 0);
	}
}

/*
 *	Bounce a cache query up to mrouted. We could use netlink for this but mrouted
 *	expects the following bizarre scheme.
 *
 *	Called under mrt_lock.
 */

static int ipmr_cache_report(struct mr_table *mrt,
			     struct sk_buff *pkt, vifi_t vifi, int assert)
{
	struct sk_buff *skb;
	const int ihl = ip_hdrlen(pkt);
	struct igmphdr *igmp;
	struct igmpmsg *msg;
	int ret;

#ifdef CONFIG_IP_PIMSM
	if (assert == IGMPMSG_WHOLEPKT)
		skb = skb_realloc_headroom(pkt, sizeof(struct iphdr));
	else
#endif
		skb = alloc_skb(128, GFP_ATOMIC);

	if (!skb)
		return -ENOBUFS;

#ifdef CONFIG_IP_PIMSM
	if (assert == IGMPMSG_WHOLEPKT) {
		/* Ugly, but we have no choice with this interface.
		   Duplicate old header, fix ihl, length etc.
		   And all this only to mangle msg->im_msgtype and
		   to set msg->im_mbz to "mbz" :-)
		 */
		skb_push(skb, sizeof(struct iphdr));
		skb_reset_network_header(skb);
		skb_reset_transport_header(skb);
		msg = (struct igmpmsg *)skb_network_header(skb);
		memcpy(msg, skb_network_header(pkt), sizeof(struct iphdr));
		msg->im_msgtype = IGMPMSG_WHOLEPKT;
		msg->im_mbz = 0;
		msg->im_vif = mrt->mroute_reg_vif_num;
		ip_hdr(skb)->ihl = sizeof(struct iphdr) >> 2;
		ip_hdr(skb)->tot_len = htons(ntohs(ip_hdr(pkt)->tot_len) +
					     sizeof(struct iphdr));
	} else
#endif
	{

	/*
	 *	Copy the IP header
	 */

	skb->network_header = skb->tail;
	skb_put(skb, ihl);
	skb_copy_to_linear_data(skb, pkt->data, ihl);
	ip_hdr(skb)->protocol = 0;			/* Flag to the kernel this is a route add */
	msg = (struct igmpmsg *)skb_network_header(skb);
	msg->im_vif = vifi;
	skb_dst_set(skb, dst_clone(skb_dst(pkt)));

	/*
	 *	Add our header
	 */

	igmp=(struct igmphdr *)skb_put(skb, sizeof(struct igmphdr));
	igmp->type	=
	msg->im_msgtype = assert;
	igmp->code 	=	0;
	ip_hdr(skb)->tot_len = htons(skb->len);			/* Fix the length */
	skb->transport_header = skb->network_header;
	}

	if (mrt->mroute_sk == NULL) {
		kfree_skb(skb);
		return -EINVAL;
	}

	/*
	 *	Deliver to mrouted
	 */
	ret = sock_queue_rcv_skb(mrt->mroute_sk, skb);
	if (ret < 0) {
		if (net_ratelimit())
			printk(KERN_WARNING "mroute: pending queue full, dropping entries.\n");
		kfree_skb(skb);
	}

	return ret;
}

/*
 *	Queue a packet for resolution. It gets locked cache entry!
 */

static int
ipmr_cache_unresolved(struct mr_table *mrt, vifi_t vifi, struct sk_buff *skb)
{
	bool found = false;
	int err;
	struct mfc_cache *c;
	const struct iphdr *iph = ip_hdr(skb);

	spin_lock_bh(&mfc_unres_lock);
	list_for_each_entry(c, &mrt->mfc_unres_queue, list) {
		if (c->mfc_mcastgrp == iph->daddr &&
		    c->mfc_origin == iph->saddr) {
			found = true;
			break;
		}
	}

	if (!found) {
		/*
		 *	Create a new entry if allowable
		 */

		if (atomic_read(&mrt->cache_resolve_queue_len) >= 10 ||
		    (c = ipmr_cache_alloc_unres()) == NULL) {
			spin_unlock_bh(&mfc_unres_lock);

			kfree_skb(skb);
			return -ENOBUFS;
		}

		/*
		 *	Fill in the new cache entry
		 */
		c->mfc_parent	= -1;
		c->mfc_origin	= iph->saddr;
		c->mfc_mcastgrp	= iph->daddr;

		/*
		 *	Reflect first query at mrouted.
		 */
		err = ipmr_cache_report(mrt, skb, vifi, IGMPMSG_NOCACHE);
		if (err < 0) {
			/* If the report failed throw the cache entry
			   out - Brad Parker
			 */
			spin_unlock_bh(&mfc_unres_lock);

			ipmr_cache_free(c);
			kfree_skb(skb);
			return err;
		}

		atomic_inc(&mrt->cache_resolve_queue_len);
		list_add(&c->list, &mrt->mfc_unres_queue);

		if (atomic_read(&mrt->cache_resolve_queue_len) == 1)
			mod_timer(&mrt->ipmr_expire_timer, c->mfc_un.unres.expires);
	}

	/*
	 *	See if we can append the packet
	 */
	if (c->mfc_un.unres.unresolved.qlen>3) {
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
 *	MFC cache manipulation by user space mroute daemon
 */

static int ipmr_mfc_delete(struct mr_table *mrt, struct mfcctl *mfc)
{
	int line;
	struct mfc_cache *c, *next;

	line = MFC_HASH(mfc->mfcc_mcastgrp.s_addr, mfc->mfcc_origin.s_addr);

	list_for_each_entry_safe(c, next, &mrt->mfc_cache_array[line], list) {
		if (c->mfc_origin == mfc->mfcc_origin.s_addr &&
		    c->mfc_mcastgrp == mfc->mfcc_mcastgrp.s_addr) {
			write_lock_bh(&mrt_lock);
			list_del(&c->list);
			write_unlock_bh(&mrt_lock);

			ipmr_cache_free(c);
			return 0;
		}
	}
	return -ENOENT;
}

static int ipmr_mfc_add(struct net *net, struct mr_table *mrt,
			struct mfcctl *mfc, int mrtsock)
{
	bool found = false;
	int line;
	struct mfc_cache *uc, *c;

	if (mfc->mfcc_parent >= MAXVIFS)
		return -ENFILE;

	line = MFC_HASH(mfc->mfcc_mcastgrp.s_addr, mfc->mfcc_origin.s_addr);

	list_for_each_entry(c, &mrt->mfc_cache_array[line], list) {
		if (c->mfc_origin == mfc->mfcc_origin.s_addr &&
		    c->mfc_mcastgrp == mfc->mfcc_mcastgrp.s_addr) {
			found = true;
			break;
		}
	}

	if (found) {
		write_lock_bh(&mrt_lock);
		c->mfc_parent = mfc->mfcc_parent;
		ipmr_update_thresholds(mrt, c, mfc->mfcc_ttls);
		if (!mrtsock)
			c->mfc_flags |= MFC_STATIC;
		write_unlock_bh(&mrt_lock);
		return 0;
	}

	if (!ipv4_is_multicast(mfc->mfcc_mcastgrp.s_addr))
		return -EINVAL;

	c = ipmr_cache_alloc();
	if (c == NULL)
		return -ENOMEM;

	c->mfc_origin = mfc->mfcc_origin.s_addr;
	c->mfc_mcastgrp = mfc->mfcc_mcastgrp.s_addr;
	c->mfc_parent = mfc->mfcc_parent;
	ipmr_update_thresholds(mrt, c, mfc->mfcc_ttls);
	if (!mrtsock)
		c->mfc_flags |= MFC_STATIC;

	write_lock_bh(&mrt_lock);
	list_add(&c->list, &mrt->mfc_cache_array[line]);
	write_unlock_bh(&mrt_lock);

	/*
	 *	Check to see if we resolved a queued list. If so we
	 *	need to send on the frames and tidy up.
	 */
	found = false;
	spin_lock_bh(&mfc_unres_lock);
	list_for_each_entry(uc, &mrt->mfc_unres_queue, list) {
		if (uc->mfc_origin == c->mfc_origin &&
		    uc->mfc_mcastgrp == c->mfc_mcastgrp) {
			list_del(&uc->list);
			atomic_dec(&mrt->cache_resolve_queue_len);
			found = true;
			break;
		}
	}
	if (list_empty(&mrt->mfc_unres_queue))
		del_timer(&mrt->ipmr_expire_timer);
	spin_unlock_bh(&mfc_unres_lock);

	if (found) {
		ipmr_cache_resolve(net, mrt, uc, c);
		ipmr_cache_free(uc);
	}
	return 0;
}

/*
 *	Close the multicast socket, and clear the vif tables etc
 */

static void mroute_clean_tables(struct mr_table *mrt)
{
	int i;
	LIST_HEAD(list);
	struct mfc_cache *c, *next;

	/*
	 *	Shut down all active vif entries
	 */
	for (i = 0; i < mrt->maxvif; i++) {
		if (!(mrt->vif_table[i].flags&VIFF_STATIC))
			vif_delete(mrt, i, 0, &list);
	}
	unregister_netdevice_many(&list);

	/*
	 *	Wipe the cache
	 */
	for (i = 0; i < MFC_LINES; i++) {
		list_for_each_entry_safe(c, next, &mrt->mfc_cache_array[i], list) {
			if (c->mfc_flags&MFC_STATIC)
				continue;
			write_lock_bh(&mrt_lock);
			list_del(&c->list);
			write_unlock_bh(&mrt_lock);

			ipmr_cache_free(c);
		}
	}

	if (atomic_read(&mrt->cache_resolve_queue_len) != 0) {
		spin_lock_bh(&mfc_unres_lock);
		list_for_each_entry_safe(c, next, &mrt->mfc_unres_queue, list) {
			list_del(&c->list);
			ipmr_destroy_unres(mrt, c);
		}
		spin_unlock_bh(&mfc_unres_lock);
	}
}

static void mrtsock_destruct(struct sock *sk)
{
	struct net *net = sock_net(sk);
	struct mr_table *mrt;

	rtnl_lock();
	ipmr_for_each_table(mrt, net) {
		if (sk == mrt->mroute_sk) {
			IPV4_DEVCONF_ALL(net, MC_FORWARDING)--;

			write_lock_bh(&mrt_lock);
			mrt->mroute_sk = NULL;
			write_unlock_bh(&mrt_lock);

			mroute_clean_tables(mrt);
		}
	}
	rtnl_unlock();
}

/*
 *	Socket options and virtual interface manipulation. The whole
 *	virtual interface system is a complete heap, but unfortunately
 *	that's how BSD mrouted happens to think. Maybe one day with a proper
 *	MOSPF/PIM router set up we can clean this up.
 */

int ip_mroute_setsockopt(struct sock *sk, int optname, char __user *optval, unsigned int optlen)
{
	int ret;
	struct vifctl vif;
	struct mfcctl mfc;
	struct net *net = sock_net(sk);
	struct mr_table *mrt;

	mrt = ipmr_get_table(net, raw_sk(sk)->ipmr_table ? : RT_TABLE_DEFAULT);
	if (mrt == NULL)
		return -ENOENT;

	if (optname != MRT_INIT) {
		if (sk != mrt->mroute_sk && !capable(CAP_NET_ADMIN))
			return -EACCES;
	}

	switch (optname) {
	case MRT_INIT:
		if (sk->sk_type != SOCK_RAW ||
		    inet_sk(sk)->inet_num != IPPROTO_IGMP)
			return -EOPNOTSUPP;
		if (optlen != sizeof(int))
			return -ENOPROTOOPT;

		rtnl_lock();
		if (mrt->mroute_sk) {
			rtnl_unlock();
			return -EADDRINUSE;
		}

		ret = ip_ra_control(sk, 1, mrtsock_destruct);
		if (ret == 0) {
			write_lock_bh(&mrt_lock);
			mrt->mroute_sk = sk;
			write_unlock_bh(&mrt_lock);

			IPV4_DEVCONF_ALL(net, MC_FORWARDING)++;
		}
		rtnl_unlock();
		return ret;
	case MRT_DONE:
		if (sk != mrt->mroute_sk)
			return -EACCES;
		return ip_ra_control(sk, 0, NULL);
	case MRT_ADD_VIF:
	case MRT_DEL_VIF:
		if (optlen != sizeof(vif))
			return -EINVAL;
		if (copy_from_user(&vif, optval, sizeof(vif)))
			return -EFAULT;
		if (vif.vifc_vifi >= MAXVIFS)
			return -ENFILE;
		rtnl_lock();
		if (optname == MRT_ADD_VIF) {
			ret = vif_add(net, mrt, &vif, sk == mrt->mroute_sk);
		} else {
			ret = vif_delete(mrt, vif.vifc_vifi, 0, NULL);
		}
		rtnl_unlock();
		return ret;

		/*
		 *	Manipulate the forwarding caches. These live
		 *	in a sort of kernel/user symbiosis.
		 */
	case MRT_ADD_MFC:
	case MRT_DEL_MFC:
		if (optlen != sizeof(mfc))
			return -EINVAL;
		if (copy_from_user(&mfc, optval, sizeof(mfc)))
			return -EFAULT;
		rtnl_lock();
		if (optname == MRT_DEL_MFC)
			ret = ipmr_mfc_delete(mrt, &mfc);
		else
			ret = ipmr_mfc_add(net, mrt, &mfc, sk == mrt->mroute_sk);
		rtnl_unlock();
		return ret;
		/*
		 *	Control PIM assert.
		 */
	case MRT_ASSERT:
	{
		int v;
		if (get_user(v,(int __user *)optval))
			return -EFAULT;
		mrt->mroute_do_assert = (v) ? 1 : 0;
		return 0;
	}
#ifdef CONFIG_IP_PIMSM
	case MRT_PIM:
	{
		int v;

		if (get_user(v,(int __user *)optval))
			return -EFAULT;
		v = (v) ? 1 : 0;

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
#ifdef CONFIG_IP_MROUTE_MULTIPLE_TABLES
	case MRT_TABLE:
	{
		u32 v;

		if (optlen != sizeof(u32))
			return -EINVAL;
		if (get_user(v, (u32 __user *)optval))
			return -EFAULT;
		if (sk == mrt->mroute_sk)
			return -EBUSY;

		rtnl_lock();
		ret = 0;
		if (!ipmr_new_table(net, v))
			ret = -ENOMEM;
		raw_sk(sk)->ipmr_table = v;
		rtnl_unlock();
		return ret;
	}
#endif
	/*
	 *	Spurious command, or MRT_VERSION which you cannot
	 *	set.
	 */
	default:
		return -ENOPROTOOPT;
	}
}

/*
 *	Getsock opt support for the multicast routing system.
 */

int ip_mroute_getsockopt(struct sock *sk, int optname, char __user *optval, int __user *optlen)
{
	int olr;
	int val;
	struct net *net = sock_net(sk);
	struct mr_table *mrt;

	mrt = ipmr_get_table(net, raw_sk(sk)->ipmr_table ? : RT_TABLE_DEFAULT);
	if (mrt == NULL)
		return -ENOENT;

	if (optname != MRT_VERSION &&
#ifdef CONFIG_IP_PIMSM
	   optname!=MRT_PIM &&
#endif
	   optname!=MRT_ASSERT)
		return -ENOPROTOOPT;

	if (get_user(olr, optlen))
		return -EFAULT;

	olr = min_t(unsigned int, olr, sizeof(int));
	if (olr < 0)
		return -EINVAL;

	if (put_user(olr, optlen))
		return -EFAULT;
	if (optname == MRT_VERSION)
		val = 0x0305;
#ifdef CONFIG_IP_PIMSM
	else if (optname == MRT_PIM)
		val = mrt->mroute_do_pim;
#endif
	else
		val = mrt->mroute_do_assert;
	if (copy_to_user(optval, &val, olr))
		return -EFAULT;
	return 0;
}

/*
 *	The IP multicast ioctl support routines.
 */

int ipmr_ioctl(struct sock *sk, int cmd, void __user *arg)
{
	struct sioc_sg_req sr;
	struct sioc_vif_req vr;
	struct vif_device *vif;
	struct mfc_cache *c;
	struct net *net = sock_net(sk);
	struct mr_table *mrt;

	mrt = ipmr_get_table(net, raw_sk(sk)->ipmr_table ? : RT_TABLE_DEFAULT);
	if (mrt == NULL)
		return -ENOENT;

	switch (cmd) {
	case SIOCGETVIFCNT:
		if (copy_from_user(&vr, arg, sizeof(vr)))
			return -EFAULT;
		if (vr.vifi >= mrt->maxvif)
			return -EINVAL;
		read_lock(&mrt_lock);
		vif = &mrt->vif_table[vr.vifi];
		if (VIF_EXISTS(mrt, vr.vifi)) {
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
	case SIOCGETSGCNT:
		if (copy_from_user(&sr, arg, sizeof(sr)))
			return -EFAULT;

		read_lock(&mrt_lock);
		c = ipmr_cache_find(mrt, sr.src.s_addr, sr.grp.s_addr);
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


static int ipmr_device_event(struct notifier_block *this, unsigned long event, void *ptr)
{
	struct net_device *dev = ptr;
	struct net *net = dev_net(dev);
	struct mr_table *mrt;
	struct vif_device *v;
	int ct;
	LIST_HEAD(list);

	if (event != NETDEV_UNREGISTER)
		return NOTIFY_DONE;

	ipmr_for_each_table(mrt, net) {
		v = &mrt->vif_table[0];
		for (ct = 0; ct < mrt->maxvif; ct++, v++) {
			if (v->dev == dev)
				vif_delete(mrt, ct, 1, &list);
		}
	}
	unregister_netdevice_many(&list);
	return NOTIFY_DONE;
}


static struct notifier_block ip_mr_notifier = {
	.notifier_call = ipmr_device_event,
};

/*
 * 	Encapsulate a packet by attaching a valid IPIP header to it.
 *	This avoids tunnel drivers and other mess and gives us the speed so
 *	important for multicast video.
 */

static void ip_encap(struct sk_buff *skb, __be32 saddr, __be32 daddr)
{
	struct iphdr *iph;
	struct iphdr *old_iph = ip_hdr(skb);

	skb_push(skb, sizeof(struct iphdr));
	skb->transport_header = skb->network_header;
	skb_reset_network_header(skb);
	iph = ip_hdr(skb);

	iph->version	= 	4;
	iph->tos	=	old_iph->tos;
	iph->ttl	=	old_iph->ttl;
	iph->frag_off	=	0;
	iph->daddr	=	daddr;
	iph->saddr	=	saddr;
	iph->protocol	=	IPPROTO_IPIP;
	iph->ihl	=	5;
	iph->tot_len	=	htons(skb->len);
	ip_select_ident(iph, skb_dst(skb), NULL);
	ip_send_check(iph);

	memset(&(IPCB(skb)->opt), 0, sizeof(IPCB(skb)->opt));
	nf_reset(skb);
}

static inline int ipmr_forward_finish(struct sk_buff *skb)
{
	struct ip_options * opt	= &(IPCB(skb)->opt);

	IP_INC_STATS_BH(dev_net(skb_dst(skb)->dev), IPSTATS_MIB_OUTFORWDATAGRAMS);

	if (unlikely(opt->optlen))
		ip_forward_options(skb);

	return dst_output(skb);
}

/*
 *	Processing handlers for ipmr_forward
 */

static void ipmr_queue_xmit(struct net *net, struct mr_table *mrt,
			    struct sk_buff *skb, struct mfc_cache *c, int vifi)
{
	const struct iphdr *iph = ip_hdr(skb);
	struct vif_device *vif = &mrt->vif_table[vifi];
	struct net_device *dev;
	struct rtable *rt;
	int    encap = 0;

	if (vif->dev == NULL)
		goto out_free;

#ifdef CONFIG_IP_PIMSM
	if (vif->flags & VIFF_REGISTER) {
		vif->pkt_out++;
		vif->bytes_out += skb->len;
		vif->dev->stats.tx_bytes += skb->len;
		vif->dev->stats.tx_packets++;
		ipmr_cache_report(mrt, skb, vifi, IGMPMSG_WHOLEPKT);
		goto out_free;
	}
#endif

	if (vif->flags&VIFF_TUNNEL) {
		struct flowi fl = { .oif = vif->link,
				    .nl_u = { .ip4_u =
					      { .daddr = vif->remote,
						.saddr = vif->local,
						.tos = RT_TOS(iph->tos) } },
				    .proto = IPPROTO_IPIP };
		if (ip_route_output_key(net, &rt, &fl))
			goto out_free;
		encap = sizeof(struct iphdr);
	} else {
		struct flowi fl = { .oif = vif->link,
				    .nl_u = { .ip4_u =
					      { .daddr = iph->daddr,
						.tos = RT_TOS(iph->tos) } },
				    .proto = IPPROTO_IPIP };
		if (ip_route_output_key(net, &rt, &fl))
			goto out_free;
	}

	dev = rt->dst.dev;

	if (skb->len+encap > dst_mtu(&rt->dst) && (ntohs(iph->frag_off) & IP_DF)) {
		/* Do not fragment multicasts. Alas, IPv4 does not
		   allow to send ICMP, so that packets will disappear
		   to blackhole.
		 */

		IP_INC_STATS_BH(dev_net(dev), IPSTATS_MIB_FRAGFAILS);
		ip_rt_put(rt);
		goto out_free;
	}

	encap += LL_RESERVED_SPACE(dev) + rt->dst.header_len;

	if (skb_cow(skb, encap)) {
		ip_rt_put(rt);
		goto out_free;
	}

	vif->pkt_out++;
	vif->bytes_out += skb->len;

	skb_dst_drop(skb);
	skb_dst_set(skb, &rt->dst);
	ip_decrease_ttl(ip_hdr(skb));

	if (vif->flags & VIFF_TUNNEL) {
		ip_encap(skb, vif->local, vif->remote);
		vif->dev->stats.tx_packets++;
		vif->dev->stats.tx_bytes += skb->len;
	}

	IPCB(skb)->flags |= IPSKB_FORWARDED;

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
	NF_HOOK(NFPROTO_IPV4, NF_INET_FORWARD, skb, skb->dev, dev,
		ipmr_forward_finish);
	return;

out_free:
	kfree_skb(skb);
}

static int ipmr_find_vif(struct mr_table *mrt, struct net_device *dev)
{
	int ct;

	for (ct = mrt->maxvif-1; ct >= 0; ct--) {
		if (mrt->vif_table[ct].dev == dev)
			break;
	}
	return ct;
}

/* "local" means that we should preserve one skb (for local delivery) */

static int ip_mr_forward(struct net *net, struct mr_table *mrt,
			 struct sk_buff *skb, struct mfc_cache *cache,
			 int local)
{
	int psend = -1;
	int vif, ct;

	vif = cache->mfc_parent;
	cache->mfc_un.res.pkt++;
	cache->mfc_un.res.bytes += skb->len;

	/*
	 * Wrong interface: drop packet and (maybe) send PIM assert.
	 */
	if (mrt->vif_table[vif].dev != skb->dev) {
		int true_vifi;

		if (skb_rtable(skb)->fl.iif == 0) {
			goto dont_forward;
		}

		cache->mfc_un.res.wrong_if++;
		true_vifi = ipmr_find_vif(mrt, skb->dev);

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
			ipmr_cache_report(mrt, skb, true_vifi, IGMPMSG_WRONGVIF);
		}
		goto dont_forward;
	}

	mrt->vif_table[vif].pkt_in++;
	mrt->vif_table[vif].bytes_in += skb->len;

	/*
	 *	Forward the frame
	 */
	for (ct = cache->mfc_un.res.maxvif-1; ct >= cache->mfc_un.res.minvif; ct--) {
		if (ip_hdr(skb)->ttl > cache->mfc_un.res.ttls[ct]) {
			if (psend != -1) {
				struct sk_buff *skb2 = skb_clone(skb, GFP_ATOMIC);
				if (skb2)
					ipmr_queue_xmit(net, mrt, skb2, cache,
							psend);
			}
			psend = ct;
		}
	}
	if (psend != -1) {
		if (local) {
			struct sk_buff *skb2 = skb_clone(skb, GFP_ATOMIC);
			if (skb2)
				ipmr_queue_xmit(net, mrt, skb2, cache, psend);
		} else {
			ipmr_queue_xmit(net, mrt, skb, cache, psend);
			return 0;
		}
	}

dont_forward:
	if (!local)
		kfree_skb(skb);
	return 0;
}


/*
 *	Multicast packets for forwarding arrive here
 */

int ip_mr_input(struct sk_buff *skb)
{
	struct mfc_cache *cache;
	struct net *net = dev_net(skb->dev);
	int local = skb_rtable(skb)->rt_flags & RTCF_LOCAL;
	struct mr_table *mrt;
	int err;

	/* Packet is looped back after forward, it should not be
	   forwarded second time, but still can be delivered locally.
	 */
	if (IPCB(skb)->flags&IPSKB_FORWARDED)
		goto dont_forward;

	err = ipmr_fib_lookup(net, &skb_rtable(skb)->fl, &mrt);
	if (err < 0) {
		kfree_skb(skb);
		return err;
	}

	if (!local) {
		    if (IPCB(skb)->opt.router_alert) {
			    if (ip_call_ra_chain(skb))
				    return 0;
		    } else if (ip_hdr(skb)->protocol == IPPROTO_IGMP){
			    /* IGMPv1 (and broken IGMPv2 implementations sort of
			       Cisco IOS <= 11.2(8)) do not put router alert
			       option to IGMP packets destined to routable
			       groups. It is very bad, because it means
			       that we can forward NO IGMP messages.
			     */
			    read_lock(&mrt_lock);
			    if (mrt->mroute_sk) {
				    nf_reset(skb);
				    raw_rcv(mrt->mroute_sk, skb);
				    read_unlock(&mrt_lock);
				    return 0;
			    }
			    read_unlock(&mrt_lock);
		    }
	}

	read_lock(&mrt_lock);
	cache = ipmr_cache_find(mrt, ip_hdr(skb)->saddr, ip_hdr(skb)->daddr);

	/*
	 *	No usable cache entry
	 */
	if (cache == NULL) {
		int vif;

		if (local) {
			struct sk_buff *skb2 = skb_clone(skb, GFP_ATOMIC);
			ip_local_deliver(skb);
			if (skb2 == NULL) {
				read_unlock(&mrt_lock);
				return -ENOBUFS;
			}
			skb = skb2;
		}

		vif = ipmr_find_vif(mrt, skb->dev);
		if (vif >= 0) {
			int err2 = ipmr_cache_unresolved(mrt, vif, skb);
			read_unlock(&mrt_lock);

			return err2;
		}
		read_unlock(&mrt_lock);
		kfree_skb(skb);
		return -ENODEV;
	}

	ip_mr_forward(net, mrt, skb, cache, local);

	read_unlock(&mrt_lock);

	if (local)
		return ip_local_deliver(skb);

	return 0;

dont_forward:
	if (local)
		return ip_local_deliver(skb);
	kfree_skb(skb);
	return 0;
}

#ifdef CONFIG_IP_PIMSM
static int __pim_rcv(struct mr_table *mrt, struct sk_buff *skb,
		     unsigned int pimlen)
{
	struct net_device *reg_dev = NULL;
	struct iphdr *encap;

	encap = (struct iphdr *)(skb_transport_header(skb) + pimlen);
	/*
	   Check that:
	   a. packet is really destinted to a multicast group
	   b. packet is not a NULL-REGISTER
	   c. packet is not truncated
	 */
	if (!ipv4_is_multicast(encap->daddr) ||
	    encap->tot_len == 0 ||
	    ntohs(encap->tot_len) + pimlen > skb->len)
		return 1;

	read_lock(&mrt_lock);
	if (mrt->mroute_reg_vif_num >= 0)
		reg_dev = mrt->vif_table[mrt->mroute_reg_vif_num].dev;
	if (reg_dev)
		dev_hold(reg_dev);
	read_unlock(&mrt_lock);

	if (reg_dev == NULL)
		return 1;

	skb->mac_header = skb->network_header;
	skb_pull(skb, (u8*)encap - skb->data);
	skb_reset_network_header(skb);
	skb->protocol = htons(ETH_P_IP);
	skb->ip_summed = 0;
	skb->pkt_type = PACKET_HOST;

	skb_tunnel_rx(skb, reg_dev);

	netif_rx(skb);
	dev_put(reg_dev);

	return 0;
}
#endif

#ifdef CONFIG_IP_PIMSM_V1
/*
 * Handle IGMP messages of PIMv1
 */

int pim_rcv_v1(struct sk_buff * skb)
{
	struct igmphdr *pim;
	struct net *net = dev_net(skb->dev);
	struct mr_table *mrt;

	if (!pskb_may_pull(skb, sizeof(*pim) + sizeof(struct iphdr)))
		goto drop;

	pim = igmp_hdr(skb);

	if (ipmr_fib_lookup(net, &skb_rtable(skb)->fl, &mrt) < 0)
		goto drop;

	if (!mrt->mroute_do_pim ||
	    pim->group != PIM_V1_VERSION || pim->code != PIM_V1_REGISTER)
		goto drop;

	if (__pim_rcv(mrt, skb, sizeof(*pim))) {
drop:
		kfree_skb(skb);
	}
	return 0;
}
#endif

#ifdef CONFIG_IP_PIMSM_V2
static int pim_rcv(struct sk_buff * skb)
{
	struct pimreghdr *pim;
	struct net *net = dev_net(skb->dev);
	struct mr_table *mrt;

	if (!pskb_may_pull(skb, sizeof(*pim) + sizeof(struct iphdr)))
		goto drop;

	pim = (struct pimreghdr *)skb_transport_header(skb);
	if (pim->type != ((PIM_VERSION<<4)|(PIM_REGISTER)) ||
	    (pim->flags&PIM_NULL_REGISTER) ||
	    (ip_compute_csum((void *)pim, sizeof(*pim)) != 0 &&
	     csum_fold(skb_checksum(skb, 0, skb->len, 0))))
		goto drop;

	if (ipmr_fib_lookup(net, &skb_rtable(skb)->fl, &mrt) < 0)
		goto drop;

	if (__pim_rcv(mrt, skb, sizeof(*pim))) {
drop:
		kfree_skb(skb);
	}
	return 0;
}
#endif

static int __ipmr_fill_mroute(struct mr_table *mrt, struct sk_buff *skb,
			      struct mfc_cache *c, struct rtmsg *rtm)
{
	int ct;
	struct rtnexthop *nhp;
	u8 *b = skb_tail_pointer(skb);
	struct rtattr *mp_head;

	/* If cache is unresolved, don't try to parse IIF and OIF */
	if (c->mfc_parent >= MAXVIFS)
		return -ENOENT;

	if (VIF_EXISTS(mrt, c->mfc_parent))
		RTA_PUT(skb, RTA_IIF, 4, &mrt->vif_table[c->mfc_parent].dev->ifindex);

	mp_head = (struct rtattr *)skb_put(skb, RTA_LENGTH(0));

	for (ct = c->mfc_un.res.minvif; ct < c->mfc_un.res.maxvif; ct++) {
		if (VIF_EXISTS(mrt, ct) && c->mfc_un.res.ttls[ct] < 255) {
			if (skb_tailroom(skb) < RTA_ALIGN(RTA_ALIGN(sizeof(*nhp)) + 4))
				goto rtattr_failure;
			nhp = (struct rtnexthop *)skb_put(skb, RTA_ALIGN(sizeof(*nhp)));
			nhp->rtnh_flags = 0;
			nhp->rtnh_hops = c->mfc_un.res.ttls[ct];
			nhp->rtnh_ifindex = mrt->vif_table[ct].dev->ifindex;
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

int ipmr_get_route(struct net *net,
		   struct sk_buff *skb, struct rtmsg *rtm, int nowait)
{
	int err;
	struct mr_table *mrt;
	struct mfc_cache *cache;
	struct rtable *rt = skb_rtable(skb);

	mrt = ipmr_get_table(net, RT_TABLE_DEFAULT);
	if (mrt == NULL)
		return -ENOENT;

	read_lock(&mrt_lock);
	cache = ipmr_cache_find(mrt, rt->rt_src, rt->rt_dst);

	if (cache == NULL) {
		struct sk_buff *skb2;
		struct iphdr *iph;
		struct net_device *dev;
		int vif;

		if (nowait) {
			read_unlock(&mrt_lock);
			return -EAGAIN;
		}

		dev = skb->dev;
		if (dev == NULL || (vif = ipmr_find_vif(mrt, dev)) < 0) {
			read_unlock(&mrt_lock);
			return -ENODEV;
		}
		skb2 = skb_clone(skb, GFP_ATOMIC);
		if (!skb2) {
			read_unlock(&mrt_lock);
			return -ENOMEM;
		}

		skb_push(skb2, sizeof(struct iphdr));
		skb_reset_network_header(skb2);
		iph = ip_hdr(skb2);
		iph->ihl = sizeof(struct iphdr) >> 2;
		iph->saddr = rt->rt_src;
		iph->daddr = rt->rt_dst;
		iph->version = 0;
		err = ipmr_cache_unresolved(mrt, vif, skb2);
		read_unlock(&mrt_lock);
		return err;
	}

	if (!nowait && (rtm->rtm_flags&RTM_F_NOTIFY))
		cache->mfc_flags |= MFC_NOTIFY;
	err = __ipmr_fill_mroute(mrt, skb, cache, rtm);
	read_unlock(&mrt_lock);
	return err;
}

static int ipmr_fill_mroute(struct mr_table *mrt, struct sk_buff *skb,
			    u32 pid, u32 seq, struct mfc_cache *c)
{
	struct nlmsghdr *nlh;
	struct rtmsg *rtm;

	nlh = nlmsg_put(skb, pid, seq, RTM_NEWROUTE, sizeof(*rtm), NLM_F_MULTI);
	if (nlh == NULL)
		return -EMSGSIZE;

	rtm = nlmsg_data(nlh);
	rtm->rtm_family   = RTNL_FAMILY_IPMR;
	rtm->rtm_dst_len  = 32;
	rtm->rtm_src_len  = 32;
	rtm->rtm_tos      = 0;
	rtm->rtm_table    = mrt->id;
	NLA_PUT_U32(skb, RTA_TABLE, mrt->id);
	rtm->rtm_type     = RTN_MULTICAST;
	rtm->rtm_scope    = RT_SCOPE_UNIVERSE;
	rtm->rtm_protocol = RTPROT_UNSPEC;
	rtm->rtm_flags    = 0;

	NLA_PUT_BE32(skb, RTA_SRC, c->mfc_origin);
	NLA_PUT_BE32(skb, RTA_DST, c->mfc_mcastgrp);

	if (__ipmr_fill_mroute(mrt, skb, c, rtm) < 0)
		goto nla_put_failure;

	return nlmsg_end(skb, nlh);

nla_put_failure:
	nlmsg_cancel(skb, nlh);
	return -EMSGSIZE;
}

static int ipmr_rtm_dumproute(struct sk_buff *skb, struct netlink_callback *cb)
{
	struct net *net = sock_net(skb->sk);
	struct mr_table *mrt;
	struct mfc_cache *mfc;
	unsigned int t = 0, s_t;
	unsigned int h = 0, s_h;
	unsigned int e = 0, s_e;

	s_t = cb->args[0];
	s_h = cb->args[1];
	s_e = cb->args[2];

	read_lock(&mrt_lock);
	ipmr_for_each_table(mrt, net) {
		if (t < s_t)
			goto next_table;
		if (t > s_t)
			s_h = 0;
		for (h = s_h; h < MFC_LINES; h++) {
			list_for_each_entry(mfc, &mrt->mfc_cache_array[h], list) {
				if (e < s_e)
					goto next_entry;
				if (ipmr_fill_mroute(mrt, skb,
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

#ifdef CONFIG_PROC_FS
/*
 *	The /proc interfaces to multicast routing /proc/ip_mr_cache /proc/ip_mr_vif
 */
struct ipmr_vif_iter {
	struct seq_net_private p;
	struct mr_table *mrt;
	int ct;
};

static struct vif_device *ipmr_vif_seq_idx(struct net *net,
					   struct ipmr_vif_iter *iter,
					   loff_t pos)
{
	struct mr_table *mrt = iter->mrt;

	for (iter->ct = 0; iter->ct < mrt->maxvif; ++iter->ct) {
		if (!VIF_EXISTS(mrt, iter->ct))
			continue;
		if (pos-- == 0)
			return &mrt->vif_table[iter->ct];
	}
	return NULL;
}

static void *ipmr_vif_seq_start(struct seq_file *seq, loff_t *pos)
	__acquires(mrt_lock)
{
	struct ipmr_vif_iter *iter = seq->private;
	struct net *net = seq_file_net(seq);
	struct mr_table *mrt;

	mrt = ipmr_get_table(net, RT_TABLE_DEFAULT);
	if (mrt == NULL)
		return ERR_PTR(-ENOENT);

	iter->mrt = mrt;

	read_lock(&mrt_lock);
	return *pos ? ipmr_vif_seq_idx(net, seq->private, *pos - 1)
		: SEQ_START_TOKEN;
}

static void *ipmr_vif_seq_next(struct seq_file *seq, void *v, loff_t *pos)
{
	struct ipmr_vif_iter *iter = seq->private;
	struct net *net = seq_file_net(seq);
	struct mr_table *mrt = iter->mrt;

	++*pos;
	if (v == SEQ_START_TOKEN)
		return ipmr_vif_seq_idx(net, iter, 0);

	while (++iter->ct < mrt->maxvif) {
		if (!VIF_EXISTS(mrt, iter->ct))
			continue;
		return &mrt->vif_table[iter->ct];
	}
	return NULL;
}

static void ipmr_vif_seq_stop(struct seq_file *seq, void *v)
	__releases(mrt_lock)
{
	read_unlock(&mrt_lock);
}

static int ipmr_vif_seq_show(struct seq_file *seq, void *v)
{
	struct ipmr_vif_iter *iter = seq->private;
	struct mr_table *mrt = iter->mrt;

	if (v == SEQ_START_TOKEN) {
		seq_puts(seq,
			 "Interface      BytesIn  PktsIn  BytesOut PktsOut Flags Local    Remote\n");
	} else {
		const struct vif_device *vif = v;
		const char *name =  vif->dev ? vif->dev->name : "none";

		seq_printf(seq,
			   "%2Zd %-10s %8ld %7ld  %8ld %7ld %05X %08X %08X\n",
			   vif - mrt->vif_table,
			   name, vif->bytes_in, vif->pkt_in,
			   vif->bytes_out, vif->pkt_out,
			   vif->flags, vif->local, vif->remote);
	}
	return 0;
}

static const struct seq_operations ipmr_vif_seq_ops = {
	.start = ipmr_vif_seq_start,
	.next  = ipmr_vif_seq_next,
	.stop  = ipmr_vif_seq_stop,
	.show  = ipmr_vif_seq_show,
};

static int ipmr_vif_open(struct inode *inode, struct file *file)
{
	return seq_open_net(inode, file, &ipmr_vif_seq_ops,
			    sizeof(struct ipmr_vif_iter));
}

static const struct file_operations ipmr_vif_fops = {
	.owner	 = THIS_MODULE,
	.open    = ipmr_vif_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = seq_release_net,
};

struct ipmr_mfc_iter {
	struct seq_net_private p;
	struct mr_table *mrt;
	struct list_head *cache;
	int ct;
};


static struct mfc_cache *ipmr_mfc_seq_idx(struct net *net,
					  struct ipmr_mfc_iter *it, loff_t pos)
{
	struct mr_table *mrt = it->mrt;
	struct mfc_cache *mfc;

	read_lock(&mrt_lock);
	for (it->ct = 0; it->ct < MFC_LINES; it->ct++) {
		it->cache = &mrt->mfc_cache_array[it->ct];
		list_for_each_entry(mfc, it->cache, list)
			if (pos-- == 0)
				return mfc;
	}
	read_unlock(&mrt_lock);

	spin_lock_bh(&mfc_unres_lock);
	it->cache = &mrt->mfc_unres_queue;
	list_for_each_entry(mfc, it->cache, list)
		if (pos-- == 0)
			return mfc;
	spin_unlock_bh(&mfc_unres_lock);

	it->cache = NULL;
	return NULL;
}


static void *ipmr_mfc_seq_start(struct seq_file *seq, loff_t *pos)
{
	struct ipmr_mfc_iter *it = seq->private;
	struct net *net = seq_file_net(seq);
	struct mr_table *mrt;

	mrt = ipmr_get_table(net, RT_TABLE_DEFAULT);
	if (mrt == NULL)
		return ERR_PTR(-ENOENT);

	it->mrt = mrt;
	it->cache = NULL;
	it->ct = 0;
	return *pos ? ipmr_mfc_seq_idx(net, seq->private, *pos - 1)
		: SEQ_START_TOKEN;
}

static void *ipmr_mfc_seq_next(struct seq_file *seq, void *v, loff_t *pos)
{
	struct mfc_cache *mfc = v;
	struct ipmr_mfc_iter *it = seq->private;
	struct net *net = seq_file_net(seq);
	struct mr_table *mrt = it->mrt;

	++*pos;

	if (v == SEQ_START_TOKEN)
		return ipmr_mfc_seq_idx(net, seq->private, 0);

	if (mfc->list.next != it->cache)
		return list_entry(mfc->list.next, struct mfc_cache, list);

	if (it->cache == &mrt->mfc_unres_queue)
		goto end_of_list;

	BUG_ON(it->cache != &mrt->mfc_cache_array[it->ct]);

	while (++it->ct < MFC_LINES) {
		it->cache = &mrt->mfc_cache_array[it->ct];
		if (list_empty(it->cache))
			continue;
		return list_first_entry(it->cache, struct mfc_cache, list);
	}

	/* exhausted cache_array, show unresolved */
	read_unlock(&mrt_lock);
	it->cache = &mrt->mfc_unres_queue;
	it->ct = 0;

	spin_lock_bh(&mfc_unres_lock);
	if (!list_empty(it->cache))
		return list_first_entry(it->cache, struct mfc_cache, list);

 end_of_list:
	spin_unlock_bh(&mfc_unres_lock);
	it->cache = NULL;

	return NULL;
}

static void ipmr_mfc_seq_stop(struct seq_file *seq, void *v)
{
	struct ipmr_mfc_iter *it = seq->private;
	struct mr_table *mrt = it->mrt;

	if (it->cache == &mrt->mfc_unres_queue)
		spin_unlock_bh(&mfc_unres_lock);
	else if (it->cache == &mrt->mfc_cache_array[it->ct])
		read_unlock(&mrt_lock);
}

static int ipmr_mfc_seq_show(struct seq_file *seq, void *v)
{
	int n;

	if (v == SEQ_START_TOKEN) {
		seq_puts(seq,
		 "Group    Origin   Iif     Pkts    Bytes    Wrong Oifs\n");
	} else {
		const struct mfc_cache *mfc = v;
		const struct ipmr_mfc_iter *it = seq->private;
		const struct mr_table *mrt = it->mrt;

		seq_printf(seq, "%08X %08X %-3hd",
			   (__force u32) mfc->mfc_mcastgrp,
			   (__force u32) mfc->mfc_origin,
			   mfc->mfc_parent);

		if (it->cache != &mrt->mfc_unres_queue) {
			seq_printf(seq, " %8lu %8lu %8lu",
				   mfc->mfc_un.res.pkt,
				   mfc->mfc_un.res.bytes,
				   mfc->mfc_un.res.wrong_if);
			for (n = mfc->mfc_un.res.minvif;
			     n < mfc->mfc_un.res.maxvif; n++ ) {
				if (VIF_EXISTS(mrt, n) &&
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

static const struct file_operations ipmr_mfc_fops = {
	.owner	 = THIS_MODULE,
	.open    = ipmr_mfc_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = seq_release_net,
};
#endif

#ifdef CONFIG_IP_PIMSM_V2
static const struct net_protocol pim_protocol = {
	.handler	=	pim_rcv,
	.netns_ok	=	1,
};
#endif


/*
 *	Setup for IP multicast routing
 */
static int __net_init ipmr_net_init(struct net *net)
{
	int err;

	err = ipmr_rules_init(net);
	if (err < 0)
		goto fail;

#ifdef CONFIG_PROC_FS
	err = -ENOMEM;
	if (!proc_net_fops_create(net, "ip_mr_vif", 0, &ipmr_vif_fops))
		goto proc_vif_fail;
	if (!proc_net_fops_create(net, "ip_mr_cache", 0, &ipmr_mfc_fops))
		goto proc_cache_fail;
#endif
	return 0;

#ifdef CONFIG_PROC_FS
proc_cache_fail:
	proc_net_remove(net, "ip_mr_vif");
proc_vif_fail:
	ipmr_rules_exit(net);
#endif
fail:
	return err;
}

static void __net_exit ipmr_net_exit(struct net *net)
{
#ifdef CONFIG_PROC_FS
	proc_net_remove(net, "ip_mr_cache");
	proc_net_remove(net, "ip_mr_vif");
#endif
	ipmr_rules_exit(net);
}

static struct pernet_operations ipmr_net_ops = {
	.init = ipmr_net_init,
	.exit = ipmr_net_exit,
};

int __init ip_mr_init(void)
{
	int err;

	mrt_cachep = kmem_cache_create("ip_mrt_cache",
				       sizeof(struct mfc_cache),
				       0, SLAB_HWCACHE_ALIGN|SLAB_PANIC,
				       NULL);
	if (!mrt_cachep)
		return -ENOMEM;

	err = register_pernet_subsys(&ipmr_net_ops);
	if (err)
		goto reg_pernet_fail;

	err = register_netdevice_notifier(&ip_mr_notifier);
	if (err)
		goto reg_notif_fail;
#ifdef CONFIG_IP_PIMSM_V2
	if (inet_add_protocol(&pim_protocol, IPPROTO_PIM) < 0) {
		printk(KERN_ERR "ip_mr_init: can't add PIM protocol\n");
		err = -EAGAIN;
		goto add_proto_fail;
	}
#endif
	rtnl_register(RTNL_FAMILY_IPMR, RTM_GETROUTE, NULL, ipmr_rtm_dumproute);
	return 0;

#ifdef CONFIG_IP_PIMSM_V2
add_proto_fail:
	unregister_netdevice_notifier(&ip_mr_notifier);
#endif
reg_notif_fail:
	unregister_pernet_subsys(&ipmr_net_ops);
reg_pernet_fail:
	kmem_cache_destroy(mrt_cachep);
	return err;
}
