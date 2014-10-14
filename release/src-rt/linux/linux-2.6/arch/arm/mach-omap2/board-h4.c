/*
 * linux/arch/arm/mach-omap2/board-h4.c
 *
 * Copyright (C) 2005 Nokia Corporation
 * Author: Paul Mundt <paul.mundt@nokia.com>
 *
 * Modified from mach-omap/omap1/board-generic.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/physmap.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/i2c.h>
#include <linux/i2c/at24.h>
#include <linux/input.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>

#include <mach/hardware.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>

#include <mach/gpio.h>
#include <plat/usb.h>
#include <plat/board.h>
#include <plat/common.h>
#include <plat/keypad.h>
#include <plat/menelaus.h>
#include <plat/dma.h>
#include <plat/gpmc.h>

#include "mux.h"
#include "control.h"

#define H4_FLASH_CS	0
#define H4_SMC91X_CS	1

#define H4_ETHR_GPIO_IRQ		92

static unsigned int row_gpios[6] = { 88, 89, 124, 11, 6, 96 };
static unsigned int col_gpios[7] = { 90, 91, 100, 36, 12, 97, 98 };

static const unsigned int h4_keymap[] = {
	KEY(0, 0, KEY_LEFT),
	KEY(1, 0, KEY_RIGHT),
	KEY(2, 0, KEY_A),
	KEY(3, 0, KEY_B),
	KEY(4, 0, KEY_C),
	KEY(0, 1, KEY_DOWN),
	KEY(1, 1, KEY_UP),
	KEY(2, 1, KEY_E),
	KEY(3, 1, KEY_F),
	KEY(4, 1, KEY_G),
	KEY(0, 2, KEY_ENTER),
	KEY(1, 2, KEY_I),
	KEY(2, 2, KEY_J),
	KEY(3, 2, KEY_K),
	KEY(4, 2, KEY_3),
	KEY(0, 3, KEY_M),
	KEY(1, 3, KEY_N),
	KEY(2, 3, KEY_O),
	KEY(3, 3, KEY_P),
	KEY(4, 3, KEY_Q),
	KEY(0, 4, KEY_R),
	KEY(1, 4, KEY_4),
	KEY(2, 4, KEY_T),
	KEY(3, 4, KEY_U),
	KEY(4, 4, KEY_ENTER),
	KEY(0, 5, KEY_V),
	KEY(1, 5, KEY_W),
	KEY(2, 5, KEY_L),
	KEY(3, 5, KEY_S),
	KEY(4, 5, KEY_ENTER),
};

static struct mtd_partition h4_partitions[] = {
	/* bootloader (U-Boot, etc) in first sector */
	{
	      .name		= "bootloader",
	      .offset		= 0,
	      .size		= SZ_128K,
	      .mask_flags	= MTD_WRITEABLE, /* force read-only */
	},
	/* bootloader params in the next sector */
	{
	      .name		= "params",
	      .offset		= MTDPART_OFS_APPEND,
	      .size		= SZ_128K,
	      .mask_flags	= 0,
	},
	/* kernel */
	{
	      .name		= "kernel",
	      .offset		= MTDPART_OFS_APPEND,
	      .size		= SZ_2M,
	      .mask_flags	= 0
	},
	/* file system */
	{
	      .name		= "filesystem",
	      .offset		= MTDPART_OFS_APPEND,
	      .size		= MTDPART_SIZ_FULL,
	      .mask_flags	= 0
	}
};

static struct physmap_flash_data h4_flash_data = {
	.width		= 2,
	.parts		= h4_partitions,
	.nr_parts	= ARRAY_SIZE(h4_partitions),
};

static struct resource h4_flash_resource = {
	.flags		= IORESOURCE_MEM,
};

static struct platform_device h4_flash_device = {
	.name		= "physmap-flash",
	.id		= 0,
	.dev		= {
		.platform_data	= &h4_flash_data,
	},
	.num_resources	= 1,
	.resource	= &h4_flash_resource,
};

static const struct matrix_keymap_data h4_keymap_data = {
	.keymap		= h4_keymap,
	.keymap_size	= ARRAY_SIZE(h4_keymap),
};

static struct omap_kp_platform_data h4_kp_data = {
	.rows		= 6,
	.cols		= 7,
	.keymap_data	= &h4_keymap_data,
	.rep		= true,
	.row_gpios 	= row_gpios,
	.col_gpios 	= col_gpios,
};

static struct platform_device h4_kp_device = {
	.name		= "omap-keypad",
	.id		= -1,
	.dev		= {
		.platform_data = &h4_kp_data,
	},
};

static struct platform_device h4_lcd_device = {
	.name		= "lcd_h4",
	.id		= -1,
};

static struct platform_device *h4_devices[] __initdata = {
	&h4_flash_device,
	&h4_kp_device,
	&h4_lcd_device,
};

/* 2420 Sysboot setup (2430 is different) */
static u32 get_sysboot_value(void)
{
	return (omap_ctrl_readl(OMAP24XX_CONTROL_STATUS) &
		(OMAP2_SYSBOOT_5_MASK | OMAP2_SYSBOOT_4_MASK |
		 OMAP2_SYSBOOT_3_MASK | OMAP2_SYSBOOT_2_MASK |
		 OMAP2_SYSBOOT_1_MASK | OMAP2_SYSBOOT_0_MASK));
}

/* H4-2420's always used muxed mode, H4-2422's always use non-muxed
 *
 * Note: OMAP-GIT doesn't correctly do is_cpu_omap2422 and is_cpu_omap2423
 *  correctly.  The macro needs to look at production_id not just hawkeye.
 */
static u32 is_gpmc_muxed(void)
{
	u32 mux;
	mux = get_sysboot_value();
	if ((mux & 0xF) == 0xd)
		return 1;	/* NAND config (could be either) */
	if (mux & 0x2)		/* if mux'ed */
		return 1;
	else
		return 0;
}

static inline void __init h4_init_debug(void)
{
	int eth_cs;
	unsigned long cs_mem_base;
	unsigned int muxed, rate;
	struct clk *gpmc_fck;

	eth_cs	= H4_SMC91X_CS;

	gpmc_fck = clk_get(NULL, "gpmc_fck");	/* Always on ENABLE_ON_INIT */
	if (IS_ERR(gpmc_fck)) {
		WARN_ON(1);
		return;
	}

	clk_enable(gpmc_fck);
	rate = clk_get_rate(gpmc_fck);
	clk_disable(gpmc_fck);
	clk_put(gpmc_fck);

	if (is_gpmc_muxed())
		muxed = 0x200;
	else
		muxed = 0;

	/* Make sure CS1 timings are correct */
	gpmc_cs_write_reg(eth_cs, GPMC_CS_CONFIG1,
			  0x00011000 | muxed);

	if (rate >= 160000000) {
		gpmc_cs_write_reg(eth_cs, GPMC_CS_CONFIG2, 0x001f1f01);
		gpmc_cs_write_reg(eth_cs, GPMC_CS_CONFIG3, 0x00080803);
		gpmc_cs_write_reg(eth_cs, GPMC_CS_CONFIG4, 0x1c0b1c0a);
		gpmc_cs_write_reg(eth_cs, GPMC_CS_CONFIG5, 0x041f1F1F);
		gpmc_cs_write_reg(eth_cs, GPMC_CS_CONFIG6, 0x000004C4);
	} else if (rate >= 130000000) {
		gpmc_cs_write_reg(eth_cs, GPMC_CS_CONFIG2, 0x001f1f00);
		gpmc_cs_write_reg(eth_cs, GPMC_CS_CONFIG3, 0x00080802);
		gpmc_cs_write_reg(eth_cs, GPMC_CS_CONFIG4, 0x1C091C09);
		gpmc_cs_write_reg(eth_cs, GPMC_CS_CONFIG5, 0x041f1F1F);
		gpmc_cs_write_reg(eth_cs, GPMC_CS_CONFIG6, 0x000004C4);
	} else {/* rate = 100000000 */
		gpmc_cs_write_reg(eth_cs, GPMC_CS_CONFIG2, 0x001f1f00);
		gpmc_cs_write_reg(eth_cs, GPMC_CS_CONFIG3, 0x00080802);
		gpmc_cs_write_reg(eth_cs, GPMC_CS_CONFIG4, 0x1C091C09);
		gpmc_cs_write_reg(eth_cs, GPMC_CS_CONFIG5, 0x031A1F1F);
		gpmc_cs_write_reg(eth_cs, GPMC_CS_CONFIG6, 0x000003C2);
	}

	if (gpmc_cs_request(eth_cs, SZ_16M, &cs_mem_base) < 0) {
		printk(KERN_ERR "Failed to request GPMC mem for smc91x\n");
		goto out;
	}

	udelay(100);

	omap_mux_init_gpio(92, 0);
	if (debug_card_init(cs_mem_base, H4_ETHR_GPIO_IRQ) < 0)
		gpmc_cs_free(eth_cs);

out:
	clk_disable(gpmc_fck);
	clk_put(gpmc_fck);
}

static void __init h4_init_flash(void)
{
	unsigned long base;

	if (gpmc_cs_request(H4_FLASH_CS, SZ_64M, &base) < 0) {
		printk("Can't request GPMC CS for flash\n");
		return;
	}
	h4_flash_resource.start	= base;
	h4_flash_resource.end	= base + SZ_64M - 1;
}

static struct omap_lcd_config h4_lcd_config __initdata = {
	.ctrl_name	= "internal",
};

static struct omap_usb_config h4_usb_config __initdata = {
	/* S1.10 OFF -- usb "download port"
	 * usb0 switched to Mini-B port and isp1105 transceiver;
	 * S2.POS3 = ON, S2.POS4 = OFF ... to enable battery charging
	 */
	.register_dev	= 1,
	.pins[0]	= 3,
/*	.hmc_mode	= 0x14,*/	/* 0:dev 1:host 2:disable */
	.hmc_mode	= 0x00,		/* 0:dev|otg 1:disable 2:disable */
};

static struct omap_board_config_kernel h4_config[] __initdata = {
	{ OMAP_TAG_LCD,		&h4_lcd_config },
};

static void __init omap_h4_init_early(void)
{
	omap2_init_common_infrastructure();
	omap2_init_common_devices(NULL, NULL);
}

static void __init omap_h4_init_irq(void)
{
	omap_init_irq();
}

static struct at24_platform_data m24c01 = {
	.byte_len	= SZ_1K / 8,
	.page_size	= 16,
};

static struct i2c_board_info __initdata h4_i2c_board_info[] = {
	{
		I2C_BOARD_INFO("isp1301_omap", 0x2d),
		.irq		= OMAP_GPIO_IRQ(125),
	},
	{	/* EEPROM on mainboard */
		I2C_BOARD_INFO("24c01", 0x52),
		.platform_data	= &m24c01,
	},
	{	/* EEPROM on cpu card */
		I2C_BOARD_INFO("24c01", 0x57),
		.platform_data	= &m24c01,
	},
};

#ifdef CONFIG_OMAP_MUX
static struct omap_board_mux board_mux[] __initdata = {
	{ .reg_offset = OMAP_MUX_TERMINATOR },
};
#endif

static void __init omap_h4_init(void)
{
	omap2420_mux_init(board_mux, OMAP_PACKAGE_ZAF);

	omap_board_config = h4_config;
	omap_board_config_size = ARRAY_SIZE(h4_config);

	/*
	 * Make sure the serial ports are muxed on at this point.
	 * You have to mux them off in device drivers later on
	 * if not needed.
	 */

#if defined(CONFIG_KEYBOARD_OMAP) || defined(CONFIG_KEYBOARD_OMAP_MODULE)
	omap_mux_init_gpio(88, OMAP_PULL_ENA | OMAP_PULL_UP);
	omap_mux_init_gpio(89, OMAP_PULL_ENA | OMAP_PULL_UP);
	omap_mux_init_gpio(124, OMAP_PULL_ENA | OMAP_PULL_UP);
	omap_mux_init_signal("mcbsp2_dr.gpio_11", OMAP_PULL_ENA | OMAP_PULL_UP);
	if (omap_has_menelaus()) {
		omap_mux_init_signal("sdrc_a14.gpio0",
			OMAP_PULL_ENA | OMAP_PULL_UP);
		omap_mux_init_signal("vlynq_rx0.gpio_15", 0);
		omap_mux_init_signal("gpio_98", 0);
		row_gpios[5] = 0;
		col_gpios[2] = 15;
		col_gpios[6] = 18;
	} else {
		omap_mux_init_signal("gpio_96", OMAP_PULL_ENA | OMAP_PULL_UP);
		omap_mux_init_signal("gpio_100", 0);
		omap_mux_init_signal("gpio_98", 0);
	}
	omap_mux_init_signal("gpio_90", 0);
	omap_mux_init_signal("gpio_91", 0);
	omap_mux_init_signal("gpio_36", 0);
	omap_mux_init_signal("mcbsp2_clkx.gpio_12", 0);
	omap_mux_init_signal("gpio_97", 0);
#endif

	i2c_register_board_info(1, h4_i2c_board_info,
			ARRAY_SIZE(h4_i2c_board_info));

	platform_add_devices(h4_devices, ARRAY_SIZE(h4_devices));
	omap2_usbfs_init(&h4_usb_config);
	omap_serial_init();
	h4_init_flash();
}

static void __init omap_h4_map_io(void)
{
	omap2_set_globals_242x();
	omap242x_map_common_io();
}

MACHINE_START(OMAP_H4, "OMAP2420 H4 board")
	/* Maintainer: Paul Mundt <paul.mundt@nokia.com> */
	.boot_params	= 0x80000100,
	.reserve	= omap_reserve,
	.map_io		= omap_h4_map_io,
	.init_early	= omap_h4_init_early,
	.init_irq	= omap_h4_init_irq,
	.init_machine	= omap_h4_init,
	.timer		= &omap_timer,
MACHINE_END
