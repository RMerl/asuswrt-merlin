/***************************************************************************/

/*
 *	linux/arch/m68knommu/platform/532x/config.c
 *
 *	Copyright (C) 1999-2002, Greg Ungerer (gerg@snapgear.com)
 *	Copyright (C) 2000, Lineo (www.lineo.com)
 *	Yaroslav Vinogradov yaroslav.vinogradov@freescale.com
 *	Copyright Freescale Semiconductor, Inc 2006
 *	Copyright (c) 2006, emlix, Sebastian Hess <sh@emlix.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

/***************************************************************************/

#include <linux/kernel.h>
#include <linux/param.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/spi/spi.h>
#include <linux/gpio.h>
#include <asm/machdep.h>
#include <asm/coldfire.h>
#include <asm/mcfsim.h>
#include <asm/mcfuart.h>
#include <asm/mcfdma.h>
#include <asm/mcfwdebug.h>
#include <asm/mcfqspi.h>

/***************************************************************************/

static struct mcf_platform_uart m532x_uart_platform[] = {
	{
		.mapbase	= MCFUART_BASE1,
		.irq		= MCFINT_VECBASE + MCFINT_UART0,
	},
	{
		.mapbase 	= MCFUART_BASE2,
		.irq		= MCFINT_VECBASE + MCFINT_UART1,
	},
	{
		.mapbase 	= MCFUART_BASE3,
		.irq		= MCFINT_VECBASE + MCFINT_UART2,
	},
	{ },
};

static struct platform_device m532x_uart = {
	.name			= "mcfuart",
	.id			= 0,
	.dev.platform_data	= m532x_uart_platform,
};

static struct resource m532x_fec_resources[] = {
	{
		.start		= 0xfc030000,
		.end		= 0xfc0307ff,
		.flags		= IORESOURCE_MEM,
	},
	{
		.start		= 64 + 36,
		.end		= 64 + 36,
		.flags		= IORESOURCE_IRQ,
	},
	{
		.start		= 64 + 40,
		.end		= 64 + 40,
		.flags		= IORESOURCE_IRQ,
	},
	{
		.start		= 64 + 42,
		.end		= 64 + 42,
		.flags		= IORESOURCE_IRQ,
	},
};

static struct platform_device m532x_fec = {
	.name			= "fec",
	.id			= 0,
	.num_resources		= ARRAY_SIZE(m532x_fec_resources),
	.resource		= m532x_fec_resources,
};

#if defined(CONFIG_SPI_COLDFIRE_QSPI) || defined(CONFIG_SPI_COLDFIRE_QSPI_MODULE)
static struct resource m532x_qspi_resources[] = {
	{
		.start		= MCFQSPI_IOBASE,
		.end		= MCFQSPI_IOBASE + MCFQSPI_IOSIZE - 1,
		.flags		= IORESOURCE_MEM,
	},
	{
		.start		= MCFINT_VECBASE + MCFINT_QSPI,
		.end		= MCFINT_VECBASE + MCFINT_QSPI,
		.flags		= IORESOURCE_IRQ,
	},
};

#define MCFQSPI_CS0    84
#define MCFQSPI_CS1    85
#define MCFQSPI_CS2    86

static int m532x_cs_setup(struct mcfqspi_cs_control *cs_control)
{
	int status;

	status = gpio_request(MCFQSPI_CS0, "MCFQSPI_CS0");
	if (status) {
		pr_debug("gpio_request for MCFQSPI_CS0 failed\n");
		goto fail0;
	}
	status = gpio_direction_output(MCFQSPI_CS0, 1);
	if (status) {
		pr_debug("gpio_direction_output for MCFQSPI_CS0 failed\n");
		goto fail1;
	}

	status = gpio_request(MCFQSPI_CS1, "MCFQSPI_CS1");
	if (status) {
		pr_debug("gpio_request for MCFQSPI_CS1 failed\n");
		goto fail1;
	}
	status = gpio_direction_output(MCFQSPI_CS1, 1);
	if (status) {
		pr_debug("gpio_direction_output for MCFQSPI_CS1 failed\n");
		goto fail2;
	}

	status = gpio_request(MCFQSPI_CS2, "MCFQSPI_CS2");
	if (status) {
		pr_debug("gpio_request for MCFQSPI_CS2 failed\n");
		goto fail2;
	}
	status = gpio_direction_output(MCFQSPI_CS2, 1);
	if (status) {
		pr_debug("gpio_direction_output for MCFQSPI_CS2 failed\n");
		goto fail3;
	}

	return 0;

fail3:
	gpio_free(MCFQSPI_CS2);
fail2:
	gpio_free(MCFQSPI_CS1);
fail1:
	gpio_free(MCFQSPI_CS0);
fail0:
	return status;
}

static void m532x_cs_teardown(struct mcfqspi_cs_control *cs_control)
{
	gpio_free(MCFQSPI_CS2);
	gpio_free(MCFQSPI_CS1);
	gpio_free(MCFQSPI_CS0);
}

static void m532x_cs_select(struct mcfqspi_cs_control *cs_control,
			    u8 chip_select, bool cs_high)
{
	gpio_set_value(MCFQSPI_CS0 + chip_select, cs_high);
}

static void m532x_cs_deselect(struct mcfqspi_cs_control *cs_control,
			      u8 chip_select, bool cs_high)
{
	gpio_set_value(MCFQSPI_CS0 + chip_select, !cs_high);
}

static struct mcfqspi_cs_control m532x_cs_control = {
	.setup                  = m532x_cs_setup,
	.teardown               = m532x_cs_teardown,
	.select                 = m532x_cs_select,
	.deselect               = m532x_cs_deselect,
};

static struct mcfqspi_platform_data m532x_qspi_data = {
	.bus_num		= 0,
	.num_chipselect		= 3,
	.cs_control		= &m532x_cs_control,
};

static struct platform_device m532x_qspi = {
	.name			= "mcfqspi",
	.id			= 0,
	.num_resources		= ARRAY_SIZE(m532x_qspi_resources),
	.resource		= m532x_qspi_resources,
	.dev.platform_data	= &m532x_qspi_data,
};

static void __init m532x_qspi_init(void)
{
	/* setup QSPS pins for QSPI with gpio CS control */
	writew(0x01f0, MCF_GPIO_PAR_QSPI);
}
#endif /* defined(CONFIG_SPI_COLDFIRE_QSPI) || defined(CONFIG_SPI_COLDFIRE_QSPI_MODULE) */


static struct platform_device *m532x_devices[] __initdata = {
	&m532x_uart,
	&m532x_fec,
#if defined(CONFIG_SPI_COLDFIRE_QSPI) || defined(CONFIG_SPI_COLDFIRE_QSPI_MODULE)
	&m532x_qspi,
#endif
};

/***************************************************************************/

static void __init m532x_uart_init_line(int line, int irq)
{
	if (line == 0) {
		/* GPIO initialization */
		MCF_GPIO_PAR_UART |= 0x000F;
	} else if (line == 1) {
		/* GPIO initialization */
		MCF_GPIO_PAR_UART |= 0x0FF0;
	}
}

static void __init m532x_uarts_init(void)
{
	const int nrlines = ARRAY_SIZE(m532x_uart_platform);
	int line;

	for (line = 0; (line < nrlines); line++)
		m532x_uart_init_line(line, m532x_uart_platform[line].irq);
}
/***************************************************************************/

static void __init m532x_fec_init(void)
{
	/* Set multi-function pins to ethernet mode for fec0 */
	MCF_GPIO_PAR_FECI2C |= (MCF_GPIO_PAR_FECI2C_PAR_MDC_EMDC |
		MCF_GPIO_PAR_FECI2C_PAR_MDIO_EMDIO);
	MCF_GPIO_PAR_FEC = (MCF_GPIO_PAR_FEC_PAR_FEC_7W_FEC |
		MCF_GPIO_PAR_FEC_PAR_FEC_MII_FEC);
}

/***************************************************************************/

static void m532x_cpu_reset(void)
{
	local_irq_disable();
	__raw_writeb(MCF_RCR_SWRESET, MCF_RCR);
}

/***************************************************************************/

void __init config_BSP(char *commandp, int size)
{
#if !defined(CONFIG_BOOTPARAM)
	/* Copy command line from FLASH to local buffer... */
	memcpy(commandp, (char *) 0x4000, 4);
	if(strncmp(commandp, "kcl ", 4) == 0){
		memcpy(commandp, (char *) 0x4004, size);
		commandp[size-1] = 0;
	} else {
		memset(commandp, 0, size);
	}
#endif

#ifdef CONFIG_BDM_DISABLE
	/*
	 * Disable the BDM clocking.  This also turns off most of the rest of
	 * the BDM device.  This is good for EMC reasons. This option is not
	 * incompatible with the memory protection option.
	 */
	wdebug(MCFDEBUG_CSR, MCFDEBUG_CSR_PSTCLK);
#endif
}

/***************************************************************************/

static int __init init_BSP(void)
{
	m532x_uarts_init();
	m532x_fec_init();
#if defined(CONFIG_SPI_COLDFIRE_QSPI) || defined(CONFIG_SPI_COLDFIRE_QSPI_MODULE)
	m532x_qspi_init();
#endif
	platform_add_devices(m532x_devices, ARRAY_SIZE(m532x_devices));
	return 0;
}

arch_initcall(init_BSP);

/***************************************************************************/
/* Board initialization */
/***************************************************************************/
/* 
 * PLL min/max specifications
 */
#define MAX_FVCO	500000	/* KHz */
#define MAX_FSYS	80000 	/* KHz */
#define MIN_FSYS	58333 	/* KHz */
#define FREF		16000   /* KHz */


#define MAX_MFD		135     /* Multiplier */
#define MIN_MFD		88      /* Multiplier */
#define BUSDIV		6       /* Divider */

/*
 * Low Power Divider specifications
 */
#define MIN_LPD		(1 << 0)    /* Divider (not encoded) */
#define MAX_LPD		(1 << 15)   /* Divider (not encoded) */
#define DEFAULT_LPD	(1 << 1)	/* Divider (not encoded) */

#define SYS_CLK_KHZ	80000
#define SYSTEM_PERIOD	12.5
/*
 *  SDRAM Timing Parameters
 */  
#define SDRAM_BL	8	/* # of beats in a burst */
#define SDRAM_TWR	2	/* in clocks */
#define SDRAM_CASL	2.5	/* CASL in clocks */
#define SDRAM_TRCD	2	/* in clocks */
#define SDRAM_TRP	2	/* in clocks */
#define SDRAM_TRFC	7	/* in clocks */
#define SDRAM_TREFI	7800	/* in ns */

#define EXT_SRAM_ADDRESS	(0xC0000000)
#define FLASH_ADDRESS		(0x00000000)
#define SDRAM_ADDRESS		(0x40000000)

#define NAND_FLASH_ADDRESS	(0xD0000000)

int sys_clk_khz = 0;
int sys_clk_mhz = 0;

void wtm_init(void);
void scm_init(void);
void gpio_init(void);
void fbcs_init(void);
void sdramc_init(void);
int  clock_pll (int fsys, int flags);
int  clock_limp (int);
int  clock_exit_limp (void);
int  get_sys_clock (void);

asmlinkage void __init sysinit(void)
{
	sys_clk_khz = clock_pll(0, 0);
	sys_clk_mhz = sys_clk_khz/1000;
	
	wtm_init();
	scm_init();
	gpio_init();
	fbcs_init();
	sdramc_init();
}

void wtm_init(void)
{
	/* Disable watchdog timer */
	MCF_WTM_WCR = 0;
}

#define MCF_SCM_BCR_GBW		(0x00000100)
#define MCF_SCM_BCR_GBR		(0x00000200)

void scm_init(void)
{
	/* All masters are trusted */
	MCF_SCM_MPR = 0x77777777;
    
	/* Allow supervisor/user, read/write, and trusted/untrusted
	   access to all slaves */
	MCF_SCM_PACRA = 0;
	MCF_SCM_PACRB = 0;
	MCF_SCM_PACRC = 0;
	MCF_SCM_PACRD = 0;
	MCF_SCM_PACRE = 0;
	MCF_SCM_PACRF = 0;

	/* Enable bursts */
	MCF_SCM_BCR = (MCF_SCM_BCR_GBR | MCF_SCM_BCR_GBW);
}


void fbcs_init(void)
{
	MCF_GPIO_PAR_CS = 0x0000003E;

	/* Latch chip select */
	MCF_FBCS1_CSAR = 0x10080000;

	MCF_FBCS1_CSCR = 0x002A3780;
	MCF_FBCS1_CSMR = (MCF_FBCS_CSMR_BAM_2M | MCF_FBCS_CSMR_V);

	/* Initialize latch to drive signals to inactive states */
	*((u16 *)(0x10080000)) = 0xFFFF;

	/* External SRAM */
	MCF_FBCS1_CSAR = EXT_SRAM_ADDRESS;
	MCF_FBCS1_CSCR = (MCF_FBCS_CSCR_PS_16
			| MCF_FBCS_CSCR_AA
			| MCF_FBCS_CSCR_SBM
			| MCF_FBCS_CSCR_WS(1));
	MCF_FBCS1_CSMR = (MCF_FBCS_CSMR_BAM_512K
			| MCF_FBCS_CSMR_V);

	/* Boot Flash connected to FBCS0 */
	MCF_FBCS0_CSAR = FLASH_ADDRESS;
	MCF_FBCS0_CSCR = (MCF_FBCS_CSCR_PS_16
			| MCF_FBCS_CSCR_BEM
			| MCF_FBCS_CSCR_AA
			| MCF_FBCS_CSCR_SBM
			| MCF_FBCS_CSCR_WS(7));
	MCF_FBCS0_CSMR = (MCF_FBCS_CSMR_BAM_32M
			| MCF_FBCS_CSMR_V);
}

void sdramc_init(void)
{
	/*
	 * Check to see if the SDRAM has already been initialized
	 * by a run control tool
	 */
	if (!(MCF_SDRAMC_SDCR & MCF_SDRAMC_SDCR_REF)) {
		/* SDRAM chip select initialization */
		
		/* Initialize SDRAM chip select */
		MCF_SDRAMC_SDCS0 = (0
			| MCF_SDRAMC_SDCS_BA(SDRAM_ADDRESS)
			| MCF_SDRAMC_SDCS_CSSZ(MCF_SDRAMC_SDCS_CSSZ_32MBYTE));

	/*
	 * Basic configuration and initialization
	 */
	MCF_SDRAMC_SDCFG1 = (0
		| MCF_SDRAMC_SDCFG1_SRD2RW((int)((SDRAM_CASL + 2) + 0.5 ))
		| MCF_SDRAMC_SDCFG1_SWT2RD(SDRAM_TWR + 1)
		| MCF_SDRAMC_SDCFG1_RDLAT((int)((SDRAM_CASL*2) + 2))
		| MCF_SDRAMC_SDCFG1_ACT2RW((int)((SDRAM_TRCD ) + 0.5))
		| MCF_SDRAMC_SDCFG1_PRE2ACT((int)((SDRAM_TRP ) + 0.5))
		| MCF_SDRAMC_SDCFG1_REF2ACT((int)(((SDRAM_TRFC) ) + 0.5))
		| MCF_SDRAMC_SDCFG1_WTLAT(3));
	MCF_SDRAMC_SDCFG2 = (0
		| MCF_SDRAMC_SDCFG2_BRD2PRE(SDRAM_BL/2 + 1)
		| MCF_SDRAMC_SDCFG2_BWT2RW(SDRAM_BL/2 + SDRAM_TWR)
		| MCF_SDRAMC_SDCFG2_BRD2WT((int)((SDRAM_CASL+SDRAM_BL/2-1.0)+0.5))
		| MCF_SDRAMC_SDCFG2_BL(SDRAM_BL-1));

            
	/*
	 * Precharge and enable write to SDMR
	 */
        MCF_SDRAMC_SDCR = (0
		| MCF_SDRAMC_SDCR_MODE_EN
		| MCF_SDRAMC_SDCR_CKE
		| MCF_SDRAMC_SDCR_DDR
		| MCF_SDRAMC_SDCR_MUX(1)
		| MCF_SDRAMC_SDCR_RCNT((int)(((SDRAM_TREFI/(SYSTEM_PERIOD*64)) - 1) + 0.5))
		| MCF_SDRAMC_SDCR_PS_16
		| MCF_SDRAMC_SDCR_IPALL);            

	/*
	 * Write extended mode register
	 */
	MCF_SDRAMC_SDMR = (0
		| MCF_SDRAMC_SDMR_BNKAD_LEMR
		| MCF_SDRAMC_SDMR_AD(0x0)
		| MCF_SDRAMC_SDMR_CMD);

	/*
	 * Write mode register and reset DLL
	 */
	MCF_SDRAMC_SDMR = (0
		| MCF_SDRAMC_SDMR_BNKAD_LMR
		| MCF_SDRAMC_SDMR_AD(0x163)
		| MCF_SDRAMC_SDMR_CMD);

	/*
	 * Execute a PALL command
	 */
	MCF_SDRAMC_SDCR |= MCF_SDRAMC_SDCR_IPALL;

	/*
	 * Perform two REF cycles
	 */
	MCF_SDRAMC_SDCR |= MCF_SDRAMC_SDCR_IREF;
	MCF_SDRAMC_SDCR |= MCF_SDRAMC_SDCR_IREF;

	/*
	 * Write mode register and clear reset DLL
	 */
	MCF_SDRAMC_SDMR = (0
		| MCF_SDRAMC_SDMR_BNKAD_LMR
		| MCF_SDRAMC_SDMR_AD(0x063)
		| MCF_SDRAMC_SDMR_CMD);
				
	/*
	 * Enable auto refresh and lock SDMR
	 */
	MCF_SDRAMC_SDCR &= ~MCF_SDRAMC_SDCR_MODE_EN;
	MCF_SDRAMC_SDCR |= (0
		| MCF_SDRAMC_SDCR_REF
		| MCF_SDRAMC_SDCR_DQS_OE(0xC));
	}
}

void gpio_init(void)
{
	/* Enable UART0 pins */
	MCF_GPIO_PAR_UART = ( 0
		| MCF_GPIO_PAR_UART_PAR_URXD0
		| MCF_GPIO_PAR_UART_PAR_UTXD0);

	/* Initialize TIN3 as a GPIO output to enable the write
	   half of the latch */
	MCF_GPIO_PAR_TIMER = 0x00;
	__raw_writeb(0x08, MCFGPIO_PDDR_TIMER);
	__raw_writeb(0x00, MCFGPIO_PCLRR_TIMER);

}

int clock_pll(int fsys, int flags)
{
	int fref, temp, fout, mfd;
	u32 i;

	fref = FREF;
        
	if (fsys == 0) {
		/* Return current PLL output */
		mfd = MCF_PLL_PFDR;

		return (fref * mfd / (BUSDIV * 4));
	}

	/* Check bounds of requested system clock */
	if (fsys > MAX_FSYS)
		fsys = MAX_FSYS;
	if (fsys < MIN_FSYS)
		fsys = MIN_FSYS;

	/* Multiplying by 100 when calculating the temp value,
	   and then dividing by 100 to calculate the mfd allows
	   for exact values without needing to include floating
	   point libraries. */
	temp = 100 * fsys / fref;
	mfd = 4 * BUSDIV * temp / 100;
    	    	    	
	/* Determine the output frequency for selected values */
	fout = (fref * mfd / (BUSDIV * 4));

	/*
	 * Check to see if the SDRAM has already been initialized.
	 * If it has then the SDRAM needs to be put into self refresh
	 * mode before reprogramming the PLL.
	 */
	if (MCF_SDRAMC_SDCR & MCF_SDRAMC_SDCR_REF)
		/* Put SDRAM into self refresh mode */
		MCF_SDRAMC_SDCR &= ~MCF_SDRAMC_SDCR_CKE;

	/*
	 * Initialize the PLL to generate the new system clock frequency.
	 * The device must be put into LIMP mode to reprogram the PLL.
	 */

	/* Enter LIMP mode */
	clock_limp(DEFAULT_LPD);
     					
	/* Reprogram PLL for desired fsys */
	MCF_PLL_PODR = (0
		| MCF_PLL_PODR_CPUDIV(BUSDIV/3)
		| MCF_PLL_PODR_BUSDIV(BUSDIV));
						
	MCF_PLL_PFDR = mfd;
		
	/* Exit LIMP mode */
	clock_exit_limp();
	
	/*
	 * Return the SDRAM to normal operation if it is in use.
	 */
	if (MCF_SDRAMC_SDCR & MCF_SDRAMC_SDCR_REF)
		/* Exit self refresh mode */
		MCF_SDRAMC_SDCR |= MCF_SDRAMC_SDCR_CKE;

	/* Errata - workaround for SDRAM opeartion after exiting LIMP mode */
	MCF_SDRAMC_LIMP_FIX = MCF_SDRAMC_REFRESH;

	/* wait for DQS logic to relock */
	for (i = 0; i < 0x200; i++)
		;

	return fout;
}

int clock_limp(int div)
{
	u32 temp;

	/* Check bounds of divider */
	if (div < MIN_LPD)
		div = MIN_LPD;
	if (div > MAX_LPD)
		div = MAX_LPD;
    
	/* Save of the current value of the SSIDIV so we don't
	   overwrite the value*/
	temp = (MCF_CCM_CDR & MCF_CCM_CDR_SSIDIV(0xF));
      
	/* Apply the divider to the system clock */
	MCF_CCM_CDR = ( 0
		| MCF_CCM_CDR_LPDIV(div)
		| MCF_CCM_CDR_SSIDIV(temp));
    
	MCF_CCM_MISCCR |= MCF_CCM_MISCCR_LIMP;
    
	return (FREF/(3*(1 << div)));
}

int clock_exit_limp(void)
{
	int fout;
	
	/* Exit LIMP mode */
	MCF_CCM_MISCCR = (MCF_CCM_MISCCR & ~ MCF_CCM_MISCCR_LIMP);

	/* Wait for PLL to lock */
	while (!(MCF_CCM_MISCCR & MCF_CCM_MISCCR_PLL_LOCK))
		;
	
	fout = get_sys_clock();

	return fout;
}

int get_sys_clock(void)
{
	int divider;
	
	/* Test to see if device is in LIMP mode */
	if (MCF_CCM_MISCCR & MCF_CCM_MISCCR_LIMP) {
		divider = MCF_CCM_CDR & MCF_CCM_CDR_LPDIV(0xF);
		return (FREF/(2 << divider));
	}
	else
		return ((FREF * MCF_PLL_PFDR) / (BUSDIV * 4));
}
