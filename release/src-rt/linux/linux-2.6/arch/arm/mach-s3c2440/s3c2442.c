/* linux/arch/arm/mach-s3c2442/s3c2442.c
 *
 * Copyright (c) 2004-2005 Simtec Electronics
 *	http://armlinux.simtec.co.uk/
 *	Ben Dooks <ben@simtec.co.uk>
 *
 * S3C2442 core and lock support
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
#include <linux/device.h>
#include <linux/sysdev.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/mutex.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/io.h>

#include <mach/hardware.h>
#include <asm/atomic.h>
#include <asm/irq.h>

#include <mach/regs-clock.h>

#include <plat/clock.h>
#include <plat/cpu.h>
#include <plat/s3c244x.h>

#include <plat/gpio-core.h>
#include <plat/gpio-cfg.h>
#include <plat/gpio-cfg-helpers.h>

/* S3C2442 extended clock support */

static unsigned long s3c2442_camif_upll_round(struct clk *clk,
					      unsigned long rate)
{
	unsigned long parent_rate = clk_get_rate(clk->parent);
	int div;

	if (rate > parent_rate)
		return parent_rate;

	div = parent_rate / rate;

	if (div == 3)
		return parent_rate / 3;

	/* note, we remove the +/- 1 calculations for the divisor */

	div /= 2;

	if (div < 1)
		div = 1;
	else if (div > 16)
		div = 16;

	return parent_rate / (div * 2);
}

static int s3c2442_camif_upll_setrate(struct clk *clk, unsigned long rate)
{
	unsigned long parent_rate = clk_get_rate(clk->parent);
	unsigned long camdivn =  __raw_readl(S3C2440_CAMDIVN);

	rate = s3c2442_camif_upll_round(clk, rate);

	camdivn &= ~S3C2442_CAMDIVN_CAMCLK_DIV3;

	if (rate == parent_rate) {
		camdivn &= ~S3C2440_CAMDIVN_CAMCLK_SEL;
	} else if ((parent_rate / rate) == 3) {
		camdivn |= S3C2440_CAMDIVN_CAMCLK_SEL;
		camdivn |= S3C2442_CAMDIVN_CAMCLK_DIV3;
	} else {
		camdivn &= ~S3C2440_CAMDIVN_CAMCLK_MASK;
		camdivn |= S3C2440_CAMDIVN_CAMCLK_SEL;
		camdivn |= (((parent_rate / rate) / 2) - 1);
	}

	__raw_writel(camdivn, S3C2440_CAMDIVN);

	return 0;
}

/* Extra S3C2442 clocks */

static struct clk s3c2442_clk_cam = {
	.name		= "camif",
	.id		= -1,
	.enable		= s3c2410_clkcon_enable,
	.ctrlbit	= S3C2440_CLKCON_CAMERA,
};

static struct clk s3c2442_clk_cam_upll = {
	.name		= "camif-upll",
	.id		= -1,
	.ops		= &(struct clk_ops) {
		.set_rate	= s3c2442_camif_upll_setrate,
		.round_rate	= s3c2442_camif_upll_round,
	},
};

static int s3c2442_clk_add(struct sys_device *sysdev)
{
	struct clk *clock_upll;
	struct clk *clock_h;
	struct clk *clock_p;

	clock_p = clk_get(NULL, "pclk");
	clock_h = clk_get(NULL, "hclk");
	clock_upll = clk_get(NULL, "upll");

	if (IS_ERR(clock_p) || IS_ERR(clock_h) || IS_ERR(clock_upll)) {
		printk(KERN_ERR "S3C2442: Failed to get parent clocks\n");
		return -EINVAL;
	}

	s3c2442_clk_cam.parent = clock_h;
	s3c2442_clk_cam_upll.parent = clock_upll;

	s3c24xx_register_clock(&s3c2442_clk_cam);
	s3c24xx_register_clock(&s3c2442_clk_cam_upll);

	clk_disable(&s3c2442_clk_cam);

	return 0;
}

static struct sysdev_driver s3c2442_clk_driver = {
	.add	= s3c2442_clk_add,
};

static __init int s3c2442_clk_init(void)
{
	return sysdev_driver_register(&s3c2442_sysclass, &s3c2442_clk_driver);
}

arch_initcall(s3c2442_clk_init);


static struct sys_device s3c2442_sysdev = {
	.cls		= &s3c2442_sysclass,
};

int __init s3c2442_init(void)
{
	printk("S3C2442: Initialising architecture\n");

	return sysdev_register(&s3c2442_sysdev);
}

void __init s3c2442_map_io(void)
{
	s3c244x_map_io();

	s3c24xx_gpiocfg_default.set_pull = s3c_gpio_setpull_1down;
	s3c24xx_gpiocfg_default.get_pull = s3c_gpio_getpull_1down;
}
