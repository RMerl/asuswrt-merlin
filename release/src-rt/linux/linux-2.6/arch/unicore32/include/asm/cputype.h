/*
 * linux/arch/unicore32/include/asm/cputype.h
 *
 * Code specific to PKUnity SoC and UniCore ISA
 *
 * Copyright (C) 2001-2010 GUAN Xue-tao
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __UNICORE_CPUTYPE_H__
#define __UNICORE_CPUTYPE_H__

#include <linux/stringify.h>

#define CPUID_CPUID	0
#define CPUID_CACHETYPE	1

#define read_cpuid(reg)							\
	({								\
		unsigned int __val;					\
		asm("movc	%0, p0.c0, #" __stringify(reg)		\
		    : "=r" (__val)					\
		    :							\
		    : "cc");						\
		__val;							\
	})

#define uc32_cpuid		read_cpuid(CPUID_CPUID)
#define uc32_cachetype		read_cpuid(CPUID_CACHETYPE)

#endif
