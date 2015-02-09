/*
 * i8253.c  8253/PIT functions
 *
 */
#include <linux/clockchips.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/jiffies.h>
#include <linux/module.h>
#include <linux/smp.h>
#include <linux/spinlock.h>

#include <asm/delay.h>
#include <asm/i8253.h>
#include <asm/io.h>
#include <asm/time.h>

DEFINE_RAW_SPINLOCK(i8253_lock);
EXPORT_SYMBOL(i8253_lock);

/*
 * Initialize the PIT timer.
 *
 * This is also called after resume to bring the PIT into operation again.
 */
static void init_pit_timer(enum clock_event_mode mode,
			   struct clock_event_device *evt)
{
	raw_spin_lock(&i8253_lock);

	switch(mode) {
	case CLOCK_EVT_MODE_PERIODIC:
		/* binary, mode 2, LSB/MSB, ch 0 */
		outb_p(0x34, PIT_MODE);
		outb_p(LATCH & 0xff , PIT_CH0);	/* LSB */
		outb(LATCH >> 8 , PIT_CH0);	/* MSB */
		break;

	case CLOCK_EVT_MODE_SHUTDOWN:
	case CLOCK_EVT_MODE_UNUSED:
		if (evt->mode == CLOCK_EVT_MODE_PERIODIC ||
		    evt->mode == CLOCK_EVT_MODE_ONESHOT) {
			outb_p(0x30, PIT_MODE);
			outb_p(0, PIT_CH0);
			outb_p(0, PIT_CH0);
		}
		break;

	case CLOCK_EVT_MODE_ONESHOT:
		/* One shot setup */
		outb_p(0x38, PIT_MODE);
		break;

	case CLOCK_EVT_MODE_RESUME:
		/* Nothing to do here */
		break;
	}
	raw_spin_unlock(&i8253_lock);
}

/*
 * Program the next event in oneshot mode
 *
 * Delta is given in PIT ticks
 */
static int pit_next_event(unsigned long delta, struct clock_event_device *evt)
{
	raw_spin_lock(&i8253_lock);
	outb_p(delta & 0xff , PIT_CH0);	/* LSB */
	outb(delta >> 8 , PIT_CH0);	/* MSB */
	raw_spin_unlock(&i8253_lock);

	return 0;
}

/*
 * On UP the PIT can serve all of the possible timer functions. On SMP systems
 * it can be solely used for the global tick.
 *
 * The profiling and update capabilites are switched off once the local apic is
 * registered. This mechanism replaces the previous #ifdef LOCAL_APIC -
 * !using_apic_timer decisions in do_timer_interrupt_hook()
 */
static struct clock_event_device pit_clockevent = {
	.name		= "pit",
	.features	= CLOCK_EVT_FEAT_PERIODIC | CLOCK_EVT_FEAT_ONESHOT,
	.set_mode	= init_pit_timer,
	.set_next_event = pit_next_event,
	.irq		= 0,
};

static irqreturn_t timer_interrupt(int irq, void *dev_id)
{
	pit_clockevent.event_handler(&pit_clockevent);

	return IRQ_HANDLED;
}

static struct irqaction irq0  = {
	.handler = timer_interrupt,
	.flags = IRQF_DISABLED | IRQF_NOBALANCING | IRQF_TIMER,
	.name = "timer"
};

/*
 * Initialize the conversion factor and the min/max deltas of the clock event
 * structure and register the clock event source with the framework.
 */
void __init setup_pit_timer(void)
{
	struct clock_event_device *cd = &pit_clockevent;
	unsigned int cpu = smp_processor_id();

	/*
	 * Start pit with the boot cpu mask and make it global after the
	 * IO_APIC has been initialized.
	 */
	cd->cpumask = cpumask_of(cpu);
	clockevent_set_clock(cd, CLOCK_TICK_RATE);
	cd->max_delta_ns = clockevent_delta2ns(0x7FFF, cd);
	cd->min_delta_ns = clockevent_delta2ns(0xF, cd);
	clockevents_register_device(cd);

	setup_irq(0, &irq0);
}

/*
 * Since the PIT overflows every tick, its not very useful
 * to just read by itself. So use jiffies to emulate a free
 * running counter:
 */
static cycle_t pit_read(struct clocksource *cs)
{
	unsigned long flags;
	int count;
	u32 jifs;
	static int old_count;
	static u32 old_jifs;

	raw_spin_lock_irqsave(&i8253_lock, flags);
	/*
	 * Although our caller may have the read side of xtime_lock,
	 * this is now a seqlock, and we are cheating in this routine
	 * by having side effects on state that we cannot undo if
	 * there is a collision on the seqlock and our caller has to
	 * retry.  (Namely, old_jifs and old_count.)  So we must treat
	 * jiffies as volatile despite the lock.  We read jiffies
	 * before latching the timer count to guarantee that although
	 * the jiffies value might be older than the count (that is,
	 * the counter may underflow between the last point where
	 * jiffies was incremented and the point where we latch the
	 * count), it cannot be newer.
	 */
	jifs = jiffies;
	outb_p(0x00, PIT_MODE);	/* latch the count ASAP */
	count = inb_p(PIT_CH0);	/* read the latched count */
	count |= inb_p(PIT_CH0) << 8;

	/* VIA686a test code... reset the latch if count > max + 1 */
	if (count > LATCH) {
		outb_p(0x34, PIT_MODE);
		outb_p(LATCH & 0xff, PIT_CH0);
		outb(LATCH >> 8, PIT_CH0);
		count = LATCH - 1;
	}

	/*
	 * It's possible for count to appear to go the wrong way for a
	 * couple of reasons:
	 *
	 *  1. The timer counter underflows, but we haven't handled the
	 *     resulting interrupt and incremented jiffies yet.
	 *  2. Hardware problem with the timer, not giving us continuous time,
	 *     the counter does small "jumps" upwards on some Pentium systems,
	 *     (see c't 95/10 page 335 for Neptun bug.)
	 *
	 * Previous attempts to handle these cases intelligently were
	 * buggy, so we just do the simple thing now.
	 */
	if (count > old_count && jifs == old_jifs) {
		count = old_count;
	}
	old_count = count;
	old_jifs = jifs;

	raw_spin_unlock_irqrestore(&i8253_lock, flags);

	count = (LATCH - 1) - count;

	return (cycle_t)(jifs * LATCH) + count;
}

static struct clocksource clocksource_pit = {
	.name	= "pit",
	.rating = 110,
	.read	= pit_read,
	.mask	= CLOCKSOURCE_MASK(32),
	.mult	= 0,
	.shift	= 20,
};

static int __init init_pit_clocksource(void)
{
	if (num_possible_cpus() > 1) /* PIT does not scale! */
		return 0;

	clocksource_pit.mult = clocksource_hz2mult(CLOCK_TICK_RATE, 20);
	return clocksource_register(&clocksource_pit);
}
arch_initcall(init_pit_clocksource);
