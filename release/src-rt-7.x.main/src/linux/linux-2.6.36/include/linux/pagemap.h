#ifndef _LINUX_PAGEMAP_H
#define _LINUX_PAGEMAP_H

/*
 * Copyright 1995 Linus Torvalds
 */
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/highmem.h>
#include <linux/compiler.h>
#include <asm/uaccess.h>
#include <linux/gfp.h>
#include <linux/bitops.h>
#include <linux/hardirq.h> /* for in_interrupt() */
#include <linux/hugetlb_inline.h>

/*
 * Bits in mapping->flags.  The lower __GFP_BITS_SHIFT bits are the page
 * allocation mode flags.
 */
enum mapping_flags {
	AS_EIO		= __GFP_BITS_SHIFT + 0,	/* IO error on async write */
	AS_ENOSPC	= __GFP_BITS_SHIFT + 1,	/* ENOSPC on async write */
	AS_MM_ALL_LOCKS	= __GFP_BITS_SHIFT + 2,	/* under mm_take_all_locks() */
	AS_UNEVICTABLE	= __GFP_BITS_SHIFT + 3,	/* e.g., ramdisk, SHM_LOCK */
};

static inline void mapping_set_error(struct address_space *mapping, int error)
{
	if (unlikely(error)) {
		if (error == -ENOSPC)
			set_bit(AS_ENOSPC, &mapping->flags);
		else
			set_bit(AS_EIO, &mapping->flags);
	}
}

static inline void mapping_set_unevictable(struct address_space *mapping)
{
	set_bit(AS_UNEVICTABLE, &mapping->flags);
}

static inline void mapping_clear_unevictable(struct address_space *mapping)
{
	clear_bit(AS_UNEVICTABLE, &mapping->flags);
}

static inline int mapping_unevictable(struct address_space *mapping)
{
	if (likely(mapping))
		return test_bit(AS_UNEVICTABLE, &mapping->flags);
	return !!mapping;
}

static inline gfp_t mapping_gfp_mask(struct address_space * mapping)
{
	return (__force gfp_t)mapping->flags & __GFP_BITS_MASK;
}

/*
 * This is non-atomic.  Only to be used before the mapping is activated.
 * Probably needs a barrier...
 */
static inline void mapping_set_gfp_mask(struct address_space *m, gfp_t mask)
{
	m->flags = (m->flags & ~(__force unsigned long)__GFP_BITS_MASK) |
				(__force unsigned long)mask;
}

/*
 * The page cache can done in larger chunks than
 * one page, because it allows for more efficient
 * throughput (it can then be mapped into user
 * space in smaller chunks for same flexibility).
 *
 * Or rather, it _will_ be done in larger chunks.
 */
#define PAGE_CACHE_SHIFT	PAGE_SHIFT
#define PAGE_CACHE_SIZE		PAGE_SIZE
#define PAGE_CACHE_MASK		PAGE_MASK
#define PAGE_CACHE_ALIGN(addr)	(((addr)+PAGE_CACHE_SIZE-1)&PAGE_CACHE_MASK)

#define page_cache_get(page)		get_page(page)
#define page_cache_release(page)	put_page(page)
void release_pages(struct page **pages, int nr, int cold);

/*
 * speculatively take a reference to a page.
 * If the page is free (_count == 0), then _count is untouched, and 0
 * is returned. Otherwise, _count is incremented by 1 and 1 is returned.
 *
 * This function must be called inside the same rcu_read_lock() section as has
 * been used to lookup the page in the pagecache radix-tree (or page table):
 * this allows allocators to use a synchronize_rcu() to stabilize _count.
 *
 * Unless an RCU grace period has passed, the count of all pages coming out
 * of the allocator must be considered unstable. page_count may return higher
 * than expected, and put_page must be able to do the right thing when the
 * page has been finished with, no matter what it is subsequently allocated
 * for (because put_page is what is used here to drop an invalid speculative
 * reference).
 *
 * This is the interesting part of the lockless pagecache (and lockless
 * get_user_pages) locking protocol, where the lookup-side (eg. find_get_page)
 * has the following pattern:
 * 1. find page in radix tree
 * 2. conditionally increment refcount
 * 3. check the page is still in pagecache (if no, goto 1)
 *
 * Remove-side that cares about stability of _count (eg. reclaim) has the
 * following (with tree_lock held for write):
 * A. atomically check refcount is correct and set it to 0 (atomic_cmpxchg)
 * B. remove page from pagecache
 * C. free the page
 *
 * There are 2 critical interleavings that matter:
 * - 2 runs before A: in this case, A sees elevated refcount and bails out
 * - A runs before 2: in this case, 2 sees zero refcount and retries;
 *   subsequently, B will complete and 1 will find no page, causing the
 *   lookup to return NULL.
 *
 * It is possible that between 1 and 2, the page is removed then the exact same
 * page is inserted into the same position in pagecache. That's OK: the
 * old find_get_page using tree_lock could equally have run before or after
 * such a re-insertion, depending on order that locks are granted.
 *
 * Lookups racing against pagecache insertion isn't a big problem: either 1
 * will find the page or it will not. Likewise, the old find_get_page could run
 * either before the insertion or afterwards, depending on timing.
 */
static inline int page_cache_get_speculative(struct page *page)
{
	VM_BUG_ON(in_interrupt());

#if !defined(CONFIG_SMP) && defined(CONFIG_TREE_RCU)
# ifdef CONFIG_PREEMPT
	VM_BUG_ON(!in_atomic());
# endif
	/*
	 * Preempt must be disabled here - we rely on rcu_read_lock doing
	 * this for us.
	 *
	 * Pagecache won't be truncated from interrupt context, so if we have
	 * found a page in the radix tree here, we have pinned its refcount by
	 * disabling preempt, and hence no need for the "speculative get" that
	 * SMP requires.
	 */
	VM_BUG_ON(page_count(page) == 0);
	atomic_inc(&page->_count);

#else
	if (unlikely(!get_page_unless_zero(page))) {
		/*
		 * Either the page has been freed, or will be freed.
		 * In either case, retry here and the caller should
		 * do the right thing (see comments above).
		 */
		return 0;
	}
#endif
	VM_BUG_ON(PageTail(page));

	return 1;
}

/*
 * Same as above, but add instead of inc (could just be merged)
 */
static inline int page_cache_add_speculative(struct page *page, int count)
{
	VM_BUG_ON(in_interrupt());

#if !defined(CONFIG_SMP) && defined(CONFIG_TREE_RCU)
# ifdef CONFIG_PREEMPT
	VM_BUG_ON(!in_atomic());
# endif
	VM_BUG_ON(page_count(page) == 0);
	atomic_add(count, &page->_count);

#else
	if (unlikely(!atomic_add_unless(&page->_count, count, 0)))
		return 0;
#endif
	VM_BUG_ON(PageCompound(page) && page != compound_head(page));

	return 1;
}

static inline int page_freeze_refs(struct page *page, int count)
{
	return likely(atomic_cmpxchg(&page->_count, count, 0) == count);
}

static inline void page_unfreeze_refs(struct page *page, int count)
{
	VM_BUG_ON(page_count(page) != 0);
	VM_BUG_ON(count == 0);

	atomic_set(&page->_count, count);
}

#ifdef CONFIG_NUMA
extern struct page *__page_cache_alloc(gfp_t gfp);
#else
static inline struct page *__page_cache_alloc(gfp_t gfp)
{
	return alloc_pages(gfp, 0);
}
#endif

static inline struct page *page_cache_alloc(struct address_space *x)
{
	return __page_cache_alloc(mapping_gfp_mask(x));
}

static inline struct page *page_cache_alloc_cold(struct address_space *x)
{
	return __page_cache_alloc(mapping_gfp_mask(x)|__GFP_COLD);
}

typedef int filler_t(void *, struct page *);

extern struct page * find_get_page(struct address_space *mapping,
				pgoff_t index);
extern struct page * find_lock_page(struct address_space *mapping,
				pgoff_t index);
extern struct page * find_or_create_page(struct address_space *mapping,
				pgoff_t index, gfp_t gfp_mask);
unsigned find_get_pages(struct address_space *mapping, pgoff_t start,
			unsigned int nr_pages, struct page **pages);
unsigned find_get_pages_contig(struct address_space *mapping, pgoff_t start,
			       unsigned int nr_pages, struct page **pages);
unsigned find_get_pages_tag(struct address_space *mapping, pgoff_t *index,
			int tag, unsigned int nr_pages, struct page **pages);

struct page *grab_cache_page_write_begin(struct address_space *mapping,
			pgoff_t index, unsigned flags);

/*
 * Returns locked page at given index in given cache, creating it if needed.
 */
static inline struct page *grab_cache_page(struct address_space *mapping,
								pgoff_t index)
{
	return find_or_create_page(mapping, index, mapping_gfp_mask(mapping));
}

extern struct page * grab_cache_page_nowait(struct address_space *mapping,
				pgoff_t index);
extern struct page * read_cache_page_async(struct address_space *mapping,
				pgoff_t index, filler_t *filler,
				void *data);
extern struct page * read_cache_page(struct address_space *mapping,
				pgoff_t index, filler_t *filler,
				void *data);
extern struct page * read_cache_page_gfp(struct address_space *mapping,
				pgoff_t index, gfp_t gfp_mask);
extern int read_cache_pages(struct address_space *mapping,
		struct list_head *pages, filler_t *filler, void *data);

static inline struct page *read_mapping_page_async(
						struct address_space *mapping,
						     pgoff_t index, void *data)
{
	filler_t *filler = (filler_t *)mapping->a_ops->readpage;
	return read_cache_page_async(mapping, index, filler, data);
}

static inline struct page *read_mapping_page(struct address_space *mapping,
					     pgoff_t index, void *data)
{
	filler_t *filler = (filler_t *)mapping->a_ops->readpage;
	return read_cache_page(mapping, index, filler, data);
}

/*
 * Return byte-offset into filesystem object for page.
 */
static inline loff_t page_offset(struct page *page)
{
	return ((loff_t)page->index) << PAGE_CACHE_SHIFT;
}

extern pgoff_t linear_hugepage_index(struct vm_area_struct *vma,
				     unsigned long address);

static inline pgoff_t linear_page_index(struct vm_area_struct *vma,
					unsigned long address)
{
	pgoff_t pgoff;
	if (unlikely(is_vm_hugetlb_page(vma)))
		return linear_hugepage_index(vma, address);
	pgoff = (address - vma->vm_start) >> PAGE_SHIFT;
	pgoff += vma->vm_pgoff;
	return pgoff >> (PAGE_CACHE_SHIFT - PAGE_SHIFT);
}

extern void __lock_page(struct page *page);
extern int __lock_page_killable(struct page *page);
extern void __lock_page_nosync(struct page *page);
extern void unlock_page(struct page *page);

static inline void __set_page_locked(struct page *page)
{
	__set_bit(PG_locked, &page->flags);
}

static inline void __clear_page_locked(struct page *page)
{
	__clear_bit(PG_locked, &page->flags);
}

static inline int trylock_page(struct page *page)
{
	return (likely(!test_and_set_bit_lock(PG_locked, &page->flags)));
}

/*
 * lock_page may only be called if we have the page's inode pinned.
 */
static inline void lock_page(struct page *page)
{
	might_sleep();
	if (!trylock_page(page))
		__lock_page(page);
}

/*
 * lock_page_killable is like lock_page but can be interrupted by fatal
 * signals.  It returns 0 if it locked the page and -EINTR if it was
 * killed while waiting.
 */
static inline int lock_page_killable(struct page *page)
{
	might_sleep();
	if (!trylock_page(page))
		return __lock_page_killable(page);
	return 0;
}

/*
 * lock_page_nosync should only be used if we can't pin the page's inode.
 * Doesn't play quite so well with block device plugging.
 */
static inline void lock_page_nosync(struct page *page)
{
	might_sleep();
	if (!trylock_page(page))
		__lock_page_nosync(page);
}
	
/*
 * This is exported only for wait_on_page_locked/wait_on_page_writeback.
 * Never use this directly!
 */
extern void wait_on_page_bit(struct page *page, int bit_nr);

/* 
 * Wait for a page to be unlocked.
 *
 * This must be called with the caller "holding" the page,
 * ie with increased "page->count" so that the page won't
 * go away during the wait..
 */
static inline void wait_on_page_locked(struct page *page)
{
	if (PageLocked(page))
		wait_on_page_bit(page, PG_locked);
}

/* 
 * Wait for a page to complete writeback
 */
static inline void wait_on_page_writeback(struct page *page)
{
	if (PageWriteback(page))
		wait_on_page_bit(page, PG_writeback);
}

extern void end_page_writeback(struct page *page);

/*
 * Add an arbitrary waiter to a page's wait queue
 */
extern void add_page_wait_queue(struct page *page, wait_queue_t *waiter);

/*
 * Fault a userspace page into pagetables.  Return non-zero on a fault.
 *
 * This assumes that two userspace pages are always sufficient.  That's
 * not true if PAGE_CACHE_SIZE > PAGE_SIZE.
 */
static inline int fault_in_pages_writeable(char __user *uaddr, int size)
{
	int ret;

	if (unlikely(size == 0))
		return 0;

	/*
	 * Writing zeroes into userspace here is OK, because we know that if
	 * the zero gets there, we'll be overwriting it.
	 */
	ret = __put_user(0, uaddr);
	if (ret == 0) {
		char __user *end = uaddr + size - 1;

		/*
		 * If the page was already mapped, this will get a cache miss
		 * for sure, so try to avoid doing it.
		 */
		if (((unsigned long)uaddr & PAGE_MASK) !=
				((unsigned long)end & PAGE_MASK))
		 	ret = __put_user(0, end);
	}
	return ret;
}

static inline int fault_in_pages_readable(const char __user *uaddr, int size)
{
	volatile char c;
	int ret;

	if (unlikely(size == 0))
		return 0;

	ret = __get_user(c, uaddr);
	if (ret == 0) {
		const char __user *end = uaddr + size - 1;

		if (((unsigned long)uaddr & PAGE_MASK) !=
				((unsigned long)end & PAGE_MASK)) {
		 	ret = __get_user(c, end);
			(void)c;
		}
	}
	return ret;
}

int add_to_page_cache_locked(struct page *page, struct address_space *mapping,
				pgoff_t index, gfp_t gfp_mask);
int add_to_page_cache_lru(struct page *page, struct address_space *mapping,
				pgoff_t index, gfp_t gfp_mask);
extern void remove_from_page_cache(struct page *page);
extern void __remove_from_page_cache(struct page *page);

/*
 * Like add_to_page_cache_locked, but used to add newly allocated pages:
 * the page is new, so we can just run __set_page_locked() against it.
 */
static inline int add_to_page_cache(struct page *page,
		struct address_space *mapping, pgoff_t offset, gfp_t gfp_mask)
{
	int error;

	__set_page_locked(page);
	error = add_to_page_cache_locked(page, mapping, offset, gfp_mask);
	if (unlikely(error))
		__clear_page_locked(page);
	return error;
}

#endif /* _LINUX_PAGEMAP_H */
