/*
 *  linux/arch/arm/mach-realview/realview_eb.c
 *
 *  Copyright (C) 2004 ARM Limited
 *  Copyright (C) 2000 Deep Blue Solutions Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/sysdev.h>
#include <linux/amba/bus.h>
#include <linux/amba/pl061.h>
#include <linux/amba/mmci.h>
#include <linux/amba/pl022.h>
#include <linux/io.h>

#include <mach/hardware.h>
#include <asm/irq.h>
#include <asm/leds.h>
#include <asm/mach-types.h>
#include <asm/pmu.h>
#include <asm/pgtable.h>
#include <asm/hardware/gic.h>
#include <asm/hardware/cache-l2x0.h>
#include <asm/localtimer.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/time.h>

#include <mach/board-eb.h>
#include <mach/irqs.h>

#include "core.h"

static struct map_desc realview_eb_io_desc[] __initdata = {
	{
		.virtual	= IO_ADDRESS(REALVIEW_SYS_BASE),
		.pfn		= __phys_to_pfn(REALVIEW_SYS_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(REALVIEW_EB_GIC_CPU_BASE),
		.pfn		= __phys_to_pfn(REALVIEW_EB_GIC_CPU_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(REALVIEW_EB_GIC_DIST_BASE),
		.pfn		= __phys_to_pfn(REALVIEW_EB_GIC_DIST_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(REALVIEW_SCTL_BASE),
		.pfn		= __phys_to_pfn(REALVIEW_SCTL_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(REALVIEW_EB_TIMER0_1_BASE),
		.pfn		= __phys_to_pfn(REALVIEW_EB_TIMER0_1_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(REALVIEW_EB_TIMER2_3_BASE),
		.pfn		= __phys_to_pfn(REALVIEW_EB_TIMER2_3_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	},
#ifdef CONFIG_DEBUG_LL
	{
		.virtual	= IO_ADDRESS(REALVIEW_EB_UART0_BASE),
		.pfn		= __phys_to_pfn(REALVIEW_EB_UART0_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}
#endif
};

static struct map_desc realview_eb11mp_io_desc[] __initdata = {
	{
		.virtual	= IO_ADDRESS(REALVIEW_EB11MP_GIC_CPU_BASE),
		.pfn		= __phys_to_pfn(REALVIEW_EB11MP_GIC_CPU_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(REALVIEW_EB11MP_GIC_DIST_BASE),
		.pfn		= __phys_to_pfn(REALVIEW_EB11MP_GIC_DIST_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= IO_ADDRESS(REALVIEW_EB11MP_L220_BASE),
		.pfn		= __phys_to_pfn(REALVIEW_EB11MP_L220_BASE),
		.length		= SZ_8K,
		.type		= MT_DEVICE,
	}
};

static void __init realview_eb_map_io(void)
{
	iotable_init(realview_eb_io_desc, ARRAY_SIZE(realview_eb_io_desc));
	if (core_tile_eb11mp() || core_tile_a9mp())
		iotable_init(realview_eb11mp_io_desc, ARRAY_SIZE(realview_eb11mp_io_desc));
}

static struct pl061_platform_data gpio0_plat_data = {
	.gpio_base	= 0,
	.irq_base	= -1,
};

static struct pl061_platform_data gpio1_plat_data = {
	.gpio_base	= 8,
	.irq_base	= -1,
};

static struct pl061_platform_data gpio2_plat_data = {
	.gpio_base	= 16,
	.irq_base	= -1,
};

static struct pl022_ssp_controller ssp0_plat_data = {
	.bus_id = 0,
	.enable_dma = 0,
	.num_chipselect = 1,
};

/*
 * RealView EB AMBA devices
 */

/*
 * These devices are connected via the core APB bridge
 */
#define GPIO2_IRQ	{ IRQ_EB_GPIO2, NO_IRQ }
#define GPIO3_IRQ	{ IRQ_EB_GPIO3, NO_IRQ }

#define AACI_IRQ	{ IRQ_EB_AACI, NO_IRQ }
#define MMCI0_IRQ	{ IRQ_EB_MMCI0A, IRQ_EB_MMCI0B }
#define KMI0_IRQ	{ IRQ_EB_KMI0, NO_IRQ }
#define KMI1_IRQ	{ IRQ_EB_KMI1, NO_IRQ }

/*
 * These devices are connected directly to the multi-layer AHB switch
 */
#define EB_SMC_IRQ	{ NO_IRQ, NO_IRQ }
#define MPMC_IRQ	{ NO_IRQ, NO_IRQ }
#define EB_CLCD_IRQ	{ IRQ_EB_CLCD, NO_IRQ }
#define DMAC_IRQ	{ IRQ_EB_DMA, NO_IRQ }

/*
 * These devices are connected via the core APB bridge
 */
#define SCTL_IRQ	{ NO_IRQ, NO_IRQ }
#define EB_WATCHDOG_IRQ	{ IRQ_EB_WDOG, NO_IRQ }
#define EB_GPIO0_IRQ	{ IRQ_EB_GPIO0, NO_IRQ }
#define GPIO1_IRQ	{ IRQ_EB_GPIO1, NO_IRQ }
#define EB_RTC_IRQ	{ IRQ_EB_RTC, NO_IRQ }

/*
 * These devices are connected via the DMA APB bridge
 */
#define SCI_IRQ		{ IRQ_EB_SCI, NO_IRQ }
#define EB_UART0_IRQ	{ IRQ_EB_UART0, NO_IRQ }
#define EB_UART1_IRQ	{ IRQ_EB_UART1, NO_IRQ }
#define EB_UART2_IRQ	{ IRQ_EB_UART2, NO_IRQ }
#define EB_UART3_IRQ	{ IRQ_EB_UART3, NO_IRQ }
#define EB_SSP_IRQ	{ IRQ_EB_SSP, NO_IRQ }

/* FPGA Primecells */
AMBA_DEVICE(aaci,  "fpga:aaci",  AACI,     NULL);
AMBA_DEVICE(mmc0,  "fpga:mmc0",  MMCI0,    &realview_mmc0_plat_data);
AMBA_DEVICE(kmi0,  "fpga:kmi0",  KMI0,     NULL);
AMBA_DEVICE(kmi1,  "fpga:kmi1",  KMI1,     NULL);
AMBA_DEVICE(uart3, "fpga:uart3", EB_UART3, NULL);

/* DevChip Primecells */
AMBA_DEVICE(smc,   "dev:smc",   EB_SMC,   NULL);
AMBA_DEVICE(clcd,  "dev:clcd",  EB_CLCD,  &clcd_plat_data);
AMBA_DEVICE(dmac,  "dev:dmac",  DMAC,     NULL);
AMBA_DEVICE(sctl,  "dev:sctl",  SCTL,     NULL);
AMBA_DEVICE(wdog,  "dev:wdog",  EB_WATCHDOG, NULL);
AMBA_DEVICE(gpio0, "dev:gpio0", EB_GPIO0, &gpio0_plat_data);
AMBA_DEVICE(gpio1, "dev:gpio1", GPIO1,    &gpio1_plat_data);
AMBA_DEVICE(gpio2, "dev:gpio2", GPIO2,    &gpio2_plat_data);
AMBA_DEVICE(rtc,   "dev:rtc",   EB_RTC,   NULL);
AMBA_DEVICE(sci0,  "dev:sci0",  SCI,      NULL);
AMBA_DEVICE(uart0, "dev:uart0", EB_UART0, NULL);
AMBA_DEVICE(uart1, "dev:uart1", EB_UART1, NULL);
AMBA_DEVICE(uart2, "dev:uart2", EB_UART2, NULL);
AMBA_DEVICE(ssp0,  "dev:ssp0",  EB_SSP,   &ssp0_plat_data);

static struct amba_device *amba_devs[] __initdata = {
	&dmac_device,
	&uart0_device,
	&uart1_device,
	&uart2_device,
	&uart3_device,
	&smc_device,
	&clcd_device,
	&sctl_device,
	&wdog_device,
	&gpio0_device,
	&gpio1_device,
	&gpio2_device,
	&rtc_device,
	&sci0_device,
	&ssp0_device,
	&aaci_device,
	&mmc0_device,
	&kmi0_device,
	&kmi1_device,
};

/*
 * RealView EB platform devices
 */
static struct resource realview_eb_flash_resource = {
	.start			= REALVIEW_EB_FLASH_BASE,
	.end			= REALVIEW_EB_FLASH_BASE + REALVIEW_EB_FLASH_SIZE - 1,
	.flags			= IORESOURCE_MEM,
};

static struct resource realview_eb_eth_resources[] = {
	[0] = {
		.start		= REALVIEW_EB_ETH_BASE,
		.end		= REALVIEW_EB_ETH_BASE + SZ_64K - 1,
		.flags		= IORESOURCE_MEM,
	},
	[1] = {
		.start		= IRQ_EB_ETH,
		.end		= IRQ_EB_ETH,
		.flags		= IORESOURCE_IRQ,
	},
};

/*
 * Detect and register the correct Ethernet device. RealView/EB rev D
 * platforms use the newer SMSC LAN9118 Ethernet chip
 */
static int eth_device_register(void)
{
	void __iomem *eth_addr = ioremap(REALVIEW_EB_ETH_BASE, SZ_4K);
	const char *name = NULL;
	u32 idrev;

	if (!eth_addr)
		return -ENOMEM;

	idrev = readl(eth_addr + 0x50);
	if ((idrev & 0xFFFF0000) != 0x01180000)
		/* SMSC LAN9118 not present, use LAN91C111 instead */
		name = "smc91x";

	iounmap(eth_addr);
	return realview_eth_register(name, realview_eb_eth_resources);
}

static struct resource realview_eb_isp1761_resources[] = {
	[0] = {
		.start		= REALVIEW_EB_USB_BASE,
		.end		= REALVIEW_EB_USB_BASE + SZ_128K - 1,
		.flags		= IORESOURCE_MEM,
	},
	[1] = {
		.start		= IRQ_EB_USB,
		.end		= IRQ_EB_USB,
		.flags		= IORESOURCE_IRQ,
	},
};

static struct resource pmu_resources[] = {
	[0] = {
		.start		= IRQ_EB11MP_PMU_CPU0,
		.end		= IRQ_EB11MP_PMU_CPU0,
		.flags		= IORESOURCE_IRQ,
	},
	[1] = {
		.start		= IRQ_EB11MP_PMU_CPU1,
		.end		= IRQ_EB11MP_PMU_CPU1,
		.flags		= IORESOURCE_IRQ,
	},
	[2] = {
		.start		= IRQ_EB11MP_PMU_CPU2,
		.end		= IRQ_EB11MP_PMU_CPU2,
		.flags		= IORESOURCE_IRQ,
	},
	[3] = {
		.start		= IRQ_EB11MP_PMU_CPU3,
		.end		= IRQ_EB11MP_PMU_CPU3,
		.flags		= IORESOURCE_IRQ,
	},
};

static struct platform_device pmu_device = {
	.name			= "arm-pmu",
	.id			= ARM_PMU_DEVICE_CPU,
	.num_resources		= ARRAY_SIZE(pmu_resources),
	.resource		= pmu_resources,
};

static struct resource char_lcd_resources[] = {
	{
		.start = REALVIEW_CHAR_LCD_BASE,
		.end   = (REALVIEW_CHAR_LCD_BASE + SZ_4K - 1),
		.flags = IORESOURCE_MEM,
	},
	{
		.start	= IRQ_EB_CHARLCD,
		.end	= IRQ_EB_CHARLCD,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device char_lcd_device = {
	.name		=	"arm-charlcd",
	.id		=	-1,
	.num_resources	=	ARRAY_SIZE(char_lcd_resources),
	.resource	=	char_lcd_resources,
};

static void __init gic_init_irq(void)
{
	if (core_tile_eb11mp() || core_tile_a9mp()) {
		unsigned int pldctrl;

		/* new irq mode */
		writel(0x0000a05f, __io_address(REALVIEW_SYS_LOCK));
		pldctrl = readl(__io_address(REALVIEW_SYS_BASE)	+ REALVIEW_EB11MP_SYS_PLD_CTRL1);
		pldctrl |= 0x00800000;
		writel(pldctrl, __io_address(REALVIEW_SYS_BASE) + REALVIEW_EB11MP_SYS_PLD_CTRL1);
		writel(0x00000000, __io_address(REALVIEW_SYS_LOCK));

		/* core tile GIC, primary */
		gic_init(0, 29, __io_address(REALVIEW_EB11MP_GIC_DIST_BASE),
			 __io_address(REALVIEW_EB11MP_GIC_CPU_BASE));

#ifndef CONFIG_REALVIEW_EB_ARM11MP_REVB
		/* board GIC, secondary */
		gic_init(1, 96, __io_address(REALVIEW_EB_GIC_DIST_BASE),
			 __io_address(REALVIEW_EB_GIC_CPU_BASE));
		gic_cascade_irq(1, IRQ_EB11MP_EB_IRQ1);
#endif
	} else {
		/* board GIC, primary */
		gic_init(0, 29, __io_address(REALVIEW_EB_GIC_DIST_BASE),
			 __io_address(REALVIEW_EB_GIC_CPU_BASE));
	}
}

/*
 * Fix up the IRQ numbers for the RealView EB/ARM11MPCore tile
 */
static void realview_eb11mp_fixup(void)
{
	/* AMBA devices */
	dmac_device.irq[0]	= IRQ_EB11MP_DMA;
	uart0_device.irq[0]	= IRQ_EB11MP_UART0;
	uart1_device.irq[0]	= IRQ_EB11MP_UART1;
	uart2_device.irq[0]	= IRQ_EB11MP_UART2;
	uart3_device.irq[0]	= IRQ_EB11MP_UART3;
	clcd_device.irq[0]	= IRQ_EB11MP_CLCD;
	wdog_device.irq[0]	= IRQ_EB11MP_WDOG;
	gpio0_device.irq[0]	= IRQ_EB11MP_GPIO0;
	gpio1_device.irq[0]	= IRQ_EB11MP_GPIO1;
	gpio2_device.irq[0]	= IRQ_EB11MP_GPIO2;
	rtc_device.irq[0]	= IRQ_EB11MP_RTC;
	sci0_device.irq[0]	= IRQ_EB11MP_SCI;
	ssp0_device.irq[0]	= IRQ_EB11MP_SSP;
	aaci_device.irq[0]	= IRQ_EB11MP_AACI;
	mmc0_device.irq[0]	= IRQ_EB11MP_MMCI0A;
	mmc0_device.irq[1]	= IRQ_EB11MP_MMCI0B;
	kmi0_device.irq[0]	= IRQ_EB11MP_KMI0;
	kmi1_device.irq[0]	= IRQ_EB11MP_KMI1;

	/* platform devices */
	realview_eb_eth_resources[1].start	= IRQ_EB11MP_ETH;
	realview_eb_eth_resources[1].end	= IRQ_EB11MP_ETH;
	realview_eb_isp1761_resources[1].start	= IRQ_EB11MP_USB;
	realview_eb_isp1761_resources[1].end	= IRQ_EB11MP_USB;
}

static void __init realview_eb_timer_init(void)
{
	unsigned int timer_irq;

	timer0_va_base = __io_address(REALVIEW_EB_TIMER0_1_BASE);
	timer1_va_base = __io_address(REALVIEW_EB_TIMER0_1_BASE) + 0x20;
	timer2_va_base = __io_address(REALVIEW_EB_TIMER2_3_BASE);
	timer3_va_base = __io_address(REALVIEW_EB_TIMER2_3_BASE) + 0x20;

	if (core_tile_eb11mp() || core_tile_a9mp()) {
#ifdef CONFIG_LOCAL_TIMERS
		twd_base = __io_address(REALVIEW_EB11MP_TWD_BASE);
#endif
		timer_irq = IRQ_EB11MP_TIMER0_1;
	} else
		timer_irq = IRQ_EB_TIMER0_1;

	realview_timer_init(timer_irq);
}

static struct sys_timer realview_eb_timer = {
	.init		= realview_eb_timer_init,
};

static void realview_eb_reset(char mode)
{
	void __iomem *reset_ctrl = __io_address(REALVIEW_SYS_RESETCTL);
	void __iomem *lock_ctrl = __io_address(REALVIEW_SYS_LOCK);

	/*
	 * To reset, we hit the on-board reset register
	 * in the system FPGA
	 */
	__raw_writel(REALVIEW_SYS_LOCK_VAL, lock_ctrl);
	if (core_tile_eb11mp())
		__raw_writel(0x0008, reset_ctrl);
}

static void __init realview_eb_init(void)
{
	int i;

	if (core_tile_eb11mp() || core_tile_a9mp()) {
		realview_eb11mp_fixup();

#ifdef CONFIG_CACHE_L2X0
		/* 1MB (128KB/way), 8-way associativity, evmon/parity/share enabled
		 * Bits:  .... ...0 0111 1001 0000 .... .... .... */
		l2x0_init(__io_address(REALVIEW_EB11MP_L220_BASE), 0x00790000, 0xfe000fff);
#endif
		platform_device_register(&pmu_device);
	}

	realview_flash_register(&realview_eb_flash_resource, 1);
	platform_device_register(&realview_i2c_device);
	platform_device_register(&char_lcd_device);
	eth_device_register();
	realview_usb_register(realview_eb_isp1761_resources);

	for (i = 0; i < ARRAY_SIZE(amba_devs); i++) {
		struct amba_device *d = amba_devs[i];
		amba_device_register(d, &iomem_resource);
	}

#ifdef CONFIG_LEDS
	leds_event = realview_leds_event;
#endif
	realview_reset = realview_eb_reset;
}

MACHINE_START(REALVIEW_EB, "ARM-RealView EB")
	/* Maintainer: ARM Ltd/Deep Blue Solutions Ltd */
	.boot_params	= PLAT_PHYS_OFFSET + 0x00000100,
	.fixup		= realview_fixup,
	.map_io		= realview_eb_map_io,
	.init_early	= realview_init_early,
	.init_irq	= gic_init_irq,
	.timer		= &realview_eb_timer,
	.init_machine	= realview_eb_init,
MACHINE_END
