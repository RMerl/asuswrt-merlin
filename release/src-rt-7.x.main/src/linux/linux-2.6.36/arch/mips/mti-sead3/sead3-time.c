/*
 * Carsten Langgaard, carstenl@mips.com
 * Copyright (C) 1999,2000 MIPS Technologies, Inc.  All rights reserved.
 *
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 * Setting up the clock on the MIPS boards.
 */

#include <linux/types.h>
#include <linux/init.h>
#include <linux/kernel_stat.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/timex.h>

#include <asm/mipsregs.h>
#include <asm/mipsmtregs.h>
#include <asm/hardirq.h>
#include <asm/i8253.h>
#include <asm/irq.h>
#include <asm/div64.h>
#include <asm/cpu.h>
#include <asm/time.h>
#include <asm/msc01_ic.h>

#include <asm/mips-boards/generic.h>
#include <asm/mips-boards/prom.h>

unsigned long cpu_khz;

static int mips_cpu_timer_irq;
static int mips_cpu_perf_irq;
extern int cp0_perfcount_irq;

static void mips_timer_dispatch(void)
{
	do_IRQ(mips_cpu_timer_irq);
}

static void mips_perf_dispatch(void)
{
	do_IRQ(mips_cpu_perf_irq);
}

static void __iomem *status_reg   = (void __iomem *)0xbf000410;

/*
 * Estimate CPU frequency.  Sets mips_hpt_frequency as a side-effect
 */
static unsigned int __init estimate_cpu_frequency(void)
{
	unsigned int prid = read_c0_prid() & 0xffff00;
	unsigned int tick = 0;
	unsigned int freq;
	unsigned int orig;
	unsigned long flags;

	local_irq_save(flags);

	orig = readl(status_reg) & 0x2;               /* get original sample */
	/* wait for transition */
	while ((readl(status_reg) & 0x2) == orig)
		;
	orig = orig ^ 0x2;                            /* flip the bit */

	write_c0_count(0);

	/* wait 1 second (the sampling clock transitions every 10ms) */
	while (tick < 100) {
		/* wait for transition */
		while ((readl(status_reg) & 0x2) == orig)
			;
		orig = orig ^ 0x2;                            /* flip the bit */
		tick++;
	}

	freq = read_c0_count();

	local_irq_restore(flags);

	mips_hpt_frequency = freq;

	/* Adjust for processor */
	if ((prid != (PRID_COMP_MIPS | PRID_IMP_20KC)) &&
		(prid != (PRID_COMP_MIPS | PRID_IMP_25KF)))
		freq *= 2;

	freq += 5000;        /* rounding */
	freq -= freq%10000;

	return freq ;
}

void read_persistent_clock(struct timespec *ts)
{
	ts->tv_sec = 0;
	ts->tv_nsec = 0;
}

static void __init plat_perf_setup(void)
{
	if (cp0_perfcount_irq >= 0) {
		if (cpu_has_vint)
			set_vi_handler(cp0_perfcount_irq, mips_perf_dispatch);
		mips_cpu_perf_irq = MIPS_CPU_IRQ_BASE + cp0_perfcount_irq;
	}
}

unsigned int __cpuinit get_c0_compare_int(void)
{
	if (cpu_has_vint)
		set_vi_handler(cp0_compare_irq, mips_timer_dispatch);
	mips_cpu_timer_irq = MIPS_CPU_IRQ_BASE + cp0_compare_irq;
	return mips_cpu_timer_irq;
}

void __init plat_time_init(void)
{
	unsigned int est_freq;

	est_freq = estimate_cpu_frequency();

	printk("CPU frequency %d.%02d MHz\n", est_freq/1000000,
	       (est_freq%1000000)*100/1000000);

	cpu_khz = est_freq / 1000;

	mips_scroll_message();

	plat_perf_setup();
}
