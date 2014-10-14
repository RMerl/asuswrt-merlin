#include <linux/clocksource.h>
#include <linux/clockchips.h>
#include <linux/errno.h>
#include <linux/hpet.h>
#include <linux/init.h>
#include <linux/sysdev.h>
#include <linux/pm.h>

#include <asm/hpet.h>
#include <asm/io.h>

extern struct clock_event_device *global_clock_event;

#define HPET_MASK	CLOCKSOURCE_MASK(32)
#define HPET_SHIFT	22

/* FSEC = 10^-15 NSEC = 10^-9 */
#define FSEC_PER_NSEC	1000000

/*
 * HPET address is set in acpi/boot.c, when an ACPI entry exists
 */
unsigned long hpet_address;
static void __iomem * hpet_virt_address;

static inline unsigned long hpet_readl(unsigned long a)
{
	return readl(hpet_virt_address + a);
}

static inline void hpet_writel(unsigned long d, unsigned long a)
{
	writel(d, hpet_virt_address + a);
}

/*
 * HPET command line enable / disable
 */
static int boot_hpet_disable;

static int __init hpet_setup(char* str)
{
	if (str) {
		if (!strncmp("disable", str, 7))
			boot_hpet_disable = 1;
	}
	return 1;
}
__setup("hpet=", hpet_setup);

static inline int is_hpet_capable(void)
{
	return (!boot_hpet_disable && hpet_address);
}

/*
 * HPET timer interrupt enable / disable
 */
static int hpet_legacy_int_enabled;

/**
 * is_hpet_enabled - check whether the hpet timer interrupt is enabled
 */
int is_hpet_enabled(void)
{
	return is_hpet_capable() && hpet_legacy_int_enabled;
}

/*
 * When the hpet driver (/dev/hpet) is enabled, we need to reserve
 * timer 0 and timer 1 in case of RTC emulation.
 */
#ifdef CONFIG_HPET
static void hpet_reserve_platform_timers(unsigned long id)
{
	struct hpet __iomem *hpet = hpet_virt_address;
	struct hpet_timer __iomem *timer = &hpet->hpet_timers[2];
	unsigned int nrtimers, i;
	struct hpet_data hd;

	nrtimers = ((id & HPET_ID_NUMBER) >> HPET_ID_NUMBER_SHIFT) + 1;

	memset(&hd, 0, sizeof (hd));
	hd.hd_phys_address = hpet_address;
	hd.hd_address = hpet_virt_address;
	hd.hd_nirqs = nrtimers;
	hd.hd_flags = HPET_DATA_PLATFORM;
	hpet_reserve_timer(&hd, 0);

#ifdef CONFIG_HPET_EMULATE_RTC
	hpet_reserve_timer(&hd, 1);
#endif

	hd.hd_irq[0] = HPET_LEGACY_8254;
	hd.hd_irq[1] = HPET_LEGACY_RTC;

	for (i = 2; i < nrtimers; timer++, i++)
		hd.hd_irq[i] = (timer->hpet_config & Tn_INT_ROUTE_CNF_MASK) >>
			Tn_INT_ROUTE_CNF_SHIFT;

	hpet_alloc(&hd);

}
#else
static void hpet_reserve_platform_timers(unsigned long id) { }
#endif

/*
 * Common hpet info
 */
static unsigned long hpet_period;

static void hpet_set_mode(enum clock_event_mode mode,
			  struct clock_event_device *evt);
static int hpet_next_event(unsigned long delta,
			   struct clock_event_device *evt);

/*
 * The hpet clock event device
 */
static struct clock_event_device hpet_clockevent = {
	.name		= "hpet",
	.features	= CLOCK_EVT_FEAT_PERIODIC | CLOCK_EVT_FEAT_ONESHOT,
	.set_mode	= hpet_set_mode,
	.set_next_event = hpet_next_event,
	.shift		= 32,
	.irq		= 0,
};

static void hpet_start_counter(void)
{
	unsigned long cfg = hpet_readl(HPET_CFG);

	cfg &= ~HPET_CFG_ENABLE;
	hpet_writel(cfg, HPET_CFG);
	hpet_writel(0, HPET_COUNTER);
	hpet_writel(0, HPET_COUNTER + 4);
	cfg |= HPET_CFG_ENABLE;
	hpet_writel(cfg, HPET_CFG);
}

static void hpet_enable_int(void)
{
	unsigned long cfg = hpet_readl(HPET_CFG);

	cfg |= HPET_CFG_LEGACY;
	hpet_writel(cfg, HPET_CFG);
	hpet_legacy_int_enabled = 1;
}

static void hpet_set_mode(enum clock_event_mode mode,
			  struct clock_event_device *evt)
{
	unsigned long cfg, cmp, now;
	uint64_t delta;

	switch(mode) {
	case CLOCK_EVT_MODE_PERIODIC:
		delta = ((uint64_t)(NSEC_PER_SEC/HZ)) * hpet_clockevent.mult;
		delta >>= hpet_clockevent.shift;
		now = hpet_readl(HPET_COUNTER);
		cmp = now + (unsigned long) delta;
		cfg = hpet_readl(HPET_T0_CFG);
		cfg |= HPET_TN_ENABLE | HPET_TN_PERIODIC |
		       HPET_TN_SETVAL | HPET_TN_32BIT;
		hpet_writel(cfg, HPET_T0_CFG);
		/*
		 * The first write after writing TN_SETVAL to the
		 * config register sets the counter value, the second
		 * write sets the period.
		 */
		hpet_writel(cmp, HPET_T0_CMP);
		udelay(1);
		hpet_writel((unsigned long) delta, HPET_T0_CMP);
		break;

	case CLOCK_EVT_MODE_ONESHOT:
		cfg = hpet_readl(HPET_T0_CFG);
		cfg &= ~HPET_TN_PERIODIC;
		cfg |= HPET_TN_ENABLE | HPET_TN_32BIT;
		hpet_writel(cfg, HPET_T0_CFG);
		break;

	case CLOCK_EVT_MODE_UNUSED:
	case CLOCK_EVT_MODE_SHUTDOWN:
		cfg = hpet_readl(HPET_T0_CFG);
		cfg &= ~HPET_TN_ENABLE;
		hpet_writel(cfg, HPET_T0_CFG);
		break;
	}
}

static int hpet_next_event(unsigned long delta,
			   struct clock_event_device *evt)
{
	unsigned long cnt;

	cnt = hpet_readl(HPET_COUNTER);
	cnt += delta;
	hpet_writel(cnt, HPET_T0_CMP);

	return ((long)(hpet_readl(HPET_COUNTER) - cnt ) > 0) ? -ETIME : 0;
}

/*
 * Clock source related code
 */
static cycle_t read_hpet(void)
{
	return (cycle_t)hpet_readl(HPET_COUNTER);
}

static struct clocksource clocksource_hpet = {
	.name		= "hpet",
	.rating		= 250,
	.read		= read_hpet,
	.mask		= HPET_MASK,
	.shift		= HPET_SHIFT,
	.flags		= CLOCK_SOURCE_IS_CONTINUOUS,
};

/*
 * Try to setup the HPET timer
 */
int __init hpet_enable(void)
{
	unsigned long id;
	uint64_t hpet_freq;
	u64 tmp, start, now;
	cycle_t t1;

	if (!is_hpet_capable())
		return 0;

	hpet_virt_address = ioremap_nocache(hpet_address, HPET_MMAP_SIZE);

	/*
	 * Read the period and check for a sane value:
	 */
	hpet_period = hpet_readl(HPET_PERIOD);
	if (hpet_period < HPET_MIN_PERIOD || hpet_period > HPET_MAX_PERIOD)
		goto out_nohpet;

	/*
	 * The period is a femto seconds value. We need to calculate the
	 * scaled math multiplication factor for nanosecond to hpet tick
	 * conversion.
	 */
	hpet_freq = 1000000000000000ULL;
	do_div(hpet_freq, hpet_period);
	hpet_clockevent.mult = div_sc((unsigned long) hpet_freq,
				      NSEC_PER_SEC, 32);
	/* Calculate the min / max delta */
	hpet_clockevent.max_delta_ns = clockevent_delta2ns(0x7FFFFFFF,
							   &hpet_clockevent);
	hpet_clockevent.min_delta_ns = clockevent_delta2ns(0x30,
							   &hpet_clockevent);

	/*
	 * Read the HPET ID register to retrieve the IRQ routing
	 * information and the number of channels
	 */
	id = hpet_readl(HPET_ID);

#ifdef CONFIG_HPET_EMULATE_RTC
	/*
	 * The legacy routing mode needs at least two channels, tick timer
	 * and the rtc emulation channel.
	 */
	if (!(id & HPET_ID_NUMBER))
		goto out_nohpet;
#endif

	/* Start the counter */
	hpet_start_counter();

	/* Verify whether hpet counter works */
	t1 = read_hpet();
	rdtscll(start);

	/*
	 * We don't know the TSC frequency yet, but waiting for
	 * 200000 TSC cycles is safe:
	 * 4 GHz == 50us
	 * 1 GHz == 200us
	 */
	do {
		rep_nop();
		rdtscll(now);
	} while ((now - start) < 200000UL);

	if (t1 == read_hpet()) {
		printk(KERN_WARNING
		       "HPET counter not counting. HPET disabled\n");
		goto out_nohpet;
	}

	/* Initialize and register HPET clocksource
	 *
	 * hpet period is in femto seconds per cycle
	 * so we need to convert this to ns/cyc units
	 * aproximated by mult/2^shift
	 *
	 *  fsec/cyc * 1nsec/1000000fsec = nsec/cyc = mult/2^shift
	 *  fsec/cyc * 1ns/1000000fsec * 2^shift = mult
	 *  fsec/cyc * 2^shift * 1nsec/1000000fsec = mult
	 *  (fsec/cyc << shift)/1000000 = mult
	 *  (hpet_period << shift)/FSEC_PER_NSEC = mult
	 */
	tmp = (u64)hpet_period << HPET_SHIFT;
	do_div(tmp, FSEC_PER_NSEC);
	clocksource_hpet.mult = (u32)tmp;

	clocksource_register(&clocksource_hpet);


	if (id & HPET_ID_LEGSUP) {
		hpet_enable_int();
		hpet_reserve_platform_timers(id);
		/*
		 * Start hpet with the boot cpu mask and make it
		 * global after the IO_APIC has been initialized.
		 */
		hpet_clockevent.cpumask =cpumask_of_cpu(0);
		clockevents_register_device(&hpet_clockevent);
		global_clock_event = &hpet_clockevent;
		return 1;
	}
	return 0;

out_nohpet:
	iounmap(hpet_virt_address);
	hpet_virt_address = NULL;
	boot_hpet_disable = 1;
	return 0;
}


#ifdef CONFIG_HPET_EMULATE_RTC

/* HPET in LegacyReplacement Mode eats up RTC interrupt line. When, HPET
 * is enabled, we support RTC interrupt functionality in software.
 * RTC has 3 kinds of interrupts:
 * 1) Update Interrupt - generate an interrupt, every sec, when RTC clock
 *    is updated
 * 2) Alarm Interrupt - generate an interrupt at a specific time of day
 * 3) Periodic Interrupt - generate periodic interrupt, with frequencies
 *    2Hz-8192Hz (2Hz-64Hz for non-root user) (all freqs in powers of 2)
 * (1) and (2) above are implemented using polling at a frequency of
 * 64 Hz. The exact frequency is a tradeoff between accuracy and interrupt
 * overhead. (DEFAULT_RTC_INT_FREQ)
 * For (3), we use interrupts at 64Hz or user specified periodic
 * frequency, whichever is higher.
 */
#include <linux/mc146818rtc.h>
#include <linux/rtc.h>

#define DEFAULT_RTC_INT_FREQ	64
#define DEFAULT_RTC_SHIFT	6
#define RTC_NUM_INTS		1

static unsigned long hpet_rtc_flags;
static unsigned long hpet_prev_update_sec;
static struct rtc_time hpet_alarm_time;
static unsigned long hpet_pie_count;
static unsigned long hpet_t1_cmp;
static unsigned long hpet_default_delta;
static unsigned long hpet_pie_delta;
static unsigned long hpet_pie_limit;

/*
 * Timer 1 for RTC emulation. We use one shot mode, as periodic mode
 * is not supported by all HPET implementations for timer 1.
 *
 * hpet_rtc_timer_init() is called when the rtc is initialized.
 */
int hpet_rtc_timer_init(void)
{
	unsigned long cfg, cnt, delta, flags;

	if (!is_hpet_enabled())
		return 0;

	if (!hpet_default_delta) {
		uint64_t clc;

		clc = (uint64_t) hpet_clockevent.mult * NSEC_PER_SEC;
		clc >>= hpet_clockevent.shift + DEFAULT_RTC_SHIFT;
		hpet_default_delta = (unsigned long) clc;
	}

	if (!(hpet_rtc_flags & RTC_PIE) || hpet_pie_limit)
		delta = hpet_default_delta;
	else
		delta = hpet_pie_delta;

	local_irq_save(flags);

	cnt = delta + hpet_readl(HPET_COUNTER);
	hpet_writel(cnt, HPET_T1_CMP);
	hpet_t1_cmp = cnt;

	cfg = hpet_readl(HPET_T1_CFG);
	cfg &= ~HPET_TN_PERIODIC;
	cfg |= HPET_TN_ENABLE | HPET_TN_32BIT;
	hpet_writel(cfg, HPET_T1_CFG);

	local_irq_restore(flags);

	return 1;
}

/*
 * The functions below are called from rtc driver.
 * Return 0 if HPET is not being used.
 * Otherwise do the necessary changes and return 1.
 */
int hpet_mask_rtc_irq_bit(unsigned long bit_mask)
{
	if (!is_hpet_enabled())
		return 0;

	hpet_rtc_flags &= ~bit_mask;
	return 1;
}

int hpet_set_rtc_irq_bit(unsigned long bit_mask)
{
	unsigned long oldbits = hpet_rtc_flags;

	if (!is_hpet_enabled())
		return 0;

	hpet_rtc_flags |= bit_mask;

	if (!oldbits)
		hpet_rtc_timer_init();

	return 1;
}

int hpet_set_alarm_time(unsigned char hrs, unsigned char min,
			unsigned char sec)
{
	if (!is_hpet_enabled())
		return 0;

	hpet_alarm_time.tm_hour = hrs;
	hpet_alarm_time.tm_min = min;
	hpet_alarm_time.tm_sec = sec;

	return 1;
}

int hpet_set_periodic_freq(unsigned long freq)
{
	uint64_t clc;

	if (!is_hpet_enabled())
		return 0;

	if (freq <= DEFAULT_RTC_INT_FREQ)
		hpet_pie_limit = DEFAULT_RTC_INT_FREQ / freq;
	else {
		clc = (uint64_t) hpet_clockevent.mult * NSEC_PER_SEC;
		do_div(clc, freq);
		clc >>= hpet_clockevent.shift;
		hpet_pie_delta = (unsigned long) clc;
	}
	return 1;
}

int hpet_rtc_dropped_irq(void)
{
	return is_hpet_enabled();
}

static void hpet_rtc_timer_reinit(void)
{
	unsigned long cfg, delta;
	int lost_ints = -1;

	if (unlikely(!hpet_rtc_flags)) {
		cfg = hpet_readl(HPET_T1_CFG);
		cfg &= ~HPET_TN_ENABLE;
		hpet_writel(cfg, HPET_T1_CFG);
		return;
	}

	if (!(hpet_rtc_flags & RTC_PIE) || hpet_pie_limit)
		delta = hpet_default_delta;
	else
		delta = hpet_pie_delta;

	/*
	 * Increment the comparator value until we are ahead of the
	 * current count.
	 */
	do {
		hpet_t1_cmp += delta;
		hpet_writel(hpet_t1_cmp, HPET_T1_CMP);
		lost_ints++;
	} while ((long)(hpet_readl(HPET_COUNTER) - hpet_t1_cmp) > 0);

	if (lost_ints) {
		if (hpet_rtc_flags & RTC_PIE)
			hpet_pie_count += lost_ints;
		if (printk_ratelimit())
			printk(KERN_WARNING "rtc: lost %d interrupts\n",
				lost_ints);
	}
}

irqreturn_t hpet_rtc_interrupt(int irq, void *dev_id)
{
	struct rtc_time curr_time;
	unsigned long rtc_int_flag = 0;

	hpet_rtc_timer_reinit();

	if (hpet_rtc_flags & (RTC_UIE | RTC_AIE))
		rtc_get_rtc_time(&curr_time);

	if (hpet_rtc_flags & RTC_UIE &&
	    curr_time.tm_sec != hpet_prev_update_sec) {
		rtc_int_flag = RTC_UF;
		hpet_prev_update_sec = curr_time.tm_sec;
	}

	if (hpet_rtc_flags & RTC_PIE &&
	    ++hpet_pie_count >= hpet_pie_limit) {
		rtc_int_flag |= RTC_PF;
		hpet_pie_count = 0;
	}

	if (hpet_rtc_flags & RTC_PIE &&
	    (curr_time.tm_sec == hpet_alarm_time.tm_sec) &&
	    (curr_time.tm_min == hpet_alarm_time.tm_min) &&
	    (curr_time.tm_hour == hpet_alarm_time.tm_hour))
			rtc_int_flag |= RTC_AF;

	if (rtc_int_flag) {
		rtc_int_flag |= (RTC_IRQF | (RTC_NUM_INTS << 8));
		rtc_interrupt(rtc_int_flag, dev_id);
	}
	return IRQ_HANDLED;
}
#endif


/*
 * Suspend/resume part
 */

#ifdef CONFIG_PM

static int hpet_suspend(struct sys_device *sys_device, pm_message_t state)
{
	unsigned long cfg = hpet_readl(HPET_CFG);

	cfg &= ~(HPET_CFG_ENABLE|HPET_CFG_LEGACY);
	hpet_writel(cfg, HPET_CFG);

	return 0;
}

static int hpet_resume(struct sys_device *sys_device)
{
	unsigned int id;

	hpet_start_counter();

	id = hpet_readl(HPET_ID);

	if (id & HPET_ID_LEGSUP)
		hpet_enable_int();

	return 0;
}

static struct sysdev_class hpet_class = {
	set_kset_name("hpet"),
	.suspend	= hpet_suspend,
	.resume		= hpet_resume,
};

static struct sys_device hpet_device = {
	.id		= 0,
	.cls		= &hpet_class,
};


static __init int hpet_register_sysfs(void)
{
	int err;

	if (!is_hpet_capable())
		return 0;

	err = sysdev_class_register(&hpet_class);

	if (!err) {
		err = sysdev_register(&hpet_device);
		if (err)
			sysdev_class_unregister(&hpet_class);
	}

	return err;
}

device_initcall(hpet_register_sysfs);

#endif
