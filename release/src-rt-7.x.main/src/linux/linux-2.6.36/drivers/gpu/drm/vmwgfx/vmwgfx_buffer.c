/**************************************************************************
 *
 * Copyright © 2009 VMware, Inc., Palo Alto, CA., USA
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS, AUTHORS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/

#include "vmwgfx_drv.h"
#include "ttm/ttm_bo_driver.h"
#include "ttm/ttm_placement.h"

static uint32_t vram_placement_flags = TTM_PL_FLAG_VRAM |
	TTM_PL_FLAG_CACHED;

static uint32_t vram_ne_placement_flags = TTM_PL_FLAG_VRAM |
	TTM_PL_FLAG_CACHED |
	TTM_PL_FLAG_NO_EVICT;

static uint32_t sys_placement_flags = TTM_PL_FLAG_SYSTEM |
	TTM_PL_FLAG_CACHED;

struct ttm_placement vmw_vram_placement = {
	.fpfn = 0,
	.lpfn = 0,
	.num_placement = 1,
	.placement = &vram_placement_flags,
	.num_busy_placement = 1,
	.busy_placement = &vram_placement_flags
};

struct ttm_placement vmw_vram_sys_placement = {
	.fpfn = 0,
	.lpfn = 0,
	.num_placement = 1,
	.placement = &vram_placement_flags,
	.num_busy_placement = 1,
	.busy_placement = &sys_placement_flags
};

struct ttm_placement vmw_vram_ne_placement = {
	.fpfn = 0,
	.lpfn = 0,
	.num_placement = 1,
	.placement = &vram_ne_placement_flags,
	.num_busy_placement = 1,
	.busy_placement = &vram_ne_placement_flags
};

struct ttm_placement vmw_sys_placement = {
	.fpfn = 0,
	.lpfn = 0,
	.num_placement = 1,
	.placement = &sys_placement_flags,
	.num_busy_placement = 1,
	.busy_placement = &sys_placement_flags
};

struct vmw_ttm_backend {
	struct ttm_backend backend;
};

static int vmw_ttm_populate(struct ttm_backend *backend,
			    unsigned long num_pages, struct page **pages,
			    struct page *dummy_read_page)
{
	return 0;
}

static int vmw_ttm_bind(struct ttm_backend *backend, struct ttm_mem_reg *bo_mem)
{
	return 0;
}

static int vmw_ttm_unbind(struct ttm_backend *backend)
{
	return 0;
}

static void vmw_ttm_clear(struct ttm_backend *backend)
{
}

static void vmw_ttm_destroy(struct ttm_backend *backend)
{
	struct vmw_ttm_backend *vmw_be =
	    container_of(backend, struct vmw_ttm_backend, backend);

	kfree(vmw_be);
}

static struct ttm_backend_func vmw_ttm_func = {
	.populate = vmw_ttm_populate,
	.clear = vmw_ttm_clear,
	.bind = vmw_ttm_bind,
	.unbind = vmw_ttm_unbind,
	.destroy = vmw_ttm_destroy,
};

struct ttm_backend *vmw_ttm_backend_init(struct ttm_bo_device *bdev)
{
	struct vmw_ttm_backend *vmw_be;

	vmw_be = kmalloc(sizeof(*vmw_be), GFP_KERNEL);
	if (!vmw_be)
		return NULL;

	vmw_be->backend.func = &vmw_ttm_func;

	return &vmw_be->backend;
}

int vmw_invalidate_caches(struct ttm_bo_device *bdev, uint32_t flags)
{
	return 0;
}

int vmw_init_mem_type(struct ttm_bo_device *bdev, uint32_t type,
		      struct ttm_mem_type_manager *man)
{
	switch (type) {
	case TTM_PL_SYSTEM:
		/* System memory */

		man->flags = TTM_MEMTYPE_FLAG_MAPPABLE;
		man->available_caching = TTM_PL_MASK_CACHING;
		man->default_caching = TTM_PL_FLAG_CACHED;
		break;
	case TTM_PL_VRAM:
		/* "On-card" video ram */
		man->gpu_offset = 0;
		man->flags = TTM_MEMTYPE_FLAG_FIXED | TTM_MEMTYPE_FLAG_MAPPABLE;
		man->available_caching = TTM_PL_MASK_CACHING;
		man->default_caching = TTM_PL_FLAG_WC;
		break;
	default:
		DRM_ERROR("Unsupported memory type %u\n", (unsigned)type);
		return -EINVAL;
	}
	return 0;
}

void vmw_evict_flags(struct ttm_buffer_object *bo,
		     struct ttm_placement *placement)
{
	*placement = vmw_sys_placement;
}


static int vmw_verify_access(struct ttm_buffer_object *bo, struct file *filp)
{
	return 0;
}

static void vmw_move_notify(struct ttm_buffer_object *bo,
		     struct ttm_mem_reg *new_mem)
{
	if (new_mem->mem_type != TTM_PL_SYSTEM)
		vmw_dmabuf_gmr_unbind(bo);
}

static void vmw_swap_notify(struct ttm_buffer_object *bo)
{
	vmw_dmabuf_gmr_unbind(bo);
}

static int vmw_ttm_io_mem_reserve(struct ttm_bo_device *bdev, struct ttm_mem_reg *mem)
{
	struct ttm_mem_type_manager *man = &bdev->man[mem->mem_type];
	struct vmw_private *dev_priv = container_of(bdev, struct vmw_private, bdev);

	mem->bus.addr = NULL;
	mem->bus.is_iomem = false;
	mem->bus.offset = 0;
	mem->bus.size = mem->num_pages << PAGE_SHIFT;
	mem->bus.base = 0;
	if (!(man->flags & TTM_MEMTYPE_FLAG_MAPPABLE))
		return -EINVAL;
	switch (mem->mem_type) {
	case TTM_PL_SYSTEM:
		/* System memory */
		return 0;
	case TTM_PL_VRAM:
		mem->bus.offset = mem->mm_node->start << PAGE_SHIFT;
		mem->bus.base = dev_priv->vram_start;
		mem->bus.is_iomem = true;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static void vmw_ttm_io_mem_free(struct ttm_bo_device *bdev, struct ttm_mem_reg *mem)
{
}

static int vmw_ttm_fault_reserve_notify(struct ttm_buffer_object *bo)
{
	return 0;
}


static void *vmw_sync_obj_ref(void *sync_obj)
{
	return sync_obj;
}

static void vmw_sync_obj_unref(void **sync_obj)
{
	*sync_obj = NULL;
}

static int vmw_sync_obj_flush(void *sync_obj, void *sync_arg)
{
	struct vmw_private *dev_priv = (struct vmw_private *)sync_arg;

	mutex_lock(&dev_priv->hw_mutex);
	vmw_write(dev_priv, SVGA_REG_SYNC, SVGA_SYNC_GENERIC);
	mutex_unlock(&dev_priv->hw_mutex);
	return 0;
}

static bool vmw_sync_obj_signaled(void *sync_obj, void *sync_arg)
{
	struct vmw_private *dev_priv = (struct vmw_private *)sync_arg;
	uint32_t sequence = (unsigned long) sync_obj;

	return vmw_fence_signaled(dev_priv, sequence);
}

static int vmw_sync_obj_wait(void *sync_obj, void *sync_arg,
			     bool lazy, bool interruptible)
{
	struct vmw_private *dev_priv = (struct vmw_private *)sync_arg;
	uint32_t sequence = (unsigned long) sync_obj;

	return vmw_wait_fence(dev_priv, false, sequence, false, 3*HZ);
}

struct ttm_bo_driver vmw_bo_driver = {
	.create_ttm_backend_entry = vmw_ttm_backend_init,
	.invalidate_caches = vmw_invalidate_caches,
	.init_mem_type = vmw_init_mem_type,
	.evict_flags = vmw_evict_flags,
	.move = NULL,
	.verify_access = vmw_verify_access,
	.sync_obj_signaled = vmw_sync_obj_signaled,
	.sync_obj_wait = vmw_sync_obj_wait,
	.sync_obj_flush = vmw_sync_obj_flush,
	.sync_obj_unref = vmw_sync_obj_unref,
	.sync_obj_ref = vmw_sync_obj_ref,
	.move_notify = vmw_move_notify,
	.swap_notify = vmw_swap_notify,
	.fault_reserve_notify = &vmw_ttm_fault_reserve_notify,
	.io_mem_reserve = &vmw_ttm_io_mem_reserve,
	.io_mem_free = &vmw_ttm_io_mem_free,
};
