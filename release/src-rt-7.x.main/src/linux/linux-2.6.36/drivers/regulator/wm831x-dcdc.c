/*
 * wm831x-dcdc.c  --  DC-DC buck convertor driver for the WM831x series
 *
 * Copyright 2009 Wolfson Microelectronics PLC.
 *
 * Author: Mark Brown <broonie@opensource.wolfsonmicro.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/bitops.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/gpio.h>
#include <linux/slab.h>

#include <linux/mfd/wm831x/core.h>
#include <linux/mfd/wm831x/regulator.h>
#include <linux/mfd/wm831x/pdata.h>

#define WM831X_BUCKV_MAX_SELECTOR 0x68
#define WM831X_BUCKP_MAX_SELECTOR 0x66

#define WM831X_DCDC_MODE_FAST    0
#define WM831X_DCDC_MODE_NORMAL  1
#define WM831X_DCDC_MODE_IDLE    2
#define WM831X_DCDC_MODE_STANDBY 3

#define WM831X_DCDC_MAX_NAME 6

/* Register offsets in control block */
#define WM831X_DCDC_CONTROL_1     0
#define WM831X_DCDC_CONTROL_2     1
#define WM831X_DCDC_ON_CONFIG     2
#define WM831X_DCDC_SLEEP_CONTROL 3
#define WM831X_DCDC_DVS_CONTROL   4

/*
 * Shared
 */

struct wm831x_dcdc {
	char name[WM831X_DCDC_MAX_NAME];
	struct regulator_desc desc;
	int base;
	struct wm831x *wm831x;
	struct regulator_dev *regulator;
	int dvs_gpio;
	int dvs_gpio_state;
	int on_vsel;
	int dvs_vsel;
};

static int wm831x_dcdc_is_enabled(struct regulator_dev *rdev)
{
	struct wm831x_dcdc *dcdc = rdev_get_drvdata(rdev);
	struct wm831x *wm831x = dcdc->wm831x;
	int mask = 1 << rdev_get_id(rdev);
	int reg;

	reg = wm831x_reg_read(wm831x, WM831X_DCDC_ENABLE);
	if (reg < 0)
		return reg;

	if (reg & mask)
		return 1;
	else
		return 0;
}

static int wm831x_dcdc_enable(struct regulator_dev *rdev)
{
	struct wm831x_dcdc *dcdc = rdev_get_drvdata(rdev);
	struct wm831x *wm831x = dcdc->wm831x;
	int mask = 1 << rdev_get_id(rdev);

	return wm831x_set_bits(wm831x, WM831X_DCDC_ENABLE, mask, mask);
}

static int wm831x_dcdc_disable(struct regulator_dev *rdev)
{
	struct wm831x_dcdc *dcdc = rdev_get_drvdata(rdev);
	struct wm831x *wm831x = dcdc->wm831x;
	int mask = 1 << rdev_get_id(rdev);

	return wm831x_set_bits(wm831x, WM831X_DCDC_ENABLE, mask, 0);
}

static unsigned int wm831x_dcdc_get_mode(struct regulator_dev *rdev)

{
	struct wm831x_dcdc *dcdc = rdev_get_drvdata(rdev);
	struct wm831x *wm831x = dcdc->wm831x;
	u16 reg = dcdc->base + WM831X_DCDC_ON_CONFIG;
	int val;

	val = wm831x_reg_read(wm831x, reg);
	if (val < 0)
		return val;

	val = (val & WM831X_DC1_ON_MODE_MASK) >> WM831X_DC1_ON_MODE_SHIFT;

	switch (val) {
	case WM831X_DCDC_MODE_FAST:
		return REGULATOR_MODE_FAST;
	case WM831X_DCDC_MODE_NORMAL:
		return REGULATOR_MODE_NORMAL;
	case WM831X_DCDC_MODE_STANDBY:
		return REGULATOR_MODE_STANDBY;
	case WM831X_DCDC_MODE_IDLE:
		return REGULATOR_MODE_IDLE;
	default:
		BUG();
	}
}

static int wm831x_dcdc_set_mode_int(struct wm831x *wm831x, int reg,
				    unsigned int mode)
{
	int val;

	switch (mode) {
	case REGULATOR_MODE_FAST:
		val = WM831X_DCDC_MODE_FAST;
		break;
	case REGULATOR_MODE_NORMAL:
		val = WM831X_DCDC_MODE_NORMAL;
		break;
	case REGULATOR_MODE_STANDBY:
		val = WM831X_DCDC_MODE_STANDBY;
		break;
	case REGULATOR_MODE_IDLE:
		val = WM831X_DCDC_MODE_IDLE;
		break;
	default:
		return -EINVAL;
	}

	return wm831x_set_bits(wm831x, reg, WM831X_DC1_ON_MODE_MASK,
			       val << WM831X_DC1_ON_MODE_SHIFT);
}

static int wm831x_dcdc_set_mode(struct regulator_dev *rdev, unsigned int mode)
{
	struct wm831x_dcdc *dcdc = rdev_get_drvdata(rdev);
	struct wm831x *wm831x = dcdc->wm831x;
	u16 reg = dcdc->base + WM831X_DCDC_ON_CONFIG;

	return wm831x_dcdc_set_mode_int(wm831x, reg, mode);
}

static int wm831x_dcdc_set_suspend_mode(struct regulator_dev *rdev,
					unsigned int mode)
{
	struct wm831x_dcdc *dcdc = rdev_get_drvdata(rdev);
	struct wm831x *wm831x = dcdc->wm831x;
	u16 reg = dcdc->base + WM831X_DCDC_SLEEP_CONTROL;

	return wm831x_dcdc_set_mode_int(wm831x, reg, mode);
}

static int wm831x_dcdc_get_status(struct regulator_dev *rdev)
{
	struct wm831x_dcdc *dcdc = rdev_get_drvdata(rdev);
	struct wm831x *wm831x = dcdc->wm831x;
	int ret;

	/* First, check for errors */
	ret = wm831x_reg_read(wm831x, WM831X_DCDC_UV_STATUS);
	if (ret < 0)
		return ret;

	if (ret & (1 << rdev_get_id(rdev))) {
		dev_dbg(wm831x->dev, "DCDC%d under voltage\n",
			rdev_get_id(rdev) + 1);
		return REGULATOR_STATUS_ERROR;
	}

	/* DCDC1 and DCDC2 can additionally detect high voltage/current */
	if (rdev_get_id(rdev) < 2) {
		if (ret & (WM831X_DC1_OV_STS << rdev_get_id(rdev))) {
			dev_dbg(wm831x->dev, "DCDC%d over voltage\n",
				rdev_get_id(rdev) + 1);
			return REGULATOR_STATUS_ERROR;
		}

		if (ret & (WM831X_DC1_HC_STS << rdev_get_id(rdev))) {
			dev_dbg(wm831x->dev, "DCDC%d over current\n",
				rdev_get_id(rdev) + 1);
			return REGULATOR_STATUS_ERROR;
		}
	}

	/* Is the regulator on? */
	ret = wm831x_reg_read(wm831x, WM831X_DCDC_STATUS);
	if (ret < 0)
		return ret;
	if (!(ret & (1 << rdev_get_id(rdev))))
		return REGULATOR_STATUS_OFF;

	/* TODO: When we handle hardware control modes so we can report the
	 * current mode. */
	return REGULATOR_STATUS_ON;
}

static irqreturn_t wm831x_dcdc_uv_irq(int irq, void *data)
{
	struct wm831x_dcdc *dcdc = data;

	regulator_notifier_call_chain(dcdc->regulator,
				      REGULATOR_EVENT_UNDER_VOLTAGE,
				      NULL);

	return IRQ_HANDLED;
}

static irqreturn_t wm831x_dcdc_oc_irq(int irq, void *data)
{
	struct wm831x_dcdc *dcdc = data;

	regulator_notifier_call_chain(dcdc->regulator,
				      REGULATOR_EVENT_OVER_CURRENT,
				      NULL);

	return IRQ_HANDLED;
}

/*
 * BUCKV specifics
 */

static int wm831x_buckv_list_voltage(struct regulator_dev *rdev,
				      unsigned selector)
{
	if (selector <= 0x8)
		return 600000;
	if (selector <= WM831X_BUCKV_MAX_SELECTOR)
		return 600000 + ((selector - 0x8) * 12500);
	return -EINVAL;
}

static int wm831x_buckv_select_min_voltage(struct regulator_dev *rdev,
					   int min_uV, int max_uV)
{
	u16 vsel;

	if (min_uV < 600000)
		vsel = 0;
	else if (min_uV <= 1800000)
		vsel = ((min_uV - 600000) / 12500) + 8;
	else
		return -EINVAL;

	if (wm831x_buckv_list_voltage(rdev, vsel) > max_uV)
		return -EINVAL;

	return vsel;
}

static int wm831x_buckv_select_max_voltage(struct regulator_dev *rdev,
					   int min_uV, int max_uV)
{
	u16 vsel;

	if (max_uV < 600000 || max_uV > 1800000)
		return -EINVAL;

	vsel = ((max_uV - 600000) / 12500) + 8;

	if (wm831x_buckv_list_voltage(rdev, vsel) < min_uV ||
	    wm831x_buckv_list_voltage(rdev, vsel) < max_uV)
		return -EINVAL;

	return vsel;
}

static int wm831x_buckv_set_dvs(struct regulator_dev *rdev, int state)
{
	struct wm831x_dcdc *dcdc = rdev_get_drvdata(rdev);

	if (state == dcdc->dvs_gpio_state)
		return 0;

	dcdc->dvs_gpio_state = state;
	gpio_set_value(dcdc->dvs_gpio, state);

	/* Should wait for DVS state change to be asserted if we have
	 * a GPIO for it, for now assume the device is configured
	 * for the fastest possible transition.
	 */

	return 0;
}

static int wm831x_buckv_set_voltage(struct regulator_dev *rdev,
				    int min_uV, int max_uV)
{
	struct wm831x_dcdc *dcdc = rdev_get_drvdata(rdev);
	struct wm831x *wm831x = dcdc->wm831x;
	int on_reg = dcdc->base + WM831X_DCDC_ON_CONFIG;
	int dvs_reg = dcdc->base + WM831X_DCDC_DVS_CONTROL;
	int vsel, ret;

	vsel = wm831x_buckv_select_min_voltage(rdev, min_uV, max_uV);
	if (vsel < 0)
		return vsel;

	/* If this value is already set then do a GPIO update if we can */
	if (dcdc->dvs_gpio && dcdc->on_vsel == vsel)
		return wm831x_buckv_set_dvs(rdev, 0);

	if (dcdc->dvs_gpio && dcdc->dvs_vsel == vsel)
		return wm831x_buckv_set_dvs(rdev, 1);

	/* Always set the ON status to the minimum voltage */
	ret = wm831x_set_bits(wm831x, on_reg, WM831X_DC1_ON_VSEL_MASK, vsel);
	if (ret < 0)
		return ret;
	dcdc->on_vsel = vsel;

	if (!dcdc->dvs_gpio)
		return ret;

	/* Kick the voltage transition now */
	ret = wm831x_buckv_set_dvs(rdev, 0);
	if (ret < 0)
		return ret;

	/* Set the high voltage as the DVS voltage.  This is optimised
	 * for CPUfreq usage, most processors will keep the maximum
	 * voltage constant and lower the minimum with the frequency. */
	vsel = wm831x_buckv_select_max_voltage(rdev, min_uV, max_uV);
	if (vsel < 0) {
		/* This should never happen - at worst the same vsel
		 * should be chosen */
		WARN_ON(vsel < 0);
		return 0;
	}

	/* Don't bother if it's the same VSEL we're already using */
	if (vsel == dcdc->on_vsel)
		return 0;

	ret = wm831x_set_bits(wm831x, dvs_reg, WM831X_DC1_DVS_VSEL_MASK, vsel);
	if (ret == 0)
		dcdc->dvs_vsel = vsel;
	else
		dev_warn(wm831x->dev, "Failed to set DCDC DVS VSEL: %d\n",
			 ret);

	return 0;
}

static int wm831x_buckv_set_suspend_voltage(struct regulator_dev *rdev,
					    int uV)
{
	struct wm831x_dcdc *dcdc = rdev_get_drvdata(rdev);
	struct wm831x *wm831x = dcdc->wm831x;
	u16 reg = dcdc->base + WM831X_DCDC_SLEEP_CONTROL;
	int vsel;

	vsel = wm831x_buckv_select_min_voltage(rdev, uV, uV);
	if (vsel < 0)
		return vsel;

	return wm831x_set_bits(wm831x, reg, WM831X_DC1_SLP_VSEL_MASK, vsel);
}

static int wm831x_buckv_get_voltage(struct regulator_dev *rdev)
{
	struct wm831x_dcdc *dcdc = rdev_get_drvdata(rdev);

	if (dcdc->dvs_gpio && dcdc->dvs_gpio_state)
		return wm831x_buckv_list_voltage(rdev, dcdc->dvs_vsel);
	else
		return wm831x_buckv_list_voltage(rdev, dcdc->on_vsel);
}

/* Current limit options */
static u16 wm831x_dcdc_ilim[] = {
	125, 250, 375, 500, 625, 750, 875, 1000
};

static int wm831x_buckv_set_current_limit(struct regulator_dev *rdev,
					   int min_uA, int max_uA)
{
	struct wm831x_dcdc *dcdc = rdev_get_drvdata(rdev);
	struct wm831x *wm831x = dcdc->wm831x;
	u16 reg = dcdc->base + WM831X_DCDC_CONTROL_2;
	int i;

	for (i = 0; i < ARRAY_SIZE(wm831x_dcdc_ilim); i++) {
		if (max_uA <= wm831x_dcdc_ilim[i])
			break;
	}
	if (i == ARRAY_SIZE(wm831x_dcdc_ilim))
		return -EINVAL;

	return wm831x_set_bits(wm831x, reg, WM831X_DC1_HC_THR_MASK, i);
}

static int wm831x_buckv_get_current_limit(struct regulator_dev *rdev)
{
	struct wm831x_dcdc *dcdc = rdev_get_drvdata(rdev);
	struct wm831x *wm831x = dcdc->wm831x;
	u16 reg = dcdc->base + WM831X_DCDC_CONTROL_2;
	int val;

	val = wm831x_reg_read(wm831x, reg);
	if (val < 0)
		return val;

	return wm831x_dcdc_ilim[val & WM831X_DC1_HC_THR_MASK];
}

static struct regulator_ops wm831x_buckv_ops = {
	.set_voltage = wm831x_buckv_set_voltage,
	.get_voltage = wm831x_buckv_get_voltage,
	.list_voltage = wm831x_buckv_list_voltage,
	.set_suspend_voltage = wm831x_buckv_set_suspend_voltage,
	.set_current_limit = wm831x_buckv_set_current_limit,
	.get_current_limit = wm831x_buckv_get_current_limit,

	.is_enabled = wm831x_dcdc_is_enabled,
	.enable = wm831x_dcdc_enable,
	.disable = wm831x_dcdc_disable,
	.get_status = wm831x_dcdc_get_status,
	.get_mode = wm831x_dcdc_get_mode,
	.set_mode = wm831x_dcdc_set_mode,
	.set_suspend_mode = wm831x_dcdc_set_suspend_mode,
};

/*
 * Set up DVS control.  We just log errors since we can still run
 * (with reduced performance) if we fail.
 */
static __devinit void wm831x_buckv_dvs_init(struct wm831x_dcdc *dcdc,
					    struct wm831x_buckv_pdata *pdata)
{
	struct wm831x *wm831x = dcdc->wm831x;
	int ret;
	u16 ctrl;

	if (!pdata || !pdata->dvs_gpio)
		return;

	switch (pdata->dvs_control_src) {
	case 1:
		ctrl = 2 << WM831X_DC1_DVS_SRC_SHIFT;
		break;
	case 2:
		ctrl = 3 << WM831X_DC1_DVS_SRC_SHIFT;
		break;
	default:
		dev_err(wm831x->dev, "Invalid DVS control source %d for %s\n",
			pdata->dvs_control_src, dcdc->name);
		return;
	}

	ret = wm831x_set_bits(wm831x, dcdc->base + WM831X_DCDC_DVS_CONTROL,
			      WM831X_DC1_DVS_SRC_MASK, ctrl);
	if (ret < 0) {
		dev_err(wm831x->dev, "Failed to set %s DVS source: %d\n",
			dcdc->name, ret);
		return;
	}

	ret = gpio_request(pdata->dvs_gpio, "DCDC DVS");
	if (ret < 0) {
		dev_err(wm831x->dev, "Failed to get %s DVS GPIO: %d\n",
			dcdc->name, ret);
		return;
	}

	/* gpiolib won't let us read the GPIO status so pick the higher
	 * of the two existing voltages so we take it as platform data.
	 */
	dcdc->dvs_gpio_state = pdata->dvs_init_state;

	ret = gpio_direction_output(pdata->dvs_gpio, dcdc->dvs_gpio_state);
	if (ret < 0) {
		dev_err(wm831x->dev, "Failed to enable %s DVS GPIO: %d\n",
			dcdc->name, ret);
		gpio_free(pdata->dvs_gpio);
		return;
	}

	dcdc->dvs_gpio = pdata->dvs_gpio;
}

static __devinit int wm831x_buckv_probe(struct platform_device *pdev)
{
	struct wm831x *wm831x = dev_get_drvdata(pdev->dev.parent);
	struct wm831x_pdata *pdata = wm831x->dev->platform_data;
	int id = pdev->id % ARRAY_SIZE(pdata->dcdc);
	struct wm831x_dcdc *dcdc;
	struct resource *res;
	int ret, irq;

	dev_dbg(&pdev->dev, "Probing DCDC%d\n", id + 1);

	if (pdata == NULL || pdata->dcdc[id] == NULL)
		return -ENODEV;

	dcdc = kzalloc(sizeof(struct wm831x_dcdc), GFP_KERNEL);
	if (dcdc == NULL) {
		dev_err(&pdev->dev, "Unable to allocate private data\n");
		return -ENOMEM;
	}

	dcdc->wm831x = wm831x;

	res = platform_get_resource(pdev, IORESOURCE_IO, 0);
	if (res == NULL) {
		dev_err(&pdev->dev, "No I/O resource\n");
		ret = -EINVAL;
		goto err;
	}
	dcdc->base = res->start;

	snprintf(dcdc->name, sizeof(dcdc->name), "DCDC%d", id + 1);
	dcdc->desc.name = dcdc->name;
	dcdc->desc.id = id;
	dcdc->desc.type = REGULATOR_VOLTAGE;
	dcdc->desc.n_voltages = WM831X_BUCKV_MAX_SELECTOR + 1;
	dcdc->desc.ops = &wm831x_buckv_ops;
	dcdc->desc.owner = THIS_MODULE;

	ret = wm831x_reg_read(wm831x, dcdc->base + WM831X_DCDC_ON_CONFIG);
	if (ret < 0) {
		dev_err(wm831x->dev, "Failed to read ON VSEL: %d\n", ret);
		goto err;
	}
	dcdc->on_vsel = ret & WM831X_DC1_ON_VSEL_MASK;

	ret = wm831x_reg_read(wm831x, dcdc->base + WM831X_DCDC_ON_CONFIG);
	if (ret < 0) {
		dev_err(wm831x->dev, "Failed to read DVS VSEL: %d\n", ret);
		goto err;
	}
	dcdc->dvs_vsel = ret & WM831X_DC1_DVS_VSEL_MASK;

	if (pdata->dcdc[id])
		wm831x_buckv_dvs_init(dcdc, pdata->dcdc[id]->driver_data);

	dcdc->regulator = regulator_register(&dcdc->desc, &pdev->dev,
					     pdata->dcdc[id], dcdc);
	if (IS_ERR(dcdc->regulator)) {
		ret = PTR_ERR(dcdc->regulator);
		dev_err(wm831x->dev, "Failed to register DCDC%d: %d\n",
			id + 1, ret);
		goto err;
	}

	irq = platform_get_irq_byname(pdev, "UV");
	ret = wm831x_request_irq(wm831x, irq, wm831x_dcdc_uv_irq,
				 IRQF_TRIGGER_RISING, dcdc->name,
				 dcdc);
	if (ret != 0) {
		dev_err(&pdev->dev, "Failed to request UV IRQ %d: %d\n",
			irq, ret);
		goto err_regulator;
	}

	irq = platform_get_irq_byname(pdev, "HC");
	ret = wm831x_request_irq(wm831x, irq, wm831x_dcdc_oc_irq,
				 IRQF_TRIGGER_RISING, dcdc->name,
				 dcdc);
	if (ret != 0) {
		dev_err(&pdev->dev, "Failed to request HC IRQ %d: %d\n",
			irq, ret);
		goto err_uv;
	}

	platform_set_drvdata(pdev, dcdc);

	return 0;

err_uv:
	wm831x_free_irq(wm831x, platform_get_irq_byname(pdev, "UV"), dcdc);
err_regulator:
	regulator_unregister(dcdc->regulator);
err:
	if (dcdc->dvs_gpio)
		gpio_free(dcdc->dvs_gpio);
	kfree(dcdc);
	return ret;
}

static __devexit int wm831x_buckv_remove(struct platform_device *pdev)
{
	struct wm831x_dcdc *dcdc = platform_get_drvdata(pdev);
	struct wm831x *wm831x = dcdc->wm831x;

	platform_set_drvdata(pdev, NULL);

	wm831x_free_irq(wm831x, platform_get_irq_byname(pdev, "HC"), dcdc);
	wm831x_free_irq(wm831x, platform_get_irq_byname(pdev, "UV"), dcdc);
	regulator_unregister(dcdc->regulator);
	if (dcdc->dvs_gpio)
		gpio_free(dcdc->dvs_gpio);
	kfree(dcdc);

	return 0;
}

static struct platform_driver wm831x_buckv_driver = {
	.probe = wm831x_buckv_probe,
	.remove = __devexit_p(wm831x_buckv_remove),
	.driver		= {
		.name	= "wm831x-buckv",
		.owner	= THIS_MODULE,
	},
};

/*
 * BUCKP specifics
 */

static int wm831x_buckp_list_voltage(struct regulator_dev *rdev,
				      unsigned selector)
{
	if (selector <= WM831X_BUCKP_MAX_SELECTOR)
		return 850000 + (selector * 25000);
	else
		return -EINVAL;
}

static int wm831x_buckp_set_voltage_int(struct regulator_dev *rdev, int reg,
					int min_uV, int max_uV)
{
	struct wm831x_dcdc *dcdc = rdev_get_drvdata(rdev);
	struct wm831x *wm831x = dcdc->wm831x;
	u16 vsel;

	if (min_uV <= 34000000)
		vsel = (min_uV - 850000) / 25000;
	else
		return -EINVAL;

	if (wm831x_buckp_list_voltage(rdev, vsel) > max_uV)
		return -EINVAL;

	return wm831x_set_bits(wm831x, reg, WM831X_DC3_ON_VSEL_MASK, vsel);
}

static int wm831x_buckp_set_voltage(struct regulator_dev *rdev,
				    int min_uV, int max_uV)
{
	struct wm831x_dcdc *dcdc = rdev_get_drvdata(rdev);
	u16 reg = dcdc->base + WM831X_DCDC_ON_CONFIG;

	return wm831x_buckp_set_voltage_int(rdev, reg, min_uV, max_uV);
}

static int wm831x_buckp_set_suspend_voltage(struct regulator_dev *rdev,
					    int uV)
{
	struct wm831x_dcdc *dcdc = rdev_get_drvdata(rdev);
	u16 reg = dcdc->base + WM831X_DCDC_SLEEP_CONTROL;

	return wm831x_buckp_set_voltage_int(rdev, reg, uV, uV);
}

static int wm831x_buckp_get_voltage(struct regulator_dev *rdev)
{
	struct wm831x_dcdc *dcdc = rdev_get_drvdata(rdev);
	struct wm831x *wm831x = dcdc->wm831x;
	u16 reg = dcdc->base + WM831X_DCDC_ON_CONFIG;
	int val;

	val = wm831x_reg_read(wm831x, reg);
	if (val < 0)
		return val;

	return wm831x_buckp_list_voltage(rdev, val & WM831X_DC3_ON_VSEL_MASK);
}

static struct regulator_ops wm831x_buckp_ops = {
	.set_voltage = wm831x_buckp_set_voltage,
	.get_voltage = wm831x_buckp_get_voltage,
	.list_voltage = wm831x_buckp_list_voltage,
	.set_suspend_voltage = wm831x_buckp_set_suspend_voltage,

	.is_enabled = wm831x_dcdc_is_enabled,
	.enable = wm831x_dcdc_enable,
	.disable = wm831x_dcdc_disable,
	.get_status = wm831x_dcdc_get_status,
	.get_mode = wm831x_dcdc_get_mode,
	.set_mode = wm831x_dcdc_set_mode,
	.set_suspend_mode = wm831x_dcdc_set_suspend_mode,
};

static __devinit int wm831x_buckp_probe(struct platform_device *pdev)
{
	struct wm831x *wm831x = dev_get_drvdata(pdev->dev.parent);
	struct wm831x_pdata *pdata = wm831x->dev->platform_data;
	int id = pdev->id % ARRAY_SIZE(pdata->dcdc);
	struct wm831x_dcdc *dcdc;
	struct resource *res;
	int ret, irq;

	dev_dbg(&pdev->dev, "Probing DCDC%d\n", id + 1);

	if (pdata == NULL || pdata->dcdc[id] == NULL)
		return -ENODEV;

	dcdc = kzalloc(sizeof(struct wm831x_dcdc), GFP_KERNEL);
	if (dcdc == NULL) {
		dev_err(&pdev->dev, "Unable to allocate private data\n");
		return -ENOMEM;
	}

	dcdc->wm831x = wm831x;

	res = platform_get_resource(pdev, IORESOURCE_IO, 0);
	if (res == NULL) {
		dev_err(&pdev->dev, "No I/O resource\n");
		ret = -EINVAL;
		goto err;
	}
	dcdc->base = res->start;

	snprintf(dcdc->name, sizeof(dcdc->name), "DCDC%d", id + 1);
	dcdc->desc.name = dcdc->name;
	dcdc->desc.id = id;
	dcdc->desc.type = REGULATOR_VOLTAGE;
	dcdc->desc.n_voltages = WM831X_BUCKP_MAX_SELECTOR + 1;
	dcdc->desc.ops = &wm831x_buckp_ops;
	dcdc->desc.owner = THIS_MODULE;

	dcdc->regulator = regulator_register(&dcdc->desc, &pdev->dev,
					     pdata->dcdc[id], dcdc);
	if (IS_ERR(dcdc->regulator)) {
		ret = PTR_ERR(dcdc->regulator);
		dev_err(wm831x->dev, "Failed to register DCDC%d: %d\n",
			id + 1, ret);
		goto err;
	}

	irq = platform_get_irq_byname(pdev, "UV");
	ret = wm831x_request_irq(wm831x, irq, wm831x_dcdc_uv_irq,
				 IRQF_TRIGGER_RISING, dcdc->name,
				 dcdc);
	if (ret != 0) {
		dev_err(&pdev->dev, "Failed to request UV IRQ %d: %d\n",
			irq, ret);
		goto err_regulator;
	}

	platform_set_drvdata(pdev, dcdc);

	return 0;

err_regulator:
	regulator_unregister(dcdc->regulator);
err:
	kfree(dcdc);
	return ret;
}

static __devexit int wm831x_buckp_remove(struct platform_device *pdev)
{
	struct wm831x_dcdc *dcdc = platform_get_drvdata(pdev);
	struct wm831x *wm831x = dcdc->wm831x;

	platform_set_drvdata(pdev, NULL);

	wm831x_free_irq(wm831x, platform_get_irq_byname(pdev, "UV"), dcdc);
	regulator_unregister(dcdc->regulator);
	kfree(dcdc);

	return 0;
}

static struct platform_driver wm831x_buckp_driver = {
	.probe = wm831x_buckp_probe,
	.remove = __devexit_p(wm831x_buckp_remove),
	.driver		= {
		.name	= "wm831x-buckp",
		.owner	= THIS_MODULE,
	},
};

/*
 * DCDC boost convertors
 */

static int wm831x_boostp_get_status(struct regulator_dev *rdev)
{
	struct wm831x_dcdc *dcdc = rdev_get_drvdata(rdev);
	struct wm831x *wm831x = dcdc->wm831x;
	int ret;

	/* First, check for errors */
	ret = wm831x_reg_read(wm831x, WM831X_DCDC_UV_STATUS);
	if (ret < 0)
		return ret;

	if (ret & (1 << rdev_get_id(rdev))) {
		dev_dbg(wm831x->dev, "DCDC%d under voltage\n",
			rdev_get_id(rdev) + 1);
		return REGULATOR_STATUS_ERROR;
	}

	/* Is the regulator on? */
	ret = wm831x_reg_read(wm831x, WM831X_DCDC_STATUS);
	if (ret < 0)
		return ret;
	if (ret & (1 << rdev_get_id(rdev)))
		return REGULATOR_STATUS_ON;
	else
		return REGULATOR_STATUS_OFF;
}

static struct regulator_ops wm831x_boostp_ops = {
	.get_status = wm831x_boostp_get_status,

	.is_enabled = wm831x_dcdc_is_enabled,
	.enable = wm831x_dcdc_enable,
	.disable = wm831x_dcdc_disable,
};

static __devinit int wm831x_boostp_probe(struct platform_device *pdev)
{
	struct wm831x *wm831x = dev_get_drvdata(pdev->dev.parent);
	struct wm831x_pdata *pdata = wm831x->dev->platform_data;
	int id = pdev->id % ARRAY_SIZE(pdata->dcdc);
	struct wm831x_dcdc *dcdc;
	struct resource *res;
	int ret, irq;

	dev_dbg(&pdev->dev, "Probing DCDC%d\n", id + 1);

	if (pdata == NULL || pdata->dcdc[id] == NULL)
		return -ENODEV;

	dcdc = kzalloc(sizeof(struct wm831x_dcdc), GFP_KERNEL);
	if (dcdc == NULL) {
		dev_err(&pdev->dev, "Unable to allocate private data\n");
		return -ENOMEM;
	}

	dcdc->wm831x = wm831x;

	res = platform_get_resource(pdev, IORESOURCE_IO, 0);
	if (res == NULL) {
		dev_err(&pdev->dev, "No I/O resource\n");
		ret = -EINVAL;
		goto err;
	}
	dcdc->base = res->start;

	snprintf(dcdc->name, sizeof(dcdc->name), "DCDC%d", id + 1);
	dcdc->desc.name = dcdc->name;
	dcdc->desc.id = id;
	dcdc->desc.type = REGULATOR_VOLTAGE;
	dcdc->desc.ops = &wm831x_boostp_ops;
	dcdc->desc.owner = THIS_MODULE;

	dcdc->regulator = regulator_register(&dcdc->desc, &pdev->dev,
					     pdata->dcdc[id], dcdc);
	if (IS_ERR(dcdc->regulator)) {
		ret = PTR_ERR(dcdc->regulator);
		dev_err(wm831x->dev, "Failed to register DCDC%d: %d\n",
			id + 1, ret);
		goto err;
	}

	irq = platform_get_irq_byname(pdev, "UV");
	ret = wm831x_request_irq(wm831x, irq, wm831x_dcdc_uv_irq,
				 IRQF_TRIGGER_RISING, dcdc->name,
				 dcdc);
	if (ret != 0) {
		dev_err(&pdev->dev, "Failed to request UV IRQ %d: %d\n",
			irq, ret);
		goto err_regulator;
	}

	platform_set_drvdata(pdev, dcdc);

	return 0;

err_regulator:
	regulator_unregister(dcdc->regulator);
err:
	kfree(dcdc);
	return ret;
}

static __devexit int wm831x_boostp_remove(struct platform_device *pdev)
{
	struct wm831x_dcdc *dcdc = platform_get_drvdata(pdev);
	struct wm831x *wm831x = dcdc->wm831x;

	platform_set_drvdata(pdev, NULL);

	wm831x_free_irq(wm831x, platform_get_irq_byname(pdev, "UV"), dcdc);
	regulator_unregister(dcdc->regulator);
	kfree(dcdc);

	return 0;
}

static struct platform_driver wm831x_boostp_driver = {
	.probe = wm831x_boostp_probe,
	.remove = __devexit_p(wm831x_boostp_remove),
	.driver		= {
		.name	= "wm831x-boostp",
		.owner	= THIS_MODULE,
	},
};

/*
 * External Power Enable
 *
 * These aren't actually DCDCs but look like them in hardware so share
 * code.
 */

#define WM831X_EPE_BASE 6

static struct regulator_ops wm831x_epe_ops = {
	.is_enabled = wm831x_dcdc_is_enabled,
	.enable = wm831x_dcdc_enable,
	.disable = wm831x_dcdc_disable,
	.get_status = wm831x_dcdc_get_status,
};

static __devinit int wm831x_epe_probe(struct platform_device *pdev)
{
	struct wm831x *wm831x = dev_get_drvdata(pdev->dev.parent);
	struct wm831x_pdata *pdata = wm831x->dev->platform_data;
	int id = pdev->id % ARRAY_SIZE(pdata->epe);
	struct wm831x_dcdc *dcdc;
	int ret;

	dev_dbg(&pdev->dev, "Probing EPE%d\n", id + 1);

	if (pdata == NULL || pdata->epe[id] == NULL)
		return -ENODEV;

	dcdc = kzalloc(sizeof(struct wm831x_dcdc), GFP_KERNEL);
	if (dcdc == NULL) {
		dev_err(&pdev->dev, "Unable to allocate private data\n");
		return -ENOMEM;
	}

	dcdc->wm831x = wm831x;

	/* For current parts this is correct; probably need to revisit
	 * in future.
	 */
	snprintf(dcdc->name, sizeof(dcdc->name), "EPE%d", id + 1);
	dcdc->desc.name = dcdc->name;
	dcdc->desc.id = id + WM831X_EPE_BASE; /* Offset in DCDC registers */
	dcdc->desc.ops = &wm831x_epe_ops;
	dcdc->desc.type = REGULATOR_VOLTAGE;
	dcdc->desc.owner = THIS_MODULE;

	dcdc->regulator = regulator_register(&dcdc->desc, &pdev->dev,
					     pdata->epe[id], dcdc);
	if (IS_ERR(dcdc->regulator)) {
		ret = PTR_ERR(dcdc->regulator);
		dev_err(wm831x->dev, "Failed to register EPE%d: %d\n",
			id + 1, ret);
		goto err;
	}

	platform_set_drvdata(pdev, dcdc);

	return 0;

err:
	kfree(dcdc);
	return ret;
}

static __devexit int wm831x_epe_remove(struct platform_device *pdev)
{
	struct wm831x_dcdc *dcdc = platform_get_drvdata(pdev);

	platform_set_drvdata(pdev, NULL);

	regulator_unregister(dcdc->regulator);
	kfree(dcdc);

	return 0;
}

static struct platform_driver wm831x_epe_driver = {
	.probe = wm831x_epe_probe,
	.remove = __devexit_p(wm831x_epe_remove),
	.driver		= {
		.name	= "wm831x-epe",
		.owner	= THIS_MODULE,
	},
};

static int __init wm831x_dcdc_init(void)
{
	int ret;
	ret = platform_driver_register(&wm831x_buckv_driver);
	if (ret != 0)
		pr_err("Failed to register WM831x BUCKV driver: %d\n", ret);

	ret = platform_driver_register(&wm831x_buckp_driver);
	if (ret != 0)
		pr_err("Failed to register WM831x BUCKP driver: %d\n", ret);

	ret = platform_driver_register(&wm831x_boostp_driver);
	if (ret != 0)
		pr_err("Failed to register WM831x BOOST driver: %d\n", ret);

	ret = platform_driver_register(&wm831x_epe_driver);
	if (ret != 0)
		pr_err("Failed to register WM831x EPE driver: %d\n", ret);

	return 0;
}
subsys_initcall(wm831x_dcdc_init);

static void __exit wm831x_dcdc_exit(void)
{
	platform_driver_unregister(&wm831x_epe_driver);
	platform_driver_unregister(&wm831x_boostp_driver);
	platform_driver_unregister(&wm831x_buckp_driver);
	platform_driver_unregister(&wm831x_buckv_driver);
}
module_exit(wm831x_dcdc_exit);

/* Module information */
MODULE_AUTHOR("Mark Brown");
MODULE_DESCRIPTION("WM831x DC-DC convertor driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:wm831x-buckv");
MODULE_ALIAS("platform:wm831x-buckp");
