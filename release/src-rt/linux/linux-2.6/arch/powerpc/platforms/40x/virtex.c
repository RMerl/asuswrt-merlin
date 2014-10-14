/*
 * Xilinx Virtex (IIpro & 4FX) based board support
 *
 * Copyright 2007 Secret Lab Technologies Ltd.
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2. This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 */

#include <linux/init.h>
#include <linux/of_platform.h>
#include <asm/machdep.h>
#include <asm/prom.h>
#include <asm/time.h>
#include <asm/xilinx_intc.h>
#include <asm/xilinx_pci.h>
#include <asm/ppc4xx.h>

static struct of_device_id xilinx_of_bus_ids[] __initdata = {
	{ .compatible = "xlnx,plb-v46-1.00.a", },
	{ .compatible = "xlnx,plb-v34-1.01.a", },
	{ .compatible = "xlnx,plb-v34-1.02.a", },
	{ .compatible = "xlnx,opb-v20-1.10.c", },
	{ .compatible = "xlnx,dcr-v29-1.00.a", },
	{ .compatible = "xlnx,compound", },
	{}
};

static int __init virtex_device_probe(void)
{
	of_platform_bus_probe(NULL, xilinx_of_bus_ids, NULL);

	return 0;
}
machine_device_initcall(virtex, virtex_device_probe);

static int __init virtex_probe(void)
{
	unsigned long root = of_get_flat_dt_root();

	if (!of_flat_dt_is_compatible(root, "xlnx,virtex"))
		return 0;

	return 1;
}

define_machine(virtex) {
	.name			= "Xilinx Virtex",
	.probe			= virtex_probe,
	.setup_arch		= xilinx_pci_init,
	.init_IRQ		= xilinx_intc_init_tree,
	.get_irq		= xilinx_intc_get_irq,
	.restart		= ppc4xx_reset_system,
	.calibrate_decr		= generic_calibrate_decr,
};
