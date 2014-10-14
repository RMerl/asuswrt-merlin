/*
 * Code for TI8168 EVM.
 *
 * Copyright (C) 2010 Texas Instruments, Inc. - http://www.ti.com/
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/kernel.h>
#include <linux/init.h>

#include <mach/hardware.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>

#include <plat/irqs.h>
#include <plat/board.h>
#include <plat/common.h>

static struct omap_board_config_kernel ti8168_evm_config[] __initdata = {
};

static void __init ti8168_init_early(void)
{
	omap2_init_common_infrastructure();
	omap2_init_common_devices(NULL, NULL);
}

static void __init ti8168_evm_init_irq(void)
{
	omap_init_irq();
}

static void __init ti8168_evm_init(void)
{
	omap_serial_init();
	omap_board_config = ti8168_evm_config;
	omap_board_config_size = ARRAY_SIZE(ti8168_evm_config);
}

static void __init ti8168_evm_map_io(void)
{
	omap2_set_globals_ti816x();
	omapti816x_map_common_io();
}

MACHINE_START(TI8168EVM, "ti8168evm")
	/* Maintainer: Texas Instruments */
	.boot_params	= 0x80000100,
	.map_io		= ti8168_evm_map_io,
	.init_early	= ti8168_init_early,
	.init_irq	= ti8168_evm_init_irq,
	.timer		= &omap_timer,
	.init_machine	= ti8168_evm_init,
MACHINE_END
