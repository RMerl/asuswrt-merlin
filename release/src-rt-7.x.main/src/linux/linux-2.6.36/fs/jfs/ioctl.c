/*
 * linux/fs/jfs/ioctl.c
 *
 * Copyright (C) 2006 Herbert Poetzl
 * adapted from Remy Card's ext2/ioctl.c
 */

#include <linux/fs.h>
#include <linux/ctype.h>
#include <linux/capability.h>
#include <linux/mount.h>
#include <linux/time.h>
#include <linux/sched.h>
#include <asm/current.h>
#include <asm/uaccess.h>

#include "jfs_incore.h"
#include "jfs_dinode.h"
#include "jfs_inode.h"


static struct {
	long jfs_flag;
	long ext2_flag;
} jfs_map[] = {
	{JFS_NOATIME_FL,	FS_NOATIME_FL},
	{JFS_DIRSYNC_FL,	FS_DIRSYNC_FL},
	{JFS_SYNC_FL,		FS_SYNC_FL},
	{JFS_SECRM_FL,		FS_SECRM_FL},
	{JFS_UNRM_FL,		FS_UNRM_FL},
	{JFS_APPEND_FL,		FS_APPEND_FL},
	{JFS_IMMUTABLE_FL,	FS_IMMUTABLE_FL},
	{0, 0},
};

static long jfs_map_ext2(unsigned long flags, int from)
{
	int index=0;
	long mapped=0;

	while (jfs_map[index].jfs_flag) {
		if (from) {
			if (jfs_map[index].ext2_flag & flags)
				mapped |= jfs_map[index].jfs_flag;
		} else {
			if (jfs_map[index].jfs_flag & flags)
				mapped |= jfs_map[index].ext2_flag;
		}
		index++;
	}
	return mapped;
}


long jfs_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct inode *inode = filp->f_dentry->d_inode;
	struct jfs_inode_info *jfs_inode = JFS_IP(inode);
	unsigned int flags;

	switch (cmd) {
	case JFS_IOC_GETFLAGS:
		jfs_get_inode_flags(jfs_inode);
		flags = jfs_inode->mode2 & JFS_FL_USER_VISIBLE;
		flags = jfs_map_ext2(flags, 0);
		return put_user(flags, (int __user *) arg);
	case JFS_IOC_SETFLAGS: {
		unsigned int oldflags;
		int err;

		err = mnt_want_write(filp->f_path.mnt);
		if (err)
			return err;

		if (!is_owner_or_cap(inode)) {
			err = -EACCES;
			goto setflags_out;
		}
		if (get_user(flags, (int __user *) arg)) {
			err = -EFAULT;
			goto setflags_out;
		}

		flags = jfs_map_ext2(flags, 1);
		if (!S_ISDIR(inode->i_mode))
			flags &= ~JFS_DIRSYNC_FL;

		/* Is it quota file? Do not allow user to mess with it */
		if (IS_NOQUOTA(inode)) {
			err = -EPERM;
			goto setflags_out;
		}

		/* Lock against other parallel changes of flags */
		mutex_lock(&inode->i_mutex);

		jfs_get_inode_flags(jfs_inode);
		oldflags = jfs_inode->mode2;

		/*
		 * The IMMUTABLE and APPEND_ONLY flags can only be changed by
		 * the relevant capability.
		 */
		if ((oldflags & JFS_IMMUTABLE_FL) ||
			((flags ^ oldflags) &
			(JFS_APPEND_FL | JFS_IMMUTABLE_FL))) {
			if (!capable(CAP_LINUX_IMMUTABLE)) {
				mutex_unlock(&inode->i_mutex);
				err = -EPERM;
				goto setflags_out;
			}
		}

		flags = flags & JFS_FL_USER_MODIFIABLE;
		flags |= oldflags & ~JFS_FL_USER_MODIFIABLE;
		jfs_inode->mode2 = flags;

		jfs_set_inode_flags(inode);
		mutex_unlock(&inode->i_mutex);
		inode->i_ctime = CURRENT_TIME_SEC;
		mark_inode_dirty(inode);
setflags_out:
		mnt_drop_write(filp->f_path.mnt);
		return err;
	}
	default:
		return -ENOTTY;
	}
}

#ifdef CONFIG_COMPAT
long jfs_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	/* While these ioctl numbers defined with 'long' and have different
	 * numbers than the 64bit ABI,
	 * the actual implementation only deals with ints and is compatible.
	 */
	switch (cmd) {
	case JFS_IOC_GETFLAGS32:
		cmd = JFS_IOC_GETFLAGS;
		break;
	case JFS_IOC_SETFLAGS32:
		cmd = JFS_IOC_SETFLAGS;
		break;
	}
	return jfs_ioctl(filp, cmd, arg);
}
#endif
