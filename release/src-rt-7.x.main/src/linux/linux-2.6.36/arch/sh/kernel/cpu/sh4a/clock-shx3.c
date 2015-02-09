/*
 * arch/sh/kernel/cpu/sh4/clock-shx3.c
 *
 * SH-X3 support for the clock framework
 *
 *  Copyright (C) 2006-2007  Renesas Technology Corp.
 *  Copyright (C) 2006-2007  Renesas Solutions Corp.
 *  Copyright (C) 2006-2007  Paul Mundt
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <asm/clkdev.h>
#include <asm/clock.h>
#include <asm/freq.h>

static int ifc_divisors[] = { 1, 2, 4 ,6 };
static int bfc_divisors[] = { 1, 1, 1, 1, 1, 12, 16, 18, 24, 32, 36, 48 };
static int pfc_divisors[] = { 1, 1, 1, 1, 1, 1, 1, 18, 24, 32, 36, 48 };
static int cfc_divisors[] = { 1, 1, 4, 6 };

#define IFC_POS		28
#define IFC_MSK		0x0003
#define BFC_MSK		0x000f
#define PFC_MSK		0x000f
#define CFC_MSK		0x0003
#define BFC_POS		16
#define PFC_POS		0
#define CFC_POS		20

static void master_clk_init(struct clk *clk)
{
	clk->rate *= pfc_divisors[(__raw_readl(FRQCR) >> PFC_POS) & PFC_MSK];
}

static struct clk_ops shx3_master_clk_ops = {
	.init		= master_clk_init,
};

static unsigned long module_clk_recalc(struct clk *clk)
{
	int idx = ((__raw_readl(FRQCR) >> PFC_POS) & PFC_MSK);
	return clk->parent->rate / pfc_divisors[idx];
}

static struct clk_ops shx3_module_clk_ops = {
	.recalc		= module_clk_recalc,
};

static unsigned long bus_clk_recalc(struct clk *clk)
{
	int idx = ((__raw_readl(FRQCR) >> BFC_POS) & BFC_MSK);
	return clk->parent->rate / bfc_divisors[idx];
}

static struct clk_ops shx3_bus_clk_ops = {
	.recalc		= bus_clk_recalc,
};

static unsigned long cpu_clk_recalc(struct clk *clk)
{
	int idx = ((__raw_readl(FRQCR) >> IFC_POS) & IFC_MSK);
	return clk->parent->rate / ifc_divisors[idx];
}

static struct clk_ops shx3_cpu_clk_ops = {
	.recalc		= cpu_clk_recalc,
};

static struct clk_ops *shx3_clk_ops[] = {
	&shx3_master_clk_ops,
	&shx3_module_clk_ops,
	&shx3_bus_clk_ops,
	&shx3_cpu_clk_ops,
};

void __init arch_init_clk_ops(struct clk_ops **ops, int idx)
{
	if (idx < ARRAY_SIZE(shx3_clk_ops))
		*ops = shx3_clk_ops[idx];
}

static unsigned long shyway_clk_recalc(struct clk *clk)
{
	int idx = ((__raw_readl(FRQCR) >> CFC_POS) & CFC_MSK);
	return clk->parent->rate / cfc_divisors[idx];
}

static struct clk_ops shx3_shyway_clk_ops = {
	.recalc		= shyway_clk_recalc,
};

static struct clk shx3_shyway_clk = {
	.flags		= CLK_ENABLE_ON_INIT,
	.ops		= &shx3_shyway_clk_ops,
};

/*
 * Additional SHx3-specific on-chip clocks that aren't already part of the
 * clock framework
 */
static struct clk *shx3_onchip_clocks[] = {
	&shx3_shyway_clk,
};

#define CLKDEV_CON_ID(_id, _clk) { .con_id = _id, .clk = _clk }

static struct clk_lookup lookups[] = {
	/* main clocks */
	CLKDEV_CON_ID("shyway_clk", &shx3_shyway_clk),
};

int __init arch_clk_init(void)
{
	struct clk *clk;
	int i, ret = 0;

	cpg_clk_init();

	clk = clk_get(NULL, "master_clk");
	for (i = 0; i < ARRAY_SIZE(shx3_onchip_clocks); i++) {
		struct clk *clkp = shx3_onchip_clocks[i];

		clkp->parent = clk;
		ret |= clk_register(clkp);
	}

	clk_put(clk);

	clkdev_add_table(lookups, ARRAY_SIZE(lookups));

	return ret;
}
