/*
 * Copyright (C) 2004-2006 Atmel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/highmem.h>
#include <linux/unistd.h>

#include <asm/cacheflush.h>
#include <asm/cachectl.h>
#include <asm/processor.h>
#include <asm/uaccess.h>
#include <asm/syscalls.h>

/*
 * If you attempt to flush anything more than this, you need superuser
 * privileges.  The value is completely arbitrary.
 */
#define CACHEFLUSH_MAX_LEN	1024

void invalidate_dcache_region(void *start, size_t size)
{
	unsigned long v, begin, end, linesz, mask;

	linesz = boot_cpu_data.dcache.linesz;
	mask = linesz - 1;

	/* when first and/or last cachelines are shared, flush them
	 * instead of invalidating ... never discard valid data!
	 */
	begin = (unsigned long)start;
	end = begin + size;

	if (begin & mask) {
		flush_dcache_line(start);
		begin += linesz;
	}
	if (end & mask) {
		flush_dcache_line((void *)end);
		end &= ~mask;
	}

	/* remaining cachelines only need invalidation */
	for (v = begin; v < end; v += linesz)
		invalidate_dcache_line((void *)v);
	flush_write_buffer();
}

void clean_dcache_region(void *start, size_t size)
{
	unsigned long v, begin, end, linesz;

	linesz = boot_cpu_data.dcache.linesz;
	begin = (unsigned long)start & ~(linesz - 1);
	end = ((unsigned long)start + size + linesz - 1) & ~(linesz - 1);

	for (v = begin; v < end; v += linesz)
		clean_dcache_line((void *)v);
	flush_write_buffer();
}

void flush_dcache_region(void *start, size_t size)
{
	unsigned long v, begin, end, linesz;

	linesz = boot_cpu_data.dcache.linesz;
	begin = (unsigned long)start & ~(linesz - 1);
	end = ((unsigned long)start + size + linesz - 1) & ~(linesz - 1);

	for (v = begin; v < end; v += linesz)
		flush_dcache_line((void *)v);
	flush_write_buffer();
}

void invalidate_icache_region(void *start, size_t size)
{
	unsigned long v, begin, end, linesz;

	linesz = boot_cpu_data.icache.linesz;
	begin = (unsigned long)start & ~(linesz - 1);
	end = ((unsigned long)start + size + linesz - 1) & ~(linesz - 1);

	for (v = begin; v < end; v += linesz)
		invalidate_icache_line((void *)v);
}

static inline void __flush_icache_range(unsigned long start, unsigned long end)
{
	unsigned long v, linesz;

	linesz = boot_cpu_data.dcache.linesz;
	for (v = start; v < end; v += linesz) {
		clean_dcache_line((void *)v);
		invalidate_icache_line((void *)v);
	}

	flush_write_buffer();
}

/*
 * This one is called after a module has been loaded.
 */
void flush_icache_range(unsigned long start, unsigned long end)
{
	unsigned long linesz;

	linesz = boot_cpu_data.dcache.linesz;
	__flush_icache_range(start & ~(linesz - 1),
			     (end + linesz - 1) & ~(linesz - 1));
}

/*
 * This one is called from __do_fault() and do_swap_page().
 */
void flush_icache_page(struct vm_area_struct *vma, struct page *page)
{
	if (vma->vm_flags & VM_EXEC) {
		void *v = page_address(page);
		__flush_icache_range((unsigned long)v, (unsigned long)v + PAGE_SIZE);
	}
}

asmlinkage int sys_cacheflush(int operation, void __user *addr, size_t len)
{
	int ret;

	if (len > CACHEFLUSH_MAX_LEN) {
		ret = -EPERM;
		if (!capable(CAP_SYS_ADMIN))
			goto out;
	}

	ret = -EFAULT;
	if (!access_ok(VERIFY_WRITE, addr, len))
		goto out;

	switch (operation) {
	case CACHE_IFLUSH:
		flush_icache_range((unsigned long)addr,
				   (unsigned long)addr + len);
		ret = 0;
		break;
	default:
		ret = -EINVAL;
	}

out:
	return ret;
}

void copy_to_user_page(struct vm_area_struct *vma, struct page *page,
		unsigned long vaddr, void *dst, const void *src,
		unsigned long len)
{
	memcpy(dst, src, len);
	if (vma->vm_flags & VM_EXEC)
		flush_icache_range((unsigned long)dst,
				(unsigned long)dst + len);
}
