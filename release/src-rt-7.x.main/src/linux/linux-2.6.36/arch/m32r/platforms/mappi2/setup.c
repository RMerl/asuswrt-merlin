/*
 *  linux/arch/m32r/platforms/mappi2/setup.c
 *
 *  Setup routines for Renesas MAPPI-II(M3A-ZA36) Board
 *
 *  Copyright (c) 2001-2005  Hiroyuki Kondo, Hirokazu Takata,
 *                           Hitoshi Yamamoto, Mamoru Sakugawa
 */

#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>

#include <asm/system.h>
#include <asm/m32r.h>
#include <asm/io.h>

#define irq2port(x) (M32R_ICU_CR1_PORTL + ((x - 1) * sizeof(unsigned long)))

icu_data_t icu_data[NR_IRQS];

static void disable_mappi2_irq(unsigned int irq)
{
	unsigned long port, data;

	if ((irq == 0) ||(irq >= NR_IRQS))  {
		printk("bad irq 0x%08x\n", irq);
		return;
	}
	port = irq2port(irq);
	data = icu_data[irq].icucr|M32R_ICUCR_ILEVEL7;
	outl(data, port);
}

static void enable_mappi2_irq(unsigned int irq)
{
	unsigned long port, data;

	if ((irq == 0) ||(irq >= NR_IRQS))  {
		printk("bad irq 0x%08x\n", irq);
		return;
	}
	port = irq2port(irq);
	data = icu_data[irq].icucr|M32R_ICUCR_IEN|M32R_ICUCR_ILEVEL6;
	outl(data, port);
}

static void mask_and_ack_mappi2(unsigned int irq)
{
	disable_mappi2_irq(irq);
}

static void end_mappi2_irq(unsigned int irq)
{
	enable_mappi2_irq(irq);
}

static unsigned int startup_mappi2_irq(unsigned int irq)
{
	enable_mappi2_irq(irq);
	return (0);
}

static void shutdown_mappi2_irq(unsigned int irq)
{
	unsigned long port;

	port = irq2port(irq);
	outl(M32R_ICUCR_ILEVEL7, port);
}

static struct irq_chip mappi2_irq_type =
{
	.typename = "MAPPI2-IRQ",
	.startup = startup_mappi2_irq,
	.shutdown = shutdown_mappi2_irq,
	.enable = enable_mappi2_irq,
	.disable = disable_mappi2_irq,
	.ack = mask_and_ack_mappi2,
	.end = end_mappi2_irq
};

void __init init_IRQ(void)
{
#if defined(CONFIG_SMC91X)
	/* INT0 : LAN controller (SMC91111) */
	irq_desc[M32R_IRQ_INT0].status = IRQ_DISABLED;
	irq_desc[M32R_IRQ_INT0].chip = &mappi2_irq_type;
	irq_desc[M32R_IRQ_INT0].action = 0;
	irq_desc[M32R_IRQ_INT0].depth = 1;
	icu_data[M32R_IRQ_INT0].icucr = M32R_ICUCR_IEN|M32R_ICUCR_ISMOD10;
	disable_mappi2_irq(M32R_IRQ_INT0);
#endif  /* CONFIG_SMC91X */

	/* MFT2 : system timer */
	irq_desc[M32R_IRQ_MFT2].status = IRQ_DISABLED;
	irq_desc[M32R_IRQ_MFT2].chip = &mappi2_irq_type;
	irq_desc[M32R_IRQ_MFT2].action = 0;
	irq_desc[M32R_IRQ_MFT2].depth = 1;
	icu_data[M32R_IRQ_MFT2].icucr = M32R_ICUCR_IEN;
	disable_mappi2_irq(M32R_IRQ_MFT2);

#ifdef CONFIG_SERIAL_M32R_SIO
	/* SIO0_R : uart receive data */
	irq_desc[M32R_IRQ_SIO0_R].status = IRQ_DISABLED;
	irq_desc[M32R_IRQ_SIO0_R].chip = &mappi2_irq_type;
	irq_desc[M32R_IRQ_SIO0_R].action = 0;
	irq_desc[M32R_IRQ_SIO0_R].depth = 1;
	icu_data[M32R_IRQ_SIO0_R].icucr = 0;
	disable_mappi2_irq(M32R_IRQ_SIO0_R);

	/* SIO0_S : uart send data */
	irq_desc[M32R_IRQ_SIO0_S].status = IRQ_DISABLED;
	irq_desc[M32R_IRQ_SIO0_S].chip = &mappi2_irq_type;
	irq_desc[M32R_IRQ_SIO0_S].action = 0;
	irq_desc[M32R_IRQ_SIO0_S].depth = 1;
	icu_data[M32R_IRQ_SIO0_S].icucr = 0;
	disable_mappi2_irq(M32R_IRQ_SIO0_S);
	/* SIO1_R : uart receive data */
	irq_desc[M32R_IRQ_SIO1_R].status = IRQ_DISABLED;
	irq_desc[M32R_IRQ_SIO1_R].chip = &mappi2_irq_type;
	irq_desc[M32R_IRQ_SIO1_R].action = 0;
	irq_desc[M32R_IRQ_SIO1_R].depth = 1;
	icu_data[M32R_IRQ_SIO1_R].icucr = 0;
	disable_mappi2_irq(M32R_IRQ_SIO1_R);

	/* SIO1_S : uart send data */
	irq_desc[M32R_IRQ_SIO1_S].status = IRQ_DISABLED;
	irq_desc[M32R_IRQ_SIO1_S].chip = &mappi2_irq_type;
	irq_desc[M32R_IRQ_SIO1_S].action = 0;
	irq_desc[M32R_IRQ_SIO1_S].depth = 1;
	icu_data[M32R_IRQ_SIO1_S].icucr = 0;
	disable_mappi2_irq(M32R_IRQ_SIO1_S);
#endif  /* CONFIG_M32R_USE_DBG_CONSOLE */

#if defined(CONFIG_USB)
	/* INT1 : USB Host controller interrupt */
	irq_desc[M32R_IRQ_INT1].status = IRQ_DISABLED;
	irq_desc[M32R_IRQ_INT1].chip = &mappi2_irq_type;
	irq_desc[M32R_IRQ_INT1].action = 0;
	irq_desc[M32R_IRQ_INT1].depth = 1;
	icu_data[M32R_IRQ_INT1].icucr = M32R_ICUCR_ISMOD01;
	disable_mappi2_irq(M32R_IRQ_INT1);
#endif /* CONFIG_USB */

	/* ICUCR40: CFC IREQ */
	irq_desc[PLD_IRQ_CFIREQ].status = IRQ_DISABLED;
	irq_desc[PLD_IRQ_CFIREQ].chip = &mappi2_irq_type;
	irq_desc[PLD_IRQ_CFIREQ].action = 0;
	irq_desc[PLD_IRQ_CFIREQ].depth = 1;	/* disable nested irq */
	icu_data[PLD_IRQ_CFIREQ].icucr = M32R_ICUCR_IEN|M32R_ICUCR_ISMOD01;
	disable_mappi2_irq(PLD_IRQ_CFIREQ);

#if defined(CONFIG_M32R_CFC)
	/* ICUCR41: CFC Insert */
	irq_desc[PLD_IRQ_CFC_INSERT].status = IRQ_DISABLED;
	irq_desc[PLD_IRQ_CFC_INSERT].chip = &mappi2_irq_type;
	irq_desc[PLD_IRQ_CFC_INSERT].action = 0;
	irq_desc[PLD_IRQ_CFC_INSERT].depth = 1;	/* disable nested irq */
	icu_data[PLD_IRQ_CFC_INSERT].icucr = M32R_ICUCR_IEN|M32R_ICUCR_ISMOD00;
	disable_mappi2_irq(PLD_IRQ_CFC_INSERT);

	/* ICUCR42: CFC Eject */
	irq_desc[PLD_IRQ_CFC_EJECT].status = IRQ_DISABLED;
	irq_desc[PLD_IRQ_CFC_EJECT].chip = &mappi2_irq_type;
	irq_desc[PLD_IRQ_CFC_EJECT].action = 0;
	irq_desc[PLD_IRQ_CFC_EJECT].depth = 1;	/* disable nested irq */
	icu_data[PLD_IRQ_CFC_EJECT].icucr = M32R_ICUCR_IEN|M32R_ICUCR_ISMOD10;
	disable_mappi2_irq(PLD_IRQ_CFC_EJECT);
#endif /* CONFIG_MAPPI2_CFC */
}

#define LAN_IOSTART     0x300
#define LAN_IOEND       0x320
static struct resource smc91x_resources[] = {
	[0] = {
		.start  = (LAN_IOSTART),
		.end    = (LAN_IOEND),
		.flags  = IORESOURCE_MEM,
	},
	[1] = {
		.start  = M32R_IRQ_INT0,
		.end    = M32R_IRQ_INT0,
		.flags  = IORESOURCE_IRQ,
	}
};

static struct platform_device smc91x_device = {
	.name		= "smc91x",
	.id		= 0,
	.num_resources  = ARRAY_SIZE(smc91x_resources),
	.resource       = smc91x_resources,
};

static int __init platform_init(void)
{
	platform_device_register(&smc91x_device);
	return 0;
}
arch_initcall(platform_init);
