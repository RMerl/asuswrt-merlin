/*
 *  Simple PWM driver for EP93XX
 *
 *	(c) Copyright 2009  Matthieu Crapet <mcrapet@gmail.com>
 *	(c) Copyright 2009  H Hartley Sweeten <hsweeten@visionengravers.com>
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 *
 *  EP9307 has only one channel:
 *    - PWMOUT
 *
 *  EP9301/02/12/15 have two channels:
 *    - PWMOUT
 *    - PWMOUT1 (alternate function for EGPIO14)
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>

#include <mach/platform.h>

#define EP93XX_PWMx_TERM_COUNT	0x00
#define EP93XX_PWMx_DUTY_CYCLE	0x04
#define EP93XX_PWMx_ENABLE	0x08
#define EP93XX_PWMx_INVERT	0x0C

#define EP93XX_PWM_MAX_COUNT	0xFFFF

struct ep93xx_pwm {
	void __iomem	*mmio_base;
	struct clk	*clk;
	u32		duty_percent;
};

static inline void ep93xx_pwm_writel(struct ep93xx_pwm *pwm,
		unsigned int val, unsigned int off)
{
	__raw_writel(val, pwm->mmio_base + off);
}

static inline unsigned int ep93xx_pwm_readl(struct ep93xx_pwm *pwm,
		unsigned int off)
{
	return __raw_readl(pwm->mmio_base + off);
}

static inline void ep93xx_pwm_write_tc(struct ep93xx_pwm *pwm, u16 value)
{
	ep93xx_pwm_writel(pwm, value, EP93XX_PWMx_TERM_COUNT);
}

static inline u16 ep93xx_pwm_read_tc(struct ep93xx_pwm *pwm)
{
	return ep93xx_pwm_readl(pwm, EP93XX_PWMx_TERM_COUNT);
}

static inline void ep93xx_pwm_write_dc(struct ep93xx_pwm *pwm, u16 value)
{
	ep93xx_pwm_writel(pwm, value, EP93XX_PWMx_DUTY_CYCLE);
}

static inline void ep93xx_pwm_enable(struct ep93xx_pwm *pwm)
{
	ep93xx_pwm_writel(pwm, 0x1, EP93XX_PWMx_ENABLE);
}

static inline void ep93xx_pwm_disable(struct ep93xx_pwm *pwm)
{
	ep93xx_pwm_writel(pwm, 0x0, EP93XX_PWMx_ENABLE);
}

static inline int ep93xx_pwm_is_enabled(struct ep93xx_pwm *pwm)
{
	return ep93xx_pwm_readl(pwm, EP93XX_PWMx_ENABLE) & 0x1;
}

static inline void ep93xx_pwm_invert(struct ep93xx_pwm *pwm)
{
	ep93xx_pwm_writel(pwm, 0x1, EP93XX_PWMx_INVERT);
}

static inline void ep93xx_pwm_normal(struct ep93xx_pwm *pwm)
{
	ep93xx_pwm_writel(pwm, 0x0, EP93XX_PWMx_INVERT);
}

static inline int ep93xx_pwm_is_inverted(struct ep93xx_pwm *pwm)
{
	return ep93xx_pwm_readl(pwm, EP93XX_PWMx_INVERT) & 0x1;
}

/*
 * /sys/devices/platform/ep93xx-pwm.N
 *   /min_freq      read-only   minimum pwm output frequency
 *   /max_req       read-only   maximum pwm output frequency
 *   /freq          read-write  pwm output frequency (0 = disable output)
 *   /duty_percent  read-write  pwm duty cycle percent (1..99)
 *   /invert        read-write  invert pwm output
 */

static ssize_t ep93xx_pwm_get_min_freq(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct ep93xx_pwm *pwm = platform_get_drvdata(pdev);
	unsigned long rate = clk_get_rate(pwm->clk);

	return sprintf(buf, "%ld\n", rate / (EP93XX_PWM_MAX_COUNT + 1));
}

static ssize_t ep93xx_pwm_get_max_freq(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct ep93xx_pwm *pwm = platform_get_drvdata(pdev);
	unsigned long rate = clk_get_rate(pwm->clk);

	return sprintf(buf, "%ld\n", rate / 2);
}

static ssize_t ep93xx_pwm_get_freq(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct ep93xx_pwm *pwm = platform_get_drvdata(pdev);

	if (ep93xx_pwm_is_enabled(pwm)) {
		unsigned long rate = clk_get_rate(pwm->clk);
		u16 term = ep93xx_pwm_read_tc(pwm);

		return sprintf(buf, "%ld\n", rate / (term + 1));
	} else {
		return sprintf(buf, "disabled\n");
	}
}

static ssize_t ep93xx_pwm_set_freq(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct ep93xx_pwm *pwm = platform_get_drvdata(pdev);
	long val;
	int err;

	err = strict_strtol(buf, 10, &val);
	if (err)
		return -EINVAL;

	if (val == 0) {
		ep93xx_pwm_disable(pwm);
	} else if (val <= (clk_get_rate(pwm->clk) / 2)) {
		u32 term, duty;

		val = (clk_get_rate(pwm->clk) / val) - 1;
		if (val > EP93XX_PWM_MAX_COUNT)
			val = EP93XX_PWM_MAX_COUNT;
		if (val < 1)
			val = 1;

		term = ep93xx_pwm_read_tc(pwm);
		duty = ((val + 1) * pwm->duty_percent / 100) - 1;

		/* If pwm is running, order is important */
		if (val > term) {
			ep93xx_pwm_write_tc(pwm, val);
			ep93xx_pwm_write_dc(pwm, duty);
		} else {
			ep93xx_pwm_write_dc(pwm, duty);
			ep93xx_pwm_write_tc(pwm, val);
		}

		if (!ep93xx_pwm_is_enabled(pwm))
			ep93xx_pwm_enable(pwm);
	} else {
		return -EINVAL;
	}

	return count;
}

static ssize_t ep93xx_pwm_get_duty_percent(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct ep93xx_pwm *pwm = platform_get_drvdata(pdev);

	return sprintf(buf, "%d\n", pwm->duty_percent);
}

static ssize_t ep93xx_pwm_set_duty_percent(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct ep93xx_pwm *pwm = platform_get_drvdata(pdev);
	long val;
	int err;

	err = strict_strtol(buf, 10, &val);
	if (err)
		return -EINVAL;

	if (val > 0 && val < 100) {
		u32 term = ep93xx_pwm_read_tc(pwm);
		ep93xx_pwm_write_dc(pwm, ((term + 1) * val / 100) - 1);
		pwm->duty_percent = val;
		return count;
	}

	return -EINVAL;
}

static ssize_t ep93xx_pwm_get_invert(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct ep93xx_pwm *pwm = platform_get_drvdata(pdev);

	return sprintf(buf, "%d\n", ep93xx_pwm_is_inverted(pwm));
}

static ssize_t ep93xx_pwm_set_invert(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct ep93xx_pwm *pwm = platform_get_drvdata(pdev);
	long val;
	int err;

	err = strict_strtol(buf, 10, &val);
	if (err)
		return -EINVAL;

	if (val == 0)
		ep93xx_pwm_normal(pwm);
	else if (val == 1)
		ep93xx_pwm_invert(pwm);
	else
		return -EINVAL;

	return count;
}

static DEVICE_ATTR(min_freq, S_IRUGO, ep93xx_pwm_get_min_freq, NULL);
static DEVICE_ATTR(max_freq, S_IRUGO, ep93xx_pwm_get_max_freq, NULL);
static DEVICE_ATTR(freq, S_IWUGO | S_IRUGO,
		   ep93xx_pwm_get_freq, ep93xx_pwm_set_freq);
static DEVICE_ATTR(duty_percent, S_IWUGO | S_IRUGO,
		   ep93xx_pwm_get_duty_percent, ep93xx_pwm_set_duty_percent);
static DEVICE_ATTR(invert, S_IWUGO | S_IRUGO,
		   ep93xx_pwm_get_invert, ep93xx_pwm_set_invert);

static struct attribute *ep93xx_pwm_attrs[] = {
	&dev_attr_min_freq.attr,
	&dev_attr_max_freq.attr,
	&dev_attr_freq.attr,
	&dev_attr_duty_percent.attr,
	&dev_attr_invert.attr,
	NULL
};

static const struct attribute_group ep93xx_pwm_sysfs_files = {
	.attrs	= ep93xx_pwm_attrs,
};

static int __init ep93xx_pwm_probe(struct platform_device *pdev)
{
	struct ep93xx_pwm *pwm;
	struct resource *res;
	int err;

	err = ep93xx_pwm_acquire_gpio(pdev);
	if (err)
		return err;

	pwm = kzalloc(sizeof(struct ep93xx_pwm), GFP_KERNEL);
	if (!pwm) {
		err = -ENOMEM;
		goto fail_no_mem;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		err = -ENXIO;
		goto fail_no_mem_resource;
	}

	res = request_mem_region(res->start, resource_size(res), pdev->name);
	if (res == NULL) {
		err = -EBUSY;
		goto fail_no_mem_resource;
	}

	pwm->mmio_base = ioremap(res->start, resource_size(res));
	if (pwm->mmio_base == NULL) {
		err = -ENXIO;
		goto fail_no_ioremap;
	}

	err = sysfs_create_group(&pdev->dev.kobj, &ep93xx_pwm_sysfs_files);
	if (err)
		goto fail_no_sysfs;

	pwm->clk = clk_get(&pdev->dev, "pwm_clk");
	if (IS_ERR(pwm->clk)) {
		err = PTR_ERR(pwm->clk);
		goto fail_no_clk;
	}

	pwm->duty_percent = 50;

	platform_set_drvdata(pdev, pwm);

	/* disable pwm at startup. Avoids zero value. */
	ep93xx_pwm_disable(pwm);
	ep93xx_pwm_write_tc(pwm, EP93XX_PWM_MAX_COUNT);
	ep93xx_pwm_write_dc(pwm, EP93XX_PWM_MAX_COUNT / 2);

	clk_enable(pwm->clk);

	return 0;

fail_no_clk:
	sysfs_remove_group(&pdev->dev.kobj, &ep93xx_pwm_sysfs_files);
fail_no_sysfs:
	iounmap(pwm->mmio_base);
fail_no_ioremap:
	release_mem_region(res->start, resource_size(res));
fail_no_mem_resource:
	kfree(pwm);
fail_no_mem:
	ep93xx_pwm_release_gpio(pdev);
	return err;
}

static int __exit ep93xx_pwm_remove(struct platform_device *pdev)
{
	struct ep93xx_pwm *pwm = platform_get_drvdata(pdev);
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	ep93xx_pwm_disable(pwm);
	clk_disable(pwm->clk);
	clk_put(pwm->clk);
	platform_set_drvdata(pdev, NULL);
	sysfs_remove_group(&pdev->dev.kobj, &ep93xx_pwm_sysfs_files);
	iounmap(pwm->mmio_base);
	release_mem_region(res->start, resource_size(res));
	kfree(pwm);
	ep93xx_pwm_release_gpio(pdev);

	return 0;
}

static struct platform_driver ep93xx_pwm_driver = {
	.driver		= {
		.name	= "ep93xx-pwm",
		.owner	= THIS_MODULE,
	},
	.remove		= __exit_p(ep93xx_pwm_remove),
};

static int __init ep93xx_pwm_init(void)
{
	return platform_driver_probe(&ep93xx_pwm_driver, ep93xx_pwm_probe);
}

static void __exit ep93xx_pwm_exit(void)
{
	platform_driver_unregister(&ep93xx_pwm_driver);
}

module_init(ep93xx_pwm_init);
module_exit(ep93xx_pwm_exit);

MODULE_AUTHOR("Matthieu Crapet <mcrapet@gmail.com>, "
	      "H Hartley Sweeten <hsweeten@visionengravers.com>");
MODULE_DESCRIPTION("EP93xx PWM driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:ep93xx-pwm");
