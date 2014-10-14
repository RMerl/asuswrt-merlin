/**
 * \file drm_bufs.c
 * Generic buffer template
 *
 * \author Rickard E. (Rik) Faith <faith@valinux.com>
 * \author Gareth Hughes <gareth@valinux.com>
 */

/*
 * Created: Thu Nov 23 03:10:50 2000 by gareth@valinux.com
 *
 * Copyright 1999, 2000 Precision Insight, Inc., Cedar Park, Texas.
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
 * VA LINUX SYSTEMS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include <linux/vmalloc.h>
#include "drmP.h"

unsigned long drm_get_resource_start(drm_device_t *dev, unsigned int resource)
{
	return pci_resource_start(dev->pdev, resource);
}
EXPORT_SYMBOL(drm_get_resource_start);

unsigned long drm_get_resource_len(drm_device_t *dev, unsigned int resource)
{
	return pci_resource_len(dev->pdev, resource);
}

EXPORT_SYMBOL(drm_get_resource_len);

static drm_map_list_t *drm_find_matching_map(drm_device_t *dev,
					     drm_local_map_t *map)
{
	struct list_head *list;

	list_for_each(list, &dev->maplist->head) {
		drm_map_list_t *entry = list_entry(list, drm_map_list_t, head);
		if (entry->map && map->type == entry->map->type &&
		    ((entry->map->offset == map->offset) ||
		     (map->type == _DRM_SHM && map->flags==_DRM_CONTAINS_LOCK))) {
			return entry;
		}
	}

	return NULL;
}

static int drm_map_handle(drm_device_t *dev, drm_hash_item_t *hash,
			  unsigned long user_token, int hashed_handle)
{
	int use_hashed_handle;
#if (BITS_PER_LONG == 64)
	use_hashed_handle = ((user_token & 0xFFFFFFFF00000000UL) || hashed_handle);
#elif (BITS_PER_LONG == 32)
	use_hashed_handle = hashed_handle;
#else
#error Unsupported long size. Neither 64 nor 32 bits.
#endif

	if (!use_hashed_handle) {
		int ret;
		hash->key = user_token >> PAGE_SHIFT;
		ret = drm_ht_insert_item(&dev->map_hash, hash);
		if (ret != -EINVAL)
			return ret;
	}
	return drm_ht_just_insert_please(&dev->map_hash, hash,
					 user_token, 32 - PAGE_SHIFT - 3,
					 0, DRM_MAP_HASH_OFFSET >> PAGE_SHIFT);
}

/**
 * Ioctl to specify a range of memory that is available for mapping by a non-root process.
 *
 * \param inode device inode.
 * \param filp file pointer.
 * \param cmd command.
 * \param arg pointer to a drm_map structure.
 * \return zero on success or a negative value on error.
 *
 * Adjusts the memory offset to its absolute value according to the mapping
 * type.  Adds the map to the map list drm_device::maplist. Adds MTRR's where
 * applicable and if supported by the kernel.
 */
static int drm_addmap_core(drm_device_t * dev, unsigned int offset,
			   unsigned int size, drm_map_type_t type,
			   drm_map_flags_t flags, drm_map_list_t ** maplist)
{
	drm_map_t *map;
	drm_map_list_t *list;
	drm_dma_handle_t *dmah;
	unsigned long user_token;
	int ret;

	map = drm_alloc(sizeof(*map), DRM_MEM_MAPS);
	if (!map)
		return -ENOMEM;

	map->offset = offset;
	map->size = size;
	map->flags = flags;
	map->type = type;

	/* Only allow shared memory to be removable since we only keep enough
	 * book keeping information about shared memory to allow for removal
	 * when processes fork.
	 */
	if ((map->flags & _DRM_REMOVABLE) && map->type != _DRM_SHM) {
		drm_free(map, sizeof(*map), DRM_MEM_MAPS);
		return -EINVAL;
	}
	DRM_DEBUG("offset = 0x%08lx, size = 0x%08lx, type = %d\n",
		  map->offset, map->size, map->type);
	if ((map->offset & (~PAGE_MASK)) || (map->size & (~PAGE_MASK))) {
		drm_free(map, sizeof(*map), DRM_MEM_MAPS);
		return -EINVAL;
	}
	map->mtrr = -1;
	map->handle = NULL;

	switch (map->type) {
	case _DRM_REGISTERS:
	case _DRM_FRAME_BUFFER:
#if !defined(__sparc__) && !defined(__alpha__) && !defined(__ia64__) && !defined(__powerpc64__) && !defined(__x86_64__)
		if (map->offset + (map->size-1) < map->offset ||
		    map->offset < virt_to_phys(high_memory)) {
			drm_free(map, sizeof(*map), DRM_MEM_MAPS);
			return -EINVAL;
		}
#endif
#ifdef __alpha__
		map->offset += dev->hose->mem_space->start;
#endif
		/* Some drivers preinitialize some maps, without the X Server
		 * needing to be aware of it.  Therefore, we just return success
		 * when the server tries to create a duplicate map.
		 */
		list = drm_find_matching_map(dev, map);
		if (list != NULL) {
			if (list->map->size != map->size) {
				DRM_DEBUG("Matching maps of type %d with "
					  "mismatched sizes, (%ld vs %ld)\n",
					  map->type, map->size,
					  list->map->size);
				list->map->size = map->size;
			}

			drm_free(map, sizeof(*map), DRM_MEM_MAPS);
			*maplist = list;
			return 0;
		}

		if (drm_core_has_MTRR(dev)) {
			if (map->type == _DRM_FRAME_BUFFER ||
			    (map->flags & _DRM_WRITE_COMBINING)) {
				map->mtrr = mtrr_add(map->offset, map->size,
						     MTRR_TYPE_WRCOMB, 1);
			}
		}
		if (map->type == _DRM_REGISTERS)
			map->handle = ioremap(map->offset, map->size);
		break;
	case _DRM_SHM:
		list = drm_find_matching_map(dev, map);
		if (list != NULL) {
			if(list->map->size != map->size) {
				DRM_DEBUG("Matching maps of type %d with "
					  "mismatched sizes, (%ld vs %ld)\n",
					  map->type, map->size, list->map->size);
				list->map->size = map->size;
			}

			drm_free(map, sizeof(*map), DRM_MEM_MAPS);
			*maplist = list;
			return 0;
		}
		map->handle = vmalloc_user(map->size);
		DRM_DEBUG("%lu %d %p\n",
			  map->size, drm_order(map->size), map->handle);
		if (!map->handle) {
			drm_free(map, sizeof(*map), DRM_MEM_MAPS);
			return -ENOMEM;
		}
		map->offset = (unsigned long)map->handle;
		if (map->flags & _DRM_CONTAINS_LOCK) {
			/* Prevent a 2nd X Server from creating a 2nd lock */
			if (dev->lock.hw_lock != NULL) {
				vfree(map->handle);
				drm_free(map, sizeof(*map), DRM_MEM_MAPS);
				return -EBUSY;
			}
			dev->sigdata.lock = dev->lock.hw_lock = map->handle;	/* Pointer to lock */
		}
		break;
	case _DRM_AGP: {
		drm_agp_mem_t *entry;
		int valid = 0;

		if (!drm_core_has_AGP(dev)) {
			drm_free(map, sizeof(*map), DRM_MEM_MAPS);
			return -EINVAL;
		}
#ifdef __alpha__
		map->offset += dev->hose->mem_space->start;
#endif
		/* Note: dev->agp->base may actually be 0 when the DRM
		 * is not in control of AGP space. But if user space is
		 * it should already have added the AGP base itself.
		 */
		map->offset += dev->agp->base;
		map->mtrr = dev->agp->agp_mtrr;	/* for getmap */

		/* This assumes the DRM is in total control of AGP space.
		 * It's not always the case as AGP can be in the control
		 * of user space (i.e. i810 driver). So this loop will get
		 * skipped and we double check that dev->agp->memory is
		 * actually set as well as being invalid before EPERM'ing
		 */
		for (entry = dev->agp->memory; entry; entry = entry->next) {
			if ((map->offset >= entry->bound) &&
			    (map->offset + map->size <= entry->bound + entry->pages * PAGE_SIZE)) {
				valid = 1;
				break;
			}
		}
		if (dev->agp->memory && !valid) {
			drm_free(map, sizeof(*map), DRM_MEM_MAPS);
			return -EPERM;
		}
		DRM_DEBUG("AGP offset = 0x%08lx, size = 0x%08lx\n", map->offset, map->size);

		break;
	}
	case _DRM_SCATTER_GATHER:
		if (!dev->sg) {
			drm_free(map, sizeof(*map), DRM_MEM_MAPS);
			return -EINVAL;
		}
		map->offset += (unsigned long)dev->sg->virtual;
		break;
	case _DRM_CONSISTENT:
		/* dma_addr_t is 64bit on i386 with CONFIG_HIGHMEM64G,
		 * As we're limiting the address to 2^32-1 (or less),
		 * casting it down to 32 bits is no problem, but we
		 * need to point to a 64bit variable first. */
		dmah = drm_pci_alloc(dev, map->size, map->size, 0xffffffffUL);
		if (!dmah) {
			drm_free(map, sizeof(*map), DRM_MEM_MAPS);
			return -ENOMEM;
		}
		map->handle = dmah->vaddr;
		map->offset = (unsigned long)dmah->busaddr;
		kfree(dmah);
		break;
	default:
		drm_free(map, sizeof(*map), DRM_MEM_MAPS);
		return -EINVAL;
	}

	list = drm_alloc(sizeof(*list), DRM_MEM_MAPS);
	if (!list) {
		if (map->type == _DRM_REGISTERS)
			iounmap(map->handle);
		drm_free(map, sizeof(*map), DRM_MEM_MAPS);
		return -EINVAL;
	}
	memset(list, 0, sizeof(*list));
	list->map = map;

	mutex_lock(&dev->struct_mutex);
	list_add(&list->head, &dev->maplist->head);

	/* Assign a 32-bit handle */
	/* We do it here so that dev->struct_mutex protects the increment */
	user_token = (map->type == _DRM_SHM) ? (unsigned long)map->handle :
		map->offset;
	ret = drm_map_handle(dev, &list->hash, user_token, 0);
	if (ret) {
		if (map->type == _DRM_REGISTERS)
			iounmap(map->handle);
		drm_free(map, sizeof(*map), DRM_MEM_MAPS);
		drm_free(list, sizeof(*list), DRM_MEM_MAPS);
		mutex_unlock(&dev->struct_mutex);
		return ret;
	}

	list->user_token = list->hash.key << PAGE_SHIFT;
	mutex_unlock(&dev->struct_mutex);

	*maplist = list;
	return 0;
	}

int drm_addmap(drm_device_t * dev, unsigned int offset,
	       unsigned int size, drm_map_type_t type,
	       drm_map_flags_t flags, drm_local_map_t ** map_ptr)
{
	drm_map_list_t *list;
	int rc;

	rc = drm_addmap_core(dev, offset, size, type, flags, &list);
	if (!rc)
		*map_ptr = list->map;
	return rc;
}

EXPORT_SYMBOL(drm_addmap);

int drm_addmap_ioctl(struct inode *inode, struct file *filp,
		     unsigned int cmd, unsigned long arg)
{
	drm_file_t *priv = filp->private_data;
	drm_device_t *dev = priv->head->dev;
	drm_map_t map;
	drm_map_list_t *maplist;
	drm_map_t __user *argp = (void __user *)arg;
	int err;

	if (!(filp->f_mode & 3))
		return -EACCES;	/* Require read/write */

	if (copy_from_user(&map, argp, sizeof(map))) {
		return -EFAULT;
	}

	if (!(capable(CAP_SYS_ADMIN) || map.type == _DRM_AGP))
		return -EPERM;

	err = drm_addmap_core(dev, map.offset, map.size, map.type, map.flags,
			      &maplist);

	if (err)
		return err;

	if (copy_to_user(argp, maplist->map, sizeof(drm_map_t)))
		return -EFAULT;

	/* avoid a warning on 64-bit, this casting isn't very nice, but the API is set so too late */
	if (put_user((void *)(unsigned long)maplist->user_token, &argp->handle))
		return -EFAULT;
	return 0;
}

/**
 * Remove a map private from list and deallocate resources if the mapping
 * isn't in use.
 *
 * \param inode device inode.
 * \param filp file pointer.
 * \param cmd command.
 * \param arg pointer to a drm_map_t structure.
 * \return zero on success or a negative value on error.
 *
 * Searches the map on drm_device::maplist, removes it from the list, see if
 * its being used, and free any associate resource (such as MTRR's) if it's not
 * being on use.
 *
 * \sa drm_addmap
 */
int drm_rmmap_locked(drm_device_t *dev, drm_local_map_t *map)
{
	struct list_head *list;
	drm_map_list_t *r_list = NULL;
	drm_dma_handle_t dmah;

	/* Find the list entry for the map and remove it */
	list_for_each(list, &dev->maplist->head) {
		r_list = list_entry(list, drm_map_list_t, head);

		if (r_list->map == map) {
			list_del(list);
			drm_ht_remove_key(&dev->map_hash,
					  r_list->user_token >> PAGE_SHIFT);
			drm_free(list, sizeof(*list), DRM_MEM_MAPS);
			break;
		}
	}

	/* List has wrapped around to the head pointer, or it's empty and we
	 * didn't find anything.
	 */
	if (list == (&dev->maplist->head)) {
		return -EINVAL;
	}

	switch (map->type) {
	case _DRM_REGISTERS:
		iounmap(map->handle);
		/* FALLTHROUGH */
	case _DRM_FRAME_BUFFER:
		if (drm_core_has_MTRR(dev) && map->mtrr >= 0) {
			int retcode;
			retcode = mtrr_del(map->mtrr, map->offset, map->size);
			DRM_DEBUG("mtrr_del=%d\n", retcode);
		}
		break;
	case _DRM_SHM:
		vfree(map->handle);
		break;
	case _DRM_AGP:
	case _DRM_SCATTER_GATHER:
		break;
	case _DRM_CONSISTENT:
		dmah.vaddr = map->handle;
		dmah.busaddr = map->offset;
		dmah.size = map->size;
		__drm_pci_free(dev, &dmah);
		break;
	}
	drm_free(map, sizeof(*map), DRM_MEM_MAPS);

	return 0;
}

int drm_rmmap(drm_device_t *dev, drm_local_map_t *map)
{
	int ret;

	mutex_lock(&dev->struct_mutex);
	ret = drm_rmmap_locked(dev, map);
	mutex_unlock(&dev->struct_mutex);

	return ret;
}

/* The rmmap ioctl appears to be unnecessary.  All mappings are torn down on
 * the last close of the device, and this is necessary for cleanup when things
 * exit uncleanly.  Therefore, having userland manually remove mappings seems
 * like a pointless exercise since they're going away anyway.
 *
 * One use case might be after addmap is allowed for normal users for SHM and
 * gets used by drivers that the server doesn't need to care about.  This seems
 * unlikely.
 */
int drm_rmmap_ioctl(struct inode *inode, struct file *filp,
		    unsigned int cmd, unsigned long arg)
{
	drm_file_t *priv = filp->private_data;
	drm_device_t *dev = priv->head->dev;
	drm_map_t request;
	drm_local_map_t *map = NULL;
	struct list_head *list;
	int ret;

	if (copy_from_user(&request, (drm_map_t __user *) arg, sizeof(request))) {
		return -EFAULT;
	}

	mutex_lock(&dev->struct_mutex);
	list_for_each(list, &dev->maplist->head) {
		drm_map_list_t *r_list = list_entry(list, drm_map_list_t, head);

		if (r_list->map &&
		    r_list->user_token == (unsigned long)request.handle &&
		    r_list->map->flags & _DRM_REMOVABLE) {
			map = r_list->map;
			break;
		}
	}

	/* List has wrapped around to the head pointer, or its empty we didn't
	 * find anything.
	 */
	if (list == (&dev->maplist->head)) {
		mutex_unlock(&dev->struct_mutex);
		return -EINVAL;
	}

	if (!map) {
		mutex_unlock(&dev->struct_mutex);
		return -EINVAL;
	}

	/* Register and framebuffer maps are permanent */
	if ((map->type == _DRM_REGISTERS) || (map->type == _DRM_FRAME_BUFFER)) {
		mutex_unlock(&dev->struct_mutex);
		return 0;
	}

	ret = drm_rmmap_locked(dev, map);

	mutex_unlock(&dev->struct_mutex);

	return ret;
}

/**
 * Cleanup after an error on one of the addbufs() functions.
 *
 * \param dev DRM device.
 * \param entry buffer entry where the error occurred.
 *
 * Frees any pages and buffers associated with the given entry.
 */
static void drm_cleanup_buf_error(drm_device_t * dev, drm_buf_entry_t * entry)
{
	int i;

	if (entry->seg_count) {
		for (i = 0; i < entry->seg_count; i++) {
			if (entry->seglist[i]) {
				drm_pci_free(dev, entry->seglist[i]);
			}
		}
		drm_free(entry->seglist,
			 entry->seg_count *
			 sizeof(*entry->seglist), DRM_MEM_SEGS);

		entry->seg_count = 0;
	}

	if (entry->buf_count) {
		for (i = 0; i < entry->buf_count; i++) {
			if (entry->buflist[i].dev_private) {
				drm_free(entry->buflist[i].dev_private,
					 entry->buflist[i].dev_priv_size,
					 DRM_MEM_BUFS);
			}
		}
		drm_free(entry->buflist,
			 entry->buf_count *
			 sizeof(*entry->buflist), DRM_MEM_BUFS);

		entry->buf_count = 0;
	}
}

#if __OS_HAS_AGP
/**
 * Add AGP buffers for DMA transfers.
 *
 * \param dev drm_device_t to which the buffers are to be added.
 * \param request pointer to a drm_buf_desc_t describing the request.
 * \return zero on success or a negative number on failure.
 *
 * After some sanity checks creates a drm_buf structure for each buffer and
 * reallocates the buffer list of the same size order to accommodate the new
 * buffers.
 */
int drm_addbufs_agp(drm_device_t * dev, drm_buf_desc_t * request)
{
	drm_device_dma_t *dma = dev->dma;
	drm_buf_entry_t *entry;
	drm_agp_mem_t *agp_entry;
	drm_buf_t *buf;
	unsigned long offset;
	unsigned long agp_offset;
	int count;
	int order;
	int size;
	int alignment;
	int page_order;
	int total;
	int byte_count;
	int i, valid;
	drm_buf_t **temp_buflist;

	if (!dma)
		return -EINVAL;

	count = request->count;
	order = drm_order(request->size);
	size = 1 << order;

	alignment = (request->flags & _DRM_PAGE_ALIGN)
	    ? PAGE_ALIGN(size) : size;
	page_order = order - PAGE_SHIFT > 0 ? order - PAGE_SHIFT : 0;
	total = PAGE_SIZE << page_order;

	byte_count = 0;
	agp_offset = dev->agp->base + request->agp_start;

	DRM_DEBUG("count:      %d\n", count);
	DRM_DEBUG("order:      %d\n", order);
	DRM_DEBUG("size:       %d\n", size);
	DRM_DEBUG("agp_offset: %lx\n", agp_offset);
	DRM_DEBUG("alignment:  %d\n", alignment);
	DRM_DEBUG("page_order: %d\n", page_order);
	DRM_DEBUG("total:      %d\n", total);

	if (order < DRM_MIN_ORDER || order > DRM_MAX_ORDER)
		return -EINVAL;
	if (dev->queue_count)
		return -EBUSY;	/* Not while in use */

	/* Make sure buffers are located in AGP memory that we own */
	valid = 0;
	for (agp_entry = dev->agp->memory; agp_entry; agp_entry = agp_entry->next) {
		if ((agp_offset >= agp_entry->bound) &&
		    (agp_offset + total * count <= agp_entry->bound + agp_entry->pages * PAGE_SIZE)) {
			valid = 1;
			break;
		}
	}
	if (dev->agp->memory && !valid) {
		DRM_DEBUG("zone invalid\n");
		return -EINVAL;
	}
	spin_lock(&dev->count_lock);
	if (dev->buf_use) {
		spin_unlock(&dev->count_lock);
		return -EBUSY;
	}
	atomic_inc(&dev->buf_alloc);
	spin_unlock(&dev->count_lock);

	mutex_lock(&dev->struct_mutex);
	entry = &dma->bufs[order];
	if (entry->buf_count) {
		mutex_unlock(&dev->struct_mutex);
		atomic_dec(&dev->buf_alloc);
		return -ENOMEM;	/* May only call once for each order */
	}

	if (count < 0 || count > 4096) {
		mutex_unlock(&dev->struct_mutex);
		atomic_dec(&dev->buf_alloc);
		return -EINVAL;
	}

	entry->buflist = drm_alloc(count * sizeof(*entry->buflist),
				   DRM_MEM_BUFS);
	if (!entry->buflist) {
		mutex_unlock(&dev->struct_mutex);
		atomic_dec(&dev->buf_alloc);
		return -ENOMEM;
	}
	memset(entry->buflist, 0, count * sizeof(*entry->buflist));

	entry->buf_size = size;
	entry->page_order = page_order;

	offset = 0;

	while (entry->buf_count < count) {
		buf = &entry->buflist[entry->buf_count];
		buf->idx = dma->buf_count + entry->buf_count;
		buf->total = alignment;
		buf->order = order;
		buf->used = 0;

		buf->offset = (dma->byte_count + offset);
		buf->bus_address = agp_offset + offset;
		buf->address = (void *)(agp_offset + offset);
		buf->next = NULL;
		buf->waiting = 0;
		buf->pending = 0;
		init_waitqueue_head(&buf->dma_wait);
		buf->filp = NULL;

		buf->dev_priv_size = dev->driver->dev_priv_size;
		buf->dev_private = drm_alloc(buf->dev_priv_size, DRM_MEM_BUFS);
		if (!buf->dev_private) {
			/* Set count correctly so we free the proper amount. */
			entry->buf_count = count;
			drm_cleanup_buf_error(dev, entry);
			mutex_unlock(&dev->struct_mutex);
			atomic_dec(&dev->buf_alloc);
			return -ENOMEM;
		}
		memset(buf->dev_private, 0, buf->dev_priv_size);

		DRM_DEBUG("buffer %d @ %p\n", entry->buf_count, buf->address);

		offset += alignment;
		entry->buf_count++;
		byte_count += PAGE_SIZE << page_order;
	}

	DRM_DEBUG("byte_count: %d\n", byte_count);

	temp_buflist = drm_realloc(dma->buflist,
				   dma->buf_count * sizeof(*dma->buflist),
				   (dma->buf_count + entry->buf_count)
				   * sizeof(*dma->buflist), DRM_MEM_BUFS);
	if (!temp_buflist) {
		/* Free the entry because it isn't valid */
		drm_cleanup_buf_error(dev, entry);
		mutex_unlock(&dev->struct_mutex);
		atomic_dec(&dev->buf_alloc);
		return -ENOMEM;
	}
	dma->buflist = temp_buflist;

	for (i = 0; i < entry->buf_count; i++) {
		dma->buflist[i + dma->buf_count] = &entry->buflist[i];
	}

	dma->buf_count += entry->buf_count;
	dma->seg_count += entry->seg_count;
	dma->page_count += byte_count >> PAGE_SHIFT;
	dma->byte_count += byte_count;

	DRM_DEBUG("dma->buf_count : %d\n", dma->buf_count);
	DRM_DEBUG("entry->buf_count : %d\n", entry->buf_count);

	mutex_unlock(&dev->struct_mutex);

	request->count = entry->buf_count;
	request->size = size;

	dma->flags = _DRM_DMA_USE_AGP;

	atomic_dec(&dev->buf_alloc);
	return 0;
}
EXPORT_SYMBOL(drm_addbufs_agp);
#endif				/* __OS_HAS_AGP */

int drm_addbufs_pci(drm_device_t * dev, drm_buf_desc_t * request)
{
	drm_device_dma_t *dma = dev->dma;
	int count;
	int order;
	int size;
	int total;
	int page_order;
	drm_buf_entry_t *entry;
	drm_dma_handle_t *dmah;
	drm_buf_t *buf;
	int alignment;
	unsigned long offset;
	int i;
	int byte_count;
	int page_count;
	unsigned long *temp_pagelist;
	drm_buf_t **temp_buflist;

	if (!drm_core_check_feature(dev, DRIVER_PCI_DMA))
		return -EINVAL;

	if (!dma)
		return -EINVAL;

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;

	count = request->count;
	order = drm_order(request->size);
	size = 1 << order;

	DRM_DEBUG("count=%d, size=%d (%d), order=%d, queue_count=%d\n",
		  request->count, request->size, size, order, dev->queue_count);

	if (order < DRM_MIN_ORDER || order > DRM_MAX_ORDER)
		return -EINVAL;
	if (dev->queue_count)
		return -EBUSY;	/* Not while in use */

	alignment = (request->flags & _DRM_PAGE_ALIGN)
	    ? PAGE_ALIGN(size) : size;
	page_order = order - PAGE_SHIFT > 0 ? order - PAGE_SHIFT : 0;
	total = PAGE_SIZE << page_order;

	spin_lock(&dev->count_lock);
	if (dev->buf_use) {
		spin_unlock(&dev->count_lock);
		return -EBUSY;
	}
	atomic_inc(&dev->buf_alloc);
	spin_unlock(&dev->count_lock);

	mutex_lock(&dev->struct_mutex);
	entry = &dma->bufs[order];
	if (entry->buf_count) {
		mutex_unlock(&dev->struct_mutex);
		atomic_dec(&dev->buf_alloc);
		return -ENOMEM;	/* May only call once for each order */
	}

	if (count < 0 || count > 4096) {
		mutex_unlock(&dev->struct_mutex);
		atomic_dec(&dev->buf_alloc);
		return -EINVAL;
	}

	entry->buflist = drm_alloc(count * sizeof(*entry->buflist),
				   DRM_MEM_BUFS);
	if (!entry->buflist) {
		mutex_unlock(&dev->struct_mutex);
		atomic_dec(&dev->buf_alloc);
		return -ENOMEM;
	}
	memset(entry->buflist, 0, count * sizeof(*entry->buflist));

	entry->seglist = drm_alloc(count * sizeof(*entry->seglist),
				   DRM_MEM_SEGS);
	if (!entry->seglist) {
		drm_free(entry->buflist,
			 count * sizeof(*entry->buflist), DRM_MEM_BUFS);
		mutex_unlock(&dev->struct_mutex);
		atomic_dec(&dev->buf_alloc);
		return -ENOMEM;
	}
	memset(entry->seglist, 0, count * sizeof(*entry->seglist));

	/* Keep the original pagelist until we know all the allocations
	 * have succeeded
	 */
	temp_pagelist = drm_alloc((dma->page_count + (count << page_order))
				  * sizeof(*dma->pagelist), DRM_MEM_PAGES);
	if (!temp_pagelist) {
		drm_free(entry->buflist,
			 count * sizeof(*entry->buflist), DRM_MEM_BUFS);
		drm_free(entry->seglist,
			 count * sizeof(*entry->seglist), DRM_MEM_SEGS);
		mutex_unlock(&dev->struct_mutex);
		atomic_dec(&dev->buf_alloc);
		return -ENOMEM;
	}
	memcpy(temp_pagelist,
	       dma->pagelist, dma->page_count * sizeof(*dma->pagelist));
	DRM_DEBUG("pagelist: %d entries\n",
		  dma->page_count + (count << page_order));

	entry->buf_size = size;
	entry->page_order = page_order;
	byte_count = 0;
	page_count = 0;

	while (entry->buf_count < count) {
		
		dmah = drm_pci_alloc(dev, PAGE_SIZE << page_order, 0x1000, 0xfffffffful);
		
		if (!dmah) {
			/* Set count correctly so we free the proper amount. */
			entry->buf_count = count;
			entry->seg_count = count;
			drm_cleanup_buf_error(dev, entry);
			drm_free(temp_pagelist,
				 (dma->page_count + (count << page_order))
				 * sizeof(*dma->pagelist), DRM_MEM_PAGES);
			mutex_unlock(&dev->struct_mutex);
			atomic_dec(&dev->buf_alloc);
			return -ENOMEM;
		}
		entry->seglist[entry->seg_count++] = dmah;
		for (i = 0; i < (1 << page_order); i++) {
			DRM_DEBUG("page %d @ 0x%08lx\n",
				  dma->page_count + page_count,
				  (unsigned long)dmah->vaddr + PAGE_SIZE * i);
			temp_pagelist[dma->page_count + page_count++]
				= (unsigned long)dmah->vaddr + PAGE_SIZE * i;
		}
		for (offset = 0;
		     offset + size <= total && entry->buf_count < count;
		     offset += alignment, ++entry->buf_count) {
			buf = &entry->buflist[entry->buf_count];
			buf->idx = dma->buf_count + entry->buf_count;
			buf->total = alignment;
			buf->order = order;
			buf->used = 0;
			buf->offset = (dma->byte_count + byte_count + offset);
			buf->address = (void *)(dmah->vaddr + offset);
			buf->bus_address = dmah->busaddr + offset;
			buf->next = NULL;
			buf->waiting = 0;
			buf->pending = 0;
			init_waitqueue_head(&buf->dma_wait);
			buf->filp = NULL;

			buf->dev_priv_size = dev->driver->dev_priv_size;
			buf->dev_private = drm_alloc(buf->dev_priv_size,
						     DRM_MEM_BUFS);
			if (!buf->dev_private) {
				/* Set count correctly so we free the proper amount. */
				entry->buf_count = count;
				entry->seg_count = count;
				drm_cleanup_buf_error(dev, entry);
				drm_free(temp_pagelist,
					 (dma->page_count +
					  (count << page_order))
					 * sizeof(*dma->pagelist),
					 DRM_MEM_PAGES);
				mutex_unlock(&dev->struct_mutex);
				atomic_dec(&dev->buf_alloc);
				return -ENOMEM;
			}
			memset(buf->dev_private, 0, buf->dev_priv_size);

			DRM_DEBUG("buffer %d @ %p\n",
				  entry->buf_count, buf->address);
		}
		byte_count += PAGE_SIZE << page_order;
	}

	temp_buflist = drm_realloc(dma->buflist,
				   dma->buf_count * sizeof(*dma->buflist),
				   (dma->buf_count + entry->buf_count)
				   * sizeof(*dma->buflist), DRM_MEM_BUFS);
	if (!temp_buflist) {
		/* Free the entry because it isn't valid */
		drm_cleanup_buf_error(dev, entry);
		drm_free(temp_pagelist,
			 (dma->page_count + (count << page_order))
			 * sizeof(*dma->pagelist), DRM_MEM_PAGES);
		mutex_unlock(&dev->struct_mutex);
		atomic_dec(&dev->buf_alloc);
		return -ENOMEM;
	}
	dma->buflist = temp_buflist;

	for (i = 0; i < entry->buf_count; i++) {
		dma->buflist[i + dma->buf_count] = &entry->buflist[i];
	}

	/* No allocations failed, so now we can replace the orginal pagelist
	 * with the new one.
	 */
	if (dma->page_count) {
		drm_free(dma->pagelist,
			 dma->page_count * sizeof(*dma->pagelist),
			 DRM_MEM_PAGES);
	}
	dma->pagelist = temp_pagelist;

	dma->buf_count += entry->buf_count;
	dma->seg_count += entry->seg_count;
	dma->page_count += entry->seg_count << page_order;
	dma->byte_count += PAGE_SIZE * (entry->seg_count << page_order);

	mutex_unlock(&dev->struct_mutex);

	request->count = entry->buf_count;
	request->size = size;

	if (request->flags & _DRM_PCI_BUFFER_RO)
		dma->flags = _DRM_DMA_USE_PCI_RO;

	atomic_dec(&dev->buf_alloc);
	return 0;

}
EXPORT_SYMBOL(drm_addbufs_pci);

static int drm_addbufs_sg(drm_device_t * dev, drm_buf_desc_t * request)
{
	drm_device_dma_t *dma = dev->dma;
	drm_buf_entry_t *entry;
	drm_buf_t *buf;
	unsigned long offset;
	unsigned long agp_offset;
	int count;
	int order;
	int size;
	int alignment;
	int page_order;
	int total;
	int byte_count;
	int i;
	drm_buf_t **temp_buflist;

	if (!drm_core_check_feature(dev, DRIVER_SG))
		return -EINVAL;

	if (!dma)
		return -EINVAL;

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;

	count = request->count;
	order = drm_order(request->size);
	size = 1 << order;

	alignment = (request->flags & _DRM_PAGE_ALIGN)
	    ? PAGE_ALIGN(size) : size;
	page_order = order - PAGE_SHIFT > 0 ? order - PAGE_SHIFT : 0;
	total = PAGE_SIZE << page_order;

	byte_count = 0;
	agp_offset = request->agp_start;

	DRM_DEBUG("count:      %d\n", count);
	DRM_DEBUG("order:      %d\n", order);
	DRM_DEBUG("size:       %d\n", size);
	DRM_DEBUG("agp_offset: %lu\n", agp_offset);
	DRM_DEBUG("alignment:  %d\n", alignment);
	DRM_DEBUG("page_order: %d\n", page_order);
	DRM_DEBUG("total:      %d\n", total);

	if (order < DRM_MIN_ORDER || order > DRM_MAX_ORDER)
		return -EINVAL;
	if (dev->queue_count)
		return -EBUSY;	/* Not while in use */

	spin_lock(&dev->count_lock);
	if (dev->buf_use) {
		spin_unlock(&dev->count_lock);
		return -EBUSY;
	}
	atomic_inc(&dev->buf_alloc);
	spin_unlock(&dev->count_lock);

	mutex_lock(&dev->struct_mutex);
	entry = &dma->bufs[order];
	if (entry->buf_count) {
		mutex_unlock(&dev->struct_mutex);
		atomic_dec(&dev->buf_alloc);
		return -ENOMEM;	/* May only call once for each order */
	}

	if (count < 0 || count > 4096) {
		mutex_unlock(&dev->struct_mutex);
		atomic_dec(&dev->buf_alloc);
		return -EINVAL;
	}

	entry->buflist = drm_alloc(count * sizeof(*entry->buflist),
				   DRM_MEM_BUFS);
	if (!entry->buflist) {
		mutex_unlock(&dev->struct_mutex);
		atomic_dec(&dev->buf_alloc);
		return -ENOMEM;
	}
	memset(entry->buflist, 0, count * sizeof(*entry->buflist));

	entry->buf_size = size;
	entry->page_order = page_order;

	offset = 0;

	while (entry->buf_count < count) {
		buf = &entry->buflist[entry->buf_count];
		buf->idx = dma->buf_count + entry->buf_count;
		buf->total = alignment;
		buf->order = order;
		buf->used = 0;

		buf->offset = (dma->byte_count + offset);
		buf->bus_address = agp_offset + offset;
		buf->address = (void *)(agp_offset + offset
					+ (unsigned long)dev->sg->virtual);
		buf->next = NULL;
		buf->waiting = 0;
		buf->pending = 0;
		init_waitqueue_head(&buf->dma_wait);
		buf->filp = NULL;

		buf->dev_priv_size = dev->driver->dev_priv_size;
		buf->dev_private = drm_alloc(buf->dev_priv_size, DRM_MEM_BUFS);
		if (!buf->dev_private) {
			/* Set count correctly so we free the proper amount. */
			entry->buf_count = count;
			drm_cleanup_buf_error(dev, entry);
			mutex_unlock(&dev->struct_mutex);
			atomic_dec(&dev->buf_alloc);
			return -ENOMEM;
		}

		memset(buf->dev_private, 0, buf->dev_priv_size);

		DRM_DEBUG("buffer %d @ %p\n", entry->buf_count, buf->address);

		offset += alignment;
		entry->buf_count++;
		byte_count += PAGE_SIZE << page_order;
	}

	DRM_DEBUG("byte_count: %d\n", byte_count);

	temp_buflist = drm_realloc(dma->buflist,
				   dma->buf_count * sizeof(*dma->buflist),
				   (dma->buf_count + entry->buf_count)
				   * sizeof(*dma->buflist), DRM_MEM_BUFS);
	if (!temp_buflist) {
		/* Free the entry because it isn't valid */
		drm_cleanup_buf_error(dev, entry);
		mutex_unlock(&dev->struct_mutex);
		atomic_dec(&dev->buf_alloc);
		return -ENOMEM;
	}
	dma->buflist = temp_buflist;

	for (i = 0; i < entry->buf_count; i++) {
		dma->buflist[i + dma->buf_count] = &entry->buflist[i];
	}

	dma->buf_count += entry->buf_count;
	dma->seg_count += entry->seg_count;
	dma->page_count += byte_count >> PAGE_SHIFT;
	dma->byte_count += byte_count;

	DRM_DEBUG("dma->buf_count : %d\n", dma->buf_count);
	DRM_DEBUG("entry->buf_count : %d\n", entry->buf_count);

	mutex_unlock(&dev->struct_mutex);

	request->count = entry->buf_count;
	request->size = size;

	dma->flags = _DRM_DMA_USE_SG;

	atomic_dec(&dev->buf_alloc);
	return 0;
}

static int drm_addbufs_fb(drm_device_t * dev, drm_buf_desc_t * request)
{
	drm_device_dma_t *dma = dev->dma;
	drm_buf_entry_t *entry;
	drm_buf_t *buf;
	unsigned long offset;
	unsigned long agp_offset;
	int count;
	int order;
	int size;
	int alignment;
	int page_order;
	int total;
	int byte_count;
	int i;
	drm_buf_t **temp_buflist;

	if (!drm_core_check_feature(dev, DRIVER_FB_DMA))
		return -EINVAL;

	if (!dma)
		return -EINVAL;

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;

	count = request->count;
	order = drm_order(request->size);
	size = 1 << order;

	alignment = (request->flags & _DRM_PAGE_ALIGN)
	    ? PAGE_ALIGN(size) : size;
	page_order = order - PAGE_SHIFT > 0 ? order - PAGE_SHIFT : 0;
	total = PAGE_SIZE << page_order;

	byte_count = 0;
	agp_offset = request->agp_start;

	DRM_DEBUG("count:      %d\n", count);
	DRM_DEBUG("order:      %d\n", order);
	DRM_DEBUG("size:       %d\n", size);
	DRM_DEBUG("agp_offset: %lu\n", agp_offset);
	DRM_DEBUG("alignment:  %d\n", alignment);
	DRM_DEBUG("page_order: %d\n", page_order);
	DRM_DEBUG("total:      %d\n", total);

	if (order < DRM_MIN_ORDER || order > DRM_MAX_ORDER)
		return -EINVAL;
	if (dev->queue_count)
		return -EBUSY;	/* Not while in use */

	spin_lock(&dev->count_lock);
	if (dev->buf_use) {
		spin_unlock(&dev->count_lock);
		return -EBUSY;
	}
	atomic_inc(&dev->buf_alloc);
	spin_unlock(&dev->count_lock);

	mutex_lock(&dev->struct_mutex);
	entry = &dma->bufs[order];
	if (entry->buf_count) {
		mutex_unlock(&dev->struct_mutex);
		atomic_dec(&dev->buf_alloc);
		return -ENOMEM;	/* May only call once for each order */
	}

	if (count < 0 || count > 4096) {
		mutex_unlock(&dev->struct_mutex);
		atomic_dec(&dev->buf_alloc);
		return -EINVAL;
	}

	entry->buflist = drm_alloc(count * sizeof(*entry->buflist),
				   DRM_MEM_BUFS);
	if (!entry->buflist) {
		mutex_unlock(&dev->struct_mutex);
		atomic_dec(&dev->buf_alloc);
		return -ENOMEM;
	}
	memset(entry->buflist, 0, count * sizeof(*entry->buflist));

	entry->buf_size = size;
	entry->page_order = page_order;

	offset = 0;

	while (entry->buf_count < count) {
		buf = &entry->buflist[entry->buf_count];
		buf->idx = dma->buf_count + entry->buf_count;
		buf->total = alignment;
		buf->order = order;
		buf->used = 0;

		buf->offset = (dma->byte_count + offset);
		buf->bus_address = agp_offset + offset;
		buf->address = (void *)(agp_offset + offset);
		buf->next = NULL;
		buf->waiting = 0;
		buf->pending = 0;
		init_waitqueue_head(&buf->dma_wait);
		buf->filp = NULL;

		buf->dev_priv_size = dev->driver->dev_priv_size;
		buf->dev_private = drm_alloc(buf->dev_priv_size, DRM_MEM_BUFS);
		if (!buf->dev_private) {
			/* Set count correctly so we free the proper amount. */
			entry->buf_count = count;
			drm_cleanup_buf_error(dev, entry);
			mutex_unlock(&dev->struct_mutex);
			atomic_dec(&dev->buf_alloc);
			return -ENOMEM;
		}
		memset(buf->dev_private, 0, buf->dev_priv_size);

		DRM_DEBUG("buffer %d @ %p\n", entry->buf_count, buf->address);

		offset += alignment;
		entry->buf_count++;
		byte_count += PAGE_SIZE << page_order;
	}

	DRM_DEBUG("byte_count: %d\n", byte_count);

	temp_buflist = drm_realloc(dma->buflist,
				   dma->buf_count * sizeof(*dma->buflist),
				   (dma->buf_count + entry->buf_count)
				   * sizeof(*dma->buflist), DRM_MEM_BUFS);
	if (!temp_buflist) {
		/* Free the entry because it isn't valid */
		drm_cleanup_buf_error(dev, entry);
		mutex_unlock(&dev->struct_mutex);
		atomic_dec(&dev->buf_alloc);
		return -ENOMEM;
	}
	dma->buflist = temp_buflist;

	for (i = 0; i < entry->buf_count; i++) {
		dma->buflist[i + dma->buf_count] = &entry->buflist[i];
	}

	dma->buf_count += entry->buf_count;
	dma->seg_count += entry->seg_count;
	dma->page_count += byte_count >> PAGE_SHIFT;
	dma->byte_count += byte_count;

	DRM_DEBUG("dma->buf_count : %d\n", dma->buf_count);
	DRM_DEBUG("entry->buf_count : %d\n", entry->buf_count);

	mutex_unlock(&dev->struct_mutex);

	request->count = entry->buf_count;
	request->size = size;

	dma->flags = _DRM_DMA_USE_FB;

	atomic_dec(&dev->buf_alloc);
	return 0;
}


/**
 * Add buffers for DMA transfers (ioctl).
 *
 * \param inode device inode.
 * \param filp file pointer.
 * \param cmd command.
 * \param arg pointer to a drm_buf_desc_t request.
 * \return zero on success or a negative number on failure.
 *
 * According with the memory type specified in drm_buf_desc::flags and the
 * build options, it dispatches the call either to addbufs_agp(),
 * addbufs_sg() or addbufs_pci() for AGP, scatter-gather or consistent
 * PCI memory respectively.
 */
int drm_addbufs(struct inode *inode, struct file *filp,
		unsigned int cmd, unsigned long arg)
{
	drm_buf_desc_t request;
	drm_file_t *priv = filp->private_data;
	drm_device_t *dev = priv->head->dev;
	int ret;

	if (!drm_core_check_feature(dev, DRIVER_HAVE_DMA))
		return -EINVAL;

	if (copy_from_user(&request, (drm_buf_desc_t __user *) arg,
			   sizeof(request)))
		return -EFAULT;

#if __OS_HAS_AGP
	if (request.flags & _DRM_AGP_BUFFER)
		ret = drm_addbufs_agp(dev, &request);
	else
#endif
	if (request.flags & _DRM_SG_BUFFER)
		ret = drm_addbufs_sg(dev, &request);
	else if (request.flags & _DRM_FB_BUFFER)
		ret = drm_addbufs_fb(dev, &request);
	else
		ret = drm_addbufs_pci(dev, &request);

	if (ret == 0) {
		if (copy_to_user((void __user *)arg, &request, sizeof(request))) {
			ret = -EFAULT;
		}
	}
	return ret;
}

/**
 * Get information about the buffer mappings.
 *
 * This was originally mean for debugging purposes, or by a sophisticated
 * client library to determine how best to use the available buffers (e.g.,
 * large buffers can be used for image transfer).
 *
 * \param inode device inode.
 * \param filp file pointer.
 * \param cmd command.
 * \param arg pointer to a drm_buf_info structure.
 * \return zero on success or a negative number on failure.
 *
 * Increments drm_device::buf_use while holding the drm_device::count_lock
 * lock, preventing of allocating more buffers after this call. Information
 * about each requested buffer is then copied into user space.
 */
int drm_infobufs(struct inode *inode, struct file *filp,
		 unsigned int cmd, unsigned long arg)
{
	drm_file_t *priv = filp->private_data;
	drm_device_t *dev = priv->head->dev;
	drm_device_dma_t *dma = dev->dma;
	drm_buf_info_t request;
	drm_buf_info_t __user *argp = (void __user *)arg;
	int i;
	int count;

	if (!drm_core_check_feature(dev, DRIVER_HAVE_DMA))
		return -EINVAL;

	if (!dma)
		return -EINVAL;

	spin_lock(&dev->count_lock);
	if (atomic_read(&dev->buf_alloc)) {
		spin_unlock(&dev->count_lock);
		return -EBUSY;
	}
	++dev->buf_use;		/* Can't allocate more after this call */
	spin_unlock(&dev->count_lock);

	if (copy_from_user(&request, argp, sizeof(request)))
		return -EFAULT;

	for (i = 0, count = 0; i < DRM_MAX_ORDER + 1; i++) {
		if (dma->bufs[i].buf_count)
			++count;
	}

	DRM_DEBUG("count = %d\n", count);

	if (request.count >= count) {
		for (i = 0, count = 0; i < DRM_MAX_ORDER + 1; i++) {
			if (dma->bufs[i].buf_count) {
				drm_buf_desc_t __user *to =
				    &request.list[count];
				drm_buf_entry_t *from = &dma->bufs[i];
				drm_freelist_t *list = &dma->bufs[i].freelist;
				if (copy_to_user(&to->count,
						 &from->buf_count,
						 sizeof(from->buf_count)) ||
				    copy_to_user(&to->size,
						 &from->buf_size,
						 sizeof(from->buf_size)) ||
				    copy_to_user(&to->low_mark,
						 &list->low_mark,
						 sizeof(list->low_mark)) ||
				    copy_to_user(&to->high_mark,
						 &list->high_mark,
						 sizeof(list->high_mark)))
					return -EFAULT;

				DRM_DEBUG("%d %d %d %d %d\n",
					  i,
					  dma->bufs[i].buf_count,
					  dma->bufs[i].buf_size,
					  dma->bufs[i].freelist.low_mark,
					  dma->bufs[i].freelist.high_mark);
				++count;
			}
		}
	}
	request.count = count;

	if (copy_to_user(argp, &request, sizeof(request)))
		return -EFAULT;

	return 0;
}

/**
 * Specifies a low and high water mark for buffer allocation
 *
 * \param inode device inode.
 * \param filp file pointer.
 * \param cmd command.
 * \param arg a pointer to a drm_buf_desc structure.
 * \return zero on success or a negative number on failure.
 *
 * Verifies that the size order is bounded between the admissible orders and
 * updates the respective drm_device_dma::bufs entry low and high water mark.
 *
 * \note This ioctl is deprecated and mostly never used.
 */
int drm_markbufs(struct inode *inode, struct file *filp,
		 unsigned int cmd, unsigned long arg)
{
	drm_file_t *priv = filp->private_data;
	drm_device_t *dev = priv->head->dev;
	drm_device_dma_t *dma = dev->dma;
	drm_buf_desc_t request;
	int order;
	drm_buf_entry_t *entry;

	if (!drm_core_check_feature(dev, DRIVER_HAVE_DMA))
		return -EINVAL;

	if (!dma)
		return -EINVAL;

	if (copy_from_user(&request,
			   (drm_buf_desc_t __user *) arg, sizeof(request)))
		return -EFAULT;

	DRM_DEBUG("%d, %d, %d\n",
		  request.size, request.low_mark, request.high_mark);
	order = drm_order(request.size);
	if (order < DRM_MIN_ORDER || order > DRM_MAX_ORDER)
		return -EINVAL;
	entry = &dma->bufs[order];

	if (request.low_mark < 0 || request.low_mark > entry->buf_count)
		return -EINVAL;
	if (request.high_mark < 0 || request.high_mark > entry->buf_count)
		return -EINVAL;

	entry->freelist.low_mark = request.low_mark;
	entry->freelist.high_mark = request.high_mark;

	return 0;
}

/**
 * Unreserve the buffers in list, previously reserved using drmDMA.
 *
 * \param inode device inode.
 * \param filp file pointer.
 * \param cmd command.
 * \param arg pointer to a drm_buf_free structure.
 * \return zero on success or a negative number on failure.
 *
 * Calls free_buffer() for each used buffer.
 * This function is primarily used for debugging.
 */
int drm_freebufs(struct inode *inode, struct file *filp,
		 unsigned int cmd, unsigned long arg)
{
	drm_file_t *priv = filp->private_data;
	drm_device_t *dev = priv->head->dev;
	drm_device_dma_t *dma = dev->dma;
	drm_buf_free_t request;
	int i;
	int idx;
	drm_buf_t *buf;

	if (!drm_core_check_feature(dev, DRIVER_HAVE_DMA))
		return -EINVAL;

	if (!dma)
		return -EINVAL;

	if (copy_from_user(&request,
			   (drm_buf_free_t __user *) arg, sizeof(request)))
		return -EFAULT;

	DRM_DEBUG("%d\n", request.count);
	for (i = 0; i < request.count; i++) {
		if (copy_from_user(&idx, &request.list[i], sizeof(idx)))
			return -EFAULT;
		if (idx < 0 || idx >= dma->buf_count) {
			DRM_ERROR("Index %d (of %d max)\n",
				  idx, dma->buf_count - 1);
			return -EINVAL;
		}
		buf = dma->buflist[idx];
		if (buf->filp != filp) {
			DRM_ERROR("Process %d freeing buffer not owned\n",
				  current->pid);
			return -EINVAL;
		}
		drm_free_buffer(dev, buf);
	}

	return 0;
}

/**
 * Maps all of the DMA buffers into client-virtual space (ioctl).
 *
 * \param inode device inode.
 * \param filp file pointer.
 * \param cmd command.
 * \param arg pointer to a drm_buf_map structure.
 * \return zero on success or a negative number on failure.
 *
 * Maps the AGP, SG or PCI buffer region with do_mmap(), and copies information
 * about each buffer into user space. For PCI buffers, it calls do_mmap() with
 * offset equal to 0, which drm_mmap() interpretes as PCI buffers and calls
 * drm_mmap_dma().
 */
int drm_mapbufs(struct inode *inode, struct file *filp,
		unsigned int cmd, unsigned long arg)
{
	drm_file_t *priv = filp->private_data;
	drm_device_t *dev = priv->head->dev;
	drm_device_dma_t *dma = dev->dma;
	drm_buf_map_t __user *argp = (void __user *)arg;
	int retcode = 0;
	const int zero = 0;
	unsigned long virtual;
	unsigned long address;
	drm_buf_map_t request;
	int i;

	if (!drm_core_check_feature(dev, DRIVER_HAVE_DMA))
		return -EINVAL;

	if (!dma)
		return -EINVAL;

	spin_lock(&dev->count_lock);
	if (atomic_read(&dev->buf_alloc)) {
		spin_unlock(&dev->count_lock);
		return -EBUSY;
	}
	dev->buf_use++;		/* Can't allocate more after this call */
	spin_unlock(&dev->count_lock);

	if (copy_from_user(&request, argp, sizeof(request)))
		return -EFAULT;

	if (request.count >= dma->buf_count) {
		if ((drm_core_has_AGP(dev) && (dma->flags & _DRM_DMA_USE_AGP))
		    || (drm_core_check_feature(dev, DRIVER_SG)
			&& (dma->flags & _DRM_DMA_USE_SG))
		    || (drm_core_check_feature(dev, DRIVER_FB_DMA)
			&& (dma->flags & _DRM_DMA_USE_FB))) {
			drm_map_t *map = dev->agp_buffer_map;
			unsigned long token = dev->agp_buffer_token;

			if (!map) {
				retcode = -EINVAL;
				goto done;
			}

			down_write(&current->mm->mmap_sem);
			virtual = do_mmap(filp, 0, map->size,
					  PROT_READ | PROT_WRITE,
					  MAP_SHARED, token);
			up_write(&current->mm->mmap_sem);
		} else {
			down_write(&current->mm->mmap_sem);
			virtual = do_mmap(filp, 0, dma->byte_count,
					  PROT_READ | PROT_WRITE,
					  MAP_SHARED, 0);
			up_write(&current->mm->mmap_sem);
		}
		if (virtual > -1024UL) {
			/* Real error */
			retcode = (signed long)virtual;
			goto done;
		}
		request.virtual = (void __user *)virtual;

		for (i = 0; i < dma->buf_count; i++) {
			if (copy_to_user(&request.list[i].idx,
					 &dma->buflist[i]->idx,
					 sizeof(request.list[0].idx))) {
				retcode = -EFAULT;
				goto done;
			}
			if (copy_to_user(&request.list[i].total,
					 &dma->buflist[i]->total,
					 sizeof(request.list[0].total))) {
				retcode = -EFAULT;
				goto done;
			}
			if (copy_to_user(&request.list[i].used,
					 &zero, sizeof(zero))) {
				retcode = -EFAULT;
				goto done;
			}
			address = virtual + dma->buflist[i]->offset;	/* *** */
			if (copy_to_user(&request.list[i].address,
					 &address, sizeof(address))) {
				retcode = -EFAULT;
				goto done;
			}
		}
	}
      done:
	request.count = dma->buf_count;
	DRM_DEBUG("%d buffers, retcode = %d\n", request.count, retcode);

	if (copy_to_user(argp, &request, sizeof(request)))
		return -EFAULT;

	return retcode;
}

/**
 * Compute size order.  Returns the exponent of the smaller power of two which
 * is greater or equal to given number.
 *
 * \param size size.
 * \return order.
 *
 * \todo Can be made faster.
 */
int drm_order(unsigned long size)
{
	int order;
	unsigned long tmp;

	for (order = 0, tmp = size >> 1; tmp; tmp >>= 1, order++) ;

	if (size & (size - 1))
		++order;

	return order;
}
EXPORT_SYMBOL(drm_order);


