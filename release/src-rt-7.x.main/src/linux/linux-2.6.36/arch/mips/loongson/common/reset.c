/*
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 * Copyright (C) 2007 Lemote, Inc. & Institute of Computing Technology
 * Author: Fuxin Zhang, zhangfx@lemote.com
 * Copyright (C) 2009 Lemote, Inc.
 * Author: Zhangjin Wu, wuzhangjin@gmail.com
 */
#include <linux/init.h>
#include <linux/pm.h>

#include <asm/reboot.h>

#include <loongson.h>

static inline void loongson_reboot(void)
{
#ifndef CONFIG_CPU_JUMP_WORKAROUNDS
	((void (*)(void))ioremap_nocache(LOONGSON_BOOT_BASE, 4)) ();
#else
	void (*func)(void);

	func = (void *)ioremap_nocache(LOONGSON_BOOT_BASE, 4);

	__asm__ __volatile__(
	"       .set    noat                                            \n"
	"       jr      %[func]                                         \n"
	"       .set    at                                              \n"
	: /* No outputs */
	: [func] "r" (func));
#endif
}

static void loongson_restart(char *command)
{
	/* do preparation for reboot */
	mach_prepare_reboot();

	/* reboot via jumping to boot base address */
	loongson_reboot();
}

static void loongson_poweroff(void)
{
	mach_prepare_shutdown();
	unreachable();
}

static void loongson_halt(void)
{
	pr_notice("\n\n** You can safely turn off the power now **\n\n");
	while (1) {
		if (cpu_wait)
			cpu_wait();
	}
}

static int __init mips_reboot_setup(void)
{
	_machine_restart = loongson_restart;
	_machine_halt = loongson_halt;
	pm_power_off = loongson_poweroff;

	return 0;
}

arch_initcall(mips_reboot_setup);
