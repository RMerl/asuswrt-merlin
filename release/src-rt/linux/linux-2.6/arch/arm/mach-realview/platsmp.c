/*
 *  linux/arch/arm/mach-realview/platsmp.c
 *
 *  Copyright (C) 2002 ARM Ltd.
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/smp.h>
#include <linux/io.h>

#include <mach/hardware.h>
#include <asm/mach-types.h>
#include <asm/smp_scu.h>
#include <asm/unified.h>

#include <mach/board-eb.h>
#include <mach/board-pb11mp.h>
#include <mach/board-pbx.h>

#include "core.h"

extern void versatile_secondary_startup(void);

static void __iomem *scu_base_addr(void)
{
	if (machine_is_realview_eb_mp())
		return __io_address(REALVIEW_EB11MP_SCU_BASE);
	else if (machine_is_realview_pb11mp())
		return __io_address(REALVIEW_TC11MP_SCU_BASE);
	else if (machine_is_realview_pbx() &&
		 (core_tile_pbx11mp() || core_tile_pbxa9mp()))
		return __io_address(REALVIEW_PBX_TILE_SCU_BASE);
	else
		return (void __iomem *)0;
}

/*
 * Initialise the CPU possible map early - this describes the CPUs
 * which may be present or become present in the system.
 */
void __init smp_init_cpus(void)
{
	void __iomem *scu_base = scu_base_addr();
	unsigned int i, ncores;

	ncores = scu_base ? scu_get_core_count(scu_base) : 1;

	/* sanity check */
	if (ncores > NR_CPUS) {
		printk(KERN_WARNING
		       "Realview: no. of cores (%d) greater than configured "
		       "maximum of %d - clipping\n",
		       ncores, NR_CPUS);
		ncores = NR_CPUS;
	}

	for (i = 0; i < ncores; i++)
		set_cpu_possible(i, true);
}

void __init platform_smp_prepare_cpus(unsigned int max_cpus)
{
	int i;

	/*
	 * Initialise the present map, which describes the set of CPUs
	 * actually populated at the present time.
	 */
	for (i = 0; i < max_cpus; i++)
		set_cpu_present(i, true);

	scu_enable(scu_base_addr());

	/*
	 * Write the address of secondary startup into the
	 * system-wide flags register. The BootMonitor waits
	 * until it receives a soft interrupt, and then the
	 * secondary CPU branches to this address.
	 */
	__raw_writel(BSYM(virt_to_phys(versatile_secondary_startup)),
		     __io_address(REALVIEW_SYS_FLAGSSET));
}
