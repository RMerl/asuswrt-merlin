/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2000, 05 by Ralf Baechle (ralf@linux-mips.org)
 * Copyright (C) 2000 by Silicon Graphics, Inc.
 * Copyright (C) 2004 by Christoph Hellwig
 *
 * On SGI IP27 the ARC memory configuration data is completly bogus but
 * alternate easier to use mechanisms are available.
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/mmzone.h>
#include <linux/module.h>
#include <linux/nodemask.h>
#include <linux/swap.h>
#include <linux/bootmem.h>
#include <linux/pfn.h>
#include <linux/highmem.h>
#include <asm/page.h>
#include <asm/pgalloc.h>
#include <asm/sections.h>

#include <asm/sn/arch.h>
#include <asm/sn/hub.h>
#include <asm/sn/klconfig.h>
#include <asm/sn/sn_private.h>


#define SLOT_PFNSHIFT           (SLOT_SHIFT - PAGE_SHIFT)
#define PFN_NASIDSHFT           (NASID_SHFT - PAGE_SHIFT)

struct node_data *__node_data[MAX_COMPACT_NODES];

EXPORT_SYMBOL(__node_data);

static int fine_mode;

static int is_fine_dirmode(void)
{
	return (((LOCAL_HUB_L(NI_STATUS_REV_ID) & NSRI_REGIONSIZE_MASK)
	        >> NSRI_REGIONSIZE_SHFT) & REGIONSIZE_FINE);
}

static hubreg_t get_region(cnodeid_t cnode)
{
	if (fine_mode)
		return COMPACT_TO_NASID_NODEID(cnode) >> NASID_TO_FINEREG_SHFT;
	else
		return COMPACT_TO_NASID_NODEID(cnode) >> NASID_TO_COARSEREG_SHFT;
}

static hubreg_t region_mask;

static void gen_region_mask(hubreg_t *region_mask)
{
	cnodeid_t cnode;

	(*region_mask) = 0;
	for_each_online_node(cnode) {
		(*region_mask) |= 1ULL << get_region(cnode);
	}
}

#define	rou_rflag	rou_flags

static int router_distance;

static void router_recurse(klrou_t *router_a, klrou_t *router_b, int depth)
{
	klrou_t *router;
	lboard_t *brd;
	int	port;

	if (router_a->rou_rflag == 1)
		return;

	if (depth >= router_distance)
		return;

	router_a->rou_rflag = 1;

	for (port = 1; port <= MAX_ROUTER_PORTS; port++) {
		if (router_a->rou_port[port].port_nasid == INVALID_NASID)
			continue;

		brd = (lboard_t *)NODE_OFFSET_TO_K0(
			router_a->rou_port[port].port_nasid,
			router_a->rou_port[port].port_offset);

		if (brd->brd_type == KLTYPE_ROUTER) {
			router = (klrou_t *)NODE_OFFSET_TO_K0(NASID_GET(brd), brd->brd_compts[0]);
			if (router == router_b) {
				if (depth < router_distance)
					router_distance = depth;
			}
			else
				router_recurse(router, router_b, depth + 1);
		}
	}

	router_a->rou_rflag = 0;
}

unsigned char __node_distances[MAX_COMPACT_NODES][MAX_COMPACT_NODES];

static int __init compute_node_distance(nasid_t nasid_a, nasid_t nasid_b)
{
	klrou_t *router, *router_a = NULL, *router_b = NULL;
	lboard_t *brd, *dest_brd;
	cnodeid_t cnode;
	nasid_t nasid;
	int port;

	/* Figure out which routers nodes in question are connected to */
	for_each_online_node(cnode) {
		nasid = COMPACT_TO_NASID_NODEID(cnode);

		if (nasid == -1) continue;

		brd = find_lboard_class((lboard_t *)KL_CONFIG_INFO(nasid),
					KLTYPE_ROUTER);

		if (!brd)
			continue;

		do {
			if (brd->brd_flags & DUPLICATE_BOARD)
				continue;

			router = (klrou_t *)NODE_OFFSET_TO_K0(NASID_GET(brd), brd->brd_compts[0]);
			router->rou_rflag = 0;

			for (port = 1; port <= MAX_ROUTER_PORTS; port++) {
				if (router->rou_port[port].port_nasid == INVALID_NASID)
					continue;

				dest_brd = (lboard_t *)NODE_OFFSET_TO_K0(
					router->rou_port[port].port_nasid,
					router->rou_port[port].port_offset);

				if (dest_brd->brd_type == KLTYPE_IP27) {
					if (dest_brd->brd_nasid == nasid_a)
						router_a = router;
					if (dest_brd->brd_nasid == nasid_b)
						router_b = router;
				}
			}

		} while ((brd = find_lboard_class(KLCF_NEXT(brd), KLTYPE_ROUTER)));
	}

	if (router_a == NULL) {
		printk("node_distance: router_a NULL\n");
		return -1;
	}
	if (router_b == NULL) {
		printk("node_distance: router_b NULL\n");
		return -1;
	}

	if (nasid_a == nasid_b)
		return 0;

	if (router_a == router_b)
		return 1;

	router_distance = 100;
	router_recurse(router_a, router_b, 2);

	return router_distance;
}

static void __init init_topology_matrix(void)
{
	nasid_t nasid, nasid2;
	cnodeid_t row, col;

	for (row = 0; row < MAX_COMPACT_NODES; row++)
		for (col = 0; col < MAX_COMPACT_NODES; col++)
			__node_distances[row][col] = -1;

	for_each_online_node(row) {
		nasid = COMPACT_TO_NASID_NODEID(row);
		for_each_online_node(col) {
			nasid2 = COMPACT_TO_NASID_NODEID(col);
			__node_distances[row][col] =
				compute_node_distance(nasid, nasid2);
		}
	}
}

static void __init dump_topology(void)
{
	nasid_t nasid;
	cnodeid_t cnode;
	lboard_t *brd, *dest_brd;
	int port;
	int router_num = 0;
	klrou_t *router;
	cnodeid_t row, col;

	printk("************** Topology ********************\n");

	printk("    ");
	for_each_online_node(col)
		printk("%02d ", col);
	printk("\n");
	for_each_online_node(row) {
		printk("%02d  ", row);
		for_each_online_node(col)
			printk("%2d ", node_distance(row, col));
		printk("\n");
	}

	for_each_online_node(cnode) {
		nasid = COMPACT_TO_NASID_NODEID(cnode);

		if (nasid == -1) continue;

		brd = find_lboard_class((lboard_t *)KL_CONFIG_INFO(nasid),
					KLTYPE_ROUTER);

		if (!brd)
			continue;

		do {
			if (brd->brd_flags & DUPLICATE_BOARD)
				continue;
			printk("Router %d:", router_num);
			router_num++;

			router = (klrou_t *)NODE_OFFSET_TO_K0(NASID_GET(brd), brd->brd_compts[0]);

			for (port = 1; port <= MAX_ROUTER_PORTS; port++) {
				if (router->rou_port[port].port_nasid == INVALID_NASID)
					continue;

				dest_brd = (lboard_t *)NODE_OFFSET_TO_K0(
					router->rou_port[port].port_nasid,
					router->rou_port[port].port_offset);

				if (dest_brd->brd_type == KLTYPE_IP27)
					printk(" %d", dest_brd->brd_nasid);
				if (dest_brd->brd_type == KLTYPE_ROUTER)
					printk(" r");
			}
			printk("\n");

		} while ( (brd = find_lboard_class(KLCF_NEXT(brd), KLTYPE_ROUTER)) );
	}
}

static pfn_t __init slot_getbasepfn(cnodeid_t cnode, int slot)
{
	nasid_t nasid = COMPACT_TO_NASID_NODEID(cnode);

	return ((pfn_t)nasid << PFN_NASIDSHFT) | (slot << SLOT_PFNSHIFT);
}

static pfn_t __init slot_psize_compute(cnodeid_t node, int slot)
{
	nasid_t nasid;
	lboard_t *brd;
	klmembnk_t *banks;
	unsigned long size;

	nasid = COMPACT_TO_NASID_NODEID(node);
	/* Find the node board */
	brd = find_lboard((lboard_t *)KL_CONFIG_INFO(nasid), KLTYPE_IP27);
	if (!brd)
		return 0;

	/* Get the memory bank structure */
	banks = (klmembnk_t *) find_first_component(brd, KLSTRUCT_MEMBNK);
	if (!banks)
		return 0;

	/* Size in _Megabytes_ */
	size = (unsigned long)banks->membnk_bnksz[slot/4];

	/* hack for 128 dimm banks */
	if (size <= 128) {
		if (slot % 4 == 0) {
			size <<= 20;		/* size in bytes */
			return(size >> PAGE_SHIFT);
		} else
			return 0;
	} else {
		size /= 4;
		size <<= 20;
		return size >> PAGE_SHIFT;
	}
}

static void __init mlreset(void)
{
	int i;

	master_nasid = get_nasid();
	fine_mode = is_fine_dirmode();

	/*
	 * Probe for all CPUs - this creates the cpumask and sets up the
	 * mapping tables.  We need to do this as early as possible.
	 */
#ifdef CONFIG_SMP
	cpu_node_probe();
#endif

	init_topology_matrix();
	dump_topology();

	gen_region_mask(&region_mask);

	setup_replication_mask();

	/*
	 * Set all nodes' calias sizes to 8k
	 */
	for_each_online_node(i) {
		nasid_t nasid;

		nasid = COMPACT_TO_NASID_NODEID(i);

		/*
		 * Always have node 0 in the region mask, otherwise
		 * CALIAS accesses get exceptions since the hub
		 * thinks it is a node 0 address.
		 */
		REMOTE_HUB_S(nasid, PI_REGION_PRESENT, (region_mask | 1));
#ifdef CONFIG_REPLICATE_EXHANDLERS
		REMOTE_HUB_S(nasid, PI_CALIAS_SIZE, PI_CALIAS_SIZE_8K);
#else
		REMOTE_HUB_S(nasid, PI_CALIAS_SIZE, PI_CALIAS_SIZE_0);
#endif

#ifdef LATER
		/*
		 * Set up all hubs to have a big window pointing at
		 * widget 0. Memory mode, widget 0, offset 0
		 */
		REMOTE_HUB_S(nasid, IIO_ITTE(SWIN0_BIGWIN),
			((HUB_PIO_MAP_TO_MEM << IIO_ITTE_IOSP_SHIFT) |
			(0 << IIO_ITTE_WIDGET_SHIFT)));
#endif
	}
}

static void __init szmem(void)
{
	pfn_t slot_psize, slot0sz = 0, nodebytes;	/* Hack to detect problem configs */
	int slot;
	cnodeid_t node;

	num_physpages = 0;

	for_each_online_node(node) {
		nodebytes = 0;
		for (slot = 0; slot < MAX_MEM_SLOTS; slot++) {
			slot_psize = slot_psize_compute(node, slot);
			if (slot == 0)
				slot0sz = slot_psize;
			/*
			 * We need to refine the hack when we have replicated
			 * kernel text.
			 */
			nodebytes += (1LL << SLOT_SHIFT);

			if (!slot_psize)
				continue;

			if ((nodebytes >> PAGE_SHIFT) * (sizeof(struct page)) >
						(slot0sz << PAGE_SHIFT)) {
				printk("Ignoring slot %d onwards on node %d\n",
								slot, node);
				slot = MAX_MEM_SLOTS;
				continue;
			}
			num_physpages += slot_psize;
			add_active_range(node, slot_getbasepfn(node, slot),
					 slot_getbasepfn(node, slot) + slot_psize);
		}
	}
}

static void __init node_mem_init(cnodeid_t node)
{
	pfn_t slot_firstpfn = slot_getbasepfn(node, 0);
	pfn_t slot_freepfn = node_getfirstfree(node);
	unsigned long bootmap_size;
	pfn_t start_pfn, end_pfn;

	get_pfn_range_for_nid(node, &start_pfn, &end_pfn);

	/*
	 * Allocate the node data structures on the node first.
	 */
	__node_data[node] = __va(slot_freepfn << PAGE_SHIFT);

	NODE_DATA(node)->bdata = &bootmem_node_data[node];
	NODE_DATA(node)->node_start_pfn = start_pfn;
	NODE_DATA(node)->node_spanned_pages = end_pfn - start_pfn;

	cpus_clear(hub_data(node)->h_cpus);

	slot_freepfn += PFN_UP(sizeof(struct pglist_data) +
			       sizeof(struct hub_data));

  	bootmap_size = init_bootmem_node(NODE_DATA(node), slot_freepfn,
					start_pfn, end_pfn);
	free_bootmem_with_active_regions(node, end_pfn);
	reserve_bootmem_node(NODE_DATA(node), slot_firstpfn << PAGE_SHIFT,
		((slot_freepfn - slot_firstpfn) << PAGE_SHIFT) + bootmap_size,
		BOOTMEM_DEFAULT);
	sparse_memory_present_with_active_regions(node);
}

/*
 * A node with nothing.  We use it to avoid any special casing in
 * cpumask_of_node
 */
static struct node_data null_node = {
	.hub = {
		.h_cpus = CPU_MASK_NONE
	}
};

/*
 * Currently, the intranode memory hole support assumes that each slot
 * contains at least 32 MBytes of memory. We assume all bootmem data
 * fits on the first slot.
 */
void __init prom_meminit(void)
{
	cnodeid_t node;

	mlreset();
	szmem();

	for (node = 0; node < MAX_COMPACT_NODES; node++) {
		if (node_online(node)) {
			node_mem_init(node);
			continue;
		}
		__node_data[node] = &null_node;
	}
}

void __init prom_free_prom_memory(void)
{
	/* We got nothing to free here ...  */
}

extern unsigned long setup_zero_pages(void);

void __init paging_init(void)
{
	unsigned long zones_size[MAX_NR_ZONES] = {0, };
	unsigned node;

	pagetable_init();

	for_each_online_node(node) {
		pfn_t start_pfn, end_pfn;

		get_pfn_range_for_nid(node, &start_pfn, &end_pfn);

		if (end_pfn > max_low_pfn)
			max_low_pfn = end_pfn;
	}
	zones_size[ZONE_NORMAL] = max_low_pfn;
	free_area_init_nodes(zones_size);
}

void __init mem_init(void)
{
	unsigned long codesize, datasize, initsize, tmp;
	unsigned node;

	high_memory = (void *) __va(num_physpages << PAGE_SHIFT);

	for_each_online_node(node) {
		/*
		 * This will free up the bootmem, ie, slot 0 memory.
		 */
		totalram_pages += free_all_bootmem_node(NODE_DATA(node));
	}

	totalram_pages -= setup_zero_pages();	/* This comes from node 0 */

	codesize =  (unsigned long) &_etext - (unsigned long) &_text;
	datasize =  (unsigned long) &_edata - (unsigned long) &_etext;
	initsize =  (unsigned long) &__init_end - (unsigned long) &__init_begin;

	tmp = nr_free_pages();
	printk(KERN_INFO "Memory: %luk/%luk available (%ldk kernel code, "
	       "%ldk reserved, %ldk data, %ldk init, %ldk highmem)\n",
	       tmp << (PAGE_SHIFT-10),
	       num_physpages << (PAGE_SHIFT-10),
	       codesize >> 10,
	       (num_physpages - tmp) << (PAGE_SHIFT-10),
	       datasize >> 10,
	       initsize >> 10,
	       totalhigh_pages << (PAGE_SHIFT-10));
}
