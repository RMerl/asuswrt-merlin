/*
 * linux/arch/arm/mach-s5pv310/setup-i2c1.c
 *
 * Copyright (C) 2010 Samsung Electronics Co., Ltd.
 *
 * I2C1 GPIO configuration.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

struct platform_device; /* don't need the contents */

#include <linux/gpio.h>
#include <plat/iic.h>
#include <plat/gpio-cfg.h>

void s3c_i2c1_cfg_gpio(struct platform_device *dev)
{
	s3c_gpio_cfgpin(S5PV310_GPD1(2), S3C_GPIO_SFN(2));
	s3c_gpio_setpull(S5PV310_GPD1(2), S3C_GPIO_PULL_UP);
	s3c_gpio_cfgpin(S5PV310_GPD1(3), S3C_GPIO_SFN(2));
	s3c_gpio_setpull(S5PV310_GPD1(3), S3C_GPIO_PULL_UP);
}
