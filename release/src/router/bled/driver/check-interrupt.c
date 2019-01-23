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
#include <linux/irq.h>
#include <linux/interrupt.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
#include "../kernel_stat.h"
#else
#include <linux/kernel_stat.h>
#endif
#include "../bled_defs.h"
#include "check.h"

static unsigned long get_stats(unsigned int interrupt)
{
	int j;
	unsigned long s = 0, flags;
	struct irqaction *action;
	struct irq_desc *desc;

	if (unlikely(!(desc = irq_to_desc(interrupt))))
		return 0;

	raw_spin_lock_irqsave(&desc->lock, flags);
	action = desc->action;
	if (unlikely(!action))
		goto exit__get_stats;

	for_each_online_cpu(j)
		s += kstat_irqs_cpu(interrupt, j);

exit__get_stats:
	raw_spin_unlock_irqrestore(&desc->lock, flags);

	return s;
}

/**
 * Evaluate number of interrupts of interrupt(s).
 * @bp:
 * @return:
 * 	< 0:			always turn on LED
 *  >= NR_BLED_INTERVAL_SET:	always turn on LED
 *  otherwise:			blink LED in accordance with bp->intervals[RETURN_VALUE]
 */
unsigned int interrupt_check_traffic(struct bled_priv *bp)
{
	int i, c;
	unsigned int b = 0;
	unsigned long d, diff, s;
	struct interrupt_bled_priv *ip = bp->check_priv;
	struct interrupt_bled_stat *intrs;

	if (bp->state != BLED_STATE_RUN)
		return -1;

	for (i = 0, c = 0, diff = 0, intrs = &ip->interrupt_stat[0]; i < ip->nr_interrupt; ++i, ++intrs) {
		s = get_stats(intrs->interrupt);
		if (unlikely(!intrs->last_nr_interrupts)) {
			intrs->last_nr_interrupts = s;
			continue;
		}

		d = bdiff(intrs->last_nr_interrupts, s);
		diff += d;
		if (unlikely(bp->flags & BLED_FLAGS_DBG_CHECK_FUNC)) {
			prn_bl_v("GPIO#%d: interrupt %u, d %10lu (%10lu,%10lu)\n",
				bp->gpio_nr, intrs->interrupt, d, intrs->last_nr_interrupts, s);
		}

		intrs->last_nr_interrupts = s;
		c++;
	}

	diff <<= 10;	/* adjust number of interrupts due to unit of bp->threshold is KB/s */
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
int interrupt_reset_check_traffic(struct bled_priv *bp)
{
	int i, c, ret = -1;
	struct interrupt_bled_priv *ip = bp->check_priv;
	struct interrupt_bled_stat *intrs;

	for (i = 0, c = 0, intrs = &ip->interrupt_stat[0]; i < ip->nr_interrupt; ++i, ++intrs) {
		intrs->last_nr_interrupts = get_stats(intrs->interrupt);
		c++;
	}
	if (ip->nr_interrupt == c)
		ret = 0;
	if (bp->type == BLED_TYPE_INTERRUPT_BLED && !c)
		bp->next_check_interval = BLED_WAIT_IF_INTERVAL;

	return ret;
}

/**
 * Print private data of interrupt LED.
 * @bp:
 */
void interrupt_led_printer(struct bled_priv *bp, struct seq_file *m)
{
	int i;
	char str[64];
	struct irq_desc *desc;
	struct interrupt_bled_priv *ip;
	struct interrupt_bled_stat *intrs;

	if (!bp || !m)
		return;

	ip = (struct interrupt_bled_priv*) bp->check_priv;
	if (unlikely(!ip))
		return;

	seq_printf(m, TFMT "%d\n", "Number of interrupts", ip->nr_interrupt);
	for (i = 0, intrs = &ip->interrupt_stat[0]; i < ip->nr_interrupt; ++i, ++intrs) {
		desc = irq_to_desc(intrs->interrupt);
		if (unlikely(!desc)) {
			seq_printf(m, TFMT "%8u | %s\n", "interrupt",
				intrs->interrupt, "N/A");
			continue;
		}

		sprintf(str, "interrupt | count");
		seq_printf(m, TFMT "%4u | %10lu\n", str,
			intrs->interrupt, intrs->last_nr_interrupts);
	}

	return;
}
