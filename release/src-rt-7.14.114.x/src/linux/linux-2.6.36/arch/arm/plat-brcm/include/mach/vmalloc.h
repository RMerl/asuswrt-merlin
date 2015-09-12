/*
 *  arch/arm/mach-versatile/include/mach/vmalloc.h
 *
 *  Copyright (C) 2003 ARM Limited
 *  Copyright (C) 2000 Russell King.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 * This file is included from architecture common code in:
 * arch/arm/include/asm/pgtable.h
 */

#if defined(CONFIG_VMSPLIT_3G)
#define VMALLOC_END		(PAGE_OFFSET + 0x30000000)
#elif defined(CONFIG_VMSPLIT_2G)
#define VMALLOC_END		(PAGE_OFFSET + 0x70000000)
#elif defined(CONFIG_VMSPLIT_1G)
#define VMALLOC_END		(PAGE_OFFSET + 0xB0000000)
#endif
