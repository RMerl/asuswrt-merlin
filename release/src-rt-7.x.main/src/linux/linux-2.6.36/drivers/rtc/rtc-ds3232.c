/*
 * RTC client/driver for the Maxim/Dallas DS3232 Real-Time Clock over I2C
 *
 * Copyright (C) 2009-2010 Freescale Semiconductor.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */
/*
 * It would be more efficient to use i2c msgs/i2c_transfer directly but, as
 * recommened in .../Documentation/i2c/writing-clients section
 * "Sending and receiving", using SMBus level communication is preferred.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/rtc.h>
#include <linux/bcd.h>
#include <linux/workqueue.h>
#include <linux/slab.h>

#define DS3232_REG_SECONDS	0x00
#define DS3232_REG_MINUTES	0x01
#define DS3232_REG_HOURS	0x02
#define DS3232_REG_AMPM		0x02
#define DS3232_REG_DAY		0x03
#define DS3232_REG_DATE		0x04
#define DS3232_REG_MONTH	0x05
#define DS3232_REG_CENTURY	0x05
#define DS3232_REG_YEAR		0x06
#define DS3232_REG_ALARM1         0x07	/* Alarm 1 BASE */
#define DS3232_REG_ALARM2         0x0B	/* Alarm 2 BASE */
#define DS3232_REG_CR		0x0E	/* Control register */
#	define DS3232_REG_CR_nEOSC        0x80
#       define DS3232_REG_CR_INTCN        0x04
#       define DS3232_REG_CR_A2IE        0x02
#       define DS3232_REG_CR_A1IE        0x01

#define DS3232_REG_SR	0x0F	/* control/status register */
#	define DS3232_REG_SR_OSF   0x80
#       define DS3232_REG_SR_BSY   0x04
#       define DS3232_REG_SR_A2F   0x02
#       define DS3232_REG_SR_A1F   0x01

struct ds3232 {
	struct i2c_client *client;
	struct rtc_device *rtc;
	struct work_struct work;

	/* The mutex protects alarm operations, and prevents a race
	 * between the enable_irq() in the workqueue and the free_irq()
	 * in the remove function.
	 */
	struct mutex mutex;
	int exiting;
};

static struct i2c_driver ds3232_driver;

static int ds3232_check_rtc_status(struct i2c_client *client)
{
	int ret = 0;
	int control, stat;

	stat = i2c_smbus_read_byte_data(client, DS3232_REG_SR);
	if (stat < 0)
		return stat;

	if (stat & DS3232_REG_SR_OSF)
		dev_warn(&client->dev,
				"oscillator discontinuity flagged, "
				"time unreliable\n");

	stat &= ~(DS3232_REG_SR_OSF | DS3232_REG_SR_A1F | DS3232_REG_SR_A2F);

	ret = i2c_smbus_write_byte_data(client, DS3232_REG_SR, stat);
	if (ret < 0)
		return ret;

	/* If the alarm is pending, clear it before requesting
	 * the interrupt, so an interrupt event isn't reported
	 * before everything is initialized.
	 */

	control = i2c_smbus_read_byte_data(client, DS3232_REG_CR);
	if (control < 0)
		return control;

	control &= ~(DS3232_REG_CR_A1IE | DS3232_REG_CR_A2IE);
	control |= DS3232_REG_CR_INTCN;

	return i2c_smbus_write_byte_data(client, DS3232_REG_CR, control);
}

static int ds3232_read_time(struct device *dev, struct rtc_time *time)
{
	struct i2c_client *client = to_i2c_client(dev);
	int ret;
	u8 buf[7];
	unsigned int year, month, day, hour, minute, second;
	unsigned int week, twelve_hr, am_pm;
	unsigned int century, add_century = 0;

	ret = i2c_smbus_read_i2c_block_data(client, DS3232_REG_SECONDS, 7, buf);

	if (ret < 0)
		return ret;
	if (ret < 7)
		return -EIO;

	second = buf[0];
	minute = buf[1];
	hour = buf[2];
	week = buf[3];
	day = buf[4];
	month = buf[5];
	year = buf[6];

	/* Extract additional information for AM/PM and century */

	twelve_hr = hour & 0x40;
	am_pm = hour & 0x20;
	century = month & 0x80;

	/* Write to rtc_time structure */

	time->tm_sec = bcd2bin(second);
	time->tm_min = bcd2bin(minute);
	if (twelve_hr) {
		/* Convert to 24 hr */
		if (am_pm)
			time->tm_hour = bcd2bin(hour & 0x1F) + 12;
		else
			time->tm_hour = bcd2bin(hour & 0x1F);
	} else {
		time->tm_hour = bcd2bin(hour);
	}

	time->tm_wday = bcd2bin(week);
	time->tm_mday = bcd2bin(day);
	time->tm_mon = bcd2bin(month & 0x7F);
	if (century)
		add_century = 100;

	time->tm_year = bcd2bin(year) + add_century;

	return rtc_valid_tm(time);
}

static int ds3232_set_time(struct device *dev, struct rtc_time *time)
{
	struct i2c_client *client = to_i2c_client(dev);
	u8 buf[7];

	/* Extract time from rtc_time and load into ds3232*/

	buf[0] = bin2bcd(time->tm_sec);
	buf[1] = bin2bcd(time->tm_min);
	buf[2] = bin2bcd(time->tm_hour);
	buf[3] = bin2bcd(time->tm_wday); /* Day of the week */
	buf[4] = bin2bcd(time->tm_mday); /* Date */
	buf[5] = bin2bcd(time->tm_mon);
	if (time->tm_year >= 100) {
		buf[5] |= 0x80;
		buf[6] = bin2bcd(time->tm_year - 100);
	} else {
		buf[6] = bin2bcd(time->tm_year);
	}

	return i2c_smbus_write_i2c_block_data(client,
					      DS3232_REG_SECONDS, 7, buf);
}

static irqreturn_t ds3232_irq(int irq, void *dev_id)
{
	struct i2c_client *client = dev_id;
	struct ds3232 *ds3232 = i2c_get_clientdata(client);

	disable_irq_nosync(irq);
	schedule_work(&ds3232->work);
	return IRQ_HANDLED;
}

static void ds3232_work(struct work_struct *work)
{
	struct ds3232 *ds3232 = container_of(work, struct ds3232, work);
	struct i2c_client *client = ds3232->client;
	int stat, control;

	mutex_lock(&ds3232->mutex);

	stat = i2c_smbus_read_byte_data(client, DS3232_REG_SR);
	if (stat < 0)
		goto unlock;

	if (stat & DS3232_REG_SR_A1F) {
		control = i2c_smbus_read_byte_data(client, DS3232_REG_CR);
		if (control < 0)
			goto out;
		/* disable alarm1 interrupt */
		control &= ~(DS3232_REG_CR_A1IE);
		i2c_smbus_write_byte_data(client, DS3232_REG_CR, control);

		/* clear the alarm pend flag */
		stat &= ~DS3232_REG_SR_A1F;
		i2c_smbus_write_byte_data(client, DS3232_REG_SR, stat);

		rtc_update_irq(ds3232->rtc, 1, RTC_AF | RTC_IRQF);
	}

out:
	if (!ds3232->exiting)
		enable_irq(client->irq);
unlock:
	mutex_unlock(&ds3232->mutex);
}

static const struct rtc_class_ops ds3232_rtc_ops = {
	.read_time = ds3232_read_time,
	.set_time = ds3232_set_time,
};

static int __devinit ds3232_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct ds3232 *ds3232;
	int ret;

	ds3232 = kzalloc(sizeof(struct ds3232), GFP_KERNEL);
	if (!ds3232)
		return -ENOMEM;

	ds3232->client = client;
	i2c_set_clientdata(client, ds3232);

	INIT_WORK(&ds3232->work, ds3232_work);
	mutex_init(&ds3232->mutex);

	ret = ds3232_check_rtc_status(client);
	if (ret)
		goto out_free;

	ds3232->rtc = rtc_device_register(client->name, &client->dev,
					  &ds3232_rtc_ops, THIS_MODULE);
	if (IS_ERR(ds3232->rtc)) {
		ret = PTR_ERR(ds3232->rtc);
		dev_err(&client->dev, "unable to register the class device\n");
		goto out_irq;
	}

	if (client->irq >= 0) {
		ret = request_irq(client->irq, ds3232_irq, 0,
				 "ds3232", client);
		if (ret) {
			dev_err(&client->dev, "unable to request IRQ\n");
			goto out_free;
		}
	}

	return 0;

out_irq:
	if (client->irq >= 0)
		free_irq(client->irq, client);

out_free:
	kfree(ds3232);
	return ret;
}

static int __devexit ds3232_remove(struct i2c_client *client)
{
	struct ds3232 *ds3232 = i2c_get_clientdata(client);

	if (client->irq >= 0) {
		mutex_lock(&ds3232->mutex);
		ds3232->exiting = 1;
		mutex_unlock(&ds3232->mutex);

		free_irq(client->irq, client);
		flush_scheduled_work();
	}

	rtc_device_unregister(ds3232->rtc);
	kfree(ds3232);
	return 0;
}

static const struct i2c_device_id ds3232_id[] = {
	{ "ds3232", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ds3232_id);

static struct i2c_driver ds3232_driver = {
	.driver = {
		.name = "rtc-ds3232",
		.owner = THIS_MODULE,
	},
	.probe = ds3232_probe,
	.remove = __devexit_p(ds3232_remove),
	.id_table = ds3232_id,
};

static int __init ds3232_init(void)
{
	return i2c_add_driver(&ds3232_driver);
}

static void __exit ds3232_exit(void)
{
	i2c_del_driver(&ds3232_driver);
}

module_init(ds3232_init);
module_exit(ds3232_exit);

MODULE_AUTHOR("Srikanth Srinivasan <srikanth.srinivasan@freescale.com>");
MODULE_DESCRIPTION("Maxim/Dallas DS3232 RTC Driver");
MODULE_LICENSE("GPL");
