/*
 * Linux Broadcom BCM47xx GPIO char driver
 *
 * Copyright (C) 2015, Broadcom Corporation. All Rights Reserved.
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
 *
 * $Id: linux_gpio.h 241182 2011-02-17 21:50:03Z $
 */

#ifndef _linux_gpio_h_
#define _linux_gpio_h_

struct gpio_ioctl {
	uint32 mask;
	uint32 val;
};

#define GPIO_IOC_MAGIC  'G'

/* reserve/release a gpio to the caller */
#define  GPIO_IOC_RESERVE	_IOWR(GPIO_IOC_MAGIC, 1, struct gpio_ioctl)
#define  GPIO_IOC_RELEASE	_IOWR(GPIO_IOC_MAGIC, 2, struct gpio_ioctl)
/* ioctls to read/write the gpio registers */
#define  GPIO_IOC_OUT		_IOWR(GPIO_IOC_MAGIC, 3, struct gpio_ioctl)
#define  GPIO_IOC_IN		_IOWR(GPIO_IOC_MAGIC, 4, struct gpio_ioctl)
#define  GPIO_IOC_OUTEN		_IOWR(GPIO_IOC_MAGIC, 5, struct gpio_ioctl)

#endif	/* _linux_gpio_h_ */
