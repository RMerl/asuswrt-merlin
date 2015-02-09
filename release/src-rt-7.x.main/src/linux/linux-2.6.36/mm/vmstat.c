/*
 *  linux/mm/vmstat.c
 *
 *  Manages VM statistics
 *  Copyright (C) 1991, 1992, 1993, 1994  Linus Torvalds
 *
 *  zoned VM statistics
 *  Copyright (C) 2006 Silicon Graphics, Inc.,
 *		Christoph Lameter <christoph@lameter.com>
 */
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/cpu.h>
#include <linux/vmstat.h>
#include <linux/sched.h>
#include <linux/math64.h>

#ifdef CONFIG_VM_EVENT_COUNTERS
DEFINE_PER_CPU(struct vm_event_state, vm_event_states) = {{0}};
EXPORT_PER_CPU_SYMBOL(vm_event_states);

static void sum_vm_events(unsigned long *ret)
{
	int cpu;
	int i;

	memset(ret, 0, NR_VM_EVENT_ITEMS * sizeof(unsigned long));

	for_each_online_cpu(cpu) {
		struct vm_event_state *this = &per_cpu(vm_event_states, cpu);

		for (i = 0; i < NR_VM_EVENT_ITEMS; i++)
			ret[i] += this->event[i];
	}
}

/*
 * Accumulate the vm event counters across all CPUs.
 * The result is unavoidably approximate - it can change
 * during and after execution of this function.
*/
void all_vm_events(unsigned long *ret)
{
	get_online_cpus();
	sum_vm_events(ret);
	put_online_cpus();
}
EXPORT_SYMBOL_GPL(all_vm_events);

#ifdef CONFIG_HOTPLUG
/*
 * Fold the foreign cpu events into our own.
 *
 * This is adding to the events on one processor
 * but keeps the global counts constant.
 */
void vm_events_fold_cpu(int cpu)
{
	struct vm_event_state *fold_state = &per_cpu(vm_event_states, cpu);
	int i;

	for (i = 0; i < NR_VM_EVENT_ITEMS; i++) {
		count_vm_events(i, fold_state->event[i]);
		fold_state->event[i] = 0;
	}
}
#endif /* CONFIG_HOTPLUG */

#endif /* CONFIG_VM_EVENT_COUNTERS */

/*
 * Manage combined zone based / global counters
 *
 * vm_stat contains the global counters
 */
atomic_long_t vm_stat[NR_VM_ZONE_STAT_ITEMS];
EXPORT_SYMBOL(vm_stat);

#ifdef CONFIG_SMP

static int calculate_pressure_threshold(struct zone *zone)
{
	int threshold;
	int watermark_distance;

	/*
	 * As vmstats are not up to date, there is drift between the estimated
	 * and real values. For high thresholds and a high number of CPUs, it
	 * is possible for the min watermark to be breached while the estimated
	 * value looks fine. The pressure threshold is a reduced value such
	 * that even the maximum amount of drift will not accidentally breach
	 * the min watermark
	 */
	watermark_distance = low_wmark_pages(zone) - min_wmark_pages(zone);
	threshold = max(1, (int)(watermark_distance / num_online_cpus()));

	/*
	 * Maximum threshold is 125
	 */
	threshold = min(125, threshold);

	return threshold;
}

static int calculate_threshold(struct zone *zone)
{
	int threshold;
	int mem;	/* memory in 128 MB units */

	/*
	 * The threshold scales with the number of processors and the amount
	 * of memory per zone. More memory means that we can defer updates for
	 * longer, more processors could lead to more contention.
 	 * fls() is used to have a cheap way of logarithmic scaling.
	 *
	 * Some sample thresholds:
	 *
	 * Threshold	Processors	(fls)	Zonesize	fls(mem+1)
	 * ------------------------------------------------------------------
	 * 8		1		1	0.9-1 GB	4
	 * 16		2		2	0.9-1 GB	4
	 * 20 		2		2	1-2 GB		5
	 * 24		2		2	2-4 GB		6
	 * 28		2		2	4-8 GB		7
	 * 32		2		2	8-16 GB		8
	 * 4		2		2	<128M		1
	 * 30		4		3	2-4 GB		5
	 * 48		4		3	8-16 GB		8
	 * 32		8		4	1-2 GB		4
	 * 32		8		4	0.9-1GB		4
	 * 10		16		5	<128M		1
	 * 40		16		5	900M		4
	 * 70		64		7	2-4 GB		5
	 * 84		64		7	4-8 GB		6
	 * 108		512		9	4-8 GB		6
	 * 125		1024		10	8-16 GB		8
	 * 125		1024		10	16-32 GB	9
	 */

	mem = zone->present_pages >> (27 - PAGE_SHIFT);

	threshold = 2 * fls(num_online_cpus()) * (1 + fls(mem));

	/*
	 * Maximum threshold is 125
	 */
	threshold = min(125, threshold);

	return threshold;
}

/*
 * Refresh the thresholds for each zone.
 */
static void refresh_zone_stat_thresholds(void)
{
	struct zone *zone;
	int cpu;
	int threshold;

	for_each_populated_zone(zone) {
		unsigned long max_drift, tolerate_drift;

		threshold = calculate_threshold(zone);

		for_each_online_cpu(cpu)
			per_cpu_ptr(zone->pageset, cpu)->stat_threshold
							= threshold;

		/*
		 * Only set percpu_drift_mark if there is a danger that
		 * NR_FREE_PAGES reports the low watermark is ok when in fact
		 * the min watermark could be breached by an allocation
		 */
		tolerate_drift = low_wmark_pages(zone) - min_wmark_pages(zone);
		max_drift = num_online_cpus() * threshold;
		if (max_drift > tolerate_drift)
			zone->percpu_drift_mark = high_wmark_pages(zone) +
					max_drift;
	}
}

void reduce_pgdat_percpu_threshold(pg_data_t *pgdat)
{
	struct zone *zone;
	int cpu;
	int threshold;
	int i;

	get_online_cpus();
	for (i = 0; i < pgdat->nr_zones; i++) {
		zone = &pgdat->node_zones[i];
		if (!zone->percpu_drift_mark)
			continue;

		threshold = calculate_pressure_threshold(zone);
		for_each_online_cpu(cpu)
			per_cpu_ptr(zone->pageset, cpu)->stat_threshold
							= threshold;
	}
	put_online_cpus();
}

void restore_pgdat_percpu_threshold(pg_data_t *pgdat)
{
	struct zone *zone;
	int cpu;
	int threshold;
	int i;

	get_online_cpus();
	for (i = 0; i < pgdat->nr_zones; i++) {
		zone = &pgdat->node_zones[i];
		if (!zone->percpu_drift_mark)
			continue;

		threshold = calculate_threshold(zone);
		for_each_online_cpu(cpu)
			per_cpu_ptr(zone->pageset, cpu)->stat_threshold
							= threshold;
	}
	put_online_cpus();
}

/*
 * For use when we know that interrupts are disabled.
 */
void __mod_zone_page_state(struct zone *zone, enum zone_stat_item item,
				int delta)
{
	struct per_cpu_pageset *pcp = this_cpu_ptr(zone->pageset);

	s8 *p = pcp->vm_stat_diff + item;
	long x;

	x = delta + *p;

	if (unlikely(x > pcp->stat_threshold || x < -pcp->stat_threshold)) {
		zone_page_state_add(x, zone, item);
		x = 0;
	}
	*p = x;
}
EXPORT_SYMBOL(__mod_zone_page_state);

/*
 * For an unknown interrupt state
 */
void mod_zone_page_state(struct zone *zone, enum zone_stat_item item,
					int delta)
{
	unsigned long flags;

	local_irq_save(flags);
	__mod_zone_page_state(zone, item, delta);
	local_irq_restore(flags);
}
EXPORT_SYMBOL(mod_zone_page_state);

/*
 * Optimized increment and decrement functions.
 *
 * These are only for a single page and therefore can take a struct page *
 * argument instead of struct zone *. This allows the inclusion of the code
 * generated for page_zone(page) into the optimized functions.
 *
 * No overflow check is necessary and therefore the differential can be
 * incremented or decremented in place which may allow the compilers to
 * generate better code.
 * The increment or decrement is known and therefore one boundary check can
 * be omitted.
 *
 * NOTE: These functions are very performance sensitive. Change only
 * with care.
 *
 * Some processors have inc/dec instructions that are atomic vs an interrupt.
 * However, the code must first determine the differential location in a zone
 * based on the processor number and then inc/dec the counter. There is no
 * guarantee without disabling preemption that the processor will not change
 * in between and therefore the atomicity vs. interrupt cannot be exploited
 * in a useful way here.
 */
void __inc_zone_state(struct zone *zone, enum zone_stat_item item)
{
	struct per_cpu_pageset *pcp = this_cpu_ptr(zone->pageset);
	s8 *p = pcp->vm_stat_diff + item;

	(*p)++;

	if (unlikely(*p > pcp->stat_threshold)) {
		int overstep = pcp->stat_threshold / 2;

		zone_page_state_add(*p + overstep, zone, item);
		*p = -overstep;
	}
}

void __inc_zone_page_state(struct page *page, enum zone_stat_item item)
{
	__inc_zone_state(page_zone(page), item);
}
EXPORT_SYMBOL(__inc_zone_page_state);

void __dec_zone_state(struct zone *zone, enum zone_stat_item item)
{
	struct per_cpu_pageset *pcp = this_cpu_ptr(zone->pageset);
	s8 *p = pcp->vm_stat_diff + item;

	(*p)--;

	if (unlikely(*p < - pcp->stat_threshold)) {
		int overstep = pcp->stat_threshold / 2;

		zone_page_state_add(*p - overstep, zone, item);
		*p = overstep;
	}
}

void __dec_zone_page_state(struct page *page, enum zone_stat_item item)
{
	__dec_zone_state(page_zone(page), item);
}
EXPORT_SYMBOL(__dec_zone_page_state);

void inc_zone_state(struct zone *zone, enum zone_stat_item item)
{
	unsigned long flags;

	local_irq_save(flags);
	__inc_zone_state(zone, item);
	local_irq_restore(flags);
}

void inc_zone_page_state(struct page *page, enum zone_stat_item item)
{
	unsigned long flags;
	struct zone *zone;

	zone = page_zone(page);
	local_irq_save(flags);
	__inc_zone_state(zone, item);
	local_irq_restore(flags);
}
EXPORT_SYMBOL(inc_zone_page_state);

void dec_zone_page_state(struct page *page, enum zone_stat_item item)
{
	unsigned long flags;

	local_irq_save(flags);
	__dec_zone_page_state(page, item);
	local_irq_restore(flags);
}
EXPORT_SYMBOL(dec_zone_page_state);

/*
 * Update the zone counters for one cpu.
 *
 * The cpu specified must be either the current cpu or a processor that
 * is not online. If it is the current cpu then the execution thread must
 * be pinned to the current cpu.
 *
 * Note that refresh_cpu_vm_stats strives to only access
 * node local memory. The per cpu pagesets on remote zones are placed
 * in the memory local to the processor using that pageset. So the
 * loop over all zones will access a series of cachelines local to
 * the processor.
 *
 * The call to zone_page_state_add updates the cachelines with the
 * statistics in the remote zone struct as well as the global cachelines
 * with the global counters. These could cause remote node cache line
 * bouncing and will have to be only done when necessary.
 */
void refresh_cpu_vm_stats(int cpu)
{
	struct zone *zone;
	int i;
	int global_diff[NR_VM_ZONE_STAT_ITEMS] = { 0, };

	for_each_populated_zone(zone) {
		struct per_cpu_pageset *p;

		p = per_cpu_ptr(zone->pageset, cpu);

		for (i = 0; i < NR_VM_ZONE_STAT_ITEMS; i++)
			if (p->vm_stat_diff[i]) {
				unsigned long flags;
				int v;

				local_irq_save(flags);
				v = p->vm_stat_diff[i];
				p->vm_stat_diff[i] = 0;
				local_irq_restore(flags);
				atomic_long_add(v, &zone->vm_stat[i]);
				global_diff[i] += v;
#ifdef CONFIG_NUMA
				/* 3 seconds idle till flush */
				p->expire = 3;
#endif
			}
		cond_resched();
#ifdef CONFIG_NUMA
		/*
		 * Deal with draining the remote pageset of this
		 * processor
		 *
		 * Check if there are pages remaining in this pageset
		 * if not then there is nothing to expire.
		 */
		if (!p->expire || !p->pcp.count)
			continue;

		/*
		 * We never drain zones local to this processor.
		 */
		if (zone_to_nid(zone) == numa_node_id()) {
			p->expire = 0;
			continue;
		}

		p->expire--;
		if (p->expire)
			continue;

		if (p->pcp.count)
			drain_zone_pages(zone, &p->pcp);
#endif
	}

	for (i = 0; i < NR_VM_ZONE_STAT_ITEMS; i++)
		if (global_diff[i])
			atomic_long_add(global_diff[i], &vm_stat[i]);
}

#endif

#ifdef CONFIG_NUMA
/*
 * zonelist = the list of zones passed to the allocator
 * z 	    = the zone from which the allocation occurred.
 *
 * Must be called with interrupts disabled.
 */
void zone_statistics(struct zone *preferred_zone, struct zone *z)
{
	if (z->zone_pgdat == preferred_zone->zone_pgdat) {
		__inc_zone_state(z, NUMA_HIT);
	} else {
		__inc_zone_state(z, NUMA_MISS);
		__inc_zone_state(preferred_zone, NUMA_FOREIGN);
	}
	if (z->node == numa_node_id())
		__inc_zone_state(z, NUMA_LOCAL);
	else
		__inc_zone_state(z, NUMA_OTHER);
}
#endif

#ifdef CONFIG_COMPACTION
struct contig_page_info {
	unsigned long free_pages;
	unsigned long free_blocks_total;
	unsigned long free_blocks_suitable;
};

/*
 * Calculate the number of free pages in a zone, how many contiguous
 * pages are free and how many are large enough to satisfy an allocation of
 * the target size. Note that this function makes no attempt to estimate
 * how many suitable free blocks there *might* be if MOVABLE pages were
 * migrated. Calculating that is possible, but expensive and can be
 * figured out from userspace
 */
static void fill_contig_page_info(struct zone *zone,
				unsigned int suitable_order,
				struct contig_page_info *info)
{
	unsigned int order;

	info->free_pages = 0;
	info->free_blocks_total = 0;
	info->free_blocks_suitable = 0;

	for (order = 0; order < MAX_ORDER; order++) {
		unsigned long blocks;

		/* Count number of free blocks */
		blocks = zone->free_area[order].nr_free;
		info->free_blocks_total += blocks;

		/* Count free base pages */
		info->free_pages += blocks << order;

		/* Count the suitable free blocks */
		if (order >= suitable_order)
			info->free_blocks_suitable += blocks <<
						(order - suitable_order);
	}
}

/*
 * A fragmentation index only makes sense if an allocation of a requested
 * size would fail. If that is true, the fragmentation index indicates
 * whether external fragmentation or a lack of memory was the problem.
 * The value can be used to determine if page reclaim or compaction
 * should be used
 */
static int __fragmentation_index(unsigned int order, struct contig_page_info *info)
{
	unsigned long requested = 1UL << order;

	if (!info->free_blocks_total)
		return 0;

	/* Fragmentation index only makes sense when a request would fail */
	if (info->free_blocks_suitable)
		return -1000;

	/*
	 * Index is between 0 and 1 so return within 3 decimal places
	 *
	 * 0 => allocation would fail due to lack of memory
	 * 1 => allocation would fail due to fragmentation
	 */
	return 1000 - div_u64( (1000+(div_u64(info->free_pages * 1000ULL, requested))), info->free_blocks_total);
}

/* Same as __fragmentation index but allocs contig_page_info on stack */
int fragmentation_index(struct zone *zone, unsigned int order)
{
	struct contig_page_info info;

	fill_contig_page_info(zone, order, &info);
	return __fragmentation_index(order, &info);
}
#endif

#if defined(CONFIG_PROC_FS) || defined(CONFIG_COMPACTION)
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

static char * const migratetype_names[MIGRATE_TYPES] = {
	"Unmovable",
	"Reclaimable",
	"Movable",
	"Reserve",
	"Isolate",
};

static void *frag_start(struct seq_file *m, loff_t *pos)
{
	pg_data_t *pgdat;
	loff_t node = *pos;
	for (pgdat = first_online_pgdat();
	     pgdat && node;
	     pgdat = next_online_pgdat(pgdat))
		--node;

	return pgdat;
}

static void *frag_next(struct seq_file *m, void *arg, loff_t *pos)
{
	pg_data_t *pgdat = (pg_data_t *)arg;

	(*pos)++;
	return next_online_pgdat(pgdat);
}

static void frag_stop(struct seq_file *m, void *arg)
{
}

/* Walk all the zones in a node and print using a callback */
static void walk_zones_in_node(struct seq_file *m, pg_data_t *pgdat,
		void (*print)(struct seq_file *m, pg_data_t *, struct zone *))
{
	struct zone *zone;
	struct zone *node_zones = pgdat->node_zones;
	unsigned long flags;

	for (zone = node_zones; zone - node_zones < MAX_NR_ZONES; ++zone) {
		if (!populated_zone(zone))
			continue;

		spin_lock_irqsave(&zone->lock, flags);
		print(m, pgdat, zone);
		spin_unlock_irqrestore(&zone->lock, flags);
	}
}
#endif

#ifdef CONFIG_PROC_FS
static void frag_show_print(struct seq_file *m, pg_data_t *pgdat,
						struct zone *zone)
{
	int order;

	seq_printf(m, "Node %d, zone %8s ", pgdat->node_id, zone->name);
	for (order = 0; order < MAX_ORDER; ++order)
		seq_printf(m, "%6lu ", zone->free_area[order].nr_free);
	seq_putc(m, '\n');
}

/*
 * This walks the free areas for each zone.
 */
static int frag_show(struct seq_file *m, void *arg)
{
	pg_data_t *pgdat = (pg_data_t *)arg;
	walk_zones_in_node(m, pgdat, frag_show_print);
	return 0;
}

static void pagetypeinfo_showfree_print(struct seq_file *m,
					pg_data_t *pgdat, struct zone *zone)
{
	int order, mtype;

	for (mtype = 0; mtype < MIGRATE_TYPES; mtype++) {
		seq_printf(m, "Node %4d, zone %8s, type %12s ",
					pgdat->node_id,
					zone->name,
					migratetype_names[mtype]);
		for (order = 0; order < MAX_ORDER; ++order) {
			unsigned long freecount = 0;
			struct free_area *area;
			struct list_head *curr;

			area = &(zone->free_area[order]);

			list_for_each(curr, &area->free_list[mtype])
				freecount++;
			seq_printf(m, "%6lu ", freecount);
		}
		seq_putc(m, '\n');
	}
}

/* Print out the free pages at each order for each migatetype */
static int pagetypeinfo_showfree(struct seq_file *m, void *arg)
{
	int order;
	pg_data_t *pgdat = (pg_data_t *)arg;

	/* Print header */
	seq_printf(m, "%-43s ", "Free pages count per migrate type at order");
	for (order = 0; order < MAX_ORDER; ++order)
		seq_printf(m, "%6d ", order);
	seq_putc(m, '\n');

	walk_zones_in_node(m, pgdat, pagetypeinfo_showfree_print);

	return 0;
}

static void pagetypeinfo_showblockcount_print(struct seq_file *m,
					pg_data_t *pgdat, struct zone *zone)
{
	int mtype;
	unsigned long pfn;
	unsigned long start_pfn = zone->zone_start_pfn;
	unsigned long end_pfn = start_pfn + zone->spanned_pages;
	unsigned long count[MIGRATE_TYPES] = { 0, };

	for (pfn = start_pfn; pfn < end_pfn; pfn += pageblock_nr_pages) {
		struct page *page;

		if (!pfn_valid(pfn))
			continue;

		page = pfn_to_page(pfn);

		/* Watch for unexpected holes punched in the memmap */
		if (!memmap_valid_within(pfn, page, zone))
			continue;

		mtype = get_pageblock_migratetype(page);

		if (mtype < MIGRATE_TYPES)
			count[mtype]++;
	}

	/* Print counts */
	seq_printf(m, "Node %d, zone %8s ", pgdat->node_id, zone->name);
	for (mtype = 0; mtype < MIGRATE_TYPES; mtype++)
		seq_printf(m, "%12lu ", count[mtype]);
	seq_putc(m, '\n');
}

/* Print out the free pages at each order for each migratetype */
static int pagetypeinfo_showblockcount(struct seq_file *m, void *arg)
{
	int mtype;
	pg_data_t *pgdat = (pg_data_t *)arg;

	seq_printf(m, "\n%-23s", "Number of blocks type ");
	for (mtype = 0; mtype < MIGRATE_TYPES; mtype++)
		seq_printf(m, "%12s ", migratetype_names[mtype]);
	seq_putc(m, '\n');
	walk_zones_in_node(m, pgdat, pagetypeinfo_showblockcount_print);

	return 0;
}

/*
 * This prints out statistics in relation to grouping pages by mobility.
 * It is expensive to collect so do not constantly read the file.
 */
static int pagetypeinfo_show(struct seq_file *m, void *arg)
{
	pg_data_t *pgdat = (pg_data_t *)arg;

	/* check memoryless node */
	if (!node_state(pgdat->node_id, N_HIGH_MEMORY))
		return 0;

	seq_printf(m, "Page block order: %d\n", pageblock_order);
	seq_printf(m, "Pages per block:  %lu\n", pageblock_nr_pages);
	seq_putc(m, '\n');
	pagetypeinfo_showfree(m, pgdat);
	pagetypeinfo_showblockcount(m, pgdat);

	return 0;
}

static const struct seq_operations fragmentation_op = {
	.start	= frag_start,
	.next	= frag_next,
	.stop	= frag_stop,
	.show	= frag_show,
};

static int fragmentation_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &fragmentation_op);
}

static const struct file_operations fragmentation_file_operations = {
	.open		= fragmentation_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

static const struct seq_operations pagetypeinfo_op = {
	.start	= frag_start,
	.next	= frag_next,
	.stop	= frag_stop,
	.show	= pagetypeinfo_show,
};

static int pagetypeinfo_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &pagetypeinfo_op);
}

static const struct file_operations pagetypeinfo_file_ops = {
	.open		= pagetypeinfo_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

#ifdef CONFIG_ZONE_DMA
#define TEXT_FOR_DMA(xx) xx "_dma",
#else
#define TEXT_FOR_DMA(xx)
#endif

#ifdef CONFIG_ZONE_DMA32
#define TEXT_FOR_DMA32(xx) xx "_dma32",
#else
#define TEXT_FOR_DMA32(xx)
#endif

#ifdef CONFIG_HIGHMEM
#define TEXT_FOR_HIGHMEM(xx) xx "_high",
#else
#define TEXT_FOR_HIGHMEM(xx)
#endif

#define TEXTS_FOR_ZONES(xx) TEXT_FOR_DMA(xx) TEXT_FOR_DMA32(xx) xx "_normal", \
					TEXT_FOR_HIGHMEM(xx) xx "_movable",

static const char * const vmstat_text[] = {
	/* Zoned VM counters */
	"nr_free_pages",
	"nr_inactive_anon",
	"nr_active_anon",
	"nr_inactive_file",
	"nr_active_file",
	"nr_unevictable",
	"nr_mlock",
	"nr_anon_pages",
	"nr_mapped",
	"nr_file_pages",
	"nr_dirty",
	"nr_writeback",
	"nr_slab_reclaimable",
	"nr_slab_unreclaimable",
	"nr_page_table_pages",
	"nr_kernel_stack",
	"nr_unstable",
	"nr_bounce",
	"nr_vmscan_write",
	"nr_writeback_temp",
	"nr_isolated_anon",
	"nr_isolated_file",
	"nr_shmem",
#ifdef CONFIG_NUMA
	"numa_hit",
	"numa_miss",
	"numa_foreign",
	"numa_interleave",
	"numa_local",
	"numa_other",
#endif

#ifdef CONFIG_VM_EVENT_COUNTERS
	"pgpgin",
	"pgpgout",
	"pswpin",
	"pswpout",

	TEXTS_FOR_ZONES("pgalloc")

	"pgfree",
	"pgactivate",
	"pgdeactivate",

	"pgfault",
	"pgmajfault",

	TEXTS_FOR_ZONES("pgrefill")
	TEXTS_FOR_ZONES("pgsteal")
	TEXTS_FOR_ZONES("pgscan_kswapd")
	TEXTS_FOR_ZONES("pgscan_direct")

#ifdef CONFIG_NUMA
	"zone_reclaim_failed",
#endif
	"pginodesteal",
	"slabs_scanned",
	"kswapd_steal",
	"kswapd_inodesteal",
	"kswapd_low_wmark_hit_quickly",
	"kswapd_high_wmark_hit_quickly",
	"kswapd_skip_congestion_wait",
	"pageoutrun",
	"allocstall",

	"pgrotated",

#ifdef CONFIG_COMPACTION
	"compact_blocks_moved",
	"compact_pages_moved",
	"compact_pagemigrate_failed",
	"compact_stall",
	"compact_fail",
	"compact_success",
#endif

#ifdef CONFIG_HUGETLB_PAGE
	"htlb_buddy_alloc_success",
	"htlb_buddy_alloc_fail",
#endif
	"unevictable_pgs_culled",
	"unevictable_pgs_scanned",
	"unevictable_pgs_rescued",
	"unevictable_pgs_mlocked",
	"unevictable_pgs_munlocked",
	"unevictable_pgs_cleared",
	"unevictable_pgs_stranded",
	"unevictable_pgs_mlockfreed",
#endif
};

static void zoneinfo_show_print(struct seq_file *m, pg_data_t *pgdat,
							struct zone *zone)
{
	int i;
	seq_printf(m, "Node %d, zone %8s", pgdat->node_id, zone->name);
	seq_printf(m,
		   "\n  pages free     %lu"
		   "\n        min      %lu"
		   "\n        low      %lu"
		   "\n        high     %lu"
		   "\n        scanned  %lu"
		   "\n        spanned  %lu"
		   "\n        present  %lu",
		   zone_page_state(zone, NR_FREE_PAGES),
		   min_wmark_pages(zone),
		   low_wmark_pages(zone),
		   high_wmark_pages(zone),
		   zone->pages_scanned,
		   zone->spanned_pages,
		   zone->present_pages);

	for (i = 0; i < NR_VM_ZONE_STAT_ITEMS; i++)
		seq_printf(m, "\n    %-12s %lu", vmstat_text[i],
				zone_page_state(zone, i));

	seq_printf(m,
		   "\n        protection: (%lu",
		   zone->lowmem_reserve[0]);
	for (i = 1; i < ARRAY_SIZE(zone->lowmem_reserve); i++)
		seq_printf(m, ", %lu", zone->lowmem_reserve[i]);
	seq_printf(m,
		   ")"
		   "\n  pagesets");
	for_each_online_cpu(i) {
		struct per_cpu_pageset *pageset;

		pageset = per_cpu_ptr(zone->pageset, i);
		seq_printf(m,
			   "\n    cpu: %i"
			   "\n              count: %i"
			   "\n              high:  %i"
			   "\n              batch: %i",
			   i,
			   pageset->pcp.count,
			   pageset->pcp.high,
			   pageset->pcp.batch);
#ifdef CONFIG_SMP
		seq_printf(m, "\n  vm stats threshold: %d",
				pageset->stat_threshold);
#endif
	}
	seq_printf(m,
		   "\n  all_unreclaimable: %u"
		   "\n  start_pfn:         %lu"
		   "\n  inactive_ratio:    %u",
		   zone->all_unreclaimable,
		   zone->zone_start_pfn,
		   zone->inactive_ratio);
	seq_putc(m, '\n');
}

/*
 * Output information about zones in @pgdat.
 */
static int zoneinfo_show(struct seq_file *m, void *arg)
{
	pg_data_t *pgdat = (pg_data_t *)arg;
	walk_zones_in_node(m, pgdat, zoneinfo_show_print);
	return 0;
}

static const struct seq_operations zoneinfo_op = {
	.start	= frag_start, /* iterate over all zones. The same as in
			       * fragmentation. */
	.next	= frag_next,
	.stop	= frag_stop,
	.show	= zoneinfo_show,
};

static int zoneinfo_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &zoneinfo_op);
}

static const struct file_operations proc_zoneinfo_file_operations = {
	.open		= zoneinfo_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

static void *vmstat_start(struct seq_file *m, loff_t *pos)
{
	unsigned long *v;
#ifdef CONFIG_VM_EVENT_COUNTERS
	unsigned long *e;
#endif
	int i;

	if (*pos >= ARRAY_SIZE(vmstat_text))
		return NULL;

#ifdef CONFIG_VM_EVENT_COUNTERS
	v = kmalloc(NR_VM_ZONE_STAT_ITEMS * sizeof(unsigned long)
			+ sizeof(struct vm_event_state), GFP_KERNEL);
#else
	v = kmalloc(NR_VM_ZONE_STAT_ITEMS * sizeof(unsigned long),
			GFP_KERNEL);
#endif
	m->private = v;
	if (!v)
		return ERR_PTR(-ENOMEM);
	for (i = 0; i < NR_VM_ZONE_STAT_ITEMS; i++)
		v[i] = global_page_state(i);
#ifdef CONFIG_VM_EVENT_COUNTERS
	e = v + NR_VM_ZONE_STAT_ITEMS;
	all_vm_events(e);
	e[PGPGIN] /= 2;		/* sectors -> kbytes */
	e[PGPGOUT] /= 2;
#endif
	return v + *pos;
}

static void *vmstat_next(struct seq_file *m, void *arg, loff_t *pos)
{
	(*pos)++;
	if (*pos >= ARRAY_SIZE(vmstat_text))
		return NULL;
	return (unsigned long *)m->private + *pos;
}

static int vmstat_show(struct seq_file *m, void *arg)
{
	unsigned long *l = arg;
	unsigned long off = l - (unsigned long *)m->private;

	seq_printf(m, "%s %lu\n", vmstat_text[off], *l);
	return 0;
}

static void vmstat_stop(struct seq_file *m, void *arg)
{
	kfree(m->private);
	m->private = NULL;
}

static const struct seq_operations vmstat_op = {
	.start	= vmstat_start,
	.next	= vmstat_next,
	.stop	= vmstat_stop,
	.show	= vmstat_show,
};

static int vmstat_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &vmstat_op);
}

static const struct file_operations proc_vmstat_file_operations = {
	.open		= vmstat_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};
#endif /* CONFIG_PROC_FS */

#ifdef CONFIG_SMP
static DEFINE_PER_CPU(struct delayed_work, vmstat_work);
int sysctl_stat_interval __read_mostly = HZ;

static void vmstat_update(struct work_struct *w)
{
	refresh_cpu_vm_stats(smp_processor_id());
	schedule_delayed_work(&__get_cpu_var(vmstat_work),
		round_jiffies_relative(sysctl_stat_interval));
}

static void __cpuinit start_cpu_timer(int cpu)
{
	struct delayed_work *work = &per_cpu(vmstat_work, cpu);

	INIT_DELAYED_WORK_DEFERRABLE(work, vmstat_update);
	schedule_delayed_work_on(cpu, work, __round_jiffies_relative(HZ, cpu));
}

/*
 * Use the cpu notifier to insure that the thresholds are recalculated
 * when necessary.
 */
static int __cpuinit vmstat_cpuup_callback(struct notifier_block *nfb,
		unsigned long action,
		void *hcpu)
{
	long cpu = (long)hcpu;

	switch (action) {
	case CPU_ONLINE:
	case CPU_ONLINE_FROZEN:
		refresh_zone_stat_thresholds();
		start_cpu_timer(cpu);
		node_set_state(cpu_to_node(cpu), N_CPU);
		break;
	case CPU_DOWN_PREPARE:
	case CPU_DOWN_PREPARE_FROZEN:
		cancel_rearming_delayed_work(&per_cpu(vmstat_work, cpu));
		per_cpu(vmstat_work, cpu).work.func = NULL;
		break;
	case CPU_DOWN_FAILED:
	case CPU_DOWN_FAILED_FROZEN:
		start_cpu_timer(cpu);
		break;
	case CPU_DEAD:
	case CPU_DEAD_FROZEN:
		refresh_zone_stat_thresholds();
		break;
	default:
		break;
	}
	return NOTIFY_OK;
}

static struct notifier_block __cpuinitdata vmstat_notifier =
	{ &vmstat_cpuup_callback, NULL, 0 };
#endif

static int __init setup_vmstat(void)
{
#ifdef CONFIG_SMP
	int cpu;

	refresh_zone_stat_thresholds();
	register_cpu_notifier(&vmstat_notifier);

	for_each_online_cpu(cpu)
		start_cpu_timer(cpu);
#endif
#ifdef CONFIG_PROC_FS
	proc_create("buddyinfo", S_IRUGO, NULL, &fragmentation_file_operations);
	proc_create("pagetypeinfo", S_IRUGO, NULL, &pagetypeinfo_file_ops);
	proc_create("vmstat", S_IRUGO, NULL, &proc_vmstat_file_operations);
	proc_create("zoneinfo", S_IRUGO, NULL, &proc_zoneinfo_file_operations);
#endif
	return 0;
}
module_init(setup_vmstat)

#if defined(CONFIG_DEBUG_FS) && defined(CONFIG_COMPACTION)
#include <linux/debugfs.h>

static struct dentry *extfrag_debug_root;

/*
 * Return an index indicating how much of the available free memory is
 * unusable for an allocation of the requested size.
 */
static int unusable_free_index(unsigned int order,
				struct contig_page_info *info)
{
	/* No free memory is interpreted as all free memory is unusable */
	if (info->free_pages == 0)
		return 1000;

	/*
	 * Index should be a value between 0 and 1. Return a value to 3
	 * decimal places.
	 *
	 * 0 => no fragmentation
	 * 1 => high fragmentation
	 */
	return div_u64((info->free_pages - (info->free_blocks_suitable << order)) * 1000ULL, info->free_pages);

}

static void unusable_show_print(struct seq_file *m,
					pg_data_t *pgdat, struct zone *zone)
{
	unsigned int order;
	int index;
	struct contig_page_info info;

	seq_printf(m, "Node %d, zone %8s ",
				pgdat->node_id,
				zone->name);
	for (order = 0; order < MAX_ORDER; ++order) {
		fill_contig_page_info(zone, order, &info);
		index = unusable_free_index(order, &info);
		seq_printf(m, "%d.%03d ", index / 1000, index % 1000);
	}

	seq_putc(m, '\n');
}

/*
 * Display unusable free space index
 *
 * The unusable free space index measures how much of the available free
 * memory cannot be used to satisfy an allocation of a given size and is a
 * value between 0 and 1. The higher the value, the more of free memory is
 * unusable and by implication, the worse the external fragmentation is. This
 * can be expressed as a percentage by multiplying by 100.
 */
static int unusable_show(struct seq_file *m, void *arg)
{
	pg_data_t *pgdat = (pg_data_t *)arg;

	/* check memoryless node */
	if (!node_state(pgdat->node_id, N_HIGH_MEMORY))
		return 0;

	walk_zones_in_node(m, pgdat, unusable_show_print);

	return 0;
}

static const struct seq_operations unusable_op = {
	.start	= frag_start,
	.next	= frag_next,
	.stop	= frag_stop,
	.show	= unusable_show,
};

static int unusable_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &unusable_op);
}

static const struct file_operations unusable_file_ops = {
	.open		= unusable_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

static void extfrag_show_print(struct seq_file *m,
					pg_data_t *pgdat, struct zone *zone)
{
	unsigned int order;
	int index;

	/* Alloc on stack as interrupts are disabled for zone walk */
	struct contig_page_info info;

	seq_printf(m, "Node %d, zone %8s ",
				pgdat->node_id,
				zone->name);
	for (order = 0; order < MAX_ORDER; ++order) {
		fill_contig_page_info(zone, order, &info);
		index = __fragmentation_index(order, &info);
		seq_printf(m, "%d.%03d ", index / 1000, index % 1000);
	}

	seq_putc(m, '\n');
}

/*
 * Display fragmentation index for orders that allocations would fail for
 */
static int extfrag_show(struct seq_file *m, void *arg)
{
	pg_data_t *pgdat = (pg_data_t *)arg;

	walk_zones_in_node(m, pgdat, extfrag_show_print);

	return 0;
}

static const struct seq_operations extfrag_op = {
	.start	= frag_start,
	.next	= frag_next,
	.stop	= frag_stop,
	.show	= extfrag_show,
};

static int extfrag_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &extfrag_op);
}

static const struct file_operations extfrag_file_ops = {
	.open		= extfrag_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

static int __init extfrag_debug_init(void)
{
	extfrag_debug_root = debugfs_create_dir("extfrag", NULL);
	if (!extfrag_debug_root)
		return -ENOMEM;

	if (!debugfs_create_file("unusable_index", 0444,
			extfrag_debug_root, NULL, &unusable_file_ops))
		return -ENOMEM;

	if (!debugfs_create_file("extfrag_index", 0444,
			extfrag_debug_root, NULL, &extfrag_file_ops))
		return -ENOMEM;

	return 0;
}

module_init(extfrag_debug_init);
#endif
