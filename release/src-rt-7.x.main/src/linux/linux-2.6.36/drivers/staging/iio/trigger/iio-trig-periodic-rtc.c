/* The industrial I/O periodic RTC trigger driver
 *
 * Copyright (c) 2008 Jonathan Cameron
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This is a heavily rewritten version of the periodic timer system in
 * earlier version of industrialio.  It supplies the same functionality
 * but via a trigger rather than a specific periodic timer system.
 */

#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/rtc.h>
#include "../iio.h"
#include "../trigger.h"

static LIST_HEAD(iio_prtc_trigger_list);
static DEFINE_MUTEX(iio_prtc_trigger_list_lock);

struct iio_prtc_trigger_info {
	struct rtc_device *rtc;
	int frequency;
	struct rtc_task task;
};

static int iio_trig_periodic_rtc_set_state(struct iio_trigger *trig, bool state)
{
	struct iio_prtc_trigger_info *trig_info = trig->private_data;
	if (trig_info->frequency == 0)
		return -EINVAL;
	printk(KERN_INFO "trigger frequency is %d\n", trig_info->frequency);
	return rtc_irq_set_state(trig_info->rtc, &trig_info->task, state);
}

static ssize_t iio_trig_periodic_read_freq(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct iio_trigger *trig = dev_get_drvdata(dev);
	struct iio_prtc_trigger_info *trig_info = trig->private_data;
	return sprintf(buf, "%u\n", trig_info->frequency);
}

static ssize_t iio_trig_periodic_write_freq(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf,
					    size_t len)
{
	struct iio_trigger *trig = dev_get_drvdata(dev);
	struct iio_prtc_trigger_info *trig_info = trig->private_data;
	unsigned long val;
	int ret;

	ret = strict_strtoul(buf, 10, &val);
	if (ret)
		goto error_ret;

	ret = rtc_irq_set_freq(trig_info->rtc, &trig_info->task, val);
	if (ret)
		goto error_ret;

	trig_info->frequency = val;

	return len;

error_ret:
	return ret;
}

static ssize_t iio_trig_periodic_read_name(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct iio_trigger *trig = dev_get_drvdata(dev);
	return sprintf(buf, "%s\n", trig->name);
}

static DEVICE_ATTR(name, S_IRUGO,
	    iio_trig_periodic_read_name,
	    NULL);
static DEVICE_ATTR(frequency, S_IRUGO | S_IWUSR,
	    iio_trig_periodic_read_freq,
	    iio_trig_periodic_write_freq);

static struct attribute *iio_trig_prtc_attrs[] = {
	&dev_attr_frequency.attr,
	&dev_attr_name.attr,
	NULL,
};
static const struct attribute_group iio_trig_prtc_attr_group = {
	.attrs = iio_trig_prtc_attrs,
};

static void iio_prtc_trigger_poll(void *private_data)
{
	/* Timestamp is not provided currently */
	iio_trigger_poll(private_data, 0);
}

static int iio_trig_periodic_rtc_probe(struct platform_device *dev)
{
	char **pdata = dev->dev.platform_data;
	struct iio_prtc_trigger_info *trig_info;
	struct iio_trigger *trig, *trig2;

	int i, ret;

	for (i = 0;; i++) {
		if (pdata[i] == NULL)
			break;
		trig = iio_allocate_trigger();
		if (!trig) {
			ret = -ENOMEM;
			goto error_free_completed_registrations;
		}
		list_add(&trig->alloc_list, &iio_prtc_trigger_list);

		trig_info = kzalloc(sizeof(*trig_info), GFP_KERNEL);
		if (!trig_info) {
			ret = -ENOMEM;
			goto error_put_trigger_and_remove_from_list;
		}
		trig->private_data = trig_info;
		trig->owner = THIS_MODULE;
		trig->set_trigger_state = &iio_trig_periodic_rtc_set_state;
		trig->name = kasprintf(GFP_KERNEL, "periodic%s", pdata[i]);
		if (trig->name == NULL) {
			ret = -ENOMEM;
			goto error_free_trig_info;
		}

		/* RTC access */
		trig_info->rtc
			= rtc_class_open(pdata[i]);
		if (trig_info->rtc == NULL) {
			ret = -EINVAL;
			goto error_free_name;
		}
		trig_info->task.func = iio_prtc_trigger_poll;
		trig_info->task.private_data = trig;
		ret = rtc_irq_register(trig_info->rtc, &trig_info->task);
		if (ret)
			goto error_close_rtc;
		trig->control_attrs = &iio_trig_prtc_attr_group;
		ret = iio_trigger_register(trig);
		if (ret)
			goto error_unregister_rtc_irq;
	}
	return 0;
error_unregister_rtc_irq:
	rtc_irq_unregister(trig_info->rtc, &trig_info->task);
error_close_rtc:
	rtc_class_close(trig_info->rtc);
error_free_name:
	kfree(trig->name);
error_free_trig_info:
	kfree(trig_info);
error_put_trigger_and_remove_from_list:
	list_del(&trig->alloc_list);
	iio_put_trigger(trig);
error_free_completed_registrations:
	list_for_each_entry_safe(trig,
				 trig2,
				 &iio_prtc_trigger_list,
				 alloc_list) {
		trig_info = trig->private_data;
		rtc_irq_unregister(trig_info->rtc, &trig_info->task);
		rtc_class_close(trig_info->rtc);
		kfree(trig->name);
		kfree(trig_info);
		iio_trigger_unregister(trig);
	}
	return ret;
}

static int iio_trig_periodic_rtc_remove(struct platform_device *dev)
{
	struct iio_trigger *trig, *trig2;
	struct iio_prtc_trigger_info *trig_info;
	mutex_lock(&iio_prtc_trigger_list_lock);
	list_for_each_entry_safe(trig,
				 trig2,
				 &iio_prtc_trigger_list,
				 alloc_list) {
		trig_info = trig->private_data;
		rtc_irq_unregister(trig_info->rtc, &trig_info->task);
		rtc_class_close(trig_info->rtc);
		kfree(trig->name);
		kfree(trig_info);
		iio_trigger_unregister(trig);
	}
	mutex_unlock(&iio_prtc_trigger_list_lock);
	return 0;
}

static struct platform_driver iio_trig_periodic_rtc_driver = {
	.probe = iio_trig_periodic_rtc_probe,
	.remove = iio_trig_periodic_rtc_remove,
	.driver = {
		.name = "iio_prtc_trigger",
		.owner = THIS_MODULE,
	},
};

static int __init iio_trig_periodic_rtc_init(void)
{
	return platform_driver_register(&iio_trig_periodic_rtc_driver);
}

static void __exit iio_trig_periodic_rtc_exit(void)
{
	return platform_driver_unregister(&iio_trig_periodic_rtc_driver);
}

module_init(iio_trig_periodic_rtc_init);
module_exit(iio_trig_periodic_rtc_exit);
MODULE_AUTHOR("Jonathan Cameron <jic23@cam.ac.uk>");
MODULE_DESCRIPTION("Periodic realtime clock  trigger for the iio subsystem");
MODULE_LICENSE("GPL v2");
