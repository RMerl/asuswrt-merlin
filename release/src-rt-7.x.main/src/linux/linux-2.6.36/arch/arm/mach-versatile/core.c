/*
 *  linux/arch/arm/mach-versatile/core.c
 *
 *  Copyright (C) 1999 - 2003 ARM Limited
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
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/sysdev.h>
#include <linux/interrupt.h>
#include <linux/amba/bus.h>
#include <linux/amba/clcd.h>
#include <linux/amba/pl061.h>
#include <linux/amba/mmci.h>
#include <linux/amba/pl022.h>
#include <linux/io.h>
#include <linux/gfp.h>

#include <asm/clkdev.h>
#include <asm/system.h>
#include <asm/irq.h>
#include <asm/leds.h>
#include <asm/hardware/arm_timer.h>
#include <asm/hardware/icst.h>
#include <asm/hardware/vic.h>
#include <asm/mach-types.h>

#include <asm/mach/arch.h>
#include <asm/mach/flash.h>
#include <asm/mach/irq.h>
#include <asm/mach/time.h>
#include <asm/mach/map.h>
#include <mach/clkdev.h>
#include <mach/hardware.h>
#include <mach/platform.h>
#include <plat/timer-sp.h>

#include "core.h"

#define VA_VIC_BASE		__io_address(VERSATILE_VIC_BASE)
#define VA_SIC_BASE		__io_address(VERSATILE_SIC_BASE)

static void sic_mask_irq(unsigned int irq)
{
	irq -= IRQ_SIC_START;
	writel(1 << irq, VA_SIC_BASE + SIC_IRQ_ENABLE_CLEAR);
}

static void sic_unmask_irq(unsigned int irq)
{
	irq -= IRQ_SIC_START;
	writel(1 << irq, VA_SIC_BASE + SIC_IRQ_ENABLE_SET);
}

static struct irq_chip sic_chip = {
	.name	= "SIC",
	.ack	= sic_mask_irq,
	.mask	= sic_mask_irq,
	.unmask	= sic_unmask_irq,
};

static void
sic_handle_irq(unsigned int irq, struct irq_desc *desc)
{
	unsigned long status = readl(VA_SIC_BASE + SIC_IRQ_STATUS);

	if (status == 0) {
		do_bad_IRQ(irq, desc);
		return;
	}

	do {
		irq = ffs(status) - 1;
		status &= ~(1 << irq);

		irq += IRQ_SIC_START;

		generic_handle_irq(irq);
	} while (status);
}

#define IRQ_MMCI0A	IRQ_VICSOURCE22
#define IRQ_AACI	IRQ_VICSOURCE24
#define IRQ_ETH		IRQ_VICSOURCE25
#define PIC_MASK	0xFFD00000

void __init versatile_init_irq(void)
{
	unsigned int i;

	vic_init(VA_VIC_BASE, IRQ_VIC_START, ~0, 0);

	set_irq_chained_handler(IRQ_VICSOURCE31, sic_handle_irq);

	/* Do second interrupt controller */
	writel(~0, VA_SIC_BASE + SIC_IRQ_ENABLE_CLEAR);

	for (i = IRQ_SIC_START; i <= IRQ_SIC_END; i++) {
		if ((PIC_MASK & (1 << (i - IRQ_SIC_START))) == 0) {
			set_irq_chip(i, &sic_chip);
			set_irq_handler(i, handle_level_irq);
			set_irq_flags(i, IRQF_VALID | IRQF_PROBE);
		}
	}

	/*
	 * Interrupts on secondary controller from 0 to 8 are routed to
	 * source 31 on PIC.
	 * Interrupts from 21 to 31 are routed directly to the VIC on
	 * the corresponding number on primary controller. This is controlled
	 * by setting PIC_ENABLEx.
	 */
	writel(PIC_MASK, VA_SIC_BASE + SIC_INT_PIC_ENABLE);
}

static struct map_desc versatile_io_desc[] __initdata = {
	{
		.virtual	=  IO_ADDRESS(VERSATILE_SYS_BASE),
		.pfn		= __phys_to_pfn(VERSATILE_SYS_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE
	}, {
		.virtual	=  IO_ADDRESS(VERSATILE_SIC_BASE),
		.pfn		= __phys_to_pfn(VERSATILE_SIC_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE
	}, {
		.virtual	=  IO_ADDRESS(VERSATILE_VIC_BASE),
		.pfn		= __phys_to_pfn(VERSATILE_VIC_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE
	}, {
		.virtual	=  IO_ADDRESS(VERSATILE_SCTL_BASE),
		.pfn		= __phys_to_pfn(VERSATILE_SCTL_BASE),
		.length		= SZ_4K * 9,
		.type		= MT_DEVICE
	},
#ifdef CONFIG_MACH_VERSATILE_AB
 	{
		.virtual	=  IO_ADDRESS(VERSATILE_GPIO0_BASE),
		.pfn		= __phys_to_pfn(VERSATILE_GPIO0_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE
	}, {
		.virtual	=  IO_ADDRESS(VERSATILE_IB2_BASE),
		.pfn		= __phys_to_pfn(VERSATILE_IB2_BASE),
		.length		= SZ_64M,
		.type		= MT_DEVICE
	},
#endif
#ifdef CONFIG_DEBUG_LL
 	{
		.virtual	=  IO_ADDRESS(VERSATILE_UART0_BASE),
		.pfn		= __phys_to_pfn(VERSATILE_UART0_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE
	},
#endif
#ifdef CONFIG_PCI
 	{
		.virtual	=  IO_ADDRESS(VERSATILE_PCI_CORE_BASE),
		.pfn		= __phys_to_pfn(VERSATILE_PCI_CORE_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE
	}, {
		.virtual	=  (unsigned long)VERSATILE_PCI_VIRT_BASE,
		.pfn		= __phys_to_pfn(VERSATILE_PCI_BASE),
		.length		= VERSATILE_PCI_BASE_SIZE,
		.type		= MT_DEVICE
	}, {
		.virtual	=  (unsigned long)VERSATILE_PCI_CFG_VIRT_BASE,
		.pfn		= __phys_to_pfn(VERSATILE_PCI_CFG_BASE),
		.length		= VERSATILE_PCI_CFG_BASE_SIZE,
		.type		= MT_DEVICE
	},
#endif
};

void __init versatile_map_io(void)
{
	iotable_init(versatile_io_desc, ARRAY_SIZE(versatile_io_desc));
}


#define VERSATILE_FLASHCTRL    (__io_address(VERSATILE_SYS_BASE) + VERSATILE_SYS_FLASH_OFFSET)

static int versatile_flash_init(void)
{
	u32 val;

	val = __raw_readl(VERSATILE_FLASHCTRL);
	val &= ~VERSATILE_FLASHPROG_FLVPPEN;
	__raw_writel(val, VERSATILE_FLASHCTRL);

	return 0;
}

static void versatile_flash_exit(void)
{
	u32 val;

	val = __raw_readl(VERSATILE_FLASHCTRL);
	val &= ~VERSATILE_FLASHPROG_FLVPPEN;
	__raw_writel(val, VERSATILE_FLASHCTRL);
}

static void versatile_flash_set_vpp(int on)
{
	u32 val;

	val = __raw_readl(VERSATILE_FLASHCTRL);
	if (on)
		val |= VERSATILE_FLASHPROG_FLVPPEN;
	else
		val &= ~VERSATILE_FLASHPROG_FLVPPEN;
	__raw_writel(val, VERSATILE_FLASHCTRL);
}

static struct flash_platform_data versatile_flash_data = {
	.map_name		= "cfi_probe",
	.width			= 4,
	.init			= versatile_flash_init,
	.exit			= versatile_flash_exit,
	.set_vpp		= versatile_flash_set_vpp,
};

static struct resource versatile_flash_resource = {
	.start			= VERSATILE_FLASH_BASE,
	.end			= VERSATILE_FLASH_BASE + VERSATILE_FLASH_SIZE - 1,
	.flags			= IORESOURCE_MEM,
};

static struct platform_device versatile_flash_device = {
	.name			= "armflash",
	.id			= 0,
	.dev			= {
		.platform_data	= &versatile_flash_data,
	},
	.num_resources		= 1,
	.resource		= &versatile_flash_resource,
};

static struct resource smc91x_resources[] = {
	[0] = {
		.start		= VERSATILE_ETH_BASE,
		.end		= VERSATILE_ETH_BASE + SZ_64K - 1,
		.flags		= IORESOURCE_MEM,
	},
	[1] = {
		.start		= IRQ_ETH,
		.end		= IRQ_ETH,
		.flags		= IORESOURCE_IRQ,
	},
};

static struct platform_device smc91x_device = {
	.name		= "smc91x",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(smc91x_resources),
	.resource	= smc91x_resources,
};

static struct resource versatile_i2c_resource = {
	.start			= VERSATILE_I2C_BASE,
	.end			= VERSATILE_I2C_BASE + SZ_4K - 1,
	.flags			= IORESOURCE_MEM,
};

static struct platform_device versatile_i2c_device = {
	.name			= "versatile-i2c",
	.id			= 0,
	.num_resources		= 1,
	.resource		= &versatile_i2c_resource,
};

static struct i2c_board_info versatile_i2c_board_info[] = {
	{
		I2C_BOARD_INFO("ds1338", 0xd0 >> 1),
	},
};

static int __init versatile_i2c_init(void)
{
	return i2c_register_board_info(0, versatile_i2c_board_info,
				       ARRAY_SIZE(versatile_i2c_board_info));
}
arch_initcall(versatile_i2c_init);

#define VERSATILE_SYSMCI	(__io_address(VERSATILE_SYS_BASE) + VERSATILE_SYS_MCI_OFFSET)

unsigned int mmc_status(struct device *dev)
{
	struct amba_device *adev = container_of(dev, struct amba_device, dev);
	u32 mask;

	if (adev->res.start == VERSATILE_MMCI0_BASE)
		mask = 1;
	else
		mask = 2;

	return readl(VERSATILE_SYSMCI) & mask;
}

static struct mmci_platform_data mmc0_plat_data = {
	.ocr_mask	= MMC_VDD_32_33|MMC_VDD_33_34,
	.status		= mmc_status,
	.gpio_wp	= -1,
	.gpio_cd	= -1,
};

static struct resource char_lcd_resources[] = {
	{
		.start = VERSATILE_CHAR_LCD_BASE,
		.end   = (VERSATILE_CHAR_LCD_BASE + SZ_4K - 1),
		.flags = IORESOURCE_MEM,
	},
};

static struct platform_device char_lcd_device = {
	.name           =       "arm-charlcd",
	.id             =       -1,
	.num_resources  =       ARRAY_SIZE(char_lcd_resources),
	.resource       =       char_lcd_resources,
};

/*
 * Clock handling
 */
static const struct icst_params versatile_oscvco_params = {
	.ref		= 24000000,
	.vco_max	= ICST307_VCO_MAX,
	.vco_min	= ICST307_VCO_MIN,
	.vd_min		= 4 + 8,
	.vd_max		= 511 + 8,
	.rd_min		= 1 + 2,
	.rd_max		= 127 + 2,
	.s2div		= icst307_s2div,
	.idx2s		= icst307_idx2s,
};

static void versatile_oscvco_set(struct clk *clk, struct icst_vco vco)
{
	void __iomem *sys_lock = __io_address(VERSATILE_SYS_BASE) + VERSATILE_SYS_LOCK_OFFSET;
	u32 val;

	val = readl(clk->vcoreg) & ~0x7ffff;
	val |= vco.v | (vco.r << 9) | (vco.s << 16);

	writel(0xa05f, sys_lock);
	writel(val, clk->vcoreg);
	writel(0, sys_lock);
}

static const struct clk_ops osc4_clk_ops = {
	.round	= icst_clk_round,
	.set	= icst_clk_set,
	.setvco	= versatile_oscvco_set,
};

static struct clk osc4_clk = {
	.ops	= &osc4_clk_ops,
	.params	= &versatile_oscvco_params,
};

/*
 * These are fixed clocks.
 */
static struct clk ref24_clk = {
	.rate	= 24000000,
};

static struct clk dummy_apb_pclk;

static struct clk_lookup lookups[] = {
	{	/* AMBA bus clock */
		.con_id		= "apb_pclk",
		.clk		= &dummy_apb_pclk,
	}, {	/* UART0 */
		.dev_id		= "dev:f1",
		.clk		= &ref24_clk,
	}, {	/* UART1 */
		.dev_id		= "dev:f2",
		.clk		= &ref24_clk,
	}, {	/* UART2 */
		.dev_id		= "dev:f3",
		.clk		= &ref24_clk,
	}, {	/* UART3 */
		.dev_id		= "fpga:09",
		.clk		= &ref24_clk,
	}, {	/* KMI0 */
		.dev_id		= "fpga:06",
		.clk		= &ref24_clk,
	}, {	/* KMI1 */
		.dev_id		= "fpga:07",
		.clk		= &ref24_clk,
	}, {	/* MMC0 */
		.dev_id		= "fpga:05",
		.clk		= &ref24_clk,
	}, {	/* MMC1 */
		.dev_id		= "fpga:0b",
		.clk		= &ref24_clk,
	}, {	/* SSP */
		.dev_id		= "dev:f4",
		.clk		= &ref24_clk,
	}, {	/* CLCD */
		.dev_id		= "dev:20",
		.clk		= &osc4_clk,
	}
};

/*
 * CLCD support.
 */
#define SYS_CLCD_MODE_MASK	(3 << 0)
#define SYS_CLCD_MODE_888	(0 << 0)
#define SYS_CLCD_MODE_5551	(1 << 0)
#define SYS_CLCD_MODE_565_RLSB	(2 << 0)
#define SYS_CLCD_MODE_565_BLSB	(3 << 0)
#define SYS_CLCD_NLCDIOON	(1 << 2)
#define SYS_CLCD_VDDPOSSWITCH	(1 << 3)
#define SYS_CLCD_PWR3V5SWITCH	(1 << 4)
#define SYS_CLCD_ID_MASK	(0x1f << 8)
#define SYS_CLCD_ID_SANYO_3_8	(0x00 << 8)
#define SYS_CLCD_ID_UNKNOWN_8_4	(0x01 << 8)
#define SYS_CLCD_ID_EPSON_2_2	(0x02 << 8)
#define SYS_CLCD_ID_SANYO_2_5	(0x07 << 8)
#define SYS_CLCD_ID_VGA		(0x1f << 8)

static struct clcd_panel vga = {
	.mode		= {
		.name		= "VGA",
		.refresh	= 60,
		.xres		= 640,
		.yres		= 480,
		.pixclock	= 39721,
		.left_margin	= 40,
		.right_margin	= 24,
		.upper_margin	= 32,
		.lower_margin	= 11,
		.hsync_len	= 96,
		.vsync_len	= 2,
		.sync		= 0,
		.vmode		= FB_VMODE_NONINTERLACED,
	},
	.width		= -1,
	.height		= -1,
	.tim2		= TIM2_BCD | TIM2_IPC,
	.cntl		= CNTL_LCDTFT | CNTL_LCDVCOMP(1),
	.bpp		= 16,
};

static struct clcd_panel sanyo_3_8_in = {
	.mode		= {
		.name		= "Sanyo QVGA",
		.refresh	= 116,
		.xres		= 320,
		.yres		= 240,
		.pixclock	= 100000,
		.left_margin	= 6,
		.right_margin	= 6,
		.upper_margin	= 5,
		.lower_margin	= 5,
		.hsync_len	= 6,
		.vsync_len	= 6,
		.sync		= 0,
		.vmode		= FB_VMODE_NONINTERLACED,
	},
	.width		= -1,
	.height		= -1,
	.tim2		= TIM2_BCD,
	.cntl		= CNTL_LCDTFT | CNTL_LCDVCOMP(1),
	.bpp		= 16,
};

static struct clcd_panel sanyo_2_5_in = {
	.mode		= {
		.name		= "Sanyo QVGA Portrait",
		.refresh	= 116,
		.xres		= 240,
		.yres		= 320,
		.pixclock	= 100000,
		.left_margin	= 20,
		.right_margin	= 10,
		.upper_margin	= 2,
		.lower_margin	= 2,
		.hsync_len	= 10,
		.vsync_len	= 2,
		.sync		= FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
		.vmode		= FB_VMODE_NONINTERLACED,
	},
	.width		= -1,
	.height		= -1,
	.tim2		= TIM2_IVS | TIM2_IHS | TIM2_IPC,
	.cntl		= CNTL_LCDTFT | CNTL_LCDVCOMP(1),
	.bpp		= 16,
};

static struct clcd_panel epson_2_2_in = {
	.mode		= {
		.name		= "Epson QCIF",
		.refresh	= 390,
		.xres		= 176,
		.yres		= 220,
		.pixclock	= 62500,
		.left_margin	= 3,
		.right_margin	= 2,
		.upper_margin	= 1,
		.lower_margin	= 0,
		.hsync_len	= 3,
		.vsync_len	= 2,
		.sync		= 0,
		.vmode		= FB_VMODE_NONINTERLACED,
	},
	.width		= -1,
	.height		= -1,
	.tim2		= TIM2_BCD | TIM2_IPC,
	.cntl		= CNTL_LCDTFT | CNTL_LCDVCOMP(1),
	.bpp		= 16,
};

/*
 * Detect which LCD panel is connected, and return the appropriate
 * clcd_panel structure.  Note: we do not have any information on
 * the required timings for the 8.4in panel, so we presently assume
 * VGA timings.
 */
static struct clcd_panel *versatile_clcd_panel(void)
{
	void __iomem *sys_clcd = __io_address(VERSATILE_SYS_BASE) + VERSATILE_SYS_CLCD_OFFSET;
	struct clcd_panel *panel = &vga;
	u32 val;

	val = readl(sys_clcd) & SYS_CLCD_ID_MASK;
	if (val == SYS_CLCD_ID_SANYO_3_8)
		panel = &sanyo_3_8_in;
	else if (val == SYS_CLCD_ID_SANYO_2_5)
		panel = &sanyo_2_5_in;
	else if (val == SYS_CLCD_ID_EPSON_2_2)
		panel = &epson_2_2_in;
	else if (val == SYS_CLCD_ID_VGA)
		panel = &vga;
	else {
		printk(KERN_ERR "CLCD: unknown LCD panel ID 0x%08x, using VGA\n",
			val);
		panel = &vga;
	}

	return panel;
}

/*
 * Disable all display connectors on the interface module.
 */
static void versatile_clcd_disable(struct clcd_fb *fb)
{
	void __iomem *sys_clcd = __io_address(VERSATILE_SYS_BASE) + VERSATILE_SYS_CLCD_OFFSET;
	u32 val;

	val = readl(sys_clcd);
	val &= ~SYS_CLCD_NLCDIOON | SYS_CLCD_PWR3V5SWITCH;
	writel(val, sys_clcd);

#ifdef CONFIG_MACH_VERSATILE_AB
	/*
	 * If the LCD is Sanyo 2x5 in on the IB2 board, turn the back-light off
	 */
	if (machine_is_versatile_ab() && fb->panel == &sanyo_2_5_in) {
		void __iomem *versatile_ib2_ctrl = __io_address(VERSATILE_IB2_CTRL);
		unsigned long ctrl;

		ctrl = readl(versatile_ib2_ctrl);
		ctrl &= ~0x01;
		writel(ctrl, versatile_ib2_ctrl);
	}
#endif
}

/*
 * Enable the relevant connector on the interface module.
 */
static void versatile_clcd_enable(struct clcd_fb *fb)
{
	void __iomem *sys_clcd = __io_address(VERSATILE_SYS_BASE) + VERSATILE_SYS_CLCD_OFFSET;
	u32 val;

	val = readl(sys_clcd);
	val &= ~SYS_CLCD_MODE_MASK;

	switch (fb->fb.var.green.length) {
	case 5:
		val |= SYS_CLCD_MODE_5551;
		break;
	case 6:
		val |= SYS_CLCD_MODE_565_RLSB;
		break;
	case 8:
		val |= SYS_CLCD_MODE_888;
		break;
	}

	/*
	 * Set the MUX
	 */
	writel(val, sys_clcd);

	/*
	 * And now enable the PSUs
	 */
	val |= SYS_CLCD_NLCDIOON | SYS_CLCD_PWR3V5SWITCH;
	writel(val, sys_clcd);

#ifdef CONFIG_MACH_VERSATILE_AB
	/*
	 * If the LCD is Sanyo 2x5 in on the IB2 board, turn the back-light on
	 */
	if (machine_is_versatile_ab() && fb->panel == &sanyo_2_5_in) {
		void __iomem *versatile_ib2_ctrl = __io_address(VERSATILE_IB2_CTRL);
		unsigned long ctrl;

		ctrl = readl(versatile_ib2_ctrl);
		ctrl |= 0x01;
		writel(ctrl, versatile_ib2_ctrl);
	}
#endif
}

static unsigned long framesize = SZ_1M;

static int versatile_clcd_setup(struct clcd_fb *fb)
{
	dma_addr_t dma;

	fb->panel		= versatile_clcd_panel();

	fb->fb.screen_base = dma_alloc_writecombine(&fb->dev->dev, framesize,
						    &dma, GFP_KERNEL);
	if (!fb->fb.screen_base) {
		printk(KERN_ERR "CLCD: unable to map framebuffer\n");
		return -ENOMEM;
	}

	fb->fb.fix.smem_start	= dma;
	fb->fb.fix.smem_len	= framesize;

	return 0;
}

static int versatile_clcd_mmap(struct clcd_fb *fb, struct vm_area_struct *vma)
{
	return dma_mmap_writecombine(&fb->dev->dev, vma,
				     fb->fb.screen_base,
				     fb->fb.fix.smem_start,
				     fb->fb.fix.smem_len);
}

static void versatile_clcd_remove(struct clcd_fb *fb)
{
	dma_free_writecombine(&fb->dev->dev, fb->fb.fix.smem_len,
			      fb->fb.screen_base, fb->fb.fix.smem_start);
}

static struct clcd_board clcd_plat_data = {
	.name		= "Versatile",
	.check		= clcdfb_check,
	.decode		= clcdfb_decode,
	.disable	= versatile_clcd_disable,
	.enable		= versatile_clcd_enable,
	.setup		= versatile_clcd_setup,
	.mmap		= versatile_clcd_mmap,
	.remove		= versatile_clcd_remove,
};

static struct pl061_platform_data gpio0_plat_data = {
	.gpio_base	= 0,
	.irq_base	= IRQ_GPIO0_START,
};

static struct pl061_platform_data gpio1_plat_data = {
	.gpio_base	= 8,
	.irq_base	= IRQ_GPIO1_START,
};

static struct pl022_ssp_controller ssp0_plat_data = {
	.bus_id = 0,
	.enable_dma = 0,
	.num_chipselect = 1,
};

#define AACI_IRQ	{ IRQ_AACI, NO_IRQ }
#define AACI_DMA	{ 0x80, 0x81 }
#define MMCI0_IRQ	{ IRQ_MMCI0A,IRQ_SIC_MMCI0B }
#define MMCI0_DMA	{ 0x84, 0 }
#define KMI0_IRQ	{ IRQ_SIC_KMI0, NO_IRQ }
#define KMI0_DMA	{ 0, 0 }
#define KMI1_IRQ	{ IRQ_SIC_KMI1, NO_IRQ }
#define KMI1_DMA	{ 0, 0 }

/*
 * These devices are connected directly to the multi-layer AHB switch
 */
#define SMC_IRQ		{ NO_IRQ, NO_IRQ }
#define SMC_DMA		{ 0, 0 }
#define MPMC_IRQ	{ NO_IRQ, NO_IRQ }
#define MPMC_DMA	{ 0, 0 }
#define CLCD_IRQ	{ IRQ_CLCDINT, NO_IRQ }
#define CLCD_DMA	{ 0, 0 }
#define DMAC_IRQ	{ IRQ_DMAINT, NO_IRQ }
#define DMAC_DMA	{ 0, 0 }

/*
 * These devices are connected via the core APB bridge
 */
#define SCTL_IRQ	{ NO_IRQ, NO_IRQ }
#define SCTL_DMA	{ 0, 0 }
#define WATCHDOG_IRQ	{ IRQ_WDOGINT, NO_IRQ }
#define WATCHDOG_DMA	{ 0, 0 }
#define GPIO0_IRQ	{ IRQ_GPIOINT0, NO_IRQ }
#define GPIO0_DMA	{ 0, 0 }
#define GPIO1_IRQ	{ IRQ_GPIOINT1, NO_IRQ }
#define GPIO1_DMA	{ 0, 0 }
#define RTC_IRQ		{ IRQ_RTCINT, NO_IRQ }
#define RTC_DMA		{ 0, 0 }

/*
 * These devices are connected via the DMA APB bridge
 */
#define SCI_IRQ		{ IRQ_SCIINT, NO_IRQ }
#define SCI_DMA		{ 7, 6 }
#define UART0_IRQ	{ IRQ_UARTINT0, NO_IRQ }
#define UART0_DMA	{ 15, 14 }
#define UART1_IRQ	{ IRQ_UARTINT1, NO_IRQ }
#define UART1_DMA	{ 13, 12 }
#define UART2_IRQ	{ IRQ_UARTINT2, NO_IRQ }
#define UART2_DMA	{ 11, 10 }
#define SSP_IRQ		{ IRQ_SSPINT, NO_IRQ }
#define SSP_DMA		{ 9, 8 }

/* FPGA Primecells */
AMBA_DEVICE(aaci,  "fpga:04", AACI,     NULL);
AMBA_DEVICE(mmc0,  "fpga:05", MMCI0,    &mmc0_plat_data);
AMBA_DEVICE(kmi0,  "fpga:06", KMI0,     NULL);
AMBA_DEVICE(kmi1,  "fpga:07", KMI1,     NULL);

/* DevChip Primecells */
AMBA_DEVICE(smc,   "dev:00",  SMC,      NULL);
AMBA_DEVICE(mpmc,  "dev:10",  MPMC,     NULL);
AMBA_DEVICE(clcd,  "dev:20",  CLCD,     &clcd_plat_data);
AMBA_DEVICE(dmac,  "dev:30",  DMAC,     NULL);
AMBA_DEVICE(sctl,  "dev:e0",  SCTL,     NULL);
AMBA_DEVICE(wdog,  "dev:e1",  WATCHDOG, NULL);
AMBA_DEVICE(gpio0, "dev:e4",  GPIO0,    &gpio0_plat_data);
AMBA_DEVICE(gpio1, "dev:e5",  GPIO1,    &gpio1_plat_data);
AMBA_DEVICE(rtc,   "dev:e8",  RTC,      NULL);
AMBA_DEVICE(sci0,  "dev:f0",  SCI,      NULL);
AMBA_DEVICE(uart0, "dev:f1",  UART0,    NULL);
AMBA_DEVICE(uart1, "dev:f2",  UART1,    NULL);
AMBA_DEVICE(uart2, "dev:f3",  UART2,    NULL);
AMBA_DEVICE(ssp0,  "dev:f4",  SSP,      &ssp0_plat_data);

static struct amba_device *amba_devs[] __initdata = {
	&dmac_device,
	&uart0_device,
	&uart1_device,
	&uart2_device,
	&smc_device,
	&mpmc_device,
	&clcd_device,
	&sctl_device,
	&wdog_device,
	&gpio0_device,
	&gpio1_device,
	&rtc_device,
	&sci0_device,
	&ssp0_device,
	&aaci_device,
	&mmc0_device,
	&kmi0_device,
	&kmi1_device,
};

#ifdef CONFIG_LEDS
#define VA_LEDS_BASE (__io_address(VERSATILE_SYS_BASE) + VERSATILE_SYS_LED_OFFSET)

static void versatile_leds_event(led_event_t ledevt)
{
	unsigned long flags;
	u32 val;

	local_irq_save(flags);
	val = readl(VA_LEDS_BASE);

	switch (ledevt) {
	case led_idle_start:
		val = val & ~VERSATILE_SYS_LED0;
		break;

	case led_idle_end:
		val = val | VERSATILE_SYS_LED0;
		break;

	case led_timer:
		val = val ^ VERSATILE_SYS_LED1;
		break;

	case led_halted:
		val = 0;
		break;

	default:
		break;
	}

	writel(val, VA_LEDS_BASE);
	local_irq_restore(flags);
}
#endif	/* CONFIG_LEDS */

void __init versatile_init(void)
{
	int i;

	osc4_clk.vcoreg	= __io_address(VERSATILE_SYS_BASE) + VERSATILE_SYS_OSCCLCD_OFFSET;

	clkdev_add_table(lookups, ARRAY_SIZE(lookups));

	platform_device_register(&versatile_flash_device);
	platform_device_register(&versatile_i2c_device);
	platform_device_register(&smc91x_device);
	platform_device_register(&char_lcd_device);

	for (i = 0; i < ARRAY_SIZE(amba_devs); i++) {
		struct amba_device *d = amba_devs[i];
		amba_device_register(d, &iomem_resource);
	}

#ifdef CONFIG_LEDS
	leds_event = versatile_leds_event;
#endif
}

/*
 * Where is the timer (VA)?
 */
#define TIMER0_VA_BASE		 __io_address(VERSATILE_TIMER0_1_BASE)
#define TIMER1_VA_BASE		(__io_address(VERSATILE_TIMER0_1_BASE) + 0x20)
#define TIMER2_VA_BASE		 __io_address(VERSATILE_TIMER2_3_BASE)
#define TIMER3_VA_BASE		(__io_address(VERSATILE_TIMER2_3_BASE) + 0x20)

/*
 * Set up timer interrupt, and return the current time in seconds.
 */
static void __init versatile_timer_init(void)
{
	u32 val;

	/* 
	 * set clock frequency: 
	 *	VERSATILE_REFCLK is 32KHz
	 *	VERSATILE_TIMCLK is 1MHz
	 */
	val = readl(__io_address(VERSATILE_SCTL_BASE));
	writel((VERSATILE_TIMCLK << VERSATILE_TIMER1_EnSel) |
	       (VERSATILE_TIMCLK << VERSATILE_TIMER2_EnSel) | 
	       (VERSATILE_TIMCLK << VERSATILE_TIMER3_EnSel) |
	       (VERSATILE_TIMCLK << VERSATILE_TIMER4_EnSel) | val,
	       __io_address(VERSATILE_SCTL_BASE));

	/*
	 * Initialise to a known state (all timers off)
	 */
	writel(0, TIMER0_VA_BASE + TIMER_CTRL);
	writel(0, TIMER1_VA_BASE + TIMER_CTRL);
	writel(0, TIMER2_VA_BASE + TIMER_CTRL);
	writel(0, TIMER3_VA_BASE + TIMER_CTRL);

	sp804_clocksource_init(TIMER3_VA_BASE);
	sp804_clockevents_init(TIMER0_VA_BASE, IRQ_TIMERINT0_1);
}

struct sys_timer versatile_timer = {
	.init		= versatile_timer_init,
};
