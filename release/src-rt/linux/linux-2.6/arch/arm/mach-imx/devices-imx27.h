/*
 * Copyright (C) 2010 Pengutronix
 * Uwe Kleine-Koenig <u.kleine-koenig@pengutronix.de>
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation.
 */
#include <mach/mx27.h>
#include <mach/devices-common.h>

extern const struct imx_fec_data imx27_fec_data __initconst;
#define imx27_add_fec(pdata)	\
	imx_add_fec(&imx27_fec_data, pdata)

extern const struct imx_fsl_usb2_udc_data imx27_fsl_usb2_udc_data __initconst;
#define imx27_add_fsl_usb2_udc(pdata)	\
	imx_add_fsl_usb2_udc(&imx27_fsl_usb2_udc_data, pdata)

extern const struct imx_imx2_wdt_data imx27_imx2_wdt_data __initconst;
#define imx27_add_imx2_wdt(pdata)	\
	imx_add_imx2_wdt(&imx27_imx2_wdt_data)

extern const struct imx_imx_fb_data imx27_imx_fb_data __initconst;
#define imx27_add_imx_fb(pdata)	\
	imx_add_imx_fb(&imx27_imx_fb_data, pdata)

extern const struct imx_imx_i2c_data imx27_imx_i2c_data[] __initconst;
#define imx27_add_imx_i2c(id, pdata)	\
	imx_add_imx_i2c(&imx27_imx_i2c_data[id], pdata)

extern const struct imx_imx_keypad_data imx27_imx_keypad_data __initconst;
#define imx27_add_imx_keypad(pdata)	\
	imx_add_imx_keypad(&imx27_imx_keypad_data, pdata)

extern const struct imx_imx_ssi_data imx27_imx_ssi_data[] __initconst;
#define imx27_add_imx_ssi(id, pdata)    \
	imx_add_imx_ssi(&imx27_imx_ssi_data[id], pdata)

extern const struct imx_imx_uart_1irq_data imx27_imx_uart_data[] __initconst;
#define imx27_add_imx_uart(id, pdata)	\
	imx_add_imx_uart_1irq(&imx27_imx_uart_data[id], pdata)
#define imx27_add_imx_uart0(pdata)	imx27_add_imx_uart(0, pdata)
#define imx27_add_imx_uart1(pdata)	imx27_add_imx_uart(1, pdata)
#define imx27_add_imx_uart2(pdata)	imx27_add_imx_uart(2, pdata)
#define imx27_add_imx_uart3(pdata)	imx27_add_imx_uart(3, pdata)
#define imx27_add_imx_uart4(pdata)	imx27_add_imx_uart(4, pdata)
#define imx27_add_imx_uart5(pdata)	imx27_add_imx_uart(5, pdata)

extern const struct imx_mx2_camera_data imx27_mx2_camera_data __initconst;
#define imx27_add_mx2_camera(pdata)	\
	imx_add_mx2_camera(&imx27_mx2_camera_data, pdata)

extern const struct imx_mxc_ehci_data imx27_mxc_ehci_otg_data __initconst;
#define imx27_add_mxc_ehci_otg(pdata)	\
	imx_add_mxc_ehci(&imx27_mxc_ehci_otg_data, pdata)
extern const struct imx_mxc_ehci_data imx27_mxc_ehci_hs_data[] __initconst;
#define imx27_add_mxc_ehci_hs(id, pdata)	\
	imx_add_mxc_ehci(&imx27_mxc_ehci_hs_data[id - 1], pdata)

extern const struct imx_mxc_mmc_data imx27_mxc_mmc_data[] __initconst;
#define imx27_add_mxc_mmc(id, pdata)	\
	imx_add_mxc_mmc(&imx27_mxc_mmc_data[id], pdata)

extern const struct imx_mxc_nand_data imx27_mxc_nand_data __initconst;
#define imx27_add_mxc_nand(pdata)	\
	imx_add_mxc_nand(&imx27_mxc_nand_data, pdata)

extern const struct imx_mxc_w1_data imx27_mxc_w1_data __initconst;
#define imx27_add_mxc_w1(pdata)	\
	imx_add_mxc_w1(&imx27_mxc_w1_data)

extern const struct imx_spi_imx_data imx27_cspi_data[] __initconst;
#define imx27_add_cspi(id, pdata)	\
	imx_add_spi_imx(&imx27_cspi_data[id], pdata)
#define imx27_add_spi_imx0(pdata)	imx27_add_cspi(0, pdata)
#define imx27_add_spi_imx1(pdata)	imx27_add_cspi(1, pdata)
#define imx27_add_spi_imx2(pdata)	imx27_add_cspi(2, pdata)
