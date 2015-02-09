/* Texas Instruments TMP102 SMBus temperature sensor driver
 *
 * Copyright (C) 2010 Steven King <sfking@fdwdc.com>
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
 * Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/device.h>

#define	DRIVER_NAME "tmp102"

#define	TMP102_TEMP_REG			0x00
#define	TMP102_CONF_REG			0x01
/* note: these bit definitions are byte swapped */
#define		TMP102_CONF_SD		0x0100
#define		TMP102_CONF_TM		0x0200
#define		TMP102_CONF_POL		0x0400
#define		TMP102_CONF_F0		0x0800
#define		TMP102_CONF_F1		0x1000
#define		TMP102_CONF_R0		0x2000
#define		TMP102_CONF_R1		0x4000
#define		TMP102_CONF_OS		0x8000
#define		TMP102_CONF_EM		0x0010
#define		TMP102_CONF_AL		0x0020
#define		TMP102_CONF_CR0		0x0040
#define		TMP102_CONF_CR1		0x0080
#define	TMP102_TLOW_REG			0x02
#define	TMP102_THIGH_REG		0x03

struct tmp102 {
	struct device *hwmon_dev;
	struct mutex lock;
	u16 config_orig;
	unsigned long last_update;
	int temp[3];
};

/* SMBus specifies low byte first, but the TMP102 returns high byte first,
 * so we have to swab16 the values */
static inline int tmp102_read_reg(struct i2c_client *client, u8 reg)
{
	int result = i2c_smbus_read_word_data(client, reg);
	return result < 0 ? result : swab16(result);
}

static inline int tmp102_write_reg(struct i2c_client *client, u8 reg, u16 val)
{
	return i2c_smbus_write_word_data(client, reg, swab16(val));
}

/* convert left adjusted 13-bit TMP102 register value to milliCelsius */
static inline int tmp102_reg_to_mC(s16 val)
{
	return ((val & ~0x01) * 1000) / 128;
}

/* convert milliCelsius to left adjusted 13-bit TMP102 register value */
static inline u16 tmp102_mC_to_reg(int val)
{
	return (val * 128) / 1000;
}

static const u8 tmp102_reg[] = {
	TMP102_TEMP_REG,
	TMP102_TLOW_REG,
	TMP102_THIGH_REG,
};

static struct tmp102 *tmp102_update_device(struct i2c_client *client)
{
	struct tmp102 *tmp102 = i2c_get_clientdata(client);

	mutex_lock(&tmp102->lock);
	if (time_after(jiffies, tmp102->last_update + HZ / 3)) {
		int i;
		for (i = 0; i < ARRAY_SIZE(tmp102->temp); ++i) {
			int status = tmp102_read_reg(client, tmp102_reg[i]);
			if (status > -1)
				tmp102->temp[i] = tmp102_reg_to_mC(status);
		}
		tmp102->last_update = jiffies;
	}
	mutex_unlock(&tmp102->lock);
	return tmp102;
}

static ssize_t tmp102_show_temp(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct sensor_device_attribute *sda = to_sensor_dev_attr(attr);
	struct tmp102 *tmp102 = tmp102_update_device(to_i2c_client(dev));

	return sprintf(buf, "%d\n", tmp102->temp[sda->index]);
}

static ssize_t tmp102_set_temp(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	struct sensor_device_attribute *sda = to_sensor_dev_attr(attr);
	struct i2c_client *client = to_i2c_client(dev);
	struct tmp102 *tmp102 = i2c_get_clientdata(client);
	long val;
	int status;

	if (strict_strtol(buf, 10, &val) < 0)
		return -EINVAL;
	val = SENSORS_LIMIT(val, -256000, 255000);

	mutex_lock(&tmp102->lock);
	tmp102->temp[sda->index] = val;
	status = tmp102_write_reg(client, tmp102_reg[sda->index],
				  tmp102_mC_to_reg(val));
	mutex_unlock(&tmp102->lock);
	return status ? : count;
}

static SENSOR_DEVICE_ATTR(temp1_input, S_IRUGO, tmp102_show_temp, NULL , 0);

static SENSOR_DEVICE_ATTR(temp1_max_hyst, S_IWUSR | S_IRUGO, tmp102_show_temp,
			  tmp102_set_temp, 1);

static SENSOR_DEVICE_ATTR(temp1_max, S_IWUSR | S_IRUGO, tmp102_show_temp,
			  tmp102_set_temp, 2);

static struct attribute *tmp102_attributes[] = {
	&sensor_dev_attr_temp1_input.dev_attr.attr,
	&sensor_dev_attr_temp1_max_hyst.dev_attr.attr,
	&sensor_dev_attr_temp1_max.dev_attr.attr,
	NULL
};

static const struct attribute_group tmp102_attr_group = {
	.attrs = tmp102_attributes,
};

#define TMP102_CONFIG  (TMP102_CONF_TM | TMP102_CONF_EM | TMP102_CONF_CR1)
#define TMP102_CONFIG_RD_ONLY (TMP102_CONF_R0 | TMP102_CONF_R1 | TMP102_CONF_AL)

static int __devinit tmp102_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	struct tmp102 *tmp102;
	int status;

	if (!i2c_check_functionality(client->adapter,
				     I2C_FUNC_SMBUS_WORD_DATA)) {
		dev_err(&client->dev, "adapter doesnt support SMBus word "
			"transactions\n");
		return -ENODEV;
	}

	tmp102 = kzalloc(sizeof(*tmp102), GFP_KERNEL);
	if (!tmp102) {
		dev_dbg(&client->dev, "kzalloc failed\n");
		return -ENOMEM;
	}
	i2c_set_clientdata(client, tmp102);

	status = tmp102_read_reg(client, TMP102_CONF_REG);
	if (status < 0) {
		dev_err(&client->dev, "error reading config register\n");
		goto fail_free;
	}
	tmp102->config_orig = status;
	status = tmp102_write_reg(client, TMP102_CONF_REG, TMP102_CONFIG);
	if (status < 0) {
		dev_err(&client->dev, "error writing config register\n");
		goto fail_restore_config;
	}
	status = tmp102_read_reg(client, TMP102_CONF_REG);
	if (status < 0) {
		dev_err(&client->dev, "error reading config register\n");
		goto fail_restore_config;
	}
	status &= ~TMP102_CONFIG_RD_ONLY;
	if (status != TMP102_CONFIG) {
		dev_err(&client->dev, "config settings did not stick\n");
		status = -ENODEV;
		goto fail_restore_config;
	}
	tmp102->last_update = jiffies - HZ;
	mutex_init(&tmp102->lock);

	status = sysfs_create_group(&client->dev.kobj, &tmp102_attr_group);
	if (status) {
		dev_dbg(&client->dev, "could not create sysfs files\n");
		goto fail_restore_config;
	}
	tmp102->hwmon_dev = hwmon_device_register(&client->dev);
	if (IS_ERR(tmp102->hwmon_dev)) {
		dev_dbg(&client->dev, "unable to register hwmon device\n");
		status = PTR_ERR(tmp102->hwmon_dev);
		goto fail_remove_sysfs;
	}

	dev_info(&client->dev, "initialized\n");

	return 0;

fail_remove_sysfs:
	sysfs_remove_group(&client->dev.kobj, &tmp102_attr_group);
fail_restore_config:
	tmp102_write_reg(client, TMP102_CONF_REG, tmp102->config_orig);
fail_free:
	kfree(tmp102);

	return status;
}

static int __devexit tmp102_remove(struct i2c_client *client)
{
	struct tmp102 *tmp102 = i2c_get_clientdata(client);

	hwmon_device_unregister(tmp102->hwmon_dev);
	sysfs_remove_group(&client->dev.kobj, &tmp102_attr_group);

	/* Stop monitoring if device was stopped originally */
	if (tmp102->config_orig & TMP102_CONF_SD) {
		int config;

		config = tmp102_read_reg(client, TMP102_CONF_REG);
		if (config >= 0)
			tmp102_write_reg(client, TMP102_CONF_REG,
					 config | TMP102_CONF_SD);
	}

	kfree(tmp102);

	return 0;
}

#ifdef CONFIG_PM
static int tmp102_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	int config;

	config = tmp102_read_reg(client, TMP102_CONF_REG);
	if (config < 0)
		return config;

	config |= TMP102_CONF_SD;
	return tmp102_write_reg(client, TMP102_CONF_REG, config);
}

static int tmp102_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	int config;

	config = tmp102_read_reg(client, TMP102_CONF_REG);
	if (config < 0)
		return config;

	config &= ~TMP102_CONF_SD;
	return tmp102_write_reg(client, TMP102_CONF_REG, config);
}

static const struct dev_pm_ops tmp102_dev_pm_ops = {
	.suspend	= tmp102_suspend,
	.resume		= tmp102_resume,
};

#define TMP102_DEV_PM_OPS (&tmp102_dev_pm_ops)
#else
#define	TMP102_DEV_PM_OPS NULL
#endif /* CONFIG_PM */

static const struct i2c_device_id tmp102_id[] = {
	{ "tmp102", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, tmp102_id);

static struct i2c_driver tmp102_driver = {
	.driver.name	= DRIVER_NAME,
	.driver.pm	= TMP102_DEV_PM_OPS,
	.probe		= tmp102_probe,
	.remove		= __devexit_p(tmp102_remove),
	.id_table	= tmp102_id,
};

static int __init tmp102_init(void)
{
	return i2c_add_driver(&tmp102_driver);
}
module_init(tmp102_init);

static void __exit tmp102_exit(void)
{
	i2c_del_driver(&tmp102_driver);
}
module_exit(tmp102_exit);

MODULE_AUTHOR("Steven King <sfking@fdwdc.com>");
MODULE_DESCRIPTION("Texas Instruments TMP102 temperature sensor driver");
MODULE_LICENSE("GPL");
