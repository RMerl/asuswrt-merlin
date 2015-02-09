/*
 * linux/arch/arm/mach-omap1/io.c
 *
 * OMAP1 I/O mapping code
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>

#include <asm/tlb.h>
#include <asm/mach/map.h>
#include <plat/mux.h>
#include <plat/tc.h>

#include "clock.h"

extern void omap_check_revision(void);
extern void omap_sram_init(void);

/*
 * The machine specific code may provide the extra mapping besides the
 * default mapping provided here.
 */
static struct map_desc omap_io_desc[] __initdata = {
	{
		.virtual	= OMAP1_IO_VIRT,
		.pfn		= __phys_to_pfn(OMAP1_IO_PHYS),
		.length		= OMAP1_IO_SIZE,
		.type		= MT_DEVICE
	}
};

#if defined(CONFIG_ARCH_OMAP730) || defined(CONFIG_ARCH_OMAP850)
static struct map_desc omap7xx_io_desc[] __initdata = {
	{
		.virtual	= OMAP7XX_DSP_BASE,
		.pfn		= __phys_to_pfn(OMAP7XX_DSP_START),
		.length		= OMAP7XX_DSP_SIZE,
		.type		= MT_DEVICE
	}, {
		.virtual	= OMAP7XX_DSPREG_BASE,
		.pfn		= __phys_to_pfn(OMAP7XX_DSPREG_START),
		.length		= OMAP7XX_DSPREG_SIZE,
		.type		= MT_DEVICE
	}
};
#endif

#ifdef CONFIG_ARCH_OMAP15XX
static struct map_desc omap1510_io_desc[] __initdata = {
	{
		.virtual	= OMAP1510_DSP_BASE,
		.pfn		= __phys_to_pfn(OMAP1510_DSP_START),
		.length		= OMAP1510_DSP_SIZE,
		.type		= MT_DEVICE
	}, {
		.virtual	= OMAP1510_DSPREG_BASE,
		.pfn		= __phys_to_pfn(OMAP1510_DSPREG_START),
		.length		= OMAP1510_DSPREG_SIZE,
		.type		= MT_DEVICE
	}
};
#endif

#if defined(CONFIG_ARCH_OMAP16XX)
static struct map_desc omap16xx_io_desc[] __initdata = {
	{
		.virtual	= OMAP16XX_DSP_BASE,
		.pfn		= __phys_to_pfn(OMAP16XX_DSP_START),
		.length		= OMAP16XX_DSP_SIZE,
		.type		= MT_DEVICE
	}, {
		.virtual	= OMAP16XX_DSPREG_BASE,
		.pfn		= __phys_to_pfn(OMAP16XX_DSPREG_START),
		.length		= OMAP16XX_DSPREG_SIZE,
		.type		= MT_DEVICE
	}
};
#endif

/*
 * Maps common IO regions for omap1. This should only get called from
 * board specific init.
 */
void __init omap1_map_common_io(void)
{
	iotable_init(omap_io_desc, ARRAY_SIZE(omap_io_desc));

	/* Normally devicemaps_init() would flush caches and tlb after
	 * mdesc->map_io(), but we must also do it here because of the CPU
	 * revision check below.
	 */
	local_flush_tlb_all();
	flush_cache_all();

	/* We want to check CPU revision early for cpu_is_omapxxxx() macros.
	 * IO space mapping must be initialized before we can do that.
	 */
	omap_check_revision();

#if defined(CONFIG_ARCH_OMAP730) || defined(CONFIG_ARCH_OMAP850)
	if (cpu_is_omap7xx()) {
		iotable_init(omap7xx_io_desc, ARRAY_SIZE(omap7xx_io_desc));
	}
#endif
#ifdef CONFIG_ARCH_OMAP15XX
	if (cpu_is_omap15xx()) {
		iotable_init(omap1510_io_desc, ARRAY_SIZE(omap1510_io_desc));
	}
#endif
#if defined(CONFIG_ARCH_OMAP16XX)
	if (cpu_is_omap16xx()) {
		iotable_init(omap16xx_io_desc, ARRAY_SIZE(omap16xx_io_desc));
	}
#endif

	omap_sram_init();
}

/*
 * Common low-level hardware init for omap1. This should only get called from
 * board specific init.
 */
void __init omap1_init_common_hw(void)
{
	/* REVISIT: Refer to OMAP5910 Errata, Advisory SYS_1: "Timeout Abort
	 * on a Posted Write in the TIPB Bridge".
	 */
	omap_writew(0x0, MPU_PUBLIC_TIPB_CNTL);
	omap_writew(0x0, MPU_PRIVATE_TIPB_CNTL);

	/* Must init clocks early to assure that timer interrupt works
	 */
	omap1_clk_init();

	omap1_mux_init();
}
