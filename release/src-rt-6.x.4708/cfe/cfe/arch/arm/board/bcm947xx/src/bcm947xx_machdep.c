/*
 * Broadcom Common Firmware Environment (CFE)
 * Architecure dependent initialization,
 * File: bcm947xx_machdep.c
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id$
 */

#include "cfe_error.h"

#include <typedefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <hndsoc.h>
#include <siutils.h>
#include <sbchipc.h>
#include <bcmdevs.h>
#include <bcmnvram.h>
#include <hndcpu.h>
#include <hndchipc.h>
#include "bsp_priv.h"
#include <chipcommonb.h>

extern bool si_arm_setclock(si_t *sih, uint32 armclock, uint32 ddrclock, uint32 axiclock);
extern int cpu_turbo_mode;


#ifdef DSLAC68U

#define	WAN_LED_GPIO	(1 << 0)	// GPIO 0
#define	PWR_LED_GPIO	(1 << 3)	// GPIO 3
#define	WL5G_LED_GPIO	(1 << 6)	// GPIO 6
#define USB_PWR1_GPIO	(1 << 9)	// GPIO 9
#define	USB3_LED_GPIO	(1 << 14)	// GPIO 14

#else	//not DSLAC68U

#ifdef RTAC87U
#define	PWR_LED_GPIO	(1 << 3)	// GPIO 3
#define	USB_LED_GPIO	(1 << 0)	// GPIO 0
#define	TURBO_LED_GPIO	(1 << 4)	// GPIO 4
#define	USB3_LED_GPIO	(1 << 14)	// GPIO 14
#define USB_PWR1_GPIO	(1 << 9)	// GPIO 9
#else	/* not RTAC87U */


#ifndef RTN18U
#define	PWR_LED_GPIO	(1 << 3)	// GPIO 3
#else	/* RT-N18U */
#define	PWR_LED_GPIO	(1 << 0)	// GPIO 0
#endif
#ifndef RTN18U
#define	WL5G_LED_GPIO	(1 << 6)	// GPIO 6
#endif
#ifdef RTAC68U
#define	USB_LED_GPIO	(1 << 0)	// GPIO 0
#define	TURBO_LED_GPIO	(1 << 4)	// GPIO 4
#define	USB3_LED_GPIO	(1 << 14)	// GPIO 14
#else
#ifndef RTN18U	/* RT-AC56U */
#define	USB3_LED_GPIO	(1 << 0)	// GPIO 0
#define	WAN_LED_GPIO	(1 << 1)	// GPIO 1
#define	LAN_LED_GPIO	(1 << 2)	// GPIO 2
#define	USB_LED_GPIO	(1 << 14)	// GPIO 14
#else	/* RT-N18U */
#define	WAN_LED_GPIO	(1 << 6)	// GPIO 6
#define	LAN_LED_GPIO	(1 << 9)	// GPIO 9
#endif
#endif

#ifndef RTN18U				// for RT-AC56U & RT-AC68U
#define USB_PWR1_GPIO	(1 << 9)	// GPIO 9
#ifndef RTAC68U
#define USB_PWR2_GPIO	(1 << 10)	// GPIO 10
#endif
#endif

#ifdef RTN18U				// RT-N18U
#define	WL2G_LED_GPIO	(1 << 10)	// GPIO 10
#define	USB_LED_GPIO	(1 << 3)	// GPIO 3
#define	USB3_LED_GPIO	(1 << 14)	// GPIO 14
#define USB_PWR1_GPIO	(1 << 13)	// GPIO 13
#endif

#endif	/* end of RTAC87U */

#endif	// end of DSLAC68U

void
board_pinmux_init(si_t *sih)
{
	uint origidx;
	chipcommonbregs_t *chipcb;

	origidx = si_coreidx(sih);
	chipcb = si_setcore(sih, NS_CCB_CORE_ID, 0);
	if (chipcb != NULL) {
		/* Default select the mux pins for GPIO */
		W_REG(osh, &chipcb->cru_gpio_control0, 0x1fffff);
	}
	si_setcoreidx(sih, origidx);
#ifdef DSLAC68U
	si_gpioouten(sih, PWR_LED_GPIO, PWR_LED_GPIO, GPIO_DRV_PRIORITY);
	si_gpioouten(sih, WL5G_LED_GPIO, WL5G_LED_GPIO, GPIO_DRV_PRIORITY);
	si_gpioouten(sih, USB3_LED_GPIO, USB3_LED_GPIO, GPIO_DRV_PRIORITY);
	si_gpioouten(sih, WAN_LED_GPIO, WAN_LED_GPIO, GPIO_DRV_PRIORITY);
	si_gpioouten(sih, USB_PWR1_GPIO, USB_PWR1_GPIO, GPIO_DRV_PRIORITY);
	si_gpioout(sih, PWR_LED_GPIO, 0, GPIO_DRV_PRIORITY);
	si_gpioout(sih, WL5G_LED_GPIO, WL5G_LED_GPIO, GPIO_DRV_PRIORITY);
	si_gpioout(sih, USB3_LED_GPIO, USB3_LED_GPIO, GPIO_DRV_PRIORITY);
	si_gpioout(sih, WAN_LED_GPIO, WAN_LED_GPIO, GPIO_DRV_PRIORITY);
	si_gpioout(sih, USB_PWR1_GPIO, USB_PWR1_GPIO, GPIO_DRV_PRIORITY);
#else

	si_gpioouten(sih, PWR_LED_GPIO, PWR_LED_GPIO, GPIO_DRV_PRIORITY);
	si_gpioouten(sih, USB_LED_GPIO, USB_LED_GPIO, GPIO_DRV_PRIORITY);
#ifdef RTAC68U
	si_gpioouten(sih, TURBO_LED_GPIO, TURBO_LED_GPIO, GPIO_DRV_PRIORITY);
#endif
#ifndef RTN18U
#ifndef RTAC87U
	si_gpioouten(sih, WL5G_LED_GPIO, WL5G_LED_GPIO, GPIO_DRV_PRIORITY);
#endif
	si_gpioouten(sih, USB3_LED_GPIO, USB3_LED_GPIO, GPIO_DRV_PRIORITY);
#endif
#if !defined(RTAC68U) && !defined(RTAC87U)
	si_gpioouten(sih, WAN_LED_GPIO, WAN_LED_GPIO, GPIO_DRV_PRIORITY);
	si_gpioouten(sih, LAN_LED_GPIO, LAN_LED_GPIO, GPIO_DRV_PRIORITY);
#endif
#ifndef RTN18U				// for RT-AC56U & RT-AC68U
	si_gpioouten(sih, USB_PWR1_GPIO, USB_PWR1_GPIO, GPIO_DRV_PRIORITY);
#if !defined(RTAC68U) && !defined(RTAC87U)
	si_gpioouten(sih, USB_PWR2_GPIO, USB_PWR2_GPIO, GPIO_DRV_PRIORITY);
#endif
#endif
#ifdef RTN18U				// RT-N18U
	si_gpioouten(sih, USB_PWR1_GPIO, USB_PWR1_GPIO, GPIO_DRV_PRIORITY);
	si_gpioouten(sih, WL2G_LED_GPIO, WL2G_LED_GPIO, GPIO_DRV_PRIORITY);
	si_gpioouten(sih, USB3_LED_GPIO, USB3_LED_GPIO, GPIO_DRV_PRIORITY);
#endif

	si_gpioout(sih, PWR_LED_GPIO, 0, GPIO_DRV_PRIORITY);
	si_gpioout(sih, USB_LED_GPIO, USB_LED_GPIO, GPIO_DRV_PRIORITY);
#ifdef RTAC68U
	si_gpioout(sih, TURBO_LED_GPIO, 0, GPIO_DRV_PRIORITY);
#endif
#ifndef RTN18U				// for RT-AC56U & RT-AC68U to enable USB power
#ifndef RTAC87U
	si_gpioout(sih, WL5G_LED_GPIO, WL5G_LED_GPIO, GPIO_DRV_PRIORITY);
#endif
	si_gpioout(sih, USB3_LED_GPIO, USB3_LED_GPIO, GPIO_DRV_PRIORITY);
#endif
#if !defined(RTAC68U) && !defined(RTAC87U)
	si_gpioout(sih, WAN_LED_GPIO, WAN_LED_GPIO, GPIO_DRV_PRIORITY);
	si_gpioout(sih, LAN_LED_GPIO, LAN_LED_GPIO, GPIO_DRV_PRIORITY);
#endif
#ifndef RTN18U				// for RT-AC56U & RT-AC68U to enable USB power
	si_gpioout(sih, USB_PWR1_GPIO, USB_PWR1_GPIO, GPIO_DRV_PRIORITY);
#if !defined(RTAC68U) && !defined(RTAC87U)
	si_gpioout(sih, USB_PWR2_GPIO, USB_PWR2_GPIO, GPIO_DRV_PRIORITY);
#endif
#endif

#endif	//DSLAC68U

#ifdef RTN18U				// RT-N18U
	/* enable USB power */
	si_gpioout(sih, USB_PWR1_GPIO, USB_PWR1_GPIO, GPIO_DRV_PRIORITY);

	/* power on LEDs */
	si_gpioout(sih, PWR_LED_GPIO, PWR_LED_GPIO, GPIO_DRV_PRIORITY);
	si_gpioout(sih, WL2G_LED_GPIO, WL2G_LED_GPIO, GPIO_DRV_PRIORITY);
	si_gpioout(sih, WAN_LED_GPIO, WAN_LED_GPIO, GPIO_DRV_PRIORITY);
	si_gpioout(sih, LAN_LED_GPIO, LAN_LED_GPIO, GPIO_DRV_PRIORITY);
	si_gpioout(sih, USB_LED_GPIO, USB_LED_GPIO, GPIO_DRV_PRIORITY);
	si_gpioout(sih, USB3_LED_GPIO, USB3_LED_GPIO, GPIO_DRV_PRIORITY);
#endif

}

void
board_clock_init(si_t *sih)
{
	uint32 armclock = 0, ddrclock = 0, axiclock = 0;
	char *nvstr;
	char *end;

	if (cpu_turbo_mode)
	{
		printf("CPU Turbo Mode\n");
		nvstr = strdup("1000,533");
	}
	else
		nvstr = nvram_safe_get("clkfreq");

	/* ARM clock speed override */
	if (nvstr) {
		printf("clkfreq: %s\n", nvstr);
		armclock = bcm_strtoul(nvstr, &end, 0) * 1000000;

		if (cpu_turbo_mode)
			KFREE(nvstr);

		if (*end == ',') {
			nvstr = ++end;
			ddrclock = bcm_strtoul(nvstr, &end, 0) * 1000000;
			if (*end == ',') {
				nvstr = ++end;
				axiclock = bcm_strtoul(nvstr, &end, 0) * 1000000;
			}
		}
	}

	if (!armclock)
		armclock = 800 * 1000000;

	/* To accommodate old sdram_ncdl usage to store DDR clock;
	 * should be removed if sdram_ncdl is used for some other purpose.
	 */
	if ((nvstr = nvram_get("sdram_ncdl"))) {
		uint32 ncdl = bcm_strtoul(nvstr, NULL, 0);
		if (ncdl && (ncdl * 1000000) != ddrclock) {
			ddrclock = ncdl * 1000000;
		}
	}

	/* Set current ARM clock speed */
	si_arm_setclock(sih, armclock, ddrclock, axiclock);

	/* Update cfe_cpu_speed */
	board_cfe_cpu_speed_upd(sih);
}

void
mach_device_init(si_t *sih)
{
	uint32 armclock = 0, ddrclock = 0;
	char *nvstr;
	char *end;
	uint32 ncdl = 0;
	uint32 clock;

	/* ARM clock speed override */
	if ((nvstr = nvram_get("clkfreq"))) {
		armclock = bcm_strtoul(nvstr, &end, 0);
		if (*end == ',') {
			nvstr = ++end;
			ddrclock = bcm_strtoul(nvstr, &end, 0);
		}
	}

	/* To accommodate old sdram_ncdl usage to store DDR clock;
	 * should be removed if sdram_ncdl is used for some other purpose.
	 */
	clock = si_mem_clock(sih) / 1000000;
	printf("DDR Clock: %u MHz\n", clock);
	if ((nvstr = nvram_get("sdram_ncdl"))) {
		ncdl = bcm_strtoul(nvstr, NULL, 0);
		if (ncdl && ncdl != ddrclock) {
			printf("Warning: using legacy sdram_ncdl parameter to set DDR frequency. "
				"Equivalent setting in clkfreq=%u,*%u* will be ignored.\n",
				armclock, ddrclock);
			if (ncdl != 0 && ncdl != clock)
				printf("Warning: invalid DDR setting of %u MHz ignored. "
					"DDR frequency will be set to %u MHz.\n", ncdl, clock);
			goto out;
		}
	}

	if (ddrclock)
		printf("Info: DDR frequency set from clkfreq=%u,*%u*\n", armclock, ddrclock);
	if (ddrclock != clock)
		printf("Warning: invalid DDR setting of %u MHz ignored. "
			"DDR frequency will be set to %u MHz.\n", ddrclock, clock);

out:
	return;
}

void
board_power_init(si_t *sih)
{
        uint origidx;
        chipcregs_t *cc;
        int gpio;

        /* Drive gpio pin to HIGH to enable on-board switching regulator for COMA mode */
        origidx = si_coreidx(sih);
        if ((cc = (chipcregs_t *)si_setcore(sih, CC_CORE_ID, 0)) != NULL) {
                gpio = getgpiopin(NULL, "coma_swreg", GPIO_PIN_NOTDEFINED);
                if (gpio != GPIO_PIN_NOTDEFINED) {
                        gpio = 1 << gpio;
                        si_gpioout(sih, gpio, gpio, GPIO_DRV_PRIORITY);
                        si_gpioouten(sih, gpio, gpio, GPIO_DRV_PRIORITY);
                }
        }
        si_setcoreidx(sih, origidx);
}

void
board_cpu_init(si_t *sih)
{
	/* Initialize clocks and interrupts */
	si_arm_init(sih);
}

/* returns the ncdl value to be programmed into sdram_ncdl for calibration */
uint32
si_memc_get_ncdl(si_t *sih)
{
	return 0;
}

void
flash_memory_size_config(si_t *sih, void *probe)
{
}


void
board_cfe_cpu_speed_upd(si_t *sih)
{
	/* Update current CPU clock speed */
	if ((cfe_cpu_speed = si_cpu_clock(sih)) == 0) {
		cfe_cpu_speed = 800000000; /* 800 MHz */
	}
}
