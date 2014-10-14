/*
 * Samsung keypad driver
 *
 * Copyright (C) 2010 Samsung Electronics Co.Ltd
 * Author: Joonyoung Shim <jy0922.shim@samsung.com>
 * Author: Donghwa Lee <dh09.lee@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <plat/keypad.h>

#define SAMSUNG_KEYIFCON			0x00
#define SAMSUNG_KEYIFSTSCLR			0x04
#define SAMSUNG_KEYIFCOL			0x08
#define SAMSUNG_KEYIFROW			0x0c
#define SAMSUNG_KEYIFFC				0x10

/* SAMSUNG_KEYIFCON */
#define SAMSUNG_KEYIFCON_INT_F_EN		(1 << 0)
#define SAMSUNG_KEYIFCON_INT_R_EN		(1 << 1)
#define SAMSUNG_KEYIFCON_DF_EN			(1 << 2)
#define SAMSUNG_KEYIFCON_FC_EN			(1 << 3)
#define SAMSUNG_KEYIFCON_WAKEUPEN		(1 << 4)

/* SAMSUNG_KEYIFSTSCLR */
#define SAMSUNG_KEYIFSTSCLR_P_INT_MASK		(0xff << 0)
#define SAMSUNG_KEYIFSTSCLR_R_INT_MASK		(0xff << 8)
#define SAMSUNG_KEYIFSTSCLR_R_INT_OFFSET	8
#define S5PV210_KEYIFSTSCLR_P_INT_MASK		(0x3fff << 0)
#define S5PV210_KEYIFSTSCLR_R_INT_MASK		(0x3fff << 16)
#define S5PV210_KEYIFSTSCLR_R_INT_OFFSET	16

/* SAMSUNG_KEYIFCOL */
#define SAMSUNG_KEYIFCOL_MASK			(0xff << 0)
#define S5PV210_KEYIFCOLEN_MASK			(0xff << 8)

/* SAMSUNG_KEYIFROW */
#define SAMSUNG_KEYIFROW_MASK			(0xff << 0)
#define S5PV210_KEYIFROW_MASK			(0x3fff << 0)

/* SAMSUNG_KEYIFFC */
#define SAMSUNG_KEYIFFC_MASK			(0x3ff << 0)

enum samsung_keypad_type {
	KEYPAD_TYPE_SAMSUNG,
	KEYPAD_TYPE_S5PV210,
};

struct samsung_keypad {
	struct input_dev *input_dev;
	struct clk *clk;
	void __iomem *base;
	wait_queue_head_t wait;
	bool stopped;
	int irq;
	unsigned int row_shift;
	unsigned int rows;
	unsigned int cols;
	unsigned int row_state[SAMSUNG_MAX_COLS];
	unsigned short keycodes[];
};

static int samsung_keypad_is_s5pv210(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	enum samsung_keypad_type type =
		platform_get_device_id(pdev)->driver_data;

	return type == KEYPAD_TYPE_S5PV210;
}

static void samsung_keypad_scan(struct samsung_keypad *keypad,
				unsigned int *row_state)
{
	struct device *dev = keypad->input_dev->dev.parent;
	unsigned int col;
	unsigned int val;

	for (col = 0; col < keypad->cols; col++) {
		if (samsung_keypad_is_s5pv210(dev)) {
			val = S5PV210_KEYIFCOLEN_MASK;
			val &= ~(1 << col) << 8;
		} else {
			val = SAMSUNG_KEYIFCOL_MASK;
			val &= ~(1 << col);
		}

		writel(val, keypad->base + SAMSUNG_KEYIFCOL);
		mdelay(1);

		val = readl(keypad->base + SAMSUNG_KEYIFROW);
		row_state[col] = ~val & ((1 << keypad->rows) - 1);
	}

	/* KEYIFCOL reg clear */
	writel(0, keypad->base + SAMSUNG_KEYIFCOL);
}

static bool samsung_keypad_report(struct samsung_keypad *keypad,
				  unsigned int *row_state)
{
	struct input_dev *input_dev = keypad->input_dev;
	unsigned int changed;
	unsigned int pressed;
	unsigned int key_down = 0;
	unsigned int val;
	unsigned int col, row;

	for (col = 0; col < keypad->cols; col++) {
		changed = row_state[col] ^ keypad->row_state[col];
		key_down |= row_state[col];
		if (!changed)
			continue;

		for (row = 0; row < keypad->rows; row++) {
			if (!(changed & (1 << row)))
				continue;

			pressed = row_state[col] & (1 << row);

			dev_dbg(&keypad->input_dev->dev,
				"key %s, row: %d, col: %d\n",
				pressed ? "pressed" : "released", row, col);

			val = MATRIX_SCAN_CODE(row, col, keypad->row_shift);

			input_event(input_dev, EV_MSC, MSC_SCAN, val);
			input_report_key(input_dev,
					keypad->keycodes[val], pressed);
		}
		input_sync(keypad->input_dev);
	}

	memcpy(keypad->row_state, row_state, sizeof(keypad->row_state));

	return key_down;
}

static irqreturn_t samsung_keypad_irq(int irq, void *dev_id)
{
	struct samsung_keypad *keypad = dev_id;
	unsigned int row_state[SAMSUNG_MAX_COLS];
	unsigned int val;
	bool key_down;

	do {
		val = readl(keypad->base + SAMSUNG_KEYIFSTSCLR);
		/* Clear interrupt. */
		writel(~0x0, keypad->base + SAMSUNG_KEYIFSTSCLR);

		samsung_keypad_scan(keypad, row_state);

		key_down = samsung_keypad_report(keypad, row_state);
		if (key_down)
			wait_event_timeout(keypad->wait, keypad->stopped,
					   msecs_to_jiffies(50));

	} while (key_down && !keypad->stopped);

	return IRQ_HANDLED;
}

static void samsung_keypad_start(struct samsung_keypad *keypad)
{
	unsigned int val;

	/* Tell IRQ thread that it may poll the device. */
	keypad->stopped = false;

	clk_enable(keypad->clk);

	/* Enable interrupt bits. */
	val = readl(keypad->base + SAMSUNG_KEYIFCON);
	val |= SAMSUNG_KEYIFCON_INT_F_EN | SAMSUNG_KEYIFCON_INT_R_EN;
	writel(val, keypad->base + SAMSUNG_KEYIFCON);

	/* KEYIFCOL reg clear. */
	writel(0, keypad->base + SAMSUNG_KEYIFCOL);
}

static void samsung_keypad_stop(struct samsung_keypad *keypad)
{
	unsigned int val;

	/* Signal IRQ thread to stop polling and disable the handler. */
	keypad->stopped = true;
	wake_up(&keypad->wait);
	disable_irq(keypad->irq);

	/* Clear interrupt. */
	writel(~0x0, keypad->base + SAMSUNG_KEYIFSTSCLR);

	/* Disable interrupt bits. */
	val = readl(keypad->base + SAMSUNG_KEYIFCON);
	val &= ~(SAMSUNG_KEYIFCON_INT_F_EN | SAMSUNG_KEYIFCON_INT_R_EN);
	writel(val, keypad->base + SAMSUNG_KEYIFCON);

	clk_disable(keypad->clk);

	/*
	 * Now that chip should not generate interrupts we can safely
	 * re-enable the handler.
	 */
	enable_irq(keypad->irq);
}

static int samsung_keypad_open(struct input_dev *input_dev)
{
	struct samsung_keypad *keypad = input_get_drvdata(input_dev);

	samsung_keypad_start(keypad);

	return 0;
}

static void samsung_keypad_close(struct input_dev *input_dev)
{
	struct samsung_keypad *keypad = input_get_drvdata(input_dev);

	samsung_keypad_stop(keypad);
}

static int __devinit samsung_keypad_probe(struct platform_device *pdev)
{
	const struct samsung_keypad_platdata *pdata;
	const struct matrix_keymap_data *keymap_data;
	struct samsung_keypad *keypad;
	struct resource *res;
	struct input_dev *input_dev;
	unsigned int row_shift;
	unsigned int keymap_size;
	int error;

	pdata = pdev->dev.platform_data;
	if (!pdata) {
		dev_err(&pdev->dev, "no platform data defined\n");
		return -EINVAL;
	}

	keymap_data = pdata->keymap_data;
	if (!keymap_data) {
		dev_err(&pdev->dev, "no keymap data defined\n");
		return -EINVAL;
	}

	if (!pdata->rows || pdata->rows > SAMSUNG_MAX_ROWS)
		return -EINVAL;

	if (!pdata->cols || pdata->cols > SAMSUNG_MAX_COLS)
		return -EINVAL;

	/* initialize the gpio */
	if (pdata->cfg_gpio)
		pdata->cfg_gpio(pdata->rows, pdata->cols);

	row_shift = get_count_order(pdata->cols);
	keymap_size = (pdata->rows << row_shift) * sizeof(keypad->keycodes[0]);

	keypad = kzalloc(sizeof(*keypad) + keymap_size, GFP_KERNEL);
	input_dev = input_allocate_device();
	if (!keypad || !input_dev) {
		error = -ENOMEM;
		goto err_free_mem;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		error = -ENODEV;
		goto err_free_mem;
	}

	keypad->base = ioremap(res->start, resource_size(res));
	if (!keypad->base) {
		error = -EBUSY;
		goto err_free_mem;
	}

	keypad->clk = clk_get(&pdev->dev, "keypad");
	if (IS_ERR(keypad->clk)) {
		dev_err(&pdev->dev, "failed to get keypad clk\n");
		error = PTR_ERR(keypad->clk);
		goto err_unmap_base;
	}

	keypad->input_dev = input_dev;
	keypad->row_shift = row_shift;
	keypad->rows = pdata->rows;
	keypad->cols = pdata->cols;
	init_waitqueue_head(&keypad->wait);

	input_dev->name = pdev->name;
	input_dev->id.bustype = BUS_HOST;
	input_dev->dev.parent = &pdev->dev;
	input_set_drvdata(input_dev, keypad);

	input_dev->open = samsung_keypad_open;
	input_dev->close = samsung_keypad_close;

	input_dev->evbit[0] = BIT_MASK(EV_KEY);
	if (!pdata->no_autorepeat)
		input_dev->evbit[0] |= BIT_MASK(EV_REP);

	input_set_capability(input_dev, EV_MSC, MSC_SCAN);

	input_dev->keycode = keypad->keycodes;
	input_dev->keycodesize = sizeof(keypad->keycodes[0]);
	input_dev->keycodemax = pdata->rows << row_shift;

	matrix_keypad_build_keymap(keymap_data, row_shift,
			input_dev->keycode, input_dev->keybit);

	keypad->irq = platform_get_irq(pdev, 0);
	if (keypad->irq < 0) {
		error = keypad->irq;
		goto err_put_clk;
	}

	error = request_threaded_irq(keypad->irq, NULL, samsung_keypad_irq,
			IRQF_ONESHOT, dev_name(&pdev->dev), keypad);
	if (error) {
		dev_err(&pdev->dev, "failed to register keypad interrupt\n");
		goto err_put_clk;
	}

	error = input_register_device(keypad->input_dev);
	if (error)
		goto err_free_irq;

	device_init_wakeup(&pdev->dev, pdata->wakeup);
	platform_set_drvdata(pdev, keypad);
	return 0;

err_free_irq:
	free_irq(keypad->irq, keypad);
err_put_clk:
	clk_put(keypad->clk);
err_unmap_base:
	iounmap(keypad->base);
err_free_mem:
	input_free_device(input_dev);
	kfree(keypad);

	return error;
}

static int __devexit samsung_keypad_remove(struct platform_device *pdev)
{
	struct samsung_keypad *keypad = platform_get_drvdata(pdev);

	device_init_wakeup(&pdev->dev, 0);
	platform_set_drvdata(pdev, NULL);

	input_unregister_device(keypad->input_dev);

	/*
	 * It is safe to free IRQ after unregistering device because
	 * samsung_keypad_close will shut off interrupts.
	 */
	free_irq(keypad->irq, keypad);

	clk_put(keypad->clk);

	iounmap(keypad->base);
	kfree(keypad);

	return 0;
}

#ifdef CONFIG_PM
static void samsung_keypad_toggle_wakeup(struct samsung_keypad *keypad,
					 bool enable)
{
	struct device *dev = keypad->input_dev->dev.parent;
	unsigned int val;

	clk_enable(keypad->clk);

	val = readl(keypad->base + SAMSUNG_KEYIFCON);
	if (enable) {
		val |= SAMSUNG_KEYIFCON_WAKEUPEN;
		if (device_may_wakeup(dev))
			enable_irq_wake(keypad->irq);
	} else {
		val &= ~SAMSUNG_KEYIFCON_WAKEUPEN;
		if (device_may_wakeup(dev))
			disable_irq_wake(keypad->irq);
	}
	writel(val, keypad->base + SAMSUNG_KEYIFCON);

	clk_disable(keypad->clk);
}

static int samsung_keypad_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct samsung_keypad *keypad = platform_get_drvdata(pdev);
	struct input_dev *input_dev = keypad->input_dev;

	mutex_lock(&input_dev->mutex);

	if (input_dev->users)
		samsung_keypad_stop(keypad);

	samsung_keypad_toggle_wakeup(keypad, true);

	mutex_unlock(&input_dev->mutex);

	return 0;
}

static int samsung_keypad_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct samsung_keypad *keypad = platform_get_drvdata(pdev);
	struct input_dev *input_dev = keypad->input_dev;

	mutex_lock(&input_dev->mutex);

	samsung_keypad_toggle_wakeup(keypad, false);

	if (input_dev->users)
		samsung_keypad_start(keypad);

	mutex_unlock(&input_dev->mutex);

	return 0;
}

static const struct dev_pm_ops samsung_keypad_pm_ops = {
	.suspend	= samsung_keypad_suspend,
	.resume		= samsung_keypad_resume,
};
#endif

static struct platform_device_id samsung_keypad_driver_ids[] = {
	{
		.name		= "samsung-keypad",
		.driver_data	= KEYPAD_TYPE_SAMSUNG,
	}, {
		.name		= "s5pv210-keypad",
		.driver_data	= KEYPAD_TYPE_S5PV210,
	},
	{ },
};
MODULE_DEVICE_TABLE(platform, samsung_keypad_driver_ids);

static struct platform_driver samsung_keypad_driver = {
	.probe		= samsung_keypad_probe,
	.remove		= __devexit_p(samsung_keypad_remove),
	.driver		= {
		.name	= "samsung-keypad",
		.owner	= THIS_MODULE,
#ifdef CONFIG_PM
		.pm	= &samsung_keypad_pm_ops,
#endif
	},
	.id_table	= samsung_keypad_driver_ids,
};

static int __init samsung_keypad_init(void)
{
	return platform_driver_register(&samsung_keypad_driver);
}
module_init(samsung_keypad_init);

static void __exit samsung_keypad_exit(void)
{
	platform_driver_unregister(&samsung_keypad_driver);
}
module_exit(samsung_keypad_exit);

MODULE_DESCRIPTION("Samsung keypad driver");
MODULE_AUTHOR("Joonyoung Shim <jy0922.shim@samsung.com>");
MODULE_AUTHOR("Donghwa Lee <dh09.lee@samsung.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:samsung-keypad");
