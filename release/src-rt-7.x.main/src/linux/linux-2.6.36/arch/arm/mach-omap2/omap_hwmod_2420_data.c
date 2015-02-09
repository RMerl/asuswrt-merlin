
#include <plat/omap_hwmod.h>
#include <mach/irqs.h>
#include <plat/cpu.h>
#include <plat/dma.h>

#include "omap_hwmod_common_data.h"

#include "prm-regbits-24xx.h"

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

/* L4_CORE -> L4_WKUP interface */
static struct omap_hwmod_ocp_if omap2420_l4_core__l4_wkup = {
	.master	= &omap2420_l4_core_hwmod,
	.slave	= &omap2420_l4_wkup_hwmod,
	.user	= OCP_USER_MPU | OCP_USER_SDMA,
};

/* Slave interfaces on the L4_CORE interconnect */
static struct omap_hwmod_ocp_if *omap2420_l4_core_slaves[] = {
	&omap2420_l3_main__l4_core,
};

/* Master interfaces on the L4_CORE interconnect */
static struct omap_hwmod_ocp_if *omap2420_l4_core_masters[] = {
	&omap2420_l4_core__l4_wkup,
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

static __initdata struct omap_hwmod *omap2420_hwmods[] = {
	&omap2420_l3_main_hwmod,
	&omap2420_l4_core_hwmod,
	&omap2420_l4_wkup_hwmod,
	&omap2420_mpu_hwmod,
	&omap2420_iva_hwmod,
	NULL,
};

int __init omap2420_hwmod_init(void)
{
	return omap_hwmod_init(omap2420_hwmods);
}
