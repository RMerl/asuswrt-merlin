/*
 * fs/inotify_user.c - inotify support for userspace
 *
 * Authors:
 *	John McCutchan	<ttb@tentacle.dhs.org>
 *	Robert Love	<rml@novell.com>
 *
 * Copyright (C) 2005 John McCutchan
 * Copyright 2006 Hewlett-Packard Development Company, L.P.
 *
 * Copyright (C) 2009 Eric Paris <Red Hat Inc>
 * inotify was largely rewriten to make use of the fsnotify infrastructure
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include <linux/file.h>
#include <linux/fs.h> /* struct inode */
#include <linux/fsnotify_backend.h>
#include <linux/idr.h>
#include <linux/init.h> /* module_init */
#include <linux/inotify.h>
#include <linux/kernel.h> /* roundup() */
#include <linux/namei.h> /* LOOKUP_FOLLOW */
#include <linux/sched.h> /* struct user */
#include <linux/slab.h> /* struct kmem_cache */
#include <linux/syscalls.h>
#include <linux/types.h>
#include <linux/anon_inodes.h>
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/wait.h>

#include "inotify.h"

#include <asm/ioctls.h>

/* these are configurable via /proc/sys/fs/inotify/ */
static int inotify_max_user_instances __read_mostly;
static int inotify_max_queued_events __read_mostly;
static int inotify_max_user_watches __read_mostly;

static struct kmem_cache *inotify_inode_mark_cachep __read_mostly;
struct kmem_cache *event_priv_cachep __read_mostly;

#ifdef CONFIG_SYSCTL

#include <linux/sysctl.h>

static int zero;

ctl_table inotify_table[] = {
	{
		.procname	= "max_user_instances",
		.data		= &inotify_max_user_instances,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= &zero,
	},
	{
		.procname	= "max_user_watches",
		.data		= &inotify_max_user_watches,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= &zero,
	},
	{
		.procname	= "max_queued_events",
		.data		= &inotify_max_queued_events,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= &zero
	},
	{ }
};
#endif /* CONFIG_SYSCTL */

static inline __u32 inotify_arg_to_mask(u32 arg)
{
	__u32 mask;

	/*
	 * everything should accept their own ignored, cares about children,
	 * and should receive events when the inode is unmounted
	 */
	mask = (FS_IN_IGNORED | FS_EVENT_ON_CHILD | FS_UNMOUNT);

	/* mask off the flags used to open the fd */
	mask |= (arg & (IN_ALL_EVENTS | IN_ONESHOT | IN_EXCL_UNLINK));

	return mask;
}

static inline u32 inotify_mask_to_arg(__u32 mask)
{
	return mask & (IN_ALL_EVENTS | IN_ISDIR | IN_UNMOUNT | IN_IGNORED |
		       IN_Q_OVERFLOW);
}

/* intofiy userspace file descriptor functions */
static unsigned int inotify_poll(struct file *file, poll_table *wait)
{
	struct fsnotify_group *group = file->private_data;
	int ret = 0;

	poll_wait(file, &group->notification_waitq, wait);
	mutex_lock(&group->notification_mutex);
	if (!fsnotify_notify_queue_is_empty(group))
		ret = POLLIN | POLLRDNORM;
	mutex_unlock(&group->notification_mutex);

	return ret;
}

/*
 * Get an inotify_kernel_event if one exists and is small
 * enough to fit in "count". Return an error pointer if
 * not large enough.
 *
 * Called with the group->notification_mutex held.
 */
static struct fsnotify_event *get_one_event(struct fsnotify_group *group,
					    size_t count)
{
	size_t event_size = sizeof(struct inotify_event);
	struct fsnotify_event *event;

	if (fsnotify_notify_queue_is_empty(group))
		return NULL;

	event = fsnotify_peek_notify_event(group);

	pr_debug("%s: group=%p event=%p\n", __func__, group, event);

	if (event->name_len)
		event_size += roundup(event->name_len + 1, event_size);

	if (event_size > count)
		return ERR_PTR(-EINVAL);

	/* held the notification_mutex the whole time, so this is the
	 * same event we peeked above */
	fsnotify_remove_notify_event(group);

	return event;
}

/*
 * Copy an event to user space, returning how much we copied.
 *
 * We already checked that the event size is smaller than the
 * buffer we had in "get_one_event()" above.
 */
static ssize_t copy_event_to_user(struct fsnotify_group *group,
				  struct fsnotify_event *event,
				  char __user *buf)
{
	struct inotify_event inotify_event;
	struct fsnotify_event_private_data *fsn_priv;
	struct inotify_event_private_data *priv;
	size_t event_size = sizeof(struct inotify_event);
	size_t name_len = 0;

	pr_debug("%s: group=%p event=%p\n", __func__, group, event);

	/* we get the inotify watch descriptor from the event private data */
	spin_lock(&event->lock);
	fsn_priv = fsnotify_remove_priv_from_event(group, event);
	spin_unlock(&event->lock);

	if (!fsn_priv)
		inotify_event.wd = -1;
	else {
		priv = container_of(fsn_priv, struct inotify_event_private_data,
				    fsnotify_event_priv_data);
		inotify_event.wd = priv->wd;
		inotify_free_event_priv(fsn_priv);
	}

	/*
	 * round up event->name_len so it is a multiple of event_size
	 * plus an extra byte for the terminating '\0'.
	 */
	if (event->name_len)
		name_len = roundup(event->name_len + 1, event_size);
	inotify_event.len = name_len;

	inotify_event.mask = inotify_mask_to_arg(event->mask);
	inotify_event.cookie = event->sync_cookie;

	/* send the main event */
	if (copy_to_user(buf, &inotify_event, event_size))
		return -EFAULT;

	buf += event_size;

	/*
	 * fsnotify only stores the pathname, so here we have to send the pathname
	 * and then pad that pathname out to a multiple of sizeof(inotify_event)
	 * with zeros.  I get my zeros from the nul_inotify_event.
	 */
	if (name_len) {
		unsigned int len_to_zero = name_len - event->name_len;
		/* copy the path name */
		if (copy_to_user(buf, event->file_name, event->name_len))
			return -EFAULT;
		buf += event->name_len;

		/* fill userspace with 0's */
		if (clear_user(buf, len_to_zero))
			return -EFAULT;
		buf += len_to_zero;
		event_size += name_len;
	}

	return event_size;
}

static ssize_t inotify_read(struct file *file, char __user *buf,
			    size_t count, loff_t *pos)
{
	struct fsnotify_group *group;
	struct fsnotify_event *kevent;
	char __user *start;
	int ret;
	DEFINE_WAIT(wait);

	start = buf;
	group = file->private_data;

	while (1) {
		prepare_to_wait(&group->notification_waitq, &wait, TASK_INTERRUPTIBLE);

		mutex_lock(&group->notification_mutex);
		kevent = get_one_event(group, count);
		mutex_unlock(&group->notification_mutex);

		pr_debug("%s: group=%p kevent=%p\n", __func__, group, kevent);

		if (kevent) {
			ret = PTR_ERR(kevent);
			if (IS_ERR(kevent))
				break;
			ret = copy_event_to_user(group, kevent, buf);
			fsnotify_put_event(kevent);
			if (ret < 0)
				break;
			buf += ret;
			count -= ret;
			continue;
		}

		ret = -EAGAIN;
		if (file->f_flags & O_NONBLOCK)
			break;
		ret = -EINTR;
		if (signal_pending(current))
			break;

		if (start != buf)
			break;

		schedule();
	}

	finish_wait(&group->notification_waitq, &wait);
	if (start != buf && ret != -EFAULT)
		ret = buf - start;
	return ret;
}

static int inotify_fasync(int fd, struct file *file, int on)
{
	struct fsnotify_group *group = file->private_data;

	return fasync_helper(fd, file, on, &group->inotify_data.fa) >= 0 ? 0 : -EIO;
}

static int inotify_release(struct inode *ignored, struct file *file)
{
	struct fsnotify_group *group = file->private_data;
	struct user_struct *user = group->inotify_data.user;

	pr_debug("%s: group=%p\n", __func__, group);

	fsnotify_clear_marks_by_group(group);

	/* free this group, matching get was inotify_init->fsnotify_obtain_group */
	fsnotify_put_group(group);

	atomic_dec(&user->inotify_devs);

	return 0;
}

static long inotify_ioctl(struct file *file, unsigned int cmd,
			  unsigned long arg)
{
	struct fsnotify_group *group;
	struct fsnotify_event_holder *holder;
	struct fsnotify_event *event;
	void __user *p;
	int ret = -ENOTTY;
	size_t send_len = 0;

	group = file->private_data;
	p = (void __user *) arg;

	pr_debug("%s: group=%p cmd=%u\n", __func__, group, cmd);

	switch (cmd) {
	case FIONREAD:
		mutex_lock(&group->notification_mutex);
		list_for_each_entry(holder, &group->notification_list, event_list) {
			event = holder->event;
			send_len += sizeof(struct inotify_event);
			if (event->name_len)
				send_len += roundup(event->name_len + 1,
						sizeof(struct inotify_event));
		}
		mutex_unlock(&group->notification_mutex);
		ret = put_user(send_len, (int __user *) p);
		break;
	}

	return ret;
}

static const struct file_operations inotify_fops = {
	.poll		= inotify_poll,
	.read		= inotify_read,
	.fasync		= inotify_fasync,
	.release	= inotify_release,
	.unlocked_ioctl	= inotify_ioctl,
	.compat_ioctl	= inotify_ioctl,
};


/*
 * find_inode - resolve a user-given path to a specific inode
 */
static int inotify_find_inode(const char __user *dirname, struct path *path, unsigned flags)
{
	int error;

	error = user_path_at(AT_FDCWD, dirname, flags, path);
	if (error)
		return error;
	/* you can only watch an inode if you have read permissions on it */
	error = inode_permission(path->dentry->d_inode, MAY_READ);
	if (error)
		path_put(path);
	return error;
}

static int inotify_add_to_idr(struct idr *idr, spinlock_t *idr_lock,
			      int *last_wd,
			      struct inotify_inode_mark *i_mark)
{
	int ret;

	do {
		if (unlikely(!idr_pre_get(idr, GFP_KERNEL)))
			return -ENOMEM;

		spin_lock(idr_lock);
		ret = idr_get_new_above(idr, i_mark, *last_wd + 1,
					&i_mark->wd);
		/* we added the mark to the idr, take a reference */
		if (!ret) {
			*last_wd = i_mark->wd;
			fsnotify_get_mark(&i_mark->fsn_mark);
		}
		spin_unlock(idr_lock);
	} while (ret == -EAGAIN);

	return ret;
}

static struct inotify_inode_mark *inotify_idr_find_locked(struct fsnotify_group *group,
								int wd)
{
	struct idr *idr = &group->inotify_data.idr;
	spinlock_t *idr_lock = &group->inotify_data.idr_lock;
	struct inotify_inode_mark *i_mark;

	assert_spin_locked(idr_lock);

	i_mark = idr_find(idr, wd);
	if (i_mark) {
		struct fsnotify_mark *fsn_mark = &i_mark->fsn_mark;

		fsnotify_get_mark(fsn_mark);
		/* One ref for being in the idr, one ref we just took */
		BUG_ON(atomic_read(&fsn_mark->refcnt) < 2);
	}

	return i_mark;
}

static struct inotify_inode_mark *inotify_idr_find(struct fsnotify_group *group,
							 int wd)
{
	struct inotify_inode_mark *i_mark;
	spinlock_t *idr_lock = &group->inotify_data.idr_lock;

	spin_lock(idr_lock);
	i_mark = inotify_idr_find_locked(group, wd);
	spin_unlock(idr_lock);

	return i_mark;
}

static void do_inotify_remove_from_idr(struct fsnotify_group *group,
				       struct inotify_inode_mark *i_mark)
{
	struct idr *idr = &group->inotify_data.idr;
	spinlock_t *idr_lock = &group->inotify_data.idr_lock;
	int wd = i_mark->wd;

	assert_spin_locked(idr_lock);

	idr_remove(idr, wd);

	/* removed from the idr, drop that ref */
	fsnotify_put_mark(&i_mark->fsn_mark);
}

/*
 * Remove the mark from the idr (if present) and drop the reference
 * on the mark because it was in the idr.
 */
static void inotify_remove_from_idr(struct fsnotify_group *group,
				    struct inotify_inode_mark *i_mark)
{
	spinlock_t *idr_lock = &group->inotify_data.idr_lock;
	struct inotify_inode_mark *found_i_mark = NULL;
	int wd;

	spin_lock(idr_lock);
	wd = i_mark->wd;

	/*
	 * does this i_mark think it is in the idr?  we shouldn't get called
	 * if it wasn't....
	 */
	if (wd == -1) {
		WARN_ONCE(1, "%s: i_mark=%p i_mark->wd=%d i_mark->group=%p"
			" i_mark->inode=%p\n", __func__, i_mark, i_mark->wd,
			i_mark->fsn_mark.group, i_mark->fsn_mark.i.inode);
		goto out;
	}

	/* Lets look in the idr to see if we find it */
	found_i_mark = inotify_idr_find_locked(group, wd);
	if (unlikely(!found_i_mark)) {
		WARN_ONCE(1, "%s: i_mark=%p i_mark->wd=%d i_mark->group=%p"
			" i_mark->inode=%p\n", __func__, i_mark, i_mark->wd,
			i_mark->fsn_mark.group, i_mark->fsn_mark.i.inode);
		goto out;
	}

	/*
	 * We found an mark in the idr at the right wd, but it's
	 * not the mark we were told to remove.  eparis seriously
	 * fucked up somewhere.
	 */
	if (unlikely(found_i_mark != i_mark)) {
		WARN_ONCE(1, "%s: i_mark=%p i_mark->wd=%d i_mark->group=%p "
			"mark->inode=%p found_i_mark=%p found_i_mark->wd=%d "
			"found_i_mark->group=%p found_i_mark->inode=%p\n",
			__func__, i_mark, i_mark->wd, i_mark->fsn_mark.group,
			i_mark->fsn_mark.i.inode, found_i_mark, found_i_mark->wd,
			found_i_mark->fsn_mark.group,
			found_i_mark->fsn_mark.i.inode);
		goto out;
	}

	/*
	 * One ref for being in the idr
	 * one ref held by the caller trying to kill us
	 * one ref grabbed by inotify_idr_find
	 */
	if (unlikely(atomic_read(&i_mark->fsn_mark.refcnt) < 3)) {
		printk(KERN_ERR "%s: i_mark=%p i_mark->wd=%d i_mark->group=%p"
			" i_mark->inode=%p\n", __func__, i_mark, i_mark->wd,
			i_mark->fsn_mark.group, i_mark->fsn_mark.i.inode);
		/* we can't really recover with bad ref cnting.. */
		BUG();
	}

	do_inotify_remove_from_idr(group, i_mark);
out:
	/* match the ref taken by inotify_idr_find_locked() */
	if (found_i_mark)
		fsnotify_put_mark(&found_i_mark->fsn_mark);
	i_mark->wd = -1;
	spin_unlock(idr_lock);
}

/*
 * Send IN_IGNORED for this wd, remove this wd from the idr.
 */
void inotify_ignored_and_remove_idr(struct fsnotify_mark *fsn_mark,
				    struct fsnotify_group *group)
{
	struct inotify_inode_mark *i_mark;
	struct fsnotify_event *ignored_event, *notify_event;
	struct inotify_event_private_data *event_priv;
	struct fsnotify_event_private_data *fsn_event_priv;
	int ret;

	ignored_event = fsnotify_create_event(NULL, FS_IN_IGNORED, NULL,
					      FSNOTIFY_EVENT_NONE, NULL, 0,
					      GFP_NOFS);
	if (!ignored_event)
		return;

	i_mark = container_of(fsn_mark, struct inotify_inode_mark, fsn_mark);

	event_priv = kmem_cache_alloc(event_priv_cachep, GFP_NOFS);
	if (unlikely(!event_priv))
		goto skip_send_ignore;

	fsn_event_priv = &event_priv->fsnotify_event_priv_data;

	fsn_event_priv->group = group;
	event_priv->wd = i_mark->wd;

	notify_event = fsnotify_add_notify_event(group, ignored_event, fsn_event_priv, NULL);
	if (notify_event) {
		if (IS_ERR(notify_event))
			ret = PTR_ERR(notify_event);
		else
			fsnotify_put_event(notify_event);
		inotify_free_event_priv(fsn_event_priv);
	}

skip_send_ignore:

	/* matches the reference taken when the event was created */
	fsnotify_put_event(ignored_event);

	/* remove this mark from the idr */
	inotify_remove_from_idr(group, i_mark);

	atomic_dec(&group->inotify_data.user->inotify_watches);
}

/* ding dong the mark is dead */
static void inotify_free_mark(struct fsnotify_mark *fsn_mark)
{
	struct inotify_inode_mark *i_mark;

	i_mark = container_of(fsn_mark, struct inotify_inode_mark, fsn_mark);

	kmem_cache_free(inotify_inode_mark_cachep, i_mark);
}

static int inotify_update_existing_watch(struct fsnotify_group *group,
					 struct inode *inode,
					 u32 arg)
{
	struct fsnotify_mark *fsn_mark;
	struct inotify_inode_mark *i_mark;
	__u32 old_mask, new_mask;
	__u32 mask;
	int add = (arg & IN_MASK_ADD);
	int ret;

	/* don't allow invalid bits: we don't want flags set */
	mask = inotify_arg_to_mask(arg);
	if (unlikely(!(mask & IN_ALL_EVENTS)))
		return -EINVAL;

	fsn_mark = fsnotify_find_inode_mark(group, inode);
	if (!fsn_mark)
		return -ENOENT;

	i_mark = container_of(fsn_mark, struct inotify_inode_mark, fsn_mark);

	spin_lock(&fsn_mark->lock);

	old_mask = fsn_mark->mask;
	if (add)
		fsnotify_set_mark_mask_locked(fsn_mark, (fsn_mark->mask | mask));
	else
		fsnotify_set_mark_mask_locked(fsn_mark, mask);
	new_mask = fsn_mark->mask;

	spin_unlock(&fsn_mark->lock);

	if (old_mask != new_mask) {
		/* more bits in old than in new? */
		int dropped = (old_mask & ~new_mask);
		/* more bits in this fsn_mark than the inode's mask? */
		int do_inode = (new_mask & ~inode->i_fsnotify_mask);

		/* update the inode with this new fsn_mark */
		if (dropped || do_inode)
			fsnotify_recalc_inode_mask(inode);

	}

	/* return the wd */
	ret = i_mark->wd;

	/* match the get from fsnotify_find_mark() */
	fsnotify_put_mark(fsn_mark);

	return ret;
}

static int inotify_new_watch(struct fsnotify_group *group,
			     struct inode *inode,
			     u32 arg)
{
	struct inotify_inode_mark *tmp_i_mark;
	__u32 mask;
	int ret;
	struct idr *idr = &group->inotify_data.idr;
	spinlock_t *idr_lock = &group->inotify_data.idr_lock;

	/* don't allow invalid bits: we don't want flags set */
	mask = inotify_arg_to_mask(arg);
	if (unlikely(!(mask & IN_ALL_EVENTS)))
		return -EINVAL;

	tmp_i_mark = kmem_cache_alloc(inotify_inode_mark_cachep, GFP_KERNEL);
	if (unlikely(!tmp_i_mark))
		return -ENOMEM;

	fsnotify_init_mark(&tmp_i_mark->fsn_mark, inotify_free_mark);
	tmp_i_mark->fsn_mark.mask = mask;
	tmp_i_mark->wd = -1;

	ret = -ENOSPC;
	if (atomic_read(&group->inotify_data.user->inotify_watches) >= inotify_max_user_watches)
		goto out_err;

	ret = inotify_add_to_idr(idr, idr_lock, &group->inotify_data.last_wd,
				 tmp_i_mark);
	if (ret)
		goto out_err;

	/* we are on the idr, now get on the inode */
	ret = fsnotify_add_mark(&tmp_i_mark->fsn_mark, group, inode, NULL, 0);
	if (ret) {
		/* we failed to get on the inode, get off the idr */
		inotify_remove_from_idr(group, tmp_i_mark);
		goto out_err;
	}

	/* increment the number of watches the user has */
	atomic_inc(&group->inotify_data.user->inotify_watches);

	/* return the watch descriptor for this new mark */
	ret = tmp_i_mark->wd;

out_err:
	/* match the ref from fsnotify_init_mark() */
	fsnotify_put_mark(&tmp_i_mark->fsn_mark);

	return ret;
}

static int inotify_update_watch(struct fsnotify_group *group, struct inode *inode, u32 arg)
{
	int ret = 0;

retry:
	/* try to update and existing watch with the new arg */
	ret = inotify_update_existing_watch(group, inode, arg);
	/* no mark present, try to add a new one */
	if (ret == -ENOENT)
		ret = inotify_new_watch(group, inode, arg);
	/*
	 * inotify_new_watch could race with another thread which did an
	 * inotify_new_watch between the update_existing and the add watch
	 * here, go back and try to update an existing mark again.
	 */
	if (ret == -EEXIST)
		goto retry;

	return ret;
}

static struct fsnotify_group *inotify_new_group(struct user_struct *user, unsigned int max_events)
{
	struct fsnotify_group *group;

	group = fsnotify_alloc_group(&inotify_fsnotify_ops);
	if (IS_ERR(group))
		return group;

	group->max_events = max_events;

	spin_lock_init(&group->inotify_data.idr_lock);
	idr_init(&group->inotify_data.idr);
	group->inotify_data.last_wd = 0;
	group->inotify_data.user = user;
	group->inotify_data.fa = NULL;

	return group;
}


/* inotify syscalls */
SYSCALL_DEFINE1(inotify_init1, int, flags)
{
	struct fsnotify_group *group;
	struct user_struct *user;
	int ret;

	/* Check the IN_* constants for consistency.  */
	BUILD_BUG_ON(IN_CLOEXEC != O_CLOEXEC);
	BUILD_BUG_ON(IN_NONBLOCK != O_NONBLOCK);

	if (flags & ~(IN_CLOEXEC | IN_NONBLOCK))
		return -EINVAL;

	user = get_current_user();
	if (unlikely(atomic_read(&user->inotify_devs) >=
			inotify_max_user_instances)) {
		ret = -EMFILE;
		goto out_free_uid;
	}

	/* fsnotify_obtain_group took a reference to group, we put this when we kill the file in the end */
	group = inotify_new_group(user, inotify_max_queued_events);
	if (IS_ERR(group)) {
		ret = PTR_ERR(group);
		goto out_free_uid;
	}

	atomic_inc(&user->inotify_devs);

	ret = anon_inode_getfd("inotify", &inotify_fops, group,
				  O_RDONLY | flags);
	if (ret >= 0)
		return ret;

	fsnotify_put_group(group);
	atomic_dec(&user->inotify_devs);
out_free_uid:
	free_uid(user);
	return ret;
}

SYSCALL_DEFINE0(inotify_init)
{
	return sys_inotify_init1(0);
}

SYSCALL_DEFINE3(inotify_add_watch, int, fd, const char __user *, pathname,
		u32, mask)
{
	struct fsnotify_group *group;
	struct inode *inode;
	struct path path;
	struct file *filp;
	int ret, fput_needed;
	unsigned flags = 0;

	filp = fget_light(fd, &fput_needed);
	if (unlikely(!filp))
		return -EBADF;

	/* verify that this is indeed an inotify instance */
	if (unlikely(filp->f_op != &inotify_fops)) {
		ret = -EINVAL;
		goto fput_and_out;
	}

	if (!(mask & IN_DONT_FOLLOW))
		flags |= LOOKUP_FOLLOW;
	if (mask & IN_ONLYDIR)
		flags |= LOOKUP_DIRECTORY;

	ret = inotify_find_inode(pathname, &path, flags);
	if (ret)
		goto fput_and_out;

	/* inode held in place by reference to path; group by fget on fd */
	inode = path.dentry->d_inode;
	group = filp->private_data;

	/* create/update an inode mark */
	ret = inotify_update_watch(group, inode, mask);
	path_put(&path);
fput_and_out:
	fput_light(filp, fput_needed);
	return ret;
}

SYSCALL_DEFINE2(inotify_rm_watch, int, fd, __s32, wd)
{
	struct fsnotify_group *group;
	struct inotify_inode_mark *i_mark;
	struct file *filp;
	int ret = 0, fput_needed;

	filp = fget_light(fd, &fput_needed);
	if (unlikely(!filp))
		return -EBADF;

	/* verify that this is indeed an inotify instance */
	ret = -EINVAL;
	if (unlikely(filp->f_op != &inotify_fops))
		goto out;

	group = filp->private_data;

	ret = -EINVAL;
	i_mark = inotify_idr_find(group, wd);
	if (unlikely(!i_mark))
		goto out;

	ret = 0;

	fsnotify_destroy_mark(&i_mark->fsn_mark);

	/* match ref taken by inotify_idr_find */
	fsnotify_put_mark(&i_mark->fsn_mark);

out:
	fput_light(filp, fput_needed);
	return ret;
}

/*
 * inotify_user_setup - Our initialization function.  Note that we cannnot return
 * error because we have compiled-in VFS hooks.  So an (unlikely) failure here
 * must result in panic().
 */
static int __init inotify_user_setup(void)
{
	BUILD_BUG_ON(IN_ACCESS != FS_ACCESS);
	BUILD_BUG_ON(IN_MODIFY != FS_MODIFY);
	BUILD_BUG_ON(IN_ATTRIB != FS_ATTRIB);
	BUILD_BUG_ON(IN_CLOSE_WRITE != FS_CLOSE_WRITE);
	BUILD_BUG_ON(IN_CLOSE_NOWRITE != FS_CLOSE_NOWRITE);
	BUILD_BUG_ON(IN_OPEN != FS_OPEN);
	BUILD_BUG_ON(IN_MOVED_FROM != FS_MOVED_FROM);
	BUILD_BUG_ON(IN_MOVED_TO != FS_MOVED_TO);
	BUILD_BUG_ON(IN_CREATE != FS_CREATE);
	BUILD_BUG_ON(IN_DELETE != FS_DELETE);
	BUILD_BUG_ON(IN_DELETE_SELF != FS_DELETE_SELF);
	BUILD_BUG_ON(IN_MOVE_SELF != FS_MOVE_SELF);
	BUILD_BUG_ON(IN_UNMOUNT != FS_UNMOUNT);
	BUILD_BUG_ON(IN_Q_OVERFLOW != FS_Q_OVERFLOW);
	BUILD_BUG_ON(IN_IGNORED != FS_IN_IGNORED);
	BUILD_BUG_ON(IN_EXCL_UNLINK != FS_EXCL_UNLINK);
	BUILD_BUG_ON(IN_ISDIR != FS_IN_ISDIR);
	BUILD_BUG_ON(IN_ONESHOT != FS_IN_ONESHOT);

	BUG_ON(hweight32(ALL_INOTIFY_BITS) != 21);

	inotify_inode_mark_cachep = KMEM_CACHE(inotify_inode_mark, SLAB_PANIC);
	event_priv_cachep = KMEM_CACHE(inotify_event_private_data, SLAB_PANIC);

	inotify_max_queued_events = 16384;
	inotify_max_user_instances = 128;
	inotify_max_user_watches = 8192;

	return 0;
}
module_init(inotify_user_setup);
