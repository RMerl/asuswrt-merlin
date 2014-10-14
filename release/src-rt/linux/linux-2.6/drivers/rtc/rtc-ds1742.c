/*
 * An rtc driver for the Dallas DS1742
 *
 * Copyright (C) 2006 Atsushi Nemoto <anemo@mba.ocn.ne.jp>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Copyright (C) 2006 Torsten Ertbjerg Rasmussen <tr@newtec.dk>
 *  - nvram size determined from resource
 *  - this ds1742 driver now supports ds1743.
 */

#include <linux/bcd.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/gfp.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/rtc.h>
#include <linux/platform_device.h>
#include <linux/io.h>

#define DRV_VERSION "0.4"

#define RTC_SIZE		8

#define RTC_CONTROL		0
#define RTC_CENTURY		0
#define RTC_SECONDS		1
#define RTC_MINUTES		2
#define RTC_HOURS		3
#define RTC_DAY			4
#define RTC_DATE		5
#define RTC_MONTH		6
#define RTC_YEAR		7

#define RTC_CENTURY_MASK	0x3f
#define RTC_SECONDS_MASK	0x7f
#define RTC_DAY_MASK		0x07

/* Bits in the Control/Century register */
#define RTC_WRITE		0x80
#define RTC_READ		0x40

/* Bits in the Seconds register */
#define RTC_STOP		0x80

/* Bits in the Day register */
#define RTC_BATT_FLAG		0x80

struct rtc_plat_data {
	struct rtc_device *rtc;
	void __iomem *ioaddr_nvram;
	void __iomem *ioaddr_rtc;
	size_t size_nvram;
	size_t size;
	unsigned long last_jiffies;
	struct bin_attribute nvram_attr;
};

static int ds1742_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct rtc_plat_data *pdata = platform_get_drvdata(pdev);
	void __iomem *ioaddr = pdata->ioaddr_rtc;
	u8 century;

	century = bin2bcd((tm->tm_year + 1900) / 100);

	writeb(RTC_WRITE, ioaddr + RTC_CONTROL);

	writeb(bin2bcd(tm->tm_year % 100), ioaddr + RTC_YEAR);
	writeb(bin2bcd(tm->tm_mon + 1), ioaddr + RTC_MONTH);
	writeb(bin2bcd(tm->tm_wday) & RTC_DAY_MASK, ioaddr + RTC_DAY);
	writeb(bin2bcd(tm->tm_mday), ioaddr + RTC_DATE);
	writeb(bin2bcd(tm->tm_hour), ioaddr + RTC_HOURS);
	writeb(bin2bcd(tm->tm_min), ioaddr + RTC_MINUTES);
	writeb(bin2bcd(tm->tm_sec) & RTC_SECONDS_MASK, ioaddr + RTC_SECONDS);

	/* RTC_CENTURY and RTC_CONTROL share same register */
	writeb(RTC_WRITE | (century & RTC_CENTURY_MASK), ioaddr + RTC_CENTURY);
	writeb(century & RTC_CENTURY_MASK, ioaddr + RTC_CONTROL);
	return 0;
}

static int ds1742_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct rtc_plat_data *pdata = platform_get_drvdata(pdev);
	void __iomem *ioaddr = pdata->ioaddr_rtc;
	unsigned int year, month, day, hour, minute, second, week;
	unsigned int century;

	/* give enough time to update RTC in case of continuous read */
	if (pdata->last_jiffies == jiffies)
		msleep(1);
	pdata->last_jiffies = jiffies;
	writeb(RTC_READ, ioaddr + RTC_CONTROL);
	second = readb(ioaddr + RTC_SECONDS) & RTC_SECONDS_MASK;
	minute = readb(ioaddr + RTC_MINUTES);
	hour = readb(ioaddr + RTC_HOURS);
	day = readb(ioaddr + RTC_DATE);
	week = readb(ioaddr + RTC_DAY) & RTC_DAY_MASK;
	month = readb(ioaddr + RTC_MONTH);
	year = readb(ioaddr + RTC_YEAR);
	century = readb(ioaddr + RTC_CENTURY) & RTC_CENTURY_MASK;
	writeb(0, ioaddr + RTC_CONTROL);
	tm->tm_sec = bcd2bin(second);
	tm->tm_min = bcd2bin(minute);
	tm->tm_hour = bcd2bin(hour);
	tm->tm_mday = bcd2bin(day);
	tm->tm_wday = bcd2bin(week);
	tm->tm_mon = bcd2bin(month) - 1;
	/* year is 1900 + tm->tm_year */
	tm->tm_year = bcd2bin(year) + bcd2bin(century) * 100 - 1900;

	if (rtc_valid_tm(tm) < 0) {
		dev_err(dev, "retrieved date/time is not valid.\n");
		rtc_time_to_tm(0, tm);
	}
	return 0;
}

static const struct rtc_class_ops ds1742_rtc_ops = {
	.read_time	= ds1742_rtc_read_time,
	.set_time	= ds1742_rtc_set_time,
};

static ssize_t ds1742_nvram_read(struct file *filp, struct kobject *kobj,
				 struct bin_attribute *bin_attr,
				 char *buf, loff_t pos, size_t size)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct platform_device *pdev = to_platform_device(dev);
	struct rtc_plat_data *pdata = platform_get_drvdata(pdev);
	void __iomem *ioaddr = pdata->ioaddr_nvram;
	ssize_t count;

	for (count = 0; size > 0 && pos < pdata->size_nvram; count++, size--)
		*buf++ = readb(ioaddr + pos++);
	return count;
}

static ssize_t ds1742_nvram_write(struct file *filp, struct kobject *kobj,
				  struct bin_attribute *bin_attr,
				  char *buf, loff_t pos, size_t size)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct platform_device *pdev = to_platform_device(dev);
	struct rtc_plat_data *pdata = platform_get_drvdata(pdev);
	void __iomem *ioaddr = pdata->ioaddr_nvram;
	ssize_t count;

	for (count = 0; size > 0 && pos < pdata->size_nvram; count++, size--)
		writeb(*buf++, ioaddr + pos++);
	return count;
}

static int __devinit ds1742_rtc_probe(struct platform_device *pdev)
{
	struct rtc_device *rtc;
	struct resource *res;
	unsigned int cen, sec;
	struct rtc_plat_data *pdata;
	void __iomem *ioaddr;
	int ret = 0;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -ENODEV;
	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;
	pdata->size = res->end - res->start + 1;
	if (!devm_request_mem_region(&pdev->dev, res->start, pdata->size,
		pdev->name))
		return -EBUSY;
	ioaddr = devm_ioremap(&pdev->dev, res->start, pdata->size);
	if (!ioaddr)
		return -ENOMEM;

	pdata->ioaddr_nvram = ioaddr;
	pdata->size_nvram = pdata->size - RTC_SIZE;
	pdata->ioaddr_rtc = ioaddr + pdata->size_nvram;

	sysfs_bin_attr_init(&pdata->nvram_attr);
	pdata->nvram_attr.attr.name = "nvram";
	pdata->nvram_attr.attr.mode = S_IRUGO | S_IWUSR;
	pdata->nvram_attr.read = ds1742_nvram_read;
	pdata->nvram_attr.write = ds1742_nvram_write;
	pdata->nvram_attr.size = pdata->size_nvram;

	/* turn RTC on if it was not on */
	ioaddr = pdata->ioaddr_rtc;
	sec = readb(ioaddr + RTC_SECONDS);
	if (sec & RTC_STOP) {
		sec &= RTC_SECONDS_MASK;
		cen = readb(ioaddr + RTC_CENTURY) & RTC_CENTURY_MASK;
		writeb(RTC_WRITE, ioaddr + RTC_CONTROL);
		writeb(sec, ioaddr + RTC_SECONDS);
		writeb(cen & RTC_CENTURY_MASK, ioaddr + RTC_CONTROL);
	}
	if (!(readb(ioaddr + RTC_DAY) & RTC_BATT_FLAG))
		dev_warn(&pdev->dev, "voltage-low detected.\n");

	pdata->last_jiffies = jiffies;
	platform_set_drvdata(pdev, pdata);
	rtc = rtc_device_register(pdev->name, &pdev->dev,
				  &ds1742_rtc_ops, THIS_MODULE);
	if (IS_ERR(rtc))
		return PTR_ERR(rtc);
	pdata->rtc = rtc;

	ret = sysfs_create_bin_file(&pdev->dev.kobj, &pdata->nvram_attr);
	if (ret) {
		dev_err(&pdev->dev, "creating nvram file in sysfs failed\n");
		rtc_device_unregister(rtc);
	}
	return ret;
}

static int __devexit ds1742_rtc_remove(struct platform_device *pdev)
{
	struct rtc_plat_data *pdata = platform_get_drvdata(pdev);

	sysfs_remove_bin_file(&pdev->dev.kobj, &pdata->nvram_attr);
	rtc_device_unregister(pdata->rtc);
	return 0;
}

static struct platform_driver ds1742_rtc_driver = {
	.probe		= ds1742_rtc_probe,
	.remove		= __devexit_p(ds1742_rtc_remove),
	.driver		= {
		.name	= "rtc-ds1742",
		.owner	= THIS_MODULE,
	},
};

static __init int ds1742_init(void)
{
	return platform_driver_register(&ds1742_rtc_driver);
}

static __exit void ds1742_exit(void)
{
	platform_driver_unregister(&ds1742_rtc_driver);
}

module_init(ds1742_init);
module_exit(ds1742_exit);

MODULE_AUTHOR("Atsushi Nemoto <anemo@mba.ocn.ne.jp>");
MODULE_DESCRIPTION("Dallas DS1742 RTC driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_VERSION);
MODULE_ALIAS("platform:rtc-ds1742");
