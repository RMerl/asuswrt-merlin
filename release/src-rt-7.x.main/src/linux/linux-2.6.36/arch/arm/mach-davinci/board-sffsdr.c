/*
 * Lyrtech SFFSDR board support.
 *
 * Copyright (C) 2008 Philip Balister, OpenSDR <philip@opensdr.com>
 * Copyright (C) 2008 Lyrtech <www.lyrtech.com>
 *
 * Based on DV-EVM platform, original copyright follows:
 *
 * Copyright (C) 2007 MontaVista Software, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/i2c/at24.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/flash.h>

#include <mach/dm644x.h>
#include <mach/common.h>
#include <mach/i2c.h>
#include <mach/serial.h>
#include <mach/mux.h>
#include <mach/usb.h>

#define SFFSDR_PHY_MASK		(0x2)
#define SFFSDR_MDIO_FREQUENCY	(2200000) /* PHY bus frequency */

static struct mtd_partition davinci_sffsdr_nandflash_partition[] = {
	/* U-Boot Environment: Block 0
	 * UBL:                Block 1
	 * U-Boot:             Blocks 6-7 (256 kb)
	 * Integrity Kernel:   Blocks 8-31 (3 Mb)
	 * Integrity Data:     Blocks 100-END
	 */
	{
		.name		= "Linux Kernel",
		.offset		= 32 * SZ_128K,
		.size		= 16 * SZ_128K, /* 2 Mb */
		.mask_flags	= MTD_WRITEABLE, /* Force read-only */
	},
	{
		.name		= "Linux ROOT",
		.offset		= MTDPART_OFS_APPEND,
		.size		= 256 * SZ_128K, /* 32 Mb */
		.mask_flags	= 0, /* R/W */
	},
};

static struct flash_platform_data davinci_sffsdr_nandflash_data = {
	.parts		= davinci_sffsdr_nandflash_partition,
	.nr_parts	= ARRAY_SIZE(davinci_sffsdr_nandflash_partition),
};

static struct resource davinci_sffsdr_nandflash_resource[] = {
	{
		.start		= DM644X_ASYNC_EMIF_DATA_CE0_BASE,
		.end		= DM644X_ASYNC_EMIF_DATA_CE0_BASE + SZ_16M - 1,
		.flags		= IORESOURCE_MEM,
	}, {
		.start		= DM644X_ASYNC_EMIF_CONTROL_BASE,
		.end		= DM644X_ASYNC_EMIF_CONTROL_BASE + SZ_4K - 1,
		.flags		= IORESOURCE_MEM,
	},
};

static struct platform_device davinci_sffsdr_nandflash_device = {
	.name		= "davinci_nand", /* Name of driver */
	.id		= 0,
	.dev		= {
		.platform_data	= &davinci_sffsdr_nandflash_data,
	},
	.num_resources	= ARRAY_SIZE(davinci_sffsdr_nandflash_resource),
	.resource	= davinci_sffsdr_nandflash_resource,
};

static struct at24_platform_data eeprom_info = {
	.byte_len	= (64*1024) / 8,
	.page_size	= 32,
	.flags		= AT24_FLAG_ADDR16,
};

static struct i2c_board_info __initdata i2c_info[] =  {
	{
		I2C_BOARD_INFO("24lc64", 0x50),
		.platform_data	= &eeprom_info,
	},
	/* Other I2C devices:
	 * MSP430,  addr 0x23 (not used)
	 * PCA9543, addr 0x70 (setup done by U-Boot)
	 * ADS7828, addr 0x48 (ADC for voltage monitoring.)
	 */
};

static struct davinci_i2c_platform_data i2c_pdata = {
	.bus_freq	= 20 /* kHz */,
	.bus_delay	= 100 /* usec */,
};

static void __init sffsdr_init_i2c(void)
{
	davinci_init_i2c(&i2c_pdata);
	i2c_register_board_info(1, i2c_info, ARRAY_SIZE(i2c_info));
}

static struct platform_device *davinci_sffsdr_devices[] __initdata = {
	&davinci_sffsdr_nandflash_device,
};

static struct davinci_uart_config uart_config __initdata = {
	.enabled_uarts = (1 << 0),
};

static void __init davinci_sffsdr_map_io(void)
{
	dm644x_init();
}

static __init void davinci_sffsdr_init(void)
{
	struct davinci_soc_info *soc_info = &davinci_soc_info;

	platform_add_devices(davinci_sffsdr_devices,
			     ARRAY_SIZE(davinci_sffsdr_devices));
	sffsdr_init_i2c();
	davinci_serial_init(&uart_config);
	soc_info->emac_pdata->phy_mask = SFFSDR_PHY_MASK;
	soc_info->emac_pdata->mdio_max_freq = SFFSDR_MDIO_FREQUENCY;
	davinci_setup_usb(0, 0); /* We support only peripheral mode. */

	/* mux VLYNQ pins */
	davinci_cfg_reg(DM644X_VLYNQEN);
	davinci_cfg_reg(DM644X_VLYNQWD);
}

MACHINE_START(SFFSDR, "Lyrtech SFFSDR")
	/* Maintainer: Hugo Villeneuve hugo.villeneuve@lyrtech.com */
	.phys_io      = IO_PHYS,
	.io_pg_offst  = (__IO_ADDRESS(IO_PHYS) >> 18) & 0xfffc,
	.boot_params  = (DAVINCI_DDR_BASE + 0x100),
	.map_io	      = davinci_sffsdr_map_io,
	.init_irq     = davinci_irq_init,
	.timer	      = &davinci_timer,
	.init_machine = davinci_sffsdr_init,
MACHINE_END
