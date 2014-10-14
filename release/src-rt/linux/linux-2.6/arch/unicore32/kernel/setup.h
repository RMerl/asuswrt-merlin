/*
 * linux/arch/unicore32/kernel/setup.h
 *
 * Code specific to PKUnity SoC and UniCore ISA
 *
 * Copyright (C) 2001-2010 GUAN Xue-tao
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __UNICORE_KERNEL_SETUP_H__
#define __UNICORE_KERNEL_SETUP_H__

extern void paging_init(void);
extern void puv3_core_init(void);

extern void puv3_ps2_init(void);
extern void pci_puv3_preinit(void);
extern void __init puv3_init_gpio(void);

extern void setup_mm_for_reboot(char mode);

extern char __stubs_start[], __stubs_end[];
extern char __vectors_start[], __vectors_end[];

extern void kernel_thread_helper(void);

extern void __init early_signal_init(void);
#endif
