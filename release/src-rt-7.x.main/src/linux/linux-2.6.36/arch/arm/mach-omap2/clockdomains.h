

/*
 * To-Do List
 * -> Port the Sleep/Wakeup dependencies for the domains
 *    from the Power domain framework
 */

#ifndef __ARCH_ARM_MACH_OMAP2_CLOCKDOMAINS_H
#define __ARCH_ARM_MACH_OMAP2_CLOCKDOMAINS_H

#include <plat/clockdomain.h>
#include "cm.h"
#include "prm.h"


/* OMAP2/3-common wakeup dependencies */

/*
 * 2420/2430 PM_WKDEP_GFX: CORE, MPU, WKUP
 * 3430ES1 PM_WKDEP_GFX: adds IVA2, removes CORE
 * 3430ES2 PM_WKDEP_SGX: adds IVA2, removes CORE
 * These can share data since they will never be present simultaneously
 * on the same device.
 */
static struct clkdm_dep gfx_sgx_wkdeps[] = {
	{
		.clkdm_name = "core_l3_clkdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP24XX)
	},
	{
		.clkdm_name = "core_l4_clkdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP24XX)
	},
	{
		.clkdm_name = "iva2_clkdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP3430)
	},
	{
		.clkdm_name = "mpu_clkdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP24XX |
					    CHIP_IS_OMAP3430)
	},
	{
		.clkdm_name = "wkup_clkdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP24XX |
					    CHIP_IS_OMAP3430)
	},
	{ NULL },
};


/* 24XX-specific possible dependencies */

#ifdef CONFIG_ARCH_OMAP2

/* Wakeup dependency source arrays */

/* 2420/2430 PM_WKDEP_DSP: CORE, MPU, WKUP */
static struct clkdm_dep dsp_24xx_wkdeps[] = {
	{
		.clkdm_name = "core_l3_clkdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP24XX)
	},
	{
		.clkdm_name = "core_l4_clkdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP24XX)
	},
	{
		.clkdm_name = "mpu_clkdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP24XX)
	},
	{
		.clkdm_name = "wkup_clkdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP24XX)
	},
	{ NULL },
};

/*
 * 2420 PM_WKDEP_MPU: CORE, DSP, WKUP
 * 2430 adds MDM
 */
static struct clkdm_dep mpu_24xx_wkdeps[] = {
	{
		.clkdm_name = "core_l3_clkdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP24XX)
	},
	{
		.clkdm_name = "core_l4_clkdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP24XX)
	},
	{
		.clkdm_name = "dsp_clkdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP24XX)
	},
	{
		.clkdm_name = "wkup_clkdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP24XX)
	},
	{
		.clkdm_name = "mdm_clkdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP2430)
	},
	{ NULL },
};

/*
 * 2420 PM_WKDEP_CORE: DSP, GFX, MPU, WKUP
 * 2430 adds MDM
 */
static struct clkdm_dep core_24xx_wkdeps[] = {
	{
		.clkdm_name = "dsp_clkdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP24XX)
	},
	{
		.clkdm_name = "gfx_clkdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP24XX)
	},
	{
		.clkdm_name = "mpu_clkdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP24XX)
	},
	{
		.clkdm_name = "wkup_clkdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP24XX)
	},
	{
		.clkdm_name = "mdm_clkdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP2430)
	},
	{ NULL },
};

#endif


/* 2430-specific possible wakeup dependencies */

#ifdef CONFIG_ARCH_OMAP2430

/* 2430 PM_WKDEP_MDM: CORE, MPU, WKUP */
static struct clkdm_dep mdm_2430_wkdeps[] = {
	{
		.clkdm_name = "core_l3_clkdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP24XX)
	},
	{
		.clkdm_name = "core_l4_clkdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP24XX)
	},
	{
		.clkdm_name = "mpu_clkdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP24XX)
	},
	{
		.clkdm_name = "wkup_clkdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP24XX)
	},
	{ NULL },
};

#endif /* CONFIG_ARCH_OMAP2430 */


/* OMAP3-specific possible dependencies */

#ifdef CONFIG_ARCH_OMAP3

/* 3430: PM_WKDEP_PER: CORE, IVA2, MPU, WKUP */
static struct clkdm_dep per_wkdeps[] = {
	{
		.clkdm_name = "core_l3_clkdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP3430)
	},
	{
		.clkdm_name = "core_l4_clkdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP3430)
	},
	{
		.clkdm_name = "iva2_clkdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP3430)
	},
	{
		.clkdm_name = "mpu_clkdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP3430)
	},
	{
		.clkdm_name = "wkup_clkdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP3430)
	},
	{ NULL },
};

/* 3430ES2: PM_WKDEP_USBHOST: CORE, IVA2, MPU, WKUP */
static struct clkdm_dep usbhost_wkdeps[] = {
	{
		.clkdm_name = "core_l3_clkdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP3430)
	},
	{
		.clkdm_name = "core_l4_clkdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP3430)
	},
	{
		.clkdm_name = "iva2_clkdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP3430)
	},
	{
		.clkdm_name = "mpu_clkdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP3430)
	},
	{
		.clkdm_name = "wkup_clkdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP3430)
	},
	{ NULL },
};

/* 3430 PM_WKDEP_MPU: CORE, IVA2, DSS, PER */
static struct clkdm_dep mpu_3xxx_wkdeps[] = {
	{
		.clkdm_name = "core_l3_clkdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP3430)
	},
	{
		.clkdm_name = "core_l4_clkdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP3430)
	},
	{
		.clkdm_name = "iva2_clkdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP3430)
	},
	{
		.clkdm_name = "dss_clkdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP3430)
	},
	{
		.clkdm_name = "per_clkdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP3430)
	},
	{ NULL },
};

/* 3430 PM_WKDEP_IVA2: CORE, MPU, WKUP, DSS, PER */
static struct clkdm_dep iva2_wkdeps[] = {
	{
		.clkdm_name = "core_l3_clkdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP3430)
	},
	{
		.clkdm_name = "core_l4_clkdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP3430)
	},
	{
		.clkdm_name = "mpu_clkdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP3430)
	},
	{
		.clkdm_name = "wkup_clkdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP3430)
	},
	{
		.clkdm_name = "dss_clkdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP3430)
	},
	{
		.clkdm_name = "per_clkdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP3430)
	},
	{ NULL },
};


/* 3430 PM_WKDEP_CAM: IVA2, MPU, WKUP */
static struct clkdm_dep cam_wkdeps[] = {
	{
		.clkdm_name = "iva2_clkdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP3430)
	},
	{
		.clkdm_name = "mpu_clkdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP3430)
	},
	{
		.clkdm_name = "wkup_clkdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP3430)
	},
	{ NULL },
};

/* 3430 PM_WKDEP_DSS: IVA2, MPU, WKUP */
static struct clkdm_dep dss_wkdeps[] = {
	{
		.clkdm_name = "iva2_clkdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP3430)
	},
	{
		.clkdm_name = "mpu_clkdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP3430)
	},
	{
		.clkdm_name = "wkup_clkdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP3430)
	},
	{ NULL },
};

/* 3430: PM_WKDEP_NEON: MPU */
static struct clkdm_dep neon_wkdeps[] = {
	{
		.clkdm_name = "mpu_clkdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP3430)
	},
	{ NULL },
};


/* Sleep dependency source arrays for OMAP3-specific clkdms */

/* 3430: CM_SLEEPDEP_DSS: MPU, IVA */
static struct clkdm_dep dss_sleepdeps[] = {
	{
		.clkdm_name = "mpu_clkdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP3430)
	},
	{
		.clkdm_name = "iva2_clkdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP3430)
	},
	{ NULL },
};

/* 3430: CM_SLEEPDEP_PER: MPU, IVA */
static struct clkdm_dep per_sleepdeps[] = {
	{
		.clkdm_name = "mpu_clkdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP3430)
	},
	{
		.clkdm_name = "iva2_clkdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP3430)
	},
	{ NULL },
};

/* 3430ES2: CM_SLEEPDEP_USBHOST: MPU, IVA */
static struct clkdm_dep usbhost_sleepdeps[] = {
	{
		.clkdm_name = "mpu_clkdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP3430)
	},
	{
		.clkdm_name = "iva2_clkdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP3430)
	},
	{ NULL },
};

/* 3430: CM_SLEEPDEP_CAM: MPU */
static struct clkdm_dep cam_sleepdeps[] = {
	{
		.clkdm_name = "mpu_clkdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP3430)
	},
	{ NULL },
};

/*
 * 3430ES1: CM_SLEEPDEP_GFX: MPU
 * 3430ES2: CM_SLEEPDEP_SGX: MPU
 * These can share data since they will never be present simultaneously
 * on the same device.
 */
static struct clkdm_dep gfx_sgx_sleepdeps[] = {
	{
		.clkdm_name = "mpu_clkdm",
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP3430)
	},
	{ NULL },
};

#endif /* CONFIG_ARCH_OMAP3 */


/*
 * OMAP2/3-common clockdomains
 *
 * Even though the 2420 has a single PRCM module from the
 * interconnect's perspective, internally it does appear to have
 * separate PRM and CM clockdomains.  The usual test case is
 * sys_clkout/sys_clkout2.
 */

#if defined(CONFIG_ARCH_OMAP2) || defined(CONFIG_ARCH_OMAP3)

/* This is an implicit clockdomain - it is never defined as such in TRM */
static struct clockdomain wkup_clkdm = {
	.name		= "wkup_clkdm",
	.pwrdm		= { .name = "wkup_pwrdm" },
	.dep_bit	= OMAP_EN_WKUP_SHIFT,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP24XX | CHIP_IS_OMAP3430),
};

static struct clockdomain prm_clkdm = {
	.name		= "prm_clkdm",
	.pwrdm		= { .name = "wkup_pwrdm" },
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP24XX | CHIP_IS_OMAP3430),
};

static struct clockdomain cm_clkdm = {
	.name		= "cm_clkdm",
	.pwrdm		= { .name = "core_pwrdm" },
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP24XX | CHIP_IS_OMAP3430),
};

#endif

/*
 * 2420-only clockdomains
 */

#if defined(CONFIG_ARCH_OMAP2420)

static struct clockdomain mpu_2420_clkdm = {
	.name		= "mpu_clkdm",
	.pwrdm		= { .name = "mpu_pwrdm" },
	.flags		= CLKDM_CAN_HWSUP,
	.clkstctrl_reg  = OMAP2420_CM_REGADDR(MPU_MOD, OMAP2_CM_CLKSTCTRL),
	.wkdep_srcs	= mpu_24xx_wkdeps,
	.clktrctrl_mask = OMAP24XX_AUTOSTATE_MPU_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2420),
};

static struct clockdomain iva1_2420_clkdm = {
	.name		= "iva1_clkdm",
	.pwrdm		= { .name = "dsp_pwrdm" },
	.flags		= CLKDM_CAN_HWSUP_SWSUP,
	.clkstctrl_reg  = OMAP2420_CM_REGADDR(OMAP24XX_DSP_MOD,
						 OMAP2_CM_CLKSTCTRL),
	.dep_bit	= OMAP24XX_PM_WKDEP_MPU_EN_DSP_SHIFT,
	.wkdep_srcs	= dsp_24xx_wkdeps,
	.clktrctrl_mask = OMAP2420_AUTOSTATE_IVA_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2420),
};

static struct clockdomain dsp_2420_clkdm = {
	.name		= "dsp_clkdm",
	.pwrdm		= { .name = "dsp_pwrdm" },
	.flags		= CLKDM_CAN_HWSUP_SWSUP,
	.clkstctrl_reg  = OMAP2420_CM_REGADDR(OMAP24XX_DSP_MOD,
						 OMAP2_CM_CLKSTCTRL),
	.clktrctrl_mask = OMAP24XX_AUTOSTATE_DSP_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2420),
};

static struct clockdomain gfx_2420_clkdm = {
	.name		= "gfx_clkdm",
	.pwrdm		= { .name = "gfx_pwrdm" },
	.flags		= CLKDM_CAN_HWSUP_SWSUP,
	.clkstctrl_reg  = OMAP2420_CM_REGADDR(GFX_MOD, OMAP2_CM_CLKSTCTRL),
	.wkdep_srcs	= gfx_sgx_wkdeps,
	.clktrctrl_mask = OMAP24XX_AUTOSTATE_GFX_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2420),
};

static struct clockdomain core_l3_2420_clkdm = {
	.name		= "core_l3_clkdm",
	.pwrdm		= { .name = "core_pwrdm" },
	.flags		= CLKDM_CAN_HWSUP,
	.clkstctrl_reg  = OMAP2420_CM_REGADDR(CORE_MOD, OMAP2_CM_CLKSTCTRL),
	.wkdep_srcs	= core_24xx_wkdeps,
	.clktrctrl_mask = OMAP24XX_AUTOSTATE_L3_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2420),
};

static struct clockdomain core_l4_2420_clkdm = {
	.name		= "core_l4_clkdm",
	.pwrdm		= { .name = "core_pwrdm" },
	.flags		= CLKDM_CAN_HWSUP,
	.clkstctrl_reg  = OMAP2420_CM_REGADDR(CORE_MOD, OMAP2_CM_CLKSTCTRL),
	.wkdep_srcs	= core_24xx_wkdeps,
	.clktrctrl_mask = OMAP24XX_AUTOSTATE_L4_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2420),
};

static struct clockdomain dss_2420_clkdm = {
	.name		= "dss_clkdm",
	.pwrdm		= { .name = "core_pwrdm" },
	.flags		= CLKDM_CAN_HWSUP,
	.clkstctrl_reg  = OMAP2420_CM_REGADDR(CORE_MOD, OMAP2_CM_CLKSTCTRL),
	.clktrctrl_mask = OMAP24XX_AUTOSTATE_DSS_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2420),
};

#endif   /* CONFIG_ARCH_OMAP2420 */


/*
 * 2430-only clockdomains
 */

#if defined(CONFIG_ARCH_OMAP2430)

static struct clockdomain mpu_2430_clkdm = {
	.name		= "mpu_clkdm",
	.pwrdm		= { .name = "mpu_pwrdm" },
	.flags		= CLKDM_CAN_HWSUP_SWSUP,
	.clkstctrl_reg  = OMAP2430_CM_REGADDR(MPU_MOD,
						 OMAP2_CM_CLKSTCTRL),
	.wkdep_srcs	= mpu_24xx_wkdeps,
	.clktrctrl_mask = OMAP24XX_AUTOSTATE_MPU_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2430),
};

/* Another case of bit name collisions between several registers: EN_MDM */
static struct clockdomain mdm_clkdm = {
	.name		= "mdm_clkdm",
	.pwrdm		= { .name = "mdm_pwrdm" },
	.flags		= CLKDM_CAN_HWSUP_SWSUP,
	.clkstctrl_reg  = OMAP2430_CM_REGADDR(OMAP2430_MDM_MOD,
						 OMAP2_CM_CLKSTCTRL),
	.dep_bit	= OMAP2430_PM_WKDEP_MPU_EN_MDM_SHIFT,
	.wkdep_srcs	= mdm_2430_wkdeps,
	.clktrctrl_mask = OMAP2430_AUTOSTATE_MDM_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2430),
};

static struct clockdomain dsp_2430_clkdm = {
	.name		= "dsp_clkdm",
	.pwrdm		= { .name = "dsp_pwrdm" },
	.flags		= CLKDM_CAN_HWSUP_SWSUP,
	.clkstctrl_reg  = OMAP2430_CM_REGADDR(OMAP24XX_DSP_MOD,
						 OMAP2_CM_CLKSTCTRL),
	.dep_bit	= OMAP24XX_PM_WKDEP_MPU_EN_DSP_SHIFT,
	.wkdep_srcs	= dsp_24xx_wkdeps,
	.clktrctrl_mask = OMAP24XX_AUTOSTATE_DSP_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2430),
};

static struct clockdomain gfx_2430_clkdm = {
	.name		= "gfx_clkdm",
	.pwrdm		= { .name = "gfx_pwrdm" },
	.flags		= CLKDM_CAN_HWSUP_SWSUP,
	.clkstctrl_reg  = OMAP2430_CM_REGADDR(GFX_MOD, OMAP2_CM_CLKSTCTRL),
	.wkdep_srcs	= gfx_sgx_wkdeps,
	.clktrctrl_mask = OMAP24XX_AUTOSTATE_GFX_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2430),
};

static struct clockdomain core_l3_2430_clkdm = {
	.name		= "core_l3_clkdm",
	.pwrdm		= { .name = "core_pwrdm" },
	.flags		= CLKDM_CAN_HWSUP,
	.clkstctrl_reg  = OMAP2430_CM_REGADDR(CORE_MOD, OMAP2_CM_CLKSTCTRL),
	.dep_bit	= OMAP24XX_EN_CORE_SHIFT,
	.wkdep_srcs	= core_24xx_wkdeps,
	.clktrctrl_mask = OMAP24XX_AUTOSTATE_L3_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2430),
};

static struct clockdomain core_l4_2430_clkdm = {
	.name		= "core_l4_clkdm",
	.pwrdm		= { .name = "core_pwrdm" },
	.flags		= CLKDM_CAN_HWSUP,
	.clkstctrl_reg  = OMAP2430_CM_REGADDR(CORE_MOD, OMAP2_CM_CLKSTCTRL),
	.dep_bit	= OMAP24XX_EN_CORE_SHIFT,
	.wkdep_srcs	= core_24xx_wkdeps,
	.clktrctrl_mask = OMAP24XX_AUTOSTATE_L4_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2430),
};

static struct clockdomain dss_2430_clkdm = {
	.name		= "dss_clkdm",
	.pwrdm		= { .name = "core_pwrdm" },
	.flags		= CLKDM_CAN_HWSUP,
	.clkstctrl_reg  = OMAP2430_CM_REGADDR(CORE_MOD, OMAP2_CM_CLKSTCTRL),
	.clktrctrl_mask = OMAP24XX_AUTOSTATE_DSS_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2430),
};

#endif    /* CONFIG_ARCH_OMAP2430 */


/*
 * OMAP3 clockdomains
 */

#if defined(CONFIG_ARCH_OMAP3)

static struct clockdomain mpu_3xxx_clkdm = {
	.name		= "mpu_clkdm",
	.pwrdm		= { .name = "mpu_pwrdm" },
	.flags		= CLKDM_CAN_HWSUP | CLKDM_CAN_FORCE_WAKEUP,
	.clkstctrl_reg	= OMAP34XX_CM_REGADDR(MPU_MOD, OMAP2_CM_CLKSTCTRL),
	.dep_bit	= OMAP3430_EN_MPU_SHIFT,
	.wkdep_srcs	= mpu_3xxx_wkdeps,
	.clktrctrl_mask = OMAP3430_CLKTRCTRL_MPU_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};

static struct clockdomain neon_clkdm = {
	.name		= "neon_clkdm",
	.pwrdm		= { .name = "neon_pwrdm" },
	.flags		= CLKDM_CAN_HWSUP_SWSUP,
	.clkstctrl_reg	= OMAP34XX_CM_REGADDR(OMAP3430_NEON_MOD,
						 OMAP2_CM_CLKSTCTRL),
	.wkdep_srcs	= neon_wkdeps,
	.clktrctrl_mask = OMAP3430_CLKTRCTRL_NEON_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};

static struct clockdomain iva2_clkdm = {
	.name		= "iva2_clkdm",
	.pwrdm		= { .name = "iva2_pwrdm" },
	.flags		= CLKDM_CAN_HWSUP_SWSUP,
	.clkstctrl_reg	= OMAP34XX_CM_REGADDR(OMAP3430_IVA2_MOD,
						 OMAP2_CM_CLKSTCTRL),
	.dep_bit	= OMAP3430_PM_WKDEP_MPU_EN_IVA2_SHIFT,
	.wkdep_srcs	= iva2_wkdeps,
	.clktrctrl_mask = OMAP3430_CLKTRCTRL_IVA2_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};

static struct clockdomain gfx_3430es1_clkdm = {
	.name		= "gfx_clkdm",
	.pwrdm		= { .name = "gfx_pwrdm" },
	.flags		= CLKDM_CAN_HWSUP_SWSUP,
	.clkstctrl_reg	= OMAP34XX_CM_REGADDR(GFX_MOD, OMAP2_CM_CLKSTCTRL),
	.wkdep_srcs	= gfx_sgx_wkdeps,
	.sleepdep_srcs	= gfx_sgx_sleepdeps,
	.clktrctrl_mask = OMAP3430ES1_CLKTRCTRL_GFX_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430ES1),
};

static struct clockdomain sgx_clkdm = {
	.name		= "sgx_clkdm",
	.pwrdm		= { .name = "sgx_pwrdm" },
	.flags		= CLKDM_CAN_HWSUP_SWSUP,
	.clkstctrl_reg	= OMAP34XX_CM_REGADDR(OMAP3430ES2_SGX_MOD,
						 OMAP2_CM_CLKSTCTRL),
	.wkdep_srcs	= gfx_sgx_wkdeps,
	.sleepdep_srcs	= gfx_sgx_sleepdeps,
	.clktrctrl_mask = OMAP3430ES2_CLKTRCTRL_SGX_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_GE_OMAP3430ES2),
};

/*
 * The die-to-die clockdomain was documented in the 34xx ES1 TRM, but
 * then that information was removed from the 34xx ES2+ TRM.  It is
 * unclear whether the core is still there, but the clockdomain logic
 * is there, and must be programmed to an appropriate state if the
 * CORE clockdomain is to become inactive.
 */
static struct clockdomain d2d_clkdm = {
	.name		= "d2d_clkdm",
	.pwrdm		= { .name = "core_pwrdm" },
	.flags		= CLKDM_CAN_HWSUP_SWSUP,
	.clkstctrl_reg	= OMAP34XX_CM_REGADDR(CORE_MOD, OMAP2_CM_CLKSTCTRL),
	.clktrctrl_mask = OMAP3430ES1_CLKTRCTRL_D2D_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};

static struct clockdomain core_l3_3xxx_clkdm = {
	.name		= "core_l3_clkdm",
	.pwrdm		= { .name = "core_pwrdm" },
	.flags		= CLKDM_CAN_HWSUP,
	.clkstctrl_reg	= OMAP34XX_CM_REGADDR(CORE_MOD, OMAP2_CM_CLKSTCTRL),
	.dep_bit	= OMAP3430_EN_CORE_SHIFT,
	.clktrctrl_mask = OMAP3430_CLKTRCTRL_L3_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};

static struct clockdomain core_l4_3xxx_clkdm = {
	.name		= "core_l4_clkdm",
	.pwrdm		= { .name = "core_pwrdm" },
	.flags		= CLKDM_CAN_HWSUP,
	.clkstctrl_reg	= OMAP34XX_CM_REGADDR(CORE_MOD, OMAP2_CM_CLKSTCTRL),
	.dep_bit	= OMAP3430_EN_CORE_SHIFT,
	.clktrctrl_mask = OMAP3430_CLKTRCTRL_L4_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};

/* Another case of bit name collisions between several registers: EN_DSS */
static struct clockdomain dss_3xxx_clkdm = {
	.name		= "dss_clkdm",
	.pwrdm		= { .name = "dss_pwrdm" },
	.flags		= CLKDM_CAN_HWSUP_SWSUP,
	.clkstctrl_reg	= OMAP34XX_CM_REGADDR(OMAP3430_DSS_MOD,
						 OMAP2_CM_CLKSTCTRL),
	.dep_bit	= OMAP3430_PM_WKDEP_MPU_EN_DSS_SHIFT,
	.wkdep_srcs	= dss_wkdeps,
	.sleepdep_srcs	= dss_sleepdeps,
	.clktrctrl_mask = OMAP3430_CLKTRCTRL_DSS_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};

static struct clockdomain cam_clkdm = {
	.name		= "cam_clkdm",
	.pwrdm		= { .name = "cam_pwrdm" },
	.flags		= CLKDM_CAN_HWSUP_SWSUP,
	.clkstctrl_reg	= OMAP34XX_CM_REGADDR(OMAP3430_CAM_MOD,
						 OMAP2_CM_CLKSTCTRL),
	.wkdep_srcs	= cam_wkdeps,
	.sleepdep_srcs	= cam_sleepdeps,
	.clktrctrl_mask = OMAP3430_CLKTRCTRL_CAM_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};

static struct clockdomain usbhost_clkdm = {
	.name		= "usbhost_clkdm",
	.pwrdm		= { .name = "usbhost_pwrdm" },
	.flags		= CLKDM_CAN_HWSUP_SWSUP,
	.clkstctrl_reg	= OMAP34XX_CM_REGADDR(OMAP3430ES2_USBHOST_MOD,
						 OMAP2_CM_CLKSTCTRL),
	.wkdep_srcs	= usbhost_wkdeps,
	.sleepdep_srcs	= usbhost_sleepdeps,
	.clktrctrl_mask = OMAP3430ES2_CLKTRCTRL_USBHOST_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_GE_OMAP3430ES2),
};

static struct clockdomain per_clkdm = {
	.name		= "per_clkdm",
	.pwrdm		= { .name = "per_pwrdm" },
	.flags		= CLKDM_CAN_HWSUP_SWSUP,
	.clkstctrl_reg	= OMAP34XX_CM_REGADDR(OMAP3430_PER_MOD,
						 OMAP2_CM_CLKSTCTRL),
	.dep_bit	= OMAP3430_EN_PER_SHIFT,
	.wkdep_srcs	= per_wkdeps,
	.sleepdep_srcs	= per_sleepdeps,
	.clktrctrl_mask = OMAP3430_CLKTRCTRL_PER_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};

/*
 * Disable hw supervised mode for emu_clkdm, because emu_pwrdm is
 * switched of even if sdti is in use
 */
static struct clockdomain emu_clkdm = {
	.name		= "emu_clkdm",
	.pwrdm		= { .name = "emu_pwrdm" },
	.flags		= /* CLKDM_CAN_ENABLE_AUTO |  */CLKDM_CAN_SWSUP,
	.clkstctrl_reg	= OMAP34XX_CM_REGADDR(OMAP3430_EMU_MOD,
						 OMAP2_CM_CLKSTCTRL),
	.clktrctrl_mask = OMAP3430_CLKTRCTRL_EMU_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};

static struct clockdomain dpll1_clkdm = {
	.name		= "dpll1_clkdm",
	.pwrdm		= { .name = "dpll1_pwrdm" },
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};

static struct clockdomain dpll2_clkdm = {
	.name		= "dpll2_clkdm",
	.pwrdm		= { .name = "dpll2_pwrdm" },
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};

static struct clockdomain dpll3_clkdm = {
	.name		= "dpll3_clkdm",
	.pwrdm		= { .name = "dpll3_pwrdm" },
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};

static struct clockdomain dpll4_clkdm = {
	.name		= "dpll4_clkdm",
	.pwrdm		= { .name = "dpll4_pwrdm" },
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};

static struct clockdomain dpll5_clkdm = {
	.name		= "dpll5_clkdm",
	.pwrdm		= { .name = "dpll5_pwrdm" },
	.omap_chip	= OMAP_CHIP_INIT(CHIP_GE_OMAP3430ES2),
};

#endif   /* CONFIG_ARCH_OMAP3 */

#include "clockdomains44xx.h"

/*
 * Clockdomain hwsup dependencies (OMAP3 only)
 */

static struct clkdm_autodep clkdm_autodeps[] = {
	{
		.clkdm	   = { .name = "mpu_clkdm" },
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP3430)
	},
	{
		.clkdm	   = { .name = "iva2_clkdm" },
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP3430)
	},
	{
		.clkdm	   = { .name = NULL },
	}
};

/*
 * List of clockdomain pointers per platform
 */

static struct clockdomain *clockdomains_omap[] = {

#if defined(CONFIG_ARCH_OMAP2) || defined(CONFIG_ARCH_OMAP3)
	&wkup_clkdm,
	&cm_clkdm,
	&prm_clkdm,
#endif

#ifdef CONFIG_ARCH_OMAP2420
	&mpu_2420_clkdm,
	&iva1_2420_clkdm,
	&dsp_2420_clkdm,
	&gfx_2420_clkdm,
	&core_l3_2420_clkdm,
	&core_l4_2420_clkdm,
	&dss_2420_clkdm,
#endif

#ifdef CONFIG_ARCH_OMAP2430
	&mpu_2430_clkdm,
	&mdm_clkdm,
	&dsp_2430_clkdm,
	&gfx_2430_clkdm,
	&core_l3_2430_clkdm,
	&core_l4_2430_clkdm,
	&dss_2430_clkdm,
#endif

#ifdef CONFIG_ARCH_OMAP3
	&mpu_3xxx_clkdm,
	&neon_clkdm,
	&iva2_clkdm,
	&gfx_3430es1_clkdm,
	&sgx_clkdm,
	&d2d_clkdm,
	&core_l3_3xxx_clkdm,
	&core_l4_3xxx_clkdm,
	&dss_3xxx_clkdm,
	&cam_clkdm,
	&usbhost_clkdm,
	&per_clkdm,
	&emu_clkdm,
	&dpll1_clkdm,
	&dpll2_clkdm,
	&dpll3_clkdm,
	&dpll4_clkdm,
	&dpll5_clkdm,
#endif

#ifdef CONFIG_ARCH_OMAP4
	&l4_cefuse_44xx_clkdm,
	&l4_cfg_44xx_clkdm,
	&tesla_44xx_clkdm,
	&l3_gfx_44xx_clkdm,
	&ivahd_44xx_clkdm,
	&l4_secure_44xx_clkdm,
	&l4_per_44xx_clkdm,
	&abe_44xx_clkdm,
	&l3_instr_44xx_clkdm,
	&l3_init_44xx_clkdm,
	&mpuss_44xx_clkdm,
	&mpu0_44xx_clkdm,
	&mpu1_44xx_clkdm,
	&l3_emif_44xx_clkdm,
	&l4_ao_44xx_clkdm,
	&ducati_44xx_clkdm,
	&l3_2_44xx_clkdm,
	&l3_1_44xx_clkdm,
	&l3_d2d_44xx_clkdm,
	&iss_44xx_clkdm,
	&l3_dss_44xx_clkdm,
	&l4_wkup_44xx_clkdm,
	&emu_sys_44xx_clkdm,
	&l3_dma_44xx_clkdm,
#endif

	NULL,
};

#endif
