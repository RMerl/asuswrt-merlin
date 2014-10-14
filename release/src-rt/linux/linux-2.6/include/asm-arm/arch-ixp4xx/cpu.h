/*
 * include/asm-arm/arch-ixp4xx/cpu.h
 *
 * IXP4XX cpu type detection
 *
 * Copyright (C) 2007 MontaVista Software, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef __ASM_ARCH_CPU_H__
#define __ASM_ARCH_CPU_H__

extern unsigned int processor_id;
/* Processor id value in CP15 Register 0 */
#define IXP425_PROCESSOR_ID_VALUE	0x690541c0
#define IXP435_PROCESSOR_ID_VALUE	0x69054040
#define IXP465_PROCESSOR_ID_VALUE	0x69054200
#define IXP4XX_PROCESSOR_ID_MASK	0xfffffff0

#define cpu_is_ixp42x()	((processor_id & IXP4XX_PROCESSOR_ID_MASK) == \
			  IXP425_PROCESSOR_ID_VALUE)
#define cpu_is_ixp43x()	((processor_id & IXP4XX_PROCESSOR_ID_MASK) == \
			  IXP435_PROCESSOR_ID_VALUE)
#define cpu_is_ixp46x()	((processor_id & IXP4XX_PROCESSOR_ID_MASK) == \
			  IXP465_PROCESSOR_ID_VALUE)

#endif  /* _ASM_ARCH_CPU_H */
