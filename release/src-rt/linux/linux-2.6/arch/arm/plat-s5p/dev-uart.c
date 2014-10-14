/* linux/arch/arm/plat-s5p/dev-uart.c
 *
 * Copyright (c) 2009 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * Base S5P UART resource and device definitions
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/platform_device.h>

#include <asm/mach/arch.h>
#include <asm/mach/irq.h>
#include <mach/hardware.h>
#include <mach/map.h>

#include <plat/devs.h>

 /* Serial port registrations */

static struct resource s5p_uart0_resource[] = {
	[0] = {
		.start	= S5P_PA_UART0,
		.end	= S5P_PA_UART0 + S5P_SZ_UART - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_S5P_UART_RX0,
		.end	= IRQ_S5P_UART_RX0,
		.flags	= IORESOURCE_IRQ,
	},
	[2] = {
		.start	= IRQ_S5P_UART_TX0,
		.end	= IRQ_S5P_UART_TX0,
		.flags	= IORESOURCE_IRQ,
	},
	[3] = {
		.start	= IRQ_S5P_UART_ERR0,
		.end	= IRQ_S5P_UART_ERR0,
		.flags	= IORESOURCE_IRQ,
	}
};

static struct resource s5p_uart1_resource[] = {
	[0] = {
		.start	= S5P_PA_UART1,
		.end	= S5P_PA_UART1 + S5P_SZ_UART - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_S5P_UART_RX1,
		.end	= IRQ_S5P_UART_RX1,
		.flags	= IORESOURCE_IRQ,
	},
	[2] = {
		.start	= IRQ_S5P_UART_TX1,
		.end	= IRQ_S5P_UART_TX1,
		.flags	= IORESOURCE_IRQ,
	},
	[3] = {
		.start	= IRQ_S5P_UART_ERR1,
		.end	= IRQ_S5P_UART_ERR1,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct resource s5p_uart2_resource[] = {
	[0] = {
		.start	= S5P_PA_UART2,
		.end	= S5P_PA_UART2 + S5P_SZ_UART - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_S5P_UART_RX2,
		.end	= IRQ_S5P_UART_RX2,
		.flags	= IORESOURCE_IRQ,
	},
	[2] = {
		.start	= IRQ_S5P_UART_TX2,
		.end	= IRQ_S5P_UART_TX2,
		.flags	= IORESOURCE_IRQ,
	},
	[3] = {
		.start	= IRQ_S5P_UART_ERR2,
		.end	= IRQ_S5P_UART_ERR2,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct resource s5p_uart3_resource[] = {
#if CONFIG_SERIAL_SAMSUNG_UARTS > 3
	[0] = {
		.start	= S5P_PA_UART3,
		.end	= S5P_PA_UART3 + S5P_SZ_UART - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_S5P_UART_RX3,
		.end	= IRQ_S5P_UART_RX3,
		.flags	= IORESOURCE_IRQ,
	},
	[2] = {
		.start	= IRQ_S5P_UART_TX3,
		.end	= IRQ_S5P_UART_TX3,
		.flags	= IORESOURCE_IRQ,
	},
	[3] = {
		.start	= IRQ_S5P_UART_ERR3,
		.end	= IRQ_S5P_UART_ERR3,
		.flags	= IORESOURCE_IRQ,
	},
#endif
};

static struct resource s5p_uart4_resource[] = {
#if CONFIG_SERIAL_SAMSUNG_UARTS > 4
	[0] = {
		.start	= S5P_PA_UART4,
		.end	= S5P_PA_UART4 + S5P_SZ_UART - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_S5P_UART_RX4,
		.end	= IRQ_S5P_UART_RX4,
		.flags	= IORESOURCE_IRQ,
	},
	[2] = {
		.start	= IRQ_S5P_UART_TX4,
		.end	= IRQ_S5P_UART_TX4,
		.flags	= IORESOURCE_IRQ,
	},
	[3] = {
		.start	= IRQ_S5P_UART_ERR4,
		.end	= IRQ_S5P_UART_ERR4,
		.flags	= IORESOURCE_IRQ,
	},
#endif
};

static struct resource s5p_uart5_resource[] = {
#if CONFIG_SERIAL_SAMSUNG_UARTS > 5
	[0] = {
		.start	= S5P_PA_UART5,
		.end	= S5P_PA_UART5 + S5P_SZ_UART - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_S5P_UART_RX5,
		.end	= IRQ_S5P_UART_RX5,
		.flags	= IORESOURCE_IRQ,
	},
	[2] = {
		.start	= IRQ_S5P_UART_TX5,
		.end	= IRQ_S5P_UART_TX5,
		.flags	= IORESOURCE_IRQ,
	},
	[3] = {
		.start	= IRQ_S5P_UART_ERR5,
		.end	= IRQ_S5P_UART_ERR5,
		.flags	= IORESOURCE_IRQ,
	},
#endif
};

struct s3c24xx_uart_resources s5p_uart_resources[] __initdata = {
	[0] = {
		.resources	= s5p_uart0_resource,
		.nr_resources	= ARRAY_SIZE(s5p_uart0_resource),
	},
	[1] = {
		.resources	= s5p_uart1_resource,
		.nr_resources	= ARRAY_SIZE(s5p_uart1_resource),
	},
	[2] = {
		.resources	= s5p_uart2_resource,
		.nr_resources	= ARRAY_SIZE(s5p_uart2_resource),
	},
	[3] = {
		.resources	= s5p_uart3_resource,
		.nr_resources	= ARRAY_SIZE(s5p_uart3_resource),
	},
	[4] = {
		.resources	= s5p_uart4_resource,
		.nr_resources	= ARRAY_SIZE(s5p_uart4_resource),
	},
	[5] = {
		.resources	= s5p_uart5_resource,
		.nr_resources	= ARRAY_SIZE(s5p_uart5_resource),
	},
};
