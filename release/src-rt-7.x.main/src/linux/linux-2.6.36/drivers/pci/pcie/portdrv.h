/*
 * File:	portdrv.h
 * Purpose:	PCI Express Port Bus Driver's Internal Data Structures
 *
 * Copyright (C) 2004 Intel
 * Copyright (C) Tom Long Nguyen (tom.l.nguyen@intel.com)
 */

#ifndef _PORTDRV_H_
#define _PORTDRV_H_

#include <linux/compiler.h>

#define PCIE_PORT_DEVICE_MAXSERVICES   4
/*
 * According to the PCI Express Base Specification 2.0, the indices of
 * the MSI-X table entires used by port services must not exceed 31
 */
#define PCIE_PORT_MAX_MSIX_ENTRIES	32

#define get_descriptor_id(type, service) (((type - 4) << 4) | service)

extern bool pcie_ports_disabled;
extern bool pcie_ports_auto;

extern struct bus_type pcie_port_bus_type;
extern int pcie_port_device_register(struct pci_dev *dev);
#ifdef CONFIG_PM
extern int pcie_port_device_suspend(struct device *dev);
extern int pcie_port_device_resume(struct device *dev);
#endif
extern void pcie_port_device_remove(struct pci_dev *dev);
extern int __must_check pcie_port_bus_register(void);
extern void pcie_port_bus_unregister(void);

struct pci_dev;

#ifdef CONFIG_PCIE_PME
extern bool pcie_pme_msi_disabled;

static inline void pcie_pme_disable_msi(void)
{
	pcie_pme_msi_disabled = true;
}

static inline bool pcie_pme_no_msi(void)
{
	return pcie_pme_msi_disabled;
}

extern void pcie_pme_interrupt_enable(struct pci_dev *dev, bool enable);
#else /* !CONFIG_PCIE_PME */
static inline void pcie_pme_disable_msi(void) {}
static inline bool pcie_pme_no_msi(void) { return false; }
static inline void pcie_pme_interrupt_enable(struct pci_dev *dev, bool en) {}
#endif /* !CONFIG_PCIE_PME */

#ifdef CONFIG_ACPI
extern int pcie_port_acpi_setup(struct pci_dev *port, int *mask);

static inline int pcie_port_platform_notify(struct pci_dev *port, int *mask)
{
	return pcie_port_acpi_setup(port, mask);
}
#else /* !CONFIG_ACPI */
static inline int pcie_port_platform_notify(struct pci_dev *port, int *mask)
{
	return 0;
}
#endif /* !CONFIG_ACPI */

#endif /* _PORTDRV_H_ */
