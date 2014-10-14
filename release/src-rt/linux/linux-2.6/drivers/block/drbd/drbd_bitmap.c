/*
   drbd_bitmap.c

   This file is part of DRBD by Philipp Reisner and Lars Ellenberg.

   Copyright (C) 2004-2008, LINBIT Information Technologies GmbH.
   Copyright (C) 2004-2008, Philipp Reisner <philipp.reisner@linbit.com>.
   Copyright (C) 2004-2008, Lars Ellenberg <lars.ellenberg@linbit.com>.

   drbd is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   drbd is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with drbd; see the file COPYING.  If not, write to
   the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/bitops.h>
#include <linux/vmalloc.h>
#include <linux/string.h>
#include <linux/drbd.h>
#include <linux/slab.h>
#include <asm/kmap_types.h>

#include "drbd_int.h"


/* OPAQUE outside this file!
 * interface defined in drbd_int.h

 * convention:
 * function name drbd_bm_... => used elsewhere, "public".
 * function name      bm_... => internal to implementation, "private".
 */


/*
 * LIMITATIONS:
 * We want to support >= peta byte of backend storage, while for now still using
 * a granularity of one bit per 4KiB of storage.
 * 1 << 50		bytes backend storage (1 PiB)
 * 1 << (50 - 12)	bits needed
 *	38 --> we need u64 to index and count bits
 * 1 << (38 - 3)	bitmap bytes needed
 *	35 --> we still need u64 to index and count bytes
 *			(that's 32 GiB of bitmap for 1 PiB storage)
 * 1 << (35 - 2)	32bit longs needed
 *	33 --> we'd even need u64 to index and count 32bit long words.
 * 1 << (35 - 3)	64bit longs needed
 *	32 --> we could get away with a 32bit unsigned int to index and count
 *	64bit long words, but I rather stay with unsigned long for now.
 *	We probably should neither count nor point to bytes or long words
 *	directly, but either by bitnumber, or by page index and offset.
 * 1 << (35 - 12)
 *	22 --> we need that much 4KiB pages of bitmap.
 *	1 << (22 + 3) --> on a 64bit arch,
 *	we need 32 MiB to store the array of page pointers.
 *
 * Because I'm lazy, and because the resulting patch was too large, too ugly
 * and still incomplete, on 32bit we still "only" support 16 TiB (minus some),
 * (1 << 32) bits * 4k storage.
 *

 * bitmap storage and IO:
 *	Bitmap is stored little endian on disk, and is kept little endian in
 *	core memory. Currently we still hold the full bitmap in core as long
 *	as we are "attached" to a local disk, which at 32 GiB for 1PiB storage
 *	seems excessive.
 *
 *	We plan to reduce the amount of in-core bitmap pages by pageing them in
 *	and out against their on-disk location as necessary, but need to make
 *	sure we don't cause too much meta data IO, and must not deadlock in
 *	tight memory situations. This needs some more work.
 */

/*
 * NOTE
 *  Access to the *bm_pages is protected by bm_lock.
 *  It is safe to read the other members within the lock.
 *
 *  drbd_bm_set_bits is called from bio_endio callbacks,
 *  We may be called with irq already disabled,
 *  so we need spin_lock_irqsave().
 *  And we need the kmap_atomic.
 */
struct drbd_bitmap {
	struct page **bm_pages;
	spinlock_t bm_lock;

	/* see LIMITATIONS: above */

	unsigned long bm_set;       /* nr of set bits; THINK maybe atomic_t? */
	unsigned long bm_bits;
	size_t   bm_words;
	size_t   bm_number_of_pages;
	sector_t bm_dev_capacity;
	struct mutex bm_change; /* serializes resize operations */

	wait_queue_head_t bm_io_wait; /* used to serialize IO of single pages */

	enum bm_flag bm_flags;

	/* debugging aid, in case we are still racy somewhere */
	char          *bm_why;
	struct task_struct *bm_task;
};

static int __bm_change_bits_to(struct drbd_conf *mdev, const unsigned long s,
			       unsigned long e, int val, const enum km_type km);

#define bm_print_lock_info(m) __bm_print_lock_info(m, __func__)
static void __bm_print_lock_info(struct drbd_conf *mdev, const char *func)
{
	struct drbd_bitmap *b = mdev->bitmap;
	if (!__ratelimit(&drbd_ratelimit_state))
		return;
	dev_err(DEV, "FIXME %s in %s, bitmap locked for '%s' by %s\n",
	    current == mdev->receiver.task ? "receiver" :
	    current == mdev->asender.task  ? "asender"  :
	    current == mdev->worker.task   ? "worker"   : current->comm,
	    func, b->bm_why ?: "?",
	    b->bm_task == mdev->receiver.task ? "receiver" :
	    b->bm_task == mdev->asender.task  ? "asender"  :
	    b->bm_task == mdev->worker.task   ? "worker"   : "?");
}

void drbd_bm_lock(struct drbd_conf *mdev, char *why, enum bm_flag flags)
{
	struct drbd_bitmap *b = mdev->bitmap;
	int trylock_failed;

	if (!b) {
		dev_err(DEV, "FIXME no bitmap in drbd_bm_lock!?\n");
		return;
	}

	trylock_failed = !mutex_trylock(&b->bm_change);

	if (trylock_failed) {
		dev_warn(DEV, "%s going to '%s' but bitmap already locked for '%s' by %s\n",
		    current == mdev->receiver.task ? "receiver" :
		    current == mdev->asender.task  ? "asender"  :
		    current == mdev->worker.task   ? "worker"   : current->comm,
		    why, b->bm_why ?: "?",
		    b->bm_task == mdev->receiver.task ? "receiver" :
		    b->bm_task == mdev->asender.task  ? "asender"  :
		    b->bm_task == mdev->worker.task   ? "worker"   : "?");
		mutex_lock(&b->bm_change);
	}
	if (BM_LOCKED_MASK & b->bm_flags)
		dev_err(DEV, "FIXME bitmap already locked in bm_lock\n");
	b->bm_flags |= flags & BM_LOCKED_MASK;

	b->bm_why  = why;
	b->bm_task = current;
}

void drbd_bm_unlock(struct drbd_conf *mdev)
{
	struct drbd_bitmap *b = mdev->bitmap;
	if (!b) {
		dev_err(DEV, "FIXME no bitmap in drbd_bm_unlock!?\n");
		return;
	}

	if (!(BM_LOCKED_MASK & mdev->bitmap->bm_flags))
		dev_err(DEV, "FIXME bitmap not locked in bm_unlock\n");

	b->bm_flags &= ~BM_LOCKED_MASK;
	b->bm_why  = NULL;
	b->bm_task = NULL;
	mutex_unlock(&b->bm_change);
}

/* we store some "meta" info about our pages in page->private */
/* at a granularity of 4k storage per bitmap bit:
 * one peta byte storage: 1<<50 byte, 1<<38 * 4k storage blocks
 *  1<<38 bits,
 *  1<<23 4k bitmap pages.
 * Use 24 bits as page index, covers 2 peta byte storage
 * at a granularity of 4k per bit.
 * Used to report the failed page idx on io error from the endio handlers.
 */
#define BM_PAGE_IDX_MASK	((1UL<<24)-1)
/* this page is currently read in, or written back */
#define BM_PAGE_IO_LOCK		31
/* if there has been an IO error for this page */
#define BM_PAGE_IO_ERROR	30
/* this is to be able to intelligently skip disk IO,
 * set if bits have been set since last IO. */
#define BM_PAGE_NEED_WRITEOUT	29
/* to mark for lazy writeout once syncer cleared all clearable bits,
 * we if bits have been cleared since last IO. */
#define BM_PAGE_LAZY_WRITEOUT	28

/* store_page_idx uses non-atomic assingment. It is only used directly after
 * allocating the page.  All other bm_set_page_* and bm_clear_page_* need to
 * use atomic bit manipulation, as set_out_of_sync (and therefore bitmap
 * changes) may happen from various contexts, and wait_on_bit/wake_up_bit
 * requires it all to be atomic as well. */
static void bm_store_page_idx(struct page *page, unsigned long idx)
{
	BUG_ON(0 != (idx & ~BM_PAGE_IDX_MASK));
	page_private(page) |= idx;
}

static unsigned long bm_page_to_idx(struct page *page)
{
	return page_private(page) & BM_PAGE_IDX_MASK;
}

/* As is very unlikely that the same page is under IO from more than one
 * context, we can get away with a bit per page and one wait queue per bitmap.
 */
static void bm_page_lock_io(struct drbd_conf *mdev, int page_nr)
{
	struct drbd_bitmap *b = mdev->bitmap;
	void *addr = &page_private(b->bm_pages[page_nr]);
	wait_event(b->bm_io_wait, !test_and_set_bit(BM_PAGE_IO_LOCK, addr));
}

static void bm_page_unlock_io(struct drbd_conf *mdev, int page_nr)
{
	struct drbd_bitmap *b = mdev->bitmap;
	void *addr = &page_private(b->bm_pages[page_nr]);
	clear_bit(BM_PAGE_IO_LOCK, addr);
	smp_mb__after_clear_bit();
	wake_up(&mdev->bitmap->bm_io_wait);
}

/* set _before_ submit_io, so it may be reset due to being changed
 * while this page is in flight... will get submitted later again */
static void bm_set_page_unchanged(struct page *page)
{
	/* use cmpxchg? */
	clear_bit(BM_PAGE_NEED_WRITEOUT, &page_private(page));
	clear_bit(BM_PAGE_LAZY_WRITEOUT, &page_private(page));
}

static void bm_set_page_need_writeout(struct page *page)
{
	set_bit(BM_PAGE_NEED_WRITEOUT, &page_private(page));
}

static int bm_test_page_unchanged(struct page *page)
{
	volatile const unsigned long *addr = &page_private(page);
	return (*addr & ((1UL<<BM_PAGE_NEED_WRITEOUT)|(1UL<<BM_PAGE_LAZY_WRITEOUT))) == 0;
}

static void bm_set_page_io_err(struct page *page)
{
	set_bit(BM_PAGE_IO_ERROR, &page_private(page));
}

static void bm_clear_page_io_err(struct page *page)
{
	clear_bit(BM_PAGE_IO_ERROR, &page_private(page));
}

static void bm_set_page_lazy_writeout(struct page *page)
{
	set_bit(BM_PAGE_LAZY_WRITEOUT, &page_private(page));
}

static int bm_test_page_lazy_writeout(struct page *page)
{
	return test_bit(BM_PAGE_LAZY_WRITEOUT, &page_private(page));
}

/* on a 32bit box, this would allow for exactly (2<<38) bits. */
static unsigned int bm_word_to_page_idx(struct drbd_bitmap *b, unsigned long long_nr)
{
	/* page_nr = (word*sizeof(long)) >> PAGE_SHIFT; */
	unsigned int page_nr = long_nr >> (PAGE_SHIFT - LN2_BPL + 3);
	BUG_ON(page_nr >= b->bm_number_of_pages);
	return page_nr;
}

static unsigned int bm_bit_to_page_idx(struct drbd_bitmap *b, u64 bitnr)
{
	/* page_nr = (bitnr/8) >> PAGE_SHIFT; */
	unsigned int page_nr = bitnr >> (PAGE_SHIFT + 3);
	BUG_ON(page_nr >= b->bm_number_of_pages);
	return page_nr;
}

static unsigned long *__bm_map_pidx(struct drbd_bitmap *b, unsigned int idx, const enum km_type km)
{
	struct page *page = b->bm_pages[idx];
	return (unsigned long *) kmap_atomic(page, km);
}

static unsigned long *bm_map_pidx(struct drbd_bitmap *b, unsigned int idx)
{
	return __bm_map_pidx(b, idx, KM_IRQ1);
}

static void __bm_unmap(unsigned long *p_addr, const enum km_type km)
{
	kunmap_atomic(p_addr, km);
};

static void bm_unmap(unsigned long *p_addr)
{
	return __bm_unmap(p_addr, KM_IRQ1);
}

/* long word offset of _bitmap_ sector */
#define S2W(s)	((s)<<(BM_EXT_SHIFT-BM_BLOCK_SHIFT-LN2_BPL))
/* word offset from start of bitmap to word number _in_page_
 * modulo longs per page
#define MLPP(X) ((X) % (PAGE_SIZE/sizeof(long))
 hm, well, Philipp thinks gcc might not optimze the % into & (... - 1)
 so do it explicitly:
 */
#define MLPP(X) ((X) & ((PAGE_SIZE/sizeof(long))-1))

/* Long words per page */
#define LWPP (PAGE_SIZE/sizeof(long))

/*
 * actually most functions herein should take a struct drbd_bitmap*, not a
 * struct drbd_conf*, but for the debug macros I like to have the mdev around
 * to be able to report device specific.
 */


static void bm_free_pages(struct page **pages, unsigned long number)
{
	unsigned long i;
	if (!pages)
		return;

	for (i = 0; i < number; i++) {
		if (!pages[i]) {
			printk(KERN_ALERT "drbd: bm_free_pages tried to free "
					  "a NULL pointer; i=%lu n=%lu\n",
					  i, number);
			continue;
		}
		__free_page(pages[i]);
		pages[i] = NULL;
	}
}

static void bm_vk_free(void *ptr, int v)
{
	if (v)
		vfree(ptr);
	else
		kfree(ptr);
}

/*
 * "have" and "want" are NUMBER OF PAGES.
 */
static struct page **bm_realloc_pages(struct drbd_bitmap *b, unsigned long want)
{
	struct page **old_pages = b->bm_pages;
	struct page **new_pages, *page;
	unsigned int i, bytes, vmalloced = 0;
	unsigned long have = b->bm_number_of_pages;

	BUG_ON(have == 0 && old_pages != NULL);
	BUG_ON(have != 0 && old_pages == NULL);

	if (have == want)
		return old_pages;

	/* Trying kmalloc first, falling back to vmalloc.
	 * GFP_KERNEL is ok, as this is done when a lower level disk is
	 * "attached" to the drbd.  Context is receiver thread or cqueue
	 * thread.  As we have no disk yet, we are not in the IO path,
	 * not even the IO path of the peer. */
	bytes = sizeof(struct page *)*want;
	new_pages = kmalloc(bytes, GFP_KERNEL);
	if (!new_pages) {
		new_pages = vmalloc(bytes);
		if (!new_pages)
			return NULL;
		vmalloced = 1;
	}

	memset(new_pages, 0, bytes);
	if (want >= have) {
		for (i = 0; i < have; i++)
			new_pages[i] = old_pages[i];
		for (; i < want; i++) {
			page = alloc_page(GFP_HIGHUSER);
			if (!page) {
				bm_free_pages(new_pages + have, i - have);
				bm_vk_free(new_pages, vmalloced);
				return NULL;
			}
			/* we want to know which page it is
			 * from the endio handlers */
			bm_store_page_idx(page, i);
			new_pages[i] = page;
		}
	} else {
		for (i = 0; i < want; i++)
			new_pages[i] = old_pages[i];
		/* NOT HERE, we are outside the spinlock!
		bm_free_pages(old_pages + want, have - want);
		*/
	}

	if (vmalloced)
		b->bm_flags |= BM_P_VMALLOCED;
	else
		b->bm_flags &= ~BM_P_VMALLOCED;

	return new_pages;
}

/*
 * called on driver init only. TODO call when a device is created.
 * allocates the drbd_bitmap, and stores it in mdev->bitmap.
 */
int drbd_bm_init(struct drbd_conf *mdev)
{
	struct drbd_bitmap *b = mdev->bitmap;
	WARN_ON(b != NULL);
	b = kzalloc(sizeof(struct drbd_bitmap), GFP_KERNEL);
	if (!b)
		return -ENOMEM;
	spin_lock_init(&b->bm_lock);
	mutex_init(&b->bm_change);
	init_waitqueue_head(&b->bm_io_wait);

	mdev->bitmap = b;

	return 0;
}

sector_t drbd_bm_capacity(struct drbd_conf *mdev)
{
	ERR_IF(!mdev->bitmap) return 0;
	return mdev->bitmap->bm_dev_capacity;
}

/* called on driver unload. TODO: call when a device is destroyed.
 */
void drbd_bm_cleanup(struct drbd_conf *mdev)
{
	ERR_IF (!mdev->bitmap) return;
	bm_free_pages(mdev->bitmap->bm_pages, mdev->bitmap->bm_number_of_pages);
	bm_vk_free(mdev->bitmap->bm_pages, (BM_P_VMALLOCED & mdev->bitmap->bm_flags));
	kfree(mdev->bitmap);
	mdev->bitmap = NULL;
}

/*
 * since (b->bm_bits % BITS_PER_LONG) != 0,
 * this masks out the remaining bits.
 * Returns the number of bits cleared.
 */
#define BITS_PER_PAGE		(1UL << (PAGE_SHIFT + 3))
#define BITS_PER_PAGE_MASK	(BITS_PER_PAGE - 1)
#define BITS_PER_LONG_MASK	(BITS_PER_LONG - 1)
static int bm_clear_surplus(struct drbd_bitmap *b)
{
	unsigned long mask;
	unsigned long *p_addr, *bm;
	int tmp;
	int cleared = 0;

	/* number of bits modulo bits per page */
	tmp = (b->bm_bits & BITS_PER_PAGE_MASK);
	/* mask the used bits of the word containing the last bit */
	mask = (1UL << (tmp & BITS_PER_LONG_MASK)) -1;
	/* bitmap is always stored little endian,
	 * on disk and in core memory alike */
	mask = cpu_to_lel(mask);

	p_addr = bm_map_pidx(b, b->bm_number_of_pages - 1);
	bm = p_addr + (tmp/BITS_PER_LONG);
	if (mask) {
		/* If mask != 0, we are not exactly aligned, so bm now points
		 * to the long containing the last bit.
		 * If mask == 0, bm already points to the word immediately
		 * after the last (long word aligned) bit. */
		cleared = hweight_long(*bm & ~mask);
		*bm &= mask;
		bm++;
	}

	if (BITS_PER_LONG == 32 && ((bm - p_addr) & 1) == 1) {
		/* on a 32bit arch, we may need to zero out
		 * a padding long to align with a 64bit remote */
		cleared += hweight_long(*bm);
		*bm = 0;
	}
	bm_unmap(p_addr);
	return cleared;
}

static void bm_set_surplus(struct drbd_bitmap *b)
{
	unsigned long mask;
	unsigned long *p_addr, *bm;
	int tmp;

	/* number of bits modulo bits per page */
	tmp = (b->bm_bits & BITS_PER_PAGE_MASK);
	/* mask the used bits of the word containing the last bit */
	mask = (1UL << (tmp & BITS_PER_LONG_MASK)) -1;
	/* bitmap is always stored little endian,
	 * on disk and in core memory alike */
	mask = cpu_to_lel(mask);

	p_addr = bm_map_pidx(b, b->bm_number_of_pages - 1);
	bm = p_addr + (tmp/BITS_PER_LONG);
	if (mask) {
		/* If mask != 0, we are not exactly aligned, so bm now points
		 * to the long containing the last bit.
		 * If mask == 0, bm already points to the word immediately
		 * after the last (long word aligned) bit. */
		*bm |= ~mask;
		bm++;
	}

	if (BITS_PER_LONG == 32 && ((bm - p_addr) & 1) == 1) {
		/* on a 32bit arch, we may need to zero out
		 * a padding long to align with a 64bit remote */
		*bm = ~0UL;
	}
	bm_unmap(p_addr);
}

/* you better not modify the bitmap while this is running,
 * or its results will be stale */
static unsigned long bm_count_bits(struct drbd_bitmap *b)
{
	unsigned long *p_addr;
	unsigned long bits = 0;
	unsigned long mask = (1UL << (b->bm_bits & BITS_PER_LONG_MASK)) -1;
	int idx, i, last_word;

	/* all but last page */
	for (idx = 0; idx < b->bm_number_of_pages - 1; idx++) {
		p_addr = __bm_map_pidx(b, idx, KM_USER0);
		for (i = 0; i < LWPP; i++)
			bits += hweight_long(p_addr[i]);
		__bm_unmap(p_addr, KM_USER0);
		cond_resched();
	}
	/* last (or only) page */
	last_word = ((b->bm_bits - 1) & BITS_PER_PAGE_MASK) >> LN2_BPL;
	p_addr = __bm_map_pidx(b, idx, KM_USER0);
	for (i = 0; i < last_word; i++)
		bits += hweight_long(p_addr[i]);
	p_addr[last_word] &= cpu_to_lel(mask);
	bits += hweight_long(p_addr[last_word]);
	/* 32bit arch, may have an unused padding long */
	if (BITS_PER_LONG == 32 && (last_word & 1) == 0)
		p_addr[last_word+1] = 0;
	__bm_unmap(p_addr, KM_USER0);
	return bits;
}

/* offset and len in long words.*/
static void bm_memset(struct drbd_bitmap *b, size_t offset, int c, size_t len)
{
	unsigned long *p_addr, *bm;
	unsigned int idx;
	size_t do_now, end;

	end = offset + len;

	if (end > b->bm_words) {
		printk(KERN_ALERT "drbd: bm_memset end > bm_words\n");
		return;
	}

	while (offset < end) {
		do_now = min_t(size_t, ALIGN(offset + 1, LWPP), end) - offset;
		idx = bm_word_to_page_idx(b, offset);
		p_addr = bm_map_pidx(b, idx);
		bm = p_addr + MLPP(offset);
		if (bm+do_now > p_addr + LWPP) {
			printk(KERN_ALERT "drbd: BUG BUG BUG! p_addr:%p bm:%p do_now:%d\n",
			       p_addr, bm, (int)do_now);
		} else
			memset(bm, c, do_now * sizeof(long));
		bm_unmap(p_addr);
		bm_set_page_need_writeout(b->bm_pages[idx]);
		offset += do_now;
	}
}

/*
 * make sure the bitmap has enough room for the attached storage,
 * if necessary, resize.
 * called whenever we may have changed the device size.
 * returns -ENOMEM if we could not allocate enough memory, 0 on success.
 * In case this is actually a resize, we copy the old bitmap into the new one.
 * Otherwise, the bitmap is initialized to all bits set.
 */
int drbd_bm_resize(struct drbd_conf *mdev, sector_t capacity, int set_new_bits)
{
	struct drbd_bitmap *b = mdev->bitmap;
	unsigned long bits, words, owords, obits;
	unsigned long want, have, onpages; /* number of pages */
	struct page **npages, **opages = NULL;
	int err = 0, growing;
	int opages_vmalloced;

	ERR_IF(!b) return -ENOMEM;

	drbd_bm_lock(mdev, "resize", BM_LOCKED_MASK);

	dev_info(DEV, "drbd_bm_resize called with capacity == %llu\n",
			(unsigned long long)capacity);

	if (capacity == b->bm_dev_capacity)
		goto out;

	opages_vmalloced = (BM_P_VMALLOCED & b->bm_flags);

	if (capacity == 0) {
		spin_lock_irq(&b->bm_lock);
		opages = b->bm_pages;
		onpages = b->bm_number_of_pages;
		owords = b->bm_words;
		b->bm_pages = NULL;
		b->bm_number_of_pages =
		b->bm_set   =
		b->bm_bits  =
		b->bm_words =
		b->bm_dev_capacity = 0;
		spin_unlock_irq(&b->bm_lock);
		bm_free_pages(opages, onpages);
		bm_vk_free(opages, opages_vmalloced);
		goto out;
	}
	bits  = BM_SECT_TO_BIT(ALIGN(capacity, BM_SECT_PER_BIT));

	/* if we would use
	   words = ALIGN(bits,BITS_PER_LONG) >> LN2_BPL;
	   a 32bit host could present the wrong number of words
	   to a 64bit host.
	*/
	words = ALIGN(bits, 64) >> LN2_BPL;

	if (get_ldev(mdev)) {
		u64 bits_on_disk = ((u64)mdev->ldev->md.md_size_sect-MD_BM_OFFSET) << 12;
		put_ldev(mdev);
		if (bits > bits_on_disk) {
			dev_info(DEV, "bits = %lu\n", bits);
			dev_info(DEV, "bits_on_disk = %llu\n", bits_on_disk);
			err = -ENOSPC;
			goto out;
		}
	}

	want = ALIGN(words*sizeof(long), PAGE_SIZE) >> PAGE_SHIFT;
	have = b->bm_number_of_pages;
	if (want == have) {
		D_ASSERT(b->bm_pages != NULL);
		npages = b->bm_pages;
	} else {
		if (drbd_insert_fault(mdev, DRBD_FAULT_BM_ALLOC))
			npages = NULL;
		else
			npages = bm_realloc_pages(b, want);
	}

	if (!npages) {
		err = -ENOMEM;
		goto out;
	}

	spin_lock_irq(&b->bm_lock);
	opages = b->bm_pages;
	owords = b->bm_words;
	obits  = b->bm_bits;

	growing = bits > obits;
	if (opages && growing && set_new_bits)
		bm_set_surplus(b);

	b->bm_pages = npages;
	b->bm_number_of_pages = want;
	b->bm_bits  = bits;
	b->bm_words = words;
	b->bm_dev_capacity = capacity;

	if (growing) {
		if (set_new_bits) {
			bm_memset(b, owords, 0xff, words-owords);
			b->bm_set += bits - obits;
		} else
			bm_memset(b, owords, 0x00, words-owords);

	}

	if (want < have) {
		/* implicit: (opages != NULL) && (opages != npages) */
		bm_free_pages(opages + want, have - want);
	}

	(void)bm_clear_surplus(b);

	spin_unlock_irq(&b->bm_lock);
	if (opages != npages)
		bm_vk_free(opages, opages_vmalloced);
	if (!growing)
		b->bm_set = bm_count_bits(b);
	dev_info(DEV, "resync bitmap: bits=%lu words=%lu pages=%lu\n", bits, words, want);

 out:
	drbd_bm_unlock(mdev);
	return err;
}

/* inherently racy:
 * if not protected by other means, return value may be out of date when
 * leaving this function...
 * we still need to lock it, since it is important that this returns
 * bm_set == 0 precisely.
 *
 * maybe bm_set should be atomic_t ?
 */
unsigned long _drbd_bm_total_weight(struct drbd_conf *mdev)
{
	struct drbd_bitmap *b = mdev->bitmap;
	unsigned long s;
	unsigned long flags;

	ERR_IF(!b) return 0;
	ERR_IF(!b->bm_pages) return 0;

	spin_lock_irqsave(&b->bm_lock, flags);
	s = b->bm_set;
	spin_unlock_irqrestore(&b->bm_lock, flags);

	return s;
}

unsigned long drbd_bm_total_weight(struct drbd_conf *mdev)
{
	unsigned long s;
	/* if I don't have a disk, I don't know about out-of-sync status */
	if (!get_ldev_if_state(mdev, D_NEGOTIATING))
		return 0;
	s = _drbd_bm_total_weight(mdev);
	put_ldev(mdev);
	return s;
}

size_t drbd_bm_words(struct drbd_conf *mdev)
{
	struct drbd_bitmap *b = mdev->bitmap;
	ERR_IF(!b) return 0;
	ERR_IF(!b->bm_pages) return 0;

	return b->bm_words;
}

unsigned long drbd_bm_bits(struct drbd_conf *mdev)
{
	struct drbd_bitmap *b = mdev->bitmap;
	ERR_IF(!b) return 0;

	return b->bm_bits;
}

/* merge number words from buffer into the bitmap starting at offset.
 * buffer[i] is expected to be little endian unsigned long.
 * bitmap must be locked by drbd_bm_lock.
 * currently only used from receive_bitmap.
 */
void drbd_bm_merge_lel(struct drbd_conf *mdev, size_t offset, size_t number,
			unsigned long *buffer)
{
	struct drbd_bitmap *b = mdev->bitmap;
	unsigned long *p_addr, *bm;
	unsigned long word, bits;
	unsigned int idx;
	size_t end, do_now;

	end = offset + number;

	ERR_IF(!b) return;
	ERR_IF(!b->bm_pages) return;
	if (number == 0)
		return;
	WARN_ON(offset >= b->bm_words);
	WARN_ON(end    >  b->bm_words);

	spin_lock_irq(&b->bm_lock);
	while (offset < end) {
		do_now = min_t(size_t, ALIGN(offset+1, LWPP), end) - offset;
		idx = bm_word_to_page_idx(b, offset);
		p_addr = bm_map_pidx(b, idx);
		bm = p_addr + MLPP(offset);
		offset += do_now;
		while (do_now--) {
			bits = hweight_long(*bm);
			word = *bm | *buffer++;
			*bm++ = word;
			b->bm_set += hweight_long(word) - bits;
		}
		bm_unmap(p_addr);
		bm_set_page_need_writeout(b->bm_pages[idx]);
	}
	/* with 32bit <-> 64bit cross-platform connect
	 * this is only correct for current usage,
	 * where we _know_ that we are 64 bit aligned,
	 * and know that this function is used in this way, too...
	 */
	if (end == b->bm_words)
		b->bm_set -= bm_clear_surplus(b);
	spin_unlock_irq(&b->bm_lock);
}

/* copy number words from the bitmap starting at offset into the buffer.
 * buffer[i] will be little endian unsigned long.
 */
void drbd_bm_get_lel(struct drbd_conf *mdev, size_t offset, size_t number,
		     unsigned long *buffer)
{
	struct drbd_bitmap *b = mdev->bitmap;
	unsigned long *p_addr, *bm;
	size_t end, do_now;

	end = offset + number;

	ERR_IF(!b) return;
	ERR_IF(!b->bm_pages) return;

	spin_lock_irq(&b->bm_lock);
	if ((offset >= b->bm_words) ||
	    (end    >  b->bm_words) ||
	    (number <= 0))
		dev_err(DEV, "offset=%lu number=%lu bm_words=%lu\n",
			(unsigned long)	offset,
			(unsigned long)	number,
			(unsigned long) b->bm_words);
	else {
		while (offset < end) {
			do_now = min_t(size_t, ALIGN(offset+1, LWPP), end) - offset;
			p_addr = bm_map_pidx(b, bm_word_to_page_idx(b, offset));
			bm = p_addr + MLPP(offset);
			offset += do_now;
			while (do_now--)
				*buffer++ = *bm++;
			bm_unmap(p_addr);
		}
	}
	spin_unlock_irq(&b->bm_lock);
}

/* set all bits in the bitmap */
void drbd_bm_set_all(struct drbd_conf *mdev)
{
	struct drbd_bitmap *b = mdev->bitmap;
	ERR_IF(!b) return;
	ERR_IF(!b->bm_pages) return;

	spin_lock_irq(&b->bm_lock);
	bm_memset(b, 0, 0xff, b->bm_words);
	(void)bm_clear_surplus(b);
	b->bm_set = b->bm_bits;
	spin_unlock_irq(&b->bm_lock);
}

/* clear all bits in the bitmap */
void drbd_bm_clear_all(struct drbd_conf *mdev)
{
	struct drbd_bitmap *b = mdev->bitmap;
	ERR_IF(!b) return;
	ERR_IF(!b->bm_pages) return;

	spin_lock_irq(&b->bm_lock);
	bm_memset(b, 0, 0, b->bm_words);
	b->bm_set = 0;
	spin_unlock_irq(&b->bm_lock);
}

struct bm_aio_ctx {
	struct drbd_conf *mdev;
	atomic_t in_flight;
	struct completion done;
	unsigned flags;
#define BM_AIO_COPY_PAGES	1
	int error;
};

/* bv_page may be a copy, or may be the original */
static void bm_async_io_complete(struct bio *bio, int error)
{
	struct bm_aio_ctx *ctx = bio->bi_private;
	struct drbd_conf *mdev = ctx->mdev;
	struct drbd_bitmap *b = mdev->bitmap;
	unsigned int idx = bm_page_to_idx(bio->bi_io_vec[0].bv_page);
	int uptodate = bio_flagged(bio, BIO_UPTODATE);


	/* strange behavior of some lower level drivers...
	 * fail the request by clearing the uptodate flag,
	 * but do not return any error?!
	 * do we want to WARN() on this? */
	if (!error && !uptodate)
		error = -EIO;

	if ((ctx->flags & BM_AIO_COPY_PAGES) == 0 &&
	    !bm_test_page_unchanged(b->bm_pages[idx]))
		dev_warn(DEV, "bitmap page idx %u changed during IO!\n", idx);

	if (error) {
		/* ctx error will hold the completed-last non-zero error code,
		 * in case error codes differ. */
		ctx->error = error;
		bm_set_page_io_err(b->bm_pages[idx]);
		/* Not identical to on disk version of it.
		 * Is BM_PAGE_IO_ERROR enough? */
		if (__ratelimit(&drbd_ratelimit_state))
			dev_err(DEV, "IO ERROR %d on bitmap page idx %u\n",
					error, idx);
	} else {
		bm_clear_page_io_err(b->bm_pages[idx]);
		dynamic_dev_dbg(DEV, "bitmap page idx %u completed\n", idx);
	}

	bm_page_unlock_io(mdev, idx);

	/* FIXME give back to page pool */
	if (ctx->flags & BM_AIO_COPY_PAGES)
		put_page(bio->bi_io_vec[0].bv_page);

	bio_put(bio);

	if (atomic_dec_and_test(&ctx->in_flight))
		complete(&ctx->done);
}

static void bm_page_io_async(struct bm_aio_ctx *ctx, int page_nr, int rw) __must_hold(local)
{
	/* we are process context. we always get a bio */
	struct bio *bio = bio_alloc(GFP_KERNEL, 1);
	struct drbd_conf *mdev = ctx->mdev;
	struct drbd_bitmap *b = mdev->bitmap;
	struct page *page;
	unsigned int len;

	sector_t on_disk_sector =
		mdev->ldev->md.md_offset + mdev->ldev->md.bm_offset;
	on_disk_sector += ((sector_t)page_nr) << (PAGE_SHIFT-9);

	/* this might happen with very small
	 * flexible external meta data device,
	 * or with PAGE_SIZE > 4k */
	len = min_t(unsigned int, PAGE_SIZE,
		(drbd_md_last_sector(mdev->ldev) - on_disk_sector + 1)<<9);

	/* serialize IO on this page */
	bm_page_lock_io(mdev, page_nr);
	/* before memcpy and submit,
	 * so it can be redirtied any time */
	bm_set_page_unchanged(b->bm_pages[page_nr]);

	if (ctx->flags & BM_AIO_COPY_PAGES) {
		/* FIXME alloc_page is good enough for now, but actually needs
		 * to use pre-allocated page pool */
		void *src, *dest;
		page = alloc_page(__GFP_HIGHMEM|__GFP_WAIT);
		dest = kmap_atomic(page, KM_USER0);
		src = kmap_atomic(b->bm_pages[page_nr], KM_USER1);
		memcpy(dest, src, PAGE_SIZE);
		kunmap_atomic(src, KM_USER1);
		kunmap_atomic(dest, KM_USER0);
		bm_store_page_idx(page, page_nr);
	} else
		page = b->bm_pages[page_nr];

	bio->bi_bdev = mdev->ldev->md_bdev;
	bio->bi_sector = on_disk_sector;
	bio_add_page(bio, page, len, 0);
	bio->bi_private = ctx;
	bio->bi_end_io = bm_async_io_complete;

	if (drbd_insert_fault(mdev, (rw & WRITE) ? DRBD_FAULT_MD_WR : DRBD_FAULT_MD_RD)) {
		bio->bi_rw |= rw;
		bio_endio(bio, -EIO);
	} else {
		submit_bio(rw, bio);
	}
}

/*
 * bm_rw: read/write the whole bitmap from/to its on disk location.
 */
static int bm_rw(struct drbd_conf *mdev, int rw, unsigned lazy_writeout_upper_idx) __must_hold(local)
{
	struct bm_aio_ctx ctx = {
		.mdev = mdev,
		.in_flight = ATOMIC_INIT(1),
		.done = COMPLETION_INITIALIZER_ONSTACK(ctx.done),
		.flags = lazy_writeout_upper_idx ? BM_AIO_COPY_PAGES : 0,
	};
	struct drbd_bitmap *b = mdev->bitmap;
	int num_pages, i, count = 0;
	unsigned long now;
	char ppb[10];
	int err = 0;

	/*
	 * We are protected against bitmap disappearing/resizing by holding an
	 * ldev reference (caller must have called get_ldev()).
	 * For read/write, we are protected against changes to the bitmap by
	 * the bitmap lock (see drbd_bitmap_io).
	 * For lazy writeout, we don't care for ongoing changes to the bitmap,
	 * as we submit copies of pages anyways.
	 */
	if (!ctx.flags)
		WARN_ON(!(BM_LOCKED_MASK & b->bm_flags));

	num_pages = b->bm_number_of_pages;

	now = jiffies;

	/* let the layers below us try to merge these bios... */
	for (i = 0; i < num_pages; i++) {
		/* ignore completely unchanged pages */
		if (lazy_writeout_upper_idx && i == lazy_writeout_upper_idx)
			break;
		if (rw & WRITE) {
			if (bm_test_page_unchanged(b->bm_pages[i])) {
				dynamic_dev_dbg(DEV, "skipped bm write for idx %u\n", i);
				continue;
			}
			/* during lazy writeout,
			 * ignore those pages not marked for lazy writeout. */
			if (lazy_writeout_upper_idx &&
			    !bm_test_page_lazy_writeout(b->bm_pages[i])) {
				dynamic_dev_dbg(DEV, "skipped bm lazy write for idx %u\n", i);
				continue;
			}
		}
		atomic_inc(&ctx.in_flight);
		bm_page_io_async(&ctx, i, rw);
		++count;
		cond_resched();
	}

	/*
	 * We initialize ctx.in_flight to one to make sure bm_async_io_complete
	 * will not complete() early, and decrement / test it here.  If there
	 * are still some bios in flight, we need to wait for them here.
	 */
	if (!atomic_dec_and_test(&ctx.in_flight))
		wait_for_completion(&ctx.done);
	dev_info(DEV, "bitmap %s of %u pages took %lu jiffies\n",
			rw == WRITE ? "WRITE" : "READ",
			count, jiffies - now);

	if (ctx.error) {
		dev_alert(DEV, "we had at least one MD IO ERROR during bitmap IO\n");
		drbd_chk_io_error(mdev, 1, true);
		err = -EIO; /* ctx.error ? */
	}

	now = jiffies;
	if (rw == WRITE) {
		drbd_md_flush(mdev);
	} else /* rw == READ */ {
		b->bm_set = bm_count_bits(b);
		dev_info(DEV, "recounting of set bits took additional %lu jiffies\n",
		     jiffies - now);
	}
	now = b->bm_set;

	dev_info(DEV, "%s (%lu bits) marked out-of-sync by on disk bit-map.\n",
	     ppsize(ppb, now << (BM_BLOCK_SHIFT-10)), now);

	return err;
}

/**
 * drbd_bm_read() - Read the whole bitmap from its on disk location.
 * @mdev:	DRBD device.
 */
int drbd_bm_read(struct drbd_conf *mdev) __must_hold(local)
{
	return bm_rw(mdev, READ, 0);
}

/**
 * drbd_bm_write() - Write the whole bitmap to its on disk location.
 * @mdev:	DRBD device.
 *
 * Will only write pages that have changed since last IO.
 */
int drbd_bm_write(struct drbd_conf *mdev) __must_hold(local)
{
	return bm_rw(mdev, WRITE, 0);
}

/**
 * drbd_bm_lazy_write_out() - Write bitmap pages 0 to @upper_idx-1, if they have changed.
 * @mdev:	DRBD device.
 * @upper_idx:	0: write all changed pages; +ve: page index to stop scanning for changed pages
 */
int drbd_bm_write_lazy(struct drbd_conf *mdev, unsigned upper_idx) __must_hold(local)
{
	return bm_rw(mdev, WRITE, upper_idx);
}


/**
 * drbd_bm_write_page: Writes a PAGE_SIZE aligned piece of bitmap
 * @mdev:	DRBD device.
 * @idx:	bitmap page index
 *
 * We don't want to special case on logical_block_size of the backend device,
 * so we submit PAGE_SIZE aligned pieces.
 * Note that on "most" systems, PAGE_SIZE is 4k.
 *
 * In case this becomes an issue on systems with larger PAGE_SIZE,
 * we may want to change this again to write 4k aligned 4k pieces.
 */
int drbd_bm_write_page(struct drbd_conf *mdev, unsigned int idx) __must_hold(local)
{
	struct bm_aio_ctx ctx = {
		.mdev = mdev,
		.in_flight = ATOMIC_INIT(1),
		.done = COMPLETION_INITIALIZER_ONSTACK(ctx.done),
		.flags = BM_AIO_COPY_PAGES,
	};

	if (bm_test_page_unchanged(mdev->bitmap->bm_pages[idx])) {
		dynamic_dev_dbg(DEV, "skipped bm page write for idx %u\n", idx);
		return 0;
	}

	bm_page_io_async(&ctx, idx, WRITE_SYNC);
	wait_for_completion(&ctx.done);

	if (ctx.error)
		drbd_chk_io_error(mdev, 1, true);
		/* that should force detach, so the in memory bitmap will be
		 * gone in a moment as well. */

	mdev->bm_writ_cnt++;
	return ctx.error;
}

/* NOTE
 * find_first_bit returns int, we return unsigned long.
 * For this to work on 32bit arch with bitnumbers > (1<<32),
 * we'd need to return u64, and get a whole lot of other places
 * fixed where we still use unsigned long.
 *
 * this returns a bit number, NOT a sector!
 */
static unsigned long __bm_find_next(struct drbd_conf *mdev, unsigned long bm_fo,
	const int find_zero_bit, const enum km_type km)
{
	struct drbd_bitmap *b = mdev->bitmap;
	unsigned long *p_addr;
	unsigned long bit_offset;
	unsigned i;


	if (bm_fo > b->bm_bits) {
		dev_err(DEV, "bm_fo=%lu bm_bits=%lu\n", bm_fo, b->bm_bits);
		bm_fo = DRBD_END_OF_BITMAP;
	} else {
		while (bm_fo < b->bm_bits) {
			/* bit offset of the first bit in the page */
			bit_offset = bm_fo & ~BITS_PER_PAGE_MASK;
			p_addr = __bm_map_pidx(b, bm_bit_to_page_idx(b, bm_fo), km);

			if (find_zero_bit)
				i = find_next_zero_bit_le(p_addr,
						PAGE_SIZE*8, bm_fo & BITS_PER_PAGE_MASK);
			else
				i = find_next_bit_le(p_addr,
						PAGE_SIZE*8, bm_fo & BITS_PER_PAGE_MASK);

			__bm_unmap(p_addr, km);
			if (i < PAGE_SIZE*8) {
				bm_fo = bit_offset + i;
				if (bm_fo >= b->bm_bits)
					break;
				goto found;
			}
			bm_fo = bit_offset + PAGE_SIZE*8;
		}
		bm_fo = DRBD_END_OF_BITMAP;
	}
 found:
	return bm_fo;
}

static unsigned long bm_find_next(struct drbd_conf *mdev,
	unsigned long bm_fo, const int find_zero_bit)
{
	struct drbd_bitmap *b = mdev->bitmap;
	unsigned long i = DRBD_END_OF_BITMAP;

	ERR_IF(!b) return i;
	ERR_IF(!b->bm_pages) return i;

	spin_lock_irq(&b->bm_lock);
	if (BM_DONT_TEST & b->bm_flags)
		bm_print_lock_info(mdev);

	i = __bm_find_next(mdev, bm_fo, find_zero_bit, KM_IRQ1);

	spin_unlock_irq(&b->bm_lock);
	return i;
}

unsigned long drbd_bm_find_next(struct drbd_conf *mdev, unsigned long bm_fo)
{
	return bm_find_next(mdev, bm_fo, 0);
}

#if 0
/* not yet needed for anything. */
unsigned long drbd_bm_find_next_zero(struct drbd_conf *mdev, unsigned long bm_fo)
{
	return bm_find_next(mdev, bm_fo, 1);
}
#endif

/* does not spin_lock_irqsave.
 * you must take drbd_bm_lock() first */
unsigned long _drbd_bm_find_next(struct drbd_conf *mdev, unsigned long bm_fo)
{
	/* WARN_ON(!(BM_DONT_SET & mdev->b->bm_flags)); */
	return __bm_find_next(mdev, bm_fo, 0, KM_USER1);
}

unsigned long _drbd_bm_find_next_zero(struct drbd_conf *mdev, unsigned long bm_fo)
{
	/* WARN_ON(!(BM_DONT_SET & mdev->b->bm_flags)); */
	return __bm_find_next(mdev, bm_fo, 1, KM_USER1);
}

/* returns number of bits actually changed.
 * for val != 0, we change 0 -> 1, return code positive
 * for val == 0, we change 1 -> 0, return code negative
 * wants bitnr, not sector.
 * expected to be called for only a few bits (e - s about BITS_PER_LONG).
 * Must hold bitmap lock already. */
static int __bm_change_bits_to(struct drbd_conf *mdev, const unsigned long s,
	unsigned long e, int val, const enum km_type km)
{
	struct drbd_bitmap *b = mdev->bitmap;
	unsigned long *p_addr = NULL;
	unsigned long bitnr;
	unsigned int last_page_nr = -1U;
	int c = 0;
	int changed_total = 0;

	if (e >= b->bm_bits) {
		dev_err(DEV, "ASSERT FAILED: bit_s=%lu bit_e=%lu bm_bits=%lu\n",
				s, e, b->bm_bits);
		e = b->bm_bits ? b->bm_bits -1 : 0;
	}
	for (bitnr = s; bitnr <= e; bitnr++) {
		unsigned int page_nr = bm_bit_to_page_idx(b, bitnr);
		if (page_nr != last_page_nr) {
			if (p_addr)
				__bm_unmap(p_addr, km);
			if (c < 0)
				bm_set_page_lazy_writeout(b->bm_pages[last_page_nr]);
			else if (c > 0)
				bm_set_page_need_writeout(b->bm_pages[last_page_nr]);
			changed_total += c;
			c = 0;
			p_addr = __bm_map_pidx(b, page_nr, km);
			last_page_nr = page_nr;
		}
		if (val)
			c += (0 == __test_and_set_bit_le(bitnr & BITS_PER_PAGE_MASK, p_addr));
		else
			c -= (0 != __test_and_clear_bit_le(bitnr & BITS_PER_PAGE_MASK, p_addr));
	}
	if (p_addr)
		__bm_unmap(p_addr, km);
	if (c < 0)
		bm_set_page_lazy_writeout(b->bm_pages[last_page_nr]);
	else if (c > 0)
		bm_set_page_need_writeout(b->bm_pages[last_page_nr]);
	changed_total += c;
	b->bm_set += changed_total;
	return changed_total;
}

/* returns number of bits actually changed.
 * for val != 0, we change 0 -> 1, return code positive
 * for val == 0, we change 1 -> 0, return code negative
 * wants bitnr, not sector */
static int bm_change_bits_to(struct drbd_conf *mdev, const unsigned long s,
	const unsigned long e, int val)
{
	unsigned long flags;
	struct drbd_bitmap *b = mdev->bitmap;
	int c = 0;

	ERR_IF(!b) return 1;
	ERR_IF(!b->bm_pages) return 0;

	spin_lock_irqsave(&b->bm_lock, flags);
	if ((val ? BM_DONT_SET : BM_DONT_CLEAR) & b->bm_flags)
		bm_print_lock_info(mdev);

	c = __bm_change_bits_to(mdev, s, e, val, KM_IRQ1);

	spin_unlock_irqrestore(&b->bm_lock, flags);
	return c;
}

/* returns number of bits changed 0 -> 1 */
int drbd_bm_set_bits(struct drbd_conf *mdev, const unsigned long s, const unsigned long e)
{
	return bm_change_bits_to(mdev, s, e, 1);
}

/* returns number of bits changed 1 -> 0 */
int drbd_bm_clear_bits(struct drbd_conf *mdev, const unsigned long s, const unsigned long e)
{
	return -bm_change_bits_to(mdev, s, e, 0);
}

/* sets all bits in full words,
 * from first_word up to, but not including, last_word */
static inline void bm_set_full_words_within_one_page(struct drbd_bitmap *b,
		int page_nr, int first_word, int last_word)
{
	int i;
	int bits;
	unsigned long *paddr = kmap_atomic(b->bm_pages[page_nr], KM_USER0);
	for (i = first_word; i < last_word; i++) {
		bits = hweight_long(paddr[i]);
		paddr[i] = ~0UL;
		b->bm_set += BITS_PER_LONG - bits;
	}
	kunmap_atomic(paddr, KM_USER0);
}

/* Same thing as drbd_bm_set_bits, but without taking the spin_lock_irqsave.
 * You must first drbd_bm_lock().
 * Can be called to set the whole bitmap in one go.
 * Sets bits from s to e _inclusive_. */
void _drbd_bm_set_bits(struct drbd_conf *mdev, const unsigned long s, const unsigned long e)
{
	/* First set_bit from the first bit (s)
	 * up to the next long boundary (sl),
	 * then assign full words up to the last long boundary (el),
	 * then set_bit up to and including the last bit (e).
	 *
	 * Do not use memset, because we must account for changes,
	 * so we need to loop over the words with hweight() anyways.
	 */
	unsigned long sl = ALIGN(s,BITS_PER_LONG);
	unsigned long el = (e+1) & ~((unsigned long)BITS_PER_LONG-1);
	int first_page;
	int last_page;
	int page_nr;
	int first_word;
	int last_word;

	if (e - s <= 3*BITS_PER_LONG) {
		/* don't bother; el and sl may even be wrong. */
		__bm_change_bits_to(mdev, s, e, 1, KM_USER0);
		return;
	}

	/* difference is large enough that we can trust sl and el */

	/* bits filling the current long */
	if (sl)
		__bm_change_bits_to(mdev, s, sl-1, 1, KM_USER0);

	first_page = sl >> (3 + PAGE_SHIFT);
	last_page = el >> (3 + PAGE_SHIFT);

	/* MLPP: modulo longs per page */
	/* LWPP: long words per page */
	first_word = MLPP(sl >> LN2_BPL);
	last_word = LWPP;

	/* first and full pages, unless first page == last page */
	for (page_nr = first_page; page_nr < last_page; page_nr++) {
		bm_set_full_words_within_one_page(mdev->bitmap, page_nr, first_word, last_word);
		cond_resched();
		first_word = 0;
	}

	/* last page (respectively only page, for first page == last page) */
	last_word = MLPP(el >> LN2_BPL);
	bm_set_full_words_within_one_page(mdev->bitmap, last_page, first_word, last_word);

	/* possibly trailing bits.
	 * example: (e & 63) == 63, el will be e+1.
	 * if that even was the very last bit,
	 * it would trigger an assert in __bm_change_bits_to()
	 */
	if (el <= e)
		__bm_change_bits_to(mdev, el, e, 1, KM_USER0);
}

/* returns bit state
 * wants bitnr, NOT sector.
 * inherently racy... area needs to be locked by means of {al,rs}_lru
 *  1 ... bit set
 *  0 ... bit not set
 * -1 ... first out of bounds access, stop testing for bits!
 */
int drbd_bm_test_bit(struct drbd_conf *mdev, const unsigned long bitnr)
{
	unsigned long flags;
	struct drbd_bitmap *b = mdev->bitmap;
	unsigned long *p_addr;
	int i;

	ERR_IF(!b) return 0;
	ERR_IF(!b->bm_pages) return 0;

	spin_lock_irqsave(&b->bm_lock, flags);
	if (BM_DONT_TEST & b->bm_flags)
		bm_print_lock_info(mdev);
	if (bitnr < b->bm_bits) {
		p_addr = bm_map_pidx(b, bm_bit_to_page_idx(b, bitnr));
		i = test_bit_le(bitnr & BITS_PER_PAGE_MASK, p_addr) ? 1 : 0;
		bm_unmap(p_addr);
	} else if (bitnr == b->bm_bits) {
		i = -1;
	} else { /* (bitnr > b->bm_bits) */
		dev_err(DEV, "bitnr=%lu > bm_bits=%lu\n", bitnr, b->bm_bits);
		i = 0;
	}

	spin_unlock_irqrestore(&b->bm_lock, flags);
	return i;
}

/* returns number of bits set in the range [s, e] */
int drbd_bm_count_bits(struct drbd_conf *mdev, const unsigned long s, const unsigned long e)
{
	unsigned long flags;
	struct drbd_bitmap *b = mdev->bitmap;
	unsigned long *p_addr = NULL;
	unsigned long bitnr;
	unsigned int page_nr = -1U;
	int c = 0;

	/* If this is called without a bitmap, that is a bug.  But just to be
	 * robust in case we screwed up elsewhere, in that case pretend there
	 * was one dirty bit in the requested area, so we won't try to do a
	 * local read there (no bitmap probably implies no disk) */
	ERR_IF(!b) return 1;
	ERR_IF(!b->bm_pages) return 1;

	spin_lock_irqsave(&b->bm_lock, flags);
	if (BM_DONT_TEST & b->bm_flags)
		bm_print_lock_info(mdev);
	for (bitnr = s; bitnr <= e; bitnr++) {
		unsigned int idx = bm_bit_to_page_idx(b, bitnr);
		if (page_nr != idx) {
			page_nr = idx;
			if (p_addr)
				bm_unmap(p_addr);
			p_addr = bm_map_pidx(b, idx);
		}
		ERR_IF (bitnr >= b->bm_bits) {
			dev_err(DEV, "bitnr=%lu bm_bits=%lu\n", bitnr, b->bm_bits);
		} else {
			c += (0 != test_bit_le(bitnr - (page_nr << (PAGE_SHIFT+3)), p_addr));
		}
	}
	if (p_addr)
		bm_unmap(p_addr);
	spin_unlock_irqrestore(&b->bm_lock, flags);
	return c;
}


/* inherently racy...
 * return value may be already out-of-date when this function returns.
 * but the general usage is that this is only use during a cstate when bits are
 * only cleared, not set, and typically only care for the case when the return
 * value is zero, or we already "locked" this "bitmap extent" by other means.
 *
 * enr is bm-extent number, since we chose to name one sector (512 bytes)
 * worth of the bitmap a "bitmap extent".
 *
 * TODO
 * I think since we use it like a reference count, we should use the real
 * reference count of some bitmap extent element from some lru instead...
 *
 */
int drbd_bm_e_weight(struct drbd_conf *mdev, unsigned long enr)
{
	struct drbd_bitmap *b = mdev->bitmap;
	int count, s, e;
	unsigned long flags;
	unsigned long *p_addr, *bm;

	ERR_IF(!b) return 0;
	ERR_IF(!b->bm_pages) return 0;

	spin_lock_irqsave(&b->bm_lock, flags);
	if (BM_DONT_TEST & b->bm_flags)
		bm_print_lock_info(mdev);

	s = S2W(enr);
	e = min((size_t)S2W(enr+1), b->bm_words);
	count = 0;
	if (s < b->bm_words) {
		int n = e-s;
		p_addr = bm_map_pidx(b, bm_word_to_page_idx(b, s));
		bm = p_addr + MLPP(s);
		while (n--)
			count += hweight_long(*bm++);
		bm_unmap(p_addr);
	} else {
		dev_err(DEV, "start offset (%d) too large in drbd_bm_e_weight\n", s);
	}
	spin_unlock_irqrestore(&b->bm_lock, flags);
	return count;
}

/* Set all bits covered by the AL-extent al_enr.
 * Returns number of bits changed. */
unsigned long drbd_bm_ALe_set_all(struct drbd_conf *mdev, unsigned long al_enr)
{
	struct drbd_bitmap *b = mdev->bitmap;
	unsigned long *p_addr, *bm;
	unsigned long weight;
	unsigned long s, e;
	int count, i, do_now;
	ERR_IF(!b) return 0;
	ERR_IF(!b->bm_pages) return 0;

	spin_lock_irq(&b->bm_lock);
	if (BM_DONT_SET & b->bm_flags)
		bm_print_lock_info(mdev);
	weight = b->bm_set;

	s = al_enr * BM_WORDS_PER_AL_EXT;
	e = min_t(size_t, s + BM_WORDS_PER_AL_EXT, b->bm_words);
	/* assert that s and e are on the same page */
	D_ASSERT((e-1) >> (PAGE_SHIFT - LN2_BPL + 3)
	      ==  s    >> (PAGE_SHIFT - LN2_BPL + 3));
	count = 0;
	if (s < b->bm_words) {
		i = do_now = e-s;
		p_addr = bm_map_pidx(b, bm_word_to_page_idx(b, s));
		bm = p_addr + MLPP(s);
		while (i--) {
			count += hweight_long(*bm);
			*bm = -1UL;
			bm++;
		}
		bm_unmap(p_addr);
		b->bm_set += do_now*BITS_PER_LONG - count;
		if (e == b->bm_words)
			b->bm_set -= bm_clear_surplus(b);
	} else {
		dev_err(DEV, "start offset (%lu) too large in drbd_bm_ALe_set_all\n", s);
	}
	weight = b->bm_set - weight;
	spin_unlock_irq(&b->bm_lock);
	return weight;
}
