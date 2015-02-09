/*
 * Carsten Langgaard, carstenl@mips.com
 * Copyright (C) 2000 MIPS Technologies, Inc.  All rights reserved.
 * Copyright (C) 2008 Dmitri Vorobiev
 *
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 */
#include <linux/cpu.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/ioport.h>
#include <linux/irq.h>
#include <linux/pci.h>
#include <linux/screen_info.h>
#include <linux/time.h>

#include <asm/bootinfo.h>
#include <asm/mips-boards/generic.h>
#include <asm/mips-boards/prom.h>
#include <asm/traps.h>

int coherentio; 	/* init to 0 => no DMA cache coherency (may be set by user) */
int hw_coherentio;	/* init to 0 => no HW DMA cache coherency (reflects real HW) */

const char *get_system_type(void)
{
	return "MIPS SEAD3";
}

#if defined(CONFIG_MIPS_MT_SMTC)
const char display_string[] = "               SMTC LINUX ON SEAD3               ";
#else
const char display_string[] = "               LINUX ON SEAD3               ";
#endif /* CONFIG_MIPS_MT_SMTC */

void __init plat_mem_setup(void)
{
}
