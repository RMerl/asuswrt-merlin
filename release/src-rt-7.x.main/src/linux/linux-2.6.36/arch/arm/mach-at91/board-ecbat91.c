/*
 * linux/arch/arm/mach-at91rm9200/board-ecbat91.c
 * Copyright (C) 2007 emQbit.com.
 *
 * We started from board-dk.c, which is Copyright (C) 2005 SAN People.
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/types.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <linux/spi/flash.h>

#include <mach/hardware.h>
#include <asm/setup.h>
#include <asm/mach-types.h>
#include <asm/irq.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/irq.h>

#include <mach/board.h>
#include <mach/gpio.h>

#include "generic.h"


static void __init ecb_at91map_io(void)
{
	/* Initialize processor: 18.432 MHz crystal */
	at91rm9200_initialize(18432000, AT91RM9200_PQFP);

	/* Setup the LEDs */
	at91_init_leds(AT91_PIN_PC7, AT91_PIN_PC7);

	/* DBGU on ttyS0. (Rx & Tx only) */
	at91_register_uart(0, 0, 0);

	/* USART0 on ttyS1. (Rx & Tx only) */
	at91_register_uart(AT91RM9200_ID_US0, 1, 0);

	/* set serial console to ttyS0 (ie, DBGU) */
	at91_set_serial_console(0);
}

static void __init ecb_at91init_irq(void)
{
	at91rm9200_init_interrupts(NULL);
}

static struct at91_eth_data __initdata ecb_at91eth_data = {
	.phy_irq_pin	= AT91_PIN_PC4,
	.is_rmii	= 0,
};

static struct at91_usbh_data __initdata ecb_at91usbh_data = {
	.ports		= 1,
};

static struct at91_mmc_data __initdata ecb_at91mmc_data = {
	.slot_b		= 0,
	.wire4		= 1,
};


#if defined(CONFIG_MTD_DATAFLASH)
static struct mtd_partition __initdata my_flash0_partitions[] =
{
	{	/* 0x8400 */
		.name	= "Darrell-loader",
		.offset	= 0,
		.size	= 12 * 1056,
	},
	{
		.name	= "U-boot",
		.offset	= MTDPART_OFS_NXTBLK,
		.size	= 110 * 1056,
	},
	{	/* 1336 (167 blocks) pages * 1056 bytes = 0x158700 bytes */
		.name	= "UBoot-env",
		.offset	= MTDPART_OFS_NXTBLK,
		.size	= 8 * 1056,
	},
	{	/* 1336 (167 blocks) pages * 1056 bytes = 0x158700 bytes */
		.name	= "Kernel",
		.offset	= MTDPART_OFS_NXTBLK,
		.size	= 1534 * 1056,
	},
	{	/* 190200 - jffs2 root filesystem */
		.name	= "Filesystem",
		.offset	= MTDPART_OFS_NXTBLK,
		.size	= MTDPART_SIZ_FULL,	/* 26 sectors */
	}
};

static struct flash_platform_data __initdata my_flash0_platform = {
	.name		= "Removable flash card",
	.parts		= my_flash0_partitions,
	.nr_parts	= ARRAY_SIZE(my_flash0_partitions)
};

#endif

static struct spi_board_info __initdata ecb_at91spi_devices[] = {
	{	/* DataFlash chip */
		.modalias	= "mtd_dataflash",
		.chip_select	= 0,
		.max_speed_hz	= 10 * 1000 * 1000,
		.bus_num	= 0,
#if defined(CONFIG_MTD_DATAFLASH)
		.platform_data	= &my_flash0_platform,
#endif
	},
	{	/* User accessable spi - cs1 (250KHz) */
		.modalias	= "spi-cs1",
		.chip_select	= 1,
		.max_speed_hz	= 250 * 1000,
	},
	{	/* User accessable spi - cs2 (1MHz) */
		.modalias	= "spi-cs2",
		.chip_select	= 2,
		.max_speed_hz	= 1 * 1000 * 1000,
	},
	{	/* User accessable spi - cs3 (10MHz) */
		.modalias	= "spi-cs3",
		.chip_select	= 3,
		.max_speed_hz	= 10 * 1000 * 1000,
	},
};

static void __init ecb_at91board_init(void)
{
	/* Serial */
	at91_add_device_serial();

	/* Ethernet */
	at91_add_device_eth(&ecb_at91eth_data);

	/* USB Host */
	at91_add_device_usbh(&ecb_at91usbh_data);

	/* I2C */
	at91_add_device_i2c(NULL, 0);

	/* MMC */
	at91_add_device_mmc(0, &ecb_at91mmc_data);

	/* SPI */
	at91_add_device_spi(ecb_at91spi_devices, ARRAY_SIZE(ecb_at91spi_devices));
}

MACHINE_START(ECBAT91, "emQbit's ECB_AT91")
	/* Maintainer: emQbit.com */
	.phys_io	= AT91_BASE_SYS,
	.io_pg_offst	= (AT91_VA_BASE_SYS >> 18) & 0xfffc,
	.boot_params	= AT91_SDRAM_BASE + 0x100,
	.timer		= &at91rm9200_timer,
	.map_io		= ecb_at91map_io,
	.init_irq	= ecb_at91init_irq,
	.init_machine	= ecb_at91board_init,
MACHINE_END
