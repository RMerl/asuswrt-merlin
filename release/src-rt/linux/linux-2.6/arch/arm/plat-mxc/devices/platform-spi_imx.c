/*
 * Copyright (C) 2009-2010 Pengutronix
 * Uwe Kleine-Koenig <u.kleine-koenig@pengutronix.de>
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation.
 */
#include <mach/hardware.h>
#include <mach/devices-common.h>

#define imx_spi_imx_data_entry_single(soc, type, _devid, _id, hwid, _size) \
	{								\
		.devid = _devid,					\
		.id = _id,						\
		.iobase = soc ## _ ## type ## hwid ## _BASE_ADDR,	\
		.iosize = _size,					\
		.irq = soc ## _INT_ ## type ## hwid,			\
	}

#define imx_spi_imx_data_entry(soc, type, devid, id, hwid, size)	\
	[id] = imx_spi_imx_data_entry_single(soc, type, devid, id, hwid, size)

#ifdef CONFIG_SOC_IMX1
const struct imx_spi_imx_data imx1_cspi_data[] __initconst = {
#define imx1_cspi_data_entry(_id, _hwid) \
	imx_spi_imx_data_entry(MX1, CSPI, "imx1-cspi", _id, _hwid, SZ_4K)
	imx1_cspi_data_entry(0, 1),
	imx1_cspi_data_entry(1, 2),
};
#endif

#ifdef CONFIG_SOC_IMX21
const struct imx_spi_imx_data imx21_cspi_data[] __initconst = {
#define imx21_cspi_data_entry(_id, _hwid)                            \
	imx_spi_imx_data_entry(MX21, CSPI, "imx21-cspi", _id, _hwid, SZ_4K)
	imx21_cspi_data_entry(0, 1),
	imx21_cspi_data_entry(1, 2),
};
#endif

#ifdef CONFIG_SOC_IMX25
const struct imx_spi_imx_data imx25_cspi_data[] __initconst = {
#define imx25_cspi_data_entry(_id, _hwid)				\
	imx_spi_imx_data_entry(MX25, CSPI, "imx25-cspi", _id, _hwid, SZ_16K)
	imx25_cspi_data_entry(0, 1),
	imx25_cspi_data_entry(1, 2),
	imx25_cspi_data_entry(2, 3),
};
#endif /* ifdef CONFIG_SOC_IMX25 */

#ifdef CONFIG_SOC_IMX27
const struct imx_spi_imx_data imx27_cspi_data[] __initconst = {
#define imx27_cspi_data_entry(_id, _hwid)				\
	imx_spi_imx_data_entry(MX27, CSPI, "imx27-cspi", _id, _hwid, SZ_4K)
	imx27_cspi_data_entry(0, 1),
	imx27_cspi_data_entry(1, 2),
	imx27_cspi_data_entry(2, 3),
};
#endif /* ifdef CONFIG_SOC_IMX27 */

#ifdef CONFIG_SOC_IMX31
const struct imx_spi_imx_data imx31_cspi_data[] __initconst = {
#define imx31_cspi_data_entry(_id, _hwid)				\
	imx_spi_imx_data_entry(MX31, CSPI, "imx31-cspi", _id, _hwid, SZ_4K)
	imx31_cspi_data_entry(0, 1),
	imx31_cspi_data_entry(1, 2),
	imx31_cspi_data_entry(2, 3),
};
#endif /* ifdef CONFIG_SOC_IMX31 */

#ifdef CONFIG_SOC_IMX35
const struct imx_spi_imx_data imx35_cspi_data[] __initconst = {
#define imx35_cspi_data_entry(_id, _hwid)                           \
	imx_spi_imx_data_entry(MX35, CSPI, "imx35-cspi", _id, _hwid, SZ_4K)
	imx35_cspi_data_entry(0, 1),
	imx35_cspi_data_entry(1, 2),
};
#endif /* ifdef CONFIG_SOC_IMX35 */

#ifdef CONFIG_SOC_IMX51
const struct imx_spi_imx_data imx51_cspi_data __initconst =
	imx_spi_imx_data_entry_single(MX51, CSPI, "imx51-cspi", 0, , SZ_4K);

const struct imx_spi_imx_data imx51_ecspi_data[] __initconst = {
#define imx51_ecspi_data_entry(_id, _hwid)				\
	imx_spi_imx_data_entry(MX51, ECSPI, "imx51-ecspi", _id, _hwid, SZ_4K)
	imx51_ecspi_data_entry(0, 1),
	imx51_ecspi_data_entry(1, 2),
};
#endif /* ifdef CONFIG_SOC_IMX51 */

#ifdef CONFIG_SOC_IMX53
const struct imx_spi_imx_data imx53_cspi_data __initconst =
	imx_spi_imx_data_entry_single(MX53, CSPI, "imx53-cspi", 0, , SZ_4K);

const struct imx_spi_imx_data imx53_ecspi_data[] __initconst = {
#define imx53_ecspi_data_entry(_id, _hwid)				\
	imx_spi_imx_data_entry(MX53, ECSPI, "imx53-ecspi", _id, _hwid, SZ_4K)
	imx53_ecspi_data_entry(0, 1),
	imx53_ecspi_data_entry(1, 2),
};
#endif /* ifdef CONFIG_SOC_IMX53 */

struct platform_device *__init imx_add_spi_imx(
		const struct imx_spi_imx_data *data,
		const struct spi_imx_master *pdata)
{
	struct resource res[] = {
		{
			.start = data->iobase,
			.end = data->iobase + data->iosize - 1,
			.flags = IORESOURCE_MEM,
		}, {
			.start = data->irq,
			.end = data->irq,
			.flags = IORESOURCE_IRQ,
		},
	};

	return imx_add_platform_device(data->devid, data->id,
			res, ARRAY_SIZE(res), pdata, sizeof(*pdata));
}
