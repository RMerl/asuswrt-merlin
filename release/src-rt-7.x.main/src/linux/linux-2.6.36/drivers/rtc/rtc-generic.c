/* rtc-generic: RTC driver using the generic RTC abstraction
 *
 * Copyright (C) 2008 Kyle McMartin <kyle@mcmartin.ca>
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/time.h>
#include <linux/platform_device.h>
#include <linux/rtc.h>

#include <asm/rtc.h>

static int generic_get_time(struct device *dev, struct rtc_time *tm)
{
	unsigned int ret = get_rtc_time(tm);

	if (ret & RTC_BATT_BAD)
		return -EOPNOTSUPP;

	return rtc_valid_tm(tm);
}

static int generic_set_time(struct device *dev, struct rtc_time *tm)
{
	if (set_rtc_time(tm) < 0)
		return -EOPNOTSUPP;

	return 0;
}

static const struct rtc_class_ops generic_rtc_ops = {
	.read_time = generic_get_time,
	.set_time = generic_set_time,
};

static int __init generic_rtc_probe(struct platform_device *dev)
{
	struct rtc_device *rtc;

	rtc = rtc_device_register("rtc-generic", &dev->dev, &generic_rtc_ops,
				  THIS_MODULE);
	if (IS_ERR(rtc))
		return PTR_ERR(rtc);

	platform_set_drvdata(dev, rtc);

	return 0;
}

static int __exit generic_rtc_remove(struct platform_device *dev)
{
	struct rtc_device *rtc = platform_get_drvdata(dev);

	rtc_device_unregister(rtc);

	return 0;
}

static struct platform_driver generic_rtc_driver = {
	.driver = {
		.name = "rtc-generic",
		.owner = THIS_MODULE,
	},
	.remove = __exit_p(generic_rtc_remove),
};

static int __init generic_rtc_init(void)
{
	return platform_driver_probe(&generic_rtc_driver, generic_rtc_probe);
}

static void __exit generic_rtc_fini(void)
{
	platform_driver_unregister(&generic_rtc_driver);
}

module_init(generic_rtc_init);
module_exit(generic_rtc_fini);

MODULE_AUTHOR("Kyle McMartin <kyle@mcmartin.ca>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Generic RTC driver");
MODULE_ALIAS("platform:rtc-generic");
