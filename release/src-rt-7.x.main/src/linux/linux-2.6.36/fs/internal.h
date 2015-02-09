/* fs/ internal definitions
 *
 * Copyright (C) 2006 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/lglock.h>

struct super_block;
struct linux_binprm;
struct path;

/*
 * block_dev.c
 */
#ifdef CONFIG_BLOCK
extern struct super_block *blockdev_superblock;
extern void __init bdev_cache_init(void);

static inline int sb_is_blkdev_sb(struct super_block *sb)
{
	return sb == blockdev_superblock;
}

extern int __sync_blockdev(struct block_device *bdev, int wait);

#else
static inline void bdev_cache_init(void)
{
}

static inline int sb_is_blkdev_sb(struct super_block *sb)
{
	return 0;
}

static inline int __sync_blockdev(struct block_device *bdev, int wait)
{
	return 0;
}
#endif

/*
 * char_dev.c
 */
extern void __init chrdev_init(void);

/*
 * exec.c
 */
extern int check_unsafe_exec(struct linux_binprm *);

/*
 * namespace.c
 */
extern int copy_mount_options(const void __user *, unsigned long *);
extern int copy_mount_string(const void __user *, char **);

extern void free_vfsmnt(struct vfsmount *);
extern struct vfsmount *alloc_vfsmnt(const char *);
extern struct vfsmount *__lookup_mnt(struct vfsmount *, struct dentry *, int);
extern void mnt_set_mountpoint(struct vfsmount *, struct dentry *,
				struct vfsmount *);
extern void release_mounts(struct list_head *);
extern void umount_tree(struct vfsmount *, int, struct list_head *);
extern struct vfsmount *copy_tree(struct vfsmount *, struct dentry *, int);

extern void __init mnt_init(void);

DECLARE_BRLOCK(vfsmount_lock);


/*
 * fs_struct.c
 */
extern void chroot_fs_refs(struct path *, struct path *);

/*
 * file_table.c
 */
extern void file_sb_list_add(struct file *f, struct super_block *sb);
extern void file_sb_list_del(struct file *f);
extern void mark_files_ro(struct super_block *);
extern struct file *get_empty_filp(void);

/*
 * super.c
 */
extern int do_remount_sb(struct super_block *, int, void *, int);
extern void __put_super(struct super_block *sb);
extern void put_super(struct super_block *sb);

/*
 * open.c
 */
struct nameidata;
extern struct file *nameidata_to_filp(struct nameidata *);
extern void release_open_intent(struct nameidata *);
