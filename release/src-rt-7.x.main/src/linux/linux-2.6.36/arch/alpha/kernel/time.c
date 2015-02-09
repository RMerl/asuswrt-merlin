/*
 *  linux/arch/alpha/kernel/time.c
 *
 *  Copyright (C) 1991, 1992, 1995, 1999, 2000  Linus Torvalds
 *
 * This file contains the PC-specific time handling details:
 * reading the RTC at bootup, etc..
 * 1994-07-02    Alan Modra
 *	fixed set_rtc_mmss, fixed time.year for >= 2000, new mktime
 * 1995-03-26    Markus Kuhn
 *      fixed 500 ms bug at call to set_rtc_mmss, fixed DS12887
 *      precision CMOS clock update
 * 1997-09-10	Updated NTP code according to technical memorandum Jan '96
 *		"A Kernel Model for Precision Timekeeping" by Dave Mills
 * 1997-01-09    Adrian Sun
 *      use interval timer if CONFIG_RTC=y
 * 1997-10-29    John Bowman (bowman@math.ualberta.ca)
 *      fixed tick loss calculation in timer_interrupt
 *      (round system clock to nearest tick instead of truncating)
 *      fixed algorithm in time_init for getting time from CMOS clock
 * 1999-04-16	Thorsten Kranzkowski (dl8bcu@gmx.net)
 *	fixed algorithm in do_gettimeofday() for calculating the precise time
 *	from processor cycle counter (now taking lost_ticks into account)
 * 2000-08-13	Jan-Benedict Glaw <jbglaw@lug-owl.de>
 * 	Fixed time_init to be aware of epoches != 1900. This prevents
 * 	booting up in 2048 for me;) Code is stolen from rtc.c.
 * 2003-06-03	R. Scott Bailey <scott.bailey@eds.com>
 *	Tighten sanity in time_init from 1% (10,000 PPM) to 250 PPM
 */
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/param.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/bcd.h>
#include <linux/profile.h>
#include <linux/perf_event.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/hwrpb.h>
#include <asm/8253pit.h>
#include <asm/rtc.h>

#include <linux/mc146818rtc.h>
#include <linux/time.h>
#include <linux/timex.h>
#include <linux/clocksource.h>

#include "proto.h"
#include "irq_impl.h"

static int set_rtc_mmss(unsigned long);

DEFINE_SPINLOCK(rtc_lock);
EXPORT_SYMBOL(rtc_lock);

#define TICK_SIZE (tick_nsec / 1000)

/*
 * Shift amount by which scaled_ticks_per_cycle is scaled.  Shifting
 * by 48 gives us 16 bits for HZ while keeping the accuracy good even
 * for large CPU clock rates.
 */
#define FIX_SHIFT	48

/* lump static variables together for more efficient access: */
static struct {
	/* cycle counter last time it got invoked */
	__u32 last_time;
	/* ticks/cycle * 2^48 */
	unsigned long scaled_ticks_per_cycle;
	/* partial unused tick */
	unsigned long partial_tick;
} state;

unsigned long est_cycle_freq;

#ifdef CONFIG_PERF_EVENTS

DEFINE_PER_CPU(u8, perf_event_pending);

#define set_perf_event_pending_flag()  __get_cpu_var(perf_event_pending) = 1
#define test_perf_event_pending()      __get_cpu_var(perf_event_pending)
#define clear_perf_event_pending()     __get_cpu_var(perf_event_pending) = 0

void set_perf_event_pending(void)
{
	set_perf_event_pending_flag();
}

#else  /* CONFIG_PERF_EVENTS */

#define test_perf_event_pending()      0
#define clear_perf_event_pending()

#endif /* CONFIG_PERF_EVENTS */


static inline __u32 rpcc(void)
{
    __u32 result;
    asm volatile ("rpcc %0" : "=r"(result));
    return result;
}

int update_persistent_clock(struct timespec now)
{
	return set_rtc_mmss(now.tv_sec);
}

void read_persistent_clock(struct timespec *ts)
{
	unsigned int year, mon, day, hour, min, sec, epoch;

	sec = CMOS_READ(RTC_SECONDS);
	min = CMOS_READ(RTC_MINUTES);
	hour = CMOS_READ(RTC_HOURS);
	day = CMOS_READ(RTC_DAY_OF_MONTH);
	mon = CMOS_READ(RTC_MONTH);
	year = CMOS_READ(RTC_YEAR);

	if (!(CMOS_READ(RTC_CONTROL) & RTC_DM_BINARY) || RTC_ALWAYS_BCD) {
		sec = bcd2bin(sec);
		min = bcd2bin(min);
		hour = bcd2bin(hour);
		day = bcd2bin(day);
		mon = bcd2bin(mon);
		year = bcd2bin(year);
	}

	/* PC-like is standard; used for year >= 70 */
	epoch = 1900;
	if (year < 20)
		epoch = 2000;
	else if (year >= 20 && year < 48)
		/* NT epoch */
		epoch = 1980;
	else if (year >= 48 && year < 70)
		/* Digital UNIX epoch */
		epoch = 1952;

	printk(KERN_INFO "Using epoch = %d\n", epoch);

	if ((year += epoch) < 1970)
		year += 100;

	ts->tv_sec = mktime(year, mon, day, hour, min, sec);
}



/*
 * timer_interrupt() needs to keep up the real-time clock,
 * as well as call the "do_timer()" routine every clocktick
 */
irqreturn_t timer_interrupt(int irq, void *dev)
{
	unsigned long delta;
	__u32 now;
	long nticks;

#ifndef CONFIG_SMP
	/* Not SMP, do kernel PC profiling here.  */
	profile_tick(CPU_PROFILING);
#endif

	write_seqlock(&xtime_lock);

	/*
	 * Calculate how many ticks have passed since the last update,
	 * including any previous partial leftover.  Save any resulting
	 * fraction for the next pass.
	 */
	now = rpcc();
	delta = now - state.last_time;
	state.last_time = now;
	delta = delta * state.scaled_ticks_per_cycle + state.partial_tick;
	state.partial_tick = delta & ((1UL << FIX_SHIFT) - 1); 
	nticks = delta >> FIX_SHIFT;

	if (nticks)
		do_timer(nticks);

	write_sequnlock(&xtime_lock);

	if (test_perf_event_pending()) {
		clear_perf_event_pending();
		perf_event_do_pending();
	}

#ifndef CONFIG_SMP
	while (nticks--)
		update_process_times(user_mode(get_irq_regs()));
#endif

	return IRQ_HANDLED;
}

void __init
common_init_rtc(void)
{
	unsigned char x;

	/* Reset periodic interrupt frequency.  */
	x = CMOS_READ(RTC_FREQ_SELECT) & 0x3f;
        /* Test includes known working values on various platforms
           where 0x26 is wrong; we refuse to change those. */
	if (x != 0x26 && x != 0x25 && x != 0x19 && x != 0x06) {
		printk("Setting RTC_FREQ to 1024 Hz (%x)\n", x);
		CMOS_WRITE(0x26, RTC_FREQ_SELECT);
	}

	/* Turn on periodic interrupts.  */
	x = CMOS_READ(RTC_CONTROL);
	if (!(x & RTC_PIE)) {
		printk("Turning on RTC interrupts.\n");
		x |= RTC_PIE;
		x &= ~(RTC_AIE | RTC_UIE);
		CMOS_WRITE(x, RTC_CONTROL);
	}
	(void) CMOS_READ(RTC_INTR_FLAGS);

	outb(0x36, 0x43);	/* pit counter 0: system timer */
	outb(0x00, 0x40);
	outb(0x00, 0x40);

	outb(0xb6, 0x43);	/* pit counter 2: speaker */
	outb(0x31, 0x42);
	outb(0x13, 0x42);

	init_rtc_irq();
}

unsigned int common_get_rtc_time(struct rtc_time *time)
{
	return __get_rtc_time(time);
}

int common_set_rtc_time(struct rtc_time *time)
{
	return __set_rtc_time(time);
}

/* Validate a computed cycle counter result against the known bounds for
   the given processor core.  There's too much brokenness in the way of
   timing hardware for any one method to work everywhere.  :-(

   Return 0 if the result cannot be trusted, otherwise return the argument.  */

static unsigned long __init
validate_cc_value(unsigned long cc)
{
	static struct bounds {
		unsigned int min, max;
	} cpu_hz[] __initdata = {
		[EV3_CPU]    = {   50000000,  200000000 },	/* guess */
		[EV4_CPU]    = {  100000000,  300000000 },
		[LCA4_CPU]   = {  100000000,  300000000 },	/* guess */
		[EV45_CPU]   = {  200000000,  300000000 },
		[EV5_CPU]    = {  250000000,  433000000 },
		[EV56_CPU]   = {  333000000,  667000000 },
		[PCA56_CPU]  = {  400000000,  600000000 },	/* guess */
		[PCA57_CPU]  = {  500000000,  600000000 },	/* guess */
		[EV6_CPU]    = {  466000000,  600000000 },
		[EV67_CPU]   = {  600000000,  750000000 },
		[EV68AL_CPU] = {  750000000,  940000000 },
		[EV68CB_CPU] = { 1000000000, 1333333333 },
		/* None of the following are shipping as of 2001-11-01.  */
		[EV68CX_CPU] = { 1000000000, 1700000000 },	/* guess */
		[EV69_CPU]   = { 1000000000, 1700000000 },	/* guess */
		[EV7_CPU]    = {  800000000, 1400000000 },	/* guess */
		[EV79_CPU]   = { 1000000000, 2000000000 },	/* guess */
	};

	/* Allow for some drift in the crystal.  10MHz is more than enough.  */
	const unsigned int deviation = 10000000;

	struct percpu_struct *cpu;
	unsigned int index;

	cpu = (struct percpu_struct *)((char*)hwrpb + hwrpb->processor_offset);
	index = cpu->type & 0xffffffff;

	/* If index out of bounds, no way to validate.  */
	if (index >= ARRAY_SIZE(cpu_hz))
		return cc;

	/* If index contains no data, no way to validate.  */
	if (cpu_hz[index].max == 0)
		return cc;

	if (cc < cpu_hz[index].min - deviation
	    || cc > cpu_hz[index].max + deviation)
		return 0;

	return cc;
}


/*
 * Calibrate CPU clock using legacy 8254 timer/counter. Stolen from
 * arch/i386/time.c.
 */

#define CALIBRATE_LATCH	0xffff
#define TIMEOUT_COUNT	0x100000

static unsigned long __init
calibrate_cc_with_pit(void)
{
	int cc, count = 0;

	/* Set the Gate high, disable speaker */
	outb((inb(0x61) & ~0x02) | 0x01, 0x61);

	/*
	 * Now let's take care of CTC channel 2
	 *
	 * Set the Gate high, program CTC channel 2 for mode 0,
	 * (interrupt on terminal count mode), binary count,
	 * load 5 * LATCH count, (LSB and MSB) to begin countdown.
	 */
	outb(0xb0, 0x43);		/* binary, mode 0, LSB/MSB, Ch 2 */
	outb(CALIBRATE_LATCH & 0xff, 0x42);	/* LSB of count */
	outb(CALIBRATE_LATCH >> 8, 0x42);	/* MSB of count */

	cc = rpcc();
	do {
		count++;
	} while ((inb(0x61) & 0x20) == 0 && count < TIMEOUT_COUNT);
	cc = rpcc() - cc;

	/* Error: ECTCNEVERSET or ECPUTOOFAST.  */
	if (count <= 1 || count == TIMEOUT_COUNT)
		return 0;

	return ((long)cc * PIT_TICK_RATE) / (CALIBRATE_LATCH + 1);
}

/* The Linux interpretation of the CMOS clock register contents:
   When the Update-In-Progress (UIP) flag goes from 1 to 0, the
   RTC registers show the second which has precisely just started.
   Let's hope other operating systems interpret the RTC the same way.  */

static unsigned long __init
rpcc_after_update_in_progress(void)
{
	do { } while (!(CMOS_READ(RTC_FREQ_SELECT) & RTC_UIP));
	do { } while (CMOS_READ(RTC_FREQ_SELECT) & RTC_UIP);

	return rpcc();
}

#ifndef CONFIG_SMP
/* Until and unless we figure out how to get cpu cycle counters
   in sync and keep them there, we can't use the rpcc.  */
static cycle_t read_rpcc(struct clocksource *cs)
{
	cycle_t ret = (cycle_t)rpcc();
	return ret;
}

static struct clocksource clocksource_rpcc = {
	.name                   = "rpcc",
	.rating                 = 300,
	.read                   = read_rpcc,
	.mask                   = CLOCKSOURCE_MASK(32),
	.flags                  = CLOCK_SOURCE_IS_CONTINUOUS
};

static inline void register_rpcc_clocksource(long cycle_freq)
{
	clocksource_calc_mult_shift(&clocksource_rpcc, cycle_freq, 4);
	clocksource_register(&clocksource_rpcc);
}
#else /* !CONFIG_SMP */
static inline void register_rpcc_clocksource(long cycle_freq)
{
}
#endif /* !CONFIG_SMP */

void __init
time_init(void)
{
	unsigned int cc1, cc2;
	unsigned long cycle_freq, tolerance;
	long diff;

	/* Calibrate CPU clock -- attempt #1.  */
	if (!est_cycle_freq)
		est_cycle_freq = validate_cc_value(calibrate_cc_with_pit());

	cc1 = rpcc();

	/* Calibrate CPU clock -- attempt #2.  */
	if (!est_cycle_freq) {
		cc1 = rpcc_after_update_in_progress();
		cc2 = rpcc_after_update_in_progress();
		est_cycle_freq = validate_cc_value(cc2 - cc1);
		cc1 = cc2;
	}

	cycle_freq = hwrpb->cycle_freq;
	if (est_cycle_freq) {
		/* If the given value is within 250 PPM of what we calculated,
		   accept it.  Otherwise, use what we found.  */
		tolerance = cycle_freq / 4000;
		diff = cycle_freq - est_cycle_freq;
		if (diff < 0)
			diff = -diff;
		if ((unsigned long)diff > tolerance) {
			cycle_freq = est_cycle_freq;
			printk("HWRPB cycle frequency bogus.  "
			       "Estimated %lu Hz\n", cycle_freq);
		} else {
			est_cycle_freq = 0;
		}
	} else if (! validate_cc_value (cycle_freq)) {
		printk("HWRPB cycle frequency bogus, "
		       "and unable to estimate a proper value!\n");
	}

	/* From John Bowman <bowman@math.ualberta.ca>: allow the values
	   to settle, as the Update-In-Progress bit going low isn't good
	   enough on some hardware.  2ms is our guess; we haven't found 
	   bogomips yet, but this is close on a 500Mhz box.  */
	__delay(1000000);


	if (HZ > (1<<16)) {
		extern void __you_loose (void);
		__you_loose();
	}

	register_rpcc_clocksource(cycle_freq);

	state.last_time = cc1;
	state.scaled_ticks_per_cycle
		= ((unsigned long) HZ << FIX_SHIFT) / cycle_freq;
	state.partial_tick = 0L;

	/* Startup the timer source. */
	alpha_mv.init_rtc();
}

/*
 * In order to set the CMOS clock precisely, set_rtc_mmss has to be
 * called 500 ms after the second nowtime has started, because when
 * nowtime is written into the registers of the CMOS clock, it will
 * jump to the next second precisely 500 ms later. Check the Motorola
 * MC146818A or Dallas DS12887 data sheet for details.
 *
 * BUG: This routine does not handle hour overflow properly; it just
 *      sets the minutes. Usually you won't notice until after reboot!
 */


static int
set_rtc_mmss(unsigned long nowtime)
{
	int retval = 0;
	int real_seconds, real_minutes, cmos_minutes;
	unsigned char save_control, save_freq_select;

	/* irq are locally disabled here */
	spin_lock(&rtc_lock);
	/* Tell the clock it's being set */
	save_control = CMOS_READ(RTC_CONTROL);
	CMOS_WRITE((save_control|RTC_SET), RTC_CONTROL);

	/* Stop and reset prescaler */
	save_freq_select = CMOS_READ(RTC_FREQ_SELECT);
	CMOS_WRITE((save_freq_select|RTC_DIV_RESET2), RTC_FREQ_SELECT);

	cmos_minutes = CMOS_READ(RTC_MINUTES);
	if (!(save_control & RTC_DM_BINARY) || RTC_ALWAYS_BCD)
		cmos_minutes = bcd2bin(cmos_minutes);

	/*
	 * since we're only adjusting minutes and seconds,
	 * don't interfere with hour overflow. This avoids
	 * messing with unknown time zones but requires your
	 * RTC not to be off by more than 15 minutes
	 */
	real_seconds = nowtime % 60;
	real_minutes = nowtime / 60;
	if (((abs(real_minutes - cmos_minutes) + 15)/30) & 1) {
		/* correct for half hour time zone */
		real_minutes += 30;
	}
	real_minutes %= 60;

	if (abs(real_minutes - cmos_minutes) < 30) {
		if (!(save_control & RTC_DM_BINARY) || RTC_ALWAYS_BCD) {
			real_seconds = bin2bcd(real_seconds);
			real_minutes = bin2bcd(real_minutes);
		}
		CMOS_WRITE(real_seconds,RTC_SECONDS);
		CMOS_WRITE(real_minutes,RTC_MINUTES);
	} else {
		printk(KERN_WARNING
		       "set_rtc_mmss: can't update from %d to %d\n",
		       cmos_minutes, real_minutes);
 		retval = -1;
	}

	/* The following flags have to be released exactly in this order,
	 * otherwise the DS12887 (popular MC146818A clone with integrated
	 * battery and quartz) will not reset the oscillator and will not
	 * update precisely 500 ms later. You won't find this mentioned in
	 * the Dallas Semiconductor data sheets, but who believes data
	 * sheets anyway ...                           -- Markus Kuhn
	 */
	CMOS_WRITE(save_control, RTC_CONTROL);
	CMOS_WRITE(save_freq_select, RTC_FREQ_SELECT);
	spin_unlock(&rtc_lock);

	return retval;
}
