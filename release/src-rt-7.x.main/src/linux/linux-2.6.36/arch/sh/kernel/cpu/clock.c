/*
 * arch/sh/kernel/cpu/clock.c - SuperH clock framework
 *
 *  Copyright (C) 2005 - 2009  Paul Mundt
 *
 * This clock framework is derived from the OMAP version by:
 *
 *	Copyright (C) 2004 - 2008 Nokia Corporation
 *	Written by Tuukka Tikkanen <tuukka.tikkanen@elektrobit.com>
 *
 *  Modified for omap shared clock framework by Tony Lindgren <tony@atomide.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <asm/clock.h>
#include <asm/machvec.h>

int __init clk_init(void)
{
	int ret;

	ret = arch_clk_init();
	if (unlikely(ret)) {
		pr_err("%s: CPU clock registration failed.\n", __func__);
		return ret;
	}

	if (sh_mv.mv_clk_init) {
		ret = sh_mv.mv_clk_init();
		if (unlikely(ret)) {
			pr_err("%s: machvec clock initialization failed.\n",
			       __func__);
			return ret;
		}
	}

	/* Kick the child clocks.. */
	recalculate_root_clocks();

	/* Enable the necessary init clocks */
	clk_enable_init_clocks();

	return ret;
}

/*
 * Returns a clock. Note that we first try to use device id on the bus
 * and clock name. If this fails, we try to use clock name only.
 */
struct clk *clk_get(struct device *dev, const char *con_id)
{
	const char *dev_id = dev ? dev_name(dev) : NULL;

	return clk_get_sys(dev_id, con_id);
}
EXPORT_SYMBOL_GPL(clk_get);

void clk_put(struct clk *clk)
{
}
EXPORT_SYMBOL_GPL(clk_put);
