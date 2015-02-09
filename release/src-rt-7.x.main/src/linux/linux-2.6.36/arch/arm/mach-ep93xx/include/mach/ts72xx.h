/*
 * arch/arm/mach-ep93xx/include/mach/ts72xx.h
 */

/*
 * TS72xx memory map:
 *
 * virt		phys		size
 * febff000	22000000	4K	model number register
 * febfe000	22400000	4K	options register
 * febfd000	22800000	4K	options register #2
 * febf9000	10800000	4K	TS-5620 RTC index register
 * febf8000	11700000	4K	TS-5620 RTC data register
 */

#define TS72XX_MODEL_PHYS_BASE		0x22000000
#define TS72XX_MODEL_VIRT_BASE		0xfebff000
#define TS72XX_MODEL_SIZE		0x00001000

#define TS72XX_MODEL_TS7200		0x00
#define TS72XX_MODEL_TS7250		0x01
#define TS72XX_MODEL_TS7260		0x02


#define TS72XX_OPTIONS_PHYS_BASE	0x22400000
#define TS72XX_OPTIONS_VIRT_BASE	0xfebfe000
#define TS72XX_OPTIONS_SIZE		0x00001000

#define TS72XX_OPTIONS_COM2_RS485	0x02
#define TS72XX_OPTIONS_MAX197		0x01


#define TS72XX_OPTIONS2_PHYS_BASE	0x22800000
#define TS72XX_OPTIONS2_VIRT_BASE	0xfebfd000
#define TS72XX_OPTIONS2_SIZE		0x00001000

#define TS72XX_OPTIONS2_TS9420		0x04
#define TS72XX_OPTIONS2_TS9420_BOOT	0x02


#define TS72XX_RTC_INDEX_VIRT_BASE	0xfebf9000
#define TS72XX_RTC_INDEX_PHYS_BASE	0x10800000
#define TS72XX_RTC_INDEX_SIZE		0x00001000

#define TS72XX_RTC_DATA_VIRT_BASE	0xfebf8000
#define TS72XX_RTC_DATA_PHYS_BASE	0x11700000
#define TS72XX_RTC_DATA_SIZE		0x00001000

#define TS72XX_WDT_CONTROL_PHYS_BASE	0x23800000
#define TS72XX_WDT_FEED_PHYS_BASE	0x23c00000

#ifndef __ASSEMBLY__

static inline int board_is_ts7200(void)
{
	return __raw_readb(TS72XX_MODEL_VIRT_BASE) == TS72XX_MODEL_TS7200;
}

static inline int board_is_ts7250(void)
{
	return __raw_readb(TS72XX_MODEL_VIRT_BASE) == TS72XX_MODEL_TS7250;
}

static inline int board_is_ts7260(void)
{
	return __raw_readb(TS72XX_MODEL_VIRT_BASE) == TS72XX_MODEL_TS7260;
}

static inline int is_max197_installed(void)
{
	return !!(__raw_readb(TS72XX_OPTIONS_VIRT_BASE) &
					TS72XX_OPTIONS_MAX197);
}

static inline int is_ts9420_installed(void)
{
	return !!(__raw_readb(TS72XX_OPTIONS2_VIRT_BASE) &
					TS72XX_OPTIONS2_TS9420);
}
#endif
