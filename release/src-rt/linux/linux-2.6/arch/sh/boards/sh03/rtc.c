/*
 * linux/arch/sh/boards/sh03/rtc.c -- CTP/PCI-SH03 on-chip RTC support
 *
 *  Copyright (C) 2004  Saito.K & Jeanne(ksaito@interface.co.jp)
 *
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/bcd.h>
#include <linux/rtc.h>
#include <linux/spinlock.h>
#include <asm/io.h>
#include <asm/rtc.h>

#define RTC_BASE	0xb0000000
#define RTC_SEC1	(RTC_BASE + 0)
#define RTC_SEC10	(RTC_BASE + 1)
#define RTC_MIN1	(RTC_BASE + 2)
#define RTC_MIN10	(RTC_BASE + 3)
#define RTC_HOU1	(RTC_BASE + 4)
#define RTC_HOU10	(RTC_BASE + 5)
#define RTC_WEE1	(RTC_BASE + 6)
#define RTC_DAY1	(RTC_BASE + 7)
#define RTC_DAY10	(RTC_BASE + 8)
#define RTC_MON1	(RTC_BASE + 9)
#define RTC_MON10	(RTC_BASE + 10)
#define RTC_YEA1	(RTC_BASE + 11)
#define RTC_YEA10	(RTC_BASE + 12)
#define RTC_YEA100	(RTC_BASE + 13)
#define RTC_YEA1000	(RTC_BASE + 14)
#define RTC_CTL		(RTC_BASE + 15)
#define RTC_BUSY	1
#define RTC_STOP	2

extern spinlock_t rtc_lock;

unsigned long get_cmos_time(void)
{
	unsigned int year, mon, day, hour, min, sec;

	spin_lock(&rtc_lock);
 again:
	do {
		sec  = (ctrl_inb(RTC_SEC1) & 0xf) + (ctrl_inb(RTC_SEC10) & 0x7) * 10;
		min  = (ctrl_inb(RTC_MIN1) & 0xf) + (ctrl_inb(RTC_MIN10) & 0xf) * 10;
		hour = (ctrl_inb(RTC_HOU1) & 0xf) + (ctrl_inb(RTC_HOU10) & 0xf) * 10;
		day  = (ctrl_inb(RTC_DAY1) & 0xf) + (ctrl_inb(RTC_DAY10) & 0xf) * 10;
		mon  = (ctrl_inb(RTC_MON1) & 0xf) + (ctrl_inb(RTC_MON10) & 0xf) * 10;
		year = (ctrl_inb(RTC_YEA1) & 0xf) + (ctrl_inb(RTC_YEA10) & 0xf) * 10
		     + (ctrl_inb(RTC_YEA100 ) & 0xf) * 100
		     + (ctrl_inb(RTC_YEA1000) & 0xf) * 1000;
	} while (sec != (ctrl_inb(RTC_SEC1) & 0xf) + (ctrl_inb(RTC_SEC10) & 0x7) * 10);
	if (year == 0 || mon < 1 || mon > 12 || day > 31 || day < 1 ||
	    hour > 23 || min > 59 || sec > 59) {
		printk(KERN_ERR
		       "SH-03 RTC: invalid value, resetting to 1 Jan 2000\n");
		printk("year=%d, mon=%d, day=%d, hour=%d, min=%d, sec=%d\n",
		       year, mon, day, hour, min, sec);

		ctrl_outb(0, RTC_SEC1); ctrl_outb(0, RTC_SEC10);
		ctrl_outb(0, RTC_MIN1); ctrl_outb(0, RTC_MIN10);
		ctrl_outb(0, RTC_HOU1); ctrl_outb(0, RTC_HOU10);
		ctrl_outb(6, RTC_WEE1);
		ctrl_outb(1, RTC_DAY1); ctrl_outb(0, RTC_DAY10);
		ctrl_outb(1, RTC_MON1); ctrl_outb(0, RTC_MON10);
		ctrl_outb(0, RTC_YEA1); ctrl_outb(0, RTC_YEA10);
		ctrl_outb(0, RTC_YEA100);
		ctrl_outb(2, RTC_YEA1000);
		ctrl_outb(0, RTC_CTL);
		goto again;
	}

	spin_unlock(&rtc_lock);
	return mktime(year, mon, day, hour, min, sec);
}

void sh03_rtc_gettimeofday(struct timespec *tv)
{

	tv->tv_sec = get_cmos_time();
	tv->tv_nsec = 0;
}

static int set_rtc_mmss(unsigned long nowtime)
{
	int retval = 0;
	int real_seconds, real_minutes, cmos_minutes;
	int i;

	/* gets recalled with irq locally disabled */
	spin_lock(&rtc_lock);
	for (i = 0 ; i < 1000000 ; i++)	/* may take up to 1 second... */
		if (!(ctrl_inb(RTC_CTL) & RTC_BUSY))
			break;
	cmos_minutes = (ctrl_inb(RTC_MIN1) & 0xf) + (ctrl_inb(RTC_MIN10) & 0xf) * 10;
	real_seconds = nowtime % 60;
	real_minutes = nowtime / 60;
	if (((abs(real_minutes - cmos_minutes) + 15)/30) & 1)
		real_minutes += 30;		/* correct for half hour time zone */
	real_minutes %= 60;

	if (abs(real_minutes - cmos_minutes) < 30) {
		ctrl_outb(real_seconds % 10, RTC_SEC1);
		ctrl_outb(real_seconds / 10, RTC_SEC10);
		ctrl_outb(real_minutes % 10, RTC_MIN1);
		ctrl_outb(real_minutes / 10, RTC_MIN10);
	} else {
		printk(KERN_WARNING
		       "set_rtc_mmss: can't update from %d to %d\n",
		       cmos_minutes, real_minutes);
		retval = -1;
	}
	spin_unlock(&rtc_lock);

	return retval;
}

int sh03_rtc_settimeofday(const time_t secs)
{
	unsigned long nowtime = secs;

	return set_rtc_mmss(nowtime);
}

void sh03_time_init(void)
{
	rtc_sh_get_time = sh03_rtc_gettimeofday;
	rtc_sh_set_time = sh03_rtc_settimeofday;
}
