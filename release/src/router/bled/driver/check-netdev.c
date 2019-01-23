/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 *
 * Copyright 2014, ASUSTeK Inc.
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND ASUS GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 */

#include <linux/version.h>
#include <linux/time.h>
#include "../bled_defs.h"
#include "check.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
/* Convert rtnl_link_stats64 to net_device_stats. They have the same
 * fields in the same order, with only the type differing.
 */
void netdev_stats64_to_stats(struct net_device_stats *netdev_stats, const struct rtnl_link_stats64 *stats64)
{
#if BITS_PER_LONG == 64
#error //eric++
	BUILD_BUG_ON(sizeof(*stats64) != sizeof(*netdev_stats));
	memcpy(netdev_stats, stats64, sizeof(*netdev_stats));
#else
	size_t i, n = sizeof(*stats64) / sizeof(u64);
	const u64 *src = (const u64 *)stats64;
	unsigned long *dst = (unsigned long *)netdev_stats;

	BUILD_BUG_ON(sizeof(*netdev_stats) / sizeof(unsigned long) !=
		     sizeof(*stats64) / sizeof(u64));
	for (i = 0; i < n; i++)
		dst[i] = src[i];
#endif
}
#endif

static struct net_device_stats *get_stats(struct net_device *ndev, struct net_device_stats *stats)
{
	struct net_device_stats *s = stats;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 29)
	const struct net_device_ops *ops = ndev->netdev_ops;
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
	struct rtnl_link_stats64 stats64;
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
	if (ops->ndo_get_stats64) {
		memset(&stats64, 0, sizeof(stats64));
		ops->ndo_get_stats64(ndev, &stats64);
		netdev_stats64_to_stats(stats, &stats64);
	} else
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 29)
	if (ops->ndo_get_stats) {
		s = ops->ndo_get_stats(ndev);
	} else
#endif
	{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 22)
		s = &ndev->stats;
#else
#error
		/* FIXME:
		 * 	net_device.stats doesn't exist until
		 * 	commit c45d286e72dd72c0229dc9e2849743ba427fee84. (2.6.22)
		 */
#endif
	}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 37)
	s->rx_dropped += atomic_long_read(&ndev->rx_dropped);
#endif

	return s;
}

/**
 * Evaluate TX/RX bytes of interface(s).
 * @bp:
 * @return:
 * 	< 0:			always turn on LED
 *  >= NR_BLED_INTERVAL_SET:	always turn on LED
 *  otherwise:			blink LED in accordance with bp->intervals[RETURN_VALUE]
 */
unsigned int ndev_check_traffic(struct bled_priv *bp)
{
	int i, c;
	unsigned int b = 0;
	unsigned long d, diff;
	struct net_device *dev;
	const struct net_device_stats *s;
	struct ndev_bled_priv *np = bp->check_priv;
	struct ndev_bled_ifstat *ifs;
	struct net_device_stats stats;

	if (bp->state != BLED_STATE_RUN)
		return -1;

	if (unlikely(bp->type != BLED_TYPE_NETDEV_BLED))
		np = bp->check2_priv;
	for (i = 0, c = 0, diff = 0, ifs = &np->ifstat[0]; i < np->nr_if; ++i, ++ifs) {
		if (unlikely(!(dev = dev_get_by_name(&init_net, ifs->ifname))))
			continue;

		if (unlikely(!(s = get_stats(dev, &stats)))) {
			dev_put(dev);
			continue;
		}

		if (unlikely(!ifs->last_rx_bytes && !ifs->last_tx_bytes)) {
			ifs->last_rx_bytes = s->rx_bytes;
			ifs->last_tx_bytes = s->tx_bytes;
			dev_put(dev);
			continue;
		}

		d = bdiff(ifs->last_rx_bytes, s->rx_bytes) + bdiff(ifs->last_tx_bytes, s->tx_bytes);
		diff += d;
		if (unlikely(bp->flags & BLED_FLAGS_DBG_CHECK_FUNC)) {
			prn_bl_v("GPIO#%d: ifname %s, d %10lu (RX %10lu,%10lu / TX %10lu,%10lu)\n",
				bp->gpio_nr, ifs->ifname, d, ifs->last_rx_bytes, s->rx_bytes,
				ifs->last_tx_bytes, s->tx_bytes);
		}

		ifs->last_rx_bytes = s->rx_bytes;
		ifs->last_tx_bytes = s->tx_bytes;

		dev_put(dev);
		c++;
	}

	if (likely(diff >= bp->threshold))
		b = 1;
	if (unlikely(!c))
		bp->next_check_interval = BLED_WAIT_IF_INTERVAL;

	return b;
}

/**
 * Reset statistics information.
 * @bp:
 * @return:
 */
int ndev_reset_check_traffic(struct bled_priv *bp)
{
	int i, c, ret = -1;
	struct net_device *dev;
	const struct net_device_stats *s;
	struct ndev_bled_priv *np = bp->check_priv;
	struct ndev_bled_ifstat *ifs;
	struct net_device_stats stats;

	if (bp->type != BLED_TYPE_NETDEV_BLED)
		np = bp->check2_priv;
	for (i = 0, c = 0, ifs = &np->ifstat[0]; i < np->nr_if; ++i, ++ifs) {
		ifs->last_rx_bytes = 0;
		ifs->last_tx_bytes = 0;
		if (!(dev = dev_get_by_name(&init_net, ifs->ifname)))
			continue;

		if (!(s = get_stats(dev, &stats))) {
			dev_put(dev);
			continue;
		}

		ifs->last_rx_bytes = s->rx_bytes;
		ifs->last_tx_bytes = s->tx_bytes;

		dev_put(dev);
		c++;
	}
	if (np->nr_if == c)
		ret = 0;
	if (bp->type == BLED_TYPE_NETDEV_BLED && !c)
		bp->next_check_interval = BLED_WAIT_IF_INTERVAL;

	return ret;
}

/**
 * Print private data of netdev LED.
 * @bp:
 */
void netdev_led_printer(struct bled_priv *bp, struct seq_file *m)
{
	int i;
	char str[64];
	struct net_device *ndev;
	struct ndev_bled_priv *np;
	struct ndev_bled_ifstat *ifs;

	if (!bp || !m)
		return;

	np = (struct ndev_bled_priv*) bp->check_priv;
	if (bp->type != BLED_TYPE_NETDEV_BLED)
		np = bp->check2_priv;
	if (unlikely(!np))
		return;

	seq_printf(m, TFMT "%d\n", "Number of interfaces", np->nr_if);
	for (i = 0, ifs = &np->ifstat[0]; i < np->nr_if; ++i, ++ifs) {
		ndev = dev_get_by_name(&init_net, ifs->ifname);
		if (unlikely(!ndev)) {
			seq_printf(m, TFMT "%-8s | %s\n", "interface",
				ifs->ifname, "N/A");
			continue;
		}
		sprintf(str, "interface | RX/TX bytes");
		seq_printf(m, TFMT "%-8s | %10lu / %10lu Bytes\n", str,
			ifs->ifname, ifs->last_rx_bytes, ifs->last_tx_bytes);
		dev_put(ndev);
	}

	return;
}
