/* linux/arch/arm/plat-s5p/cpu.c
 *
 * Copyright (c) 2009-2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * S5P CPU Support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/init.h>
#include <linux/module.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>

#include <mach/map.h>
#include <mach/regs-clock.h>

#include <plat/cpu.h>
#include <plat/s5p6440.h>
#include <plat/s5p6442.h>
#include <plat/s5p6450.h>
#include <plat/s5pc100.h>
#include <plat/s5pv210.h>
#include <plat/exynos4.h>

/* table of supported CPUs */

static const char name_s5p6440[] = "S5P6440";
static const char name_s5p6442[] = "S5P6442";
static const char name_s5p6450[] = "S5P6450";
static const char name_s5pc100[] = "S5PC100";
static const char name_s5pv210[] = "S5PV210/S5PC110";
static const char name_exynos4210[] = "EXYNOS4210";

static struct cpu_table cpu_ids[] __initdata = {
	{
		.idcode		= 0x56440100,
		.idmask		= 0xfffff000,
		.map_io		= s5p6440_map_io,
		.init_clocks	= s5p6440_init_clocks,
		.init_uarts	= s5p6440_init_uarts,
		.init		= s5p64x0_init,
		.name		= name_s5p6440,
	}, {
		.idcode		= 0x36442000,
		.idmask		= 0xfffff000,
		.map_io		= s5p6442_map_io,
		.init_clocks	= s5p6442_init_clocks,
		.init_uarts	= s5p6442_init_uarts,
		.init		= s5p6442_init,
		.name		= name_s5p6442,
	}, {
		.idcode		= 0x36450000,
		.idmask		= 0xfffff000,
		.map_io		= s5p6450_map_io,
		.init_clocks	= s5p6450_init_clocks,
		.init_uarts	= s5p6450_init_uarts,
		.init		= s5p64x0_init,
		.name		= name_s5p6450,
	}, {
		.idcode		= 0x43100000,
		.idmask		= 0xfffff000,
		.map_io		= s5pc100_map_io,
		.init_clocks	= s5pc100_init_clocks,
		.init_uarts	= s5pc100_init_uarts,
		.init		= s5pc100_init,
		.name		= name_s5pc100,
	}, {
		.idcode		= 0x43110000,
		.idmask		= 0xfffff000,
		.map_io		= s5pv210_map_io,
		.init_clocks	= s5pv210_init_clocks,
		.init_uarts	= s5pv210_init_uarts,
		.init		= s5pv210_init,
		.name		= name_s5pv210,
	}, {
		.idcode		= 0x43210000,
		.idmask		= 0xfffe0000,
		.map_io		= exynos4_map_io,
		.init_clocks	= exynos4_init_clocks,
		.init_uarts	= exynos4_init_uarts,
		.init		= exynos4_init,
		.name		= name_exynos4210,
	},
};

/* minimal IO mapping */

static struct map_desc s5p_iodesc[] __initdata = {
	{
		.virtual	= (unsigned long)S5P_VA_CHIPID,
		.pfn		= __phys_to_pfn(S5P_PA_CHIPID),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)S3C_VA_SYS,
		.pfn		= __phys_to_pfn(S5P_PA_SYSCON),
		.length		= SZ_64K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)S3C_VA_TIMER,
		.pfn		= __phys_to_pfn(S5P_PA_TIMER),
		.length		= SZ_16K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)S3C_VA_WATCHDOG,
		.pfn		= __phys_to_pfn(S3C_PA_WDT),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)S5P_VA_SROMC,
		.pfn		= __phys_to_pfn(S5P_PA_SROMC),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	},
};

/* read cpu identification code */

void __init s5p_init_io(struct map_desc *mach_desc,
			int size, void __iomem *cpuid_addr)
{
	unsigned long idcode;

	/* initialize the io descriptors we need for initialization */
	iotable_init(s5p_iodesc, ARRAY_SIZE(s5p_iodesc));
	if (mach_desc)
		iotable_init(mach_desc, size);

	idcode = __raw_readl(cpuid_addr);
	s3c_init_cpu(idcode, cpu_ids, ARRAY_SIZE(cpu_ids));
}
