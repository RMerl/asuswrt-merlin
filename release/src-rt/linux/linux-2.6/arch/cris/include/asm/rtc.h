
#ifndef __RTC_H__
#define __RTC_H__

#ifdef CONFIG_ETRAX_DS1302
   /* Dallas DS1302 clock/calendar register numbers. */
#  define RTC_SECONDS      0
#  define RTC_MINUTES      1
#  define RTC_HOURS        2
#  define RTC_DAY_OF_MONTH 3
#  define RTC_MONTH        4
#  define RTC_WEEKDAY      5
#  define RTC_YEAR         6
#  define RTC_CONTROL      7

   /* Bits in CONTROL register. */
#  define RTC_CONTROL_WRITEPROTECT	0x80
#  define RTC_TRICKLECHARGER		8

  /* Bits in TRICKLECHARGER register TCS TCS TCS TCS DS DS RS RS. */
#  define RTC_TCR_PATTERN	0xA0	/* 1010xxxx */
#  define RTC_TCR_1DIOD		0x04	/* xxxx01xx */
#  define RTC_TCR_2DIOD		0x08	/* xxxx10xx */
#  define RTC_TCR_DISABLED	0x00	/* xxxxxx00 Disabled */
#  define RTC_TCR_2KOHM		0x01	/* xxxxxx01 2KOhm */
#  define RTC_TCR_4KOHM		0x02	/* xxxxxx10 4kOhm */
#  define RTC_TCR_8KOHM		0x03	/* xxxxxx11 8kOhm */

#elif defined(CONFIG_ETRAX_PCF8563)
   /* I2C bus slave registers. */
#  define RTC_I2C_READ		0xa3
#  define RTC_I2C_WRITE		0xa2

   /* Phillips PCF8563 registers. */
#  define RTC_CONTROL1		0x00		/* Control/Status register 1. */
#  define RTC_CONTROL2		0x01		/* Control/Status register 2. */
#  define RTC_CLOCKOUT_FREQ	0x0d		/* CLKOUT frequency. */
#  define RTC_TIMER_CONTROL	0x0e		/* Timer control. */
#  define RTC_TIMER_CNTDOWN	0x0f		/* Timer countdown. */

   /* BCD encoded clock registers. */
#  define RTC_SECONDS		0x02
#  define RTC_MINUTES		0x03
#  define RTC_HOURS		0x04
#  define RTC_DAY_OF_MONTH	0x05
#  define RTC_WEEKDAY		0x06	/* Not coded in BCD! */
#  define RTC_MONTH		0x07
#  define RTC_YEAR		0x08
#  define RTC_MINUTE_ALARM	0x09
#  define RTC_HOUR_ALARM	0x0a
#  define RTC_DAY_ALARM		0x0b
#  define RTC_WEEKDAY_ALARM 0x0c

#endif

#ifdef CONFIG_ETRAX_DS1302
extern unsigned char ds1302_readreg(int reg);
extern void ds1302_writereg(int reg, unsigned char val);
extern int ds1302_init(void);
#  define CMOS_READ(x) ds1302_readreg(x)
#  define CMOS_WRITE(val,reg) ds1302_writereg(reg,val)
#  define RTC_INIT() ds1302_init()
#elif defined(CONFIG_ETRAX_PCF8563)
extern unsigned char pcf8563_readreg(int reg);
extern void pcf8563_writereg(int reg, unsigned char val);
extern int pcf8563_init(void);
#  define CMOS_READ(x) pcf8563_readreg(x)
#  define CMOS_WRITE(val,reg) pcf8563_writereg(reg,val)
#  define RTC_INIT() pcf8563_init()
#else
  /* No RTC configured so we shouldn't try to access any. */
#  define CMOS_READ(x) 42
#  define CMOS_WRITE(x,y)
#  define RTC_INIT() (-1)
#endif

/*
 * The struct used to pass data via the following ioctl. Similar to the
 * struct tm in <time.h>, but it needs to be here so that the kernel
 * source is self contained, allowing cross-compiles, etc. etc.
 */
struct rtc_time {
	int tm_sec;
	int tm_min;
	int tm_hour;
	int tm_mday;
	int tm_mon;
	int tm_year;
	int tm_wday;
	int tm_yday;
	int tm_isdst;
};

/* ioctl() calls that are permitted to the /dev/rtc interface. */
#define RTC_MAGIC 'p'
/* Read RTC time. */
#define RTC_RD_TIME		_IOR(RTC_MAGIC, 0x09, struct rtc_time)
/* Set RTC time. */
#define RTC_SET_TIME		_IOW(RTC_MAGIC, 0x0a, struct rtc_time)
#define RTC_SET_CHARGE		_IOW(RTC_MAGIC, 0x0b, int)
/* Voltage low detector */
#define RTC_VL_READ		_IOR(RTC_MAGIC, 0x13, int)
/* Clear voltage low information */
#define RTC_VL_CLR		_IO(RTC_MAGIC, 0x14)
#define RTC_MAX_IOCTL 0x14

#endif /* __RTC_H__ */
