/*
 * linux/arch/arm/mach-at91/board-flexibity.c
 *
 *  Copyright (C) 2010 Flexibity
 *  Copyright (C) 2005 SAN People
 *  Copyright (C) 2006 Atmel
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

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <linux/input.h>
#include <linux/gpio.h>

#include <asm/mach-types.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/irq.h>

#include <mach/hardware.h>
#include <mach/board.h>

#include "generic.h"

static void __init flexibity_map_io(void)
{
	/* Initialize processor: 18.432 MHz crystal */
	at91sam9260_initialize(18432000);

	/* DBGU on ttyS0. (Rx & Tx only) */
	at91_register_uart(0, 0, 0);

	/* set serial console to ttyS0 (ie, DBGU) */
	at91_set_serial_console(0);
}

static void __init flexibity_init_irq(void)
{
	at91sam9260_init_interrupts(NULL);
}

/* USB Host port */
static struct at91_usbh_data __initdata flexibity_usbh_data = {
	.ports		= 2,
};

/* USB Device port */
static struct at91_udc_data __initdata flexibity_udc_data = {
	.vbus_pin	= AT91_PIN_PC5,
	.pullup_pin	= 0,		/* pull-up driven by UDC */
};

/* SPI devices */
static struct spi_board_info flexibity_spi_devices[] = {
	{	/* DataFlash chip */
		.modalias	= "mtd_dataflash",
		.chip_select	= 1,
		.max_speed_hz	= 15 * 1000 * 1000,
		.bus_num	= 0,
	},
};

/* MCI (SD/MMC) */
static struct at91_mmc_data __initdata flexibity_mmc_data = {
	.slot_b		= 0,
	.wire4		= 1,
	.det_pin	= AT91_PIN_PC9,
	.wp_pin		= AT91_PIN_PC4,
};

/* LEDs */
static struct gpio_led flexibity_leds[] = {
	{
		.name			= "usb1:green",
		.gpio			= AT91_PIN_PA12,
		.active_low		= 1,
		.default_trigger	= "default-on",
	},
	{
		.name			= "usb1:red",
		.gpio			= AT91_PIN_PA13,
		.active_low		= 1,
		.default_trigger	= "default-on",
	},
	{
		.name			= "usb2:green",
		.gpio			= AT91_PIN_PB26,
		.active_low		= 1,
		.default_trigger	= "default-on",
	},
	{
		.name			= "usb2:red",
		.gpio			= AT91_PIN_PB27,
		.active_low		= 1,
		.default_trigger	= "default-on",
	},
	{
		.name			= "usb3:green",
		.gpio			= AT91_PIN_PC8,
		.active_low		= 1,
		.default_trigger	= "default-on",
	},
	{
		.name			= "usb3:red",
		.gpio			= AT91_PIN_PC6,
		.active_low		= 1,
		.default_trigger	= "default-on",
	},
	{
		.name			= "usb4:green",
		.gpio			= AT91_PIN_PB4,
		.active_low		= 1,
		.default_trigger	= "default-on",
	},
	{
		.name			= "usb4:red",
		.gpio			= AT91_PIN_PB5,
		.active_low		= 1,
		.default_trigger	= "default-on",
	}
};

static void __init flexibity_board_init(void)
{
	/* Serial */
	at91_add_device_serial();
	/* USB Host */
	at91_add_device_usbh(&flexibity_usbh_data);
	/* USB Device */
	at91_add_device_udc(&flexibity_udc_data);
	/* SPI */
	at91_add_device_spi(flexibity_spi_devices,
		ARRAY_SIZE(flexibity_spi_devices));
	/* MMC */
	at91_add_device_mmc(0, &flexibity_mmc_data);
	/* LEDs */
	at91_gpio_leds(flexibity_leds, ARRAY_SIZE(flexibity_leds));
}

MACHINE_START(FLEXIBITY, "Flexibity Connect")
	/* Maintainer: Maxim Osipov */
	.boot_params	= AT91_SDRAM_BASE + 0x100,
	.timer		= &at91sam926x_timer,
	.map_io		= flexibity_map_io,
	.init_irq	= flexibity_init_irq,
	.init_machine	= flexibity_board_init,
MACHINE_END
