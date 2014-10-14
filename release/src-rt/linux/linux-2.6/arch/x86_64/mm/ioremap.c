/*
 * arch/x86_64/mm/ioremap.c
 *
 * Re-map IO memory to kernel address space so that we can access it.
 * This is needed for high PCI addresses that aren't mapped in the
 * 640k-1MB IO memory area on PC's
 *
 * (C) Copyright 1995 1996 Linus Torvalds
 */

#include <linux/vmalloc.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/io.h>

#include <asm/pgalloc.h>
#include <asm/fixmap.h>
#include <asm/tlbflush.h>
#include <asm/cacheflush.h>
#include <asm/proto.h>

unsigned long __phys_addr(unsigned long x)
{
	if (x >= __START_KERNEL_map)
		return x - __START_KERNEL_map + phys_base;
	return x - PAGE_OFFSET;
}
EXPORT_SYMBOL(__phys_addr);

#define ISA_START_ADDRESS      0xa0000
#define ISA_END_ADDRESS                0x100000

/*
 * Fix up the linear direct mapping of the kernel to avoid cache attribute
 * conflicts.
 */
static int
ioremap_change_attr(unsigned long phys_addr, unsigned long size,
					unsigned long flags)
{
	int err = 0;
	if (phys_addr + size - 1 < (end_pfn_map << PAGE_SHIFT)) {
		unsigned long npages = (size + PAGE_SIZE - 1) >> PAGE_SHIFT;
		unsigned long vaddr = (unsigned long) __va(phys_addr);

		/*
 		 * Must use a address here and not struct page because the phys addr
		 * can be a in hole between nodes and not have an memmap entry.
		 */
		err = change_page_attr_addr(vaddr,npages,__pgprot(__PAGE_KERNEL|flags));
		if (!err)
			global_flush_tlb();
	}
	return err;
}

/*
 * Generic mapping function
 */

/*
 * Remap an arbitrary physical address space into the kernel virtual
 * address space. Needed when the kernel wants to access high addresses
 * directly.
 *
 * NOTE! We need to allow non-page-aligned mappings too: we will obviously
 * have to convert them into an offset in a page-aligned mapping, but the
 * caller shouldn't need to know that small detail.
 */
void __iomem * __ioremap(unsigned long phys_addr, unsigned long size, unsigned long flags)
{
	void * addr;
	struct vm_struct * area;
	unsigned long offset, last_addr;
	pgprot_t pgprot;

	/* Don't allow wraparound or zero size */
	last_addr = phys_addr + size - 1;
	if (!size || last_addr < phys_addr)
		return NULL;

	/*
	 * Don't remap the low PCI/ISA area, it's always mapped..
	 */
	if (phys_addr >= ISA_START_ADDRESS && last_addr < ISA_END_ADDRESS)
		return (__force void __iomem *)phys_to_virt(phys_addr);

#ifdef CONFIG_FLATMEM
	/*
	 * Don't allow anybody to remap normal RAM that we're using..
	 */
	if (last_addr < virt_to_phys(high_memory)) {
		char *t_addr, *t_end;
 		struct page *page;

		t_addr = __va(phys_addr);
		t_end = t_addr + (size - 1);
	   
		for(page = virt_to_page(t_addr); page <= virt_to_page(t_end); page++)
			if(!PageReserved(page))
				return NULL;
	}
#endif

	pgprot = __pgprot(_PAGE_PRESENT | _PAGE_RW | _PAGE_GLOBAL
			  | _PAGE_DIRTY | _PAGE_ACCESSED | flags);
	/*
	 * Mappings have to be page-aligned
	 */
	offset = phys_addr & ~PAGE_MASK;
	phys_addr &= PAGE_MASK;
	size = PAGE_ALIGN(last_addr+1) - phys_addr;

	/*
	 * Ok, go for it..
	 */
	area = get_vm_area(size, VM_IOREMAP | (flags << 20));
	if (!area)
		return NULL;
	area->phys_addr = phys_addr;
	addr = area->addr;
	if (ioremap_page_range((unsigned long)addr, (unsigned long)addr + size,
			       phys_addr, pgprot)) {
		remove_vm_area((void *)(PAGE_MASK & (unsigned long) addr));
		return NULL;
	}
	if (flags && ioremap_change_attr(phys_addr, size, flags) < 0) {
		area->flags &= 0xffffff;
		vunmap(addr);
		return NULL;
	}
	return (__force void __iomem *) (offset + (char *)addr);
}
EXPORT_SYMBOL(__ioremap);

/**
 * ioremap_nocache     -   map bus memory into CPU space
 * @offset:    bus address of the memory
 * @size:      size of the resource to map
 *
 * ioremap_nocache performs a platform specific sequence of operations to
 * make bus memory CPU accessible via the readb/readw/readl/writeb/
 * writew/writel functions and the other mmio helpers. The returned
 * address is not guaranteed to be usable directly as a virtual
 * address. 
 *
 * This version of ioremap ensures that the memory is marked uncachable
 * on the CPU as well as honouring existing caching rules from things like
 * the PCI bus. Note that there are other caches and buffers on many 
 * busses. In particular driver authors should read up on PCI writes
 *
 * It's useful if some control registers are in such an area and
 * write combining or read caching is not desirable:
 * 
 * Must be freed with iounmap.
 */

void __iomem *ioremap_nocache (unsigned long phys_addr, unsigned long size)
{
	return __ioremap(phys_addr, size, _PAGE_PCD);
}
EXPORT_SYMBOL(ioremap_nocache);

/**
 * iounmap - Free a IO remapping
 * @addr: virtual address from ioremap_*
 *
 * Caller must ensure there is only one unmapping for the same pointer.
 */
void iounmap(volatile void __iomem *addr)
{
	struct vm_struct *p, *o;

	if (addr <= high_memory) 
		return; 
	if (addr >= phys_to_virt(ISA_START_ADDRESS) &&
		addr < phys_to_virt(ISA_END_ADDRESS))
		return;

	addr = (volatile void __iomem *)(PAGE_MASK & (unsigned long __force)addr);
	/* Use the vm area unlocked, assuming the caller
	   ensures there isn't another iounmap for the same address
	   in parallel. Reuse of the virtual address is prevented by
	   leaving it in the global lists until we're done with it.
	   cpa takes care of the direct mappings. */
	read_lock(&vmlist_lock);
	for (p = vmlist; p; p = p->next) {
		if (p->addr == addr)
			break;
	}
	read_unlock(&vmlist_lock);

	if (!p) {
		printk("iounmap: bad address %p\n", addr);
		dump_stack();
		return;
	}

	/* Reset the direct mapping. Can block */
	if (p->flags >> 20)
		ioremap_change_attr(p->phys_addr, p->size, 0);

	/* Finally remove it */
	o = remove_vm_area((void *)addr);
	BUG_ON(p != o || o == NULL);
	kfree(p); 
}
EXPORT_SYMBOL(iounmap);

