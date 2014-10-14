/* drivers/net/ifb.c:

	The purpose of this driver is to provide a device that allows
	for sharing of resources:

	1) qdiscs/policies that are per device as opposed to system wide.
	ifb allows for a device which can be redirected to thus providing
	an impression of sharing.

	2) Allows for queueing incoming traffic for shaping instead of
	dropping.

	The original concept is based on what is known as the IMQ
	driver initially written by Martin Devera, later rewritten
	by Patrick McHardy and then maintained by Andre Correa.

	You need the tc action  mirror or redirect to feed this device
       	packets.

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version
	2 of the License, or (at your option) any later version.

  	Authors:	Jamal Hadi Salim (2005)

*/


#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/init.h>
#include <linux/moduleparam.h>
#include <net/pkt_sched.h>
#include <net/net_namespace.h>

#define TX_Q_LIMIT    32
struct ifb_private {
	struct tasklet_struct   ifb_tasklet;
	int     tasklet_pending;
	struct sk_buff_head     rq;
	struct sk_buff_head     tq;
};

static int numifbs = 2;

static void ri_tasklet(unsigned long dev);
static netdev_tx_t ifb_xmit(struct sk_buff *skb, struct net_device *dev);
static int ifb_open(struct net_device *dev);
static int ifb_close(struct net_device *dev);

static void ri_tasklet(unsigned long dev)
{

	struct net_device *_dev = (struct net_device *)dev;
	struct ifb_private *dp = netdev_priv(_dev);
	struct net_device_stats *stats = &_dev->stats;
	struct netdev_queue *txq;
	struct sk_buff *skb;

	txq = netdev_get_tx_queue(_dev, 0);
	if ((skb = skb_peek(&dp->tq)) == NULL) {
		if (__netif_tx_trylock(txq)) {
			skb_queue_splice_tail_init(&dp->rq, &dp->tq);
			__netif_tx_unlock(txq);
		} else {
			/* reschedule */
			goto resched;
		}
	}

	while ((skb = __skb_dequeue(&dp->tq)) != NULL) {
		u32 from = G_TC_FROM(skb->tc_verd);

		skb->tc_verd = 0;
		skb->tc_verd = SET_TC_NCLS(skb->tc_verd);
		stats->tx_packets++;
		stats->tx_bytes +=skb->len;

		rcu_read_lock();
		skb->dev = dev_get_by_index_rcu(&init_net, skb->skb_iif);
		if (!skb->dev) {
			rcu_read_unlock();
			dev_kfree_skb(skb);
			stats->tx_dropped++;
			if (skb_queue_len(&dp->tq) != 0)
				goto resched;
			break;
		}
		rcu_read_unlock();
		skb->skb_iif = _dev->ifindex;

		if (from & AT_EGRESS) {
			dev_queue_xmit(skb);
		} else if (from & AT_INGRESS) {
			skb_pull(skb, skb->dev->hard_header_len);
			netif_receive_skb(skb);
		} else
			BUG();
	}

	if (__netif_tx_trylock(txq)) {
		if ((skb = skb_peek(&dp->rq)) == NULL) {
			dp->tasklet_pending = 0;
			if (netif_queue_stopped(_dev))
				netif_wake_queue(_dev);
		} else {
			__netif_tx_unlock(txq);
			goto resched;
		}
		__netif_tx_unlock(txq);
	} else {
resched:
		dp->tasklet_pending = 1;
		tasklet_schedule(&dp->ifb_tasklet);
	}

}

static const struct net_device_ops ifb_netdev_ops = {
	.ndo_open	= ifb_open,
	.ndo_stop	= ifb_close,
	.ndo_start_xmit	= ifb_xmit,
	.ndo_validate_addr = eth_validate_addr,
};

#define IFB_FEATURES (NETIF_F_NO_CSUM | NETIF_F_SG  | NETIF_F_FRAGLIST	| \
		      NETIF_F_TSO_ECN | NETIF_F_TSO | NETIF_F_TSO6	| \
		      NETIF_F_HIGHDMA | NETIF_F_HW_VLAN_TX)

static void ifb_setup(struct net_device *dev)
{
	/* Initialize the device structure. */
	dev->destructor = free_netdev;
	dev->netdev_ops = &ifb_netdev_ops;

	/* Fill in device structure with ethernet-generic values. */
	ether_setup(dev);
	dev->tx_queue_len = TX_Q_LIMIT;

	dev->features |= IFB_FEATURES;
	dev->vlan_features |= IFB_FEATURES;

	dev->flags |= IFF_NOARP;
	dev->flags &= ~IFF_MULTICAST;
	dev->priv_flags &= ~IFF_XMIT_DST_RELEASE;
	random_ether_addr(dev->dev_addr);
}

static netdev_tx_t ifb_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct ifb_private *dp = netdev_priv(dev);
	struct net_device_stats *stats = &dev->stats;
	u32 from = G_TC_FROM(skb->tc_verd);

	stats->rx_packets++;
	stats->rx_bytes+=skb->len;

	if (!(from & (AT_INGRESS|AT_EGRESS)) || !skb->skb_iif) {
		dev_kfree_skb(skb);
		stats->rx_dropped++;
		return NETDEV_TX_OK;
	}

	if (skb_queue_len(&dp->rq) >= dev->tx_queue_len) {
		netif_stop_queue(dev);
	}

	__skb_queue_tail(&dp->rq, skb);
	if (!dp->tasklet_pending) {
		dp->tasklet_pending = 1;
		tasklet_schedule(&dp->ifb_tasklet);
	}

	return NETDEV_TX_OK;
}

static int ifb_close(struct net_device *dev)
{
	struct ifb_private *dp = netdev_priv(dev);

	tasklet_kill(&dp->ifb_tasklet);
	netif_stop_queue(dev);
	__skb_queue_purge(&dp->rq);
	__skb_queue_purge(&dp->tq);
	return 0;
}

static int ifb_open(struct net_device *dev)
{
	struct ifb_private *dp = netdev_priv(dev);

	tasklet_init(&dp->ifb_tasklet, ri_tasklet, (unsigned long)dev);
	__skb_queue_head_init(&dp->rq);
	__skb_queue_head_init(&dp->tq);
	netif_start_queue(dev);

	return 0;
}

static int ifb_validate(struct nlattr *tb[], struct nlattr *data[])
{
	if (tb[IFLA_ADDRESS]) {
		if (nla_len(tb[IFLA_ADDRESS]) != ETH_ALEN)
			return -EINVAL;
		if (!is_valid_ether_addr(nla_data(tb[IFLA_ADDRESS])))
			return -EADDRNOTAVAIL;
	}
	return 0;
}

static struct rtnl_link_ops ifb_link_ops __read_mostly = {
	.kind		= "ifb",
	.priv_size	= sizeof(struct ifb_private),
	.setup		= ifb_setup,
	.validate	= ifb_validate,
};

/* Number of ifb devices to be set up by this module. */
module_param(numifbs, int, 0);
MODULE_PARM_DESC(numifbs, "Number of ifb devices");

static int __init ifb_init_one(int index)
{
	struct net_device *dev_ifb;
	int err;

	dev_ifb = alloc_netdev(sizeof(struct ifb_private),
				 "ifb%d", ifb_setup);

	if (!dev_ifb)
		return -ENOMEM;

	err = dev_alloc_name(dev_ifb, dev_ifb->name);
	if (err < 0)
		goto err;

	dev_ifb->rtnl_link_ops = &ifb_link_ops;
	err = register_netdevice(dev_ifb);
	if (err < 0)
		goto err;

	return 0;

err:
	free_netdev(dev_ifb);
	return err;
}

static int __init ifb_init_module(void)
{
	int i, err;

	rtnl_lock();
	err = __rtnl_link_register(&ifb_link_ops);

	for (i = 0; i < numifbs && !err; i++)
		err = ifb_init_one(i);
	if (err)
		__rtnl_link_unregister(&ifb_link_ops);
	rtnl_unlock();

	return err;
}

static void __exit ifb_cleanup_module(void)
{
	rtnl_link_unregister(&ifb_link_ops);
}

module_init(ifb_init_module);
module_exit(ifb_cleanup_module);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jamal Hadi Salim");
MODULE_ALIAS_RTNL_LINK("ifb");
