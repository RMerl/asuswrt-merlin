/*
 *
 * arch/arm/mach-u300/timer.c
 *
 *
 * Copyright (C) 2007-2009 ST-Ericsson AB
 * License terms: GNU General Public License (GPL) version 2
 * Timer COH 901 328, runs the OS timer interrupt.
 * Author: Linus Walleij <linus.walleij@stericsson.com>
 */
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/timex.h>
#include <linux/clockchips.h>
#include <linux/clocksource.h>
#include <linux/types.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/err.h>

#include <mach/hardware.h>

/* Generic stuff */
#include <asm/sched_clock.h>
#include <asm/mach/map.h>
#include <asm/mach/time.h>
#include <asm/mach/irq.h>

/* Be able to sleep for atleast 4 seconds (usually more) */
#define APPTIMER_MIN_RANGE 4

/*
 * APP side special timer registers
 * This timer contains four timers which can fire an interrupt each.
 * OS (operating system) timer @ 32768 Hz
 * DD (device driver) timer @ 1 kHz
 * GP1 (general purpose 1) timer @ 1MHz
 * GP2 (general purpose 2) timer @ 1MHz
 */

/* Reset OS Timer 32bit (-/W) */
#define U300_TIMER_APP_ROST					(0x0000)
#define U300_TIMER_APP_ROST_TIMER_RESET				(0x00000000)
/* Enable OS Timer 32bit (-/W) */
#define U300_TIMER_APP_EOST					(0x0004)
#define U300_TIMER_APP_EOST_TIMER_ENABLE			(0x00000000)
/* Disable OS Timer 32bit (-/W) */
#define U300_TIMER_APP_DOST					(0x0008)
#define U300_TIMER_APP_DOST_TIMER_DISABLE			(0x00000000)
/* OS Timer Mode Register 32bit (-/W) */
#define U300_TIMER_APP_SOSTM					(0x000c)
#define U300_TIMER_APP_SOSTM_MODE_CONTINUOUS			(0x00000000)
#define U300_TIMER_APP_SOSTM_MODE_ONE_SHOT			(0x00000001)
/* OS Timer Status Register 32bit (R/-) */
#define U300_TIMER_APP_OSTS					(0x0010)
#define U300_TIMER_APP_OSTS_TIMER_STATE_MASK			(0x0000000F)
#define U300_TIMER_APP_OSTS_TIMER_STATE_IDLE			(0x00000001)
#define U300_TIMER_APP_OSTS_TIMER_STATE_ACTIVE			(0x00000002)
#define U300_TIMER_APP_OSTS_ENABLE_IND				(0x00000010)
#define U300_TIMER_APP_OSTS_MODE_MASK				(0x00000020)
#define U300_TIMER_APP_OSTS_MODE_CONTINUOUS			(0x00000000)
#define U300_TIMER_APP_OSTS_MODE_ONE_SHOT			(0x00000020)
#define U300_TIMER_APP_OSTS_IRQ_ENABLED_IND			(0x00000040)
#define U300_TIMER_APP_OSTS_IRQ_PENDING_IND			(0x00000080)
/* OS Timer Current Count Register 32bit (R/-) */
#define U300_TIMER_APP_OSTCC					(0x0014)
/* OS Timer Terminal Count Register 32bit (R/W) */
#define U300_TIMER_APP_OSTTC					(0x0018)
/* OS Timer Interrupt Enable Register 32bit (-/W) */
#define U300_TIMER_APP_OSTIE					(0x001c)
#define U300_TIMER_APP_OSTIE_IRQ_DISABLE			(0x00000000)
#define U300_TIMER_APP_OSTIE_IRQ_ENABLE				(0x00000001)
/* OS Timer Interrupt Acknowledge Register 32bit (-/W) */
#define U300_TIMER_APP_OSTIA					(0x0020)
#define U300_TIMER_APP_OSTIA_IRQ_ACK				(0x00000080)

/* Reset DD Timer 32bit (-/W) */
#define U300_TIMER_APP_RDDT					(0x0040)
#define U300_TIMER_APP_RDDT_TIMER_RESET				(0x00000000)
/* Enable DD Timer 32bit (-/W) */
#define U300_TIMER_APP_EDDT					(0x0044)
#define U300_TIMER_APP_EDDT_TIMER_ENABLE			(0x00000000)
/* Disable DD Timer 32bit (-/W) */
#define U300_TIMER_APP_DDDT					(0x0048)
#define U300_TIMER_APP_DDDT_TIMER_DISABLE			(0x00000000)
/* DD Timer Mode Register 32bit (-/W) */
#define U300_TIMER_APP_SDDTM					(0x004c)
#define U300_TIMER_APP_SDDTM_MODE_CONTINUOUS			(0x00000000)
#define U300_TIMER_APP_SDDTM_MODE_ONE_SHOT			(0x00000001)
/* DD Timer Status Register 32bit (R/-) */
#define U300_TIMER_APP_DDTS					(0x0050)
#define U300_TIMER_APP_DDTS_TIMER_STATE_MASK			(0x0000000F)
#define U300_TIMER_APP_DDTS_TIMER_STATE_IDLE			(0x00000001)
#define U300_TIMER_APP_DDTS_TIMER_STATE_ACTIVE			(0x00000002)
#define U300_TIMER_APP_DDTS_ENABLE_IND				(0x00000010)
#define U300_TIMER_APP_DDTS_MODE_MASK				(0x00000020)
#define U300_TIMER_APP_DDTS_MODE_CONTINUOUS			(0x00000000)
#define U300_TIMER_APP_DDTS_MODE_ONE_SHOT			(0x00000020)
#define U300_TIMER_APP_DDTS_IRQ_ENABLED_IND			(0x00000040)
#define U300_TIMER_APP_DDTS_IRQ_PENDING_IND			(0x00000080)
/* DD Timer Current Count Register 32bit (R/-) */
#define U300_TIMER_APP_DDTCC					(0x0054)
/* DD Timer Terminal Count Register 32bit (R/W) */
#define U300_TIMER_APP_DDTTC					(0x0058)
/* DD Timer Interrupt Enable Register 32bit (-/W) */
#define U300_TIMER_APP_DDTIE					(0x005c)
#define U300_TIMER_APP_DDTIE_IRQ_DISABLE			(0x00000000)
#define U300_TIMER_APP_DDTIE_IRQ_ENABLE				(0x00000001)
/* DD Timer Interrupt Acknowledge Register 32bit (-/W) */
#define U300_TIMER_APP_DDTIA					(0x0060)
#define U300_TIMER_APP_DDTIA_IRQ_ACK				(0x00000080)

/* Reset GP1 Timer 32bit (-/W) */
#define U300_TIMER_APP_RGPT1					(0x0080)
#define U300_TIMER_APP_RGPT1_TIMER_RESET			(0x00000000)
/* Enable GP1 Timer 32bit (-/W) */
#define U300_TIMER_APP_EGPT1					(0x0084)
#define U300_TIMER_APP_EGPT1_TIMER_ENABLE			(0x00000000)
/* Disable GP1 Timer 32bit (-/W) */
#define U300_TIMER_APP_DGPT1					(0x0088)
#define U300_TIMER_APP_DGPT1_TIMER_DISABLE			(0x00000000)
/* GP1 Timer Mode Register 32bit (-/W) */
#define U300_TIMER_APP_SGPT1M					(0x008c)
#define U300_TIMER_APP_SGPT1M_MODE_CONTINUOUS			(0x00000000)
#define U300_TIMER_APP_SGPT1M_MODE_ONE_SHOT			(0x00000001)
/* GP1 Timer Status Register 32bit (R/-) */
#define U300_TIMER_APP_GPT1S					(0x0090)
#define U300_TIMER_APP_GPT1S_TIMER_STATE_MASK			(0x0000000F)
#define U300_TIMER_APP_GPT1S_TIMER_STATE_IDLE			(0x00000001)
#define U300_TIMER_APP_GPT1S_TIMER_STATE_ACTIVE			(0x00000002)
#define U300_TIMER_APP_GPT1S_ENABLE_IND				(0x00000010)
#define U300_TIMER_APP_GPT1S_MODE_MASK				(0x00000020)
#define U300_TIMER_APP_GPT1S_MODE_CONTINUOUS			(0x00000000)
#define U300_TIMER_APP_GPT1S_MODE_ONE_SHOT			(0x00000020)
#define U300_TIMER_APP_GPT1S_IRQ_ENABLED_IND			(0x00000040)
#define U300_TIMER_APP_GPT1S_IRQ_PENDING_IND			(0x00000080)
/* GP1 Timer Current Count Register 32bit (R/-) */
#define U300_TIMER_APP_GPT1CC					(0x0094)
/* GP1 Timer Terminal Count Register 32bit (R/W) */
#define U300_TIMER_APP_GPT1TC					(0x0098)
/* GP1 Timer Interrupt Enable Register 32bit (-/W) */
#define U300_TIMER_APP_GPT1IE					(0x009c)
#define U300_TIMER_APP_GPT1IE_IRQ_DISABLE			(0x00000000)
#define U300_TIMER_APP_GPT1IE_IRQ_ENABLE			(0x00000001)
/* GP1 Timer Interrupt Acknowledge Register 32bit (-/W) */
#define U300_TIMER_APP_GPT1IA					(0x00a0)
#define U300_TIMER_APP_GPT1IA_IRQ_ACK				(0x00000080)

/* Reset GP2 Timer 32bit (-/W) */
#define U300_TIMER_APP_RGPT2					(0x00c0)
#define U300_TIMER_APP_RGPT2_TIMER_RESET			(0x00000000)
/* Enable GP2 Timer 32bit (-/W) */
#define U300_TIMER_APP_EGPT2					(0x00c4)
#define U300_TIMER_APP_EGPT2_TIMER_ENABLE			(0x00000000)
/* Disable GP2 Timer 32bit (-/W) */
#define U300_TIMER_APP_DGPT2					(0x00c8)
#define U300_TIMER_APP_DGPT2_TIMER_DISABLE			(0x00000000)
/* GP2 Timer Mode Register 32bit (-/W) */
#define U300_TIMER_APP_SGPT2M					(0x00cc)
#define U300_TIMER_APP_SGPT2M_MODE_CONTINUOUS			(0x00000000)
#define U300_TIMER_APP_SGPT2M_MODE_ONE_SHOT			(0x00000001)
/* GP2 Timer Status Register 32bit (R/-) */
#define U300_TIMER_APP_GPT2S					(0x00d0)
#define U300_TIMER_APP_GPT2S_TIMER_STATE_MASK			(0x0000000F)
#define U300_TIMER_APP_GPT2S_TIMER_STATE_IDLE			(0x00000001)
#define U300_TIMER_APP_GPT2S_TIMER_STATE_ACTIVE			(0x00000002)
#define U300_TIMER_APP_GPT2S_ENABLE_IND				(0x00000010)
#define U300_TIMER_APP_GPT2S_MODE_MASK				(0x00000020)
#define U300_TIMER_APP_GPT2S_MODE_CONTINUOUS			(0x00000000)
#define U300_TIMER_APP_GPT2S_MODE_ONE_SHOT			(0x00000020)
#define U300_TIMER_APP_GPT2S_IRQ_ENABLED_IND			(0x00000040)
#define U300_TIMER_APP_GPT2S_IRQ_PENDING_IND			(0x00000080)
/* GP2 Timer Current Count Register 32bit (R/-) */
#define U300_TIMER_APP_GPT2CC					(0x00d4)
/* GP2 Timer Terminal Count Register 32bit (R/W) */
#define U300_TIMER_APP_GPT2TC					(0x00d8)
/* GP2 Timer Interrupt Enable Register 32bit (-/W) */
#define U300_TIMER_APP_GPT2IE					(0x00dc)
#define U300_TIMER_APP_GPT2IE_IRQ_DISABLE			(0x00000000)
#define U300_TIMER_APP_GPT2IE_IRQ_ENABLE			(0x00000001)
/* GP2 Timer Interrupt Acknowledge Register 32bit (-/W) */
#define U300_TIMER_APP_GPT2IA					(0x00e0)
#define U300_TIMER_APP_GPT2IA_IRQ_ACK				(0x00000080)

/* Clock request control register - all four timers */
#define U300_TIMER_APP_CRC					(0x100)
#define U300_TIMER_APP_CRC_CLOCK_REQUEST_ENABLE			(0x00000001)

#define TICKS_PER_JIFFY ((CLOCK_TICK_RATE + (HZ/2)) / HZ)
#define US_PER_TICK ((1000000 + (HZ/2)) / HZ)

/*
 * The u300_set_mode() function is always called first, if we
 * have oneshot timer active, the oneshot scheduling function
 * u300_set_next_event() is called immediately after.
 */
static void u300_set_mode(enum clock_event_mode mode,
			  struct clock_event_device *evt)
{
	switch (mode) {
	case CLOCK_EVT_MODE_PERIODIC:
		/* Disable interrupts on GPT1 */
		writel(U300_TIMER_APP_GPT1IE_IRQ_DISABLE,
		       U300_TIMER_APP_VBASE + U300_TIMER_APP_GPT1IE);
		/* Disable GP1 while we're reprogramming it. */
		writel(U300_TIMER_APP_DGPT1_TIMER_DISABLE,
		       U300_TIMER_APP_VBASE + U300_TIMER_APP_DGPT1);
		/*
		 * Set the periodic mode to a certain number of ticks per
		 * jiffy.
		 */
		writel(TICKS_PER_JIFFY,
		       U300_TIMER_APP_VBASE + U300_TIMER_APP_GPT1TC);
		/*
		 * Set continuous mode, so the timer keeps triggering
		 * interrupts.
		 */
		writel(U300_TIMER_APP_SGPT1M_MODE_CONTINUOUS,
		       U300_TIMER_APP_VBASE + U300_TIMER_APP_SGPT1M);
		/* Enable timer interrupts */
		writel(U300_TIMER_APP_GPT1IE_IRQ_ENABLE,
		       U300_TIMER_APP_VBASE + U300_TIMER_APP_GPT1IE);
		/* Then enable the OS timer again */
		writel(U300_TIMER_APP_EGPT1_TIMER_ENABLE,
		       U300_TIMER_APP_VBASE + U300_TIMER_APP_EGPT1);
		break;
	case CLOCK_EVT_MODE_ONESHOT:
		/* Just break; here? */
		/*
		 * The actual event will be programmed by the next event hook,
		 * so we just set a dummy value somewhere at the end of the
		 * universe here.
		 */
		/* Disable interrupts on GPT1 */
		writel(U300_TIMER_APP_GPT1IE_IRQ_DISABLE,
		       U300_TIMER_APP_VBASE + U300_TIMER_APP_GPT1IE);
		/* Disable GP1 while we're reprogramming it. */
		writel(U300_TIMER_APP_DGPT1_TIMER_DISABLE,
		       U300_TIMER_APP_VBASE + U300_TIMER_APP_DGPT1);
		/*
		 * Expire far in the future, u300_set_next_event() will be
		 * called soon...
		 */
		writel(0xFFFFFFFF, U300_TIMER_APP_VBASE + U300_TIMER_APP_GPT1TC);
		/* We run one shot per tick here! */
		writel(U300_TIMER_APP_SGPT1M_MODE_ONE_SHOT,
		       U300_TIMER_APP_VBASE + U300_TIMER_APP_SGPT1M);
		/* Enable interrupts for this timer */
		writel(U300_TIMER_APP_GPT1IE_IRQ_ENABLE,
		       U300_TIMER_APP_VBASE + U300_TIMER_APP_GPT1IE);
		/* Enable timer */
		writel(U300_TIMER_APP_EGPT1_TIMER_ENABLE,
		       U300_TIMER_APP_VBASE + U300_TIMER_APP_EGPT1);
		break;
	case CLOCK_EVT_MODE_UNUSED:
	case CLOCK_EVT_MODE_SHUTDOWN:
		/* Disable interrupts on GP1 */
		writel(U300_TIMER_APP_GPT1IE_IRQ_DISABLE,
		       U300_TIMER_APP_VBASE + U300_TIMER_APP_GPT1IE);
		/* Disable GP1 */
		writel(U300_TIMER_APP_DGPT1_TIMER_DISABLE,
		       U300_TIMER_APP_VBASE + U300_TIMER_APP_DGPT1);
		break;
	case CLOCK_EVT_MODE_RESUME:
		/* Ignore this call */
		break;
	}
}

/*
 * The app timer in one shot mode obviously has to be reprogrammed
 * in EXACTLY this sequence to work properly. Do NOT try to e.g. replace
 * the interrupt disable + timer disable commands with a reset command,
 * it will fail miserably. Apparently (and I found this the hard way)
 * the timer is very sensitive to the instruction order, though you don't
 * get that impression from the data sheet.
 */
static int u300_set_next_event(unsigned long cycles,
			       struct clock_event_device *evt)

{
	/* Disable interrupts on GPT1 */
	writel(U300_TIMER_APP_GPT1IE_IRQ_DISABLE,
	       U300_TIMER_APP_VBASE + U300_TIMER_APP_GPT1IE);
	/* Disable GP1 while we're reprogramming it. */
	writel(U300_TIMER_APP_DGPT1_TIMER_DISABLE,
	       U300_TIMER_APP_VBASE + U300_TIMER_APP_DGPT1);
	/* Reset the General Purpose timer 1. */
	writel(U300_TIMER_APP_RGPT1_TIMER_RESET,
	       U300_TIMER_APP_VBASE + U300_TIMER_APP_RGPT1);
	/* IRQ in n * cycles */
	writel(cycles, U300_TIMER_APP_VBASE + U300_TIMER_APP_GPT1TC);
	/*
	 * We run one shot per tick here! (This is necessary to reconfigure,
	 * the timer will tilt if you don't!)
	 */
	writel(U300_TIMER_APP_SGPT1M_MODE_ONE_SHOT,
	       U300_TIMER_APP_VBASE + U300_TIMER_APP_SGPT1M);
	/* Enable timer interrupts */
	writel(U300_TIMER_APP_GPT1IE_IRQ_ENABLE,
	       U300_TIMER_APP_VBASE + U300_TIMER_APP_GPT1IE);
	/* Then enable the OS timer again */
	writel(U300_TIMER_APP_EGPT1_TIMER_ENABLE,
	       U300_TIMER_APP_VBASE + U300_TIMER_APP_EGPT1);
	return 0;
}


/* Use general purpose timer 1 as clock event */
static struct clock_event_device clockevent_u300_1mhz = {
	.name           = "GPT1",
	.rating         = 300, /* Reasonably fast and accurate clock event */
	.features       = CLOCK_EVT_FEAT_PERIODIC | CLOCK_EVT_FEAT_ONESHOT,
	.set_next_event = u300_set_next_event,
	.set_mode       = u300_set_mode,
};

/* Clock event timer interrupt handler */
static irqreturn_t u300_timer_interrupt(int irq, void *dev_id)
{
	struct clock_event_device *evt = &clockevent_u300_1mhz;
	/* ACK/Clear timer IRQ for the APP GPT1 Timer */
	writel(U300_TIMER_APP_GPT1IA_IRQ_ACK,
		U300_TIMER_APP_VBASE + U300_TIMER_APP_GPT1IA);
	evt->event_handler(evt);
	return IRQ_HANDLED;
}

static struct irqaction u300_timer_irq = {
	.name           = "U300 Timer Tick",
	.flags          = IRQF_DISABLED | IRQF_TIMER | IRQF_IRQPOLL,
	.handler        = u300_timer_interrupt,
};

/* Use general purpose timer 2 as clock source */
static cycle_t u300_get_cycles(struct clocksource *cs)
{
	return (cycles_t) readl(U300_TIMER_APP_VBASE + U300_TIMER_APP_GPT2CC);
}

static struct clocksource clocksource_u300_1mhz = {
	.name           = "GPT2",
	.rating         = 300, /* Reasonably fast and accurate clock source */
	.read           = u300_get_cycles,
	.mask           = CLOCKSOURCE_MASK(32), /* 32 bits */
	.flags          = CLOCK_SOURCE_IS_CONTINUOUS,
};

/*
 * Override the global weak sched_clock symbol with this
 * local implementation which uses the clocksource to get some
 * better resolution when scheduling the kernel. We accept that
 * this wraps around for now, since it is just a relative time
 * stamp. (Inspired by OMAP implementation.)
 */
static DEFINE_CLOCK_DATA(cd);

unsigned long long notrace sched_clock(void)
{
	u32 cyc = readl(U300_TIMER_APP_VBASE + U300_TIMER_APP_GPT2CC);
	return cyc_to_sched_clock(&cd, cyc, (u32)~0);
}

static void notrace u300_update_sched_clock(void)
{
	u32 cyc = readl(U300_TIMER_APP_VBASE + U300_TIMER_APP_GPT2CC);
	update_sched_clock(&cd, cyc, (u32)~0);
}


/*
 * This sets up the system timers, clock source and clock event.
 */
static void __init u300_timer_init(void)
{
	struct clk *clk;
	unsigned long rate;

	/* Clock the interrupt controller */
	clk = clk_get_sys("apptimer", NULL);
	BUG_ON(IS_ERR(clk));
	clk_enable(clk);
	rate = clk_get_rate(clk);

	init_sched_clock(&cd, u300_update_sched_clock, 32, rate);

	/*
	 * Disable the "OS" and "DD" timers - these are designed for Symbian!
	 * Example usage in cnh1601578 cpu subsystem pd_timer_app.c
	 */
	writel(U300_TIMER_APP_CRC_CLOCK_REQUEST_ENABLE,
		U300_TIMER_APP_VBASE + U300_TIMER_APP_CRC);
	writel(U300_TIMER_APP_ROST_TIMER_RESET,
		U300_TIMER_APP_VBASE + U300_TIMER_APP_ROST);
	writel(U300_TIMER_APP_DOST_TIMER_DISABLE,
		U300_TIMER_APP_VBASE + U300_TIMER_APP_DOST);
	writel(U300_TIMER_APP_RDDT_TIMER_RESET,
		U300_TIMER_APP_VBASE + U300_TIMER_APP_RDDT);
	writel(U300_TIMER_APP_DDDT_TIMER_DISABLE,
		U300_TIMER_APP_VBASE + U300_TIMER_APP_DDDT);

	/* Reset the General Purpose timer 1. */
	writel(U300_TIMER_APP_RGPT1_TIMER_RESET,
		U300_TIMER_APP_VBASE + U300_TIMER_APP_RGPT1);

	/* Set up the IRQ handler */
	setup_irq(IRQ_U300_TIMER_APP_GP1, &u300_timer_irq);

	/* Reset the General Purpose timer 2 */
	writel(U300_TIMER_APP_RGPT2_TIMER_RESET,
		U300_TIMER_APP_VBASE + U300_TIMER_APP_RGPT2);
	/* Set this timer to run around forever */
	writel(0xFFFFFFFFU, U300_TIMER_APP_VBASE + U300_TIMER_APP_GPT2TC);
	/* Set continuous mode so it wraps around */
	writel(U300_TIMER_APP_SGPT2M_MODE_CONTINUOUS,
	       U300_TIMER_APP_VBASE + U300_TIMER_APP_SGPT2M);
	/* Disable timer interrupts */
	writel(U300_TIMER_APP_GPT2IE_IRQ_DISABLE,
		U300_TIMER_APP_VBASE + U300_TIMER_APP_GPT2IE);
	/* Then enable the GP2 timer to use as a free running us counter */
	writel(U300_TIMER_APP_EGPT2_TIMER_ENABLE,
		U300_TIMER_APP_VBASE + U300_TIMER_APP_EGPT2);

	if (clocksource_register_hz(&clocksource_u300_1mhz, rate))
		printk(KERN_ERR "timer: failed to initialize clock "
		       "source %s\n", clocksource_u300_1mhz.name);

	clockevents_calc_mult_shift(&clockevent_u300_1mhz,
				    rate, APPTIMER_MIN_RANGE);
	/* 32bit counter, so 32bits delta is max */
	clockevent_u300_1mhz.max_delta_ns =
		clockevent_delta2ns(0xffffffff, &clockevent_u300_1mhz);
	/* This timer is slow enough to set for 1 cycle == 1 MHz */
	clockevent_u300_1mhz.min_delta_ns =
		clockevent_delta2ns(1, &clockevent_u300_1mhz);
	clockevent_u300_1mhz.cpumask = cpumask_of(0);
	clockevents_register_device(&clockevent_u300_1mhz);
	/*
	 * TODO: init and register the rest of the timers too, they can be
	 * used by hrtimers!
	 */
}

/*
 * Very simple system timer that only register the clock event and
 * clock source.
 */
struct sys_timer u300_timer = {
	.init		= u300_timer_init,
};
