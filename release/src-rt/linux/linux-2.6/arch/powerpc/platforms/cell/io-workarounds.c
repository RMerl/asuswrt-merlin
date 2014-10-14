/*
 * Support PCI IO workaround
 *
 *  Copyright (C) 2006 Benjamin Herrenschmidt <benh@kernel.crashing.org>
 *		       IBM, Corp.
 *  (C) Copyright 2007-2008 TOSHIBA CORPORATION
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#undef DEBUG

#include <linux/kernel.h>

#include <asm/io.h>
#include <asm/machdep.h>
#include <asm/pgtable.h>
#include <asm/ppc-pci.h>

#include "io-workarounds.h"

#define IOWA_MAX_BUS	8

static struct iowa_bus iowa_busses[IOWA_MAX_BUS];
static unsigned int iowa_bus_count;

static struct iowa_bus *iowa_pci_find(unsigned long vaddr, unsigned long paddr)
{
	int i, j;
	struct resource *res;
	unsigned long vstart, vend;

	for (i = 0; i < iowa_bus_count; i++) {
		struct iowa_bus *bus = &iowa_busses[i];
		struct pci_controller *phb = bus->phb;

		if (vaddr) {
			vstart = (unsigned long)phb->io_base_virt;
			vend = vstart + phb->pci_io_size - 1;
			if ((vaddr >= vstart) && (vaddr <= vend))
				return bus;
		}

		if (paddr)
			for (j = 0; j < 3; j++) {
				res = &phb->mem_resources[j];
				if (paddr >= res->start && paddr <= res->end)
					return bus;
			}
	}

	return NULL;
}

struct iowa_bus *iowa_mem_find_bus(const PCI_IO_ADDR addr)
{
	struct iowa_bus *bus;
	int token;

	token = PCI_GET_ADDR_TOKEN(addr);

	if (token && token <= iowa_bus_count)
		bus = &iowa_busses[token - 1];
	else {
		unsigned long vaddr, paddr;
		pte_t *ptep;

		vaddr = (unsigned long)PCI_FIX_ADDR(addr);
		if (vaddr < PHB_IO_BASE || vaddr >= PHB_IO_END)
			return NULL;

		ptep = find_linux_pte(init_mm.pgd, vaddr);
		if (ptep == NULL)
			paddr = 0;
		else
			paddr = pte_pfn(*ptep) << PAGE_SHIFT;
		bus = iowa_pci_find(vaddr, paddr);

		if (bus == NULL)
			return NULL;
	}

	return bus;
}

struct iowa_bus *iowa_pio_find_bus(unsigned long port)
{
	unsigned long vaddr = (unsigned long)pci_io_base + port;
	return iowa_pci_find(vaddr, 0);
}


#define DEF_PCI_AC_RET(name, ret, at, al, space, aa)		\
static ret iowa_##name at					\
{								\
	struct iowa_bus *bus;					\
	bus = iowa_##space##_find_bus(aa);			\
	if (bus && bus->ops && bus->ops->name)			\
		return bus->ops->name al;			\
	return __do_##name al;					\
}

#define DEF_PCI_AC_NORET(name, at, al, space, aa)		\
static void iowa_##name at					\
{								\
	struct iowa_bus *bus;					\
	bus = iowa_##space##_find_bus(aa);			\
	if (bus && bus->ops && bus->ops->name) {		\
		bus->ops->name al;				\
		return;						\
	}							\
	__do_##name al;						\
}

#include <asm/io-defs.h>

#undef DEF_PCI_AC_RET
#undef DEF_PCI_AC_NORET

static const struct ppc_pci_io __devinitconst iowa_pci_io = {

#define DEF_PCI_AC_RET(name, ret, at, al, space, aa)	.name = iowa_##name,
#define DEF_PCI_AC_NORET(name, at, al, space, aa)	.name = iowa_##name,

#include <asm/io-defs.h>

#undef DEF_PCI_AC_RET
#undef DEF_PCI_AC_NORET

};

static void __iomem *iowa_ioremap(phys_addr_t addr, unsigned long size,
				  unsigned long flags, void *caller)
{
	struct iowa_bus *bus;
	void __iomem *res = __ioremap_caller(addr, size, flags, caller);
	int busno;

	bus = iowa_pci_find(0, (unsigned long)addr);
	if (bus != NULL) {
		busno = bus - iowa_busses;
		PCI_SET_ADDR_TOKEN(res, busno + 1);
	}
	return res;
}

/* Regist new bus to support workaround */
void __devinit iowa_register_bus(struct pci_controller *phb,
			struct ppc_pci_io *ops,
			int (*initfunc)(struct iowa_bus *, void *), void *data)
{
	struct iowa_bus *bus;
	struct device_node *np = phb->dn;

	if (iowa_bus_count >= IOWA_MAX_BUS) {
		pr_err("IOWA:Too many pci bridges, "
		       "workarounds disabled for %s\n", np->full_name);
		return;
	}

	bus = &iowa_busses[iowa_bus_count];
	bus->phb = phb;
	bus->ops = ops;

	if (initfunc)
		if ((*initfunc)(bus, data))
			return;

	iowa_bus_count++;

	pr_debug("IOWA:[%d]Add bus, %s.\n", iowa_bus_count-1, np->full_name);
}

/* enable IO workaround */
void __devinit io_workaround_init(void)
{
	static int io_workaround_inited;

	if (io_workaround_inited)
		return;
	ppc_pci_io = iowa_pci_io;
	ppc_md.ioremap = iowa_ioremap;
	io_workaround_inited = 1;
}
