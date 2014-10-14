/* linux/arch/arm/plat-s5pc100/s5pc100-init.c
 *
 * Copyright 2009 Samsung Electronics Co.
 *      Byungho Min <bhmin@samsung.com>
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/init.h>

#include <plat/cpu.h>
#include <plat/devs.h>
#include <plat/s5pc100.h>

/* uart registration process */
void __init s5pc100_common_init_uarts(struct s3c2410_uartcfg *cfg, int no)
{
	s3c24xx_init_uartdevs("s3c6400-uart", s5p_uart_resources, cfg, no);
}
