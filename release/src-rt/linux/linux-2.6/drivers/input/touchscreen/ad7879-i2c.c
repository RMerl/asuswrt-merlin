/*
 * AD7879-1/AD7889-1 touchscreen (I2C bus)
 *
 * Copyright (C) 2008-2010 Michael Hennerich, Analog Devices Inc.
 *
 * Licensed under the GPL-2 or later.
 */

#include <linux/input.h>	/* BUS_I2C */
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/pm.h>

#include "ad7879.h"

#define AD7879_DEVID		0x79	/* AD7879-1/AD7889-1 */

#ifdef CONFIG_PM
static int ad7879_i2c_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ad7879 *ts = i2c_get_clientdata(client);

	ad7879_suspend(ts);

	return 0;
}

static int ad7879_i2c_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ad7879 *ts = i2c_get_clientdata(client);

	ad7879_resume(ts);

	return 0;
}

static SIMPLE_DEV_PM_OPS(ad7879_i2c_pm, ad7879_i2c_suspend, ad7879_i2c_resume);
#endif

/* All registers are word-sized.
 * AD7879 uses a high-byte first convention.
 */
static int ad7879_i2c_read(struct device *dev, u8 reg)
{
	struct i2c_client *client = to_i2c_client(dev);

	return swab16(i2c_smbus_read_word_data(client, reg));
}

static int ad7879_i2c_multi_read(struct device *dev,
				 u8 first_reg, u8 count, u16 *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	u8 idx;

	i2c_smbus_read_i2c_block_data(client, first_reg, count * 2, (u8 *)buf);

	for (idx = 0; idx < count; ++idx)
		buf[idx] = swab16(buf[idx]);

	return 0;
}

static int ad7879_i2c_write(struct device *dev, u8 reg, u16 val)
{
	struct i2c_client *client = to_i2c_client(dev);

	return i2c_smbus_write_word_data(client, reg, swab16(val));
}

static const struct ad7879_bus_ops ad7879_i2c_bus_ops = {
	.bustype	= BUS_I2C,
	.read		= ad7879_i2c_read,
	.multi_read	= ad7879_i2c_multi_read,
	.write		= ad7879_i2c_write,
};

static int __devinit ad7879_i2c_probe(struct i2c_client *client,
				      const struct i2c_device_id *id)
{
	struct ad7879 *ts;

	if (!i2c_check_functionality(client->adapter,
				     I2C_FUNC_SMBUS_WORD_DATA)) {
		dev_err(&client->dev, "SMBUS Word Data not Supported\n");
		return -EIO;
	}

	ts = ad7879_probe(&client->dev, AD7879_DEVID, client->irq,
			  &ad7879_i2c_bus_ops);
	if (IS_ERR(ts))
		return PTR_ERR(ts);

	i2c_set_clientdata(client, ts);

	return 0;
}

static int __devexit ad7879_i2c_remove(struct i2c_client *client)
{
	struct ad7879 *ts = i2c_get_clientdata(client);

	ad7879_remove(ts);

	return 0;
}

static const struct i2c_device_id ad7879_id[] = {
	{ "ad7879", 0 },
	{ "ad7889", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ad7879_id);

static struct i2c_driver ad7879_i2c_driver = {
	.driver = {
		.name	= "ad7879",
		.owner	= THIS_MODULE,
#ifdef CONFIG_PM
		.pm	= &ad7879_i2c_pm,
#endif
	},
	.probe		= ad7879_i2c_probe,
	.remove		= __devexit_p(ad7879_i2c_remove),
	.id_table	= ad7879_id,
};

static int __init ad7879_i2c_init(void)
{
	return i2c_add_driver(&ad7879_i2c_driver);
}
module_init(ad7879_i2c_init);

static void __exit ad7879_i2c_exit(void)
{
	i2c_del_driver(&ad7879_i2c_driver);
}
module_exit(ad7879_i2c_exit);

MODULE_AUTHOR("Michael Hennerich <hennerich@blackfin.uclinux.org>");
MODULE_DESCRIPTION("AD7879(-1) touchscreen I2C bus driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("i2c:ad7879");
