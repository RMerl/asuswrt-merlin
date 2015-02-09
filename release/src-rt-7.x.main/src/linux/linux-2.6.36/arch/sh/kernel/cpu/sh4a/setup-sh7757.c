/*
 * SH7757 Setup
 *
 * Copyright (C) 2009  Renesas Solutions Corp.
 *
 *  based on setup-sh7785.c : Copyright (C) 2007  Paul Mundt
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/serial.h>
#include <linux/serial_sci.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/sh_timer.h>

static struct plat_sci_port scif2_platform_data = {
	.mapbase	= 0xfe4b0000,		/* SCIF2 */
	.flags		= UPF_BOOT_AUTOCONF,
	.type		= PORT_SCIF,
	.irqs		= { 40, 40, 40, 40 },
};

static struct platform_device scif2_device = {
	.name		= "sh-sci",
	.id		= 2,
	.dev		= {
		.platform_data	= &scif2_platform_data,
	},
};

static struct plat_sci_port scif3_platform_data = {
	.mapbase	= 0xfe4c0000,		/* SCIF3 */
	.flags		= UPF_BOOT_AUTOCONF,
	.type		= PORT_SCIF,
	.irqs		= { 76, 76, 76, 76 },
};

static struct platform_device scif3_device = {
	.name		= "sh-sci",
	.id		= 3,
	.dev		= {
		.platform_data	= &scif3_platform_data,
	},
};

static struct plat_sci_port scif4_platform_data = {
	.mapbase	= 0xfe4d0000,		/* SCIF4 */
	.flags		= UPF_BOOT_AUTOCONF,
	.type		= PORT_SCIF,
	.irqs		= { 104, 104, 104, 104 },
};

static struct platform_device scif4_device = {
	.name		= "sh-sci",
	.id		= 4,
	.dev		= {
		.platform_data	= &scif4_platform_data,
	},
};

static struct sh_timer_config tmu0_platform_data = {
	.channel_offset = 0x04,
	.timer_bit = 0,
	.clockevent_rating = 200,
};

static struct resource tmu0_resources[] = {
	[0] = {
		.start	= 0xfe430008,
		.end	= 0xfe430013,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= 28,
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
	.channel_offset = 0x10,
	.timer_bit = 1,
	.clocksource_rating = 200,
};

static struct resource tmu1_resources[] = {
	[0] = {
		.start	= 0xfe430014,
		.end	= 0xfe43001f,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= 29,
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

static struct platform_device *sh7757_devices[] __initdata = {
	&scif2_device,
	&scif3_device,
	&scif4_device,
	&tmu0_device,
	&tmu1_device,
};

static int __init sh7757_devices_setup(void)
{
	return platform_add_devices(sh7757_devices,
				    ARRAY_SIZE(sh7757_devices));
}
arch_initcall(sh7757_devices_setup);

static struct platform_device *sh7757_early_devices[] __initdata = {
	&scif2_device,
	&scif3_device,
	&scif4_device,
	&tmu0_device,
	&tmu1_device,
};

void __init plat_early_device_setup(void)
{
	early_platform_add_devices(sh7757_early_devices,
				   ARRAY_SIZE(sh7757_early_devices));
}

enum {
	UNUSED = 0,

	/* interrupt sources */

	IRL0_LLLL, IRL0_LLLH, IRL0_LLHL, IRL0_LLHH,
	IRL0_LHLL, IRL0_LHLH, IRL0_LHHL, IRL0_LHHH,
	IRL0_HLLL, IRL0_HLLH, IRL0_HLHL, IRL0_HLHH,
	IRL0_HHLL, IRL0_HHLH, IRL0_HHHL,

	IRL4_LLLL, IRL4_LLLH, IRL4_LLHL, IRL4_LLHH,
	IRL4_LHLL, IRL4_LHLH, IRL4_LHHL, IRL4_LHHH,
	IRL4_HLLL, IRL4_HLLH, IRL4_HLHL, IRL4_HLHH,
	IRL4_HHLL, IRL4_HHLH, IRL4_HHHL,
	IRQ0, IRQ1, IRQ2, IRQ3, IRQ4, IRQ5, IRQ6, IRQ7,

	SDHI,
	DVC,
	IRQ8, IRQ9, IRQ10,
	WDT0,
	TMU0, TMU1, TMU2, TMU2_TICPI,
	HUDI,

	ARC4,
	DMAC0,
	IRQ11,
	SCIF2,
	DMAC1_6,
	USB0,
	IRQ12,
	JMC,
	SPI1,
	IRQ13, IRQ14,
	USB1,
	TMR01, TMR23, TMR45,
	WDT1,
	FRT,
	LPC,
	SCIF0, SCIF1, SCIF3,
	PECI0I, PECI1I, PECI2I,
	IRQ15,
	ETHERC,
	SPI0,
	ADC1,
	DMAC1_8,
	SIM,
	TMU3, TMU4, TMU5,
	ADC0,
	SCIF4,
	IIC0_0, IIC0_1, IIC0_2, IIC0_3,
	IIC1_0, IIC1_1, IIC1_2, IIC1_3,
	IIC2_0, IIC2_1, IIC2_2, IIC2_3,
	IIC3_0, IIC3_1, IIC3_2, IIC3_3,
	IIC4_0, IIC4_1, IIC4_2, IIC4_3,
	IIC5_0, IIC5_1, IIC5_2, IIC5_3,
	IIC6_0, IIC6_1, IIC6_2, IIC6_3,
	IIC7_0, IIC7_1, IIC7_2, IIC7_3,
	IIC8_0, IIC8_1, IIC8_2, IIC8_3,
	IIC9_0, IIC9_1, IIC9_2, IIC9_3,
	PCIINTA,
	PCIE,
	SGPIO,

	/* interrupt groups */

	TMU012, TMU345,
};

static struct intc_vect vectors[] __initdata = {
	INTC_VECT(SDHI, 0x480), INTC_VECT(SDHI, 0x04a0),
	INTC_VECT(SDHI, 0x4c0),
	INTC_VECT(DVC, 0x4e0),
	INTC_VECT(IRQ8, 0x500), INTC_VECT(IRQ9, 0x520),
	INTC_VECT(IRQ10, 0x540),
	INTC_VECT(WDT0, 0x560),
	INTC_VECT(TMU0, 0x580), INTC_VECT(TMU1, 0x5a0),
	INTC_VECT(TMU2, 0x5c0), INTC_VECT(TMU2_TICPI, 0x5e0),
	INTC_VECT(HUDI, 0x600),
	INTC_VECT(ARC4, 0x620),
	INTC_VECT(DMAC0, 0x640), INTC_VECT(DMAC0, 0x660),
	INTC_VECT(DMAC0, 0x680), INTC_VECT(DMAC0, 0x6a0),
	INTC_VECT(DMAC0, 0x6c0),
	INTC_VECT(IRQ11, 0x6e0),
	INTC_VECT(SCIF2, 0x700), INTC_VECT(SCIF2, 0x720),
	INTC_VECT(SCIF2, 0x740), INTC_VECT(SCIF2, 0x760),
	INTC_VECT(DMAC0, 0x780), INTC_VECT(DMAC0, 0x7a0),
	INTC_VECT(DMAC1_6, 0x7c0), INTC_VECT(DMAC1_6, 0x7e0),
	INTC_VECT(USB0, 0x840),
	INTC_VECT(IRQ12, 0x880),
	INTC_VECT(JMC, 0x8a0),
	INTC_VECT(SPI1, 0x8c0),
	INTC_VECT(IRQ13, 0x8e0), INTC_VECT(IRQ14, 0x900),
	INTC_VECT(USB1, 0x920),
	INTC_VECT(TMR01, 0xa00), INTC_VECT(TMR23, 0xa20),
	INTC_VECT(TMR45, 0xa40),
	INTC_VECT(WDT1, 0xa60),
	INTC_VECT(FRT, 0xa80),
	INTC_VECT(LPC, 0xaa0), INTC_VECT(LPC, 0xac0),
	INTC_VECT(LPC, 0xae0), INTC_VECT(LPC, 0xb00),
	INTC_VECT(LPC, 0xb20),
	INTC_VECT(SCIF0, 0xb40), INTC_VECT(SCIF1, 0xb60),
	INTC_VECT(SCIF3, 0xb80), INTC_VECT(SCIF3, 0xba0),
	INTC_VECT(SCIF3, 0xbc0), INTC_VECT(SCIF3, 0xbe0),
	INTC_VECT(PECI0I, 0xc00), INTC_VECT(PECI1I, 0xc20),
	INTC_VECT(PECI2I, 0xc40),
	INTC_VECT(IRQ15, 0xc60),
	INTC_VECT(ETHERC, 0xc80), INTC_VECT(ETHERC, 0xca0),
	INTC_VECT(SPI0, 0xcc0),
	INTC_VECT(ADC1, 0xce0),
	INTC_VECT(DMAC1_8, 0xd00), INTC_VECT(DMAC1_8, 0xd20),
	INTC_VECT(DMAC1_8, 0xd40), INTC_VECT(DMAC1_8, 0xd60),
	INTC_VECT(SIM, 0xd80), INTC_VECT(SIM, 0xda0),
	INTC_VECT(SIM, 0xdc0), INTC_VECT(SIM, 0xde0),
	INTC_VECT(TMU3, 0xe00), INTC_VECT(TMU4, 0xe20),
	INTC_VECT(TMU5, 0xe40),
	INTC_VECT(ADC0, 0xe60),
	INTC_VECT(SCIF4, 0xf00), INTC_VECT(SCIF4, 0xf20),
	INTC_VECT(SCIF4, 0xf40), INTC_VECT(SCIF4, 0xf60),
	INTC_VECT(IIC0_0, 0x1400), INTC_VECT(IIC0_1, 0x1420),
	INTC_VECT(IIC0_2, 0x1440), INTC_VECT(IIC0_3, 0x1460),
	INTC_VECT(IIC1_0, 0x1480), INTC_VECT(IIC1_1, 0x14e0),
	INTC_VECT(IIC1_2, 0x1500), INTC_VECT(IIC1_3, 0x1520),
	INTC_VECT(IIC2_0, 0x1540), INTC_VECT(IIC2_1, 0x1560),
	INTC_VECT(IIC2_2, 0x1580), INTC_VECT(IIC2_3, 0x1600),
	INTC_VECT(IIC3_0, 0x1620), INTC_VECT(IIC3_1, 0x1640),
	INTC_VECT(IIC3_2, 0x16e0), INTC_VECT(IIC3_3, 0x1700),
	INTC_VECT(IIC4_0, 0x17c0), INTC_VECT(IIC4_1, 0x1800),
	INTC_VECT(IIC4_2, 0x1820), INTC_VECT(IIC4_3, 0x1840),
	INTC_VECT(IIC5_0, 0x1860), INTC_VECT(IIC5_1, 0x1880),
	INTC_VECT(IIC5_2, 0x18a0), INTC_VECT(IIC5_3, 0x18c0),
	INTC_VECT(IIC6_0, 0x18e0), INTC_VECT(IIC6_1, 0x1900),
	INTC_VECT(IIC6_2, 0x1920), INTC_VECT(IIC6_3, 0x1980),
	INTC_VECT(IIC7_0, 0x19a0), INTC_VECT(IIC7_1, 0x1a00),
	INTC_VECT(IIC7_2, 0x1a20), INTC_VECT(IIC7_3, 0x1a40),
	INTC_VECT(IIC8_0, 0x1a60), INTC_VECT(IIC8_1, 0x1a80),
	INTC_VECT(IIC8_2, 0x1aa0), INTC_VECT(IIC8_3, 0x1b40),
	INTC_VECT(IIC9_0, 0x1b60), INTC_VECT(IIC9_1, 0x1b80),
	INTC_VECT(IIC9_2, 0x1c00), INTC_VECT(IIC9_3, 0x1c20),
	INTC_VECT(PCIINTA, 0x1ce0),
	INTC_VECT(PCIE, 0x1e00),
	INTC_VECT(SGPIO, 0x1f80),
	INTC_VECT(SGPIO, 0x1fa0),
};

static struct intc_group groups[] __initdata = {
	INTC_GROUP(TMU012, TMU0, TMU1, TMU2, TMU2_TICPI),
	INTC_GROUP(TMU345, TMU3, TMU4, TMU5),
};

static struct intc_mask_reg mask_registers[] __initdata = {
	{ 0xffd00044, 0xffd00064, 32, /* INTMSK0 / INTMSKCLR0 */
	  { IRQ0, IRQ1, IRQ2, IRQ3, IRQ4, IRQ5, IRQ6, IRQ7 } },

	{ 0xffd40080, 0xffd40084, 32, /* INTMSK2 / INTMSKCLR2 */
	  { IRL0_LLLL, IRL0_LLLH, IRL0_LLHL, IRL0_LLHH,
	    IRL0_LHLL, IRL0_LHLH, IRL0_LHHL, IRL0_LHHH,
	    IRL0_HLLL, IRL0_HLLH, IRL0_HLHL, IRL0_HLHH,
	    IRL0_HHLL, IRL0_HHLH, IRL0_HHHL, 0,
	    IRL4_LLLL, IRL4_LLLH, IRL4_LLHL, IRL4_LLHH,
	    IRL4_LHLL, IRL4_LHLH, IRL4_LHHL, IRL4_LHHH,
	    IRL4_HLLL, IRL4_HLLH, IRL4_HLHL, IRL4_HLHH,
	    IRL4_HHLL, IRL4_HHLH, IRL4_HHHL, 0, } },

	{ 0xffd40038, 0xffd4003c, 32, /* INT2MSKR / INT2MSKCR */
	  { 0, 0, 0, 0, 0, 0, 0, 0,
	    0, DMAC1_8, 0, PECI0I, LPC, FRT, WDT1, TMR45,
	    TMR23, TMR01, 0, 0, 0, 0, 0, DMAC0,
	    HUDI, 0, WDT0, SCIF3, SCIF2, SDHI, TMU345, TMU012
	     } },

	{ 0xffd400d0, 0xffd400d4, 32, /* INT2MSKR1 / INT2MSKCR1 */
	  { IRQ15, IRQ14, IRQ13, IRQ12, IRQ11, IRQ10, SCIF4, ETHERC,
	    IRQ9, IRQ8, SCIF1, SCIF0, USB0, 0, 0, USB1,
	    ADC1, 0, DMAC1_6, ADC0, SPI0, SIM, PECI2I, PECI1I,
	    ARC4, 0, SPI1, JMC, 0, 0, 0, DVC
	     } },

	{ 0xffd10038, 0xffd1003c, 32, /* INT2MSKR2 / INT2MSKCR2 */
	  { IIC4_1, IIC4_2, IIC5_0, 0, 0, 0, SGPIO, 0,
	    0, 0, 0, IIC9_2, IIC8_2, IIC8_1, IIC8_0, IIC7_3,
	    IIC7_2, IIC7_1, IIC6_3, IIC0_0, IIC0_1, IIC0_2, IIC0_3, IIC3_1,
	    IIC2_3, 0, IIC2_1, IIC9_1, IIC3_3, IIC1_0, PCIE, IIC2_2
	     } },

	{ 0xffd100d0, 0xff1400d4, 32, /* INT2MSKR3 / INT2MSKCR4 */
	  { 0, IIC6_1, IIC6_0, IIC5_1, IIC3_2, IIC2_0, 0, 0,
	    IIC1_3, IIC1_2, IIC9_0, IIC8_3, IIC4_3, IIC7_0, 0, IIC6_2,
	    PCIINTA, 0, IIC4_0, 0, 0, 0, 0, IIC9_3,
	    IIC3_0, 0, IIC5_3, IIC5_2, 0, 0, 0, IIC1_1
	     } },
};

#define INTPRI		0xffd00010
#define INT2PRI0	0xffd40000
#define INT2PRI1	0xffd40004
#define INT2PRI2	0xffd40008
#define INT2PRI3	0xffd4000c
#define INT2PRI4	0xffd40010
#define INT2PRI5	0xffd40014
#define INT2PRI6	0xffd40018
#define INT2PRI7	0xffd4001c
#define INT2PRI8	0xffd400a0
#define INT2PRI9	0xffd400a4
#define INT2PRI10	0xffd400a8
#define INT2PRI11	0xffd400ac
#define INT2PRI12	0xffd400b0
#define INT2PRI13	0xffd400b4
#define INT2PRI14	0xffd400b8
#define INT2PRI15	0xffd400bc
#define INT2PRI16	0xffd10000
#define INT2PRI17	0xffd10004
#define INT2PRI18	0xffd10008
#define INT2PRI19	0xffd1000c
#define INT2PRI20	0xffd10010
#define INT2PRI21	0xffd10014
#define INT2PRI22	0xffd10018
#define INT2PRI23	0xffd1001c
#define INT2PRI24	0xffd100a0
#define INT2PRI25	0xffd100a4
#define INT2PRI26	0xffd100a8
#define INT2PRI27	0xffd100ac
#define INT2PRI28	0xffd100b0
#define INT2PRI29	0xffd100b4
#define INT2PRI30	0xffd100b8
#define INT2PRI31	0xffd100bc

static struct intc_prio_reg prio_registers[] __initdata = {
	{ INTPRI, 0, 32, 4, { IRQ0, IRQ1, IRQ2, IRQ3,
			      IRQ4, IRQ5, IRQ6, IRQ7 } },

	{ INT2PRI0, 0, 32, 8, { TMU0, TMU1, TMU2, TMU2_TICPI } },
	{ INT2PRI1, 0, 32, 8, { TMU3, TMU4, TMU5, SDHI } },
	{ INT2PRI2, 0, 32, 8, { SCIF2, SCIF3, WDT0, IRQ8 } },
	{ INT2PRI3, 0, 32, 8, { HUDI, DMAC0, ADC0, IRQ9 } },
	{ INT2PRI4, 0, 32, 8, { IRQ10, 0, TMR01, TMR23 } },
	{ INT2PRI5, 0, 32, 8, { TMR45, WDT1, FRT, LPC } },
	{ INT2PRI6, 0, 32, 8, { PECI0I, ETHERC, DMAC1_8, 0 } },
	{ INT2PRI7, 0, 32, 8, { SCIF4, 0, IRQ11, IRQ12 } },
	{ INT2PRI8, 0, 32, 8, { 0, 0, 0, DVC } },
	{ INT2PRI9, 0, 32, 8, { ARC4, 0, SPI1, JMC } },
	{ INT2PRI10, 0, 32, 8, { SPI0, SIM, PECI2I, PECI1I } },
	{ INT2PRI11, 0, 32, 8, { ADC1, IRQ13, DMAC1_6, IRQ14 } },
	{ INT2PRI12, 0, 32, 8, { USB0, 0, IRQ15, USB1 } },
	{ INT2PRI13, 0, 32, 8, { 0, 0, SCIF1, SCIF0 } },

	{ INT2PRI16, 0, 32, 8, { IIC2_2, 0, 0, 0 } },
	{ INT2PRI17, 0, 32, 8, { PCIE, 0, 0, IIC1_0 } },
	{ INT2PRI18, 0, 32, 8, { IIC3_3, IIC9_1, IIC2_1, IIC1_2 } },
	{ INT2PRI19, 0, 32, 8, { IIC2_3, IIC3_1, 0, IIC1_3 } },
	{ INT2PRI20, 0, 32, 8, { IIC2_0, IIC6_3, IIC7_1, IIC7_2 } },
	{ INT2PRI21, 0, 32, 8, { IIC7_3, IIC8_0, IIC8_1, IIC8_2 } },
	{ INT2PRI22, 0, 32, 8, { IIC9_2, 0, 0, 0 } },
	{ INT2PRI23, 0, 32, 8, { 0, SGPIO, IIC3_2, IIC5_1 } },
	{ INT2PRI24, 0, 32, 8, { 0, 0, 0, IIC1_1 } },
	{ INT2PRI25, 0, 32, 8, { IIC3_0, 0, IIC5_3, IIC5_2 } },
	{ INT2PRI26, 0, 32, 8, { 0, 0, 0, IIC9_3 } },
	{ INT2PRI27, 0, 32, 8, { PCIINTA, IIC6_0, IIC4_0, IIC6_1 } },
	{ INT2PRI28, 0, 32, 8, { IIC4_3, IIC7_0, 0, IIC6_2 } },
	{ INT2PRI29, 0, 32, 8, { 0, 0, IIC9_0, IIC8_3 } },
	{ INT2PRI30, 0, 32, 8, { IIC4_1, IIC4_2, IIC5_0, 0 } },
	{ INT2PRI31, 0, 32, 8, { IIC0_0, IIC0_1, IIC0_2, IIC0_3 } },
};

static DECLARE_INTC_DESC(intc_desc, "sh7757", vectors, groups,
			 mask_registers, prio_registers, NULL);

/* Support for external interrupt pins in IRQ mode */
static struct intc_vect vectors_irq0123[] __initdata = {
	INTC_VECT(IRQ0, 0x240), INTC_VECT(IRQ1, 0x280),
	INTC_VECT(IRQ2, 0x2c0), INTC_VECT(IRQ3, 0x300),
};

static struct intc_vect vectors_irq4567[] __initdata = {
	INTC_VECT(IRQ4, 0x340), INTC_VECT(IRQ5, 0x380),
	INTC_VECT(IRQ6, 0x3c0), INTC_VECT(IRQ7, 0x200),
};

static struct intc_sense_reg sense_registers[] __initdata = {
	{ 0xffd0001c, 32, 2, /* ICR1 */   { IRQ0, IRQ1, IRQ2, IRQ3,
					    IRQ4, IRQ5, IRQ6, IRQ7 } },
};

static struct intc_mask_reg ack_registers[] __initdata = {
	{ 0xffd00024, 0, 32, /* INTREQ */
	  { IRQ0, IRQ1, IRQ2, IRQ3, IRQ4, IRQ5, IRQ6, IRQ7 } },
};

static DECLARE_INTC_DESC_ACK(intc_desc_irq0123, "sh7757-irq0123",
			     vectors_irq0123, NULL, mask_registers,
			     prio_registers, sense_registers, ack_registers);

static DECLARE_INTC_DESC_ACK(intc_desc_irq4567, "sh7757-irq4567",
			     vectors_irq4567, NULL, mask_registers,
			     prio_registers, sense_registers, ack_registers);

/* External interrupt pins in IRL mode */
static struct intc_vect vectors_irl0123[] __initdata = {
	INTC_VECT(IRL0_LLLL, 0x200), INTC_VECT(IRL0_LLLH, 0x220),
	INTC_VECT(IRL0_LLHL, 0x240), INTC_VECT(IRL0_LLHH, 0x260),
	INTC_VECT(IRL0_LHLL, 0x280), INTC_VECT(IRL0_LHLH, 0x2a0),
	INTC_VECT(IRL0_LHHL, 0x2c0), INTC_VECT(IRL0_LHHH, 0x2e0),
	INTC_VECT(IRL0_HLLL, 0x300), INTC_VECT(IRL0_HLLH, 0x320),
	INTC_VECT(IRL0_HLHL, 0x340), INTC_VECT(IRL0_HLHH, 0x360),
	INTC_VECT(IRL0_HHLL, 0x380), INTC_VECT(IRL0_HHLH, 0x3a0),
	INTC_VECT(IRL0_HHHL, 0x3c0),
};

static struct intc_vect vectors_irl4567[] __initdata = {
	INTC_VECT(IRL4_LLLL, 0xb00), INTC_VECT(IRL4_LLLH, 0xb20),
	INTC_VECT(IRL4_LLHL, 0xb40), INTC_VECT(IRL4_LLHH, 0xb60),
	INTC_VECT(IRL4_LHLL, 0xb80), INTC_VECT(IRL4_LHLH, 0xba0),
	INTC_VECT(IRL4_LHHL, 0xbc0), INTC_VECT(IRL4_LHHH, 0xbe0),
	INTC_VECT(IRL4_HLLL, 0xc00), INTC_VECT(IRL4_HLLH, 0xc20),
	INTC_VECT(IRL4_HLHL, 0xc40), INTC_VECT(IRL4_HLHH, 0xc60),
	INTC_VECT(IRL4_HHLL, 0xc80), INTC_VECT(IRL4_HHLH, 0xca0),
	INTC_VECT(IRL4_HHHL, 0xcc0),
};

static DECLARE_INTC_DESC(intc_desc_irl0123, "sh7757-irl0123", vectors_irl0123,
			 NULL, mask_registers, NULL, NULL);

static DECLARE_INTC_DESC(intc_desc_irl4567, "sh7757-irl4567", vectors_irl4567,
			 NULL, mask_registers, NULL, NULL);

#define INTC_ICR0	0xffd00000
#define INTC_INTMSK0	0xffd00044
#define INTC_INTMSK1	0xffd00048
#define INTC_INTMSK2	0xffd40080
#define INTC_INTMSKCLR1	0xffd00068
#define INTC_INTMSKCLR2	0xffd40084

void __init plat_irq_setup(void)
{
	/* disable IRQ3-0 + IRQ7-4 */
	__raw_writel(0xff000000, INTC_INTMSK0);

	/* disable IRL3-0 + IRL7-4 */
	__raw_writel(0xc0000000, INTC_INTMSK1);
	__raw_writel(0xfffefffe, INTC_INTMSK2);

	/* select IRL mode for IRL3-0 + IRL7-4 */
	__raw_writel(__raw_readl(INTC_ICR0) & ~0x00c00000, INTC_ICR0);

	/* disable holding function, ie enable "SH-4 Mode" */
	__raw_writel(__raw_readl(INTC_ICR0) | 0x00200000, INTC_ICR0);

	register_intc_controller(&intc_desc);
}

void __init plat_irq_setup_pins(int mode)
{
	switch (mode) {
	case IRQ_MODE_IRQ7654:
		/* select IRQ mode for IRL7-4 */
		__raw_writel(__raw_readl(INTC_ICR0) | 0x00400000, INTC_ICR0);
		register_intc_controller(&intc_desc_irq4567);
		break;
	case IRQ_MODE_IRQ3210:
		/* select IRQ mode for IRL3-0 */
		__raw_writel(__raw_readl(INTC_ICR0) | 0x00800000, INTC_ICR0);
		register_intc_controller(&intc_desc_irq0123);
		break;
	case IRQ_MODE_IRL7654:
		/* enable IRL7-4 but don't provide any masking */
		__raw_writel(0x40000000, INTC_INTMSKCLR1);
		__raw_writel(0x0000fffe, INTC_INTMSKCLR2);
		break;
	case IRQ_MODE_IRL3210:
		/* enable IRL0-3 but don't provide any masking */
		__raw_writel(0x80000000, INTC_INTMSKCLR1);
		__raw_writel(0xfffe0000, INTC_INTMSKCLR2);
		break;
	case IRQ_MODE_IRL7654_MASK:
		/* enable IRL7-4 and mask using cpu intc controller */
		__raw_writel(0x40000000, INTC_INTMSKCLR1);
		register_intc_controller(&intc_desc_irl4567);
		break;
	case IRQ_MODE_IRL3210_MASK:
		/* enable IRL0-3 and mask using cpu intc controller */
		__raw_writel(0x80000000, INTC_INTMSKCLR1);
		register_intc_controller(&intc_desc_irl0123);
		break;
	default:
		BUG();
	}
}

void __init plat_mem_setup(void)
{
}
