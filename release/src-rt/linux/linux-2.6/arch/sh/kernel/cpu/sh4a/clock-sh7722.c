/*
 * arch/sh/kernel/cpu/sh4a/clock-sh7722.c
 *
 * SH7722 clock framework support
 *
 * Copyright (C) 2009 Magnus Damm
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/clkdev.h>
#include <asm/clock.h>
#include <asm/hwblk.h>
#include <cpu/sh7722.h>

/* SH7722 registers */
#define FRQCR		0xa4150000
#define VCLKCR		0xa4150004
#define SCLKACR		0xa4150008
#define SCLKBCR		0xa415000c
#define IRDACLKCR	0xa4150018
#define PLLCR		0xa4150024
#define DLLFRQ		0xa4150050

/* Fixed 32 KHz root clock for RTC and Power Management purposes */
static struct clk r_clk = {
	.rate           = 32768,
};

/*
 * Default rate for the root input clock, reset this with clk_set_rate()
 * from the platform code.
 */
struct clk extal_clk = {
	.rate		= 33333333,
};

/* The dll block multiplies the 32khz r_clk, may be used instead of extal */
static unsigned long dll_recalc(struct clk *clk)
{
	unsigned long mult;

	if (__raw_readl(PLLCR) & 0x1000)
		mult = __raw_readl(DLLFRQ);
	else
		mult = 0;

	return clk->parent->rate * mult;
}

static struct clk_ops dll_clk_ops = {
	.recalc		= dll_recalc,
};

static struct clk dll_clk = {
	.ops		= &dll_clk_ops,
	.parent		= &r_clk,
	.flags		= CLK_ENABLE_ON_INIT,
};

static unsigned long pll_recalc(struct clk *clk)
{
	unsigned long mult = 1;
	unsigned long div = 1;

	if (__raw_readl(PLLCR) & 0x4000)
		mult = (((__raw_readl(FRQCR) >> 24) & 0x1f) + 1);
	else
		div = 2;

	return (clk->parent->rate * mult) / div;
}

static struct clk_ops pll_clk_ops = {
	.recalc		= pll_recalc,
};

static struct clk pll_clk = {
	.ops		= &pll_clk_ops,
	.flags		= CLK_ENABLE_ON_INIT,
};

struct clk *main_clks[] = {
	&r_clk,
	&extal_clk,
	&dll_clk,
	&pll_clk,
};

static int multipliers[] = { 1, 2, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
static int divisors[] = { 1, 3, 2, 5, 3, 4, 5, 6, 8, 10, 12, 16, 20 };

static struct clk_div_mult_table div4_div_mult_table = {
	.divisors = divisors,
	.nr_divisors = ARRAY_SIZE(divisors),
	.multipliers = multipliers,
	.nr_multipliers = ARRAY_SIZE(multipliers),
};

static struct clk_div4_table div4_table = {
	.div_mult_table = &div4_div_mult_table,
};

#define DIV4(_reg, _bit, _mask, _flags) \
  SH_CLK_DIV4(&pll_clk, _reg, _bit, _mask, _flags)

enum { DIV4_I, DIV4_U, DIV4_SH, DIV4_B, DIV4_B3, DIV4_P, DIV4_NR };

struct clk div4_clks[DIV4_NR] = {
	[DIV4_I] = DIV4(FRQCR, 20, 0x1fef, CLK_ENABLE_ON_INIT),
	[DIV4_U] = DIV4(FRQCR, 16, 0x1fff, CLK_ENABLE_ON_INIT),
	[DIV4_SH] = DIV4(FRQCR, 12, 0x1fff, CLK_ENABLE_ON_INIT),
	[DIV4_B] = DIV4(FRQCR, 8, 0x1fff, CLK_ENABLE_ON_INIT),
	[DIV4_B3] = DIV4(FRQCR, 4, 0x1fff, CLK_ENABLE_ON_INIT),
	[DIV4_P] = DIV4(FRQCR, 0, 0x1fff, 0),
};

enum { DIV4_IRDA, DIV4_ENABLE_NR };

struct clk div4_enable_clks[DIV4_ENABLE_NR] = {
	[DIV4_IRDA] = DIV4(IRDACLKCR, 0, 0x1fff, 0),
};

enum { DIV4_SIUA, DIV4_SIUB, DIV4_REPARENT_NR };

struct clk div4_reparent_clks[DIV4_REPARENT_NR] = {
	[DIV4_SIUA] = DIV4(SCLKACR, 0, 0x1fff, 0),
	[DIV4_SIUB] = DIV4(SCLKBCR, 0, 0x1fff, 0),
};

enum { DIV6_V, DIV6_NR };

struct clk div6_clks[DIV6_NR] = {
	[DIV6_V] = SH_CLK_DIV6(&pll_clk, VCLKCR, 0),
};

static struct clk mstp_clks[HWBLK_NR] = {
	SH_HWBLK_CLK(HWBLK_URAM, &div4_clks[DIV4_U], CLK_ENABLE_ON_INIT),
	SH_HWBLK_CLK(HWBLK_XYMEM, &div4_clks[DIV4_B], CLK_ENABLE_ON_INIT),
	SH_HWBLK_CLK(HWBLK_TMU, &div4_clks[DIV4_P], 0),
	SH_HWBLK_CLK(HWBLK_CMT, &r_clk, 0),
	SH_HWBLK_CLK(HWBLK_RWDT, &r_clk, 0),
	SH_HWBLK_CLK(HWBLK_FLCTL, &div4_clks[DIV4_P], 0),
	SH_HWBLK_CLK(HWBLK_SCIF0, &div4_clks[DIV4_P], 0),
	SH_HWBLK_CLK(HWBLK_SCIF1, &div4_clks[DIV4_P], 0),
	SH_HWBLK_CLK(HWBLK_SCIF2, &div4_clks[DIV4_P], 0),

	SH_HWBLK_CLK(HWBLK_IIC, &div4_clks[DIV4_P], 0),
	SH_HWBLK_CLK(HWBLK_RTC, &r_clk, 0),

	SH_HWBLK_CLK(HWBLK_SDHI, &div4_clks[DIV4_P], 0),
	SH_HWBLK_CLK(HWBLK_KEYSC, &r_clk, 0),
	SH_HWBLK_CLK(HWBLK_USBF, &div4_clks[DIV4_P], 0),
	SH_HWBLK_CLK(HWBLK_2DG, &div4_clks[DIV4_B], 0),
	SH_HWBLK_CLK(HWBLK_SIU, &div4_clks[DIV4_B], 0),
	SH_HWBLK_CLK(HWBLK_VOU, &div4_clks[DIV4_B], 0),
	SH_HWBLK_CLK(HWBLK_JPU, &div4_clks[DIV4_B], 0),
	SH_HWBLK_CLK(HWBLK_BEU, &div4_clks[DIV4_B], 0),
	SH_HWBLK_CLK(HWBLK_CEU, &div4_clks[DIV4_B], 0),
	SH_HWBLK_CLK(HWBLK_VEU, &div4_clks[DIV4_B], 0),
	SH_HWBLK_CLK(HWBLK_VPU, &div4_clks[DIV4_B], 0),
	SH_HWBLK_CLK(HWBLK_LCDC, &div4_clks[DIV4_P], 0),
};

#define CLKDEV_CON_ID(_id, _clk) { .con_id = _id, .clk = _clk }

static struct clk_lookup lookups[] = {
	/* main clocks */
	CLKDEV_CON_ID("rclk", &r_clk),
	CLKDEV_CON_ID("extal", &extal_clk),
	CLKDEV_CON_ID("dll_clk", &dll_clk),
	CLKDEV_CON_ID("pll_clk", &pll_clk),

	/* DIV4 clocks */
	CLKDEV_CON_ID("cpu_clk", &div4_clks[DIV4_I]),
	CLKDEV_CON_ID("umem_clk", &div4_clks[DIV4_U]),
	CLKDEV_CON_ID("shyway_clk", &div4_clks[DIV4_SH]),
	CLKDEV_CON_ID("bus_clk", &div4_clks[DIV4_B]),
	CLKDEV_CON_ID("b3_clk", &div4_clks[DIV4_B3]),
	CLKDEV_CON_ID("peripheral_clk", &div4_clks[DIV4_P]),
	CLKDEV_CON_ID("irda_clk", &div4_enable_clks[DIV4_IRDA]),
	CLKDEV_CON_ID("siua_clk", &div4_reparent_clks[DIV4_SIUA]),
	CLKDEV_CON_ID("siub_clk", &div4_reparent_clks[DIV4_SIUB]),

	/* DIV6 clocks */
	CLKDEV_CON_ID("video_clk", &div6_clks[DIV6_V]),

	/* MSTP clocks */
	CLKDEV_CON_ID("uram0", &mstp_clks[HWBLK_URAM]),
	CLKDEV_CON_ID("xymem0", &mstp_clks[HWBLK_XYMEM]),
	{
		/* TMU0 */
		.dev_id		= "sh_tmu.0",
		.con_id		= "tmu_fck",
		.clk		= &mstp_clks[HWBLK_TMU],
	}, {
		/* TMU1 */
		.dev_id		= "sh_tmu.1",
		.con_id		= "tmu_fck",
		.clk		= &mstp_clks[HWBLK_TMU],
	}, {
		/* TMU2 */
		.dev_id		= "sh_tmu.2",
		.con_id		= "tmu_fck",
		.clk		= &mstp_clks[HWBLK_TMU],
	},
	CLKDEV_CON_ID("cmt_fck", &mstp_clks[HWBLK_CMT]),
	CLKDEV_CON_ID("rwdt0", &mstp_clks[HWBLK_RWDT]),
	CLKDEV_CON_ID("flctl0", &mstp_clks[HWBLK_FLCTL]),
	{
		/* SCIF0 */
		.dev_id		= "sh-sci.0",
		.con_id		= "sci_fck",
		.clk		= &mstp_clks[HWBLK_SCIF0],
	}, {
		/* SCIF1 */
		.dev_id		= "sh-sci.1",
		.con_id		= "sci_fck",
		.clk		= &mstp_clks[HWBLK_SCIF1],
	}, {
		/* SCIF2 */
		.dev_id		= "sh-sci.2",
		.con_id		= "sci_fck",
		.clk		= &mstp_clks[HWBLK_SCIF2],
	},
	CLKDEV_CON_ID("i2c0", &mstp_clks[HWBLK_IIC]),
	CLKDEV_CON_ID("rtc0", &mstp_clks[HWBLK_RTC]),
	CLKDEV_CON_ID("sdhi0", &mstp_clks[HWBLK_SDHI]),
	CLKDEV_CON_ID("keysc0", &mstp_clks[HWBLK_KEYSC]),
	CLKDEV_CON_ID("usbf0", &mstp_clks[HWBLK_USBF]),
	CLKDEV_CON_ID("2dg0", &mstp_clks[HWBLK_2DG]),
	CLKDEV_CON_ID("siu0", &mstp_clks[HWBLK_SIU]),
	CLKDEV_CON_ID("vou0", &mstp_clks[HWBLK_VOU]),
	CLKDEV_CON_ID("jpu0", &mstp_clks[HWBLK_JPU]),
	CLKDEV_CON_ID("beu0", &mstp_clks[HWBLK_BEU]),
	CLKDEV_CON_ID("ceu0", &mstp_clks[HWBLK_CEU]),
	CLKDEV_CON_ID("veu0", &mstp_clks[HWBLK_VEU]),
	CLKDEV_CON_ID("vpu0", &mstp_clks[HWBLK_VPU]),
	CLKDEV_CON_ID("lcdc0", &mstp_clks[HWBLK_LCDC]),
};

int __init arch_clk_init(void)
{
	int k, ret = 0;

	/* autodetect extal or dll configuration */
	if (__raw_readl(PLLCR) & 0x1000)
		pll_clk.parent = &dll_clk;
	else
		pll_clk.parent = &extal_clk;

	for (k = 0; !ret && (k < ARRAY_SIZE(main_clks)); k++)
		ret = clk_register(main_clks[k]);

	clkdev_add_table(lookups, ARRAY_SIZE(lookups));

	if (!ret)
		ret = sh_clk_div4_register(div4_clks, DIV4_NR, &div4_table);

	if (!ret)
		ret = sh_clk_div4_enable_register(div4_enable_clks,
					DIV4_ENABLE_NR, &div4_table);

	if (!ret)
		ret = sh_clk_div4_reparent_register(div4_reparent_clks,
					DIV4_REPARENT_NR, &div4_table);

	if (!ret)
		ret = sh_clk_div6_register(div6_clks, DIV6_NR);

	if (!ret)
		ret = sh_hwblk_clk_register(mstp_clks, HWBLK_NR);

	return ret;
}
