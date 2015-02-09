/*
 *	intel TCO Watchdog Driver
 *
 *	(c) Copyright 2006-2009 Wim Van Sebroeck <wim@iguana.be>.
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 *
 *	Neither Wim Van Sebroeck nor Iguana vzw. admit liability nor
 *	provide warranty for any of this software. This material is
 *	provided "AS-IS" and at no charge.
 *
 *	The TCO watchdog is implemented in the following I/O controller hubs:
 *	(See the intel documentation on http://developer.intel.com.)
 *	document number 290655-003, 290677-014: 82801AA (ICH), 82801AB (ICHO)
 *	document number 290687-002, 298242-027: 82801BA (ICH2)
 *	document number 290733-003, 290739-013: 82801CA (ICH3-S)
 *	document number 290716-001, 290718-007: 82801CAM (ICH3-M)
 *	document number 290744-001, 290745-025: 82801DB (ICH4)
 *	document number 252337-001, 252663-008: 82801DBM (ICH4-M)
 *	document number 273599-001, 273645-002: 82801E (C-ICH)
 *	document number 252516-001, 252517-028: 82801EB (ICH5), 82801ER (ICH5R)
 *	document number 300641-004, 300884-013: 6300ESB
 *	document number 301473-002, 301474-026: 82801F (ICH6)
 *	document number 313082-001, 313075-006: 631xESB, 632xESB
 *	document number 307013-003, 307014-024: 82801G (ICH7)
 *	document number 313056-003, 313057-017: 82801H (ICH8)
 *	document number 316972-004, 316973-012: 82801I (ICH9)
 *	document number 319973-002, 319974-002: 82801J (ICH10)
 *	document number 322169-001, 322170-003: 5 Series, 3400 Series (PCH)
 *	document number 320066-003, 320257-008: EP80597 (IICH)
 *	document number TBD                   : Cougar Point (CPT)
 */

/*
 *	Includes, defines, variables, module parameters, ...
 */

/* Module and version information */
#define DRV_NAME	"iTCO_wdt"
#define DRV_VERSION	"1.06"
#define PFX		DRV_NAME ": "

/* Includes */
#include <linux/module.h>		/* For module specific items */
#include <linux/moduleparam.h>		/* For new moduleparam's */
#include <linux/types.h>		/* For standard types (like size_t) */
#include <linux/errno.h>		/* For the -ENODEV/... values */
#include <linux/kernel.h>		/* For printk/panic/... */
#include <linux/miscdevice.h>		/* For MODULE_ALIAS_MISCDEV
							(WATCHDOG_MINOR) */
#include <linux/watchdog.h>		/* For the watchdog specific items */
#include <linux/init.h>			/* For __init/__exit/... */
#include <linux/fs.h>			/* For file operations */
#include <linux/platform_device.h>	/* For platform_driver framework */
#include <linux/pci.h>			/* For pci functions */
#include <linux/ioport.h>		/* For io-port access */
#include <linux/spinlock.h>		/* For spin_lock/spin_unlock/... */
#include <linux/uaccess.h>		/* For copy_to_user/put_user/... */
#include <linux/io.h>			/* For inb/outb/... */

#include "iTCO_vendor.h"

/* TCO related info */
enum iTCO_chipsets {
	TCO_ICH = 0,	/* ICH */
	TCO_ICH0,	/* ICH0 */
	TCO_ICH2,	/* ICH2 */
	TCO_ICH2M,	/* ICH2-M */
	TCO_ICH3,	/* ICH3-S */
	TCO_ICH3M,	/* ICH3-M */
	TCO_ICH4,	/* ICH4 */
	TCO_ICH4M,	/* ICH4-M */
	TCO_CICH,	/* C-ICH */
	TCO_ICH5,	/* ICH5 & ICH5R */
	TCO_6300ESB,	/* 6300ESB */
	TCO_ICH6,	/* ICH6 & ICH6R */
	TCO_ICH6M,	/* ICH6-M */
	TCO_ICH6W,	/* ICH6W & ICH6RW */
	TCO_631XESB,	/* 631xESB/632xESB */
	TCO_ICH7,	/* ICH7 & ICH7R */
	TCO_ICH7DH,	/* ICH7DH */
	TCO_ICH7M,	/* ICH7-M & ICH7-U */
	TCO_ICH7MDH,	/* ICH7-M DH */
	TCO_ICH8,	/* ICH8 & ICH8R */
	TCO_ICH8DH,	/* ICH8DH */
	TCO_ICH8DO,	/* ICH8DO */
	TCO_ICH8M,	/* ICH8M */
	TCO_ICH8ME,	/* ICH8M-E */
	TCO_ICH9,	/* ICH9 */
	TCO_ICH9R,	/* ICH9R */
	TCO_ICH9DH,	/* ICH9DH */
	TCO_ICH9DO,	/* ICH9DO */
	TCO_ICH9M,	/* ICH9M */
	TCO_ICH9ME,	/* ICH9M-E */
	TCO_ICH10,	/* ICH10 */
	TCO_ICH10R,	/* ICH10R */
	TCO_ICH10D,	/* ICH10D */
	TCO_ICH10DO,	/* ICH10DO */
	TCO_PCH,	/* PCH Desktop Full Featured */
	TCO_PCHM,	/* PCH Mobile Full Featured */
	TCO_P55,	/* P55 */
	TCO_PM55,	/* PM55 */
	TCO_H55,	/* H55 */
	TCO_QM57,	/* QM57 */
	TCO_H57,	/* H57 */
	TCO_HM55,	/* HM55 */
	TCO_Q57,	/* Q57 */
	TCO_HM57,	/* HM57 */
	TCO_PCHMSFF,	/* PCH Mobile SFF Full Featured */
	TCO_QS57,	/* QS57 */
	TCO_3400,	/* 3400 */
	TCO_3420,	/* 3420 */
	TCO_3450,	/* 3450 */
	TCO_EP80579,	/* EP80579 */
	TCO_CPT1,	/* Cougar Point */
	TCO_CPT2,	/* Cougar Point Desktop */
	TCO_CPT3,	/* Cougar Point Mobile */
	TCO_CPT4,	/* Cougar Point */
	TCO_CPT5,	/* Cougar Point */
	TCO_CPT6,	/* Cougar Point */
	TCO_CPT7,	/* Cougar Point */
	TCO_CPT8,	/* Cougar Point */
	TCO_CPT9,	/* Cougar Point */
	TCO_CPT10,	/* Cougar Point */
	TCO_CPT11,	/* Cougar Point */
	TCO_CPT12,	/* Cougar Point */
	TCO_CPT13,	/* Cougar Point */
	TCO_CPT14,	/* Cougar Point */
	TCO_CPT15,	/* Cougar Point */
	TCO_CPT16,	/* Cougar Point */
	TCO_CPT17,	/* Cougar Point */
	TCO_CPT18,	/* Cougar Point */
	TCO_CPT19,	/* Cougar Point */
	TCO_CPT20,	/* Cougar Point */
	TCO_CPT21,	/* Cougar Point */
	TCO_CPT22,	/* Cougar Point */
	TCO_CPT23,	/* Cougar Point */
	TCO_CPT24,	/* Cougar Point */
	TCO_CPT25,	/* Cougar Point */
	TCO_CPT26,	/* Cougar Point */
	TCO_CPT27,	/* Cougar Point */
	TCO_CPT28,	/* Cougar Point */
	TCO_CPT29,	/* Cougar Point */
	TCO_CPT30,	/* Cougar Point */
	TCO_CPT31,	/* Cougar Point */
};

static struct {
	char *name;
	unsigned int iTCO_version;
} iTCO_chipset_info[] __devinitdata = {
	{"ICH", 1},
	{"ICH0", 1},
	{"ICH2", 1},
	{"ICH2-M", 1},
	{"ICH3-S", 1},
	{"ICH3-M", 1},
	{"ICH4", 1},
	{"ICH4-M", 1},
	{"C-ICH", 1},
	{"ICH5 or ICH5R", 1},
	{"6300ESB", 1},
	{"ICH6 or ICH6R", 2},
	{"ICH6-M", 2},
	{"ICH6W or ICH6RW", 2},
	{"631xESB/632xESB", 2},
	{"ICH7 or ICH7R", 2},
	{"ICH7DH", 2},
	{"ICH7-M or ICH7-U", 2},
	{"ICH7-M DH", 2},
	{"ICH8 or ICH8R", 2},
	{"ICH8DH", 2},
	{"ICH8DO", 2},
	{"ICH8M", 2},
	{"ICH8M-E", 2},
	{"ICH9", 2},
	{"ICH9R", 2},
	{"ICH9DH", 2},
	{"ICH9DO", 2},
	{"ICH9M", 2},
	{"ICH9M-E", 2},
	{"ICH10", 2},
	{"ICH10R", 2},
	{"ICH10D", 2},
	{"ICH10DO", 2},
	{"PCH Desktop Full Featured", 2},
	{"PCH Mobile Full Featured", 2},
	{"P55", 2},
	{"PM55", 2},
	{"H55", 2},
	{"QM57", 2},
	{"H57", 2},
	{"HM55", 2},
	{"Q57", 2},
	{"HM57", 2},
	{"PCH Mobile SFF Full Featured", 2},
	{"QS57", 2},
	{"3400", 2},
	{"3420", 2},
	{"3450", 2},
	{"EP80579", 2},
	{"Cougar Point", 2},
	{"Cougar Point", 2},
	{"Cougar Point", 2},
	{"Cougar Point", 2},
	{"Cougar Point", 2},
	{"Cougar Point", 2},
	{"Cougar Point", 2},
	{"Cougar Point", 2},
	{"Cougar Point", 2},
	{"Cougar Point", 2},
	{"Cougar Point", 2},
	{"Cougar Point", 2},
	{"Cougar Point", 2},
	{"Cougar Point", 2},
	{"Cougar Point", 2},
	{"Cougar Point", 2},
	{"Cougar Point", 2},
	{"Cougar Point", 2},
	{"Cougar Point", 2},
	{"Cougar Point", 2},
	{"Cougar Point", 2},
	{"Cougar Point", 2},
	{"Cougar Point", 2},
	{"Cougar Point", 2},
	{"Cougar Point", 2},
	{"Cougar Point", 2},
	{"Cougar Point", 2},
	{"Cougar Point", 2},
	{"Cougar Point", 2},
	{"Cougar Point", 2},
	{"Cougar Point", 2},
	{NULL, 0}
};

#define ITCO_PCI_DEVICE(dev, data) 	\
	.vendor = PCI_VENDOR_ID_INTEL,	\
	.device = dev,			\
	.subvendor = PCI_ANY_ID,	\
	.subdevice = PCI_ANY_ID,	\
	.class = 0,			\
	.class_mask = 0,		\
	.driver_data = data

/*
 * This data only exists for exporting the supported PCI ids
 * via MODULE_DEVICE_TABLE.  We do not actually register a
 * pci_driver, because the I/O Controller Hub has also other
 * functions that probably will be registered by other drivers.
 */
static struct pci_device_id iTCO_wdt_pci_tbl[] = {
	{ ITCO_PCI_DEVICE(PCI_DEVICE_ID_INTEL_82801AA_0,	TCO_ICH)},
	{ ITCO_PCI_DEVICE(PCI_DEVICE_ID_INTEL_82801AB_0,	TCO_ICH0)},
	{ ITCO_PCI_DEVICE(PCI_DEVICE_ID_INTEL_82801BA_0,	TCO_ICH2)},
	{ ITCO_PCI_DEVICE(PCI_DEVICE_ID_INTEL_82801BA_10,	TCO_ICH2M)},
	{ ITCO_PCI_DEVICE(PCI_DEVICE_ID_INTEL_82801CA_0,	TCO_ICH3)},
	{ ITCO_PCI_DEVICE(PCI_DEVICE_ID_INTEL_82801CA_12,	TCO_ICH3M)},
	{ ITCO_PCI_DEVICE(PCI_DEVICE_ID_INTEL_82801DB_0,	TCO_ICH4)},
	{ ITCO_PCI_DEVICE(PCI_DEVICE_ID_INTEL_82801DB_12,	TCO_ICH4M)},
	{ ITCO_PCI_DEVICE(PCI_DEVICE_ID_INTEL_82801E_0,		TCO_CICH)},
	{ ITCO_PCI_DEVICE(PCI_DEVICE_ID_INTEL_82801EB_0,	TCO_ICH5)},
	{ ITCO_PCI_DEVICE(PCI_DEVICE_ID_INTEL_ESB_1,		TCO_6300ESB)},
	{ ITCO_PCI_DEVICE(PCI_DEVICE_ID_INTEL_ICH6_0,		TCO_ICH6)},
	{ ITCO_PCI_DEVICE(PCI_DEVICE_ID_INTEL_ICH6_1,		TCO_ICH6M)},
	{ ITCO_PCI_DEVICE(PCI_DEVICE_ID_INTEL_ICH6_2,		TCO_ICH6W)},
	{ ITCO_PCI_DEVICE(PCI_DEVICE_ID_INTEL_ESB2_0,		TCO_631XESB)},
	{ ITCO_PCI_DEVICE(0x2671,				TCO_631XESB)},
	{ ITCO_PCI_DEVICE(0x2672,				TCO_631XESB)},
	{ ITCO_PCI_DEVICE(0x2673,				TCO_631XESB)},
	{ ITCO_PCI_DEVICE(0x2674,				TCO_631XESB)},
	{ ITCO_PCI_DEVICE(0x2675,				TCO_631XESB)},
	{ ITCO_PCI_DEVICE(0x2676,				TCO_631XESB)},
	{ ITCO_PCI_DEVICE(0x2677,				TCO_631XESB)},
	{ ITCO_PCI_DEVICE(0x2678,				TCO_631XESB)},
	{ ITCO_PCI_DEVICE(0x2679,				TCO_631XESB)},
	{ ITCO_PCI_DEVICE(0x267a,				TCO_631XESB)},
	{ ITCO_PCI_DEVICE(0x267b,				TCO_631XESB)},
	{ ITCO_PCI_DEVICE(0x267c,				TCO_631XESB)},
	{ ITCO_PCI_DEVICE(0x267d,				TCO_631XESB)},
	{ ITCO_PCI_DEVICE(0x267e,				TCO_631XESB)},
	{ ITCO_PCI_DEVICE(0x267f,				TCO_631XESB)},
	{ ITCO_PCI_DEVICE(PCI_DEVICE_ID_INTEL_ICH7_0,		TCO_ICH7)},
	{ ITCO_PCI_DEVICE(PCI_DEVICE_ID_INTEL_ICH7_30,		TCO_ICH7DH)},
	{ ITCO_PCI_DEVICE(PCI_DEVICE_ID_INTEL_ICH7_1,		TCO_ICH7M)},
	{ ITCO_PCI_DEVICE(PCI_DEVICE_ID_INTEL_ICH7_31,		TCO_ICH7MDH)},
	{ ITCO_PCI_DEVICE(PCI_DEVICE_ID_INTEL_ICH8_0,		TCO_ICH8)},
	{ ITCO_PCI_DEVICE(PCI_DEVICE_ID_INTEL_ICH8_2,		TCO_ICH8DH)},
	{ ITCO_PCI_DEVICE(PCI_DEVICE_ID_INTEL_ICH8_3,		TCO_ICH8DO)},
	{ ITCO_PCI_DEVICE(PCI_DEVICE_ID_INTEL_ICH8_4,		TCO_ICH8M)},
	{ ITCO_PCI_DEVICE(PCI_DEVICE_ID_INTEL_ICH8_1,		TCO_ICH8ME)},
	{ ITCO_PCI_DEVICE(0x2918,				TCO_ICH9)},
	{ ITCO_PCI_DEVICE(0x2916,				TCO_ICH9R)},
	{ ITCO_PCI_DEVICE(PCI_DEVICE_ID_INTEL_ICH9_2,		TCO_ICH9DH)},
	{ ITCO_PCI_DEVICE(PCI_DEVICE_ID_INTEL_ICH9_4,		TCO_ICH9DO)},
	{ ITCO_PCI_DEVICE(0x2919,				TCO_ICH9M)},
	{ ITCO_PCI_DEVICE(0x2917,				TCO_ICH9ME)},
	{ ITCO_PCI_DEVICE(0x3a18,				TCO_ICH10)},
	{ ITCO_PCI_DEVICE(0x3a16,				TCO_ICH10R)},
	{ ITCO_PCI_DEVICE(0x3a1a,				TCO_ICH10D)},
	{ ITCO_PCI_DEVICE(0x3a14,				TCO_ICH10DO)},
	{ ITCO_PCI_DEVICE(0x3b00,				TCO_PCH)},
	{ ITCO_PCI_DEVICE(0x3b01,				TCO_PCHM)},
	{ ITCO_PCI_DEVICE(0x3b02,				TCO_P55)},
	{ ITCO_PCI_DEVICE(0x3b03,				TCO_PM55)},
	{ ITCO_PCI_DEVICE(0x3b06,				TCO_H55)},
	{ ITCO_PCI_DEVICE(0x3b07,				TCO_QM57)},
	{ ITCO_PCI_DEVICE(0x3b08,				TCO_H57)},
	{ ITCO_PCI_DEVICE(0x3b09,				TCO_HM55)},
	{ ITCO_PCI_DEVICE(0x3b0a,				TCO_Q57)},
	{ ITCO_PCI_DEVICE(0x3b0b,				TCO_HM57)},
	{ ITCO_PCI_DEVICE(0x3b0d,				TCO_PCHMSFF)},
	{ ITCO_PCI_DEVICE(0x3b0f,				TCO_QS57)},
	{ ITCO_PCI_DEVICE(0x3b12,				TCO_3400)},
	{ ITCO_PCI_DEVICE(0x3b14,				TCO_3420)},
	{ ITCO_PCI_DEVICE(0x3b16,				TCO_3450)},
	{ ITCO_PCI_DEVICE(0x5031,				TCO_EP80579)},
	{ ITCO_PCI_DEVICE(0x1c41,				TCO_CPT1)},
	{ ITCO_PCI_DEVICE(0x1c42,				TCO_CPT2)},
	{ ITCO_PCI_DEVICE(0x1c43,				TCO_CPT3)},
	{ ITCO_PCI_DEVICE(0x1c44,				TCO_CPT4)},
	{ ITCO_PCI_DEVICE(0x1c45,				TCO_CPT5)},
	{ ITCO_PCI_DEVICE(0x1c46,				TCO_CPT6)},
	{ ITCO_PCI_DEVICE(0x1c47,				TCO_CPT7)},
	{ ITCO_PCI_DEVICE(0x1c48,				TCO_CPT8)},
	{ ITCO_PCI_DEVICE(0x1c49,				TCO_CPT9)},
	{ ITCO_PCI_DEVICE(0x1c4a,				TCO_CPT10)},
	{ ITCO_PCI_DEVICE(0x1c4b,				TCO_CPT11)},
	{ ITCO_PCI_DEVICE(0x1c4c,				TCO_CPT12)},
	{ ITCO_PCI_DEVICE(0x1c4d,				TCO_CPT13)},
	{ ITCO_PCI_DEVICE(0x1c4e,				TCO_CPT14)},
	{ ITCO_PCI_DEVICE(0x1c4f,				TCO_CPT15)},
	{ ITCO_PCI_DEVICE(0x1c50,				TCO_CPT16)},
	{ ITCO_PCI_DEVICE(0x1c51,				TCO_CPT17)},
	{ ITCO_PCI_DEVICE(0x1c52,				TCO_CPT18)},
	{ ITCO_PCI_DEVICE(0x1c53,				TCO_CPT19)},
	{ ITCO_PCI_DEVICE(0x1c54,				TCO_CPT20)},
	{ ITCO_PCI_DEVICE(0x1c55,				TCO_CPT21)},
	{ ITCO_PCI_DEVICE(0x1c56,				TCO_CPT22)},
	{ ITCO_PCI_DEVICE(0x1c57,				TCO_CPT23)},
	{ ITCO_PCI_DEVICE(0x1c58,				TCO_CPT24)},
	{ ITCO_PCI_DEVICE(0x1c59,				TCO_CPT25)},
	{ ITCO_PCI_DEVICE(0x1c5a,				TCO_CPT26)},
	{ ITCO_PCI_DEVICE(0x1c5b,				TCO_CPT27)},
	{ ITCO_PCI_DEVICE(0x1c5c,				TCO_CPT28)},
	{ ITCO_PCI_DEVICE(0x1c5d,				TCO_CPT29)},
	{ ITCO_PCI_DEVICE(0x1c5e,				TCO_CPT30)},
	{ ITCO_PCI_DEVICE(0x1c5f,				TCO_CPT31)},
	{ 0, },			/* End of list */
};
MODULE_DEVICE_TABLE(pci, iTCO_wdt_pci_tbl);

/* Address definitions for the TCO */
/* TCO base address */
#define TCOBASE		(iTCO_wdt_private.ACPIBASE + 0x60)
/* SMI Control and Enable Register */
#define SMI_EN		(iTCO_wdt_private.ACPIBASE + 0x30)

#define TCO_RLD		(TCOBASE + 0x00) /* TCO Timer Reload and Curr. Value */
#define TCOv1_TMR	(TCOBASE + 0x01) /* TCOv1 Timer Initial Value	*/
#define TCO_DAT_IN	(TCOBASE + 0x02) /* TCO Data In Register	*/
#define TCO_DAT_OUT	(TCOBASE + 0x03) /* TCO Data Out Register	*/
#define TCO1_STS	(TCOBASE + 0x04) /* TCO1 Status Register	*/
#define TCO2_STS	(TCOBASE + 0x06) /* TCO2 Status Register	*/
#define TCO1_CNT	(TCOBASE + 0x08) /* TCO1 Control Register	*/
#define TCO2_CNT	(TCOBASE + 0x0a) /* TCO2 Control Register	*/
#define TCOv2_TMR	(TCOBASE + 0x12) /* TCOv2 Timer Initial Value	*/

/* internal variables */
static unsigned long is_active;
static char expect_release;
static struct {		/* this is private data for the iTCO_wdt device */
	/* TCO version/generation */
	unsigned int iTCO_version;
	/* The cards ACPIBASE address (TCOBASE = ACPIBASE+0x60) */
	unsigned long ACPIBASE;
	/* NO_REBOOT flag is Memory-Mapped GCS register bit 5 (TCO version 2)*/
	unsigned long __iomem *gcs;
	/* the lock for io operations */
	spinlock_t io_lock;
	/* the PCI-device */
	struct pci_dev *pdev;
} iTCO_wdt_private;

/* the watchdog platform device */
static struct platform_device *iTCO_wdt_platform_device;

/* module parameters */
#define WATCHDOG_HEARTBEAT 30	/* 30 sec default heartbeat */
static int heartbeat = WATCHDOG_HEARTBEAT;  /* in seconds */
module_param(heartbeat, int, 0);
MODULE_PARM_DESC(heartbeat, "Watchdog timeout in seconds. "
	"5..76 (TCO v1) or 3..614 (TCO v2), default="
				__MODULE_STRING(WATCHDOG_HEARTBEAT) ")");

static int nowayout = WATCHDOG_NOWAYOUT;
module_param(nowayout, int, 0);
MODULE_PARM_DESC(nowayout,
	"Watchdog cannot be stopped once started (default="
				__MODULE_STRING(WATCHDOG_NOWAYOUT) ")");

/*
 * Some TCO specific functions
 */

static inline unsigned int seconds_to_ticks(int seconds)
{
	/* the internal timer is stored as ticks which decrement
	 * every 0.6 seconds */
	return (seconds * 10) / 6;
}

static void iTCO_wdt_set_NO_REBOOT_bit(void)
{
	u32 val32;

	/* Set the NO_REBOOT bit: this disables reboots */
	if (iTCO_wdt_private.iTCO_version == 2) {
		val32 = readl(iTCO_wdt_private.gcs);
		val32 |= 0x00000020;
		writel(val32, iTCO_wdt_private.gcs);
	} else if (iTCO_wdt_private.iTCO_version == 1) {
		pci_read_config_dword(iTCO_wdt_private.pdev, 0xd4, &val32);
		val32 |= 0x00000002;
		pci_write_config_dword(iTCO_wdt_private.pdev, 0xd4, val32);
	}
}

static int iTCO_wdt_unset_NO_REBOOT_bit(void)
{
	int ret = 0;
	u32 val32;

	/* Unset the NO_REBOOT bit: this enables reboots */
	if (iTCO_wdt_private.iTCO_version == 2) {
		val32 = readl(iTCO_wdt_private.gcs);
		val32 &= 0xffffffdf;
		writel(val32, iTCO_wdt_private.gcs);

		val32 = readl(iTCO_wdt_private.gcs);
		if (val32 & 0x00000020)
			ret = -EIO;
	} else if (iTCO_wdt_private.iTCO_version == 1) {
		pci_read_config_dword(iTCO_wdt_private.pdev, 0xd4, &val32);
		val32 &= 0xfffffffd;
		pci_write_config_dword(iTCO_wdt_private.pdev, 0xd4, val32);

		pci_read_config_dword(iTCO_wdt_private.pdev, 0xd4, &val32);
		if (val32 & 0x00000002)
			ret = -EIO;
	}

	return ret; /* returns: 0 = OK, -EIO = Error */
}

static int iTCO_wdt_start(void)
{
	unsigned int val;

	spin_lock(&iTCO_wdt_private.io_lock);

	iTCO_vendor_pre_start(iTCO_wdt_private.ACPIBASE, heartbeat);

	/* disable chipset's NO_REBOOT bit */
	if (iTCO_wdt_unset_NO_REBOOT_bit()) {
		spin_unlock(&iTCO_wdt_private.io_lock);
		printk(KERN_ERR PFX "failed to reset NO_REBOOT flag, "
					"reboot disabled by hardware\n");
		return -EIO;
	}

	/* Force the timer to its reload value by writing to the TCO_RLD
	   register */
	if (iTCO_wdt_private.iTCO_version == 2)
		outw(0x01, TCO_RLD);
	else if (iTCO_wdt_private.iTCO_version == 1)
		outb(0x01, TCO_RLD);

	/* Bit 11: TCO Timer Halt -> 0 = The TCO timer is enabled to count */
	val = inw(TCO1_CNT);
	val &= 0xf7ff;
	outw(val, TCO1_CNT);
	val = inw(TCO1_CNT);
	spin_unlock(&iTCO_wdt_private.io_lock);

	if (val & 0x0800)
		return -1;
	return 0;
}

static int iTCO_wdt_stop(void)
{
	unsigned int val;

	spin_lock(&iTCO_wdt_private.io_lock);

	iTCO_vendor_pre_stop(iTCO_wdt_private.ACPIBASE);

	/* Bit 11: TCO Timer Halt -> 1 = The TCO timer is disabled */
	val = inw(TCO1_CNT);
	val |= 0x0800;
	outw(val, TCO1_CNT);
	val = inw(TCO1_CNT);

	/* Set the NO_REBOOT bit to prevent later reboots, just for sure */
	iTCO_wdt_set_NO_REBOOT_bit();

	spin_unlock(&iTCO_wdt_private.io_lock);

	if ((val & 0x0800) == 0)
		return -1;
	return 0;
}

static int iTCO_wdt_keepalive(void)
{
	spin_lock(&iTCO_wdt_private.io_lock);

	iTCO_vendor_pre_keepalive(iTCO_wdt_private.ACPIBASE, heartbeat);

	/* Reload the timer by writing to the TCO Timer Counter register */
	if (iTCO_wdt_private.iTCO_version == 2)
		outw(0x01, TCO_RLD);
	else if (iTCO_wdt_private.iTCO_version == 1) {
		/* Reset the timeout status bit so that the timer
		 * needs to count down twice again before rebooting */
		outw(0x0008, TCO1_STS);	/* write 1 to clear bit */

		outb(0x01, TCO_RLD);
	}

	spin_unlock(&iTCO_wdt_private.io_lock);
	return 0;
}

static int iTCO_wdt_set_heartbeat(int t)
{
	unsigned int val16;
	unsigned char val8;
	unsigned int tmrval;

	tmrval = seconds_to_ticks(t);

	/* For TCO v1 the timer counts down twice before rebooting */
	if (iTCO_wdt_private.iTCO_version == 1)
		tmrval /= 2;

	/* from the specs: */
	/* "Values of 0h-3h are ignored and should not be attempted" */
	if (tmrval < 0x04)
		return -EINVAL;
	if (((iTCO_wdt_private.iTCO_version == 2) && (tmrval > 0x3ff)) ||
	    ((iTCO_wdt_private.iTCO_version == 1) && (tmrval > 0x03f)))
		return -EINVAL;

	iTCO_vendor_pre_set_heartbeat(tmrval);

	/* Write new heartbeat to watchdog */
	if (iTCO_wdt_private.iTCO_version == 2) {
		spin_lock(&iTCO_wdt_private.io_lock);
		val16 = inw(TCOv2_TMR);
		val16 &= 0xfc00;
		val16 |= tmrval;
		outw(val16, TCOv2_TMR);
		val16 = inw(TCOv2_TMR);
		spin_unlock(&iTCO_wdt_private.io_lock);

		if ((val16 & 0x3ff) != tmrval)
			return -EINVAL;
	} else if (iTCO_wdt_private.iTCO_version == 1) {
		spin_lock(&iTCO_wdt_private.io_lock);
		val8 = inb(TCOv1_TMR);
		val8 &= 0xc0;
		val8 |= (tmrval & 0xff);
		outb(val8, TCOv1_TMR);
		val8 = inb(TCOv1_TMR);
		spin_unlock(&iTCO_wdt_private.io_lock);

		if ((val8 & 0x3f) != tmrval)
			return -EINVAL;
	}

	heartbeat = t;
	return 0;
}

static int iTCO_wdt_get_timeleft(int *time_left)
{
	unsigned int val16;
	unsigned char val8;

	/* read the TCO Timer */
	if (iTCO_wdt_private.iTCO_version == 2) {
		spin_lock(&iTCO_wdt_private.io_lock);
		val16 = inw(TCO_RLD);
		val16 &= 0x3ff;
		spin_unlock(&iTCO_wdt_private.io_lock);

		*time_left = (val16 * 6) / 10;
	} else if (iTCO_wdt_private.iTCO_version == 1) {
		spin_lock(&iTCO_wdt_private.io_lock);
		val8 = inb(TCO_RLD);
		val8 &= 0x3f;
		if (!(inw(TCO1_STS) & 0x0008))
			val8 += (inb(TCOv1_TMR) & 0x3f);
		spin_unlock(&iTCO_wdt_private.io_lock);

		*time_left = (val8 * 6) / 10;
	} else
		return -EINVAL;
	return 0;
}

/*
 *	/dev/watchdog handling
 */

static int iTCO_wdt_open(struct inode *inode, struct file *file)
{
	/* /dev/watchdog can only be opened once */
	if (test_and_set_bit(0, &is_active))
		return -EBUSY;

	/*
	 *      Reload and activate timer
	 */
	iTCO_wdt_start();
	return nonseekable_open(inode, file);
}

static int iTCO_wdt_release(struct inode *inode, struct file *file)
{
	/*
	 *      Shut off the timer.
	 */
	if (expect_release == 42) {
		iTCO_wdt_stop();
	} else {
		printk(KERN_CRIT PFX
			"Unexpected close, not stopping watchdog!\n");
		iTCO_wdt_keepalive();
	}
	clear_bit(0, &is_active);
	expect_release = 0;
	return 0;
}

static ssize_t iTCO_wdt_write(struct file *file, const char __user *data,
			      size_t len, loff_t *ppos)
{
	/* See if we got the magic character 'V' and reload the timer */
	if (len) {
		if (!nowayout) {
			size_t i;

			/* note: just in case someone wrote the magic
			   character five months ago... */
			expect_release = 0;

			/* scan to see whether or not we got the
			   magic character */
			for (i = 0; i != len; i++) {
				char c;
				if (get_user(c, data + i))
					return -EFAULT;
				if (c == 'V')
					expect_release = 42;
			}
		}

		/* someone wrote to us, we should reload the timer */
		iTCO_wdt_keepalive();
	}
	return len;
}

static long iTCO_wdt_ioctl(struct file *file, unsigned int cmd,
							unsigned long arg)
{
	int new_options, retval = -EINVAL;
	int new_heartbeat;
	void __user *argp = (void __user *)arg;
	int __user *p = argp;
	static const struct watchdog_info ident = {
		.options =		WDIOF_SETTIMEOUT |
					WDIOF_KEEPALIVEPING |
					WDIOF_MAGICCLOSE,
		.firmware_version =	0,
		.identity =		DRV_NAME,
	};

	switch (cmd) {
	case WDIOC_GETSUPPORT:
		return copy_to_user(argp, &ident, sizeof(ident)) ? -EFAULT : 0;
	case WDIOC_GETSTATUS:
	case WDIOC_GETBOOTSTATUS:
		return put_user(0, p);

	case WDIOC_SETOPTIONS:
	{
		if (get_user(new_options, p))
			return -EFAULT;

		if (new_options & WDIOS_DISABLECARD) {
			iTCO_wdt_stop();
			retval = 0;
		}
		if (new_options & WDIOS_ENABLECARD) {
			iTCO_wdt_keepalive();
			iTCO_wdt_start();
			retval = 0;
		}
		return retval;
	}
	case WDIOC_KEEPALIVE:
		iTCO_wdt_keepalive();
		return 0;

	case WDIOC_SETTIMEOUT:
	{
		if (get_user(new_heartbeat, p))
			return -EFAULT;
		if (iTCO_wdt_set_heartbeat(new_heartbeat))
			return -EINVAL;
		iTCO_wdt_keepalive();
		/* Fall */
	}
	case WDIOC_GETTIMEOUT:
		return put_user(heartbeat, p);
	case WDIOC_GETTIMELEFT:
	{
		int time_left;
		if (iTCO_wdt_get_timeleft(&time_left))
			return -EINVAL;
		return put_user(time_left, p);
	}
	default:
		return -ENOTTY;
	}
}

/*
 *	Kernel Interfaces
 */

static const struct file_operations iTCO_wdt_fops = {
	.owner =		THIS_MODULE,
	.llseek =		no_llseek,
	.write =		iTCO_wdt_write,
	.unlocked_ioctl =	iTCO_wdt_ioctl,
	.open =			iTCO_wdt_open,
	.release =		iTCO_wdt_release,
};

static struct miscdevice iTCO_wdt_miscdev = {
	.minor =	WATCHDOG_MINOR,
	.name =		"watchdog",
	.fops =		&iTCO_wdt_fops,
};

/*
 *	Init & exit routines
 */

static int __devinit iTCO_wdt_init(struct pci_dev *pdev,
		const struct pci_device_id *ent, struct platform_device *dev)
{
	int ret;
	u32 base_address;
	unsigned long RCBA;
	unsigned long val32;

	/*
	 *      Find the ACPI/PM base I/O address which is the base
	 *      for the TCO registers (TCOBASE=ACPIBASE + 0x60)
	 *      ACPIBASE is bits [15:7] from 0x40-0x43
	 */
	pci_read_config_dword(pdev, 0x40, &base_address);
	base_address &= 0x0000ff80;
	if (base_address == 0x00000000) {
		/* Something's wrong here, ACPIBASE has to be set */
		printk(KERN_ERR PFX "failed to get TCOBASE address\n");
		pci_dev_put(pdev);
		return -ENODEV;
	}
	iTCO_wdt_private.iTCO_version =
			iTCO_chipset_info[ent->driver_data].iTCO_version;
	iTCO_wdt_private.ACPIBASE = base_address;
	iTCO_wdt_private.pdev = pdev;

	/* Get the Memory-Mapped GCS register, we need it for the
	   NO_REBOOT flag (TCO v2). To get access to it you have to
	   read RCBA from PCI Config space 0xf0 and use it as base.
	   GCS = RCBA + ICH6_GCS(0x3410). */
	if (iTCO_wdt_private.iTCO_version == 2) {
		pci_read_config_dword(pdev, 0xf0, &base_address);
		if ((base_address & 1) == 0) {
			printk(KERN_ERR PFX "RCBA is disabled by hardware\n");
			ret = -ENODEV;
			goto out;
		}
		RCBA = base_address & 0xffffc000;
		iTCO_wdt_private.gcs = ioremap((RCBA + 0x3410), 4);
	}

	/* Check chipset's NO_REBOOT bit */
	if (iTCO_wdt_unset_NO_REBOOT_bit() && iTCO_vendor_check_noreboot_on()) {
		printk(KERN_INFO PFX "unable to reset NO_REBOOT flag, "
					"platform may have disabled it\n");
		ret = -ENODEV;	/* Cannot reset NO_REBOOT bit */
		goto out_unmap;
	}

	/* Set the NO_REBOOT bit to prevent later reboots, just for sure */
	iTCO_wdt_set_NO_REBOOT_bit();

	/* The TCO logic uses the TCO_EN bit in the SMI_EN register */
	if (!request_region(SMI_EN, 4, "iTCO_wdt")) {
		printk(KERN_ERR PFX
			"I/O address 0x%04lx already in use\n", SMI_EN);
		ret = -EIO;
		goto out_unmap;
	}
	/* Bit 13: TCO_EN -> 0 = Disables TCO logic generating an SMI# */
	val32 = inl(SMI_EN);
	val32 &= 0xffffdfff;	/* Turn off SMI clearing watchdog */
	outl(val32, SMI_EN);

	/* The TCO I/O registers reside in a 32-byte range pointed to
	   by the TCOBASE value */
	if (!request_region(TCOBASE, 0x20, "iTCO_wdt")) {
		printk(KERN_ERR PFX "I/O address 0x%04lx already in use\n",
			TCOBASE);
		ret = -EIO;
		goto unreg_smi_en;
	}

	printk(KERN_INFO PFX
		"Found a %s TCO device (Version=%d, TCOBASE=0x%04lx)\n",
			iTCO_chipset_info[ent->driver_data].name,
			iTCO_chipset_info[ent->driver_data].iTCO_version,
			TCOBASE);

	/* Clear out the (probably old) status */
	outw(0x0008, TCO1_STS);	/* Clear the Time Out Status bit */
	outw(0x0002, TCO2_STS);	/* Clear SECOND_TO_STS bit */
	outw(0x0004, TCO2_STS);	/* Clear BOOT_STS bit */

	/* Make sure the watchdog is not running */
	iTCO_wdt_stop();

	/* Check that the heartbeat value is within it's range;
	   if not reset to the default */
	if (iTCO_wdt_set_heartbeat(heartbeat)) {
		iTCO_wdt_set_heartbeat(WATCHDOG_HEARTBEAT);
		printk(KERN_INFO PFX
			"timeout value out of range, using %d\n", heartbeat);
	}

	ret = misc_register(&iTCO_wdt_miscdev);
	if (ret != 0) {
		printk(KERN_ERR PFX
			"cannot register miscdev on minor=%d (err=%d)\n",
							WATCHDOG_MINOR, ret);
		goto unreg_region;
	}

	printk(KERN_INFO PFX "initialized. heartbeat=%d sec (nowayout=%d)\n",
							heartbeat, nowayout);

	return 0;

unreg_region:
	release_region(TCOBASE, 0x20);
unreg_smi_en:
	release_region(SMI_EN, 4);
out_unmap:
	if (iTCO_wdt_private.iTCO_version == 2)
		iounmap(iTCO_wdt_private.gcs);
out:
	pci_dev_put(iTCO_wdt_private.pdev);
	iTCO_wdt_private.ACPIBASE = 0;
	return ret;
}

static void __devexit iTCO_wdt_cleanup(void)
{
	/* Stop the timer before we leave */
	if (!nowayout)
		iTCO_wdt_stop();

	/* Deregister */
	misc_deregister(&iTCO_wdt_miscdev);
	release_region(TCOBASE, 0x20);
	release_region(SMI_EN, 4);
	if (iTCO_wdt_private.iTCO_version == 2)
		iounmap(iTCO_wdt_private.gcs);
	pci_dev_put(iTCO_wdt_private.pdev);
	iTCO_wdt_private.ACPIBASE = 0;
}

static int __devinit iTCO_wdt_probe(struct platform_device *dev)
{
	int ret = -ENODEV;
	int found = 0;
	struct pci_dev *pdev = NULL;
	const struct pci_device_id *ent;

	spin_lock_init(&iTCO_wdt_private.io_lock);

	for_each_pci_dev(pdev) {
		ent = pci_match_id(iTCO_wdt_pci_tbl, pdev);
		if (ent) {
			found++;
			ret = iTCO_wdt_init(pdev, ent, dev);
			if (!ret)
				break;
		}
	}

	if (!found)
		printk(KERN_INFO PFX "No card detected\n");

	return ret;
}

static int __devexit iTCO_wdt_remove(struct platform_device *dev)
{
	if (iTCO_wdt_private.ACPIBASE)
		iTCO_wdt_cleanup();

	return 0;
}

static void iTCO_wdt_shutdown(struct platform_device *dev)
{
	iTCO_wdt_stop();
}

#define iTCO_wdt_suspend NULL
#define iTCO_wdt_resume  NULL

static struct platform_driver iTCO_wdt_driver = {
	.probe          = iTCO_wdt_probe,
	.remove         = __devexit_p(iTCO_wdt_remove),
	.shutdown       = iTCO_wdt_shutdown,
	.suspend        = iTCO_wdt_suspend,
	.resume         = iTCO_wdt_resume,
	.driver         = {
		.owner  = THIS_MODULE,
		.name   = DRV_NAME,
	},
};

static int __init iTCO_wdt_init_module(void)
{
	int err;

	printk(KERN_INFO PFX "Intel TCO WatchDog Timer Driver v%s\n",
		DRV_VERSION);

	err = platform_driver_register(&iTCO_wdt_driver);
	if (err)
		return err;

	iTCO_wdt_platform_device = platform_device_register_simple(DRV_NAME,
								-1, NULL, 0);
	if (IS_ERR(iTCO_wdt_platform_device)) {
		err = PTR_ERR(iTCO_wdt_platform_device);
		goto unreg_platform_driver;
	}

	return 0;

unreg_platform_driver:
	platform_driver_unregister(&iTCO_wdt_driver);
	return err;
}

static void __exit iTCO_wdt_cleanup_module(void)
{
	platform_device_unregister(iTCO_wdt_platform_device);
	platform_driver_unregister(&iTCO_wdt_driver);
	printk(KERN_INFO PFX "Watchdog Module Unloaded.\n");
}

module_init(iTCO_wdt_init_module);
module_exit(iTCO_wdt_cleanup_module);

MODULE_AUTHOR("Wim Van Sebroeck <wim@iguana.be>");
MODULE_DESCRIPTION("Intel TCO WatchDog Timer Driver");
MODULE_VERSION(DRV_VERSION);
MODULE_LICENSE("GPL");
MODULE_ALIAS_MISCDEV(WATCHDOG_MINOR);
