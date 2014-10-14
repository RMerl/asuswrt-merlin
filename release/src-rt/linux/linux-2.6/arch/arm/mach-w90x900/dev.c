/*
 * linux/arch/arm/mach-w90x900/dev.c
 *
 * Copyright (C) 2009 Nuvoton corporation.
 *
 * Wan ZongShun <mcuos.com@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation;version 2 of the License.
 *
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/timer.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include <linux/mtd/physmap.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>

#include <linux/spi/spi.h>
#include <linux/spi/flash.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/irq.h>
#include <asm/mach-types.h>

#include <mach/regs-serial.h>
#include <mach/nuc900_spi.h>
#include <mach/map.h>
#include <mach/fb.h>
#include <mach/regs-ldm.h>
#include <mach/w90p910_keypad.h>

#include "cpu.h"

/*NUC900 evb norflash driver data */

#define NUC900_FLASH_BASE	0xA0000000
#define NUC900_FLASH_SIZE	0x400000
#define SPIOFFSET		0x200
#define SPIOREG_SIZE		0x100

static struct mtd_partition nuc900_flash_partitions[] = {
	{
		.name	=	"NOR Partition 1 for kernel (960K)",
		.size	=	0xF0000,
		.offset	=	0x10000,
	},
	{
		.name	=	"NOR Partition 2 for image (1M)",
		.size	=	0x100000,
		.offset	=	0x100000,
	},
	{
		.name	=	"NOR Partition 3 for user (2M)",
		.size	=	0x200000,
		.offset	=	0x00200000,
	}
};

static struct physmap_flash_data nuc900_flash_data = {
	.width		=	2,
	.parts		=	nuc900_flash_partitions,
	.nr_parts	=	ARRAY_SIZE(nuc900_flash_partitions),
};

static struct resource nuc900_flash_resources[] = {
	{
		.start	=	NUC900_FLASH_BASE,
		.end	=	NUC900_FLASH_BASE + NUC900_FLASH_SIZE - 1,
		.flags	=	IORESOURCE_MEM,
	}
};

static struct platform_device nuc900_flash_device = {
	.name		=	"physmap-flash",
	.id		=	0,
	.dev		= {
				.platform_data = &nuc900_flash_data,
			},
	.resource	=	nuc900_flash_resources,
	.num_resources	=	ARRAY_SIZE(nuc900_flash_resources),
};

/* USB EHCI Host Controller */

static struct resource nuc900_usb_ehci_resource[] = {
	[0] = {
		.start = W90X900_PA_USBEHCIHOST,
		.end   = W90X900_PA_USBEHCIHOST + W90X900_SZ_USBEHCIHOST - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_USBH,
		.end   = IRQ_USBH,
		.flags = IORESOURCE_IRQ,
	}
};

static u64 nuc900_device_usb_ehci_dmamask = 0xffffffffUL;

static struct platform_device nuc900_device_usb_ehci = {
	.name		  = "nuc900-ehci",
	.id		  = -1,
	.num_resources	  = ARRAY_SIZE(nuc900_usb_ehci_resource),
	.resource	  = nuc900_usb_ehci_resource,
	.dev              = {
		.dma_mask = &nuc900_device_usb_ehci_dmamask,
		.coherent_dma_mask = 0xffffffffUL
	}
};

/* USB OHCI Host Controller */

static struct resource nuc900_usb_ohci_resource[] = {
	[0] = {
		.start = W90X900_PA_USBOHCIHOST,
		.end   = W90X900_PA_USBOHCIHOST + W90X900_SZ_USBOHCIHOST - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_USBH,
		.end   = IRQ_USBH,
		.flags = IORESOURCE_IRQ,
	}
};

static u64 nuc900_device_usb_ohci_dmamask = 0xffffffffUL;
static struct platform_device nuc900_device_usb_ohci = {
	.name		  = "nuc900-ohci",
	.id		  = -1,
	.num_resources	  = ARRAY_SIZE(nuc900_usb_ohci_resource),
	.resource	  = nuc900_usb_ohci_resource,
	.dev              = {
		.dma_mask = &nuc900_device_usb_ohci_dmamask,
		.coherent_dma_mask = 0xffffffffUL
	}
};

/* USB Device (Gadget)*/

static struct resource nuc900_usbgadget_resource[] = {
	[0] = {
		.start = W90X900_PA_USBDEV,
		.end   = W90X900_PA_USBDEV + W90X900_SZ_USBDEV - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_USBD,
		.end   = IRQ_USBD,
		.flags = IORESOURCE_IRQ,
	}
};

static struct platform_device nuc900_device_usbgadget = {
	.name		= "nuc900-usbgadget",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(nuc900_usbgadget_resource),
	.resource	= nuc900_usbgadget_resource,
};

/* MAC device */

static struct resource nuc900_emc_resource[] = {
	[0] = {
		.start = W90X900_PA_EMC,
		.end   = W90X900_PA_EMC + W90X900_SZ_EMC - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_EMCTX,
		.end   = IRQ_EMCTX,
		.flags = IORESOURCE_IRQ,
	},
	[2] = {
		.start = IRQ_EMCRX,
		.end   = IRQ_EMCRX,
		.flags = IORESOURCE_IRQ,
	}
};

static u64 nuc900_device_emc_dmamask = 0xffffffffUL;
static struct platform_device nuc900_device_emc = {
	.name		= "nuc900-emc",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(nuc900_emc_resource),
	.resource	= nuc900_emc_resource,
	.dev              = {
		.dma_mask = &nuc900_device_emc_dmamask,
		.coherent_dma_mask = 0xffffffffUL
	}
};

/* SPI device */

static struct nuc900_spi_info nuc900_spiflash_data = {
	.num_cs		= 1,
	.lsb		= 0,
	.txneg		= 1,
	.rxneg		= 0,
	.divider	= 24,
	.sleep		= 0,
	.txnum		= 0,
	.txbitlen	= 8,
	.bus_num	= 0,
};

static struct resource nuc900_spi_resource[] = {
	[0] = {
		.start = W90X900_PA_I2C + SPIOFFSET,
		.end   = W90X900_PA_I2C + SPIOFFSET + SPIOREG_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_SSP,
		.end   = IRQ_SSP,
		.flags = IORESOURCE_IRQ,
	}
};

static struct platform_device nuc900_device_spi = {
	.name		= "nuc900-spi",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(nuc900_spi_resource),
	.resource	= nuc900_spi_resource,
	.dev		= {
				.platform_data = &nuc900_spiflash_data,
			}
};

/* spi device, spi flash info */

static struct mtd_partition nuc900_spi_flash_partitions[] = {
	{
		.name = "bootloader(spi)",
		.size = 0x0100000,
		.offset = 0,
	},
};

static struct flash_platform_data nuc900_spi_flash_data = {
	.name = "m25p80",
	.parts =  nuc900_spi_flash_partitions,
	.nr_parts = ARRAY_SIZE(nuc900_spi_flash_partitions),
	.type = "w25x16",
};

static struct spi_board_info nuc900_spi_board_info[] __initdata = {
	{
		.modalias = "m25p80",
		.max_speed_hz = 20000000,
		.bus_num = 0,
		.chip_select = 0,
		.platform_data = &nuc900_spi_flash_data,
		.mode = SPI_MODE_0,
	},
};

/* WDT Device */

static struct resource nuc900_wdt_resource[] = {
	[0] = {
		.start = W90X900_PA_TIMER,
		.end   = W90X900_PA_TIMER + W90X900_SZ_TIMER - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_WDT,
		.end   = IRQ_WDT,
		.flags = IORESOURCE_IRQ,
	}
};

static struct platform_device nuc900_device_wdt = {
	.name		= "nuc900-wdt",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(nuc900_wdt_resource),
	.resource	= nuc900_wdt_resource,
};

/*
 * public device definition between 910 and 920, or 910
 * and 950 or 950 and 960...,their dev platform register
 * should be in specific file such as nuc950, nuc960 c
 * files rather than the public dev.c file here. so the
 * corresponding platform_device definition should not be
 * static.
*/

/* RTC controller*/

static struct resource nuc900_rtc_resource[] = {
	[0] = {
		.start = W90X900_PA_RTC,
		.end   = W90X900_PA_RTC + 0xff,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_RTC,
		.end   = IRQ_RTC,
		.flags = IORESOURCE_IRQ,
	},
};

struct platform_device nuc900_device_rtc = {
	.name		= "nuc900-rtc",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(nuc900_rtc_resource),
	.resource	= nuc900_rtc_resource,
};

/*TouchScreen controller*/

static struct resource nuc900_ts_resource[] = {
	[0] = {
		.start = W90X900_PA_ADC,
		.end   = W90X900_PA_ADC + W90X900_SZ_ADC-1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_ADC,
		.end   = IRQ_ADC,
		.flags = IORESOURCE_IRQ,
	},
};

struct platform_device nuc900_device_ts = {
	.name		= "nuc900-ts",
	.id		= -1,
	.resource	= nuc900_ts_resource,
	.num_resources	= ARRAY_SIZE(nuc900_ts_resource),
};

/* FMI Device */

static struct resource nuc900_fmi_resource[] = {
	[0] = {
		.start = W90X900_PA_FMI,
		.end   = W90X900_PA_FMI + W90X900_SZ_FMI - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_FMI,
		.end   = IRQ_FMI,
		.flags = IORESOURCE_IRQ,
	}
};

struct platform_device nuc900_device_fmi = {
	.name		= "nuc900-fmi",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(nuc900_fmi_resource),
	.resource	= nuc900_fmi_resource,
};

/* KPI controller*/

static int nuc900_keymap[] = {
	KEY(0, 0, KEY_A),
	KEY(0, 1, KEY_B),
	KEY(0, 2, KEY_C),
	KEY(0, 3, KEY_D),

	KEY(1, 0, KEY_E),
	KEY(1, 1, KEY_F),
	KEY(1, 2, KEY_G),
	KEY(1, 3, KEY_H),

	KEY(2, 0, KEY_I),
	KEY(2, 1, KEY_J),
	KEY(2, 2, KEY_K),
	KEY(2, 3, KEY_L),

	KEY(3, 0, KEY_M),
	KEY(3, 1, KEY_N),
	KEY(3, 2, KEY_O),
	KEY(3, 3, KEY_P),
};

static struct matrix_keymap_data nuc900_map_data = {
	.keymap			= nuc900_keymap,
	.keymap_size		= ARRAY_SIZE(nuc900_keymap),
};

struct w90p910_keypad_platform_data nuc900_keypad_info = {
	.keymap_data	= &nuc900_map_data,
	.prescale	= 0xfa,
	.debounce	= 0x50,
};

static struct resource nuc900_kpi_resource[] = {
	[0] = {
		.start = W90X900_PA_KPI,
		.end   = W90X900_PA_KPI + W90X900_SZ_KPI - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_KPI,
		.end   = IRQ_KPI,
		.flags = IORESOURCE_IRQ,
	}

};

struct platform_device nuc900_device_kpi = {
	.name		= "nuc900-kpi",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(nuc900_kpi_resource),
	.resource	= nuc900_kpi_resource,
	.dev		= {
				.platform_data = &nuc900_keypad_info,
			}
};

/* LCD controller*/

static struct nuc900fb_display __initdata nuc900_lcd_info[] = {
	/* Giantplus Technology GPM1040A0 320x240 Color TFT LCD */
	[0] = {
		.type		= LCM_DCCS_VA_SRC_RGB565,
		.width		= 320,
		.height		= 240,
		.xres		= 320,
		.yres		= 240,
		.bpp		= 16,
		.pixclock	= 200000,
		.left_margin	= 34,
		.right_margin   = 54,
		.hsync_len	= 10,
		.upper_margin	= 18,
		.lower_margin	= 4,
		.vsync_len	= 1,
		.dccs		= 0x8e00041a,
		.devctl		= 0x060800c0,
		.fbctrl		= 0x00a000a0,
		.scale		= 0x04000400,
	},
};

static struct nuc900fb_mach_info nuc900_fb_info __initdata = {
#if defined(CONFIG_GPM1040A0_320X240)
	.displays		= &nuc900_lcd_info[0],
#else
	.displays		= nuc900_lcd_info,
#endif
	.num_displays		= ARRAY_SIZE(nuc900_lcd_info),
	.default_display	= 0,
	.gpio_dir		= 0x00000004,
	.gpio_dir_mask		= 0xFFFFFFFD,
	.gpio_data		= 0x00000004,
	.gpio_data_mask		= 0xFFFFFFFD,
};

static struct resource nuc900_lcd_resource[] = {
	[0] = {
		.start = W90X900_PA_LCD,
		.end   = W90X900_PA_LCD + W90X900_SZ_LCD - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_LCD,
		.end   = IRQ_LCD,
		.flags = IORESOURCE_IRQ,
	}
};

static u64 nuc900_device_lcd_dmamask = -1;
struct platform_device nuc900_device_lcd = {
	.name             = "nuc900-lcd",
	.id               = -1,
	.num_resources    = ARRAY_SIZE(nuc900_lcd_resource),
	.resource         = nuc900_lcd_resource,
	.dev              = {
		.dma_mask               = &nuc900_device_lcd_dmamask,
		.coherent_dma_mask      = -1,
		.platform_data = &nuc900_fb_info,
	}
};

/* AUDIO controller*/
static u64 nuc900_device_audio_dmamask = -1;
static struct resource nuc900_ac97_resource[] = {
	[0] = {
		.start = W90X900_PA_ACTL,
		.end   = W90X900_PA_ACTL + W90X900_SZ_ACTL - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_ACTL,
		.end   = IRQ_ACTL,
		.flags = IORESOURCE_IRQ,
	}

};

struct platform_device nuc900_device_audio = {
	.name		= "nuc900-audio",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(nuc900_ac97_resource),
	.resource	= nuc900_ac97_resource,
	.dev              = {
		.dma_mask               = &nuc900_device_audio_dmamask,
		.coherent_dma_mask      = -1,
	}
};

/*Here should be your evb resourse,such as LCD*/

static struct platform_device *nuc900_public_dev[] __initdata = {
	&nuc900_serial_device,
	&nuc900_flash_device,
	&nuc900_device_usb_ehci,
	&nuc900_device_usb_ohci,
	&nuc900_device_usbgadget,
	&nuc900_device_emc,
	&nuc900_device_spi,
	&nuc900_device_wdt,
	&nuc900_device_audio,
};

/* Provide adding specific CPU platform devices API */

void __init nuc900_board_init(struct platform_device **device, int size)
{
	platform_add_devices(device, size);
	platform_add_devices(nuc900_public_dev, ARRAY_SIZE(nuc900_public_dev));
	spi_register_board_info(nuc900_spi_board_info,
					ARRAY_SIZE(nuc900_spi_board_info));
}

