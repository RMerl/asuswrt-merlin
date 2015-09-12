/*
 * Copyright (C) 2015, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: time.c,v 1.9 2009-07-17 06:23:12 $
 */
#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
#include <linux/config.h>
#endif
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/serial_reg.h>
#include <linux/interrupt.h>
#include <asm/addrspace.h>
#include <asm/io.h>
#include <asm/time.h>

#include <typedefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <bcmnvram.h>
#include <hndsoc.h>
#include <sbchipc.h>
#include <siutils.h>
#include <hndmips.h>
#include <mipsinc.h>
#include <hndcpu.h>
#include <bcmdevs.h>

/* Global SB handle */
extern si_t *bcm947xx_sih;
extern spinlock_t bcm947xx_sih_lock;

/* Convenience */
#define sih bcm947xx_sih
#define sih_lock bcm947xx_sih_lock

#define WATCHDOG_MIN	3000	/* milliseconds */
extern int panic_timeout;
extern int panic_on_oops;
static int watchdog = 0;

#ifndef	CONFIG_HWSIM
static u8 *mcr = NULL;
#endif /* CONFIG_HWSIM */

static void __init
bcm947xx_time_init(void)
{
	unsigned int hz;
	char cn[8];

	/*
	 * Use deterministic values for initial counter interrupt
	 * so that calibrate delay avoids encountering a counter wrap.
	 */
	write_c0_count(0);
	write_c0_compare(0xffff);

	if (!(hz = si_cpu_clock(sih)))
		hz = 100000000;

	bcm_chipname(sih->chip, cn, 8);
	printk(KERN_INFO "CPU: BCM%s rev %d at %d MHz\n", cn, sih->chiprev,
	       (hz + 500000) / 1000000);

	/* Set MIPS counter frequency for fixed_rate_gettimeoffset() */
	mips_hpt_frequency = hz / 2;

	/* Set watchdog interval in ms */
	watchdog = simple_strtoul(nvram_safe_get("watchdog"), NULL, 0);

	/* Ensure at least WATCHDOG_MIN */
	if ((watchdog > 0) && (watchdog < WATCHDOG_MIN))
		watchdog = WATCHDOG_MIN;

	/* Set panic timeout in seconds */
	panic_timeout = watchdog / 1000;
	panic_on_oops = watchdog / 1000;
}

#ifdef CONFIG_HND_BMIPS3300_PROF
extern bool hndprofiling;
#ifdef CONFIG_MIPS64
typedef u_int64_t sbprof_pc;
#else
typedef u_int32_t sbprof_pc;
#endif
extern void sbprof_cpu_intr(sbprof_pc restartpc);
#endif	/* CONFIG_HND_BMIPS3300_PROF */

static irqreturn_t
bcm947xx_timer_interrupt(int irq, void *dev_id)
{
#ifdef CONFIG_HND_BMIPS3300_PROF
	/*
	 * Are there any ExcCode or other mean(s) to determine what has caused
	 * the timer interrupt? For now simply stop the normal timer proc if
	 * count register is less than compare register.
	 */
	if (hndprofiling) {
		sbprof_cpu_intr(read_c0_epc() +
		                ((read_c0_cause() >> (CAUSEB_BD - 2)) & 4));
		if (read_c0_count() < read_c0_compare())
			return (IRQ_HANDLED);
	}
#endif	/* CONFIG_HND_BMIPS3300_PROF */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
	/* Generic MIPS timer code */
	timer_interrupt(irq, dev_id);
#else	/* 2.6.36 and up */
	{
	/* There is no more a MIPS generic timer ISR */
	struct clock_event_device *cd = dev_id;
	BUG_ON( ! cd );
	cd->event_handler(cd);
	/* Clear Count/Compare Interrupt */
	write_c0_compare(read_c0_count() + mips_hpt_frequency / HZ);
	}
#endif

	/* Set the watchdog timer to reset after the specified number of ms */
	if (watchdog > 0)
		si_watchdog_ms(sih, watchdog);

#ifdef	CONFIG_HWSIM
	(*((int *)0xa0000f1c))++;
#else
	/* Blink one of the LEDs in the external UART */
	if (mcr && !(jiffies % (HZ/2)))
		writeb(readb(mcr) ^ UART_MCR_OUT2, mcr);
#endif

	return (IRQ_HANDLED);
}


#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36)
static void bcm947xx_clockevent_set_mode(enum clock_event_mode mode,
	struct clock_event_device *cd)
{
	printk( KERN_CRIT "bcm947xx_clockevent_set_mode: %d\n", mode );
	/* Need to add mode switch to support both
	periodic and one-shot operation here */
}
#ifdef BRCM_TIMER_ONESHOT
/* This is used in one-shot operation mode */
static int bcm947xx_clockevent_set_next(unsigned long delta,
	struct clock_event_device *cd)
{
        unsigned int cnt;
        int res;

	printk( KERN_CRIT "bcm947xx_clockevent_set_next: %#lx\n", delta );

        cnt = read_c0_count();
        cnt += delta;
        write_c0_compare(cnt);
        res = ((int)(read_c0_count() - cnt) >= 0) ? -ETIME : 0;
        return res;
}
#endif

struct clock_event_device bcm947xx_clockevent = {
	.name		= "bcm947xx",
	.features	= CLOCK_EVT_FEAT_PERIODIC,
	.rating		= 300,
	.irq		= 7,
	.set_mode 	= bcm947xx_clockevent_set_mode,
#ifdef BRCM_TIMER_ONESHOT
	.set_next_event = bcm947xx_clockevent_set_next, 
#endif
};
#endif

/* named initialization should work on earlier 2.6 too */
static struct irqaction bcm947xx_timer_irqaction = {
	.handler	= bcm947xx_timer_interrupt,
	.flags		= IRQF_DISABLED | IRQF_TIMER,
	.name		= "bcm947xx timer",
	.dev_id		= &bcm947xx_clockevent,
};

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
void __init plat_timer_setup(struct irqaction *irq)
{
	/* Enable the timer interrupt */
	setup_irq(7, &bcm947xx_timer_irqaction);
}
#else
void __init plat_time_init(void)
{
	struct clock_event_device *cd = &bcm947xx_clockevent;
	const u8 irq = 7;

	/* Initialize the timer */
	bcm947xx_time_init();

	cd->cpumask = cpumask_of(smp_processor_id());

        clockevent_set_clock(cd, mips_hpt_frequency);
#ifdef BRCM_TIMER_ONESHOT
        /* Calculate the min / max delta */
        cd->max_delta_ns        = clockevent_delta2ns(0x7fffffff, cd);
        cd->min_delta_ns        = clockevent_delta2ns(0x300, cd);
#endif
	clockevents_register_device(cd);

	/* Enable the timer interrupt */
	setup_irq(irq, &bcm947xx_timer_irqaction);
}
#endif
