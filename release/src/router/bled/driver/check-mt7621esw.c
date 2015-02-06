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
#include "../bled_defs.h"
#include "check.h"

/* copy from rt_mmap.h and raether.c */
#define RALINK_ETH_SW_BASE		0xBE110000
#define REG_ESW_PORT_TGOCN_LOW_P0       0x4048
#define REG_ESW_PORT_TGOCN_HIGH_P0      0x404C
#define REG_ESW_PORT_RGOCN_LOW_P0       0x40A8
#define REG_ESW_PORT_RGOCN_HIGH_P0      0x40AC
#define _ESW_REG(x)			(*((volatile u32 *)(RALINK_ETH_SW_BASE + x)))

/**
 * Get TX/RX bytes from MT7621 embedded switch.
 * @port:
 * @rx:
 * @tx:
 */
static inline void get_port_stats(int port, unsigned long *rx, unsigned long *tx)
{
	*rx = _ESW_REG(REG_ESW_PORT_RGOCN_LOW_P0 + port * 0x100) & 0xffff | _ESW_REG(REG_ESW_PORT_RGOCN_HIGH_P0 + port * 0x100)<<16;
	*tx = _ESW_REG(REG_ESW_PORT_TGOCN_LOW_P0 + port * 0x100) & 0xffff | _ESW_REG(REG_ESW_PORT_TGOCN_HIGH_P0 + port * 0x100) <<16;
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
	unsigned long d, diff, rx_bytes, tx_bytes;
	struct swport_bled_ifstat *ifs;
	struct swport_bled_priv *sp = bp->check_priv;

	m = sp->port_mask;
	for (p = 0, c = 0, diff = 0, ifs = &sp->portstat[0]; m > 0; ++p, ++ifs, m >>= 1) {
		if (!(m & 1))
			continue;

		get_port_stats(p, &rx_bytes, &tx_bytes);
		if (unlikely(!ifs->last_rx_bytes && !ifs->last_tx_bytes)) {
			ifs->last_rx_bytes = rx_bytes;
			ifs->last_tx_bytes = tx_bytes;
			continue;
		}

		d = bdiff(ifs->last_rx_bytes, rx_bytes) + bdiff(ifs->last_tx_bytes, tx_bytes);
		diff += d;
		if (unlikely(bp->flags & BLED_FLAGS_DBG_CHECK_FUNC)) {
			prn_bl_v("GPIO#%d: port %2d, d %10lu (RX %10lu,%10lu / TX %10lu,%10lu)\n",
				bp->gpio_nr, p, d, ifs->last_rx_bytes, rx_bytes,
				ifs->last_tx_bytes, tx_bytes);
		}

		ifs->last_rx_bytes = rx_bytes;
		ifs->last_tx_bytes = tx_bytes;
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
int swports_reset_check_traffic(struct bled_priv *bp)
{
	int p;
	unsigned int m;
	unsigned long rx_bytes, tx_bytes;
	struct swport_bled_ifstat *ifs;
	struct swport_bled_priv *sp = bp->check_priv;

	m = sp->port_mask;
	for (p = 0, ifs = &sp->portstat[0]; m > 0; ++p, ++ifs, m >>= 1) {
		ifs->last_rx_bytes = 0;
		ifs->last_tx_bytes = 0;
		if (!(m & 1))
			continue;

		get_port_stats(p, &rx_bytes, &tx_bytes);
		ifs->last_rx_bytes = rx_bytes;
		ifs->last_tx_bytes = tx_bytes;
	}

	return 0;
}

/**
 * Print private data of switch LED.
 * @bp:
 * @return:	length of printed bytes.
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

	mask = sp->port_mask;
	seq_printf(m, TFMT "%d\n", "Number of ports", sp->nr_port);
	for (p = 0, ifs = &sp->portstat[0]; mask > 0; ++p, ++ifs, mask >>= 1) {
		if (!(mask & 1))
			continue;
		seq_printf(m, TFMT "port %2d | %10lu / %10lu Bytes\n",
			"Port | RX/TX bytes", p, ifs->last_rx_bytes, ifs->last_tx_bytes);
	}

	return;
}
