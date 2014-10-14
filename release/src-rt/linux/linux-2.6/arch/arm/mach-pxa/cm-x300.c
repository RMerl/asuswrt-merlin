/*
 * linux/arch/arm/mach-pxa/cm-x300.c
 *
 * Support for the CompuLab CM-X300 modules
 *
 * Copyright (C) 2008,2009 CompuLab Ltd.
 *
 * Mike Rapoport <mike@compulab.co.il>
 * Igor Grinberg <grinberg@compulab.co.il>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/clk.h>

#include <linux/gpio.h>
#include <linux/dm9000.h>
#include <linux/leds.h>
#include <linux/rtc-v3020.h>
#include <linux/pwm_backlight.h>

#include <linux/i2c.h>
#include <linux/i2c/pca953x.h>
#include <linux/i2c/pxa-i2c.h>

#include <linux/mfd/da903x.h>
#include <linux/regulator/machine.h>
#include <linux/power_supply.h>
#include <linux/apm-emulation.h>

#include <linux/spi/spi.h>
#include <linux/spi/spi_gpio.h>
#include <linux/spi/tdo24m.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/setup.h>

#include <mach/pxa300.h>
#include <mach/pxa27x-udc.h>
#include <mach/pxafb.h>
#include <mach/mmc.h>
#include <mach/ohci.h>
#include <plat/pxa3xx_nand.h>
#include <mach/audio.h>
#include <mach/pxa3xx-u2d.h>

#include <asm/mach/map.h>

#include "generic.h"
#include "devices.h"

#define CM_X300_ETH_PHYS	0x08000010

#define GPIO82_MMC_IRQ		(82)
#define GPIO85_MMC_WP		(85)

#define	CM_X300_MMC_IRQ		IRQ_GPIO(GPIO82_MMC_IRQ)

#define GPIO95_RTC_CS		(95)
#define GPIO96_RTC_WR		(96)
#define GPIO97_RTC_RD		(97)
#define GPIO98_RTC_IO		(98)

#define GPIO_ULPI_PHY_RST	(127)

static mfp_cfg_t cm_x3xx_mfp_cfg[] __initdata = {
	/* LCD */
	GPIO54_LCD_LDD_0,
	GPIO55_LCD_LDD_1,
	GPIO56_LCD_LDD_2,
	GPIO57_LCD_LDD_3,
	GPIO58_LCD_LDD_4,
	GPIO59_LCD_LDD_5,
	GPIO60_LCD_LDD_6,
	GPIO61_LCD_LDD_7,
	GPIO62_LCD_LDD_8,
	GPIO63_LCD_LDD_9,
	GPIO64_LCD_LDD_10,
	GPIO65_LCD_LDD_11,
	GPIO66_LCD_LDD_12,
	GPIO67_LCD_LDD_13,
	GPIO68_LCD_LDD_14,
	GPIO69_LCD_LDD_15,
	GPIO72_LCD_FCLK,
	GPIO73_LCD_LCLK,
	GPIO74_LCD_PCLK,
	GPIO75_LCD_BIAS,

	/* BTUART */
	GPIO111_UART2_RTS,
	GPIO112_UART2_RXD | MFP_LPM_EDGE_FALL,
	GPIO113_UART2_TXD,
	GPIO114_UART2_CTS | MFP_LPM_EDGE_BOTH,

	/* STUART */
	GPIO109_UART3_TXD,
	GPIO110_UART3_RXD | MFP_LPM_EDGE_FALL,

	/* AC97 */
	GPIO23_AC97_nACRESET,
	GPIO24_AC97_SYSCLK,
	GPIO29_AC97_BITCLK,
	GPIO25_AC97_SDATA_IN_0,
	GPIO27_AC97_SDATA_OUT,
	GPIO28_AC97_SYNC,

	/* Keypad */
	GPIO115_KP_MKIN_0 | MFP_LPM_EDGE_BOTH,
	GPIO116_KP_MKIN_1 | MFP_LPM_EDGE_BOTH,
	GPIO117_KP_MKIN_2 | MFP_LPM_EDGE_BOTH,
	GPIO118_KP_MKIN_3 | MFP_LPM_EDGE_BOTH,
	GPIO119_KP_MKIN_4 | MFP_LPM_EDGE_BOTH,
	GPIO120_KP_MKIN_5 | MFP_LPM_EDGE_BOTH,
	GPIO2_2_KP_MKIN_6 | MFP_LPM_EDGE_BOTH,
	GPIO3_2_KP_MKIN_7 | MFP_LPM_EDGE_BOTH,
	GPIO121_KP_MKOUT_0,
	GPIO122_KP_MKOUT_1,
	GPIO123_KP_MKOUT_2,
	GPIO124_KP_MKOUT_3,
	GPIO125_KP_MKOUT_4,
	GPIO4_2_KP_MKOUT_5,

	/* MMC1 */
	GPIO3_MMC1_DAT0,
	GPIO4_MMC1_DAT1 | MFP_LPM_EDGE_BOTH,
	GPIO5_MMC1_DAT2,
	GPIO6_MMC1_DAT3,
	GPIO7_MMC1_CLK,
	GPIO8_MMC1_CMD,	/* CMD0 for slot 0 */

	/* MMC2 */
	GPIO9_MMC2_DAT0,
	GPIO10_MMC2_DAT1 | MFP_LPM_EDGE_BOTH,
	GPIO11_MMC2_DAT2,
	GPIO12_MMC2_DAT3,
	GPIO13_MMC2_CLK,
	GPIO14_MMC2_CMD,

	/* FFUART */
	GPIO30_UART1_RXD | MFP_LPM_EDGE_FALL,
	GPIO31_UART1_TXD,
	GPIO32_UART1_CTS,
	GPIO37_UART1_RTS,
	GPIO33_UART1_DCD,
	GPIO34_UART1_DSR | MFP_LPM_EDGE_FALL,
	GPIO35_UART1_RI,
	GPIO36_UART1_DTR,

	/* GPIOs */
	GPIO82_GPIO | MFP_PULL_HIGH,	/* MMC CD */
	GPIO85_GPIO,			/* MMC WP */
	GPIO99_GPIO,			/* Ethernet IRQ */

	/* RTC GPIOs */
	GPIO95_GPIO,			/* RTC CS */
	GPIO96_GPIO,			/* RTC WR */
	GPIO97_GPIO,			/* RTC RD */
	GPIO98_GPIO,			/* RTC IO */

	/* Standard I2C */
	GPIO21_I2C_SCL,
	GPIO22_I2C_SDA,

	/* PWM Backlight */
	GPIO19_PWM2_OUT,
};

static mfp_cfg_t cm_x3xx_rev_lt130_mfp_cfg[] __initdata = {
	/* GPIOs */
	GPIO79_GPIO,			/* LED */
	GPIO77_GPIO,			/* WiFi reset */
	GPIO78_GPIO,			/* BT reset */
};

static mfp_cfg_t cm_x3xx_rev_ge130_mfp_cfg[] __initdata = {
	/* GPIOs */
	GPIO76_GPIO,			/* LED */
	GPIO71_GPIO,			/* WiFi reset */
	GPIO70_GPIO,			/* BT reset */
};

static mfp_cfg_t cm_x310_mfp_cfg[] __initdata = {
	/* USB PORT 2 */
	ULPI_STP,
	ULPI_NXT,
	ULPI_DIR,
	GPIO30_ULPI_DATA_OUT_0,
	GPIO31_ULPI_DATA_OUT_1,
	GPIO32_ULPI_DATA_OUT_2,
	GPIO33_ULPI_DATA_OUT_3,
	GPIO34_ULPI_DATA_OUT_4,
	GPIO35_ULPI_DATA_OUT_5,
	GPIO36_ULPI_DATA_OUT_6,
	GPIO37_ULPI_DATA_OUT_7,
	GPIO38_ULPI_CLK,
	/* external PHY reset pin */
	GPIO127_GPIO,

	/* USB PORT 3 */
	GPIO77_USB_P3_1,
	GPIO78_USB_P3_2,
	GPIO79_USB_P3_3,
	GPIO80_USB_P3_4,
	GPIO81_USB_P3_5,
	GPIO82_USB_P3_6,
	GPIO0_2_USBH_PEN,
};

#if defined(CONFIG_DM9000) || defined(CONFIG_DM9000_MODULE)
static struct resource dm9000_resources[] = {
	[0] = {
		.start	= CM_X300_ETH_PHYS,
		.end	= CM_X300_ETH_PHYS + 0x3,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= CM_X300_ETH_PHYS + 0x4,
		.end	= CM_X300_ETH_PHYS + 0x4 + 500,
		.flags	= IORESOURCE_MEM,
	},
	[2] = {
		.start	= IRQ_GPIO(mfp_to_gpio(MFP_PIN_GPIO99)),
		.end	= IRQ_GPIO(mfp_to_gpio(MFP_PIN_GPIO99)),
		.flags	= IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHEDGE,
	}
};

static struct dm9000_plat_data cm_x300_dm9000_platdata = {
	.flags		= DM9000_PLATF_16BITONLY | DM9000_PLATF_NO_EEPROM,
};

static struct platform_device dm9000_device = {
	.name		= "dm9000",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(dm9000_resources),
	.resource	= dm9000_resources,
	.dev		= {
		.platform_data = &cm_x300_dm9000_platdata,
	}

};

static void __init cm_x300_init_dm9000(void)
{
	platform_device_register(&dm9000_device);
}
#else
static inline void cm_x300_init_dm9000(void) {}
#endif

/* LCD */
#if defined(CONFIG_FB_PXA) || defined(CONFIG_FB_PXA_MODULE)
static struct pxafb_mode_info cm_x300_lcd_modes[] = {
	[0] = {
		.pixclock	= 38250,
		.bpp		= 16,
		.xres		= 480,
		.yres		= 640,
		.hsync_len	= 8,
		.vsync_len	= 2,
		.left_margin	= 8,
		.upper_margin	= 2,
		.right_margin	= 24,
		.lower_margin	= 4,
		.cmap_greyscale	= 0,
	},
	[1] = {
		.pixclock	= 153800,
		.bpp		= 16,
		.xres		= 240,
		.yres		= 320,
		.hsync_len	= 8,
		.vsync_len	= 2,
		.left_margin	= 8,
		.upper_margin	= 2,
		.right_margin	= 88,
		.lower_margin	= 2,
		.cmap_greyscale	= 0,
	},
};

static struct pxafb_mach_info cm_x300_lcd = {
	.modes			= cm_x300_lcd_modes,
	.num_modes		= ARRAY_SIZE(cm_x300_lcd_modes),
	.lcd_conn		= LCD_COLOR_TFT_16BPP | LCD_PCLK_EDGE_FALL,
};

static void __init cm_x300_init_lcd(void)
{
	pxa_set_fb_info(NULL, &cm_x300_lcd);
}
#else
static inline void cm_x300_init_lcd(void) {}
#endif

#if defined(CONFIG_BACKLIGHT_PWM) || defined(CONFIG_BACKLIGHT_PWM_MODULE)
static struct platform_pwm_backlight_data cm_x300_backlight_data = {
	.pwm_id		= 2,
	.max_brightness	= 100,
	.dft_brightness	= 100,
	.pwm_period_ns	= 10000,
};

static struct platform_device cm_x300_backlight_device = {
	.name		= "pwm-backlight",
	.dev		= {
		.parent = &pxa27x_device_pwm0.dev,
		.platform_data	= &cm_x300_backlight_data,
	},
};

static void cm_x300_init_bl(void)
{
	platform_device_register(&cm_x300_backlight_device);
}
#else
static inline void cm_x300_init_bl(void) {}
#endif

#if defined(CONFIG_SPI_GPIO) || defined(CONFIG_SPI_GPIO_MODULE)
#define GPIO_LCD_BASE	(144)
#define GPIO_LCD_DIN	(GPIO_LCD_BASE + 8)	/* aux_gpio3_0 */
#define GPIO_LCD_DOUT	(GPIO_LCD_BASE + 9)	/* aux_gpio3_1 */
#define GPIO_LCD_SCL	(GPIO_LCD_BASE + 10)	/* aux_gpio3_2 */
#define GPIO_LCD_CS	(GPIO_LCD_BASE + 11)	/* aux_gpio3_3 */
#define LCD_SPI_BUS_NUM	(1)

static struct spi_gpio_platform_data cm_x300_spi_gpio_pdata = {
	.sck		= GPIO_LCD_SCL,
	.mosi		= GPIO_LCD_DIN,
	.miso		= GPIO_LCD_DOUT,
	.num_chipselect	= 1,
};

static struct platform_device cm_x300_spi_gpio = {
	.name		= "spi_gpio",
	.id		= LCD_SPI_BUS_NUM,
	.dev		= {
		.platform_data	= &cm_x300_spi_gpio_pdata,
	},
};

static struct tdo24m_platform_data cm_x300_tdo24m_pdata = {
	.model = TDO35S,
};

static struct spi_board_info cm_x300_spi_devices[] __initdata = {
	{
		.modalias		= "tdo24m",
		.max_speed_hz		= 1000000,
		.bus_num		= LCD_SPI_BUS_NUM,
		.chip_select		= 0,
		.controller_data	= (void *) GPIO_LCD_CS,
		.platform_data		= &cm_x300_tdo24m_pdata,
	},
};

static void __init cm_x300_init_spi(void)
{
	spi_register_board_info(cm_x300_spi_devices,
				ARRAY_SIZE(cm_x300_spi_devices));
	platform_device_register(&cm_x300_spi_gpio);
}
#else
static inline void cm_x300_init_spi(void) {}
#endif

#if defined(CONFIG_SND_PXA2XX_LIB_AC97)
static void __init cm_x300_init_ac97(void)
{
	pxa_set_ac97_info(NULL);
}
#else
static inline void cm_x300_init_ac97(void) {}
#endif

#if defined(CONFIG_MTD_NAND_PXA3xx) || defined(CONFIG_MTD_NAND_PXA3xx_MODULE)
static struct mtd_partition cm_x300_nand_partitions[] = {
	[0] = {
		.name        = "OBM",
		.offset      = 0,
		.size        = SZ_256K,
		.mask_flags  = MTD_WRITEABLE, /* force read-only */
	},
	[1] = {
		.name        = "U-Boot",
		.offset      = MTDPART_OFS_APPEND,
		.size        = SZ_256K,
		.mask_flags  = MTD_WRITEABLE, /* force read-only */
	},
	[2] = {
		.name        = "Environment",
		.offset      = MTDPART_OFS_APPEND,
		.size        = SZ_256K,
	},
	[3] = {
		.name        = "reserved",
		.offset      = MTDPART_OFS_APPEND,
		.size        = SZ_256K + SZ_1M,
		.mask_flags  = MTD_WRITEABLE, /* force read-only */
	},
	[4] = {
		.name        = "kernel",
		.offset      = MTDPART_OFS_APPEND,
		.size        = SZ_4M,
	},
	[5] = {
		.name        = "fs",
		.offset      = MTDPART_OFS_APPEND,
		.size        = MTDPART_SIZ_FULL,
	},
};

static struct pxa3xx_nand_platform_data cm_x300_nand_info = {
	.enable_arbiter	= 1,
	.keep_config	= 1,
	.parts		= cm_x300_nand_partitions,
	.nr_parts	= ARRAY_SIZE(cm_x300_nand_partitions),
};

static void __init cm_x300_init_nand(void)
{
	pxa3xx_set_nand_info(&cm_x300_nand_info);
}
#else
static inline void cm_x300_init_nand(void) {}
#endif

#if defined(CONFIG_MMC) || defined(CONFIG_MMC_MODULE)
static struct pxamci_platform_data cm_x300_mci_platform_data = {
	.detect_delay_ms	= 200,
	.ocr_mask		= MMC_VDD_32_33|MMC_VDD_33_34,
	.gpio_card_detect	= GPIO82_MMC_IRQ,
	.gpio_card_ro		= GPIO85_MMC_WP,
	.gpio_power		= -1,
};

/* The second MMC slot of CM-X300 is hardwired to Libertas card and has
   no detection/ro pins */
static int cm_x300_mci2_init(struct device *dev,
			     irq_handler_t cm_x300_detect_int,
	void *data)
{
	return 0;
}

static void cm_x300_mci2_exit(struct device *dev, void *data)
{
}

static struct pxamci_platform_data cm_x300_mci2_platform_data = {
	.detect_delay_ms	= 200,
	.ocr_mask		= MMC_VDD_32_33|MMC_VDD_33_34,
	.init 			= cm_x300_mci2_init,
	.exit			= cm_x300_mci2_exit,
	.gpio_card_detect	= -1,
	.gpio_card_ro		= -1,
	.gpio_power		= -1,
};

static void __init cm_x300_init_mmc(void)
{
	pxa_set_mci_info(&cm_x300_mci_platform_data);
	pxa3xx_set_mci2_info(&cm_x300_mci2_platform_data);
}
#else
static inline void cm_x300_init_mmc(void) {}
#endif

#if defined(CONFIG_PXA310_ULPI)
static struct clk *pout_clk;

static int cm_x300_ulpi_phy_reset(void)
{
	int err;

	/* reset the PHY */
	err = gpio_request(GPIO_ULPI_PHY_RST, "ulpi reset");
	if (err) {
		pr_err("%s: failed to request ULPI reset GPIO: %d\n",
		       __func__, err);
		return err;
	}

	gpio_direction_output(GPIO_ULPI_PHY_RST, 0);
	msleep(10);
	gpio_set_value(GPIO_ULPI_PHY_RST, 1);
	msleep(10);

	gpio_free(GPIO_ULPI_PHY_RST);

	return 0;
}

static inline int cm_x300_u2d_init(struct device *dev)
{
	int err = 0;

	if (cpu_is_pxa310()) {
		/* CLK_POUT is connected to the ULPI PHY */
		pout_clk = clk_get(NULL, "CLK_POUT");
		if (IS_ERR(pout_clk)) {
			err = PTR_ERR(pout_clk);
			pr_err("%s: failed to get CLK_POUT: %d\n",
			       __func__, err);
			return err;
		}
		clk_enable(pout_clk);

		err = cm_x300_ulpi_phy_reset();
		if (err) {
			clk_disable(pout_clk);
			clk_put(pout_clk);
		}
	}

	return err;
}

static void cm_x300_u2d_exit(struct device *dev)
{
	if (cpu_is_pxa310()) {
		clk_disable(pout_clk);
		clk_put(pout_clk);
	}
}

static struct pxa3xx_u2d_platform_data cm_x300_u2d_platform_data = {
	.ulpi_mode	= ULPI_SER_6PIN,
	.init		= cm_x300_u2d_init,
	.exit		= cm_x300_u2d_exit,
};

static void cm_x300_init_u2d(void)
{
	pxa3xx_set_u2d_info(&cm_x300_u2d_platform_data);
}
#else
static inline void cm_x300_init_u2d(void) {}
#endif

#if defined(CONFIG_USB_OHCI_HCD) || defined(CONFIG_USB_OHCI_HCD_MODULE)
static int cm_x300_ohci_init(struct device *dev)
{
	if (cpu_is_pxa300())
		UP2OCR = UP2OCR_HXS
			| UP2OCR_HXOE | UP2OCR_DMPDE | UP2OCR_DPPDE;

	return 0;
}

static struct pxaohci_platform_data cm_x300_ohci_platform_data = {
	.port_mode	= PMM_PERPORT_MODE,
	.flags		= ENABLE_PORT_ALL | POWER_CONTROL_LOW,
	.init		= cm_x300_ohci_init,
};

static void __init cm_x300_init_ohci(void)
{
	pxa_set_ohci_info(&cm_x300_ohci_platform_data);
}
#else
static inline void cm_x300_init_ohci(void) {}
#endif

#if defined(CONFIG_LEDS_GPIO) || defined(CONFIG_LEDS_GPIO_MODULE)
static struct gpio_led cm_x300_leds[] = {
	[0] = {
		.name = "cm-x300:green",
		.default_trigger = "heartbeat",
		.active_low = 1,
	},
};

static struct gpio_led_platform_data cm_x300_gpio_led_pdata = {
	.num_leds = ARRAY_SIZE(cm_x300_leds),
	.leds = cm_x300_leds,
};

static struct platform_device cm_x300_led_device = {
	.name		= "leds-gpio",
	.id		= -1,
	.dev		= {
		.platform_data = &cm_x300_gpio_led_pdata,
	},
};

static void __init cm_x300_init_leds(void)
{
	if (system_rev < 130)
		cm_x300_leds[0].gpio = 79;
	else
		cm_x300_leds[0].gpio = 76;

	platform_device_register(&cm_x300_led_device);
}
#else
static inline void cm_x300_init_leds(void) {}
#endif

#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
/* PCA9555 */
static struct pca953x_platform_data cm_x300_gpio_ext_pdata_0 = {
	.gpio_base = 128,
};

static struct pca953x_platform_data cm_x300_gpio_ext_pdata_1 = {
	.gpio_base = 144,
};

static struct i2c_board_info cm_x300_gpio_ext_info[] = {
	[0] = {
		I2C_BOARD_INFO("pca9555", 0x24),
		.platform_data = &cm_x300_gpio_ext_pdata_0,
	},
	[1] = {
		I2C_BOARD_INFO("pca9555", 0x25),
		.platform_data = &cm_x300_gpio_ext_pdata_1,
	},
};

static void __init cm_x300_init_i2c(void)
{
	pxa_set_i2c_info(NULL);
	i2c_register_board_info(0, cm_x300_gpio_ext_info,
				ARRAY_SIZE(cm_x300_gpio_ext_info));
}
#else
static inline void cm_x300_init_i2c(void) {}
#endif

#if defined(CONFIG_RTC_DRV_V3020) || defined(CONFIG_RTC_DRV_V3020_MODULE)
struct v3020_platform_data cm_x300_v3020_pdata = {
	.use_gpio	= 1,
	.gpio_cs	= GPIO95_RTC_CS,
	.gpio_wr	= GPIO96_RTC_WR,
	.gpio_rd	= GPIO97_RTC_RD,
	.gpio_io	= GPIO98_RTC_IO,
};

static struct platform_device cm_x300_rtc_device = {
	.name		= "v3020",
	.id		= -1,
	.dev		= {
		.platform_data = &cm_x300_v3020_pdata,
	}
};

static void __init cm_x300_init_rtc(void)
{
	platform_device_register(&cm_x300_rtc_device);
}
#else
static inline void cm_x300_init_rtc(void) {}
#endif

/* Battery */
struct power_supply_info cm_x300_psy_info = {
	.name = "battery",
	.technology = POWER_SUPPLY_TECHNOLOGY_LIPO,
	.voltage_max_design = 4200000,
	.voltage_min_design = 3000000,
	.use_for_apm = 1,
};

static void cm_x300_battery_low(void)
{
#if defined(CONFIG_APM_EMULATION)
	apm_queue_event(APM_LOW_BATTERY);
#endif
}

static void cm_x300_battery_critical(void)
{
#if defined(CONFIG_APM_EMULATION)
	apm_queue_event(APM_CRITICAL_SUSPEND);
#endif
}

struct da9030_battery_info cm_x300_battery_info = {
	.battery_info = &cm_x300_psy_info,

	.charge_milliamp = 1000,
	.charge_millivolt = 4200,

	.vbat_low = 3600,
	.vbat_crit = 3400,
	.vbat_charge_start = 4100,
	.vbat_charge_stop = 4200,
	.vbat_charge_restart = 4000,

	.vcharge_min = 3200,
	.vcharge_max = 5500,

	.tbat_low = 197,
	.tbat_high = 78,
	.tbat_restart = 100,

	.batmon_interval = 0,

	.battery_low = cm_x300_battery_low,
	.battery_critical = cm_x300_battery_critical,
};

static struct regulator_consumer_supply buck2_consumers[] = {
	{
		.dev = NULL,
		.supply = "vcc_core",
	},
};

static struct regulator_init_data buck2_data = {
	.constraints = {
		.min_uV = 1375000,
		.max_uV = 1375000,
		.state_mem = {
			.enabled = 0,
		},
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE,
		.apply_uV = 1,
	},
	.num_consumer_supplies = ARRAY_SIZE(buck2_consumers),
	.consumer_supplies = buck2_consumers,
};

/* DA9030 */
struct da903x_subdev_info cm_x300_da9030_subdevs[] = {
	{
		.name = "da903x-battery",
		.id = DA9030_ID_BAT,
		.platform_data = &cm_x300_battery_info,
	},
	{
		.name = "da903x-regulator",
		.id = DA9030_ID_BUCK2,
		.platform_data = &buck2_data,
	},
};

static struct da903x_platform_data cm_x300_da9030_info = {
	.num_subdevs = ARRAY_SIZE(cm_x300_da9030_subdevs),
	.subdevs = cm_x300_da9030_subdevs,
};

static struct i2c_board_info cm_x300_pmic_info = {
	I2C_BOARD_INFO("da9030", 0x49),
	.irq = IRQ_WAKEUP0,
	.platform_data = &cm_x300_da9030_info,
};

static struct i2c_pxa_platform_data cm_x300_pwr_i2c_info = {
	.use_pio = 1,
};

static void __init cm_x300_init_da9030(void)
{
	pxa3xx_set_i2c_power_info(&cm_x300_pwr_i2c_info);
	i2c_register_board_info(1, &cm_x300_pmic_info, 1);
	irq_set_irq_wake(IRQ_WAKEUP0, 1);
}

static void __init cm_x300_init_wi2wi(void)
{
	int bt_reset, wlan_en;
	int err;

	if (system_rev < 130) {
		wlan_en = 77;
		bt_reset = 78;
	} else {
		wlan_en = 71;
		bt_reset = 70;
	}

	/* Libertas and CSR reset */
	err = gpio_request(wlan_en, "wlan en");
	if (err) {
		pr_err("CM-X300: failed to request wlan en gpio: %d\n", err);
	} else {
		gpio_direction_output(wlan_en, 1);
		gpio_free(wlan_en);
	}

	err = gpio_request(bt_reset, "bt reset");
	if (err) {
		pr_err("CM-X300: failed to request bt reset gpio: %d\n", err);
	} else {
		gpio_direction_output(bt_reset, 1);
		udelay(10);
		gpio_set_value(bt_reset, 0);
		udelay(10);
		gpio_set_value(bt_reset, 1);
		gpio_free(bt_reset);
	}
}

/* MFP */
static void __init cm_x300_init_mfp(void)
{
	/* board-processor specific GPIO initialization */
	pxa3xx_mfp_config(ARRAY_AND_SIZE(cm_x3xx_mfp_cfg));

	if (system_rev < 130)
		pxa3xx_mfp_config(ARRAY_AND_SIZE(cm_x3xx_rev_lt130_mfp_cfg));
	else
		pxa3xx_mfp_config(ARRAY_AND_SIZE(cm_x3xx_rev_ge130_mfp_cfg));

	if (cpu_is_pxa310())
		pxa3xx_mfp_config(ARRAY_AND_SIZE(cm_x310_mfp_cfg));
}

static void __init cm_x300_init(void)
{
	cm_x300_init_mfp();

	pxa_set_btuart_info(NULL);
	pxa_set_stuart_info(NULL);
	if (cpu_is_pxa300())
		pxa_set_ffuart_info(NULL);

	cm_x300_init_da9030();
	cm_x300_init_dm9000();
	cm_x300_init_lcd();
	cm_x300_init_u2d();
	cm_x300_init_ohci();
	cm_x300_init_mmc();
	cm_x300_init_nand();
	cm_x300_init_leds();
	cm_x300_init_i2c();
	cm_x300_init_spi();
	cm_x300_init_rtc();
	cm_x300_init_ac97();
	cm_x300_init_wi2wi();
	cm_x300_init_bl();
}

static void __init cm_x300_fixup(struct machine_desc *mdesc, struct tag *tags,
				 char **cmdline, struct meminfo *mi)
{
	/* Make sure that mi->bank[0].start = PHYS_ADDR */
	for (; tags->hdr.size; tags = tag_next(tags))
		if (tags->hdr.tag == ATAG_MEM &&
			tags->u.mem.start == 0x80000000) {
			tags->u.mem.start = 0xa0000000;
			break;
		}
}

MACHINE_START(CM_X300, "CM-X300 module")
	.boot_params	= 0xa0000100,
	.map_io		= pxa3xx_map_io,
	.init_irq	= pxa3xx_init_irq,
	.timer		= &pxa_timer,
	.init_machine	= cm_x300_init,
	.fixup		= cm_x300_fixup,
MACHINE_END
