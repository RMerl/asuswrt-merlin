/*
 * FSL SoC setup code
 *
 * Maintained by Kumar Gala (see MAINTAINERS for contact information)
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/stddef.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/module.h>

#include <asm/system.h>
#include <asm/atomic.h>
#include <asm/io.h>
#include <asm/pci-bridge.h>
#include <asm/prom.h>
#include <sysdev/fsl_soc.h>

#undef DEBUG

#ifdef DEBUG
#define DBG(x...) printk(x)
#else
#define DBG(x...)
#endif

int mpc83xx_pci2_busno;

int mpc83xx_exclude_device(u_char bus, u_char devfn)
{
	if (bus == 0 && PCI_SLOT(devfn) == 0)
		return PCIBIOS_DEVICE_NOT_FOUND;
	if (mpc83xx_pci2_busno)
		if (bus == (mpc83xx_pci2_busno) && PCI_SLOT(devfn) == 0)
			return PCIBIOS_DEVICE_NOT_FOUND;
	return PCIBIOS_SUCCESSFUL;
}

int __init add_bridge(struct device_node *dev)
{
	int len;
	struct pci_controller *hose;
	struct resource rsrc;
	const int *bus_range;
	int primary = 1, has_address = 0;
	phys_addr_t immr = get_immrbase();

	DBG("Adding PCI host bridge %s\n", dev->full_name);

	/* Fetch host bridge registers address */
	has_address = (of_address_to_resource(dev, 0, &rsrc) == 0);

	/* Get bus range if any */
	bus_range = of_get_property(dev, "bus-range", &len);
	if (bus_range == NULL || len < 2 * sizeof(int)) {
		printk(KERN_WARNING "Can't get bus-range for %s, assume"
		       " bus 0\n", dev->full_name);
	}

	hose = pcibios_alloc_controller();
	if (!hose)
		return -ENOMEM;
	hose->arch_data = dev;
	hose->set_cfg_type = 1;

	hose->first_busno = bus_range ? bus_range[0] : 0;
	hose->last_busno = bus_range ? bus_range[1] : 0xff;

	/* MPC83xx supports up to two host controllers one at 0x8500 from immrbar
	 * the other at 0x8600, we consider the 0x8500 the primary controller
	 */
	/* PCI 1 */
	if ((rsrc.start & 0xfffff) == 0x8500) {
		setup_indirect_pci(hose, immr + 0x8300, immr + 0x8304);
	}
	/* PCI 2 */
	if ((rsrc.start & 0xfffff) == 0x8600) {
		setup_indirect_pci(hose, immr + 0x8380, immr + 0x8384);
		primary = 0;
		hose->bus_offset = hose->first_busno;
		mpc83xx_pci2_busno = hose->first_busno;
	}

	printk(KERN_INFO "Found MPC83xx PCI host bridge at 0x%016llx. "
	       "Firmware bus number: %d->%d\n",
	       (unsigned long long)rsrc.start, hose->first_busno,
	       hose->last_busno);

	DBG(" ->Hose at 0x%p, cfg_addr=0x%p,cfg_data=0x%p\n",
	    hose, hose->cfg_addr, hose->cfg_data);

	/* Interpret the "ranges" property */
	/* This also maps the I/O region and sets isa_io/mem_base */
	pci_process_bridge_OF_ranges(hose, dev, primary);

	return 0;
}
