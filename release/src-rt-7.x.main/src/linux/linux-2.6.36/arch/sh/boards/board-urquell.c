/*
 * Renesas Technology Corp. SH7786 Urquell Support.
 *
 * Copyright (C) 2008  Kuninori Morimoto <morimoto.kuninori@renesas.com>
 * Copyright (C) 2009, 2010  Paul Mundt
 *
 * Based on board-sh7785lcr.c
 * Copyright (C) 2008  Yoshihiro Shimoda
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <linux/smc91x.h>
#include <linux/mtd/physmap.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/clk.h>
#include <mach/urquell.h>
#include <cpu/sh7786.h>
#include <asm/heartbeat.h>
#include <asm/sizes.h>
#include <asm/smp-ops.h>


/* HeartBeat */
static struct resource heartbeat_resource = {
	.start	= BOARDREG(SLEDR),
	.end	= BOARDREG(SLEDR),
	.flags	= IORESOURCE_MEM | IORESOURCE_MEM_16BIT,
};

static struct platform_device heartbeat_device = {
	.name		= "heartbeat",
	.id		= -1,
	.num_resources	= 1,
	.resource	= &heartbeat_resource,
};

/* LAN91C111 */
static struct smc91x_platdata smc91x_info = {
	.flags = SMC91X_USE_16BIT | SMC91X_NOWAIT,
};

static struct resource smc91x_eth_resources[] = {
	[0] = {
		.name   = "SMC91C111" ,
		.start  = 0x05800300,
		.end    = 0x0580030f,
		.flags  = IORESOURCE_MEM,
	},
	[1] = {
		.start  = 11,
		.flags  = IORESOURCE_IRQ,
	},
};

static struct platform_device smc91x_eth_device = {
	.name           = "smc91x",
	.num_resources  = ARRAY_SIZE(smc91x_eth_resources),
	.resource       = smc91x_eth_resources,
	.dev	= {
		.platform_data	= &smc91x_info,
	},
};

/* Nor Flash */
static struct mtd_partition nor_flash_partitions[] = {
	{
		.name		= "loader",
		.offset		= 0x00000000,
		.size		= SZ_512K,
		.mask_flags	= MTD_WRITEABLE,	/* Read-only */
	},
	{
		.name		= "bootenv",
		.offset		= MTDPART_OFS_APPEND,
		.size		= SZ_512K,
		.mask_flags	= MTD_WRITEABLE,	/* Read-only */
	},
	{
		.name		= "kernel",
		.offset		= MTDPART_OFS_APPEND,
		.size		= SZ_4M,
	},
	{
		.name		= "data",
		.offset		= MTDPART_OFS_APPEND,
		.size		= MTDPART_SIZ_FULL,
	},
};

static struct physmap_flash_data nor_flash_data = {
	.width		= 2,
	.parts		= nor_flash_partitions,
	.nr_parts	= ARRAY_SIZE(nor_flash_partitions),
};

static struct resource nor_flash_resources[] = {
	[0] = {
		.start	= NOR_FLASH_ADDR,
		.end	= NOR_FLASH_ADDR + NOR_FLASH_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	}
};

static struct platform_device nor_flash_device = {
	.name		= "physmap-flash",
	.dev		= {
		.platform_data	= &nor_flash_data,
	},
	.num_resources	= ARRAY_SIZE(nor_flash_resources),
	.resource	= nor_flash_resources,
};

static struct platform_device *urquell_devices[] __initdata = {
	&heartbeat_device,
	&smc91x_eth_device,
	&nor_flash_device,
};

static int __init urquell_devices_setup(void)
{
	/* USB */
	gpio_request(GPIO_FN_USB_OVC0,  NULL);
	gpio_request(GPIO_FN_USB_PENC0, NULL);

	/* enable LAN */
	__raw_writew(__raw_readw(UBOARDREG(IRL2MSKR)) & ~0x00000001,
		  UBOARDREG(IRL2MSKR));

	return platform_add_devices(urquell_devices,
				    ARRAY_SIZE(urquell_devices));
}
device_initcall(urquell_devices_setup);

static void urquell_power_off(void)
{
	__raw_writew(0xa5a5, UBOARDREG(SRSTR));
}

static void __init urquell_init_irq(void)
{
	plat_irq_setup_pins(IRQ_MODE_IRL3210_MASK);
}

static int urquell_mode_pins(void)
{
	return __raw_readw(UBOARDREG(MDSWMR));
}

static int urquell_clk_init(void)
{
	struct clk *clk;
	int ret;

	/*
	 * Only handle the EXTAL case, anyone interfacing a crystal
	 * resonator will need to provide their own input clock.
	 */
	if (test_mode_pin(MODE_PIN9))
		return -EINVAL;

	clk = clk_get(NULL, "extal");
	if (!clk || IS_ERR(clk))
		return PTR_ERR(clk);
	ret = clk_set_rate(clk, 33333333);
	clk_put(clk);

	return ret;
}

/* Initialize the board */
static void __init urquell_setup(char **cmdline_p)
{
	printk(KERN_INFO "Renesas Technology Corp. Urquell support.\n");

	pm_power_off = urquell_power_off;

	register_smp_ops(&shx3_smp_ops);
}

/*
 * The Machine Vector
 */
static struct sh_machine_vector mv_urquell __initmv = {
	.mv_name	= "Urquell",
	.mv_setup	= urquell_setup,
	.mv_init_irq	= urquell_init_irq,
	.mv_mode_pins	= urquell_mode_pins,
	.mv_clk_init	= urquell_clk_init,
};
