/*
 *  Copyright (C) 1999 - 2003 ARM Limited
 *  Copyright (C) 2000 Deep Blue Solutions Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/clockchips.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/io.h>

#include <plat/mpcore.h>
#if defined(CONFIG_BUZZZ)
#include <asm/buzzz.h>
#endif  /*  CONFIG_BUZZZ */

/*
 * The ARM9 MPCORE Global Timer is a continously-running 64-bit timer,
 * which is used both as a "clock source" and as a "clock event" -
 * there is a banked per-cpu compare and reload registers that are
 * used to generated either one-shot or periodic interrupts on the cpu
 * that calls the mode_set function.
 *
 * NOTE: This code does not support dynamic change of the source clock
 * frequency. The interrupt interval is only calculated once during
 * initialization.
 */

/*
 * Global Timer Registers
 */
#define	GTIMER_COUNT_LO		0x00	/* Lower 32 of 64 bits counter */
#define	GTIMER_COUNT_HI		0x04	/* Higher 32 of 64 bits counter */
#define	GTIMER_CTRL		0x08	/* Control (partially banked) */
#define	GTIMER_CTRL_EN		(1<<0)	/* Timer enable bit */
#define	GTIMER_CTRL_CMP_EN	(1<<1)	/* Comparator enable */
#define	GTIMER_CTRL_IRQ_EN	(1<<2)	/* Interrupt enable */
#define	GTIMER_CTRL_AUTO_EN	(1<<3)	/* Auto-increment enable */
#define	GTIMER_INT_STAT		0x0C	/* Interrupt Status (banked) */
#define	GTIMER_COMP_LO		0x10	/* Lower half comparator (banked) */
#define	GTIMER_COMP_HI		0x14	/* Upper half comparator (banked) */
#define	GTIMER_RELOAD		0x18	/* Auto-increment (banked) */

#define	GTIMER_MIN_RANGE	30	/* Minimum wrap-around time in sec */

/* Gobal variables */
static void __iomem *gtimer_base;
static u32 ticks_per_jiffy;

extern void soc_watchdog(void);


static cycle_t gptimer_count_read(struct clocksource *cs)
{
	u32 count_hi, count_ho, count_lo;
	u64 count;

	/* Avoid unexpected rollover with double-read of upper half */
	do {
		count_hi = readl( gtimer_base + GTIMER_COUNT_HI );
		count_lo = readl( gtimer_base + GTIMER_COUNT_LO );
		count_ho = readl( gtimer_base + GTIMER_COUNT_HI );
	} while( count_hi != count_ho );

	count = (u64) count_hi << 32 | count_lo ;
	return count;
}

static struct clocksource clocksource_gptimer = {
	.name		= "mpcore_gtimer",
	.rating		= 300,
	.read		= gptimer_count_read,
	.mask		= CLOCKSOURCE_MASK(64),
	.shift		= 20,
	.flags		= CLOCK_SOURCE_IS_CONTINUOUS,
};

static void __init gptimer_clocksource_init(u32 freq)
{
	struct clocksource *cs = &clocksource_gptimer;

	/* <freq> is timer clock in Hz */
        clocksource_calc_mult_shift(cs, freq, GTIMER_MIN_RANGE);

	clocksource_register(cs);
}

/*
 * IRQ handler for the global timer
 * This interrupt is banked per CPU so is handled identically
 */
static irqreturn_t gtimer_interrupt(int irq, void *dev_id)
{
	struct clock_event_device *evt = dev_id;

	/* clear the interrupt */
	writel(1, gtimer_base + GTIMER_INT_STAT);

#if defined(BUZZZ_KEVT_LVL) && (BUZZZ_KEVT_LVL >= 2)
	buzzz_kevt_log1(BUZZZ_KEVT_ID_GTIMER_EVENT, (u32)evt->event_handler);
#endif	/* BUZZZ_KEVT_LVL */

	evt->event_handler(evt);

	soc_watchdog();

	return IRQ_HANDLED;
}

static void gtimer_set_mode(
	enum clock_event_mode mode,
	struct clock_event_device *evt
	)
{
	u32 ctrl, period;
	u64 count;

	/* Get current register with global enable and prescaler */
	ctrl = readl( gtimer_base + GTIMER_CTRL );

	/* Clear the mode-related bits */
	ctrl &= ~( 	GTIMER_CTRL_CMP_EN | 
			GTIMER_CTRL_IRQ_EN | 
			GTIMER_CTRL_AUTO_EN);

	switch (mode) {
	case CLOCK_EVT_MODE_PERIODIC:
		period = ticks_per_jiffy;
		count = gptimer_count_read( NULL );
		count += period ;
		writel(ctrl, gtimer_base + GTIMER_CTRL);
		writel(count & 0xffffffffUL, 	gtimer_base + GTIMER_COMP_LO);
		writel(count >> 32, 		gtimer_base + GTIMER_COMP_HI);
		writel(period, gtimer_base + GTIMER_RELOAD);
		ctrl |= GTIMER_CTRL_CMP_EN |
			GTIMER_CTRL_IRQ_EN |
			GTIMER_CTRL_AUTO_EN ;
		break;

	case CLOCK_EVT_MODE_ONESHOT:
		/* period set, and timer enabled in 'next_event' hook */
		break;

	case CLOCK_EVT_MODE_UNUSED:
	case CLOCK_EVT_MODE_SHUTDOWN:
	default:
		break;
	}
	/* Apply the new mode */
	writel(ctrl, gtimer_base + GTIMER_CTRL);
}

static int gtimer_set_next_event(
	unsigned long next,
	struct clock_event_device *evt
	)
{
	u32 ctrl;
	u64 count;

#if defined(BUZZZ_KEVT_LVL) && (BUZZZ_KEVT_LVL >= 2)
	buzzz_kevt_log1(BUZZZ_KEVT_ID_GTIMER_NEXT, (u32)next);
#endif	/* BUZZZ_KEVT_LVL */

	ctrl = readl(gtimer_base + GTIMER_CTRL);
	count = gptimer_count_read( NULL );

	ctrl &= ~GTIMER_CTRL_CMP_EN ;
	writel(ctrl, gtimer_base + GTIMER_CTRL);

	count += next ;

	writel(count & 0xffffffffUL, 	gtimer_base + GTIMER_COMP_LO);
	writel(count >> 32, 		gtimer_base + GTIMER_COMP_HI);

	/* enable IRQ for the same cpu that loaded comparator */
	ctrl |= GTIMER_CTRL_CMP_EN ;
	ctrl |= GTIMER_CTRL_IRQ_EN ;

	writel(ctrl, gtimer_base + GTIMER_CTRL);

	return 0;
}

static struct clock_event_device gtimer_clockevent = {
	.name		= "mpcore_gtimer",
	.shift		= 20,
	.features       = CLOCK_EVT_FEAT_PERIODIC,
	.set_mode	= gtimer_set_mode,
	.set_next_event	= gtimer_set_next_event,
	.rating		= 300,
	.cpumask	= cpu_all_mask,
};

static struct irqaction gtimer_irq = {
	.name		= "mpcore_gtimer",
	.flags		= IRQF_DISABLED | IRQF_TIMER | IRQF_PERCPU,
	.handler	= gtimer_interrupt,
	.dev_id		= &gtimer_clockevent,
};

static void __init gtimer_clockevents_init(u32 freq, unsigned timer_irq)
{
	struct clock_event_device *evt = &gtimer_clockevent;

	evt->irq = timer_irq;
        ticks_per_jiffy = DIV_ROUND_CLOSEST(freq, HZ);

        clockevents_calc_mult_shift(evt, freq, GTIMER_MIN_RANGE);

	evt->max_delta_ns = clockevent_delta2ns(0xffffffff, evt);
	evt->min_delta_ns = clockevent_delta2ns(0xf, evt);

	/* Register the device to install handler before enabing IRQ */
	clockevents_register_device(evt);
	setup_irq(timer_irq, &gtimer_irq);
}

/*
 * MPCORE Global Timer initialization function
 */
void __init mpcore_gtimer_init( 
	void __iomem *base, 
	unsigned long freq,
	unsigned int timer_irq)
{
	u32 ctrl ;
	u64 count;

	gtimer_base = base;

	printk(KERN_INFO "MPCORE Global Timer Clock %luHz\n", 
		(unsigned long) freq);

	/* Prescaler = 0; let the Global Timer run at native PERIPHCLK rate */

	ctrl = GTIMER_CTRL_EN;

	/* Enable the free-running global counter */

	writel(ctrl, gtimer_base + GTIMER_CTRL);

	/* Self-test the timer is running */
	count = gptimer_count_read(NULL);

	/* Register as time source */
	gptimer_clocksource_init(freq);

	/* Register as system timer */
	gtimer_clockevents_init(freq, timer_irq);

	count = gptimer_count_read(NULL) - count ;
	if( count == 0 )
		printk(KERN_CRIT "MPCORE Global Timer Dead!!\n");
}
