/*
 *	intel TCO vendor specific watchdog driver support
 *
 *	(c) Copyright 2006 Wim Van Sebroeck <wim@iguana.be>.
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 *
 *	Neither Wim Van Sebroeck nor Iguana vzw. admit liability nor
 *	provide warranty for any of this software. This material is
 *	provided "AS-IS" and at no charge.
 */

/*
 *	Includes, defines, variables, module parameters, ...
 */

/* Module and version information */
#define DRV_NAME        "iTCO_vendor_support"
#define DRV_VERSION     "1.01"
#define DRV_RELDATE     "11-Nov-2006"
#define PFX		DRV_NAME ": "

/* Includes */
#include <linux/module.h>		/* For module specific items */
#include <linux/moduleparam.h>		/* For new moduleparam's */
#include <linux/types.h>		/* For standard types (like size_t) */
#include <linux/errno.h>		/* For the -ENODEV/... values */
#include <linux/kernel.h>		/* For printk/panic/... */
#include <linux/init.h>			/* For __init/__exit/... */
#include <linux/ioport.h>		/* For io-port access */

#include <asm/io.h>			/* For inb/outb/... */

/* iTCO defines */
#define	SMI_EN		acpibase + 0x30	/* SMI Control and Enable Register */
#define	TCOBASE		acpibase + 0x60	/* TCO base address		*/
#define	TCO1_STS	TCOBASE + 0x04	/* TCO1 Status Register		*/

/* List of vendor support modes */
#define SUPERMICRO_OLD_BOARD	1	/* SuperMicro Pentium 3 Era 370SSE+-OEM1/P3TSSE */
#define SUPERMICRO_NEW_BOARD	2	/* SuperMicro Pentium 4 / Xeon 4 / EMT64T Era Systems */

static int vendorsupport = 0;
module_param(vendorsupport, int, 0);
MODULE_PARM_DESC(vendorsupport, "iTCO vendor specific support mode, default=0 (none), 1=SuperMicro Pent3, 2=SuperMicro Pent4+");

/*
 *	Vendor Specific Support
 */

/*
 *	Vendor Support: 1
 *	Board: Super Micro Computer Inc. 370SSE+-OEM1/P3TSSE
 *	iTCO chipset: ICH2
 *
 *	Code contributed by: R. Seretny <lkpatches@paypc.com>
 *	Documentation obtained by R. Seretny from SuperMicro Technical Support
 *
 *	To enable Watchdog function:
 *	    BIOS setup -> Power -> TCO Logic SMI Enable -> Within5Minutes
 *	    This setting enables SMI to clear the watchdog expired flag.
 *	    If BIOS or CPU fail which may cause SMI hang, then system will
 *	    reboot. When application starts to use watchdog function,
 *	    application has to take over the control from SMI.
 *
 *	    For P3TSSE, J36 jumper needs to be removed to enable the Watchdog
 *	    function.
 *
 *	    Note: The system will reboot when Expire Flag is set TWICE.
 *	    So, if the watchdog timer is 20 seconds, then the maximum hang
 *	    time is about 40 seconds, and the minimum hang time is about
 *	    20.6 seconds.
 */

static void supermicro_old_pre_start(unsigned long acpibase)
{
	unsigned long val32;

	val32 = inl(SMI_EN);
	val32 &= 0xffffdfff;	/* Turn off SMI clearing watchdog */
	outl(val32, SMI_EN);	/* Needed to activate watchdog */
}

static void supermicro_old_pre_stop(unsigned long acpibase)
{
	unsigned long val32;

	val32 = inl(SMI_EN);
	val32 &= 0x00002000;	/* Turn on SMI clearing watchdog */
	outl(val32, SMI_EN);	/* Needed to deactivate watchdog */
}

static void supermicro_old_pre_keepalive(unsigned long acpibase)
{
	/* Reload TCO Timer (done in iTCO_wdt_keepalive) + */
	/* Clear "Expire Flag" (Bit 3 of TC01_STS register) */
	outb(0x08, TCO1_STS);
}

/*
 *	Vendor Support: 2
 *	Board: Super Micro Computer Inc. P4SBx, P4DPx
 *	iTCO chipset: ICH4
 *
 *	Code contributed by: R. Seretny <lkpatches@paypc.com>
 *	Documentation obtained by R. Seretny from SuperMicro Technical Support
 *
 *	To enable Watchdog function:
 *	 1. BIOS
 *	  For P4SBx:
 *	  BIOS setup -> Advanced -> Integrated Peripherals -> Watch Dog Feature
 *	  For P4DPx:
 *	  BIOS setup -> Advanced -> I/O Device Configuration -> Watch Dog
 *	 This setting enables or disables Watchdog function. When enabled, the
 *	 default watchdog timer is set to be 5 minutes (about 4’35”). It is
 *	 enough to load and run the OS. The application (service or driver) has
 *	 to take over the control once OS is running up and before watchdog
 *	 expires.
 *
 *	 2. JUMPER
 *	  For P4SBx: JP39
 *	  For P4DPx: JP37
 *	  This jumper is used for safety.  Closed is enabled. This jumper
 *	  prevents user enables watchdog in BIOS by accident.
 *
 *	 To enable Watch Dog function, both BIOS and JUMPER must be enabled.
 *
 *	The documentation lists motherboards P4SBx and P4DPx series as of
 *	20-March-2002. However, this code works flawlessly with much newer
 *	motherboards, such as my X6DHR-8G2 (SuperServer 6014H-82).
 *
 *	The original iTCO driver as written does not actually reset the
 *	watchdog timer on these machines, as a result they reboot after five
 *	minutes.
 *
 *	NOTE: You may leave the Watchdog function disabled in the SuperMicro
 *	BIOS to avoid a "boot-race"... This driver will enable watchdog
 *	functionality even if it's disabled in the BIOS once the /dev/watchdog
 *	file is opened.
 */

/* I/O Port's */
#define SM_REGINDEX	0x2e		/* SuperMicro ICH4+ Register Index */
#define SM_DATAIO	0x2f		/* SuperMicro ICH4+ Register Data I/O */

/* Control Register's */
#define SM_CTLPAGESW	0x07		/* SuperMicro ICH4+ Control Page Switch */
#define SM_CTLPAGE		0x08	/* SuperMicro ICH4+ Control Page Num */

#define SM_WATCHENABLE	0x30		/* Watchdog enable: Bit 0: 0=off, 1=on */

#define SM_WATCHPAGE	0x87		/* Watchdog unlock control page */

#define SM_ENDWATCH	0xAA		/* Watchdog lock control page */

#define SM_COUNTMODE	0xf5		/* Watchdog count mode select */
					/* (Bit 3: 0 = seconds, 1 = minutes */

#define SM_WATCHTIMER	0xf6		/* 8-bits, Watchdog timer counter (RW) */

#define SM_RESETCONTROL	0xf7		/* Watchdog reset control */
					/* Bit 6: timer is reset by kbd interrupt */
					/* Bit 7: timer is reset by mouse interrupt */

static void supermicro_new_unlock_watchdog(void)
{
	outb(SM_WATCHPAGE, SM_REGINDEX);	/* Write 0x87 to port 0x2e twice */
	outb(SM_WATCHPAGE, SM_REGINDEX);

	outb(SM_CTLPAGESW, SM_REGINDEX);	/* Switch to watchdog control page */
	outb(SM_CTLPAGE, SM_DATAIO);
}

static void supermicro_new_lock_watchdog(void)
{
	outb(SM_ENDWATCH, SM_REGINDEX);
}

static void supermicro_new_pre_start(unsigned int heartbeat)
{
	unsigned int val;

	supermicro_new_unlock_watchdog();

	/* Watchdog timer setting needs to be in seconds*/
	outb(SM_COUNTMODE, SM_REGINDEX);
	val = inb(SM_DATAIO);
	val &= 0xF7;
	outb(val, SM_DATAIO);

	/* Write heartbeat interval to WDOG */
	outb (SM_WATCHTIMER, SM_REGINDEX);
	outb((heartbeat & 255), SM_DATAIO);

	/* Make sure keyboard/mouse interrupts don't interfere */
	outb(SM_RESETCONTROL, SM_REGINDEX);
	val = inb(SM_DATAIO);
	val &= 0x3f;
	outb(val, SM_DATAIO);

	/* enable watchdog by setting bit 0 of Watchdog Enable to 1 */
	outb(SM_WATCHENABLE, SM_REGINDEX);
	val = inb(SM_DATAIO);
	val |= 0x01;
	outb(val, SM_DATAIO);

	supermicro_new_lock_watchdog();
}

static void supermicro_new_pre_stop(void)
{
	unsigned int val;

	supermicro_new_unlock_watchdog();

	/* disable watchdog by setting bit 0 of Watchdog Enable to 0 */
	outb(SM_WATCHENABLE, SM_REGINDEX);
	val = inb(SM_DATAIO);
	val &= 0xFE;
	outb(val, SM_DATAIO);

	supermicro_new_lock_watchdog();
}

static void supermicro_new_pre_set_heartbeat(unsigned int heartbeat)
{
	supermicro_new_unlock_watchdog();

	/* reset watchdog timeout to heartveat value */
	outb(SM_WATCHTIMER, SM_REGINDEX);
	outb((heartbeat & 255), SM_DATAIO);

	supermicro_new_lock_watchdog();
}

/*
 *	Generic Support Functions
 */

void iTCO_vendor_pre_start(unsigned long acpibase,
			   unsigned int heartbeat)
{
	if (vendorsupport == SUPERMICRO_OLD_BOARD)
		supermicro_old_pre_start(acpibase);
	else if (vendorsupport == SUPERMICRO_NEW_BOARD)
		supermicro_new_pre_start(heartbeat);
}
EXPORT_SYMBOL(iTCO_vendor_pre_start);

void iTCO_vendor_pre_stop(unsigned long acpibase)
{
	if (vendorsupport == SUPERMICRO_OLD_BOARD)
		supermicro_old_pre_stop(acpibase);
	else if (vendorsupport == SUPERMICRO_NEW_BOARD)
		supermicro_new_pre_stop();
}
EXPORT_SYMBOL(iTCO_vendor_pre_stop);

void iTCO_vendor_pre_keepalive(unsigned long acpibase, unsigned int heartbeat)
{
	if (vendorsupport == SUPERMICRO_OLD_BOARD)
		supermicro_old_pre_keepalive(acpibase);
	else if (vendorsupport == SUPERMICRO_NEW_BOARD)
		supermicro_new_pre_set_heartbeat(heartbeat);
}
EXPORT_SYMBOL(iTCO_vendor_pre_keepalive);

void iTCO_vendor_pre_set_heartbeat(unsigned int heartbeat)
{
	if (vendorsupport == SUPERMICRO_NEW_BOARD)
		supermicro_new_pre_set_heartbeat(heartbeat);
}
EXPORT_SYMBOL(iTCO_vendor_pre_set_heartbeat);

int iTCO_vendor_check_noreboot_on(void)
{
	switch(vendorsupport) {
	case SUPERMICRO_OLD_BOARD:
		return 0;
	default:
		return 1;
	}
}
EXPORT_SYMBOL(iTCO_vendor_check_noreboot_on);

static int __init iTCO_vendor_init_module(void)
{
	printk (KERN_INFO PFX "vendor-support=%d\n", vendorsupport);
	return 0;
}

static void __exit iTCO_vendor_exit_module(void)
{
	printk (KERN_INFO PFX "Module Unloaded\n");
}

module_init(iTCO_vendor_init_module);
module_exit(iTCO_vendor_exit_module);

MODULE_AUTHOR("Wim Van Sebroeck <wim@iguana.be>, R. Seretny <lkpatches@paypc.com>");
MODULE_DESCRIPTION("Intel TCO Vendor Specific WatchDog Timer Driver Support");
MODULE_VERSION(DRV_VERSION);
MODULE_LICENSE("GPL");

