/*
 * Contains common pci routines for ALL ppc platform
 * (based on pci_32.c and pci_64.c)
 *
 * Port for PPC64 David Engebretsen, IBM Corp.
 * Contains common pci routines for ppc64 platform, pSeries and iSeries brands.
 *
 * Copyright (C) 2003 Anton Blanchard <anton@au.ibm.com>, IBM
 *   Rework, based on alpha PCI code.
 *
 * Common pmac/prep/chrp pci routines. -- Cort
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/bootmem.h>
#include <linux/mm.h>
#include <linux/list.h>
#include <linux/syscalls.h>
#include <linux/irq.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_address.h>

#include <asm/processor.h>
#include <asm/io.h>
#include <asm/pci-bridge.h>
#include <asm/byteorder.h>

static DEFINE_SPINLOCK(hose_spinlock);
LIST_HEAD(hose_list);

static int global_phb_number;		/* Global phb counter */

/* ISA Memory physical address */
resource_size_t isa_mem_base;

/* Default PCI flags is 0 on ppc32, modified at boot on ppc64 */
unsigned int pci_flags;

static struct dma_map_ops *pci_dma_ops = &dma_direct_ops;

void set_pci_dma_ops(struct dma_map_ops *dma_ops)
{
	pci_dma_ops = dma_ops;
}

struct dma_map_ops *get_pci_dma_ops(void)
{
	return pci_dma_ops;
}
EXPORT_SYMBOL(get_pci_dma_ops);

int pci_set_dma_mask(struct pci_dev *dev, u64 mask)
{
	return dma_set_mask(&dev->dev, mask);
}

int pci_set_consistent_dma_mask(struct pci_dev *dev, u64 mask)
{
	int rc;

	rc = dma_set_mask(&dev->dev, mask);
	dev->dev.coherent_dma_mask = dev->dma_mask;

	return rc;
}

struct pci_controller *pcibios_alloc_controller(struct device_node *dev)
{
	struct pci_controller *phb;

	phb = zalloc_maybe_bootmem(sizeof(struct pci_controller), GFP_KERNEL);
	if (!phb)
		return NULL;
	spin_lock(&hose_spinlock);
	phb->global_number = global_phb_number++;
	list_add_tail(&phb->list_node, &hose_list);
	spin_unlock(&hose_spinlock);
	phb->dn = dev;
	phb->is_dynamic = mem_init_done;
	return phb;
}

void pcibios_free_controller(struct pci_controller *phb)
{
	spin_lock(&hose_spinlock);
	list_del(&phb->list_node);
	spin_unlock(&hose_spinlock);

	if (phb->is_dynamic)
		kfree(phb);
}

static resource_size_t pcibios_io_size(const struct pci_controller *hose)
{
	return hose->io_resource.end - hose->io_resource.start + 1;
}

int pcibios_vaddr_is_ioport(void __iomem *address)
{
	int ret = 0;
	struct pci_controller *hose;
	resource_size_t size;

	spin_lock(&hose_spinlock);
	list_for_each_entry(hose, &hose_list, list_node) {
		size = pcibios_io_size(hose);
		if (address >= hose->io_base_virt &&
		    address < (hose->io_base_virt + size)) {
			ret = 1;
			break;
		}
	}
	spin_unlock(&hose_spinlock);
	return ret;
}

unsigned long pci_address_to_pio(phys_addr_t address)
{
	struct pci_controller *hose;
	resource_size_t size;
	unsigned long ret = ~0;

	spin_lock(&hose_spinlock);
	list_for_each_entry(hose, &hose_list, list_node) {
		size = pcibios_io_size(hose);
		if (address >= hose->io_base_phys &&
		    address < (hose->io_base_phys + size)) {
			unsigned long base =
				(unsigned long)hose->io_base_virt - _IO_BASE;
			ret = base + (address - hose->io_base_phys);
			break;
		}
	}
	spin_unlock(&hose_spinlock);

	return ret;
}
EXPORT_SYMBOL_GPL(pci_address_to_pio);

/*
 * Return the domain number for this bus.
 */
int pci_domain_nr(struct pci_bus *bus)
{
	struct pci_controller *hose = pci_bus_to_host(bus);

	return hose->global_number;
}
EXPORT_SYMBOL(pci_domain_nr);

/* This routine is meant to be used early during boot, when the
 * PCI bus numbers have not yet been assigned, and you need to
 * issue PCI config cycles to an OF device.
 * It could also be used to "fix" RTAS config cycles if you want
 * to set pci_assign_all_buses to 1 and still use RTAS for PCI
 * config cycles.
 */
struct pci_controller *pci_find_hose_for_OF_device(struct device_node *node)
{
	while (node) {
		struct pci_controller *hose, *tmp;
		list_for_each_entry_safe(hose, tmp, &hose_list, list_node)
			if (hose->dn == node)
				return hose;
		node = node->parent;
	}
	return NULL;
}

static ssize_t pci_show_devspec(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct pci_dev *pdev;
	struct device_node *np;

	pdev = to_pci_dev(dev);
	np = pci_device_to_OF_node(pdev);
	if (np == NULL || np->full_name == NULL)
		return 0;
	return sprintf(buf, "%s", np->full_name);
}
static DEVICE_ATTR(devspec, S_IRUGO, pci_show_devspec, NULL);

/* Add sysfs properties */
int pcibios_add_platform_entries(struct pci_dev *pdev)
{
	return device_create_file(&pdev->dev, &dev_attr_devspec);
}

char __devinit *pcibios_setup(char *str)
{
	return str;
}

/*
 * Reads the interrupt pin to determine if interrupt is use by card.
 * If the interrupt is used, then gets the interrupt line from the
 * openfirmware and sets it in the pci_dev and pci_config line.
 */
int pci_read_irq_line(struct pci_dev *pci_dev)
{
	struct of_irq oirq;
	unsigned int virq;

	/* The current device-tree that iSeries generates from the HV
	 * PCI informations doesn't contain proper interrupt routing,
	 * and all the fallback would do is print out crap, so we
	 * don't attempt to resolve the interrupts here at all, some
	 * iSeries specific fixup does it.
	 *
	 * In the long run, we will hopefully fix the generated device-tree
	 * instead.
	 */
	pr_debug("PCI: Try to map irq for %s...\n", pci_name(pci_dev));

#ifdef DEBUG
	memset(&oirq, 0xff, sizeof(oirq));
#endif
	/* Try to get a mapping from the device-tree */
	if (of_irq_map_pci(pci_dev, &oirq)) {
		u8 line, pin;

		/* If that fails, lets fallback to what is in the config
		 * space and map that through the default controller. We
		 * also set the type to level low since that's what PCI
		 * interrupts are. If your platform does differently, then
		 * either provide a proper interrupt tree or don't use this
		 * function.
		 */
		if (pci_read_config_byte(pci_dev, PCI_INTERRUPT_PIN, &pin))
			return -1;
		if (pin == 0)
			return -1;
		if (pci_read_config_byte(pci_dev, PCI_INTERRUPT_LINE, &line) ||
		    line == 0xff || line == 0) {
			return -1;
		}
		pr_debug(" No map ! Using line %d (pin %d) from PCI config\n",
			 line, pin);

		virq = irq_create_mapping(NULL, line);
		if (virq != NO_IRQ)
			set_irq_type(virq, IRQ_TYPE_LEVEL_LOW);
	} else {
		pr_debug(" Got one, spec %d cells (0x%08x 0x%08x...) on %s\n",
			 oirq.size, oirq.specifier[0], oirq.specifier[1],
			 oirq.controller ? oirq.controller->full_name :
			 "<default>");

		virq = irq_create_of_mapping(oirq.controller, oirq.specifier,
					     oirq.size);
	}
	if (virq == NO_IRQ) {
		pr_debug(" Failed to map !\n");
		return -1;
	}

	pr_debug(" Mapped to linux irq %d\n", virq);

	pci_dev->irq = virq;

	return 0;
}
EXPORT_SYMBOL(pci_read_irq_line);

/*
 * Platform support for /proc/bus/pci/X/Y mmap()s,
 * modelled on the sparc64 implementation by Dave Miller.
 *  -- paulus.
 */

static struct resource *__pci_mmap_make_offset(struct pci_dev *dev,
					       resource_size_t *offset,
					       enum pci_mmap_state mmap_state)
{
	struct pci_controller *hose = pci_bus_to_host(dev->bus);
	unsigned long io_offset = 0;
	int i, res_bit;

	if (hose == 0)
		return NULL;		/* should never happen */

	/* If memory, add on the PCI bridge address offset */
	if (mmap_state == pci_mmap_mem) {
		res_bit = IORESOURCE_MEM;
	} else {
		io_offset = (unsigned long)hose->io_base_virt - _IO_BASE;
		*offset += io_offset;
		res_bit = IORESOURCE_IO;
	}

	/*
	 * Check that the offset requested corresponds to one of the
	 * resources of the device.
	 */
	for (i = 0; i <= PCI_ROM_RESOURCE; i++) {
		struct resource *rp = &dev->resource[i];
		int flags = rp->flags;

		/* treat ROM as memory (should be already) */
		if (i == PCI_ROM_RESOURCE)
			flags |= IORESOURCE_MEM;

		/* Active and same type? */
		if ((flags & res_bit) == 0)
			continue;

		/* In the range of this resource? */
		if (*offset < (rp->start & PAGE_MASK) || *offset > rp->end)
			continue;

		/* found it! construct the final physical address */
		if (mmap_state == pci_mmap_io)
			*offset += hose->io_base_phys - io_offset;
		return rp;
	}

	return NULL;
}

/*
 * Set vm_page_prot of VMA, as appropriate for this architecture, for a pci
 * device mapping.
 */
static pgprot_t __pci_mmap_set_pgprot(struct pci_dev *dev, struct resource *rp,
				      pgprot_t protection,
				      enum pci_mmap_state mmap_state,
				      int write_combine)
{
	pgprot_t prot = protection;

	if (mmap_state != pci_mmap_mem)
		write_combine = 0;
	else if (write_combine == 0) {
		if (rp->flags & IORESOURCE_PREFETCH)
			write_combine = 1;
	}

	return pgprot_noncached(prot);
}

/*
 * This one is used by /dev/mem and fbdev who have no clue about the
 * PCI device, it tries to find the PCI device first and calls the
 * above routine
 */
pgprot_t pci_phys_mem_access_prot(struct file *file,
				  unsigned long pfn,
				  unsigned long size,
				  pgprot_t prot)
{
	struct pci_dev *pdev = NULL;
	struct resource *found = NULL;
	resource_size_t offset = ((resource_size_t)pfn) << PAGE_SHIFT;
	int i;

	if (page_is_ram(pfn))
		return prot;

	prot = pgprot_noncached(prot);
	for_each_pci_dev(pdev) {
		for (i = 0; i <= PCI_ROM_RESOURCE; i++) {
			struct resource *rp = &pdev->resource[i];
			int flags = rp->flags;

			/* Active and same type? */
			if ((flags & IORESOURCE_MEM) == 0)
				continue;
			/* In the range of this resource? */
			if (offset < (rp->start & PAGE_MASK) ||
			    offset > rp->end)
				continue;
			found = rp;
			break;
		}
		if (found)
			break;
	}
	if (found) {
		if (found->flags & IORESOURCE_PREFETCH)
			prot = pgprot_noncached_wc(prot);
		pci_dev_put(pdev);
	}

	pr_debug("PCI: Non-PCI map for %llx, prot: %lx\n",
		 (unsigned long long)offset, pgprot_val(prot));

	return prot;
}

/*
 * Perform the actual remap of the pages for a PCI device mapping, as
 * appropriate for this architecture.  The region in the process to map
 * is described by vm_start and vm_end members of VMA, the base physical
 * address is found in vm_pgoff.
 * The pci device structure is provided so that architectures may make mapping
 * decisions on a per-device or per-bus basis.
 *
 * Returns a negative error code on failure, zero on success.
 */
int pci_mmap_page_range(struct pci_dev *dev, struct vm_area_struct *vma,
			enum pci_mmap_state mmap_state, int write_combine)
{
	resource_size_t offset =
		((resource_size_t)vma->vm_pgoff) << PAGE_SHIFT;
	struct resource *rp;
	int ret;

	rp = __pci_mmap_make_offset(dev, &offset, mmap_state);
	if (rp == NULL)
		return -EINVAL;

	vma->vm_pgoff = offset >> PAGE_SHIFT;
	vma->vm_page_prot = __pci_mmap_set_pgprot(dev, rp,
						  vma->vm_page_prot,
						  mmap_state, write_combine);

	ret = remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,
			       vma->vm_end - vma->vm_start, vma->vm_page_prot);

	return ret;
}

/* This provides legacy IO read access on a bus */
int pci_legacy_read(struct pci_bus *bus, loff_t port, u32 *val, size_t size)
{
	unsigned long offset;
	struct pci_controller *hose = pci_bus_to_host(bus);
	struct resource *rp = &hose->io_resource;
	void __iomem *addr;

	/* Check if port can be supported by that bus. We only check
	 * the ranges of the PHB though, not the bus itself as the rules
	 * for forwarding legacy cycles down bridges are not our problem
	 * here. So if the host bridge supports it, we do it.
	 */
	offset = (unsigned long)hose->io_base_virt - _IO_BASE;
	offset += port;

	if (!(rp->flags & IORESOURCE_IO))
		return -ENXIO;
	if (offset < rp->start || (offset + size) > rp->end)
		return -ENXIO;
	addr = hose->io_base_virt + port;

	switch (size) {
	case 1:
		*((u8 *)val) = in_8(addr);
		return 1;
	case 2:
		if (port & 1)
			return -EINVAL;
		*((u16 *)val) = in_le16(addr);
		return 2;
	case 4:
		if (port & 3)
			return -EINVAL;
		*((u32 *)val) = in_le32(addr);
		return 4;
	}
	return -EINVAL;
}

/* This provides legacy IO write access on a bus */
int pci_legacy_write(struct pci_bus *bus, loff_t port, u32 val, size_t size)
{
	unsigned long offset;
	struct pci_controller *hose = pci_bus_to_host(bus);
	struct resource *rp = &hose->io_resource;
	void __iomem *addr;

	/* Check if port can be supported by that bus. We only check
	 * the ranges of the PHB though, not the bus itself as the rules
	 * for forwarding legacy cycles down bridges are not our problem
	 * here. So if the host bridge supports it, we do it.
	 */
	offset = (unsigned long)hose->io_base_virt - _IO_BASE;
	offset += port;

	if (!(rp->flags & IORESOURCE_IO))
		return -ENXIO;
	if (offset < rp->start || (offset + size) > rp->end)
		return -ENXIO;
	addr = hose->io_base_virt + port;

	/* WARNING: The generic code is idiotic. It gets passed a pointer
	 * to what can be a 1, 2 or 4 byte quantity and always reads that
	 * as a u32, which means that we have to correct the location of
	 * the data read within those 32 bits for size 1 and 2
	 */
	switch (size) {
	case 1:
		out_8(addr, val >> 24);
		return 1;
	case 2:
		if (port & 1)
			return -EINVAL;
		out_le16(addr, val >> 16);
		return 2;
	case 4:
		if (port & 3)
			return -EINVAL;
		out_le32(addr, val);
		return 4;
	}
	return -EINVAL;
}

/* This provides legacy IO or memory mmap access on a bus */
int pci_mmap_legacy_page_range(struct pci_bus *bus,
			       struct vm_area_struct *vma,
			       enum pci_mmap_state mmap_state)
{
	struct pci_controller *hose = pci_bus_to_host(bus);
	resource_size_t offset =
		((resource_size_t)vma->vm_pgoff) << PAGE_SHIFT;
	resource_size_t size = vma->vm_end - vma->vm_start;
	struct resource *rp;

	pr_debug("pci_mmap_legacy_page_range(%04x:%02x, %s @%llx..%llx)\n",
		 pci_domain_nr(bus), bus->number,
		 mmap_state == pci_mmap_mem ? "MEM" : "IO",
		 (unsigned long long)offset,
		 (unsigned long long)(offset + size - 1));

	if (mmap_state == pci_mmap_mem) {
		/* Hack alert !
		 *
		 * Because X is lame and can fail starting if it gets an error
		 * trying to mmap legacy_mem (instead of just moving on without
		 * legacy memory access) we fake it here by giving it anonymous
		 * memory, effectively behaving just like /dev/zero
		 */
		if ((offset + size) > hose->isa_mem_size) {
#ifdef CONFIG_MMU
			printk(KERN_DEBUG
				"Process %s (pid:%d) mapped non-existing PCI"
				"legacy memory for 0%04x:%02x\n",
				current->comm, current->pid, pci_domain_nr(bus),
								bus->number);
#endif
			if (vma->vm_flags & VM_SHARED)
				return shmem_zero_setup(vma);
			return 0;
		}
		offset += hose->isa_mem_phys;
	} else {
		unsigned long io_offset = (unsigned long)hose->io_base_virt - \
								_IO_BASE;
		unsigned long roffset = offset + io_offset;
		rp = &hose->io_resource;
		if (!(rp->flags & IORESOURCE_IO))
			return -ENXIO;
		if (roffset < rp->start || (roffset + size) > rp->end)
			return -ENXIO;
		offset += hose->io_base_phys;
	}
	pr_debug(" -> mapping phys %llx\n", (unsigned long long)offset);

	vma->vm_pgoff = offset >> PAGE_SHIFT;
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	return remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,
			       vma->vm_end - vma->vm_start,
			       vma->vm_page_prot);
}

void pci_resource_to_user(const struct pci_dev *dev, int bar,
			  const struct resource *rsrc,
			  resource_size_t *start, resource_size_t *end)
{
	struct pci_controller *hose = pci_bus_to_host(dev->bus);
	resource_size_t offset = 0;

	if (hose == NULL)
		return;

	if (rsrc->flags & IORESOURCE_IO)
		offset = (unsigned long)hose->io_base_virt - _IO_BASE;

	/* We pass a fully fixed up address to userland for MMIO instead of
	 * a BAR value because X is lame and expects to be able to use that
	 * to pass to /dev/mem !
	 *
	 * That means that we'll have potentially 64 bits values where some
	 * userland apps only expect 32 (like X itself since it thinks only
	 * Sparc has 64 bits MMIO) but if we don't do that, we break it on
	 * 32 bits CHRPs :-(
	 *
	 * Hopefully, the sysfs insterface is immune to that gunk. Once X
	 * has been fixed (and the fix spread enough), we can re-enable the
	 * 2 lines below and pass down a BAR value to userland. In that case
	 * we'll also have to re-enable the matching code in
	 * __pci_mmap_make_offset().
	 *
	 * BenH.
	 */

	*start = rsrc->start - offset;
	*end = rsrc->end - offset;
}

/**
 * pci_process_bridge_OF_ranges - Parse PCI bridge resources from device tree
 * @hose: newly allocated pci_controller to be setup
 * @dev: device node of the host bridge
 * @primary: set if primary bus (32 bits only, soon to be deprecated)
 *
 * This function will parse the "ranges" property of a PCI host bridge device
 * node and setup the resource mapping of a pci controller based on its
 * content.
 *
 * Life would be boring if it wasn't for a few issues that we have to deal
 * with here:
 *
 *   - We can only cope with one IO space range and up to 3 Memory space
 *     ranges. However, some machines (thanks Apple !) tend to split their
 *     space into lots of small contiguous ranges. So we have to coalesce.
 *
 *   - We can only cope with all memory ranges having the same offset
 *     between CPU addresses and PCI addresses. Unfortunately, some bridges
 *     are setup for a large 1:1 mapping along with a small "window" which
 *     maps PCI address 0 to some arbitrary high address of the CPU space in
 *     order to give access to the ISA memory hole.
 *     The way out of here that I've chosen for now is to always set the
 *     offset based on the first resource found, then override it if we
 *     have a different offset and the previous was set by an ISA hole.
 *
 *   - Some busses have IO space not starting at 0, which causes trouble with
 *     the way we do our IO resource renumbering. The code somewhat deals with
 *     it for 64 bits but I would expect problems on 32 bits.
 *
 *   - Some 32 bits platforms such as 4xx can have physical space larger than
 *     32 bits so we need to use 64 bits values for the parsing
 */
void __devinit pci_process_bridge_OF_ranges(struct pci_controller *hose,
					    struct device_node *dev,
					    int primary)
{
	const u32 *ranges;
	int rlen;
	int pna = of_n_addr_cells(dev);
	int np = pna + 5;
	int memno = 0, isa_hole = -1;
	u32 pci_space;
	unsigned long long pci_addr, cpu_addr, pci_next, cpu_next, size;
	unsigned long long isa_mb = 0;
	struct resource *res;

	printk(KERN_INFO "PCI host bridge %s %s ranges:\n",
	       dev->full_name, primary ? "(primary)" : "");

	/* Get ranges property */
	ranges = of_get_property(dev, "ranges", &rlen);
	if (ranges == NULL)
		return;

	/* Parse it */
	pr_debug("Parsing ranges property...\n");
	while ((rlen -= np * 4) >= 0) {
		/* Read next ranges element */
		pci_space = ranges[0];
		pci_addr = of_read_number(ranges + 1, 2);
		cpu_addr = of_translate_address(dev, ranges + 3);
		size = of_read_number(ranges + pna + 3, 2);

		pr_debug("pci_space: 0x%08x pci_addr:0x%016llx "
				"cpu_addr:0x%016llx size:0x%016llx\n",
					pci_space, pci_addr, cpu_addr, size);

		ranges += np;

		/* If we failed translation or got a zero-sized region
		 * (some FW try to feed us with non sensical zero sized regions
		 * such as power3 which look like some kind of attempt
		 * at exposing the VGA memory hole)
		 */
		if (cpu_addr == OF_BAD_ADDR || size == 0)
			continue;

		/* Now consume following elements while they are contiguous */
		for (; rlen >= np * sizeof(u32);
		     ranges += np, rlen -= np * 4) {
			if (ranges[0] != pci_space)
				break;
			pci_next = of_read_number(ranges + 1, 2);
			cpu_next = of_translate_address(dev, ranges + 3);
			if (pci_next != pci_addr + size ||
			    cpu_next != cpu_addr + size)
				break;
			size += of_read_number(ranges + pna + 3, 2);
		}

		/* Act based on address space type */
		res = NULL;
		switch ((pci_space >> 24) & 0x3) {
		case 1:		/* PCI IO space */
			printk(KERN_INFO
			       "  IO 0x%016llx..0x%016llx -> 0x%016llx\n",
			       cpu_addr, cpu_addr + size - 1, pci_addr);

			/* We support only one IO range */
			if (hose->pci_io_size) {
				printk(KERN_INFO
				       " \\--> Skipped (too many) !\n");
				continue;
			}
			/* On 32 bits, limit I/O space to 16MB */
			if (size > 0x01000000)
				size = 0x01000000;

			/* 32 bits needs to map IOs here */
			hose->io_base_virt = ioremap(cpu_addr, size);

			/* Expect trouble if pci_addr is not 0 */
			if (primary)
				isa_io_base =
					(unsigned long)hose->io_base_virt;
			/* pci_io_size and io_base_phys always represent IO
			 * space starting at 0 so we factor in pci_addr
			 */
			hose->pci_io_size = pci_addr + size;
			hose->io_base_phys = cpu_addr - pci_addr;

			/* Build resource */
			res = &hose->io_resource;
			res->flags = IORESOURCE_IO;
			res->start = pci_addr;
			break;
		case 2:		/* PCI Memory space */
		case 3:		/* PCI 64 bits Memory space */
			printk(KERN_INFO
			       " MEM 0x%016llx..0x%016llx -> 0x%016llx %s\n",
			       cpu_addr, cpu_addr + size - 1, pci_addr,
			       (pci_space & 0x40000000) ? "Prefetch" : "");

			/* We support only 3 memory ranges */
			if (memno >= 3) {
				printk(KERN_INFO
				       " \\--> Skipped (too many) !\n");
				continue;
			}
			/* Handles ISA memory hole space here */
			if (pci_addr == 0) {
				isa_mb = cpu_addr;
				isa_hole = memno;
				if (primary || isa_mem_base == 0)
					isa_mem_base = cpu_addr;
				hose->isa_mem_phys = cpu_addr;
				hose->isa_mem_size = size;
			}

			/* We get the PCI/Mem offset from the first range or
			 * the, current one if the offset came from an ISA
			 * hole. If they don't match, bugger.
			 */
			if (memno == 0 ||
			    (isa_hole >= 0 && pci_addr != 0 &&
			     hose->pci_mem_offset == isa_mb))
				hose->pci_mem_offset = cpu_addr - pci_addr;
			else if (pci_addr != 0 &&
				 hose->pci_mem_offset != cpu_addr - pci_addr) {
				printk(KERN_INFO
				       " \\--> Skipped (offset mismatch) !\n");
				continue;
			}

			/* Build resource */
			res = &hose->mem_resources[memno++];
			res->flags = IORESOURCE_MEM;
			if (pci_space & 0x40000000)
				res->flags |= IORESOURCE_PREFETCH;
			res->start = cpu_addr;
			break;
		}
		if (res != NULL) {
			res->name = dev->full_name;
			res->end = res->start + size - 1;
			res->parent = NULL;
			res->sibling = NULL;
			res->child = NULL;
		}
	}

	/* If there's an ISA hole and the pci_mem_offset is -not- matching
	 * the ISA hole offset, then we need to remove the ISA hole from
	 * the resource list for that brige
	 */
	if (isa_hole >= 0 && hose->pci_mem_offset != isa_mb) {
		unsigned int next = isa_hole + 1;
		printk(KERN_INFO " Removing ISA hole at 0x%016llx\n", isa_mb);
		if (next < memno)
			memmove(&hose->mem_resources[isa_hole],
				&hose->mem_resources[next],
				sizeof(struct resource) * (memno - next));
		hose->mem_resources[--memno].flags = 0;
	}
}

/* Decide whether to display the domain number in /proc */
int pci_proc_domain(struct pci_bus *bus)
{
	struct pci_controller *hose = pci_bus_to_host(bus);

	if (!(pci_flags & PCI_ENABLE_PROC_DOMAINS))
		return 0;
	if (pci_flags & PCI_COMPAT_DOMAIN_0)
		return hose->global_number != 0;
	return 1;
}

void pcibios_resource_to_bus(struct pci_dev *dev, struct pci_bus_region *region,
			     struct resource *res)
{
	resource_size_t offset = 0, mask = (resource_size_t)-1;
	struct pci_controller *hose = pci_bus_to_host(dev->bus);

	if (!hose)
		return;
	if (res->flags & IORESOURCE_IO) {
		offset = (unsigned long)hose->io_base_virt - _IO_BASE;
		mask = 0xffffffffu;
	} else if (res->flags & IORESOURCE_MEM)
		offset = hose->pci_mem_offset;

	region->start = (res->start - offset) & mask;
	region->end = (res->end - offset) & mask;
}
EXPORT_SYMBOL(pcibios_resource_to_bus);

void pcibios_bus_to_resource(struct pci_dev *dev, struct resource *res,
			     struct pci_bus_region *region)
{
	resource_size_t offset = 0, mask = (resource_size_t)-1;
	struct pci_controller *hose = pci_bus_to_host(dev->bus);

	if (!hose)
		return;
	if (res->flags & IORESOURCE_IO) {
		offset = (unsigned long)hose->io_base_virt - _IO_BASE;
		mask = 0xffffffffu;
	} else if (res->flags & IORESOURCE_MEM)
		offset = hose->pci_mem_offset;
	res->start = (region->start + offset) & mask;
	res->end = (region->end + offset) & mask;
}
EXPORT_SYMBOL(pcibios_bus_to_resource);

/* Fixup a bus resource into a linux resource */
static void __devinit fixup_resource(struct resource *res, struct pci_dev *dev)
{
	struct pci_controller *hose = pci_bus_to_host(dev->bus);
	resource_size_t offset = 0, mask = (resource_size_t)-1;

	if (res->flags & IORESOURCE_IO) {
		offset = (unsigned long)hose->io_base_virt - _IO_BASE;
		mask = 0xffffffffu;
	} else if (res->flags & IORESOURCE_MEM)
		offset = hose->pci_mem_offset;

	res->start = (res->start + offset) & mask;
	res->end = (res->end + offset) & mask;
}

/* This header fixup will do the resource fixup for all devices as they are
 * probed, but not for bridge ranges
 */
static void __devinit pcibios_fixup_resources(struct pci_dev *dev)
{
	struct pci_controller *hose = pci_bus_to_host(dev->bus);
	int i;

	if (!hose) {
		printk(KERN_ERR "No host bridge for PCI dev %s !\n",
		       pci_name(dev));
		return;
	}
	for (i = 0; i < DEVICE_COUNT_RESOURCE; i++) {
		struct resource *res = dev->resource + i;
		if (!res->flags)
			continue;
		/* On platforms that have PCI_PROBE_ONLY set, we don't
		 * consider 0 as an unassigned BAR value. It's technically
		 * a valid value, but linux doesn't like it... so when we can
		 * re-assign things, we do so, but if we can't, we keep it
		 * around and hope for the best...
		 */
		if (res->start == 0 && !(pci_flags & PCI_PROBE_ONLY)) {
			pr_debug("PCI:%s Resource %d %016llx-%016llx [%x]" \
							"is unassigned\n",
				 pci_name(dev), i,
				 (unsigned long long)res->start,
				 (unsigned long long)res->end,
				 (unsigned int)res->flags);
			res->end -= res->start;
			res->start = 0;
			res->flags |= IORESOURCE_UNSET;
			continue;
		}

		pr_debug("PCI:%s Resource %d %016llx-%016llx [%x] fixup...\n",
			 pci_name(dev), i,
			 (unsigned long long)res->start,\
			 (unsigned long long)res->end,
			 (unsigned int)res->flags);

		fixup_resource(res, dev);

		pr_debug("PCI:%s            %016llx-%016llx\n",
			 pci_name(dev),
			 (unsigned long long)res->start,
			 (unsigned long long)res->end);
	}
}
DECLARE_PCI_FIXUP_HEADER(PCI_ANY_ID, PCI_ANY_ID, pcibios_fixup_resources);

/* This function tries to figure out if a bridge resource has been initialized
 * by the firmware or not. It doesn't have to be absolutely bullet proof, but
 * things go more smoothly when it gets it right. It should covers cases such
 * as Apple "closed" bridge resources and bare-metal pSeries unassigned bridges
 */
static int __devinit pcibios_uninitialized_bridge_resource(struct pci_bus *bus,
							   struct resource *res)
{
	struct pci_controller *hose = pci_bus_to_host(bus);
	struct pci_dev *dev = bus->self;
	resource_size_t offset;
	u16 command;
	int i;

	/* We don't do anything if PCI_PROBE_ONLY is set */
	if (pci_flags & PCI_PROBE_ONLY)
		return 0;

	/* Job is a bit different between memory and IO */
	if (res->flags & IORESOURCE_MEM) {
		/* If the BAR is non-0 (res != pci_mem_offset) then it's
		 * probably been initialized by somebody
		 */
		if (res->start != hose->pci_mem_offset)
			return 0;

		/* The BAR is 0, let's check if memory decoding is enabled on
		 * the bridge. If not, we consider it unassigned
		 */
		pci_read_config_word(dev, PCI_COMMAND, &command);
		if ((command & PCI_COMMAND_MEMORY) == 0)
			return 1;

		/* Memory decoding is enabled and the BAR is 0. If any of
		 * the bridge resources covers that starting address (0 then
		 * it's good enough for us for memory
		 */
		for (i = 0; i < 3; i++) {
			if ((hose->mem_resources[i].flags & IORESOURCE_MEM) &&
			   hose->mem_resources[i].start == hose->pci_mem_offset)
				return 0;
		}

		/* Well, it starts at 0 and we know it will collide so we may as
		 * well consider it as unassigned. That covers the Apple case.
		 */
		return 1;
	} else {
		/* If the BAR is non-0, then we consider it assigned */
		offset = (unsigned long)hose->io_base_virt - _IO_BASE;
		if (((res->start - offset) & 0xfffffffful) != 0)
			return 0;

		/* Here, we are a bit different than memory as typically IO
		 * space starting at low addresses -is- valid. What we do
		 * instead if that we consider as unassigned anything that
		 * doesn't have IO enabled in the PCI command register,
		 * and that's it.
		 */
		pci_read_config_word(dev, PCI_COMMAND, &command);
		if (command & PCI_COMMAND_IO)
			return 0;

		/* It's starting at 0 and IO is disabled in the bridge, consider
		 * it unassigned
		 */
		return 1;
	}
}

/* Fixup resources of a PCI<->PCI bridge */
static void __devinit pcibios_fixup_bridge(struct pci_bus *bus)
{
	struct resource *res;
	int i;

	struct pci_dev *dev = bus->self;

	pci_bus_for_each_resource(bus, res, i) {
		res = bus->resource[i];
		if (!res)
			continue;
		if (!res->flags)
			continue;
		if (i >= 3 && bus->self->transparent)
			continue;

		pr_debug("PCI:%s Bus rsrc %d %016llx-%016llx [%x] fixup...\n",
			 pci_name(dev), i,
			 (unsigned long long)res->start,\
			 (unsigned long long)res->end,
			 (unsigned int)res->flags);

		/* Perform fixup */
		fixup_resource(res, dev);

		/* Try to detect uninitialized P2P bridge resources,
		 * and clear them out so they get re-assigned later
		 */
		if (pcibios_uninitialized_bridge_resource(bus, res)) {
			res->flags = 0;
			pr_debug("PCI:%s            (unassigned)\n",
								pci_name(dev));
		} else {
			pr_debug("PCI:%s            %016llx-%016llx\n",
				 pci_name(dev),
				 (unsigned long long)res->start,
				 (unsigned long long)res->end);
		}
	}
}

void __devinit pcibios_setup_bus_self(struct pci_bus *bus)
{
	/* Fix up the bus resources for P2P bridges */
	if (bus->self != NULL)
		pcibios_fixup_bridge(bus);
}

void __devinit pcibios_setup_bus_devices(struct pci_bus *bus)
{
	struct pci_dev *dev;

	pr_debug("PCI: Fixup bus devices %d (%s)\n",
		 bus->number, bus->self ? pci_name(bus->self) : "PHB");

	list_for_each_entry(dev, &bus->devices, bus_list) {
		struct dev_archdata *sd = &dev->dev.archdata;

		/* Setup OF node pointer in archdata */
		dev->dev.of_node = pci_device_to_OF_node(dev);

		/* Fixup NUMA node as it may not be setup yet by the generic
		 * code and is needed by the DMA init
		 */
		set_dev_node(&dev->dev, pcibus_to_node(dev->bus));

		/* Hook up default DMA ops */
		sd->dma_ops = pci_dma_ops;
		sd->dma_data = (void *)PCI_DRAM_OFFSET;

		/* Read default IRQs and fixup if necessary */
		pci_read_irq_line(dev);
	}
}

void __devinit pcibios_fixup_bus(struct pci_bus *bus)
{
	/* When called from the generic PCI probe, read PCI<->PCI bridge
	 * bases. This is -not- called when generating the PCI tree from
	 * the OF device-tree.
	 */
	if (bus->self != NULL)
		pci_read_bridge_bases(bus);

	/* Now fixup the bus bus */
	pcibios_setup_bus_self(bus);

	/* Now fixup devices on that bus */
	pcibios_setup_bus_devices(bus);
}
EXPORT_SYMBOL(pcibios_fixup_bus);

static int skip_isa_ioresource_align(struct pci_dev *dev)
{
	if ((pci_flags & PCI_CAN_SKIP_ISA_ALIGN) &&
	    !(dev->bus->bridge_ctl & PCI_BRIDGE_CTL_ISA))
		return 1;
	return 0;
}

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
resource_size_t pcibios_align_resource(void *data, const struct resource *res,
				resource_size_t size, resource_size_t align)
{
	struct pci_dev *dev = data;
	resource_size_t start = res->start;

	if (res->flags & IORESOURCE_IO) {
		if (skip_isa_ioresource_align(dev))
			return start;
		if (start & 0x300)
			start = (start + 0x3ff) & ~0x3ff;
	}

	return start;
}
EXPORT_SYMBOL(pcibios_align_resource);

/*
 * Reparent resource children of pr that conflict with res
 * under res, and make res replace those children.
 */
static int __init reparent_resources(struct resource *parent,
				     struct resource *res)
{
	struct resource *p, **pp;
	struct resource **firstpp = NULL;

	for (pp = &parent->child; (p = *pp) != NULL; pp = &p->sibling) {
		if (p->end < res->start)
			continue;
		if (res->end < p->start)
			break;
		if (p->start < res->start || p->end > res->end)
			return -1;	/* not completely contained */
		if (firstpp == NULL)
			firstpp = pp;
	}
	if (firstpp == NULL)
		return -1;	/* didn't find any conflicting entries? */
	res->parent = parent;
	res->child = *firstpp;
	res->sibling = *pp;
	*firstpp = res;
	*pp = NULL;
	for (p = res->child; p != NULL; p = p->sibling) {
		p->parent = res;
		pr_debug("PCI: Reparented %s [%llx..%llx] under %s\n",
			 p->name,
			 (unsigned long long)p->start,
			 (unsigned long long)p->end, res->name);
	}
	return 0;
}


void pcibios_allocate_bus_resources(struct pci_bus *bus)
{
	struct pci_bus *b;
	int i;
	struct resource *res, *pr;

	pr_debug("PCI: Allocating bus resources for %04x:%02x...\n",
		 pci_domain_nr(bus), bus->number);

	pci_bus_for_each_resource(bus, res, i) {
		res = bus->resource[i];
		if (!res || !res->flags
		    || res->start > res->end || res->parent)
			continue;
		if (bus->parent == NULL)
			pr = (res->flags & IORESOURCE_IO) ?
				&ioport_resource : &iomem_resource;
		else {
			/* Don't bother with non-root busses when
			 * re-assigning all resources. We clear the
			 * resource flags as if they were colliding
			 * and as such ensure proper re-allocation
			 * later.
			 */
			if (pci_flags & PCI_REASSIGN_ALL_RSRC)
				goto clear_resource;
			pr = pci_find_parent_resource(bus->self, res);
			if (pr == res) {
				/* this happens when the generic PCI
				 * code (wrongly) decides that this
				 * bridge is transparent  -- paulus
				 */
				continue;
			}
		}

		pr_debug("PCI: %s (bus %d) bridge rsrc %d: %016llx-%016llx "
			 "[0x%x], parent %p (%s)\n",
			 bus->self ? pci_name(bus->self) : "PHB",
			 bus->number, i,
			 (unsigned long long)res->start,
			 (unsigned long long)res->end,
			 (unsigned int)res->flags,
			 pr, (pr && pr->name) ? pr->name : "nil");

		if (pr && !(pr->flags & IORESOURCE_UNSET)) {
			if (request_resource(pr, res) == 0)
				continue;
			/*
			 * Must be a conflict with an existing entry.
			 * Move that entry (or entries) under the
			 * bridge resource and try again.
			 */
			if (reparent_resources(pr, res) == 0)
				continue;
		}
		printk(KERN_WARNING "PCI: Cannot allocate resource region "
		       "%d of PCI bridge %d, will remap\n", i, bus->number);
clear_resource:
		res->start = res->end = 0;
		res->flags = 0;
	}

	list_for_each_entry(b, &bus->children, node)
		pcibios_allocate_bus_resources(b);
}

static inline void __devinit alloc_resource(struct pci_dev *dev, int idx)
{
	struct resource *pr, *r = &dev->resource[idx];

	pr_debug("PCI: Allocating %s: Resource %d: %016llx..%016llx [%x]\n",
		 pci_name(dev), idx,
		 (unsigned long long)r->start,
		 (unsigned long long)r->end,
		 (unsigned int)r->flags);

	pr = pci_find_parent_resource(dev, r);
	if (!pr || (pr->flags & IORESOURCE_UNSET) ||
	    request_resource(pr, r) < 0) {
		printk(KERN_WARNING "PCI: Cannot allocate resource region %d"
		       " of device %s, will remap\n", idx, pci_name(dev));
		if (pr)
			pr_debug("PCI:  parent is %p: %016llx-%016llx [%x]\n",
				 pr,
				 (unsigned long long)pr->start,
				 (unsigned long long)pr->end,
				 (unsigned int)pr->flags);
		/* We'll assign a new address later */
		r->flags |= IORESOURCE_UNSET;
		r->end -= r->start;
		r->start = 0;
	}
}

static void __init pcibios_allocate_resources(int pass)
{
	struct pci_dev *dev = NULL;
	int idx, disabled;
	u16 command;
	struct resource *r;

	for_each_pci_dev(dev) {
		pci_read_config_word(dev, PCI_COMMAND, &command);
		for (idx = 0; idx <= PCI_ROM_RESOURCE; idx++) {
			r = &dev->resource[idx];
			if (r->parent)		/* Already allocated */
				continue;
			if (!r->flags || (r->flags & IORESOURCE_UNSET))
				continue;	/* Not assigned at all */
			/* We only allocate ROMs on pass 1 just in case they
			 * have been screwed up by firmware
			 */
			if (idx == PCI_ROM_RESOURCE)
				disabled = 1;
			if (r->flags & IORESOURCE_IO)
				disabled = !(command & PCI_COMMAND_IO);
			else
				disabled = !(command & PCI_COMMAND_MEMORY);
			if (pass == disabled)
				alloc_resource(dev, idx);
		}
		if (pass)
			continue;
		r = &dev->resource[PCI_ROM_RESOURCE];
		if (r->flags) {
			/* Turn the ROM off, leave the resource region,
			 * but keep it unregistered.
			 */
			u32 reg;
			pci_read_config_dword(dev, dev->rom_base_reg, &reg);
			if (reg & PCI_ROM_ADDRESS_ENABLE) {
				pr_debug("PCI: Switching off ROM of %s\n",
					 pci_name(dev));
				r->flags &= ~IORESOURCE_ROM_ENABLE;
				pci_write_config_dword(dev, dev->rom_base_reg,
						reg & ~PCI_ROM_ADDRESS_ENABLE);
			}
		}
	}
}

static void __init pcibios_reserve_legacy_regions(struct pci_bus *bus)
{
	struct pci_controller *hose = pci_bus_to_host(bus);
	resource_size_t	offset;
	struct resource *res, *pres;
	int i;

	pr_debug("Reserving legacy ranges for domain %04x\n",
							pci_domain_nr(bus));

	/* Check for IO */
	if (!(hose->io_resource.flags & IORESOURCE_IO))
		goto no_io;
	offset = (unsigned long)hose->io_base_virt - _IO_BASE;
	res = kzalloc(sizeof(struct resource), GFP_KERNEL);
	BUG_ON(res == NULL);
	res->name = "Legacy IO";
	res->flags = IORESOURCE_IO;
	res->start = offset;
	res->end = (offset + 0xfff) & 0xfffffffful;
	pr_debug("Candidate legacy IO: %pR\n", res);
	if (request_resource(&hose->io_resource, res)) {
		printk(KERN_DEBUG
		       "PCI %04x:%02x Cannot reserve Legacy IO %pR\n",
		       pci_domain_nr(bus), bus->number, res);
		kfree(res);
	}

 no_io:
	/* Check for memory */
	offset = hose->pci_mem_offset;
	pr_debug("hose mem offset: %016llx\n", (unsigned long long)offset);
	for (i = 0; i < 3; i++) {
		pres = &hose->mem_resources[i];
		if (!(pres->flags & IORESOURCE_MEM))
			continue;
		pr_debug("hose mem res: %pR\n", pres);
		if ((pres->start - offset) <= 0xa0000 &&
		    (pres->end - offset) >= 0xbffff)
			break;
	}
	if (i >= 3)
		return;
	res = kzalloc(sizeof(struct resource), GFP_KERNEL);
	BUG_ON(res == NULL);
	res->name = "Legacy VGA memory";
	res->flags = IORESOURCE_MEM;
	res->start = 0xa0000 + offset;
	res->end = 0xbffff + offset;
	pr_debug("Candidate VGA memory: %pR\n", res);
	if (request_resource(pres, res)) {
		printk(KERN_DEBUG
		       "PCI %04x:%02x Cannot reserve VGA memory %pR\n",
		       pci_domain_nr(bus), bus->number, res);
		kfree(res);
	}
}

void __init pcibios_resource_survey(void)
{
	struct pci_bus *b;

	/* Allocate and assign resources. If we re-assign everything, then
	 * we skip the allocate phase
	 */
	list_for_each_entry(b, &pci_root_buses, node)
		pcibios_allocate_bus_resources(b);

	if (!(pci_flags & PCI_REASSIGN_ALL_RSRC)) {
		pcibios_allocate_resources(0);
		pcibios_allocate_resources(1);
	}

	/* Before we start assigning unassigned resource, we try to reserve
	 * the low IO area and the VGA memory area if they intersect the
	 * bus available resources to avoid allocating things on top of them
	 */
	if (!(pci_flags & PCI_PROBE_ONLY)) {
		list_for_each_entry(b, &pci_root_buses, node)
			pcibios_reserve_legacy_regions(b);
	}

	/* Now, if the platform didn't decide to blindly trust the firmware,
	 * we proceed to assigning things that were left unassigned
	 */
	if (!(pci_flags & PCI_PROBE_ONLY)) {
		pr_debug("PCI: Assigning unassigned resources...\n");
		pci_assign_unassigned_resources();
	}
}

#ifdef CONFIG_HOTPLUG

/* This is used by the PCI hotplug driver to allocate resource
 * of newly plugged busses. We can try to consolidate with the
 * rest of the code later, for now, keep it as-is as our main
 * resource allocation function doesn't deal with sub-trees yet.
 */
void __devinit pcibios_claim_one_bus(struct pci_bus *bus)
{
	struct pci_dev *dev;
	struct pci_bus *child_bus;

	list_for_each_entry(dev, &bus->devices, bus_list) {
		int i;

		for (i = 0; i < PCI_NUM_RESOURCES; i++) {
			struct resource *r = &dev->resource[i];

			if (r->parent || !r->start || !r->flags)
				continue;

			pr_debug("PCI: Claiming %s: "
				 "Resource %d: %016llx..%016llx [%x]\n",
				 pci_name(dev), i,
				 (unsigned long long)r->start,
				 (unsigned long long)r->end,
				 (unsigned int)r->flags);

			pci_claim_resource(dev, i);
		}
	}

	list_for_each_entry(child_bus, &bus->children, node)
		pcibios_claim_one_bus(child_bus);
}
EXPORT_SYMBOL_GPL(pcibios_claim_one_bus);


/* pcibios_finish_adding_to_bus
 *
 * This is to be called by the hotplug code after devices have been
 * added to a bus, this include calling it for a PHB that is just
 * being added
 */
void pcibios_finish_adding_to_bus(struct pci_bus *bus)
{
	pr_debug("PCI: Finishing adding to hotplug bus %04x:%02x\n",
		 pci_domain_nr(bus), bus->number);

	/* Allocate bus and devices resources */
	pcibios_allocate_bus_resources(bus);
	pcibios_claim_one_bus(bus);

	/* Add new devices to global lists.  Register in proc, sysfs. */
	pci_bus_add_devices(bus);

	/* Fixup EEH */
	/* eeh_add_device_tree_late(bus); */
}
EXPORT_SYMBOL_GPL(pcibios_finish_adding_to_bus);

#endif /* CONFIG_HOTPLUG */

int pcibios_enable_device(struct pci_dev *dev, int mask)
{
	return pci_enable_resources(dev, mask);
}

void __devinit pcibios_setup_phb_resources(struct pci_controller *hose)
{
	struct pci_bus *bus = hose->bus;
	struct resource *res;
	int i;

	/* Hookup PHB IO resource */
	bus->resource[0] = res = &hose->io_resource;

	if (!res->flags) {
		printk(KERN_WARNING "PCI: I/O resource not set for host"
		       " bridge %s (domain %d)\n",
		       hose->dn->full_name, hose->global_number);
		res->start = (unsigned long)hose->io_base_virt - isa_io_base;
		res->end = res->start + IO_SPACE_LIMIT;
		res->flags = IORESOURCE_IO;
	}

	pr_debug("PCI: PHB IO resource    = %016llx-%016llx [%lx]\n",
		 (unsigned long long)res->start,
		 (unsigned long long)res->end,
		 (unsigned long)res->flags);

	/* Hookup PHB Memory resources */
	for (i = 0; i < 3; ++i) {
		res = &hose->mem_resources[i];
		if (!res->flags) {
			if (i > 0)
				continue;
			printk(KERN_ERR "PCI: Memory resource 0 not set for "
			       "host bridge %s (domain %d)\n",
			       hose->dn->full_name, hose->global_number);

			res->start = hose->pci_mem_offset;
			res->end = (resource_size_t)-1LL;
			res->flags = IORESOURCE_MEM;

		}
		bus->resource[i+1] = res;

		pr_debug("PCI: PHB MEM resource %d = %016llx-%016llx [%lx]\n",
			i, (unsigned long long)res->start,
			(unsigned long long)res->end,
			(unsigned long)res->flags);
	}

	pr_debug("PCI: PHB MEM offset     = %016llx\n",
		 (unsigned long long)hose->pci_mem_offset);
	pr_debug("PCI: PHB IO  offset     = %08lx\n",
		 (unsigned long)hose->io_base_virt - _IO_BASE);
}

/*
 * Null PCI config access functions, for the case when we can't
 * find a hose.
 */
#define NULL_PCI_OP(rw, size, type)					\
static int								\
null_##rw##_config_##size(struct pci_dev *dev, int offset, type val)	\
{									\
	return PCIBIOS_DEVICE_NOT_FOUND;				\
}

static int
null_read_config(struct pci_bus *bus, unsigned int devfn, int offset,
		 int len, u32 *val)
{
	return PCIBIOS_DEVICE_NOT_FOUND;
}

static int
null_write_config(struct pci_bus *bus, unsigned int devfn, int offset,
		  int len, u32 val)
{
	return PCIBIOS_DEVICE_NOT_FOUND;
}

static struct pci_ops null_pci_ops = {
	.read = null_read_config,
	.write = null_write_config,
};

/*
 * These functions are used early on before PCI scanning is done
 * and all of the pci_dev and pci_bus structures have been created.
 */
static struct pci_bus *
fake_pci_bus(struct pci_controller *hose, int busnr)
{
	static struct pci_bus bus;

	if (!hose)
		printk(KERN_ERR "Can't find hose for PCI bus %d!\n", busnr);

	bus.number = busnr;
	bus.sysdata = hose;
	bus.ops = hose ? hose->ops : &null_pci_ops;
	return &bus;
}

#define EARLY_PCI_OP(rw, size, type)					\
int early_##rw##_config_##size(struct pci_controller *hose, int bus,	\
			       int devfn, int offset, type value)	\
{									\
	return pci_bus_##rw##_config_##size(fake_pci_bus(hose, bus),	\
					    devfn, offset, value);	\
}

EARLY_PCI_OP(read, byte, u8 *)
EARLY_PCI_OP(read, word, u16 *)
EARLY_PCI_OP(read, dword, u32 *)
EARLY_PCI_OP(write, byte, u8)
EARLY_PCI_OP(write, word, u16)
EARLY_PCI_OP(write, dword, u32)

int early_find_capability(struct pci_controller *hose, int bus, int devfn,
			  int cap)
{
	return pci_bus_find_capability(fake_pci_bus(hose, bus), devfn, cap);
}
