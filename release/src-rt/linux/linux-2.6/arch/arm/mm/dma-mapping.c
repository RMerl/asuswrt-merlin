/*
 *  linux/arch/arm/mm/dma-mapping.c
 *
 *  Copyright (C) 2000-2004 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  DMA uncached mapping support.
 */
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/gfp.h>
#include <linux/errno.h>
#include <linux/list.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/highmem.h>

#include <asm/memory.h>
#include <asm/highmem.h>
#include <asm/cacheflush.h>
#include <asm/tlbflush.h>
#include <asm/sizes.h>

static u64 get_coherent_dma_mask(struct device *dev)
{
	u64 mask = ISA_DMA_THRESHOLD;

	if (dev) {
		mask = dev->coherent_dma_mask;

		/*
		 * Sanity check the DMA mask - it must be non-zero, and
		 * must be able to be satisfied by a DMA allocation.
		 */
		if (mask == 0) {
			dev_warn(dev, "coherent DMA mask is unset\n");
			return 0;
		}

		if ((~mask) & ISA_DMA_THRESHOLD) {
			dev_warn(dev, "coherent DMA mask %#llx is smaller "
				 "than system GFP_DMA mask %#llx\n",
				 mask, (unsigned long long)ISA_DMA_THRESHOLD);
			return 0;
		}
	}

	return mask;
}

/*
 * Allocate a DMA buffer for 'dev' of size 'size' using the
 * specified gfp mask.  Note that 'size' must be page aligned.
 */
static struct page *__dma_alloc_buffer(struct device *dev, size_t size, gfp_t gfp)
{
	unsigned long order = get_order(size);
	struct page *page, *p, *e;
	void *ptr;
	u64 mask = get_coherent_dma_mask(dev);

#ifdef CONFIG_DMA_API_DEBUG
	u64 limit = (mask + 1) & ~mask;
	if (limit && size >= limit) {
		dev_warn(dev, "coherent allocation too big (requested %#x mask %#llx)\n",
			size, mask);
		return NULL;
	}
#endif

	if (!mask)
		return NULL;

	if (mask < 0xffffffffULL)
		gfp |= GFP_DMA;

	page = alloc_pages(gfp, order);
	if (!page)
		return NULL;

	/*
	 * Now split the huge page and free the excess pages
	 */
	split_page(page, order);
	for (p = page + (size >> PAGE_SHIFT), e = page + (1 << order); p < e; p++)
		__free_page(p);

	/*
	 * Ensure that the allocated pages are zeroed, and that any data
	 * lurking in the kernel direct-mapped region is invalidated.
	 */
	ptr = page_address(page);
	memset(ptr, 0, size);
	dmac_flush_range(ptr, ptr + size);
	outer_flush_range(__pa(ptr), __pa(ptr) + size);

	return page;
}

/*
 * Free a DMA buffer.  'size' must be page aligned.
 */
static void __dma_free_buffer(struct page *page, size_t size)
{
	struct page *e = page + (size >> PAGE_SHIFT);

	while (page < e) {
		__free_page(page);
		page++;
	}
}

#ifdef CONFIG_MMU
/* Sanity check size */
#if (CONSISTENT_DMA_SIZE % SZ_2M)
#error "CONSISTENT_DMA_SIZE must be multiple of 2MiB"
#endif

#define CONSISTENT_OFFSET(x)	(((unsigned long)(x) - CONSISTENT_BASE) >> PAGE_SHIFT)
#define CONSISTENT_PTE_INDEX(x) (((unsigned long)(x) - CONSISTENT_BASE) >> PGDIR_SHIFT)
#define NUM_CONSISTENT_PTES (CONSISTENT_DMA_SIZE >> PGDIR_SHIFT)

/*
 * These are the page tables (2MB each) covering uncached, DMA consistent allocations
 */
static pte_t *consistent_pte[NUM_CONSISTENT_PTES];

#include "vmregion.h"

static struct arm_vmregion_head consistent_head = {
	.vm_lock	= __SPIN_LOCK_UNLOCKED(&consistent_head.vm_lock),
	.vm_list	= LIST_HEAD_INIT(consistent_head.vm_list),
	.vm_start	= CONSISTENT_BASE,
	.vm_end		= CONSISTENT_END,
};

#ifdef CONFIG_HUGETLB_PAGE
#error ARM Coherent DMA allocator does not (yet) support huge TLB
#endif

/*
 * Initialise the consistent memory allocation.
 */
static int __init consistent_init(void)
{
	int ret = 0;
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;
	int i = 0;
	u32 base = CONSISTENT_BASE;

	do {
		pgd = pgd_offset(&init_mm, base);

		pud = pud_alloc(&init_mm, pgd, base);
		if (!pud) {
			printk(KERN_ERR "%s: no pud tables\n", __func__);
			ret = -ENOMEM;
			break;
		}

		pmd = pmd_alloc(&init_mm, pud, base);
		if (!pmd) {
			printk(KERN_ERR "%s: no pmd tables\n", __func__);
			ret = -ENOMEM;
			break;
		}
		WARN_ON(!pmd_none(*pmd));

		pte = pte_alloc_kernel(pmd, base);
		if (!pte) {
			printk(KERN_ERR "%s: no pte tables\n", __func__);
			ret = -ENOMEM;
			break;
		}

		consistent_pte[i++] = pte;
		base += (1 << PGDIR_SHIFT);
	} while (base < CONSISTENT_END);

	return ret;
}

core_initcall(consistent_init);

static void *
__dma_alloc_remap(struct page *page, size_t size, gfp_t gfp, pgprot_t prot)
{
	struct arm_vmregion *c;
	size_t align;
	int bit;

	if (!consistent_pte[0]) {
		printk(KERN_ERR "%s: not initialised\n", __func__);
		dump_stack();
		return NULL;
	}

	/*
	 * Align the virtual region allocation - maximum alignment is
	 * a section size, minimum is a page size.  This helps reduce
	 * fragmentation of the DMA space, and also prevents allocations
	 * smaller than a section from crossing a section boundary.
	 */
	bit = fls(size - 1);
	if (bit > SECTION_SHIFT)
		bit = SECTION_SHIFT;
	align = 1 << bit;

	/*
	 * Allocate a virtual address in the consistent mapping region.
	 */
	c = arm_vmregion_alloc(&consistent_head, align, size,
			    gfp & ~(__GFP_DMA | __GFP_HIGHMEM));
	if (c) {
		pte_t *pte;
		int idx = CONSISTENT_PTE_INDEX(c->vm_start);
		u32 off = CONSISTENT_OFFSET(c->vm_start) & (PTRS_PER_PTE-1);

		pte = consistent_pte[idx] + off;
		c->vm_pages = page;

		do {
			BUG_ON(!pte_none(*pte));

			set_pte_ext(pte, mk_pte(page, prot), 0);
			page++;
			pte++;
			off++;
			if (off >= PTRS_PER_PTE) {
				off = 0;
				pte = consistent_pte[++idx];
			}
		} while (size -= PAGE_SIZE);

		dsb();

		return (void *)c->vm_start;
	}
	return NULL;
}

static void __dma_free_remap(void *cpu_addr, size_t size)
{
	struct arm_vmregion *c;
	unsigned long addr;
	pte_t *ptep;
	int idx;
	u32 off;

	c = arm_vmregion_find_remove(&consistent_head, (unsigned long)cpu_addr);
	if (!c) {
		printk(KERN_ERR "%s: trying to free invalid coherent area: %p\n",
		       __func__, cpu_addr);
		dump_stack();
		return;
	}

	if ((c->vm_end - c->vm_start) != size) {
		printk(KERN_ERR "%s: freeing wrong coherent size (%ld != %d)\n",
		       __func__, c->vm_end - c->vm_start, size);
		dump_stack();
		size = c->vm_end - c->vm_start;
	}

	idx = CONSISTENT_PTE_INDEX(c->vm_start);
	off = CONSISTENT_OFFSET(c->vm_start) & (PTRS_PER_PTE-1);
	ptep = consistent_pte[idx] + off;
	addr = c->vm_start;
	do {
		pte_t pte = ptep_get_and_clear(&init_mm, addr, ptep);

		ptep++;
		addr += PAGE_SIZE;
		off++;
		if (off >= PTRS_PER_PTE) {
			off = 0;
			ptep = consistent_pte[++idx];
		}

		if (pte_none(pte) || !pte_present(pte))
			printk(KERN_CRIT "%s: bad page in kernel page table\n",
			       __func__);
	} while (size -= PAGE_SIZE);

	flush_tlb_kernel_range(c->vm_start, c->vm_end);

	arm_vmregion_free(&consistent_head, c);
}

#else	/* !CONFIG_MMU */

#define __dma_alloc_remap(page, size, gfp, prot)	page_address(page)
#define __dma_free_remap(addr, size)			do { } while (0)

#endif	/* CONFIG_MMU */

static void *
__dma_alloc(struct device *dev, size_t size, dma_addr_t *handle, gfp_t gfp,
	    pgprot_t prot)
{
	struct page *page;
	void *addr;

	*handle = ~0;
	size = PAGE_ALIGN(size);

	page = __dma_alloc_buffer(dev, size, gfp);
	if (!page)
		return NULL;

	if (!arch_is_coherent())
		addr = __dma_alloc_remap(page, size, gfp, prot);
	else
		addr = page_address(page);

	if (addr)
		*handle = pfn_to_dma(dev, page_to_pfn(page));

	return addr;
}

/*
 * Allocate DMA-coherent memory space and return both the kernel remapped
 * virtual and bus address for that space.
 */
void *
dma_alloc_coherent(struct device *dev, size_t size, dma_addr_t *handle, gfp_t gfp)
{
	void *memory;

	if (dma_alloc_from_coherent(dev, size, handle, &memory))
		return memory;

	return __dma_alloc(dev, size, handle, gfp,
			   pgprot_dmacoherent(pgprot_kernel));
}
EXPORT_SYMBOL(dma_alloc_coherent);

/*
 * Allocate a writecombining region, in much the same way as
 * dma_alloc_coherent above.
 */
void *
dma_alloc_writecombine(struct device *dev, size_t size, dma_addr_t *handle, gfp_t gfp)
{
	return __dma_alloc(dev, size, handle, gfp,
			   pgprot_writecombine(pgprot_kernel));
}
EXPORT_SYMBOL(dma_alloc_writecombine);

static int dma_mmap(struct device *dev, struct vm_area_struct *vma,
		    void *cpu_addr, dma_addr_t dma_addr, size_t size)
{
	int ret = -ENXIO;
#ifdef CONFIG_MMU
	unsigned long user_size, kern_size;
	struct arm_vmregion *c;

	user_size = (vma->vm_end - vma->vm_start) >> PAGE_SHIFT;

	c = arm_vmregion_find(&consistent_head, (unsigned long)cpu_addr);
	if (c) {
		unsigned long off = vma->vm_pgoff;

		kern_size = (c->vm_end - c->vm_start) >> PAGE_SHIFT;

		if (off < kern_size &&
		    user_size <= (kern_size - off)) {
			ret = remap_pfn_range(vma, vma->vm_start,
					      page_to_pfn(c->vm_pages) + off,
					      user_size << PAGE_SHIFT,
					      vma->vm_page_prot);
		}
	}
#endif	/* CONFIG_MMU */

	return ret;
}

int dma_mmap_coherent(struct device *dev, struct vm_area_struct *vma,
		      void *cpu_addr, dma_addr_t dma_addr, size_t size)
{
	vma->vm_page_prot = pgprot_dmacoherent(vma->vm_page_prot);
	return dma_mmap(dev, vma, cpu_addr, dma_addr, size);
}
EXPORT_SYMBOL(dma_mmap_coherent);

int dma_mmap_writecombine(struct device *dev, struct vm_area_struct *vma,
			  void *cpu_addr, dma_addr_t dma_addr, size_t size)
{
	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
	return dma_mmap(dev, vma, cpu_addr, dma_addr, size);
}
EXPORT_SYMBOL(dma_mmap_writecombine);

/*
 * free a page as defined by the above mapping.
 * Must not be called with IRQs disabled.
 */
void dma_free_coherent(struct device *dev, size_t size, void *cpu_addr, dma_addr_t handle)
{
	WARN_ON(irqs_disabled());

	if (dma_release_from_coherent(dev, get_order(size), cpu_addr))
		return;

	size = PAGE_ALIGN(size);

	if (!arch_is_coherent())
		__dma_free_remap(cpu_addr, size);

	__dma_free_buffer(pfn_to_page(dma_to_pfn(dev, handle)), size);
}
EXPORT_SYMBOL(dma_free_coherent);

/*
 * Make an area consistent for devices.
 * Note: Drivers should NOT use this function directly, as it will break
 * platforms with CONFIG_DMABOUNCE.
 * Use the driver DMA support - see dma-mapping.h (dma_sync_*)
 */
void ___dma_single_cpu_to_dev(const void *kaddr, size_t size,
	enum dma_data_direction dir)
{
	unsigned long paddr;

	BUG_ON(!virt_addr_valid(kaddr) || !virt_addr_valid(kaddr + size - 1));

	dmac_map_area(kaddr, size, dir);

	paddr = __pa(kaddr);
	if (dir == DMA_FROM_DEVICE) {
		outer_inv_range(paddr, paddr + size);
	} else {
		outer_clean_range(paddr, paddr + size);
	}
	/* FIXME: non-speculating: flush on bidirectional mappings? */
}
EXPORT_SYMBOL(___dma_single_cpu_to_dev);

void ___dma_single_dev_to_cpu(const void *kaddr, size_t size,
	enum dma_data_direction dir)
{
	BUG_ON(!virt_addr_valid(kaddr) || !virt_addr_valid(kaddr + size - 1));

	/* FIXME: non-speculating: not required */
	/* don't bother invalidating if DMA to device */
	if (dir != DMA_TO_DEVICE) {
		unsigned long paddr = __pa(kaddr);
		outer_inv_range(paddr, paddr + size);
	}

	dmac_unmap_area(kaddr, size, dir);
}
EXPORT_SYMBOL(___dma_single_dev_to_cpu);

static void dma_cache_maint_page(struct page *page, unsigned long offset,
	size_t size, enum dma_data_direction dir,
	void (*op)(const void *, size_t, int))
{
	/*
	 * A single sg entry may refer to multiple physically contiguous
	 * pages.  But we still need to process highmem pages individually.
	 * If highmem is not configured then the bulk of this loop gets
	 * optimized out.
	 */
	size_t left = size;
	do {
		size_t len = left;
		void *vaddr;

		if (PageHighMem(page)) {
			if (len + offset > PAGE_SIZE) {
				if (offset >= PAGE_SIZE) {
					page += offset / PAGE_SIZE;
					offset %= PAGE_SIZE;
				}
				len = PAGE_SIZE - offset;
			}
			vaddr = kmap_high_get(page);
			if (vaddr) {
				vaddr += offset;
				op(vaddr, len, dir);
				kunmap_high(page);
			} else if (cache_is_vipt()) {
				/* unmapped pages might still be cached */
				vaddr = kmap_atomic(page);
				op(vaddr + offset, len, dir);
				kunmap_atomic(vaddr);
			}
		} else {
			vaddr = page_address(page) + offset;
			op(vaddr, len, dir);
		}
		offset = 0;
		page++;
		left -= len;
	} while (left);
}

void ___dma_page_cpu_to_dev(struct page *page, unsigned long off,
	size_t size, enum dma_data_direction dir)
{
	unsigned long paddr;

	dma_cache_maint_page(page, off, size, dir, dmac_map_area);

	paddr = page_to_phys(page) + off;
	if (dir == DMA_FROM_DEVICE) {
		outer_inv_range(paddr, paddr + size);
	} else {
		outer_clean_range(paddr, paddr + size);
	}
	/* FIXME: non-speculating: flush on bidirectional mappings? */
}
EXPORT_SYMBOL(___dma_page_cpu_to_dev);

void ___dma_page_dev_to_cpu(struct page *page, unsigned long off,
	size_t size, enum dma_data_direction dir)
{
	unsigned long paddr = page_to_phys(page) + off;

	/* FIXME: non-speculating: not required */
	/* don't bother invalidating if DMA to device */
	if (dir != DMA_TO_DEVICE)
		outer_inv_range(paddr, paddr + size);

	dma_cache_maint_page(page, off, size, dir, dmac_unmap_area);

	/*
	 * Mark the D-cache clean for this page to avoid extra flushing.
	 */
	if (dir != DMA_TO_DEVICE && off == 0 && size >= PAGE_SIZE)
		set_bit(PG_dcache_clean, &page->flags);
}
EXPORT_SYMBOL(___dma_page_dev_to_cpu);

/**
 * dma_map_sg - map a set of SG buffers for streaming mode DMA
 * @dev: valid struct device pointer, or NULL for ISA and EISA-like devices
 * @sg: list of buffers
 * @nents: number of buffers to map
 * @dir: DMA transfer direction
 *
 * Map a set of buffers described by scatterlist in streaming mode for DMA.
 * This is the scatter-gather version of the dma_map_single interface.
 * Here the scatter gather list elements are each tagged with the
 * appropriate dma address and length.  They are obtained via
 * sg_dma_{address,length}.
 *
 * Device ownership issues as mentioned for dma_map_single are the same
 * here.
 */
int dma_map_sg(struct device *dev, struct scatterlist *sg, int nents,
		enum dma_data_direction dir)
{
	struct scatterlist *s;
	int i, j;

	BUG_ON(!valid_dma_direction(dir));

	for_each_sg(sg, s, nents, i) {
		s->dma_address = __dma_map_page(dev, sg_page(s), s->offset,
						s->length, dir);
		if (dma_mapping_error(dev, s->dma_address))
			goto bad_mapping;
	}
	debug_dma_map_sg(dev, sg, nents, nents, dir);
	return nents;

 bad_mapping:
	for_each_sg(sg, s, i, j)
		__dma_unmap_page(dev, sg_dma_address(s), sg_dma_len(s), dir);
	return 0;
}
EXPORT_SYMBOL(dma_map_sg);

/**
 * dma_unmap_sg - unmap a set of SG buffers mapped by dma_map_sg
 * @dev: valid struct device pointer, or NULL for ISA and EISA-like devices
 * @sg: list of buffers
 * @nents: number of buffers to unmap (same as was passed to dma_map_sg)
 * @dir: DMA transfer direction (same as was passed to dma_map_sg)
 *
 * Unmap a set of streaming mode DMA translations.  Again, CPU access
 * rules concerning calls here are the same as for dma_unmap_single().
 */
void dma_unmap_sg(struct device *dev, struct scatterlist *sg, int nents,
		enum dma_data_direction dir)
{
	struct scatterlist *s;
	int i;

	debug_dma_unmap_sg(dev, sg, nents, dir);

	for_each_sg(sg, s, nents, i)
		__dma_unmap_page(dev, sg_dma_address(s), sg_dma_len(s), dir);
}
EXPORT_SYMBOL(dma_unmap_sg);

/**
 * dma_sync_sg_for_cpu
 * @dev: valid struct device pointer, or NULL for ISA and EISA-like devices
 * @sg: list of buffers
 * @nents: number of buffers to map (returned from dma_map_sg)
 * @dir: DMA transfer direction (same as was passed to dma_map_sg)
 */
void dma_sync_sg_for_cpu(struct device *dev, struct scatterlist *sg,
			int nents, enum dma_data_direction dir)
{
	struct scatterlist *s;
	int i;

	for_each_sg(sg, s, nents, i) {
		if (!dmabounce_sync_for_cpu(dev, sg_dma_address(s), 0,
					    sg_dma_len(s), dir))
			continue;

		__dma_page_dev_to_cpu(sg_page(s), s->offset,
				      s->length, dir);
	}

	debug_dma_sync_sg_for_cpu(dev, sg, nents, dir);
}
EXPORT_SYMBOL(dma_sync_sg_for_cpu);

/**
 * dma_sync_sg_for_device
 * @dev: valid struct device pointer, or NULL for ISA and EISA-like devices
 * @sg: list of buffers
 * @nents: number of buffers to map (returned from dma_map_sg)
 * @dir: DMA transfer direction (same as was passed to dma_map_sg)
 */
void dma_sync_sg_for_device(struct device *dev, struct scatterlist *sg,
			int nents, enum dma_data_direction dir)
{
	struct scatterlist *s;
	int i;

	for_each_sg(sg, s, nents, i) {
		if (!dmabounce_sync_for_device(dev, sg_dma_address(s), 0,
					sg_dma_len(s), dir))
			continue;

		__dma_page_cpu_to_dev(sg_page(s), s->offset,
				      s->length, dir);
	}

	debug_dma_sync_sg_for_device(dev, sg, nents, dir);
}
EXPORT_SYMBOL(dma_sync_sg_for_device);

#define PREALLOC_DMA_DEBUG_ENTRIES	4096

static int __init dma_debug_do_init(void)
{
	dma_debug_init(PREALLOC_DMA_DEBUG_ENTRIES);
	return 0;
}
fs_initcall(dma_debug_do_init);
