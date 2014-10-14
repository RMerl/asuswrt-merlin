/*
 * linux/mm/compaction.c
 *
 * Memory compaction for the reduction of external fragmentation. Note that
 * this heavily depends upon page migration to do all the real heavy
 * lifting
 *
 * Copyright IBM Corp. 2007-2010 Mel Gorman <mel@csn.ul.ie>
 */
#include <linux/swap.h>
#include <linux/migrate.h>
#include <linux/compaction.h>
#include <linux/mm_inline.h>
#include <linux/backing-dev.h>
#include <linux/sysctl.h>
#include <linux/sysfs.h>
#include "internal.h"

#define CREATE_TRACE_POINTS
#include <trace/events/compaction.h>

/*
 * compact_control is used to track pages being migrated and the free pages
 * they are being migrated to during memory compaction. The free_pfn starts
 * at the end of a zone and migrate_pfn begins at the start. Movable pages
 * are moved to the end of a zone during a compaction run and the run
 * completes when free_pfn <= migrate_pfn
 */
struct compact_control {
	struct list_head freepages;	/* List of free pages to migrate to */
	struct list_head migratepages;	/* List of pages being migrated */
	unsigned long nr_freepages;	/* Number of isolated free pages */
	unsigned long nr_migratepages;	/* Number of pages to migrate */
	unsigned long free_pfn;		/* isolate_freepages search base */
	unsigned long migrate_pfn;	/* isolate_migratepages search base */
	bool sync;			/* Synchronous migration */

	/* Account for isolated anon and file pages */
	unsigned long nr_anon;
	unsigned long nr_file;

	unsigned int order;		/* order a direct compactor needs */
	int migratetype;		/* MOVABLE, RECLAIMABLE etc */
	struct zone *zone;
};

static unsigned long release_freepages(struct list_head *freelist)
{
	struct page *page, *next;
	unsigned long count = 0;

	list_for_each_entry_safe(page, next, freelist, lru) {
		list_del(&page->lru);
		__free_page(page);
		count++;
	}

	return count;
}

/* Isolate free pages onto a private freelist. Must hold zone->lock */
static unsigned long isolate_freepages_block(struct zone *zone,
				unsigned long blockpfn,
				struct list_head *freelist)
{
	unsigned long zone_end_pfn, end_pfn;
	int nr_scanned = 0, total_isolated = 0;
	struct page *cursor;

	/* Get the last PFN we should scan for free pages at */
	zone_end_pfn = zone->zone_start_pfn + zone->spanned_pages;
	end_pfn = min(blockpfn + pageblock_nr_pages, zone_end_pfn);

	/* Find the first usable PFN in the block to initialse page cursor */
	for (; blockpfn < end_pfn; blockpfn++) {
		if (pfn_valid_within(blockpfn))
			break;
	}
	cursor = pfn_to_page(blockpfn);

	/* Isolate free pages. This assumes the block is valid */
	for (; blockpfn < end_pfn; blockpfn++, cursor++) {
		int isolated, i;
		struct page *page = cursor;

		if (!pfn_valid_within(blockpfn))
			continue;
		nr_scanned++;

		if (!PageBuddy(page))
			continue;

		/* Found a free page, break it into order-0 pages */
		isolated = split_free_page(page);
		total_isolated += isolated;
		for (i = 0; i < isolated; i++) {
			list_add(&page->lru, freelist);
			page++;
		}

		/* If a page was split, advance to the end of it */
		if (isolated) {
			blockpfn += isolated - 1;
			cursor += isolated - 1;
		}
	}

	trace_mm_compaction_isolate_freepages(nr_scanned, total_isolated);
	return total_isolated;
}

/* Returns true if the page is within a block suitable for migration to */
static bool suitable_migration_target(struct page *page)
{

	int migratetype = get_pageblock_migratetype(page);

	/* Don't interfere with memory hot-remove or the min_free_kbytes blocks */
	if (migratetype == MIGRATE_ISOLATE || migratetype == MIGRATE_RESERVE)
		return false;

	/* If the page is a large free page, then allow migration */
	if (PageBuddy(page) && page_order(page) >= pageblock_order)
		return true;

	/* If the block is MIGRATE_MOVABLE, allow migration */
	if (migratetype == MIGRATE_MOVABLE)
		return true;

	/* Otherwise skip the block */
	return false;
}

/*
 * Based on information in the current compact_control, find blocks
 * suitable for isolating free pages from and then isolate them.
 */
static void isolate_freepages(struct zone *zone,
				struct compact_control *cc)
{
	struct page *page;
	unsigned long high_pfn, low_pfn, pfn;
	unsigned long flags;
	int nr_freepages = cc->nr_freepages;
	struct list_head *freelist = &cc->freepages;

	/*
	 * Initialise the free scanner. The starting point is where we last
	 * scanned from (or the end of the zone if starting). The low point
	 * is the end of the pageblock the migration scanner is using.
	 */
	pfn = cc->free_pfn;
	low_pfn = cc->migrate_pfn + pageblock_nr_pages;

	/*
	 * Take care that if the migration scanner is at the end of the zone
	 * that the free scanner does not accidentally move to the next zone
	 * in the next isolation cycle.
	 */
	high_pfn = min(low_pfn, pfn);

	/*
	 * Isolate free pages until enough are available to migrate the
	 * pages on cc->migratepages. We stop searching if the migrate
	 * and free page scanners meet or enough free pages are isolated.
	 */
	for (; pfn > low_pfn && cc->nr_migratepages > nr_freepages;
					pfn -= pageblock_nr_pages) {
		unsigned long isolated;

		if (!pfn_valid(pfn))
			continue;

		/*
		 * Check for overlapping nodes/zones. It's possible on some
		 * configurations to have a setup like
		 * node0 node1 node0
		 * i.e. it's possible that all pages within a zones range of
		 * pages do not belong to a single zone.
		 */
		page = pfn_to_page(pfn);
		if (page_zone(page) != zone)
			continue;

		/* Check the block is suitable for migration */
		if (!suitable_migration_target(page))
			continue;

		/*
		 * Found a block suitable for isolating free pages from. Now
		 * we disabled interrupts, double check things are ok and
		 * isolate the pages. This is to minimise the time IRQs
		 * are disabled
		 */
		isolated = 0;
		spin_lock_irqsave(&zone->lock, flags);
		if (suitable_migration_target(page)) {
			isolated = isolate_freepages_block(zone, pfn, freelist);
			nr_freepages += isolated;
		}
		spin_unlock_irqrestore(&zone->lock, flags);

		/*
		 * Record the highest PFN we isolated pages from. When next
		 * looking for free pages, the search will restart here as
		 * page migration may have returned some pages to the allocator
		 */
		if (isolated)
			high_pfn = max(high_pfn, pfn);
	}

	/* split_free_page does not map the pages */
	list_for_each_entry(page, freelist, lru) {
		arch_alloc_page(page, 0);
		kernel_map_pages(page, 1, 1);
	}

	cc->free_pfn = high_pfn;
	cc->nr_freepages = nr_freepages;
}

/* Update the number of anon and file isolated pages in the zone */
static void acct_isolated(struct zone *zone, struct compact_control *cc)
{
	struct page *page;
	unsigned int count[NR_LRU_LISTS] = { 0, };

	list_for_each_entry(page, &cc->migratepages, lru) {
		int lru = page_lru_base_type(page);
		count[lru]++;
	}

	cc->nr_anon = count[LRU_ACTIVE_ANON] + count[LRU_INACTIVE_ANON];
	cc->nr_file = count[LRU_ACTIVE_FILE] + count[LRU_INACTIVE_FILE];
	__mod_zone_page_state(zone, NR_ISOLATED_ANON, cc->nr_anon);
	__mod_zone_page_state(zone, NR_ISOLATED_FILE, cc->nr_file);
}

/* Similar to reclaim, but different enough that they don't share logic */
static bool too_many_isolated(struct zone *zone)
{
	unsigned long active, inactive, isolated;

	inactive = zone_page_state(zone, NR_INACTIVE_FILE) +
					zone_page_state(zone, NR_INACTIVE_ANON);
	active = zone_page_state(zone, NR_ACTIVE_FILE) +
					zone_page_state(zone, NR_ACTIVE_ANON);
	isolated = zone_page_state(zone, NR_ISOLATED_FILE) +
					zone_page_state(zone, NR_ISOLATED_ANON);

	return isolated > (inactive + active) / 2;
}

/* possible outcome of isolate_migratepages */
typedef enum {
	ISOLATE_ABORT,		/* Abort compaction now */
	ISOLATE_NONE,		/* No pages isolated, continue scanning */
	ISOLATE_SUCCESS,	/* Pages isolated, migrate */
} isolate_migrate_t;

/*
 * Isolate all pages that can be migrated from the block pointed to by
 * the migrate scanner within compact_control.
 */
static isolate_migrate_t isolate_migratepages(struct zone *zone,
					struct compact_control *cc)
{
	unsigned long low_pfn, end_pfn;
	unsigned long last_pageblock_nr = 0, pageblock_nr;
	unsigned long nr_scanned = 0, nr_isolated = 0;
	struct list_head *migratelist = &cc->migratepages;

	/* Do not scan outside zone boundaries */
	low_pfn = max(cc->migrate_pfn, zone->zone_start_pfn);

	/* Only scan within a pageblock boundary */
	end_pfn = ALIGN(low_pfn + pageblock_nr_pages, pageblock_nr_pages);

	/* Do not cross the free scanner or scan within a memory hole */
	if (end_pfn > cc->free_pfn || !pfn_valid(low_pfn)) {
		cc->migrate_pfn = end_pfn;
		return ISOLATE_NONE;
	}

	/*
	 * Ensure that there are not too many pages isolated from the LRU
	 * list by either parallel reclaimers or compaction. If there are,
	 * delay for some time until fewer pages are isolated
	 */
	while (unlikely(too_many_isolated(zone))) {
		/* async migration should just abort */
		if (!cc->sync)
			return ISOLATE_ABORT;

		congestion_wait(BLK_RW_ASYNC, HZ/10);

		if (fatal_signal_pending(current))
			return ISOLATE_ABORT;
	}

	/* Time to isolate some pages for migration */
	cond_resched();
	spin_lock_irq(&zone->lru_lock);
	for (; low_pfn < end_pfn; low_pfn++) {
		struct page *page;
		bool locked = true;

		/* give a chance to irqs before checking need_resched() */
		if (!((low_pfn+1) % SWAP_CLUSTER_MAX)) {
			spin_unlock_irq(&zone->lru_lock);
			locked = false;
		}
		if (need_resched() || spin_is_contended(&zone->lru_lock)) {
			if (locked)
				spin_unlock_irq(&zone->lru_lock);
			cond_resched();
			spin_lock_irq(&zone->lru_lock);
			if (fatal_signal_pending(current))
				break;
		} else if (!locked)
			spin_lock_irq(&zone->lru_lock);

		if (!pfn_valid_within(low_pfn))
			continue;
		nr_scanned++;

		/* Get the page and skip if free */
		page = pfn_to_page(low_pfn);
		if (PageBuddy(page))
			continue;

		/*
		 * For async migration, also only scan in MOVABLE blocks. Async
		 * migration is optimistic to see if the minimum amount of work
		 * satisfies the allocation
		 */
		pageblock_nr = low_pfn >> pageblock_order;
		if (!cc->sync && last_pageblock_nr != pageblock_nr &&
				get_pageblock_migratetype(page) != MIGRATE_MOVABLE) {
			low_pfn += pageblock_nr_pages;
			low_pfn = ALIGN(low_pfn, pageblock_nr_pages) - 1;
			last_pageblock_nr = pageblock_nr;
			continue;
		}

		if (!PageLRU(page))
			continue;

		/*
		 * PageLRU is set, and lru_lock excludes isolation,
		 * splitting and collapsing (collapsing has already
		 * happened if PageLRU is set).
		 */
		if (PageTransHuge(page)) {
			low_pfn += (1 << compound_order(page)) - 1;
			continue;
		}

		/* Try isolate the page */
		if (__isolate_lru_page(page, ISOLATE_BOTH, 0) != 0)
			continue;

		VM_BUG_ON(PageTransCompound(page));

		/* Successfully isolated */
		del_page_from_lru_list(zone, page, page_lru(page));
		list_add(&page->lru, migratelist);
		cc->nr_migratepages++;
		nr_isolated++;

		/* Avoid isolating too much */
		if (cc->nr_migratepages == COMPACT_CLUSTER_MAX)
			break;
	}

	acct_isolated(zone, cc);

	spin_unlock_irq(&zone->lru_lock);
	cc->migrate_pfn = low_pfn;

	trace_mm_compaction_isolate_migratepages(nr_scanned, nr_isolated);

	return ISOLATE_SUCCESS;
}

/*
 * This is a migrate-callback that "allocates" freepages by taking pages
 * from the isolated freelists in the block we are migrating to.
 */
static struct page *compaction_alloc(struct page *migratepage,
					unsigned long data,
					int **result)
{
	struct compact_control *cc = (struct compact_control *)data;
	struct page *freepage;

	/* Isolate free pages if necessary */
	if (list_empty(&cc->freepages)) {
		isolate_freepages(cc->zone, cc);

		if (list_empty(&cc->freepages))
			return NULL;
	}

	freepage = list_entry(cc->freepages.next, struct page, lru);
	list_del(&freepage->lru);
	cc->nr_freepages--;

	return freepage;
}

/*
 * We cannot control nr_migratepages and nr_freepages fully when migration is
 * running as migrate_pages() has no knowledge of compact_control. When
 * migration is complete, we count the number of pages on the lists by hand.
 */
static void update_nr_listpages(struct compact_control *cc)
{
	int nr_migratepages = 0;
	int nr_freepages = 0;
	struct page *page;

	list_for_each_entry(page, &cc->migratepages, lru)
		nr_migratepages++;
	list_for_each_entry(page, &cc->freepages, lru)
		nr_freepages++;

	cc->nr_migratepages = nr_migratepages;
	cc->nr_freepages = nr_freepages;
}

static int compact_finished(struct zone *zone,
			    struct compact_control *cc)
{
	unsigned int order;
	unsigned long watermark;

	if (fatal_signal_pending(current))
		return COMPACT_PARTIAL;

	/* Compaction run completes if the migrate and free scanner meet */
	if (cc->free_pfn <= cc->migrate_pfn)
		return COMPACT_COMPLETE;

	/* Compaction run is not finished if the watermark is not met */
	watermark = low_wmark_pages(zone);
	watermark += (1 << cc->order);

	if (!zone_watermark_ok(zone, cc->order, watermark, 0, 0))
		return COMPACT_CONTINUE;

	/*
	 * order == -1 is expected when compacting via
	 * /proc/sys/vm/compact_memory
	 */
	if (cc->order == -1)
		return COMPACT_CONTINUE;

	/* Direct compactor: Is a suitable page free? */
	for (order = cc->order; order < MAX_ORDER; order++) {
		/* Job done if page is free of the right migratetype */
		if (!list_empty(&zone->free_area[order].free_list[cc->migratetype]))
			return COMPACT_PARTIAL;

		/* Job done if allocation would set block type */
		if (order >= pageblock_order && zone->free_area[order].nr_free)
			return COMPACT_PARTIAL;
	}

	return COMPACT_CONTINUE;
}

/*
 * compaction_suitable: Is this suitable to run compaction on this zone now?
 * Returns
 *   COMPACT_SKIPPED  - If there are too few free pages for compaction
 *   COMPACT_PARTIAL  - If the allocation would succeed without compaction
 *   COMPACT_CONTINUE - If compaction should run now
 */
unsigned long compaction_suitable(struct zone *zone, int order)
{
	int fragindex;
	unsigned long watermark;

	/*
	 * Watermarks for order-0 must be met for compaction. Note the 2UL.
	 * This is because during migration, copies of pages need to be
	 * allocated and for a short time, the footprint is higher
	 */
	watermark = low_wmark_pages(zone) + (2UL << order);
	if (!zone_watermark_ok(zone, 0, watermark, 0, 0))
		return COMPACT_SKIPPED;

	/*
	 * order == -1 is expected when compacting via
	 * /proc/sys/vm/compact_memory
	 */
	if (order == -1)
		return COMPACT_CONTINUE;

	/*
	 * fragmentation index determines if allocation failures are due to
	 * low memory or external fragmentation
	 *
	 * index of -1 implies allocations might succeed dependingon watermarks
	 * index towards 0 implies failure is due to lack of memory
	 * index towards 1000 implies failure is due to fragmentation
	 *
	 * Only compact if a failure would be due to fragmentation.
	 */
	fragindex = fragmentation_index(zone, order);
	if (fragindex >= 0 && fragindex <= sysctl_extfrag_threshold)
		return COMPACT_SKIPPED;

	if (fragindex == -1 && zone_watermark_ok(zone, order, watermark, 0, 0))
		return COMPACT_PARTIAL;

	return COMPACT_CONTINUE;
}

static int compact_zone(struct zone *zone, struct compact_control *cc)
{
	int ret;

	ret = compaction_suitable(zone, cc->order);
	switch (ret) {
	case COMPACT_PARTIAL:
	case COMPACT_SKIPPED:
		/* Compaction is likely to fail */
		return ret;
	case COMPACT_CONTINUE:
		/* Fall through to compaction */
		;
	}

	/* Setup to move all movable pages to the end of the zone */
	cc->migrate_pfn = zone->zone_start_pfn;
	cc->free_pfn = cc->migrate_pfn + zone->spanned_pages;
	cc->free_pfn &= ~(pageblock_nr_pages-1);

	migrate_prep_local();

	while ((ret = compact_finished(zone, cc)) == COMPACT_CONTINUE) {
		unsigned long nr_migrate, nr_remaining;
		int err;

		switch (isolate_migratepages(zone, cc)) {
		case ISOLATE_ABORT:
			ret = COMPACT_PARTIAL;
			goto out;
		case ISOLATE_NONE:
			continue;
		case ISOLATE_SUCCESS:
			;
		}

		nr_migrate = cc->nr_migratepages;
		err = migrate_pages(&cc->migratepages, compaction_alloc,
				(unsigned long)cc, false,
				cc->sync);
		update_nr_listpages(cc);
		nr_remaining = cc->nr_migratepages;

		count_vm_event(COMPACTBLOCKS);
		count_vm_events(COMPACTPAGES, nr_migrate - nr_remaining);
		if (nr_remaining)
			count_vm_events(COMPACTPAGEFAILED, nr_remaining);
		trace_mm_compaction_migratepages(nr_migrate - nr_remaining,
						nr_remaining);

		/* Release LRU pages not migrated */
		if (err) {
			putback_lru_pages(&cc->migratepages);
			cc->nr_migratepages = 0;
		}

	}

out:
	/* Release free pages and check accounting */
	cc->nr_freepages -= release_freepages(&cc->freepages);
	VM_BUG_ON(cc->nr_freepages != 0);

	return ret;
}

unsigned long compact_zone_order(struct zone *zone,
				 int order, gfp_t gfp_mask,
				 bool sync)
{
	struct compact_control cc = {
		.nr_freepages = 0,
		.nr_migratepages = 0,
		.order = order,
		.migratetype = allocflags_to_migratetype(gfp_mask),
		.zone = zone,
		.sync = sync,
	};
	INIT_LIST_HEAD(&cc.freepages);
	INIT_LIST_HEAD(&cc.migratepages);

	return compact_zone(zone, &cc);
}

int sysctl_extfrag_threshold = 500;

/**
 * try_to_compact_pages - Direct compact to satisfy a high-order allocation
 * @zonelist: The zonelist used for the current allocation
 * @order: The order of the current allocation
 * @gfp_mask: The GFP mask of the current allocation
 * @nodemask: The allowed nodes to allocate from
 * @sync: Whether migration is synchronous or not
 *
 * This is the main entry point for direct page compaction.
 */
unsigned long try_to_compact_pages(struct zonelist *zonelist,
			int order, gfp_t gfp_mask, nodemask_t *nodemask,
			bool sync)
{
	enum zone_type high_zoneidx = gfp_zone(gfp_mask);
	int may_enter_fs = gfp_mask & __GFP_FS;
	int may_perform_io = gfp_mask & __GFP_IO;
	struct zoneref *z;
	struct zone *zone;
	int rc = COMPACT_SKIPPED;

	/*
	 * Check whether it is worth even starting compaction. The order check is
	 * made because an assumption is made that the page allocator can satisfy
	 * the "cheaper" orders without taking special steps
	 */
	if (!order || !may_enter_fs || !may_perform_io)
		return rc;

	count_vm_event(COMPACTSTALL);

	/* Compact each zone in the list */
	for_each_zone_zonelist_nodemask(zone, z, zonelist, high_zoneidx,
								nodemask) {
		int status;

		status = compact_zone_order(zone, order, gfp_mask, sync);
		rc = max(status, rc);

		/* If a normal allocation would succeed, stop compacting */
		if (zone_watermark_ok(zone, order, low_wmark_pages(zone), 0, 0))
			break;
	}

	return rc;
}


/* Compact all zones within a node */
static int compact_node(int nid)
{
	int zoneid;
	pg_data_t *pgdat;
	struct zone *zone;

	if (nid < 0 || nid >= nr_node_ids || !node_online(nid))
		return -EINVAL;
	pgdat = NODE_DATA(nid);

	/* Flush pending updates to the LRU lists */
	lru_add_drain_all();

	for (zoneid = 0; zoneid < MAX_NR_ZONES; zoneid++) {
		struct compact_control cc = {
			.nr_freepages = 0,
			.nr_migratepages = 0,
			.order = -1,
		};

		zone = &pgdat->node_zones[zoneid];
		if (!populated_zone(zone))
			continue;

		cc.zone = zone;
		INIT_LIST_HEAD(&cc.freepages);
		INIT_LIST_HEAD(&cc.migratepages);

		compact_zone(zone, &cc);

		VM_BUG_ON(!list_empty(&cc.freepages));
		VM_BUG_ON(!list_empty(&cc.migratepages));
	}

	return 0;
}

/* Compact all nodes in the system */
static int compact_nodes(void)
{
	int nid;

	for_each_online_node(nid)
		compact_node(nid);

	return COMPACT_COMPLETE;
}

/* The written value is actually unused, all memory is compacted */
int sysctl_compact_memory;

/* This is the entry point for compacting all nodes via /proc/sys/vm */
int sysctl_compaction_handler(struct ctl_table *table, int write,
			void __user *buffer, size_t *length, loff_t *ppos)
{
	if (write)
		return compact_nodes();

	return 0;
}

int sysctl_extfrag_handler(struct ctl_table *table, int write,
			void __user *buffer, size_t *length, loff_t *ppos)
{
	proc_dointvec_minmax(table, write, buffer, length, ppos);

	return 0;
}

#if defined(CONFIG_SYSFS) && defined(CONFIG_NUMA)
ssize_t sysfs_compact_node(struct sys_device *dev,
			struct sysdev_attribute *attr,
			const char *buf, size_t count)
{
	compact_node(dev->id);

	return count;
}
static SYSDEV_ATTR(compact, S_IWUSR, NULL, sysfs_compact_node);

int compaction_register_node(struct node *node)
{
	return sysdev_create_file(&node->sysdev, &attr_compact);
}

void compaction_unregister_node(struct node *node)
{
	return sysdev_remove_file(&node->sysdev, &attr_compact);
}
#endif /* CONFIG_SYSFS && CONFIG_NUMA */
