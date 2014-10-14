/*
 * Dynamic DMA mapping support for AMD Hammer.
 * 
 * Use the integrated AGP GART in the Hammer northbridge as an IOMMU for PCI.
 * This allows to use PCI devices that only support 32bit addresses on systems
 * with more than 4GB. 
 *
 * See Documentation/DMA-mapping.txt for the interface specification.
 * 
 * Copyright 2002 Andi Kleen, SuSE Labs.
 */

#include <linux/types.h>
#include <linux/ctype.h>
#include <linux/agp_backend.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/spinlock.h>
#include <linux/pci.h>
#include <linux/module.h>
#include <linux/topology.h>
#include <linux/interrupt.h>
#include <linux/bitops.h>
#include <linux/kdebug.h>
#include <asm/atomic.h>
#include <asm/io.h>
#include <asm/mtrr.h>
#include <asm/pgtable.h>
#include <asm/proto.h>
#include <asm/cacheflush.h>
#include <asm/swiotlb.h>
#include <asm/dma.h>
#include <asm/k8.h>

unsigned long iommu_bus_base;	/* GART remapping area (physical) */
static unsigned long iommu_size; 	/* size of remapping area bytes */
static unsigned long iommu_pages;	/* .. and in pages */

u32 *iommu_gatt_base; 		/* Remapping table */

/* If this is disabled the IOMMU will use an optimized flushing strategy
   of only flushing when an mapping is reused. With it true the GART is flushed 
   for every mapping. Problem is that doing the lazy flush seems to trigger
   bugs with some popular PCI cards, in particular 3ware (but has been also
   also seen with Qlogic at least). */
int iommu_fullflush = 1;

/* Allocation bitmap for the remapping area */ 
static DEFINE_SPINLOCK(iommu_bitmap_lock);
static unsigned long *iommu_gart_bitmap; /* guarded by iommu_bitmap_lock */

static u32 gart_unmapped_entry; 

#define GPTE_VALID    1
#define GPTE_COHERENT 2
#define GPTE_ENCODE(x) \
	(((x) & 0xfffff000) | (((x) >> 32) << 4) | GPTE_VALID | GPTE_COHERENT)
#define GPTE_DECODE(x) (((x) & 0xfffff000) | (((u64)(x) & 0xff0) << 28))

#define to_pages(addr,size) \
	(round_up(((addr) & ~PAGE_MASK) + (size), PAGE_SIZE) >> PAGE_SHIFT)

#define EMERGENCY_PAGES 32 /* = 128KB */ 

#ifdef CONFIG_AGP
#define AGPEXTERN extern
#else
#define AGPEXTERN
#endif

/* backdoor interface to AGP driver */
AGPEXTERN int agp_memory_reserved;
AGPEXTERN __u32 *agp_gatt_table;

static unsigned long next_bit;  /* protected by iommu_bitmap_lock */
static int need_flush; 		/* global flush state. set for each gart wrap */

static unsigned long alloc_iommu(int size) 
{ 	
	unsigned long offset, flags;

	spin_lock_irqsave(&iommu_bitmap_lock, flags);	
	offset = find_next_zero_string(iommu_gart_bitmap,next_bit,iommu_pages,size);
	if (offset == -1) {
		need_flush = 1;
		offset = find_next_zero_string(iommu_gart_bitmap,0,iommu_pages,size);
	}
	if (offset != -1) { 
		set_bit_string(iommu_gart_bitmap, offset, size); 
		next_bit = offset+size; 
		if (next_bit >= iommu_pages) { 
			next_bit = 0;
			need_flush = 1;
		} 
	} 
	if (iommu_fullflush)
		need_flush = 1;
	spin_unlock_irqrestore(&iommu_bitmap_lock, flags);      
	return offset;
} 

static void free_iommu(unsigned long offset, int size)
{ 
	unsigned long flags;
	spin_lock_irqsave(&iommu_bitmap_lock, flags);
	__clear_bit_string(iommu_gart_bitmap, offset, size);
	spin_unlock_irqrestore(&iommu_bitmap_lock, flags);
} 

/* 
 * Use global flush state to avoid races with multiple flushers.
 */
static void flush_gart(void)
{ 
	unsigned long flags;
	spin_lock_irqsave(&iommu_bitmap_lock, flags);
	if (need_flush) {
		k8_flush_garts();
		need_flush = 0;
	} 
	spin_unlock_irqrestore(&iommu_bitmap_lock, flags);
} 

#ifdef CONFIG_IOMMU_LEAK

#define SET_LEAK(x) if (iommu_leak_tab) \
			iommu_leak_tab[x] = __builtin_return_address(0);
#define CLEAR_LEAK(x) if (iommu_leak_tab) \
			iommu_leak_tab[x] = NULL;

/* Debugging aid for drivers that don't free their IOMMU tables */
static void **iommu_leak_tab; 
static int leak_trace;
int iommu_leak_pages = 20; 
void dump_leak(void)
{
	int i;
	static int dump; 
	if (dump || !iommu_leak_tab) return;
	dump = 1;
	show_stack(NULL,NULL);
	/* Very crude. dump some from the end of the table too */ 
	printk("Dumping %d pages from end of IOMMU:\n", iommu_leak_pages); 
	for (i = 0; i < iommu_leak_pages; i+=2) {
		printk("%lu: ", iommu_pages-i);
		printk_address((unsigned long) iommu_leak_tab[iommu_pages-i]);
		printk("%c", (i+1)%2 == 0 ? '\n' : ' '); 
	} 
	printk("\n");
}
#else
#define SET_LEAK(x)
#define CLEAR_LEAK(x)
#endif

static void iommu_full(struct device *dev, size_t size, int dir)
{
	/* 
	 * Ran out of IOMMU space for this operation. This is very bad.
	 * Unfortunately the drivers cannot handle this operation properly.
	 * Return some non mapped prereserved space in the aperture and 
	 * let the Northbridge deal with it. This will result in garbage
	 * in the IO operation. When the size exceeds the prereserved space
	 * memory corruption will occur or random memory will be DMAed 
	 * out. Hopefully no network devices use single mappings that big.
	 */ 
	
	printk(KERN_ERR 
  "PCI-DMA: Out of IOMMU space for %lu bytes at device %s\n",
	       size, dev->bus_id);

	if (size > PAGE_SIZE*EMERGENCY_PAGES) {
		if (dir == PCI_DMA_FROMDEVICE || dir == PCI_DMA_BIDIRECTIONAL)
			panic("PCI-DMA: Memory would be corrupted\n");
		if (dir == PCI_DMA_TODEVICE || dir == PCI_DMA_BIDIRECTIONAL) 
			panic(KERN_ERR "PCI-DMA: Random memory would be DMAed\n");
	} 

#ifdef CONFIG_IOMMU_LEAK
	dump_leak(); 
#endif
} 

static inline int need_iommu(struct device *dev, unsigned long addr, size_t size)
{ 
	u64 mask = *dev->dma_mask;
	int high = addr + size > mask;
	int mmu = high;
	if (force_iommu) 
		mmu = 1; 
	return mmu; 
}

static inline int nonforced_iommu(struct device *dev, unsigned long addr, size_t size)
{ 
	u64 mask = *dev->dma_mask;
	int high = addr + size > mask;
	int mmu = high;
	return mmu; 
}

/* Map a single continuous physical area into the IOMMU.
 * Caller needs to check if the iommu is needed and flush.
 */
static dma_addr_t dma_map_area(struct device *dev, dma_addr_t phys_mem,
				size_t size, int dir)
{ 
	unsigned long npages = to_pages(phys_mem, size);
	unsigned long iommu_page = alloc_iommu(npages);
	int i;
	if (iommu_page == -1) {
		if (!nonforced_iommu(dev, phys_mem, size))
			return phys_mem; 
		if (panic_on_overflow)
			panic("dma_map_area overflow %lu bytes\n", size);
		iommu_full(dev, size, dir);
		return bad_dma_address;
	}

	for (i = 0; i < npages; i++) {
		iommu_gatt_base[iommu_page + i] = GPTE_ENCODE(phys_mem);
		SET_LEAK(iommu_page + i);
		phys_mem += PAGE_SIZE;
	}
	return iommu_bus_base + iommu_page*PAGE_SIZE + (phys_mem & ~PAGE_MASK);
}

static dma_addr_t gart_map_simple(struct device *dev, char *buf,
				 size_t size, int dir)
{
	dma_addr_t map = dma_map_area(dev, virt_to_bus(buf), size, dir);
	flush_gart();
	return map;
}

/* Map a single area into the IOMMU */
dma_addr_t gart_map_single(struct device *dev, void *addr, size_t size, int dir)
{
	unsigned long phys_mem, bus;

	if (!dev)
		dev = &fallback_dev;

	phys_mem = virt_to_phys(addr); 
	if (!need_iommu(dev, phys_mem, size))
		return phys_mem; 

	bus = gart_map_simple(dev, addr, size, dir);
	return bus; 
}

/*
 * Free a DMA mapping.
 */
void gart_unmap_single(struct device *dev, dma_addr_t dma_addr,
		      size_t size, int direction)
{
	unsigned long iommu_page;
	int npages;
	int i;

	if (dma_addr < iommu_bus_base + EMERGENCY_PAGES*PAGE_SIZE ||
	    dma_addr >= iommu_bus_base + iommu_size)
		return;
	iommu_page = (dma_addr - iommu_bus_base)>>PAGE_SHIFT;
	npages = to_pages(dma_addr, size);
	for (i = 0; i < npages; i++) {
		iommu_gatt_base[iommu_page + i] = gart_unmapped_entry;
		CLEAR_LEAK(iommu_page + i);
	}
	free_iommu(iommu_page, npages);
}

/*
 * Wrapper for pci_unmap_single working with scatterlists.
 */
void gart_unmap_sg(struct device *dev, struct scatterlist *sg, int nents, int dir)
{
	int i;

	for (i = 0; i < nents; i++) {
		struct scatterlist *s = &sg[i];
		if (!s->dma_length || !s->length)
			break;
		gart_unmap_single(dev, s->dma_address, s->dma_length, dir);
	}
}

/* Fallback for dma_map_sg in case of overflow */
static int dma_map_sg_nonforce(struct device *dev, struct scatterlist *sg,
			       int nents, int dir)
{
	int i;

#ifdef CONFIG_IOMMU_DEBUG
	printk(KERN_DEBUG "dma_map_sg overflow\n");
#endif

 	for (i = 0; i < nents; i++ ) {
		struct scatterlist *s = &sg[i];
		unsigned long addr = page_to_phys(s->page) + s->offset; 
		if (nonforced_iommu(dev, addr, s->length)) { 
			addr = dma_map_area(dev, addr, s->length, dir);
			if (addr == bad_dma_address) { 
				if (i > 0) 
					gart_unmap_sg(dev, sg, i, dir);
				nents = 0; 
				sg[0].dma_length = 0;
				break;
			}
		}
		s->dma_address = addr;
		s->dma_length = s->length;
	}
	flush_gart();
	return nents;
}

/* Map multiple scatterlist entries continuous into the first. */
static int __dma_map_cont(struct scatterlist *sg, int start, int stopat,
		      struct scatterlist *sout, unsigned long pages)
{
	unsigned long iommu_start = alloc_iommu(pages);
	unsigned long iommu_page = iommu_start; 
	int i;

	if (iommu_start == -1)
		return -1;
	
	for (i = start; i < stopat; i++) {
		struct scatterlist *s = &sg[i];
		unsigned long pages, addr;
		unsigned long phys_addr = s->dma_address;
		
		BUG_ON(i > start && s->offset);
		if (i == start) {
			*sout = *s; 
			sout->dma_address = iommu_bus_base;
			sout->dma_address += iommu_page*PAGE_SIZE + s->offset;
			sout->dma_length = s->length;
		} else { 
			sout->dma_length += s->length; 
		}

		addr = phys_addr;
		pages = to_pages(s->offset, s->length); 
		while (pages--) { 
			iommu_gatt_base[iommu_page] = GPTE_ENCODE(addr); 
			SET_LEAK(iommu_page);
			addr += PAGE_SIZE;
			iommu_page++;
		}
	} 
	BUG_ON(iommu_page - iommu_start != pages);	
	return 0;
}

static inline int dma_map_cont(struct scatterlist *sg, int start, int stopat,
		      struct scatterlist *sout,
		      unsigned long pages, int need)
{
	if (!need) { 
		BUG_ON(stopat - start != 1);
		*sout = sg[start]; 
		sout->dma_length = sg[start].length; 
		return 0;
	} 
	return __dma_map_cont(sg, start, stopat, sout, pages);
}
		
/*
 * DMA map all entries in a scatterlist.
 * Merge chunks that have page aligned sizes into a continuous mapping. 
 */
int gart_map_sg(struct device *dev, struct scatterlist *sg, int nents, int dir)
{
	int i;
	int out;
	int start;
	unsigned long pages = 0;
	int need = 0, nextneed;

	if (nents == 0) 
		return 0;

	if (!dev)
		dev = &fallback_dev;

	out = 0;
	start = 0;
	for (i = 0; i < nents; i++) {
		struct scatterlist *s = &sg[i];
		dma_addr_t addr = page_to_phys(s->page) + s->offset;
		s->dma_address = addr;
		BUG_ON(s->length == 0); 

		nextneed = need_iommu(dev, addr, s->length); 

		/* Handle the previous not yet processed entries */
		if (i > start) {
			struct scatterlist *ps = &sg[i-1];
			/* Can only merge when the last chunk ends on a page 
			   boundary and the new one doesn't have an offset. */
			if (!iommu_merge || !nextneed || !need || s->offset ||
			    (ps->offset + ps->length) % PAGE_SIZE) { 
				if (dma_map_cont(sg, start, i, sg+out, pages,
						 need) < 0)
					goto error;
				out++;
				pages = 0;
				start = i;	
			}
		}

		need = nextneed;
		pages += to_pages(s->offset, s->length);
	}
	if (dma_map_cont(sg, start, i, sg+out, pages, need) < 0)
		goto error;
	out++;
	flush_gart();
	if (out < nents) 
		sg[out].dma_length = 0; 
	return out;

error:
	flush_gart();
	gart_unmap_sg(dev, sg, nents, dir);
	/* When it was forced or merged try again in a dumb way */
	if (force_iommu || iommu_merge) {
		out = dma_map_sg_nonforce(dev, sg, nents, dir);
		if (out > 0)
			return out;
	}
	if (panic_on_overflow)
		panic("dma_map_sg: overflow on %lu pages\n", pages);
	iommu_full(dev, pages << PAGE_SHIFT, dir);
	for (i = 0; i < nents; i++)
		sg[i].dma_address = bad_dma_address;
	return 0;
} 

static int no_agp;

static __init unsigned long check_iommu_size(unsigned long aper, u64 aper_size)
{ 
	unsigned long a; 
	if (!iommu_size) { 
		iommu_size = aper_size; 
		if (!no_agp) 
			iommu_size /= 2; 
	} 

	a = aper + iommu_size; 
	iommu_size -= round_up(a, LARGE_PAGE_SIZE) - a;

	if (iommu_size < 64*1024*1024) 
		printk(KERN_WARNING
  "PCI-DMA: Warning: Small IOMMU %luMB. Consider increasing the AGP aperture in BIOS\n",iommu_size>>20); 
	
	return iommu_size;
} 

static __init unsigned read_aperture(struct pci_dev *dev, u32 *size) 
{ 
	unsigned aper_size = 0, aper_base_32;
	u64 aper_base;
	unsigned aper_order;

	pci_read_config_dword(dev, 0x94, &aper_base_32); 
	pci_read_config_dword(dev, 0x90, &aper_order);
	aper_order = (aper_order >> 1) & 7;	

	aper_base = aper_base_32 & 0x7fff; 
	aper_base <<= 25;

	aper_size = (32 * 1024 * 1024) << aper_order; 
       if (aper_base + aper_size > 0x100000000UL || !aper_size)
		aper_base = 0;

	*size = aper_size;
	return aper_base;
} 

/* 
 * Private Northbridge GATT initialization in case we cannot use the
 * AGP driver for some reason.  
 */
static __init int init_k8_gatt(struct agp_kern_info *info)
{ 
	struct pci_dev *dev;
	void *gatt;
	unsigned aper_base, new_aper_base;
	unsigned aper_size, gatt_size, new_aper_size;
	int i;

	printk(KERN_INFO "PCI-DMA: Disabling AGP.\n");
	aper_size = aper_base = info->aper_size = 0;
	dev = NULL;
	for (i = 0; i < num_k8_northbridges; i++) {
		dev = k8_northbridges[i];
		new_aper_base = read_aperture(dev, &new_aper_size); 
		if (!new_aper_base) 
			goto nommu; 
		
		if (!aper_base) { 
			aper_size = new_aper_size;
			aper_base = new_aper_base;
		} 
		if (aper_size != new_aper_size || aper_base != new_aper_base) 
			goto nommu;
	}
	if (!aper_base)
		goto nommu; 
	info->aper_base = aper_base;
	info->aper_size = aper_size>>20; 

	gatt_size = (aper_size >> PAGE_SHIFT) * sizeof(u32); 
	gatt = (void *)__get_free_pages(GFP_KERNEL, get_order(gatt_size)); 
	if (!gatt) 
		panic("Cannot allocate GATT table");
	if (change_page_attr_addr((unsigned long)gatt, gatt_size >> PAGE_SHIFT, PAGE_KERNEL_NOCACHE))
		panic("Could not set GART PTEs to uncacheable pages");
	global_flush_tlb();

	memset(gatt, 0, gatt_size); 
	agp_gatt_table = gatt;

	for (i = 0; i < num_k8_northbridges; i++) {
		u32 ctl; 
		u32 gatt_reg; 

		dev = k8_northbridges[i];
		gatt_reg = __pa(gatt) >> 12; 
		gatt_reg <<= 4; 
		pci_write_config_dword(dev, 0x98, gatt_reg);
		pci_read_config_dword(dev, 0x90, &ctl); 

		ctl |= 1;
		ctl &= ~((1<<4) | (1<<5));

		pci_write_config_dword(dev, 0x90, ctl); 
	}
	flush_gart();
	
	printk("PCI-DMA: aperture base @ %x size %u KB\n",aper_base, aper_size>>10); 
	return 0;

 nommu:
 	/* Should not happen anymore */
	printk(KERN_ERR "PCI-DMA: More than 4GB of RAM and no IOMMU\n"
	       KERN_ERR "PCI-DMA: 32bit PCI IO may malfunction.\n");
	return -1; 
} 

extern int agp_amd64_init(void);

static const struct dma_mapping_ops gart_dma_ops = {
	.mapping_error = NULL,
	.map_single = gart_map_single,
	.map_simple = gart_map_simple,
	.unmap_single = gart_unmap_single,
	.sync_single_for_cpu = NULL,
	.sync_single_for_device = NULL,
	.sync_single_range_for_cpu = NULL,
	.sync_single_range_for_device = NULL,
	.sync_sg_for_cpu = NULL,
	.sync_sg_for_device = NULL,
	.map_sg = gart_map_sg,
	.unmap_sg = gart_unmap_sg,
};

void __init gart_iommu_init(void)
{ 
	struct agp_kern_info info;
	unsigned long aper_size;
	unsigned long iommu_start;
	unsigned long scratch;
	long i;

	if (cache_k8_northbridges() < 0 || num_k8_northbridges == 0) {
		printk(KERN_INFO "PCI-GART: No AMD northbridge found.\n");
		return;
	}

#ifndef CONFIG_AGP_AMD64
	no_agp = 1; 
#else
	/* Makefile puts PCI initialization via subsys_initcall first. */
	/* Add other K8 AGP bridge drivers here */
	no_agp = no_agp || 
		(agp_amd64_init() < 0) || 
		(agp_copy_info(agp_bridge, &info) < 0);
#endif	

	if (swiotlb)
		return;

	/* Did we detect a different HW IOMMU? */
	if (iommu_detected && !iommu_aperture)
		return;

	if (no_iommu ||
	    (!force_iommu && end_pfn <= MAX_DMA32_PFN) ||
	    !iommu_aperture ||
	    (no_agp && init_k8_gatt(&info) < 0)) {
		if (end_pfn > MAX_DMA32_PFN) {
			printk(KERN_ERR "WARNING more than 4GB of memory "
					"but GART IOMMU not available.\n"
			       KERN_ERR "WARNING 32bit PCI may malfunction.\n");
		}
		return;
	}

	printk(KERN_INFO "PCI-DMA: using GART IOMMU.\n");
	aper_size = info.aper_size * 1024 * 1024;	
	iommu_size = check_iommu_size(info.aper_base, aper_size); 
	iommu_pages = iommu_size >> PAGE_SHIFT; 

	iommu_gart_bitmap = (void*)__get_free_pages(GFP_KERNEL, 
						    get_order(iommu_pages/8)); 
	if (!iommu_gart_bitmap) 
		panic("Cannot allocate iommu bitmap\n"); 
	memset(iommu_gart_bitmap, 0, iommu_pages/8);

#ifdef CONFIG_IOMMU_LEAK
	if (leak_trace) { 
		iommu_leak_tab = (void *)__get_free_pages(GFP_KERNEL, 
				  get_order(iommu_pages*sizeof(void *)));
		if (iommu_leak_tab) 
			memset(iommu_leak_tab, 0, iommu_pages * 8); 
		else
			printk("PCI-DMA: Cannot allocate leak trace area\n"); 
	} 
#endif

	/* 
	 * Out of IOMMU space handling.
	 * Reserve some invalid pages at the beginning of the GART. 
	 */ 
	set_bit_string(iommu_gart_bitmap, 0, EMERGENCY_PAGES); 

	agp_memory_reserved = iommu_size;	
	printk(KERN_INFO
	       "PCI-DMA: Reserving %luMB of IOMMU area in the AGP aperture\n",
	       iommu_size>>20); 

	iommu_start = aper_size - iommu_size;	
	iommu_bus_base = info.aper_base + iommu_start; 
	bad_dma_address = iommu_bus_base;
	iommu_gatt_base = agp_gatt_table + (iommu_start>>PAGE_SHIFT);

	/* 
	 * Unmap the IOMMU part of the GART. The alias of the page is
	 * always mapped with cache enabled and there is no full cache
	 * coherency across the GART remapping. The unmapping avoids
	 * automatic prefetches from the CPU allocating cache lines in
	 * there. All CPU accesses are done via the direct mapping to
	 * the backing memory. The GART address is only used by PCI
	 * devices. 
	 */
	clear_kernel_mapping((unsigned long)__va(iommu_bus_base), iommu_size);

	/* 
	 * Try to workaround a bug (thanks to BenH) 
	 * Set unmapped entries to a scratch page instead of 0. 
	 * Any prefetches that hit unmapped entries won't get an bus abort
	 * then.
	 */
	scratch = get_zeroed_page(GFP_KERNEL); 
	if (!scratch) 
		panic("Cannot allocate iommu scratch page");
	gart_unmapped_entry = GPTE_ENCODE(__pa(scratch));
	for (i = EMERGENCY_PAGES; i < iommu_pages; i++) 
		iommu_gatt_base[i] = gart_unmapped_entry;

	flush_gart();
	dma_ops = &gart_dma_ops;
} 

void __init gart_parse_options(char *p)
{
	int arg;

#ifdef CONFIG_IOMMU_LEAK
	if (!strncmp(p,"leak",4)) {
		leak_trace = 1;
		p += 4;
		if (*p == '=') ++p;
		if (isdigit(*p) && get_option(&p, &arg))
			iommu_leak_pages = arg;
	}
#endif
	if (isdigit(*p) && get_option(&p, &arg))
		iommu_size = arg;
	if (!strncmp(p, "fullflush",8))
		iommu_fullflush = 1;
	if (!strncmp(p, "nofullflush",11))
		iommu_fullflush = 0;
	if (!strncmp(p,"noagp",5))
		no_agp = 1;
	if (!strncmp(p, "noaperture",10))
		fix_aperture = 0;
	/* duplicated from pci-dma.c */
	if (!strncmp(p,"force",5))
		iommu_aperture_allowed = 1;
	if (!strncmp(p,"allowed",7))
		iommu_aperture_allowed = 1;
	if (!strncmp(p, "memaper", 7)) {
		fallback_aper_force = 1;
		p += 7;
		if (*p == '=') {
			++p;
			if (get_option(&p, &arg))
				fallback_aper_order = arg;
		}
	}
}
