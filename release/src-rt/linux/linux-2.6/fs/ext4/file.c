/*
 *  linux/fs/ext4/file.c
 *
 * Copyright (C) 1992, 1993, 1994, 1995
 * Remy Card (card@masi.ibp.fr)
 * Laboratoire MASI - Institut Blaise Pascal
 * Universite Pierre et Marie Curie (Paris VI)
 *
 *  from
 *
 *  linux/fs/minix/file.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *
 *  ext4 fs regular file handling primitives
 *
 *  64-bit file support on 64-bit platforms by Jakub Jelinek
 *	(jj@sunsite.ms.mff.cuni.cz)
 */

#include <linux/time.h>
#include <linux/fs.h>
#include <linux/jbd2.h>
#include <linux/mount.h>
#include <linux/path.h>
#include <linux/quotaops.h>
#include "ext4.h"
#include "ext4_jbd2.h"
#include "xattr.h"
#include "acl.h"

/*
 * Called when an inode is released. Note that this is different
 * from ext4_file_open: open gets called at every open, but release
 * gets called only when /all/ the files are closed.
 */
static int ext4_release_file(struct inode *inode, struct file *filp)
{
	if (ext4_test_inode_state(inode, EXT4_STATE_DA_ALLOC_CLOSE)) {
		ext4_alloc_da_blocks(inode);
		ext4_clear_inode_state(inode, EXT4_STATE_DA_ALLOC_CLOSE);
	}
	/* if we are the last writer on the inode, drop the block reservation */
	if ((filp->f_mode & FMODE_WRITE) &&
			(atomic_read(&inode->i_writecount) == 1) &&
		        !EXT4_I(inode)->i_reserved_data_blocks)
	{
		down_write(&EXT4_I(inode)->i_data_sem);
		ext4_discard_preallocations(inode);
		up_write(&EXT4_I(inode)->i_data_sem);
	}
	if (is_dx(inode) && filp->private_data)
		ext4_htree_free_dir_info(filp->private_data);

	return 0;
}

static void ext4_aiodio_wait(struct inode *inode)
{
	wait_queue_head_t *wq = ext4_ioend_wq(inode);

	wait_event(*wq, (atomic_read(&EXT4_I(inode)->i_aiodio_unwritten) == 0));
}

/*
 * This tests whether the IO in question is block-aligned or not.
 * Ext4 utilizes unwritten extents when hole-filling during direct IO, and they
 * are converted to written only after the IO is complete.  Until they are
 * mapped, these blocks appear as holes, so dio_zero_block() will assume that
 * it needs to zero out portions of the start and/or end block.  If 2 AIO
 * threads are at work on the same unwritten block, they must be synchronized
 * or one thread will zero the other's data, causing corruption.
 */
static int
ext4_unaligned_aio(struct inode *inode, const struct iovec *iov,
		   unsigned long nr_segs, loff_t pos)
{
	struct super_block *sb = inode->i_sb;
	int blockmask = sb->s_blocksize - 1;
	size_t count = iov_length(iov, nr_segs);
	loff_t final_size = pos + count;

	if (pos >= inode->i_size)
		return 0;

	if ((pos & blockmask) || (final_size & blockmask))
		return 1;

	return 0;
}

static ssize_t
ext4_file_write(struct kiocb *iocb, const struct iovec *iov,
		unsigned long nr_segs, loff_t pos)
{
	struct inode *inode = iocb->ki_filp->f_path.dentry->d_inode;
	int unaligned_aio = 0;
	int ret;

	/*
	 * If we have encountered a bitmap-format file, the size limit
	 * is smaller than s_maxbytes, which is for extent-mapped files.
	 */

	if (!(ext4_test_inode_flag(inode, EXT4_INODE_EXTENTS))) {
		struct ext4_sb_info *sbi = EXT4_SB(inode->i_sb);
		size_t length = iov_length(iov, nr_segs);

		if ((pos > sbi->s_bitmap_maxbytes ||
		    (pos == sbi->s_bitmap_maxbytes && length > 0)))
			return -EFBIG;

		if (pos + length > sbi->s_bitmap_maxbytes) {
			nr_segs = iov_shorten((struct iovec *)iov, nr_segs,
					      sbi->s_bitmap_maxbytes - pos);
		}
	} else if (unlikely((iocb->ki_filp->f_flags & O_DIRECT) &&
		   !is_sync_kiocb(iocb))) {
		unaligned_aio = ext4_unaligned_aio(inode, iov, nr_segs, pos);
	}

	/* Unaligned direct AIO must be serialized; see comment above */
	if (unaligned_aio) {
		static unsigned long unaligned_warn_time;

		/* Warn about this once per day */
		if (printk_timed_ratelimit(&unaligned_warn_time, 60*60*24*HZ))
			ext4_msg(inode->i_sb, KERN_WARNING,
				 "Unaligned AIO/DIO on inode %ld by %s; "
				 "performance will be poor.",
				 inode->i_ino, current->comm);
		mutex_lock(ext4_aio_mutex(inode));
		ext4_aiodio_wait(inode);
	}

	ret = generic_file_aio_write(iocb, iov, nr_segs, pos);

	if (unaligned_aio)
		mutex_unlock(ext4_aio_mutex(inode));

	return ret;
}

static const struct vm_operations_struct ext4_file_vm_ops = {
	.fault		= filemap_fault,
	.page_mkwrite   = ext4_page_mkwrite,
};

static int ext4_file_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct address_space *mapping = file->f_mapping;

	if (!mapping->a_ops->readpage)
		return -ENOEXEC;
	file_accessed(file);
	vma->vm_ops = &ext4_file_vm_ops;
	vma->vm_flags |= VM_CAN_NONLINEAR;
	return 0;
}

static int ext4_file_open(struct inode * inode, struct file * filp)
{
	struct super_block *sb = inode->i_sb;
	struct ext4_sb_info *sbi = EXT4_SB(inode->i_sb);
	struct ext4_inode_info *ei = EXT4_I(inode);
	struct vfsmount *mnt = filp->f_path.mnt;
	struct path path;
	char buf[64], *cp;

	if (unlikely(!(sbi->s_mount_flags & EXT4_MF_MNTDIR_SAMPLED) &&
		     !(sb->s_flags & MS_RDONLY))) {
		sbi->s_mount_flags |= EXT4_MF_MNTDIR_SAMPLED;
		/*
		 * Sample where the filesystem has been mounted and
		 * store it in the superblock for sysadmin convenience
		 * when trying to sort through large numbers of block
		 * devices or filesystem images.
		 */
		memset(buf, 0, sizeof(buf));
		path.mnt = mnt;
		path.dentry = mnt->mnt_root;
		cp = d_path(&path, buf, sizeof(buf));
		if (!IS_ERR(cp)) {
			memcpy(sbi->s_es->s_last_mounted, cp,
			       sizeof(sbi->s_es->s_last_mounted));
			ext4_mark_super_dirty(sb);
		}
	}
	/*
	 * Set up the jbd2_inode if we are opening the inode for
	 * writing and the journal is present
	 */
	if (sbi->s_journal && !ei->jinode && (filp->f_mode & FMODE_WRITE)) {
		struct jbd2_inode *jinode = jbd2_alloc_inode(GFP_KERNEL);

		spin_lock(&inode->i_lock);
		if (!ei->jinode) {
			if (!jinode) {
				spin_unlock(&inode->i_lock);
				return -ENOMEM;
			}
			ei->jinode = jinode;
			jbd2_journal_init_jbd_inode(ei->jinode, inode);
			jinode = NULL;
		}
		spin_unlock(&inode->i_lock);
		if (unlikely(jinode != NULL))
			jbd2_free_inode(jinode);
	}
	return dquot_file_open(inode, filp);
}

/*
 * ext4_llseek() copied from generic_file_llseek() to handle both
 * block-mapped and extent-mapped maxbytes values. This should
 * otherwise be identical with generic_file_llseek().
 */
loff_t ext4_llseek(struct file *file, loff_t offset, int origin)
{
	struct inode *inode = file->f_mapping->host;
	loff_t maxbytes;

	if (!(ext4_test_inode_flag(inode, EXT4_INODE_EXTENTS)))
		maxbytes = EXT4_SB(inode->i_sb)->s_bitmap_maxbytes;
	else
		maxbytes = inode->i_sb->s_maxbytes;
	mutex_lock(&inode->i_mutex);
	switch (origin) {
	case SEEK_END:
		offset += inode->i_size;
		break;
	case SEEK_CUR:
		if (offset == 0) {
			mutex_unlock(&inode->i_mutex);
			return file->f_pos;
		}
		offset += file->f_pos;
		break;
	}

	if (offset < 0 || offset > maxbytes) {
		mutex_unlock(&inode->i_mutex);
		return -EINVAL;
	}

	if (offset != file->f_pos) {
		file->f_pos = offset;
		file->f_version = 0;
	}
	mutex_unlock(&inode->i_mutex);

	return offset;
}

const struct file_operations ext4_file_operations = {
	.llseek		= ext4_llseek,
	.read		= do_sync_read,
	.write		= do_sync_write,
	.aio_read	= generic_file_aio_read,
	.aio_write	= ext4_file_write,
	.unlocked_ioctl = ext4_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= ext4_compat_ioctl,
#endif
	.mmap		= ext4_file_mmap,
	.open		= ext4_file_open,
	.release	= ext4_release_file,
	.fsync		= ext4_sync_file,
	.splice_read	= generic_file_splice_read,
	.splice_write	= generic_file_splice_write,
	.fallocate	= ext4_fallocate,
};

const struct inode_operations ext4_file_inode_operations = {
	.truncate	= ext4_truncate,
	.setattr	= ext4_setattr,
	.getattr	= ext4_getattr,
#ifdef CONFIG_EXT4_FS_XATTR
	.setxattr	= generic_setxattr,
	.getxattr	= generic_getxattr,
	.listxattr	= ext4_listxattr,
	.removexattr	= generic_removexattr,
#endif
	.check_acl	= ext4_check_acl,
	.fiemap		= ext4_fiemap,
};

