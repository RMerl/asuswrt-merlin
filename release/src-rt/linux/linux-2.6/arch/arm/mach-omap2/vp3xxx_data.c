/*
 * OMAP3 Voltage Processor (VP) data
 *
 * Copyright (C) 2007, 2010 Texas Instruments, Inc.
 * Rajendra Nayak <rnayak@ti.com>
 * Lesly A M <x0080970@ti.com>
 * Thara Gopinath <thara@ti.com>
 *
 * Copyright (C) 2008, 2011 Nokia Corporation
 * Kalle Jokiniemi
 * Paul Walmsley
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/io.h>
#include <linux/err.h>
#include <linux/init.h>

#include <plat/common.h>

#include "prm-regbits-34xx.h"
#include "voltage.h"

#include "vp.h"

/*
 * VP data common to 34xx/36xx chips
 * XXX This stuff presumably belongs in the vp3xxx.c or vp.c file.
 */
static const struct omap_vp_common_data omap3_vp_common = {
	.vpconfig_erroroffset_shift = OMAP3430_ERROROFFSET_SHIFT,
	.vpconfig_errorgain_mask = OMAP3430_ERRORGAIN_MASK,
	.vpconfig_errorgain_shift = OMAP3430_ERRORGAIN_SHIFT,
	.vpconfig_initvoltage_shift = OMAP3430_INITVOLTAGE_SHIFT,
	.vpconfig_initvoltage_mask = OMAP3430_INITVOLTAGE_MASK,
	.vpconfig_timeouten = OMAP3430_TIMEOUTEN_MASK,
	.vpconfig_initvdd = OMAP3430_INITVDD_MASK,
	.vpconfig_forceupdate = OMAP3430_FORCEUPDATE_MASK,
	.vpconfig_vpenable = OMAP3430_VPENABLE_MASK,
	.vstepmin_smpswaittimemin_shift = OMAP3430_SMPSWAITTIMEMIN_SHIFT,
	.vstepmax_smpswaittimemax_shift = OMAP3430_SMPSWAITTIMEMAX_SHIFT,
	.vstepmin_stepmin_shift = OMAP3430_VSTEPMIN_SHIFT,
	.vstepmax_stepmax_shift = OMAP3430_VSTEPMAX_SHIFT,
	.vlimitto_vddmin_shift = OMAP3430_VDDMIN_SHIFT,
	.vlimitto_vddmax_shift = OMAP3430_VDDMAX_SHIFT,
	.vlimitto_timeout_shift = OMAP3430_TIMEOUT_SHIFT,
};

static const struct omap_vp_prm_irqst_data omap3_vp1_prm_irqst_data = {
	.prm_irqst_reg = OMAP3_PRM_IRQSTATUS_MPU_OFFSET,
	.tranxdone_status = OMAP3430_VP1_TRANXDONE_ST_MASK,
};

struct omap_vp_instance_data omap3_vp1_data = {
	.vp_common = &omap3_vp_common,
	.vpconfig = OMAP3_PRM_VP1_CONFIG_OFFSET,
	.vstepmin = OMAP3_PRM_VP1_VSTEPMIN_OFFSET,
	.vstepmax = OMAP3_PRM_VP1_VSTEPMAX_OFFSET,
	.vlimitto = OMAP3_PRM_VP1_VLIMITTO_OFFSET,
	.vstatus = OMAP3_PRM_VP1_STATUS_OFFSET,
	.voltage = OMAP3_PRM_VP1_VOLTAGE_OFFSET,
	.prm_irqst_data = &omap3_vp1_prm_irqst_data,
};

static const struct omap_vp_prm_irqst_data omap3_vp2_prm_irqst_data = {
	.prm_irqst_reg = OMAP3_PRM_IRQSTATUS_MPU_OFFSET,
	.tranxdone_status = OMAP3430_VP2_TRANXDONE_ST_MASK,
};

struct omap_vp_instance_data omap3_vp2_data = {
	.vp_common = &omap3_vp_common,
	.vpconfig = OMAP3_PRM_VP2_CONFIG_OFFSET,
	.vstepmin = OMAP3_PRM_VP2_VSTEPMIN_OFFSET,
	.vstepmax = OMAP3_PRM_VP2_VSTEPMAX_OFFSET,
	.vlimitto = OMAP3_PRM_VP2_VLIMITTO_OFFSET,
	.vstatus = OMAP3_PRM_VP2_STATUS_OFFSET,
	.voltage = OMAP3_PRM_VP2_VOLTAGE_OFFSET,
	.prm_irqst_data = &omap3_vp2_prm_irqst_data,
};
