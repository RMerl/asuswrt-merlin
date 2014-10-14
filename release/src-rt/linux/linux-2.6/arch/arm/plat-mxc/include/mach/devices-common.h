/*
 * Copyright (C) 2009-2010 Pengutronix
 * Uwe Kleine-Koenig <u.kleine-koenig@pengutronix.de>
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/init.h>

struct platform_device *imx_add_platform_device_dmamask(
		const char *name, int id,
		const struct resource *res, unsigned int num_resources,
		const void *data, size_t size_data, u64 dmamask);

static inline struct platform_device *imx_add_platform_device(
		const char *name, int id,
		const struct resource *res, unsigned int num_resources,
		const void *data, size_t size_data)
{
	return imx_add_platform_device_dmamask(
			name, id, res, num_resources, data, size_data, 0);
}

#include <linux/fec.h>
struct imx_fec_data {
	resource_size_t iobase;
	resource_size_t irq;
};
struct platform_device *__init imx_add_fec(
		const struct imx_fec_data *data,
		const struct fec_platform_data *pdata);

#include <linux/can/platform/flexcan.h>
struct imx_flexcan_data {
	int id;
	resource_size_t iobase;
	resource_size_t iosize;
	resource_size_t irq;
};
struct platform_device *__init imx_add_flexcan(
		const struct imx_flexcan_data *data,
		const struct flexcan_platform_data *pdata);

#include <linux/fsl_devices.h>
struct imx_fsl_usb2_udc_data {
	resource_size_t iobase;
	resource_size_t irq;
};
struct platform_device *__init imx_add_fsl_usb2_udc(
		const struct imx_fsl_usb2_udc_data *data,
		const struct fsl_usb2_platform_data *pdata);

#include <linux/gpio_keys.h>
struct platform_device *__init imx_add_gpio_keys(
		const struct gpio_keys_platform_data *pdata);

#include <mach/mx21-usbhost.h>
struct imx_imx21_hcd_data {
	resource_size_t iobase;
	resource_size_t irq;
};
struct platform_device *__init imx_add_imx21_hcd(
		const struct imx_imx21_hcd_data *data,
		const struct mx21_usbh_platform_data *pdata);

struct imx_imx2_wdt_data {
	int id;
	resource_size_t iobase;
	resource_size_t iosize;
};
struct platform_device *__init imx_add_imx2_wdt(
		const struct imx_imx2_wdt_data *data);

struct imx_imxdi_rtc_data {
	resource_size_t iobase;
	resource_size_t irq;
};
struct platform_device *__init imx_add_imxdi_rtc(
		const struct imx_imxdi_rtc_data *data);

#include <mach/imxfb.h>
struct imx_imx_fb_data {
	resource_size_t iobase;
	resource_size_t iosize;
	resource_size_t irq;
};
struct platform_device *__init imx_add_imx_fb(
		const struct imx_imx_fb_data *data,
		const struct imx_fb_platform_data *pdata);

#include <mach/i2c.h>
struct imx_imx_i2c_data {
	int id;
	resource_size_t iobase;
	resource_size_t iosize;
	resource_size_t irq;
};
struct platform_device *__init imx_add_imx_i2c(
		const struct imx_imx_i2c_data *data,
		const struct imxi2c_platform_data *pdata);

#include <linux/input/matrix_keypad.h>
struct imx_imx_keypad_data {
	resource_size_t iobase;
	resource_size_t iosize;
	resource_size_t irq;
};
struct platform_device *__init imx_add_imx_keypad(
		const struct imx_imx_keypad_data *data,
		const struct matrix_keymap_data *pdata);

#include <mach/ssi.h>
struct imx_imx_ssi_data {
	int id;
	resource_size_t iobase;
	resource_size_t iosize;
	resource_size_t irq;
	resource_size_t dmatx0;
	resource_size_t dmarx0;
	resource_size_t dmatx1;
	resource_size_t dmarx1;
};
struct platform_device *__init imx_add_imx_ssi(
		const struct imx_imx_ssi_data *data,
		const struct imx_ssi_platform_data *pdata);

#include <mach/imx-uart.h>
struct imx_imx_uart_3irq_data {
	int id;
	resource_size_t iobase;
	resource_size_t iosize;
	resource_size_t irqrx;
	resource_size_t irqtx;
	resource_size_t irqrts;
};
struct platform_device *__init imx_add_imx_uart_3irq(
		const struct imx_imx_uart_3irq_data *data,
		const struct imxuart_platform_data *pdata);

struct imx_imx_uart_1irq_data {
	int id;
	resource_size_t iobase;
	resource_size_t iosize;
	resource_size_t irq;
};
struct platform_device *__init imx_add_imx_uart_1irq(
		const struct imx_imx_uart_1irq_data *data,
		const struct imxuart_platform_data *pdata);

#include <mach/usb.h>
struct imx_imx_udc_data {
	resource_size_t iobase;
	resource_size_t iosize;
	resource_size_t irq0;
	resource_size_t irq1;
	resource_size_t irq2;
	resource_size_t irq3;
	resource_size_t irq4;
	resource_size_t irq5;
	resource_size_t irq6;
};
struct platform_device *__init imx_add_imx_udc(
		const struct imx_imx_udc_data *data,
		const struct imxusb_platform_data *pdata);

#include <mach/mx1_camera.h>
struct imx_mx1_camera_data {
	resource_size_t iobase;
	resource_size_t iosize;
	resource_size_t irq;
};
struct platform_device *__init imx_add_mx1_camera(
		const struct imx_mx1_camera_data *data,
		const struct mx1_camera_pdata *pdata);

#include <mach/mx2_cam.h>
struct imx_mx2_camera_data {
	resource_size_t iobasecsi;
	resource_size_t iosizecsi;
	resource_size_t irqcsi;
	resource_size_t iobaseemmaprp;
	resource_size_t iosizeemmaprp;
	resource_size_t irqemmaprp;
};
struct platform_device *__init imx_add_mx2_camera(
		const struct imx_mx2_camera_data *data,
		const struct mx2_camera_platform_data *pdata);

#include <mach/mxc_ehci.h>
struct imx_mxc_ehci_data {
	int id;
	resource_size_t iobase;
	resource_size_t irq;
};
struct platform_device *__init imx_add_mxc_ehci(
		const struct imx_mxc_ehci_data *data,
		const struct mxc_usbh_platform_data *pdata);

#include <mach/mmc.h>
struct imx_mxc_mmc_data {
	int id;
	resource_size_t iobase;
	resource_size_t iosize;
	resource_size_t irq;
	resource_size_t dmareq;
};
struct platform_device *__init imx_add_mxc_mmc(
		const struct imx_mxc_mmc_data *data,
		const struct imxmmc_platform_data *pdata);

#include <mach/mxc_nand.h>
struct imx_mxc_nand_data {
	/*
	 * id is traditionally 0, but -1 is more appropriate.  We use -1 for new
	 * machines but don't change existing devices as the nand device usually
	 * appears in the kernel command line to pass its partitioning.
	 */
	int id;
	resource_size_t iobase;
	resource_size_t iosize;
	resource_size_t axibase;
	resource_size_t irq;
};
struct platform_device *__init imx_add_mxc_nand(
		const struct imx_mxc_nand_data *data,
		const struct mxc_nand_platform_data *pdata);

struct imx_mxc_pwm_data {
	int id;
	resource_size_t iobase;
	resource_size_t iosize;
	resource_size_t irq;
};
struct platform_device *__init imx_add_mxc_pwm(
		const struct imx_mxc_pwm_data *data);

struct imx_mxc_w1_data {
	resource_size_t iobase;
};
struct platform_device *__init imx_add_mxc_w1(
		const struct imx_mxc_w1_data *data);

#include <mach/esdhc.h>
struct imx_sdhci_esdhc_imx_data {
	int id;
	resource_size_t iobase;
	resource_size_t irq;
};
struct platform_device *__init imx_add_sdhci_esdhc_imx(
		const struct imx_sdhci_esdhc_imx_data *data,
		const struct esdhc_platform_data *pdata);

#include <mach/spi.h>
struct imx_spi_imx_data {
	const char *devid;
	int id;
	resource_size_t iobase;
	resource_size_t iosize;
	int irq;
};
struct platform_device *__init imx_add_spi_imx(
		const struct imx_spi_imx_data *data,
		const struct spi_imx_master *pdata);
