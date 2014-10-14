/*
 * Copyright (C) 2004 Matthew Wilcox <matthew@wil.cx>
 * Copyright (C) 2004 Intel Corp.
 *
 * This code is released under the GNU General Public License version 2.
 */

/*
 * mmconfig.c - Low-level direct PCI config space access via MMCONFIG
 */

#include <linux/pci.h>
#include <linux/init.h>
#include <linux/acpi.h>
#include <asm/e820.h>
#include "pci.h"

/* Assume systems with more busses have correct MCFG */
#define mmcfg_virt_addr ((void __iomem *) fix_to_virt(FIX_PCIE_MCFG))

/* The base address of the last MMCONFIG device accessed */
static u32 mmcfg_last_accessed_device;
static int mmcfg_last_accessed_cpu;

/*
 * Functions for accessing PCI configuration space with MMCONFIG accesses
 */
static u32 get_base_addr(unsigned int seg, int bus, unsigned devfn)
{
	struct acpi_mcfg_allocation *cfg;
	int cfg_num;

	if (seg == 0 && bus < PCI_MMCFG_MAX_CHECK_BUS &&
	    test_bit(PCI_SLOT(devfn) + 32*bus, pci_mmcfg_fallback_slots))
		return 0;

	for (cfg_num = 0; cfg_num < pci_mmcfg_config_num; cfg_num++) {
		cfg = &pci_mmcfg_config[cfg_num];
		if (cfg->pci_segment == seg &&
		    (cfg->start_bus_number <= bus) &&
		    (cfg->end_bus_number >= bus))
			return cfg->address;
	}

	/* Fall back to type 0 */
	return 0;
}

/*
 * This is always called under pci_config_lock
 */
static void pci_exp_set_dev_base(unsigned int base, int bus, int devfn)
{
	u32 dev_base = base | (bus << 20) | (devfn << 12);
	int cpu = smp_processor_id();
	if (dev_base != mmcfg_last_accessed_device ||
	    cpu != mmcfg_last_accessed_cpu) {
		mmcfg_last_accessed_device = dev_base;
		mmcfg_last_accessed_cpu = cpu;
		set_fixmap_nocache(FIX_PCIE_MCFG, dev_base);
	}
}

static int pci_mmcfg_read(unsigned int seg, unsigned int bus,
			  unsigned int devfn, int reg, int len, u32 *value)
{
	unsigned long flags;
	u32 base;

	if ((bus > 255) || (devfn > 255) || (reg > 4095)) {
		*value = -1;
		return -EINVAL;
	}

	base = get_base_addr(seg, bus, devfn);
	if (!base)
		return pci_conf1_read(seg,bus,devfn,reg,len,value);

	spin_lock_irqsave(&pci_config_lock, flags);

	pci_exp_set_dev_base(base, bus, devfn);

	switch (len) {
	case 1:
		*value = readb(mmcfg_virt_addr + reg);
		break;
	case 2:
		*value = readw(mmcfg_virt_addr + reg);
		break;
	case 4:
		*value = readl(mmcfg_virt_addr + reg);
		break;
	}

	spin_unlock_irqrestore(&pci_config_lock, flags);

	return 0;
}

static int pci_mmcfg_write(unsigned int seg, unsigned int bus,
			   unsigned int devfn, int reg, int len, u32 value)
{
	unsigned long flags;
	u32 base;

	if ((bus > 255) || (devfn > 255) || (reg > 4095))
		return -EINVAL;

	base = get_base_addr(seg, bus, devfn);
	if (!base)
		return pci_conf1_write(seg,bus,devfn,reg,len,value);

	spin_lock_irqsave(&pci_config_lock, flags);

	pci_exp_set_dev_base(base, bus, devfn);

	switch (len) {
	case 1:
		writeb(value, mmcfg_virt_addr + reg);
		break;
	case 2:
		writew(value, mmcfg_virt_addr + reg);
		break;
	case 4:
		writel(value, mmcfg_virt_addr + reg);
		break;
	}

	spin_unlock_irqrestore(&pci_config_lock, flags);

	return 0;
}

static struct pci_raw_ops pci_mmcfg = {
	.read =		pci_mmcfg_read,
	.write =	pci_mmcfg_write,
};

int __init pci_mmcfg_arch_reachable(unsigned int seg, unsigned int bus,
				    unsigned int devfn)
{
	return get_base_addr(seg, bus, devfn) != 0;
}

int __init pci_mmcfg_arch_init(void)
{
	printk(KERN_INFO "PCI: Using MMCONFIG\n");
	raw_pci_ops = &pci_mmcfg;
	return 1;
}
