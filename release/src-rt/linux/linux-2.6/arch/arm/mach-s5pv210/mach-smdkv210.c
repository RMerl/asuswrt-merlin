/* linux/arch/arm/mach-s5pv210/mach-smdkv210.c
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/serial_core.h>
#include <linux/sysdev.h>
#include <linux/dm9000.h>
#include <linux/fb.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/pwm_backlight.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/setup.h>
#include <asm/mach-types.h>

#include <video/platform_lcd.h>

#include <mach/map.h>
#include <mach/regs-clock.h>
#include <mach/regs-fb.h>

#include <plat/regs-serial.h>
#include <plat/regs-srom.h>
#include <plat/gpio-cfg.h>
#include <plat/s5pv210.h>
#include <plat/devs.h>
#include <plat/cpu.h>
#include <plat/adc.h>
#include <plat/ts.h>
#include <plat/ata.h>
#include <plat/iic.h>
#include <plat/keypad.h>
#include <plat/pm.h>
#include <plat/fb.h>
#include <plat/s5p-time.h>

/* Following are default values for UCON, ULCON and UFCON UART registers */
#define SMDKV210_UCON_DEFAULT	(S3C2410_UCON_TXILEVEL |	\
				 S3C2410_UCON_RXILEVEL |	\
				 S3C2410_UCON_TXIRQMODE |	\
				 S3C2410_UCON_RXIRQMODE |	\
				 S3C2410_UCON_RXFIFO_TOI |	\
				 S3C2443_UCON_RXERR_IRQEN)

#define SMDKV210_ULCON_DEFAULT	S3C2410_LCON_CS8

#define SMDKV210_UFCON_DEFAULT	(S3C2410_UFCON_FIFOMODE |	\
				 S5PV210_UFCON_TXTRIG4 |	\
				 S5PV210_UFCON_RXTRIG4)

static struct s3c2410_uartcfg smdkv210_uartcfgs[] __initdata = {
	[0] = {
		.hwport		= 0,
		.flags		= 0,
		.ucon		= SMDKV210_UCON_DEFAULT,
		.ulcon		= SMDKV210_ULCON_DEFAULT,
		.ufcon		= SMDKV210_UFCON_DEFAULT,
	},
	[1] = {
		.hwport		= 1,
		.flags		= 0,
		.ucon		= SMDKV210_UCON_DEFAULT,
		.ulcon		= SMDKV210_ULCON_DEFAULT,
		.ufcon		= SMDKV210_UFCON_DEFAULT,
	},
	[2] = {
		.hwport		= 2,
		.flags		= 0,
		.ucon		= SMDKV210_UCON_DEFAULT,
		.ulcon		= SMDKV210_ULCON_DEFAULT,
		.ufcon		= SMDKV210_UFCON_DEFAULT,
	},
	[3] = {
		.hwport		= 3,
		.flags		= 0,
		.ucon		= SMDKV210_UCON_DEFAULT,
		.ulcon		= SMDKV210_ULCON_DEFAULT,
		.ufcon		= SMDKV210_UFCON_DEFAULT,
	},
};

static struct s3c_ide_platdata smdkv210_ide_pdata __initdata = {
	.setup_gpio	= s5pv210_ide_setup_gpio,
};

static uint32_t smdkv210_keymap[] __initdata = {
	/* KEY(row, col, keycode) */
	KEY(0, 3, KEY_1), KEY(0, 4, KEY_2), KEY(0, 5, KEY_3),
	KEY(0, 6, KEY_4), KEY(0, 7, KEY_5),
	KEY(1, 3, KEY_A), KEY(1, 4, KEY_B), KEY(1, 5, KEY_C),
	KEY(1, 6, KEY_D), KEY(1, 7, KEY_E)
};

static struct matrix_keymap_data smdkv210_keymap_data __initdata = {
	.keymap		= smdkv210_keymap,
	.keymap_size	= ARRAY_SIZE(smdkv210_keymap),
};

static struct samsung_keypad_platdata smdkv210_keypad_data __initdata = {
	.keymap_data	= &smdkv210_keymap_data,
	.rows		= 8,
	.cols		= 8,
};

static struct resource smdkv210_dm9000_resources[] = {
	[0] = {
		.start	= S5PV210_PA_SROM_BANK5,
		.end	= S5PV210_PA_SROM_BANK5,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= S5PV210_PA_SROM_BANK5 + 2,
		.end	= S5PV210_PA_SROM_BANK5 + 2,
		.flags	= IORESOURCE_MEM,
	},
	[2] = {
		.start	= IRQ_EINT(9),
		.end	= IRQ_EINT(9),
		.flags	= IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHLEVEL,
	},
};

static struct dm9000_plat_data smdkv210_dm9000_platdata = {
	.flags		= DM9000_PLATF_16BITONLY | DM9000_PLATF_NO_EEPROM,
	.dev_addr	= { 0x00, 0x09, 0xc0, 0xff, 0xec, 0x48 },
};

struct platform_device smdkv210_dm9000 = {
	.name		= "dm9000",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(smdkv210_dm9000_resources),
	.resource	= smdkv210_dm9000_resources,
	.dev		= {
		.platform_data	= &smdkv210_dm9000_platdata,
	},
};

static void smdkv210_lte480wv_set_power(struct plat_lcd_data *pd,
					unsigned int power)
{
	if (power) {
#if !defined(CONFIG_BACKLIGHT_PWM)
		gpio_request(S5PV210_GPD0(3), "GPD0");
		gpio_direction_output(S5PV210_GPD0(3), 1);
		gpio_free(S5PV210_GPD0(3));
#endif

		/* fire nRESET on power up */
		gpio_request(S5PV210_GPH0(6), "GPH0");

		gpio_direction_output(S5PV210_GPH0(6), 1);

		gpio_set_value(S5PV210_GPH0(6), 0);
		mdelay(10);

		gpio_set_value(S5PV210_GPH0(6), 1);
		mdelay(10);

		gpio_free(S5PV210_GPH0(6));
	} else {
#if !defined(CONFIG_BACKLIGHT_PWM)
		gpio_request(S5PV210_GPD0(3), "GPD0");
		gpio_direction_output(S5PV210_GPD0(3), 0);
		gpio_free(S5PV210_GPD0(3));
#endif
	}
}

static struct plat_lcd_data smdkv210_lcd_lte480wv_data = {
	.set_power	= smdkv210_lte480wv_set_power,
};

static struct platform_device smdkv210_lcd_lte480wv = {
	.name			= "platform-lcd",
	.dev.parent		= &s3c_device_fb.dev,
	.dev.platform_data	= &smdkv210_lcd_lte480wv_data,
};

static struct s3c_fb_pd_win smdkv210_fb_win0 = {
	.win_mode = {
		.left_margin	= 13,
		.right_margin	= 8,
		.upper_margin	= 7,
		.lower_margin	= 5,
		.hsync_len	= 3,
		.vsync_len	= 1,
		.xres		= 800,
		.yres		= 480,
	},
	.max_bpp	= 32,
	.default_bpp	= 24,
};

static struct s3c_fb_platdata smdkv210_lcd0_pdata __initdata = {
	.win[0]		= &smdkv210_fb_win0,
	.vidcon0	= VIDCON0_VIDOUT_RGB | VIDCON0_PNRMODE_RGB,
	.vidcon1	= VIDCON1_INV_HSYNC | VIDCON1_INV_VSYNC,
	.setup_gpio	= s5pv210_fb_gpio_setup_24bpp,
};

static int smdkv210_backlight_init(struct device *dev)
{
	int ret;

	ret = gpio_request(S5PV210_GPD0(3), "Backlight");
	if (ret) {
		printk(KERN_ERR "failed to request GPD for PWM-OUT 3\n");
		return ret;
	}

	/* Configure GPIO pin with S5PV210_GPD_0_3_TOUT_3 */
	s3c_gpio_cfgpin(S5PV210_GPD0(3), S3C_GPIO_SFN(2));

	return 0;
}

static void smdkv210_backlight_exit(struct device *dev)
{
	s3c_gpio_cfgpin(S5PV210_GPD0(3), S3C_GPIO_OUTPUT);
	gpio_free(S5PV210_GPD0(3));
}

static struct platform_pwm_backlight_data smdkv210_backlight_data = {
	.pwm_id		= 3,
	.max_brightness	= 255,
	.dft_brightness	= 255,
	.pwm_period_ns	= 78770,
	.init		= smdkv210_backlight_init,
	.exit		= smdkv210_backlight_exit,
};

static struct platform_device smdkv210_backlight_device = {
	.name		= "pwm-backlight",
	.dev		= {
		.parent		= &s3c_device_timer[3].dev,
		.platform_data	= &smdkv210_backlight_data,
	},
};

static struct platform_device *smdkv210_devices[] __initdata = {
	&s3c_device_adc,
	&s3c_device_cfcon,
	&s3c_device_fb,
	&s3c_device_hsmmc0,
	&s3c_device_hsmmc1,
	&s3c_device_hsmmc2,
	&s3c_device_hsmmc3,
	&s3c_device_i2c0,
	&s3c_device_i2c1,
	&s3c_device_i2c2,
	&s3c_device_rtc,
	&s3c_device_ts,
	&s3c_device_wdt,
	&s5pv210_device_ac97,
	&s5pv210_device_iis0,
	&s5pv210_device_spdif,
	&samsung_asoc_dma,
	&samsung_device_keypad,
	&smdkv210_dm9000,
	&smdkv210_lcd_lte480wv,
	&s3c_device_timer[3],
	&smdkv210_backlight_device,
};

static void __init smdkv210_dm9000_init(void)
{
	unsigned int tmp;

	gpio_request(S5PV210_MP01(5), "nCS5");
	s3c_gpio_cfgpin(S5PV210_MP01(5), S3C_GPIO_SFN(2));
	gpio_free(S5PV210_MP01(5));

	tmp = (5 << S5P_SROM_BCX__TACC__SHIFT);
	__raw_writel(tmp, S5P_SROM_BC5);

	tmp = __raw_readl(S5P_SROM_BW);
	tmp &= (S5P_SROM_BW__CS_MASK << S5P_SROM_BW__NCS5__SHIFT);
	tmp |= (1 << S5P_SROM_BW__NCS5__SHIFT);
	__raw_writel(tmp, S5P_SROM_BW);
}

static struct i2c_board_info smdkv210_i2c_devs0[] __initdata = {
	{ I2C_BOARD_INFO("24c08", 0x50), },     /* Samsung S524AD0XD1 */
	{ I2C_BOARD_INFO("wm8580", 0x1b), },
};

static struct i2c_board_info smdkv210_i2c_devs1[] __initdata = {
	/* To Be Updated */
};

static struct i2c_board_info smdkv210_i2c_devs2[] __initdata = {
	/* To Be Updated */
};

static struct s3c2410_ts_mach_info s3c_ts_platform __initdata = {
	.delay			= 10000,
	.presc			= 49,
	.oversampling_shift	= 2,
};

static void __init smdkv210_map_io(void)
{
	s5p_init_io(NULL, 0, S5P_VA_CHIPID);
	s3c24xx_init_clocks(24000000);
	s3c24xx_init_uarts(smdkv210_uartcfgs, ARRAY_SIZE(smdkv210_uartcfgs));
	s5p_set_timer_source(S5P_PWM2, S5P_PWM4);
}

static void __init smdkv210_machine_init(void)
{
	s3c_pm_init();

	smdkv210_dm9000_init();

	samsung_keypad_set_platdata(&smdkv210_keypad_data);
	s3c24xx_ts_set_platdata(&s3c_ts_platform);

	s3c_i2c0_set_platdata(NULL);
	s3c_i2c1_set_platdata(NULL);
	s3c_i2c2_set_platdata(NULL);
	i2c_register_board_info(0, smdkv210_i2c_devs0,
			ARRAY_SIZE(smdkv210_i2c_devs0));
	i2c_register_board_info(1, smdkv210_i2c_devs1,
			ARRAY_SIZE(smdkv210_i2c_devs1));
	i2c_register_board_info(2, smdkv210_i2c_devs2,
			ARRAY_SIZE(smdkv210_i2c_devs2));

	s3c_ide_set_platdata(&smdkv210_ide_pdata);

	s3c_fb_set_platdata(&smdkv210_lcd0_pdata);

	platform_add_devices(smdkv210_devices, ARRAY_SIZE(smdkv210_devices));
}

MACHINE_START(SMDKV210, "SMDKV210")
	/* Maintainer: Kukjin Kim <kgene.kim@samsung.com> */
	.boot_params	= S5P_PA_SDRAM + 0x100,
	.init_irq	= s5pv210_init_irq,
	.map_io		= smdkv210_map_io,
	.init_machine	= smdkv210_machine_init,
	.timer		= &s5p_timer,
MACHINE_END
