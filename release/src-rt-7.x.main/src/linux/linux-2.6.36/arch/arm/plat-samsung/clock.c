/* linux/arch/arm/plat-s3c24xx/clock.c
 *
 * Copyright 2004-2005 Simtec Electronics
 *	Ben Dooks <ben@simtec.co.uk>
 *
 * S3C24XX Core clock control support
 *
 * Based on, and code from linux/arch/arm/mach-versatile/clock.c
 **
 **  Copyright (C) 2004 ARM Limited.
 **  Written by Deep Blue Solutions Limited.
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/sysdev.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/clk.h>
#include <linux/spinlock.h>
#include <linux/io.h>

#include <mach/hardware.h>
#include <asm/irq.h>

#include <plat/cpu-freq.h>

#include <plat/clock.h>
#include <plat/cpu.h>

#include <linux/serial_core.h>
#include <plat/regs-serial.h> /* for s3c24xx_uart_devs */

/* clock information */

static LIST_HEAD(clocks);

/* We originally used an mutex here, but some contexts (see resume)
 * are calling functions such as clk_set_parent() with IRQs disabled
 * causing an BUG to be triggered.
 */
DEFINE_SPINLOCK(clocks_lock);

/* enable and disable calls for use with the clk struct */

static int clk_null_enable(struct clk *clk, int enable)
{
	return 0;
}

static int dev_is_s3c_uart(struct device *dev)
{
	struct platform_device **pdev = s3c24xx_uart_devs;
	int i;
	for (i = 0; i < ARRAY_SIZE(s3c24xx_uart_devs); i++, pdev++)
		if (*pdev && dev == &(*pdev)->dev)
			return 1;
	return 0;
}

/*
 * Serial drivers call get_clock() very early, before platform bus
 * has been set up, this requires a special check to let them get
 * a proper clock
 */

static int dev_is_platform_device(struct device *dev)
{
	return dev->bus == &platform_bus_type ||
	       (dev->bus == NULL && dev_is_s3c_uart(dev));
}

/* Clock API calls */

struct clk *clk_get(struct device *dev, const char *id)
{
	struct clk *p;
	struct clk *clk = ERR_PTR(-ENOENT);
	int idno;

	if (dev == NULL || !dev_is_platform_device(dev))
		idno = -1;
	else
		idno = to_platform_device(dev)->id;

	spin_lock(&clocks_lock);

	list_for_each_entry(p, &clocks, list) {
		if (p->id == idno &&
		    strcmp(id, p->name) == 0 &&
		    try_module_get(p->owner)) {
			clk = p;
			break;
		}
	}

	/* check for the case where a device was supplied, but the
	 * clock that was being searched for is not device specific */

	if (IS_ERR(clk)) {
		list_for_each_entry(p, &clocks, list) {
			if (p->id == -1 && strcmp(id, p->name) == 0 &&
			    try_module_get(p->owner)) {
				clk = p;
				break;
			}
		}
	}

	spin_unlock(&clocks_lock);
	return clk;
}

void clk_put(struct clk *clk)
{
	module_put(clk->owner);
}

int clk_enable(struct clk *clk)
{
	if (IS_ERR(clk) || clk == NULL)
		return -EINVAL;

	clk_enable(clk->parent);

	spin_lock(&clocks_lock);

	if ((clk->usage++) == 0)
		(clk->enable)(clk, 1);

	spin_unlock(&clocks_lock);
	return 0;
}

void clk_disable(struct clk *clk)
{
	if (IS_ERR(clk) || clk == NULL)
		return;

	spin_lock(&clocks_lock);

	if ((--clk->usage) == 0)
		(clk->enable)(clk, 0);

	spin_unlock(&clocks_lock);
	clk_disable(clk->parent);
}


unsigned long clk_get_rate(struct clk *clk)
{
	if (IS_ERR(clk))
		return 0;

	if (clk->rate != 0)
		return clk->rate;

	if (clk->ops != NULL && clk->ops->get_rate != NULL)
		return (clk->ops->get_rate)(clk);

	if (clk->parent != NULL)
		return clk_get_rate(clk->parent);

	return clk->rate;
}

long clk_round_rate(struct clk *clk, unsigned long rate)
{
	if (!IS_ERR(clk) && clk->ops && clk->ops->round_rate)
		return (clk->ops->round_rate)(clk, rate);

	return rate;
}

int clk_set_rate(struct clk *clk, unsigned long rate)
{
	int ret;

	if (IS_ERR(clk))
		return -EINVAL;

	/* We do not default just do a clk->rate = rate as
	 * the clock may have been made this way by choice.
	 */

	WARN_ON(clk->ops == NULL);
	WARN_ON(clk->ops && clk->ops->set_rate == NULL);

	if (clk->ops == NULL || clk->ops->set_rate == NULL)
		return -EINVAL;

	spin_lock(&clocks_lock);
	ret = (clk->ops->set_rate)(clk, rate);
	spin_unlock(&clocks_lock);

	return ret;
}

struct clk *clk_get_parent(struct clk *clk)
{
	return clk->parent;
}

int clk_set_parent(struct clk *clk, struct clk *parent)
{
	int ret = 0;

	if (IS_ERR(clk))
		return -EINVAL;

	spin_lock(&clocks_lock);

	if (clk->ops && clk->ops->set_parent)
		ret = (clk->ops->set_parent)(clk, parent);

	spin_unlock(&clocks_lock);

	return ret;
}

EXPORT_SYMBOL(clk_get);
EXPORT_SYMBOL(clk_put);
EXPORT_SYMBOL(clk_enable);
EXPORT_SYMBOL(clk_disable);
EXPORT_SYMBOL(clk_get_rate);
EXPORT_SYMBOL(clk_round_rate);
EXPORT_SYMBOL(clk_set_rate);
EXPORT_SYMBOL(clk_get_parent);
EXPORT_SYMBOL(clk_set_parent);

/* base clocks */

int clk_default_setrate(struct clk *clk, unsigned long rate)
{
	clk->rate = rate;
	return 0;
}

struct clk_ops clk_ops_def_setrate = {
	.set_rate	= clk_default_setrate,
};

struct clk clk_xtal = {
	.name		= "xtal",
	.id		= -1,
	.rate		= 0,
	.parent		= NULL,
	.ctrlbit	= 0,
};

struct clk clk_ext = {
	.name		= "ext",
	.id		= -1,
};

struct clk clk_epll = {
	.name		= "epll",
	.id		= -1,
};

struct clk clk_mpll = {
	.name		= "mpll",
	.id		= -1,
	.ops		= &clk_ops_def_setrate,
};

struct clk clk_upll = {
	.name		= "upll",
	.id		= -1,
	.parent		= NULL,
	.ctrlbit	= 0,
};

struct clk clk_f = {
	.name		= "fclk",
	.id		= -1,
	.rate		= 0,
	.parent		= &clk_mpll,
	.ctrlbit	= 0,
};

struct clk clk_h = {
	.name		= "hclk",
	.id		= -1,
	.rate		= 0,
	.parent		= NULL,
	.ctrlbit	= 0,
	.ops		= &clk_ops_def_setrate,
};

struct clk clk_p = {
	.name		= "pclk",
	.id		= -1,
	.rate		= 0,
	.parent		= NULL,
	.ctrlbit	= 0,
	.ops		= &clk_ops_def_setrate,
};

struct clk clk_usb_bus = {
	.name		= "usb-bus",
	.id		= -1,
	.rate		= 0,
	.parent		= &clk_upll,
};


struct clk s3c24xx_uclk = {
	.name		= "uclk",
	.id		= -1,
};

/* initialise the clock system */

/**
 * s3c24xx_register_clock() - register a clock
 * @clk: The clock to register
 *
 * Add the specified clock to the list of clocks known by the system.
 */
int s3c24xx_register_clock(struct clk *clk)
{
	if (clk->enable == NULL)
		clk->enable = clk_null_enable;

	/* add to the list of available clocks */

	/* Quick check to see if this clock has already been registered. */
	BUG_ON(clk->list.prev != clk->list.next);

	spin_lock(&clocks_lock);
	list_add(&clk->list, &clocks);
	spin_unlock(&clocks_lock);

	return 0;
}

/**
 * s3c24xx_register_clocks() - register an array of clock pointers
 * @clks: Pointer to an array of struct clk pointers
 * @nr_clks: The number of clocks in the @clks array.
 *
 * Call s3c24xx_register_clock() for all the clock pointers contained
 * in the @clks list. Returns the number of failures.
 */
int s3c24xx_register_clocks(struct clk **clks, int nr_clks)
{
	int fails = 0;

	for (; nr_clks > 0; nr_clks--, clks++) {
		if (s3c24xx_register_clock(*clks) < 0) {
			struct clk *clk = *clks;
			printk(KERN_ERR "%s: failed to register %p: %s\n",
			       __func__, clk, clk->name);
			fails++;
		}
	}

	return fails;
}

/**
 * s3c_register_clocks() - register an array of clocks
 * @clkp: Pointer to the first clock in the array.
 * @nr_clks: Number of clocks to register.
 *
 * Call s3c24xx_register_clock() on the @clkp array given, printing an
 * error if it fails to register the clock (unlikely).
 */
void __init s3c_register_clocks(struct clk *clkp, int nr_clks)
{
	int ret;

	for (; nr_clks > 0; nr_clks--, clkp++) {
		ret = s3c24xx_register_clock(clkp);

		if (ret < 0) {
			printk(KERN_ERR "Failed to register clock %s (%d)\n",
			       clkp->name, ret);
		}
	}
}

/**
 * s3c_disable_clocks() - disable an array of clocks
 * @clkp: Pointer to the first clock in the array.
 * @nr_clks: Number of clocks to register.
 *
 * for internal use only at initialisation time. disable the clocks in the
 * @clkp array.
 */

void __init s3c_disable_clocks(struct clk *clkp, int nr_clks)
{
	for (; nr_clks > 0; nr_clks--, clkp++)
		(clkp->enable)(clkp, 0);
}

/* initialise all the clocks */

int __init s3c24xx_register_baseclocks(unsigned long xtal)
{
	printk(KERN_INFO "S3C24XX Clocks, Copyright 2004 Simtec Electronics\n");

	clk_xtal.rate = xtal;

	/* register our clocks */

	if (s3c24xx_register_clock(&clk_xtal) < 0)
		printk(KERN_ERR "failed to register master xtal\n");

	if (s3c24xx_register_clock(&clk_mpll) < 0)
		printk(KERN_ERR "failed to register mpll clock\n");

	if (s3c24xx_register_clock(&clk_upll) < 0)
		printk(KERN_ERR "failed to register upll clock\n");

	if (s3c24xx_register_clock(&clk_f) < 0)
		printk(KERN_ERR "failed to register cpu fclk\n");

	if (s3c24xx_register_clock(&clk_h) < 0)
		printk(KERN_ERR "failed to register cpu hclk\n");

	if (s3c24xx_register_clock(&clk_p) < 0)
		printk(KERN_ERR "failed to register cpu pclk\n");

	return 0;
}
