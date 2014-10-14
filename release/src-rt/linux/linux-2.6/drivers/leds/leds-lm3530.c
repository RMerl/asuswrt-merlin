/*
 * Copyright (C) 2011 ST-Ericsson SA.
 * Copyright (C) 2009 Motorola, Inc.
 *
 * License Terms: GNU General Public License v2
 *
 * Simple driver for National Semiconductor LM3530 Backlight driver chip
 *
 * Author: Shreshtha Kumar SAHU <shreshthakumar.sahu@stericsson.com>
 * based on leds-lm3530.c by Dan Murphy <D.Murphy@motorola.com>
 */

#include <linux/i2c.h>
#include <linux/leds.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/led-lm3530.h>
#include <linux/types.h>

#define LM3530_LED_DEV "lcd-backlight"
#define LM3530_NAME "lm3530-led"

#define LM3530_GEN_CONFIG		0x10
#define LM3530_ALS_CONFIG		0x20
#define LM3530_BRT_RAMP_RATE		0x30
#define LM3530_ALS_ZONE_REG		0x40
#define LM3530_ALS_IMP_SELECT		0x41
#define LM3530_BRT_CTRL_REG		0xA0
#define LM3530_ALS_ZB0_REG		0x60
#define LM3530_ALS_ZB1_REG		0x61
#define LM3530_ALS_ZB2_REG		0x62
#define LM3530_ALS_ZB3_REG		0x63
#define LM3530_ALS_Z0T_REG		0x70
#define LM3530_ALS_Z1T_REG		0x71
#define LM3530_ALS_Z2T_REG		0x72
#define LM3530_ALS_Z3T_REG		0x73
#define LM3530_ALS_Z4T_REG		0x74
#define LM3530_REG_MAX			15

/* General Control Register */
#define LM3530_EN_I2C_SHIFT		(0)
#define LM3530_RAMP_LAW_SHIFT		(1)
#define LM3530_MAX_CURR_SHIFT		(2)
#define LM3530_EN_PWM_SHIFT		(5)
#define LM3530_PWM_POL_SHIFT		(6)
#define LM3530_EN_PWM_SIMPLE_SHIFT	(7)

#define LM3530_ENABLE_I2C		(1 << LM3530_EN_I2C_SHIFT)
#define LM3530_ENABLE_PWM		(1 << LM3530_EN_PWM_SHIFT)
#define LM3530_POL_LOW			(1 << LM3530_PWM_POL_SHIFT)
#define LM3530_ENABLE_PWM_SIMPLE	(1 << LM3530_EN_PWM_SIMPLE_SHIFT)

/* ALS Config Register Options */
#define LM3530_ALS_AVG_TIME_SHIFT	(0)
#define LM3530_EN_ALS_SHIFT		(3)
#define LM3530_ALS_SEL_SHIFT		(5)

#define LM3530_ENABLE_ALS		(3 << LM3530_EN_ALS_SHIFT)

/* Brightness Ramp Rate Register */
#define LM3530_BRT_RAMP_FALL_SHIFT	(0)
#define LM3530_BRT_RAMP_RISE_SHIFT	(3)

/* ALS Resistor Select */
#define LM3530_ALS1_IMP_SHIFT		(0)
#define LM3530_ALS2_IMP_SHIFT		(4)

/* Zone Boundary Register defaults */
#define LM3530_DEF_ZB_0			(0x33)
#define LM3530_DEF_ZB_1			(0x66)
#define LM3530_DEF_ZB_2			(0x99)
#define LM3530_DEF_ZB_3			(0xCC)

/* Zone Target Register defaults */
#define LM3530_DEF_ZT_0			(0x19)
#define LM3530_DEF_ZT_1			(0x33)
#define LM3530_DEF_ZT_2			(0x4C)
#define LM3530_DEF_ZT_3			(0x66)
#define LM3530_DEF_ZT_4			(0x7F)

struct lm3530_mode_map {
	const char *mode;
	enum lm3530_mode mode_val;
};

static struct lm3530_mode_map mode_map[] = {
	{ "man", LM3530_BL_MODE_MANUAL },
	{ "als", LM3530_BL_MODE_ALS },
	{ "pwm", LM3530_BL_MODE_PWM },
};

/**
 * struct lm3530_data
 * @led_dev: led class device
 * @client: i2c client
 * @pdata: LM3530 platform data
 * @mode: mode of operation - manual, ALS, PWM
 */
struct lm3530_data {
	struct led_classdev led_dev;
	struct i2c_client *client;
	struct lm3530_platform_data *pdata;
	enum lm3530_mode mode;
};

static const u8 lm3530_reg[LM3530_REG_MAX] = {
	LM3530_GEN_CONFIG,
	LM3530_ALS_CONFIG,
	LM3530_BRT_RAMP_RATE,
	LM3530_ALS_ZONE_REG,
	LM3530_ALS_IMP_SELECT,
	LM3530_BRT_CTRL_REG,
	LM3530_ALS_ZB0_REG,
	LM3530_ALS_ZB1_REG,
	LM3530_ALS_ZB2_REG,
	LM3530_ALS_ZB3_REG,
	LM3530_ALS_Z0T_REG,
	LM3530_ALS_Z1T_REG,
	LM3530_ALS_Z2T_REG,
	LM3530_ALS_Z3T_REG,
	LM3530_ALS_Z4T_REG,
};

static int lm3530_get_mode_from_str(const char *str)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mode_map); i++)
		if (sysfs_streq(str, mode_map[i].mode))
			return mode_map[i].mode_val;

	return -1;
}

static int lm3530_init_registers(struct lm3530_data *drvdata)
{
	int ret = 0;
	int i;
	u8 gen_config;
	u8 als_config = 0;
	u8 brt_ramp;
	u8 als_imp_sel = 0;
	u8 brightness;
	u8 reg_val[LM3530_REG_MAX];
	struct lm3530_platform_data *pltfm = drvdata->pdata;
	struct i2c_client *client = drvdata->client;

	gen_config = (pltfm->brt_ramp_law << LM3530_RAMP_LAW_SHIFT) |
			((pltfm->max_current & 7) << LM3530_MAX_CURR_SHIFT);

	if (drvdata->mode == LM3530_BL_MODE_MANUAL ||
	    drvdata->mode == LM3530_BL_MODE_ALS)
		gen_config |= (LM3530_ENABLE_I2C);

	if (drvdata->mode == LM3530_BL_MODE_ALS) {
		als_config =
			(pltfm->als_avrg_time << LM3530_ALS_AVG_TIME_SHIFT) |
			(LM3530_ENABLE_ALS) |
			(pltfm->als_input_mode << LM3530_ALS_SEL_SHIFT);

		als_imp_sel =
			(pltfm->als1_resistor_sel << LM3530_ALS1_IMP_SHIFT) |
			(pltfm->als2_resistor_sel << LM3530_ALS2_IMP_SHIFT);
	}

	if (drvdata->mode == LM3530_BL_MODE_PWM)
		gen_config |= (LM3530_ENABLE_PWM) |
				(pltfm->pwm_pol_hi << LM3530_PWM_POL_SHIFT) |
				(LM3530_ENABLE_PWM_SIMPLE);

	brt_ramp = (pltfm->brt_ramp_fall << LM3530_BRT_RAMP_FALL_SHIFT) |
			(pltfm->brt_ramp_rise << LM3530_BRT_RAMP_RISE_SHIFT);

	brightness = pltfm->brt_val;

	reg_val[0] = gen_config;	/* LM3530_GEN_CONFIG */
	reg_val[1] = als_config;	/* LM3530_ALS_CONFIG */
	reg_val[2] = brt_ramp;		/* LM3530_BRT_RAMP_RATE */
	reg_val[3] = 0x00;		/* LM3530_ALS_ZONE_REG */
	reg_val[4] = als_imp_sel;	/* LM3530_ALS_IMP_SELECT */
	reg_val[5] = brightness;	/* LM3530_BRT_CTRL_REG */
	reg_val[6] = LM3530_DEF_ZB_0;	/* LM3530_ALS_ZB0_REG */
	reg_val[7] = LM3530_DEF_ZB_1;	/* LM3530_ALS_ZB1_REG */
	reg_val[8] = LM3530_DEF_ZB_2;	/* LM3530_ALS_ZB2_REG */
	reg_val[9] = LM3530_DEF_ZB_3;	/* LM3530_ALS_ZB3_REG */
	reg_val[10] = LM3530_DEF_ZT_0;	/* LM3530_ALS_Z0T_REG */
	reg_val[11] = LM3530_DEF_ZT_1;	/* LM3530_ALS_Z1T_REG */
	reg_val[12] = LM3530_DEF_ZT_2;	/* LM3530_ALS_Z2T_REG */
	reg_val[13] = LM3530_DEF_ZT_3;	/* LM3530_ALS_Z3T_REG */
	reg_val[14] = LM3530_DEF_ZT_4;	/* LM3530_ALS_Z4T_REG */

	for (i = 0; i < LM3530_REG_MAX; i++) {
		ret = i2c_smbus_write_byte_data(client,
				lm3530_reg[i], reg_val[i]);
		if (ret)
			break;
	}

	return ret;
}

static void lm3530_brightness_set(struct led_classdev *led_cdev,
				     enum led_brightness brt_val)
{
	int err;
	struct lm3530_data *drvdata =
	    container_of(led_cdev, struct lm3530_data, led_dev);

	switch (drvdata->mode) {
	case LM3530_BL_MODE_MANUAL:

		/* set the brightness in brightness control register*/
		err = i2c_smbus_write_byte_data(drvdata->client,
				LM3530_BRT_CTRL_REG, brt_val / 2);
		if (err)
			dev_err(&drvdata->client->dev,
				"Unable to set brightness: %d\n", err);
		break;
	case LM3530_BL_MODE_ALS:
		break;
	case LM3530_BL_MODE_PWM:
		break;
	default:
		break;
	}
}


static ssize_t lm3530_mode_set(struct device *dev, struct device_attribute
				   *attr, const char *buf, size_t size)
{
	int err;
	struct i2c_client *client = container_of(
					dev->parent, struct i2c_client, dev);
	struct lm3530_data *drvdata = i2c_get_clientdata(client);
	int mode;

	mode = lm3530_get_mode_from_str(buf);
	if (mode < 0) {
		dev_err(dev, "Invalid mode\n");
		return -EINVAL;
	}

	if (mode == LM3530_BL_MODE_MANUAL)
		drvdata->mode = LM3530_BL_MODE_MANUAL;
	else if (mode == LM3530_BL_MODE_ALS)
		drvdata->mode = LM3530_BL_MODE_ALS;
	else if (mode == LM3530_BL_MODE_PWM) {
		dev_err(dev, "PWM mode not supported\n");
		return -EINVAL;
	}

	err = lm3530_init_registers(drvdata);
	if (err) {
		dev_err(dev, "Setting %s Mode failed :%d\n", buf, err);
		return err;
	}

	return sizeof(drvdata->mode);
}

static DEVICE_ATTR(mode, 0644, NULL, lm3530_mode_set);

static int __devinit lm3530_probe(struct i2c_client *client,
			   const struct i2c_device_id *id)
{
	struct lm3530_platform_data *pdata = client->dev.platform_data;
	struct lm3530_data *drvdata;
	int err = 0;

	if (pdata == NULL) {
		dev_err(&client->dev, "platform data required\n");
		err = -ENODEV;
		goto err_out;
	}

	/* BL mode */
	if (pdata->mode > LM3530_BL_MODE_PWM) {
		dev_err(&client->dev, "Illegal Mode request\n");
		err = -EINVAL;
		goto err_out;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "I2C_FUNC_I2C not supported\n");
		err = -EIO;
		goto err_out;
	}

	drvdata = kzalloc(sizeof(struct lm3530_data), GFP_KERNEL);
	if (drvdata == NULL) {
		err = -ENOMEM;
		goto err_out;
	}

	drvdata->mode = pdata->mode;
	drvdata->client = client;
	drvdata->pdata = pdata;
	drvdata->led_dev.name = LM3530_LED_DEV;
	drvdata->led_dev.brightness_set = lm3530_brightness_set;

	i2c_set_clientdata(client, drvdata);

	err = lm3530_init_registers(drvdata);
	if (err < 0) {
		dev_err(&client->dev, "Register Init failed: %d\n", err);
		err = -ENODEV;
		goto err_reg_init;
	}

	err = led_classdev_register((struct device *)
				      &client->dev, &drvdata->led_dev);
	if (err < 0) {
		dev_err(&client->dev, "Register led class failed: %d\n", err);
		err = -ENODEV;
		goto err_class_register;
	}

	err = device_create_file(drvdata->led_dev.dev, &dev_attr_mode);
	if (err < 0) {
		dev_err(&client->dev, "File device creation failed: %d\n", err);
		err = -ENODEV;
		goto err_create_file;
	}

	return 0;

err_create_file:
	led_classdev_unregister(&drvdata->led_dev);
err_class_register:
err_reg_init:
	kfree(drvdata);
err_out:
	return err;
}

static int __devexit lm3530_remove(struct i2c_client *client)
{
	struct lm3530_data *drvdata = i2c_get_clientdata(client);

	device_remove_file(drvdata->led_dev.dev, &dev_attr_mode);
	led_classdev_unregister(&drvdata->led_dev);
	kfree(drvdata);
	return 0;
}

static const struct i2c_device_id lm3530_id[] = {
	{LM3530_NAME, 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, lm3530_id);

static struct i2c_driver lm3530_i2c_driver = {
	.probe = lm3530_probe,
	.remove = lm3530_remove,
	.id_table = lm3530_id,
	.driver = {
		.name = LM3530_NAME,
		.owner = THIS_MODULE,
	},
};

static int __init lm3530_init(void)
{
	return i2c_add_driver(&lm3530_i2c_driver);
}

static void __exit lm3530_exit(void)
{
	i2c_del_driver(&lm3530_i2c_driver);
}

module_init(lm3530_init);
module_exit(lm3530_exit);

MODULE_DESCRIPTION("Back Light driver for LM3530");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Shreshtha Kumar SAHU <shreshthakumar.sahu@stericsson.com>");
