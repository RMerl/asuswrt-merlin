/*
 * GE PPC9A board support
 *
 * Author: Martyn Welch <martyn.welch@ge.com>
 *
 * Copyright 2008 GE Intelligent Platforms Embedded Systems, Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 * Based on: mpc86xx_hpcn.c (MPC86xx HPCN board specific routines)
 * Copyright 2006 Freescale Semiconductor Inc.
 *
 * NEC fixup adapted from arch/mips/pci/fixup-lm2e.c
 */

#include <linux/stddef.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/kdev_t.h>
#include <linux/delay.h>
#include <linux/seq_file.h>
#include <linux/of_platform.h>

#include <asm/system.h>
#include <asm/time.h>
#include <asm/machdep.h>
#include <asm/pci-bridge.h>
#include <asm/prom.h>
#include <mm/mmu_decl.h>
#include <asm/udbg.h>

#include <asm/mpic.h>
#include <asm/nvram.h>

#include <sysdev/fsl_pci.h>
#include <sysdev/fsl_soc.h>

#include "mpc86xx.h"
#include "gef_pic.h"

#undef DEBUG

#ifdef DEBUG
#define DBG (fmt...) do { printk(KERN_ERR "PPC9A: " fmt); } while (0)
#else
#define DBG (fmt...) do { } while (0)
#endif

void __iomem *ppc9a_regs;

static void __init gef_ppc9a_init_irq(void)
{
	struct device_node *cascade_node = NULL;

	mpc86xx_init_irq();

	/*
	 * There is a simple interrupt handler in the main FPGA, this needs
	 * to be cascaded into the MPIC
	 */
	cascade_node = of_find_compatible_node(NULL, NULL, "gef,fpga-pic-1.00");
	if (!cascade_node) {
		printk(KERN_WARNING "PPC9A: No FPGA PIC\n");
		return;
	}

	gef_pic_init(cascade_node);
	of_node_put(cascade_node);
}

static void __init gef_ppc9a_setup_arch(void)
{
	struct device_node *regs;
#ifdef CONFIG_PCI
	struct device_node *np;

	for_each_compatible_node(np, "pci", "fsl,mpc8641-pcie") {
		fsl_add_bridge(np, 1);
	}
#endif

	printk(KERN_INFO "GE Intelligent Platforms PPC9A 6U VME SBC\n");

#ifdef CONFIG_SMP
	mpc86xx_smp_init();
#endif

	/* Remap basic board registers */
	regs = of_find_compatible_node(NULL, NULL, "gef,ppc9a-fpga-regs");
	if (regs) {
		ppc9a_regs = of_iomap(regs, 0);
		if (ppc9a_regs == NULL)
			printk(KERN_WARNING "Unable to map board registers\n");
		of_node_put(regs);
	}

#if defined(CONFIG_MMIO_NVRAM)
	mmio_nvram_init();
#endif
}

/* Return the PCB revision */
static unsigned int gef_ppc9a_get_pcb_rev(void)
{
	unsigned int reg;

	reg = ioread32be(ppc9a_regs);
	return (reg >> 16) & 0xff;
}

/* Return the board (software) revision */
static unsigned int gef_ppc9a_get_board_rev(void)
{
	unsigned int reg;

	reg = ioread32be(ppc9a_regs);
	return (reg >> 8) & 0xff;
}

/* Return the FPGA revision */
static unsigned int gef_ppc9a_get_fpga_rev(void)
{
	unsigned int reg;

	reg = ioread32be(ppc9a_regs);
	return reg & 0xf;
}

/* Return VME Geographical Address */
static unsigned int gef_ppc9a_get_vme_geo_addr(void)
{
	unsigned int reg;

	reg = ioread32be(ppc9a_regs + 0x4);
	return reg & 0x1f;
}

/* Return VME System Controller Status */
static unsigned int gef_ppc9a_get_vme_is_syscon(void)
{
	unsigned int reg;

	reg = ioread32be(ppc9a_regs + 0x4);
	return (reg >> 9) & 0x1;
}

static void gef_ppc9a_show_cpuinfo(struct seq_file *m)
{
	uint svid = mfspr(SPRN_SVR);

	seq_printf(m, "Vendor\t\t: GE Intelligent Platforms\n");

	seq_printf(m, "Revision\t: %u%c\n", gef_ppc9a_get_pcb_rev(),
		('A' + gef_ppc9a_get_board_rev()));
	seq_printf(m, "FPGA Revision\t: %u\n", gef_ppc9a_get_fpga_rev());

	seq_printf(m, "SVR\t\t: 0x%x\n", svid);

	seq_printf(m, "VME geo. addr\t: %u\n", gef_ppc9a_get_vme_geo_addr());

	seq_printf(m, "VME syscon\t: %s\n",
		gef_ppc9a_get_vme_is_syscon() ? "yes" : "no");
}

static void __init gef_ppc9a_nec_fixup(struct pci_dev *pdev)
{
	unsigned int val;

	/* Do not do the fixup on other platforms! */
	if (!machine_is(gef_ppc9a))
		return;

	printk(KERN_INFO "Running NEC uPD720101 Fixup\n");

	/* Ensure ports 1, 2, 3, 4 & 5 are enabled */
	pci_read_config_dword(pdev, 0xe0, &val);
	pci_write_config_dword(pdev, 0xe0, (val & ~7) | 0x5);

	/* System clock is 48-MHz Oscillator and EHCI Enabled. */
	pci_write_config_dword(pdev, 0xe4, 1 << 5);
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_NEC, PCI_DEVICE_ID_NEC_USB,
	gef_ppc9a_nec_fixup);

/*
 * Called very early, device-tree isn't unflattened
 *
 * This function is called to determine whether the BSP is compatible with the
 * supplied device-tree, which is assumed to be the correct one for the actual
 * board. It is expected thati, in the future, a kernel may support multiple
 * boards.
 */
static int __init gef_ppc9a_probe(void)
{
	unsigned long root = of_get_flat_dt_root();

	if (of_flat_dt_is_compatible(root, "gef,ppc9a"))
		return 1;

	return 0;
}

static long __init mpc86xx_time_init(void)
{
	unsigned int temp;

	/* Set the time base to zero */
	mtspr(SPRN_TBWL, 0);
	mtspr(SPRN_TBWU, 0);

	temp = mfspr(SPRN_HID0);
	temp |= HID0_TBEN;
	mtspr(SPRN_HID0, temp);
	asm volatile("isync");

	return 0;
}

static __initdata struct of_device_id of_bus_ids[] = {
	{ .compatible = "simple-bus", },
	{ .compatible = "gianfar", },
	{},
};

static int __init declare_of_platform_devices(void)
{
	printk(KERN_DEBUG "Probe platform devices\n");
	of_platform_bus_probe(NULL, of_bus_ids, NULL);

	return 0;
}
machine_device_initcall(gef_ppc9a, declare_of_platform_devices);

define_machine(gef_ppc9a) {
	.name			= "GE PPC9A",
	.probe			= gef_ppc9a_probe,
	.setup_arch		= gef_ppc9a_setup_arch,
	.init_IRQ		= gef_ppc9a_init_irq,
	.show_cpuinfo		= gef_ppc9a_show_cpuinfo,
	.get_irq		= mpic_get_irq,
	.restart		= fsl_rstcr_restart,
	.time_init		= mpc86xx_time_init,
	.calibrate_decr		= generic_calibrate_decr,
	.progress		= udbg_progress,
#ifdef CONFIG_PCI
	.pcibios_fixup_bus	= fsl_pcibios_fixup_bus,
#endif
};
