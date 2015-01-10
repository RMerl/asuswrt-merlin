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
#include <linux/phy.h>
#include <linux/time.h>
#include <../drivers/net/ethernet/atheros/ag71xx/ag71xx.h>	/* struct ag71xx */
#include "../bled_defs.h"
#include "check.h"

/* copy from ar8216.h */
#define AR8236_STATS_RXGOODBYTE		0x3c
#define AR8236_STATS_TXBYTE		0x84
#define AR8327_REG_PORT_STATS_BASE(_i)		(0x1000 + (_i) * 0x100)

/* copy from ar8216.c */
static inline void
split_addr(u32 regaddr, u16 *r1, u16 *r2, u16 *page)
{
	regaddr >>= 1;
	*r1 = regaddr & 0x1e;

	regaddr >>= 5;
	*r2 = regaddr & 0x7;

	regaddr >>= 3;
	*page = regaddr & 0x1ff;
}

/* copy from ar8216.c & minor modification */
static u32
ar8216_mii_read(struct mii_bus *bus, int reg)
{
	u16 r1, r2, page;
	u16 lo, hi;

	split_addr((u32) reg, &r1, &r2, &page);

	mutex_lock(&bus->mdio_lock);

	bus->write(bus, 0x18, 0, page);
	usleep_range(1000, 2000); /* wait for the page switch to propagate */
	lo = bus->read(bus, 0x10 | r2, r1);
	hi = bus->read(bus, 0x10 | r2, r1 + 1);

	mutex_unlock(&bus->mdio_lock);

	return (hi << 16) | lo;
}

/**
 * Get TX/RX incremental bytes from QCA8337 switch.
 * @bus:
 * @port:
 * @rx:
 * @tx:
 */
static inline void get_port_stats(struct mii_bus *bus, int port, unsigned long *rx, unsigned long *tx)
{
	unsigned int base = AR8327_REG_PORT_STATS_BASE(port);

	/* Take lower 32-bit only. */
	*rx = ar8216_mii_read(bus, base + AR8236_STATS_RXGOODBYTE);
	*tx = ar8216_mii_read(bus, base + AR8236_STATS_TXBYTE);
}

/**
 * Evaluate TX/RX bytes of switch ports.
 * @bp:
 * @return:
 * 	< 0:			always turn on LED
 *  >= NR_BLED_INTERVAL_SET:	always turn on LED
 *  otherwise:			blink LED in accordance with bp->intervals[RETURN_VALUE]
 */
unsigned int swports_check_traffic(struct bled_priv *bp)
{
	int p, c;
	unsigned int b = 0, m;
	unsigned long diff, rx_bytes, tx_bytes;
	struct net_device *dev = dev_get_by_name(&init_net, "eth0");
	struct ag71xx *ag;
	struct mii_bus *bus;
	struct swport_bled_ifstat *ifs;
	struct swport_bled_priv *sp = bp->check_priv;

	if (unlikely(!dev))
		return -1;
	if (unlikely(bp->state != BLED_STATE_RUN)) {
		dev_put(dev);
		return -1;
	}

	ag = netdev_priv(dev);
	bus = ag->mii_bus;
	if (unlikely(!bus)) {
		dbg_bl_v("%s: mii_bus = NULL!\n", __func__);
		dev_put(dev);
		return -1;
	}
	m = sp->port_mask;
	for (p = 0, c = 0, diff = 0, ifs = &sp->portstat[0]; m > 0; ++p, ++ifs, m >>= 1) {
		if (!(m & 1))
			continue;

		get_port_stats(bus, p, &rx_bytes, &tx_bytes);
		if (unlikely(!ifs->last_rx_bytes && !ifs->last_tx_bytes)) {
			ifs->last_rx_bytes = rx_bytes;
			ifs->last_tx_bytes = tx_bytes;
			continue;
		}

		diff += rx_bytes + tx_bytes;
		if (unlikely(bp->flags & BLED_FLAGS_DBG_CHECK_FUNC)) {
			prn_bl_v("GPIO#%d: port %2d, d %10lu (RX %10lu,%10lu / TX %10lu,%10lu)\n",
				bp->gpio_nr, p, rx_bytes + tx_bytes, ifs->last_rx_bytes, rx_bytes,
				ifs->last_tx_bytes, tx_bytes);
		}

		ifs->last_rx_bytes += rx_bytes;
		ifs->last_tx_bytes += tx_bytes;
		c++;
	}

	if (likely(diff >= bp->threshold))
		b = 1;
	if (unlikely(!c))
		bp->next_check_interval = BLED_WAIT_IF_INTERVAL;

	dev_put(dev);

	return b;
}

/**
 * Reset statistics information.
 * @bp:
 * @return:
 */
int swports_reset_check_traffic(struct bled_priv *bp)
{
	struct swport_bled_priv *sp = bp->check_priv;

	/* QCA8337 switch can't report total RX/TX bytes. Reset RX/TX bytes. */
	memset(sp->portstat, 0, sizeof(sp->portstat));
	return 0;
}

/**
 * Print private data of switch LED.
 * @bp:
 */
void swports_led_printer(struct bled_priv *bp, struct seq_file *m)
{
	int p;
	unsigned int mask;
	struct swport_bled_priv *sp;
	struct swport_bled_ifstat *ifs;

	if (!bp || !m)
		return;

	sp = (struct swport_bled_priv*) bp->check_priv;
	if (unlikely(!sp))
		return;

	mask= sp->port_mask;
	seq_printf(m, TFMT "%d\n", "Number of ports", sp->nr_port);
	for (p = 0, ifs = &sp->portstat[0]; mask > 0; ++p, ++ifs, mask >>= 1) {
		if (!(mask & 1))
			continue;
		seq_printf(m, TFMT "port %2d | %10lu / %10lu Bytes\n",
			"Port | RX/TX bytes", p, ifs->last_rx_bytes, ifs->last_tx_bytes);
	}

	return;
}
