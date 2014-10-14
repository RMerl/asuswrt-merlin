/*
 * Definitions for Device tree / OpenFirmware handling on X86
 *
 * based on arch/powerpc/include/asm/prom.h which is
 *         Copyright (C) 1996-2005 Paul Mackerras.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#ifndef _ASM_X86_PROM_H
#define _ASM_X86_PROM_H
#ifndef __ASSEMBLY__

#include <linux/of.h>
#include <linux/types.h>
#include <linux/pci.h>

#include <asm/irq.h>
#include <asm/atomic.h>
#include <asm/setup.h>
#include <asm/irq_controller.h>

#ifdef CONFIG_OF
extern int of_ioapic;
extern u64 initial_dtb;
extern void add_dtb(u64 data);
extern void x86_add_irq_domains(void);
void __cpuinit x86_of_pci_init(void);
void x86_dtb_init(void);

static inline struct device_node *pci_device_to_OF_node(struct pci_dev *pdev)
{
	return pdev ? pdev->dev.of_node : NULL;
}

static inline struct device_node *pci_bus_to_OF_node(struct pci_bus *bus)
{
	return pci_device_to_OF_node(bus->self);
}

#else
static inline void add_dtb(u64 data) { }
static inline void x86_add_irq_domains(void) { }
static inline void x86_of_pci_init(void) { }
static inline void x86_dtb_init(void) { }
#define of_ioapic 0
#endif

extern char cmd_line[COMMAND_LINE_SIZE];

#define pci_address_to_pio pci_address_to_pio
unsigned long pci_address_to_pio(phys_addr_t addr);

/**
 * irq_dispose_mapping - Unmap an interrupt
 * @virq: linux virq number of the interrupt to unmap
 *
 * FIXME: We really should implement proper virq handling like power,
 * but that's going to be major surgery.
 */
static inline void irq_dispose_mapping(unsigned int virq) { }

#define HAVE_ARCH_DEVTREE_FIXUPS

#endif /* __ASSEMBLY__ */
#endif
