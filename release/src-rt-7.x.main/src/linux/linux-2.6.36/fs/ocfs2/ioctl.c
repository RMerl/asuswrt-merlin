/*
 * linux/fs/ocfs2/ioctl.c
 *
 * Copyright (C) 2006 Herbert Poetzl
 * adapted from Remy Card's ext2/ioctl.c
 */

#include <linux/fs.h>
#include <linux/mount.h>
#include <linux/compat.h>

#define MLOG_MASK_PREFIX ML_INODE
#include <cluster/masklog.h>

#include "ocfs2.h"
#include "alloc.h"
#include "dlmglue.h"
#include "file.h"
#include "inode.h"
#include "journal.h"

#include "ocfs2_fs.h"
#include "ioctl.h"
#include "resize.h"
#include "refcounttree.h"

#include <linux/ext2_fs.h>

static int ocfs2_get_inode_attr(struct inode *inode, unsigned *flags)
{
	int status;

	status = ocfs2_inode_lock(inode, NULL, 0);
	if (status < 0) {
		mlog_errno(status);
		return status;
	}
	ocfs2_get_inode_flags(OCFS2_I(inode));
	*flags = OCFS2_I(inode)->ip_attr;
	ocfs2_inode_unlock(inode, 0);

	mlog_exit(status);
	return status;
}

static int ocfs2_set_inode_attr(struct inode *inode, unsigned flags,
				unsigned mask)
{
	struct ocfs2_inode_info *ocfs2_inode = OCFS2_I(inode);
	struct ocfs2_super *osb = OCFS2_SB(inode->i_sb);
	handle_t *handle = NULL;
	struct buffer_head *bh = NULL;
	unsigned oldflags;
	int status;

	mutex_lock(&inode->i_mutex);

	status = ocfs2_inode_lock(inode, &bh, 1);
	if (status < 0) {
		mlog_errno(status);
		goto bail;
	}

	status = -EACCES;
	if (!is_owner_or_cap(inode))
		goto bail_unlock;

	if (!S_ISDIR(inode->i_mode))
		flags &= ~OCFS2_DIRSYNC_FL;

	handle = ocfs2_start_trans(osb, OCFS2_INODE_UPDATE_CREDITS);
	if (IS_ERR(handle)) {
		status = PTR_ERR(handle);
		mlog_errno(status);
		goto bail_unlock;
	}

	oldflags = ocfs2_inode->ip_attr;
	flags = flags & mask;
	flags |= oldflags & ~mask;

	/*
	 * The IMMUTABLE and APPEND_ONLY flags can only be changed by
	 * the relevant capability.
	 */
	status = -EPERM;
	if ((oldflags & OCFS2_IMMUTABLE_FL) || ((flags ^ oldflags) &
		(OCFS2_APPEND_FL | OCFS2_IMMUTABLE_FL))) {
		if (!capable(CAP_LINUX_IMMUTABLE))
			goto bail_unlock;
	}

	ocfs2_inode->ip_attr = flags;
	ocfs2_set_inode_flags(inode);

	status = ocfs2_mark_inode_dirty(handle, inode, bh);
	if (status < 0)
		mlog_errno(status);

	ocfs2_commit_trans(osb, handle);
bail_unlock:
	ocfs2_inode_unlock(inode, 1);
bail:
	mutex_unlock(&inode->i_mutex);

	brelse(bh);

	mlog_exit(status);
	return status;
}

long ocfs2_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct inode *inode = filp->f_path.dentry->d_inode;
	unsigned int flags;
	int new_clusters;
	int status;
	struct ocfs2_space_resv sr;
	struct ocfs2_new_group_input input;
	struct reflink_arguments args;
	const char *old_path, *new_path;
	bool preserve;

	switch (cmd) {
	case OCFS2_IOC_GETFLAGS:
		status = ocfs2_get_inode_attr(inode, &flags);
		if (status < 0)
			return status;

		flags &= OCFS2_FL_VISIBLE;
		return put_user(flags, (int __user *) arg);
	case OCFS2_IOC_SETFLAGS:
		if (get_user(flags, (int __user *) arg))
			return -EFAULT;

		status = mnt_want_write(filp->f_path.mnt);
		if (status)
			return status;
		status = ocfs2_set_inode_attr(inode, flags,
			OCFS2_FL_MODIFIABLE);
		mnt_drop_write(filp->f_path.mnt);
		return status;
	case OCFS2_IOC_RESVSP:
	case OCFS2_IOC_RESVSP64:
	case OCFS2_IOC_UNRESVSP:
	case OCFS2_IOC_UNRESVSP64:
		if (copy_from_user(&sr, (int __user *) arg, sizeof(sr)))
			return -EFAULT;

		return ocfs2_change_file_space(filp, cmd, &sr);
	case OCFS2_IOC_GROUP_EXTEND:
		if (!capable(CAP_SYS_RESOURCE))
			return -EPERM;

		if (get_user(new_clusters, (int __user *)arg))
			return -EFAULT;

		return ocfs2_group_extend(inode, new_clusters);
	case OCFS2_IOC_GROUP_ADD:
	case OCFS2_IOC_GROUP_ADD64:
		if (!capable(CAP_SYS_RESOURCE))
			return -EPERM;

		if (copy_from_user(&input, (int __user *) arg, sizeof(input)))
			return -EFAULT;

		return ocfs2_group_add(inode, &input);
	case OCFS2_IOC_REFLINK:
		if (copy_from_user(&args, (struct reflink_arguments *)arg,
				   sizeof(args)))
			return -EFAULT;
		old_path = (const char *)(unsigned long)args.old_path;
		new_path = (const char *)(unsigned long)args.new_path;
		preserve = (args.preserve != 0);

		return ocfs2_reflink_ioctl(inode, old_path, new_path, preserve);
	default:
		return -ENOTTY;
	}
}

#ifdef CONFIG_COMPAT
long ocfs2_compat_ioctl(struct file *file, unsigned cmd, unsigned long arg)
{
	bool preserve;
	struct reflink_arguments args;
	struct inode *inode = file->f_path.dentry->d_inode;

	switch (cmd) {
	case OCFS2_IOC32_GETFLAGS:
		cmd = OCFS2_IOC_GETFLAGS;
		break;
	case OCFS2_IOC32_SETFLAGS:
		cmd = OCFS2_IOC_SETFLAGS;
		break;
	case OCFS2_IOC_RESVSP:
	case OCFS2_IOC_RESVSP64:
	case OCFS2_IOC_UNRESVSP:
	case OCFS2_IOC_UNRESVSP64:
	case OCFS2_IOC_GROUP_EXTEND:
	case OCFS2_IOC_GROUP_ADD:
	case OCFS2_IOC_GROUP_ADD64:
		break;
	case OCFS2_IOC_REFLINK:
		if (copy_from_user(&args, (struct reflink_arguments *)arg,
				   sizeof(args)))
			return -EFAULT;
		preserve = (args.preserve != 0);

		return ocfs2_reflink_ioctl(inode, compat_ptr(args.old_path),
					   compat_ptr(args.new_path), preserve);
	default:
		return -ENOIOCTLCMD;
	}

	return ocfs2_ioctl(file, cmd, arg);
}
#endif
