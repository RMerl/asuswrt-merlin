/*
 * linux/arch/sh/boards/se/7751/setup.c
 *
 * Copyright (C) 2000  Kazumoto Kojima
 *
 * Hitachi SolutionEngine Support.
 *
 * Modified for 7751 Solution Engine by
 * Ian da Silva and Jeremy Siegel, 2001.
 */
#include <linux/init.h>
#include <linux/platform_device.h>
#include <asm/machvec.h>
#include <asm/se7751.h>
#include <asm/io.h>

static unsigned char heartbeat_bit_pos[] = { 8, 9, 10, 11, 12, 13, 14, 15 };

static struct resource heartbeat_resources[] = {
	[0] = {
		.start	= PA_LED,
		.end	= PA_LED + ARRAY_SIZE(heartbeat_bit_pos) - 1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct platform_device heartbeat_device = {
	.name		= "heartbeat",
	.id		= -1,
	.dev	= {
		.platform_data	= heartbeat_bit_pos,
	},
	.num_resources	= ARRAY_SIZE(heartbeat_resources),
	.resource	= heartbeat_resources,
};

static struct platform_device *se7751_devices[] __initdata = {
	&smc91x_device,
	&heartbeat_device,
};

static int __init se7751_devices_setup(void)
{
	return platform_add_devices(se7751_devices, ARRAY_SIZE(se7751_devices));
}
__initcall(se7751_devices_setup);

/*
 * The Machine Vector
 */
struct sh_machine_vector mv_7751se __initmv = {
	.mv_name		= "7751 SolutionEngine",
	.mv_nr_irqs		= 72,

	.mv_inb			= sh7751se_inb,
	.mv_inw			= sh7751se_inw,
	.mv_inl			= sh7751se_inl,
	.mv_outb		= sh7751se_outb,
	.mv_outw		= sh7751se_outw,
	.mv_outl		= sh7751se_outl,

	.mv_inb_p		= sh7751se_inb_p,
	.mv_inw_p		= sh7751se_inw,
	.mv_inl_p		= sh7751se_inl,
	.mv_outb_p		= sh7751se_outb_p,
	.mv_outw_p		= sh7751se_outw,
	.mv_outl_p		= sh7751se_outl,

	.mv_insl		= sh7751se_insl,
	.mv_outsl		= sh7751se_outsl,

	.mv_init_irq		= init_7751se_IRQ,
};
ALIAS_MV(7751se)
