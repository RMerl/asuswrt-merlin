/**
 * \file drm_scatter.c
 * IOCTLs to manage scatter/gather memory
 *
 * \author Gareth Hughes <gareth@valinux.com>
 */

/*
 * Created: Mon Dec 18 23:20:54 2000 by gareth@valinux.com
 *
 * Copyright 2000 VA Linux Systems, Inc., Sunnyvale, California.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <linux/vmalloc.h>
#include <linux/slab.h>
#include "drmP.h"

#define DEBUG_SCATTER 0

static inline void *drm_vmalloc_dma(unsigned long size)
{
#if defined(__powerpc__) && defined(CONFIG_NOT_COHERENT_CACHE)
	return __vmalloc(size, GFP_KERNEL, PAGE_KERNEL | _PAGE_NO_CACHE);
#else
	return vmalloc_32(size);
#endif
}

void drm_sg_cleanup(struct drm_sg_mem * entry)
{
	struct page *page;
	int i;

	for (i = 0; i < entry->pages; i++) {
		page = entry->pagelist[i];
		if (page)
			ClearPageReserved(page);
	}

	vfree(entry->virtual);

	kfree(entry->busaddr);
	kfree(entry->pagelist);
	kfree(entry);
}

#ifdef _LP64
# define ScatterHandle(x) (unsigned int)((x >> 32) + (x & ((1L << 32) - 1)))
#else
# define ScatterHandle(x) (unsigned int)(x)
#endif

int drm_sg_alloc(struct drm_device *dev, struct drm_scatter_gather * request)
{
	struct drm_sg_mem *entry;
	unsigned long pages, i, j;

	DRM_DEBUG("\n");

	if (!drm_core_check_feature(dev, DRIVER_SG))
		return -EINVAL;

	if (dev->sg)
		return -EINVAL;

	entry = kmalloc(sizeof(*entry), GFP_KERNEL);
	if (!entry)
		return -ENOMEM;

	memset(entry, 0, sizeof(*entry));
	pages = (request->size + PAGE_SIZE - 1) / PAGE_SIZE;
	DRM_DEBUG("size=%ld pages=%ld\n", request->size, pages);

	entry->pages = pages;
	entry->pagelist = kmalloc(pages * sizeof(*entry->pagelist), GFP_KERNEL);
	if (!entry->pagelist) {
		kfree(entry);
		return -ENOMEM;
	}

	memset(entry->pagelist, 0, pages * sizeof(*entry->pagelist));

	entry->busaddr = kmalloc(pages * sizeof(*entry->busaddr), GFP_KERNEL);
	if (!entry->busaddr) {
		kfree(entry->pagelist);
		kfree(entry);
		return -ENOMEM;
	}
	memset((void *)entry->busaddr, 0, pages * sizeof(*entry->busaddr));

	entry->virtual = drm_vmalloc_dma(pages << PAGE_SHIFT);
	if (!entry->virtual) {
		kfree(entry->busaddr);
		kfree(entry->pagelist);
		kfree(entry);
		return -ENOMEM;
	}

	/* This also forces the mapping of COW pages, so our page list
	 * will be valid.  Please don't remove it...
	 */
	memset(entry->virtual, 0, pages << PAGE_SHIFT);

	entry->handle = ScatterHandle((unsigned long)entry->virtual);

	DRM_DEBUG("handle  = %08lx\n", entry->handle);
	DRM_DEBUG("virtual = %p\n", entry->virtual);

	for (i = (unsigned long)entry->virtual, j = 0; j < pages;
	     i += PAGE_SIZE, j++) {
		entry->pagelist[j] = vmalloc_to_page((void *)i);
		if (!entry->pagelist[j])
			goto failed;
		SetPageReserved(entry->pagelist[j]);
	}

	request->handle = entry->handle;

	dev->sg = entry;

#if DEBUG_SCATTER
	/* Verify that each page points to its virtual address, and vice
	 * versa.
	 */
	{
		int error = 0;

		for (i = 0; i < pages; i++) {
			unsigned long *tmp;

			tmp = page_address(entry->pagelist[i]);
			for (j = 0;
			     j < PAGE_SIZE / sizeof(unsigned long);
			     j++, tmp++) {
				*tmp = 0xcafebabe;
			}
			tmp = (unsigned long *)((u8 *) entry->virtual +
						(PAGE_SIZE * i));
			for (j = 0;
			     j < PAGE_SIZE / sizeof(unsigned long);
			     j++, tmp++) {
				if (*tmp != 0xcafebabe && error == 0) {
					error = 1;
					DRM_ERROR("Scatter allocation error, "
						  "pagelist does not match "
						  "virtual mapping\n");
				}
			}
			tmp = page_address(entry->pagelist[i]);
			for (j = 0;
			     j < PAGE_SIZE / sizeof(unsigned long);
			     j++, tmp++) {
				*tmp = 0;
			}
		}
		if (error == 0)
			DRM_ERROR("Scatter allocation matches pagelist\n");
	}
#endif

	return 0;

      failed:
	drm_sg_cleanup(entry);
	return -ENOMEM;
}
EXPORT_SYMBOL(drm_sg_alloc);


int drm_sg_alloc_ioctl(struct drm_device *dev, void *data,
		       struct drm_file *file_priv)
{
	struct drm_scatter_gather *request = data;

	return drm_sg_alloc(dev, request);

}

int drm_sg_free(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	struct drm_scatter_gather *request = data;
	struct drm_sg_mem *entry;

	if (!drm_core_check_feature(dev, DRIVER_SG))
		return -EINVAL;

	entry = dev->sg;
	dev->sg = NULL;

	if (!entry || entry->handle != request->handle)
		return -EINVAL;

	DRM_DEBUG("virtual  = %p\n", entry->virtual);

	drm_sg_cleanup(entry);

	return 0;
}
