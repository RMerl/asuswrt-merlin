/*
 *  linux/arch/arm/mach-mmp/avengers_lite.c
 *
 *  Support for the Marvell PXA168-based Avengers lite Development Platform.
 *
 *  Copyright (C) 2009-2010 Marvell International Ltd.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  publishhed by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <mach/addr-map.h>
#include <mach/mfp-pxa168.h>
#include <mach/pxa168.h>
#include <mach/irqs.h>


#include "common.h"
#include <linux/delay.h>

/* Avengers lite MFP configurations */
static unsigned long avengers_lite_pin_config_V16F[] __initdata = {
	/* DEBUG_UART */
	GPIO88_UART2_TXD,
	GPIO89_UART2_RXD,
};

static void __init avengers_lite_init(void)
{
	mfp_config(ARRAY_AND_SIZE(avengers_lite_pin_config_V16F));

	/* on-chip devices */
	pxa168_add_uart(2);
}

MACHINE_START(AVENGERS_LITE, "PXA168 Avengers lite Development Platform")
	.phys_io        = APB_PHYS_BASE,
	.io_pg_offst    = (APB_VIRT_BASE >> 18) & 0xfffc,
	.map_io		= mmp_map_io,
	.init_irq       = pxa168_init_irq,
	.timer          = &pxa168_timer,
	.init_machine   = avengers_lite_init,
MACHINE_END
