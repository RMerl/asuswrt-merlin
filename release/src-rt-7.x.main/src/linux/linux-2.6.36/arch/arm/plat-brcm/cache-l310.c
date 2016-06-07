/* Modified by Broadcom Corp. Portions Copyright (c) Broadcom Corp, 2012. */
/*
 * arch/arm/mm/cache-l230.c - L310 cache controller support
 *
 * Copyright (C) 2007 ARM Limited
 * Copyright (c) 2012 Broadcom, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * Modified for L310 and improved performance it allows.
 */
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/module.h>

#include <asm/cacheflush.h>
#include <asm/hardware/cache-l2x0.h>	/* Old register offsets */

#include <typedefs.h>
#include <bcmdefs.h>

#define CACHE_LINE_SIZE		32

static void __iomem *l2x0_base;
static DEFINE_SPINLOCK(l2x0_lock);
static uint32_t l2x0_way_mask;	/* Bitmask of active ways */
int l2x0_irq = 32 ;

DEFINE_SPINLOCK(l2x0_reg_lock);
EXPORT_SYMBOL(l2x0_reg_lock);

#define L2C_WAR_LOCK(flags) \
	do { \
		if (ACP_WAR_ENAB()) \
			spin_lock_irqsave(&l2x0_reg_lock, flags); \
	} while(0)

#define L2C_WAR_UNLOCK(flags) \
	do { \
		if (ACP_WAR_ENAB()) \
			spin_unlock_irqrestore(&l2x0_reg_lock, flags); \
	} while(0)

static inline void cache_wait(void __iomem *reg, unsigned long mask)
{
	/* wait for the operation to complete */
	while (readl_relaxed(reg) & mask)
		;
}

/*
 * Atomic operations
 * 
 * The following are atomic operations:
 * . Clean Line by PA or by Set/Way.
 * . Invalidate Line by PA.
 * . Clean and Invalidate Line by PA or by Set/Way.
 * . Cache Sync.
 * These operations stall the slave ports until they are complete.
 * When these registers are read, bit [0], the C flag,
 * indicates that a background operation is in progress.
 * When written, bit 0 must be zero.
 */
static inline void atomic_cache_sync( void __iomem *base )
{
	writel_relaxed(0, base + L2X0_CACHE_SYNC);
}

static inline void atomic_clean_line( void __iomem *base, unsigned long addr)
{
	writel_relaxed(addr, base + L2X0_CLEAN_LINE_PA);
}

static inline void atomic_inv_line( void __iomem *base, unsigned long addr)
{
	writel_relaxed(addr, base + L2X0_INV_LINE_PA);
}

static inline void atomic_flush_line( void __iomem *base, unsigned long addr)
{
	writel_relaxed(addr, base + L2X0_CLEAN_INV_LINE_PA);
}

/*
 * Atomic operations do not require the use of the spinlock
 */

static void l2x0_cache_sync(void)
{
	void __iomem *base = l2x0_base;
	unsigned long flags = 0;

	L2C_WAR_LOCK(flags);
	atomic_cache_sync( base );
	L2C_WAR_UNLOCK(flags);
}

static void BCMFASTPATH l2x0_inv_range(unsigned long start, unsigned long end)
{
	void __iomem *base = l2x0_base;
	unsigned long flags = 0;

	L2C_WAR_LOCK(flags);

	/* Range edges could contain live dirty data */
	if( start & (CACHE_LINE_SIZE-1) )
		atomic_flush_line(base, start & ~(CACHE_LINE_SIZE-1));
	if( end & (CACHE_LINE_SIZE-1) )
		atomic_flush_line(base, end & ~(CACHE_LINE_SIZE-1));

	start &= ~(CACHE_LINE_SIZE - 1);

	while (start < end) {
		atomic_inv_line(base, start);
		start += CACHE_LINE_SIZE;
	}
	atomic_cache_sync(base);

	L2C_WAR_UNLOCK(flags);
}

static void BCMFASTPATH l2x0_clean_range(unsigned long start, unsigned long end)
{
	void __iomem *base = l2x0_base;
	unsigned long flags = 0;

	L2C_WAR_LOCK(flags);

	start &= ~(CACHE_LINE_SIZE - 1);

	while (start < end) {
		atomic_clean_line(base, start);
		start += CACHE_LINE_SIZE;
	}
	atomic_cache_sync(base);

	L2C_WAR_UNLOCK(flags);
}

static void l2x0_flush_range(unsigned long start, unsigned long end)
{
	void __iomem *base = l2x0_base;
	unsigned long flags = 0;

	L2C_WAR_LOCK(flags);

	start &= ~(CACHE_LINE_SIZE - 1);
	while (start < end) {
		atomic_flush_line(base, start);
		start += CACHE_LINE_SIZE;
	}
	atomic_cache_sync(base);

	L2C_WAR_UNLOCK(flags);
}

/*
 * Invalidate by way is non-atomic, background operation
 * has to be protected with the spinlock.
 */
static inline void l2x0_inv_all(void)
{
	void __iomem *base = l2x0_base;
	unsigned long flags;
	unsigned long flags2 = 0;

	L2C_WAR_LOCK(flags2);

	/* invalidate all ways */
	spin_lock_irqsave(&l2x0_lock, flags);
	writel_relaxed(l2x0_way_mask, base + L2X0_INV_WAY);
	cache_wait(base + L2X0_INV_WAY, l2x0_way_mask);
	atomic_cache_sync(base);
	spin_unlock_irqrestore(&l2x0_lock, flags);

	L2C_WAR_UNLOCK(flags2);
}

static irqreturn_t l2x0_isr( int irq, void * cookie )
{
	u32 reg ;
	/* Read pending interrupts */
	reg = readl_relaxed(l2x0_base + L2X0_RAW_INTR_STAT);
	/* Acknowledge the interupts */
	writel_relaxed(reg, l2x0_base + L2X0_INTR_CLEAR);
	printk(KERN_WARNING "L310: interrupt bits %#x\n", reg );

	return IRQ_HANDLED ;
}

unsigned int
l2x0_read_event_cnt(int idx)
{
	unsigned int val;

	if (idx == 1)
		val = readl_relaxed(l2x0_base + L2X0_EVENT_CNT1_VAL);
	else if (idx == 0)
		val = readl_relaxed(l2x0_base + L2X0_EVENT_CNT0_VAL);
	else
		val = -1;

	return val;
}

void __init l310_init(void __iomem *base, u32 aux_val, u32 aux_mask, int irq)
{
	__u32 aux;
	__u32 cache_id;
	int ways;

	l2x0_base = base;
	l2x0_irq = irq;

	cache_id = readl_relaxed(l2x0_base + L2X0_CACHE_ID);
	aux = readl_relaxed(l2x0_base + L2X0_AUX_CTRL);

	aux &= aux_mask;
	aux |= aux_val;

	/* This module unly supports the L310 */
	BUG_ON((cache_id & L2X0_CACHE_ID_PART_MASK) != L2X0_CACHE_ID_PART_L310);

	/* Determine the number of ways */
	if (aux & (1 << 16))
		ways = 16;
	else
		ways = 8;

	l2x0_way_mask = (1 << ways) - 1;

	if (ACP_WAR_ENAB() || arch_is_coherent()) {
		/* Enable L2C filtering */
		writel_relaxed(PHYS_OFFSET + SZ_1G, l2x0_base + L2X0_FILT_END);
		writel_relaxed((PHYS_OFFSET | 1), l2x0_base + L2X0_FILT_START);
	}

	/*
	 * Check if l2x0 controller is already enabled.
	 * If you are booting from non-secure mode
	 * accessing the below registers will fault.
	 */
	if (!(readl_relaxed(l2x0_base + L2X0_CTRL) & 1)) {

		/* l2x0 controller is disabled */
		writel_relaxed(aux, l2x0_base + L2X0_AUX_CTRL);

		l2x0_inv_all();

		/* enable L2X0 */
		writel_relaxed(1, l2x0_base + L2X0_CTRL);
	}

 	/* Enable interrupts */
 	WARN_ON( request_irq( l2x0_irq, l2x0_isr, 0, "L2C", NULL ));
 	writel_relaxed(0x00ff, l2x0_base + L2X0_INTR_MASK);

	outer_cache.inv_range = l2x0_inv_range;
	outer_cache.clean_range = l2x0_clean_range;
	outer_cache.flush_range = l2x0_flush_range;
	outer_cache.sync = l2x0_cache_sync;

	/* configure total hits */
	writel_relaxed((2 << 2), l2x0_base + L2X0_EVENT_CNT1_CFG);

	/* configure total read accesses */
	writel_relaxed((3 << 2), l2x0_base + L2X0_EVENT_CNT0_CFG);

	/* enable event counting */
	writel_relaxed(0x1, l2x0_base + L2X0_EVENT_CNT_CTRL);

	printk(KERN_INFO "L310: cache controller enabled %d ways, "
			"CACHE_ID 0x%08x, AUX_CTRL 0x%08x\n",
			 ways, cache_id, aux);
}
EXPORT_SYMBOL(l2x0_read_event_cnt);
