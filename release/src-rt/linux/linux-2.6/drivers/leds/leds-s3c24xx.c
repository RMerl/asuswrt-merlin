/* drivers/leds/leds-s3c24xx.c
 *
 * (c) 2006 Simtec Electronics
 *	http://armlinux.simtec.co.uk/
 *	Ben Dooks <ben@simtec.co.uk>
 *
 * S3C24XX - LEDs GPIO driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/gpio.h>
#include <linux/slab.h>

#include <mach/hardware.h>
#include <mach/regs-gpio.h>
#include <mach/leds-gpio.h>

/* our context */

struct s3c24xx_gpio_led {
	struct led_classdev		 cdev;
	struct s3c24xx_led_platdata	*pdata;
};

static inline struct s3c24xx_gpio_led *pdev_to_gpio(struct platform_device *dev)
{
	return platform_get_drvdata(dev);
}

static inline struct s3c24xx_gpio_led *to_gpio(struct led_classdev *led_cdev)
{
	return container_of(led_cdev, struct s3c24xx_gpio_led, cdev);
}

static void s3c24xx_led_set(struct led_classdev *led_cdev,
			    enum led_brightness value)
{
	struct s3c24xx_gpio_led *led = to_gpio(led_cdev);
	struct s3c24xx_led_platdata *pd = led->pdata;

	/* there will be a short delay between setting the output and
	 * going from output to input when using tristate. */

	s3c2410_gpio_setpin(pd->gpio, (value ? 1 : 0) ^
			    (pd->flags & S3C24XX_LEDF_ACTLOW));

	if (pd->flags & S3C24XX_LEDF_TRISTATE)
		s3c2410_gpio_cfgpin(pd->gpio,
			value ? S3C2410_GPIO_OUTPUT : S3C2410_GPIO_INPUT);

}

static int s3c24xx_led_remove(struct platform_device *dev)
{
	struct s3c24xx_gpio_led *led = pdev_to_gpio(dev);

	led_classdev_unregister(&led->cdev);
	kfree(led);

	return 0;
}

static int s3c24xx_led_probe(struct platform_device *dev)
{
	struct s3c24xx_led_platdata *pdata = dev->dev.platform_data;
	struct s3c24xx_gpio_led *led;
	int ret;

	led = kzalloc(sizeof(struct s3c24xx_gpio_led), GFP_KERNEL);
	if (led == NULL) {
		dev_err(&dev->dev, "No memory for device\n");
		return -ENOMEM;
	}

	platform_set_drvdata(dev, led);

	led->cdev.brightness_set = s3c24xx_led_set;
	led->cdev.default_trigger = pdata->def_trigger;
	led->cdev.name = pdata->name;
	led->cdev.flags |= LED_CORE_SUSPENDRESUME;

	led->pdata = pdata;

	/* no point in having a pull-up if we are always driving */

	if (pdata->flags & S3C24XX_LEDF_TRISTATE) {
		s3c2410_gpio_setpin(pdata->gpio, 0);
		s3c2410_gpio_cfgpin(pdata->gpio, S3C2410_GPIO_INPUT);
	} else {
		s3c2410_gpio_pullup(pdata->gpio, 0);
		s3c2410_gpio_setpin(pdata->gpio, 0);
		s3c2410_gpio_cfgpin(pdata->gpio, S3C2410_GPIO_OUTPUT);
	}

	/* register our new led device */

	ret = led_classdev_register(&dev->dev, &led->cdev);
	if (ret < 0) {
		dev_err(&dev->dev, "led_classdev_register failed\n");
		kfree(led);
		return ret;
	}

	return 0;
}

static struct platform_driver s3c24xx_led_driver = {
	.probe		= s3c24xx_led_probe,
	.remove		= s3c24xx_led_remove,
	.driver		= {
		.name		= "s3c24xx_led",
		.owner		= THIS_MODULE,
	},
};

static int __init s3c24xx_led_init(void)
{
	return platform_driver_register(&s3c24xx_led_driver);
}

static void __exit s3c24xx_led_exit(void)
{
	platform_driver_unregister(&s3c24xx_led_driver);
}

module_init(s3c24xx_led_init);
module_exit(s3c24xx_led_exit);

MODULE_AUTHOR("Ben Dooks <ben@simtec.co.uk>");
MODULE_DESCRIPTION("S3C24XX LED driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:s3c24xx_led");
