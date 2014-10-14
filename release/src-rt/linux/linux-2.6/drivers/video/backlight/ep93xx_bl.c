/*
 * Driver for the Cirrus EP93xx lcd backlight
 *
 * Copyright (c) 2010 H Hartley Sweeten <hsweeten@visionengravers.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This driver controls the pulse width modulated brightness control output,
 * BRIGHT, on the Cirrus EP9307, EP9312, and EP9315 processors.
 */


#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/fb.h>
#include <linux/backlight.h>

#include <mach/hardware.h>

#define EP93XX_RASTER_REG(x)		(EP93XX_RASTER_BASE + (x))
#define EP93XX_RASTER_BRIGHTNESS	EP93XX_RASTER_REG(0x20)

#define EP93XX_MAX_COUNT		255
#define EP93XX_MAX_BRIGHT		255
#define EP93XX_DEF_BRIGHT		128

struct ep93xxbl {
	void __iomem *mmio;
	int brightness;
};

static int ep93xxbl_set(struct backlight_device *bl, int brightness)
{
	struct ep93xxbl *ep93xxbl = bl_get_data(bl);

	__raw_writel((brightness << 8) | EP93XX_MAX_COUNT, ep93xxbl->mmio);

	ep93xxbl->brightness = brightness;

	return 0;
}

static int ep93xxbl_update_status(struct backlight_device *bl)
{
	int brightness = bl->props.brightness;

	if (bl->props.power != FB_BLANK_UNBLANK ||
	    bl->props.fb_blank != FB_BLANK_UNBLANK)
		brightness = 0;

	return ep93xxbl_set(bl, brightness);
}

static int ep93xxbl_get_brightness(struct backlight_device *bl)
{
	struct ep93xxbl *ep93xxbl = bl_get_data(bl);

	return ep93xxbl->brightness;
}

static const struct backlight_ops ep93xxbl_ops = {
	.update_status	= ep93xxbl_update_status,
	.get_brightness	= ep93xxbl_get_brightness,
};

static int __init ep93xxbl_probe(struct platform_device *dev)
{
	struct ep93xxbl *ep93xxbl;
	struct backlight_device *bl;
	struct backlight_properties props;

	ep93xxbl = devm_kzalloc(&dev->dev, sizeof(*ep93xxbl), GFP_KERNEL);
	if (!ep93xxbl)
		return -ENOMEM;

	/*
	 * This register is located in the range already ioremap'ed by
	 * the framebuffer driver.  A MFD driver seems a bit of overkill
	 * to handle this so use the static I/O mapping; this address
	 * is already virtual.
	 *
	 * NOTE: No locking is required; the framebuffer does not touch
	 * this register.
	 */
	ep93xxbl->mmio = EP93XX_RASTER_BRIGHTNESS;

	memset(&props, 0, sizeof(struct backlight_properties));
	props.type = BACKLIGHT_RAW;
	props.max_brightness = EP93XX_MAX_BRIGHT;
	bl = backlight_device_register(dev->name, &dev->dev, ep93xxbl,
				       &ep93xxbl_ops, &props);
	if (IS_ERR(bl))
		return PTR_ERR(bl);

	bl->props.brightness = EP93XX_DEF_BRIGHT;

	platform_set_drvdata(dev, bl);

	ep93xxbl_update_status(bl);

	return 0;
}

static int ep93xxbl_remove(struct platform_device *dev)
{
	struct backlight_device *bl = platform_get_drvdata(dev);

	backlight_device_unregister(bl);
	platform_set_drvdata(dev, NULL);
	return 0;
}

#ifdef CONFIG_PM
static int ep93xxbl_suspend(struct platform_device *dev, pm_message_t state)
{
	struct backlight_device *bl = platform_get_drvdata(dev);

	return ep93xxbl_set(bl, 0);
}

static int ep93xxbl_resume(struct platform_device *dev)
{
	struct backlight_device *bl = platform_get_drvdata(dev);

	backlight_update_status(bl);
	return 0;
}
#else
#define ep93xxbl_suspend	NULL
#define ep93xxbl_resume		NULL
#endif

static struct platform_driver ep93xxbl_driver = {
	.driver		= {
		.name	= "ep93xx-bl",
		.owner	= THIS_MODULE,
	},
	.probe		= ep93xxbl_probe,
	.remove		= __devexit_p(ep93xxbl_remove),
	.suspend	= ep93xxbl_suspend,
	.resume		= ep93xxbl_resume,
};

static int __init ep93xxbl_init(void)
{
	return platform_driver_register(&ep93xxbl_driver);
}
module_init(ep93xxbl_init);

static void __exit ep93xxbl_exit(void)
{
	platform_driver_unregister(&ep93xxbl_driver);
}
module_exit(ep93xxbl_exit);

MODULE_DESCRIPTION("EP93xx Backlight Driver");
MODULE_AUTHOR("H Hartley Sweeten <hsweeten@visionengravers.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:ep93xx-bl");
