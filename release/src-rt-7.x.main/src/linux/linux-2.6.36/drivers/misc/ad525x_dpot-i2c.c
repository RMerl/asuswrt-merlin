/*
 * Driver for the Analog Devices digital potentiometers (I2C bus)
 *
 * Copyright (C) 2010 Michael Hennerich, Analog Devices Inc.
 *
 * Licensed under the GPL-2 or later.
 */

#include <linux/i2c.h>
#include <linux/module.h>

#include "ad525x_dpot.h"

/* ------------------------------------------------------------------------- */
/* I2C bus functions */
static int write_d8(void *client, u8 val)
{
	return i2c_smbus_write_byte(client, val);
}

static int write_r8d8(void *client, u8 reg, u8 val)
{
	return i2c_smbus_write_byte_data(client, reg, val);
}

static int write_r8d16(void *client, u8 reg, u16 val)
{
	return i2c_smbus_write_word_data(client, reg, val);
}

static int read_d8(void *client)
{
	return i2c_smbus_read_byte(client);
}

static int read_r8d8(void *client, u8 reg)
{
	return i2c_smbus_read_byte_data(client, reg);
}

static int read_r8d16(void *client, u8 reg)
{
	return i2c_smbus_read_word_data(client, reg);
}

static const struct ad_dpot_bus_ops bops = {
	.read_d8	= read_d8,
	.read_r8d8	= read_r8d8,
	.read_r8d16	= read_r8d16,
	.write_d8	= write_d8,
	.write_r8d8	= write_r8d8,
	.write_r8d16	= write_r8d16,
};

static int __devinit ad_dpot_i2c_probe(struct i2c_client *client,
				      const struct i2c_device_id *id)
{
	struct ad_dpot_bus_data bdata = {
		.client = client,
		.bops = &bops,
	};

	struct ad_dpot_id dpot_id = {
		.name = (char *) &id->name,
		.devid = id->driver_data,
	};

	if (!i2c_check_functionality(client->adapter,
				     I2C_FUNC_SMBUS_WORD_DATA)) {
		dev_err(&client->dev, "SMBUS Word Data not Supported\n");
		return -EIO;
	}

	return ad_dpot_probe(&client->dev, &bdata, &dpot_id);
}

static int __devexit ad_dpot_i2c_remove(struct i2c_client *client)
{
	return ad_dpot_remove(&client->dev);
}

static const struct i2c_device_id ad_dpot_id[] = {
	{"ad5258", AD5258_ID},
	{"ad5259", AD5259_ID},
	{"ad5251", AD5251_ID},
	{"ad5252", AD5252_ID},
	{"ad5253", AD5253_ID},
	{"ad5254", AD5254_ID},
	{"ad5255", AD5255_ID},
	{"ad5241", AD5241_ID},
	{"ad5242", AD5242_ID},
	{"ad5243", AD5243_ID},
	{"ad5245", AD5245_ID},
	{"ad5246", AD5246_ID},
	{"ad5247", AD5247_ID},
	{"ad5248", AD5248_ID},
	{"ad5280", AD5280_ID},
	{"ad5282", AD5282_ID},
	{"adn2860", ADN2860_ID},
	{"ad5273", AD5273_ID},
	{"ad5171", AD5171_ID},
	{"ad5170", AD5170_ID},
	{"ad5172", AD5172_ID},
	{"ad5173", AD5173_ID},
	{}
};
MODULE_DEVICE_TABLE(i2c, ad_dpot_id);

static struct i2c_driver ad_dpot_i2c_driver = {
	.driver = {
		.name	= "ad_dpot",
		.owner	= THIS_MODULE,
	},
	.probe		= ad_dpot_i2c_probe,
	.remove		= __devexit_p(ad_dpot_i2c_remove),
	.id_table	= ad_dpot_id,
};

static int __init ad_dpot_i2c_init(void)
{
	return i2c_add_driver(&ad_dpot_i2c_driver);
}
module_init(ad_dpot_i2c_init);

static void __exit ad_dpot_i2c_exit(void)
{
	i2c_del_driver(&ad_dpot_i2c_driver);
}
module_exit(ad_dpot_i2c_exit);

MODULE_AUTHOR("Michael Hennerich <hennerich@blackfin.uclinux.org>");
MODULE_DESCRIPTION("digital potentiometer I2C bus driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("i2c:ad_dpot");
