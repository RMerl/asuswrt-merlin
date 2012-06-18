/* internal.h: mm/ internal definitions
 *
 * Copyright (C) 2004 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */
#ifndef __MM_INTERNAL_H
#define __MM_INTERNAL_H

#include <linux/mm.h>

static inline void set_page_count(struct page *page, int v)
{
	atomic_set(&page->_count, v);
}

/*
 * Turn a non-refcounted page (->_count == 0) into refcounted with
 * a count of one.
 */
static inline void set_page_refcounted(struct page *page)
{
	VM_BUG_ON(PageCompound(page) && PageTail(page));
	VM_BUG_ON(atomic_read(&page->_count));
	set_page_count(page, 1);
}

static inline void __put_page(struct page *page)
{
	atomic_dec(&page->_count);
}

extern void fastcall __init __free_pages_bootmem(struct page *page,
						unsigned int order);

/* Memory initialisation debug and verification */
enum mminit_level {
	MMINIT_WARNING,
	MMINIT_VERIFY,
	MMINIT_TRACE
};

#ifdef CONFIG_DEBUG_MEMORY_INIT

extern int mminit_loglevel;

#define mminit_dprintk(level, prefix, fmt, arg...) \
do { \
	if (level < mminit_loglevel) { \
		printk(level <= MMINIT_WARNING ? KERN_WARNING : KERN_DEBUG); \
		printk(KERN_CONT "mminit::" prefix " " fmt, ##arg); \
	} \
} while (0)

#else

static inline void mminit_dprintk(enum mminit_level level,
				const char *prefix, const char *fmt, ...)
{
}
#endif /* CONFIG_DEBUG_MEMORY_INIT */

/* mminit_validate_memmodel_limits is independent of CONFIG_DEBUG_MEMORY_INIT */
#if defined(CONFIG_SPARSEMEM)
extern void mminit_validate_memmodel_limits(unsigned long *start_pfn,
				unsigned long *end_pfn);
#else
static inline void mminit_validate_memmodel_limits(unsigned long *start_pfn,
				unsigned long *end_pfn)
{
}
#endif /* CONFIG_SPARSEMEM */

#endif
