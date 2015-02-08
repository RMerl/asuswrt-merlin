/*
 * Broadcom Common Firmware Environment (CFE)
 * Architecure dependent initialization,
 * File: bcm947xx_machdep.c
 *
 * Copyright (C) 2014, Broadcom Corporation
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
#include "dev_newflash.h"
#include "bsp_priv.h"



void
board_pinmux_init(si_t *sih)
{
	/* No pin muxing initialization required for now. */
}

void
board_clock_init(si_t *sih)
{
	uint32 mipsclock = 0, siclock = 0, pciclock = 0;
	char *nvstr;
	char *end;

	/* MIPS clock speed override */
	if ((nvstr = nvram_get("clkfreq"))) {
		mipsclock = bcm_strtoul(nvstr, &end, 0) * 1000000;
		if (*end == ',') {
			nvstr = ++end;
			siclock = bcm_strtoul(nvstr, &end, 0) * 1000000;
			if (*end == ',') {
				nvstr = ++end;
				pciclock = bcm_strtoul(nvstr, &end, 0) * 1000000;
			}
		}
	}

	if (mipsclock) {
		/* Set current MIPS clock speed */
		si_mips_setclock(sih, mipsclock, siclock, pciclock);

		/* Update cfe_cpu_speed */
		board_cfe_cpu_speed_upd(sih);
	}
}

void
mach_device_init(si_t *sih)
{

}

void
board_power_init(si_t *sih)
{
	char	*nvstr;
	uint	origidx;
	chipcregs_t *cc;

	/* Next sections all want to talk to chipcommon */
	origidx = si_coreidx(sih);
	cc = si_setcoreidx(sih, SI_CC_IDX);

	nvstr = nvram_get("boardpwrctl");

	if ((CHIPID(sih->chip) == BCM4716_CHIP_ID) ||
	    (CHIPID(sih->chip) == BCM4748_CHIP_ID) ||
	    (CHIPID(sih->chip) == BCM47162_CHIP_ID)) {
		uint32 reg, new;

		/* Adjust regulator settings */
		W_REG(si_osh(sih), &cc->regcontrol_addr, 2);
		/* Readback to ensure completion of the write */
		(void)R_REG(si_osh(sih), &cc->regcontrol_addr);
		reg = R_REG(si_osh(sih), &cc->regcontrol_data);
		/* Make the regulator frequency to 1.2MHz
		 *   00 1.2MHz
		 *   01 200kHz
		 *   10 600kHz
		 *   11 2.4MHz
		 */
		reg &= ~0x00c00000;
		/* Take 2.5v regulator output down one notch,
		 * officially to 2.45, but in reality to be
		 * closer to 2.5 than the default.
		 */
		reg |= 0xf0000000;

		/* Bits corresponding to mask 0x00078000
		 * controls 1.3v source
		 *	Value           Voltage
	         *	========================
	         *	0xf0000000  1.2 V (default)
		 * 	0xf0008000  0.975
		 *	0xf0010000  1
		 *	0xf0018000  1.025
		 *	0xf0020000  1.05
		 *	0xf0028000  1.075
		 *	0xf0030000  1.1
		 *	0xf0038000  1.125
		 *	0xf0040000  1.15
		 *	0xf0048000  1.175
		 *	0xf0050000  1.225
		 *	0xf0058000  1.25
		 *	0xf0060000  1.275
		 *	0xf0068000  1.3
		 *	0xf0070000  1.325
		 *	0xf0078000  1.35
		 */
		if (nvstr) {
			uint32 pwrctl = bcm_strtoul(nvstr, NULL, 0);

			reg &= ~0xf0c78000;
			reg |= (pwrctl & 0xf0c78000);
		}
		W_REG(si_osh(sih), &cc->regcontrol_data, reg);

		/* Turn off unused PLLs */
		W_REG(si_osh(sih), &cc->pllcontrol_addr, 6);
		(void)R_REG(si_osh(sih), &cc->pllcontrol_addr);
		new = reg = R_REG(si_osh(sih), &cc->pllcontrol_data);
		if (sih->chippkg == BCM4716_PKG_ID)
			new |= 0x68;	/* Channels 3, 5 & 6 off in 4716 */
		if ((sih->chipst & 0x00000c00) == 0x00000400)
			new |= 0x10;	/* Channel 4 if MII mode */
		if (new != reg) {
			/* apply new value */
			W_REG(si_osh(sih), &cc->pllcontrol_data, new);
			(void)R_REG(si_osh(sih), &cc->pllcontrol_data);
			W_REG(si_osh(sih), &cc->pmucontrol,
			      PCTL_PLL_PLLCTL_UPD | PCTL_NOILP_ON_WAIT |
			      PCTL_HT_REQ_EN | PCTL_ALP_REQ_EN | PCTL_LPO_SEL);
		}
	}

	if ((CHIPID(sih->chip) == BCM5356_CHIP_ID) ||
	    (CHIPID(sih->chip) == BCM5357_CHIP_ID) ||
	    (CHIPID(sih->chip) == BCM53572_CHIP_ID) ||
	    (CHIPID(sih->chip) == BCM4749_CHIP_ID)) {
		uint32 reg;

		/* Change regulator if requested */
		if (nvstr) {
			uint32 pwrctl = bcm_strtoul(nvstr, NULL, 0);

			W_REG(si_osh(sih), &cc->regcontrol_addr, 1);
			/* Readback to ensure completion of the write */
			(void)R_REG(si_osh(sih), &cc->regcontrol_addr);
			reg = R_REG(si_osh(sih), &cc->regcontrol_data);
			reg &= ~0x00018f00;
			reg |= (pwrctl & 0x00018f00);

			W_REG(si_osh(sih), &cc->regcontrol_data, reg);
		}
	}

	/* For 4716, change sflash divisor if requested. */
	if ((CHIPID(sih->chip) == BCM4716_CHIP_ID) ||
	    (CHIPID(sih->chip) == BCM4748_CHIP_ID) ||
	    (CHIPID(sih->chip) == BCM47162_CHIP_ID)) {
		char *end;
		uint32 fltype, clkdiv, bpclock, sflmaxclk, sfldiv;

		fltype = sih->cccaps & CC_CAP_FLASH_MASK;
		if ((fltype != SFLASH_ST) && (fltype != SFLASH_AT))
			goto nosflch;

		/* sdram_init is really a field in the nvram header */
		nvstr = nvram_get("sdram_init");
		sflmaxclk = bcm_strtoul(nvstr, &end, 0);
		if ((sflmaxclk = 0xffff) || (sflmaxclk == 0x0419))
			goto nosflch;

		sflmaxclk &= 0xf;
		if (sflmaxclk == 0)
			goto nosflch;

		bpclock = si_clock(sih);
		sflmaxclk *= 10000000;
		for (sfldiv = 2; sfldiv < 16; sfldiv += 2) {
			if ((bpclock / sfldiv) < sflmaxclk)
				break;
		}
		if (sfldiv > 14)
			sfldiv = 14;

		clkdiv = R_REG(si_osh(sih), &cc->clkdiv);
		if (((clkdiv & CLKD_SFLASH) >> CLKD_SFLASH_SHIFT) != sfldiv) {
			clkdiv = (clkdiv & ~CLKD_SFLASH) | (sfldiv << CLKD_SFLASH_SHIFT);
			W_REG(si_osh(sih), &cc->clkdiv, clkdiv);
		}
	}
nosflch:

	si_setcoreidx(sih, origidx);
}

void
board_cpu_init(si_t *sih)
{
	/* Initialize clocks and interrupts */
	si_mips_init(sih, 0);
}

void
flash_memory_size_config(si_t *sih, void *probe)
{
	chipcregs_t *cc = NULL;
	uint size, reg_sz, val;
	newflash_probe_t *fprobe = (newflash_probe_t *)probe;

	if (CHIPID(sih->chip) != BCM4706_CHIP_ID)
		return;
	if ((cc = (chipcregs_t *)si_setcoreidx(sih, SI_CC_IDX)))
		return;

	size = fprobe->flash_size;	/* flash total size */

	if (size <= 4*1024*1024)
		reg_sz = FLSTRCF4706_1ST_MADDR_SEG_4MB;
	else if (size > 4*1024*1024 && size <= 8*1024*1024)
		reg_sz = FLSTRCF4706_1ST_MADDR_SEG_8MB;
	else if (size > 8*1024*1024 && size <= 16*1024*1024)
		reg_sz = FLSTRCF4706_1ST_MADDR_SEG_16MB;
	else if (size > 16*1024*1024 && size <= 32*1024*1024)
		reg_sz = FLSTRCF4706_1ST_MADDR_SEG_32MB;
	else if (size > 32*1024*1024 && size <= 64*1024*1024)
		reg_sz = FLSTRCF4706_1ST_MADDR_SEG_64MB;
	else if (size > 64*1024*1024 && size <= 128*1024*1024)
		reg_sz = FLSTRCF4706_1ST_MADDR_SEG_128MB;
	else
		reg_sz = FLSTRCF4706_1ST_MADDR_SEG_256MB;

	val = R_REG(NULL, &cc->eci.flashconf.flashstrconfig);
	val &= ~FLSTRCF4706_1ST_MADDR_SEG_MASK;
	val |= reg_sz;

	W_REG(NULL, &cc->eci.flashconf.flashstrconfig, val);
}


void
board_cfe_cpu_speed_upd(si_t *sih)
{
	/* Update current CPU clock speed */
	if ((cfe_cpu_speed = si_cpu_clock(sih)) == 0) {
		cfe_cpu_speed = 133000000; /* 133 MHz */
	}
}
