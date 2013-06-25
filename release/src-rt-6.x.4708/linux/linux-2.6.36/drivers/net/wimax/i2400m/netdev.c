
#include <linux/if_arp.h>
#include <linux/slab.h>
#include <linux/netdevice.h>
#include <linux/ethtool.h>
#include "i2400m.h"


#define D_SUBMODULE netdev
#include "debug-levels.h"

enum {
/* netdev interface */
	/* 20 secs? yep, this is the maximum timeout that the device
	 * might take to get out of IDLE / negotiate it with the base
	 * station. We add 1sec for good measure. */
	I2400M_TX_TIMEOUT = 21 * HZ,
	/*
	 * Experimentation has determined that, 20 to be a good value
	 * for minimizing the jitter in the throughput.
	 */
	I2400M_TX_QLEN = 20,
};


static
int i2400m_open(struct net_device *net_dev)
{
	int result;
	struct i2400m *i2400m = net_dev_to_i2400m(net_dev);
	struct device *dev = i2400m_dev(i2400m);

	d_fnstart(3, dev, "(net_dev %p [i2400m %p])\n", net_dev, i2400m);
	/* Make sure we wait until init is complete... */
	mutex_lock(&i2400m->init_mutex);
	if (i2400m->updown)
		result = 0;
	else
		result = -EBUSY;
	mutex_unlock(&i2400m->init_mutex);
	d_fnend(3, dev, "(net_dev %p [i2400m %p]) = %d\n",
		net_dev, i2400m, result);
	return result;
}


static
int i2400m_stop(struct net_device *net_dev)
{
	struct i2400m *i2400m = net_dev_to_i2400m(net_dev);
	struct device *dev = i2400m_dev(i2400m);

	d_fnstart(3, dev, "(net_dev %p [i2400m %p])\n", net_dev, i2400m);
	i2400m_net_wake_stop(i2400m);
	d_fnend(3, dev, "(net_dev %p [i2400m %p]) = 0\n", net_dev, i2400m);
	return 0;
}


/*
 * Wake up the device and transmit a held SKB, then restart the net queue
 *
 * When the device goes into basestation-idle mode, we need to tell it
 * to exit that mode; it will negotiate with the base station, user
 * space may have to intervene to rehandshake crypto and then tell us
 * when it is ready to transmit the packet we have "queued". Still we
 * need to give it sometime after it reports being ok.
 *
 * On error, there is not much we can do. If the error was on TX, we
 * still wake the queue up to see if the next packet will be luckier.
 *
 * If _cmd_exit_idle() fails...well, it could be many things; most
 * commonly it is that something else took the device out of IDLE mode
 * (for example, the base station). In that case we get an -EILSEQ and
 * we are just going to ignore that one. If the device is back to
 * connected, then fine -- if it is someother state, the packet will
 * be dropped anyway.
 */
void i2400m_wake_tx_work(struct work_struct *ws)
{
	int result;
	struct i2400m *i2400m = container_of(ws, struct i2400m, wake_tx_ws);
	struct net_device *net_dev = i2400m->wimax_dev.net_dev;
	struct device *dev = i2400m_dev(i2400m);
	struct sk_buff *skb = i2400m->wake_tx_skb;
	unsigned long flags;

	spin_lock_irqsave(&i2400m->tx_lock, flags);
	skb = i2400m->wake_tx_skb;
	i2400m->wake_tx_skb = NULL;
	spin_unlock_irqrestore(&i2400m->tx_lock, flags);

	d_fnstart(3, dev, "(ws %p i2400m %p skb %p)\n", ws, i2400m, skb);
	result = -EINVAL;
	if (skb == NULL) {
		dev_err(dev, "WAKE&TX: skb dissapeared!\n");
		goto out_put;
	}
	/* If we have, somehow, lost the connection after this was
	 * queued, don't do anything; this might be the device got
	 * reset or just disconnected. */
	if (unlikely(!netif_carrier_ok(net_dev)))
		goto out_kfree;
	result = i2400m_cmd_exit_idle(i2400m);
	if (result == -EILSEQ)
		result = 0;
	if (result < 0) {
		dev_err(dev, "WAKE&TX: device didn't get out of idle: "
			"%d - resetting\n", result);
		i2400m_reset(i2400m, I2400M_RT_BUS);
		goto error;
	}
	result = wait_event_timeout(i2400m->state_wq,
				    i2400m->state != I2400M_SS_IDLE,
				    net_dev->watchdog_timeo - HZ/2);
	if (result == 0)
		result = -ETIMEDOUT;
	if (result < 0) {
		dev_err(dev, "WAKE&TX: error waiting for device to exit IDLE: "
			"%d - resetting\n", result);
		i2400m_reset(i2400m, I2400M_RT_BUS);
		goto error;
	}
	msleep(20);	/* device still needs some time or it drops it */
	result = i2400m_tx(i2400m, skb->data, skb->len, I2400M_PT_DATA);
error:
	netif_wake_queue(net_dev);
out_kfree:
	kfree_skb(skb);	/* refcount transferred by _hard_start_xmit() */
out_put:
	i2400m_put(i2400m);
	d_fnend(3, dev, "(ws %p i2400m %p skb %p) = void [%d]\n",
		ws, i2400m, skb, result);
}


/*
 * Prepare the data payload TX header
 *
 * The i2400m expects a 4 byte header in front of a data packet.
 *
 * Because we pretend to be an ethernet device, this packet comes with
 * an ethernet header. Pull it and push our header.
 */
static
void i2400m_tx_prep_header(struct sk_buff *skb)
{
	struct i2400m_pl_data_hdr *pl_hdr;
	skb_pull(skb, ETH_HLEN);
	pl_hdr = (struct i2400m_pl_data_hdr *) skb_push(skb, sizeof(*pl_hdr));
	pl_hdr->reserved = 0;
}



/*
 * Cleanup resources acquired during i2400m_net_wake_tx()
 *
 * This is called by __i2400m_dev_stop and means we have to make sure
 * the workqueue is flushed from any pending work.
 */
void i2400m_net_wake_stop(struct i2400m *i2400m)
{
	struct device *dev = i2400m_dev(i2400m);

	d_fnstart(3, dev, "(i2400m %p)\n", i2400m);
	/* See i2400m_hard_start_xmit(), references are taken there
	 * and here we release them if the work was still
	 * pending. Note we can't differentiate work not pending vs
	 * never scheduled, so the NULL check does that. */
	if (cancel_work_sync(&i2400m->wake_tx_ws) == 0
	    && i2400m->wake_tx_skb != NULL) {
		unsigned long flags;
		struct sk_buff *wake_tx_skb;
		spin_lock_irqsave(&i2400m->tx_lock, flags);
		wake_tx_skb = i2400m->wake_tx_skb;	/* compat help */
		i2400m->wake_tx_skb = NULL;	/* compat help */
		spin_unlock_irqrestore(&i2400m->tx_lock, flags);
		i2400m_put(i2400m);
		kfree_skb(wake_tx_skb);
	}
	d_fnend(3, dev, "(i2400m %p) = void\n", i2400m);
}


/*
 * TX an skb to an idle device
 *
 * When the device is in basestation-idle mode, we need to wake it up
 * and then TX. So we queue a work_struct for doing so.
 *
 * We need to get an extra ref for the skb (so it is not dropped), as
 * well as be careful not to queue more than one request (won't help
 * at all). If more than one request comes or there are errors, we
 * just drop the packets (see i2400m_hard_start_xmit()).
 */
static
int i2400m_net_wake_tx(struct i2400m *i2400m, struct net_device *net_dev,
		       struct sk_buff *skb)
{
	int result;
	struct device *dev = i2400m_dev(i2400m);
	unsigned long flags;

	d_fnstart(3, dev, "(skb %p net_dev %p)\n", skb, net_dev);
	if (net_ratelimit()) {
		d_printf(3, dev, "WAKE&NETTX: "
			 "skb %p sending %d bytes to radio\n",
			 skb, skb->len);
		d_dump(4, dev, skb->data, skb->len);
	}
	/* We hold a ref count for i2400m and skb, so when
	 * stopping() the device, we need to cancel that work
	 * and if pending, release those resources. */
	result = 0;
	spin_lock_irqsave(&i2400m->tx_lock, flags);
	if (!work_pending(&i2400m->wake_tx_ws)) {
		netif_stop_queue(net_dev);
		i2400m_get(i2400m);
		i2400m->wake_tx_skb = skb_get(skb);	/* transfer ref count */
		i2400m_tx_prep_header(skb);
		result = schedule_work(&i2400m->wake_tx_ws);
		WARN_ON(result == 0);
	}
	spin_unlock_irqrestore(&i2400m->tx_lock, flags);
	if (result == 0) {
		/* Yes, this happens even if we stopped the
		 * queue -- blame the queue disciplines that
		 * queue without looking -- I guess there is a reason
		 * for that. */
		if (net_ratelimit())
			d_printf(1, dev, "NETTX: device exiting idle, "
				 "dropping skb %p, queue running %d\n",
				 skb, netif_queue_stopped(net_dev));
		result = -EBUSY;
	}
	d_fnend(3, dev, "(skb %p net_dev %p) = %d\n", skb, net_dev, result);
	return result;
}


/*
 * Transmit a packet to the base station on behalf of the network stack.
 *
 * Returns: 0 if ok, < 0 errno code on error.
 *
 * We need to pull the ethernet header and add the hardware header,
 * which is currently set to all zeroes and reserved.
 */
static
int i2400m_net_tx(struct i2400m *i2400m, struct net_device *net_dev,
		  struct sk_buff *skb)
{
	int result;
	struct device *dev = i2400m_dev(i2400m);

	d_fnstart(3, dev, "(i2400m %p net_dev %p skb %p)\n",
		  i2400m, net_dev, skb);
	net_dev->trans_start = jiffies;
	i2400m_tx_prep_header(skb);
	d_printf(3, dev, "NETTX: skb %p sending %d bytes to radio\n",
		 skb, skb->len);
	d_dump(4, dev, skb->data, skb->len);
	result = i2400m_tx(i2400m, skb->data, skb->len, I2400M_PT_DATA);
	d_fnend(3, dev, "(i2400m %p net_dev %p skb %p) = %d\n",
		i2400m, net_dev, skb, result);
	return result;
}


/*
 * Transmit a packet to the base station on behalf of the network stack
 *
 *
 * Returns: NETDEV_TX_OK (always, even in case of error)
 *
 * In case of error, we just drop it. Reasons:
 *
 *  - we add a hw header to each skb, and if the network stack
 *    retries, we have no way to know if that skb has it or not.
 *
 *  - network protocols have their own drop-recovery mechanisms
 *
 *  - there is not much else we can do
 *
 * If the device is idle, we need to wake it up; that is an operation
 * that will sleep. See i2400m_net_wake_tx() for details.
 */
static
netdev_tx_t i2400m_hard_start_xmit(struct sk_buff *skb,
					 struct net_device *net_dev)
{
	struct i2400m *i2400m = net_dev_to_i2400m(net_dev);
	struct device *dev = i2400m_dev(i2400m);
	int result;

	d_fnstart(3, dev, "(skb %p net_dev %p)\n", skb, net_dev);
	if (skb_header_cloned(skb)) {
		/*
		 * Make tcpdump/wireshark happy -- if they are
		 * running, the skb is cloned and we will overwrite
		 * the mac fields in i2400m_tx_prep_header. Expand
		 * seems to fix this...
		 */
		result = pskb_expand_head(skb, 0, 0, GFP_ATOMIC);
		if (result) {
			result = NETDEV_TX_BUSY;
			goto error_expand;
		}
	}

	if (i2400m->state == I2400M_SS_IDLE)
		result = i2400m_net_wake_tx(i2400m, net_dev, skb);
	else
		result = i2400m_net_tx(i2400m, net_dev, skb);
	if (result <  0)
		net_dev->stats.tx_dropped++;
	else {
		net_dev->stats.tx_packets++;
		net_dev->stats.tx_bytes += skb->len;
	}
	result = NETDEV_TX_OK;
error_expand:
	kfree_skb(skb);
	d_fnend(3, dev, "(skb %p net_dev %p) = %d\n", skb, net_dev, result);
	return result;
}


static
int i2400m_change_mtu(struct net_device *net_dev, int new_mtu)
{
	int result;
	struct i2400m *i2400m = net_dev_to_i2400m(net_dev);
	struct device *dev = i2400m_dev(i2400m);

	if (new_mtu >= I2400M_MAX_MTU) {
		dev_err(dev, "Cannot change MTU to %d (max is %d)\n",
			new_mtu, I2400M_MAX_MTU);
		result = -EINVAL;
	} else {
		net_dev->mtu = new_mtu;
		result = 0;
	}
	return result;
}


static
void i2400m_tx_timeout(struct net_device *net_dev)
{
	/*
	 * We might want to kick the device
	 *
	 * There is not much we can do though, as the device requires
	 * that we send the data aggregated. By the time we receive
	 * this, there might be data pending to be sent or not...
	 */
	net_dev->stats.tx_errors++;
}


/*
 * Create a fake ethernet header
 *
 * For emulating an ethernet device, every received IP header has to
 * be prefixed with an ethernet header. Fake it with the given
 * protocol.
 */
static
void i2400m_rx_fake_eth_header(struct net_device *net_dev,
			       void *_eth_hdr, __be16 protocol)
{
	struct i2400m *i2400m = net_dev_to_i2400m(net_dev);
	struct ethhdr *eth_hdr = _eth_hdr;

	memcpy(eth_hdr->h_dest, net_dev->dev_addr, sizeof(eth_hdr->h_dest));
	memcpy(eth_hdr->h_source, i2400m->src_mac_addr,
	       sizeof(eth_hdr->h_source));
	eth_hdr->h_proto = protocol;
}


void i2400m_net_rx(struct i2400m *i2400m, struct sk_buff *skb_rx,
		   unsigned i, const void *buf, int buf_len)
{
	struct net_device *net_dev = i2400m->wimax_dev.net_dev;
	struct device *dev = i2400m_dev(i2400m);
	struct sk_buff *skb;

	d_fnstart(2, dev, "(i2400m %p buf %p buf_len %d)\n",
		  i2400m, buf, buf_len);
	if (i) {
		skb = skb_get(skb_rx);
		d_printf(2, dev, "RX: reusing first payload skb %p\n", skb);
		skb_pull(skb, buf - (void *) skb->data);
		skb_trim(skb, (void *) skb_end_pointer(skb) - buf);
	} else {
		/* Yes, this is bad -- a lot of overhead -- see
		 * comments at the top of the file */
		skb = __netdev_alloc_skb(net_dev, buf_len, GFP_KERNEL);
		if (skb == NULL) {
			dev_err(dev, "NETRX: no memory to realloc skb\n");
			net_dev->stats.rx_dropped++;
			goto error_skb_realloc;
		}
		memcpy(skb_put(skb, buf_len), buf, buf_len);
	}
	i2400m_rx_fake_eth_header(i2400m->wimax_dev.net_dev,
				  skb->data - ETH_HLEN,
				  cpu_to_be16(ETH_P_IP));
	skb_set_mac_header(skb, -ETH_HLEN);
	skb->dev = i2400m->wimax_dev.net_dev;
	skb->protocol = htons(ETH_P_IP);
	net_dev->stats.rx_packets++;
	net_dev->stats.rx_bytes += buf_len;
	d_printf(3, dev, "NETRX: receiving %d bytes to network stack\n",
		buf_len);
	d_dump(4, dev, buf, buf_len);
	netif_rx_ni(skb);	/* see notes in function header */
error_skb_realloc:
	d_fnend(2, dev, "(i2400m %p buf %p buf_len %d) = void\n",
		i2400m, buf, buf_len);
}


void i2400m_net_erx(struct i2400m *i2400m, struct sk_buff *skb,
		    enum i2400m_cs cs)
{
	struct net_device *net_dev = i2400m->wimax_dev.net_dev;
	struct device *dev = i2400m_dev(i2400m);
	int protocol;

	d_fnstart(2, dev, "(i2400m %p skb %p [%u] cs %d)\n",
		  i2400m, skb, skb->len, cs);
	switch(cs) {
	case I2400M_CS_IPV4_0:
	case I2400M_CS_IPV4:
		protocol = ETH_P_IP;
		i2400m_rx_fake_eth_header(i2400m->wimax_dev.net_dev,
					  skb->data - ETH_HLEN,
					  cpu_to_be16(ETH_P_IP));
		skb_set_mac_header(skb, -ETH_HLEN);
		skb->dev = i2400m->wimax_dev.net_dev;
		skb->protocol = htons(ETH_P_IP);
		net_dev->stats.rx_packets++;
		net_dev->stats.rx_bytes += skb->len;
		break;
	default:
		dev_err(dev, "ERX: BUG? CS type %u unsupported\n", cs);
		goto error;

	}
	d_printf(3, dev, "ERX: receiving %d bytes to the network stack\n",
		 skb->len);
	d_dump(4, dev, skb->data, skb->len);
	netif_rx_ni(skb);	/* see notes in function header */
error:
	d_fnend(2, dev, "(i2400m %p skb %p [%u] cs %d) = void\n",
		i2400m, skb, skb->len, cs);
}

static const struct net_device_ops i2400m_netdev_ops = {
	.ndo_open = i2400m_open,
	.ndo_stop = i2400m_stop,
	.ndo_start_xmit = i2400m_hard_start_xmit,
	.ndo_tx_timeout = i2400m_tx_timeout,
	.ndo_change_mtu = i2400m_change_mtu,
};

static void i2400m_get_drvinfo(struct net_device *net_dev,
			       struct ethtool_drvinfo *info)
{
	struct i2400m *i2400m = net_dev_to_i2400m(net_dev);

	strncpy(info->driver, KBUILD_MODNAME, sizeof(info->driver) - 1);
	strncpy(info->fw_version, i2400m->fw_name, sizeof(info->fw_version) - 1);
	if (net_dev->dev.parent)
		strncpy(info->bus_info, dev_name(net_dev->dev.parent),
			sizeof(info->bus_info) - 1);
}

static const struct ethtool_ops i2400m_ethtool_ops = {
	.get_drvinfo = i2400m_get_drvinfo,
	.get_link = ethtool_op_get_link,
};

/**
 * i2400m_netdev_setup - Setup setup @net_dev's i2400m private data
 *
 * Called by alloc_netdev()
 */
void i2400m_netdev_setup(struct net_device *net_dev)
{
	d_fnstart(3, NULL, "(net_dev %p)\n", net_dev);
	ether_setup(net_dev);
	net_dev->mtu = I2400M_MAX_MTU;
	net_dev->tx_queue_len = I2400M_TX_QLEN;
	net_dev->features =
		  NETIF_F_VLAN_CHALLENGED
		| NETIF_F_HIGHDMA;
	net_dev->flags =
		IFF_NOARP		/* i2400m is apure IP device */
		& (~IFF_BROADCAST	/* i2400m is P2P */
		   & ~IFF_MULTICAST);
	net_dev->watchdog_timeo = I2400M_TX_TIMEOUT;
	net_dev->netdev_ops = &i2400m_netdev_ops;
	net_dev->ethtool_ops = &i2400m_ethtool_ops;
	d_fnend(3, NULL, "(net_dev %p) = void\n", net_dev);
}
EXPORT_SYMBOL_GPL(i2400m_netdev_setup);
