/*
 *  Atheros AR71XX/AR724X/AR913X specific setup
 *
 *  Copyright (C) 2008-2011 Gabor Juhos <juhosg@openwrt.org>
 *  Copyright (C) 2008 Imre Kaloz <kaloz@openwrt.org>
 *
 *  Parts of this file are based on Atheros' 2.6.15 BSP
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version 2 as published
 *  by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/bootmem.h>
#include <linux/err.h>
#include <linux/clk.h>

#include <asm/bootinfo.h>
#include <asm/time.h>		/* for mips_hpt_frequency */
#include <asm/reboot.h>		/* for _machine_{restart,halt} */
#include <asm/mips_machine.h>

#include <asm/mach-ath79/ath79.h>
#include <asm/mach-ath79/ar71xx_regs.h>
#include "common.h"
#include "dev-common.h"
#include "machtypes.h"

#define ATH79_SYS_TYPE_LEN	64

#define AR71XX_BASE_FREQ	40000000
#define AR724X_BASE_FREQ	5000000
#define AR913X_BASE_FREQ	5000000

static char ath79_sys_type[ATH79_SYS_TYPE_LEN];

static void ath79_restart(char *command)
{
	ath79_device_reset_set(AR71XX_RESET_FULL_CHIP);
	for (;;)
		if (cpu_wait)
			cpu_wait();
}

static void ath79_halt(void)
{
	while (1)
		cpu_wait();
}

static void __init ath79_detect_mem_size(void)
{
	unsigned long size;

	for (size = ATH79_MEM_SIZE_MIN; size < ATH79_MEM_SIZE_MAX;
	     size <<= 1) {
		if (!memcmp(ath79_detect_mem_size,
			    ath79_detect_mem_size + size, 1024))
			break;
	}

	add_memory_region(0, size, BOOT_MEM_RAM);
}

static void __init ath79_detect_sys_type(void)
{
	char *chip = "????";
	u32 id;
	u32 major;
	u32 minor;
	u32 rev = 0;

	id = ath79_reset_rr(AR71XX_RESET_REG_REV_ID);
	major = id & REV_ID_MAJOR_MASK;

	switch (major) {
	case REV_ID_MAJOR_AR71XX:
		minor = id & AR71XX_REV_ID_MINOR_MASK;
		rev = id >> AR71XX_REV_ID_REVISION_SHIFT;
		rev &= AR71XX_REV_ID_REVISION_MASK;
		switch (minor) {
		case AR71XX_REV_ID_MINOR_AR7130:
			ath79_soc = ATH79_SOC_AR7130;
			chip = "7130";
			break;

		case AR71XX_REV_ID_MINOR_AR7141:
			ath79_soc = ATH79_SOC_AR7141;
			chip = "7141";
			break;

		case AR71XX_REV_ID_MINOR_AR7161:
			ath79_soc = ATH79_SOC_AR7161;
			chip = "7161";
			break;
		}
		break;

	case REV_ID_MAJOR_AR7240:
		ath79_soc = ATH79_SOC_AR7240;
		chip = "7240";
		rev = (id & AR724X_REV_ID_REVISION_MASK);
		break;

	case REV_ID_MAJOR_AR7241:
		ath79_soc = ATH79_SOC_AR7241;
		chip = "7241";
		rev = (id & AR724X_REV_ID_REVISION_MASK);
		break;

	case REV_ID_MAJOR_AR7242:
		ath79_soc = ATH79_SOC_AR7242;
		chip = "7242";
		rev = (id & AR724X_REV_ID_REVISION_MASK);
		break;

	case REV_ID_MAJOR_AR913X:
		minor = id & AR913X_REV_ID_MINOR_MASK;
		rev = id >> AR913X_REV_ID_REVISION_SHIFT;
		rev &= AR913X_REV_ID_REVISION_MASK;
		switch (minor) {
		case AR913X_REV_ID_MINOR_AR9130:
			ath79_soc = ATH79_SOC_AR9130;
			chip = "9130";
			break;

		case AR913X_REV_ID_MINOR_AR9132:
			ath79_soc = ATH79_SOC_AR9132;
			chip = "9132";
			break;
		}
		break;

	default:
		panic("ath79: unknown SoC, id:0x%08x\n", id);
	}

	sprintf(ath79_sys_type, "Atheros AR%s rev %u", chip, rev);
	pr_info("SoC: %s\n", ath79_sys_type);
}

const char *get_system_type(void)
{
	return ath79_sys_type;
}

unsigned int __cpuinit get_c0_compare_int(void)
{
	return CP0_LEGACY_COMPARE_IRQ;
}

void __init plat_mem_setup(void)
{
	set_io_port_base(KSEG1);

	ath79_reset_base = ioremap_nocache(AR71XX_RESET_BASE,
					   AR71XX_RESET_SIZE);
	ath79_pll_base = ioremap_nocache(AR71XX_PLL_BASE,
					 AR71XX_PLL_SIZE);
	ath79_ddr_base = ioremap_nocache(AR71XX_DDR_CTRL_BASE,
					 AR71XX_DDR_CTRL_SIZE);

	ath79_detect_sys_type();
	ath79_detect_mem_size();
	ath79_clocks_init();

	_machine_restart = ath79_restart;
	_machine_halt = ath79_halt;
	pm_power_off = ath79_halt;
}

void __init plat_time_init(void)
{
	struct clk *clk;

	clk = clk_get(NULL, "cpu");
	if (IS_ERR(clk))
		panic("unable to get CPU clock, err=%ld", PTR_ERR(clk));

	mips_hpt_frequency = clk_get_rate(clk) / 2;
}

static int __init ath79_setup(void)
{
	ath79_gpio_init();
	ath79_register_uart();
	ath79_register_wdt();

	mips_machine_setup();

	return 0;
}

arch_initcall(ath79_setup);

static void __init ath79_generic_init(void)
{
	/* Nothing to do */
}

MIPS_MACHINE(ATH79_MACH_GENERIC,
	     "Generic",
	     "Generic AR71XX/AR724X/AR913X based board",
	     ath79_generic_init);
