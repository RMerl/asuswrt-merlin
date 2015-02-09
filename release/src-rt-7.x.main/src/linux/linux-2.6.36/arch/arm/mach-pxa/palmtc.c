/*
 * linux/arch/arm/mach-pxa/palmtc.c
 *
 * Support for the Palm Tungsten|C
 *
 * Author:	Marek Vasut <marek.vasut@gmail.com>
 *
 * Based on work of:
 *		Petr Blaha <p3t3@centrum.cz>
 *		Chetan S. Kumar <shivakumar.chetan@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/input.h>
#include <linux/pwm_backlight.h>
#include <linux/gpio.h>
#include <linux/input/matrix_keypad.h>
#include <linux/ucb1400.h>
#include <linux/power_supply.h>
#include <linux/gpio_keys.h>
#include <linux/mtd/physmap.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>

#include <mach/audio.h>
#include <mach/palmtc.h>
#include <mach/mmc.h>
#include <mach/pxafb.h>
#include <mach/mfp-pxa25x.h>
#include <mach/irda.h>
#include <mach/udc.h>
#include <mach/pxa2xx-regs.h>

#include "generic.h"
#include "devices.h"

/******************************************************************************
 * Pin configuration
 ******************************************************************************/
static unsigned long palmtc_pin_config[] __initdata = {
	/* MMC */
	GPIO6_MMC_CLK,
	GPIO8_MMC_CS0,
	GPIO12_GPIO,	/* detect */
	GPIO32_GPIO,	/* power */
	GPIO54_GPIO,	/* r/o switch */

	/* PCMCIA */
	GPIO52_nPCE_1,
	GPIO53_nPCE_2,
	GPIO50_nPIOR,
	GPIO51_nPIOW,
	GPIO49_nPWE,
	GPIO48_nPOE,
	GPIO52_nPCE_1,
	GPIO53_nPCE_2,
	GPIO57_nIOIS16,
	GPIO56_nPWAIT,

	/* AC97 */
	GPIO28_AC97_BITCLK,
	GPIO29_AC97_SDATA_IN_0,
	GPIO30_AC97_SDATA_OUT,
	GPIO31_AC97_SYNC,

	/* IrDA */
	GPIO45_GPIO,	/* ir disable */
	GPIO46_FICP_RXD,
	GPIO47_FICP_TXD,

	/* PWM */
	GPIO17_PWM1_OUT,

	/* USB */
	GPIO4_GPIO,	/* detect */
	GPIO36_GPIO,	/* pullup */

	/* LCD */
	GPIOxx_LCD_TFT_16BPP,

	/* MATRIX KEYPAD */
	GPIO0_GPIO | WAKEUP_ON_EDGE_BOTH,	/* in 0 */
	GPIO9_GPIO | WAKEUP_ON_EDGE_BOTH,	/* in 1 */
	GPIO10_GPIO | WAKEUP_ON_EDGE_BOTH,	/* in 2 */
	GPIO11_GPIO | WAKEUP_ON_EDGE_BOTH,	/* in 3 */
	GPIO18_GPIO | MFP_LPM_DRIVE_LOW,	/* out 0 */
	GPIO19_GPIO | MFP_LPM_DRIVE_LOW,	/* out 1 */
	GPIO20_GPIO | MFP_LPM_DRIVE_LOW,	/* out 2 */
	GPIO21_GPIO | MFP_LPM_DRIVE_LOW,	/* out 3 */
	GPIO22_GPIO | MFP_LPM_DRIVE_LOW,	/* out 4 */
	GPIO23_GPIO | MFP_LPM_DRIVE_LOW,	/* out 5 */
	GPIO24_GPIO | MFP_LPM_DRIVE_LOW,	/* out 6 */
	GPIO25_GPIO | MFP_LPM_DRIVE_LOW,	/* out 7 */
	GPIO26_GPIO | MFP_LPM_DRIVE_LOW,	/* out 8 */
	GPIO27_GPIO | MFP_LPM_DRIVE_LOW,	/* out 9 */
	GPIO79_GPIO | MFP_LPM_DRIVE_LOW,	/* out 10 */
	GPIO80_GPIO | MFP_LPM_DRIVE_LOW,	/* out 11 */

	/* PXA GPIO KEYS */
	GPIO7_GPIO | WAKEUP_ON_EDGE_BOTH,	/* hotsync button on cradle */

	/* MISC */
	GPIO1_RST,	/* reset */
	GPIO2_GPIO,	/* earphone detect */
	GPIO16_GPIO,	/* backlight switch */
};

/******************************************************************************
 * SD/MMC card controller
 ******************************************************************************/
static struct pxamci_platform_data palmtc_mci_platform_data = {
	.ocr_mask		= MMC_VDD_32_33 | MMC_VDD_33_34,
	.gpio_power		= GPIO_NR_PALMTC_SD_POWER,
	.gpio_card_ro		= GPIO_NR_PALMTC_SD_READONLY,
	.gpio_card_detect	= GPIO_NR_PALMTC_SD_DETECT_N,
	.detect_delay_ms	= 200,
};

/******************************************************************************
 * GPIO keys
 ******************************************************************************/
static struct gpio_keys_button palmtc_pxa_buttons[] = {
	{KEY_F8, GPIO_NR_PALMTC_HOTSYNC_BUTTON, 1, "HotSync Button", EV_KEY, 1},
};

static struct gpio_keys_platform_data palmtc_pxa_keys_data = {
	.buttons	= palmtc_pxa_buttons,
	.nbuttons	= ARRAY_SIZE(palmtc_pxa_buttons),
};

static struct platform_device palmtc_pxa_keys = {
	.name	= "gpio-keys",
	.id	= -1,
	.dev	= {
		.platform_data = &palmtc_pxa_keys_data,
	},
};

/******************************************************************************
 * Backlight
 ******************************************************************************/
static int palmtc_backlight_init(struct device *dev)
{
	int ret;

	ret = gpio_request(GPIO_NR_PALMTC_BL_POWER, "BL POWER");
	if (ret)
		goto err;
	ret = gpio_direction_output(GPIO_NR_PALMTC_BL_POWER, 1);
	if (ret)
		goto err2;

	return 0;

err2:
	gpio_free(GPIO_NR_PALMTC_BL_POWER);
err:
	return ret;
}

static int palmtc_backlight_notify(struct device *dev, int brightness)
{
	/* backlight is on when GPIO16 AF0 is high */
	gpio_set_value(GPIO_NR_PALMTC_BL_POWER, brightness);
	return brightness;
}

static void palmtc_backlight_exit(struct device *dev)
{
	gpio_free(GPIO_NR_PALMTC_BL_POWER);
}

static struct platform_pwm_backlight_data palmtc_backlight_data = {
	.pwm_id		= 1,
	.max_brightness	= PALMTC_MAX_INTENSITY,
	.dft_brightness	= PALMTC_MAX_INTENSITY,
	.pwm_period_ns	= PALMTC_PERIOD_NS,
	.init		= palmtc_backlight_init,
	.notify		= palmtc_backlight_notify,
	.exit		= palmtc_backlight_exit,
};

static struct platform_device palmtc_backlight = {
	.name	= "pwm-backlight",
	.dev	= {
		.parent		= &pxa25x_device_pwm1.dev,
		.platform_data	= &palmtc_backlight_data,
	},
};

/******************************************************************************
 * IrDA
 ******************************************************************************/
static struct pxaficp_platform_data palmtc_ficp_platform_data = {
	.gpio_pwdown		= GPIO_NR_PALMTC_IR_DISABLE,
	.transceiver_cap	= IR_SIRMODE | IR_OFF,
};

/******************************************************************************
 * Keyboard
 ******************************************************************************/
static const uint32_t palmtc_matrix_keys[] = {
	KEY(0, 0, KEY_F1),
	KEY(0, 1, KEY_X),
	KEY(0, 2, KEY_POWER),
	KEY(0, 3, KEY_TAB),
	KEY(0, 4, KEY_A),
	KEY(0, 5, KEY_Q),
	KEY(0, 6, KEY_LEFTSHIFT),
	KEY(0, 7, KEY_Z),
	KEY(0, 8, KEY_S),
	KEY(0, 9, KEY_W),
	KEY(0, 10, KEY_E),
	KEY(0, 11, KEY_UP),

	KEY(1, 0, KEY_F2),
	KEY(1, 1, KEY_DOWN),
	KEY(1, 3, KEY_D),
	KEY(1, 4, KEY_C),
	KEY(1, 5, KEY_F),
	KEY(1, 6, KEY_R),
	KEY(1, 7, KEY_SPACE),
	KEY(1, 8, KEY_V),
	KEY(1, 9, KEY_G),
	KEY(1, 10, KEY_T),
	KEY(1, 11, KEY_LEFT),

	KEY(2, 0, KEY_F3),
	KEY(2, 1, KEY_LEFTCTRL),
	KEY(2, 3, KEY_H),
	KEY(2, 4, KEY_Y),
	KEY(2, 5, KEY_N),
	KEY(2, 6, KEY_J),
	KEY(2, 7, KEY_U),
	KEY(2, 8, KEY_M),
	KEY(2, 9, KEY_K),
	KEY(2, 10, KEY_I),
	KEY(2, 11, KEY_RIGHT),

	KEY(3, 0, KEY_F4),
	KEY(3, 1, KEY_ENTER),
	KEY(3, 3, KEY_DOT),
	KEY(3, 4, KEY_L),
	KEY(3, 5, KEY_O),
	KEY(3, 6, KEY_LEFTALT),
	KEY(3, 7, KEY_ENTER),
	KEY(3, 8, KEY_BACKSPACE),
	KEY(3, 9, KEY_P),
	KEY(3, 10, KEY_B),
	KEY(3, 11, KEY_FN),
};

const struct matrix_keymap_data palmtc_keymap_data = {
	.keymap			= palmtc_matrix_keys,
	.keymap_size		= ARRAY_SIZE(palmtc_matrix_keys),
};

static const unsigned int palmtc_keypad_row_gpios[] = {
	0, 9, 10, 11
};

static const unsigned int palmtc_keypad_col_gpios[] = {
	18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 79, 80
};

static struct matrix_keypad_platform_data palmtc_keypad_platform_data = {
	.keymap_data	= &palmtc_keymap_data,
	.row_gpios	= palmtc_keypad_row_gpios,
	.num_row_gpios	= ARRAY_SIZE(palmtc_keypad_row_gpios),
	.col_gpios	= palmtc_keypad_col_gpios,
	.num_col_gpios	= ARRAY_SIZE(palmtc_keypad_col_gpios),
	.active_low	= 1,

	.debounce_ms		= 20,
	.col_scan_delay_us	= 5,
};

static struct platform_device palmtc_keyboard = {
	.name	= "matrix-keypad",
	.id	= -1,
	.dev	= {
		.platform_data = &palmtc_keypad_platform_data,
	},
};

/******************************************************************************
 * UDC
 ******************************************************************************/
static struct pxa2xx_udc_mach_info palmtc_udc_info __initdata = {
	.gpio_vbus		= GPIO_NR_PALMTC_USB_DETECT_N,
	.gpio_vbus_inverted	= 1,
	.gpio_pullup		= GPIO_NR_PALMTC_USB_POWER,
};

/******************************************************************************
 * Touchscreen / Battery / GPIO-extender
 ******************************************************************************/
static struct platform_device palmtc_ucb1400_core = {
	.name	= "ucb1400_core",
	.id	= -1,
};

/******************************************************************************
 * NOR Flash
 ******************************************************************************/
static struct resource palmtc_flash_resource = {
	.start	= PXA_CS0_PHYS,
	.end	= PXA_CS0_PHYS + SZ_16M - 1,
	.flags	= IORESOURCE_MEM,
};

static struct mtd_partition palmtc_flash_parts[] = {
	{
		.name	= "U-Boot Bootloader",
		.offset	= 0x0,
		.size	= 0x40000,
	},
	{
		.name	= "Linux Kernel",
		.offset	= 0x40000,
		.size	= 0x2c0000,
	},
	{
		.name	= "Filesystem",
		.offset	= 0x300000,
		.size	= 0xcc0000,
	},
	{
		.name	= "U-Boot Environment",
		.offset	= 0xfc0000,
		.size	= MTDPART_SIZ_FULL,
	},
};

static struct physmap_flash_data palmtc_flash_data = {
	.width		= 4,
	.parts		= palmtc_flash_parts,
	.nr_parts	= ARRAY_SIZE(palmtc_flash_parts),
};

static struct platform_device palmtc_flash = {
	.name		= "physmap-flash",
	.id		= -1,
	.resource	= &palmtc_flash_resource,
	.num_resources	= 1,
	.dev = {
		.platform_data	= &palmtc_flash_data,
	},
};

/******************************************************************************
 * Framebuffer
 ******************************************************************************/
static struct pxafb_mode_info palmtc_lcd_modes[] = {
{
	.pixclock	= 115384,
	.xres		= 320,
	.yres		= 320,
	.bpp		= 16,

	.left_margin	= 27,
	.right_margin	= 7,
	.upper_margin	= 7,
	.lower_margin	= 8,

	.hsync_len	= 6,
	.vsync_len	= 1,
},
};

static struct pxafb_mach_info palmtc_lcd_screen = {
	.modes			= palmtc_lcd_modes,
	.num_modes		= ARRAY_SIZE(palmtc_lcd_modes),
	.lcd_conn		= LCD_COLOR_TFT_16BPP | LCD_PCLK_EDGE_FALL,
};

/******************************************************************************
 * Machine init
 ******************************************************************************/
static struct platform_device *devices[] __initdata = {
	&palmtc_backlight,
	&palmtc_ucb1400_core,
	&palmtc_keyboard,
	&palmtc_pxa_keys,
	&palmtc_flash,
};

static void __init palmtc_init(void)
{
	pxa2xx_mfp_config(ARRAY_AND_SIZE(palmtc_pin_config));

	pxa_set_ffuart_info(NULL);
	pxa_set_btuart_info(NULL);
	pxa_set_stuart_info(NULL);
	pxa_set_hwuart_info(NULL);

	set_pxa_fb_info(&palmtc_lcd_screen);
	pxa_set_mci_info(&palmtc_mci_platform_data);
	pxa_set_udc_info(&palmtc_udc_info);
	pxa_set_ac97_info(NULL);
	pxa_set_ficp_info(&palmtc_ficp_platform_data);

	platform_add_devices(devices, ARRAY_SIZE(devices));
};

MACHINE_START(PALMTC, "Palm Tungsten|C")
	.phys_io	= 0x40000000,
	.boot_params 	= 0xa0000100,
	.io_pg_offst	= (io_p2v(0x40000000) >> 18) & 0xfffc,
	.map_io		= pxa_map_io,
	.init_irq	= pxa25x_init_irq,
	.timer		= &pxa_timer,
	.init_machine	= palmtc_init
MACHINE_END
