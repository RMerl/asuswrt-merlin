/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * Author: Rabin Vincent <rabin.vincent@stericsson.com> for ST-Ericsson
 * License terms: GNU General Public License (GPL) version 2
 */

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/amba/bus.h>

#include <plat/ste_dma40.h>

#include <mach/hardware.h>
#include <mach/setup.h>

#include "ste-dma40-db8500.h"

static struct nmk_gpio_platform_data u8500_gpio_data[] = {
	GPIO_DATA("GPIO-0-31", 0),
	GPIO_DATA("GPIO-32-63", 32), /* 37..63 not routed to pin */
	GPIO_DATA("GPIO-64-95", 64),
	GPIO_DATA("GPIO-96-127", 96), /* 98..127 not routed to pin */
	GPIO_DATA("GPIO-128-159", 128),
	GPIO_DATA("GPIO-160-191", 160), /* 172..191 not routed to pin */
	GPIO_DATA("GPIO-192-223", 192),
	GPIO_DATA("GPIO-224-255", 224), /* 231..255 not routed to pin */
	GPIO_DATA("GPIO-256-288", 256), /* 268..288 not routed to pin */
};

static struct resource u8500_gpio_resources[] = {
	GPIO_RESOURCE(0),
	GPIO_RESOURCE(1),
	GPIO_RESOURCE(2),
	GPIO_RESOURCE(3),
	GPIO_RESOURCE(4),
	GPIO_RESOURCE(5),
	GPIO_RESOURCE(6),
	GPIO_RESOURCE(7),
	GPIO_RESOURCE(8),
};

struct platform_device u8500_gpio_devs[] = {
	GPIO_DEVICE(0),
	GPIO_DEVICE(1),
	GPIO_DEVICE(2),
	GPIO_DEVICE(3),
	GPIO_DEVICE(4),
	GPIO_DEVICE(5),
	GPIO_DEVICE(6),
	GPIO_DEVICE(7),
	GPIO_DEVICE(8),
};

struct amba_device u8500_ssp0_device = {
	.dev = {
		.coherent_dma_mask = ~0,
		.init_name = "ssp0",
	},
	.res = {
		.start = U8500_SSP0_BASE,
		.end   = U8500_SSP0_BASE + SZ_4K - 1,
		.flags = IORESOURCE_MEM,
	},
	.irq = {IRQ_DB8500_SSP0, NO_IRQ },
	/* ST-Ericsson modified id */
	.periphid = SSP_PER_ID,
};

static struct resource u8500_i2c0_resources[] = {
	[0] = {
		.start	= U8500_I2C0_BASE,
		.end	= U8500_I2C0_BASE + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_DB8500_I2C0,
		.end	= IRQ_DB8500_I2C0,
		.flags	= IORESOURCE_IRQ,
	}
};

struct platform_device u8500_i2c0_device = {
	.name		= "nmk-i2c",
	.id		= 0,
	.resource	= u8500_i2c0_resources,
	.num_resources	= ARRAY_SIZE(u8500_i2c0_resources),
};

static struct resource u8500_i2c4_resources[] = {
	[0] = {
		.start	= U8500_I2C4_BASE,
		.end	= U8500_I2C4_BASE + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_DB8500_I2C4,
		.end	= IRQ_DB8500_I2C4,
		.flags	= IORESOURCE_IRQ,
	}
};

struct platform_device u8500_i2c4_device = {
	.name		= "nmk-i2c",
	.id		= 4,
	.resource	= u8500_i2c4_resources,
	.num_resources	= ARRAY_SIZE(u8500_i2c4_resources),
};

static struct resource dma40_resources[] = {
	[0] = {
		.start = U8500_DMA_BASE,
		.end   = U8500_DMA_BASE + SZ_4K - 1,
		.flags = IORESOURCE_MEM,
		.name  = "base",
	},
	[1] = {
		.start = U8500_DMA_LCPA_BASE,
		.end   = U8500_DMA_LCPA_BASE + 2 * SZ_1K - 1,
		.flags = IORESOURCE_MEM,
		.name  = "lcpa",
	},
	[2] = {
		.start = IRQ_DB8500_DMA,
		.end   = IRQ_DB8500_DMA,
		.flags = IORESOURCE_IRQ,
	}
};

/* Default configuration for physcial memcpy */
struct stedma40_chan_cfg dma40_memcpy_conf_phy = {
	.channel_type = (STEDMA40_CHANNEL_IN_PHY_MODE |
			 STEDMA40_LOW_PRIORITY_CHANNEL |
			 STEDMA40_PCHAN_BASIC_MODE),
	.dir = STEDMA40_MEM_TO_MEM,

	.src_info.endianess = STEDMA40_LITTLE_ENDIAN,
	.src_info.data_width = STEDMA40_BYTE_WIDTH,
	.src_info.psize = STEDMA40_PSIZE_PHY_1,
	.src_info.flow_ctrl = STEDMA40_NO_FLOW_CTRL,

	.dst_info.endianess = STEDMA40_LITTLE_ENDIAN,
	.dst_info.data_width = STEDMA40_BYTE_WIDTH,
	.dst_info.psize = STEDMA40_PSIZE_PHY_1,
	.dst_info.flow_ctrl = STEDMA40_NO_FLOW_CTRL,
};
/* Default configuration for logical memcpy */
struct stedma40_chan_cfg dma40_memcpy_conf_log = {
	.channel_type = (STEDMA40_CHANNEL_IN_LOG_MODE |
			 STEDMA40_LOW_PRIORITY_CHANNEL |
			 STEDMA40_LCHAN_SRC_LOG_DST_LOG |
			 STEDMA40_NO_TIM_FOR_LINK),
	.dir = STEDMA40_MEM_TO_MEM,

	.src_info.endianess = STEDMA40_LITTLE_ENDIAN,
	.src_info.data_width = STEDMA40_BYTE_WIDTH,
	.src_info.psize = STEDMA40_PSIZE_LOG_1,
	.src_info.flow_ctrl = STEDMA40_NO_FLOW_CTRL,

	.dst_info.endianess = STEDMA40_LITTLE_ENDIAN,
	.dst_info.data_width = STEDMA40_BYTE_WIDTH,
	.dst_info.psize = STEDMA40_PSIZE_LOG_1,
	.dst_info.flow_ctrl = STEDMA40_NO_FLOW_CTRL,
};

/*
 * Mapping between destination event lines and physical device address.
 * The event line is tied to a device and therefor the address is constant.
 */
static const dma_addr_t dma40_tx_map[STEDMA40_NR_DEV];

/* Mapping between source event lines and physical device address */
static const dma_addr_t dma40_rx_map[STEDMA40_NR_DEV];

/* Reserved event lines for memcpy only */
static int dma40_memcpy_event[] = {
	STEDMA40_MEMCPY_TX_0,
	STEDMA40_MEMCPY_TX_1,
	STEDMA40_MEMCPY_TX_2,
	STEDMA40_MEMCPY_TX_3,
	STEDMA40_MEMCPY_TX_4,
	STEDMA40_MEMCPY_TX_5,
};

static struct stedma40_platform_data dma40_plat_data = {
	.dev_len = STEDMA40_NR_DEV,
	.dev_rx = dma40_rx_map,
	.dev_tx = dma40_tx_map,
	.memcpy = dma40_memcpy_event,
	.memcpy_len = ARRAY_SIZE(dma40_memcpy_event),
	.memcpy_conf_phy = &dma40_memcpy_conf_phy,
	.memcpy_conf_log = &dma40_memcpy_conf_log,
	.llis_per_log = 8,
	.disabled_channels = {-1},
};

struct platform_device u8500_dma40_device = {
	.dev = {
		.platform_data = &dma40_plat_data,
	},
	.name = "dma40",
	.id = 0,
	.num_resources = ARRAY_SIZE(dma40_resources),
	.resource = dma40_resources
};

void dma40_u8500ed_fixup(void)
{
	dma40_plat_data.memcpy = NULL;
	dma40_plat_data.memcpy_len = 0;
	dma40_resources[0].start = U8500_DMA_BASE_ED;
	dma40_resources[0].end = U8500_DMA_BASE_ED + SZ_4K - 1;
	dma40_resources[1].start = U8500_DMA_LCPA_BASE_ED;
	dma40_resources[1].end = U8500_DMA_LCPA_BASE_ED + 2 * SZ_1K - 1;
}
