/*
 * Copyright 2010 Tilera Corporation. All Rights Reserved.
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation, version 2.
 *
 *   This program is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
 *   NON INFRINGEMENT.  See the GNU General Public License for
 *   more details.
 */

#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mmzone.h>
#include <linux/bootmem.h>
#include <linux/module.h>
#include <linux/node.h>
#include <linux/cpu.h>
#include <linux/ioport.h>
#include <linux/irq.h>
#include <linux/kexec.h>
#include <linux/pci.h>
#include <linux/initrd.h>
#include <linux/io.h>
#include <linux/highmem.h>
#include <linux/smp.h>
#include <linux/timex.h>
#include <asm/setup.h>
#include <asm/sections.h>
#include <asm/sections.h>
#include <asm/cacheflush.h>
#include <asm/cacheflush.h>
#include <asm/pgalloc.h>
#include <asm/mmu_context.h>
#include <hv/hypervisor.h>
#include <arch/interrupts.h>

/* <linux/smp.h> doesn't provide this definition. */
#ifndef CONFIG_SMP
#define setup_max_cpus 1
#endif

static inline int ABS(int x) { return x >= 0 ? x : -x; }

/* Chip information */
char chip_model[64] __write_once;

struct pglist_data node_data[MAX_NUMNODES] __read_mostly;
EXPORT_SYMBOL(node_data);

/* We only create bootmem data on node 0. */
static bootmem_data_t __initdata node0_bdata;

/* Information on the NUMA nodes that we compute early */
unsigned long __cpuinitdata node_start_pfn[MAX_NUMNODES];
unsigned long __cpuinitdata node_end_pfn[MAX_NUMNODES];
unsigned long __initdata node_memmap_pfn[MAX_NUMNODES];
unsigned long __initdata node_percpu_pfn[MAX_NUMNODES];
unsigned long __initdata node_free_pfn[MAX_NUMNODES];

#ifdef CONFIG_HIGHMEM
/* Page frame index of end of lowmem on each controller. */
unsigned long __cpuinitdata node_lowmem_end_pfn[MAX_NUMNODES];

/* Number of pages that can be mapped into lowmem. */
static unsigned long __initdata mappable_physpages;
#endif

/* Data on which physical memory controller corresponds to which NUMA node */
int node_controller[MAX_NUMNODES] = { [0 ... MAX_NUMNODES-1] = -1 };

#ifdef CONFIG_HIGHMEM
/* Map information from VAs to PAs */
unsigned long pbase_map[1 << (32 - HPAGE_SHIFT)]
  __write_once __attribute__((aligned(L2_CACHE_BYTES)));
EXPORT_SYMBOL(pbase_map);

/* Map information from PAs to VAs */
void *vbase_map[NR_PA_HIGHBIT_VALUES]
  __write_once __attribute__((aligned(L2_CACHE_BYTES)));
EXPORT_SYMBOL(vbase_map);
#endif

/* Node number as a function of the high PA bits */
int highbits_to_node[NR_PA_HIGHBIT_VALUES] __write_once;
EXPORT_SYMBOL(highbits_to_node);

static unsigned int __initdata maxmem_pfn = -1U;
static unsigned int __initdata maxnodemem_pfn[MAX_NUMNODES] = {
	[0 ... MAX_NUMNODES-1] = -1U
};
static nodemask_t __initdata isolnodes;

#ifdef CONFIG_PCI
enum { DEFAULT_PCI_RESERVE_MB = 64 };
static unsigned int __initdata pci_reserve_mb = DEFAULT_PCI_RESERVE_MB;
unsigned long __initdata pci_reserve_start_pfn = -1U;
unsigned long __initdata pci_reserve_end_pfn = -1U;
#endif

static int __init setup_maxmem(char *str)
{
	long maxmem_mb;
	if (str == NULL || strict_strtol(str, 0, &maxmem_mb) != 0 ||
	    maxmem_mb == 0)
		return -EINVAL;

	maxmem_pfn = (maxmem_mb >> (HPAGE_SHIFT - 20)) <<
		(HPAGE_SHIFT - PAGE_SHIFT);
	pr_info("Forcing RAM used to no more than %dMB\n",
	       maxmem_pfn >> (20 - PAGE_SHIFT));
	return 0;
}
early_param("maxmem", setup_maxmem);

static int __init setup_maxnodemem(char *str)
{
	char *endp;
	long maxnodemem_mb, node;

	node = str ? simple_strtoul(str, &endp, 0) : INT_MAX;
	if (node >= MAX_NUMNODES || *endp != ':' ||
	    strict_strtol(endp+1, 0, &maxnodemem_mb) != 0)
		return -EINVAL;

	maxnodemem_pfn[node] = (maxnodemem_mb >> (HPAGE_SHIFT - 20)) <<
		(HPAGE_SHIFT - PAGE_SHIFT);
	pr_info("Forcing RAM used on node %ld to no more than %dMB\n",
	       node, maxnodemem_pfn[node] >> (20 - PAGE_SHIFT));
	return 0;
}
early_param("maxnodemem", setup_maxnodemem);

static int __init setup_isolnodes(char *str)
{
	char buf[MAX_NUMNODES * 5];
	if (str == NULL || nodelist_parse(str, isolnodes) != 0)
		return -EINVAL;

	nodelist_scnprintf(buf, sizeof(buf), isolnodes);
	pr_info("Set isolnodes value to '%s'\n", buf);
	return 0;
}
early_param("isolnodes", setup_isolnodes);

#ifdef CONFIG_PCI
static int __init setup_pci_reserve(char* str)
{
	unsigned long mb;

	if (str == NULL || strict_strtoul(str, 0, &mb) != 0 ||
	    mb > 3 * 1024)
		return -EINVAL;

	pci_reserve_mb = mb;
	pr_info("Reserving %dMB for PCIE root complex mappings\n",
	       pci_reserve_mb);
	return 0;
}
early_param("pci_reserve", setup_pci_reserve);
#endif

#ifndef __tilegx__
/*
 * vmalloc=size forces the vmalloc area to be exactly 'size' bytes.
 * This can be used to increase (or decrease) the vmalloc area.
 */
static int __init parse_vmalloc(char *arg)
{
	if (!arg)
		return -EINVAL;

	VMALLOC_RESERVE = (memparse(arg, &arg) + PGDIR_SIZE - 1) & PGDIR_MASK;

	/* See validate_va() for more on this test. */
	if ((long)_VMALLOC_START >= 0)
		early_panic("\"vmalloc=%#lx\" value too large: maximum %#lx\n",
			    VMALLOC_RESERVE, _VMALLOC_END - 0x80000000UL);

	return 0;
}
early_param("vmalloc", parse_vmalloc);
#endif

#ifdef CONFIG_HIGHMEM
/*
 * Determine for each controller where its lowmem is mapped and how
 * much of it is mapped there.  On controller zero, the first few
 * megabytes are mapped at 0xfd000000 as code, so in principle we
 * could start our data mappings higher up, but for now we don't
 * bother, to avoid additional confusion.
 *
 * One question is whether, on systems with more than 768 Mb and
 * controllers of different sizes, to map in a proportionate amount of
 * each one, or to try to map the same amount from each controller.
 * (E.g. if we have three controllers with 256MB, 1GB, and 256MB
 * respectively, do we map 256MB from each, or do we map 128 MB, 512
 * MB, and 128 MB respectively?)  For now we use a proportionate
 * solution like the latter.
 *
 * The VA/PA mapping demands that we align our decisions at 16 MB
 * boundaries so that we can rapidly convert VA to PA.
 */
static void *__init setup_pa_va_mapping(void)
{
	unsigned long curr_pages = 0;
	unsigned long vaddr = PAGE_OFFSET;
	nodemask_t highonlynodes = isolnodes;
	int i, j;

	memset(pbase_map, -1, sizeof(pbase_map));
	memset(vbase_map, -1, sizeof(vbase_map));

	/* Node zero cannot be isolated for LOWMEM purposes. */
	node_clear(0, highonlynodes);

	/* Count up the number of pages on non-highonlynodes controllers. */
	mappable_physpages = 0;
	for_each_online_node(i) {
		if (!node_isset(i, highonlynodes))
			mappable_physpages +=
				node_end_pfn[i] - node_start_pfn[i];
	}

	for_each_online_node(i) {
		unsigned long start = node_start_pfn[i];
		unsigned long end = node_end_pfn[i];
		unsigned long size = end - start;
		unsigned long vaddr_end;

		if (node_isset(i, highonlynodes)) {
			/* Mark this controller as having no lowmem. */
			node_lowmem_end_pfn[i] = start;
			continue;
		}

		curr_pages += size;
		if (mappable_physpages > MAXMEM_PFN) {
			vaddr_end = PAGE_OFFSET +
				(((u64)curr_pages * MAXMEM_PFN /
				  mappable_physpages)
				 << PAGE_SHIFT);
		} else {
			vaddr_end = PAGE_OFFSET + (curr_pages << PAGE_SHIFT);
		}
		for (j = 0; vaddr < vaddr_end; vaddr += HPAGE_SIZE, ++j) {
			unsigned long this_pfn =
				start + (j << HUGETLB_PAGE_ORDER);
			pbase_map[vaddr >> HPAGE_SHIFT] = this_pfn;
			if (vbase_map[__pfn_to_highbits(this_pfn)] ==
			    (void *)-1)
				vbase_map[__pfn_to_highbits(this_pfn)] =
					(void *)(vaddr & HPAGE_MASK);
		}
		node_lowmem_end_pfn[i] = start + (j << HUGETLB_PAGE_ORDER);
		BUG_ON(node_lowmem_end_pfn[i] > end);
	}

	/* Return highest address of any mapped memory. */
	return (void *)vaddr;
}
#endif /* CONFIG_HIGHMEM */

/*
 * Register our most important memory mappings with the debug stub.
 *
 * This is up to 4 mappings for lowmem, one mapping per memory
 * controller, plus one for our text segment.
 */
static void __cpuinit store_permanent_mappings(void)
{
	int i;

	for_each_online_node(i) {
		HV_PhysAddr pa = ((HV_PhysAddr)node_start_pfn[i]) << PAGE_SHIFT;
#ifdef CONFIG_HIGHMEM
		HV_PhysAddr high_mapped_pa = node_lowmem_end_pfn[i];
#else
		HV_PhysAddr high_mapped_pa = node_end_pfn[i];
#endif

		unsigned long pages = high_mapped_pa - node_start_pfn[i];
		HV_VirtAddr addr = (HV_VirtAddr) __va(pa);
		hv_store_mapping(addr, pages << PAGE_SHIFT, pa);
	}

	hv_store_mapping((HV_VirtAddr)_stext,
			 (uint32_t)(_einittext - _stext), 0);
}

/*
 * Use hv_inquire_physical() to populate node_{start,end}_pfn[]
 * and node_online_map, doing suitable sanity-checking.
 * Also set min_low_pfn, max_low_pfn, and max_pfn.
 */
static void __init setup_memory(void)
{
	int i, j;
	int highbits_seen[NR_PA_HIGHBIT_VALUES] = { 0 };
#ifdef CONFIG_HIGHMEM
	long highmem_pages;
#endif
#ifndef __tilegx__
	int cap;
#endif
#if defined(CONFIG_HIGHMEM) || defined(__tilegx__)
	long lowmem_pages;
#endif

	/* We are using a char to hold the cpu_2_node[] mapping */
	BUG_ON(MAX_NUMNODES > 127);

	/* Discover the ranges of memory available to us */
	for (i = 0; ; ++i) {
		unsigned long start, size, end, highbits;
		HV_PhysAddrRange range = hv_inquire_physical(i);
		if (range.size == 0)
			break;
#ifdef CONFIG_FLATMEM
		if (i > 0) {
			pr_err("Can't use discontiguous PAs: %#llx..%#llx\n",
			       range.size, range.start + range.size);
			continue;
		}
#endif
#ifndef __tilegx__
		if ((unsigned long)range.start) {
			pr_err("Range not at 4GB multiple: %#llx..%#llx\n",
			       range.start, range.start + range.size);
			continue;
		}
#endif
		if ((range.start & (HPAGE_SIZE-1)) != 0 ||
		    (range.size & (HPAGE_SIZE-1)) != 0) {
			unsigned long long start_pa = range.start;
			unsigned long long orig_size = range.size;
			range.start = (start_pa + HPAGE_SIZE - 1) & HPAGE_MASK;
			range.size -= (range.start - start_pa);
			range.size &= HPAGE_MASK;
			pr_err("Range not hugepage-aligned: %#llx..%#llx:"
			       " now %#llx-%#llx\n",
			       start_pa, start_pa + orig_size,
			       range.start, range.start + range.size);
		}
		highbits = __pa_to_highbits(range.start);
		if (highbits >= NR_PA_HIGHBIT_VALUES) {
			pr_err("PA high bits too high: %#llx..%#llx\n",
			       range.start, range.start + range.size);
			continue;
		}
		if (highbits_seen[highbits]) {
			pr_err("Range overlaps in high bits: %#llx..%#llx\n",
			       range.start, range.start + range.size);
			continue;
		}
		highbits_seen[highbits] = 1;
		if (PFN_DOWN(range.size) > maxnodemem_pfn[i]) {
			int max_size = maxnodemem_pfn[i];
			if (max_size > 0) {
				pr_err("Maxnodemem reduced node %d to"
				       " %d pages\n", i, max_size);
				range.size = PFN_PHYS(max_size);
			} else {
				pr_err("Maxnodemem disabled node %d\n", i);
				continue;
			}
		}
		if (num_physpages + PFN_DOWN(range.size) > maxmem_pfn) {
			int max_size = maxmem_pfn - num_physpages;
			if (max_size > 0) {
				pr_err("Maxmem reduced node %d to %d pages\n",
				       i, max_size);
				range.size = PFN_PHYS(max_size);
			} else {
				pr_err("Maxmem disabled node %d\n", i);
				continue;
			}
		}
		if (i >= MAX_NUMNODES) {
			pr_err("Too many PA nodes (#%d): %#llx...%#llx\n",
			       i, range.size, range.size + range.start);
			continue;
		}

		start = range.start >> PAGE_SHIFT;
		size = range.size >> PAGE_SHIFT;
		end = start + size;

#ifndef __tilegx__
		if (((HV_PhysAddr)end << PAGE_SHIFT) !=
		    (range.start + range.size)) {
			pr_err("PAs too high to represent: %#llx..%#llx\n",
			       range.start, range.start + range.size);
			continue;
		}
#endif
#ifdef CONFIG_PCI
		/*
		 * Blocks that overlap the pci reserved region must
		 * have enough space to hold the maximum percpu data
		 * region at the top of the range.  If there isn't
		 * enough space above the reserved region, just
		 * truncate the node.
		 */
		if (start <= pci_reserve_start_pfn &&
		    end > pci_reserve_start_pfn) {
			unsigned int per_cpu_size =
				__per_cpu_end - __per_cpu_start;
			unsigned int percpu_pages =
				NR_CPUS * (PFN_UP(per_cpu_size) >> PAGE_SHIFT);
			if (end < pci_reserve_end_pfn + percpu_pages) {
				end = pci_reserve_start_pfn;
				pr_err("PCI mapping region reduced node %d to"
				       " %ld pages\n", i, end - start);
			}
		}
#endif

		for (j = __pfn_to_highbits(start);
		     j <= __pfn_to_highbits(end - 1); j++)
			highbits_to_node[j] = i;

		node_start_pfn[i] = start;
		node_end_pfn[i] = end;
		node_controller[i] = range.controller;
		num_physpages += size;
		max_pfn = end;

		/* Mark node as online */
		node_set(i, node_online_map);
		node_set(i, node_possible_map);
	}

#ifndef __tilegx__
	/*
	 * For 4KB pages, mem_map "struct page" data is 1% of the size
	 * of the physical memory, so can be quite big (640 MB for
	 * four 16G zones).  These structures must be mapped in
	 * lowmem, and since we currently cap out at about 768 MB,
	 * it's impractical to try to use this much address space.
	 * For now, arbitrarily cap the amount of physical memory
	 * we're willing to use at 8 million pages (32GB of 4KB pages).
	 */
	cap = 8 * 1024 * 1024;  /* 8 million pages */
	if (num_physpages > cap) {
		int num_nodes = num_online_nodes();
		int cap_each = cap / num_nodes;
		unsigned long dropped_pages = 0;
		for (i = 0; i < num_nodes; ++i) {
			int size = node_end_pfn[i] - node_start_pfn[i];
			if (size > cap_each) {
				dropped_pages += (size - cap_each);
				node_end_pfn[i] = node_start_pfn[i] + cap_each;
			}
		}
		num_physpages -= dropped_pages;
		pr_warning("Only using %ldMB memory;"
		       " ignoring %ldMB.\n",
		       num_physpages >> (20 - PAGE_SHIFT),
		       dropped_pages >> (20 - PAGE_SHIFT));
		pr_warning("Consider using a larger page size.\n");
	}
#endif

	/* Heap starts just above the last loaded address. */
	min_low_pfn = PFN_UP((unsigned long)_end - PAGE_OFFSET);

#ifdef CONFIG_HIGHMEM
	/* Find where we map lowmem from each controller. */
	high_memory = setup_pa_va_mapping();

	/* Set max_low_pfn based on what node 0 can directly address. */
	max_low_pfn = node_lowmem_end_pfn[0];

	lowmem_pages = (mappable_physpages > MAXMEM_PFN) ?
		MAXMEM_PFN : mappable_physpages;
	highmem_pages = (long) (num_physpages - lowmem_pages);

	pr_notice("%ldMB HIGHMEM available.\n",
	       pages_to_mb(highmem_pages > 0 ? highmem_pages : 0));
	pr_notice("%ldMB LOWMEM available.\n",
			pages_to_mb(lowmem_pages));
#else
	/* Set max_low_pfn based on what node 0 can directly address. */
	max_low_pfn = node_end_pfn[0];

#ifndef __tilegx__
	if (node_end_pfn[0] > MAXMEM_PFN) {
		pr_warning("Only using %ldMB LOWMEM.\n",
		       MAXMEM>>20);
		pr_warning("Use a HIGHMEM enabled kernel.\n");
		max_low_pfn = MAXMEM_PFN;
		max_pfn = MAXMEM_PFN;
		num_physpages = MAXMEM_PFN;
		node_end_pfn[0] = MAXMEM_PFN;
	} else {
		pr_notice("%ldMB memory available.\n",
		       pages_to_mb(node_end_pfn[0]));
	}
	for (i = 1; i < MAX_NUMNODES; ++i) {
		node_start_pfn[i] = 0;
		node_end_pfn[i] = 0;
	}
	high_memory = __va(node_end_pfn[0]);
#else
	lowmem_pages = 0;
	for (i = 0; i < MAX_NUMNODES; ++i) {
		int pages = node_end_pfn[i] - node_start_pfn[i];
		lowmem_pages += pages;
		if (pages)
			high_memory = pfn_to_kaddr(node_end_pfn[i]);
	}
	pr_notice("%ldMB memory available.\n",
	       pages_to_mb(lowmem_pages));
#endif
#endif
}

static void __init setup_bootmem_allocator(void)
{
	unsigned long bootmap_size, first_alloc_pfn, last_alloc_pfn;

	/* Provide a node 0 bdata. */
	NODE_DATA(0)->bdata = &node0_bdata;

#ifdef CONFIG_PCI
	/* Don't let boot memory alias the PCI region. */
	last_alloc_pfn = min(max_low_pfn, pci_reserve_start_pfn);
#else
	last_alloc_pfn = max_low_pfn;
#endif

	/*
	 * Initialize the boot-time allocator (with low memory only):
	 * The first argument says where to put the bitmap, and the
	 * second says where the end of allocatable memory is.
	 */
	bootmap_size = init_bootmem(min_low_pfn, last_alloc_pfn);

	/*
	 * Let the bootmem allocator use all the space we've given it
	 * except for its own bitmap.
	 */
	first_alloc_pfn = min_low_pfn + PFN_UP(bootmap_size);
	if (first_alloc_pfn >= last_alloc_pfn)
		early_panic("Not enough memory on controller 0 for bootmem\n");

	free_bootmem(PFN_PHYS(first_alloc_pfn),
		     PFN_PHYS(last_alloc_pfn - first_alloc_pfn));

#ifdef CONFIG_KEXEC
	if (crashk_res.start != crashk_res.end)
		reserve_bootmem(crashk_res.start,
			crashk_res.end - crashk_res.start + 1, 0);
#endif

}

void *__init alloc_remap(int nid, unsigned long size)
{
	int pages = node_end_pfn[nid] - node_start_pfn[nid];
	void *map = pfn_to_kaddr(node_memmap_pfn[nid]);
	BUG_ON(size != pages * sizeof(struct page));
	memset(map, 0, size);
	return map;
}

static int __init percpu_size(void)
{
	int size = ALIGN(__per_cpu_end - __per_cpu_start, PAGE_SIZE);
#ifdef CONFIG_MODULES
	if (size < PERCPU_ENOUGH_ROOM)
		size = PERCPU_ENOUGH_ROOM;
#endif
	/* In several places we assume the per-cpu data fits on a huge page. */
	BUG_ON(kdata_huge && size > HPAGE_SIZE);
	return size;
}

static inline unsigned long alloc_bootmem_pfn(int size, unsigned long goal)
{
	void *kva = __alloc_bootmem(size, PAGE_SIZE, goal);
	unsigned long pfn = kaddr_to_pfn(kva);
	BUG_ON(goal && PFN_PHYS(pfn) != goal);
	return pfn;
}

static void __init zone_sizes_init(void)
{
	unsigned long zones_size[MAX_NR_ZONES] = { 0 };
	unsigned long node_percpu[MAX_NUMNODES] = { 0 };
	int size = percpu_size();
	int num_cpus = smp_height * smp_width;
	int i;

	for (i = 0; i < num_cpus; ++i)
		node_percpu[cpu_to_node(i)] += size;

	for_each_online_node(i) {
		unsigned long start = node_start_pfn[i];
		unsigned long end = node_end_pfn[i];
#ifdef CONFIG_HIGHMEM
		unsigned long lowmem_end = node_lowmem_end_pfn[i];
#else
		unsigned long lowmem_end = end;
#endif
		int memmap_size = (end - start) * sizeof(struct page);
		node_free_pfn[i] = start;

		/*
		 * Set aside pages for per-cpu data and the mem_map array.
		 *
		 * Since the per-cpu data requires special homecaching,
		 * if we are in kdata_huge mode, we put it at the end of
		 * the lowmem region.  If we're not in kdata_huge mode,
		 * we take the per-cpu pages from the bottom of the
		 * controller, since that avoids fragmenting a huge page
		 * that users might want.  We always take the memmap
		 * from the bottom of the controller, since with
		 * kdata_huge that lets it be under a huge TLB entry.
		 *
		 * If the user has requested isolnodes for a controller,
		 * though, there'll be no lowmem, so we just alloc_bootmem
		 * the memmap.  There will be no percpu memory either.
		 */
		if (__pfn_to_highbits(start) == 0) {
			/* In low PAs, allocate via bootmem. */
			unsigned long goal = 0;
			node_memmap_pfn[i] =
				alloc_bootmem_pfn(memmap_size, goal);
			if (kdata_huge)
				goal = PFN_PHYS(lowmem_end) - node_percpu[i];
			if (node_percpu[i])
				node_percpu_pfn[i] =
				    alloc_bootmem_pfn(node_percpu[i], goal);
		} else if (cpu_isset(i, isolnodes)) {
			node_memmap_pfn[i] = alloc_bootmem_pfn(memmap_size, 0);
			BUG_ON(node_percpu[i] != 0);
		} else {
			/* In high PAs, just reserve some pages. */
			node_memmap_pfn[i] = node_free_pfn[i];
			node_free_pfn[i] += PFN_UP(memmap_size);
			if (!kdata_huge) {
				node_percpu_pfn[i] = node_free_pfn[i];
				node_free_pfn[i] += PFN_UP(node_percpu[i]);
			} else {
				node_percpu_pfn[i] =
					lowmem_end - PFN_UP(node_percpu[i]);
			}
		}

#ifdef CONFIG_HIGHMEM
		if (start > lowmem_end) {
			zones_size[ZONE_NORMAL] = 0;
			zones_size[ZONE_HIGHMEM] = end - start;
		} else {
			zones_size[ZONE_NORMAL] = lowmem_end - start;
			zones_size[ZONE_HIGHMEM] = end - lowmem_end;
		}
#else
		zones_size[ZONE_NORMAL] = end - start;
#endif

		/*
		 * Everyone shares node 0's bootmem allocator, but
		 * we use alloc_remap(), above, to put the actual
		 * struct page array on the individual controllers,
		 * which is most of the data that we actually care about.
		 * We can't place bootmem allocators on the other
		 * controllers since the bootmem allocator can only
		 * operate on 32-bit physical addresses.
		 */
		NODE_DATA(i)->bdata = NODE_DATA(0)->bdata;

		free_area_init_node(i, zones_size, start, NULL);
		printk(KERN_DEBUG "  DMA zone: %ld per-cpu pages\n",
		       PFN_UP(node_percpu[i]));

		/* Track the type of memory on each node */
		if (zones_size[ZONE_NORMAL])
			node_set_state(i, N_NORMAL_MEMORY);
#ifdef CONFIG_HIGHMEM
		if (end != start)
			node_set_state(i, N_HIGH_MEMORY);
#endif

		node_set_online(i);
	}
}

#ifdef CONFIG_NUMA

/* which logical CPUs are on which nodes */
struct cpumask node_2_cpu_mask[MAX_NUMNODES] __write_once;
EXPORT_SYMBOL(node_2_cpu_mask);

/* which node each logical CPU is on */
char cpu_2_node[NR_CPUS] __write_once __attribute__((aligned(L2_CACHE_BYTES)));
EXPORT_SYMBOL(cpu_2_node);

/* Return cpu_to_node() except for cpus not yet assigned, which return -1 */
static int __init cpu_to_bound_node(int cpu, struct cpumask* unbound_cpus)
{
	if (!cpu_possible(cpu) || cpumask_test_cpu(cpu, unbound_cpus))
		return -1;
	else
		return cpu_to_node(cpu);
}

/* Return number of immediately-adjacent tiles sharing the same NUMA node. */
static int __init node_neighbors(int node, int cpu,
				 struct cpumask *unbound_cpus)
{
	int neighbors = 0;
	int w = smp_width;
	int h = smp_height;
	int x = cpu % w;
	int y = cpu / w;
	if (x > 0 && cpu_to_bound_node(cpu-1, unbound_cpus) == node)
		++neighbors;
	if (x < w-1 && cpu_to_bound_node(cpu+1, unbound_cpus) == node)
		++neighbors;
	if (y > 0 && cpu_to_bound_node(cpu-w, unbound_cpus) == node)
		++neighbors;
	if (y < h-1 && cpu_to_bound_node(cpu+w, unbound_cpus) == node)
		++neighbors;
	return neighbors;
}

static void __init setup_numa_mapping(void)
{
	int distance[MAX_NUMNODES][NR_CPUS];
	HV_Coord coord;
	int cpu, node, cpus, i, x, y;
	int num_nodes = num_online_nodes();
	struct cpumask unbound_cpus;
	nodemask_t default_nodes;

	cpumask_clear(&unbound_cpus);

	/* Get set of nodes we will use for defaults */
	nodes_andnot(default_nodes, node_online_map, isolnodes);
	if (nodes_empty(default_nodes)) {
		BUG_ON(!node_isset(0, node_online_map));
		pr_err("Forcing NUMA node zero available as a default node\n");
		node_set(0, default_nodes);
	}

	/* Populate the distance[] array */
	memset(distance, -1, sizeof(distance));
	cpu = 0;
	for (coord.y = 0; coord.y < smp_height; ++coord.y) {
		for (coord.x = 0; coord.x < smp_width;
		     ++coord.x, ++cpu) {
			BUG_ON(cpu >= nr_cpu_ids);
			if (!cpu_possible(cpu)) {
				cpu_2_node[cpu] = -1;
				continue;
			}
			for_each_node_mask(node, default_nodes) {
				HV_MemoryControllerInfo info =
					hv_inquire_memory_controller(
						coord, node_controller[node]);
				distance[node][cpu] =
					ABS(info.coord.x) + ABS(info.coord.y);
			}
			cpumask_set_cpu(cpu, &unbound_cpus);
		}
	}
	cpus = cpu;

	/*
	 * Round-robin through the NUMA nodes until all the cpus are
	 * assigned.  We could be more clever here (e.g. create four
	 * sorted linked lists on the same set of cpu nodes, and pull
	 * off them in round-robin sequence, removing from all four
	 * lists each time) but given the relatively small numbers
	 * involved, O(n^2) seem OK for a one-time cost.
	 */
	node = first_node(default_nodes);
	while (!cpumask_empty(&unbound_cpus)) {
		int best_cpu = -1;
		int best_distance = INT_MAX;
		for (cpu = 0; cpu < cpus; ++cpu) {
			if (cpumask_test_cpu(cpu, &unbound_cpus)) {
				/*
				 * Compute metric, which is how much
				 * closer the cpu is to this memory
				 * controller than the others, shifted
				 * up, and then the number of
				 * neighbors already in the node as an
				 * epsilon adjustment to try to keep
				 * the nodes compact.
				 */
				int d = distance[node][cpu] * num_nodes;
				for_each_node_mask(i, default_nodes) {
					if (i != node)
						d -= distance[i][cpu];
				}
				d *= 8;  /* allow space for epsilon */
				d -= node_neighbors(node, cpu, &unbound_cpus);
				if (d < best_distance) {
					best_cpu = cpu;
					best_distance = d;
				}
			}
		}
		BUG_ON(best_cpu < 0);
		cpumask_set_cpu(best_cpu, &node_2_cpu_mask[node]);
		cpu_2_node[best_cpu] = node;
		cpumask_clear_cpu(best_cpu, &unbound_cpus);
		node = next_node(node, default_nodes);
		if (node == MAX_NUMNODES)
			node = first_node(default_nodes);
	}

	/* Print out node assignments and set defaults for disabled cpus */
	cpu = 0;
	for (y = 0; y < smp_height; ++y) {
		printk(KERN_DEBUG "NUMA cpu-to-node row %d:", y);
		for (x = 0; x < smp_width; ++x, ++cpu) {
			if (cpu_to_node(cpu) < 0) {
				pr_cont(" -");
				cpu_2_node[cpu] = first_node(default_nodes);
			} else {
				pr_cont(" %d", cpu_to_node(cpu));
			}
		}
		pr_cont("\n");
	}
}

static struct cpu cpu_devices[NR_CPUS];

static int __init topology_init(void)
{
	int i;

	for_each_online_node(i)
		register_one_node(i);

	for_each_present_cpu(i)
		register_cpu(&cpu_devices[i], i);

	return 0;
}

subsys_initcall(topology_init);

#else /* !CONFIG_NUMA */

#define setup_numa_mapping() do { } while (0)

#endif /* CONFIG_NUMA */

/**
 * setup_cpu() - Do all necessary per-cpu, tile-specific initialization.
 * @boot: Is this the boot cpu?
 *
 * Called from setup_arch() on the boot cpu, or online_secondary().
 */
void __cpuinit setup_cpu(int boot)
{
	/* The boot cpu sets up its permanent mappings much earlier. */
	if (!boot)
		store_permanent_mappings();

	/* Allow asynchronous TLB interrupts. */
#if CHIP_HAS_TILE_DMA()
	raw_local_irq_unmask(INT_DMATLB_MISS);
	raw_local_irq_unmask(INT_DMATLB_ACCESS);
#endif
#if CHIP_HAS_SN_PROC()
	raw_local_irq_unmask(INT_SNITLB_MISS);
#endif

	/*
	 * Allow user access to many generic SPRs, like the cycle
	 * counter, PASS/FAIL/DONE, INTERRUPT_CRITICAL_SECTION, etc.
	 */
	__insn_mtspr(SPR_MPL_WORLD_ACCESS_SET_0, 1);

#if CHIP_HAS_SN()
	/* Static network is not restricted. */
	__insn_mtspr(SPR_MPL_SN_ACCESS_SET_0, 1);
#endif
#if CHIP_HAS_SN_PROC()
	__insn_mtspr(SPR_MPL_SN_NOTIFY_SET_0, 1);
	__insn_mtspr(SPR_MPL_SN_CPL_SET_0, 1);
#endif

	/*
	 * Set the MPL for interrupt control 0 to user level.
	 * This includes access to the SYSTEM_SAVE and EX_CONTEXT SPRs,
	 * as well as the PL 0 interrupt mask.
	 */
	__insn_mtspr(SPR_MPL_INTCTRL_0_SET_0, 1);

	/* Initialize IRQ support for this cpu. */
	setup_irq_regs();

#ifdef CONFIG_HARDWALL
	/* Reset the network state on this cpu. */
	reset_network_state();
#endif
}

static int __initdata set_initramfs_file;
static char __initdata initramfs_file[128] = "initramfs.cpio.gz";

static int __init setup_initramfs_file(char *str)
{
	if (str == NULL)
		return -EINVAL;
	strncpy(initramfs_file, str, sizeof(initramfs_file) - 1);
	set_initramfs_file = 1;

	return 0;
}
early_param("initramfs_file", setup_initramfs_file);

/*
 * We look for an additional "initramfs.cpio.gz" file in the hvfs.
 * If there is one, we allocate some memory for it and it will be
 * unpacked to the initramfs after any built-in initramfs_data.
 */
static void __init load_hv_initrd(void)
{
	HV_FS_StatInfo stat;
	int fd, rc;
	void *initrd;

	fd = hv_fs_findfile((HV_VirtAddr) initramfs_file);
	if (fd == HV_ENOENT) {
		if (set_initramfs_file)
			pr_warning("No such hvfs initramfs file '%s'\n",
				   initramfs_file);
		return;
	}
	BUG_ON(fd < 0);
	stat = hv_fs_fstat(fd);
	BUG_ON(stat.size < 0);
	if (stat.flags & HV_FS_ISDIR) {
		pr_warning("Ignoring hvfs file '%s': it's a directory.\n",
			   initramfs_file);
		return;
	}
	initrd = alloc_bootmem_pages(stat.size);
	rc = hv_fs_pread(fd, (HV_VirtAddr) initrd, stat.size, 0);
	if (rc != stat.size) {
		pr_err("Error reading %d bytes from hvfs file '%s': %d\n",
		       stat.size, initramfs_file, rc);
		free_initrd_mem((unsigned long) initrd, stat.size);
		return;
	}
	initrd_start = (unsigned long) initrd;
	initrd_end = initrd_start + stat.size;
}

void __init free_initrd_mem(unsigned long begin, unsigned long end)
{
	free_bootmem(__pa(begin), end - begin);
}

static void __init validate_hv(void)
{
	/*
	 * It may already be too late, but let's check our built-in
	 * configuration against what the hypervisor is providing.
	 */
	unsigned long glue_size = hv_sysconf(HV_SYSCONF_GLUE_SIZE);
	int hv_page_size = hv_sysconf(HV_SYSCONF_PAGE_SIZE_SMALL);
	int hv_hpage_size = hv_sysconf(HV_SYSCONF_PAGE_SIZE_LARGE);
	HV_ASIDRange asid_range;

#ifndef CONFIG_SMP
	HV_Topology topology = hv_inquire_topology();
	BUG_ON(topology.coord.x != 0 || topology.coord.y != 0);
	if (topology.width != 1 || topology.height != 1) {
		pr_warning("Warning: booting UP kernel on %dx%d grid;"
			   " will ignore all but first tile.\n",
			   topology.width, topology.height);
	}
#endif

	if (PAGE_OFFSET + HV_GLUE_START_CPA + glue_size > (unsigned long)_text)
		early_panic("Hypervisor glue size %ld is too big!\n",
			    glue_size);
	if (hv_page_size != PAGE_SIZE)
		early_panic("Hypervisor page size %#x != our %#lx\n",
			    hv_page_size, PAGE_SIZE);
	if (hv_hpage_size != HPAGE_SIZE)
		early_panic("Hypervisor huge page size %#x != our %#lx\n",
			    hv_hpage_size, HPAGE_SIZE);

#ifdef CONFIG_SMP
	/*
	 * Some hypervisor APIs take a pointer to a bitmap array
	 * whose size is at least the number of cpus on the chip.
	 * We use a struct cpumask for this, so it must be big enough.
	 */
	if ((smp_height * smp_width) > nr_cpu_ids)
		early_panic("Hypervisor %d x %d grid too big for Linux"
			    " NR_CPUS %d\n", smp_height, smp_width,
			    nr_cpu_ids);
#endif

	/*
	 * Check that we're using allowed ASIDs, and initialize the
	 * various asid variables to their appropriate initial states.
	 */
	asid_range = hv_inquire_asid(0);
	__get_cpu_var(current_asid) = min_asid = asid_range.start;
	max_asid = asid_range.start + asid_range.size - 1;

	if (hv_confstr(HV_CONFSTR_CHIP_MODEL, (HV_VirtAddr)chip_model,
		       sizeof(chip_model)) < 0) {
		pr_err("Warning: HV_CONFSTR_CHIP_MODEL not available\n");
		strlcpy(chip_model, "unknown", sizeof(chip_model));
	}
}

static void __init validate_va(void)
{
#ifndef __tilegx__
	/*
	 * Similarly, make sure we're only using allowed VAs.
	 * We assume we can contiguously use MEM_USER_INTRPT .. MEM_HV_INTRPT,
	 * and 0 .. KERNEL_HIGH_VADDR.
	 * In addition, make sure we CAN'T use the end of memory, since
	 * we use the last chunk of each pgd for the pgd_list.
	 */
	int i, fc_fd_ok = 0;
	unsigned long max_va = 0;
	unsigned long list_va =
		((PGD_LIST_OFFSET / sizeof(pgd_t)) << PGDIR_SHIFT);

	for (i = 0; ; ++i) {
		HV_VirtAddrRange range = hv_inquire_virtual(i);
		if (range.size == 0)
			break;
		if (range.start <= MEM_USER_INTRPT &&
		    range.start + range.size >= MEM_HV_INTRPT)
			fc_fd_ok = 1;
		if (range.start == 0)
			max_va = range.size;
		BUG_ON(range.start + range.size > list_va);
	}
	if (!fc_fd_ok)
		early_panic("Hypervisor not configured for VAs 0xfc/0xfd\n");
	if (max_va == 0)
		early_panic("Hypervisor not configured for low VAs\n");
	if (max_va < KERNEL_HIGH_VADDR)
		early_panic("Hypervisor max VA %#lx smaller than %#lx\n",
			    max_va, KERNEL_HIGH_VADDR);

	/* Kernel PCs must have their high bit set; see intvec.S. */
	if ((long)VMALLOC_START >= 0)
		early_panic(
			"Linux VMALLOC region below the 2GB line (%#lx)!\n"
			"Reconfigure the kernel with fewer NR_HUGE_VMAPS\n"
			"or smaller VMALLOC_RESERVE.\n",
			VMALLOC_START);
#endif
}

/*
 * cpu_lotar_map lists all the cpus that are valid for the supervisor
 * to cache data on at a page level, i.e. what cpus can be placed in
 * the LOTAR field of a PTE.  It is equivalent to the set of possible
 * cpus plus any other cpus that are willing to share their cache.
 * It is set by hv_inquire_tiles(HV_INQ_TILES_LOTAR).
 */
struct cpumask __write_once cpu_lotar_map;
EXPORT_SYMBOL(cpu_lotar_map);

#if CHIP_HAS_CBOX_HOME_MAP()
/*
 * hash_for_home_map lists all the tiles that hash-for-home data
 * will be cached on.  Note that this may includes tiles that are not
 * valid for this supervisor to use otherwise (e.g. if a hypervisor
 * device is being shared between multiple supervisors).
 * It is set by hv_inquire_tiles(HV_INQ_TILES_HFH_CACHE).
 */
struct cpumask hash_for_home_map;
EXPORT_SYMBOL(hash_for_home_map);
#endif

/*
 * cpu_cacheable_map lists all the cpus whose caches the hypervisor can
 * flush on our behalf.  It is set to cpu_possible_map OR'ed with
 * hash_for_home_map, and it is what should be passed to
 * hv_flush_remote() to flush all caches.  Note that if there are
 * dedicated hypervisor driver tiles that have authorized use of their
 * cache, those tiles will only appear in cpu_lotar_map, NOT in
 * cpu_cacheable_map, as they are a special case.
 */
struct cpumask __write_once cpu_cacheable_map;
EXPORT_SYMBOL(cpu_cacheable_map);

static __initdata struct cpumask disabled_map;

static int __init disabled_cpus(char *str)
{
	int boot_cpu = smp_processor_id();

	if (str == NULL || cpulist_parse_crop(str, &disabled_map) != 0)
		return -EINVAL;
	if (cpumask_test_cpu(boot_cpu, &disabled_map)) {
		pr_err("disabled_cpus: can't disable boot cpu %d\n", boot_cpu);
		cpumask_clear_cpu(boot_cpu, &disabled_map);
	}
	return 0;
}

early_param("disabled_cpus", disabled_cpus);

void __init print_disabled_cpus(void)
{
	if (!cpumask_empty(&disabled_map)) {
		char buf[100];
		cpulist_scnprintf(buf, sizeof(buf), &disabled_map);
		pr_info("CPUs not available for Linux: %s\n", buf);
	}
}

static void __init setup_cpu_maps(void)
{
	struct cpumask hv_disabled_map, cpu_possible_init;
	int boot_cpu = smp_processor_id();
	int cpus, i, rc;

	/* Learn which cpus are allowed by the hypervisor. */
	rc = hv_inquire_tiles(HV_INQ_TILES_AVAIL,
			      (HV_VirtAddr) cpumask_bits(&cpu_possible_init),
			      sizeof(cpu_cacheable_map));
	if (rc < 0)
		early_panic("hv_inquire_tiles(AVAIL) failed: rc %d\n", rc);
	if (!cpumask_test_cpu(boot_cpu, &cpu_possible_init))
		early_panic("Boot CPU %d disabled by hypervisor!\n", boot_cpu);

	/* Compute the cpus disabled by the hvconfig file. */
	cpumask_complement(&hv_disabled_map, &cpu_possible_init);

	/* Include them with the cpus disabled by "disabled_cpus". */
	cpumask_or(&disabled_map, &disabled_map, &hv_disabled_map);

	/*
	 * Disable every cpu after "setup_max_cpus".  But don't mark
	 * as disabled the cpus that are outside of our initial rectangle,
	 * since that turns out to be confusing.
	 */
	cpus = 1;                          /* this cpu */
	cpumask_set_cpu(boot_cpu, &disabled_map);   /* ignore this cpu */
	for (i = 0; cpus < setup_max_cpus; ++i)
		if (!cpumask_test_cpu(i, &disabled_map))
			++cpus;
	for (; i < smp_height * smp_width; ++i)
		cpumask_set_cpu(i, &disabled_map);
	cpumask_clear_cpu(boot_cpu, &disabled_map); /* reset this cpu */
	for (i = smp_height * smp_width; i < NR_CPUS; ++i)
		cpumask_clear_cpu(i, &disabled_map);

	/*
	 * Setup cpu_possible map as every cpu allocated to us, minus
	 * the results of any "disabled_cpus" settings.
	 */
	cpumask_andnot(&cpu_possible_init, &cpu_possible_init, &disabled_map);
	init_cpu_possible(&cpu_possible_init);

	/* Learn which cpus are valid for LOTAR caching. */
	rc = hv_inquire_tiles(HV_INQ_TILES_LOTAR,
			      (HV_VirtAddr) cpumask_bits(&cpu_lotar_map),
			      sizeof(cpu_lotar_map));
	if (rc < 0) {
		pr_err("warning: no HV_INQ_TILES_LOTAR; using AVAIL\n");
		cpu_lotar_map = cpu_possible_map;
	}

#if CHIP_HAS_CBOX_HOME_MAP()
	/* Retrieve set of CPUs used for hash-for-home caching */
	rc = hv_inquire_tiles(HV_INQ_TILES_HFH_CACHE,
			      (HV_VirtAddr) hash_for_home_map.bits,
			      sizeof(hash_for_home_map));
	if (rc < 0)
		early_panic("hv_inquire_tiles(HFH_CACHE) failed: rc %d\n", rc);
	cpumask_or(&cpu_cacheable_map, &cpu_possible_map, &hash_for_home_map);
#else
	cpu_cacheable_map = cpu_possible_map;
#endif
}


static int __init dataplane(char *str)
{
	pr_warning("WARNING: dataplane support disabled in this kernel\n");
	return 0;
}

early_param("dataplane", dataplane);

#ifdef CONFIG_CMDLINE_BOOL
static char __initdata builtin_cmdline[COMMAND_LINE_SIZE] = CONFIG_CMDLINE;
#endif

void __init setup_arch(char **cmdline_p)
{
	int len;

#if defined(CONFIG_CMDLINE_BOOL) && defined(CONFIG_CMDLINE_OVERRIDE)
	len = hv_get_command_line((HV_VirtAddr) boot_command_line,
				  COMMAND_LINE_SIZE);
	if (boot_command_line[0])
		pr_warning("WARNING: ignoring dynamic command line \"%s\"\n",
			   boot_command_line);
	strlcpy(boot_command_line, builtin_cmdline, COMMAND_LINE_SIZE);
#else
	char *hv_cmdline;
#if defined(CONFIG_CMDLINE_BOOL)
	if (builtin_cmdline[0]) {
		int builtin_len = strlcpy(boot_command_line, builtin_cmdline,
					  COMMAND_LINE_SIZE);
		if (builtin_len < COMMAND_LINE_SIZE-1)
			boot_command_line[builtin_len++] = ' ';
		hv_cmdline = &boot_command_line[builtin_len];
		len = COMMAND_LINE_SIZE - builtin_len;
	} else
#endif
	{
		hv_cmdline = boot_command_line;
		len = COMMAND_LINE_SIZE;
	}
	len = hv_get_command_line((HV_VirtAddr) hv_cmdline, len);
	if (len < 0 || len > COMMAND_LINE_SIZE)
		early_panic("hv_get_command_line failed: %d\n", len);
#endif

	*cmdline_p = boot_command_line;

	/* Set disabled_map and setup_max_cpus very early */
	parse_early_param();

	/* Make sure the kernel is compatible with the hypervisor. */
	validate_hv();
	validate_va();

	setup_cpu_maps();


#ifdef CONFIG_PCI
	/*
	 * Initialize the PCI structures.  This is done before memory
	 * setup so that we know whether or not a pci_reserve region
	 * is necessary.
	 */
	if (tile_pci_init() == 0)
		pci_reserve_mb = 0;

	/* PCI systems reserve a region just below 4GB for mapping iomem. */
	pci_reserve_end_pfn  = (1 << (32 - PAGE_SHIFT));
	pci_reserve_start_pfn = pci_reserve_end_pfn -
		(pci_reserve_mb << (20 - PAGE_SHIFT));
#endif

	init_mm.start_code = (unsigned long) _text;
	init_mm.end_code = (unsigned long) _etext;
	init_mm.end_data = (unsigned long) _edata;
	init_mm.brk = (unsigned long) _end;

	setup_memory();
	store_permanent_mappings();
	setup_bootmem_allocator();

	/*
	 * NOTE: before this point _nobody_ is allowed to allocate
	 * any memory using the bootmem allocator.
	 */

	paging_init();
	setup_numa_mapping();
	zone_sizes_init();
	set_page_homes();
	setup_cpu(1);
	setup_clock();
	load_hv_initrd();
}


/*
 * Set up per-cpu memory.
 */

unsigned long __per_cpu_offset[NR_CPUS] __write_once;
EXPORT_SYMBOL(__per_cpu_offset);

static size_t __initdata pfn_offset[MAX_NUMNODES] = { 0 };
static unsigned long __initdata percpu_pfn[NR_CPUS] = { 0 };

/*
 * As the percpu code allocates pages, we return the pages from the
 * end of the node for the specified cpu.
 */
static void *__init pcpu_fc_alloc(unsigned int cpu, size_t size, size_t align)
{
	int nid = cpu_to_node(cpu);
	unsigned long pfn = node_percpu_pfn[nid] + pfn_offset[nid];

	BUG_ON(size % PAGE_SIZE != 0);
	pfn_offset[nid] += size / PAGE_SIZE;
	if (percpu_pfn[cpu] == 0)
		percpu_pfn[cpu] = pfn;
	return pfn_to_kaddr(pfn);
}

/*
 * Pages reserved for percpu memory are not freeable, and in any case we are
 * on a short path to panic() in setup_per_cpu_area() at this point anyway.
 */
static void __init pcpu_fc_free(void *ptr, size_t size)
{
}

/*
 * Set up vmalloc page tables using bootmem for the percpu code.
 */
static void __init pcpu_fc_populate_pte(unsigned long addr)
{
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;

	BUG_ON(pgd_addr_invalid(addr));

	pgd = swapper_pg_dir + pgd_index(addr);
	pud = pud_offset(pgd, addr);
	BUG_ON(!pud_present(*pud));
	pmd = pmd_offset(pud, addr);
	if (pmd_present(*pmd)) {
		BUG_ON(pmd_huge_page(*pmd));
	} else {
		pte = __alloc_bootmem(L2_KERNEL_PGTABLE_SIZE,
				      HV_PAGE_TABLE_ALIGN, 0);
		pmd_populate_kernel(&init_mm, pmd, pte);
	}
}

void __init setup_per_cpu_areas(void)
{
	struct page *pg;
	unsigned long delta, pfn, lowmem_va;
	unsigned long size = percpu_size();
	char *ptr;
	int rc, cpu, i;

	rc = pcpu_page_first_chunk(PERCPU_MODULE_RESERVE, pcpu_fc_alloc,
				   pcpu_fc_free, pcpu_fc_populate_pte);
	if (rc < 0)
		panic("Cannot initialize percpu area (err=%d)", rc);

	delta = (unsigned long)pcpu_base_addr - (unsigned long)__per_cpu_start;
	for_each_possible_cpu(cpu) {
		__per_cpu_offset[cpu] = delta + pcpu_unit_offsets[cpu];

		/* finv the copy out of cache so we can change homecache */
		ptr = pcpu_base_addr + pcpu_unit_offsets[cpu];
		__finv_buffer(ptr, size);
		pfn = percpu_pfn[cpu];

		/* Rewrite the page tables to cache on that cpu */
		pg = pfn_to_page(pfn);
		for (i = 0; i < size; i += PAGE_SIZE, ++pfn, ++pg) {

			/* Update the vmalloc mapping and page home. */
			pte_t *ptep =
				virt_to_pte(NULL, (unsigned long)ptr + i);
			pte_t pte = *ptep;
			BUG_ON(pfn != pte_pfn(pte));
			pte = hv_pte_set_mode(pte, HV_PTE_MODE_CACHE_TILE_L3);
			pte = set_remote_cache_cpu(pte, cpu);
			set_pte(ptep, pte);

			/* Update the lowmem mapping for consistency. */
			lowmem_va = (unsigned long)pfn_to_kaddr(pfn);
			ptep = virt_to_pte(NULL, lowmem_va);
			if (pte_huge(*ptep)) {
				printk(KERN_DEBUG "early shatter of huge page"
				       " at %#lx\n", lowmem_va);
				shatter_pmd((pmd_t *)ptep);
				ptep = virt_to_pte(NULL, lowmem_va);
				BUG_ON(pte_huge(*ptep));
			}
			BUG_ON(pfn != pte_pfn(*ptep));
			set_pte(ptep, pte);
		}
	}

	/* Set our thread pointer appropriately. */
	set_my_cpu_offset(__per_cpu_offset[smp_processor_id()]);

	/* Make sure the finv's have completed. */
	mb_incoherent();

	/* Flush the TLB so we reference it properly from here on out. */
	local_flush_tlb_all();
}

static struct resource data_resource = {
	.name	= "Kernel data",
	.start	= 0,
	.end	= 0,
	.flags	= IORESOURCE_BUSY | IORESOURCE_MEM
};

static struct resource code_resource = {
	.name	= "Kernel code",
	.start	= 0,
	.end	= 0,
	.flags	= IORESOURCE_BUSY | IORESOURCE_MEM
};

/*
 * We reserve all resources above 4GB so that PCI won't try to put
 * mappings above 4GB; the standard allows that for some devices but
 * the probing code trunates values to 32 bits.
 */
#ifdef CONFIG_PCI
static struct resource* __init
insert_non_bus_resource(void)
{
	struct resource *res =
		kzalloc(sizeof(struct resource), GFP_ATOMIC);
	res->name = "Non-Bus Physical Address Space";
	res->start = (1ULL << 32);
	res->end = -1LL;
	res->flags = IORESOURCE_BUSY | IORESOURCE_MEM;
	if (insert_resource(&iomem_resource, res)) {
		kfree(res);
		return NULL;
	}
	return res;
}
#endif

static struct resource* __init
insert_ram_resource(u64 start_pfn, u64 end_pfn)
{
	struct resource *res =
		kzalloc(sizeof(struct resource), GFP_ATOMIC);
	res->name = "System RAM";
	res->start = start_pfn << PAGE_SHIFT;
	res->end = (end_pfn << PAGE_SHIFT) - 1;
	res->flags = IORESOURCE_BUSY | IORESOURCE_MEM;
	if (insert_resource(&iomem_resource, res)) {
		kfree(res);
		return NULL;
	}
	return res;
}

/*
 * Request address space for all standard resources
 *
 * If the system includes PCI root complex drivers, we need to create
 * a window just below 4GB where PCI BARs can be mapped.
 */
static int __init request_standard_resources(void)
{
	int i;
	enum { CODE_DELTA = MEM_SV_INTRPT - PAGE_OFFSET };

	iomem_resource.end = -1LL;
#ifdef CONFIG_PCI
	insert_non_bus_resource();
#endif

	for_each_online_node(i) {
		u64 start_pfn = node_start_pfn[i];
		u64 end_pfn = node_end_pfn[i];

#ifdef CONFIG_PCI
		if (start_pfn <= pci_reserve_start_pfn &&
		    end_pfn > pci_reserve_start_pfn) {
			if (end_pfn > pci_reserve_end_pfn)
				insert_ram_resource(pci_reserve_end_pfn,
						     end_pfn);
			end_pfn = pci_reserve_start_pfn;
		}
#endif
		insert_ram_resource(start_pfn, end_pfn);
	}

	code_resource.start = __pa(_text - CODE_DELTA);
	code_resource.end = __pa(_etext - CODE_DELTA)-1;
	data_resource.start = __pa(_sdata);
	data_resource.end = __pa(_end)-1;

	insert_resource(&iomem_resource, &code_resource);
	insert_resource(&iomem_resource, &data_resource);

#ifdef CONFIG_KEXEC
	insert_resource(&iomem_resource, &crashk_res);
#endif

	return 0;
}

subsys_initcall(request_standard_resources);
