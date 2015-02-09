/*
 * Generic implementation of a polled input device

 * Copyright (c) 2007 Dmitry Torokhov
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */

#include <linux/jiffies.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/input-polldev.h>

MODULE_AUTHOR("Dmitry Torokhov <dtor@mail.ru>");
MODULE_DESCRIPTION("Generic implementation of a polled input device");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("0.1");

static DEFINE_MUTEX(polldev_mutex);
static int polldev_users;
static struct workqueue_struct *polldev_wq;

static int input_polldev_start_workqueue(void)
{
	int retval;

	retval = mutex_lock_interruptible(&polldev_mutex);
	if (retval)
		return retval;

	if (!polldev_users) {
		polldev_wq = create_singlethread_workqueue("ipolldevd");
		if (!polldev_wq) {
			printk(KERN_ERR "input-polldev: failed to create "
				"ipolldevd workqueue\n");
			retval = -ENOMEM;
			goto out;
		}
	}

	polldev_users++;

 out:
	mutex_unlock(&polldev_mutex);
	return retval;
}

static void input_polldev_stop_workqueue(void)
{
	mutex_lock(&polldev_mutex);

	if (!--polldev_users)
		destroy_workqueue(polldev_wq);

	mutex_unlock(&polldev_mutex);
}

static void input_polldev_queue_work(struct input_polled_dev *dev)
{
	unsigned long delay;

	delay = msecs_to_jiffies(dev->poll_interval);
	if (delay >= HZ)
		delay = round_jiffies_relative(delay);

	queue_delayed_work(polldev_wq, &dev->work, delay);
}

static void input_polled_device_work(struct work_struct *work)
{
	struct input_polled_dev *dev =
		container_of(work, struct input_polled_dev, work.work);

	dev->poll(dev);
	input_polldev_queue_work(dev);
}

static int input_open_polled_device(struct input_dev *input)
{
	struct input_polled_dev *dev = input_get_drvdata(input);
	int error;

	error = input_polldev_start_workqueue();
	if (error)
		return error;

	if (dev->open)
		dev->open(dev);

	/* Only start polling if polling is enabled */
	if (dev->poll_interval > 0)
		queue_delayed_work(polldev_wq, &dev->work, 0);

	return 0;
}

static void input_close_polled_device(struct input_dev *input)
{
	struct input_polled_dev *dev = input_get_drvdata(input);

	cancel_delayed_work_sync(&dev->work);
	/*
	 * Clean up work struct to remove references to the workqueue.
	 * It may be destroyed by the next call. This causes problems
	 * at next device open-close in case of poll_interval == 0.
	 */
	INIT_DELAYED_WORK(&dev->work, dev->work.work.func);
	input_polldev_stop_workqueue();

	if (dev->close)
		dev->close(dev);
}

/* SYSFS interface */

static ssize_t input_polldev_get_poll(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct input_polled_dev *polldev = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", polldev->poll_interval);
}

static ssize_t input_polldev_set_poll(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct input_polled_dev *polldev = dev_get_drvdata(dev);
	struct input_dev *input = polldev->input;
	unsigned long interval;

	if (strict_strtoul(buf, 0, &interval))
		return -EINVAL;

	if (interval < polldev->poll_interval_min)
		return -EINVAL;

	if (interval > polldev->poll_interval_max)
		return -EINVAL;

	mutex_lock(&input->mutex);

	polldev->poll_interval = interval;

	if (input->users) {
		cancel_delayed_work_sync(&polldev->work);
		if (polldev->poll_interval > 0)
			input_polldev_queue_work(polldev);
	}

	mutex_unlock(&input->mutex);

	return count;
}

static DEVICE_ATTR(poll, S_IRUGO | S_IWUSR, input_polldev_get_poll,
					    input_polldev_set_poll);


static ssize_t input_polldev_get_max(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct input_polled_dev *polldev = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", polldev->poll_interval_max);
}

static DEVICE_ATTR(max, S_IRUGO, input_polldev_get_max, NULL);

static ssize_t input_polldev_get_min(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct input_polled_dev *polldev = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", polldev->poll_interval_min);
}

static DEVICE_ATTR(min, S_IRUGO, input_polldev_get_min, NULL);

static struct attribute *sysfs_attrs[] = {
	&dev_attr_poll.attr,
	&dev_attr_max.attr,
	&dev_attr_min.attr,
	NULL
};

static struct attribute_group input_polldev_attribute_group = {
	.attrs = sysfs_attrs
};

/**
 * input_allocate_polled_device - allocated memory polled device
 *
 * The function allocates memory for a polled device and also
 * for an input device associated with this polled device.
 */
struct input_polled_dev *input_allocate_polled_device(void)
{
	struct input_polled_dev *dev;

	dev = kzalloc(sizeof(struct input_polled_dev), GFP_KERNEL);
	if (!dev)
		return NULL;

	dev->input = input_allocate_device();
	if (!dev->input) {
		kfree(dev);
		return NULL;
	}

	return dev;
}
EXPORT_SYMBOL(input_allocate_polled_device);

/**
 * input_free_polled_device - free memory allocated for polled device
 * @dev: device to free
 *
 * The function frees memory allocated for polling device and drops
 * reference to the associated input device.
 */
void input_free_polled_device(struct input_polled_dev *dev)
{
	if (dev) {
		input_free_device(dev->input);
		kfree(dev);
	}
}
EXPORT_SYMBOL(input_free_polled_device);

/**
 * input_register_polled_device - register polled device
 * @dev: device to register
 *
 * The function registers previously initialized polled input device
 * with input layer. The device should be allocated with call to
 * input_allocate_polled_device(). Callers should also set up poll()
 * method and set up capabilities (id, name, phys, bits) of the
 * corresponing input_dev structure.
 */
int input_register_polled_device(struct input_polled_dev *dev)
{
	struct input_dev *input = dev->input;
	int error;

	input_set_drvdata(input, dev);
	INIT_DELAYED_WORK(&dev->work, input_polled_device_work);
	if (!dev->poll_interval)
		dev->poll_interval = 500;
	if (!dev->poll_interval_max)
		dev->poll_interval_max = dev->poll_interval;
	input->open = input_open_polled_device;
	input->close = input_close_polled_device;

	error = input_register_device(input);
	if (error)
		return error;

	error = sysfs_create_group(&input->dev.kobj,
				   &input_polldev_attribute_group);
	if (error) {
		input_unregister_device(input);
		return error;
	}

	/*
	 * Take extra reference to the underlying input device so
	 * that it survives call to input_unregister_polled_device()
	 * and is deleted only after input_free_polled_device()
	 * has been invoked. This is needed to ease task of freeing
	 * sparse keymaps.
	 */
	input_get_device(input);

	return 0;
}
EXPORT_SYMBOL(input_register_polled_device);

/**
 * input_unregister_polled_device - unregister polled device
 * @dev: device to unregister
 *
 * The function unregisters previously registered polled input
 * device from input layer. Polling is stopped and device is
 * ready to be freed with call to input_free_polled_device().
 */
void input_unregister_polled_device(struct input_polled_dev *dev)
{
	sysfs_remove_group(&dev->input->dev.kobj,
			   &input_polldev_attribute_group);

	input_unregister_device(dev->input);
}
EXPORT_SYMBOL(input_unregister_polled_device);
