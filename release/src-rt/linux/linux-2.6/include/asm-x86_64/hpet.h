#ifndef _ASM_X8664_HPET_H
#define _ASM_X8664_HPET_H 1

/*
 * Documentation on HPET can be found at:
 *      http://www.intel.com/ial/home/sp/pcmmspec.htm
 *      ftp://download.intel.com/ial/home/sp/mmts098.pdf
 */

#define HPET_MMAP_SIZE	1024

#define HPET_ID		0x000
#define HPET_PERIOD	0x004
#define HPET_CFG	0x010
#define HPET_STATUS	0x020
#define HPET_COUNTER	0x0f0
#define HPET_Tn_OFFSET	0x20
#define HPET_Tn_CFG(n)	 (0x100 + (n) * HPET_Tn_OFFSET)
#define HPET_Tn_ROUTE(n) (0x104 + (n) * HPET_Tn_OFFSET)
#define HPET_Tn_CMP(n)	 (0x108 + (n) * HPET_Tn_OFFSET)
#define HPET_T0_CFG	HPET_Tn_CFG(0)
#define HPET_T0_CMP	HPET_Tn_CMP(0)
#define HPET_T1_CFG	HPET_Tn_CFG(1)
#define HPET_T1_CMP	HPET_Tn_CMP(1)

#define HPET_ID_VENDOR	0xffff0000
#define HPET_ID_LEGSUP	0x00008000
#define HPET_ID_64BIT	0x00002000
#define HPET_ID_NUMBER	0x00001f00
#define HPET_ID_REV	0x000000ff
#define	HPET_ID_NUMBER_SHIFT	8

#define HPET_ID_VENDOR_SHIFT	16
#define HPET_ID_VENDOR_8086	0x8086

#define HPET_CFG_ENABLE	0x001
#define HPET_CFG_LEGACY	0x002
#define	HPET_LEGACY_8254	2
#define	HPET_LEGACY_RTC		8

#define HPET_TN_LEVEL		0x0002
#define HPET_TN_ENABLE		0x0004
#define HPET_TN_PERIODIC	0x0008
#define HPET_TN_PERIODIC_CAP	0x0010
#define HPET_TN_64BIT_CAP	0x0020
#define HPET_TN_SETVAL		0x0040
#define HPET_TN_32BIT		0x0100
#define HPET_TN_ROUTE		0x3e00
#define HPET_TN_FSB		0x4000
#define HPET_TN_FSB_CAP		0x8000

#define HPET_TN_ROUTE_SHIFT	9

#define HPET_TICK_RATE (HZ * 100000UL)

extern int is_hpet_enabled(void);
extern int hpet_rtc_timer_init(void);
extern int apic_is_clustered_box(void);
extern int hpet_arch_init(void);
extern int hpet_timer_stop_set_go(unsigned long tick);
extern int hpet_reenable(void);
extern unsigned int hpet_calibrate_tsc(void);

extern int hpet_use_timer;
extern unsigned long hpet_address;
extern unsigned long hpet_period;
extern unsigned long hpet_tick;

#ifdef CONFIG_HPET_EMULATE_RTC
extern int hpet_mask_rtc_irq_bit(unsigned long bit_mask);
extern int hpet_set_rtc_irq_bit(unsigned long bit_mask);
extern int hpet_set_alarm_time(unsigned char hrs, unsigned char min, unsigned char sec);
extern int hpet_set_periodic_freq(unsigned long freq);
extern int hpet_rtc_dropped_irq(void);
extern int hpet_rtc_timer_init(void);
#endif /* CONFIG_HPET_EMULATE_RTC */

#endif
