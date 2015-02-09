/*
 * wm831x-otp.c  --  OTP for Wolfson WM831x PMICs
 *
 * Copyright 2009 Wolfson Microelectronics PLC.
 *
 * Author: Mark Brown <broonie@opensource.wolfsonmicro.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/bcd.h>
#include <linux/delay.h>
#include <linux/mfd/core.h>

#include <linux/mfd/wm831x/core.h>
#include <linux/mfd/wm831x/otp.h>

/* In bytes */
#define WM831X_UNIQUE_ID_LEN 16

/* Read the unique ID from the chip into id */
static int wm831x_unique_id_read(struct wm831x *wm831x, char *id)
{
	int i, val;

	for (i = 0; i < WM831X_UNIQUE_ID_LEN / 2; i++) {
		val = wm831x_reg_read(wm831x, WM831X_UNIQUE_ID_1 + i);
		if (val < 0)
			return val;

		id[i * 2]       = (val >> 8) & 0xff;
		id[(i * 2) + 1] = val & 0xff;
	}

	return 0;
}

static ssize_t wm831x_unique_id_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct wm831x *wm831x = dev_get_drvdata(dev);
	int i, rval;
	char id[WM831X_UNIQUE_ID_LEN];
	ssize_t ret = 0;

	rval = wm831x_unique_id_read(wm831x, id);
	if (rval < 0)
		return 0;

	for (i = 0; i < WM831X_UNIQUE_ID_LEN; i++)
		ret += sprintf(&buf[ret], "%02x", buf[i]);

	ret += sprintf(&buf[ret], "\n");

	return ret;
}

static DEVICE_ATTR(unique_id, 0444, wm831x_unique_id_show, NULL);

int wm831x_otp_init(struct wm831x *wm831x)
{
	int ret;

	ret = device_create_file(wm831x->dev, &dev_attr_unique_id);
	if (ret != 0)
		dev_err(wm831x->dev, "Unique ID attribute not created: %d\n",
			ret);

	return ret;
}

void wm831x_otp_exit(struct wm831x *wm831x)
{
	device_remove_file(wm831x->dev, &dev_attr_unique_id);
}
