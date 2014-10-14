/* i915_mem.c -- Simple agp/fb memory manager for i915 -*- linux-c -*-
 */
/*
 * Copyright 2003 Tungsten Graphics, Inc., Cedar Park, Texas.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL TUNGSTEN GRAPHICS AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "drmP.h"
#include "drm.h"
#include "i915_drm.h"
#include "i915_drv.h"

/* This memory manager is integrated into the global/local lru
 * mechanisms used by the clients.  Specifically, it operates by
 * setting the 'in_use' fields of the global LRU to indicate whether
 * this region is privately allocated to a client.
 *
 * This does require the client to actually respect that field.
 *
 * Currently no effort is made to allocate 'private' memory in any
 * clever way - the LRU information isn't used to determine which
 * block to allocate, and the ring is drained prior to allocations --
 * in other words allocation is expensive.
 */
static void mark_block(drm_device_t * dev, struct mem_block *p, int in_use)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	drm_i915_sarea_t *sarea_priv = dev_priv->sarea_priv;
	drm_tex_region_t *list;
	unsigned shift, nr;
	unsigned start;
	unsigned end;
	unsigned i;
	int age;

	shift = dev_priv->tex_lru_log_granularity;
	nr = I915_NR_TEX_REGIONS;

	start = p->start >> shift;
	end = (p->start + p->size - 1) >> shift;

	age = ++sarea_priv->texAge;
	list = sarea_priv->texList;

	/* Mark the regions with the new flag and update their age.  Move
	 * them to head of list to preserve LRU semantics.
	 */
	for (i = start; i <= end; i++) {
		list[i].in_use = in_use;
		list[i].age = age;

		/* remove_from_list(i)
		 */
		list[(unsigned)list[i].next].prev = list[i].prev;
		list[(unsigned)list[i].prev].next = list[i].next;

		/* insert_at_head(list, i)
		 */
		list[i].prev = nr;
		list[i].next = list[nr].next;
		list[(unsigned)list[nr].next].prev = i;
		list[nr].next = i;
	}
}

/* Very simple allocator for agp memory, working on a static range
 * already mapped into each client's address space.
 */

static struct mem_block *split_block(struct mem_block *p, int start, int size,
				     DRMFILE filp)
{
	/* Maybe cut off the start of an existing block */
	if (start > p->start) {
		struct mem_block *newblock =
		    drm_alloc(sizeof(*newblock), DRM_MEM_BUFLISTS);
		if (!newblock)
			goto out;
		newblock->start = start;
		newblock->size = p->size - (start - p->start);
		newblock->filp = NULL;
		newblock->next = p->next;
		newblock->prev = p;
		p->next->prev = newblock;
		p->next = newblock;
		p->size -= newblock->size;
		p = newblock;
	}

	/* Maybe cut off the end of an existing block */
	if (size < p->size) {
		struct mem_block *newblock =
		    drm_alloc(sizeof(*newblock), DRM_MEM_BUFLISTS);
		if (!newblock)
			goto out;
		newblock->start = start + size;
		newblock->size = p->size - size;
		newblock->filp = NULL;
		newblock->next = p->next;
		newblock->prev = p;
		p->next->prev = newblock;
		p->next = newblock;
		p->size = size;
	}

      out:
	/* Our block is in the middle */
	p->filp = filp;
	return p;
}

static struct mem_block *alloc_block(struct mem_block *heap, int size,
				     int align2, DRMFILE filp)
{
	struct mem_block *p;
	int mask = (1 << align2) - 1;

	for (p = heap->next; p != heap; p = p->next) {
		int start = (p->start + mask) & ~mask;
		if (p->filp == NULL && start + size <= p->start + p->size)
			return split_block(p, start, size, filp);
	}

	return NULL;
}

static struct mem_block *find_block(struct mem_block *heap, int start)
{
	struct mem_block *p;

	for (p = heap->next; p != heap; p = p->next)
		if (p->start == start)
			return p;

	return NULL;
}

static void free_block(struct mem_block *p)
{
	p->filp = NULL;

	/* Assumes a single contiguous range.  Needs a special filp in
	 * 'heap' to stop it being subsumed.
	 */
	if (p->next->filp == NULL) {
		struct mem_block *q = p->next;
		p->size += q->size;
		p->next = q->next;
		p->next->prev = p;
		drm_free(q, sizeof(*q), DRM_MEM_BUFLISTS);
	}

	if (p->prev->filp == NULL) {
		struct mem_block *q = p->prev;
		q->size += p->size;
		q->next = p->next;
		q->next->prev = q;
		drm_free(p, sizeof(*q), DRM_MEM_BUFLISTS);
	}
}

/* Initialize.  How to check for an uninitialized heap?
 */
static int init_heap(struct mem_block **heap, int start, int size)
{
	struct mem_block *blocks = drm_alloc(sizeof(*blocks), DRM_MEM_BUFLISTS);

	if (!blocks)
		return -ENOMEM;

	*heap = drm_alloc(sizeof(**heap), DRM_MEM_BUFLISTS);
	if (!*heap) {
		drm_free(blocks, sizeof(*blocks), DRM_MEM_BUFLISTS);
		return -ENOMEM;
	}

	blocks->start = start;
	blocks->size = size;
	blocks->filp = NULL;
	blocks->next = blocks->prev = *heap;

	memset(*heap, 0, sizeof(**heap));
	(*heap)->filp = (DRMFILE) - 1;
	(*heap)->next = (*heap)->prev = blocks;
	return 0;
}

/* Free all blocks associated with the releasing file.
 */
void i915_mem_release(drm_device_t * dev, DRMFILE filp, struct mem_block *heap)
{
	struct mem_block *p;

	if (!heap || !heap->next)
		return;

	for (p = heap->next; p != heap; p = p->next) {
		if (p->filp == filp) {
			p->filp = NULL;
			mark_block(dev, p, 0);
		}
	}

	/* Assumes a single contiguous range.  Needs a special filp in
	 * 'heap' to stop it being subsumed.
	 */
	for (p = heap->next; p != heap; p = p->next) {
		while (p->filp == NULL && p->next->filp == NULL) {
			struct mem_block *q = p->next;
			p->size += q->size;
			p->next = q->next;
			p->next->prev = p;
			drm_free(q, sizeof(*q), DRM_MEM_BUFLISTS);
		}
	}
}

/* Shutdown.
 */
void i915_mem_takedown(struct mem_block **heap)
{
	struct mem_block *p;

	if (!*heap)
		return;

	for (p = (*heap)->next; p != *heap;) {
		struct mem_block *q = p;
		p = p->next;
		drm_free(q, sizeof(*q), DRM_MEM_BUFLISTS);
	}

	drm_free(*heap, sizeof(**heap), DRM_MEM_BUFLISTS);
	*heap = NULL;
}

static struct mem_block **get_heap(drm_i915_private_t * dev_priv, int region)
{
	switch (region) {
	case I915_MEM_REGION_AGP:
		return &dev_priv->agp_heap;
	default:
		return NULL;
	}
}

/* IOCTL HANDLERS */

int i915_mem_alloc(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	drm_i915_private_t *dev_priv = dev->dev_private;
	drm_i915_mem_alloc_t alloc;
	struct mem_block *block, **heap;

	if (!dev_priv) {
		DRM_ERROR("%s called with no initialization\n", __FUNCTION__);
		return DRM_ERR(EINVAL);
	}

	DRM_COPY_FROM_USER_IOCTL(alloc, (drm_i915_mem_alloc_t __user *) data,
				 sizeof(alloc));

	heap = get_heap(dev_priv, alloc.region);
	if (!heap || !*heap)
		return DRM_ERR(EFAULT);

	/* Make things easier on ourselves: all allocations at least
	 * 4k aligned.
	 */
	if (alloc.alignment < 12)
		alloc.alignment = 12;

	block = alloc_block(*heap, alloc.size, alloc.alignment, filp);

	if (!block)
		return DRM_ERR(ENOMEM);

	mark_block(dev, block, 1);

	if (DRM_COPY_TO_USER(alloc.region_offset, &block->start, sizeof(int))) {
		DRM_ERROR("copy_to_user\n");
		return DRM_ERR(EFAULT);
	}

	return 0;
}

int i915_mem_free(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	drm_i915_private_t *dev_priv = dev->dev_private;
	drm_i915_mem_free_t memfree;
	struct mem_block *block, **heap;

	if (!dev_priv) {
		DRM_ERROR("%s called with no initialization\n", __FUNCTION__);
		return DRM_ERR(EINVAL);
	}

	DRM_COPY_FROM_USER_IOCTL(memfree, (drm_i915_mem_free_t __user *) data,
				 sizeof(memfree));

	heap = get_heap(dev_priv, memfree.region);
	if (!heap || !*heap)
		return DRM_ERR(EFAULT);

	block = find_block(*heap, memfree.region_offset);
	if (!block)
		return DRM_ERR(EFAULT);

	if (block->filp != filp)
		return DRM_ERR(EPERM);

	mark_block(dev, block, 0);
	free_block(block);
	return 0;
}

int i915_mem_init_heap(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	drm_i915_private_t *dev_priv = dev->dev_private;
	drm_i915_mem_init_heap_t initheap;
	struct mem_block **heap;

	if (!dev_priv) {
		DRM_ERROR("%s called with no initialization\n", __FUNCTION__);
		return DRM_ERR(EINVAL);
	}

	DRM_COPY_FROM_USER_IOCTL(initheap,
				 (drm_i915_mem_init_heap_t __user *) data,
				 sizeof(initheap));

	heap = get_heap(dev_priv, initheap.region);
	if (!heap)
		return DRM_ERR(EFAULT);

	if (*heap) {
		DRM_ERROR("heap already initialized?");
		return DRM_ERR(EFAULT);
	}

	return init_heap(heap, initheap.start, initheap.size);
}

int i915_mem_destroy_heap( DRM_IOCTL_ARGS )
{
	DRM_DEVICE;
	drm_i915_private_t *dev_priv = dev->dev_private;
	drm_i915_mem_destroy_heap_t destroyheap;
	struct mem_block **heap;

	if ( !dev_priv ) {
		DRM_ERROR( "%s called with no initialization\n", __FUNCTION__ );
		return DRM_ERR(EINVAL);
	}

	DRM_COPY_FROM_USER_IOCTL( destroyheap, (drm_i915_mem_destroy_heap_t *)data,
				  sizeof(destroyheap) );

	heap = get_heap( dev_priv, destroyheap.region );
	if (!heap) {
		DRM_ERROR("get_heap failed");
		return DRM_ERR(EFAULT);
	}
	
	if (!*heap) {
		DRM_ERROR("heap not initialized?");
		return DRM_ERR(EFAULT);
	}

	i915_mem_takedown( heap );
	return 0;
}

