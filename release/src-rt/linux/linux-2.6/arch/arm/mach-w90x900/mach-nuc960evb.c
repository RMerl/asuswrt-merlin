/*
 * linux/arch/arm/mach-w90x900/mach-nuc960evb.c
 *
 * Based on mach-s3c2410/mach-smdk2410.c by Jonas Dietsche
 *
 * Copyright (C) 2008 Nuvoton technology corporation.
 *
 * Wan ZongShun <mcuos.com@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation;version 2 of the License.
 *
 */

#include <linux/platform_device.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach-types.h>
#include <mach/map.h>

#include "nuc960.h"

static void __init nuc960evb_map_io(void)
{
	nuc960_map_io();
	nuc960_init_clocks();
}

static void __init nuc960evb_init(void)
{
	nuc960_board_init();
}

MACHINE_START(W90N960EVB, "W90N960EVB")
	/* Maintainer: Wan ZongShun */
	.boot_params	= 0,
	.map_io		= nuc960evb_map_io,
	.init_irq	= nuc900_init_irq,
	.init_machine	= nuc960evb_init,
	.timer		= &nuc900_timer,
MACHINE_END
