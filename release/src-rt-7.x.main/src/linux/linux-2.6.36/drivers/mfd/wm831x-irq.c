/*
 * wm831x-irq.c  --  Interrupt controller support for Wolfson WM831x PMICs
 *
 * Copyright 2009 Wolfson Microelectronics PLC.
 *
 * Author: Mark Brown <broonie@opensource.wolfsonmicro.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/mfd/core.h>
#include <linux/interrupt.h>

#include <linux/mfd/wm831x/core.h>
#include <linux/mfd/wm831x/pdata.h>
#include <linux/mfd/wm831x/gpio.h>
#include <linux/mfd/wm831x/irq.h>

#include <linux/delay.h>

/*
 * Since generic IRQs don't currently support interrupt controllers on
 * interrupt driven buses we don't use genirq but instead provide an
 * interface that looks very much like the standard ones.  This leads
 * to some bodges, including storing interrupt handler information in
 * the static irq_data table we use to look up the data for individual
 * interrupts, but hopefully won't last too long.
 */

struct wm831x_irq_data {
	int primary;
	int reg;
	int mask;
};

static struct wm831x_irq_data wm831x_irqs[] = {
	[WM831X_IRQ_TEMP_THW] = {
		.primary = WM831X_TEMP_INT,
		.reg = 1,
		.mask = WM831X_TEMP_THW_EINT,
	},
	[WM831X_IRQ_GPIO_1] = {
		.primary = WM831X_GP_INT,
		.reg = 5,
		.mask = WM831X_GP1_EINT,
	},
	[WM831X_IRQ_GPIO_2] = {
		.primary = WM831X_GP_INT,
		.reg = 5,
		.mask = WM831X_GP2_EINT,
	},
	[WM831X_IRQ_GPIO_3] = {
		.primary = WM831X_GP_INT,
		.reg = 5,
		.mask = WM831X_GP3_EINT,
	},
	[WM831X_IRQ_GPIO_4] = {
		.primary = WM831X_GP_INT,
		.reg = 5,
		.mask = WM831X_GP4_EINT,
	},
	[WM831X_IRQ_GPIO_5] = {
		.primary = WM831X_GP_INT,
		.reg = 5,
		.mask = WM831X_GP5_EINT,
	},
	[WM831X_IRQ_GPIO_6] = {
		.primary = WM831X_GP_INT,
		.reg = 5,
		.mask = WM831X_GP6_EINT,
	},
	[WM831X_IRQ_GPIO_7] = {
		.primary = WM831X_GP_INT,
		.reg = 5,
		.mask = WM831X_GP7_EINT,
	},
	[WM831X_IRQ_GPIO_8] = {
		.primary = WM831X_GP_INT,
		.reg = 5,
		.mask = WM831X_GP8_EINT,
	},
	[WM831X_IRQ_GPIO_9] = {
		.primary = WM831X_GP_INT,
		.reg = 5,
		.mask = WM831X_GP9_EINT,
	},
	[WM831X_IRQ_GPIO_10] = {
		.primary = WM831X_GP_INT,
		.reg = 5,
		.mask = WM831X_GP10_EINT,
	},
	[WM831X_IRQ_GPIO_11] = {
		.primary = WM831X_GP_INT,
		.reg = 5,
		.mask = WM831X_GP11_EINT,
	},
	[WM831X_IRQ_GPIO_12] = {
		.primary = WM831X_GP_INT,
		.reg = 5,
		.mask = WM831X_GP12_EINT,
	},
	[WM831X_IRQ_GPIO_13] = {
		.primary = WM831X_GP_INT,
		.reg = 5,
		.mask = WM831X_GP13_EINT,
	},
	[WM831X_IRQ_GPIO_14] = {
		.primary = WM831X_GP_INT,
		.reg = 5,
		.mask = WM831X_GP14_EINT,
	},
	[WM831X_IRQ_GPIO_15] = {
		.primary = WM831X_GP_INT,
		.reg = 5,
		.mask = WM831X_GP15_EINT,
	},
	[WM831X_IRQ_GPIO_16] = {
		.primary = WM831X_GP_INT,
		.reg = 5,
		.mask = WM831X_GP16_EINT,
	},
	[WM831X_IRQ_ON] = {
		.primary = WM831X_ON_PIN_INT,
		.reg = 1,
		.mask = WM831X_ON_PIN_EINT,
	},
	[WM831X_IRQ_PPM_SYSLO] = {
		.primary = WM831X_PPM_INT,
		.reg = 1,
		.mask = WM831X_PPM_SYSLO_EINT,
	},
	[WM831X_IRQ_PPM_PWR_SRC] = {
		.primary = WM831X_PPM_INT,
		.reg = 1,
		.mask = WM831X_PPM_PWR_SRC_EINT,
	},
	[WM831X_IRQ_PPM_USB_CURR] = {
		.primary = WM831X_PPM_INT,
		.reg = 1,
		.mask = WM831X_PPM_USB_CURR_EINT,
	},
	[WM831X_IRQ_WDOG_TO] = {
		.primary = WM831X_WDOG_INT,
		.reg = 1,
		.mask = WM831X_WDOG_TO_EINT,
	},
	[WM831X_IRQ_RTC_PER] = {
		.primary = WM831X_RTC_INT,
		.reg = 1,
		.mask = WM831X_RTC_PER_EINT,
	},
	[WM831X_IRQ_RTC_ALM] = {
		.primary = WM831X_RTC_INT,
		.reg = 1,
		.mask = WM831X_RTC_ALM_EINT,
	},
	[WM831X_IRQ_CHG_BATT_HOT] = {
		.primary = WM831X_CHG_INT,
		.reg = 2,
		.mask = WM831X_CHG_BATT_HOT_EINT,
	},
	[WM831X_IRQ_CHG_BATT_COLD] = {
		.primary = WM831X_CHG_INT,
		.reg = 2,
		.mask = WM831X_CHG_BATT_COLD_EINT,
	},
	[WM831X_IRQ_CHG_BATT_FAIL] = {
		.primary = WM831X_CHG_INT,
		.reg = 2,
		.mask = WM831X_CHG_BATT_FAIL_EINT,
	},
	[WM831X_IRQ_CHG_OV] = {
		.primary = WM831X_CHG_INT,
		.reg = 2,
		.mask = WM831X_CHG_OV_EINT,
	},
	[WM831X_IRQ_CHG_END] = {
		.primary = WM831X_CHG_INT,
		.reg = 2,
		.mask = WM831X_CHG_END_EINT,
	},
	[WM831X_IRQ_CHG_TO] = {
		.primary = WM831X_CHG_INT,
		.reg = 2,
		.mask = WM831X_CHG_TO_EINT,
	},
	[WM831X_IRQ_CHG_MODE] = {
		.primary = WM831X_CHG_INT,
		.reg = 2,
		.mask = WM831X_CHG_MODE_EINT,
	},
	[WM831X_IRQ_CHG_START] = {
		.primary = WM831X_CHG_INT,
		.reg = 2,
		.mask = WM831X_CHG_START_EINT,
	},
	[WM831X_IRQ_TCHDATA] = {
		.primary = WM831X_TCHDATA_INT,
		.reg = 1,
		.mask = WM831X_TCHDATA_EINT,
	},
	[WM831X_IRQ_TCHPD] = {
		.primary = WM831X_TCHPD_INT,
		.reg = 1,
		.mask = WM831X_TCHPD_EINT,
	},
	[WM831X_IRQ_AUXADC_DATA] = {
		.primary = WM831X_AUXADC_INT,
		.reg = 1,
		.mask = WM831X_AUXADC_DATA_EINT,
	},
	[WM831X_IRQ_AUXADC_DCOMP1] = {
		.primary = WM831X_AUXADC_INT,
		.reg = 1,
		.mask = WM831X_AUXADC_DCOMP1_EINT,
	},
	[WM831X_IRQ_AUXADC_DCOMP2] = {
		.primary = WM831X_AUXADC_INT,
		.reg = 1,
		.mask = WM831X_AUXADC_DCOMP2_EINT,
	},
	[WM831X_IRQ_AUXADC_DCOMP3] = {
		.primary = WM831X_AUXADC_INT,
		.reg = 1,
		.mask = WM831X_AUXADC_DCOMP3_EINT,
	},
	[WM831X_IRQ_AUXADC_DCOMP4] = {
		.primary = WM831X_AUXADC_INT,
		.reg = 1,
		.mask = WM831X_AUXADC_DCOMP4_EINT,
	},
	[WM831X_IRQ_CS1] = {
		.primary = WM831X_CS_INT,
		.reg = 2,
		.mask = WM831X_CS1_EINT,
	},
	[WM831X_IRQ_CS2] = {
		.primary = WM831X_CS_INT,
		.reg = 2,
		.mask = WM831X_CS2_EINT,
	},
	[WM831X_IRQ_HC_DC1] = {
		.primary = WM831X_HC_INT,
		.reg = 4,
		.mask = WM831X_HC_DC1_EINT,
	},
	[WM831X_IRQ_HC_DC2] = {
		.primary = WM831X_HC_INT,
		.reg = 4,
		.mask = WM831X_HC_DC2_EINT,
	},
	[WM831X_IRQ_UV_LDO1] = {
		.primary = WM831X_UV_INT,
		.reg = 3,
		.mask = WM831X_UV_LDO1_EINT,
	},
	[WM831X_IRQ_UV_LDO2] = {
		.primary = WM831X_UV_INT,
		.reg = 3,
		.mask = WM831X_UV_LDO2_EINT,
	},
	[WM831X_IRQ_UV_LDO3] = {
		.primary = WM831X_UV_INT,
		.reg = 3,
		.mask = WM831X_UV_LDO3_EINT,
	},
	[WM831X_IRQ_UV_LDO4] = {
		.primary = WM831X_UV_INT,
		.reg = 3,
		.mask = WM831X_UV_LDO4_EINT,
	},
	[WM831X_IRQ_UV_LDO5] = {
		.primary = WM831X_UV_INT,
		.reg = 3,
		.mask = WM831X_UV_LDO5_EINT,
	},
	[WM831X_IRQ_UV_LDO6] = {
		.primary = WM831X_UV_INT,
		.reg = 3,
		.mask = WM831X_UV_LDO6_EINT,
	},
	[WM831X_IRQ_UV_LDO7] = {
		.primary = WM831X_UV_INT,
		.reg = 3,
		.mask = WM831X_UV_LDO7_EINT,
	},
	[WM831X_IRQ_UV_LDO8] = {
		.primary = WM831X_UV_INT,
		.reg = 3,
		.mask = WM831X_UV_LDO8_EINT,
	},
	[WM831X_IRQ_UV_LDO9] = {
		.primary = WM831X_UV_INT,
		.reg = 3,
		.mask = WM831X_UV_LDO9_EINT,
	},
	[WM831X_IRQ_UV_LDO10] = {
		.primary = WM831X_UV_INT,
		.reg = 3,
		.mask = WM831X_UV_LDO10_EINT,
	},
	[WM831X_IRQ_UV_DC1] = {
		.primary = WM831X_UV_INT,
		.reg = 4,
		.mask = WM831X_UV_DC1_EINT,
	},
	[WM831X_IRQ_UV_DC2] = {
		.primary = WM831X_UV_INT,
		.reg = 4,
		.mask = WM831X_UV_DC2_EINT,
	},
	[WM831X_IRQ_UV_DC3] = {
		.primary = WM831X_UV_INT,
		.reg = 4,
		.mask = WM831X_UV_DC3_EINT,
	},
	[WM831X_IRQ_UV_DC4] = {
		.primary = WM831X_UV_INT,
		.reg = 4,
		.mask = WM831X_UV_DC4_EINT,
	},
};

static inline int irq_data_to_status_reg(struct wm831x_irq_data *irq_data)
{
	return WM831X_INTERRUPT_STATUS_1 - 1 + irq_data->reg;
}

static inline int irq_data_to_mask_reg(struct wm831x_irq_data *irq_data)
{
	return WM831X_INTERRUPT_STATUS_1_MASK - 1 + irq_data->reg;
}

static inline struct wm831x_irq_data *irq_to_wm831x_irq(struct wm831x *wm831x,
							int irq)
{
	return &wm831x_irqs[irq - wm831x->irq_base];
}

static void wm831x_irq_lock(unsigned int irq)
{
	struct wm831x *wm831x = get_irq_chip_data(irq);

	mutex_lock(&wm831x->irq_lock);
}

static void wm831x_irq_sync_unlock(unsigned int irq)
{
	struct wm831x *wm831x = get_irq_chip_data(irq);
	int i;

	for (i = 0; i < ARRAY_SIZE(wm831x->irq_masks_cur); i++) {
		/* If there's been a change in the mask write it back
		 * to the hardware. */
		if (wm831x->irq_masks_cur[i] != wm831x->irq_masks_cache[i]) {
			wm831x->irq_masks_cache[i] = wm831x->irq_masks_cur[i];
			wm831x_reg_write(wm831x,
					 WM831X_INTERRUPT_STATUS_1_MASK + i,
					 wm831x->irq_masks_cur[i]);
		}
	}

	mutex_unlock(&wm831x->irq_lock);
}

static void wm831x_irq_unmask(unsigned int irq)
{
	struct wm831x *wm831x = get_irq_chip_data(irq);
	struct wm831x_irq_data *irq_data = irq_to_wm831x_irq(wm831x, irq);

	wm831x->irq_masks_cur[irq_data->reg - 1] &= ~irq_data->mask;
}

static void wm831x_irq_mask(unsigned int irq)
{
	struct wm831x *wm831x = get_irq_chip_data(irq);
	struct wm831x_irq_data *irq_data = irq_to_wm831x_irq(wm831x, irq);

	wm831x->irq_masks_cur[irq_data->reg - 1] |= irq_data->mask;
}

static int wm831x_irq_set_type(unsigned int irq, unsigned int type)
{
	struct wm831x *wm831x = get_irq_chip_data(irq);
	int val;

	irq = irq - wm831x->irq_base;

	if (irq < WM831X_IRQ_GPIO_1 || irq > WM831X_IRQ_GPIO_11) {
		/* Ignore internal-only IRQs */
		if (irq >= 0 && irq < WM831X_NUM_IRQS)
			return 0;
		else
			return -EINVAL;
	}

	switch (type) {
	case IRQ_TYPE_EDGE_BOTH:
		val = WM831X_GPN_INT_MODE;
		break;
	case IRQ_TYPE_EDGE_RISING:
		val = WM831X_GPN_POL;
		break;
	case IRQ_TYPE_EDGE_FALLING:
		val = 0;
		break;
	default:
		return -EINVAL;
	}

	return wm831x_set_bits(wm831x, WM831X_GPIO1_CONTROL + irq,
			       WM831X_GPN_INT_MODE | WM831X_GPN_POL, val);
}

static struct irq_chip wm831x_irq_chip = {
	.name = "wm831x",
	.bus_lock = wm831x_irq_lock,
	.bus_sync_unlock = wm831x_irq_sync_unlock,
	.mask = wm831x_irq_mask,
	.unmask = wm831x_irq_unmask,
	.set_type = wm831x_irq_set_type,
};

/* The processing of the primary interrupt occurs in a thread so that
 * we can interact with the device over I2C or SPI. */
static irqreturn_t wm831x_irq_thread(int irq, void *data)
{
	struct wm831x *wm831x = data;
	unsigned int i;
	int primary;
	int status_regs[WM831X_NUM_IRQ_REGS] = { 0 };
	int read[WM831X_NUM_IRQ_REGS] = { 0 };
	int *status;

	primary = wm831x_reg_read(wm831x, WM831X_SYSTEM_INTERRUPTS);
	if (primary < 0) {
		dev_err(wm831x->dev, "Failed to read system interrupt: %d\n",
			primary);
		goto out;
	}

	for (i = 0; i < ARRAY_SIZE(wm831x_irqs); i++) {
		int offset = wm831x_irqs[i].reg - 1;

		if (!(primary & wm831x_irqs[i].primary))
			continue;

		status = &status_regs[offset];

		/* Hopefully there should only be one register to read
		 * each time otherwise we ought to do a block read. */
		if (!read[offset]) {
			*status = wm831x_reg_read(wm831x,
				     irq_data_to_status_reg(&wm831x_irqs[i]));
			if (*status < 0) {
				dev_err(wm831x->dev,
					"Failed to read IRQ status: %d\n",
					*status);
				goto out;
			}

			read[offset] = 1;
		}

		/* Report it if it isn't masked, or forget the status. */
		if ((*status & ~wm831x->irq_masks_cur[offset])
		    & wm831x_irqs[i].mask)
			handle_nested_irq(wm831x->irq_base + i);
		else
			*status &= ~wm831x_irqs[i].mask;
	}

out:
	for (i = 0; i < ARRAY_SIZE(status_regs); i++) {
		if (status_regs[i])
			wm831x_reg_write(wm831x, WM831X_INTERRUPT_STATUS_1 + i,
					 status_regs[i]);
	}

	return IRQ_HANDLED;
}

int wm831x_irq_init(struct wm831x *wm831x, int irq)
{
	struct wm831x_pdata *pdata = wm831x->dev->platform_data;
	int i, cur_irq, ret;

	mutex_init(&wm831x->irq_lock);

	/* Mask the individual interrupt sources */
	for (i = 0; i < ARRAY_SIZE(wm831x->irq_masks_cur); i++) {
		wm831x->irq_masks_cur[i] = 0xffff;
		wm831x->irq_masks_cache[i] = 0xffff;
		wm831x_reg_write(wm831x, WM831X_INTERRUPT_STATUS_1_MASK + i,
				 0xffff);
	}

	if (!irq) {
		dev_warn(wm831x->dev,
			 "No interrupt specified - functionality limited\n");
		return 0;
	}

	if (!pdata || !pdata->irq_base) {
		dev_err(wm831x->dev,
			"No interrupt base specified, no interrupts\n");
		return 0;
	}

	wm831x->irq = irq;
	wm831x->irq_base = pdata->irq_base;

	/* Register them with genirq */
	for (cur_irq = wm831x->irq_base;
	     cur_irq < ARRAY_SIZE(wm831x_irqs) + wm831x->irq_base;
	     cur_irq++) {
		set_irq_chip_data(cur_irq, wm831x);
		set_irq_chip_and_handler(cur_irq, &wm831x_irq_chip,
					 handle_edge_irq);
		set_irq_nested_thread(cur_irq, 1);

		/* ARM needs us to explicitly flag the IRQ as valid
		 * and will set them noprobe when we do so. */
#ifdef CONFIG_ARM
		set_irq_flags(cur_irq, IRQF_VALID);
#else
		set_irq_noprobe(cur_irq);
#endif
	}

	ret = request_threaded_irq(irq, NULL, wm831x_irq_thread,
				   IRQF_TRIGGER_LOW | IRQF_ONESHOT,
				   "wm831x", wm831x);
	if (ret != 0) {
		dev_err(wm831x->dev, "Failed to request IRQ %d: %d\n",
			irq, ret);
		return ret;
	}

	/* Enable top level interrupts, we mask at secondary level */
	wm831x_reg_write(wm831x, WM831X_SYSTEM_INTERRUPTS_MASK, 0);

	return 0;
}

void wm831x_irq_exit(struct wm831x *wm831x)
{
	if (wm831x->irq)
		free_irq(wm831x->irq, wm831x);
}
