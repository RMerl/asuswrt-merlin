/*
 * linux/arch/arm/mach-omap2/board-omap3logic.c
 *
 * Copyright (C) 2010 Li-Pro.Net
 * Stephan Linz <linz@li-pro.net>
 *
 * Copyright (C) 2010 Logic Product Development, Inc.
 * Peter Barada <peter.barada@logicpd.com>
 *
 * Modified from Beagle, EVM, and RX51
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/gpio.h>

#include <linux/regulator/machine.h>

#include <linux/i2c/twl.h>
#include <linux/mmc/host.h>

#include <mach/hardware.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>

#include "mux.h"
#include "hsmmc.h"
#include "timer-gp.h"
#include "control.h"

#include <plat/mux.h>
#include <plat/board.h>
#include <plat/common.h>
#include <plat/gpmc-smsc911x.h>
#include <plat/gpmc.h>
#include <plat/sdrc.h>

#define OMAP3LOGIC_SMSC911X_CS			1

#define OMAP3530_LV_SOM_MMC_GPIO_CD		110
#define OMAP3530_LV_SOM_MMC_GPIO_WP		126
#define OMAP3530_LV_SOM_SMSC911X_GPIO_IRQ	152

#define OMAP3_TORPEDO_MMC_GPIO_CD		127
#define OMAP3_TORPEDO_SMSC911X_GPIO_IRQ		129

static struct regulator_consumer_supply omap3logic_vmmc1_supply = {
	.supply			= "vmmc",
};

/* VMMC1 for MMC1 pins CMD, CLK, DAT0..DAT3 (20 mA, plus card == max 220 mA) */
static struct regulator_init_data omap3logic_vmmc1 = {
	.constraints = {
		.name			= "VMMC1",
		.min_uV			= 1850000,
		.max_uV			= 3150000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies  = 1,
	.consumer_supplies      = &omap3logic_vmmc1_supply,
};

static struct twl4030_gpio_platform_data omap3logic_gpio_data = {
	.gpio_base	= OMAP_MAX_GPIO_LINES,
	.irq_base	= TWL4030_GPIO_IRQ_BASE,
	.irq_end	= TWL4030_GPIO_IRQ_END,
	.use_leds	= true,
	.pullups	= BIT(1),
	.pulldowns	= BIT(2)  | BIT(6)  | BIT(7)  | BIT(8)
			| BIT(13) | BIT(15) | BIT(16) | BIT(17),
};

static struct twl4030_platform_data omap3logic_twldata = {
	.irq_base	= TWL4030_IRQ_BASE,
	.irq_end	= TWL4030_IRQ_END,

	/* platform_data for children goes here */
	.gpio		= &omap3logic_gpio_data,
	.vmmc1		= &omap3logic_vmmc1,
};

static struct i2c_board_info __initdata omap3logic_i2c_boardinfo[] = {
	{
		I2C_BOARD_INFO("twl4030", 0x48),
		.flags = I2C_CLIENT_WAKE,
		.irq = INT_34XX_SYS_NIRQ,
		.platform_data = &omap3logic_twldata,
	},
};

static int __init omap3logic_i2c_init(void)
{
	omap_register_i2c_bus(1, 2600, omap3logic_i2c_boardinfo,
				ARRAY_SIZE(omap3logic_i2c_boardinfo));
	return 0;
}

static struct omap2_hsmmc_info __initdata board_mmc_info[] = {
	{
		.name		= "external",
		.mmc		= 1,
		.caps		= MMC_CAP_4_BIT_DATA,
		.gpio_cd	= -EINVAL,
		.gpio_wp	= -EINVAL,
	},
	{}      /* Terminator */
};

static void __init board_mmc_init(void)
{
	if (machine_is_omap3530_lv_som()) {
		/* OMAP3530 LV SOM board */
		board_mmc_info[0].gpio_cd = OMAP3530_LV_SOM_MMC_GPIO_CD;
		board_mmc_info[0].gpio_wp = OMAP3530_LV_SOM_MMC_GPIO_WP;
		omap_mux_init_signal("gpio_110", OMAP_PIN_OUTPUT);
		omap_mux_init_signal("gpio_126", OMAP_PIN_OUTPUT);
	} else if (machine_is_omap3_torpedo()) {
		/* OMAP3 Torpedo board */
		board_mmc_info[0].gpio_cd = OMAP3_TORPEDO_MMC_GPIO_CD;
		omap_mux_init_signal("gpio_127", OMAP_PIN_OUTPUT);
	} else {
		/* unsupported board */
		printk(KERN_ERR "%s(): unknown machine type\n", __func__);
		return;
	}

	omap2_hsmmc_init(board_mmc_info);
	/* link regulators to MMC adapters */
	omap3logic_vmmc1_supply.dev = board_mmc_info[0].dev;
}

static struct omap_smsc911x_platform_data __initdata board_smsc911x_data = {
	.cs             = OMAP3LOGIC_SMSC911X_CS,
	.gpio_irq       = -EINVAL,
	.gpio_reset     = -EINVAL,
	.flags          = IORESOURCE_IRQ_LOWLEVEL,
};

/* TODO/FIXME (comment by Peter Barada, LogicPD):
 * Fix the PBIAS voltage for Torpedo MMC1 pins that
 * are used for other needs (IRQs, etc).            */
static void omap3torpedo_fix_pbias_voltage(void)
{
	u16 control_pbias_offset = OMAP343X_CONTROL_PBIAS_LITE;
	u32 reg;

	if (machine_is_omap3_torpedo())
	{
		/* Set the bias for the pin */
		reg = omap_ctrl_readl(control_pbias_offset);

		reg &= ~OMAP343X_PBIASLITEPWRDNZ1;
		omap_ctrl_writel(reg, control_pbias_offset);

		/* 100ms delay required for PBIAS configuration */
		msleep(100);

		reg |= OMAP343X_PBIASLITEVMODE1;
		reg |= OMAP343X_PBIASLITEPWRDNZ1;
		omap_ctrl_writel(reg | 0x300, control_pbias_offset);
	}
}

static inline void __init board_smsc911x_init(void)
{
	if (machine_is_omap3530_lv_som()) {
		/* OMAP3530 LV SOM board */
		board_smsc911x_data.gpio_irq =
					OMAP3530_LV_SOM_SMSC911X_GPIO_IRQ;
		omap_mux_init_signal("gpio_152", OMAP_PIN_INPUT);
	} else if (machine_is_omap3_torpedo()) {
		/* OMAP3 Torpedo board */
		board_smsc911x_data.gpio_irq = OMAP3_TORPEDO_SMSC911X_GPIO_IRQ;
		omap_mux_init_signal("gpio_129", OMAP_PIN_INPUT);
	} else {
		/* unsupported board */
		printk(KERN_ERR "%s(): unknown machine type\n", __func__);
		return;
	}

	gpmc_smsc911x_init(&board_smsc911x_data);
}

static void __init omap3logic_init_early(void)
{
	omap2_init_common_infrastructure();
	omap2_init_common_devices(NULL, NULL);
}

#ifdef CONFIG_OMAP_MUX
static struct omap_board_mux board_mux[] __initdata = {
	{ .reg_offset = OMAP_MUX_TERMINATOR },
};
#endif

static void __init omap3logic_init(void)
{
	omap3_mux_init(board_mux, OMAP_PACKAGE_CBB);
	omap3torpedo_fix_pbias_voltage();
	omap3logic_i2c_init();
	omap_serial_init();
	board_mmc_init();
	board_smsc911x_init();

	/* Ensure SDRC pins are mux'd for self-refresh */
	omap_mux_init_signal("sdrc_cke0", OMAP_PIN_OUTPUT);
	omap_mux_init_signal("sdrc_cke1", OMAP_PIN_OUTPUT);
}

MACHINE_START(OMAP3_TORPEDO, "Logic OMAP3 Torpedo board")
	.boot_params	= 0x80000100,
	.map_io		= omap3_map_io,
	.init_early	= omap3logic_init_early,
	.init_irq	= omap_init_irq,
	.init_machine	= omap3logic_init,
	.timer		= &omap_timer,
MACHINE_END

MACHINE_START(OMAP3530_LV_SOM, "OMAP Logic 3530 LV SOM board")
	.boot_params	= 0x80000100,
	.map_io		= omap3_map_io,
	.init_early	= omap3logic_init_early,
	.init_irq	= omap_init_irq,
	.init_machine	= omap3logic_init,
	.timer		= &omap_timer,
MACHINE_END
