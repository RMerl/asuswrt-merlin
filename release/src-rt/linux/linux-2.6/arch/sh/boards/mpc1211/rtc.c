/*
 * linux/arch/sh/kernel/rtc-mpc1211.c -- MPC-1211 on-chip RTC support
 *
 *  Copyright (C) 2002  Saito.K & Jeanne
 *
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/bcd.h>
#include <linux/mc146818rtc.h>

unsigned long get_cmos_time(void)
{
	unsigned int year, mon, day, hour, min, sec;

	spin_lock(&rtc_lock);

	do {
		sec = CMOS_READ(RTC_SECONDS);
		min = CMOS_READ(RTC_MINUTES);
		hour = CMOS_READ(RTC_HOURS);
		day = CMOS_READ(RTC_DAY_OF_MONTH);
		mon = CMOS_READ(RTC_MONTH);
		year = CMOS_READ(RTC_YEAR);
	} while (sec != CMOS_READ(RTC_SECONDS));

	if (!(CMOS_READ(RTC_CONTROL) & RTC_DM_BINARY) || RTC_ALWAYS_BCD) {
		BCD_TO_BIN(sec);
		BCD_TO_BIN(min);
		BCD_TO_BIN(hour);
		BCD_TO_BIN(day);
		BCD_TO_BIN(mon);
		BCD_TO_BIN(year);
	}

	spin_unlock(&rtc_lock);

	year += 1900;
	if (year < 1970)
		year += 100;

	return mktime(year, mon, day, hour, min, sec);
}

void mpc1211_rtc_gettimeofday(struct timeval *tv)
{

	tv->tv_sec = get_cmos_time();
	tv->tv_usec = 0;
}

/* arc/i386/kernel/time.c */
/*
 * In order to set the CMOS clock precisely, set_rtc_mmss has to be
 * called 500 ms after the second nowtime has started, because when
 * nowtime is written into the registers of the CMOS clock, it will
 * jump to the next second precisely 500 ms later. Check the Motorola
 * MC146818A or Dallas DS12887 data sheet for details.
 *
 * BUG: This routine does not handle hour overflow properly; it just
 *      sets the minutes. Usually you'll only notice that after reboot!
 */
static int set_rtc_mmss(unsigned long nowtime)
{
	int retval = 0;
	int real_seconds, real_minutes, cmos_minutes;
	unsigned char save_control, save_freq_select;

	/* gets recalled with irq locally disabled */
	spin_lock(&rtc_lock);
	save_control = CMOS_READ(RTC_CONTROL); /* tell the clock it's being set */
	CMOS_WRITE((save_control|RTC_SET), RTC_CONTROL);

	save_freq_select = CMOS_READ(RTC_FREQ_SELECT); /* stop and reset prescaler */
	CMOS_WRITE((save_freq_select|RTC_DIV_RESET2), RTC_FREQ_SELECT);

	cmos_minutes = CMOS_READ(RTC_MINUTES);
	if (!(save_control & RTC_DM_BINARY) || RTC_ALWAYS_BCD)
		BCD_TO_BIN(cmos_minutes);

	/*
	 * since we're only adjusting minutes and seconds,
	 * don't interfere with hour overflow. This avoids
	 * messing with unknown time zones but requires your
	 * RTC not to be off by more than 15 minutes
	 */
	real_seconds = nowtime % 60;
	real_minutes = nowtime / 60;
	if (((abs(real_minutes - cmos_minutes) + 15)/30) & 1)
		real_minutes += 30;		/* correct for half hour time zone */
	real_minutes %= 60;

	if (abs(real_minutes - cmos_minutes) < 30) {
		if (!(save_control & RTC_DM_BINARY) || RTC_ALWAYS_BCD) {
			BIN_TO_BCD(real_seconds);
			BIN_TO_BCD(real_minutes);
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

int mpc1211_rtc_settimeofday(const struct timeval *tv)
{
	unsigned long nowtime = tv->tv_sec;

	return set_rtc_mmss(nowtime);
}

void mpc1211_time_init(void)
{
	rtc_sh_get_time = mpc1211_rtc_gettimeofday;
	rtc_sh_set_time = mpc1211_rtc_settimeofday;
}

