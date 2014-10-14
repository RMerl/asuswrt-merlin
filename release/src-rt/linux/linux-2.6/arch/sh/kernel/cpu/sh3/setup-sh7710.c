/*
 * SH3 Setup code for SH7710, SH7712
 *
 *  Copyright (C) 2006 - 2009  Paul Mundt
 *  Copyright (C) 2007  Nobuhiro Iwamatsu
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/serial.h>
#include <linux/serial_sci.h>
#include <linux/sh_timer.h>
#include <asm/rtc.h>

enum {
	UNUSED = 0,

	/* interrupt sources */
	IRQ0, IRQ1, IRQ2, IRQ3, IRQ4, IRQ5,
	DMAC1, SCIF0, SCIF1, DMAC2, IPSEC,
	EDMAC0, EDMAC1, EDMAC2,
	SIOF0, SIOF1,

	TMU0, TMU1, TMU2,
	RTC, WDT, REF,
};

static struct intc_vect vectors[] __initdata = {
	/* IRQ0->5 are handled in setup-sh3.c */
	INTC_VECT(DMAC1, 0x800), INTC_VECT(DMAC1, 0x820),
	INTC_VECT(DMAC1, 0x840), INTC_VECT(DMAC1, 0x860),
	INTC_VECT(SCIF0, 0x880), INTC_VECT(SCIF0, 0x8a0),
	INTC_VECT(SCIF0, 0x8c0), INTC_VECT(SCIF0, 0x8e0),
	INTC_VECT(SCIF1, 0x900), INTC_VECT(SCIF1, 0x920),
	INTC_VECT(SCIF1, 0x940), INTC_VECT(SCIF1, 0x960),
	INTC_VECT(DMAC2, 0xb80), INTC_VECT(DMAC2, 0xba0),
#ifdef CONFIG_CPU_SUBTYPE_SH7710
	INTC_VECT(IPSEC, 0xbe0),
#endif
	INTC_VECT(EDMAC0, 0xc00), INTC_VECT(EDMAC1, 0xc20),
	INTC_VECT(EDMAC2, 0xc40),
	INTC_VECT(SIOF0, 0xe00), INTC_VECT(SIOF0, 0xe20),
	INTC_VECT(SIOF0, 0xe40), INTC_VECT(SIOF0, 0xe60),
	INTC_VECT(SIOF1, 0xe80), INTC_VECT(SIOF1, 0xea0),
	INTC_VECT(SIOF1, 0xec0), INTC_VECT(SIOF1, 0xee0),
	INTC_VECT(TMU0, 0x400), INTC_VECT(TMU1, 0x420),
	INTC_VECT(TMU2, 0x440),
	INTC_VECT(RTC, 0x480), INTC_VECT(RTC, 0x4a0),
	INTC_VECT(RTC, 0x4c0),
	INTC_VECT(WDT, 0x560),
	INTC_VECT(REF, 0x580),
};

static struct intc_prio_reg prio_registers[] __initdata = {
	{ 0xfffffee2, 0, 16, 4, /* IPRA */ { TMU0, TMU1, TMU2, RTC } },
	{ 0xfffffee4, 0, 16, 4, /* IPRB */ { WDT, REF, 0, 0 } },
	{ 0xa4000016, 0, 16, 4, /* IPRC */ { IRQ3, IRQ2, IRQ1, IRQ0 } },
	{ 0xa4000018, 0, 16, 4, /* IPRD */ { 0, 0, IRQ5, IRQ4 } },
	{ 0xa400001a, 0, 16, 4, /* IPRE */ { DMAC1, SCIF0, SCIF1 } },
	{ 0xa4080000, 0, 16, 4, /* IPRF */ { IPSEC, DMAC2 } },
	{ 0xa4080002, 0, 16, 4, /* IPRG */ { EDMAC0, EDMAC1, EDMAC2 } },
	{ 0xa4080004, 0, 16, 4, /* IPRH */ { 0, 0, 0, SIOF0 } },
	{ 0xa4080006, 0, 16, 4, /* IPRI */ { 0, 0, SIOF1 } },
};

static DECLARE_INTC_DESC(intc_desc, "sh7710", vectors, NULL,
			 NULL, prio_registers, NULL);

static struct resource rtc_resources[] = {
	[0] =	{
		.start	= 0xa413fec0,
		.end	= 0xa413fec0 + 0x1e,
		.flags  = IORESOURCE_IO,
	},
	[1] =	{
		.start  = 20,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct sh_rtc_platform_info rtc_info = {
	.capabilities	= RTC_CAP_4_DIGIT_YEAR,
};

static struct platform_device rtc_device = {
	.name		= "sh-rtc",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(rtc_resources),
	.resource	= rtc_resources,
	.dev		= {
		.platform_data = &rtc_info,
	},
};

static struct plat_sci_port scif0_platform_data = {
	.mapbase	= 0xa4400000,
	.flags		= UPF_BOOT_AUTOCONF,
	.scscr		= SCSCR_TE | SCSCR_RE | SCSCR_REIE |
			  SCSCR_CKE1 | SCSCR_CKE0,
	.scbrr_algo_id	= SCBRR_ALGO_2,
	.type		= PORT_SCIF,
	.irqs		= { 52, 52, 52, 52 },
};

static struct platform_device scif0_device = {
	.name		= "sh-sci",
	.id		= 0,
	.dev		= {
		.platform_data	= &scif0_platform_data,
	},
};

static struct plat_sci_port scif1_platform_data = {
	.mapbase	= 0xa4410000,
	.flags		= UPF_BOOT_AUTOCONF,
	.scscr		= SCSCR_TE | SCSCR_RE | SCSCR_REIE |
			  SCSCR_CKE1 | SCSCR_CKE0,
	.scbrr_algo_id	= SCBRR_ALGO_2,
	.type		= PORT_SCIF,
	.irqs           = { 56, 56, 56, 56 },
};

static struct platform_device scif1_device = {
	.name		= "sh-sci",
	.id		= 1,
	.dev		= {
		.platform_data	= &scif1_platform_data,
	},
};

static struct sh_timer_config tmu0_platform_data = {
	.channel_offset = 0x02,
	.timer_bit = 0,
	.clockevent_rating = 200,
};

static struct resource tmu0_resources[] = {
	[0] = {
		.start	= 0xa412fe94,
		.end	= 0xa412fe9f,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= 16,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device tmu0_device = {
	.name		= "sh_tmu",
	.id		= 0,
	.dev = {
		.platform_data	= &tmu0_platform_data,
	},
	.resource	= tmu0_resources,
	.num_resources	= ARRAY_SIZE(tmu0_resources),
};

static struct sh_timer_config tmu1_platform_data = {
	.channel_offset = 0xe,
	.timer_bit = 1,
	.clocksource_rating = 200,
};

static struct resource tmu1_resources[] = {
	[0] = {
		.start	= 0xa412fea0,
		.end	= 0xa412feab,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= 17,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device tmu1_device = {
	.name		= "sh_tmu",
	.id		= 1,
	.dev = {
		.platform_data	= &tmu1_platform_data,
	},
	.resource	= tmu1_resources,
	.num_resources	= ARRAY_SIZE(tmu1_resources),
};

static struct sh_timer_config tmu2_platform_data = {
	.channel_offset = 0x1a,
	.timer_bit = 2,
};

static struct resource tmu2_resources[] = {
	[0] = {
		.start	= 0xa412feac,
		.end	= 0xa412feb5,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= 18,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device tmu2_device = {
	.name		= "sh_tmu",
	.id		= 2,
	.dev = {
		.platform_data	= &tmu2_platform_data,
	},
	.resource	= tmu2_resources,
	.num_resources	= ARRAY_SIZE(tmu2_resources),
};

static struct platform_device *sh7710_devices[] __initdata = {
	&scif0_device,
	&scif1_device,
	&tmu0_device,
	&tmu1_device,
	&tmu2_device,
	&rtc_device,
};

static int __init sh7710_devices_setup(void)
{
	return platform_add_devices(sh7710_devices,
				    ARRAY_SIZE(sh7710_devices));
}
arch_initcall(sh7710_devices_setup);

static struct platform_device *sh7710_early_devices[] __initdata = {
	&scif0_device,
	&scif1_device,
	&tmu0_device,
	&tmu1_device,
	&tmu2_device,
};

void __init plat_early_device_setup(void)
{
	early_platform_add_devices(sh7710_early_devices,
				   ARRAY_SIZE(sh7710_early_devices));
}

void __init plat_irq_setup(void)
{
	register_intc_controller(&intc_desc);
	plat_irq_setup_sh3();
}
