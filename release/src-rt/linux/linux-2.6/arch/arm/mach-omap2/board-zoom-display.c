/*
 * Copyright (C) 2010 Texas Instruments Inc.
 *
 * Modified from mach-omap2/board-zoom-peripherals.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/i2c/twl.h>
#include <linux/spi/spi.h>
#include <plat/mcspi.h>
#include <plat/display.h>

#define LCD_PANEL_RESET_GPIO_PROD	96
#define LCD_PANEL_RESET_GPIO_PILOT	55
#define LCD_PANEL_QVGA_GPIO		56

static void zoom_lcd_panel_init(void)
{
	int ret;
	unsigned char lcd_panel_reset_gpio;

	lcd_panel_reset_gpio = (omap_rev() > OMAP3430_REV_ES3_0) ?
			LCD_PANEL_RESET_GPIO_PROD :
			LCD_PANEL_RESET_GPIO_PILOT;

	ret = gpio_request(lcd_panel_reset_gpio, "lcd reset");
	if (ret) {
		pr_err("Failed to get LCD reset GPIO (gpio%d).\n",
			lcd_panel_reset_gpio);
		return;
	}
	gpio_direction_output(lcd_panel_reset_gpio, 1);

	ret = gpio_request(LCD_PANEL_QVGA_GPIO, "lcd qvga");
	if (ret) {
		pr_err("Failed to get LCD_PANEL_QVGA_GPIO (gpio%d).\n",
			LCD_PANEL_QVGA_GPIO);
		goto err0;
	}
	gpio_direction_output(LCD_PANEL_QVGA_GPIO, 1);

	return;
err0:
	gpio_free(lcd_panel_reset_gpio);
}

static int zoom_panel_enable_lcd(struct omap_dss_device *dssdev)
{
	return 0;
}

static void zoom_panel_disable_lcd(struct omap_dss_device *dssdev)
{
}

/*
 * PWMA/B register offsets (TWL4030_MODULE_PWMA)
 */
#define TWL_INTBR_PMBR1	0xD
#define TWL_INTBR_GPBR1	0xC
#define TWL_LED_PWMON	0x0
#define TWL_LED_PWMOFF	0x1

static int zoom_set_bl_intensity(struct omap_dss_device *dssdev, int level)
{
	unsigned char c;
	u8 mux_pwm, enb_pwm;

	if (level > 100)
		return -1;

	twl_i2c_read_u8(TWL4030_MODULE_INTBR, &mux_pwm, TWL_INTBR_PMBR1);
	twl_i2c_read_u8(TWL4030_MODULE_INTBR, &enb_pwm, TWL_INTBR_GPBR1);

	if (level == 0) {
		/* disable pwm1 output and clock */
		enb_pwm = enb_pwm & 0xF5;
		/* change pwm1 pin to gpio pin */
		mux_pwm = mux_pwm & 0xCF;
		twl_i2c_write_u8(TWL4030_MODULE_INTBR,
					enb_pwm, TWL_INTBR_GPBR1);
		twl_i2c_write_u8(TWL4030_MODULE_INTBR,
					mux_pwm, TWL_INTBR_PMBR1);
		return 0;
	}

	if (!((enb_pwm & 0xA) && (mux_pwm & 0x30))) {
		/* change gpio pin to pwm1 pin */
		mux_pwm = mux_pwm | 0x30;
		/* enable pwm1 output and clock*/
		enb_pwm = enb_pwm | 0x0A;
		twl_i2c_write_u8(TWL4030_MODULE_INTBR,
					mux_pwm, TWL_INTBR_PMBR1);
		twl_i2c_write_u8(TWL4030_MODULE_INTBR,
					enb_pwm, TWL_INTBR_GPBR1);
	}

	c = ((50 * (100 - level)) / 100) + 1;
	twl_i2c_write_u8(TWL4030_MODULE_PWM1, 0x7F, TWL_LED_PWMOFF);
	twl_i2c_write_u8(TWL4030_MODULE_PWM1, c, TWL_LED_PWMON);

	return 0;
}

static struct omap_dss_device zoom_lcd_device = {
	.name			= "lcd",
	.driver_name		= "NEC_8048_panel",
	.type			= OMAP_DISPLAY_TYPE_DPI,
	.phy.dpi.data_lines	= 24,
	.platform_enable	= zoom_panel_enable_lcd,
	.platform_disable	= zoom_panel_disable_lcd,
	.max_backlight_level	= 100,
	.set_backlight		= zoom_set_bl_intensity,
};

static struct omap_dss_device *zoom_dss_devices[] = {
	&zoom_lcd_device,
};

static struct omap_dss_board_info zoom_dss_data = {
	.num_devices		= ARRAY_SIZE(zoom_dss_devices),
	.devices		= zoom_dss_devices,
	.default_device		= &zoom_lcd_device,
};

static struct omap2_mcspi_device_config dss_lcd_mcspi_config = {
	.turbo_mode		= 1,
	.single_channel	= 1,  /* 0: slave, 1: master */
};

static struct spi_board_info nec_8048_spi_board_info[] __initdata = {
	[0] = {
		.modalias		= "nec_8048_spi",
		.bus_num		= 1,
		.chip_select		= 2,
		.max_speed_hz		= 375000,
		.controller_data	= &dss_lcd_mcspi_config,
	},
};

void __init zoom_display_init(void)
{
	omap_display_init(&zoom_dss_data);
	spi_register_board_info(nec_8048_spi_board_info,
				ARRAY_SIZE(nec_8048_spi_board_info));
	zoom_lcd_panel_init();
}

