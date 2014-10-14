/***************************************************************************/

/*
 *	timers.c -- generic ColdFire hardware timer support.
 *
 *	Copyright (C) 1999-2006, Greg Ungerer (gerg@snapgear.com)
 */

/***************************************************************************/

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/param.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/traps.h>
#include <asm/machdep.h>
#include <asm/coldfire.h>
#include <asm/mcftimer.h>
#include <asm/mcfsim.h>

/***************************************************************************/

/*
 *	By default use timer1 as the system clock timer.
 */
#define	TA(a)	(MCF_MBAR + MCFTIMER_BASE1 + (a))

/*
 *	Default the timer and vector to use for ColdFire. Some ColdFire
 *	CPU's and some boards may want different. Their sub-architecture
 *	startup code (in config.c) can change these if they want.
 */
unsigned int	mcf_timervector = 29;
unsigned int	mcf_profilevector = 31;
unsigned int	mcf_timerlevel = 5;

/*
 *	These provide the underlying interrupt vector support.
 *	Unfortunately it is a little different on each ColdFire.
 */
extern void mcf_settimericr(int timer, int level);
extern int mcf_timerirqpending(int timer);

#if defined(CONFIG_M532x)
#define	__raw_readtrr	__raw_readl
#define	__raw_writetrr	__raw_writel
#else
#define	__raw_readtrr	__raw_readw
#define	__raw_writetrr	__raw_writew
#endif

/***************************************************************************/

void coldfire_tick(void)
{
	/* Reset the ColdFire timer */
	__raw_writeb(MCFTIMER_TER_CAP | MCFTIMER_TER_REF, TA(MCFTIMER_TER));
}

/***************************************************************************/

static int ticks_per_intr;

void coldfire_timer_init(irq_handler_t handler)
{
	__raw_writew(MCFTIMER_TMR_DISABLE, TA(MCFTIMER_TMR));
	ticks_per_intr = (MCF_BUSCLK / 16) / HZ;
	__raw_writetrr(ticks_per_intr - 1, TA(MCFTIMER_TRR));
	__raw_writew(MCFTIMER_TMR_ENORI | MCFTIMER_TMR_CLK16 |
		MCFTIMER_TMR_RESTART | MCFTIMER_TMR_ENABLE, TA(MCFTIMER_TMR));

	request_irq(mcf_timervector, handler, IRQF_DISABLED, "timer", NULL);
	mcf_settimericr(1, mcf_timerlevel);

#ifdef CONFIG_HIGHPROFILE
	coldfire_profile_init();
#endif
}

/***************************************************************************/

unsigned long coldfire_timer_offset(void)
{
	unsigned long tcn, offset;

	tcn = __raw_readw(TA(MCFTIMER_TCN));
	offset = ((tcn + 1) * (1000000 / HZ)) / ticks_per_intr;

	/* Check if we just wrapped the counters and maybe missed a tick */
	if ((offset < (1000000 / HZ / 2)) && mcf_timerirqpending(1))
		offset += 1000000 / HZ;
	return offset;
}

/***************************************************************************/
#ifdef CONFIG_HIGHPROFILE
/***************************************************************************/

/*
 *	By default use timer2 as the profiler clock timer.
 */
#define	PA(a)	(MCF_MBAR + MCFTIMER_BASE2 + (a))

/*
 *	Choose a reasonably fast profile timer. Make it an odd value to
 *	try and get good coverage of kernel operations.
 */
#define	PROFILEHZ	1013

/*
 *	Use the other timer to provide high accuracy profiling info.
 */
irqreturn_t coldfire_profile_tick(int irq, void *dummy)
{
	/* Reset ColdFire timer2 */
	__raw_writeb(MCFTIMER_TER_CAP | MCFTIMER_TER_REF, PA(MCFTIMER_TER));
	if (current->pid)
		profile_tick(CPU_PROFILING, regs);
	return IRQ_HANDLED;
}

/***************************************************************************/

void coldfire_profile_init(void)
{
	printk(KERN_INFO "PROFILE: lodging TIMER2 @ %dHz as profile timer\n", PROFILEHZ);

	/* Set up TIMER 2 as high speed profile clock */
	__raw_writew(MCFTIMER_TMR_DISABLE, PA(MCFTIMER_TMR));

	__raw_writetrr(((MCF_CLK / 16) / PROFILEHZ), PA(MCFTIMER_TRR));
	__raw_writew(MCFTIMER_TMR_ENORI | MCFTIMER_TMR_CLK16 |
		MCFTIMER_TMR_RESTART | MCFTIMER_TMR_ENABLE, PA(MCFTIMER_TMR));

	request_irq(mcf_profilevector, coldfire_profile_tick,
		(IRQF_DISABLED | IRQ_FLG_FAST), "profile timer", NULL);
	mcf_settimericr(2, 7);
}

/***************************************************************************/
#endif	/* CONFIG_HIGHPROFILE */
/***************************************************************************/
