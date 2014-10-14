/* linux/arch/arm/mach-s3c2416/pm.c
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * S3C2416 - PM support (Based on Ben Dooks' S3C2412 PM support)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/sysdev.h>
#include <linux/io.h>

#include <asm/cacheflush.h>

#include <mach/regs-power.h>
#include <mach/regs-s3c2443-clock.h>

#include <plat/cpu.h>
#include <plat/pm.h>

extern void s3c2412_sleep_enter(void);

static void s3c2416_cpu_suspend(void)
{
	flush_cache_all();

	/* enable wakeup sources regardless of battery state */
	__raw_writel(S3C2443_PWRCFG_SLEEP, S3C2443_PWRCFG);

	/* set the mode as sleep, 2BED represents "Go to BED" */
	__raw_writel(0x2BED, S3C2443_PWRMODE);

	s3c2412_sleep_enter();
}

static void s3c2416_pm_prepare(void)
{
	/*
	 * write the magic value u-boot uses to check for resume into
	 * the INFORM0 register, and ensure INFORM1 is set to the
	 * correct address to resume from.
	 */
	__raw_writel(0x2BED, S3C2412_INFORM0);
	__raw_writel(virt_to_phys(s3c_cpu_resume), S3C2412_INFORM1);
}

static int s3c2416_pm_add(struct sys_device *sysdev)
{
	pm_cpu_prep = s3c2416_pm_prepare;
	pm_cpu_sleep = s3c2416_cpu_suspend;

	return 0;
}

static int s3c2416_pm_suspend(struct sys_device *dev, pm_message_t state)
{
	return 0;
}

static int s3c2416_pm_resume(struct sys_device *dev)
{
	/* unset the return-from-sleep amd inform flags */
	__raw_writel(0x0, S3C2443_PWRMODE);
	__raw_writel(0x0, S3C2412_INFORM0);
	__raw_writel(0x0, S3C2412_INFORM1);

	return 0;
}

static struct sysdev_driver s3c2416_pm_driver = {
	.add		= s3c2416_pm_add,
	.suspend	= s3c2416_pm_suspend,
	.resume		= s3c2416_pm_resume,
};

static __init int s3c2416_pm_init(void)
{
	return sysdev_driver_register(&s3c2416_sysclass, &s3c2416_pm_driver);
}

arch_initcall(s3c2416_pm_init);
