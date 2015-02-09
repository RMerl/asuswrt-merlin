/*
 * inode.c - NILFS inode operations.
 *
 * Copyright (C) 2005-2008 Nippon Telegraph and Telephone Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Written by Ryusuke Konishi <ryusuke@osrg.net>
 *
 */

#include <linux/buffer_head.h>
#include <linux/gfp.h>
#include <linux/mpage.h>
#include <linux/writeback.h>
#include <linux/uio.h>
#include "nilfs.h"
#include "btnode.h"
#include "segment.h"
#include "page.h"
#include "mdt.h"
#include "cpfile.h"
#include "ifile.h"


/**
 * nilfs_get_block() - get a file block on the filesystem (callback function)
 * @inode - inode struct of the target file
 * @blkoff - file block number
 * @bh_result - buffer head to be mapped on
 * @create - indicate whether allocating the block or not when it has not
 *      been allocated yet.
 *
 * This function does not issue actual read request of the specified data
 * block. It is done by VFS.
 */
int nilfs_get_block(struct inode *inode, sector_t blkoff,
		    struct buffer_head *bh_result, int create)
{
	struct nilfs_inode_info *ii = NILFS_I(inode);
	__u64 blknum = 0;
	int err = 0, ret;
	struct inode *dat = nilfs_dat_inode(NILFS_I_NILFS(inode));
	unsigned maxblocks = bh_result->b_size >> inode->i_blkbits;

	down_read(&NILFS_MDT(dat)->mi_sem);
	ret = nilfs_bmap_lookup_contig(ii->i_bmap, blkoff, &blknum, maxblocks);
	up_read(&NILFS_MDT(dat)->mi_sem);
	if (ret >= 0) {	/* found */
		map_bh(bh_result, inode->i_sb, blknum);
		if (ret > 0)
			bh_result->b_size = (ret << inode->i_blkbits);
		goto out;
	}
	/* data block was not found */
	if (ret == -ENOENT && create) {
		struct nilfs_transaction_info ti;

		bh_result->b_blocknr = 0;
		err = nilfs_transaction_begin(inode->i_sb, &ti, 1);
		if (unlikely(err))
			goto out;
		err = nilfs_bmap_insert(ii->i_bmap, (unsigned long)blkoff,
					(unsigned long)bh_result);
		if (unlikely(err != 0)) {
			if (err == -EEXIST) {
				/*
				 * The get_block() function could be called
				 * from multiple callers for an inode.
				 * However, the page having this block must
				 * be locked in this case.
				 */
				printk(KERN_WARNING
				       "nilfs_get_block: a race condition "
				       "while inserting a data block. "
				       "(inode number=%lu, file block "
				       "offset=%llu)\n",
				       inode->i_ino,
				       (unsigned long long)blkoff);
				err = 0;
			} else if (err == -EINVAL) {
				nilfs_error(inode->i_sb, __func__,
					    "broken bmap (inode=%lu)\n",
					    inode->i_ino);
				err = -EIO;
			}
			nilfs_transaction_abort(inode->i_sb);
			goto out;
		}
		nilfs_mark_inode_dirty(inode);
		nilfs_transaction_commit(inode->i_sb); /* never fails */
		/* Error handling should be detailed */
		set_buffer_new(bh_result);
		map_bh(bh_result, inode->i_sb, 0); /* dbn must be changed
						      to proper value */
	} else if (ret == -ENOENT) {
		/* not found is not error (e.g. hole); must return without
		   the mapped state flag. */
		;
	} else {
		err = ret;
	}

 out:
	return err;
}

/**
 * nilfs_readpage() - implement readpage() method of nilfs_aops {}
 * address_space_operations.
 * @file - file struct of the file to be read
 * @page - the page to be read
 */
static int nilfs_readpage(struct file *file, struct page *page)
{
	return mpage_readpage(page, nilfs_get_block);
}

/**
 * nilfs_readpages() - implement readpages() method of nilfs_aops {}
 * address_space_operations.
 * @file - file struct of the file to be read
 * @mapping - address_space struct used for reading multiple pages
 * @pages - the pages to be read
 * @nr_pages - number of pages to be read
 */
static int nilfs_readpages(struct file *file, struct address_space *mapping,
			   struct list_head *pages, unsigned nr_pages)
{
	return mpage_readpages(mapping, pages, nr_pages, nilfs_get_block);
}

static int nilfs_writepages(struct address_space *mapping,
			    struct writeback_control *wbc)
{
	struct inode *inode = mapping->host;
	int err = 0;

	if (wbc->sync_mode == WB_SYNC_ALL)
		err = nilfs_construct_dsync_segment(inode->i_sb, inode,
						    wbc->range_start,
						    wbc->range_end);
	return err;
}

static int nilfs_writepage(struct page *page, struct writeback_control *wbc)
{
	struct inode *inode = page->mapping->host;
	int err;

	redirty_page_for_writepage(wbc, page);
	unlock_page(page);

	if (wbc->sync_mode == WB_SYNC_ALL) {
		err = nilfs_construct_segment(inode->i_sb);
		if (unlikely(err))
			return err;
	} else if (wbc->for_reclaim)
		nilfs_flush_segment(inode->i_sb, inode->i_ino);

	return 0;
}

static int nilfs_set_page_dirty(struct page *page)
{
	int ret = __set_page_dirty_buffers(page);

	if (ret) {
		struct inode *inode = page->mapping->host;
		struct nilfs_sb_info *sbi = NILFS_SB(inode->i_sb);
		unsigned nr_dirty = 1 << (PAGE_SHIFT - inode->i_blkbits);

		nilfs_set_file_dirty(sbi, inode, nr_dirty);
	}
	return ret;
}

static int nilfs_write_begin(struct file *file, struct address_space *mapping,
			     loff_t pos, unsigned len, unsigned flags,
			     struct page **pagep, void **fsdata)

{
	struct inode *inode = mapping->host;
	int err = nilfs_transaction_begin(inode->i_sb, NULL, 1);

	if (unlikely(err))
		return err;

	err = block_write_begin(mapping, pos, len, flags, pagep,
				nilfs_get_block);
	if (unlikely(err)) {
		loff_t isize = mapping->host->i_size;
		if (pos + len > isize)
			vmtruncate(mapping->host, isize);

		nilfs_transaction_abort(inode->i_sb);
	}
	return err;
}

static int nilfs_write_end(struct file *file, struct address_space *mapping,
			   loff_t pos, unsigned len, unsigned copied,
			   struct page *page, void *fsdata)
{
	struct inode *inode = mapping->host;
	unsigned start = pos & (PAGE_CACHE_SIZE - 1);
	unsigned nr_dirty;
	int err;

	nr_dirty = nilfs_page_count_clean_buffers(page, start,
						  start + copied);
	copied = generic_write_end(file, mapping, pos, len, copied, page,
				   fsdata);
	nilfs_set_file_dirty(NILFS_SB(inode->i_sb), inode, nr_dirty);
	err = nilfs_transaction_commit(inode->i_sb);
	return err ? : copied;
}

static ssize_t
nilfs_direct_IO(int rw, struct kiocb *iocb, const struct iovec *iov,
		loff_t offset, unsigned long nr_segs)
{
	struct file *file = iocb->ki_filp;
	struct inode *inode = file->f_mapping->host;
	ssize_t size;

	if (rw == WRITE)
		return 0;

	/* Needs synchronization with the cleaner */
	size = blockdev_direct_IO(rw, iocb, inode, inode->i_sb->s_bdev, iov,
				  offset, nr_segs, nilfs_get_block, NULL);

	/*
	 * In case of error extending write may have instantiated a few
	 * blocks outside i_size. Trim these off again.
	 */
	if (unlikely((rw & WRITE) && size < 0)) {
		loff_t isize = i_size_read(inode);
		loff_t end = offset + iov_length(iov, nr_segs);

		if (end > isize)
			vmtruncate(inode, isize);
	}

	return size;
}

const struct address_space_operations nilfs_aops = {
	.writepage		= nilfs_writepage,
	.readpage		= nilfs_readpage,
	.sync_page		= block_sync_page,
	.writepages		= nilfs_writepages,
	.set_page_dirty		= nilfs_set_page_dirty,
	.readpages		= nilfs_readpages,
	.write_begin		= nilfs_write_begin,
	.write_end		= nilfs_write_end,
	/* .releasepage		= nilfs_releasepage, */
	.invalidatepage		= block_invalidatepage,
	.direct_IO		= nilfs_direct_IO,
	.is_partially_uptodate  = block_is_partially_uptodate,
};

struct inode *nilfs_new_inode(struct inode *dir, int mode)
{
	struct super_block *sb = dir->i_sb;
	struct nilfs_sb_info *sbi = NILFS_SB(sb);
	struct inode *inode;
	struct nilfs_inode_info *ii;
	int err = -ENOMEM;
	ino_t ino;

	inode = new_inode(sb);
	if (unlikely(!inode))
		goto failed;

	mapping_set_gfp_mask(inode->i_mapping,
			     mapping_gfp_mask(inode->i_mapping) & ~__GFP_FS);

	ii = NILFS_I(inode);
	ii->i_state = 1 << NILFS_I_NEW;

	err = nilfs_ifile_create_inode(sbi->s_ifile, &ino, &ii->i_bh);
	if (unlikely(err))
		goto failed_ifile_create_inode;
	/* reference count of i_bh inherits from nilfs_mdt_read_block() */

	atomic_inc(&sbi->s_inodes_count);
	inode_init_owner(inode, dir, mode);
	inode->i_ino = ino;
	inode->i_mtime = inode->i_atime = inode->i_ctime = CURRENT_TIME;

	if (S_ISREG(mode) || S_ISDIR(mode) || S_ISLNK(mode)) {
		err = nilfs_bmap_read(ii->i_bmap, NULL);
		if (err < 0)
			goto failed_bmap;

		set_bit(NILFS_I_BMAP, &ii->i_state);
		/* No lock is needed; iget() ensures it. */
	}

	ii->i_flags = NILFS_I(dir)->i_flags;
	if (S_ISLNK(mode))
		ii->i_flags &= ~(NILFS_IMMUTABLE_FL | NILFS_APPEND_FL);
	if (!S_ISDIR(mode))
		ii->i_flags &= ~NILFS_DIRSYNC_FL;

	/* ii->i_file_acl = 0; */
	/* ii->i_dir_acl = 0; */
	ii->i_dir_start_lookup = 0;
	ii->i_cno = 0;
	nilfs_set_inode_flags(inode);
	spin_lock(&sbi->s_next_gen_lock);
	inode->i_generation = sbi->s_next_generation++;
	spin_unlock(&sbi->s_next_gen_lock);
	insert_inode_hash(inode);

	err = nilfs_init_acl(inode, dir);
	if (unlikely(err))
		goto failed_acl; /* never occur. When supporting
				    nilfs_init_acl(), proper cancellation of
				    above jobs should be considered */

	return inode;

 failed_acl:
 failed_bmap:
	inode->i_nlink = 0;
	iput(inode);  /* raw_inode will be deleted through
			 generic_delete_inode() */
	goto failed;

 failed_ifile_create_inode:
	make_bad_inode(inode);
	iput(inode);  /* if i_nlink == 1, generic_forget_inode() will be
			 called */
 failed:
	return ERR_PTR(err);
}

void nilfs_free_inode(struct inode *inode)
{
	struct super_block *sb = inode->i_sb;
	struct nilfs_sb_info *sbi = NILFS_SB(sb);

	(void) nilfs_ifile_delete_inode(sbi->s_ifile, inode->i_ino);
	atomic_dec(&sbi->s_inodes_count);
}

void nilfs_set_inode_flags(struct inode *inode)
{
	unsigned int flags = NILFS_I(inode)->i_flags;

	inode->i_flags &= ~(S_SYNC | S_APPEND | S_IMMUTABLE | S_NOATIME |
			    S_DIRSYNC);
	if (flags & NILFS_SYNC_FL)
		inode->i_flags |= S_SYNC;
	if (flags & NILFS_APPEND_FL)
		inode->i_flags |= S_APPEND;
	if (flags & NILFS_IMMUTABLE_FL)
		inode->i_flags |= S_IMMUTABLE;
#ifndef NILFS_ATIME_DISABLE
	if (flags & NILFS_NOATIME_FL)
#endif
		inode->i_flags |= S_NOATIME;
	if (flags & NILFS_DIRSYNC_FL)
		inode->i_flags |= S_DIRSYNC;
	mapping_set_gfp_mask(inode->i_mapping,
			     mapping_gfp_mask(inode->i_mapping) & ~__GFP_FS);
}

int nilfs_read_inode_common(struct inode *inode,
			    struct nilfs_inode *raw_inode)
{
	struct nilfs_inode_info *ii = NILFS_I(inode);
	int err;

	inode->i_mode = le16_to_cpu(raw_inode->i_mode);
	inode->i_uid = (uid_t)le32_to_cpu(raw_inode->i_uid);
	inode->i_gid = (gid_t)le32_to_cpu(raw_inode->i_gid);
	inode->i_nlink = le16_to_cpu(raw_inode->i_links_count);
	inode->i_size = le64_to_cpu(raw_inode->i_size);
	inode->i_atime.tv_sec = le64_to_cpu(raw_inode->i_mtime);
	inode->i_ctime.tv_sec = le64_to_cpu(raw_inode->i_ctime);
	inode->i_mtime.tv_sec = le64_to_cpu(raw_inode->i_mtime);
	inode->i_atime.tv_nsec = le32_to_cpu(raw_inode->i_mtime_nsec);
	inode->i_ctime.tv_nsec = le32_to_cpu(raw_inode->i_ctime_nsec);
	inode->i_mtime.tv_nsec = le32_to_cpu(raw_inode->i_mtime_nsec);
	if (inode->i_nlink == 0 && inode->i_mode == 0)
		return -EINVAL; /* this inode is deleted */

	inode->i_blocks = le64_to_cpu(raw_inode->i_blocks);
	ii->i_flags = le32_to_cpu(raw_inode->i_flags);
	ii->i_dir_start_lookup = 0;
	ii->i_cno = 0;
	inode->i_generation = le32_to_cpu(raw_inode->i_generation);

	if (S_ISREG(inode->i_mode) || S_ISDIR(inode->i_mode) ||
	    S_ISLNK(inode->i_mode)) {
		err = nilfs_bmap_read(ii->i_bmap, raw_inode);
		if (err < 0)
			return err;
		set_bit(NILFS_I_BMAP, &ii->i_state);
		/* No lock is needed; iget() ensures it. */
	}
	return 0;
}

static int __nilfs_read_inode(struct super_block *sb, unsigned long ino,
			      struct inode *inode)
{
	struct nilfs_sb_info *sbi = NILFS_SB(sb);
	struct inode *dat = nilfs_dat_inode(sbi->s_nilfs);
	struct buffer_head *bh;
	struct nilfs_inode *raw_inode;
	int err;

	down_read(&NILFS_MDT(dat)->mi_sem);
	err = nilfs_ifile_get_inode_block(sbi->s_ifile, ino, &bh);
	if (unlikely(err))
		goto bad_inode;

	raw_inode = nilfs_ifile_map_inode(sbi->s_ifile, ino, bh);

	err = nilfs_read_inode_common(inode, raw_inode);
	if (err)
		goto failed_unmap;

	if (S_ISREG(inode->i_mode)) {
		inode->i_op = &nilfs_file_inode_operations;
		inode->i_fop = &nilfs_file_operations;
		inode->i_mapping->a_ops = &nilfs_aops;
	} else if (S_ISDIR(inode->i_mode)) {
		inode->i_op = &nilfs_dir_inode_operations;
		inode->i_fop = &nilfs_dir_operations;
		inode->i_mapping->a_ops = &nilfs_aops;
	} else if (S_ISLNK(inode->i_mode)) {
		inode->i_op = &nilfs_symlink_inode_operations;
		inode->i_mapping->a_ops = &nilfs_aops;
	} else {
		inode->i_op = &nilfs_special_inode_operations;
		init_special_inode(
			inode, inode->i_mode,
			huge_decode_dev(le64_to_cpu(raw_inode->i_device_code)));
	}
	nilfs_ifile_unmap_inode(sbi->s_ifile, ino, bh);
	brelse(bh);
	up_read(&NILFS_MDT(dat)->mi_sem);
	nilfs_set_inode_flags(inode);
	return 0;

 failed_unmap:
	nilfs_ifile_unmap_inode(sbi->s_ifile, ino, bh);
	brelse(bh);

 bad_inode:
	up_read(&NILFS_MDT(dat)->mi_sem);
	return err;
}

struct inode *nilfs_iget(struct super_block *sb, unsigned long ino)
{
	struct inode *inode;
	int err;

	inode = iget_locked(sb, ino);
	if (unlikely(!inode))
		return ERR_PTR(-ENOMEM);
	if (!(inode->i_state & I_NEW))
		return inode;

	err = __nilfs_read_inode(sb, ino, inode);
	if (unlikely(err)) {
		iget_failed(inode);
		return ERR_PTR(err);
	}
	unlock_new_inode(inode);
	return inode;
}

void nilfs_write_inode_common(struct inode *inode,
			      struct nilfs_inode *raw_inode, int has_bmap)
{
	struct nilfs_inode_info *ii = NILFS_I(inode);

	raw_inode->i_mode = cpu_to_le16(inode->i_mode);
	raw_inode->i_uid = cpu_to_le32(inode->i_uid);
	raw_inode->i_gid = cpu_to_le32(inode->i_gid);
	raw_inode->i_links_count = cpu_to_le16(inode->i_nlink);
	raw_inode->i_size = cpu_to_le64(inode->i_size);
	raw_inode->i_ctime = cpu_to_le64(inode->i_ctime.tv_sec);
	raw_inode->i_mtime = cpu_to_le64(inode->i_mtime.tv_sec);
	raw_inode->i_ctime_nsec = cpu_to_le32(inode->i_ctime.tv_nsec);
	raw_inode->i_mtime_nsec = cpu_to_le32(inode->i_mtime.tv_nsec);
	raw_inode->i_blocks = cpu_to_le64(inode->i_blocks);

	raw_inode->i_flags = cpu_to_le32(ii->i_flags);
	raw_inode->i_generation = cpu_to_le32(inode->i_generation);

	if (has_bmap)
		nilfs_bmap_write(ii->i_bmap, raw_inode);
	else if (S_ISCHR(inode->i_mode) || S_ISBLK(inode->i_mode))
		raw_inode->i_device_code =
			cpu_to_le64(huge_encode_dev(inode->i_rdev));
	/* When extending inode, nilfs->ns_inode_size should be checked
	   for substitutions of appended fields */
}

void nilfs_update_inode(struct inode *inode, struct buffer_head *ibh)
{
	ino_t ino = inode->i_ino;
	struct nilfs_inode_info *ii = NILFS_I(inode);
	struct super_block *sb = inode->i_sb;
	struct nilfs_sb_info *sbi = NILFS_SB(sb);
	struct nilfs_inode *raw_inode;

	raw_inode = nilfs_ifile_map_inode(sbi->s_ifile, ino, ibh);

	if (test_and_clear_bit(NILFS_I_NEW, &ii->i_state))
		memset(raw_inode, 0, NILFS_MDT(sbi->s_ifile)->mi_entry_size);
	set_bit(NILFS_I_INODE_DIRTY, &ii->i_state);

	nilfs_write_inode_common(inode, raw_inode, 0);
	nilfs_ifile_unmap_inode(sbi->s_ifile, ino, ibh);
}

#define NILFS_MAX_TRUNCATE_BLOCKS	16384  /* 64MB for 4KB block */

static void nilfs_truncate_bmap(struct nilfs_inode_info *ii,
				unsigned long from)
{
	unsigned long b;
	int ret;

	if (!test_bit(NILFS_I_BMAP, &ii->i_state))
		return;
 repeat:
	ret = nilfs_bmap_last_key(ii->i_bmap, &b);
	if (ret == -ENOENT)
		return;
	else if (ret < 0)
		goto failed;

	if (b < from)
		return;

	b -= min_t(unsigned long, NILFS_MAX_TRUNCATE_BLOCKS, b - from);
	ret = nilfs_bmap_truncate(ii->i_bmap, b);
	nilfs_relax_pressure_in_lock(ii->vfs_inode.i_sb);
	if (!ret || (ret == -ENOMEM &&
		     nilfs_bmap_truncate(ii->i_bmap, b) == 0))
		goto repeat;

 failed:
	if (ret == -EINVAL)
		nilfs_error(ii->vfs_inode.i_sb, __func__,
			    "bmap is broken (ino=%lu)", ii->vfs_inode.i_ino);
	else
		nilfs_warning(ii->vfs_inode.i_sb, __func__,
			      "failed to truncate bmap (ino=%lu, err=%d)",
			      ii->vfs_inode.i_ino, ret);
}

void nilfs_truncate(struct inode *inode)
{
	unsigned long blkoff;
	unsigned int blocksize;
	struct nilfs_transaction_info ti;
	struct super_block *sb = inode->i_sb;
	struct nilfs_inode_info *ii = NILFS_I(inode);

	if (!test_bit(NILFS_I_BMAP, &ii->i_state))
		return;
	if (IS_APPEND(inode) || IS_IMMUTABLE(inode))
		return;

	blocksize = sb->s_blocksize;
	blkoff = (inode->i_size + blocksize - 1) >> sb->s_blocksize_bits;
	nilfs_transaction_begin(sb, &ti, 0); /* never fails */

	block_truncate_page(inode->i_mapping, inode->i_size, nilfs_get_block);

	nilfs_truncate_bmap(ii, blkoff);

	inode->i_mtime = inode->i_ctime = CURRENT_TIME;
	if (IS_SYNC(inode))
		nilfs_set_transaction_flag(NILFS_TI_SYNC);

	nilfs_mark_inode_dirty(inode);
	nilfs_set_file_dirty(NILFS_SB(sb), inode, 0);
	nilfs_transaction_commit(sb);
	/* May construct a logical segment and may fail in sync mode.
	   But truncate has no return value. */
}

static void nilfs_clear_inode(struct inode *inode)
{
	struct nilfs_inode_info *ii = NILFS_I(inode);

	/*
	 * Free resources allocated in nilfs_read_inode(), here.
	 */
	BUG_ON(!list_empty(&ii->i_dirty));
	brelse(ii->i_bh);
	ii->i_bh = NULL;

	if (test_bit(NILFS_I_BMAP, &ii->i_state))
		nilfs_bmap_clear(ii->i_bmap);

	nilfs_btnode_cache_clear(&ii->i_btnode_cache);
}

void nilfs_evict_inode(struct inode *inode)
{
	struct nilfs_transaction_info ti;
	struct super_block *sb = inode->i_sb;
	struct nilfs_inode_info *ii = NILFS_I(inode);

	if (inode->i_nlink || unlikely(is_bad_inode(inode))) {
		if (inode->i_data.nrpages)
			truncate_inode_pages(&inode->i_data, 0);
		end_writeback(inode);
		nilfs_clear_inode(inode);
		return;
	}
	nilfs_transaction_begin(sb, &ti, 0); /* never fails */

	if (inode->i_data.nrpages)
		truncate_inode_pages(&inode->i_data, 0);

	nilfs_truncate_bmap(ii, 0);
	nilfs_mark_inode_dirty(inode);
	end_writeback(inode);
	nilfs_clear_inode(inode);
	nilfs_free_inode(inode);
	/* nilfs_free_inode() marks inode buffer dirty */
	if (IS_SYNC(inode))
		nilfs_set_transaction_flag(NILFS_TI_SYNC);
	nilfs_transaction_commit(sb);
	/* May construct a logical segment and may fail in sync mode.
	   But delete_inode has no return value. */
}

int nilfs_setattr(struct dentry *dentry, struct iattr *iattr)
{
	struct nilfs_transaction_info ti;
	struct inode *inode = dentry->d_inode;
	struct super_block *sb = inode->i_sb;
	int err;

	err = inode_change_ok(inode, iattr);
	if (err)
		return err;

	err = nilfs_transaction_begin(sb, &ti, 0);
	if (unlikely(err))
		return err;

	if ((iattr->ia_valid & ATTR_SIZE) &&
	    iattr->ia_size != i_size_read(inode)) {
		err = vmtruncate(inode, iattr->ia_size);
		if (unlikely(err))
			goto out_err;
	}

	setattr_copy(inode, iattr);
	mark_inode_dirty(inode);

	if (iattr->ia_valid & ATTR_MODE) {
		err = nilfs_acl_chmod(inode);
		if (unlikely(err))
			goto out_err;
	}

	return nilfs_transaction_commit(sb);

out_err:
	nilfs_transaction_abort(sb);
	return err;
}

int nilfs_load_inode_block(struct nilfs_sb_info *sbi, struct inode *inode,
			   struct buffer_head **pbh)
{
	struct nilfs_inode_info *ii = NILFS_I(inode);
	int err;

	spin_lock(&sbi->s_inode_lock);
	if (ii->i_bh == NULL) {
		spin_unlock(&sbi->s_inode_lock);
		err = nilfs_ifile_get_inode_block(sbi->s_ifile, inode->i_ino,
						  pbh);
		if (unlikely(err))
			return err;
		spin_lock(&sbi->s_inode_lock);
		if (ii->i_bh == NULL)
			ii->i_bh = *pbh;
		else {
			brelse(*pbh);
			*pbh = ii->i_bh;
		}
	} else
		*pbh = ii->i_bh;

	get_bh(*pbh);
	spin_unlock(&sbi->s_inode_lock);
	return 0;
}

int nilfs_inode_dirty(struct inode *inode)
{
	struct nilfs_inode_info *ii = NILFS_I(inode);
	struct nilfs_sb_info *sbi = NILFS_SB(inode->i_sb);
	int ret = 0;

	if (!list_empty(&ii->i_dirty)) {
		spin_lock(&sbi->s_inode_lock);
		ret = test_bit(NILFS_I_DIRTY, &ii->i_state) ||
			test_bit(NILFS_I_BUSY, &ii->i_state);
		spin_unlock(&sbi->s_inode_lock);
	}
	return ret;
}

int nilfs_set_file_dirty(struct nilfs_sb_info *sbi, struct inode *inode,
			 unsigned nr_dirty)
{
	struct nilfs_inode_info *ii = NILFS_I(inode);

	atomic_add(nr_dirty, &sbi->s_nilfs->ns_ndirtyblks);

	if (test_and_set_bit(NILFS_I_DIRTY, &ii->i_state))
		return 0;

	spin_lock(&sbi->s_inode_lock);
	if (!test_bit(NILFS_I_QUEUED, &ii->i_state) &&
	    !test_bit(NILFS_I_BUSY, &ii->i_state)) {
		/* Because this routine may race with nilfs_dispose_list(),
		   we have to check NILFS_I_QUEUED here, too. */
		if (list_empty(&ii->i_dirty) && igrab(inode) == NULL) {
			/* This will happen when somebody is freeing
			   this inode. */
			nilfs_warning(sbi->s_super, __func__,
				      "cannot get inode (ino=%lu)\n",
				      inode->i_ino);
			spin_unlock(&sbi->s_inode_lock);
			return -EINVAL; /* NILFS_I_DIRTY may remain for
					   freeing inode */
		}
		list_del(&ii->i_dirty);
		list_add_tail(&ii->i_dirty, &sbi->s_dirty_files);
		set_bit(NILFS_I_QUEUED, &ii->i_state);
	}
	spin_unlock(&sbi->s_inode_lock);
	return 0;
}

int nilfs_mark_inode_dirty(struct inode *inode)
{
	struct nilfs_sb_info *sbi = NILFS_SB(inode->i_sb);
	struct buffer_head *ibh;
	int err;

	err = nilfs_load_inode_block(sbi, inode, &ibh);
	if (unlikely(err)) {
		nilfs_warning(inode->i_sb, __func__,
			      "failed to reget inode block.\n");
		return err;
	}
	nilfs_update_inode(inode, ibh);
	nilfs_mdt_mark_buffer_dirty(ibh);
	nilfs_mdt_mark_dirty(sbi->s_ifile);
	brelse(ibh);
	return 0;
}

/**
 * nilfs_dirty_inode - reflect changes on given inode to an inode block.
 * @inode: inode of the file to be registered.
 *
 * nilfs_dirty_inode() loads a inode block containing the specified
 * @inode and copies data from a nilfs_inode to a corresponding inode
 * entry in the inode block. This operation is excluded from the segment
 * construction. This function can be called both as a single operation
 * and as a part of indivisible file operations.
 */
void nilfs_dirty_inode(struct inode *inode)
{
	struct nilfs_transaction_info ti;

	if (is_bad_inode(inode)) {
		nilfs_warning(inode->i_sb, __func__,
			      "tried to mark bad_inode dirty. ignored.\n");
		dump_stack();
		return;
	}
	nilfs_transaction_begin(inode->i_sb, &ti, 0);
	nilfs_mark_inode_dirty(inode);
	nilfs_transaction_commit(inode->i_sb); /* never fails */
}
