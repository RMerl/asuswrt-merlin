/*
 *  linux/arch/arm/kernel/dma-ebsa285.c
 *
 *  Copyright (C) 1998 Phil Blundell
 *
 * DMA functions specific to EBSA-285/CATS architectures
 *
 *  Changelog:
 *   09-Nov-1998 RMK	Split out ISA DMA functions to dma-isa.c
 *   17-Mar-1999 RMK	Allow any EBSA285-like architecture to have
 *			ISA DMA controllers.
 */
#include <linux/init.h>
#include <linux/io.h>
#include <linux/spinlock.h>

#include <asm/dma.h>
#include <asm/scatterlist.h>

#include <asm/mach/dma.h>
#include <asm/hardware/dec21285.h>


static int __init fb_dma_init(void)
{
#ifdef CONFIG_ISA_DMA
	if (footbridge_cfn_mode())
		isa_init_dma();
#endif
	return 0;
}
core_initcall(fb_dma_init);
