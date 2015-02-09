/* linux/arch/arm/plat-s3c/include/plat/gpio-cfg.h
 *
 * Copyright 2008 Openmoko, Inc.
 * Copyright 2008 Simtec Electronics
 *	http://armlinux.simtec.co.uk/
 *	Ben Dooks <ben@simtec.co.uk>
 *
 * S3C Platform - GPIO pin configuration
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

/* This file contains the necessary definitions to get the basic gpio
 * pin configuration done such as setting a pin to input or output or
 * changing the pull-{up,down} configurations.
 */

/* Note, this interface is being added to the s3c64xx arch first and will
 * be added to the s3c24xx systems later.
 */

#ifndef __PLAT_GPIO_CFG_H
#define __PLAT_GPIO_CFG_H __FILE__

typedef unsigned int __bitwise__ s3c_gpio_pull_t;
typedef unsigned int __bitwise__ s5p_gpio_drvstr_t;

/* forward declaration if gpio-core.h hasn't been included */
struct s3c_gpio_chip;

/**
 * struct s3c_gpio_cfg GPIO configuration
 * @cfg_eint: Configuration setting when used for external interrupt source
 * @get_pull: Read the current pull configuration for the GPIO
 * @set_pull: Set the current pull configuraiton for the GPIO
 * @set_config: Set the current configuration for the GPIO
 * @get_config: Read the current configuration for the GPIO
 *
 * Each chip can have more than one type of GPIO bank available and some
 * have different capabilites even when they have the same control register
 * layouts. Provide an point to vector control routine and provide any
 * per-bank configuration information that other systems such as the
 * external interrupt code will need.
 *
 * @sa s3c_gpio_cfgpin
 * @sa s3c_gpio_getcfg
 * @sa s3c_gpio_setpull
 * @sa s3c_gpio_getpull
 */
struct s3c_gpio_cfg {
	unsigned int	cfg_eint;

	s3c_gpio_pull_t	(*get_pull)(struct s3c_gpio_chip *chip, unsigned offs);
	int		(*set_pull)(struct s3c_gpio_chip *chip, unsigned offs,
				    s3c_gpio_pull_t pull);

	unsigned (*get_config)(struct s3c_gpio_chip *chip, unsigned offs);
	int	 (*set_config)(struct s3c_gpio_chip *chip, unsigned offs,
			       unsigned config);
};

#define S3C_GPIO_SPECIAL_MARK	(0xfffffff0)
#define S3C_GPIO_SPECIAL(x)	(S3C_GPIO_SPECIAL_MARK | (x))

/* Defines for generic pin configurations */
#define S3C_GPIO_INPUT	(S3C_GPIO_SPECIAL(0))
#define S3C_GPIO_OUTPUT	(S3C_GPIO_SPECIAL(1))
#define S3C_GPIO_SFN(x)	(S3C_GPIO_SPECIAL(x))

#define s3c_gpio_is_cfg_special(_cfg) \
	(((_cfg) & S3C_GPIO_SPECIAL_MARK) == S3C_GPIO_SPECIAL_MARK)

/**
 * s3c_gpio_cfgpin() - Change the GPIO function of a pin.
 * @pin pin The pin number to configure.
 * @to to The configuration for the pin's function.
 *
 * Configure which function is actually connected to the external
 * pin, such as an gpio input, output or some form of special function
 * connected to an internal peripheral block.
 *
 * The @to parameter can be one of the generic S3C_GPIO_INPUT, S3C_GPIO_OUTPUT
 * or S3C_GPIO_SFN() to indicate one of the possible values that the helper
 * will then generate the correct bit mask and shift for the configuration.
 *
 * If a bank of GPIOs all needs to be set to special-function 2, then
 * the following code will work:
 *
 *	for (gpio = start; gpio < end; gpio++)
 *		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
 *
 * The @to parameter can also be a specific value already shifted to the
 * correct position in the control register, although these are discouraged
 * in newer kernels and are only being kept for compatibility.
 */
extern int s3c_gpio_cfgpin(unsigned int pin, unsigned int to);

/**
 * s3c_gpio_getcfg - Read the current function for a GPIO pin
 * @pin: The pin to read the configuration value for.
 *
 * Read the configuration state of the given @pin, returning a value that
 * could be passed back to s3c_gpio_cfgpin().
 *
 * @sa s3c_gpio_cfgpin
 */
extern unsigned s3c_gpio_getcfg(unsigned int pin);

/* Define values for the pull-{up,down} available for each gpio pin.
 *
 * These values control the state of the weak pull-{up,down} resistors
 * available on most pins on the S3C series. Not all chips support both
 * up or down settings, and it may be dependant on the chip that is being
 * used to whether the particular mode is available.
 */
#define S3C_GPIO_PULL_NONE	((__force s3c_gpio_pull_t)0x00)
#define S3C_GPIO_PULL_DOWN	((__force s3c_gpio_pull_t)0x01)
#define S3C_GPIO_PULL_UP	((__force s3c_gpio_pull_t)0x02)

/**
 * s3c_gpio_setpull() - set the state of a gpio pin pull resistor
 * @pin: The pin number to configure the pull resistor.
 * @pull: The configuration for the pull resistor.
 *
 * This function sets the state of the pull-{up,down} resistor for the
 * specified pin. It will return 0 if successfull, or a negative error
 * code if the pin cannot support the requested pull setting.
 *
 * @pull is one of S3C_GPIO_PULL_NONE, S3C_GPIO_PULL_DOWN or S3C_GPIO_PULL_UP.
*/
extern int s3c_gpio_setpull(unsigned int pin, s3c_gpio_pull_t pull);

/**
 * s3c_gpio_getpull() - get the pull resistor state of a gpio pin
 * @pin: The pin number to get the settings for
 *
 * Read the pull resistor value for the specified pin.
*/
extern s3c_gpio_pull_t s3c_gpio_getpull(unsigned int pin);

/* Define values for the drvstr available for each gpio pin.
 *
 * These values control the value of the output signal driver strength,
 * configurable on most pins on the S5P series.
 */
#define S5P_GPIO_DRVSTR_LV1	((__force s5p_gpio_drvstr_t)0x0)
#define S5P_GPIO_DRVSTR_LV2	((__force s5p_gpio_drvstr_t)0x2)
#define S5P_GPIO_DRVSTR_LV3	((__force s5p_gpio_drvstr_t)0x1)
#define S5P_GPIO_DRVSTR_LV4	((__force s5p_gpio_drvstr_t)0x3)

/**
 * s5c_gpio_get_drvstr() - get the driver streght value of a gpio pin
 * @pin: The pin number to get the settings for
 *
 * Read the driver streght value for the specified pin.
*/
extern s5p_gpio_drvstr_t s5p_gpio_get_drvstr(unsigned int pin);

/**
 * s3c_gpio_set_drvstr() - set the driver streght value of a gpio pin
 * @pin: The pin number to configure the driver streght value
 * @drvstr: The new value of the driver strength
 *
 * This function sets the driver strength value for the specified pin.
 * It will return 0 if successfull, or a negative error code if the pin
 * cannot support the requested setting.
*/
extern int s5p_gpio_set_drvstr(unsigned int pin, s5p_gpio_drvstr_t drvstr);

#endif /* __PLAT_GPIO_CFG_H */
