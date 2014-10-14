/*
 *  linux/arch/arm/mach-rpc/riscpc.c
 *
 *  Copyright (C) 1998-2001 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Architecture specific fixups.
 */
#include <linux/kernel.h>
#include <linux/tty.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/device.h>
#include <linux/serial_8250.h>
#include <linux/ata_platform.h>
#include <linux/io.h>
#include <linux/i2c.h>

#include <asm/elf.h>
#include <asm/mach-types.h>
#include <mach/hardware.h>
#include <asm/page.h>
#include <asm/domain.h>
#include <asm/setup.h>

#include <asm/mach/map.h>
#include <asm/mach/arch.h>
#include <asm/mach/time.h>

extern void rpc_init_irq(void);

unsigned int vram_size;
unsigned int memc_ctrl_reg;
unsigned int number_mfm_drives;

static int __init parse_tag_acorn(const struct tag *tag)
{
	memc_ctrl_reg = tag->u.acorn.memc_control_reg;
	number_mfm_drives = tag->u.acorn.adfsdrives;

	switch (tag->u.acorn.vram_pages) {
	case 512:
		vram_size += PAGE_SIZE * 256;
	case 256:
		vram_size += PAGE_SIZE * 256;
	default:
		break;
	}
#if 0
	if (vram_size) {
		desc->video_start = 0x02000000;
		desc->video_end   = 0x02000000 + vram_size;
	}
#endif
	return 0;
}

__tagtable(ATAG_ACORN, parse_tag_acorn);

static struct map_desc rpc_io_desc[] __initdata = {
 	{	/* VRAM		*/
		.virtual	=  SCREEN_BASE,
		.pfn		= __phys_to_pfn(SCREEN_START),
		.length		= 	2*1048576,
		.type		= MT_DEVICE
	}, {	/* IO space	*/
		.virtual	=  (u32)IO_BASE,
		.pfn		= __phys_to_pfn(IO_START),
		.length		= 	IO_SIZE	 ,
		.type		= MT_DEVICE
	}, {	/* EASI space	*/
		.virtual	= EASI_BASE,
		.pfn		= __phys_to_pfn(EASI_START),
		.length		= EASI_SIZE,
		.type		= MT_DEVICE
	}
};

static void __init rpc_map_io(void)
{
	iotable_init(rpc_io_desc, ARRAY_SIZE(rpc_io_desc));

	/*
	 * Turn off floppy.
	 */
	writeb(0xc, PCIO_BASE + (0x3f2 << 2));

	/*
	 * RiscPC can't handle half-word loads and stores
	 */
	elf_hwcap &= ~HWCAP_HALF;
}

static struct resource acornfb_resources[] = {
	{	/* VIDC */
		.start		= 0x03400000,
		.end		= 0x035fffff,
		.flags		= IORESOURCE_MEM,
	}, {
		.start		= IRQ_VSYNCPULSE,
		.end		= IRQ_VSYNCPULSE,
		.flags		= IORESOURCE_IRQ,
	},
};

static struct platform_device acornfb_device = {
	.name			= "acornfb",
	.id			= -1,
	.dev			= {
		.coherent_dma_mask = 0xffffffff,
	},
	.num_resources		= ARRAY_SIZE(acornfb_resources),
	.resource		= acornfb_resources,
};

static struct resource iomd_resources[] = {
	{
		.start		= 0x03200000,
		.end		= 0x0320ffff,
		.flags		= IORESOURCE_MEM,
	},
};

static struct platform_device iomd_device = {
	.name			= "iomd",
	.id			= -1,
	.num_resources		= ARRAY_SIZE(iomd_resources),
	.resource		= iomd_resources,
};

static struct platform_device kbd_device = {
	.name			= "kart",
	.id			= -1,
	.dev			= {
		.parent 	= &iomd_device.dev,
	},
};

static struct plat_serial8250_port serial_platform_data[] = {
	{
		.mapbase	= 0x03010fe0,
		.irq		= 10,
		.uartclk	= 1843200,
		.regshift	= 2,
		.iotype		= UPIO_MEM,
		.flags		= UPF_BOOT_AUTOCONF | UPF_IOREMAP | UPF_SKIP_TEST,
	},
	{ },
};

static struct platform_device serial_device = {
	.name			= "serial8250",
	.id			= PLAT8250_DEV_PLATFORM,
	.dev			= {
		.platform_data	= serial_platform_data,
	},
};

static struct pata_platform_info pata_platform_data = {
	.ioport_shift		= 2,
};

static struct resource pata_resources[] = {
	[0] = {
		.start		= 0x030107c0,
		.end		= 0x030107df,
		.flags		= IORESOURCE_MEM,
	},
	[1] = {
		.start		= 0x03010fd8,
		.end		= 0x03010fdb,
		.flags		= IORESOURCE_MEM,
	},
	[2] = {
		.start		= IRQ_HARDDISK,
		.end		= IRQ_HARDDISK,
		.flags		= IORESOURCE_IRQ,
	},
};

static struct platform_device pata_device = {
	.name			= "pata_platform",
	.id			= -1,
	.num_resources		= ARRAY_SIZE(pata_resources),
	.resource		= pata_resources,
	.dev			= {
		.platform_data	= &pata_platform_data,
		.coherent_dma_mask = ~0,	/* grumble */
	},
};

static struct platform_device *devs[] __initdata = {
	&iomd_device,
	&kbd_device,
	&serial_device,
	&acornfb_device,
	&pata_device,
};

static struct i2c_board_info i2c_rtc = {
	I2C_BOARD_INFO("pcf8583", 0x50)
};

static int __init rpc_init(void)
{
	i2c_register_board_info(0, &i2c_rtc, 1);
	return platform_add_devices(devs, ARRAY_SIZE(devs));
}

arch_initcall(rpc_init);

extern struct sys_timer ioc_timer;

MACHINE_START(RISCPC, "Acorn-RiscPC")
	/* Maintainer: Russell King */
	.boot_params	= 0x10000100,
	.reserve_lp0	= 1,
	.reserve_lp1	= 1,
	.map_io		= rpc_map_io,
	.init_irq	= rpc_init_irq,
	.timer		= &ioc_timer,
MACHINE_END
