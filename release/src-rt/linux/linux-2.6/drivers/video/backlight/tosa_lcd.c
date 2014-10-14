/*
 *  LCD / Backlight control code for Sharp SL-6000x (tosa)
 *
 *  Copyright (c) 2005		Dirk Opfer
 *  Copyright (c) 2007,2008	Dmitry Baryshkov
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/spi/spi.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/lcd.h>
#include <linux/fb.h>

#include <asm/mach/sharpsl_param.h>

#include <mach/tosa.h>

#define POWER_IS_ON(pwr)	((pwr) <= FB_BLANK_NORMAL)

#define TG_REG0_VQV	0x0001
#define TG_REG0_COLOR	0x0002
#define TG_REG0_UD	0x0004
#define TG_REG0_LR	0x0008

#define	DAC_BASE	0x4e

struct tosa_lcd_data {
	struct spi_device *spi;
	struct lcd_device *lcd;
	struct i2c_client *i2c;

	int lcd_power;
	bool is_vga;
};

static int tosa_tg_send(struct spi_device *spi, int adrs, uint8_t data)
{
	u8 buf[1];
	struct spi_message msg;
	struct spi_transfer xfer = {
		.len		= 1,
		.cs_change	= 1,
		.tx_buf		= buf,
	};

	buf[0] = ((adrs & 0x07) << 5) | (data & 0x1f);
	spi_message_init(&msg);
	spi_message_add_tail(&xfer, &msg);

	return spi_sync(spi, &msg);
}

int tosa_bl_enable(struct spi_device *spi, int enable)
{
	/* bl_enable GP04=1 otherwise GP04=0*/
	return tosa_tg_send(spi, TG_GPODR2, enable? 0x01 : 0x00);
}
EXPORT_SYMBOL(tosa_bl_enable);

static void tosa_lcd_tg_init(struct tosa_lcd_data *data)
{
	/* TG on */
	gpio_set_value(TOSA_GPIO_TG_ON, 0);

	mdelay(60);

	/* delayed 0clk TCTL signal for VGA */
	tosa_tg_send(data->spi, TG_TPOSCTL, 0x00);
	/* GPOS0=powercontrol, GPOS1=GPIO, GPOS2=TCTL */
	tosa_tg_send(data->spi, TG_GPOSR, 0x02);
}

static void tosa_lcd_tg_on(struct tosa_lcd_data *data)
{
	struct spi_device *spi = data->spi;
	int value = TG_REG0_COLOR | TG_REG0_UD | TG_REG0_LR;

	if (data->is_vga)
		value |= TG_REG0_VQV;

	tosa_tg_send(spi, TG_PNLCTL, value);

	/* TG LCD pannel power up */
	tosa_tg_send(spi, TG_PINICTL,0x4);
	mdelay(50);

	/* TG LCD GVSS */
	tosa_tg_send(spi, TG_PINICTL,0x0);

	if (!data->i2c) {
		/* after the pannel is powered up the first time, we can access the i2c bus */
		/* so probe for the DAC */
		struct i2c_adapter *adap = i2c_get_adapter(0);
		struct i2c_board_info info = {
			.type	= "tosa-bl",
			.addr	= DAC_BASE,
			.platform_data = data->spi,
		};
		data->i2c = i2c_new_device(adap, &info);
	}
}

static void tosa_lcd_tg_off(struct tosa_lcd_data *data)
{
	struct spi_device *spi = data->spi;

	/* TG LCD VHSA off */
	tosa_tg_send(spi, TG_PINICTL,0x4);
	mdelay(50);

	/* TG LCD signal off */
	tosa_tg_send(spi, TG_PINICTL,0x6);
	mdelay(50);

	/* TG Off */
	gpio_set_value(TOSA_GPIO_TG_ON, 1);
	mdelay(100);
}

int tosa_lcd_set_power(struct lcd_device *lcd, int power)
{
	struct tosa_lcd_data *data = lcd_get_data(lcd);

	if (POWER_IS_ON(power) && !POWER_IS_ON(data->lcd_power))
		tosa_lcd_tg_on(data);

	if (!POWER_IS_ON(power) && POWER_IS_ON(data->lcd_power))
		tosa_lcd_tg_off(data);

	data->lcd_power = power;
	return 0;
}

static int tosa_lcd_get_power(struct lcd_device *lcd)
{
	struct tosa_lcd_data *data = lcd_get_data(lcd);

	return data->lcd_power;
}

static int tosa_lcd_set_mode(struct lcd_device *lcd, struct fb_videomode *mode)
{
	struct tosa_lcd_data *data = lcd_get_data(lcd);

	if (mode->xres == 320 || mode->yres == 320)
		data->is_vga = false;
	else
		data->is_vga = true;

	if (POWER_IS_ON(data->lcd_power))
		tosa_lcd_tg_on(data);

	return 0;
}

static struct lcd_ops tosa_lcd_ops = {
	.set_power = tosa_lcd_set_power,
	.get_power = tosa_lcd_get_power,
	.set_mode = tosa_lcd_set_mode,
};

static int __devinit tosa_lcd_probe(struct spi_device *spi)
{
	int ret;
	struct tosa_lcd_data *data;

	data = kzalloc(sizeof(struct tosa_lcd_data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->is_vga = true; /* default to VGA mode */

	/*
	 * bits_per_word cannot be configured in platform data
	 */
	spi->bits_per_word = 8;

	ret = spi_setup(spi);
	if (ret < 0)
		goto err_spi;

	data->spi = spi;
	dev_set_drvdata(&spi->dev, data);

	ret = gpio_request(TOSA_GPIO_TG_ON, "tg #pwr");
	if (ret < 0)
		goto err_gpio_tg;

	mdelay(60);

	ret = gpio_direction_output(TOSA_GPIO_TG_ON, 0);
	if (ret < 0)
		goto err_gpio_dir;

	mdelay(60);
	tosa_lcd_tg_init(data);

	tosa_lcd_tg_on(data);

	data->lcd = lcd_device_register("tosa-lcd", &spi->dev, data,
			&tosa_lcd_ops);

	if (IS_ERR(data->lcd)) {
		ret = PTR_ERR(data->lcd);
		data->lcd = NULL;
		goto err_register;
	}

	return 0;

err_register:
	tosa_lcd_tg_off(data);
err_gpio_dir:
	gpio_free(TOSA_GPIO_TG_ON);
err_gpio_tg:
	dev_set_drvdata(&spi->dev, NULL);
err_spi:
	kfree(data);
	return ret;
}

static int __devexit tosa_lcd_remove(struct spi_device *spi)
{
	struct tosa_lcd_data *data = dev_get_drvdata(&spi->dev);

	lcd_device_unregister(data->lcd);

	if (data->i2c)
		i2c_unregister_device(data->i2c);

	tosa_lcd_tg_off(data);

	gpio_free(TOSA_GPIO_TG_ON);
	dev_set_drvdata(&spi->dev, NULL);
	kfree(data);

	return 0;
}

#ifdef CONFIG_PM
static int tosa_lcd_suspend(struct spi_device *spi, pm_message_t state)
{
	struct tosa_lcd_data *data = dev_get_drvdata(&spi->dev);

	tosa_lcd_tg_off(data);

	return 0;
}

static int tosa_lcd_resume(struct spi_device *spi)
{
	struct tosa_lcd_data *data = dev_get_drvdata(&spi->dev);

	tosa_lcd_tg_init(data);
	if (POWER_IS_ON(data->lcd_power))
		tosa_lcd_tg_on(data);
	else
		tosa_lcd_tg_off(data);

	return 0;
}
#else
#define tosa_lcd_suspend	NULL
#define tosa_lcd_reume NULL
#endif

static struct spi_driver tosa_lcd_driver = {
	.driver = {
		.name		= "tosa-lcd",
		.owner		= THIS_MODULE,
	},
	.probe		= tosa_lcd_probe,
	.remove		= __devexit_p(tosa_lcd_remove),
	.suspend	= tosa_lcd_suspend,
	.resume		= tosa_lcd_resume,
};

static int __init tosa_lcd_init(void)
{
	return spi_register_driver(&tosa_lcd_driver);
}

static void __exit tosa_lcd_exit(void)
{
	spi_unregister_driver(&tosa_lcd_driver);
}

module_init(tosa_lcd_init);
module_exit(tosa_lcd_exit);

MODULE_AUTHOR("Dmitry Baryshkov");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("LCD/Backlight control for Sharp SL-6000 PDA");
MODULE_ALIAS("spi:tosa-lcd");
