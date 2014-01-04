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
}

void
board_clock_init(si_t *sih)
{
	uint32 armclock = 0, ddrclock = 0, axiclock = 0;
	char *nvstr;
	char *end;

	/* ARM clock speed override */
	if ((nvstr = nvram_get("clkfreq"))) {
		armclock = bcm_strtoul(nvstr, &end, 0) * 1000000;
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
	/* Empty function for future extensions */
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
