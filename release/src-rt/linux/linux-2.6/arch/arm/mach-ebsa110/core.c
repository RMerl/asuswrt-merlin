/*
 *  linux/arch/arm/mach-ebsa110/core.c
 *
 *  Copyright (C) 1998-2001 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Extra MM routines for the EBSA-110 architecture
 */
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/serial_8250.h>
#include <linux/init.h>
#include <linux/io.h>

#include <mach/hardware.h>
#include <asm/irq.h>
#include <asm/setup.h>
#include <asm/mach-types.h>
#include <asm/pgtable.h>
#include <asm/page.h>
#include <asm/system.h>

#include <asm/mach/arch.h>
#include <asm/mach/irq.h>
#include <asm/mach/map.h>

#include <asm/mach/time.h>

#define IRQ_MASK		0xfe000000	/* read */
#define IRQ_MSET		0xfe000000	/* write */
#define IRQ_STAT		0xff000000	/* read */
#define IRQ_MCLR		0xff000000	/* write */

static void ebsa110_mask_irq(struct irq_data *d)
{
	__raw_writeb(1 << d->irq, IRQ_MCLR);
}

static void ebsa110_unmask_irq(struct irq_data *d)
{
	__raw_writeb(1 << d->irq, IRQ_MSET);
}

static struct irq_chip ebsa110_irq_chip = {
	.irq_ack	= ebsa110_mask_irq,
	.irq_mask	= ebsa110_mask_irq,
	.irq_unmask	= ebsa110_unmask_irq,
};
 
static void __init ebsa110_init_irq(void)
{
	unsigned long flags;
	unsigned int irq;

	local_irq_save(flags);
	__raw_writeb(0xff, IRQ_MCLR);
	__raw_writeb(0x55, IRQ_MSET);
	__raw_writeb(0x00, IRQ_MSET);
	if (__raw_readb(IRQ_MASK) != 0x55)
		while (1);
	__raw_writeb(0xff, IRQ_MCLR);	/* clear all interrupt enables */
	local_irq_restore(flags);

	for (irq = 0; irq < NR_IRQS; irq++) {
		irq_set_chip_and_handler(irq, &ebsa110_irq_chip,
					 handle_level_irq);
		set_irq_flags(irq, IRQF_VALID | IRQF_PROBE);
	}
}

static struct map_desc ebsa110_io_desc[] __initdata = {
	/*
	 * sparse external-decode ISAIO space
	 */
	{	/* IRQ_STAT/IRQ_MCLR */
		.virtual	= IRQ_STAT,
		.pfn		= __phys_to_pfn(TRICK4_PHYS),
		.length		= PGDIR_SIZE,
		.type		= MT_DEVICE
	}, {	/* IRQ_MASK/IRQ_MSET */
		.virtual	= IRQ_MASK,
		.pfn		= __phys_to_pfn(TRICK3_PHYS),
		.length		= PGDIR_SIZE,
		.type		= MT_DEVICE
	}, {	/* SOFT_BASE */
		.virtual	= SOFT_BASE,
		.pfn		= __phys_to_pfn(TRICK1_PHYS),
		.length		= PGDIR_SIZE,
		.type		= MT_DEVICE
	}, {	/* PIT_BASE */
		.virtual	= PIT_BASE,
		.pfn		= __phys_to_pfn(TRICK0_PHYS),
		.length		= PGDIR_SIZE,
		.type		= MT_DEVICE
	},

	/*
	 * self-decode ISAIO space
	 */
	{
		.virtual	= ISAIO_BASE,
		.pfn		= __phys_to_pfn(ISAIO_PHYS),
		.length		= ISAIO_SIZE,
		.type		= MT_DEVICE
	}, {
		.virtual	= ISAMEM_BASE,
		.pfn		= __phys_to_pfn(ISAMEM_PHYS),
		.length		= ISAMEM_SIZE,
		.type		= MT_DEVICE
	}
};

static void __init ebsa110_map_io(void)
{
	iotable_init(ebsa110_io_desc, ARRAY_SIZE(ebsa110_io_desc));
}


#define PIT_CTRL		(PIT_BASE + 0x0d)
#define PIT_T2			(PIT_BASE + 0x09)
#define PIT_T1			(PIT_BASE + 0x05)
#define PIT_T0			(PIT_BASE + 0x01)

/*
 * This is the rate at which your MCLK signal toggles (in Hz)
 * This was measured on a 10 digit frequency counter sampling
 * over 1 second.
 */
#define MCLK	47894000

/*
 * This is the rate at which the PIT timers get clocked
 */
#define CLKBY7	(MCLK / 7)

/*
 * This is the counter value.  We tick at 200Hz on this platform.
 */
#define COUNT	((CLKBY7 + (HZ / 2)) / HZ)

/*
 * Get the time offset from the system PIT.  Note that if we have missed an
 * interrupt, then the PIT counter will roll over (ie, be negative).
 * This actually works out to be convenient.
 */
static unsigned long ebsa110_gettimeoffset(void)
{
	unsigned long offset, count;

	__raw_writeb(0x40, PIT_CTRL);
	count = __raw_readb(PIT_T1);
	count |= __raw_readb(PIT_T1) << 8;

	/*
	 * If count > COUNT, make the number negative.
	 */
	if (count > COUNT)
		count |= 0xffff0000;

	offset = COUNT;
	offset -= count;

	/*
	 * `offset' is in units of timer counts.  Convert
	 * offset to units of microseconds.
	 */
	offset = offset * (1000000 / HZ) / COUNT;

	return offset;
}

static irqreturn_t
ebsa110_timer_interrupt(int irq, void *dev_id)
{
	u32 count;

	/* latch and read timer 1 */
	__raw_writeb(0x40, PIT_CTRL);
	count = __raw_readb(PIT_T1);
	count |= __raw_readb(PIT_T1) << 8;

	count += COUNT;

	__raw_writeb(count & 0xff, PIT_T1);
	__raw_writeb(count >> 8, PIT_T1);

	timer_tick();

	return IRQ_HANDLED;
}

static struct irqaction ebsa110_timer_irq = {
	.name		= "EBSA110 Timer Tick",
	.flags		= IRQF_DISABLED | IRQF_TIMER | IRQF_IRQPOLL,
	.handler	= ebsa110_timer_interrupt,
};

/*
 * Set up timer interrupt.
 */
static void __init ebsa110_timer_init(void)
{
	/*
	 * Timer 1, mode 2, LSB/MSB
	 */
	__raw_writeb(0x70, PIT_CTRL);
	__raw_writeb(COUNT & 0xff, PIT_T1);
	__raw_writeb(COUNT >> 8, PIT_T1);

	setup_irq(IRQ_EBSA110_TIMER0, &ebsa110_timer_irq);
}

static struct sys_timer ebsa110_timer = {
	.init		= ebsa110_timer_init,
	.offset		= ebsa110_gettimeoffset,
};

static struct plat_serial8250_port serial_platform_data[] = {
	{
		.iobase		= 0x3f8,
		.irq		= 1,
		.uartclk	= 1843200,
		.regshift	= 0,
		.iotype		= UPIO_PORT,
		.flags		= UPF_BOOT_AUTOCONF | UPF_SKIP_TEST,
	},
	{
		.iobase		= 0x2f8,
		.irq		= 2,
		.uartclk	= 1843200,
		.regshift	= 0,
		.iotype		= UPIO_PORT,
		.flags		= UPF_BOOT_AUTOCONF | UPF_SKIP_TEST,
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

static struct resource am79c961_resources[] = {
	{
		.start		= 0x220,
		.end		= 0x238,
		.flags		= IORESOURCE_IO,
	}, {
		.start		= IRQ_EBSA110_ETHERNET,
		.end		= IRQ_EBSA110_ETHERNET,
		.flags		= IORESOURCE_IRQ,
	},
};

static struct platform_device am79c961_device = {
	.name			= "am79c961",
	.id			= -1,
	.num_resources		= ARRAY_SIZE(am79c961_resources),
	.resource		= am79c961_resources,
};

static struct platform_device *ebsa110_devices[] = {
	&serial_device,
	&am79c961_device,
};

static int __init ebsa110_init(void)
{
	return platform_add_devices(ebsa110_devices, ARRAY_SIZE(ebsa110_devices));
}

arch_initcall(ebsa110_init);

MACHINE_START(EBSA110, "EBSA110")
	/* Maintainer: Russell King */
	.boot_params	= 0x00000400,
	.reserve_lp0	= 1,
	.reserve_lp2	= 1,
	.soft_reboot	= 1,
	.map_io		= ebsa110_map_io,
	.init_irq	= ebsa110_init_irq,
	.timer		= &ebsa110_timer,
MACHINE_END
