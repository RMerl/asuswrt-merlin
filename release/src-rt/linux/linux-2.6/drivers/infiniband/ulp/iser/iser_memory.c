/*
 * Copyright (c) 2004, 2005, 2006 Voltaire, Inc. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *	- Redistributions of source code must retain the above
 *	  copyright notice, this list of conditions and the following
 *	  disclaimer.
 *
 *	- Redistributions in binary form must reproduce the above
 *	  copyright notice, this list of conditions and the following
 *	  disclaimer in the documentation and/or other materials
 *	  provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/highmem.h>
#include <linux/scatterlist.h>

#include "iscsi_iser.h"

#define ISER_KMALLOC_THRESHOLD 0x20000 /* 128K - kmalloc limit */

/**
 * iser_start_rdma_unaligned_sg
 */
static int iser_start_rdma_unaligned_sg(struct iscsi_iser_task *iser_task,
					enum iser_data_dir cmd_dir)
{
	int dma_nents;
	struct ib_device *dev;
	char *mem = NULL;
	struct iser_data_buf *data = &iser_task->data[cmd_dir];
	unsigned long  cmd_data_len = data->data_len;

	if (cmd_data_len > ISER_KMALLOC_THRESHOLD)
		mem = (void *)__get_free_pages(GFP_ATOMIC,
		      ilog2(roundup_pow_of_two(cmd_data_len)) - PAGE_SHIFT);
	else
		mem = kmalloc(cmd_data_len, GFP_ATOMIC);

	if (mem == NULL) {
		iser_err("Failed to allocate mem size %d %d for copying sglist\n",
			 data->size,(int)cmd_data_len);
		return -ENOMEM;
	}

	if (cmd_dir == ISER_DIR_OUT) {
		/* copy the unaligned sg the buffer which is used for RDMA */
		struct scatterlist *sgl = (struct scatterlist *)data->buf;
		struct scatterlist *sg;
		int i;
		char *p, *from;

		p = mem;
		for_each_sg(sgl, sg, data->size, i) {
			from = kmap_atomic(sg_page(sg), KM_USER0);
			memcpy(p,
			       from + sg->offset,
			       sg->length);
			kunmap_atomic(from, KM_USER0);
			p += sg->length;
		}
	}

	sg_init_one(&iser_task->data_copy[cmd_dir].sg_single, mem, cmd_data_len);
	iser_task->data_copy[cmd_dir].buf  =
		&iser_task->data_copy[cmd_dir].sg_single;
	iser_task->data_copy[cmd_dir].size = 1;

	iser_task->data_copy[cmd_dir].copy_buf  = mem;

	dev = iser_task->iser_conn->ib_conn->device->ib_device;
	dma_nents = ib_dma_map_sg(dev,
				  &iser_task->data_copy[cmd_dir].sg_single,
				  1,
				  (cmd_dir == ISER_DIR_OUT) ?
				  DMA_TO_DEVICE : DMA_FROM_DEVICE);
	BUG_ON(dma_nents == 0);

	iser_task->data_copy[cmd_dir].dma_nents = dma_nents;
	return 0;
}

/**
 * iser_finalize_rdma_unaligned_sg
 */
void iser_finalize_rdma_unaligned_sg(struct iscsi_iser_task *iser_task,
				     enum iser_data_dir         cmd_dir)
{
	struct ib_device *dev;
	struct iser_data_buf *mem_copy;
	unsigned long  cmd_data_len;

	dev = iser_task->iser_conn->ib_conn->device->ib_device;
	mem_copy = &iser_task->data_copy[cmd_dir];

	ib_dma_unmap_sg(dev, &mem_copy->sg_single, 1,
			(cmd_dir == ISER_DIR_OUT) ?
			DMA_TO_DEVICE : DMA_FROM_DEVICE);

	if (cmd_dir == ISER_DIR_IN) {
		char *mem;
		struct scatterlist *sgl, *sg;
		unsigned char *p, *to;
		unsigned int sg_size;
		int i;

		/* copy back read RDMA to unaligned sg */
		mem	= mem_copy->copy_buf;

		sgl	= (struct scatterlist *)iser_task->data[ISER_DIR_IN].buf;
		sg_size = iser_task->data[ISER_DIR_IN].size;

		p = mem;
		for_each_sg(sgl, sg, sg_size, i) {
			to = kmap_atomic(sg_page(sg), KM_SOFTIRQ0);
			memcpy(to + sg->offset,
			       p,
			       sg->length);
			kunmap_atomic(to, KM_SOFTIRQ0);
			p += sg->length;
		}
	}

	cmd_data_len = iser_task->data[cmd_dir].data_len;

	if (cmd_data_len > ISER_KMALLOC_THRESHOLD)
		free_pages((unsigned long)mem_copy->copy_buf,
			   ilog2(roundup_pow_of_two(cmd_data_len)) - PAGE_SHIFT);
	else
		kfree(mem_copy->copy_buf);

	mem_copy->copy_buf = NULL;
}

#define IS_4K_ALIGNED(addr)	((((unsigned long)addr) & ~MASK_4K) == 0)

/**
 * iser_sg_to_page_vec - Translates scatterlist entries to physical addresses
 * and returns the length of resulting physical address array (may be less than
 * the original due to possible compaction).
 *
 * we build a "page vec" under the assumption that the SG meets the RDMA
 * alignment requirements. Other then the first and last SG elements, all
 * the "internal" elements can be compacted into a list whose elements are
 * dma addresses of physical pages. The code supports also the weird case
 * where --few fragments of the same page-- are present in the SG as
 * consecutive elements. Also, it handles one entry SG.
 */

static int iser_sg_to_page_vec(struct iser_data_buf *data,
			       struct iser_page_vec *page_vec,
			       struct ib_device *ibdev)
{
	struct scatterlist *sg, *sgl = (struct scatterlist *)data->buf;
	u64 start_addr, end_addr, page, chunk_start = 0;
	unsigned long total_sz = 0;
	unsigned int dma_len;
	int i, new_chunk, cur_page, last_ent = data->dma_nents - 1;

	/* compute the offset of first element */
	page_vec->offset = (u64) sgl[0].offset & ~MASK_4K;

	new_chunk = 1;
	cur_page  = 0;
	for_each_sg(sgl, sg, data->dma_nents, i) {
		start_addr = ib_sg_dma_address(ibdev, sg);
		if (new_chunk)
			chunk_start = start_addr;
		dma_len = ib_sg_dma_len(ibdev, sg);
		end_addr = start_addr + dma_len;
		total_sz += dma_len;

		/* collect page fragments until aligned or end of SG list */
		if (!IS_4K_ALIGNED(end_addr) && i < last_ent) {
			new_chunk = 0;
			continue;
		}
		new_chunk = 1;

		/* address of the first page in the contiguous chunk;
		   masking relevant for the very first SG entry,
		   which might be unaligned */
		page = chunk_start & MASK_4K;
		do {
			page_vec->pages[cur_page++] = page;
			page += SIZE_4K;
		} while (page < end_addr);
	}

	page_vec->data_size = total_sz;
	iser_dbg("page_vec->data_size:%d cur_page %d\n", page_vec->data_size,cur_page);
	return cur_page;
}


/**
 * iser_data_buf_aligned_len - Tries to determine the maximal correctly aligned
 * for RDMA sub-list of a scatter-gather list of memory buffers, and  returns
 * the number of entries which are aligned correctly. Supports the case where
 * consecutive SG elements are actually fragments of the same physcial page.
 */
static int iser_data_buf_aligned_len(struct iser_data_buf *data,
				      struct ib_device *ibdev)
{
	struct scatterlist *sgl, *sg, *next_sg = NULL;
	u64 start_addr, end_addr;
	int i, ret_len, start_check = 0;

	if (data->dma_nents == 1)
		return 1;

	sgl = (struct scatterlist *)data->buf;
	start_addr  = ib_sg_dma_address(ibdev, sgl);

	for_each_sg(sgl, sg, data->dma_nents, i) {
		if (start_check && !IS_4K_ALIGNED(start_addr))
			break;

		next_sg = sg_next(sg);
		if (!next_sg)
			break;

		end_addr    = start_addr + ib_sg_dma_len(ibdev, sg);
		start_addr  = ib_sg_dma_address(ibdev, next_sg);

		if (end_addr == start_addr) {
			start_check = 0;
			continue;
		} else
			start_check = 1;

		if (!IS_4K_ALIGNED(end_addr))
			break;
	}
	ret_len = (next_sg) ? i : i+1;
	iser_dbg("Found %d aligned entries out of %d in sg:0x%p\n",
		 ret_len, data->dma_nents, data);
	return ret_len;
}

static void iser_data_buf_dump(struct iser_data_buf *data,
			       struct ib_device *ibdev)
{
	struct scatterlist *sgl = (struct scatterlist *)data->buf;
	struct scatterlist *sg;
	int i;

	if (iser_debug_level == 0)
		return;

	for_each_sg(sgl, sg, data->dma_nents, i)
		iser_warn("sg[%d] dma_addr:0x%lX page:0x%p "
			 "off:0x%x sz:0x%x dma_len:0x%x\n",
			 i, (unsigned long)ib_sg_dma_address(ibdev, sg),
			 sg_page(sg), sg->offset,
			 sg->length, ib_sg_dma_len(ibdev, sg));
}

static void iser_dump_page_vec(struct iser_page_vec *page_vec)
{
	int i;

	iser_err("page vec length %d data size %d\n",
		 page_vec->length, page_vec->data_size);
	for (i = 0; i < page_vec->length; i++)
		iser_err("%d %lx\n",i,(unsigned long)page_vec->pages[i]);
}

static void iser_page_vec_build(struct iser_data_buf *data,
				struct iser_page_vec *page_vec,
				struct ib_device *ibdev)
{
	int page_vec_len = 0;

	page_vec->length = 0;
	page_vec->offset = 0;

	iser_dbg("Translating sg sz: %d\n", data->dma_nents);
	page_vec_len = iser_sg_to_page_vec(data, page_vec, ibdev);
	iser_dbg("sg len %d page_vec_len %d\n", data->dma_nents,page_vec_len);

	page_vec->length = page_vec_len;

	if (page_vec_len * SIZE_4K < page_vec->data_size) {
		iser_err("page_vec too short to hold this SG\n");
		iser_data_buf_dump(data, ibdev);
		iser_dump_page_vec(page_vec);
		BUG();
	}
}

int iser_dma_map_task_data(struct iscsi_iser_task *iser_task,
			    struct iser_data_buf *data,
			    enum iser_data_dir iser_dir,
			    enum dma_data_direction dma_dir)
{
	struct ib_device *dev;

	iser_task->dir[iser_dir] = 1;
	dev = iser_task->iser_conn->ib_conn->device->ib_device;

	data->dma_nents = ib_dma_map_sg(dev, data->buf, data->size, dma_dir);
	if (data->dma_nents == 0) {
		iser_err("dma_map_sg failed!!!\n");
		return -EINVAL;
	}
	return 0;
}

void iser_dma_unmap_task_data(struct iscsi_iser_task *iser_task)
{
	struct ib_device *dev;
	struct iser_data_buf *data;

	dev = iser_task->iser_conn->ib_conn->device->ib_device;

	if (iser_task->dir[ISER_DIR_IN]) {
		data = &iser_task->data[ISER_DIR_IN];
		ib_dma_unmap_sg(dev, data->buf, data->size, DMA_FROM_DEVICE);
	}

	if (iser_task->dir[ISER_DIR_OUT]) {
		data = &iser_task->data[ISER_DIR_OUT];
		ib_dma_unmap_sg(dev, data->buf, data->size, DMA_TO_DEVICE);
	}
}

/**
 * iser_reg_rdma_mem - Registers memory intended for RDMA,
 * obtaining rkey and va
 *
 * returns 0 on success, errno code on failure
 */
int iser_reg_rdma_mem(struct iscsi_iser_task *iser_task,
		      enum   iser_data_dir        cmd_dir)
{
	struct iscsi_conn    *iscsi_conn = iser_task->iser_conn->iscsi_conn;
	struct iser_conn     *ib_conn = iser_task->iser_conn->ib_conn;
	struct iser_device   *device = ib_conn->device;
	struct ib_device     *ibdev = device->ib_device;
	struct iser_data_buf *mem = &iser_task->data[cmd_dir];
	struct iser_regd_buf *regd_buf;
	int aligned_len;
	int err;
	int i;
	struct scatterlist *sg;

	regd_buf = &iser_task->rdma_regd[cmd_dir];

	aligned_len = iser_data_buf_aligned_len(mem, ibdev);
	if (aligned_len != mem->dma_nents) {
		iscsi_conn->fmr_unalign_cnt++;
		iser_warn("rdma alignment violation %d/%d aligned\n",
			 aligned_len, mem->size);
		iser_data_buf_dump(mem, ibdev);

		/* unmap the command data before accessing it */
		iser_dma_unmap_task_data(iser_task);

		/* allocate copy buf, if we are writing, copy the */
		/* unaligned scatterlist, dma map the copy        */
		if (iser_start_rdma_unaligned_sg(iser_task, cmd_dir) != 0)
				return -ENOMEM;
		mem = &iser_task->data_copy[cmd_dir];
	}

	/* if there a single dma entry, FMR is not needed */
	if (mem->dma_nents == 1) {
		sg = (struct scatterlist *)mem->buf;

		regd_buf->reg.lkey = device->mr->lkey;
		regd_buf->reg.rkey = device->mr->rkey;
		regd_buf->reg.len  = ib_sg_dma_len(ibdev, &sg[0]);
		regd_buf->reg.va   = ib_sg_dma_address(ibdev, &sg[0]);
		regd_buf->reg.is_fmr = 0;

		iser_dbg("PHYSICAL Mem.register: lkey: 0x%08X rkey: 0x%08X  "
			 "va: 0x%08lX sz: %ld]\n",
			 (unsigned int)regd_buf->reg.lkey,
			 (unsigned int)regd_buf->reg.rkey,
			 (unsigned long)regd_buf->reg.va,
			 (unsigned long)regd_buf->reg.len);
	} else { /* use FMR for multiple dma entries */
		iser_page_vec_build(mem, ib_conn->page_vec, ibdev);
		err = iser_reg_page_vec(ib_conn, ib_conn->page_vec, &regd_buf->reg);
		if (err) {
			iser_data_buf_dump(mem, ibdev);
			iser_err("mem->dma_nents = %d (dlength = 0x%x)\n",
				 mem->dma_nents,
				 ntoh24(iser_task->desc.iscsi_header.dlength));
			iser_err("page_vec: data_size = 0x%x, length = %d, offset = 0x%x\n",
				 ib_conn->page_vec->data_size, ib_conn->page_vec->length,
				 ib_conn->page_vec->offset);
			for (i=0 ; i<ib_conn->page_vec->length ; i++)
				iser_err("page_vec[%d] = 0x%llx\n", i,
					 (unsigned long long) ib_conn->page_vec->pages[i]);
			return err;
		}
	}
	return 0;
}
