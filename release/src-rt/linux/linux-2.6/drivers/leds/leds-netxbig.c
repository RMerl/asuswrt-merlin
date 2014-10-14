/*
 * leds-netxbig.c - Driver for the 2Big and 5Big Network series LEDs
 *
 * Copyright (C) 2010 LaCie
 *
 * Author: Simon Guinot <sguinot@lacie.com>
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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/leds.h>
#include <mach/leds-netxbig.h>

/*
 * GPIO extension bus.
 */

static DEFINE_SPINLOCK(gpio_ext_lock);

static void gpio_ext_set_addr(struct netxbig_gpio_ext *gpio_ext, int addr)
{
	int pin;

	for (pin = 0; pin < gpio_ext->num_addr; pin++)
		gpio_set_value(gpio_ext->addr[pin], (addr >> pin) & 1);
}

static void gpio_ext_set_data(struct netxbig_gpio_ext *gpio_ext, int data)
{
	int pin;

	for (pin = 0; pin < gpio_ext->num_data; pin++)
		gpio_set_value(gpio_ext->data[pin], (data >> pin) & 1);
}

static void gpio_ext_enable_select(struct netxbig_gpio_ext *gpio_ext)
{
	/* Enable select is done on the raising edge. */
	gpio_set_value(gpio_ext->enable, 0);
	gpio_set_value(gpio_ext->enable, 1);
}

static void gpio_ext_set_value(struct netxbig_gpio_ext *gpio_ext,
			       int addr, int value)
{
	unsigned long flags;

	spin_lock_irqsave(&gpio_ext_lock, flags);
	gpio_ext_set_addr(gpio_ext, addr);
	gpio_ext_set_data(gpio_ext, value);
	gpio_ext_enable_select(gpio_ext);
	spin_unlock_irqrestore(&gpio_ext_lock, flags);
}

static int __devinit gpio_ext_init(struct netxbig_gpio_ext *gpio_ext)
{
	int err;
	int i;

	if (unlikely(!gpio_ext))
		return -EINVAL;

	/* Configure address GPIOs. */
	for (i = 0; i < gpio_ext->num_addr; i++) {
		err = gpio_request(gpio_ext->addr[i], "GPIO extension addr");
		if (err)
			goto err_free_addr;
		err = gpio_direction_output(gpio_ext->addr[i], 0);
		if (err) {
			gpio_free(gpio_ext->addr[i]);
			goto err_free_addr;
		}
	}
	/* Configure data GPIOs. */
	for (i = 0; i < gpio_ext->num_data; i++) {
		err = gpio_request(gpio_ext->data[i], "GPIO extension data");
		if (err)
			goto err_free_data;
		err = gpio_direction_output(gpio_ext->data[i], 0);
		if (err) {
			gpio_free(gpio_ext->data[i]);
			goto err_free_data;
		}
	}
	/* Configure "enable select" GPIO. */
	err = gpio_request(gpio_ext->enable, "GPIO extension enable");
	if (err)
		goto err_free_data;
	err = gpio_direction_output(gpio_ext->enable, 0);
	if (err) {
		gpio_free(gpio_ext->enable);
		goto err_free_data;
	}

	return 0;

err_free_data:
	for (i = i - 1; i >= 0; i--)
		gpio_free(gpio_ext->data[i]);
	i = gpio_ext->num_addr;
err_free_addr:
	for (i = i - 1; i >= 0; i--)
		gpio_free(gpio_ext->addr[i]);

	return err;
}

static void __devexit gpio_ext_free(struct netxbig_gpio_ext *gpio_ext)
{
	int i;

	gpio_free(gpio_ext->enable);
	for (i = gpio_ext->num_addr - 1; i >= 0; i--)
		gpio_free(gpio_ext->addr[i]);
	for (i = gpio_ext->num_data - 1; i >= 0; i--)
		gpio_free(gpio_ext->data[i]);
}

/*
 * Class LED driver.
 */

struct netxbig_led_data {
	struct netxbig_gpio_ext	*gpio_ext;
	struct led_classdev	cdev;
	int			mode_addr;
	int			*mode_val;
	int			bright_addr;
	int			bright_max;
	struct			netxbig_led_timer *timer;
	int			num_timer;
	enum netxbig_led_mode	mode;
	int			sata;
	spinlock_t		lock;
};

static int netxbig_led_get_timer_mode(enum netxbig_led_mode *mode,
				      unsigned long delay_on,
				      unsigned long delay_off,
				      struct netxbig_led_timer *timer,
				      int num_timer)
{
	int i;

	for (i = 0; i < num_timer; i++) {
		if (timer[i].delay_on == delay_on &&
		    timer[i].delay_off == delay_off) {
			*mode = timer[i].mode;
			return 0;
		}
	}
	return -EINVAL;
}

static int netxbig_led_blink_set(struct led_classdev *led_cdev,
				 unsigned long *delay_on,
				 unsigned long *delay_off)
{
	struct netxbig_led_data *led_dat =
		container_of(led_cdev, struct netxbig_led_data, cdev);
	enum netxbig_led_mode mode;
	int mode_val;
	int ret;

	/* Look for a LED mode with the requested timer frequency. */
	ret = netxbig_led_get_timer_mode(&mode, *delay_on, *delay_off,
					 led_dat->timer, led_dat->num_timer);
	if (ret < 0)
		return ret;

	mode_val = led_dat->mode_val[mode];
	if (mode_val == NETXBIG_LED_INVALID_MODE)
		return -EINVAL;

	spin_lock_irq(&led_dat->lock);

	gpio_ext_set_value(led_dat->gpio_ext, led_dat->mode_addr, mode_val);
	led_dat->mode = mode;

	spin_unlock_irq(&led_dat->lock);

	return 0;
}

static void netxbig_led_set(struct led_classdev *led_cdev,
			    enum led_brightness value)
{
	struct netxbig_led_data *led_dat =
		container_of(led_cdev, struct netxbig_led_data, cdev);
	enum netxbig_led_mode mode;
	int mode_val, bright_val;
	int set_brightness = 1;
	unsigned long flags;

	spin_lock_irqsave(&led_dat->lock, flags);

	if (value == LED_OFF) {
		mode = NETXBIG_LED_OFF;
		set_brightness = 0;
	} else {
		if (led_dat->sata)
			mode = NETXBIG_LED_SATA;
		else if (led_dat->mode == NETXBIG_LED_OFF)
			mode = NETXBIG_LED_ON;
		else /* Keep 'timer' mode. */
			mode = led_dat->mode;
	}
	mode_val = led_dat->mode_val[mode];

	gpio_ext_set_value(led_dat->gpio_ext, led_dat->mode_addr, mode_val);
	led_dat->mode = mode;
	/*
	 * Note that the brightness register is shared between all the
	 * SATA LEDs. So, change the brightness setting for a single
	 * SATA LED will affect all the others.
	 */
	if (set_brightness) {
		bright_val = DIV_ROUND_UP(value * led_dat->bright_max,
					  LED_FULL);
		gpio_ext_set_value(led_dat->gpio_ext,
				   led_dat->bright_addr, bright_val);
	}

	spin_unlock_irqrestore(&led_dat->lock, flags);
}

static ssize_t netxbig_led_sata_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buff, size_t count)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct netxbig_led_data *led_dat =
		container_of(led_cdev, struct netxbig_led_data, cdev);
	unsigned long enable;
	enum netxbig_led_mode mode;
	int mode_val;
	int ret;

	ret = strict_strtoul(buff, 10, &enable);
	if (ret < 0)
		return ret;

	enable = !!enable;

	spin_lock_irq(&led_dat->lock);

	if (led_dat->sata == enable) {
		ret = count;
		goto exit_unlock;
	}

	if (led_dat->mode != NETXBIG_LED_ON &&
	    led_dat->mode != NETXBIG_LED_SATA)
		mode = led_dat->mode; /* Keep modes 'off' and 'timer'. */
	else if (enable)
		mode = NETXBIG_LED_SATA;
	else
		mode = NETXBIG_LED_ON;

	mode_val = led_dat->mode_val[mode];
	if (mode_val == NETXBIG_LED_INVALID_MODE) {
		ret = -EINVAL;
		goto exit_unlock;
	}

	gpio_ext_set_value(led_dat->gpio_ext, led_dat->mode_addr, mode_val);
	led_dat->mode = mode;
	led_dat->sata = enable;

	ret = count;

exit_unlock:
	spin_unlock_irq(&led_dat->lock);

	return ret;
}

static ssize_t netxbig_led_sata_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct netxbig_led_data *led_dat =
		container_of(led_cdev, struct netxbig_led_data, cdev);

	return sprintf(buf, "%d\n", led_dat->sata);
}

static DEVICE_ATTR(sata, 0644, netxbig_led_sata_show, netxbig_led_sata_store);

static void __devexit delete_netxbig_led(struct netxbig_led_data *led_dat)
{
	if (led_dat->mode_val[NETXBIG_LED_SATA] != NETXBIG_LED_INVALID_MODE)
		device_remove_file(led_dat->cdev.dev, &dev_attr_sata);
	led_classdev_unregister(&led_dat->cdev);
}

static int __devinit
create_netxbig_led(struct platform_device *pdev,
		   struct netxbig_led_data *led_dat,
		   const struct netxbig_led *template)
{
	struct netxbig_led_platform_data *pdata = pdev->dev.platform_data;
	int ret;

	spin_lock_init(&led_dat->lock);
	led_dat->gpio_ext = pdata->gpio_ext;
	led_dat->cdev.name = template->name;
	led_dat->cdev.default_trigger = template->default_trigger;
	led_dat->cdev.blink_set = netxbig_led_blink_set;
	led_dat->cdev.brightness_set = netxbig_led_set;
	/*
	 * Because the GPIO extension bus don't allow to read registers
	 * value, there is no way to probe the LED initial state.
	 * So, the initial sysfs LED value for the "brightness" and "sata"
	 * attributes are inconsistent.
	 *
	 * Note that the initial LED state can't be reconfigured.
	 * The reason is that the LED behaviour must stay uniform during
	 * the whole boot process (bootloader+linux).
	 */
	led_dat->sata = 0;
	led_dat->cdev.brightness = LED_OFF;
	led_dat->cdev.flags |= LED_CORE_SUSPENDRESUME;
	led_dat->mode_addr = template->mode_addr;
	led_dat->mode_val = template->mode_val;
	led_dat->bright_addr = template->bright_addr;
	led_dat->bright_max = (1 << pdata->gpio_ext->num_data) - 1;
	led_dat->timer = pdata->timer;
	led_dat->num_timer = pdata->num_timer;

	ret = led_classdev_register(&pdev->dev, &led_dat->cdev);
	if (ret < 0)
		return ret;

	/*
	 * If available, expose the SATA activity blink capability through
	 * a "sata" sysfs attribute.
	 */
	if (led_dat->mode_val[NETXBIG_LED_SATA] != NETXBIG_LED_INVALID_MODE) {
		ret = device_create_file(led_dat->cdev.dev, &dev_attr_sata);
		if (ret)
			led_classdev_unregister(&led_dat->cdev);
	}

	return ret;
}

static int __devinit netxbig_led_probe(struct platform_device *pdev)
{
	struct netxbig_led_platform_data *pdata = pdev->dev.platform_data;
	struct netxbig_led_data *leds_data;
	int i;
	int ret;

	if (!pdata)
		return -EINVAL;

	leds_data = kzalloc(sizeof(struct netxbig_led_data) * pdata->num_leds,
			    GFP_KERNEL);
	if (!leds_data)
		return -ENOMEM;

	ret = gpio_ext_init(pdata->gpio_ext);
	if (ret < 0)
		goto err_free_data;

	for (i = 0; i < pdata->num_leds; i++) {
		ret = create_netxbig_led(pdev, &leds_data[i], &pdata->leds[i]);
		if (ret < 0)
			goto err_free_leds;
	}

	platform_set_drvdata(pdev, leds_data);

	return 0;

err_free_leds:
	for (i = i - 1; i >= 0; i--)
		delete_netxbig_led(&leds_data[i]);

	gpio_ext_free(pdata->gpio_ext);
err_free_data:
	kfree(leds_data);

	return ret;
}

static int __devexit netxbig_led_remove(struct platform_device *pdev)
{
	struct netxbig_led_platform_data *pdata = pdev->dev.platform_data;
	struct netxbig_led_data *leds_data;
	int i;

	leds_data = platform_get_drvdata(pdev);

	for (i = 0; i < pdata->num_leds; i++)
		delete_netxbig_led(&leds_data[i]);

	gpio_ext_free(pdata->gpio_ext);
	kfree(leds_data);

	return 0;
}

static struct platform_driver netxbig_led_driver = {
	.probe		= netxbig_led_probe,
	.remove		= __devexit_p(netxbig_led_remove),
	.driver		= {
		.name	= "leds-netxbig",
		.owner	= THIS_MODULE,
	},
};
MODULE_ALIAS("platform:leds-netxbig");

static int __init netxbig_led_init(void)
{
	return platform_driver_register(&netxbig_led_driver);
}

static void __exit netxbig_led_exit(void)
{
	platform_driver_unregister(&netxbig_led_driver);
}

module_init(netxbig_led_init);
module_exit(netxbig_led_exit);

MODULE_AUTHOR("Simon Guinot <sguinot@lacie.com>");
MODULE_DESCRIPTION("LED driver for LaCie xBig Network boards");
MODULE_LICENSE("GPL");
