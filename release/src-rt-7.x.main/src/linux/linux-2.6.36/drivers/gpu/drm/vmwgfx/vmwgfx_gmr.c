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
#include "drmP.h"
#include "ttm/ttm_bo_driver.h"


static int vmw_gmr_build_descriptors(struct list_head *desc_pages,
				     struct page *pages[],
				     unsigned long num_pages)
{
	struct page *page, *next;
	struct svga_guest_mem_descriptor *page_virtual = NULL;
	struct svga_guest_mem_descriptor *desc_virtual = NULL;
	unsigned int desc_per_page;
	unsigned long prev_pfn;
	unsigned long pfn;
	int ret;

	desc_per_page = PAGE_SIZE /
	    sizeof(struct svga_guest_mem_descriptor) - 1;

	while (likely(num_pages != 0)) {
		page = alloc_page(__GFP_HIGHMEM);
		if (unlikely(page == NULL)) {
			ret = -ENOMEM;
			goto out_err;
		}

		list_add_tail(&page->lru, desc_pages);

		/*
		 * Point previous page terminating descriptor to this
		 * page before unmapping it.
		 */

		if (likely(page_virtual != NULL)) {
			desc_virtual->ppn = page_to_pfn(page);
			kunmap_atomic(page_virtual, KM_USER0);
		}

		page_virtual = kmap_atomic(page, KM_USER0);
		desc_virtual = page_virtual - 1;
		prev_pfn = ~(0UL);

		while (likely(num_pages != 0)) {
			pfn = page_to_pfn(*pages);

			if (pfn != prev_pfn + 1) {

				if (desc_virtual - page_virtual ==
				    desc_per_page - 1)
					break;

				(++desc_virtual)->ppn = cpu_to_le32(pfn);
				desc_virtual->num_pages = cpu_to_le32(1);
			} else {
				uint32_t tmp =
				    le32_to_cpu(desc_virtual->num_pages);
				desc_virtual->num_pages = cpu_to_le32(tmp + 1);
			}
			prev_pfn = pfn;
			--num_pages;
			++pages;
		}

		(++desc_virtual)->ppn = cpu_to_le32(0);
		desc_virtual->num_pages = cpu_to_le32(0);
	}

	if (likely(page_virtual != NULL))
		kunmap_atomic(page_virtual, KM_USER0);

	return 0;
out_err:
	list_for_each_entry_safe(page, next, desc_pages, lru) {
		list_del_init(&page->lru);
		__free_page(page);
	}
	return ret;
}

static inline void vmw_gmr_free_descriptors(struct list_head *desc_pages)
{
	struct page *page, *next;

	list_for_each_entry_safe(page, next, desc_pages, lru) {
		list_del_init(&page->lru);
		__free_page(page);
	}
}

static void vmw_gmr_fire_descriptors(struct vmw_private *dev_priv,
				     int gmr_id, struct list_head *desc_pages)
{
	struct page *page;

	if (unlikely(list_empty(desc_pages)))
		return;

	page = list_entry(desc_pages->next, struct page, lru);

	mutex_lock(&dev_priv->hw_mutex);

	vmw_write(dev_priv, SVGA_REG_GMR_ID, gmr_id);
	wmb();
	vmw_write(dev_priv, SVGA_REG_GMR_DESCRIPTOR, page_to_pfn(page));
	mb();

	mutex_unlock(&dev_priv->hw_mutex);

}


static unsigned long vmw_gmr_count_descriptors(struct page *pages[],
					       unsigned long num_pages)
{
	unsigned long prev_pfn = ~(0UL);
	unsigned long pfn;
	unsigned long descriptors = 0;

	while (num_pages--) {
		pfn = page_to_pfn(*pages++);
		if (prev_pfn + 1 != pfn)
			++descriptors;
		prev_pfn = pfn;
	}

	return descriptors;
}

int vmw_gmr_bind(struct vmw_private *dev_priv,
		 struct ttm_buffer_object *bo)
{
	struct ttm_tt *ttm = bo->ttm;
	unsigned long descriptors;
	int ret;
	uint32_t id;
	struct list_head desc_pages;

	if (!(dev_priv->capabilities & SVGA_CAP_GMR))
		return -EINVAL;

	ret = ttm_tt_populate(ttm);
	if (unlikely(ret != 0))
		return ret;

	descriptors = vmw_gmr_count_descriptors(ttm->pages, ttm->num_pages);
	if (unlikely(descriptors > dev_priv->max_gmr_descriptors))
		return -EINVAL;

	INIT_LIST_HEAD(&desc_pages);
	ret = vmw_gmr_build_descriptors(&desc_pages, ttm->pages,
					ttm->num_pages);
	if (unlikely(ret != 0))
		return ret;

	ret = vmw_gmr_id_alloc(dev_priv, &id);
	if (unlikely(ret != 0))
		goto out_no_id;

	vmw_gmr_fire_descriptors(dev_priv, id, &desc_pages);
	vmw_gmr_free_descriptors(&desc_pages);
	vmw_dmabuf_set_gmr(bo, id);
	return 0;

out_no_id:
	vmw_gmr_free_descriptors(&desc_pages);
	return ret;
}

void vmw_gmr_unbind(struct vmw_private *dev_priv, int gmr_id)
{
	mutex_lock(&dev_priv->hw_mutex);
	vmw_write(dev_priv, SVGA_REG_GMR_ID, gmr_id);
	wmb();
	vmw_write(dev_priv, SVGA_REG_GMR_DESCRIPTOR, 0);
	mb();
	mutex_unlock(&dev_priv->hw_mutex);
}
