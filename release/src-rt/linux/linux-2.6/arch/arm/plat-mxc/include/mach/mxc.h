/*
 * Copyright 2004-2007, 2010 Freescale Semiconductor, Inc. All Rights Reserved.
 * Copyright (C) 2008 Juergen Beisert (kernel@pengutronix.de)
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
 * MA 02110-1301, USA.
 */

#ifndef __ASM_ARCH_MXC_H__
#define __ASM_ARCH_MXC_H__

#include <linux/types.h>

#ifndef __ASM_ARCH_MXC_HARDWARE_H__
#error "Do not include directly."
#endif

#define MXC_CPU_MX1		1
#define MXC_CPU_MX21		21
#define MXC_CPU_MX25		25
#define MXC_CPU_MX27		27
#define MXC_CPU_MX31		31
#define MXC_CPU_MX35		35
#define MXC_CPU_MX50		50
#define MXC_CPU_MX51		51
#define MXC_CPU_MX53		53
#define MXC_CPU_MXC91231	91231

#define IMX_CHIP_REVISION_1_0		0x10
#define IMX_CHIP_REVISION_1_1		0x11
#define IMX_CHIP_REVISION_1_2		0x12
#define IMX_CHIP_REVISION_1_3		0x13
#define IMX_CHIP_REVISION_2_0		0x20
#define IMX_CHIP_REVISION_2_1		0x21
#define IMX_CHIP_REVISION_2_2		0x22
#define IMX_CHIP_REVISION_2_3		0x23
#define IMX_CHIP_REVISION_3_0		0x30
#define IMX_CHIP_REVISION_3_1		0x31
#define IMX_CHIP_REVISION_3_2		0x32
#define IMX_CHIP_REVISION_3_3		0x33
#define IMX_CHIP_REVISION_UNKNOWN	0xff

#define IMX_CHIP_REVISION_1_0_STRING		"1.0"
#define IMX_CHIP_REVISION_1_1_STRING		"1.1"
#define IMX_CHIP_REVISION_1_2_STRING		"1.2"
#define IMX_CHIP_REVISION_1_3_STRING		"1.3"
#define IMX_CHIP_REVISION_2_0_STRING		"2.0"
#define IMX_CHIP_REVISION_2_1_STRING		"2.1"
#define IMX_CHIP_REVISION_2_2_STRING		"2.2"
#define IMX_CHIP_REVISION_2_3_STRING		"2.3"
#define IMX_CHIP_REVISION_3_0_STRING		"3.0"
#define IMX_CHIP_REVISION_3_1_STRING		"3.1"
#define IMX_CHIP_REVISION_3_2_STRING		"3.2"
#define IMX_CHIP_REVISION_3_3_STRING		"3.3"
#define IMX_CHIP_REVISION_UNKNOWN_STRING	"unknown"

#ifndef __ASSEMBLY__
extern unsigned int __mxc_cpu_type;
#endif

#ifdef CONFIG_ARCH_MX1
# ifdef mxc_cpu_type
#  undef mxc_cpu_type
#  define mxc_cpu_type __mxc_cpu_type
# else
#  define mxc_cpu_type MXC_CPU_MX1
# endif
# define cpu_is_mx1()		(mxc_cpu_type == MXC_CPU_MX1)
#else
# define cpu_is_mx1()		(0)
#endif

#ifdef CONFIG_MACH_MX21
# ifdef mxc_cpu_type
#  undef mxc_cpu_type
#  define mxc_cpu_type __mxc_cpu_type
# else
#  define mxc_cpu_type MXC_CPU_MX21
# endif
# define cpu_is_mx21()		(mxc_cpu_type == MXC_CPU_MX21)
#else
# define cpu_is_mx21()		(0)
#endif

#ifdef CONFIG_ARCH_MX25
# ifdef mxc_cpu_type
#  undef mxc_cpu_type
#  define mxc_cpu_type __mxc_cpu_type
# else
#  define mxc_cpu_type MXC_CPU_MX25
# endif
# define cpu_is_mx25()		(mxc_cpu_type == MXC_CPU_MX25)
#else
# define cpu_is_mx25()		(0)
#endif

#ifdef CONFIG_MACH_MX27
# ifdef mxc_cpu_type
#  undef mxc_cpu_type
#  define mxc_cpu_type __mxc_cpu_type
# else
#  define mxc_cpu_type MXC_CPU_MX27
# endif
# define cpu_is_mx27()		(mxc_cpu_type == MXC_CPU_MX27)
#else
# define cpu_is_mx27()		(0)
#endif

#ifdef CONFIG_SOC_IMX31
# ifdef mxc_cpu_type
#  undef mxc_cpu_type
#  define mxc_cpu_type __mxc_cpu_type
# else
#  define mxc_cpu_type MXC_CPU_MX31
# endif
# define cpu_is_mx31()		(mxc_cpu_type == MXC_CPU_MX31)
#else
# define cpu_is_mx31()		(0)
#endif

#ifdef CONFIG_SOC_IMX35
# ifdef mxc_cpu_type
#  undef mxc_cpu_type
#  define mxc_cpu_type __mxc_cpu_type
# else
#  define mxc_cpu_type MXC_CPU_MX35
# endif
# define cpu_is_mx35()		(mxc_cpu_type == MXC_CPU_MX35)
#else
# define cpu_is_mx35()		(0)
#endif

#ifdef CONFIG_SOC_IMX50
# ifdef mxc_cpu_type
#  undef mxc_cpu_type
#  define mxc_cpu_type __mxc_cpu_type
# else
#  define mxc_cpu_type MXC_CPU_MX50
# endif
# define cpu_is_mx50()		(mxc_cpu_type == MXC_CPU_MX50)
#else
# define cpu_is_mx50()		(0)
#endif

#ifdef CONFIG_SOC_IMX51
# ifdef mxc_cpu_type
#  undef mxc_cpu_type
#  define mxc_cpu_type __mxc_cpu_type
# else
#  define mxc_cpu_type MXC_CPU_MX51
# endif
# define cpu_is_mx51()		(mxc_cpu_type == MXC_CPU_MX51)
#else
# define cpu_is_mx51()		(0)
#endif

#ifdef CONFIG_SOC_IMX53
# ifdef mxc_cpu_type
#  undef mxc_cpu_type
#  define mxc_cpu_type __mxc_cpu_type
# else
#  define mxc_cpu_type MXC_CPU_MX53
# endif
# define cpu_is_mx53()		(mxc_cpu_type == MXC_CPU_MX53)
#else
# define cpu_is_mx53()		(0)
#endif

#ifdef CONFIG_ARCH_MXC91231
# ifdef mxc_cpu_type
#  undef mxc_cpu_type
#  define mxc_cpu_type __mxc_cpu_type
# else
#  define mxc_cpu_type MXC_CPU_MXC91231
# endif
# define cpu_is_mxc91231()	(mxc_cpu_type == MXC_CPU_MXC91231)
#else
# define cpu_is_mxc91231()	(0)
#endif

#ifndef __ASSEMBLY__

struct cpu_op {
	u32 cpu_rate;
};

int tzic_enable_wake(int is_idle);
enum mxc_cpu_pwr_mode {
	WAIT_CLOCKED,		/* wfi only */
	WAIT_UNCLOCKED,		/* WAIT */
	WAIT_UNCLOCKED_POWER_OFF,	/* WAIT + SRPG */
	STOP_POWER_ON,		/* just STOP */
	STOP_POWER_OFF,		/* STOP + SRPG */
};

extern struct cpu_op *(*get_cpu_op)(int *op);
#endif

#if defined(CONFIG_ARCH_MX3) || defined(CONFIG_ARCH_MX2)
/* These are deprecated, use mx[23][157]_setup_weimcs instead. */
#define CSCR_U(n) (IO_ADDRESS(WEIM_BASE_ADDR + n * 0x10))
#define CSCR_L(n) (IO_ADDRESS(WEIM_BASE_ADDR + n * 0x10 + 0x4))
#define CSCR_A(n) (IO_ADDRESS(WEIM_BASE_ADDR + n * 0x10 + 0x8))
#endif

#define cpu_is_mx3()	(cpu_is_mx31() || cpu_is_mx35() || cpu_is_mxc91231())
#define cpu_is_mx2()	(cpu_is_mx21() || cpu_is_mx27())

#endif /*  __ASM_ARCH_MXC_H__ */
