/*
 *  linux/fs/sysv/inode.c
 *
 *  minix/inode.c
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *
 *  xenix/inode.c
 *  Copyright (C) 1992  Doug Evans
 *
 *  coh/inode.c
 *  Copyright (C) 1993  Pascal Haible, Bruno Haible
 *
 *  sysv/inode.c
 *  Copyright (C) 1993  Paul B. Monday
 *
 *  sysv/inode.c
 *  Copyright (C) 1993  Bruno Haible
 *  Copyright (C) 1997, 1998  Krzysztof G. Baranowski
 *
 *  This file contains code for allocating/freeing inodes and for read/writing
 *  the superblock.
 */

#include <linux/highuid.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/buffer_head.h>
#include <linux/vfs.h>
#include <linux/writeback.h>
#include <linux/namei.h>
#include <asm/byteorder.h>
#include "sysv.h"

static int sysv_sync_fs(struct super_block *sb, int wait)
{
	struct sysv_sb_info *sbi = SYSV_SB(sb);
	unsigned long time = get_seconds(), old_time;

	lock_super(sb);

	/*
	 * If we are going to write out the super block,
	 * then attach current time stamp.
	 * But if the filesystem was marked clean, keep it clean.
	 */
	sb->s_dirt = 0;
	old_time = fs32_to_cpu(sbi, *sbi->s_sb_time);
	if (sbi->s_type == FSTYPE_SYSV4) {
		if (*sbi->s_sb_state == cpu_to_fs32(sbi, 0x7c269d38 - old_time))
			*sbi->s_sb_state = cpu_to_fs32(sbi, 0x7c269d38 - time);
		*sbi->s_sb_time = cpu_to_fs32(sbi, time);
		mark_buffer_dirty(sbi->s_bh2);
	}

	unlock_super(sb);

	return 0;
}

static void sysv_write_super(struct super_block *sb)
{
	if (!(sb->s_flags & MS_RDONLY))
		sysv_sync_fs(sb, 1);
	else
		sb->s_dirt = 0;
}

static int sysv_remount(struct super_block *sb, int *flags, char *data)
{
	struct sysv_sb_info *sbi = SYSV_SB(sb);
	lock_super(sb);
	if (sbi->s_forced_ro)
		*flags |= MS_RDONLY;
	if (*flags & MS_RDONLY)
		sysv_write_super(sb);
	unlock_super(sb);
	return 0;
}

static void sysv_put_super(struct super_block *sb)
{
	struct sysv_sb_info *sbi = SYSV_SB(sb);

	if (sb->s_dirt)
		sysv_write_super(sb);

	if (!(sb->s_flags & MS_RDONLY)) {
		mark_buffer_dirty(sbi->s_bh1);
		if (sbi->s_bh1 != sbi->s_bh2)
			mark_buffer_dirty(sbi->s_bh2);
	}

	brelse(sbi->s_bh1);
	if (sbi->s_bh1 != sbi->s_bh2)
		brelse(sbi->s_bh2);

	kfree(sbi);
}

static int sysv_statfs(struct dentry *dentry, struct kstatfs *buf)
{
	struct super_block *sb = dentry->d_sb;
	struct sysv_sb_info *sbi = SYSV_SB(sb);
	u64 id = huge_encode_dev(sb->s_bdev->bd_dev);

	buf->f_type = sb->s_magic;
	buf->f_bsize = sb->s_blocksize;
	buf->f_blocks = sbi->s_ndatazones;
	buf->f_bavail = buf->f_bfree = sysv_count_free_blocks(sb);
	buf->f_files = sbi->s_ninodes;
	buf->f_ffree = sysv_count_free_inodes(sb);
	buf->f_namelen = SYSV_NAMELEN;
	buf->f_fsid.val[0] = (u32)id;
	buf->f_fsid.val[1] = (u32)(id >> 32);
	return 0;
}

/* 
 * NXI <-> N0XI for PDP, XIN <-> XIN0 for le32, NIX <-> 0NIX for be32
 */
static inline void read3byte(struct sysv_sb_info *sbi,
	unsigned char * from, unsigned char * to)
{
	if (sbi->s_bytesex == BYTESEX_PDP) {
		to[0] = from[0];
		to[1] = 0;
		to[2] = from[1];
		to[3] = from[2];
	} else if (sbi->s_bytesex == BYTESEX_LE) {
		to[0] = from[0];
		to[1] = from[1];
		to[2] = from[2];
		to[3] = 0;
	} else {
		to[0] = 0;
		to[1] = from[0];
		to[2] = from[1];
		to[3] = from[2];
	}
}

static inline void write3byte(struct sysv_sb_info *sbi,
	unsigned char * from, unsigned char * to)
{
	if (sbi->s_bytesex == BYTESEX_PDP) {
		to[0] = from[0];
		to[1] = from[2];
		to[2] = from[3];
	} else if (sbi->s_bytesex == BYTESEX_LE) {
		to[0] = from[0];
		to[1] = from[1];
		to[2] = from[2];
	} else {
		to[0] = from[1];
		to[1] = from[2];
		to[2] = from[3];
	}
}

static const struct inode_operations sysv_symlink_inode_operations = {
	.readlink	= generic_readlink,
	.follow_link	= page_follow_link_light,
	.put_link	= page_put_link,
	.getattr	= sysv_getattr,
};

void sysv_set_inode(struct inode *inode, dev_t rdev)
{
	if (S_ISREG(inode->i_mode)) {
		inode->i_op = &sysv_file_inode_operations;
		inode->i_fop = &sysv_file_operations;
		inode->i_mapping->a_ops = &sysv_aops;
	} else if (S_ISDIR(inode->i_mode)) {
		inode->i_op = &sysv_dir_inode_operations;
		inode->i_fop = &sysv_dir_operations;
		inode->i_mapping->a_ops = &sysv_aops;
	} else if (S_ISLNK(inode->i_mode)) {
		if (inode->i_blocks) {
			inode->i_op = &sysv_symlink_inode_operations;
			inode->i_mapping->a_ops = &sysv_aops;
		} else {
			inode->i_op = &sysv_fast_symlink_inode_operations;
			nd_terminate_link(SYSV_I(inode)->i_data, inode->i_size,
				sizeof(SYSV_I(inode)->i_data) - 1);
		}
	} else
		init_special_inode(inode, inode->i_mode, rdev);
}

struct inode *sysv_iget(struct super_block *sb, unsigned int ino)
{
	struct sysv_sb_info * sbi = SYSV_SB(sb);
	struct buffer_head * bh;
	struct sysv_inode * raw_inode;
	struct sysv_inode_info * si;
	struct inode *inode;
	unsigned int block;

	if (!ino || ino > sbi->s_ninodes) {
		printk("Bad inode number on dev %s: %d is out of range\n",
		       sb->s_id, ino);
		return ERR_PTR(-EIO);
	}

	inode = iget_locked(sb, ino);
	if (!inode)
		return ERR_PTR(-ENOMEM);
	if (!(inode->i_state & I_NEW))
		return inode;

	raw_inode = sysv_raw_inode(sb, ino, &bh);
	if (!raw_inode) {
		printk("Major problem: unable to read inode from dev %s\n",
		       inode->i_sb->s_id);
		goto bad_inode;
	}
	/* SystemV FS: kludge permissions if ino==SYSV_ROOT_INO ?? */
	inode->i_mode = fs16_to_cpu(sbi, raw_inode->i_mode);
	inode->i_uid = (uid_t)fs16_to_cpu(sbi, raw_inode->i_uid);
	inode->i_gid = (gid_t)fs16_to_cpu(sbi, raw_inode->i_gid);
	inode->i_nlink = fs16_to_cpu(sbi, raw_inode->i_nlink);
	inode->i_size = fs32_to_cpu(sbi, raw_inode->i_size);
	inode->i_atime.tv_sec = fs32_to_cpu(sbi, raw_inode->i_atime);
	inode->i_mtime.tv_sec = fs32_to_cpu(sbi, raw_inode->i_mtime);
	inode->i_ctime.tv_sec = fs32_to_cpu(sbi, raw_inode->i_ctime);
	inode->i_ctime.tv_nsec = 0;
	inode->i_atime.tv_nsec = 0;
	inode->i_mtime.tv_nsec = 0;
	inode->i_blocks = 0;

	si = SYSV_I(inode);
	for (block = 0; block < 10+1+1+1; block++)
		read3byte(sbi, &raw_inode->i_data[3*block],
				(u8 *)&si->i_data[block]);
	brelse(bh);
	si->i_dir_start_lookup = 0;
	if (S_ISCHR(inode->i_mode) || S_ISBLK(inode->i_mode))
		sysv_set_inode(inode,
			       old_decode_dev(fs32_to_cpu(sbi, si->i_data[0])));
	else
		sysv_set_inode(inode, 0);
	unlock_new_inode(inode);
	return inode;

bad_inode:
	iget_failed(inode);
	return ERR_PTR(-EIO);
}

static int __sysv_write_inode(struct inode *inode, int wait)
{
	struct super_block * sb = inode->i_sb;
	struct sysv_sb_info * sbi = SYSV_SB(sb);
	struct buffer_head * bh;
	struct sysv_inode * raw_inode;
	struct sysv_inode_info * si;
	unsigned int ino, block;
	int err = 0;

	ino = inode->i_ino;
	if (!ino || ino > sbi->s_ninodes) {
		printk("Bad inode number on dev %s: %d is out of range\n",
		       inode->i_sb->s_id, ino);
		return -EIO;
	}
	raw_inode = sysv_raw_inode(sb, ino, &bh);
	if (!raw_inode) {
		printk("unable to read i-node block\n");
		return -EIO;
	}

	raw_inode->i_mode = cpu_to_fs16(sbi, inode->i_mode);
	raw_inode->i_uid = cpu_to_fs16(sbi, fs_high2lowuid(inode->i_uid));
	raw_inode->i_gid = cpu_to_fs16(sbi, fs_high2lowgid(inode->i_gid));
	raw_inode->i_nlink = cpu_to_fs16(sbi, inode->i_nlink);
	raw_inode->i_size = cpu_to_fs32(sbi, inode->i_size);
	raw_inode->i_atime = cpu_to_fs32(sbi, inode->i_atime.tv_sec);
	raw_inode->i_mtime = cpu_to_fs32(sbi, inode->i_mtime.tv_sec);
	raw_inode->i_ctime = cpu_to_fs32(sbi, inode->i_ctime.tv_sec);

	si = SYSV_I(inode);
	if (S_ISCHR(inode->i_mode) || S_ISBLK(inode->i_mode))
		si->i_data[0] = cpu_to_fs32(sbi, old_encode_dev(inode->i_rdev));
	for (block = 0; block < 10+1+1+1; block++)
		write3byte(sbi, (u8 *)&si->i_data[block],
			&raw_inode->i_data[3*block]);
	mark_buffer_dirty(bh);
	if (wait) {
                sync_dirty_buffer(bh);
                if (buffer_req(bh) && !buffer_uptodate(bh)) {
                        printk ("IO error syncing sysv inode [%s:%08x]\n",
                                sb->s_id, ino);
                        err = -EIO;
                }
        }
	brelse(bh);
	return 0;
}

int sysv_write_inode(struct inode *inode, struct writeback_control *wbc)
{
	return __sysv_write_inode(inode, wbc->sync_mode == WB_SYNC_ALL);
}

int sysv_sync_inode(struct inode *inode)
{
	return __sysv_write_inode(inode, 1);
}

static void sysv_evict_inode(struct inode *inode)
{
	truncate_inode_pages(&inode->i_data, 0);
	if (!inode->i_nlink) {
		inode->i_size = 0;
		sysv_truncate(inode);
	}
	invalidate_inode_buffers(inode);
	end_writeback(inode);
	if (!inode->i_nlink)
		sysv_free_inode(inode);
}

static struct kmem_cache *sysv_inode_cachep;

static struct inode *sysv_alloc_inode(struct super_block *sb)
{
	struct sysv_inode_info *si;

	si = kmem_cache_alloc(sysv_inode_cachep, GFP_KERNEL);
	if (!si)
		return NULL;
	return &si->vfs_inode;
}

static void sysv_destroy_inode(struct inode *inode)
{
	kmem_cache_free(sysv_inode_cachep, SYSV_I(inode));
}

static void init_once(void *p)
{
	struct sysv_inode_info *si = (struct sysv_inode_info *)p;

	inode_init_once(&si->vfs_inode);
}

const struct super_operations sysv_sops = {
	.alloc_inode	= sysv_alloc_inode,
	.destroy_inode	= sysv_destroy_inode,
	.write_inode	= sysv_write_inode,
	.evict_inode	= sysv_evict_inode,
	.put_super	= sysv_put_super,
	.write_super	= sysv_write_super,
	.sync_fs	= sysv_sync_fs,
	.remount_fs	= sysv_remount,
	.statfs		= sysv_statfs,
};

int __init sysv_init_icache(void)
{
	sysv_inode_cachep = kmem_cache_create("sysv_inode_cache",
			sizeof(struct sysv_inode_info), 0,
			SLAB_RECLAIM_ACCOUNT|SLAB_MEM_SPREAD,
			init_once);
	if (!sysv_inode_cachep)
		return -ENOMEM;
	return 0;
}

void sysv_destroy_icache(void)
{
	kmem_cache_destroy(sysv_inode_cachep);
}
