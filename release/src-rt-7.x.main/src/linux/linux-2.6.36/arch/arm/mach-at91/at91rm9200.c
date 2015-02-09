/*
 * arch/arm/mach-at91/at91rm9200.c
 *
 *  Copyright (C) 2005 SAN People
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#include <linux/module.h>

#include <asm/irq.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <mach/at91rm9200.h>
#include <mach/at91_pmc.h>
#include <mach/at91_st.h>

#include "generic.h"
#include "clock.h"

static struct map_desc at91rm9200_io_desc[] __initdata = {
	{
		.virtual	= AT91_VA_BASE_SYS,
		.pfn		= __phys_to_pfn(AT91_BASE_SYS),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= AT91_VA_BASE_EMAC,
		.pfn		= __phys_to_pfn(AT91RM9200_BASE_EMAC),
		.length		= SZ_16K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= AT91_IO_VIRT_BASE - AT91RM9200_SRAM_SIZE,
		.pfn		= __phys_to_pfn(AT91RM9200_SRAM_BASE),
		.length		= AT91RM9200_SRAM_SIZE,
		.type		= MT_DEVICE,
	},
};

/* --------------------------------------------------------------------
 *  Clocks
 * -------------------------------------------------------------------- */

/*
 * The peripheral clocks.
 */
static struct clk udc_clk = {
	.name		= "udc_clk",
	.pmc_mask	= 1 << AT91RM9200_ID_UDP,
	.type		= CLK_TYPE_PERIPHERAL,
};
static struct clk ohci_clk = {
	.name		= "ohci_clk",
	.pmc_mask	= 1 << AT91RM9200_ID_UHP,
	.type		= CLK_TYPE_PERIPHERAL,
};
static struct clk ether_clk = {
	.name		= "ether_clk",
	.pmc_mask	= 1 << AT91RM9200_ID_EMAC,
	.type		= CLK_TYPE_PERIPHERAL,
};
static struct clk mmc_clk = {
	.name		= "mci_clk",
	.pmc_mask	= 1 << AT91RM9200_ID_MCI,
	.type		= CLK_TYPE_PERIPHERAL,
};
static struct clk twi_clk = {
	.name		= "twi_clk",
	.pmc_mask	= 1 << AT91RM9200_ID_TWI,
	.type		= CLK_TYPE_PERIPHERAL,
};
static struct clk usart0_clk = {
	.name		= "usart0_clk",
	.pmc_mask	= 1 << AT91RM9200_ID_US0,
	.type		= CLK_TYPE_PERIPHERAL,
};
static struct clk usart1_clk = {
	.name		= "usart1_clk",
	.pmc_mask	= 1 << AT91RM9200_ID_US1,
	.type		= CLK_TYPE_PERIPHERAL,
};
static struct clk usart2_clk = {
	.name		= "usart2_clk",
	.pmc_mask	= 1 << AT91RM9200_ID_US2,
	.type		= CLK_TYPE_PERIPHERAL,
};
static struct clk usart3_clk = {
	.name		= "usart3_clk",
	.pmc_mask	= 1 << AT91RM9200_ID_US3,
	.type		= CLK_TYPE_PERIPHERAL,
};
static struct clk spi_clk = {
	.name		= "spi_clk",
	.pmc_mask	= 1 << AT91RM9200_ID_SPI,
	.type		= CLK_TYPE_PERIPHERAL,
};
static struct clk pioA_clk = {
	.name		= "pioA_clk",
	.pmc_mask	= 1 << AT91RM9200_ID_PIOA,
	.type		= CLK_TYPE_PERIPHERAL,
};
static struct clk pioB_clk = {
	.name		= "pioB_clk",
	.pmc_mask	= 1 << AT91RM9200_ID_PIOB,
	.type		= CLK_TYPE_PERIPHERAL,
};
static struct clk pioC_clk = {
	.name		= "pioC_clk",
	.pmc_mask	= 1 << AT91RM9200_ID_PIOC,
	.type		= CLK_TYPE_PERIPHERAL,
};
static struct clk pioD_clk = {
	.name		= "pioD_clk",
	.pmc_mask	= 1 << AT91RM9200_ID_PIOD,
	.type		= CLK_TYPE_PERIPHERAL,
};
static struct clk ssc0_clk = {
	.name		= "ssc0_clk",
	.pmc_mask	= 1 << AT91RM9200_ID_SSC0,
	.type		= CLK_TYPE_PERIPHERAL,
};
static struct clk ssc1_clk = {
	.name		= "ssc1_clk",
	.pmc_mask	= 1 << AT91RM9200_ID_SSC1,
	.type		= CLK_TYPE_PERIPHERAL,
};
static struct clk ssc2_clk = {
	.name		= "ssc2_clk",
	.pmc_mask	= 1 << AT91RM9200_ID_SSC2,
	.type		= CLK_TYPE_PERIPHERAL,
};
static struct clk tc0_clk = {
	.name		= "tc0_clk",
	.pmc_mask	= 1 << AT91RM9200_ID_TC0,
	.type		= CLK_TYPE_PERIPHERAL,
};
static struct clk tc1_clk = {
	.name		= "tc1_clk",
	.pmc_mask	= 1 << AT91RM9200_ID_TC1,
	.type		= CLK_TYPE_PERIPHERAL,
};
static struct clk tc2_clk = {
	.name		= "tc2_clk",
	.pmc_mask	= 1 << AT91RM9200_ID_TC2,
	.type		= CLK_TYPE_PERIPHERAL,
};
static struct clk tc3_clk = {
	.name		= "tc3_clk",
	.pmc_mask	= 1 << AT91RM9200_ID_TC3,
	.type		= CLK_TYPE_PERIPHERAL,
};
static struct clk tc4_clk = {
	.name		= "tc4_clk",
	.pmc_mask	= 1 << AT91RM9200_ID_TC4,
	.type		= CLK_TYPE_PERIPHERAL,
};
static struct clk tc5_clk = {
	.name		= "tc5_clk",
	.pmc_mask	= 1 << AT91RM9200_ID_TC5,
	.type		= CLK_TYPE_PERIPHERAL,
};

static struct clk *periph_clocks[] __initdata = {
	&pioA_clk,
	&pioB_clk,
	&pioC_clk,
	&pioD_clk,
	&usart0_clk,
	&usart1_clk,
	&usart2_clk,
	&usart3_clk,
	&mmc_clk,
	&udc_clk,
	&twi_clk,
	&spi_clk,
	&ssc0_clk,
	&ssc1_clk,
	&ssc2_clk,
	&tc0_clk,
	&tc1_clk,
	&tc2_clk,
	&tc3_clk,
	&tc4_clk,
	&tc5_clk,
	&ohci_clk,
	&ether_clk,
	// irq0 .. irq6
};

/*
 * The four programmable clocks.
 * You must configure pin multiplexing to bring these signals out.
 */
static struct clk pck0 = {
	.name		= "pck0",
	.pmc_mask	= AT91_PMC_PCK0,
	.type		= CLK_TYPE_PROGRAMMABLE,
	.id		= 0,
};
static struct clk pck1 = {
	.name		= "pck1",
	.pmc_mask	= AT91_PMC_PCK1,
	.type		= CLK_TYPE_PROGRAMMABLE,
	.id		= 1,
};
static struct clk pck2 = {
	.name		= "pck2",
	.pmc_mask	= AT91_PMC_PCK2,
	.type		= CLK_TYPE_PROGRAMMABLE,
	.id		= 2,
};
static struct clk pck3 = {
	.name		= "pck3",
	.pmc_mask	= AT91_PMC_PCK3,
	.type		= CLK_TYPE_PROGRAMMABLE,
	.id		= 3,
};

static void __init at91rm9200_register_clocks(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(periph_clocks); i++)
		clk_register(periph_clocks[i]);

	clk_register(&pck0);
	clk_register(&pck1);
	clk_register(&pck2);
	clk_register(&pck3);
}

/* --------------------------------------------------------------------
 *  GPIO
 * -------------------------------------------------------------------- */

static struct at91_gpio_bank at91rm9200_gpio[] = {
	{
		.id		= AT91RM9200_ID_PIOA,
		.offset		= AT91_PIOA,
		.clock		= &pioA_clk,
	}, {
		.id		= AT91RM9200_ID_PIOB,
		.offset		= AT91_PIOB,
		.clock		= &pioB_clk,
	}, {
		.id		= AT91RM9200_ID_PIOC,
		.offset		= AT91_PIOC,
		.clock		= &pioC_clk,
	}, {
		.id		= AT91RM9200_ID_PIOD,
		.offset		= AT91_PIOD,
		.clock		= &pioD_clk,
	}
};

static void at91rm9200_reset(void)
{
	/*
	 * Perform a hardware reset with the use of the Watchdog timer.
	 */
	at91_sys_write(AT91_ST_WDMR, AT91_ST_RSTEN | AT91_ST_EXTEN | 1);
	at91_sys_write(AT91_ST_CR, AT91_ST_WDRST);
}


/* --------------------------------------------------------------------
 *  AT91RM9200 processor initialization
 * -------------------------------------------------------------------- */
void __init at91rm9200_initialize(unsigned long main_clock, unsigned short banks)
{
	/* Map peripherals */
	iotable_init(at91rm9200_io_desc, ARRAY_SIZE(at91rm9200_io_desc));

	at91_arch_reset = at91rm9200_reset;
	at91_extern_irq = (1 << AT91RM9200_ID_IRQ0) | (1 << AT91RM9200_ID_IRQ1)
			| (1 << AT91RM9200_ID_IRQ2) | (1 << AT91RM9200_ID_IRQ3)
			| (1 << AT91RM9200_ID_IRQ4) | (1 << AT91RM9200_ID_IRQ5)
			| (1 << AT91RM9200_ID_IRQ6);

	/* Init clock subsystem */
	at91_clock_init(main_clock);

	/* Register the processor-specific clocks */
	at91rm9200_register_clocks();

	/* Initialize GPIO subsystem */
	at91_gpio_init(at91rm9200_gpio, banks);
}


/* --------------------------------------------------------------------
 *  Interrupt initialization
 * -------------------------------------------------------------------- */

/*
 * The default interrupt priority levels (0 = lowest, 7 = highest).
 */
static unsigned int at91rm9200_default_irq_priority[NR_AIC_IRQS] __initdata = {
	7,	/* Advanced Interrupt Controller (FIQ) */
	7,	/* System Peripherals */
	1,	/* Parallel IO Controller A */
	1,	/* Parallel IO Controller B */
	1,	/* Parallel IO Controller C */
	1,	/* Parallel IO Controller D */
	5,	/* USART 0 */
	5,	/* USART 1 */
	5,	/* USART 2 */
	5,	/* USART 3 */
	0,	/* Multimedia Card Interface */
	2,	/* USB Device Port */
	6,	/* Two-Wire Interface */
	5,	/* Serial Peripheral Interface */
	4,	/* Serial Synchronous Controller 0 */
	4,	/* Serial Synchronous Controller 1 */
	4,	/* Serial Synchronous Controller 2 */
	0,	/* Timer Counter 0 */
	0,	/* Timer Counter 1 */
	0,	/* Timer Counter 2 */
	0,	/* Timer Counter 3 */
	0,	/* Timer Counter 4 */
	0,	/* Timer Counter 5 */
	2,	/* USB Host port */
	3,	/* Ethernet MAC */
	0,	/* Advanced Interrupt Controller (IRQ0) */
	0,	/* Advanced Interrupt Controller (IRQ1) */
	0,	/* Advanced Interrupt Controller (IRQ2) */
	0,	/* Advanced Interrupt Controller (IRQ3) */
	0,	/* Advanced Interrupt Controller (IRQ4) */
	0,	/* Advanced Interrupt Controller (IRQ5) */
	0	/* Advanced Interrupt Controller (IRQ6) */
};

void __init at91rm9200_init_interrupts(unsigned int priority[NR_AIC_IRQS])
{
	if (!priority)
		priority = at91rm9200_default_irq_priority;

	/* Initialize the AIC interrupt controller */
	at91_aic_init(priority);

	/* Enable GPIO interrupts */
	at91_gpio_irq_setup();
}
