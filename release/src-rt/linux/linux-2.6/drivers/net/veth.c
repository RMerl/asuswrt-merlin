/*
 *  drivers/net/veth.c
 *
 *  Copyright (C) 2007 OpenVZ http://openvz.org, SWsoft Inc
 *
 * Author: Pavel Emelianov <xemul@openvz.org>
 * Ethtool interface from: Eric W. Biederman <ebiederm@xmission.com>
 *
 */

#include <linux/netdevice.h>
#include <linux/slab.h>
#include <linux/ethtool.h>
#include <linux/etherdevice.h>

#include <net/dst.h>
#include <net/xfrm.h>
#include <linux/veth.h>

#define DRV_NAME	"veth"
#define DRV_VERSION	"1.0"

#define MIN_MTU 68		/* Min L3 MTU */
#define MAX_MTU 65535		/* Max L3 MTU (arbitrary) */
#define MTU_PAD (ETH_HLEN + 4)  /* Max difference between L2 and L3 size MTU */

struct veth_net_stats {
	unsigned long	rx_packets;
	unsigned long	tx_packets;
	unsigned long	rx_bytes;
	unsigned long	tx_bytes;
	unsigned long	tx_dropped;
	unsigned long	rx_dropped;
};

struct veth_priv {
	struct net_device *peer;
	struct veth_net_stats __percpu *stats;
	unsigned ip_summed;
};

/*
 * ethtool interface
 */

static struct {
	const char string[ETH_GSTRING_LEN];
} ethtool_stats_keys[] = {
	{ "peer_ifindex" },
};

static int veth_get_settings(struct net_device *dev, struct ethtool_cmd *cmd)
{
	cmd->supported		= 0;
	cmd->advertising	= 0;
	cmd->speed		= SPEED_10000;
	cmd->duplex		= DUPLEX_FULL;
	cmd->port		= PORT_TP;
	cmd->phy_address	= 0;
	cmd->transceiver	= XCVR_INTERNAL;
	cmd->autoneg		= AUTONEG_DISABLE;
	cmd->maxtxpkt		= 0;
	cmd->maxrxpkt		= 0;
	return 0;
}

static void veth_get_drvinfo(struct net_device *dev, struct ethtool_drvinfo *info)
{
	strcpy(info->driver, DRV_NAME);
	strcpy(info->version, DRV_VERSION);
	strcpy(info->fw_version, "N/A");
}

static void veth_get_strings(struct net_device *dev, u32 stringset, u8 *buf)
{
	switch(stringset) {
	case ETH_SS_STATS:
		memcpy(buf, &ethtool_stats_keys, sizeof(ethtool_stats_keys));
		break;
	}
}

static int veth_get_sset_count(struct net_device *dev, int sset)
{
	switch (sset) {
	case ETH_SS_STATS:
		return ARRAY_SIZE(ethtool_stats_keys);
	default:
		return -EOPNOTSUPP;
	}
}

static void veth_get_ethtool_stats(struct net_device *dev,
		struct ethtool_stats *stats, u64 *data)
{
	struct veth_priv *priv;

	priv = netdev_priv(dev);
	data[0] = priv->peer->ifindex;
}

static u32 veth_get_rx_csum(struct net_device *dev)
{
	struct veth_priv *priv;

	priv = netdev_priv(dev);
	return priv->ip_summed == CHECKSUM_UNNECESSARY;
}

static int veth_set_rx_csum(struct net_device *dev, u32 data)
{
	struct veth_priv *priv;

	priv = netdev_priv(dev);
	priv->ip_summed = data ? CHECKSUM_UNNECESSARY : CHECKSUM_NONE;
	return 0;
}

static u32 veth_get_tx_csum(struct net_device *dev)
{
	return (dev->features & NETIF_F_NO_CSUM) != 0;
}

static int veth_set_tx_csum(struct net_device *dev, u32 data)
{
	if (data)
		dev->features |= NETIF_F_NO_CSUM;
	else
		dev->features &= ~NETIF_F_NO_CSUM;
	return 0;
}

static const struct ethtool_ops veth_ethtool_ops = {
	.get_settings		= veth_get_settings,
	.get_drvinfo		= veth_get_drvinfo,
	.get_link		= ethtool_op_get_link,
	.get_rx_csum		= veth_get_rx_csum,
	.set_rx_csum		= veth_set_rx_csum,
	.get_tx_csum		= veth_get_tx_csum,
	.set_tx_csum		= veth_set_tx_csum,
	.get_sg			= ethtool_op_get_sg,
	.set_sg			= ethtool_op_set_sg,
	.get_strings		= veth_get_strings,
	.get_sset_count		= veth_get_sset_count,
	.get_ethtool_stats	= veth_get_ethtool_stats,
};

/*
 * xmit
 */

static netdev_tx_t veth_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct net_device *rcv = NULL;
	struct veth_priv *priv, *rcv_priv;
	struct veth_net_stats *stats, *rcv_stats;
	int length;

	priv = netdev_priv(dev);
	rcv = priv->peer;
	rcv_priv = netdev_priv(rcv);

	stats = this_cpu_ptr(priv->stats);
	rcv_stats = this_cpu_ptr(rcv_priv->stats);

	if (!(rcv->flags & IFF_UP))
		goto tx_drop;

	/* don't change ip_summed == CHECKSUM_PARTIAL, as that
	   will cause bad checksum on forwarded packets */
	if (skb->ip_summed == CHECKSUM_NONE)
		skb->ip_summed = rcv_priv->ip_summed;

	length = skb->len;
	if (dev_forward_skb(rcv, skb) != NET_RX_SUCCESS)
		goto rx_drop;

	stats->tx_bytes += length;
	stats->tx_packets++;

	rcv_stats->rx_bytes += length;
	rcv_stats->rx_packets++;

	return NETDEV_TX_OK;

tx_drop:
	kfree_skb(skb);
	stats->tx_dropped++;
	return NETDEV_TX_OK;

rx_drop:
	rcv_stats->rx_dropped++;
	return NETDEV_TX_OK;
}

/*
 * general routines
 */

static struct net_device_stats *veth_get_stats(struct net_device *dev)
{
	struct veth_priv *priv;
	int cpu;
	struct veth_net_stats *stats, total = {0};

	priv = netdev_priv(dev);

	for_each_possible_cpu(cpu) {
		stats = per_cpu_ptr(priv->stats, cpu);

		total.rx_packets += stats->rx_packets;
		total.tx_packets += stats->tx_packets;
		total.rx_bytes   += stats->rx_bytes;
		total.tx_bytes   += stats->tx_bytes;
		total.tx_dropped += stats->tx_dropped;
		total.rx_dropped += stats->rx_dropped;
	}
	dev->stats.rx_packets = total.rx_packets;
	dev->stats.tx_packets = total.tx_packets;
	dev->stats.rx_bytes   = total.rx_bytes;
	dev->stats.tx_bytes   = total.tx_bytes;
	dev->stats.tx_dropped = total.tx_dropped;
	dev->stats.rx_dropped = total.rx_dropped;

	return &dev->stats;
}

static int veth_open(struct net_device *dev)
{
	struct veth_priv *priv;

	priv = netdev_priv(dev);
	if (priv->peer == NULL)
		return -ENOTCONN;

	if (priv->peer->flags & IFF_UP) {
		netif_carrier_on(dev);
		netif_carrier_on(priv->peer);
	}
	return 0;
}

static int veth_close(struct net_device *dev)
{
	struct veth_priv *priv = netdev_priv(dev);

	netif_carrier_off(dev);
	netif_carrier_off(priv->peer);

	return 0;
}

static int is_valid_veth_mtu(int new_mtu)
{
	return new_mtu >= MIN_MTU && new_mtu <= MAX_MTU;
}

static int veth_change_mtu(struct net_device *dev, int new_mtu)
{
	if (!is_valid_veth_mtu(new_mtu))
		return -EINVAL;
	dev->mtu = new_mtu;
	return 0;
}

static int veth_dev_init(struct net_device *dev)
{
	struct veth_net_stats __percpu *stats;
	struct veth_priv *priv;

	stats = alloc_percpu(struct veth_net_stats);
	if (stats == NULL)
		return -ENOMEM;

	priv = netdev_priv(dev);
	priv->stats = stats;
	return 0;
}

static void veth_dev_free(struct net_device *dev)
{
	struct veth_priv *priv;

	priv = netdev_priv(dev);
	free_percpu(priv->stats);
	free_netdev(dev);
}

static const struct net_device_ops veth_netdev_ops = {
	.ndo_init            = veth_dev_init,
	.ndo_open            = veth_open,
	.ndo_stop            = veth_close,
	.ndo_start_xmit      = veth_xmit,
	.ndo_change_mtu      = veth_change_mtu,
	.ndo_get_stats       = veth_get_stats,
	.ndo_set_mac_address = eth_mac_addr,
};

static void veth_setup(struct net_device *dev)
{
	ether_setup(dev);

	dev->netdev_ops = &veth_netdev_ops;
	dev->ethtool_ops = &veth_ethtool_ops;
	dev->features |= NETIF_F_LLTX;
	dev->destructor = veth_dev_free;
}

/*
 * netlink interface
 */

static int veth_validate(struct nlattr *tb[], struct nlattr *data[])
{
	if (tb[IFLA_ADDRESS]) {
		if (nla_len(tb[IFLA_ADDRESS]) != ETH_ALEN)
			return -EINVAL;
		if (!is_valid_ether_addr(nla_data(tb[IFLA_ADDRESS])))
			return -EADDRNOTAVAIL;
	}
	if (tb[IFLA_MTU]) {
		if (!is_valid_veth_mtu(nla_get_u32(tb[IFLA_MTU])))
			return -EINVAL;
	}
	return 0;
}

static struct rtnl_link_ops veth_link_ops;

static int veth_newlink(struct net *src_net, struct net_device *dev,
			 struct nlattr *tb[], struct nlattr *data[])
{
	int err;
	struct net_device *peer;
	struct veth_priv *priv;
	char ifname[IFNAMSIZ];
	struct nlattr *peer_tb[IFLA_MAX + 1], **tbp;
	struct ifinfomsg *ifmp;
	struct net *net;

	/*
	 * create and register peer first
	 */
	if (data != NULL && data[VETH_INFO_PEER] != NULL) {
		struct nlattr *nla_peer;

		nla_peer = data[VETH_INFO_PEER];
		ifmp = nla_data(nla_peer);
		err = nla_parse(peer_tb, IFLA_MAX,
				nla_data(nla_peer) + sizeof(struct ifinfomsg),
				nla_len(nla_peer) - sizeof(struct ifinfomsg),
				ifla_policy);
		if (err < 0)
			return err;

		err = veth_validate(peer_tb, NULL);
		if (err < 0)
			return err;

		tbp = peer_tb;
	} else {
		ifmp = NULL;
		tbp = tb;
	}

	if (tbp[IFLA_IFNAME])
		nla_strlcpy(ifname, tbp[IFLA_IFNAME], IFNAMSIZ);
	else
		snprintf(ifname, IFNAMSIZ, DRV_NAME "%%d");

	net = rtnl_link_get_net(src_net, tbp);
	if (IS_ERR(net))
		return PTR_ERR(net);

	peer = rtnl_create_link(src_net, net, ifname, &veth_link_ops, tbp);
	if (IS_ERR(peer)) {
		put_net(net);
		return PTR_ERR(peer);
	}

	if (tbp[IFLA_ADDRESS] == NULL)
		random_ether_addr(peer->dev_addr);

	err = register_netdevice(peer);
	put_net(net);
	net = NULL;
	if (err < 0)
		goto err_register_peer;

	netif_carrier_off(peer);

	err = rtnl_configure_link(peer, ifmp);
	if (err < 0)
		goto err_configure_peer;

	/*
	 * register dev last
	 *
	 * note, that since we've registered new device the dev's name
	 * should be re-allocated
	 */

	if (tb[IFLA_ADDRESS] == NULL)
		random_ether_addr(dev->dev_addr);

	if (tb[IFLA_IFNAME])
		nla_strlcpy(dev->name, tb[IFLA_IFNAME], IFNAMSIZ);
	else
		snprintf(dev->name, IFNAMSIZ, DRV_NAME "%%d");

	if (strchr(dev->name, '%')) {
		err = dev_alloc_name(dev, dev->name);
		if (err < 0)
			goto err_alloc_name;
	}

	err = register_netdevice(dev);
	if (err < 0)
		goto err_register_dev;

	netif_carrier_off(dev);

	/*
	 * tie the deviced together
	 */

	priv = netdev_priv(dev);
	priv->peer = peer;

	priv = netdev_priv(peer);
	priv->peer = dev;
	return 0;

err_register_dev:
	/* nothing to do */
err_alloc_name:
err_configure_peer:
	unregister_netdevice(peer);
	return err;

err_register_peer:
	free_netdev(peer);
	return err;
}

static void veth_dellink(struct net_device *dev, struct list_head *head)
{
	struct veth_priv *priv;
	struct net_device *peer;

	priv = netdev_priv(dev);
	peer = priv->peer;

	unregister_netdevice_queue(dev, head);
	unregister_netdevice_queue(peer, head);
}

static const struct nla_policy veth_policy[VETH_INFO_MAX + 1];

static struct rtnl_link_ops veth_link_ops = {
	.kind		= DRV_NAME,
	.priv_size	= sizeof(struct veth_priv),
	.setup		= veth_setup,
	.validate	= veth_validate,
	.newlink	= veth_newlink,
	.dellink	= veth_dellink,
	.policy		= veth_policy,
	.maxtype	= VETH_INFO_MAX,
};

/*
 * init/fini
 */

static __init int veth_init(void)
{
	return rtnl_link_register(&veth_link_ops);
}

static __exit void veth_exit(void)
{
	rtnl_link_unregister(&veth_link_ops);
}

module_init(veth_init);
module_exit(veth_exit);

MODULE_DESCRIPTION("Virtual Ethernet Tunnel");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS_RTNL_LINK(DRV_NAME);
