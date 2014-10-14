/*
 * Copyright (C) Sistina Software, Inc.  1997-2003 All rights reserved.
 * Copyright (C) 2004-2006 Red Hat, Inc.  All rights reserved.
 *
 * This copyrighted material is made available to anyone wishing to use,
 * modify, copy, or redistribute it subject to the terms and conditions
 * of the GNU General Public License version 2.
 */

#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/completion.h>
#include <linux/buffer_head.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/gfs2_ondisk.h>
#include <linux/lm_interface.h>

#include "gfs2.h"
#include "incore.h"
#include "bmap.h"
#include "glock.h"
#include "inode.h"
#include "ops_vm.h"
#include "quota.h"
#include "rgrp.h"
#include "trans.h"
#include "util.h"

static int gfs2_private_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
	struct gfs2_inode *ip = GFS2_I(vma->vm_file->f_mapping->host);

	set_bit(GIF_PAGED, &ip->i_flags);
	return filemap_fault(vma, vmf);
}

static int alloc_page_backing(struct gfs2_inode *ip, struct page *page)
{
	struct gfs2_sbd *sdp = GFS2_SB(&ip->i_inode);
	unsigned long index = page->index;
	u64 lblock = index << (PAGE_CACHE_SHIFT -
				    sdp->sd_sb.sb_bsize_shift);
	unsigned int blocks = PAGE_CACHE_SIZE >> sdp->sd_sb.sb_bsize_shift;
	struct gfs2_alloc *al;
	unsigned int data_blocks, ind_blocks;
	unsigned int x;
	int error;

	al = gfs2_alloc_get(ip);

	error = gfs2_quota_lock(ip, NO_QUOTA_CHANGE, NO_QUOTA_CHANGE);
	if (error)
		goto out;

	error = gfs2_quota_check(ip, ip->i_inode.i_uid, ip->i_inode.i_gid);
	if (error)
		goto out_gunlock_q;

	gfs2_write_calc_reserv(ip, PAGE_CACHE_SIZE, &data_blocks, &ind_blocks);

	al->al_requested = data_blocks + ind_blocks;

	error = gfs2_inplace_reserve(ip);
	if (error)
		goto out_gunlock_q;

	error = gfs2_trans_begin(sdp, al->al_rgd->rd_ri.ri_length +
				 ind_blocks + RES_DINODE +
				 RES_STATFS + RES_QUOTA, 0);
	if (error)
		goto out_ipres;

	if (gfs2_is_stuffed(ip)) {
		error = gfs2_unstuff_dinode(ip, NULL);
		if (error)
			goto out_trans;
	}

	for (x = 0; x < blocks; ) {
		u64 dblock;
		unsigned int extlen;
		int new = 1;

		error = gfs2_extent_map(&ip->i_inode, lblock, &new, &dblock, &extlen);
		if (error)
			goto out_trans;

		lblock += extlen;
		x += extlen;
	}

	gfs2_assert_warn(sdp, al->al_alloced);

out_trans:
	gfs2_trans_end(sdp);
out_ipres:
	gfs2_inplace_release(ip);
out_gunlock_q:
	gfs2_quota_unlock(ip);
out:
	gfs2_alloc_put(ip);
	return error;
}

static int gfs2_sharewrite_fault(struct vm_area_struct *vma,
						struct vm_fault *vmf)
{
	struct file *file = vma->vm_file;
	struct gfs2_file *gf = file->private_data;
	struct gfs2_inode *ip = GFS2_I(file->f_mapping->host);
	struct gfs2_holder i_gh;
	int alloc_required;
	int error;
	int ret = 0;

	error = gfs2_glock_nq_init(ip->i_gl, LM_ST_EXCLUSIVE, 0, &i_gh);
	if (error)
		goto out;

	set_bit(GIF_PAGED, &ip->i_flags);
	set_bit(GIF_SW_PAGED, &ip->i_flags);

	error = gfs2_write_alloc_required(ip,
					(u64)vmf->pgoff << PAGE_CACHE_SHIFT,
					PAGE_CACHE_SIZE, &alloc_required);
	if (error) {
		ret = VM_FAULT_OOM; /* XXX: are these right? */
		goto out_unlock;
	}

	set_bit(GFF_EXLOCK, &gf->f_flags);
	ret = filemap_fault(vma, vmf);
	clear_bit(GFF_EXLOCK, &gf->f_flags);
	if (ret & VM_FAULT_ERROR)
		goto out_unlock;

	if (alloc_required) {
		/* XXX: do we need to drop page lock around alloc_page_backing?*/
		error = alloc_page_backing(ip, vmf->page);
		if (error) {
			/*
			 * VM_FAULT_LOCKED should always be the case for
			 * filemap_fault, but it may not be in a future
			 * implementation.
			 */
			if (ret & VM_FAULT_LOCKED)
				unlock_page(vmf->page);
			page_cache_release(vmf->page);
			ret = VM_FAULT_OOM;
			goto out_unlock;
		}
		set_page_dirty(vmf->page);
	}

out_unlock:
	gfs2_glock_dq_uninit(&i_gh);
out:
	return ret;
}

struct vm_operations_struct gfs2_vm_ops_private = {
	.fault = gfs2_private_fault,
};

struct vm_operations_struct gfs2_vm_ops_sharewrite = {
	.fault = gfs2_sharewrite_fault,
};

