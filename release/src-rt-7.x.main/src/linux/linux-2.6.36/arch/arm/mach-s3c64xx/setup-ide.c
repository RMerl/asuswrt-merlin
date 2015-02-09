/* linux/arch/arm/mach-s3c64xx/setup-ide.c
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * S3C64XX setup information for IDE
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/io.h>

#include <mach/map.h>
#include <mach/regs-clock.h>
#include <plat/gpio-cfg.h>

void s3c64xx_ide_setup_gpio(void)
{
	u32 reg;
	u32 gpio = 0;

	reg = readl(S3C_MEM_SYS_CFG) & (~0x3f);

	/* Independent CF interface, CF chip select configuration */
	writel(reg | MEM_SYS_CFG_INDEP_CF |
		MEM_SYS_CFG_EBI_FIX_PRI_CFCON, S3C_MEM_SYS_CFG);

	s3c_gpio_cfgpin(S3C64XX_GPB(4), S3C_GPIO_SFN(4));

	/* Set XhiDATA[15:0] pins as CF Data[15:0] */
	for (gpio = S3C64XX_GPK(0); gpio <= S3C64XX_GPK(15); gpio++)
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(5));

	/* Set XhiADDR[2:0] pins as CF ADDR[2:0] */
	for (gpio = S3C64XX_GPL(0); gpio <= S3C64XX_GPL(2); gpio++)
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(6));

	/* Set Xhi ctrl pins as CF ctrl pins(IORDY, IOWR, IORD, CE[0:1]) */
	s3c_gpio_cfgpin(S3C64XX_GPM(5), S3C_GPIO_SFN(1));
	for (gpio = S3C64XX_GPM(0); gpio <= S3C64XX_GPM(4); gpio++)
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(6));
}
