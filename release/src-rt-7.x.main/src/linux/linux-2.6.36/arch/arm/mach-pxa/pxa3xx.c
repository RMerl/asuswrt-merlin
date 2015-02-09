/*
 * linux/arch/arm/mach-pxa/pxa3xx.c
 *
 * code specific to pxa3xx aka Monahans
 *
 * Copyright (C) 2006 Marvell International Ltd.
 *
 * 2007-09-02: eric miao <eric.miao@marvell.com>
 *             initial version
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/pm.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/sysdev.h>

#include <mach/hardware.h>
#include <mach/gpio.h>
#include <mach/pxa3xx-regs.h>
#include <mach/reset.h>
#include <mach/ohci.h>
#include <mach/pm.h>
#include <mach/dma.h>
#include <mach/regs-intc.h>
#include <plat/i2c.h>

#include "generic.h"
#include "devices.h"
#include "clock.h"

/* Crystal clock: 13MHz */
#define BASE_CLK	13000000

/* Ring Oscillator Clock: 60MHz */
#define RO_CLK		60000000

#define ACCR_D0CS	(1 << 26)
#define ACCR_PCCE	(1 << 11)

#define PECR_IE(n)	((1 << ((n) * 2)) << 28)
#define PECR_IS(n)	((1 << ((n) * 2)) << 29)

/* crystal frequency to static memory controller multiplier (SMCFS) */
static unsigned char smcfs_mult[8] = { 6, 0, 8, 0, 0, 16, };

/* crystal frequency to HSIO bus frequency multiplier (HSS) */
static unsigned char hss_mult[4] = { 8, 12, 16, 24 };

/*
 * Get the clock frequency as reflected by CCSR and the turbo flag.
 * We assume these values have been applied via a fcs.
 * If info is not 0 we also display the current settings.
 */
unsigned int pxa3xx_get_clk_frequency_khz(int info)
{
	unsigned long acsr, xclkcfg;
	unsigned int t, xl, xn, hss, ro, XL, XN, CLK, HSS;

	/* Read XCLKCFG register turbo bit */
	__asm__ __volatile__("mrc\tp14, 0, %0, c6, c0, 0" : "=r"(xclkcfg));
	t = xclkcfg & 0x1;

	acsr = ACSR;

	xl  = acsr & 0x1f;
	xn  = (acsr >> 8) & 0x7;
	hss = (acsr >> 14) & 0x3;

	XL = xl * BASE_CLK;
	XN = xn * XL;

	ro = acsr & ACCR_D0CS;

	CLK = (ro) ? RO_CLK : ((t) ? XN : XL);
	HSS = (ro) ? RO_CLK : hss_mult[hss] * BASE_CLK;

	if (info) {
		pr_info("RO Mode clock: %d.%02dMHz (%sactive)\n",
			RO_CLK / 1000000, (RO_CLK % 1000000) / 10000,
			(ro) ? "" : "in");
		pr_info("Run Mode clock: %d.%02dMHz (*%d)\n",
			XL / 1000000, (XL % 1000000) / 10000, xl);
		pr_info("Turbo Mode clock: %d.%02dMHz (*%d, %sactive)\n",
			XN / 1000000, (XN % 1000000) / 10000, xn,
			(t) ? "" : "in");
		pr_info("HSIO bus clock: %d.%02dMHz\n",
			HSS / 1000000, (HSS % 1000000) / 10000);
	}

	return CLK / 1000;
}

/*
 * Return the current static memory controller clock frequency
 * in units of 10kHz
 */
unsigned int pxa3xx_get_memclk_frequency_10khz(void)
{
	unsigned long acsr;
	unsigned int smcfs, clk = 0;

	acsr = ACSR;

	smcfs = (acsr >> 23) & 0x7;
	clk = (acsr & ACCR_D0CS) ? RO_CLK : smcfs_mult[smcfs] * BASE_CLK;

	return (clk / 10000);
}

void pxa3xx_clear_reset_status(unsigned int mask)
{
	/* RESET_STATUS_* has a 1:1 mapping with ARSR */
	ARSR = mask;
}

/*
 * Return the current AC97 clock frequency.
 */
static unsigned long clk_pxa3xx_ac97_getrate(struct clk *clk)
{
	unsigned long rate = 312000000;
	unsigned long ac97_div;

	ac97_div = AC97_DIV;

	/* This may loose precision for some rates but won't for the
	 * standard 24.576MHz.
	 */
	rate /= (ac97_div >> 12) & 0x7fff;
	rate *= (ac97_div & 0xfff);

	return rate;
}

/*
 * Return the current HSIO bus clock frequency
 */
static unsigned long clk_pxa3xx_hsio_getrate(struct clk *clk)
{
	unsigned long acsr;
	unsigned int hss, hsio_clk;

	acsr = ACSR;

	hss = (acsr >> 14) & 0x3;
	hsio_clk = (acsr & ACCR_D0CS) ? RO_CLK : hss_mult[hss] * BASE_CLK;

	return hsio_clk;
}

void clk_pxa3xx_cken_enable(struct clk *clk)
{
	unsigned long mask = 1ul << (clk->cken & 0x1f);

	if (clk->cken < 32)
		CKENA |= mask;
	else
		CKENB |= mask;
}

void clk_pxa3xx_cken_disable(struct clk *clk)
{
	unsigned long mask = 1ul << (clk->cken & 0x1f);

	if (clk->cken < 32)
		CKENA &= ~mask;
	else
		CKENB &= ~mask;
}

const struct clkops clk_pxa3xx_cken_ops = {
	.enable		= clk_pxa3xx_cken_enable,
	.disable	= clk_pxa3xx_cken_disable,
};

static const struct clkops clk_pxa3xx_hsio_ops = {
	.enable		= clk_pxa3xx_cken_enable,
	.disable	= clk_pxa3xx_cken_disable,
	.getrate	= clk_pxa3xx_hsio_getrate,
};

static const struct clkops clk_pxa3xx_ac97_ops = {
	.enable		= clk_pxa3xx_cken_enable,
	.disable	= clk_pxa3xx_cken_disable,
	.getrate	= clk_pxa3xx_ac97_getrate,
};

static void clk_pout_enable(struct clk *clk)
{
	OSCC |= OSCC_PEN;
}

static void clk_pout_disable(struct clk *clk)
{
	OSCC &= ~OSCC_PEN;
}

static const struct clkops clk_pout_ops = {
	.enable		= clk_pout_enable,
	.disable	= clk_pout_disable,
};

static void clk_dummy_enable(struct clk *clk)
{
}

static void clk_dummy_disable(struct clk *clk)
{
}

static const struct clkops clk_dummy_ops = {
	.enable		= clk_dummy_enable,
	.disable	= clk_dummy_disable,
};

static struct clk clk_pxa3xx_pout = {
	.ops		= &clk_pout_ops,
	.rate		= 13000000,
	.delay		= 70,
};

static struct clk clk_dummy = {
	.ops		= &clk_dummy_ops,
};

static DEFINE_PXA3_CK(pxa3xx_lcd, LCD, &clk_pxa3xx_hsio_ops);
static DEFINE_PXA3_CK(pxa3xx_camera, CAMERA, &clk_pxa3xx_hsio_ops);
static DEFINE_PXA3_CK(pxa3xx_ac97, AC97, &clk_pxa3xx_ac97_ops);
static DEFINE_PXA3_CKEN(pxa3xx_ffuart, FFUART, 14857000, 1);
static DEFINE_PXA3_CKEN(pxa3xx_btuart, BTUART, 14857000, 1);
static DEFINE_PXA3_CKEN(pxa3xx_stuart, STUART, 14857000, 1);
static DEFINE_PXA3_CKEN(pxa3xx_i2c, I2C, 32842000, 0);
static DEFINE_PXA3_CKEN(pxa3xx_udc, UDC, 48000000, 5);
static DEFINE_PXA3_CKEN(pxa3xx_usbh, USBH, 48000000, 0);
static DEFINE_PXA3_CKEN(pxa3xx_u2d, USB2, 48000000, 0);
static DEFINE_PXA3_CKEN(pxa3xx_keypad, KEYPAD, 32768, 0);
static DEFINE_PXA3_CKEN(pxa3xx_ssp1, SSP1, 13000000, 0);
static DEFINE_PXA3_CKEN(pxa3xx_ssp2, SSP2, 13000000, 0);
static DEFINE_PXA3_CKEN(pxa3xx_ssp3, SSP3, 13000000, 0);
static DEFINE_PXA3_CKEN(pxa3xx_ssp4, SSP4, 13000000, 0);
static DEFINE_PXA3_CKEN(pxa3xx_pwm0, PWM0, 13000000, 0);
static DEFINE_PXA3_CKEN(pxa3xx_pwm1, PWM1, 13000000, 0);
static DEFINE_PXA3_CKEN(pxa3xx_mmc1, MMC1, 19500000, 0);
static DEFINE_PXA3_CKEN(pxa3xx_mmc2, MMC2, 19500000, 0);

static struct clk_lookup pxa3xx_clkregs[] = {
	INIT_CLKREG(&clk_pxa3xx_pout, NULL, "CLK_POUT"),
	/* Power I2C clock is always on */
	INIT_CLKREG(&clk_dummy, "pxa3xx-pwri2c.1", NULL),
	INIT_CLKREG(&clk_pxa3xx_lcd, "pxa2xx-fb", NULL),
	INIT_CLKREG(&clk_pxa3xx_camera, NULL, "CAMCLK"),
	INIT_CLKREG(&clk_pxa3xx_ac97, NULL, "AC97CLK"),
	INIT_CLKREG(&clk_pxa3xx_ffuart, "pxa2xx-uart.0", NULL),
	INIT_CLKREG(&clk_pxa3xx_btuart, "pxa2xx-uart.1", NULL),
	INIT_CLKREG(&clk_pxa3xx_stuart, "pxa2xx-uart.2", NULL),
	INIT_CLKREG(&clk_pxa3xx_stuart, "pxa2xx-ir", "UARTCLK"),
	INIT_CLKREG(&clk_pxa3xx_i2c, "pxa2xx-i2c.0", NULL),
	INIT_CLKREG(&clk_pxa3xx_udc, "pxa27x-udc", NULL),
	INIT_CLKREG(&clk_pxa3xx_usbh, "pxa27x-ohci", NULL),
	INIT_CLKREG(&clk_pxa3xx_u2d, NULL, "U2DCLK"),
	INIT_CLKREG(&clk_pxa3xx_keypad, "pxa27x-keypad", NULL),
	INIT_CLKREG(&clk_pxa3xx_ssp1, "pxa27x-ssp.0", NULL),
	INIT_CLKREG(&clk_pxa3xx_ssp2, "pxa27x-ssp.1", NULL),
	INIT_CLKREG(&clk_pxa3xx_ssp3, "pxa27x-ssp.2", NULL),
	INIT_CLKREG(&clk_pxa3xx_ssp4, "pxa27x-ssp.3", NULL),
	INIT_CLKREG(&clk_pxa3xx_pwm0, "pxa27x-pwm.0", NULL),
	INIT_CLKREG(&clk_pxa3xx_pwm1, "pxa27x-pwm.1", NULL),
	INIT_CLKREG(&clk_pxa3xx_mmc1, "pxa2xx-mci.0", NULL),
	INIT_CLKREG(&clk_pxa3xx_mmc2, "pxa2xx-mci.1", NULL),
};

#ifdef CONFIG_PM

#define ISRAM_START	0x5c000000
#define ISRAM_SIZE	SZ_256K

static void __iomem *sram;
static unsigned long wakeup_src;

#define SAVE(x)		sleep_save[SLEEP_SAVE_##x] = x
#define RESTORE(x)	x = sleep_save[SLEEP_SAVE_##x]

enum {	SLEEP_SAVE_CKENA,
	SLEEP_SAVE_CKENB,
	SLEEP_SAVE_ACCR,

	SLEEP_SAVE_COUNT,
};

static void pxa3xx_cpu_pm_save(unsigned long *sleep_save)
{
	SAVE(CKENA);
	SAVE(CKENB);
	SAVE(ACCR);
}

static void pxa3xx_cpu_pm_restore(unsigned long *sleep_save)
{
	RESTORE(ACCR);
	RESTORE(CKENA);
	RESTORE(CKENB);
}

/*
 * Enter a standby mode (S0D1C2 or S0D2C2).  Upon wakeup, the dynamic
 * memory controller has to be reinitialised, so we place some code
 * in the SRAM to perform this function.
 *
 * We disable FIQs across the standby - otherwise, we might receive a
 * FIQ while the SDRAM is unavailable.
 */
static void pxa3xx_cpu_standby(unsigned int pwrmode)
{
	extern const char pm_enter_standby_start[], pm_enter_standby_end[];
	void (*fn)(unsigned int) = (void __force *)(sram + 0x8000);

	memcpy_toio(sram + 0x8000, pm_enter_standby_start,
		    pm_enter_standby_end - pm_enter_standby_start);

	AD2D0SR = ~0;
	AD2D1SR = ~0;
	AD2D0ER = wakeup_src;
	AD2D1ER = 0;
	ASCR = ASCR;
	ARSR = ARSR;

	local_fiq_disable();
	fn(pwrmode);
	local_fiq_enable();

	AD2D0ER = 0;
	AD2D1ER = 0;
}

/*
 * NOTE:  currently, the OBM (OEM Boot Module) binary comes along with
 * PXA3xx development kits assumes that the resuming process continues
 * with the address stored within the first 4 bytes of SDRAM. The PSPR
 * register is used privately by BootROM and OBM, and _must_ be set to
 * 0x5c014000 for the moment.
 */
static void pxa3xx_cpu_pm_suspend(void)
{
	volatile unsigned long *p = (volatile void *)0xc0000000;
	unsigned long saved_data = *p;

	extern void pxa3xx_cpu_suspend(void);
	extern void pxa3xx_cpu_resume(void);

	/* resuming from D2 requires the HSIO2/BOOT/TPM clocks enabled */
	CKENA |= (1 << CKEN_BOOT) | (1 << CKEN_TPM);
	CKENB |= 1 << (CKEN_HSIO2 & 0x1f);

	/* clear and setup wakeup source */
	AD3SR = ~0;
	AD3ER = wakeup_src;
	ASCR = ASCR;
	ARSR = ARSR;

	PCFR |= (1u << 13);			/* L1_DIS */
	PCFR &= ~((1u << 12) | (1u << 1));	/* L0_EN | SL_ROD */

	PSPR = 0x5c014000;

	/* overwrite with the resume address */
	*p = virt_to_phys(pxa3xx_cpu_resume);

	pxa3xx_cpu_suspend();

	*p = saved_data;

	AD3ER = 0;
}

static void pxa3xx_cpu_pm_enter(suspend_state_t state)
{
	/*
	 * Don't sleep if no wakeup sources are defined
	 */
	if (wakeup_src == 0) {
		printk(KERN_ERR "Not suspending: no wakeup sources\n");
		return;
	}

	switch (state) {
	case PM_SUSPEND_STANDBY:
		pxa3xx_cpu_standby(PXA3xx_PM_S0D2C2);
		break;

	case PM_SUSPEND_MEM:
		pxa3xx_cpu_pm_suspend();
		break;
	}
}

static int pxa3xx_cpu_pm_valid(suspend_state_t state)
{
	return state == PM_SUSPEND_MEM || state == PM_SUSPEND_STANDBY;
}

static struct pxa_cpu_pm_fns pxa3xx_cpu_pm_fns = {
	.save_count	= SLEEP_SAVE_COUNT,
	.save		= pxa3xx_cpu_pm_save,
	.restore	= pxa3xx_cpu_pm_restore,
	.valid		= pxa3xx_cpu_pm_valid,
	.enter		= pxa3xx_cpu_pm_enter,
};

static void __init pxa3xx_init_pm(void)
{
	sram = ioremap(ISRAM_START, ISRAM_SIZE);
	if (!sram) {
		printk(KERN_ERR "Unable to map ISRAM: disabling standby/suspend\n");
		return;
	}

	/*
	 * Since we copy wakeup code into the SRAM, we need to ensure
	 * that it is preserved over the low power modes.  Note: bit 8
	 * is undocumented in the developer manual, but must be set.
	 */
	AD1R |= ADXR_L2 | ADXR_R0;
	AD2R |= ADXR_L2 | ADXR_R0;
	AD3R |= ADXR_L2 | ADXR_R0;

	/*
	 * Clear the resume enable registers.
	 */
	AD1D0ER = 0;
	AD2D0ER = 0;
	AD2D1ER = 0;
	AD3ER = 0;

	pxa_cpu_pm_fns = &pxa3xx_cpu_pm_fns;
}

static int pxa3xx_set_wake(unsigned int irq, unsigned int on)
{
	unsigned long flags, mask = 0;

	switch (irq) {
	case IRQ_SSP3:
		mask = ADXER_MFP_WSSP3;
		break;
	case IRQ_MSL:
		mask = ADXER_WMSL0;
		break;
	case IRQ_USBH2:
	case IRQ_USBH1:
		mask = ADXER_WUSBH;
		break;
	case IRQ_KEYPAD:
		mask = ADXER_WKP;
		break;
	case IRQ_AC97:
		mask = ADXER_MFP_WAC97;
		break;
	case IRQ_USIM:
		mask = ADXER_WUSIM0;
		break;
	case IRQ_SSP2:
		mask = ADXER_MFP_WSSP2;
		break;
	case IRQ_I2C:
		mask = ADXER_MFP_WI2C;
		break;
	case IRQ_STUART:
		mask = ADXER_MFP_WUART3;
		break;
	case IRQ_BTUART:
		mask = ADXER_MFP_WUART2;
		break;
	case IRQ_FFUART:
		mask = ADXER_MFP_WUART1;
		break;
	case IRQ_MMC:
		mask = ADXER_MFP_WMMC1;
		break;
	case IRQ_SSP:
		mask = ADXER_MFP_WSSP1;
		break;
	case IRQ_RTCAlrm:
		mask = ADXER_WRTC;
		break;
	case IRQ_SSP4:
		mask = ADXER_MFP_WSSP4;
		break;
	case IRQ_TSI:
		mask = ADXER_WTSI;
		break;
	case IRQ_USIM2:
		mask = ADXER_WUSIM1;
		break;
	case IRQ_MMC2:
		mask = ADXER_MFP_WMMC2;
		break;
	case IRQ_NAND:
		mask = ADXER_MFP_WFLASH;
		break;
	case IRQ_USB2:
		mask = ADXER_WUSB2;
		break;
	case IRQ_WAKEUP0:
		mask = ADXER_WEXTWAKE0;
		break;
	case IRQ_WAKEUP1:
		mask = ADXER_WEXTWAKE1;
		break;
	case IRQ_MMC3:
		mask = ADXER_MFP_GEN12;
		break;
	default:
		return -EINVAL;
	}

	local_irq_save(flags);
	if (on)
		wakeup_src |= mask;
	else
		wakeup_src &= ~mask;
	local_irq_restore(flags);

	return 0;
}
#else
static inline void pxa3xx_init_pm(void) {}
#define pxa3xx_set_wake	NULL
#endif

static void pxa_ack_ext_wakeup(unsigned int irq)
{
	PECR |= PECR_IS(irq - IRQ_WAKEUP0);
}

static void pxa_mask_ext_wakeup(unsigned int irq)
{
	ICMR2 &= ~(1 << ((irq - PXA_IRQ(0)) & 0x1f));
	PECR &= ~PECR_IE(irq - IRQ_WAKEUP0);
}

static void pxa_unmask_ext_wakeup(unsigned int irq)
{
	ICMR2 |= 1 << ((irq - PXA_IRQ(0)) & 0x1f);
	PECR |= PECR_IE(irq - IRQ_WAKEUP0);
}

static int pxa_set_ext_wakeup_type(unsigned int irq, unsigned int flow_type)
{
	if (flow_type & IRQ_TYPE_EDGE_RISING)
		PWER |= 1 << (irq - IRQ_WAKEUP0);

	if (flow_type & IRQ_TYPE_EDGE_FALLING)
		PWER |= 1 << (irq - IRQ_WAKEUP0 + 2);

	return 0;
}

static struct irq_chip pxa_ext_wakeup_chip = {
	.name		= "WAKEUP",
	.ack		= pxa_ack_ext_wakeup,
	.mask		= pxa_mask_ext_wakeup,
	.unmask		= pxa_unmask_ext_wakeup,
	.set_type	= pxa_set_ext_wakeup_type,
};

static void __init pxa_init_ext_wakeup_irq(set_wake_t fn)
{
	int irq;

	for (irq = IRQ_WAKEUP0; irq <= IRQ_WAKEUP1; irq++) {
		set_irq_chip(irq, &pxa_ext_wakeup_chip);
		set_irq_handler(irq, handle_edge_irq);
		set_irq_flags(irq, IRQF_VALID);
	}

	pxa_ext_wakeup_chip.set_wake = fn;
}

void __init pxa3xx_init_irq(void)
{
	/* enable CP6 access */
	u32 value;
	__asm__ __volatile__("mrc p15, 0, %0, c15, c1, 0\n": "=r"(value));
	value |= (1 << 6);
	__asm__ __volatile__("mcr p15, 0, %0, c15, c1, 0\n": :"r"(value));

	pxa_init_irq(56, pxa3xx_set_wake);
	pxa_init_ext_wakeup_irq(pxa3xx_set_wake);
	pxa_init_gpio(IRQ_GPIO_2_x, 2, 127, NULL);
}

/*
 * device registration specific to PXA3xx.
 */

void __init pxa3xx_set_i2c_power_info(struct i2c_pxa_platform_data *info)
{
	pxa_register_device(&pxa3xx_device_i2c_power, info);
}

static struct platform_device *devices[] __initdata = {
	&pxa27x_device_udc,
	&pxa_device_pmu,
	&pxa_device_i2s,
	&sa1100_device_rtc,
	&pxa_device_rtc,
	&pxa27x_device_ssp1,
	&pxa27x_device_ssp2,
	&pxa27x_device_ssp3,
	&pxa3xx_device_ssp4,
	&pxa27x_device_pwm0,
	&pxa27x_device_pwm1,
};

static struct sys_device pxa3xx_sysdev[] = {
	{
		.cls	= &pxa_irq_sysclass,
	}, {
		.cls	= &pxa3xx_mfp_sysclass,
	}, {
		.cls	= &pxa_gpio_sysclass,
	},
};

static int __init pxa3xx_init(void)
{
	int i, ret = 0;

	if (cpu_is_pxa3xx()) {

		reset_status = ARSR;

		/*
		 * clear RDH bit every time after reset
		 *
		 * Note: the last 3 bits DxS are write-1-to-clear so carefully
		 * preserve them here in case they will be referenced later
		 */
		ASCR &= ~(ASCR_RDH | ASCR_D1S | ASCR_D2S | ASCR_D3S);

		clkdev_add_table(pxa3xx_clkregs, ARRAY_SIZE(pxa3xx_clkregs));

		if ((ret = pxa_init_dma(IRQ_DMA, 32)))
			return ret;

		pxa3xx_init_pm();

		for (i = 0; i < ARRAY_SIZE(pxa3xx_sysdev); i++) {
			ret = sysdev_register(&pxa3xx_sysdev[i]);
			if (ret)
				pr_err("failed to register sysdev[%d]\n", i);
		}

		ret = platform_add_devices(devices, ARRAY_SIZE(devices));
	}

	return ret;
}

postcore_initcall(pxa3xx_init);
