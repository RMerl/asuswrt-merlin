/* Modified by Broadcom Corp. Portions Copyright (c) Broadcom Corp, 2012. */
/*
 *	linux/mm/filemap.c
 *
 * Copyright (C) 1994-1999  Linus Torvalds
 */

/*
 * This file handles the generic file mmap semantics used by
 * most "normal" filesystems (but you don't /have/ to use this:
 * the NFS filesystem used to do this differently, for example)
 */
#include <linux/module.h>
#include <linux/compiler.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/aio.h>
#include <linux/capability.h>
#include <linux/kernel_stat.h>
#include <linux/gfp.h>
#include <linux/mm.h>
#include <linux/swap.h>
#include <linux/mman.h>
#include <linux/pagemap.h>
#include <linux/file.h>
#include <linux/uio.h>
#include <linux/hash.h>
#include <linux/writeback.h>
#include <linux/backing-dev.h>
#include <linux/pagevec.h>
#include <linux/blkdev.h>
#include <linux/security.h>
#include <linux/syscalls.h>
#include <linux/cpuset.h>
#include <linux/hardirq.h> /* for BUG_ON(!in_atomic()) only */
#include <linux/memcontrol.h>
#include <linux/mm_inline.h> /* for page_is_file_cache() */
#include "internal.h"

#include <linux/buffer_head.h> /* for try_to_free_buffers */

#include <asm/mman.h>

#include <typedefs.h>
#include <bcmdefs.h>

/*
 * Shared mappings implemented 30.11.1994. It's not fully working yet,
 * though.
 *
 * Shared mappings now work. 15.8.1995  Bruno.
 *
 * finished 'unifying' the page and buffer cache and SMP-threaded the
 * page-cache, 21.05.1999, Ingo Molnar <mingo@redhat.com>
 *
 * SMP-threaded pagemap-LRU 1999, Andrea Arcangeli <andrea@suse.de>
 */

/*
 * Lock ordering:
 *
 *  ->i_mmap_lock		(truncate_pagecache)
 *    ->private_lock		(__free_pte->__set_page_dirty_buffers)
 *      ->swap_lock		(exclusive_swap_page, others)
 *        ->mapping->tree_lock
 *
 *  ->i_mutex
 *    ->i_mmap_lock		(truncate->unmap_mapping_range)
 *
 *  ->mmap_sem
 *    ->i_mmap_lock
 *      ->page_table_lock or pte_lock	(various, mainly in memory.c)
 *        ->mapping->tree_lock	(arch-dependent flush_dcache_mmap_lock)
 *
 *  ->mmap_sem
 *    ->lock_page		(access_process_vm)
 *
 *  ->i_mutex			(generic_file_buffered_write)
 *    ->mmap_sem		(fault_in_pages_readable->do_page_fault)
 *
 *  ->i_mutex
 *    ->i_alloc_sem             (various)
 *
 *  ->inode_lock
 *    ->sb_lock			(fs/fs-writeback.c)
 *    ->mapping->tree_lock	(__sync_single_inode)
 *
 *  ->i_mmap_lock
 *    ->anon_vma.lock		(vma_adjust)
 *
 *  ->anon_vma.lock
 *    ->page_table_lock or pte_lock	(anon_vma_prepare and various)
 *
 *  ->page_table_lock or pte_lock
 *    ->swap_lock		(try_to_unmap_one)
 *    ->private_lock		(try_to_unmap_one)
 *    ->tree_lock		(try_to_unmap_one)
 *    ->zone.lru_lock		(follow_page->mark_page_accessed)
 *    ->zone.lru_lock		(check_pte_range->isolate_lru_page)
 *    ->private_lock		(page_remove_rmap->set_page_dirty)
 *    ->tree_lock		(page_remove_rmap->set_page_dirty)
 *    ->inode_lock		(page_remove_rmap->set_page_dirty)
 *    ->inode_lock		(zap_pte_range->set_page_dirty)
 *    ->private_lock		(zap_pte_range->__set_page_dirty_buffers)
 *
 *  ->task->proc_lock
 *    ->dcache_lock		(proc_pid_lookup)
 *
 *  (code doesn't rely on that order, so you could switch it around)
 *  ->tasklist_lock             (memory_failure, collect_procs_ao)
 *    ->i_mmap_lock
 */

/*
 * Remove a page from the page cache and free it. Caller has to make
 * sure the page is locked and that nobody else uses it - or that usage
 * is safe.  The caller must hold the mapping's tree_lock.
 */
void __remove_from_page_cache(struct page *page)
{
	struct address_space *mapping = page->mapping;

	radix_tree_delete(&mapping->page_tree, page->index);
	page->mapping = NULL;
	mapping->nrpages--;
	__dec_zone_page_state(page, NR_FILE_PAGES);
	if (PageSwapBacked(page))
		__dec_zone_page_state(page, NR_SHMEM);
	BUG_ON(page_mapped(page));

	/*
	 * Some filesystems seem to re-dirty the page even after
	 * the VM has canceled the dirty bit (eg ext3 journaling).
	 *
	 * Fix it up by doing a final dirty accounting check after
	 * having removed the page entirely.
	 */
	if (PageDirty(page) && mapping_cap_account_dirty(mapping)) {
		dec_zone_page_state(page, NR_FILE_DIRTY);
		dec_bdi_stat(mapping->backing_dev_info, BDI_RECLAIMABLE);
	}
}

void remove_from_page_cache(struct page *page)
{
	struct address_space *mapping = page->mapping;

	BUG_ON(!PageLocked(page));

	spin_lock_irq(&mapping->tree_lock);
	__remove_from_page_cache(page);
	spin_unlock_irq(&mapping->tree_lock);
	mem_cgroup_uncharge_cache_page(page);
}
EXPORT_SYMBOL(remove_from_page_cache);

static int sync_page(void *word)
{
	struct address_space *mapping;
	struct page *page;

	page = container_of((unsigned long *)word, struct page, flags);

	/*
	 * page_mapping() is being called without PG_locked held.
	 * Some knowledge of the state and use of the page is used to
	 * reduce the requirements down to a memory barrier.
	 * The danger here is of a stale page_mapping() return value
	 * indicating a struct address_space different from the one it's
	 * associated with when it is associated with one.
	 * After smp_mb(), it's either the correct page_mapping() for
	 * the page, or an old page_mapping() and the page's own
	 * page_mapping() has gone NULL.
	 * The ->sync_page() address_space operation must tolerate
	 * page_mapping() going NULL. By an amazing coincidence,
	 * this comes about because none of the users of the page
	 * in the ->sync_page() methods make essential use of the
	 * page_mapping(), merely passing the page down to the backing
	 * device's unplug functions when it's non-NULL, which in turn
	 * ignore it for all cases but swap, where only page_private(page) is
	 * of interest. When page_mapping() does go NULL, the entire
	 * call stack gracefully ignores the page and returns.
	 * -- wli
	 */
	smp_mb();
	mapping = page_mapping(page);
	if (mapping && mapping->a_ops && mapping->a_ops->sync_page)
		mapping->a_ops->sync_page(page);
	io_schedule();
	return 0;
}

static int sync_page_killable(void *word)
{
	sync_page(word);
	return fatal_signal_pending(current) ? -EINTR : 0;
}

/**
 * __filemap_fdatawrite_range - start writeback on mapping dirty pages in range
 * @mapping:	address space structure to write
 * @start:	offset in bytes where the range starts
 * @end:	offset in bytes where the range ends (inclusive)
 * @sync_mode:	enable synchronous operation
 *
 * Start writeback against all of a mapping's dirty pages that lie
 * within the byte offsets <start, end> inclusive.
 *
 * If sync_mode is WB_SYNC_ALL then this is a "data integrity" operation, as
 * opposed to a regular memory cleansing writeback.  The difference between
 * these two operations is that if a dirty page/buffer is encountered, it must
 * be waited upon, and not just skipped over.
 */
int __filemap_fdatawrite_range(struct address_space *mapping, loff_t start,
				loff_t end, int sync_mode)
{
	int ret;
	struct writeback_control wbc = {
		.sync_mode = sync_mode,
		.nr_to_write = LONG_MAX,
		.range_start = start,
		.range_end = end,
	};

	if (!mapping_cap_writeback_dirty(mapping))
		return 0;

	ret = do_writepages(mapping, &wbc);
	return ret;
}

static inline int __filemap_fdatawrite(struct address_space *mapping,
	int sync_mode)
{
	return __filemap_fdatawrite_range(mapping, 0, LLONG_MAX, sync_mode);
}

int filemap_fdatawrite(struct address_space *mapping)
{
	return __filemap_fdatawrite(mapping, WB_SYNC_ALL);
}
EXPORT_SYMBOL(filemap_fdatawrite);

int filemap_fdatawrite_range(struct address_space *mapping, loff_t start,
				loff_t end)
{
	return __filemap_fdatawrite_range(mapping, start, end, WB_SYNC_ALL);
}
EXPORT_SYMBOL(filemap_fdatawrite_range);

/**
 * filemap_flush - mostly a non-blocking flush
 * @mapping:	target address_space
 *
 * This is a mostly non-blocking flush.  Not suitable for data-integrity
 * purposes - I/O may not be started against all dirty pages.
 */
int filemap_flush(struct address_space *mapping)
{
	return __filemap_fdatawrite(mapping, WB_SYNC_NONE);
}
EXPORT_SYMBOL(filemap_flush);

/**
 * filemap_fdatawait_range - wait for writeback to complete
 * @mapping:		address space structure to wait for
 * @start_byte:		offset in bytes where the range starts
 * @end_byte:		offset in bytes where the range ends (inclusive)
 *
 * Walk the list of under-writeback pages of the given address space
 * in the given range and wait for all of them.
 */
int filemap_fdatawait_range(struct address_space *mapping, loff_t start_byte,
			    loff_t end_byte)
{
	pgoff_t index = start_byte >> PAGE_CACHE_SHIFT;
	pgoff_t end = end_byte >> PAGE_CACHE_SHIFT;
	struct pagevec pvec;
	int nr_pages;
	int ret = 0;

	if (end_byte < start_byte)
		return 0;

	pagevec_init(&pvec, 0);
	while ((index <= end) &&
			(nr_pages = pagevec_lookup_tag(&pvec, mapping, &index,
			PAGECACHE_TAG_WRITEBACK,
			min(end - index, (pgoff_t)PAGEVEC_SIZE-1) + 1)) != 0) {
		unsigned i;

		for (i = 0; i < nr_pages; i++) {
			struct page *page = pvec.pages[i];

			/* until radix tree lookup accepts end_index */
			if (page->index > end)
				continue;

			wait_on_page_writeback(page);
			if (PageError(page))
				ret = -EIO;
		}
		pagevec_release(&pvec);
		cond_resched();
	}

	/* Check for outstanding write errors */
	if (test_and_clear_bit(AS_ENOSPC, &mapping->flags))
		ret = -ENOSPC;
	if (test_and_clear_bit(AS_EIO, &mapping->flags))
		ret = -EIO;

	return ret;
}
EXPORT_SYMBOL(filemap_fdatawait_range);

/**
 * filemap_fdatawait - wait for all under-writeback pages to complete
 * @mapping: address space structure to wait for
 *
 * Walk the list of under-writeback pages of the given address space
 * and wait for all of them.
 */
int filemap_fdatawait(struct address_space *mapping)
{
	loff_t i_size = i_size_read(mapping->host);

	if (i_size == 0)
		return 0;

	return filemap_fdatawait_range(mapping, 0, i_size - 1);
}
EXPORT_SYMBOL(filemap_fdatawait);

int filemap_write_and_wait(struct address_space *mapping)
{
	int err = 0;

	if (mapping->nrpages) {
		err = filemap_fdatawrite(mapping);
		/*
		 * Even if the above returned error, the pages may be
		 * written partially (e.g. -ENOSPC), so we wait for it.
		 * But the -EIO is special case, it may indicate the worst
		 * thing (e.g. bug) happened, so we avoid waiting for it.
		 */
		if (err != -EIO) {
			int err2 = filemap_fdatawait(mapping);
			if (!err)
				err = err2;
		}
	}
	return err;
}
EXPORT_SYMBOL(filemap_write_and_wait);

/**
 * filemap_write_and_wait_range - write out & wait on a file range
 * @mapping:	the address_space for the pages
 * @lstart:	offset in bytes where the range starts
 * @lend:	offset in bytes where the range ends (inclusive)
 *
 * Write out and wait upon file offsets lstart->lend, inclusive.
 *
 * Note that `lend' is inclusive (describes the last byte to be written) so
 * that this function can be used to write to the very end-of-file (end = -1).
 */
int filemap_write_and_wait_range(struct address_space *mapping,
				 loff_t lstart, loff_t lend)
{
	int err = 0;

	if (mapping->nrpages) {
		err = __filemap_fdatawrite_range(mapping, lstart, lend,
						 WB_SYNC_ALL);
		/* See comment of filemap_write_and_wait() */
		if (err != -EIO) {
			int err2 = filemap_fdatawait_range(mapping,
						lstart, lend);
			if (!err)
				err = err2;
		}
	}
	return err;
}
EXPORT_SYMBOL(filemap_write_and_wait_range);

/**
 * add_to_page_cache_locked - add a locked page to the pagecache
 * @page:	page to add
 * @mapping:	the page's address_space
 * @offset:	page index
 * @gfp_mask:	page allocation mode
 *
 * This function is used to add a page to the pagecache. It must be locked.
 * This function does not add the page to the LRU.  The caller must do that.
 */
int add_to_page_cache_locked(struct page *page, struct address_space *mapping,
		pgoff_t offset, gfp_t gfp_mask)
{
	int error;

	VM_BUG_ON(!PageLocked(page));

	error = mem_cgroup_cache_charge(page, current->mm,
					gfp_mask & GFP_RECLAIM_MASK);
	if (error)
		goto out;

	error = radix_tree_preload(gfp_mask & ~__GFP_HIGHMEM);
	if (error == 0) {
		page_cache_get(page);
		page->mapping = mapping;
		page->index = offset;

		spin_lock_irq(&mapping->tree_lock);
		error = radix_tree_insert(&mapping->page_tree, offset, page);
		if (likely(!error)) {
			mapping->nrpages++;
			__inc_zone_page_state(page, NR_FILE_PAGES);
			if (PageSwapBacked(page))
				__inc_zone_page_state(page, NR_SHMEM);
			spin_unlock_irq(&mapping->tree_lock);
		} else {
			page->mapping = NULL;
			spin_unlock_irq(&mapping->tree_lock);
			mem_cgroup_uncharge_cache_page(page);
			page_cache_release(page);
		}
		radix_tree_preload_end();
	} else
		mem_cgroup_uncharge_cache_page(page);
out:
	return error;
}
EXPORT_SYMBOL(add_to_page_cache_locked);

int add_to_page_cache_lru(struct page *page, struct address_space *mapping,
				pgoff_t offset, gfp_t gfp_mask)
{
	int ret;

	/*
	 * Splice_read and readahead add shmem/tmpfs pages into the page cache
	 * before shmem_readpage has a chance to mark them as SwapBacked: they
	 * need to go on the anon lru below, and mem_cgroup_cache_charge
	 * (called in add_to_page_cache) needs to know where they're going too.
	 */
	if (mapping_cap_swap_backed(mapping))
		SetPageSwapBacked(page);

	ret = add_to_page_cache(page, mapping, offset, gfp_mask);
	if (ret == 0) {
		if (page_is_file_cache(page))
			lru_cache_add_file(page);
		else
			lru_cache_add_anon(page);
	}
	return ret;
}
EXPORT_SYMBOL_GPL(add_to_page_cache_lru);

#ifdef CONFIG_NUMA
struct page *__page_cache_alloc(gfp_t gfp)
{
	int n;
	struct page *page;

	if (cpuset_do_page_mem_spread()) {
		get_mems_allowed();
		n = cpuset_mem_spread_node();
		page = alloc_pages_exact_node(n, gfp, 0);
		put_mems_allowed();
		return page;
	}
	return alloc_pages(gfp, 0);
}
EXPORT_SYMBOL(__page_cache_alloc);
#endif

static int __sleep_on_page_lock(void *word)
{
	io_schedule();
	return 0;
}

/*
 * In order to wait for pages to become available there must be
 * waitqueues associated with pages. By using a hash table of
 * waitqueues where the bucket discipline is to maintain all
 * waiters on the same queue and wake all when any of the pages
 * become available, and for the woken contexts to check to be
 * sure the appropriate page became available, this saves space
 * at a cost of "thundering herd" phenomena during rare hash
 * collisions.
 */
static wait_queue_head_t *page_waitqueue(struct page *page)
{
	const struct zone *zone = page_zone(page);

	return &zone->wait_table[hash_ptr(page, zone->wait_table_bits)];
}

static inline void wake_up_page(struct page *page, int bit)
{
	__wake_up_bit(page_waitqueue(page), &page->flags, bit);
}

void wait_on_page_bit(struct page *page, int bit_nr)
{
	DEFINE_WAIT_BIT(wait, &page->flags, bit_nr);

	if (test_bit(bit_nr, &page->flags))
		__wait_on_bit(page_waitqueue(page), &wait, sync_page,
							TASK_UNINTERRUPTIBLE);
}
EXPORT_SYMBOL(wait_on_page_bit);

/**
 * add_page_wait_queue - Add an arbitrary waiter to a page's wait queue
 * @page: Page defining the wait queue of interest
 * @waiter: Waiter to add to the queue
 *
 * Add an arbitrary @waiter to the wait queue for the nominated @page.
 */
void add_page_wait_queue(struct page *page, wait_queue_t *waiter)
{
	wait_queue_head_t *q = page_waitqueue(page);
	unsigned long flags;

	spin_lock_irqsave(&q->lock, flags);
	__add_wait_queue(q, waiter);
	spin_unlock_irqrestore(&q->lock, flags);
}
EXPORT_SYMBOL_GPL(add_page_wait_queue);

/**
 * unlock_page - unlock a locked page
 * @page: the page
 *
 * Unlocks the page and wakes up sleepers in ___wait_on_page_locked().
 * Also wakes sleepers in wait_on_page_writeback() because the wakeup
 * mechananism between PageLocked pages and PageWriteback pages is shared.
 * But that's OK - sleepers in wait_on_page_writeback() just go back to sleep.
 *
 * The mb is necessary to enforce ordering between the clear_bit and the read
 * of the waitqueue (to avoid SMP races with a parallel wait_on_page_locked()).
 */
void BCMFASTPATH_HOST unlock_page(struct page *page)
{
	VM_BUG_ON(!PageLocked(page));
	clear_bit_unlock(PG_locked, &page->flags);
	smp_mb__after_clear_bit();
	wake_up_page(page, PG_locked);
}
EXPORT_SYMBOL(unlock_page);

/**
 * end_page_writeback - end writeback against a page
 * @page: the page
 */
void end_page_writeback(struct page *page)
{
	if (TestClearPageReclaim(page))
		rotate_reclaimable_page(page);

	if (!test_clear_page_writeback(page))
		BUG();

	smp_mb__after_clear_bit();
	wake_up_page(page, PG_writeback);
}
EXPORT_SYMBOL(end_page_writeback);

/**
 * __lock_page - get a lock on the page, assuming we need to sleep to get it
 * @page: the page to lock
 *
 * Ugly. Running sync_page() in state TASK_UNINTERRUPTIBLE is scary.  If some
 * random driver's requestfn sets TASK_RUNNING, we could busywait.  However
 * chances are that on the second loop, the block layer's plug list is empty,
 * so sync_page() will then return in state TASK_UNINTERRUPTIBLE.
 */
void __lock_page(struct page *page)
{
	DEFINE_WAIT_BIT(wait, &page->flags, PG_locked);

	__wait_on_bit_lock(page_waitqueue(page), &wait, sync_page,
							TASK_UNINTERRUPTIBLE);
}
EXPORT_SYMBOL(__lock_page);

int __lock_page_killable(struct page *page)
{
	DEFINE_WAIT_BIT(wait, &page->flags, PG_locked);

	return __wait_on_bit_lock(page_waitqueue(page), &wait,
					sync_page_killable, TASK_KILLABLE);
}
EXPORT_SYMBOL_GPL(__lock_page_killable);

/**
 * __lock_page_nosync - get a lock on the page, without calling sync_page()
 * @page: the page to lock
 *
 * Variant of lock_page that does not require the caller to hold a reference
 * on the page's mapping.
 */
void __lock_page_nosync(struct page *page)
{
	DEFINE_WAIT_BIT(wait, &page->flags, PG_locked);
	__wait_on_bit_lock(page_waitqueue(page), &wait, __sleep_on_page_lock,
							TASK_UNINTERRUPTIBLE);
}

/**
 * find_get_page - find and get a page reference
 * @mapping: the address_space to search
 * @offset: the page index
 *
 * Is there a pagecache struct page at the given (mapping, offset) tuple?
 * If yes, increment its refcount and return it; if no, return NULL.
 */
struct page *find_get_page(struct address_space *mapping, pgoff_t offset)
{
	void **pagep;
	struct page *page;

	rcu_read_lock();
repeat:
	page = NULL;
	pagep = radix_tree_lookup_slot(&mapping->page_tree, offset);
	if (pagep) {
		page = radix_tree_deref_slot(pagep);
		if (unlikely(!page))
			goto out;
		if (radix_tree_deref_retry(page))
			goto repeat;

		if (!page_cache_get_speculative(page))
			goto repeat;

		/*
		 * Has the page moved?
		 * This is part of the lockless pagecache protocol. See
		 * include/linux/pagemap.h for details.
		 */
		if (unlikely(page != *pagep)) {
			page_cache_release(page);
			goto repeat;
		}
	}
out:
	rcu_read_unlock();

	return page;
}
EXPORT_SYMBOL(find_get_page);

/**
 * find_lock_page - locate, pin and lock a pagecache page
 * @mapping: the address_space to search
 * @offset: the page index
 *
 * Locates the desired pagecache page, locks it, increments its reference
 * count and returns its address.
 *
 * Returns zero if the page was not present. find_lock_page() may sleep.
 */
struct page *find_lock_page(struct address_space *mapping, pgoff_t offset)
{
	struct page *page;

repeat:
	page = find_get_page(mapping, offset);
	if (page) {
		lock_page(page);
		/* Has the page been truncated? */
		if (unlikely(page->mapping != mapping)) {
			unlock_page(page);
			page_cache_release(page);
			goto repeat;
		}
		VM_BUG_ON(page->index != offset);
	}
	return page;
}
EXPORT_SYMBOL(find_lock_page);

/**
 * find_or_create_page - locate or add a pagecache page
 * @mapping: the page's address_space
 * @index: the page's index into the mapping
 * @gfp_mask: page allocation mode
 *
 * Locates a page in the pagecache.  If the page is not present, a new page
 * is allocated using @gfp_mask and is added to the pagecache and to the VM's
 * LRU list.  The returned page is locked and has its reference count
 * incremented.
 *
 * find_or_create_page() may sleep, even if @gfp_flags specifies an atomic
 * allocation!
 *
 * find_or_create_page() returns the desired page's address, or zero on
 * memory exhaustion.
 */
struct page *find_or_create_page(struct address_space *mapping,
		pgoff_t index, gfp_t gfp_mask)
{
	struct page *page;
	int err;
repeat:
	page = find_lock_page(mapping, index);
	if (!page) {
		page = __page_cache_alloc(gfp_mask);
		if (!page)
			return NULL;
		/*
		 * We want a regular kernel memory (not highmem or DMA etc)
		 * allocation for the radix tree nodes, but we need to honour
		 * the context-specific requirements the caller has asked for.
		 * GFP_RECLAIM_MASK collects those requirements.
		 */
		err = add_to_page_cache_lru(page, mapping, index,
			(gfp_mask & GFP_RECLAIM_MASK));
		if (unlikely(err)) {
			page_cache_release(page);
			page = NULL;
			if (err == -EEXIST)
				goto repeat;
		}
	}
	return page;
}
EXPORT_SYMBOL(find_or_create_page);

/**
 * find_get_pages - gang pagecache lookup
 * @mapping:	The address_space to search
 * @start:	The starting page index
 * @nr_pages:	The maximum number of pages
 * @pages:	Where the resulting pages are placed
 *
 * find_get_pages() will search for and return a group of up to
 * @nr_pages pages in the mapping.  The pages are placed at @pages.
 * find_get_pages() takes a reference against the returned pages.
 *
 * The search returns a group of mapping-contiguous pages with ascending
 * indexes.  There may be holes in the indices due to not-present pages.
 *
 * find_get_pages() returns the number of pages which were found.
 */
unsigned find_get_pages(struct address_space *mapping, pgoff_t start,
			    unsigned int nr_pages, struct page **pages)
{
	unsigned int i;
	unsigned int ret;
	unsigned int nr_found;

	rcu_read_lock();
restart:
	nr_found = radix_tree_gang_lookup_slot(&mapping->page_tree,
				(void ***)pages, start, nr_pages);
	ret = 0;
	for (i = 0; i < nr_found; i++) {
		struct page *page;
repeat:
		page = radix_tree_deref_slot((void **)pages[i]);
		if (unlikely(!page))
			continue;
		if (radix_tree_deref_retry(page)) {
			if (ret)
				start = pages[ret-1]->index;
			goto restart;
		}

		if (!page_cache_get_speculative(page))
			goto repeat;

		/* Has the page moved? */
		if (unlikely(page != *((void **)pages[i]))) {
			page_cache_release(page);
			goto repeat;
		}

		pages[ret] = page;
		ret++;
	}
	rcu_read_unlock();
	return ret;
}

/**
 * find_get_pages_contig - gang contiguous pagecache lookup
 * @mapping:	The address_space to search
 * @index:	The starting page index
 * @nr_pages:	The maximum number of pages
 * @pages:	Where the resulting pages are placed
 *
 * find_get_pages_contig() works exactly like find_get_pages(), except
 * that the returned number of pages are guaranteed to be contiguous.
 *
 * find_get_pages_contig() returns the number of pages which were found.
 */
unsigned BCMFASTPATH_HOST find_get_pages_contig(struct address_space *mapping, pgoff_t index,
			       unsigned int nr_pages, struct page **pages)
{
	unsigned int i;
	unsigned int ret;
	unsigned int nr_found;

	rcu_read_lock();
restart:
	nr_found = radix_tree_gang_lookup_slot(&mapping->page_tree,
				(void ***)pages, index, nr_pages);
	ret = 0;
	for (i = 0; i < nr_found; i++) {
		struct page *page;
repeat:
		page = radix_tree_deref_slot((void **)pages[i]);
		if (unlikely(!page))
			continue;
		if (radix_tree_deref_retry(page))
			goto restart;

		if (page->mapping == NULL || page->index != index)
			break;

		if (!page_cache_get_speculative(page))
			goto repeat;

		/* Has the page moved? */
		if (unlikely(page != *((void **)pages[i]))) {
			page_cache_release(page);
			goto repeat;
		}

		pages[ret] = page;
		ret++;
		index++;
	}
	rcu_read_unlock();
	return ret;
}
EXPORT_SYMBOL(find_get_pages_contig);

/**
 * find_get_pages_tag - find and return pages that match @tag
 * @mapping:	the address_space to search
 * @index:	the starting page index
 * @tag:	the tag index
 * @nr_pages:	the maximum number of pages
 * @pages:	where the resulting pages are placed
 *
 * Like find_get_pages, except we only return pages which are tagged with
 * @tag.   We update @index to index the next page for the traversal.
 */
unsigned find_get_pages_tag(struct address_space *mapping, pgoff_t *index,
			int tag, unsigned int nr_pages, struct page **pages)
{
	unsigned int i;
	unsigned int ret;
	unsigned int nr_found;

	rcu_read_lock();
restart:
	nr_found = radix_tree_gang_lookup_tag_slot(&mapping->page_tree,
				(void ***)pages, *index, nr_pages, tag);
	ret = 0;
	for (i = 0; i < nr_found; i++) {
		struct page *page;
repeat:
		page = radix_tree_deref_slot((void **)pages[i]);
		if (unlikely(!page))
			continue;
		if (radix_tree_deref_retry(page))
			goto restart;

		if (!page_cache_get_speculative(page))
			goto repeat;

		/* Has the page moved? */
		if (unlikely(page != *((void **)pages[i]))) {
			page_cache_release(page);
			goto repeat;
		}

		pages[ret] = page;
		ret++;
	}
	rcu_read_unlock();

	if (ret)
		*index = pages[ret - 1]->index + 1;

	return ret;
}
EXPORT_SYMBOL(find_get_pages_tag);

/**
 * grab_cache_page_nowait - returns locked page at given index in given cache
 * @mapping: target address_space
 * @index: the page index
 *
 * Same as grab_cache_page(), but do not wait if the page is unavailable.
 * This is intended for speculative data generators, where the data can
 * be regenerated if the page couldn't be grabbed.  This routine should
 * be safe to call while holding the lock for another page.
 *
 * Clear __GFP_FS when allocating the page to avoid recursion into the fs
 * and deadlock against the caller's locked page.
 */
struct page *
grab_cache_page_nowait(struct address_space *mapping, pgoff_t index)
{
	struct page *page = find_get_page(mapping, index);

	if (page) {
		if (trylock_page(page))
			return page;
		page_cache_release(page);
		return NULL;
	}
	page = __page_cache_alloc(mapping_gfp_mask(mapping) & ~__GFP_FS);
	if (page && add_to_page_cache_lru(page, mapping, index, GFP_NOFS)) {
		page_cache_release(page);
		page = NULL;
	}
	return page;
}
EXPORT_SYMBOL(grab_cache_page_nowait);

/*
 * CD/DVDs are error prone. When a medium error occurs, the driver may fail
 * a _large_ part of the i/o request. Imagine the worst scenario:
 *
 *      ---R__________________________________________B__________
 *         ^ reading here                             ^ bad block(assume 4k)
 *
 * read(R) => miss => readahead(R...B) => media error => frustrating retries
 * => failing the whole request => read(R) => read(R+1) =>
 * readahead(R+1...B+1) => bang => read(R+2) => read(R+3) =>
 * readahead(R+3...B+2) => bang => read(R+3) => read(R+4) =>
 * readahead(R+4...B+3) => bang => read(R+4) => read(R+5) => ......
 *
 * It is going insane. Fix it by quickly scaling down the readahead size.
 */
static void shrink_readahead_size_eio(struct file *filp,
					struct file_ra_state *ra)
{
	ra->ra_pages /= 4;
}

/**
 * do_generic_file_read - generic file read routine
 * @filp:	the file to read
 * @ppos:	current file position
 * @desc:	read_descriptor
 * @actor:	read method
 *
 * This is a generic file read routine, and uses the
 * mapping->a_ops->readpage() function for the actual low-level stuff.
 *
 * This is really ugly. But the goto's actually try to clarify some
 * of the logic when it comes to error handling etc.
 */
static void do_generic_file_read(struct file *filp, loff_t *ppos,
		read_descriptor_t *desc, read_actor_t actor)
{
	struct address_space *mapping = filp->f_mapping;
	struct inode *inode = mapping->host;
	struct file_ra_state *ra = &filp->f_ra;
	pgoff_t index;
	pgoff_t last_index;
	pgoff_t prev_index;
	unsigned long offset;      /* offset into pagecache page */
	unsigned int prev_offset;
	int error;

	index = *ppos >> PAGE_CACHE_SHIFT;
	prev_index = ra->prev_pos >> PAGE_CACHE_SHIFT;
	prev_offset = ra->prev_pos & (PAGE_CACHE_SIZE-1);
	last_index = (*ppos + desc->count + PAGE_CACHE_SIZE-1) >> PAGE_CACHE_SHIFT;
	offset = *ppos & ~PAGE_CACHE_MASK;

	for (;;) {
		struct page *page;
		pgoff_t end_index;
		loff_t isize;
		unsigned long nr, ret;

		cond_resched();
find_page:
		page = find_get_page(mapping, index);
		if (!page) {
			page_cache_sync_readahead(mapping,
					ra, filp,
					index, last_index - index);
			page = find_get_page(mapping, index);
			if (unlikely(page == NULL))
				goto no_cached_page;
		}
		if (PageReadahead(page)) {
			page_cache_async_readahead(mapping,
					ra, filp, page,
					index, last_index - index);
		}
		if (!PageUptodate(page)) {
			if (inode->i_blkbits == PAGE_CACHE_SHIFT ||
					!mapping->a_ops->is_partially_uptodate)
				goto page_not_up_to_date;
			if (!trylock_page(page))
				goto page_not_up_to_date;
			/* Did it get truncated before we got the lock? */
			if (!page->mapping)
				goto page_not_up_to_date_locked;
			if (!mapping->a_ops->is_partially_uptodate(page,
								desc, offset))
				goto page_not_up_to_date_locked;
			unlock_page(page);
		}
page_ok:
		/*
		 * i_size must be checked after we know the page is Uptodate.
		 *
		 * Checking i_size after the check allows us to calculate
		 * the correct value for "nr", which means the zero-filled
		 * part of the page is not copied back to userspace (unless
		 * another truncate extends the file - this is desired though).
		 */

		isize = i_size_read(inode);
		end_index = (isize - 1) >> PAGE_CACHE_SHIFT;
		if (unlikely(!isize || index > end_index)) {
			page_cache_release(page);
			goto out;
		}

		/* nr is the maximum number of bytes to copy from this page */
		nr = PAGE_CACHE_SIZE;
		if (index == end_index) {
			nr = ((isize - 1) & ~PAGE_CACHE_MASK) + 1;
			if (nr <= offset) {
				page_cache_release(page);
				goto out;
			}
		}
		nr = nr - offset;

		/* If users can be writing to this page using arbitrary
		 * virtual addresses, take care about potential aliasing
		 * before reading the page on the kernel side.
		 */
		if (mapping_writably_mapped(mapping))
			flush_dcache_page(page);

		/*
		 * When a sequential read accesses a page several times,
		 * only mark it as accessed the first time.
		 */
		if (prev_index != index || offset != prev_offset)
			mark_page_accessed(page);
		prev_index = index;

		/*
		 * Ok, we have the page, and it's up-to-date, so
		 * now we can copy it to user space...
		 *
		 * The actor routine returns how many bytes were actually used..
		 * NOTE! This may not be the same as how much of a user buffer
		 * we filled up (we may be padding etc), so we can only update
		 * "pos" here (the actor routine has to update the user buffer
		 * pointers and the remaining count).
		 */
		ret = actor(desc, page, offset, nr);
		offset += ret;
		index += offset >> PAGE_CACHE_SHIFT;
		offset &= ~PAGE_CACHE_MASK;
		prev_offset = offset;

		page_cache_release(page);
		if (ret == nr && desc->count)
			continue;
		goto out;

page_not_up_to_date:
		/* Get exclusive access to the page ... */
		error = lock_page_killable(page);
		if (unlikely(error))
			goto readpage_error;

page_not_up_to_date_locked:
		/* Did it get truncated before we got the lock? */
		if (!page->mapping) {
			unlock_page(page);
			page_cache_release(page);
			continue;
		}

		/* Did somebody else fill it already? */
		if (PageUptodate(page)) {
			unlock_page(page);
			goto page_ok;
		}

readpage:
		/*
		 * A previous I/O error may have been due to temporary
		 * failures, eg. multipath errors.
		 * PG_error will be set again if readpage fails.
		 */
		ClearPageError(page);
		/* Start the actual read. The read will unlock the page. */
		error = mapping->a_ops->readpage(filp, page);

		if (unlikely(error)) {
			if (error == AOP_TRUNCATED_PAGE) {
				page_cache_release(page);
				goto find_page;
			}
			goto readpage_error;
		}

		if (!PageUptodate(page)) {
			error = lock_page_killable(page);
			if (unlikely(error))
				goto readpage_error;
			if (!PageUptodate(page)) {
				if (page->mapping == NULL) {
					/*
					 * invalidate_mapping_pages got it
					 */
					unlock_page(page);
					page_cache_release(page);
					goto find_page;
				}
				unlock_page(page);
				shrink_readahead_size_eio(filp, ra);
				error = -EIO;
				goto readpage_error;
			}
			unlock_page(page);
		}

		goto page_ok;

readpage_error:
		/* UHHUH! A synchronous read error occurred. Report it */
		desc->error = error;
		page_cache_release(page);
		goto out;

no_cached_page:
		/*
		 * Ok, it wasn't cached, so we need to create a new
		 * page..
		 */
		page = page_cache_alloc_cold(mapping);
		if (!page) {
			desc->error = -ENOMEM;
			goto out;
		}
		error = add_to_page_cache_lru(page, mapping,
						index, GFP_KERNEL);
		if (error) {
			page_cache_release(page);
			if (error == -EEXIST)
				goto find_page;
			desc->error = error;
			goto out;
		}
		goto readpage;
	}

out:
	ra->prev_pos = prev_index;
	ra->prev_pos <<= PAGE_CACHE_SHIFT;
	ra->prev_pos |= prev_offset;

	*ppos = ((loff_t)index << PAGE_CACHE_SHIFT) + offset;
	file_accessed(filp);
}

int file_read_actor(read_descriptor_t *desc, struct page *page,
			unsigned long offset, unsigned long size)
{
	char *kaddr;
	unsigned long left, count = desc->count;

	if (size > count)
		size = count;

	/*
	 * Faults on the destination of a read are common, so do it before
	 * taking the kmap.
	 */
	if (!fault_in_pages_writeable(desc->arg.buf, size)) {
		kaddr = kmap_atomic(page, KM_USER0);
		left = __copy_to_user_inatomic(desc->arg.buf,
						kaddr + offset, size);
		kunmap_atomic(kaddr, KM_USER0);
		if (left == 0)
			goto success;
	}

	/* Do it the slow way */
	kaddr = kmap(page);
	left = __copy_to_user(desc->arg.buf, kaddr + offset, size);
	kunmap(page);

	if (left) {
		size -= left;
		desc->error = -EFAULT;
	}
success:
	desc->count = count - size;
	desc->written += size;
	desc->arg.buf += size;
	return size;
}

/*
 * Performs necessary checks before doing a write
 * @iov:	io vector request
 * @nr_segs:	number of segments in the iovec
 * @count:	number of bytes to write
 * @access_flags: type of access: %VERIFY_READ or %VERIFY_WRITE
 *
 * Adjust number of segments and amount of bytes to write (nr_segs should be
 * properly initialized first). Returns appropriate error code that caller
 * should return or zero in case that write should be allowed.
 */
int generic_segment_checks(const struct iovec *iov,
			unsigned long *nr_segs, size_t *count, int access_flags)
{
	unsigned long   seg;
	size_t cnt = 0;
	for (seg = 0; seg < *nr_segs; seg++) {
		const struct iovec *iv = &iov[seg];

		/*
		 * If any segment has a negative length, or the cumulative
		 * length ever wraps negative then return -EINVAL.
		 */
		cnt += iv->iov_len;
		if (unlikely((ssize_t)(cnt|iv->iov_len) < 0))
			return -EINVAL;
		if (access_ok(access_flags, iv->iov_base, iv->iov_len))
			continue;
		if (seg == 0)
			return -EFAULT;
		*nr_segs = seg;
		cnt -= iv->iov_len;	/* This segment is no good */
		break;
	}
	*count = cnt;
	return 0;
}
EXPORT_SYMBOL(generic_segment_checks);

/**
 * generic_file_aio_read - generic filesystem read routine
 * @iocb:	kernel I/O control block
 * @iov:	io vector request
 * @nr_segs:	number of segments in the iovec
 * @pos:	current file position
 *
 * This is the "read()" routine for all filesystems
 * that can use the page cache directly.
 */
ssize_t
generic_file_aio_read(struct kiocb *iocb, const struct iovec *iov,
		unsigned long nr_segs, loff_t pos)
{
	struct file *filp = iocb->ki_filp;
	ssize_t retval;
	unsigned long seg = 0;
	size_t count;
	loff_t *ppos = &iocb->ki_pos;

	count = 0;
	retval = generic_segment_checks(iov, &nr_segs, &count, VERIFY_WRITE);
	if (retval)
		return retval;

	/* coalesce the iovecs and go direct-to-BIO for O_DIRECT */
	if (filp->f_flags & O_DIRECT) {
		loff_t size;
		struct address_space *mapping;
		struct inode *inode;

		mapping = filp->f_mapping;
		inode = mapping->host;
		if (!count)
			goto out; /* skip atime */
		size = i_size_read(inode);
		if (pos < size) {
			retval = filemap_write_and_wait_range(mapping, pos,
					pos + iov_length(iov, nr_segs) - 1);
			if (!retval) {
				retval = mapping->a_ops->direct_IO(READ, iocb,
							iov, pos, nr_segs);
			}
			if (retval > 0) {
				*ppos = pos + retval;
				count -= retval;
			}

			/*
			 * Btrfs can have a short DIO read if we encounter
			 * compressed extents, so if there was an error, or if
			 * we've already read everything we wanted to, or if
			 * there was a short read because we hit EOF, go ahead
			 * and return.  Otherwise fallthrough to buffered io for
			 * the rest of the read.
			 */
			if (retval < 0 || !count || *ppos >= size) {
				file_accessed(filp);
				goto out;
			}
		}
	}

	count = retval;
	for (seg = 0; seg < nr_segs; seg++) {
		read_descriptor_t desc;
		loff_t offset = 0;

		/*
		 * If we did a short DIO read we need to skip the section of the
		 * iov that we've already read data into.
		 */
		if (count) {
			if (count > iov[seg].iov_len) {
				count -= iov[seg].iov_len;
				continue;
			}
			offset = count;
			count = 0;
		}

		desc.written = 0;
		desc.arg.buf = iov[seg].iov_base + offset;
		desc.count = iov[seg].iov_len - offset;
		if (desc.count == 0)
			continue;
		desc.error = 0;
		do_generic_file_read(filp, ppos, &desc, file_read_actor);
		retval += desc.written;
		if (desc.error) {
			retval = retval ?: desc.error;
			break;
		}
		if (desc.count > 0)
			break;
	}
out:
	return retval;
}
EXPORT_SYMBOL(generic_file_aio_read);

static ssize_t
do_readahead(struct address_space *mapping, struct file *filp,
	     pgoff_t index, unsigned long nr)
{
	if (!mapping || !mapping->a_ops || !mapping->a_ops->readpage)
		return -EINVAL;

	force_page_cache_readahead(mapping, filp, index, nr);
	return 0;
}

SYSCALL_DEFINE(readahead)(int fd, loff_t offset, size_t count)
{
	ssize_t ret;
	struct file *file;

	ret = -EBADF;
	file = fget(fd);
	if (file) {
		if (file->f_mode & FMODE_READ) {
			struct address_space *mapping = file->f_mapping;
			pgoff_t start = offset >> PAGE_CACHE_SHIFT;
			pgoff_t end = (offset + count - 1) >> PAGE_CACHE_SHIFT;
			unsigned long len = end - start + 1;
			ret = do_readahead(mapping, file, start, len);
		}
		fput(file);
	}
	return ret;
}
#ifdef CONFIG_HAVE_SYSCALL_WRAPPERS
asmlinkage long SyS_readahead(long fd, loff_t offset, long count)
{
	return SYSC_readahead((int) fd, offset, (size_t) count);
}
SYSCALL_ALIAS(sys_readahead, SyS_readahead);
#endif

#ifdef CONFIG_MMU
/**
 * page_cache_read - adds requested page to the page cache if not already there
 * @file:	file to read
 * @offset:	page index
 *
 * This adds the requested page to the page cache if it isn't already there,
 * and schedules an I/O to read in its contents from disk.
 */
static int page_cache_read(struct file *file, pgoff_t offset)
{
	struct address_space *mapping = file->f_mapping;
	struct page *page; 
	int ret;

	do {
		page = page_cache_alloc_cold(mapping);
		if (!page)
			return -ENOMEM;

		ret = add_to_page_cache_lru(page, mapping, offset, GFP_KERNEL);
		if (ret == 0)
			ret = mapping->a_ops->readpage(file, page);
		else if (ret == -EEXIST)
			ret = 0; /* losing race to add is OK */

		page_cache_release(page);

	} while (ret == AOP_TRUNCATED_PAGE);
		
	return ret;
}

#define MMAP_LOTSAMISS  (100)

/*
 * Synchronous readahead happens when we don't even find
 * a page in the page cache at all.
 */
static void do_sync_mmap_readahead(struct vm_area_struct *vma,
				   struct file_ra_state *ra,
				   struct file *file,
				   pgoff_t offset)
{
	unsigned long ra_pages;
	struct address_space *mapping = file->f_mapping;

	/* If we don't want any read-ahead, don't bother */
	if (VM_RandomReadHint(vma))
		return;

	if (VM_SequentialReadHint(vma) ||
			offset - 1 == (ra->prev_pos >> PAGE_CACHE_SHIFT)) {
		page_cache_sync_readahead(mapping, ra, file, offset,
					  ra->ra_pages);
		return;
	}

	if (ra->mmap_miss < INT_MAX)
		ra->mmap_miss++;

	/*
	 * Do we miss much more than hit in this file? If so,
	 * stop bothering with read-ahead. It will only hurt.
	 */
	if (ra->mmap_miss > MMAP_LOTSAMISS)
		return;

	/*
	 * mmap read-around
	 */
	ra_pages = max_sane_readahead(ra->ra_pages);
	if (ra_pages) {
		ra->start = max_t(long, 0, offset - ra_pages/2);
		ra->size = ra_pages;
		ra->async_size = 0;
		ra_submit(ra, mapping, file);
	}
}

/*
 * Asynchronous readahead happens when we find the page and PG_readahead,
 * so we want to possibly extend the readahead further..
 */
static void do_async_mmap_readahead(struct vm_area_struct *vma,
				    struct file_ra_state *ra,
				    struct file *file,
				    struct page *page,
				    pgoff_t offset)
{
	struct address_space *mapping = file->f_mapping;

	/* If we don't want any read-ahead, don't bother */
	if (VM_RandomReadHint(vma))
		return;
	if (ra->mmap_miss > 0)
		ra->mmap_miss--;
	if (PageReadahead(page))
		page_cache_async_readahead(mapping, ra, file,
					   page, offset, ra->ra_pages);
}

/**
 * filemap_fault - read in file data for page fault handling
 * @vma:	vma in which the fault was taken
 * @vmf:	struct vm_fault containing details of the fault
 *
 * filemap_fault() is invoked via the vma operations vector for a
 * mapped memory region to read in file data during a page fault.
 *
 * The goto's are kind of ugly, but this streamlines the normal case of having
 * it in the page cache, and handles the special cases reasonably without
 * having a lot of duplicated code.
 */
int filemap_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
	int error;
	struct file *file = vma->vm_file;
	struct address_space *mapping = file->f_mapping;
	struct file_ra_state *ra = &file->f_ra;
	struct inode *inode = mapping->host;
	pgoff_t offset = vmf->pgoff;
	struct page *page;
	pgoff_t size;
	int ret = 0;

	size = (i_size_read(inode) + PAGE_CACHE_SIZE - 1) >> PAGE_CACHE_SHIFT;
	if (offset >= size)
		return VM_FAULT_SIGBUS;

	/*
	 * Do we have something in the page cache already?
	 */
	page = find_get_page(mapping, offset);
	if (likely(page)) {
		/*
		 * We found the page, so try async readahead before
		 * waiting for the lock.
		 */
		do_async_mmap_readahead(vma, ra, file, page, offset);
		lock_page(page);

		/* Did it get truncated? */
		if (unlikely(page->mapping != mapping)) {
			unlock_page(page);
			put_page(page);
			goto no_cached_page;
		}
	} else {
		/* No page in the page cache at all */
		do_sync_mmap_readahead(vma, ra, file, offset);
		count_vm_event(PGMAJFAULT);
		ret = VM_FAULT_MAJOR;
retry_find:
		page = find_lock_page(mapping, offset);
		if (!page)
			goto no_cached_page;
	}

	/*
	 * We have a locked page in the page cache, now we need to check
	 * that it's up-to-date. If not, it is going to be due to an error.
	 */
	if (unlikely(!PageUptodate(page)))
		goto page_not_uptodate;

	/*
	 * Found the page and have a reference on it.
	 * We must recheck i_size under page lock.
	 */
	size = (i_size_read(inode) + PAGE_CACHE_SIZE - 1) >> PAGE_CACHE_SHIFT;
	if (unlikely(offset >= size)) {
		unlock_page(page);
		page_cache_release(page);
		return VM_FAULT_SIGBUS;
	}

	ra->prev_pos = (loff_t)offset << PAGE_CACHE_SHIFT;
	vmf->page = page;
	return ret | VM_FAULT_LOCKED;

no_cached_page:
	/*
	 * We're only likely to ever get here if MADV_RANDOM is in
	 * effect.
	 */
	error = page_cache_read(file, offset);

	/*
	 * The page we want has now been added to the page cache.
	 * In the unlikely event that someone removed it in the
	 * meantime, we'll just come back here and read it again.
	 */
	if (error >= 0)
		goto retry_find;

	/*
	 * An error return from page_cache_read can result if the
	 * system is low on memory, or a problem occurs while trying
	 * to schedule I/O.
	 */
	if (error == -ENOMEM)
		return VM_FAULT_OOM;
	return VM_FAULT_SIGBUS;

page_not_uptodate:
	/*
	 * Umm, take care of errors if the page isn't up-to-date.
	 * Try to re-read it _once_. We do this synchronously,
	 * because there really aren't any performance issues here
	 * and we need to check for errors.
	 */
	ClearPageError(page);
	error = mapping->a_ops->readpage(file, page);
	if (!error) {
		wait_on_page_locked(page);
		if (!PageUptodate(page))
			error = -EIO;
	}
	page_cache_release(page);

	if (!error || error == AOP_TRUNCATED_PAGE)
		goto retry_find;

	/* Things didn't work out. Return zero to tell the mm layer so. */
	shrink_readahead_size_eio(file, ra);
	return VM_FAULT_SIGBUS;
}
EXPORT_SYMBOL(filemap_fault);

const struct vm_operations_struct generic_file_vm_ops = {
	.fault		= filemap_fault,
};

/* This is used for a general mmap of a disk file */

int generic_file_mmap(struct file * file, struct vm_area_struct * vma)
{
	struct address_space *mapping = file->f_mapping;

	if (!mapping->a_ops->readpage)
		return -ENOEXEC;
	file_accessed(file);
	vma->vm_ops = &generic_file_vm_ops;
	vma->vm_flags |= VM_CAN_NONLINEAR;
	return 0;
}

/*
 * This is for filesystems which do not implement ->writepage.
 */
int generic_file_readonly_mmap(struct file *file, struct vm_area_struct *vma)
{
	if ((vma->vm_flags & VM_SHARED) && (vma->vm_flags & VM_MAYWRITE))
		return -EINVAL;
	return generic_file_mmap(file, vma);
}
#else
int generic_file_mmap(struct file * file, struct vm_area_struct * vma)
{
	return -ENOSYS;
}
int generic_file_readonly_mmap(struct file * file, struct vm_area_struct * vma)
{
	return -ENOSYS;
}
#endif /* CONFIG_MMU */

EXPORT_SYMBOL(generic_file_mmap);
EXPORT_SYMBOL(generic_file_readonly_mmap);

static struct page *__read_cache_page(struct address_space *mapping,
				pgoff_t index,
				int (*filler)(void *,struct page*),
				void *data,
				gfp_t gfp)
{
	struct page *page;
	int err;
repeat:
	page = find_get_page(mapping, index);
	if (!page) {
		page = __page_cache_alloc(gfp | __GFP_COLD);
		if (!page)
			return ERR_PTR(-ENOMEM);
		err = add_to_page_cache_lru(page, mapping, index, GFP_KERNEL);
		if (unlikely(err)) {
			page_cache_release(page);
			if (err == -EEXIST)
				goto repeat;
			/* Presumably ENOMEM for radix tree node */
			return ERR_PTR(err);
		}
		err = filler(data, page);
		if (err < 0) {
			page_cache_release(page);
			page = ERR_PTR(err);
		}
	}
	return page;
}

static struct page *do_read_cache_page(struct address_space *mapping,
				pgoff_t index,
				int (*filler)(void *,struct page*),
				void *data,
				gfp_t gfp)

{
	struct page *page;
	int err;

retry:
	page = __read_cache_page(mapping, index, filler, data, gfp);
	if (IS_ERR(page))
		return page;
	if (PageUptodate(page))
		goto out;

	lock_page(page);
	if (!page->mapping) {
		unlock_page(page);
		page_cache_release(page);
		goto retry;
	}
	if (PageUptodate(page)) {
		unlock_page(page);
		goto out;
	}
	err = filler(data, page);
	if (err < 0) {
		page_cache_release(page);
		return ERR_PTR(err);
	}
out:
	mark_page_accessed(page);
	return page;
}

/**
 * read_cache_page_async - read into page cache, fill it if needed
 * @mapping:	the page's address_space
 * @index:	the page index
 * @filler:	function to perform the read
 * @data:	destination for read data
 *
 * Same as read_cache_page, but don't wait for page to become unlocked
 * after submitting it to the filler.
 *
 * Read into the page cache. If a page already exists, and PageUptodate() is
 * not set, try to fill the page but don't wait for it to become unlocked.
 *
 * If the page does not get brought uptodate, return -EIO.
 */
struct page *read_cache_page_async(struct address_space *mapping,
				pgoff_t index,
				int (*filler)(void *,struct page*),
				void *data)
{
	return do_read_cache_page(mapping, index, filler, data, mapping_gfp_mask(mapping));
}
EXPORT_SYMBOL(read_cache_page_async);

static struct page *wait_on_page_read(struct page *page)
{
	if (!IS_ERR(page)) {
		wait_on_page_locked(page);
		if (!PageUptodate(page)) {
			page_cache_release(page);
			page = ERR_PTR(-EIO);
		}
	}
	return page;
}

/**
 * read_cache_page_gfp - read into page cache, using specified page allocation flags.
 * @mapping:	the page's address_space
 * @index:	the page index
 * @gfp:	the page allocator flags to use if allocating
 *
 * This is the same as "read_mapping_page(mapping, index, NULL)", but with
 * any new page allocations done using the specified allocation flags. Note
 * that the Radix tree operations will still use GFP_KERNEL, so you can't
 * expect to do this atomically or anything like that - but you can pass in
 * other page requirements.
 *
 * If the page does not get brought uptodate, return -EIO.
 */
struct page *read_cache_page_gfp(struct address_space *mapping,
				pgoff_t index,
				gfp_t gfp)
{
	filler_t *filler = (filler_t *)mapping->a_ops->readpage;

	return wait_on_page_read(do_read_cache_page(mapping, index, filler, NULL, gfp));
}
EXPORT_SYMBOL(read_cache_page_gfp);

/**
 * read_cache_page - read into page cache, fill it if needed
 * @mapping:	the page's address_space
 * @index:	the page index
 * @filler:	function to perform the read
 * @data:	destination for read data
 *
 * Read into the page cache. If a page already exists, and PageUptodate() is
 * not set, try to fill the page then wait for it to become unlocked.
 *
 * If the page does not get brought uptodate, return -EIO.
 */
struct page *read_cache_page(struct address_space *mapping,
				pgoff_t index,
				int (*filler)(void *,struct page*),
				void *data)
{
	return wait_on_page_read(read_cache_page_async(mapping, index, filler, data));
}
EXPORT_SYMBOL(read_cache_page);

/*
 * The logic we want is
 *
 *	if suid or (sgid and xgrp)
 *		remove privs
 */
int should_remove_suid(struct dentry *dentry)
{
	mode_t mode = dentry->d_inode->i_mode;
	int kill = 0;

	/* suid always must be killed */
	if (unlikely(mode & S_ISUID))
		kill = ATTR_KILL_SUID;

	/*
	 * sgid without any exec bits is just a mandatory locking mark; leave
	 * it alone.  If some exec bits are set, it's a real sgid; kill it.
	 */
	if (unlikely((mode & S_ISGID) && (mode & S_IXGRP)))
		kill |= ATTR_KILL_SGID;

	if (unlikely(kill && !capable(CAP_FSETID) && S_ISREG(mode)))
		return kill;

	return 0;
}
EXPORT_SYMBOL(should_remove_suid);

static int __remove_suid(struct dentry *dentry, int kill)
{
	struct iattr newattrs;

	newattrs.ia_valid = ATTR_FORCE | kill;
	return notify_change(dentry, &newattrs);
}

int file_remove_suid(struct file *file)
{
	struct dentry *dentry = file->f_path.dentry;
	int killsuid = should_remove_suid(dentry);
	int killpriv = security_inode_need_killpriv(dentry);
	int error = 0;

	if (killpriv < 0)
		return killpriv;
	if (killpriv)
		error = security_inode_killpriv(dentry);
	if (!error && killsuid)
		error = __remove_suid(dentry, killsuid);

	return error;
}
EXPORT_SYMBOL(file_remove_suid);

static size_t __iovec_copy_from_user_inatomic(char *vaddr,
			const struct iovec *iov, size_t base, size_t bytes)
{
	size_t copied = 0, left = 0;

	while (bytes) {
		char __user *buf = iov->iov_base + base;
		int copy = min(bytes, iov->iov_len - base);

		base = 0;
		left = __copy_from_user_inatomic(vaddr, buf, copy);
		copied += copy;
		bytes -= copy;
		vaddr += copy;
		iov++;

		if (unlikely(left))
			break;
	}
	return copied - left;
}

/*
 * Copy as much as we can into the page and return the number of bytes which
 * were successfully copied.  If a fault is encountered then return the number of
 * bytes which were copied.
 */
size_t iov_iter_copy_from_user_atomic(struct page *page,
		struct iov_iter *i, unsigned long offset, size_t bytes)
{
	char *kaddr;
	size_t copied;

	BUG_ON(!in_atomic());
	kaddr = kmap_atomic(page, KM_USER0);
	if (likely(i->nr_segs == 1)) {
		int left;
		char __user *buf = i->iov->iov_base + i->iov_offset;
		left = __copy_from_user_inatomic(kaddr + offset, buf, bytes);
		copied = bytes - left;
	} else {
		copied = __iovec_copy_from_user_inatomic(kaddr + offset,
						i->iov, i->iov_offset, bytes);
	}
	kunmap_atomic(kaddr, KM_USER0);

	return copied;
}
EXPORT_SYMBOL(iov_iter_copy_from_user_atomic);

/*
 * This has the same sideeffects and return value as
 * iov_iter_copy_from_user_atomic().
 * The difference is that it attempts to resolve faults.
 * Page must not be locked.
 */
size_t iov_iter_copy_from_user(struct page *page,
		struct iov_iter *i, unsigned long offset, size_t bytes)
{
	char *kaddr;
	size_t copied;

	kaddr = kmap(page);
	if (likely(i->nr_segs == 1)) {
		int left;
		char __user *buf = i->iov->iov_base + i->iov_offset;
		left = __copy_from_user(kaddr + offset, buf, bytes);
		copied = bytes - left;
	} else {
		copied = __iovec_copy_from_user_inatomic(kaddr + offset,
						i->iov, i->iov_offset, bytes);
	}
	kunmap(page);
	return copied;
}
EXPORT_SYMBOL(iov_iter_copy_from_user);

void iov_iter_advance(struct iov_iter *i, size_t bytes)
{
	BUG_ON(i->count < bytes);

	if (likely(i->nr_segs == 1)) {
		i->iov_offset += bytes;
		i->count -= bytes;
	} else {
		const struct iovec *iov = i->iov;
		size_t base = i->iov_offset;

		/*
		 * The !iov->iov_len check ensures we skip over unlikely
		 * zero-length segments (without overruning the iovec).
		 */
		while (bytes || unlikely(i->count && !iov->iov_len)) {
			int copy;

			copy = min(bytes, iov->iov_len - base);
			BUG_ON(!i->count || i->count < copy);
			i->count -= copy;
			bytes -= copy;
			base += copy;
			if (iov->iov_len == base) {
				iov++;
				base = 0;
			}
		}
		i->iov = iov;
		i->iov_offset = base;
	}
}
EXPORT_SYMBOL(iov_iter_advance);

/*
 * Fault in the first iovec of the given iov_iter, to a maximum length
 * of bytes. Returns 0 on success, or non-zero if the memory could not be
 * accessed (ie. because it is an invalid address).
 *
 * writev-intensive code may want this to prefault several iovecs -- that
 * would be possible (callers must not rely on the fact that _only_ the
 * first iovec will be faulted with the current implementation).
 */
int iov_iter_fault_in_readable(struct iov_iter *i, size_t bytes)
{
	char __user *buf = i->iov->iov_base + i->iov_offset;
	bytes = min(bytes, i->iov->iov_len - i->iov_offset);
	return fault_in_pages_readable(buf, bytes);
}
EXPORT_SYMBOL(iov_iter_fault_in_readable);

/*
 * Return the count of just the current iov_iter segment.
 */
size_t iov_iter_single_seg_count(struct iov_iter *i)
{
	const struct iovec *iov = i->iov;
	if (i->nr_segs == 1)
		return i->count;
	else
		return min(i->count, iov->iov_len - i->iov_offset);
}
EXPORT_SYMBOL(iov_iter_single_seg_count);

/*
 * Performs necessary checks before doing a write
 *
 * Can adjust writing position or amount of bytes to write.
 * Returns appropriate error code that caller should return or
 * zero in case that write should be allowed.
 */
inline int generic_write_checks(struct file *file, loff_t *pos, size_t *count, int isblk)
{
	struct inode *inode = file->f_mapping->host;
	unsigned long limit = rlimit(RLIMIT_FSIZE);

        if (unlikely(*pos < 0))
                return -EINVAL;

	if (!isblk) {
		if (file->f_flags & O_APPEND)
                        *pos = i_size_read(inode);

		if (limit != RLIM_INFINITY) {
			if (*pos >= limit) {
				send_sig(SIGXFSZ, current, 0);
				return -EFBIG;
			}
			if (*count > limit - (typeof(limit))*pos) {
				*count = limit - (typeof(limit))*pos;
			}
		}
	}

	/*
	 * LFS rule
	 */
	if (unlikely(*pos + *count > MAX_NON_LFS &&
				!(file->f_flags & O_LARGEFILE))) {
		if (*pos >= MAX_NON_LFS) {
			return -EFBIG;
		}
		if (*count > MAX_NON_LFS - (unsigned long)*pos) {
			*count = MAX_NON_LFS - (unsigned long)*pos;
		}
	}

	/*
	 * Are we about to exceed the fs block limit ?
	 *
	 * If we have written data it becomes a short write.  If we have
	 * exceeded without writing data we send a signal and return EFBIG.
	 * Linus frestrict idea will clean these up nicely..
	 */
	if (likely(!isblk)) {
		if (unlikely(*pos >= inode->i_sb->s_maxbytes)) {
			if (*count || *pos > inode->i_sb->s_maxbytes) {
				return -EFBIG;
			}
			/* zero-length writes at ->s_maxbytes are OK */
		}

		if (unlikely(*pos + *count > inode->i_sb->s_maxbytes))
			*count = inode->i_sb->s_maxbytes - *pos;
	} else {
#ifdef CONFIG_BLOCK
		loff_t isize;
		if (bdev_read_only(I_BDEV(inode)))
			return -EPERM;
		isize = i_size_read(inode);
		if (*pos >= isize) {
			if (*count || *pos > isize)
				return -ENOSPC;
		}

		if (*pos + *count > isize)
			*count = isize - *pos;
#else
		return -EPERM;
#endif
	}
	return 0;
}
EXPORT_SYMBOL(generic_write_checks);

int pagecache_write_begin(struct file *file, struct address_space *mapping,
				loff_t pos, unsigned len, unsigned flags,
				struct page **pagep, void **fsdata)
{
	const struct address_space_operations *aops = mapping->a_ops;

	return aops->write_begin(file, mapping, pos, len, flags,
							pagep, fsdata);
}
EXPORT_SYMBOL(pagecache_write_begin);

int pagecache_write_end(struct file *file, struct address_space *mapping,
				loff_t pos, unsigned len, unsigned copied,
				struct page *page, void *fsdata)
{
	const struct address_space_operations *aops = mapping->a_ops;

	mark_page_accessed(page);
	return aops->write_end(file, mapping, pos, len, copied, page, fsdata);
}
EXPORT_SYMBOL(pagecache_write_end);

ssize_t
generic_file_direct_write(struct kiocb *iocb, const struct iovec *iov,
		unsigned long *nr_segs, loff_t pos, loff_t *ppos,
		size_t count, size_t ocount)
{
	struct file	*file = iocb->ki_filp;
	struct address_space *mapping = file->f_mapping;
	struct inode	*inode = mapping->host;
	ssize_t		written;
	size_t		write_len;
	pgoff_t		end;

	if (count != ocount)
		*nr_segs = iov_shorten((struct iovec *)iov, *nr_segs, count);

	write_len = iov_length(iov, *nr_segs);
	end = (pos + write_len - 1) >> PAGE_CACHE_SHIFT;

	written = filemap_write_and_wait_range(mapping, pos, pos + write_len - 1);
	if (written)
		goto out;

	/*
	 * After a write we want buffered reads to be sure to go to disk to get
	 * the new data.  We invalidate clean cached page from the region we're
	 * about to write.  We do this *before* the write so that we can return
	 * without clobbering -EIOCBQUEUED from ->direct_IO().
	 */
	if (mapping->nrpages) {
		written = invalidate_inode_pages2_range(mapping,
					pos >> PAGE_CACHE_SHIFT, end);
		/*
		 * If a page can not be invalidated, return 0 to fall back
		 * to buffered write.
		 */
		if (written) {
			if (written == -EBUSY)
				return 0;
			goto out;
		}
	}

	written = mapping->a_ops->direct_IO(WRITE, iocb, iov, pos, *nr_segs);

	/*
	 * Finally, try again to invalidate clean pages which might have been
	 * cached by non-direct readahead, or faulted in by get_user_pages()
	 * if the source of the write was an mmap'ed region of the file
	 * we're writing.  Either one is a pretty crazy thing to do,
	 * so we don't support it 100%.  If this invalidation
	 * fails, tough, the write still worked...
	 */
	if (mapping->nrpages) {
		invalidate_inode_pages2_range(mapping,
					      pos >> PAGE_CACHE_SHIFT, end);
	}

	if (written > 0) {
		loff_t end = pos + written;
		if (end > i_size_read(inode) && !S_ISBLK(inode->i_mode)) {
			i_size_write(inode,  end);
			mark_inode_dirty(inode);
		}
		*ppos = end;
	}
out:
	return written;
}
EXPORT_SYMBOL(generic_file_direct_write);

/*
 * Find or create a page at the given pagecache position. Return the locked
 * page. This function is specifically for buffered writes.
 */
struct page *grab_cache_page_write_begin(struct address_space *mapping,
					pgoff_t index, unsigned flags)
{
	int status;
	struct page *page;
	gfp_t gfp_notmask = 0;
	if (flags & AOP_FLAG_NOFS)
		gfp_notmask = __GFP_FS;
repeat:
	page = find_lock_page(mapping, index);
	if (likely(page))
		return page;

	page = __page_cache_alloc(mapping_gfp_mask(mapping) & ~gfp_notmask);
	if (!page)
		return NULL;
	status = add_to_page_cache_lru(page, mapping, index,
						GFP_KERNEL & ~gfp_notmask);
	if (unlikely(status)) {
		page_cache_release(page);
		if (status == -EEXIST)
			goto repeat;
		return NULL;
	}
	return page;
}
EXPORT_SYMBOL(grab_cache_page_write_begin);

static ssize_t generic_perform_write(struct file *file,
				struct iov_iter *i, loff_t pos)
{
	struct address_space *mapping = file->f_mapping;
	const struct address_space_operations *a_ops = mapping->a_ops;
	long status = 0;
	ssize_t written = 0;
	unsigned int flags = 0;

	/*
	 * Copies from kernel address space cannot fail (NFSD is a big user).
	 */
	if (segment_eq(get_fs(), KERNEL_DS))
		flags |= AOP_FLAG_UNINTERRUPTIBLE;

	do {
		struct page *page;
		unsigned long offset;	/* Offset into pagecache page */
		unsigned long bytes;	/* Bytes to write to page */
		size_t copied;		/* Bytes copied from user */
		void *fsdata;

		offset = (pos & (PAGE_CACHE_SIZE - 1));
		bytes = min_t(unsigned long, PAGE_CACHE_SIZE - offset,
						iov_iter_count(i));

again:

		/*
		 * Bring in the user page that we will copy from _first_.
		 * Otherwise there's a nasty deadlock on copying from the
		 * same page as we're writing to, without it being marked
		 * up-to-date.
		 *
		 * Not only is this an optimisation, but it is also required
		 * to check that the address is actually valid, when atomic
		 * usercopies are used, below.
		 */
		if (unlikely(iov_iter_fault_in_readable(i, bytes))) {
			status = -EFAULT;
			break;
		}

		status = a_ops->write_begin(file, mapping, pos, bytes, flags,
						&page, &fsdata);
		if (unlikely(status))
			break;

		if (mapping_writably_mapped(mapping))
			flush_dcache_page(page);

		pagefault_disable();
		copied = iov_iter_copy_from_user_atomic(page, i, offset, bytes);
		pagefault_enable();
		flush_dcache_page(page);

		mark_page_accessed(page);
		status = a_ops->write_end(file, mapping, pos, bytes, copied,
						page, fsdata);
		if (unlikely(status < 0))
			break;
		copied = status;

		cond_resched();

		iov_iter_advance(i, copied);
		if (unlikely(copied == 0)) {
			/*
			 * If we were unable to copy any data at all, we must
			 * fall back to a single segment length write.
			 *
			 * If we didn't fallback here, we could livelock
			 * because not all segments in the iov can be copied at
			 * once without a pagefault.
			 */
			bytes = min_t(unsigned long, PAGE_CACHE_SIZE - offset,
						iov_iter_single_seg_count(i));
			goto again;
		}
		pos += copied;
		written += copied;

		balance_dirty_pages_ratelimited(mapping);

	} while (iov_iter_count(i));

	return written ? written : status;
}

ssize_t
generic_file_buffered_write(struct kiocb *iocb, const struct iovec *iov,
		unsigned long nr_segs, loff_t pos, loff_t *ppos,
		size_t count, ssize_t written)
{
	struct file *file = iocb->ki_filp;
	ssize_t status;
	struct iov_iter i;

	iov_iter_init(&i, iov, nr_segs, count, written);
	status = generic_perform_write(file, &i, pos);

	if (likely(status >= 0)) {
		written += status;
		*ppos = pos + status;
  	}
	
	return written ? written : status;
}
EXPORT_SYMBOL(generic_file_buffered_write);

/**
 * __generic_file_aio_write - write data to a file
 * @iocb:	IO state structure (file, offset, etc.)
 * @iov:	vector with data to write
 * @nr_segs:	number of segments in the vector
 * @ppos:	position where to write
 *
 * This function does all the work needed for actually writing data to a
 * file. It does all basic checks, removes SUID from the file, updates
 * modification times and calls proper subroutines depending on whether we
 * do direct IO or a standard buffered write.
 *
 * It expects i_mutex to be grabbed unless we work on a block device or similar
 * object which does not need locking at all.
 *
 * This function does *not* take care of syncing data in case of O_SYNC write.
 * A caller has to handle it. This is mainly due to the fact that we want to
 * avoid syncing under i_mutex.
 */
ssize_t __generic_file_aio_write(struct kiocb *iocb, const struct iovec *iov,
				 unsigned long nr_segs, loff_t *ppos)
{
	struct file *file = iocb->ki_filp;
	struct address_space * mapping = file->f_mapping;
	size_t ocount;		/* original count */
	size_t count;		/* after file limit checks */
	struct inode 	*inode = mapping->host;
	loff_t		pos;
	ssize_t		written;
	ssize_t		err;

	ocount = 0;
	err = generic_segment_checks(iov, &nr_segs, &ocount, VERIFY_READ);
	if (err)
		return err;

	count = ocount;
	pos = *ppos;

	vfs_check_frozen(inode->i_sb, SB_FREEZE_WRITE);

	/* We can write back this queue in page reclaim */
	current->backing_dev_info = mapping->backing_dev_info;
	written = 0;

	err = generic_write_checks(file, &pos, &count, S_ISBLK(inode->i_mode));
	if (err)
		goto out;

	if (count == 0)
		goto out;

	err = file_remove_suid(file);
	if (err)
		goto out;

	file_update_time(file);

	/* coalesce the iovecs and go direct-to-BIO for O_DIRECT */
	if (unlikely(file->f_flags & O_DIRECT)) {
		loff_t endbyte;
		ssize_t written_buffered;

		written = generic_file_direct_write(iocb, iov, &nr_segs, pos,
							ppos, count, ocount);
		if (written < 0 || written == count)
			goto out;
		/*
		 * direct-io write to a hole: fall through to buffered I/O
		 * for completing the rest of the request.
		 */
		pos += written;
		count -= written;
		written_buffered = generic_file_buffered_write(iocb, iov,
						nr_segs, pos, ppos, count,
						written);
		/*
		 * If generic_file_buffered_write() retuned a synchronous error
		 * then we want to return the number of bytes which were
		 * direct-written, or the error code if that was zero.  Note
		 * that this differs from normal direct-io semantics, which
		 * will return -EFOO even if some bytes were written.
		 */
		if (written_buffered < 0) {
			err = written_buffered;
			goto out;
		}

		/*
		 * We need to ensure that the page cache pages are written to
		 * disk and invalidated to preserve the expected O_DIRECT
		 * semantics.
		 */
		endbyte = pos + written_buffered - written - 1;
		err = filemap_write_and_wait_range(file->f_mapping, pos, endbyte);
		if (err == 0) {
			written = written_buffered;
			invalidate_mapping_pages(mapping,
						 pos >> PAGE_CACHE_SHIFT,
						 endbyte >> PAGE_CACHE_SHIFT);
		} else {
			/*
			 * We don't know how much we wrote, so just return
			 * the number of bytes which were direct-written
			 */
		}
	} else {
		written = generic_file_buffered_write(iocb, iov, nr_segs,
				pos, ppos, count, written);
	}
out:
	current->backing_dev_info = NULL;
	return written ? written : err;
}
EXPORT_SYMBOL(__generic_file_aio_write);

/**
 * generic_file_aio_write - write data to a file
 * @iocb:	IO state structure
 * @iov:	vector with data to write
 * @nr_segs:	number of segments in the vector
 * @pos:	position in file where to write
 *
 * This is a wrapper around __generic_file_aio_write() to be used by most
 * filesystems. It takes care of syncing the file in case of O_SYNC file
 * and acquires i_mutex as needed.
 */
ssize_t generic_file_aio_write(struct kiocb *iocb, const struct iovec *iov,
		unsigned long nr_segs, loff_t pos)
{
	struct file *file = iocb->ki_filp;
	struct inode *inode = file->f_mapping->host;
	ssize_t ret;

	BUG_ON(iocb->ki_pos != pos);

	mutex_lock(&inode->i_mutex);
	ret = __generic_file_aio_write(iocb, iov, nr_segs, &iocb->ki_pos);
	mutex_unlock(&inode->i_mutex);

	if (ret > 0 || ret == -EIOCBQUEUED) {
		ssize_t err;

		err = generic_write_sync(file, pos, ret);
		if (err < 0 && ret > 0)
			ret = err;
	}
	return ret;
}
EXPORT_SYMBOL(generic_file_aio_write);

/**
 * try_to_release_page() - release old fs-specific metadata on a page
 *
 * @page: the page which the kernel is trying to free
 * @gfp_mask: memory allocation flags (and I/O mode)
 *
 * The address_space is to try to release any data against the page
 * (presumably at page->private).  If the release was successful, return `1'.
 * Otherwise return zero.
 *
 * This may also be called if PG_fscache is set on a page, indicating that the
 * page is known to the local caching routines.
 *
 * The @gfp_mask argument specifies whether I/O may be performed to release
 * this page (__GFP_IO), and whether the call may block (__GFP_WAIT & __GFP_FS).
 *
 */
int try_to_release_page(struct page *page, gfp_t gfp_mask)
{
	struct address_space * const mapping = page->mapping;

	BUG_ON(!PageLocked(page));
	if (PageWriteback(page))
		return 0;

	if (mapping && mapping->a_ops->releasepage)
		return mapping->a_ops->releasepage(page, gfp_mask);
	return try_to_free_buffers(page);
}

EXPORT_SYMBOL(try_to_release_page);
