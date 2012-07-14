/*
 * Linux Broadcom BCM47xx GPIO char driver
 *
 * Copyright (C) 2011, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 * $Id: linux_gpio.c 300516 2011-12-04 17:39:44Z $
 *
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>

#include <typedefs.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmdevs.h>

#include <linux_gpio.h>

/* handle to the sb */
static si_t *gpio_sih;

/* major number assigned to the device and device handles */
static int gpio_major;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
static struct class *gpiodev_class = NULL;
#else
devfs_handle_t gpiodev_handle;
#endif

static int
gpio_open(struct inode *inode, struct file * file)
{
	MOD_INC_USE_COUNT;
	return 0;
}

static int
gpio_release(struct inode *inode, struct file * file)
{
	MOD_DEC_USE_COUNT;
	return 0;
}

static int
gpio_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	struct gpio_ioctl gpioioc;

	if (copy_from_user(&gpioioc, (struct gpio_ioctl *)arg, sizeof(struct gpio_ioctl)))
		return -EFAULT;

	switch (cmd) {
		case GPIO_IOC_RESERVE:
			gpioioc.val = si_gpioreserve(gpio_sih, gpioioc.mask, GPIO_APP_PRIORITY);
			break;
		case GPIO_IOC_RELEASE:
			/*
			 * releasing the gpio doesn't change the current
			 * value on the GPIO last write value
			 * persists till some one overwrites it
			 */
			gpioioc.val = si_gpiorelease(gpio_sih, gpioioc.mask, GPIO_APP_PRIORITY);
			break;
		case GPIO_IOC_OUT:
			gpioioc.val = si_gpioout(gpio_sih, gpioioc.mask, gpioioc.val,
			                         GPIO_APP_PRIORITY);
			break;
		case GPIO_IOC_OUTEN:
			gpioioc.val = si_gpioouten(gpio_sih, gpioioc.mask, gpioioc.val,
			                           GPIO_APP_PRIORITY);
			break;
		case GPIO_IOC_IN:
			gpioioc.val = si_gpioin(gpio_sih);
			break;
		default:
			break;
	}
	if (copy_to_user((struct gpio_ioctl *)arg, &gpioioc, sizeof(struct gpio_ioctl)))
		return -EFAULT;

	return 0;

}
static struct file_operations gpio_fops = {
	owner:		THIS_MODULE,
	open:		gpio_open,
	release:	gpio_release,
	ioctl:		gpio_ioctl
};

static int __init
gpio_init(void)
{
	if (!(gpio_sih = si_kattach(SI_OSH)))
		return -ENODEV;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
	if ((gpio_major = register_chrdev(0, "gpio", &gpio_fops)) < 0)
#else
	if ((gpio_major = devfs_register_chrdev(0, "gpio", &gpio_fops)) < 0)
#endif
		return gpio_major;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
	gpiodev_class = class_create(THIS_MODULE, "gpio");
	if (IS_ERR(gpiodev_class)) {
		printk("Error creating gpio class\n");
		return -1;
	}

	/* Add the device gpio0 */
	class_device_create(gpiodev_class, NULL, MKDEV(gpio_major, 0), NULL, "gpio");
#else
	gpiodev_handle = devfs_register(NULL, "gpio", DEVFS_FL_DEFAULT,
	                                gpio_major, 0, S_IFCHR | S_IRUGO | S_IWUGO,
	                                &gpio_fops, NULL);
#endif

	return 0;
}

static void __exit
gpio_exit(void)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
	if (gpiodev_class != NULL) {
		class_device_destroy(gpiodev_class, MKDEV(gpio_major, 0));
		class_destroy(gpiodev_class);
	}

	gpiodev_class = NULL;
	if (gpio_major >= 0)
		unregister_chrdev(gpio_major, "gpio");
#else
	if (gpiodev_handle != NULL)
		devfs_unregister(gpiodev_handle);
	gpiodev_handle = NULL;
	devfs_unregister_chrdev(gpio_major, "gpio");
#endif
	si_detach(gpio_sih);
}

module_init(gpio_init);
module_exit(gpio_exit);
