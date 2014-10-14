/* arch/arm/mach-msm/include/mach/memory.h
 *
 * Copyright (C) 2007 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __ASM_ARCH_MEMORY_H
#define __ASM_ARCH_MEMORY_H

/* physical offset of RAM */
#if defined(CONFIG_ARCH_QSD8X50) && defined(CONFIG_MSM_SOC_REV_A)
#define PLAT_PHYS_OFFSET		UL(0x00000000)
#elif defined(CONFIG_ARCH_QSD8X50)
#define PLAT_PHYS_OFFSET		UL(0x20000000)
#elif defined(CONFIG_ARCH_MSM7X30)
#define PLAT_PHYS_OFFSET		UL(0x00200000)
#elif defined(CONFIG_ARCH_MSM8X60)
#define PLAT_PHYS_OFFSET		UL(0x40200000)
#elif defined(CONFIG_ARCH_MSM8960)
#define PLAT_PHYS_OFFSET		UL(0x40200000)
#else
#define PLAT_PHYS_OFFSET		UL(0x10000000)
#endif

#endif

