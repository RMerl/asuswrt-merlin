/**
 * OMAP and TWL PMIC specific intializations.
 *
 * Copyright (C) 2010 Texas Instruments Incorporated.
 * Thara Gopinath
 * Copyright (C) 2009 Texas Instruments Incorporated.
 * Nishanth Menon
 * Copyright (C) 2009 Nokia Corporation
 * Paul Walmsley
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/err.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/i2c/twl.h>

#include "voltage.h"

#include "pm.h"

#define OMAP3_SRI2C_SLAVE_ADDR		0x12
#define OMAP3_VDD_MPU_SR_CONTROL_REG	0x00
#define OMAP3_VDD_CORE_SR_CONTROL_REG	0x01
#define OMAP3_VP_CONFIG_ERROROFFSET	0x00
#define OMAP3_VP_VSTEPMIN_VSTEPMIN	0x1
#define OMAP3_VP_VSTEPMAX_VSTEPMAX	0x04
#define OMAP3_VP_VLIMITTO_TIMEOUT_US	200

#define OMAP3430_VP1_VLIMITTO_VDDMIN	0x14
#define OMAP3430_VP1_VLIMITTO_VDDMAX	0x42
#define OMAP3430_VP2_VLIMITTO_VDDMIN	0x18
#define OMAP3430_VP2_VLIMITTO_VDDMAX	0x2c

#define OMAP3630_VP1_VLIMITTO_VDDMIN	0x18
#define OMAP3630_VP1_VLIMITTO_VDDMAX	0x3c
#define OMAP3630_VP2_VLIMITTO_VDDMIN	0x18
#define OMAP3630_VP2_VLIMITTO_VDDMAX	0x30

#define OMAP4_SRI2C_SLAVE_ADDR		0x12
#define OMAP4_VDD_MPU_SR_VOLT_REG	0x55
#define OMAP4_VDD_IVA_SR_VOLT_REG	0x5B
#define OMAP4_VDD_CORE_SR_VOLT_REG	0x61

#define OMAP4_VP_CONFIG_ERROROFFSET	0x00
#define OMAP4_VP_VSTEPMIN_VSTEPMIN	0x01
#define OMAP4_VP_VSTEPMAX_VSTEPMAX	0x04
#define OMAP4_VP_VLIMITTO_TIMEOUT_US	200

#define OMAP4_VP_MPU_VLIMITTO_VDDMIN	0xA
#define OMAP4_VP_MPU_VLIMITTO_VDDMAX	0x39
#define OMAP4_VP_IVA_VLIMITTO_VDDMIN	0xA
#define OMAP4_VP_IVA_VLIMITTO_VDDMAX	0x2D
#define OMAP4_VP_CORE_VLIMITTO_VDDMIN	0xA
#define OMAP4_VP_CORE_VLIMITTO_VDDMAX	0x28

static bool is_offset_valid;
static u8 smps_offset;
/*
 * Flag to ensure Smartreflex bit in TWL
 * being cleared in board file is not overwritten.
 */
static bool __initdata twl_sr_enable_autoinit;

#define TWL4030_DCDC_GLOBAL_CFG        0x06
#define REG_SMPS_OFFSET         0xE0
#define SMARTREFLEX_ENABLE     BIT(3)

static unsigned long twl4030_vsel_to_uv(const u8 vsel)
{
	return (((vsel * 125) + 6000)) * 100;
}

static u8 twl4030_uv_to_vsel(unsigned long uv)
{
	return DIV_ROUND_UP(uv - 600000, 12500);
}

static unsigned long twl6030_vsel_to_uv(const u8 vsel)
{
	/*
	 * In TWL6030 depending on the value of SMPS_OFFSET
	 * efuse register the voltage range supported in
	 * standard mode can be either between 0.6V - 1.3V or
	 * 0.7V - 1.4V. In TWL6030 ES1.0 SMPS_OFFSET efuse
	 * is programmed to all 0's where as starting from
	 * TWL6030 ES1.1 the efuse is programmed to 1
	 */
	if (!is_offset_valid) {
		twl_i2c_read_u8(TWL6030_MODULE_ID0, &smps_offset,
				REG_SMPS_OFFSET);
		is_offset_valid = true;
	}

	/*
	 * There is no specific formula for voltage to vsel
	 * conversion above 1.3V. There are special hardcoded
	 * values for voltages above 1.3V. Currently we are
	 * hardcoding only for 1.35 V which is used for 1GH OPP for
	 * OMAP4430.
	 */
	if (vsel == 0x3A)
		return 1350000;

	if (smps_offset & 0x8)
		return ((((vsel - 1) * 125) + 7000)) * 100;
	else
		return ((((vsel - 1) * 125) + 6000)) * 100;
}

static u8 twl6030_uv_to_vsel(unsigned long uv)
{
	/*
	 * In TWL6030 depending on the value of SMPS_OFFSET
	 * efuse register the voltage range supported in
	 * standard mode can be either between 0.6V - 1.3V or
	 * 0.7V - 1.4V. In TWL6030 ES1.0 SMPS_OFFSET efuse
	 * is programmed to all 0's where as starting from
	 * TWL6030 ES1.1 the efuse is programmed to 1
	 */
	if (!is_offset_valid) {
		twl_i2c_read_u8(TWL6030_MODULE_ID0, &smps_offset,
				REG_SMPS_OFFSET);
		is_offset_valid = true;
	}

	/*
	 * There is no specific formula for voltage to vsel
	 * conversion above 1.3V. There are special hardcoded
	 * values for voltages above 1.3V. Currently we are
	 * hardcoding only for 1.35 V which is used for 1GH OPP for
	 * OMAP4430.
	 */
	if (uv == 1350000)
		return 0x3A;

	if (smps_offset & 0x8)
		return DIV_ROUND_UP(uv - 700000, 12500) + 1;
	else
		return DIV_ROUND_UP(uv - 600000, 12500) + 1;
}

static struct omap_volt_pmic_info omap3_mpu_volt_info = {
	.slew_rate		= 4000,
	.step_size		= 12500,
	.on_volt		= 1200000,
	.onlp_volt		= 1000000,
	.ret_volt		= 975000,
	.off_volt		= 600000,
	.volt_setup_time	= 0xfff,
	.vp_erroroffset		= OMAP3_VP_CONFIG_ERROROFFSET,
	.vp_vstepmin		= OMAP3_VP_VSTEPMIN_VSTEPMIN,
	.vp_vstepmax		= OMAP3_VP_VSTEPMAX_VSTEPMAX,
	.vp_vddmin		= OMAP3430_VP1_VLIMITTO_VDDMIN,
	.vp_vddmax		= OMAP3430_VP1_VLIMITTO_VDDMAX,
	.vp_timeout_us		= OMAP3_VP_VLIMITTO_TIMEOUT_US,
	.i2c_slave_addr		= OMAP3_SRI2C_SLAVE_ADDR,
	.pmic_reg		= OMAP3_VDD_MPU_SR_CONTROL_REG,
	.vsel_to_uv		= twl4030_vsel_to_uv,
	.uv_to_vsel		= twl4030_uv_to_vsel,
};

static struct omap_volt_pmic_info omap3_core_volt_info = {
	.slew_rate		= 4000,
	.step_size		= 12500,
	.on_volt                = 1200000,
	.onlp_volt              = 1000000,
	.ret_volt               = 975000,
	.off_volt               = 600000,
	.volt_setup_time        = 0xfff,
	.vp_erroroffset		= OMAP3_VP_CONFIG_ERROROFFSET,
	.vp_vstepmin		= OMAP3_VP_VSTEPMIN_VSTEPMIN,
	.vp_vstepmax		= OMAP3_VP_VSTEPMAX_VSTEPMAX,
	.vp_vddmin		= OMAP3430_VP2_VLIMITTO_VDDMIN,
	.vp_vddmax		= OMAP3430_VP2_VLIMITTO_VDDMAX,
	.vp_timeout_us		= OMAP3_VP_VLIMITTO_TIMEOUT_US,
	.i2c_slave_addr		= OMAP3_SRI2C_SLAVE_ADDR,
	.pmic_reg		= OMAP3_VDD_CORE_SR_CONTROL_REG,
	.vsel_to_uv		= twl4030_vsel_to_uv,
	.uv_to_vsel		= twl4030_uv_to_vsel,
};

static struct omap_volt_pmic_info omap4_mpu_volt_info = {
	.slew_rate		= 4000,
	.step_size		= 12500,
	.on_volt		= 1350000,
	.onlp_volt		= 1350000,
	.ret_volt		= 837500,
	.off_volt		= 600000,
	.volt_setup_time	= 0,
	.vp_erroroffset		= OMAP4_VP_CONFIG_ERROROFFSET,
	.vp_vstepmin		= OMAP4_VP_VSTEPMIN_VSTEPMIN,
	.vp_vstepmax		= OMAP4_VP_VSTEPMAX_VSTEPMAX,
	.vp_vddmin		= OMAP4_VP_MPU_VLIMITTO_VDDMIN,
	.vp_vddmax		= OMAP4_VP_MPU_VLIMITTO_VDDMAX,
	.vp_timeout_us		= OMAP4_VP_VLIMITTO_TIMEOUT_US,
	.i2c_slave_addr		= OMAP4_SRI2C_SLAVE_ADDR,
	.pmic_reg		= OMAP4_VDD_MPU_SR_VOLT_REG,
	.vsel_to_uv		= twl6030_vsel_to_uv,
	.uv_to_vsel		= twl6030_uv_to_vsel,
};

static struct omap_volt_pmic_info omap4_iva_volt_info = {
	.slew_rate		= 4000,
	.step_size		= 12500,
	.on_volt		= 1100000,
	.onlp_volt		= 1100000,
	.ret_volt		= 837500,
	.off_volt		= 600000,
	.volt_setup_time	= 0,
	.vp_erroroffset		= OMAP4_VP_CONFIG_ERROROFFSET,
	.vp_vstepmin		= OMAP4_VP_VSTEPMIN_VSTEPMIN,
	.vp_vstepmax		= OMAP4_VP_VSTEPMAX_VSTEPMAX,
	.vp_vddmin		= OMAP4_VP_IVA_VLIMITTO_VDDMIN,
	.vp_vddmax		= OMAP4_VP_IVA_VLIMITTO_VDDMAX,
	.vp_timeout_us		= OMAP4_VP_VLIMITTO_TIMEOUT_US,
	.i2c_slave_addr		= OMAP4_SRI2C_SLAVE_ADDR,
	.pmic_reg		= OMAP4_VDD_IVA_SR_VOLT_REG,
	.vsel_to_uv		= twl6030_vsel_to_uv,
	.uv_to_vsel		= twl6030_uv_to_vsel,
};

static struct omap_volt_pmic_info omap4_core_volt_info = {
	.slew_rate		= 4000,
	.step_size		= 12500,
	.on_volt		= 1100000,
	.onlp_volt		= 1100000,
	.ret_volt		= 837500,
	.off_volt		= 600000,
	.volt_setup_time	= 0,
	.vp_erroroffset		= OMAP4_VP_CONFIG_ERROROFFSET,
	.vp_vstepmin		= OMAP4_VP_VSTEPMIN_VSTEPMIN,
	.vp_vstepmax		= OMAP4_VP_VSTEPMAX_VSTEPMAX,
	.vp_vddmin		= OMAP4_VP_CORE_VLIMITTO_VDDMIN,
	.vp_vddmax		= OMAP4_VP_CORE_VLIMITTO_VDDMAX,
	.vp_timeout_us		= OMAP4_VP_VLIMITTO_TIMEOUT_US,
	.i2c_slave_addr		= OMAP4_SRI2C_SLAVE_ADDR,
	.pmic_reg		= OMAP4_VDD_CORE_SR_VOLT_REG,
	.vsel_to_uv		= twl6030_vsel_to_uv,
	.uv_to_vsel		= twl6030_uv_to_vsel,
};

int __init omap4_twl_init(void)
{
	struct voltagedomain *voltdm;

	if (!cpu_is_omap44xx())
		return -ENODEV;

	voltdm = omap_voltage_domain_lookup("mpu");
	omap_voltage_register_pmic(voltdm, &omap4_mpu_volt_info);

	voltdm = omap_voltage_domain_lookup("iva");
	omap_voltage_register_pmic(voltdm, &omap4_iva_volt_info);

	voltdm = omap_voltage_domain_lookup("core");
	omap_voltage_register_pmic(voltdm, &omap4_core_volt_info);

	return 0;
}

int __init omap3_twl_init(void)
{
	struct voltagedomain *voltdm;

	if (!cpu_is_omap34xx())
		return -ENODEV;

	if (cpu_is_omap3630()) {
		omap3_mpu_volt_info.vp_vddmin = OMAP3630_VP1_VLIMITTO_VDDMIN;
		omap3_mpu_volt_info.vp_vddmax = OMAP3630_VP1_VLIMITTO_VDDMAX;
		omap3_core_volt_info.vp_vddmin = OMAP3630_VP2_VLIMITTO_VDDMIN;
		omap3_core_volt_info.vp_vddmax = OMAP3630_VP2_VLIMITTO_VDDMAX;
	}

	/*
	 * The smartreflex bit on twl4030 specifies if the setting of voltage
	 * is done over the I2C_SR path. Since this setting is independent of
	 * the actual usage of smartreflex AVS module, we enable TWL SR bit
	 * by default irrespective of whether smartreflex AVS module is enabled
	 * on the OMAP side or not. This is because without this bit enabled,
	 * the voltage scaling through vp forceupdate/bypass mechanism of
	 * voltage scaling will not function on TWL over I2C_SR.
	 */
	if (!twl_sr_enable_autoinit)
		omap3_twl_set_sr_bit(true);

	voltdm = omap_voltage_domain_lookup("mpu");
	omap_voltage_register_pmic(voltdm, &omap3_mpu_volt_info);

	voltdm = omap_voltage_domain_lookup("core");
	omap_voltage_register_pmic(voltdm, &omap3_core_volt_info);

	return 0;
}

/**
 * omap3_twl_set_sr_bit() - Set/Clear SR bit on TWL
 * @enable: enable SR mode in twl or not
 *
 * If 'enable' is true, enables Smartreflex bit on TWL 4030 to make sure
 * voltage scaling through OMAP SR works. Else, the smartreflex bit
 * on twl4030 is cleared as there are platforms which use OMAP3 and T2 but
 * use Synchronized Scaling Hardware Strategy (ENABLE_VMODE=1) and Direct
 * Strategy Software Scaling Mode (ENABLE_VMODE=0), for setting the voltages,
 * in those scenarios this bit is to be cleared (enable = false).
 *
 * Returns 0 on success, error is returned if I2C read/write fails.
 */
int __init omap3_twl_set_sr_bit(bool enable)
{
	u8 temp;
	int ret;
	if (twl_sr_enable_autoinit)
		pr_warning("%s: unexpected multiple calls\n", __func__);

	ret = twl_i2c_read_u8(TWL4030_MODULE_PM_RECEIVER, &temp,
					TWL4030_DCDC_GLOBAL_CFG);
	if (ret)
		goto err;

	if (enable)
		temp |= SMARTREFLEX_ENABLE;
	else
		temp &= ~SMARTREFLEX_ENABLE;

	ret = twl_i2c_write_u8(TWL4030_MODULE_PM_RECEIVER, temp,
				TWL4030_DCDC_GLOBAL_CFG);
	if (!ret) {
		twl_sr_enable_autoinit = true;
		return 0;
	}
err:
	pr_err("%s: Error access to TWL4030 (%d)\n", __func__, ret);
	return ret;
}
