/*
 *  arch/arm/mach-ebsa110/include/mach/memory.h
 *
 *  Copyright (C) 1996-1999 Russell King.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Changelog:
 *   20-Oct-1996 RMK	Created
 *   31-Dec-1997 RMK	Fixed definitions to reduce warnings
 *   21-Mar-1999 RMK	Renamed to memory.h
 *		 RMK	Moved TASK_SIZE and PAGE_OFFSET here
 */
#ifndef __ASM_ARCH_MEMORY_H
#define __ASM_ARCH_MEMORY_H

/*
 * Physical DRAM offset.
 */
#define PLAT_PHYS_OFFSET	UL(0x00000000)

/*
 * Cache flushing area - SRAM
 */
#define FLUSH_BASE_PHYS		0x40000000
#define FLUSH_BASE		0xdf000000

#endif
