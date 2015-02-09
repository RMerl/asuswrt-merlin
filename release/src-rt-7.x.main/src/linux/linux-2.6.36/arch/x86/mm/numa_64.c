/*
 * Generic VM initialization for x86-64 NUMA setups.
 * Copyright 2002,2003 Andi Kleen, SuSE Labs.
 */
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/bootmem.h>
#include <linux/mmzone.h>
#include <linux/ctype.h>
#include <linux/module.h>
#include <linux/nodemask.h>
#include <linux/sched.h>

#include <asm/e820.h>
#include <asm/proto.h>
#include <asm/dma.h>
#include <asm/numa.h>
#include <asm/acpi.h>
#include <asm/k8.h>

struct pglist_data *node_data[MAX_NUMNODES] __read_mostly;
EXPORT_SYMBOL(node_data);

struct memnode memnode;

s16 apicid_to_node[MAX_LOCAL_APIC] __cpuinitdata = {
	[0 ... MAX_LOCAL_APIC-1] = NUMA_NO_NODE
};

int numa_off __initdata;
static unsigned long __initdata nodemap_addr;
static unsigned long __initdata nodemap_size;

/*
 * Map cpu index to node index
 */
DEFINE_EARLY_PER_CPU(int, x86_cpu_to_node_map, NUMA_NO_NODE);
EXPORT_EARLY_PER_CPU_SYMBOL(x86_cpu_to_node_map);

/*
 * Given a shift value, try to populate memnodemap[]
 * Returns :
 * 1 if OK
 * 0 if memnodmap[] too small (of shift too small)
 * -1 if node overlap or lost ram (shift too big)
 */
static int __init populate_memnodemap(const struct bootnode *nodes,
				      int numnodes, int shift, int *nodeids)
{
	unsigned long addr, end;
	int i, res = -1;

	memset(memnodemap, 0xff, sizeof(s16)*memnodemapsize);
	for (i = 0; i < numnodes; i++) {
		addr = nodes[i].start;
		end = nodes[i].end;
		if (addr >= end)
			continue;
		if ((end >> shift) >= memnodemapsize)
			return 0;
		do {
			if (memnodemap[addr >> shift] != NUMA_NO_NODE)
				return -1;

			if (!nodeids)
				memnodemap[addr >> shift] = i;
			else
				memnodemap[addr >> shift] = nodeids[i];

			addr += (1UL << shift);
		} while (addr < end);
		res = 1;
	}
	return res;
}

static int __init allocate_cachealigned_memnodemap(void)
{
	unsigned long addr;

	memnodemap = memnode.embedded_map;
	if (memnodemapsize <= ARRAY_SIZE(memnode.embedded_map))
		return 0;

	addr = 0x8000;
	nodemap_size = roundup(sizeof(s16) * memnodemapsize, L1_CACHE_BYTES);
	nodemap_addr = find_e820_area(addr, max_pfn<<PAGE_SHIFT,
				      nodemap_size, L1_CACHE_BYTES);
	if (nodemap_addr == -1UL) {
		printk(KERN_ERR
		       "NUMA: Unable to allocate Memory to Node hash map\n");
		nodemap_addr = nodemap_size = 0;
		return -1;
	}
	memnodemap = phys_to_virt(nodemap_addr);
	reserve_early(nodemap_addr, nodemap_addr + nodemap_size, "MEMNODEMAP");

	printk(KERN_DEBUG "NUMA: Allocated memnodemap from %lx - %lx\n",
	       nodemap_addr, nodemap_addr + nodemap_size);
	return 0;
}

/*
 * The LSB of all start and end addresses in the node map is the value of the
 * maximum possible shift.
 */
static int __init extract_lsb_from_nodes(const struct bootnode *nodes,
					 int numnodes)
{
	int i, nodes_used = 0;
	unsigned long start, end;
	unsigned long bitfield = 0, memtop = 0;

	for (i = 0; i < numnodes; i++) {
		start = nodes[i].start;
		end = nodes[i].end;
		if (start >= end)
			continue;
		bitfield |= start;
		nodes_used++;
		if (end > memtop)
			memtop = end;
	}
	if (nodes_used <= 1)
		i = 63;
	else
		i = find_first_bit(&bitfield, sizeof(unsigned long)*8);
	memnodemapsize = (memtop >> i)+1;
	return i;
}

int __init compute_hash_shift(struct bootnode *nodes, int numnodes,
			      int *nodeids)
{
	int shift;

	shift = extract_lsb_from_nodes(nodes, numnodes);
	if (allocate_cachealigned_memnodemap())
		return -1;
	printk(KERN_DEBUG "NUMA: Using %d for the hash shift.\n",
		shift);

	if (populate_memnodemap(nodes, numnodes, shift, nodeids) != 1) {
		printk(KERN_INFO "Your memory is not aligned you need to "
		       "rebuild your kernel with a bigger NODEMAPSIZE "
		       "shift=%d\n", shift);
		return -1;
	}
	return shift;
}

int __meminit  __early_pfn_to_nid(unsigned long pfn)
{
	return phys_to_nid(pfn << PAGE_SHIFT);
}

static void * __init early_node_mem(int nodeid, unsigned long start,
				    unsigned long end, unsigned long size,
				    unsigned long align)
{
	unsigned long mem;

	/*
	 * put it on high as possible
	 * something will go with NODE_DATA
	 */
	if (start < (MAX_DMA_PFN<<PAGE_SHIFT))
		start = MAX_DMA_PFN<<PAGE_SHIFT;
	if (start < (MAX_DMA32_PFN<<PAGE_SHIFT) &&
	    end > (MAX_DMA32_PFN<<PAGE_SHIFT))
		start = MAX_DMA32_PFN<<PAGE_SHIFT;
	mem = find_e820_area(start, end, size, align);
	if (mem != -1L)
		return __va(mem);

	/* extend the search scope */
	end = max_pfn_mapped << PAGE_SHIFT;
	if (end > (MAX_DMA32_PFN<<PAGE_SHIFT))
		start = MAX_DMA32_PFN<<PAGE_SHIFT;
	else
		start = MAX_DMA_PFN<<PAGE_SHIFT;
	mem = find_e820_area(start, end, size, align);
	if (mem != -1L)
		return __va(mem);

	printk(KERN_ERR "Cannot find %lu bytes in node %d\n",
		       size, nodeid);

	return NULL;
}

/* Initialize bootmem allocator for a node */
void __init
setup_node_bootmem(int nodeid, unsigned long start, unsigned long end)
{
	unsigned long start_pfn, last_pfn, nodedata_phys;
	const int pgdat_size = roundup(sizeof(pg_data_t), PAGE_SIZE);
	int nid;
#ifndef CONFIG_NO_BOOTMEM
	unsigned long bootmap_start, bootmap_pages, bootmap_size;
	void *bootmap;
#endif

	if (!end)
		return;

	/*
	 * Don't confuse VM with a node that doesn't have the
	 * minimum amount of memory:
	 */
	if (end && (end - start) < NODE_MIN_SIZE)
		return;

	start = roundup(start, ZONE_ALIGN);

	printk(KERN_INFO "Initmem setup node %d %016lx-%016lx\n", nodeid,
	       start, end);

	start_pfn = start >> PAGE_SHIFT;
	last_pfn = end >> PAGE_SHIFT;

	node_data[nodeid] = early_node_mem(nodeid, start, end, pgdat_size,
					   SMP_CACHE_BYTES);
	if (node_data[nodeid] == NULL)
		return;
	nodedata_phys = __pa(node_data[nodeid]);
	reserve_early(nodedata_phys, nodedata_phys + pgdat_size, "NODE_DATA");
	printk(KERN_INFO "  NODE_DATA [%016lx - %016lx]\n", nodedata_phys,
		nodedata_phys + pgdat_size - 1);
	nid = phys_to_nid(nodedata_phys);
	if (nid != nodeid)
		printk(KERN_INFO "    NODE_DATA(%d) on node %d\n", nodeid, nid);

	memset(NODE_DATA(nodeid), 0, sizeof(pg_data_t));
	NODE_DATA(nodeid)->node_id = nodeid;
	NODE_DATA(nodeid)->node_start_pfn = start_pfn;
	NODE_DATA(nodeid)->node_spanned_pages = last_pfn - start_pfn;

#ifndef CONFIG_NO_BOOTMEM
	NODE_DATA(nodeid)->bdata = &bootmem_node_data[nodeid];

	/*
	 * Find a place for the bootmem map
	 * nodedata_phys could be on other nodes by alloc_bootmem,
	 * so need to sure bootmap_start not to be small, otherwise
	 * early_node_mem will get that with find_e820_area instead
	 * of alloc_bootmem, that could clash with reserved range
	 */
	bootmap_pages = bootmem_bootmap_pages(last_pfn - start_pfn);
	bootmap_start = roundup(nodedata_phys + pgdat_size, PAGE_SIZE);
	/*
	 * SMP_CACHE_BYTES could be enough, but init_bootmem_node like
	 * to use that to align to PAGE_SIZE
	 */
	bootmap = early_node_mem(nodeid, bootmap_start, end,
				 bootmap_pages<<PAGE_SHIFT, PAGE_SIZE);
	if (bootmap == NULL)  {
		free_early(nodedata_phys, nodedata_phys + pgdat_size);
		node_data[nodeid] = NULL;
		return;
	}
	bootmap_start = __pa(bootmap);
	reserve_early(bootmap_start, bootmap_start+(bootmap_pages<<PAGE_SHIFT),
			"BOOTMAP");

	bootmap_size = init_bootmem_node(NODE_DATA(nodeid),
					 bootmap_start >> PAGE_SHIFT,
					 start_pfn, last_pfn);

	printk(KERN_INFO "  bootmap [%016lx -  %016lx] pages %lx\n",
		 bootmap_start, bootmap_start + bootmap_size - 1,
		 bootmap_pages);
	nid = phys_to_nid(bootmap_start);
	if (nid != nodeid)
		printk(KERN_INFO "    bootmap(%d) on node %d\n", nodeid, nid);

	free_bootmem_with_active_regions(nodeid, end);
#endif

	node_set_online(nodeid);
}

/*
 * There are unfortunately some poorly designed mainboards around that
 * only connect memory to a single CPU. This breaks the 1:1 cpu->node
 * mapping. To avoid this fill in the mapping for all possible CPUs,
 * as the number of CPUs is not known yet. We round robin the existing
 * nodes.
 */
void __init numa_init_array(void)
{
	int rr, i;

	rr = first_node(node_online_map);
	for (i = 0; i < nr_cpu_ids; i++) {
		if (early_cpu_to_node(i) != NUMA_NO_NODE)
			continue;
		numa_set_node(i, rr);
		rr = next_node(rr, node_online_map);
		if (rr == MAX_NUMNODES)
			rr = first_node(node_online_map);
	}
}

#ifdef CONFIG_NUMA_EMU
/* Numa emulation */
static struct bootnode nodes[MAX_NUMNODES] __initdata;
static struct bootnode physnodes[MAX_NUMNODES] __initdata;
static char *cmdline __initdata;

static int __init setup_physnodes(unsigned long start, unsigned long end,
					int acpi, int k8)
{
	int nr_nodes = 0;
	int ret = 0;
	int i;

#ifdef CONFIG_ACPI_NUMA
	if (acpi)
		nr_nodes = acpi_get_nodes(physnodes);
#endif
#ifdef CONFIG_K8_NUMA
	if (k8)
		nr_nodes = k8_get_nodes(physnodes);
#endif
	/*
	 * Basic sanity checking on the physical node map: there may be errors
	 * if the SRAT or K8 incorrectly reported the topology or the mem=
	 * kernel parameter is used.
	 */
	for (i = 0; i < nr_nodes; i++) {
		if (physnodes[i].start == physnodes[i].end)
			continue;
		if (physnodes[i].start > end) {
			physnodes[i].end = physnodes[i].start;
			continue;
		}
		if (physnodes[i].end < start) {
			physnodes[i].start = physnodes[i].end;
			continue;
		}
		if (physnodes[i].start < start)
			physnodes[i].start = start;
		if (physnodes[i].end > end)
			physnodes[i].end = end;
	}

	/*
	 * Remove all nodes that have no memory or were truncated because of the
	 * limited address range.
	 */
	for (i = 0; i < nr_nodes; i++) {
		if (physnodes[i].start == physnodes[i].end)
			continue;
		physnodes[ret].start = physnodes[i].start;
		physnodes[ret].end = physnodes[i].end;
		ret++;
	}

	/*
	 * If no physical topology was detected, a single node is faked to cover
	 * the entire address space.
	 */
	if (!ret) {
		physnodes[ret].start = start;
		physnodes[ret].end = end;
		ret = 1;
	}
	return ret;
}

/*
 * Setups up nid to range from addr to addr + size.  If the end
 * boundary is greater than max_addr, then max_addr is used instead.
 * The return value is 0 if there is additional memory left for
 * allocation past addr and -1 otherwise.  addr is adjusted to be at
 * the end of the node.
 */
static int __init setup_node_range(int nid, u64 *addr, u64 size, u64 max_addr)
{
	int ret = 0;
	nodes[nid].start = *addr;
	*addr += size;
	if (*addr >= max_addr) {
		*addr = max_addr;
		ret = -1;
	}
	nodes[nid].end = *addr;
	node_set(nid, node_possible_map);
	printk(KERN_INFO "Faking node %d at %016Lx-%016Lx (%LuMB)\n", nid,
	       nodes[nid].start, nodes[nid].end,
	       (nodes[nid].end - nodes[nid].start) >> 20);
	return ret;
}

/*
 * Sets up nr_nodes fake nodes interleaved over physical nodes ranging from addr
 * to max_addr.  The return value is the number of nodes allocated.
 */
static int __init split_nodes_interleave(u64 addr, u64 max_addr,
						int nr_phys_nodes, int nr_nodes)
{
	nodemask_t physnode_mask = NODE_MASK_NONE;
	u64 size;
	int big;
	int ret = 0;
	int i;

	if (nr_nodes <= 0)
		return -1;
	if (nr_nodes > MAX_NUMNODES) {
		pr_info("numa=fake=%d too large, reducing to %d\n",
			nr_nodes, MAX_NUMNODES);
		nr_nodes = MAX_NUMNODES;
	}

	size = (max_addr - addr - e820_hole_size(addr, max_addr)) / nr_nodes;
	/*
	 * Calculate the number of big nodes that can be allocated as a result
	 * of consolidating the remainder.
	 */
	big = ((size & ~FAKE_NODE_MIN_HASH_MASK) * nr_nodes) /
		FAKE_NODE_MIN_SIZE;

	size &= FAKE_NODE_MIN_HASH_MASK;
	if (!size) {
		pr_err("Not enough memory for each node.  "
			"NUMA emulation disabled.\n");
		return -1;
	}

	for (i = 0; i < nr_phys_nodes; i++)
		if (physnodes[i].start != physnodes[i].end)
			node_set(i, physnode_mask);

	/*
	 * Continue to fill physical nodes with fake nodes until there is no
	 * memory left on any of them.
	 */
	while (nodes_weight(physnode_mask)) {
		for_each_node_mask(i, physnode_mask) {
			u64 end = physnodes[i].start + size;
			u64 dma32_end = PFN_PHYS(MAX_DMA32_PFN);

			if (ret < big)
				end += FAKE_NODE_MIN_SIZE;

			/*
			 * Continue to add memory to this fake node if its
			 * non-reserved memory is less than the per-node size.
			 */
			while (end - physnodes[i].start -
				e820_hole_size(physnodes[i].start, end) < size) {
				end += FAKE_NODE_MIN_SIZE;
				if (end > physnodes[i].end) {
					end = physnodes[i].end;
					break;
				}
			}

			/*
			 * If there won't be at least FAKE_NODE_MIN_SIZE of
			 * non-reserved memory in ZONE_DMA32 for the next node,
			 * this one must extend to the boundary.
			 */
			if (end < dma32_end && dma32_end - end -
			    e820_hole_size(end, dma32_end) < FAKE_NODE_MIN_SIZE)
				end = dma32_end;

			/*
			 * If there won't be enough non-reserved memory for the
			 * next node, this one must extend to the end of the
			 * physical node.
			 */
			if (physnodes[i].end - end -
			    e820_hole_size(end, physnodes[i].end) < size)
				end = physnodes[i].end;

			/*
			 * Avoid allocating more nodes than requested, which can
			 * happen as a result of rounding down each node's size
			 * to FAKE_NODE_MIN_SIZE.
			 */
			if (nodes_weight(physnode_mask) + ret >= nr_nodes)
				end = physnodes[i].end;

			if (setup_node_range(ret++, &physnodes[i].start,
						end - physnodes[i].start,
						physnodes[i].end) < 0)
				node_clear(i, physnode_mask);
		}
	}
	return ret;
}

/*
 * Returns the end address of a node so that there is at least `size' amount of
 * non-reserved memory or `max_addr' is reached.
 */
static u64 __init find_end_of_node(u64 start, u64 max_addr, u64 size)
{
	u64 end = start + size;

	while (end - start - e820_hole_size(start, end) < size) {
		end += FAKE_NODE_MIN_SIZE;
		if (end > max_addr) {
			end = max_addr;
			break;
		}
	}
	return end;
}

/*
 * Sets up fake nodes of `size' interleaved over physical nodes ranging from
 * `addr' to `max_addr'.  The return value is the number of nodes allocated.
 */
static int __init split_nodes_size_interleave(u64 addr, u64 max_addr, u64 size)
{
	nodemask_t physnode_mask = NODE_MASK_NONE;
	u64 min_size;
	int ret = 0;
	int i;

	if (!size)
		return -1;
	/*
	 * The limit on emulated nodes is MAX_NUMNODES, so the size per node is
	 * increased accordingly if the requested size is too small.  This
	 * creates a uniform distribution of node sizes across the entire
	 * machine (but not necessarily over physical nodes).
	 */
	min_size = (max_addr - addr - e820_hole_size(addr, max_addr)) /
						MAX_NUMNODES;
	min_size = max(min_size, FAKE_NODE_MIN_SIZE);
	if ((min_size & FAKE_NODE_MIN_HASH_MASK) < min_size)
		min_size = (min_size + FAKE_NODE_MIN_SIZE) &
						FAKE_NODE_MIN_HASH_MASK;
	if (size < min_size) {
		pr_err("Fake node size %LuMB too small, increasing to %LuMB\n",
			size >> 20, min_size >> 20);
		size = min_size;
	}
	size &= FAKE_NODE_MIN_HASH_MASK;

	for (i = 0; i < MAX_NUMNODES; i++)
		if (physnodes[i].start != physnodes[i].end)
			node_set(i, physnode_mask);
	/*
	 * Fill physical nodes with fake nodes of size until there is no memory
	 * left on any of them.
	 */
	while (nodes_weight(physnode_mask)) {
		for_each_node_mask(i, physnode_mask) {
			u64 dma32_end = MAX_DMA32_PFN << PAGE_SHIFT;
			u64 end;

			end = find_end_of_node(physnodes[i].start,
						physnodes[i].end, size);
			/*
			 * If there won't be at least FAKE_NODE_MIN_SIZE of
			 * non-reserved memory in ZONE_DMA32 for the next node,
			 * this one must extend to the boundary.
			 */
			if (end < dma32_end && dma32_end - end -
			    e820_hole_size(end, dma32_end) < FAKE_NODE_MIN_SIZE)
				end = dma32_end;

			/*
			 * If there won't be enough non-reserved memory for the
			 * next node, this one must extend to the end of the
			 * physical node.
			 */
			if (physnodes[i].end - end -
			    e820_hole_size(end, physnodes[i].end) < size)
				end = physnodes[i].end;

			/*
			 * Setup the fake node that will be allocated as bootmem
			 * later.  If setup_node_range() returns non-zero, there
			 * is no more memory available on this physical node.
			 */
			if (setup_node_range(ret++, &physnodes[i].start,
						end - physnodes[i].start,
						physnodes[i].end) < 0)
				node_clear(i, physnode_mask);
		}
	}
	return ret;
}

/*
 * Sets up the system RAM area from start_pfn to last_pfn according to the
 * numa=fake command-line option.
 */
static int __init numa_emulation(unsigned long start_pfn,
			unsigned long last_pfn, int acpi, int k8)
{
	u64 addr = start_pfn << PAGE_SHIFT;
	u64 max_addr = last_pfn << PAGE_SHIFT;
	int num_phys_nodes;
	int num_nodes;
	int i;

	num_phys_nodes = setup_physnodes(addr, max_addr, acpi, k8);
	/*
	 * If the numa=fake command-line contains a 'M' or 'G', it represents
	 * the fixed node size.  Otherwise, if it is just a single number N,
	 * split the system RAM into N fake nodes.
	 */
	if (strchr(cmdline, 'M') || strchr(cmdline, 'G')) {
		u64 size;

		size = memparse(cmdline, &cmdline);
		num_nodes = split_nodes_size_interleave(addr, max_addr, size);
	} else {
		unsigned long n;

		n = simple_strtoul(cmdline, NULL, 0);
		num_nodes = split_nodes_interleave(addr, max_addr, num_phys_nodes, n);
	}

	if (num_nodes < 0)
		return num_nodes;
	memnode_shift = compute_hash_shift(nodes, num_nodes, NULL);
	if (memnode_shift < 0) {
		memnode_shift = 0;
		printk(KERN_ERR "No NUMA hash function found.  NUMA emulation "
		       "disabled.\n");
		return -1;
	}

	/*
	 * We need to vacate all active ranges that may have been registered for
	 * the e820 memory map.
	 */
	remove_all_active_ranges();
	for_each_node_mask(i, node_possible_map) {
		e820_register_active_regions(i, nodes[i].start >> PAGE_SHIFT,
						nodes[i].end >> PAGE_SHIFT);
		setup_node_bootmem(i, nodes[i].start, nodes[i].end);
	}
	acpi_fake_nodes(nodes, num_nodes);
	numa_init_array();
	return 0;
}
#endif /* CONFIG_NUMA_EMU */

void __init initmem_init(unsigned long start_pfn, unsigned long last_pfn,
				int acpi, int k8)
{
	int i;

	nodes_clear(node_possible_map);
	nodes_clear(node_online_map);

#ifdef CONFIG_NUMA_EMU
	if (cmdline && !numa_emulation(start_pfn, last_pfn, acpi, k8))
		return;
	nodes_clear(node_possible_map);
	nodes_clear(node_online_map);
#endif

#ifdef CONFIG_ACPI_NUMA
	if (!numa_off && acpi && !acpi_scan_nodes(start_pfn << PAGE_SHIFT,
						  last_pfn << PAGE_SHIFT))
		return;
	nodes_clear(node_possible_map);
	nodes_clear(node_online_map);
#endif

#ifdef CONFIG_K8_NUMA
	if (!numa_off && k8 && !k8_scan_nodes())
		return;
	nodes_clear(node_possible_map);
	nodes_clear(node_online_map);
#endif
	printk(KERN_INFO "%s\n",
	       numa_off ? "NUMA turned off" : "No NUMA configuration found");

	printk(KERN_INFO "Faking a node at %016lx-%016lx\n",
	       start_pfn << PAGE_SHIFT,
	       last_pfn << PAGE_SHIFT);
	/* setup dummy node covering all memory */
	memnode_shift = 63;
	memnodemap = memnode.embedded_map;
	memnodemap[0] = 0;
	node_set_online(0);
	node_set(0, node_possible_map);
	for (i = 0; i < nr_cpu_ids; i++)
		numa_set_node(i, 0);
	e820_register_active_regions(0, start_pfn, last_pfn);
	setup_node_bootmem(0, start_pfn << PAGE_SHIFT, last_pfn << PAGE_SHIFT);
}

unsigned long __init numa_free_all_bootmem(void)
{
	unsigned long pages = 0;
	int i;

	for_each_online_node(i)
		pages += free_all_bootmem_node(NODE_DATA(i));

#ifdef CONFIG_NO_BOOTMEM
	pages += free_all_memory_core_early(MAX_NUMNODES);
#endif

	return pages;
}

static __init int numa_setup(char *opt)
{
	if (!opt)
		return -EINVAL;
	if (!strncmp(opt, "off", 3))
		numa_off = 1;
#ifdef CONFIG_NUMA_EMU
	if (!strncmp(opt, "fake=", 5))
		cmdline = opt + 5;
#endif
#ifdef CONFIG_ACPI_NUMA
	if (!strncmp(opt, "noacpi", 6))
		acpi_numa = -1;
#endif
	return 0;
}
early_param("numa", numa_setup);

#ifdef CONFIG_NUMA

static __init int find_near_online_node(int node)
{
	int n, val;
	int min_val = INT_MAX;
	int best_node = -1;

	for_each_online_node(n) {
		val = node_distance(node, n);

		if (val < min_val) {
			min_val = val;
			best_node = n;
		}
	}

	return best_node;
}

/*
 * Setup early cpu_to_node.
 *
 * Populate cpu_to_node[] only if x86_cpu_to_apicid[],
 * and apicid_to_node[] tables have valid entries for a CPU.
 * This means we skip cpu_to_node[] initialisation for NUMA
 * emulation and faking node case (when running a kernel compiled
 * for NUMA on a non NUMA box), which is OK as cpu_to_node[]
 * is already initialized in a round robin manner at numa_init_array,
 * prior to this call, and this initialization is good enough
 * for the fake NUMA cases.
 *
 * Called before the per_cpu areas are setup.
 */
void __init init_cpu_to_node(void)
{
	int cpu;
	u16 *cpu_to_apicid = early_per_cpu_ptr(x86_cpu_to_apicid);

	BUG_ON(cpu_to_apicid == NULL);

	for_each_possible_cpu(cpu) {
		int node;
		u16 apicid = cpu_to_apicid[cpu];

		if (apicid == BAD_APICID)
			continue;
		node = apicid_to_node[apicid];
		if (node == NUMA_NO_NODE)
			continue;
		if (!node_online(node))
			node = find_near_online_node(node);
		numa_set_node(cpu, node);
	}
}
#endif


void __cpuinit numa_set_node(int cpu, int node)
{
	int *cpu_to_node_map = early_per_cpu_ptr(x86_cpu_to_node_map);

	/* early setting, no percpu area yet */
	if (cpu_to_node_map) {
		cpu_to_node_map[cpu] = node;
		return;
	}

#ifdef CONFIG_DEBUG_PER_CPU_MAPS
	if (cpu >= nr_cpu_ids || !cpu_possible(cpu)) {
		printk(KERN_ERR "numa_set_node: invalid cpu# (%d)\n", cpu);
		dump_stack();
		return;
	}
#endif
	per_cpu(x86_cpu_to_node_map, cpu) = node;

	if (node != NUMA_NO_NODE)
		set_cpu_numa_node(cpu, node);
}

void __cpuinit numa_clear_node(int cpu)
{
	numa_set_node(cpu, NUMA_NO_NODE);
}

#ifndef CONFIG_DEBUG_PER_CPU_MAPS

void __cpuinit numa_add_cpu(int cpu)
{
	cpumask_set_cpu(cpu, node_to_cpumask_map[early_cpu_to_node(cpu)]);
}

void __cpuinit numa_remove_cpu(int cpu)
{
	cpumask_clear_cpu(cpu, node_to_cpumask_map[early_cpu_to_node(cpu)]);
}

#else /* CONFIG_DEBUG_PER_CPU_MAPS */

/*
 * --------- debug versions of the numa functions ---------
 */
static void __cpuinit numa_set_cpumask(int cpu, int enable)
{
	int node = early_cpu_to_node(cpu);
	struct cpumask *mask;
	char buf[64];

	mask = node_to_cpumask_map[node];
	if (mask == NULL) {
		printk(KERN_ERR "node_to_cpumask_map[%i] NULL\n", node);
		dump_stack();
		return;
	}

	if (enable)
		cpumask_set_cpu(cpu, mask);
	else
		cpumask_clear_cpu(cpu, mask);

	cpulist_scnprintf(buf, sizeof(buf), mask);
	printk(KERN_DEBUG "%s cpu %d node %d: mask now %s\n",
		enable ? "numa_add_cpu" : "numa_remove_cpu", cpu, node, buf);
}

void __cpuinit numa_add_cpu(int cpu)
{
	numa_set_cpumask(cpu, 1);
}

void __cpuinit numa_remove_cpu(int cpu)
{
	numa_set_cpumask(cpu, 0);
}

int __cpu_to_node(int cpu)
{
	if (early_per_cpu_ptr(x86_cpu_to_node_map)) {
		printk(KERN_WARNING
			"cpu_to_node(%d): usage too early!\n", cpu);
		dump_stack();
		return early_per_cpu_ptr(x86_cpu_to_node_map)[cpu];
	}
	return per_cpu(x86_cpu_to_node_map, cpu);
}
EXPORT_SYMBOL(__cpu_to_node);

/*
 * Same function as cpu_to_node() but used if called before the
 * per_cpu areas are setup.
 */
int early_cpu_to_node(int cpu)
{
	if (early_per_cpu_ptr(x86_cpu_to_node_map))
		return early_per_cpu_ptr(x86_cpu_to_node_map)[cpu];

	if (!cpu_possible(cpu)) {
		printk(KERN_WARNING
			"early_cpu_to_node(%d): no per_cpu area!\n", cpu);
		dump_stack();
		return NUMA_NO_NODE;
	}
	return per_cpu(x86_cpu_to_node_map, cpu);
}

/*
 * --------- end of debug versions of the numa functions ---------
 */

#endif /* CONFIG_DEBUG_PER_CPU_MAPS */
