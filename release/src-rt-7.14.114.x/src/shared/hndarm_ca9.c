/*
 * BCM43XX Sonics SiliconBackplane ARM core routines
 *
 * Copyright (C) 2015, Broadcom Corporation. All Rights Reserved.
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

#define PLL_SUP_4708	0x00000001
#define PLL_SUP_4709	0x00000002
#define PLL_SUP_47092	0x00000004
#define PLL_SUP_47094	0x00000008
#define PLL_SUP_NS_ALL	(PLL_SUP_4708 | PLL_SUP_4709 | PLL_SUP_47092 | PLL_SUP_47094)
#define PLL_SUP_DDR2	0x00000001
#define PLL_SUP_DDR3	0x00000002

struct arm_pll {
	uint32 clk;
	uint32 reg_val;
	uint32 flag;
};

struct ddr_clk {
	uint32 clk;
	uint32 pll_ctrl_1;
	uint32 pll_ctrl_2;
	uint32 type_flag;
	uint32 flag;
};

static struct arm_pll arm_pll_table[] = {
	{ 600,	0x1003001, PLL_SUP_NS_ALL},
	{ 800,	0x1004001, PLL_SUP_NS_ALL},
	{ 1000, 0x1005001, PLL_SUP_4709 | PLL_SUP_47092 | PLL_SUP_47094},
	{ 1200, 0x1006001, PLL_SUP_47094},
	{ 1400, 0x1007001, PLL_SUP_47094},
	{0}
};

static struct ddr_clk ddr_clock_pll_table[] = {
	{ 333, 0x17800000, 0x1e0f1219, PLL_SUP_DDR2 | PLL_SUP_DDR3, PLL_SUP_NS_ALL},
	{ 389, 0x18c00000, 0x23121219, PLL_SUP_DDR2 | PLL_SUP_DDR3, PLL_SUP_NS_ALL},
	{ 400, 0x18000000, 0x20101019, PLL_SUP_DDR2 | PLL_SUP_DDR3, PLL_SUP_NS_ALL},
	{ 533, 0x18000000, 0x20100c19, PLL_SUP_DDR3, PLL_SUP_NS_ALL},
	{ 666, 0x17800000, 0x1e0f0919, PLL_SUP_DDR3, PLL_SUP_4709 | PLL_SUP_47094},
	{ 775, 0x17c00000, 0x20100819, PLL_SUP_DDR3, PLL_SUP_4709 | PLL_SUP_47094},
	{ 800, 0x18000000, 0x20100819, PLL_SUP_DDR3, PLL_SUP_4709 | PLL_SUP_47094},
	{0}
};

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

	osh = si_osh(sih);

	if (BCM4707_CHIP(CHIPID(sih->chip))) {
		chipcb = si_setcore(sih, NS_CCB_CORE_ID, 0);
		if (chipcb) {
			control1 = R_REG(osh, &chipcb->cru_lcpll_control1);
			control2 = R_REG(osh, &chipcb->cru_lcpll_control2);
			for (idx = 0; ddr_clock_pll_table[idx].clk != 0; idx++) {
				if ((control1 == ddr_clock_pll_table[idx].pll_ctrl_1) &&
				    (control2 == ddr_clock_pll_table[idx].pll_ctrl_2)) {
					val = ddr_clock_pll_table[idx].clk;
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
	uint32 *ddrclk;
	osh = si_osh(sih);

	if (BCM4707_CHIP(CHIPID(sih->chip))) {
		uint32 platform_flag = 0, ddr_flag = 0;
		void *regs = (void *)si_setcore(sih, NS_DDR23_CORE_ID, 0);
		if (regs) {
			ddr_flag = ((si_core_sflags(sih, 0, 0) & DDR_TYPE_MASK)
				== DDR_STAT_DDR3)? PLL_SUP_DDR3 : PLL_SUP_DDR2;
		}

		switch (sih->chippkg) {
			case BCM4708_PKG_ID:
				if (CHIPID(sih->chip) == BCM47094_CHIP_ID) {
					platform_flag = PLL_SUP_47092;
				} else {
					platform_flag = PLL_SUP_4708;
				}
				break;
			case BCM4709_PKG_ID:
				if (CHIPID(sih->chip) == BCM47094_CHIP_ID) {
					platform_flag = PLL_SUP_47094;
				} else {
					platform_flag = PLL_SUP_4709;
				}
				break;
		}

		/* Check CPU CLK table */
		for (idx = 0; arm_pll_table[idx].clk != 0; idx++) {
			if ((arm_pll_table[idx].flag & platform_flag) == 0)
				arm_pll_table[idx].clk = 0;
		}

		/* Check DDR CLK table */
		for (idx = 0; ddr_clock_pll_table[idx].clk != 0; idx++) {
			if ((ddr_clock_pll_table[idx].type_flag & ddr_flag) == 0 ||
			    (ddr_clock_pll_table[idx].flag & platform_flag) == 0) {
				ddr_clock_pll_table[idx].clk = 0;
				break;
			}
		}

		/* Check DDR clock */
		if (ddrclock && si_mem_clock(sih) != ddrclock) {
			ddrclock /= 1000000;
			for (idx = 0; ddr_clock_pll_table[idx].clk != 0; idx ++) {
				if (ddrclock == ddr_clock_pll_table[idx].clk)
					break;
			}
			if (ddr_clock_pll_table[idx].clk != 0) {
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

		/* Bypass ARM clock and run on the default sysclk */
		W_REG(osh, (uint32 *)IHOST_PROC_CLK_POLICY_FREQ, 0x82020202);

		val = (1 << IHOST_PROC_CLK_POLICY_CTL__GO) |
		      (1 << IHOST_PROC_CLK_POLICY_CTL__GO_AC);
		W_REG(osh, (uint32 *)IHOST_PROC_CLK_POLICY_CTL, val);

		do {
			val = R_REG(osh, (uint32 *)IHOST_PROC_CLK_POLICY_CTL);
			if ((val & (1 << IHOST_PROC_CLK_POLICY_CTL__GO)) == 0)
				break;
		} while (1);

		/* Now it is safe to program the ARM PLL.
		 * ndiv_int
		 */
		for (idx = 0; arm_pll_table[idx].clk != 0; idx++) {
			if (armclock <= arm_pll_table[idx].clk)
				break;
		}
		if (arm_pll_table[idx].clk == 0) {
			ret = FALSE;
			goto done;
		}
		val = arm_pll_table[idx].reg_val;
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

	for (idx = 0; ddr_clock_pll_table[idx].clk != 0; idx++) {
		if (ddr_clock_pll_table[idx].clk == ddrclock)
			break;
	}
	if (ddr_clock_pll_table[idx].clk == 0)
		return;

	osh = si_osh(sih);
	chipcb = (chipcommonbregs_t *)si_setcore(sih, NS_CCB_CORE_ID, 0);
	if (chipcb) {
		val = 0xea68;
		W_REG(osh, &chipcb->cru_clkset_key, val);
		val = R_REG(osh, &chipcb->cru_lcpll_control1);
		val &= ~0x0ff00000;
		val |= (ddr_clock_pll_table[idx].pll_ctrl_1 & 0x0ff00000);
		W_REG(osh, &chipcb->cru_lcpll_control1, val);

		val = R_REG(osh, &chipcb->cru_lcpll_control2);
		val &= ~0xffffff00;
		val |= (ddr_clock_pll_table[idx].pll_ctrl_2 & 0xffffff00);
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
