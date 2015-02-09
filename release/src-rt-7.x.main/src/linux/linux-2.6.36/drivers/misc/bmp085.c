/*  Copyright (c) 2010  Christoph Mair <christoph.mair@gmail.com>

    This driver supports the bmp085 digital barometric pressure
    and temperature sensor from Bosch Sensortec. The datasheet
    is avaliable from their website:
    http://www.bosch-sensortec.com/content/language1/downloads/BST-BMP085-DS000-05.pdf

    A pressure measurement is issued by reading from pressure0_input.
    The return value ranges from 30000 to 110000 pascal with a resulution
    of 1 pascal (0.01 millibar) which enables measurements from 9000m above
    to 500m below sea level.

    The temperature can be read from temp0_input. Values range from
    -400 to 850 representing the ambient temperature in degree celsius
    multiplied by 10.The resolution is 0.1 celsius.

    Because ambient pressure is temperature dependent, a temperature
    measurement will be executed automatically even if the user is reading
    from pressure0_input. This happens if the last temperature measurement
    has been executed more then one second ago.

    To decrease RMS noise from pressure measurements, the bmp085 can
    autonomously calculate the average of up to eight samples. This is
    set up by writing to the oversampling sysfs file. Accepted values
    are 0, 1, 2 and 3. 2^x when x is the value written to this file
    specifies the number of samples used to calculate the ambient pressure.
    RMS noise is specified with six pascal (without averaging) and decreases
    down to 3 pascal when using an oversampling setting of 3.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/


#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/delay.h>


#define BMP085_I2C_ADDRESS		0x77
#define BMP085_CHIP_ID			0x55

#define BMP085_CALIBRATION_DATA_START	0xAA
#define BMP085_CALIBRATION_DATA_LENGTH	11	/* 16 bit values */
#define BMP085_CHIP_ID_REG		0xD0
#define BMP085_VERSION_REG		0xD1
#define BMP085_CTRL_REG			0xF4
#define BMP085_TEMP_MEASUREMENT		0x2E
#define BMP085_PRESSURE_MEASUREMENT	0x34
#define BMP085_CONVERSION_REGISTER_MSB	0xF6
#define BMP085_CONVERSION_REGISTER_LSB	0xF7
#define BMP085_CONVERSION_REGISTER_XLSB	0xF8
#define BMP085_TEMP_CONVERSION_TIME	5

#define BMP085_CLIENT_NAME		"bmp085"


static const unsigned short normal_i2c[] = { BMP085_I2C_ADDRESS,
							I2C_CLIENT_END };

struct bmp085_calibration_data {
	s16 AC1, AC2, AC3;
	u16 AC4, AC5, AC6;
	s16 B1, B2;
	s16 MB, MC, MD;
};


/* Each client has this additional data */
struct bmp085_data {
	struct i2c_client *client;
	struct mutex lock;
	struct bmp085_calibration_data calibration;
	u32 raw_temperature;
	u32 raw_pressure;
	unsigned char oversampling_setting;
	u32 last_temp_measurement;
	s32 b6; /* calculated temperature correction coefficient */
};


static s32 bmp085_read_calibration_data(struct i2c_client *client)
{
	u16 tmp[BMP085_CALIBRATION_DATA_LENGTH];
	struct bmp085_data *data = i2c_get_clientdata(client);
	struct bmp085_calibration_data *cali = &(data->calibration);
	s32 status = i2c_smbus_read_i2c_block_data(client,
				BMP085_CALIBRATION_DATA_START,
				BMP085_CALIBRATION_DATA_LENGTH*sizeof(u16),
				(u8 *)tmp);
	if (status < 0)
		return status;

	if (status != BMP085_CALIBRATION_DATA_LENGTH*sizeof(u16))
		return -EIO;

	cali->AC1 =  be16_to_cpu(tmp[0]);
	cali->AC2 =  be16_to_cpu(tmp[1]);
	cali->AC3 =  be16_to_cpu(tmp[2]);
	cali->AC4 =  be16_to_cpu(tmp[3]);
	cali->AC5 =  be16_to_cpu(tmp[4]);
	cali->AC6 = be16_to_cpu(tmp[5]);
	cali->B1 = be16_to_cpu(tmp[6]);
	cali->B2 = be16_to_cpu(tmp[7]);
	cali->MB = be16_to_cpu(tmp[8]);
	cali->MC = be16_to_cpu(tmp[9]);
	cali->MD = be16_to_cpu(tmp[10]);
	return 0;
}


static s32 bmp085_update_raw_temperature(struct bmp085_data *data)
{
	u16 tmp;
	s32 status;

	mutex_lock(&data->lock);
	status = i2c_smbus_write_byte_data(data->client, BMP085_CTRL_REG,
						BMP085_TEMP_MEASUREMENT);
	if (status != 0) {
		dev_err(&data->client->dev,
			"Error while requesting temperature measurement.\n");
		goto exit;
	}
	msleep(BMP085_TEMP_CONVERSION_TIME);

	status = i2c_smbus_read_i2c_block_data(data->client,
		BMP085_CONVERSION_REGISTER_MSB, sizeof(tmp), (u8 *)&tmp);
	if (status < 0)
		goto exit;
	if (status != sizeof(tmp)) {
		dev_err(&data->client->dev,
			"Error while reading temperature measurement result\n");
		status = -EIO;
		goto exit;
	}
	data->raw_temperature = be16_to_cpu(tmp);
	data->last_temp_measurement = jiffies;
	status = 0;	/* everything ok, return 0 */

exit:
	mutex_unlock(&data->lock);
	return status;
}

static s32 bmp085_update_raw_pressure(struct bmp085_data *data)
{
	u32 tmp = 0;
	s32 status;

	mutex_lock(&data->lock);
	status = i2c_smbus_write_byte_data(data->client, BMP085_CTRL_REG,
		BMP085_PRESSURE_MEASUREMENT + (data->oversampling_setting<<6));
	if (status != 0) {
		dev_err(&data->client->dev,
			"Error while requesting pressure measurement.\n");
		goto exit;
	}

	/* wait for the end of conversion */
	msleep(2+(3 << data->oversampling_setting));

	/* copy data into a u32 (4 bytes), but skip the first byte. */
	status = i2c_smbus_read_i2c_block_data(data->client,
			BMP085_CONVERSION_REGISTER_MSB, 3, ((u8 *)&tmp)+1);
	if (status < 0)
		goto exit;
	if (status != 3) {
		dev_err(&data->client->dev,
			"Error while reading pressure measurement results\n");
		status = -EIO;
		goto exit;
	}
	data->raw_pressure = be32_to_cpu((tmp));
	data->raw_pressure >>= (8-data->oversampling_setting);
	status = 0;	/* everything ok, return 0 */

exit:
	mutex_unlock(&data->lock);
	return status;
}


/*
 * This function starts the temperature measurement and returns the value
 * in tenth of a degree celsius.
 */
static s32 bmp085_get_temperature(struct bmp085_data *data, int *temperature)
{
	struct bmp085_calibration_data *cali = &data->calibration;
	long x1, x2;
	int status;

	status = bmp085_update_raw_temperature(data);
	if (status != 0)
		goto exit;

	x1 = ((data->raw_temperature - cali->AC6) * cali->AC5) >> 15;
	x2 = (cali->MC << 11) / (x1 + cali->MD);
	data->b6 = x1 + x2 - 4000;
	/* if NULL just update b6. Used for pressure only measurements */
	if (temperature != NULL)
		*temperature = (x1+x2+8) >> 4;

exit:
	return status;;
}

/*
 * This function starts the pressure measurement and returns the value
 * in millibar. Since the pressure depends on the ambient temperature,
 * a temperature measurement is executed if the last known value is older
 * than one second.
 */
static s32 bmp085_get_pressure(struct bmp085_data *data, int *pressure)
{
	struct bmp085_calibration_data *cali = &data->calibration;
	s32 x1, x2, x3, b3;
	u32 b4, b7;
	s32 p;
	int status;

	/* alt least every second force an update of the ambient temperature */
	if (data->last_temp_measurement + 1*HZ < jiffies) {
		status = bmp085_get_temperature(data, NULL);
		if (status != 0)
			goto exit;
	}

	status = bmp085_update_raw_pressure(data);
	if (status != 0)
		goto exit;

	x1 = (data->b6 * data->b6) >> 12;
	x1 *= cali->B2;
	x1 >>= 11;

	x2 = cali->AC2 * data->b6;
	x2 >>= 11;

	x3 = x1 + x2;

	b3 = (((((s32)cali->AC1) * 4 + x3) << data->oversampling_setting) + 2);
	b3 >>= 2;

	x1 = (cali->AC3 * data->b6) >> 13;
	x2 = (cali->B1 * ((data->b6 * data->b6) >> 12)) >> 16;
	x3 = (x1 + x2 + 2) >> 2;
	b4 = (cali->AC4 * (u32)(x3 + 32768)) >> 15;

	b7 = ((u32)data->raw_pressure - b3) *
					(50000 >> data->oversampling_setting);
	p = ((b7 < 0x80000000) ? ((b7 << 1) / b4) : ((b7 / b4) * 2));

	x1 = p >> 8;
	x1 *= x1;
	x1 = (x1 * 3038) >> 16;
	x2 = (-7357 * p) >> 16;
	p += (x1 + x2 + 3791) >> 4;

	*pressure = p;

exit:
	return status;
}

/*
 * This function sets the chip-internal oversampling. Valid values are 0..3.
 * The chip will use 2^oversampling samples for internal averaging.
 * This influences the measurement time and the accuracy; larger values
 * increase both. The datasheet gives on overview on how measurement time,
 * accuracy and noise correlate.
 */
static void bmp085_set_oversampling(struct bmp085_data *data,
						unsigned char oversampling)
{
	if (oversampling > 3)
		oversampling = 3;
	data->oversampling_setting = oversampling;
}

/*
 * Returns the currently selected oversampling. Range: 0..3
 */
static unsigned char bmp085_get_oversampling(struct bmp085_data *data)
{
	return data->oversampling_setting;
}

/* sysfs callbacks */
static ssize_t set_oversampling(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bmp085_data *data = i2c_get_clientdata(client);
	unsigned long oversampling;
	int success = strict_strtoul(buf, 10, &oversampling);
	if (success == 0) {
		bmp085_set_oversampling(data, oversampling);
		return count;
	}
	return success;
}

static ssize_t show_oversampling(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bmp085_data *data = i2c_get_clientdata(client);
	return sprintf(buf, "%u\n", bmp085_get_oversampling(data));
}
static DEVICE_ATTR(oversampling, S_IWUSR | S_IRUGO,
					show_oversampling, set_oversampling);


static ssize_t show_temperature(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int temperature;
	int status;
	struct i2c_client *client = to_i2c_client(dev);
	struct bmp085_data *data = i2c_get_clientdata(client);

	status = bmp085_get_temperature(data, &temperature);
	if (status != 0)
		return status;
	else
		return sprintf(buf, "%d\n", temperature);
}
static DEVICE_ATTR(temp0_input, S_IRUGO, show_temperature, NULL);


static ssize_t show_pressure(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	int pressure;
	int status;
	struct i2c_client *client = to_i2c_client(dev);
	struct bmp085_data *data = i2c_get_clientdata(client);

	status = bmp085_get_pressure(data, &pressure);
	if (status != 0)
		return status;
	else
		return sprintf(buf, "%d\n", pressure);
}
static DEVICE_ATTR(pressure0_input, S_IRUGO, show_pressure, NULL);


static struct attribute *bmp085_attributes[] = {
	&dev_attr_temp0_input.attr,
	&dev_attr_pressure0_input.attr,
	&dev_attr_oversampling.attr,
	NULL
};

static const struct attribute_group bmp085_attr_group = {
	.attrs = bmp085_attributes,
};

static int bmp085_detect(struct i2c_client *client, struct i2c_board_info *info)
{
	if (client->addr != BMP085_I2C_ADDRESS)
		return -ENODEV;

	if (i2c_smbus_read_byte_data(client, BMP085_CHIP_ID_REG) != BMP085_CHIP_ID)
		return -ENODEV;

	return 0;
}

static int bmp085_init_client(struct i2c_client *client)
{
	unsigned char version;
	int status;
	struct bmp085_data *data = i2c_get_clientdata(client);
	data->client = client;
	status = bmp085_read_calibration_data(client);
	if (status != 0)
		goto exit;
	version = i2c_smbus_read_byte_data(client, BMP085_VERSION_REG);
	data->last_temp_measurement = 0;
	data->oversampling_setting = 3;
	mutex_init(&data->lock);
	dev_info(&data->client->dev, "BMP085 ver. %d.%d found.\n",
			(version & 0x0F), (version & 0xF0) >> 4);
exit:
	return status;
}

static int bmp085_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct bmp085_data *data;
	int err = 0;

	data = kzalloc(sizeof(struct bmp085_data), GFP_KERNEL);
	if (!data) {
		err = -ENOMEM;
		goto exit;
	}

	/* default settings after POR */
	data->oversampling_setting = 0x00;

	i2c_set_clientdata(client, data);

	/* Initialize the BMP085 chip */
	err = bmp085_init_client(client);
	if (err != 0)
		goto exit_free;

	/* Register sysfs hooks */
	err = sysfs_create_group(&client->dev.kobj, &bmp085_attr_group);
	if (err)
		goto exit_free;

	dev_info(&data->client->dev, "Succesfully initialized bmp085!\n");
	goto exit;

exit_free:
	kfree(data);
exit:
	return err;
}

static int bmp085_remove(struct i2c_client *client)
{
	sysfs_remove_group(&client->dev.kobj, &bmp085_attr_group);
	kfree(i2c_get_clientdata(client));
	return 0;
}

static const struct i2c_device_id bmp085_id[] = {
	{ "bmp085", 0 },
	{ }
};

static struct i2c_driver bmp085_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name	= "bmp085"
	},
	.id_table	= bmp085_id,
	.probe		= bmp085_probe,
	.remove		= bmp085_remove,

	.detect		= bmp085_detect,
	.address_list	= normal_i2c
};

static int __init bmp085_init(void)
{
	return i2c_add_driver(&bmp085_driver);
}

static void __exit bmp085_exit(void)
{
	i2c_del_driver(&bmp085_driver);
}


MODULE_AUTHOR("Christoph Mair <christoph.mair@gmail.com");
MODULE_DESCRIPTION("BMP085 driver");
MODULE_LICENSE("GPL");

module_init(bmp085_init);
module_exit(bmp085_exit);
