/*
 * BCM43XX Sonics SiliconBackplane ARM core routines
 *
 * Copyright (C) 2013, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: hndarm.c 328955 2012-04-23 09:06:12Z $
 */

#include <typedefs.h>
#include <bcmutils.h>
#include <siutils.h>
#include <hndsoc.h>
#include <sbchipc.h>
#include <bcmdevs.h>
#include <hndpmu.h>
#include <chipcommonb.h>
#include <armca9_core.h>
#include <ddr_core.h>

uint
BCMINITFN(si_irq)(si_t *sih)
{
	return 0;
}

/*
 * Initializes clocks and interrupts. SB and NVRAM access must be
 * initialized prior to calling.
 */
void
BCMATTACHFN(si_arm_init)(si_t *sih)
{
	return;
}

uint32
BCMINITFN(si_cpu_clock)(si_t *sih)
{
	uint32 val;
	osl_t *osh;
	osh = si_osh(sih);

	if (BCM4707_CHIP(CHIPID(sih->chip))) {
		/* Return 100 MHz if we are in default value policy 2 */
		val = (uint32)R_REG(osh, (uint32 *)IHOST_PROC_CLK_POLICY_FREQ);
		if ((val & 0x7070707) == 0x2020202)
			return 100000000;

		val = (uint32)R_REG(osh, (uint32 *)IHOST_PROC_CLK_PLLARMA);
		val = (val >> 8) & 0x2ff;
		val = (val * 25 * 1000000) / 2;

		return val;
	}

	return si_clock(sih);
}

uint32
BCMINITFN(si_mem_clock)(si_t *sih)
{
	osl_t *osh;
	uint idx;
	chipcommonbregs_t *chipcb;
	uint32 control1, control2, val;
	static uint32 BCMINITDATA(pll_table)[][3] = {
		/* DDR clock, PLLCONTROL1, PLLCONTROL2 */
		{ 333,  0x17800000,     0x1e0f1219 },
		{ 389,  0x18c00000,     0x23121219 },
		{ 400,  0x18000000,     0x20101019 },
		{ 533,  0x18000000,     0x20100c19 },
		{ 666,  0x17800000,     0x1e0f0919 },
		{ 775,  0x17c00000,     0x20100819 },
		{ 800,  0x18000000,     0x20100819 },
		{0}
	};

	osh = si_osh(sih);

	if (BCM4707_CHIP(CHIPID(sih->chip))) {
		chipcb = si_setcore(sih, NS_CCB_CORE_ID, 0);
		if (chipcb) {
			control1 = R_REG(osh, &chipcb->cru_lcpll_control1);
			control2 = R_REG(osh, &chipcb->cru_lcpll_control2);
			for (idx = 0; pll_table[idx][0] != 0; idx++) {
				if ((control1 == pll_table[idx][1]) &&
				    (control2 == pll_table[idx][2])) {
					val = pll_table[idx][0];
					return (val * 1000000);
				}
			}
		}

	}

	return si_clock(sih);
}

bool
BCMINITFN(si_arm_setclock)(si_t *sih, uint32 armclock, uint32 ddrclock, uint32 axiclock)
{
	osl_t *osh;
	uint32 val;
	bool ret = TRUE;
	int idx;
	int bootdev;
	uint32 *ddrclk, ddrclk_limit = 0;
	static uint32 BCMINITDATA(arm_pll_table)[][2] = {
		{ 600,	0x1003001 },
		{ 800,	0x1004001 },
		{ 1000,	0x1005001 },
		{ 1200,	0x1006001 },
		{ 1400,	0x1007001 },
		{ 1600,	0x1008001 },
		{0}
	};
	static uint32 BCMINITDATA(ddr_clock_table)[] = {
		333, 389, 400, 533, 666, 775, 800, 0
	};

	osh = si_osh(sih);

	if (BCM4707_CHIP(CHIPID(sih->chip))) {
		/* Check DDR clock */
		if (ddrclock && si_mem_clock(sih) != ddrclock) {
			ddrclock /= 1000000;
			for (idx = 0; ddr_clock_table[idx] != 0; idx ++) {
				if (ddrclock == ddr_clock_table[idx])
					break;
			}
			if (CHIPID(sih->chip) == BCM4707_CHIP_ID &&
				sih->chippkg != BCM4709_PKG_ID) {
				void *regs = (void *)si_setcore(sih, NS_DDR23_CORE_ID, 0);
				int ddrtype_ddr3 = 0;
				if (regs) {
					ddrtype_ddr3 = ((si_core_sflags(sih, 0, 0) & DDR_TYPE_MASK)
						== DDR_STAT_DDR3);
				}
				if (ddrtype_ddr3)
					ddrclk_limit = 533;
				else
					ddrclk_limit = 400;
			}
			if (ddr_clock_table[idx] != 0 &&
				(ddrclk_limit == 0 || ddrclock <= ddrclk_limit)) {
				ddrclk = (uint32 *)(0x1000 + BISZ_OFFSET - 4);
				*ddrclk = ddrclock;
				bootdev = soc_boot_dev((void *)sih);
				if (bootdev == SOC_BOOTDEV_NANDFLASH) {
					__asm__ __volatile__("ldr\tpc,=0x1c000000\n\t");
				} else if (bootdev == SOC_BOOTDEV_SFLASH) {
					__asm__ __volatile__("ldr\tpc,=0x1e000000\n\t");
				}
			}
		}

		/* Set CPU clock */
		armclock /= 1000000;

		/* The password */
		W_REG(osh, (uint32 *)IHOST_PROC_CLK_WR_ACCESS, 0xa5a501);

		/* ndiv_int */
		for (idx = 0; arm_pll_table[idx][0] != 0; idx++) {
			if (armclock <= arm_pll_table[idx][0])
				break;
		}
		if (arm_pll_table[idx][0] == 0) {
			ret = FALSE;
			goto done;
		}
		val = arm_pll_table[idx][1];
		W_REG(osh, (uint32 *)IHOST_PROC_CLK_PLLARMA, val);

		do {
			val = (uint32)R_REG(osh, (uint32 *)IHOST_PROC_CLK_PLLARMA);
			if (val & (1 << IHOST_PROC_CLK_PLLARMA__PLLARM_LOCK))
				break;
		} while (1);


		val |= (1 << IHOST_PROC_CLK_PLLARMA__PLLARM_SOFT_POST_RESETB);
		W_REG(osh, (uint32 *)IHOST_PROC_CLK_PLLARMA, val);

		W_REG(osh, (uint32 *)IHOST_PROC_CLK_POLICY_FREQ, 0x87070707);
		W_REG(osh, (uint32 *)IHOST_PROC_CLK_CORE0_CLKGATE, 0x00010303);
		W_REG(osh, (uint32 *)IHOST_PROC_CLK_CORE1_CLKGATE, 0x00000303);
		W_REG(osh, (uint32 *)IHOST_PROC_CLK_ARM_SWITCH_CLKGATE, 0x00010303);
		W_REG(osh, (uint32 *)IHOST_PROC_CLK_ARM_PERIPH_CLKGATE, 0x00010303);
		W_REG(osh, (uint32 *)IHOST_PROC_CLK_APB0_CLKGATE, 0x00010303);

		val = (1 << IHOST_PROC_CLK_POLICY_CTL__GO) |
		      (1 << IHOST_PROC_CLK_POLICY_CTL__GO_AC);
		W_REG(osh, (uint32 *)IHOST_PROC_CLK_POLICY_CTL, val);

		do {
			val = R_REG(osh, (uint32 *)IHOST_PROC_CLK_POLICY_CTL);
			if ((val & (1 << IHOST_PROC_CLK_POLICY_CTL__GO)) == 0)
				break;
		} while (1);
	}
done:
	return (ret);
}

void si_mem_setclock(si_t *sih, uint32 ddrclock)
{
	osl_t *osh;
	chipcommonbregs_t *chipcb;
	uint32 val;
	int idx;
	static uint32 BCMINITDATA(pll_table)[][3] = {
		/* DDR clock, PLLCONTROL1, PLLCONTROL2 */
		{ 333,  0x17800000,     0x1e0f1219 },
		{ 389,  0x18c00000,     0x23121219 },
		{ 400,  0x18000000,     0x20101019 },
		{ 533,  0x18000000,     0x20100c19 },
		{ 666,  0x17800000,     0x1e0f0919 },
		{ 775,  0x17c00000,     0x20100819 },
		{ 800,  0x18000000,     0x20100819 },
		{0}
	};

	for (idx = 0; pll_table[idx][0] != 0; idx++) {
		if (pll_table[idx][0] == ddrclock)
			break;
	}
	if (pll_table[idx][0] == 0)
		return;

	osh = si_osh(sih);
	chipcb = (chipcommonbregs_t *)si_setcore(sih, NS_CCB_CORE_ID, 0);
	if (chipcb) {
		val = 0xea68;
		W_REG(osh, &chipcb->cru_clkset_key, val);
		val = R_REG(osh, &chipcb->cru_lcpll_control1);
		val &= ~0x0ff00000;
		val |= (pll_table[idx][1] & 0x0ff00000);
		W_REG(osh, &chipcb->cru_lcpll_control1, val);

		val = R_REG(osh, &chipcb->cru_lcpll_control2);
		val &= ~0xffffff00;
		val |= (pll_table[idx][2] & 0xffffff00);
		W_REG(osh, &chipcb->cru_lcpll_control2, val);
		/* Enable change */
		val = R_REG(osh, &chipcb->cru_lcpll_control0);
		val |= 0x7;
		W_REG(osh, &chipcb->cru_lcpll_control0, val);
		val &= ~0x7;
		W_REG(osh, &chipcb->cru_lcpll_control0, val);
		val = 0;
		W_REG(osh, &chipcb->cru_clkset_key, val);
	}
}

/* Start chipc watchdog timer and wait till reset */
void
hnd_cpu_reset(si_t *sih)
{
	si_watchdog(sih, 1);
	while (1);
}

void
hnd_cpu_jumpto(void *addr)
{
	void (*jumpto)(void) = addr;

	(jumpto)();
}
