/*
 * Driver for Linear Technology LTC4151 High Voltage I2C Current
 * and Voltage Monitor
 *
 * Copyright (C) 2011 AppearTV AS
 *
 * Derived from:
 *
 *  Driver for Linear Technology LTC4261 I2C Negative Voltage Hot
 *  Swap Controller
 *  Copyright (C) 2010 Ericsson AB.
 *
 * Datasheet: http://www.linear.com/docs/Datasheet/4151fc.pdf
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>

/* chip registers */
#define LTC4151_SENSE_H	0x00
#define LTC4151_SENSE_L	0x01
#define LTC4151_VIN_H	0x02
#define LTC4151_VIN_L	0x03
#define LTC4151_ADIN_H	0x04
#define LTC4151_ADIN_L	0x05

struct ltc4151_data {
	struct device *hwmon_dev;

	struct mutex update_lock;
	bool valid;
	unsigned long last_updated; /* in jiffies */

	/* Registers */
	u8 regs[6];
};

static struct ltc4151_data *ltc4151_update_device(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ltc4151_data *data = i2c_get_clientdata(client);
	struct ltc4151_data *ret = data;

	mutex_lock(&data->update_lock);

	/*
	 * The chip's A/D updates 6 times per second
	 * (Conversion Rate 6 - 9 Hz)
	 */
	if (time_after(jiffies, data->last_updated + HZ / 6) || !data->valid) {
		int i;

		dev_dbg(&client->dev, "Starting ltc4151 update\n");

		/* Read all registers */
		for (i = 0; i < ARRAY_SIZE(data->regs); i++) {
			int val;

			val = i2c_smbus_read_byte_data(client, i);
			if (unlikely(val < 0)) {
				dev_dbg(dev,
					"Failed to read ADC value: error %d\n",
					val);
				ret = ERR_PTR(val);
				goto abort;
			}
			data->regs[i] = val;
		}
		data->last_updated = jiffies;
		data->valid = 1;
	}
abort:
	mutex_unlock(&data->update_lock);
	return ret;
}

/* Return the voltage from the given register in mV */
static int ltc4151_get_value(struct ltc4151_data *data, u8 reg)
{
	u32 val;

	val = (data->regs[reg] << 4) + (data->regs[reg + 1] >> 4);

	switch (reg) {
	case LTC4151_ADIN_H:
		/* 500uV resolution. Convert to mV. */
		val = val * 500 / 1000;
		break;
	case LTC4151_SENSE_H:
		/*
		 * 20uV resolution. Convert to current as measured with
		 * an 1 mOhm sense resistor, in mA.
		 */
		val = val * 20;
		break;
	case LTC4151_VIN_H:
		/* 25 mV per increment */
		val = val * 25;
		break;
	default:
		/* If we get here, the developer messed up */
		WARN_ON_ONCE(1);
		val = 0;
		break;
	}

	return val;
}

static ssize_t ltc4151_show_value(struct device *dev,
				  struct device_attribute *da, char *buf)
{
	struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
	struct ltc4151_data *data = ltc4151_update_device(dev);
	int value;

	if (IS_ERR(data))
		return PTR_ERR(data);

	value = ltc4151_get_value(data, attr->index);
	return snprintf(buf, PAGE_SIZE, "%d\n", value);
}

/*
 * Input voltages.
 */
static SENSOR_DEVICE_ATTR(in1_input, S_IRUGO, \
	ltc4151_show_value, NULL, LTC4151_VIN_H);
static SENSOR_DEVICE_ATTR(in2_input, S_IRUGO, \
	ltc4151_show_value, NULL, LTC4151_ADIN_H);

/* Currents (via sense resistor) */
static SENSOR_DEVICE_ATTR(curr1_input, S_IRUGO, \
	ltc4151_show_value, NULL, LTC4151_SENSE_H);

/* Finally, construct an array of pointers to members of the above objects,
 * as required for sysfs_create_group()
 */
static struct attribute *ltc4151_attributes[] = {
	&sensor_dev_attr_in1_input.dev_attr.attr,
	&sensor_dev_attr_in2_input.dev_attr.attr,

	&sensor_dev_attr_curr1_input.dev_attr.attr,

	NULL,
};

static const struct attribute_group ltc4151_group = {
	.attrs = ltc4151_attributes,
};

static int ltc4151_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = client->adapter;
	struct ltc4151_data *data;
	int ret;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		return -ENODEV;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data) {
		ret = -ENOMEM;
		goto out_kzalloc;
	}

	i2c_set_clientdata(client, data);
	mutex_init(&data->update_lock);

	/* Register sysfs hooks */
	ret = sysfs_create_group(&client->dev.kobj, &ltc4151_group);
	if (ret)
		goto out_sysfs_create_group;

	data->hwmon_dev = hwmon_device_register(&client->dev);
	if (IS_ERR(data->hwmon_dev)) {
		ret = PTR_ERR(data->hwmon_dev);
		goto out_hwmon_device_register;
	}

	return 0;

out_hwmon_device_register:
	sysfs_remove_group(&client->dev.kobj, &ltc4151_group);
out_sysfs_create_group:
	kfree(data);
out_kzalloc:
	return ret;
}

static int ltc4151_remove(struct i2c_client *client)
{
	struct ltc4151_data *data = i2c_get_clientdata(client);

	hwmon_device_unregister(data->hwmon_dev);
	sysfs_remove_group(&client->dev.kobj, &ltc4151_group);

	kfree(data);

	return 0;
}

static const struct i2c_device_id ltc4151_id[] = {
	{ "ltc4151", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ltc4151_id);

/* This is the driver that will be inserted */
static struct i2c_driver ltc4151_driver = {
	.driver = {
		.name	= "ltc4151",
	},
	.probe		= ltc4151_probe,
	.remove		= ltc4151_remove,
	.id_table	= ltc4151_id,
};

static int __init ltc4151_init(void)
{
	return i2c_add_driver(&ltc4151_driver);
}

static void __exit ltc4151_exit(void)
{
	i2c_del_driver(&ltc4151_driver);
}

MODULE_AUTHOR("Per Dalen <per.dalen@appeartv.com>");
MODULE_DESCRIPTION("LTC4151 driver");
MODULE_LICENSE("GPL");

module_init(ltc4151_init);
module_exit(ltc4151_exit);
