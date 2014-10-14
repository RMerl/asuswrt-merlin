/* MN10300 Cache flushing
 *
 * Copyright (C) 2007 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public Licence
 * as published by the Free Software Foundation; either version
 * 2 of the Licence, or (at your option) any later version.
 */
#ifndef _ASM_CACHEFLUSH_H
#define _ASM_CACHEFLUSH_H

#ifndef __ASSEMBLY__

/* Keep includes the same across arches.  */
#include <linux/mm.h>

/*
 * Primitive routines
 */
#ifdef CONFIG_MN10300_CACHE_ENABLED
extern void mn10300_local_icache_inv(void);
extern void mn10300_local_icache_inv_page(unsigned long start);
extern void mn10300_local_icache_inv_range(unsigned long start, unsigned long end);
extern void mn10300_local_icache_inv_range2(unsigned long start, unsigned long size);
extern void mn10300_local_dcache_inv(void);
extern void mn10300_local_dcache_inv_page(unsigned long start);
extern void mn10300_local_dcache_inv_range(unsigned long start, unsigned long end);
extern void mn10300_local_dcache_inv_range2(unsigned long start, unsigned long size);
extern void mn10300_icache_inv(void);
extern void mn10300_icache_inv_page(unsigned long start);
extern void mn10300_icache_inv_range(unsigned long start, unsigned long end);
extern void mn10300_icache_inv_range2(unsigned long start, unsigned long size);
extern void mn10300_dcache_inv(void);
extern void mn10300_dcache_inv_page(unsigned long start);
extern void mn10300_dcache_inv_range(unsigned long start, unsigned long end);
extern void mn10300_dcache_inv_range2(unsigned long start, unsigned long size);
#ifdef CONFIG_MN10300_CACHE_WBACK
extern void mn10300_local_dcache_flush(void);
extern void mn10300_local_dcache_flush_page(unsigned long start);
extern void mn10300_local_dcache_flush_range(unsigned long start, unsigned long end);
extern void mn10300_local_dcache_flush_range2(unsigned long start, unsigned long size);
extern void mn10300_local_dcache_flush_inv(void);
extern void mn10300_local_dcache_flush_inv_page(unsigned long start);
extern void mn10300_local_dcache_flush_inv_range(unsigned long start, unsigned long end);
extern void mn10300_local_dcache_flush_inv_range2(unsigned long start, unsigned long size);
extern void mn10300_dcache_flush(void);
extern void mn10300_dcache_flush_page(unsigned long start);
extern void mn10300_dcache_flush_range(unsigned long start, unsigned long end);
extern void mn10300_dcache_flush_range2(unsigned long start, unsigned long size);
extern void mn10300_dcache_flush_inv(void);
extern void mn10300_dcache_flush_inv_page(unsigned long start);
extern void mn10300_dcache_flush_inv_range(unsigned long start, unsigned long end);
extern void mn10300_dcache_flush_inv_range2(unsigned long start, unsigned long size);
#else
#define mn10300_local_dcache_flush()			do {} while (0)
#define mn10300_local_dcache_flush_page(start)		do {} while (0)
#define mn10300_local_dcache_flush_range(start, end)	do {} while (0)
#define mn10300_local_dcache_flush_range2(start, size)	do {} while (0)
#define mn10300_local_dcache_flush_inv() \
		mn10300_local_dcache_inv()
#define mn10300_local_dcache_flush_inv_page(start) \
		mn10300_local_dcache_inv_page(start)
#define mn10300_local_dcache_flush_inv_range(start, end) \
		mn10300_local_dcache_inv_range(start, end)
#define mn10300_local_dcache_flush_inv_range2(start, size) \
		mn10300_local_dcache_inv_range2(start, size)
#define mn10300_dcache_flush()				do {} while (0)
#define mn10300_dcache_flush_page(start)		do {} while (0)
#define mn10300_dcache_flush_range(start, end)		do {} while (0)
#define mn10300_dcache_flush_range2(start, size)	do {} while (0)
#define mn10300_dcache_flush_inv()			mn10300_dcache_inv()
#define mn10300_dcache_flush_inv_page(start) \
	mn10300_dcache_inv_page((start))
#define mn10300_dcache_flush_inv_range(start, end) \
	mn10300_dcache_inv_range((start), (end))
#define mn10300_dcache_flush_inv_range2(start, size) \
	mn10300_dcache_inv_range2((start), (size))
#endif /* CONFIG_MN10300_CACHE_WBACK */
#else
#define mn10300_local_icache_inv()			do {} while (0)
#define mn10300_local_icache_inv_page(start)		do {} while (0)
#define mn10300_local_icache_inv_range(start, end)	do {} while (0)
#define mn10300_local_icache_inv_range2(start, size)	do {} while (0)
#define mn10300_local_dcache_inv()			do {} while (0)
#define mn10300_local_dcache_inv_page(start)		do {} while (0)
#define mn10300_local_dcache_inv_range(start, end)	do {} while (0)
#define mn10300_local_dcache_inv_range2(start, size)	do {} while (0)
#define mn10300_local_dcache_flush()			do {} while (0)
#define mn10300_local_dcache_flush_inv_page(start)	do {} while (0)
#define mn10300_local_dcache_flush_inv()		do {} while (0)
#define mn10300_local_dcache_flush_inv_range(start, end)do {} while (0)
#define mn10300_local_dcache_flush_inv_range2(start, size) do {} while (0)
#define mn10300_local_dcache_flush_page(start)		do {} while (0)
#define mn10300_local_dcache_flush_range(start, end)	do {} while (0)
#define mn10300_local_dcache_flush_range2(start, size)	do {} while (0)
#define mn10300_icache_inv()				do {} while (0)
#define mn10300_icache_inv_page(start)			do {} while (0)
#define mn10300_icache_inv_range(start, end)		do {} while (0)
#define mn10300_icache_inv_range2(start, size)		do {} while (0)
#define mn10300_dcache_inv()				do {} while (0)
#define mn10300_dcache_inv_page(start)			do {} while (0)
#define mn10300_dcache_inv_range(start, end)		do {} while (0)
#define mn10300_dcache_inv_range2(start, size)		do {} while (0)
#define mn10300_dcache_flush()				do {} while (0)
#define mn10300_dcache_flush_inv_page(start)		do {} while (0)
#define mn10300_dcache_flush_inv()			do {} while (0)
#define mn10300_dcache_flush_inv_range(start, end)	do {} while (0)
#define mn10300_dcache_flush_inv_range2(start, size)	do {} while (0)
#define mn10300_dcache_flush_page(start)		do {} while (0)
#define mn10300_dcache_flush_range(start, end)		do {} while (0)
#define mn10300_dcache_flush_range2(start, size)	do {} while (0)
#endif /* CONFIG_MN10300_CACHE_ENABLED */

/*
 * Virtually-indexed cache management (our cache is physically indexed)
 */
#define flush_cache_all()			do {} while (0)
#define flush_cache_mm(mm)			do {} while (0)
#define flush_cache_dup_mm(mm)			do {} while (0)
#define flush_cache_range(mm, start, end)	do {} while (0)
#define flush_cache_page(vma, vmaddr, pfn)	do {} while (0)
#define flush_cache_vmap(start, end)		do {} while (0)
#define flush_cache_vunmap(start, end)		do {} while (0)
#define ARCH_IMPLEMENTS_FLUSH_DCACHE_PAGE 0
#define flush_dcache_page(page)			do {} while (0)
#define flush_dcache_mmap_lock(mapping)		do {} while (0)
#define flush_dcache_mmap_unlock(mapping)	do {} while (0)

/*
 * Physically-indexed cache management
 */
#if defined(CONFIG_MN10300_CACHE_FLUSH_ICACHE)
extern void flush_icache_page(struct vm_area_struct *vma, struct page *page);
extern void flush_icache_range(unsigned long start, unsigned long end);
#elif defined(CONFIG_MN10300_CACHE_INV_ICACHE)
static inline void flush_icache_page(struct vm_area_struct *vma,
				     struct page *page)
{
	mn10300_icache_inv_page(page_to_phys(page));
}
extern void flush_icache_range(unsigned long start, unsigned long end);
#else
#define flush_icache_range(start, end)		do {} while (0)
#define flush_icache_page(vma, pg)		do {} while (0)
#endif


#define flush_icache_user_range(vma, pg, adr, len) \
	flush_icache_range(adr, adr + len)

#define copy_to_user_page(vma, page, vaddr, dst, src, len) \
	do {					\
		memcpy(dst, src, len);		\
		flush_icache_page(vma, page);	\
	} while (0)

#define copy_from_user_page(vma, page, vaddr, dst, src, len) \
	memcpy(dst, src, len)

/*
 * Internal debugging function
 */
#ifdef CONFIG_DEBUG_PAGEALLOC
extern void kernel_map_pages(struct page *page, int numpages, int enable);
#endif

#endif /* __ASSEMBLY__ */

#endif /* _ASM_CACHEFLUSH_H */
