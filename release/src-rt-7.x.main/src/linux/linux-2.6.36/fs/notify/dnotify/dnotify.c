/*
 * Directory notifications for Linux.
 *
 * Copyright (C) 2000,2001,2002 Stephen Rothwell
 *
 * Copyright (C) 2009 Eric Paris <Red Hat Inc>
 * dnotify was largly rewritten to use the new fsnotify infrastructure
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
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/dnotify.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/fdtable.h>
#include <linux/fsnotify_backend.h>

int dir_notify_enable __read_mostly = 1;

static struct kmem_cache *dnotify_struct_cache __read_mostly;
static struct kmem_cache *dnotify_mark_cache __read_mostly;
static struct fsnotify_group *dnotify_group __read_mostly;
static DEFINE_MUTEX(dnotify_mark_mutex);

/*
 * dnotify will attach one of these to each inode (i_fsnotify_marks) which
 * is being watched by dnotify.  If multiple userspace applications are watching
 * the same directory with dnotify their information is chained in dn
 */
struct dnotify_mark {
	struct fsnotify_mark fsn_mark;
	struct dnotify_struct *dn;
};

/*
 * When a process starts or stops watching an inode the set of events which
 * dnotify cares about for that inode may change.  This function runs the
 * list of everything receiving dnotify events about this directory and calculates
 * the set of all those events.  After it updates what dnotify is interested in
 * it calls the fsnotify function so it can update the set of all events relevant
 * to this inode.
 */
static void dnotify_recalc_inode_mask(struct fsnotify_mark *fsn_mark)
{
	__u32 new_mask, old_mask;
	struct dnotify_struct *dn;
	struct dnotify_mark *dn_mark  = container_of(fsn_mark,
						     struct dnotify_mark,
						     fsn_mark);

	assert_spin_locked(&fsn_mark->lock);

	old_mask = fsn_mark->mask;
	new_mask = 0;
	for (dn = dn_mark->dn; dn != NULL; dn = dn->dn_next)
		new_mask |= (dn->dn_mask & ~FS_DN_MULTISHOT);
	fsnotify_set_mark_mask_locked(fsn_mark, new_mask);

	if (old_mask == new_mask)
		return;

	if (fsn_mark->i.inode)
		fsnotify_recalc_inode_mask(fsn_mark->i.inode);
}

/*
 * Mains fsnotify call where events are delivered to dnotify.
 * Find the dnotify mark on the relevant inode, run the list of dnotify structs
 * on that mark and determine which of them has expressed interest in receiving
 * events of this type.  When found send the correct process and signal and
 * destroy the dnotify struct if it was not registered to receive multiple
 * events.
 */
static int dnotify_handle_event(struct fsnotify_group *group,
				struct fsnotify_mark *inode_mark,
				struct fsnotify_mark *vfsmount_mark,
				struct fsnotify_event *event)
{
	struct dnotify_mark *dn_mark;
	struct inode *to_tell;
	struct dnotify_struct *dn;
	struct dnotify_struct **prev;
	struct fown_struct *fown;
	__u32 test_mask = event->mask & ~FS_EVENT_ON_CHILD;

	BUG_ON(vfsmount_mark);

	to_tell = event->to_tell;

	dn_mark = container_of(inode_mark, struct dnotify_mark, fsn_mark);

	spin_lock(&inode_mark->lock);
	prev = &dn_mark->dn;
	while ((dn = *prev) != NULL) {
		if ((dn->dn_mask & test_mask) == 0) {
			prev = &dn->dn_next;
			continue;
		}
		fown = &dn->dn_filp->f_owner;
		send_sigio(fown, dn->dn_fd, POLL_MSG);
		if (dn->dn_mask & FS_DN_MULTISHOT)
			prev = &dn->dn_next;
		else {
			*prev = dn->dn_next;
			kmem_cache_free(dnotify_struct_cache, dn);
			dnotify_recalc_inode_mask(inode_mark);
		}
	}

	spin_unlock(&inode_mark->lock);

	return 0;
}

/*
 * Given an inode and mask determine if dnotify would be interested in sending
 * userspace notification for that pair.
 */
static bool dnotify_should_send_event(struct fsnotify_group *group,
				      struct inode *inode,
				      struct fsnotify_mark *inode_mark,
				      struct fsnotify_mark *vfsmount_mark,
				      __u32 mask, void *data, int data_type)
{
	/* not a dir, dnotify doesn't care */
	if (!S_ISDIR(inode->i_mode))
		return false;

	return true;
}

static void dnotify_free_mark(struct fsnotify_mark *fsn_mark)
{
	struct dnotify_mark *dn_mark = container_of(fsn_mark,
						    struct dnotify_mark,
						    fsn_mark);

	BUG_ON(dn_mark->dn);

	kmem_cache_free(dnotify_mark_cache, dn_mark);
}

static struct fsnotify_ops dnotify_fsnotify_ops = {
	.handle_event = dnotify_handle_event,
	.should_send_event = dnotify_should_send_event,
	.free_group_priv = NULL,
	.freeing_mark = NULL,
	.free_event_priv = NULL,
};

/*
 * Called every time a file is closed.  Looks first for a dnotify mark on the
 * inode.  If one is found run all of the ->dn structures attached to that
 * mark for one relevant to this process closing the file and remove that
 * dnotify_struct.  If that was the last dnotify_struct also remove the
 * fsnotify_mark.
 */
void dnotify_flush(struct file *filp, fl_owner_t id)
{
	struct fsnotify_mark *fsn_mark;
	struct dnotify_mark *dn_mark;
	struct dnotify_struct *dn;
	struct dnotify_struct **prev;
	struct inode *inode;

	inode = filp->f_path.dentry->d_inode;
	if (!S_ISDIR(inode->i_mode))
		return;

	fsn_mark = fsnotify_find_inode_mark(dnotify_group, inode);
	if (!fsn_mark)
		return;
	dn_mark = container_of(fsn_mark, struct dnotify_mark, fsn_mark);

	mutex_lock(&dnotify_mark_mutex);

	spin_lock(&fsn_mark->lock);
	prev = &dn_mark->dn;
	while ((dn = *prev) != NULL) {
		if ((dn->dn_owner == id) && (dn->dn_filp == filp)) {
			*prev = dn->dn_next;
			kmem_cache_free(dnotify_struct_cache, dn);
			dnotify_recalc_inode_mask(fsn_mark);
			break;
		}
		prev = &dn->dn_next;
	}

	spin_unlock(&fsn_mark->lock);

	/* nothing else could have found us thanks to the dnotify_mark_mutex */
	if (dn_mark->dn == NULL)
		fsnotify_destroy_mark(fsn_mark);

	mutex_unlock(&dnotify_mark_mutex);

	fsnotify_put_mark(fsn_mark);
}

/* this conversion is done only at watch creation */
static __u32 convert_arg(unsigned long arg)
{
	__u32 new_mask = FS_EVENT_ON_CHILD;

	if (arg & DN_MULTISHOT)
		new_mask |= FS_DN_MULTISHOT;
	if (arg & DN_DELETE)
		new_mask |= (FS_DELETE | FS_MOVED_FROM);
	if (arg & DN_MODIFY)
		new_mask |= FS_MODIFY;
	if (arg & DN_ACCESS)
		new_mask |= FS_ACCESS;
	if (arg & DN_ATTRIB)
		new_mask |= FS_ATTRIB;
	if (arg & DN_RENAME)
		new_mask |= FS_DN_RENAME;
	if (arg & DN_CREATE)
		new_mask |= (FS_CREATE | FS_MOVED_TO);

	return new_mask;
}

/*
 * If multiple processes watch the same inode with dnotify there is only one
 * dnotify mark in inode->i_fsnotify_marks but we chain a dnotify_struct
 * onto that mark.  This function either attaches the new dnotify_struct onto
 * that list, or it |= the mask onto an existing dnofiy_struct.
 */
static int attach_dn(struct dnotify_struct *dn, struct dnotify_mark *dn_mark,
		     fl_owner_t id, int fd, struct file *filp, __u32 mask)
{
	struct dnotify_struct *odn;

	odn = dn_mark->dn;
	while (odn != NULL) {
		/* adding more events to existing dnofiy_struct? */
		if ((odn->dn_owner == id) && (odn->dn_filp == filp)) {
			odn->dn_fd = fd;
			odn->dn_mask |= mask;
			return -EEXIST;
		}
		odn = odn->dn_next;
	}

	dn->dn_mask = mask;
	dn->dn_fd = fd;
	dn->dn_filp = filp;
	dn->dn_owner = id;
	dn->dn_next = dn_mark->dn;
	dn_mark->dn = dn;

	return 0;
}

/*
 * When a process calls fcntl to attach a dnotify watch to a directory it ends
 * up here.  Allocate both a mark for fsnotify to add and a dnotify_struct to be
 * attached to the fsnotify_mark.
 */
int fcntl_dirnotify(int fd, struct file *filp, unsigned long arg)
{
	struct dnotify_mark *new_dn_mark, *dn_mark;
	struct fsnotify_mark *new_fsn_mark, *fsn_mark;
	struct dnotify_struct *dn;
	struct inode *inode;
	fl_owner_t id = current->files;
	struct file *f;
	int destroy = 0, error = 0;
	__u32 mask;

	/* we use these to tell if we need to kfree */
	new_fsn_mark = NULL;
	dn = NULL;

	if (!dir_notify_enable) {
		error = -EINVAL;
		goto out_err;
	}

	/* a 0 mask means we are explicitly removing the watch */
	if ((arg & ~DN_MULTISHOT) == 0) {
		dnotify_flush(filp, id);
		error = 0;
		goto out_err;
	}

	/* dnotify only works on directories */
	inode = filp->f_path.dentry->d_inode;
	if (!S_ISDIR(inode->i_mode)) {
		error = -ENOTDIR;
		goto out_err;
	}

	/* expect most fcntl to add new rather than augment old */
	dn = kmem_cache_alloc(dnotify_struct_cache, GFP_KERNEL);
	if (!dn) {
		error = -ENOMEM;
		goto out_err;
	}

	/* new fsnotify mark, we expect most fcntl calls to add a new mark */
	new_dn_mark = kmem_cache_alloc(dnotify_mark_cache, GFP_KERNEL);
	if (!new_dn_mark) {
		error = -ENOMEM;
		goto out_err;
	}

	/* convert the userspace DN_* "arg" to the internal FS_* defines in fsnotify */
	mask = convert_arg(arg);

	/* set up the new_fsn_mark and new_dn_mark */
	new_fsn_mark = &new_dn_mark->fsn_mark;
	fsnotify_init_mark(new_fsn_mark, dnotify_free_mark);
	new_fsn_mark->mask = mask;
	new_dn_mark->dn = NULL;

	/* this is needed to prevent the fcntl/close race described below */
	mutex_lock(&dnotify_mark_mutex);

	/* add the new_fsn_mark or find an old one. */
	fsn_mark = fsnotify_find_inode_mark(dnotify_group, inode);
	if (fsn_mark) {
		dn_mark = container_of(fsn_mark, struct dnotify_mark, fsn_mark);
		spin_lock(&fsn_mark->lock);
	} else {
		fsnotify_add_mark(new_fsn_mark, dnotify_group, inode, NULL, 0);
		spin_lock(&new_fsn_mark->lock);
		fsn_mark = new_fsn_mark;
		dn_mark = new_dn_mark;
		/* we used new_fsn_mark, so don't free it */
		new_fsn_mark = NULL;
	}

	rcu_read_lock();
	f = fcheck(fd);
	rcu_read_unlock();

	/* if (f != filp) means that we lost a race and another task/thread
	 * actually closed the fd we are still playing with before we grabbed
	 * the dnotify_mark_mutex and fsn_mark->lock.  Since closing the fd is the
	 * only time we clean up the marks we need to get our mark off
	 * the list. */
	if (f != filp) {
		/* if we added ourselves, shoot ourselves, it's possible that
		 * the flush actually did shoot this fsn_mark.  That's fine too
		 * since multiple calls to destroy_mark is perfectly safe, if
		 * we found a dn_mark already attached to the inode, just sod
		 * off silently as the flush at close time dealt with it.
		 */
		if (dn_mark == new_dn_mark)
			destroy = 1;
		goto out;
	}

	error = __f_setown(filp, task_pid(current), PIDTYPE_PID, 0);
	if (error) {
		/* if we added, we must shoot */
		if (dn_mark == new_dn_mark)
			destroy = 1;
		goto out;
	}

	error = attach_dn(dn, dn_mark, id, fd, filp, mask);
	/* !error means that we attached the dn to the dn_mark, so don't free it */
	if (!error)
		dn = NULL;
	/* -EEXIST means that we didn't add this new dn and used an old one.
	 * that isn't an error (and the unused dn should be freed) */
	else if (error == -EEXIST)
		error = 0;

	dnotify_recalc_inode_mask(fsn_mark);
out:
	spin_unlock(&fsn_mark->lock);

	if (destroy)
		fsnotify_destroy_mark(fsn_mark);

	mutex_unlock(&dnotify_mark_mutex);
	fsnotify_put_mark(fsn_mark);
out_err:
	if (new_fsn_mark)
		fsnotify_put_mark(new_fsn_mark);
	if (dn)
		kmem_cache_free(dnotify_struct_cache, dn);
	return error;
}

static int __init dnotify_init(void)
{
	dnotify_struct_cache = KMEM_CACHE(dnotify_struct, SLAB_PANIC);
	dnotify_mark_cache = KMEM_CACHE(dnotify_mark, SLAB_PANIC);

	dnotify_group = fsnotify_alloc_group(&dnotify_fsnotify_ops);
	if (IS_ERR(dnotify_group))
		panic("unable to allocate fsnotify group for dnotify\n");
	return 0;
}

module_init(dnotify_init)
