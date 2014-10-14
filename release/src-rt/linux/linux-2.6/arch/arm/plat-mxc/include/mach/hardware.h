/*
 * Copyright 2004-2007 Freescale Semiconductor, Inc. All Rights Reserved.
 * Copyright 2008 Juergen Beisert, kernel@pengutronix.de
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA.
 */

#ifndef __ASM_ARCH_MXC_HARDWARE_H__
#define __ASM_ARCH_MXC_HARDWARE_H__

#include <asm/sizes.h>

#ifdef __ASSEMBLER__
#define IOMEM(addr)	(addr)
#else
#define IOMEM(addr)	((void __force __iomem *)(addr))
#endif

#define IMX_IO_P2V_MODULE(addr, module)					\
	(((addr) - module ## _BASE_ADDR) < module ## _SIZE ?		\
	 (addr) - (module ## _BASE_ADDR) + (module ## _BASE_ADDR_VIRT) : 0)

/*
 * This is rather complicated for humans and ugly to verify, but for a machine
 * it's OK.  Still more as it is usually only applied to constants.  The upsides
 * on using this approach are:
 *
 *  - same mapping on all i.MX machines
 *  - works for assembler, too
 *  - no need to nurture #defines for virtual addresses
 *
 * The downside it, it's hard to verify (but I have a script for that).
 *
 * Obviously this needs to be injective for each SoC.  In general it maps the
 * whole address space to [0xf4000000, 0xf5ffffff].  So [0xf6000000,0xfeffffff]
 * is free for per-machine use (e.g. KZM_ARM11_01 uses 64MiB there).
 *
 * It applies the following mappings for the different SoCs:
 *
 * mx1:
 *	IO	0x00200000+0x100000	->	0xf4000000+0x100000
 * mx21:
 *	AIPI	0x10000000+0x100000	->	0xf4400000+0x100000
 *	SAHB1	0x80000000+0x100000	->	0xf4000000+0x100000
 *	X_MEMC	0xdf000000+0x004000	->	0xf5f00000+0x004000
 * mx25:
 *	AIPS1	0x43f00000+0x100000	->	0xf5300000+0x100000
 *	AIPS2	0x53f00000+0x100000	->	0xf5700000+0x100000
 *	AVIC	0x68000000+0x100000	->	0xf5800000+0x100000
 * mx27:
 *	AIPI	0x10000000+0x100000	->	0xf4400000+0x100000
 *	SAHB1	0x80000000+0x100000	->	0xf4000000+0x100000
 *	X_MEMC	0xd8000000+0x100000	->	0xf5c00000+0x100000
 * mx31:
 *	AIPS1	0x43f00000+0x100000	->	0xf5300000+0x100000
 *	AIPS2	0x53f00000+0x100000	->	0xf5700000+0x100000
 *	AVIC	0x68000000+0x100000	->	0xf5800000+0x100000
 *	X_MEMC	0xb8000000+0x010000	->	0xf4c00000+0x010000
 *	SPBA0	0x50000000+0x100000	->	0xf5400000+0x100000
 * mx35:
 *	AIPS1	0x43f00000+0x100000	->	0xf5300000+0x100000
 *	AIPS2	0x53f00000+0x100000	->	0xf5700000+0x100000
 *	AVIC	0x68000000+0x100000	->	0xf5800000+0x100000
 *	X_MEMC	0xb8000000+0x010000	->	0xf4c00000+0x010000
 *	SPBA0	0x50000000+0x100000	->	0xf5400000+0x100000
 * mx50:
 *	TZIC	0x0fffc000+0x004000	->	0xf4bfc000+0x004000
 *	SPBA0	0x50000000+0x100000	->	0xf5400000+0x100000
 *	AIPS1	0x53f00000+0x100000	->	0xf5700000+0x100000
 *	AIPS2	0x63f00000+0x100000	->	0xf5300000+0x100000
 * mx51:
 *	IRAM	0x1ffe0000+0x020000	->	0xf4fe0000+0x020000
 *	DEBUG	0x60000000+0x100000	->	0xf5000000+0x100000
 *	SPBA0	0x70000000+0x100000	->	0xf5400000+0x100000
 *	AIPS1	0x73f00000+0x100000	->	0xf5700000+0x100000
 *	AIPS2	0x83f00000+0x100000	->	0xf4300000+0x100000
 * mxc91231:
 *	L2CC	0x30000000+0x010000	->	0xf4400000+0x010000
 *	X_MEMC	0xb8000000+0x010000	->	0xf4c00000+0x010000
 *	ROMP	0x60000000+0x010000	->	0xf5000000+0x010000
 *	AVIC	0x68000000+0x010000	->	0xf5800000+0x010000
 *	AIPS1	0x43f00000+0x100000	->	0xf5300000+0x100000
 *	SPBA0	0x50000000+0x100000	->	0xf5400000+0x100000
 *	SPBA1	0x52000000+0x100000	->	0xf5600000+0x100000
 *	AIPS2	0x53f00000+0x100000	->	0xf5700000+0x100000
 */
#define IMX_IO_P2V(x)	(						\
			0xf4000000 +					\
			(((x) & 0x50000000) >> 6) +			\
			(((x) & 0x0b000000) >> 4) +			\
			(((x) & 0x000fffff)))

#define IMX_IO_ADDRESS(x)	IOMEM(IMX_IO_P2V(x))

#ifdef CONFIG_ARCH_MX5
#include <mach/mx50.h>
#include <mach/mx51.h>
#include <mach/mx53.h>
#endif

#ifdef CONFIG_ARCH_MX3
#include <mach/mx3x.h>
#include <mach/mx31.h>
#include <mach/mx35.h>
#endif

#ifdef CONFIG_ARCH_MX2
# include <mach/mx2x.h>
# ifdef CONFIG_MACH_MX21
#  include <mach/mx21.h>
# endif
# ifdef CONFIG_MACH_MX27
#  include <mach/mx27.h>
# endif
#endif

#ifdef CONFIG_ARCH_MX1
# include <mach/mx1.h>
#endif

#ifdef CONFIG_ARCH_MX25
# include <mach/mx25.h>
#endif

#ifdef CONFIG_ARCH_MXC91231
# include <mach/mxc91231.h>
#endif

#include <mach/mxc.h>

#define imx_map_entry(soc, name, _type)	{				\
	.virtual = soc ## _IO_P2V(soc ## _ ## name ## _BASE_ADDR),	\
	.pfn = __phys_to_pfn(soc ## _ ## name ## _BASE_ADDR),		\
	.length = soc ## _ ## name ## _SIZE,				\
	.type = _type,							\
}

#endif /* __ASM_ARCH_MXC_HARDWARE_H__ */
