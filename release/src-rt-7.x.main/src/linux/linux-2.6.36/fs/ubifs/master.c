/*
 * This file is part of UBIFS.
 *
 * Copyright (C) 2006-2008 Nokia Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * Authors: Artem Bityutskiy (Битюцкий Артём)
 *          Adrian Hunter
 */

/* This file implements reading and writing the master node */

#include "ubifs.h"

/**
 * scan_for_master - search the valid master node.
 * @c: UBIFS file-system description object
 *
 * This function scans the master node LEBs and search for the latest master
 * node. Returns zero in case of success, %-EUCLEAN if there master area is
 * corrupted and requires recovery, and a negative error code in case of
 * failure.
 */
static int scan_for_master(struct ubifs_info *c)
{
	struct ubifs_scan_leb *sleb;
	struct ubifs_scan_node *snod;
	int lnum, offs = 0, nodes_cnt;

	lnum = UBIFS_MST_LNUM;

	sleb = ubifs_scan(c, lnum, 0, c->sbuf, 1);
	if (IS_ERR(sleb))
		return PTR_ERR(sleb);
	nodes_cnt = sleb->nodes_cnt;
	if (nodes_cnt > 0) {
		snod = list_entry(sleb->nodes.prev, struct ubifs_scan_node,
				  list);
		if (snod->type != UBIFS_MST_NODE)
			goto out_dump;
		memcpy(c->mst_node, snod->node, snod->len);
		offs = snod->offs;
	}
	ubifs_scan_destroy(sleb);

	lnum += 1;

	sleb = ubifs_scan(c, lnum, 0, c->sbuf, 1);
	if (IS_ERR(sleb))
		return PTR_ERR(sleb);
	if (sleb->nodes_cnt != nodes_cnt)
		goto out;
	if (!sleb->nodes_cnt)
		goto out;
	snod = list_entry(sleb->nodes.prev, struct ubifs_scan_node, list);
	if (snod->type != UBIFS_MST_NODE)
		goto out_dump;
	if (snod->offs != offs)
		goto out;
	if (memcmp((void *)c->mst_node + UBIFS_CH_SZ,
		   (void *)snod->node + UBIFS_CH_SZ,
		   UBIFS_MST_NODE_SZ - UBIFS_CH_SZ))
		goto out;
	c->mst_offs = offs;
	ubifs_scan_destroy(sleb);
	return 0;

out:
	ubifs_scan_destroy(sleb);
	return -EUCLEAN;

out_dump:
	ubifs_err("unexpected node type %d master LEB %d:%d",
		  snod->type, lnum, snod->offs);
	ubifs_scan_destroy(sleb);
	return -EINVAL;
}

/**
 * validate_master - validate master node.
 * @c: UBIFS file-system description object
 *
 * This function validates data which was read from master node. Returns zero
 * if the data is all right and %-EINVAL if not.
 */
static int validate_master(const struct ubifs_info *c)
{
	long long main_sz;
	int err;

	if (c->max_sqnum >= SQNUM_WATERMARK) {
		err = 1;
		goto out;
	}

	if (c->cmt_no >= c->max_sqnum) {
		err = 2;
		goto out;
	}

	if (c->highest_inum >= INUM_WATERMARK) {
		err = 3;
		goto out;
	}

	if (c->lhead_lnum < UBIFS_LOG_LNUM ||
	    c->lhead_lnum >= UBIFS_LOG_LNUM + c->log_lebs ||
	    c->lhead_offs < 0 || c->lhead_offs >= c->leb_size ||
	    c->lhead_offs & (c->min_io_size - 1)) {
		err = 4;
		goto out;
	}

	if (c->zroot.lnum >= c->leb_cnt || c->zroot.lnum < c->main_first ||
	    c->zroot.offs >= c->leb_size || c->zroot.offs & 7) {
		err = 5;
		goto out;
	}

	if (c->zroot.len < c->ranges[UBIFS_IDX_NODE].min_len ||
	    c->zroot.len > c->ranges[UBIFS_IDX_NODE].max_len) {
		err = 6;
		goto out;
	}

	if (c->gc_lnum >= c->leb_cnt || c->gc_lnum < c->main_first) {
		err = 7;
		goto out;
	}

	if (c->ihead_lnum >= c->leb_cnt || c->ihead_lnum < c->main_first ||
	    c->ihead_offs % c->min_io_size || c->ihead_offs < 0 ||
	    c->ihead_offs > c->leb_size || c->ihead_offs & 7) {
		err = 8;
		goto out;
	}

	main_sz = (long long)c->main_lebs * c->leb_size;
	if (c->old_idx_sz & 7 || c->old_idx_sz >= main_sz) {
		err = 9;
		goto out;
	}

	if (c->lpt_lnum < c->lpt_first || c->lpt_lnum > c->lpt_last ||
	    c->lpt_offs < 0 || c->lpt_offs + c->nnode_sz > c->leb_size) {
		err = 10;
		goto out;
	}

	if (c->nhead_lnum < c->lpt_first || c->nhead_lnum > c->lpt_last ||
	    c->nhead_offs < 0 || c->nhead_offs % c->min_io_size ||
	    c->nhead_offs > c->leb_size) {
		err = 11;
		goto out;
	}

	if (c->ltab_lnum < c->lpt_first || c->ltab_lnum > c->lpt_last ||
	    c->ltab_offs < 0 ||
	    c->ltab_offs + c->ltab_sz > c->leb_size) {
		err = 12;
		goto out;
	}

	if (c->big_lpt && (c->lsave_lnum < c->lpt_first ||
	    c->lsave_lnum > c->lpt_last || c->lsave_offs < 0 ||
	    c->lsave_offs + c->lsave_sz > c->leb_size)) {
		err = 13;
		goto out;
	}

	if (c->lscan_lnum < c->main_first || c->lscan_lnum >= c->leb_cnt) {
		err = 14;
		goto out;
	}

	if (c->lst.empty_lebs < 0 || c->lst.empty_lebs > c->main_lebs - 2) {
		err = 15;
		goto out;
	}

	if (c->lst.idx_lebs < 0 || c->lst.idx_lebs > c->main_lebs - 1) {
		err = 16;
		goto out;
	}

	if (c->lst.total_free < 0 || c->lst.total_free > main_sz ||
	    c->lst.total_free & 7) {
		err = 17;
		goto out;
	}

	if (c->lst.total_dirty < 0 || (c->lst.total_dirty & 7)) {
		err = 18;
		goto out;
	}

	if (c->lst.total_used < 0 || (c->lst.total_used & 7)) {
		err = 19;
		goto out;
	}

	if (c->lst.total_free + c->lst.total_dirty +
	    c->lst.total_used > main_sz) {
		err = 20;
		goto out;
	}

	if (c->lst.total_dead + c->lst.total_dark +
	    c->lst.total_used + c->old_idx_sz > main_sz) {
		err = 21;
		goto out;
	}

	if (c->lst.total_dead < 0 ||
	    c->lst.total_dead > c->lst.total_free + c->lst.total_dirty ||
	    c->lst.total_dead & 7) {
		err = 22;
		goto out;
	}

	if (c->lst.total_dark < 0 ||
	    c->lst.total_dark > c->lst.total_free + c->lst.total_dirty ||
	    c->lst.total_dark & 7) {
		err = 23;
		goto out;
	}

	return 0;

out:
	ubifs_err("bad master node at offset %d error %d", c->mst_offs, err);
	dbg_dump_node(c, c->mst_node);
	return -EINVAL;
}

/**
 * ubifs_read_master - read master node.
 * @c: UBIFS file-system description object
 *
 * This function finds and reads the master node during file-system mount. If
 * the flash is empty, it creates default master node as well. Returns zero in
 * case of success and a negative error code in case of failure.
 */
int ubifs_read_master(struct ubifs_info *c)
{
	int err, old_leb_cnt;

	c->mst_node = kzalloc(c->mst_node_alsz, GFP_KERNEL);
	if (!c->mst_node)
		return -ENOMEM;

	err = scan_for_master(c);
	if (err) {
		if (err == -EUCLEAN)
			err = ubifs_recover_master_node(c);
		if (err)
			/*
			 * Note, we do not free 'c->mst_node' here because the
			 * unmount routine will take care of this.
			 */
			return err;
	}

	/* Make sure that the recovery flag is clear */
	c->mst_node->flags &= cpu_to_le32(~UBIFS_MST_RCVRY);

	c->max_sqnum       = le64_to_cpu(c->mst_node->ch.sqnum);
	c->highest_inum    = le64_to_cpu(c->mst_node->highest_inum);
	c->cmt_no          = le64_to_cpu(c->mst_node->cmt_no);
	c->zroot.lnum      = le32_to_cpu(c->mst_node->root_lnum);
	c->zroot.offs      = le32_to_cpu(c->mst_node->root_offs);
	c->zroot.len       = le32_to_cpu(c->mst_node->root_len);
	c->lhead_lnum      = le32_to_cpu(c->mst_node->log_lnum);
	c->gc_lnum         = le32_to_cpu(c->mst_node->gc_lnum);
	c->ihead_lnum      = le32_to_cpu(c->mst_node->ihead_lnum);
	c->ihead_offs      = le32_to_cpu(c->mst_node->ihead_offs);
	c->old_idx_sz      = le64_to_cpu(c->mst_node->index_size);
	c->lpt_lnum        = le32_to_cpu(c->mst_node->lpt_lnum);
	c->lpt_offs        = le32_to_cpu(c->mst_node->lpt_offs);
	c->nhead_lnum      = le32_to_cpu(c->mst_node->nhead_lnum);
	c->nhead_offs      = le32_to_cpu(c->mst_node->nhead_offs);
	c->ltab_lnum       = le32_to_cpu(c->mst_node->ltab_lnum);
	c->ltab_offs       = le32_to_cpu(c->mst_node->ltab_offs);
	c->lsave_lnum      = le32_to_cpu(c->mst_node->lsave_lnum);
	c->lsave_offs      = le32_to_cpu(c->mst_node->lsave_offs);
	c->lscan_lnum      = le32_to_cpu(c->mst_node->lscan_lnum);
	c->lst.empty_lebs  = le32_to_cpu(c->mst_node->empty_lebs);
	c->lst.idx_lebs    = le32_to_cpu(c->mst_node->idx_lebs);
	old_leb_cnt        = le32_to_cpu(c->mst_node->leb_cnt);
	c->lst.total_free  = le64_to_cpu(c->mst_node->total_free);
	c->lst.total_dirty = le64_to_cpu(c->mst_node->total_dirty);
	c->lst.total_used  = le64_to_cpu(c->mst_node->total_used);
	c->lst.total_dead  = le64_to_cpu(c->mst_node->total_dead);
	c->lst.total_dark  = le64_to_cpu(c->mst_node->total_dark);

	c->calc_idx_sz = c->old_idx_sz;

	if (c->mst_node->flags & cpu_to_le32(UBIFS_MST_NO_ORPHS))
		c->no_orphs = 1;

	if (old_leb_cnt != c->leb_cnt) {
		/* The file system has been resized */
		int growth = c->leb_cnt - old_leb_cnt;

		if (c->leb_cnt < old_leb_cnt ||
		    c->leb_cnt < UBIFS_MIN_LEB_CNT) {
			ubifs_err("bad leb_cnt on master node");
			dbg_dump_node(c, c->mst_node);
			return -EINVAL;
		}

		dbg_mnt("Auto resizing (master) from %d LEBs to %d LEBs",
			old_leb_cnt, c->leb_cnt);
		c->lst.empty_lebs += growth;
		c->lst.total_free += growth * (long long)c->leb_size;
		c->lst.total_dark += growth * (long long)c->dark_wm;

		/*
		 * Reflect changes back onto the master node. N.B. the master
		 * node gets written immediately whenever mounting (or
		 * remounting) in read-write mode, so we do not need to write it
		 * here.
		 */
		c->mst_node->leb_cnt = cpu_to_le32(c->leb_cnt);
		c->mst_node->empty_lebs = cpu_to_le32(c->lst.empty_lebs);
		c->mst_node->total_free = cpu_to_le64(c->lst.total_free);
		c->mst_node->total_dark = cpu_to_le64(c->lst.total_dark);
	}

	err = validate_master(c);
	if (err)
		return err;

	err = dbg_old_index_check_init(c, &c->zroot);

	return err;
}

/**
 * ubifs_write_master - write master node.
 * @c: UBIFS file-system description object
 *
 * This function writes the master node. The caller has to take the
 * @c->mst_mutex lock before calling this function. Returns zero in case of
 * success and a negative error code in case of failure. The master node is
 * written twice to enable recovery.
 */
int ubifs_write_master(struct ubifs_info *c)
{
	int err, lnum, offs, len;

	if (c->ro_media)
		return -EROFS;

	lnum = UBIFS_MST_LNUM;
	offs = c->mst_offs + c->mst_node_alsz;
	len = UBIFS_MST_NODE_SZ;

	if (offs + UBIFS_MST_NODE_SZ > c->leb_size) {
		err = ubifs_leb_unmap(c, lnum);
		if (err)
			return err;
		offs = 0;
	}

	c->mst_offs = offs;
	c->mst_node->highest_inum = cpu_to_le64(c->highest_inum);

	err = ubifs_write_node(c, c->mst_node, len, lnum, offs, UBI_SHORTTERM);
	if (err)
		return err;

	lnum += 1;

	if (offs == 0) {
		err = ubifs_leb_unmap(c, lnum);
		if (err)
			return err;
	}
	err = ubifs_write_node(c, c->mst_node, len, lnum, offs, UBI_SHORTTERM);

	return err;
}
