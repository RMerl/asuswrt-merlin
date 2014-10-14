/*
 * TI's OMAP DSP platform device registration
 *
 * Copyright (C) 2005-2006 Texas Instruments, Inc.
 * Copyright (C) 2009 Nokia Corporation
 *
 * Written by Hiroshi DOYU <Hiroshi.DOYU@nokia.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/*
 * XXX The function pointers to the PRM/CM functions are incorrect and
 * should be removed.  No device driver should be changing PRM/CM bits
 * directly; that's a layering violation -- those bits are the responsibility
 * of the OMAP PM core code.
 */

#include <linux/platform_device.h>
#include "cm2xxx_3xxx.h"
#include "prm2xxx_3xxx.h"
#ifdef CONFIG_BRIDGE_DVFS
#include <plat/omap-pm.h>
#endif

#include <plat/dsp.h>

extern phys_addr_t omap_dsp_get_mempool_base(void);

static struct platform_device *omap_dsp_pdev;

static struct omap_dsp_platform_data omap_dsp_pdata __initdata = {
#ifdef CONFIG_BRIDGE_DVFS
	.dsp_set_min_opp = omap_pm_dsp_set_min_opp,
	.dsp_get_opp = omap_pm_dsp_get_opp,
	.cpu_set_freq = omap_pm_cpu_set_freq,
	.cpu_get_freq = omap_pm_cpu_get_freq,
#endif
	.dsp_prm_read = omap2_prm_read_mod_reg,
	.dsp_prm_write = omap2_prm_write_mod_reg,
	.dsp_prm_rmw_bits = omap2_prm_rmw_mod_reg_bits,
	.dsp_cm_read = omap2_cm_read_mod_reg,
	.dsp_cm_write = omap2_cm_write_mod_reg,
	.dsp_cm_rmw_bits = omap2_cm_rmw_mod_reg_bits,
};

static int __init omap_dsp_init(void)
{
	struct platform_device *pdev;
	int err = -ENOMEM;
	struct omap_dsp_platform_data *pdata = &omap_dsp_pdata;

	pdata->phys_mempool_base = omap_dsp_get_mempool_base();

	if (pdata->phys_mempool_base) {
		pdata->phys_mempool_size = CONFIG_TIDSPBRIDGE_MEMPOOL_SIZE;
		pr_info("%s: %x bytes @ %x\n", __func__,
			pdata->phys_mempool_size, pdata->phys_mempool_base);
	}

	pdev = platform_device_alloc("omap-dsp", -1);
	if (!pdev)
		goto err_out;

	err = platform_device_add_data(pdev, pdata, sizeof(*pdata));
	if (err)
		goto err_out;

	err = platform_device_add(pdev);
	if (err)
		goto err_out;

	omap_dsp_pdev = pdev;
	return 0;

err_out:
	platform_device_put(pdev);
	return err;
}
module_init(omap_dsp_init);

static void __exit omap_dsp_exit(void)
{
	platform_device_unregister(omap_dsp_pdev);
}
module_exit(omap_dsp_exit);

MODULE_AUTHOR("Hiroshi DOYU");
MODULE_DESCRIPTION("TI's OMAP DSP platform device registration");
MODULE_LICENSE("GPL");
