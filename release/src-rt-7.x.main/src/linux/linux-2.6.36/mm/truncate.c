/*
 * mm/truncate.c - code for taking down pages from address_spaces
 *
 * Copyright (C) 2002, Linus Torvalds
 *
 * 10Sep2002	Andrew Morton
 *		Initial version.
 */

#include <linux/kernel.h>
#include <linux/backing-dev.h>
#include <linux/gfp.h>
#include <linux/mm.h>
#include <linux/swap.h>
#include <linux/module.h>
#include <linux/pagemap.h>
#include <linux/highmem.h>
#include <linux/pagevec.h>
#include <linux/task_io_accounting_ops.h>
#include <linux/buffer_head.h>	/* grr. try_to_release_page,
				   do_invalidatepage */
#include "internal.h"


/**
 * do_invalidatepage - invalidate part or all of a page
 * @page: the page which is affected
 * @offset: the index of the truncation point
 *
 * do_invalidatepage() is called when all or part of the page has become
 * invalidated by a truncate operation.
 *
 * do_invalidatepage() does not have to release all buffers, but it must
 * ensure that no dirty buffer is left outside @offset and that no I/O
 * is underway against any of the blocks which are outside the truncation
 * point.  Because the caller is about to free (and possibly reuse) those
 * blocks on-disk.
 */
void do_invalidatepage(struct page *page, unsigned long offset)
{
	void (*invalidatepage)(struct page *, unsigned long);
	invalidatepage = page->mapping->a_ops->invalidatepage;
#ifdef CONFIG_BLOCK
	if (!invalidatepage)
		invalidatepage = block_invalidatepage;
#endif
	if (invalidatepage)
		(*invalidatepage)(page, offset);
}

static inline void truncate_partial_page(struct page *page, unsigned partial)
{
	zero_user_segment(page, partial, PAGE_CACHE_SIZE);
	if (page_has_private(page))
		do_invalidatepage(page, partial);
}

/*
 * This cancels just the dirty bit on the kernel page itself, it
 * does NOT actually remove dirty bits on any mmap's that may be
 * around. It also leaves the page tagged dirty, so any sync
 * activity will still find it on the dirty lists, and in particular,
 * clear_page_dirty_for_io() will still look at the dirty bits in
 * the VM.
 *
 * Doing this should *normally* only ever be done when a page
 * is truncated, and is not actually mapped anywhere at all. However,
 * fs/buffer.c does this when it notices that somebody has cleaned
 * out all the buffers on a page without actually doing it through
 * the VM. Can you say "ext3 is horribly ugly"? Tought you could.
 */
void cancel_dirty_page(struct page *page, unsigned int account_size)
{
	if (TestClearPageDirty(page)) {
		struct address_space *mapping = page->mapping;
		if (mapping && mapping_cap_account_dirty(mapping)) {
			dec_zone_page_state(page, NR_FILE_DIRTY);
			dec_bdi_stat(mapping->backing_dev_info,
					BDI_RECLAIMABLE);
			if (account_size)
				task_io_account_cancelled_write(account_size);
		}
	}
}
EXPORT_SYMBOL(cancel_dirty_page);

/*
 * If truncate cannot remove the fs-private metadata from the page, the page
 * becomes orphaned.  It will be left on the LRU and may even be mapped into
 * user pagetables if we're racing with filemap_fault().
 *
 * We need to bale out if page->mapping is no longer equal to the original
 * mapping.  This happens a) when the VM reclaimed the page while we waited on
 * its lock, b) when a concurrent invalidate_mapping_pages got there first and
 * c) when tmpfs swizzles a page between a tmpfs inode and swapper_space.
 */
static int
truncate_complete_page(struct address_space *mapping, struct page *page)
{
	if (page->mapping != mapping)
		return -EIO;

	if (page_has_private(page))
		do_invalidatepage(page, 0);

	cancel_dirty_page(page, PAGE_CACHE_SIZE);

	clear_page_mlock(page);
	remove_from_page_cache(page);
	ClearPageMappedToDisk(page);
	page_cache_release(page);	/* pagecache ref */
	return 0;
}

/*
 * This is for invalidate_mapping_pages().  That function can be called at
 * any time, and is not supposed to throw away dirty pages.  But pages can
 * be marked dirty at any time too, so use remove_mapping which safely
 * discards clean, unused pages.
 *
 * Returns non-zero if the page was successfully invalidated.
 */
static int
invalidate_complete_page(struct address_space *mapping, struct page *page)
{
	int ret;

	if (page->mapping != mapping)
		return 0;

	if (page_has_private(page) && !try_to_release_page(page, 0))
		return 0;

	clear_page_mlock(page);
	ret = remove_mapping(mapping, page);

	return ret;
}

int truncate_inode_page(struct address_space *mapping, struct page *page)
{
	if (page_mapped(page)) {
		unmap_mapping_range(mapping,
				   (loff_t)page->index << PAGE_CACHE_SHIFT,
				   PAGE_CACHE_SIZE, 0);
	}
	return truncate_complete_page(mapping, page);
}

/*
 * Used to get rid of pages on hardware memory corruption.
 */
int generic_error_remove_page(struct address_space *mapping, struct page *page)
{
	if (!mapping)
		return -EINVAL;
	/*
	 * Only punch for normal data pages for now.
	 * Handling other types like directories would need more auditing.
	 */
	if (!S_ISREG(mapping->host->i_mode))
		return -EIO;
	return truncate_inode_page(mapping, page);
}
EXPORT_SYMBOL(generic_error_remove_page);

/*
 * Safely invalidate one page from its pagecache mapping.
 * It only drops clean, unused pages. The page must be locked.
 *
 * Returns 1 if the page is successfully invalidated, otherwise 0.
 */
int invalidate_inode_page(struct page *page)
{
	struct address_space *mapping = page_mapping(page);
	if (!mapping)
		return 0;
	if (PageDirty(page) || PageWriteback(page))
		return 0;
	if (page_mapped(page))
		return 0;
	return invalidate_complete_page(mapping, page);
}

/**
 * truncate_inode_pages - truncate range of pages specified by start & end byte offsets
 * @mapping: mapping to truncate
 * @lstart: offset from which to truncate
 * @lend: offset to which to truncate
 *
 * Truncate the page cache, removing the pages that are between
 * specified offsets (and zeroing out partial page
 * (if lstart is not page aligned)).
 *
 * Truncate takes two passes - the first pass is nonblocking.  It will not
 * block on page locks and it will not block on writeback.  The second pass
 * will wait.  This is to prevent as much IO as possible in the affected region.
 * The first pass will remove most pages, so the search cost of the second pass
 * is low.
 *
 * When looking at page->index outside the page lock we need to be careful to
 * copy it into a local to avoid races (it could change at any time).
 *
 * We pass down the cache-hot hint to the page freeing code.  Even if the
 * mapping is large, it is probably the case that the final pages are the most
 * recently touched, and freeing happens in ascending file offset order.
 */
void truncate_inode_pages_range(struct address_space *mapping,
				loff_t lstart, loff_t lend)
{
	const pgoff_t start = (lstart + PAGE_CACHE_SIZE-1) >> PAGE_CACHE_SHIFT;
	pgoff_t end;
	const unsigned partial = lstart & (PAGE_CACHE_SIZE - 1);
	struct pagevec pvec;
	pgoff_t next;
	int i;

	if (mapping->nrpages == 0)
		return;

	BUG_ON((lend & (PAGE_CACHE_SIZE - 1)) != (PAGE_CACHE_SIZE - 1));
	end = (lend >> PAGE_CACHE_SHIFT);

	pagevec_init(&pvec, 0);
	next = start;
	while (next <= end &&
	       pagevec_lookup(&pvec, mapping, next, PAGEVEC_SIZE)) {
		for (i = 0; i < pagevec_count(&pvec); i++) {
			struct page *page = pvec.pages[i];
			pgoff_t page_index = page->index;

			if (page_index > end) {
				next = page_index;
				break;
			}

			if (page_index > next)
				next = page_index;
			next++;
			if (!trylock_page(page))
				continue;
			if (PageWriteback(page)) {
				unlock_page(page);
				continue;
			}
			truncate_inode_page(mapping, page);
			unlock_page(page);
		}
		pagevec_release(&pvec);
		cond_resched();
	}

	if (partial) {
		struct page *page = find_lock_page(mapping, start - 1);
		if (page) {
			wait_on_page_writeback(page);
			truncate_partial_page(page, partial);
			unlock_page(page);
			page_cache_release(page);
		}
	}

	next = start;
	for ( ; ; ) {
		cond_resched();
		if (!pagevec_lookup(&pvec, mapping, next, PAGEVEC_SIZE)) {
			if (next == start)
				break;
			next = start;
			continue;
		}
		if (pvec.pages[0]->index > end) {
			pagevec_release(&pvec);
			break;
		}
		mem_cgroup_uncharge_start();
		for (i = 0; i < pagevec_count(&pvec); i++) {
			struct page *page = pvec.pages[i];

			if (page->index > end)
				break;
			lock_page(page);
			wait_on_page_writeback(page);
			truncate_inode_page(mapping, page);
			if (page->index > next)
				next = page->index;
			next++;
			unlock_page(page);
		}
		pagevec_release(&pvec);
		mem_cgroup_uncharge_end();
	}
}
EXPORT_SYMBOL(truncate_inode_pages_range);

/**
 * truncate_inode_pages - truncate *all* the pages from an offset
 * @mapping: mapping to truncate
 * @lstart: offset from which to truncate
 *
 * Called under (and serialised by) inode->i_mutex.
 */
void truncate_inode_pages(struct address_space *mapping, loff_t lstart)
{
	truncate_inode_pages_range(mapping, lstart, (loff_t)-1);
}
EXPORT_SYMBOL(truncate_inode_pages);

/**
 * invalidate_mapping_pages - Invalidate all the unlocked pages of one inode
 * @mapping: the address_space which holds the pages to invalidate
 * @start: the offset 'from' which to invalidate
 * @end: the offset 'to' which to invalidate (inclusive)
 *
 * This function only removes the unlocked pages, if you want to
 * remove all the pages of one inode, you must call truncate_inode_pages.
 *
 * invalidate_mapping_pages() will not block on IO activity. It will not
 * invalidate pages which are dirty, locked, under writeback or mapped into
 * pagetables.
 */
unsigned long invalidate_mapping_pages(struct address_space *mapping,
				       pgoff_t start, pgoff_t end)
{
	struct pagevec pvec;
	pgoff_t next = start;
	unsigned long ret = 0;
	int i;

	pagevec_init(&pvec, 0);
	while (next <= end &&
			pagevec_lookup(&pvec, mapping, next, PAGEVEC_SIZE)) {
		mem_cgroup_uncharge_start();
		for (i = 0; i < pagevec_count(&pvec); i++) {
			struct page *page = pvec.pages[i];
			pgoff_t index;
			int lock_failed;

			lock_failed = !trylock_page(page);

			/*
			 * We really shouldn't be looking at the ->index of an
			 * unlocked page.  But we're not allowed to lock these
			 * pages.  So we rely upon nobody altering the ->index
			 * of this (pinned-by-us) page.
			 */
			index = page->index;
			if (index > next)
				next = index;
			next++;
			if (lock_failed)
				continue;

			ret += invalidate_inode_page(page);

			unlock_page(page);
			if (next > end)
				break;
		}
		pagevec_release(&pvec);
		mem_cgroup_uncharge_end();
		cond_resched();
	}
	return ret;
}
EXPORT_SYMBOL(invalidate_mapping_pages);

/*
 * This is like invalidate_complete_page(), except it ignores the page's
 * refcount.  We do this because invalidate_inode_pages2() needs stronger
 * invalidation guarantees, and cannot afford to leave pages behind because
 * shrink_page_list() has a temp ref on them, or because they're transiently
 * sitting in the lru_cache_add() pagevecs.
 */
static int
invalidate_complete_page2(struct address_space *mapping, struct page *page)
{
	if (page->mapping != mapping)
		return 0;

	if (page_has_private(page) && !try_to_release_page(page, GFP_KERNEL))
		return 0;

	spin_lock_irq(&mapping->tree_lock);
	if (PageDirty(page))
		goto failed;

	clear_page_mlock(page);
	BUG_ON(page_has_private(page));
	__remove_from_page_cache(page);
	spin_unlock_irq(&mapping->tree_lock);
	mem_cgroup_uncharge_cache_page(page);
	page_cache_release(page);	/* pagecache ref */
	return 1;
failed:
	spin_unlock_irq(&mapping->tree_lock);
	return 0;
}

static int do_launder_page(struct address_space *mapping, struct page *page)
{
	if (!PageDirty(page))
		return 0;
	if (page->mapping != mapping || mapping->a_ops->launder_page == NULL)
		return 0;
	return mapping->a_ops->launder_page(page);
}

/**
 * invalidate_inode_pages2_range - remove range of pages from an address_space
 * @mapping: the address_space
 * @start: the page offset 'from' which to invalidate
 * @end: the page offset 'to' which to invalidate (inclusive)
 *
 * Any pages which are found to be mapped into pagetables are unmapped prior to
 * invalidation.
 *
 * Returns -EBUSY if any pages could not be invalidated.
 */
int invalidate_inode_pages2_range(struct address_space *mapping,
				  pgoff_t start, pgoff_t end)
{
	struct pagevec pvec;
	pgoff_t next;
	int i;
	int ret = 0;
	int ret2 = 0;
	int did_range_unmap = 0;
	int wrapped = 0;

	pagevec_init(&pvec, 0);
	next = start;
	while (next <= end && !wrapped &&
		pagevec_lookup(&pvec, mapping, next,
			min(end - next, (pgoff_t)PAGEVEC_SIZE - 1) + 1)) {
		mem_cgroup_uncharge_start();
		for (i = 0; i < pagevec_count(&pvec); i++) {
			struct page *page = pvec.pages[i];
			pgoff_t page_index;

			lock_page(page);
			if (page->mapping != mapping) {
				unlock_page(page);
				continue;
			}
			page_index = page->index;
			next = page_index + 1;
			if (next == 0)
				wrapped = 1;
			if (page_index > end) {
				unlock_page(page);
				break;
			}
			wait_on_page_writeback(page);
			if (page_mapped(page)) {
				if (!did_range_unmap) {
					/*
					 * Zap the rest of the file in one hit.
					 */
					unmap_mapping_range(mapping,
					   (loff_t)page_index<<PAGE_CACHE_SHIFT,
					   (loff_t)(end - page_index + 1)
							<< PAGE_CACHE_SHIFT,
					    0);
					did_range_unmap = 1;
				} else {
					/*
					 * Just zap this page
					 */
					unmap_mapping_range(mapping,
					  (loff_t)page_index<<PAGE_CACHE_SHIFT,
					  PAGE_CACHE_SIZE, 0);
				}
			}
			BUG_ON(page_mapped(page));
			ret2 = do_launder_page(mapping, page);
			if (ret2 == 0) {
				if (!invalidate_complete_page2(mapping, page))
					ret2 = -EBUSY;
			}
			if (ret2 < 0)
				ret = ret2;
			unlock_page(page);
		}
		pagevec_release(&pvec);
		mem_cgroup_uncharge_end();
		cond_resched();
	}
	return ret;
}
EXPORT_SYMBOL_GPL(invalidate_inode_pages2_range);

/**
 * invalidate_inode_pages2 - remove all pages from an address_space
 * @mapping: the address_space
 *
 * Any pages which are found to be mapped into pagetables are unmapped prior to
 * invalidation.
 *
 * Returns -EBUSY if any pages could not be invalidated.
 */
int invalidate_inode_pages2(struct address_space *mapping)
{
	return invalidate_inode_pages2_range(mapping, 0, -1);
}
EXPORT_SYMBOL_GPL(invalidate_inode_pages2);

/**
 * truncate_pagecache - unmap and remove pagecache that has been truncated
 * @inode: inode
 * @old: old file offset
 * @new: new file offset
 *
 * inode's new i_size must already be written before truncate_pagecache
 * is called.
 *
 * This function should typically be called before the filesystem
 * releases resources associated with the freed range (eg. deallocates
 * blocks). This way, pagecache will always stay logically coherent
 * with on-disk format, and the filesystem would not have to deal with
 * situations such as writepage being called for a page that has already
 * had its underlying blocks deallocated.
 */
void truncate_pagecache(struct inode *inode, loff_t old, loff_t new)
{
	struct address_space *mapping = inode->i_mapping;

	/*
	 * unmap_mapping_range is called twice, first simply for
	 * efficiency so that truncate_inode_pages does fewer
	 * single-page unmaps.  However after this first call, and
	 * before truncate_inode_pages finishes, it is possible for
	 * private pages to be COWed, which remain after
	 * truncate_inode_pages finishes, hence the second
	 * unmap_mapping_range call must be made for correctness.
	 */
	unmap_mapping_range(mapping, new + PAGE_SIZE - 1, 0, 1);
	truncate_inode_pages(mapping, new);
	unmap_mapping_range(mapping, new + PAGE_SIZE - 1, 0, 1);
}
EXPORT_SYMBOL(truncate_pagecache);

/**
 * truncate_setsize - update inode and pagecache for a new file size
 * @inode: inode
 * @newsize: new file size
 *
 * truncate_setsize updastes i_size update and performs pagecache
 * truncation (if necessary) for a file size updates. It will be
 * typically be called from the filesystem's setattr function when
 * ATTR_SIZE is passed in.
 *
 * Must be called with inode_mutex held and after all filesystem
 * specific block truncation has been performed.
 */
void truncate_setsize(struct inode *inode, loff_t newsize)
{
	loff_t oldsize;

	oldsize = inode->i_size;
	i_size_write(inode, newsize);

	truncate_pagecache(inode, oldsize, newsize);
}
EXPORT_SYMBOL(truncate_setsize);

/**
 * vmtruncate - unmap mappings "freed" by truncate() syscall
 * @inode: inode of the file used
 * @offset: file offset to start truncating
 *
 * This function is deprecated and truncate_setsize or truncate_pagecache
 * should be used instead, together with filesystem specific block truncation.
 */
int vmtruncate(struct inode *inode, loff_t offset)
{
	int error;

	error = inode_newsize_ok(inode, offset);
	if (error)
		return error;

	truncate_setsize(inode, offset);
	if (inode->i_op->truncate)
		inode->i_op->truncate(inode);
	return 0;
}
EXPORT_SYMBOL(vmtruncate);
