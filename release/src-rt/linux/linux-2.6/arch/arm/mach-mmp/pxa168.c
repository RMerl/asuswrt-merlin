/*
 *  linux/arch/arm/mach-mmp/pxa168.c
 *
 *  Code specific to PXA168
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/io.h>
#include <linux/clk.h>

#include <asm/mach/time.h>
#include <mach/addr-map.h>
#include <mach/cputype.h>
#include <mach/regs-apbc.h>
#include <mach/regs-apmu.h>
#include <mach/irqs.h>
#include <mach/gpio.h>
#include <mach/dma.h>
#include <mach/devices.h>
#include <mach/mfp.h>

#include "common.h"
#include "clock.h"

#define MFPR_VIRT_BASE	(APB_VIRT_BASE + 0x1e000)

static struct mfp_addr_map pxa168_mfp_addr_map[] __initdata =
{
	MFP_ADDR_X(GPIO0,   GPIO36,  0x04c),
	MFP_ADDR_X(GPIO37,  GPIO55,  0x000),
	MFP_ADDR_X(GPIO56,  GPIO123, 0x0e0),
	MFP_ADDR_X(GPIO124, GPIO127, 0x0f4),

	MFP_ADDR_END,
};

#define APMASK(i)	(GPIO_REGS_VIRT + BANK_OFF(i) + 0x09c)

static void __init pxa168_init_gpio(void)
{
	int i;

	/* enable GPIO clock */
	__raw_writel(APBC_APBCLK | APBC_FNCLK, APBC_PXA168_GPIO);

	/* unmask GPIO edge detection for all 4 banks - APMASKx */
	for (i = 0; i < 4; i++)
		__raw_writel(0xffffffff, APMASK(i));

	pxa_init_gpio(IRQ_PXA168_GPIOX, 0, 127, NULL);
}

void __init pxa168_init_irq(void)
{
	icu_init_irq();
	pxa168_init_gpio();
}

/* APB peripheral clocks */
static APBC_CLK(uart1, PXA168_UART1, 1, 14745600);
static APBC_CLK(uart2, PXA168_UART2, 1, 14745600);
static APBC_CLK(twsi0, PXA168_TWSI0, 1, 33000000);
static APBC_CLK(twsi1, PXA168_TWSI1, 1, 33000000);
static APBC_CLK(pwm1, PXA168_PWM1, 1, 13000000);
static APBC_CLK(pwm2, PXA168_PWM2, 1, 13000000);
static APBC_CLK(pwm3, PXA168_PWM3, 1, 13000000);
static APBC_CLK(pwm4, PXA168_PWM4, 1, 13000000);
static APBC_CLK(ssp1, PXA168_SSP1, 4, 0);
static APBC_CLK(ssp2, PXA168_SSP2, 4, 0);
static APBC_CLK(ssp3, PXA168_SSP3, 4, 0);
static APBC_CLK(ssp4, PXA168_SSP4, 4, 0);
static APBC_CLK(ssp5, PXA168_SSP5, 4, 0);
static APBC_CLK(keypad, PXA168_KPC, 0, 32000);

static APMU_CLK(nand, NAND, 0x19b, 156000000);
static APMU_CLK(lcd, LCD, 0x7f, 312000000);

/* device and clock bindings */
static struct clk_lookup pxa168_clkregs[] = {
	INIT_CLKREG(&clk_uart1, "pxa2xx-uart.0", NULL),
	INIT_CLKREG(&clk_uart2, "pxa2xx-uart.1", NULL),
	INIT_CLKREG(&clk_twsi0, "pxa2xx-i2c.0", NULL),
	INIT_CLKREG(&clk_twsi1, "pxa2xx-i2c.1", NULL),
	INIT_CLKREG(&clk_pwm1, "pxa168-pwm.0", NULL),
	INIT_CLKREG(&clk_pwm2, "pxa168-pwm.1", NULL),
	INIT_CLKREG(&clk_pwm3, "pxa168-pwm.2", NULL),
	INIT_CLKREG(&clk_pwm4, "pxa168-pwm.3", NULL),
	INIT_CLKREG(&clk_ssp1, "pxa168-ssp.0", NULL),
	INIT_CLKREG(&clk_ssp2, "pxa168-ssp.1", NULL),
	INIT_CLKREG(&clk_ssp3, "pxa168-ssp.2", NULL),
	INIT_CLKREG(&clk_ssp4, "pxa168-ssp.3", NULL),
	INIT_CLKREG(&clk_ssp5, "pxa168-ssp.4", NULL),
	INIT_CLKREG(&clk_nand, "pxa3xx-nand", NULL),
	INIT_CLKREG(&clk_lcd, "pxa168-fb", NULL),
	INIT_CLKREG(&clk_keypad, "pxa27x-keypad", NULL),
};

static int __init pxa168_init(void)
{
	if (cpu_is_pxa168()) {
		mfp_init_base(MFPR_VIRT_BASE);
		mfp_init_addr(pxa168_mfp_addr_map);
		pxa_init_dma(IRQ_PXA168_DMA_INT0, 32);
		clkdev_add_table(ARRAY_AND_SIZE(pxa168_clkregs));
	}

	return 0;
}
postcore_initcall(pxa168_init);

/* system timer - clock enabled, 3.25MHz */
#define TIMER_CLK_RST	(APBC_APBCLK | APBC_FNCLK | APBC_FNCLKSEL(3))

static void __init pxa168_timer_init(void)
{
	/* this is early, we have to initialize the CCU registers by
	 * ourselves instead of using clk_* API. Clock rate is defined
	 * by APBC_TIMERS_CLK_RST (3.25MHz) and enabled free-running
	 */
	__raw_writel(APBC_APBCLK | APBC_RST, APBC_PXA168_TIMERS);

	/* 3.25MHz, bus/functional clock enabled, release reset */
	__raw_writel(TIMER_CLK_RST, APBC_PXA168_TIMERS);

	timer_init(IRQ_PXA168_TIMER1);
}

struct sys_timer pxa168_timer = {
	.init	= pxa168_timer_init,
};

void pxa168_clear_keypad_wakeup(void)
{
	uint32_t val;
	uint32_t mask = APMU_PXA168_KP_WAKE_CLR;

	/* wake event clear is needed in order to clear keypad interrupt */
	val = __raw_readl(APMU_WAKE_CLR);
	__raw_writel(val |  mask, APMU_WAKE_CLR);
}

/* on-chip devices */
PXA168_DEVICE(uart1, "pxa2xx-uart", 0, UART1, 0xd4017000, 0x30, 21, 22);
PXA168_DEVICE(uart2, "pxa2xx-uart", 1, UART2, 0xd4018000, 0x30, 23, 24);
PXA168_DEVICE(twsi0, "pxa2xx-i2c", 0, TWSI0, 0xd4011000, 0x28);
PXA168_DEVICE(twsi1, "pxa2xx-i2c", 1, TWSI1, 0xd4025000, 0x28);
PXA168_DEVICE(pwm1, "pxa168-pwm", 0, NONE, 0xd401a000, 0x10);
PXA168_DEVICE(pwm2, "pxa168-pwm", 1, NONE, 0xd401a400, 0x10);
PXA168_DEVICE(pwm3, "pxa168-pwm", 2, NONE, 0xd401a800, 0x10);
PXA168_DEVICE(pwm4, "pxa168-pwm", 3, NONE, 0xd401ac00, 0x10);
PXA168_DEVICE(nand, "pxa3xx-nand", -1, NAND, 0xd4283000, 0x80, 97, 99);
PXA168_DEVICE(ssp1, "pxa168-ssp", 0, SSP1, 0xd401b000, 0x40, 52, 53);
PXA168_DEVICE(ssp2, "pxa168-ssp", 1, SSP2, 0xd401c000, 0x40, 54, 55);
PXA168_DEVICE(ssp3, "pxa168-ssp", 2, SSP3, 0xd401f000, 0x40, 56, 57);
PXA168_DEVICE(ssp4, "pxa168-ssp", 3, SSP4, 0xd4020000, 0x40, 58, 59);
PXA168_DEVICE(ssp5, "pxa168-ssp", 4, SSP5, 0xd4021000, 0x40, 60, 61);
PXA168_DEVICE(fb, "pxa168-fb", -1, LCD, 0xd420b000, 0x1c8);
PXA168_DEVICE(keypad, "pxa27x-keypad", -1, KEYPAD, 0xd4012000, 0x4c);
