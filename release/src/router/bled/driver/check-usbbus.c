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
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
#include "../usb-bus-stats.h"
#else
#include <linux/usb/usb-bus-stats.h>
#endif
#include "../bled_defs.h"
#include "check.h"

/**
 * Evaluate TX/RX bytes of usb bus.
 * @bp:
 * @return:
 * 	< 0:			always turn on LED
 *  >= NR_BLED_INTERVAL_SET:	always turn on LED
 *  otherwise:			blink LED in accordance with bp->intervals[RETURN_VALUE]
 */
unsigned int usbbus_check_traffic(struct bled_priv *bp)
{
	int bus, c = 0;
	unsigned int b = 0, m;
	unsigned long d, diff = 0;
	struct usbbus_bled_stat *ifs;
	struct usbbus_bled_priv *up = bp->check_priv;
	struct usb_bus_stat_s *s;

	if (unlikely(bp->state != BLED_STATE_RUN))
		return -1;

	m = up->bus_mask;
	for (bus = 1, c = 0, diff = 0, ifs = &up->busstat[0]; m > 0; ++bus, ++ifs, m >>= 1) {
		if (!(m & 1))
			continue;

		s = &usb_bus_stat[bus];
		if (unlikely(!ifs->last_rx_bytes && !ifs->last_tx_bytes)) {
			ifs->last_rx_bytes = s->rx_bytes;
			ifs->last_tx_bytes = s->tx_bytes;
			continue;
		}

		d = bdiff(ifs->last_rx_bytes, s->rx_bytes) + bdiff(ifs->last_tx_bytes, s->tx_bytes);
		diff += d;
		if (unlikely(bp->flags & BLED_FLAGS_DBG_CHECK_FUNC)) {
			prn_bl_v("GPIO#%d: bus %2d, d %10lu (RX %10lu,%10lu / TX %10lu,%10lu)\n",
				bp->gpio_nr, bus, d, ifs->last_rx_bytes, s->rx_bytes,
				ifs->last_tx_bytes, s->tx_bytes);
		}

		ifs->last_rx_bytes = s->rx_bytes;
		ifs->last_tx_bytes = s->tx_bytes;
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
int usbbus_reset_check_traffic(struct bled_priv *bp)
{
	int bus;
	unsigned int m;
	struct usbbus_bled_stat *ifs;
	struct usbbus_bled_priv *up = bp->check_priv;
	struct usb_bus_stat_s *s;

	memset(up->busstat, 0, sizeof(up->busstat));
	m = up->bus_mask;
	for (bus = 1, ifs = &up->busstat[0]; m > 0; ++bus, ++ifs, m >>= 1) {
		if (!(m & 1))
			continue;

		s = &usb_bus_stat[bus];
		ifs->last_rx_bytes = s->rx_bytes;
		ifs->last_tx_bytes = s->tx_bytes;
	}

	return 0;
}

/**
 * Print private data of usb bus LED.
 * @bp:
 * @return:	length of printed bytes.
 */
void usbbus_led_printer(struct bled_priv *bp, struct seq_file *m)
{
	int b;
	unsigned int mask;
	struct usbbus_bled_priv *up;
	struct usbbus_bled_stat *ifs;

	if (!bp || !m)
		return;

	up = (struct usbbus_bled_priv*) bp->check_priv;
	if (unlikely(!up))
		return;

	mask = up->bus_mask;
	seq_printf(m, TFMT "%d\n", "Number of USB BUS", up->nr_bus);
	for (b = 1, ifs = &up->busstat[0]; mask> 0; ++b, ++ifs, mask >>= 1) {
		if (!(mask & 1))
			continue;
		seq_printf(m, TFMT "bus %2d | %10lu / %10lu Bytes\n",
			"BUS | RX/TX bytes", b, ifs->last_rx_bytes, ifs->last_tx_bytes);
	}

	return;
}
