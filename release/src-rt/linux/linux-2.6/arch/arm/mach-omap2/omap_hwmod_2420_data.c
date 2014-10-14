/*
 * omap_hwmod_2420_data.c - hardware modules present on the OMAP2420 chips
 *
 * Copyright (C) 2009-2010 Nokia Corporation
 * Paul Walmsley
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * XXX handle crossbar/shared link difference for L3?
 * XXX these should be marked initdata for multi-OMAP kernels
 */
#include <plat/omap_hwmod.h>
#include <mach/irqs.h>
#include <plat/cpu.h>
#include <plat/dma.h>
#include <plat/serial.h>
#include <plat/i2c.h>
#include <plat/gpio.h>
#include <plat/mcspi.h>
#include <plat/dmtimer.h>
#include <plat/l3_2xxx.h>
#include <plat/l4_2xxx.h>

#include "omap_hwmod_common_data.h"

#include "cm-regbits-24xx.h"
#include "prm-regbits-24xx.h"
#include "wd_timer.h"

/*
 * OMAP2420 hardware module integration data
 *
 * ALl of the data in this section should be autogeneratable from the
 * TI hardware database or other technical documentation.  Data that
 * is driver-specific or driver-kernel integration-specific belongs
 * elsewhere.
 */

static struct omap_hwmod omap2420_mpu_hwmod;
static struct omap_hwmod omap2420_iva_hwmod;
static struct omap_hwmod omap2420_l3_main_hwmod;
static struct omap_hwmod omap2420_l4_core_hwmod;
static struct omap_hwmod omap2420_dss_core_hwmod;
static struct omap_hwmod omap2420_dss_dispc_hwmod;
static struct omap_hwmod omap2420_dss_rfbi_hwmod;
static struct omap_hwmod omap2420_dss_venc_hwmod;
static struct omap_hwmod omap2420_wd_timer2_hwmod;
static struct omap_hwmod omap2420_gpio1_hwmod;
static struct omap_hwmod omap2420_gpio2_hwmod;
static struct omap_hwmod omap2420_gpio3_hwmod;
static struct omap_hwmod omap2420_gpio4_hwmod;
static struct omap_hwmod omap2420_dma_system_hwmod;
static struct omap_hwmod omap2420_mcspi1_hwmod;
static struct omap_hwmod omap2420_mcspi2_hwmod;

/* L3 -> L4_CORE interface */
static struct omap_hwmod_ocp_if omap2420_l3_main__l4_core = {
	.master	= &omap2420_l3_main_hwmod,
	.slave	= &omap2420_l4_core_hwmod,
	.user	= OCP_USER_MPU | OCP_USER_SDMA,
};

/* MPU -> L3 interface */
static struct omap_hwmod_ocp_if omap2420_mpu__l3_main = {
	.master = &omap2420_mpu_hwmod,
	.slave	= &omap2420_l3_main_hwmod,
	.user	= OCP_USER_MPU,
};

/* Slave interfaces on the L3 interconnect */
static struct omap_hwmod_ocp_if *omap2420_l3_main_slaves[] = {
	&omap2420_mpu__l3_main,
};

/* DSS -> l3 */
static struct omap_hwmod_ocp_if omap2420_dss__l3 = {
	.master		= &omap2420_dss_core_hwmod,
	.slave		= &omap2420_l3_main_hwmod,
	.fw = {
		.omap2 = {
			.l3_perm_bit  = OMAP2_L3_CORE_FW_CONNID_DSS,
			.flags	= OMAP_FIREWALL_L3,
		}
	},
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* Master interfaces on the L3 interconnect */
static struct omap_hwmod_ocp_if *omap2420_l3_main_masters[] = {
	&omap2420_l3_main__l4_core,
};

/* L3 */
static struct omap_hwmod omap2420_l3_main_hwmod = {
	.name		= "l3_main",
	.class		= &l3_hwmod_class,
	.masters	= omap2420_l3_main_masters,
	.masters_cnt	= ARRAY_SIZE(omap2420_l3_main_masters),
	.slaves		= omap2420_l3_main_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap2420_l3_main_slaves),
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2420),
	.flags		= HWMOD_NO_IDLEST,
};

static struct omap_hwmod omap2420_l4_wkup_hwmod;
static struct omap_hwmod omap2420_uart1_hwmod;
static struct omap_hwmod omap2420_uart2_hwmod;
static struct omap_hwmod omap2420_uart3_hwmod;
static struct omap_hwmod omap2420_i2c1_hwmod;
static struct omap_hwmod omap2420_i2c2_hwmod;
static struct omap_hwmod omap2420_mcbsp1_hwmod;
static struct omap_hwmod omap2420_mcbsp2_hwmod;

/* l4 core -> mcspi1 interface */
static struct omap_hwmod_addr_space omap2420_mcspi1_addr_space[] = {
	{
		.pa_start	= 0x48098000,
		.pa_end		= 0x480980ff,
		.flags		= ADDR_TYPE_RT,
	},
};

static struct omap_hwmod_ocp_if omap2420_l4_core__mcspi1 = {
	.master		= &omap2420_l4_core_hwmod,
	.slave		= &omap2420_mcspi1_hwmod,
	.clk		= "mcspi1_ick",
	.addr		= omap2420_mcspi1_addr_space,
	.addr_cnt	= ARRAY_SIZE(omap2420_mcspi1_addr_space),
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* l4 core -> mcspi2 interface */
static struct omap_hwmod_addr_space omap2420_mcspi2_addr_space[] = {
	{
		.pa_start	= 0x4809a000,
		.pa_end		= 0x4809a0ff,
		.flags		= ADDR_TYPE_RT,
	},
};

static struct omap_hwmod_ocp_if omap2420_l4_core__mcspi2 = {
	.master		= &omap2420_l4_core_hwmod,
	.slave		= &omap2420_mcspi2_hwmod,
	.clk		= "mcspi2_ick",
	.addr		= omap2420_mcspi2_addr_space,
	.addr_cnt	= ARRAY_SIZE(omap2420_mcspi2_addr_space),
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* L4_CORE -> L4_WKUP interface */
static struct omap_hwmod_ocp_if omap2420_l4_core__l4_wkup = {
	.master	= &omap2420_l4_core_hwmod,
	.slave	= &omap2420_l4_wkup_hwmod,
	.user	= OCP_USER_MPU | OCP_USER_SDMA,
};

/* L4 CORE -> UART1 interface */
static struct omap_hwmod_addr_space omap2420_uart1_addr_space[] = {
	{
		.pa_start	= OMAP2_UART1_BASE,
		.pa_end		= OMAP2_UART1_BASE + SZ_8K - 1,
		.flags		= ADDR_MAP_ON_INIT | ADDR_TYPE_RT,
	},
};

static struct omap_hwmod_ocp_if omap2_l4_core__uart1 = {
	.master		= &omap2420_l4_core_hwmod,
	.slave		= &omap2420_uart1_hwmod,
	.clk		= "uart1_ick",
	.addr		= omap2420_uart1_addr_space,
	.addr_cnt	= ARRAY_SIZE(omap2420_uart1_addr_space),
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* L4 CORE -> UART2 interface */
static struct omap_hwmod_addr_space omap2420_uart2_addr_space[] = {
	{
		.pa_start	= OMAP2_UART2_BASE,
		.pa_end		= OMAP2_UART2_BASE + SZ_1K - 1,
		.flags		= ADDR_MAP_ON_INIT | ADDR_TYPE_RT,
	},
};

static struct omap_hwmod_ocp_if omap2_l4_core__uart2 = {
	.master		= &omap2420_l4_core_hwmod,
	.slave		= &omap2420_uart2_hwmod,
	.clk		= "uart2_ick",
	.addr		= omap2420_uart2_addr_space,
	.addr_cnt	= ARRAY_SIZE(omap2420_uart2_addr_space),
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* L4 PER -> UART3 interface */
static struct omap_hwmod_addr_space omap2420_uart3_addr_space[] = {
	{
		.pa_start	= OMAP2_UART3_BASE,
		.pa_end		= OMAP2_UART3_BASE + SZ_1K - 1,
		.flags		= ADDR_MAP_ON_INIT | ADDR_TYPE_RT,
	},
};

static struct omap_hwmod_ocp_if omap2_l4_core__uart3 = {
	.master		= &omap2420_l4_core_hwmod,
	.slave		= &omap2420_uart3_hwmod,
	.clk		= "uart3_ick",
	.addr		= omap2420_uart3_addr_space,
	.addr_cnt	= ARRAY_SIZE(omap2420_uart3_addr_space),
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* I2C IP block address space length (in bytes) */
#define OMAP2_I2C_AS_LEN		128

/* L4 CORE -> I2C1 interface */
static struct omap_hwmod_addr_space omap2420_i2c1_addr_space[] = {
	{
		.pa_start	= 0x48070000,
		.pa_end		= 0x48070000 + OMAP2_I2C_AS_LEN - 1,
		.flags		= ADDR_TYPE_RT,
	},
};

static struct omap_hwmod_ocp_if omap2420_l4_core__i2c1 = {
	.master		= &omap2420_l4_core_hwmod,
	.slave		= &omap2420_i2c1_hwmod,
	.clk		= "i2c1_ick",
	.addr		= omap2420_i2c1_addr_space,
	.addr_cnt	= ARRAY_SIZE(omap2420_i2c1_addr_space),
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* L4 CORE -> I2C2 interface */
static struct omap_hwmod_addr_space omap2420_i2c2_addr_space[] = {
	{
		.pa_start	= 0x48072000,
		.pa_end		= 0x48072000 + OMAP2_I2C_AS_LEN - 1,
		.flags		= ADDR_TYPE_RT,
	},
};

static struct omap_hwmod_ocp_if omap2420_l4_core__i2c2 = {
	.master		= &omap2420_l4_core_hwmod,
	.slave		= &omap2420_i2c2_hwmod,
	.clk		= "i2c2_ick",
	.addr		= omap2420_i2c2_addr_space,
	.addr_cnt	= ARRAY_SIZE(omap2420_i2c2_addr_space),
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* Slave interfaces on the L4_CORE interconnect */
static struct omap_hwmod_ocp_if *omap2420_l4_core_slaves[] = {
	&omap2420_l3_main__l4_core,
};

/* Master interfaces on the L4_CORE interconnect */
static struct omap_hwmod_ocp_if *omap2420_l4_core_masters[] = {
	&omap2420_l4_core__l4_wkup,
	&omap2_l4_core__uart1,
	&omap2_l4_core__uart2,
	&omap2_l4_core__uart3,
	&omap2420_l4_core__i2c1,
	&omap2420_l4_core__i2c2
};

/* L4 CORE */
static struct omap_hwmod omap2420_l4_core_hwmod = {
	.name		= "l4_core",
	.class		= &l4_hwmod_class,
	.masters	= omap2420_l4_core_masters,
	.masters_cnt	= ARRAY_SIZE(omap2420_l4_core_masters),
	.slaves		= omap2420_l4_core_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap2420_l4_core_slaves),
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2420),
	.flags		= HWMOD_NO_IDLEST,
};

/* Slave interfaces on the L4_WKUP interconnect */
static struct omap_hwmod_ocp_if *omap2420_l4_wkup_slaves[] = {
	&omap2420_l4_core__l4_wkup,
};

/* Master interfaces on the L4_WKUP interconnect */
static struct omap_hwmod_ocp_if *omap2420_l4_wkup_masters[] = {
};

/* L4 WKUP */
static struct omap_hwmod omap2420_l4_wkup_hwmod = {
	.name		= "l4_wkup",
	.class		= &l4_hwmod_class,
	.masters	= omap2420_l4_wkup_masters,
	.masters_cnt	= ARRAY_SIZE(omap2420_l4_wkup_masters),
	.slaves		= omap2420_l4_wkup_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap2420_l4_wkup_slaves),
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2420),
	.flags		= HWMOD_NO_IDLEST,
};

/* Master interfaces on the MPU device */
static struct omap_hwmod_ocp_if *omap2420_mpu_masters[] = {
	&omap2420_mpu__l3_main,
};

/* MPU */
static struct omap_hwmod omap2420_mpu_hwmod = {
	.name		= "mpu",
	.class		= &mpu_hwmod_class,
	.main_clk	= "mpu_ck",
	.masters	= omap2420_mpu_masters,
	.masters_cnt	= ARRAY_SIZE(omap2420_mpu_masters),
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2420),
};

/*
 * IVA1 interface data
 */

/* IVA <- L3 interface */
static struct omap_hwmod_ocp_if omap2420_l3__iva = {
	.master		= &omap2420_l3_main_hwmod,
	.slave		= &omap2420_iva_hwmod,
	.clk		= "iva1_ifck",
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

static struct omap_hwmod_ocp_if *omap2420_iva_masters[] = {
	&omap2420_l3__iva,
};

/*
 * IVA2 (IVA2)
 */

static struct omap_hwmod omap2420_iva_hwmod = {
	.name		= "iva",
	.class		= &iva_hwmod_class,
	.masters	= omap2420_iva_masters,
	.masters_cnt	= ARRAY_SIZE(omap2420_iva_masters),
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2420)
};

/* Timer Common */
static struct omap_hwmod_class_sysconfig omap2420_timer_sysc = {
	.rev_offs	= 0x0000,
	.sysc_offs	= 0x0010,
	.syss_offs	= 0x0014,
	.sysc_flags	= (SYSC_HAS_SIDLEMODE | SYSC_HAS_CLOCKACTIVITY |
			   SYSC_HAS_ENAWAKEUP | SYSC_HAS_SOFTRESET |
			   SYSC_HAS_AUTOIDLE),
	.idlemodes	= (SIDLE_FORCE | SIDLE_NO | SIDLE_SMART),
	.sysc_fields    = &omap_hwmod_sysc_type1,
};

static struct omap_hwmod_class omap2420_timer_hwmod_class = {
	.name = "timer",
	.sysc = &omap2420_timer_sysc,
	.rev = OMAP_TIMER_IP_VERSION_1,
};

/* timer1 */
static struct omap_hwmod omap2420_timer1_hwmod;
static struct omap_hwmod_irq_info omap2420_timer1_mpu_irqs[] = {
	{ .irq = 37, },
};

static struct omap_hwmod_addr_space omap2420_timer1_addrs[] = {
	{
		.pa_start	= 0x48028000,
		.pa_end		= 0x48028000 + SZ_1K - 1,
		.flags		= ADDR_TYPE_RT
	},
};

/* l4_wkup -> timer1 */
static struct omap_hwmod_ocp_if omap2420_l4_wkup__timer1 = {
	.master		= &omap2420_l4_wkup_hwmod,
	.slave		= &omap2420_timer1_hwmod,
	.clk		= "gpt1_ick",
	.addr		= omap2420_timer1_addrs,
	.addr_cnt	= ARRAY_SIZE(omap2420_timer1_addrs),
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* timer1 slave port */
static struct omap_hwmod_ocp_if *omap2420_timer1_slaves[] = {
	&omap2420_l4_wkup__timer1,
};

/* timer1 hwmod */
static struct omap_hwmod omap2420_timer1_hwmod = {
	.name		= "timer1",
	.mpu_irqs	= omap2420_timer1_mpu_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(omap2420_timer1_mpu_irqs),
	.main_clk	= "gpt1_fck",
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP24XX_EN_GPT1_SHIFT,
			.module_offs = WKUP_MOD,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP24XX_ST_GPT1_SHIFT,
		},
	},
	.slaves		= omap2420_timer1_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap2420_timer1_slaves),
	.class		= &omap2420_timer_hwmod_class,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2420)
};

/* timer2 */
static struct omap_hwmod omap2420_timer2_hwmod;
static struct omap_hwmod_irq_info omap2420_timer2_mpu_irqs[] = {
	{ .irq = 38, },
};

static struct omap_hwmod_addr_space omap2420_timer2_addrs[] = {
	{
		.pa_start	= 0x4802a000,
		.pa_end		= 0x4802a000 + SZ_1K - 1,
		.flags		= ADDR_TYPE_RT
	},
};

/* l4_core -> timer2 */
static struct omap_hwmod_ocp_if omap2420_l4_core__timer2 = {
	.master		= &omap2420_l4_core_hwmod,
	.slave		= &omap2420_timer2_hwmod,
	.clk		= "gpt2_ick",
	.addr		= omap2420_timer2_addrs,
	.addr_cnt	= ARRAY_SIZE(omap2420_timer2_addrs),
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* timer2 slave port */
static struct omap_hwmod_ocp_if *omap2420_timer2_slaves[] = {
	&omap2420_l4_core__timer2,
};

/* timer2 hwmod */
static struct omap_hwmod omap2420_timer2_hwmod = {
	.name		= "timer2",
	.mpu_irqs	= omap2420_timer2_mpu_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(omap2420_timer2_mpu_irqs),
	.main_clk	= "gpt2_fck",
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP24XX_EN_GPT2_SHIFT,
			.module_offs = CORE_MOD,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP24XX_ST_GPT2_SHIFT,
		},
	},
	.slaves		= omap2420_timer2_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap2420_timer2_slaves),
	.class		= &omap2420_timer_hwmod_class,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2420)
};

/* timer3 */
static struct omap_hwmod omap2420_timer3_hwmod;
static struct omap_hwmod_irq_info omap2420_timer3_mpu_irqs[] = {
	{ .irq = 39, },
};

static struct omap_hwmod_addr_space omap2420_timer3_addrs[] = {
	{
		.pa_start	= 0x48078000,
		.pa_end		= 0x48078000 + SZ_1K - 1,
		.flags		= ADDR_TYPE_RT
	},
};

/* l4_core -> timer3 */
static struct omap_hwmod_ocp_if omap2420_l4_core__timer3 = {
	.master		= &omap2420_l4_core_hwmod,
	.slave		= &omap2420_timer3_hwmod,
	.clk		= "gpt3_ick",
	.addr		= omap2420_timer3_addrs,
	.addr_cnt	= ARRAY_SIZE(omap2420_timer3_addrs),
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* timer3 slave port */
static struct omap_hwmod_ocp_if *omap2420_timer3_slaves[] = {
	&omap2420_l4_core__timer3,
};

/* timer3 hwmod */
static struct omap_hwmod omap2420_timer3_hwmod = {
	.name		= "timer3",
	.mpu_irqs	= omap2420_timer3_mpu_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(omap2420_timer3_mpu_irqs),
	.main_clk	= "gpt3_fck",
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP24XX_EN_GPT3_SHIFT,
			.module_offs = CORE_MOD,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP24XX_ST_GPT3_SHIFT,
		},
	},
	.slaves		= omap2420_timer3_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap2420_timer3_slaves),
	.class		= &omap2420_timer_hwmod_class,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2420)
};

/* timer4 */
static struct omap_hwmod omap2420_timer4_hwmod;
static struct omap_hwmod_irq_info omap2420_timer4_mpu_irqs[] = {
	{ .irq = 40, },
};

static struct omap_hwmod_addr_space omap2420_timer4_addrs[] = {
	{
		.pa_start	= 0x4807a000,
		.pa_end		= 0x4807a000 + SZ_1K - 1,
		.flags		= ADDR_TYPE_RT
	},
};

/* l4_core -> timer4 */
static struct omap_hwmod_ocp_if omap2420_l4_core__timer4 = {
	.master		= &omap2420_l4_core_hwmod,
	.slave		= &omap2420_timer4_hwmod,
	.clk		= "gpt4_ick",
	.addr		= omap2420_timer4_addrs,
	.addr_cnt	= ARRAY_SIZE(omap2420_timer4_addrs),
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* timer4 slave port */
static struct omap_hwmod_ocp_if *omap2420_timer4_slaves[] = {
	&omap2420_l4_core__timer4,
};

/* timer4 hwmod */
static struct omap_hwmod omap2420_timer4_hwmod = {
	.name		= "timer4",
	.mpu_irqs	= omap2420_timer4_mpu_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(omap2420_timer4_mpu_irqs),
	.main_clk	= "gpt4_fck",
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP24XX_EN_GPT4_SHIFT,
			.module_offs = CORE_MOD,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP24XX_ST_GPT4_SHIFT,
		},
	},
	.slaves		= omap2420_timer4_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap2420_timer4_slaves),
	.class		= &omap2420_timer_hwmod_class,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2420)
};

/* timer5 */
static struct omap_hwmod omap2420_timer5_hwmod;
static struct omap_hwmod_irq_info omap2420_timer5_mpu_irqs[] = {
	{ .irq = 41, },
};

static struct omap_hwmod_addr_space omap2420_timer5_addrs[] = {
	{
		.pa_start	= 0x4807c000,
		.pa_end		= 0x4807c000 + SZ_1K - 1,
		.flags		= ADDR_TYPE_RT
	},
};

/* l4_core -> timer5 */
static struct omap_hwmod_ocp_if omap2420_l4_core__timer5 = {
	.master		= &omap2420_l4_core_hwmod,
	.slave		= &omap2420_timer5_hwmod,
	.clk		= "gpt5_ick",
	.addr		= omap2420_timer5_addrs,
	.addr_cnt	= ARRAY_SIZE(omap2420_timer5_addrs),
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* timer5 slave port */
static struct omap_hwmod_ocp_if *omap2420_timer5_slaves[] = {
	&omap2420_l4_core__timer5,
};

/* timer5 hwmod */
static struct omap_hwmod omap2420_timer5_hwmod = {
	.name		= "timer5",
	.mpu_irqs	= omap2420_timer5_mpu_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(omap2420_timer5_mpu_irqs),
	.main_clk	= "gpt5_fck",
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP24XX_EN_GPT5_SHIFT,
			.module_offs = CORE_MOD,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP24XX_ST_GPT5_SHIFT,
		},
	},
	.slaves		= omap2420_timer5_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap2420_timer5_slaves),
	.class		= &omap2420_timer_hwmod_class,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2420)
};


/* timer6 */
static struct omap_hwmod omap2420_timer6_hwmod;
static struct omap_hwmod_irq_info omap2420_timer6_mpu_irqs[] = {
	{ .irq = 42, },
};

static struct omap_hwmod_addr_space omap2420_timer6_addrs[] = {
	{
		.pa_start	= 0x4807e000,
		.pa_end		= 0x4807e000 + SZ_1K - 1,
		.flags		= ADDR_TYPE_RT
	},
};

/* l4_core -> timer6 */
static struct omap_hwmod_ocp_if omap2420_l4_core__timer6 = {
	.master		= &omap2420_l4_core_hwmod,
	.slave		= &omap2420_timer6_hwmod,
	.clk		= "gpt6_ick",
	.addr		= omap2420_timer6_addrs,
	.addr_cnt	= ARRAY_SIZE(omap2420_timer6_addrs),
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* timer6 slave port */
static struct omap_hwmod_ocp_if *omap2420_timer6_slaves[] = {
	&omap2420_l4_core__timer6,
};

/* timer6 hwmod */
static struct omap_hwmod omap2420_timer6_hwmod = {
	.name		= "timer6",
	.mpu_irqs	= omap2420_timer6_mpu_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(omap2420_timer6_mpu_irqs),
	.main_clk	= "gpt6_fck",
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP24XX_EN_GPT6_SHIFT,
			.module_offs = CORE_MOD,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP24XX_ST_GPT6_SHIFT,
		},
	},
	.slaves		= omap2420_timer6_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap2420_timer6_slaves),
	.class		= &omap2420_timer_hwmod_class,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2420)
};

/* timer7 */
static struct omap_hwmod omap2420_timer7_hwmod;
static struct omap_hwmod_irq_info omap2420_timer7_mpu_irqs[] = {
	{ .irq = 43, },
};

static struct omap_hwmod_addr_space omap2420_timer7_addrs[] = {
	{
		.pa_start	= 0x48080000,
		.pa_end		= 0x48080000 + SZ_1K - 1,
		.flags		= ADDR_TYPE_RT
	},
};

/* l4_core -> timer7 */
static struct omap_hwmod_ocp_if omap2420_l4_core__timer7 = {
	.master		= &omap2420_l4_core_hwmod,
	.slave		= &omap2420_timer7_hwmod,
	.clk		= "gpt7_ick",
	.addr		= omap2420_timer7_addrs,
	.addr_cnt	= ARRAY_SIZE(omap2420_timer7_addrs),
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* timer7 slave port */
static struct omap_hwmod_ocp_if *omap2420_timer7_slaves[] = {
	&omap2420_l4_core__timer7,
};

/* timer7 hwmod */
static struct omap_hwmod omap2420_timer7_hwmod = {
	.name		= "timer7",
	.mpu_irqs	= omap2420_timer7_mpu_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(omap2420_timer7_mpu_irqs),
	.main_clk	= "gpt7_fck",
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP24XX_EN_GPT7_SHIFT,
			.module_offs = CORE_MOD,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP24XX_ST_GPT7_SHIFT,
		},
	},
	.slaves		= omap2420_timer7_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap2420_timer7_slaves),
	.class		= &omap2420_timer_hwmod_class,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2420)
};

/* timer8 */
static struct omap_hwmod omap2420_timer8_hwmod;
static struct omap_hwmod_irq_info omap2420_timer8_mpu_irqs[] = {
	{ .irq = 44, },
};

static struct omap_hwmod_addr_space omap2420_timer8_addrs[] = {
	{
		.pa_start	= 0x48082000,
		.pa_end		= 0x48082000 + SZ_1K - 1,
		.flags		= ADDR_TYPE_RT
	},
};

/* l4_core -> timer8 */
static struct omap_hwmod_ocp_if omap2420_l4_core__timer8 = {
	.master		= &omap2420_l4_core_hwmod,
	.slave		= &omap2420_timer8_hwmod,
	.clk		= "gpt8_ick",
	.addr		= omap2420_timer8_addrs,
	.addr_cnt	= ARRAY_SIZE(omap2420_timer8_addrs),
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* timer8 slave port */
static struct omap_hwmod_ocp_if *omap2420_timer8_slaves[] = {
	&omap2420_l4_core__timer8,
};

/* timer8 hwmod */
static struct omap_hwmod omap2420_timer8_hwmod = {
	.name		= "timer8",
	.mpu_irqs	= omap2420_timer8_mpu_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(omap2420_timer8_mpu_irqs),
	.main_clk	= "gpt8_fck",
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP24XX_EN_GPT8_SHIFT,
			.module_offs = CORE_MOD,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP24XX_ST_GPT8_SHIFT,
		},
	},
	.slaves		= omap2420_timer8_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap2420_timer8_slaves),
	.class		= &omap2420_timer_hwmod_class,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2420)
};

/* timer9 */
static struct omap_hwmod omap2420_timer9_hwmod;
static struct omap_hwmod_irq_info omap2420_timer9_mpu_irqs[] = {
	{ .irq = 45, },
};

static struct omap_hwmod_addr_space omap2420_timer9_addrs[] = {
	{
		.pa_start	= 0x48084000,
		.pa_end		= 0x48084000 + SZ_1K - 1,
		.flags		= ADDR_TYPE_RT
	},
};

/* l4_core -> timer9 */
static struct omap_hwmod_ocp_if omap2420_l4_core__timer9 = {
	.master		= &omap2420_l4_core_hwmod,
	.slave		= &omap2420_timer9_hwmod,
	.clk		= "gpt9_ick",
	.addr		= omap2420_timer9_addrs,
	.addr_cnt	= ARRAY_SIZE(omap2420_timer9_addrs),
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* timer9 slave port */
static struct omap_hwmod_ocp_if *omap2420_timer9_slaves[] = {
	&omap2420_l4_core__timer9,
};

/* timer9 hwmod */
static struct omap_hwmod omap2420_timer9_hwmod = {
	.name		= "timer9",
	.mpu_irqs	= omap2420_timer9_mpu_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(omap2420_timer9_mpu_irqs),
	.main_clk	= "gpt9_fck",
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP24XX_EN_GPT9_SHIFT,
			.module_offs = CORE_MOD,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP24XX_ST_GPT9_SHIFT,
		},
	},
	.slaves		= omap2420_timer9_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap2420_timer9_slaves),
	.class		= &omap2420_timer_hwmod_class,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2420)
};

/* timer10 */
static struct omap_hwmod omap2420_timer10_hwmod;
static struct omap_hwmod_irq_info omap2420_timer10_mpu_irqs[] = {
	{ .irq = 46, },
};

static struct omap_hwmod_addr_space omap2420_timer10_addrs[] = {
	{
		.pa_start	= 0x48086000,
		.pa_end		= 0x48086000 + SZ_1K - 1,
		.flags		= ADDR_TYPE_RT
	},
};

/* l4_core -> timer10 */
static struct omap_hwmod_ocp_if omap2420_l4_core__timer10 = {
	.master		= &omap2420_l4_core_hwmod,
	.slave		= &omap2420_timer10_hwmod,
	.clk		= "gpt10_ick",
	.addr		= omap2420_timer10_addrs,
	.addr_cnt	= ARRAY_SIZE(omap2420_timer10_addrs),
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* timer10 slave port */
static struct omap_hwmod_ocp_if *omap2420_timer10_slaves[] = {
	&omap2420_l4_core__timer10,
};

/* timer10 hwmod */
static struct omap_hwmod omap2420_timer10_hwmod = {
	.name		= "timer10",
	.mpu_irqs	= omap2420_timer10_mpu_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(omap2420_timer10_mpu_irqs),
	.main_clk	= "gpt10_fck",
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP24XX_EN_GPT10_SHIFT,
			.module_offs = CORE_MOD,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP24XX_ST_GPT10_SHIFT,
		},
	},
	.slaves		= omap2420_timer10_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap2420_timer10_slaves),
	.class		= &omap2420_timer_hwmod_class,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2420)
};

/* timer11 */
static struct omap_hwmod omap2420_timer11_hwmod;
static struct omap_hwmod_irq_info omap2420_timer11_mpu_irqs[] = {
	{ .irq = 47, },
};

static struct omap_hwmod_addr_space omap2420_timer11_addrs[] = {
	{
		.pa_start	= 0x48088000,
		.pa_end		= 0x48088000 + SZ_1K - 1,
		.flags		= ADDR_TYPE_RT
	},
};

/* l4_core -> timer11 */
static struct omap_hwmod_ocp_if omap2420_l4_core__timer11 = {
	.master		= &omap2420_l4_core_hwmod,
	.slave		= &omap2420_timer11_hwmod,
	.clk		= "gpt11_ick",
	.addr		= omap2420_timer11_addrs,
	.addr_cnt	= ARRAY_SIZE(omap2420_timer11_addrs),
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* timer11 slave port */
static struct omap_hwmod_ocp_if *omap2420_timer11_slaves[] = {
	&omap2420_l4_core__timer11,
};

/* timer11 hwmod */
static struct omap_hwmod omap2420_timer11_hwmod = {
	.name		= "timer11",
	.mpu_irqs	= omap2420_timer11_mpu_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(omap2420_timer11_mpu_irqs),
	.main_clk	= "gpt11_fck",
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP24XX_EN_GPT11_SHIFT,
			.module_offs = CORE_MOD,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP24XX_ST_GPT11_SHIFT,
		},
	},
	.slaves		= omap2420_timer11_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap2420_timer11_slaves),
	.class		= &omap2420_timer_hwmod_class,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2420)
};

/* timer12 */
static struct omap_hwmod omap2420_timer12_hwmod;
static struct omap_hwmod_irq_info omap2420_timer12_mpu_irqs[] = {
	{ .irq = 48, },
};

static struct omap_hwmod_addr_space omap2420_timer12_addrs[] = {
	{
		.pa_start	= 0x4808a000,
		.pa_end		= 0x4808a000 + SZ_1K - 1,
		.flags		= ADDR_TYPE_RT
	},
};

/* l4_core -> timer12 */
static struct omap_hwmod_ocp_if omap2420_l4_core__timer12 = {
	.master		= &omap2420_l4_core_hwmod,
	.slave		= &omap2420_timer12_hwmod,
	.clk		= "gpt12_ick",
	.addr		= omap2420_timer12_addrs,
	.addr_cnt	= ARRAY_SIZE(omap2420_timer12_addrs),
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* timer12 slave port */
static struct omap_hwmod_ocp_if *omap2420_timer12_slaves[] = {
	&omap2420_l4_core__timer12,
};

/* timer12 hwmod */
static struct omap_hwmod omap2420_timer12_hwmod = {
	.name		= "timer12",
	.mpu_irqs	= omap2420_timer12_mpu_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(omap2420_timer12_mpu_irqs),
	.main_clk	= "gpt12_fck",
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP24XX_EN_GPT12_SHIFT,
			.module_offs = CORE_MOD,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP24XX_ST_GPT12_SHIFT,
		},
	},
	.slaves		= omap2420_timer12_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap2420_timer12_slaves),
	.class		= &omap2420_timer_hwmod_class,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2420)
};

/* l4_wkup -> wd_timer2 */
static struct omap_hwmod_addr_space omap2420_wd_timer2_addrs[] = {
	{
		.pa_start	= 0x48022000,
		.pa_end		= 0x4802207f,
		.flags		= ADDR_TYPE_RT
	},
};

static struct omap_hwmod_ocp_if omap2420_l4_wkup__wd_timer2 = {
	.master		= &omap2420_l4_wkup_hwmod,
	.slave		= &omap2420_wd_timer2_hwmod,
	.clk		= "mpu_wdt_ick",
	.addr		= omap2420_wd_timer2_addrs,
	.addr_cnt	= ARRAY_SIZE(omap2420_wd_timer2_addrs),
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/*
 * 'wd_timer' class
 * 32-bit watchdog upward counter that generates a pulse on the reset pin on
 * overflow condition
 */

static struct omap_hwmod_class_sysconfig omap2420_wd_timer_sysc = {
	.rev_offs	= 0x0000,
	.sysc_offs	= 0x0010,
	.syss_offs	= 0x0014,
	.sysc_flags	= (SYSC_HAS_EMUFREE | SYSC_HAS_SOFTRESET |
			   SYSC_HAS_AUTOIDLE | SYSS_HAS_RESET_STATUS),
	.sysc_fields    = &omap_hwmod_sysc_type1,
};

static struct omap_hwmod_class omap2420_wd_timer_hwmod_class = {
	.name		= "wd_timer",
	.sysc		= &omap2420_wd_timer_sysc,
	.pre_shutdown	= &omap2_wd_timer_disable
};

/* wd_timer2 */
static struct omap_hwmod_ocp_if *omap2420_wd_timer2_slaves[] = {
	&omap2420_l4_wkup__wd_timer2,
};

static struct omap_hwmod omap2420_wd_timer2_hwmod = {
	.name		= "wd_timer2",
	.class		= &omap2420_wd_timer_hwmod_class,
	.main_clk	= "mpu_wdt_fck",
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP24XX_EN_MPU_WDT_SHIFT,
			.module_offs = WKUP_MOD,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP24XX_ST_MPU_WDT_SHIFT,
		},
	},
	.slaves		= omap2420_wd_timer2_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap2420_wd_timer2_slaves),
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2420),
};

/* UART */

static struct omap_hwmod_class_sysconfig uart_sysc = {
	.rev_offs	= 0x50,
	.sysc_offs	= 0x54,
	.syss_offs	= 0x58,
	.sysc_flags	= (SYSC_HAS_SIDLEMODE |
			   SYSC_HAS_ENAWAKEUP | SYSC_HAS_SOFTRESET |
			   SYSC_HAS_AUTOIDLE | SYSS_HAS_RESET_STATUS),
	.idlemodes	= (SIDLE_FORCE | SIDLE_NO | SIDLE_SMART),
	.sysc_fields    = &omap_hwmod_sysc_type1,
};

static struct omap_hwmod_class uart_class = {
	.name = "uart",
	.sysc = &uart_sysc,
};

/* UART1 */

static struct omap_hwmod_irq_info uart1_mpu_irqs[] = {
	{ .irq = INT_24XX_UART1_IRQ, },
};

static struct omap_hwmod_dma_info uart1_sdma_reqs[] = {
	{ .name = "rx",	.dma_req = OMAP24XX_DMA_UART1_RX, },
	{ .name = "tx",	.dma_req = OMAP24XX_DMA_UART1_TX, },
};

static struct omap_hwmod_ocp_if *omap2420_uart1_slaves[] = {
	&omap2_l4_core__uart1,
};

static struct omap_hwmod omap2420_uart1_hwmod = {
	.name		= "uart1",
	.mpu_irqs	= uart1_mpu_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(uart1_mpu_irqs),
	.sdma_reqs	= uart1_sdma_reqs,
	.sdma_reqs_cnt	= ARRAY_SIZE(uart1_sdma_reqs),
	.main_clk	= "uart1_fck",
	.prcm		= {
		.omap2 = {
			.module_offs = CORE_MOD,
			.prcm_reg_id = 1,
			.module_bit = OMAP24XX_EN_UART1_SHIFT,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP24XX_EN_UART1_SHIFT,
		},
	},
	.slaves		= omap2420_uart1_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap2420_uart1_slaves),
	.class		= &uart_class,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2420),
};

/* UART2 */

static struct omap_hwmod_irq_info uart2_mpu_irqs[] = {
	{ .irq = INT_24XX_UART2_IRQ, },
};

static struct omap_hwmod_dma_info uart2_sdma_reqs[] = {
	{ .name = "rx",	.dma_req = OMAP24XX_DMA_UART2_RX, },
	{ .name = "tx",	.dma_req = OMAP24XX_DMA_UART2_TX, },
};

static struct omap_hwmod_ocp_if *omap2420_uart2_slaves[] = {
	&omap2_l4_core__uart2,
};

static struct omap_hwmod omap2420_uart2_hwmod = {
	.name		= "uart2",
	.mpu_irqs	= uart2_mpu_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(uart2_mpu_irqs),
	.sdma_reqs	= uart2_sdma_reqs,
	.sdma_reqs_cnt	= ARRAY_SIZE(uart2_sdma_reqs),
	.main_clk	= "uart2_fck",
	.prcm		= {
		.omap2 = {
			.module_offs = CORE_MOD,
			.prcm_reg_id = 1,
			.module_bit = OMAP24XX_EN_UART2_SHIFT,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP24XX_EN_UART2_SHIFT,
		},
	},
	.slaves		= omap2420_uart2_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap2420_uart2_slaves),
	.class		= &uart_class,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2420),
};

/* UART3 */

static struct omap_hwmod_irq_info uart3_mpu_irqs[] = {
	{ .irq = INT_24XX_UART3_IRQ, },
};

static struct omap_hwmod_dma_info uart3_sdma_reqs[] = {
	{ .name = "rx",	.dma_req = OMAP24XX_DMA_UART3_RX, },
	{ .name = "tx",	.dma_req = OMAP24XX_DMA_UART3_TX, },
};

static struct omap_hwmod_ocp_if *omap2420_uart3_slaves[] = {
	&omap2_l4_core__uart3,
};

static struct omap_hwmod omap2420_uart3_hwmod = {
	.name		= "uart3",
	.mpu_irqs	= uart3_mpu_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(uart3_mpu_irqs),
	.sdma_reqs	= uart3_sdma_reqs,
	.sdma_reqs_cnt	= ARRAY_SIZE(uart3_sdma_reqs),
	.main_clk	= "uart3_fck",
	.prcm		= {
		.omap2 = {
			.module_offs = CORE_MOD,
			.prcm_reg_id = 2,
			.module_bit = OMAP24XX_EN_UART3_SHIFT,
			.idlest_reg_id = 2,
			.idlest_idle_bit = OMAP24XX_EN_UART3_SHIFT,
		},
	},
	.slaves		= omap2420_uart3_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap2420_uart3_slaves),
	.class		= &uart_class,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2420),
};

/*
 * 'dss' class
 * display sub-system
 */

static struct omap_hwmod_class_sysconfig omap2420_dss_sysc = {
	.rev_offs	= 0x0000,
	.sysc_offs	= 0x0010,
	.syss_offs	= 0x0014,
	.sysc_flags	= (SYSC_HAS_SOFTRESET | SYSC_HAS_AUTOIDLE),
	.sysc_fields	= &omap_hwmod_sysc_type1,
};

static struct omap_hwmod_class omap2420_dss_hwmod_class = {
	.name = "dss",
	.sysc = &omap2420_dss_sysc,
};

static struct omap_hwmod_dma_info omap2420_dss_sdma_chs[] = {
	{ .name = "dispc", .dma_req = 5 },
};

/* dss */
/* dss master ports */
static struct omap_hwmod_ocp_if *omap2420_dss_masters[] = {
	&omap2420_dss__l3,
};

static struct omap_hwmod_addr_space omap2420_dss_addrs[] = {
	{
		.pa_start	= 0x48050000,
		.pa_end		= 0x480503FF,
		.flags		= ADDR_TYPE_RT
	},
};

/* l4_core -> dss */
static struct omap_hwmod_ocp_if omap2420_l4_core__dss = {
	.master		= &omap2420_l4_core_hwmod,
	.slave		= &omap2420_dss_core_hwmod,
	.clk		= "dss_ick",
	.addr		= omap2420_dss_addrs,
	.addr_cnt	= ARRAY_SIZE(omap2420_dss_addrs),
	.fw = {
		.omap2 = {
			.l4_fw_region  = OMAP2420_L4_CORE_FW_DSS_CORE_REGION,
			.flags	= OMAP_FIREWALL_L4,
		}
	},
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* dss slave ports */
static struct omap_hwmod_ocp_if *omap2420_dss_slaves[] = {
	&omap2420_l4_core__dss,
};

static struct omap_hwmod_opt_clk dss_opt_clks[] = {
	{ .role = "tv_clk", .clk = "dss_54m_fck" },
	{ .role = "sys_clk", .clk = "dss2_fck" },
};

static struct omap_hwmod omap2420_dss_core_hwmod = {
	.name		= "dss_core",
	.class		= &omap2420_dss_hwmod_class,
	.main_clk	= "dss1_fck", /* instead of dss_fck */
	.sdma_reqs	= omap2420_dss_sdma_chs,
	.sdma_reqs_cnt	= ARRAY_SIZE(omap2420_dss_sdma_chs),
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP24XX_EN_DSS1_SHIFT,
			.module_offs = CORE_MOD,
			.idlest_reg_id = 1,
			.idlest_stdby_bit = OMAP24XX_ST_DSS_SHIFT,
		},
	},
	.opt_clks	= dss_opt_clks,
	.opt_clks_cnt = ARRAY_SIZE(dss_opt_clks),
	.slaves		= omap2420_dss_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap2420_dss_slaves),
	.masters	= omap2420_dss_masters,
	.masters_cnt	= ARRAY_SIZE(omap2420_dss_masters),
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2420),
	.flags		= HWMOD_NO_IDLEST,
};

/*
 * 'dispc' class
 * display controller
 */

static struct omap_hwmod_class_sysconfig omap2420_dispc_sysc = {
	.rev_offs	= 0x0000,
	.sysc_offs	= 0x0010,
	.syss_offs	= 0x0014,
	.sysc_flags	= (SYSC_HAS_SIDLEMODE | SYSC_HAS_MIDLEMODE |
			   SYSC_HAS_SOFTRESET | SYSC_HAS_AUTOIDLE),
	.idlemodes	= (SIDLE_FORCE | SIDLE_NO | SIDLE_SMART |
			   MSTANDBY_FORCE | MSTANDBY_NO | MSTANDBY_SMART),
	.sysc_fields	= &omap_hwmod_sysc_type1,
};

static struct omap_hwmod_class omap2420_dispc_hwmod_class = {
	.name = "dispc",
	.sysc = &omap2420_dispc_sysc,
};

static struct omap_hwmod_irq_info omap2420_dispc_irqs[] = {
	{ .irq = 25 },
};

static struct omap_hwmod_addr_space omap2420_dss_dispc_addrs[] = {
	{
		.pa_start	= 0x48050400,
		.pa_end		= 0x480507FF,
		.flags		= ADDR_TYPE_RT
	},
};

/* l4_core -> dss_dispc */
static struct omap_hwmod_ocp_if omap2420_l4_core__dss_dispc = {
	.master		= &omap2420_l4_core_hwmod,
	.slave		= &omap2420_dss_dispc_hwmod,
	.clk		= "dss_ick",
	.addr		= omap2420_dss_dispc_addrs,
	.addr_cnt	= ARRAY_SIZE(omap2420_dss_dispc_addrs),
	.fw = {
		.omap2 = {
			.l4_fw_region  = OMAP2420_L4_CORE_FW_DSS_DISPC_REGION,
			.flags	= OMAP_FIREWALL_L4,
		}
	},
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* dss_dispc slave ports */
static struct omap_hwmod_ocp_if *omap2420_dss_dispc_slaves[] = {
	&omap2420_l4_core__dss_dispc,
};

static struct omap_hwmod omap2420_dss_dispc_hwmod = {
	.name		= "dss_dispc",
	.class		= &omap2420_dispc_hwmod_class,
	.mpu_irqs	= omap2420_dispc_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(omap2420_dispc_irqs),
	.main_clk	= "dss1_fck",
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP24XX_EN_DSS1_SHIFT,
			.module_offs = CORE_MOD,
			.idlest_reg_id = 1,
			.idlest_stdby_bit = OMAP24XX_ST_DSS_SHIFT,
		},
	},
	.slaves		= omap2420_dss_dispc_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap2420_dss_dispc_slaves),
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2420),
	.flags		= HWMOD_NO_IDLEST,
};

/*
 * 'rfbi' class
 * remote frame buffer interface
 */

static struct omap_hwmod_class_sysconfig omap2420_rfbi_sysc = {
	.rev_offs	= 0x0000,
	.sysc_offs	= 0x0010,
	.syss_offs	= 0x0014,
	.sysc_flags	= (SYSC_HAS_SIDLEMODE | SYSC_HAS_SOFTRESET |
			   SYSC_HAS_AUTOIDLE),
	.idlemodes	= (SIDLE_FORCE | SIDLE_NO | SIDLE_SMART),
	.sysc_fields	= &omap_hwmod_sysc_type1,
};

static struct omap_hwmod_class omap2420_rfbi_hwmod_class = {
	.name = "rfbi",
	.sysc = &omap2420_rfbi_sysc,
};

static struct omap_hwmod_addr_space omap2420_dss_rfbi_addrs[] = {
	{
		.pa_start	= 0x48050800,
		.pa_end		= 0x48050BFF,
		.flags		= ADDR_TYPE_RT
	},
};

/* l4_core -> dss_rfbi */
static struct omap_hwmod_ocp_if omap2420_l4_core__dss_rfbi = {
	.master		= &omap2420_l4_core_hwmod,
	.slave		= &omap2420_dss_rfbi_hwmod,
	.clk		= "dss_ick",
	.addr		= omap2420_dss_rfbi_addrs,
	.addr_cnt	= ARRAY_SIZE(omap2420_dss_rfbi_addrs),
	.fw = {
		.omap2 = {
			.l4_fw_region  = OMAP2420_L4_CORE_FW_DSS_CORE_REGION,
			.flags	= OMAP_FIREWALL_L4,
		}
	},
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* dss_rfbi slave ports */
static struct omap_hwmod_ocp_if *omap2420_dss_rfbi_slaves[] = {
	&omap2420_l4_core__dss_rfbi,
};

static struct omap_hwmod omap2420_dss_rfbi_hwmod = {
	.name		= "dss_rfbi",
	.class		= &omap2420_rfbi_hwmod_class,
	.main_clk	= "dss1_fck",
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP24XX_EN_DSS1_SHIFT,
			.module_offs = CORE_MOD,
		},
	},
	.slaves		= omap2420_dss_rfbi_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap2420_dss_rfbi_slaves),
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2420),
	.flags		= HWMOD_NO_IDLEST,
};

/*
 * 'venc' class
 * video encoder
 */

static struct omap_hwmod_class omap2420_venc_hwmod_class = {
	.name = "venc",
};

/* dss_venc */
static struct omap_hwmod_addr_space omap2420_dss_venc_addrs[] = {
	{
		.pa_start	= 0x48050C00,
		.pa_end		= 0x48050FFF,
		.flags		= ADDR_TYPE_RT
	},
};

/* l4_core -> dss_venc */
static struct omap_hwmod_ocp_if omap2420_l4_core__dss_venc = {
	.master		= &omap2420_l4_core_hwmod,
	.slave		= &omap2420_dss_venc_hwmod,
	.clk		= "dss_54m_fck",
	.addr		= omap2420_dss_venc_addrs,
	.addr_cnt	= ARRAY_SIZE(omap2420_dss_venc_addrs),
	.fw = {
		.omap2 = {
			.l4_fw_region  = OMAP2420_L4_CORE_FW_DSS_VENC_REGION,
			.flags	= OMAP_FIREWALL_L4,
		}
	},
	.flags		= OCPIF_SWSUP_IDLE,
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* dss_venc slave ports */
static struct omap_hwmod_ocp_if *omap2420_dss_venc_slaves[] = {
	&omap2420_l4_core__dss_venc,
};

static struct omap_hwmod omap2420_dss_venc_hwmod = {
	.name		= "dss_venc",
	.class		= &omap2420_venc_hwmod_class,
	.main_clk	= "dss1_fck",
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP24XX_EN_DSS1_SHIFT,
			.module_offs = CORE_MOD,
		},
	},
	.slaves		= omap2420_dss_venc_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap2420_dss_venc_slaves),
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2420),
	.flags		= HWMOD_NO_IDLEST,
};

/* I2C common */
static struct omap_hwmod_class_sysconfig i2c_sysc = {
	.rev_offs	= 0x00,
	.sysc_offs	= 0x20,
	.syss_offs	= 0x10,
	.sysc_flags	= (SYSC_HAS_SOFTRESET | SYSS_HAS_RESET_STATUS),
	.sysc_fields	= &omap_hwmod_sysc_type1,
};

static struct omap_hwmod_class i2c_class = {
	.name		= "i2c",
	.sysc		= &i2c_sysc,
};

static struct omap_i2c_dev_attr i2c_dev_attr;

/* I2C1 */

static struct omap_hwmod_irq_info i2c1_mpu_irqs[] = {
	{ .irq = INT_24XX_I2C1_IRQ, },
};

static struct omap_hwmod_dma_info i2c1_sdma_reqs[] = {
	{ .name = "tx", .dma_req = OMAP24XX_DMA_I2C1_TX },
	{ .name = "rx", .dma_req = OMAP24XX_DMA_I2C1_RX },
};

static struct omap_hwmod_ocp_if *omap2420_i2c1_slaves[] = {
	&omap2420_l4_core__i2c1,
};

static struct omap_hwmod omap2420_i2c1_hwmod = {
	.name		= "i2c1",
	.mpu_irqs	= i2c1_mpu_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(i2c1_mpu_irqs),
	.sdma_reqs	= i2c1_sdma_reqs,
	.sdma_reqs_cnt	= ARRAY_SIZE(i2c1_sdma_reqs),
	.main_clk	= "i2c1_fck",
	.prcm		= {
		.omap2 = {
			.module_offs = CORE_MOD,
			.prcm_reg_id = 1,
			.module_bit = OMAP2420_EN_I2C1_SHIFT,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP2420_ST_I2C1_SHIFT,
		},
	},
	.slaves		= omap2420_i2c1_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap2420_i2c1_slaves),
	.class		= &i2c_class,
	.dev_attr	= &i2c_dev_attr,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2420),
	.flags		= HWMOD_16BIT_REG,
};

/* I2C2 */

static struct omap_hwmod_irq_info i2c2_mpu_irqs[] = {
	{ .irq = INT_24XX_I2C2_IRQ, },
};

static struct omap_hwmod_dma_info i2c2_sdma_reqs[] = {
	{ .name = "tx", .dma_req = OMAP24XX_DMA_I2C2_TX },
	{ .name = "rx", .dma_req = OMAP24XX_DMA_I2C2_RX },
};

static struct omap_hwmod_ocp_if *omap2420_i2c2_slaves[] = {
	&omap2420_l4_core__i2c2,
};

static struct omap_hwmod omap2420_i2c2_hwmod = {
	.name		= "i2c2",
	.mpu_irqs	= i2c2_mpu_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(i2c2_mpu_irqs),
	.sdma_reqs	= i2c2_sdma_reqs,
	.sdma_reqs_cnt	= ARRAY_SIZE(i2c2_sdma_reqs),
	.main_clk	= "i2c2_fck",
	.prcm		= {
		.omap2 = {
			.module_offs = CORE_MOD,
			.prcm_reg_id = 1,
			.module_bit = OMAP2420_EN_I2C2_SHIFT,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP2420_ST_I2C2_SHIFT,
		},
	},
	.slaves		= omap2420_i2c2_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap2420_i2c2_slaves),
	.class		= &i2c_class,
	.dev_attr	= &i2c_dev_attr,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2420),
	.flags		= HWMOD_16BIT_REG,
};

/* l4_wkup -> gpio1 */
static struct omap_hwmod_addr_space omap2420_gpio1_addr_space[] = {
	{
		.pa_start	= 0x48018000,
		.pa_end		= 0x480181ff,
		.flags		= ADDR_TYPE_RT
	},
};

static struct omap_hwmod_ocp_if omap2420_l4_wkup__gpio1 = {
	.master		= &omap2420_l4_wkup_hwmod,
	.slave		= &omap2420_gpio1_hwmod,
	.clk		= "gpios_ick",
	.addr		= omap2420_gpio1_addr_space,
	.addr_cnt	= ARRAY_SIZE(omap2420_gpio1_addr_space),
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* l4_wkup -> gpio2 */
static struct omap_hwmod_addr_space omap2420_gpio2_addr_space[] = {
	{
		.pa_start	= 0x4801a000,
		.pa_end		= 0x4801a1ff,
		.flags		= ADDR_TYPE_RT
	},
};

static struct omap_hwmod_ocp_if omap2420_l4_wkup__gpio2 = {
	.master		= &omap2420_l4_wkup_hwmod,
	.slave		= &omap2420_gpio2_hwmod,
	.clk		= "gpios_ick",
	.addr		= omap2420_gpio2_addr_space,
	.addr_cnt	= ARRAY_SIZE(omap2420_gpio2_addr_space),
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* l4_wkup -> gpio3 */
static struct omap_hwmod_addr_space omap2420_gpio3_addr_space[] = {
	{
		.pa_start	= 0x4801c000,
		.pa_end		= 0x4801c1ff,
		.flags		= ADDR_TYPE_RT
	},
};

static struct omap_hwmod_ocp_if omap2420_l4_wkup__gpio3 = {
	.master		= &omap2420_l4_wkup_hwmod,
	.slave		= &omap2420_gpio3_hwmod,
	.clk		= "gpios_ick",
	.addr		= omap2420_gpio3_addr_space,
	.addr_cnt	= ARRAY_SIZE(omap2420_gpio3_addr_space),
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* l4_wkup -> gpio4 */
static struct omap_hwmod_addr_space omap2420_gpio4_addr_space[] = {
	{
		.pa_start	= 0x4801e000,
		.pa_end		= 0x4801e1ff,
		.flags		= ADDR_TYPE_RT
	},
};

static struct omap_hwmod_ocp_if omap2420_l4_wkup__gpio4 = {
	.master		= &omap2420_l4_wkup_hwmod,
	.slave		= &omap2420_gpio4_hwmod,
	.clk		= "gpios_ick",
	.addr		= omap2420_gpio4_addr_space,
	.addr_cnt	= ARRAY_SIZE(omap2420_gpio4_addr_space),
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* gpio dev_attr */
static struct omap_gpio_dev_attr gpio_dev_attr = {
	.bank_width = 32,
	.dbck_flag = false,
};

static struct omap_hwmod_class_sysconfig omap242x_gpio_sysc = {
	.rev_offs	= 0x0000,
	.sysc_offs	= 0x0010,
	.syss_offs	= 0x0014,
	.sysc_flags	= (SYSC_HAS_ENAWAKEUP | SYSC_HAS_SIDLEMODE |
			   SYSC_HAS_SOFTRESET | SYSC_HAS_AUTOIDLE |
			   SYSS_HAS_RESET_STATUS),
	.idlemodes	= (SIDLE_FORCE | SIDLE_NO | SIDLE_SMART),
	.sysc_fields    = &omap_hwmod_sysc_type1,
};

/*
 * 'gpio' class
 * general purpose io module
 */
static struct omap_hwmod_class omap242x_gpio_hwmod_class = {
	.name = "gpio",
	.sysc = &omap242x_gpio_sysc,
	.rev = 0,
};

/* gpio1 */
static struct omap_hwmod_irq_info omap242x_gpio1_irqs[] = {
	{ .irq = 29 }, /* INT_24XX_GPIO_BANK1 */
};

static struct omap_hwmod_ocp_if *omap2420_gpio1_slaves[] = {
	&omap2420_l4_wkup__gpio1,
};

static struct omap_hwmod omap2420_gpio1_hwmod = {
	.name		= "gpio1",
	.flags		= HWMOD_CONTROL_OPT_CLKS_IN_RESET,
	.mpu_irqs	= omap242x_gpio1_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(omap242x_gpio1_irqs),
	.main_clk	= "gpios_fck",
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP24XX_EN_GPIOS_SHIFT,
			.module_offs = WKUP_MOD,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP24XX_ST_GPIOS_SHIFT,
		},
	},
	.slaves		= omap2420_gpio1_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap2420_gpio1_slaves),
	.class		= &omap242x_gpio_hwmod_class,
	.dev_attr	= &gpio_dev_attr,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2420),
};

/* gpio2 */
static struct omap_hwmod_irq_info omap242x_gpio2_irqs[] = {
	{ .irq = 30 }, /* INT_24XX_GPIO_BANK2 */
};

static struct omap_hwmod_ocp_if *omap2420_gpio2_slaves[] = {
	&omap2420_l4_wkup__gpio2,
};

static struct omap_hwmod omap2420_gpio2_hwmod = {
	.name		= "gpio2",
	.flags		= HWMOD_CONTROL_OPT_CLKS_IN_RESET,
	.mpu_irqs	= omap242x_gpio2_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(omap242x_gpio2_irqs),
	.main_clk	= "gpios_fck",
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP24XX_EN_GPIOS_SHIFT,
			.module_offs = WKUP_MOD,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP24XX_ST_GPIOS_SHIFT,
		},
	},
	.slaves		= omap2420_gpio2_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap2420_gpio2_slaves),
	.class		= &omap242x_gpio_hwmod_class,
	.dev_attr	= &gpio_dev_attr,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2420),
};

/* gpio3 */
static struct omap_hwmod_irq_info omap242x_gpio3_irqs[] = {
	{ .irq = 31 }, /* INT_24XX_GPIO_BANK3 */
};

static struct omap_hwmod_ocp_if *omap2420_gpio3_slaves[] = {
	&omap2420_l4_wkup__gpio3,
};

static struct omap_hwmod omap2420_gpio3_hwmod = {
	.name		= "gpio3",
	.flags		= HWMOD_CONTROL_OPT_CLKS_IN_RESET,
	.mpu_irqs	= omap242x_gpio3_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(omap242x_gpio3_irqs),
	.main_clk	= "gpios_fck",
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP24XX_EN_GPIOS_SHIFT,
			.module_offs = WKUP_MOD,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP24XX_ST_GPIOS_SHIFT,
		},
	},
	.slaves		= omap2420_gpio3_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap2420_gpio3_slaves),
	.class		= &omap242x_gpio_hwmod_class,
	.dev_attr	= &gpio_dev_attr,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2420),
};

/* gpio4 */
static struct omap_hwmod_irq_info omap242x_gpio4_irqs[] = {
	{ .irq = 32 }, /* INT_24XX_GPIO_BANK4 */
};

static struct omap_hwmod_ocp_if *omap2420_gpio4_slaves[] = {
	&omap2420_l4_wkup__gpio4,
};

static struct omap_hwmod omap2420_gpio4_hwmod = {
	.name		= "gpio4",
	.flags		= HWMOD_CONTROL_OPT_CLKS_IN_RESET,
	.mpu_irqs	= omap242x_gpio4_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(omap242x_gpio4_irqs),
	.main_clk	= "gpios_fck",
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP24XX_EN_GPIOS_SHIFT,
			.module_offs = WKUP_MOD,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP24XX_ST_GPIOS_SHIFT,
		},
	},
	.slaves		= omap2420_gpio4_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap2420_gpio4_slaves),
	.class		= &omap242x_gpio_hwmod_class,
	.dev_attr	= &gpio_dev_attr,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2420),
};

/* system dma */
static struct omap_hwmod_class_sysconfig omap2420_dma_sysc = {
	.rev_offs	= 0x0000,
	.sysc_offs	= 0x002c,
	.syss_offs	= 0x0028,
	.sysc_flags	= (SYSC_HAS_SOFTRESET | SYSC_HAS_MIDLEMODE |
			   SYSC_HAS_CLOCKACTIVITY | SYSC_HAS_EMUFREE |
			   SYSC_HAS_AUTOIDLE | SYSS_HAS_RESET_STATUS),
	.idlemodes	= (MSTANDBY_FORCE | MSTANDBY_NO | MSTANDBY_SMART),
	.sysc_fields	= &omap_hwmod_sysc_type1,
};

static struct omap_hwmod_class omap2420_dma_hwmod_class = {
	.name = "dma",
	.sysc = &omap2420_dma_sysc,
};

/* dma attributes */
static struct omap_dma_dev_attr dma_dev_attr = {
	.dev_caps  = RESERVE_CHANNEL | DMA_LINKED_LCH | GLOBAL_PRIORITY |
						IS_CSSA_32 | IS_CDSA_32,
	.lch_count = 32,
};

static struct omap_hwmod_irq_info omap2420_dma_system_irqs[] = {
	{ .name = "0", .irq = 12 }, /* INT_24XX_SDMA_IRQ0 */
	{ .name = "1", .irq = 13 }, /* INT_24XX_SDMA_IRQ1 */
	{ .name = "2", .irq = 14 }, /* INT_24XX_SDMA_IRQ2 */
	{ .name = "3", .irq = 15 }, /* INT_24XX_SDMA_IRQ3 */
};

static struct omap_hwmod_addr_space omap2420_dma_system_addrs[] = {
	{
		.pa_start	= 0x48056000,
		.pa_end		= 0x48056fff,
		.flags		= ADDR_TYPE_RT
	},
};

/* dma_system -> L3 */
static struct omap_hwmod_ocp_if omap2420_dma_system__l3 = {
	.master		= &omap2420_dma_system_hwmod,
	.slave		= &omap2420_l3_main_hwmod,
	.clk		= "core_l3_ck",
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* dma_system master ports */
static struct omap_hwmod_ocp_if *omap2420_dma_system_masters[] = {
	&omap2420_dma_system__l3,
};

/* l4_core -> dma_system */
static struct omap_hwmod_ocp_if omap2420_l4_core__dma_system = {
	.master		= &omap2420_l4_core_hwmod,
	.slave		= &omap2420_dma_system_hwmod,
	.clk		= "sdma_ick",
	.addr		= omap2420_dma_system_addrs,
	.addr_cnt	= ARRAY_SIZE(omap2420_dma_system_addrs),
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* dma_system slave ports */
static struct omap_hwmod_ocp_if *omap2420_dma_system_slaves[] = {
	&omap2420_l4_core__dma_system,
};

static struct omap_hwmod omap2420_dma_system_hwmod = {
	.name		= "dma",
	.class		= &omap2420_dma_hwmod_class,
	.mpu_irqs	= omap2420_dma_system_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(omap2420_dma_system_irqs),
	.main_clk	= "core_l3_ck",
	.slaves		= omap2420_dma_system_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap2420_dma_system_slaves),
	.masters	= omap2420_dma_system_masters,
	.masters_cnt	= ARRAY_SIZE(omap2420_dma_system_masters),
	.dev_attr	= &dma_dev_attr,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2420),
	.flags		= HWMOD_NO_IDLEST,
};

/*
 * 'mailbox' class
 * mailbox module allowing communication between the on-chip processors
 * using a queued mailbox-interrupt mechanism.
 */

static struct omap_hwmod_class_sysconfig omap2420_mailbox_sysc = {
	.rev_offs	= 0x000,
	.sysc_offs	= 0x010,
	.syss_offs	= 0x014,
	.sysc_flags	= (SYSC_HAS_CLOCKACTIVITY | SYSC_HAS_SIDLEMODE |
			   SYSC_HAS_SOFTRESET | SYSC_HAS_AUTOIDLE),
	.idlemodes	= (SIDLE_FORCE | SIDLE_NO | SIDLE_SMART),
	.sysc_fields	= &omap_hwmod_sysc_type1,
};

static struct omap_hwmod_class omap2420_mailbox_hwmod_class = {
	.name = "mailbox",
	.sysc = &omap2420_mailbox_sysc,
};

/* mailbox */
static struct omap_hwmod omap2420_mailbox_hwmod;
static struct omap_hwmod_irq_info omap2420_mailbox_irqs[] = {
	{ .name = "dsp", .irq = 26 },
	{ .name = "iva", .irq = 34 },
};

static struct omap_hwmod_addr_space omap2420_mailbox_addrs[] = {
	{
		.pa_start	= 0x48094000,
		.pa_end		= 0x480941ff,
		.flags		= ADDR_TYPE_RT,
	},
};

/* l4_core -> mailbox */
static struct omap_hwmod_ocp_if omap2420_l4_core__mailbox = {
	.master		= &omap2420_l4_core_hwmod,
	.slave		= &omap2420_mailbox_hwmod,
	.addr		= omap2420_mailbox_addrs,
	.addr_cnt	= ARRAY_SIZE(omap2420_mailbox_addrs),
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* mailbox slave ports */
static struct omap_hwmod_ocp_if *omap2420_mailbox_slaves[] = {
	&omap2420_l4_core__mailbox,
};

static struct omap_hwmod omap2420_mailbox_hwmod = {
	.name		= "mailbox",
	.class		= &omap2420_mailbox_hwmod_class,
	.mpu_irqs	= omap2420_mailbox_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(omap2420_mailbox_irqs),
	.main_clk	= "mailboxes_ick",
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP24XX_EN_MAILBOXES_SHIFT,
			.module_offs = CORE_MOD,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP24XX_ST_MAILBOXES_SHIFT,
		},
	},
	.slaves		= omap2420_mailbox_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap2420_mailbox_slaves),
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2420),
};

/*
 * 'mcspi' class
 * multichannel serial port interface (mcspi) / master/slave synchronous serial
 * bus
 */

static struct omap_hwmod_class_sysconfig omap2420_mcspi_sysc = {
	.rev_offs	= 0x0000,
	.sysc_offs	= 0x0010,
	.syss_offs	= 0x0014,
	.sysc_flags	= (SYSC_HAS_CLOCKACTIVITY | SYSC_HAS_SIDLEMODE |
				SYSC_HAS_ENAWAKEUP | SYSC_HAS_SOFTRESET |
				SYSC_HAS_AUTOIDLE | SYSS_HAS_RESET_STATUS),
	.idlemodes	= (SIDLE_FORCE | SIDLE_NO | SIDLE_SMART),
	.sysc_fields    = &omap_hwmod_sysc_type1,
};

static struct omap_hwmod_class omap2420_mcspi_class = {
	.name = "mcspi",
	.sysc = &omap2420_mcspi_sysc,
	.rev = OMAP2_MCSPI_REV,
};

/* mcspi1 */
static struct omap_hwmod_irq_info omap2420_mcspi1_mpu_irqs[] = {
	{ .irq = 65 },
};

static struct omap_hwmod_dma_info omap2420_mcspi1_sdma_reqs[] = {
	{ .name = "tx0", .dma_req = 35 }, /* DMA_SPI1_TX0 */
	{ .name = "rx0", .dma_req = 36 }, /* DMA_SPI1_RX0 */
	{ .name = "tx1", .dma_req = 37 }, /* DMA_SPI1_TX1 */
	{ .name = "rx1", .dma_req = 38 }, /* DMA_SPI1_RX1 */
	{ .name = "tx2", .dma_req = 39 }, /* DMA_SPI1_TX2 */
	{ .name = "rx2", .dma_req = 40 }, /* DMA_SPI1_RX2 */
	{ .name = "tx3", .dma_req = 41 }, /* DMA_SPI1_TX3 */
	{ .name = "rx3", .dma_req = 42 }, /* DMA_SPI1_RX3 */
};

static struct omap_hwmod_ocp_if *omap2420_mcspi1_slaves[] = {
	&omap2420_l4_core__mcspi1,
};

static struct omap2_mcspi_dev_attr omap_mcspi1_dev_attr = {
	.num_chipselect = 4,
};

static struct omap_hwmod omap2420_mcspi1_hwmod = {
	.name		= "mcspi1_hwmod",
	.mpu_irqs	= omap2420_mcspi1_mpu_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(omap2420_mcspi1_mpu_irqs),
	.sdma_reqs	= omap2420_mcspi1_sdma_reqs,
	.sdma_reqs_cnt	= ARRAY_SIZE(omap2420_mcspi1_sdma_reqs),
	.main_clk	= "mcspi1_fck",
	.prcm		= {
		.omap2 = {
			.module_offs = CORE_MOD,
			.prcm_reg_id = 1,
			.module_bit = OMAP24XX_EN_MCSPI1_SHIFT,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP24XX_ST_MCSPI1_SHIFT,
		},
	},
	.slaves		= omap2420_mcspi1_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap2420_mcspi1_slaves),
	.class		= &omap2420_mcspi_class,
	.dev_attr       = &omap_mcspi1_dev_attr,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2420),
};

/* mcspi2 */
static struct omap_hwmod_irq_info omap2420_mcspi2_mpu_irqs[] = {
	{ .irq = 66 },
};

static struct omap_hwmod_dma_info omap2420_mcspi2_sdma_reqs[] = {
	{ .name = "tx0", .dma_req = 43 }, /* DMA_SPI2_TX0 */
	{ .name = "rx0", .dma_req = 44 }, /* DMA_SPI2_RX0 */
	{ .name = "tx1", .dma_req = 45 }, /* DMA_SPI2_TX1 */
	{ .name = "rx1", .dma_req = 46 }, /* DMA_SPI2_RX1 */
};

static struct omap_hwmod_ocp_if *omap2420_mcspi2_slaves[] = {
	&omap2420_l4_core__mcspi2,
};

static struct omap2_mcspi_dev_attr omap_mcspi2_dev_attr = {
	.num_chipselect = 2,
};

static struct omap_hwmod omap2420_mcspi2_hwmod = {
	.name		= "mcspi2_hwmod",
	.mpu_irqs	= omap2420_mcspi2_mpu_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(omap2420_mcspi2_mpu_irqs),
	.sdma_reqs	= omap2420_mcspi2_sdma_reqs,
	.sdma_reqs_cnt	= ARRAY_SIZE(omap2420_mcspi2_sdma_reqs),
	.main_clk	= "mcspi2_fck",
	.prcm		= {
		.omap2 = {
			.module_offs = CORE_MOD,
			.prcm_reg_id = 1,
			.module_bit = OMAP24XX_EN_MCSPI2_SHIFT,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP24XX_ST_MCSPI2_SHIFT,
		},
	},
	.slaves		= omap2420_mcspi2_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap2420_mcspi2_slaves),
	.class		= &omap2420_mcspi_class,
	.dev_attr       = &omap_mcspi2_dev_attr,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2420),
};

/*
 * 'mcbsp' class
 * multi channel buffered serial port controller
 */

static struct omap_hwmod_class omap2420_mcbsp_hwmod_class = {
	.name = "mcbsp",
};

/* mcbsp1 */
static struct omap_hwmod_irq_info omap2420_mcbsp1_irqs[] = {
	{ .name = "tx", .irq = 59 },
	{ .name = "rx", .irq = 60 },
};

static struct omap_hwmod_dma_info omap2420_mcbsp1_sdma_chs[] = {
	{ .name = "rx", .dma_req = 32 },
	{ .name = "tx", .dma_req = 31 },
};

static struct omap_hwmod_addr_space omap2420_mcbsp1_addrs[] = {
	{
		.name		= "mpu",
		.pa_start	= 0x48074000,
		.pa_end		= 0x480740ff,
		.flags		= ADDR_TYPE_RT
	},
};

/* l4_core -> mcbsp1 */
static struct omap_hwmod_ocp_if omap2420_l4_core__mcbsp1 = {
	.master		= &omap2420_l4_core_hwmod,
	.slave		= &omap2420_mcbsp1_hwmod,
	.clk		= "mcbsp1_ick",
	.addr		= omap2420_mcbsp1_addrs,
	.addr_cnt	= ARRAY_SIZE(omap2420_mcbsp1_addrs),
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* mcbsp1 slave ports */
static struct omap_hwmod_ocp_if *omap2420_mcbsp1_slaves[] = {
	&omap2420_l4_core__mcbsp1,
};

static struct omap_hwmod omap2420_mcbsp1_hwmod = {
	.name		= "mcbsp1",
	.class		= &omap2420_mcbsp_hwmod_class,
	.mpu_irqs	= omap2420_mcbsp1_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(omap2420_mcbsp1_irqs),
	.sdma_reqs	= omap2420_mcbsp1_sdma_chs,
	.sdma_reqs_cnt	= ARRAY_SIZE(omap2420_mcbsp1_sdma_chs),
	.main_clk	= "mcbsp1_fck",
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP24XX_EN_MCBSP1_SHIFT,
			.module_offs = CORE_MOD,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP24XX_ST_MCBSP1_SHIFT,
		},
	},
	.slaves		= omap2420_mcbsp1_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap2420_mcbsp1_slaves),
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2420),
};

/* mcbsp2 */
static struct omap_hwmod_irq_info omap2420_mcbsp2_irqs[] = {
	{ .name = "tx", .irq = 62 },
	{ .name = "rx", .irq = 63 },
};

static struct omap_hwmod_dma_info omap2420_mcbsp2_sdma_chs[] = {
	{ .name = "rx", .dma_req = 34 },
	{ .name = "tx", .dma_req = 33 },
};

static struct omap_hwmod_addr_space omap2420_mcbsp2_addrs[] = {
	{
		.name		= "mpu",
		.pa_start	= 0x48076000,
		.pa_end		= 0x480760ff,
		.flags		= ADDR_TYPE_RT
	},
};

/* l4_core -> mcbsp2 */
static struct omap_hwmod_ocp_if omap2420_l4_core__mcbsp2 = {
	.master		= &omap2420_l4_core_hwmod,
	.slave		= &omap2420_mcbsp2_hwmod,
	.clk		= "mcbsp2_ick",
	.addr		= omap2420_mcbsp2_addrs,
	.addr_cnt	= ARRAY_SIZE(omap2420_mcbsp2_addrs),
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* mcbsp2 slave ports */
static struct omap_hwmod_ocp_if *omap2420_mcbsp2_slaves[] = {
	&omap2420_l4_core__mcbsp2,
};

static struct omap_hwmod omap2420_mcbsp2_hwmod = {
	.name		= "mcbsp2",
	.class		= &omap2420_mcbsp_hwmod_class,
	.mpu_irqs	= omap2420_mcbsp2_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(omap2420_mcbsp2_irqs),
	.sdma_reqs	= omap2420_mcbsp2_sdma_chs,
	.sdma_reqs_cnt	= ARRAY_SIZE(omap2420_mcbsp2_sdma_chs),
	.main_clk	= "mcbsp2_fck",
	.prcm		= {
		.omap2 = {
			.prcm_reg_id = 1,
			.module_bit = OMAP24XX_EN_MCBSP2_SHIFT,
			.module_offs = CORE_MOD,
			.idlest_reg_id = 1,
			.idlest_idle_bit = OMAP24XX_ST_MCBSP2_SHIFT,
		},
	},
	.slaves		= omap2420_mcbsp2_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap2420_mcbsp2_slaves),
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2420),
};

static __initdata struct omap_hwmod *omap2420_hwmods[] = {
	&omap2420_l3_main_hwmod,
	&omap2420_l4_core_hwmod,
	&omap2420_l4_wkup_hwmod,
	&omap2420_mpu_hwmod,
	&omap2420_iva_hwmod,

	&omap2420_timer1_hwmod,
	&omap2420_timer2_hwmod,
	&omap2420_timer3_hwmod,
	&omap2420_timer4_hwmod,
	&omap2420_timer5_hwmod,
	&omap2420_timer6_hwmod,
	&omap2420_timer7_hwmod,
	&omap2420_timer8_hwmod,
	&omap2420_timer9_hwmod,
	&omap2420_timer10_hwmod,
	&omap2420_timer11_hwmod,
	&omap2420_timer12_hwmod,

	&omap2420_wd_timer2_hwmod,
	&omap2420_uart1_hwmod,
	&omap2420_uart2_hwmod,
	&omap2420_uart3_hwmod,
	/* dss class */
	&omap2420_dss_core_hwmod,
	&omap2420_dss_dispc_hwmod,
	&omap2420_dss_rfbi_hwmod,
	&omap2420_dss_venc_hwmod,
	/* i2c class */
	&omap2420_i2c1_hwmod,
	&omap2420_i2c2_hwmod,

	/* gpio class */
	&omap2420_gpio1_hwmod,
	&omap2420_gpio2_hwmod,
	&omap2420_gpio3_hwmod,
	&omap2420_gpio4_hwmod,

	/* dma_system class*/
	&omap2420_dma_system_hwmod,

	/* mailbox class */
	&omap2420_mailbox_hwmod,

	/* mcbsp class */
	&omap2420_mcbsp1_hwmod,
	&omap2420_mcbsp2_hwmod,

	/* mcspi class */
	&omap2420_mcspi1_hwmod,
	&omap2420_mcspi2_hwmod,
	NULL,
};

int __init omap2420_hwmod_init(void)
{
	return omap_hwmod_register(omap2420_hwmods);
}
