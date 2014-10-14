/*
 * arch/arm/mach-netx/nxdb500.c
 *
 * Copyright (c) 2005 Sascha Hauer <s.hauer@pengutronix.de>, Pengutronix
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/dma-mapping.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/mtd/plat-ram.h>
#include <linux/platform_device.h>
#include <linux/amba/bus.h>
#include <linux/amba/clcd.h>

#include <mach/hardware.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <mach/netx-regs.h>
#include <mach/eth.h>

#include "generic.h"
#include "fb.h"

static struct clcd_panel qvga = {
	.mode		= {
		.name		= "QVGA",
		.refresh	= 60,
		.xres		= 240,
		.yres		= 320,
		.pixclock	= 187617,
		.left_margin	= 6,
		.right_margin	= 26,
		.upper_margin	= 0,
		.lower_margin	= 6,
		.hsync_len	= 6,
		.vsync_len	= 1,
		.sync		= 0,
		.vmode		= FB_VMODE_NONINTERLACED,
	},
	.width		= -1,
	.height		= -1,
	.tim2		=  16,
	.cntl		= CNTL_LCDTFT | CNTL_BGR,
	.bpp		= 16,
	.grayscale	= 0,
};

static inline int nxdb500_check(struct clcd_fb *fb, struct fb_var_screeninfo *var)
{
	var->green.length = 5;
	var->green.msb_right = 0;

	return clcdfb_check(fb, var);
}

static int nxdb500_clcd_setup(struct clcd_fb *fb)
{
	unsigned int val;

	fb->fb.var.green.length = 5;
	fb->fb.var.green.msb_right = 0;

	/* enable asic control */
	val = readl(NETX_SYSTEM_IOC_ACCESS_KEY);
	writel(val, NETX_SYSTEM_IOC_ACCESS_KEY);

	writel(3, NETX_SYSTEM_IOC_CR);

	val = readl(NETX_PIO_OUTPIO);
	writel(val | 1, NETX_PIO_OUTPIO);

	val = readl(NETX_PIO_OEPIO);
	writel(val | 1, NETX_PIO_OEPIO);
	return netx_clcd_setup(fb);
}

static struct clcd_board clcd_data = {
	.name		= "netX",
	.check		= nxdb500_check,
	.decode		= clcdfb_decode,
	.enable		= netx_clcd_enable,
	.setup		= nxdb500_clcd_setup,
	.mmap		= netx_clcd_mmap,
	.remove		= netx_clcd_remove,
};

static struct netxeth_platform_data eth0_platform_data = {
	.xcno = 0,
};

static struct platform_device netx_eth0_device = {
	.name		= "netx-eth",
	.id		= 0,
	.num_resources	= 0,
	.resource	= NULL,
	.dev = {
		.platform_data = &eth0_platform_data,
	}
};

static struct netxeth_platform_data eth1_platform_data = {
	.xcno = 1,
};

static struct platform_device netx_eth1_device = {
	.name		= "netx-eth",
	.id		= 1,
	.num_resources	= 0,
	.resource	= NULL,
	.dev = {
		.platform_data = &eth1_platform_data,
	}
};

static struct resource netx_uart0_resources[] = {
	[0] = {
		.start	= 0x00100A00,
		.end	= 0x00100A3F,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= (NETX_IRQ_UART0),
		.end	= (NETX_IRQ_UART0),
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device netx_uart0_device = {
	.name		= "netx-uart",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(netx_uart0_resources),
	.resource	= netx_uart0_resources,
};

static struct resource netx_uart1_resources[] = {
	[0] = {
		.start	= 0x00100A40,
		.end	= 0x00100A7F,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= (NETX_IRQ_UART1),
		.end	= (NETX_IRQ_UART1),
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device netx_uart1_device = {
	.name		= "netx-uart",
	.id		= 1,
	.num_resources	= ARRAY_SIZE(netx_uart1_resources),
	.resource	= netx_uart1_resources,
};

static struct resource netx_uart2_resources[] = {
	[0] = {
		.start	= 0x00100A80,
		.end	= 0x00100ABF,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= (NETX_IRQ_UART2),
		.end	= (NETX_IRQ_UART2),
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device netx_uart2_device = {
	.name		= "netx-uart",
	.id		= 2,
	.num_resources	= ARRAY_SIZE(netx_uart2_resources),
	.resource	= netx_uart2_resources,
};

static struct platform_device *devices[] __initdata = {
	&netx_eth0_device,
	&netx_eth1_device,
	&netx_uart0_device,
	&netx_uart1_device,
	&netx_uart2_device,
};

static void __init nxdb500_init(void)
{
	netx_fb_init(&clcd_data, &qvga);
	platform_add_devices(devices, ARRAY_SIZE(devices));
}

MACHINE_START(NXDB500, "Hilscher nxdb500")
	.boot_params	= 0x80000100,
	.map_io		= netx_map_io,
	.init_irq	= netx_init_irq,
	.timer		= &netx_timer,
	.init_machine	= nxdb500_init,
MACHINE_END
