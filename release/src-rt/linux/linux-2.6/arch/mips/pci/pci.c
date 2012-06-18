/*
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 * Copyright (C) 2003, 04 Ralf Baechle (ralf@linux-mips.org)
 */
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/bootmem.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/pci.h>

/*
 * Indicate whether we respect the PCI setup left by the firmware.
 *
 * Make this long-lived  so that we know when shutting down
 * whether we probed only or not.
 */
int pci_probe_only;

#define PCI_ASSIGN_ALL_BUSSES	1

unsigned int pci_probe = PCI_ASSIGN_ALL_BUSSES;

/*
 * The PCI controller list.
 */

struct pci_controller *hose_head, **hose_tail = &hose_head;
struct pci_controller *pci_isa_hose;

unsigned long PCIBIOS_MIN_IO	= 0x0000;
unsigned long PCIBIOS_MIN_MEM	= 0;

/*
 * We need to avoid collisions with `mirrored' VGA ports
 * and other strange ISA hardware, so we always want the
 * addresses to be allocated in the 0x000-0x0ff region
 * modulo 0x400.
 *
 * Why? Because some silly external IO cards only decode
 * the low 10 bits of the IO address. The 0x00-0xff region
 * is reserved for motherboard devices that decode all 16
 * bits, so it's ok to allocate at, say, 0x2800-0x28ff,
 * but we want to try to avoid allocating at 0x2900-0x2bff
 * which might have be mirrored at 0x0100-0x03ff..
 */
void
pcibios_align_resource(void *data, struct resource *res,
		       resource_size_t size, resource_size_t align)
{
	struct pci_dev *dev = data;
	struct pci_controller *hose = dev->sysdata;
	resource_size_t start = res->start;

	if (res->flags & IORESOURCE_IO) {
		/* Make sure we start at our min on all hoses */
		if (start < PCIBIOS_MIN_IO + hose->io_resource->start)
			start = PCIBIOS_MIN_IO + hose->io_resource->start;

		/*
		 * Put everything into 0x00-0xff region modulo 0x400
		 */
		if (start & 0x300)
			start = (start + 0x3ff) & ~0x3ff;
	} else if (res->flags & IORESOURCE_MEM) {
		/* Make sure we start at our min on all hoses */
		if (start < PCIBIOS_MIN_MEM + hose->mem_resource->start)
			start = PCIBIOS_MIN_MEM + hose->mem_resource->start;
	}

	res->start = start;
}

void __init register_pci_controller(struct pci_controller *hose)
{
	if (request_resource(&iomem_resource, hose->mem_resource) < 0)
		goto out;
	if (request_resource(&ioport_resource, hose->io_resource) < 0) {
		release_resource(hose->mem_resource);
		goto out;
	}

	*hose_tail = hose;
	hose_tail = &hose->next;

	/*
	 * Do not panic here but later - this might hapen before console init.
	 */
	if (!hose->io_map_base) {
		printk(KERN_WARNING
		       "registering PCI controller with io_map_base unset\n");
	}
	return;

out:
	printk(KERN_WARNING
	       "Skipping PCI bus scan due to resource conflict\n");
}

/* Most MIPS systems have straight-forward swizzling needs.  */

static inline u8 bridge_swizzle(u8 pin, u8 slot)
{
	return (((pin - 1) + slot) % 4) + 1;
}

extern int __init pcibios_init(void);
subsys_initcall(pcibios_init);

/*
 *  If we set up a device for bus mastering, we need to check the latency
 *  timer as certain crappy BIOSes forget to set it properly.
 */
unsigned int pcibios_max_latency = 255;

void pcibios_set_master(struct pci_dev *dev)
{
	u8 lat;
	pci_read_config_byte(dev, PCI_LATENCY_TIMER, &lat);
	if (lat < 16)
		lat = (64 <= pcibios_max_latency) ? 64 : pcibios_max_latency;
	else if (lat > pcibios_max_latency)
		lat = pcibios_max_latency;
	else
		return;
	printk(KERN_DEBUG "PCI: Setting latency timer of device %s to %d\n",
	       pci_name(dev), lat);
	pci_write_config_byte(dev, PCI_LATENCY_TIMER, lat);
}

unsigned int pcibios_assign_all_busses(void)
{
	return (pci_probe & PCI_ASSIGN_ALL_BUSSES) ? 1 : 0;
}


void __init
pcibios_update_irq(struct pci_dev *dev, int irq)
{
	pci_write_config_byte(dev, PCI_INTERRUPT_LINE, irq);
}

void __devinit
pcibios_resource_to_bus(struct pci_dev *dev, struct pci_bus_region *region,
			 struct resource *res)
{
	struct pci_controller *hose = (struct pci_controller *)dev->sysdata;
	unsigned long offset = 0;

	if (res->flags & IORESOURCE_IO)
		offset = hose->io_offset;
	else if (res->flags & IORESOURCE_MEM)
		offset = hose->mem_offset;

	region->start = res->start - offset;
	region->end = res->end - offset;
}

void __devinit
pcibios_bus_to_resource(struct pci_dev *dev, struct resource *res,
			struct pci_bus_region *region)
{
	struct pci_controller *hose = (struct pci_controller *)dev->sysdata;
	unsigned long offset = 0;

	if (res->flags & IORESOURCE_IO)
		offset = hose->io_offset;
	else if (res->flags & IORESOURCE_MEM)
		offset = hose->mem_offset;

	res->start = region->start + offset;
	res->end = region->end + offset;
}

#ifdef CONFIG_HOTPLUG
EXPORT_SYMBOL(pcibios_resource_to_bus);
EXPORT_SYMBOL(pcibios_bus_to_resource);
EXPORT_SYMBOL(PCIBIOS_MIN_IO);
EXPORT_SYMBOL(PCIBIOS_MIN_MEM);
#endif
