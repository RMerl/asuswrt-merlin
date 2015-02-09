/*
 * Roccat driver for Linux
 *
 * Copyright (c) 2010 Stefan Achatz <erazor_de@users.sourceforge.net>
 */

/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

/*
 * Module roccat is a char device used to report special events of roccat
 * hardware to userland. These events include requests for on-screen-display of
 * profile or dpi settings or requests for execution of macro sequences that are
 * not stored in device. The information in these events depends on hid device
 * implementation and contains data that is not available in a single hid event
 * or else hidraw could have been used.
 * It is inspired by hidraw, but uses only one circular buffer for all readers.
 */

#include <linux/cdev.h>
#include <linux/poll.h>
#include <linux/sched.h>

#include "hid-roccat.h"

#define ROCCAT_FIRST_MINOR 0
#define ROCCAT_MAX_DEVICES 8

/* should be a power of 2 for performance reason */
#define ROCCAT_CBUF_SIZE 16

struct roccat_report {
	uint8_t *value;
	int len;
};

struct roccat_device {
	unsigned int minor;
	int open;
	int exist;
	wait_queue_head_t wait;
	struct device *dev;
	struct hid_device *hid;
	struct list_head readers;
	/* protects modifications of readers list */
	struct mutex readers_lock;

	/*
	 * circular_buffer has one writer and multiple readers with their own
	 * read pointers
	 */
	struct roccat_report cbuf[ROCCAT_CBUF_SIZE];
	int cbuf_end;
	struct mutex cbuf_lock;
};

struct roccat_reader {
	struct list_head node;
	struct roccat_device *device;
	int cbuf_start;
};

static int roccat_major;
static struct class *roccat_class;
static struct cdev roccat_cdev;

static struct roccat_device *devices[ROCCAT_MAX_DEVICES];
/* protects modifications of devices array */
static DEFINE_MUTEX(devices_lock);

static ssize_t roccat_read(struct file *file, char __user *buffer,
		size_t count, loff_t *ppos)
{
	struct roccat_reader *reader = file->private_data;
	struct roccat_device *device = reader->device;
	struct roccat_report *report;
	ssize_t retval = 0, len;
	DECLARE_WAITQUEUE(wait, current);

	mutex_lock(&device->cbuf_lock);

	/* no data? */
	if (reader->cbuf_start == device->cbuf_end) {
		add_wait_queue(&device->wait, &wait);
		set_current_state(TASK_INTERRUPTIBLE);

		/* wait for data */
		while (reader->cbuf_start == device->cbuf_end) {
			if (file->f_flags & O_NONBLOCK) {
				retval = -EAGAIN;
				break;
			}
			if (signal_pending(current)) {
				retval = -ERESTARTSYS;
				break;
			}
			if (!device->exist) {
				retval = -EIO;
				break;
			}

			mutex_unlock(&device->cbuf_lock);
			schedule();
			mutex_lock(&device->cbuf_lock);
			set_current_state(TASK_INTERRUPTIBLE);
		}

		set_current_state(TASK_RUNNING);
		remove_wait_queue(&device->wait, &wait);
	}

	/* here we either have data or a reason to return if retval is set */
	if (retval)
		goto exit_unlock;

	report = &device->cbuf[reader->cbuf_start];
	/*
	 * If report is larger than requested amount of data, rest of report
	 * is lost!
	 */
	len = report->len > count ? count : report->len;

	if (copy_to_user(buffer, report->value, len)) {
		retval = -EFAULT;
		goto exit_unlock;
	}
	retval += len;
	reader->cbuf_start = (reader->cbuf_start + 1) % ROCCAT_CBUF_SIZE;

exit_unlock:
	mutex_unlock(&device->cbuf_lock);
	return retval;
}

static unsigned int roccat_poll(struct file *file, poll_table *wait)
{
	struct roccat_reader *reader = file->private_data;
	poll_wait(file, &reader->device->wait, wait);
	if (reader->cbuf_start != reader->device->cbuf_end)
		return POLLIN | POLLRDNORM;
	if (!reader->device->exist)
		return POLLERR | POLLHUP;
	return 0;
}

static int roccat_open(struct inode *inode, struct file *file)
{
	unsigned int minor = iminor(inode);
	struct roccat_reader *reader;
	struct roccat_device *device;
	int error = 0;

	reader = kzalloc(sizeof(struct roccat_reader), GFP_KERNEL);
	if (!reader)
		return -ENOMEM;

	mutex_lock(&devices_lock);

	device = devices[minor];

	mutex_lock(&device->readers_lock);

	if (!device) {
		printk(KERN_EMERG "roccat device with minor %d doesn't exist\n",
				minor);
		error = -ENODEV;
		goto exit_err;
	}

	if (!device->open++) {
		/* power on device on adding first reader */
		if (device->hid->ll_driver->power) {
			error = device->hid->ll_driver->power(device->hid,
					PM_HINT_FULLON);
			if (error < 0) {
				--device->open;
				goto exit_err;
			}
		}
		error = device->hid->ll_driver->open(device->hid);
		if (error < 0) {
			if (device->hid->ll_driver->power)
				device->hid->ll_driver->power(device->hid,
						PM_HINT_NORMAL);
			--device->open;
			goto exit_err;
		}
	}

	reader->device = device;
	/* new reader doesn't get old events */
	reader->cbuf_start = device->cbuf_end;

	list_add_tail(&reader->node, &device->readers);
	file->private_data = reader;

exit_unlock:
	mutex_unlock(&device->readers_lock);
	mutex_unlock(&devices_lock);
	return error;
exit_err:
	kfree(reader);
	goto exit_unlock;
}

static int roccat_release(struct inode *inode, struct file *file)
{
	unsigned int minor = iminor(inode);
	struct roccat_reader *reader = file->private_data;
	struct roccat_device *device;

	mutex_lock(&devices_lock);

	device = devices[minor];
	if (!device) {
		mutex_unlock(&devices_lock);
		printk(KERN_EMERG "roccat device with minor %d doesn't exist\n",
				minor);
		return -ENODEV;
	}

	mutex_lock(&device->readers_lock);
	list_del(&reader->node);
	mutex_unlock(&device->readers_lock);
	kfree(reader);

	if (!--device->open) {
		/* removing last reader */
		if (device->exist) {
			if (device->hid->ll_driver->power)
				device->hid->ll_driver->power(device->hid,
						PM_HINT_NORMAL);
			device->hid->ll_driver->close(device->hid);
		} else {
			kfree(device);
		}
	}

	mutex_unlock(&devices_lock);

	return 0;
}

/*
 * roccat_report_event() - output data to readers
 * @minor: minor device number returned by roccat_connect()
 * @data: pointer to data
 * @len: size of data
 *
 * Return value is zero on success, a negative error code on failure.
 *
 * This is called from interrupt handler.
 */
int roccat_report_event(int minor, u8 const *data, int len)
{
	struct roccat_device *device;
	struct roccat_reader *reader;
	struct roccat_report *report;
	uint8_t *new_value;

	new_value = kmemdup(data, len, GFP_ATOMIC);
	if (!new_value)
		return -ENOMEM;

	device = devices[minor];

	report = &device->cbuf[device->cbuf_end];

	/* passing NULL is safe */
	kfree(report->value);

	report->value = new_value;
	report->len = len;
	device->cbuf_end = (device->cbuf_end + 1) % ROCCAT_CBUF_SIZE;

	list_for_each_entry(reader, &device->readers, node) {
		/*
		 * As we already inserted one element, the buffer can't be
		 * empty. If start and end are equal, buffer is full and we
		 * increase start, so that slow reader misses one event, but
		 * gets the newer ones in the right order.
		 */
		if (reader->cbuf_start == device->cbuf_end)
			reader->cbuf_start = (reader->cbuf_start + 1) % ROCCAT_CBUF_SIZE;
	}

	wake_up_interruptible(&device->wait);
	return 0;
}
EXPORT_SYMBOL_GPL(roccat_report_event);

/*
 * roccat_connect() - create a char device for special event output
 * @hid: the hid device the char device should be connected to.
 *
 * Return value is minor device number in Range [0, ROCCAT_MAX_DEVICES] on
 * success, a negative error code on failure.
 */
int roccat_connect(struct hid_device *hid)
{
	unsigned int minor;
	struct roccat_device *device;
	int temp;

	device = kzalloc(sizeof(struct roccat_device), GFP_KERNEL);
	if (!device)
		return -ENOMEM;

	mutex_lock(&devices_lock);

	for (minor = 0; minor < ROCCAT_MAX_DEVICES; ++minor) {
		if (devices[minor])
			continue;
		break;
	}

	if (minor < ROCCAT_MAX_DEVICES) {
		devices[minor] = device;
	} else {
		mutex_unlock(&devices_lock);
		kfree(device);
		return -EINVAL;
	}

	device->dev = device_create(roccat_class, &hid->dev,
			MKDEV(roccat_major, minor), NULL,
			"%s%s%d", "roccat", hid->driver->name, minor);

	if (IS_ERR(device->dev)) {
		devices[minor] = NULL;
		mutex_unlock(&devices_lock);
		temp = PTR_ERR(device->dev);
		kfree(device);
		return temp;
	}

	mutex_unlock(&devices_lock);

	init_waitqueue_head(&device->wait);
	INIT_LIST_HEAD(&device->readers);
	mutex_init(&device->readers_lock);
	mutex_init(&device->cbuf_lock);
	device->minor = minor;
	device->hid = hid;
	device->exist = 1;
	device->cbuf_end = 0;

	return minor;
}
EXPORT_SYMBOL_GPL(roccat_connect);

/* roccat_disconnect() - remove char device from hid device
 * @minor: the minor device number returned by roccat_connect()
 */
void roccat_disconnect(int minor)
{
	struct roccat_device *device;

	mutex_lock(&devices_lock);
	device = devices[minor];
	devices[minor] = NULL;
	mutex_unlock(&devices_lock);

	device->exist = 0; /* TODO exist maybe not needed */

	device_destroy(roccat_class, MKDEV(roccat_major, minor));

	if (device->open) {
		device->hid->ll_driver->close(device->hid);
		wake_up_interruptible(&device->wait);
	} else {
		kfree(device);
	}
}
EXPORT_SYMBOL_GPL(roccat_disconnect);

static const struct file_operations roccat_ops = {
	.owner = THIS_MODULE,
	.read = roccat_read,
	.poll = roccat_poll,
	.open = roccat_open,
	.release = roccat_release,
};

static int __init roccat_init(void)
{
	int retval;
	dev_t dev_id;

	retval = alloc_chrdev_region(&dev_id, ROCCAT_FIRST_MINOR,
			ROCCAT_MAX_DEVICES, "roccat");

	roccat_major = MAJOR(dev_id);

	if (retval < 0) {
		printk(KERN_WARNING "roccat: can't get major number\n");
		return retval;
	}

	roccat_class = class_create(THIS_MODULE, "roccat");
	if (IS_ERR(roccat_class)) {
		retval = PTR_ERR(roccat_class);
		unregister_chrdev_region(dev_id, ROCCAT_MAX_DEVICES);
		return retval;
	}

	cdev_init(&roccat_cdev, &roccat_ops);
	cdev_add(&roccat_cdev, dev_id, ROCCAT_MAX_DEVICES);

	return 0;
}

static void __exit roccat_exit(void)
{
	dev_t dev_id = MKDEV(roccat_major, 0);

	cdev_del(&roccat_cdev);
	class_destroy(roccat_class);
	unregister_chrdev_region(dev_id, ROCCAT_MAX_DEVICES);
}

module_init(roccat_init);
module_exit(roccat_exit);

MODULE_AUTHOR("Stefan Achatz");
MODULE_DESCRIPTION("USB Roccat char device");
MODULE_LICENSE("GPL v2");
