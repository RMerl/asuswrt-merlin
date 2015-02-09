/*
 * linux/arch/arm/mach-omap2/board-omap3evm.c
 *
 * Copyright (C) 2008 Guangzhou EMA-Tech
 *
 * Modified from mach-omap2/board-omap3evm.c
 *
 * Initial code: Syed Mohammed Khasim
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
#include <linux/leds.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/gpio_keys.h>

#include <linux/regulator/machine.h>
#include <linux/i2c/twl.h>

#include <mach/hardware.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/flash.h>

#include <plat/board.h>
#include <plat/common.h>
#include <plat/gpmc.h>
#include <plat/nand.h>
#include <plat/usb.h>
#include <plat/timer-gp.h>
#include <plat/display.h>

#include <plat/mcspi.h>
#include <linux/input/matrix_keypad.h>
#include <linux/spi/spi.h>
#include <linux/spi/ads7846.h>
#include <linux/interrupt.h>
#include <linux/smsc911x.h>
#include <linux/i2c/at24.h>

#include "sdram-micron-mt46h32m32lf-6.h"
#include "mux.h"
#include "hsmmc.h"

#if defined(CONFIG_SMSC911X) || defined(CONFIG_SMSC911X_MODULE)
#define OMAP3STALKER_ETHR_START	0x2c000000
#define OMAP3STALKER_ETHR_SIZE	1024
#define OMAP3STALKER_ETHR_GPIO_IRQ	19
#define OMAP3STALKER_SMC911X_CS	5

static struct resource omap3stalker_smsc911x_resources[] = {
	[0] = {
	       .start	= OMAP3STALKER_ETHR_START,
	       .end	=
	       (OMAP3STALKER_ETHR_START + OMAP3STALKER_ETHR_SIZE - 1),
	       .flags	= IORESOURCE_MEM,
	},
	[1] = {
	       .start	= OMAP_GPIO_IRQ(OMAP3STALKER_ETHR_GPIO_IRQ),
	       .end	= OMAP_GPIO_IRQ(OMAP3STALKER_ETHR_GPIO_IRQ),
	       .flags	= (IORESOURCE_IRQ | IRQF_TRIGGER_LOW),
	},
};

static struct smsc911x_platform_config smsc911x_config = {
	.phy_interface	= PHY_INTERFACE_MODE_MII,
	.irq_polarity	= SMSC911X_IRQ_POLARITY_ACTIVE_LOW,
	.irq_type	= SMSC911X_IRQ_TYPE_OPEN_DRAIN,
	.flags		= (SMSC911X_USE_32BIT | SMSC911X_SAVE_MAC_ADDRESS),
};

static struct platform_device omap3stalker_smsc911x_device = {
	.name		= "smsc911x",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(omap3stalker_smsc911x_resources),
	.resource	= &omap3stalker_smsc911x_resources[0],
	.dev		= {
		.platform_data	= &smsc911x_config,
	},
};

static inline void __init omap3stalker_init_eth(void)
{
	int eth_cs;
	struct clk *l3ck;
	unsigned int rate;

	eth_cs = OMAP3STALKER_SMC911X_CS;

	l3ck = clk_get(NULL, "l3_ck");
	if (IS_ERR(l3ck))
		rate = 100000000;
	else
		rate = clk_get_rate(l3ck);

	omap_mux_init_gpio(19, OMAP_PIN_INPUT_PULLUP);
	if (gpio_request(OMAP3STALKER_ETHR_GPIO_IRQ, "SMC911x irq") < 0) {
		printk(KERN_ERR
		       "Failed to request GPIO%d for smc911x IRQ\n",
		       OMAP3STALKER_ETHR_GPIO_IRQ);
		return;
	}

	gpio_direction_input(OMAP3STALKER_ETHR_GPIO_IRQ);

	platform_device_register(&omap3stalker_smsc911x_device);
}

#else
static inline void __init omap3stalker_init_eth(void)
{
	return;
}
#endif

/*
 * OMAP3 DSS control signals
 */

#define DSS_ENABLE_GPIO	199
#define LCD_PANEL_BKLIGHT_GPIO	210
#define ENABLE_VPLL2_DEV_GRP	0xE0

static int lcd_enabled;
static int dvi_enabled;

static void __init omap3_stalker_display_init(void)
{
	return;
}

static int omap3_stalker_enable_lcd(struct omap_dss_device *dssdev)
{
	if (dvi_enabled) {
		printk(KERN_ERR "cannot enable LCD, DVI is enabled\n");
		return -EINVAL;
	}
	gpio_set_value(DSS_ENABLE_GPIO, 1);
	gpio_set_value(LCD_PANEL_BKLIGHT_GPIO, 1);
	lcd_enabled = 1;
	return 0;
}

static void omap3_stalker_disable_lcd(struct omap_dss_device *dssdev)
{
	gpio_set_value(DSS_ENABLE_GPIO, 0);
	gpio_set_value(LCD_PANEL_BKLIGHT_GPIO, 0);
	lcd_enabled = 0;
}

static struct omap_dss_device omap3_stalker_lcd_device = {
	.name			= "lcd",
	.driver_name		= "generic_panel",
	.phy.dpi.data_lines	= 24,
	.type			= OMAP_DISPLAY_TYPE_DPI,
	.platform_enable	= omap3_stalker_enable_lcd,
	.platform_disable	= omap3_stalker_disable_lcd,
};

static int omap3_stalker_enable_tv(struct omap_dss_device *dssdev)
{
	return 0;
}

static void omap3_stalker_disable_tv(struct omap_dss_device *dssdev)
{
}

static struct omap_dss_device omap3_stalker_tv_device = {
	.name			= "tv",
	.driver_name		= "venc",
	.type			= OMAP_DISPLAY_TYPE_VENC,
#if defined(CONFIG_OMAP2_VENC_OUT_TYPE_SVIDEO)
	.phy.venc.type		= OMAP_DSS_VENC_TYPE_SVIDEO,
#elif defined(CONFIG_OMAP2_VENC_OUT_TYPE_COMPOSITE)
	.u.venc.type		= OMAP_DSS_VENC_TYPE_COMPOSITE,
#endif
	.platform_enable	= omap3_stalker_enable_tv,
	.platform_disable	= omap3_stalker_disable_tv,
};

static int omap3_stalker_enable_dvi(struct omap_dss_device *dssdev)
{
	if (lcd_enabled) {
		printk(KERN_ERR "cannot enable DVI, LCD is enabled\n");
		return -EINVAL;
	}
	gpio_set_value(DSS_ENABLE_GPIO, 1);
	dvi_enabled = 1;
	return 0;
}

static void omap3_stalker_disable_dvi(struct omap_dss_device *dssdev)
{
	gpio_set_value(DSS_ENABLE_GPIO, 0);
	dvi_enabled = 0;
}

static struct omap_dss_device omap3_stalker_dvi_device = {
	.name			= "dvi",
	.driver_name		= "generic_panel",
	.type			= OMAP_DISPLAY_TYPE_DPI,
	.phy.dpi.data_lines	= 24,
	.platform_enable	= omap3_stalker_enable_dvi,
	.platform_disable	= omap3_stalker_disable_dvi,
};

static struct omap_dss_device *omap3_stalker_dss_devices[] = {
	&omap3_stalker_lcd_device,
	&omap3_stalker_tv_device,
	&omap3_stalker_dvi_device,
};

static struct omap_dss_board_info omap3_stalker_dss_data = {
	.num_devices	= ARRAY_SIZE(omap3_stalker_dss_devices),
	.devices	= omap3_stalker_dss_devices,
	.default_device	= &omap3_stalker_dvi_device,
};

static struct platform_device omap3_stalker_dss_device = {
	.name	= "omapdss",
	.id	= -1,
	.dev	= {
		.platform_data	= &omap3_stalker_dss_data,
	},
};

static struct regulator_consumer_supply omap3stalker_vmmc1_supply = {
	.supply		= "vmmc",
};

static struct regulator_consumer_supply omap3stalker_vsim_supply = {
	.supply		= "vmmc_aux",
};

/* VMMC1 for MMC1 pins CMD, CLK, DAT0..DAT3 (20 mA, plus card == max 220 mA) */
static struct regulator_init_data omap3stalker_vmmc1 = {
	.constraints		= {
		.min_uV			= 1850000,
		.max_uV			= 3150000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
		| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_VOLTAGE
		| REGULATOR_CHANGE_MODE | REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &omap3stalker_vmmc1_supply,
};

/* VSIM for MMC1 pins DAT4..DAT7 (2 mA, plus card == max 50 mA) */
static struct regulator_init_data omap3stalker_vsim = {
	.constraints		= {
		.min_uV			= 1800000,
		.max_uV			= 3000000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
		| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_VOLTAGE
		| REGULATOR_CHANGE_MODE | REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &omap3stalker_vsim_supply,
};

static struct omap2_hsmmc_info mmc[] = {
	{
	 .mmc		= 1,
	 .wires		= 4,
	 .gpio_cd	= -EINVAL,
	 .gpio_wp	= 23,
	 },
	{}			/* Terminator */
};

static struct gpio_keys_button gpio_buttons[] = {
	{
	 .code		= BTN_EXTRA,
	 .gpio		= 18,
	 .desc		= "user",
	 .wakeup	= 1,
	 },
};

static struct gpio_keys_platform_data gpio_key_info = {
	.buttons	= gpio_buttons,
	.nbuttons	= ARRAY_SIZE(gpio_buttons),
};

static struct platform_device keys_gpio = {
	.name		= "gpio-keys",
	.id		= -1,
	.dev		= {
		.platform_data	= &gpio_key_info,
	},
};

static struct gpio_led gpio_leds[] = {
	{
	 .name			= "stalker:D8:usr0",
	 .default_trigger	= "default-on",
	 .gpio			= 126,
	 },
	{
	 .name			= "stalker:D9:usr1",
	 .default_trigger	= "default-on",
	 .gpio			= 127,
	 },
	{
	 .name			= "stalker:D3:mmc0",
	 .gpio			= -EINVAL,	/* gets replaced */
	 .active_low		= true,
	 .default_trigger	= "mmc0",
	 },
	{
	 .name			= "stalker:D4:heartbeat",
	 .gpio			= -EINVAL,	/* gets replaced */
	 .active_low		= true,
	 .default_trigger	= "heartbeat",
	 },
};

static struct gpio_led_platform_data gpio_led_info = {
	.leds		= gpio_leds,
	.num_leds	= ARRAY_SIZE(gpio_leds),
};

static struct platform_device leds_gpio = {
	.name	= "leds-gpio",
	.id	= -1,
	.dev	= {
		.platform_data	= &gpio_led_info,
	},
};

static int
omap3stalker_twl_gpio_setup(struct device *dev,
			    unsigned gpio, unsigned ngpio)
{
	/* gpio + 0 is "mmc0_cd" (input/IRQ) */
	omap_mux_init_gpio(23, OMAP_PIN_INPUT);
	mmc[0].gpio_cd = gpio + 0;
	omap2_hsmmc_init(mmc);

	/* link regulators to MMC adapters */
	omap3stalker_vmmc1_supply.dev = mmc[0].dev;
	omap3stalker_vsim_supply.dev = mmc[0].dev;

	/*
	 * Most GPIOs are for USB OTG.  Some are mostly sent to
	 * the P2 connector; notably LEDA for the LCD backlight.
	 */

	/* TWL4030_GPIO_MAX + 0 == ledA, LCD Backlight control */
	gpio_request(gpio + TWL4030_GPIO_MAX, "EN_LCD_BKL");
	gpio_direction_output(gpio + TWL4030_GPIO_MAX, 0);

	/* gpio + 7 == DVI Enable */
	gpio_request(gpio + 7, "EN_DVI");
	gpio_direction_output(gpio + 7, 0);

	/* TWL4030_GPIO_MAX + 1 == ledB (out, mmc0) */
	gpio_leds[2].gpio = gpio + TWL4030_GPIO_MAX + 1;
	/* GPIO + 13 == ledsync (out, heartbeat) */
	gpio_leds[3].gpio = gpio + 13;

	platform_device_register(&leds_gpio);
	return 0;
}

static struct twl4030_gpio_platform_data omap3stalker_gpio_data = {
	.gpio_base	= OMAP_MAX_GPIO_LINES,
	.irq_base	= TWL4030_GPIO_IRQ_BASE,
	.irq_end	= TWL4030_GPIO_IRQ_END,
	.use_leds	= true,
	.setup		= omap3stalker_twl_gpio_setup,
};

static struct twl4030_usb_data omap3stalker_usb_data = {
	.usb_mode	= T2_USB_MODE_ULPI,
};

static int board_keymap[] = {
	KEY(0, 0, KEY_LEFT),
	KEY(0, 1, KEY_DOWN),
	KEY(0, 2, KEY_ENTER),
	KEY(0, 3, KEY_M),

	KEY(1, 0, KEY_RIGHT),
	KEY(1, 1, KEY_UP),
	KEY(1, 2, KEY_I),
	KEY(1, 3, KEY_N),

	KEY(2, 0, KEY_A),
	KEY(2, 1, KEY_E),
	KEY(2, 2, KEY_J),
	KEY(2, 3, KEY_O),

	KEY(3, 0, KEY_B),
	KEY(3, 1, KEY_F),
	KEY(3, 2, KEY_K),
	KEY(3, 3, KEY_P)
};

static struct matrix_keymap_data board_map_data = {
	.keymap		= board_keymap,
	.keymap_size	= ARRAY_SIZE(board_keymap),
};

static struct twl4030_keypad_data omap3stalker_kp_data = {
	.keymap_data	= &board_map_data,
	.rows		= 4,
	.cols		= 4,
	.rep		= 1,
};

static struct twl4030_madc_platform_data omap3stalker_madc_data = {
	.irq_line	= 1,
};

static struct twl4030_codec_audio_data omap3stalker_audio_data = {
	.audio_mclk	= 26000000,
};

static struct twl4030_codec_data omap3stalker_codec_data = {
	.audio_mclk	= 26000000,
	.audio		= &omap3stalker_audio_data,
};

static struct regulator_consumer_supply omap3_stalker_vdda_dac_supply = {
	.supply		= "vdda_dac",
	.dev		= &omap3_stalker_dss_device.dev,
};

/* VDAC for DSS driving S-Video */
static struct regulator_init_data omap3_stalker_vdac = {
	.constraints		= {
		.min_uV			= 1800000,
		.max_uV			= 1800000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
		| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
		| REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &omap3_stalker_vdda_dac_supply,
};

/* VPLL2 for digital video outputs */
static struct regulator_consumer_supply omap3_stalker_vpll2_supply = {
	.supply		= "vdds_dsi",
	.dev		= &omap3_stalker_lcd_device.dev,
};

static struct regulator_init_data omap3_stalker_vpll2 = {
	.constraints		= {
		.name			= "VDVI",
		.min_uV			= 1800000,
		.max_uV			= 1800000,
		.apply_uV = true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
		| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
		| REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &omap3_stalker_vpll2_supply,
};

static struct twl4030_platform_data omap3stalker_twldata = {
	.irq_base	= TWL4030_IRQ_BASE,
	.irq_end	= TWL4030_IRQ_END,

	/* platform_data for children goes here */
	.keypad		= &omap3stalker_kp_data,
	.madc		= &omap3stalker_madc_data,
	.usb		= &omap3stalker_usb_data,
	.gpio		= &omap3stalker_gpio_data,
	.codec		= &omap3stalker_codec_data,
	.vdac		= &omap3_stalker_vdac,
	.vpll2		= &omap3_stalker_vpll2,
};

static struct i2c_board_info __initdata omap3stalker_i2c_boardinfo[] = {
	{
	 I2C_BOARD_INFO("twl4030", 0x48),
	 .flags		= I2C_CLIENT_WAKE,
	 .irq		= INT_34XX_SYS_NIRQ,
	 .platform_data	= &omap3stalker_twldata,
	 },
};

static struct at24_platform_data fram_info = {
	.byte_len	= (64 * 1024) / 8,
	.page_size	= 8192,
	.flags		= AT24_FLAG_ADDR16 | AT24_FLAG_IRUGO,
};

static struct i2c_board_info __initdata omap3stalker_i2c_boardinfo3[] = {
	{
	 I2C_BOARD_INFO("24c64", 0x50),
	 .flags		= I2C_CLIENT_WAKE,
	 .platform_data	= &fram_info,
	 },
};

static int __init omap3_stalker_i2c_init(void)
{
	/*
	 * REVISIT: These entries can be set in omap3evm_twl_data
	 * after a merge with MFD tree
	 */
	omap3stalker_twldata.vmmc1 = &omap3stalker_vmmc1;
	omap3stalker_twldata.vsim = &omap3stalker_vsim;

	omap_register_i2c_bus(1, 2600, omap3stalker_i2c_boardinfo,
			      ARRAY_SIZE(omap3stalker_i2c_boardinfo));
	omap_register_i2c_bus(2, 400, NULL, 0);
	omap_register_i2c_bus(3, 400, omap3stalker_i2c_boardinfo3,
			      ARRAY_SIZE(omap3stalker_i2c_boardinfo3));
	return 0;
}

#define OMAP3_STALKER_TS_GPIO	175
static void ads7846_dev_init(void)
{
	if (gpio_request(OMAP3_STALKER_TS_GPIO, "ADS7846 pendown") < 0)
		printk(KERN_ERR "can't get ads7846 pen down GPIO\n");

	gpio_direction_input(OMAP3_STALKER_TS_GPIO);
	gpio_set_debounce(OMAP3_STALKER_TS_GPIO, 310);
}

static int ads7846_get_pendown_state(void)
{
	return !gpio_get_value(OMAP3_STALKER_TS_GPIO);
}

static struct ads7846_platform_data ads7846_config = {
	.x_max			= 0x0fff,
	.y_max			= 0x0fff,
	.x_plate_ohms		= 180,
	.pressure_max		= 255,
	.debounce_max		= 10,
	.debounce_tol		= 3,
	.debounce_rep		= 1,
	.get_pendown_state	= ads7846_get_pendown_state,
	.keep_vref_on		= 1,
	.settle_delay_usecs	= 150,
};

static struct omap2_mcspi_device_config ads7846_mcspi_config = {
	.turbo_mode		= 0,
	.single_channel		= 1,	/* 0: slave, 1: master */
};

struct spi_board_info omap3stalker_spi_board_info[] = {
	[0] = {
	       .modalias	= "ads7846",
	       .bus_num		= 1,
	       .chip_select	= 0,
	       .max_speed_hz	= 1500000,
	       .controller_data	= &ads7846_mcspi_config,
	       .irq		= OMAP_GPIO_IRQ(OMAP3_STALKER_TS_GPIO),
	       .platform_data	= &ads7846_config,
	},
};

static struct omap_board_config_kernel omap3_stalker_config[] __initdata = {
};

static void __init omap3_stalker_init_irq(void)
{
	omap_board_config = omap3_stalker_config;
	omap_board_config_size = ARRAY_SIZE(omap3_stalker_config);
	omap2_init_common_hw(mt46h32m32lf6_sdrc_params, NULL);
	omap_init_irq();
#ifdef CONFIG_OMAP_32K_TIMER
	omap2_gp_clockevent_set_gptimer(12);
#endif
	omap_gpio_init();
}

static struct platform_device *omap3_stalker_devices[] __initdata = {
	&omap3_stalker_dss_device,
	&keys_gpio,
};

static struct ehci_hcd_omap_platform_data ehci_pdata __initconst = {
	.port_mode[0] = EHCI_HCD_OMAP_MODE_UNKNOWN,
	.port_mode[1] = EHCI_HCD_OMAP_MODE_PHY,
	.port_mode[2] = EHCI_HCD_OMAP_MODE_UNKNOWN,

	.phy_reset = true,
	.reset_gpio_port[0] = -EINVAL,
	.reset_gpio_port[1] = 21,
	.reset_gpio_port[2] = -EINVAL,
};

#ifdef CONFIG_OMAP_MUX
static struct omap_board_mux board_mux[] __initdata = {
	OMAP3_MUX(SYS_NIRQ, OMAP_MUX_MODE0 | OMAP_PIN_INPUT_PULLUP |
		  OMAP_PIN_OFF_INPUT_PULLUP | OMAP_PIN_OFF_WAKEUPENABLE),
	OMAP3_MUX(MCSPI1_CS1, OMAP_MUX_MODE4 | OMAP_PIN_INPUT_PULLUP |
		  OMAP_PIN_OFF_INPUT_PULLUP | OMAP_PIN_OFF_WAKEUPENABLE),
	{.reg_offset = OMAP_MUX_TERMINATOR},
};
#else
#define board_mux	NULL
#endif

static struct omap_musb_board_data musb_board_data = {
	.interface_type	= MUSB_INTERFACE_ULPI,
	.mode		= MUSB_OTG,
	.power		= 100,
};

static void __init omap3_stalker_init(void)
{
	omap3_mux_init(board_mux, OMAP_PACKAGE_CUS);

	omap3_stalker_i2c_init();

	platform_add_devices(omap3_stalker_devices,
			     ARRAY_SIZE(omap3_stalker_devices));

	spi_register_board_info(omap3stalker_spi_board_info,
				ARRAY_SIZE(omap3stalker_spi_board_info));

	omap_serial_init();
	usb_musb_init(&musb_board_data);
	usb_ehci_init(&ehci_pdata);
	ads7846_dev_init();

	omap_mux_init_gpio(21, OMAP_PIN_OUTPUT);
	omap_mux_init_gpio(18, OMAP_PIN_INPUT_PULLUP);

	omap3stalker_init_eth();
	omap3_stalker_display_init();
/* Ensure SDRC pins are mux'd for self-refresh */
	omap_mux_init_signal("sdr_cke0", OMAP_PIN_OUTPUT);
	omap_mux_init_signal("sdr_cke1", OMAP_PIN_OUTPUT);
}

MACHINE_START(SBC3530, "OMAP3 STALKER")
	/* Maintainer: Jason Lam -lzg@ema-tech.com */
	.phys_io		= 0x48000000,
	.io_pg_offst		= ((0xfa000000) >> 18) & 0xfffc,
	.boot_params		= 0x80000100,
	.map_io			= omap3_map_io,
	.init_irq		= omap3_stalker_init_irq,
	.init_machine		= omap3_stalker_init,
	.timer			= &omap_timer,
MACHINE_END
